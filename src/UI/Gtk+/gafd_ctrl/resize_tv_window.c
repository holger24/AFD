/*
 *  resize_tv_window.c - Part of AFD, an automatic file distribution
 *                       program.
 *  Copyright (c) 1998 - 2003 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   resize_tv_window - resizes the detailed transfer view window
 **
 ** SYNOPSIS
 **   signed char resize_tv_window(void)
 **
 ** DESCRIPTION
 **   The size of the window is changed, and when the _AUTO_REPOSITION
 **   option is set, the window is repositioned when it touches the
 **   right or bottom end of the screen.
 **   The size of the label window is changed when the height in
 **   line (other font) has changed.
 **
 ** RETURN VALUES
 **   YES - Window has been resized
 **   NO  - Window has not been resized
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   07.01.1998 H.Kiehl Created
 **   26.11.2003 H.Kiehl Disallow user to change window width and height
 **                      via any window manager.
 **
 */
DESCR__E_M3

#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include "gafd_ctrl.h"

extern Display      *display;
extern Widget       transviewshell,
                    detailed_window_w,
                    tv_label_window_w;
extern int          line_height;
extern Dimension    tv_window_width,
                    tv_window_height;
extern unsigned int glyph_height;


/*########################## resize_tv_window() #########################*/
signed char
resize_tv_window(void)
{
   if ((transviewshell != (Widget)NULL) &&
       (tv_window_size(&tv_window_width, &tv_window_height) == YES))
   {
#ifdef __AUTO_REPOSITION
      XWindowAttributes window_attrib;
      int               display_width,
                        display_height,
                        new_x,
                        new_y;
      Position          root_x,
                        root_y;
#endif
      static int        old_line_height = 0;
      Arg               args[5];
      Cardinal          argcount;

      argcount = 0;
      XtSetArg(args[argcount], XmNheight, tv_window_height);
      argcount++;
      XtSetArg(args[argcount], XmNwidth, tv_window_width);
      argcount++;
      XtSetValues(detailed_window_w, args, argcount);
#ifdef __AUTO_REPOSITION
      /* Get new window position. */
      display_width = DisplayWidth(display, DefaultScreen(display));
      display_height = DisplayHeight(display, DefaultScreen(display));
      XGetWindowAttributes(display, XtWindow(transviewshell), &window_attrib);

      /* Translate coordinates relative to root window. */
      XtTranslateCoords(transviewshell, window_attrib.x, window_attrib.y,
                        &root_x, &root_y);

      /* Change x coordinate. */
      if ((root_x + tv_window_width) > display_width)
      {
         new_x = display_width - tv_window_width;

         /* Is window wider then display? */
         if (new_x < 0)
         {
            new_x = 0;
         }
      }
      else
      {
         new_x = root_x;
      }

      /* Change y coordinate. */
      if ((root_y + tv_window_height + 23) > display_height)
      {
         new_y = display_height - tv_window_height;

         /* Is window wider then display? */
         if (new_y < 23)
         {
            new_y = 23;
         }
      }
      else
      {
         new_y = root_y;
      }

      /* Resize window. */
      XtVaSetValues(transviewshell,
                    XmNminWidth, tv_window_width,
                    XmNmaxWidth, tv_window_width,
                    XmNminHeight, tv_window_height + line_height,
                    XmNmaxHeight, tv_window_height + line_height,
                    NULL);
      XMoveResizeWindow(display, XtWindow(transviewshell),
                        new_x,
                        new_y,
                        tv_window_width,
                        tv_window_height + line_height);
#else
      XtVaSetValues(transviewshell,
                    XmNminWidth, tv_window_width,
                    XmNmaxWidth, tv_window_width,
                    XmNminHeight, tv_window_height + line_height,
                    XmNmaxHeight, tv_window_height + line_height,
                    NULL);
      XResizeWindow(display, XtWindow(transviewshell), tv_window_width,
                    tv_window_height + line_height);
#endif

      /* If the line_height changed, don't forget to change the */
      /* height of the label and button window!                 */
      if (line_height != old_line_height)
      {
         argcount = 0;
         XtSetArg(args[argcount], XmNheight, (Dimension)line_height);
         argcount++;
         XtSetValues(tv_label_window_w, args, argcount);

         old_line_height = line_height;
      }

      return(YES);
   }
   else
   {
      return(NO);
   }
}
