/*
 *  afd_load.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __afd_load_h
#define __afd_load_h

#include "motif_common_defs.h"

#define FILE_CHART              0
#define KBYTE_CHART             1
#define CONNECTION_CHART        2
#define TRANSFER_CHART          3

#define DEFAULT_UPDATE_INTERVAL 2.0 /* In seconds. */

#define MAX_CURRENT_VALUE_DIGIT 10

/* Function prototypes. */
void close_button(Widget, XtPointer, XtPointer),
     get_connection_value(Widget, XtPointer, XtPointer),
     get_file_value(Widget, XtPointer, XtPointer),
     get_kbyte_value(Widget, XtPointer, XtPointer),
     get_transfer_value(Widget, XtPointer, XtPointer);

#endif /* __afd_load_h */
