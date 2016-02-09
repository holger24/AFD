/*
 *  handle_wmo_request.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   handle_wmo_request - handles a WMO request to send data
 **
 ** SYNOPSIS
 **   void handle_wmo_request(int    sd,
 **                           int    pos,
 **                           int    acknowledge,
 **                           int    chs,
 **                           time_t disconnect,
 **                           char   *req)
 **
 ** DESCRIPTION
 **   Handles all request from remote user in a loop. If user is inactive
 **   for AFDD_CMD_TIMEOUT seconds, the connection will be closed.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   20.09.2005 H.Kiehl Created
 */
DESCR__E_M3

#include <stdio.h>            /* fprintf()                               */
#include <string.h>           /* memset()                                */
#include <stdlib.h>           /* atoi()                                  */
#include <time.h>             /* time()                                  */
#include <ctype.h>            /* toupper()                               */
#include <signal.h>           /* signal()                                */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>           /* close()                                 */
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <netdb.h>
#include <errno.h>
#include "commondefs.h"

/* External global variables. */
extern int                        fsa_pos;
extern struct proclist            pl[];
extern struct filetransfer_status *fsa;


/*######################### handle_wmo_request() ########################*/
void
handle_wmo_request(int    sd,
                   int    pos,
                   int    acknowledge,
                   int    chs,
                   time_t disconnect,
                   char   *req)
{
   int            status;
   time_t         start_time;
   fd_set         rset;
   struct timeval timeout;

   FD_ZERO(&rset);
   start_time = time(NULL);
   do
   {
      check_fsa_pos();
      FD_SET(sd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = fsa[fsa_pos].transfer_timeout;

      /* Wait for message x seconds and then continue. */
      status = select(sd + 1, &rset, NULL, NULL, &timeout);

      if (status == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "select() error : %s", strerror(errno));
         return;
      }
      else if (status == 0)
           {
           }
           else
           {
              if (FD_ISSET(sd, &rset))
              {
              }
           }
   } while ((disconnect == -1) || ((start_time + disconnect) < time(NULL)));

   trans_log(INFO_SIGN, NULL, 0, pl[pos].job_pos,
             "WMOD: Timeout after waiting %d seconds.", );

   return;
}
