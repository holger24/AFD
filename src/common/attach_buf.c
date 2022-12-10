/*
 *  attach_buf.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   attach_buf - maps to a file
 **
 ** SYNOPSIS
 **   void *attach_buf(char   *file,
 **                    int    *fd,
 **                    size_t *new_size,
 **                    char   *prog_name,
 **                    mode_t mode,
 **                    int    wait_lock)
 **
 ** DESCRIPTION
 **   The function attach_buf() attaches to the file 'file'. If
 **   the file does not exist, it is created and a binary int
 **   zero is written to the first four bytes.
 **
 ** RETURN VALUES
 **   On success, attach_buf() returns a pointer to the mapped area.
 **   On error, MAP_FAILED (-1) is returned.
 **
 ** SEE ALSO
 **   unmap_data(), mmap_resize()
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   21.01.1998 H.Kiehl Created
 **   30.04.2003 H.Kiehl Added mode parameter.
 **   05.08.2004 H.Kiehl Added wait for lock.
 **
 */
DESCR__E_M3

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>                 /* open()                            */
#endif
#include <unistd.h>                 /* fstat(), lseek(), write()         */
#ifdef HAVE_MMAP
# include <sys/mman.h>              /* mmap()                            */
#endif
#include <errno.h>


/*############################ attach_buf() #############################*/
void *
attach_buf(char   *file,
           int    *fd,
           size_t *new_size,
           char   *prog_name,
           mode_t mode,
           int    wait_lock)
{
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   /*
    * This buffer has no open file descriptor, so we do not
    * need to unmap from the area. However this can be the
    * first time that the this process is started and no buffer
    * exists, then we need to create it.
    */
   if ((*fd = coe_open(file, O_RDWR | O_CREAT, mode)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to open() and create `%s' : %s"),
                 file, strerror(errno));
      return((caddr_t) -1);
   }
   if (prog_name != NULL)
   {
      /*
       * Lock the file. That way it is not possible for two same
       * processes to run at the same time.
       */
      if (wait_lock == YES)
      {
#ifdef LOCK_DEBUG
         lock_region_w(*fd, 0, __FILE__, __LINE__);
#else
         lock_region_w(*fd, 0);
#endif
      }
      else
      {
#ifdef LOCK_DEBUG
         if (lock_region(*fd, 0, __FILE__, __LINE__) == LOCK_IS_SET)
#else
         if (lock_region(*fd, 0) == LOCK_IS_SET)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Another `%s' is already running. Terminating."),
                       prog_name);
            return((caddr_t) -1);
         }
      }
   }

#ifdef HAVE_STATX
   if (statx(*fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(*fd, &stat_buf) == -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                 _("Failed to statx() `%s' : %s"),
#else
                 _("Failed to fstat() `%s' : %s"),
#endif
                 file, strerror(errno));
      return((caddr_t) -1);
   }

#ifdef HAVE_STATX
   if (stat_buf.stx_size < *new_size)
#else
   if (stat_buf.st_size < *new_size)
#endif
   {
      int  i,
           loops,
           rest,
           write_size;
      char buffer[4096];

#ifdef HAVE_STATX
      if (stat_buf.stx_size < sizeof(int))
#else
      if (stat_buf.st_size < sizeof(int))
#endif
      {
         int buf_size = 0;

         if (write(*fd, &buf_size, sizeof(int)) != sizeof(int))
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to write() to `%s' : %s"),
                       file, strerror(errno));
            return((caddr_t) -1);
         }
      }
#ifdef HAVE_STATX
      if (lseek(*fd, stat_buf.stx_size, SEEK_SET) == -1)
#else
      if (lseek(*fd, stat_buf.st_size, SEEK_SET) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to lseek() in `%s' : %s"),
                    file, strerror(errno));
         return((caddr_t) -1);
      }
#ifdef HAVE_STATX
      write_size = *new_size - stat_buf.stx_size;
#else
      write_size = *new_size - stat_buf.st_size;
#endif
      (void)memset(buffer, 0, 4096);
      loops = write_size / 4096;
      rest = write_size % 4096;
      for (i = 0; i < loops; i++)
      {
         if (write(*fd, buffer, 4096) != 4096)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to write() to `%s' : %s"),
                       file, strerror(errno));
            return((caddr_t) -1);
         }
      }
      if (rest > 0)
      {
         if (write(*fd, buffer, rest) != rest)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to write() to `%s' : %s"),
                       file, strerror(errno));
            return((caddr_t) -1);
         }
      }
   }
   else
   {
#ifdef HAVE_STATX
      *new_size = stat_buf.stx_size;
#else
      *new_size = stat_buf.st_size;
#endif
   }

   return(mmap(NULL, *new_size, (PROT_READ | PROT_WRITE), MAP_SHARED, *fd, 0));
}
