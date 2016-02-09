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
 **   24.02.2008 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include "dir_ctrl.h"

/* External global variables. */
extern Window       label_window,
                    line_window;
extern Pixmap       label_pixmap,
                    line_pixmap;
extern Display      *display;
extern GC           default_bg_gc;
extern int          depth,
                    line_height,
                    no_of_dirs,
                    no_of_rows,
                    window_width;


/*############################# redraw_all() ############################*/
void
redraw_all(void)
{
   int i;

   /* Clear everything. */
   XClearWindow(display, line_window);
   XFreePixmap(display, label_pixmap);
   label_pixmap = XCreatePixmap(display, label_window, window_width,
                                line_height, depth);
   XFreePixmap(display, line_pixmap);
   line_pixmap = XCreatePixmap(display, line_window, window_width,
                               (line_height * no_of_rows), depth);
   XFillRectangle(display, line_pixmap, default_bg_gc, 0, 0,
                  window_width, (line_height * no_of_rows));

   /* Redraw everything. */
   draw_dir_label_line();
   for (i = 0; i < no_of_dirs; i++)
   {
      draw_dir_line_status(i, 1);
   }

   return;
}
