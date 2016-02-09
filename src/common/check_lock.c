/*
 *  check_lock.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2012 Deutscher Wetterdienst (DWD),
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
 **   check_lock - waits for a lock to be released
 **
 ** SYNOPSIS
 **   int check_lock(char *file, char block_flag)
 **
 ** DESCRIPTION
 **   This function checks if 'file' is locked. If it is locked
 **   and the block_flag is set to YES, it waits until the lock is
 **   released before it returns. Otherwise it will return immediately.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it either fails to open() or lock
 **   a region. When block_flag is YES zero is returned otherwise
 **   LOCK_IS_SET or LOCK_IS_NOT_SET is returned when the region
 **   is locked or not locked respectively.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   06.03.1996 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>


/*############################ check_lock() #############################*/
int
check_lock(char *file, char block_flag)
{
   int fd;

   if ((fd = open(file, O_WRONLY)) == -1)
   {
      if (errno != ENOENT)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not open() `%s' : %s"), file, strerror(errno));
      }
      return(INCORRECT);
   }

   if (block_flag == YES)
   {
      struct flock wlock = {F_WRLCK, SEEK_SET, 0, 1},
                   ulock = {F_UNLCK, SEEK_SET, 0, 1};

      if (fcntl(fd, F_SETLKW, &wlock) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not set write lock : %s"), strerror(errno));
         (void)close(fd);
         return(INCORRECT);
      }

      /*
       * Unlocking is actually not necessary. Since when we
       * close the file descriptor it is automatically unlocked.
       * But you never know ...
       */
      if (fcntl(fd, F_SETLKW, &ulock) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not unlock `%s' : %s"), file, strerror(errno));
         (void)close(fd);
         return(INCORRECT);
      }
   }
   else
   {
      struct flock tlock = {F_WRLCK, SEEK_SET, 0, 1};

      if (fcntl(fd, F_GETLK, &tlock) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not set write lock : %s"), strerror(errno));
         (void)close(fd);
         return(INCORRECT);
      }

      (void)close(fd);
      if (tlock.l_type == F_UNLCK)
      {
         return(LOCK_IS_NOT_SET);
      }
      else
      {
         return(LOCK_IS_SET);
      }
   }

   if (close(fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 _("close() error : %s"), strerror(errno));
   }

   return(0);
}
