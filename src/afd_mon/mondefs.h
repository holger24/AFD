/*
 *  mondefs.h - Part of AFD, an automatic file distribution program.
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

#ifndef __mondefs_h
#define __mondefs_h

#ifdef WITH_SSL
# include <openssl/ssl.h>
#endif

/* #define NEW_MSA 1 */

#define MON_WD_ENV_NAME             "MON_WORK_DIR" /* Environment variable */
                                                   /* for working          */
                                                   /* directory.           */

#ifndef MAX_AFDNAME_LENGTH
# define MAX_AFDNAME_LENGTH         12
#endif
#define MAX_REMOTE_CMD_LENGTH       10
#ifndef MAX_REAL_HOSTNAME_LENGTH
# define MAX_REAL_HOSTNAME_LENGTH   70
#endif
#define MAX_RET_MSG_LENGTH          4096  /* How much data is buffered from */
                                          /* the remote TCP port.           */
#define MAX_VERSION_LENGTH          40
#define MAX_CONVERT_USERNAME        5
#define MAX_INODE_LOG_NO_LENGTH     (MAX_LONG_LONG_LENGTH + 1 + MAX_INT_LENGTH + 1)
#define DATA_STEP_SIZE              10
#define AFD_MON_RESCAN_TIME         1

#define MON_CONFIG_FILE             "/MON_CONFIG"
#define AFD_MON_CONFIG_FILE         "/AFD_MON_CONFIG"

#define REMOTE_INODE_EXTENSION      "remote.inode"

#define AFDMON_BLOCK_FILE           "/NO_AFDMON_AUTO_RESTART"
#define MON_ACTIVE_FILE             "/AFD_MON_ACTIVE"
#define MON_STATUS_FILE             "/mon_status"
#define MON_STATUS_FILE_ALL         "/mon_status.*"
#define AFD_MON_STATUS_FILE         "/afd_mon.status"
#define MSA_ID_FILE                 "/msa.id"
#define ADL_FILE_NAME               "/afd_dir_list."
#define ADL_FILE_NAME_ALL           "/afd_dir_list.*"
#define OLD_ADL_FILE_NAME           "/afd_old_dir_list."
#define OLD_ADL_FILE_NAME_ALL       "/afd_old_dir_list.*"
#define TMP_ADL_FILE_NAME           "/afd_tmp_dir_list."
#define TMP_ADL_FILE_NAME_ALL       "/afd_tmp_dir_list.*"
#ifdef _INPUT_LOG
# define MAX_ADL_FILES_DEF          MAX_INPUT_LOG_FILES_DEF
# define MAX_ADL_FILES              MAX_INPUT_LOG_FILES
#else
# ifdef _DISTRIBUTION_LOG
#  define MAX_ADL_FILES_DEF         MAX_DISTRIBUTION_LOG_FILES_DEF
#  define MAX_ADL_FILES             MAX_DISTRIBUTION_LOG_FILES
# else
#  define MAX_ADL_FILES_DEF         "MAX_AFD_DIR_LIST_FILES"
#  define MAX_ADL_FILES             7
# endif
#endif
#define AHL_FILE_NAME               "/afd_host_list."
#define AHL_FILE_NAME_ALL           "/afd_host_list.*"
#define AJL_FILE_NAME               "/afd_job_list."
#define AJL_FILE_NAME_ALL           "/afd_job_list.*"
#define OLD_AJL_FILE_NAME           "/afd_old_job_list."
#define OLD_AJL_FILE_NAME_ALL       "/afd_old_job_list.*"
#define TMP_AJL_FILE_NAME           "/afd_tmp_job_list."
#define TMP_AJL_FILE_NAME_ALL       "/afd_tmp_job_list.*"
#define ATD_FILE_NAME               "/afd_typesize_data."
#ifdef _OUTPUT_LOG
# define MAX_AJL_FILES_DEF          MAX_OUTPUT_LOG_FILES_DEF
# define MAX_AJL_FILES              MAX_OUTPUT_LOG_FILES
#else
# define MAX_AJL_FILES_DEF          "MAX_AFD_JOB_LIST_FILES"
# define MAX_AJL_FILES              7
#endif
#define MON_CMD_FIFO                "/afd_mon_cmd.fifo"
#define MON_RESP_FIFO               "/afd_mon_resp.fifo"
#define MON_PROBE_ONLY_FIFO         "/afd_mon_probe_only.fifo"
#define RETRY_MON_FIFO              "/retry_mon.fifo."
#define RETRY_MON_FIFO_ALL          "/retry_mon.fifo.*"
#define MON_SYS_LOG_FIFO            "/mon_sys_log.fifo"

#define STORAGE_TIME                7 /* Time in days to store top       */
                                      /* values for transfer rate and    */
                                      /* file rate.                      */
#define SUM_STORAGE                 6 /* The number of summary values    */
                                      /* stored. See sumdefs.h for the   */
                                      /* meaning each value defined.     */
#define CURRENT_SUM                 0
#define HOUR_SUM                    1
#define DAY_SUM                     2
#define WEEK_SUM                    3
#define MONTH_SUM                   4
#define YEAR_SUM                    5
#define DEFAULT_POLL_INTERVAL       5
#define DEFAULT_OPTION_ENTRY        0
#define DEFAULT_REMOTE_CMD          "ssh"
#define DEFAULT_CONNECT_TIME        0
#define DEFAULT_DISCONNECT_TIME     0
#define RETRY_INTERVAL              60 /* Interval at which the mon       */
                                       /* process will try to reconnect   */
                                       /* after an error occured.         */

/* Values for special flag in MSA. */
#define SUM_VAL_INITIALIZED         1

#define LOG_RESHUFFEL               1
#define LOG_STALE                   2

/* Error return values for mon process. */
#define MON_SYNTAX_ERROR            1
#define MON_SELECT_ERROR            2

/* Return values of log sub process of afd_mon. */
#define MISSED_PACKET               5
#define REMOTE_HANGUP               6
#define FAILED_LOG_CMD              7
#define LOG_CONNECT_ERROR           8
#define LOG_DATA_TIMEOUT            9

/* Different return codes for function evaluate_message(). */
#define UNKNOWN_MESSAGE             1
#define AFDD_SHUTTING_DOWN          24
/* NOTE: Do not go above 99! Values from 100 on are used to return */
/*       the true return code of a TCP command.                    */

/* Flags for the options field in struct mon_status_area. */
#define COMPRESS_FLAG               1        /*  1 */
#define MINUS_Y_FLAG                2        /*  2 */
#define DONT_USE_FULL_PATH_FLAG     4        /*  3 */
#define ENABLE_TLS_ENCRYPTION       8        /*  4 */ /* Still to be implemented. */
/* NOTE: afddefs.h defines bits 5 - 17. */
/*       Bits 18 - 23 undefined         */
#define DISABLE_SSH_STRICT_HOST_KEY 8388608  /* 24 */
/*       Bits 25 - 31 undefined         */

/* Different toggling status for switching AFD's. */
#define NO_SWITCHING                0
#define AUTO_SWITCHING              1
#define USER_SWITCHING              2


#define PRINT_SIZE_STR(value, str)\
   {\
      (str)[3] = ' ';\
      (str)[6] = '\0';\
      if ((value) < KILOBYTE)\
      {\
         if ((value) < 1000)\
         {\
            (str)[4] = 'B';\
            (str)[5] = ' ';\
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
            (str)[3] = ' ';\
            (str)[4] = 'K';\
            (str)[5] = 'B';\
         }\
      }\
      else if ((value) < MEGABYTE)\
           {\
              if ((value) < 1000000)\
              {\
                 int num = (value) / KILOBYTE;\
\
                 (str)[4] = 'K';\
                 (str)[5] = 'B';\
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
                 (str)[3] = ' ';\
                 (str)[4] = 'M';\
                 (str)[5] = 'B';\
              }\
           }\
      else if ((value) < GIGABYTE)\
           {\
              if ((value) < 1000000000)\
              {\
                 int num = (value) / MEGABYTE;\
\
                 (str)[4] = 'M';\
                 (str)[5] = 'B';\
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
                 (str)[3] = ' ';\
                 (str)[4] = 'G';\
                 (str)[5] = 'B';\
              }\
           }\
      else if ((value) < TERABYTE)\
           {\
              if ((value) < 1000000000000LL)\
              {\
                 int num = (value) / GIGABYTE;\
\
                 (str)[4] = 'G';\
                 (str)[5] = 'B';\
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
                 (str)[3] = ' ';\
                 (str)[4] = 'T';\
                 (str)[5] = 'B';\
              }\
           }\
      else if ((value) < PETABYTE)\
           {\
              if ((value) < 1000000000000000LL)\
              {\
                 int num = (value) / TERABYTE;\
\
                 (str)[4] = 'T';\
                 (str)[5] = 'B';\
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
                 (str)[3] = ' ';\
                 (str)[4] = 'P';\
                 (str)[5] = 'B';\
              }\
           }\
      else if ((value) < EXABYTE)\
           {\
              if ((value) < 1000000000000000000LL)\
              {\
                 int num = (value) / PETABYTE;\
\
                 (str)[4] = 'P';\
                 (str)[5] = 'B';\
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
                 (str)[3] = ' ';\
                 (str)[4] = 'E';\
                 (str)[5] = 'B';\
              }\
           }\
           else\
           {\
              int num = (value) / EXABYTE;\
\
              (str)[4] = 'E';\
              (str)[5] = 'B';\
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

#ifdef WITH_SYSTEMD
#define UPDATE_HEARTBEAT()\
        {\
           if (systemd_watchdog_enabled > 0)\
           {\
              sd_notify(0, "WATCHDOG=1");\
           }\
        }
#endif


/* Structure to hold all host alias names and their real names. */
struct afd_host_list
       {
          unsigned int  host_id;
          char          host_alias[MAX_HOSTNAME_LENGTH + 1];
          char          real_hostname[2][MAX_REAL_HOSTNAME_LENGTH];
          unsigned char error_history[ERROR_HISTORY_LENGTH];
       };

/* Structure to hold all dir alias names, dir ID's and names from DIR_CONFIG. */
struct afd_dir_list
       {
          unsigned int  dir_id;
          unsigned int  home_dir_length;
          time_t        entry_time; /* Time when this first enlisted. */
          char          dir_alias[MAX_DIR_ALIAS_LENGTH + 1];
          char          dir_name[MAX_PATH_LENGTH];
          char          orig_dir_name[MAX_PATH_LENGTH];
          char          home_dir_user[MAX_USER_NAME_LENGTH];
       };

/* Structure to hold all job ID's. */
struct afd_job_list
       {
          unsigned int  job_id;
          unsigned int  dir_id;
          int           no_of_loptions;
          time_t        entry_time; /* Time when this first enlisted. */
          char          recipient[MAX_RECIPIENT_LENGTH];
          char          priority;
       };

/* Structure to hold all typesize data elements. */
#define MAX_MSG_NAME_LENGTH_ELEMENT      0
#define MAX_FILENAME_LENGTH_ELEMENT      1
#define MAX_HOSTNAME_LENGTH_ELEMENT      2
#define MAX_REAL_HOSTNAME_LENGTH_ELEMENT 3
#define MAX_PROXY_NAME_LENGTH_ELEMENT    4
#define MAX_TOGGLE_STR_LENGTH_ELEMENT    5
#define ERROR_HISTORY_LENGTH_ELEMENT     6
#define MAX_NO_PARALLEL_JOBS_ELEMENT     7
#define MAX_DIR_ALIAS_LENGTH_ELEMENT     8
#define MAX_RECIPIENT_LENGTH_ELEMENT     9
#define MAX_WAIT_FOR_LENGTH_ELEMENT      10
#define MAX_FRA_TIME_ENTRIES_ELEMENT     11
#define MAX_OPTION_LENGTH_ELEMENT        12
#define MAX_PATH_LENGTH_ELEMENT          13
#define MAX_USER_NAME_LENGTH_ELEMENT     14
#define MAX_TIMEZONE_LENGTH_ELEMENT      15
#define AFD_TYPESIZE_ELEMENTS            16
struct afd_typesize_data
       {
          int val[AFD_TYPESIZE_ELEMENTS];
       };

/* Structure to hold data from AFD_MON_CONFIG file. */
struct mon_list
       {
          char          convert_username[MAX_CONVERT_USERNAME][2][MAX_USER_NAME_LENGTH];
                                                 /* Command to call      */
                                                 /* remote programms.    */
          char          afd_alias[MAX_AFDNAME_LENGTH + 1];
                                                 /* The name of the      */
                                                 /* remote AFD shown in  */
                                                 /* the dialog.          */
          char          hostname[2][MAX_REAL_HOSTNAME_LENGTH];
                                                 /* The remote host name.*/
          char          rcmd[MAX_REMOTE_CMD_LENGTH];
                                                 /* The command to       */
                                                 /* execute on remote    */
                                                 /* host, either rsh or  */
                                                 /* ssh.                 */
          int           port[2];                 /* Port number of       */
                                                 /* remote AFDD.         */
          int           poll_interval;           /* The interval in      */
                                                 /* seconds, getting     */
                                                 /* data from AFDD.      */
          unsigned int  connect_time;            /* How long should we   */
                                                 /* stay connected when  */
                                                 /* disconnect.          */
          unsigned int  disconnect_time;         /* How long should we   */
                                                 /* stay disconnected.   */
          unsigned int  options;                 /* Special options with */
                                                 /* following meaning:   */
                                                 /*+------+-------------+*/
                                                 /*|Bit(s)|   Meaning   |*/
                                                 /*+------+-------------+*/
                                                 /*| 17-32| Not used.   |*/
                                                 /*| 16   | Distribution|*/
                                                 /*|      | Log.        |*/
                                                 /*| 15   | Event Log   |*/
                                                 /*| 14   | Compression1|*/
                                                 /*| 13   | Job data    |*/
                                                 /*| 12   | Delete Log  |*/
                                                 /*| 11   | Output Log  |*/
                                                 /*| 10   | Product. Log|*/
                                                 /*| 9    | Input Log   |*/
                                                 /*| 8    | Trans_db Log|*/
                                                 /*| 7    | Transfer Log|*/
                                                 /*| 6    | Receive Log |*/
                                                 /*| 5    | System Log  |*/
                                                 /*| 4    | SSL Encrypt.|*/
                                                 /*| 3    | Don't use   |*/
                                                 /*|      | full path.  |*/
                                                 /*| 2    | Use -Y      |*/
                                                 /*|      | instead of  |*/
                                                 /*|      | -X (ssh).   |*/
                                                 /*| 1    | compression |*/
                                                 /*|      | for ssh     |*/
                                                 /*+------+-------------+*/
          unsigned char afd_switching;           /* Flag to specify if   */
                                                 /* automatic AFD switch-*/
                                                 /* ing (AUTO_SWITCHING),*/
                                                 /* user switching       */
                                                 /* (USER_SWITCHING) or  */
                                                 /* no switching         */
                                                 /* (NO_SWITCHING) is    */
                                                 /* being used.          */
       };

/* Structure to hold status of afd_mon and the process it starts. */
struct afd_mon_status
       {
          time_t       start_time;
          signed char  afd_mon;
          signed char  mon_sys_log;
          signed char  mon_log;
          signed char  aldad;
          unsigned int mon_sys_log_ec;  /* Mon system log entry counter. */
          char         mon_sys_log_fifo[LOG_FIFO_SIZE + 1];
          unsigned int mon_log_ec;      /* Mon log entry counter.        */
          char         mon_log_fifo[LOG_FIFO_SIZE + 1];
       };

/* Structure holding all relevant data of one remote AFD. */
/*-----------------------------------------------------------------------*
 * For the MSA these bytes are used to store information about the
 * AFD_MON with the following meaning (assuming SIZEOF_INT is 4):
 *     Byte  | Type          | Description
 *    -------+---------------+---------------------------------------
 *     1 - 4 | int           | The number of hosts served by the AFD.
 *           |               | If this FSA in no longer in use there
 *           |               | will be a -1 here.
 *    -------+---------------+---------------------------------------
 *     5 - 7 | unsigned char | Not used.
 *    -------+---------------+---------------------------------------
 *       8   | unsigned char | Version of this structure.
 *    -------+---------------+---------------------------------------
 *     9 - 16|               | Not used.
 *-----------------------------------------------------------------------*/
#ifdef NEW_MSA
# define CURRENT_MSA_VERSION           3
#else
# define CURRENT_MSA_VERSION           2
#endif
struct mon_status_area
       {
          char          r_work_dir[MAX_PATH_LENGTH];
          char          convert_username[MAX_CONVERT_USERNAME][2][MAX_USER_NAME_LENGTH];
                                                 /* Command to call      */
                                                 /* remote programms.    */
          char          afd_alias[MAX_AFDNAME_LENGTH + 1];
                                                 /* The name of the      */
                                                 /* remote AFD shown in  */
                                                 /* the dialog.          */
          char          hostname[2][MAX_REAL_HOSTNAME_LENGTH];
                                                 /* The remote host name.*/
          char          rcmd[MAX_REMOTE_CMD_LENGTH];
                                                 /* The command to       */
                                                 /* execute on remote    */
                                                 /* host, either rsh or  */
                                                 /* ssh.                 */
          char          afd_version[MAX_VERSION_LENGTH];
                                                 /* Version of remote    */
                                                 /* AFD.                 */
          int           port[2];                 /* Port number of       */
                                                 /* remote AFDD.         */
          int           poll_interval;           /* The interval in      */
                                                 /* seconds, getting     */
                                                 /* data from AFDD.      */
          unsigned int  connect_time;            /* How long should we   */
                                                 /* stay connected when  */
                                                 /* disconnect.          */
          unsigned int  disconnect_time;         /* How long should we   */
                                                 /* stay disconnected.   */
#ifdef NEW_MSA
          unsigned int  afd_id;                  /* CRC-32 checksum of   */
                                                 /* afd_alias above.     */
#endif
          char          amg;                     /* Status of AMG.       */
          char          fd;                      /* Status of FD.        */
          char          archive_watch;           /* Status of AW.        */
          int           jobs_in_queue;           /* The number of jobs   */
                                                 /* still to be done     */
                                                 /* by the FD.           */
          long          danger_no_of_jobs;       /* The number of jobs   */
                                                 /* when this field      */
                                                 /* should become orange.*/
          int           no_of_transfers;         /* The number of        */
                                                 /* active transfers.    */
          int           top_no_of_transfers[STORAGE_TIME];
                                                 /* Maximum number of    */
                                                 /* sf_xxx/gf_xxx        */
                                                 /* process on a per     */
                                                 /* day basis.           */
          time_t        top_not_time;            /* Time when the        */
                                                 /* this maximum was     */
                                                 /* reached.             */
          int           max_connections;         /* Maximum number of    */
                                                 /* sf_xxx/gf_xxx        */
                                                 /* process.             */
          unsigned int  sys_log_ec;              /* System log entry     */
                                                 /* counter.             */
          char          sys_log_fifo[LOG_FIFO_SIZE + 1];
                                                 /* Activity in the      */
                                                 /* system log.          */
          char          log_history[NO_OF_LOG_HISTORY][MAX_LOG_HISTORY];
                                                 /* All log activities   */
                                                 /* the last 48 hours.   */
          int           host_error_counter;      /* Number of host       */
                                                 /* names that are red.  */
          int           no_of_hosts;             /* The number of hosts  */
                                                 /* configured.          */
          int           no_of_dirs;              /* The number of        */
                                                 /* directories          */
                                                 /* configured.          */
          unsigned int  no_of_jobs;              /* The number of jobs   */
                                                 /* configured.          */
          unsigned int  options;                 /* Special options with */
                                                 /* following meaning:   */
                                                 /*+------+-------------+*/
                                                 /*|Bit(s)|   Meaning   |*/
                                                 /*+------+-------------+*/
                                                 /*| 25-31| Not used.   |*/
                                                 /*| 24   | No strict   |*/
                                                 /*|      | SSH host key|*/
                                                 /*|      | check.      |*/
                                                 /*| 18-23| Not used.   |*/
                                                 /*| 17   | Confirmation|*/
                                                 /*|      | Log.        |*/
                                                 /*| 16   | Distribution|*/
                                                 /*|      | Log.        |*/
                                                 /*| 15   | Event Log   |*/
                                                 /*| 14   | Compression1|*/
                                                 /*| 13   | Job data    |*/
                                                 /*| 12   | Delete Log  |*/
                                                 /*| 11   | Output Log  |*/
                                                 /*| 10   | Product. Log|*/
                                                 /*| 9    | Input Log   |*/
                                                 /*| 8    | Trans_db Log|*/
                                                 /*| 7    | Transfer Log|*/
                                                 /*| 6    | Receive Log |*/
                                                 /*| 5    | System Log  |*/
                                                 /*| 4    | SSL Encrypt.|*/
                                                 /*| 3    | Don't use   |*/
                                                 /*|      | full path.  |*/
                                                 /*| 2    | Use -Y      |*/
                                                 /*|      | instead of  |*/
                                                 /*|      | -X (ssh).   |*/
                                                 /*| 1    | compression |*/
                                                 /*|      | for ssh.    |*/
                                                 /*+------+-------------+*/
                                                 /* that this AFD has.   */
          unsigned int  log_capabilities;        /* Flags have the same  */
                                                 /* meaning as above, but*/
                                                 /* only for bits 5-14.  */
          unsigned int  fc;                      /* Files still to be    */
                                                 /* send.                */
          u_off_t       fs;                      /* File size still to   */
                                                 /* be send.             */
          u_off_t       tr;                      /* Transfer rate.       */
          u_off_t       top_tr[STORAGE_TIME];    /* Maximum transfer     */
                                                 /* rate on a per day    */
                                                 /* basis.               */
          time_t        top_tr_time;             /* Time when this       */
                                                 /* maximum was reached. */
          unsigned int  fr;                      /* File rate.           */
          unsigned int  top_fr[STORAGE_TIME];    /* Maximum file rate    */
                                                 /* on a per day basis.  */
          time_t        top_fr_time;             /* Time when this       */
                                                 /* maximum was reached. */
          unsigned int  ec;                      /* Error counter.       */
          time_t        last_data_time;          /* Last time data was   */
                                                 /* received from        */
                                                 /* remote AFD.          */
#ifdef NEW_MSA
          double        bytes_send[SUM_STORAGE]; /* The number of bytes  */
                                                 /* send.                */
          double        bytes_received[SUM_STORAGE]; /* The number of    */
                                                 /* bytes received.      */
          double        log_bytes_received[SUM_STORAGE]; /* The number of*/
                                                 /* log bytes received.  */
#else
          u_off_t       bytes_send[SUM_STORAGE]; /* The number of bytes  */
                                                 /* send.                */
          u_off_t       bytes_received[SUM_STORAGE]; /* The number of    */
                                                 /* bytes received.      */
          u_off_t       log_bytes_received[SUM_STORAGE]; /* The number of*/
                                                 /* log bytes received.  */
#endif
          unsigned int  files_send[SUM_STORAGE]; /* The number of files  */
                                                 /* send.                */
          unsigned int  files_received[SUM_STORAGE]; /* The number of    */
                                                 /* files received.      */
          unsigned int  connections[SUM_STORAGE];/* Number of connections*/
                                                 /* done so far.         */
          unsigned int  total_errors[SUM_STORAGE]; /* Number of errors   */
                                                 /* that have occured so */
                                                 /* far.                 */
          char          connect_status;          /* Connect status to    */
                                                 /* remote AFD.          */
          unsigned char special_flag;            /* Special flag with the*/
                                                 /* following meaning:   */
                                                 /* +------+------------+*/
                                                 /* |Bit(s)|  Meaning   |*/
                                                 /* +------+------------+*/
                                                 /* | 2 - 8| Not used   |*/
                                                 /* | 1    | SUM_VAL_INITIALIZED|*/
                                                 /* +------+------------+*/
          unsigned char afd_switching;           /* Flag to specify if   */
                                                 /* automatic AFD switch-*/
                                                 /* ing (AUTO_SWITCHING),*/
                                                 /* user switching       */
                                                 /* (USER_SWITCHING) or  */
                                                 /* no switching         */
                                                 /* (NO_SWITCHING) is    */
                                                 /* being used.          */
          char          afd_toggle;              /* If there is more then*/
                                                 /* one AFD, this tells  */
                                                 /* which ist the active */
                                                 /* one (HOST_ONE or     */
                                                 /* HOST_TWO).           */
       };

struct process_list
       {
          char         afd_alias[MAX_AFDNAME_LENGTH + 1];
                                                 /* The name of the     */
                                                 /* remote AFD shown in */
                                                 /* the dialog.         */
          pid_t        mon_pid;                  /* Process ID of       */
                                                 /* process monitoring  */
                                                 /* this AFD.           */
          pid_t        log_pid;                  /* Process ID of log   */
                                                 /* proccess receiving  */
                                                 /* remote log data.    */
          time_t       start_time;               /* Time when process   */
                                                 /* was started.        */
          time_t       next_retry_time_log;      /* At what time we     */
                                                 /* try another restart */
                                                 /* of the log process. */
          int          number_of_restarts;       /* Number of times     */
                                                 /* this process has    */
                                                 /* restarted.          */
       };

/* Function prototypes. */
extern int     attach_afd_mon_status(void),
               check_afdmon_database(void),
               check_mon(long),
               detach_afd_mon_status(void),
               evaluate_message(int *),
               handle_log_data(int, int),
               init_fifos_mon(void),
               read_msg(void),
               send_afdmon_start(void),
               send_log_cmd(int, char *, int *),
#ifdef WITH_SSL
               tcp_connect(char *, int, int, int),
#else
               tcp_connect(char *, int, int),
#endif
               tcp_quit(void);
extern pid_t   start_process(char *, int);
extern char    *convert_msa(int, char *, off_t *, int, unsigned char,
               unsigned char);
extern void    create_msa(void),
               eval_afd_mon_db(struct mon_list **),
               mon_log(char *, char *, int, time_t, char *, char *, ...),
#ifdef WITH_SYSTEMD
               shutdown_mon(int, char *, int),
#else
               shutdown_mon(int, char *),
#endif
               start_all(void),
               start_log_process(int, unsigned int),
               stop_log_process(int),
               stop_process(int, int),
               update_group_summary(int),
               write_afd_log(int, int, unsigned int, unsigned int, char *);
#ifdef WITH_SSL
extern ssize_t ssl_write(SSL *, const char *, size_t);
#endif

#endif /* __mondefs_h */
