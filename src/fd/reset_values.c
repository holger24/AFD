/*
 *  reset_values.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2004 - 2024 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   reset_values - resets total_file_counter + total_file_size in FSA
 **
 ** SYNOPSIS
 **   void reset_values(int        files_retrieved,
 **                     off_t      file_size_retrieved,
 **                     int        files_to_retrieve,
 **                     off_t      file_size_to_retrieve,
 **                     struct job *p_db)
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
 **   24.04.2004 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "fddefs.h"

/* External global variables. */
extern int                        fsa_fd;
extern struct filetransfer_status *fsa;


/*############################ reset_values() ###########################*/
void
reset_values(int        files_retrieved,
             off_t      file_size_retrieved,
             int        files_to_retrieve,
             off_t      file_size_to_retrieve,
             struct job *p_db)
{
   if (((files_retrieved < files_to_retrieve) ||
        (file_size_retrieved < file_size_to_retrieve)) &&
       (p_db->fsa_pos != INCORRECT))
   {
      if (gsf_check_fsa(p_db) != NEITHER)
      {
#ifdef LOCK_DEBUG
         lock_region_w(fsa_fd, p_db->lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
         lock_region_w(fsa_fd, p_db->lock_offset + LOCK_TFC);
#endif
         if (files_retrieved < files_to_retrieve)
         {
            fsa->total_file_counter -= (files_to_retrieve - files_retrieved);
#ifdef _VERIFY_FSA
            if (fsa->total_file_counter < 0)
            {
               trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Total file counter less then zero. Correcting to 0.");
               fsa->total_file_counter = 0;
            }
#endif
         }

         if (file_size_retrieved < file_size_to_retrieve)
         {
            fsa->total_file_size -= (file_size_to_retrieve - file_size_retrieved);
#ifdef _VERIFY_FSA
            if (fsa->total_file_size < 0)
            {
               fsa->total_file_size = 0;
               trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Total file size overflowed. Correcting to 0.");
            }
            else if ((fsa->total_file_counter == 0) &&
                     (fsa->total_file_size > 0))
                 {
                    trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
                              "fc is zero but fs is not zero. Correcting.");
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
   return;
}
