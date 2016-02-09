/*
 *  accept_drop.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2008 Deutscher Wetterdienst (DWD),
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
 **   accept_drop - handles the drop part from "Drag & Drop"
 **
 ** SYNOPSIS
 **   void accept_drop(Widget             w,
 **                    XtPointer          client_data,
 **                    XmDropProcCallback drop)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   21.08.1997 H.Kiehl Created
 **   27.08.2008 H.Kiehl Notify list widget about selected item.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <Xm/Xm.h>
#include <Xm/List.h>
#include <Xm/DragDrop.h>
#include <Xm/AtomMgr.h>
#include "edit_hc.h"

/* External global variables. */
extern Display  *display;
extern Widget   host_list_w;
extern Atom     compound_text;
extern int      host_alias_order_change,
                last_selected;
extern char     last_selected_host[];

/* Local global variables. */
static Position y;

/* Local function prototypes. */
static void     transfer_data(Widget, XtPointer, Atom *, Atom *, XtPointer,
                              unsigned long *, int *);


/*############################# accept_drop() ###########################*/
void
accept_drop(Widget w, XtPointer client_data, XmDropProcCallback drop)
{
   int                    i;
   Cardinal               no_of_exports,
                          argcount;
   Arg                    args[MAXARGS];
   XmDropTransferEntryRec entries[1];
   Atom                   *exports;

   /* See what types of targets are available. */
   XtVaGetValues(drop->dragContext,
                 XmNexportTargets,    &exports,
                 XmNnumExportTargets, &no_of_exports,
                 NULL);

   for (i = 0; i < no_of_exports; i++)
   {
      if (exports[i] == compound_text)
      {
         break;
      }
   }

   if (i > no_of_exports)
   {
      return;
   }

   /* See whether the operation is a supported one. */
   argcount = 0;
   if ((drop->dropAction == XmDROP) &&
       (drop->operations == XmDROP_MOVE))
   {
      /* Set up transfer procedure. */
      entries[0].target = compound_text;
      XtSetArg(args[argcount], XmNdropTransfers,    entries);
      argcount++;
      XtSetArg(args[argcount], XmNnumDropTransfers, 1);
      argcount++;
      XtSetArg(args[argcount], XmNtransferProc,     transfer_data);
      argcount++;
      y = drop->y;
   }
   else
   {
      XtSetArg(args[argcount], XmNtransferStatus, XmTRANSFER_FAILURE);
      argcount++;
      XtSetArg(args[argcount], XmNdropTransfers,  0);
      argcount++;
      y = -1;
   }

   XmDropTransferStart(drop->dragContext, args, argcount);

   return;
}


/*++++++++++++++++++++++++++++ transfer_data() ++++++++++++++++++++++++++*/
static void
transfer_data(Widget        w,
              XtPointer     client_data,
              Atom          *selection,
              Atom          *type,
              XtPointer     value,
              unsigned long *length,
              int           *format)
{
   if ((*type == compound_text) && (y != -1))
   {
      int           i,
                    no_selected,
                    *select_list,
                    pos = XmListYToPos(host_list_w, y);
      char          *ptr;
      XmString      str;
      XmStringTable xmsel;

      /* Retrieve the selected items from the list. */
      XtVaGetValues(host_list_w,
                    XmNselectedItemCount, &no_selected,
                    XmNselectedItems,     &xmsel,
                    NULL);

      for (i = 0; i < no_selected; i++)
      {
         ptr = XmCvtXmStringToCT(xmsel[i]);
         str = XmStringCreateLocalized(ptr);
         if (pos == 0) /* Last position! */
         {
            XmListAddItemUnselected(host_list_w, str, 0);
         }
         else
         {
            XmListAddItemUnselected(host_list_w, str, pos + i);
         }
         XmStringFree(str);
      }
      if (XmListGetSelectedPos(host_list_w, &select_list, &no_selected) == True)
      {
         XmListDeletePositions(host_list_w, select_list, no_selected);
      }

      /* Select the host that was selected last. */
      str = XmStringCreateLocalized(last_selected_host);
      last_selected = -1;
      XmListSelectItem(host_list_w, str, True);
      XmStringFree(str);

      XtFree((char *)select_list);

      host_alias_order_change = YES;
   }

   return;
}
