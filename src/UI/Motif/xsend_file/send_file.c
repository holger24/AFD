/*
 *  send_file.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 - 2015 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   send_file - sends files by executing one of the aXXX programs
 **
 ** SYNOPSIS
 **   void send_file(void)
 **
 ** DESCRIPTION
 **   send_file() executes a command specified in 'cmd' by calling
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
 **   31.01.2005 H.Kiehl Adapted from xexec_cmd().
 **   02.01.2019 A.Maul  Add parameter for LOCK_OFF to 'cmd'.
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
#include <Xm/Label.h>
#include "xsend_file.h"
#include "ftpdefs.h"

#define  READ  0
#define  WRITE 1

/* External global variables. */
extern int              button_flag,
                        cmd_fd;
extern char             file_name_file[],
                        url_file_name[];
extern pid_t            cmd_pid;
extern struct send_data *db;
extern Display          *display;
extern Widget           appshell,
                        cmd_output,
                        special_button_w;
extern XmTextPosition   wpr_position;
extern XtInputId        cmd_input_id;

/* Local function prototypes. */
static void             read_data(XtPointer, int *, XtInputId *);


/*############################# send_file() #############################*/
void
send_file(void)
{
   int    channels[2];
   size_t length = 0;
   char   cmd[MAX_PATH_LENGTH];

   /*
    * First put together the command from the input gathered.
    */
   if (db->protocol == FTP)
   {
      length = sprintf(&cmd[length], "%s -c %s -p %d -m %c",
                       AFTP, url_file_name, db->port, (char)db->transfer_mode);
      if (db->mode_flag & PASSIVE_MODE)
      {
         length += sprintf(&cmd[length], " -x");
      }
      if (db->mode_flag & EXTENDED_MODE)
      {
         length += sprintf(&cmd[length], " -X");
      }
      if (db->proxy_name[0] != '\0')
      {
         length += sprintf(&cmd[length], " -P %s", db->proxy_name);
      }
   }
   else if (db->protocol == SFTP)
        {
           length = sprintf(&cmd[length], "%s -c %s -p %d",
                            ASFTP, url_file_name, db->port);
        }
   else if (db->protocol == SMTP)
        {
           length = sprintf(&cmd[length], "%s -c %s -p %d",
                            ASMTP, url_file_name, db->port);
           if (db->attach_file_flag == YES)
           {
              length += sprintf(&cmd[length], " -e");
           }
        }
#ifdef _WITH_WMO_SUPPORT
   else if (db->protocol == WMO)
        {
           length = sprintf(&cmd[length], "%s -c %s -p %d",
                            AWMO, url_file_name, db->port);
        }
#endif
        else
        {
#if SIZEOF_LONG == 4
           (void)fprintf(stderr, "Unknown or not implemented protocol (%d)\n",
#else
           (void)fprintf(stderr, "Unknown or not implemented protocol (%ld)\n",
#endif
                         db->protocol);
           exit(INCORRECT);
        }

   if ((db->protocol != SMTP) && (db->protocol != WMO))
   {
      if (db->lock == SET_LOCK_DOT)
      {
         length += sprintf(&cmd[length], " -l DOT");
      }
      else if (db->lock == SET_LOCK_OFF)
           {
              length += sprintf(&cmd[length], " -l OFF");
           }
      else if (db->lock == SET_LOCK_DOT_VMS)
           {
              length += sprintf(&cmd[length], " -l DOT_VMS");
           }
      else if (db->lock == SET_LOCK_PREFIX)
           {
              length += sprintf(&cmd[length], " -l %s", db->prefix);
           }

      if (db->create_target_dir == YES)
      {
         length += sprintf(&cmd[length], " -C");
      }
   }
   if (db->subject[0] != '\0')
   {
      length += sprintf(&cmd[length], " -s \"%s\"", db->subject);
   }
   if (db->debug == YES)
   {
      length += sprintf(&cmd[length], " -v");
   }
#if SIZEOF_TIME_T == 4
   (void)sprintf(&cmd[length], " -t %ld -f %s",
#else
   (void)sprintf(&cmd[length], " -t %lld -f %s",
#endif
                 (pri_time_t)db->timeout, file_name_file);

#ifdef DEBUG_SHOW_CMD
   printf("cmd=%s\n", cmd);
#endif

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
   char buffer[MAX_PATH_LENGTH + 1];

   if ((n = read(*fd, buffer, MAX_PATH_LENGTH)) > 0)
   {
      buffer[n] = '\0';
      XmTextInsert(cmd_output, wpr_position, buffer);
      wpr_position += n;
      XmTextShowPosition(cmd_output, wpr_position);
      XFlush(display);
   }
   else if (n == 0)
        {
           buffer[0] = '>';
           buffer[1] = '\0';
           XmTextInsert(cmd_output, wpr_position, buffer);
           XmTextShowPosition(cmd_output, wpr_position);
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
           if (button_flag == STOP_BUTTON)
           {
              XmString xstr;

              /* Set button back to Send. */
              xstr = XmStringCreateLtoR("Send", XmFONTLIST_DEFAULT_TAG);
              XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
              XmStringFree(xstr);
              button_flag = SEND_BUTTON;
           }
        }

   return;
}
