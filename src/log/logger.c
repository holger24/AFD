/*
 *  logger.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2015 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   logger - writes messages to a log file
 **
 ** SYNOPSIS
 **   int logger(FILE  *p_log_file,
 **              off_t max_logfile_size,
 **              int   log_fd,
 **              int   rescan_time)
 **
 ** DESCRIPTION
 **   The function logger() reads data from the fifo log_fd and stores
 **   it in the file p_log_file. When the file has reached 'max_logfile_size'
 **   it will return START. The messages that come via log_fd are checked
 **   if they are duplicate messages. This will help to make the log file
 **   more readable when some program goes wild and constantly writes to
 **   the fifo.
 **
 ** RETURN VALUES
 **   Returns START when the log file size has reached 'max_logfile_size'.
 **   It will exit when select() fails.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   03.02.1996 H.Kiehl Created
 **   31.05.1997 H.Kiehl Added debug option.
 **   10.12.1997 H.Kiehl Write out that there are duplicate messages
 **                      after rescan_time.
 **                      Excluded the date from the duplicate message
 **                      check.
 **   10.08.1999 H.Kiehl When reading binary zero logger() would go
 **                      into an idefinite loop.
 **   19.03.2000 H.Kiehl Do not show any non printable data.
 **   20.03.2000 H.Kiehl When logger receives a normal exit signal
 **                      it will try to read what is still left in
 **                      the fifo.
 **   27.12.2003 H.Kiehl Catch fifo buffer overflow.
 **   16.07.2007 H.Kiehl Added offline ID.
 **
 */
DESCR__E_M3

#include <stdio.h>                    /* fprintf(), fflush()             */
#include <string.h>                   /* strcpy(), strerror()            */
#include <stdlib.h>                   /* exit(), free()                  */
#include <time.h>                     /* time()                          */
#include <signal.h>                   /* signal()                        */
#include <sys/types.h>                /* fdset                           */
#include <sys/time.h>                 /* struct timeval                  */
#include <unistd.h>                   /* select(), read()                */
#include <errno.h>
#include "logdefs.h"

/* External global variables. */
extern int          bytes_buffered;
extern unsigned int *p_log_counter,
                    total_length;
extern long         fifo_size;
extern char         *fifo_buffer,
                    *msg_str,
                    *p_log_fifo,
                    *p_log_his,
                    *prev_msg_str;

/* Local global varaibles. */
static int          log_fd;
static FILE         *p_log_file;

/* Local function prototypes. */
static void         check_data(long),
                    sig_exit(int);


/*############################### logger() ##############################*/
int
logger(FILE  *fp,
       off_t max_logfile_size,
       int   fd,
       int  rescan_time)
{
   log_fd = fd;
   p_log_file = fp;
   if (signal(SIGINT, sig_exit) == SIG_ERR)
   {
      (void)fprintf(stderr, "Could not set signal handler : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   for (;;)
   {
      check_data(rescan_time);
      if (total_length > max_logfile_size)
      {
         /* Time to start a new log file. */
         return(START);
      }
   } /* for (;;) */
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   check_data(0L);

   exit(SUCCESS);
}


/*------------------------------ check_data() ---------------------------*/
static void
check_data(long rescan_time)
{
   int            status;
   static int     log_pos = -1,
                  prev_length = 0,
                  dup_msg = 0;
   static time_t  dup_msg_start_time = 0L,
                  now = 0L,
                  prev_msg_time;
   fd_set         rset;
   struct timeval timeout;

   /* Check if we need to reshuffel the log history data. */
   if (p_log_his != NULL)
   {
      static time_t next_his_time = 0L;

      if (now == 0L)
      {
         next_his_time = (time(NULL) / HISTORY_LOG_INTERVAL) *
                         HISTORY_LOG_INTERVAL + HISTORY_LOG_INTERVAL;
      }
      else if (time(&now) > next_his_time)
           {
              (void)memmove(p_log_his, p_log_his + 1, MAX_LOG_HISTORY - 1);
              p_log_his[MAX_LOG_HISTORY - 1] = NO_INFORMATION;
              next_his_time = (now / HISTORY_LOG_INTERVAL) *
                              HISTORY_LOG_INTERVAL + HISTORY_LOG_INTERVAL;
           }
   }
   if ((log_pos == -1) && (p_log_counter != NULL))
   {
      log_pos = *p_log_counter % LOG_FIFO_SIZE;
   }

   FD_ZERO(&rset);
   FD_SET(log_fd, &rset);
   timeout.tv_usec = 0;
   timeout.tv_sec = rescan_time;

   /* Wait for message x seconds and then continue. */
   status = select(log_fd + 1, &rset, NULL, NULL, &timeout);

   /* Did we get a timeout? */
   if (status == 0)
   {
      /* If there are duplicate messages, show them. */
      if (dup_msg > 0)
      {
         if (dup_msg == 1)
         {
            total_length += fprintf(p_log_file, "%s", prev_msg_str);
         }
         else
         {
            if (p_log_his != NULL)
            {
               total_length += fprint_dup_msg(p_log_file, dup_msg,
                                              &prev_msg_str[LOG_SIGN_POSITION - 1],
                                              NULL, 0, time(NULL));
            }
            else
            {
               total_length += fprint_dup_msg(p_log_file, dup_msg,
                                              &prev_msg_str[LOG_SIGN_POSITION - 1],
                                              &prev_msg_str[LOG_SIGN_POSITION + 3],
                                              MAX_HOSTNAME_LENGTH + 3,
                                              time(NULL));
            }
         }
         (void)fflush(p_log_file);
         dup_msg = 0;
         if (p_log_counter != NULL)
         {
            if (log_pos == LOG_FIFO_SIZE)
            {
               log_pos = 0;
            }
            switch (prev_msg_str[LOG_SIGN_POSITION])
            {
               case 'I' : /* Info */
                  p_log_fifo[log_pos] = INFO_ID;
                  break;
               case 'W' : /* Warn */
                  p_log_fifo[log_pos] = WARNING_ID;
                  break;
               case 'E' : /* Error */
                  p_log_fifo[log_pos] = ERROR_ID;
                  break;
               case 'D' : /* Debug is NOT made vissible! */
                  break;
               case 'T' : /* Trace is NOT made vissible! */
                  break;
               case 'O' : /* Error/Warn Offline, NOT vissible! */
                  break;
               case 'C' : /* Config */
                  p_log_fifo[log_pos] = CONFIG_ID;
                  break;
               case 'F' : /* Faulty */
                  p_log_fifo[log_pos] = FAULTY_ID;
                  break;
               case '#' : /* Date, etc */
                  p_log_fifo[log_pos] = INFO_ID;
                  break;
               default  : /* Unknown */
                  p_log_fifo[log_pos] = CHAR_BACKGROUND;
                  break;
            }
            if ((prev_msg_str[LOG_SIGN_POSITION] != 'D') &&
                (prev_msg_str[LOG_SIGN_POSITION] != 'T') &&
                (prev_msg_str[LOG_SIGN_POSITION] != 'O'))
            {
               if (p_log_his != NULL)
               {
                  if (p_log_fifo[log_pos] > p_log_his[MAX_LOG_HISTORY - 1])
                  {
                     p_log_his[MAX_LOG_HISTORY - 1] = p_log_fifo[log_pos];
                  }
               }
               log_pos++;
               (*p_log_counter)++;
            }
         }
      }
   }
   else if (FD_ISSET(log_fd, &rset))
        {
           int  n;
           char *ptr;

           ptr = fifo_buffer;
           if ((n = read(log_fd, &fifo_buffer[bytes_buffered],
                         fifo_size - bytes_buffered)) > 0)
           {
              int count = 0,
                  length;

              /* Remember the time when message arrived. */
              prev_msg_time = now;
              now = time(NULL);

              if (bytes_buffered != 0)
              {
                 n += bytes_buffered;
                 bytes_buffered = 0;
              }

              /* Now evaluate all data read from fifo, byte after byte. */
              while (count < n)
              {
                 length = 0;
                 while ((count < n) && (*ptr != '\n'))
                 {
                    if (*ptr >= ' ')
                    {
                       msg_str[length] = *ptr;
                       length++;
                    }
                    ptr++; count++;
                 }
                 if ((count < n) && (*ptr == '\n'))
                 {
                    ptr++; count++;
                    msg_str[length++] = '\n';
                    msg_str[length] = '\0';

                    if ((length == prev_length) &&
                        ((now - prev_msg_time) < rescan_time))
                    {
                       if (CHECK_STRCMP(&msg_str[LOG_SIGN_POSITION],
                                        &prev_msg_str[LOG_SIGN_POSITION]) == 0)
                       {
                          dup_msg++;

                          /*
                           * There must be some limit here. Else if some
                           * process constantly writes to the fifo we will
                           * never know.
                           */
                          if (dup_msg == 1)
                          {
                             dup_msg_start_time = now;

                             /*
                              * Copy the message again, so we have the correct
                              * time when we print the message when dup_msg is
                              * one.
                              */
                             (void)memcpy(prev_msg_str, msg_str, length + 1);
                          }
                          else if ((now - dup_msg_start_time) > rescan_time)
                               {
                                  if (dup_msg == 1)
                                  {
                                     total_length += fprintf(p_log_file, "%s",
                                                             prev_msg_str);
                                  }
                                  else
                                  {
                                     if (p_log_his != NULL)
                                     {
                                        total_length += fprint_dup_msg(p_log_file,
                                                                       dup_msg,
                                                                       &prev_msg_str[LOG_SIGN_POSITION - 1],
                                                                       NULL, 0,
                                                                       now);
                                     }
                                     else
                                     {
                                        total_length += fprint_dup_msg(p_log_file,
                                                                       dup_msg,
                                                                       &prev_msg_str[LOG_SIGN_POSITION - 1],
                                                                       &prev_msg_str[LOG_SIGN_POSITION + 3],
                                                                       MAX_HOSTNAME_LENGTH + 3,
                                                                       now);
                                     }
                                  }
                                  (void)fflush(p_log_file);
                                  dup_msg = 0;
                               }
                       }
                       else
                       {
                          if (dup_msg > 0)
                          {
                             if (dup_msg == 1)
                             {
                                total_length += fprintf(p_log_file, "%s",
                                                        prev_msg_str);
                             }
                             else
                             {
                                if (p_log_his != NULL)
                                {
                                   total_length += fprint_dup_msg(p_log_file,
                                                                  dup_msg,
                                                                  &prev_msg_str[LOG_SIGN_POSITION - 1],
                                                                  NULL, 0,
                                                                  now);
                                }
                                else
                                {
                                   total_length += fprint_dup_msg(p_log_file,
                                                                  dup_msg,
                                                                  &prev_msg_str[LOG_SIGN_POSITION - 1],
                                                                  &prev_msg_str[LOG_SIGN_POSITION + 3],
                                                                  MAX_HOSTNAME_LENGTH + 3,
                                                                  now);
                                }
                             }
                             dup_msg = 0;
                          }

                          total_length += fprintf(p_log_file, "%s", msg_str);
                          (void)fflush(p_log_file);
                          (void)memcpy(prev_msg_str, msg_str, length + 1);
                          prev_length = length;
                       }
                    }
                    else
                    {
                       if (dup_msg > 0)
                       {
                          if (dup_msg == 1)
                          {
                             total_length += fprintf(p_log_file, "%s",
                                                     prev_msg_str);
                          }
                          else
                          {
                             if (p_log_his != NULL)
                             {
                                total_length += fprint_dup_msg(p_log_file,
                                                               dup_msg,
                                                               &prev_msg_str[LOG_SIGN_POSITION - 1],
                                                               NULL, 0,
                                                               now);
                             }
                             else
                             {
                                total_length += fprint_dup_msg(p_log_file,
                                                               dup_msg,
                                                               &prev_msg_str[LOG_SIGN_POSITION - 1],
                                                               &prev_msg_str[LOG_SIGN_POSITION + 3],
                                                               MAX_HOSTNAME_LENGTH + 3,
                                                               now);
                             }
                          }
                          dup_msg = 0;
                       }

                       total_length += fprintf(p_log_file, "%s", msg_str);
                       (void)fflush(p_log_file);
                       (void)memcpy(prev_msg_str, msg_str, length + 1);
                       prev_length = length;
                    }

                    if ((p_log_counter != NULL) && (dup_msg < 1))
                    {
                       if (log_pos == LOG_FIFO_SIZE)
                       {
                          log_pos = 0;
                       }
                       switch (msg_str[LOG_SIGN_POSITION])
                       {
                          case 'I' : /* Info */
                             p_log_fifo[log_pos] = INFO_ID;
                             break;
                          case 'W' : /* Warn */
                             p_log_fifo[log_pos] = WARNING_ID;
                             break;
                          case 'E' : /* Error */
                             p_log_fifo[log_pos] = ERROR_ID;
                             break;
                          case 'D' : /* Debug is NOT made vissible! */
                             break;
                          case 'T' : /* Trace is NOT made vissible! */
                             break;
                          case 'O' : /* Error/Warn Offline, NOT vissible! */
                             break;
                          case 'C' : /* Config */
                             p_log_fifo[log_pos] = CONFIG_ID;
                             break;
                          case 'F' : /* Faulty */
                             p_log_fifo[log_pos] = FAULTY_ID;
                             break;
                          case '#' : /* Date, etc */
                             p_log_fifo[log_pos] = INFO_ID;
                             break;
                          default  : /* Unknown */
                             p_log_fifo[log_pos] = CHAR_BACKGROUND;
                             break;
                       }
                       if ((msg_str[LOG_SIGN_POSITION] != 'D') &&
                           (msg_str[LOG_SIGN_POSITION] != 'T') &&
                           (msg_str[LOG_SIGN_POSITION] != 'O'))
                       {
                          if (p_log_his != NULL)
                          {
                             if (p_log_fifo[log_pos] > p_log_his[MAX_LOG_HISTORY - 1])
                             {
                                p_log_his[MAX_LOG_HISTORY - 1] = p_log_fifo[log_pos];
                             }
                          }
                          log_pos++;
                          (*p_log_counter)++;
                       }
                    }
                 }
                 else /* Buffer it until line is complete. */
                 {
                    (void)memcpy(fifo_buffer, msg_str, length);
                    bytes_buffered = length;
                    count = n;
                 }
              } /* while (count < n) */
           } /* if (n > 0) */
        }
        /* An error has occurred. */
   else if (status < 0)
        {
           (void)fprintf(stderr,
                         "ERROR   : Select error : %s (%s %d)\n",
                         strerror(errno), __FILE__, __LINE__);
           exit(INCORRECT);
        }
        else
        {
           (void)fprintf(stderr,
                         "ERROR   : Unknown condition. (%s %d)\n",
                         __FILE__, __LINE__);
           exit(INCORRECT);
        }

   return;
}
