/*
 *  remove_connection.c - Part of AFD, an automatic file distribution program.
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
 **   remove_connection - resets data in connection and FSA structures
 **
 ** SYNOPSIS
 **   void remove_connection(struct connection *p_con,
 **                          int               faulty,
 **                          time_t            now)
 **
 ** DESCRIPTION
 **   This function resets all necessary values in the connection and
 **   FSA structure after a job has been removed.
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

#include <stdio.h>
#include "fddefs.h"

/* #define WITH_MULTI_FSA_CHECKS */

/* External global variables. */
extern int                        fsa_fd,
                                  fra_fd,
                                  no_of_hosts,
                                  no_of_trl_groups;
extern struct filetransfer_status *fsa;
extern struct fileretrieve_status *fra;
extern struct afd_status          *p_afd_status;


/*######################### remove_connection() #########################*/
void
remove_connection(struct connection *p_con, int faulty, time_t now)
{
   /*
    * Reset some values of FSA structure. But before we do so it's
    * essential that we do NOT write to an old FSA! So lets check if we
    * are still in the correct FSA. Otherwise we subtract the number of
    * active transfers without ever resetting the pid! This can lead to
    * some very fatal behaviour of the AFD.
    */
#ifdef WITH_MULTI_FSA_CHECKS
   if ((p_con->fsa_pos != -1) && (p_con->fsa_pos < no_of_hosts))
   {
      if (fd_check_fsa() == YES)
      {
         (void)check_fra_fd();
         get_new_positions();
         init_msg_buffer();
      }
   }
#endif

   if ((p_con->fsa_pos != -1) && (p_con->fsa_pos < no_of_hosts))
   {
      /* Decrease number of active transfers to this host in FSA. */
      if (faulty == YES)
      {
         off_t lock_offset;

         lock_offset = AFD_WORD_OFFSET +
                       (p_con->fsa_pos * sizeof(struct filetransfer_status));
         fsa[p_con->fsa_pos].last_retry_time = now;
         if (p_con->fra_pos != -1)
         {
            lock_region_w(fra_fd,
#ifdef LOCK_DEBUG
                          (char *)&fra[p_con->fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                          (char *)&fra[p_con->fra_pos].error_counter - (char *)fra);
#endif
            fra[p_con->fra_pos].error_counter += 1;
            if ((fra[p_con->fra_pos].error_counter >= fra[p_con->fra_pos].max_errors) &&
                ((fra[p_con->fra_pos].dir_flag & DIR_ERROR_SET) == 0))
            {
               fra[p_con->fra_pos].dir_flag |= DIR_ERROR_SET;
               SET_DIR_STATUS(fra[p_con->fra_pos].dir_flag,
                              now,
                              fra[p_con->fra_pos].start_event_handle,
                              fra[p_con->fra_pos].end_event_handle,
                              fra[p_con->fra_pos].dir_status);
            }
            unlock_region(fra_fd,
#ifdef LOCK_DEBUG
                          (char *)&fra[p_con->fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                          (char *)&fra[p_con->fra_pos].error_counter - (char *)fra);
#endif
            error_action(fra[p_con->fra_pos].dir_alias, "stop",
                         DIR_ERROR_ACTION);
            event_log(now, EC_DIR, ET_EXT, EA_ERROR_END, "%s",
                      fra[p_con->fra_pos].dir_alias);
         }

#ifdef LOCK_DEBUG
         lock_region_w(fsa_fd, lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
         lock_region_w(fsa_fd, lock_offset + LOCK_EC);
#endif
         fsa[p_con->fsa_pos].error_counter += 1;
         fsa[p_con->fsa_pos].total_errors += 1;

         /* Check if we need to toggle hosts. */
         if (fsa[p_con->fsa_pos].auto_toggle == ON)
         {
            if ((fsa[p_con->fsa_pos].error_counter == fsa[p_con->fsa_pos].max_errors) &&
                (fsa[p_con->fsa_pos].original_toggle_pos == NONE))
            {
               fsa[p_con->fsa_pos].original_toggle_pos = fsa[p_con->fsa_pos].host_toggle;
            }
            if ((fsa[p_con->fsa_pos].error_counter % fsa[p_con->fsa_pos].max_errors) == 0)
            {
               system_log(INFO_SIGN, NULL, 0,
                          "Automatic host switch initiated for host %s",
                          fsa[p_con->fsa_pos].host_dsp_name);
               if (fsa[p_con->fsa_pos].host_toggle == HOST_ONE)
               {
                  fsa[p_con->fsa_pos].host_toggle = HOST_TWO;
               }
               else
               {
                  fsa[p_con->fsa_pos].host_toggle = HOST_ONE;
               }
               fsa[p_con->fsa_pos].host_dsp_name[(int)fsa[p_con->fsa_pos].toggle_pos] = fsa[p_con->fsa_pos].host_toggle_str[(int)fsa[p_con->fsa_pos].host_toggle];
            }
         }
#ifdef LOCK_DEBUG
         unlock_region(fsa_fd, lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
         unlock_region(fsa_fd, lock_offset + LOCK_EC);
#endif
      }
      else
      {
         if ((faulty != NEITHER) &&
             (fsa[p_con->fsa_pos].error_counter > 0) &&
             (p_con->temp_toggle == OFF))
         {
            int   j;
            off_t lock_offset;

            lock_offset = AFD_WORD_OFFSET +
                          (p_con->fsa_pos * sizeof(struct filetransfer_status));
#ifdef LOCK_DEBUG
            lock_region_w(fsa_fd, lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
            lock_region_w(fsa_fd, lock_offset + LOCK_EC);
#endif
            fsa[p_con->fsa_pos].error_counter = 0;

            /*
             * Remove the error condition (NOT_WORKING) from all jobs
             * of this host.
             */
            for (j = 0; j < fsa[p_con->fsa_pos].allowed_transfers; j++)
            {
               if (fsa[p_con->fsa_pos].job_status[j].connect_status == NOT_WORKING)
               {
                  fsa[p_con->fsa_pos].job_status[j].connect_status = DISCONNECT;
               }
            }
            fsa[p_con->fsa_pos].error_history[0] = 0;
            fsa[p_con->fsa_pos].error_history[1] = 0;
#ifdef LOCK_DEBUG
            unlock_region(fsa_fd, lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
            unlock_region(fsa_fd, lock_offset + LOCK_EC);
#endif
         }
      }

      if (fsa[p_con->fsa_pos].active_transfers > fsa[p_con->fsa_pos].allowed_transfers)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Active transfers > allowed transfers %d!? [%d]",
                    fsa[p_con->fsa_pos].allowed_transfers,
                    fsa[p_con->fsa_pos].active_transfers);
         fsa[p_con->fsa_pos].active_transfers = fsa[p_con->fsa_pos].allowed_transfers;
      }
      fsa[p_con->fsa_pos].active_transfers -= 1;
      if (fsa[p_con->fsa_pos].active_transfers < 0)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Active transfers for FSA position %d < 0!? [%d]",
                    p_con->fsa_pos, fsa[p_con->fsa_pos].active_transfers);
         fsa[p_con->fsa_pos].active_transfers = 0;
      }
      if ((fsa[p_con->fsa_pos].transfer_rate_limit > 0) ||
          (no_of_trl_groups > 0))
      {
         calc_trl_per_process(p_con->fsa_pos);
      }
      fsa[p_con->fsa_pos].job_status[p_con->job_no].proc_id = -1;
#ifdef _WITH_BURST_2
      fsa[p_con->fsa_pos].job_status[p_con->job_no].unique_name[0] = '\0';
      fsa[p_con->fsa_pos].job_status[p_con->job_no].job_id = NO_ID;
#endif
   }

   /* Decrease the number of active transfers. */
   if (p_afd_status->no_of_transfers > 0)
   {
      p_afd_status->no_of_transfers--;
   }
   else
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Huh?! Whats this trying to reduce number of transfers although its zero???");
   }

   /*
    * Reset all values of connection structure.
    */
   p_con->hostname[0] = '\0';
   p_con->host_id = 0;
   p_con->job_no = -1;
   p_con->fsa_pos = -1;
   p_con->fra_pos = -1;
   p_con->msg_name[0] = '\0';
   p_con->pid = 0;

   return;
}
