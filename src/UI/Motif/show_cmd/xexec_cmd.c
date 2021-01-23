/*
 *  xexec_cmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2021 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   xexec_cmd - executes a shell command
 **
 ** SYNOPSIS
 **   void xexec_cmd(char *cmd)
 **
 ** DESCRIPTION
 **   xexec_cmd() executes a command specified in 'cmd' by calling
 **   /bin/sh -c 'cmd', and returns after the command has been
 **   completed. All output is written to the text widget cmd_output.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to execute the command 'cmd'.
 **   In buffer will be the results of STDOUT and STDERR.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   05.12.1999 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* strlen()                        */
#include <stdlib.h>                   /* exit()                          */
#include <unistd.h>                   /* read(), close(), STDOUT_FILENO, */
                                      /* STDERR_FILENO                   */
#include <fcntl.h>                    /* O_NONBLOCK                      */
#include <sys/types.h>
#include <sys/wait.h>                 /* wait()                          */
#include <errno.h>
#include <Xm/Xm.h>
#include <Xm/Text.h>
#include "show_cmd.h"

#define  READ  0
#define  WRITE 1

/* External global variables. */
extern int            cmd_fd,
                      go_to_beginning,
                      no_repeat_button;
extern pid_t          cmd_pid;
extern Display        *display;
extern Widget         appshell,
                      cmd_output;
extern XmTextPosition wpr_position;
extern XtInputId      cmd_input_id;

/* Local function prototypes. */
static void           read_data(XtPointer, int *, XtInputId *);


/*############################# xexec_cmd() #############################*/
void
xexec_cmd(char *cmd)
{
   int channels[2];

   if (cmd_pid > 0)
   {
      if (wait(NULL) == -1)
      {
         (void)fprintf(stderr, "wait() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      cmd_pid = 0;
      if (close(cmd_fd) == -1)
      {
         (void)fprintf(stderr, "close() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
      }
   }

   if (pipe(channels) == -1)
   {
      (void)fprintf(stderr, "pipe() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   switch (cmd_pid = fork())
   {
      case -1: /* Failed to fork. */
         (void)fprintf(stderr, "fork() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);

      case 0 : /* Child process. */
         (void)close(channels[READ]);
         dup2(channels[WRITE], STDOUT_FILENO);
         dup2(channels[WRITE], STDERR_FILENO);
         (void)execl("/bin/sh", "sh", "-c", cmd, (char *)0);
         _exit(INCORRECT);

      default: /* Parent process. */
         (void)close(channels[WRITE]);

         cmd_fd = channels[READ];
         cmd_input_id = XtAppAddInput(XtWidgetToApplicationContext(appshell),
                                      channels[READ],
                                      (XtPointer)XtInputReadMask,
                                      (XtInputCallbackProc)read_data,
                                      (XtPointer)NULL);
   }

   return;
}


/*++++++++++++++++++++++++++++ read_data() ++++++++++++++++++++++++++++++*/
static void
read_data(XtPointer client_data, int *fd, XtInputId *id)
{
   int  n;
   char buffer[4096 + 1];

   if ((n = read(*fd, buffer, 4096)) > 0)
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
           if (no_repeat_button == NO)
           {
              buffer[0] = '>';
              buffer[1] = '\0';
              XmTextInsert(cmd_output, wpr_position, buffer);
           }
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
