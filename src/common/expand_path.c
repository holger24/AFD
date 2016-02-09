/*
 *  expand_path.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 - 2011 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   expand_path - expands a relative to absolute path
 **
 ** SYNOPSIS
 **   int expand_path(char *user, char *path)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   INCORRECT when we fail to lookup the user name or id. Otherwise
 **   SUCCESS will be returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.04.2008 H.Kiehl Created
 **   25.10.2008 H.Kiehl Return a value to indicate success or failure.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                  /* strcpy(), strerror()             */
#include <sys/types.h>
#include <pwd.h>                     /* getpwuid(), getpwnam()           */
#include <unistd.h>                  /* getuid()                         */
#include <errno.h>


/*############################ expand_path() ############################*/
int
expand_path(char *user, char *path)
{
   struct passwd *pwd;

   errno = 0;
   if (user[0] == '\0')
   {
      if ((pwd = getpwuid(getuid())) == NULL)
      {
         if (errno == 0)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_UID_T == 4
                       _("Cannot find working directory for userid %d in /etc/passwd"),
#else
                       _("Cannot find working directory for userid %lld in /etc/passwd"),
#endif
                       (pri_uid_t)getuid());
            return(INCORRECT);
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_UID_T == 4
                       _("Cannot find working directory for userid %d in /etc/passwd : %s"),
#else
                       _("Cannot find working directory for userid %lld in /etc/passwd : %s"),
#endif
                       (pri_uid_t)getuid(), strerror(errno));
            return(INCORRECT);
         }
      }
   }
   else
   {
      if ((pwd = getpwnam(user)) == NULL)
      {
         if (errno == 0)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Cannot find users `%s' working directory in /etc/passwd"),
                       user);
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Cannot find users `%s' working directory in /etc/passwd : %s"),
                       user, strerror(errno));
         }
         return(INCORRECT);
      }
   }

   if (*path != '\0')
   {
      char *ptr,
           tmp_path[MAX_PATH_LENGTH];

      (void)strcpy(tmp_path, path);
      (void)strcpy(path, pwd->pw_dir);

      ptr = path + (strlen(path) - 1);
      if (ptr > path)
      {
         if (*ptr != '/')
         {
            *(ptr + 1) = '/';
            ptr += 2;
         }
         else
         {
            ptr++;
         }
      }
      else
      {
         ptr = path;
         *ptr = '/';
         ptr++;
      }
      (void)strcpy(ptr, tmp_path);
   }
   else
   {
      path[0] = '/';
      (void)strcpy(&path[1], pwd->pw_dir);
   }

   return(SUCCESS);
}
