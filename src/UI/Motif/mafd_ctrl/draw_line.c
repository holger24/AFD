/*
 *  draw_line.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2017 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   draw_line - draws one complete line of the afd_ctrl window
 **
 ** SYNOPSIS
 **   void draw_label_line(void)
 **   void draw_line_status(int pos, signed char delta)
 **   void draw_button_line(void)
 **   void draw_blank_line(int pos)
 **   void draw_dest_identifier(Window w, Pixmap p, int pos, int x, int y)
 **   void draw_debug_led(int pos, int x, int y)
 **   void draw_led(int pos, int led_no, int x, int y)
 **   void draw_proc_led(int led_no, signed char led_status)
 **   void draw_history(int type, int left)
 **   void draw_log_status(int log_typ, int si_pos)
 **   void draw_queue_counter(nlink_t queue_counter)
 **   void draw_plus_minus(int pos, int x, int y)
 **   void draw_proc_stat(int pos, int job_no, int x, int y)
 **   void draw_detailed_selection(int pos, int job_no, int x, int y)
 **   void draw_chars(int pos, char type, int x, int y, int column)
 **   void draw_bar(int pos, signed cahr delta, char bar_no, int x, int y, int column);
 **
 ** DESCRIPTION
 **   The function draw_label_line() draws the label which is just
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
 **   22.01.1996 H.Kiehl Created
 **   30.08.1997 H.Kiehl Removed all sprintf().
 **   03.09.1997 H.Kiehl Added AFDD Led.
 **   22.12.2001 H.Kiehl Added variable column length.
 **   26.12.2001 H.Kiehl Allow for more changes in line style.
 **   14.03.2003 H.Kiehl Added history log in button bar.
 **   21.06.2007 H.Kiehl Split second LED in two halfs to show the
 **                      transfer direction.
 **   16.02.2008 H.Kiehl For drawing areas, draw to offline pixmap as well.
 **   25.08.2013 H.Kiehl Added compact process status.
 **   12.01.2017 H.Kiehl Prevent this dialog from crashing when we access
 **                      color_pool values that do not exist due to broken
 **                      data.
 **   04.03.2017 H.Kiehl Add support for groups.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* strlen()                        */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include "mafd_ctrl.h"

extern Display                    *display;
extern Pixmap                     button_pixmap,
                                  label_pixmap,
                                  line_pixmap;
extern Window                     label_window,
                                  line_window,
                                  button_window;
extern GC                         letter_gc,
                                  normal_letter_gc,
                                  locked_letter_gc,
                                  color_letter_gc,
                                  default_bg_gc,
                                  normal_bg_gc,
                                  locked_bg_gc,
                                  label_bg_gc,
                                  button_bg_gc,
                                  tr_bar_gc,
                                  color_gc,
                                  black_line_gc,
                                  unset_led_bg_gc,
                                  white_line_gc,
                                  led_gc;
extern Colormap                   default_cmap;
extern char                       line_style,
                                  other_options;
extern unsigned char              saved_feature_flag;
extern unsigned long              color_pool[];
extern float                      max_bar_length;
extern int                        *line_length,
                                  max_line_length,
                                  line_height,
                                  bar_thickness_2,
                                  bar_thickness_3,
                                  button_width,
                                  even_height,
                                  hostname_display_length,
                                  led_width,
                                  no_of_his_log,
                                  *vpl,
                                  window_width,
                                  x_offset_debug_led,
                                  x_offset_led,
                                  x_offset_proc,
                                  x_offset_bars,
                                  x_offset_characters,
                                  x_offset_stat_leds,
                                  x_offset_receive_log,
                                  x_center_receive_log,
                                  x_offset_sys_log,
                                  x_center_sys_log,
                                  x_offset_trans_log,
                                  x_center_trans_log,
                                  x_offset_log_history_left,
                                  x_offset_log_history_right,
                                  y_offset_led,
                                  y_center_log,
                                  log_angle,
                                  no_of_columns;
extern unsigned int               glyph_height,
                                  glyph_width,
                                  text_offset;
extern long                       danger_no_of_jobs,
                                  link_max;
extern struct coord               coord[3][LOG_FIFO_SIZE];
extern struct afd_status          prev_afd_status;
extern struct line                *connect_data;
extern struct filetransfer_status *fsa;

#ifdef _DEBUG
static unsigned int               counter = 0;
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
                     x + line_length[i] - 2,
                     line_height - 4);
      XFillRectangle(display, label_pixmap, label_bg_gc,
                     x + 2,
                     2,
                     x + line_length[i] - 2,
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
                x + line_length[i],
                0);
      XDrawLine(display, label_window, white_line_gc,
                x + 1,
                1,
                x + line_length[i],
                1);
      XDrawLine(display, label_window, black_line_gc,
                x,
                line_height - 2,
                x + line_length[i],
                line_height - 2);
      XDrawLine(display, label_window, white_line_gc,
                x,
                line_height - 1,
                x + line_length[i],
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
                x + line_length[i],
                0);
      XDrawLine(display, label_pixmap, white_line_gc,
                x + 1,
                1,
                x + line_length[i],
                1);
      XDrawLine(display, label_pixmap, black_line_gc,
                x,
                line_height - 2,
                x + line_length[i],
                line_height - 2);
      XDrawLine(display, label_pixmap, white_line_gc,
                x,
                line_height - 1,
                x + line_length[i],
                line_height - 1);

      /* Draw string "  host". */
      XDrawString(display, label_window, letter_gc,
                  x + DEFAULT_FRAME_SPACE,
                  text_offset + SPACE_ABOVE_LINE,
                  "  host",
                  6);
      XDrawString(display, label_pixmap, letter_gc,
                  x + DEFAULT_FRAME_SPACE,
                  text_offset + SPACE_ABOVE_LINE,
                  "  host",
                  6);

      /* See if we need to extend heading for "Character" display. */
      if (line_style & SHOW_CHARACTERS)
      {
         /* Draw string " fc   fs   tr  ec". */
         XDrawString(display, label_window, letter_gc,
                     x + x_offset_characters - (max_line_length - line_length[i]),
                     text_offset + SPACE_ABOVE_LINE,
                     " fc   fs   tr  ec",
                     17);
         XDrawString(display, label_pixmap, letter_gc,
                     x + x_offset_characters - (max_line_length - line_length[i]),
                     text_offset + SPACE_ABOVE_LINE,
                     " fc   fs   tr  ec",
                     17);
      }
      x += line_length[i];
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
   int column,
       x, y;
   GC  tmp_gc;

   /* First locate position of x and y. */
   locate_xy_column(pos, -1, &x, &y, &column);

#ifdef _DEBUG
   (void)printf("Drawing line %d %d  x = %d  y = %d\n",
                pos, counter++, x, y);
#endif

   if ((connect_data[vpl[pos]].inverse > OFF) && (delta >= 0))
   {
      if (connect_data[vpl[pos]].inverse == ON)
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
   XFillRectangle(display, line_window, tmp_gc, x, y,
                  line_length[column], line_height);
   XFillRectangle(display, line_pixmap, tmp_gc, x, y,
                  line_length[column], line_height);

   if (connect_data[vpl[pos]].type == GROUP_IDENTIFIER)
   {
      draw_plus_minus(vpl[pos], x, y);

      /* Write destination identifier to screen. */
      draw_dest_identifier(line_window, line_pixmap, vpl[pos],
                           x - DEFAULT_FRAME_SPACE + (3 * glyph_width), y);

      if (line_style & SHOW_LEDS)
      {
         /* Draw status LED's. */
         draw_led(vpl[pos], 0, x + glyph_width + (glyph_width / 2) - DEFAULT_FRAME_SPACE, y);
         draw_led(vpl[pos], 1, x + glyph_width + (glyph_width / 2) - DEFAULT_FRAME_SPACE + led_width + LED_SPACING, y);
      }
   }
   else
   {
      /* Write destination identifier to screen. */
      draw_dest_identifier(line_window, line_pixmap, vpl[pos], x, y);

      if (line_style & SHOW_LEDS)
      {
         /* Draw debug led. */
         draw_debug_led(vpl[pos], x, y);

         /* Draw status LED's. */
         draw_led(vpl[pos], 0, x, y);
         draw_led(vpl[pos], 1, x + led_width + LED_SPACING, y);
      }

      if ((line_style & SHOW_JOBS) || (line_style & SHOW_JOBS_COMPACT))
      {
         int i;

         /* Draw status button for each parallel transfer. */
         for (i = 0; i < fsa[vpl[pos]].allowed_transfers; i++)
         {
            draw_proc_stat(vpl[pos], i, x, y);
         }
         if (line_style & SHOW_JOBS_COMPACT)
         {
            draw_detailed_selection(vpl[pos], fsa[vpl[pos]].allowed_transfers,
                                    x, y);
         }
      }
   }

   /* Print information for number of files to be send (nf), */
   /* total file size (tfs), transfer rate (tr) and error    */
   /* counter (ec).                                          */
   if (line_style & SHOW_CHARACTERS)
   {
      draw_chars(vpl[pos], NO_OF_FILES, x, y, column);
      draw_chars(vpl[pos], TOTAL_FILE_SIZE, x + (5 * glyph_width), y, column);
      draw_chars(vpl[pos], TRANSFER_RATE, x + (10 * glyph_width), y, column);
      draw_chars(vpl[pos], ERROR_COUNTER, x + (15 * glyph_width), y, column);
   }

   /* Draw bars, indicating graphically how many errors have */
   /* occurred, total file size still to do and the transfer */
   /* rate.                                                  */
   if (line_style & SHOW_BARS)
   {
      /* Draw bars. */
      draw_bar(vpl[pos], delta, TR_BAR_NO, x, y, column);
      draw_bar(vpl[pos], delta, ERROR_BAR_NO, x, y + bar_thickness_2, column);

      /* Show beginning and end of bars. */
      if (connect_data[vpl[pos]].inverse > OFF)
      {
         tmp_gc = white_line_gc;
      }
      else
      {
         tmp_gc = black_line_gc;
      }
      XDrawLine(display, line_window, black_line_gc,
                x + x_offset_bars - (max_line_length - line_length[column]) - 1,
                y + SPACE_ABOVE_LINE,
                x + x_offset_bars - (max_line_length - line_length[column]) - 1,
                y + glyph_height);
      XDrawLine(display, line_window, black_line_gc,
                x + x_offset_bars - (max_line_length - line_length[column]) + (int)max_bar_length,
                y + SPACE_ABOVE_LINE,
                x + x_offset_bars - (max_line_length - line_length[column]) + (int)max_bar_length, y + glyph_height);
      XDrawLine(display, line_pixmap, black_line_gc,
                x + x_offset_bars - (max_line_length - line_length[column]) - 1,
                y + SPACE_ABOVE_LINE,
                x + x_offset_bars - (max_line_length - line_length[column]) - 1,
                y + glyph_height);
      XDrawLine(display, line_pixmap, black_line_gc,
                x + x_offset_bars - (max_line_length - line_length[column]) + (int)max_bar_length,
                y + SPACE_ABOVE_LINE,
                x + x_offset_bars - (max_line_length - line_length[column]) + (int)max_bar_length, y + glyph_height);
   }

   if ((connect_data[vpl[pos]].type == GROUP_IDENTIFIER) &&
       (other_options & FRAMED_GROUPS))
   {
      XDrawLine(display, line_window, black_line_gc,
                x,
                y,
                x,
                y + line_height - 2);
      XDrawLine(display, line_pixmap, black_line_gc,
                x,
                y,
                x,
                y + line_height - 2);
      XDrawLine(display, line_window, black_line_gc,
                x,
                y,
                x + line_length[column] - 1,
                y);
      XDrawLine(display, line_pixmap, black_line_gc,
                x,
                y,
                x + line_length[column] - 1,
                y);
      XDrawLine(display, line_window, black_line_gc,
                x,
                y + line_height - 2,
                x + line_length[column] - 1,
                y + line_height - 2);
      XDrawLine(display, line_pixmap, black_line_gc,
                x,
                y + line_height - 2,
                x + line_length[column] - 1,
                y + line_height - 2);
      XDrawLine(display, line_window, black_line_gc,
                x + line_length[column] - 1,
                y,
                x + line_length[column] - 1,
                y + line_height - 2);
      XDrawLine(display, line_pixmap, black_line_gc,
                x + line_length[column] - 1,
                y,
                x + line_length[column] - 1,
                y + line_height - 2);
   }

   return;
}


/*########################## draw_button_line() #########################*/
void
draw_button_line(void)
{
   XFillRectangle(display, button_window, button_bg_gc, 0, 0, window_width,
                  line_height + 1);
   XFillRectangle(display, button_pixmap, button_bg_gc, 0, 0, window_width,
                  line_height + 1);

   /* Draw status LED's for AFD. */
   draw_proc_led(AMG_LED, prev_afd_status.amg);
   draw_proc_led(FD_LED, prev_afd_status.fd);
   draw_proc_led(AW_LED, prev_afd_status.archive_watch);
   if (prev_afd_status.afdd != NEITHER)
   {
      draw_proc_led(AFDD_LED, prev_afd_status.afdd);
   }

   if (no_of_his_log > 0)
   {
      /* Draw left history log part. */
      draw_history(RECEIVE_HISTORY, 1);
      draw_history(SYSTEM_HISTORY, 1);
      draw_history(TRANSFER_HISTORY, 1);
   }

   /* Draw log status indicators. */
   draw_log_status(RECEIVE_LOG_INDICATOR,
                   prev_afd_status.receive_log_ec % LOG_FIFO_SIZE);
   draw_log_status(SYS_LOG_INDICATOR,
                   prev_afd_status.sys_log_ec % LOG_FIFO_SIZE);
   draw_log_status(TRANS_LOG_INDICATOR,
                   prev_afd_status.trans_log_ec % LOG_FIFO_SIZE);

   if (no_of_his_log > 0)
   {
      /* Draw right history log part. */
      draw_history(RECEIVE_HISTORY, 0);
      draw_history(SYSTEM_HISTORY, 0);
      draw_history(TRANSFER_HISTORY, 0);
   }

   /* Draw job queue counter. */
   draw_queue_counter(prev_afd_status.jobs_in_queue);

   return;
}


/*########################## draw_blank_line() ##########################*/
void
draw_blank_line(int pos)
{
   int column,
       x, y;

   locate_xy_column(pos, -1, &x, &y, &column);
   XFillRectangle(display, line_window, default_bg_gc, x, y,
                  line_length[column], line_height);
   XFillRectangle(display, line_pixmap, default_bg_gc, x, y,
                  line_length[column], line_height);

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
   XChangeGC(display, color_letter_gc, GCForeground | GCBackground, &gc_values);

   if (connect_data[pos].plus_minus == PM_CLOSE_STATE)
   {
      plus_minus_str[1] = '+';
   }

#ifdef _WITH_DEBUG_PLUS_MINUS
   (void)fprintf(stderr, "%s (%d) %c\n",
                 connect_data[pos].afd_alias, pos, plus_minus_str[1]);
#endif

   XDrawImageString(display, line_window, color_letter_gc,
                    x,
                    y + text_offset + SPACE_ABOVE_LINE,
                    plus_minus_str, 3);
   XDrawImageString(display, line_pixmap, color_letter_gc,
                    x,
                    y + text_offset + SPACE_ABOVE_LINE,
                    plus_minus_str, 3);

   return;
}


/*+++++++++++++++++++++++ draw_dest_identifier() ++++++++++++++++++++++++*/
void
draw_dest_identifier(Window w, Pixmap p, int pos, int x, int y)
{
   XGCValues gc_values;

   /* Change color of letters when background color is to dark. */
   if ((connect_data[pos].stat_color_no == TRANSFER_ACTIVE) ||
       (connect_data[pos].stat_color_no == NOT_WORKING2) ||
       (connect_data[pos].stat_color_no == PAUSE_QUEUE) ||
       ((connect_data[pos].stat_color_no == STOP_TRANSFER) &&
       (fsa[pos].active_transfers > 0)))
   {
      gc_values.foreground = color_pool[WHITE];
   }
   else
   {
      gc_values.foreground = color_pool[FG];
   }
   gc_values.background = color_pool[connect_data[pos].stat_color_no];
   XChangeGC(display, color_letter_gc, GCForeground | GCBackground, &gc_values);

   XDrawImageString(display, w, color_letter_gc,
                    DEFAULT_FRAME_SPACE + x,
                    y + text_offset + SPACE_ABOVE_LINE,
                    connect_data[pos].host_display_str,
                    hostname_display_length);
   XDrawImageString(display, p, color_letter_gc,
                    DEFAULT_FRAME_SPACE + x,
                    y + text_offset + SPACE_ABOVE_LINE,
                    connect_data[pos].host_display_str,
                    hostname_display_length);

   return;
}


/*+++++++++++++++++++++++++ draw_debug_led() ++++++++++++++++++++++++++++*/
void
draw_debug_led(int pos, int x, int y)
{
   int       x_offset,
             y_offset;
   XGCValues gc_values;

   x_offset = x + x_offset_debug_led;
   y_offset = y + SPACE_ABOVE_LINE + y_offset_led;

   if (connect_data[pos].debug > NORMAL_MODE)
   {
      if (connect_data[pos].debug < COLOR_POOL_SIZE)
      {
         gc_values.foreground = color_pool[(int)connect_data[pos].debug];
      }
      else
      {
         gc_values.foreground = color_pool[DEFAULT_BG];
      }
   }
   else
   {
      if (connect_data[pos].inverse == OFF)
      {
         gc_values.foreground = color_pool[DEFAULT_BG];
      }
      else if (connect_data[pos].inverse == ON)
           {
              gc_values.foreground = color_pool[BLACK];
           }
           else
           {
              gc_values.foreground = color_pool[LOCKED_INVERSE];
           }
   }
   XChangeGC(display, color_gc, GCForeground, &gc_values);
#ifdef _SQUARE_LED
   XFillRectangle(display, line_window, color_gc, x_offset, y_offset,
                  glyph_width, glyph_width);
   XFillRectangle(display, line_pixmap, color_gc, x_offset, y_offset,
                  glyph_width, glyph_width);
#else
   XFillArc(display, line_window, color_gc, x_offset, y_offset,
            glyph_width, glyph_width, 0, 23040);
   XFillArc(display, line_pixmap, color_gc, x_offset, y_offset,
            glyph_width, glyph_width, 0, 23040);
#endif

   if (connect_data[pos].inverse == OFF)
   {
#ifdef _SQUARE_LED
      XDrawRectangle(display, line_window, black_line_gc, x_offset, y_offset,
                     glyph_width, glyph_width);
      XDrawRectangle(display, line_pixmap, black_line_gc, x_offset, y_offset,
                     glyph_width, glyph_width);
#else
      XDrawArc(display, line_window, black_line_gc, x_offset, y_offset,
               glyph_width, glyph_width, 0, 23040);
      XDrawArc(display, line_pixmap, black_line_gc, x_offset, y_offset,
               glyph_width, glyph_width, 0, 23040);
#endif
   }
   else
   {
#ifdef _SQUARE_LED
      XDrawRectangle(display, line_window, white_line_gc, x_offset, y_offset,
                     glyph_width, glyph_width);
      XDrawRectangle(display, line_pixmap, white_line_gc, x_offset, y_offset,
                     glyph_width, glyph_width);
#else
      XDrawArc(display, line_window, white_line_gc, x_offset, y_offset,
               glyph_width, glyph_width, 0, 23040);
      XDrawArc(display, line_pixmap, white_line_gc, x_offset, y_offset,
               glyph_width, glyph_width, 0, 23040);
#endif
   }

   return;
}


/*++++++++++++++++++++++++++++ draw_led() ++++++++++++++++++++++++++++++*/
void
draw_led(int pos, int led_no, int x, int y)
{
   int       x_offset,
             y_offset;
   XGCValues gc_values;

   x_offset = x + x_offset_led;
   y_offset = y + SPACE_ABOVE_LINE;

   gc_values.foreground = color_pool[(int)connect_data[pos].status_led[led_no]];
   XChangeGC(display, color_gc, GCForeground, &gc_values);
   if (led_no == 1)
   {
      if (connect_data[pos].status_led[2] == 1)
      {
         /* Transfer only. */
         XFillRectangle(display, line_window, color_gc, x_offset, y_offset,
                        led_width, bar_thickness_2 + even_height);
         XFillRectangle(display, line_window, unset_led_bg_gc,
                        x_offset, y_offset + bar_thickness_2 + even_height,
                        led_width, bar_thickness_2);
         XFillRectangle(display, line_pixmap, color_gc, x_offset, y_offset,
                        led_width, bar_thickness_2 + even_height);
         XFillRectangle(display, line_pixmap, unset_led_bg_gc,
                        x_offset, y_offset + bar_thickness_2 + even_height,
                        led_width, bar_thickness_2);
      }
      else if (connect_data[pos].status_led[2] == 2)
           {
              /* Retrieve only. */
              XFillRectangle(display, line_window, unset_led_bg_gc,
                             x_offset, y_offset,
                             led_width, bar_thickness_2 + even_height);
              XFillRectangle(display, line_pixmap, unset_led_bg_gc,
                             x_offset, y_offset,
                             led_width, bar_thickness_2 + even_height);
              if ((saved_feature_flag & DISABLE_RETRIEVE) == 0)
              {
                 XFillRectangle(display, line_window, color_gc,
                                x_offset, y_offset + bar_thickness_2 + even_height,
                                led_width, bar_thickness_2);
                 XFillRectangle(display, line_pixmap, color_gc,
                                x_offset, y_offset + bar_thickness_2 + even_height,
                                led_width, bar_thickness_2);
              }
              else
              {
                 XFillRectangle(display, line_window, white_line_gc,
                                x_offset, y_offset + bar_thickness_2 + even_height,
                                led_width, bar_thickness_2);
                 XFillRectangle(display, line_pixmap, white_line_gc,
                                x_offset, y_offset + bar_thickness_2 + even_height,
                                led_width, bar_thickness_2);
              }
           }
      else if (connect_data[pos].status_led[2] == 3)
           {
              /* Transfer + Retrieve. */
              if ((saved_feature_flag & DISABLE_RETRIEVE) == 0)
              {
                 XFillRectangle(display, line_window, color_gc,
                                x_offset, y_offset, led_width, glyph_height);
                 XFillRectangle(display, line_pixmap, color_gc,
                                x_offset, y_offset, led_width, glyph_height);
              }
              else
              {
                 XFillRectangle(display, line_window, color_gc,
                                x_offset, y_offset,
                                led_width, bar_thickness_2 + even_height);
                 XFillRectangle(display, line_window, white_line_gc,
                                x_offset, y_offset + bar_thickness_2 + even_height,
                                led_width, bar_thickness_2);
                 XFillRectangle(display, line_pixmap, color_gc,
                                x_offset, y_offset,
                                led_width, bar_thickness_2 + even_height);
                 XFillRectangle(display, line_pixmap, white_line_gc,
                                x_offset, y_offset + bar_thickness_2 + even_height,
                                led_width, bar_thickness_2);
              }
           }
           else
           {
              /* Not configured. */
              XFillRectangle(display, line_window, unset_led_bg_gc,
                             x_offset, y_offset, led_width, glyph_height);
              XFillRectangle(display, line_pixmap, unset_led_bg_gc,
                             x_offset, y_offset, led_width, glyph_height);
           }
   }
   else
   {
      XFillRectangle(display, line_window, color_gc, x_offset, y_offset,
                     led_width, glyph_height);
      XFillRectangle(display, line_pixmap, color_gc, x_offset, y_offset,
                     led_width, glyph_height);
   }
#ifndef _NO_LED_FRAME
   if (connect_data[pos].inverse == OFF)
   {
      XDrawRectangle(display, line_window, black_line_gc, x_offset, y_offset,
                     led_width, glyph_height);
      XDrawRectangle(display, line_pixmap, black_line_gc, x_offset, y_offset,
                     led_width, glyph_height);
   }
   else
   {
      XDrawRectangle(display, line_window, white_line_gc, x_offset, y_offset,
                     led_width, glyph_height);
      XDrawRectangle(display, line_pixmap, white_line_gc, x_offset, y_offset,
                     led_width, glyph_height);
   }
#endif /* _NO_LED_FRAME */

   return;
}


/*++++++++++++++++++++++++++ draw_proc_led() ++++++++++++++++++++++++++++*/
void
draw_proc_led(int led_no, signed char led_status)
{
   int       x_offset,
             y_offset;
   GC        tmp_gc;
   XGCValues gc_values;

   x_offset = x_offset_stat_leds + (led_no * (glyph_width + PROC_LED_SPACING));
   y_offset = SPACE_ABOVE_LINE + y_offset_led;

   if (led_status == ON)
   {
      XFillArc(display, button_window, led_gc, x_offset, y_offset,
               glyph_width, glyph_width, 0, 23040);
      XFillArc(display, button_pixmap, led_gc, x_offset, y_offset,
               glyph_width, glyph_width, 0, 23040);
      tmp_gc = black_line_gc;
   }
   else
   {
      if (led_status == OFF)
      {
         gc_values.foreground = color_pool[NOT_WORKING2];
         XChangeGC(display, color_gc, GCForeground, &gc_values);
         XFillArc(display, button_window, color_gc, x_offset, y_offset,
                  glyph_width, glyph_width, 0, 23040);
         XFillArc(display, button_pixmap, color_gc, x_offset, y_offset,
                  glyph_width, glyph_width, 0, 23040);
         tmp_gc = black_line_gc;
      }
      else if (led_status == STOPPED)
           {
              gc_values.foreground = color_pool[STOP_TRANSFER];
              XChangeGC(display, color_gc, GCForeground, &gc_values);
              XFillArc(display, button_window, color_gc, x_offset, y_offset,
                       glyph_width, glyph_width, 0, 23040);
              XFillArc(display, button_pixmap, color_gc, x_offset, y_offset,
                       glyph_width, glyph_width, 0, 23040);
              tmp_gc = black_line_gc;
           }
      else if (led_status == SHUTDOWN)
           {
              gc_values.foreground = color_pool[CLOSING_CONNECTION];
              XChangeGC(display, color_gc, GCForeground, &gc_values);
              XFillArc(display, button_window, color_gc, x_offset, y_offset,
                       glyph_width, glyph_width, 0, 23040);
              XFillArc(display, button_pixmap, color_gc, x_offset, y_offset,
                       glyph_width, glyph_width, 0, 23040);
              tmp_gc = black_line_gc;
           }
      else if (led_status == NEITHER)
           {
              XFillArc(display, button_window, button_bg_gc, x_offset,
                       y_offset, glyph_width, glyph_width, 0, 23040);
              XFillArc(display, button_pixmap, button_bg_gc, x_offset,
                       y_offset, glyph_width, glyph_width, 0, 23040);
              tmp_gc = button_bg_gc;
           }
           else
           {
              gc_values.foreground = color_pool[(int)led_status];
              XChangeGC(display, color_gc, GCForeground, &gc_values);
              XFillArc(display, button_window, color_gc, x_offset, y_offset,
                       glyph_width, glyph_width, 0, 23040);
              XFillArc(display, button_pixmap, color_gc, x_offset, y_offset,
                       glyph_width, glyph_width, 0, 23040);
              tmp_gc = black_line_gc;
           }
   }

   /* Draw LED frame. */
   XDrawArc(display, button_window, tmp_gc, x_offset, y_offset,
            glyph_width, glyph_width, 0, 23040);
   XDrawArc(display, button_pixmap, tmp_gc, x_offset, y_offset,
            glyph_width, glyph_width, 0, 23040);

   return;
}


/*++++++++++++++++++++++++++++ draw_history() +++++++++++++++++++++++++++*/
void
draw_history(int type, int left)
{
   int       i, start, end, x_offset, y_offset;
   XGCValues gc_values;

   if (left == 1)
   {
      start = MAX_LOG_HISTORY - no_of_his_log - no_of_his_log;
      end = MAX_LOG_HISTORY - no_of_his_log;
      x_offset = x_offset_log_history_left;
   }
   else
   {
      start = MAX_LOG_HISTORY - no_of_his_log;
      end = MAX_LOG_HISTORY;
      x_offset = x_offset_log_history_right;
   }
   if (type == RECEIVE_HISTORY)
   {
      y_offset = SPACE_ABOVE_LINE;
   }
   else if (type == SYSTEM_HISTORY)
        {
           y_offset = SPACE_ABOVE_LINE + bar_thickness_3;
        }
   else if (type == TRANSFER_HISTORY)
        {
           y_offset = SPACE_ABOVE_LINE + bar_thickness_3 + bar_thickness_3;
        }

   for (i = start; i < end; i++)
   {
      if (type == RECEIVE_HISTORY)
      {
         if (prev_afd_status.receive_log_history[i] < COLOR_POOL_SIZE)
         {
            gc_values.foreground =
               color_pool[(int)prev_afd_status.receive_log_history[i]];
         }
         else
         {
            gc_values.foreground = color_pool[NO_INFORMATION];
         }
      }
      else if (type == SYSTEM_HISTORY)
           {
              if (prev_afd_status.sys_log_history[i] < COLOR_POOL_SIZE)
              {
                 gc_values.foreground =
                    color_pool[(int)prev_afd_status.sys_log_history[i]];
              }
              else
              {
                 gc_values.foreground = color_pool[NO_INFORMATION];
              }
           }
      else if (type == TRANSFER_HISTORY)
           {
              if (prev_afd_status.trans_log_history[i] < COLOR_POOL_SIZE)
              {
                 gc_values.foreground =
                    color_pool[(int)prev_afd_status.trans_log_history[i]];
              }
              else
              {
                 gc_values.foreground = color_pool[NO_INFORMATION];
              }
           }
      XChangeGC(display, color_gc, GCForeground, &gc_values);
      XFillRectangle(display, button_window, color_gc, x_offset, y_offset,
                     bar_thickness_3, bar_thickness_3);
      XDrawRectangle(display, button_window, default_bg_gc, x_offset, y_offset,
                     bar_thickness_3, bar_thickness_3);
      XFillRectangle(display, button_pixmap, color_gc, x_offset, y_offset,
                     bar_thickness_3, bar_thickness_3);
      XDrawRectangle(display, button_pixmap, default_bg_gc, x_offset, y_offset,
                     bar_thickness_3, bar_thickness_3);
      x_offset += bar_thickness_3;
   }

   return;
}


/*++++++++++++++++++++++++ draw_log_status() ++++++++++++++++++++++++++++*/
void
draw_log_status(int log_typ, int si_pos)
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
   if (log_typ == SYS_LOG_INDICATOR)
   {
      for (i = 0; i < LOG_FIFO_SIZE; i++)
      {
         if (prev_afd_status.sys_log_fifo[i] < COLOR_POOL_SIZE)
         {
            gc_values.foreground = color_pool[(int)prev_afd_status.sys_log_fifo[i]];
         }
         else
         {
            gc_values.foreground = color_pool[NO_INFORMATION];
         }
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
      if ((prev_afd_status.sys_log_fifo[si_pos] == BLACK) ||
          (prev_afd_status.sys_log_fifo[prev_si_pos] == BLACK))
      {
         XDrawLine(display, button_window, white_line_gc,
                   x_center_sys_log, y_center_log,
                   coord[log_typ][si_pos].x, coord[log_typ][si_pos].y);
         XDrawLine(display, button_pixmap, white_line_gc,
                   x_center_sys_log, y_center_log,
                   coord[log_typ][si_pos].x, coord[log_typ][si_pos].y);
      }
      else
      {
         XDrawLine(display, button_window, black_line_gc,
                   x_center_sys_log, y_center_log,
                   coord[log_typ][si_pos].x, coord[log_typ][si_pos].y);
         XDrawLine(display, button_pixmap, black_line_gc,
                   x_center_sys_log, y_center_log,
                   coord[log_typ][si_pos].x, coord[log_typ][si_pos].y);
      }
   }
   else if (log_typ == TRANS_LOG_INDICATOR)
        {
           for (i = 0; i < LOG_FIFO_SIZE; i++)
           {
              if (prev_afd_status.trans_log_fifo[i] < COLOR_POOL_SIZE)
              {
                 gc_values.foreground = color_pool[(int)prev_afd_status.trans_log_fifo[i]];
              }
              else
              {
                 gc_values.foreground = color_pool[NO_INFORMATION];
              }
              XChangeGC(display, color_gc, GCForeground, &gc_values);
              XFillArc(display, button_window, color_gc,
                       x_offset_trans_log, SPACE_ABOVE_LINE,
                       glyph_height, glyph_height,
                       ((i * log_angle) * 64),
                       (log_angle * 64));
              XFillArc(display, button_pixmap, color_gc,
                       x_offset_trans_log, SPACE_ABOVE_LINE,
                       glyph_height, glyph_height,
                       ((i * log_angle) * 64),
                       (log_angle * 64));
           }
           if ((prev_afd_status.trans_log_fifo[si_pos] == BLACK) ||
               (prev_afd_status.trans_log_fifo[prev_si_pos] == BLACK))
           {
              XDrawLine(display, button_window, white_line_gc,
                        x_center_trans_log, y_center_log,
                        coord[log_typ][si_pos].x, coord[log_typ][si_pos].y);
              XDrawLine(display, button_pixmap, white_line_gc,
                        x_center_trans_log, y_center_log,
                        coord[log_typ][si_pos].x, coord[log_typ][si_pos].y);
           }
           else
           {
              XDrawLine(display, button_window, black_line_gc,
                        x_center_trans_log, y_center_log,
                        coord[log_typ][si_pos].x, coord[log_typ][si_pos].y);
              XDrawLine(display, button_pixmap, black_line_gc,
                        x_center_trans_log, y_center_log,
                        coord[log_typ][si_pos].x, coord[log_typ][si_pos].y);
           }
        }
        else
        {
           for (i = 0; i < LOG_FIFO_SIZE; i++)
           {
              if (prev_afd_status.receive_log_fifo[i] < COLOR_POOL_SIZE)
              {
                 gc_values.foreground = color_pool[(int)prev_afd_status.receive_log_fifo[i]];
              }
              else
              {
                 gc_values.foreground = color_pool[NO_INFORMATION];
              }
              XChangeGC(display, color_gc, GCForeground, &gc_values);
              XFillArc(display, button_window, color_gc,
                       x_offset_receive_log, SPACE_ABOVE_LINE,
                       glyph_height, glyph_height,
                       ((i * log_angle) * 64),
                       (log_angle * 64));
              XFillArc(display, button_pixmap, color_gc,
                       x_offset_receive_log, SPACE_ABOVE_LINE,
                       glyph_height, glyph_height,
                       ((i * log_angle) * 64),
                       (log_angle * 64));
           }
           if ((prev_afd_status.receive_log_fifo[si_pos] == BLACK) ||
               (prev_afd_status.receive_log_fifo[prev_si_pos] == BLACK))
           {
              XDrawLine(display, button_window, white_line_gc,
                        x_center_receive_log, y_center_log,
                        coord[log_typ][si_pos].x, coord[log_typ][si_pos].y);
              XDrawLine(display, button_pixmap, white_line_gc,
                        x_center_receive_log, y_center_log,
                        coord[log_typ][si_pos].x, coord[log_typ][si_pos].y);
           }
           else
           {
              XDrawLine(display, button_window, black_line_gc,
                        x_center_receive_log, y_center_log,
                        coord[log_typ][si_pos].x, coord[log_typ][si_pos].y);
              XDrawLine(display, button_pixmap, black_line_gc,
                        x_center_receive_log, y_center_log,
                        coord[log_typ][si_pos].x, coord[log_typ][si_pos].y);
           }
        }

   return;
}


/*+++++++++++++++++++++++ draw_queue_counter() ++++++++++++++++++++++++++*/
void
draw_queue_counter(nlink_t queue_counter)
{
   char      string[QUEUE_COUNTER_CHARS + 1];
   XGCValues gc_values;

   if ((queue_counter > danger_no_of_jobs) &&
       (queue_counter <= (link_max - STOP_AMG_THRESHOLD - DIRS_IN_FILE_DIR)))
   {
      gc_values.background = color_pool[WARNING_ID];
      gc_values.foreground = color_pool[FG];
   }
   else if (queue_counter > (link_max - STOP_AMG_THRESHOLD - DIRS_IN_FILE_DIR))
        {
           gc_values.background = color_pool[ERROR_ID];
           gc_values.foreground = color_pool[WHITE];
        }
        else
        {
           gc_values.background = color_pool[CHAR_BACKGROUND];
           gc_values.foreground = color_pool[FG];
        }

   string[QUEUE_COUNTER_CHARS] = '\0';
   if (queue_counter == 0)
   {
      string[0] = string[1] = string[2] = ' ';
      string[3] = '0';
   }
   else
   {
      if (queue_counter < 10)
      {
         string[0] = string[1] = string[2] = ' ';
         string[3] = queue_counter + '0';
      }
      else if (queue_counter < 100)
           {
              string[0] = string[1] = ' ';
              string[2] = (queue_counter / 10) + '0';
              string[3] = (queue_counter % 10) + '0';
           }
      else if (queue_counter < 1000)
           {
              string[0] = ' ';
              string[1] = (queue_counter / 100) + '0';
              string[2] = ((queue_counter / 10) % 10) + '0';
              string[3] = (queue_counter % 10) + '0';
           }
           else
           {
              string[0] = ((queue_counter / 1000) % 10) + '0';
              string[1] = ((queue_counter / 100) % 10) + '0';
              string[2] = ((queue_counter / 10) % 10) + '0';
              string[3] = (queue_counter % 10) + '0';
           }
   }

   XChangeGC(display, color_letter_gc, GCForeground | GCBackground, &gc_values);
   XDrawImageString(display, button_window, color_letter_gc,
                    window_width - DEFAULT_FRAME_SPACE - (QUEUE_COUNTER_CHARS * glyph_width),
                    text_offset + SPACE_ABOVE_LINE + 1,
                    string,
                    QUEUE_COUNTER_CHARS);
   XDrawImageString(display, button_pixmap, color_letter_gc,
                    window_width - DEFAULT_FRAME_SPACE - (QUEUE_COUNTER_CHARS * glyph_width),
                    text_offset + SPACE_ABOVE_LINE + 1,
                    string,
                    QUEUE_COUNTER_CHARS);

   return;
}


/*+++++++++++++++++++++++++ draw_proc_stat() ++++++++++++++++++++++++++++*/
void
draw_proc_stat(int pos, int job_no, int x, int y)
{
   XGCValues gc_values;

   if (job_no >= fsa[pos].allowed_transfers)
   {
      if (connect_data[pos].inverse == OFF)
      {
         gc_values.foreground = color_pool[DEFAULT_BG];
      }
      else if (connect_data[pos].inverse == ON)
           {
              gc_values.foreground = color_pool[BLACK];
           }
           else
           {
              gc_values.foreground = color_pool[LOCKED_INVERSE];
           }
      XChangeGC(display, color_gc, GCForeground, &gc_values);

      if (line_style & SHOW_JOBS_COMPACT)
      {
         int x_offset,
             y_offset;

         x_offset = x + x_offset_proc + ((job_no / 3) * bar_thickness_3);
         y_offset = y + SPACE_ABOVE_LINE + ((job_no % 3) * bar_thickness_3);
         XFillRectangle(display, line_window, color_gc, x_offset, y_offset,
                        bar_thickness_3, bar_thickness_3);
         XDrawRectangle(display, line_window, color_gc, x_offset, y_offset,
                        bar_thickness_3, bar_thickness_3);
         XFillRectangle(display, line_pixmap, color_gc, x_offset, y_offset,
                        bar_thickness_3, bar_thickness_3);
         XDrawRectangle(display, line_pixmap, color_gc, x_offset, y_offset,
                        bar_thickness_3, bar_thickness_3);
      }
      else
      {
         int offset = job_no * (button_width + BUTTON_SPACING);

         XFillRectangle(display, line_window, color_gc,
                        x + x_offset_proc + offset - 1,
                        y + SPACE_ABOVE_LINE - 1,
                        button_width + 2,
                        glyph_height + 2);
         XFillRectangle(display, line_pixmap, color_gc,
                        x + x_offset_proc + offset - 1,
                        y + SPACE_ABOVE_LINE - 1,
                        button_width + 2,
                        glyph_height + 2);
      }
   }
   else
   {
      if (line_style & SHOW_JOBS_COMPACT)
      {
         int x_offset,
             y_offset;

         x_offset = x + x_offset_proc + ((job_no / 3) * bar_thickness_3);
         y_offset = y + SPACE_ABOVE_LINE + ((job_no % 3) * bar_thickness_3);
         if (connect_data[pos].connect_status[job_no] < COLOR_POOL_SIZE)
         {
            gc_values.foreground = color_pool[(int)connect_data[pos].connect_status[job_no]];
         }
         else
         {
            gc_values.foreground = color_pool[DEFAULT_BG];
         }
         XChangeGC(display, color_gc, GCForeground, &gc_values);
         XFillRectangle(display, line_window, color_gc, x_offset, y_offset,
                        bar_thickness_3, bar_thickness_3);
         XFillRectangle(display, line_pixmap, color_gc, x_offset, y_offset,
                        bar_thickness_3, bar_thickness_3);
         if (connect_data[pos].inverse == OFF)
         {
            gc_values.foreground = color_pool[DEFAULT_BG];
         }
         else if (connect_data[pos].inverse == ON)
              {
                 gc_values.foreground = color_pool[BLACK];
              }
              else
              {
                 gc_values.foreground = color_pool[LOCKED_INVERSE];
              }
         XChangeGC(display, color_gc, GCForeground, &gc_values);
         XDrawRectangle(display, line_window, color_gc, x_offset, y_offset,
                        bar_thickness_3, bar_thickness_3);
         XDrawRectangle(display, line_pixmap, color_gc, x_offset, y_offset,
                        bar_thickness_3, bar_thickness_3);
      }
      else
      {
         int  offset;
         char string[3];

         offset = job_no * (button_width + BUTTON_SPACING);

         string[2] = '\0';
         if (connect_data[pos].no_of_files[job_no] > -1)
         {
            int num = connect_data[pos].no_of_files[job_no] % 100;

            string[0] = ((num / 10) % 10) + '0';
            string[1] = (num % 10) + '0';
         }
         else
         {
            string[0] = '0';
            string[1] = '0';
         }

         /* Change color of letters when background color is to dark. */
         if ((connect_data[pos].connect_status[job_no] == FTP_ACTIVE) ||
             (connect_data[pos].connect_status[job_no] == SFTP_RETRIEVE_ACTIVE) ||
#ifdef _WITH_SCP_SUPPORT
             (connect_data[pos].connect_status[job_no] == SCP_ACTIVE) ||
#endif
             (connect_data[pos].connect_status[job_no] == HTTP_RETRIEVE_ACTIVE) ||
             (connect_data[pos].connect_status[job_no] == CONNECTING))
         {
            gc_values.foreground = color_pool[WHITE];
         }
         else
         {
            gc_values.foreground = color_pool[FG];
         }

         if (connect_data[pos].connect_status[job_no] < COLOR_POOL_SIZE)
         {
            gc_values.background = color_pool[(int)connect_data[pos].connect_status[job_no]];
         }
         else
         {
            gc_values.background = color_pool[DEFAULT_BG];
         }
         XChangeGC(display, color_letter_gc, GCForeground | GCBackground, &gc_values);
         XDrawImageString(display, line_window, color_letter_gc,
                          x + x_offset_proc + offset,
                          y + text_offset + SPACE_ABOVE_LINE,
                          string,
                          2);
         XDrawImageString(display, line_pixmap, color_letter_gc,
                          x + x_offset_proc + offset,
                          y + text_offset + SPACE_ABOVE_LINE,
                          string,
                          2);

         if (connect_data[pos].detailed_selection[job_no] == YES)
         {
            gc_values.foreground = color_pool[DEBUG_MODE];
            XChangeGC(display, color_gc, GCForeground, &gc_values);
            XDrawRectangle(display, line_window, color_gc,
                           x + x_offset_proc + offset - 1,
                           y + SPACE_ABOVE_LINE - 1,
                           button_width + 1,
                           glyph_height + 1);
            XDrawRectangle(display, line_pixmap, color_gc,
                           x + x_offset_proc + offset - 1,
                           y + SPACE_ABOVE_LINE - 1,
                           button_width + 1,
                           glyph_height + 1);
         }
      }
   }

   return;
}


/*+++++++++++++++++++++ draw_detailed_selection() +++++++++++++++++++++++*/
void
draw_detailed_selection(int pos, int job_no, int x, int y)
{
   int       offset, proc_width;
   XGCValues gc_values;

   if (line_style & SHOW_JOBS_COMPACT)
   {
      proc_width = (job_no / 3) * bar_thickness_3;
      if (job_no % 3)          
      {
         proc_width += bar_thickness_3;
      }
      proc_width += 1;
      offset = 0;
      job_no -= 1;
   }
   else
   {
      offset = job_no * (button_width + BUTTON_SPACING);
      proc_width = button_width;
   }

   if (connect_data[pos].detailed_selection[job_no] == YES)
   {
      gc_values.foreground = color_pool[DEBUG_MODE];
   }
   else
   {
      if (connect_data[pos].inverse == OFF)
      {
         gc_values.foreground = color_pool[DEFAULT_BG];
      }
      else if (connect_data[pos].inverse == ON)
           {
              gc_values.foreground = color_pool[BLACK];
           }
           else
           {
              gc_values.foreground = color_pool[LOCKED_INVERSE];
           }
   }
   XChangeGC(display, color_gc, GCForeground, &gc_values);
   XDrawRectangle(display, line_window, color_gc,
                  x + x_offset_proc + offset - 1,
                  y + SPACE_ABOVE_LINE - 1,
                  proc_width + 1,
                  glyph_height + 1);
   XDrawRectangle(display, line_pixmap, color_gc,
                  x + x_offset_proc + offset - 1,
                  y + SPACE_ABOVE_LINE - 1,
                  proc_width + 1,
                  glyph_height + 1);

   return;
}


/*+++++++++++++++++++++++++++++ draw_chars() ++++++++++++++++++++++++++++*/
void
draw_chars(int pos, char type, int x, int y, int column)
{
   int       length;
   char      *ptr = NULL;
   XGCValues gc_values;
   GC        tmp_gc;

   switch (type)
   {
      case NO_OF_FILES : 
         ptr = connect_data[pos].str_tfc;
         length = 4;
         break;

      case TOTAL_FILE_SIZE : 
         ptr = connect_data[pos].str_tfs;
         length = 4;
         break;

      case TRANSFER_RATE : 
         ptr = connect_data[pos].str_tr;
         length = 4;
         break;

      case ERROR_COUNTER : 
         ptr = connect_data[pos].str_ec;
         length = 2;
         break;

      default : /* That's not possible! */
         (void)xrec(ERROR_DIALOG, "Unknown character type %d. (%s %d)",
                    (int)type, __FILE__, __LINE__);
         return;
   }

   if (connect_data[pos].inverse > OFF)
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
   else
   {
      gc_values.foreground = color_pool[BLACK];
      gc_values.background = color_pool[CHAR_BACKGROUND];
      XChangeGC(display, color_letter_gc, GCForeground | GCBackground,
                &gc_values);
      tmp_gc = color_letter_gc;
   }
   XDrawImageString(display, line_window, tmp_gc,
                    x + x_offset_characters - (max_line_length - line_length[column]),
                    y + text_offset + SPACE_ABOVE_LINE,
                    ptr,
                    length);
   XDrawImageString(display, line_pixmap, tmp_gc,
                    x + x_offset_characters - (max_line_length - line_length[column]),
                    y + text_offset + SPACE_ABOVE_LINE,
                    ptr,
                    length);

   return;
}


/*+++++++++++++++++++++++++++++ draw_bar() ++++++++++++++++++++++++++++++*/
void
draw_bar(int         pos,
         signed char delta,
         char        bar_no,
         int         x,
         int         y,
         int         column)
{
   int x_offset,
       y_offset;

   x_offset = x + x_offset_bars - (max_line_length - line_length[column]);
   y_offset = y + SPACE_ABOVE_LINE;

   if (connect_data[pos].bar_length[(int)bar_no] > 0)
   {
      if (bar_no == TR_BAR_NO)  /* TRANSFER RATE. */
      {
         XFillRectangle(display, line_window, tr_bar_gc, x_offset, y_offset,
                        connect_data[pos].bar_length[(int)bar_no],
                        bar_thickness_2);
         XFillRectangle(display, line_pixmap, tr_bar_gc, x_offset, y_offset,
                        connect_data[pos].bar_length[(int)bar_no],
                        bar_thickness_2);
      }
      else if (bar_no == ERROR_BAR_NO) /* ERROR. */
           {
              XColor    color;
              XGCValues gc_values;

              color.blue = 0;
              color.green = connect_data[pos].green_color_offset;
              color.red = connect_data[pos].red_color_offset;
              lookup_color(&color);
              gc_values.foreground = color.pixel;
              XChangeGC(display, color_gc, GCForeground, &gc_values);
              XFillRectangle(display, line_window, color_gc,
                             x_offset, y_offset,
                             connect_data[pos].bar_length[(int)bar_no],
                             bar_thickness_2);
              XFillRectangle(display, line_pixmap, color_gc,
                             x_offset, y_offset,
                             connect_data[pos].bar_length[(int)bar_no],
                             bar_thickness_2);
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
                     bar_thickness_2);
      XFillRectangle(display, line_pixmap, tmp_gc,
                     x_offset + connect_data[pos].bar_length[(int)bar_no],
                     y_offset,
                     (int)max_bar_length - connect_data[pos].bar_length[(int)bar_no],
                     bar_thickness_2);
   }

   return;
}
