/*
 *  check_status.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_status - checks the status of AFD
 **
 ** SYNOPSIS
 **   void check_status(Widget w)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.02.1996 H.Kiehl Created
 **   03.09.1997 H.Kiehl Added LED for AFDD.
 **   12.01.2000 H.Kiehl Added receive log.
 **   14.03.2003 H.Kiehl Added history log in button bar.
 **
 */
DESCR__E_M3

#include <stdio.h>               /* sprintf()                            */
#include <string.h>              /* strcmp(), strcpy(), memmove()        */
#include <math.h>                /* log10()                              */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>           /* mmap(), munmap()                     */
#endif
#include <signal.h>              /* kill()                               */
#include <fcntl.h>
#include <unistd.h>              /* close()                              */
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <Xm/Xm.h>
#include "mafd_ctrl.h"

extern Display                    *display;
extern XtAppContext               app;
extern int                        no_of_his_log,
                                  glyph_width;
extern unsigned long              redraw_time_status;
#ifdef HAVE_MMAP
extern off_t                      afd_active_size;
#endif
extern time_t                     afd_active_time;
extern char                       blink_flag,
                                  *pid_list,
                                  afd_active_file[];
extern struct afd_status          *p_afd_status,
                                  prev_afd_status;
extern struct filetransfer_status *fsa;


/*########################### check_status() ############################*/
void
check_status(Widget w)
{
   static int  loop_timer = 0;
   static char blink = TR_BAR;
   signed char flush = NO;

   /*
    * Check if all process are still running.
    */
   if (prev_afd_status.amg != p_afd_status->amg)
   {
      if (p_afd_status->amg == OFF)
      {
         blink_flag = ON;
      }
      else if ((p_afd_status->amg == ON) &&
               (prev_afd_status.amg == OFF) &&
               (p_afd_status->fd != OFF) &&
               (p_afd_status->archive_watch != OFF) &&
               (p_afd_status->afdd != OFF))
           {
              blink_flag = OFF;
           }
      prev_afd_status.amg = p_afd_status->amg;
      draw_proc_led(AMG_LED, prev_afd_status.amg);
      flush = YES;
   }
   if (prev_afd_status.fd != p_afd_status->fd)
   {
      if (p_afd_status->fd == OFF)
      {
         blink_flag = ON;
      }
      else if ((p_afd_status->fd == ON) &&
               (prev_afd_status.fd == OFF) &&
               (p_afd_status->amg != OFF) &&
               (p_afd_status->archive_watch != OFF) &&
               (p_afd_status->afdd != OFF))
           {
              blink_flag = OFF;
           }
      prev_afd_status.fd = p_afd_status->fd;
      draw_proc_led(FD_LED, prev_afd_status.fd);
      flush = YES;
   }
   if (prev_afd_status.archive_watch != p_afd_status->archive_watch)
   {
      if (p_afd_status->archive_watch == OFF)
      {
         blink_flag = ON;
      }
      else if ((p_afd_status->archive_watch == ON) &&
               (prev_afd_status.archive_watch == OFF) &&
               (p_afd_status->amg != OFF) &&
               (p_afd_status->fd != OFF) &&
               (p_afd_status->afdd != OFF))
           {
              blink_flag = OFF;
           }
      prev_afd_status.archive_watch = p_afd_status->archive_watch;
      draw_proc_led(AW_LED, prev_afd_status.archive_watch);
      flush = YES;
   }
   if (prev_afd_status.afdd != p_afd_status->afdd)
   {
      if (p_afd_status->afdd == OFF)
      {
         blink_flag = ON;
      }
      else if ((p_afd_status->afdd == ON) &&
               ((prev_afd_status.afdd == OFF) ||
                (prev_afd_status.afdd == NEITHER)) &&
               (p_afd_status->amg != OFF) &&
               (p_afd_status->fd != OFF) &&
               (p_afd_status->archive_watch != OFF))
           {
              blink_flag = OFF;
           }
      prev_afd_status.afdd = p_afd_status->afdd;
      draw_proc_led(AFDD_LED, prev_afd_status.afdd);
      flush = YES;
   }

   /*
    * If the AFD_ACTIVE file says a process is still running, it
    * is only true if init_afd still is alive. However, if this process
    * killed in such a way that it does not have a chance to
    * update the AFD_ACTIVE file, then this assumption is wrong.
    * So do check if the key process are still running.
    */
   loop_timer += redraw_time_status;
   if (loop_timer > 20000)
   {
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat stat_buf;
#endif

      loop_timer = 0;

#ifdef HAVE_STATX
      if (statx(0, afd_active_file, AT_STATX_SYNC_AS_STAT,
                STATX_SIZE | STATX_MTIME, &stat_buf) == 0)
#else
      if (stat(afd_active_file, &stat_buf) == 0)
#endif
      {
#ifdef HAVE_STATX
         if (stat_buf.stx_mtime.tv_sec != afd_active_time)
#else
         if (stat_buf.st_mtime != afd_active_time)
#endif
         {
            int fd;

            if (pid_list != NULL)
            {
#ifdef HAVE_MMAP
               (void)munmap((void *)pid_list, afd_active_size);
#else
               (void)munmap_emu((void *)pid_list);
#endif
            }
#ifdef HAVE_STATX
            afd_active_time = stat_buf.stx_mtime.tv_sec;
#else
            afd_active_time = stat_buf.st_mtime;
#endif

            if ((fd = open(afd_active_file, O_RDWR)) < 0)
            {
               pid_list = NULL;
            }
            else
            {
#ifdef HAVE_MMAP
               if ((pid_list = mmap(NULL,
# ifdef HAVE_STATX
                                    stat_buf.stx_size,
# else
                                    stat_buf.st_size,
# endif
                                    (PROT_READ | PROT_WRITE), MAP_SHARED,
                                    fd, 0)) == (caddr_t) -1)
#else
               if ((pid_list = mmap_emu(NULL,
# ifdef HAVE_STATX
                                        stat_buf.stx_size,
# else
                                        stat_buf.st_size,
# endif
                                        (PROT_READ | PROT_WRITE), MAP_SHARED,
                                        afd_active_file, 0)) == (caddr_t) -1)
#endif
               {
                  (void)xrec(ERROR_DIALOG, "mmap() error : %s (%s %d)",
                             strerror(errno), __FILE__, __LINE__);
                  pid_list = NULL;
               }

#ifdef HAVE_MMAP
# ifdef HAVE_STATX
               afd_active_size = stat_buf.stx_size;
# else
               afd_active_size = stat_buf.st_size;
# endif
#endif

               if (close(fd) == -1)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "close() error : %s", strerror(errno));
               }
            }
         }

#ifdef AFD_CTRL_PROC_CHECK
         if (pid_list != NULL)
         {
            int ret;

            if ((*(pid_t *)(pid_list + ((AMG_NO + 1) * sizeof(pid_t))) > 0) &&
                (p_afd_status->amg != STOPPED))
            {
               if (((ret = kill(*(pid_t *)(pid_list + ((AMG_NO + 1) * sizeof(pid_t))), 0)) == -1) &&
                   (errno == ESRCH) && (p_afd_status->amg == ON))
               {
                  /* Process is not alive, but it is in the AFD_ACTIVE file?! */
                  blink_flag = ON;
                  p_afd_status->amg = OFF;
                  prev_afd_status.amg = p_afd_status->amg;
                  draw_proc_led(AMG_LED, prev_afd_status.amg);
                  flush = YES;
               }
               else if ((ret == 0) && (prev_afd_status.amg == OFF))
                    {
                       blink_flag = OFF;
                       p_afd_status->amg = ON;
                       prev_afd_status.amg = p_afd_status->amg;
                       draw_proc_led(AMG_LED, prev_afd_status.amg);
                       flush = YES;
                    }
            }
            if ((*(pid_t *)(pid_list + ((FD_NO + 1)  * sizeof(pid_t))) > 0) &&
                (p_afd_status->fd != STOPPED))
            {
               if (((ret = kill(*(pid_t *)(pid_list + ((FD_NO + 1)  * sizeof(pid_t))), 0)) == -1) &&
                   (errno == ESRCH) && (p_afd_status->fd == ON))
               {
                  /* Process is not alive, but it is in the AFD_ACTIVE file?! */
                  blink_flag = ON;
                  p_afd_status->fd = OFF;
                  prev_afd_status.fd = p_afd_status->fd;
                  draw_proc_led(FD_LED, prev_afd_status.fd);
                  flush = YES;
               }
               else if ((ret == 0) && (prev_afd_status.fd == OFF))
                    {
                       blink_flag = OFF;
                       p_afd_status->fd = ON;
                       prev_afd_status.fd = p_afd_status->fd;
                       draw_proc_led(FD_LED, prev_afd_status.fd);
                       flush = YES;
                    }
            }
            if ((*(pid_t *)(pid_list + ((AW_NO + 1) * sizeof(pid_t))) > 0) &&
                (p_afd_status->archive_watch != STOPPED))
            {
               if (((ret = kill(*(pid_t *)(pid_list + ((AW_NO + 1) * sizeof(pid_t))), 0)) == -1) &&
                   (errno == ESRCH))
               {
                  /* Process is not alive, but it is in the AFD_ACTIVE file?! */
                  blink_flag = ON;
                  p_afd_status->archive_watch = OFF;
                  prev_afd_status.archive_watch = p_afd_status->archive_watch;
                  draw_proc_led(AW_LED, prev_afd_status.archive_watch);
                  flush = YES;
               }
               else if ((ret == 0) && (prev_afd_status.archive_watch == OFF))
                    {
                       blink_flag = OFF;
                       p_afd_status->archive_watch = ON;
                       prev_afd_status.archive_watch = p_afd_status->archive_watch;
                       draw_proc_led(AW_LED, prev_afd_status.archive_watch);
                       flush = YES;
                    }
            }
            if (prev_afd_status.afdd != NEITHER)
            {
               if ((*(pid_t *)(pid_list + ((AFDD_NO + 1) * sizeof(pid_t))) > 0) &&
                   (p_afd_status->afdd != STOPPED))
               {
                  if (((ret = kill(*(pid_t *)(pid_list + ((AFDD_NO + 1) * sizeof(pid_t))), 0)) == -1) &&
                      (errno == ESRCH))
                  {
                     /*
                      * Process is not alive, but it is in the AFD_ACTIVE
                      * file?!
                      */
                     blink_flag = ON;
                     p_afd_status->afdd = OFF;
                     prev_afd_status.afdd = p_afd_status->afdd;
                     draw_proc_led(AFDD_LED, prev_afd_status.afdd);
                     flush = YES;
                  }
                  else if ((ret == 0) && (prev_afd_status.afdd == OFF))
                       {
                          blink_flag = OFF;
                          p_afd_status->afdd = ON;
                          prev_afd_status.afdd = p_afd_status->afdd;
                          draw_proc_led(AFDD_LED, prev_afd_status.afdd);
                          flush = YES;
                       }
               }
            }
         }
#endif /* AFD_CTRL_PROC_CHECK */
      }
   }

   if (blink_flag == ON)
   {
      if (prev_afd_status.amg == OFF)
      {
         draw_proc_led(AMG_LED, blink);
         flush = YES;
      }
      if (prev_afd_status.fd == OFF)
      {
         draw_proc_led(FD_LED, blink);
         flush = YES;
      }
      if (blink == TR_BAR)
      {
         blink = OFF;
      }
      else
      {
         blink = TR_BAR;
      }
   }

   /*
    * See if there is any activity in the log files.
    */
   if (prev_afd_status.receive_log_ec != p_afd_status->receive_log_ec)
   {
      prev_afd_status.receive_log_ec = p_afd_status->receive_log_ec;
      (void)memcpy(prev_afd_status.receive_log_fifo,
                   p_afd_status->receive_log_fifo,
                   LOG_FIFO_SIZE + 1);
      draw_log_status(RECEIVE_LOG_INDICATOR,
                      prev_afd_status.receive_log_ec % LOG_FIFO_SIZE);
      flush = YES;
   }
   if (prev_afd_status.sys_log_ec != p_afd_status->sys_log_ec)
   {
      prev_afd_status.sys_log_ec = p_afd_status->sys_log_ec;
      (void)memcpy(prev_afd_status.sys_log_fifo,
                   p_afd_status->sys_log_fifo,
                   LOG_FIFO_SIZE + 1);
      draw_log_status(SYS_LOG_INDICATOR,
                      prev_afd_status.sys_log_ec % LOG_FIFO_SIZE);
      flush = YES;
   }
   if (prev_afd_status.trans_log_ec != p_afd_status->trans_log_ec)
   {
      prev_afd_status.trans_log_ec = p_afd_status->trans_log_ec;
      (void)memcpy(prev_afd_status.trans_log_fifo,
                   p_afd_status->trans_log_fifo,
                   LOG_FIFO_SIZE + 1);
      draw_log_status(TRANS_LOG_INDICATOR,
                      prev_afd_status.trans_log_ec % LOG_FIFO_SIZE);
      flush = YES;
   }

   if (p_afd_status->jobs_in_queue != prev_afd_status.jobs_in_queue)
   {
      prev_afd_status.jobs_in_queue = p_afd_status->jobs_in_queue;
      draw_queue_counter(prev_afd_status.jobs_in_queue);
      flush = YES;
   }

   if (flush == YES)
   {
      XFlush(display);
      redraw_time_status = MIN_REDRAW_TIME;
   }
   else
   {
      if (redraw_time_status < 3500)
      {
         redraw_time_status += REDRAW_STEP_TIME;
      }
#ifdef _DEBUG
      (void)fprintf(stderr, "count_channels: Redraw time = %d\n",
                    redraw_time_status);
#endif
   }

   if (no_of_his_log > 0)
   {
      if (memcmp(prev_afd_status.receive_log_history,
                 p_afd_status->receive_log_history, MAX_LOG_HISTORY) != 0)
      {
         (void)memcpy(prev_afd_status.receive_log_history,
                      p_afd_status->receive_log_history, MAX_LOG_HISTORY);
         draw_history(RECEIVE_HISTORY, 1);
         draw_history(RECEIVE_HISTORY, 0);
         flush = YES;
      }
      if (memcmp(prev_afd_status.sys_log_history,
                 p_afd_status->sys_log_history, MAX_LOG_HISTORY) != 0)
      {
         (void)memcpy(prev_afd_status.sys_log_history,
                      p_afd_status->sys_log_history, MAX_LOG_HISTORY);
         draw_history(SYSTEM_HISTORY, 1);
         draw_history(SYSTEM_HISTORY, 0);
         flush = YES;
      }
      if (memcmp(prev_afd_status.trans_log_history,
                 p_afd_status->trans_log_history, MAX_LOG_HISTORY) != 0)
      {
         (void)memcpy(prev_afd_status.trans_log_history,
                      p_afd_status->trans_log_history, MAX_LOG_HISTORY);
         draw_history(TRANSFER_HISTORY, 1);
         draw_history(TRANSFER_HISTORY, 0);
         flush = YES;
      }
   }

   /* Redraw every redraw_time ms. */
   (void)XtAppAddTimeOut(app, redraw_time_status,
                         (XtTimerCallbackProc)check_status, w);
 
   return;
}
