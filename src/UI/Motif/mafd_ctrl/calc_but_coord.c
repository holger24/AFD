/*
 *  calc_but_coord.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2003 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   calc_but_coord -
 **
 ** SYNOPSIS
 **   void calc_but_coord(int new_window_width)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   18.02.1996 H.Kiehl Created
 **   13.03.2003 H.Kiehl Added history log in button bar.
 **
 */
DESCR__E_M3

#include <math.h>             /* sin(), cos()                            */
#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include "mafd_ctrl.h"

extern Display           *display;
extern int               bar_thickness_3,
                         no_of_his_log,
                         x_offset_stat_leds,
                         x_offset_receive_log,
                         x_center_receive_log,
                         x_offset_sys_log,
                         x_center_sys_log,
                         x_offset_trans_log,
                         x_center_trans_log,
                         x_offset_log_history_left,
                         x_offset_log_history_right,
                         y_center_log,
                         log_angle;
extern unsigned int      glyph_height,
                         glyph_width;
extern struct coord      coord[3][LOG_FIFO_SIZE];
extern struct afd_status prev_afd_status;


/*########################### calc_but_coord() ###########################*/
void
calc_but_coord(int new_window_width)
{
   int avail_history_length,
       his_log_length_half,
       i,
       left,
       right;

   /*
    * Now lets calculate all x and y coordinates for the
    * the line in circle showing whats going on in the
    * logs. Here we go ......
    */
   x_offset_sys_log     = (new_window_width / 2);
   x_center_sys_log     = x_offset_sys_log + (glyph_height / 2);
   x_offset_receive_log = x_offset_sys_log - DEFAULT_FRAME_SPACE - glyph_height;
   x_center_receive_log = x_offset_receive_log + (glyph_height / 2);
   x_offset_trans_log   = x_offset_sys_log + DEFAULT_FRAME_SPACE + glyph_height;
   x_center_trans_log   = x_offset_trans_log + (glyph_height / 2);
   y_center_log         = SPACE_ABOVE_LINE + (glyph_height / 2);
   for (i = 0; i < LOG_FIFO_SIZE; i++)
   {
      coord[0][i].x = x_center_receive_log + (int)((glyph_height / 2) *
                      cos((double)((log_angle * i * PI) / 180)));
      coord[1][i].x = x_center_sys_log + (int)((glyph_height / 2) *
                      cos((double)((log_angle * i * PI) / 180)));
      coord[2][i].x = x_center_trans_log + (int)((glyph_height / 2) *
                      cos((double)((log_angle * i * PI) / 180)));
      coord[0][i].y = coord[1][i].y = coord[2][i].y =
                      y_center_log - (int)((glyph_height / 2) *
                      sin((double)((log_angle * i * PI) / 180)));
   }

   /* Position of status LED's. */
   x_offset_stat_leds = DEFAULT_FRAME_SPACE;

   /* Calculate position for log history. */
   if (prev_afd_status.afdd != NEITHER)
   {
      left = DEFAULT_FRAME_SPACE + (4 * glyph_width) + (3 * PROC_LED_SPACING);
   }
   else
   {
      left = DEFAULT_FRAME_SPACE + (3 * glyph_width) + (2 * PROC_LED_SPACING);
   }
   right = (QUEUE_COUNTER_CHARS * glyph_width) + DEFAULT_FRAME_SPACE;
   if (left > right)
   {
      avail_history_length = new_window_width - ((3 * glyph_height) +
                             (2 * DEFAULT_FRAME_SPACE)) - (2 * left) -
                             ((2 * DEFAULT_FRAME_SPACE) +
			     (2 * DEFAULT_FRAME_SPACE));
   }
   else
   {
      avail_history_length = new_window_width - ((3 * glyph_height) +
                             (2 * DEFAULT_FRAME_SPACE)) - (2 * right) -
                             ((2 * DEFAULT_FRAME_SPACE) +
			     (2 * DEFAULT_FRAME_SPACE));
   }
   no_of_his_log = avail_history_length / bar_thickness_3;
   if (no_of_his_log > MAX_LOG_HISTORY)
   {
      no_of_his_log = MAX_LOG_HISTORY;
   }
   no_of_his_log = no_of_his_log / 2;
   his_log_length_half = no_of_his_log * bar_thickness_3;
   x_offset_log_history_left = x_offset_sys_log - (glyph_height +
                               DEFAULT_FRAME_SPACE) - DEFAULT_FRAME_SPACE -
	                       his_log_length_half;
   x_offset_log_history_right = x_offset_sys_log + (glyph_height +
                                DEFAULT_FRAME_SPACE + glyph_height) +
	                        DEFAULT_FRAME_SPACE;

   return;
}
