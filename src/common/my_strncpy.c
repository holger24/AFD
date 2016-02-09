/*
 *  my_strncpy.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   my_strncpy - copy a string
 **
 ** SYNOPSIS
 **   int my_strncpy(char *dest, const char *src, const size_t n)
 **
 ** DESCRIPTION
 **   The my_strncpy() function copies the string pointed to by src
 **   to the array pointed to by dest. Only n bytes will be copied.
 **   dest will always be terminated with a null byte.
 **
 ** RETURN VALUES
 **   If n is not reached SUCCESS will be returned, otherwise -1 is
 **   returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   31.08.2002 H.Kiehl Created
 **
 */
DESCR__E_M3


/*############################# my_strncpy() ############################*/
int
my_strncpy(char *dest, const char *src, const size_t n)
{
   size_t i = 0;

   while ((i < n) && (src[i] != '\0'))
   {
      dest[i] = src[i];
      i++;
   }
   if (i == n)
   {
      dest[i - 1] = '\0';
      return(-1);
   }
   else
   {
      dest[i] = '\0';
      return(SUCCESS);
   }
}
