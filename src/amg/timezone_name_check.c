/*
 *  timezone_name_check.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2023 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   timezone_name_check - checks if a file exists for given zone
 **
 ** SYNOPSIS
 **   int timezone_name_check(const char *timezone_name)
 **
 ** DESCRIPTION
 **   The function timezone_name_check() looks in TZDIR directory
 **   of the system and checks if it finds such a name.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   21.07.2023 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>     /* strerror(), strlen()                          */
#include <stdlib.h>     /* malloc(), free()                              */
#include <unistd.h>
#include <errno.h>

#include "amgdefs.h"


/*###################### timezone_name_check() ##########################*/
int
timezone_name_check(const char *timezone_name)
{
#ifdef TZDIR
   int    ret;
   size_t length = (sizeof(TZDIR) - 1) + 1 + strlen(timezone_name) + 1;
   char   *fullname;

   if ((fullname = malloc(length)) == NULL)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      ret = INCORRECT;
   }
   else
   {
      (void)snprintf(fullname, length, "%s/%s", TZDIR, timezone_name);
      if (eaccess(fullname, R_OK) != 0)
      {
         ret = INCORRECT;
      }
      else
      {
         ret = SUCCESS;
      }
      free(fullname);
   }
   return(ret);
#else
   return(SUCCESS);
#endif
}
