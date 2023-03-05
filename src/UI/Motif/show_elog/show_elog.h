/*
 *  show_elog.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __show_elog_h
#define __show_elog_h

#include "motif_common_defs.h"

/* What information should be displayed. */
#define SHOW_CLASS_GLOBAL        1
#define SHOW_CLASS_DIRECTORY     2
#define SHOW_CLASS_PRODUCTION    4
#define SHOW_CLASS_HOST          8
#define SHOW_TYPE_EXTERNAL       16
#define SHOW_TYPE_MANUAL         32
#define SHOW_TYPE_AUTO           64

#define GOT_JOB_ID               -2
#define GOT_JOB_ID_DIR_ONLY      -3
#define GOT_JOB_ID_USER_ONLY     -4

#define SEARCH_BUTTON            1
#define STOP_BUTTON              2
#define STOP_BUTTON_PRESSED      4

#if MAX_DIR_ALIAS_LENGTH > MAX_HOSTNAME_LENGTH
# define MAX_ALIAS_LENGTH        MAX_DIR_ALIAS_LENGTH
#else
# define MAX_ALIAS_LENGTH        MAX_HOSTNAME_LENGTH
#endif
#define LENGTH_TO_ADD_INFO      (24 + MAX_ALIAS_LENGTH + 1 + MAX_EVENT_ACTION_LENGTH + 1)
#define MAX_OUTPUT_LINE_LENGTH  (LENGTH_TO_ADD_INFO + ADDITIONAL_INFO_LENGTH)
#define MAX_TEXT_LINE_LENGTH    (LENGTH_TO_ADD_INFO + MAX_EVENT_REASON_LENGTH)

/* When saving input lets define some names so we know where */
/* to store the user input.                                  */
#define HOST_ALIAS_NO_ENTER      5
#define HOST_ALIAS               6
#define DIR_ALIAS_NO_ENTER       7
#define DIR_ALIAS                8
#define SEARCH_ADD_INFO_NO_ENTER 9
#define SEARCH_ADD_INFO          10

#define NO_OF_VISIBLE_LINES      20

#define MAX_MS_LABEL_STR_LENGTH  10
#define LINES_BUFFERED           1000

#define LOG_CHECK_INTERVAL       1000L  /* Default interval in milli-    */
                                        /* seconds to check for changes  */ 
                                        /* in log file.                  */

#define TIME_FORMAT              "Absolut: MMDDhhmm or DDhhmm or hhmm   Relative: -DDhhmm or -hhmm or -mm"

/* Permission structure for show_elog. */
struct sol_perm
       {
          int         list_limit;
          signed char view_data;
          signed char view_passwd;
       };

/* Various macro definitions. */
#define SHOW_MESSAGE()                                                        \
        {                                                                     \
           XmString xstr;                                                     \
                                                                              \
           xstr = XmStringCreateLtoR(status_message, XmFONTLIST_DEFAULT_TAG); \
           XtVaSetValues(statusbox_w, XmNlabelString, xstr, NULL);            \
           XmStringFree(xstr);                                                \
        }
#define CHECK_INTERRUPT()                                                  \
        {                                                                  \
           XEvent event;                                                   \
                                                                           \
           XFlush(display);                                                \
           XmUpdateDisplay(appshell);                                      \
                                                                           \
           while (XCheckMaskEvent(display, ButtonPressMask |               \
                  ButtonReleaseMask | ButtonMotionMask | KeyPressMask,     \
                  &event))                                                 \
           {                                                               \
              if ((event.xany.window == XtWindow(special_button_w)) ||     \
                  (event.xany.window == XtWindow(scrollbar_w)) ||          \
                  (event.xany.window == XtWindow(outputbox_w)))            \
              {                                                            \
                 XtDispatchEvent(&event);                                  \
              }                                                            \
              else                                                         \
              {                                                            \
                 if (event.type != MotionNotify)                           \
                 {                                                         \
                    XBell(display, 50);                                    \
                 }                                                         \
              }                                                            \
           }                                                               \
        }

/* Function prototypes. */
extern void close_button(Widget, XtPointer, XtPointer),
            continues_toggle(Widget, XtPointer, XtPointer),
            get_data(void),
            print_button(Widget, XtPointer, XtPointer),
            save_input(Widget, XtPointer, XtPointer),
            set_sensitive(void),
            scrollbar_moved(Widget, XtPointer, XtPointer),
            search_button(Widget, XtPointer, XtPointer),
            select_event_actions(Widget, XtPointer, XtPointer),
            toggled(Widget, XtPointer, XtPointer);

#endif /* __show_elog_h */
