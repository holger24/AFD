/*
 *  draw_stat.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   draw_stat - draws graph with statistic information
 **
 ** SYNOPSIS
 **   void draw_stat(void)
 **   void draw_graph(void)
 **   void draw_x_axis(void)
 **   void draw_y_axis(void)
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
 **   31.12.2001 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <stdlib.h>                   /* abs()                           */
#include <string.h>                   /* strlen()                        */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include "xshow_stat.h"

extern Display        *display;
extern Window         stat_window;
extern GC             letter_gc,
                      normal_letter_gc,
                      locked_letter_gc,
                      color_letter_gc,
                      default_bg_gc,
                      normal_bg_gc,
                      locked_bg_gc,
                      label_bg_gc,
                      button_bg_gc,
                      tr_bar_gc,
                      color_gc,
                      black_line_gc,
                      white_line_gc,
                      led_gc;
extern Colormap       default_cmap;
extern char           line_style,
                      **x_data_point;
extern unsigned long  color_pool[];
extern int            data_height,
                      data_length,
                      first_data_pos,
                      no_of_chars,
                      no_of_x_data_points,
                      x_data_spacing,
                      x_offset_left_xaxis,
                      y_data_spacing,
                      y_offset_top_yaxis,
                      y_offset_xaxis,
                      window_width;
extern unsigned int   glyph_height,
                      glyph_width;
extern struct afdstat *stat_db;

/* Local function prototypes. */
static void draw_x_axis(void),
            draw_y_axis(void);


/*############################# draw_stat() #############################*/
void
draw_stat(void)
{
   draw_x_axis();
   draw_x_values();
   draw_y_axis();
   draw_graph();

   return;
}


/*############################# draw_graph() ############################*/
void
draw_graph(void)
{
   return;
}


/*########################### draw_x_values() ###########################*/
void
draw_x_values(void)
{
   int i,
       pos = first_data_pos,
       x;

   if (no_of_chars == 3)
   {
      x = x_offset_left_xaxis - glyph_width;
   }
   else
   {
      x = x_offset_left_xaxis - (glyph_width + (glyph_width / 2));
   }
   for (i = 0; i < no_of_x_data_points; i++)
   {
      if (pos >= no_of_x_data_points)
      {
         pos = 0;
      }
      XDrawString(display, stat_window, letter_gc,
                  x, y_offset_top_yaxis + data_height + y_offset_xaxis,
                  x_data_point[pos], no_of_chars - 1);
      pos++;
      x += x_data_spacing;
   }

   return;
}


/*############################ draw_x_axis() ############################*/
static void
draw_x_axis(void)
{
   int i,
       offset = x_data_spacing;

   XDrawLine(display, stat_window, black_line_gc,
             x_offset_left_xaxis, y_offset_top_yaxis + data_height,
             x_offset_left_xaxis + data_length, y_offset_top_yaxis + data_height);

   /* Draw marks on the line. */
   for (i = 0; i < (no_of_x_data_points - 1); i++)
   {
      XDrawLine(display, stat_window, black_line_gc,
                x_offset_left_xaxis + offset, y_offset_top_yaxis + data_height - 3,
                x_offset_left_xaxis + offset, y_offset_top_yaxis + data_height + 3);
      offset += x_data_spacing;
   }

   return;
}


/*############################ draw_y_axis() ############################*/
static void
draw_y_axis(void)
{
   XDrawLine(display, stat_window, black_line_gc,
             x_offset_left_xaxis, y_offset_top_yaxis,
             x_offset_left_xaxis, y_offset_top_yaxis + data_height);

   return;
}
