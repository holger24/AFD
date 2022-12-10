/*
 *  init_fifos_afd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2022 Deutscher Wetterdienst (DWD),
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
 **   init_fifos_afd - initialize some fifos for init_afd
 **
 ** SYNOPSIS
 **   void init_fifos_afd(void)
 **
 ** DESCRIPTION
 **   Initializes fifos for process init_afd. If they do not exist
 **   they will be created.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.02.1996 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>           /* strcpy(), strcat(), strerror(),         */
#include <stdlib.h>           /* exit()                                  */
#include <unistd.h>           /* unlink()                                */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>            /* O_RDWR, O_WRONLY, etc                   */
#include <errno.h>

extern int  afd_cmd_fd,
            afd_resp_fd,
            amg_cmd_fd,
            fd_cmd_fd,
            probe_only_fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
extern int  afd_cmd_writefd,
            afd_resp_readfd,
            amg_cmd_readfd,
            fd_cmd_readfd,
            probe_only_readfd;
#endif
extern char *p_work_dir;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$ init_fifos_afd() $$$$$$$$$$$$$$$$$$$$$$$$$$$*/
void
init_fifos_afd(void)
{
   char         afd_cmd_fifo[MAX_PATH_LENGTH],
                afd_resp_fifo[MAX_PATH_LENGTH],
                amg_cmd_fifo[MAX_PATH_LENGTH],
                event_log_fifo[MAX_PATH_LENGTH],
                fd_cmd_fifo[MAX_PATH_LENGTH],
                ip_fin_fifo[MAX_PATH_LENGTH],
#ifdef _MAINTAINER_LOG
                maintainer_log_fifo[MAX_PATH_LENGTH],
#endif
                probe_only_fifo[MAX_PATH_LENGTH],
                system_log_fifo[MAX_PATH_LENGTH],
                trans_db_log_fifo[MAX_PATH_LENGTH],
                transfer_log_fifo[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   /* Initialise fifo names. */
   (void)strcpy(transfer_log_fifo, p_work_dir);
   (void)strcat(transfer_log_fifo, FIFO_DIR);
   (void)strcpy(event_log_fifo, transfer_log_fifo);
   (void)strcat(event_log_fifo, EVENT_LOG_FIFO);
   (void)strcpy(trans_db_log_fifo, transfer_log_fifo);
   (void)strcat(trans_db_log_fifo, TRANS_DEBUG_LOG_FIFO);
   (void)strcpy(system_log_fifo, transfer_log_fifo);
   (void)strcat(system_log_fifo, SYSTEM_LOG_FIFO);
#ifdef _MAINTAINER_LOG
   (void)strcpy(maintainer_log_fifo, transfer_log_fifo);
   (void)strcat(maintainer_log_fifo, MAINTAINER_LOG_FIFO);
#endif
   (void)strcpy(afd_cmd_fifo, transfer_log_fifo);
   (void)strcat(afd_cmd_fifo, AFD_CMD_FIFO);
   (void)strcpy(afd_resp_fifo, transfer_log_fifo);
   (void)strcat(afd_resp_fifo, AFD_RESP_FIFO);
   (void)strcpy(amg_cmd_fifo, transfer_log_fifo);
   (void)strcat(amg_cmd_fifo, AMG_CMD_FIFO);
   (void)strcpy(fd_cmd_fifo, transfer_log_fifo);
   (void)strcat(fd_cmd_fifo, FD_CMD_FIFO);
   (void)strcpy(ip_fin_fifo, transfer_log_fifo);
   (void)strcat(ip_fin_fifo, IP_FIN_FIFO);
   (void)strcpy(probe_only_fifo, transfer_log_fifo);
   (void)strcat(probe_only_fifo, PROBE_ONLY_FIFO);

   (void)strcat(transfer_log_fifo, TRANSFER_LOG_FIFO);

   /* First remove any stale fifos. Maybe they still have        */
   /* some garbage. So lets remove it before it can do any harm. */
   (void)unlink(trans_db_log_fifo);
   (void)unlink(afd_cmd_fifo);
   (void)unlink(afd_resp_fifo);
   (void)unlink(amg_cmd_fifo);
   (void)unlink(fd_cmd_fifo);
   (void)unlink(ip_fin_fifo);

   /* OK. Now lets make all fifos. */
   if (make_fifo(system_log_fifo) < 0)
   {
      (void)fprintf(stderr, _("Could not create fifo `%s'. (%s %d)\n"),
                    system_log_fifo, __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (make_fifo(transfer_log_fifo) < 0)
   {
      (void)fprintf(stderr, _("Could not create fifo `%s'. (%s %d)\n"),
                    transfer_log_fifo, __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (make_fifo(event_log_fifo) < 0)
   {
      (void)fprintf(stderr, _("Could not create fifo `%s'. (%s %d)\n"),
                    event_log_fifo, __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (make_fifo(trans_db_log_fifo) < 0)
   {
      (void)fprintf(stderr, _("Could not create fifo `%s'. (%s %d)\n"),
                    trans_db_log_fifo, __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef _MAINTAINER_LOG
   if (make_fifo(maintainer_log_fifo) < 0)
   {
      (void)fprintf(stderr, _("Could not create fifo `%s'. (%s %d)\n"),
                    maintainer_log_fifo, __FILE__, __LINE__);
      exit(INCORRECT);
   }
#endif
   if (make_fifo(afd_cmd_fifo) < 0)
   {
      (void)fprintf(stderr, _("Could not create fifo `%s'. (%s %d)\n"),
                    afd_cmd_fifo, __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (make_fifo(afd_resp_fifo) < 0)
   {
      (void)fprintf(stderr, _("Could not create fifo `%s'. (%s %d)\n"),
                    afd_resp_fifo, __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (make_fifo(amg_cmd_fifo) < 0)
   {
      (void)fprintf(stderr, _("Could not create fifo `%s'. (%s %d)\n"),
                    amg_cmd_fifo, __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (make_fifo(fd_cmd_fifo) < 0)
   {
      (void)fprintf(stderr, _("Could not create fifo `%s'. (%s %d)\n"),
                    fd_cmd_fifo, __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (make_fifo(ip_fin_fifo) < 0)
   {
      (void)fprintf(stderr, _("Could not create fifo `%s'. (%s %d)\n"),
                    ip_fin_fifo, __FILE__, __LINE__);
      exit(INCORRECT);
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
         (void)fprintf(stderr, _("Could not create fifo `%s'. (%s %d)\n"),
                       probe_only_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   /* Now lets open all fifos needed by the AFD. */
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(afd_cmd_fifo, &afd_cmd_fd, &afd_cmd_writefd) == -1)
#else
   if ((afd_cmd_fd = coe_open(afd_cmd_fifo, O_RDWR)) == -1)
#endif
   {
      (void)fprintf(stderr, _("Could not open fifo `%s' : %s (%s %d)\n"),
                    afd_cmd_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(afd_resp_fifo, &afd_resp_readfd, &afd_resp_fd) == -1)
#else
   if ((afd_resp_fd = coe_open(afd_resp_fifo, O_RDWR)) == -1)
#endif
   {
      (void)fprintf(stderr, _("Could not open fifo `%s' : %s (%s %d)\n"),
                    afd_resp_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(amg_cmd_fifo, &amg_cmd_readfd, &amg_cmd_fd) == -1)
#else
   if ((amg_cmd_fd = coe_open(amg_cmd_fifo, O_RDWR)) == -1)
#endif
   {
      (void)fprintf(stderr, _("Could not open fifo `%s' : %s (%s %d)\n"),
                    amg_cmd_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(fd_cmd_fifo, &fd_cmd_readfd, &fd_cmd_fd) == -1)
#else
   if ((fd_cmd_fd = coe_open(fd_cmd_fifo, O_RDWR)) == -1)
#endif
   {
      (void)fprintf(stderr, _("Could not open fifo `%s' : %s (%s %d)\n"),
                    fd_cmd_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(probe_only_fifo, &probe_only_readfd, &probe_only_fd) == -1)
#else
   if ((probe_only_fd = coe_open(probe_only_fifo, O_RDWR)) == -1)
#endif
   {
      (void)fprintf(stderr, _("Could not open fifo `%s' : %s (%s %d)\n"),
                    probe_only_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   return;
}
