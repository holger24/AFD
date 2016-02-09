/*
 *  init_color.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   lookup_color - searches cache for color
 **
 ** SYNOPSIS
 **   void lookup_color(XColor *color)
 **
 ** DESCRIPTION
 **   This function lookup_color() stores colors in a local static
 **   buffer if it has not yet been previously alocated with
 **   XAllocColor(). If the color is in this buffer it sets
 **   color.pixel and returns.
 **
 ** RETURN VALUES
 **   Returns the pixel value Xcolor color structure.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   24.09.2005 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <errno.h>
#include "ui_common_defs.h"

/* External global variables. */
extern Display       *display;
extern Colormap      default_cmap;
extern unsigned long color_pool[];


/*########################### lookup_color() ############################*/
void
lookup_color(XColor *color)
{
   int           i;
   static int    stored_colors = 0;
   static XColor *cdb = NULL;

   for (i = 0; i < stored_colors; i++)
   {
      if ((cdb[i].red == color->red) &&
          (cdb[i].blue == color->blue) &&
          (cdb[i].green == color->green))
      {
         color->pixel = cdb[i].pixel;
         return;
      }
   }
   if ((stored_colors % 10) == 0)
   {
      size_t new_size;

      new_size = ((stored_colors / 10) + 1) * 10 * sizeof(XColor);
      if ((cdb = realloc(cdb, new_size)) == NULL)
      {
         (void)xrec(FATAL_DIALOG, "realloc() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
      }
   }
   cdb[stored_colors].red = color->red;
   cdb[stored_colors].blue = color->blue;
   cdb[stored_colors].green = color->green;
   if (XAllocColor(display, default_cmap, &cdb[stored_colors]) == 0)
   {
      cdb[stored_colors].pixel = color_pool[BLACK];
   }
   color->pixel = cdb[stored_colors].pixel;
   stored_colors++;

   return;
}

