/*
 *  setup_icon.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2009 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   setup_icon - setup icon for given widget
 **
 ** SYNOPSIS
 **   void setup_icon(Display *display, Widget w)
 **
 ** DESCRIPTION
 **   setup_icon() initializes icon to AFD logo.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.11.2009 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>

#include <Xm/Xm.h>
#include <X11/xpm.h>
#include "motif_common_defs.h"
#include "afd_logo.h"


/*############################ setup_icon() #############################*/
void
setup_icon(Display *display, Widget w)
{
   Pixmap        logo_pixmap,
                 shapemask_pixmap;
   XpmAttributes xpm_attributes;

   (void)memset((void *)&xpm_attributes, '\0', sizeof(XpmAttributes));
   xpm_attributes.valuemask   = (XpmExactColors | XpmCloseness);
   xpm_attributes.exactColors = FALSE;
   xpm_attributes.closeness   = 40000;
   if (XpmCreatePixmapFromData(display,
                               RootWindow(display, DefaultScreen(display)),
                               afd_logo,
                               &logo_pixmap, &shapemask_pixmap,
                               &xpm_attributes) != XpmSuccess)
   {
      (void)fprintf(stderr, "WARNING : XpmCreatePixmapFromData() error.\n");
   }
   else
   {
      XtVaSetValues(w, XmNiconPixmap, logo_pixmap, NULL);
      XpmFreeAttributes(&xpm_attributes);
   }

   return;
}
