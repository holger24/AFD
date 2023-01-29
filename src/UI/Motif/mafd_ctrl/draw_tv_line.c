/*
 *  draw_tv_line.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   draw_tv_line - draws one complete line of the detailed transfer
 **                  view window of the AFD
 **
 ** SYNOPSIS
 **   void draw_rotating_dash(int pos, int x, int y)
 **   void draw_tv_label_line(void)
 **   void draw_detailed_line(int pos)
 **   void draw_tv_blank_line(int pos)
 **   void draw_tv_dest_identifier(int pos, int x, int y)
 **   void draw_tv_job_number(int pos, int x, int y)
 **   void draw_tv_priority(int pos, int x, int y)
 **   void draw_file_name(int pos, int x, int y)
 **   void draw_tv_chars(int pos, char type, int x, int y)
 **   void draw_tv_bar(int pos, signed char delta, char bar_no, int x, int y)
 **   void tv_locate_xy(int pos, int *x, int *y)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None. Except for the function tv_locate_xy() which fills
 **   x and y with data.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   08.01.1998 H.Kiehl Created
 **   05.03.2010 H.Kiehl Show again priority.
 **
 */
DESCR__E_M3

#include <string.h>
#include "mafd_ctrl.h"


/* External global variables. */
extern Display                    *display;
extern Window                     detailed_window,
                                  tv_label_window;
extern GC                         black_line_gc,
                                  color_gc,
                                  color_letter_gc,
                                  default_bg_gc,
                                  label_bg_gc,
                                  letter_gc,
                                  tr_bar_gc,
                                  white_line_gc;
extern int                        bar_thickness_3,
                                  filename_display_length,
                                  hostname_display_length,
                                  line_height,
                                  tv_line_length,
                                  tv_no_of_columns,
                                  tv_no_of_rows,
                                  x_offset_tv_bars,
                                  x_offset_tv_characters,
                                  x_offset_tv_file_name,
                                  x_offset_tv_job_number,
                                  x_offset_tv_priority,
                                  x_offset_tv_rotating_dash;
extern unsigned int               glyph_height,
                                  glyph_width,
                                  text_offset;
extern long                       color_pool[];
extern float                      max_bar_length;
extern char                       line_style;
extern struct job_data            *jd;
extern struct filetransfer_status *fsa;


/*######################## draw_tv_label_line() #########################*/
void
draw_tv_label_line(void)
{
   int i,
       x = 0;

   for (i = 0; i < tv_no_of_columns; i++)
   {
      /* First draw the background in the appropriate color. */
      XFillRectangle(display, tv_label_window, label_bg_gc,
                     x + 2,
                     2,
                     x + tv_line_length - 2,
                     line_height - 4);

      /* Now draw left, top and bottom end for button style. */
      XDrawLine(display, tv_label_window, black_line_gc,
                x,
                0,
                x,
                line_height);
      XDrawLine(display, tv_label_window, white_line_gc,
                x + 1,
                1,
                x + 1,
                line_height - 3);
      XDrawLine(display, tv_label_window, black_line_gc,
                x,
                0,
                x + tv_line_length,
                0);
      XDrawLine(display, tv_label_window, white_line_gc,
                x + 1,
                1,
                x + tv_line_length,
                1);
      XDrawLine(display, tv_label_window, black_line_gc,
                x,
                line_height - 2,
                x + tv_line_length,
                line_height - 2);
      XDrawLine(display, tv_label_window, white_line_gc,
                x,
                line_height - 1,
                x + tv_line_length,
                line_height - 1);

      /* Draw string "host". */
      XDrawString(display, tv_label_window, letter_gc,
                  x + DEFAULT_FRAME_SPACE,
                  text_offset + SPACE_ABOVE_LINE,
                  "host",
                  4);

      /* Draw string "J P     file name". */
      XDrawString(display, tv_label_window, letter_gc,
                  x + x_offset_tv_job_number,
                  text_offset + SPACE_ABOVE_LINE,
                  "J P     file name",
                  17);

      /* See if we need to extend heading for "Character" display. */
      if (line_style & SHOW_CHARACTERS)
      {
         /* Draw string " fs   fsd   fc   fcd   tfs tfsd". */
         XDrawString(display, tv_label_window, letter_gc,
                     x + x_offset_tv_characters,
                     text_offset + SPACE_ABOVE_LINE,
                     " fs   fsd  fc   fcd  tfs tfsd",
                     29);
      }

      x += tv_line_length;
   }

   /* Draw right end for button style. */
   XDrawLine(display, tv_label_window, black_line_gc,
             x - 2,
             0,
             x - 2,
             line_height - 2);
   XDrawLine(display, tv_label_window, white_line_gc,
             x - 1,
             1,
             x - 1,
             line_height - 2);

   return;
}


/*######################## draw_detailed_line() #########################*/
void
draw_detailed_line(int pos)
{
   int x = 0,
       y = 0;

   /* First locate position of x and y. */
   tv_locate_xy(pos, &x, &y);

   XFillRectangle(display, detailed_window, default_bg_gc,
                  x,
                  y,
                  tv_line_length,
                  line_height);

   /* Write destination identifier to screen. */
   draw_tv_dest_identifier(pos, x, y);

   /* Write the job number of the host. */
   draw_tv_job_number(pos, x, y);

   /* Write the priority of the host. */
   draw_tv_priority(pos, x, y);

   /* Show file currently being transfered. */
   draw_file_name(pos, x, y);

   /* Show rotating dash to indicate if data is being transferred. */
   draw_rotating_dash(pos, x, y);

   /* Print information for file size in use (fs), file size   */
   /* size in use done (fsd), number of files to be done (fc), */
   /* number of files done (fcd), total file size (tfs) and    */
   /* total file size done (tfsd).                             */
   if (line_style & SHOW_CHARACTERS)
   {
      draw_tv_chars(pos, FILE_SIZE_IN_USE, x, y);
      draw_tv_chars(pos, FILE_SIZE_IN_USE_DONE, x, y);
      draw_tv_chars(pos, NUMBER_OF_FILES, x, y);
      draw_tv_chars(pos, NUMBER_OF_FILES_DONE, x, y);
      draw_tv_chars(pos, FILE_SIZE, x, y);
      draw_tv_chars(pos, FILE_SIZE_DONE, x, y);
   }

   /* Draw bars, indicating graphically how many bytes are  */
   /* send for the current file, how many files have been   */
   /* send and the total number of bytes send for this job. */
   if (line_style & SHOW_BARS)
   {
      /* Draw bars. */
      draw_tv_bar(pos, 0, CURRENT_FILE_SIZE_BAR_NO, x, y);
      draw_tv_bar(pos, 0, NO_OF_FILES_DONE_BAR_NO, x, y + bar_thickness_3);
      draw_tv_bar(pos, 0, FILE_SIZE_DONE_BAR_NO,
                  x, y + bar_thickness_3 + bar_thickness_3);

      /* Show beginning and end of bars. */
      XDrawLine(display, detailed_window, black_line_gc,
                x + x_offset_tv_bars - 1,
                y + SPACE_ABOVE_LINE,
                x + x_offset_tv_bars - 1,
                y + glyph_height);
      XDrawLine(display, detailed_window, black_line_gc,
                x + x_offset_tv_bars + (int)max_bar_length,
                y + SPACE_ABOVE_LINE,
                x + x_offset_tv_bars + (int)max_bar_length, y + glyph_height);
   }

   return;
}


/*######################## draw_tv_blank_line() #########################*/
void
draw_tv_blank_line(int pos)
{
   int   x,
         y;

   tv_locate_xy(pos, &x, &y);

   XFillRectangle(display, detailed_window, default_bg_gc,
                  x, y, tv_line_length, line_height);

   return;
}


/*+++++++++++++++++++++++++ draw_rotating_dash() ++++++++++++++++++++++++++*/
void
draw_rotating_dash(int pos, int x, int y)
{
   char      string[2];
   XGCValues gc_values;

   if (jd[pos].rotate == -1)
   {
      string[0] = '-';
   }
   else if (jd[pos].rotate == 0)
        {
           string[0] = '\\';
        }
   else if (jd[pos].rotate == 1)
        {
           string[0] = '|';
        }
   else if (jd[pos].rotate == 2)
        {
           string[0] = '/';
           jd[pos].rotate = -2;
        }
        else
        {
           string[0] = ' ';
        }
   string[1] = '\0';

   gc_values.background = color_pool[DEFAULT_BG];
   gc_values.foreground = color_pool[BLACK];
   XChangeGC(display, color_letter_gc, GCForeground | GCBackground, &gc_values);
   XDrawImageString(display, detailed_window, color_letter_gc,
                    x + x_offset_tv_rotating_dash,
                    y + text_offset + SPACE_ABOVE_LINE,
                    string,
                    1);

   return;
}


/*+++++++++++++++++++++++ draw_tv_dest_identifier() +++++++++++++++++++++++*/
void
draw_tv_dest_identifier(int pos, int x, int y)
{
   XGCValues  gc_values;

   /* Change color of letters when background color is to dark. */
   if ((jd[pos].stat_color_no == TRANSFER_ACTIVE) ||
       (jd[pos].stat_color_no == NOT_WORKING2) ||
       (jd[pos].stat_color_no == PAUSE_QUEUE) ||
       ((jd[pos].stat_color_no == STOP_TRANSFER) &&
       (fsa[jd[pos].fsa_no].active_transfers > 0)))
   {
      gc_values.foreground = color_pool[WHITE];
   }
   else
   {
      gc_values.foreground = color_pool[FG];
   }
   gc_values.background = color_pool[jd[pos].stat_color_no];
   XChangeGC(display, color_letter_gc, GCForeground | GCBackground, &gc_values);

   XDrawImageString(display, detailed_window, color_letter_gc,
                    DEFAULT_FRAME_SPACE + x,
                    y + text_offset + SPACE_ABOVE_LINE,
                    jd[pos].host_display_str,
                    hostname_display_length);

   return;
}


/*++++++++++++++++++++++++ draw_tv_job_number() +++++++++++++++++++++++++*/
void
draw_tv_job_number(int pos, int x, int y)
{
   char      string[2];
   XGCValues gc_values;

   string[0] = jd[pos].job_no + '0';
   string[1] = '\0';

   /* Change color of letters when background color is to dark. */
   if ((jd[pos].connect_status == FTP_ACTIVE) ||
#ifdef _WITH_SCP_SUPPORT
       (jd[pos].connect_status == SCP_ACTIVE) ||
#endif
       (jd[pos].connect_status == HTTP_RETRIEVE_ACTIVE) ||
       (jd[pos].connect_status == CONNECTING))
   {
      gc_values.foreground = color_pool[WHITE];
   }
   else
   {
      gc_values.foreground = color_pool[FG];
   }

   gc_values.background = color_pool[(int)jd[pos].connect_status];
   XChangeGC(display, color_letter_gc, GCForeground | GCBackground, &gc_values);
   XDrawImageString(display, detailed_window, color_letter_gc,
                    x + x_offset_tv_job_number,
                    y + text_offset + SPACE_ABOVE_LINE,
                    string,
                    1);

   return;
}


/*+++++++++++++++++++++++++ draw_tv_priority() ++++++++++++++++++++++++++*/
void
draw_tv_priority(int pos, int x, int y)
{
   char      string[2];
   XGCValues gc_values;

   if (jd[pos].priority[0] == '\0')
   {
      string[0] = '-';
   }
   else
   {
      string[0] = jd[pos].priority[0];
   }
   string[1] = '\0';

   gc_values.background = color_pool[DEFAULT_BG];
   gc_values.foreground = color_pool[BLACK];
   XChangeGC(display, color_letter_gc, GCForeground | GCBackground, &gc_values);
   XDrawImageString(display, detailed_window, color_letter_gc,
                    x + x_offset_tv_priority,
                    y + text_offset + SPACE_ABOVE_LINE,
                    string,
                    1);

   return;
}


/*++++++++++++++++++++++++++ draw_file_name() +++++++++++++++++++++++++++*/
void
draw_file_name(int pos, int x, int y)
{
   XGCValues gc_values;

   gc_values.foreground = color_pool[BLACK];
   gc_values.background = color_pool[WHITE];
   XChangeGC(display, color_letter_gc, GCForeground | GCBackground, &gc_values);
   XDrawImageString(display, detailed_window, color_letter_gc,
                    x + x_offset_tv_file_name,
                    y + text_offset + SPACE_ABOVE_LINE,
                    jd[pos].file_name_in_use,
                    filename_display_length);

   return;
}


/*++++++++++++++++++++++++++ draw_tv_chars() ++++++++++++++++++++++++++++*/
void
draw_tv_chars(int pos, char type, int x, int y)
{
   int        length,
              offset;
   char       *ptr = NULL;
   XGCValues  gc_values;  

   switch (type)
   {
      case FILE_SIZE_IN_USE :
         ptr = jd[pos].str_fs_use;
         offset = 0;
         length = 4;
         break;

      case FILE_SIZE_IN_USE_DONE :
         ptr = jd[pos].str_fs_use_done;
         offset = 5 * glyph_width;
         length = 4;
         break;
         
      case NUMBER_OF_FILES :
         ptr = jd[pos].str_fc;
         offset = 10 * glyph_width;
         length = 4;
         break;
         
      case NUMBER_OF_FILES_DONE :
         ptr = jd[pos].str_fc_done;
         offset = 15 * glyph_width;
         length = 4;
         break;
         
      case FILE_SIZE :
         ptr = jd[pos].str_fs;
         offset = 20 * glyph_width;
         length = 4;
         break;
         
      case FILE_SIZE_DONE :
         ptr = jd[pos].str_fs_done;
         offset = 25 * glyph_width;
         length = 4;
         break;

      default : /* That's not possible! */
         (void)fprintf(stderr, "ERROR   : Unknown type. (%s %d)\n",
                       __FILE__, __LINE__);
         return;
   }

   gc_values.foreground = color_pool[BLACK];
   gc_values.background = color_pool[CHAR_BACKGROUND];
   XChangeGC(display, color_letter_gc, GCForeground | GCBackground, &gc_values);
   XDrawImageString(display, detailed_window, color_letter_gc,
                    x + x_offset_tv_characters + offset,
                    y + text_offset + SPACE_ABOVE_LINE,
                    ptr,
                    length);

   return;
}


/*+++++++++++++++++++++++++++ draw_tv_bar() +++++++++++++++++++++++++++++*/
void
draw_tv_bar(int pos, signed char delta, char bar_no, int x, int y)
{
   if (delta < 0)
   {
      /* Bar length is reduced so remove color behind bar. */
      XFillRectangle(display, detailed_window, default_bg_gc,
                     x + x_offset_tv_bars + jd[pos].bar_length[(int)bar_no],
                     y + SPACE_ABOVE_LINE,
                     (int)max_bar_length - jd[pos].bar_length[(int)bar_no],
                     bar_thickness_3);
   }
   else
   {
      XGCValues gc_values;

      if (bar_no == CURRENT_FILE_SIZE_BAR_NO)
      {
         gc_values.foreground = color_pool[NORMAL_STATUS];
         XChangeGC(display, color_gc, GCForeground, &gc_values);
         XFillRectangle(display, detailed_window, color_gc,
                        x + x_offset_tv_bars,
                        y + SPACE_ABOVE_LINE,
                        jd[pos].bar_length[(int)bar_no],
                        bar_thickness_3);
      }
      else if (bar_no == NO_OF_FILES_DONE_BAR_NO)
           {
              XFillRectangle(display, detailed_window, tr_bar_gc,
                             x + x_offset_tv_bars,
                             y + SPACE_ABOVE_LINE,
                             jd[pos].bar_length[(int)bar_no],
                             bar_thickness_3);
           }
           else if (bar_no == FILE_SIZE_DONE_BAR_NO)
                {
                   gc_values.foreground = color_pool[NORMAL_STATUS];
                   XChangeGC(display, color_gc, GCForeground, &gc_values);
                   XFillRectangle(display, detailed_window, color_gc,
                                  x + x_offset_tv_bars,
                                  y + SPACE_ABOVE_LINE,
                                  jd[pos].bar_length[(int)bar_no],
                                  bar_thickness_3);
                }
                else
                {
                   (void)fprintf(stderr, "ERROR   : Unknown type. (%s %d)\n",
                                 __FILE__, __LINE__);
                   return;
                }
   }

   return;
}


/*+++++++++++++++++++++++++++ tv_locate_xy() ++++++++++++++++++++++++++++*/
void
tv_locate_xy(int pos, int *x, int *y)
{
   int   column_no;

   /* First check that we do not divide by zero. */
   if (tv_no_of_rows <= 1)
   {
      column_no = (pos + 1);
   }
   else
   {
      column_no = (pos + 1) / tv_no_of_rows;
   }

   if (((pos + 1) % tv_no_of_rows) != 0)
   {
      column_no += 1;
      *y = line_height * (pos % tv_no_of_rows);
   }
   else
   {
      *y = line_height * (tv_no_of_rows - 1);
   }

   if (column_no > 1)
   {
      *x = (column_no - 1) * tv_line_length;
   }
   else
   {
      *x = 0;
   }

   return;
}
