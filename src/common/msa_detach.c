/*
 *  msa_detach.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   msa_detach - detaches from the MSA
 **
 ** SYNOPSIS
 **   int msa_detach(void)
 **
 ** DESCRIPTION
 **   Detaches from the memory mapped area of the MSA (Monitor Status
 **   Area).
 **
 ** RETURN VALUES
 **   SUCCESS when detaching from the MSA successful. Otherwise
 **   INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.08.1998 H.Kiehl Created
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
#include "mondefs.h"

/* Global variables. */
extern int                    msa_fd,
                              msa_id,
                              no_of_afds;
#ifdef HAVE_MMAP
extern off_t                  msa_size;
#endif
extern char                   *p_work_dir;
extern struct mon_status_area *msa;


/*############################ msa_detach() #############################*/
int
msa_detach(void)
{
   if (msa_fd > 0)
   {
      (void)close(msa_fd);
      msa_fd = -1;
   }

   /* Make sure this is not the case when the */
   /* no_of_afds is stale.                    */
   if (no_of_afds > 0)
   {
      /* Detach from MSA. */
#ifdef HAVE_MMAP
      if (msync(((char *)msa - AFD_WORD_OFFSET), msa_size, MS_ASYNC) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to msync() MSA : %s"), strerror(errno));
         return(INCORRECT);
      }
      if (munmap(((char *)msa - AFD_WORD_OFFSET), msa_size) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to munmap() MSA : %s"), strerror(errno));
         return(INCORRECT);
      }
#else
      if (msync_emu((void *)((char *)msa - AFD_WORD_OFFSET)) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to msync_emu() MSA : %s"), strerror(errno));
         return(INCORRECT);
      }
      if (munmap_emu((void *)((char *)msa - AFD_WORD_OFFSET)) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to munmap_emu() MSA : %s"), strerror(errno));
         return(INCORRECT);
      }
#endif
      msa = NULL;
   }

   return(SUCCESS);
}
