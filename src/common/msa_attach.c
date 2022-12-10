/*
 *  msa_attach.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   msa_attach - attaches to the MSA
 **
 ** SYNOPSIS
 **   int msa_attach(void)
 **   int msa_attach_passive(void)
 **
 ** DESCRIPTION
 **   Attaches to the memory mapped area of the MSA (Monitor Status
 **   Area). The first 8 bytes of this area contains the number of
 **   AFD's that are being monitored. The rest consist of the
 **   structure mon_status_area for each AFD. When the function
 **   returns successful it returns the pointer 'msa' to the begining
 **   of the first structure.
 **
 ** RETURN VALUES
 **   SUCCESS when attaching to the MSA successful and sets the
 **   global pointer 'msa' to the start of the MSA and the size of
 **   the memory mapped file is returned. Also the MSA ID and the
 **   number of AFD's in the MSA is returned in 'msa_id' and
 **   'no_of_afds' respectively. Otherwise INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.08.1998 H.Kiehl Created
 **   12.03.2009 H.Kiehl Added function msa_attach_passive().
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
#include "mondefs.h"

/* Global variables. */
extern int                    msa_fd,
                              msa_id,
                              no_of_afds;
#ifdef HAVE_MMAP
extern off_t                  msa_size;
#endif
extern char                   *p_work_dir;
extern struct mon_status_area *msa;


/*############################ msa_attach() #############################*/
int
msa_attach(void)
{
   int          fd,
                loop_counter,
                retries = 0,
                timeout_loops = 0;
   char         *ptr = NULL,
                *p_msa_stat_file,
                msa_id_file[MAX_PATH_LENGTH],
                msa_stat_file[MAX_PATH_LENGTH];
   struct flock wlock = {F_WRLCK, SEEK_SET, 0, 1},
                ulock = {F_UNLCK, SEEK_SET, 0, 1};
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   /* Get absolute path of MSA_ID_FILE. */
   (void)strcpy(msa_id_file, p_work_dir);
   (void)strcat(msa_id_file, FIFO_DIR);
   (void)strcpy(msa_stat_file, msa_id_file);
   (void)strcat(msa_stat_file, MON_STATUS_FILE);
   p_msa_stat_file = msa_stat_file + strlen(msa_stat_file);
   (void)strcat(msa_id_file, MSA_ID_FILE);

   do
   {
      /* Make sure this is not the case when the */
      /* no_of_afds is stale.                    */
      if ((no_of_afds < 0) && (msa != NULL))
      {
         /* Unmap from MSA. */
         ptr = (char *)msa - AFD_WORD_OFFSET;
#ifdef HAVE_MMAP
         if (munmap((void *)ptr, msa_size) == -1)
#else
         if (munmap_emu((void *)ptr) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to munmap() `%s' : %s"),
                       msa_stat_file, strerror(errno));
         }
         else
         {
            msa = NULL;
         }

         timeout_loops++;
         if (timeout_loops > 200)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Unable to attach to a new MSA."));
            return(INCORRECT);
         }

         /* No need to speed things up here. */
         my_usleep(400000L);
      }

      /*
       * Retrieve the MSA (Monitor Status Area) ID from MSA_ID_FILE.
       * Make sure that it is not locked.
       */
      loop_counter = 0;
      while ((fd = coe_open(msa_id_file, O_RDWR)) == -1)
      {
         if (errno == ENOENT)
         {
            my_usleep(400000L);
            if (loop_counter++ > 24)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to coe_open() `%s' : %s"),
                          msa_id_file, strerror(errno));
               return(INCORRECT);
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to coe_open() `%s' : %s"),
                       msa_id_file, strerror(errno));
            return(INCORRECT);
         }
      }

      /* Check if its locked. */
      if (fcntl(fd, F_SETLKW, &wlock) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not set write lock for `%s' : %s"),
                    msa_id_file, strerror(errno));
         (void)close(fd);
         return(INCORRECT);
      }

      /* Read the msa_id. */
      if (read(fd, &msa_id, sizeof(int)) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not read the value of the msa_id : %s"),
                    strerror(errno));
         (void)close(fd);
         return(INCORRECT);
      }

      /* Unlock file and close it. */
      if (fcntl(fd, F_SETLKW, &ulock) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not unlock `%s' : %s"),
                    msa_id_file, strerror(errno));
         (void)close(fd);
         return(INCORRECT);
      }
      if (close(fd) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Could not close() `%s' : %s"),
                    msa_id_file, strerror(errno));
      }

      (void)snprintf(p_msa_stat_file,
                     MAX_PATH_LENGTH - (p_msa_stat_file - msa_stat_file),
                     ".%d", msa_id);

      if (msa_fd > 0)
      {
         if (close(msa_fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("close() error : %s"), strerror(errno));
         }
      }

      if ((msa_fd = coe_open(msa_stat_file, O_RDWR)) == -1)
      {
         if (errno == ENOENT)
         {
            if (retries++ > 8)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to open() `%s' : %s"),
                          msa_stat_file, strerror(errno));
               return(INCORRECT);
            }
            else
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          _("Failed to open() `%s' : %s"),
                          msa_stat_file, strerror(errno));
               (void)sleep(1);
               continue;
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to open() `%s' : %s"),
                       msa_stat_file, strerror(errno));
            return(INCORRECT);
         }
      }

#ifdef HAVE_STATX
      if (statx(msa_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(msa_fd, &stat_buf) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                    _("Failed to statx() `%s' : %s"),
#else
                    _("Failed to fstat() `%s' : %s"),
#endif
                    msa_stat_file, strerror(errno));
         (void)close(msa_fd);
         msa_fd = -1;
         return(INCORRECT);
      }

#ifdef HAVE_MMAP
      if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                      stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                      stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                      MAP_SHARED, msa_fd, 0)) == (caddr_t) -1)
#else
      if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                          stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                          stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                          MAP_SHARED, msa_stat_file, 0)) == (caddr_t) -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("mmap() error : %s"), strerror(errno));
         (void)close(msa_fd);
         msa_fd = -1;
         return(INCORRECT);
      }

      /* Read number of AFD's. */
      no_of_afds = *(int *)ptr;

      /* Check MSA version number. */
      if ((no_of_afds > 0) &&
          (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_MSA_VERSION))
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("This code is compiled for of MSA version %d, but the MSA we try to attach is %d."),
                    CURRENT_MSA_VERSION, (int)(*(ptr + SIZEOF_INT + 1 + 1 + 1)));
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
         if (munmap(ptr, stat_buf.stx_size) == -1)
# else
         if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
         if (munmap_emu(ptr) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to munmap() MSA : %s"), strerror(errno));
         }
         (void)close(msa_fd);
         msa_fd = -1;
         return(INCORRECT_VERSION);
      }

      ptr += AFD_WORD_OFFSET;
      msa = (struct mon_status_area *)ptr;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
      msa_size = stat_buf.stx_size;
# else
      msa_size = stat_buf.st_size;
# endif
#endif
   } while (no_of_afds <= 0);

   return(SUCCESS);
}


/*######################## msa_attach_passive() #########################*/
int
msa_attach_passive(void)
{
   int          fd,
                retries = 0,
                timeout_loops = 0;
   char         *ptr = NULL,
                *p_msa_stat_file,
                msa_id_file[MAX_PATH_LENGTH],
                msa_stat_file[MAX_PATH_LENGTH];
   struct flock rlock = {F_RDLCK, SEEK_SET, 0, 1};
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   /* Get absolute path of MSA_ID_FILE. */
   (void)strcpy(msa_id_file, p_work_dir);
   (void)strcat(msa_id_file, FIFO_DIR);
   (void)strcpy(msa_stat_file, msa_id_file);
   (void)strcat(msa_stat_file, MON_STATUS_FILE);
   p_msa_stat_file = msa_stat_file + strlen(msa_stat_file);
   (void)strcat(msa_id_file, MSA_ID_FILE);

   do
   {
      /* Make sure this is not the case when the */
      /* no_of_afds is stale.                   */
      if ((no_of_afds < 0) && (msa != NULL))
      {
         /* Unmap from MSA. */
         ptr = (char *)msa - AFD_WORD_OFFSET;
#ifdef HAVE_MMAP
         if (munmap(ptr, msa_size) == -1)
#else
         if (munmap_emu((void *)ptr) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to munmap() `%s' : %s"),
                       msa_stat_file, strerror(errno));
         }
         else
         {
            msa = NULL;
         }

         timeout_loops++;
         if (timeout_loops > 200)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Unable to attach to a new MSA."));
            return(INCORRECT);
         }

         /* No need to speed things up here. */
         my_usleep(400000L);
      }

      /* Read the MSA ID. */
      if ((fd = coe_open(msa_id_file, O_RDONLY)) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to open() `%s' : %s"),
                    msa_id_file, strerror(errno));
         return(INCORRECT);
      }

      if (fcntl(fd, F_SETLKW, &rlock) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not get read lock for `%s' : %s"),
                    msa_id_file, strerror(errno));
         (void)close(fd);
         return(INCORRECT);
      }
      if (read(fd, &msa_id, sizeof(int)) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not read the value of the msa_id : %s"),
                    strerror(errno));
         (void)close(fd);
         return(INCORRECT);
      }
      if (close(fd) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Could not close() `%s' : %s"),
                    msa_id_file, strerror(errno));
      }
      (void)snprintf(p_msa_stat_file,
                     MAX_PATH_LENGTH - (p_msa_stat_file - msa_stat_file),
                     ".%d", msa_id);

      if (msa_fd > 0)
      {
         if (close(msa_fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("close() error : %s"), strerror(errno));
         }
      }

      if ((msa_fd = coe_open(msa_stat_file, O_RDONLY)) == -1)
      {
         if (errno == ENOENT)
         {
            if (retries++ > 8)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to open() `%s' : %s"),
                          msa_stat_file, strerror(errno));
               return(INCORRECT);
            }
            else
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          _("Failed to open() `%s' : %s"),
                          msa_stat_file, strerror(errno));
               (void)sleep(1);
               continue;
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to open() `%s' : %s"),
                       msa_stat_file, strerror(errno));
            return(INCORRECT);
         }
      }

#ifdef HAVE_STATX
      if (statx(msa_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(msa_fd, &stat_buf) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                    _("Failed to statx() `%s' : %s"),
#else
                    _("Failed to fstat() `%s' : %s"),
#endif
                    msa_stat_file, strerror(errno));
         (void)close(msa_fd);
         msa_fd = -1;
         return(INCORRECT);
      }
#ifdef HAVE_STATX
      if (stat_buf.stx_size < AFD_WORD_OFFSET)
#else
      if (stat_buf.st_size < AFD_WORD_OFFSET)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("MSA not large enough to contain any meaningful data."));
         (void)close(msa_fd);
         msa_fd = -1;
         return(INCORRECT);
      }

#ifdef HAVE_MMAP
      if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                      stat_buf.stx_size, PROT_READ,
# else
                      stat_buf.st_size, PROT_READ,
# endif
                      MAP_SHARED, msa_fd, 0)) == (caddr_t) -1)
#else
      if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                          stat_buf.stx_size, PROT_READ,
# else
                          stat_buf.st_size, PROT_READ,
# endif
                          MAP_SHARED, msa_stat_file, 0)) == (caddr_t) -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("mmap() error : %s"), strerror(errno));
         (void)close(msa_fd);
         msa_fd = -1;
         return(INCORRECT);
      }

      /* Read number of current MSA. */
      no_of_afds = *(int *)ptr;

      /* Check MSA version number. */
      if ((no_of_afds > 0) &&
          (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_MSA_VERSION))
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                 "This code is compiled for of MSA version %d, but the MSA we try to attach is %d.",
                 CURRENT_MSA_VERSION, (int)(*(ptr + SIZEOF_INT + 1 + 1 + 1)));
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
         if (munmap(ptr, stat_buf.stx_size) == -1)
# else
         if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
         if (munmap_emu(ptr) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to munmap() MSA : %s"), strerror(errno));
         }
         (void)close(msa_fd);
         msa_fd = -1;
         return(INCORRECT_VERSION);
      }

      ptr += AFD_WORD_OFFSET;
      msa = (struct mon_status_area *)ptr;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
      msa_size = stat_buf.stx_size;
# else
      msa_size = stat_buf.st_size;
# endif
#endif
   } while (no_of_afds <= 0);

   return(SUCCESS);
}
