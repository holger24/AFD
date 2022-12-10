/*
 *  inform_fd_about_fsa_change.c - Part of AFD, an automatic file
 *                                 distribution program.
 *  Copyright (c) 2002 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   inform_fd_about_fsa_change - informs FD that FSA will be changed
 **
 ** SYNOPSIS
 **   int inform_fd_about_fsa_change(void)
 **
 ** DESCRIPTION
 **   This function informs FD that we are about to change the FSA.
 **   We need to ensure that FD gets this message so it does not
 **   continue to write stuff to the FSA and FD must confirm it.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   07.04.2002 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                    /* strcpy(), strcat(), strerror() */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

/* External global variables. */
extern char              *p_work_dir;
extern struct afd_status *p_afd_status;


/*##################### inform_fd_about_fsa_change() ####################*/
void
inform_fd_about_fsa_change(void)
{
   if (p_afd_status->fd == ON)
   {
      int          cmd_fd,
                   gotcha = NO,
                   loops = 0,
                   status;
#ifdef WITHOUT_FIFO_RW_SUPPORT
      int          readfd; /* Not used. */
#endif
      char         cmd_fifo[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat  stat_buf;
#endif

      (void)snprintf(cmd_fifo, MAX_PATH_LENGTH, "%s%s%s",
                     p_work_dir, FIFO_DIR, FD_CMD_FIFO);
#ifdef HAVE_STATX
      if ((statx(0, cmd_fifo, AT_STATX_SYNC_AS_STAT, STATX_MODE,
                 &stat_buf) == -1) || (!S_ISFIFO(stat_buf.stx_mode)))
#else
      if ((stat(cmd_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
#endif
      {
         if (make_fifo(cmd_fifo) < 0)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                      _("Could not create fifo `%s'."), cmd_fifo);
            exit(INCORRECT);
         }
      }
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(cmd_fifo, &readfd, &cmd_fd) == -1)
#else
      if ((cmd_fd = open(cmd_fifo, O_RDWR)) == -1)
#endif
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                   _("Could not open() fifo `%s' : %s"),
                   cmd_fifo, strerror(errno));
         exit(INCORRECT);
      }
      if ((status = send_cmd(FSA_ABOUT_TO_CHANGE, cmd_fd)) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                   _("Failed to send update command to FD : %s"),
                   strerror(-status));
      }
      if (close(cmd_fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                   _("close() error : %s"), strerror(errno));
      }
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (close(readfd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                   _("close() error : %s"), strerror(errno));
      }
#endif

      /* Wait for FD to reply. */
      do
      {
         if (p_afd_status->amg_jobs & FD_WAITING)
         {
            gotcha = YES;
            break;
         }
         (void)my_usleep(100000L);
      } while (loops++ < WAIT_LOOPS);
      if (gotcha == NO)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                   _("Hmmm, FD does not reply!"));
      }
#ifdef DEBUG_WAIT_LOOP
      else
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                   _("Got FD_WAITING after %d loops (%8.3fs)."),
                   loops, (float)((float)loops / 10.0));
      }
#endif
   } /* if (p_afd_status->fd == ON) */

   return;
}
