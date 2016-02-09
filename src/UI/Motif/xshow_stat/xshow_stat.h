/*
 *  xshow_stat.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __xshow_stat_h
#define __xshow_stat_h

#include "motif_common_defs.h"

/* What statistics can be displayed. */
#define SHOW_KBYTE_STAT         0
#define SHOW_FILE_STAT          1
#define SHOW_CONNECT_STAT       2
#define SHOW_ERROR_STAT         3

/* The time/duration that can be displayed. */
#define HOUR_STAT               0
#define DAY_STAT                1
#define YEAR_STAT               2

/* Function prototypes. */
extern int  window_size(int *, int *, int, int);
extern void close_button(Widget, XtPointer, XtPointer),
            draw_graph(void),
            draw_stat(void),
            draw_x_values(void),
            expose_handler_stat(Widget, XtPointer, XmDrawingAreaCallbackStruct*),
            get_x_data_points(void),
            get_y_data_points(double),
            init_gcs(void),
            setup_window(char *);

#endif /* __xshow_stat_h */
