/*
 *  stop_process.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2021 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **
 */
DESCR__E_M3

#include <string.h>            /* strerror()                             */
#include <sys/types.h>
#ifdef WITH_SYSTEMD
# include <systemd/sd-daemon.h>  /* sd_notify()                          */
#endif
#include <sys/wait.h>          /* waitpid()                              */
#include <signal.h>            /* kill()                                 */
#include <stdlib.h>            /* malloc(), free()                       */
#include <errno.h>
#include "mondefs.h"

/* External global variables */
extern int                 mon_resp_fd,
                           no_of_afds;
#ifdef WITH_SYSTEMD
extern int                 systemd_watchdog_enabled;
#endif
extern struct process_list *pl;


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
            last,
            status;
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
         (void)my_usleep(100000);

         /* Catch zombies. */
         if (kill_counter > 0)
         {
            for (i = 0; i < kill_counter; i++)
            {
               if (kill_pos_list[i] != -1)
               {
                  for (j = 0; j < 9; j++)
                  {
                     if ((pid = waitpid(kill_list[i], NULL, WNOHANG)) > 0)
                     {
                        if (pid == kill_list[i])
                        {
                           pl[kill_pos_list[i]].mon_pid = 0;
                           pl[kill_pos_list[i]].afd_alias[0] = '\0';
                           pl[kill_pos_list[i]].start_time = 0;
                           pl[kill_pos_list[i]].number_of_restarts = 0;

                           if ((shutdown == YES) && (mon_resp_fd > 0) &&
                               (process != -1))
                           {
                              if (send_cmd(PROC_TERM, mon_resp_fd) < 0)
                              {
                                 system_log(WARN_SIGN, __FILE__, __LINE__,
                                            "Failed to send PROC_TERM : %s",
                                            strerror(errno));
                              }
                           }
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

                                 if ((shutdown == YES) && (mon_resp_fd > 0) &&
                                     (process != -1))
                                 {
                                    if (send_cmd(PROC_TERM, mon_resp_fd) < 0)
                                    {
                                       system_log(WARN_SIGN, __FILE__, __LINE__,
                                                  "Failed to send PROC_TERM : %s",
                                                  strerror(errno));
                                    }
                                 }
                                 kill_pos_list[k] = -1;
                                 k = kill_counter;
                              }
                           }
#ifdef WITH_SYSTEMD
                           UPDATE_HEARTBEAT();
#endif
                           my_usleep(100000);
                        }
                     }
                     else
                     {
#ifdef WITH_SYSTEMD
                        UPDATE_HEARTBEAT();
#endif
                        my_usleep(100000);
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
                     (void)waitpid(kill_list[i], &status, 0);
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
         (void)my_usleep(100000);

         /* Catch zombies. */
         if (kill_counter > 0)
         {
            for (i = 0; i < kill_counter; i++)
            {
               if (kill_pos_list[i] != -1)
               {
                  for (j = 0; j < 9; j++)
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
                           my_usleep(100000);
                        }
                     }
                     else
                     {
#ifdef WITH_SYSTEMD
                        UPDATE_HEARTBEAT();
#endif
                        my_usleep(100000);
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
                     (void)waitpid(kill_list[i], &status, 0);
                  }
                  pl[kill_pos_list[i]].log_pid = 0;
               }
            }
         }
      }
      free(kill_list);
      free(kill_pos_list);
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

      for (j = 0; j < 9; j++)
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
               my_usleep(100000);
            }
         }
         else
         {
#ifdef WITH_SYSTEMD
            UPDATE_HEARTBEAT();
#endif
            my_usleep(100000);
         }
      }

      if (pl[process].log_pid != 0)
      {
         int status;

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
            (void)waitpid(pl[process].log_pid, &status, 0);
         }
         pl[process].log_pid = 0;
      }
   }

   return;
}
