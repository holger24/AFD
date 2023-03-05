/*
 *  ui_common_defs.h - Part of AFD, an automatic file distribution program.
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

#ifndef __ui_common_defs_h
#define __ui_common_defs_h

#include <X11/Xlib.h>

#if SIZEOF_LONG == 4
# define XT_PTR_TYPE int
#else
# define XT_PTR_TYPE long
#endif

#define MAXARGS                          21
#define MAX_INTENSITY                    65535
#define STARTING_REDRAW_TIME             500
#define MIN_REDRAW_TIME                  1500
#define MAX_REDRAW_TIME                  4000
#define REDRAW_STEP_TIME                 500
#define DEFAULT_FRAME_SPACE              4
#define SPACE_ABOVE_LINE                 1
#define SPACE_BELOW_LINE                 2
#define BUTTON_SPACING                   3
#define LED_SPACING                      1
#define PROC_LED_SPACING                 3
#define DEFAULT_AFD_ALIAS_DISPLAY_LENGTH 12
#define DEFAULT_DIR_ALIAS_DISPLAY_LENGTH 10
#define DEFAULT_HOSTNAME_DISPLAY_LENGTH  MAX_HOSTNAME_LENGTH
#define DEFAULT_FILENAME_DISPLAY_LENGTH  25
#define DEFAULT_NO_OF_HISTORY_LOGS       0
#define DEFAULT_WINDOW_ID_STEPSIZE       5
#define ZOMBIE_CHECK_INTERVAL            2000 /* 2 seconds */
#define MAX_VIEW_DATA_WINDOWS            4
#define ADDITIONAL_INFO_LENGTH           50
#if MAX_REAL_HOSTNAME_LENGTH > 65
# define MAX_INFO_STRING_LENGTH          MAX_REAL_HOSTNAME_LENGTH + 1
#else
# define MAX_INFO_STRING_LENGTH          65
#endif
#define MAX_WNINDOW_TITLE_LENGTH         80
#define MULTI_SELECT_THRESHOLD           6
#if MAX_HOSTNAME_LENGTH > 4
# define MIN_ALIAS_DISPLAY_LENGTH        4
#else
# define MIN_ALIAS_DISPLAY_LENGTH        MAX_HOSTNAME_LENGTH
#endif

/* Separator character when inserting multiple values in show_xlog. */
#define DEFAULT_MULTI_SEARCH_SEPARATOR   '|'

/* Definitions for the printer interface. */
#define SELECTION_TOGGLE                 1
#define ALL_TOGGLE                       2
#define PRINTER_TOGGLE                   3
#define FILE_TOGGLE                      4
#define MAIL_TOGGLE                      5
#define CONTROL_D                        '\004'

/* Definitions for showing protocols. */
#define SHOW_FTP                         1
#define SHOW_FILE                        2
#define SHOW_HTTP                        4
#define SHOW_SMTP                        8
#ifdef _WITH_SCP_SUPPORT
# define SHOW_SCP                        16
#endif
#ifdef _WITH_WMO_SUPPORT
# define SHOW_WMO                        32
#endif
#ifdef _WITH_MAP_SUPPORT
# define SHOW_MAP                        64
#endif
#ifdef WITH_SSL
# define SHOW_FTPS                       128
# define SHOW_HTTPS                      256
# define SHOW_SMTPS                      512
#endif
#define SHOW_SFTP                        1024
#define SHOW_EXEC                        2048
#ifdef _WITH_DE_MAIL_SUPPORT
# define SHOW_DEMAIL                     4096
#endif
#ifdef _WITH_DFAX_SUPPORT
# define SHOW_DFAX                       8192
#endif
#define SHOW_NONE                        16384

/* Definitions for all X programs of the AFD. */
#define MAX_MESSAGE_LENGTH               80

#define PRINTER_INFO_LENGTH              40

/* Definitions for the different message dialogs. */
#define INFO_DIALOG                      INFO_NO
#define CONFIG_DIALOG                    CONFIG_NO
#define WARN_DIALOG                      WARN_NO
#define ERROR_DIALOG                     ERROR_NO
#define FATAL_DIALOG                     FATAL_NO
#define ABORT_DIALOG                     6
#define QUESTION_DIALOG                  7

/* Plus Minus Sign state. */
#define PM_OPEN_STATE                    0
#define PM_CLOSE_STATE                   1

/* LED indicators. */
#define AMG_LED                          0
#define FD_LED                           1
#define AW_LED                           2
#define AFDD_LED                         3
#define AFDMON_LED                       4

/* Definitions for testing connections. */
#define SHOW_PING_TEST                   "Ping"
#define SHOW_TRACEROUTE_TEST             "Traceroute"
#define PING_W                           0           
#define TRACEROUTE_W                     1

/* Bits to indicate the line style. The first two bits are not used */
/* and are kept for backwards compatibility.                        */
#define SHOW_LEDS                        4
#define SHOW_JOBS                        8
#define SHOW_CHARACTERS                  16
#define SHOW_BARS                        32
#define SHOW_JOBS_COMPACT                64

/* Bits to indicate what other options are set. */
#define DEFAULT_OTHER_OPTIONS            0
#define FORCE_SHIFT_SELECT               1
#define AUTO_SAVE                        2
#define FRAMED_GROUPS                    4

/* Definitions for the Help pulldown. */
#define ABOUT_W                          0
#define HYPER_W                          1
#define VERSION_W                        2

/* Definitions for the Font pulldown. */
#define FONT_0_W                         0
#define FONT_1_W                         1
#define FONT_2_W                         2
#define FONT_3_W                         3
#define FONT_4_W                         4
#define FONT_5_W                         5
#define FONT_6_W                         6
#define FONT_7_W                         7
#define FONT_8_W                         8
#define FONT_9_W                         9
#define FONT_10_W                        10
#define FONT_11_W                        11
#define FONT_12_W                        12
#define NO_OF_FONTS                      13

/* Definitions for the Row pulldown. */
#define ROW_0_W                          0
#define ROW_1_W                          1
#define ROW_2_W                          2
#define ROW_3_W                          3
#define ROW_4_W                          4
#define ROW_5_W                          5
#define ROW_6_W                          6
#define ROW_7_W                          7
#define ROW_8_W                          8
#define ROW_9_W                          9
#define ROW_10_W                         10
#define ROW_11_W                         11
#define ROW_12_W                         12
#define ROW_13_W                         13
#define ROW_14_W                         14
#define ROW_15_W                         15
#define ROW_16_W                         16
#define ROW_17_W                         17
#define ROW_18_W                         18
#define ROW_19_W                         19
#define ROW_20_W                         20
#define NO_OF_ROWS                       21

/* Definitions for the Line Style pulldown widgets. */
#define STYLE_0_W                        0
#define STYLE_1_W                        1
#define STYLE_2_W                        2
#define STYLE_3_W                        3

/* Definitions of popup selections. */
#define S_LOG_SEL                        100    /* System Log */
#define M_LOG_SEL                        101    /* Maintainer Log */
#define E_LOG_SEL                        102    /* Event Log */
#define R_LOG_SEL                        103    /* Receive Log */
#define T_LOG_SEL                        104    /* Transfer Log */
#define TD_LOG_SEL                       105    /* Transfer Debug Log */
#define I_LOG_SEL                        106    /* Input Log */
#define P_LOG_SEL                        107    /* Production Log */
#define O_LOG_SEL                        108    /* Output Log */
#define D_LOG_SEL                        109    /* Delete Log */
#define EXIT_SEL                         110
#define VIEW_FILE_LOAD_SEL               111
#define VIEW_KBYTE_LOAD_SEL              112
#define VIEW_CONNECTION_LOAD_SEL         113
#define VIEW_TRANSFER_LOAD_SEL           114
#define PING_SEL                         115
#define TRACEROUTE_SEL                   116
#define DIR_CTRL_SEL                     117
#define SHOW_QUEUE_SEL                   118
#define AFD_CTRL_SEL                     119
#define CONTROL_AMG_SEL                  120
#define CONTROL_FD_SEL                   121
#define REREAD_DIR_CONFIG_SEL            122
#define REREAD_HOST_CONFIG_SEL           123
#define EDIT_DC_SEL                      124
#define EDIT_HC_SEL                      125
#define STARTUP_AFD_SEL                  126
#define SHUTDOWN_AFD_SEL                 127
#define OPEN_ALL_GROUPS_SEL              128
#define CLOSE_ALL_GROUPS_SEL             129
/* NOTE: Since some of these are used by more then one */
/*       program each may define only a certain range: */
/*         afd_ctrl.h        0 - 39                    */
/*         mon_ctrl.h       40 - 69                    */
/*         dir_ctrl.h       70 - 99                    */
/*         ui_common_defs.h 100 onwards.               */

#ifndef PI
#define PI                               3.141592654
#endif

#define STATIC                           2
#define YUP                              2

#define FONT_ID                          "Default font:"
#define ROW_ID                           "Number of rows:"
#define STYLE_ID                         "Line style:"
#define OTHER_ID                         "Other options:"
#define INVISIBLE_GROUP_ID               "Invisible group:"
#define HOSTNAME_DISPLAY_LENGTH_ID       "Hostname display length:"
#define FILENAME_DISPLAY_LENGTH_ID       "Filename display length:"
#define NO_OF_HISTORY_LENGTH_ID          "History log length:"

#define INFO_IDENTIFIER                  "INFO-"
#define AFD_INFO_FILE                    "afd.info"
#define HOST_INFO_FILE                   "host.info"
#define DIR_INFO_FILE                    "dir.info"

#define NO_INFO_AVAILABLE                "No information available."

/* Different line styles. */
#define BARS_ONLY                        0
#define CHARACTERS_ONLY                  1
#define CHARACTERS_AND_BARS              2

/* Definitions for program afd_load. */
#define SHOW_FILE_LOAD                   "Files"
#define SHOW_KBYTE_LOAD                  "KBytes"
#define SHOW_CONNECTION_LOAD             "Connections"
#define SHOW_TRANSFER_LOAD               "Active-Transfers"
#define FILE_LOAD_W                      0
#define KBYTE_LOAD_W                     1
#define CONNECTION_LOAD_W                2
#define TRANSFER_LOAD_W                  3

/* List of fonts that should be available. */
#define FONT_0                           "5x7"
#define FONT_1                           "5x8"
#define FONT_2                           "6x9"
#define FONT_3                           "6x10"
#define FONT_4                           "6x12"
#define FONT_5                           "6x13"
#define FONT_6                           "7x13"
#define FONT_7                           "7x14"
#define FONT_8                           "8x13"
#define FONT_9                           "8x16"
#define FONT_10                          "9x15bold"
#define FONT_11                          "10x20"
#define FONT_12                          "12x24"
#define DEFAULT_FONT                     "6x13"

/* List of number of rows in a column. */
#define ROW_0                            "2"
#define ROW_1                            "4"
#define ROW_2                            "5"
#define ROW_3                            "6"
#define ROW_4                            "8"
#define ROW_5                            "10"
#define ROW_6                            "14"
#define ROW_7                            "16"
#define ROW_8                            "20"
#define ROW_9                            "25"
#define ROW_10                           "30"
#define ROW_11                           "40"
#define ROW_12                           "50"
#define ROW_13                           "60"
#define ROW_14                           "70"
#define ROW_15                           "80"
#define ROW_16                           "90"
#define ROW_17                           "100"
#define ROW_18                           "120"
#define ROW_19                           "140"
#define ROW_20                           "160"

/* All colors for status, background, foreground, etc. */
#define DEFAULT_BG_COLOR                   "LightBlue1"
#define DEFAULT_BG_COLOR_1                 "LightSkyBlue1"
#define DEFAULT_BG_COLOR_2                 "LightBlue"
#define DEFAULT_BG_COLOR_3                 "PowderBue"
#define WHITE_COLOR                        "White"
#define WHITE_COLOR_1                      "snow"
#define WHITE_COLOR_2                      "GhostWhite"
#define WHITE_COLOR_3                      "WhiteSmoke"
#define NOT_WORKING_COLOR                  "tomato"
#define NOT_WORKING_COLOR_1                "tomato1"
#define NOT_WORKING_COLOR_2                "OrangeRed"
#define NOT_WORKING_COLOR_3                "coral"
#define NOT_WORKING2_COLOR                 "Red"
#define NOT_WORKING2_COLOR_1               "Red1"
#define NOT_WORKING2_COLOR_2               "Red2"
#define NOT_WORKING2_COLOR_3               "firebrick1"
#define STOP_TRANSFER_COLOR                "DarkOrange"
#define STOP_TRANSFER_COLOR_1              "choclate1"
#define STOP_TRANSFER_COLOR_2              "orange"
#define STOP_TRANSFER_COLOR_3              "DarkOrange1"
#define TRANSFER_ACTIVE_COLOR              "SeaGreen"
#define TRANSFER_ACTIVE_COLOR_1            "ForestGreen"
#define TRANSFER_ACTIVE_COLOR_2            "OliveDrab"
#define TRANSFER_ACTIVE_COLOR_3            "PaleGreen4"
#define PAUSE_QUEUE_COLOR                  "SaddleBrown"
#define PAUSE_QUEUE_COLOR_1                "choclate4"
#define PAUSE_QUEUE_COLOR_2                "sienna4"
#define PAUSE_QUEUE_COLOR_3                "DarkOrange4"
#define AUTO_PAUSE_QUEUE_COLOR             "brown3"
#define AUTO_PAUSE_QUEUE_COLOR_1           "OrangeRed3"
#define AUTO_PAUSE_QUEUE_COLOR_2           "firebrick"
#define AUTO_PAUSE_QUEUE_COLOR_3           "IndianRed3"
#define NORMAL_STATUS_COLOR                "green3"
#define NORMAL_STATUS_COLOR_1              "LimeGreen"
#define NORMAL_STATUS_COLOR_2              "chartreuse3"
#define NORMAL_STATUS_COLOR_3              "SpringGreen3"
#define CONNECTING_COLOR                   "Blue"
#define CONNECTING_COLOR_1                 "MediumBlue"
#define CONNECTING_COLOR_2                 "blue1"
#define CONNECTING_COLOR_3                 "blue2"
#define BLACK_COLOR                        "Black"
#define BLACK_COLOR_1                      "grey1"
#define BLACK_COLOR_2                      "grey2"
#define BLACK_COLOR_3                      "grey3"
#define LOCKED_INVERSE_COLOR               "gray37"
#define LOCKED_INVERSE_COLOR_1             "seashell4"
#define LOCKED_INVERSE_COLOR_2             "ivory4"
#define LOCKED_INVERSE_COLOR_3             "gray40"
#define TR_BAR_COLOR                       "gold"
#define TR_BAR_COLOR_1                     "goldenrod"
#define TR_BAR_COLOR_2                     "gold1"
#define TR_BAR_COLOR_3                     "orange"
#define LABEL_BG_COLOR                     "NavajoWhite1"
#define LABEL_BG_COLOR_1                   "NavajoWhite"
#define LABEL_BG_COLOR_2                   "moccasin"
#define LABEL_BG_COLOR_3                   "bisque"
#define BUTTON_BACKGROUND_COLOR            "SteelBlue1"
#define BUTTON_BACKGROUND_COLOR_1          "LightSkyBlue"
#define BUTTON_BACKGROUND_COLOR_2          "DeepSkyBlue1"
#define BUTTON_BACKGROUND_COLOR_3          "SteelBlue2"
#define SMTP_ACTIVE_COLOR                  "pink"
#define SMTP_ACTIVE_COLOR_1                "LightPink"
#define SMTP_ACTIVE_COLOR_2                "MistyRose1"
#define SMTP_ACTIVE_COLOR_3                "RosyBrown1"
#define CHAR_BACKGROUND_COLOR              "lightskyblue2"
#define CHAR_BACKGROUND_COLOR_1            "SkyBlue1"
#define CHAR_BACKGROUND_COLOR_2            "LightBlue1"
#define CHAR_BACKGROUND_COLOR_3            "SkyBlue"
#define FTP_BURST_TRANSFER_ACTIVE_COLOR    "green"
#define FTP_BURST_TRANSFER_ACTIVE_COLOR_1  "LawnGreen"
#define FTP_BURST_TRANSFER_ACTIVE_COLOR_2  "chartreuse"
#define FTP_BURST_TRANSFER_ACTIVE_COLOR_3  "green1"
#ifdef _WITH_WMO_SUPPORT
#define WMO_BURST_TRANSFER_ACTIVE_COLOR    "yellow"
#define WMO_BURST_TRANSFER_ACTIVE_COLOR_1  "LightYellow"
#define WMO_BURST_TRANSFER_ACTIVE_COLOR_2  "LightYellow2"
#define WMO_BURST_TRANSFER_ACTIVE_COLOR_3  "yellow2"
#endif
#define SFTP_BURST_TRANSFER_ACTIVE_COLOR   "BlanchedAlmond"
#define SFTP_BURST_TRANSFER_ACTIVE_COLOR_1 "PapayaWhip"
#define SFTP_BURST_TRANSFER_ACTIVE_COLOR_2 "AntiqueWhite"
#define SFTP_BURST_TRANSFER_ACTIVE_COLOR_3 "MistyRose"
#define SMTP_BURST_TRANSFER_ACTIVE_COLOR   "HotPink"
#define SMTP_BURST_TRANSFER_ACTIVE_COLOR_1 "DeepPink"
#define SMTP_BURST_TRANSFER_ACTIVE_COLOR_2 "LightPink"
#define SMTP_BURST_TRANSFER_ACTIVE_COLOR_3 "PaleVioletRed"
#define SFTP_RETRIEVE_ACTIVE_COLOR         "peru"
#define SFTP_RETRIEVE_ACTIVE_COLOR_1       "tan"
#define SFTP_RETRIEVE_ACTIVE_COLOR_2       "LightSalmon"
#define SFTP_RETRIEVE_ACTIVE_COLOR_3       "DarkKhaki"


/* Position of each color in global array. */
/*****************/
/* see afddefs.h */
/*****************/

/* Structure definitions. */
struct apps_list
       {
          char  progname[MAX_PROCNAME_LENGTH + 1];
          pid_t pid;
          int   position;           /* Position in FSA or MSA. */
       };

struct coord
       {
          int x;
          int y;
       };

struct window_ids
       {
          Window window_id;
          pid_t  pid;
       };

struct view_process_list
       {
          char *progname;
          char **filter;
          char **args;
          int  no_of_filters;
       };

#define MAX_VIEW_ALIAS_NAME_LENGTH        8
struct view_modes
       {
          char alias[MAX_VIEW_ALIAS_NAME_LENGTH + 1];
          char **args;
          char *cmd;
          char *p_cmd;
          char *progname;
          int  argcounter;
          char with_show_cmd;
       };

/* Structure holding all ALDA information. */
struct alda_call_data
       {
           char          alias_name[MAX_REAL_HOSTNAME_LENGTH + 1];
           char          real_hostname[MAX_REAL_HOSTNAME_LENGTH + 1];
           char          final_name[MAX_PATH_LENGTH + 1];
           off_t         final_size;
           char          hr_final_size[12 + 1];
           time_t        delivery_time;
           char          transmission_time[MAX_INT_LENGTH + 1 + 2 + 1 + 2 + 1 + 1 + 1];
           unsigned int  output_job_id;
           unsigned int  retries;
           unsigned int  split_job_counter;
           char          archive_dir[MAX_PATH_LENGTH + 1];
           time_t        delete_time;
           unsigned int  delete_job_id;
           char          production_input_name[MAX_FILENAME_LENGTH + 1];
           char          production_final_name[MAX_FILENAME_LENGTH + 1];
           unsigned int  production_job_id;
           unsigned int  distribution_type;
           unsigned int  no_of_distribution_types;
           unsigned int  *job_id_list;
           unsigned int  delete_type;
           char          user_process[MAX_USER_NAME_LENGTH + 1];
           char          add_reason[MAX_PATH_LENGTH + 1];
       };

#define HEX_CHAR_TO_INT(hex_char)   \
        {                           \
           switch ((hex_char))      \
           {                        \
              case '0' : type = 0;  \
                         break;     \
              case '1' : type = 1;  \
                         break;     \
              case '2' : type = 2;  \
                         break;     \
              case '3' : type = 3;  \
                         break;     \
              case '4' : type = 4;  \
                         break;     \
              case '5' : type = 5;  \
                         break;     \
              case '6' : type = 6;  \
                         break;     \
              case '7' : type = 7;  \
                         break;     \
              case '8' : type = 8;  \
                         break;     \
              case '9' : type = 9;  \
                         break;     \
              case 'a' : type = 10; \
                         break;     \
              case 'b' : type = 11; \
                         break;     \
              case 'c' : type = 12; \
                         break;     \
              case 'd' : type = 13; \
                         break;     \
              case 'e' : type = 14; \
                         break;     \
              case 'f' : type = 15; \
                         break;     \
              default  : type = 100;\
                         break;     \
           }                        \
        }
#define CONVERT_TIME()                               \
        {                                            \
           if (p_ts == NULL)                         \
           {                                         \
              line[0] = '?';                         \
              line[1] = '?';                         \
              line[3] = '?';                         \
              line[4] = '?';                         \
              line[7] = '?';                         \
              line[8] = '?';                         \
              line[10] = '?';                        \
              line[11] = '?';                        \
              line[13] = '?';                        \
              line[14] = '?';                        \
           }                                         \
           else                                      \
           {                                         \
              line[0] = ((p_ts->tm_mon + 1) / 10) + '0';\
              line[1] = ((p_ts->tm_mon + 1) % 10) + '0';\
              line[3] = (p_ts->tm_mday / 10) + '0';  \
              line[4] = (p_ts->tm_mday % 10) + '0';  \
              line[7] = (p_ts->tm_hour / 10) + '0';  \
              line[8] = (p_ts->tm_hour % 10) + '0';  \
              line[10] = (p_ts->tm_min / 10) + '0';  \
              line[11] = (p_ts->tm_min % 10) + '0';  \
              line[13] = (p_ts->tm_sec / 10) + '0';  \
              line[14] = (p_ts->tm_sec % 10) + '0';  \
           }                                         \
           line[2] = '.';                            \
           line[5] = '.';                            \
           line[9] = ':';                            \
           line[12] = ':';                           \
        }
#define CONVERT_TIME_YEAR()                          \
        {                                            \
           if (p_ts == NULL)                         \
           {                                         \
              line[0] = '?';                         \
              line[1] = '?';                         \
              line[3] = '?';                         \
              line[4] = '?';                         \
              line[6] = '?';                         \
              line[7] = '?';                         \
              line[8] = '?';                         \
              line[9] = '?';                         \
              line[11] = '?';                        \
              line[12] = '?';                        \
              line[14] = '?';                        \
              line[15] = '?';                        \
              line[17] = '?';                        \
              line[18] = '?';                        \
           }                                         \
           else                                      \
           {                                         \
              line[0] = (p_ts->tm_mday / 10) + '0';  \
              line[1] = (p_ts->tm_mday % 10) + '0';  \
              line[3] = ((p_ts->tm_mon + 1) / 10) + '0';\
              line[4] = ((p_ts->tm_mon + 1) % 10) + '0';\
              line[6] = (((p_ts->tm_year + 1900) / 1000) % 10) + '0';\
              line[7] = (((p_ts->tm_year + 1900) / 100) % 10) + '0';\
              line[8] = (((p_ts->tm_year + 1900) / 10) % 10) + '0';\
              line[9] = ((p_ts->tm_year + 1900) % 10) + '0';\
              line[11] = (p_ts->tm_hour / 10) + '0'; \
              line[12] = (p_ts->tm_hour % 10) + '0'; \
              line[14] = (p_ts->tm_min / 10) + '0';  \
              line[15] = (p_ts->tm_min % 10) + '0';  \
              line[17] = (p_ts->tm_sec / 10) + '0';  \
              line[18] = (p_ts->tm_sec % 10) + '0';  \
           }                                         \
           line[2] = '.';                            \
           line[5] = '.';                            \
           line[13] = ':';                           \
           line[16] = ':';                           \
        }
#define CREATE_LFC_STRING(str, value)              \
        {                                          \
           (str)[5] = '\0';                        \
           if ((value) < 10)                       \
           {                                       \
              (str)[0] = ' ';                      \
              (str)[1] = ' ';                      \
              (str)[2] = ' ';                      \
              (str)[3] = ' ';                      \
              (str)[4] = (value) + '0';            \
           }                                       \
           else if ((value) < 100)                 \
                {                                  \
                   (str)[0] = ' ';                 \
                   (str)[1] = ' ';                 \
                   (str)[2] = ' ';                 \
                   (str)[3] = ((value) / 10) + '0';\
                   (str)[4] = ((value) % 10) + '0';\
                }                                  \
           else if ((value) < 1000)                \
                {                                  \
                   (str)[0] = ' ';                 \
                   (str)[1] = ' ';                 \
                   (str)[2] = ((value) / 100) + '0';\
                   (str)[3] = (((value) / 10) % 10) + '0';\
                   (str)[4] = ((value) % 10) + '0';\
                }                                  \
           else if ((value) < 10000)               \
                {                                  \
                   (str)[0] = ' ';                 \
                   (str)[1] = ((value) / 1000) + '0';\
                   (str)[2] = (((value) / 100) % 10) + '0';\
                   (str)[3] = (((value) / 10) % 10) + '0';\
                   (str)[4] = ((value) % 10) + '0';\
                }                                  \
                else                               \
                {                                  \
                   (str)[0] = (((value) / 10000) % 10) + '0';\
                   (str)[1] = (((value) / 1000) % 10) + '0';\
                   (str)[2] = (((value) / 100) % 10) + '0';\
                   (str)[3] = (((value) / 10) % 10) + '0';\
                   (str)[4] = ((value) % 10) + '0';\
                }                                  \
        }

#define CREATE_FC_STRING(str, value)               \
        {                                          \
           (str)[4] = '\0';                        \
           if ((value) < 10000)                    \
           {                                       \
              if ((value) < 10)                    \
              {                                    \
                 (str)[0] = ' ';                   \
                 (str)[1] = ' ';                   \
                 (str)[2] = ' ';                   \
                 (str)[3] = (value) + '0';         \
              }                                    \
              else if ((value) < 100)              \
                   {                               \
                      (str)[0] = ' ';              \
                      (str)[1] = ' ';              \
                      (str)[2] = ((value) / 10) + '0';\
                      (str)[3] = ((value) % 10) + '0';\
                   }                               \
              else if ((value) < 1000)             \
                   {                               \
                      (str)[0] = ' ';              \
                      (str)[1] = ((value) / 100) + '0';\
                      (str)[2] = (((value) / 10) % 10) + '0';\
                      (str)[3] = ((value) % 10) + '0';\
                   }                               \
                   else                            \
                   {                               \
                      (str)[0] = (((value) / 1000) % 10) + '0';\
                      (str)[1] = (((value) / 100) % 10) + '0';\
                      (str)[2] = (((value) / 10) % 10) + '0';\
                      (str)[3] = ((value) % 10) + '0';\
                   }                               \
           }                                       \
           else                                    \
           {                                       \
              if ((value) < MEGAFILE)              \
              {                                    \
                 int num = (value) / KILOFILE;     \
                                                   \
                 (str)[3] = 'k';                   \
                 if (num < 100)                    \
                 {                                 \
                    (str)[0] = ' ';                \
                    (str)[1] = (num / 10) + '0';   \
                    (str)[2] = (num % 10) + '0';   \
                 }                                 \
                 else                              \
                 {                                 \
                    (str)[0] = (num / 100) + '0';  \
                    (str)[1] = ((num / 10) % 10) + '0';\
                    (str)[2] = (num % 10) + '0';   \
                 }                                 \
              }                                    \
              else if ((value) < GIGAFILE)         \
                   {                               \
                      int num = (value) / MEGAFILE;\
                                                   \
                      (str)[3] = 'm';              \
                      if (num < 10)                \
                      {                            \
                         num = ((value) * 10) / MEGAFILE;\
                         (str)[0] = (num / 10) + '0';\
                         (str)[1] = '.';           \
                         (str)[2] = (num % 10) + '0';\
                      }                            \
                      else if (num < 100)          \
                           {                       \
                              (str)[0] = ' ';      \
                              (str)[1] = (num / 10) + '0';\
                              (str)[2] = (num % 10) + '0';\
                           }                       \
                           else                    \
                           {                       \
                              (str)[0] = (num / 100) + '0';\
                              (str)[1] = ((num / 10) % 10) + '0';\
                              (str)[2] = (num % 10) + '0';\
                           }                       \
                   }                               \
                   else                            \
                   {                               \
                      int num = (value) / GIGAFILE;\
                                                   \
                      (str)[3] = 'g';              \
                      if (num < 10)                \
                      {                            \
                         num = (value) / (GIGAFILE / 10);\
                         (str)[0] = (num / 10) + '0';\
                         (str)[1] = '.';           \
                         (str)[2] = (num % 10) + '0';\
                      }                            \
                      else if (num < 100)          \
                           {                       \
                              (str)[0] = ' ';      \
                              (str)[1] = (num / 10) + '0';\
                              (str)[2] = (num % 10) + '0';\
                           }                       \
                           else                    \
                           {                       \
                              (str)[0] = ((num / 100) % 10) + '0';\
                              (str)[1] = ((num / 10) % 10) + '0';\
                              (str)[2] = (num % 10) + '0';\
                           }                       \
                   }                               \
           }                                       \
        }

#define CREATE_FR_STRING(str, value)               \
        {                                          \
           (str)[4] = '\0';                        \
           if ((value) < 1.0)                      \
           {                                       \
              (str)[0] = ' ';                      \
              (str)[1] = '0';                      \
              (str)[2] = '.';                      \
              (str)[3] = ((int)((value) * 10.0) % 10) + '0';\
           }                                       \
           else if ((value) < 10.0)                \
                {                                  \
                   (str)[0] = ' ';                 \
                   (str)[1] = (int)(value) + '0';  \
                   (str)[2] = '.';                 \
                   (str)[3] = ((int)((value) * 10.0) % 10) + '0';\
                }                                  \
           else if ((value) < 100.0)               \
                {                                  \
                   (str)[0] = ((int)(value) / 10) + '0';\
                   (str)[1] = ((int)(value) % 10) + '0';\
                   (str)[2] = '.';                 \
                   (str)[3] = ((int)((value) * 10.0) % 10) + '0';\
                }                                  \
           else if ((value) < 1000.0)              \
                {                                  \
                   (str)[0] = ' ';                 \
                   (str)[1] = ((int)(value) / 100) + '0';\
                   (str)[2] = ((int)((value) / 10) % 10) + '0';\
                   (str)[3] = ((int)(value) % 10) + '0';\
                }                                  \
                else                               \
                {                                  \
                   (str)[0] = ((int)((value) / 1000) % 10) + '0';\
                   (str)[1] = ((int)((value) / 100) % 10) + '0';\
                   (str)[2] = (((int)(value) / 10) % 10) + '0';\
                   (str)[3] = ((int)(value) % 10) + '0';\
                }                                  \
        }

#define CREATE_SFC_STRING(str, value)              \
        {                                          \
           (str)[3] = '\0';                        \
           if ((value) < 10)                       \
           {                                       \
              (str)[0] = ' ';                      \
              (str)[1] = ' ';                      \
              (str)[2] = (value) + '0';            \
           }                                       \
           else if ((value) < 100)                 \
                {                                  \
                   (str)[0] = ' ';                 \
                   (str)[1] = ((value) / 10) + '0';\
                   (str)[2] = ((value) % 10) + '0';\
                }                                  \
                else                               \
                {                                  \
                   (str)[0] = (((value) / 100) % 10) + '0';\
                   (str)[1] = (((value) / 10) % 10) + '0';\
                   (str)[2] = ((value) % 10) + '0';\
                }                                  \
        }

#define CREATE_EC_STRING(str, value)               \
        {                                          \
           (str)[2] = '\0';                        \
           if ((value) < 10)                       \
           {                                       \
              (str)[0] = ' ';                      \
              (str)[1] = (value) + '0';            \
           }                                       \
           else if ((value) < 100)                 \
                {                                  \
                   (str)[0] = ((value) / 10) + '0';\
                   (str)[1] = ((value) % 10) + '0';\
                }                                  \
                else                               \
                {                                  \
                   (str)[0] = ' ';                 \
                   (str)[1] = '>';                 \
                }                                  \
        }

#define CREATE_FP_STRING(str, value)               \
        {                                          \
           (str)[3] = '\0';                        \
           if ((value) < 10)                       \
           {                                       \
              (str)[0] = ' ';                      \
              (str)[1] = ' ';                      \
              (str)[2] = (value) + '0';            \
           }                                       \
           else if ((value) < 100)                 \
                {                                  \
                   (str)[0] = ' ';                 \
                   (str)[1] = ((value) / 10) + '0';\
                   (str)[2] = ((value) % 10) + '0';\
                }                                  \
           else if ((value) < 1000)                \
                {                                  \
                   (str)[0] = ((value) / 100) + '0';\
                   (str)[1] = (((value) / 10) % 10) + '0';\
                   (str)[2] = ((value) % 10) + '0';\
                }                                  \
                else                               \
                {                                  \
                   (str)[0] = ' ';                 \
                   (str)[1] = ' ';                 \
                   (str)[2] = '>';                 \
                }                                  \
        }

#define CREATE_JQ_STRING(str, value)               \
        {                                          \
           (str)[3] = '\0';                        \
           if ((value) < KILOFILE)                 \
           {                                       \
              if ((value) < 10)                    \
              {                                    \
                 (str)[0] = ' ';                   \
                 (str)[1] = ' ';                   \
                 (str)[2] = (value) + '0';         \
              }                                    \
              else if ((value) < 100)              \
                   {                               \
                      (str)[0] = ' ';              \
                      (str)[1] = ((value) / 10) + '0';\
                      (str)[2] = ((value) % 10) + '0';\
                   }                               \
                   else                            \
                   {                               \
                      (str)[0] = (((value) / 100) % 10) + '0';\
                      (str)[1] = (((value) / 10) % 10) + '0';\
                      (str)[2] = ((value) % 10) + '0';\
                   }                               \
           }                                       \
           else                                    \
           {                                       \
              if ((value) < MEGAFILE)              \
              {                                    \
                 int num = (value) / KILOFILE;     \
                                                   \
                 (str)[2] = 'k';                   \
                 if (num < 10)                     \
                 {                                 \
                    (str)[0] = ' ';                \
                    (str)[1] = num + '0';          \
                 }                                 \
                 else                              \
                 {                                 \
                    (str)[0] = (num / 10) + '0';   \
                    (str)[1] = (num % 10) + '0';   \
                 }                                 \
              }                                    \
              else if ((value) < GIGAFILE)         \
                   {                               \
                      int num = (value) / MEGAFILE;\
                                                   \
                      (str)[2] = 'm';              \
                      if (num < 10)                \
                      {                            \
                         (str)[0] = ' ';           \
                         (str)[1] = num + '0';     \
                      }                            \
                      else                         \
                      {                            \
                         (str)[0] = (num / 10) + '0';\
                         (str)[1] = (num % 10) + '0';\
                      }                            \
                   }                               \
                   else                            \
                   {                               \
                      int num = (value) / GIGAFILE;\
                                                   \
                      (str)[2] = 'g';              \
                      if (num < 10)                \
                      {                            \
                         (str)[0] = ' ';           \
                         (str)[1] = num + '0';     \
                      }                            \
                      else                         \
                      {                            \
                         (str)[0] = (num / 10) + '0';\
                         (str)[1] = (num % 10) + '0';\
                      }                            \
                   }                               \
           }                                       \
        }

#define CREATE_FS_STRING(str, value)\
   {\
      (str)[4] = '\0';\
      if ((value) < KILOBYTE)\
      {\
         if ((value) < 1000)\
         {\
            (str)[3] = 'B';\
            if ((value) < 10)\
            {\
               (str)[0] = ' ';\
               (str)[1] = ' ';\
               (str)[2] = (value) + '0';\
            }\
            else if ((value) < 100)\
                 {\
                    (str)[0] = ' ';\
                    (str)[1] = ((value) / 10) + '0';\
                    (str)[2] = ((value) % 10) + '0';\
                 }\
                 else\
                 {\
                    (str)[0] = ((value) / 100) + '0';\
                    (str)[1] = (((value) / 10) % 10) + '0';\
                    (str)[2] = ((value) % 10) + '0';\
                 }\
         }\
         else\
         {\
            (str)[0] = '0';\
            (str)[1] = '.';\
            (str)[2] = '9';\
            (str)[3] = 'K';\
         }\
      }\
      else if ((value) < MEGABYTE)\
           {\
              if ((value) < 1000000)\
              {\
                 int num = (value) / KILOBYTE;\
\
                 (str)[3] = 'K';\
                 if (num < 10)\
                 {\
                    num = ((value) * 10) / KILOBYTE;\
                    (str)[0] = (num / 10) + '0';\
                    (str)[1] = '.';\
                    (str)[2] = (num % 10) + '0';\
                 }\
                 else if (num < 100)\
                      {\
                         (str)[0] = ' ';\
                         (str)[1] = (num / 10) + '0';\
                         (str)[2] = (num % 10) + '0';\
                      }\
                      else\
                      {\
                         (str)[0] = (num / 100) + '0';\
                         (str)[1] = ((num / 10) % 10) + '0';\
                         (str)[2] = (num % 10) + '0';\
                      }\
              }\
              else\
              {\
                 (str)[0] = '0';\
                 (str)[1] = '.';\
                 (str)[2] = '9';\
                 (str)[3] = 'M';\
              }\
           }\
      else if ((value) < GIGABYTE)\
           {\
              if ((value) < 1000000000)\
              {\
                 int num = (value) / MEGABYTE;\
\
                 (str)[3] = 'M';\
                 if (num < 10)\
                 {\
                    num = ((value) * 10) / MEGABYTE;\
                    (str)[0] = (num / 10) + '0';\
                    (str)[1] = '.';\
                    (str)[2] = (num % 10) + '0';\
                 }\
                 else if (num < 100)\
                      {\
                         (str)[0] = ' ';\
                         (str)[1] = (num / 10) + '0';\
                         (str)[2] = (num % 10) + '0';\
                      }\
                      else\
                      {\
                         (str)[0] = (num / 100) + '0';\
                         (str)[1] = ((num / 10) % 10) + '0';\
                         (str)[2] = (num % 10) + '0';\
                      }\
              }\
              else\
              {\
                 (str)[0] = '0';\
                 (str)[1] = '.';\
                 (str)[2] = '9';\
                 (str)[3] = 'G';\
              }\
           }\
      else if ((value) < TERABYTE)\
           {\
              if ((value) < 1000000000000LL)\
              {\
                 int num = (value) / GIGABYTE;\
\
                 (str)[3] = 'G';\
                 if (num < 10)\
                 {\
                    num = ((value) * 10) / GIGABYTE;\
                    (str)[0] = (num / 10) + '0';\
                    (str)[1] = '.';\
                    (str)[2] = (num % 10) + '0';\
                 }\
                 else if (num < 100)\
                      {\
                         (str)[0] = ' ';\
                         (str)[1] = (num / 10) + '0';\
                         (str)[2] = (num % 10) + '0';\
                      }\
                      else\
                      {\
                         (str)[0] = (num / 100) + '0';\
                         (str)[1] = ((num / 10) % 10) + '0';\
                         (str)[2] = (num % 10) + '0';\
                      }\
              }\
              else\
              {\
                 (str)[0] = '0';\
                 (str)[1] = '.';\
                 (str)[2] = '9';\
                 (str)[3] = 'T';\
              }\
           }\
      else if ((value) < PETABYTE)\
           {\
              if ((value) < 1000000000000000LL)\
              {\
                 int num = (value) / TERABYTE;\
\
                 (str)[3] = 'T';\
                 if (num < 10)\
                 {\
                    num = ((value) * 10) / TERABYTE;\
                    (str)[0] = (num / 10) + '0';\
                    (str)[1] = '.';\
                    (str)[2] = (num % 10) + '0';\
                 }\
                 else if (num < 100)\
                      {\
                         (str)[0] = ' ';\
                         (str)[1] = (num / 10) + '0';\
                         (str)[2] = (num % 10) + '0';\
                      }\
                      else\
                      {\
                         (str)[0] = (num / 100) + '0';\
                         (str)[1] = ((num / 10) % 10) + '0';\
                         (str)[2] = (num % 10) + '0';\
                      }\
              }\
              else\
              {\
                 (str)[0] = '0';\
                 (str)[1] = '.';\
                 (str)[2] = '9';\
                 (str)[3] = 'P';\
              }\
           }\
      else if ((value) < EXABYTE)\
           {\
              if ((value) < 1000000000000000000LL)\
              {\
                 int num = (value) / PETABYTE;\
\
                 (str)[3] = 'P';\
                 if (num < 10)\
                 {\
                    num = ((value) * 10) / PETABYTE;\
                    (str)[0] = (num / 10) + '0';\
                    (str)[1] = '.';\
                    (str)[2] = (num % 10) + '0';\
                 }\
                 else if (num < 100)\
                      {\
                         (str)[0] = ' ';\
                         (str)[1] = (num / 10) + '0';\
                         (str)[2] = (num % 10) + '0';\
                      }\
                      else\
                      {\
                         (str)[0] = (num / 100) + '0';\
                         (str)[1] = ((num / 10) % 10) + '0';\
                         (str)[2] = (num % 10) + '0';\
                      }\
              }\
              else\
              {\
                 (str)[0] = '0';\
                 (str)[1] = '.';\
                 (str)[2] = '9';\
                 (str)[3] = 'E';\
              }\
           }\
           else\
           {\
              int num = (value) / EXABYTE;\
\
              (str)[3] = 'E';\
              if (num < 10)\
              {\
                 (str)[0] = num + '0';\
                 (str)[1] = '.';\
                 num = (value) / EXABYTE;\
                 (str)[2] = (num % 10) + '0';\
              }\
              else if (num < 100)\
                   {\
                      (str)[0] = ' ';\
                      (str)[1] = (num / 10) + '0';\
                      (str)[2] = (num % 10) + '0';\
                   }\
                   else\
                   {\
                      (str)[0] = ((num / 100) % 10) + '0';\
                      (str)[1] = ((num / 10) % 10) + '0';\
                      (str)[2] = (num % 10) + '0';\
                   }\
           }\
   }

/* Function Prototypes. */
extern int    check_host_permissions(char *, char **, int),
              get_dir_id(char *, unsigned int *),
              sfilter(char const *, char const *, char),
              store_host_names(char ***, char *),
              xrec(char, char *, ...);
extern Window get_window_id(pid_t, char *);
extern void   check_window_ids(char *),
              config_log(unsigned int, unsigned int, unsigned int, char *,
                         char *, ...),
              eval_alda_data(char *),
              get_ip_no(char *, char *),
              get_printer_cmd(char *, char *, char *, int *),
              init_color(Display *),
              insert_passwd(char *),
              locate_xy(int, int *, int *),
              lookup_color(XColor *),
              make_xprocess(char *, char *, char **, int),
              print_file_size(char *, off_t),
              read_setup(char *, char *, int *, int *, int *, int *, char ***),
              remove_window_id(pid_t, char *),
              view_data(char *, char *),
              view_data_no_filter(char *, char *, int),
              write_window_id(Window, pid_t, char *),
              write_setup(int, int, int, char *);

#endif /* __ui_common_defs_h */
