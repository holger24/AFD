/*
 *  remove_msg.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2015 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   remove_msg - removes a message from the internal queue of the FD
 **
 ** SYNOPSIS
 **   void remove_msg(int qb_pos)
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
 **   27.07.1998 H.Kiehl Created
 **   11.08.2000 H.Kiehl Support for retrieving files.
 **   29.09.2006 H.Kiehl Added error_counter handling for FRA.
 **
 */
DESCR__E_M3

#include <string.h>
#include <time.h>
#include "fddefs.h"

/* External global variables. */
extern int                        fra_fd,
                                  no_of_dirs,
                                  *no_msg_queued;
extern struct queue_buf           *qb;
extern struct fileretrieve_status *fra;


/*############################ remove_msg() #############################*/
void
remove_msg(int qb_pos)
{
   if ((fra != NULL) && (qb[qb_pos].special_flag & FETCH_JOB) &&
       (qb[qb_pos].pos < no_of_dirs))
   {
      /* Dequeue in FRA. */
      fra[qb[qb_pos].pos].queued -= 1;

      if (fra[qb[qb_pos].pos].queued < 1)
      {
         if (fra[qb[qb_pos].pos].queued < 0)
         {
            fra[qb[qb_pos].pos].queued = 0;
         }
      }

      if (fra[qb[qb_pos].pos].error_counter > 0)
      {
         lock_region_w(fra_fd,
#ifdef LOCK_DEBUG
                       (char *)&fra[qb[qb_pos].pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                       (char *)&fra[qb[qb_pos].pos].error_counter - (char *)fra);
#endif
         fra[qb[qb_pos].pos].error_counter = 0;
         if (fra[qb[qb_pos].pos].dir_flag & DIR_ERROR_SET)
         {
            fra[qb[qb_pos].pos].dir_flag &= ~DIR_ERROR_SET;
            SET_DIR_STATUS(fra[qb[qb_pos].pos].dir_flag,
                           time(NULL),
                           fra[qb[qb_pos].pos].start_event_handle,
                           fra[qb[qb_pos].pos].end_event_handle,
                           fra[qb[qb_pos].pos].dir_status);
            error_action(fra[qb[qb_pos].pos].dir_alias, "start",
                         DIR_ERROR_ACTION);
            event_log(0L, EC_DIR, ET_EXT, EA_ERROR_START, "%s",
                      fra[qb[qb_pos].pos].dir_alias);
         }
         unlock_region(fra_fd,
#ifdef LOCK_DEBUG
                       (char *)&fra[qb[qb_pos].pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                       (char *)&fra[qb[qb_pos].pos].error_counter - (char *)fra);
#endif
      }

      /* Calculate the next scan time. */
      if (fra[qb[qb_pos].pos].no_of_time_entries > 0)
      {
         fra[qb[qb_pos].pos].next_check_time = calc_next_time_array(fra[qb[qb_pos].pos].no_of_time_entries,
                                                                    &fra[qb[qb_pos].pos].te[0],
                                                                    time(NULL),
                                                                    __FILE__, __LINE__);
      }
   }
   if (*no_msg_queued > 0)
   {
      if (qb_pos <= (*no_msg_queued - 1))
      {
         (void)memmove(&qb[qb_pos], &qb[qb_pos + 1],
                       ((*no_msg_queued - 1 - qb_pos) * sizeof(struct queue_buf)));
      }
      (*no_msg_queued)--;
   }
   else
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Hmm, number of messages queued is %d!", *no_msg_queued);
      if (*no_msg_queued < 0)
      {
         *no_msg_queued = 0;
      }
   }

   return;
}
