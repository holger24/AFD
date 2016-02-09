/*
 *  mshow_log.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __mshow_log_h
#define __mshow_log_h

#include "motif_common_defs.h"

#define _WITH_SEARCH_FUNCTION

/* What information should be displayed. */
#define SHOW_INFO               1
#define SHOW_CONFIG             2
#define SHOW_WARN               4
#define SHOW_ERROR              8
#define SHOW_FATAL              16
#define SHOW_OFFLINE            32
#define SHOW_DEBUG              64
#define SHOW_TRACE              128

#define MISS                    0
#define HIT                     1

#define DEFAULT_SHOW_LOG_WIDTH  92
#define TRANS_DB_LOG_WIDTH      104

#define SYSTEM_STR              "System"
#define MAINTAINER_STR          "Maintainer"
#define RECEIVE_STR             "Receive"
#define TRANSFER_STR            "Transfer"
#define TRANS_DB_STR            "Debug"
#define MONITOR_STR             "Monitor"
#define MON_SYSTEM_STR          "Monsystem"

#define SYSTEM_LOG_TYPE         1
#ifdef _MAINTAINER_LOG
# define MAINTAINER_LOG_TYPE    2
#endif
#define RECEIVE_LOG_TYPE        3
#define TRANSFER_LOG_TYPE       4
#define TRANS_DB_LOG_TYPE       5
#define MONITOR_LOG_TYPE        6
#define MON_SYSTEM_LOG_TYPE     7

#define MAX_LINE_COUNTER_DIGITS 9
#define LOG_START_TIMEOUT       100
#define LOG_TIMEOUT             2000
#define FALLING_SAND_SPEED      50

/* Function prototypes. */
extern int  log_filter(char const *, char const *);
extern void check_log(Widget),
            check_selection(Widget, XtPointer, XtPointer),
            close_button(Widget, XtPointer, XtPointer),
            toggled(Widget, XtPointer, XtPointer),
            toggled_jobs(Widget, XtPointer, XtPointer),
            update_button(Widget, XtPointer, XtPointer),
#ifdef _WITH_SEARCH_FUNCTION
            search_text(Widget, XtPointer, XtPointer),
            toggled_log_no(Widget, XtPointer, XtPointer),
#else
# ifdef _WITH_SCROLLBAR
            slider_moved(Widget, XtPointer, XtPointer),
# endif
#endif
            auto_scroll_switch(Widget, XtPointer, XtPointer),
            init_text(void);

#endif /* __mshow_log_h */
