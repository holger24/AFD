/*
 *  next.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 Deutscher Wetterdienst (DWD),
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
 **   next - positions pointer just after binary zero
 **
 ** SYNOPSIS
 **   char *next(char *ptr)
 **
 ** DESCRIPTION
 **   This function is used to walk through elements of the shared
 **   memory areas of the AMG. It always places the pointer 'ptr'
 **   after the first '\0' it finds.
 **
 ** RETURN VALUES
 **   Returns pointer 'ptr' just after binary zero.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.10.1995 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "amgdefs.h"


/*################################# next() ##############################*/
char *
next(char *ptr)
{
   while (*ptr != '\0')
   {
      ptr++;
   }
   ptr++;

   return(ptr);
}
