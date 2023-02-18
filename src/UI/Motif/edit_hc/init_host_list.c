/*
 *  init_host_list.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   init_host_list - initialiaze host list in list widget
 **
 ** SYNOPSIS
 **   void init_host_list(int seleted_host_no)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   20.09.2008 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <Xm/Xm.h>
#include <Xm/List.h>
#include <errno.h>
#include "edit_hc.h"

/* External global variables. */
extern Widget                     host_list_w;
extern int                        no_of_hosts;
extern struct changed_entry       *ce;
extern struct filetransfer_status *fsa;


/*########################### init_host_list() ##########################*/
void
init_host_list(int seleted_host_no)
{
   int           i;
   XmStringTable item_list;

   if (ce != NULL)
   {
      free(ce);
      XmListDeleteAllItems(host_list_w);
   }

   /* Allocate memory to store all changes. */
   if ((ce = malloc(no_of_hosts * sizeof(struct changed_entry))) == NULL)
   {
      (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   item_list = (XmStringTable)XtMalloc(no_of_hosts * sizeof(XmString));

   for (i = 0; i < no_of_hosts; i++)
   {
      item_list[i] = XmStringCreateLocalized(fsa[i].host_alias);

      /* Initialize array holding all changed entries. */
      ce[i].value_changed = 0;
      ce[i].value_changed2 = 0;
      ce[i].value_changed3 = 0;
      ce[i].real_hostname[0][0] = -1;
      ce[i].real_hostname[1][0] = -1;
      ce[i].proxy_name[0] = -1;
      ce[i].transfer_timeout = -1L;
      ce[i].retry_interval = -1;
      ce[i].max_errors = -1;
      ce[i].max_successful_retries = -1;
      ce[i].allowed_transfers = -1;
      ce[i].block_size = -1;
      ce[i].file_size_offset = -3;
      ce[i].transfer_rate_limit = -1;
      ce[i].sndbuf_size = 0;
      ce[i].rcvbuf_size = 0;
      ce[i].keep_connected = 0;
#ifdef WITH_DUP_CHECK
      ce[i].dup_check_flag = 0;
      ce[i].dup_check_timeout = 0L;
#endif
      ce[i].warn_time_days = 0;
      ce[i].warn_time_hours = 0;
      ce[i].warn_time_mins = 0;
      ce[i].warn_time_secs = 0;
      if (fsa[i].host_toggle_str[0] == '\0')
      {
         ce[i].host_toggle[0][0] = '1';
         ce[i].host_toggle[1][0] = '2';
         ce[i].host_switch_toggle = OFF;
         ce[i].auto_toggle = OFF;
      }
      else
      {
         ce[i].host_toggle[0][0] = fsa[i].host_toggle_str[HOST_ONE];
         ce[i].host_toggle[1][0] = fsa[i].host_toggle_str[HOST_TWO];
         ce[i].host_switch_toggle = ON;
         if (fsa[i].auto_toggle == ON)
         {
            ce[i].auto_toggle = ON;
         }
         else
         {
            ce[i].auto_toggle = OFF;
         }
      }
   }

   XtVaSetValues(host_list_w,
                 XmNitems,      item_list,
                 XmNitemCount,  no_of_hosts,
                 NULL);

   for (i = 0; i < no_of_hosts; i++)
   {
      XmStringFree(item_list[i]);
   }
   XtFree((char *)item_list);

   /* Select the given host. */
   if (no_of_hosts > 0)
   {
      int top,
          visible;

      XmListSelectPos(host_list_w, seleted_host_no + 1, True);

      /*
       * This code was taken from the book Motif Programming Manual
       * Volume 6A by Dan Heller & Paula M. Ferguson.
       */
      XtVaGetValues(host_list_w,
                    XmNtopItemPosition,  &top,
                    XmNvisibleItemCount, &visible,
                    NULL);
      if ((seleted_host_no + 1) < top)
      {
         XmListSetPos(host_list_w, (seleted_host_no + 1));
      }
      else if ((seleted_host_no + 1) >= (top + visible))
           {
              XmListSetBottomPos(host_list_w, (seleted_host_no + 1));
           }
   }

   return;
}
