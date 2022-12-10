/*
 *  rec_rmdir.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2022 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   rec_rmdir - deletes a directory recursive
 **
 ** SYNOPSIS
 **   int rec_rmdir(char *dirname)
 **
 ** DESCRIPTION
 **   Deletes 'dirname' recursive. This means all files and
 **   directories in 'dirname' are removed.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to delete the directory.
 **   When successful it will return 0.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   15.02.1996 H.Kiehl Created
 **   24.09.2000 H.Kiehl Use rmdir() instead of unlink() for removing
 **                      directories. Causes problems with Solaris.
 **   28.02.2005 H.Kiehl Return SUCCESS if the directory we want to
 **                      delete does not exist.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>             /* strcpy(), strlen()                    */
#include <unistd.h>             /* unlink(), rmdir()                     */
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>             /* Definition of AT_* constants          */
#endif
#include <sys/stat.h>           /* lstat(), S_ISDIR()                    */
#include <dirent.h>             /* opendir(), readdir(), closedir()      */
#include <errno.h>


/*############################ rec_rmdir() ##############################*/
int
rec_rmdir(char *dirname)
{
   char          *ptr;
#ifdef HAVE_STATX
   struct statx  stat_buf;
#else
   struct stat   stat_buf;
#endif
   struct dirent *dirp;
   DIR           *dp;
   int           ret = 0;

#ifdef HAVE_STATX
   if (statx(0, dirname, AT_STATX_SYNC_AS_STAT | AT_SYMLINK_NOFOLLOW,
             STATX_MODE, &stat_buf) == -1)
#else
   if (lstat(dirname, &stat_buf) == -1)
#endif
   {
      if (errno != ENOENT)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                    _("Failed to statx() `%s' : %s"), dirname, strerror(errno));
#else
                    _("Failed to lstat() `%s' : %s"), dirname, strerror(errno));
#endif
         return(INCORRECT);
      }
      else
      {
         /* If the directory does not exist lets assume it is okay. */
         return(SUCCESS);
      }
   }

   /* Make sure it is NOT a directory. */
#ifdef HAVE_STATX
   if (S_ISDIR(stat_buf.stx_mode) == 0)
#else
   if (S_ISDIR(stat_buf.st_mode) == 0)
#endif
   {
      if (unlink(dirname) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to delete `%s' : %s"), dirname, strerror(errno));
         return(INCORRECT);
      }
      return(0);
   }

   /* It's a directory. */
   ptr = dirname + strlen(dirname);
   *ptr++ = '/';
   *ptr = '\0';

   if ((dp = opendir(dirname)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to opendir() `%s' : %s"), dirname, strerror(errno));
      return(INCORRECT);
   }

   while ((dirp = readdir(dp)) != NULL)
   {
      if ((dirp->d_name[0] == '.') && ((dirp->d_name[1] == '\0') ||
          ((dirp->d_name[1] == '.') && (dirp->d_name[2] == '\0'))))
      {
         continue;
      }
      (void)strcpy(ptr, dirp->d_name);
      if ((ret = rec_rmdir(dirname)) != 0)
      {
         break;
      }
   }
   ptr[-1] = 0;
   if (ret == 0)
   {
      if (rmdir(dirname) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to delete directory `%s' with rmdir() : %s"),
                    dirname, strerror(errno));
         (void)closedir(dp);
         return(INCORRECT);
      }
   }
   if (closedir(dp) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to closedir() `%s' : %s"), dirname, strerror(errno));
      return(INCORRECT);
   }

   return(ret);
}
