/*
 *  start_drag.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   start_drag - handles the drag part from "Drag & Drop"
 **
 ** SYNOPSIS
 **   void start_drag(Widget   w,
 **                   XEvent   *event,
 **                   String   *params,
 **                   Cardinal *no_of_params)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   23.08.1997 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <Xm/Xm.h>
#include <Xm/DragDrop.h>
#include <Xm/AtomMgr.h>
#include "edit_hc.h"

/* External global variables. */
extern Display *display;
extern Widget  host_list_w,
               source_icon_w,
               start_drag_w;
extern Atom    compound_text;
extern int     in_drop_site;

/* Local function prototypes. */
static Boolean text_convert(Widget, Atom *, Atom *, Atom *, XtPointer *,
                            unsigned long *, int *);


/*############################# start_drag() ############################*/
void
start_drag(Widget   w,
           XEvent   *event,
           String   *params,
           Cardinal *no_of_params)
{
   int           no_selected;
   XmStringTable xmsel;

   /* Retrieve the selected items from the list. */
   XtVaGetValues(host_list_w,
                 XmNselectedItemCount, &no_selected,
                 XmNselectedItems,     &xmsel,
                 NULL);

   if (no_selected > 0)
   {
      Cardinal argcount;
      Arg      args[MAXARGS];
      Atom     targets[1];
      
      targets[0] = compound_text;

      argcount = 0;
      XtSetArg(args[argcount], XmNexportTargets,         targets);
      argcount++;
      XtSetArg(args[argcount], XmNnumExportTargets,      1);
      argcount++;
      XtSetArg(args[argcount], XmNconvertProc,           text_convert);
      argcount++;
      XtSetArg(args[argcount], XmNdragOperations,        XmDROP_MOVE);
      argcount++;
      XtSetArg(args[argcount], XmNblendModel,            XmBLEND_JUST_SOURCE);
      argcount++;
      XtSetArg(args[argcount], XmNsourceCursorIcon,      source_icon_w);
      argcount++;
      start_drag_w = XmDragStart(w, event, args, argcount);
      XtAddCallback(start_drag_w, XmNdropSiteLeaveCallback, leave_notify, NULL);
      XtAddCallback(start_drag_w, XmNdropSiteEnterCallback, enter_notify, NULL);
      in_drop_site = YES;
   }

   return;
}


/*++++++++++++++++++++++++++++ text_convert() +++++++++++++++++++++++++++*/
static Boolean
text_convert(Widget        w,
             Atom          *selection,
             Atom          *target,
             Atom          *type_return,
             XtPointer     *value_return,
             unsigned long *length_return,
             int           *format_return)
{
   if (*target == compound_text)
   {
      *type_return = compound_text;
      *format_return = 8;

      return(True);
   }

   return(False);
}
