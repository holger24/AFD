/*
 *  open_fifo_rw.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2006 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   open_fifo_rw.c - open a fifo without blocking
 **
 ** SYNOPSIS
 **   int open_fifo_rw(char *pathname, int *readfd, int *writefd)
 **
 ** DESCRIPTION
 **   open_fifo_rw() attempts to open a fifo without blocking. This
 **   is done by first opening the fifo read only and non-blocking then
 **   open it for writting, after this the read filedescriptor is set
 **   in blocking mode again.
 **
 ** RETURN VALUES
 **   On success, open_fifo_rw() returns SUCCESS and the read and write
 **   file descriptor in readfd and writefd respectivly. On error, -1
 **   will be returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   26.08.2006 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif


/*########################### open_fifo_rw() ############################*/
int
open_fifo_rw(char *pathname, int *readfd, int *writefd)
{
   int ret;

   if ((*readfd = coe_open(pathname, O_RDONLY | O_NONBLOCK)) == -1)
   {
      *writefd = -1;
      *readfd = -1;
      ret = -1;
   }
   else
   {
      if ((*writefd = coe_open(pathname, O_WRONLY)) == -1)
      {
         (void)close(*readfd);
         *readfd = -1;
         ret = -1;
      }
      else
      {
         int val;

         if ((val = fcntl(*readfd, F_GETFD, 0)) == -1)
         {
            (void)close(*readfd);
            (void)close(*writefd);
            *writefd = -1;
            *readfd = -1;
            ret = -1;
         }
         else
         {
            val &= ~O_NONBLOCK;
            if (fcntl(*readfd, F_SETFD, val) == -1)
            {
               (void)close(*readfd);
               (void)close(*writefd);
               *writefd = -1;
               *readfd = -1;
               ret = -1;
            }
            else
            {
               ret = SUCCESS;
            }
         }
      }
   }

   return(ret);
}
