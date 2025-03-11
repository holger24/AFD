/*
 *  show_host_list.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2025 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   show_host_list - Shows the current hosts that are being served
 **
 ** SYNOPSIS
 **   void show_host_list(SSL *ssl)
 **
 ** DESCRIPTION
 **   This function prints a list of all host currently being served
 **   by this AFD in the following format:
 **     HL <host_number> <host alias> <real hostname 1> [<real hostname 2>]
 **     EL <host_number> <error code 1> ... <error code n>
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   05.06.1999 H.Kiehl Created
 **   26.06.2004 H.Kiehl Added error history.
 **   31.03.2017 H.Kiehl Do not count the data from group identifiers.
 */
DESCR__E_M3

#include <stdio.h>
#include "afddsdefs.h"

/* External global variables. */
extern int                        no_of_hosts;
extern unsigned char              **old_error_history;
extern struct filetransfer_status *fsa;


/*########################### show_host_list() ##########################*/
void
show_host_list(SSL *ssl)
{
   int  i, k,
        length;
   char ebuf[((ERROR_HISTORY_LENGTH - 1) * (MAX_INT_LENGTH + 1)) + 1];

   (void)command(ssl, "211- AFD host list:");

   (void)command(ssl, "NH %d", no_of_hosts);

   for (i = 0; i < no_of_hosts; i++)
   {
      if (fsa[i].real_hostname[0][0] == GROUP_IDENTIFIER)
      {
         (void)command(ssl, "HL %d %s", i, fsa[i].host_alias);
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
