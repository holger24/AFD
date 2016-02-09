/*
 *  lookup_fra_pos.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   lookup_fra_pos - searches the FRA position for the given
 **                    directory alias
 **
 ** SYNOPSIS
 **   int lookup_fra_pos(char *dir_alias)
 **
 ** DESCRIPTION
 **   This function searches in the FRA for the dir_alias and if
 **   found returns the position in the FRA.
 **
 ** RETURN VALUES
 **   Returns the position in the FRA when successful, otherwise
 **   INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   09.08.2000 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>           /* strcmp()                                */
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int                        no_of_dirs;
extern struct fileretrieve_status *fra;


/*########################## lookup_fra_pos() ###########################*/
int
lookup_fra_pos(char *dir_alias)
{
   register int i;

   for (i = 0; i < no_of_dirs; i++)
   {
      if (CHECK_STRCMP(fra[i].dir_alias, dir_alias) == 0)
      {
         return(i);
      }
   }

   return(INCORRECT);
}
