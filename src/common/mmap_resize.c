/*
 *  mmap_resize.c - Part of AFD, an automatic file distribution program.
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
 **   mmap_resize - resizes a memory mapped area
 **
 ** SYNOPSIS
 **   void *mmap_resize(int fd, void *area, size_t new_size)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   On success, mmap_resize() returns a pointer to the mapped area.
 **   On error, MAP_FAILED (-1) is returned.
 **
 ** SEE ALSO
 **   attach_buf(), unmap_data()
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   19.01.1998 H.Kiehl Created
 **   22.04.2007 H.Kiehl Handle case when the mapped area already has
 **                      the given size.
 **
 */
DESCR__E_M3

#include <string.h>
#include <unistd.h>     /* lseek(), write(), ftruncate()                 */
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>     /* Definition of AT_* constants                  */
#endif
#include <sys/stat.h>
#include <sys/mman.h>   /* mmap(), msync(), munmap()                     */
#include <errno.h>


/*############################ mmap_resize() ############################*/
void *
mmap_resize(int fd, void *area, size_t new_size)
{
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat stat_buf;
#endif

#ifdef HAVE_STATX
   if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(fd, &stat_buf) == -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                 _("statx() error : %s"),
#else
                 _("fstat() error : %s"),
#endif
                 strerror(errno));
      return((void *)-1);
   }
#ifdef HAVE_STATX
   if (stat_buf.stx_size == new_size)
#else
   if (stat_buf.st_size == new_size)
#endif
   {
      return(area);
   }

   /* Always unmap from current mmapped area. */
#ifdef HAVE_STATX
   if (stat_buf.stx_size > 0)
#else
   if (stat_buf.st_size > 0)
#endif
   {
#ifdef HAVE_STATX
      if (msync(area, stat_buf.stx_size, MS_SYNC) == -1)
#else
      if (msync(area, stat_buf.st_size, MS_SYNC) == -1)
#endif
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("msync() error : %s"), strerror(errno));
         return((void *)-1);
      }
#ifdef HAVE_STATX
      if (munmap(area, stat_buf.stx_size) == -1)
#else
      if (munmap(area, stat_buf.st_size) == -1)
#endif
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("munmap() error : %s"), strerror(errno));
         return((void *)-1);
      }
   }

#ifdef HAVE_STATX
   if (new_size > stat_buf.stx_size)
#else
   if (new_size > stat_buf.st_size)
#endif
   {
      int  i,
           loops,
           rest,
           write_size;
      char buffer[4096];

#ifdef HAVE_STATX
      if (lseek(fd, stat_buf.stx_size, SEEK_SET) == -1)
#else
      if (lseek(fd, stat_buf.st_size, SEEK_SET) == -1)
#endif
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("lseek() error : %s"), strerror(errno));
         return((void *)-1);
      }
#ifdef HAVE_STATX
      write_size = new_size - stat_buf.stx_size;
#else
      write_size = new_size - stat_buf.st_size;
#endif
      (void)memset(buffer, 0, 4096);
      loops = write_size / 4096;
      rest = write_size % 4096;
      for (i = 0; i < loops; i++)
      {
         if (write(fd, buffer, 4096) != 4096)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       _("write() error : %s"), strerror(errno));
            return((void *)-1);
         }
      }
      if (rest > 0)
      {
         if (write(fd, buffer, rest) != rest)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       _("write() error : %s"), strerror(errno));
            return((void *)-1);
         }
      }
   }
#ifdef HAVE_STATX
   else if (new_size < stat_buf.stx_size)
#else
   else if (new_size < stat_buf.st_size)
#endif
        {
           if (ftruncate(fd, new_size) == -1)
           {
              system_log(FATAL_SIGN, __FILE__, __LINE__,
                         _("ftruncate() error : %s"), strerror(errno));
              return((void *)-1);
           }
        }

   return(mmap(NULL, new_size, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, 0));
}
