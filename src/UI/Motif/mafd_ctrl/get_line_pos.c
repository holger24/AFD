/*
 *  get_line_pos.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_line_pos - searches for the relevant long or short position
 **                  in the connect_data structure
 **
 ** SYNOPSIS
 **   int get_long_pos(int select_no, int verbose)
 **   int get_short_pos(int select_no, int verbose)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   Returns postion in connect_data structure.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   24.11.2002 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "mafd_ctrl.h"

extern int         no_of_hosts;
extern struct line *connect_data;


/*########################### get_long_pos() ############################*/
int
get_long_pos(int select_no, int verbose)
{
   register int i;

   for (i = 0; i < no_of_hosts; i++)
   {
      if (connect_data[i].long_pos == select_no)
      {
         return(i);
      }
   }
   if (verbose == YES)
   {
      (void)xrec(INFO_DIALOG,
                 "Hmmm, unable to get long position for %d.", select_no);
   }
   return(-1);
}


/*########################### get_short_pos() ###########################*/
int
get_short_pos(int select_no, int verbose)
{
   register int i;

   for (i = 0; i < no_of_hosts; i++)
   {
      if (connect_data[i].short_pos == select_no)
      {
         return(i);
      }
   }
   if (verbose == YES)
   {
      (void)xrec(INFO_DIALOG,
                 "Hmmm, unable to get short position for %d.", select_no);
   }
   return(-1);
}
