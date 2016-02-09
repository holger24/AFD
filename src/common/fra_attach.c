/*
 *  fra_attach.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "afddefs.h"

DESCR__S_M3
/*
 ** NAME
 **   fra_attach - attaches to the FRA
 **
 ** SYNOPSIS
 **   int fra_attach(void)
 **   int fra_attach_passive(void)
 **
 ** DESCRIPTION
 **   Attaches to the memory mapped area of the FRA (File Retrieve
 **   Status Area). The first 4 bytes of this area contains the number
 **   of host in the FRA. The rest consist of the structure
 **   fileretrieve_status for each host. When the function returns
 **   successful it returns the pointer 'fra' to the begining of the
 **   first structure.
 **
 **   fra_attach_passive() attaches to the FRA in read only mode.
 **
 ** RETURN VALUES
 **   SUCCESS when attaching to the FRA successful and sets the
 **   global pointer 'fra' to the start of the FRA. Also the FRA ID,
 **   the size of the FRA and the number of host in the FRA is returned
 **   in 'fra_id', 'fra_size' and 'no_of_dirs' respectively. Otherwise
 **   INCORRECT is returned.
 **
 ** SEE ALSO
 **   check_fra()
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   31.01.2000 H.Kiehl Created
 **   29.04.2003 H.Kiehl Added function fra_attach_passive().
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                      /* strerror(), strlen()         */
#include <unistd.h>                      /* close(), read()              */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>                   /* mmap(), munmap()             */
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>                      /* fcntl()                      */
#endif
#include <errno.h>

/* Global variables. */
extern int                        fra_fd,
                                  fra_id,
                                  no_of_dirs;
#ifdef HAVE_MMAP
extern off_t                      fra_size;
#endif
extern char                       *p_work_dir;
extern struct fileretrieve_status *fra;


/*############################ fra_attach() #############################*/
int
fra_attach(void)
{
   int          fd,
                loop_counter,
                retries = 0,
                tmp_errno;
   char         *ptr = NULL,
                *p_fra_stat_file,
                fra_id_file[MAX_PATH_LENGTH],
                fra_stat_file[MAX_PATH_LENGTH];
   struct flock wlock = {F_WRLCK, SEEK_SET, 0, 1},
                ulock = {F_UNLCK, SEEK_SET, 0, 1};
   struct stat  stat_buf;

   /* Get absolute path of FRA_ID_FILE. */
   (void)strcpy(fra_id_file, p_work_dir);
   (void)strcat(fra_id_file, FIFO_DIR);
   (void)strcpy(fra_stat_file, fra_id_file);
   (void)strcat(fra_stat_file, FRA_STAT_FILE);
   p_fra_stat_file = fra_stat_file + strlen(fra_stat_file);
   (void)strcat(fra_id_file, FRA_ID_FILE);

   do
   {
      /* Make sure this is not the case when the */
      /* no_of_dirs is stale.                    */
      if ((no_of_dirs < 0) && (fra != NULL))
      {
         /* Unmap from FRA. */
#ifdef HAVE_MMAP
         (void)snprintf(p_fra_stat_file,
                        MAX_PATH_LENGTH - (p_fra_stat_file - fra_stat_file),
                        ".%d", fra_id);
         if (stat(fra_stat_file, &stat_buf) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to stat() `%s' : %s"),
                       fra_stat_file, strerror(errno));
         }
         else
         {
            if (munmap((void *)fra, stat_buf.st_size) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to munmap() `%s' : %s"),
                          fra_stat_file, strerror(errno));
            }
            else
            {
               fra = NULL;
            }
         }
#else
         if (munmap_emu((void *)fra) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to munmap() `%s' : %s"),
                       fra_stat_file, strerror(errno));
         }
         else
         {
            fra = NULL;
         }
#endif

         /* No need to speed things up here. */
         my_usleep(400000L);
      }

      /*
       * Retrieve the FRA (Fileretrieve Status Area) ID from FRA_ID_FILE.
       * Make sure that it is not locked.
       */
      loop_counter = 0;
      while ((fd = coe_open(fra_id_file, O_RDWR)) == -1)
      {
         tmp_errno = errno;
         if (errno == ENOENT)
         {
            my_usleep(400000L);
            if (loop_counter++ > 24)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to open() `%s' : %s"),
                          fra_id_file, strerror(errno));
               return(tmp_errno);
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to open() `%s' : %s"),
                       fra_id_file, strerror(errno));
            return(tmp_errno);
         }
      }

      /* Check if its locked. */
      if (fcntl(fd, F_SETLKW, &wlock) == -1)
      {
         tmp_errno = errno;
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not set write lock for `%s' : %s"),
                    fra_id_file, strerror(errno));
         (void)close(fd);
         return(tmp_errno);
      }

      /* Read the fra_id. */
      if (read(fd, &fra_id, sizeof(int)) < 0)
      {
         tmp_errno = errno;
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not read the value of the fra_id : %s"),
                    strerror(errno));
         (void)close(fd);
         return(tmp_errno);
      }

      /* Unlock file and close it. */
      if (fcntl(fd, F_SETLKW, &ulock) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not unlock `%s' : %s",
                    fra_id_file, strerror(errno));
         (void)close(fd);
         return(INCORRECT);
      }
      if (close(fd) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Could not close() `%s' : %s",
                    fra_id_file, strerror(errno));
      }

      (void)snprintf(p_fra_stat_file,
                     MAX_PATH_LENGTH - (p_fra_stat_file - fra_stat_file),
                     ".%d", fra_id);

      if (fra_fd > 0)
      {
         if (close(fra_fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("close() error : %s"), strerror(errno));
         }
      }

      if ((fra_fd = coe_open(fra_stat_file, O_RDWR)) == -1)
      {
         tmp_errno = errno;
         if (errno == ENOENT)
         {
            if (retries++ > 8)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to open() `%s' : %s"),
                          fra_stat_file, strerror(errno));
               return(tmp_errno);
            }
            else
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          _("Failed to open() `%s' : %s"),
                          fra_stat_file, strerror(errno));
               (void)sleep(1);
               continue;
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to open() `%s' : %s"),
                       fra_stat_file, strerror(errno));
            return(tmp_errno);
         }
      }

      if (fstat(fra_fd, &stat_buf) == -1)
      {
         tmp_errno = errno;
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to stat() `%s' : %s"),
                    fra_stat_file, strerror(errno));
         (void)close(fra_fd);
         fra_fd = -1;
         return(tmp_errno);
      }

#ifdef HAVE_MMAP
      if ((ptr = mmap(NULL, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                      MAP_SHARED, fra_fd, 0)) == (caddr_t) -1)
#else
      if ((ptr = mmap_emu(NULL, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                          MAP_SHARED, fra_stat_file, 0)) == (caddr_t) -1)
#endif
      {
         tmp_errno = errno;
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("mmap() error : %s"), strerror(errno));
         (void)close(fra_fd);
         fra_fd = -1;
         return(tmp_errno);
      }

      /* Read number of current FRA. */
      no_of_dirs = *(int *)ptr;

      /* Check FRA version number. */
      if ((no_of_dirs > 0) &&
          (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_FRA_VERSION))
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("This code is compiled for of FRA version %d, but the FRA we try to attach is %d."),
                    CURRENT_FRA_VERSION, (int)(*(ptr + SIZEOF_INT + 1 + 1 + 1)));
#ifdef HAVE_MMAP
         if (munmap(ptr, stat_buf.st_size) == -1)
#else
         if (munmap_emu(ptr) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to munmap() FRA : %s"), strerror(errno));
         }
         (void)close(fra_fd);
         fra_fd = -1;
         return(INCORRECT_VERSION);
      }
   } while (no_of_dirs <= 0);

   ptr += AFD_WORD_OFFSET;
   fra = (struct fileretrieve_status *)ptr;
#ifdef HAVE_MMAP
   fra_size = stat_buf.st_size;
#endif

   return(SUCCESS);
}


/*######################## fra_attach_passive() #########################*/
int
fra_attach_passive(void)
{
   int         fd,
               tmp_errno;
   char        *ptr = NULL,
               *p_fra_stat_file,
               fra_id_file[MAX_PATH_LENGTH],
               fra_stat_file[MAX_PATH_LENGTH];
   struct stat stat_buf;

   /* Get absolute path of FRA_ID_FILE. */
   (void)strcpy(fra_id_file, p_work_dir);
   (void)strcat(fra_id_file, FIFO_DIR);
   (void)strcpy(fra_stat_file, fra_id_file);
   (void)strcat(fra_stat_file, FRA_STAT_FILE);
   p_fra_stat_file = fra_stat_file + strlen(fra_stat_file);
   (void)strcat(fra_id_file, FRA_ID_FILE);

   /* Read the FRA ID. */
   if ((fd = coe_open(fra_id_file, O_RDONLY)) == -1)
   {
      tmp_errno = errno;
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to open() `%s' : %s"), fra_id_file, strerror(errno));
      return(tmp_errno);
   }
   if (read(fd, &fra_id, sizeof(int)) < 0)
   {
      tmp_errno = errno;
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not read the value of the fra_id : %s"),
                 strerror(errno));
      (void)close(fd);
      return(tmp_errno);
   }
   if (close(fd) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 _("Could not close() `%s' : %s"),
                 fra_id_file, strerror(errno));
   }
   (void)snprintf(p_fra_stat_file,
                  MAX_PATH_LENGTH - (p_fra_stat_file - fra_stat_file),
                  ".%d", fra_id);

   if (fra_fd > 0)
   {
      if (close(fra_fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("close() error : %s"), strerror(errno));
      }
   }

   if ((fra_fd = coe_open(fra_stat_file, O_RDONLY)) == -1)
   {
      tmp_errno = errno;
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to open() `%s' : %s"),
                 fra_stat_file, strerror(errno));
      return(tmp_errno);
   }

   if (fstat(fra_fd, &stat_buf) == -1)
   {
      tmp_errno = errno;
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to stat() `%s' : %s"),
                 fra_stat_file, strerror(errno));
      (void)close(fra_fd);
      fra_fd = -1;
      return(tmp_errno);
   }
   if (stat_buf.st_size < AFD_WORD_OFFSET)
   {
      tmp_errno = errno;
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("FRA not large enough to contain any meaningful data."));
      (void)close(fra_fd);
      fra_fd = -1;
      return(tmp_errno);
   }

#ifdef HAVE_MMAP
   if ((ptr = mmap(NULL, stat_buf.st_size, PROT_READ,
                   MAP_SHARED, fra_fd, 0)) == (caddr_t) -1)
#else
   if ((ptr = mmap_emu(NULL, stat_buf.st_size, PROT_READ,
                       MAP_SHARED, fra_stat_file, 0)) == (caddr_t) -1)
#endif
   {
      tmp_errno = errno;
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("mmap() error : %s"), strerror(errno));
      (void)close(fra_fd);
      fra_fd = -1;
      return(tmp_errno);
   }

   /* Check FRA version number. */
   if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_FRA_VERSION)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 _("This code is compiled for of FRA version %d, but the FRA we try to attach is %d."),
                 CURRENT_FRA_VERSION, (int)(*(ptr + SIZEOF_INT + 1 + 1 + 1)));
#ifdef HAVE_MMAP
      if (munmap(ptr, stat_buf.st_size) == -1)
#else
      if (munmap_emu(ptr) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to munmap() FRA : %s"), strerror(errno));
      }
      (void)close(fra_fd);
      fra_fd = -1;
      return(INCORRECT_VERSION);
   }

   /* Read number of current FRA */
   no_of_dirs = *(int *)ptr;

   ptr += AFD_WORD_OFFSET;
   fra = (struct fileretrieve_status *)ptr;
#ifdef HAVE_MMAP
   fra_size = stat_buf.st_size;
#endif

   return(SUCCESS);
}
