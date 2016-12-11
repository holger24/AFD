/*
 *  get_printer_cmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2016 Deutscher Wetterdienst (DWD),
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
 **   get_printer_cmd - reads the printer command and default printer
 **                     name
 **
 ** SYNOPSIS
 **   void get_printer_cmd(char *printer_cmd,
 **                        char *default_printer,
 **                        char *mailserver,
 **                        int  *port)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.12.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ui_common_defs.h"
#include "smtpdefs.h"

/* External global variables. */
extern char *p_work_dir;


/*++++++++++++++++++++++++++ get_printer_cmd() ++++++++++++++++++++++++++*/
void                                                                
get_printer_cmd(char *printer_cmd,
                char *default_printer,
                char *mailserver,
                int  *port)
{
   char *buffer,
        config_file[MAX_PATH_LENGTH];

   (void)snprintf(config_file, MAX_PATH_LENGTH, "%s%s%s",
                  p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   if ((eaccess(config_file, F_OK) == 0) &&
       (read_file_no_cr(config_file, &buffer, YES,
                        __FILE__, __LINE__) != INCORRECT))
   {
      if (get_definition(buffer, DEFAULT_PRINTER_CMD_DEF,
                         printer_cmd, PRINTER_INFO_LENGTH) == NULL)
      {
         (void)strcpy(printer_cmd, "lpr -P");
      }

      if (get_definition(buffer, DEFAULT_PRINTER_NAME_DEF,
                         default_printer, PRINTER_INFO_LENGTH) == NULL)
      {
         default_printer[0] = '\0';
      }
      if (get_definition(buffer, DEFAULT_PRINT_SMTP_SERVER_DEF,
                         mailserver, MAX_REAL_HOSTNAME_LENGTH + 1 + MAX_INT_LENGTH) == NULL)
      {
         if (get_definition(buffer, DEFAULT_SMTP_SERVER_DEF,
                            mailserver, MAX_REAL_HOSTNAME_LENGTH + 1 + MAX_INT_LENGTH) == NULL)
         {
            (void)strcpy(mailserver, SMTP_HOST_NAME);
            *port = DEFAULT_SMTP_PORT;
         }
         else
         {
            char *ptr = mailserver;

            while ((*ptr != ':') && (*ptr != '\0'))
            {
               ptr++;
            }
            if (*ptr == ':')
            {
               *ptr = '\0';
               *port = atoi(ptr + 1);
            }
            else
            {
               *port = DEFAULT_SMTP_PORT;
            }
         }
      }
      else
      {
         char *ptr = mailserver;

         while ((*ptr != ':') && (*ptr != '\0'))
         {
            ptr++;
         }
         if (*ptr == ':')
         {
            *ptr = '\0';
            *port = atoi(ptr + 1);
         }
         else
         {
            *port = DEFAULT_SMTP_PORT;
         }
      }
      free(buffer);
   }
   else
   {
      (void)strcpy(printer_cmd, "lpr -P");
      default_printer[0] = '\0';
      (void)strcpy(mailserver, SMTP_HOST_NAME);
      *port = DEFAULT_SMTP_PORT;
   }

   return;
}
