/*
 *  info_callbacks.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2010 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   info_callbacks - all callback functions for module afd_info
 **
 ** SYNOPSIS
 **   void save_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void close_button(Widget w, XtPointer client_data, XtPointer call_data)
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
 **   10.11.1996 H.Kiehl Created
 **   20.09.2010 H.Kiehl Added save_button().
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <stdlib.h>
#include <Xm/Xm.h>
#include "afd_info.h"

/* Global variables. */
extern Widget info_w;
extern char   host_name[];


/*########################### save_button() #############################*/
void
save_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   write_info_file(info_w, host_name, HOST_INFO_FILE);

   return;
}


/*########################### close_button() ############################*/
void
close_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   exit(0);
}
