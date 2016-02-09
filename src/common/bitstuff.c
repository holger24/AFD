/*
 *  bitstuff.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   bitstuff - functions to set or test bits in a bit field larger
 **              then four bytes
 **
 ** SYNOPSIS
 **   void bitset(unsigned char *bf, int bit_no)
 **   int  bittest(unsigned char *bf, int bit_no)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None for function bitset(). bittest() returns YES when bit is set
 **   otherwise NO is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   05.05.1999 H.Kiehl Created, function from H.Lepper.
 **
 */
DESCR__E_M3


/*############################## bitset() ###############################*/
void
bitset(unsigned char *bf, int bit_no)
{
   register int i,
                j,
                k;

   i = bit_no / 8;  /* Retrief byte number        */
   j = bit_no % 8;  /* Retrief bit number in byte */
   k = 0x80 >> j;   /* Generate bit mask          */
   bf[i] |= k;      /* Set bit                    */

   return;
}


/*############################## bittest() ##############################*/
int
bittest(unsigned char *bf, int bit_no)
{
   register int i,
                j,
                k;

   i = bit_no / 8;  /* Retrief byte number        */
   j = bit_no % 8;  /* Retrief bit number in byte */
   k = 0x80 >> j;   /* Generate bit mask          */
   j = bf[i] & k;   /* Test bit                   */

   return(j ? YES : NO);
}
