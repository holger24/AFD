/*
 *  afdd_common_defs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __afdd_common_defs_h
#define __afdd_common_defs_h

#include "../log/logdefs.h" /* MAX_LOG_NAME_LENGTH */

#define NOT_SET                  -1
#define DEFAULT_FILE_NO          0
#define HUNK_MAX                 4096
#define MAX_LOG_DATA_BUFFER      131072 /* 128KB */
/********************************************************************/
/* NOTE: MAX_LOG_DATA_BUFFER _MUST_ be divisible by MAX_LINE_LENGTH */
/*       and there may NOT be a rest!                               */
/********************************************************************/
#define MAX_LOG_COMMAND_LENGTH   (2 + 1 + MAX_INT_LENGTH + 1 + \
                                  MAX_INT_LENGTH + 1 + MAX_INT_LENGTH + 1)
#define DEFAULT_AFDD_LOG_DEFS    0
#define DEFAULT_FILE_NO          0
#define EVERYTHING               -1
#define AFDD_CMD_TIMEOUT         900
#define AFDD_LOG_CHECK_INTERVAL  2
#define MAX_AFDD_CONNECTIONS     5
#define MAX_AFDD_CONNECTIONS_DEF "MAX_AFDD_CONNECTIONS"
#define AFD_SHUTTING_DOWN        124
#define LOG_WRITE_INTERVAL       30 /* Interval at which we must write */
                                    /* some log data before afd_mon    */
                                    /* thinks that the connection is   */
                                    /* dead and disconnects.           */

#define DEFAULT_CHECK_INTERVAL   3 /* Default interval in seconds to */
                                   /* check if certain values have   */
                                   /* changed in the FSA.            */

#define HELP_CMD              "HELP\r\n"
#define QUIT_CMD              "QUIT\r\n"
#define TRACEI_CMD            "TRACEI"
#define TRACEI_CMD_LENGTH     (sizeof(TRACEI_CMD) - 1)
#define TRACEI_CMDL           "TRACEI\r\n"
#define TRACEO_CMD            "TRACEO"
#define TRACEO_CMD_LENGTH     (sizeof(TRACEO_CMD) - 1)
#define TRACEO_CMDL           "TRACEO\r\n"
#define TRACEF_CMD            "TRACEF"
#define TRACEF_CMD_LENGTH     (sizeof(TRACEF_CMD) - 1)
#define TRACEF_CMDL           "TRACEF\r\n"
#define ILOG_CMD              "ILOG"
#define ILOG_CMD_LENGTH       (sizeof(ILOG_CMD) - 1)
#define ILOG_CMDL             "ILOG\r\n"
#define OLOG_CMD              "OLOG"
#define OLOG_CMD_LENGTH       (sizeof(OLOG_CMD) - 1)
#define OLOG_CMDL             "OLOG\r\n"
#define SLOG_CMD              "SLOG"
#define SLOG_CMD_LENGTH       (sizeof(SLOG_CMD) - 1)
#define SLOG_CMDL             "SLOG\r\n"
#define TLOG_CMD              "TLOG"
#define TLOG_CMD_LENGTH       (sizeof(TLOG_CMD) - 1)
#define TLOG_CMDL             "TLOG\r\n"
#define TDLOG_CMD             "TDLOG"
#define TDLOG_CMD_LENGTH      (sizeof(TDLOG_CMD) - 1)
#define TDLOG_CMDL            "TDLOG\r\n"
#define PROC_CMD              "PROC\r\n"
#define PROC_CMD_LENGTH       (sizeof(PROC_CMD) - 1)
#define DISC_CMD              "DISC\r\n"
#define DISC_CMD_LENGTH       (sizeof(DISC_CMD) - 1)
#define STAT_CMD              "STAT"
#define STAT_CMD_LENGTH       (sizeof(STAT_CMD) - 1)
#define STAT_CMDL             "STAT\r\n"
#define HSTAT_CMD             "HSTAT"
#define HSTAT_CMD_LENGTH      (sizeof(HSTAT_CMD) - 1)
#define HSTAT_CMDL            "HSTAT\r\n"
#define START_STAT_CMD        "SSTAT"
#define START_STAT_CMD_LENGTH (sizeof(START_STAT_CMD) - 1)
#define START_STAT_CMDL       "SSTAT\r\n"
#define LDB_CMD               "LDB\r\n"
#define LDB_CMD_LENGTH        (sizeof(LDB_CMD) - 1)
#define LRF_CMD               "LRF\r\n"
#define LRF_CMD_LENGTH        (sizeof(LRF_CMD) - 1)
#define INFO_CMD              "INFO "
#define INFO_CMD_LENGTH       (sizeof(INFO_CMD) - 1)
#define INFO_CMDL             "INFO\r\n"
#define AFDSTAT_CMD           "AFDSTAT"
#define AFDSTAT_CMD_LENGTH    (sizeof(AFDSTAT_CMD) - 1)
#define AFDSTAT_CMDL          "AFDSTAT\r\n"
#define LOG_CMD               "LOG"
#define LOG_CMD_LENGTH        (sizeof(LOG_CMD) - 1)
#define LOG_CMDL              "LOG\r\n"
#define NOP_CMD               "NOP"
#define NOP_CMD_LENGTH        (sizeof(NOP_CMD) - 1)
#define NOP_CMDL              "NOP\r\n"

#define QUIT_SYNTAX           "214 Syntax: QUIT (terminate service)"
#define HELP_SYNTAX           "214 Syntax: HELP [ <sp> <string> ]"
#define TRACEI_SYNTAX         "214 Syntax: TRACEI [<sp> <file name>] (trace input)"
#define TRACEO_SYNTAX         "214 Syntax: TRACEO [<sp> <file name>] (trace output)"
#define TRACEF_SYNTAX         "214 Syntax: TRACEF [<sp> <file name>] (trace input)"
#define ILOG_SYNTAX           "214 Syntax: ILOG [<sp> <search string>] [<sp> -<lines>] [<sp> +<duration>] [<sp> #<log number>] (input log)"
#define OLOG_SYNTAX           "214 Syntax: OLOG [<sp> <search string>] [<sp> -<lines>] [<sp> +<duration>] [<sp> #<log number>] (output log)"
#define SLOG_SYNTAX           "214 Syntax: SLOG [<sp> <search string>] [<sp> -<lines>] [<sp> +<duration>] [<sp> #<log number>] (system log)"
#define TLOG_SYNTAX           "214 Syntax: TLOG [<sp> <search string>] [<sp> -<lines>] [<sp> +<duration>] [<sp> #<log number>] (transfer log)"
#define TDLOG_SYNTAX          "214 Syntax: TDLOG [<sp> <search string>] [<sp> -<lines>] [<sp> +<duration>] [<sp> #<log number>] (transfer debug log)"
#define PROC_SYNTAX           "214 Syntax: PROC (shows all process of the AFD)"
#define DISC_SYNTAX           "214 Syntax: DISC (shows disk usage)"
#define STAT_SYNTAX           "214 Syntax: STAT [<sp> <host name>] [<sp> -H | -D | -Y [<sp> n]]"
#define HSTAT_SYNTAX          "214 Syntax: HSTAT (shows all host statistics and status)"
#define START_STAT_SYNTAX     "214 Syntax: SSTAT (start summary status of AFD)"
#define LDB_SYNTAX            "214 Syntax: LDB (list AMG database)"
#define LRF_SYNTAX            "214 Syntax: LRF (list rename file)"
#define INFO_SYNTAX           "214 Syntax: INFO <sp> <host name>"
#define AFDSTAT_SYNTAX        "214 Syntax: AFDSTAT [<sp> <host name>]"
#define NOP_SYNTAX            "214 Syntax: NOP (checks if connection is still up)"
#define LOG_SYNTAX            "214 Syntax: LOG <sp> <log type> <sp> <options> <sp> <date> <offset>"
#define LOG_TYPES_SYNTAX      "214         log types: LS,LE,LR,LT,LB,LI,LU,LP,LO,LD,LN,JD"

/* Definitions for the different logs in the logdata array. */
#define SYS_LOG_POS           0
#define EVE_LOG_POS           1
#define REC_LOG_POS           2
#define TRA_LOG_POS           3
#define TDB_LOG_POS           4
#ifdef _INPUT_LOG
# define INP_LOG_POS          5
# ifdef _DISTRIBUTION_LOG
#  define DIS_LOG_POS         6
#  ifdef _PRODUCTION_LOG
#   define PRO_LOG_POS        7
#   ifdef _OUTPUT_LOG
#    define OUT_LOG_POS       8
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      9
#     define DUM_LOG_POS      10
#     define NO_OF_LOGS       11
#    else
#     define DUM_LOG_POS      9
#     define NO_OF_LOGS       10
#    endif
#   else
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      8
#     define DUM_LOG_POS      9
#     define NO_OF_LOGS       10
#    else
#     define DUM_LOG_POS      7
#     define NO_OF_LOGS       8
#    endif
#   endif
#  else
#   ifdef _OUTPUT_LOG
#    define OUT_LOG_POS       7
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      8
#     define DUM_LOG_POS      9
#     define NO_OF_LOGS       10
#    else
#     define DUM_LOG_POS      7
#     define NO_OF_LOGS       8
#    endif
#   else
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      7
#     define DUM_LOG_POS      8
#     define NO_OF_LOGS       9
#    else
#     define DUM_LOG_POS      7
#     define NO_OF_LOGS       8
#    endif
#   endif
#  endif
# else
#  ifdef _PRODUCTION_LOG
#   define PRO_LOG_POS        6
#   ifdef _OUTPUT_LOG
#    define OUT_LOG_POS       7
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      8
#     define DUM_LOG_POS      9
#     define NO_OF_LOGS       10
#    else
#     define DUM_LOG_POS      8
#     define NO_OF_LOGS       9
#    endif
#   else
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      7
#     define DUM_LOG_POS      8
#     define NO_OF_LOGS       9
#    else
#     define DUM_LOG_POS      6
#     define NO_OF_LOGS       7
#    endif
#   endif
#  else
#   ifdef _OUTPUT_LOG
#    define OUT_LOG_POS       6
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      7
#     define DUM_LOG_POS      8
#     define NO_OF_LOGS       9
#    else
#     define DUM_LOG_POS      6
#     define NO_OF_LOGS       7
#    endif
#   else
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      6
#     define DUM_LOG_POS      7
#     define NO_OF_LOGS       8
#    else
#     define DUM_LOG_POS      6
#     define NO_OF_LOGS       7
#    endif
#   endif
#  endif
# endif
#else
# ifdef _DISTRIBUTION_LOG
#  define DIS_LOG_POS         5
#  ifdef _PRODUCTION_LOG
#   define PRO_LOG_POS        6
#   ifdef _OUTPUT_LOG
#    define OUT_LOG_POS       7
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      8
#     define DUM_LOG_POS      9
#     define NO_OF_LOGS       10
#    else
#     define DUM_LOG_POS      7
#     define NO_OF_LOGS       8
#    endif
#   else
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      7
#     define DUM_LOG_POS      8
#     define NO_OF_LOGS       9
#    else
#     define DUM_LOG_POS      7
#     define NO_OF_LOGS       8
#    endif
#   endif
#  else
#   ifdef _OUTPUT_LOG
#    define OUT_LOG_POS       6
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      7
#     define DUM_LOG_POS      8
#     define NO_OF_LOGS       9
#    else
#     define DUM_LOG_POS      7
#     define NO_OF_LOGS       8
#    endif
#   else
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      6
#     define DUM_LOG_POS      7
#     define NO_OF_LOGS       8
#    else
#     define DUM_LOG_POS      6
#     define NO_OF_LOGS       7
#    endif
#   endif
#  endif
# else
#  ifdef _PRODUCTION_LOG
#   define PRO_LOG_POS        5
#   ifdef _OUTPUT_LOG
#    define OUT_LOG_POS       6
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      7
#     define DUM_LOG_POS      8
#     define NO_OF_LOGS       9
#    else
#     define DUM_LOG_POS      6
#     define NO_OF_LOGS       7
#    endif
#   else
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      6
#     define DUM_LOG_POS      7
#     define NO_OF_LOGS       8
#    else
#     define DUM_LOG_POS      6
#     define NO_OF_LOGS       7
#    endif
#   endif
#  else
#   ifdef _OUTPUT_LOG
#    define OUT_LOG_POS       5
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      6
#     define DUM_LOG_POS      7
#     define NO_OF_LOGS       8
#    else
#     define DUM_LOG_POS      6
#     define NO_OF_LOGS       7
#    endif
#   else
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      5
#     define DUM_LOG_POS      6
#     define NO_OF_LOGS       7
#    else
#     define DUM_LOG_POS      5
#     define NO_OF_LOGS       6
#    endif
#   endif
#  endif
# endif
#endif

/* Structure to hold all relevant log managemant data. */
#define FIRST_POS_SET         1
struct logdata
       {
          char         log_name[MAX_LOG_NAME_LENGTH + 1];
          char         log_data_cmd[3];
          char         log_inode_cmd[3];
          FILE         *fp;
          ino_t        current_log_inode;
          off_t        offset;
          int          current_log_no;
          int          log_name_length;
          unsigned int log_flag;
          unsigned int options;
          unsigned int packet_no;
          unsigned int flag;
       };

/* Structure to hold open file descriptors. */
#define AFDD_ILOG_NO             0
#define AFDD_OLOG_NO             1
#define AFDD_SLOG_NO             2
#define AFDD_TLOG_NO             3
#define AFDD_TDLOG_NO            4
#define MAX_AFDD_LOG_FILES       5
struct fd_cache
       {
          ino_t st_ino;
          int   fd;
       };


/* Function Prototypes. */
extern long check_logs(time_t);
extern void close_get_display_data(void),
            init_get_display_data(void);
extern int  get_free_connection(const int);

#endif /* __afdd_common_defs_h */
