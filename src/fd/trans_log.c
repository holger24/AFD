/*
 *  trans_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   21.03.2009 H.Kiehl Added function parameter.
 **   10.08.2012 H.Kiehl Added some more length checking.
 **   17.08.2018 H.Kiehl Do not show empty lines.
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
#include <fcntl.h>
#include <errno.h>
#include "fddefs.h"

extern int                        *p_no_of_hosts,
                                  timeout_flag,
                                  trans_db_log_fd,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                                  trans_db_log_readfd,
#endif
                                  transfer_log_fd;
extern long                       transfer_timeout;
extern char                       *p_work_dir,
                                  tr_hostname[];
extern struct job                 db;
extern struct filetransfer_status *fsa;

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
   int       tmp_errno = errno;
   size_t    header_length,
             length = HOSTNAME_OFFSET;
   time_t    tvalue;
   char      buf[MAX_LINE_LENGTH + MAX_LINE_LENGTH + 1],
             *ptr = tr_hostname;
   va_list   ap;
   struct tm *p_ts;

   tvalue = time(NULL);
   p_ts = localtime(&tvalue);
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
   if (((sign[1] == 'E') || (sign[1] == 'W')) && (fsa != NULL) &&
        ((fsa->host_status & HOST_ERROR_OFFLINE_STATIC) ||
         (fsa->host_status & HOST_ERROR_OFFLINE) ||
         (fsa->host_status & HOST_ERROR_OFFLINE_T) ||
         (*(unsigned char *)((char *)p_no_of_hosts + AFD_START_ERROR_OFFSET_START) > fsa->error_counter)))
   {
      buf[13] = 'O';
   }
   else
   {
      buf[13] = sign[1];
   }
   buf[14] = sign[2];
   buf[15] = ' ';

   while (((length - HOSTNAME_OFFSET) < MAX_HOSTNAME_LENGTH) && (*ptr != '\0'))
   {
      buf[length] = *ptr;
      ptr++; length++;
   }
   while ((length - HOSTNAME_OFFSET) < MAX_HOSTNAME_LENGTH)
   {
      buf[length] = ' ';
      length++;
   }
   buf[length] = '[';
   buf[length + 1] = db.job_no + '0';
   buf[length + 2] = ']';
   buf[length + 3] = ':';
   buf[length + 4] = ' ';
   length += 5;
   if ((function != NULL) && (function[0] != '\0'))
   {
      length += snprintf(&buf[length],
                         (MAX_LINE_LENGTH + MAX_LINE_LENGTH) - length,
                         "%s(): ", function);
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
      if ((file == NULL) || (line == 0) ||
          (length >= (MAX_LINE_LENGTH + MAX_LINE_LENGTH)))
      {
         /* Don't write anything. */
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
                            " due to timeout (%lds). %c%x (%s %d)\n",
                            transfer_timeout,
                            (db.fra_pos == -2) ? '#' : '@',
                            (db.fra_pos == -2) ? db.id.job : db.id.dir,
                            file, line);
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
                            " %c%x (%s %d)\n",
                            (db.fra_pos == -2) ? '#' : '@',
                            (db.fra_pos == -2) ? db.id.job : db.id.dir,
                            file, line);
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
               while ((*end_ptr == '\n') || (*end_ptr == '\r'))
               {
                  end_ptr++;
               }
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

   if (write(transfer_log_fd, buf, length) != length)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "write() error : %s", strerror(errno));
   }

   if ((fsa != NULL) && (fsa->debug > NORMAL_MODE))
   {
      if ((trans_db_log_fd == STDERR_FILENO) && (p_work_dir != NULL))
      {
         char trans_db_log_fifo[MAX_PATH_LENGTH];

         (void)strcpy(trans_db_log_fifo, p_work_dir);
         (void)strcat(trans_db_log_fifo, FIFO_DIR);
         (void)strcat(trans_db_log_fifo, TRANS_DEBUG_LOG_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
         if (open_fifo_rw(trans_db_log_fifo, &trans_db_log_readfd,
                          &trans_db_log_fd) == -1)
#else
         if ((trans_db_log_fd = open(trans_db_log_fifo, O_RDWR)) == -1)
#endif
         {
            if (errno == ENOENT)
            {
               if ((make_fifo(trans_db_log_fifo) == SUCCESS) &&
#ifdef WITHOUT_FIFO_RW_SUPPORT
                   (open_fifo_rw(trans_db_log_fifo, &trans_db_log_readfd,
                                 &trans_db_log_fd) == -1))
#else
                   ((trans_db_log_fd = open(trans_db_log_fifo, O_RDWR)) == -1))
#endif
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Could not open fifo <%s> : %s",
                             TRANS_DEBUG_LOG_FIFO, strerror(errno));
               }
            }
            else
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not open fifo %s : %s",
                          TRANS_DEBUG_LOG_FIFO, strerror(errno));
            }
         }
      }
      if (trans_db_log_fd != -1)
      {
         if (write(trans_db_log_fd, buf, length) != length)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "write() error : %s", strerror(errno));
         }
      }
   }
   errno = tmp_errno;

   return;
}
