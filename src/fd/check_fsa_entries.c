/*
 *  check_fsa_entries.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2024 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_fsa_entries - checks all FSA entries if they are still
 **                       correct
 **
 ** SYNOPSIS
 **   void check_fsa_entries(int lock_set)
 **
 ** DESCRIPTION
 **   The function check_fsa_entries() checks the file counter,
 **   file size, active transfers and error counter of all hosts
 **   in the FSA. This check is only performed when there are
 **   currently no message for the checked host in the message
 **   queue qb.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.06.1998 H.Kiehl Created
 **   07.04.2002 H.Kiehl Added some more checks for the job_status struct.
 **   10.05.2004 H.Kiehl Reset the queued flag in FRA when we reset the
 **                      active_transfers to zero.
 **   18.09.2012 H.Kiehl Put in more checks in case the FSA has been
 **                      corrupted.
 **
 */
DESCR__E_M3

#include <string.h>            /* strcmp()                               */
#include "fddefs.h"

/* External global variables. */
extern int                        fsa_fd;
extern int                        no_of_dirs,
                                  no_of_hosts,
                                  *no_msg_queued;
extern struct filetransfer_status *fsa;
extern struct fileretrieve_status *fra;
extern struct queue_buf           *qb;
extern struct msg_cache_buf       *mdb;


/*########################## check_fsa_entries() ########################*/
void
check_fsa_entries(int lock_set)
{
   register int gotcha, i, j;
#ifdef WITH_ERROR_QUEUE
   time_t       now;

   now = time(NULL);
#endif

   if (lock_set != YES)
   {
#ifdef LOCK_DEBUG
      lock_region_w(fsa_fd, LOCK_CHECK_FSA_ENTRIES, __FILE__, __LINE__);
#else
      lock_region_w(fsa_fd, LOCK_CHECK_FSA_ENTRIES);
#endif
   }

   for (i = 0; i < no_of_hosts; i++)
   {
      if (fsa[i].real_hostname[0][0] != GROUP_IDENTIFIER)
      {
#ifdef WITH_ERROR_QUEUE
         if (fsa[i].host_status & ERROR_QUEUE_SET)
         {
            if (host_check_error_queue(fsa[i].host_id, now,
                                       fsa[i].retry_interval) == 0)
            {
# ifdef LOCK_DEBUG
               lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (i * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
# else
               lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (i * sizeof(struct filetransfer_status)) + LOCK_HS));
# endif
               fsa[i].host_status &= ~ERROR_QUEUE_SET;
# ifdef LOCK_DEBUG
               unlock_region(fsa_fd, (AFD_WORD_OFFSET + (i * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
# else
               unlock_region(fsa_fd, (AFD_WORD_OFFSET + (i * sizeof(struct filetransfer_status)) + LOCK_HS));
# endif
            }
         }
#endif
         gotcha = NO;
         for (j = 0; j < *no_msg_queued; j++)
         {
            if (qb[j].special_flag & FETCH_JOB)
            {
               if (fra[qb[j].pos].fsa_pos == i)
               {
                  gotcha = YES;
                  break;
               }
            }
            else
            {
               if (mdb[qb[j].pos].fsa_pos == i)
               {
                  gotcha = YES;
                  break;
               }
            }
         }

         /*
          * If there are currently no messages stored for this host
          * we can check if the values for file size, number of files,
          * number of active transfers and the error counter in the FSA
          * are still correct.
          */
         if (gotcha == NO)
         {
            if (fsa[i].active_transfers != 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Active transfers for host %s is %d. It should be 0. Correcting.",
                          fsa[i].host_dsp_name, fsa[i].active_transfers);
               fsa[i].active_transfers = 0;
               calc_trl_per_process(i);

               /*
                * If active transfers is zero and this is a retrieve job
                * we must reset the queued flag in FRA, otherwise retrieve
                * for this job will never again be possible (unless FD is
                * restarted).
                */
               if (fsa[i].protocol & RETRIEVE_FLAG)
               {
                  for (j = 0; j < no_of_dirs; j++)
                  {
                     if ((fra[j].queued > 0) &&
                         (CHECK_STRCMP(fsa[i].host_alias, fra[j].host_alias) == 0))
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   "Queued flag set for dir_alias %s, but active_transfers is 0. Unsetting queued flag.",
                                   fra[j].dir_alias);
                        fra[j].queued = 0;
                     }
                  }
               }
            }
            if (fsa[i].total_file_counter != 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "File counter for host %s is %d. It should be 0. Correcting.",
                          fsa[i].host_dsp_name, fsa[i].total_file_counter);
               fsa[i].total_file_counter = 0;
            }
            if (fsa[i].total_file_size != 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "File size for host %s is %lu. It should be 0. Correcting.",
                          fsa[i].host_dsp_name, fsa[i].total_file_size);
               fsa[i].total_file_size = 0;
            }
            if (fsa[i].error_counter != 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Error counter for host %s is %d. It should be 0. Correcting.",
                          fsa[i].host_dsp_name, fsa[i].error_counter);
               fsa[i].error_counter = 0;
            }
            if (fsa[i].error_history[0] != 0)
            {
               fsa[i].error_history[0] = 0;
               fsa[i].error_history[1] = 0;
            }
            if (fsa[i].allowed_transfers <= MAX_NO_PARALLEL_JOBS)
            {
               for (j = 0; j < fsa[i].allowed_transfers; j++)
               {
                  if (fsa[i].job_status[j].connect_status != DISCONNECT)
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "Connect status %d for host %s is %d. It should be %d. Correcting.",
                                j, fsa[i].host_dsp_name,
                                fsa[i].job_status[j].connect_status,
                                DISCONNECT);
                     fsa[i].job_status[j].connect_status = DISCONNECT;
                  }
                  if (fsa[i].job_status[j].proc_id != -1)
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "Process ID in job %d for host %s is %d. It should be -1. Correcting.",
                                j, fsa[i].host_dsp_name,
                                fsa[i].job_status[j].proc_id);
                     fsa[i].job_status[j].proc_id = -1;
                  }
#ifdef _WITH_BURST_2
                  if (fsa[i].job_status[j].job_id != NO_ID)
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "Job ID in job %d for host %s is #%x. It should be %d. Correcting.",
                                j, fsa[i].host_dsp_name,
                                fsa[i].job_status[j].job_id, NO_ID);
                     fsa[i].job_status[j].job_id = NO_ID;
                  }
#endif
               }
            }
            else
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "The maximum number of allowed transfers for %s is to large (%d)!",
                          fsa[i].host_dsp_name, fsa[i].allowed_transfers);
               for (j = 0; j < MAX_NO_PARALLEL_JOBS; j++)
               {
                  fsa[i].job_status[j].connect_status = DISCONNECT;
                  fsa[i].job_status[j].proc_id = -1;
#ifdef _WITH_BURST_2
                  fsa[i].job_status[j].job_id = NO_ID;
#endif
               }
            }
         } /* if (gotcha == NO) */
      } /* if (fsa[i].real_hostname[0][0] != GROUP_IDENTIFIER) */
   } /* for (i = 0; i < no_of_hosts; i++) */

#ifdef LOCK_DEBUG
   unlock_region(fsa_fd, LOCK_CHECK_FSA_ENTRIES, __FILE__, __LINE__);
#else
   unlock_region(fsa_fd, LOCK_CHECK_FSA_ENTRIES);
#endif

   return;
}
