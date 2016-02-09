/*
 *  send_cmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   send_cmd - sends one character to a fifo
 **
 ** SYNOPSIS
 **   int send_cmd(char cmd, int fd)
 **        cmd - the character to be send
 **        fd  - file descriptor of the fifo
 **
 ** DESCRIPTION
 **   The function send_cmd() sends a command 'cmd' (one character)
 **   via the fifo 'fd' to another process.
 **
 ** RETURN VALUES
 **   Returns SUCCESS when successful else it will return the negative
 **   errno value.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.12.1995 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <sys/types.h>
#include <unistd.h>                      /* write()                      */
#include <errno.h>


/*############################ send_cmd() ###############################*/
int
send_cmd(char cmd, int fd)
{
   if (write(fd, &cmd, 1) != 1)
   {
      return(-errno);
   }
   return(SUCCESS);
} 
