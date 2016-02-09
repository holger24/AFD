/*
 *  check_nummeric.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2012 Deutscher Wetterdienst (DWD),
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
 **   check_nummeric - checks the if the input of a text widget is
 **                    nummeric
 **
 ** SYNOPSIS
 **   void check_nummeric(Widget    w,
 **                       XtPointer client_data,
 **                       XtPointer call_data)
 **
 ** DESCRIPTION
 **   This function checks if the input of a text widget is nummeric.
 **   If it detects an invalid character the input is terminated.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   28.08.1997 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <ctype.h>
#include <Xm/Xm.h>
#include <Xm/Text.h>
#include "motif_common_defs.h"


/*############################ check_nummeric() #########################*/
void
check_nummeric(Widget w, XtPointer client_data, XtPointer call_data)
{
   XmTextVerifyCallbackStruct *cbs = (XmTextVerifyCallbackStruct *)call_data;

   /* Check for backspace. */
   if (cbs->text->length == 0)
   {
      /* Reset pointer because of bug in X11: it must be zero. */
      cbs->text->ptr = NULL;
   }
   else
   {
      int i;

      for (i = 0; i < cbs->text->length; i++)
      {
         if (isdigit((int)(*(cbs->text->ptr + i))) == 0)
         {
            cbs->doit = False;
            return;
         }
      }
   }
   cbs->doit = True;

   return;
}
