/*
 *  lock_file.c - Part of AFD, an automatic file distribution program.
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
 **   lock_file - locks a file
 **
 ** SYNOPSIS
 **   int lock_file(char *file, int block_flag)
 **
 ** DESCRIPTION
 **   If 'block_flag' is set to ON, lock_file() will wait for the
 **   lock on 'file' to be released if it is already locked by
 **   another process. The caller is responsible for closing the
 **   file descriptor returned by this function.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to lock file 'file',
 **   LOCKFILE_NOT_THERE if the lock file does not exist or
 **   LOCK_IS_SET if it is already locked by another process.
 **   Otherwise it returns the file-descriptor 'fd' of the
 **   file 'file'.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   06.03.1996 H.Kiehl Created
 **   04.01.1997 H.Kiehl When already locked, return LOCK_IS_SET.
 **   19.01.2005 H.Kiehl When lock file is not there, return
 **                      LOCKFILE_NOT_THERE.
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


/*############################ lock_file() ##############################*/
int
lock_file(char *file, int block_flag)
{
   int fd;

   if ((fd = coe_open(file, O_RDWR)) == -1)
   {
      if (errno == ENOENT)
      {
         fd = LOCKFILE_NOT_THERE;
      }
      else
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not open() `%s' : %s"), file, strerror(errno));
         fd = INCORRECT;
      }
   }
   else
   {
      struct flock wlock = {F_WRLCK, SEEK_SET, (off_t)0, (off_t)1};

      if (block_flag == ON)
      {
         if (fcntl(fd, F_SETLKW, &wlock) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Could not set read lock : %s"), strerror(errno));
            (void)close(fd);
            fd = INCORRECT;
         }
      }
      else
      {
         if (fcntl(fd, F_SETLK, &wlock) == -1)
         {
            if ((errno == EACCES) || (errno == EAGAIN) || (errno == EBUSY))
            {
               /* The file is already locked, so don't bother. */
               (void)close(fd);
               fd = LOCK_IS_SET;
            }
            else
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Could not set read lock : %s"), strerror(errno));
               (void)close(fd);
               fd = INCORRECT;
            }
         }
      }
   }

   return(fd);
}
