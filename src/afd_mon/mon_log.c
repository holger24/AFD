/*
 *  mon_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2006 - 2017 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   mon_log - writes formated log output to monitor log
 **
 ** SYNOPSIS
 **   void mon_log(char   *sign,
 **                char   *file,
 **                int    line,
 **                time_t current_time,
 **                char   *msg_str,
 **                char   *fmt, ...)
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
 **   01.08.2006 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* memcpy()                        */
#include <stdarg.h>                   /* va_start(), va_end()            */
#include <time.h>                     /* time(), localtime()             */
#ifdef TM_IN_SYS_TIME
# include <sys/time.h>
#endif
#include <sys/types.h>
#include <unistd.h>                   /* write()                         */
#include <errno.h>
#include "mondefs.h"

extern int  mon_log_fd,
            timeout_flag;
extern long tcp_timeout;
extern char *p_mon_alias;

#define MON_ALIAS_OFFSET 16


/*############################# mon_log() ###############################*/
void
mon_log(char   *sign,
        char   *file,
        int    line,
        time_t current_time,
        char   *msg_str,
        char   *fmt, ...)
{
   int       tmp_errno = errno;
   char      *ptr = p_mon_alias;
   size_t    header_length,
             length = MON_ALIAS_OFFSET;
   char      buf[MAX_LINE_LENGTH + MAX_LINE_LENGTH + 1];
   va_list   ap;
   struct tm *p_ts;

   if (current_time == 0L)
   {
      current_time = time(NULL);
   }
   p_ts = localtime(&current_time);
   if (p_ts == NULL)
   {
      buf[0]  = '?';
      buf[1]  = '?';
      buf[3]  = '?';
      buf[4]  = '?';
      buf[6]  = '?';
      buf[7]  = '?';
      buf[9]  = '?';
      buf[10] = '?';
   }
   else
   {
      buf[0]  = (p_ts->tm_mday / 10) + '0';
      buf[1]  = (p_ts->tm_mday % 10) + '0';
      buf[3]  = (p_ts->tm_hour / 10) + '0';
      buf[4]  = (p_ts->tm_hour % 10) + '0';
      buf[6]  = (p_ts->tm_min / 10) + '0';
      buf[7]  = (p_ts->tm_min % 10) + '0';
      buf[9]  = (p_ts->tm_sec / 10) + '0';
      buf[10] = (p_ts->tm_sec % 10) + '0';
   }
   buf[2]  = ' ';
   buf[5]  = ':';
   buf[8]  = ':';
   buf[11] = ' ';
   buf[12] = sign[0];
   buf[13] = sign[1];
   buf[14] = sign[2];
   buf[15] = ' ';

   while (*ptr != '\0')
   {
      buf[length] = *ptr;
      ptr++; length++;
   }
   while ((length - MON_ALIAS_OFFSET) < MAX_AFDNAME_LENGTH)
   {
      buf[length] = ' ';
      length++;
   }
   buf[length]     = ':';
   buf[length + 1] = ' ';
   length += 2;
   header_length = length;

   va_start(ap, fmt);
   length += vsnprintf(&buf[length],
                       (MAX_LINE_LENGTH + MAX_LINE_LENGTH) - length, fmt, ap);
   va_end(ap);

   if (length > (MAX_LINE_LENGTH + MAX_LINE_LENGTH))
   {
      length = MAX_LINE_LENGTH + MAX_LINE_LENGTH;
   }

   if (timeout_flag == ON)
   {
      if ((file == NULL) || (line == 0) ||
          (length >= (MAX_LINE_LENGTH + MAX_LINE_LENGTH)))
      {
         buf[length] = '\n';
         length += 1;
      }
      else
      {
         if (buf[length - 1] == '.')
         {
            length--;
         }
         length += snprintf(&buf[length],
                            (MAX_LINE_LENGTH + MAX_LINE_LENGTH) - length,
                            " due to timeout (%lds). (%s %d)\n",
                            tcp_timeout, file, line);
         if (length > (MAX_LINE_LENGTH + MAX_LINE_LENGTH))
         {
            buf[MAX_LINE_LENGTH + MAX_LINE_LENGTH] = '\n';
            length = MAX_LINE_LENGTH + MAX_LINE_LENGTH + 1;
         }
      }
   }
   else
   {
      if ((file == NULL) || (line == 0) ||
          (length >= (MAX_LINE_LENGTH + MAX_LINE_LENGTH)))
      {
         buf[length] = '\n';
         length += 1;
      }
      else
      {
         length += snprintf(&buf[length],
                            (MAX_LINE_LENGTH + MAX_LINE_LENGTH) - length,
                            " (%s %d)\n", file, line);
         if (length > (MAX_LINE_LENGTH + MAX_LINE_LENGTH))
         {
            buf[MAX_LINE_LENGTH + MAX_LINE_LENGTH] = '\n';
            length = MAX_LINE_LENGTH + MAX_LINE_LENGTH + 1;
         }
      }

      if ((msg_str != NULL) && (msg_str[0] != '\0') && (timeout_flag == OFF) &&
          (length < (MAX_LINE_LENGTH + MAX_LINE_LENGTH)))
      {
         char *end_ptr,
              tmp_char;

         tmp_char = buf[header_length];
         buf[header_length] = '\0';
         end_ptr = msg_str;
         do
         {
            while ((*end_ptr == '\n') || (*end_ptr == '\r'))
            {
               end_ptr++;
            }
            ptr = end_ptr;
            while (((end_ptr - msg_str) < MAX_RET_MSG_LENGTH) &&
                   (*end_ptr != '\n') && (*end_ptr != '\r') &&
                   (*end_ptr != '\0'))
            {
               /* Replace any unprintable characters with a dot. */
               if ((*end_ptr < ' ') || (*end_ptr > '~'))
               {
                  *end_ptr = '.';
               }
               end_ptr++;
            }
            if ((*end_ptr == '\n') || (*end_ptr == '\r'))
            {
               *end_ptr = '\0';
               end_ptr++;
            }
            length += snprintf(&buf[length],
                               (MAX_LINE_LENGTH + MAX_LINE_LENGTH) - length,
                               "%s%s\n", buf, ptr);
            if ((length >= (MAX_LINE_LENGTH + MAX_LINE_LENGTH)) ||
                ((end_ptr - msg_str) >= MAX_RET_MSG_LENGTH))
            {
               buf[MAX_LINE_LENGTH + MAX_LINE_LENGTH] = '\n';
               length = MAX_LINE_LENGTH + MAX_LINE_LENGTH + 1;
               break;
            }
         } while (*end_ptr != '\0');
         buf[header_length] = tmp_char;
      }
   }

   if (write(mon_log_fd, buf, length) != length)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "write() error : %s", strerror(errno));
   }
   errno = tmp_errno;

   return;
}
