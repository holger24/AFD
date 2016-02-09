/*
 *  check_additional_lock_filters.c - Part of AFD, an automatic file
 *                                    distribution program.
 *  Copyright (c) 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_additional_lock_filters - checks if one of the additional
 **                                   lock filters matches the given name
 **
 ** SYNOPSIS
 **   int check_additional_lock_filters(char const *name)
 **
 ** DESCRIPTION
 **   Checks if one of the additional lock filters matches with
 **   the given name.
 **
 ** RETURN VALUES
 **   Returns 1 if one of the filter matches, other wise 0 is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.04.2014 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>

/* External global variables. */
extern int  alfc;
extern char *alfiles;


/*################### check_additional_lock_filters() ###################*/
int
check_additional_lock_filters(char const *name)
{
   register int i;
   char         *p_filter = alfiles;

   for (i = 0; i < alfc; i++)
   {
      p_filter++; /* Away with the '!', we want it to match. */
      if (pmatch(p_filter, name, NULL) == 0)
      {
         return(1);
      }
      while (*p_filter != '\0')
      {
         p_filter++;
      }
      p_filter++;
   }

   return(0);
}
