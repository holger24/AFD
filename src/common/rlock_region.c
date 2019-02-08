/*
 *  rlock_region.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2019 Deutscher Wetterdienst (DWD),
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
 **   rlock_region - sets a read lock for a specific region in a file
 **
 ** SYNOPSIS
 **   int rlock_region(const int fd, const off_t offset)
 **
 ** DESCRIPTION
 **   This function sets a read lock to the region specified by offset
 **   plus one byte in the file with file descriptor fd. If the region
 **   is already locked by another process it does NOT care since
 **   it is a read lock.
 **
 ** RETURN VALUES
 **   Either LOCK_IS_SET when the region is locked or LOCK_IS_NOT_SET
 **   when it has succesfully locked the region. The function exits
 **   with LOCK_REGION_ERROR if fcntl() call fails.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.09.1997 H.Kiehl Created
 **   08.02.2019 H.Kiehl Modified function with return value, to show
 **                      that lock is already set or not.
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


/*########################### rlock_region() ############################*/
int
#ifdef LOCK_DEBUG
rlock_region(int fd, off_t offset, char *file, int line)
#else
rlock_region(const int fd, const off_t offset)
#endif
{
   struct flock rlock;

   rlock.l_type   = F_RDLCK;
   rlock.l_whence = SEEK_SET;
   rlock.l_start  = offset;
   rlock.l_len    = 1;
#ifdef LOCK_DEBUG
   system_log(DEBUG_SIGN, NULL, 0,
# if SIZEOF_OFF_T == 4
              "lock_region_w(): fd=%d start=%ld length=1 file=%s line=%d",
# else
              "lock_region_w(): fd=%d start=%lld length=1 file=%s line=%d",
# endif
              fd, (pri_off_t)offset, file, line);                                 
#endif

   if (fcntl(fd, F_SETLKW, &rlock) == -1)
   {
      if ((errno == EACCES) || (errno == EAGAIN) || (errno == EBUSY))
      {
         /* The file is already locked! */
         return(LOCK_IS_SET);
      }
      else
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("fcntl() error : %s"), strerror(errno));
         exit(LOCK_REGION_ERROR);
      }
   }

   return(LOCK_IS_NOT_SET);
}
