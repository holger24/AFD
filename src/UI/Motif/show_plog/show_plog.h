/*
 *  show_plog.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2016 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __show_plog_h
#define __show_plog_h

#include "motif_common_defs.h"

#define GOT_JOB_ID                 -2
#define GOT_DIR_ID_DIR_ONLY        -3
#define GOT_JOB_ID_USER_ONLY       -4
#define GOT_JOB_ID_HOST_ONLY       -5

#define SEARCH_BUTTON              1
#define STOP_BUTTON                2
#define STOP_BUTTON_PRESSED        4

#define ANY_RATIO                  0
#define ONE_TO_ONE_RATIO           1
#define ONE_TO_NONE_RATIO          2
#define ONE_TO_N_RATIO             3
#define N_TO_ONE_RATIO             4
#define N_TO_N_RATIO               5

/* NOTE: DONE is defined in afddefs.h as 3. */

/* When saving input lets define some names so we know where */
/* to store the user input.                                  */
#define ORIG_FILE_NAME_NO_ENTER    5
#define ORIG_FILE_NAME             6
#define NEW_FILE_NAME_NO_ENTER     7
#define NEW_FILE_NAME              8
#define DIRECTORY_NAME_NO_ENTER    9
#define DIRECTORY_NAME             10
#define COMMAND_NAME_NO_ENTER      11
#define COMMAND_NAME               12
#define ORIG_FILE_SIZE_NO_ENTER    13
#define ORIG_FILE_SIZE             14
#define NEW_FILE_SIZE_NO_ENTER     15
#define NEW_FILE_SIZE              16
#define RECIPIENT_NAME_NO_ENTER    17
#define RECIPIENT_NAME             18
#define JOB_ID_NO_ENTER            19
#define JOB_ID                     20
#define RETURN_CODE_NO_ENTER       21
#define RETURN_CODE                22
#define PROD_TIME_NO_ENTER         23
#define PROD_TIME                  24
#define CPU_TIME_NO_ENTER          25
#define CPU_TIME                   26

#define NO_OF_VISIBLE_LINES        20

#define MAX_MS_LABEL_STR_LENGTH    20
#define LINES_BUFFERED             1000
#define MAX_DISPLAYED_RATIO        7
#define MAX_DISPLAYED_COMMAND      18
#define MAX_DISPLAYED_RC           4
#define MAX_DISPLAYED_FILE_SIZE    10
#define MAX_DISPLAYED_PROD_TIME    7
#define MAX_DISPLAYED_CPU_TIME     7
#define MAX_PRODUCTION_LINE_LENGTH (16 + 1 + MAX_DISPLAYED_FILE_SIZE + 1 + 1 + MAX_DISPLAYED_FILE_SIZE + 1 + MAX_DISPLAYED_RATIO + 1 + MAX_DISPLAYED_COMMAND + 1 + MAX_DISPLAYED_RC + 1 + MAX_DISPLAYED_PROD_TIME + 1 + MAX_DISPLAYED_CPU_TIME)

#define FILE_SIZE_FORMAT           "Enter file size in bytes: [!=<>]file size"
#define RETURN_CODE_FORMAT         "Enter return code: [!=<>]return code"
#define PROD_TIME_FORMAT           "Enter production time: [!=<>]number.number"
#define CPU_TIME_FORMAT            "Enter cpu time: [!=<>]number.number"
#define TIME_FORMAT                "Absolut: MMDDhhmm or DDhhmm or hhmm   Relative: -DDhhmm or -hhmm or -mm"

/* Maximum length of the file name that is displayed. */
#define SHOW_SHORT_FORMAT          18
#define SHOW_MEDIUM_FORMAT         27
#define SHOW_LONG_FORMAT           50
#define DATE_TIME_HEADER           "mm.dd. HH:MM:SS "
#define ORIG_FILE_NAME_HEADER      "Orig File name"
#define ORIG_FILE_SIZE_HEADER      "Size"
#define NEW_FILE_NAME_HEADER       "New File name"
#define REST_HEADER                "      Size   Ratio Exec cmd             RC  P-time  C-time"

#define LOG_CHECK_INTERVAL         1000L /* Default interval in milli-    */
                                         /* seconds to check for changes  */
                                         /* in log file.                  */

/* Structure that holds offset (to job ID) to each item in list. */
struct item_list
       {
          FILE  *fp;
          int   no_of_items;
          off_t *line_offset; /* Array that contains the offset to the */
                              /* file name of that item.               */
          int   *offset;      /* Array that contains the offset to the */
                              /* job ID of that item.                  */
       };

/* Structure to hold all data for a single job ID. */
struct info_data
       {
          unsigned int       job_id;
          unsigned int       dir_id;
          unsigned int       unique_id;
          unsigned int       split_job_counter;
          int                ratio_1;
          int                ratio_2;
          int                no_of_files;
          int                return_code;
          off_t              orig_file_size;
          off_t              new_file_size;
          time_t             time_when_produced;
          time_t             input_time; /* When it appeared in input log. */
          double             production_time;
          double             cpu_time;
          char               *files;
#ifdef _WITH_DYNAMIC_MEMORY
          int                no_of_loptions;
          char               **loptions;
          int                no_of_soptions;
          char               *soptions;
#else
          int                no_of_loptions;
          int                no_of_soptions;
          char               *soptions;
          char               loptions[MAX_NO_OPTIONS][MAX_OPTION_LENGTH];
#endif
          char               recipient[MAX_RECIPIENT_LENGTH];
          unsigned char      user[MAX_RECIPIENT_LENGTH];
          char               mail_destination[MAX_RECIPIENT_LENGTH];
          char               host_alias[MAX_HOSTNAME_LENGTH + 2];
          char               original_filename[MAX_FILENAME_LENGTH];
          char               new_filename[MAX_FILENAME_LENGTH];
          char               dir_config_file[MAX_PATH_LENGTH];
          char               dir_id_str[MAX_DIR_ALIAS_LENGTH + 1];
          char               command[MAX_OPTION_LENGTH];
          char               unique_name[MAX_ADD_FNL + 1];
          char               priority;
          unsigned char      dir[MAX_PATH_LENGTH];
          struct dir_options d_o;
       };

/* Structure where we remember the different ratio elements, so the */
/* size is calculated correctly.                                    */
struct ratio_n_list
       {
          time_t       time_when_produced;
          unsigned int unique_id;
          unsigned int split_job_counter;
       };
struct ratio_nn_list
       {
          time_t       time_when_produced;
          unsigned int unique_id;
          unsigned int split_job_counter;
          int          ratio_1;
          int          ratio_2;
          int          counted_orig_names;
          int          counted_new_names;
          char         **original_filename;
          char         **new_filename;
       };

/* Permission structure for show_plog. */
struct sol_perm
       {
          int  resend_limit;
          int  send_limit;
          int  list_limit;
          char view_passwd;
          char view_data;
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
extern void calculate_summary(char *, time_t, time_t, unsigned int, double,
                              double, double, double),
            close_button(Widget, XtPointer, XtPointer),
#ifdef _WITH_DE_MAIL_SUPPORT
            confirmation_toggle(Widget, XtPointer, XtPointer),
#endif
            continues_toggle(Widget, XtPointer, XtPointer),
            format_info(char **),
            get_info(int),
            get_info_free(void),
            get_data(void),
            info_click(Widget, XtPointer, XEvent *),
            item_selection(Widget, XtPointer, XtPointer),
            print_button(Widget, XtPointer, XtPointer),
            radio_button(Widget, XtPointer, XtPointer),
            save_input(Widget, XtPointer, XtPointer),
            set_ratio_mode(Widget, XtPointer, XtPointer),
            set_sensitive(void),
            scrollbar_moved(Widget, XtPointer, XtPointer),
            search_button(Widget, XtPointer, XtPointer),
            select_all_button(Widget, XtPointer, XtPointer),
            select_protocol(Widget, XtPointer, XtPointer);
extern int  get_sum_data(int, time_t *, double *, double *, double *, double *);

#endif /* __show_plog_h */
