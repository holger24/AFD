/*
 *  show_host_stat.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2013 - 2025 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   show_host_stat - Shows the current hosts and their status
 **
 ** SYNOPSIS
 **   void show_host_stat(SSL *ssl)
 **
 ** DESCRIPTION
 **   This function prints a list of all host currently being served
 **   by this AFD in the following format:
 **     HL <host_number> <host alias> <real hostname 1> [<real hostname 2>]
 **     HS <host_number> <host status> <error counter> <active transfers>
 **                      <files send> <bytes send> <files queued>
 **                      <bytes queued> <toggle pos> <last connect time>
 **     EL <host_number> <error code 1> ... <error code n>
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.06.2013 H.Kiehl Created
 **   29.03.2017 H.Kiehl Do not show all data of group entries.
 */
DESCR__E_M3

#include <stdio.h>
#include "afddsdefs.h"

/* External global variables. */
extern int                        no_of_hosts;
extern unsigned char              **old_error_history;
extern struct filetransfer_status *fsa;


/*########################### show_host_stat() ##########################*/
void
show_host_stat(SSL *ssl)
{
   int  i, k,
        length;
   char ebuf[((ERROR_HISTORY_LENGTH - 1) * (MAX_INT_LENGTH + 1)) + 1];

   (void)command(ssl, "211- AFD host status:");
   (void)command(ssl, "NH %d", no_of_hosts);

   for (i = 0; i < no_of_hosts; i++)
   {
      if (fsa[i].real_hostname[0][0] == GROUP_IDENTIFIER)
      {
         (void)command(ssl, "HL %d %s", i, fsa[i].host_alias);
         (void)command(ssl, "HS %d %d 0 0 0 0 0 0 %d 0",
                       i, fsa[i].host_status, HOST_ONE);
      }
      else
      {
         if (fsa[i].real_hostname[1][0] == '\0')
         {
            (void)command(ssl, "HL %d %s %s", i, fsa[i].host_alias,
                          fsa[i].real_hostname[0]);
         }
         else
         {
            (void)command(ssl, "HL %d %s %s %s", i, fsa[i].host_alias,
                          fsa[i].real_hostname[0], fsa[i].real_hostname[1]);
         }
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
         (void)command(ssl, "HS %d %d %d %d %u %lu %d %ld %d %ld",
# else
         (void)command(ssl, "HS %d %d %d %d %u %lu %d %ld %d %lld",
# endif
#else
# if SIZEOF_TIME_T == 4
         (void)command(ssl, "HS %d %d %d %d %u %llu %d %lld %d %ld",
# else
         (void)command(ssl, "HS %d %d %d %d %u %llu %d %lld %d %lld",
# endif
#endif
                       i, fsa[i].host_status, fsa[i].error_counter,
                       fsa[i].active_transfers, fsa[i].file_counter_done,
                       fsa[i].bytes_send, fsa[i].total_file_counter,
                       (pri_off_t)fsa[i].total_file_size, (int)fsa[i].toggle_pos,
                       (pri_time_t)fsa[i].last_connection);
         length = 0;
         for (k = 1; k < ERROR_HISTORY_LENGTH; k++)
         {
            length += snprintf(ebuf + length,
                               (((ERROR_HISTORY_LENGTH - 1) * (MAX_INT_LENGTH + 1)) + 1) - length,
                               " %d", (int)old_error_history[i][k]);
         }
         (void)command(ssl, "EL %d %d %s",
                       i, (int)old_error_history[i][0], ebuf);
      }
   }

   return;
}
