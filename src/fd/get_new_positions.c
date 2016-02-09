/*
 *  get_new_positions.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_new_positions - get new FSA and FRA positions for connection
 **                       structure
 **
 ** SYNOPSIS
 **   void get_new_positions(void)
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
 **   18.10.2001 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "fddefs.h"

/* External global variables. */
extern int                        max_connections,
                                  no_of_dirs,
                                  no_of_hosts;
extern struct filetransfer_status *fsa;
extern struct fileretrieve_status *fra;
extern struct connection          *connection;


/*######################### get_new_positions() #########################*/
void
get_new_positions(void)
{
   int          old_pos;
   register int i;

   for (i = 0; i < max_connections; i++)
   {
      if (connection[i].pid > 0)
      {
         old_pos = connection[i].fsa_pos;
         if ((connection[i].fsa_pos = get_host_id_position(fsa,
                                                           connection[i].host_id,
                                                           no_of_hosts)) < 0)
         {
            /*
             * Hmm, not sure what is the best strategy. We have
             * two options. One is to kill the job and remove
             * all data. Second one is to finish this job
             * until it is done. At first glance it looks as
             * if the first is the correct solution. It has
             * however the big disadvantage that when a user
             * changes the alias name of the host, the data is
             * lost. For this reason the second option was
             * implemented. This has the disadvantage that since
             * we cannot differentiate here if we realy deleted a
             * host or just renamed it we must continue sending
             * data to this host, even if the host has been
             * deleted by the user. We do this by putting this
             * host at the end of the FSA that is not visible to
             * the user. We hide it from the user.
             */
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                       "Hmm. Failed to locate host <%s> for connection job %d [pid = %d] has been removed. Writing data to end of FSA 8-(",
#else
                       "Hmm. Failed to locate host <%s> for connection job %d [pid = %lld] has been removed. Writing data to end of FSA 8-(",
#endif
                       connection[i].hostname, i, (pri_pid_t)connection[i].pid);
            connection[i].fsa_pos = no_of_hosts;
            fsa[connection[i].fsa_pos].keep_connected = 0;

            /*
             * Since we now have moved the complete job
             * to the end of the FSA, we need to free up
             * the old FSA position for new jobs. Otherwise
             * it can happen that function get_free_disp_pos()
             * will not find a free slot and no more data
             * is distributed for this host.
             */
            fsa[old_pos].job_status[connection[i].job_no].proc_id = -1;
#ifdef _WITH_BURST_2
            fsa[old_pos].job_status[connection[i].job_no].unique_name[0] = '\0';
            fsa[old_pos].job_status[connection[i].job_no].job_id = NO_ID;
#endif
         }
         if (connection[i].fra_pos != -1)
         {
            if ((connection[i].fra_pos = get_dir_position(fra,
                                                          connection[i].dir_alias,
                                                          no_of_dirs)) < 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                          "Hmm. Failed to locate dir_alias %s for connection job %d [pid = %d] has been removed. Writing data to end of FRA 8-(",
#else
                          "Hmm. Failed to locate dir_alias %s for connection job %d [pid = %lld] has been removed. Writing data to end of FRA 8-(",
#endif
                          connection[i].dir_alias, i,
                          (pri_pid_t)connection[i].pid);
               connection[i].fra_pos = no_of_dirs;
            }
         }
      }
   }

   return;
}
