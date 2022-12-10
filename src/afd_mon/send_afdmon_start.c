/*
 *  send_afdmon_start.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2021, 2022 Deutscher Wetterdienst (DWD),
 *                           Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   send_afdmon_start - sends a start command via fifo to afd_mon
 **
 ** SYNOPSIS
 **   int send_afdmon_start(void)
 **
 ** DESCRIPTION
 **   This function sends a start comman via fifo to a running
 **   afd_mon process.
 **
 ** RETURN VALUES
 **   Returns 1 if the afd_mon process has acknowledged the reply.
 **   Otherwise 0 is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   15.02.2021 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>               /* NULL                                 */
#include <stdlib.h>              /* calloc(), free()                     */
#include <string.h>              /* strerror(), memset()                 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>            /* struct timeval, FD_SET...            */
#include <unistd.h>              /* select()                             */
#ifdef HAVE_FCNTL_H
#include <fcntl.h>               /* O_RDWR, O_CREAT, O_WRONLY, etc       */
#endif
#include <errno.h>
#include "mondefs.h"

/* External global variables */
extern char *p_work_dir,
            mon_cmd_fifo[],
            probe_only_fifo[];


/*######################### send_afdmon_start() #########################*/
int
send_afdmon_start(void)
{
   int            readfd,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                  writefd,
                  mon_cmd_readfd,
#endif
                  mon_cmd_fd,
                  n,
                  status;
   char           buffer[2];
#ifdef _FIFO_DEBUG
   char           cmd[2];
#endif
   fd_set         rset;
   struct timeval timeout;
#ifdef HAVE_STATX
   struct statx   stat_buf_fifo;
#else
   struct stat    stat_buf_fifo;
#endif

#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(mon_cmd_fifo, &mon_cmd_readfd, &mon_cmd_fd) == -1)
#else
   if ((mon_cmd_fd = open(mon_cmd_fifo, O_RDWR)) == -1)
#endif
   {
      (void)fprintf(stderr,
                    "ERROR   : Failed to open() `%s' : %s\n",
                    mon_cmd_fifo, strerror(errno));
   }
   else
   {
      int val,
          tmp_val;

#ifdef HAVE_STATX
      if ((statx(0, probe_only_fifo, AT_STATX_SYNC_AS_STAT,
                 STATX_MODE, &stat_buf_fifo) == -1) ||
          (!S_ISFIFO(stat_buf_fifo.stx_mode)))
#else
      if ((stat(probe_only_fifo, &stat_buf_fifo) == -1) ||
          (!S_ISFIFO(stat_buf_fifo.st_mode)))
#endif
      {
         if (make_fifo(probe_only_fifo) < 0)
         {
            (void)fprintf(stderr,
                          "ERROR   : Could not create fifo %s.\n",
                          probe_only_fifo);
            exit(INCORRECT);
         }
      }
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(probe_only_fifo, &readfd, &writefd) == -1)
#else
      if ((readfd = open(probe_only_fifo, O_RDWR)) == -1)
#endif
      {
         (void)fprintf(stderr,
                       "ERROR   : Could not open fifo %s : %s",
                       probe_only_fifo, strerror(errno));
         exit(INCORRECT);
      }
      if ((val = fcntl(readfd, F_GETFL, 0)) == -1)
      {
         (void)fprintf(stderr,
                       "ERROR   : Failed to get file status flag with fcntl() : %s\n",
                       strerror(errno));
         exit(INCORRECT);
      }
      tmp_val = val;
      val |= O_NONBLOCK;
      if (fcntl(readfd, F_SETFL, val) == -1)
      {
         (void)fprintf(stderr,
                       "ERROR   : Failed to set file status flag with fcntl() : %s\n",
                       strerror(errno));
         exit(INCORRECT);
      }

      /* Make sure there is no garbage in the fifo. */
      while (read(readfd, buffer, 1) > 0)
      {
         ;
      }
      if (fcntl(readfd, F_SETFL, tmp_val) == -1)
      {
         (void)fprintf(stderr,
                       "ERROR   : Failed to set file status flag with fcntl() : %s\n",
                       strerror(errno));
         exit(INCORRECT);
      }
#ifdef _FIFO_DEBUG
      cmd[0] = START; cmd[1] = '\0';
      show_fifo_data('W', "mon_cmd", cmd, 1, __FILE__, __LINE__);
#endif
      if (send_cmd(START, mon_cmd_fd) < 0)
      {
         (void)fprintf(stderr,
                       "ERROR   : Was not able to send START command via fifo.\n");
         exit(INCORRECT);
      }

      /* Lets see if it's still alive. For this we   */
      /* listen on another fifo. If we listen on the */
      /* same fifo we might read our own request.    */
      FD_ZERO(&rset);
      FD_SET(readfd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = 15;

      /* Wait for message x seconds and then continue. */
      status = select(readfd + 1, &rset, NULL, NULL, &timeout);

      if (status == 0)
      {
         /* No answer from the AFD_MON process. */
         (void)fprintf(stderr, "afd_mon process not responding. Unable to send start command.\n");
      }
      else if (FD_ISSET(readfd, &rset))
           {
              if ((n = read(readfd, buffer, 1)) > 0)
              {
#ifdef _FIFO_DEBUG
                 show_fifo_data('R', "probe_only", buffer, 1, __FILE__, __LINE__);
#endif
                 if (buffer[0] == ACKN)
                 {
                    if (close(readfd) == -1)
                    {
                       system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                  "close() error : %s", strerror(errno));
                    }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                    if (close(writefd) == -1)
                    {
                       system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                  "close() error : %s", strerror(errno));
                    }
                    if (close(mon_cmd_readfd) == -1)
                    {
                       system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                  "close() error : %s", strerror(errno));
                    }
#endif
                    if (close(mon_cmd_fd) == -1)
                    {
                       system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                  "close() error : %s", strerror(errno));
                    }
                    return(1);
                 }
                 else
                 {
                    (void)fprintf(stderr,
                                  "Reading garbage from fifo %s.\n",
                                  probe_only_fifo);
                    exit(INCORRECT);
                 }
              }
              else if (n < 0)
                   {
                      (void)fprintf(stderr,
                                    "ERROR   : read() error : %s\n",
                                    strerror(errno));
                      exit(INCORRECT);
                   }
           }
      else if (status < 0)
           {
              (void)fprintf(stderr,
                            "ERROR   : select() error : %s\n", strerror(errno));
              exit(INCORRECT);
           }
           else
           {
              (void)fprintf(stderr,
                            "ERROR   : Unknown condition. Maybe you can tell what's going on here.\n");
              exit(INCORRECT);
           }

      if (close(readfd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "close() error : %s", strerror(errno));
      }
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (close(writefd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "close() error : %s", strerror(errno));
      }
      if (close(mon_cmd_readfd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "close() error : %s", strerror(errno));
      }
#endif
      if (close(mon_cmd_fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "close() error : %s", strerror(errno));
      }
   }

   return(0);
}
