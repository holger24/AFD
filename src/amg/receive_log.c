/*
 *  receive_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   receive_log - writes formated log output to receive log
 **
 ** SYNOPSIS
 **   void receive_log(char   *sign,
 **                    char   *file,
 **                    int    line,
 **                    time_t current_time,
 **                    char   *fmt, ...)
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
 **   29.08.2000 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* memcpy()                        */
#include <stdarg.h>                   /* va_start(), va_end()            */
#include <time.h>                     /* time(), localtime()             */
#include <sys/time.h>                 /* struct tm                       */
#include <sys/types.h>
#include <unistd.h>                   /* write()                         */
#include <errno.h>
#include "amgdefs.h"

extern int                        receive_log_fd;
extern struct fileretrieve_status *p_fra;

#define DIR_ALIAS_OFFSET 16


/*########################### receive_log() #############################*/
void
receive_log(char   *sign,
            char   *file,
            int    line,
            time_t current_time,
            char   *fmt, ...)
{
   int       tmp_errno = errno;
   size_t    length = DIR_ALIAS_OFFSET;
   char      buf[MAX_LINE_LENGTH + MAX_LINE_LENGTH + 1],
             *ptr = p_fra->dir_alias;
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
   if (((sign[1] == 'E') || (sign[1] == 'W')) &&
        ((p_fra->dir_flag & DIR_ERROR_OFFLINE) ||
         (p_fra->dir_flag & DIR_ERROR_OFFL_T)))
   {
      buf[13] = 'O';
   }
   else
   {
      buf[13] = sign[1];
   }
   buf[14] = sign[2];
   buf[15] = ' ';

   while ((length < (MAX_LINE_LENGTH + MAX_LINE_LENGTH)) && (*ptr != '\0'))
   {
      buf[length] = *ptr;
      ptr++; length++;
   }
   while ((length - DIR_ALIAS_OFFSET) < MAX_DIR_ALIAS_LENGTH)
   {
      buf[length] = ' ';
      length++;
   }
   buf[length]     = ':';
   buf[length + 1] = ' ';
   length += 2;

   va_start(ap, fmt);
   length += vsnprintf(&buf[length],
                       (MAX_LINE_LENGTH + MAX_LINE_LENGTH) - length, fmt, ap);
   va_end(ap);

   if (length > (MAX_LINE_LENGTH + MAX_LINE_LENGTH))
   {
      length = MAX_LINE_LENGTH + MAX_LINE_LENGTH;
   }

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
         length = MAX_LINE_LENGTH + MAX_LINE_LENGTH;
         buf[length] = '\n';
         length += 1;
      }
   }

   if (write(receive_log_fd, buf, length) != length)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "write() error : %s", strerror(errno));
   }
   errno = tmp_errno;

   return;
}
