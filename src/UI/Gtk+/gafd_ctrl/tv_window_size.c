/*
 *  tv_window_size.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998, 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   tv_window_size - calculates the new window size
 **
 ** SYNOPSIS
 **   signed char tv_window_size(int *window_width, int *window_height)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   YES - Window size must be changed
 **   NO  - Window size must not be changed
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   07.01.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "gafd_ctrl.h"

extern Display *display;
extern int     tv_line_length,
               line_height,
               tv_no_of_columns,
               tv_no_of_rows,
               no_of_rows_set,
               no_of_jobs_selected;


/*########################## tv_window_size() ###########################*/
signed char
tv_window_size(Dimension *window_width, Dimension *window_height)
{
   Dimension   new_window_width,
               new_window_height;
   int         previous_no_of_rows,
               max_no_of_jobs_selected;
   signed char window_size_changed;

   /* How many columns do we need? */
   tv_no_of_columns = no_of_jobs_selected / no_of_rows_set;
   if ((no_of_jobs_selected % no_of_rows_set) != 0)
   {
      tv_no_of_columns += 1;
   }

   /* Ensure that there is no division by zero. */
   if (tv_no_of_columns == 0)
   {
      tv_no_of_columns = 1;
   }

   /* How many lines per window? */
   previous_no_of_rows = tv_no_of_rows;
   tv_no_of_rows = no_of_jobs_selected / tv_no_of_columns;
   if (no_of_jobs_selected != (tv_no_of_columns * tv_no_of_rows))
   {
      if ((no_of_jobs_selected % tv_no_of_columns) != 0)
      {
         tv_no_of_rows += 1;
      }
   }

   /* Check if in last column rows moved up. */
   if (((max_no_of_jobs_selected = (tv_no_of_columns * tv_no_of_rows)) > no_of_jobs_selected) &&
       (previous_no_of_rows != tv_no_of_rows) && (previous_no_of_rows != 0))
   {
      int i;

      for (i = max_no_of_jobs_selected; i > no_of_jobs_selected; i--)
      {
         draw_tv_blank_line(i - 1);
      }
   }

   /* Calculate window width and height. */
   new_window_width  = tv_line_length * tv_no_of_columns;
   new_window_height = line_height  * tv_no_of_rows;

   /* Window resize necessary? */
   if ((new_window_width  != *window_width) ||
       (new_window_height != *window_height))
   {
      window_size_changed = YES;
   }
   else
   {
      window_size_changed = NO;
   }

   *window_width  = new_window_width;
   *window_height = new_window_height;

   return(window_size_changed);
}
