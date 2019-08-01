/*
 *  gsf_check_fra.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 - 2019 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   gsf_check_fra - checks if FRA has been changed
 **
 ** SYNOPSIS
 **   int gsf_check_fra(struct job *p_db)
 **
 ** DESCRIPTION
 **   Checks if the FRA has changed. If it did change it attaches
 **   the new FRA, search for the directory, detaches and then attaches
 **   again to the new FRA.
 **
 ** RETURN VALUES
 **   Returns NO when FRA did not change. It will return YES when it
 **   has changed and has successfully attached to the new FRA. If it
 **   fails to attach to the new FRA, NEITHER is returned. Also, when
 **   the FRA has changed and it has mapped to the new directory
 **   position db.fra_pos will be set. When it fails to map it
 **   will set db.fra_pos to INCORRECT.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   08.03.2008 H.Kiehl Created
 **   01.08.2019 H.Kiehl Rewrite so it can be used with fra_attach_pos().
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <stdlib.h>             /* malloc(), free(), exit()              */
#include <errno.h>
#include "fddefs.h"

/* Global variables. */
extern int                        *p_no_of_dirs,
                                  no_of_dirs;
extern struct fileretrieve_status *fra;


/*########################### gsf_check_fra() ###########################*/
int
gsf_check_fra(struct job *p_db)
{
   int ret;

   if (p_db->fra_pos == INCORRECT)
   {
      ret = NEITHER;
   }
   else
   {
      if ((p_no_of_dirs != NULL) && (*p_no_of_dirs == STALE))
      {
         unsigned char prev_no_of_time_entries;

         prev_no_of_time_entries = fra->no_of_time_entries;
         fra_detach_pos(p_db->fra_pos);
         if (fra_attach() == SUCCESS)
         {
            p_db->fra_pos = get_dir_id_position(fra, p_db->id.dir, no_of_dirs);
            (void)fra_detach();
            if (p_db->fra_pos != INCORRECT)
            {
               if ((ret = fra_attach_pos(p_db->fra_pos)) != SUCCESS)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to attach to FRA position %d (%d).",
                             p_db->fra_pos, ret);
                  p_db->fra_pos = INCORRECT;
                  ret = NEITHER;
               }
               else
               {
                  if (fra->no_of_time_entries == 0)
                  {
                     if (prev_no_of_time_entries > 0)
                     {
                        if ((p_db->te = malloc(sizeof(struct bd_time_entry))) == NULL)
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      "Could not malloc() memory : %s",
                                      strerror(errno));
                           exit(ALLOC_ERROR);
                        }
                        if (eval_time_str("* * * * *", &p_db->te[0], NULL) != SUCCESS)
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      "Failed to evaluate time string.");
                           exit(INCORRECT);
                        }
                     }
                  }
                  else
                  {
                     if (prev_no_of_time_entries == 0)
                     {
                        free(p_db->te);
                     }
                     p_db->te = &fra->te[0];
                  }
                  p_db->fra_lock_offset = AFD_WORD_OFFSET +
                                          (p_db->fra_pos * sizeof(struct fileretrieve_status));
                  ret = YES;
               }
            }
            else
            {
               ret = NEITHER;
            }
         }
         else
         {
            p_db->fra_pos = INCORRECT;
            ret = NEITHER;
         }
      }
      else
      {
         ret = NO;
      }
   }
   return(ret);
}
