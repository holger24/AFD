/*
 *  show_queue.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __show_queue_h
#define __show_queue_h

#include "motif_common_defs.h"

/* What information should be displayed. */
#define SHOW_INPUT              1      /* Files queued, due to error or */
                                       /* queue stopped.                */
#define SHOW_OUTPUT             2      /* Files in FD queue.            */
#define SHOW_RETRIEVES          4      /* Retrieving jobs.              */
#define SHOW_UNSENT_INPUT       8      /* All files in input dir.       */
#define SHOW_UNSENT_OUTPUT      16     /* Files currently send by FD.   */
#define SHOW_PENDING_RETRIEVES  32     /* Pending retrieve jobs.        */
#define SHOW_TIME_JOBS          64     /* Pending time jobs.            */

#define GOT_JOB_ID              -2
#define GOT_JOB_ID_DIR_ONLY     -3
#define GOT_JOB_ID_USER_ONLY    -4

#define SEARCH_BUTTON           1
#define STOP_BUTTON             2
#define STOP_BUTTON_PRESSED     4

/* When saving input lets define some names so we know where */
/* to store the user input.                                  */
#define FILE_NAME_NO_ENTER      5
#define FILE_NAME               6
#define DIRECTORY_NAME_NO_ENTER 7
#define DIRECTORY_NAME          8
#define FILE_LENGTH_NO_ENTER    9
#define FILE_LENGTH             10
#define RECIPIENT_NAME_NO_ENTER 11
#define RECIPIENT_NAME          12

#define NO_OF_VISIBLE_LINES     20

#define MAX_MS_LABEL_STR_LENGTH 15
#define LINES_BUFFERED          1000
#define MAX_DISPLAYED_FILE_SIZE 12
#define MAX_OUTPUT_LINE_LENGTH  20 + MAX_HOSTNAME_LENGTH + 1 + 4 + 1 + MAX_DISPLAYED_FILE_SIZE + 1

#define FILE_SIZE_FORMAT        "Enter file size in bytes: [!=<>]file size"
#define TIME_FORMAT             "Absolut: MMDDhhmm or DDhhmm or hhmm   Relative: -DDhhmm or -hhmm or -mm"

/* Maximum length of the file name that is displayed. */
#define SHOW_SHORT_FORMAT       32
#define SHOW_MEDIUM_FORMAT      46
#define SHOW_LONG_FORMAT        115
#define HEADING_LINE_SHORT      "dd.mm.yyyy HH:MM:SS File name                        Type Hostname    File size"
#define SUM_SEP_LINE_SHORT      "==============================================================================="
#define HEADING_LINE_MEDIUM     "dd.mm.yyyy HH:MM:SS File name                                      Type Hostname    File size"
#define SUM_SEP_LINE_MEDIUM     "============================================================================================="
#define HEADING_LINE_LONG       "dd.mm.yyyy HH:MM:SS File name                                                                                       Type Hostname    File size"
#define SUM_SEP_LINE_LONG       "=============================================================================================================================================="

/* Structure that holds a list of files that where found. */
struct queued_file_list
       {
          double        msg_number;
          off_t         size;
          time_t        mtime;
          int           pos;
          int           dir_id_pos;
          int           queue_tmp_buf_pos;
          unsigned int  job_id;
          unsigned int  dir_id;
          unsigned char *file_name;
          char          msg_name[MAX_MSG_NAME_LENGTH]; /* NOT used anymore. */
          char          hostname[MAX_HOSTNAME_LENGTH + 1];
          char          dir_alias[MAX_DIR_ALIAS_LENGTH + 1];
          char          priority;
          char          queue_type;/* SHOW_INPUT, SHOW_OUTPUT,              */
                                   /* SHOW_UNSENT_INPUT, SHOW_UNSENT_OUTPUT,*/
                                   /* SHOW_RETRIEVES, SHOW_PENDING_RETRIEVES*/
       };

struct queue_tmp_buf
       {
          unsigned int files_to_send;
          unsigned int files_to_delete;
          int          *qfl_pos;
          char         msg_name[MAX_MSG_NAME_LENGTH];
       };

/* Permission structure for show_queue. */
struct sol_perm
       {
          int         list_limit;
          int         send_limit;
          signed char delete;
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
                  (event.xany.window == XtWindow(listbox_w)))              \
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
            delete_button(Widget, XtPointer, XtPointer),
            delete_files(int, int *),
            display_data(void),
            format_input_info(char **, int),
            format_output_info(char **, int),
            format_retrieve_info(char **, int),
            get_data(void),
            info_click(Widget, XtPointer, XEvent *),
            item_selection(Widget, XtPointer, XtPointer),
            print_button(Widget, XtPointer, XtPointer),
            radio_button(Widget, XtPointer, XtPointer),
            save_input(Widget, XtPointer, XtPointer),
            send_button(Widget, XtPointer, XtPointer),
            send_files(int, int *),
            scrollbar_moved(Widget, XtPointer, XtPointer),
            search_button(Widget, XtPointer, XtPointer),
            select_all_button(Widget, XtPointer, XtPointer),
            show_summary(unsigned int, double),
            toggled(Widget, XtPointer, XtPointer),
            view_button(Widget, XtPointer, XtPointer),
            view_files(int, int *);

#endif /* __show_queue_h */
