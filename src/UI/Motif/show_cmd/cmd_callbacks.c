/*
 *  cmd_callbacks.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   cmd_callbacks - all callback functions for module show_cmd
 **
 ** SYNOPSIS
 **   cmd_callbacks
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   04.12.1999 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                  /* popen(), pclose()                 */
#include <string.h>
#include <stdlib.h>                 /* exit()                            */
#include <sys/types.h>
#include <sys/wait.h>               /* wait()                            */
#include <unistd.h>                 /* read(), close()                   */
#include <signal.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <errno.h>
#include "mafd_ctrl.h"
#include "show_cmd.h"

/* External global variables. */
extern int            cmd_fd,
                      go_to_beginning;
extern pid_t          cmd_pid;
extern char           cmd[];
extern Display        *display;
extern Widget         cmd_output,
                      statusbox_w;
extern XtInputId      cmd_input_id;
extern XmTextPosition wpr_position;
extern XtAppContext   app;

/* Local function prototypes. */
static void           catch_child(Widget),
                      kill_child(Widget),
                      repeat_cmd(Widget);


/*############################ close_button() ###########################*/
void
close_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   if (cmd_pid > 0)
   {
      if (kill(cmd_pid, SIGINT) == -1)
      {
         (void)fprintf(stderr,
#if SIZEOF_PID_T == 4
                       "Failed to kill() process %d : %s (%s %d)\n",
#else
                       "Failed to kill() process %lld : %s (%s %d)\n",
#endif
                       (pri_pid_t)cmd_pid, strerror(errno),
                       __FILE__, __LINE__);
      }
   }

   exit(SUCCESS);
}


/*############################ print_button() ###########################*/
void
print_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   reset_message(statusbox_w);
   print_data(w, client_data, call_data);

   return;
}


/*########################## repeat_button() ############################*/
void
repeat_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   if (cmd_pid > 0)
   {
      XtAppAddTimeOut(app, 0, (XtTimerCallbackProc)kill_child, cmd_output);
   }

   XtAppAddTimeOut(app, 0, (XtTimerCallbackProc)repeat_cmd, cmd_output);

   return;
}


/*++++++++++++++++++++++++++++ kill_child() +++++++++++++++++++++++++++++*/
static void
kill_child(Widget w)
{
   if (cmd_pid > 0)
   {
      pid_t pid = cmd_pid;

      XtRemoveInput(cmd_input_id);
      cmd_input_id = 0L;
      if (kill(pid, SIGINT) == -1)
      {
         (void)fprintf(stderr,
#if SIZEOF_PID_T == 4
                       "Failed to kill() process %d : %s (%s %d)\n",
#else
                       "Failed to kill() process %lld : %s (%s %d)\n",
#endif
                       (pri_pid_t)cmd_pid, strerror(errno), __FILE__, __LINE__);
      }
      else
      {
         catch_child(cmd_output);
      }
   }

   return;
}


/*--------------------------- catch_child() -----------------------------*/
static void
catch_child(Widget w)
{
   int   n;
   char  buffer[4096 + 3];

   if ((n = read(cmd_fd, buffer, 4096)) > 0)
   {
      buffer[n] = '\0';
      XmTextInsert(cmd_output, wpr_position, buffer);
      wpr_position += n;
      if (go_to_beginning == NO)
      {
         XmTextShowPosition(cmd_output, wpr_position);
      }
      XFlush(display);
   }
   else if (n == 0)
        {
           buffer[0] = '>';
           buffer[1] = '\0';
           XmTextInsert(cmd_output, wpr_position, buffer);
           if (go_to_beginning == NO)
           {
              XmTextShowPosition(cmd_output, wpr_position);
           }
           else
           {
              XmTextShowPosition(cmd_output, 0);
           }
           XFlush(display);
           if (cmd_pid > 0)
           {
              if (wait(NULL) == -1)
              {
                 (void)fprintf(stderr, "wait() error : %s (%s %d)\n",
                               strerror(errno), __FILE__, __LINE__);
                 exit(INCORRECT);
              }
              cmd_pid = 0;
           }
           wpr_position = 0;
           if (cmd_input_id != 0L)
           {
              XtRemoveInput(cmd_input_id);
              cmd_input_id = 0L;
              if (close(cmd_fd) == -1)
              {
                 (void)fprintf(stderr, "close() error : %s (%s %d)\n",
                               strerror(errno), __FILE__, __LINE__);
              }
           }
        }

   return;
}


/*++++++++++++++++++++++++++++ repeat_cmd() +++++++++++++++++++++++++++++*/
static void
repeat_cmd(Widget w)
{
   XmTextSetInsertionPosition(cmd_output, 0);
   XmTextSetString(cmd_output, NULL);
   XFlush(display);
   wpr_position = 0;

   xexec_cmd(cmd);

   return;
}
