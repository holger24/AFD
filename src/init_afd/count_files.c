/*
 *  count_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   count_files - counts the number of files and bytes in directory
 **
 ** SYNOPSIS
 **   void count_files(char *dirname, unsigned int *files, off_t *bytes)
 **
 ** DESCRIPTION
 **   The function count_files counts the number of files and bytes
 **   int the directory 'dirname'. If there are directories in
 **   'dirname' they will not be counted.
 **
 ** RETURN VALUES
 **   Returns the number of files (in 'files') and the number of bytes
 **   (in 'bytes') in the directory.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   05.05.2002 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>             /* strcpy(), strlen()                    */
#include <unistd.h>
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>             /* Definition of AT_* constants          */
#endif
#include <sys/stat.h>           /* stat(), S_ISDIR()                     */
#include <dirent.h>             /* opendir(), readdir(), closedir()      */
#include <errno.h>


/*########################### count_files() #############################*/
void
count_files(char *dirname, unsigned int *files, off_t *bytes)
{
   DIR *dp;

   *files = 0;
   *bytes = 0;
   if ((dp = opendir(dirname)) == NULL)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 _("Can't access directory %s : %s"), dirname, strerror(errno));
   }
   else
   {
      char          *ptr,
                    fullname[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
      struct statx  stat_buf;
#else
      struct stat   stat_buf;
#endif
      struct dirent *p_dir;

      (void)strcpy(fullname, dirname);
      ptr = fullname + strlen(fullname);
      *ptr++ = '/';

      errno = 0;
      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] != '.')
         {
            (void)strcpy(ptr, p_dir->d_name);
#ifdef HAVE_STATX
            if (statx(0, fullname, AT_STATX_SYNC_AS_STAT,
                      STATX_MODE | STATX_SIZE, &stat_buf) == -1)
#else
            if (stat(fullname, &stat_buf) == -1)
#endif
            {
               if (errno != ENOENT)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Can't access file %s : %s",
                             fullname, strerror(errno));
               }
            }
            else
            {
               /* Sure it is a normal file? */
#ifdef HAVE_STATX
               if (S_ISREG(stat_buf.stx_mode))
#else
               if (S_ISREG(stat_buf.st_mode))
#endif
               {
#ifdef HAVE_STATX
                  (*bytes) += stat_buf.stx_size;
#else
                  (*bytes) += stat_buf.st_size;
#endif
                  (*files)++;
               }
            }
         }
         errno = 0;
      }
      if (errno)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not readdir() %s : %s"), dirname, strerror(errno));
      }

      if (closedir(dp) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not closedir() %s : %s"),
                    dirname, strerror(errno));
      }
   }

   return;
}
