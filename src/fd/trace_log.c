/*
 *  trace_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2018 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   trace_log - writes formated trace log output to transfer debug log
 **
 ** SYNOPSIS
 **   void trace_log(char *file,
 **                  int  line,
 **                  int  type,
 **                  char *buffer,
 **                  int  buffer_length,
 **                  char *fmt, ...)
 **
 ** DESCRIPTION
 **   Function trace_log() prints out more details of what has been
 **   send or received in 'buffer'. Depending on the 'type' flag
 **   it prints out 'buffer' in hex or just normal ASCII. The 'type'
 **   flag can be one of the following:
 **
 **     W_TRACE         - ASCII write trace
 **     R_TRACE         - ASCII read trace
 **     C_TRACE         - ASCII command trace
 **     LIST_R_TRACE    - ASCII listing read trace
 **     CRLF_R_TRACE    - ASCII but does not show CRLF
 **     BIN_CMD_W_TRACE - binary command write trace (hex)
 **     BIN_CMD_R_TRACE - binary command read trace (hex)
 **     BIN_W_TRACE     - binary write trace (hex)
 **     BIN_R_TRACE     - binary read trace (hex)
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.12.2003 H.Kiehl Created
 **   08.02.2006 H.Kiehl Added binary commad tracing.
 **   07.04.2006 H.Kiehl Added ASCII command flag C_TRACE.
 **   03.05.2013 H.Kiehl Added ASCII list read LIST_R_TRACE.
 **   10.06.2015 H.Kiehl Added CRLF_R_TRACE.
 **   11.08.2018 H.Kiehl Show file and line in C_TRACE and others.
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
#include "fddefs.h"

extern int                        trans_db_log_fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
extern int                        trans_db_log_readfd;
#endif
extern char                       *p_work_dir,
                                  tr_hostname[];
extern struct job                 db;
extern struct filetransfer_status *fsa;

static void                       hex_print(char *, int, char *, int);

#define HOSTNAME_OFFSET 16
#define ASCII_OFFSET    54


/*############################ trace_log() ###############################*/
void
trace_log(char *file,
          int  line,
          int  type,
          char *buffer,
          int  buffer_length,
          char *fmt, ...)
{
   int tmp_errno = errno;

   if ((((type == BIN_R_TRACE) || (type == BIN_W_TRACE)) &&
        (fsa->debug == FULL_TRACE_MODE)) ||
       (((type == R_TRACE) || (type == W_TRACE) || (type == C_TRACE) ||
         (type == BIN_CMD_R_TRACE) || (type == BIN_CMD_W_TRACE) ||
         (type == LIST_R_TRACE) || (type == CRLF_R_TRACE)) &&
        (fsa->debug > DEBUG_MODE)))
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
         char      *ptr = tr_hostname;
         size_t    header_length,
                   length = HOSTNAME_OFFSET;
         time_t    tvalue;
         char      buf[MAX_LINE_LENGTH + MAX_LINE_LENGTH + 1],
                   *hex = "0123456789ABCDEF";
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
         buf[8]  = ':';
         buf[5]  = ':';
         buf[2]  = ' ';
         buf[11] = ' ';
         buf[12] = '<';
         buf[13] = 'T';
         buf[14] = '>';
         buf[15] = ' ';

         while (((length - HOSTNAME_OFFSET) < MAX_HOSTNAME_LENGTH) &&
                (*ptr != '\0'))
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

         buf[length + 1] = '-';
         switch (type)
         {
            case BIN_R_TRACE     :
            case BIN_CMD_R_TRACE :
            case R_TRACE         :
            case LIST_R_TRACE    :
            case CRLF_R_TRACE    :
               buf[length] = '<';
               buf[length + 2] = 'R';
               break;

            case BIN_W_TRACE     :
            case BIN_CMD_W_TRACE :
            case W_TRACE         :
               buf[length] = 'W';
               buf[length + 2] = '>';
               break;

            case C_TRACE :
               buf[length] = '<';
               buf[length + 1] = 'C';
               buf[length + 2] = '>';
               break;

            default :
               buf[length] = '-';
               buf[length + 2] = '-';
               break;
         }
         buf[length + 3] = ' ';
         length += 4;
         header_length = length;

         if ((buffer != NULL) && (buffer_length > 0))
         {
            if ((type == BIN_R_TRACE) || (type == BIN_W_TRACE) ||
                (type == BIN_CMD_R_TRACE) || (type == BIN_CMD_W_TRACE))
            {
               hex_print(buf, header_length, buffer, buffer_length);
            }
            else
            {
               int bytes_done = 0,
                   wpos = header_length;

               if (type == LIST_R_TRACE)
               {
                  static int  bytes_in_buffer = 0;
                  static char list_buffer[MAX_LINE_LENGTH + MAX_LINE_LENGTH + 1];

                  if (bytes_in_buffer > 0)
                  {
                     (void)memcpy(buf, list_buffer, bytes_in_buffer);
                     wpos = bytes_in_buffer;
                     bytes_in_buffer = 0;
                  }
                  while ((bytes_done < buffer_length) &&
                         (wpos < (MAX_LINE_LENGTH + MAX_LINE_LENGTH + 1 - 5)))
                  {
                     while ((bytes_done < buffer_length) &&
                            (buffer[bytes_done] != '\r') &&
                            (buffer[bytes_done] != '\n') &&
                            (wpos < (MAX_LINE_LENGTH + MAX_LINE_LENGTH + 1 - 5)))
                     {
                        if (((unsigned char)buffer[bytes_done] < ' ') ||
                            ((unsigned char)buffer[bytes_done] > '~'))
                        {
                           buf[wpos] = '<';
                           if ((unsigned char)buffer[bytes_done] > 15)
                           {
                              buf[wpos + 1] = hex[((unsigned char)buffer[bytes_done]) >> 4];
                              buf[wpos + 2] = hex[((unsigned char)buffer[bytes_done]) & 0x0F];
                           }
                           else
                           {
                              buf[wpos + 1] = '0'; 
                              buf[wpos + 2] = hex[(unsigned char)buffer[bytes_done]];
                           }
                           buf[wpos + 3] = '>';
                           wpos += 4;
                        }
                        else
                        {
                           buf[wpos] = buffer[bytes_done];
                           wpos++;
                        }
                        bytes_done++;
                     }
                     if ((buffer[bytes_done] == '\r') ||
                         (buffer[bytes_done] == '\n'))
                     {
                        if (wpos > header_length)
                        {
                           buf[wpos] = '\n';
                           if (write(trans_db_log_fd, buf, (wpos + 1)) != (wpos + 1))
                           {
                              system_log(ERROR_SIGN, __FILE__, __LINE__,
                                         "write() error : %s", strerror(errno));
                           }
                        }
                        while ((buffer[bytes_done] == '\r') ||
                               (buffer[bytes_done] == '\n'))
                        {
                           bytes_done++;
                        }
                        wpos = header_length;
                     }
                     else
                     {
                        int offset;

                        if (wpos > (MAX_LINE_LENGTH + MAX_LINE_LENGTH + 1))
                        {
                           offset = wpos - (MAX_LINE_LENGTH + MAX_LINE_LENGTH + 1);
                        }
                        else
                        {
                           offset = 0;
                        }
                        (void)memcpy(list_buffer, &buf[offset],
                                     (wpos - offset));
                        bytes_in_buffer = wpos - offset;
                     }
                  }
               }
               else if (type == CRLF_R_TRACE)
                    {
                       while ((bytes_done < buffer_length) &&
                              (wpos < (MAX_LINE_LENGTH + MAX_LINE_LENGTH + 1 - 5)))
                       {
                          if ((buffer[bytes_done] == '\r') ||
                              (buffer[bytes_done] == '\n'))
                          {
                             if (wpos > header_length)
                             {
                                buf[wpos] = '\n';
                                if (write(trans_db_log_fd, buf, (wpos + 1)) != (wpos + 1))
                                {
                                   system_log(ERROR_SIGN, __FILE__, __LINE__,
                                              "write() error : %s", strerror(errno));
                                }
                             }
                             while ((buffer[bytes_done] == '\r') ||
                                    (buffer[bytes_done] == '\n'))
                             {
                                bytes_done++;
                             }
                             wpos = header_length;
                          }
                          else
                          {
                             if (((unsigned char)buffer[bytes_done] < ' ') ||
                                 ((unsigned char)buffer[bytes_done] > '~'))
                             {
                                buf[wpos] = '<';
                                if ((unsigned char)buffer[bytes_done] > 15)
                                {
                                   buf[wpos + 1] = hex[((unsigned char)buffer[bytes_done]) >> 4];
                                   buf[wpos + 2] = hex[((unsigned char)buffer[bytes_done]) & 0x0F];
                                }
                                else
                                {
                                   buf[wpos + 1] = '0'; 
                                   buf[wpos + 2] = hex[(unsigned char)buffer[bytes_done]];
                                }
                                buf[wpos + 3] = '>';
                                wpos += 4;
                             }
                             else
                             {
                                buf[wpos] = buffer[bytes_done];
                                wpos++;
                             }
                             bytes_done++;
                          }
                       }
                       buf[wpos] = '\n';
                       if (write(trans_db_log_fd, buf, (wpos + 1)) != (wpos + 1))
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "write() error : %s", strerror(errno));
                       }
                    }
                    else
                    {
                       while ((bytes_done < buffer_length) &&
                              (wpos < (MAX_LINE_LENGTH + MAX_LINE_LENGTH + 1 - 5)))
                       {
                          if (((unsigned char)buffer[bytes_done] < ' ') ||
                              ((unsigned char)buffer[bytes_done] > '~'))
                          {
                             buf[wpos] = '<';
                             if ((unsigned char)buffer[bytes_done] > 15)
                             {
                                buf[wpos + 1] = hex[((unsigned char)buffer[bytes_done]) >> 4];
                                buf[wpos + 2] = hex[((unsigned char)buffer[bytes_done]) & 0x0F];
                             }
                             else
                             {
                                buf[wpos + 1] = '0'; 
                                buf[wpos + 2] = hex[(unsigned char)buffer[bytes_done]];
                             }
                             buf[wpos + 3] = '>';
                             wpos += 4;
                          }
                          else
                          {
                             buf[wpos] = buffer[bytes_done];
                             wpos++;
                          }
                          bytes_done++;
                       }
                       if ((file == NULL) || (line == 0) ||
                           (wpos >= (MAX_LINE_LENGTH + MAX_LINE_LENGTH)))
                       {
                          buf[wpos] = '\n';
                       }
                       else
                       {
                          wpos += snprintf(&buf[wpos],
                                             (MAX_LINE_LENGTH + MAX_LINE_LENGTH) - wpos,
                                             " (%s %d)\n", file, line);
                          if (wpos > (MAX_LINE_LENGTH + MAX_LINE_LENGTH))
                          {
                             wpos = MAX_LINE_LENGTH + MAX_LINE_LENGTH;
                             buf[wpos] = '\n';
                             wpos += 1;
                          }
                       }
                       if (write(trans_db_log_fd, buf, (wpos + 1)) != (wpos + 1))
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "write() error : %s", strerror(errno));
                       }
                    }
            }
         }

         if (fmt != NULL)
         {
            va_list   ap;

            va_start(ap, fmt);
            length += vsnprintf(&buf[length],
                                (MAX_LINE_LENGTH + MAX_LINE_LENGTH) - length,
                                fmt, ap);
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

            if (write(trans_db_log_fd, buf, length) != length)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "write() error : %s", strerror(errno));
            }
         }
      }
   }
   errno = tmp_errno;

   return;
}


/*++++++++++++++++++++++++++++ hex_print() ++++++++++++++++++++++++++++++*/
static void
hex_print(char *wbuf, int header_length, char *buffer, int length)
{
   int  ascii_offset = header_length + ASCII_OFFSET,
        i,
        line_length = 0,
        offset,
        wpos = header_length;
   char *hex = "0123456789ABCDEF";

   for (i = 0; i < length; i++)
   {
      if ((i % 16) == 0)
      {
         if (line_length > 0)
         {
            offset = ascii_offset + line_length;
            wbuf[ascii_offset - 1] = ' ';
            wbuf[offset] = '\n';
            if (write(trans_db_log_fd, wbuf, (offset + 1)) != (offset + 1))
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "write() error : %s", strerror(errno));
            }
            wpos = header_length;
            line_length = 0;
         }
      }
      else
      {
         if ((i % 4) == 0)
         {
            wbuf[wpos] = '|'; wbuf[wpos + 1] = ' ';
            wpos += 2;
         }
      }
      if ((unsigned char)buffer[i] > 15)
      {
         wbuf[wpos] = hex[((unsigned char)buffer[i]) >> 4];
         wbuf[wpos + 1] = hex[((unsigned char)buffer[i]) & 0x0F];
      }
      else
      {
         wbuf[wpos] = '0';
         wbuf[wpos + 1] = hex[(unsigned char)buffer[i]];
      }
      wbuf[wpos + 2] = ' ';
      wpos += 3;
      if (((unsigned char)buffer[i] < 32) || ((unsigned char)buffer[i] > 126))
      {
         wbuf[ascii_offset + line_length] = '.';
      }
      else
      {
         wbuf[ascii_offset + line_length] = buffer[i];
      }
      line_length++;
   }
   if (line_length > 0)
   {
      for (i = line_length; i < 16; i++)
      {
         if ((i % 4) == 0)
         {
            wbuf[wpos] = '|'; wbuf[wpos + 1] = ' ';
            wpos += 2;
         }
         wbuf[wpos] = ' '; wbuf[wpos + 1] = ' '; wbuf[wpos + 2] = ' ';
         wpos += 3;
      }
      offset = ascii_offset + line_length;
      wbuf[ascii_offset - 1] = ' ';
      wbuf[offset] = '\n';
      if (write(trans_db_log_fd, wbuf, (offset + 1)) != (offset + 1))
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "write() error : %s", strerror(errno));
      }
   }
   return;
}
