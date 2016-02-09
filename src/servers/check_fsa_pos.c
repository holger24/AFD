/*
 *  check_fsa_pos.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 - 2013 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_fsa_pos - checks if FSA has changed and the fsa_pos
 **
 ** SYNOPSIS
 **   void check_fsa_pos(void)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   20.09.2005 H.Kiehl Created
 */
DESCR__E_M3

#include "commondefs.h"

/* External global variables. */
extern int                        fsa_pos,
                                  no_of_hosts;
extern char                       alias_name[];
extern struct filetransfer_status *fsa;


/*############################ check_fsa_pos() ##########################*/
void
check_fsa_pos(void)
{
   if (check_fsa(NO, "servers") == YES)
   {
      if ((fsa_pos = get_host_position(fsa, alias_name, no_of_hosts)) == INCORRECT)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Host `%s' no longer in FSA, terminating.", alias_name);
         exit(SUCCESS);
      }
   }

   return;
}
