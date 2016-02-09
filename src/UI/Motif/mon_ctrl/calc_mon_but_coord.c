/*
 *  calc_mon_but_coord.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   08.06.2005 H.Kiehl Taken from afd_ctrl and adapted for mon_ctrl.
 **
 */
DESCR__E_M3

#include <math.h>             /* sin(), cos()                            */
#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include "mon_ctrl.h"

extern Display           *display;
extern int               x_offset_stat_leds,
                         x_offset_sys_log,
                         x_center_sys_log,
                         x_offset_mon_log,
                         x_center_mon_log,
                         y_center_log,
                         log_angle;
extern unsigned int      glyph_height,
                         glyph_width;
extern struct coord      button_coord[2][LOG_FIFO_SIZE];


/*######################### calc_mon_but_coord() #########################*/
void
calc_mon_but_coord(int new_window_width)
{
   int i;

   /*
    * Now lets calculate all x and y coordinates for the
    * the line in circle showing whats going on in the
    * logs. Here we go ......
    */
   x_offset_sys_log = (new_window_width / 2) - DEFAULT_FRAME_SPACE -
                      glyph_height;
   x_center_sys_log = x_offset_sys_log + (glyph_height / 2);
   x_offset_mon_log = (new_window_width / 2) + DEFAULT_FRAME_SPACE +
                      glyph_height;
   x_center_mon_log = x_offset_mon_log + (glyph_height / 2);
   y_center_log     = SPACE_ABOVE_LINE + (glyph_height / 2);
   for (i = 0; i < LOG_FIFO_SIZE; i++)
   {
      button_coord[0][i].x = x_center_sys_log + (int)((glyph_height / 2) *
                             cos((double)((log_angle * i * PI) / 180)));
      button_coord[1][i].x = x_center_mon_log + (int)((glyph_height / 2) *
                             cos((double)((log_angle * i * PI) / 180)));
      button_coord[0][i].y = button_coord[1][i].y =
                             y_center_log - (int)((glyph_height / 2) *
                             sin((double)((log_angle * i * PI) / 180)));
   }

   /* Position of status LED's. */
   x_offset_stat_leds = DEFAULT_FRAME_SPACE;

   return;
}
