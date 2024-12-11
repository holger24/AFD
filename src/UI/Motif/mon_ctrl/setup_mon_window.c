/*
 *  setup_mon_window.c - Part of AFD, an automatic file distribution
 *                       program.
 *  Copyright (c) 1998 - 2024 Deutscher Wetterdienst (DWD),
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
 **   setup_mon_window - determines the initial size for the window
 **
 ** SYNOPSIS
 **   void setup_mon_window(char *font_name)
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
 **   02.09.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>            /* fprintf(), stderr                       */
#include <stdlib.h>           /* exit()                                  */
#include <math.h>             /* log10()                                 */
#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include <errno.h>
#include "mon_ctrl.h"
#include "permission.h"

extern Display                 *display;
extern XFontStruct             *font_struct;
extern XmFontList              fontlist;
extern Widget                  mw[],
                               ow[],
                               tw[],
                               vw[],
                               cw[],
                               sw[],
                               hw[],
                               rw[],
                               hlw[],
                               lw[],
                               lsw[],
                               oow[],
                               pw[];
extern GC                      letter_gc,
                               normal_letter_gc,
                               locked_letter_gc,
                               color_letter_gc,
                               default_bg_gc,
                               normal_bg_gc,
                               locked_bg_gc,
                               label_bg_gc,
                               button_bg_gc,
                               red_color_letter_gc,
                               red_error_letter_gc,
                               tr_bar_gc,
                               color_gc,
                               black_line_gc,
                               white_line_gc,
                               led_gc;
extern float                   max_bar_length;
extern int                     no_of_afds,
                               line_length,
                               line_height,
                               have_groups,
                               his_log_set,
                               log_angle,
                               no_of_columns,
                               bar_thickness_3,
                               x_center_log_status,
                               x_offset_log_status,
                               x_offset_log_history,
                               x_offset_led,
                               x_offset_bars,
                               x_offset_characters,
                               x_offset_ec,
                               x_offset_eh,
                               y_center_log,
                               y_offset_led;
extern unsigned int            glyph_height,
                               glyph_width,
                               text_offset;
extern unsigned short          step_size;
extern unsigned long           color_pool[];
extern struct coord            coord[];
extern char                    line_style,
                               *ping_cmd,
                               *traceroute_cmd;
extern struct mon_line         *connect_data;
extern struct mon_status_area  *msa;
extern struct mon_control_perm mcp;


/*######################### setup_mon_window() #########################*/
void
setup_mon_window(char *font_name)
{
   int             i,
                   his_log_length,
                   led_width,
                   new_max_bar_length;
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
       (void)fprintf(stderr,
                     "Failed to load font with XmFontListEntryLoad() : %s (%s %d)\n",
                     strerror(errno), __FILE__, __LINE__);
       exit(INCORRECT);
   }
   fontlist = XmFontListAppendEntry(NULL, entry);
   XmFontListEntryFree(&entry);

   if (line_height != 0)
   {
      /* Set the font for the monitor pulldown. */
      XtVaSetValues(mw[MON_W], XmNfontList, fontlist, NULL);
      if ((mcp.show_ms_log != NO_PERMISSION) ||
          (mcp.show_mon_log != NO_PERMISSION) ||
          (mcp.mon_info != NO_PERMISSION) ||
          (mcp.retry != NO_PERMISSION) ||
          (mcp.switch_afd != NO_PERMISSION) ||
          (mcp.disable != NO_PERMISSION))
      {
         if (mcp.show_ms_log != NO_PERMISSION)
         {
            XtVaSetValues(ow[MON_SYS_LOG_W], XmNfontList, fontlist, NULL);
            XtVaSetValues(pw[0], XmNfontList, fontlist, NULL);
         }
         if (mcp.show_mon_log != NO_PERMISSION)
         {
            XtVaSetValues(ow[MON_LOG_W], XmNfontList, fontlist, NULL);
            XtVaSetValues(pw[1], XmNfontList, fontlist, NULL);
         }
         if (mcp.mon_info != NO_PERMISSION)
         {
            XtVaSetValues(ow[MON_INFO_W], XmNfontList, fontlist, NULL);
            XtVaSetValues(pw[4], XmNfontList, fontlist, NULL);
         }
         if (mcp.retry != NO_PERMISSION)
         {
            XtVaSetValues(ow[MON_RETRY_W], XmNfontList, fontlist, NULL);
            XtVaSetValues(pw[2], XmNfontList, fontlist, NULL);
         }
         if (mcp.switch_afd != NO_PERMISSION)
         {
            XtVaSetValues(ow[MON_SWITCH_W], XmNfontList, fontlist, NULL);
            XtVaSetValues(pw[3], XmNfontList, fontlist, NULL);
         }
         if (mcp.disable != NO_PERMISSION)
         {
            XtVaSetValues(ow[MON_DISABLE_W], XmNfontList, fontlist, NULL);
            XtVaSetValues(pw[5], XmNfontList, fontlist, NULL);
         }
         if ((ping_cmd != NULL) || (traceroute_cmd != NULL))
         {
            XtVaSetValues(ow[MON_TEST_W], XmNfontList, fontlist, NULL);
            if (ping_cmd != NULL)
            {
               XtVaSetValues(tw[PING_W], XmNfontList, fontlist, NULL);
            }
            if (traceroute_cmd != NULL)
            {
               XtVaSetValues(tw[TRACEROUTE_W], XmNfontList, fontlist, NULL);
            }
         }
      }
      XtVaSetValues(ow[MON_SELECT_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(ow[MON_EXIT_W], XmNfontList, fontlist, NULL);

      /* Set the font for the RAFD pulldown. */
      if ((mcp.afd_ctrl != NO_PERMISSION) ||
          (mcp.show_slog != NO_PERMISSION) ||
          (mcp.show_elog != NO_PERMISSION) ||
          (mcp.show_rlog != NO_PERMISSION) ||
          (mcp.show_tlog != NO_PERMISSION) ||
          (mcp.show_ilog != NO_PERMISSION) ||
          (mcp.show_plog != NO_PERMISSION) ||
          (mcp.show_olog != NO_PERMISSION) ||
          (mcp.show_dlog != NO_PERMISSION) ||
          (mcp.show_queue != NO_PERMISSION) ||
          (mcp.afd_load != NO_PERMISSION))
      {
         XtVaSetValues(mw[LOG_W], XmNfontList, fontlist, NULL);
         if (mcp.afd_ctrl != NO_PERMISSION)
         {
            XtVaSetValues(vw[MON_AFD_CTRL_W], XmNfontList, fontlist, NULL);
            XtVaSetValues(pw[6], XmNfontList, fontlist, NULL);
         }
         if (mcp.show_slog != NO_PERMISSION)
         {
            XtVaSetValues(vw[MON_SYSTEM_W], XmNfontList, fontlist, NULL);
            XtVaSetValues(pw[8], XmNfontList, fontlist, NULL);
         }
         if (mcp.show_elog != NO_PERMISSION)
         {
            XtVaSetValues(vw[MON_EVENT_W], XmNfontList, fontlist, NULL);
         }
         if (mcp.show_rlog != NO_PERMISSION)
         {
            XtVaSetValues(vw[MON_RECEIVE_W], XmNfontList, fontlist, NULL);
            XtVaSetValues(pw[7], XmNfontList, fontlist, NULL);
         }
         if (mcp.show_tlog != NO_PERMISSION)
         {
            XtVaSetValues(vw[MON_TRANS_W], XmNfontList, fontlist, NULL);
            XtVaSetValues(pw[9], XmNfontList, fontlist, NULL);
         }
         if (mcp.show_ilog != NO_PERMISSION)
         {
            XtVaSetValues(vw[MON_INPUT_W], XmNfontList, fontlist, NULL);
         }
         if (mcp.show_plog != NO_PERMISSION)
         {
            XtVaSetValues(vw[MON_PRODUCTION_W], XmNfontList, fontlist, NULL);
         }
         if (mcp.show_olog != NO_PERMISSION)
         {
            XtVaSetValues(vw[MON_OUTPUT_W], XmNfontList, fontlist, NULL);
         }
         if (mcp.show_dlog != NO_PERMISSION)
         {
            XtVaSetValues(vw[MON_DELETE_W], XmNfontList, fontlist, NULL);
         }
         if (mcp.show_queue != NO_PERMISSION)
         {
            XtVaSetValues(vw[MON_SHOW_QUEUE_W], XmNfontList, fontlist, NULL);
         }
         if (mcp.afd_load != NO_PERMISSION)
         {
            XtVaSetValues(vw[MON_VIEW_LOAD_W], XmNfontList, fontlist, NULL);
            XtVaSetValues(lw[FILE_LOAD_W], XmNfontList, fontlist, NULL);
            XtVaSetValues(lw[KBYTE_LOAD_W], XmNfontList, fontlist, NULL);
            XtVaSetValues(lw[CONNECTION_LOAD_W], XmNfontList, fontlist, NULL);
            XtVaSetValues(lw[TRANSFER_LOAD_W], XmNfontList, fontlist, NULL);
         }
      }

      /* Set the font for the Control pulldown. */
      if ((mcp.amg_ctrl != NO_PERMISSION) ||
          (mcp.fd_ctrl != NO_PERMISSION) ||
          (mcp.rr_dc != NO_PERMISSION) ||
          (mcp.rr_hc != NO_PERMISSION) ||
          (mcp.edit_hc != NO_PERMISSION) ||
          (mcp.dir_ctrl != NO_PERMISSION) ||
          (mcp.startup_afd != NO_PERMISSION) ||
          (mcp.shutdown_afd != NO_PERMISSION))
      {
         XtVaSetValues(mw[CONTROL_W], XmNfontList, fontlist, NULL);
         if (mcp.amg_ctrl != NO_PERMISSION)
         {
            XtVaSetValues(cw[AMG_CTRL_W], XmNfontList, fontlist, NULL);
         }
         if (mcp.fd_ctrl != NO_PERMISSION)
         {
            XtVaSetValues(cw[FD_CTRL_W], XmNfontList, fontlist, NULL);
         }
         if (mcp.rr_dc != NO_PERMISSION)
         {
            XtVaSetValues(cw[RR_DC_W], XmNfontList, fontlist, NULL);
         }
         if (mcp.rr_hc != NO_PERMISSION)
         {
            XtVaSetValues(cw[RR_HC_W], XmNfontList, fontlist, NULL);
         }
         if (mcp.edit_hc != NO_PERMISSION)
         {
            XtVaSetValues(cw[EDIT_HC_W], XmNfontList, fontlist, NULL);
         }
         if (mcp.dir_ctrl != NO_PERMISSION)
         {
            XtVaSetValues(cw[DIR_CTRL_W], XmNfontList, fontlist, NULL);
         }
         if (mcp.startup_afd != NO_PERMISSION)
         {
            XtVaSetValues(cw[STARTUP_AFD_W], XmNfontList, fontlist, NULL);
         }
         if (mcp.shutdown_afd != NO_PERMISSION)
         {
            XtVaSetValues(cw[SHUTDOWN_AFD_W], XmNfontList, fontlist, NULL);
         }
      }

      /* Set the font for the Setup pulldown. */
      XtVaSetValues(mw[CONFIG_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(sw[MON_FONT_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(sw[MON_ROWS_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(sw[MON_STYLE_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(sw[MON_HISTORY_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(sw[MON_OTHER_W], XmNfontList, fontlist, NULL);
      if (have_groups == YES)
      {
         XtVaSetValues(sw[MON_OPEN_ALL_GROUPS_W], XmNfontList, fontlist, NULL);
         XtVaSetValues(sw[MON_CLOSE_ALL_GROUPS_W], XmNfontList, fontlist, NULL);
      }
      XtVaSetValues(sw[MON_SAVE_W], XmNfontList, fontlist, NULL);

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
      XtVaSetValues(rw[ROW_20_W], XmNfontList, fontlist, NULL);

      /* Set the font for the Line Style pulldown. */
      XtVaSetValues(lsw[STYLE_0_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(lsw[STYLE_1_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(lsw[STYLE_2_W], XmNfontList, fontlist, NULL);

      /* Set the font for the history pulldown. */
      for (i = 0; i < NO_OF_HISTORY_LOGS; i++)
      {
         XtVaSetValues(hlw[i], XmNfontList, fontlist, NULL);
      }

      /* Set the font for the Other options pulldown. */
      XtVaSetValues(oow[FORCE_SHIFT_SELECT_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(oow[AUTO_SAVE_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(oow[FRAMED_GROUPS_W], XmNfontList, fontlist, NULL);
   }

   glyph_height       = font_struct->ascent + font_struct->descent;
   glyph_width        = font_struct->per_char->width;
   new_max_bar_length = glyph_width * BAR_LENGTH_MODIFIER;

   /* We now have to recalculate the length of all */
   /* bars and the scale, because a font change    */
   /* might have occurred.                         */
   if (new_max_bar_length != max_bar_length)
   {
      unsigned int new_bar_length;

      max_bar_length = new_max_bar_length;
      step_size = MAX_INTENSITY / max_bar_length;

      /* NOTE: We do not care what the line style is because the */
      /*       following could happen: font size = 7x13 style =  */
      /*       chars + bars, the user now wants chars only and   */
      /*       then reduces the font to 5x7. After a while he    */
      /*       wants the bars again. Thus we always need to re-  */
      /*       calculate the bar length and queue scale!         */
      for (i = 0; i < no_of_afds; i++)
      {
         /* Calculate new scale for active transfers bar. */
         if (connect_data[i].max_connections < 1)
         {
            connect_data[i].scale[ACTIVE_TRANSFERS_BAR_NO - 1] = (double)max_bar_length;
         }
         else
         {
            connect_data[i].scale[ACTIVE_TRANSFERS_BAR_NO - 1] = max_bar_length / connect_data[i].max_connections;
         }
         if (connect_data[i].no_of_transfers == 0)
         {
            new_bar_length = 0;
         }
         else if (connect_data[i].no_of_transfers >= connect_data[i].max_connections)
              {
                 new_bar_length = max_bar_length;
              }
              else
              {
                 new_bar_length = connect_data[i].no_of_transfers * connect_data[i].scale[ACTIVE_TRANSFERS_BAR_NO - 1];
              }
         if (new_bar_length >= max_bar_length)
         {
            connect_data[i].bar_length[ACTIVE_TRANSFERS_BAR_NO] = max_bar_length;
            connect_data[i].blue_color_offset = MAX_INTENSITY;
            connect_data[i].green_color_offset = 0;
         }
         else
         {
            connect_data[i].bar_length[ACTIVE_TRANSFERS_BAR_NO] = new_bar_length;
            connect_data[i].blue_color_offset = new_bar_length * step_size;
            connect_data[i].green_color_offset = MAX_INTENSITY - connect_data[i].blue_color_offset;
         }

         /* Calculate new scale for error bar. */
         if (connect_data[i].no_of_hosts < 1)
         {
            connect_data[i].scale[HOST_ERROR_BAR_NO - 1] = (double)max_bar_length;
         }
         else
         {
            connect_data[i].scale[HOST_ERROR_BAR_NO - 1] = max_bar_length / connect_data[i].no_of_hosts;
         }
         if (connect_data[i].host_error_counter == 0)
         {
            connect_data[i].bar_length[HOST_ERROR_BAR_NO] = 0;
         }
         else if (connect_data[i].host_error_counter >= connect_data[i].no_of_hosts)
              {
                 connect_data[i].bar_length[HOST_ERROR_BAR_NO] = max_bar_length;
              }
              else
              {
                 connect_data[i].bar_length[HOST_ERROR_BAR_NO] = connect_data[i].host_error_counter *
                                                                 connect_data[i].scale[HOST_ERROR_BAR_NO - 1];
              }

         /* Calculate new bar length for the transfer rate. */
         if (connect_data[i].average_tr > 1.0)
         {
            /* First ensure we do not divide by zero. */
            if (connect_data[i].max_average_tr < 2.0)
            {
               connect_data[i].bar_length[MON_TR_BAR_NO] =
                 log10(connect_data[i].average_tr) *
                 max_bar_length /
                 log10((double) 2.0);
            }
            else
            {
               connect_data[i].bar_length[MON_TR_BAR_NO] =
                 log10(connect_data[i].average_tr) *
                 max_bar_length /
                 log10(connect_data[i].max_average_tr);
            }
         }
         else
         {
            connect_data[i].bar_length[MON_TR_BAR_NO] = 0;
         }
      } /* for (i = 0; i < no_of_afds; i++) */
   }

   text_offset     = font_struct->ascent;
   line_height     = SPACE_ABOVE_LINE + glyph_height + SPACE_BELOW_LINE;
   bar_thickness_3 = glyph_height / 3;
   y_offset_led    = (glyph_height - glyph_width) / 2;
   led_width       = glyph_height / 3;
   y_center_log    = SPACE_ABOVE_LINE + (glyph_height / 2);
   if (his_log_set > 0)
   {
      his_log_length  = (his_log_set * bar_thickness_3) + DEFAULT_FRAME_SPACE;
   }
   else
   {
      his_log_length  = 0;
   }
   line_length     = DEFAULT_FRAME_SPACE +
                     (MAX_AFDNAME_LENGTH * glyph_width) +
                     DEFAULT_FRAME_SPACE +
                     (3 * (led_width + PROC_LED_SPACING)) +
                     glyph_height + (glyph_height / 2) + DEFAULT_FRAME_SPACE +
                     his_log_length + DEFAULT_FRAME_SPACE;

   if (line_style == BARS_ONLY)
   {
      line_length += (int)max_bar_length + DEFAULT_FRAME_SPACE;
   }
   else  if (line_style == CHARACTERS_ONLY)
         {
            line_length += (32 * glyph_width) + DEFAULT_FRAME_SPACE;
         }
         else
         {
            line_length += (32 * glyph_width) + DEFAULT_FRAME_SPACE +
                           (int)max_bar_length + DEFAULT_FRAME_SPACE;
         }

   x_offset_led = DEFAULT_FRAME_SPACE + (MAX_AFDNAME_LENGTH * glyph_width) +
                  DEFAULT_FRAME_SPACE;
   x_offset_log_status = x_offset_led + (3 * (led_width + PROC_LED_SPACING)) +
                         (glyph_height / 2) + DEFAULT_FRAME_SPACE;
   x_center_log_status = x_offset_log_status + (glyph_height / 2);
   x_offset_log_history = x_offset_log_status + glyph_height +
                          DEFAULT_FRAME_SPACE;
   for (i = 0; i < LOG_FIFO_SIZE; i++)
   {
      coord[i].x = x_center_log_status +
                   (int)((glyph_height / 2) *
                   cos((double)((log_angle * i * PI) / 180)));
      coord[i].y = y_center_log -
                   (int)((glyph_height / 2) *
                   sin((double)((log_angle * i * PI) / 180)));
   }
   if (line_style == BARS_ONLY)
   {
      x_offset_bars = x_offset_log_history + his_log_length;
   }
   else  if (line_style == CHARACTERS_ONLY)
         {
            x_offset_characters = x_offset_log_history + his_log_length;
            x_offset_ec = x_offset_characters + (27 * glyph_width);
            x_offset_eh = x_offset_ec + (3 *glyph_width);
         }
         else
         {
            x_offset_characters = x_offset_log_history + his_log_length;
            x_offset_bars = x_offset_characters + (32 * glyph_width) +
                            DEFAULT_FRAME_SPACE;
            x_offset_ec = x_offset_characters + (27 * glyph_width);
            x_offset_eh = x_offset_ec + (3 *glyph_width);
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

   /* GC for drawing error letters for EH counters. */
   gc_values.font = font_struct->fid;
   gc_values.foreground = color_pool[WHITE];
   gc_values.background = color_pool[ERROR_ID];
   red_error_letter_gc = XCreateGC(display, window, (XtGCMask) GCFont |
                                   GCForeground | GCBackground, &gc_values);
   XSetFunction(display, red_error_letter_gc, GXcopy);


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
