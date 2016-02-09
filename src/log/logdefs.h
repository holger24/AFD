/*
 *  logdefs.h - Part of AFD, an automatic file distribution program.
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

#ifndef __logdefs_h
#define __logdefs_h

#define MAX_SYSTEM_LOG_FILES              4            /* Must be > 1!   */
#ifdef LOCK_DEBUG
# define MAX_SYS_LOGFILE_SIZE             41943040
#else
# define MAX_SYS_LOGFILE_SIZE             2097152
#endif
#define MAX_SYS_LOGFILE_SIZE_DEF          "MAX_SYS_LOGFILE_SIZE"
#define SYSTEM_LOG_RESCAN_TIME            10
#define SYSTEM_LOG_NAME                   "SYSTEM_LOG."
#define SYSTEM_LOG_NAME_LENGTH            (sizeof(SYSTEM_LOG_NAME) - 1)
#define SYSTEM_LOG_NAME_ALL               "SYSTEM_LOG.*"
#define MAX_SYSTEM_LOG_FILES_DEF          "MAX_SYSTEM_LOG_FILES"
#define MAX_MAINTAINER_LOG_FILES          5            /* Must be > 1!   */
#define MAX_MAINTAINER_LOGFILE_SIZE       41943040
#define MAX_MAINTAINER_LOGFILE_SIZE_DEF   "MAX_MAINTAINER_LOGFILE_SIZE"
#define MAINTAINER_LOG_RESCAN_TIME        10
#define MAINTAINER_LOG_NAME               "MAINTAINER_LOG."
#define MAINTAINER_LOG_NAME_LENGTH        (sizeof(MAINTAINER_LOG_NAME) - 1)
#define MAINTAINER_LOG_NAME_ALL           "MAINTAINER_LOG.*"
#define MAX_MAINTAINER_LOG_FILES_DEF      "MAX_MAINTAINER_LOG_FILES"
#define MAX_EVENT_LOG_FILES               5            /* Must be > 1!   */
#define MAX_EVE_LOGFILE_SIZE              10485760
#define MAX_EVE_LOGFILE_SIZE_DEF          "MAX_EVE_LOGFILE_SIZE"
#define EVENT_LOG_RESCAN_TIME             60
#define EVENT_LOG_NAME                    "EVENT_LOG."
#define EVENT_LOG_NAME_LENGTH             (sizeof(EVENT_LOG_NAME) - 1)
#define EVENT_LOG_NAME_ALL                "EVENT_LOG.*"
#define MAX_EVENT_LOG_FILES_DEF           "MAX_EVENT_LOG_FILES"
#define MAX_RECEIVE_LOG_FILES             7            /* Must be > 1!   */
#define RECEIVE_LOG_NAME                  "RECEIVE_LOG."
#define RECEIVE_LOG_NAME_LENGTH           (sizeof(RECEIVE_LOG_NAME) - 1)
#define RECEIVE_LOG_NAME_ALL              "RECEIVE_LOG.*"
#define MAX_RECEIVE_LOG_FILES_DEF         "MAX_RECEIVE_LOG_FILES"
#define MAX_TRANSFER_LOG_FILES            7            /* Must be > 1!   */
#define TRANSFER_LOG_NAME                 "TRANSFER_LOG."
#define TRANSFER_LOG_NAME_LENGTH          (sizeof(TRANSFER_LOG_NAME) - 1)
#define TRANSFER_LOG_NAME_ALL             "TRANSFER_LOG.*"
#define MAX_TRANSFER_LOG_FILES_DEF        "MAX_TRANSFER_LOG_FILES"
#define MAX_TRANS_DB_LOG_FILES            3            /* Must be > 1!   */
#define MAX_TRANS_DB_LOGFILE_SIZE         8388608
#define MAX_TRANS_DB_LOGFILE_SIZE_DEF     "MAX_TRANS_DB_LOGFILE_SIZE"
#define MAX_TRANS_DB_LOG_FILES_DEF        "MAX_TRANS_DB_LOG_FILES"
#define TRANS_DB_LOG_RESCAN_TIME          10
#define TRANS_DB_LOG_NAME                 "TRANS_DB_LOG."
#define TRANS_DB_LOG_NAME_LENGTH          (sizeof(TRANS_DB_LOG_NAME) - 1)
#define TRANS_DB_LOG_NAME_ALL             "TRANS_DB_LOG.*"

/* Definitions for the log process of afd_monitor. */
#define MAX_MON_SYS_LOGFILE_SIZE          2097152
#define MAX_MON_SYS_LOGFILE_SIZE_DEF      "MAX_MON_SYS_LOGFILE_SIZE"
#define MAX_MON_SYS_LOG_FILES             4            /* Must be > 1!   */
#define MON_SYS_LOG_RESCAN_TIME           5
#define MON_SYS_LOG_NAME                  "MON_SYS_LOG."
#define MON_SYS_LOG_NAME_LENGTH           (sizeof(MON_SYS_LOG_NAME) - 1)
#define MON_SYS_LOG_NAME_ALL              "MON_SYS_LOG.*"
#define MAX_MON_SYS_LOG_FILES_DEF         "MAX_MON_SYS_LOG_FILES"
#define MAX_MON_LOG_FILES                 14           /* Must be > 1!   */
#define MON_LOG_NAME                      "MONITOR_LOG."
#define MON_LOG_NAME_LENGTH               (sizeof(MON_LOG_NAME) - 1)
#define MON_LOG_NAME_ALL                  "MONITOR_LOG.*"
#define MAX_MON_LOG_FILES_DEF             "MAX_MON_LOG_FILES"

#define BUFFERED_WRITES_BEFORE_FLUSH_FAST 5
#define BUFFERED_WRITES_BEFORE_FLUSH_SLOW 20

/*-----------------------------------------------------------------------*
 * MAX_INPUT_LOG_FILES        - The number of log files that should be kept
 *                              for input logging. If it is set to 10 and
 *                              SWITCH_FILE_TIME is 86400 (i.e. one day),
 *                              you will store the input log for 10 days.
 * MAX_DISTRIBUTION_LOG_FILES - The number of log files that should be kept
 *                              for distribution logging. If it is set to
 *                              10 and SWITCH_FILE_TIME is 86400 (i.e. one
 *                              day), you will store the input log for 10
 *                              days.
 * MAX_OUTPUT_LOG_FILES       - The number of log files that should be kept
 *                              for output logging. If it is set to 10 and
 *                              SWITCH_FILE_TIME is 86400 (i.e. one day),
 *                              you will store the output log for 10 days.
 * MAX_DELETE_LOG_FILES       - Same as above only for the delete log.
 * MAX_PRODUCTION_LOG_FILES   - Same as above only for the production log.
 *-----------------------------------------------------------------------*/
#ifdef _INPUT_LOG
/* Definitions for input logging. */
# define MAX_INPUT_LOG_FILES              7
# define INPUT_BUFFER_FILE                "INPUT_LOG."
# define INPUT_BUFFER_FILE_LENGTH         (sizeof(INPUT_BUFFER_FILE) - 1)
# define INPUT_BUFFER_FILE_ALL            "INPUT_LOG.*"
# define MAX_INPUT_LOG_FILES_DEF          "MAX_INPUT_LOG_FILES"
#endif
#ifdef _DISTRIBUTION_LOG
/* Definitions for input logging. */
# define MAX_DISTRIBUTION_LOG_FILES      7
# define DISTRIBUTION_BUFFER_FILE        "DISTRIBUTION_LOG."
# define DISTRIBUTION_BUFFER_FILE_LENGTH (sizeof(DISTRIBUTION_BUFFER_FILE) - 1)
# define DISTRIBUTION_BUFFER_FILE_ALL    "DISTRIBUTION_LOG.*"
# define MAX_DISTRIBUTION_LOG_FILES_DEF  "MAX_DISTRIBUTION_LOG_FILES"
# define MAX_SEGMENTED_LINES_BUFFERED    100 /* The maximum number of lines  */
                                             /* we may buffer before we data */
                                             /* gets thrown away.            */
# define MAX_HOLD_TIME_SEGMENTED_LINE    120 /* Maximum time in seconds we   */
                                             /* hold a segmented line in     */
                                             /* buffer.                      */
# define SEGMENTED_BUFFER_CHECK_INTERVAL 30  /* Interval at which we need to */
                                             /* check if we have unused      */
                                             /* segmented lines to clear     */
                                             /* them from buffer.            */

#endif
#ifdef _OUTPUT_LOG
/* Definitions for output logging. */
# define MAX_OUTPUT_LOG_FILES             7
# define OUTPUT_BUFFER_FILE               "OUTPUT_LOG."
# define OUTPUT_BUFFER_FILE_LENGTH        (sizeof(OUTPUT_BUFFER_FILE) - 1)
# define OUTPUT_BUFFER_FILE_ALL           "OUTPUT_LOG.*"
# define MAX_OUTPUT_LOG_FILES_DEF         "MAX_OUTPUT_LOG_FILES"
# ifdef WITH_LOG_CACHE
#  define OUTPUT_BUFFER_CACHE_FILE        "OUTPUT_CACHE_LOG."
# endif
#endif
#ifdef _DELETE_LOG
/* Definitions for delete logging. */
# define MAX_DELETE_LOG_FILES             7
# define DELETE_BUFFER_FILE               "DELETE_LOG."
# define DELETE_BUFFER_FILE_LENGTH        (sizeof(DELETE_BUFFER_FILE) - 1)
# define DELETE_BUFFER_FILE_ALL           "DELETE_LOG.*"
# define MAX_DELETE_LOG_FILES_DEF         "MAX_DELETE_LOG_FILES"
#endif
#ifdef _PRODUCTION_LOG
# define MAX_PRODUCTION_LOG_FILES         7
# define PRODUCTION_BUFFER_FILE           "PRODUCTION_LOG."
# define PRODUCTION_BUFFER_FILE_LENGTH    (sizeof(PRODUCTION_BUFFER_FILE) - 1)
# define PRODUCTION_BUFFER_FILE_ALL       "PRODUCTION_LOG.*"
# define MAX_PRODUCTION_LOG_FILES_DEF     "MAX_PRODUCTION_LOG_FILES"
#endif
#define MAX_LOG_NAME_LENGTH               17 /* Max length of:           */
                                             /* SYSTEM_LOG_NAME          */
                                             /* RECEIVE_LOG_NAME         */
                                             /* TRANSFER_LOG_NAME        */
                                             /* TRANS_DB_LOG_NAME        */
                                             /* INPUT_BUFFER_FILE        */
                                             /* DISTRIBUTION_BUFFER_FILE */
                                             /* OUTPUT_BUFFER_FILE       */
                                             /* DELETE_BUFFER_FILE       */
                                             /* PRODUCTION_BUFFER_FILE   */
#define MAX_LOG_DEF_NAME_LENGTH           26 /* Max length of:               */
                                             /* MAX_SYSTEM_LOG_FILES_DEF     */
                                             /* MAX_RECEIVE_LOG_FILES_DEF    */
                                             /* MAX_TRANSFER_LOG_FILES_DEF   */
                                             /* MAX_TRANS_DB_LOG_FILES_DEF   */
                                             /* MAX_INPUT_LOG_FILES_DEF      */
                                             /* MAX_OUTPUT_LOG_FILES_DEF     */
                                             /* MAX_DELETE_LOG_FILES_DEF     */
                                             /* MAX_PRODUCTION_LOG_FILES_DEF */

#ifdef _DISTRIBUTION_LOG
/* Structure for distribution_log to store segmented lines. */
struct buffered_line
       {
          char         *line;
          int          buffer_length;
          int          line_offset;
          unsigned int did;
          unsigned int unique_number;
          time_t       entry_time;
       };
#endif

/* Function prototypes. */
extern int  event_logger(FILE *, off_t, int, int),
            fprint_dup_msg(FILE *, int, char *, char *, int, time_t),
            logger(FILE *, off_t, int, int);
#ifdef WITH_LOG_CACHE
extern FILE *open_log_file(char *, char *, int *, off_t *);
#else
extern FILE *open_log_file(char *);
#endif

#endif /* __logdefs_h */
