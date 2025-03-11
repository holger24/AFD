/*
 *  check_changes.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2025 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_changes - 
 **
 ** SYNOPSIS
 **   void check_changes(SSL *ssl)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.01.1999 H.Kiehl Created
 **   03.09.2000 H.Kiehl Addition of log history.
 **   26.06.2004 H.Kiehl Added error history for each host.
 **   31.03.2017 H.Kiehl Do not count the data from group identifiers.
 **
 */
DESCR__E_M3

#include <stdio.h>           /* fprintf()                                */
#include <string.h>          /* strerror()                               */
#include <time.h>            /* time()                                   */
#include <stdlib.h>          /* atoi()                                   */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>          /* F_OK                                     */
#include <errno.h>
#include "afddsdefs.h"

/* External global variables. */
extern int                        host_config_counter,
                                  log_defs,
                                  no_of_hosts;
extern pid_t                      log_pid;
extern char                       afd_config_file[];
extern unsigned char              **old_error_history;
extern struct afd_status          *p_afd_status;
extern struct filetransfer_status *fsa;


/*############################ check_changes() ##########################*/
void
check_changes(SSL *ssl)
{
   static int          old_amg_status = PROC_INIT_VALUE,
                       old_archive_watch_status = PROC_INIT_VALUE,
                       old_fd_status = PROC_INIT_VALUE,
                       old_max_connections;
   static unsigned int old_sys_log_ec;
   static time_t       next_stat_time,
                       old_st_mtime;
   static char         old_receive_log_history[MAX_LOG_HISTORY],
                       old_sys_log_history[MAX_LOG_HISTORY],
                       old_trans_log_history[MAX_LOG_HISTORY];
   register int        i;
   time_t              now;

   if (check_fsa(YES, AFDDS) == YES)
   {
      int loop_counter = 0,
          status;

retry_check:
      if (old_error_history != NULL)
      {
         FREE_RT_ARRAY(old_error_history);
         old_error_history = NULL;
      }

      if (check_fsa(YES, AFDDS) == YES)
      {
         loop_counter++;
         if (loop_counter < 10)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("Hmm, FSA has changed again!"));
            my_usleep(500000L);
            goto retry_check;
         }
      }

      status = 0;
      while (p_afd_status->amg_jobs & WRITTING_JID_STRUCT)
      {
         (void)my_usleep(100000L);
         status++;
         if ((status > 1) && ((status % 100) == 0))
         {
            system_log(INFO_SIGN, __FILE__, __LINE__,
                       _("AFDDS: Timeout arrived for waiting for AMG to finish writting to JID structure."));
         }
      }

      RT_ARRAY(old_error_history, no_of_hosts, ERROR_HISTORY_LENGTH,
               unsigned char);
      for (i = 0; i < no_of_hosts; i++)
      {
         if (fsa[i].real_hostname[0][0] == GROUP_IDENTIFIER)
         {
            (void)memset(old_error_history[i], 0, ERROR_HISTORY_LENGTH);
         }
         else
         {
            (void)memcpy(old_error_history[i], fsa[i].error_history,
                         ERROR_HISTORY_LENGTH);
         }
      }
      host_config_counter = (int)*(unsigned char *)((char *)fsa - AFD_WORD_OFFSET + SIZEOF_INT);
      show_host_list(ssl);
      show_job_list(ssl);
   }
   else
   {
      if (fsa == NULL)
      {
         if (fsa_attach_passive(NO, AFDDS) != SUCCESS)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__, _("Failed to attach to FSA."));
            exit(INCORRECT);
         }
      }
      if (host_config_counter != (int)*(unsigned char *)((char *)fsa - AFD_WORD_OFFSET + SIZEOF_INT))
      {
         FREE_RT_ARRAY(old_error_history);
         RT_ARRAY(old_error_history, no_of_hosts, ERROR_HISTORY_LENGTH,
                  unsigned char);
         for (i = 0; i < no_of_hosts; i++)
         {
            if (fsa[i].real_hostname[0][0] == GROUP_IDENTIFIER)
            {
               (void)memset(old_error_history[i], 0, ERROR_HISTORY_LENGTH);
            }
            else
            {
               (void)memcpy(old_error_history[i], fsa[i].error_history,
                            ERROR_HISTORY_LENGTH);
            }
         }
         host_config_counter = (int)*(unsigned char *)((char *)fsa - AFD_WORD_OFFSET + SIZEOF_INT);
         show_host_list(ssl);
      }
   }
   if (check_fra(YES) == YES)
   {
      show_dir_list(ssl);
   }

   /*
    * It costs too much system performance to constantly stat()
    * the AFD_CONFIG file to see if the modification time has
    * changed. For this reason lets only stat() this file at a
    * reasonable interval of say STAT_INTERVAL seconds.
    */
   now = time(NULL);
   if (next_stat_time < now)
   {
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat stat_buf;
#endif

      next_stat_time = now + STAT_INTERVAL;
#ifdef HAVE_STATX
      if (statx(0, afd_config_file, AT_STATX_SYNC_AS_STAT,
                STATX_MTIME, &stat_buf) == 0)
#else
      if (stat(afd_config_file, &stat_buf) == 0)
#endif
      {
#ifdef HAVE_STATX
         if (stat_buf.stx_mtime.tv_sec != old_st_mtime)
#else
         if (stat_buf.st_mtime != old_st_mtime)
#endif
         {
            char *buffer;

#ifdef HAVE_STATX
            old_st_mtime = stat_buf.stx_mtime.tv_sec;
#else
            old_st_mtime = stat_buf.st_mtime;
#endif
            if ((eaccess(afd_config_file, F_OK) == 0) &&
                (read_file_no_cr(afd_config_file, &buffer,
                                 YES, __FILE__, __LINE__) != INCORRECT))
            {
               int  max_connections = 0;
               char value[MAX_INT_LENGTH];

               if (get_definition(buffer,
                                  MAX_CONNECTIONS_DEF,
                                  value, MAX_INT_LENGTH) != NULL)
               {
                  max_connections = atoi(value);
               }
               if ((max_connections < 1) ||
                   (max_connections > MAX_CONFIGURABLE_CONNECTIONS))
               {
                  max_connections = MAX_DEFAULT_CONNECTIONS;
               }
               if (max_connections != old_max_connections)
               {
                  old_max_connections = max_connections;
                  (void)command(ssl, "MC %d", old_max_connections);
               }
               free(buffer);
            }
         }
      }
      else
      {
         if (errno != ENOENT)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                      _("Failed to stat() `%s' : %s"),
                      afd_config_file, strerror(errno));
         }
      }
   }

   if (old_sys_log_ec != p_afd_status->sys_log_ec)
   {
      char buf[LOG_FIFO_SIZE + 1];

      old_sys_log_ec = p_afd_status->sys_log_ec;
      for (i = 0; i < LOG_FIFO_SIZE; i++)
      {
         buf[i] = p_afd_status->sys_log_fifo[i] + ' ';
      }
      buf[i] = '\0';
      (void)command(ssl, "SR %u %s", old_sys_log_ec, buf);
   }

   if (memcmp(old_receive_log_history, p_afd_status->receive_log_history,
              MAX_LOG_HISTORY) != 0)
   {
      char buf[MAX_LOG_HISTORY + 1];

      (void)memcpy(old_receive_log_history, p_afd_status->receive_log_history,
                   MAX_LOG_HISTORY);
      for (i = 0; i < MAX_LOG_HISTORY; i++)
      {
         buf[i] = p_afd_status->receive_log_history[i] + ' ';
      }
      buf[i] = '\0';
      (void)command(ssl, "RH %s", buf);
   }
   if (memcmp(old_sys_log_history, p_afd_status->sys_log_history,
              MAX_LOG_HISTORY) != 0)
   {
      char buf[MAX_LOG_HISTORY + 1];

      (void)memcpy(old_sys_log_history, p_afd_status->sys_log_history,
                   MAX_LOG_HISTORY);
      for (i = 0; i < MAX_LOG_HISTORY; i++)
      {
         buf[i] = p_afd_status->sys_log_history[i] + ' ';
      }
      buf[i] = '\0';
      (void)command(ssl, "SH %s", buf);
   }
   if (memcmp(old_trans_log_history, p_afd_status->trans_log_history,
              MAX_LOG_HISTORY) != 0)
   {
      char buf[MAX_LOG_HISTORY + 1];

      (void)memcpy(old_trans_log_history, p_afd_status->trans_log_history,
                   MAX_LOG_HISTORY);
      for (i = 0; i < MAX_LOG_HISTORY; i++)
      {
         buf[i] = p_afd_status->trans_log_history[i] + ' ';
      }
      buf[i] = '\0';
      (void)command(ssl, "TH %s", buf);
   }
   for (i = 0; i < no_of_hosts; i++)
   {
      if (fsa[i].real_hostname[0][0] != GROUP_IDENTIFIER)
      {
         if (memcmp(old_error_history[i], fsa[i].error_history,
                    ERROR_HISTORY_LENGTH) != 0)
         {
            int  k,
                 length = 0;
            char ebuf[((ERROR_HISTORY_LENGTH - 1) * (MAX_INT_LENGTH + 1)) + 1];

            (void)memcpy(old_error_history[i], fsa[i].error_history,
                         ERROR_HISTORY_LENGTH);
            for (k = 1; k < ERROR_HISTORY_LENGTH; k++)
            {
               length += snprintf(ebuf + length,
                                  (((ERROR_HISTORY_LENGTH - 1) * (MAX_INT_LENGTH + 1)) + 1) - length,
                                  " %d", old_error_history[i][k]);
            }
            (void)command(ssl, "EL %d %d %s", i, old_error_history[i][0], ebuf);
         }
      }
   }

   /*
    * Check if status of any of the main process (AMG, FD and
    * archive_watch) have changed.
    */
   if (old_amg_status != p_afd_status->amg)
   {
      old_amg_status = p_afd_status->amg;
      (void)command(ssl, "AM %d", old_amg_status);
   }
   if (old_fd_status != p_afd_status->fd)
   {
      old_fd_status = p_afd_status->fd;
      (void)command(ssl, "FD %d", old_fd_status);
   }
   if (old_archive_watch_status != p_afd_status->archive_watch)
   {
      old_archive_watch_status = p_afd_status->archive_watch;
      (void)command(ssl, "AW %d", old_archive_watch_status);
   }

   return;
}
