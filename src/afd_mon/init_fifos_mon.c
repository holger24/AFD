/*
 *  init_fifos_mon.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2022 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   init_fifos_mon - creates and opens all fifos needed by AFD_MON
 **
 ** SYNOPSIS
 **   int init_fifos_mon(void)
 **
 ** DESCRIPTION
 **   Creates and opens all fifos that are needed by the AFD_MON to
 **   communicate with afdm, AFD, etc.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   28.08.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>           /* strcpy(), strcat(), strerror(),         */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>            /* O_RDWR, O_CREAT, O_WRONLY, etc          */
#endif
#include <errno.h>
#include "mondefs.h"

extern int  mon_cmd_fd,
            mon_log_fd,
            mon_resp_fd,
            probe_only_fd,
#ifdef WITHOUT_FIFO_RW_SUPPORT
            mon_cmd_writefd,
            mon_log_readfd,
            mon_resp_readfd,
            probe_only_readfd,
#endif
            sys_log_fd;
extern char mon_cmd_fifo[],
            probe_only_fifo[],
            *p_work_dir;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$ init_fifos_mon() $$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
init_fifos_mon(void)
{
   char         mon_log_fifo[MAX_PATH_LENGTH],
                mon_resp_fifo[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   /* Initialise fifo names. */
   (void)strcpy(mon_resp_fifo, p_work_dir);
   (void)strcat(mon_resp_fifo, FIFO_DIR);
   (void)strcpy(mon_cmd_fifo, mon_resp_fifo);
   (void)strcat(mon_cmd_fifo, MON_CMD_FIFO);
   (void)strcpy(mon_log_fifo, mon_resp_fifo);
   (void)strcat(mon_log_fifo, MON_LOG_FIFO);
   (void)strcpy(probe_only_fifo, mon_resp_fifo);
   (void)strcat(probe_only_fifo, MON_PROBE_ONLY_FIFO);
   (void)strcat(mon_resp_fifo, MON_RESP_FIFO);

   /* If the process AFD has not yet created these fifos */
   /* create them now.                                   */
#ifdef HAVE_STATX
   if ((statx(0, mon_cmd_fifo, AT_STATX_SYNC_AS_STAT,
              STATX_MODE, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.stx_mode)))
#else
   if ((stat(mon_cmd_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
#endif
   {
      if (make_fifo(mon_cmd_fifo) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not create fifo %s.", mon_cmd_fifo);
         return(INCORRECT);
      }
   }
#ifdef HAVE_STATX
   if ((statx(0, mon_resp_fifo, AT_STATX_SYNC_AS_STAT,
              STATX_MODE, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.stx_mode)))
#else
   if ((stat(mon_resp_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
#endif
   {
      if (make_fifo(mon_resp_fifo) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not create fifo %s.", mon_resp_fifo);
         return(INCORRECT);
      }
   }
#ifdef HAVE_STATX
   if ((statx(0, probe_only_fifo, AT_STATX_SYNC_AS_STAT,
              STATX_MODE, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.stx_mode)))
#else
   if ((stat(probe_only_fifo, &stat_buf) == -1) ||
       (!S_ISFIFO(stat_buf.st_mode)))
#endif
   {
      if (make_fifo(probe_only_fifo) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not create fifo %s.", probe_only_fifo);
         return(INCORRECT);
      }
   }
#ifdef HAVE_STATX
   if ((statx(0, mon_log_fifo, AT_STATX_SYNC_AS_STAT,
              STATX_MODE, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.stx_mode)))
#else
   if ((stat(mon_log_fifo, &stat_buf) == -1) ||
       (!S_ISFIFO(stat_buf.st_mode)))
#endif
   {
      if (make_fifo(mon_log_fifo) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not create fifo %s.", mon_log_fifo);
         return(INCORRECT);
      }
   }

   /* Open fifo to log to monitor_log. */
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(mon_log_fifo, &mon_log_readfd, &mon_log_fd) == -1)
#else
   if ((mon_log_fd = coe_open(mon_log_fifo, O_RDWR)) == -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s", mon_log_fifo, strerror(errno));
      return(INCORRECT);
   }

   /* Open fifo to AFD to receive commands. */
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(mon_cmd_fifo, &mon_cmd_fd, &mon_cmd_writefd) == -1)
#else
   if ((mon_cmd_fd = coe_open(mon_cmd_fifo, O_RDWR)) == -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s", mon_cmd_fifo, strerror(errno));
      return(INCORRECT);
   }

   /* Open fifo to AFD to acknowledge the command. */
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(mon_resp_fifo, &mon_resp_readfd, &mon_resp_fd) == -1)
#else
   if ((mon_resp_fd = coe_open(mon_resp_fifo, O_RDWR)) == -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s", mon_resp_fifo, strerror(errno));
      return(INCORRECT);
   }

#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(probe_only_fifo, &probe_only_readfd, &probe_only_fd) == -1)
#else
   if ((probe_only_fd = coe_open(probe_only_fifo, O_RDWR)) == -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s",
                 probe_only_fifo, strerror(errno));
      return(INCORRECT);
   }

   return(SUCCESS);
}
