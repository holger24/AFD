/*
 *  remove_dir.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   remove_dir - removes one directory with all its files
 **
 ** SYNOPSIS
 **   int remove_dir(char *dirname, int wait_time)
 **
 ** DESCRIPTION
 **   Deletes the directory 'dirname' with all its files. If there
 **   are directories within this directory the function will fail.
 **   For this purpose rather use the function rec_rmdir().
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to delete the directory. Or
 **   FILE_IS_DIR is returned when there are directories in 'dirname'.
 **   When successful it will return 0.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.12.2000 H.Kiehl Created
 **   10.07.2002 H.Kiehl Return FILE_IS_DIR when there are directories
 **                      in dirname.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>             /* strcpy(), strlen()                    */
#include <unistd.h>             /* rmdir(), unlink()                     */
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>             /* Definition of AT_* constants          */
#endif
#include <sys/stat.h>           /* stat()                                */
#include <dirent.h>             /* opendir(), readdir(), closedir()      */
#include <errno.h>


/*############################# remove_dir() ############################*/
int
#ifdef WITH_UNLINK_DELAY
remove_dir(char *dirname, int wait_time)
#else
remove_dir(char *dirname)
#endif
{
   int           addchar = NO;
#ifdef WITH_UNLINK_DELAY
   int           loops = 0;
#endif
   char          *ptr;
   struct dirent *dirp;
   DIR           *dp;

#ifdef WITH_UNLINK_DELAY
try_again2:
#endif
   ptr = dirname + strlen(dirname);

   if ((dp = opendir(dirname)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to opendir() `%s' : %s"), dirname, strerror(errno));
      return(INCORRECT);
   }
   if (*(ptr - 1) != '/')
   {
      *(ptr++) = '/';
      addchar = YES;
   }

   while ((dirp = readdir(dp)) != NULL)
   {
      if ((dirp->d_name[0] == '.') && ((dirp->d_name[1] == '\0') ||
          ((dirp->d_name[1] == '.') && (dirp->d_name[2] == '\0'))))
      {
         continue;
      }
#ifdef LINUX
      if (dirp->d_type == DT_DIR)
      {
         (void)closedir(dp);
         if (addchar == YES)
         {
            ptr[-1] = 0;
         }
         else
         {
            *ptr = '\0';
         }
         return(FILE_IS_DIR);
      }
#endif
      (void)strcpy(ptr, dirp->d_name);
#ifdef WITH_UNLINK_DELAY
try_again:
#endif
      if (unlink(dirname) == -1)
      {
         if (errno == ENOENT)
         {
#ifdef WITH_UNLINK_DELAY
            if (loops == 0)
            {
#endif
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("Failed to delete `%s' : %s"),
                       dirname, strerror(errno));
#ifdef WITH_UNLINK_DELAY
            }
#endif
         }
#ifdef EISDIR
         else if (errno == EISDIR)
              {
                 (void)closedir(dp);
                 if (addchar == YES)
                 {
                    ptr[-1] = 0;
                 }
                 else
                 {
                    *ptr = '\0';
                 }
                 return(FILE_IS_DIR);
              }
#endif
#ifdef WITH_UNLINK_DELAY
         else if ((errno == EBUSY) && (wait_time > 0) &&
                  (loops < (10 * wait_time)))
              {
                 (void)my_usleep(100000L);
                 loops++;
                 goto try_again;
              }
#endif
              else
              {
                 int ret = INCORRECT,
                     save_errno = errno;

                 if (errno == EPERM)
                 {
#ifdef HAVE_STATX
                    struct statx stat_buf;
#else
                    struct stat stat_buf;
#endif

#ifdef HAVE_STATX
                    if (statx(0, dirname, AT_STATX_SYNC_AS_STAT,
                              STATX_MODE, &stat_buf) == -1)
#else
                    if (stat(dirname, &stat_buf) == -1)
#endif
                    {
                       system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                                  _("Failed to statx() `%s' : %s"),
#else
                                  _("Failed to stat() `%s' : %s"),
#endif
                                  dirname, strerror(errno));
                    }
                    else
                    {
#ifdef HAVE_STATX
                       if (S_ISDIR(stat_buf.stx_mode))
#else
                       if (S_ISDIR(stat_buf.st_mode))
#endif
                       {
                          ret = FILE_IS_DIR;
                       }
                    }
                 }
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            _("Failed to delete `%s' : %s"),
                            dirname, strerror(save_errno));
                 (void)closedir(dp);
                 if (addchar == YES)
                 {
                    ptr[-1] = 0;
                 }
                 else
                 {
                    *ptr = '\0';
                 }
                 return(ret);
              }
      }
   }
   if (addchar == YES)
   {
      ptr[-1] = 0;
   }
   else
   {
      *ptr = '\0';
   }
   if (closedir(dp) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to closedir() `%s' : %s"), dirname, strerror(errno));
      return(INCORRECT);
   }
   if (rmdir(dirname) == -1)
   {
#ifdef WITH_UNLINK_DELAY
      if ((errno == ENOTEMPTY) && (wait_time > 0) && (loops < (10 * wait_time)))
      {
         (void)my_usleep(100000L);
         loops++;
         goto try_again2;
      }
#endif
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to delete directory `%s' with rmdir() : %s"),
                 dirname, strerror(errno));
      return(INCORRECT);
   }

   return(SUCCESS);
}
