/*
 *  edit_hc.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2023 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __edit_hc_h
#define __edit_hc_h

#include <Xm/DragDrop.h>
#include "motif_common_defs.h"

#define SIDE_OFFSET             4

/* Definitions for the save_input() callback routine. */
#define REAL_HOST_NAME_1        1
#define REAL_HOST_NAME_2        2
#define PROXY_NAME              3
#define TRANSFER_TIMEOUT        4
#define MAXIMUM_ERRORS          5
#define RETRY_INTERVAL          6
#define SUCCESSFUL_RETRIES      7
#define TRANSFER_RATE_LIMIT     8
#define HOST_1_ID               9
#define HOST_2_ID               10
#define SOCKET_SEND_BUFFER      11
#define SOCKET_RECEIVE_BUFFER   12
#define KEEP_CONNECTED          13
#define WARN_TIME_DAYS          14
#define WARN_TIME_HOURS         15
#define WARN_TIME_MINS          16
#define WARN_TIME_SECS          17
#ifdef WITH_DUP_CHECK
# define DC_TIMEOUT             18
#endif

#define KC_BOTH_SEL             1
#define KC_FETCH_ONLY_SEL       2
#define KC_SEND_ONLY_SEL        3

#define FTP_ACTIVE_MODE_SEL     1
#define FTP_PASSIVE_MODE_SEL    2
#ifdef WITH_DUP_CHECK
# define ENABLE_DUPCHECK_SEL    3
# define DISABLE_DUPCHECK_SEL   4
# define FILE_NAME_SEL          5
# define FILE_NAMESIZE_SEL      6
# define FILE_NOSUFFIX_SEL      7
# define FILE_CONTENT_SEL       8
# define FILE_NAME_CONTENT_SEL  9
# define DC_DELETE_SEL          10
# define DC_STORE_SEL           11
# define ALIAS_DUPCHECK_SEL     12
# define RECIPIENT_DUPCHECK_SEL 13
# define CRC32_DUPCHECK_SEL     14
# define CRC32C_DUPCHECK_SEL    15
# define MURMUR3_DUPCHECK_SEL   16
#endif

#define MAX_TB_BUTTONS          16
#define MAX_FSO_BUTTONS         14
#define MAX_FSO_SFTP_BUTTONS    2

#define HOST_SWITCHING          1
#define AUTO_SWITCHING          2

#define MAX_CHARS_IN_LINE       56

/* Messages returned to the user. */
#define REAL_HOST_NAME_WRONG    "You must enter a real hostname."

/* Label name for host alias list. */
#define HOST_ALIAS_LABEL        "Alias Hostname"
#define HOST_ALIAS_LABEL_LENGTH (sizeof(HOST_ALIAS_LABEL) - 1)

/* Definitions to show which values have been changed. */
#define REAL_HOSTNAME_1_CHANGED          1
#define REAL_HOSTNAME_2_CHANGED          2
#define PROXY_NAME_CHANGED               4
#define TRANSFER_TIMEOUT_CHANGED         8
#define RETRY_INTERVAL_CHANGED           16
#define MAX_ERRORS_CHANGED               32
#define SUCCESSFUL_RETRIES_CHANGED       64
#define ALLOWED_TRANSFERS_CHANGED        128
#define BLOCK_SIZE_CHANGED               256
#define FILE_SIZE_OFFSET_CHANGED         512
#define NO_OF_NO_BURST_CHANGED           1024
#define HOST_1_ID_CHANGED                2048
#define HOST_2_ID_CHANGED                4096
#define HOST_SWITCH_TOGGLE_CHANGED       8192
#define AUTO_TOGGLE_CHANGED              16384
#define FTP_MODE_CHANGED                 32768
#define FTP_SET_IDLE_TIME_CHANGED        65536
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
# define FTP_KEEPALIVE_CHANGED           131072
#endif
#define FTP_FAST_RENAME_CHANGED          262144
#define FTP_FAST_CD_CHANGED              524288
#define TRANSFER_RATE_LIMIT_CHANGED      1048576
#define TTL_CHANGED                      2097152
#define FTP_IGNORE_BIN_CHANGED           4194304
#define SOCKET_SEND_BUFFER_CHANGED       8388608
#define SOCKET_RECEIVE_BUFFER_CHANGED    16777216
#define KEEP_CONNECTED_CHANGED           33554432
#ifdef WITH_DUP_CHECK
# define DC_TYPE_CHANGED                 67108864
# define DC_DELETE_CHANGED               134217728
# define DC_STORE_CHANGED                268435456
# define DC_WARN_CHANGED                 536870912
# define DC_TIMEOUT_CHANGED              1073741824
#endif
#define FTP_EXTENDED_MODE_CHANGED        2147483648U
#ifdef _WITH_BURST_2
# define ALLOW_BURST_CHANGED             1
#endif
#define FTP_PASSIVE_REDIRECT_CHANGED     2
#define ERROR_OFFLINE_STATIC_CHANGED     4
#define KC_DIRECTION_CHANGED             8
#define FILE_WHEN_LOCAL_CHANGED          16
#define TCP_KEEPALIVE_CHANGED            32
#define BUCKETNAME_IN_PATH_CHANGED       64
#define DO_NOT_DELETE_DATA_CHANGED       128
#ifdef WITH_DUP_CHECK
# define DC_REF_CHANGED                  256
# define DC_CRC_CHANGED                  512
# define DC_TIMEOUT_FIXED_CHANGED        1024
#endif
#define USE_SEQUENCE_LOCKING_CHANGED     2048
#define WARN_TIME_DAYS_CHANGED           4096
#define WARN_TIME_HOURS_CHANGED          8192
#define WARN_TIME_MINS_CHANGED           16384
#define WARN_TIME_SECS_CHANGED           32768
#define COMPRESION_CHANGED               65536
#define KEEP_TIME_STAMP_CHANGED          131072
#define SORT_FILE_NAMES_CHANGED          262144
#define NO_AGEING_JOBS_CHANGED           524288
#define CHECK_SIZE_CHANGED               1048576
#define TIMEOUT_TRANSFER_CHANGED         2097152
#define FTPS_CCC_CHANGED                 4194304
#define USE_LIST_CHANGED                 8388608
#define STRICT_TLS_CHANGED               16777216
#define DISABLE_MLST_CHANGED             33554432
#define DISCONNECT_CHANGED               67108864
#define DISABLE_STRICT_HOST_KEY_CHANGED  134217728
#define USE_STAT_LIST_CHANGED            268435456
#define FTPS_IMPLICIT_CHANGED            536870912
#define NO_EXPECT_CHANGED                1073741824
#define TLS_LEGACY_RENEGOTIATION_CHANGED 2147483648U
#define SEND_UTF8_ON_CHANGED             1

/* Structure holding all changed entries of one host. */
struct changed_entry
       {
          unsigned int   value_changed;
          unsigned int   value_changed2;
          unsigned int   value_changed3;
          char           real_hostname[2][MAX_REAL_HOSTNAME_LENGTH];
          char           host_toggle[2][1];
          char           proxy_name[MAX_PROXY_NAME_LENGTH + 1];
          unsigned char  special_flag;
          off_t          transfer_rate_limit;
          long           transfer_timeout;
          int            retry_interval;
          int            max_errors;
          int            max_successful_retries;
          int            allowed_transfers;
          int            block_size;
          int            ttl;
          unsigned int   sndbuf_size;
          unsigned int   rcvbuf_size;
          unsigned int   keep_connected;
#ifdef WITH_DUP_CHECK
          unsigned int   dup_check_flag;
          time_t         dup_check_timeout;
#endif
          unsigned int   warn_time_days;
          unsigned short warn_time_hours;
          unsigned short warn_time_mins;
          unsigned short warn_time_secs;
          signed char    file_size_offset;
          signed char    host_switch_toggle;
          signed char    auto_toggle;
          signed char    ftp_mode;
       };

/* Structures holding widget id's for option menu. */
struct parallel_transfers
       {
          XT_PTR_TYPE value[MAX_NO_PARALLEL_JOBS];
          Widget      button_w[MAX_NO_PARALLEL_JOBS];
          Widget      label_w;
          Widget      option_menu_w;
       };
struct transfer_blocksize
       {
          int    value[MAX_TB_BUTTONS];
          Widget button_w[MAX_TB_BUTTONS];
          Widget label_w;
          Widget option_menu_w;
       };
struct file_size_offset
       {
          XT_PTR_TYPE value[MAX_FSO_BUTTONS];
          Widget      button_w[MAX_FSO_BUTTONS];
          Widget      label_w;
          Widget      option_menu_w;
       };

/* Function prototypes. */
extern int  remove_host(char *, int);
extern void accept_drop(Widget, XtPointer, XmDropProcCallback),
            close_button(Widget, XtPointer, XtPointer),
#ifdef WITH_DUP_CHECK
            dc_crc_radio_button(Widget, XtPointer, XtPointer),
            dc_ref_radio_button(Widget, XtPointer, XtPointer),
            dc_type_radio_button(Widget, XtPointer, XtPointer),
            edc_radio_button(Widget, XtPointer, XtPointer),
#endif
            enter_notify(Widget, XtPointer, XtPointer),
            fso_option_changed(Widget, XtPointer, XtPointer),
            ftp_mode_radio_button(Widget, XtPointer, XtPointer),
            host_switch_toggle(Widget, XtPointer, XtPointer),
            init_host_list(int),
            kc_radio_button(Widget, XtPointer, XtPointer),
            leave_notify(Widget, XtPointer, XtPointer),
            pt_option_changed(Widget, XtPointer, XtPointer),
            remove_button(Widget, XtPointer, XtPointer),
            save_input(Widget, XtPointer, XtPointer),
            selected(Widget, XtPointer, XtPointer),
            start_drag(Widget, XEvent *, String *, Cardinal *),
            submite_button(Widget, XtPointer, XtPointer),
            tb_option_changed(Widget, XtPointer, XtPointer),
            toggle_button(Widget, XtPointer, XtPointer),
            toggle_button2(Widget, XtPointer, XtPointer),
            toggle_button3(Widget, XtPointer, XtPointer),
            value_change(Widget, XtPointer, XtPointer);

#endif /* __edit_hc_h */
