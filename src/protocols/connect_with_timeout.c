/*
 *  connect_with_timeout.c - Part of AFD, an automatic file distribution
 *                           program.
 *  Copyright (c) 2013 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   connect_with_timeout - times out connect() system call
 **
 ** SYNOPSIS
 **   int connect_with_timeout(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
 **
 ** DESCRIPTION
 **   This function calls connect() with a timeout. The timeout is set by
 **   the global variabel transfer_timeout. As Richard Stevens noticed one
 **   can only shorten the connect time, there's normally no way to
 **   lengthen it. The code was taken from the UNIX Socket FAQ:
 **   http://developerweb.net/viewtopic.php?id=3196
 **
 ** RETURN VALUES
 **   When timeout is reached, the global variable timeout_flag is
 **   set to YES and errno is set to 0. The function returns
 **   PERMANENT_INCORRECT when we fail to set or unset the O_NONBLOCK
 **   flag. When connect() fails it will just return INCORRECT and
 **   errno will be set. Errno will be 0 when getsockopt() fails.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.02.2013 H.Kiehl Created
 */
DESCR__E_M3


#include <stdio.h>
#include <string.h>       /* memcpy(), strerror()                        */
#include <sys/types.h>    /* fd_set                                      */
#include <sys/time.h>     /* struct timeval                              */
#include <sys/stat.h>     /* S_ISUID, S_ISGID, etc                       */
#include <sys/socket.h>
#include <unistd.h>       /* write()                                     */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "fddefs.h"
#include "commondefs.h"


/* External global variables. */
extern int  timeout_flag;
extern long transfer_timeout;


/*####################### connect_with_timeout() ########################*/
int
connect_with_timeout(int                   sockfd,
                     const struct sockaddr *addr,
                     socklen_t             addrlen)
{
   int flags,
       ret;

   if ((flags = fcntl(sockfd, F_GETFL, 0)) == -1)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "connect_with_timeout", NULL,
                "Failed to get flag via fcntl() : %s", strerror(errno));
      ret = PERMANENT_INCORRECT;
   }
   else
   {
      flags |= O_NONBLOCK;
      if (fcntl(sockfd, F_SETFL, flags) == -1)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "connect_with_timeout", NULL,
                   "Failed to set O_NONBLOCK flag via fcntl() : %s",
                   strerror(errno));
         ret = PERMANENT_INCORRECT;
      }
      else
      {
         ret = connect(sockfd, addr, addrlen);
         if (ret < 0)
         {
            if (errno == EINPROGRESS)
            {
               fd_set         wset;
               struct timeval tv;

               FD_ZERO(&wset);
               do
               {
                  FD_SET(sockfd, &wset);
                  tv.tv_sec = transfer_timeout;
                  tv.tv_usec = 0;
                  ret = select(sockfd + 1, NULL, &wset, NULL, &tv);
                  if ((ret < 0) && (errno != EINTR))
                  {
                     /* Error in connect() */
                     return(INCORRECT);
                  }
                  else if (ret > 0)
                       {
                          int       so_error;
                          socklen_t length = sizeof(so_error);

                          /* socket seems to be ready */
                          if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR,
                                         &so_error, &length) < 0)
                          {
                             /* Error in getsockopt() */
                             trans_log(ERROR_SIGN, __FILE__, __LINE__, "connect_with_timeout", NULL,
                                       "getsockopt() error : %s",
                                       strerror(errno));
                             errno = 0;
                             return(INCORRECT);
                          }
                          if (so_error)
                          {
                             /* Error in delayed connect() */
                             errno = so_error;
                             return(INCORRECT);
                          }

                          /* Success, socket is ready with data. */
                          break;
                       }
                       else
                       {
                          /* Timeout in select() */
                          timeout_flag = YES;
                          errno = 0;
                          return(INCORRECT);
                       }
               } while (1);
            }
            else
            {
               /* Error in connect() */
               return(INCORRECT);
            }
         }
         if ((flags = fcntl(sockfd, F_GETFL, 0)) == -1)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "connect_with_timeout", NULL,
                      "Failed to get flag via fcntl() : %s", strerror(errno));
            ret = PERMANENT_INCORRECT;
         }
         else
         {
            flags &= ~O_NONBLOCK;
            if (fcntl(sockfd, F_SETFL, flags) == -1)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "connect_with_timeout", NULL,
                         "Failed to unset O_NONBLOCK flag via fcntl() : %s",
                         strerror(errno));
               ret = PERMANENT_INCORRECT;
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
