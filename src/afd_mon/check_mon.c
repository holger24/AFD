/*
 *  check_mon.c - Part of AFD, an automatic file distribution program.
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
 **   check_mon - Checks if another afd_mon process is running in this
 **               directory
 **
 ** SYNOPSIS
 **   int check_mon(long wait_time)
 **
 ** DESCRIPTION
 **   This function checks if another afd_mon is running. If not and the
 **   mafd process has crashed, it removes all jobs that might have
 **   survived the crash.
 **   This function is always called when we start the mafd or afd_mon
 **   process.
 **
 ** RETURN VALUES
 **   Returns ACKN or ACKN_STOPPED if another afd_mon process is active,
 **   otherwise it returns 0.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   24.08.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>               /* NULL                                 */
#include <stdlib.h>              /* calloc(), free()                     */
#include <string.h>              /* strerror(), memset()                 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>            /* struct timeval, FD_SET...            */
#include <signal.h>              /* kill()                               */
#include <unistd.h>              /* select()                             */
#ifdef HAVE_FCNTL_H
#include <fcntl.h>               /* O_RDWR, O_CREAT, O_WRONLY, etc       */
#endif
#include <errno.h>
#include "mondefs.h"

/* Local global variables */
#ifdef HAVE_STATX
static struct statx stat_buf;
#else
static struct stat stat_buf;
#endif

/* External global variables */
extern char *p_work_dir,
            mon_active_file[],
            mon_cmd_fifo[],
            probe_only_fifo[];

/* Local functions */
static void kill_jobs(void);


/*############################ check_mon() ##############################*/
int
check_mon(long wait_time)
{
#ifdef HAVE_STATX
   struct statx stat_buf_fifo;
#else
   struct stat stat_buf_fifo;
#endif

#ifdef HAVE_STATX
   if (statx(0, mon_active_file, AT_STATX_SYNC_AS_STAT,
             STATX_SIZE, &stat_buf) == 0)
#else
   if (stat(mon_active_file, &stat_buf) == 0)
#endif
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

      /*
       * Uups. Seems like another mafd process is running.
       * Make sure that this is really the case. It could be that
       * when it crashed really hard that it had no time to
       * remove this file.
       */
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(mon_cmd_fifo, &mon_cmd_readfd, &mon_cmd_fd) == -1)
#else
      if ((mon_cmd_fd = open(mon_cmd_fifo, O_RDWR)) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to open() `%s' : %s",
                    mon_cmd_fifo, strerror(errno));
         /* Now we have no way to determine if another */
         /* AFD is still running. Lets kill ALL jobs   */
         /* which appear in the 'mon_active_file'.     */
         kill_jobs();
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
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "Could not create fifo %s.", probe_only_fifo);
               exit(INCORRECT);
            }
         }
#ifdef WITHOUT_FIFO_RW_SUPPORT
         if (open_fifo_rw(probe_only_fifo, &readfd, &writefd) == -1)
#else
         if ((readfd = open(probe_only_fifo, O_RDWR)) == -1)
#endif
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Could not open fifo %s : %s",
                       probe_only_fifo, strerror(errno));
            exit(INCORRECT);
         }
         if ((val = fcntl(readfd, F_GETFL, 0)) == -1)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Failed to get file status flag with fcntl() : %s",
                       strerror(errno));
            exit(INCORRECT);
         }
         tmp_val = val;
         val |= O_NONBLOCK;
         if (fcntl(readfd, F_SETFL, val) == -1)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Failed to set file status flag with fcntl() : %s",
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
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Failed to set file status flag with fcntl() : %s",
                       strerror(errno));
            exit(INCORRECT);
         }
#ifdef _FIFO_DEBUG
         cmd[0] = IS_ALIVE; cmd[1] = '\0';
         show_fifo_data('W', "mon_cmd", cmd, 1, __FILE__, __LINE__);
#endif
         if (send_cmd(IS_ALIVE, mon_cmd_fd) < 0)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Was not able to send command via fifo.");
            exit(INCORRECT);
         }

         /* Lets see if it's still alive. For this we   */
         /* listen on another fifo. If we listen on the */
         /* same fifo we might read our own request.    */
         FD_ZERO(&rset);
         FD_SET(readfd, &rset);
         timeout.tv_usec = 0L;
         timeout.tv_sec = wait_time;

         /* Wait for message x seconds and then continue. */
         status = select(readfd + 1, &rset, NULL, NULL, &timeout);

         if (status == 0)
         {
            /* No answer from the other AFD_MON. Lets       */
            /* assume it has crashed. Now we have to remove */
            /* any old jobs and shared memory areas that    */
            /* have survived the crash.                     */
            kill_jobs();
         }
         else if (FD_ISSET(readfd, &rset))
              {
                 /* Ahhh! Another AFD_MON is working here. */
                 /* Lets quickly vanish before someone */
                 /* notice our presence.               */
                 if ((n = read(readfd, buffer, 1)) > 0)
                 {
#ifdef _FIFO_DEBUG
                    show_fifo_data('R', "probe_only", buffer, 1, __FILE__, __LINE__);
#endif
                    if ((buffer[0] == ACKN) || (buffer[0] == ACKN_STOPPED))
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
                       return((int)buffer[0]);
                    }
                    else
                    {
                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Reading garbage from fifo %s.",
                                  probe_only_fifo);
                       exit(INCORRECT);
                    }
                 }
                 else if (n < 0)
                      {
                         system_log(ERROR_SIGN, __FILE__, __LINE__,
                                    "read() error : %s", strerror(errno));
                         exit(INCORRECT);
                      }
              }
         else if (status < 0)
              {
                 system_log(FATAL_SIGN, __FILE__, __LINE__,
                            "select() error : %s", strerror(errno));
                 exit(INCORRECT);
              }
              else
              {
                 system_log(FATAL_SIGN, __FILE__, __LINE__,
                            "Unknown condition. Maybe you can tell what's going on here.");
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
   }
   /* Else we did not find the AFD_MON active file */
   /* and we may just continue.                    */

   return(0);
}


/*++++++++++++++++++++++++++++ kill_jobs() ++++++++++++++++++++++++++++++*/
static void
kill_jobs(void)
{
   char *ptr,
        *buffer;
   int  i,
        no_of_process,
        read_fd;

   if ((read_fd = open(mon_active_file, O_RDWR)) < 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to open %s : %s", mon_active_file, strerror(errno));
      exit(INCORRECT);
   }

#ifdef HAVE_STATX
   if ((buffer = calloc(stat_buf.stx_size, sizeof(char))) == NULL)
#else
   if ((buffer = calloc(stat_buf.st_size, sizeof(char))) == NULL)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "calloc() error : %s", strerror(errno));
      exit(INCORRECT);
   }

#ifdef HAVE_STATX
   if (read(read_fd, buffer, stat_buf.stx_size) != stat_buf.stx_size)
#else
   if (read(read_fd, buffer, stat_buf.st_size) != stat_buf.st_size)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "read() error : %s", strerror(errno));
      exit(INCORRECT);
   }
   if (close(read_fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }

   /* Kill the log processes. */
   ptr = buffer;
   ptr += sizeof(pid_t); /* Ignore afd_mon process */
   if (*(pid_t *)ptr > 0)
   {
      (void)kill(*(pid_t *)ptr, SIGINT); /* system log */
   }
   ptr += sizeof(pid_t);
   if (*(pid_t *)ptr > 0)
   {
      (void)kill(*(pid_t *)ptr, SIGINT); /* monitor log */
   }
   ptr += sizeof(pid_t);

   /* Try to send kill signal to all running process */
   no_of_process = *(int *)ptr;
   ptr += sizeof(pid_t);
   for (i = 0; i < no_of_process; i++)
   {
      if (*(pid_t *)ptr > 0)
      {
         (void)kill(*(pid_t *)ptr, SIGINT);
      }
      ptr += sizeof(pid_t);
      if (*(pid_t *)ptr > 0)
      {
         (void)kill(*(pid_t *)ptr, SIGINT);
      }
      ptr += sizeof(pid_t);
   }
   free(buffer);

   return;
}
