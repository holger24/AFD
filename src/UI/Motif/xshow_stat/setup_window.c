/*
 *  setup_window.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   void setup_window(char *font_name)
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
 **   19.04.2001 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>            /* fprintf(), stderr                       */
#include <stdlib.h>           /* exit()                                  */
#include <math.h>             /* log10()                                 */
#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include <errno.h>
#include "xshow_stat.h"

extern Display        *display;
extern XFontStruct    *font_struct;
extern XmFontList     fontlist;
extern GC             letter_gc,
                      normal_letter_gc,
                      color_letter_gc,
                      default_bg_gc,
                      normal_bg_gc,
                      button_bg_gc,
                      color_gc,
                      black_line_gc,
                      white_line_gc;
extern int            no_of_chars,
                      no_of_hosts,
                      line_length,
                      line_height,
                      time_type,
                      x_data_spacing,
                      x_offset_left_xaxis,
                      x_offset_right_xaxis,
                      y_data_spacing,
                      y_offset_top_yaxis,
                      y_offset_bottom_yaxis,
                      y_offset_xaxis;
extern unsigned int   glyph_height,
                      glyph_width;
extern unsigned long  color_pool[];
extern char           stat_type;
extern struct afdstat *stat_db;


/*########################### setup_window() ###########################*/
void
setup_window(char *font_name)
{
   XmFontListEntry entry;

   /* Get width and height of font and fid for the GC. */
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

   glyph_height          = font_struct->ascent + font_struct->descent;
   glyph_width           = font_struct->per_char->width;
   y_offset_xaxis        = font_struct->ascent + 4;
   x_offset_left_xaxis   = 9 * glyph_width;
   x_offset_right_xaxis  = 2 * glyph_width;
   y_offset_top_yaxis    = glyph_height;
   y_offset_bottom_yaxis = 5 * glyph_height;

   if (time_type == YEAR_STAT)
   {
      no_of_chars = 4;
   }
   else
   {
      no_of_chars = 3;
   }
   x_data_spacing = no_of_chars * glyph_width;
   y_data_spacing = glyph_height;

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

   /* GC for drawing the button background. */
   gc_values.foreground = color_pool[BUTTON_BACKGROUND];
   button_bg_gc = XCreateGC(display, window, (XtGCMask) GCForeground,
                            &gc_values);
   XSetFunction(display, button_bg_gc, GXcopy);

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

   /* Flush buffers so all GC's are known. */
   XFlush(display);

   return;
}
