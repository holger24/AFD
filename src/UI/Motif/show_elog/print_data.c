/*
 *  print_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 - 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   print_data - prints data from the AFD event log
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
 **   01.07.2007 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>               /* snprintf(), pclose()                 */
#include <string.h>              /* strcpy(), strcat(), strerror()       */
#include <stdlib.h>              /* exit()                               */
#include <unistd.h>              /* write(), close()                     */
#include <time.h>                /* strftime()                           */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <errno.h>
#include "motif_common_defs.h"
#include "show_elog.h"

/* External global variables. */
extern Widget         outputbox_w,
                      printshell,
                      statusbox_w;
extern XmTextPosition wpr_position;
extern XT_PTR_TYPE    device_type,
                      toggles_set;
extern int            items_selected,
                      no_of_search_dir_alias,
                      no_of_search_host_alias;
extern time_t         start_time_val,
                      end_time_val;
extern char           file_name[],
                      heading_line[],
                      **search_dir_alias,
                      **search_host_alias,
                      sum_sep_line[];
extern FILE           *fp;

/* Local function prototypes. */
static void           write_header(int);


/*######################### print_data_button() #########################*/
void
print_data_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   int  fd,
        prepare_status;
   char message[MAX_MESSAGE_LENGTH],
        *text_buffer;

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
      write_header(fd);

      if ((text_buffer = XmTextGetString(outputbox_w)))
      {
         if (write(fd, text_buffer, wpr_position) != wpr_position)
         {
            (void)fprintf(stderr, "write() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            XtFree(text_buffer);
            exit(INCORRECT);
         }
      }

      if (device_type == PRINTER_TOGGLE)
      {
         int  status;
         char buf;

         /* Send Control-D to printer queue. */
         buf = CONTROL_D;
         if (write(fd, &buf, 1) != 1)
         {
            (void)fprintf(stderr, "write() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }

         if ((status = pclose(fp)) < 0)
         {
            (void)snprintf(message, MAX_MESSAGE_LENGTH,
                           "Failed to send printer command (%d) : %s",
                           status, strerror(errno));
         }
         else
         {
            (void)snprintf(message, MAX_MESSAGE_LENGTH,
                           "Send job to printer (%d)", status);
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


/*--------------------------- write_header() ----------------------------*/
static void
write_header(int fd)
{
   int  length;
   char buffer[1024];

   length = snprintf(buffer, 1024,
                     "                                AFD EVENT LOG\n\n");
   if ((start_time_val < 0) && (end_time_val < 0))
   {
      length += snprintf(&buffer[length], 1024 - length,
                         "\tTime Interval : earliest entry - latest entry\n");
   }
   else if ((start_time_val > 0) && (end_time_val < 0))
        {
           length += strftime(&buffer[length], 1024 - length,
                              "\tTime Interval : %m.%d. %H:%M",
                              localtime(&start_time_val));
           length += snprintf(&buffer[length], 1024 - length,
                              " - latest entry\n");
        }
        else if ((start_time_val < 0) && (end_time_val > 0))
             {
                length += strftime(&buffer[length], 1024 - length,
                                   "\tTime Interval : earliest entry - %m.%d. %H:%M\n",
                                   localtime(&end_time_val));
             }
             else
             {
                length += strftime(&buffer[length], 1024 - length,
                                   "\tTime Interval : %m.%d. %H:%M",
                                   localtime(&start_time_val));
                length += strftime(&buffer[length], 1024 - length,
                                   " - %m.%d. %H:%M",
                                   localtime(&end_time_val));
             }
   if (length >= 1024)
   {
      length = 1024;
      goto write_data;
   }

   if (no_of_search_dir_alias > 0)
   {
      int i;

      length += snprintf(&buffer[length], 1024 - length,
                         "\tDir alias     : %s\n", search_dir_alias[0]);
      if (length >= 1024)
      {
         length = 1024;
         goto write_data;
      }
      for (i = 1; i < no_of_search_dir_alias; i++)
      {
         length += snprintf(&buffer[length], 1024 - length,
                            "\t                %s\n", search_dir_alias[i]);
         if (length >= 1024)
         {
            length = 1024;
            goto write_data;
         }
      }
   }
   else
   {
      length += snprintf(&buffer[length], 1024 - length, "\tDir alias     :\n");
      if (length >= 1024)
      {
         length = 1024;
         goto write_data;
      }
   }
   if (no_of_search_host_alias > 0)
   {
      int i;

      length += snprintf(&buffer[length], 1024 - length, "\tHost alias    : %s",
                       search_host_alias[0]);
      if (length >= 1024)
      {
         length = 1024;
         goto write_data;
      }
      for (i = 1; i < no_of_search_host_alias; i++)
      {
         length += snprintf(&buffer[length], 1024 - length, ", %s",
                            search_host_alias[i]);
         if (length >= 1024)
         {
            length = 1024;
            goto write_data;
         }
      }
      if (length == 1024)
      {
         buffer[1023] = '\n';
      }
      else
      {
         buffer[length] = '\n';
         length++;
      }
   }
   else
   {
      length += snprintf(&buffer[length], 1024 - length,
                         "\tHost alias    : \n");
   }
   if (length >= 1024)
   {
      length = 1024;
      goto write_data;
   }

   length += snprintf(&buffer[length], 1024 - length, "\tEvent class   :");
   if (length >= 1024)
   {
      length = 1024;
      goto write_data;
   }
   if (toggles_set & SHOW_CLASS_GLOBAL)
   {
      length += snprintf(&buffer[length], 1024 - length, " Global");
      if (length >= 1024)
      {
         length = 1024;
         goto write_data;
      }
   }
   if (toggles_set & SHOW_CLASS_DIRECTORY)
   {
      length += snprintf(&buffer[length], 1024 - length, " Directory");
      if (length >= 1024)
      {
         length = 1024;
         goto write_data;
      }
   }
   if (toggles_set & SHOW_CLASS_PRODUCTION)
   {
      length += snprintf(&buffer[length], 1024 - length, " Production");
      if (length >= 1024)
      {
         length = 1024;
         goto write_data;
      }
   }
   if (toggles_set & SHOW_CLASS_HOST)
   {
      length += snprintf(&buffer[length], 1024 - length, " Host");
      if (length >= 1024)
      {
         length = 1024;
         goto write_data;
      }
   }
   buffer[length++] = '\n';
   if (length >= 1024)
   {
      length = 1024;
      goto write_data;
   }

   length += snprintf(&buffer[length], 1024 - length, "\tEvent type    :");
   if (length >= 1024)
   {
      length = 1024;
      goto write_data;
   }
   if (toggles_set & SHOW_TYPE_EXTERNAL)
   {
      length += snprintf(&buffer[length], 1024 - length, " Extern");
      if (length >= 1024)
      {
         length = 1024;
         goto write_data;
      }
   }
   if (toggles_set & SHOW_TYPE_MANUAL)
   {
      length += snprintf(&buffer[length], 1024 - length, " Manual");
      if (length >= 1024)
      {
         length = 1024;
         goto write_data;
      }
   }
   if (toggles_set & SHOW_TYPE_AUTO)
   {
      length += snprintf(&buffer[length], 1024 - length, " Auto");
      if (length >= 1024)
      {
         length = 1024;
         goto write_data;
      }
   }
   buffer[length++] = '\n';
   if (length >= 1024)
   {
      length = 1024;
      goto write_data;
   }

   /* Don't forget the heading for the data. */
   length += snprintf(&buffer[length], 1024 - length, "\n\n%s\n%s\n",
                      heading_line, sum_sep_line);
   if (length > 1024)
   {
      length = 1024;
   }

write_data:
   /* Write heading to file/printer. */
   if (write(fd, buffer, length) != length)
   {
      (void)fprintf(stderr, "write() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   return;
}
