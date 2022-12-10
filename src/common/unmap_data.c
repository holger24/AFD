/*
 *  unmap_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   unmap_data - unmaps from a memory mapped region
 **
 ** SYNOPSIS
 **   void unmap_data(int fd, void **area)
 **
 ** DESCRIPTION
 **   Function unmap_data() unmaps from the given area and then closes
 **   the given filedescriptor fd. Before it unmaps it does a sync,
 **   so no data is lost.
 **
 ** RETURN VALUES
 **   None.
 **
 ** SEE ALSO
 **   attach_buf(), mmap_resize()
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.12.2003 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                /* strerror()                         */
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>                /* Definition of AT_* constants       */
#endif
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>             /* msync(), munmap()                  */
#endif
#include <unistd.h>                /* fstat()                            */
#include <errno.h>


/*############################ unmap_data() #############################*/
void
unmap_data(int fd, void **area)
{
   if (*area != NULL)
   {
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
      struct statx stat_buf;
# else
      struct stat stat_buf;
# endif

# ifdef HAVE_STATX
      if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
# else
      if (fstat(fd, &stat_buf) == -1)
# endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
# ifdef HAVE_STATX
                    _("statx() error : %s"),
# else
                    _("fstat() error : %s"),
# endif
                    strerror(errno));
      }
      else
      {
         char *ptr = (char *)*area - AFD_WORD_OFFSET;

# ifdef HAVE_STATX
         if (msync(ptr, stat_buf.stx_size, MS_SYNC) == -1)
# else
         if (msync(ptr, stat_buf.st_size, MS_SYNC) == -1)
# endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("msync() error : %s"), strerror(errno));
         }
# ifdef HAVE_STATX
         if (munmap(ptr, stat_buf.stx_size) == -1)
# else
         if (munmap(ptr, stat_buf.st_size) == -1)
# endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("munmap() error : %s"), strerror(errno));
         }
         else
         {
            *area = NULL;
         }
      }
#else
      char *ptr = (char *)*area - AFD_WORD_OFFSET;

      if (msync_emu(ptr) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("msync_emu() error : %s"), strerror(errno));
      }
      if (munmap_emu(ptr) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("munmap_emu() error : %s"), strerror(errno));
      }
      else
      {
         *area = NULL;
      }
#endif /* HAVE_MMAP */
   }
   if (close(fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 _("close() error : %s"), strerror(errno));
   }

   return;
}
