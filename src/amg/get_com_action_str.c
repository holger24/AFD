/*
 *  get_com_action_str.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2017 - 2023 Deutscher Wetterdienst (DWD),
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
 **   get_com_action_str - returns the com action string for the given number
 **
 ** SYNOPSIS
 **   char *get_com_action_str(int com_action_code)
 **
 ** DESCRIPTION
 **   The function get_com_action_str() looks up the com action string
 **   for the given com_action_code and returns this to the caller.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.09.2017 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "amgdefs.h"


/*###################### get_com_action_str() ###########################*/
char *
get_com_action_str(int com_action_code)
{
   switch (com_action_code)
   {
      case STOP       : return("STOP");                /*  1 */
      case SHUTDOWN   : return("SHUTDOWN");            /* 19 */
      case DATA_READY : return("DATA_READY");          /* 33 */
   }
   return(_("<Unknown com action>"));
}
