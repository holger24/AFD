/*
 *  disable_drag_drop.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   disable_drag_drop - disables drag and drop for the application
 **
 ** SYNOPSIS
 **   void disable_drag_drop(Widget top_level_shell)
 **
 ** DESCRIPTION
 **   The function disable_drag_drop() disables drag and drop functionality
 **   for the given top level shell.
 **
 ** RETURN VALUES
 **   Returns nothing.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   08.08.2009 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>

#include <Xm/Xm.h>
#include <Xm/Display.h>
#include "motif_common_defs.h"


/*######################## disable_drag_drop() ##########################*/
void
disable_drag_drop(Widget top_level_shell)
{
   Widget display_w;

   display_w = XmGetXmDisplay(XtDisplay(top_level_shell));
   XtVaSetValues(display_w, XmNdragInitiatorProtocolStyle, XmDRAG_NONE, NULL);
   XtVaSetValues(display_w, XmNdragReceiverProtocolStyle, XmDRAG_NONE, NULL);

   return;
}
