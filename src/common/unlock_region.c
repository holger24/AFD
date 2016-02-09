/*
 *  unlock_region.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2009 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   unlock_region - locks a file
 **
 ** SYNOPSIS
 **   int unlock_region(const int fd, const off_t offset)
 **
 ** DESCRIPTION
 **   This function unlocks the region specified by offset plus
 **   one byte in the file with file descriptor fd.
 **
 ** RETURN VALUES
 **   None. The function exits with UNLOCK_REGION_ERROR if fcntl()
 **   call fails.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   21.11.1996 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>                     /* strerror()                    */
#include <stdlib.h>                     /* exit()                        */
#include <sys/types.h>
#include <unistd.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "fddefs.h"


/*########################## unlock_region() ############################*/
void
#ifdef LOCK_DEBUG
unlock_region(int fd, off_t offset, char *file, int line)
#else
unlock_region(const int fd, const off_t offset)
#endif
{
   struct flock ulock;

   ulock.l_type   = F_UNLCK;
   ulock.l_whence = SEEK_SET;
   ulock.l_start  = offset;
   ulock.l_len    = 1;
#ifdef LOCK_DEBUG
   system_log(DEBUG_SIGN, NULL, 0,
# if SIZEOF_OFF_T == 4
              "unlock_region(): fd=%d start=%ld length=1 file=%s line=%d",
# else
              "unlock_region(): fd=%d start=%lld length=1 file=%s line=%d",
# endif
              fd, (pri_off_t)offset, file, line);                                 
#endif

   if (fcntl(fd, F_SETLK, &ulock) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("fcntl() error : %s"), strerror(errno));
      exit(UNLOCK_REGION_ERROR);
   }

   return;
}
