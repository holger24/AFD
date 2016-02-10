/*
 *  my_strcmp.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2015 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   my_strcmp - compare two strings
 **
 ** SYNOPSIS
 **   int strcmp(const char *s1, const char *s2)
 **
 ** DESCRIPTION
 **   The function my_strcmp() compares the two strings s1 and s2.
 **   It returns 0 if they match, otherwise 1 is returned.
 **
 ** RETURN VALUES
 **   Returns 0 if the two strings match, otherwise 1 is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.10.2015 H.Kiehl Created
 **
 */
DESCR__E_M3


/*############################# my_strcmp() #############################*/
int
my_strcmp(const char *s1, const char *s2)
{
   while (*s1 == *s2)
   {
     if (*s1 == '\0')
     {
        return(0);
     }
     s1++; s2++;
   }

   return(1);
}
