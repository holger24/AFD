/*
 *  draw_mon_line.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2016 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   draw_mon_line - draws one complete line of the mon_ctrl window
 **
 ** SYNOPSIS
 **   void draw_mon_bar(int pos, signed char delta, char bar_no, int x, int y)
 **   void draw_mon_blank_line(int pos)
 **   void draw_mon_button_line(void)
 **   void draw_mon_chars(int pos, char type, int x, int y)
 **   void draw_mon_label_line(void)
 **   void draw_mon_log_status(int, int)
 **   void draw_mon_line_status(int pos, signed char delta)
 **   void draw_mon_blank_line(int pos)
 **   void draw_mon_proc_led(int led_no, signed char led_status, int x, int y)
 **   void draw_remote_log_status(int pos, int si_pos, int x, int y)
 **   void draw_clock(time_t current_time)
 **
 ** DESCRIPTION
 **   The function draw_mon_label_line() draws the label which is just
 **   under the menu bar. It draws the following labels: host, fc,
 **   fs, tr and ec when character style is set.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   05.09.1998 H.Kiehl Created
 **   08.06.2005 H.Kiehl Added button line at bottom.
 **   24.02.2008 H.Kiehl For drawing areas, draw to offline pixmap as well.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* strlen()                        */
#include <time.h>                     /* time(), localtime(), strftime() */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include "mon_ctrl.h"

extern Display               *display;
extern Pixmap                button_pixmap,
                             label_pixmap,
                             line_pixmap;
extern Window                button_window,
                             label_window,
                             line_window;
extern GC                    letter_gc,
                             normal_letter_gc,
                             locked_letter_gc,
                             color_letter_gc,
                             default_bg_gc,
                             button_bg_gc,
                             normal_bg_gc,
                             locked_bg_gc,
                             label_bg_gc,
                             red_color_letter_gc,
                             red_error_letter_gc,
                             tr_bar_gc,
                             color_gc,
                             black_line_gc,
                             white_line_gc,
                             led_gc;
extern Colormap              default_cmap;
extern char                  line_style;
extern unsigned long         color_pool[];
extern float                 max_bar_length;
extern int                   line_length,
                             line_height,
                             bar_thickness_3,
                             his_log_set,
                             x_center_log_status,
                             x_center_mon_log,
                             x_center_sys_log,
                             x_offset_log_status,
                             x_offset_log_history,
                             x_offset_led,
                             x_offset_bars,
                             x_offset_characters,
                             x_offset_mon_log,
                             x_offset_stat_leds,
                             x_offset_sys_log,
                             y_center_log,
                             y_offset_led,
                             log_angle,
                             no_of_columns,
                             window_width;
extern unsigned int          glyph_height,
                             glyph_width,
                             text_offset;
extern struct coord          button_coord[2][LOG_FIFO_SIZE],
                             coord[LOG_FIFO_SIZE];
extern struct afd_mon_status prev_afd_mon_status;
extern struct mon_line       *connect_data;

#ifdef _DEBUG
static unsigned int          counter = 0;
#endif


/*########################## draw_label_line() ##########################*/
void
draw_label_line(void)
{
   int i,
       x = 0;

   for (i = 0; i < no_of_columns; i++)
   {
      /* First draw the background in the appropriate color. */
      XFillRectangle(display, label_window, label_bg_gc,
                     x + 2,
                     2,
                     x + line_length - 2,
                     line_height - 4);
      XFillRectangle(display, label_pixmap, label_bg_gc,
                     x + 2,
                     2,
                     x + line_length - 2,
                     line_height - 4);

      /* Now draw left, top and bottom end for button style. */
      XDrawLine(display, label_window, black_line_gc,
                x,
                0,
                x,
                line_height);
      XDrawLine(display, label_window, white_line_gc,
                x + 1,
                1,
                x + 1,
                line_height - 3);
      XDrawLine(display, label_window, black_line_gc,
                x,
                0,
                x + line_length,
                0);
      XDrawLine(display, label_window, white_line_gc,
                x + 1,
                1,
                x + line_length,
                1);
      XDrawLine(display, label_window, black_line_gc,
                x,
                line_height - 2,
                x + line_length,
                line_height - 2);
      XDrawLine(display, label_window, white_line_gc,
                x,
                line_height - 1,
                x + line_length,
                line_height - 1);
      XDrawLine(display, label_pixmap, black_line_gc,
                x,
                0,
                x,
                line_height);
      XDrawLine(display, label_pixmap, white_line_gc,
                x + 1,
                1,
                x + 1,
                line_height - 3);
      XDrawLine(display, label_pixmap, black_line_gc,
                x,
                0,
                x + line_length,
                0);
      XDrawLine(display, label_pixmap, white_line_gc,
                x + 1,
                1,
                x + line_length,
                1);
      XDrawLine(display, label_pixmap, black_line_gc,
                x,
                line_height - 2,
                x + line_length,
                line_height - 2);
      XDrawLine(display, label_pixmap, white_line_gc,
                x,
                line_height - 1,
                x + line_length,
                line_height - 1);

      /* Draw string "   AFD". */
      XDrawString(display, label_window, letter_gc,
                  x + DEFAULT_FRAME_SPACE,
                  text_offset + SPACE_ABOVE_LINE,
                  "   AFD",
                  6);
      XDrawString(display, label_pixmap, letter_gc,
                  x + DEFAULT_FRAME_SPACE,
                  text_offset + SPACE_ABOVE_LINE,
                  "   AFD",
                  6);

      /* See if we need to extend heading for "Character" display. */
      if (line_style != BARS_ONLY)
      {
         /*
          * Draw string " fc   fs   tr   fr  jq  at ec eh"
          *     fc - file counter
          *     fs - file size
          *     tr - transfer rate
          *     fr - file rate
          *     jq - jobs in queue
          *     at - active transfers
          *     ec - error counter
          *     eh - error hosts
          */
         XDrawString(display, label_window, letter_gc,
                     x + x_offset_characters,
                     text_offset + SPACE_ABOVE_LINE,
                     " fc   fs   tr   fr  jq  at ec eh",
                     32);
         XDrawString(display, label_pixmap, letter_gc,
                     x + x_offset_characters,
                     text_offset + SPACE_ABOVE_LINE,
                     " fc   fs   tr   fr  jq  at ec eh",
                     32);
      }

      x += line_length;
   }

   /* Draw right end for button style. */
   XDrawLine(display, label_window, black_line_gc,
             x - 2,
             0,
             x - 2,
             line_height - 2);
   XDrawLine(display, label_window, white_line_gc,
             x - 1,
             1,
             x - 1,
             line_height - 2);
   XDrawLine(display, label_pixmap, black_line_gc,
             x - 2,
             0,
             x - 2,
             line_height - 2);
   XDrawLine(display, label_pixmap, white_line_gc,
             x - 1,
             1,
             x - 1,
             line_height - 2);

   return;
}


/*######################### draw_line_status() ##########################*/
void
draw_line_status(int pos, signed char delta)
{
   int x = 0,
       y = 0;
   GC  tmp_gc;

   /* First locate position of x and y. */
   locate_xy(pos, &x, &y);

#ifdef _DEBUG
   (void)printf("Drawing line %d %d  x = %d  y = %d\n",
                pos, counter++, x, y);
#endif

   if ((connect_data[pos].inverse > OFF) && (delta >= 0))
   {
      if (connect_data[pos].inverse == ON)
      {
         tmp_gc = normal_bg_gc;
      }
      else
      {
         tmp_gc = locked_bg_gc;
      }
   }
   else
   {
      tmp_gc = default_bg_gc;
   }
   XFillRectangle(display, line_window, tmp_gc, x, y, line_length, line_height);
   XFillRectangle(display, line_pixmap, tmp_gc, x, y, line_length, line_height);

   if (connect_data[pos].rcmd == '\0')
   {
      draw_plus_minus(pos, x, y);

      draw_afd_identifier(pos, x + (4 * glyph_width), y);
   }
   else
   {
      /* Write destination identifier to screen. */
      draw_afd_identifier(pos, x, y);

      /* Draw status LED's of remote AFD. */
      draw_mon_proc_led(AMG_LED, connect_data[pos].amg, x, y);
      draw_mon_proc_led(FD_LED, connect_data[pos].fd, x, y);
      draw_mon_proc_led(AW_LED, connect_data[pos].archive_watch, x, y);

      draw_remote_log_status(pos, connect_data[pos].sys_log_ec % LOG_FIFO_SIZE, x, y);

      if (his_log_set > 0)
      {
         draw_remote_history(pos, RECEIVE_HISTORY, x, y);
         draw_remote_history(pos, SYSTEM_HISTORY, x, y + bar_thickness_3);
         draw_remote_history(pos, TRANSFER_HISTORY, x,
                             y + bar_thickness_3 + bar_thickness_3);
      }

      /* Print information for number of files to be send (nf), */
      /* total file size (tfs), transfer rate (tr) and error    */
      /* counter (ec).                                          */
      if (line_style != BARS_ONLY)
      {
         draw_mon_chars(pos, FILES_TO_BE_SEND, x, y);
         draw_mon_chars(pos, FILE_SIZE_TO_BE_SEND, x + (5 * glyph_width), y);
         draw_mon_chars(pos, AVERAGE_TRANSFER_RATE, x + (10 * glyph_width), y);
         draw_mon_chars(pos, AVERAGE_CONNECTION_RATE, x + (15 * glyph_width), y);
         draw_mon_chars(pos, JOBS_IN_QUEUE, x + (19 * glyph_width), y);
         draw_mon_chars(pos, ACTIVE_TRANSFERS, x + (23 * glyph_width), y);
         draw_mon_chars(pos, TOTAL_ERROR_COUNTER, x + (27 * glyph_width), y);
         draw_mon_chars(pos, ERROR_HOSTS, x + (30 * glyph_width), y);
      }

      /* Draw bars, indicating graphically how many errors have */
      /* occurred, total file size still to do and the transfer */
      /* rate.                                                  */
      if (line_style != CHARACTERS_ONLY)
      {
         /* Draw bars. */
         draw_mon_bar(pos, delta, MON_TR_BAR_NO, x, y);
         draw_mon_bar(pos, delta, ACTIVE_TRANSFERS_BAR_NO, x, y);
         draw_mon_bar(pos, delta, HOST_ERROR_BAR_NO, x, y);

         /* Show beginning and end of bars. */
         if (connect_data[pos].inverse > OFF)
         {
            tmp_gc = white_line_gc;
         }
         else
         {
            tmp_gc = black_line_gc;
         }
         XDrawLine(display, line_window, black_line_gc,
                   x + x_offset_bars - 1,
                   y + SPACE_ABOVE_LINE,
                   x + x_offset_bars - 1,
                   y + glyph_height);
         XDrawLine(display, line_window, black_line_gc,
                   x + x_offset_bars + (int)max_bar_length,
                   y + SPACE_ABOVE_LINE,
                   x + x_offset_bars + (int)max_bar_length, y + glyph_height);
         XDrawLine(display, line_pixmap, black_line_gc,
                   x + x_offset_bars - 1,
                   y + SPACE_ABOVE_LINE,
                   x + x_offset_bars - 1,
                   y + glyph_height);
         XDrawLine(display, line_pixmap, black_line_gc,
                   x + x_offset_bars + (int)max_bar_length,
                   y + SPACE_ABOVE_LINE,
                   x + x_offset_bars + (int)max_bar_length, y + glyph_height);
      }
   }

   return;
}


/*######################## draw_mon_blank_line() ########################*/
void
draw_mon_blank_line(int pos)
{
   int x,
       y;

   locate_xy(pos, &x, &y);

   XFillRectangle(display, line_window, default_bg_gc, x, y,
                  line_length, line_height);
   XFillRectangle(display, line_pixmap, default_bg_gc, x, y,
                  line_length, line_height);

   return;
}


/*######################## draw_mon_button_line() #######################*/
void
draw_mon_button_line(void)
{
   XFillRectangle(display, button_window, button_bg_gc, 0, 0, window_width,
                  line_height + 1);
   XFillRectangle(display, button_pixmap, button_bg_gc, 0, 0, window_width,
                  line_height + 1);

   /* Draw status LED for AFDMON. */
   draw_mon_proc_led(AFDMON_LED, prev_afd_mon_status.afd_mon, -1, -1);

   /* Draw log status indicators. */
   draw_mon_log_status(MON_SYS_LOG_INDICATOR,
                       prev_afd_mon_status.mon_sys_log_ec % LOG_FIFO_SIZE);
   draw_mon_log_status(MON_LOG_INDICATOR,
                       prev_afd_mon_status.mon_log_ec % LOG_FIFO_SIZE);

   /* Draw clock. */
   draw_clock(time(NULL));

   return;
}


/*++++++++++++++++++++++++++ draw_plus_minus() ++++++++++++++++++++++++++*/
void
draw_plus_minus(int pos, int x, int y)
{
   char      plus_minus_str[] = { '[', '-', ']', '\0' };
   XGCValues gc_values;

   gc_values.foreground = color_pool[FG];
   gc_values.background = color_pool[DEFAULT_BG];
   XChangeGC(display, color_letter_gc,
             GCForeground | GCBackground, &gc_values);

   if (connect_data[pos].plus_minus == PM_CLOSE_STATE)
   {
      plus_minus_str[1] = '+';
   }

   XDrawImageString(display, line_window, color_letter_gc,
                    DEFAULT_FRAME_SPACE + x,
                    y + text_offset + SPACE_ABOVE_LINE,
                    plus_minus_str, 3);
   XDrawImageString(display, line_pixmap, color_letter_gc,
                    DEFAULT_FRAME_SPACE + x,
                    y + text_offset + SPACE_ABOVE_LINE,
                    plus_minus_str, 3);

   return;
}


/*++++++++++++++++++++++++ draw_afd_identifier() ++++++++++++++++++++++++*/
void
draw_afd_identifier(int pos, int x, int y)
{
   XGCValues gc_values;

   /* Change color of letters when background color is to dark. */
   if ((connect_data[pos].connect_status == CONNECTING) ||
       (connect_data[pos].connect_status == NOT_WORKING2))
   {
      gc_values.foreground = color_pool[WHITE];
   }
   else
   {
      gc_values.foreground = color_pool[FG];
   }
   gc_values.background = color_pool[(int)connect_data[pos].connect_status];
   XChangeGC(display, color_letter_gc,
             GCForeground | GCBackground, &gc_values);

   XDrawImageString(display, line_window, color_letter_gc,
                    DEFAULT_FRAME_SPACE + x,
                    y + text_offset + SPACE_ABOVE_LINE,
                    connect_data[pos].afd_display_str,
                    MAX_AFDNAME_LENGTH);
   XDrawImageString(display, line_pixmap, color_letter_gc,
                    DEFAULT_FRAME_SPACE + x,
                    y + text_offset + SPACE_ABOVE_LINE,
                    connect_data[pos].afd_display_str,
                    MAX_AFDNAME_LENGTH);

   return;
}


/*++++++++++++++++++++++++ draw_mon_proc_led() ++++++++++++++++++++++++++*/
void
draw_mon_proc_led(int led_no, signed char led_status, int x, int y)
{
   int       x_offset,
             y_offset;
   Window    current_window;
   Pixmap    current_pixmap;
   XGCValues gc_values;

   if (led_no == AFDMON_LED)
   {
      x_offset = x_offset_stat_leds + glyph_width + PROC_LED_SPACING;
      y_offset = SPACE_ABOVE_LINE + y_offset_led;
      current_window = button_window;
      current_pixmap = button_pixmap;
   }
   else
   {
      x_offset = x + x_offset_led + (led_no * (glyph_width + PROC_LED_SPACING));
      y_offset = y + SPACE_ABOVE_LINE + y_offset_led;
      current_window = line_window;
      current_pixmap = line_pixmap;
   }

   if (led_status == ON)
   {
      XFillArc(display, current_window, led_gc, x_offset, y_offset,
               glyph_width, glyph_width, 0, 23040);
      XFillArc(display, current_pixmap, led_gc, x_offset, y_offset,
               glyph_width, glyph_width, 0, 23040);
   }                                               
   else
   {
      if (led_status == OFF)
      {
         gc_values.foreground = color_pool[NOT_WORKING2];
         XChangeGC(display, color_gc, GCForeground, &gc_values);
      }
      else if (led_status == STOPPED)
           {
              gc_values.foreground = color_pool[STOP_TRANSFER];
              XChangeGC(display, color_gc, GCForeground, &gc_values);
           }
      else if (led_status == SHUTDOWN)
           {
              gc_values.foreground = color_pool[CLOSING_CONNECTION];
              XChangeGC(display, color_gc, GCForeground, &gc_values);
           }
           else
           {
              gc_values.foreground = color_pool[(int)led_status];
              XChangeGC(display, color_gc, GCForeground, &gc_values);
           }
      XFillArc(display, current_window, color_gc, x_offset, y_offset,
               glyph_width, glyph_width, 0, 23040);
      XFillArc(display, current_pixmap, color_gc, x_offset, y_offset,
               glyph_width, glyph_width, 0, 23040);
   }

   /* Draw LED frame. */
   XDrawArc(display, current_window, black_line_gc, x_offset, y_offset,
            glyph_width, glyph_width, 0, 23040);
   XDrawArc(display, current_pixmap, black_line_gc, x_offset, y_offset,
            glyph_width, glyph_width, 0, 23040);

   return;
}


/*+++++++++++++++++++++ draw_remote_log_status() ++++++++++++++++++++++++*/
void
draw_remote_log_status(int pos, int si_pos, int x, int y)
{
   int       i,
             prev_si_pos;
   XGCValues gc_values;

   if (si_pos == 0)
   {
      prev_si_pos = LOG_FIFO_SIZE - 1;
   }
   else
   {
      prev_si_pos = si_pos - 1;
   }
   for (i = 0; i < LOG_FIFO_SIZE; i++)
   {
      gc_values.foreground = color_pool[(int)connect_data[pos].sys_log_fifo[i]];
      XChangeGC(display, color_gc, GCForeground, &gc_values);
      XFillArc(display, line_window, color_gc,
               x + x_offset_log_status,
               y + SPACE_ABOVE_LINE,
               glyph_height, glyph_height,
               ((i * log_angle) * 64),
               (log_angle * 64));
      XFillArc(display, line_pixmap, color_gc,
               x + x_offset_log_status,
               y + SPACE_ABOVE_LINE,
               glyph_height, glyph_height,
               ((i * log_angle) * 64),
               (log_angle * 64));
   }
   if ((connect_data[pos].sys_log_fifo[si_pos] == BLACK) ||
       (connect_data[pos].sys_log_fifo[prev_si_pos] == BLACK))
   {
      XDrawLine(display, line_window, white_line_gc,
                x + x_center_log_status,
                y + y_center_log,
                x + coord[si_pos].x,
                y + coord[si_pos].y);
      XDrawLine(display, line_pixmap, white_line_gc,
                x + x_center_log_status,
                y + y_center_log,
                x + coord[si_pos].x,
                y + coord[si_pos].y);
   }
   else
   {
      XDrawLine(display, line_window, black_line_gc,
                x + x_center_log_status,
                y + y_center_log,
                x + coord[si_pos].x,
                y + coord[si_pos].y);
      XDrawLine(display, line_pixmap, black_line_gc,
                x + x_center_log_status,
                y + y_center_log,
                x + coord[si_pos].x,
                y + coord[si_pos].y);
   }

   return;
}


/*++++++++++++++++++++++ draw_mon_log_status() ++++++++++++++++++++++++++*/
void
draw_mon_log_status(int log_typ, int si_pos)
{
   int       i,
             prev_si_pos;
   XGCValues gc_values;

   if (si_pos == 0)
   {
      prev_si_pos = LOG_FIFO_SIZE - 1;
   }
   else
   {
      prev_si_pos = si_pos - 1;
   }
   if (log_typ == MON_SYS_LOG_INDICATOR)
   {
      for (i = 0; i < LOG_FIFO_SIZE; i++)
      {
         gc_values.foreground = color_pool[(int)prev_afd_mon_status.mon_sys_log_fifo[i]];
         XChangeGC(display, color_gc, GCForeground, &gc_values);
         XFillArc(display, button_window, color_gc,
                  x_offset_sys_log, SPACE_ABOVE_LINE,
                  glyph_height, glyph_height,
                  ((i * log_angle) * 64),
                  (log_angle * 64));
         XFillArc(display, button_pixmap, color_gc,
                  x_offset_sys_log, SPACE_ABOVE_LINE,
                  glyph_height, glyph_height,
                  ((i * log_angle) * 64),
                  (log_angle * 64));
      }
      if ((prev_afd_mon_status.mon_sys_log_fifo[si_pos] == BLACK) ||
          (prev_afd_mon_status.mon_sys_log_fifo[prev_si_pos] == BLACK))
      {
         XDrawLine(display, button_window, white_line_gc,
                   x_center_sys_log, y_center_log,
                   button_coord[log_typ][si_pos].x,
                   button_coord[log_typ][si_pos].y);
         XDrawLine(display, button_pixmap, white_line_gc,
                   x_center_sys_log, y_center_log,
                   button_coord[log_typ][si_pos].x,
                   button_coord[log_typ][si_pos].y);
      }
      else
      {
         XDrawLine(display, button_window, black_line_gc,
                   x_center_sys_log, y_center_log,
                   button_coord[log_typ][si_pos].x,
                   button_coord[log_typ][si_pos].y);
         XDrawLine(display, button_pixmap, black_line_gc,
                   x_center_sys_log, y_center_log,
                   button_coord[log_typ][si_pos].x,
                   button_coord[log_typ][si_pos].y);
      }
   }
   else
   {
      for (i = 0; i < LOG_FIFO_SIZE; i++)
      {
         gc_values.foreground = color_pool[(int)prev_afd_mon_status.mon_log_fifo[i]];
         XChangeGC(display, color_gc, GCForeground, &gc_values);
         XFillArc(display, button_window, color_gc,
                  x_offset_mon_log, SPACE_ABOVE_LINE,
                  glyph_height, glyph_height,
                  ((i * log_angle) * 64),
                  (log_angle * 64));
         XFillArc(display, button_pixmap, color_gc,
                  x_offset_mon_log, SPACE_ABOVE_LINE,
                  glyph_height, glyph_height,
                  ((i * log_angle) * 64),
                  (log_angle * 64));
      }
      if ((prev_afd_mon_status.mon_log_fifo[si_pos] == BLACK) ||
          (prev_afd_mon_status.mon_log_fifo[prev_si_pos] == BLACK))
      {
         XDrawLine(display, button_window, white_line_gc,
                   x_center_mon_log, y_center_log,
                   button_coord[log_typ][si_pos].x,
                   button_coord[log_typ][si_pos].y);
         XDrawLine(display, button_pixmap, white_line_gc,
                   x_center_mon_log, y_center_log,
                   button_coord[log_typ][si_pos].x,
                   button_coord[log_typ][si_pos].y);
      }
      else
      {
         XDrawLine(display, button_window, black_line_gc,
                   x_center_mon_log, y_center_log,
                   button_coord[log_typ][si_pos].x,
                   button_coord[log_typ][si_pos].y);
         XDrawLine(display, button_pixmap, black_line_gc,
                   x_center_mon_log, y_center_log,
                   button_coord[log_typ][si_pos].x,
                   button_coord[log_typ][si_pos].y);
      }
   }

   return;
}


/*++++++++++++++++++++++++ draw_remote_history() ++++++++++++++++++++++++*/
void
draw_remote_history(int pos, int type, int x, int y)
{
   int       i, x_offset, y_offset;
   XGCValues gc_values;

   x_offset = x + x_offset_log_history;
   y_offset = y + SPACE_ABOVE_LINE;

   for (i = (MAX_LOG_HISTORY - his_log_set); i < MAX_LOG_HISTORY; i++)
   {
      gc_values.foreground = color_pool[(int)connect_data[pos].log_history[type][i]];
      XChangeGC(display, color_gc, GCForeground, &gc_values);
      XFillRectangle(display, line_window, color_gc, x_offset, y_offset,
                     bar_thickness_3, bar_thickness_3);
      XDrawRectangle(display, line_window, default_bg_gc, x_offset, y_offset,
                     bar_thickness_3, bar_thickness_3);
      XFillRectangle(display, line_pixmap, color_gc, x_offset, y_offset,
                     bar_thickness_3, bar_thickness_3);
      XDrawRectangle(display, line_pixmap, default_bg_gc, x_offset, y_offset,
                     bar_thickness_3, bar_thickness_3);
      x_offset += bar_thickness_3;
   }

   return;
}


/*+++++++++++++++++++++++++++ draw_mon_chars() ++++++++++++++++++++++++++*/
void
draw_mon_chars(int pos, char type, int x, int y)
{
   int       length;
   char      *ptr = NULL;
   XGCValues gc_values;
   GC        tmp_gc;

   switch (type)
   {
      case FILES_TO_BE_SEND : 
         ptr = connect_data[pos].str_fc;
         length = 4;
         break;

      case FILE_SIZE_TO_BE_SEND : 
         ptr = connect_data[pos].str_fs;
         length = 4;
         break;

      case AVERAGE_TRANSFER_RATE : 
         ptr = connect_data[pos].str_tr;
         length = 4;
         break;

      case AVERAGE_CONNECTION_RATE : 
         ptr = connect_data[pos].str_fr;
         length = 3;
         break;

      case JOBS_IN_QUEUE : 
         ptr = connect_data[pos].str_jq;
         length = 3;
         break;

      case ACTIVE_TRANSFERS : 
         ptr = connect_data[pos].str_at;
         length = 3;
         break;

      case TOTAL_ERROR_COUNTER : 
         ptr = connect_data[pos].str_ec;
         length = 2;
         break;

      case ERROR_HOSTS : 
         ptr = connect_data[pos].str_hec;
         length = 2;
         break;

      default : /* That's not possible! */
         (void)xrec(ERROR_DIALOG, "Unknown character type %d. (%s %d)",
                    (int)type, __FILE__, __LINE__);
         return;
   }

   if (connect_data[pos].inverse > OFF)
   {
      if (((type == TOTAL_ERROR_COUNTER) && (connect_data[pos].ec > 0)) ||
          ((type == ERROR_HOSTS) && (connect_data[pos].host_error_counter > 0)))
      {
         if (connect_data[pos].inverse == ON)
         {
            gc_values.background = color_pool[BLACK];
         }
         else
         {
            gc_values.background = color_pool[LOCKED_INVERSE];
         }
         XChangeGC(display, red_color_letter_gc, GCBackground,
                   &gc_values);
         tmp_gc = red_color_letter_gc;
      }
      else
      {
         if (connect_data[pos].inverse == ON)
         {
            tmp_gc = normal_letter_gc;
         }
         else
         {
            tmp_gc = locked_letter_gc;
         }
      }
   }
   else
   {
      if ((type == TOTAL_ERROR_COUNTER) && (connect_data[pos].ec > 0))
      {
         gc_values.background = color_pool[CHAR_BACKGROUND];
         XChangeGC(display, red_color_letter_gc, GCBackground, &gc_values);
         tmp_gc = red_color_letter_gc;
      }
      else if ((type == ERROR_HOSTS) && (connect_data[pos].host_error_counter > 0))
           {
              tmp_gc = red_error_letter_gc;
           }
      else if (type == JOBS_IN_QUEUE)
           {
              if ((connect_data[pos].danger_no_of_jobs != 0) &&
                  (connect_data[pos].jobs_in_queue > connect_data[pos].danger_no_of_jobs) &&
                  (connect_data[pos].jobs_in_queue <= (connect_data[pos].link_max - STOP_AMG_THRESHOLD - DIRS_IN_FILE_DIR)))
              {
                 gc_values.background = color_pool[WARNING_ID];
                 gc_values.foreground = color_pool[FG];
              }
              else if ((connect_data[pos].danger_no_of_jobs != 0) &&
                       (connect_data[pos].jobs_in_queue > (connect_data[pos].link_max - STOP_AMG_THRESHOLD - DIRS_IN_FILE_DIR)))
                   {
                      gc_values.background = color_pool[ERROR_ID];
                      gc_values.foreground = color_pool[WHITE];
                   }
                   else
                   {
                      gc_values.background = color_pool[CHAR_BACKGROUND];
                      gc_values.foreground = color_pool[FG];
                   }

              XChangeGC(display, color_letter_gc, GCBackground | GCForeground,
                        &gc_values);
              tmp_gc = color_letter_gc;
           }
           else
           {
              gc_values.background = color_pool[CHAR_BACKGROUND];
              gc_values.foreground = color_pool[BLACK];
              XChangeGC(display, color_letter_gc, GCBackground | GCForeground,
                        &gc_values);
              tmp_gc = color_letter_gc;
           }
   }
   XDrawImageString(display, line_window, tmp_gc,
                    x + x_offset_characters,
                    y + text_offset + SPACE_ABOVE_LINE,
                    ptr,
                    length);
   XDrawImageString(display, line_pixmap, tmp_gc,
                    x + x_offset_characters,
                    y + text_offset + SPACE_ABOVE_LINE,
                    ptr,
                    length);

   return;
}


/*+++++++++++++++++++++++++++ draw_mon_bar() ++++++++++++++++++++++++++++*/
void
draw_mon_bar(int         pos,
             signed char delta,
             char        bar_no,
             int         x,
             int         y)
{
   int x_offset,
       y_offset;

   x_offset = x + x_offset_bars;

   if (connect_data[pos].bar_length[(int)bar_no] > 0)
   {
      if (bar_no == MON_TR_BAR_NO)  /* TRANSFER RATE */
      {
         y_offset = y + SPACE_ABOVE_LINE;
         XFillRectangle(display, line_window, tr_bar_gc, x_offset, y_offset,
                        connect_data[pos].bar_length[(int)bar_no],
                        bar_thickness_3);
         XFillRectangle(display, line_pixmap, tr_bar_gc, x_offset, y_offset,
                        connect_data[pos].bar_length[(int)bar_no],
                        bar_thickness_3);
      }
      else if (bar_no == HOST_ERROR_BAR_NO) /* ERROR HOSTS */
           {
              XGCValues gc_values;

              y_offset = y + SPACE_ABOVE_LINE + bar_thickness_3 + bar_thickness_3;
              gc_values.foreground = color_pool[ERROR_ID];
              XChangeGC(display, color_gc, GCForeground, &gc_values);
              XFillRectangle(display, line_window, color_gc,
                             x_offset, y_offset,
                             connect_data[pos].bar_length[(int)bar_no],
                             bar_thickness_3);
              XFillRectangle(display, line_pixmap, color_gc,
                             x_offset, y_offset,
                             connect_data[pos].bar_length[(int)bar_no],
                             bar_thickness_3);
           }
           else
           {
              XColor    color;
              XGCValues gc_values;

              y_offset = y + SPACE_ABOVE_LINE + bar_thickness_3;
              color.red = 0;
              color.green = connect_data[pos].green_color_offset;
              color.blue = connect_data[pos].blue_color_offset;
              lookup_color(&color);
              gc_values.foreground = color.pixel;
              XChangeGC(display, color_gc, GCForeground, &gc_values);
              XFillRectangle(display, line_window, color_gc,
                             x_offset, y_offset,
                             connect_data[pos].bar_length[(int)bar_no],
                             bar_thickness_3);
              XFillRectangle(display, line_pixmap, color_gc,
                             x_offset, y_offset,
                             connect_data[pos].bar_length[(int)bar_no],
                             bar_thickness_3);
           }
   }
   else
   {
      if (bar_no == MON_TR_BAR_NO)  /* TRANSFER RATE */
      {
         y_offset = y + SPACE_ABOVE_LINE;
      }
      else if (bar_no == HOST_ERROR_BAR_NO) /* ERROR HOSTS */
           {
              y_offset = y + SPACE_ABOVE_LINE + bar_thickness_3 + bar_thickness_3;
           }
           else
           {
              y_offset = y + SPACE_ABOVE_LINE + bar_thickness_3;
           }
   }

   /* Remove color behind shrunken bar. */
   if (delta < 0)
   {
      GC tmp_gc;

      if (connect_data[pos].inverse == OFF)
      {
         tmp_gc = default_bg_gc;
      }
      else
      {
         if (connect_data[pos].inverse == ON)
         {
            tmp_gc = normal_bg_gc;
         }
         else
         {
            tmp_gc = locked_bg_gc;
         }
      }
      XFillRectangle(display, line_window, tmp_gc,
                     x_offset + connect_data[pos].bar_length[(int)bar_no],
                     y_offset,
                     (int)max_bar_length - connect_data[pos].bar_length[(int)bar_no],
                     bar_thickness_3);
      XFillRectangle(display, line_pixmap, tmp_gc,
                     x_offset + connect_data[pos].bar_length[(int)bar_no],
                     y_offset,
                     (int)max_bar_length - connect_data[pos].bar_length[(int)bar_no],
                     bar_thickness_3);
   }

   return;
}


/*############################ draw_clock() #############################*/
void
draw_clock(time_t current_time)
{
   char      str_line[6];
   XGCValues gc_values;

   gc_values.background = color_pool[CHAR_BACKGROUND];
   gc_values.foreground = color_pool[FG];
   (void)strftime(str_line, 6, "%H:%M", localtime(&current_time));
   XChangeGC(display, color_letter_gc, GCForeground | GCBackground,
             &gc_values);
   XDrawImageString(display, button_window, color_letter_gc,
                    window_width - DEFAULT_FRAME_SPACE - (5 * glyph_width),
                    text_offset + SPACE_ABOVE_LINE + 1,
                    str_line, 5);
   XDrawImageString(display, button_pixmap, color_letter_gc,
                    window_width - DEFAULT_FRAME_SPACE - (5 * glyph_width),
                    text_offset + SPACE_ABOVE_LINE + 1,
                    str_line, 5);

   return;
}
