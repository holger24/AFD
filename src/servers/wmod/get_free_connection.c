/*
 *  get_free_connection.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_free_connection - gets a free connection
 **
 ** SYNOPSIS
 **   int get_free_connection(int fsa_pos, pid_t current_pid)
 **
 ** DESCRIPTION
 **   The function get_free_connection() searches in the pid array
 **   for a free process slot and then asks process FD for a free
 **   job position in FSA.
 **
 ** RETURN VALUES
 **   Returns position in pid array. Otherwise it returns INCORRECT.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   20.09.2005 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "commondefs.h"


/* External global variables. */
extern int                        fsa_fd;
extern struct proclist            pl[];
extern struct filetransfer_status *fsa;


/*####################### get_free_connection() #########################*/
int
get_free_connection(int fsa_pos, pid_t current_pid)
{
   int i;

   for (i = 0; i < fsa[fsa_pos].allowed_transfers; i++)
   {
      if (pl[i].pid == 0)
      {
         if (fsa[fsa_pos].active_transfers < fsa[fsa_pos].allowed_transfers)
         {
            int   j;
            off_t lock_offset;

            pl[i].job_pos = -1;
            lock_offset = AFD_WORD_OFFSET +
                          (fsa_pos * sizeof(struct filetransfer_status));
#ifdef LOCK_DEBUG
            lock_region_w(fsa_fd, lock_offset + LOCK_CON, __FILE__, __LINE__);
#else
            lock_region_w(fsa_fd, lock_offset + LOCK_CON);
#endif
            for (j = 0; j < fsa[fsa_pos].allowed_transfers; j++)
            {
               if (fsa[fsa_pos].job_status[j].proc_id < 1)
               {
                  fsa[fsa_pos].job_status[j].proc_id = current_pid;
                  pl[i].job_pos = j;
                  fsa[fsa_pos].active_transfers++;
                  if ((fsa[fsa_pos].active_transfers > 1) &&
                      (fsa[fsa_pos].transfer_rate_limit > 0))
                  {
                     fsa[fsa_pos].trl_per_process = fsa[fsa_pos].transfer_rate_limit /
                                                    fsa[fsa_pos].active_transfers;
                     if (fsa[fsa_pos].trl_per_process == 0)
                     {
                        fsa[fsa_pos].trl_per_process = 1;
                     }
                  }
                  else
                  {
                     fsa[fsa_pos].trl_per_process = fsa[fsa_pos].transfer_rate_limit;
                  }
                  break;
               }
            }
#ifdef LOCK_DEBUG
            unlock_region(fsa_fd, lock_offset + LOCK_CON, __FILE__, __LINE__);
#else
            unlock_region(fsa_fd, lock_offset + LOCK_CON);
#endif
            if (pl[i].job_pos == -1)
            {
               i = INCORRECT;
            }
         
            return(i);
         }
         else
         {
            return(INCORRECT);
         }
      }
   }

   return(INCORRECT);
}
