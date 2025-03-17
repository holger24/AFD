/*
 *  get_afd_status_struct_size.c - Part of AFD, an automatic file distribution
 *                                 program.
 *  Copyright (c) 2017 - 2025 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_afd_status_struct_size - get the structure size of struct
 **                                afd_status
 **
 ** SYNOPSIS
 **   unsigned int get_afd_status_struct_size(void)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   Returns the size of struct afd_status without padding.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.01.2017 H.Kiehl Created
 **
 */
DESCR__E_M3


/*#################### get_afd_status_struct_size() #####################*/
unsigned int
get_afd_status_struct_size(void)
{
   return(sizeof(signed char) +       /* amg               */
          sizeof(unsigned char) +     /* amg_jobs          */
          sizeof(signed char) +       /* fd                */
          sizeof(signed char) +       /* sys_log           */
          sizeof(signed char) +       /* maintainer_log    */
          sizeof(signed char) +       /* event_log         */
          sizeof(signed char) +       /* receive_log       */
          sizeof(signed char) +       /* trans_log         */
          sizeof(signed char) +       /* trans_db_log      */
          sizeof(signed char) +       /* archive_watch     */
          sizeof(signed char) +       /* afd_stat          */
          sizeof(signed char) +       /* afdd              */
#ifdef WITH_SSL
          sizeof(signed char) +       /* afdds             */
#endif
#ifdef _WITH_ATPD_SUPPORT
          sizeof(signed char) +       /* atpd              */
#endif
#ifdef _WITH_WMOD_SUPPORT
          sizeof(signed char) +       /* wmod              */
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
          sizeof(signed char) +       /* demcd             */
#endif
#ifndef HAVE_MMAP
          sizeof(signed char) +       /* mapper            */
#endif
#ifdef _INPUT_LOG
          sizeof(signed char) +       /* input_log         */
#endif
#ifdef _DISTRIBUTION_LOG
          sizeof(signed char) +       /* distribution_log  */
#endif
#ifdef _OUTPUT_LOG
          sizeof(signed char) +       /* output_log        */
#endif
#ifdef _CONFIRMATION_LOG
          sizeof(signed char) +       /* confirmation_log  */
#endif
#ifdef _DELETE_LOG
          sizeof(signed char) +       /* delete_log        */
#endif
#ifdef _PRODUCTION_LOG
          sizeof(signed char) +       /* production_log    */
#endif
#ifdef _TRANSFER_RATE_LOG
          sizeof(signed char) +       /* transfer_rate_log */
#endif
#if defined (_INPUT_LOG) || defined (_OUTPUT_LOG) || defined (_CONFIRMATION_LOG) || defined (_DELETE_LOG) || defined (_PRODUCTION_LOG) || defined (_DISTRIBUTION_LOG)
          sizeof(signed char) +       /* aldad             */
#endif
          sizeof(signed char) +       /* afd_worker        */
          sizeof(unsigned int) +                 /* sys_log_ec        */
          ((LOG_FIFO_SIZE + 1) * sizeof(char)) + /* sys_log_fifo[LOG_FIFO_SIZE + 1]  */
          (MAX_LOG_HISTORY * sizeof(char)) +     /* sys_log_history[MAX_LOG_HISTORY] */
          sizeof(unsigned int) +                 /* receive_log_ec    */
          ((LOG_FIFO_SIZE + 1) * sizeof(char)) + /* receive_log_fifo[LOG_FIFO_SIZE + 1]  */
          (MAX_LOG_HISTORY * sizeof(char)) +     /* receive_log_history[MAX_LOG_HISTORY] */
          sizeof(unsigned int) +                 /* trans_log_ec      */
          ((LOG_FIFO_SIZE + 1) * sizeof(char)) + /* trans_log_fifo[LOG_FIFO_SIZE + 1]  */
          (MAX_LOG_HISTORY * sizeof(char)) +     /* trans_log_history[MAX_LOG_HISTORY] */
          (MAX_REAL_HOSTNAME_LENGTH * sizeof(char)) + /* hostname[MAX_REAL_HOSTNAME_LENGTH] */
          (MAX_PATH_LENGTH * sizeof(char)) +          /* work_dir[MAX_PATH_LENGTH]          */
          sizeof(uid_t) +             /* user_id           */
          sizeof(int) +               /* no_of_transfers   */
          sizeof(int) +               /* no_of_retrieves   */
          sizeof(nlink_t) +           /* jobs_in_queue     */
          sizeof(time_t) +            /* start_time        */
          sizeof(unsigned int) +      /* fd_fork_counter   */
          sizeof(unsigned int) +      /* amg_fork_counter  */
          sizeof(unsigned int) +      /* burst2_counter    */
          sizeof(unsigned int) +      /* max_queue_length  */
          sizeof(unsigned int) +      /* dir_scans         */
#ifdef WITH_INOTIFY
          sizeof(unsigned int) +      /* inotify_events    */
#endif
#ifdef HAVE_WAIT4
          sizeof(struct timeval) +    /* amg_child_utime   */
          sizeof(struct timeval) +    /* amg_child_stime   */
          sizeof(struct timeval) +    /* fd_child_utime    */
          sizeof(struct timeval) +    /* fd_child_stime    */
#endif
          0);
}
