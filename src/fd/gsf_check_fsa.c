/*
 *  gsf_check_fsa.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   gsf_check_fsa - checks if FSA has been changed
 **
 ** SYNOPSIS
 **   int gsf_check_fsa(struct job *p_db)
 **
 ** DESCRIPTION
 **   Checks if the FSA has changed. If it did change it attaches
 **   the new FSA, search for the host, detach and then attach only
 **   to this position.
 **
 ** RETURN VALUES
 **   Returns NO when FSA did not change. It will return YES when it
 **   has changed and has successfully attached to the new FSA. If it
 **   fails to attach to the new FSA, NEITHER is returned. Also, when
 **   the FSA has changed and it has mapped to the new host
 **   position db.fsa_pos will be set. When it fails to map it
 **   will set db.fsa_pos to INCORRECT.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   18.03.2003 H.Kiehl Created
 **   09.04.2021 H.Kiehl Introduce global variable fsa_pos_save to
 **                      indicate if the fsa pointer is save to use.
 **   12.02.2023 H.Kiehl Add maintainer_log() debug information when
 **                      FSA has changed.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include "fddefs.h"

/* Global variables. */
extern int                        fsa_pos_save,
                                  *p_no_of_hosts,
                                  no_of_hosts;
extern struct filetransfer_status *fsa;


/*########################### gsf_check_fsa() ###########################*/
int
gsf_check_fsa(struct job *p_db)
{
   int ret;

   if (p_db->fsa_pos == INCORRECT)
   {
      ret = NEITHER;
   }
   else
   {
      if ((p_no_of_hosts != NULL) && (*p_no_of_hosts == STALE))
      {
#ifdef _MAINTAINER_LOG
         maintainer_log(DEBUG_SIGN, __FILE__, __LINE__,
# if SIZEOF_PID_T == 4
                        "FSA before change: %s old_fsa_pos=%d job_no=%d pid=%d",
# else
                        "FSA before change: %s old_fsa_pos=%d job_no=%d pid=%lld",
# endif
                        p_db->host_alias, p_db->fsa_pos, (int)p_db->job_no,
                        (pri_pid_t)p_db->my_pid);
#endif
         fsa_pos_save = NO;
         fsa_detach_pos(p_db->fsa_pos);
         if (fsa_attach("sf/gf_xxx") == SUCCESS)
         {
            p_db->fsa_pos = get_host_position(fsa, p_db->host_alias, no_of_hosts);
            (void)fsa_detach(NO);
            if (p_db->fsa_pos != INCORRECT)
            {
               if ((ret = fsa_attach_pos(p_db->fsa_pos)) != SUCCESS)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to attach to FSA position %d (%d).",
                             p_db->fsa_pos, ret);
                  p_db->fsa_pos = INCORRECT;
                  ret = NEITHER;
               }
               else
               {
                  p_db->lock_offset = AFD_WORD_OFFSET +
                                      (p_db->fsa_pos * sizeof(struct filetransfer_status));
                  ret = YES;
#ifdef _MAINTAINER_LOG
                  maintainer_log(DEBUG_SIGN, __FILE__, __LINE__,
# if SIZEOF_PID_T == 4
                                 "FSA after change: %s old_fsa_pos=%d job_no=%d pid=%d",
# else
                                 "FSA after change: %s old_fsa_pos=%d job_no=%d pid=%lld",
# endif
                                 p_db->host_alias, p_db->fsa_pos,
                                 (int)p_db->job_no, (pri_pid_t)p_db->my_pid);
#endif
               }
            }
            else
            {
               ret = NEITHER;
            }
         }
         else
         {
            p_db->fsa_pos = INCORRECT;
            ret = NEITHER;
         }
         fsa_pos_save = YES;
      }
      else
      {
         ret = NO;
      }
   }
   return(ret);
}
