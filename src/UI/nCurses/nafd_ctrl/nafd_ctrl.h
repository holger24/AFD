/*
 *  nafd_ctrl.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __nafd_ctrl_h
#define __nafd_ctrl_h

#include <time.h>                       /* clock_t                       */

#define STATIC_EVENT_REASON             "Host error/warning set offline by admin."

#define DEBUG_SYMBOL                    "D"

/* Status LED's. */
#define LED_ONE                         1
#define LED_TWO                         2

/* Log indicators (bit mapped). */
#define RECEIVE_LOG_INDICATOR           0
#define SYS_LOG_INDICATOR               1
#define TRANS_LOG_INDICATOR             2

#define LEDS_STYLE_W                    0
#define JOBS_STYLE_W                    1
#define CHARACTERS_STYLE_W              2
#define BARS_STYLE_W                    3

#define QUEUE_COUNTER_CHARS             4

/* Structure definitions. */
struct line 
       {
          char           hostname[MAX_HOSTNAME_LENGTH + 1];
          char           host_display_str[MAX_HOSTNAME_LENGTH + 1];
          int            no_of_files[MAX_NO_PARALLEL_JOBS];
          char           connect_status[MAX_NO_PARALLEL_JOBS];
          char           detailed_selection[MAX_NO_PARALLEL_JOBS];
          u_off_t        bytes_send[MAX_NO_PARALLEL_JOBS]; /* No. of bytes send so far.*/
          char           debug;              /* Is debugging enabled     */
                                             /* or disabled?             */
          char           host_toggle;
          char           host_toggle_display;
          unsigned char  stat_color_no;
          unsigned char  special_flag;
          clock_t        start_time;
          time_t         start_event_handle;
          time_t         end_event_handle;
          char           status_led[3];
          int            total_file_counter; /* The overall number of    */
                                             /* files still to be send.  */
          char           str_tfc[5];         /* String holding this      */
                                             /* number.                  */
          off_t          total_file_size;    /* The overall number of    */
                                             /* bytes still to be send.  */
          char           str_tfs[5];         /* String holding this      */
                                             /* number.                  */
          off_t          bytes_per_sec;      /* Actual transfer rate.    */
          char           str_tr[5];          /* String holding this      */
                                             /* number.                  */
          double         average_tr;         /* Average transfer rate.   */
          double         max_average_tr;     /* Max transfer rate        */
                                             /* (dynamic).               */
          int            error_counter;      /* Number of errors so far. */
          char           str_ec[3];          /* String holding this      */
                                             /* number.                  */
          int            max_errors;
          int            allowed_transfers;
          float          scale;
          unsigned int   host_id;
          unsigned int   host_status;
          unsigned int   protocol;
          unsigned int   bar_length[2];
          unsigned short green_color_offset;
          unsigned short red_color_offset;
          unsigned char  inverse;
       };
      
/* Structure that holds the permissions. */
struct afd_control_perm
       {
          char        **afd_ctrl_list;
          char        **ctrl_transfer_list;
          char        **ctrl_queue_list;
          char        **ctrl_queue_transfer_list;
          char        **handle_event_list;
          char        **switch_host_list;
          char        **disable_list;
          char        **info_list;
          char        **debug_list;
          char        **retry_list;
          char        **show_slog_list;
          char        **show_elog_list;
          char        **show_rlog_list;
          char        **show_tlog_list;
          char        **show_tdlog_list;
          char        **show_ilog_list;
          char        **show_olog_list;
          char        **show_dlog_list;
          char        **show_queue_list;
          char        **afd_load_list;
          char        **view_jobs_list;
          char        **edit_hc_list;
          char        **view_dc_list;
          signed char amg_ctrl;              /* Start/Stop the AMG       */
          signed char fd_ctrl;               /* Start/Stop the FD        */
          signed char rr_dc;                 /* Reread DIR_CONFIG        */
          signed char rr_hc;                 /* Reread HOST_CONFIG       */
          signed char startup_afd;           /* Startup AFD              */
          signed char shutdown_afd;          /* Shutdown AFD             */
          signed char ctrl_transfer;         /* Start/Stop transfer      */
          signed char ctrl_queue;            /* Start/Stop queue         */
          signed char ctrl_queue_transfer;   /* Start/Stop host          */
          signed char handle_event;          /* Handle event             */
          signed char switch_host;           /* Switch host              */
          signed char disable;               /* Disable host             */
          signed char info;                  /* Info about host          */
          signed char debug;                 /* Enable/disable debugging */
          signed char trace;                 /* Enable/disable tracing   */
          signed char full_trace;            /* Enable/disable full trace*/
          signed char retry;                 /* Retry sending file       */
          signed char show_slog;             /* Show System Log          */
          signed char show_elog;             /* Show Event Log           */
          signed char show_rlog;             /* Show Receive Log         */
          signed char show_tlog;             /* Show Transfer Log        */
          signed char show_tdlog;            /* Show Debug Log           */
          signed char show_ilog;             /* Show Input Log           */
          signed char show_olog;             /* Show Output Log          */
          signed char show_dlog;             /* Show Delete Log          */
          signed char show_queue;            /* Show AFD Queue           */
          signed char afd_load;              /* Show load of AFD         */
          signed char view_jobs;             /* View detailed transfer   */
          signed char edit_hc;               /* Edit HOST_CONFIG         */
          signed char view_dc;               /* View DIR_CONFIG entries  */
          signed char dir_ctrl;              /* dir_ctrl dialog          */
       };

struct job_data
       {
          char          hostname[MAX_HOSTNAME_LENGTH + 1];
          char          host_display_str[MAX_HOSTNAME_LENGTH + 1];
          char          file_name_in_use[MAX_FILENAME_LENGTH + 1];
          char          str_fs_use[5];        /* String file_size_in_use.*/
          char          str_fs_use_done[5];
          char          str_fc[5];            /* String no_of_files.     */
          char          str_fc_done[5];       /* String fc done.         */
          char          str_fs[5];            /* String file_size.       */
          char          str_fs_done[5];       /* String fs done.         */
          char          connect_status;
          unsigned char expose_flag;
          unsigned char stat_color_no;
          unsigned char special_flag;
          off_t         file_size_in_use;
          off_t         file_size_in_use_done;
          int           no_of_files;          /* Number of all files.    */
          int           no_of_files_done;
          off_t         file_size;            /* Size of all files.      */
          u_off_t       file_size_done;
          float         scale[3];
          unsigned int  bar_length[3];
          unsigned int  host_id;
          int           job_no;
          int           fsa_no;
          int           rotate;
          size_t        filename_compare_length;
       };

/* Function Prototypes. */

#endif /* __nafd_ctrl_h */
