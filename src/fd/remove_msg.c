/*
 *  remove_msg.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2021 Deutscher Wetterdienst (DWD),
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
 **   void remove_msg(int qb_pos, int remove_only)
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include "fddefs.h"

/* External global variables. */
extern int                        fra_fd,
                                  no_of_dirs,
                                  *no_msg_queued;
extern char                       *p_work_dir;
extern struct queue_buf           *qb;
extern struct fileretrieve_status *fra;


/*############################ remove_msg() #############################*/
void
#if defined (_RMQUEUE_) && defined (_MAINTAINER_LOG)
remove_msg(int qb_pos, int remove_only, char *file, int line)
#else
remove_msg(int qb_pos, int remove_only)
#endif
{
   if ((fra != NULL) && (qb[qb_pos].special_flag & FETCH_JOB) &&
       (qb[qb_pos].pos < no_of_dirs))
   {
      /* Dequeue in FRA. */
      fra[qb[qb_pos].pos].queued -= 1;

      if (fra[qb[qb_pos].pos].queued < 0)
      {
         fra[qb[qb_pos].pos].queued = 0;
      }

      if (remove_only != YES)
      {
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
               int  receive_log_fd = -1;
#ifdef WITHOUT_FIFO_RW_SUPPORT
               int  receive_log_readfd;
#endif
               char receive_log_fifo[MAX_PATH_LENGTH];

               (void)strcpy(receive_log_fifo, p_work_dir);
               (void)strcat(receive_log_fifo, FIFO_DIR);
               (void)strcat(receive_log_fifo, RECEIVE_LOG_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
               if (open_fifo_rw(receive_log_fifo, &receive_log_readfd,
                                &receive_log_fd) == -1)
#else
               if ((receive_log_fd = open(receive_log_fifo, O_RDWR)) == -1)
#endif
               {
                  if (errno == ENOENT)
                  {
                     if ((make_fifo(receive_log_fifo) == SUCCESS) &&
#ifdef WITHOUT_FIFO_RW_SUPPORT
                         (open_fifo_rw(receive_log_fifo, &receive_log_readfd,
                                       &receive_log_fd) == -1))
#else
                         ((receive_log_fd = open(receive_log_fifo,
                                                 O_RDWR)) == -1))
#endif
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "Could not open fifo <%s> : %s",
                                   RECEIVE_LOG_FIFO, strerror(errno));
                     }
                  }
                  else
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Could not open fifo %s : %s",
                                RECEIVE_LOG_FIFO, strerror(errno));
                  }
               }

               fra[qb[qb_pos].pos].dir_flag &= ~DIR_ERROR_SET;
               SET_DIR_STATUS(fra[qb[qb_pos].pos].dir_flag,
                              time(NULL),
                              fra[qb[qb_pos].pos].start_event_handle,
                              fra[qb[qb_pos].pos].end_event_handle,
                              fra[qb[qb_pos].pos].dir_status);
               error_action(fra[qb[qb_pos].pos].dir_alias, "start",
                            DIR_ERROR_ACTION, receive_log_fd);
               event_log(0L, EC_DIR, ET_EXT, EA_ERROR_START, "%s",
                         fra[qb[qb_pos].pos].dir_alias);
               (void)close(receive_log_fd);
#ifdef WITHOUT_FIFO_RW_SUPPORT
               (void)close(receive_log_readfd);
#endif
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
#ifdef WITH_TIMEZONE
                                                                       fra[qb[qb_pos].pos].timezone,
#endif
                                                                       time(NULL),
                                                                       __FILE__, __LINE__);
         }
      }
   }
   if (*no_msg_queued > 0)
   {
#if defined (_RMQUEUE_) && defined (_MAINTAINER_LOG)
      if ((qb[qb_pos].special_flag & FETCH_JOB) == 0)
      {
         maintainer_log(DEBUG_SIGN, NULL, 0,
# if SIZEOF_OFF_T == 4
                        "del msg: %d %s %u %ld %d %u %d %d %d %.0f [%d] %s:%d",
# else
                        "del msg: %d %s %u %lld %d %u %d %d %d %.0f [%d] %s:%d",
# endif
                        qb_pos, qb[qb_pos].msg_name, qb[qb_pos].files_to_send,
                        qb[qb_pos].file_size_to_send,
                        (int)qb[qb_pos].msg_name[MAX_MSG_NAME_LENGTH - 1],
                        qb[qb_pos].retries, qb[qb_pos].pos,
                        qb[qb_pos].connect_pos, (int)qb[qb_pos].special_flag,
                        qb[qb_pos].msg_number, *no_msg_queued, file, line);
      }
#endif
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
