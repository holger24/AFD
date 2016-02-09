/*
 *  get_job_priority.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2010, 2011 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_job_priority - gets the priority from Job ID Database (JID)
 **
 ** SYNOPSIS
 **   int get_job_priority(unsigned int job_id)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   On success it will return the priority, otherwise 0 is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   08.03.2010 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdlib.h>
#include "mafd_ctrl.h"


/*########################## get_job_priority() ##########################*/
int
get_job_priority(unsigned int job_id)
{
   int                no_of_job_ids;
   struct job_id_data *jd = NULL;

   if (read_job_ids(NULL, &no_of_job_ids, &jd) == SUCCESS)
   {
      int i;

      for (i = 0; i < no_of_job_ids; i++)
      {
         if (job_id == jd[i].job_id)
         {
            int priority = (int)jd[i].priority;

            free(jd);
            return(priority);
         }
      }
   }
   free(jd);

   return(0);
}
