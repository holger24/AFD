/*
 *  draw_line.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   void draw_dest_identifier(int pos, int x, int y)
 **   void draw_debug_led(int pos, int x, int y)
 **   void draw_led(int pos, int led_no, int x, int y)
 **   void draw_proc_led(int led_no, signed char led_status)
 **   void draw_queue_counter(nlink_t queue_counter)
 **   void draw_proc_stat(int pos, int job_no, int x, int y)
 **   void draw_chars(int pos, char type, int x, int y, int column)
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
 **   27.08.2008 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* strlen()                        */
#include <ncurses.h>
#include "nafd_ctrl.h"
#include "ui_common_defs.h"

extern char                       line_style;
extern unsigned char              saved_feature_flag;
extern int                        *line_length,
                                  max_line_length,
                                  line_height,
                                  button_width,
                                  even_height,
                                  hostname_display_length,
                                  led_width,
                                  window_width,
                                  x_offset_debug_led,
                                  x_offset_led,
                                  x_offset_proc,
                                  x_offset_bars,
                                  x_offset_characters,
                                  y_offset_led,
                                  y_center_log,
                                  no_of_columns;
extern unsigned int               text_offset;
extern long                       danger_no_of_jobs,
                                  link_max;
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
       x = 1;

   for (i = 0; i < no_of_columns; i++)
   {
      /* Draw string "  host". */
      mvaddstr(1, x, "  host");

      /* See if we need to extend heading for "Character" display. */
      if (line_style & SHOW_CHARACTERS)
      {
         /* Draw string " fc   fs   tr  ec". */
         mvaddstr(1, x_offset_characters, " fc   fs   tr  ec");
      }
      x += line_length[i];
   }

   return;
}


/*######################### draw_line_status() ##########################*/
void
draw_line_status(int pos, signed char delta)
{
   int  column,
        x, y;
   char line[MAX_COLUMN_LENGTH + 1];

   /* First locate position of x and y. */
   locate_xy_column(pos, &x, &y, &column);

#ifdef _DEBUG
   (void)printf("Drawing line %d %d  x = %d  y = %d\n",
                pos, counter++, x, y);
#endif

   if ((connect_data[pos].inverse > OFF) && (delta >= 0))
   {
      if (connect_data[pos].inverse == ON)
      {
         attrset(COLOR_PAIR(NORMAL_BG));
      }
      else
      {
         attrset(COLOR_PAIR(LOCKED_BG));
      }
   }
   else
   {
      attrset(COLOR_PAIR(DEFAULT_BG));
   }
   memset(line, ' ', line_length[column]);
   line[line_length[column]] = '\0';
   mvaddstr(y, x, line);

   /* Write destination identifier to screen. */
   draw_dest_identifier(pos, x, y);

   if (line_style & SHOW_LEDS)
   {
      /* Draw debug led. */
      draw_debug_led(pos, x, y);

      /* Draw status LED's. */
      draw_led(pos, 0, x, y);
      draw_led(pos, 1, x + 1 + LED_SPACING, y);
   }

   if (line_style & SHOW_JOBS)
   {
      int i;

      /* Draw status button for each parallel transfer. */
      for (i = 0; i < fsa[pos].allowed_transfers; i++)
      {
         draw_proc_stat(pos, i, x, y);
      }
   }

   /* Print information for number of files to be send (nf), */
   /* total file size (tfs), transfer rate (tr) and error    */
   /* counter (ec).                                          */
   if (line_style & SHOW_CHARACTERS)
   {
      draw_chars(pos, NO_OF_FILES, x, y, column);
      draw_chars(pos, TOTAL_FILE_SIZE, x + (5 * glyph_width), y, column);
      draw_chars(pos, TRANSFER_RATE, x + (10 * glyph_width), y, column);
      draw_chars(pos, ERROR_COUNTER, x + (15 * glyph_width), y, column);
   }

   return;
}


/*########################## draw_button_line() #########################*/
void
draw_button_line(void)
{
   /* Draw status LED's for AFD. */
   draw_proc_led(AMG_LED, prev_afd_status.amg);
   draw_proc_led(FD_LED, prev_afd_status.fd);
   draw_proc_led(AW_LED, prev_afd_status.archive_watch);
   if (prev_afd_status.afdd != NEITHER)
   {
      draw_proc_led(AFDD_LED, prev_afd_status.afdd);
   }

   /* Draw job queue counter. */
   draw_queue_counter(prev_afd_status.jobs_in_queue);

   return;
}


/*########################## draw_blank_line() ##########################*/
void
draw_blank_line(int pos)
{
   int  column,
        x, y;
   char line[MAX_COLUMN_LENGTH + 1];

   locate_xy_column(connect_data[pos].long_pos, &x, &y, &column);
   memset(line, ' ', line_length[column]);
   line[line_length[column]] = '\0';
   mvaddstr(y, x, line);

   return;
}


/*+++++++++++++++++++++++ draw_dest_identifier() ++++++++++++++++++++++++*/
void
draw_dest_identifier(int pos, int x, int y)
{
   /* Change color of letters when background color is to dark. */
   if ((connect_data[pos].stat_color_no == TRANSFER_ACTIVE) ||
       (connect_data[pos].stat_color_no == NOT_WORKING2) ||
       (connect_data[pos].stat_color_no == PAUSE_QUEUE) ||
       ((connect_data[pos].stat_color_no == STOP_TRANSFER) &&
       (fsa[pos].active_transfers > 0)))
   {
      attrset(COLOR_PAIR(WHITE_BG_BLACK_FG));
   }
   else
   {
      attrset(COLOR_PAIR(connect_data[pos].stat_color_no));
   }
   mvaddstr(y, x, connect_data[pos].host_display_str);

   return;
}


/*+++++++++++++++++++++++++ draw_debug_led() ++++++++++++++++++++++++++++*/
void
draw_debug_led(int pos, int x, int y)
{
   if (connect_data[pos].debug > NORMAL_MODE)
   {
      attrset(COLOR_PAIR(connect_data[pos].debug));
   }
   else
   {
      if (connect_data[pos].inverse == OFF)
      {
         attrset(COLOR_PAIR(DEFAULT_BG));
      }
      else if (connect_data[pos].inverse == ON)
           {
              attrset(COLOR_PAIR(BLACK));
           }
           else
           {
              attrset(COLOR_PAIR(LOCKED_INVERSE));
           }
   }
   mvaddstr(y, (x + x_offset_debug_led), DEBUG_SYMBOL);

   return;
}


/*++++++++++++++++++++++++++++ draw_led() ++++++++++++++++++++++++++++++*/
void
draw_led(int pos, int led_no, int x, int y)
{
   attrset(COLOR_PAIR(connect_data[pos].status_led[led_no]));
   mvaddstr(y, (x + x_offset_led), " ");

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


/*+++++++++++++++++++++++ draw_queue_counter() ++++++++++++++++++++++++++*/
void
draw_queue_counter(nlink_t queue_counter)
{
   char string[QUEUE_COUNTER_CHARS + 1];

   if ((queue_counter > danger_no_of_jobs) &&
       (queue_counter <= (link_max - STOP_AMG_THRESHOLD - DIRS_IN_FILE_DIR)))
   {
      init_pair(MIXED_PAIR, FG, WARNING_ID);
   }
   else if (queue_counter > (link_max - STOP_AMG_THRESHOLD - DIRS_IN_FILE_DIR))
        {
           init_pair(MIXED_PAIR, COLOR_WHITE, ERROR_ID);
        }
        else
        {
           init_pair(MIXED_PAIR, FG, CHAR_BACKGROUND);
        }

   attrset(COLOR_PAIR(MIXED_PAIR));
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
   mvaddstr(0, window_width - DEFAULT_FRAME_SPACE - (QUEUE_COUNTER_CHARS * 1),
            string);

   return;
}


/*+++++++++++++++++++++++++ draw_proc_stat() ++++++++++++++++++++++++++++*/
void
draw_proc_stat(int pos, int job_no, int x, int y)
{
   int       offset;
   char      string[3];
   XGCValues gc_values;

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

   gc_values.background = color_pool[(int)connect_data[pos].connect_status[job_no]];
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
