/*
 *  aldadefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 - 2016 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __aldadefs_h
#define __aldadefs_h

#ifdef TM_IN_SYS_TIME
# include <sys/time.h>                /* struct tm                       */
#else
# include <time.h>                    /* struct tm                       */
#endif

#ifndef WITH_AFD_MON
# define MAX_AFDNAME_LENGTH 12
#endif

/* Definitions for evaluating log data (alda). */
#define ALDA_CONTINUOUS_MODE                1
#define ALDA_CONTINUOUS_DAEMON_MODE         2
#define ALDA_LOCAL_MODE                     4
#define ALDA_REMOTE_MODE                    8
#define ALDA_BACKWARD_MODE                  16
#define ALDA_FORWARD_MODE                   32

#define N_TO_1_CACHE_STEP_SIZE              20

#ifdef HAVE_GETLINE
# define MAX_INPUT_LINE_LENGTH              256
#else
# define MAX_INPUT_LINE_LENGTH              (MAX_LINE_LENGTH + 1)
#endif
#define MAX_OUTPUT_LINE_LENGTH              4096
#define MAX_FORMAT_ORIENTATION_LENGTH       (1 + 1 + 1 + MAX_INT_LENGTH + 1 + MAX_INT_LENGTH + 2 + 1 + 1)
#define DEFAULT_MAX_DIFF_TIME               3600L
#define DEFAULT_ROTATE_LIMIT                7

#define DEFAULT_BASE_CHAR                   'd'

/* Definitions for the different protocols. */
#define ALDA_FTP                            FTP
#define ALDA_FTP_FLAG                       1
#define ALDA_FTP_SHEME                      FTP_SHEME
#define ALDA_FTP_SHEME_LENGTH               (sizeof(ALDA_FTP_SHEME) - 1)
#define ALDA_LOC                            LOC
#define ALDA_LOC_FLAG                       2
#define ALDA_LOC_SHEME                      LOC_SHEME
#define ALDA_LOC_SHEME_LENGTH               (sizeof(ALDA_LOC_SHEME) - 1)
#define ALDA_SMTP                           SMTP
#define ALDA_SMTP_FLAG                      4
#define ALDA_SMTP_SHEME                     SMTP_SHEME
#define ALDA_SMTP_SHEME_LENGTH              (sizeof(ALDA_SMTP_SHEME) - 1)
#define ALDA_SFTP                           SFTP
#define ALDA_SFTP_FLAG                      8
#define ALDA_SFTP_SHEME                     SFTP_SHEME
#define ALDA_SFTP_SHEME_LENGTH              (sizeof(ALDA_SFTP_SHEME) - 1)
#define ALDA_SCP                            4     /* Check init_afd/afddefs.h */
#define ALDA_SCP_FLAG                       16
#define ALDA_SCP_SHEME                      "scp" /* Check init_afd/afddefs.h */
#define ALDA_SCP_SHEME_LENGTH               (sizeof(ALDA_SCP_SHEME) - 1)
#define ALDA_HTTP                           HTTP
#define ALDA_HTTP_FLAG                      32
#define ALDA_HTTP_SHEME                     HTTP_SHEME
#define ALDA_HTTP_SHEME_LENGTH              (sizeof(ALDA_HTTP_SHEME) - 1)
#define ALDA_HTTPS                          8     /* Check init_afd/afddefs.h */
#define ALDA_HTTPS_FLAG                     64
#define ALDA_HTTPS_SHEME                    "https"/* Check init_afd/afddefs.h */
#define ALDA_HTTPS_SHEME_LENGTH             (sizeof(ALDA_HTTPS_SHEME) - 1)
#define ALDA_FTPS                           7     /* Check init_afd/afddefs.h */
#define ALDA_FTPS_FLAG                      128
#define ALDA_FTPS_SHEME                     "ftps"/* Check init_afd/afddefs.h */
#define ALDA_FTPS_SHEME_LENGTH              (sizeof(ALDA_FTPS_SHEME) - 1)
#define ALDA_WMO                            5     /* Check init_afd/afddefs.h */
#define ALDA_WMO_FLAG                       256
#define ALDA_WMO_SHEME                      "wmo" /* Check init_afd/afddefs.h */
#define ALDA_WMO_SHEME_LENGTH               (sizeof(ALDA_WMO_SHEME) - 1)
#define ALDA_MAP                            3     /* Check init_afd/afddefs.h */
#define ALDA_MAP_FLAG                       512
#define ALDA_MAP_SHEME                      "map" /* Check init_afd/afddefs.h */
#define ALDA_MAP_SHEME_LENGTH               (sizeof(ALDA_MAP_SHEME) - 1)
#define ALDA_EXEC                           EXEC
#define ALDA_EXEC_FLAG                      1024
#define ALDA_EXEC_SHEME                     "exec" /* Check init_afd/afddefs.h */
#define ALDA_EXEC_SHEME_LENGTH              (sizeof(ALDA_EXEC_SHEME) - 1)
#define ALDA_DFAX                           12    /* Check init_afd/afddefs.h */
#define ALDA_DFAX_FLAG                      2048
#define ALDA_DFAX_SHEME                     "dfax" /* Check init_afd/afddefs.h */
#define ALDA_DFAX_SHEME_LENGTH              (sizeof(ALDA_DFAX_SHEME) - 1)
#define ALDA_DE_MAIL                        13    /* Check init_afd/afddefs.h */
#define ALDA_DE_MAIL_FLAG                   4096
#define ALDA_DEMAIL_SHEME                   "demail" /* Check init_afd/afddefs.h */
#define ALDA_DEMAIL_SHEME_LENGTH            (sizeof(ALDA_DEMAIL_SHEME) - 1)
#define ALDA_UNKNOWN_SHEME                  "unknown"
#define ALDA_UNKNOWN_SHEME_LENGTH           (sizeof(ALDA_UNKNOWN_SHEME) - 1)

/* Definitions for which log data we want to search. */
#define SEARCH_INPUT_LOG                    1
#ifdef _INPUT_LOG
# define DEFAULT_OUTPUT_INPUT_FORMAT        "%ITd %ITX|%-40IF|%11ISB|%4xIU|%9xII|%IN"
# define RESET_ILOG()                      \
         {                                 \
            ilog.bd_input_time.tm_mday = 0;\
            ilog.filename[0] = '\0';       \
            ilog.full_source[0] = '\0';    \
            ilog.file_size = -1;           \
            ilog.input_time = -1L;         \
            ilog.filename_length = 0;      \
            ilog.full_source_length = 0;   \
            ilog.dir_id = 0;               \
            ilog.unique_number = 0;        \
         }
# ifdef _DISTRIBUTION_LOG
#  define RESET_ULOG()                                  \
          {                                             \
             ulog.bd_distribution_time.tm_mday = 0;     \
             ulog.bd_input_time.tm_mday = 0;            \
             ulog.filename[0] = '\0';                   \
             ulog.file_size = -1;                       \
             ulog.distribution_time = -1L;              \
             ulog.input_time = -1L;                     \
             ulog.filename_length = 0;                  \
             ulog.no_of_dist_jobs = 0;                  \
             ulog.djid_buffer_length = 0;               \
             ulog.dir_id = 0;                           \
             ulog.no_of_distribution_types = 0;         \
             ulog.unique_number = 0;                    \
             ulog.job_id_list = NULL;                   \
             ulog.proc_cycles = NULL;                   \
             ulog.distribution_type = (unsigned char)-1;\
          }
#  define RESET_ULOG_PART()                             \
          {                                             \
             ulog.bd_distribution_time.tm_mday = 0;     \
             ulog.bd_input_time.tm_mday = 0;            \
             ulog.filename[0] = '\0';                   \
             ulog.file_size = -1;                       \
             ulog.distribution_time = -1L;              \
             ulog.input_time = -1L;                     \
             ulog.filename_length = 0;                  \
             ulog.no_of_dist_jobs = 0;                  \
             ulog.dir_id = 0;                           \
             ulog.unique_number = 0;                    \
             ulog.distribution_type = (unsigned char)-1;\
          }
# endif
#else
# ifdef _DISTRIBUTION_LOG
#  define RESET_ULOG()                                  \
          {                                             \
             ulog.bd_distribution_time.tm_mday = 0;     \
             ulog.bd_input_time.tm_mday = 0;            \
             ulog.filename[0] = '\0';                   \
             ulog.full_source[0] = '\0';                \
             ulog.file_size = -1;                       \
             ulog.distribution_time = -1L;              \
             ulog.input_time = -1L;                     \
             ulog.filename_length = 0;                  \
             ulog.full_source_length = 0;               \
             ulog.no_of_dist_jobs = 0;                  \
             ulog.djid_buffer_length = 0;               \
             ulog.dir_id = 0;                           \
             ulog.no_of_distribution_types = 0;         \
             ulog.unique_number = 0;                    \
             ulog.job_id_list = NULL;                   \
             ulog.proc_cycles = NULL;                   \
             ulog.distribution_type = (unsigned char)-1;\
          }
#  define RESET_ULOG_PART()                             \
          {                                             \
             ulog.bd_distribution_time.tm_mday = 0;     \
             ulog.bd_input_time.tm_mday = 0;            \
             ulog.filename[0] = '\0';                   \
             ulog.full_source[0] = '\0';                \
             ulog.file_size = -1;                       \
             ulog.distribution_time = -1L;              \
             ulog.input_time = -1L;                     \
             ulog.filename_length = 0;                  \
             ulog.full_source_length = 0;               \
             ulog.no_of_dist_jobs = 0;                  \
             ulog.dir_id = 0;                           \
             ulog.unique_number = 0;                    \
             ulog.distribution_type = (unsigned char)-1;\
          }
# endif
#endif
#define SEARCH_DISTRIBUTION_LOG             2
#ifdef _DISTRIBUTION_LOG
# define DEFAULT_OUTPUT_DISTRIBUTION_FORMAT "%UTd %UTX|%-40UF|%11USB|%4xUU|%9xUI|%Un|%xUj,|%xUc,"
#endif
#define SEARCH_PRODUCTION_LOG               4
#ifdef _PRODUCTION_LOG
# define DEFAULT_OUTPUT_PRODUCTION_FORMAT   "%Ptd %PtX (%PDX)|%-30Pf|%-30PF|%11PSB|%9xPJ|%4xPU|%4xPL|%11dPR|%PC"
# define RESET_PLOG()                         \
         {                                    \
            plog.bd_input_time.tm_mday = 0;   \
            plog.bd_output_time.tm_mday = 0;  \
            plog.original_filename[0] = '\0'; \
            plog.new_filename[0] = '\0';      \
            plog.what_done[0] = '\0';         \
            plog.cpu_time = 0.0;              \
            plog.production_time = 0.0;       \
            plog.original_file_size = -1;     \
            plog.new_file_size = -1;          \
            plog.input_time = -1L;            \
            plog.output_time = -1L;           \
            plog.original_filename_length = 0;\
            plog.new_filename_length = 0;     \
            plog.what_done_length = 0;        \
            plog.return_code = 0;             \
            plog.ratio_1 = 0;                 \
            plog.ratio_2 = 0;                 \
            plog.dir_id = 0;                  \
            plog.job_id = 0;                  \
            plog.unique_number = 0;           \
            plog.split_job_counter = 0;       \
         }
#endif
#define SEARCH_OUTPUT_LOG                   8
#ifdef _OUTPUT_LOG
# define DEFAULT_OUTPUT_OUTPUT_FORMAT       "%Otd %OtX - %OTd %OTX (%ODX)|%-6OP|%2dOp|%-8OH|%-14Oh|%9xOJ|%4xOU|%4xOL|%11OSB|%11dOe|%-30Of|%-30OF|%OA"
# define RESET_OLOG()                             \
         {                                        \
            olog.bd_job_creation_time.tm_mday = 0;\
            olog.bd_send_start_time.tm_mday = 0;  \
            olog.bd_output_time.tm_mday = 0;      \
            olog.local_filename[0] = '\0';        \
            olog.remote_name[0] = '\0';           \
            olog.archive_dir[0] = '\0';           \
            olog.alias_name[0] = '\0';            \
            olog.real_hostname[0] = '\0';         \
            olog.recipient[0] = '\0';             \
            olog.mail_id[0] = '\0';               \
            olog.transmission_time = 0.0;         \
            olog.file_size = -1;                  \
            olog.job_creation_time = -1L;         \
            olog.send_start_time = -1L;           \
            olog.output_time = -1L;               \
            olog.local_filename_length = 0;       \
            olog.remote_name_length = 0;          \
            olog.archive_dir_length = 0;          \
            olog.alias_name_length = 0;           \
            olog.mail_id_length = 0;              \
            olog.output_type = OT_UNKNOWN;        \
            olog.current_toggle = 0;              \
            olog.job_id = 0;                      \
            olog.dir_id = 0;                      \
            olog.unique_number = 0;               \
            olog.split_job_counter = 0;           \
            olog.protocol = 0;                    \
            olog.retries = 0;                     \
         }
#endif
#define SEARCH_DELETE_LOG                   16
#ifdef _DELETE_LOG
# define DEFAULT_OUTPUT_DELETE_FORMAT       "%DTd %DTX|%-30DF|%11DSB|%-10DW|%-36DR|%9xDr|%9xDJ|%9xDI|%-8DH|%DA"
# define RESET_DLOG()                             \
         {                                        \
            dlog.bd_job_creation_time.tm_mday = 0;\
            dlog.bd_delete_time.tm_mday = 0;      \
            dlog.filename[0] = '\0';              \
            dlog.alias_name[0] = '\0';            \
            dlog.user_process[0] = '\0';          \
            dlog.add_reason[0] = '\0';            \
            dlog.file_size = -1;                  \
            dlog.job_creation_time = -1L;         \
            dlog.delete_time = -1L;               \
            dlog.filename_length = 0;             \
            dlog.alias_name_length = 0;           \
            dlog.user_process_length = 0;         \
            dlog.add_reason_length = 0;           \
            dlog.job_id = 0;                      \
            dlog.dir_id = 0;                      \
            dlog.deletion_type = 0;               \
            dlog.unique_number = 0;               \
            dlog.split_job_counter = 0;           \
         }
#endif
#if defined (_INPUT_LOG) && defined (_OUTPUT_LOG) && defined (_DELETE_LOG)
# define SEARCH_ALL_LOGS                    (SEARCH_INPUT_LOG | SEARCH_OUTPUT_LOG | SEARCH_DELETE_LOG)
# define DEFAULT_OUTPUT_ALL_FORMAT          "%ITd %ITX|%-30IF|%11ISB|+|%OTd %OTX (%ODX)|%-6OP|%-8OH|%-30OE|+|%DTd %DTX|%DR"
#endif
#if defined (_INPUT_LOG) && defined (_DISTRIBUTION_LOG) && defined (_PRODUCTION_LOG) && defined (_OUTPUT_LOG) && defined (_DELETE_LOG)
# undef  SEARCH_ALL_LOGS
# define SEARCH_ALL_LOGS                    (SEARCH_INPUT_LOG | SEARCH_DISTRIBUTION_LOG | SEARCH_PRODUCTION_LOG | SEARCH_OUTPUT_LOG | SEARCH_DELETE_LOG)
# undef  DEFAULT_OUTPUT_ALL_FORMAT
# define DEFAULT_OUTPUT_ALL_FORMAT          "%ITd %ITX|%-30IF|%11ISB|+|%Ptd %PtX (%PDX)|%-30PF|%11PSB|+|%OTd %OTX (%ODX)|%-6OP|%-8OH|%-30OE|+|%DTd %DTX|%DR"
#endif
#if defined (_INPUT_LOG) && defined (_DISTRIBUTION_LOG) && defined (_PRODUCTION_LOG) && defined (_OUTPUT_LOG) && !defined (_DELETE_LOG)
# undef  SEARCH_ALL_LOGS
# define SEARCH_ALL_LOGS                    (SEARCH_INPUT_LOG | SEARCH_DISTRIBUTION_LOG | SEARCH_PRODUCTION_LOG | SEARCH_OUTPUT_LOG)
# undef  DEFAULT_OUTPUT_ALL_FORMAT
# define DEFAULT_OUTPUT_ALL_FORMAT          "%ITd %ITX|%-30IF|%11ISB|+|%Ptd %PtX (%PDX)|%-30PF|%11PSB|+|%OTd %OTX (%ODX)|%-6OP|%-8OH|%-30OE"
#endif
#if defined (_INPUT_LOG) && defined (_DISTRIBUTION_LOG) && defined (_PRODUCTION_LOG) && !defined (_OUTPUT_LOG) && defined (_DELETE_LOG)
# undef  SEARCH_ALL_LOGS
# define SEARCH_ALL_LOGS                    (SEARCH_INPUT_LOG | SEARCH_DISTRIBUTION_LOG | SEARCH_PRODUCTION_LOG | SEARCH_DELETE_LOG)
# undef  DEFAULT_OUTPUT_ALL_FORMAT
# define DEFAULT_OUTPUT_ALL_FORMAT          "%ITd %ITX|%-30IF|%11ISB|+|%Ptd %PtX (%PDX)|%-30PF|%11PSB|+|%DTd %DTX|%DR"
#endif
#if defined (_INPUT_LOG) && defined (_DISTRIBUTION_LOG) && !defined (_PRODUCTION_LOG) && defined (_OUTPUT_LOG) && defined (_DELETE_LOG)
# undef  SEARCH_ALL_LOGS
# define SEARCH_ALL_LOGS                    (SEARCH_INPUT_LOG | SEARCH_DISTRIBUTION_LOG | SEARCH_OUTPUT_LOG | SEARCH_DELETE_LOG)
# undef  DEFAULT_OUTPUT_ALL_FORMAT
# define DEFAULT_OUTPUT_ALL_FORMAT          "%ITd %ITX|%-30IF|%11ISB|+|%OTd %OTX (%ODX)|%-6OP|%-8OH|%-30OE|+|%DTd %DTX|%DR"
#endif
#if defined (_INPUT_LOG) && !defined (_DISTRIBUTION_LOG) && defined (_PRODUCTION_LOG) && defined (_OUTPUT_LOG) && defined (_DELETE_LOG)
# undef  SEARCH_ALL_LOGS
# define SEARCH_ALL_LOGS                    (SEARCH_INPUT_LOG | SEARCH_DISTRIBUTION_LOG | SEARCH_OUTPUT_LOG | SEARCH_DELETE_LOG)
# undef  DEFAULT_OUTPUT_ALL_FORMAT
# define DEFAULT_OUTPUT_ALL_FORMAT          "%ITd %ITX|%-30IF|%11ISB|+|%Ptd %PtX (%PDX)|%-30PF|%11PSB|+|%OTd %OTX (%ODX)|%-6OP|%-8OH|%-30OE|+|%DTd %DTX|%DR"
#endif
#if !defined (_INPUT_LOG) && defined (_DISTRIBUTION_LOG) && defined (_PRODUCTION_LOG) && defined (_OUTPUT_LOG) && defined (_DELETE_LOG)
# undef  SEARCH_ALL_LOGS
# define SEARCH_ALL_LOGS                    (SEARCH_PRODUCTION_LOG | SEARCH_OUTPUT_LOG | SEARCH_DELETE_LOG)
# undef  DEFAULT_OUTPUT_ALL_FORMAT
# define DEFAULT_OUTPUT_ALL_FORMAT          "%UTd %UTX|%-30UF|%11USB|+|%Ptd %PtX (%PDX)|%-30Pf|%-30PF|%11PSB|+|%OTd %OTX (%ODX)|%-6OP|%-8OH|%-30OE|+|%DTd %DTX|%DR"
#endif
#if !defined (_INPUT_LOG) && defined (_DISTRIBUTION_LOG) && !defined (_PRODUCTION_LOG) && defined (_OUTPUT_LOG) && defined (_DELETE_LOG)
# undef  SEARCH_ALL_LOGS
# define SEARCH_ALL_LOGS                    (SEARCH_DISTRIBUTION_LOG | SEARCH_OUTPUT_LOG | SEARCH_DELETE_LOG)
# undef  DEFAULT_OUTPUT_ALL_FORMAT
# define DEFAULT_OUTPUT_ALL_FORMAT          "%UTd %UTX|%-30UF|%11USB|+|%OTd %OTX (%ODX)|%-6OP|%-8OH|%-30OE|%11OSB|%-30OF|+|%DTd %DTX|%-20DF|%20DSB|%DR"
#endif
#if !defined (_INPUT_LOG) && !defined (_DISTRIBUTION_LOG) && !defined (_PRODUCTION_LOG) && defined (_OUTPUT_LOG) && defined (_DELETE_LOG)
# undef  SEARCH_ALL_LOGS
# define SEARCH_ALL_LOGS                    (SEARCH_OUTPUT_LOG | SEARCH_DELETE_LOG)
# undef  DEFAULT_OUTPUT_ALL_FORMAT
# define DEFAULT_OUTPUT_ALL_FORMAT          "%OTd %OTX (%ODX)|%-6OP|%-8OH|%-30OE|%11OSB|%-30OF|+|%DTd %DTX|%-20DF|%20DSB|%DR"
#endif
#if !defined (_INPUT_LOG) && defined (_DISTRIBUTION_LOG) && defined (_PRODUCTION_LOG) && !defined (_OUTPUT_LOG) && defined (_DELETE_LOG)
# undef  SEARCH_ALL_LOGS
# define SEARCH_ALL_LOGS                    (SEARCH_DISTRIBUTION_LOG | SEARCH_PRODUCTION_LOG | SEARCH_DELETE_LOG)
# undef  DEFAULT_OUTPUT_ALL_FORMAT
# define DEFAULT_OUTPUT_ALL_FORMAT          "%UTd %UTX|%-30UF|%11USB|+|%Ptd %PtX (%PDX)|%-30Pf|%-30PF|%11PSB|+|%DTd %DTX|%-30DF|%11DSB|%DR"
#endif
#if !defined (_INPUT_LOG) && !defined (_DISTRIBUTION_LOG) && defined (_PRODUCTION_LOG) && !defined (_OUTPUT_LOG) && defined (_DELETE_LOG)
# undef  SEARCH_ALL_LOGS
# define SEARCH_ALL_LOGS                    (SEARCH_PRODUCTION_LOG | SEARCH_DELETE_LOG)
# undef  DEFAULT_OUTPUT_ALL_FORMAT
# define DEFAULT_OUTPUT_ALL_FORMAT          "%Ptd %PtX (%PDX)|%-30Pf|%-30PF|%11PSB|+|%DTd %DTX|%-30DF|%11DSB|%DR"
#endif
#if !defined (_INPUT_LOG) && defined (_DISTRIBUTION_LOG) && defined (_PRODUCTION_LOG) && defined (_OUTPUT_LOG) && !defined (_DELETE_LOG)
# undef  SEARCH_ALL_LOGS
# define SEARCH_ALL_LOGS                    (SEARCH_DISTRIBUTION_LOG | SEARCH_PRODUCTION_LOG | SEARCH_OUTPUT_LOG)
# undef  DEFAULT_OUTPUT_ALL_FORMAT
# define DEFAULT_OUTPUT_ALL_FORMAT          "%UTd %UTX|%-30UF|%11USB|+|%Ptd %PtH:%PtM:%PtS (%PDX)|%-30Pf|%-30PF|%11PSB|+|%OTd %OTX (%ODX)|%-6OP|%-8OH|%-30OE"
#endif
#if !defined (_INPUT_LOG) && !defined (_DISTRIBUTION_LOG) && defined (_PRODUCTION_LOG) && defined (_OUTPUT_LOG) && !defined (_DELETE_LOG)
# undef  SEARCH_ALL_LOGS
# define SEARCH_ALL_LOGS                    (SEARCH_PRODUCTION_LOG | SEARCH_OUTPUT_LOG)
# undef  DEFAULT_OUTPUT_ALL_FORMAT
# define DEFAULT_OUTPUT_ALL_FORMAT          "%Ptd %PtH:%PtM:%PtS (%PDX)|%-30Pf|%-30PF|%11PSB|+|%OTd %OTX (%ODX)|%-6OP|%-8OH|%-30OE"
#endif
#if defined (_INPUT_LOG) && !defined (_DISTRIBUTION_LOG) && !defined (_PRODUCTION_LOG) && !defined (_OUTPUT_LOG) && !defined (_DELETE_LOG)
# undef  SEARCH_ALL_LOGS
# define SEARCH_ALL_LOGS                    SEARCH_INPUT_LOG
# undef  DEFAULT_OUTPUT_ALL_FORMAT
# define DEFAULT_OUTPUT_ALL_FORMAT          DEFAULT_OUTPUT_INPUT_FORMAT
#endif
#if !defined (_INPUT_LOG) && defined (_DISTRIBUTION_LOG) && !defined (_PRODUCTION_LOG) && !defined (_OUTPUT_LOG) && !defined (_DELETE_LOG)
# undef  SEARCH_ALL_LOGS
# define SEARCH_ALL_LOGS                    SEARCH_DISTRIBUTION_LOG
# undef  DEFAULT_OUTPUT_ALL_FORMAT
# define DEFAULT_OUTPUT_ALL_FORMAT          DEFAULT_OUTPUT_DISTRIBUTION_FORMAT
#endif
#if !defined (_INPUT_LOG) && !defined (_DISTRIBUTION_LOG) && defined (_PRODUCTION_LOG) && !defined (_OUTPUT_LOG) && !defined (_DELETE_LOG)
# undef  SEARCH_ALL_LOGS
# define SEARCH_ALL_LOGS                    SEARCH_PRODUCTION_LOG
# undef  DEFAULT_OUTPUT_ALL_FORMAT
# define DEFAULT_OUTPUT_ALL_FORMAT          DEFAULT_OUTPUT_PRODUCTION_FORMAT
#endif
#if !defined (_INPUT_LOG) && !defined (_DISTRIBUTION_LOG) && !defined (_PRODUCTION_LOG) && defined (_OUTPUT_LOG) && !defined (_DELETE_LOG)
# undef  SEARCH_ALL_LOGS
# define SEARCH_ALL_LOGS                    SEARCH_OUTPUT_LOG
# undef  DEFAULT_OUTPUT_ALL_FORMAT
# define DEFAULT_OUTPUT_ALL_FORMAT          DEFAULT_OUTPUT_OUTPUT__FORMAT
#endif
#if !defined (_INPUT_LOG) && !defined (_DISTRIBUTION_LOG) && !defined (_PRODUCTION_LOG) && !defined (_OUTPUT_LOG) && defined (_DELETE_LOG)
# undef  SEARCH_ALL_LOGS
# define SEARCH_ALL_LOGS                    SEARCH_DELETE_LOG
# undef  DEFAULT_OUTPUT_ALL_FORMAT
# define DEFAULT_OUTPUT_ALL_FORMAT          DEFAULT_OUTPUT_DELETE_FORMAT
#endif
#if !defined (_INPUT_LOG) && !defined (_DISTRIBUTION_LOG) && !defined (_PRODUCTION_LOG) && !defined (_OUTPUT_LOG) && !defined (_DELETE_LOG)
# undef  SEARCH_ALL_LOGS
# define SEARCH_ALL_LOGS                    0
# undef  DEFAULT_OUTPUT_ALL_FORMAT
# define DEFAULT_OUTPUT_ALL_FORMAT          ""
#endif
#define DEFAULT_OUTPUT_ALL_FORMAT_LENGTH    (sizeof(DEFAULT_OUTPUT_ALL_FORMAT) - 1)

/* Some return status values. */
#define NO_LOG_DATA                         2
#define NOT_WANTED                          3
#define GOT_DATA                            4
#define DATA_ALREADY_SHOWN                  5
#define SEARCH_TIME_UP                      6

/* Structure for holding data of DNA (Directory Name Area). */
struct dir_name_area
       {
          struct dir_name_buf *dnb;
          char                filename[MAX_PATH_LENGTH];
          off_t               size;
          time_t              mtime;
          int                 fd;
          int                 initial_no_of_dir_names;
          char                *area;
          char                *buffer;
          int                 *no_of_dir_names;
       };

/* Structure for the different log files. */
struct log_file_data
       {
          char   log_dir[MAX_PATH_LENGTH];
#ifdef WITH_LOG_CACHE
          char   log_cache_dir[MAX_PATH_LENGTH];
#endif
          int    current_file_no;  /* Current log file in use.           */
          int    end_file_no;      /* Last log file to be searched.      */
          int    start_file_no;    /* First log file to be searched.     */
          int    no_of_log_files;  /* Log files to search.               */
          int    max_log_files;    /* Maximum possible log files.        */
          int    fd;               /* File descriptor to log file.       */
#ifdef WITH_LOG_CACHE
          int    cache_fd;         /* File descriptor to log cache file. */
#endif
          ino_t  inode_number;     /* Inode number of current log file.  */
          size_t line_length;      /* The length of the line buffer.     */
          off_t  bytes_read;       /* Bytes read from current log file.  */
          FILE   *fp;              /* File pointer to log file.          */
          char   *p_log_number;    /* Position of log number in log_dir. */
#ifdef WITH_LOG_CACHE
          char   *p_log_cache_number; /* Ditto, this time for cache file.*/
#endif
          char   *line;            /* Holds one log line.                */
       };

/* Structure for holding all meta data for struct job_id_data. */
struct jid_data
       {
          int                      no_of_job_ids;
          int                      prev_pos;
          struct job_id_data       *jd;
#ifdef WITH_AFD_MON
          struct afd_job_list      *ajl;
          struct afd_typesize_data *atd;
#endif
          char                     name[MAX_PATH_LENGTH];
       };

/* Structure holding the start position of a line for each log file. */
/* #define CACHE_DEBUG */
#define LOG_LIST_STEP_SIZE 20
struct alda_position_list
       {
          off_t  pos;
          time_t time;
          char   gotcha;
#ifdef CACHE_DEBUG
          char   filename[MAX_FILENAME_LENGTH + 1];
#endif
       };

struct alda_cache_data
       {
          ino_t  inode;       /* Inode number.             */
          time_t first_entry; /* First log entry.          */
          time_t last_entry;  /* Last log entry.           */
          int    mpc;         /* Maximum position counter. */
          int    pc;          /* Position counter.         */
       };

/* Structure for evaluating log data (alda). */
#ifdef _INPUT_LOG
struct alda_idata
       {
          struct tm     bd_input_time; /* Broken-down input time. */
          char          filename[MAX_FILENAME_LENGTH + 1];
          char          full_source[MAX_PATH_LENGTH + 1];
          off_t         file_size;
          time_t        input_time;
          int           filename_length;
          int           full_source_length;
          unsigned int  dir_id;
          unsigned int  unique_number;
       };
struct alda_udata
       {
          struct tm     bd_distribution_time;/* Broken-down distribution time. */
          struct tm     bd_input_time;       /* Broken-down input time. */
          char          filename[MAX_FILENAME_LENGTH + 1];
          off_t         file_size;
          time_t        distribution_time;
          time_t        input_time;
          int           filename_length;
          int           no_of_dist_jobs;
          int           djid_buffer_length; /* Distributed JID buffer length. */
          unsigned int  no_of_distribution_types;
          unsigned int  dir_id;
          unsigned int  unique_number;
          unsigned int  *job_id_list;
          unsigned char *proc_cycles;
          unsigned char distribution_type;
       };
#else
struct alda_udata
       {
          struct tm     bd_distribution_time;/* Broken-down distribution time. */
          struct tm     bd_input_time;       /* Broken-down input time. */
          char          filename[MAX_FILENAME_LENGTH + 1];
          char          full_source[MAX_PATH_LENGTH + 1];
          off_t         file_size;
          time_t        distribution_time;
          time_t        input_time;
          int           filename_length;
          int           full_source_length;
          int           no_of_dist_jobs;
          int           djid_buffer_length; /* Distributed JID buffer length. */
          unsigned int  no_of_distribution_types;
          unsigned int  dir_id;
          unsigned int  unique_number;
          unsigned int  *job_id_list;
          unsigned char *proc_cycles;
          unsigned char distribution_type;
       };
#endif
#ifdef _DISTRIBUTION_LOG
# define DIS_JOB_LIST_STEP_SIZE 10
#endif
#ifdef _PRODUCTION_LOG
struct alda_pdata
       {
          struct tm     bd_input_time;  /* Broken-down input time. */
          struct tm     bd_output_time; /* Broken-down output time. */
          char          original_filename[MAX_FILENAME_LENGTH + 1];
          char          new_filename[MAX_FILENAME_LENGTH + 1];
          char          what_done[MAX_OPTION_LENGTH + 1];
          double        production_time;
          double        cpu_time;
          off_t         original_file_size;
          off_t         new_file_size;
          time_t        input_time;
          time_t        output_time;
          int           original_filename_length;
          int           new_filename_length;
          int           what_done_length;
          int           return_code;
          unsigned int  ratio_1;
          unsigned int  ratio_2;
          unsigned int  dir_id;
          unsigned int  job_id;
          unsigned int  unique_number;
          unsigned int  split_job_counter;
       };
#endif
#ifdef _OUTPUT_LOG
struct alda_odata
       {
          struct tm     bd_job_creation_time;/* Broken-down job creation time. */
          struct tm     bd_send_start_time;  /* Broken-down send start time. */
          struct tm     bd_output_time;      /* Broken-down output time. */
          char          local_filename[MAX_FILENAME_LENGTH + 1];
          char          remote_name[MAX_PATH_LENGTH + 1];
          char          archive_dir[MAX_PATH_LENGTH + 1];
          char          alias_name[MAX_REAL_HOSTNAME_LENGTH + 1]; /* Note: We us MAX_REAL_HOSTNAME_LENGTH since */
                                                                  /*       this can vary in future.             */
          char          real_hostname[MAX_REAL_HOSTNAME_LENGTH + 1];
          char          recipient[MAX_RECIPIENT_LENGTH];
          char          mail_id[MAX_MAIL_ID_LENGTH + 1];
          double        transmission_time;
          off_t         file_size;
          time_t        job_creation_time;
          time_t        send_start_time;
          time_t        output_time;
          int           local_filename_length;
          int           remote_name_length;
          int           archive_dir_length;
          int           alias_name_length;
          int           mail_id_length;
          int           output_type;
          int           current_toggle;
          unsigned int  job_id;
          unsigned int  dir_id;
          unsigned int  unique_number;
          unsigned int  split_job_counter;
          unsigned int  protocol;
          unsigned int  retries;
          unsigned int  cache_todo;
          unsigned int  cache_done;
       };
#endif
#ifdef _DELETE_LOG
struct alda_ddata
       {
          struct tm     bd_job_creation_time; /* Broken-down job creation time. */
          struct tm     bd_delete_time;       /* Broken-down delete time. */
          char          filename[MAX_FILENAME_LENGTH + 1];
          char          alias_name[MAX_REAL_HOSTNAME_LENGTH + 1]; /* Note: We us MAX_REAL_HOSTNAME_LENGTH since */
                                                                  /*       this can vary in future.             */
          char          user_process[MAX_USER_NAME_LENGTH + 1];
          char          add_reason[MAX_PATH_LENGTH + 1];
          off_t         file_size;
          time_t        job_creation_time;
          time_t        delete_time;
          int           filename_length;
          int           alias_name_length;
          int           user_process_length;
          int           add_reason_length;
          unsigned int  job_id;
          unsigned int  dir_id;
          unsigned int  deletion_type;
          unsigned int  unique_number;
          unsigned int  split_job_counter;
          unsigned int  cache_todo;
          unsigned int  cache_done;
       };
#endif
struct aldad_proc_list
       {
          pid_t pid;
          char  *parameters;
          char  in_list;
       };

/* Function prototypes. */
extern void check_dna(void),
            eval_input_alda(int *, char **),
            get_full_source(unsigned int, char *, int *),
            alloc_jid(char *),
#ifdef WITH_AFD_MON
            attach_adl(char *),
            attach_ahl(char *),
            attach_atd(char *),
            detach_adl(void),
            detach_ahl(void),
            detach_atd(void),
#endif
            dealloc_jid(void),
            print_alda_data(void),
            seek_cache_position(struct log_file_data *, time_t),
            show_file_content(FILE *, char *);
#ifdef _INPUT_LOG
extern int  check_input_line(char *, char *, off_t, time_t, unsigned int);
#endif
#ifdef _DISTRIBUTION_LOG
extern int  check_distribution_line(char *, char *, off_t, time_t, unsigned int,
                                    unsigned int *);
#endif
#ifdef _PRODUCTION_LOG
extern int  check_production_line(char *, char *, off_t, time_t, unsigned int,
                                  unsigned int, unsigned int *, unsigned int *);
#endif
#ifdef _OUTPUT_LOG
extern int  check_output_line(char *, char *, off_t, time_t, unsigned int,
                              unsigned int *, unsigned int *),
            get_recipient(unsigned int),
            get_recipient_alias(unsigned int);
#endif
#ifdef _DELETE_LOG
extern int  check_delete_line(char *, char *, off_t, time_t, unsigned int,
                              unsigned int *, unsigned int *);
#endif
extern int  check_did(unsigned int),
            check_host_alias(char *, char *, int),
            get_real_hostname(char *, int, char *);

#endif /* __aldadefs_h */
