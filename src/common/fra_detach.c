/*
 *  fra_detach.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   fra_detach - detaches from the FRA
 **
 ** SYNOPSIS
 **   int fra_detach(void)
 **
 ** DESCRIPTION
 **   Detaches from the memory mapped area of the FRA (File Retrieve
 **   Status Area).
 **
 ** RETURN VALUES
 **   SUCCESS when detaching from the FRA successful. Otherwise
 **   INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.08.2000 H.Kiehl Created
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
extern int                        fra_fd,
                                  fra_id,
                                  no_of_dirs;
#ifdef HAVE_MMAP
extern off_t                      fra_size;
#endif
extern char                       *p_work_dir;
extern struct fileretrieve_status *fra;


/*############################ fra_detach() #############################*/
int
fra_detach(void)
{
   if (fra_fd > 0)
   {
      if (close(fra_fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("close() error : %s"), strerror(errno));
      }
      fra_fd = -1;
   }

   /* Make sure this is not the case when the */
   /* no_of_dirs is stale.                    */
   if (no_of_dirs > 0)
   {
      /* Detach from FRA. */
#ifdef HAVE_MMAP
      if (munmap(((char *)fra - AFD_WORD_OFFSET), fra_size) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to munmap() FRA : %s"), strerror(errno));
         return(INCORRECT);
      }
#else
      if (munmap_emu((void *)((char *)fra - AFD_WORD_OFFSET)) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to munmap_emu() FRA : %s"), strerror(errno));
         return(INCORRECT);
      }
#endif
      fra = NULL;
   }

   return(SUCCESS);
}
