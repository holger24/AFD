/*
 *  window_size.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2017 Deutscher Wetterdienst (DWD),
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
 **   window_size - calculates the new window size
 **
 ** SYNOPSIS
 **   signed char window_size(int *window_width, int *window_height)
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
 **   26.01.1996 H.Kiehl Created
 **   22.12.2001 H.Kiehl Added variable column length.
 **   25.08.2013 H.Kiehl Added compact process status.
 **
 */
DESCR__E_M3

#include <string.h>                 /* strerror()                       */
#include <stdlib.h>                 /* malloc(), free()                 */
#include <errno.h>
#include "mafd_ctrl.h"

/* External global variables. */
extern Display      *display;
extern int          bar_thickness_3,
                    button_width,
                    *line_length,
                    max_line_length,
                    max_parallel_jobs_columns,
                    line_height,
                    no_of_columns,
                    no_of_hosts,
                    no_of_hosts_visible,
                    no_of_rows,
                    no_of_rows_set,
                    *vpl;
extern unsigned int glyph_width;
extern char         line_style;
extern struct line  *connect_data;


/*########################### window_size() ############################*/
signed char
window_size(int *window_width, int *window_height)
{
   int         i,
               max_no_of_lines,
               new_window_width,
               new_window_height,
               previous_no_of_rows;
   signed char window_size_changed;

   /* How many columns do we need? */
   no_of_columns = no_of_hosts_visible / no_of_rows_set;
   if ((no_of_hosts_visible % no_of_rows_set) != 0)
   {
      no_of_columns += 1;
   }

   /* Ensure that there is no division by zero. */
   if (no_of_columns == 0)
   {
      no_of_columns = 1;
   }

   /* How many lines per window? */
   previous_no_of_rows = no_of_rows;
   no_of_rows = no_of_hosts_visible / no_of_columns;
   if (no_of_hosts_visible != (no_of_columns * no_of_rows))
   {
      if ((no_of_hosts_visible % no_of_columns) != 0)
      {
         no_of_rows += 1;
      }
   }

   /* Determine the length of each column. */
   free(line_length);
   if ((line_length = malloc(no_of_columns * sizeof(int))) == NULL)
   {
      (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   new_window_width = 0;
   if ((line_style & SHOW_JOBS) || (line_style & SHOW_JOBS_COMPACT))
   {
      int group_name_in_row,
          j,
          max_no_parallel_jobs = 0,
          pos = 0;

      for (i = 0; i < no_of_columns; i++)
      {
         group_name_in_row = NO;
         for (j = 0; j < no_of_rows; j++)
         {
            if (connect_data[vpl[pos]].type == GROUP_IDENTIFIER)
            {
               group_name_in_row = YES;
            }
            if ((connect_data[vpl[pos]].plus_minus == PM_OPEN_STATE) &&
                (connect_data[vpl[pos]].type == NORMAL_IDENTIFIER))
            {
               if (max_no_parallel_jobs < connect_data[vpl[pos]].allowed_transfers)
               {
                  max_no_parallel_jobs = connect_data[vpl[pos]].allowed_transfers;
               }
            }
            pos++;
            if (pos >= no_of_hosts_visible)
            {
               break;
            }
         }
         if (line_style & SHOW_JOBS_COMPACT)
         {
            int parallel_jobs_columns_less;

            if (max_no_parallel_jobs % 3)
            {
               parallel_jobs_columns_less = max_parallel_jobs_columns -
                                            ((max_no_parallel_jobs / 3) + 1);
            }
            else
            {
               parallel_jobs_columns_less = max_parallel_jobs_columns -
                                            (max_no_parallel_jobs / 3);
            }
            line_length[i] = max_line_length -
                             (parallel_jobs_columns_less * bar_thickness_3);
         }
         else
         {
            line_length[i] = max_line_length -
                             (((MAX_NO_PARALLEL_JOBS - max_no_parallel_jobs) *
                              (button_width + BUTTON_SPACING)) - BUTTON_SPACING);
         }
         if ((group_name_in_row == YES) && (max_no_parallel_jobs == 0))
         {
            line_length[i] += glyph_width;
         }
         new_window_width += line_length[i];
         max_no_parallel_jobs = 0;
      }
   }
   else
   {
      for (i = 0; i < no_of_columns; i++)
      {
         line_length[i] = max_line_length;
         new_window_width += max_line_length;
      }
   }

   /* Check if in last column rows moved up. */
   if (((max_no_of_lines = (no_of_columns * no_of_rows)) > no_of_hosts_visible) &&
       (previous_no_of_rows != no_of_rows) && (previous_no_of_rows != 0))
   {
      for (i = max_no_of_lines; i > no_of_hosts_visible; i--)
      {
         draw_blank_line(i - 1);
      }
   }

   calc_but_coord(new_window_width);

   /* Calculate window width and height. */
   new_window_height = line_height * no_of_rows;

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
