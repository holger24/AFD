/*
 *  check_burst_gf.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2014 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_burst_gf - checks if FD still has jobs in the queue for gf_xxx
 **
 ** SYNOPSIS
 **   int check_burst_gf(unsigned int *values_changed)
 **
 ** DESCRIPTION
 **   The function check_burst_gf() checks if FD has jobs in the queue
 **   for this host.
 **
 ** RETURN VALUES
 **   Returns NO if FD does not have any job in queue or if an error
 **   has occurred. If there is a job in queue YES will be returned
 **   and if the dir_id of the current job is not the same it will
 **   fill up the structure job db with the new data.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   05.08.2014 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* snprintf()                         */
#include <string.h>                /* strlen(), strcpy()                 */
#include <stdlib.h>                /* atoi(), malloc()                   */
#include <signal.h>
#include <sys/time.h>              /* struct timeval                     */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>                /* read(), write(), close()           */
#include <errno.h>
#include "fddefs.h"
#include "ftpdefs.h"
#include "httpdefs.h"
#include "ssh_commondefs.h"

/* External global variables. */
extern int                        no_of_dirs,
                                  no_of_listed_files,
                                  prev_no_of_files_done,
                                  rl_fd;
extern unsigned int               burst_2_counter;
extern u_off_t                    prev_file_size_done;
extern char                       msg_str[],
                                  *p_work_dir;
extern struct fileretrieve_status *fra;
extern struct filetransfer_status *fsa;
extern struct job                 db;
extern struct retrieve_list       *rl;

/* Local variables. */
static sig_atomic_t               signal_caught;

/* Local function prototypes. */
static void                       free_db(struct job *),
                                  sig_alarm(int);


/*########################### check_burst_gf() ##########################*/
int
check_burst_gf(unsigned int *values_changed)
{
   int ret;

   if ((fsa->protocol_options & DISABLE_BURSTING) == 0)
   {
      unsigned int     alarm_sleep_time;
      time_t           start_time = 0L;
      sigset_t         newmask,
                       oldmask,
                       pendmask,
                       suspmask;
      struct sigaction newact,
                       oldact_alrm,
                       oldact_usr1;
      struct job       *p_new_db;

      /*
       * First check if there are any jobs queued for this host.
       */
      if ((fsa->keep_connected > 0) &&
          ((fsa->special_flag & KEEP_CON_NO_SEND) == 0))
      {
         db.keep_connected = fsa->keep_connected;
         alarm_sleep_time = DEFAULT_NOOP_INTERVAL;
         start_time = time(NULL);
      }
      else
      {
         db.keep_connected = 0;
         alarm_sleep_time = WAIT_FOR_FD_REPLY;
      }
      ret = NO;

      /* It could be that the FSA changed. */
      if ((gsf_check_fsa((struct job *)&db) != NO) &&
          (db.fsa_pos == INCORRECT))
      {
         /*
          * Host is no longer in FSA, so there is
          * no way we can communicate with FD.
          */
         return(NO);
      }

      if (((fra->dir_options & ONE_PROCESS_JUST_SCANNING) == 0) ||
          (db.special_flag & DISTRIBUTED_HELPER_JOB))
      {
         if ((db.protocol != LOC_FLAG) && (db.protocol != EXEC_FLAG) &&
             (my_strcmp(db.hostname,
                        fsa->real_hostname[(int)(fsa->host_toggle - 1)]) != 0))
         {
            /*
             * Hostname changed, either a switch host or the real
             * hostname has changed. Regardless what ever happened
             * we now need to disconnect.
             */
            fsa->job_status[(int)db.job_no].unique_name[2] = 0;
            return(NO);
         }

         fsa->job_status[(int)db.job_no].unique_name[1] = '\0';
         if ((fsa->jobs_queued > 0) &&
             (fsa->active_transfers == fsa->allowed_transfers))
         {
            int   fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
            int   readfd;
#endif
            pid_t pid;
            char  generic_fifo[MAX_PATH_LENGTH];

            (void)strcpy(generic_fifo, p_work_dir);
            (void)strcat(generic_fifo, FIFO_DIR);
            (void)strcat(generic_fifo, SF_FIN_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
            if (open_fifo_rw(generic_fifo, &readfd, &fd) == -1)
#else
            if ((fd = open(generic_fifo, O_RDWR)) == -1)
#endif
            {
               fsa->job_status[(int)db.job_no].unique_name[2] = 0;
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to open() %s : %s",
                          generic_fifo, strerror(errno));
               return(NO);
            }
            signal_caught = NO;
            pid = -db.my_pid;

            newact.sa_handler = sig_alarm;
            sigemptyset(&newact.sa_mask);
            newact.sa_flags = 0;
            if ((sigaction(SIGALRM, &newact, &oldact_alrm) < 0) ||
                (sigaction(SIGUSR1, &newact, &oldact_usr1) < 0))
            {
               fsa->job_status[(int)db.job_no].unique_name[2] = 0;
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to establish a signal handler for SIGUSR1 and/or SIGALRM : %s",
                          strerror(errno));
               return(NO);
            }
            sigemptyset(&newmask);
            sigaddset(&newmask, SIGALRM);
            sigaddset(&newmask, SIGUSR1);
            if (sigprocmask(SIG_BLOCK, &newmask, &oldmask) < 0)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "sigprocmask() error : %s", strerror(errno));
            }

            fsa->job_status[(int)db.job_no].unique_name[2] = 4;

            /* Indicate to FD that signal handler is in place. */
            fsa->job_status[(int)db.job_no].file_name_in_use[MAX_FILENAME_LENGTH - 1] = 1;

            if (write(fd, &pid, sizeof(pid_t)) != sizeof(pid_t))
            {
               int tmp_errno = errno;

               fsa->job_status[(int)db.job_no].unique_name[2] = 0;
               fsa->job_status[(int)db.job_no].file_name_in_use[MAX_FILENAME_LENGTH - 1] = 2;
               if ((sigaction(SIGUSR1, &oldact_usr1, NULL) < 0) ||
                   (sigaction(SIGALRM, &oldact_alrm, NULL) < 0))
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Failed to reestablish a signal handler for SIGUSR1 and/or SIGALRM : %s",
                             strerror(errno));
               }
               if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "sigprocmask() error : %s", strerror(errno));
               }
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "write() error : %s", strerror(tmp_errno));
#ifdef WITHOUT_FIFO_RW_SUPPORT
               if ((close(readfd) == -1) || (close(fd) == -1))
#else
               if (close(fd) == -1)
#endif
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "close() error : %s", strerror(errno));
               }
               return(NO);
            }
            (void)alarm(alarm_sleep_time);
            suspmask = oldmask;
            sigdelset(&suspmask, SIGALRM);
            sigdelset(&suspmask, SIGUSR1);
            sigsuspend(&suspmask);
            (void)alarm(0);
            if (gsf_check_fsa((struct job *)&db) != NEITHER)
            {
               if (signal_caught == NO)
               {
                  if (fsa->job_status[(int)db.job_no].unique_name[2] == 4)
                  {
                     fsa->job_status[(int)db.job_no].unique_name[2] = 0;
                  }
#ifdef _MAINTAINER_LOG
                  else
                  {
                     maintainer_log(WARN_SIGN, __FILE__, __LINE__,
                                    "unique_name unexpectedly modified to %s [%s]",
                                    fsa->job_status[(int)db.job_no].unique_name,
                                    db.msg_name);
                  }
#endif
               }

               /* Indicate FD we no longer want any signals. */
               fsa->job_status[(int)db.job_no].file_name_in_use[MAX_FILENAME_LENGTH - 1] = 2;
            }

            /*
             * Lets unblock any remaining signals before restoring
             * the original signal handler.
             */
            if (sigpending(&pendmask) < 0)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "sigpending() error : %s", strerror(errno));
            }
            else
            {
               if ((sigismember(&pendmask, SIGALRM)) ||
                   (sigismember(&pendmask, SIGUSR1)))
               {
                  if (sigprocmask(SIG_UNBLOCK, &newmask, NULL) < 0)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "sigprocmask() error : %s", strerror(errno));
                  }
               }
            }

            /* Restore the original signal mask. */
            if ((sigaction(SIGUSR1, &oldact_usr1, NULL) < 0) ||
                (sigaction(SIGALRM, &oldact_alrm, NULL) < 0))
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to reestablish a signal handler for SIGUSR1 and/or SIGALRM : %s",
                          strerror(errno));
            }
            if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "sigprocmask() error : %s", strerror(errno));
            }

#ifdef WITHOUT_FIFO_RW_SUPPORT
            if ((close(readfd) == -1) || (close(fd) == -1))
#else
            if (close(fd) == -1)
#endif
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "close() error : %s", strerror(errno));
            }

            if ((signal_caught == NO) &&
                (fsa->job_status[(int)db.job_no].unique_name[1] == '\0'))
            {
               if (gsf_check_fsa((struct job *)&db) != NEITHER)
               {
                  fsa->job_status[(int)db.job_no].unique_name[2] = 1;
               }
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                          "Hmmm, FD had no message for <%s> [%u sec] [job %d (%d)]!",
#else
                          "Hmmm, FD had no message for <%s> [%u sec] [job %d (%lld)]!",
#endif
                          fsa->host_alias, alarm_sleep_time,
                          (int)db.job_no, (pri_pid_t)-pid);
               return(NO);
            }
         }
         else
         {
            if (db.keep_connected == 0)
            {
               return(NO);
            }
            else
            {
               ret = NO;
            }
         }

         /* It could be that the FSA changed. */
         if ((gsf_check_fsa((struct job *)&db) == YES) &&
             (db.fsa_pos == INCORRECT))
         {
            /*
             * Host is no longer in FSA, so there is
             * no way we can communicate with FD.
             */
            return(NO);
         }

         if ((fsa->job_status[(int)db.job_no].unique_name[1] != '\0') &&
             (fsa->job_status[(int)db.job_no].unique_name[0] != '\0') &&
             (fsa->job_status[(int)db.job_no].unique_name[2] != '\0'))
         {
#ifdef RETRIEVE_JOB_HACK
            /*
             * This is only a hack! Somehow FD sends send jobs
             * to gf_xxx!. If the bug is found remove this.
             */
            int i = 3;

            while ((i < MAX_MSG_NAME_LENGTH) &&
                   (fsa->job_status[(int)db.job_no].unique_name[i] != '/') &&
                   (fsa->job_status[(int)db.job_no].unique_name[i] != '\0'))
            {
               i++;
            }
            if ((i == MAX_MSG_NAME_LENGTH) ||
                (fsa->job_status[(int)db.job_no].unique_name[i] == '/'))
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "FD trying to give me a send job #%x. [%s[%d]]",
                          fsa->job_status[(int)db.job_no].job_id,
                          fsa->host_dsp_name, (int)db.job_no);
               return(NO);
            }
            /*
             * End of hack!
             */
#endif /* RETRIEVE_JOB_HACK */

            (void)memcpy(db.msg_name, 
                         fsa->job_status[(int)db.job_no].unique_name,
                         MAX_INT_HEX_LENGTH);
            if (fsa->job_status[(int)db.job_no].job_id != db.id.dir)
            {
               time_t next_check_time;

               if ((rl_fd != -1) && (rl != NULL))
               {
                  int i;

                  for (i = 0; i < no_of_listed_files; i++)
                  {
                     if (rl[i].assigned == ((unsigned char)db.job_no + 1))
                     {
                        rl[i].assigned = 0;
                     }
                  }
               }
#ifdef DO_NOT_PARALLELIZE_ALL_FETCH
               if ((fra->stupid_mode == YES) || (fra->remove == YES))
               {
                  detach_ls_data(YES);
               }
               else
               {
#endif
                  detach_ls_data(NO);
#ifdef DO_NOT_PARALLELIZE_ALL_FETCH
               }
#endif
               fra_detach_pos(db.fra_pos);
               db.id.dir = fsa->job_status[(int)db.job_no].job_id;
               if (fra_attach() != SUCCESS)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to attach to FRA.");
                  return(NO);
               }
               if ((db.fra_pos = get_dir_id_position(fra, db.id.dir, no_of_dirs)) < 0)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to locate dir_id %x in the FRA.",
                             db.id.dir);
                  return(NO);
               }
               (void)fra_detach();
               if (fra_attach_pos(db.fra_pos) != SUCCESS)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to attach to FRA position %d.", db.fra_pos);
                  return(NO);
               }
               if ((p_new_db = malloc(sizeof(struct job))) == NULL)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "malloc() error : %s", strerror(errno));
                  return(NO);
               }

               if (fsa->protocol_options & FTP_IGNORE_BIN)
               {
                  p_new_db->transfer_mode       = 'N';
               }
               else
               {
                  p_new_db->transfer_mode       = DEFAULT_TRANSFER_MODE;
               }
               p_new_db->special_ptr            = NULL;
               p_new_db->special_flag           = 0;
               p_new_db->mode_flag              = 0;
               if ((fsa->job_status[(int)db.job_no].file_name_in_use[0] == '\0') &&
                   (fsa->job_status[(int)db.job_no].file_name_in_use[1] == 1))
               {
                  p_new_db->retries             = (unsigned int)atoi(&fsa->job_status[(int)db.job_no].file_name_in_use[2]);
                  fsa->job_status[(int)db.job_no].file_name_in_use[2] = 0;
                  if (p_new_db->retries > 0)
                  {
                     p_new_db->special_flag |= OLD_ERROR_JOB;
                  }
               }
               else
               {
                  p_new_db->retries             = 0;
               }
               p_new_db->http_proxy[0]          = '\0';
               p_new_db->dir_mode               = db.dir_mode;
               p_new_db->dir_mode_str[0]        = db.dir_mode_str[0];
               p_new_db->dir_mode_str[1]        = db.dir_mode_str[1];
               p_new_db->dir_mode_str[2]        = db.dir_mode_str[2];
               p_new_db->dir_mode_str[3]        = db.dir_mode_str[3];
               p_new_db->dir_mode_str[4]        = db.dir_mode_str[4];
               p_new_db->no_of_restart_files    = 0;
               p_new_db->restart_file           = NULL;
               p_new_db->user_id                = -1;
               p_new_db->group_id               = -1;
#ifdef WITH_SSL
               p_new_db->tls_auth               = NO;
#endif
               p_new_db->ssh_protocol           = 0;
               if (db.protocol & FTP_FLAG)
               {
                  p_new_db->port = DEFAULT_FTP_PORT;
               }
               else if (db.protocol & SFTP_FLAG)
                    {
                       p_new_db->port = SSH_PORT_UNSET;
                    }
               else if (db.protocol & HTTP_FLAG)
                    {
#ifdef WITH_SSL
                       if (db.protocol & SSL_FLAG)
                       {
                          p_new_db->port = DEFAULT_HTTPS_PORT;
                       }
                       else
                       {
#endif
                          p_new_db->port = DEFAULT_HTTP_PORT;
#ifdef WITH_SSL
                       }
#endif
                    }
                    else
                    {
                       p_new_db->port = -1;
                    }

               /*
                * NOTE: We must set some values for eval_recipient()
                *       otherwise some values are NOT set!
                */
               p_new_db->fsa_pos = db.fsa_pos;
               p_new_db->protocol = db.protocol;
               p_new_db->id.dir = db.id.dir;
               p_new_db->password[0] = '\0';
               p_new_db->smtp_server[0] = '\0';
#ifdef HAVE_SETPRIORITY
               p_new_db->afd_config_mtime = db.afd_config_mtime;
#endif
               if ((fsa->error_counter > 0) && (fra->no_of_time_entries > 0))
               {
                  next_check_time = fra->next_check_time;
               }
               else
               {
                  next_check_time = 0;
               }
               if ((db.protocol & HTTP_FLAG) &&
                   (fra->dir_options & URL_WITH_INDEX_FILE_NAME))
               {
                  if ((p_new_db->index_file = malloc(MAX_RECIPIENT_LENGTH)) == NULL)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Could not malloc() memory for index file : %s",
                                strerror(errno));
                     exit(ALLOC_ERROR);
                  }
               }
               else
               {
                  p_new_db->index_file = NULL;
               }
               if (eval_recipient(fra->url, p_new_db, NULL,
                                  next_check_time) == INCORRECT)
               {
                  free_db(p_new_db);

                  return(NO);
               }

               /*
                * Ports must be the same!
                */
               if ((p_new_db->port != db.port) ||
#ifdef WITH_SSL
                   ((db.tls_auth == NO) && (p_new_db->tls_auth != NO)) ||
                   ((db.tls_auth != NO) && (p_new_db->tls_auth == NO)) ||
#endif
                   ((db.protocol & SFTP_FLAG) &&
                    (CHECK_STRCMP(p_new_db->user, db.user) != 0)))
               {
                  free_db(p_new_db);
                  ret = NEITHER;
               }
               else
               {
                  if ((p_new_db->protocol & FTP_FLAG) &&
                      (p_new_db->mode_flag == 0))
                  {
                     if (fsa->protocol_options & FTP_PASSIVE_MODE)
                     {
                        p_new_db->mode_flag = PASSIVE_MODE;
                        if (fsa->protocol_options & FTP_EXTENDED_MODE)
                        {
                           (void)strcpy(p_new_db->mode_str, "extended passive");
                        }
                        else
                        {
                           if (fsa->protocol_options & FTP_ALLOW_DATA_REDIRECT)
                           {
                              (void)strcpy(p_new_db->mode_str,
                                           "passive (with redirect)");
                           }
                           else
                           {
                              (void)strcpy(p_new_db->mode_str, "passive");
                           }
                        }
                     }
                     else
                     {
                        p_new_db->mode_flag = ACTIVE_MODE;
                        if (fsa->protocol_options & FTP_EXTENDED_MODE)
                        {
                           (void)strcpy(p_new_db->mode_str, "extended active");
                        }
                        else
                        {
                           (void)strcpy(p_new_db->mode_str, "active");
                        }
                     }
                     if (fsa->protocol_options & FTP_EXTENDED_MODE)
                     {
                        p_new_db->mode_flag |= EXTENDED_MODE;
                     }
                  }
                  ret = YES;
               }
            }
            else
            {
               if (db.special_flag & PATH_MAY_CHANGE)
               {
                  unsigned int error_mask;
                  time_t       now = time(NULL);

                  if ((error_mask = url_evaluate(fra->url, NULL, NULL, NULL,
                                                 NULL,
#ifdef WITH_SSH_FINGERPRINT
                                                 NULL, NULL,
#endif
                                                 NULL, NO, NULL, NULL,
                                                 db.target_dir, NULL, &now,
                                                 NULL, NULL, NULL, NULL,
                                                 NULL, NULL)) > 3)
                  {
                     char error_msg[MAX_URL_ERROR_MSG];

                     url_get_error(error_mask, error_msg, MAX_URL_ERROR_MSG);
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "Incorrect url `%s'. Error is: %s.",
                                fra->url, error_msg);
                     ret = NO;
                     db.keep_connected = 0;
                  }
                  else
                  {
                     if (error_mask & TARGET_DIR_CAN_CHANGE)
                     {
                        db.special_flag |= PATH_MAY_CHANGE;
                     }
                     if (db.protocol == HTTP_FLAG)
                     {
                        if (db.target_dir[0] == '\0')
                        {
                           db.target_dir[0] = '/';
                           db.target_dir[1] = '\0';
                        }
                        else
                        {
                           ret = strlen(db.target_dir) - 1;
                           if (db.target_dir[ret] != '/')
                           {
                              db.target_dir[ret + 1] = '/';
                              db.target_dir[ret + 2] = '\0';
                           }
                        }
                     }
                     if (CHECK_STRCMP(db.active_target_dir,
                                      db.target_dir) != 0)
                     {
                        *values_changed |= TARGET_DIR_CHANGED;
                        (void)strcpy(db.active_target_dir, db.target_dir);
                     }
                     ret = YES;
                     p_new_db = NULL;
                  }
               }
               else
               {
                  ret = YES;
                  p_new_db = NULL;
               }
            }
         }
      } /* if (((fra->dir_options & ONE_PROCESS_JUST_SCANNING) == 0) || */
        /*     (db.special_flag & DISTRIBUTED_HELPER_JOB))              */

      if (ret == YES)
      {
         init_gf_burst2(p_new_db, values_changed);
      }
      else if ((ret == NO) && (db.keep_connected > 0))
           {
              (void)gsf_check_fra((struct job *)&db);
              if (db.fra_pos != INCORRECT)
              {
                 time_t timeup;

                 if (fra->keep_connected > 0)
                 {
                    db.keep_connected = fra->keep_connected;
                 }
                 else if ((fsa->keep_connected > 0) &&
                          ((fsa->special_flag & KEEP_CON_NO_FETCH) == 0))
                      {
                         db.keep_connected = fsa->keep_connected;
                      }
                      else
                      {
                         db.keep_connected = 0;
                         return(NO);
                      }

                 timeup = start_time + db.keep_connected;
                 if (db.no_of_time_entries == 0)
                 {
                    fra->next_check_time = start_time + db.remote_file_check_interval;
                 }
                 else
                 {
                    fra->next_check_time = calc_next_time_array(db.no_of_time_entries,
                                                                db.te,
#ifdef WITH_TIMEZONE
                                                                NULL,
#endif
                                                                start_time,
                                                                __FILE__, __LINE__);
                 }
                 if (fra->next_check_time > timeup)
                 {
                    return(NO);
                 }
                 else
                 {
                    if (fra->next_check_time < start_time)
                    {
                       return(YES);
                    }
                    else
                    {
                       timeup = fra->next_check_time;
                    }
                 }
                 if (gsf_check_fsa((struct job *)&db) != NEITHER)
                 {
                    int    status;
                    time_t sleeptime = 0;

                    if (fsa->protocol_options & STAT_KEEPALIVE)
                    {
                       sleeptime = fsa->transfer_timeout - 5;
                    }
                    if (sleeptime < 1)
                    {
                       sleeptime = DEFAULT_NOOP_INTERVAL;
                    }
                    if ((start_time + sleeptime) > timeup)
                    {
                       sleeptime = timeup - start_time;
                    }
                    do
                    {
                       (void)sleep(sleeptime);
                       (void)gsf_check_fra((struct job *)&db);
                       if (db.fra_pos == INCORRECT)
                       {
                          return(NO);
                       }
                       if (gsf_check_fsa((struct job *)&db) == NEITHER)
                       {
                          break;
                       }
                       if (((fra->dir_options & ONE_PROCESS_JUST_SCANNING) == 0) ||
                           (db.special_flag & DISTRIBUTED_HELPER_JOB))
                       {
                          if (fsa->job_status[(int)db.job_no].unique_name[2] == 6)
                          {
                             fsa->job_status[(int)db.job_no].unique_name[2] = '\0';
                             return(NO);
                          }
                       }
                       else
                       {
                          if (((db.protocol & FTP_FLAG) == 0) &&
                              ((db.protocol & SFTP_FLAG) == 0) &&
                              ((db.protocol & HTTP_FLAG) == 0))
                          {
                             return(RESCAN_SOURCE);
                          }
                       }
                       start_time = time(NULL);
                       if (start_time < timeup)
                       {
                          if (fsa->transfer_rate_limit > 0)
                          {
                             int  fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
                             int  readfd;
#endif
                             char trl_calc_fifo[MAX_PATH_LENGTH];

                             (void)strcpy(trl_calc_fifo, p_work_dir);
                             (void)strcat(trl_calc_fifo, FIFO_DIR);
                             (void)strcat(trl_calc_fifo, TRL_CALC_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
                             if (open_fifo_rw(trl_calc_fifo, &readfd, &fd) == -1)
#else
                             if ((fd = open(trl_calc_fifo, O_RDWR)) == -1)
#endif
                             {
                                system_log(WARN_SIGN, __FILE__, __LINE__,
                                           "Failed to open() FIFO `%s' : %s",
                                           trl_calc_fifo, strerror(errno));
                             }
                             else
                             {
                                if (write(fd, &db.fsa_pos, sizeof(int)) != sizeof(int))
                                {
                                   system_log(WARN_SIGN, __FILE__, __LINE__,
                                              "Failed to write() to FIFO `%s' : %s",
                                              trl_calc_fifo, strerror(errno));
                                }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                                if (close(readfd) == -1)
                                {
                                   system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                              "Failed to close() FIFO `%s' (read) : %s",
                                              trl_calc_fifo, strerror(errno));
                                }
#endif
                                if (close(fd) == -1)
                                {
                                   system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                              "Failed to close() FIFO `%s' (read) : %s",
                                              trl_calc_fifo, strerror(errno));
                                }
                             }
                          }

                          if ((status = noop_wrapper()) != SUCCESS)
                          {
                             trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                                       "Failed to send NOOP command (%d). [%s]",
                                       status, fra->dir_alias);
                             return(NO);
                          }
                          start_time = time(NULL);
                       }
                       if ((fra->dir_options & ONE_PROCESS_JUST_SCANNING) &&
                           ((db.special_flag & DISTRIBUTED_HELPER_JOB) == 0) &&
                           (start_time >= timeup))
                       {
                          break;
                       }
                       if ((start_time + sleeptime) > timeup)
                       {
                          sleeptime = timeup - start_time;
                       }
                    } while (timeup > start_time);

                    if (db.special_flag & PATH_MAY_CHANGE)
                    {
                       unsigned int error_mask;

                       if ((error_mask = url_evaluate(fra->url, NULL,
                                                      NULL, NULL, NULL,
#ifdef WITH_SSH_FINGERPRINT
                                                      NULL, NULL,
#endif
                                                      NULL, NO, NULL, NULL,
                                                      db.target_dir, NULL,
                                                      &start_time, NULL,
                                                      NULL, NULL, NULL,
                                                      NULL, NULL)) > 3)
                       {
                          char error_msg[MAX_URL_ERROR_MSG];

                          url_get_error(error_mask, error_msg,
                                        MAX_URL_ERROR_MSG);
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                    "Failed to determine directory from %s. Error is: %s",
                                    fra->url, error_msg);
                          return(NO);
                       }
                       if (db.protocol == HTTP_FLAG)
                       {
                          if (db.target_dir[0] == '\0')
                          {
                             db.target_dir[0] = '/';
                             db.target_dir[1] = '\0';
                          }
                          else
                          {
                             ret = strlen(db.target_dir) - 1;
                             if (db.target_dir[ret] != '/')
                             {
                                db.target_dir[ret + 1] = '/';
                                db.target_dir[ret + 2] = '\0';
                             }
                          }
                       }

                       if (CHECK_STRCMP(db.active_target_dir,
                                        db.target_dir) != 0)
                       {
                          *values_changed |= TARGET_DIR_CHANGED;
                          (void)strcpy(db.active_target_dir, db.target_dir);
                       }
                    }

                    ret = RESCAN_SOURCE;
                 }
              }
           }
   }
   else
   {
      ret = NO;
   }

   if (ret == YES)
   {
      burst_2_counter++;
   }

   return(ret);
}


/*++++++++++++++++++++++++++++++ free_db() ++++++++++++++++++++++++++++++*/
static void
free_db(struct job *p_new_db)
{
   if (p_new_db->index_file != NULL)
   {
      free(p_new_db->index_file);
   }

   free(p_new_db);

   return;
}


/*+++++++++++++++++++++++++++++++ sig_alarm() +++++++++++++++++++++++++++*/
static void                                                                
sig_alarm(int signo)
{
   if (signo == SIGUSR1)
   {
      signal_caught = YES;
   }

   return; /* Return to wakeup sigsuspend(). */
}
