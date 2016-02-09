/*
 *  update_info.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   update_info - updates any information that changes for module
 **                 mon_info
 **
 ** SYNOPSIS
 **   void update_info(Widget w)
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
 **   21.02.1999 H.Kiehl Created
 **   10.09.2000 H.Kiehl Added top transfer and top file rate.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>           /* strerror()                              */
#include <time.h>             /* strftime(), localtime()                 */
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <Xm/Xm.h>
#include <Xm/Text.h>
#include "mafd_ctrl.h"
#include "mon_info.h"

/* External global variables. */
extern int                    afd_position;
extern char                   afd_name[],
                              *info_data,
                              label_l[NO_OF_MSA_ROWS][21],
                              label_r[NO_OF_MSA_ROWS][17];
extern Display                *display;
extern XtIntervalId           interval_id_host;
extern XtAppContext           app;
extern Widget                 info_w,
                              text_wl[],
                              text_wr[],
                              label_l_widget[],
                              label_r_widget[];
extern struct mon_status_area *msa;
extern struct prev_values     prev;


/*############################ update_info() ############################*/
void
update_info(Widget w)
{
   static int  interval = 0;
   signed char flush = NO;
   char        str_line[MAX_INFO_STRING_LENGTH],
               tmp_str_line[MAX_INFO_STRING_LENGTH];

   /* Check if MSA changed. */
   (void)check_msa();

   if (prev.afd_toggle != msa[afd_position].afd_toggle)
   {
      prev.afd_toggle = msa[afd_position].afd_toggle;

      /*
       * No need for further actions, since the next two checks
       * will take care for any changes in hostname, IP and port number.
       */
   }

   if (strcmp(prev.real_hostname[(int)prev.afd_toggle],
              msa[afd_position].hostname[(int)prev.afd_toggle]) != 0)
   {
      (void)strcpy(prev.real_hostname[(int)prev.afd_toggle],
                   msa[afd_position].hostname[(int)prev.afd_toggle]);
      (void)sprintf(str_line, "%*s",
                    MON_INFO_LENGTH, prev.real_hostname[(int)prev.afd_toggle]);
      XmTextSetString(text_wl[0], str_line);
      get_ip_no(msa[afd_position].hostname[(int)prev.afd_toggle], tmp_str_line);
      (void)sprintf(str_line, "%*s", MON_INFO_LENGTH, tmp_str_line);
      XmTextSetString(text_wr[0], str_line);
      flush = YES;
   }

   if (prev.port[(int)prev.afd_toggle] != msa[afd_position].port[(int)prev.afd_toggle])
   {
      prev.port[(int)prev.afd_toggle] = msa[afd_position].port[(int)prev.afd_toggle];
      (void)sprintf(str_line, "%*d", MON_INFO_LENGTH, prev.port[(int)prev.afd_toggle]);
      XmTextSetString(text_wl[1], str_line);
      flush = YES;
   }

   if (strcmp(prev.r_work_dir, msa[afd_position].r_work_dir) != 0)
   {
      (void)strcpy(prev.r_work_dir, msa[afd_position].r_work_dir);
      (void)sprintf(str_line, "%*s", MON_INFO_LENGTH, prev.r_work_dir);
      XmTextSetString(text_wr[1], str_line);
      flush = YES;
   }

   if (prev.last_data_time != msa[afd_position].last_data_time)
   {
      prev.last_data_time = msa[afd_position].last_data_time;
      (void)strftime(tmp_str_line, MAX_INFO_STRING_LENGTH, "%d.%m.%Y  %H:%M:%S",
                     localtime(&prev.last_data_time));
      (void)sprintf(str_line, "%*s", MON_INFO_LENGTH, tmp_str_line);
      XmTextSetString(text_wl[2], str_line);
      flush = YES;
   }

   if (prev.poll_interval != msa[afd_position].poll_interval)
   {
      prev.poll_interval = msa[afd_position].poll_interval;
      (void)sprintf(str_line, "%*d", MON_INFO_LENGTH, prev.poll_interval);
      XmTextSetString(text_wr[2], str_line);
      flush = YES;
   }

   if (prev.max_connections != msa[afd_position].max_connections)
   {
      prev.max_connections = msa[afd_position].max_connections;
      (void)sprintf(str_line, "%*u", MON_INFO_LENGTH, prev.max_connections);
      XmTextSetString(text_wl[3], str_line);
      flush = YES;
   }

   if (prev.top_not != msa[afd_position].top_no_of_transfers[0])
   {
      prev.top_not = msa[afd_position].top_no_of_transfers[0];
      (void)sprintf(str_line, "%*d", MON_INFO_LENGTH, prev.top_not);
      XmTextSetString(text_wr[3], str_line);
      flush = YES;
   }

   if (strcmp(prev.afd_version, msa[afd_position].afd_version) != 0)
   {
      (void)strcpy(prev.afd_version, msa[afd_position].afd_version);
      (void)sprintf(str_line, "%*s", MON_INFO_LENGTH, prev.afd_version);
      XmTextSetString(text_wl[4], str_line);
      flush = YES;
   }

   if (prev.no_of_hosts != msa[afd_position].no_of_hosts)
   {
      prev.no_of_hosts = msa[afd_position].no_of_hosts;
      (void)sprintf(str_line, "%*u", MON_INFO_LENGTH, prev.no_of_hosts);
      XmTextSetString(text_wr[4], str_line);
      flush = YES;
   }

   if (prev.top_tr != msa[afd_position].top_tr[0])
   {
      prev.top_tr = msa[afd_position].top_tr[0];
      if (prev.top_tr > 1048576)
      {
         (void)sprintf(str_line, "%*u MB/s",
                       MON_INFO_LENGTH - 5, prev.top_tr / 1048576);
      }
      else if (prev.top_tr > 1024)
           {
              (void)sprintf(str_line, "%*u KB/s",
                            MON_INFO_LENGTH - 5, prev.top_tr / 1024);
           }
           else
           {
              (void)sprintf(str_line, "%*u Bytes/s",
                            MON_INFO_LENGTH - 8, prev.top_tr);
           }
      XmTextSetString(text_wl[5], str_line);
      flush = YES;
   }

   if (prev.top_fr != msa[afd_position].top_fr[0])
   {
      prev.top_fr = msa[afd_position].top_fr[0];
      (void)sprintf(str_line, "%*u files/s", MON_INFO_LENGTH - 8, prev.top_fr);
      XmTextSetString(text_wr[5], str_line);
      flush = YES;
   }

   if (interval++ == FILE_UPDATE_INTERVAL)
   {
      interval = 0;

      /* Check if the information file for this AFD has changed. */
      if (check_info_file(afd_name, AFD_INFO_FILE, YES) == YES)
      {
         flush = YES;
         XmTextSetString(info_w, NULL);  /* Clears old entry. */
         XmTextSetString(info_w, info_data);
      }
   }

   if (flush == YES)
   {
      XFlush(display);
   }

   /* Call update_info() after UPDATE_INTERVAL ms. */
   interval_id_host = XtAppAddTimeOut(app, UPDATE_INTERVAL,
                                      (XtTimerCallbackProc)update_info,
                                      NULL);

   return;
}
