/*
 *  print_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   print_data - prints data from the input log
 **
 ** SYNOPSIS
 **   void print_data(void)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   18.03.2000 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>               /* snprintf()                           */
#include <string.h>              /* strcpy(), strcat(), strerror()       */
#include <stdlib.h>              /* exit()                               */
#include <unistd.h>              /* write(), close()                     */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <errno.h>
#include "mafd_ctrl.h"
#include "show_cmd.h"

/* External global variables. */
extern Widget      cmd_output,
                   printshell,
                   statusbox_w;
extern char        file_name[];
extern XT_PTR_TYPE device_type,
                   range_type;
extern FILE        *fp;


/*######################### print_data_button() #########################*/
void
print_data_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   int  fd,
        prepare_status;
   char message[MAX_MESSAGE_LENGTH],
        *p_buffer;

   if (device_type == PRINTER_TOGGLE)
   {
      prepare_status = prepare_printer(&fd);
   }
   else
   {
      prepare_status = prepare_file(&fd, (device_type == MAIL_TOGGLE) ? 0 : 1);
      if ((prepare_status != SUCCESS) && (device_type == MAIL_TOGGLE))
      {
         prepare_tmp_name();
         prepare_status = prepare_file(&fd, 1);
      }
   }
   if (prepare_status == SUCCESS)
   {
      int length;

      if (range_type == SELECTION_TOGGLE)
      {
         if ((p_buffer = XmTextGetSelection(cmd_output)) != NULL)
         {
            length = strlen(p_buffer);
            if (write(fd, p_buffer, length) != length)
            {
               (void)fprintf(stderr, "write() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
               XtFree(p_buffer);
               exit(INCORRECT);
            }
            XtFree(p_buffer);
            XmTextClearSelection(cmd_output, CurrentTime);
         }
      }
      else
      {
         if ((p_buffer = XmTextGetString(cmd_output)) != NULL)
         {
            length = strlen(p_buffer);
            if (write(fd, p_buffer, length) != length)
            {
               (void)fprintf(stderr, "write() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
               XtFree(p_buffer);
               exit(INCORRECT);
            }
            XtFree(p_buffer);
         }
      }
      if (device_type == PRINTER_TOGGLE)
      {
         char buf;

         /* Send Control-D to printer queue. */
         buf = CONTROL_D;
         if (write(fd, &buf, 1) != 1)
         {
            (void)fprintf(stderr, "write() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }

         if ((prepare_status = pclose(fp)) < 0)
         {
            (void)snprintf(message, MAX_MESSAGE_LENGTH,
                           "Failed to send printer command (%d) : %s",
                           prepare_status, strerror(errno));
         }
         else
         {
            (void)snprintf(message, MAX_MESSAGE_LENGTH,
                           "Send job to printer (%d)", prepare_status);
         }
      }
      else
      {
         if (close(fd) < 0)
         {
            (void)fprintf(stderr, "close() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
         }
         if (device_type == MAIL_TOGGLE)
         {
            send_mail_cmd(message, MAX_MESSAGE_LENGTH);
         }
         else
         {
            (void)snprintf(message, MAX_MESSAGE_LENGTH,
                           "Send job to file %s.", file_name);
         }
      }
   }

   show_message(statusbox_w, message);
   XtPopdown(printshell);

   return;
}
