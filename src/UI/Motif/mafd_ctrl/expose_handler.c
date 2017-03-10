/*
 *  expose_handler.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2017 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   expose_handler - handles any expose event for the various
 **                    drawing areas
 **
 ** SYNOPSIS
 **   void expose_handler_label(Widget w, XtPointer client_data,
 **                             XmDrawingAreaCallbackStruct *call_data)
 **   void expose_handler_line(Widget w, XtPointer client_data,
 **                            XmDrawingAreaCallbackStruct *call_data)
 **   void expose_handler_button(Widget w, XtPointer client_data,
 **                              XmDrawingAreaCallbackStruct *call_data)
 **   void expose_handler_tv_line(Widget w, XtPointer client_data,
 **                               XmDrawingAreaCallbackStruct *call_data)
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
 **   27.01.1996 H.Kiehl Created
 **   04.08.1997 H.Kiehl Added check for backing store and save under.
 **                      Backing store now also for menu bar.
 **   10.01.1998 H.Kiehl Support for detailed view of transfers.
 **   30.07.2001 H.Kiehl Support for the show_queue dialog.
 **   08.11.2002 H.Kiehl Expose handler for short line drawing area.
 **
 */
DESCR__E_M3

#include <stdio.h>              /* printf()                              */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <Xm/Xm.h>
#include "mafd_ctrl.h"
#include "permission.h"

extern Display                 *display;
extern XtAppContext            app;
extern XtIntervalId            interval_id_tv;
extern Pixmap                  button_pixmap,
                               label_pixmap,
                               line_pixmap;
extern Window                  button_window,
                               detailed_window,
                               label_window,
                               line_window,
                               tv_label_window;
extern Widget                  appshell,
                               detailed_window_w,
                               mw[],
                               tv_label_window_w;
extern GC                      color_letter_gc,
                               default_bg_gc,
                               label_bg_gc;
extern int                     ft_exposure_tv_line,
                               magic_value,
                               no_input,
                               no_of_hosts_visible,
                               no_of_jobs_selected,
                               window_height,
                               tv_line_length,
                               line_height,
                               no_of_columns,
                               tv_no_of_columns,
                               no_of_rows,
                               tv_no_of_rows,
                               *vpl,
                               window_width;
extern unsigned long           redraw_time_host,
                               redraw_time_status;
extern unsigned int            glyph_height;
extern struct line             *connect_data;
extern struct job_data         *jd;
extern struct afd_control_perm acp;


/*######################## expose_handler_label() #######################*/
void
expose_handler_label(Widget                      w,
                     XtPointer                   client_data,
                     XmDrawingAreaCallbackStruct *call_data)
{
   XT_PTR_TYPE label_type = (XT_PTR_TYPE)client_data;
   static int  ft_exposure_label = 0; /* First time exposure label. */

   if (label_type == 0)
   {
      if (ft_exposure_label == 0)
      {
         draw_label_line();
         ft_exposure_label = 1;
      }
      else
      {
         XEvent *p_event = call_data->event;

/*
printf("x=%d y=%d width=%d height=%d COUNT=%d\n",
       p_event->xexpose.x, p_event->xexpose.y, p_event->xexpose.width,
       p_event->xexpose.height, p_event->xexpose.count);
*/
         XCopyArea(display, label_pixmap, label_window, label_bg_gc,
                   p_event->xexpose.x, p_event->xexpose.y,
                   p_event->xexpose.width, p_event->xexpose.height,
                   p_event->xexpose.x, p_event->xexpose.y);
      }
   }
   else
   {
      if (ft_exposure_tv_line == 0)
      {
         tv_label_window = XtWindow(tv_label_window_w);
      }
      draw_tv_label_line();
   }
   XFlush(display);

   return;
}


/*######################## expose_handler_line() ########################*/
void
expose_handler_line(Widget                      w,
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
      int       bs_attribute,
                i;
      Dimension height;
      Screen    *c_screen = ScreenOfDisplay(display, DefaultScreen(display));

      XFillRectangle(display, line_pixmap, default_bg_gc, 0, 0,
                     window_width, (line_height * no_of_rows));
      for (i = 0; i < no_of_hosts_visible; i++)
      {
         draw_line_status(i, 1);
      }

      (void)XtAppAddTimeOut(app, redraw_time_host,
                            (XtTimerCallbackProc)check_host_status, w);
      ft_exposure_line = 1;

      if ((bs_attribute = DoesBackingStore(c_screen)) != NotUseful)
      {
         XSetWindowAttributes attr;

         attr.backing_store = bs_attribute;
         attr.save_under = DoesSaveUnders(c_screen);
         XChangeWindowAttributes(display, line_window,
                                 CWBackingStore | CWSaveUnder, &attr);
         XChangeWindowAttributes(display, button_window,
                                 CWBackingStore, &attr);
         XChangeWindowAttributes(display, label_window,
                                 CWBackingStore, &attr);
         if (no_input == False)
         {
            XChangeWindowAttributes(display, XtWindow(mw[HOST_W]),
                                    CWBackingStore, &attr);

            if ((acp.show_slog != NO_PERMISSION) ||
                (acp.show_mlog != NO_PERMISSION) ||
                (acp.show_rlog != NO_PERMISSION) ||
                (acp.show_tlog != NO_PERMISSION) ||
                (acp.show_dlog != NO_PERMISSION) ||
                (acp.show_ilog != NO_PERMISSION) ||
                (acp.show_olog != NO_PERMISSION) ||
                (acp.show_queue != NO_PERMISSION) ||
                (acp.show_elog != NO_PERMISSION) ||
                (acp.view_jobs != NO_PERMISSION))
            {
               XChangeWindowAttributes(display, XtWindow(mw[LOG_W]),
                                       CWBackingStore, &attr);
            }

            if ((acp.amg_ctrl != NO_PERMISSION) ||
                (acp.fd_ctrl != NO_PERMISSION) ||
                (acp.rr_dc != NO_PERMISSION) ||
                (acp.rr_hc != NO_PERMISSION) ||
                (acp.edit_hc != NO_PERMISSION) ||
                (acp.startup_afd != NO_PERMISSION) ||
                (acp.shutdown_afd != NO_PERMISSION) ||
                (acp.dir_ctrl != NO_PERMISSION))
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


/*####################### expose_handler_button() #######################*/
void
expose_handler_button(Widget                      w,
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
      draw_button_line();

      (void)XtAppAddTimeOut(app, redraw_time_status,
                            (XtTimerCallbackProc)check_status, w);
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


/*###################### expose_handler_tv_line() #######################*/
void
expose_handler_tv_line(Widget                      w,
                       XtPointer                   client_data,
                       XmDrawingAreaCallbackStruct *call_data)
{
   XEvent *p_event = call_data->event;
   int    i,
          top_line,
          bottom_line,
          left_column,
          top_row,
          bottom_row,
          right_column;

   /* Get the channel no's, which have to be redrawn. */
   left_column = p_event->xexpose.x / tv_line_length;
   top_row = p_event->xexpose.y / line_height;
   bottom_row = (p_event->xexpose.y  + p_event->xexpose.height) /
                 line_height;
   if (bottom_row >= no_of_rows)
   {
      bottom_row--;
   }
   right_column = (p_event->xexpose.x  + p_event->xexpose.width) /
                   tv_line_length;
   if (right_column >= tv_no_of_columns)
   {
      right_column--;
   }

#ifdef _DEBUG
   (void)printf("xexpose.x   = %d    xexpose.width= %d   tv_line_length = %d\n",
                 p_event->xexpose.x, p_event->xexpose.width, tv_line_length);
   (void)printf("left_column = %d    right_column = %d\n",
                 left_column, right_column);
   (void)printf("top_row     = %d    bottom_row   = %d\n",
                 top_row, bottom_row);
#endif

   /* Note which lines have to be redrawn, but do not    */
   /* redraw them here. First collect all expose events. */
   do
   {
      top_line = (right_column * tv_no_of_rows) + top_row;
      bottom_line = (right_column * tv_no_of_rows) + bottom_row;
      while (bottom_line >= no_of_jobs_selected)
      {
         bottom_line--;
      }

#ifdef _DEBUG
      (void)printf("top_line     = %d    bottom_line   = %d\n",
                   top_line, bottom_line);
      (void)printf("=====> tv_no_of_columns = %d   tv_no_of_rows = %d <=====\n",
                   tv_no_of_columns, tv_no_of_rows);
#endif

      for (i = top_line; ((i <= bottom_line) && (i < no_of_jobs_selected)); i++)
      {
         jd[i].expose_flag = YES;
      }
      right_column--;
   } while (left_column <= right_column);

   /* Now see if there are still expose events. If so, */
   /* do NOT redraw.                                   */
   if (p_event->xexpose.count == 0)
   {
      /*
       * To ensure that widgets are realized before calling XtAppAddTimeOut()
       * we wait for the widget to get its first expose event. This should
       * take care of the nasty BadDrawable error on slow connections.
       */
      if (ft_exposure_tv_line == 0)
      {
         int bs_attribute;
         Screen *c_screen = ScreenOfDisplay(display, DefaultScreen(display));

         detailed_window = XtWindow(detailed_window_w);
         interval_id_tv = XtAppAddTimeOut(app, TV_STARTING_REDRAW_TIME,
                                          (XtTimerCallbackProc)check_tv_status,
                                          w);
         ft_exposure_tv_line = 1;

         if ((bs_attribute = DoesBackingStore(c_screen)) != NotUseful)
         {
            XSetWindowAttributes attr;

            attr.backing_store = bs_attribute;
            attr.save_under = DoesSaveUnders(c_screen);
            XChangeWindowAttributes(display, detailed_window,
                                    CWBackingStore, &attr);
            XChangeWindowAttributes(display, tv_label_window,
                                    CWBackingStore, &attr);
         }
      }

      for (i = 0; i < no_of_jobs_selected; i++)
      {
         if (jd[i].expose_flag == YES)
         {
            draw_detailed_line(i);
            jd[i].expose_flag = NO;
         }
      }

      XFlush(display);
   }

   return;
}
