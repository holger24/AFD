/*
 *  update_group_summary.c - Part of AFD, an automatic file distribution
 *                           program.
 *  Copyright (c) 2017 Holger Kiehl <Holger.Kiehl@dwd.de>
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
   int  i,
        j,
        k,
        m;
   char connect_status,
        log_history[NO_OF_LOG_HISTORY][MAX_LOG_HISTORY];

   for (i = 0; i < no_of_afds; i++)
   {
      if (msa[i].rcmd[0] == '\0')
      {
         connect_status = DISABLED;
         (void)memset(log_history, NO_INFORMATION,
                      (NO_OF_LOG_HISTORY * MAX_LOG_HISTORY));
         msa[i].no_of_transfers = 0;
         msa[i].max_connections = 0;
         msa[i].host_error_counter = 0;
         msa[i].fc = 0;
         msa[i].fs = 0;
         msa[i].tr = 0;
         msa[i].fr = 0;
         msa[i].ec = 0;
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
            msa[i].no_of_transfers += msa[j].no_of_transfers;
            msa[i].max_connections += msa[j].max_connections;
            msa[i].host_error_counter += msa[j].host_error_counter;
            msa[i].fc += msa[j].fc;
            msa[i].fs += msa[j].fs;
            msa[i].tr += msa[j].tr;
            msa[i].fr += msa[j].fr;
            msa[i].ec += msa[j].ec;
         }
         msa[i].connect_status = connect_status;
         for (k = 0;  k < NO_OF_LOG_HISTORY; k++)
         {
            (void)memcpy(msa[i].log_history[k], log_history[k], MAX_LOG_HISTORY);
         }
         i = j - 1;
      }
   }

   return;
}
