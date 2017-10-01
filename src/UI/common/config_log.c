/*
 *  config_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2004 - 2017 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   config_log - log configuration entries to system log
 **
 ** SYNOPSIS
 **   void config_log(unsigned int event_class,
 **                   unsigned int event_type,
 **                   unsigned int event_action,
 **                   char         *alias,
 **                   char         *fmt, ...)
 **
 ** DESCRIPTION
 **   This function logs all configuration options to SYSTEM_LOG and
 **   EVENT_LOG.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   28.03.2004 H.Kiehl Created
 **   24.06.2007 H.Kiehl Added event log.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* strcpy(), strcat(), strerror()  */
#include <stdarg.h>                   /* va_start(), va_end()            */
#include <time.h>                     /* time(), localtime()             */
#include <sys/types.h>
#include <unistd.h>                   /* write()                         */
#include <fcntl.h>
#include <errno.h>
#include "ui_common_defs.h"
#include "ea_str.h"

/* External global variables. */
extern int  sys_log_fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
extern int  sys_log_readfd;
#endif
extern char *p_work_dir,
            user[];


/*############################ config_log() #############################*/
void
config_log(unsigned int event_class,
           unsigned int event_type,
           unsigned int event_action,
           char         *alias,
           char         *fmt, ...)
{
   size_t    length,
             pos1 = 0,
             pos2;
   time_t    tvalue;
   char      buf[MAX_LINE_LENGTH + 1];
   va_list   ap;
   struct tm *p_ts;

   /*
    * Only open sys_log_fd to SYSTEM_LOG when it is STDERR_FILENO.
    * If it is STDOUT_FILENO it is an X application and here we do
    * NOT wish to write to SYSTEM_LOG.
    */
   if ((sys_log_fd == STDERR_FILENO) && (p_work_dir != NULL))
   {
      char sys_log_fifo[MAX_PATH_LENGTH];

      (void)strcpy(sys_log_fifo, p_work_dir);
      (void)strcat(sys_log_fifo, FIFO_DIR);
      (void)strcat(sys_log_fifo, SYSTEM_LOG_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(sys_log_fifo, &sys_log_readfd, &sys_log_fd) == -1)
#else
      if ((sys_log_fd = open(sys_log_fifo, O_RDWR)) == -1)
#endif
      {
         if (errno == ENOENT)
         {
            if ((make_fifo(sys_log_fifo) == SUCCESS) &&
#ifdef WITHOUT_FIFO_RW_SUPPORT
                (open_fifo_rw(sys_log_fifo, &sys_log_readfd, &sys_log_fd) == -1))
#else
                ((sys_log_fd = open(sys_log_fifo, O_RDWR)) == -1))
#endif
            {
               (void)fprintf(stderr,
                             "WARNING : Could not open fifo %s : %s (%s %d)\n",
                             SYSTEM_LOG_FIFO, strerror(errno),
                             __FILE__, __LINE__);
            }
         }
         else
         {
            (void)fprintf(stderr,
                          "WARNING : Could not open fifo %s : %s (%s %d)\n",
                          SYSTEM_LOG_FIFO, strerror(errno), __FILE__, __LINE__);
         }
      }
   }

   tvalue = time(NULL);
   p_ts    = localtime(&tvalue);
   if (p_ts == NULL)
   {
      buf[0]  = '?';
      buf[1]  = '?';
      buf[2]  = ' ';
      buf[3]  = '?';
      buf[4]  = '?';
      buf[5]  = ':';
      buf[6]  = '?';
      buf[7]  = '?';
      buf[8]  = '?';
      buf[9]  = '?';
      buf[10] = '?';
   }
   else
   {
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
   }
   buf[11] = ' ';
   buf[12] = '<';
   buf[13] = 'C';
   buf[14] = '>';
   buf[15] = ' ';
   length = 16;

   if (alias == NULL)
   {
      length += snprintf(&buf[length], MAX_LINE_LENGTH - length, "%s",
                         (event_action > EA_MAX_EVENT_ACTION) ? "Undefined action no." : eastr[event_action]);
   }
   else
   {
      int alias_length;

      if (event_class == EC_HOST)
      {
         alias_length = MAX_HOSTNAME_LENGTH;
      }
      else
      {
         alias_length = MAX_DIR_ALIAS_LENGTH;
      }
      length += snprintf(&buf[length], MAX_LINE_LENGTH - length, "%-*s: %s",
                         alias_length, alias,
                         (event_action > EA_MAX_EVENT_ACTION) ? "Undefined action no." : eastr[event_action]);
   }
   if (fmt != NULL)
   {
      if (length > MAX_LINE_LENGTH)
      {
         length = MAX_LINE_LENGTH;
      }
      else
      {
         buf[length++] = ' ';
         pos1 = length;
         va_start(ap, fmt);
         length += vsnprintf(&buf[length], MAX_LINE_LENGTH - length, fmt, ap);
         va_end(ap);

         if (length > MAX_LINE_LENGTH)
         {
            length = MAX_LINE_LENGTH;
         }
      }
   }

   pos2 = length;
   length += snprintf(&buf[length], MAX_LINE_LENGTH - length, " (%s)\n", user);
   if (length > MAX_LINE_LENGTH)
   {
      length = MAX_LINE_LENGTH;
   }
   if (sys_log_fd != STDOUT_FILENO)
   {
      if (write(sys_log_fd, buf, length) != length)
      {
         (void)fprintf(stderr, "write() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
      }
   }

   buf[pos2 + (length - pos2 - 2)] = '\0';
   if (alias == NULL)
   {
      if (fmt == NULL)
      {
         event_log(tvalue, event_class, event_type, event_action,
                   "%s", &buf[pos2 + 2]);
      }
      else
      {
         buf[pos1] = '\0';
         event_log(tvalue, event_class, event_type, event_action,
                   "%x%c%s%c%s", &buf[pos1], SEPARATOR_CHAR, &buf[pos2 + 2]);
      }
   }
   else
   {
      if (fmt == NULL)
      {
         event_log(tvalue, event_class, event_type, event_action,
                   "%s%c%s", alias, SEPARATOR_CHAR, &buf[pos2 + 2]);
      }
      else
      {
         buf[pos2] = '\0';
         event_log(tvalue, event_class, event_type, event_action,
                   "%s%c%s%c%s", alias, SEPARATOR_CHAR,
                   &buf[pos2 + 2], SEPARATOR_CHAR, &buf[pos1]);
      }
   }

   return;
}
