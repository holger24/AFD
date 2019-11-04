/*
 *  jid_detach.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2019 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   jid_detach - detaches from the job ID data (JID)
 **
 ** SYNOPSIS
 **   int jid_detach(int sync)
 **
 ** DESCRIPTION
 **   Detaches from the memory mapped area of the JID (Job ID
 **   Data).
 **
 ** RETURN VALUES
 **   SUCCESS when detaching from the JID successful. Otherwise
 **   INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.11.2019 H.Kiehl Created
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
extern int                jid_fd;
#ifdef HAVE_MMAP
extern off_t              jid_size;
#endif
extern char               *p_work_dir;
extern struct job_id_data *jid;


/*############################ jid_detach() #############################*/
int
jid_detach(int sync)
{
   if (jid_fd > 0)
   {
      if (close(jid_fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("close() error : %s"), strerror(errno));
      }
      jid_fd = -1;
   }

   /* Detach from JID. */
#ifdef HAVE_MMAP
   if (sync == YES)
   {
      if (msync(((char *)jid - AFD_WORD_OFFSET), jid_size, MS_SYNC) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to msync() JID : %s"), strerror(errno));
         return(INCORRECT);
      }
   }
   if (munmap(((char *)jid - AFD_WORD_OFFSET), jid_size) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to munmap() JID : %s"), strerror(errno));
      return(INCORRECT);
   }
#else
   if (munmap_emu((void *)((char *)jid - AFD_WORD_OFFSET)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to munmap_emu() JID : %s"), strerror(errno));
      return(INCORRECT);
   }
#endif
   jid = NULL;

   return(SUCCESS);
}
