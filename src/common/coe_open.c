/*
 *  coe_open.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   coe_open - close-on-exec open()
 **
 ** SYNOPSIS
 **   int coe_open(char *pathname, int flags)
 **
 ** DESCRIPTION
 **   coe_open() attempts to open a file and return a file descriptor.
 **   The FD_CLOEXEC flag is set so that the file descriptor is closed
 **   when calling one of the exec functions.
 **
 ** RETURN VALUES
 **   On success, coe_open() returns the file descriptor.
 **   On error, -1 will be returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   19.03.1999 H.Kiehl Created
 **   10.10.2008 H.Kiehl Added support for O_CLOEXEC.
 **
 */
DESCR__E_M3

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif


/*############################# coe_open() ##############################*/
int
coe_open(char *pathname, int flags, ...)
{
#ifdef O_CLOEXEC
   if (flags & O_CREAT)
   {
      int     mode;
      va_list arg;

      va_start(arg, flags);
      mode = va_arg(arg, int);
      va_end(arg);

      return(open(pathname, (flags | O_CLOEXEC), mode));
   }
   else
   {
      return(open(pathname, (flags | O_CLOEXEC)));
   }
#else
   int fd,
       val;

   if (flags & O_CREAT)
   {
      int     mode;
      va_list arg;

      va_start(arg, flags);
      mode = va_arg(arg, int);
      va_end(arg);

      if ((fd = open(pathname, flags, mode)) == -1)
      {
         return(-1);
      }
   }
   else
   {
      if ((fd = open(pathname, flags)) == -1)
      {
         return(-1);
      }
   }

   if ((val = fcntl(fd, F_GETFD, 0)) == -1)
   {
      (void)close(fd);
      return(-1);
   }
   val |= FD_CLOEXEC;
   if (fcntl(fd, F_SETFD, val) == -1)
   {
      (void)close(fd);
      return(-1);
   }

   return(fd);
#endif
}
