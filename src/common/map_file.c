/*
 *  map_file.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   map_file - uses mmap() to map to a file
 **
 ** SYNOPSIS
 **   void *map_file(char         *file,
 **                  int          *fd,
 **                  off_t        *size,
 **                  struct statx *p_stat_buf,
 **                  int          flags,
 **                  ...)
 **
 ** DESCRIPTION
 **   The function map_file() attaches to the file 'file'. If
 **   the file does not exist, it is created.
 **
 ** RETURN VALUES
 **   On success, map_file() returns a pointer to the mapped area
 **   and if 'size' is not NULL, the size of the mapped area.
 **   On error, INCORRECT (-1) is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   06.06.1999 H.Kiehl Created
 **   02.03.2008 H.Kiehl On request return size of mapped area.
 **
 */
DESCR__E_M3

#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>                 /* open()                            */
#endif
#include <unistd.h>                 /* fstat()                           */
#include <sys/mman.h>               /* mmap()                            */
#include <errno.h>


/*############################# map_file() ##############################*/
void *
map_file(char         *file,
         int          *fd,
         off_t        *size,
#ifdef HAVE_STATX
         struct statx *p_stat_buf,
#else
         struct stat  *p_stat_buf,
#endif
         int          flags,
         ...)
{
   int          prot;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   if (flags & O_RDWR)
   {
      prot = PROT_READ | PROT_WRITE;
   }
   else if (flags & O_WRONLY)
        {
           prot = PROT_WRITE;
        }
        else
        {
           prot = PROT_READ;
        }

   if (flags & O_CREAT)
   {
      int     mode;
      va_list arg;

      va_start(arg, flags);
      mode = va_arg(arg, int);
      va_end(arg);

      if ((*fd = coe_open(file, flags, mode)) == -1)
      {
         return((caddr_t)-1);
      }
   }
   else
   {
      if ((*fd = coe_open(file, flags)) == -1)
      {
         return((caddr_t)-1);
      }
   }

   if (p_stat_buf == NULL)
   {
#ifdef HAVE_STATX
      if (statx(*fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(*fd, &stat_buf) == -1)
#endif
      {
         int tmp_errno = errno;

         (void)close(*fd);
         errno = tmp_errno;
         return((caddr_t)-1);
      }
      p_stat_buf = &stat_buf;
   }

#ifdef HAVE_STATX
   if (p_stat_buf->stx_size == 0)
#else
   if (p_stat_buf->st_size == 0)
#endif
   {
      (void)close(*fd);
      errno = EINVAL;
      return((caddr_t)-1);
   }
   if (size != NULL)
   {
#ifdef HAVE_STATX
      *size = p_stat_buf->stx_size;
#else
      *size = p_stat_buf->st_size;
#endif
   }

#ifdef HAVE_STATX
   return(mmap(NULL, p_stat_buf->stx_size, prot, MAP_SHARED, *fd, 0));
#else
   return(mmap(NULL, p_stat_buf->st_size, prot, MAP_SHARED, *fd, 0));
#endif
}
