/*
 *  writen.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008, 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   writen - writes a definite number of bytes to a file descriptor
 **
 ** SYNOPSIS
 **   ssize_t writen(int        fd,
 **                  const void *buf,
 **                  size_t     nbyte,
 **                  ssize_t    block_size)
 **
 ** DESCRIPTION
 **   The function writen() tries to write nbyte from the buffer buf to
 **   file descriptor fd. Unfortunately this function is also necessary
 **   for some network filesytems (such as NEC's NFS/GFS).
 **
 **   NOTE: This function was taken (and modified) from the book
 **         Advanced Programming in the UNIX Environment by
 **         W. Richard Stevens. (p408)
 **
 ** RETURN VALUES
 **   Returns the number of bytes written or -1 on error.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.12.2008 H.Kiehl Created
 **   27.02.2009 H.Kiehl Added block size option.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <unistd.h>                   /* write()                         */

#define DEFAULT_WRITEN_BLOCK_SIZE 4096


/*############################## writen() ###############################*/
ssize_t
writen(int fd, const void *buf, size_t nbyte, ssize_t block_size)
{
   ssize_t    nleft,
              nwritten,
              write_size;
   const char *ptr;

   if (block_size < 1)
   {
      block_size = DEFAULT_WRITEN_BLOCK_SIZE;
   }

   ptr = buf;
   nleft = nbyte;
   while (nleft > 0)
   {
      if (nleft > block_size)
      {
         write_size = block_size;
      }
      else
      {
         write_size = nleft;
      }
      if ((nwritten = write(fd, ptr, write_size)) <= 0)
      {
         return(nwritten); /* Error! */
      }
      nleft -= nwritten;
      ptr += nwritten;
   }

   return(nbyte);
}
