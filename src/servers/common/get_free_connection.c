/*
 *  get_free_connection.c - Part of AFD, an automatic file distribution program.
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
 **   get_free_connection - searches for a free pid that can be used
 **                         to connect
 **
 ** SYNOPSIS
 **   int get_free_connection(const int max_connections)
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
 **   23.07.2015 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "afdd_common_defs.h"

/* External global variables. */
extern pid_t *pid;

/*######################### get_free_connection() #######################*/
int
get_free_connection(const int max_connections)
{
   int i;

   for (i = 0; i < max_connections; i++)
   {
      if (pid[i] == 0)
      {
         return(i);
      }
   }

   return(INCORRECT);
}
