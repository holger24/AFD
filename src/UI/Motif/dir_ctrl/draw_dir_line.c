/*
 *  draw_dir_line.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2013 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   draw_dir_line - draws one complete line of the dir_ctrl window
 **
 ** SYNOPSIS
 **   void draw_dir_bar(int pos, signed char delta, char bar_no, int x, int y)
 **   void draw_dir_blank_line(int pos)
 **   void draw_dir_chars(int pos, char type, int x, int y)
 **   void draw_dir_full_marker(int pos, int x, int y, int dir_full)
 **   void draw_dir_identifier(int pos, int x, int y)
 **   void draw_dir_label_line(void)
 **   void draw_dir_line_status(int pos, signed char delta)
 **   void draw_dir_type(int pos, int x, int y)
 **
 ** DESCRIPTION
 **   The function draw_dir_label_line() draws the label which is just
 **   under the menu bar. It draws the following labels: directory, type,
 **   pr, tr, and fr when character style is set.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.09.2000 H.Kiehl Created
 **   05.05.2002 H.Kiehl Show the number files currently in the directory.
 **   28.09.2006 H.Kiehl Added error counter.
 **   24.02.2008 H.Kiehl For drawing areas, draw to offline pixmap as well.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* strlen()                        */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include "dir_ctrl.h"

extern Display                    *display;
extern Pixmap                     label_pixmap,
                                  line_pixmap;
extern Window                     label_window,
                                  line_window;
extern GC                         letter_gc,
                                  normal_letter_gc,
                                  locked_letter_gc,
                                  color_letter_gc,
                                  default_bg_gc,
                                  normal_bg_gc,
                                  locked_bg_gc,
                                  label_bg_gc,
                                  red_color_letter_gc,
                                  tr_bar_gc,
                                  tu_bar_gc,
                                  fr_bar_gc,
                                  color_gc,
                                  black_line_gc,
                                  white_line_gc;
extern Colormap                   default_cmap;
extern char                       line_style;
extern unsigned long              color_pool[];
extern float                      max_bar_length;
extern int                        line_length,
                                  line_height,
                                  bar_thickness_3,
                                  x_offset_bars,
                                  x_offset_characters,
                                  x_offset_dir_full,
                                  x_offset_type,
                                  no_of_columns;
extern unsigned int               glyph_height,
                                  glyph_width,
                                  text_offset;
extern struct dir_line            *connect_data;
extern struct fileretrieve_status *fra;

#ifdef _DEBUG
static unsigned int               counter = 0;
#endif


/*######################## draw_dir_label_line() ########################*/
void
draw_dir_label_line(void)
{
   int  i,
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

      /* Draw string "   DIR     F". */
      XDrawString(display, label_window, letter_gc,
                  x + DEFAULT_FRAME_SPACE,
                  text_offset + SPACE_ABOVE_LINE,
                  "   DIR     F",
                  12);
      XDrawString(display, label_pixmap, letter_gc,
                  x + DEFAULT_FRAME_SPACE,
                  text_offset + SPACE_ABOVE_LINE,
                  "   DIR     F",
                  12);

      /* See if we need to extend heading for "Character" display. */
      if (line_style != BARS_ONLY)
      {
         /*
          * Draw string " fd   bd  qc  fq   bq  pr  tr   fr  ec"
          *     fd - files in dir
          *     bd - bytes in dir
          *     fq - files in queue(s)
          *     bq - bytes in queue(s)
          *     pr - active process
          *     tr - transfer rate
          *     fr - file rate
          *     ec - error counter
          */
         XDrawString(display, label_window, letter_gc,
                     x + x_offset_characters,
                     text_offset + SPACE_ABOVE_LINE,
                     " fd   bd   fq   bq  pr  tr   fr  ec",
                     35);
         XDrawString(display, label_pixmap, letter_gc,
                     x + x_offset_characters,
                     text_offset + SPACE_ABOVE_LINE,
                     " fd   bd   fq   bq  pr  tr   fr  ec",
                     35);
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


/*####################### draw_dir_line_status() ########################*/
void
draw_dir_line_status(int pos, signed char delta)
{
   int x = 0,
       y = 0;
   GC  tmp_gc;

   /* First locate position of x and y. */
   locate_xy(pos, &x, &y);

#ifdef _DEBUG
   (void)printf("Drawing line %d %d  x = %d  y = %d\n", pos, counter++, x, y);
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

   /* Write destination identifier to screen. */
   draw_dir_identifier(pos, x, y);

   if (connect_data[pos].dir_flag & MAX_COPIED)
   {
      draw_dir_full_marker(pos, x, y, YES);
   }
   else
   {
      draw_dir_full_marker(pos, x, y, NO);
   }

   /* Draw protocol type. */
   draw_dir_type(pos, x, y);

   if (line_style != BARS_ONLY)
   {
      draw_dir_chars(pos, FILES_IN_DIR, x, y);
      draw_dir_chars(pos, BYTES_IN_DIR, x, y);
      draw_dir_chars(pos, FILES_QUEUED, x, y);
      draw_dir_chars(pos, BYTES_QUEUED, x, y);
      draw_dir_chars(pos, NO_OF_DIR_PROCESS, x, y);
      draw_dir_chars(pos, BYTE_RATE, x, y);
      draw_dir_chars(pos, FILE_RATE, x, y);
      draw_dir_chars(pos, DIR_ERRORS, x, y);
   }

   if (line_style != CHARACTERS_ONLY)
   {
      /* Draw bars. */
      draw_dir_bar(pos, delta, BYTE_RATE_BAR_NO, x, y);
      draw_dir_bar(pos, delta, TIME_UP_BAR_NO, x, y + bar_thickness_3);
      draw_dir_bar(pos, delta, FILE_RATE_BAR_NO, x, y + bar_thickness_3 + bar_thickness_3);

      /* Show beginning and end of bars. */
      if (connect_data[pos].inverse > OFF)
      {
         tmp_gc = white_line_gc;
      }
      else
      {
         tmp_gc = black_line_gc;
      }
      XDrawLine(display, line_window, tmp_gc,
                x + x_offset_bars - 1,
                y + SPACE_ABOVE_LINE,
                x + x_offset_bars - 1,
                y + glyph_height);
      XDrawLine(display, line_window, tmp_gc,
                x + x_offset_bars + (int)max_bar_length,
                y + SPACE_ABOVE_LINE,
                x + x_offset_bars + (int)max_bar_length, y + glyph_height);
      XDrawLine(display, line_pixmap, tmp_gc,
                x + x_offset_bars - 1,
                y + SPACE_ABOVE_LINE,
                x + x_offset_bars - 1,
                y + glyph_height);
      XDrawLine(display, line_pixmap, tmp_gc,
                x + x_offset_bars + (int)max_bar_length,
                y + SPACE_ABOVE_LINE,
                x + x_offset_bars + (int)max_bar_length, y + glyph_height);
   }

   return;
}


/*######################## draw_dir_blank_line() ########################*/
void
draw_dir_blank_line(int pos)
{
   int x, y;

   locate_xy(pos, &x, &y);

   XFillRectangle(display, line_window, default_bg_gc, x, y,
                  line_length, line_height);
   XFillRectangle(display, line_pixmap, default_bg_gc, x, y,
                  line_length, line_height);

   return;
}


/*++++++++++++++++++++++++ draw_dir_identifier() ++++++++++++++++++++++++*/
void
draw_dir_identifier(int pos, int x, int y)
{
   XGCValues  gc_values;

   /* Change color of letters when background color is to dark. */
   if ((connect_data[pos].dir_status == DIRECTORY_ACTIVE) ||
       (connect_data[pos].dir_status == NOT_WORKING2))
   {
      gc_values.foreground = color_pool[WHITE];
   }
   else
   {
      gc_values.foreground = color_pool[FG];
   }
   gc_values.background = color_pool[(int)connect_data[pos].dir_status];
   XChangeGC(display, color_letter_gc,
             GCForeground | GCBackground, &gc_values);

   XDrawImageString(display, line_window, color_letter_gc,
                    DEFAULT_FRAME_SPACE + x,
                    y + text_offset + SPACE_ABOVE_LINE,
                    connect_data[pos].dir_display_str,
                    MAX_DIR_ALIAS_LENGTH);
   XDrawImageString(display, line_pixmap, color_letter_gc,
                    DEFAULT_FRAME_SPACE + x,
                    y + text_offset + SPACE_ABOVE_LINE,
                    connect_data[pos].dir_display_str,
                    MAX_DIR_ALIAS_LENGTH);

   return;
}


/*+++++++++++++++++++++++ draw_dir_full_marker() ++++++++++++++++++++++++*/
void
draw_dir_full_marker(int pos, int x, int y, int dir_full)
{
   char str[2];
   GC   tmp_gc;

   if (dir_full == YES)
   {
      str[0] = '*';
   }
   else
   {
      str[0] = ' ';
   }
   str[1] = '\0';

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
      XGCValues gc_values;

      gc_values.foreground = color_pool[BLACK];
      gc_values.background = color_pool[DEFAULT_BG];
      XChangeGC(display, color_letter_gc, GCForeground | GCBackground,
                &gc_values);
      tmp_gc = color_letter_gc;
   }
   XDrawImageString(display, line_window, tmp_gc,
                    x + x_offset_dir_full,
                    y + text_offset + SPACE_ABOVE_LINE,
                    str, 1);
   XDrawImageString(display, line_pixmap, tmp_gc,
                    x + x_offset_dir_full,
                    y + text_offset + SPACE_ABOVE_LINE,
                    str, 1);
   return;
}


/*+++++++++++++++++++++++++++ draw_dir_type() +++++++++++++++++++++++++++*/
void
draw_dir_type(int pos, int x, int y)
{
   char str[5];
   GC   tmp_gc;

   switch (fra[pos].protocol)
   {
      case FTP : 
         str[0] = ' '; str[1] = 'F'; str[2] = 'T'; str[3] = 'P';
         break;

      case HTTP : 
         str[0] = 'H'; str[1] = 'T'; str[2] = 'T'; str[3] = 'P';
         break;

      case LOC : 
         str[0] = ' '; str[1] = 'L'; str[2] = 'O'; str[3] = 'C';
         break;

      case SFTP : 
         str[0] = 'S'; str[1] = 'F'; str[2] = 'T'; str[3] = 'P';
         break;

      case EXEC : 
         str[0] = 'E'; str[1] = 'X'; str[2] = 'E'; str[3] = 'C';
         break;

#ifdef _WITH_WMO_SUPPORT
      case WMO : 
         str[0] = ' '; str[1] = 'W'; str[2] = 'M'; str[3] = 'O';
         break;
#endif

#ifdef MBOX_SUPPORT
      case MBOX : 
         str[0] = 'M'; str[1] = 'B'; str[2] = 'O'; str[3] = 'X';
         break;
#endif
      default : /* That's not possible! */
         str[0] = 'U'; str[1] = 'N'; str[2] = 'K'; str[3] = 'N';
         (void)xrec(ERROR_DIALOG, "Unknown protocol type %d. (%s %d)",
                    fra[pos].protocol, __FILE__, __LINE__);
         return;
   }
   str[4] = '\0';

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
      tmp_gc = letter_gc;
   }
   XDrawString(display, line_window, tmp_gc, x + x_offset_type,
               y + text_offset + SPACE_ABOVE_LINE, str, 4);
   XDrawString(display, line_pixmap, tmp_gc, x + x_offset_type,
               y + text_offset + SPACE_ABOVE_LINE, str, 4);

   return;
}


/*+++++++++++++++++++++++++++ draw_dir_chars() ++++++++++++++++++++++++++*/
void
draw_dir_chars(int pos, char type, int x, int y)
{
   int        bc,   /* background color */
              fc,   /* foreground color */
              length,
              x_offset;
   char       *ptr = NULL;
   XGCValues  gc_values;
   GC         tmp_gc;

   switch (type)
   {
      case FILES_IN_DIR :
         ptr = connect_data[pos].str_files_in_dir;
         x_offset = 0;
         length = 4;
         bc = CHAR_BACKGROUND;
         fc = BLACK;
         break;

      case BYTES_IN_DIR :
         ptr = connect_data[pos].str_bytes_in_dir;
         x_offset = 5 * glyph_width;
         length = 4;
         bc = CHAR_BACKGROUND;
         fc = BLACK;
         break;

      case FILES_QUEUED :
         ptr = connect_data[pos].str_files_queued;
         x_offset = 10 * glyph_width;
         length = 4;
         bc = CHAR_BACKGROUND;
         fc = BLACK;
         break;

      case BYTES_QUEUED :
         ptr = connect_data[pos].str_bytes_queued;
         x_offset = 15 * glyph_width;
         length = 4;
         bc = CHAR_BACKGROUND;
         fc = BLACK;
         break;

      case NO_OF_DIR_PROCESS :
         ptr = connect_data[pos].str_np;
         x_offset = 20 * glyph_width;
         length = 2;
         bc = CHAR_BACKGROUND;
         fc = BLACK;
         break;

      case BYTE_RATE :
         ptr = connect_data[pos].str_tr;
         x_offset = 23 * glyph_width;
         length = 4;
         bc = CHAR_BACKGROUND;
         fc = BLACK;
         break;

      case FILE_RATE :
         ptr = connect_data[pos].str_fr;
         x_offset = 28 * glyph_width;
         length = 4;
         bc = CHAR_BACKGROUND;
         fc = BLACK;
         break;

      case DIR_ERRORS :
         ptr = connect_data[pos].str_ec;
         x_offset = 33 * glyph_width;
         length = 2;
         if (connect_data[pos].error_counter > 0)
         {
            bc = NOT_WORKING2;
            fc = WHITE;
         }
         else
         {
            bc = CHAR_BACKGROUND;
            fc = BLACK;
         }
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
      gc_values.background = color_pool[bc];
      gc_values.foreground = color_pool[fc];
      XChangeGC(display, color_letter_gc, GCBackground | GCForeground,
                &gc_values);
      tmp_gc = color_letter_gc;
   }
   XDrawImageString(display, line_window, tmp_gc,
                    x + x_offset_characters + x_offset,
                    y + text_offset + SPACE_ABOVE_LINE,
                    ptr,
                    length);
   XDrawImageString(display, line_pixmap, tmp_gc,
                    x + x_offset_characters + x_offset,
                    y + text_offset + SPACE_ABOVE_LINE,
                    ptr,
                    length);

   return;
}


/*+++++++++++++++++++++++++++ draw_dir_bar() ++++++++++++++++++++++++++++*/
void
draw_dir_bar(int         pos,
             signed char delta,
             char        bar_no,
             int         x,
             int         y)
{
   int x_offset,
       y_offset;
   GC  tmp_gc;

   x_offset = x + x_offset_bars;
   y_offset = y + SPACE_ABOVE_LINE;
   if (connect_data[pos].bar_length[(int)bar_no] > 0)
   {
      if (bar_no == BYTE_RATE_BAR_NO)
      {
         tmp_gc = tr_bar_gc;
      }
      else if (bar_no == TIME_UP_BAR_NO)
           {
              tmp_gc = tu_bar_gc;
           }
           else
           {
              tmp_gc = fr_bar_gc;
           }
      XFillRectangle(display, line_window, tmp_gc, x_offset, y_offset,
                     connect_data[pos].bar_length[(int)bar_no],
                     bar_thickness_3);
      XFillRectangle(display, line_pixmap, tmp_gc, x_offset, y_offset,
                     connect_data[pos].bar_length[(int)bar_no],
                     bar_thickness_3);
   }

   /* Remove color behind shrunken bar. */
   if (delta < 0)
   {
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
