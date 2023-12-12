/*
 *  fsa_attach.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   fsa_attach - attaches to the FSA
 **
 ** SYNOPSIS
 **   int fsa_attach(char *who)
 **   int fsa_attach_passive(int silent, char *who)
 **   int fsa_attach_features(char *who)
 **   int fsa_attach_features_passive(int silent, char *who)
 **   int fsa_check_id_changed(int fsa_id)
 **
 ** DESCRIPTION
 **   Attaches to the memory mapped area of the FSA (File Transfer
 **   Status Area). The first 8 bytes of this area is build up as
 **   follows (assuming SIZEOF_INT is 4):
 **     Byte  | Type          | Description
 **    -------+---------------+---------------------------------------
 **     1 - 4 | int           | The number of hosts served by the AFD.
 **           |               | If this FSA in no longer in use there
 **           |               | will be -1 here.
 **    -------+---------------+---------------------------------------
 **       5   | unsigned char | Counter that is increased each time
 **           |               | there was a change in the HOST_CONFIG.
 **    -------+---------------+---------------------------------------
 **       6   | unsigned char | Flag to enable or disable certain
 **           |               | features in the AFD.
 **           |               | Bit| Meaning
 **           |               | ---+-------------------------
 **           |               |  1 | DISABLE_RETRIEVE
 **           |               |  2 | DISABLE_ARCHIVE
 **           |               |  3 | ENABLE_CREATE_TARGET_DIR
 **           |               |  4 | DISABLE_HOST_WARN_TIME
 **           |               |  5 | DISABLE_CREATE_SOURCE_DIR
 **           |               |  6 | ENABLE_SIMULATE_SEND_MODE
 **    -------+---------------+---------------------------------------
 **       7   | unsigned char | The number of errors that are shown
 **           |               | in offline mode. 0 (default) means all
 **           |               | are shown as errors/warning.
 **    -------+---------------+---------------------------------------
 **       8   | unsigned char | Version of the FSA structure.
 **   --------+---------------+--------------------------------------
 **    9 - 12 | int           | Pagesize of this system.
 **   --------+---------------+--------------------------------------
 **   13 - 16 |               | Not used.
 **   --------+---------------+--------------------------------------
 **   The rest consist of the structure filetransfer_status for each
 **   host. When the function returns successful it returns the pointer
 **   'fsa' to the begining of the first structure.
 **
 **   fsa_attach_passive() attaches to the FSA in read only mode. If
 **   silent is set to YES it will not report an error if the FSA file
 **   does not exist.
 **
 ** RETURN VALUES
 **   SUCCESS when attaching to the FSA successful and sets the
 **   global pointer 'fsa' to the start of the FSA. Also the FSA ID,
 **   the size of the FSA and the number of host in the FSA is returned
 **   in 'fsa_id', 'fsa_size' and 'no_of_hosts' respectively. On error
 **   INCORRECT or errno is returned.
 **
 ** SEE ALSO
 **   check_fsa()
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   24.12.1995 H.Kiehl Created
 **   15.08.1997 H.Kiehl When failing to open() the FSA file don't just
 **                      exit. Give it another try.
 **   29.04.2003 H.Kiehl Added function fsa_attach_passive().
 **   05.02.2013 H.Kiehl Added parameter who so we can see who tried to
 **                      attach.
 **   24.02.2024 H.Kiehl Added fsa_attach_features() and
 **                      fsa_attach_features_passive() to attach only
 **                      to the first AFD_WORD_OFFSET bytes at the
 **                      begining.
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
extern int                        fsa_fd,
                                  fsa_id,
                                  no_of_hosts;
#ifdef HAVE_MMAP
extern off_t                      fsa_size;
#endif
extern char                       *p_work_dir;
extern struct filetransfer_status *fsa;


/*############################ fsa_attach() #############################*/
int
fsa_attach(char *who)
{
   int          fd,
                loop_counter,
                retries = 0,
                tmp_errno,
                timeout_loops = 0;
   char         *ptr = NULL,
                *p_fsa_stat_file,
                fsa_id_file[MAX_PATH_LENGTH],
                fsa_stat_file[MAX_PATH_LENGTH];
   struct flock wlock = {F_WRLCK, SEEK_SET, 0, 1},
                ulock = {F_UNLCK, SEEK_SET, 0, 1};
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   /* Get absolute path of FSA_ID_FILE. */
   (void)strcpy(fsa_id_file, p_work_dir);
   (void)strcat(fsa_id_file, FIFO_DIR);
   (void)strcpy(fsa_stat_file, fsa_id_file);
   (void)strcat(fsa_stat_file, FSA_STAT_FILE);
   p_fsa_stat_file = fsa_stat_file + strlen(fsa_stat_file);
   (void)strcat(fsa_id_file, FSA_ID_FILE);

   do
   {
      /* Make sure this is not the case when the */
      /* no_of_hosts is stale.                   */
      if ((no_of_hosts < 0) && (fsa != NULL))
      {
         /* Unmap from FSA. */
         ptr = (char *)fsa - AFD_WORD_OFFSET;
#ifdef HAVE_MMAP
         if (munmap(ptr, fsa_size) == -1)
#else
         if (munmap_emu((void *)ptr) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to munmap() `%s' [%s] : %s"),
                       fsa_stat_file, who, strerror(errno));
         }
         else
         {
            fsa = NULL;
         }

         timeout_loops++;
         if (timeout_loops > 200)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Unable to attach to a new FSA [%s]."), who);
            return(INCORRECT);
         }

         /* No need to speed things up here. */
         my_usleep(400000L);
      }

      /*
       * Retrieve the FSA (Filetransfer Status Area) ID from FSA_ID_FILE.
       * Make sure that it is not locked.
       */
      loop_counter = 0;
      while ((fd = coe_open(fsa_id_file, O_RDWR)) == -1)
      {
         tmp_errno = errno;
         if (errno == ENOENT)
         {
            my_usleep(400000L);
            if (loop_counter++ > 24)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to open() `%s' [%s] : %s"),
                          fsa_id_file, who, strerror(errno));
               return(tmp_errno);
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to open() `%s' [%s] : %s"),
                       fsa_id_file, who, strerror(errno));
            return(tmp_errno);
         }
      }

      /* Check if its locked. */
      if (fcntl(fd, F_SETLKW, &wlock) == -1)
      {
         tmp_errno = errno;
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not set write lock for `%s' [%s] : %s"),
                    fsa_id_file, who, strerror(errno));
         (void)close(fd);
         return(tmp_errno);
      }

      /* Read the fsa_id. */
      if (read(fd, &fsa_id, sizeof(int)) < 0)
      {
         tmp_errno = errno;
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not read the value of the fsa_id [%s] : %s"),
                    who, strerror(errno));
         (void)close(fd);
         return(tmp_errno);
      }

      /* Unlock file and close it. */
      if (fcntl(fd, F_SETLKW, &ulock) == -1)
      {
         tmp_errno = errno;
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not unlock `%s' [%s] : %s"),
                    fsa_id_file, who, strerror(errno));
         (void)close(fd);
         return(tmp_errno);
      }
      if (close(fd) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Could not close() `%s' [%s] : %s"),
                    fsa_id_file, who, strerror(errno));
      }

      (void)snprintf(p_fsa_stat_file,
                     MAX_PATH_LENGTH - (p_fsa_stat_file - fsa_stat_file),
                     ".%d", fsa_id);

      if (fsa_fd > 0)
      {
         if (close(fsa_fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("close() error [%s] : %s"), who, strerror(errno));
         }
      }

      if ((fsa_fd = coe_open(fsa_stat_file, O_RDWR)) == -1)
      {
         tmp_errno = errno;
         if (errno == ENOENT)
         {
            if (retries++ > 8)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to open() `%s' [%s] : %s"),
                          fsa_stat_file, who, strerror(errno));
               return(tmp_errno);
            }
            else
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          _("Failed to open() `%s' [%s] : %s"),
                          fsa_stat_file, who, strerror(errno));
               (void)sleep(1);
               continue;
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to open() `%s' [%s] : %s"),
                       fsa_stat_file, who, strerror(errno));
            return(tmp_errno);
         }
      }

#ifdef HAVE_STATX
      if (statx(fsa_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(fsa_fd, &stat_buf) == -1)
#endif
      {
         tmp_errno = errno;
         system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                    _("Failed to statx() `%s' [%s] : %s"),
#else
                    _("Failed to fstat() `%s' [%s] : %s"),
#endif
                    fsa_stat_file, who, strerror(errno));
         (void)close(fsa_fd);
         fsa_fd = -1;
         return(tmp_errno);
      }

#ifdef HAVE_MMAP
      if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                      stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                      stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                      MAP_SHARED, fsa_fd, 0)) == (caddr_t) -1)
#else
      if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                          stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                          stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                          MAP_SHARED, fsa_stat_file, 0)) == (caddr_t) -1)
#endif
      {
         tmp_errno = errno;
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("mmap() error [%s] : %s"), who, strerror(errno));
         (void)close(fsa_fd);
         fsa_fd = -1;
         return(tmp_errno);
      }

      /* Read number of current FSA. */
      no_of_hosts = *(int *)ptr;

      /* Check FSA version number. */
      if ((no_of_hosts > 0) &&
          (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_FSA_VERSION))
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "This code is compiled for of FSA version %d, but the FSA we try to attach is %d [%s].",
                    CURRENT_FSA_VERSION, (int)(*(ptr + SIZEOF_INT + 1 + 1 + 1)),
                    who);
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
                       _("Failed to munmap() FSA [%s] : %s"),
                       who, strerror(errno));
         }
         (void)close(fsa_fd);
         fsa_fd = -1;
         return(INCORRECT_VERSION);
      }

      ptr += AFD_WORD_OFFSET;
      fsa = (struct filetransfer_status *)ptr;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
      fsa_size = stat_buf.stx_size;
# else
      fsa_size = stat_buf.st_size;
# endif
#endif
   } while (no_of_hosts <= 0);

   return(SUCCESS);
}


/*######################## fsa_attach_passive() #########################*/
int
fsa_attach_passive(int silent, char *who)
{
   int          fd,
                retries = 0,
                timeout_loops = 0,
                tmp_errno;
   char         *ptr = NULL,
                *p_fsa_stat_file,
                fsa_id_file[MAX_PATH_LENGTH],
                fsa_stat_file[MAX_PATH_LENGTH];
   struct flock rlock = {F_RDLCK, SEEK_SET, 0, 1};
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   /* Get absolute path of FSA_ID_FILE. */
   (void)strcpy(fsa_id_file, p_work_dir);
   (void)strcat(fsa_id_file, FIFO_DIR);
   (void)strcpy(fsa_stat_file, fsa_id_file);
   (void)strcat(fsa_stat_file, FSA_STAT_FILE);
   p_fsa_stat_file = fsa_stat_file + strlen(fsa_stat_file);
   (void)strcat(fsa_id_file, FSA_ID_FILE);

   do
   {
      /* Make sure this is not the case when the */
      /* no_of_hosts is stale.                   */
      if ((no_of_hosts < 0) && (fsa != NULL))
      {
         /* Unmap from FSA. */
         ptr = (char *)fsa - AFD_WORD_OFFSET;
#ifdef HAVE_MMAP
         if (munmap(ptr, fsa_size) == -1)
#else
         if (munmap_emu((void *)ptr) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to munmap() `%s' [%s] : %s"),
                       fsa_stat_file, who, strerror(errno));
         }
         else
         {
            fsa = NULL;
         }

         timeout_loops++;
         if (timeout_loops > 200)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Unable to attach to a new FSA [%s]."), who);
            return(INCORRECT);
         }

         /* No need to speed things up here. */
         my_usleep(400000L);
      }

      /* Read the FSA ID. */
      if ((fd = coe_open(fsa_id_file, O_RDONLY)) == -1)
      {
         tmp_errno = errno;
         if (silent == NO)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to open() `%s' [%s] : %s"),
                       fsa_id_file, who, strerror(errno));
         }
         return(tmp_errno);
      }

      if (fcntl(fd, F_SETLKW, &rlock) == -1)
      {
         tmp_errno = errno;
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not get read lock for `%s' [%s] : %s"),
                    fsa_id_file, who, strerror(errno));
         (void)close(fd);
         return(tmp_errno);
      }
      if (read(fd, &fsa_id, sizeof(int)) < 0)
      {
         tmp_errno = errno;
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not read the value of the fsa_id [%s] : %s"),
                    who, strerror(errno));
         (void)close(fd);
         return(tmp_errno);
      }
      if (close(fd) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Could not close() `%s' [%s] : %s"),
                    fsa_id_file, who, strerror(errno));
      }
      (void)snprintf(p_fsa_stat_file,
                     MAX_PATH_LENGTH - (p_fsa_stat_file - fsa_stat_file),
                     ".%d", fsa_id);

      if (fsa_fd > 0)
      {
         if (close(fsa_fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("close() error [%s] : %s"), who, strerror(errno));
         }
      }

      if ((fsa_fd = coe_open(fsa_stat_file, O_RDONLY)) == -1)
      {
         tmp_errno = errno;
         if (errno == ENOENT)
         {
            if (retries++ > 8)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to open() `%s' [%s] : %s"),
                          fsa_stat_file, who, strerror(errno));
               return(tmp_errno);
            }
            else
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          _("Failed to open() `%s' [%s] : %s"),
                          fsa_stat_file, who, strerror(errno));
               (void)sleep(1);
               continue;
            }
         }
         else
         {
            if (silent == NO)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to open() `%s' [%s] : %s"),
                          fsa_stat_file, who, strerror(errno));
            }
            return(tmp_errno);
         }
      }

#ifdef HAVE_STATX
      if (statx(fsa_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(fsa_fd, &stat_buf) == -1)
#endif
      {
         tmp_errno = errno;
         system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                    _("Failed to statx() `%s' [%s] : %s"),
#else
                    _("Failed to fstat() `%s' [%s] : %s"),
#endif
                    fsa_stat_file, who, strerror(errno));
         (void)close(fsa_fd);
         fsa_fd = -1;
         return(tmp_errno);
      }
#ifdef HAVE_STATX
      if (stat_buf.stx_size < AFD_WORD_OFFSET)
#else
      if (stat_buf.st_size < AFD_WORD_OFFSET)
#endif
      {
         tmp_errno = errno;
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("FSA not large enough to contain any meaningful data [%s]."),
                    who);
         (void)close(fsa_fd);
         fsa_fd = -1;
         return(tmp_errno);
      }

#ifdef HAVE_MMAP
      if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                      stat_buf.stx_size, PROT_READ,
# else
                      stat_buf.st_size, PROT_READ,
# endif
                      MAP_SHARED, fsa_fd, 0)) == (caddr_t) -1)
#else
      if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                          stat_buf.stx_size, PROT_READ,
# else
                          stat_buf.st_size, PROT_READ,
# endif
                          MAP_SHARED, fsa_stat_file, 0)) == (caddr_t) -1)
#endif
      {
         tmp_errno = errno;
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("mmap() error [%s] : %s"), who, strerror(errno));
         (void)close(fsa_fd);
         fsa_fd = -1;
         return(tmp_errno);
      }

      /* Read number of current FSA. */
      no_of_hosts = *(int *)ptr;

      /* Check FSA version number. */
      if ((no_of_hosts > 0) &&
          (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_FSA_VERSION))
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("This code is compiled for of FSA version %d, but the FSA we try to attach is %d [%s]."),
                    CURRENT_FSA_VERSION, (int)(*(ptr + SIZEOF_INT + 1 + 1 + 1)),
                    who);
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
                       _("Failed to munmap() FSA [%s] : %s"),
                       who, strerror(errno));
         }
         (void)close(fsa_fd);
         fsa_fd = -1;
         return(INCORRECT_VERSION);
      }

      ptr += AFD_WORD_OFFSET;
      fsa = (struct filetransfer_status *)ptr;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
      fsa_size = stat_buf.stx_size;
# else
      fsa_size = stat_buf.st_size;
# endif
#endif
   } while (no_of_hosts <= 0);

   return(SUCCESS);
}


/*######################## fsa_attach_features() ########################*/
int
fsa_attach_features(char *who)
{
   int          fd,
                loop_counter,
                retries = 0,
                tmp_errno,
                timeout_loops = 0;
   char         *ptr = NULL,
                *p_fsa_stat_file,
                fsa_id_file[MAX_PATH_LENGTH],
                fsa_stat_file[MAX_PATH_LENGTH];
   struct flock wlock = {F_WRLCK, SEEK_SET, 0, 1},
                ulock = {F_UNLCK, SEEK_SET, 0, 1};

   /* Get absolute path of FSA_ID_FILE. */
   (void)strcpy(fsa_id_file, p_work_dir);
   (void)strcat(fsa_id_file, FIFO_DIR);
   (void)strcpy(fsa_stat_file, fsa_id_file);
   (void)strcat(fsa_stat_file, FSA_STAT_FILE);
   p_fsa_stat_file = fsa_stat_file + strlen(fsa_stat_file);
   (void)strcat(fsa_id_file, FSA_ID_FILE);

   do
   {
      /* Make sure this is not the case when the */
      /* no_of_hosts is stale.                   */
      if ((no_of_hosts < 0) && (fsa != NULL))
      {
         /* Unmap from FSA. */
         ptr = (char *)fsa - AFD_WORD_OFFSET;
#ifdef HAVE_MMAP
         if (munmap(ptr, fsa_size) == -1)
#else
         if (munmap_emu((void *)ptr) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to munmap() `%s' [%s] : %s"),
                       fsa_stat_file, who, strerror(errno));
         }
         else
         {
            fsa = NULL;
         }

         timeout_loops++;
         if (timeout_loops > 200)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Unable to attach to a new FSA [%s]."), who);
            return(INCORRECT);
         }

         /* No need to speed things up here. */
         my_usleep(400000L);
      }

      /*
       * Retrieve the FSA (Filetransfer Status Area) ID from FSA_ID_FILE.
       * Make sure that it is not locked.
       */
      loop_counter = 0;
      while ((fd = coe_open(fsa_id_file, O_RDWR)) == -1)
      {
         tmp_errno = errno;
         if (errno == ENOENT)
         {
            my_usleep(400000L);
            if (loop_counter++ > 24)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to open() `%s' [%s] : %s"),
                          fsa_id_file, who, strerror(errno));
               return(tmp_errno);
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to open() `%s' [%s] : %s"),
                       fsa_id_file, who, strerror(errno));
            return(tmp_errno);
         }
      }

      /* Check if its locked. */
      if (fcntl(fd, F_SETLKW, &wlock) == -1)
      {
         tmp_errno = errno;
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not set write lock for `%s' [%s] : %s"),
                    fsa_id_file, who, strerror(errno));
         (void)close(fd);
         return(tmp_errno);
      }

      /* Read the fsa_id. */
      if (read(fd, &fsa_id, sizeof(int)) < 0)
      {
         tmp_errno = errno;
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not read the value of the fsa_id [%s] : %s"),
                    who, strerror(errno));
         (void)close(fd);
         return(tmp_errno);
      }

      /* Unlock file and close it. */
      if (fcntl(fd, F_SETLKW, &ulock) == -1)
      {
         tmp_errno = errno;
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not unlock `%s' [%s] : %s"),
                    fsa_id_file, who, strerror(errno));
         (void)close(fd);
         return(tmp_errno);
      }
      if (close(fd) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Could not close() `%s' [%s] : %s"),
                    fsa_id_file, who, strerror(errno));
      }

      (void)snprintf(p_fsa_stat_file,
                     MAX_PATH_LENGTH - (p_fsa_stat_file - fsa_stat_file),
                     ".%d", fsa_id);

      if (fsa_fd > 0)
      {
         if (close(fsa_fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("close() error [%s] : %s"), who, strerror(errno));
         }
      }

      if ((fsa_fd = coe_open(fsa_stat_file, O_RDWR)) == -1)
      {
         tmp_errno = errno;
         if (errno == ENOENT)
         {
            if (retries++ > 8)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to open() `%s' [%s] : %s"),
                          fsa_stat_file, who, strerror(errno));
               return(tmp_errno);
            }
            else
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          _("Failed to open() `%s' [%s] : %s"),
                          fsa_stat_file, who, strerror(errno));
               (void)sleep(1);
               continue;
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to open() `%s' [%s] : %s"),
                       fsa_stat_file, who, strerror(errno));
            return(tmp_errno);
         }
      }

#ifdef HAVE_MMAP
      if ((ptr = mmap(NULL,
                      AFD_WORD_OFFSET, (PROT_READ | PROT_WRITE),
                      MAP_SHARED, fsa_fd, 0)) == (caddr_t) -1)
#else
      if ((ptr = mmap_emu(NULL,
                          AFD_WORD_OFFSET, (PROT_READ | PROT_WRITE),
                          MAP_SHARED, fsa_stat_file, 0)) == (caddr_t) -1)
#endif
      {
         tmp_errno = errno;
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("mmap() error [%s] : %s"), who, strerror(errno));
         (void)close(fsa_fd);
         fsa_fd = -1;
         return(tmp_errno);
      }

      /* Read number of current hosts. */
      no_of_hosts = *(int *)ptr;

      ptr += AFD_WORD_OFFSET;
      fsa = (struct filetransfer_status *)ptr;
#ifdef HAVE_MMAP
      fsa_size = AFD_WORD_OFFSET;
#endif
   } while (no_of_hosts <= 0);

   return(SUCCESS);
}


/*##################### fsa_attach_features_passive() #####################*/
int
fsa_attach_features_passive(int silent, char *who)
{
   int          fd,
                retries = 0,
                timeout_loops = 0,
                tmp_errno;
   char         *ptr = NULL,
                *p_fsa_stat_file,
                fsa_id_file[MAX_PATH_LENGTH],
                fsa_stat_file[MAX_PATH_LENGTH];
   struct flock rlock = {F_RDLCK, SEEK_SET, 0, 1};

   /* Get absolute path of FSA_ID_FILE. */
   (void)strcpy(fsa_id_file, p_work_dir);
   (void)strcat(fsa_id_file, FIFO_DIR);
   (void)strcpy(fsa_stat_file, fsa_id_file);
   (void)strcat(fsa_stat_file, FSA_STAT_FILE);
   p_fsa_stat_file = fsa_stat_file + strlen(fsa_stat_file);
   (void)strcat(fsa_id_file, FSA_ID_FILE);

   do
   {
      /* Make sure this is not the case when the */
      /* no_of_hosts is stale.                   */
      if ((no_of_hosts < 0) && (fsa != NULL))
      {
         /* Unmap from FSA. */
         ptr = (char *)fsa - AFD_WORD_OFFSET;
#ifdef HAVE_MMAP
         if (munmap(ptr, fsa_size) == -1)
#else
         if (munmap_emu((void *)ptr) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to munmap() `%s' [%s] : %s"),
                       fsa_stat_file, who, strerror(errno));
         }
         else
         {
            fsa = NULL;
         }

         timeout_loops++;
         if (timeout_loops > 200)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Unable to attach to a new FSA [%s]."), who);
            return(INCORRECT);
         }

         /* No need to speed things up here. */
         my_usleep(400000L);
      }

      /* Read the FSA ID. */
      if ((fd = coe_open(fsa_id_file, O_RDONLY)) == -1)
      {
         tmp_errno = errno;
         if (silent == NO)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to open() `%s' [%s] : %s"),
                       fsa_id_file, who, strerror(errno));
         }
         return(tmp_errno);
      }

      if (fcntl(fd, F_SETLKW, &rlock) == -1)
      {
         tmp_errno = errno;
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not get read lock for `%s' [%s] : %s"),
                    fsa_id_file, who, strerror(errno));
         (void)close(fd);
         return(tmp_errno);
      }
      if (read(fd, &fsa_id, sizeof(int)) < 0)
      {
         tmp_errno = errno;
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not read the value of the fsa_id [%s] : %s"),
                    who, strerror(errno));
         (void)close(fd);
         return(tmp_errno);
      }
      if (close(fd) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Could not close() `%s' [%s] : %s"),
                    fsa_id_file, who, strerror(errno));
      }
      (void)snprintf(p_fsa_stat_file,
                     MAX_PATH_LENGTH - (p_fsa_stat_file - fsa_stat_file),
                     ".%d", fsa_id);

      if (fsa_fd > 0)
      {
         if (close(fsa_fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("close() error [%s] : %s"), who, strerror(errno));
         }
      }

      if ((fsa_fd = coe_open(fsa_stat_file, O_RDONLY)) == -1)
      {
         tmp_errno = errno;
         if (errno == ENOENT)
         {
            if (retries++ > 8)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to open() `%s' [%s] : %s"),
                          fsa_stat_file, who, strerror(errno));
               return(tmp_errno);
            }
            else
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          _("Failed to open() `%s' [%s] : %s"),
                          fsa_stat_file, who, strerror(errno));
               (void)sleep(1);
               continue;
            }
         }
         else
         {
            if (silent == NO)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to open() `%s' [%s] : %s"),
                          fsa_stat_file, who, strerror(errno));
            }
            return(tmp_errno);
         }
      }

#ifdef HAVE_MMAP
      if ((ptr = mmap(NULL,
                      AFD_WORD_OFFSET, PROT_READ,
                      MAP_SHARED, fsa_fd, 0)) == (caddr_t) -1)
#else
      if ((ptr = mmap_emu(NULL,
                          AFD_WORD_OFFSET, PROT_READ,
                          MAP_SHARED, fsa_stat_file, 0)) == (caddr_t) -1)
#endif
      {
         tmp_errno = errno;
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("mmap() error [%s] : %s"), who, strerror(errno));
         (void)close(fsa_fd);
         fsa_fd = -1;
         return(tmp_errno);
      }

      /* Read number of current FSA. */
      no_of_hosts = *(int *)ptr;

      ptr += AFD_WORD_OFFSET;
      fsa = (struct filetransfer_status *)ptr;
#ifdef HAVE_MMAP
      fsa_size = AFD_WORD_OFFSET;
#endif
   } while (no_of_hosts <= 0);

   return(SUCCESS);
}


/*####################### fsa_check_id_changed() ########################*/
int
fsa_check_id_changed(int attached_fsa_id)
{
   int          current_fsa_id,
                fd,
                loop_counter;
   char         fsa_id_file[MAX_PATH_LENGTH];
   struct flock wlock = {F_WRLCK, SEEK_SET, 0, 1},
                ulock = {F_UNLCK, SEEK_SET, 0, 1};

   /* Get absolute path of FSA_ID_FILE. */
   (void)strcpy(fsa_id_file, p_work_dir);
   (void)strcat(fsa_id_file, FIFO_DIR);
   (void)strcat(fsa_id_file, FSA_ID_FILE);

   /*
    * Retrieve the FSA (Filetransfer Status Area) ID from FSA_ID_FILE.
    * Make sure that it is not locked.
    */
   loop_counter = 0;
   while ((fd = coe_open(fsa_id_file, O_RDWR)) == -1)
   {
      if (errno == ENOENT)
      {
         my_usleep(400000L);
         if (loop_counter++ > 24)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to open() `%s' : %s"),
                       fsa_id_file, strerror(errno));
            return(INCORRECT);
         }
      }
      else
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to open() `%s' : %s"),
                    fsa_id_file, strerror(errno));
         return(INCORRECT);
      }
   }

   /* Check if its locked. */
   if (fcntl(fd, F_SETLKW, &wlock) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not set write lock for `%s' : %s"),
                 fsa_id_file, strerror(errno));
      (void)close(fd);
      return(INCORRECT);
   }

   /* Read the fsa_id. */
   if (read(fd, &current_fsa_id, sizeof(int)) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not read the value of the fsa_id : %s"),
                 strerror(errno));
      (void)close(fd);
      return(INCORRECT);
   }

   /* Unlock file and close it. */
   if (fcntl(fd, F_SETLKW, &ulock) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not unlock `%s' : %s"),
                 fsa_id_file, strerror(errno));
      (void)close(fd);
      return(INCORRECT);
   }
   if (close(fd) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 _("Could not close() `%s' : %s"),
                 fsa_id_file, strerror(errno));
   }

   if (current_fsa_id != attached_fsa_id)
   {
      return(YES);
   }
   else
   {
      return(NO);
   }
}
