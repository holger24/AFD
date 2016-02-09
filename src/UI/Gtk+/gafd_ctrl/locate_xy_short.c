/*
 *  locate_xy_short.c - Part of AFD, an automatic file distribution program.
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
 **   locate_xy_short - calculates x and y coordinates for a certain position
 **
 ** SYNOPSIS
 **   void locate_xy_short(int pos, int *x, int *y)
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
 **   08.11.2002 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "gafd_ctrl.h"

extern int line_height,
           no_of_short_columns,
           short_line_length;


/*########################## locate_xy_short() ##########################*/
void
locate_xy_short(int pos, int *x, int *y)
{
   int column_no,
       row_no;

   row_no = pos / no_of_short_columns;
   column_no = pos % no_of_short_columns;
   *x = column_no * short_line_length;
   *y = row_no * line_height;

   return;
}
