/*
 *  stop_process.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   stop_process - stop one or all all monitor and log process
 **
 ** SYNOPSIS
 **   void stop_process(int process, int shutdown)
 **   void stop_log_process(int process)
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
 **   27.12.1998 H.Kiehl Created
 **   14.05.2000 H.Kiehl Enable killing only a single process.
 **   21.03.2007 H.Kiehl Added stop_log_process().
 **   14.10.2020 H.Kiehl Do not send a PROC_TERM if process is -1.
 **   09.02.2021 H.Kiehl Kill all process that do not want to stop
 **                      after some time with a SIGKILL.
 **   04.03.2021 H.Kiehl Remove PROC_TERM.
 **   22.04.2023 H.Kiehl Addied support for aldad.
 **
 */
DESCR__E_M3

#include <stdio.h>            /* fprintf()                               */
#include <string.h>           /* strerror()                              */
#include <sys/types.h>
#ifdef WITH_SYSTEMD
# include <systemd/sd-daemon.h>  /* sd_notify()                          */
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <sys/wait.h>         /* waitpid()                               */
#include <signal.h>           /* kill()                                  */
#include <time.h>             /* time()                                  */
#include <stdlib.h>           /* malloc(), free()                        */
#include <sys/time.h>         /* struct timeval                          */
#ifdef HAVE_MMAP
# include <sys/mman.h>        /* munmap()                                */
#endif
#include <unistd.h>           /* unlink(), gethostname()                 */
#include <errno.h>
#include "mondefs.h"

/* External global variables */
extern int                    daemon_log_fd,
                              got_shuttdown_message,
                              mon_resp_fd,
                              no_of_afds,
                              sleep_sys_log_fd,
                              sys_log_fd;
#ifdef WITH_SYSTEMD
extern int                    systemd_watchdog_enabled;
#endif
extern char                   mon_active_file[],
                              *p_work_dir,
                              *service_name;
extern pid_t                  aldad_pid,
                              mon_log_pid,
                              sys_log_pid;
extern struct process_list    *pl;
extern struct mon_status_area *msa;
extern struct afd_mon_status  *p_afd_mon_status;


/*############################ stop_process() ###########################*/
void
stop_process(int process, int shutdown)
{
   if (pl != NULL)
   {
      int   first,
            process_to_kill,
            kill_counter = 0,
            *kill_pos_list = NULL,
            i,
            last;
      pid_t *kill_list;

      if (process == -1)
      {
         first = 0;
         last = no_of_afds;
         process_to_kill = no_of_afds;
      }
      else
      {
         if (process < no_of_afds)
         {
            first = process;
            last = process + 1;
            process_to_kill = 1;
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Hmm, position in MSA to large [%d %d]",
                       process, no_of_afds);
            return;
         }
      }
      if (((kill_pos_list = malloc((process_to_kill + 1) * sizeof(int))) == NULL) ||
          ((kill_list = malloc((process_to_kill + 1) * sizeof(pid_t))) == NULL))
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to allocate memory for kill list : %s",
                    strerror(errno));
         exit(INCORRECT);
      }
      for (i = first; i < last; i++)
      {
         if (pl[i].mon_pid > 0)
         {
            if (kill(pl[i].mon_pid, SIGINT) == -1)
            {
               if (errno != ESRCH)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                             "Failed to kill %s process to %s (%d) : %s",
#else
                             "Failed to kill %s process to %s (%lld) : %s",
#endif
                             MON_PROC, pl[i].afd_alias,
                             (pri_pid_t)pl[i].mon_pid, strerror(errno));
               }
               pl[i].mon_pid = 0;
            }
            else
            {
               kill_list[kill_counter] = pl[i].mon_pid;
               kill_pos_list[kill_counter] = i;
               kill_counter++;
            }
         }
      }
#ifdef WITH_SYSTEMD
      UPDATE_HEARTBEAT();
#endif
      if (kill_counter > 0)
      {
         int   j;
         pid_t pid;

         /* Give them some time to terminate themself. */
         (void)my_usleep(200000); /* 0.2 seconds */

         /* Catch zombies. */
         if (kill_counter > 0)
         {
            for (i = 0; i < kill_counter; i++)
            {
               if (kill_pos_list[i] != -1)
               {
                  /* Wait at most 15 seconds per process. */
                  for (j = 0; j < 75; j++)
                  {
                     if ((pid = waitpid(kill_list[i], NULL, WNOHANG)) > 0)
                     {
                        if (pid == kill_list[i])
                        {
                           pl[kill_pos_list[i]].mon_pid = 0;
                           pl[kill_pos_list[i]].afd_alias[0] = '\0';
                           pl[kill_pos_list[i]].start_time = 0;
                           pl[kill_pos_list[i]].number_of_restarts = 0;
                           kill_pos_list[i] = -1;
                           break;
                        }
                        else
                        {
                           int k;

                           for (k = 0; k < kill_counter; k++)
                           {
                              if (pid == kill_list[k])
                              {
                                 pl[kill_pos_list[i]].mon_pid = 0;
                                 pl[kill_pos_list[i]].afd_alias[0] = '\0';
                                 pl[kill_pos_list[i]].start_time = 0;
                                 pl[kill_pos_list[i]].number_of_restarts = 0;
                                 kill_pos_list[k] = -1;
                                 k = kill_counter;
                              }
                           }
#ifdef WITH_SYSTEMD
                           UPDATE_HEARTBEAT();
#endif
                           my_usleep(200000); /* 0.2 seconds */
                        }
                     }
                     else
                     {
#ifdef WITH_SYSTEMD
                        UPDATE_HEARTBEAT();
#endif
                        my_usleep(200000); /* 0.2 seconds */
                     }
                  }
               }
            }

            for (i = 0; i < kill_counter; i++)
            {
               /* Check if it was caught by waitpid(). */
               if (kill_pos_list[i] != -1)
               {
                  if (kill(kill_list[i], SIGKILL) != -1)
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                                _("Killed process %s for %s (%d) the hard way!"),
#else
                                _("Killed process %s for %s (%lld) the hard way!"),
#endif
                                MON_PROC, pl[kill_pos_list[i]].afd_alias,
                                (pri_pid_t)kill_list[i]);

                     /* Hopefully catch zombie! */
                     my_usleep(100000);
                     (void)waitpid(kill_list[i], NULL, WNOHANG);
#ifdef WITH_SYSTEMD
                     UPDATE_HEARTBEAT();
#endif
                  }
                  pl[kill_pos_list[i]].mon_pid = 0;
                  pl[kill_pos_list[i]].afd_alias[0] = '\0';
                  pl[kill_pos_list[i]].start_time = 0;
                  pl[kill_pos_list[i]].number_of_restarts = 0;
               }
            }
         }
      }

      kill_counter = 0;
      for (i = first; i < last; i++)
      {
         if (pl[i].log_pid > 0)
         {
            if (kill(pl[i].log_pid, SIGINT) == -1)
            {
               if (errno != ESRCH)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                             "Failed to kill %s process to %s (%d) : %s",
#else
                             "Failed to kill %s process to %s (%lld) : %s",
#endif
                             LOG_MON, pl[i].afd_alias,
                             (pri_pid_t)pl[i].log_pid, strerror(errno));
               }
               pl[i].log_pid = 0;
            }
            else
            {
               kill_list[kill_counter] = pl[i].log_pid;
               kill_pos_list[kill_counter] = i;
               kill_counter++;
            }
         }
      }
#ifdef WITH_SYSTEMD
      UPDATE_HEARTBEAT();
#endif
      if (kill_counter > 0)
      {
         int   j;
         pid_t pid;

         /* Give them some time to terminate themself. */
         (void)my_usleep(200000); /* 0.2 seconds */

         /* Catch zombies. */
         if (kill_counter > 0)
         {
            for (i = 0; i < kill_counter; i++)
            {
               if (kill_pos_list[i] != -1)
               {
                  /* Wait at most 15 seconds per process. */
                  for (j = 0; j < 75; j++)
                  {
                     if ((pid = waitpid(kill_list[i], NULL, WNOHANG)) > 0)
                     {
                        if (pid == kill_list[i])
                        {
                           pl[kill_pos_list[i]].log_pid = 0;
                           kill_pos_list[i] = -1;
                           break;
                        }
                        else
                        {
                           int k;

                           for (k = 0; k < kill_counter; k++)
                           {
                              if (pid == kill_list[k])
                              {
                                 pl[kill_pos_list[i]].log_pid = 0;
                                 kill_pos_list[k] = -1;
                                 k = kill_counter;
                              }
                           }
#ifdef WITH_SYSTEMD
                           UPDATE_HEARTBEAT();
#endif
                           my_usleep(200000); /* 0.2 seconds */
                        }
                     }
                     else
                     {
#ifdef WITH_SYSTEMD
                        UPDATE_HEARTBEAT();
#endif
                        my_usleep(200000); /* 0.2 seconds */
                     }
                  }
               }
            }

            for (i = 0; i < kill_counter; i++)
            {
               /* Check if it was caught by waitpid(). */
               if (kill_pos_list[i] != -1)
               {
                  if (kill(kill_list[i], SIGKILL) != -1)
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                                _("Killed process %s for %s (%d) the hard way!"),
#else
                                _("Killed process %s for %s (%lld) the hard way!"),
#endif
                                LOG_MON, pl[kill_pos_list[i]].afd_alias,
                                (pri_pid_t)kill_list[i]);

                     /* Hopefully catch zombie! */
                     my_usleep(100000);
                     (void)waitpid(kill_list[i], NULL, WNOHANG);
#ifdef WITH_SYSTEMD
                     UPDATE_HEARTBEAT();
#endif
                  }
                  pl[kill_pos_list[i]].log_pid = 0;
               }
            }
         }
      }
      free(kill_list);
      free(kill_pos_list);
   }

   if (shutdown == YES)
   {
      int    i;
      size_t length;
      char   *buffer,
             daemon_log[MAX_PATH_LENGTH],
             hostname[64];

      if (p_afd_mon_status != NULL)
      {
         p_afd_mon_status->afd_mon = STOPPED;
      }
      system_log(INFO_SIGN, NULL, 0, "Stopped %s.", AFD_MON);

      if (mon_log_pid > 0)
      {
         int j;

         if (kill(mon_log_pid, SIGINT) == -1)
         {
            if (errno != ESRCH)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                          "Failed to kill monitor log process (%d) : %s",
#else
                          "Failed to kill monitor log process (%lld) : %s",
#endif
                          (pri_pid_t)mon_log_pid, strerror(errno));
            }
         }

         /* Wait for the child to terminate. */
         for (j = 0; j < MAX_SHUTDOWN_TIME;  j++)
         {
            if (waitpid(mon_log_pid, NULL, WNOHANG) == mon_log_pid)
            {
               mon_log_pid = NOT_RUNNING;
               break;
            }
            else
            {
               my_usleep(100000L);
            }
         }
         if (mon_log_pid != NOT_RUNNING)
         {
            if (kill(mon_log_pid, SIGKILL) != -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                          _("Killed %s (%d) the hard way!"),
#else
                          _("Killed %s (%lld) the hard way!"),
#endif
                          MONITOR_LOG, mon_log_pid);
               my_usleep(100000);
               (void)waitpid(mon_log_pid, NULL, WNOHANG);
            }
         }
         mon_log_pid = 0;
      }
      if (p_afd_mon_status != NULL)
      {
         p_afd_mon_status->mon_log = STOPPED;
      }

      if (aldad_pid > 0)
      {
         int j;

         if (kill(aldad_pid, SIGINT) == -1)
         {
            if (errno != ESRCH)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                          "Failed to kill monitor log process (%d) : %s",
#else
                          "Failed to kill monitor log process (%lld) : %s",
#endif
                          (pri_pid_t)aldad_pid, strerror(errno));
            }
         }

         /* Wait for the child to terminate. */
         for (j = 0; j < MAX_SHUTDOWN_TIME;  j++)
         {
            if (waitpid(aldad_pid, NULL, WNOHANG) == aldad_pid)
            {
               aldad_pid = NOT_RUNNING;
               break;
            }
            else
            {
               my_usleep(100000L);
            }
         }
         if (aldad_pid != NOT_RUNNING)
         {
            if (kill(aldad_pid, SIGKILL) != -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                          _("Killed %s (%d) the hard way!"),
#else
                          _("Killed %s (%lld) the hard way!"),
#endif
                          ALDAD, aldad_pid);
               my_usleep(100000);
               (void)waitpid(aldad_pid, NULL, WNOHANG);
            }
         }
         aldad_pid = 0;
      }
      if (p_afd_mon_status != NULL)
      {
         p_afd_mon_status->aldad = STOPPED;
      }

      if (msa != NULL)
      {
         if (msa_detach() != SUCCESS)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to detach from MSA.");
         }
      }

      if (gethostname(hostname, 64) == 0)
      {
         char      date_str[26];
         time_t    now;
         struct tm *p_ts;

         now = time(NULL);
         p_ts = localtime(&now);
         strftime(date_str, 26, "%a %h %d %H:%M:%S %Y", p_ts);
         system_log(CONFIG_SIGN, NULL, 0,
                    "Shutdown on <%s> %s", hostname, date_str);
      }
      system_log(INFO_SIGN, NULL, 0,
                 "=================> SHUTDOWN <=================");

      /* As the last process kill the system log process. */
      if (sys_log_pid > 0)
      {
         int            gotcha = NO;
         fd_set         rset;
         struct timeval timeout;

         /* Give system log some time to tell whether all mon, */
         /* log_mon, etc have been stopped.                    */
         FD_ZERO(&rset);
         i = 0;
         do
         {
            (void)my_usleep(5000L);
            FD_SET(sys_log_fd, &rset);
            timeout.tv_usec = 10000L;
            timeout.tv_sec = 0L;
            i++;
         } while ((select(sys_log_fd + 1, &rset, NULL, NULL, &timeout) > 0) &&
                  (i < 1000));
         (void)my_usleep(10000L);
         (void)kill(sys_log_pid, SIGINT);

         (void)my_usleep(100000L);
         for (i = 0; i < 3; i++)
         {
            if (waitpid(sys_log_pid, NULL, WNOHANG) == sys_log_pid)
            {
               gotcha = YES;
               break;
            }
            else
            {
               (void)my_usleep(100000L);
            }
         }
         if (gotcha == NO)
         {
            (void)kill(sys_log_pid, SIGKILL);
            (void)fprintf(stderr,
#if SIZEOF_PID_T == 4
                          "Killed process %s (%d) the hard way. (%s %d)\n",
#else
                          "Killed process %s (%lld) the hard way. (%s %d)\n",
#endif
                          MON_SYS_LOG, (pri_pid_t)sys_log_pid,
                          __FILE__, __LINE__);
            my_usleep(100000);
            (void)waitpid(sys_log_pid, NULL, WNOHANG);
         }
         sys_log_pid = 0;
      }

      if (daemon_log_fd == -1)
      {
         /* At this point writting to sys log Fifo is dangerous   */
         /* since it will block because there is no reader. Lets  */
         /* try to write to $AFD_WORK_DIR/log/DAEMON_LOG.afd_mon. */
         /* If this somehow fails, write to STDERR_FILENO.        */
         sleep_sys_log_fd = sys_log_fd;
         (void)snprintf(daemon_log, MAX_PATH_LENGTH, "%s%s/DAEMON_LOG.%s",
                        p_work_dir, LOG_DIR, AFD_MON);
         if ((daemon_log_fd = coe_open(daemon_log,
                                       O_CREAT | O_APPEND | O_WRONLY,
                                       (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH))) == -1)
         {
            (void)fprintf(stderr, _("Failed to coe_open() `%s' : %s (%s %d)\n"),
                          daemon_log, strerror(errno), __FILE__, __LINE__);
            sys_log_fd = STDERR_FILENO;
         }
         else
         {
            sys_log_fd = daemon_log_fd;
         }
      }

      if (p_afd_mon_status != NULL)
      {
         p_afd_mon_status->mon_sys_log = STOPPED;
#ifdef HAVE_MMAP
         if (msync((void *)p_afd_mon_status, sizeof(struct afd_mon_status),
                    MS_SYNC) == -1)
         {
            (void)fprintf(stderr, "msync() error : %s\n", strerror(errno));
         }
         if (munmap((void *)p_afd_mon_status, sizeof(struct afd_mon_status)) == -1)
         {
            (void)fprintf(stderr, "munmap() error : %s\n", strerror(errno));
         }
#else
         if (munmap_emu((void *)p_afd_mon_status) == -1)
         {
            (void)fprintf(stderr, "munmap_emu() error : %s\n", strerror(errno));
         }
#endif
         p_afd_mon_status = NULL;
      }

      if (service_name == NULL)
      {
         length = 38 + AFD_LENGTH;
      }
      else
      {
         length = 44 + AFD_LENGTH + strlen(service_name);
      }
      if ((buffer = malloc(length + 1)) != NULL)
      {
         time_t current_time;

         current_time = time(NULL);
         (void)memset(buffer, '-', length);
         buffer[length] = '\0';
         if (service_name == NULL)
         {
            (void)fprintf(stderr,
                          _("%.24s : %s terminated (%s %d)\n%s\n"),
                          ctime(&current_time), AFD_MON, __FILE__,
                          __LINE__, buffer);
         }
         else
         {
            (void)fprintf(stderr,
                          _("%.24s : %s for %s terminated (%s %d)\n%s\n"),
                          ctime(&current_time), AFD_MON, service_name,
                          __FILE__, __LINE__, buffer);
         }
         (void)free(buffer);
      }

      if (got_shuttdown_message == YES)
      {
         /* Tell 'mafd' that we received shutdown message */
         if (send_cmd(ACKN, mon_resp_fd) < 0)
         {
            (void)fprintf(stderr,
                          "Failed to send ACKN : %s\n", strerror(errno));
         }
      }
   }

   return;
}


/*########################## stop_log_process() #########################*/
void
stop_log_process(int process)
{
   if (kill(pl[process].log_pid, SIGINT) == -1)
   {
      if (errno != ESRCH)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                    "Failed to kill %s process to %s (%d) : %s",
#else
                    "Failed to kill %s process to %s (%lld) : %s",
#endif
                    LOG_MON, pl[process].afd_alias,
                    (pri_pid_t)pl[process].log_pid, strerror(errno));
      }
   }
   else
   {
      int   j;
      pid_t pid;

      /* Wait at most 15 seconds per process. */
      for (j = 0; j < 75; j++)
      {
         if ((pid = waitpid(pl[process].log_pid, NULL, WNOHANG)) > 0)
         {
            if (pid == pl[process].log_pid)
            {
               pl[process].log_pid = 0;
               break;
            }
            else
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                          "Caught another zombie %d?",
#else
                          "Caught another zombie %lld?",
#endif
                          (pri_pid_t)pid);
#ifdef WITH_SYSTEMD
               UPDATE_HEARTBEAT();
#endif
               my_usleep(200000); /* 0.2 seconds */
            }
         }
         else
         {
#ifdef WITH_SYSTEMD
            UPDATE_HEARTBEAT();
#endif
            my_usleep(200000); /* 0.2 seconds */
         }
      }

      if (pl[process].log_pid != 0)
      {
         /* Assume process hangs, so kill it hard. */
         if (kill(pl[process].log_pid, SIGKILL) != -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                       _("Killed process %s for %s (%d) the hard way!"),
#else
                       _("Killed process %s for %s (%lld) the hard way!"),
#endif
                       LOG_MON, pl[process].afd_alias,
                       (pri_pid_t)pl[process].log_pid);

            /* Hopefully catch zombie! */
            my_usleep(100000);
            (void)waitpid(pl[process].log_pid, NULL, WNOHANG);
#ifdef WITH_SYSTEMD
            UPDATE_HEARTBEAT();
#endif
         }
         pl[process].log_pid = 0;
      }
   }

   return;
}
