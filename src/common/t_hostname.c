/*
 *  t_hostname.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2015 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   t_hostname - truncates the hostname to a certain length
 **
 ** SYNOPSIS
 **   void t_hostname(char *hostname, char *trunc_hostname)
 **
 ** DESCRIPTION
 **   The function t_hostname() truncates the hostname 'hostname'
 **   to the length MAX_HOSTNAME_LENGTH. Also if it finds a ':'
 **   or a '.' in the hostname the rest gets truncated as well.
 **
 ** RETURN VALUES
 **   Returns the truncated hostname in 'trunc_hostname'.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.01.1996 H.Kiehl Created
 **   21.04.2015 H.Kiehl Try not to return an empty string.
 **
 */
DESCR__E_M3


/*############################# t_hostname() ############################*/
void
t_hostname(char *hostname, char *trunc_hostname)
{
   register int i = 0;

   while ((hostname[i] != '\0') &&
          (hostname[i] != '\n') &&
          (hostname[i] != ':') &&
          (hostname[i] != '.') &&
          (i != MAX_HOSTNAME_LENGTH))
   {
      trunc_hostname[i] = hostname[i];
      i++;
   }
   if (i == 0)
   {
      (void)strcpy(trunc_hostname, hostname);
   }
   else
   {
      trunc_hostname[i] = '\0';
   }

   return;
}
