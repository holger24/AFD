/*
 *  show_message.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2007 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   show_message - shows message in status box
 **   reset_message - clears message in status box
 **
 ** SYNOPSIS
 **   void show_message(Widget w, char *message)
 **   void reset_message(Widget w)
 **
 ** DESCRIPTION
 **   show_message() will display a short message 'message' at the
 **   bottom of the show_olog dialog.
 **   reset_message() will remove any message that is currently
 **   shown.
 **
 ** RETURN VALUES
 **   Both have no return value.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   04.04.1997 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>

#include <Xm/Xm.h>
#include <Xm/Label.h>
#include "motif_common_defs.h"

/* Local global variables. */
static char   status_message[MAX_MESSAGE_LENGTH];


/*########################## reset_message() ############################*/
void
reset_message(Widget w)
{
   if ((status_message[0] != ' ') && (status_message[1] != '\0'))
   {
      XmString xstr;

      status_message[0] = ' ';
      status_message[1] = '\0';

      xstr = XmStringCreateLtoR(status_message, XmFONTLIST_DEFAULT_TAG);
      XtVaSetValues(w, XmNlabelString, xstr, NULL);
      XmStringFree(xstr);
   }

   return;
}


/*########################### show_message() ############################*/
void
show_message(Widget w, char *message)
{
   XmString xstr;

   (void)strcpy(status_message, message);
   xstr = XmStringCreateLtoR(status_message, XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);

   return;
}
