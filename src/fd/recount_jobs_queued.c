/*
 *  recount_jobs_queued.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 - 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   recount_jobs_queued - counts the number of jobs queued
 **
 ** SYNOPSIS
 **   int recount_jobs_queued(int fsa_pos)
 **
 ** DESCRIPTION
 **   The function recount_jobs_queued() counts the number of jobs
 **   queued for the given FSA position 'fsa_pos'.
 **
 ** RETURN VALUES
 **   Number of jobs queued for given fsa position.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.01.2005 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "fddefs.h"

/* External global variables. */
extern int                        *no_msg_queued;
extern struct fileretrieve_status *fra;
extern struct queue_buf           *qb;
extern struct msg_cache_buf       *mdb;


/*######################### recount_jobs_queued() #######################*/
int
recount_jobs_queued(int fsa_pos)
{
   register int i,
                jobs_queued = 0;

   for (i = 0; i < *no_msg_queued; i++)
   {
      if (qb[i].pid == PENDING)
      {
         if ((qb[i].special_flag & FETCH_JOB) == 0)
         {
            if (fsa_pos == mdb[qb[i].pos].fsa_pos)
            {
               jobs_queued++;
            }
         }
         else
         {
            if (fsa_pos == fra[qb[i].pos].fsa_pos)
            {
               jobs_queued++;
            }
         }
      }
   }
   return(jobs_queued);
}
