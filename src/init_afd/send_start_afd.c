/*
 *  shutdown_afd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2020 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   send_start_afd - sends the start command to init_afd via fifo
 **
 ** SYNOPSIS
 **   int send_start_afd(char *user, long response_time)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   Returns YES if startup was succesfull. Otherwise a value greater
 **   then zero is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.07.2020 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>            /* fprintf()                               */
#include <string.h>
#include <stdlib.h>           /* exit()                                  */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>         /* struct timeval                          */
#include <fcntl.h>            /* O_RDWR, O_WRONLY, etc                   */
#include <unistd.h>           /* select()                                */
#include <errno.h>
#include "version.h"

/* External global variables */
extern int  pause_dir_check;
extern char afd_cmd_fifo[],
            *p_work_dir;


/*########################## send_start_afd() ###########################*/
int
send_start_afd(char *user, long response_time)
{
   int            afd_cmd_fd,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                  afd_cmd_readfd,
                  afd_resp_writefd,
#endif
                  afd_resp_fd,
                  status;
   fd_set         rset;
   char           buffer[DEFAULT_BUFFER_SIZE],
                  afd_resp_fifo[MAX_PATH_LENGTH];
   struct timeval timeout;

   /* Initialise variables. */
   (void)strcpy(afd_resp_fifo, p_work_dir);
   (void)strcat(afd_resp_fifo, FIFO_DIR);
   (void)strcat(afd_resp_fifo, AFD_RESP_FIFO);

#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(afd_cmd_fifo, &afd_cmd_readfd, &afd_cmd_fd) == -1)
#else
   if ((afd_cmd_fd = open(afd_cmd_fifo, O_RDWR)) == -1)
#endif
   {
      if (errno == ENOENT)
      {
         /*
          * Lets assume when there is no afd_cmd_fifo there is no AFD
          * active in this location.
          */
         return(2);
      }
      else
      {
         (void)fprintf(stderr,
                       _("ERROR   : Could not open fifo `%s' : %s (%s %d)\n"),
                       afd_cmd_fifo, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(afd_resp_fifo, &afd_resp_fd, &afd_resp_writefd) == -1)
#else
   if ((afd_resp_fd = open(afd_resp_fifo, O_RDWR)) == -1)
#endif
   {
      (void)fprintf(stderr,
                    _("ERROR   : Could not open fifo `%s' : %s (%s %d)\n"),
                    afd_resp_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Tell user what we are doing. */
   system_log(CONFIG_SIGN, NULL, 0, _("Sending start AFD (%s) ..."), user);

   /* Send START_AFD command. */
   if (send_cmd((pause_dir_check & NO) ? START_AFD : START_AFD_NO_DIR_SCAN,
                afd_cmd_fd) < 0)
   {
      (void)fprintf(stderr,
                    _("ERROR   : Failed to send start command to %s : %s (%s %d)\n"),
                    AFD, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /*
    * Now lets wait for a reply from 'init_afd', but not more then
    * 'response_time' seconds.
    */

   /* Initialise descriptor set and timeout. */
   FD_ZERO(&rset);
   FD_SET(afd_resp_fd, &rset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = response_time;

   /* Wait for message x seconds and then continue. */
   status = select(afd_resp_fd + 1, &rset, NULL, NULL, &timeout);

   /* Did we get a timeout? */
   if (status == 0)
   {
      int val;

      if (response_time > 1L)
      {
         (void)fprintf(stderr, _("\nAFD is NOT responding!\n"));
      }

      /*
       * Since the AFD does not answer let's remove the command
       * from the FIFO.
       */
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if ((val = fcntl(afd_cmd_readfd, F_GETFL, 0)) == -1)
#else
      if ((val = fcntl(afd_cmd_fd, F_GETFL, 0)) == -1)
#endif
      {
         (void)fprintf(stderr,
                       _("ERROR   : Failed to get file status flag : %s (%s %d)\n"),
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      val |= O_NONBLOCK;
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (fcntl(afd_cmd_readfd, F_SETFL, val) == -1)
#else
      if (fcntl(afd_cmd_fd, F_SETFL, val) == -1)
#endif
      {
         (void)fprintf(stderr,
                       _("ERROR   : Failed to set file status flag : %s (%s %d)\n"),
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (read(afd_cmd_readfd, buffer, DEFAULT_BUFFER_SIZE) == -1)
#else
      if (read(afd_cmd_fd, buffer, DEFAULT_BUFFER_SIZE) == -1)
#endif
      {
         (void)fprintf(stderr, _("WARN    : read() error : %s (%s %d)\n"),
                       strerror(errno), __FILE__, __LINE__);
      }
      status = NO;
   }
   else if (FD_ISSET(afd_resp_fd, &rset))
        {
           if (read(afd_resp_fd, buffer, DEFAULT_BUFFER_SIZE) > 0)
           {
              if (buffer[0] != ACKN)
              {
                 (void)fprintf(stderr,
                               _("Hmm. Something is wrong here! (%s %d)\n"),
                               __FILE__, __LINE__);
                 status = 3;
              }
              else
              {
                 status = YES;
              }
           }
           else
           {
              status = 4;
           }
        }
   else if (status < 0)
        {
           (void)fprintf(stderr, _("select() error : %s (%s %d)\n"),
                         strerror(errno), __FILE__, __LINE__);
           exit(INCORRECT);
        }
        else
        {
           (void)fprintf(stderr, _("Unknown condition. (%s %d)\n"),
                    __FILE__, __LINE__);
           exit(INCORRECT);
        }

   return(status);
}
