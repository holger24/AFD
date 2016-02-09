/*
 *  readn.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2013 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   readn - reads a definite number of bytes from a file descriptor
 **
 ** SYNOPSIS
 **   ssize_t readn(int fd, void *buf, int nbyte, long transfer_timeout)
 **
 ** DESCRIPTION
 **   The function readn() tries to read nbyte from the file descriptor
 **   fd into the buffer buf.
 **
 ** RETURN VALUES
 **   Returns 0 if all bytes have been read. If it returns a value
 **   larger then 0, it received a remote hangup. On a read() error
 **   -1 is returned and -3 is returned on a select() error. If
 **   the timeout set in the global variable connect_timeout is reached
 **   -2 is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.10.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <sys/time.h>                 /* struct timeval                  */
#include <sys/types.h>
#include <unistd.h>                   /* read()                          */
#include <errno.h>


/*############################## readn() ################################*/
ssize_t
readn(int fd, void *buf, int nbyte, long transfer_timeout)
{
   int            status,
                  nleft,
                  nread;
   char           *ptr;
   fd_set         rset;
   struct timeval timeout;

   ptr = buf;
   nleft = nbyte;
   FD_ZERO(&rset);
   while (nleft > 0)
   {
      FD_SET(fd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = transfer_timeout;

      status = select(fd + 1, &rset, NULL, NULL, &timeout);

      if (FD_ISSET(fd, &rset))
      {
         if ((nread = read(fd, ptr, nleft)) < 0)
         {
            return(nread); /* read() error. */
         }
         else if (nread == 0)
              {
                 break; /* EOF or remote hangup. */
              }
      }
      else if (status == 0)
           {
              return(-2); /* Timeout arrived. */
           }
           else
           {
              return(-3); /* select() error. */
           }
      nleft -= nread;
      ptr += nread;
   }

   return(nbyte - nleft);
}
