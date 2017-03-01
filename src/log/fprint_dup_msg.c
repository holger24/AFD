/*
 *  fprint_dup_msg.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2017 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   fprint_dup_msg - writes a duplicate message
 **
 ** SYNOPSIS
 **   int fprint_dup_msg(FILE   *p_log_file,
 **                      int    dup_msg,
 **                      char   *sign,
 **                      char   *host_pos_str,
 **                      int    offset,
 **                      time_t now)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   Returns the number bytes printed.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.08.1999 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                    /* fprintf()                       */
#include <time.h>                     /* localtime()                     */
#ifdef TM_IN_SYS_TIME
# include <sys/time.h>                /* struct tm                       */
#endif
#include "logdefs.h"


/*########################## fprint_dup_msg() ###########################*/
int
fprint_dup_msg(FILE   *p_log_file,
               int    dup_msg,
               char   *sign,
               char   *host_pos_str,
               int    offset,
               time_t now)
{
   char      buf[17];
   struct tm *p_ts;

   p_ts = localtime(&now);
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
   buf[12] = sign[0];
   buf[13] = sign[1];
   buf[14] = sign[2];
   buf[15] = ' ';
   buf[16] = '\0';

   if (host_pos_str == NULL)
   {
      return(fprintf(p_log_file, "%sLast message repeated %d times.\n",
                     buf, dup_msg));
   }
   else
   {
      char *ptr = host_pos_str + offset;

      while ((*ptr != ':') && (*ptr != '\0'))
      {
         ptr++;
      }
      if (*ptr == ':')
      {
         *(ptr + 2) = '\0';
         return(fprintf(p_log_file, "%s%sLast message repeated %d times.\n",
                        buf, host_pos_str, dup_msg));
      }
      else
      {
         return(fprintf(p_log_file, "%s%-*sLast message repeated %d times.\n",
                        buf, offset, "?", dup_msg));
      }
   }
}
