/*
 *  make_xprocess.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   make_xprocess - creates a new process
 **
 ** SYNOPSIS
 **   void make_xprocess(char *progname,
 **                      char *real_progname,
 **                      char **args,
 **                      int  position)
 **
 ** DESCRIPTION
 **   This function forks to start another process. The process id
 **   as well as the process name is held in the structure apps_list.
 **   For each process that is started with this function a
 **   XtAppAddInput() handler is started, that removes the process
 **   from the structure apps_list.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   06.09.1998 H.Kiehl Created
 **   24.01.1999 H.Kiehl If command (progname, args) returns anything
 **                      it is shown with warn dialog.
 **   16.08.2004 H.Kiehl Replace XtAppAddInput() with XtAppAddTimeOut()
 **                      since it does not work reliably on all platforms.
 **
 */
DESCR__E_M3

#include <string.h>         /* strerror(), memmove()                     */
#include <unistd.h>         /* fork(), read(), close()                   */
#include <stdlib.h>         /* realloc(), free()                         */
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>       /* waitpid()                                 */
#ifdef WITH_MEMCHECK
# include <mcheck.h>
#endif
#include <errno.h>

#include <Xm/Xm.h>
#include "motif_common_defs.h"

/* #define SHOW_FULL_CMD */

/* External global variables. */
extern int              no_of_active_process;
extern Widget           appshell;
extern XtAppContext     app;
extern struct apps_list *apps_list;

/* Local function prototypes. */
static void             check_zombies(void);


/*+++++++++++++++++++++++++++ make_xprocess() +++++++++++++++++++++++++++*/
void
make_xprocess(char *progname, char *real_progname, char **args, int position)
{
   pid_t pid;
#ifdef SHOW_FULL_CMD
   int   i = 0;

   while (args[i] != NULL)
   {
      (void)fprintf(stderr, "%s ", args[i]);
      i++;
   }
   (void)fprintf(stderr, "\n");
#endif

   if ((no_of_active_process % 10) == 0)
   {
      size_t new_size = ((no_of_active_process / 10) + 1) *
                        10 * sizeof(struct apps_list);
      struct apps_list *tmp_apps_list;

      if ((tmp_apps_list = realloc(apps_list, new_size)) == NULL)
      {
         int tmp_errno = errno;

         free(apps_list);
         (void)xrec(FATAL_DIALOG, "realloc() error : %s (%s %d)\n",
                    strerror(tmp_errno), __FILE__, __LINE__);
         return;
      }
      apps_list = tmp_apps_list;
   }

   if ((pid = fork()) < 0)
   {
      (void)xrec(FATAL_DIALOG, "Failed to fork() : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }
   else if (pid == 0)
        {
#ifdef WITH_MEMCHECK
           muntrace();
#endif
           if (((progname[0] == 'r') || (progname[0] == 's')) &&
               (progname[1] == 's') &&
               (progname[2] == 'h') &&
               (progname[3] == '\0'))
           {
              if (freopen("/dev/null", "r+", stdin) == NULL)
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to redirect stdin to /dev/null : %s",
                            strerror(errno));
              }
           }

           /* Child process. */
           (void)execvp(progname, args); /* NOTE: NO return from execvp(). */

           _exit(INCORRECT);
        }

   apps_list[no_of_active_process].pid = pid;
   apps_list[no_of_active_process].position = position;
   (void)strcpy(apps_list[no_of_active_process].progname, real_progname);
   no_of_active_process++;
   if (no_of_active_process == 1)
   {
      check_zombies();
   }

   return;
}


/*-------------------------- check_zombies() ----------------------------*/
static void
check_zombies(void)
{
   int i;

   for (i = 0; i < no_of_active_process; i++)
   {
      if (waitpid(apps_list[i].pid, NULL, WNOHANG) == apps_list[i].pid)
      {
         no_of_active_process--;
         if (no_of_active_process == 0)
         {
            if (apps_list != NULL)
            {
               free(apps_list);
               apps_list = NULL;
            }
            break;
         }
         else
         {
            if (i != -1)
            {
               if (i != no_of_active_process)
               {
                  size_t move_size = (no_of_active_process - i) *
                                     sizeof(struct apps_list);

                  (void)memmove(&apps_list[i], &apps_list[i + 1], move_size);
               }
            }
         }
         i--;
      }
   }
   if (no_of_active_process > 0)
   {
      (void)XtAppAddTimeOut(app, ZOMBIE_CHECK_INTERVAL,
                            (XtTimerCallbackProc)check_zombies, appshell);
   }
   return;
}
