/*
 *  update_tfc.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2009 - 2015 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   update_tfc - Updates total_file_counter, total_file_size,
 **                file_counter_done, bytes_send and
 **                last_connection in FSA.
 **
 ** SYNOPSIS
 **   void update_tfc(int    file_counter,
 **                   off_t  file_size,
 **                   off_t  *p_file_size_buffer,
 **                   int    files_to_do,
 **                   int    current_file_pos,
 **                   time_t now)
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
 **   09.05.2009 H.Kiehl Created
 **   20.07.2014 H.Kiehl Also update last_connection.
 **
 */
DESCR__E_M3

#include <sys/types.h>
#include "fddefs.h"

/* Global variables. */
extern int                        fsa_fd;
extern struct filetransfer_status *fsa;
extern struct job                 db;


/*############################# update_tfc() ############################*/
void
update_tfc(int    file_counter,
           off_t  file_size,
           off_t  *p_file_size_buffer,
           int    files_to_do,
           int    current_file_pos,
           time_t now)
{
#ifdef LOCK_DEBUG
   lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
   lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC);
#endif

   /* Total file counter. */
   fsa->total_file_counter -= file_counter;
#ifdef _VERIFY_FSA
   if (fsa->total_file_counter < 0)
   {
      int total_file_counter = files_to_do - (current_file_pos + 1);

      if (total_file_counter < 0)
      {
         total_file_counter = 0;
      }
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Total file counter for host %s less then zero. Correcting to %d. [%s] #%x",
                 fsa->host_dsp_name, total_file_counter,
                 db.msg_name, db.id.job);
      fsa->total_file_counter = total_file_counter;
   }
#endif

   /* Total file size. */
   fsa->total_file_size -= file_size;
#ifdef _VERIFY_FSA
   if (fsa->total_file_size < 0)
   {
      int   k;
      off_t *tmp_ptr = p_file_size_buffer;

      tmp_ptr++;
      fsa->total_file_size = 0;
      for (k = (current_file_pos + 1); k < files_to_do; k++)
      {
         fsa->total_file_size += *tmp_ptr;
      }
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
# if SIZEOF_OFF_T == 4
                 "Total file size for host %s overflowed. Correcting to %ld. [%s] #%x",
# else
                 "Total file size for host %s overflowed. Correcting to %lld. [%s] #%x",
# endif
                 fsa->host_dsp_name, (pri_off_t)fsa->total_file_size,
                 db.msg_name, db.id.job);
   }
   else if ((fsa->total_file_counter == 0) &&
            (fsa->total_file_size > 0))
        {
           system_log(DEBUG_SIGN, __FILE__, __LINE__,
                      "fc for host %s is zero but fs is not zero. Correcting. [%s] #%x",
                      fsa->host_dsp_name, db.msg_name, db.id.job);
           fsa->total_file_size = 0;
        }
#endif

   /* File counter done. */
   fsa->file_counter_done += file_counter;

   /* Number of bytes send. */
   fsa->bytes_send += file_size;

   /* Last activity. */
   fsa->last_connection = now;
#ifdef LOCK_DEBUG
   unlock_region(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
   unlock_region(fsa_fd, db.lock_offset + LOCK_TFC);
#endif

   return;
}
