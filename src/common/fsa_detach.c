/*
 *  fsa_detach.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   fsa_detach - detaches from the FSA
 **
 ** SYNOPSIS
 **   int fsa_detach(int sync)
 **
 ** DESCRIPTION
 **   Detaches from the memory mapped area of the FSA (File Transfer
 **   Status Area).
 **
 ** RETURN VALUES
 **   SUCCESS when detaching from the FSA successful. Otherwise
 **   INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.05.1997 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                      /* strerror(), strlen()         */
#include <unistd.h>                      /* close()                      */
#ifdef HAVE_MMAP
# include <sys/mman.h>                   /* mmap(), munmap()             */
#endif
#include <errno.h>

/* Global variables. */
extern int                        fsa_fd,
                                  fsa_id,
                                  no_of_hosts;
#ifdef HAVE_MMAP
extern off_t                      fsa_size;
#endif
extern char                       *p_work_dir;
extern struct filetransfer_status *fsa;


/*############################ fsa_detach() #############################*/
int
fsa_detach(int sync)
{
   if (fsa_fd > 0)
   {
      if (close(fsa_fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("close() error : %s"), strerror(errno));
      }
      fsa_fd = -1;
   }

   /* Make sure this is not the case when the */
   /* no_of_hosts is stale.                   */
   if (no_of_hosts > 0)
   {
      /* Detach from FSA. */
#ifdef HAVE_MMAP
      if (sync == YES)
      {
         if (msync(((char *)fsa - AFD_WORD_OFFSET), fsa_size, MS_SYNC) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to msync() FSA : %s"), strerror(errno));
            return(INCORRECT);
         }
      }
      if (munmap(((char *)fsa - AFD_WORD_OFFSET), fsa_size) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to munmap() FSA : %s"), strerror(errno));
         return(INCORRECT);
      }
#else
      if (munmap_emu((void *)((char *)fsa - AFD_WORD_OFFSET)) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to munmap_emu() FSA : %s"), strerror(errno));
         return(INCORRECT);
      }
#endif
      fsa = NULL;
   }

   return(SUCCESS);
}
