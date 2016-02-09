/*
 *  afd_load_callbacks.c - Part of AFD, an automatic file distribution
 *                         program.
 *  Copyright (c) 1998 - 2013 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   afd_load_callbacks - various callback functions for afd_load
 **
 ** SYNOPSIS
 **   void close_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void get_connection_value(Widget w, XtPointer client_data, XtPointer chart_value)
 **   void get_file_value(Widget w, XtPointer client_data, XtPointer chart_value)
 **   void get_kbyte_value(Widget w, XtPointer client_data, XtPointer chart_value)
 **   void get_transfer_value(Widget w, XtPointer client_data, XtPointer chart_value)
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
 **   14.03.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* sprintf()                          */
#include <stdlib.h>                /* free()                             */
#include <errno.h>

#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <X11/Intrinsic.h>
#include <X11/Xaw/StripChart.h>
#include "afd_load.h"

/* External global variables. */
extern Widget                     current_value_w;
extern int                        no_of_hosts;
extern double                     prev_value,
                                  update_interval;
extern struct filetransfer_status *fsa;
extern struct afd_status          *p_afd_status;
extern struct afdstat             *afd_stat;


/*############################ close_button() ###########################*/
void
close_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   exit(0);
}


/*####################### get_connection_value() ########################*/
void
get_connection_value(Widget w, XtPointer client_data, XtPointer chart_value)
{
   register int i;
   double       connections = 0.0;

   (void)check_fsa(YES, AFDD);
   for (i = 0; i < no_of_hosts; i++)
   {
      connections += (double)fsa[i].connections;
   }
   connections = prev_value + ((connections - prev_value) / 8.0);
   if (connections < 0.01)
   {
      connections = 0.0;
   }
   *(double *)chart_value = (double)((connections - prev_value) / update_interval);

   if (connections != prev_value)
   {
      char str_line[MAX_CURRENT_VALUE_DIGIT + 1];

      (void)sprintf(str_line, "%10.2f", *(double *)chart_value);
      XmTextSetString(current_value_w, str_line);
   }
   prev_value = connections;

   return;
}


/*########################## get_file_value() ###########################*/
void
get_file_value(Widget w, XtPointer client_data, XtPointer chart_value)
{
   register int i;
   double       no_of_files = 0.0;

   (void)check_fsa(YES, AFDD);
   for (i = 0; i < no_of_hosts; i++)
   {
      no_of_files += (double)fsa[i].file_counter_done;
   }
   no_of_files = prev_value + ((no_of_files - prev_value) / 8.0);
   if (no_of_files < 0.01)
   {
      no_of_files = 0.0;
   }
   *(double *)chart_value = (double)((no_of_files - prev_value) / update_interval);

   if (no_of_files != prev_value)
   {
      char str_line[MAX_CURRENT_VALUE_DIGIT + 1];

      (void)sprintf(str_line, "%10.2f", *(double *)chart_value);
      XmTextSetString(current_value_w, str_line);
   }
   prev_value = no_of_files;

   return;
}


/*########################## get_kbyte_value() ##########################*/
void
get_kbyte_value(Widget w, XtPointer client_data, XtPointer chart_value)
{
   register int i;
   double       kbytes_send = 0.0;

   (void)check_fsa(YES, AFDD);
   for (i = 0; i < no_of_hosts; i++)
   {
      kbytes_send += (double)fsa[i].bytes_send;
   }
   kbytes_send /= 1024.0;
   kbytes_send = prev_value + ((kbytes_send - prev_value) / 8.0);
   if (kbytes_send < 0.01)
   {
      kbytes_send = 0.0;
   }
   *(double *)chart_value = (double)((kbytes_send - prev_value) / (update_interval * 100.0));
   if (*(double *)chart_value < 0.01)
   {
      *(double *)chart_value = 0.0;
   }

   if (kbytes_send != prev_value)
   {
      char str_line[MAX_CURRENT_VALUE_DIGIT + 1];

      (void)sprintf(str_line, "%10.2f", *(double *)chart_value * 100.0);
      XmTextSetString(current_value_w, str_line);
   }
   prev_value = kbytes_send;

   return;
}


/*######################### get_transfer_value() ########################*/
void
get_transfer_value(Widget w, XtPointer client_data, XtPointer chart_value)
{
   double active_transfers;

   active_transfers = prev_value +
                      (((double)p_afd_status->no_of_transfers - prev_value) /
                      8.0);
   if (active_transfers < 0.01)
   {
      active_transfers = 0.0;
   }
   *(double *)chart_value = active_transfers;

   if (active_transfers != prev_value)
   {
      char str_line[MAX_CURRENT_VALUE_DIGIT + 1];

      (void)sprintf(str_line, "%10.2f", *(double *)chart_value);
      XmTextSetString(current_value_w, str_line);
   }
   prev_value = active_transfers;

   return;
}
