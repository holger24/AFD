/*
 *  unset_error_counter.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2021 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   unset_error_counter_fsa - set error counter to 0 in FSA
 **   unset_error_counter_fra - set error counter to 0 in FRA
 **
 ** SYNOPSIS
 **   void unset_error_counter_fsa(void)
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
 **   13.04.2021 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>             /* snprintf()                             */
#include <string.h>            /* strerror()                             */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>             /* open()                                 */
#include <time.h>              /* time()                                 */
#include <unistd.h>            /* close()                                */
#include <errno.h>
#include "fddefs.h"


/*#################### unset_error_counter_fsa() ########################*/
void
unset_error_counter_fsa(int                        fsa_fd,
                        int                        transfer_log_fd,
                        char                       *p_work_dir,
                        struct filetransfer_status *fsa,
                        struct job                 *p_db)
{
   if ((p_db->fsa_pos != INCORRECT) &&
       (fsa != NULL) && (fsa->error_counter > 0))
   {
      int  fd, j;
#ifdef WITHOUT_FIFO_RW_SUPPORT
      int  readfd;
#endif
      char fd_wake_up_fifo[MAX_PATH_LENGTH];

#ifdef LOCK_DEBUG
      lock_region_w(fsa_fd, p_db->lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
      lock_region_w(fsa_fd, p_db->lock_offset + LOCK_EC);
#endif
      fsa->error_counter = 0;

      /* Wake up FD! */
      (void)snprintf(fd_wake_up_fifo, MAX_PATH_LENGTH,
                     "%s%s%s", p_work_dir, FIFO_DIR, FD_WAKE_UP_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(fd_wake_up_fifo, &readfd, &fd) == -1)
#else
      if ((fd = open(fd_wake_up_fifo, O_RDWR)) == -1)
#endif
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to open() FIFO `%s' : %s",
                    fd_wake_up_fifo, strerror(errno));
      }
      else
      {
         if (write(fd, "", 1) != 1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to write() to FIFO `%s' : %s",
                       fd_wake_up_fifo, strerror(errno));
         }
#ifdef WITHOUT_FIFO_RW_SUPPORT
         if (close(readfd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Failed to close() FIFO `%s' (read) : %s",
                       fd_wake_up_fifo, strerror(errno));
         }
#endif
         if (close(fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Failed to close() FIFO `%s' : %s",
                       fd_wake_up_fifo, strerror(errno));
         }
      }

      /*
       * Remove the error condition (NOT_WORKING) from all jobs
       * of this host.
       */
      for (j = 0; j < fsa->allowed_transfers; j++)
      {
         if ((j != p_db->job_no) &&
             (fsa->job_status[j].connect_status == NOT_WORKING))
         {
            fsa->job_status[j].connect_status = DISCONNECT;
         }
      }
      fsa->error_history[0] = 0;
      fsa->error_history[1] = 0;
#ifdef LOCK_DEBUG
      unlock_region(fsa_fd, p_db->lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
      unlock_region(fsa_fd, p_db->lock_offset + LOCK_EC);
#endif

#ifdef LOCK_DEBUG
      lock_region_w(fsa_fd, p_db->lock_offset + LOCK_HS, __FILE__, __LINE__);
#else
      lock_region_w(fsa_fd, p_db->lock_offset + LOCK_HS);
#endif
      if (time(NULL) > fsa->end_event_handle)
      {
         fsa->host_status &= ~(EVENT_STATUS_FLAGS | AUTO_PAUSE_QUEUE_STAT);
         if (fsa->end_event_handle > 0L)
         {
            fsa->end_event_handle = 0L;
         }
         if (fsa->start_event_handle > 0L)
         {
            fsa->start_event_handle = 0L;
         }
      }
      else
      {
         fsa->host_status &= ~(EVENT_STATUS_STATIC_FLAGS | AUTO_PAUSE_QUEUE_STAT);
      }
#ifdef LOCK_DEBUG
      unlock_region(fsa_fd, p_db->lock_offset + LOCK_HS, __FILE__, __LINE__);
#else
      unlock_region(fsa_fd, p_db->lock_offset + LOCK_HS);
#endif

      /*
       * Since we have successfully retrieved a file, no
       * need to have the queue stopped anymore.
       */
      if (fsa->host_status & AUTO_PAUSE_QUEUE_STAT)
      {
         char sign[LOG_SIGN_LENGTH];

         error_action(fsa->host_alias, "stop", HOST_ERROR_ACTION,
                      transfer_log_fd);
         event_log(0L, EC_HOST, ET_EXT, EA_ERROR_END, "%s",
                   fsa->host_alias);
         if ((fsa->host_status & HOST_ERROR_OFFLINE_STATIC) ||
             (fsa->host_status & HOST_ERROR_OFFLINE) ||
             (fsa->host_status & HOST_ERROR_OFFLINE_T))
         {
            (void)memcpy(sign, OFFLINE_SIGN, LOG_SIGN_LENGTH);
         }
         else
         {
            (void)memcpy(sign, INFO_SIGN, LOG_SIGN_LENGTH);
         }
         trans_log(sign, __FILE__, __LINE__, NULL, NULL,
                   "Starting input queue that was stopped by init_afd.");
         event_log(0L, EC_HOST, ET_AUTO, EA_START_QUEUE, "%s", fsa->host_alias);
      }
   }

   return;
}


/*#################### unset_error_counter_fra() ########################*/
void
unset_error_counter_fra(int                        fra_fd,
                        char                       *p_work_dir,
                        struct fileretrieve_status *fra,
                        struct job                 *p_db)
{
   if ((p_db->fra_pos != INCORRECT) &&
       (fra != NULL) && (fra->error_counter > 0))
   {
#ifdef LOCK_DEBUG
      lock_region_w(fra_fd,
                    p_db->fra_lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
      lock_region_w(fra_fd,
                    p_db->fra_lock_offset + LOCK_EC);
#endif
      fra->error_counter = 0;
      if (fra->dir_flag & DIR_ERROR_SET)
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
                   ((receive_log_fd = open(receive_log_fifo, O_RDWR)) == -1))
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

         fra->dir_flag &= ~DIR_ERROR_SET;
         SET_DIR_STATUS(fra->dir_flag, time(NULL), fra->start_event_handle,
                        fra->end_event_handle, fra->dir_status);
         error_action(fra->dir_alias, "stop", DIR_ERROR_ACTION, receive_log_fd);
         event_log(0L, EC_DIR, ET_EXT, EA_ERROR_END, "%s", fra->dir_alias);
         (void)close(receive_log_fd);
#ifdef WITHOUT_FIFO_RW_SUPPORT
         (void)close(receive_log_readfd);
#endif
      }
#ifdef LOCK_DEBUG
      unlock_region(fra_fd,
                    p_db->fra_lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
      unlock_region(fra_fd, p_db->fra_lock_offset + LOCK_EC);
#endif
   }

   return;
}
