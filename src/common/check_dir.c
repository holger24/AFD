/*
 *  check_dir.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2025 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_dir - Checks if a directory exists, if not it tries
 **               to create this directory
 **
 ** SYNOPSIS
 **   int check_dir(char *directory, int access_mode)
 **
 ** DESCRIPTION
 **   This function checks if the 'directory' exists and
 **   if not it will try to create it. If it does exist, it
 **   is checked if it has the correct access permissions.
 **
 ** RETURN VALUES
 **   SUCCESS if this directoy exists or has been created.
 **   Otherwise INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.12.1996 H.Kiehl Created
 **   19.03.2025 H.Kiehl Print the directories and mode when creating
 **                      them.
 **
 */
DESCR__E_M3

#include <string.h>            /* strerror()                             */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <unistd.h>
#include <errno.h>


/*############################ check_dir() ##############################*/
int
check_dir(char *directory, int access_mode)
{
   struct stat stat_buf;

   if (stat(directory, &stat_buf) == -1)
   {
      if (mkdir(directory, DIR_MODE) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to create directory `%s' : %s"),
                    directory, strerror(errno));
         return(INCORRECT);
      }
      else
      {
         char str_mode[11];

         mode_t2str(DIR_MODE, str_mode);
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("Created directory `%s' with mode `%s'."),
                    directory, str_mode);
      }
   }
   else if (!S_ISDIR(stat_buf.st_mode))
        {
           system_log(ERROR_SIGN, __FILE__, __LINE__,
                      _("There already exists a file `%s', thus unable to create the directory."),
                      directory);
           return(INCORRECT);
        }
        else /* Lets check if the correct permissions are set. */
        {
           if (eaccess(directory, access_mode) == -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         _("Incorrect permission for directory `%s'"), directory);
              return(INCORRECT);
           }
        }

   return(SUCCESS);
}
