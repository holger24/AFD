/*
 *  redraw_all.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   redraw_all - redraws all drawing areas
 **
 ** SYNOPSIS
 **   void redraw_all(void)
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
 **   16.02.2008 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include "mafd_ctrl.h"

/* External global variables. */
extern Window       button_window,
                    label_window,
                    line_window,
                    short_line_window;
extern Pixmap       button_pixmap,
                    label_pixmap,
                    line_pixmap,
                    short_line_pixmap;
extern Display      *display;
extern GC           default_bg_gc;
extern int          depth,
                    line_height,
                    no_of_hosts,
                    no_of_long_lines,
                    no_of_rows,
                    no_of_short_lines,
                    no_of_short_rows,
                    window_width;


/*############################# redraw_all() ############################*/
void
redraw_all(void)
{
   unsigned int height;
   int          i;

   /* Clear everything. */
   if (no_of_long_lines > 0)
   {
      XClearWindow(display, line_window);
   }
   if (no_of_short_lines > 0)
   {
      XClearWindow(display, short_line_window);
   }
   XFreePixmap(display, label_pixmap);
   label_pixmap = XCreatePixmap(display, label_window, window_width,
                                line_height, depth);
   XFreePixmap(display, line_pixmap);
   if (no_of_long_lines > 0)
   {
      height = line_height * no_of_rows;
   }
   else
   {
      height = 1;
   }
   line_pixmap = XCreatePixmap(display, line_window, window_width,
                               height, depth);
   if (height != 1)
   {
      XFillRectangle(display, line_pixmap, default_bg_gc, 0, 0,
                     window_width, height);
   }
   XFreePixmap(display, button_pixmap);
   button_pixmap = XCreatePixmap(display, button_window, window_width,
                                 line_height, depth);
   XFreePixmap(display, short_line_pixmap);
   if (no_of_short_rows > 0)
   {
      height = line_height * no_of_short_rows;
   }
   else
   {
      height = 1;
   }
   short_line_pixmap = XCreatePixmap(display, short_line_window,
                                     window_width, height, depth);
   if (height != 1)
   {
      XFillRectangle(display, short_line_pixmap, default_bg_gc, 0, 0,
                     window_width, height);
   }

   /* Redraw everything. */
   draw_label_line();
   for (i = 0; i < no_of_hosts; i++)
   {
      draw_line_status(i, 1);
   }
   draw_button_line();

   return;
}
