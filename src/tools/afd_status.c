/*
 *  afd_status.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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

DESCR__S_M1
/*
 ** NAME
 **   afd_status - shows status of AFD by printing contents of
 **                afd_status structure
 **
 ** SYNOPSIS
 **   afd_status [-w <working directory>] [--reset_log_to_info]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.10.1998 H.Kiehl Created
 **   29.03.2022 H.Kiehl Added process afdds.
 **   24.01.2023 H.Kiehl Added parameter --reset_log_to_info.
 **
 */

#include <stdio.h>
#include <string.h>                   /* strcmp() in CHECK_FOR_VERSION   */
#include <stdlib.h>                   /* exit()                          */
#include <sys/types.h>
#include <pwd.h>                      /* getpwuid()                      */
#include <time.h>                     /* ctime()                         */
#include <unistd.h>                   /* STDERR_FILENO                   */
#include "version.h"

/* Global variables. */
int               sys_log_fd = STDERR_FILENO;
char              *p_work_dir;
struct afd_status *p_afd_status;
const char        *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void       usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ afd_status() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int           i,
                 reset_log_to_info;
   char          work_dir[MAX_PATH_LENGTH];
   struct passwd *pwd;

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if ((get_arg(&argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      usage(argv[0]);
      exit(SUCCESS);
   }
   if (get_arg(&argc, argv, "--reset_log_to_info", NULL, 0) == SUCCESS)
   {
      reset_log_to_info = YES;
   }
   else
   {
      reset_log_to_info = NO;
   }

   /* Attach to the AFD Status Area. */
   if (attach_afd_status(NULL, WAIT_AFD_STATUS_ATTACH) < 0)
   {
      (void)fprintf(stderr,
                    _("ERROR   : Failed to map to AFD status area. (%s %d)\n"),
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   (void)fprintf(stdout, "Hostname              : %s\n", p_afd_status->hostname);
   (void)fprintf(stdout, "Working directory     : %s\n", p_afd_status->work_dir);
   if ((pwd = getpwuid(p_afd_status->user_id)) == NULL)
   {
#if SIZEOF_UID_T == 4
      (void)fprintf(stdout, "User ID               : %d\n", p_afd_status->user_id);
#else
      (void)fprintf(stdout, "User ID               : %lld\n", (pri_uid_t)p_afd_status->user_id);
#endif
   }
   else
   {
#if SIZEOF_UID_T == 4
      (void)fprintf(stdout, "User name + ID        : %s (%d)\n", pwd->pw_name, p_afd_status->user_id);
#else
      (void)fprintf(stdout, "User name + ID        : %s (%lld)\n", pwd->pw_name, (pri_uid_t)p_afd_status->user_id);
#endif
   }
   (void)fprintf(stdout, "AMG                   : %d\n", p_afd_status->amg);
   (void)fprintf(stdout, "AMG jobs status flag  : %d\n", p_afd_status->amg_jobs);
   (void)fprintf(stdout, "FD                    : %d\n", p_afd_status->fd);
   (void)fprintf(stdout, "System log            : %d\n", p_afd_status->sys_log);
#ifdef _MAINTAINER_LOG
   (void)fprintf(stdout, "Maintainer log        : %d\n", p_afd_status->maintainer_log);
#endif
   (void)fprintf(stdout, "Event log             : %d\n", p_afd_status->event_log);
   (void)fprintf(stdout, "Transfer log          : %d\n", p_afd_status->trans_log);
   (void)fprintf(stdout, "Trans debug log       : %d\n", p_afd_status->trans_db_log);
   (void)fprintf(stdout, "Archive watch         : %d\n", p_afd_status->archive_watch);
   (void)fprintf(stdout, "afd_stat              : %d\n", p_afd_status->afd_stat);
   (void)fprintf(stdout, "afdd                  : %d\n", p_afd_status->afdd);
   (void)fprintf(stdout, "afdds                 : %d\n", p_afd_status->afdds);
#ifdef _WITH_ATPD_SUPPORT
   (void)fprintf(stdout, "atpd                  : %d\n", p_afd_status->atpd);
#endif
#ifdef _WITH_WMOD_SUPPORT
   (void)fprintf(stdout, "wmod                  : %d\n", p_afd_status->wmod);
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
   (void)fprintf(stdout, "demcd                 : %d\n", p_afd_status->demcd);
#endif
#ifndef HAVE_MMAP
   (void)fprintf(stdout, "mapper                : %d\n", p_afd_status->mapper);
#endif
#ifdef _INPUT_LOG
   (void)fprintf(stdout, "input_log             : %d\n", p_afd_status->input_log);
#endif
#ifdef _OUTPUT_LOG
   (void)fprintf(stdout, "output_log            : %d\n", p_afd_status->output_log);
#endif
#ifdef _DELETE_LOG
   (void)fprintf(stdout, "delete_log            : %d\n", p_afd_status->delete_log);
#endif
#ifdef _PRODUCTION_LOG
   (void)fprintf(stdout, "production_log        : %d\n", p_afd_status->production_log);
#endif
#ifdef _DISTRIBUTION_LOG
   (void)fprintf(stdout, "distribution_log      : %d\n", p_afd_status->distribution_log);
#endif
#if ALDAD_OFFSET != 0
   (void)fprintf(stdout, "ALDA daemon           : %d\n", p_afd_status->aldad);
#endif
   if (reset_log_to_info == YES)
   {
      for (i = 0; i < LOG_FIFO_SIZE; i++)
      {
         p_afd_status->receive_log_fifo[i] = INFO_ID;
         p_afd_status->sys_log_fifo[i] = INFO_ID;
         p_afd_status->trans_log_fifo[i] = INFO_ID;
      }
      for (i = 0; i < MAX_LOG_HISTORY; i++)
      {
         p_afd_status->receive_log_history[i] = INFO_ID;
         p_afd_status->sys_log_history[i] = INFO_ID;
         p_afd_status->trans_log_history[i] = INFO_ID;
      }
   }
   (void)fprintf(stdout, "Receivelog indicator  : %u <",
                 p_afd_status->receive_log_ec);
   for (i = 0; i < LOG_FIFO_SIZE; i++)
   {
      switch (p_afd_status->receive_log_fifo[i])
      {
         case INFO_ID :
            (void)fprintf(stdout, " I");
            break;
         case ERROR_ID :
            (void)fprintf(stdout, " E");
            break;
         case WARNING_ID :
            (void)fprintf(stdout, " W");
            break;
         case FAULTY_ID :
            (void)fprintf(stdout, " F");
            break;
         default :
            (void)fprintf(stdout, " ?");
            break;
      }
   }
   (void)fprintf(stdout, " >\n");
   (void)fprintf(stdout, "Receive log history   :");
   for (i = 0; i < MAX_LOG_HISTORY; i++)
   {
      switch (p_afd_status->receive_log_history[i])
      {
         case INFO_ID :
            (void)fprintf(stdout, " I");
            break;
         case ERROR_ID :
            (void)fprintf(stdout, " E");
            break;
         case WARNING_ID :
            (void)fprintf(stdout, " W");
            break;
         case FAULTY_ID :
            (void)fprintf(stdout, " F");
            break;
         default :
            (void)fprintf(stdout, " ?");
            break;
      }
   }
   (void)fprintf(stdout, "\n");
   (void)fprintf(stdout, "Syslog indicator      : %u <",
                 p_afd_status->sys_log_ec);
   for (i = 0; i < LOG_FIFO_SIZE; i++)
   {
      switch (p_afd_status->sys_log_fifo[i])
      {
         case INFO_ID :
            (void)fprintf(stdout, " I");
            break;
         case ERROR_ID :
            (void)fprintf(stdout, " E");
            break;
         case WARNING_ID :
            (void)fprintf(stdout, " W");
            break;
         case CONFIG_ID :
            (void)fprintf(stdout, " C");
            break;
         case FAULTY_ID :
            (void)fprintf(stdout, " F");
            break;
         default :
            (void)fprintf(stdout, " ?");
            break;
      }
   }
   (void)fprintf(stdout, " >\n");
   (void)fprintf(stdout, "System log history    :");
   for (i = 0; i < MAX_LOG_HISTORY; i++)
   {
      switch (p_afd_status->sys_log_history[i])
      {
         case INFO_ID :
            (void)fprintf(stdout, " I");
            break;
         case ERROR_ID :
            (void)fprintf(stdout, " E");
            break;
         case WARNING_ID :
            (void)fprintf(stdout, " W");
            break;
         case CONFIG_ID :
            (void)fprintf(stdout, " C");
            break;
         case FAULTY_ID :
            (void)fprintf(stdout, " F");
            break;
         default :
            (void)fprintf(stdout, " ?");
            break;
      }
   }
   (void)fprintf(stdout, "\n");
   (void)fprintf(stdout, "Translog indicator    : %u <",
                 p_afd_status->trans_log_ec);
   for (i = 0; i < LOG_FIFO_SIZE; i++)
   {
      switch (p_afd_status->trans_log_fifo[i])
      {
         case INFO_ID :
            (void)fprintf(stdout, " I");
            break;
         case ERROR_ID :
            (void)fprintf(stdout, " E");
            break;
         case WARNING_ID :
            (void)fprintf(stdout, " W");
            break;
         case ERROR_OFFLINE_ID :
            (void)fprintf(stdout, " O");
            break;
         case FAULTY_ID :
            (void)fprintf(stdout, " F");
            break;
         default :
            (void)fprintf(stdout, " ?");
            break;
      }
   }
   (void)fprintf(stdout, " >\n");
   (void)fprintf(stdout, "Transfer log history  :");
   for (i = 0; i < MAX_LOG_HISTORY; i++)
   {
      switch (p_afd_status->trans_log_history[i])
      {
         case INFO_ID :
            (void)fprintf(stdout, " I");
            break;
         case ERROR_ID :
            (void)fprintf(stdout, " E");
            break;
         case WARNING_ID :
            (void)fprintf(stdout, " W");
            break;
         case ERROR_OFFLINE_ID :
            (void)fprintf(stdout, " O");
            break;
         case FAULTY_ID :
            (void)fprintf(stdout, " F");
            break;
         default :
            (void)fprintf(stdout, " ?");
            break;
      }
   }
   (void)fprintf(stdout, "\n");
   (void)fprintf(stdout, "Number of transfers   : %d\n", p_afd_status->no_of_transfers);
   (void)fprintf(stdout, "Number of retrieves   : %d\n", p_afd_status->no_of_retrieves);
#if SIZEOF_NLINK_T > 4
   (void)fprintf(stdout, "Jobs in queue         : %lld\n", (pri_nlink_t)p_afd_status->jobs_in_queue);
#else
   (void)fprintf(stdout, "Jobs in queue         : %d\n", (pri_nlink_t)p_afd_status->jobs_in_queue);
#endif
   (void)fprintf(stdout, "AMG fork() counter    : %u\n", p_afd_status->amg_fork_counter);
   (void)fprintf(stdout, "FD fork() counter     : %u\n", p_afd_status->fd_fork_counter);
   (void)fprintf(stdout, "Burst2 counter        : %u\n", p_afd_status->burst2_counter);
   (void)fprintf(stdout, "AMG child user time   : %ld.%ld\n", p_afd_status->amg_child_utime.tv_sec, p_afd_status->amg_child_utime.tv_usec);
   (void)fprintf(stdout, "AMG child system time : %ld.%ld\n", p_afd_status->amg_child_stime.tv_sec, p_afd_status->amg_child_stime.tv_usec);
   (void)fprintf(stdout, "FD child user time    : %ld.%ld\n", p_afd_status->fd_child_utime.tv_sec, p_afd_status->fd_child_utime.tv_usec);
   (void)fprintf(stdout, "FD child system time  : %ld.%ld\n", p_afd_status->fd_child_stime.tv_sec, p_afd_status->fd_child_stime.tv_usec);
   (void)fprintf(stdout, "Max. FD queue length  : %u\n", p_afd_status->max_queue_length);
   (void)fprintf(stdout, "Directories scanned   : %u\n", p_afd_status->dir_scans);
#ifdef WITH_INOTIFY
   (void)fprintf(stdout, "Inotify events handled: %u\n", p_afd_status->inotify_events);
#endif
   (void)fprintf(stdout, "AFD start time        : %s", ctime(&p_afd_status->start_time));

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{
   (void)fprintf(stderr,
                 _("SYNTAX  : %s [-w <working directory>] [--reset_log_to_info]\n"),
                 progname);
   return;
}
