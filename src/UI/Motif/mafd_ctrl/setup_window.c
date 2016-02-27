/*
 *  setup_window.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2016 Deutscher Wetterdienst (DWD),
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
 **   setup_window - determines the initial size for the window
 **
 ** SYNOPSIS
 **   void setup_window(char *font_name, int redraw_mainmenu)
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
 **   17.01.1996 H.Kiehl Created
 **   30.07.2001 H.Kiehl Support for the show_queue dialog.
 **   22.12.2001 H.Kiehl Added variable column length.
 **   26.12.2001 H.Kiehl Allow for more changes in line style.
 **   13.03.2003 H.Kiehl Added history log in button bar.
 **   25.08.2013 H.Kiehl Added compact process status.
 **
 */
DESCR__E_M3

#include <stdio.h>            /* fprintf(), stderr                       */
#include <stdlib.h>           /* exit()                                  */
#include <math.h>             /* log10()                                 */
#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include <errno.h>
#include "mafd_ctrl.h"
#include "permission.h"

/* External global variables. */
extern Display                    *display;
extern XFontStruct                *font_struct;
extern XmFontList                 fontlist;
extern Widget                     mw[],
                                  ow[],
                                  tw[],
                                  vw[],
                                  cw[],
                                  sw[],
                                  hw[],
                                  rw[],
                                  lw[],
                                  lsw[],
                                  ptw[],
                                  oow[],
                                  pw[];
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
extern float                      max_bar_length;
extern int                        no_of_hosts,
                                  max_line_length,
                                  line_height,
                                  led_width,
                                  hostname_display_length,
                                  bar_thickness_2,
                                  bar_thickness_3,
                                  button_width,
                                  even_height,
                                  x_offset_led,
                                  x_offset_debug_led,
                                  x_offset_proc,
                                  x_offset_bars,
                                  x_offset_characters,
                                  y_offset_led;
extern unsigned int               glyph_height,
                                  glyph_width,
                                  text_offset;
extern unsigned short             step_size;
extern unsigned long              color_pool[];
extern char                       line_style,
                                  *ping_cmd,
                                  *traceroute_cmd;
extern struct line                *connect_data;
extern struct filetransfer_status *fsa;
extern struct afd_control_perm    acp;


/*########################### setup_window() ###########################*/
void
setup_window(char *font_name, int redraw_mainmenu)
{
   int new_max_bar_length;

   if (redraw_mainmenu == YES)
   {
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
      if ((entry = XmFontListEntryLoad(display, font_name, XmFONT_IS_FONT, "TAG1")) == NULL)
      {
          (void)fprintf(stderr, "Failed to load font with XmFontListEntryLoad() : %s (%s %d)\n",
                        strerror(errno), __FILE__, __LINE__);
          exit(INCORRECT);
      }
      fontlist = XmFontListAppendEntry(NULL, entry);
      XmFontListEntryFree(&entry);

      if (line_height != 0)
      {
         /* Set the font for the Host pulldown. */
         XtVaSetValues(mw[HOST_W], XmNfontList, fontlist, NULL);
         if ((acp.handle_event != NO_PERMISSION) ||
             (acp.ctrl_queue != NO_PERMISSION) ||
             (acp.ctrl_transfer != NO_PERMISSION) ||
             (acp.ctrl_queue_transfer != NO_PERMISSION) ||
             (acp.disable != NO_PERMISSION) ||
             (acp.switch_host != NO_PERMISSION) ||
             (acp.retry != NO_PERMISSION) ||
             (acp.debug != NO_PERMISSION) ||
             (acp.trace != NO_PERMISSION) ||
             (acp.full_trace != NO_PERMISSION) ||
             (acp.simulation != NO_PERMISSION) ||
             (ping_cmd != NULL) ||
             (traceroute_cmd != NULL) ||
             (acp.afd_load != NO_PERMISSION))
         {
            if (acp.handle_event != NO_PERMISSION)
            {
               XtVaSetValues(ow[HANDLE_EVENT_W], XmNfontList, fontlist, NULL);
               XtVaSetValues(pw[0], XmNfontList, fontlist, NULL);
            }
            if (acp.ctrl_queue != NO_PERMISSION)
            {
               XtVaSetValues(ow[QUEUE_W], XmNfontList, fontlist, NULL);
               XtVaSetValues(pw[1], XmNfontList, fontlist, NULL);
            }
            if (acp.ctrl_transfer != NO_PERMISSION)
            {
               XtVaSetValues(ow[TRANSFER_W], XmNfontList, fontlist, NULL);
               XtVaSetValues(pw[2], XmNfontList, fontlist, NULL);
            }
            if (acp.ctrl_queue_transfer != NO_PERMISSION)
            {
               XtVaSetValues(ow[QUEUE_TRANSFER_W], XmNfontList, fontlist, NULL);
               XtVaSetValues(pw[3], XmNfontList, fontlist, NULL);
            }
            if (acp.disable != NO_PERMISSION)
            {
               XtVaSetValues(ow[DISABLE_W], XmNfontList, fontlist, NULL);
               XtVaSetValues(pw[4], XmNfontList, fontlist, NULL);
            }
            if (acp.switch_host != NO_PERMISSION)
            {
               XtVaSetValues(ow[SWITCH_W], XmNfontList, fontlist, NULL);
               XtVaSetValues(pw[5], XmNfontList, fontlist, NULL);
            }
            if (acp.retry != NO_PERMISSION)
            {
               XtVaSetValues(ow[RETRY_W], XmNfontList, fontlist, NULL);
               XtVaSetValues(pw[6], XmNfontList, fontlist, NULL);
            }
            if (acp.debug != NO_PERMISSION)
            {
               XtVaSetValues(ow[DEBUG_W], XmNfontList, fontlist, NULL);
               XtVaSetValues(pw[7], XmNfontList, fontlist, NULL);
            }
            if (acp.simulation != NO_PERMISSION)
            {
               XtVaSetValues(ow[SIMULATION_W], XmNfontList, fontlist, NULL);
            }
            if ((ping_cmd != NULL) || (traceroute_cmd != NULL))
            {
               XtVaSetValues(ow[TEST_W], XmNfontList, fontlist, NULL);
               if (ping_cmd != NULL)
               {
                  XtVaSetValues(tw[PING_W], XmNfontList, fontlist, NULL);
               }
               if (traceroute_cmd != NULL)
               {
                  XtVaSetValues(tw[TRACEROUTE_W], XmNfontList, fontlist, NULL);
               }
            }
            if (acp.afd_load != NO_PERMISSION)
            {
               XtVaSetValues(ow[VIEW_LOAD_W], XmNfontList, fontlist, NULL);
               XtVaSetValues(lw[FILE_LOAD_W], XmNfontList, fontlist, NULL);
               XtVaSetValues(lw[KBYTE_LOAD_W], XmNfontList, fontlist, NULL);
               XtVaSetValues(lw[CONNECTION_LOAD_W], XmNfontList, fontlist, NULL);
               XtVaSetValues(lw[TRANSFER_LOAD_W], XmNfontList, fontlist, NULL);
            }
         }
         XtVaSetValues(ow[SELECT_W], XmNfontList, fontlist, NULL);
         XtVaSetValues(ow[EXIT_W], XmNfontList, fontlist, NULL);

         /* Set the font for the View pulldown. */
         if ((acp.show_slog != NO_PERMISSION) ||
             (acp.show_mlog != NO_PERMISSION) ||
             (acp.show_elog != NO_PERMISSION) ||
             (acp.show_rlog != NO_PERMISSION) ||
             (acp.show_tlog != NO_PERMISSION) ||
             (acp.show_tdlog != NO_PERMISSION) ||
             (acp.show_ilog != NO_PERMISSION) ||
             (acp.show_olog != NO_PERMISSION) ||
             (acp.show_dlog != NO_PERMISSION) ||
             (acp.show_queue != NO_PERMISSION) ||
             (acp.info != NO_PERMISSION) ||
             (acp.view_dc != NO_PERMISSION) ||
             (acp.view_jobs != NO_PERMISSION))
         {
            XtVaSetValues(mw[LOG_W], XmNfontList, fontlist, NULL);
            if (acp.show_slog != NO_PERMISSION)
            {
               XtVaSetValues(vw[SYSTEM_W], XmNfontList, fontlist, NULL);
            }
#ifdef _MAINTAINER_LOG
            if (acp.show_mlog != NO_PERMISSION)
            {
               XtVaSetValues(vw[MAINTAINER_W], XmNfontList, fontlist, NULL);
            }
#endif
            if (acp.show_elog != NO_PERMISSION)
            {
               XtVaSetValues(vw[EVENT_W], XmNfontList, fontlist, NULL);
               XtVaSetValues(pw[10], XmNfontList, fontlist, NULL);
            }
            if (acp.show_rlog != NO_PERMISSION)
            {
               XtVaSetValues(vw[RECEIVE_W], XmNfontList, fontlist, NULL);
            }
            if (acp.show_tlog != NO_PERMISSION)
            {
               XtVaSetValues(vw[TRANS_W], XmNfontList, fontlist, NULL);
               XtVaSetValues(pw[11], XmNfontList, fontlist, NULL);
            }
            if (acp.show_tdlog != NO_PERMISSION)
            {
               XtVaSetValues(vw[TRANS_DEBUG_W], XmNfontList, fontlist, NULL);
            }
            if (acp.show_ilog != NO_PERMISSION)
            {
               XtVaSetValues(vw[INPUT_W], XmNfontList, fontlist, NULL);
            }
            if (acp.show_olog != NO_PERMISSION)
            {
               XtVaSetValues(vw[OUTPUT_W], XmNfontList, fontlist, NULL);
               XtVaSetValues(pw[12], XmNfontList, fontlist, NULL);
            }
            if (acp.show_dlog != NO_PERMISSION)
            {
               XtVaSetValues(vw[DELETE_W], XmNfontList, fontlist, NULL);
            }
            if (acp.show_queue != NO_PERMISSION)
            {
               XtVaSetValues(vw[SHOW_QUEUE_W], XmNfontList, fontlist, NULL);
            }
            if (acp.info != NO_PERMISSION)
            {
               XtVaSetValues(vw[INFO_W], XmNfontList, fontlist, NULL);
               XtVaSetValues(pw[8], XmNfontList, fontlist, NULL);
            }
            if (acp.view_dc != NO_PERMISSION)
            {
               XtVaSetValues(vw[VIEW_DC_W], XmNfontList, fontlist, NULL);
               XtVaSetValues(pw[9], XmNfontList, fontlist, NULL);
            }
            if (acp.view_jobs != NO_PERMISSION)
            {
               XtVaSetValues(vw[VIEW_JOB_W], XmNfontList, fontlist, NULL);
            }
         }

         /* Set the font for the Control pulldown. */
         if ((acp.amg_ctrl != NO_PERMISSION) || (acp.fd_ctrl != NO_PERMISSION) ||
             (acp.rr_dc != NO_PERMISSION) || (acp.rr_hc != NO_PERMISSION) ||
             (acp.edit_hc != NO_PERMISSION) ||
             (acp.startup_afd != NO_PERMISSION) ||
             (acp.shutdown_afd != NO_PERMISSION) ||
             (acp.dir_ctrl != NO_PERMISSION))
         {
            XtVaSetValues(mw[CONTROL_W], XmNfontList, fontlist, NULL);
            if (acp.amg_ctrl != NO_PERMISSION)
            {
               XtVaSetValues(cw[AMG_CTRL_W], XmNfontList, fontlist, NULL);
            }
            if (acp.fd_ctrl != NO_PERMISSION)
            {
               XtVaSetValues(cw[FD_CTRL_W], XmNfontList, fontlist, NULL);
            }
            if (acp.rr_dc != NO_PERMISSION)
            {
               XtVaSetValues(cw[RR_DC_W], XmNfontList, fontlist, NULL);
            }
            if (acp.rr_hc != NO_PERMISSION)
            {
               XtVaSetValues(cw[RR_HC_W], XmNfontList, fontlist, NULL);
            }
            if (acp.edit_hc != NO_PERMISSION)
            {
               XtVaSetValues(cw[EDIT_HC_W], XmNfontList, fontlist, NULL);
            }
            if (acp.dir_ctrl != NO_PERMISSION)
            {
               XtVaSetValues(cw[DIR_CTRL_W], XmNfontList, fontlist, NULL);
            }
            if (acp.startup_afd != NO_PERMISSION)
            {
               XtVaSetValues(cw[STARTUP_AFD_W], XmNfontList, fontlist, NULL);
            }
            if (acp.shutdown_afd != NO_PERMISSION)
            {
               XtVaSetValues(cw[SHUTDOWN_AFD_W], XmNfontList, fontlist, NULL);
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
         XtVaSetValues(rw[ROW_17_W], XmNfontList, fontlist, NULL);
         XtVaSetValues(rw[ROW_18_W], XmNfontList, fontlist, NULL);
         XtVaSetValues(rw[ROW_19_W], XmNfontList, fontlist, NULL);

         /* Set the font for the Line Style pulldown. */
         XtVaSetValues(lsw[STYLE_0_W], XmNfontList, fontlist, NULL);
         XtVaSetValues(lsw[STYLE_1_W], XmNfontList, fontlist, NULL);
         XtVaSetValues(ptw[0], XmNfontList, fontlist, NULL);
         XtVaSetValues(ptw[1], XmNfontList, fontlist, NULL);
         XtVaSetValues(ptw[2], XmNfontList, fontlist, NULL);
         XtVaSetValues(lsw[STYLE_2_W], XmNfontList, fontlist, NULL);
         XtVaSetValues(lsw[STYLE_3_W], XmNfontList, fontlist, NULL);

         /* Set the font for the Other options pulldown. */
         XtVaSetValues(oow[FORCE_SHIFT_SELECT_W], XmNfontList, fontlist, NULL);
      }
   }

   glyph_height       = font_struct->ascent + font_struct->descent;
   glyph_width        = font_struct->per_char->width;
   new_max_bar_length = glyph_width * BAR_LENGTH_MODIFIER;

   /* We now have to recalculate the length of all */
   /* bars and the scale, because a font change    */
   /* might have occurred.                         */
   if (new_max_bar_length != max_bar_length)
   {
      int          i;
      unsigned int new_bar_length;

      max_bar_length = new_max_bar_length;
      step_size = MAX_INTENSITY / max_bar_length;

      /* NOTE: We do not care what the line style is because the */
      /*       following could happen: font size = 7x13 style =  */
      /*       chars + bars, the user now wants chars only and   */
      /*       then reduces the font to 5x7. After a while he    */
      /*       wants the bars again. Thus we always need to re-  */
      /*       calculate the bar length and queue scale!         */
      for (i = 0; i < no_of_hosts; i++)
      {
         /* Calculate new scale for error bar. */
         if (connect_data[i].error_counter > 0)
         {
            if (fsa[i].max_errors < 1)
            {
               connect_data[i].scale = (double)max_bar_length;
            }
            else
            {
               connect_data[i].scale = max_bar_length / fsa[i].max_errors;
            }
            new_bar_length = connect_data[i].error_counter * connect_data[i].scale;
            if (new_bar_length >= max_bar_length)
            {
               connect_data[i].bar_length[ERROR_BAR_NO] = max_bar_length;
               connect_data[i].red_color_offset = MAX_INTENSITY;
               connect_data[i].green_color_offset = 0;
            }
            else
            {
               connect_data[i].bar_length[ERROR_BAR_NO] = new_bar_length;
               connect_data[i].red_color_offset = new_bar_length * step_size;
               connect_data[i].green_color_offset = MAX_INTENSITY - connect_data[i].red_color_offset;
            }
         }
         else
         {
            connect_data[i].bar_length[ERROR_BAR_NO] = 0;
            connect_data[i].red_color_offset = 0;
            connect_data[i].green_color_offset = MAX_INTENSITY;
         }

         /* Calculate new bar length for the transfer rate. */
         if (connect_data[i].average_tr > 1.0)
         {
            /* First ensure we do not divide by zero. */
            if (connect_data[i].max_average_tr < 2.0)
            {
               connect_data[i].bar_length[TR_BAR_NO] =
                 log10(connect_data[i].average_tr) *
                 max_bar_length / log10((double) 2.0);
            }
            else
            {
               connect_data[i].bar_length[TR_BAR_NO] =
                 log10(connect_data[i].average_tr) *
                 max_bar_length / log10(connect_data[i].max_average_tr);
            }
         }
         else
         {
            connect_data[i].bar_length[TR_BAR_NO] = 0;
         }
      }
   }

   text_offset     = font_struct->ascent;
   line_height     = SPACE_ABOVE_LINE + glyph_height + SPACE_BELOW_LINE;
   bar_thickness_2 = glyph_height / 2;
   even_height     = glyph_height % 2;
   bar_thickness_3 = glyph_height / 3;
   button_width    = 2 * glyph_width;
   y_offset_led    = (glyph_height - glyph_width) / 2;
   led_width       = glyph_height / 3;
   max_line_length = DEFAULT_FRAME_SPACE + (hostname_display_length * glyph_width) +
                     DEFAULT_FRAME_SPACE;

   x_offset_proc = x_offset_characters = x_offset_bars = max_line_length;
   if (line_style & SHOW_LEDS)
   {
      x_offset_debug_led = max_line_length;
      x_offset_led = x_offset_debug_led + glyph_width + DEFAULT_FRAME_SPACE;
      max_line_length += glyph_width + DEFAULT_FRAME_SPACE +
                         (2 * led_width) + LED_SPACING + DEFAULT_FRAME_SPACE;
      x_offset_proc = x_offset_characters = x_offset_bars = max_line_length;
   }
   else
   {
      x_offset_debug_led = x_offset_led = 0;
   }
   if (line_style & SHOW_JOBS)
   {
      max_line_length += ((MAX_NO_PARALLEL_JOBS *
                           (button_width + BUTTON_SPACING)) - BUTTON_SPACING);
      x_offset_characters = x_offset_bars = max_line_length;
   }
   else if (line_style & SHOW_JOBS_COMPACT)
        {
           if (MAX_NO_PARALLEL_JOBS % 3)
           {
              max_line_length += (((MAX_NO_PARALLEL_JOBS / 3) + 1) * bar_thickness_3) +
                                 BUTTON_SPACING;
           }
           else
           {
              max_line_length += ((MAX_NO_PARALLEL_JOBS / 3) * bar_thickness_3) +
                                 BUTTON_SPACING;
           }
           x_offset_characters = x_offset_bars = max_line_length;
        }
        else
        {
           x_offset_proc = 0;
        }
   if (line_style & SHOW_CHARACTERS)
   {
      max_line_length += (17 * glyph_width) + DEFAULT_FRAME_SPACE;
      x_offset_bars = max_line_length;
   }
   else
   {
      x_offset_characters = 0;
   }
   if (line_style & SHOW_BARS)
   {
      max_line_length += (int)max_bar_length + DEFAULT_FRAME_SPACE;
   }
   else
   {
      x_offset_bars = 0;
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

   /* GC for drawing the unset LED. */
   gc_values.foreground = color_pool[CHAR_BACKGROUND];
   unset_led_bg_gc = XCreateGC(display, window, (XtGCMask) GCForeground,
                            &gc_values);
   XSetFunction(display, unset_led_bg_gc, GXcopy);

   /* GC for drawing the label background. */
   gc_values.foreground = color_pool[LABEL_BG];
   label_bg_gc = XCreateGC(display, window, (XtGCMask) GCForeground,
                           &gc_values);
   XSetFunction(display, label_bg_gc, GXcopy);

   /* GC for drawing the button background. */
   gc_values.foreground = color_pool[BUTTON_BACKGROUND];
   button_bg_gc = XCreateGC(display, window, (XtGCMask) GCForeground,
                            &gc_values);
   XSetFunction(display, button_bg_gc, GXcopy);

   /* GC for drawing the background for "bytes on input" bar. */
   gc_values.foreground = color_pool[TR_BAR];
   tr_bar_gc = XCreateGC(display, window, (XtGCMask) GCForeground,
                            &gc_values);
   XSetFunction(display, tr_bar_gc, GXcopy);

   /* GC for drawing the background for queue bar and leds. */
   gc_values.foreground = color_pool[TR_BAR];
   color_gc = XCreateGC(display, window, (XtGCMask) GCForeground,
                        &gc_values);
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

   /* GC for drawing led's. */
   gc_values.foreground = color_pool[TR_BAR];
   led_gc = XCreateGC(display, window, (XtGCMask) GCForeground,
                      &gc_values);
   XSetFunction(display, led_gc, GXcopy);

   /* Flush buffers so all GC's are known. */
   XFlush(display);

   return;
}
