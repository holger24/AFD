/*
 *  check_mon_status.c - Part of AFD, an automatic file distribution program.
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
 **   check_mon_status - checks the status of AFD_MON
 **
 ** SYNOPSIS
 **   void check_mon_status(Widget w)
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
 **   09.06.2005 H.Kiehl Created
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
#include <time.h>                /* time()                               */
#include <signal.h>              /* kill()                               */
#include <fcntl.h>
#include <unistd.h>              /* close()                              */
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <Xm/Xm.h>
#include "mon_ctrl.h"

extern Display                *display;
extern XtAppContext           app;
extern Widget                 appshell;
extern int                    glyph_width;
extern unsigned long          redraw_time_status;
extern time_t                 afd_mon_active_time;
#ifdef HAVE_MMAP
extern off_t                  afd_mon_active_size;
#endif
extern char                   mon_active_file[],
                              blink_flag,
                              *pid_list;
extern struct afd_mon_status  *p_afd_mon_status,
                              prev_afd_mon_status;
extern struct mon_status_area *msa;


/*######################### check_mon_status() ##########################*/
void
check_mon_status(Widget w)
{
   static int    loop_timer = 0;
   static char   blink = TR_BAR;
   static time_t next_minute = 0L;
   signed char   flush = NO;
   time_t        current_time;

   /*
    * Check if afd_mon process is still running.
    */
   if (prev_afd_mon_status.afd_mon != p_afd_mon_status->afd_mon)
   {
      if (p_afd_mon_status->afd_mon == OFF)
      {
         blink_flag = ON;
      }
      prev_afd_mon_status.afd_mon = p_afd_mon_status->afd_mon;
      draw_mon_proc_led(AFDMON_LED, prev_afd_mon_status.afd_mon, -1, -1);
      flush = YES;
   }

   loop_timer += redraw_time_status;
   if (loop_timer > 20000)
   {
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat stat_buf;
#endif

#ifdef HAVE_STATX
      if (statx(0, mon_active_file, AT_STATX_SYNC_AS_STAT,
                STATX_SIZE | STATX_MTIME, &stat_buf) == 0)
#else
      if (stat(mon_active_file, &stat_buf) == 0)
#endif
      {
#ifdef HAVE_STATX
         if (stat_buf.stx_mtime.tv_sec != afd_mon_active_time)
#else
         if (stat_buf.st_mtime != afd_mon_active_time)
#endif
         {
            int fd;

            if (pid_list != NULL)
            {
#ifdef HAVE_MMAP
               (void)munmap((void *)pid_list, afd_mon_active_size);
#else
               (void)munmap_emu((void *)pid_list);
#endif
            }
#ifdef HAVE_STATX
            afd_mon_active_time = stat_buf.stx_mtime.tv_sec;
#else
            afd_mon_active_time = stat_buf.st_mtime;
#endif

            if ((fd = open(mon_active_file, O_RDWR)) < 0)
            {
               pid_list = NULL;
            }
            else
            {
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
               afd_mon_active_size = stat_buf.stx_size;
# else
               afd_mon_active_size = stat_buf.st_size;
# endif
               if ((pid_list = mmap(NULL, afd_mon_active_size,
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
                                        mon_active_file, 0)) == (caddr_t) -1)
#endif
               {
                  (void)xrec(ERROR_DIALOG, "mmap() error : %s (%s %d)",
                             strerror(errno), __FILE__, __LINE__);
                  pid_list = NULL;
               }


               if (close(fd) == -1)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "close() error : %s", strerror(errno));
               }
            }
         }

         if (pid_list != NULL)
         {
            if (*(pid_t *)(pid_list) > 0)
            {
               if ((kill(*(pid_t *)(pid_list), 0) == -1) && (errno == ESRCH) &&
                   (p_afd_mon_status->afd_mon == ON))
               {
                  /* Process is not alive, but it is in the AFD_ACTIVE file?! */
                  blink_flag = ON;
                  p_afd_mon_status->afd_mon = OFF;
                  prev_afd_mon_status.afd_mon = p_afd_mon_status->afd_mon;
                  draw_mon_proc_led(AFDMON_LED, prev_afd_mon_status.afd_mon, -1, -1);
                  flush = YES;
               }
            }
         }
      }
   }

   if (blink_flag == ON)
   {
      if (prev_afd_mon_status.afd_mon == OFF)
      {
         draw_mon_proc_led(AFDMON_LED, blink, -1, -1);
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
   if (prev_afd_mon_status.mon_sys_log_ec != p_afd_mon_status->mon_sys_log_ec)
   {
      prev_afd_mon_status.mon_sys_log_ec = p_afd_mon_status->mon_sys_log_ec;
      (void)memcpy(prev_afd_mon_status.mon_sys_log_fifo,
                   p_afd_mon_status->mon_sys_log_fifo,
                   LOG_FIFO_SIZE + 1);
      draw_mon_log_status(MON_SYS_LOG_INDICATOR,
                          prev_afd_mon_status.mon_sys_log_ec % LOG_FIFO_SIZE);
      flush = YES;
   }
   if (prev_afd_mon_status.mon_log_ec != p_afd_mon_status->mon_log_ec)
   {
      prev_afd_mon_status.mon_log_ec = p_afd_mon_status->mon_log_ec;
      (void)memcpy(prev_afd_mon_status.mon_log_fifo,
                   p_afd_mon_status->mon_log_fifo,
                   LOG_FIFO_SIZE + 1);
      draw_mon_log_status(MON_LOG_INDICATOR,
                          prev_afd_mon_status.mon_log_ec % LOG_FIFO_SIZE);
      flush = YES;
   }

   current_time = time(NULL);
   if (current_time >= next_minute)
   {
      draw_clock(current_time);
      next_minute = ((current_time / 60) + 1) * 60;
   }

   if (flush == YES)
   {
      XFlush(display);
      redraw_time_status = MIN_REDRAW_TIME;
   }
   else
   {
      if (redraw_time_status < 2000)
      {
         redraw_time_status += REDRAW_STEP_TIME;
      }
   }

   /* Redraw every redraw_time ms. */
   (void)XtAppAddTimeOut(app, redraw_time_status,
                         (XtTimerCallbackProc)check_mon_status, w);
 
   return;
}
