/*
 *  init_afd_worker.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2017 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   init_afd_worker - helper process that does some work for init_afd
 **
 ** SYNOPSIS
 **   init_afd_worker [--version] [-w <work dir>]
 **        --version      Prints current version and copyright
 **        -w <work dir>  Working directory of the AFD
 **
 ** DESCRIPTION
 **   This program is started by init_afd and does some work for it.
 **   init_afd should just monitor the other process and do as little
 **   work as possible so that the possibility that it crashes is
 **   small.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.03.2017 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>              /* strcpy(), strcat(), strerror()       */
#include <stdlib.h>              /* getenv(), atexit(), exit(), abort()  */
#include <time.h>                /* time(), ctime()                      */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>            /* struct timeval                       */
#include <signal.h>              /* signal()                             */
#include <unistd.h>              /* select()                             */
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"
#include "version.h"

#define FULL_DIR_CHECK_INTERVAL     300   /* Every 5 minutes. */
#define ACTION_DIR_CHECK_INTERVAL   60
#ifdef WITH_IP_DB
# define IP_DB_RESET_CHECK_INTERVAL 21600 /* Every 6 hours. */
#endif
#define WITH_ACKNOWLEDGED_OFFLINE_CHECK

/* Global definitions. */
int                        sys_log_fd = STDERR_FILENO,
                           event_log_fd = STDERR_FILENO,
                           trans_db_log_fd = STDERR_FILENO,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                           trans_db_log_readfd,
#endif
                           transfer_log_fd = STDERR_FILENO,
                           no_of_dirs = 0,
                           no_of_hosts = 0,
                           fra_fd = -1,
                           fra_id,
                           fsa_fd = -1,
                           fsa_id;
#ifdef HAVE_MMAP
off_t                      fra_size,
                           fsa_size;
#endif
char                       *p_work_dir;
struct filetransfer_status *fsa;
struct fileretrieve_status *fra;
struct afd_status          *p_afd_status;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void                afd_worker_exit(void),
                           get_afd_config_value(int *, int *),
                           sig_exit(int),
                           sig_bus(int),
                           sig_segv(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int            afd_worker_cmd_fd,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                  afd_worker_cmd_writefd,
#endif
                  amg_rescan_time,
                  danger_no_of_files,
                  i,
                  status;
   long           link_max;
   time_t         action_dir_check_time,
#ifdef WITH_IP_DB
                  ip_db_reset_time,
#endif
                  full_dir_check_time,
                  last_action_success_dir_time,
                  now;
   fd_set         rset;
   char           afd_action_dir[MAX_PATH_LENGTH],
                  *p_afd_action_dir,
                  work_dir[MAX_PATH_LENGTH];
   struct timeval timeout;
#ifdef HAVE_STATX
   struct statx   stat_buf;
#else
   struct stat    stat_buf;
#endif

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD. */
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   (void)umask(0);
   p_work_dir = work_dir;
   get_afd_config_value(&danger_no_of_files, &amg_rescan_time);

   /* Do some cleanups when we exit */
   if (atexit(afd_worker_exit) != 0)
   {
      (void)fprintf(stderr,
                    _("Could not register exit function : %s (%s %d)\n"),
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   else
   {
      char afd_worker_cmd[MAX_PATH_LENGTH];

      (void)strcpy(afd_worker_cmd, work_dir);
      (void)strcat(afd_worker_cmd, FIFO_DIR);
      (void)strcat(afd_worker_cmd, AFD_WORKER_CMD_FIFO);

      (void)unlink(afd_worker_cmd);
      if (make_fifo(afd_worker_cmd) < 0)
      {
         (void)fprintf(stderr, _("Could not create fifo `%s'. (%s %d)\n"),
                       afd_worker_cmd, __FILE__, __LINE__);
         exit(INCORRECT);
      }

      /* Now lets open all fifos needed by the AFD_WORKER. */
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(afd_worker_cmd, &afd_worker_cmd_fd,
                       &afd_worker_cmd_writefd) == -1)
#else
      if ((afd_worker_cmd_fd = coe_open(afd_worker_cmd, O_RDWR)) == -1)
#endif
      {  
         (void)fprintf(stderr, _("Could not open fifo `%s' : %s (%s %d)\n"),
                       afd_worker_cmd, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }

#ifdef _LINK_MAX_TEST
      link_max = LINKY_MAX;
#else
# ifdef REDUCED_LINK_MAX
      link_max = REDUCED_LINK_MAX;
# else
      (void)strcpy(afd_worker_cmd, work_dir);
      (void)strcat(afd_worker_cmd, AFD_FILE_DIR);
      if ((link_max = pathconf(afd_worker_cmd, _PC_LINK_MAX)) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("pathconf() _PC_LINK_MAX error, setting to %d : %s"),
                    _POSIX_LINK_MAX, strerror(errno));
         link_max = _POSIX_LINK_MAX;
      }
# endif
#endif
   }
   p_afd_action_dir = afd_action_dir + snprintf(afd_action_dir, MAX_PATH_LENGTH,
                                                "%s%s%s/",
                                                p_work_dir, ETC_DIR, ACTION_DIR);

   /* Activate some signal handlers, so we know what happened. */
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      (void)fprintf(stderr, _("signal() error : %s (%s %d)\n"),
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if ((i = fsa_attach(AFD_WORKER)) != SUCCESS)
   {
      if (i != INCORRECT_VERSION)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to attach to FSA."));
      }
      else
      {
         system_log(INFO_SIGN, __FILE__, __LINE__,
                    _("You can ignore the last warning about incorrect version."));
      }
   }
   if (attach_afd_status(NULL, 15) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to attach to AFD status area.");
      exit(INCORRECT);
   }

   now = time(NULL);
   full_dir_check_time = ((now / FULL_DIR_CHECK_INTERVAL) *
                          FULL_DIR_CHECK_INTERVAL) + FULL_DIR_CHECK_INTERVAL;
   action_dir_check_time = ((now / ACTION_DIR_CHECK_INTERVAL) *
                            ACTION_DIR_CHECK_INTERVAL) +
                           ACTION_DIR_CHECK_INTERVAL;
#ifdef WITH_IP_DB
   ip_db_reset_time = ((now / IP_DB_RESET_CHECK_INTERVAL) *
                       IP_DB_RESET_CHECK_INTERVAL) + IP_DB_RESET_CHECK_INTERVAL;
#endif
   last_action_success_dir_time = 0L;

   FD_ZERO(&rset);
   for (;;)
   {
      if (time(&now) > full_dir_check_time)
      {
         if (fra_attach() == SUCCESS)
         {
            unsigned int files_in_dir;
            off_t        bytes_in_dir;

            for (i = 0; i < no_of_dirs; i++)
            {
               if ((fra[i].fsa_pos == -1) && (fra[i].dir_flag & MAX_COPIED) &&
                   ((now - fra[i].last_retrieval) < (2 * amg_rescan_time)))
               {
                  count_files(fra[i].url, &files_in_dir, &bytes_in_dir);
                  fra[i].files_in_dir = files_in_dir;
                  fra[i].bytes_in_dir = bytes_in_dir;

                  /* Bail out if the scans take to long. */
                  if ((time(NULL) - now) > 30)
                  {
                     i = no_of_dirs;
                  }
               }
            }
            (void)fra_detach();
         }
         full_dir_check_time = ((now / FULL_DIR_CHECK_INTERVAL) *
                                FULL_DIR_CHECK_INTERVAL) +
                               FULL_DIR_CHECK_INTERVAL;
      }

      if (now > action_dir_check_time)
      {
         *p_afd_action_dir = '\0';
#ifdef HAVE_STATX
         if (statx(0, afd_action_dir, AT_STATX_SYNC_AS_STAT,
                   STATX_MODE, &stat_buf) == -1)
#else
         if (stat(afd_action_dir, &stat_buf) == -1)
#endif
         {
            if (errno != ENOENT)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          _("Failed to look at directory `%s' : %s"),
                          afd_action_dir, strerror(errno));
            }
         }
         else
         {
#ifdef HAVE_STATX
            if (S_ISDIR(stat_buf.stx_mode))
#else
            if (S_ISDIR(stat_buf.st_mode))
#endif
            {
               char *p_script;

               p_script = p_afd_action_dir +
                          snprintf(p_afd_action_dir,
                                   MAX_PATH_LENGTH - (p_afd_action_dir - afd_action_dir),
                                   "%s%s/",
                                   ACTION_TARGET_DIR, ACTION_SUCCESS_DIR);
#ifdef HAVE_STATX
               if (statx(0, afd_action_dir, AT_STATX_SYNC_AS_STAT,
                         STATX_MODE | STATX_MTIME, &stat_buf) == -1)
#else
               if (stat(afd_action_dir, &stat_buf) == -1)
#endif
               {
                  if (errno != ENOENT)
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                _("Failed to look at directory `%s' : %s"),
                                afd_action_dir, strerror(errno));
                  }
               }
               else
               {
#ifdef HAVE_STATX
                  if ((S_ISDIR(stat_buf.stx_mode)) &&
                      (last_action_success_dir_time < stat_buf.stx_mtime.tv_sec))
#else
                  if ((S_ISDIR(stat_buf.st_mode)) &&
                      (last_action_success_dir_time < stat_buf.st_mtime))
#endif
                  {
#ifdef HAVE_STATX
                     last_action_success_dir_time = stat_buf.stx_mtime.tv_sec;
#else
                     last_action_success_dir_time = stat_buf.st_mtime;
#endif
                     for (i = 0; i < no_of_hosts; i++)
                     {
                        (void)strcpy(p_script, fsa[i].host_alias);
                        if (eaccess(afd_action_dir, (R_OK | X_OK)) == 0)
                        {
                           fsa[i].host_status |= HOST_ACTION_SUCCESS;
                        }
                        else
                        {
                           fsa[i].host_status &= ~HOST_ACTION_SUCCESS;
                        }
                     }
                  }
               }
            }
         }
         action_dir_check_time = ((now / ACTION_DIR_CHECK_INTERVAL) *
                                  ACTION_DIR_CHECK_INTERVAL) +
                                 ACTION_DIR_CHECK_INTERVAL;
      }

#ifdef WITH_IP_DB
      if (now > ip_db_reset_time)
      {
         int  j,
              no_of_ip_hl;
         char *ip_hl = NULL,
              *p_ip_hl;

         no_of_ip_hl = get_current_ip_hl(&ip_hl, NULL);
         for (i = 0; i < no_of_hosts; i++)
         {
            if (fsa[i].real_hostname[0][0] != GROUP_IDENTIFIER)
            {
               p_ip_hl = ip_hl;
               for (j = 0; j < no_of_ip_hl; j++)
               {
                  if (my_strcmp(fsa[i].real_hostname[0], p_ip_hl) == 0)
                  {
                     if (j != (no_of_ip_hl - 1))
                     {
                        size_t move_size;

                        move_size = (no_of_ip_hl - 1 - j) * MAX_REAL_HOSTNAME_LENGTH;
                        (void)memmove(p_ip_hl, p_ip_hl + MAX_REAL_HOSTNAME_LENGTH,
                                      move_size);
                     }
                     no_of_ip_hl--;
                     break;
                  }
                  p_ip_hl += MAX_REAL_HOSTNAME_LENGTH;
               }
               fsa[i].host_status |= STORE_IP;
            }
         }
         if (no_of_ip_hl > 0)
         {
            if (attach_ip_db() == SUCCESS)
            {
               p_ip_hl = ip_hl;
               for (i = 0; i < no_of_ip_hl; i++)
               {
                  (void)remove_from_ip_db(p_ip_hl);
                  p_ip_hl += MAX_REAL_HOSTNAME_LENGTH;
               }
               (void)detach_ip_db();
            }
         }
         free(ip_hl);
         ip_db_reset_time = ((now / IP_DB_RESET_CHECK_INTERVAL) *
                             IP_DB_RESET_CHECK_INTERVAL) +
                            IP_DB_RESET_CHECK_INTERVAL;
      }
#endif /* WITH_IP_DB */

      /* Initialise descriptor set and timeout. */
      FD_SET(afd_worker_cmd_fd, &rset);
      timeout.tv_usec = 0;
      timeout.tv_sec = AFD_RESCAN_TIME;

      /* Wait for message x seconds and then continue. */
      status = select(afd_worker_cmd_fd + 1, &rset, NULL, NULL, &timeout);

      /* Did we get a timeout? */
      if (status == 0)
      {
         /*
          * If the number of errors are larger then max_errors
          * stop the queue for this host.
          */
         if (fsa != NULL)
         {
            unsigned char special_flag;
            unsigned int  host_status,
                          protocol;
            int           active_transfers,
                          error_counter,
                          error_hosts,
                          host_counter,
                          host_disabled_counter,
                          j,
                          jobs_in_queue = 0,
                          k,
                          lock_set = NO,
                          max_errors,
                          total_file_counter,
                          warn_hosts;
            off_t         total_file_size;
            u_off_t       bytes_send[MAX_NO_PARALLEL_JOBS];

            (void)check_fsa(NO, AFD_WORKER);

            for (i = 0; i < no_of_hosts; i++)
            {
               jobs_in_queue += fsa[i].jobs_queued;

               /* Update group summary. */
               if (fsa[i].real_hostname[0][0] == GROUP_IDENTIFIER)
               {
                  active_transfers = 0;
                  (void)memset(bytes_send, 0, sizeof(bytes_send));
                  error_counter = 0;
                  host_status = 0;
                  max_errors = 0;
                  protocol = 0;
                  special_flag = 0;
                  total_file_counter = 0;
                  total_file_size = 0;
                  host_counter = 0;
                  host_disabled_counter = 0;
                  error_hosts = warn_hosts = 0;
                  for (j = i + 1; ((j < no_of_hosts) &&
                                   (fsa[j].real_hostname[0][0] != GROUP_IDENTIFIER)); j++)
                  {
                     active_transfers += fsa[j].active_transfers;
                     error_counter += fsa[j].error_counter;
                     host_status |= fsa[j].host_status;
                     max_errors += fsa[j].max_errors;
                     protocol |= fsa[j].protocol;
                     special_flag |= fsa[j].special_flag;
                     if (fsa[j].special_flag & HOST_DISABLED)
                     {
                        host_disabled_counter++;
                     }
                     total_file_counter += fsa[j].total_file_counter;
                     total_file_size += fsa[j].total_file_size;
                     for (k = 0; ((k < fsa[j].allowed_transfers) &&
                                  (k < MAX_NO_PARALLEL_JOBS)); k++)
                     {
                        bytes_send[k] += fsa[j].job_status[k].bytes_send;
                     }

                     if ((fsa[j].error_counter >= fsa[j].max_errors) &&
                         ((fsa[j].host_status & HOST_ERROR_ACKNOWLEDGED) == 0) &&
                         ((fsa[j].host_status & HOST_ERROR_ACKNOWLEDGED_T) == 0) &&
                         ((fsa[j].host_status & HOST_ERROR_OFFLINE) == 0) &&
                         ((fsa[j].host_status & HOST_ERROR_OFFLINE_T) == 0) &&
                         ((fsa[j].host_status & HOST_ERROR_OFFLINE_STATIC) == 0))
                     {
                        /* NOT_WORKING2 */
                        error_hosts++;
                     }
                     else if ((fsa[j].host_status & HOST_WARN_TIME_REACHED) &&
                              ((fsa[j].host_status & HOST_ERROR_ACKNOWLEDGED) == 0) &&
                              ((fsa[j].host_status & HOST_ERROR_ACKNOWLEDGED_T) == 0) &&
                              ((fsa[j].host_status & HOST_ERROR_OFFLINE) == 0) &&
                              ((fsa[j].host_status & HOST_ERROR_OFFLINE_T) == 0) &&
                              ((fsa[j].host_status & HOST_ERROR_OFFLINE_STATIC) == 0))
                          {
                             /* WARNING_ID */
                             warn_hosts++;
                          }

                     host_counter++;
                  }
                  fsa[i].active_transfers = active_transfers;
                  for (k = 0; k < MAX_NO_PARALLEL_JOBS; k++)
                  {
                     fsa[i].job_status[k].bytes_send = bytes_send[k];
                  }
                  fsa[i].error_counter = error_counter;
                  if (error_hosts > 0)
                  {
                     host_status |= ERROR_HOSTS_IN_GROUP;
                  }
                  else
                  {
                     host_status &= ~ERROR_HOSTS_IN_GROUP;
                  }
                  if (warn_hosts > 0)
                  {
                     host_status |= WARN_HOSTS_IN_GROUP;
                  }
                  else
                  {
                     host_status &= ~WARN_HOSTS_IN_GROUP;
                  }
                  fsa[i].host_status = host_status;
                  fsa[i].max_errors = max_errors;
                  fsa[i].protocol = protocol;
                  if ((host_disabled_counter != host_counter) &&
                      (special_flag & HOST_DISABLED))
                  {
                     /*
                      * Don't show white status if not all host in group
                      * are disabled.
                      */
                     special_flag &= ~HOST_DISABLED;
                  }
                  fsa[i].special_flag = special_flag;
                  fsa[i].total_file_counter = total_file_counter;
                  fsa[i].total_file_size = total_file_size;
                  i = j - 1;
               }
            }
            p_afd_status->jobs_in_queue = jobs_in_queue;

            for (i = 0; i < no_of_hosts; i++)
            {
               if (fsa[i].real_hostname[0][0] != GROUP_IDENTIFIER)
               {
                  if ((fsa[i].max_errors > 0) &&
                      (((fsa[i].error_counter >= fsa[i].max_errors) &&
                        ((fsa[i].host_status & AUTO_PAUSE_QUEUE_STAT) == 0)) ||
                       ((fsa[i].error_counter < fsa[i].max_errors) &&
                        (fsa[i].host_status & AUTO_PAUSE_QUEUE_STAT))))
                  {
                     char *sign;

#ifdef LOCK_DEBUG
                     lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (i * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
                     lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (i * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
                     lock_set = YES;
                     fsa[i].host_status ^= AUTO_PAUSE_QUEUE_STAT;
                     if (fsa[i].error_counter >= fsa[i].max_errors)
                     {
                        if ((fsa[i].host_status & HOST_ERROR_OFFLINE_STATIC) ||
                            (fsa[i].host_status & HOST_ERROR_OFFLINE) ||
                            (fsa[i].host_status & HOST_ERROR_OFFLINE_T))
                        {
                           sign = OFFLINE_SIGN;
                        }
                        else
                        {
                           sign = WARN_SIGN;
                        }
                        ia_trans_log(sign, __FILE__, __LINE__, i,
                                     _("Stopped input queue, since there are to many errors."));
                        if ((fsa[i].host_status & PENDING_ERRORS) == 0)
                        {
                           fsa[i].host_status |= PENDING_ERRORS;
                           event_log(0L, EC_HOST, ET_EXT, EA_ERROR_START, "%s",
                                     fsa[i].host_alias);
                           error_action(fsa[i].host_alias, "start",
                                        HOST_ERROR_ACTION, transfer_log_fd);
                        }
                        event_log(0L, EC_HOST, ET_AUTO, EA_STOP_QUEUE,
                                  "%s%cErrors %d >= max errors %d",
                                  fsa[i].host_alias, SEPARATOR_CHAR,
                                  fsa[i].error_counter, fsa[i].max_errors);
                     }
                     else
                     {
                        if ((fsa[i].host_status & HOST_ERROR_OFFLINE_STATIC) ||
                            (fsa[i].host_status & HOST_ERROR_OFFLINE) ||
                            (fsa[i].host_status & HOST_ERROR_OFFLINE_T))
                        {
                           sign = OFFLINE_SIGN;
                        }
                        else
                        {
                           sign = INFO_SIGN;
                        }
                        ia_trans_log(sign, __FILE__, __LINE__, i,
                                     _("Started input queue that has been stopped due to too many errors."));
                        if (fsa[i].last_connection > fsa[i].first_error_time)
                        {
                           if (now > fsa[i].end_event_handle)
                           {
                              fsa[i].host_status &= ~EVENT_STATUS_FLAGS;
                              if (fsa[i].end_event_handle > 0L)
                              {
                                 fsa[i].end_event_handle = 0L;
                              }
                              if (fsa[i].start_event_handle > 0L)
                              {
                                 fsa[i].start_event_handle = 0L;
                              }
                           }
                           else
                           {
                              fsa[i].host_status &= ~EVENT_STATUS_STATIC_FLAGS;
                           }
                           event_log(0L, EC_HOST, ET_EXT, EA_ERROR_END, "%s",
                                     fsa[i].host_alias);
                           error_action(fsa[i].host_alias, "stop",
                                        HOST_ERROR_ACTION, transfer_log_fd);
                        }
                        event_log(0L, EC_HOST, ET_AUTO, EA_START_QUEUE, "%s",
                                  fsa[i].host_alias);
                     }
                  }
#ifdef WITH_ACKNOWLEDGED_OFFLINE_CHECK
                  if ((fsa[i].error_counter == 0) &&
                      (fsa[i].host_status & (HOST_ERROR_OFFLINE | HOST_ERROR_OFFLINE_T | HOST_ERROR_ACKNOWLEDGED | HOST_ERROR_ACKNOWLEDGED_T)))
                  {
                     if (fsa[i].host_status & HOST_ERROR_OFFLINE)
                     {
                        fsa[i].host_status &= ~HOST_ERROR_OFFLINE;
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   "Hmm, removing HOST_ERROR_OFFLINE flag from %s",
                                   fsa[i].host_alias);
                     }
                     if ((fsa[i].host_status & HOST_ERROR_OFFLINE_T) &&
                         (now > fsa[i].end_event_handle))
                     {
                        fsa[i].host_status &= ~HOST_ERROR_OFFLINE_T;
                        fsa[i].end_event_handle = 0L;
                        fsa[i].start_event_handle = 0L;
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   "Hmm, removing HOST_ERROR_OFFLINE_T flag from %s",
                                   fsa[i].host_alias);
                     }
                     if (fsa[i].host_status & HOST_ERROR_ACKNOWLEDGED)
                     {
                        fsa[i].host_status &= ~HOST_ERROR_ACKNOWLEDGED;
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   "Hmm, removing HOST_ERROR_ACKNOWLEDGED flag from %s",
                                   fsa[i].host_alias);
                     }
                     if ((fsa[i].host_status & HOST_ERROR_ACKNOWLEDGED_T) &&
                         (now > fsa[i].end_event_handle))
                     {
                        fsa[i].host_status &= ~HOST_ERROR_ACKNOWLEDGED_T;
                        fsa[i].end_event_handle = 0L;
                        fsa[i].start_event_handle = 0L;
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   "Hmm, removing HOST_ERROR_ACKNOWLEDGED_T flag from %s",
                                   fsa[i].host_alias);
                     }
                  }
#endif /* WITH_ACKNOWLEDGED_OFFLINE_CHECK */
                  if (((*(unsigned char *)((char *)fsa - AFD_FEATURE_FLAG_OFFSET_END) & DISABLE_HOST_WARN_TIME) == 0) &&
                      (fsa[i].warn_time > 0L) &&
                      ((now - fsa[i].last_connection) >= fsa[i].warn_time))
                  {
                     if ((fsa[i].host_status & HOST_WARN_TIME_REACHED) == 0)
                     {
                        char *sign;

                        if (lock_set == NO)
                        {
#ifdef LOCK_DEBUG
                           lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (i * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
                           lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (i * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
                           lock_set = YES;
                        }
                        fsa[i].host_status |= HOST_WARN_TIME_REACHED;
                        if ((fsa[i].host_status & HOST_ERROR_OFFLINE_STATIC) ||
                            (fsa[i].host_status & HOST_ERROR_OFFLINE) ||
                            (fsa[i].host_status & HOST_ERROR_OFFLINE_T))
                        {
                           sign = OFFLINE_SIGN;
                        }
                        else
                        {
                           sign = WARN_SIGN;
                        }
                        ia_trans_log(sign, __FILE__, __LINE__, i,
                                     _("Warn time reached."));
                        error_action(fsa[i].host_alias, "start",
                                     HOST_WARN_ACTION, transfer_log_fd);
                        event_log(0L, EC_HOST, ET_AUTO, EA_WARN_TIME_SET, "%s",
                                  fsa[i].host_alias);
                     }
                  }
                  else
                  {
                     if (fsa[i].host_status & HOST_WARN_TIME_REACHED)
                     {
                        if (lock_set == NO)
                        {
#ifdef LOCK_DEBUG
                           lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (i * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
                           lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (i * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
                           lock_set = YES;
                        }
                        fsa[i].host_status &= ~HOST_WARN_TIME_REACHED;
                        ia_trans_log(DEBUG_SIGN, __FILE__, __LINE__, i,
                                     _("Warn time stopped."));
                        error_action(fsa[i].host_alias, "stop",
                                     HOST_WARN_ACTION, transfer_log_fd);
                        event_log(0L, EC_HOST, ET_AUTO, EA_WARN_TIME_UNSET, "%s",
                                  fsa[i].host_alias);
                     }
                  }
                  if ((p_afd_status->jobs_in_queue >= (link_max / 2)) &&
                      ((fsa[i].host_status & DANGER_PAUSE_QUEUE_STAT) == 0) &&
                      (fsa[i].total_file_counter > danger_no_of_files))
                  {
                     if (lock_set == NO)
                     {
#ifdef LOCK_DEBUG
                        lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (i * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
                        lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (i * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
                        lock_set = YES;
                     }
                     fsa[i].host_status |= DANGER_PAUSE_QUEUE_STAT;
                     ia_trans_log(WARN_SIGN, __FILE__, __LINE__, i,
                                  _("Stopped input queue, since there are to many jobs in the input queue."));
                     event_log(0L, EC_HOST, ET_AUTO, EA_STOP_QUEUE,
                               _("%s%cNumber of files %d > file threshold %d"),
                               fsa[i].host_alias, SEPARATOR_CHAR,
                               fsa[i].total_file_counter, danger_no_of_files);
                  }
                  else if ((fsa[i].host_status & DANGER_PAUSE_QUEUE_STAT) &&
                           ((fsa[i].total_file_counter < (danger_no_of_files / 2)) ||
                            (p_afd_status->jobs_in_queue < (link_max / 4))))
                       {
                          if (lock_set == NO)
                          {
#ifdef LOCK_DEBUG
                             lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (i * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
                             lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (i * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
                             lock_set = YES;
                          }
                          fsa[i].host_status &= ~DANGER_PAUSE_QUEUE_STAT;
                          ia_trans_log(INFO_SIGN, __FILE__, __LINE__, i,
                                       _("Started input queue, that was stopped due to too many jobs in the input queue."));
                          event_log(0L, EC_HOST, ET_AUTO, EA_START_QUEUE, "%s",
                                    fsa[i].host_alias);
                       }

                  if (lock_set == YES)
                  {
#ifdef LOCK_DEBUG
                     unlock_region(fsa_fd, (AFD_WORD_OFFSET + (i * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
                     unlock_region(fsa_fd, (AFD_WORD_OFFSET + (i * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
                     lock_set = NO;
                  }
               }
            } /* for (i = 0; i < no_of_hosts; i++) */
         } /* if (fsa != NULL) */
      }
           /* Did we get a message from the process */
           /* that controls the AFD?                */
      else if (FD_ISSET(afd_worker_cmd_fd, &rset))
           {
              int  n;
              char buffer[DEFAULT_BUFFER_SIZE];

              if ((n = read(afd_worker_cmd_fd, buffer,
                            DEFAULT_BUFFER_SIZE)) > 0)
              {
                 int    count = 0;
                 time_t current_time;

                 /* Now evaluate all data read from fifo, byte after byte */
                 while (count < n)
                 {
                    switch (buffer[count])
                    {
                       case SHUTDOWN  : /* Shutdown AFD_WORKER. */
                          /* Tell init_afd that we received shutdown message */
                          if (send_cmd(ACKN, afd_worker_cmd_fd) < 0)
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        _("Failed to send ACKN : %s"),
                                        strerror(errno));
                          }

                          current_time = time(NULL);
                          (void)memset(buffer, '-', 35 + AFD_WORKER_LENGTH);
                          buffer[35 + AFD_WORKER_LENGTH] = '\0';
                          (void)fprintf(stderr, _("%.24s : Stopped %s\n%s\n"),
                                        ctime(&current_time), AFD_WORKER,
                                        buffer);
                          exit(SUCCESS);

                       default        : /* Reading garbage from fifo */

                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                     _("Reading garbage on AFD_WORKER command fifo [%d]. Ignoring."),
                                     (int)buffer[count]);
                          break;
                    }
                    count++;
                 } /* while (count < n) */
              } /* if (n > 0) */
           }
                /* An error has occurred */
           else if (status < 0)
                {
                   system_log(FATAL_SIGN, __FILE__, __LINE__,
                              _("select() error : %s"), strerror(errno));
                   exit(INCORRECT);
                }
                else
                {
                   system_log(FATAL_SIGN, __FILE__, __LINE__,
                              _("Unknown condition."));
                   exit(INCORRECT);
                }
   } /* for (;;) */

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++ get_afd_config_value() ++++++++++++++++++++++*/
static void
get_afd_config_value(int *danger_no_of_files, int *amg_rescan_time)
{
   char *buffer,
        config_file[MAX_PATH_LENGTH + ETC_DIR_LENGTH + AFD_CONFIG_FILE_LENGTH + 1];

   (void)snprintf(config_file,
                  MAX_PATH_LENGTH + ETC_DIR_LENGTH + AFD_CONFIG_FILE_LENGTH + 1,
                  "%s%s%s", p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   if ((eaccess(config_file, F_OK) == 0) &&
       (read_file_no_cr(config_file, &buffer, YES, __FILE__, __LINE__) != INCORRECT))
   {
      char value[MAX_INT_LENGTH];

      if (get_definition(buffer, MAX_COPIED_FILES_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         *danger_no_of_files = atoi(value);
         if (*danger_no_of_files < 1)
         {
            *danger_no_of_files = MAX_COPIED_FILES;
         }
         *danger_no_of_files = *danger_no_of_files + *danger_no_of_files;
      }
      else
      {
         *danger_no_of_files = MAX_COPIED_FILES + MAX_COPIED_FILES;
      }
      if (get_definition(buffer, AMG_DIR_RESCAN_TIME_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         *amg_rescan_time = atoi(value);
      }
      else
      {
         *amg_rescan_time = DEFAULT_RESCAN_TIME;
      }
      free(buffer);
   }
   else
   {
      *danger_no_of_files = MAX_COPIED_FILES + MAX_COPIED_FILES;
      *amg_rescan_time = DEFAULT_RESCAN_TIME;
   }

   return;
}


/*+++++++++++++++++++++++++++ afd_worker_exit() +++++++++++++++++++++++++*/
static void
afd_worker_exit(void)
{
   system_log(DEBUG_SIGN, NULL, 0,
              "Stopped %s. (%s)", AFD_WORKER, PACKAGE_VERSION);
   return;
}

/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   system_log(FATAL_SIGN, __FILE__, __LINE__,
              _("Aaarrrggh! Received SIGSEGV."));
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   system_log(FATAL_SIGN, __FILE__, __LINE__, _("Uuurrrggh! Received SIGBUS."));
   abort();
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   if (signo == SIGINT)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__, _("Received SIGINT!"));
   }
   else
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__, _("Received %d!"), signo);
   }

   exit(INCORRECT);
}
