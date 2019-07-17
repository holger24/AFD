/*
 *  mon_expose_handler.c - Part of AFD, an automatic file distribution
 *                         program.
 *  Copyright (c) 1998 - 2019 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   mon_expose_handler - handles any expose event for label and
 **                        line window
 **
 ** SYNOPSIS
 **   void mon_expose_handler_label(Widget                      w,
 **                                 XtPointer                   client_data,
 **                                 XmDrawingAreaCallbackStruct *call_data)
 **   void mon_expose_handler(Widget                      w,
 **                           XtPointer                   client_data,
 **                           XmDrawingAreaCallbackStruct *call_data)
 **   void mon_expose_handler_button(Widget w, XtPointer client_data,
 **                                  XmDrawingAreaCallbackStruct *call_data)
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
 **   04.10.1998 H.Kiehl Created
 **   09.06.2005 H.Kiehl Added button line expose handler at bottom.
 **   17.07.2019 H.Kiehl Option to disable backing store and save
 **                      under.
 **
 */
DESCR__E_M3

#include <stdio.h>              /* printf()                              */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <Xm/Xm.h>
#include "mon_ctrl.h"
#include "permission.h"

extern Display                 *display;
extern XtAppContext            app;
extern Pixmap                  button_pixmap,
                               label_pixmap,
                               line_pixmap;
extern Window                  button_window,
                               label_window,
                               line_window;
extern Widget                  appshell,
                               mw[];
extern GC                      color_letter_gc,
                               default_bg_gc,
                               label_bg_gc;
extern int                     magic_value,
                               no_backing_store,
                               no_input,
                               no_of_afds,
                               line_length,
                               line_height,
                               no_of_columns,
                               no_of_rows,
                               window_height,
                               window_width;
extern unsigned int            glyph_height;
extern unsigned long           redraw_time_line,
                               redraw_time_status;
extern struct mon_line         *connect_data;
extern struct mon_control_perm mcp;


/*###################### mon_expose_handler_label() #####################*/
void
mon_expose_handler_label(Widget                      w,
                         XtPointer                   client_data,
                         XmDrawingAreaCallbackStruct *call_data)
{
   static int ft_exposure_label = 0; /* First time exposure label. */

   if (ft_exposure_label == 0)
   {
      draw_mon_label_line();
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


/*###################### mon_expose_handler_line() ######################*/
void
mon_expose_handler_line(Widget                      w,
                        XtPointer                   client_data,
                        XmDrawingAreaCallbackStruct *call_data)
{
   static int ft_exposure_line = 0; /* First time exposure line. */

   /*
    * To ensure that widgets are realized before calling XtAppAddTimeOut()
    * we wait for the widget to get its first expose event. This should
    * take care of the nasty BadDrawable error on slow connections.
    */
   if (ft_exposure_line == 0)
   {
      int       i,
                j = 0,
                x,
                y;
      Dimension height;

      XFillRectangle(display, line_pixmap, default_bg_gc, 0, 0,
                     window_width, (line_height * no_of_rows));
      for (i = 0; i < no_of_afds; i++)
      {
         if ((connect_data[i].plus_minus == PM_OPEN_STATE) ||
             (connect_data[i].rcmd == '\0'))
         {
            locate_xy(j, &x, &y);
            draw_mon_line_status(i, 1, x, y);
            j++;
         }
      }

      (void)XtAppAddTimeOut(app, redraw_time_line,
                            (XtTimerCallbackProc)check_afd_status, w);
      ft_exposure_line = 1;

      if (no_backing_store == False)
      {
         int    bs_attribute;
         Screen *c_screen = ScreenOfDisplay(display, DefaultScreen(display));

         if ((bs_attribute = DoesBackingStore(c_screen)) != NotUseful)
         {
            XSetWindowAttributes attr;

            attr.backing_store = bs_attribute;
            attr.save_under = DoesSaveUnders(c_screen);
            XChangeWindowAttributes(display, line_window,
                                    CWBackingStore | CWSaveUnder, &attr);
            XChangeWindowAttributes(display, label_window,
                                    CWBackingStore, &attr);
            XChangeWindowAttributes(display, button_window,
                                    CWBackingStore, &attr);
            if (no_input == False)
            {
               XChangeWindowAttributes(display, XtWindow(mw[MON_W]),
                                       CWBackingStore, &attr);

               if ((mcp.show_slog != NO_PERMISSION) ||
                   (mcp.show_rlog != NO_PERMISSION) ||
                   (mcp.show_tlog != NO_PERMISSION) ||
                   (mcp.show_ilog != NO_PERMISSION) ||
                   (mcp.show_olog != NO_PERMISSION) ||
                   (mcp.show_elog != NO_PERMISSION) ||
                   (mcp.show_queue != NO_PERMISSION) ||
                   (mcp.afd_load != NO_PERMISSION))
               {
                  XChangeWindowAttributes(display, XtWindow(mw[LOG_W]),
                                          CWBackingStore, &attr);
               }

               if ((mcp.amg_ctrl != NO_PERMISSION) ||
                   (mcp.fd_ctrl != NO_PERMISSION) ||
                   (mcp.rr_dc != NO_PERMISSION) ||
                   (mcp.rr_hc != NO_PERMISSION) ||
                   (mcp.edit_hc != NO_PERMISSION) ||
                   (mcp.dir_ctrl != NO_PERMISSION) ||
                   (mcp.startup_afd != NO_PERMISSION) ||
                   (mcp.shutdown_afd != NO_PERMISSION))
               {
                  XChangeWindowAttributes(display, XtWindow(mw[CONTROL_W]),
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
      }

      /*
       * Calculate the magic unkown height factor we need to add to the
       * height of the widget when it is being resized.
       */
      XtVaGetValues(appshell, XmNheight, &height, NULL);
      magic_value = height - (window_height + line_height +
                    line_height + glyph_height);
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


/*##################### mon_expose_handler_button() #####################*/
void
mon_expose_handler_button(Widget                      w,
                          XtPointer                   client_data,
                          XmDrawingAreaCallbackStruct *call_data)
{
   static int ft_exposure_status = 0;

   XFlush(display);

   /*
    * To ensure that widgets are realized before calling XtAppAddTimeOut()
    * we wait for the widget to get its first expose event. This should
    * take care of the nasty BadDrawable error on slow connections.
    */
   if (ft_exposure_status == 0)
   {
      draw_mon_button_line();

      (void)XtAppAddTimeOut(app, redraw_time_status,
                            (XtTimerCallbackProc)check_mon_status, w);
      ft_exposure_status = 1;
   }
   else
   {
      XEvent *p_event = call_data->event;

      XCopyArea(display, button_pixmap, button_window, color_letter_gc,
                p_event->xexpose.x, p_event->xexpose.y,
                p_event->xexpose.width, p_event->xexpose.height,
                p_event->xexpose.x, p_event->xexpose.y);
   }
   XFlush(display);

   return;
}
