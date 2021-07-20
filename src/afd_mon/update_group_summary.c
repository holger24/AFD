/*
 *  update_group_summary.c - Part of AFD, an automatic file distribution
 *                           program.
 *  Copyright (c) 2017 - 2021 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   update_group_summary - updates all values of the group elements
 **
 ** SYNOPSIS
 **   void update_group_summary(void)
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
 **   25.02.2017 H.Kiehl Created
 **   20.07.2021 H.Kiehl Calculate top values as well and add no_of_jobs.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* memcpy()                        */
#include <errno.h>
#include "mondefs.h"


/* External global variables. */
extern int                    no_of_afds;
extern struct mon_status_area *msa;


/*############################# mon_log() ###############################*/
void
update_group_summary(void)
{
   int          host_error_counter,
                i,
                j,
                jobs_in_queue,
                k,
                m,
                max_connections,
                no_of_dirs,
                no_of_hosts,
                no_of_transfers;
   unsigned int ec,
                fc,
                fr,
                no_of_jobs;
   long         danger_no_of_jobs;
   u_off_t      fs,
                tr;
   time_t       last_data_time;
   char         amg,
                archive_watch,
                connect_status,
                fd,
                log_history[NO_OF_LOG_HISTORY][MAX_LOG_HISTORY];

   for (i = 0; i < no_of_afds; i++)
   {
      if (msa[i].rcmd[0] == '\0')
      {
         connect_status = DISABLED;
         (void)memset(log_history, NO_INFORMATION,
                      (NO_OF_LOG_HISTORY * MAX_LOG_HISTORY));
         no_of_transfers = 0;
         max_connections = 0;
         host_error_counter = 0;
         jobs_in_queue = 0;
         danger_no_of_jobs = 0;
         fc = 0;
         fs = 0;
         tr = 0;
         fr = 0;
         ec = 0;
         last_data_time = 0;
         no_of_hosts = 0;
         no_of_dirs = 0;
         no_of_jobs = 0;
         amg = fd = archive_watch = 20;
         for (j = i + 1; j < no_of_afds && msa[j].rcmd[0] != '\0'; j++)
         {
            if (msa[j].connect_status > connect_status)
            {
               connect_status = msa[j].connect_status;
            }
            for (k = 0;  k < NO_OF_LOG_HISTORY; k++)
            {
               for (m = 0; m < MAX_LOG_HISTORY; m++)
               {
                  if (msa[j].log_history[k][m] > log_history[k][m])
                  {
                     log_history[k][m] = msa[j].log_history[k][m];
                  }
               }
            }
            no_of_transfers += msa[j].no_of_transfers;
            max_connections += msa[j].max_connections;
            host_error_counter += msa[j].host_error_counter;
            jobs_in_queue += msa[j].jobs_in_queue;
            danger_no_of_jobs += msa[j].danger_no_of_jobs;
            fc += msa[j].fc;
            fs += msa[j].fs;
            tr += msa[j].tr;
            fr += msa[j].fr;
            ec += msa[j].ec;
            if (msa[j].last_data_time > last_data_time)
            {
               last_data_time = msa[j].last_data_time;
            }
            no_of_hosts += msa[j].no_of_hosts;
            no_of_dirs += msa[j].no_of_dirs;
            no_of_jobs += msa[j].no_of_jobs;
            if (msa[j].amg < amg)
            {
               amg = msa[j].amg;
            }
            else if ((amg == ON) && (msa[j].amg == SHUTDOWN))
                 {
                    amg = SHUTDOWN;
                 }
            if (msa[j].fd < fd)
            {
               fd = msa[j].fd;
            }
            else if ((fd == ON) && (msa[j].fd == SHUTDOWN))
                 {
                    fd = SHUTDOWN;
                 }
            if (msa[j].archive_watch < archive_watch)
            {
               archive_watch = msa[j].archive_watch;
            }
            else if ((archive_watch == ON) && (msa[j].archive_watch == SHUTDOWN))
                 {
                    archive_watch = SHUTDOWN;
                 }
         }
         msa[i].no_of_transfers = no_of_transfers;
         if (no_of_transfers > msa[i].top_no_of_transfers[0])
         {
            msa[i].top_no_of_transfers[0] = msa[i].no_of_transfers;
            msa[i].top_not_time = msa[i].last_data_time;
         }
         msa[i].max_connections = max_connections;
         msa[i].host_error_counter = host_error_counter;
         msa[i].jobs_in_queue = jobs_in_queue;
         msa[i].danger_no_of_jobs = danger_no_of_jobs;
         msa[i].fc = fc;
         msa[i].fs = fs;
         msa[i].tr = tr;
         if (tr > msa[i].top_tr[0])
         {
            msa[i].top_tr[0] = msa[i].tr;
            msa[i].top_tr_time = msa[i].last_data_time;
         }
         msa[i].fr = fr;
         if (fr > msa[i].top_fr[0])
         {
            msa[i].top_fr[0] = msa[i].fr;
            msa[i].top_fr_time = msa[i].last_data_time;
         }
         msa[i].ec = ec;
         msa[i].no_of_hosts = no_of_hosts;
         msa[i].no_of_dirs = no_of_dirs;
         msa[i].no_of_jobs = no_of_jobs;
         msa[i].connect_status = connect_status;
         for (k = 0;  k < NO_OF_LOG_HISTORY; k++)
         {
            (void)memcpy(msa[i].log_history[k], log_history[k],
                         MAX_LOG_HISTORY);
         }
         msa[i].amg = amg;
         msa[i].fd = fd;
         msa[i].archive_watch = archive_watch;
         i = j - 1;
      }
   }

   return;
}
