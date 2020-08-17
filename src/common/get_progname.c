/*
 *  get_progname.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2020 Deutscher Wetterdienst (DWD),
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
 **   get_progname - gets the proccess name from a name with path
 **
 ** SYNOPSIS
 **   char *get_progname(char *full_name)
 **
 ** DESCRIPTION
 **   This function determines the process name from a name with path.
 **
 ** RETURN VALUES
 **   Returns the pocess name without any path information.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.07.2020 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>            /* strlen()                               */


/*############################# get_progname() ##########################*/
char *
get_progname(char *full_name)
{
   char *ptr;

   ptr = full_name + strlen(full_name) - 1;

   while ((ptr != full_name) && (*ptr != '/'))
   {
      ptr--;
   }
   if (*ptr == '/')
   {
      ptr++;
   }

   return(ptr);
}
