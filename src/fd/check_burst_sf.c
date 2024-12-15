/*
 *  check_burst_sf.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2024 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_burst_sf - checks if FD still has jobs in the queue
 **
 ** SYNOPSIS
 **   int check_burst_sf(char         *file_path,
 **                      int          *files_to_send,
 **                      int          move_flag,
 **                      int          *ol_fd,
 **                      unsigned int *total_append_count,
 **                      unsigned int *values_changed)
 **
 ** DESCRIPTION
 **   The function check_burst_sf() checks if FD has jobs in the queue
 **   for this host. If so it gets the new job name and if it is in
 **   the error directory via a fifo created by this function. The
 **   fifo will be removed once it has the data.
 **
 **   The structure of data send via the fifo will be as follows:
 **             char in_error_dir
 **             char msg_name[MAX_MSG_NAME_LENGTH]
 **
 ** RETURN VALUES
 **   Returns NO if FD does not have any job in queue or if an error
 **   has occurred. If there is a job in queue YES will be returned
 **   and if the job_id of the current job is not the same it will
 **   fill up the structure job db with the new data.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.05.2001 H.Kiehl Created
 **   22.01.2005 H.Kiehl Check that ports are the some, otherwise don't
 **                      burst.
 **   11.04.2006 H.Kiehl For protocol SCP we must also check if the
 **                      target directories are the same before we do
 **                      a burst.
 **   18.05.2006 H.Kiehl For protocol SFTP users must be the same,
 **                      otherwise a burst is not possible.
 **   16.10.2021 H.Kiehl Do send noop for HTTP.
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
#include "smtpdefs.h"
#include "httpdefs.h"
#include "ssh_commondefs.h"
#ifdef _WITH_WMO_SUPPORT
# include "wmodefs.h"
#endif

/* External global variables. */
extern int                        no_of_hosts,
                                  *p_no_of_hosts,
                                  prev_no_of_files_done;
extern unsigned int               burst_2_counter;
extern u_off_t                    prev_file_size_done;
extern char                       *p_work_dir;
extern struct filetransfer_status *fsa;
extern struct job                 db;

/* Local variables. */
static sig_atomic_t               signal_caught;

/* Local function prototypes. */
static void                       free_db(struct job *),
                                  sig_alarm(int);
#ifdef SF_BURST_ACK
static void                       ack_burst(char *);
#endif


/*########################## check_burst_sf() ###########################*/
int
check_burst_sf(char         *file_path,
               int          *files_to_send,
               int          move_flag,
#ifdef _WITH_INTERRUPT_JOB
               int          interrupt,
#endif
#ifdef _OUTPUT_LOG
               int          *ol_fd,
#endif
#ifndef AFDBENCH_CONFIG
               unsigned int *total_append_count,
#endif
               unsigned int *values_changed)
{
   int ret;

   if ((fsa->protocol_options & DISABLE_BURSTING) == 0)
   {
      int              in_keep_connected_loop;
      unsigned int     alarm_sleep_time;
      time_t           start_time = 0L;
      sigset_t         newmask,
                       oldmask,
                       pendmask,
                       suspmask;
      struct sigaction newact,
                       oldact_alrm,
                       oldact_usr1;
#ifdef SF_BURST_ACK
      char             ack_msg_name[MAX_MSG_NAME_LENGTH];
#endif
      struct job       *p_new_db;

      /*
       * First check if there are any jobs queued for this host.
       */
      in_keep_connected_loop = NO;
#ifdef SF_BURST_ACK
      ack_msg_name[0] = '\0';
#endif
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
      do
      {
         ret = NO;

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
         if ((db.protocol != LOC_FLAG) && (db.protocol != EXEC_FLAG) &&
             (my_strcmp(db.hostname, fsa->real_hostname[(int)(fsa->host_toggle - 1)]) != 0))
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
         if (in_keep_connected_loop == YES)
         {
#ifndef AFDBENCH_CONFIG
            int diff_no_of_files_done;
#endif

            signal_caught = NO;
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
            fsa->job_status[(int)db.job_no].unique_name[2] = 5;

            /* Indicate to FD that signal handler is in place. */
            fsa->job_status[(int)db.job_no].file_name_in_use[MAX_FILENAME_LENGTH - 1] = 1;

#ifdef _WITH_INTERRUPT_JOB
            if (interrupt == YES)
            {
               fsa->job_status[(int)db.job_no].unique_name[3] = 4;
            }
#endif

#ifndef AFDBENCH_CONFIG
            diff_no_of_files_done = fsa->job_status[(int)db.job_no].no_of_files_done -
                                    prev_no_of_files_done;
            if (diff_no_of_files_done > 0)
            {
               int     length = MAX_PATH_LENGTH;
               char    how[13],
                       msg_str[MAX_PATH_LENGTH];
               u_off_t diff_file_size_done;

               if (db.protocol & LOC_FLAG)
               {
                  if ((move_flag & FILES_MOVED) &&
                      ((move_flag & FILES_COPIED) == 0))
                  {
                     /* moved */
                     how[0] = 'm'; how[1] = 'o'; how[2] = 'v';
                     how[3] = 'e'; how[4] = 'd'; how[5] = '\0';
                  }
                  else if (((move_flag & FILES_MOVED) == 0) &&
                           (move_flag & FILES_COPIED))
                       {
                          /* copied */
                          how[0] = 'c'; how[1] = 'o'; how[2] = 'p';
                          how[3] = 'i'; how[4] = 'e'; how[5] = 'd';
                          how[6] = '\0';
                       }
                       else
                       {
                          /* copied/moved */
                          how[0] = 'c'; how[1] = 'o'; how[2] = 'p';
                          how[3] = 'i'; how[4] = 'e'; how[5] = 'd';
                          how[6] = '/'; how[7] = 'm'; how[8] = 'o';
                          how[9] = 'v'; how[10] = 'e'; how[11] = 'd';
                          how[12] = '\0';
                       }
               }
               else if (db.protocol & SMTP_FLAG)
                    {
                       /* mailed */
                       how[0] = 'm'; how[1] = 'a'; how[2] = 'i';
                       how[3] = 'l'; how[4] = 'e'; how[5] = 'd';
                       how[6] = '\0';
                    }
#ifdef _WITH_DE_MAIL_SUPPORT
               else if (db.protocol & DE_MAIL_FLAG)
                    {
                       /* de-mailed */
                       how[0] = 'd'; how[1] = 'e'; how[2] = '-';
                       how[3] = 'm'; how[4] = 'a'; how[5] = 'i';
                       how[6] = 'l'; how[7] = 'e'; how[8] = 'd';
                       how[9] = '\0';
                    }
#endif
               else if (db.protocol & EXEC_FLAG)
                    {
                       /* execed */
                       how[0] = 'e'; how[1] = 'x'; how[2] = 'e';
                       how[3] = 'c'; how[4] = 'e'; how[5] = 'd';
                       how[6] = '\0';
                    }
#ifdef _WITH_DFAX_SUPPORT
               else if (db.protocol & DFAX_FLAG)
                    {
                       /* faxed */
                       how[0] = 'f'; how[1] = 'a'; how[2] = 'e';
                       how[3] = 'd'; how[4] = '\0';
                    }
#endif
                    else
                    {
                       /* sent */
                       how[0] = 's'; how[1] = 'e'; how[2] = 'n';
                       how[3] = 't'; how[4] = '\0';
                    }
               diff_file_size_done = fsa->job_status[(int)db.job_no].file_size_done - prev_file_size_done;
               WHAT_DONE_BUFFER(length, msg_str, how,
                                diff_file_size_done, diff_no_of_files_done);
               prev_no_of_files_done = fsa->job_status[(int)db.job_no].no_of_files_done;
               prev_file_size_done = fsa->job_status[(int)db.job_no].file_size_done;
               if (total_append_count != NULL)
               {
                  if (*total_append_count == 1)
                  {
                     /* Write " [APPEND]" */
                     msg_str[length] = ' ';
                     msg_str[length + 1] = '[';
                     msg_str[length + 2] = 'A';
                     msg_str[length + 3] = 'P';
                     msg_str[length + 4] = 'P';
                     msg_str[length + 5] = 'E';
                     msg_str[length + 6] = 'N';
                     msg_str[length + 7] = 'D';
                     msg_str[length + 8] = ']';
                     msg_str[length + 9] = '\0';
                     length += 9;
                     *total_append_count = 0;
                  }
                  else if (*total_append_count > 1)
                       {
                          length += snprintf(&msg_str[length],
                                             MAX_PATH_LENGTH - length,
                                             " [APPEND * %u]",
                                             *total_append_count);
                          *total_append_count = 0;
                       }
               }
               if ((burst_2_counter - 1) == 1)
               {
                  /* Write " [BURST]" */
                  msg_str[length] = ' ';
                  msg_str[length + 1] = '[';
                  msg_str[length + 2] = 'B';
                  msg_str[length + 3] = 'U';
                  msg_str[length + 4] = 'R';
                  msg_str[length + 5] = 'S';
                  msg_str[length + 6] = 'T';
                  msg_str[length + 7] = ']';
                  msg_str[length + 8] = '\0';
                  burst_2_counter = 1;
               }
               else if ((burst_2_counter - 1) > 1)
                    {
                       (void)snprintf(&msg_str[length],
                                      MAX_PATH_LENGTH - length,
                                      " [BURST * %u]", (burst_2_counter - 1));
                       burst_2_counter = 1;
                    }
               if ((db.special_flag & SEND_ZERO_SIZE) == 0)
               {
                  trans_log(INFO_SIGN, NULL, 0, NULL, NULL, "%s #%x",
                            msg_str, db.id.job);
               }
               else
               {
                  trans_log(INFO_SIGN, NULL, 0, NULL, NULL,
                            "[Zero size] %s #%x", msg_str, db.id.job);
               }
            }
#endif
            (void)alarm(alarm_sleep_time);
            suspmask = oldmask;
            sigdelset(&suspmask, SIGALRM);
            sigdelset(&suspmask, SIGUSR1);
            sigsuspend(&suspmask); /* Wait for SIGUSR1 or SIGALRM. */
            (void)alarm(0);
            if (gsf_check_fsa((struct job *)&db) != NEITHER)
            {
               if (signal_caught == NO)
               {
                  if (fsa->job_status[(int)db.job_no].unique_name[2] == 5)
                  {
                     fsa->job_status[(int)db.job_no].unique_name[2] = 0;
                  }
                  else
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "unique_name unexpectedly modified to %s [%s]",
                                fsa->job_status[(int)db.job_no].unique_name,
                                db.msg_name);
#ifdef _MAINTAINER_LOG
                     maintainer_log(WARN_SIGN, __FILE__, __LINE__,
                                    "unique_name unexpectedly modified to %s [%s]",
                                    fsa->job_status[(int)db.job_no].unique_name,
                                    db.msg_name);
#endif
                  }
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

            if (fsa == NULL)
            {
               return(NO);
            }
            if (fsa->job_status[(int)db.job_no].unique_name[2] == 6)
            {
               /*
                * Another job is waiting that cannot use the current
                * connection.
                */
               fsa->job_status[(int)db.job_no].unique_name[2] = 0;
               return(NO);
            }
         }
         else /* Not in keep connected loop. */
         {
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

#ifdef _WITH_INTERRUPT_JOB
               if (interrupt == YES)
               {
                  fsa->job_status[(int)db.job_no].unique_name[3] = 4;
               }
#endif

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
                     else
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   "unique_name unexpectedly modified to %s [%s]",
                                   fsa->job_status[(int)db.job_no].unique_name,
                                   db.msg_name);
#ifdef _MAINTAINER_LOG
                        maintainer_log(WARN_SIGN, __FILE__, __LINE__,
                                       "unique_name unexpectedly modified to %s [%s]",
                                       fsa->job_status[(int)db.job_no].unique_name,
                                       db.msg_name);
#endif
                     }
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
         }

         /* It could be that the FSA changed. */
         if ((gsf_check_fsa((struct job *)&db) == YES) &&
             (db.fsa_pos == INCORRECT))
         {
#ifdef SF_BURST_ACK
            if (ret == YES)
            {
               ack_burst(db.msg_name);
            }
#endif

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
             * This is only a hack! Somehow FD sends retrieve jobs
             * to sf_xxx!. If the bug is found remove this.
             */
            int i = 3;

            while ((i < MAX_MSG_NAME_LENGTH) &&
                   (fsa->job_status[(int)db.job_no].unique_name[i] != '/') &&
                   (fsa->job_status[(int)db.job_no].unique_name[i] != '\0'))
            {
               i++;
            }
            if ((i == MAX_MSG_NAME_LENGTH) ||
                (fsa->job_status[(int)db.job_no].unique_name[i] == '\0'))
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "FD trying to give me a retrieve job %x. [%s[%d] %s %s %x]",
                          fsa->job_status[(int)db.job_no].job_id,
                          fsa->host_dsp_name, (int)db.job_no,
                          (i == MAX_MSG_NAME_LENGTH) ? "To Long!" : fsa->job_status[(int)db.job_no].unique_name,
                          db.msg_name, db.id.job);
               return(NO);
            }
            /*
             * End of hack!
             */
#endif /* RETRIEVE_JOB_HACK */

#ifdef SF_BURST_ACK
            (void)memcpy(ack_msg_name, db.msg_name, MAX_MSG_NAME_LENGTH);
#endif
            (void)memcpy(db.msg_name, 
                         fsa->job_status[(int)db.job_no].unique_name,
                         MAX_MSG_NAME_LENGTH);
            if (fsa->job_status[(int)db.job_no].job_id != db.id.job)
            {
               char msg_name[MAX_PATH_LENGTH];

               db.id.job = fsa->job_status[(int)db.job_no].job_id;
               if ((p_new_db = malloc(sizeof(struct job))) == NULL)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "malloc() error : %s", strerror(errno));
#ifdef SF_BURST_ACK
                  if (ret == YES)
                  {
                     ack_burst(ack_msg_name);
                  }
#endif
                  return(NO);
               }

               if (fsa->protocol_options & FTP_IGNORE_BIN)
               {
                  p_new_db->transfer_mode         = 'N';
               }
               else
               {
                  p_new_db->transfer_mode         = DEFAULT_TRANSFER_MODE;
               }
               p_new_db->special_ptr              = NULL;
               p_new_db->subject                  = NULL;
               p_new_db->from                     = NULL;
               p_new_db->reply_to                 = NULL;
               p_new_db->group_to                 = NULL;
               p_new_db->group_mail_domain        = NULL;
#ifdef _WITH_DE_MAIL_SUPPORT
               p_new_db->de_mail_options          = DEFAULT_CONFIRMATION;
               p_new_db->de_mail_privat_id        = NULL;
               p_new_db->de_mail_privat_id_length = 0;
               p_new_db->de_mail_sender           = NULL;
               p_new_db->demcd_log                = YES;
#endif
               p_new_db->charset                  = NULL;
               p_new_db->lock_file_name           = NULL;
               p_new_db->exec_cmd                 = NULL;
#ifdef _WITH_TRANS_EXEC
               p_new_db->trans_exec_cmd           = NULL;
               p_new_db->trans_exec_timeout       = DEFAULT_EXEC_TIMEOUT;
               p_new_db->set_trans_exec_lock      = NO;
#endif
               p_new_db->special_flag             = 0;
               p_new_db->mode_flag                = 0;
               p_new_db->archive_time             = DEFAULT_ARCHIVE_TIME;
               if ((fsa->job_status[(int)db.job_no].file_name_in_use[0] == '\0') &&
                   (fsa->job_status[(int)db.job_no].file_name_in_use[1] == 1))
               {
                  p_new_db->retries               = (unsigned int)atoi(&fsa->job_status[(int)db.job_no].file_name_in_use[2]);
                  if (p_new_db->retries > 0)
                  {
                     p_new_db->special_flag |= OLD_ERROR_JOB;
                  }
               }
               else
               {
                  p_new_db->retries               = 0;
               }
               p_new_db->age_limit                = DEFAULT_AGE_LIMIT;
#ifdef _OUTPUT_LOG
               p_new_db->output_log               = YES;
#endif
               p_new_db->name2dir_char            = '\0';
               p_new_db->lock                     = DEFAULT_LOCK;
               p_new_db->http_proxy[0]            = '\0';
               p_new_db->smtp_server[0]           = '\0';
               p_new_db->chmod_str[0]             = '\0';
               p_new_db->dir_mode                 = 0;
               p_new_db->dir_mode_str[0]          = '\0';
               p_new_db->cn_filter                = NULL;
               p_new_db->cn_rename_to             = NULL;
               p_new_db->trans_rename_rule[0]     = '\0';
               p_new_db->user_rename_rule[0]      = '\0';
               p_new_db->rename_file_busy         = '\0';
               p_new_db->group_list               = NULL;
               p_new_db->no_of_rhardlinks         = 0;
               p_new_db->hardlinks                = NULL;
               p_new_db->no_of_rsymlinks          = 0;
               p_new_db->symlinks                 = NULL;
               p_new_db->no_of_restart_files      = 0;
               p_new_db->restart_file             = NULL;
               p_new_db->user_id                  = -1;
               p_new_db->group_id                 = -1;
               p_new_db->filename_pos_subject     = -1;
               p_new_db->subject_rename_rule[0]   = '\0';
               p_new_db->recipient                = db.recipient;
#ifdef WITH_DUP_CHECK
               p_new_db->dup_check_flag           = fsa->dup_check_flag;
               p_new_db->dup_check_timeout        = fsa->dup_check_timeout;
               p_new_db->trans_dup_check_flag     = 0;
               p_new_db->trans_dup_check_timeout  = 0L;
#endif
#ifdef WITH_SSL
               p_new_db->tls_auth                 = NO;
#endif
               if (db.protocol & FTP_FLAG)
               {
                  p_new_db->port = DEFAULT_FTP_PORT;
               }
               else if (db.protocol & SFTP_FLAG)
                    {
                       p_new_db->port = SSH_PORT_UNSET;
                    }
#ifdef _WITH_SCP_SUPPORT
               else if (db.protocol & SCP_FLAG)
                    {
                       p_new_db->port = SSH_PORT_UNSET;
                       p_new_db->chmod = FILE_MODE;
                    }
#endif
#ifdef _WITH_WMO_SUPPORT
               else if (db.protocol & WMO_FLAG)
                    {
                       p_new_db->port = DEFAULT_WMO_PORT;
                    }
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
               else if ((db.protocol & SMTP_FLAG) ||
                        (db.protocol & DE_MAIL_FLAG))
#else
               else if (db.protocol & SMTP_FLAG)
#endif
                    {
                       p_new_db->port = DEFAULT_SMTP_PORT;
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
               if (fsa->protocol_options & USE_SEQUENCE_LOCKING)
               {
                  p_new_db->special_flag |= SEQUENCE_LOCKING;
               }
               (void)strcpy(p_new_db->lock_notation, DOT_NOTATION);
               (void)snprintf(msg_name, MAX_PATH_LENGTH, "%s%s/%x",
                              p_work_dir, AFD_MSG_DIR, db.id.job);
               if (*(unsigned char *)((char *)p_no_of_hosts + AFD_FEATURE_FLAG_OFFSET_START) & ENABLE_CREATE_TARGET_DIR)
               {
                  p_new_db->special_flag |= CREATE_TARGET_DIR;
               }

               /*
                * NOTE: We must set protocol for eval_message()
                *       otherwise some values are NOT set!
                */
               p_new_db->default_from = db.default_from;
               p_new_db->protocol = db.protocol;
               p_new_db->id.job = db.id.job;
               p_new_db->password[0] = '\0';
               p_new_db->index_file = NULL; /* For sf_xxx always NULL */
#ifdef HAVE_SETPRIORITY
               p_new_db->afd_config_mtime = db.afd_config_mtime;
#endif
               if (eval_message(msg_name, p_new_db) < 0)
               {
                  free_db(p_new_db);
#ifdef SF_BURST_ACK
                  if (ret == YES)
                  {
                     ack_burst(ack_msg_name);
                  }
#endif

                  return(NO);
               }

               /*
                * Ports must be the same!
                */
               if ((p_new_db->port != db.port) ||
#ifdef _WITH_SCP_SUPPORT
                   ((db.protocol & SCP_FLAG) &&
                    (CHECK_STRCMP(p_new_db->target_dir, db.target_dir) != 0)) ||
#endif
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
               if (((db.protocol & EXEC_FLAG) == 0) &&
                   (db.special_flag & PATH_MAY_CHANGE) &&
                   (db.recipient != NULL))
               {
                  unsigned int error_mask;
                  time_t       now = time(NULL);

                  if ((error_mask = url_evaluate(db.recipient, NULL, NULL,
                                                 NULL, NULL,
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
                                db.recipient, error_msg);
                     ret = NO;
                     db.keep_connected = 0;
                  }
                  else
                  {
                     if (error_mask & TARGET_DIR_CAN_CHANGE)
                     {
                        db.special_flag |= PATH_MAY_CHANGE;
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
                  p_new_db = NULL;
                  ret = YES;
               }
            }
         }

         if (ret == YES)
         {
            *files_to_send = init_sf_burst2(p_new_db, file_path, values_changed);
            if (*files_to_send < 1)
            {
#ifdef SF_BURST_ACK
               ack_burst(ack_msg_name);
#endif
               ret = RETRY;
            }
         }
         else if ((ret == NO) && (db.keep_connected > 0))
              {
                 if (time(NULL) < (start_time + db.keep_connected))
                 {
#ifdef _OUTPUT_LOG
                    if (*ol_fd > -1)
                    {
                       (void)close(*ol_fd);
                       *ol_fd = -2;
                    }
#endif
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
                    if ((db.protocol & FTP_FLAG) || (db.protocol & SFTP_FLAG) ||
#ifdef _WITH_DE_MAIL_SUPPORT
                        (db.protocol & DE_MAIL_FLAG) ||
#endif
                        (db.protocol & HTTP_FLAG) || (db.protocol & SMTP_FLAG))
                    {
                       if (noop_wrapper() == SUCCESS)
                       {
                          ret = RETRY;
                       }
                    }
                    else
                    {
                       ret = RETRY;
                    }

                    if (ret == RETRY)
                    {
                       time_t diff_time;

                       diff_time = time(NULL) - start_time;
                       if (diff_time < db.keep_connected)
                       {
                          if ((diff_time > DEFAULT_NOOP_INTERVAL) ||
                              (diff_time == 0))
                          {
                             alarm_sleep_time = DEFAULT_NOOP_INTERVAL;
                          }
                          else
                          {
                             alarm_sleep_time = diff_time;
                          }
                          if (alarm_sleep_time > db.keep_connected)
                          {
                             alarm_sleep_time = db.keep_connected;
                          }
                          if (alarm_sleep_time == 0)
                          {
                             ret = NO;
                          }
                          else
                          {
                             in_keep_connected_loop = YES;
                          }
                       }
                       else
                       {
                          ret = NO;
                       }
                    }
                 }
              }
      } while (ret == RETRY);
#ifdef SF_BURST_ACK
      if (ret == YES)
      {
         ack_burst(ack_msg_name);
      }
#endif
   }
   else
   {
      ret = NO;
   }

   return(ret);
}


/*++++++++++++++++++++++++++++++ free_db() ++++++++++++++++++++++++++++++*/
static void
free_db(struct job *p_new_db)
{
   if (p_new_db->recipient != NULL)
   {
      free(p_new_db->recipient);
   }
   if (p_new_db->lock_file_name != NULL)
   {
      free(p_new_db->lock_file_name);
   }
   if (p_new_db->cn_filter != NULL)
   {
      free(p_new_db->cn_filter);
   }
   if (p_new_db->cn_rename_to != NULL)
   {
      free(p_new_db->cn_rename_to);
   }
   if (p_new_db->subject != NULL)
   {
      free(p_new_db->subject);
   }
   if (p_new_db->from != NULL)
   {
      free(p_new_db->from);
   }
   if (p_new_db->reply_to != NULL)
   {
      free(p_new_db->reply_to);
   }
   if (p_new_db->group_to != NULL)
   {
      free(p_new_db->group_to);
   }
   if (p_new_db->group_mail_domain != NULL)
   {
      free(p_new_db->group_mail_domain);
   }
#ifdef _WITH_DE_MAIL_SUPPORT
   if (p_new_db->de_mail_sender != NULL)
   {
      free(p_new_db->de_mail_sender);
   }
   if (p_new_db->de_mail_privat_id != NULL)
   {
      free(p_new_db->de_mail_privat_id);
   }
#endif
   if (p_new_db->charset != NULL)
   {
      free(p_new_db->charset);
   }
   if (p_new_db->lock_file_name != NULL)
   {
      free(p_new_db->lock_file_name);
   }
#ifdef _WITH_TRANS_EXEC
   if (p_new_db->trans_exec_cmd != NULL)
   {
      free(p_new_db->trans_exec_cmd);
   }
#endif
   if (p_new_db->special_ptr != NULL)
   {
      free(p_new_db->special_ptr);
   }

   free(p_new_db);

   return;
}


#ifdef SF_BURST_ACK
/*+++++++++++++++++++++++++++++++ ack_burst() +++++++++++++++++++++++++++*/
static void
ack_burst(char *ack_msg_name)
{
   if (ack_msg_name[0] != '\0')
   {
      int            fd,
                     i;
# ifdef WITHOUT_FIFO_RW_SUPPORT
      int            readfd;
# endif
      unsigned int   job_id,
                     unique_number,
                     split_job_counter;
      unsigned short dir_no;
      time_t         creation_time;
# ifdef MULTI_FS_SUPPORT
      dev_t          dev;
# endif
      char           ack_fifo[MAX_PATH_LENGTH],
                     fifo_buffer[SF_BURST_ACK_MSG_LENGTH],
                     str_num[MAX_TIME_T_HEX_LENGTH + 1],
                     *p_unique_name = ack_msg_name;

      /* Extract numbers from ack_msg_name. */
# ifdef MULTI_FS_SUPPORT
      /* Device number. */
      i = 0;
      while ((i < MAX_TIME_T_HEX_LENGTH) && (*(p_unique_name + i) != '/') &&
             (*(p_unique_name + i) != '\0'))
      {
         str_num[i] = *(p_unique_name + i);
         i++;
      }
      if ((*(p_unique_name + i) != '/') || (i == 0))
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not determine message name from `%s'.",
                    ack_msg_name);
         return;
      }
      str_num[i] = '\0';
      dev = (dev_t)str2devt(str_num, NULL, 16);
      p_unique_name += (i + 1);
# endif

      /* Job ID. */
      i = 0;
      while ((i < MAX_INT_HEX_LENGTH) &&
             (*(p_unique_name + i) != '/') && (*(p_unique_name + i) != '\0'))
      {
         str_num[i] = *(p_unique_name + i);
         i++;
      }
      if ((*(p_unique_name + i) != '/') || (i == 0))
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not determine message name from `%s' (i=%d).",
                    ack_msg_name, i);
         return;
      }
      str_num[i] = '\0';
      job_id = (unsigned int)strtoul(str_num, NULL, 16);
      p_unique_name += (i + 1);

      /* Directory number. */
      i = 0;
      while ((i < MAX_INT_HEX_LENGTH) &&
             (*(p_unique_name + i) != '/') && (*(p_unique_name + i) != '\0'))
      {
         str_num[i] = *(p_unique_name + i);
         i++;
      }
      if ((*(p_unique_name + i) != '/') || (i == 0))
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not determine message name from `%s' (i=%d).",
                    ack_msg_name, i);
         return;
      }
      str_num[i] = '\0';
      dir_no = (unsigned short)strtoul(str_num, NULL, 16);
      p_unique_name += (i + 1);

      /* Creation time. */
      i = 0;
      while ((i < MAX_TIME_T_HEX_LENGTH) &&
             (*(p_unique_name + i) != '_') && (*(p_unique_name + i) != '\0'))
      {
         str_num[i] = *(p_unique_name + i);
         i++;
      }
      if ((*(p_unique_name + i) != '_') || (i == 0))
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not determine message name from `%s' (i=%d).",
                    ack_msg_name, i);
         return;
      }
      str_num[i] = '\0';
      creation_time = (time_t)str2timet(str_num, NULL, 16);
      p_unique_name += (i + 1);

      /* Unique number. */
      i = 0;
      while ((i < MAX_INT_HEX_LENGTH) &&
             (*(p_unique_name + i) != '_') && (*(p_unique_name + i) != '\0'))
      {
         str_num[i] = *(p_unique_name + i);
         i++;
      }
      if ((*(p_unique_name + i) != '_') || (i == 0))
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not determine message name from `%s' (i=%d).",
                    ack_msg_name, i);
         return;
      }
      str_num[i] = '\0';
      unique_number = (unsigned int)strtoul(str_num, NULL, 16);
      p_unique_name += (i + 1);

      /* Split job number. */
      i = 0;
      while ((i <= MAX_INT_HEX_LENGTH) && (*(p_unique_name + i) != '\0'))
      {
         str_num[i] = *(p_unique_name + i);
         i++;
      }
      if (i == 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not determine message name from `%s' (i=%d).",
                    ack_msg_name, i);
         return;
      }
      str_num[i] = '\0';
      split_job_counter = (unsigned int)strtoul(str_num, NULL, 16);

      *(time_t *)(fifo_buffer) = creation_time;
# ifdef MULTI_FS_SUPPORT
      *(dev_t *)(fifo_buffer + sizeof(time_t)) = dev;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) +
                        sizeof(dev_t)) = job_id;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(dev_t) +
                        sizeof(unsigned int)) = split_job_counter;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(dev_t) +
                        sizeof(unsigned int) +
                        sizeof(unsigned int)) = unique_number;
      *(unsigned short *)(fifo_buffer + sizeof(time_t) + sizeof(dev_t) +
                          sizeof(unsigned int) + sizeof(unsigned int) +
                          sizeof(unsigned int)) = dir_no;
# else
      *(unsigned int *)(fifo_buffer + sizeof(time_t)) = job_id;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) +
                        sizeof(unsigned int)) = split_job_counter;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(unsigned int) +
                        sizeof(unsigned int)) = unique_number;
      *(unsigned short *)(fifo_buffer + sizeof(time_t) + sizeof(unsigned int) +
                          sizeof(unsigned int) + sizeof(unsigned int)) = dir_no;
# endif /* MULTI_FS_SUPPORT */

      (void)strcpy(ack_fifo, p_work_dir);
      (void)strcat(ack_fifo, FIFO_DIR);
      (void)strcat(ack_fifo, SF_BURST_ACK_FIFO);
# ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(ack_fifo, &readfd, &fd) == -1)
# else
      if ((fd = open(ack_fifo, O_RDWR)) == -1)
# endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to open() %s : %s",
                    ack_fifo, strerror(errno));
      }
      else
      {
         if (write(fd, fifo_buffer,
                   SF_BURST_ACK_MSG_LENGTH) != SF_BURST_ACK_MSG_LENGTH)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to write() to FIFO `%s' : %s",
                       ack_fifo, strerror(errno));
         }
# ifdef WITHOUT_FIFO_RW_SUPPORT
         if ((close(readfd) == -1) || (close(fd) == -1))
# else
         if (close(fd) == -1)
# endif
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Failed to close() `%s' : %s", ack_fifo, strerror(errno));
         }
      }
   }
#ifdef _MAINTAINER_LOG
   else
   {
      maintainer_log(WARN_SIGN, __FILE__, __LINE__,
                     "No ack_msg_name! (%s)", db.msg_name);
   }
#endif

   return;
}
#endif


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
