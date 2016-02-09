/*
 *  locate_xy.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   locate_xy - calculates x and y coordinates for a certain position
 **
 ** SYNOPSIS
 **   void locate_xy(int pos, int *x, int *y)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   Fills x and y with data.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   03.10.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "ui_common_defs.h"

extern int line_length,
           line_height,
           no_of_rows;


/*############################# locate_xy() #############################*/
void
locate_xy(int pos, int *x, int *y)
{
   int column_no;

   /* First check that we do not divide by zero. */
   if (no_of_rows <= 1)
   {
      column_no = (pos + 1);
   }
   else
   {
      column_no = (pos + 1) / no_of_rows;
   }

   if (((pos + 1) % no_of_rows) != 0)
   {
      column_no += 1;
      *y = line_height * (pos % no_of_rows);
   }
   else
   {
      *y = line_height * (no_of_rows - 1);
   }

   if (column_no > 1)
   {
      *x = (column_no - 1) * line_length;
   }
   else
   {
      *x = 0;
   }

   return;
}
