/*
 *  remove_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2006 - 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   remove_files - removes the given files in the specified directory
 **
 ** SYNOPSIS
 **   int remove_files(char *dirname, char *filter)
 **
 ** DESCRIPTION
 **   Deletes 'filter' files in the directory 'dirname'. It will
 **   fail to delete any directories.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to delete any files matching
 **   the filter. Otherwise it returns the number of files that where
 **   deleted, even 0 when no files are deleted.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   07.04.2006 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>             /* strcpy(), strlen()                    */
#include <unistd.h>             /* rmdir(), unlink()                     */
#include <sys/types.h>
#include <dirent.h>             /* opendir(), readdir(), closedir()      */
#include <errno.h>


/*############################ remove_files() ###########################*/
int
remove_files(char *dirname, char *filter)
{
   int           addchar = NO,
                 count = 0,
                 ret = SUCCESS;
   char          *ptr;
   struct dirent *dirp;
   DIR           *dp;

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

   errno = 0;
   while ((dirp = readdir(dp)) != NULL)
   {
      if ((dirp->d_name[0] == '.') && ((dirp->d_name[1] == '\0') ||
          ((dirp->d_name[1] == '.') && (dirp->d_name[2] == '\0'))))
      {
         continue;
      }
      if (pmatch(filter, dirp->d_name, NULL) == 0)
      {
         (void)strcpy(ptr, dirp->d_name);
         if (unlink(dirname) == -1)
         {
            if (errno != ENOENT)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to delete `%s' : %s"),
                          dirname, strerror(errno));
               ret = INCORRECT;
            }
         }
         else
         {
            count++;
         }
      }
      errno = 0;
   }
   if (addchar == YES)
   {
      ptr[-1] = 0;
   }
   else
   {
      *ptr = '\0';
   }
   if (errno != 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to readdir() `%s' : %s"), dirname, strerror(errno));
      ret = INCORRECT;
   }
   if (closedir(dp) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to closedir() `%s' : %s"), dirname, strerror(errno));
      ret = INCORRECT;
   }
   if (ret != INCORRECT)
   {
      ret = count;
   }

   return(ret);
}
