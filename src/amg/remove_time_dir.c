/*
 *  remove_time_dir.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   remove_time_dir - removes all files in a time directory
 **
 ** SYNOPSIS
 **   void remove_time_dir(char         *host_name,
 **                        int          warn_delete,
 **                        unsigned int job_id,
 **                        unsigned int dir_id,
 **                        int          reason,
 **                        char         *cfile,
 **                        int          cfile_pos)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.05.1999 H.Kiehl Created
 **   07.05.2013 H.Kiehl Added warn_delete parameter to warn user that
 **                      files have been removed.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>           /* strerror(), strcpy(), strlen()          */
#include <sys/types.h>
#include <sys/stat.h>         /* S_ISDIR()                               */
#include <dirent.h>           /* opendir(), readdir(), closedir()        */
#include <unistd.h>           /* rmdir(), unlink()                       */
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern char              time_dir[];
#ifdef _DELETE_LOG
extern struct delete_log dl;
#endif

/* #define _CHECK_TIME_DIR_DEBUG 1 */


/*++++++++++++++++++++++++++ remove_time_dir() ++++++++++++++++++++++++++*/
void
#ifdef _DELETE_LOG
remove_time_dir(char         *host_name,
                int          warn_delete,
                unsigned int job_id,
                unsigned int dir_id,
                int          reason,
                char         *cfile,
                int          cfile_pos)
#else
remove_time_dir(char *host_name, int warn_delete, unsigned int job_id)
#endif
{
#ifdef _CHECK_TIME_DIR_DEBUG
   system_log(INFO_SIGN, __FILE__, __LINE__,
              "Removing time directory `%s'", time_dir);
#else
   DIR *dp;

   if ((dp = opendir(time_dir)) == NULL)
   {
      if (errno != ENOENT)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to opendir() `%s' to remove old time jobs : %s",
                    time_dir, strerror(errno));
      }
   }
   else
   {
      int           number_deleted = 0;
      off_t         file_size_deleted = 0;
      char          *ptr;
      struct dirent *p_dir;
      struct stat   stat_buf;

      ptr = time_dir + strlen(time_dir);
      *(ptr++) = '/';

      errno = 0;
      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] == '.')
         {
            continue;
         }
         (void)strcpy(ptr, p_dir->d_name);

         if (stat(time_dir, &stat_buf) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to stat() `%s' : %s", time_dir, strerror(errno));
         }
         else
         {
            if (unlink(time_dir) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to unlink() file `%s' : %s",
                          time_dir, strerror(errno));
            }
            else
            {
# ifdef _DELETE_LOG
               int    prog_name_length;
               size_t dl_real_size;
# endif

               number_deleted++;
               file_size_deleted += stat_buf.st_size;
# ifdef _DELETE_LOG
               (void)strcpy(dl.file_name, p_dir->d_name);
               (void)snprintf(dl.host_name, MAX_HOSTNAME_LENGTH + 4 + 1,
                              "%-*s %03x",
                              MAX_HOSTNAME_LENGTH, host_name, reason);
               *dl.file_size = stat_buf.st_size;
               *dl.job_id = job_id;
               *dl.dir_id = dir_id;
               *dl.input_time = 0L;
               *dl.split_job_counter = 0;
               *dl.unique_number = 0;
               *dl.file_name_length = strlen(p_dir->d_name);
               prog_name_length = snprintf((dl.file_name + *dl.file_name_length + 1),
                                           MAX_FILENAME_LENGTH + 1,
                                           "%s%c(%s %d)",
                                           DIR_CHECK, SEPARATOR_CHAR,
                                           cfile, cfile_pos);
               dl_real_size = *dl.file_name_length + dl.size + prog_name_length;
               if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "write() error : %s", strerror(errno));
               }
# endif
            }
         }
         errno = 0;
      }

      if ((number_deleted > 0) && (warn_delete == YES))
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
# if SIZEOF_OFF_T == 4
                    "Deleted %d files %ld bytes from changed time job for %s", 
# else
                    "Deleted %d files %lld bytes from changed time job for %s", 
# endif
                    number_deleted, (pri_off_t)file_size_deleted, host_name);
      }

      *(ptr - 1) = '\0';
      if (errno)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not readdir() `%s' : %s", time_dir, strerror(errno));
      }
      if (closedir(dp) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not closedir() `%s' : %s",
                    time_dir, strerror(errno));
      }
      if (rmdir(time_dir) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not rmdir() `%s' : %s", time_dir, strerror(errno));
      }
   }
#endif

   return;
}
