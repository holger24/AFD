/*
 *  show_ilog.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __show_ilog_h
#define __show_ilog_h

#include "motif_common_defs.h"

#define MAX_ALDA_DIFF_TIME_STR  "172800"

#define GOT_JOB_ID_DIR_ONLY     -3
#define GOT_JOB_ID_DIR_AND_RECIPIENT -5

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
#define MAX_DISPLAYED_FILE_SIZE 10
#define MAX_OUTPUT_LINE_LENGTH  (16 + MAX_DISPLAYED_FILE_SIZE + 1)

#define FILE_SIZE_FORMAT        "Enter file size in bytes: [!=<>]file size"
#define TIME_FORMAT             "Absolut: MMDDhhmm or DDhhmm or hhmm   Relative: -DDhhmm or -hhmm or -mm"

/* Maximum length of the file name that is displayed. */
#define SHOW_SHORT_FORMAT       50
#define SHOW_LONG_FORMAT        115
#define DATE_TIME_HEADER        "mm.dd. HH:MM:SS "
#define FILE_NAME_HEADER        "File name"
#define REST_HEADER             "File size"

#define LOG_CHECK_INTERVAL      1000L   /* Default interval in milli-    */
                                        /* seconds to check for changes  */
                                        /* in log file.                  */

/* Structure that holds offset (to dir ID) to each item in list. */
struct item_list
       {
          FILE  *fp;
          int   no_of_items;
          off_t *line_offset;  /* Array that contains the offset to the */
                               /* file name of that item.               */
          int   *offset;       /* Array that contains the offset to the */
                               /* dir ID of that item.                  */
       };

/* Structure to hold the data for a single entry in the AMG history file. */
struct db_entry
       {
          unsigned int job_id;
          int          no_of_files;
          int          no_of_loptions;
          int          no_of_soptions;
          char         *soptions;
          char         *files;
          char         loptions[MAX_NO_OPTIONS][MAX_OPTION_LENGTH];
          char         recipient[MAX_RECIPIENT_LENGTH];
          char         user[MAX_RECIPIENT_LENGTH];
          char         dir_url_hostname[MAX_HOSTNAME_LENGTH + 2 + 1];
          char         dir_url_user[MAX_USER_NAME_LENGTH + 2 + 1];
          char         dir_config_file[MAX_PATH_LENGTH];
          char         priority;
       };

/* Structure to hold all data for a single dir ID. */
struct info_data
       {
          time_t             arrival_time;
          unsigned int       dir_id;
          int                unique_number;
          int                count;       /* Counts number of dbe entries. */
          unsigned char      dir[MAX_PATH_LENGTH];
          unsigned char      file_name[MAX_FILENAME_LENGTH];
          char               file_size[MAX_INT_LENGTH + MAX_INT_LENGTH];
          struct dir_options d_o;
          struct db_entry    *dbe;
       };

/* Permission structure for show_ilog. */
struct sol_perm
       {
          int  list_limit;
          char view_passwd;
       };

/* Various macro definitions. */
#define SHOW_MESSAGE()                                                        \
        {                                                                     \
           XmString     xstr;                                                 \
           XExposeEvent xeev;                                                 \
           int          w, h;                                                 \
                                                                              \
           xeev.type = Expose;                                                \
           xeev.display = display;                                            \
           xeev.window = XtWindow(statusbox_w);                               \
           xeev.x = 0; xeev.y = 0;                                            \
           XtVaGetValues (statusbox_w, XmNwidth, &w, XmNheight, &h, NULL);    \
           xeev.width = w; xeev.height = h;                                   \
           xstr = XmStringCreateLtoR(status_message, XmFONTLIST_DEFAULT_TAG); \
           XtVaSetValues(statusbox_w, XmNlabelString, xstr, NULL);            \
           (XtClass(statusbox_w))->core_class.expose (statusbox_w, (XEvent *)&xeev, NULL);\
           XmStringFree(xstr);                                                \
        }
#define SHOW_SUMMARY_DATA()                                                   \
        {                                                                     \
           XmString     xstr;                                                 \
           XExposeEvent xeev;                                                 \
           int          w, h;                                                 \
                                                                              \
           xeev.type = Expose;                                                \
           xeev.display = display;                                            \
           xeev.window = XtWindow(summarybox_w);                              \
           xeev.x = 0; xeev.y = 0;                                            \
           XtVaGetValues(summarybox_w, XmNwidth, &w, XmNheight, &h, NULL);    \
           xeev.width = w; xeev.height = h;                                   \
           xstr = XmStringCreateLtoR(summary_str, XmFONTLIST_DEFAULT_TAG);    \
           XtVaSetValues(summarybox_w, XmNlabelString, xstr, NULL);           \
           (XtClass(summarybox_w))->core_class.expose (summarybox_w, (XEvent *)&xeev, NULL);\
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
extern int  get_sum_data(int, time_t *, double *);
extern void calculate_summary(char *, time_t, time_t, unsigned int, double),
            close_button(Widget, XtPointer, XtPointer),
            continues_toggle(Widget, XtPointer, XtPointer),
            format_info(char **, int),
            get_info(int),
            get_info_free(void),
            get_data(void),
            info_click(Widget, XtPointer, XEvent *),
            item_selection(Widget, XtPointer, XtPointer),
            print_button(Widget, XtPointer, XtPointer),
            radio_button(Widget, XtPointer, XtPointer),
            save_input(Widget, XtPointer, XtPointer),
            scrollbar_moved(Widget, XtPointer, XtPointer),
            search_button(Widget, XtPointer, XtPointer),
            select_all_button(Widget, XtPointer, XtPointer),
            set_sensitive(void);

#endif /* __show_ilog_h */
