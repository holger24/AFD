/*
 *  eval_timeout.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 - 2025 Deutscher Wetterdienst (DWD),
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
 **   eval_timeout - searches message directory for any changes
 **
 ** SYNOPSIS
 **   int eval_timeout(int error)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.09.2005 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "fddefs.h"

/* External global variables. */
extern int timeout_flag;


/*############################ eval_timeout() ###########################*/
int
eval_timeout(int error)
{
   if (timeout_flag == ON)
   {
      return(TIMEOUT_ERROR);
   }
   else if (timeout_flag == CON_RESET)
        {
           return(CONNECTION_RESET_ERROR);
        }
   else if (timeout_flag == PIPE_CLOSED)
        {
           return(PIPE_CLOSED_ERROR);
        }
   else if (timeout_flag == CON_REFUSED)
        {
           return(CONNECTION_REFUSED_ERROR);
        }

   return(error);
}
