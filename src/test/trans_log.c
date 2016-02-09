/*
 *  trans_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   trans_log - writes formated log output to transfer log
 **
 ** SYNOPSIS
 **   void trans_log(char *sign,
 **                  char *file,
 **                  int  line,
 **                  char *function,
 **                  char *msg_str,
 **                  char *fmt, ...)
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
 **   19.03.1999 H.Kiehl Created
 **   08.07.2000 H.Kiehl Revised to reduce code size in sf_xxx().
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* memcpy()                        */
#include <stdarg.h>                   /* va_start(), va_end()            */
#include <time.h>                     /* time(), localtime()             */
#include <sys/types.h>
#include <unistd.h>                   /* write()                         */
#include "fddefs.h"

extern int                        timeout_flag;
extern long                       transfer_timeout;
extern struct job                 db;

#define HOSTNAME_OFFSET 16


/*############################ trans_log() ###############################*/
void
trans_log(char *sign,
          char *file,
          int  line,
          char *function,
          char *msg_str,
          char *fmt,
          ...)
{
   size_t    length;
   time_t    tvalue;
   char      buf[MAX_LINE_LENGTH + MAX_LINE_LENGTH];
   va_list   ap;
   struct tm *p_ts;

   tvalue = time(NULL);
   p_ts    = localtime(&tvalue);
   buf[0]  = (p_ts->tm_mday / 10) + '0';
   buf[1]  = (p_ts->tm_mday % 10) + '0';
   buf[2]  = ' ';
   buf[3]  = (p_ts->tm_hour / 10) + '0';
   buf[4]  = (p_ts->tm_hour % 10) + '0';
   buf[5]  = ':';
   buf[6]  = (p_ts->tm_min / 10) + '0';
   buf[7]  = (p_ts->tm_min % 10) + '0';
   buf[8]  = ':';
   buf[9]  = (p_ts->tm_sec / 10) + '0';
   buf[10] = (p_ts->tm_sec % 10) + '0';
   buf[11] = ' ';
   buf[12] = sign[0];
   buf[13] = sign[1];
   buf[14] = sign[2];
   buf[15] = ' ';
   length = 16;
   if ((function != NULL) && (function[0] != '\0'))
   {
      length += sprintf(&buf[length], "%s(): ", function);
   }

   va_start(ap, fmt);
   length += vsprintf(&buf[length], fmt, ap);
   va_end(ap);

   if (timeout_flag == ON)
   {
      if (buf[length - 1] == '.')
      {
         length--;
      }
      if ((file == NULL) || (line == 0))
      {
         length += sprintf(&buf[length], " due to timeout (%lds).\n",
                           transfer_timeout);
      }
      else
      {
         length += sprintf(&buf[length], " due to timeout (%lds). (%s %d)\n",
                           transfer_timeout, file, line);
      }
   }
   else
   {
      if ((file == NULL) || (line == 0))
      {
         buf[length] = '\n';
         length += 1;
      }
      else
      {
         length += sprintf(&buf[length], " (%s %d)\n", file, line);
      }
   }

   (void)write(STDOUT_FILENO, buf, length);

   return;
}
