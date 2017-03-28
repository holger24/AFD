/*
 *  get_vpl_pos.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2017 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_vpl_pos - searches for the fsa_pos in the visible position list
 **
 ** SYNOPSIS
 **   int get_vpl_pos(int fsa_pos)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   On success it will return the position in the visible position list
 **   (vpl). If it is not found INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.03.2017 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "mafd_ctrl.h"

/* External global variables. */
extern int no_of_hosts,
           *vpl;


/*############################ get_vpl_pos() ############################*/
int
get_vpl_pos(int fsa_pos)
{
   int i;

   for (i = 0; i < no_of_hosts; i++)
   {
      if (vpl[i] == fsa_pos)
      {
         return(i);
      }
   }

   return(INCORRECT);
}
