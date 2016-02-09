/*
 *  wait_visible.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   wait_visible - ensures that a window is visible.
 **
 ** SYNOPSIS
 **   void wait_visible(Widget w)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** NOTE
 **   This function was taken and slightly modified from the book
 **   Motif Programming Manual Volume 6A from Dan Heller and
 **   Paula M. Ferguson on page 750.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.05.2000 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>

#include "motif_common_defs.h"


/*########################### wait_visible() ############################*/
void
wait_visible(Widget w)
{
   Widget diashell,
          topshell;

   for (diashell = w; !XtIsShell(diashell); diashell = XtParent(diashell))
   {
      ;
   }
   for (topshell = diashell; !XtIsTopLevelShell(topshell); topshell = XtParent(topshell))
   {
      ;
   }

   if (XtIsRealized(diashell) && XtIsRealized(topshell))
   {
      Display           *display = XtDisplay(topshell);
      XEvent            event;
      Window            diawindow = XtWindow(diashell),
                        topwindow = XtWindow(topshell);
      XtAppContext      cxt = XtWidgetToApplicationContext(w);
      XWindowAttributes xwa;

      while (XGetWindowAttributes(display, diawindow, &xwa) &&
             (xwa.map_state != IsViewable))
      {
         if (XGetWindowAttributes(display, topwindow, &xwa) &&
             (xwa.map_state != IsViewable))
         {
            break;
         }
         XtAppNextEvent(cxt, &event);
         XtDispatchEvent(&event);
      }
   }

   XmUpdateDisplay(topshell);

   return;
}
