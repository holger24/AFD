/*
 *  lock_region_w.c - Part of AFD, an automatic file distribution program.
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
 **   lock_region_w - locks a specific region of a file
 **
 ** SYNOPSIS
 **   void lock_region_w(int fd, off_t offset)
 **
 ** DESCRIPTION
 **   This function locks the region specified by offset plus
 **   one byte in the file with file descriptor fd. If the region
 **   is already locked by another process it waits until it
 **   frees the region.
 **
 ** RETURN VALUES
 **   None. The function exits with LOCK_REGION_ERROR if fcntl()
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


/*########################## lock_region_w() ############################*/
void
#ifdef LOCK_DEBUG
lock_region_w(int fd, off_t offset, char *file, int line)
#else
lock_region_w(int fd, off_t offset)
#endif
{
   int          count = 0,
                ret;
   struct flock wlock;

   wlock.l_type   = F_WRLCK;
   wlock.l_whence = SEEK_SET;
   wlock.l_start  = offset;
   wlock.l_len    = (off_t)1;
#ifdef LOCK_DEBUG
   system_log(DEBUG_SIGN, NULL, 0,
# if SIZEOF_OFF_T == 4
              "lock_region_w(): fd=%d start=%ld length=1 file=%s line=%d",
# else
              "lock_region_w(): fd=%d start=%lld length=1 file=%s line=%d",
# endif
              fd, (pri_off_t)offset, file, line);
#endif

   while (((ret = fcntl(fd, F_SETLKW, &wlock)) == -1) && (errno == EAGAIN) &&
          (count < 20))
   {
      my_usleep(50000L);
      count++;
   }
   if (ret == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("fcntl() error : %s"), strerror(errno));
      exit(LOCK_REGION_ERROR);
   }

   return;
}
