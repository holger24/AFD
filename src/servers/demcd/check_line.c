/*
 *  check_line.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2015 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_line - checks if line contains one of the mail ID's
 **
 ** SYNOPSIS
 **   void check_line(char *line)
 **
 ** DESCRIPTION
 **   The function check_line() checks if the line contains one of
 **   the mail ID's we need to get a confirmation.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.09.2015 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>
#include "demcddefs.h"

/* External global variables. */
extern int                    *no_demcd_queued;
extern struct demcd_queue_buf *dqb;


/*############################# check_line() ############################*/
void
check_line(char *line)
{
   int i;

   for (i = 0; i < *no_demcd_queued; i++)
   {
      if (strstr(line, dqb[i].de_mail_privat_id) != NULL)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "demcd: Found %s", dqb[i].de_mail_privat_id);
         return;
      }
   }

   return;
}
