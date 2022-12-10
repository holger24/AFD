/*
 *  remove_pool_directory.c - Part of AFD, an automatic file distribution
 *                            program.
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
 **   remove_pool_directory - removes all files in a job directory
 **
 ** SYNOPSIS
 **   void remove_pool_directory(char *job_dir, unsigned int dir_id)
 **
 ** DESCRIPTION
 **   The function remove_pool_directory() removes all files in job_dir
 **   and the job_directory. All files deleted are logged in the
 **   delete log.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.05.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                     /* sprintf()                      */
#include <string.h>                    /* strerror()                     */
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>                    /* Definition of AT_* constants   */
#endif
#include <sys/stat.h>
#include <dirent.h>                    /* opendir(), closedir(), DIR,    */
                                       /* readdir(), struct dirent       */
#include <unistd.h>                    /* rmdir(), unlink()              */
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
#ifdef _DELETE_LOG
extern struct delete_log dl;
#endif


/*####################### remove_pool_directory() ########################*/
void
remove_pool_directory(char *job_dir, unsigned int dir_id)
{
#ifdef _DELETE_LOG
   char          *ptr;
# ifdef HAVE_STATX
   struct statx  stat_buf;
# else
   struct stat   stat_buf;
# endif
   struct dirent *p_dir;
   DIR           *dp;

   if ((dp = opendir(job_dir)) == NULL)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Could not opendir() %s : %s", job_dir, strerror(errno));
      return;
   }
   ptr = job_dir + strlen(job_dir);
   *(ptr++) = '/';

   errno = 0;
   while ((p_dir = readdir(dp)) != NULL)
   {
# ifdef LINUX
      if ((p_dir->d_type != DT_DIR) && (p_dir->d_name[0] != '.'))
# else
      if (p_dir->d_name[0] != '.')
# endif
      {
         (void)strcpy(ptr, p_dir->d_name);
# ifdef HAVE_STATX
         if (statx(0, job_dir, AT_STATX_SYNC_AS_STAT,
#  ifdef LINUX
                   STATX_SIZE,
#  else
                   STATX_SIZE | STATX_MODE,
#  endif
                   &stat_buf) == -1)
# else
         if (stat(job_dir, &stat_buf) == -1)
# endif
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Can't access file %s : %s", job_dir, strerror(errno));
            continue;
         }

# ifndef LINUX
         /* Sure it is a normal file? */
#  ifdef HAVE_STATX
         if (S_ISDIR(stat_buf.stx_mode) == 0)
#  else
         if (S_ISDIR(stat_buf.st_mode) == 0)
#  endif
         {
# endif
            if (unlink(job_dir) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to unlink() file %s : %s",
                          p_dir->d_name, strerror(errno));
            }
            else
            {
               int    prog_name_length;
               size_t dl_real_size;

               (void)strcpy(dl.file_name, p_dir->d_name);
               (void)snprintf(dl.host_name,
                              MAX_HOSTNAME_LENGTH + 4 + 1,
                              "%-*s %03x",
                              MAX_HOSTNAME_LENGTH, "-", DELETE_UNKNOWN_POOL_DIR);
# ifdef HAVE_STATX
               *dl.file_size = stat_buf.stx_size;
# else
               *dl.file_size = stat_buf.st_size;
# endif
               *dl.dir_id = dir_id;
               *dl.job_id = 0;
               *dl.input_time = 0L;
               *dl.split_job_counter = 0;
               *dl.unique_number = 0;
               *dl.file_name_length = strlen(p_dir->d_name);
               *(ptr - 1) = '\0';
               prog_name_length = snprintf((dl.file_name + *dl.file_name_length + 1),
                                           MAX_FILENAME_LENGTH + 1,
                                           "%s%c%s",
                                           AMG, SEPARATOR_CHAR, job_dir);
               *(ptr - 1) = '/';
               dl_real_size = *dl.file_name_length + dl.size + prog_name_length;
               if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "write() error : %s", strerror(errno));
               }
            }
# ifndef LINUX
         }
# endif
      }
      errno = 0;
   } /* while ((p_dir = readdir(dp)) != NULL) */

   *ptr = '\0';
   if (errno)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not readdir() %s : %s", job_dir, strerror(errno));
   }
   if (closedir(dp) < 0)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Could not closedir() %s : %s", job_dir, strerror(errno));
   }

   if (rmdir(job_dir) == -1)
   {
      if ((errno == ENOTEMPTY) || (errno == EEXIST))
      {
         (void)rec_rmdir(job_dir);
      }
      else
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to rmdir() %s : %s", job_dir, strerror(errno));
      }
   }
#endif /* _DELETE_LOG */

   return;
}
