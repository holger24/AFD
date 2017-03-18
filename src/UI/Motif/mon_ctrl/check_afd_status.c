/*
 *  check_afd_status.c - Part of AFD, an automatic file distribution
 *                       program.
 *  Copyright (c) 1998 - 2017 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_afd_status - checks the if status of each AFD for any
 **                      change
 **
 ** SYNOPSIS
 **   void check_afd_status(Widget w)
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
 **   04.10.1998 H.Kiehl Created
 **   10.09.2000 H.Kiehl Addition of log history.
 **   27.02.2005 H.Kiehl Option to switch between two AFD's.
 **   23.11.2008 H.Kiehl Added danger_no_of_jobs.
 **
 */
DESCR__E_M3

#include <stdio.h>               /* sprintf()                            */
#include <string.h>              /* strcpy(), memcpy()                   */
#include <stdlib.h>              /* calloc(), realloc(), free()          */
#include <signal.h>              /* kill(), SIGKILL                      */
#include <math.h>                /* log10()                              */
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <Xm/Xm.h>
#include "mon_ctrl.h"

extern Display                *display;
extern Window                 line_window;
extern XtAppContext           app;
extern char                   line_style,
                              *p_work_dir;
extern float                  max_bar_length;
extern int                    bar_thickness_3,
                              glyph_width,
                              his_log_set,
                              no_of_active_process,
                              no_of_afds,
                              no_of_afds_invisible,
                              no_of_afds_visible,
                              no_of_columns,
                              no_selected,
                              *vpl,
                              window_width;
extern unsigned short         step_size;
extern unsigned long          redraw_time_line;
extern struct apps_list       *apps_list;
extern struct mon_line        *connect_data;
extern struct mon_status_area *msa;

/* Local function prototypes. */
static int                    check_msa_data(char *),
                              check_disp_data(char *, int);


/*######################### check_afd_status() ##########################*/
void
check_afd_status(Widget w)
{
   signed char flush;
   int         draw_identifier,
               i,
               k,
               x,
               y,
               pos,
               location_where_changed,
               new_bar_length,
               old_bar_length,
               prev_no_of_afds,
               redraw_everything = NO;

   /* Initialise variables. */
   location_where_changed = no_of_afds + 10;
   flush = NO;

   /*
    * See if a host has been added or removed from the MSA.
    * If it changed resize the window.
    */
   prev_no_of_afds = no_of_afds;
   if (check_msa() == YES)
   {
      int             prev_no_of_afds_visible = no_of_afds_visible,
                      prev_plus_minus;
      size_t          new_size = no_of_afds * sizeof(struct mon_line);
      struct mon_line *new_connect_data,
                      *tmp_connect_data;

      if ((new_connect_data = calloc(no_of_afds,
                                     sizeof(struct mon_line))) == NULL)
      {
         (void)xrec(FATAL_DIALOG, "calloc() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }
      if ((vpl = realloc(vpl, (no_of_afds * sizeof(int)))) == NULL)
      {
         (void)xrec(FATAL_DIALOG, "Failed to realloc() %d bytes : %s (%s %d)",
                    (no_of_afds * sizeof(int)), strerror(errno),
                    __FILE__, __LINE__);
         return;
      }

      /*
       * First try to copy the connect data from the old structure
       * so long as the hostnames are the same.
       */
      for (i = 0, location_where_changed = 0;
           i < prev_no_of_afds; i++, location_where_changed++)
      {
         if (my_strcmp(connect_data[i].afd_alias, msa[i].afd_alias) == 0)
         {
            (void)memcpy(&new_connect_data[i], &connect_data[i],
                         sizeof(struct mon_line));
         }
         else
         {
            break;
         }
      }

      prev_plus_minus = -1;
      for (i = location_where_changed; i < no_of_afds; i++)
      {
         if ((pos = check_disp_data(msa[i].afd_alias,
                                    prev_no_of_afds)) != INCORRECT)
         {
            (void)memcpy(&new_connect_data[i], &connect_data[pos],
                         sizeof(struct mon_line));
            if (new_connect_data[i].rcmd == '\0')
            {
               prev_plus_minus = new_connect_data[i].plus_minus;
            }
            else
            {
               if (prev_plus_minus != -1)
               {
                  new_connect_data[i].plus_minus = prev_plus_minus;
               }
            }
         }
         else /* A new host has been added. */
         {
            /* Initialise values for new host. */
            (void)strcpy(new_connect_data[i].afd_alias, msa[i].afd_alias);
            new_connect_data[i].afd_toggle = msa[i].afd_toggle;
            new_connect_data[i].afd_alias_length = strlen(new_connect_data[i].afd_alias);
            if ((msa[i].afd_switching != NO_SWITCHING) &&
                (new_connect_data[i].afd_alias_length < MAX_AFDNAME_LENGTH))
            {
               int pos;

               (void)memset(new_connect_data[i].afd_display_str, ' ',
                            MAX_AFDNAME_LENGTH);
               pos = sprintf(new_connect_data[i].afd_display_str, "%s%d",
                             new_connect_data[i].afd_alias,
                             new_connect_data[i].afd_toggle + 1);
               new_connect_data[i].afd_display_str[pos] = ' ';
               new_connect_data[i].afd_display_str[MAX_AFDNAME_LENGTH] = '\0';
            }
            else
            {
               (void)sprintf(new_connect_data[i].afd_display_str, "%-*s",
                             MAX_AFDNAME_LENGTH, new_connect_data[i].afd_alias);
            }
            (void)memcpy(new_connect_data[i].sys_log_fifo,
                         msa[i].sys_log_fifo, LOG_FIFO_SIZE + 1);
            if (his_log_set > 0)
            {
               (void)memcpy(new_connect_data[i].log_history, msa[i].log_history,
                            (NO_OF_LOG_HISTORY * MAX_LOG_HISTORY));
            }
            new_connect_data[i].sys_log_ec = msa[i].sys_log_ec;
            new_connect_data[i].amg = msa[i].amg;
            new_connect_data[i].fd = msa[i].fd;
            new_connect_data[i].archive_watch = msa[i].archive_watch;
            new_connect_data[i].rcmd = msa[i].rcmd[0];
            if (new_connect_data[i].rcmd == '\0')
            {
               new_connect_data[i].plus_minus = PM_OPEN_STATE;
               prev_plus_minus = PM_OPEN_STATE;
            }
            else
            {
               if ((i - 1) >= 0)
               {
                  new_connect_data[i].plus_minus = new_connect_data[i - 1].plus_minus;
               }
               else
               {
                  new_connect_data[i].plus_minus = PM_OPEN_STATE;
               }
            }
            if ((new_connect_data[i].amg == OFF) ||
                (new_connect_data[i].fd == OFF) ||
                (new_connect_data[i].archive_watch == OFF))
            {                                 
               new_connect_data[i].blink_flag = ON;
            }
            else
            {
               new_connect_data[i].blink_flag = OFF;
            }
            new_connect_data[i].blink = TR_BAR;
            new_connect_data[i].jobs_in_queue = msa[i].jobs_in_queue;
            new_connect_data[i].danger_no_of_jobs = msa[i].danger_no_of_jobs;
            new_connect_data[i].link_max = msa[i].danger_no_of_jobs * 2;
            new_connect_data[i].no_of_transfers = msa[i].no_of_transfers;
            new_connect_data[i].host_error_counter = msa[i].host_error_counter;
            new_connect_data[i].fc = msa[i].fc;
            new_connect_data[i].fs = msa[i].fs;
            new_connect_data[i].tr = msa[i].tr;
            new_connect_data[i].fr = msa[i].fr;
            new_connect_data[i].ec = msa[i].ec;
            new_connect_data[i].last_data_time = msa[i].last_data_time;
            new_connect_data[i].connect_status = msa[i].connect_status;
            CREATE_FC_STRING(new_connect_data[i].str_fc,
                             new_connect_data[i].fc);
            CREATE_FS_STRING(new_connect_data[i].str_fs,
                             new_connect_data[i].fs);
            CREATE_FS_STRING(new_connect_data[i].str_tr,
                             new_connect_data[i].tr);
            CREATE_JQ_STRING(new_connect_data[i].str_fr,
                             new_connect_data[i].fr);
            CREATE_EC_STRING(new_connect_data[i].str_ec,
                             new_connect_data[i].ec);
            CREATE_JQ_STRING(new_connect_data[i].str_jq,
                             new_connect_data[i].jobs_in_queue);
            CREATE_JQ_STRING(new_connect_data[i].str_at,
                             new_connect_data[i].no_of_transfers);
            CREATE_EC_STRING(new_connect_data[i].str_hec,
                             new_connect_data[i].host_error_counter);
            new_connect_data[i].average_tr = 0.0;
            new_connect_data[i].max_average_tr = 0.0;
            new_connect_data[i].no_of_hosts = msa[i].no_of_hosts;
            new_connect_data[i].max_connections = msa[i].max_connections;
            if (new_connect_data[i].max_connections < 1)
            {
               new_connect_data[i].scale[ACTIVE_TRANSFERS_BAR_NO - 1] = (double)max_bar_length;
            }
            else
            {
               new_connect_data[i].scale[ACTIVE_TRANSFERS_BAR_NO - 1] = max_bar_length / new_connect_data[i].max_connections;
            }
            if (new_connect_data[i].no_of_hosts < 1)
            {
               new_connect_data[i].scale[HOST_ERROR_BAR_NO - 1] = (double)max_bar_length;
            }
            else
            {
               new_connect_data[i].scale[HOST_ERROR_BAR_NO - 1] = max_bar_length / new_connect_data[i].no_of_hosts;
            }
            if (new_connect_data[i].no_of_transfers == 0)
            {
               new_bar_length = 0;
            }
            else if (new_connect_data[i].no_of_transfers >= new_connect_data[i].max_connections)
                 {
                    new_bar_length = max_bar_length;
                 }
                 else
                 {
                    new_bar_length = new_connect_data[i].no_of_transfers * new_connect_data[i].scale[ACTIVE_TRANSFERS_BAR_NO - 1];
                 }
            if (new_bar_length >= max_bar_length)
            {
               new_connect_data[i].bar_length[ACTIVE_TRANSFERS_BAR_NO] = max_bar_length;
               new_connect_data[i].blue_color_offset = MAX_INTENSITY;
               new_connect_data[i].green_color_offset = 0;
            }
            else
            {
               new_connect_data[i].bar_length[ACTIVE_TRANSFERS_BAR_NO] = new_bar_length;
               new_connect_data[i].blue_color_offset = new_bar_length *
                                                      step_size;
               new_connect_data[i].green_color_offset = MAX_INTENSITY -
                                                        new_connect_data[i].blue_color_offset;
            }
            new_connect_data[i].bar_length[MON_TR_BAR_NO] = 0;
            if (new_connect_data[i].host_error_counter == 0)
            {
               new_connect_data[i].bar_length[HOST_ERROR_BAR_NO] = 0;
            }
            else if (new_connect_data[i].host_error_counter >= new_connect_data[i].no_of_hosts)
                 {
                    new_connect_data[i].bar_length[HOST_ERROR_BAR_NO] = max_bar_length;
                 }
                 else
                 {
                    new_connect_data[i].bar_length[HOST_ERROR_BAR_NO] = new_connect_data[i].host_error_counter *
                                                                        new_connect_data[i].scale[HOST_ERROR_BAR_NO - 1];
                 }
            new_connect_data[i].inverse = OFF;

            /*
             * If this line has been selected in the old
             * connect_data structure, we have to make sure
             * that this host has not been deleted. If it
             * is deleted reduce the select counter!
             */
            if ((i < prev_no_of_afds) && (connect_data[i].inverse == ON))
            {
               if ((pos = check_msa_data(connect_data[i].afd_alias)) == INCORRECT)
               {
                  /* Host has been deleted. */
                  ABS_REDUCE_GLOBAL(no_selected);
               }
            }
         }
      } /* for (i = location_where_changed; i < no_of_afds; i++) */

      /*
       * Ensure that we really have checked all AFD's in old
       * structure.
       */
      if (prev_no_of_afds > no_of_afds)
      {
         while (i < prev_no_of_afds)
         {
            if (connect_data[i].inverse == ON)
            {
               if ((pos = check_msa_data(connect_data[i].afd_alias)) == INCORRECT)
               {
                  /* Host has been deleted. */
                  ABS_REDUCE_GLOBAL(no_selected);
               }
            }
            i++;
         }
      }

      /* Count number of invisible AFD's. */
      no_of_afds_invisible = no_of_afds_visible = 0;
      for (i = 0; i < no_of_afds; i++)
      {
         if ((new_connect_data[i].plus_minus == PM_CLOSE_STATE) &&
             (new_connect_data[i].rcmd != '\0'))
         {
            no_of_afds_invisible++;
         }
         else
         {
            vpl[no_of_afds_visible] = i;
            no_of_afds_visible++;
         }
      }

      if ((tmp_connect_data = realloc(connect_data, new_size)) == NULL)
      {
         int tmp_errno = errno;

         free(connect_data);
         (void)xrec(FATAL_DIALOG, "realloc() error : %s (%s %d)",
                    strerror(tmp_errno), __FILE__, __LINE__);
         return;
      }
      connect_data = tmp_connect_data;

      /* Activate the new connect_data structure. */
      (void)memcpy(&connect_data[0], &new_connect_data[0],
                   no_of_afds * sizeof(struct mon_line));

      free(new_connect_data);

      /* Resize window if necessary. */
      if ((redraw_everything = resize_mon_window()) == YES)
      {
         if (no_of_columns != 0)
         {
            location_where_changed = 0;
         }
      }

      /* When no. of AFD's have been reduced, then delete */
      /* removed AFD's from end of list.                  */
      for (i = prev_no_of_afds_visible; i > no_of_afds_visible; i--)
      {
         locate_xy(i, &x, &y);
         draw_mon_blank_line(x, y);
      }

      /* Make sure changes are drawn !!! */
      flush = YES;
   } /* if (check_msa() == YES) */

   /* Change information for each remote AFD if necessary. */
   k = 0;
   for (i = 0; i < no_of_afds; i++)
   {
      if ((connect_data[i].plus_minus == PM_OPEN_STATE) ||
          (connect_data[i].rcmd == '\0'))
      {
         locate_xy(k, &x, &y);

         draw_identifier = NO;
      }
      if (connect_data[i].connect_status != msa[i].connect_status)
      {
         connect_data[i].connect_status = msa[i].connect_status;
         draw_identifier = YES;
      }
      if ((msa[i].afd_switching != NO_SWITCHING) &&
          (connect_data[i].afd_toggle != msa[i].afd_toggle))
      {
#ifndef ONLY_KILL_AUTO_SWITCHING
         int j;
#endif

         connect_data[i].afd_toggle = msa[i].afd_toggle;
         if (connect_data[i].afd_alias_length < MAX_AFDNAME_LENGTH)
         {
            int pos;

            (void)memset(connect_data[i].afd_display_str, ' ',
                         MAX_AFDNAME_LENGTH);
            pos = sprintf(connect_data[i].afd_display_str, "%s%d",
                          connect_data[i].afd_alias,
                          connect_data[i].afd_toggle + 1);
            connect_data[i].afd_display_str[pos] = ' ';
            connect_data[i].afd_display_str[MAX_AFDNAME_LENGTH] = '\0';
            draw_identifier = YES;
         }

         /*
          * With autoswitching it is best to kill the connection,
          * otherwise if the host/connection dies the user will not
          * be able to restart a window because the rsh/ssh process
          * do not die.
          */
#ifdef ONLY_KILL_AUTO_SWITCHING
         if (msa[i].afd_switching == AUTO_SWITCHING)
         {
            int j;
#endif

            for (j = 0; j < no_of_active_process; j++)
            {
               if (apps_list[j].position == i)
               {
                  if (kill(apps_list[j].pid, SIGKILL) < 0)
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "Failed to kill() %d : %s",
                                apps_list[j].pid, strerror(errno));
                  }
               }
            }

            /*
             * Note: make_xprocess() will take care of any zombies.
             */
#ifdef ONLY_KILL_AUTO_SWITCHING
         }
#endif
      }

      if ((connect_data[i].plus_minus == PM_OPEN_STATE) ||
          (connect_data[i].rcmd == '\0'))
      {
         if (draw_identifier == YES)
         {
            if (connect_data[i].rcmd == '\0')
            {
                draw_afd_identifier(i, x - DEFAULT_FRAME_SPACE + (3 * glyph_width), y);
            }
            else
            {
                draw_afd_identifier(i, x, y);
            }
            flush = YES;
         }
      }

      if (connect_data[i].no_of_hosts != msa[i].no_of_hosts)
      {
         connect_data[i].no_of_hosts = msa[i].no_of_hosts;
         connect_data[i].scale[HOST_ERROR_BAR_NO - 1] = max_bar_length / connect_data[i].no_of_hosts;
      }
      if (connect_data[i].max_connections != msa[i].max_connections)
      {
         connect_data[i].max_connections = msa[i].max_connections;
         connect_data[i].scale[ACTIVE_TRANSFERS_BAR_NO - 1] = max_bar_length / connect_data[i].max_connections;
      }

      /*
       * PROCESS INFORMATION
       * ===================
       */
      if (connect_data[i].amg != msa[i].amg)
      {
         if (msa[i].amg == OFF)
         {
            connect_data[i].blink_flag = ON;
         }
         else if ((msa[i].amg == ON) &&
                  (connect_data[i].amg != ON) &&
                  (connect_data[i].fd != OFF) &&
                  (connect_data[i].archive_watch != OFF))
              {
                 connect_data[i].blink_flag = OFF;
              }
         connect_data[i].amg = msa[i].amg;
         if (connect_data[i].rcmd == '\0')
         {
            draw_mon_proc_led(AMG_LED, connect_data[i].amg,
                              x - DEFAULT_FRAME_SPACE + (3 * glyph_width), y);
            flush = YES;
         }
         else if (connect_data[i].plus_minus == PM_OPEN_STATE)
              {
                 draw_mon_proc_led(AMG_LED, connect_data[i].amg, x, y);
                 flush = YES;
              }
      }
      if (connect_data[i].fd != msa[i].fd)
      {
         if (msa[i].fd == OFF)
         {
            connect_data[i].blink_flag = ON;
         }
         else if ((msa[i].fd == ON) &&
                  (connect_data[i].fd != ON) &&
                  (connect_data[i].amg != OFF) &&
                  (connect_data[i].archive_watch != OFF))
              {
                 connect_data[i].blink_flag = OFF;
              }
         connect_data[i].fd = msa[i].fd;
         if (connect_data[i].rcmd == '\0')
         {
            draw_mon_proc_led(FD_LED, connect_data[i].fd,
                              x - DEFAULT_FRAME_SPACE + (3 * glyph_width), y);
            flush = YES;
         }
         else if (connect_data[i].plus_minus == PM_OPEN_STATE)
              {
                 draw_mon_proc_led(FD_LED, connect_data[i].fd, x, y);
                 flush = YES;
              }
      }
      if (connect_data[i].archive_watch != msa[i].archive_watch)
      {
         connect_data[i].archive_watch = msa[i].archive_watch;
         if (connect_data[i].rcmd == '\0')
         {
            draw_mon_proc_led(AW_LED, connect_data[i].archive_watch,
                              x - DEFAULT_FRAME_SPACE + (3 * glyph_width), y);
            flush = YES;
         }
         else if (connect_data[i].plus_minus == PM_OPEN_STATE)
              {
                 draw_mon_proc_led(AW_LED, connect_data[i].archive_watch, x, y);
                 flush = YES;
              }
      }
      if (connect_data[i].rcmd == '\0')
      {
         if (connect_data[i].blink_flag == ON)
         {
            if (connect_data[i].amg == OFF)
            {
               draw_mon_proc_led(AMG_LED, connect_data[i].blink,
                                 x - DEFAULT_FRAME_SPACE + (3 * glyph_width), y);
               flush = YES;
            }
            if (connect_data[i].fd == OFF)
            {
               draw_mon_proc_led(FD_LED, connect_data[i].blink,
                                 x - DEFAULT_FRAME_SPACE + (3 * glyph_width), y);
               flush = YES;
            }
            if (connect_data[i].blink == TR_BAR)
            {
               connect_data[i].blink = OFF;
            }
            else
            {
               connect_data[i].blink = TR_BAR;
            }
         }
      }
      else if (connect_data[i].plus_minus == PM_OPEN_STATE)
           {
              if (connect_data[i].blink_flag == ON)
              {
                 if (connect_data[i].amg == OFF)
                 {
                    draw_mon_proc_led(AMG_LED, connect_data[i].blink, x, y);
                    flush = YES;
                 }
                 if (connect_data[i].fd == OFF)
                 {
                    draw_mon_proc_led(FD_LED, connect_data[i].blink, x, y);
                    flush = YES;
                 }
                 if (connect_data[i].blink == TR_BAR)
                 {
                    connect_data[i].blink = OFF;
                 }
                 else
                 {
                    connect_data[i].blink = TR_BAR;
                 }
              }
           }

      /*
       * SYSTEM LOG INFORMATION
       * ======================
       */
      if (connect_data[i].sys_log_ec != msa[i].sys_log_ec)
      {
         connect_data[i].sys_log_ec = msa[i].sys_log_ec;
         (void)memcpy(connect_data[i].sys_log_fifo,
                      msa[i].sys_log_fifo,
                      LOG_FIFO_SIZE + 1);
         if ((connect_data[i].plus_minus == PM_OPEN_STATE) ||
             (connect_data[i].rcmd == '\0'))
         {
            draw_remote_log_status(i,
                                   connect_data[i].sys_log_ec % LOG_FIFO_SIZE,
                                   x, y);
            flush = YES;
         }
      }

      /*
       * HISTORY LOG INFORMATION
       * =======================
       */
      if (his_log_set > 0)
      {
         if (memcmp(connect_data[i].log_history[RECEIVE_HISTORY],
                    msa[i].log_history[RECEIVE_HISTORY], MAX_LOG_HISTORY) != 0)
         {
            (void)memcpy(connect_data[i].log_history[RECEIVE_HISTORY],
                         msa[i].log_history[RECEIVE_HISTORY], MAX_LOG_HISTORY);
            if ((connect_data[i].plus_minus == PM_OPEN_STATE) ||
                (connect_data[i].rcmd == '\0'))
            {
               draw_remote_history(i, RECEIVE_HISTORY, x, y);
               flush = YES;
            }
         }
         if (memcmp(connect_data[i].log_history[SYSTEM_HISTORY],
                    msa[i].log_history[SYSTEM_HISTORY], MAX_LOG_HISTORY) != 0)
         {
            (void)memcpy(connect_data[i].log_history[SYSTEM_HISTORY],
                         msa[i].log_history[SYSTEM_HISTORY], MAX_LOG_HISTORY);
            if ((connect_data[i].plus_minus == PM_OPEN_STATE) ||
                (connect_data[i].rcmd == '\0'))
            {
               draw_remote_history(i, SYSTEM_HISTORY, x, y + bar_thickness_3);
               flush = YES;
            }
         }
         if (memcmp(connect_data[i].log_history[TRANSFER_HISTORY],
                    msa[i].log_history[TRANSFER_HISTORY], MAX_LOG_HISTORY) != 0)
         {
            (void)memcpy(connect_data[i].log_history[TRANSFER_HISTORY],
                         msa[i].log_history[TRANSFER_HISTORY], MAX_LOG_HISTORY);
            if ((connect_data[i].plus_minus == PM_OPEN_STATE) ||
                (connect_data[i].rcmd == '\0'))
            {
               draw_remote_history(i, TRANSFER_HISTORY, x,
                                   y + bar_thickness_3 + bar_thickness_3);
               flush = YES;
            }
         }
      }

      /*
       * CHARACTER INFORMATION
       * =====================
       *
       * If in character mode see if any change took place,
       * if so, redraw only those characters.
       */
      if (line_style != BARS_ONLY)
      {
         /*
          * Number of files to be send.
          */
         if (connect_data[i].fc != msa[i].fc)
         {
            connect_data[i].fc = msa[i].fc;
            CREATE_FC_STRING(connect_data[i].str_fc, connect_data[i].fc);

            if ((i < location_where_changed) &&
                ((connect_data[i].plus_minus == PM_OPEN_STATE) ||
                 (connect_data[i].rcmd == '\0')))
            {
               draw_mon_chars(i, FILES_TO_BE_SEND, x, y);
               flush = YES;
            }
         }

         /*
          * File size to be send.
          */
         if (connect_data[i].fs != msa[i].fs)
         {
            char tmp_string[5];

            connect_data[i].fs = msa[i].fs;
            CREATE_FS_STRING(tmp_string, connect_data[i].fs);

            if ((tmp_string[2] != connect_data[i].str_fs[2]) ||
                (tmp_string[1] != connect_data[i].str_fs[1]) ||
                (tmp_string[0] != connect_data[i].str_fs[0]) ||
                (tmp_string[3] != connect_data[i].str_fs[3]))
            {
               connect_data[i].str_fs[0] = tmp_string[0];
               connect_data[i].str_fs[1] = tmp_string[1];
               connect_data[i].str_fs[2] = tmp_string[2];
               connect_data[i].str_fs[3] = tmp_string[3];

               if ((i < location_where_changed) &&
                   ((connect_data[i].plus_minus == PM_OPEN_STATE) ||
                    (connect_data[i].rcmd == '\0')))
               {
                  draw_mon_chars(i, FILE_SIZE_TO_BE_SEND,
                                 x + (5 * glyph_width), y);
                  flush = YES;
               }
            }
         }

         /*
          * Transfer rate.
          */
         if (connect_data[i].tr != msa[i].tr)
         {
            char tmp_string[5];

            connect_data[i].tr = msa[i].tr;
            CREATE_FS_STRING(tmp_string, connect_data[i].tr);

            if ((tmp_string[2] != connect_data[i].str_tr[2]) ||
                (tmp_string[1] != connect_data[i].str_tr[1]) ||
                (tmp_string[0] != connect_data[i].str_tr[0]) ||
                (tmp_string[3] != connect_data[i].str_tr[3]))
            {
               connect_data[i].str_tr[0] = tmp_string[0];
               connect_data[i].str_tr[1] = tmp_string[1];
               connect_data[i].str_tr[2] = tmp_string[2];
               connect_data[i].str_tr[3] = tmp_string[3];

               if ((i < location_where_changed) &&
                   ((connect_data[i].plus_minus == PM_OPEN_STATE) ||
                    (connect_data[i].rcmd == '\0')))
               {
                  draw_mon_chars(i, AVERAGE_TRANSFER_RATE,
                                 x + (10 * glyph_width), y);
                  flush = YES;
               }
            }
         }

         /*
          * Connection rate.
          */
         if (connect_data[i].fr != msa[i].fr)
         {
            connect_data[i].fr = msa[i].fr;
            CREATE_JQ_STRING(connect_data[i].str_fr, connect_data[i].fr);

            if ((i < location_where_changed) &&
                ((connect_data[i].plus_minus == PM_OPEN_STATE) ||
                 (connect_data[i].rcmd == '\0')))
            {
               draw_mon_chars(i, AVERAGE_CONNECTION_RATE,
                              x + (15 * glyph_width), y);
               flush = YES;
            }
         }

         /*
          * Jobs in queue.
          */
         if (connect_data[i].jobs_in_queue != msa[i].jobs_in_queue)
         {
            connect_data[i].jobs_in_queue = msa[i].jobs_in_queue;
            CREATE_JQ_STRING(connect_data[i].str_jq,
                             connect_data[i].jobs_in_queue);

            if ((i < location_where_changed) &&
                ((connect_data[i].plus_minus == PM_OPEN_STATE) ||
                 (connect_data[i].rcmd == '\0')))
            {
               draw_mon_chars(i, JOBS_IN_QUEUE,
                              x + (19 * glyph_width), y);
               flush = YES;
            }
         }

         /*
          * Danger number of Jobs.
          */
         if (connect_data[i].danger_no_of_jobs != msa[i].danger_no_of_jobs)
         {
            connect_data[i].danger_no_of_jobs = msa[i].danger_no_of_jobs;
            connect_data[i].link_max = connect_data[i].danger_no_of_jobs * 2;

            if ((i < location_where_changed) &&
                ((connect_data[i].plus_minus == PM_OPEN_STATE) ||
                 (connect_data[i].rcmd == '\0')))
            {
               draw_mon_chars(i, JOBS_IN_QUEUE,
                              x + (19 * glyph_width), y);
               flush = YES;
            }
         }

         /*
          * Active transfers.
          */
         if (connect_data[i].no_of_transfers != msa[i].no_of_transfers)
         {
            connect_data[i].no_of_transfers = msa[i].no_of_transfers;
            CREATE_JQ_STRING(connect_data[i].str_at, msa[i].no_of_transfers);

            if ((i < location_where_changed) &&
                ((connect_data[i].plus_minus == PM_OPEN_STATE) ||
                 (connect_data[i].rcmd == '\0')))
            {
               draw_mon_chars(i, ACTIVE_TRANSFERS,
                              x + (23 * glyph_width), y);
               flush = YES;
            }
         }

         /*
          * Error counter.
          */
         if (connect_data[i].ec != msa[i].ec)
         {
            connect_data[i].ec = msa[i].ec;
            CREATE_EC_STRING(connect_data[i].str_ec, connect_data[i].ec);

            if ((i < location_where_changed) &&
                ((connect_data[i].plus_minus == PM_OPEN_STATE) ||
                 (connect_data[i].rcmd == '\0')))
            {
               draw_mon_chars(i, TOTAL_ERROR_COUNTER,
                              x + (27 * glyph_width), y);
               flush = YES;
            }
         }

         /*
          * Error hosts.
          */
         if (connect_data[i].host_error_counter != msa[i].host_error_counter)
         {
            /*
             * If line_style is CHARACTERS and BARS don't update
             * the connect_data structure. Otherwise when we draw the bar
             * we will not notice any change. There we will then update
             * the structure member host_error_counter.
             */
            if (line_style == CHARACTERS_ONLY)
            {
               connect_data[i].host_error_counter = msa[i].host_error_counter;
            }
            CREATE_EC_STRING(connect_data[i].str_hec, msa[i].host_error_counter);

            if ((i < location_where_changed) &&
                ((connect_data[i].plus_minus == PM_OPEN_STATE) ||
                 (connect_data[i].rcmd == '\0')))
            {
               draw_mon_chars(i, ERROR_HOSTS,
                              x + (30 * glyph_width), y);
               flush = YES;
            }
         }
      }
      else
      {
         /*
          * Transfer rate.
          */
         if (connect_data[i].tr != msa[i].tr)
         {
            connect_data[i].tr = msa[i].tr;
         }
      }

      /*
       * BAR INFORMATION
       * ===============
       */
      if (line_style != CHARACTERS_ONLY)
      {
         /*
          * Transfer Rate Bar
          */
         /* Calculate transfer rate (arithmetischer Mittelwert). */
         connect_data[i].average_tr = (connect_data[i].average_tr +
                                      connect_data[i].tr) / 2.0;
         if (connect_data[i].average_tr > connect_data[i].max_average_tr)
         {
            connect_data[i].max_average_tr = connect_data[i].average_tr;
         }

         if (connect_data[i].average_tr > 1.0)
         {
            if (connect_data[i].max_average_tr < 2.0)
            {
               new_bar_length = (log10(connect_data[i].average_tr) *
                                 max_bar_length /
                                 log10((double) 2.0));
            }
            else
            {
               new_bar_length = (log10(connect_data[i].average_tr) *
                                 max_bar_length /
                                 log10(connect_data[i].max_average_tr));
            }
         }
         else
         {
            new_bar_length = 0;
         }

         if ((connect_data[i].bar_length[MON_TR_BAR_NO] != new_bar_length) &&
             (new_bar_length < max_bar_length))
         {
            old_bar_length = connect_data[i].bar_length[MON_TR_BAR_NO];
            connect_data[i].bar_length[MON_TR_BAR_NO] = new_bar_length;

            if ((i < location_where_changed) &&
                ((connect_data[i].plus_minus == PM_OPEN_STATE) ||
                 (connect_data[i].rcmd == '\0')))
            {
               if (old_bar_length < new_bar_length)
               {
                  draw_mon_bar(i, 1, MON_TR_BAR_NO, x, y);
               }
               else
               {
                  draw_mon_bar(i, -1, MON_TR_BAR_NO, x, y);
               }

               if (flush != YES)
               {
                  flush = YUP;
               }
            }
         }
         else if ((new_bar_length >= max_bar_length) &&
                  (connect_data[i].bar_length[MON_TR_BAR_NO] < max_bar_length))
              {
                 connect_data[i].bar_length[MON_TR_BAR_NO] = max_bar_length;
                 if ((i < location_where_changed) &&
                     ((connect_data[i].plus_minus == PM_OPEN_STATE) ||
                      (connect_data[i].rcmd == '\0')))
                 {
                    draw_mon_bar(i, 1, MON_TR_BAR_NO, x, y);

                    if (flush != YES)
                    {
                       flush = YUP;
                    }
                 }
              }

         /*
          * Active Transfers Bar
          */
         if (connect_data[i].no_of_transfers != msa[i].no_of_transfers)
         {
            connect_data[i].no_of_transfers = msa[i].no_of_transfers;
            if (connect_data[i].no_of_transfers == 0)
            {
               new_bar_length = 0;
            }
            else if (connect_data[i].no_of_transfers >= msa[i].max_connections)
                 {
                    new_bar_length = max_bar_length;
                 }
                 else
                 {
                    new_bar_length = connect_data[i].no_of_transfers * connect_data[i].scale[ACTIVE_TRANSFERS_BAR_NO - 1];
                 }
            if (connect_data[i].bar_length[ACTIVE_TRANSFERS_BAR_NO] != new_bar_length)
            {
               connect_data[i].blue_color_offset = new_bar_length * step_size;
               connect_data[i].green_color_offset = MAX_INTENSITY - connect_data[i].blue_color_offset;
               connect_data[i].bar_length[ACTIVE_TRANSFERS_BAR_NO] = new_bar_length;

               if ((i < location_where_changed) &&
                   ((connect_data[i].plus_minus == PM_OPEN_STATE) ||
                    (connect_data[i].rcmd == '\0')))
               {
                  int delta;

                  if (connect_data[i].bar_length[ACTIVE_TRANSFERS_BAR_NO] < new_bar_length)
                  {
                     delta = 1;
                  }
                  else
                  {
                     delta = -1;
                  }
                  draw_mon_bar(i, delta, ACTIVE_TRANSFERS_BAR_NO, x, y);
                  flush = YES;
               }
            }
         }

         /*
          * Host Error Bar
          */
         if (connect_data[i].host_error_counter != msa[i].host_error_counter)
         {
            connect_data[i].host_error_counter = msa[i].host_error_counter;
            if (connect_data[i].host_error_counter >= connect_data[i].no_of_hosts)
            {
               new_bar_length = max_bar_length;
            }
            else
            {
               new_bar_length = connect_data[i].host_error_counter *
                                connect_data[i].scale[HOST_ERROR_BAR_NO - 1];
            }
            if (connect_data[i].bar_length[HOST_ERROR_BAR_NO] != new_bar_length)
            {
               connect_data[i].bar_length[HOST_ERROR_BAR_NO] = new_bar_length;
               if ((i < location_where_changed) &&
                   ((connect_data[i].plus_minus == PM_OPEN_STATE) ||
                    (connect_data[i].rcmd == '\0')))
               {
                  if (connect_data[i].bar_length[HOST_ERROR_BAR_NO] < new_bar_length)
                  {
                     draw_mon_bar(i, 1, HOST_ERROR_BAR_NO, x, y);
                  }
                  else
                  {
                     draw_mon_bar(i, -1, HOST_ERROR_BAR_NO, x, y);
                  }
                  flush = YES;
               }
            }
         }
      }

      if ((connect_data[i].plus_minus == PM_OPEN_STATE) ||
          (connect_data[i].rcmd == '\0'))
      {
         /* Redraw the line. */
         if (i >= location_where_changed)
         {
#ifdef _DEBUG
            (void)fprintf(stderr, "count_channels: i = %d\n", i);
#endif
            flush = YES;
            draw_mon_line_status(i, 1, x, y);
         }
         k++;
      }
   }

   if (redraw_everything == YES)
   {
      calc_mon_but_coord(window_width);
      redraw_all();
      flush = YES;
   }

   /* Make sure all changes are shown. */
   if ((flush == YES) || (flush == YUP))
   {
      XFlush(display);

      if (flush != YUP)
      {
         redraw_time_line = MIN_REDRAW_TIME;
      }
   }
   else
   {
      if (redraw_time_line < MAX_REDRAW_TIME)
      {
         redraw_time_line += REDRAW_STEP_TIME;
      }
#ifdef _DEBUG
      (void)fprintf(stderr, "count_channels: Redraw time = %d\n", redraw_time_line);
#endif
   }

   /* Redraw every redraw_time_line ms. */
   (void)XtAppAddTimeOut(app, redraw_time_line,
                         (XtTimerCallbackProc)check_afd_status, w);
 
   return;
}


/*+++++++++++++++++++++++++++ check_msa_data() ++++++++++++++++++++++++++*/
static int
check_msa_data(char *afd_alias)
{
   register int i;

   for (i = 0; i < no_of_afds; i++)
   {
      if (my_strcmp(msa[i].afd_alias, afd_alias) == 0)
      {
         return(i);
      }
   }

   return(INCORRECT);
}


/*++++++++++++++++++++++++++ check_disp_data() ++++++++++++++++++++++++++*/
static int
check_disp_data(char *afd_alias, int prev_no_of_afds)
{
   register int i;

   for (i = 0; i < prev_no_of_afds; i++)
   {
      if (my_strcmp(connect_data[i].afd_alias, afd_alias) == 0)
      {
         return(i);
      }
   }

   return(INCORRECT);
}
