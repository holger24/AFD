/*
 *  locate_xy_column.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   locate_xy_column - calculates x and y coordinates for a certain position
 **
 ** SYNOPSIS
 **   void locate_xy_column(int pos, int *x, int *y, int *column)
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
 **   22.12.2001 H.Kiehl Return column number as well.
 **
 */
DESCR__E_M3

#include "mafd_ctrl.h"

extern int *line_length,
           line_height,
           no_of_columns,
           no_of_rows;


/*########################## locate_xy_column() ##########################*/
void
locate_xy_column(int pos, int *x, int *y, int *column)
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

   *x = 0;
   if (column_no > 1)
   {
      register int i;

      for (i = 0; ((i < no_of_columns) && (i < (column_no - 1))); i++)
      {
         *x += line_length[i];
      }
   }
   if (column != NULL)
   {
      *column = column_no - 1;
      if ((no_of_columns > 0) && (*column >= no_of_columns))
      {
         *column = no_of_columns - 1;
      }
   }

   return;
}
