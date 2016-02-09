/*
 *  mon_ctrl.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2013 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __mon_ctrl_h
#define __mon_ctrl_h

#include "motif_common_defs.h"
#include "mondefs.h"

#define BAR_LENGTH_MODIFIER            7
#define DEFAULT_NO_OF_ROWS             50
#define DEFAULT_OTHER_OPTIONS          0
#define FORCE_SHIFT_SELECT_W           0

/* Definitions for the menu bar items. */
#define MON_W                          0
#define LOG_W                          1
#define CONTROL_W                      2
#define CONFIG_W                       3
#define HELP_W                         4

/* Definitions for Monitor pulldown. */
#define MON_SYS_LOG_W                  0
#define MON_LOG_W                      1
#define MON_RETRY_W                    2
#define MON_SWITCH_W                   3
#define MON_SELECT_W                   4
#define MON_DISABLE_W                  5
#define MON_TEST_W                     6
#define MON_INFO_W                     7
#define MON_EXIT_W                     8

/* Definitions for View pulldown. */
#define MON_AFD_CTRL_W                 0
#define MON_SYSTEM_W                   1
#define MON_EVENT_W                    2
#define MON_RECEIVE_W                  3
#define MON_TRANS_W                    4
#define MON_INPUT_W                    5
#define MON_OUTPUT_W                   6
#define MON_DELETE_W                   7
#define MON_SHOW_QUEUE_W               8
#define MON_VIEW_LOAD_W                9

/* Definitions for Control pulldown. */
#define AMG_CTRL_W                     0
#define FD_CTRL_W                      1
#define RR_DC_W                        2
#define RR_HC_W                        3
#define EDIT_HC_W                      4
#define DIR_CTRL_W                     5
#define STARTUP_AFD_W                  6
#define SHUTDOWN_AFD_W                 7

/* Definitions of popup selections. */
#define MON_SYS_LOG_SEL                40
#define MON_LOG_SEL                    41
#define MON_INFO_SEL                   42
#define MON_RETRY_SEL                  43
#define MON_SWITCH_SEL                 44
#define MON_DISABLE_SEL                45
/* NOTE: Since some of these are used by more then one */
/*       program each may define only a certain range: */
/*         mafd_ctrl.h       0 - 39                    */
/*         mon_ctrl.h       40 - 69                    */
/*         dir_ctrl.h       70 - 99                    */
/*         x_common_defs.h 100 onwards.                */

/* Definitions for the Setup pulldown. */
#define HISTORY_W                      3
#define MON_OTHER_W                    4
#define MON_SAVE_W                     5

/* History length definitions. */
#define HIS_0                          "0"
#define HIS_1                          "6"
#define HIS_2                          "12"
#define HIS_3                          "18"
#define HIS_4                          "24"
#define HIS_5                          "30"
#define HIS_6                          "36"
#define HIS_7                          "42"
#define HIS_8                          "48"
#define NO_OF_HISTORY_LOGS             9

/* Log types. */
#define MON_LOG                        0
#define SYS_LOG                        1

/* History types. */
#define RECEIVE_HISTORY                0
#define SYSTEM_HISTORY                 1
#define TRANSFER_HISTORY               2

/* Character types. */
#define FILES_TO_BE_SEND               0
#define FILE_SIZE_TO_BE_SEND           1
#define AVERAGE_TRANSFER_RATE          2
#define AVERAGE_CONNECTION_RATE        3
#define TOTAL_ERROR_COUNTER            4
#define JOBS_IN_QUEUE                  5
#define ACTIVE_TRANSFERS               6
#define ERROR_HOSTS                    7

/* Bar types. */
#define MON_TR_BAR_NO                  0
#define ACTIVE_TRANSFERS_BAR_NO        1
#define HOST_ERROR_BAR_NO              2

/* Log indicators (bit mapped). */
#define MON_SYS_LOG_INDICATOR          0
#define MON_LOG_INDICATOR              1

struct mon_line 
       {
          char           afd_alias[MAX_AFDNAME_LENGTH + 1];
          char           afd_display_str[MAX_AFDNAME_LENGTH + 2];
          char           str_fc[5];
          char           str_fs[5];
          char           str_tr[5];
          char           str_fr[4];
          char           str_ec[3];
          char           str_jq[4];
          char           str_at[4];
          char           str_hec[3];
          char           sys_log_fifo[LOG_FIFO_SIZE + 1];
          char           log_history[NO_OF_LOG_HISTORY][MAX_LOG_HISTORY];
          char           amg;
          char           fd;
          signed char    blink_flag;
          signed char    blink;
          char           archive_watch;
          int            jobs_in_queue;
          long           danger_no_of_jobs;
          long           link_max;
          int            no_of_transfers;
          int            host_error_counter;
          int            no_of_hosts;
          int            max_connections;
          size_t         afd_alias_length;
          unsigned int   sys_log_ec;
          unsigned int   fc;
          u_off_t        fs;
          u_off_t        tr;
          unsigned int   fr;
          unsigned int   ec;
          time_t         last_data_time;
          char           connect_status;
          float          scale[2];
          double         average_tr;         /* Average transfer rate.   */
          double         max_average_tr;     /* Max transfer rate.       */
          unsigned int   bar_length[3];
          unsigned short green_color_offset;
          unsigned short blue_color_offset;
          unsigned char  inverse;
          char           afd_toggle;
       };
      
/* Structure that holds the permissions. */
struct mon_control_perm
       {
          char        **mon_ctrl_list;
          char        **info_list;
          char        **retry_list;
          char        **switch_list;
          char        **disable_list;
          char        **afd_ctrl_list;
          char        **show_slog_list;
          char        **show_elog_list;
          char        **show_rlog_list;
          char        **show_tlog_list;
          char        **show_mm_log_list;
          char        **show_ilog_list;
          char        **show_olog_list;
          char        **show_dlog_list;
          char        **afd_load_list;
          char        **edit_hc_list;
          signed char amg_ctrl;              /* Start/Stop the AMG       */
          signed char fd_ctrl;               /* Start/Stop the FD        */
          signed char rr_dc;                 /* Reread DIR_CONFIG        */
          signed char rr_hc;                 /* Reread HOST_CONFIG       */
          signed char startup_afd;           /* Startup AFD              */
          signed char shutdown_afd;          /* Shutdown AFD             */
          signed char info;                  /* Info about AFD           */
          signed char retry;                 /* Retry connecting to AFD  */
          signed char switch_afd;            /* Switch AFD               */
          signed char disable;               /* Enable/Disable AFD       */
          signed char show_ms_log;           /* Show Monitor System Log  */
          signed char show_mm_log;           /* Show Monitor Log         */
          signed char afd_ctrl;              /* Show AFD Control Dialog  */
          signed char show_slog;             /* Show System Log          */
          signed char show_elog;             /* Show Event Log           */
          signed char show_rlog;             /* Show Receive Log         */
          signed char show_tlog;             /* Show Transfer Log        */
          signed char show_ilog;             /* Show Input Log           */
          signed char show_olog;             /* Show Output Log          */
          signed char show_dlog;             /* Show Delete Log          */
          signed char show_queue;            /* Show AFD Queue           */
          signed char afd_load;              /* Show load of AFD         */
          signed char edit_hc;               /* Edit HOST_CONFIG         */
          signed char dir_ctrl;              /* dir_ctrl dialog          */
       };

/* Function Prototypes. */
signed char mon_window_size(int *, int *),
            resize_mon_window(void);
void        calc_mon_but_coord(int),
            change_mon_history_cb(Widget, XtPointer, XtPointer),
            change_mon_font_cb(Widget, XtPointer, XtPointer),
            change_mon_other_cb(Widget, XtPointer, XtPointer),
            change_mon_rows_cb(Widget, XtPointer, XtPointer),
            change_mon_style_cb(Widget, XtPointer, XtPointer),
            check_afd_status(Widget),
            check_mon_status(Widget),
            destroy_error_history(void),
            draw_afd_identifier(int, int, int),
            draw_clock(time_t),
            draw_label_line(void),
            draw_line_status(int, signed char),
            draw_mon_bar(int, signed char, char, int, int),
            draw_mon_blank_line(int),
            draw_mon_button_line(void),
            draw_mon_chars(int, char, int, int),
            draw_mon_label_line(void),
            draw_mon_line_status(int, signed char),
            draw_mon_log_status(int, int),
            draw_mon_proc_led(int, signed char, int, int),
            draw_remote_log_status(int, int, int, int),
            draw_remote_history(int, int, int, int),
            init_gcs(void),
            mconfig_log(int, char *, char *, ...),
            mon_expose_handler_button(Widget, XtPointer,
                                      XmDrawingAreaCallbackStruct *),
            mon_expose_handler_label(Widget, XtPointer,
                                     XmDrawingAreaCallbackStruct *),
            mon_expose_handler_line(Widget, XtPointer,
                                    XmDrawingAreaCallbackStruct *),
            mon_focus(Widget, XtPointer, XEvent *),
            mon_input(Widget, XtPointer, XEvent *),
            mon_popup_cb(Widget, XtPointer, XtPointer),
            popup_error_history(int, int, int),
            popup_mon_menu_cb(Widget, XtPointer, XEvent *),
            redraw_all(void),
            save_mon_setup_cb(Widget, XtPointer, XtPointer),
            select_afd_dialog(Widget, XtPointer, XtPointer),
            setup_mon_window(char *),
            start_remote_prog(Widget, XtPointer, XtPointer);

#endif /* __mon_ctrl_h */
