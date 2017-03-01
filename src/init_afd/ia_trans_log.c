/*
 *  ia_trans_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2017 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   ia_trans_log - writes formated log output to transfer log
 **
 ** SYNOPSIS
 **   void ia_trans_log(char *sign,
 **                     char *file,
 **                     int  line,
 **                     int  fsa_pos,
 **                     char *fmt, ...)
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
 **   20.12.2010 H.Kiehl Adapted for init_afd().
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* memcpy()                        */
#include <stdarg.h>                   /* va_start(), va_end()            */
#include <time.h>                     /* time(), localtime()             */
#ifdef TM_IN_SYS_TIME
# include <sys/time.h>                /* struct tm                       */
#endif
#include <sys/types.h>
#include <unistd.h>                   /* write()                         */
#include <fcntl.h>
#include <errno.h>

/* Global definitions. */
extern int                        trans_db_log_fd,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                                  trans_db_log_readfd,
#endif
                                  transfer_log_fd;
extern char                       *p_work_dir;
extern struct filetransfer_status *fsa;

#define HOSTNAME_OFFSET 16


/*########################## ia_trans_log() #############################*/
void
ia_trans_log(char *sign,
             char *file,
             int  line,
             int  fsa_pos,
             char *fmt,
             ...)
{
   int       tmp_errno = errno;
   size_t    length = HOSTNAME_OFFSET;
   time_t    tvalue;
   char      buf[MAX_LINE_LENGTH + MAX_LINE_LENGTH + 1],
             *ptr = fsa[fsa_pos].host_alias;
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
   if (((sign[1] == 'E') || (sign[1] == 'W')) &&
        ((fsa[fsa_pos].host_status & HOST_ERROR_OFFLINE_STATIC) ||
         (fsa[fsa_pos].host_status & HOST_ERROR_OFFLINE) ||
         (fsa[fsa_pos].host_status & HOST_ERROR_OFFLINE_T)))
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
   buf[length + 1] = NOT_APPLICABLE_SIGN;
   buf[length + 2] = ']';
   buf[length + 3] = ':';
   buf[length + 4] = ' ';
   length += 5;

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

   if ((transfer_log_fd == STDERR_FILENO) && (p_work_dir != NULL))
   {
      char transfer_log_fifo[MAX_PATH_LENGTH];

      (void)strcpy(transfer_log_fifo, p_work_dir);
      (void)strcat(transfer_log_fifo, FIFO_DIR);
      (void)strcat(transfer_log_fifo, TRANSFER_LOG_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(transfer_log_fifo, &transfer_log_readfd,
                       &transfer_log_fd) == -1)
#else
      if ((transfer_log_fd = open(transfer_log_fifo, O_RDWR)) == -1)
#endif
      {
         if (errno == ENOENT)
         {
            if ((make_fifo(transfer_log_fifo) == SUCCESS) &&
#ifdef WITHOUT_FIFO_RW_SUPPORT
                (open_fifo_rw(transfer_log_fifo, &transfer_log_readfd,
                              &transfer_log_fd) == -1))
#else
                ((transfer_log_fd = open(transfer_log_fifo, O_RDWR)) == -1))
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not open fifo <%s> : %s",
                          TRANSFER_LOG_FIFO, strerror(errno));
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not open fifo %s : %s",
                       TRANSFER_LOG_FIFO, strerror(errno));
         }
         transfer_log_fd = STDOUT_FILENO;
      }
   }

   if (write(transfer_log_fd, buf, length) != length)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "write() error : %s", strerror(errno));
   }

   if (fsa[fsa_pos].debug > NORMAL_MODE)
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
