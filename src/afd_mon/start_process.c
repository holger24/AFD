/*
 *  start_process.c - Part of AFD, an automatic file distribution program.
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
 **   start_process - starts a process
 **
 ** SYNOPSIS
 **   pid_t start_process(char *progname, int afd)
 **   pid_t start_log_process(int afd, unsigned int log_capabilities)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.08.1998 H.Kiehl Created
 **   21.03.2007 H.Kiehl Added start_log_process().
 **   22.04.2023 H.Kiehl Add parameter to aldad.
 **
 */
DESCR__E_M3

#include <string.h>            /* strerror()                             */
#include <time.h>              /* time()                                 */
#include <unistd.h>            /* fork(), execlp(), _exit()              */
#include <errno.h>
#include "mondefs.h"

/* External global variables. */
extern int                 in_child;
extern char                *p_work_dir;
extern struct process_list *pl;


/*########################### start_process() ###########################*/
pid_t
start_process(char *progname, int afd)
{
   pid_t pid;

   switch (pid = fork())
   {
      case -1 : /* Error creating process. */

         /* Could not generate process */
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not create a new process : %s", strerror(errno));
         return(INCORRECT);

      case  0 : /* Child process. */

         in_child = YES;
         if (afd == -1)
         {
            (void)execlp(progname, progname, WORK_DIR_ID,
                         p_work_dir, (char *)0);
         }
         else if (afd == -2)
              {
                 (void)execlp(progname, progname, "--afdmon", WORK_DIR_ID,
                              p_work_dir, (char *)0);
              }
              else
              {
                 char str_afd_num[MAX_INT_LENGTH];

                 (void)sprintf(str_afd_num, "%d", afd);
                 (void)execlp(progname, progname, WORK_DIR_ID,
                                p_work_dir, str_afd_num, (char *)0);
              }
         _exit(INCORRECT);

      default : /* Parent process. */

         return(pid);
   }
}


/*######################### start_log_process() #########################*/
void
start_log_process(int afd, unsigned int log_capabilities)
{
   switch (pl[afd].log_pid = fork())
   {
      case -1 : /* Error creating process. */

         /* Could not generate process */
         pl[afd].next_retry_time_log = time(NULL) + RETRY_INTERVAL;
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not create a new process : %s", strerror(errno));
         return;

      case  0 : /* Child process. */

         {
            char str_afd_num[MAX_INT_LENGTH],
                 str_lc_num[MAX_INT_LENGTH];

            in_child = YES;
            (void)sprintf(str_afd_num, "%d", afd);
            (void)sprintf(str_lc_num, "%u", log_capabilities);
            (void)execlp(LOG_MON, LOG_MON, WORK_DIR_ID, p_work_dir,
                         str_afd_num, str_lc_num, (char *)0);
         }
         _exit(INCORRECT);

      default : /* Parent process. */

         pl[afd].next_retry_time_log = 0L;
         return;
   }
}
