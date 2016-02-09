/*
 *  stop_process.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2010 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **
 */
DESCR__E_M3

#include <string.h>            /* strerror()                             */
#include <sys/types.h>
#include <sys/wait.h>          /* waitpid()                              */
#include <signal.h>            /* kill()                                 */
#include <errno.h>
#include "mondefs.h"

/* External global variables */
extern int                 mon_resp_fd,
                           no_of_afds;
extern struct process_list *pl;


/*############################ stop_process() ###########################*/
void
stop_process(int process, int shutdown)
{
   int status;

   if (pl != NULL)
   {
      int first, last, i;

      if (process == -1)
      {
         first = 0;
         last = no_of_afds;
      }
      else
      {
         if (process < no_of_afds)
         {
            first = process;
            last = process + 1;
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Hmm, position in MSA to large [%d %d]",
                       process, no_of_afds);
            return;
         }
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
            }
            else
            {
               if (waitpid(pl[i].mon_pid, &status, 0) == pl[i].mon_pid)
               {
                  pl[i].mon_pid = 0;
                  pl[i].afd_alias[0] = '\0';
                  pl[i].start_time = 0;
                  pl[i].number_of_restarts = 0;

                  if ((shutdown == YES) && (mon_resp_fd > 0))
                  {
                     if (send_cmd(PROC_TERM, mon_resp_fd) < 0)
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   "Failed to send PROC_TERM : %s",
                                   strerror(errno));
                     }
                  }
               }
            }
         }
         if (pl[i].log_pid > 0)
         {
            stop_log_process(i);
         }
      }
   }

   return;
}


/*########################## stop_log_process() #########################*/
void
stop_log_process(int process)
{
   int status;

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
      if (waitpid(pl[process].log_pid, &status, 0) == pl[process].log_pid)
      {
         pl[process].log_pid = 0;
      }
   }

   return;
}
