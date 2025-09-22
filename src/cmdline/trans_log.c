/*
 *  trans_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2025 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifdef _STANDALONE_
# include "cmdline.h"
#else
# include "afddefs.h"
#endif

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
 **   29.07.2000 H.Kiehl Revised to reduce code size in aftp.
 **   21.03.2009 H.Kiehl Added function parameter.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* memcpy(), strerror()            */
#include <stdarg.h>                   /* va_start(), va_end()            */
#include <time.h>                     /* time(), localtime()             */
#include <sys/time.h>                 /* struct tm                       */
#include <sys/types.h>
#include <unistd.h>                   /* write()                         */
#include <errno.h>
#ifndef _STANDALONE_
# include "cmdline.h"
#endif

extern int  timeout_flag,
            transfer_log_fd;
extern long transfer_timeout;


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
   int       tmp_errno = errno;
   size_t    header_length,
             length;
   time_t    tvalue;
   char      buf[MAX_LINE_LENGTH + MAX_LINE_LENGTH + 1];
   va_list   ap;
   struct tm *p_ts;

   tvalue = time(NULL);
   p_ts    = localtime(&tvalue);
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
   buf[16] = ':';
   buf[17] = ' ';
   length = 18;
   if ((function != NULL) && (function[0] != '\0'))
   {
      length += sprintf(&buf[length], "%s(): ", function);
   }
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
      char *tmp_ptr;

      if (buf[length - 1] == '.')
      {
         tmp_ptr = &buf[length - 1];
      }
      else
      {
         tmp_ptr = &buf[length];
      }
      if ((file == NULL) || (line == 0))
      {
         length += snprintf(tmp_ptr,
                            (MAX_LINE_LENGTH + MAX_LINE_LENGTH) - length,
                            _(" due to timeout (%lds).\n"), transfer_timeout);
      }
      else
      {
         length += snprintf(tmp_ptr,
                            (MAX_LINE_LENGTH + MAX_LINE_LENGTH) - length,
                            _(" due to timeout (%lds). (%s %d)\n"),
                            transfer_timeout, file, line);
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
   }
   if ((msg_str != NULL) && (timeout_flag == OFF) && (msg_str[0] != '\0') &&
       (length < (MAX_LINE_LENGTH + MAX_LINE_LENGTH)))
   {
      char *end_ptr,
           *ptr,
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
   if (write(transfer_log_fd, buf, length) != length)
   {
      (void)fprintf(stderr, "Failed to write() buffer : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
   }
   errno = tmp_errno;

   return;
}
