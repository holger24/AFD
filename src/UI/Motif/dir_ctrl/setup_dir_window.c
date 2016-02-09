/*
 *  setup_dir_window.c - Part of AFD, an automatic file distribution
 *                       program.
 *  Copyright (c) 2000 - 2015 Deutscher Wetterdienst (DWD),
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
 **   setup_dir_window - determines the initial size for the window
 **
 ** SYNOPSIS
 **   void setup_dir_window(char *font_name)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   31.08.2000 H.Kiehl Created
 **   05.05.2002 H.Kiehl Show the number files currently in the directory.
 **   28.09.2006 H.Kiehl Added error counter.
 **
 */
DESCR__E_M3

#include <stdio.h>            /* fprintf(), stderr                       */
#include <stdlib.h>           /* exit()                                  */
#include <math.h>             /* log10()                                 */
#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include <errno.h>
#include "dir_ctrl.h"
#include "permission.h"

extern Display                  *display;
extern XFontStruct              *font_struct;
extern XmFontList               fontlist;
extern Widget                   mw[],
                                dw[],
                                vw[],
                                sw[],
                                hw[],
                                rw[],
                                lw[],
                                lsw[],
                                oow[];
extern GC                       letter_gc,
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
extern float                    max_bar_length;
extern int                      no_of_dirs,
                                line_length,
                                line_height,
                                no_of_columns,
                                bar_thickness_3,
                                x_offset_bars,
                                x_offset_characters,
                                x_offset_dir_full,
                                x_offset_type;
extern time_t                   now;
extern unsigned int             glyph_height,
                                glyph_width,
                                text_offset;
extern unsigned long            color_pool[];
extern char                     line_style;
extern struct dir_line          *connect_data;
extern struct fileretrieve_area *fra;
extern struct dir_control_perm  dcp;


/*######################### setup_dir_window() #########################*/
void
setup_dir_window(char *font_name)
{
   int             new_max_bar_length;
   XmFontListEntry entry;

   /* Get width and height of font and fid for the GC. */
   if (font_struct != NULL)
   {
      XFreeFont(display, font_struct);
      font_struct = NULL;
   }
   if (fontlist != NULL)
   {
      XmFontListFree(fontlist);
      fontlist = NULL;
   }
   if ((font_struct = XLoadQueryFont(display, font_name)) == NULL)
   {
      (void)fprintf(stderr, "Could not load %s font.\n", font_name);
      if ((font_struct = XLoadQueryFont(display, DEFAULT_FONT)) == NULL)
      {
         (void)fprintf(stderr, "Could not load %s font.\n", DEFAULT_FONT);
         exit(INCORRECT);
      }
      else
      {
         (void)strcpy(font_name, DEFAULT_FONT);
      }
   }
   if ((entry = XmFontListEntryLoad(display, font_name, XmFONT_IS_FONT,
                                    "TAG1")) == NULL)
   {
       (void)fprintf(stderr,
                     "Failed to load font with XmFontListEntryLoad() : %s (%s %d)\n",
                     strerror(errno), __FILE__, __LINE__);
       exit(INCORRECT);
   }
   fontlist = XmFontListAppendEntry(NULL, entry);
   XmFontListEntryFree(&entry);

   if (line_height != 0)
   {
      /* Set the font for the directory pulldown. */
      XtVaSetValues(mw[DIR_W], XmNfontList, fontlist, NULL);
      if (dcp.handle_event != NO_PERMISSION)
      {
         XtVaSetValues(dw[DIR_HANDLE_EVENT_W], XmNfontList, fontlist, NULL);
      }
      if (dcp.stop != NO_PERMISSION)
      {
         XtVaSetValues(dw[DIR_STOP_W], XmNfontList, fontlist, NULL);
      }
      if (dcp.disable != NO_PERMISSION)
      {
         XtVaSetValues(dw[DIR_DISABLE_W], XmNfontList, fontlist, NULL);
      }
      if (dcp.rescan != NO_PERMISSION)
      {
         XtVaSetValues(dw[DIR_RESCAN_W], XmNfontList, fontlist, NULL);
      }
      if (dcp.afd_load != NO_PERMISSION)
      {
         XtVaSetValues(dw[DIR_VIEW_LOAD_W], XmNfontList, fontlist, NULL);
         XtVaSetValues(lw[FILE_LOAD_W], XmNfontList, fontlist, NULL);
         XtVaSetValues(lw[KBYTE_LOAD_W], XmNfontList, fontlist, NULL);
         XtVaSetValues(lw[CONNECTION_LOAD_W], XmNfontList, fontlist, NULL);
         XtVaSetValues(lw[TRANSFER_LOAD_W], XmNfontList, fontlist, NULL);
      }
      XtVaSetValues(dw[DIR_SELECT_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(dw[DIR_EXIT_W], XmNfontList, fontlist, NULL);

      /* Set the font for the View pulldown. */
      if ((dcp.show_slog != NO_PERMISSION) ||
          (dcp.show_elog != NO_PERMISSION) ||
          (dcp.show_rlog != NO_PERMISSION) ||
          (dcp.show_tlog != NO_PERMISSION) ||
          (dcp.show_ilog != NO_PERMISSION) ||
          (dcp.show_olog != NO_PERMISSION) ||
          (dcp.show_dlog != NO_PERMISSION) ||
          (dcp.show_queue != NO_PERMISSION) ||
          (dcp.info != NO_PERMISSION) ||
          (dcp.view_dc != NO_PERMISSION))
      {
         XtVaSetValues(mw[LOG_W], XmNfontList, fontlist, NULL);
         if (dcp.show_slog != NO_PERMISSION)
         {
            XtVaSetValues(vw[DIR_SYSTEM_W], XmNfontList, fontlist, NULL);
         }
         if (dcp.show_elog != NO_PERMISSION)
         {
            XtVaSetValues(vw[DIR_EVENT_W], XmNfontList, fontlist, NULL);
         }
         if (dcp.show_rlog != NO_PERMISSION)
         {
            XtVaSetValues(vw[DIR_RECEIVE_W], XmNfontList, fontlist, NULL);
         }
         if (dcp.show_tlog != NO_PERMISSION)
         {
            XtVaSetValues(vw[DIR_TRANS_W], XmNfontList, fontlist, NULL);
         }
         if (dcp.show_ilog != NO_PERMISSION)
         {
            XtVaSetValues(vw[DIR_INPUT_W], XmNfontList, fontlist, NULL);
         }
         if (dcp.show_olog != NO_PERMISSION)
         {
            XtVaSetValues(vw[DIR_OUTPUT_W], XmNfontList, fontlist, NULL);
         }
         if (dcp.show_dlog != NO_PERMISSION)
         {
            XtVaSetValues(vw[DIR_DELETE_W], XmNfontList, fontlist, NULL);
         }
         if (dcp.show_queue != NO_PERMISSION)
         {
            XtVaSetValues(vw[DIR_SHOW_QUEUE_W], XmNfontList, fontlist, NULL);
         }
         if (dcp.info != NO_PERMISSION)
         {
            XtVaSetValues(vw[DIR_INFO_W], XmNfontList, fontlist, NULL);
         }
         if (dcp.view_dc != NO_PERMISSION)
         {
            XtVaSetValues(vw[DIR_VIEW_DC_W], XmNfontList, fontlist, NULL);
         }
      }

      /* Set the font for the Setup pulldown. */
      XtVaSetValues(mw[CONFIG_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(sw[FONT_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(sw[ROWS_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(sw[STYLE_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(sw[OTHER_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(sw[SAVE_W], XmNfontList, fontlist, NULL);

      /* Set the font for the Help pulldown. */
#ifdef _WITH_HELP_PULLDOWN
      XtVaSetValues(mw[HELP_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(hw[ABOUT_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(hw[HYPER_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(hw[VERSION_W], XmNfontList, fontlist, NULL);
#endif

      /* Set the font for the Row pulldown. */
      XtVaSetValues(rw[ROW_0_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(rw[ROW_1_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(rw[ROW_2_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(rw[ROW_3_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(rw[ROW_4_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(rw[ROW_5_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(rw[ROW_6_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(rw[ROW_7_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(rw[ROW_8_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(rw[ROW_9_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(rw[ROW_10_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(rw[ROW_11_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(rw[ROW_12_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(rw[ROW_13_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(rw[ROW_14_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(rw[ROW_15_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(rw[ROW_16_W], XmNfontList, fontlist, NULL);

      /* Set the font for the Line Style pulldown. */
      XtVaSetValues(lsw[STYLE_0_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(lsw[STYLE_1_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(lsw[STYLE_2_W], XmNfontList, fontlist, NULL);

      /* Set the font for the Other options pulldown. */
      XtVaSetValues(oow[FORCE_SHIFT_SELECT_W], XmNfontList, fontlist, NULL);
   }

   glyph_height       = font_struct->ascent + font_struct->descent;
   glyph_width        = font_struct->per_char->width;
   new_max_bar_length = glyph_width * BAR_LENGTH_MODIFIER;

   /* We now have to recalculate the length of all    */
   /* bars because a font change might have occurred. */
   if (new_max_bar_length != max_bar_length)
   {
      int          i;
      unsigned int new_bar_length;

      max_bar_length = new_max_bar_length;

      /* NOTE: We do not care what the line style is because the */
      /*       following could happen: font size = 7x13 style =  */
      /*       chars + bars, the user now wants chars only and   */
      /*       then reduces the font to 5x7. After a while he    */
      /*       wants the bars again. Thus we always need to re-  */
      /*       calculate the bar length!                         */
      for (i = 0; i < no_of_dirs; i++)
      {
         /* Calculate new bar length for file rate. */
         if (connect_data[i].average_fr > 1.0)
         {
            /* First ensure we do not divide by zero. */
            if (connect_data[i].max_average_fr < 2.0)
            {
               connect_data[i].bar_length[FILE_RATE_BAR_NO] =
                 log10(connect_data[i].average_fr) * max_bar_length /
                 log10((double) 2.0);
            }
            else
            {
               connect_data[i].bar_length[FILE_RATE_BAR_NO] =
                 log10(connect_data[i].average_fr) * max_bar_length /
                 log10(connect_data[i].max_average_fr);
            }
         }
         else
         {
            connect_data[i].bar_length[FILE_RATE_BAR_NO] = 0;
         }

         /* Calculate new bar length for directory warn time. */
         if (connect_data[i].warn_time < 1)
         {
            connect_data[i].scale = 0.0;
            connect_data[i].bar_length[TIME_UP_BAR_NO] = 0;
         }
         else
         {
            connect_data[i].scale = max_bar_length / connect_data[i].warn_time;
            new_bar_length = (now - connect_data[i].last_retrieval) * connect_data[i].scale;
            if (new_bar_length > 0)
            {
               if (new_bar_length >= max_bar_length)
               {
                  connect_data[i].bar_length[TIME_UP_BAR_NO] = max_bar_length;
               }
               else
               {
                  connect_data[i].bar_length[TIME_UP_BAR_NO] = new_bar_length;
               }
            }
            else
            {
               connect_data[i].bar_length[TIME_UP_BAR_NO] = 0;
            }
         }

         /* Calculate new bar length for file rate. */
         if (connect_data[i].average_tr > 1.0)
         {
            /* First ensure we do not divide by zero. */
            if (connect_data[i].max_average_tr < 2.0)
            {
               connect_data[i].bar_length[BYTE_RATE_BAR_NO] =
                 log10(connect_data[i].average_tr) * max_bar_length /
                 log10((double) 2.0);
            }
            else
            {
               connect_data[i].bar_length[BYTE_RATE_BAR_NO] =
                 log10(connect_data[i].average_tr) * max_bar_length /
                 log10(connect_data[i].max_average_tr);
            }
         }
         else
         {
            connect_data[i].bar_length[BYTE_RATE_BAR_NO] = 0;
         }
      } /* for (i = 0; i < no_of_dirs; i++) */
   }

   text_offset       = font_struct->ascent;
   line_height       = SPACE_ABOVE_LINE + glyph_height + SPACE_BELOW_LINE;
   bar_thickness_3   = glyph_height / 3;
   x_offset_dir_full = DEFAULT_FRAME_SPACE +
                       (MAX_DIR_ALIAS_LENGTH * glyph_width) +
                       DEFAULT_FRAME_SPACE;
   x_offset_type     = x_offset_dir_full + (1 * glyph_width) +
                       DEFAULT_FRAME_SPACE;
   line_length       = x_offset_type + (3 * glyph_width) +
                       DEFAULT_FRAME_SPACE + DEFAULT_FRAME_SPACE;

   if (line_style == BARS_ONLY)
   {
      x_offset_bars = line_length;
      line_length += (int)max_bar_length + DEFAULT_FRAME_SPACE;
   }
   else  if (line_style == CHARACTERS_ONLY)
         {
            x_offset_characters = line_length;
            line_length += (35 * glyph_width) + DEFAULT_FRAME_SPACE;
         }
         else
         {
            x_offset_characters = line_length;
            x_offset_bars = line_length + (35 * glyph_width) +
                            DEFAULT_FRAME_SPACE;
            line_length = x_offset_bars + (int)max_bar_length +
                          DEFAULT_FRAME_SPACE;
         }

   return;
}


/*############################# init_gcs() #############################*/
void
init_gcs(void)
{
   XGCValues  gc_values;
   Window     window = RootWindow(display, DefaultScreen(display));

   /* GC for drawing letters on default background. */
   gc_values.font = font_struct->fid;
   gc_values.foreground = color_pool[FG];
   gc_values.background = color_pool[DEFAULT_BG];
   letter_gc = XCreateGC(display, window, GCFont | GCForeground |
                         GCBackground, &gc_values);
   XSetFunction(display, letter_gc, GXcopy);

   /* GC for drawing letters for normal selection. */
   gc_values.font = font_struct->fid;
   gc_values.foreground = color_pool[WHITE];
   gc_values.background = color_pool[BLACK];
   normal_letter_gc = XCreateGC(display, window, (XtGCMask) GCFont |
                                GCForeground | GCBackground, &gc_values);
   XSetFunction(display, normal_letter_gc, GXcopy);

   /* GC for drawing letters for locked selection. */
   gc_values.font = font_struct->fid;
   gc_values.foreground = color_pool[WHITE];
   gc_values.background = color_pool[LOCKED_INVERSE];
   locked_letter_gc = XCreateGC(display, window, (XtGCMask) GCFont |
                                GCForeground | GCBackground, &gc_values);
   XSetFunction(display, locked_letter_gc, GXcopy);

   /* GC for drawing letters for host name. */
   gc_values.font = font_struct->fid;
   gc_values.foreground = color_pool[FG];
   gc_values.background = color_pool[WHITE];
   color_letter_gc = XCreateGC(display, window, (XtGCMask) GCFont |
                               GCForeground | GCBackground, &gc_values);
   XSetFunction(display, color_letter_gc, GXcopy);

   /* GC for drawing error letters for EC counters. */
   gc_values.font = font_struct->fid;
   gc_values.foreground = color_pool[NOT_WORKING];
   red_color_letter_gc = XCreateGC(display, window, (XtGCMask) GCFont |
                                   GCForeground, &gc_values);
   XSetFunction(display, red_color_letter_gc, GXcopy);

   /* GC for drawing the default background. */
   gc_values.foreground = color_pool[DEFAULT_BG];
   default_bg_gc = XCreateGC(display, window, (XtGCMask) GCForeground,
                             &gc_values);
   XSetFunction(display, default_bg_gc, GXcopy);

   /* GC for drawing the normal selection background. */
   gc_values.foreground = color_pool[BLACK];
   normal_bg_gc = XCreateGC(display, window, (XtGCMask) GCForeground,
                            &gc_values);
   XSetFunction(display, normal_bg_gc, GXcopy);

   /* GC for drawing the locked selection background. */
   gc_values.foreground = color_pool[LOCKED_INVERSE];
   locked_bg_gc = XCreateGC(display, window, (XtGCMask) GCForeground,
                            &gc_values);
   XSetFunction(display, locked_bg_gc, GXcopy);

   /* GC for drawing the label background. */
   gc_values.foreground = color_pool[LABEL_BG];
   label_bg_gc = XCreateGC(display, window, (XtGCMask) GCForeground,
                           &gc_values);
   XSetFunction(display, label_bg_gc, GXcopy);

   /* GC for drawing the background for "bytes on input" bar. */
   gc_values.foreground = color_pool[TR_BAR];
   tr_bar_gc = XCreateGC(display, window, (XtGCMask) GCForeground,
                         &gc_values);
   XSetFunction(display, tr_bar_gc, GXcopy);

   /* GC for drawing the background for "directory time up" bar. */
   gc_values.foreground = color_pool[WARNING_ID];
   tu_bar_gc = XCreateGC(display, window, (XtGCMask) GCForeground,
                         &gc_values);
   XSetFunction(display, tu_bar_gc, GXcopy);

   /* GC for drawing the background for "files on input" bar. */
   gc_values.foreground = color_pool[NORMAL_STATUS];
   fr_bar_gc = XCreateGC(display, window, (XtGCMask) GCForeground, &gc_values);
   XSetFunction(display, fr_bar_gc, GXcopy);

   /* GC for drawing the background for queue bar and leds. */
   gc_values.foreground = color_pool[TR_BAR];
   color_gc = XCreateGC(display, window, (XtGCMask) GCForeground, &gc_values);
   XSetFunction(display, color_gc, GXcopy);

   /* GC for drawing the black lines. */
   gc_values.foreground = color_pool[BLACK];
   black_line_gc = XCreateGC(display, window, (XtGCMask) GCForeground,
                             &gc_values);
   XSetFunction(display, black_line_gc, GXcopy);

   /* GC for drawing the white lines. */
   gc_values.foreground = color_pool[WHITE];
   white_line_gc = XCreateGC(display, window, (XtGCMask) GCForeground,
                             &gc_values);
   XSetFunction(display, white_line_gc, GXcopy);

   /* Flush buffers so all GC's are known. */
   XFlush(display);

   return;
}
