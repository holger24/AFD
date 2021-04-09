/*
 *  my_strncpy.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 - 2021 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   int    my_strncpy(char *dest, const char *src, const size_t n)
 **   size_t my_strlcpy(char *dest, const char *src, const size_t n)
 **   void   my_fillncpy(char *dest, const char *src,
 **                      const char fill_char, const size_t n)
 **
 ** DESCRIPTION
 **   The my_strncpy() function copies the string pointed to by src
 **   to the array pointed to by dest. Only n bytes will be copied.
 **   dest will always be terminated with a null byte.
 **
 **   my_strlcpy() function differs from my_strncpy() in that
 **   it returns the number of bytes it has copied.
 **
 **   Function my_fillncpy() copies n bytes from src to dest, if
 **   src is shorter then n, the remaining bytes are filled by
 **   fill_char. dest will be null-terminated.
 **
 ** RETURN VALUES
 **   In my_strncpy() if n is not reached SUCCESS will be returned,
 **   otherwise -1 is returned.
 **
 **   Function my_strlcpy() returns the number of bytes copied.
 **
 **   None for function my_fillncpy().
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   31.08.2002 H.Kiehl Created
 **   18.12.2017 H.Kiehl Added function my_fillncpy().
 **   23.03.2021 H.Kiehl Added function my_strlcpy().
 **                      Fixed case where n is zero for my_strncpy().
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
      if (n > 1)
      {
         dest[i - 1] = '\0';
      }
      return(-1);
   }
   else
   {
      dest[i] = '\0';
      return(SUCCESS);
   }
}


/*############################# my_strlcpy() ############################*/
size_t
my_strlcpy(char *dest, const char *src, const size_t n)
{
   size_t i = 0;

   while ((i < n) && (src[i] != '\0'))
   {
      dest[i] = src[i];
      i++;
   }
   if (i == n)
   {
      if (n > 1)
      {
         dest[i - 1] = '\0';
         i--;
      }
   }
   else
   {
      dest[i] = '\0';
   }

   return(i);
}


/*############################# my_fillncpy() ###########################*/
void
my_fillncpy(char *dest, const char *src, const char fill_char, const size_t n)
{
   size_t i = 0;

   while ((i < n) && (src[i] != '\0'))
   {
      dest[i] = src[i];
      i++;
   }
   while (i < n)
   {
      dest[i] = fill_char;
      i++;
   }
   dest[i] = '\0';

   return;
}
