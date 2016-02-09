/*
 *  reset_fsa.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   reset_fsa - resets certain values in the FSA
 **
 ** SYNOPSIS
 **   void reset_fsa(struct job *p_db, int faulty, int mode,
 **                  int file_total_shown, off_t file_size_total_shown)
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
 **   14.12.1996 H.Kiehl Created
 **   30.04.2012 H.Kiehl Added possibility to reset the values
 **                      total_file_counter and total_file_size.
 **
 */
DESCR__E_M3

#include "fddefs.h"

/* External global variables. */
extern int                        fsa_fd;
extern struct filetransfer_status *fsa;


/*############################# reset_fsa() #############################*/
void
reset_fsa(struct job *p_db,
          int        mode,
          int        file_total_shown,
          off_t      file_size_total_shown)
{
   if ((fsa != NULL) && (p_db->fsa_pos != INCORRECT))
   {
      if (gsf_check_fsa(p_db) != NEITHER)
      {
         if (mode & IS_FAULTY_VAR)
         {
            fsa->job_status[(int)p_db->job_no].connect_status = NOT_WORKING;
         }
         else
         {
            fsa->job_status[(int)p_db->job_no].connect_status = DISCONNECT;
         }
         fsa->job_status[(int)p_db->job_no].no_of_files_done = 0;
         fsa->job_status[(int)p_db->job_no].file_size_done = 0;
         fsa->job_status[(int)p_db->job_no].file_size_in_use = 0;
         fsa->job_status[(int)p_db->job_no].file_size_in_use_done = 0;
         fsa->job_status[(int)p_db->job_no].file_name_in_use[0] = '\0';
         fsa->job_status[(int)p_db->job_no].file_name_in_use[1] = 0;
         fsa->job_status[(int)p_db->job_no].no_of_files = 0;
         fsa->job_status[(int)p_db->job_no].file_size = 0;
         if ((file_total_shown != 0) || (file_size_total_shown != 0))
         {
#ifdef LOCK_DEBUG
            lock_region_w(fsa_fd, p_db->lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
            lock_region_w(fsa_fd, p_db->lock_offset + LOCK_TFC);
#endif
            if (file_total_shown != 0)
            {
               fsa->total_file_counter -= file_total_shown;
#ifdef _VERIFY_FSA
               if (fsa->total_file_counter < 0)
               {
                  trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Total file counter less then zero. Correcting to 0.");
                  fsa->total_file_counter = 0;
               }
#endif
            }
            if (file_size_total_shown != 0)
            {
               fsa->total_file_size -= file_size_total_shown;
#ifdef _VERIFY_FSA
               if (fsa->total_file_size < 0)
               {
                  trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Total file size less then zero. Correcting to 0.");
                  fsa->total_file_size = 0;
               }
#endif
            }
#ifdef LOCK_DEBUG
            unlock_region(fsa_fd, p_db->lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
            unlock_region(fsa_fd, p_db->lock_offset + LOCK_TFC);
#endif
         }
      }
   }

   return;
}
