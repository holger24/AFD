/*
 *  dir_expose_handler.c - Part of AFD, an automatic file distribution
 *                         program.
 *  Copyright (c) 2000 - 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   dir_expose_handler - handles any expose event for label and
 **                        line window
 **
 ** SYNOPSIS
 **   void dir_expose_handler_label(Widget                      w,
 **                                 XtPointer                   client_data,
 **                                 XmDrawingAreaCallbackStruct *call_data)
 **   void dir_expose_handler(Widget                      w,
 **                           XtPointer                   client_data,
 **                           XmDrawingAreaCallbackStruct *call_data)
 **
 ** DESCRIPTION
 **   When an expose event occurs, only those parts of the window
 **   will be redrawn that where covered. For the label window
 **   the whole line will always be redrawn, also if only part of
 **   it was covered. In the line window we will only redraw those
 **   those lines that were covered.
 **
 ** RETURN VALUES
 **   None
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   31.08.2000 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>              /* printf()                              */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <Xm/Xm.h>
#include "dir_ctrl.h"
#include "permission.h"

extern Display                 *display;
extern XtAppContext            app;
extern Pixmap                  label_pixmap,
                               line_pixmap;
extern Window                  label_window,
                               line_window;
extern Widget                  appshell,
                               mw[];
extern GC                      color_letter_gc,
                               default_bg_gc,
                               label_bg_gc;
extern int                     magic_value,
                               no_input,
                               no_of_dirs,
                               no_of_jobs_selected,
                               window_width,
                               window_height,
                               line_length,
                               line_height,
                               no_of_columns,
                               no_of_rows,
                               redraw_time_line;
extern unsigned int            glyph_height;
extern struct dir_line         *connect_data;
extern struct dir_control_perm dcp;


/*###################### dir_expose_handler_label() #####################*/
void
dir_expose_handler_label(Widget                      w,
                         XtPointer                   client_data,
                         XmDrawingAreaCallbackStruct *call_data)
{
   static int ft_exposure_label = 0; /* First time exposure label. */

   if (ft_exposure_label == 0)
   {
      draw_dir_label_line();
      ft_exposure_label = 1;
   }
   else
   {
      XEvent *p_event = call_data->event;

      XCopyArea(display, label_pixmap, label_window, label_bg_gc,
                p_event->xexpose.x, p_event->xexpose.y,
                p_event->xexpose.width, p_event->xexpose.height,
                p_event->xexpose.x, p_event->xexpose.y);
   }
   XFlush(display);

   return;
}


/*###################### dir_expose_handler_line() ######################*/
void
dir_expose_handler_line(Widget                      w,
                        XtPointer                   client_data,
                        XmDrawingAreaCallbackStruct *call_data)
{
   static int ft_exposure_line = 0; /* first time exposure line. */

   /*
    * To ensure that widgets are realized before calling XtAppAddTimeOut()
    * we wait for the widget to get its first expose event. This should
    * take care of the nasty BadDrawable error on slow connections.
    */
   if (ft_exposure_line == 0)
   {
      int       bs_attribute,
                i;
      Dimension height;
      Screen    *c_screen = ScreenOfDisplay(display, DefaultScreen(display));

      XFillRectangle(display, line_pixmap, default_bg_gc, 0, 0,
                     window_width, (line_height * no_of_rows));
      for (i = 0; i < no_of_dirs; i++)
      {
         draw_dir_line_status(i, 1);
      }

      (void)XtAppAddTimeOut(app, redraw_time_line,
                            (XtTimerCallbackProc)check_dir_status, w);
      ft_exposure_line = 1;

      if ((bs_attribute = DoesBackingStore(c_screen)) != NotUseful)
      {
         XSetWindowAttributes attr;

         attr.backing_store = bs_attribute;
         attr.save_under = DoesSaveUnders(c_screen);
         XChangeWindowAttributes(display, line_window,
                                 CWBackingStore | CWSaveUnder, &attr);
         XChangeWindowAttributes(display, label_window,
                                 CWBackingStore, &attr);
         if (no_input == False)
         {
            XChangeWindowAttributes(display, XtWindow(mw[DIR_W]),
                                    CWBackingStore, &attr);

            if ((dcp.show_slog != NO_PERMISSION) ||
                (dcp.show_rlog != NO_PERMISSION) ||
                (dcp.show_tlog != NO_PERMISSION) ||
                (dcp.show_ilog != NO_PERMISSION) ||
                (dcp.show_olog != NO_PERMISSION) ||
                (dcp.show_elog != NO_PERMISSION) ||
                (dcp.info != NO_PERMISSION))
            {
               XChangeWindowAttributes(display, XtWindow(mw[LOG_W]),
                                       CWBackingStore, &attr);
            }

            XChangeWindowAttributes(display, XtWindow(mw[CONFIG_W]),
                                    CWBackingStore, &attr);
#ifdef _WITH_HELP_PULLDOWN
            XChangeWindowAttributes(display, XtWindow(mw[HELP_W]),
                                    CWBackingStore, &attr);
#endif
         } /* if (no_input == False) */
      }

      /*
       * Calculate the magic unkown height factor we need to add to the
       * height of the widget when it is being resized.
       */
      XtVaGetValues(appshell, XmNheight, &height, NULL);
      magic_value = height - (window_height + line_height + glyph_height);
   }
   else
   {
      XEvent *p_event = call_data->event;

      XCopyArea(display, line_pixmap, line_window, color_letter_gc,
                p_event->xexpose.x, p_event->xexpose.y,
                p_event->xexpose.width, p_event->xexpose.height,
                p_event->xexpose.x, p_event->xexpose.y);
   }
   XFlush(display);

   return;
}
