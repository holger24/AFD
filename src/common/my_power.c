/*
 *  my_power.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2013 Deutscher Wetterdienst (DWD),
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
 **   my_power - simple power math function
 **
 ** SYNOPSIS
 **   int my_power(int x, int n)
 **
 ** DESCRIPTION
 **   Calcultes x^n without any floats.
 **
 ** RETURN VALUES
 **   Returns the result of x^n.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   07.08.2013 H.Kiehl Created.
 **
 */
DESCR__E_M3


/*############################## bitset() ###############################*/
int
my_power(int x, int n)
{
   int ret = 1;

   while (n > 0)
   {
      ret *= x;
      n--;
   }

   return(ret);
}
