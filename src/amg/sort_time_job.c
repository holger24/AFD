/*
 *  sort_time_job.c - Part of AFD, an automatic file distribution program.
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
 **   sort_time_job - sorts the time job list according to priority
 **
 ** SYNOPSIS
 **   void sort_time_job(void)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.05.1999 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>             /* strerror()                            */
#include <stdlib.h>             /* malloc(), free()                      */
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int               no_of_time_jobs,
                         *time_job_list;
extern struct instant_db *db;


/*########################## sort_time_job() ############################*/
void
sort_time_job(void)
{
   int *tmp_time_job_list;

   if ((tmp_time_job_list = malloc(no_of_time_jobs * sizeof(int))) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
   }
   else
   {
      int done = 0,
          i,
          j;

      for (i = 0; i < 10; i++)
      {
         for (j = 0; j < no_of_time_jobs; j++)
         {
            if ((time_job_list[j] != -1) &&
                ((char)(i + 48) == db[time_job_list[j]].priority))
            {
               tmp_time_job_list[done] = time_job_list[j];
               time_job_list[j] = -1;
               done++;
            }
         }
         if (done == no_of_time_jobs)
         {
            break;
         }
      }

      (void)memcpy(time_job_list, tmp_time_job_list,
                   (no_of_time_jobs * sizeof(int)));
      free(tmp_time_job_list);
   }

   return;
}
