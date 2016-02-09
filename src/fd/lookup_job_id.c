/*
 *  lookup_job_id.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2003 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   lookup_job_id - searches message cache for job ID
 **
 ** SYNOPSIS
 **   int lookup_job_id(unsigned int job_id)
 **
 ** DESCRIPTION
 **   This function searches in the message cache 'mdb' for the
 **   job ID 'job_id'. If it does not find it, it tries to load
 **   the message from the message directory.
 **
 ** RETURN VALUES
 **   Returns the job ID position in the message cache if it could
 **   be found. Otherwise INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.04.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "fddefs.h"

/* External global variables. */
extern int                  *no_msg_cached;
extern struct msg_cache_buf *mdb;


/*########################### lookup_job_id() ###########################*/
int
lookup_job_id(unsigned int job_id)
{
   register int i;

   for (i = 0; i < *no_msg_cached; i++)
   {
      if (mdb[i].job_id == job_id)
      {
         return(i);
      }
   }

   /*
    * Hmm, message not in cache lets try and read it from the
    * message directory. If we find it store it into the
    * cache.
    */
   if (get_job_data(job_id, -1, 0L, 0) == SUCCESS)
   {
      i = *no_msg_cached - 1;
   }
   else
   {
      i = INCORRECT;
   }

   return(i);
}
