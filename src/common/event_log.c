/*
 *  event_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 - 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   event_log - writes event data to the event log fifo
 **
 ** SYNOPSIS
 **   void event_log(time_t       event_time,
 **                  unsigned int event_class, 
 **                  unsigned int event_type, 
 **                  unsigned int event_action, 
 **                  char         *fmt,
 **                  ...)
 **
 ** DESCRIPTION
 **   When process wants to log events, it writes them via a fifo. The data
 **   it will write look as follows:
 **        <ET> <EC> <ET> <EA>|<AI>\n
 **         |    |    |    |    |
 **         |    |    |    |    +----------> Additional information.
 **         |    |    |    +---------------> Event action.
 **         |    |    +--------------------> Event type.
 **         |    +-------------------------> Event class.
 **         +------------------------------> Event time.
 **
 ** RETURN VALUES
 **   None
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.06.2007 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdarg.h>                   /* va_start(), va_end()            */
#include <time.h>                     /* time()                          */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>                   /* STDERR_FILENO                   */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>

/* External global variables. */
extern int  event_log_fd;
extern char *p_work_dir;


/*############################# event_log() #############################*/
void
event_log(time_t       event_time,
          unsigned int event_class,
          unsigned int event_type,
          unsigned int event_action,
          char         *fmt,
          ...)
{
   size_t length;
   char   event_buffer[MAX_TIME_T_LENGTH + 1 + MAX_INT_LENGTH + 1 + MAX_INT_LENGTH + 1 + MAX_INT_LENGTH + 1 + MAX_DIR_ALIAS_LENGTH + MAX_HOSTNAME_LENGTH + MAX_USER_NAME_LENGTH + MAX_EVENT_REASON_LENGTH + 1];

   if ((event_log_fd == STDERR_FILENO) && (p_work_dir != NULL))
   {
#ifdef WITHOUT_FIFO_RW_SUPPORT
      int  readfd;
#endif
      char event_log_fifo[MAX_PATH_LENGTH];

      (void)strcpy(event_log_fifo, p_work_dir);
      (void)strcat(event_log_fifo, FIFO_DIR);
      (void)strcat(event_log_fifo, EVENT_LOG_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(event_log_fifo, &readfd, &event_log_fd) == -1)
#else
      if ((event_log_fd = coe_open(event_log_fifo, O_RDWR)) == -1)
#endif
      {
         if (errno == ENOENT)
         {
            if (make_fifo(event_log_fifo) != SUCCESS)
            {
               return;
            }
            else
            {
#ifdef WITHOUT_FIFO_RW_SUPPORT
               if (open_fifo_rw(event_log_fifo, &readfd, &event_log_fd) == -1)
#else
               if ((event_log_fd = coe_open(event_log_fifo, O_RDWR)) == -1)
#endif
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Could not open fifo `%s' : %s"),
                             event_log_fifo, strerror(errno));
                  return;
               }
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Could not open fifo `%s' : %s"),
                       event_log_fifo, strerror(errno));
            return;
         }
      }
   }
   if (event_time == 0L)
   {
      event_time = time(NULL);
   }

   if (fmt == NULL)
   {
      length = snprintf(event_buffer,
                        MAX_TIME_T_LENGTH + 1 + MAX_INT_LENGTH + 1 + MAX_INT_LENGTH + 1 + MAX_INT_LENGTH + 1 + MAX_DIR_ALIAS_LENGTH + MAX_HOSTNAME_LENGTH + MAX_USER_NAME_LENGTH + MAX_EVENT_REASON_LENGTH + 1,
#if SIZEOF_TIME_T == 4
                        "%-*lx %x %x %x",
#else
                        "%-*llx %x %x %x",
#endif
                        LOG_DATE_LENGTH, (pri_time_t)event_time, event_class,
                        event_type, event_action);
   }
   else
   {
      va_list ap;

      length = snprintf(event_buffer,
                        MAX_TIME_T_LENGTH + 1 + MAX_INT_LENGTH + 1 + MAX_INT_LENGTH + 1 + MAX_INT_LENGTH + 1 + MAX_DIR_ALIAS_LENGTH + MAX_HOSTNAME_LENGTH + MAX_USER_NAME_LENGTH + MAX_EVENT_REASON_LENGTH + 1,
#if SIZEOF_TIME_T == 4
                        "%-*lx %x %x %x%c",
#else
                        "%-*llx %x %x %x%c",
#endif
                        LOG_DATE_LENGTH, (pri_time_t)event_time, event_class,
                        event_type, event_action, SEPARATOR_CHAR);
      va_start(ap, fmt);
      length += vsnprintf(&event_buffer[length],
                          (MAX_TIME_T_LENGTH + 1 + MAX_INT_LENGTH + 1 + MAX_INT_LENGTH + 1 + MAX_INT_LENGTH + 1 + MAX_DIR_ALIAS_LENGTH + MAX_HOSTNAME_LENGTH + MAX_USER_NAME_LENGTH + MAX_EVENT_REASON_LENGTH) - length,
                          fmt, ap);
      va_end(ap);
   }
   if (length > (MAX_TIME_T_LENGTH + 1 + MAX_INT_LENGTH + 1 + MAX_INT_LENGTH + 1 + MAX_INT_LENGTH + 1 + MAX_DIR_ALIAS_LENGTH + MAX_HOSTNAME_LENGTH + MAX_USER_NAME_LENGTH + MAX_EVENT_REASON_LENGTH))
   {
      length = MAX_TIME_T_LENGTH + 1 + MAX_INT_LENGTH + 1 + MAX_INT_LENGTH + 1 + MAX_INT_LENGTH + 1 + MAX_DIR_ALIAS_LENGTH + MAX_HOSTNAME_LENGTH + MAX_USER_NAME_LENGTH + MAX_EVENT_REASON_LENGTH;
   }
   event_buffer[length++] = '\n';
   if (write(event_log_fd, event_buffer, length) != length)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__, _("write() error : %s"),
                 strerror(errno));
   }

   return;
}
