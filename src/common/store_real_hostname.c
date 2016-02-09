/*
 *  store_real_hostname.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   store_real_hostname - stores real hostname masking the :
 **
 ** SYNOPSIS
 **   void store_real_hostname(char *new_real_hostname, char *old_real_hostname)
 **
 ** DESCRIPTION
 **   The function store_real_hostname() checks if the name contains
 **   a : and if that is case masks it with a \.
 **
 ** RETURN VALUES
 **   Returns the modified real hostname in 'new_real_hostname'.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.08.2012 H.Kiehl Created
 **
 */
DESCR__E_M3


/*######################## store_real_hostname() ########################*/
void
store_real_hostname(char *new_real_hostname, char *old_real_hostname)
{
   register int i = 0,
                j = 0;

   while ((old_real_hostname[i] != '\0') &&
          (i != MAX_REAL_HOSTNAME_LENGTH))
   {
      if (old_real_hostname[i] == ':')
      {
         new_real_hostname[j] = '\\';
         j++;
      }
      new_real_hostname[j] = old_real_hostname[i];
      i++; j++;
   }
   new_real_hostname[j] = '\0';

   return;
}
