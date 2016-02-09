/*
 *  event_logger.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   event_logger - writes messages to a log file
 **
 ** SYNOPSIS
 **   int event_logger(FILE  *p_log_file,
 **                    off_t max_logfile_size,
 **                    int   log_fd,
 **                    int   rescan_time)
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
 **   18.06.2007 H.Kiehl Created
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
                    *p_log_fifo;

/* Local global varaibles. */
static int          log_fd;
static FILE         *p_log_file;

/* Local function prototypes. */
static void         check_data(long),
                    sig_exit(int);


/*############################ event_logger() ###########################*/
int
event_logger(FILE  *fp,
             off_t max_logfile_size,
             int   fd,
             int   rescan_time)
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
   fd_set         rset;
   struct timeval timeout;

   FD_ZERO(&rset);
   FD_SET(log_fd, &rset);
   timeout.tv_usec = 0;
   timeout.tv_sec = rescan_time;

   /* Wait for message x seconds and then continue. */
   status = select(log_fd + 1, &rset, NULL, NULL, &timeout);

   /* Did we get a timeout? */
   if (FD_ISSET(log_fd, &rset))
   {
      int  n;
      char *ptr;

      ptr = fifo_buffer;
      if ((n = read(log_fd, &fifo_buffer[bytes_buffered],
                    fifo_size - bytes_buffered)) > 0)
      {
         int count = 0,
             length;

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

               total_length += fprintf(p_log_file, "%s", msg_str);
               (void)fflush(p_log_file);
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

   return;
}
