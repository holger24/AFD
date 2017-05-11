/*
 *  make_process_amg.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2017 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   make_process_amg - generates a new process
 **
 ** SYNOPSIS
 **   pid_t make_process_amg(char   *work_dir,
 **                          char   *prog_name,
 **                          int    rescan_time,
 **                          int    max_process,
 **                          mode_t create_source_dir_mode,
 **                          pid_t  udc_pid)
 **
 ** DESCRIPTION
 **   The function make_process_amg() allows the AMG to generate new
 **   processes.
 **
 ** RETURN VALUES
 **   Returns SUCCESS when process creation was successful,
 **   otherwise INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.08.1995 H.Kiehl Created
 **   27.03.1998 H.Kiehl Put this function into a separate file.
 **
 */
DESCR__E_M3

#include <stdio.h>              /* sprintf()                             */
#include <string.h>             /* strerror()                            */
#include <stdlib.h>             /* exit()                                */
#include <sys/types.h>
#include <unistd.h>             /* fork(), exit()                        */
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int no_of_local_dirs;


/*########################## make_process_amg() #########################*/
pid_t
make_process_amg(char   *work_dir,
                 char   *prog_name,
                 int    rescan_time,
                 int    max_process,
                 mode_t create_source_dir_mode,
                 pid_t  udc_pid)
{
   pid_t proc_id;
   char  rt_str[MAX_INT_LENGTH],
         mp_str[MAX_INT_LENGTH],
         nd_str[MAX_INT_LENGTH],
         sd_str[MAX_LONG_LENGTH],
         up_str[MAX_PID_T_LENGTH];

   /* First convert int's into a char string. */
   (void)snprintf(rt_str, MAX_INT_LENGTH, "%d", rescan_time);
   (void)snprintf(mp_str, MAX_INT_LENGTH, "%d", max_process);
   (void)snprintf(nd_str, MAX_INT_LENGTH, "%d", no_of_local_dirs);
   (void)snprintf(sd_str, MAX_LONG_LENGTH, "%ld", (long)create_source_dir_mode);
#if SIZEOF_PID_T == 4
   (void)snprintf(up_str, MAX_PID_T_LENGTH, "%d", (pri_pid_t)udc_pid);
#else
   (void)snprintf(up_str, MAX_PID_T_LENGTH, "%lld", (pri_pid_t)udc_pid);
#endif

   switch (proc_id = fork())
   {
      case -1: /* Could not generate process. */
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "Could not create a new process : %s",
                          strerror(errno));
               exit(INCORRECT);

      case  0: /* Child process. */
               if (execlp(prog_name, prog_name, work_dir,
                          rt_str, mp_str, nd_str, sd_str,
                          up_str, (char *)0) < 0)
               {                                                
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to start process %s : %s",
                             prog_name, strerror(errno));
                  _exit(INCORRECT);
               }
               exit(SUCCESS);

      default: /* Parent process. */
               return(proc_id);
   }

   return(INCORRECT);
}
