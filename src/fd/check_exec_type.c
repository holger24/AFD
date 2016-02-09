/*
 *  check_exec_type.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2011 Deutscher Wetterdienst (DWD),
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
 **   check_exec_type - checks if command is to be executed once only
 **
 ** SYNOPSIS
 **   int check_exec_type(char *exec_cmd)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   Returns 1 if the command is to be executed once only. Otherwise
 **   0 is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   28.11.2011 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "fddefs.h"


/*########################## check_exec_type() ##########################*/
int
check_exec_type(char *exec_cmd)
{
   while (*exec_cmd != '\0')
   {
      if ((*exec_cmd == '%') && (*(exec_cmd + 1) == 's'))
      {
         return(0);
      }
      exec_cmd++;
   }

   return(1);
}
