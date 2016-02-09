/*
 *  enter_time_job.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2004 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   enter_time_job - 
 **
 ** SYNOPSIS
 **   void enter_time_job(int pos)
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
 **   12.05.1999 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>           /* strerror()                              */
#include <stdlib.h>           /* realloc()                               */
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int no_of_time_jobs,
           *time_job_list;


/*########################## enter_time_job() ###########################*/
void
enter_time_job(int pos)
{
   if ((no_of_time_jobs % TIME_JOB_STEP_SIZE) == 0)
   {
      size_t new_size = (((no_of_time_jobs / TIME_JOB_STEP_SIZE) + 1) *
                        TIME_JOB_STEP_SIZE * sizeof(int));

      if ((time_job_list = realloc(time_job_list, new_size)) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "mmap_resize() error : %s", strerror(errno));
         exit(INCORRECT);
      }
   }

   /*
    * Enter the position in the structure instant_db to the time job list.
    */
   time_job_list[no_of_time_jobs] = pos;
   no_of_time_jobs++;

   return;
}
