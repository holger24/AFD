/*
 *  setup_tv_window.c - Part of AFD, an automatic file distribution
 *                      program.
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
 **   setup_tv_window - determines the initial size for the window
 **
 ** SYNOPSIS
 **   void setup_tv_window(void)
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
 **   01.01.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include "mafd_ctrl.h"

extern float        max_bar_length;
extern int          filename_display_length,
                    hostname_display_length,
                    line_height,
                    tv_line_length,
                    x_offset_tv_bars,
                    x_offset_tv_characters,
                    x_offset_tv_file_name,
                    x_offset_tv_job_number,
                    x_offset_tv_priority,
                    x_offset_tv_rotating_dash;
extern unsigned int glyph_height,
                    glyph_width;
extern char         line_style;


/*########################## setup_tv_window() ##########################*/
void
setup_tv_window(void)
{
   x_offset_tv_job_number = DEFAULT_FRAME_SPACE +
                            (hostname_display_length * glyph_width) +
                            DEFAULT_FRAME_SPACE;
   x_offset_tv_priority = x_offset_tv_job_number + glyph_width +
                          DEFAULT_FRAME_SPACE;
   x_offset_tv_file_name = x_offset_tv_priority + glyph_width +
                           DEFAULT_FRAME_SPACE;
   x_offset_tv_rotating_dash = x_offset_tv_file_name +
                               (filename_display_length * glyph_width) +
                               DEFAULT_FRAME_SPACE;
   x_offset_tv_characters = x_offset_tv_rotating_dash + glyph_width +
                            DEFAULT_FRAME_SPACE;
   tv_line_length = x_offset_tv_characters;

   if (line_style & SHOW_CHARACTERS)
   {
      tv_line_length += (29 * glyph_width) + DEFAULT_FRAME_SPACE;
      x_offset_tv_bars = tv_line_length;
   }
   else
   {
      x_offset_tv_bars = x_offset_tv_characters;
   }
   if (line_style & SHOW_BARS)
   {
      tv_line_length += (int)max_bar_length + DEFAULT_FRAME_SPACE;
   }

   return;
}
