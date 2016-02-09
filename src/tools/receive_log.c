/*
 *  receive_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2013 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   void receive_log(char *sign, char *file, int line, time_t current_time, char *fmt, ...)
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
#include <string.h>                   /* memcpy(), strerror()            */
#include <stdarg.h>                   /* va_start(), va_end()            */
#include <sys/types.h>
#include <unistd.h>                   /* write()                         */
#include <errno.h>
#include "amgdefs.h"

extern int receive_log_fd;


/*########################### receive_log() #############################*/
void
receive_log(char   *sign,
            char   *file,
            int    line,
            time_t current_time,
            char   *fmt, ...)
{
   size_t  length = 0;
   char    buf[MAX_LINE_LENGTH + MAX_LINE_LENGTH];
   va_list ap;

   va_start(ap, fmt);
   length += vsprintf(buf, fmt, ap);
   va_end(ap);

   if ((file == NULL) || (line == 0))
   {
      buf[length] = '\n';
      length += 1;
   }
   else
   {
      length += sprintf(&buf[length], " (%s %d)\n", file, line);
   }

   if (write(receive_log_fd, buf, length) == -1)
   {
      (void)fprintf(stderr, "write() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
   }

   return;
}
