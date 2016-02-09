/*
 *  remove_paste_newline.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   remove_paste_newline - removes the last new line of a paste
 **
 ** SYNOPSIS
 **   void remove_paste_newline(Widget w, XtPointer client_data, XtPointer call_data)
 **
 ** DESCRIPTION
 **   The callback function remove_paste_newline() removes the last new
 **   line of a paste into a test widget.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.11.2014 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>

#include <Xm/Xm.h>
#include <Xm/Text.h>
#include "motif_common_defs.h"


/*####################### remove_paste_newline() ########################*/
void
remove_paste_newline(Widget w, XtPointer client_data, XtPointer call_data)
{
   XmTextVerifyCallbackStruct *cbs = (XmTextVerifyCallbackStruct *)call_data;

   if (cbs->text->length > 1)
   {
      if (cbs->text->ptr[cbs->text->length - 1] == '\n')
      {
         cbs->text->ptr[cbs->text->length - 1] = '\0';
         cbs->text->length -= 1;
      }
   }

   return;
}
