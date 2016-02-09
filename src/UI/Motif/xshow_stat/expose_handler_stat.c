/*
 *  expose_handler_stat.c - Part of AFD, an automatic file distribution
 *                          program.
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
 **   expose_handler_stat - handles any expose event
 **
 ** SYNOPSIS
 **   void expose_handler_stat(Widget w, XtPointer client_data,
 **                            XmDrawingAreaCallbackStruct *call_data)
 **
 ** DESCRIPTION
 **   This function redraws data in the drawing area widget when an
 **   expose event has occured. Since there is not much to be redrawn
 **   lets not try to be clever, we just redraw everything.
 **
 ** RETURN VALUES
 **   None
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.12.2001 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>              /* printf()                              */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <Xm/Xm.h>
#include "xshow_stat.h"
#include "permission.h"

extern Display                 *display;
extern XtAppContext            app;
extern Window                  stat_window;
extern int                     no_of_hosts;


/*######################## expose_handler_stat() ########################*/
void
expose_handler_stat(Widget                      w,
                    XtPointer                   client_data,
                    XmDrawingAreaCallbackStruct *call_data)
{
   XEvent     *p_event = call_data->event;
   int        i,
              top_line,
              bottom_line,
              left_column = 0,
              top_row,
              bottom_row,
              right_column = 0;
   static int ft_exposure_line = 0; /* First time exposure line. */

#ifdef _DEBUG
   (void)printf("xexpose.x   = %d    xexpose.width= %d\n",
                 p_event->xexpose.x, p_event->xexpose.width);
   (void)printf("left_column = %d    right_column = %d\n",
                 left_column, right_column);
   (void)printf("top_row     = %d    bottom_row   = %d\n",
                 top_row, bottom_row);
#endif

   /* Now see if there are still expose events. If so, */
   /* do NOT redraw.                                   */
   if (p_event->xexpose.count == 0)
   {
      draw_stat();

      XFlush(display);

      /*
       * To ensure that widgets are realized before calling XtAppAddTimeOut()
       * we wait for the widget to get its first expose event. This should
       * take care of the nasty BadDrawable error on slow connections.
       */
      if (ft_exposure_line == 0)
      {
         int       bs_attribute;
         Screen    *c_screen = ScreenOfDisplay(display, DefaultScreen(display));

/*
         interval_id_host = XtAppAddTimeOut(app, redraw_time_host,
                                            (XtTimerCallbackProc)check_host_status,
                                            w);
*/
         ft_exposure_line = 1;

         if ((bs_attribute = DoesBackingStore(c_screen)) != NotUseful)
         {
            XSetWindowAttributes attr;

            attr.backing_store = bs_attribute;
            attr.save_under = DoesSaveUnders(c_screen);
            XChangeWindowAttributes(display, stat_window,
                                    CWBackingStore | CWSaveUnder, &attr);
         }
      }
   }

   return;
}
