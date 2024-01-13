/*
 *  afddefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2024 Deutscher Wetterdienst (DWD),
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

#ifndef __afddefs_h
#define __afddefs_h

/* Indicators for start and end of module description for man pages. */
#define DESCR__S_M1             /* Start for User Command Man Page. */
#define DESCR__E_M1             /* End for User Command Man Page.   */
#define DESCR__S_M3             /* Start for Subroutines Man Page.  */
#define DESCR__E_M3             /* End for Subroutines Man Page.    */

/* #define NEW_PWB */    /* Nothing new yet! */

#define WITH_TIMEZONE 1
#define _WITH_BURST_MISS_CHECK 1

/*
 * SF_BURST_ACK is not working because when in fd.c queue_burst_ack()
 * msg_name is not updated during a burst. So with multiple burst the
 * same msg_name gets stored to the ack_queue. Have not found a solution
 * for this.
 */
/* #define SF_BURST_ACK 1 */

/*
 * This enables extra checks when retrieving data. The only protocol
 * where this is currently implemented is HTTP. Here it uses the ETag
 * as an additional check in the GET statement via If-None-Match.
 * However I could not find a useful use of this because most HTTP
 * servers use in the etag the modification time and size. And these
 * two values are already checked. So we still must always issue a
 * HEAD :-(
 */
/* #define _WITH_EXTRA_CHECK 1 */

#ifdef HAVE_CONFIG_H
# include "config.h"
# include "ports.h"
# include "configmake.h"
#endif
#include "afdsetup.h"
#if MAX_DIR_ALIAS_LENGTH < 10
# define MAX_DIR_ALIAS_LENGTH 10
#endif
#include <stdio.h>
#ifdef STDC_HEADERS
# include <string.h>
#else
# ifndef HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
#  define memmove(d, s, n) bcopy ((s), (d), (n))
# endif
#endif
#ifdef HAVE_STDINT_H
# include <stdint.h>
#else
# typedef signed int        int32_t;
# typedef unsigned int      uint32_t;
# typedef unsigned long int uint64_t;
#endif
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_WAIT4
# include <sys/time.h>                /* struct timeval                  */
#endif
#ifdef _PRODUCTION_LOG
# include <sys/resource.h>
#endif
#include <float.h>                    /* DBL_MANT_DIG, DBL_MIN_EXP       */
#ifdef WITH_SSL
# ifdef HAVE_TLS_CLIENT_METHOD
#  define afd_encrypt_client_method TLS_client_method
# else
#  define afd_encrypt_client_method SSLv23_client_method
# endif
# ifdef HAVE_TLS_SERVER_METHOD
#  define afd_encrypt_server_method TLS_server_method
# else
#  define afd_encrypt_server_method SSLv23_server_method
# endif
#endif

#if defined(__GNUC__)
# define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
# if GCC_VERSION >= 40500
  /* gcc diagnostic pragmas available */
# define GCC_DIAGNOSTIC_PRAGMA
# endif
#endif

#define MY_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MY_MAX(a, b) (((a) > (b)) ? (a) : (b))

#if HAVE_STDARG_H
#include <stdarg.h>
#if !HAVE_VSNPRINTF
int rpl_vsnprintf(char *, size_t, const char *, va_list);
#endif
#if !HAVE_SNPRINTF
int rpl_snprintf(char *, size_t, const char *, ...);
#endif
#if !HAVE_VASPRINTF
int rpl_vasprintf(char **, const char *, va_list);
#endif
#if !HAVE_ASPRINTF
int rpl_asprintf(char **, const char *, ...);
#endif
#endif	/* HAVE_STDARG_H */


#ifdef HAVE_GETTEXT
# include <locale.h>
# include <libintl.h>
#include <libintl.h>
# define _(String) gettext(String)
# define gettext_noop(String)String
# define N_(String) gettext_noop(String)
#else
# define _(String) (String)
# define N_(String) String
# define textdomain(Domain)
# define bindtextdomain(Package, Directory)
#endif

#ifndef HAVE_STRERROR
extern char *sys_errlist[];
extern int  sys_nerr;

# define strerror(e) sys_errlist[((unsigned)(e) < sys_nerr) ? (e) : 0]
#endif

#ifndef HOST_NAME_MAX
# define HOST_NAME_MAX 255
#endif

#if SIZEOF_OFF_T == 4
typedef unsigned long       u_off_t;
typedef long                pri_off_t;
#else
typedef unsigned long long  u_off_t;
typedef long long           pri_off_t;
#endif
#if SIZEOF_TIME_T == 4
typedef long                pri_time_t;
#else
typedef long long           pri_time_t;
#endif
#if SIZEOF_INO_T == 4
typedef long                pri_ino_t;
#else
typedef long long           pri_ino_t;
#endif
#if SIZEOF_DEV_T == 4
typedef long                pri_dev_t;
#else
typedef long long           pri_dev_t;
#endif
#if SIZEOF_PID_T == 4
typedef int                 pri_pid_t;
#else
typedef long long           pri_pid_t;
#endif
#if SIZEOF_NLINK_T > 4
typedef long long           pri_nlink_t;
#else
typedef int                 pri_nlink_t;
#endif
#if SIZEOF_SIZE_T == 4
typedef int                 pri_size_t;
#else
typedef long long           pri_size_t;
#endif
#if SIZEOF_SSIZE_T == 4
typedef int                 pri_ssize_t;
#else
typedef long long           pri_ssize_t;
#endif
#if SIZEOF_UID_T == 4
typedef int                 pri_uid_t;
#else
typedef long long           pri_uid_t;
#endif
#ifdef HAVE_LONG_LONG
typedef unsigned long long  u_long_64;
#else
typedef unsigned long       u_long_64;
#endif

#ifdef _LINK_MAX_TEST
# define LINKY_MAX                 4
#endif

#ifdef _CYGWIN
# ifndef LINK_MAX
#  define LINK_MAX                 1000
# endif
#endif
#ifdef _SCO
# ifndef LINK_MAX
#  define LINK_MAX                 1000 /* SCO does not define it. */
# endif
#endif /* _SCO */
#ifdef LINUX
# define REDUCED_LINK_MAX          8192 /* Reduced for testing.    */
#endif /* LINUX */

#ifndef HAVE_MMAP
/* Dummy definitions for systems that do not have mmap(). */
# define PROT_READ                 1
# define PROT_WRITE                1
# define MAP_SHARED                1
#endif

/* In case configure failed and there is no config.h. */
/*
 * The default number of parallel jobs per host.
 */
#ifndef DEFAULT_NO_PARALLEL_JOBS
# define DEFAULT_NO_PARALLEL_JOBS 3
#endif
/*
 * The maximum length of the host name (alias) that is displayed by
 * 'afd_ctrl'.
 * DEFAULT: 8
 */
#ifndef MAX_HOSTNAME_LENGTH
# define MAX_HOSTNAME_LENGTH 8
#endif
/*
 * The maximum directory alias name length that is displayed by
 * 'dir_ctrl'. Note: this value may NOT be less then 10!
 * DEFAULT: 10
 */
#ifndef MAX_DIR_ALIAS_LENGTH
# define MAX_DIR_ALIAS_LENGTH 10
#endif
/*
 * The maximum real host name or IP number length.
 * DEFAULT: 40
 * N O T E : When this is modified edit configure.ac and
 *           README.configure to correct the values there too.
 */
#ifndef MAX_REAL_HOSTNAME_LENGTH
# define MAX_REAL_HOSTNAME_LENGTH 70
#endif

/* Define the names of the program's. */
#ifndef HAVE_MMAP
# define MAPPER                    "mapper"
#endif
#define AFD                        "init_afd"
#define AFD_LENGTH                 (sizeof(AFD) - 1)
#define AFD_WORKER                 "init_afd_worker"
#define AFD_WORKER_LENGTH          (sizeof(AFD_WORKER) - 1)
#define AMG                        "amg"
#define AMG_LENGTH                 (sizeof(AMG) - 1)
#define FD                         "fd"
#define AFD_ENVIRONMENT_WRAPPER    "afd_environment_wrapper"
#define AFD_ENVIRONMENT_FILE       "environment.conf"
#define SEND_FILE_FTP              "sf_ftp"
#define SEND_FILE_FTP_LENGTH       (sizeof(SEND_FILE_FTP) - 1)
#define SEND_FILE_FTP_TRACE        "sf_ftp_trace"
#define GET_FILE_FTP               "gf_ftp"
#define GET_FILE_FTP_TRACE         "gf_ftp_trace"
#define SEND_FILE_SMTP             "sf_smtp"
#define SEND_FILE_SMTP_LENGTH      (sizeof(SEND_FILE_SMTP) - 1)
#define SEND_FILE_SMTP_TRACE       "sf_smtp_trace"
#define GET_FILE_SMTP              "gf_smtp"
#define SEND_FILE_HTTP             "sf_http"
#define SEND_FILE_HTTP_LENGTH      (sizeof(SEND_FILE_HTTP) - 1)
#define SEND_FILE_HTTP_TRACE       "sf_http_trace"
#define GET_FILE_HTTP              "gf_http"
#define GET_FILE_HTTP_TRACE        "gf_http_trace"
#define SEND_FILE_LOC              "sf_loc"
#define SEND_FILE_LOC_LENGTH       (sizeof(SEND_FILE_LOC) - 1)
#define SEND_FILE_EXEC             "sf_exec"
#define SEND_FILE_EXEC_LENGTH      (sizeof(SEND_FILE_EXEC) - 1)
#define GET_FILE_EXEC              "gf_exec"
#define GET_FILE_EXEC_LENGTH       (sizeof(GET_FILE_EXEC) - 1)
#ifdef _WITH_SCP_SUPPORT
# define SEND_FILE_SCP             "sf_scp"
# define SEND_FILE_SCP_TRACE       "sf_scp_trace"
# define GET_FILE_SCP              "gf_scp"
#endif /* _WITH_SCP_SUPPORT */
#ifdef _WITH_WMO_SUPPORT
# define SEND_FILE_WMO             "sf_wmo"
# define SEND_FILE_WMO_TRACE       "sf_wmo_trace"
#endif
#ifdef _WITH_MAP_SUPPORT
# define SEND_FILE_MAP             "sf_map"
#endif
#ifdef _WITH_DFAX_SUPPORT
# define SEND_FILE_DFAX            "sf_dfax"
# define SEND_FILE_DFAX_TRACE      "sf_dfax_trace"
#endif
#define SEND_FILE_SFTP             "sf_sftp"
#define SEND_FILE_SFTP_LENGTH      (sizeof(SEND_FILE_SFTP) - 1)
#define SEND_FILE_SFTP_TRACE       "sf_sftp_trace"
#define GET_FILE_SFTP              "gf_sftp"
#define GET_FILE_SFTP_TRACE        "gf_sftp_trace"
#define SLOG                       "system_log"
#define MLOG                       "maintainer_log"
#define ELOG                       "event_log"
#define RLOG                       "receive_log"
#define TLOG                       "transfer_log"
#define TDBLOG                     "trans_db_log"
#define TRLOG                      "transfer_rate_log"
#define MON_SYS_LOG                "mon_sys_log"
#define MONITOR_LOG                "monitor_log"
#define SHOW_ELOG                  "show_elog"
#define SHOW_ILOG                  "show_ilog"
#define SHOW_PLOG                  "show_plog"
#define SHOW_OLOG                  "show_olog"
#define SHOW_DLOG                  "show_dlog"
#define SHOW_QUEUE                 "show_queue"
#define SHOW_TRANS                 "show_trans"
#define XSEND_FILE                 "xsend_file"
#define XSEND_FILE_LENGTH          (sizeof(XSEND_FILE) - 1)
#ifdef _INPUT_LOG
# define INPUT_LOG_PROCESS         "input_log"
#endif
#ifdef _OUTPUT_LOG
# define OUTPUT_LOG_PROCESS        "output_log"
#endif
#ifdef _CONFIRMATION_LOG
# define CONFIRMATION_LOG_PROCESS  "confirmation_log"
#endif
#ifdef _DELETE_LOG
# define DELETE_LOG_PROCESS        "delete_log"
#endif
#ifdef _PRODUCTION_LOG
# define PRODUCTION_LOG_PROCESS    "production_log"
#endif
#ifdef _DISTRIBUTION_LOG
# define DISTRIBUTION_LOG_PROCESS  "distribution_log"
#endif
#if defined (_INPUT_LOG) || defined (_OUTPUT_LOG) || defined (_CONFIRMATION_LOG) || defined (_DELETE_LOG) || defined (_PRODUCTION_LOG) || defined (DISTRIBUTION_OFFSET)
# define ALDAD                     "aldad"
# define ALDA_CMD                  "alda"
#endif
#define ARCHIVE_WATCH              "archive_watch"
#define SHOW_LOG                   "show_log"
#define SHOW_CMD                   "show_cmd"
#define SHOW_CMD_LENGTH            (sizeof(SHOW_CMD) - 1)
#define AFD_STAT                   "afd_stat"
#define AFD_INFO                   "afd_info"
#define AFD_INFO_LENGTH            (sizeof(AFD_INFO) - 1)
#define EDIT_HC                    "edit_hc"
#define AFD_LOAD                   "afd_load"
#define AFD_CTRL                   "afd_ctrl"
#define AFD_CTRL_LENGTH            (sizeof(AFD_CTRL) - 1)
#define AFDD                       "afdd"
#define AFDDS                      "afdds"
#ifdef _WITH_ATPD_SUPPORT
# define ATPD                      "atpd"
#endif
#ifdef _WITH_WMOD_SUPPORT
# define WMOD                      "wmod"
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
# define DEMCD                     "demcd"
#endif
#define AFD_MON                    "afd_mon"
#define MON_PROC                   "mon"
#define LOG_MON                    "log_mon"
#define VIEW_HOSTS                 "view_hosts"
#define MON_CTRL                   "mon_ctrl"
#define MON_INFO                   "mon_info"
#define AFD_CMD                    "afdcmd"
#define AFDCFG                     "afdcfg"
#define VIEW_DC                    "view_dc"
#define GET_DC_DATA                "get_dc_data"
#define GET_DC_DATA_LENGTH         (sizeof(GET_DC_DATA) - 1)
#define GET_RR_DATA                "get_rr_data"
#define GET_RR_DATA_LENGTH         (sizeof(GET_RR_DATA) - 1)
#define JID_VIEW                   "jid_view"
#define DIR_CTRL                   "dir_ctrl"
#define DIR_INFO                   "dir_info"
#define DIR_CHECK                  "dir_check"
#define HANDLE_EVENT               "handle_event"
#define MAX_PROCNAME_LENGTH        18
#define AFTP                       "aftp"
#define ASFTP                      "asftp"
#define ASMTP                      "asmtp"
#define ASMTP_LENGTH               (sizeof(ASMTP) - 1)
#define AWMO                       "awmo"
#define HEX_PRINT                  "afd_hex_print"
#ifdef WITH_AUTO_CONFIG
# define AFD_AUTO_CONFIG           "afd_auto_config"
#endif
#define AFD_USER_NAME              "afd"
#define AFD_USER_NAME_LENGTH       (sizeof(AFD_USER_NAME) - 1)

#ifdef _OUTPUT_LOG
/* Definitions for different output types. */
# define OT_NORMAL_DELIVERED       0
# define OT_AGE_LIMIT_DELETE       1
# ifdef WITH_DUP_CHECK
#  define OT_DUPLICATE_STORED      2
#  define OT_DUPLICATE_DELETE      3
# endif
# define OT_OTHER_PROC_DELETE      4
# define OT_ADRESS_REJ_DELETE      5
# define OT_HOST_DISABLED_DELETE   6
# ifdef WITH_DUP_CHECK
#  define OT_DUPLICATE             7
# endif
# define OT_UNKNOWN                8
# define OT_NORMAL_RECEIVED        9
# if defined(_WITH_DE_MAIL_SUPPORT) && !defined(_CONFIRMATION_LOG)
#  define OT_CONF_OF_DISPATCH      10
#  define OT_CONF_OF_RECEIPT       11
#  define OT_CONF_OF_RETRIEVE      12
#  define OT_CONF_TIMEUP           13
# endif
#endif

#ifdef _DELETE_LOG
/* Definitions for different delete reasons. */
# define AGE_OUTPUT                 0
# define AGE_INPUT                  1
# define USER_DEL                   2
# define EXEC_FAILED_DEL            3
# define NO_MESSAGE_FILE_DEL        4
# ifdef WITH_DUP_CHECK
#  define DUP_INPUT                 5
#  define DUP_OUTPUT                6
# else
#  define DUP_OUTPUT                0 /* Dirty hack for show_dlog. */
# endif
# define DEL_UNKNOWN_FILE           7
# define JID_LOOKUP_FAILURE_DEL     8
# define DEL_OLD_LOCKED_FILE        9
# define DEL_QUEUED_FILE            10
# define DELETE_OPTION              11
# define DELETE_STALE_ERROR_JOBS    12
# define CLEAR_STALE_MESSAGES       13
# define FILE_CURRENTLY_TRANSMITTED 14
# define DELETE_UNKNOWN_POOL_DIR    15
# define EXEC_FAILED_STORED         16
# define DELETE_HOST_DISABLED       17
# define CONVERSION_FAILED          18
# define RENAME_OVERWRITE           19
# define RECIPIENT_REJECTED         20
# define MIRROR_REMOVE              21
# define MKDIR_QUEUE_ERROR          22
# define INTERNAL_LINK_FAILED       23
# define DEL_UNREADABLE_FILE        24
# define DEL_UNKNOWN_FILE_GLOB      25
# define DEL_OLD_LOCKED_FILE_GLOB   26
# define DEL_OLD_RLOCKED_FILE_GLOB  27
# define DEL_QUEUED_FILE_GLOB       28
# define MAX_DELETE_REASONS         28
# define UKN_DEL_REASON_STR         "Unknown delete reason"
# define UKN_DEL_REASON_STR_LENGTH  (sizeof(UKN_DEL_REASON_STR) - 1)
/* NOTE: Only 4096 (0 - fff) may be defined here. */
#define MAX_DELETE_REASON_LENGTH    (sizeof("Delete old locked file remote (AFD_CONFIG)") - 1)
/* Check dr_str.h! */
#endif

#ifdef _WITH_AFW2WMO
# define WMO_MESSAGE               2
#endif

/* Exit status of afd program that starts the AFD. */
#define AFD_IS_ACTIVE              5
#define AFD_DISABLED_BY_SYSADM     6
#define AFD_NOT_RESPONDING         9  /* If changed, update scripts/afd.in */
#define AFD_IS_NOT_ACTIVE          10 /* If changed, update scripts/afd.in */
#define NOT_ON_CORRECT_HOST        11 /* If changed, update scripts/afd.in */
#define AFD_MON_IS_NOT_ACTIVE      10
#define NO_DIR_CONFIG              -2

/* Definitions of lock ID's. */
#define EDIT_HC_LOCK_ID            0    /* Edit host configuration      */
#define EDIT_DC_LOCK_ID            1    /* Edit directory configuration */
#define AMG_LOCK_ID                2    /* AMG                          */
#define FD_LOCK_ID                 3    /* FD                           */
#define AW_LOCK_ID                 4    /* Archive watch                */
#define AS_LOCK_ID                 5    /* AFD statistics               */
#define AFDD_LOCK_ID               6    /* AFD TCP Daemon               */
#define AFDDS_LOCK_ID              7    /* AFD TLS TCP Daemon           */
#ifdef _WITH_ATPD_SUPPORT
# define ATPD_LOCK_ID              8    /* AFD Transfer Protocol Daemon */
# ifdef _WITH_WMOD_SUPPORT
#  define WMOD_LOCK_ID             9    /* WMO protocol Daemon          */
#  ifdef _WITH_DE_MAIL_SUPPORT
#   define DEMCD_LOCK_ID           10   /* De Mail Confirmation Daemon  */
#   define NO_OF_LOCK_PROC         11
#  else
#   define NO_OF_LOCK_PROC         10
#  endif
# else
#  ifdef _WITH_DE_MAIL_SUPPORT
#   define DEMCD_LOCK_ID           9    /* De Mail Confirmation Daemon  */
#   define NO_OF_LOCK_PROC         10
#  else
#   define NO_OF_LOCK_PROC         9
#  endif
# endif
#else
# ifdef _WITH_WMOD_SUPPORT
#  define WMOD_LOCK_ID             8    /* WMO protocol Daemon          */
#  ifdef _WITH_DE_MAIL_SUPPORT
#   define DEMCD_LOCK_ID           9    /* De Mail Confirmation Daemon  */
#   define NO_OF_LOCK_PROC         10
#  else
#   define NO_OF_LOCK_PROC         9
#  endif
# else
#  ifdef _WITH_DE_MAIL_SUPPORT
#   define DEMCD_LOCK_ID           8    /* De Mail Confirmation Daemon  */
#   define NO_OF_LOCK_PROC         9
#  else
#   define NO_OF_LOCK_PROC         8
#  endif
# endif
#endif

/* Commands that can be send to DB_UPDATE_FIFO of the AMG. */
#define HOST_CONFIG_UPDATE          4
#define DIR_CONFIG_UPDATE           5
#define REREAD_HOST_CONFIG          6
#define REREAD_HOST_CONFIG_VERBOSE1 7
#define REREAD_HOST_CONFIG_VERBOSE2 8
#define REREAD_DIR_CONFIG           9
#define REREAD_DIR_CONFIG_VERBOSE1  10
#define REREAD_DIR_CONFIG_VERBOSE2  11

#define WORK_DIR_ID                "-w"
#define WORK_DIR_ID_LENGTH         (sizeof(WORK_DIR_ID) - 1)

#define WAIT_LOOPS                 (MAX_SHUTDOWN_TIME + (MAX_SHUTDOWN_TIME / 2))

/* Definitions when AFD file directory is running full. */
#define STOP_AMG_THRESHOLD         20
#define START_AMG_THRESHOLD        100

/* Bit map flag to to enable/disable certain features in the AFD (FSA). */
#define DISABLE_RETRIEVE           1
#define DISABLE_ARCHIVE            2
#define ENABLE_CREATE_TARGET_DIR   4
#define DISABLE_HOST_WARN_TIME     8
#define DISABLE_CREATE_SOURCE_DIR  16
#define ENABLE_SIMULATE_SEND_MODE  32

/* Bit map flag to to enable/disable certain features in the AFD (FRA). */
#define DISABLE_DIR_WARN_TIME      1

/* The number of directories that are always in the AFD file directory: */
/*         ".", "..", "outgoing", "pool", "time", "incoming", "store",  */
/*         "crc"                                                        */
#ifdef WITH_DUP_CHECK
# define DIRS_IN_FILE_DIR          8
#else
# define DIRS_IN_FILE_DIR          7
#endif


/* Definitions for special_flag field in FSA. */
#define KEEP_CON_NO_FETCH          1
#define KEEP_CON_NO_SEND           2
#define HOST_DISABLED              32
#define HOST_IN_DIR_CONFIG         64  /* Host in DIR_CONFIG file (bit 7)*/

/* Process numbers that are started by AFD. */
#define AMG_NO                     0
#define FD_NO                      1
#define SLOG_NO                    2
#define ELOG_NO                    3
#define RLOG_NO                    4
#define TLOG_NO                    5
#define TDBLOG_NO                  6
#define AW_NO                      7
#define STAT_NO                    8
#define DC_NO                      9
#define AFDD_NO                    10
#define AFDDS_NO                   11
#ifdef _WITH_ATPD_SUPPORT
# define ATPD_OFFSET               1
# define ATPD_NO                   (AFDDS_NO + ATPD_OFFSET)
#else
# define ATPD_OFFSET               0
#endif
#ifdef _WITH_WMOD_SUPPORT
# define WMOD_OFFSET               1
# define WMOD_NO                   (AFDDS_NO + ATPD_OFFSET + WMOD_OFFSET)
#else
# define WMOD_OFFSET               0
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
# define DEMCD_OFFSET              1
# define DEMCD_NO                  (AFDDS_NO + ATPD_OFFSET + WMOD_OFFSET + DEMCD_OFFSET)
#else
# define DEMCD_OFFSET              0
#endif
#ifdef HAVE_MMAP
# define MAPPER_OFFSET             0
#else
# define MAPPER_OFFSET             1
# define MAPPER_NO                 (AFDDS_NO + ATPD_OFFSET + WMOD_OFFSET + DEMCD_OFFSET + MAPPER_OFFSET)
#endif
#ifdef _INPUT_LOG
# define INPUT_OFFSET              1
# define INPUT_LOG_NO              (AFDDS_NO + ATPD_OFFSET + WMOD_OFFSET + DEMCD_OFFSET + MAPPER_OFFSET + INPUT_OFFSET)
#else
# define INPUT_OFFSET              0
#endif
#ifdef _OUTPUT_LOG
# define OUTPUT_OFFSET             1
# define OUTPUT_LOG_NO             (AFDDS_NO + ATPD_OFFSET + WMOD_OFFSET + DEMCD_OFFSET + MAPPER_OFFSET + INPUT_OFFSET + OUTPUT_OFFSET)
#else
# define OUTPUT_OFFSET             0
#endif
#ifdef _CONFIRMATION_LOG
# define CONFIRMATION_OFFSET       1
# define CONFIRMATION_LOG_NO       (AFDDS_NO + ATPD_OFFSET + WMOD_OFFSET + DEMCD_OFFSET + MAPPER_OFFSET + INPUT_OFFSET + OUTPUT_OFFSET + CONFIRMATION_OFFSET)
#else
# define CONFIRMATION_OFFSET       0
#endif
#ifdef _DELETE_LOG
# define DELETE_OFFSET             1
# define DELETE_LOG_NO             (AFDDS_NO + ATPD_OFFSET + WMOD_OFFSET + DEMCD_OFFSET + MAPPER_OFFSET + INPUT_OFFSET + OUTPUT_OFFSET + CONFIRMATION_OFFSET + DELETE_OFFSET)
#else
# define DELETE_OFFSET             0
#endif
#ifdef _PRODUCTION_LOG
# define PRODUCTION_OFFSET         1
# define PRODUCTION_LOG_NO         (AFDDS_NO + ATPD_OFFSET + WMOD_OFFSET + DEMCD_OFFSET + MAPPER_OFFSET + INPUT_OFFSET + OUTPUT_OFFSET + CONFIRMATION_OFFSET + DELETE_OFFSET + PRODUCTION_OFFSET)
#else
# define PRODUCTION_OFFSET         0
#endif
#ifdef _DISTRIBUTION_LOG
# define DISTRIBUTION_OFFSET       1
# define DISTRIBUTION_LOG_NO       (AFDDS_NO + ATPD_OFFSET + WMOD_OFFSET + DEMCD_OFFSET + MAPPER_OFFSET + INPUT_OFFSET + OUTPUT_OFFSET + CONFIRMATION_OFFSET + DELETE_OFFSET + PRODUCTION_OFFSET + DISTRIBUTION_OFFSET)
#else
# define DISTRIBUTION_OFFSET       0
#endif
#ifdef _TRANSFER_RATE_LOG
# define TRANSFER_RATE_OFFSET      1
# define TRANSFER_RATE_LOG_NO      (AFDDS_NO + ATPD_OFFSET + WMOD_OFFSET + DEMCD_OFFSET + MAPPER_OFFSET + INPUT_OFFSET + OUTPUT_OFFSET + CONFIRMATION_OFFSET + DELETE_OFFSET + PRODUCTION_OFFSET + DISTRIBUTION_OFFSET + TRANSFER_RATE_OFFSET)
#else
# define TRANSFER_RATE_OFFSET      0
#endif
#define MAINTAINER_LOG_NO          (AFDDS_NO + ATPD_OFFSET + WMOD_OFFSET + DEMCD_OFFSET + MAPPER_OFFSET + INPUT_OFFSET + OUTPUT_OFFSET + CONFIRMATION_OFFSET + DELETE_OFFSET + PRODUCTION_OFFSET + DISTRIBUTION_OFFSET + TRANSFER_RATE_OFFSET + 1)
#define AFD_WORKER_NO              (AFDDS_NO + ATPD_OFFSET + WMOD_OFFSET + DEMCD_OFFSET + MAPPER_OFFSET + INPUT_OFFSET + OUTPUT_OFFSET + CONFIRMATION_OFFSET + DELETE_OFFSET + PRODUCTION_OFFSET + DISTRIBUTION_OFFSET + TRANSFER_RATE_OFFSET + 1 + 1)
#if defined (_INPUT_LOG) || defined (_OUTPUT_LOG) || defined (_CONFIRMATION_LOG) || defined (_DELETE_LOG) || defined (_PRODUCTION_LOG) || defined (_DISTRIBUTION_LOG)
# define ALDAD_OFFSET              1
# define ALDAD_NO                  (AFDDS_NO + ATPD_OFFSET + WMOD_OFFSET + DEMCD_OFFSET + MAPPER_OFFSET + INPUT_OFFSET + OUTPUT_OFFSET + CONFIRMATION_OFFSET + DELETE_OFFSET + PRODUCTION_OFFSET + DISTRIBUTION_OFFSET + TRANSFER_RATE_OFFSET + 1 + 1 + ALDAD_OFFSET)
#else
# define ALDAD_OFFSET              0
#endif
#define NO_OF_PROCESS              (AFDDS_NO + ATPD_OFFSET + WMOD_OFFSET + DEMCD_OFFSET + MAPPER_OFFSET + INPUT_OFFSET + OUTPUT_OFFSET + CONFIRMATION_OFFSET + DELETE_OFFSET + PRODUCTION_OFFSET + DISTRIBUTION_OFFSET + TRANSFER_RATE_OFFSET + 1 + 1 + ALDAD_OFFSET + 1)
#define SHOW_OLOG_NO               30

#define NA                         -1
#define NO                         0
#define YES                        1
#define NEITHER                    2
#define BOTH                       3
#define INCORRECT                  -1
#define PERMANENT_INCORRECT        -2
#define SUCCESS                    0
#define SIMULATION                 100
#define STALE                      -1
#define CON_RESET                  2
#define CON_REFUSED                3
#define ON                         1
#define OFF                        0
#define ALL                        0
#define ONE                        1
#define PAUSED                     2
#define PAUSED_REMOTE              2
#define DONE                       3
#define NORMAL                     4
#define PROCESS_NEEDS_RESTART      4
#define NONE                       5
#define NO_ACCESS                  10
#define STAT_ERROR                 17
#define CREATED_DIR                20
#define MKDIR_ERROR                26
#define CHOWN_ERROR                27
#define ALLOC_ERROR                34
#define LOCK_IS_SET                -2
#define LOCKFILE_NOT_THERE         -3
#define LOCK_IS_NOT_SET            11
#define AUTO_SIZE_DETECT           -2
#define FILE_IS_DIR                -2     /* Used by remove_dir().        */
#define GET_ONCE_ONLY              2
#define APPEND_ONLY                3
#define GET_ONCE_NOT_EXACT         4
#define DATA_MOVED                 1
#define DATA_COPIED                3
#define NORMAL_IDENTIFIER          0
#define GROUP_IDENTIFIER           1
#define LOCAL_ONLY                 2
#define REMOTE_ONLY                3

#define NO_PRIORITY                -100   /* So it knows it does not need */
                                          /* to create the name with      */
                                          /* priority, which is used by   */
                                          /* the function check_files().  */
                                          /* This is also used when not   */
                                          /* setting scheduling priorities*/
                                          /* in exec_cmd().               */
#define INCORRECT_VERSION          -2
#define EQUAL_SIGN                 1
#define LESS_THEN_SIGN             2
#define GREATER_THEN_SIGN          3
#define NOT_SIGN                   4

/* Size definitions. */
#define KILOFILE                   1000
#define MEGAFILE                   1000000
#define GIGAFILE                   1000000000
#define TERAFILE                   1000000000000LL
#define PETAFILE                   1000000000000000LL
#define EXAFILE                    1000000000000000000LL
#define F_KILOFILE                 1000.0
#define F_MEGAFILE                 1000000.0
#define F_GIGAFILE                 1000000000.0
#define F_TERAFILE                 1000000000000.0
#define F_PETAFILE                 1000000000000000.0
#define F_EXAFILE                  1000000000000000000.0
#define KILOBYTE                   1024
#define MEGABYTE                   1048576
#define GIGABYTE                   1073741824
#define TERABYTE                   1099511627776LL
#define PETABYTE                   1125899906842624LL
#define EXABYTE                    1152921504606846976LL
#define F_KILOBYTE                 1024.0
#define F_MEGABYTE                 1048576.0
#define F_GIGABYTE                 1073741824.0
#define F_TERABYTE                 1099511627776.0
#define F_PETABYTE                 1125899906842624.0
#define F_EXABYTE                  1152921504606846976.0
/* Next comes ZETTABYTE and YOTTABYTE, but these are to large for 2^64, */
/* ie. for 64-bit systems. Wonder what will be the size of the next     */
/* systems.                                                             */


#ifdef WITH_ONETIME
# define ONETIME_JOB_ID          4294967295U
#endif

#define SSH_PORT_UNSET           -2
#ifdef WITH_SSH_FINGERPRINT
# define SSH_RSA_KEY             1
# define SSH_DSS_KEY             2
# define SSH_PGP_DSS_KEY         3
# define SSH_PGP_RSA_KEY         4
# define MAX_FINGERPRINT_LENGTH  47
#endif

/* Definitions for ignore options in struct fileretrieve_status. */
#define ISIZE_EQUAL                1
#define ISIZE_LESS_THEN            2
#define ISIZE_GREATER_THEN         4
#define ISIZE_OFF_MASK             7
#define IFTIME_EQUAL               8
#define IFTIME_LESS_THEN           16
#define IFTIME_GREATER_THEN        32
#define IFTIME_OFF_MASK            56

#define INFO_SIGN                  "<I>"
#define CONFIG_SIGN                "<C>"   /* If changed see:            */
                                           /*           config_log.c     */
                                           /*           mconfig_log.c    */
#define OFFLINE_SIGN               "<O>"
#define WARN_SIGN                  "<W>"
#define ERROR_SIGN                 "<E>"
#define FATAL_SIGN                 "<F>"   /* donated by Paul Merken.    */
#define DEBUG_SIGN                 "<D>"
#define TRACE_SIGN                 "<T>"
#define DUMMY_SIGN                 "<#>"   /* To always show some general*/
                                           /* information, eg month.     */
#define LOG_SIGN_LENGTH            3

#define SEPARATOR                  "-->"

#define INFO_NO                    1
#define CONFIG_NO                  2
#define WARN_NO                    3
#define ERROR_NO                   4
#define FATAL_NO                   5
/* NOTE: Check UI/Motif/x_common_defs.h if the above are changed. */

#define NOT_APPLICABLE_SIGN        'X'

/* Separator to separate elements in log files. */
#define SEPARATOR_CHAR             '|'

/* Definitions of different exit status. */
#define NOT_RUNNING                -1
#define UNKNOWN_STATE              -2
#define STOPPED                    -3
#define DIED                       -4
#define PROC_INIT_VALUE            -10

/* Definitions for different addresses for one host. */
#define HOST_ONE                   1
#define HOST_TWO                   2
#define DEFAULT_TOGGLE_HOST        HOST_ONE
#define HOST_TWO_FLAG              64 /* For 'Host status' in HOST_CONFIG. */
#define AUTO_TOGGLE_OPEN           '{'
#define AUTO_TOGGLE_CLOSE          '}'
#define STATIC_TOGGLE_OPEN         '['
#define STATIC_TOGGLE_CLOSE        ']'

/* Some editors cannot handle curly and spare brackets in code. */
#define CURLY_BRACKET_OPEN         '{'
#define CURLY_BRACKET_CLOSE        '}'
#define SQUARE_BRACKET_OPEN        '['
#define SQUARE_BRACKET_CLOSE       ']'

/* Definitions of the protocol's and extensions. */
#define UNKNOWN_FLAG               0
#define FTP                        0
#define FTP_FLAG                   1
#define LOC                        1
#define LOC_FLAG                   2
#define LOCAL_ID                   "local"
#define SMTP                       2
#define SMTP_FLAG                  4
#ifdef _WITH_MAP_SUPPORT
# define MAP                       3 /* NOTE: when changed, change log/aldadefs.h! */
# define MAP_FLAG                  8
#endif
#ifdef _WITH_SCP_SUPPORT
# define SCP                       4 /* NOTE: when changed, change log/aldadefs.h! */
# define SCP_FLAG                  16
#endif /* _WITH_SCP_SUPPORT */
#ifdef _WITH_WMO_SUPPORT
# define WMO                       5 /* NOTE: when changed, change log/aldadefs.h! */
# define WMO_FLAG                  32
#endif
#define HTTP                       6
#define HTTP_FLAG                  64
#ifdef WITH_SSL
# define SSL_FLAG                  536870912
# define FTPS                      7 /* NOTE: when changed, change log/aldadefs.h! */
# define HTTPS                     8 /* NOTE: when changed, change log/aldadefs.h! */
# define SMTPS                     9
#endif
#define SFTP                       10
#define SFTP_FLAG                  128
#define EXEC                       11
#define EXEC_FLAG                  256
#ifdef _WITH_DFAX_SUPPORT
# define DFAX                      12 /* NOTE: when changed, change log/aldadefs.h! */
# define DFAX_FLAG                 512
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
# define DE_MAIL                   13 /* NOTE: when changed, change log/aldadefs.h! */
# define DE_MAIL_FLAG              1024
#endif
#define DISABLE_IPV6_FLAG          268435456
#define SEND_FLAG                  1073741824
#define RETRIEVE_FLAG              2147483648U

/* Different SMTP authentication methods. */
#define SMTP_AUTH_NONE             0
#define SMTP_AUTH_LOGIN            1
#define SMTP_AUTH_PLAIN            2

/* Different URL auth= options. */
#define AUTH_NONE                  0
#define AUTH_BASIC                 1
#define AUTH_DIGEST                2
#define AUTH_AWS4_HMAC_SHA256      4
#define AUTH_AWS_NO_SIGN_REQUEST   8

/* Different URL service= options. */
#define SERVICE_NONE               0
#define SERVICE_S3                 1

#define MIN_KEEP_ALIVE_INTERVAL    45L

/* Bitmap containing errors in a given URL. */
#define TARGET_DIR_CAN_CHANGE      1
#define FOR_FUTURE_NEEDS           2  /* Not used! */
#define UNKNOWN_SCHEME             4
#define USER_NAME_TO_LONG          8
#ifdef WITH_SSH_FINGERPRINT
# define UNKNOWN_KEY_TYPE          16
# define NOT_A_FINGERPRINT         32
# define ONLY_FINGERPRINT_KNOWN    64
#endif
#define PASSWORD_TO_LONG           128
#define HOSTNAME_TO_LONG           256
#define PORT_TO_LONG               512
#define TIME_MODIFIER_TO_LONG      1024
#define NO_TIME_MODIFIER_SPECIFIED 2048
#define RECIPIENT_TO_LONG          4096
#define UNKNOWN_TRANSFER_TYPE      8192
#define PROTOCOL_VERSION_TO_LONG   16384
#define NO_PROTOCOL_VERSION        32768
#define NOT_A_URL                  65536
#define UNKNOWN_SMTP_AUTH          131072
#define NO_PORT_SPECIFIED          262144
#define SERVER_NAME_TO_LONG        524288
#define EXEC_CMD_ERROR             1048576
#define EXEC_NO_RETURN             2097152
#define EXEC_NOT_TERMINATED        4194304
#define EXEC_CMD_TO_LONG           8388608
#define BUFFER_TO_SHORT            16777216
#define REGION_NAME_TO_LONG        33554432
#define PARAMETER_MISSING          67108864
#define URL_UNKNOWN_VALUE          134217728
#ifdef WITH_SSH_FINGERPRINT
                           /*    USER_NAME_TO_LONG     UNKNOWN_SMTP_AUTH UNKNOWN_KEY_TYPE NOT_A_FINGERPRINT ONLY_FINGERPRINT_KNOWN PASSWORD_TO_LONG      HOSTNAME_TO_LONG      PORT_TO_LONG          TIME_MODIFIER_TO_LONG NO_TIME_MODIFIER_SPECIFIED RECIPIENT_TO_LONG     UNKNOWN_TRANSFER_TYPE TARGET_DIR_CAN_CHANGE PROTOCOL_VERSION_TO_LONG NO_PROTOCOL_VERSION NO_PORT_SPECIFIED SERVER_NAME_TO_LONG   EXEC_CMD_ERROR EXEC_NO_RETURN EXEC_NOT_TERMINATED EXEC_CMD_TO_LONG      REGION_NAME_TO_LONG   PARAMETER_MISSING URL_UNKNOWN_VALUE BUFFER_TO_SHORT */
# define MAX_URL_ERROR_MSG (14 + 35 + MAX_INT_LENGTH + 29 +              18 +             21 +              48 +                   34 + MAX_INT_LENGTH + 34 + MAX_INT_LENGTH + 37 + MAX_INT_LENGTH + 47 + MAX_INT_LENGTH + 34 +                       35 + MAX_INT_LENGTH + 23 +                  28 +                  42 + MAX_INT_LENGTH +    30 +                25 +              37 + MAX_INT_LENGTH + 29 +           25 +           43 +                52 + MAX_INT_LENGTH + 37 + MAX_INT_LENGTH + 19 +              15 +              17 +            1)
#else
                           /*    USER_NAME_TO_LONG     UNKNOWN_SMTP_AUTH                                                           PASSWORD_TO_LONG      HOSTNAME_TO_LONG      PORT_TO_LONG          TIME_MODIFIER_TO_LONG NO_TIME_MODIFIER_SPECIFIED RECIPIENT_TO_LONG     UNKNOWN_TRANSFER_TYPE TARGET_DIR_CAN_CHANGE PROTOCOL_VERSION_TO_LONG NO_PROTOCOL_VERSION NO_PORT_SPECIFIED SERVER_NAME_TO_LONG   EXEC_CMD_ERROR EXEC_NO_RETURN EXEC_NOT_TERMINATED EXEC_CMD_TO_LONG      REGION_NAME_TO_LONG   PARAMETER_MISSING URL_UNKNOWN_VALUE BUFFER_TO_SHORT */
# define MAX_URL_ERROR_MSG (14 + 35 + MAX_INT_LENGTH + 29 +                                                                        34 + MAX_INT_LENGTH + 34 + MAX_INT_LENGTH + 37 + MAX_INT_LENGTH + 47 + MAX_INT_LENGTH + 34 +                       35 + MAX_INT_LENGTH + 23 +                  28 +                  42 + MAX_INT_LENGTH +    30 +                25 +              37 + MAX_INT_LENGTH + 29 +           25 +           43 +                52 + MAX_INT_LENGTH + 37 + MAX_INT_LENGTH + 19 +              15 +              17 +            1)
#endif

/* When looking at difference in two URL's, flags for which parts differ. */
#define URL_SCHEME_DIFS            1
#define URL_PORT_DIFS              2
#define URL_TRANSFER_TYPE_DIFS     4
#define URL_PROTOCOL_VERSION_DIFS  8
#define URL_SMTP_AUTH_DIFS         16
#define URL_USER_DIFS              32
#define URL_SMTP_USER_DIFS         64
#define URL_PASSWORD_DIFS          128
#define URL_HOSTNAME_DIFS          256
#define URL_PATH_DIFS              512
#define URL_SERVER_DIFS            1024
#define URL_KEYTYPE_DIFS           2048
#ifdef WITH_SSH_FINGERPRINT
# define URL_FINGERPRINT_DIFS      4096
#endif
#define URL_REGION_DIFS            8192
#define URL_AUTH_DIFS              16384
#define URL_SERVICE_DIFS           32768

/* Definitions for protocol_options in FSA. */
#define FTP_PASSIVE_MODE           1                 /*  1 */
#define SET_IDLE_TIME              2                 /*  2 */
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
# define STAT_KEEPALIVE            4                 /*  3 */
#endif
#define FTP_FAST_MOVE              8                 /*  4 */
#define FTP_FAST_CD                16                /*  5 */
#define FTP_IGNORE_BIN             32                /*  6 */
#define FTP_EXTENDED_MODE          64                /*  7 */
#ifdef _WITH_BURST_2
# define DISABLE_BURSTING          128               /*  8 */
#endif
#define FTP_ALLOW_DATA_REDIRECT    256               /*  9 */
#define FILE_WHEN_LOCAL_FLAG       512               /* 10 */
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
# define AFD_TCP_KEEPALIVE         1024              /* 11 */
#endif
#define USE_SEQUENCE_LOCKING       2048              /* 12 */
#define ENABLE_COMPRESSION         4096              /* 13 */
#define KEEP_TIME_STAMP            8192              /* 14 */
#define SORT_FILE_NAMES            16384             /* 15 */
#define NO_AGEING_JOBS             32768             /* 16 */
#define CHECK_SIZE                 65536             /* 17 */
#define TIMEOUT_TRANSFER           131072            /* 18 */
#define KEEP_CON_NO_FETCH_2        262144            /* 19 */
#define KEEP_CON_NO_SEND_2         524288            /* 20 */
#define FTP_CCC_OPTION             1048576           /* 21 */
#define FTP_USE_LIST               2097152           /* 22 */
#define TLS_STRICT_VERIFY          4194304           /* 23 */
#define FTP_DISABLE_MLST           8388608           /* 24 */
#define KEEP_CONNECTED_DISCONNECT  16777216          /* 25 */
#define DISABLE_STRICT_HOST_KEY    33554432          /* 26 */
#define USE_STAT_LIST              67108864          /* 27 */
#define IMPLICIT_FTPS              134217728         /* 28 */
#ifdef _WITH_EXTRA_CHECK
# define USE_EXTRA_CHECK           268435456         /* 29 */
#endif
#define NO_EXPECT                  536870912         /* 30 */
#define HTTP_BUCKETNAME_IN_PATH    1073741824        /* 31 */
#define TLS_LEGACY_RENEGOTIATION   2147483648U       /* 32 */

/* Definitions for protocol_options2 in FSA. */
#define FTP_SEND_UTF8_ON           1                 /*  1 */

/* Definitions for protocol_options in sf_xxx + gf_xxx functions. */
#define PROT_OPT_NO_EXPECT                 1
#define BUCKETNAME_IS_IN_PATH              2
#ifdef WITH_SSL
# define PROT_OPT_TLS_STRICT_VERIFY        4
# define PROT_OPT_TLS_LEGACY_RENEGOTIATION 8
#endif


#define FTP_SHEME                  "ftp"
#define FTP_SHEME_LENGTH           (sizeof(FTP_SHEME) - 1)
#ifdef WITH_SSL
# define FTPS_SHEME                "ftps"
# define FTPS_SHEME_LENGTH         (sizeof(FTPS_SHEME) - 1)
#endif
#define LOC_SHEME                  "file"
#define LOC_SHEME_LENGTH           (sizeof(LOC_SHEME) - 1)
#define EXEC_SHEME                 "exec"
#define EXEC_SHEME_LENGTH          (sizeof(EXEC_SHEME) - 1)
#ifdef _WITH_SCP_SUPPORT
# define SCP_SHEME                 "scp" /* NOTE: when changed, change log/alda/aldadefs.h! */
# define SCP_SHEME_LENGTH          (sizeof(SCP_SHEME) - 1)
#endif
#ifdef _WITH_WMO_SUPPORT
# define WMO_SHEME                 "wmo" /* NOTE: when changed, change log/alda/aldadefs.h! */
# define WMO_SHEME_LENGTH          (sizeof(WMO_SHEME) - 1)
#endif
#ifdef _WITH_MAP_SUPPORT
# define MAP_SHEME                 "map" /* NOTE: when changed, change log/alda/aldadefs.h! */
# define MAP_SHEME_LENGTH          (sizeof(MAP_SHEME) - 1)
#endif
#define SMTP_SHEME                 "mailto"
#define SMTP_SHEME_LENGTH          (sizeof(SMTP_SHEME) - 1)
#ifdef WITH_SSL
# define SMTPS_SHEME               "mailtos" /* NOTE: when changed, change log/alda/aldadefs.h! */
# define SMTPS_SHEME_LENGTH        (sizeof(SMTPS_SHEME) - 1)
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
# define DEMAIL_SHEME              "demail"
# define DEMAIL_SHEME_LENGTH       (sizeof(DEMAIL_SHEME) - 1)
#endif
#define HTTP_SHEME                 "http"
#define HTTP_SHEME_LENGTH          (sizeof(HTTP_SHEME) - 1)
#ifdef WITH_SSL
# define HTTPS_SHEME               "https" /* NOTE: when changed, change log/alda/aldadefs.h! */
# define HTTPS_SHEME_LENGTH        (sizeof(HTTPS_SHEME) - 1)
#endif
#define SFTP_SHEME                 "sftp"
#define SFTP_SHEME_LENGTH          (sizeof(SFTP_SHEME) - 1)
#ifdef _WITH_DFAX_SUPPORT
# define DFAX_SHEME                "dfax" /* NOTE: when changed, change log/alda/aldadefs.h! */
# define DFAX_SHEME_LENGTH         (sizeof(DFAX_SHEME) - 1)
#endif


/* Definitions for [dir options]. */
#define DEL_UNKNOWN_FILES_ID             "delete unknown files"
#define DEL_UNKNOWN_FILES_ID_LENGTH      (sizeof(DEL_UNKNOWN_FILES_ID) - 1)
#define DEL_QUEUED_FILES_ID              "delete queued files"
#define DEL_QUEUED_FILES_ID_LENGTH       (sizeof(DEL_QUEUED_FILES_ID) - 1)
#define DEL_OLD_LOCKED_FILES_ID          "delete old locked files"
#define DEL_OLD_LOCKED_FILES_ID_LENGTH   (sizeof(DEL_OLD_LOCKED_FILES_ID) - 1)
#define DEL_UNREADABLE_FILES_ID          "delete unreadable files"
#define DEL_UNREADABLE_FILES_ID_LENGTH   (sizeof(DEL_UNREADABLE_FILES_ID) - 1)
#ifdef WITH_INOTIFY
# define INOTIFY_FLAG_ID                 "inotify"
# define INOTIFY_FLAG_ID_LENGTH          (sizeof(INOTIFY_FLAG_ID) - 1)
#endif
#define OLD_FILE_TIME_ID                 "old file time"
#define OLD_FILE_TIME_ID_LENGTH          (sizeof(OLD_FILE_TIME_ID) - 1)
#define DONT_REP_UNKNOWN_FILES_ID        "do not report unknown files"
#define DONT_REP_UNKNOWN_FILES_ID_LENGTH (sizeof(DONT_REP_UNKNOWN_FILES_ID) - 1)
#define END_CHARACTER_ID                 "end character"
#define END_CHARACTER_ID_LENGTH          (sizeof(END_CHARACTER_ID) - 1)
#define TIME_ID                          "time"
#define TIME_ID_LENGTH                   (sizeof(TIME_ID) - 1)
#define MAX_PROCESS_ID                   "max process"
#define MAX_PROCESS_ID_LENGTH            (sizeof(MAX_PROCESS_ID) - 1)
#define DO_NOT_REMOVE_ID                 "do not remove"
#define DO_NOT_REMOVE_ID_LENGTH          (sizeof(DO_NOT_REMOVE_ID) - 1)
#define STORE_RETRIEVE_LIST_ID           "store retrieve list"
#define STORE_RETRIEVE_LIST_ID_LENGTH    (sizeof(STORE_RETRIEVE_LIST_ID) - 1)
#define STORE_REMOTE_LIST                "store remote list"
#define STORE_REMOTE_LIST_LENGTH         (sizeof(STORE_REMOTE_LIST) - 1)
#define DONT_DEL_UNKNOWN_FILES_ID        "do not delete unknown files"
#define DONT_DEL_UNKNOWN_FILES_ID_LENGTH (sizeof(DONT_DEL_UNKNOWN_FILES_ID) - 1)
#define REP_UNKNOWN_FILES_ID             "report unknown files"
#define REP_UNKNOWN_FILES_ID_LENGTH      (sizeof(REP_UNKNOWN_FILES_ID) - 1)
#define FORCE_REREAD_ID                  "force reread"
#define FORCE_REREAD_ID_LENGTH           (sizeof(FORCE_REREAD_ID) - 1)
#define FORCE_REREAD_LOCAL_ID            "force reread local"
#define FORCE_REREAD_LOCAL_ID_LENGTH     (sizeof(FORCE_REREAD_LOCAL_ID) - 1)
#define FORCE_REREAD_REMOTE_ID           "force reread remote"
#define FORCE_REREAD_REMOTE_ID_LENGTH    (sizeof(FORCE_REREAD_REMOTE_ID) - 1)
#define IMPORTANT_DIR_ID                 "important dir"
#define IMPORTANT_DIR_ID_LENGTH          (sizeof(IMPORTANT_DIR_ID) - 1)
#define IGNORE_SIZE_ID                   "ignore size"
#define IGNORE_SIZE_ID_LENGTH            (sizeof(IGNORE_SIZE_ID) - 1)
#define IGNORE_FILE_TIME_ID              "ignore file time"
#define IGNORE_FILE_TIME_ID_LENGTH       (sizeof(IGNORE_FILE_TIME_ID) - 1)
#define MAX_FILES_ID                     "max files"
#define MAX_FILES_ID_LENGTH              (sizeof(MAX_FILES_ID) - 1)
#define MAX_SIZE_ID                      "max size"
#define MAX_SIZE_ID_LENGTH               (sizeof(MAX_SIZE_ID) - 1)
#define WAIT_FOR_FILENAME_ID             "wait for"
#define WAIT_FOR_FILENAME_ID_LENGTH      (sizeof(WAIT_FOR_FILENAME_ID) - 1)
#define ACCUMULATE_ID                    "accumulate"
#define ACCUMULATE_ID_LENGTH             (sizeof(ACCUMULATE_ID) - 1)
#define ACCUMULATE_SIZE_ID               "accumulate size"
#define ACCUMULATE_SIZE_ID_LENGTH        (sizeof(ACCUMULATE_SIZE_ID) - 1)
#ifdef WITH_DUP_CHECK
# define DUPCHECK_ID                     "dupcheck"
# define DUPCHECK_ID_LENGTH              (sizeof(DUPCHECK_ID) - 1)
#endif
#define ACCEPT_DOT_FILES_ID              "accept dot files"
#define ACCEPT_DOT_FILES_ID_LENGTH       (sizeof(ACCEPT_DOT_FILES_ID) - 1)
#define DO_NOT_GET_DIR_LIST_ID           "do not get dir list"
#define DO_NOT_GET_DIR_LIST_ID_LENGTH    (sizeof(DO_NOT_GET_DIR_LIST_ID) - 1)
#define URL_CREATES_FILE_NAME_ID         "url creates file name"
#define URL_CREATES_FILE_NAME_ID_LENGTH  (sizeof(URL_CREATES_FILE_NAME_ID) -1)
#define URL_WITH_INDEX_FILE_NAME_ID      "url with index file name"
#define URL_WITH_INDEX_FILE_NAME_ID_LENGTH (sizeof(URL_WITH_INDEX_FILE_NAME_ID) -1)
#define DIR_WARN_TIME_ID                 "warn time"
#define DIR_WARN_TIME_ID_LENGTH          (sizeof(DIR_WARN_TIME_ID) - 1)
#define DIR_INFO_TIME_ID                 "info time"
#define DIR_INFO_TIME_ID_LENGTH          (sizeof(DIR_INFO_TIME_ID) - 1)
#define KEEP_CONNECTED_ID                "keep connected"
#define KEEP_CONNECTED_ID_LENGTH         (sizeof(KEEP_CONNECTED_ID) - 1)
#define CREATE_SOURCE_DIR_ID             "create source dir"
#define CREATE_SOURCE_DIR_ID_LENGTH      (sizeof(CREATE_SOURCE_DIR_ID) - 1)
#define DONT_CREATE_SOURCE_DIR_ID        "do not create source dir"
#define DONT_CREATE_SOURCE_DIR_ID_LENGTH (sizeof(DONT_CREATE_SOURCE_DIR_ID) - 1)
#define MAX_ERRORS_ID                    "max errors"
#define MAX_ERRORS_ID_LENGTH             (sizeof(MAX_ERRORS_ID) - 1)
#define DO_NOT_PARALLELIZE_ID            "do not parallelize"
#define DO_NOT_PARALLELIZE_ID_LENGTH     (sizeof(DO_NOT_PARALLELIZE_ID) - 1)
#define TIMEZONE_ID                      "timezone"
#define TIMEZONE_ID_LENGTH               (sizeof(TIMEZONE_ID) - 1)
#define LS_DATA_FILENAME_ID              "ls data filename"
#define LS_DATA_FILENAME_ID_LENGTH       (sizeof(LS_DATA_FILENAME_ID) - 1)
#define LOCAL_REMOTE_DIR_ID              "local remote dir"
#define LOCAL_REMOTE_DIR_ID_LENGTH       (sizeof(LOCAL_REMOTE_DIR_ID) - 1)
#define ONE_PROCESS_JUST_SCANNING_ID     "one process just scanning"
#define ONE_PROCESS_JUST_SCANNING_ID_LENGTH (sizeof(ONE_PROCESS_JUST_SCANNING_ID) - 1)
#define NO_DELIMITER_ID                  "no delimiter"
#define NO_DELIMITER_ID_LENGTH           (sizeof(NO_DELIMITER_ID) - 1)
#define KEEP_PATH_ID                     "keep path"
#define KEEP_PATH_ID_LENGTH              (sizeof(KEEP_PATH_ID) - 1)
#define UNKNOWN_FILES                    1
#define QUEUED_FILES                     2
#define OLD_LOCKED_FILES                 4
#define UNREADABLE_FILES                 8
#define OLD_RLOCKED_FILES                16

/* Definitions for [options]. */
#define TIFF2GTS_ID                      "tiff2gts"
#define DELETE_ID                        "delete"
#define DELETE_ID_LENGTH                 (sizeof(DELETE_ID) - 1)
#define AGE_LIMIT_ID                     "age-limit"
#define AGE_LIMIT_ID_LENGTH              (sizeof(AGE_LIMIT_ID) - 1)
#ifdef _WITH_TRANS_EXEC
# define TRANS_EXEC_ID                   "pexec"
# define TRANS_EXEC_ID_LENGTH            (sizeof(TRANS_EXEC_ID) - 1)
#endif

/* Definitions for FD [options]. */
#define OUTPUT_LOG_ID                    "no log output"
#define OUTPUT_LOG_ID_LENGTH             (sizeof(OUTPUT_LOG_ID) - 1)
#define ARCHIVE_ID                       "archive"                  
#define ARCHIVE_ID_LENGTH                (sizeof(ARCHIVE_ID) - 1)
#define LOCK_ID                          "lock"
#define LOCK_ID_LENGTH                   (sizeof(LOCK_ID) - 1)
#define ULOCK_ID                         "ulock"
#define ULOCK_ID_LENGTH                  (sizeof(ULOCK_ID) - 1)
#define LOCK_POSTFIX_ID                  "lockp"
#define LOCK_POSTFIX_ID_LENGTH           (sizeof(LOCK_POSTFIX_ID) - 1)
#define REMOTE_HARDLINK_ID               "hardlink"
#define REMOTE_HARDLINK_ID_LENGTH        (sizeof(REMOTE_HARDLINK_ID) - 1)
#define REMOTE_SYMLINK_ID                "symlink"
#define REMOTE_SYMLINK_ID_LENGTH         (sizeof(REMOTE_SYMLINK_ID) - 1)
#define RESTART_FILE_ID                  "restart"
#define RESTART_FILE_ID_LENGTH           (sizeof(RESTART_FILE_ID) - 1)
#define TRANS_RENAME_ID                  "trans_rename"
#define TRANS_RENAME_ID_LENGTH           (sizeof(TRANS_RENAME_ID) - 1)
#define TRANS_SRENAME_ID                 "trans_srename"
#define TRANS_SRENAME_ID_LENGTH          (sizeof(TRANS_SRENAME_ID) - 1)
#ifdef _WITH_WMO_SUPPORT
# define WITH_SEQUENCE_NUMBER_ID         "sequence numbering"
# define WITH_SEQUENCE_NUMBER_ID_LENGTH  (sizeof(WITH_SEQUENCE_NUMBER_ID) - 1)
# define CHECK_REPLY_ID                  "check reply"
# define CHECK_REPLY_ID_LENGTH           (sizeof(CHECK_REPLY_ID) - 1)
#endif
#define FILE_NAME_IS_HEADER_ID           "file name is header"
#define FILE_NAME_IS_HEADER_ID_LENGTH    (sizeof(FILE_NAME_IS_HEADER_ID) - 1)
#define FILE_NAME_IS_USER_ID             "file name is user"
#define FILE_NAME_IS_USER_ID_LENGTH      (sizeof(FILE_NAME_IS_USER_ID) - 1)
#define FILE_NAME_IS_TARGET_ID           "file name is target"
#define FILE_NAME_IS_TARGET_ID_LENGTH    (sizeof(FILE_NAME_IS_TARGET_ID) - 1)
#define FILE_NAME_IS_SUBJECT_ID          "file name is subject"
#define FILE_NAME_IS_SUBJECT_ID_LENGTH   (sizeof(FILE_NAME_IS_SUBJECT_ID) - 1)
#define ADD_MAIL_HEADER_ID               "mail header"
#define ADD_MAIL_HEADER_ID_LENGTH        (sizeof(ADD_MAIL_HEADER_ID) - 1)
#define ATTACH_FILE_ID                   "attach file"
#define ATTACH_FILE_ID_LENGTH            (sizeof(ATTACH_FILE_ID) - 1)
#define ATTACH_ALL_FILES_ID              "attach all files"
#define ATTACH_ALL_FILES_ID_LENGTH       (sizeof(ATTACH_ALL_FILES_ID) - 1)
#define REPLY_TO_ID                      "reply-to"
#define REPLY_TO_ID_LENGTH               (sizeof(REPLY_TO_ID) - 1)
#define GROUP_TO_ID                      "group-to"
#define GROUP_TO_ID_LENGTH               (sizeof(GROUP_TO_ID) - 1)
#define FROM_ID                          "from"
#define FROM_ID_LENGTH                   (sizeof(FROM_ID) - 1)
#define CHARSET_ID                       "charset"
#define CHARSET_ID_LENGTH                (sizeof(CHARSET_ID) - 1)
#ifdef _WITH_DE_MAIL_SUPPORT
# define CONF_OF_RETRIEVE_ID             "confirmation of retrieve"
# define CONF_OF_RETRIEVE_ID_LENGTH      (sizeof(CONF_OF_RETRIEVE_ID) - 1)
#endif
#ifdef WITH_EUMETSAT_HEADERS
# define EUMETSAT_HEADER_ID              "eumetsat"
# define EUMETSAT_HEADER_ID_LENGTH       (sizeof(EUMETSAT_HEADER_ID) - 1)
#endif
#define CHMOD_ID                         "chmod"
#define CHMOD_ID_LENGTH                  (sizeof(CHMOD_ID) - 1)
#define CHOWN_ID                         "chown"
#define CHOWN_ID_LENGTH                  (sizeof(CHOWN_ID) - 1)
#define ENCODE_ANSI_ID                   "encode ansi"
#define ENCODE_ANSI_ID_LENGTH            (sizeof(ENCODE_ANSI_ID) - 1)
#define SUBJECT_ID                       "subject"                   
#define SUBJECT_ID_LENGTH                (sizeof(SUBJECT_ID) - 1)
#define FORCE_COPY_ID                    "force copy"
#define FORCE_COPY_ID_LENGTH             (sizeof(FORCE_COPY_ID) - 1)
#define RENAME_FILE_BUSY_ID              "file busy rename"
#define RENAME_FILE_BUSY_ID_LENGTH       (sizeof(RENAME_FILE_BUSY_ID) - 1)
#define ACTIVE_FTP_MODE                  "mode active"
#define ACTIVE_FTP_MODE_LENGTH           (sizeof(ACTIVE_FTP_MODE) - 1)
#define PASSIVE_FTP_MODE                 "mode passive"
#define PASSIVE_FTP_MODE_LENGTH          (sizeof(PASSIVE_FTP_MODE) - 1)
#define FTP_EXEC_CMD                     "site"
#define FTP_EXEC_CMD_LENGTH              (sizeof(FTP_EXEC_CMD) - 1)
#define LOGIN_SITE_CMD                   "login site"
#define LOGIN_SITE_CMD_LENGTH            (sizeof(LOGIN_SITE_CMD) - 1)
#define CREATE_TARGET_DIR_ID             "create target dir"
#define CREATE_TARGET_DIR_ID_LENGTH      (sizeof(CREATE_TARGET_DIR_ID) - 1)
#define DONT_CREATE_TARGET_DIR           "do not create target dir"
#define DONT_CREATE_TARGET_DIR_LENGTH    (sizeof(DONT_CREATE_TARGET_DIR) - 1)
#define SEQUENCE_LOCKING_ID              "sequence locking"
#define SEQUENCE_LOCKING_ID_LENGTH       (sizeof(SEQUENCE_LOCKING_ID) - 1)
#define SOCKET_SEND_BUFFER_ID            "socket send buffer"
#define SOCKET_SEND_BUFFER_ID_LENGTH     (sizeof(SOCKET_SEND_BUFFER_ID) - 1)
#define SOCKET_RECEIVE_BUFFER_ID         "socket receive buffer"
#define SOCKET_RECEIVE_BUFFER_ID_LENGTH  (sizeof(SOCKET_RECEIVE_BUFFER_ID) - 1)
#define MIRROR_DIR_ID                    "mirror source"
#define MIRROR_DIR_ID_LENGTH             (sizeof(MIRROR_DIR_ID) - 1)
#define SHOW_ALL_GROUP_MEMBERS_ID        "show all group members"
#define SHOW_ALL_GROUP_MEMBERS_ID_LENGTH (sizeof(SHOW_ALL_GROUP_MEMBERS_ID) - 1)
#define HIDE_ALL_GROUP_MEMBERS_ID        "hide all group members"
#define HIDE_ALL_GROUP_MEMBERS_ID_LENGTH (sizeof(HIDE_ALL_GROUP_MEMBERS_ID) - 1)
#define SHOW_NO_TO_LINE_ID               "show no to line"
#define SHOW_NO_TO_LINE_ID_LENGTH        (sizeof(SHOW_NO_TO_LINE_ID) - 1)
#define MATCH_REMOTE_SIZE_ID             "match size"
#define MATCH_REMOTE_SIZE_ID_LENGTH      (sizeof(MATCH_REMOTE_SIZE_ID) - 1)
#define SILENT_NOT_LOCKED_FILE_ID        "silent not locked"
#define SILENT_NOT_LOCKED_FILE_ID_LENGTH (sizeof(SILENT_NOT_LOCKED_FILE_ID) - 1)
#define AGEING_ID                        "ageing"
#define AGEING_ID_LENGTH                 (sizeof(AGEING_ID) - 1)

/* Definitions for ageing. */
#define DEFAULT_AGEING                   5
#define MIN_AGEING_VALUE                 0
#define MAX_AGEING_VALUE                 9
#define AGEING_TABLE_LENGTH              10

/* Default definitions. */
#define AFD_CONFIG_FILE                  "/AFD_CONFIG"
#define AFD_CONFIG_FILE_LENGTH           (sizeof(AFD_CONFIG_FILE) - 1)
#define DEFAULT_DIR_CONFIG_FILE          "/DIR_CONFIG"
#define DEFAULT_HOST_CONFIG_FILE         "/HOST_CONFIG"
#define RENAME_RULE_FILE                 "/rename.rule"
#define RENAME_RULE_FILE_LENGTH          (sizeof(RENAME_RULE_FILE) - 1)
#define AFD_USER_FILE                    "/afd.users"
#define AFD_USER_FILE_LENGTH             (sizeof(AFD_USER_FILE) - 1)
#define GROUP_FILE                       "/group.list"
#define GROUP_FILE_LENGTH                (sizeof(GROUP_FILE) - 1)
#define ALIAS_NAME_FILE                  "/alias.list"
#define AFD_LOCAL_INTERFACE_FILE         "/local_interface.list"
#ifndef XOR_KEY
# define XOR_KEY_FILENAME                "/.xor.key"
#endif
#define DISABLED_DIR_FILE                "DISABLED_DIRS"
#define DEFAULT_FIFO_SIZE                4096
#define DEFAULT_BUFFER_SIZE              1024
#define DEFAULT_MAX_ERRORS               10
#define DEFAULT_SUCCESSFUL_RETRIES       10
#define DEFAULT_FILE_SIZE_OFFSET         -1
#define DEFAULT_TRANSFER_TIMEOUT         120L
#define DEFAULT_NO_OF_NO_BURSTS          0
#define DEFAULT_EXEC_TIMEOUT             0L
#ifdef WITH_DUP_CHECK
# define DEFAULT_DUPCHECK_TIMEOUT        3600L
#endif
#define DEFAULT_OLD_FILE_TIME            24      /* Hours.               */
#define DEFAULT_DIR_INFO_TIME            0L      /* Seconds (0 = unset)  */
#define DEFAULT_DIR_WARN_TIME            0L      /* Seconds (0 = unset)  */
#define DEFAULT_KEEP_CONNECTED_TIME      0       /* Seconds (0 = unset)  */
#define DEFAULT_CREATE_SOURCE_DIR_DEF    YES
#ifdef WITH_INOTIFY
# define DEFAULT_INOTIFY_FLAG            0
#endif
#define DEFAULT_HEARTBEAT_TIMEOUT        25L
#define DEFAULT_TRANSFER_MODE            'I'
#define DEFAULT_PROTOCOL_OPTIONS         FTP_PASSIVE_MODE
#define DEFAULT_PROTOCOL_OPTIONS2        0
#define DIR_ALIAS_OFFSET                 16
#ifdef _WITH_DE_MAIL_SUPPORT
# define DEFAULT_DE_MAIL_CONF_TIMEUP     172800L /* 2 days */
#endif
#ifdef ERROR_OFFLINE_FOR_NEW_HOST
# define DEFAULT_FSA_HOST_STATUS         HOST_ERROR_OFFLINE_STATIC
#else
# define DEFAULT_FSA_HOST_STATUS         0
#endif

/* Definitions to be read from the AFD_CONFIG file. */
#define AFD_TCP_PORT_DEF                 "AFD_TCP_PORT"
#define AFD_TLS_PORT_DEF                 "AFD_TLS_PORT"
#define AFD_TCP_LOGS_DEF                 "AFD_TCP_LOGS"
#ifdef _WITH_ATPD_SUPPORT
# define ATPD_TCP_PORT_DEF               "ATPD_TCP_PORT"
#endif
#define DEFAULT_PRINTER_CMD_DEF          "DEFAULT_PRINTER_CMD"
#define DEFAULT_PRINTER_NAME_DEF         "DEFAULT_PRINTER_NAME"
#define DEFAULT_AGE_LIMIT_DEF            "DEFAULT_AGE_LIMIT"
#define DEFAULT_AGEING_DEF               "DEFAULT_AGEING"
#define MAX_CONNECTIONS_DEF              "MAX_CONNECTIONS"
#define MAX_COPIED_FILES_DEF             "MAX_COPIED_FILES"
#define MAX_COPIED_FILE_SIZE_DEF         "MAX_COPIED_FILE_SIZE"
#define MAX_SHUTDOWN_TIME_DEF            "MAX_SHUTDOWN_TIME"
#define ONE_DIR_COPY_TIMEOUT_DEF         "ONE_DIR_COPY_TIMEOUT"
#define FULL_SCAN_TIMEOUT_DEF            "FULL_SCAN_TIMEOUT"
#define REMOTE_FILE_CHECK_INTERVAL_DEF   "REMOTE_FILE_CHECK_INTERVAL"
#ifdef WITH_INOTIFY
# define DEFAULT_INOTIFY_FLAG_DEF        "DEFAULT_INOTIFY_FLAG"
#endif
#ifndef _WITH_PTHREAD
# define DIR_CHECK_TIMEOUT_DEF           "DIR_CHECK_TIMEOUT"
#endif
#define TRUSTED_REMOTE_IP_DEF            "TRUSTED_REMOTE_IP"
#ifdef _WITH_ATPD_SUPPORT
# define ATPD_TRUSTED_REMOTE_IP_DEF      "ATPD_TRUSTED_REMOTE_IP"
#endif
#define ALDA_DAEMON_DEF                  "ALDA_DAEMON"
#define PING_CMD_DEF                     "PING_CMD"
#define TRACEROUTE_CMD_DEF               "TRACEROUTE_CMD"
#define DIR_CONFIG_NAME_DEF              "DIR_CONFIG_NAME"
#define FAKE_USER_DEF                    "FAKE_USER"
#define CREATE_SOURCE_DIR_DEF            "CREATE_SOURCE_DIR"
#define CREATE_REMOTE_SOURCE_DIR_DEF     "CREATE_REMOTE_SOURCE_DIR"
#define CREATE_SOURCE_DIR_MODE_DEF       "CREATE_SOURCE_DIR_MODE"
#define CREATE_TARGET_DIR_DEF            "CREATE_TARGET_DIR"
#define CREATE_TARGET_DIR_MODE_DEF       "CREATE_TARGET_DIR_MODE"
#define SIMULATE_SEND_MODE_DEF           "SIMULATE_SEND_MODE"
#define EXEC_TIMEOUT_DEF                 "EXEC_TIMEOUT"
#define DEFAULT_NO_PARALLEL_JOBS_DEF     "DEFAULT_NO_PARALLEL_JOBS"
#define DEFAULT_MAX_ERRORS_DEF           "DEFAULT_MAX_ERRORS"
#define DEFAULT_RETRY_INTERVAL_DEF       "DEFAULT_RETRY_INTERVAL"
#define DEFAULT_TRANSFER_BLOCKSIZE_DEF   "DEFAULT_TRANSFER_BLOCKSIZE"
#define DEFAULT_SUCCESSFUL_RETRIES_DEF   "DEFAULT_SUCCESSFUL_RETRIES"
#define DEFAULT_TRANSFER_TIMEOUT_DEF     "DEFAULT_TRANSFER_TIMEOUT"
#define DEFAULT_ERROR_OFFLINE_DEF        "DEFAULT_ERROR_OFFLINE"
#define DEFAULT_OLD_FILE_TIME_DEF        "DEFAULT_OLD_FILE_TIME"
#define DEFAULT_DELETE_FILES_FLAG_DEF    "DEFAULT_DELETE_FILES_FLAG"
#define DEFAULT_HTTP_PROXY_DEF           "DEFAULT_HTTP_PROXY"
#define DEFAULT_PRINT_SMTP_SERVER_DEF    "DEFAULT_PRINT_SMTP_SERVER"
#define DEFAULT_SMTP_SERVER_DEF          "DEFAULT_SMTP_SERVER"
#define DEFAULT_SMTP_FROM_DEF            "DEFAULT_SMTP_FROM"
#define DEFAULT_SMTP_REPLY_TO_DEF        "DEFAULT_SMTP_REPLY_TO"
#define DEFAULT_CHARSET_DEF              "DEFAULT_CHARSET"
#define DEFAULT_GROUP_MAIL_DOMAIN_DEF    "DEFAULT_GROUP_MAIL_DOMAIN"
#ifdef _WITH_DE_MAIL_SUPPORT
# define DEFAULT_DE_MAIL_SENDER_DEF      "DEFAULT_DE_MAIL_SENDER"
# define DEFAULT_DE_MAIL_CONF_TIMEUP_DEF "DEFAULT_DE_MAIL_CONF_TIMEUP"
#endif
#define REMOVE_UNUSED_HOSTS_DEF          "REMOVE_UNUSED_HOSTS"
#define DELETE_STALE_ERROR_JOBS_DEF      "DELETE_STALE_ERROR_JOBS"
#define DEFAULT_DIR_INFO_TIME_DEF        "DEFAULT_DIR_INFO_TIME"
#define DEFAULT_DIR_WARN_TIME_DEF        "DEFAULT_DIR_WARN_TIME"
#define VIEW_DATA_PROG_DEF               "VIEW_DATA_PROG"
#define VIEW_DATA_PROG_DEF_LENGTH        (sizeof(VIEW_DATA_PROG_DEF) - 1)
#define VIEW_DATA_NO_FILTER_PROG_DEF     "VIEW_DATA_NO_FILTER_PROG"
#define VIEW_DATA_NO_FILTER_PROG_DEF_LENGTH (sizeof(VIEW_DATA_NO_FILTER_PROG_DEF) - 1)
#define IN_GLOBAL_FILESYSTEM_DEF         "IN_GLOBAL_FILESYSTEM"
#define FORCE_REREAD_INTERVAL_DEF        "FORCE_REREAD_INTERVAL"
#define RENAME_RULE_NAME_DEF             "RENAME_RULE_NAME"
#ifdef HAVE_SETPRIORITY
# define AFDD_PRIORITY_DEF               "AFDD_PRIORITY"
# define AFDDS_PRIORITY_DEF              "AFDDS_PRIORITY"
# ifdef _WITH_ATPD_SUPPORT
#  define ATPD_PRIORITY_DEF              "ATPD_PRIORITY"
# endif
# ifdef _WITH_WMOD_SUPPORT
#  define WMOD_PRIORITY_DEF              "WMOD_PRIORITY"
# endif
# ifdef _WITH_DE_MAIL_SUPPORT
#  define DEMCD_PRIORITY_DEF             "DEMCD_PRIORITY"
# endif
# define AFD_STAT_PRIORITY_DEF           "AFD_STAT_PRIORITY"
# define AMG_PRIORITY_DEF                "AMG_PRIORITY"
# define ARCHIVE_WATCH_PRIORITY_DEF      "ARCHIVE_WATCH_PRIORITY"
# define EXEC_BASE_PRIORITY_DEF          "EXEC_BASE_PRIORITY"
# define FD_PRIORITY_DEF                 "FD_PRIORITY"
# define INIT_AFD_PRIORITY_DEF           "INIT_AFD_PRIORITY"
# define SHOW_LOG_PRIORITY_DEF           "SHOW_LOG_PRIORITY"
# define ADD_AFD_PRIORITY_DEF            "ADD_AFD_PRIORITY"
# define DEFAULT_ADD_AFD_PRIORITY_DEF    NO
# define MAX_NICE_VALUE_DEF              "MAX_NICE_VALUE"
# define MIN_NICE_VALUE_DEF              "MIN_NICE_VALUE"
# define DEFAULT_MAX_NICE_VALUE          0
# define DEFAULT_MIN_NICE_VALUE          19
#endif
#define BUL_RULE_FILE_NAME_DEF           "BUL_RULE_FILE_NAME"
#define REP_RULE_FILE_NAME_DEF           "REP_RULE_FILE_NAME"
#define GF_FORCE_DISCONNECT_DEF          "GF_FORCE_DISCONNECT"
#define SF_FORCE_DISCONNECT_DEF          "SF_FORCE_DISCONNECT"
#ifdef MULTI_FS_SUPPORT
# define EXTRA_WORK_DIR_DEF              "EXTRA_WORK_DIR"
#endif
#define ADDITIONAL_LOCKED_FILES_DEF      "ADDITIONAL_LOCKED_FILES"
#ifdef _WITH_DE_MAIL_SUPPORT
# define DE_MAIL_RESPONSE_FILE_DEF       "DE_MAIL_RESPONSE_FILE"
#endif


/* Heading identifiers for the DIR_CONFIG file and messages. */
#define DIR_IDENTIFIER                   "[directory]"
#define DIR_IDENTIFIER_LENGTH            (sizeof(DIR_IDENTIFIER) - 1)
#define DIR_OPTION_IDENTIFIER            "[dir options]"
#define DIR_OPTION_IDENTIFIER_LENGTH     (sizeof(DIR_OPTION_IDENTIFIER) - 1)
#define FILE_IDENTIFIER                  "[files]"
#define FILE_IDENTIFIER_LENGTH           (sizeof(FILE_IDENTIFIER) - 1)
#define DESTINATION_IDENTIFIER           "[destination]"
#define DESTINATION_IDENTIFIER_LENGTH    (sizeof(DESTINATION_IDENTIFIER) - 1)
#define RECIPIENT_IDENTIFIER             "[recipient]"
#define RECIPIENT_IDENTIFIER_LENGTH      (sizeof(RECIPIENT_IDENTIFIER) - 1)
#define OPTION_IDENTIFIER                "[options]"
#define OPTION_IDENTIFIER_LENGTH         (sizeof(OPTION_IDENTIFIER) - 1)

#define VIEW_DC_DIR_IDENTIFIER           "Directory     : "
#define VIEW_DC_DIR_IDENTIFIER_LENGTH    (sizeof(VIEW_DC_DIR_IDENTIFIER) - 1)

#ifdef _DISTRIBUTION_LOG
/* Definitions for the different distribution log types. */
# define NORMAL_DIS_TYPE                 0
# define TIME_JOB_DIS_TYPE               1
# define QUEUE_STOPPED_DIS_TYPE          2
# define DISABLED_DIS_TYPE               3
# define AGE_LIMIT_DELETE_DIS_TYPE       4
# ifdef WITH_DUP_CHECK
#  define DUPCHECK_DIS_TYPE              5
#  define NO_OF_DISTRIBUTION_TYPES       6
# else
#  define NO_OF_DISTRIBUTION_TYPES       5
# endif
# define DONE_DIS_TYPE                   254
# define ERROR_DIS_TYPE                  255
#endif

/* Definitions for AFDD Logs. */
/* NOTE: Bits 1-4, 24 are defined in afd_mon/mondefs.h */
#define AFDD_SYSTEM_LOG                  16     /*  5 */
#define AFDD_RECEIVE_LOG                 32     /*  6 */
#define AFDD_TRANSFER_LOG                64     /*  7 */
#define AFDD_TRANSFER_DEBUG_LOG          128    /*  8 */
#define AFDD_INPUT_LOG                   256    /*  9 */
#define AFDD_PRODUCTION_LOG              512    /* 10 */
#define AFDD_OUTPUT_LOG                  1024   /* 11 */
#define AFDD_DELETE_LOG                  2048   /* 12 */
#define AFDD_JOB_DATA                    4096   /* 13 */
#define AFDD_COMPRESSION_1               8192   /* 14 */
#define AFDD_EVENT_LOG                   16384  /* 15 */
#define AFDD_DISTRIBUTION_LOG            32768  /* 16 */
#define AFDD_CONFIRMATION_LOG            65536  /* 17 */
/* NOTE: If new flags are added check afd_mon/mondefs.h first! */

/* Group identifier for mails. */
#define MAIL_GROUP_IDENTIFIER        '$'

/* Group identifier for either recipient, file or directory groups. */
#define GROUP_SIGN                   '&'

/* Length of log date in log files. */
#define LOG_DATE_LENGTH              10

/* Definitions of maximum values. */
#define MAX_GROUPNAME_LENGTH         65     /* The maximum length a group   */
                                            /* name (GROUP_SIGN) may have.  */
#define MAX_SHUTDOWN_TIME           600     /* When the AMG gets the order  */
                                            /* shutdown, this the time it   */
                                            /* waits for its children to    */
                                            /* return before they get       */
                                            /* eliminated.                  */
#define MIN_SHUTDOWN_TIME            50     /* Ths time is subtracted from  */
                                            /* MAX_SHUTDOWN_TIME. So this   */
                                            /* value may not be higher.     */
#define MAX_RENAME_RULE_FILES        20     /* Maximum number of rename rule*/
                                            /* files that may be configured */
                                            /* in AFD_CONFIG.               */
#define MAX_REAL_HOSTNAME_LENGTH_STR "MAX_REAL_HOSTNAME_LENGTH"
#define MAX_PROXY_NAME_LENGTH        80     /* The maximum length of the    */
                                            /* remote proxy name.           */
#define MAX_PROXY_NAME_LENGTH_STR    "MAX_PROXY_NAME_LENGTH"
#define MAX_ADD_FNL                  35     /* Maximum additional file name */
                                            /* length:                      */
                                            /* <creation_time>_<unique_no>_<split_job_counter>_ */
                                            /* 16 + 1 + 8 + 1 + 8 + 1        */
#ifdef MULTI_FS_SUPPORT
# define MAX_MSG_NAME_LENGTH         (MAX_ADD_FNL + 28) /* Maximum length   */
                                            /* of message name.             */
                                            /* <dev_id>/<job_id>/<counter>/<creation_time>_<unique_no>_<split_job_counter>_ */
                                            /* 8 + 1 + 8 + 1 + 8 + 1 + 16 + 1 + 8 + 1 + 8 + 1 + 1 */
#else
# define MAX_MSG_NAME_LENGTH         (MAX_ADD_FNL + 19) /* Maximum length   */
                                            /* of message name.             */
                                            /* <job_id>/<counter>/<creation_time>_<unique_no>_<split_job_counter>_ */
                                            /* 8 + 1 + 8 + 1 + 16 + 1 + 8 + 1 + 8 + 1 + 1 */
#endif
#define MAX_MSG_NAME_LENGTH_STR      "MAX_MSG_NAME_LENGTH"
#define MAX_DOUBLE_LENGTH            (3 + DBL_MANT_DIG - DBL_MIN_EXP)
#define MAX_INT_LENGTH               11     /* When storing integer values  */
                                            /* as string this is the no.    */
                                            /* characters needed to store   */
                                            /* the largest integer value.   */
#define MAX_INT_HEX_LENGTH            9     /* Ditto, just using hex values */
                                            /* here.                        */
#define MAX_INT_OCT_LENGTH           12     /* Ditto, just using octal      */
                                            /* values.                      */
#if SIZEOF_SHORT == 2
# define MAX_SHORT_LENGTH             6
#else
# define MAX_SHORT_LENGTH            11
#endif
#define MAX_CHAR_HEX_LENGTH           3
#if SIZEOF_LONG == 4
# define MAX_LONG_LENGTH             11
# define MAX_LONG_HEX_LENGTH          9
#else
# define MAX_LONG_LENGTH             21
# define MAX_LONG_HEX_LENGTH         17
#endif
#define MAX_LONG_LONG_LENGTH         21
#define MAX_LONG_LONG_HEX_LENGTH     17
#if SIZEOF_OFF_T == 4
# define MAX_OFF_T_LENGTH            11
# define MAX_OFF_T_HEX_LENGTH         9
#else
# define MAX_OFF_T_LENGTH            21
# define MAX_OFF_T_HEX_LENGTH        17
#endif
#if SIZEOF_TIME_T == 4
# define MAX_TIME_T_LENGTH           11
# define MAX_TIME_T_HEX_LENGTH        9
#else
# define MAX_TIME_T_LENGTH           21
# define MAX_TIME_T_HEX_LENGTH       17
#endif
#if SIZEOF_PID_T == 4
# define MAX_PID_T_LENGTH            11
# define MAX_PID_T_HEX_LENGTH         9
#else
# define MAX_PID_T_LENGTH            21
# define MAX_PID_T_HEX_LENGTH        17
#endif
#define MAX_TOGGLE_STR_LENGTH        5
#define MAX_TOGGLE_STR_LENGTH_STR    "MAX_TOGGLE_STR_LENGTH"
#define MAX_USER_NAME_LENGTH         80     /* Maximum length of the user   */
                                            /* name and password.           */
#define MAX_USER_NAME_LENGTH_STR     "MAX_USER_NAME_LENGTH"
#define MAX_PROFILE_NAME_LENGTH      40     /* Maximum length og the profile*/
                                            /* name.                        */
#define MAX_FULL_USER_ID_LENGTH      (MAX_PROFILE_NAME_LENGTH + 80)
                                            /* Max length for user name and */
                                            /* display.                     */
#define MAX_COPIED_FILES             100    /* The maximum number of files  */
                                            /* that the AMG may collect     */
                                            /* before it creates a message  */
                                            /* for the FD.                  */
#define MAX_FILE_BUFFER_SIZE         51200  /* The maximum number that can  */
                                            /* be handled in one directory  */
                                            /* scan.                        */
#define MAX_COPIED_FILE_SIZE         102400 /* Same as above only that this */
                                            /* limits the total size copied */
                                            /* in Kilobytes.                */
#define MAX_COPIED_FILE_SIZE_UNIT    1024   /* Unit in Kilobytes for above  */
                                            /* value.                       */
#define MAX_MSG_PER_SEC              65535  /* The maximum number of        */
                                            /* messages that may be         */
                                            /* generated in one second.     */
#define MAX_WMO_COUNTER              999
#define MAX_PRODUCTION_BUFFER_LENGTH 8192   /* Buffer size to hold job ID   */
                                            /* and new file names after a   */
                                            /* rename, exec, etc.           */
#define MAX_EVENT_REASON_LENGTH      2048   /* The maximum length of the    */
                                            /* event reason.                */
#ifndef MAX_NO_PARALLEL_JOBS
# ifdef AFDBENCH_CONFIG
#  define MAX_NO_PARALLEL_JOBS       12
# else
#  define MAX_NO_PARALLEL_JOBS       9      /* Maximum number of parallel   */
                                            /* jobs per host alias.         */
# endif
#endif
#define MAX_NO_PARALLEL_JOBS_STR     "MAX_NO_PARALLEL_JOBS"
#define MAX_FILENAME_LENGTH          256    /* Maximum length of a filename.*/
#define MAX_FILENAME_LENGTH_STR      "MAX_FILENAME_LENGTH"
#define MAX_ERROR_STR_LENGTH         35     /* Maximum length of the FD     */
                                            /* error strings. (see fddefs.h)*/
#define MAX_IP_LENGTH                16     /* Maximum length of an IP      */
                                            /* number as string.            */
#define MAX_UDC_RESPONCE_LENGTH      (SIZEOF_INT + SIZEOF_INT + SIZEOF_INT + SIZEOF_INT)
                                            /* The maximum responce length  */
                                            /* when DIR_CONFIG is updated.  */
#define MAX_UHC_RESPONCE_LENGTH      (SIZEOF_INT + SIZEOF_INT)
                                            /* The maximum responce length  */
                                            /* when HOST_CONFIG is updated. */
#define MAX_UPDATE_REPLY_STR_LENGTH   256   /* The maximum length that a    */
                                            /* string may be long from a    */
                                            /* responce when updating       */
                                            /* configuration.               */
#define MAX_ALIAS_NAME_LENGTH        16
#define MAX_MAIL_ID_LENGTH           17     /* The maximum length of the    */
                                            /* mail queue ID returned by    */
                                            /* the SMTP server.             */
#define MAX_EXEC_FILE_SUBSTITUTION   10     /* The maximum number of %s     */
                                            /* substitutions that may be in */
                                            /* the exec or pexec option or  */
                                            /* scheme.                      */
#define MAX_ADD_LOCKED_FILES_LENGTH  4096   /* Maximum length of all        */
                                            /* additional locked files set  */
                                            /* via ADDITIONAL_LOCKED_FILES  */
                                            /* in AFD_CONFIG.               */
#define MAX_TTAAii_HEADER_LENGTH     22     /* The maximum WMO header length*/
                                            /* TTAAii_CCCC_YYGGgg[_BBB].    */
#define MAX_TIMEZONE_LENGTH          32     /* The maximum length of a time */
                                            /* zone entry.                  */
#define MAX_TIMEZONE_LENGTH_STR      "MAX_TIMEZONE_LENGTH"

/* String names for certain definitions. */
#define MAX_HOSTNAME_LENGTH_STR      "MAX_HOSTNAME_LENGTH"
#define MAX_AFDNAME_LENGTH_STR       "MAX_AFDNAME_LENGTH"
#define MAX_DIR_ALIAS_LENGTH_STR     "MAX_DIR_ALIAS_LENGTH"
#define MAX_RECIPIENT_LENGTH_STR     "MAX_RECIPIENT_LENGTH"
#define MAX_OPTION_LENGTH_STR        "MAX_OPTION_LENGTH"
#define MAX_PATH_LENGTH_STR          "MAX_PATH_LENGTH"
#ifdef WITH_IP_DB
# define MAX_AFD_INET_ADDRSTRLEN     46
#endif

/* The length of message we send via fifo from AMG to FD. */
#ifdef MULTI_FS_SUPPORT
# define MAX_BIN_MSG_LENGTH (sizeof(time_t) + sizeof(dev_t) + sizeof(unsigned int) + sizeof(unsigned int) + sizeof(unsigned int) + sizeof(off_t) + sizeof(unsigned int) + sizeof(unsigned short) + sizeof(char) + sizeof(char))
#else
# define MAX_BIN_MSG_LENGTH (sizeof(time_t) + sizeof(unsigned int) + sizeof(unsigned int) + sizeof(unsigned int) + sizeof(off_t) + sizeof(unsigned int) + sizeof(unsigned short) + sizeof(char) + sizeof(char))
#endif

/* Miscellaneous definitions. */
#define LOG_SIGN_POSITION          13     /* Position in log file where   */
                                          /* type of log entry can be     */
                                          /* determined (I, W, E, F, D).  */
#define LOG_FIFO_SIZE              5      /* The number of letters        */
                                          /* defining the type of log.    */
                                          /* These are displayed in the   */
                                          /* button line.                 */
#define ERROR_HISTORY_LENGTH       5      /* The last five error types    */
                                          /* during transmission.         */
                                          /* NOTE: This value MUST be at  */
                                          /*       least 2!               */
#define ERROR_HISTORY_LENGTH_STR   "ERROR_HISTORY_LENGTH"
#ifndef DEFAULT_ARCHIVE_UNIT
# define DEFAULT_ARCHIVE_UNIT      86400  /* Seconds => 1 day             */
#endif
#define WD_ENV_NAME                "AFD_WORK_DIR"   /* The working dir-   */
                                                    /* ectory environment */
#define WAIT_AFD_STATUS_ATTACH     80

/* Different host status. */
#define STOP_TRANSFER_STAT         1
#define PAUSE_QUEUE_STAT           2
#define AUTO_PAUSE_QUEUE_STAT      4
#define DANGER_PAUSE_QUEUE_STAT    8
#define HOST_ERROR_OFFLINE_STATIC  16
#define HOST_CONFIG_HOST_DISABLED  32
/* NOTE: HOST_TWO_FLAG             64 */
#ifdef WITH_ERROR_QUEUE
# define ERROR_QUEUE_SET           128
#endif
#define PENDING_ERRORS             256
#define HOST_ERROR_ACKNOWLEDGED    512
#define HOST_ERROR_OFFLINE         1024
#define HOST_ERROR_ACKNOWLEDGED_T  2048
#define HOST_ERROR_OFFLINE_T       4096
#define HOST_ERROR_EA_STATIC       8192  /* Host error event action static. Not used! */
#define HOST_WARN_TIME_REACHED     16384
#define DO_NOT_DELETE_DATA         32768
#define HOST_ACTION_SUCCESS        65536
#ifdef WITH_IP_DB
# define STORE_IP                  131072
#endif
#define SIMULATE_SEND_MODE         262144
#define ERROR_HOSTS_IN_GROUP       524288
#define WARN_HOSTS_IN_GROUP        1048576

#define EVENT_STATUS_STATIC_FLAGS  (PENDING_ERRORS | HOST_ERROR_ACKNOWLEDGED | HOST_ERROR_OFFLINE)
#define EVENT_STATUS_FLAGS         (EVENT_STATUS_STATIC_FLAGS | HOST_ERROR_ACKNOWLEDGED_T | HOST_ERROR_OFFLINE_T)

#define HOST_NOT_IN_DIR_CONFIG     4

/* Position of each colour in global array. */
/*############################ LightBlue1 ###############################*/
#define HTML_COLOR_0               "#BFEFFF"
#define DEFAULT_BG                 0  /* Background                      */
#define HTTP_ACTIVE                0
#define NORMAL_MODE                0
/*############################## White ##################################*/
#define HTML_COLOR_1               "#FFFFFF"
#define WHITE                      1
#define DISCONNECT                 1  /* Successful completion of        */
                                      /* operation and disconnected.     */
#define DISABLED                   1
#define NO_INFORMATION             1
/*########################### lightskyblue2 #############################*/
#define HTML_COLOR_2               "#A4D3EE"
#define CHAR_BACKGROUND            2  /* Background color for characters.*/
#define DISCONNECTED               2  /* AFD_MON not connected.          */
#define CLOSING_CONNECTION         2  /* Closing an active connection.   */
/*############################ SaddleBrown ##############################*/
#define HTML_COLOR_3               "#8B4513"
#define PAUSE_QUEUE                3
#ifdef _WITH_SCP_SUPPORT
# define SCP_ACTIVE                3
#endif
/*############################## brown3 #################################*/
#define HTML_COLOR_4               "#CD3333"
#define AUTO_PAUSE_QUEUE           4
#ifdef _WITH_SCP_SUPPORT
# define SCP_BURST_TRANSFER_ACTIVE 4
#endif
/*############################### Blue ##################################*/
#define HTML_COLOR_5               "#0000FF"
#define CONNECTING                 5  /* Open connection to remote host, */
                                      /* sending user and password,      */
                                      /* setting transfer type and       */
                                      /* changing directory.             */
#define LOC_BURST_TRANSFER_ACTIVE  5
#define EXEC_BURST_TRANSFER_ACTIVE 5
#define SIMULATE_MODE              5
/*############################## gray37 #################################*/
#define HTML_COLOR_6               "#5E5E5E"
#define LOCKED_INVERSE             6
#define HTTP_RETRIEVE_ACTIVE       6
#define EXEC_RETRIEVE_ACTIVE       6
/*############################### gold ##################################*/
#define HTML_COLOR_7               "#FFD700"
#define TR_BAR                     7  /* Colour for transfer rate bar.   */
#define DEBUG_MODE                 7
#ifdef _WITH_WMO_SUPPORT
# define WMO_ACTIVE                7
#endif
/*########################### NavajoWhite1 ##############################*/
#define HTML_COLOR_8               "#FFDEAD"
#define LABEL_BG                   8  /* Background for label.           */
#ifdef _WITH_MAP_SUPPORT
# define MAP_ACTIVE                8
#endif
#ifdef _WITH_DFAX_SUPPORT
# define DFAX_ACTIVE               8
#endif
#define SFTP_ACTIVE                8
/*############################ SteelBlue1 ###############################*/
#define HTML_COLOR_9               "#63B8FF"
#define BUTTON_BACKGROUND          9  /* Background for button line in   */
                                      /* afd_ctrl dialog.                */
#define LOC_ACTIVE                 9
#define EXEC_ACTIVE                9
#define ERROR_OFFLINE_ID           9
/*############################### pink ##################################*/
#define HTML_COLOR_10              "#FFC0CB"
#define SMTP_ACTIVE                10
#define ERROR_ACKNOWLEDGED_ID      10
/*############################## green ##################################*/
#define HTML_COLOR_11              "#00FF00"
#define FTP_BURST2_TRANSFER_ACTIVE 11
/*############################## green3 #################################*/
#define HTML_COLOR_12              "#00CD00"
#define CONNECTION_ESTABLISHED     12 /* AFD_MON                         */
#define NORMAL_STATUS              12
#define INFO_ID                    12
#define FTP_RETRIEVE_ACTIVE        12 /* When gf_ftp retrieves files.    */
/*############################# SeaGreen ################################*/
#define HTML_COLOR_13              "#2E8B57"
#define CONFIG_ID                  13
#define TRANSFER_ACTIVE            13 /* Creating remote lockfile and    */
                                      /* transferring files.             */
#define FTP_ACTIVE                 13
#define DIRECTORY_ACTIVE           13
/*############################ DarkOrange ###############################*/
#define HTML_COLOR_14              "#FF8C00"
#define STOP_TRANSFER              14 /* Transfer to this host is        */
                                      /* stopped.                        */
#ifdef WITH_ERROR_QUEUE
# define JOBS_IN_ERROR_QUEUE       14
#endif
#define WARNING_ID                 14
#define TRACE_MODE                 14
#ifdef _WITH_TRANS_EXEC
# define POST_EXEC                 14
#endif
/*############################## tomato #################################*/
#define HTML_COLOR_15              "#FF6347"
#define NOT_WORKING                15
/*################################ Red ##################################*/
#define HTML_COLOR_16              "#FF0000"
#define NOT_WORKING2               16
#define FULL_TRACE_MODE            16
#define ERROR_ID                   16
#define CONNECTION_DEFUNCT         16 /* AFD_MON, connection not         */
                                      /* working.                        */
/*############################### Black #################################*/
#define HTML_COLOR_17              "#000000"
#define BLACK                      17
#define FG                         17 /* Foreground                      */
#define FAULTY_ID                  17
/*########################## BlanchedAlmond #############################*/
#define HTML_COLOR_18              "#FFEBCD"
#define SFTP_BURST_TRANSFER_ACTIVE 18
/*############################### cyan ##################################*/
#define HTML_COLOR_19              "#00FFFF"
#define SMTP_BURST_TRANSFER_ACTIVE 19
/*############################### peru ##################################*/
#define HTML_COLOR_20              "#CD853F"
#define SFTP_RETRIEVE_ACTIVE       20
/*############################## yellow #################################*/
#ifdef _WITH_WMO_SUPPORT
# define HTML_COLOR_21             "#FFFF00"
# define WMO_BURST_TRANSFER_ACTIVE 21
# define COLOR_POOL_SIZE           22
#else
# define COLOR_POOL_SIZE           21
#endif

/* History types. */
#define RECEIVE_HISTORY            0
#define SYSTEM_HISTORY             1
#define TRANSFER_HISTORY           2
#define NO_OF_LOG_HISTORY          3

/* Error action types. */
#define HOST_SUCCESS_ACTION        0
#define HOST_WARN_ACTION           1
#define HOST_ERROR_ACTION          2
#define DIR_SUCCESS_ACTION         3
#define DIR_WARN_ACTION            4
#define DIR_ERROR_ACTION           5
#define DIR_INFO_ACTION            6

/* Directory definitions. */
#define AFD_MSG_DIR                   "/messages"
#ifdef WITH_ONETIME
# define AFD_ONETIME_DIR              "/onetime"
# define AFD_ONETIME_DIR_LENGTH       (sizeof(AFD_ONETIME_DIR) - 1)
# define AFD_LIST_DIR                 "/list"
# define AFD_CONFIG_DIR               "/config"
# define AFD_CONFIG_DIR_LENGTH        (sizeof(AFD_CONFIG_DIR) - 1)
#endif
#define AFD_FILE_DIR                  "/files"
#define AFD_FILE_DIR_LENGTH           (sizeof(AFD_FILE_DIR) - 1)
#define AFD_TMP_DIR                   "/pool"
#define AFD_TMP_DIR_LENGTH            (sizeof(AFD_TMP_DIR) - 1)
#define AFD_TIME_DIR                  "/time"
#define AFD_TIME_DIR_LENGTH           (sizeof(AFD_TIME_DIR) - 1)
#define AFD_ARCHIVE_DIR               "/archive"
#define AFD_ARCHIVE_DIR_LENGTH        (sizeof(AFD_ARCHIVE_DIR) - 1)
#define FIFO_DIR                      "/fifodir"
#define FIFO_DIR_LENGTH               (sizeof(FIFO_DIR) - 1)
#define LOG_DIR                       "/log"
#define LOG_DIR_LENGTH                (sizeof(LOG_DIR) - 1)
#define RLOG_DIR                      "/rlog"  /* Only used for afd_mon. */
#define ETC_DIR                       "/etc"
#define ETC_DIR_LENGTH                (sizeof(ETC_DIR) - 1)
#define INFO_DIR                      "/info"
#define INFO_DIR_LENGTH               (sizeof(INFO_DIR) - 1)
#define ACTION_DIR                    "/action"
#define ACTION_DIR_LENGTH             (sizeof(ACTION_DIR) - 1)
#define ACTION_TARGET_DIR             "/target"
#define ACTION_TARGET_DIR_LENGTH      (sizeof(ACTION_TARGET_DIR) - 1)
#define ACTION_SOURCE_DIR             "/source"
#define ACTION_SOURCE_DIR_LENGTH      (sizeof(ACTION_SOURCE_DIR) - 1)
#define ACTION_ERROR_DIR              "/error"
#define ACTION_WARN_DIR               "/warn"
#define ACTION_INFO_DIR               "/info"
#define ACTION_SUCCESS_DIR            "/success"
#define MAIL_HEADER_DIR               "/mail_header"
#define GROUP_NAME_DIR                "/groups"
#define GROUP_NAME_DIR_LENGTH         (sizeof(GROUP_NAME_DIR) - 1)
#define SOURCE_GROUP_NAME             "/source"
#define SOURCE_GROUP_NAME_LENGTH      (sizeof(SOURCE_GROUP_NAME) - 1)
#define RECIPIENT_GROUP_NAME          "/recipient"
#define RECIPIENT_GROUP_NAME_LENGTH   (sizeof(RECIPIENT_GROUP_NAME) - 1)
#define FILE_GROUP_NAME               "/files"
#define FILE_GROUP_NAME_LENGTH        (sizeof(FILE_GROUP_NAME) - 1)
#define INCOMING_DIR                  "/incoming"
#define INCOMING_DIR_LENGTH           (sizeof(INCOMING_DIR) - 1)
#define OUTGOING_DIR                  "/outgoing"
#define OUTGOING_DIR_LENGTH           (sizeof(OUTGOING_DIR) - 1)
#define STORE_DIR                     "/store"
#ifdef WITH_DUP_CHECK
# define CRC_DIR                      "/crc"
# define CRC_DIR_LENGTH               (sizeof(CRC_DIR) - 1)
#endif
#define FILE_MASK_DIR                 "/file_mask"
#define LS_DATA_DIR                   "/ls_data"
#define LS_DATA_DIR_LENGTH            (sizeof(LS_DATA_DIR) - 1)

/*-----------------------------------------------------------------------*/
/* If definitions are added or removed, update init_afd/afd.c!           */
/*-----------------------------------------------------------------------*/
/* Data file definitions. */
#define FSA_ID_FILE                  "/fsa.id"
#define FSA_STAT_FILE                "/fsa_status"
#define FSA_STAT_FILE_ALL            "/fsa_status.*"
#define FRA_ID_FILE                  "/fra.id"
#define FRA_STAT_FILE                "/fra_status"
#define FRA_STAT_FILE_ALL            "/fra_status.*"
#define AFD_STATUS_FILE              "afd.status"
#define AFD_STATUS_FILE_LENGTH       (sizeof(AFD_STATUS_FILE) - 1)
#define AFD_STATUS_FILE_ALL          "/afd.status.*"
#define AFDCFG_RECOVER               "/afdcfg.recover"
#define NNN_FILE                     "/nnn"
#define NNN_ASSEMBLE_FILE            "/nnn.assemble"
#define NNN_FILE_ALL                 "/nnn.*"
#define BLOCK_FILE                   "/NO_AUTO_RESTART"
#define AMG_COUNTER_FILE             "/amg_counter"
#define COUNTER_FILE                 "/any_counter"
#define MESSAGE_BUF_FILE             "/tmp_msg_buffer"
#define MSG_CACHE_FILE               "/fd_msg_cache"
#define MSG_CACHE_FILE_LENGTH        (sizeof(MSG_CACHE_FILE) - 1)
#define MSG_QUEUE_FILE               "/fd_msg_queue"
#define MSG_QUEUE_FILE_LENGTH        (sizeof(MSG_QUEUE_FILE) - 1)
#ifdef SF_BURST_ACK
# define ACK_QUEUE_FILE              "/fd_ack_queue"
# define ACK_QUEUE_FILE_LENGTH       (sizeof(ACK_QUEUE_FILE) - 1)
#endif
#ifdef WITH_ERROR_QUEUE
# define ERROR_QUEUE_FILE            "/error_queue"
#endif
#define FILE_MASK_FILE               "/file_masks"
#define FILE_MASK_FILE_LENGTH        (sizeof(FILE_MASK_FILE) - 1)
#define DC_LIST_FILE                 "/dc_name_data"
#define DC_LIST_FILE_LENGTH          (sizeof(DC_LIST_FILE) - 1)
#define DIR_NAME_FILE                "/directory_names"
#define DIR_NAME_FILE_LENGTH         (sizeof(DIR_NAME_FILE) - 1)
#define JOB_ID_DATA_FILE             "/job_id_data"
#define DCPL_FILE_NAME               "/dcpl_data"  /* dir_check process list */
#ifdef WITH_ONETIME
# define OTPL_FILE_NAME              "/otpl_data"
#endif
#define PWB_DATA_FILE                "/pwb_data"
#define CURRENT_MSG_LIST_FILE        "/current_job_id_list"
#define CURRENT_MSG_LIST_FILE_LENGTH (sizeof(CURRENT_MSG_LIST_FILE) - 1)
#define AMG_DATA_FILE                "/amg_data"
#define AMG_DATA_FILE_TMP            "/amg_data.tmp"
#ifdef WITH_ONETIME
# define AMG_ONETIME_DATA_FILE       "/amg_data_onetime"
#endif
#define TYPESIZE_DATA_FILE           "/typesize_data"
#define SYSTEM_DATA_FILE             "/system_data"
#define ALTERNATE_FILE               "/alternate."
#define ALTERNATE_FILE_ALL           "/alternate.*"
#define LOCK_PROC_FILE               "/LOCK_FILE"
#define AFD_ACTIVE_FILE              "/AFD_ACTIVE"
#define WINDOW_ID_FILE               "/window_ids" /* No longer used, but keep */
                                                   /* it so that it gets       */
                                                   /* deleted by afd -i or -I. */
#define DEFAULT_ACTION_FILE          "all.default"
#ifdef WITH_IP_DB
# define IP_DB_FILE                  "/ip_data"
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
# define DEMCD_QUEUE_FILE            "/demcd_queue"
#endif
#define JIS_FILE                     "/jis_data"
#define DB_UPDATE_REPLY_DEBUG_FILE   "/db_update_reply_debug"
#define ENVIRONMENT_VARIABLES_SET    "environment_variables_set.txt"

/* Definitions of fifo names. */
#define SYSTEM_LOG_FIFO              "/system_log.fifo"
#ifdef _MAINTAINER_LOG
# define MAINTAINER_LOG_FIFO         "/maintainer_log.fifo"
#endif
#define EVENT_LOG_FIFO               "/event_log.fifo"
#define RECEIVE_LOG_FIFO             "/receive_log.fifo"
#define TRANSFER_LOG_FIFO            "/transfer_log.fifo"
#define TRANS_DEBUG_LOG_FIFO         "/trans_db_log.fifo"
#define MON_LOG_FIFO                 "/monitor_log.fifo"
#define AFD_CMD_FIFO                 "/afd_cmd.fifo"
#define AFD_RESP_FIFO                "/afd_resp.fifo"
#define AFD_WORKER_CMD_FIFO          "/afd_worker_cmd.fifo"
#define AMG_CMD_FIFO                 "/amg_cmd.fifo"
#define DB_UPDATE_FIFO               "/db_update.fifo"
#define DB_UPDATE_REPLY_FIFO         "/db_update_reply.fifo."
#define DB_UPDATE_REPLY_FIFO_ALL     "/db_update_reply.fifo.*"
#define FD_CMD_FIFO                  "/fd_cmd.fifo"
#define AW_CMD_FIFO                  "/aw_cmd.fifo"
#define IP_FIN_FIFO                  "/ip_fin.fifo"
#ifdef WITH_ONETIME
# define OT_FIN_FIFO                 "/ot_fin.fifo"
#endif
#define SF_FIN_FIFO                  "/sf_fin.fifo"
#ifdef SF_BURST_ACK
# define SF_BURST_ACK_FIFO           "/sf_burst_ack.fifo"
#endif
#define RETRY_FD_FIFO                "/retry_fd.fifo"
#define FD_DELETE_FIFO               "/fd_delete.fifo"
#define FD_WAKE_UP_FIFO              "/fd_wake_up.fifo"
#define TRL_CALC_FIFO                "/trl_calc.fifo"
#define QUEUE_LIST_READY_FIFO        "/queue_list_ready.fifo"
#define QUEUE_LIST_DONE_FIFO         "/queue_list_done.fifo"
#define PROBE_ONLY_FIFO              "/probe_only.fifo"
#ifdef _INPUT_LOG
# define INPUT_LOG_FIFO              "/input_log.fifo"
#endif
#ifdef _DISTRIBUTION_LOG
# define DISTRIBUTION_LOG_FIFO       "/distribution_log.fifo"
#endif
#ifdef _OUTPUT_LOG
# define OUTPUT_LOG_FIFO             "/output_log.fifo"
#endif
#ifdef _CONFIRMATION_LOG
# define CONFIRMATION_LOG_FIFO       "/confirmation_log.fifo"
#endif
#ifdef _DELETE_LOG
# define DELETE_LOG_FIFO             "/delete_log.fifo"
#endif
#ifdef _PRODUCTION_LOG
# define PRODUCTION_LOG_FIFO         "/production_log.fifo"
#endif
#define RETRY_MON_FIFO               "/retry_mon.fifo."
#define DEL_TIME_JOB_FIFO            "/del_time_job.fifo"
#define MSG_FIFO                     "/msg.fifo"
#define AFDD_LOG_FIFO                "/afdd_log.fifo"
#define AFDDS_LOG_FIFO               "/afdds_log.fifo"
#ifdef _WITH_DE_MAIL_SUPPORT
# define DEMCD_FIFO                  "/demcd.fifo"
# define DEMCD_FIFO_LENGTH           (sizeof(DEMCD_FIFO) - 1)
#endif
/*-----------------------------------------------------------------------*/

/* Definitions for the AFD name. */
#define AFD_NAME                   "afd.name"
#define MAX_AFD_NAME_LENGTH        30

#define MSG_CACHE_BUF_SIZE         10000

/* Definitions for the different actions over fifos. */
#define HALT                       0
#define STOP                       1
#define START                      2
#define SAVE_STOP                  3
#define QUICK_STOP                 4
#define ACKN                       5
#define NEW_DATA                   6    /* Used by AMG-DB-Editor */
#define START_AMG                  7
#define START_FD                   8
#define STOP_AMG                   9
#define STOP_FD                    10
#define AMG_READY                  11
#define ACKN_STOPPED               12
#define PROC_TERM                  13
#define DEBUG                      14
#define RETRY                      15
#define QUEUE                      16
#define TRANSFER                   17
#define IS_ALIVE                   18
#define SHUTDOWN                   19
#define FSA_ABOUT_TO_CHANGE        20
#define CHECK_FILE_DIR             21 /* Check for jobs without message. */
#define DISABLE_MON                22
#define ENABLE_MON                 23
#define TRACE                      24
#define FULL_TRACE                 25
#define SR_EXEC_STAT               26 /* Show + reset exec stat in dir_check. */
#define SWITCH_MON                 27
#define FORCE_REMOTE_DIR_CHECK     28
#define GOT_LC                     29 /* Got log capabilities. */
#define REREAD_LOC_INTERFACE_FILE  30
#define FLUSH_MSG_FIFO_DUMP_QUEUE  31
#define CHECK_FSA_ENTRIES          32
#define DATA_READY                 33
#define BUSY_WORKING               34
#define SHUTDOWN_ALL               35
#define START_AFD                  36
#define START_AFD_NO_DIR_SCAN      37
#define SEARCH_OLD_FILES           38

#define DELETE_ALL_JOBS_FROM_HOST  1
#define DELETE_MESSAGE             2
#define DELETE_SINGLE_FILE         3
#define DELETE_RETRIEVE            4
#define DELETE_RETRIEVES_FROM_DIR  5

#define QUEUE_LIST_READY           1
#define QUEUE_LIST_EMPTY           2
#define QUEUE_LIST_DONE            3

/* Definitions for directory flags + options. */
/* NOTE: Do not seperate these into flags + options. Reason is that */
/*       Up to FRA version 7 these where all under flags. Only with */
/*       version came the new variable dir_options.                 */
#define MAX_COPIED                 1
#define FILES_IN_QUEUE             2
/* #define ____no_used____            4 */
#define LINK_NO_EXEC               8
#define DIR_DISABLED               16
#define ACCEPT_DOT_FILES           32
#define DONT_GET_DIR_LIST          64
#define DIR_ERROR_SET              128
#define WARN_TIME_REACHED          256
#define DIR_ERROR_ACKN             512
#define DIR_ERROR_OFFLINE          1024
#define DIR_ERROR_ACKN_T           2048
#define DIR_ERROR_OFFL_T           4096
#define DIR_STOPPED                8192
#ifdef WITH_INOTIFY
# define INOTIFY_RENAME            16384
# define INOTIFY_CLOSE             32768
#endif
#define ALL_DISABLED               65536
#ifdef WITH_INOTIFY
# define INOTIFY_ATTRIB            131072
# define INOTIFY_NEEDS_SCAN        262144
# define INOTIFY_CREATE            524288
#endif
#define INFO_TIME_REACHED          1048576
#define DO_NOT_PARALLELIZE         2097152
#define DO_NOT_MOVE                4194304
#ifdef WITH_INOTIFY
# define INOTIFY_DELETE            8388608
#endif
#define DIR_DISABLED_STATIC        16777216
#define ONE_PROCESS_JUST_SCANNING  33554432
#define URL_CREATES_FILE_NAME      67108864
#define URL_WITH_INDEX_FILE_NAME   134217728
#define NO_DELIMITER               268435456
#define KEEP_PATH                  536870912

#ifdef WITH_INOTIFY
/*
 * Note: The following inotify flag are for the user interface
 *       AFD_CONFIG and DIR_CONFIG.
 */
# define INOTIFY_RENAME_FLAG       1
# define INOTIFY_CLOSE_FLAG        2
# define INOTIFY_CREATE_FLAG       4
# define INOTIFY_DELETE_FLAG       8
# define INOTIFY_ATTRIB_FLAG      16
#endif


#ifdef WITH_DUP_CHECK
/* Definitions for duplicate check. */
# define DC_FILENAME_ONLY          1
# define DC_FILENAME_ONLY_BIT      1
# define DC_FILE_CONTENT           2
# define DC_FILE_CONTENT_BIT       2
# define DC_FILE_CONT_NAME         4
# define DC_FILE_CONT_NAME_BIT     3
# define DC_NAME_NO_SUFFIX         8
# define DC_NAME_NO_SUFFIX_BIT     4
# define DC_FILENAME_AND_SIZE      16
# define DC_FILENAME_AND_SIZE_BIT  5
# define DC_CRC32                  32768
# define DC_CRC32_BIT              16
# define DC_CRC32C                 65536
# define DC_CRC32C_BIT             17
# define DC_MURMUR3                131072
# define DC_MURMUR3_BIT            18
# define DC_DELETE                 8388608
# define DC_DELETE_BIT             24
# define DC_STORE                  16777216
# define DC_STORE_BIT              25
# define DC_WARN                   33554432
# define DC_WARN_BIT               26
# define TIMEOUT_IS_FIXED          1073741824
# define TIMEOUT_IS_FIXED_BIT      31
# define USE_RECIPIENT_ID          2147483648U
# define USE_RECIPIENT_ID_BIT      32
# define DC_DELETE_WARN_BIT        33
# define DC_STORE_WARN_BIT         34
#endif

/* Bitmap definitions for in_dc_flag in struct fileretrieve_status. */
#define DIR_ALIAS_IDC              1
#define UNKNOWN_FILES_IDC          2
#define QUEUED_FILES_IDC           4
#define OLD_LOCKED_FILES_IDC       8
#define REPUKW_FILES_IDC           16
#define DONT_REPUKW_FILES_IDC      32
#define MAX_CP_FILES_IDC           64
#define MAX_CP_FILE_SIZE_IDC       128
#define WARN_TIME_IDC              256
#define KEEP_CONNECTED_IDC         512
#ifdef WITH_INOTIFY
# define INOTIFY_FLAG_IDC          1024
#endif
#define DONT_DELUKW_FILES_IDC      2048
#define MAX_PROCESS_IDC            4096
#define INFO_TIME_IDC              8192
#define MAX_ERRORS_IDC             16384
#define UNREADABLE_FILES_IDC       32768
#define LOCAL_REMOTE_DIR_IDC       65536
#define CREATE_SRC_DIR_IDC         131072

/* In process AFD we have various stop flags. */
#define STARTUP_ID                 -1
#define NONE_ID                    0
#define ALL_ID                     1
#define AMG_ID                     2
#define FD_ID                      3

#define NO_ID                      0

/* Possible return values for parameter accuracy in datestr2unixtime() */
#define DS2UT_NONE                 0
#define DS2UT_DAY                  1
#define DS2UT_MINUTE               2
#define DS2UT_SECOND               3

/* Definitions for event classes. */
#define EC_GLOB                    1  /* Global event. */
#define EC_DIR                     2  /* Directory event. */
#define EC_PROD                    3  /* Production event. */
#define EC_HOST                    4  /* Host event. */

/* Definitions for event types. */
#define ET_MAN                     1  /* Manual event. */
#define ET_EXT                     2  /* External event. */
#define ET_AUTO                    3  /* Automatic event. */

/* Definitions for different event actions, see doc/txt/event_log.txt. */
#define EA_REREAD_DIR_CONFIG             1
#define EA_REREAD_HOST_CONFIG            2
#define EA_REREAD_RENAME_RULE            3
#define EA_AFD_CONFIG_CHANGE             4
#define EA_ENABLE_RETRIEVE               5
#define EA_DISABLE_RETRIEVE              6
#define EA_ENABLE_ARCHIVE                7
#define EA_DISABLE_ARCHIVE               8
#define EA_ENABLE_CREATE_TARGET_DIR      9
#define EA_DISABLE_CREATE_TARGET_DIR    10
#define EA_ENABLE_DIR_WARN_TIME         11
#define EA_DISABLE_DIR_WARN_TIME        12
#define EA_AMG_STOP                     13
#define EA_AMG_START                    14
#define EA_FD_STOP                      15
#define EA_FD_START                     16
#define EA_AFD_STOP                     17
#define EA_AFD_START                    18
#define EA_PRODUCTION_ERROR             19
#define EA_ERROR_START                  20
#define EA_ERROR_END                    21
#define EA_ENABLE_DIRECTORY             22
#define EA_DISABLE_DIRECTORY            23
#define EA_RESCAN_DIRECTORY             24
#define EA_EXEC_ERROR_ACTION_START      25
#define EA_EXEC_ERROR_ACTION_STOP       26
#define EA_OFFLINE                      27
#define EA_ACKNOWLEDGE                  28
#define EA_ENABLE_HOST                  29
#define EA_DISABLE_HOST                 30
#define EA_START_TRANSFER               31
#define EA_STOP_TRANSFER                32
#define EA_START_QUEUE                  33
#define EA_STOP_QUEUE                   34
#define EA_START_ERROR_QUEUE            35
#define EA_STOP_ERROR_QUEUE             36
#define EA_SWITCH_HOST                  37
#define EA_RETRY_HOST                   38
#define EA_ENABLE_DEBUG_HOST            39
#define EA_ENABLE_TRACE_HOST            40
#define EA_ENABLE_FULL_TRACE_HOST       41
#define EA_DISABLE_DEBUG_HOST           42
#define EA_DISABLE_TRACE_HOST           43
#define EA_DISABLE_FULL_TRACE_HOST      44
#define EA_UNSET_ACK_OFFL               45
#define EA_WARN_TIME_SET                46
#define EA_WARN_TIME_UNSET              47
#define EA_ENABLE_HOST_WARN_TIME        48
#define EA_DISABLE_HOST_WARN_TIME       49
#define EA_ENABLE_DELETE_DATA           50
#define EA_DISABLE_DELETE_DATA          51
#define EA_EXEC_WARN_ACTION_START       52
#define EA_EXEC_WARN_ACTION_STOP        53
#define EA_EXEC_SUCCESS_ACTION_START    54
#define EA_EXEC_SUCCESS_ACTION_STOP     55
#define EA_START_DIRECTORY              56
#define EA_STOP_DIRECTORY               57
#define EA_CHANGE_INFO                  58
#define EA_ENABLE_CREATE_SOURCE_DIR     59
#define EA_DISABLE_CREATE_SOURCE_DIR    60
#define EA_INFO_TIME_SET                61
#define EA_INFO_TIME_UNSET              62
#define EA_EXEC_INFO_ACTION_START       63
#define EA_EXEC_INFO_ACTION_STOP        64
#define EA_ENABLE_SIMULATE_SEND_MODE    65
#define EA_DISABLE_SIMULATE_SEND_MODE   66
#define EA_ENABLE_SIMULATE_SEND_HOST    67
#define EA_DISABLE_SIMULATE_SEND_HOST   68
#define EA_MODIFY_ERRORS_OFFLINE        69
#define EA_CHANGE_REAL_HOSTNAME         70
#define EA_MAX_EVENT_ACTION             70

/* See ea_str.h for longest event string. */
#define MAX_EVENT_ACTION_LENGTH     (sizeof("Disable create target dir") - 1)

/* Different return status codes when updating configuration. */
#define NO_CHANGE_IN_DIR_CONFIG         1
#define DIR_CONFIG_UPDATED              2
#define DIR_CONFIG_UPDATED_DC_PROBLEMS  3
#define DIR_CONFIG_NO_VALID_DATA        4
#define DIR_CONFIG_EMPTY                5
#define DIR_CONFIG_ACCESS_ERROR         6
#define DIR_CONFIG_NOTHING_DONE         7
#define NO_CHANGE_IN_HOST_CONFIG        101
#define HOST_CONFIG_RECREATED           102
#define HOST_CONFIG_DATA_CHANGED        103
#define HOST_CONFIG_DATA_ORDER_CHANGED  104
#define HOST_CONFIG_ORDER_CHANGED       105
#define HOST_CONFIG_UPDATED_DC_PROBLEMS 106

/* The following definitions are used for the function */
/* write_fsa(), so it knows where to write the info.   */
#define ERROR_COUNTER              1
#define TOTAL_FILE_SIZE            3
#define TRANSFER_RATE              9
#define NO_OF_FILES                11
#define CONNECT_STATUS             20

/* In case some key values of any structure are changed. */
#define MAX_MSG_NAME_LENGTH_NR       1
#define MAX_FILENAME_LENGTH_NR       2
#define MAX_HOSTNAME_LENGTH_NR       4
#define MAX_REAL_HOSTNAME_LENGTH_NR  8
#define MAX_AFDNAME_LENGTH_NR        16
#define MAX_PROXY_NAME_LENGTH_NR     32
#define MAX_TOGGLE_STR_LENGTH_NR     64
#define ERROR_HISTORY_LENGTH_NR      128
#define MAX_NO_PARALLEL_JOBS_NR      256
#define MAX_DIR_ALIAS_LENGTH_NR      512
#define MAX_RECIPIENT_LENGTH_NR      1024
#define MAX_WAIT_FOR_LENGTH_NR       2048
#define MAX_FRA_TIME_ENTRIES_NR      4096
#define MAX_OPTION_LENGTH_NR         8192
#define MAX_PATH_LENGTH_NR           16384
#define MAX_USER_NAME_LENGTH_NR      32768
#define CHAR_NR                      65536
#define INT_NR                       131072
#define OFF_T_NR                     262144
#define TIME_T_NR                    524288
#define SHORT_NR                     1048576
#define LONG_LONG_NR                 2097152
#define PID_T_NR                     4194304
#define MAX_TIMEZONE_LENGTH_NR       8388608

#define MAX_MSG_NAME_LENGTH_POS      0
#define MAX_FILENAME_LENGTH_POS      1
#define MAX_HOSTNAME_LENGTH_POS      2
#define MAX_REAL_HOSTNAME_LENGTH_POS 3
#define MAX_AFDNAME_LENGTH_POS       4
#define MAX_PROXY_NAME_LENGTH_POS    5
#define MAX_TOGGLE_STR_LENGTH_POS    6
#define ERROR_HISTORY_LENGTH_POS     7
#define MAX_NO_PARALLEL_JOBS_POS     8
#define MAX_DIR_ALIAS_LENGTH_POS     9
#define MAX_RECIPIENT_LENGTH_POS     10
#define MAX_WAIT_FOR_LENGTH_POS      11
#define MAX_FRA_TIME_ENTRIES_POS     12
#define MAX_OPTION_LENGTH_POS        13
#define MAX_PATH_LENGTH_POS          14
#define MAX_USER_NAME_LENGTH_POS     15
#define CHAR_POS                     16
#define INT_POS                      17
#define OFF_T_POS                    18
#define TIME_T_POS                   19
#define SHORT_POS                    20
#define LONG_LONG_POS                21
#define PID_T_POS                    22
#define MAX_TIMEZONE_LENGTH_POS      23
#define MAX_CHANGEABLE_VARS          1 + 24 /* First is used as flag*/
                                        /* which of the below values*/
                                        /* are set.                 */
                                        /* MAX_MSG_NAME_LENGTH      */
                                        /* MAX_FILENAME_LENGTH      */
                                        /* MAX_HOSTNAME_LENGTH      */
                                        /* MAX_REAL_HOSTNAME_LENGTH */
                                        /* MAX_AFDNAME_LENGTH       */
                                        /* MAX_PROXY_NAME_LENGTH    */
                                        /* MAX_TOGGLE_STR_LENGTH    */
                                        /* ERROR_HISTORY_LENGTH     */
                                        /* MAX_NO_PARALLEL_JOBS     */
                                        /* MAX_DIR_ALIAS_LENGTH     */
                                        /* MAX_RECIPIENT_LENGTH     */
                                        /* MAX_WAIT_FOR_LENGTH      */
                                        /* MAX_FRA_TIME_ENTRIES     */
                                        /* MAX_OPTION_LENGTH        */
                                        /* MAX_PATH_LENGTH          */
                                        /* MAX_USER_NAME_LENGTH     */
                                        /* CHAR                     */
                                        /* INT                      */
                                        /* OFF_T                    */
                                        /* TIME_T                   */
                                        /* SHORT                    */
                                        /* LONG_LONG                */
                                        /* MAX_TIMEZONE_LENGTH      */

/* Definitions for the different lock positions in the FSA. */
#define LOCK_TFC                   3   /* Total file counter */
#define LOCK_EC                    4   /* Error counter */
#define LOCK_CON                   5   /* Connections */
#define LOCK_EXEC                  6   /* Lock for exec and pexec option, */
                                       /* this is ALSO used in FRA.       */
#define LOCK_HS                    7   /* Lock host_status in FSA.        */
#define LOCK_FIU                   8   /* Lock the file_name_in_use.      */
#define LOCK_CHECK_FSA_ENTRIES     (AFD_WORD_OFFSET - 1)

/*-----------------------------------------------------------------------*
 * Word offset for memory mapped structures of the AFD. Best is to leave
 * this value as it is. If you do change it you must remove all existing
 * memory mapped files from the fifo_dir, before starting the AFD with the
 * new value.
 * For the FSA these bytes are used to store information about the hole
 * AFD with the following meaning (assuming SIZEOF_INT is 4):
 *     Byte  | Type          | Description
 *    -------+---------------+---------------------------------------
 *     1 - 4 | int           | The number of hosts served by the AFD.
 *           |               | If this FSA in no longer in use there
 *           |               | will be a -1 here.
 *    -------+---------------+---------------------------------------
 *       5   | unsigned char | Counter that is increased each time
 *           |               | there was a change in the HOST_CONFIG.
 *    -------+---------------+---------------------------------------
 *       6   | unsigned char | Flag to enable or disable the
 *           |               | following features:
 *           |               | Bit| Meaning
 *           |               | ---+---------------------------
 *           |               |  1 | DISABLE_RETRIEVE
 *           |               |  2 | DISABLE_ARCHIVE
 *           |               |  3 | ENABLE_CREATE_TARGET_DIR
 *           |               |  4 | DISABLE_HOST_WARN_TIME
 *           |               |  5 | DISABLE_CREATE_SOURCE_DIR
 *           |               |  6 | ENABLE_SIMULATE_SEND_MODE
 *    -------+---------------+---------------------------------------
 *       7   | unsigned char | The number of errors that are shown
 *           |               | in offline mode. 0 (default) means all
 *           |               | are shown as errors/warning.
 *    -------+---------------+---------------------------------------
 *       8   | unsigned char | Version of this structure.
 *    -------+---------------+---------------------------------------
 *    9 - 12 | int           | Pagesize of this system.
 *    -------+---------------+---------------------------------------
 *   13 - 16 | int           | Not used.
 *
 * This is also used for the FRA with the following meaning:
 *     Byte  | Type          | Description
 *    -------+---------------+---------------------------------------
 *     1 - 4 | int           | The number of directories that are
 *           |               | monitored by AFD. If this FRA in no
 *           |               | longer in use there will be a -1 here.
 *    -------+---------------+---------------------------------------
 *       5   | unsigned char | Not used.
 *    -------+---------------+---------------------------------------
 *       6   | unsigned char | Flag to enable or disable the
 *           |               | following features:
 *           |               | Bit| Meaning
 *           |               | ---+-----------------------
 *           |               |  1 | DISABLE_DIR_WARN_TIME
 *    -------+---------------+---------------------------------------
 *       7   | unsigned char | Not used.
 *    -------+---------------+---------------------------------------
 *       8   | unsigned char | Version of this structure.
 *    -------+---------------+---------------------------------------
 *    9 - 16 |               | Not used.
 *
 * And also for struct retrieve_list with the following meaning:
 *     Byte  | Type          | Description
 *    -------+---------------+---------------------------------------
 *     1 - 4 | int           | The number of files in the list.
 *    -------+---------------+---------------------------------------
 *       5   | unsigned char | Not used.
 *    -------+---------------+---------------------------------------
 *       6   | unsigned char | Not used.
 *    -------+---------------+---------------------------------------
 *       7   | unsigned char | Not used.
 *    -------+---------------+---------------------------------------
 *       8   | unsigned char | Version of this structure.
 *    -------+---------------+---------------------------------------
 *    9 - 16 | time_t        | The creation time of this retrieve
 *           |               | list.
 *-----------------------------------------------------------------------*/
#define AFD_WORD_OFFSET               (SIZEOF_INT + 4 + SIZEOF_INT + 4)
#define AFD_FEATURE_FLAG_OFFSET_START (SIZEOF_INT + 1)                      /* From start */
#define AFD_FEATURE_FLAG_OFFSET_END   (SIZEOF_INT + SIZEOF_INT + 1 + 1 + 1) /* From end   */
#define AFD_START_ERROR_OFFSET_START  (SIZEOF_INT + 1 + 1)                  /* From start */
#define AFD_START_ERROR_OFFSET_END    (SIZEOF_INT + SIZEOF_INT + 1 + 1)     /* From end   */

/* Structure that holds status of the file transfer for each host. */
#define CURRENT_FSA_VERSION 4
struct status
       {
          pid_t         proc_id;            /* Process ID of transfering */
                                            /* job.                      */
#ifdef _WITH_BURST_2
          char          unique_name[MAX_MSG_NAME_LENGTH];
          unsigned int  job_id;             /* Since each host can have  */
                                            /* different type of jobs    */
                                            /* (other user, different    */
                                            /* directory, other options, */
                                            /* etc), each of these is    */
                                            /* identified by this number.*/
#endif
          unsigned char special_flag;       /*+-----+-------------------+*/
                                            /*| Bit |      Meaning      |*/
                                            /*+-----+-------------------+*/
                                            /*| 1-8 | Not used.         |*/
                                            /*+-----+-------------------+*/
          char          connect_status;     /* The status of what        */
                                            /* sf_xxx() is doing.        */
          int           no_of_files;        /* Total number of all files */
                                            /* when job started.         */
          int           no_of_files_done;   /* Number of files done since*/
                                            /* the job has been started. */
          off_t         file_size;          /* Total size of all files   */
                                            /* when we started this job. */
          u_off_t       file_size_done;     /* The total number of bytes */
                                            /* we have send so far.      */
          u_off_t       bytes_send;         /* Overall number of bytes   */
                                            /* send so far for this job. */
          char          file_name_in_use[MAX_FILENAME_LENGTH];
                                            /* The name of the file that */
                                            /* is in transfer.           */
#ifdef _WITH_BURST_2
                                            /* NOTE: We misuse this      */
                                            /* field in case of a burst  */
                                            /* to specify the number of  */
                                            /* retries.                  */
#endif
          off_t         file_size_in_use;   /* Total size of current     */
                                            /* file.                     */
          off_t         file_size_in_use_done;/* Number of bytes send    */
                                            /* for current file.         */
       };

struct filetransfer_status
       {
          char           host_alias[MAX_HOSTNAME_LENGTH + 1];
                                            /* Here the alias hostname is*/
                                            /* stored. When a secondary  */
                                            /* host can be specified,    */
                                            /* only that part is stored  */
                                            /* up to the position of the */
                                            /* toggling character eg:    */
                                            /* mrz_mfa + mrz_mfb =>      */
                                            /*       mrz_mf              */
          char           real_hostname[2][MAX_REAL_HOSTNAME_LENGTH];
                                            /* This is the real hostname */
                                            /* where the data should be  */
                                            /* send to.                  */
          char           host_dsp_name[MAX_HOSTNAME_LENGTH + 2];
                                            /* This is the hostname that */
                                            /* is being displayed by     */
                                            /* afd_ctrl. It's the same   */
                                            /* as stored in host_alias   */
                                            /* plus the toggling         */
                                            /* character.                */
          char           proxy_name[MAX_PROXY_NAME_LENGTH + 1];
          char           host_toggle_str[MAX_TOGGLE_STR_LENGTH];
          char           toggle_pos;        /* The position of the       */
                                            /* toggling character in the */
                                            /* host name.                */
          char           original_toggle_pos;/* The original position    */
                                            /* before it was toggled     */
                                            /* automatically.            */
          char           auto_toggle;       /* When ON and an error      */
                                            /* occurs it switches auto-  */
                                            /* matically to the other    */
                                            /* host.                     */
          signed char    file_size_offset;  /* When doing an ls on the   */
                                            /* remote site, this is the  */
                                            /* position where to find    */
                                            /* the size of the file. If  */
                                            /* it is less than 0, it     */
                                            /* means that we do not want */
                                            /* to append files that have */
                                            /* been partly send.         */
          int            successful_retries;/* Number of current         */
                                            /* successful retries.       */
          int            max_successful_retries; /* Retries before       */
                                            /* switching hosts.          */
          unsigned char  special_flag;      /* Special flag with the     */
                                            /* following meaning:        */
                                            /*+-----+-------------------+*/
                                            /*| Bit |      Meaning      |*/
                                            /*+-----+-------------------+*/
                                            /*|   8 | Not used.         |*/
                                            /*|   7 | Host is in        |*/
                                            /*|     | DIR_CONFIG file.  |*/
                                            /*|   6 | Host disabled.    |*/
                                            /*| 3-5 | Not used.         |*/
                                            /*|   2 | KEEP_CON_NO_SEND  |*/
                                            /*|   1 | KEEP_CON_NO_FETCH |*/
                                            /*+-----+-------------------+*/
          unsigned int   protocol;          /* Transfer protocol that    */
                                            /* is being used:            */
                                            /*+-----+-------------------+*/
                                            /*| Bit |      Meaning      |*/
                                            /*+-----+-------------------+*/
                                            /*|   32| RETRIEVE          |*/
                                            /*|   31| SEND              |*/
                                            /*|   30| SSL               |*/
                                            /*|   29| DISABLE_IPV6      |*/
                                            /*|12-28| Not used.         |*/
                                            /*|   11| DEMAIL            |*/
                                            /*|   10| DFAX              |*/
                                            /*|    9| EXEC              |*/
                                            /*|    8| SFTP              |*/
                                            /*|    7| HTTP       [SSL]  |*/
                                            /*|    6| WMO               |*/
                                            /*|    5| SCP               |*/
                                            /*|    4| MAP               |*/
                                            /*|    3| SMTP              |*/
                                            /*|    2| LOC               |*/
                                            /*|    1| FTP        [SSL]  |*/
                                            /*+-----+-------------------+*/
          unsigned int   protocol_options;  /* Special options for the   */
                                            /* protocols:                */
                                            /*+-----+-------------------+*/
                                            /*| Bit |      Meaning      |*/
                                            /*+-----+-------------------+*/
                                            /*|   32| TLS_LEGACY_RENEGOTIATION|*/
                                            /*|   31| HTTP_BUCKETNAME_IN_PATH|*/
                                            /*|   30| NO_EXPECT         |*/
#ifdef _WITH_EXTRA_CHECK
                                            /*|   29| USE_EXTRA_CHECK   |*/
#endif
                                            /*|   28| IMPLICIT_FTPS     |*/
                                            /*|   27| USE_STAT_LIST     |*/
                                            /*|   26| DISABLE_STRICT_HOST_KEY|*/
                                            /*|   25| KEEP_CONNECTED_DISCONNECT|*/
                                            /*|   24| FTP_DISABLE_MLST  |*/
                                            /*|   23| TLS_STRICT_VERIFY |*/
                                            /*|   22| FTP_USE_LIST      |*/
                                            /*|   21| FTP_CCC_OPTION    |*/
                                            /*|   20| KEEP_CON_NO_SEND_2|*/
                                            /*|   19| KEEP_CON_NO_FETCH_2|*/
                                            /*|   18| TIMEOUT_TRANSFER  |*/
                                            /*|   17| CHECK_SIZE        |*/
                                            /*|   16| NO_AGEING_JOBS    |*/
                                            /*|   15| SORT_FILE_NAMES   |*/
                                            /*|   14| KEEP_TIME_STAMP   |*/
                                            /*|   13| ENABLE_COMPRESSION|*/
                                            /*|   12| USE_SEQUENCE_LOCKING|*/
                                            /*|   11| AFD_TCP_KEEPALIVE |*/
                                            /*|   10| FILE_WHEN_LOCAL_FLAG|*/
                                            /*|    9| FTP_ALLOW_DATA_REDIRECT|*/
                                            /*|    8| DISABLE_BURSTING  |*/
                                            /*|    7| FTP_EXTENDED_MODE |*/
                                            /*|    6| FTP_IGNORE_BIN    |*/
                                            /*|    5| FTP_FAST_CD       |*/
                                            /*|    4| FTP_FAST_MOVE     |*/
                                            /*|    3| STAT_KEEPALIVE    |*/
                                            /*|    2| SET_IDLE_TIME     |*/
                                            /*|    1| FTP_PASSIVE_MODE  |*/
                                            /*+-----+-------------------+*/
          unsigned int   protocol_options2; /* More special options for  */
                                            /* the protocols:            */
                                            /*+-----+-------------------+*/
                                            /*| Bit |      Meaning      |*/
                                            /*+-----+-------------------+*/
                                            /*| 2-32| Not used.         |*/
                                            /*|    1| FTP_SEND_UTF8_ON  |*/
                                            /*+-----+-------------------+*/
          unsigned int   socksnd_bufsize;   /* Socket buffer size for    */
                                            /* sending data. 0 is default*/
                                            /* which is the socket buffer*/
                                            /* is left to system default.*/
          unsigned int   sockrcv_bufsize;   /* Socket buffer size for    */
                                            /* receiving data.           */
          unsigned int   keep_connected;    /* Keep connection open for  */
                                            /* the given number of       */
                                            /* seconds, after all files  */
                                            /* have been transmitted. A  */
                                            /* value of 0 means not set. */
#ifdef WITH_DUP_CHECK                                                      
          unsigned int   dup_check_flag;    /* Flag storing the type of  */
                                            /* check that is to be done  */
                                            /* and what type of CRC to   */
                                            /* use:                      */
                                            /*+-----+-------------------+*/
                                            /*| Bit |     Meaning       |*/
                                            /*+-----+-------------------+*/
                                            /*|   32| USE_RECIPIENT_ID  |*/
                                            /*|   31| TIMEOUT_IS_FIXED  |*/
                                            /*|27-30| Not used.         |*/
                                            /*|   26| DC_WARN           |*/
                                            /*|   25| DC_STORE          |*/
                                            /*|   24| DC_DELETE         |*/
                                            /*|19-23| Not used.         |*/
                                            /*|   18| DC_MURMUR3        |*/
                                            /*|   17| DC_CRC32C         |*/
                                            /*|   16| DC_CRC32          |*/
                                            /*| 6-15| Not used.         |*/
                                            /*|    5| DC_FILENAME_AND_SIZE|*/
                                            /*|    4| DC_NAME_NO_SUFFIX |*/
                                            /*|    3| DC_FILE_CONT_NAME |*/
                                            /*|    2| DC_FILE_CONTENT   |*/
                                            /*|    1| DC_FILENAME_ONLY  |*/
                                            /*+-----+-------------------+*/
#endif
          unsigned int   host_id;           /* CRC-32 checksum of        */
                                            /* host_alias above.         */
          char           debug;             /* When this flag is set all */
                                            /* transfer information is   */
                                            /* logged.                   */
          char           host_toggle;       /* If their is more then one */
                                            /* host you can toggle       */
                                            /* between these two         */
                                            /* addresses by toggling     */
                                            /* this switch.              */
          unsigned int   host_status;       /* What is the status for    */
                                            /* this host?                */
                                            /*+-----+-------------------------+*/
                                            /*| Bit |        Meaning          |*/
                                            /*+-----+-------------------------+*/
                                            /*|22-32|Not used.                |*/
                                            /*|   21|WARN_HOSTS_IN_GROUP      |*/
                                            /*|   20|ERROR_HOSTS_IN_GROUP     |*/
                                            /*|   19|SIMULATE_SEND_MODE       |*/
                                            /*|   18|STORE_IP                 |*/
                                            /*|   17|HOST_ACTION_SUCCESS      |*/
                                            /*|   16|DO_NOT_DELETE_DATA       |*/
                                            /*|   15|HOST_WARN_TIME_REACHED   |*/
                                            /*|   14|HOST_ERROR_EA_STATIC (not used!)|*/
                                            /*|   13|HOST_ERROR_OFFLINE_T     |*/
                                            /*|   12|HOST_ERROR_ACKNOWLEDGED_T|*/
                                            /*|   11|HOST_ERROR_OFFLINE       |*/
                                            /*|   10|HOST_ERROR_ACKNOWLEDGED  |*/
                                            /*|    9|PENDING_ERRORS           |*/
                                            /*|    8|ERROR_QUEUE_SET          |*/
                                            /*|    7|HOST_TWO_FLAG            |*/
                                            /*|    6|HOST_CONFIG_HOST_DISABLED|*/
                                            /*|    5|HOST_ERROR_OFFLINE_STATIC|*/
                                            /*|    4|DANGER_PAUSE_QUEUE_STAT  |*/
                                            /*|    3|AUTO_PAUSE_QUEUE_STAT    |*/
                                            /*|    2|PAUSE_QUEUE_STAT         |*/
                                            /*|    1|STOP_TRANSFER_STAT       |*/
                                            /*+-----+-------------------------+*/
          int            error_counter;     /* Errors that have so far   */
                                            /* occurred. With the next   */
                                            /* successful transfer this  */
                                            /* will be set to 0.         */
          unsigned int   total_errors;      /* No. of errors so far.     */
          int            max_errors;        /* The maximum errors that   */
                                            /* may occur before we ring  */
                                            /* the alarm bells ;-).      */
          unsigned char  error_history[ERROR_HISTORY_LENGTH];
                                            /* For the error code see    */
                                            /* fddefs.h. (_ERROR_STR)    */
          int            retry_interval;    /* After an error has        */
                                            /* occurred, when should we  */
                                            /* retry?                    */
          int            block_size;        /* Block size at which the   */
                                            /* files get transfered.     */
          int            ttl;               /* Time-to-live for outgoing */
                                            /* multicasts.               */
                                            /* NOTE: Not used. Also in   */
                                            /*       HOST_CONFIG!        */
#ifdef WITH_DUP_CHECK                                                      
          time_t         dup_check_timeout; /* When the stored CRC for   */
                                            /* duplicate checks are no   */
                                            /* longer valid. Value is in */
                                            /* seconds.                  */
#endif
          time_t         last_retry_time;   /* When was the last time we */
                                            /* tried to send a file for  */
                                            /* this host?                */
          time_t         last_connection;   /* Time of last successfull  */
                                            /* transfer.                 */
          time_t         first_error_time;  /* The first time when a     */
                                            /* transmission error        */
                                            /* condition started.        */
          time_t         start_event_handle;/* Time when to set the      */
                                            /* specified event.          */
          time_t         end_event_handle;  /* Time when to remove the   */
                                            /* specified event.          */
          time_t         warn_time;         /* Time when to warn that the*/
                                            /* the host has not received */
                                            /* any data.                 */
          int            total_file_counter;/* The overall number of     */
                                            /* files still to be send.   */
          off_t          total_file_size;   /* The overall number of     */
                                            /* bytes still to be send.   */
          unsigned int   jobs_queued;       /* The number of jobs queued */
                                            /* by the FD.                */
          unsigned int   file_counter_done; /* No. of files done so far. */
          u_off_t        bytes_send;        /* No. of bytes send so far. */
          unsigned int   connections;       /* No. of connections.       */
          int            active_transfers;  /* No. of jobs transferring  */
                                            /* data.                     */
          int            allowed_transfers; /* Maximum no. of parallel   */
                                            /* transfers for this host.  */
          long           transfer_timeout;  /* When to timeout the       */
                                            /* transmitting job.         */
          off_t          transfer_rate_limit;/* The maximum bytes that   */
                                            /* may be transfered per     */
                                            /* second.                   */
          off_t          trl_per_process;   /* Transfer rate limit per   */
                                            /* active process.           */
          struct status  job_status[MAX_NO_PARALLEL_JOBS];
       };

/* Structure that holds all hosts. */
#define HOST_BUF_SIZE 100
struct host_list
       {
          char           host_alias[MAX_HOSTNAME_LENGTH + 1];
          char           fullname[MAX_FILENAME_LENGTH];
                                              /* This is needed when we   */
                                              /* have hostname with []    */
                                              /* syntax.                  */
          char           real_hostname[2][MAX_REAL_HOSTNAME_LENGTH];
          char           host_toggle_str[MAX_TOGGLE_STR_LENGTH];
          char           proxy_name[MAX_PROXY_NAME_LENGTH + 1];
          int            allowed_transfers;
          int            max_errors;
          int            retry_interval;
          int            ttl;               /* Not used!                 */
          int            transfer_blksize;
          int            transfer_rate_limit;
          int            successful_retries;/* NOTE: Corresponds to      */
                                            /* max_successful_retries in */
                                            /* FSA.                      */
          unsigned int   protocol_options;  /* Mostly used for FTP, to   */
                                            /* indicate for example:     */
                                            /* active-, passive-mode,    */
                                            /* send IDLE command, etc.   */
          unsigned int   protocol_options2;
          unsigned int   socksnd_bufsize;   /* Socket buffer size for    */
                                            /* sending data. 0 is default*/
                                            /* which is the socket buffer*/
                                            /* is left to system default.*/
          unsigned int   sockrcv_bufsize;   /* Socket buffer size for    */
                                            /* receiving data.           */
          unsigned int   keep_connected;    /* Keep connection open for  */
                                            /* the given number of       */
                                            /* seconds, after all files  */
                                            /* have been transmitted.    */
#ifdef WITH_DUP_CHECK                                                      
          unsigned int   dup_check_flag;    /* Flag storing the type of  */
                                            /* check that is to be done  */
                                            /* and what type of CRC to   */
                                            /* use:                      */
                                            /*+-----+-------------------+*/
                                            /*| Bit |      Meaning      |*/
                                            /*+-----+-------------------+*/
                                            /*|27-32| Not used.         |*/
                                            /*|   26| DC_WARN           |*/
                                            /*|   25| DC_STORE          |*/
                                            /*|   24| DC_DELETE         |*/
                                            /*|19-23| Not used.         |*/
                                            /*|   18| DC_MURMUR3        |*/
                                            /*|   17| DC_CRC32C         |*/
                                            /*|   16| DC_CRC32          |*/
                                            /*| 6-15| Not used.         |*/
                                            /*|    5| DC_FILENAME_AND_SIZE|*/
                                            /*|    4| DC_NAME_NO_SUFFIX |*/
                                            /*|    3| DC_FILE_CONT_NAME |*/
                                            /*|    2| DC_FILE_CONTENT   |*/
                                            /*|    1| DC_FILENAME_ONLY  |*/
                                            /*+-----+-------------------+*/
#endif
          unsigned int  protocol;
          unsigned int  host_status;
          long          transfer_timeout;
#ifdef WITH_DUP_CHECK                                                      
          time_t        dup_check_timeout;  /* When the stored CRC for   */
                                            /* duplicate checks are no   */
                                            /* longer valid. Value is in */
                                            /* seconds.                  */
#endif
          time_t        warn_time;          /* Time when to warn that the*/
                                            /* the host has not received */
                                            /* any data.                 */
          signed char   file_size_offset;
          unsigned char number_of_no_bursts;/* Not used!                 */
          signed char   in_dir_config;
       };

#ifdef MULTI_FS_SUPPORT
/* Structure holding all information of extra working directories. */
struct extra_work_dirs
       {
          dev_t dev;
          char  *dir_name;
          char  *time_dir;
          char  *p_time_dir_id;
          char  *afd_file_dir;
          char  *outgoing_file_dir;
          int   dir_name_length;
          int   time_dir_length;
          int   afd_file_dir_length;
          int   outgoing_file_dir_length;
       };
#endif

/* Structure to hold all possible bits for a time entry. */
#define TIME_EXTERNAL SHRT_MAX
struct bd_time_entry
       {
#ifdef TIME_WITH_SECOND
# ifdef HAVE_LONG_LONG
          unsigned long long continuous_second;
          unsigned long long second;
# else
          unsigned char      continuous_second[8];
          unsigned char      second[8];
# endif
#endif
#ifdef HAVE_LONG_LONG
          unsigned long long continuous_minute;
          unsigned long long minute;
#else
          unsigned char      continuous_minute[8];
          unsigned char      minute[8];
#endif
          unsigned int       hour;
          unsigned int       day_of_month;
          unsigned short     month;
          unsigned char      day_of_week;
       };

/* Structure holding all neccessary data for retrieving files. */
#define CURRENT_FRA_VERSION      8
#define MAX_FRA_TIME_ENTRIES     12
#define MAX_FRA_TIME_ENTRIES_STR "MAX_FRA_TIME_ENTRIES"
#define MAX_WAIT_FOR_LENGTH      64
#define MAX_WAIT_FOR_LENGTH_STR  "MAX_WAIT_FOR_LENGTH"
struct fileretrieve_status
       {
          char          dir_alias[MAX_DIR_ALIAS_LENGTH + 1];
          char          host_alias[MAX_HOSTNAME_LENGTH + 1];
                                            /* Here the alias hostname is*/
                                            /* stored. When a secondary  */
                                            /* host can be specified,    */
                                            /* only that part is stored  */
                                            /* up to the position of the */
                                            /* toggling character eg:    */
                                            /* mrz_mfa + mrz_mfb =>      */
                                            /*       mrz_mf              */
          char          url[MAX_RECIPIENT_LENGTH];
          char          ls_data_alias[MAX_DIR_ALIAS_LENGTH + 1];
                                            /* This allows the user to   */
                                            /* specify the same alias for*/
                                            /* two or more retrieve dirs.*/
                                            /* Usefull when the dir name */
                                            /* has the %T time modifier  */
                                            /* and the time frame is     */
                                            /* across a new day.         */
          char          retrieve_work_dir[MAX_FILENAME_LENGTH];
                                            /* Other work dir for        */
                                            /* retrieving data.          */
          char          wait_for_filename[MAX_WAIT_FOR_LENGTH]; /* Wait  */
                                            /* for the given file name|  */
                                            /* pattern before we take    */
                                            /* files from this directory.*/
          char          timezone[MAX_TIMEZONE_LENGTH + 1];
          struct bd_time_entry te[MAX_FRA_TIME_ENTRIES]; /* Time entry,  */
                                            /* when files are to be      */
                                            /* searched for.             */
          struct bd_time_entry ate;         /* Additional time entry.    */
          unsigned char dir_status;         /* Status of this directory. */
          unsigned char remove;             /* Should the files be       */
                                            /* removed when they are     */
                                            /* being retrieved.          */
          unsigned char stupid_mode;        /* If set to YES it will NOT */
                                            /* collect information about */
                                            /* files that where found in */
                                            /* directory. So that when   */
                                            /* remove is not set we will */
                                            /* not always collect the    */
                                            /* same files. This ensures  */
                                            /* that files are collected  */
                                            /* only once.                */
                                            /* If this is set to         */
                                            /* GET_ONCE_ONLY it will get */
                                            /* the file once only,       */
                                            /* regardless if the file is */
                                            /* changed. If set to        */
                                            /* APPEND_ONLY, it will just */
                                            /* retrieve the appended data*/
                                            /* only. If set to NO it will*/
                                            /* try to fetch it again when*/
                                            /* it changes.               */
          unsigned char delete_files_flag;  /* UNKNOWN_FILES: All unknown*/
                                            /* files will be deleted.    */
                                            /* QUEUED_FILES: Queues will */
                                            /* also be checked for old   */
                                            /* files.                    */
                                            /* OLD_LOCKED_FILES: Old     */
                                            /* locked files are to be    */
                                            /* deleted.                  */
                                            /* UNREADABLE_FILES: Un-     */
                                            /* readable files are to be  */
                                            /* deleted.                  */
                                            /* OLD_RLOCKED_FILES: Old    */
                                            /* locked files on remote    */
                                            /* hosts are to be deleted.  */
                                            /*+-----+-------------------+*/
                                            /*| Bit |      Meaning      |*/
                                            /*+-----+-------------------+*/
                                            /*| 6-8 | Not used.         |*/
                                            /*|   5 | OLD_RLOCKED_FILES |*/
                                            /*|   4 | UNREADABLE_FILES  |*/
                                            /*|   3 | OLD_LOCKED_FILES  |*/
                                            /*|   2 | QUEUED_FILES      |*/
                                            /*|   1 | UNKNOWN_FILES     |*/
                                            /*+-----+-------------------+*/
          unsigned char report_unknown_files;
          unsigned char important_dir;      /* Directory is important.   */
                                            /* In times where all        */
                                            /* directories contain lots  */
                                            /* files or the filesystem   */
                                            /* is very slow, this        */
                                            /* directory will get more   */
                                            /* attention.                */
          unsigned char no_of_time_entries; /* The number of time options*/
                                            /* in use.                   */
          char          force_reread;       /* Always read the directory.*/
                                            /* Don't check the directory */
                                            /* time. If set to YES it is */
                                            /* for local and remote dirs.*/
                                            /* LOCAL_ONLY for local only.*/
                                            /* REMOTE_ONLY is only for   */
                                            /* remote directories.       */
          char          queued;             /* Used by FD, so it knows   */
                                            /* if the job is in the      */
                                            /* queue or not.             */
          char          priority;           /* Priority of this          */
                                            /* directory.                */
          unsigned int  protocol;           /* Transfer protocol that    */
                                            /* is being used.            */
          unsigned int  files_received;     /* No. of files received so  */
                                            /* far.                      */
          unsigned int  dir_options;
                                            /*+-----+-------------------+*/
                                            /*| Bit |      Meaning      |*/
                                            /*+-----+-------------------+*/
                                            /*|31-32| Not used.         |*/
                                            /*|   30| KEEP_PATH         |*/
                                            /*|   29| NO_DELIMITER      |*/
                                            /*|   28| URL_WITH_INDEX_FILE_NAME|*/
                                            /*|   27| URL_CREATES_FILE_NAME|*/
                                            /*|   26| ONE_PROCESS_JUST_SCANNING|*/
                                            /*|   25| Not used.         |*/
                                            /*|   24| INOTIFY_DELETE    |*/
                                            /*|   23| DO_NOT_MOVE       |*/
                                            /*|   22| DO_NOT_PARALLELIZE|*/
                                            /*|   21| Not used.         |*/
                                            /*|   20| INOTIFY_CREATE    |*/
                                            /*|   19| Not used.         |*/
                                            /*|   18| INOTIFY_ATTRIB    |*/
                                            /*|   17| Not used.         |*/
                                            /*|   16| INOTIFY_CLOSE     |*/
                                            /*|   15| INOTIFY_RENAME    |*/
                                            /*| 8-14| Not used.         |*/
                                            /*|    7| DONT_GET_DIR_LIST |*/
                                            /*|    6| ACCEPT_DOT_FILES  |*/
                                            /*|  1-5| Not used.         |*/
                                            /*+-----+-------------------+*/
          unsigned int  dir_flag;           /* Flag for this directory   */
                                            /* informing about the       */
                                            /* following status:         */
                                            /*+-----+-------------------+*/
                                            /*| Bit |      Meaning      |*/
                                            /*+-----+-------------------+*/
                                            /*|26-32| Not used.         |*/
                                            /*|   25| DIR_DISABLED_STATIC|*/
                                            /*|22-24| Not used.         |*/
                                            /*|   21| INFO_TIME_REACHED |*/
                                            /*|   20| Not used.         |*/
                                            /*|   19| INOTIFY_NEEDS_SCAN|*/
                                            /*|   18| Not used.         |*/
                                            /*|   17| ALL_DISABLED      |*/
                                            /*|15-16| Not used.         |*/
                                            /*|   14| DIR_STOPPED       |*/
                                            /*|   13| DIR_ERROR_OFFL_T  |*/
                                            /*|   12| DIR_ERROR_ACKN_T  |*/
                                            /*|   11| DIR_ERROR_OFFLINE |*/
                                            /*|   10| DIR_ERROR_ACKN    |*/
                                            /*|    9| WARN_TIME_REACHED |*/
                                            /*|    8| DIR_ERROR_SET     |*/
                                            /*|  6-7| Not used.         |*/
                                            /*|    5| DIR_DISABLED      |*/
                                            /*|    4| LINK_NO_EXEC      |*/
                                            /*|    3| Not used.         |*/
                                            /*|    2| FILES_IN_QUEUE    |*/
                                            /*|    1| MAX_COPIED        |*/
                                            /*+-----+-------------------+*/
          unsigned int  in_dc_flag;         /* Flag to indicate which of */
                                            /* the options have been     */
                                            /* stored in DIR_CONFIG. This*/
                                            /* is usefull for restoring  */
                                            /* the DIR_CONFIG from       */
                                            /* scratch. The following    */
                                            /* flags are possible:       */
                                            /*+-----+-------------------+*/
                                            /*| Bit |      Meaning      |*/
                                            /*+-----+-------------------+*/
                                            /*|18-32|Not used.          |*/
                                            /*|   18|CREATE_SRC_DIR_IDC |*/
                                            /*|   17|LOCAL_REMOTE_DIR_IDC|*/
                                            /*|   16|UNREADABLE_FILES_IDC|*/
                                            /*|   15|MAX_ERRORS_IDC     |*/
                                            /*|   14|INFO_TIME_IDC      |*/
                                            /*|   13|MAX_PROCESS_IDC    |*/
                                            /*|   12|DONT_DELUKW_FILES_IDC|*/
                                            /*|   11|INOTIFY_FLAG_IDC   |*/
                                            /*|   10|KEEP_CONNECTED_IDC |*/
                                            /*|    9|WARN_TIME_IDC      |*/
                                            /*|    8|MAX_CP_FILE_SIZE_ID|*/
                                            /*|    7|MAX_CP_FILES_IDC   |*/
                                            /*|    6|DONT_REPUKW_FILES_IDC|*/
                                            /*|    5|REPUKW_FILES_IDC   |*/
                                            /*|    4|OLD_LOCKED_FILES_IDC|*/
                                            /*|    3|QUEUED_FILES_IDC   |*/
                                            /*|    2|UNKNOWN_FILES_IDC  |*/
                                            /*|    1|DIR_ALIAS_IDC      |*/
                                            /*+-----+-------------------+*/
          unsigned int  files_in_dir;       /* The number of files       */
                                            /* currently in this         */
                                            /* directory.                */
          unsigned int  files_queued;       /* The number of files in    */
                                            /* the queue.                */
          unsigned int  accumulate;         /* How many files need to    */
                                            /* accumulate before start   */
                                            /* sending from this dir.    */
          unsigned int  max_copied_files;   /* Maximum number of files   */
                                            /* that we copy in one go.   */
          unsigned int  ignore_file_time;   /* Ignore files which are    */
                                            /* older, equal or newer     */
                                            /* the given time in sec.    */
          unsigned int  gt_lt_sign;         /* The sign for the following*/
                                            /* variables:                */
                                            /*     ignore_size           */
                                            /*     ignore_file_time      */
                                            /* These are bit masked      */
                                            /* for each variable.        */
                                            /*+-----+-------------------+*/
                                            /*| Bit |      Meaning      |*/
                                            /*+-----+-------------------+*/
                                            /*| 7-32| Not used.         |*/
                                            /*+-----+-------------------+*/
                                            /*|    6| IFTIME_GREATER_THEN|*/
                                            /*|    5| IFTIME_LESS_THEN  |*/
                                            /*|    4| IFTIME_EQUAL      |*/
                                            /*+-----+-------------------+*/
                                            /*|    3| ISIZE_GREATER_THEN|*/
                                            /*|    2| ISIZE_LESS_THEN   |*/
                                            /*|    1| ISIZE_EQUAL       |*/
                                            /*+-----+-------------------+*/
          unsigned int  keep_connected;     /* After all files have been */
                                            /* retrieved, the time to    */
                                            /* stay connected.           */
#ifdef WITH_DUP_CHECK                                                      
          unsigned int  dup_check_flag;     /* Flag storing the type of  */
                                            /* check that is to be done  */
                                            /* and what type of CRC to   */
                                            /* use:                      */
                                            /*+-----+-------------------+*/
                                            /*| Bit |      Meaning      |*/
                                            /*+-----+-------------------+*/
                                            /*|27-32| Not used.         |*/
                                            /*|   26| DC_WARN           |*/
                                            /*|   25| DC_STORE          |*/
                                            /*|   24| DC_DELETE         |*/
                                            /*|19-23| Not used.         |*/
                                            /*|   18| DC_MURMUR3        |*/
                                            /*|   17| DC_CRC32C         |*/
                                            /*|   16| DC_CRC32          |*/
                                            /*| 6-15| Not used.         |*/
                                            /*|    5| DC_FILENAME_AND_SIZE|*/
                                            /*|    4| DC_NAME_NO_SUFFIX |*/
                                            /*|    3| DC_FILE_CONT_NAME |*/
                                            /*|    2| DC_FILE_CONTENT   |*/
                                            /*|    1| DC_FILENAME_ONLY  |*/
                                            /*+-----+-------------------+*/
#endif
          mode_t        dir_mode;           /* Permission of directory   */
                                            /* when created via AFD.     */
          u_off_t       bytes_received;     /* No. of bytes received so  */
                                            /* far.                      */
          off_t         bytes_in_dir;       /* No. of bytes in this      */
                                            /* directory.                */
          off_t         bytes_in_queue;     /* No. of bytes in queue(s). */
          off_t         accumulate_size;    /* How many Bytes need to    */
                                            /* accumulate before we take */
                                            /* files from this dir.      */
          off_t         ignore_size;        /* Ignore any files less     */
                                            /* then, equal or greater    */
                                            /* then the given size.      */
          off_t         max_copied_file_size; /* The maximum number of   */
                                            /* bytes that we copy in one */
                                            /* go.                       */
#ifdef WITH_DUP_CHECK                                                      
          time_t        dup_check_timeout;  /* When the stored CRC for   */
                                            /* duplicate checks are no   */
                                            /* longer valid. Value is in */
                                            /* seconds.                  */
#endif
          time_t        last_retrieval;     /* Time of last retrieval.   */
          time_t        next_check_time;    /* Next time to check for    */
                                            /* files in directory.       */
          time_t        info_time;          /* Time when to inform that  */
                                            /* the directory has not     */
                                            /* received any data.        */
          time_t        warn_time;          /* Time when to warn that the*/
                                            /* directory has not received*/
                                            /* any data.                 */
          time_t        start_event_handle; /* Time when to set the      */
                                            /* specified event.          */
          time_t        end_event_handle;   /* Time when to remove the   */
                                            /* specified event.          */
          time_t        dir_mtime;          /* The modification time of  */
                                            /* monitored directory.      */
          int           unreadable_file_time; /* After how many hours    */
                                            /* can a unreadable file be  */
                                            /* deleted.                  */
          int           unknown_file_time;  /* After how many hours can  */
                                            /* a unknown file be deleted.*/
          int           queued_file_time;   /* After how many hours can  */
                                            /* a queued file be deleted. */
          int           locked_file_time;   /* After how many hours can  */
                                            /* a locked file be deleted. */
          int           end_character;      /* Only pick up files where  */
                                            /* the last charachter (of   */
                                            /* the content) contains     */
                                            /* this character. A -1      */
                                            /* means not to check the    */
                                            /* last character.           */
          unsigned int  dir_id;             /* Unique number to identify */
                                            /* directory faster and      */
                                            /* easier.                   */
          int           fsa_pos;            /* Position of this host in  */
                                            /* FSA, to get the data that */
                                            /* are in the HOST_CONFIG.   */
          int           no_of_process;      /* The number of process     */
                                            /* that currently process    */
                                            /* data for this directory.  */
          int           max_process;        /* The maximum number of     */
                                            /* process that may be       */
                                            /* forked for this directory.*/
          int           max_errors;         /* Max errors before we ring */
                                            /* the alarm bells.          */
          unsigned int  error_counter;      /* The number of errors when */
                                            /* trying to access this     */
                                            /* directory. Will be set to */
                                            /* zero after each succesfull*/
                                            /* access.                   */
       };

/* Bit map flag for AMG and FD communication. */
#define DIR_CHECK_ACTIVE            1
#define REREADING_DIR_CONFIG        2
#define FD_WAITING                  4
#define PAUSE_DISTRIBUTION          8
#define WRITTING_JID_STRUCT         64
#define CHECK_FILE_DIR_ACTIVE       128

/*
 * Structure that holds status of all process.
 *
 * NOTE: If any change is done to the structure below, do not forgot
 *       to do the changes in function get_afd_status_struct_size()!!!
 */
struct afd_status
       {
          signed char    amg;             /* Automatic Message Generator, */
                                          /* can have the following       */
                                          /* values:                      */
                                          /*  -3 - Process has been       */
                                          /*       stopped normally.      */
                                          /*   0 - Not running.           */
                                          /*   1 - Process is running.    */
                                          /*  19 - Shutting down.         */
          unsigned char  amg_jobs;        /* Bitmap to show if jobs of    */
                                          /* the AMG (dir_check(),        */
                                          /* time_job(), ...) are active: */
                                          /*+-----+----------------------+*/
                                          /*| Bit |       Meaning        |*/
                                          /*+-----+----------------------+*/
                                          /*|  1  | dir_check() active   |*/
                                          /*|  2  | Rereading DIR_CONFIG |*/
                                          /*|  3  | FD waiting for AMG   |*/
                                          /*|     | to finish DIR_CONFIG |*/
                                          /*|  4  | Pause distribution   |*/
                                          /*|     | at start.            |*/
                                          /*|  5  | not used             |*/
                                          /*|  6  | dir_check() has msg  |*/
                                          /*|     | queued.              |*/
                                          /*|  7  | AMG writting to      |*/
                                          /*|     | JID structure.       |*/
                                          /*|  8  | Check file directory |*/
                                          /*|     | for jobs without a   |*/
                                          /*|     | message.             |*/
                                          /*+-----+----------------------+*/
          signed char    fd;              /* File Distributor             */
          signed char    sys_log;         /* System Log                   */
          signed char    maintainer_log;  /* Maintainer Log               */
          signed char    event_log;       /* Event Log                    */
          signed char    receive_log;     /* Receive Log                  */
          signed char    trans_log;       /* Transfer Log                 */
          signed char    trans_db_log;    /* Transfer Debug Log           */
          signed char    archive_watch;
          signed char    afd_stat;        /* Statistic program            */
          signed char    afdd;
          signed char    afdds;
#ifdef _WITH_ATPD_SUPPORT
          signed char    atpd;            /* AFD Transfer Protocol Daemon.*/
#endif
#ifdef _WITH_WMOD_SUPPORT
          signed char    wmod;            /* WMO Protocol Daemon.         */
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
          signed char    demcd;           /* De Mail Confirmation Daemon. */
#endif
#ifndef HAVE_MMAP
          signed char    mapper;
#endif
#ifdef _INPUT_LOG
          signed char    input_log;
#endif
#ifdef _DISTRIBUTION_LOG
          signed char    distribution_log;
#endif
#ifdef _OUTPUT_LOG
          signed char    output_log;
#endif
#ifdef _CONFIRMATION_LOG
          signed char    confirmation_log;
#endif
#ifdef _DELETE_LOG
          signed char    delete_log;
#endif
#ifdef _PRODUCTION_LOG
          signed char    production_log;
#endif
#ifdef _TRANSFER_RATE_LOG
          signed char    transfer_rate_log;
#endif
#if defined (_INPUT_LOG) || defined (_OUTPUT_LOG) || defined (_CONFIRMATION_LOG) || defined (_DELETE_LOG) || defined (_PRODUCTION_LOG) || defined (_DISTRIBUTION_LOG)
          signed char    aldad;
#endif
          signed char    afd_worker;
          unsigned int   sys_log_ec;         /* System log entry counter. */
          char           sys_log_fifo[LOG_FIFO_SIZE + 1];
          char           sys_log_history[MAX_LOG_HISTORY];
          unsigned int   receive_log_ec;     /* Receive log entry counter.*/
          char           receive_log_fifo[LOG_FIFO_SIZE + 1];
          char           receive_log_history[MAX_LOG_HISTORY];
          unsigned int   trans_log_ec;       /* Transfer log entry        */
                                             /* counter.                  */
          char           trans_log_fifo[LOG_FIFO_SIZE + 1];
          char           trans_log_history[MAX_LOG_HISTORY];
          char           hostname[MAX_REAL_HOSTNAME_LENGTH];
          char           work_dir[MAX_PATH_LENGTH];
          uid_t          user_id;
          int            no_of_transfers;    /* The number of active      */
                                             /* transfers system wide.    */
          int            no_of_retrieves;    /* The number of process     */
                                             /* that may collect files.   */
          nlink_t        jobs_in_queue;      /* The number of jobs still  */
                                             /* to be done by the FD.     */
          time_t         start_time;         /* Time when AFD was started.*/
                                             /* This value is used for    */
                                             /* eval_database() so it     */
                                             /* when it has started the   */
                                             /* first time.               */
          unsigned int   fd_fork_counter;    /* Number of forks() by FD.  */
          unsigned int   amg_fork_counter;   /* Number of forks() by AMG. */
          unsigned int   burst2_counter;     /* Number of burst2 done by  */
                                             /* FD.                       */
          unsigned int   max_queue_length;   /* Max. FD queue length.     */
          unsigned int   dir_scans;          /* Number of directory scans.*/
#ifdef WITH_INOTIFY
          unsigned int   inotify_events;     /* Inotify events handled.   */
#endif
#ifdef HAVE_WAIT4
          struct timeval amg_child_utime;    /* AMG child user CPU time.  */
          struct timeval amg_child_stime;    /* AMG child system CPU time.*/
          struct timeval fd_child_utime;     /* FD  child user CPU time.  */
          struct timeval fd_child_stime;     /* FD  child system CPU time.*/
#endif
       };

/* Structure that holds important data for restoring if database changes. */
struct system_data
       {
          unsigned char  fsa_feature_flag;
          unsigned char  fra_feature_flag;
          unsigned int   sys_log_ec;         /* System log entry counter. */
          char           sys_log_fifo[LOG_FIFO_SIZE + 1];
          char           sys_log_history[MAX_LOG_HISTORY];
          unsigned int   receive_log_ec;     /* Receive log entry counter.*/
          char           receive_log_fifo[LOG_FIFO_SIZE + 1];
          char           receive_log_history[MAX_LOG_HISTORY];
          unsigned int   trans_log_ec;       /* Transfer log entry        */
                                             /* counter.                  */
          char           trans_log_fifo[LOG_FIFO_SIZE + 1];
          char           trans_log_history[MAX_LOG_HISTORY];
          unsigned int   fd_fork_counter;    /* Number of forks() by FD.  */
          unsigned int   amg_fork_counter;   /* Number of forks() by AMG. */
          unsigned int   burst2_counter;     /* Number of burst2 done by  */
                                             /* FD.                       */
          unsigned int   max_queue_length;   /* Max. FD queue length.     */
          unsigned int   dir_scans;          /* Number of directory scans.*/
#ifdef WITH_INOTIFY
          unsigned int   inotify_events;     /* Inotify events handled.   */
#endif
#ifdef HAVE_WAIT4
          struct timeval amg_child_utime;    /* AMG child user CPU time.  */
          struct timeval amg_child_stime;    /* AMG child system CPU time.*/
          struct timeval fd_child_utime;     /* FD  child user CPU time.  */
          struct timeval fd_child_stime;     /* FD  child system CPU time.*/
#endif
       };

/* Structure that holds all relevant information of */
/* all process that have been started by the AFD.   */
struct proc_table
       {
          pid_t       pid;
          signed char *status;
          char        proc_name[MAX_PROCNAME_LENGTH];
       };

/* Definitions for renaming. */
#define READ_RULES_INTERVAL        30          /* in seconds             */
#define MAX_RULE_HEADER_LENGTH     80
struct rule
       {
          int  no_of_rules;
          char header[MAX_RULE_HEADER_LENGTH + 1];
          char **filter;
          char **rename_to;
       };

/* Definition for structure that holds all data for one job ID. */
#define CURRENT_JID_VERSION 2
struct job_id_data
       {
#ifdef _NEW_JID
          time_t       creation_time;   /* Time when job was created.    */
          unsigned int special_flag;    /*+-----+-----------------------+*/
                                        /*| Bit |        Meaning        |*/
                                        /*+-----+-----------------------+*/
                                        /*| 1-32| Not used.             |*/
                                        /*+-----+-----------------------+*/
#endif
          unsigned int job_id;          /* CRC-32 checksum of the job.   */
          unsigned int dir_id;          /* CRC-32 checksum of the dir.   */
          unsigned int file_mask_id;    /* CRC-32 checksum of file masks.*/
          unsigned int dir_config_id;   /* CRC-32 checksum of DIR_CONFIG.*/
          unsigned int host_id;         /* CRC-32 checksum of host_alias.*/
          unsigned int recipient_id;    /* CRC-32 checksum of recipient. */
          int          dir_id_pos;      /* Position of the directory     */
                                        /* name in the structure         */
                                        /* dir_name_buf.                 */
          int          no_of_loptions;
          int          no_of_soptions;
#ifdef _NEW_JID
          char         loptions[MAX_NO_OPTIONS * MAX_OPTION_LENGTH];
#else
          char         loptions[MAX_OPTION_LENGTH];
#endif
          char         soptions[MAX_OPTION_LENGTH]; /* NOTE: The last    */
                                        /* character is used to change   */
                                        /* CRC value in the very unusal  */
                                        /* case when there is one CRC    */
                                        /* for two or more jobs.         */
          char         recipient[MAX_RECIPIENT_LENGTH];
          char         host_alias[MAX_HOSTNAME_LENGTH + 1];
          char         priority;
       };
#define CURRENT_DNB_VERSION 1
struct dir_name_buf
       {
          char         dir_name[MAX_PATH_LENGTH];/* Full directory name. */
          char         orig_dir_name[MAX_PATH_LENGTH]; /* Directory name */
                                            /* as it is in DIR_CONFIG.   */
          unsigned int dir_id;              /* Unique number to identify */
                                            /* directory faster and      */
                                            /* easier.                   */
       };
#define CURRENT_PWB_VERSION 0
#define PWB_STEP_SIZE       20
struct passwd_buf
       {
          char          uh_name[MAX_USER_NAME_LENGTH + MAX_REAL_HOSTNAME_LENGTH + 1];
          unsigned char passwd[MAX_USER_NAME_LENGTH];
          signed char   dup_check;
       };

#ifdef WHEN_WE_KNOW
/* Definition for structure that holds all status and statistic for one job ID. */
#define CURRENT_JIS_VERSION 0
struct job_id_stat
       {
          double       nbs;             /* Number of bytes send.         */
          time_t       creation_time;   /* Time when job was created.    */
          time_t       usage_time;      /* Last time this job was used.  */
          unsigned int special_flag;    /*+-----+-----------------------+*/
                                        /*| Bit |        Meaning        |*/
                                        /*+-----+-----------------------+*/
                                        /*| 1-32| Not used.             |*/
                                        /*+-----+-----------------------+*/
          unsigned int nfs;             /* Number of files send.         */
          unsigned int ne;              /* Number of errors.             */
       };
#endif /* WHEN_WE_KNOW */

/* Structure to hold a message of process FD. */
#define MSG_QUE_BUF_SIZE  10000
#define RESEND_JOB        2
#define HELPER_JOB        4
#define FETCH_JOB         8
#ifdef _WITH_BURST_MISS_CHECK
# define QUEUED_FOR_BURST 16
#endif
struct queue_buf
       {
          double        msg_number;
          pid_t         pid;
          time_t        creation_time;
          off_t         file_size_to_send;
          unsigned int  files_to_send;
          unsigned int  retries;            /* The number of times it    */
                                            /* was tried to send this    */
                                            /* job.                      */
          int           pos;                /* Position in msg_cache_buf */
                                            /* for sf_xxx or position in */
                                            /* FRA for gf_xxx.           */
          int           connect_pos;
          unsigned char special_flag;       /*+-----+-------------------+*/
                                            /*| Bit |      Meaning      |*/
                                            /*+-----+-------------------+*/
                                            /*| 6-8 | Not used.         |*/
#ifdef _WITH_BURST_MISS_CHECK
                                            /*|   5 | QUEUED_FOR_BURST  |*/
#endif
                                            /*|   4 | FETCH_JOB         |*/
                                            /*|   3 | HELPER_JOB        |*/
                                            /*|   2 | RESEND_JOB        |*/
                                            /*|   1 | Not used.         |*/
                                            /*+-----+-------------------+*/
          char          msg_name[MAX_MSG_NAME_LENGTH];
#ifdef HAVE_SETPRIORITY
                                            /* NOTE: At position         */
                                            /* MAX_MSG_NAME_LENGTH - 1 we*/
                                            /* store the priority of the */
                                            /* job. It is pretty save to */
                                            /* do, since it is highly    */
                                            /* unlikely that dir_no and  */
                                            /* and split_job_number ever */
                                            /* reach 0xffffffff.         */
#endif
       };
#ifdef SF_BURST_ACK
#define ACK_QUE_BUF_SIZE  4000
#define ACK_QUE_TIMEOUT   60                /* in seconds                */
struct ack_queue_buf
       {
          time_t        insert_time;
          char          msg_name[MAX_MSG_NAME_LENGTH];
       };
#endif

/* The file mask structure is not a structure but a collection of */
/* ints, unsigned ints and chars. See amg/lookup_file_mask_id.c   */
/* for more details.                                              */
#define CURRENT_FMD_VERSION 0

/* Definition for structure that holds data for different DIR_CONFIG's. */
#define CURRENT_DCID_VERSION 0
struct dir_config_list
       {
          unsigned int dc_id;
          char         dir_config_file[MAX_PATH_LENGTH];
       };

struct delete_log
       {
          int           fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
          int           readfd;
#endif
          unsigned int  *job_id;
          unsigned int  *dir_id;
          time_t        *input_time;
          unsigned int  *split_job_counter;
          unsigned int  *unique_number;
          char          *data;
          char          *file_name;
          unsigned char *file_name_length;
          off_t         *file_size;
          char          *host_name;
          size_t        size;
       };

#ifdef WITH_DUP_CHECK
/* Definition for structure holding CRC values for duplicate checks. */
# define INITIAL_CRC             ~0u
# define CRC_STEP_SIZE           1000
# define DUPCHECK_MAX_CHECK_TIME 40
# define DUPCHECK_MIN_CHECK_TIME 5
struct crc_buf
       {
          unsigned int crc;
          unsigned int flag;
          time_t       timeout;
       };
#endif

struct dir_options
       {
          int  no_of_dir_options;
          char aoptions[MAX_NO_OPTIONS + 1][LOCAL_REMOTE_DIR_ID_LENGTH + 1 + MAX_OPTION_LENGTH];
          char dir_alias[MAX_DIR_ALIAS_LENGTH + 1];
          char url[MAX_PATH_LENGTH];
       };

/* Structure holding all filenames that are/have been retrieved. */
#ifdef _WITH_EXTRA_CHECK
# define CURRENT_RL_VERSION       3
#else
# define CURRENT_RL_VERSION       2
#endif
#define RETRIEVE_LIST_STEP_SIZE   50
#ifdef _WITH_EXTRA_CHECK
# define MAX_EXTRA_LS_DATA_LENGTH 90
#endif

#define RL_GOT_SIZE_DATE          1
#define RL_GOT_EXACT_SIZE         2   /* Exact -> resolution in byte.    */
#define RL_GOT_EXACT_DATE         4   /* Exact -> resolution in seconds. */
struct retrieve_list
       {
          char          file_name[MAX_FILENAME_LENGTH];
#ifdef _WITH_EXTRA_CHECK
          char          extra_data[MAX_EXTRA_LS_DATA_LENGTH]; /* Store some*/
                                         /* extra information that might   */
                                         /* be useful depnding on protocol */
                                         /* like: ETAG, CRC, etc.          */
#endif
          unsigned char assigned;        /* Which job has been assigned to */
                                         /* fetch these files.             */
          unsigned char special_flag;    /*+-----+------------------------+*/
                                         /*| Bit |        Meaning         |*/
                                         /*+-----+------------------------+*/
                                         /*| 4-32| Not used.              |*/
                                         /*|    3| RL_GOT_EXACT_DATE      |*/
                                         /*|    2| RL_GOT_EXACT_SIZE      |*/
                                         /*|    1| RL_GOT_SIZE_DATE       |*/
                                         /*+-----+------------------------+*/
          char          got_date;
          char          retrieved;       /* Has the file already been      */
                                         /* retrieved?                     */
          char          in_list;         /* Used to remove any files from  */
                                         /* the list that are no longer at */
                                         /* the remote host.               */
          off_t         size;            /* Size of the file.              */
          off_t         prev_size;       /* Previous size of the file.     */
          time_t        file_mtime;      /* Modification time of file.     */
       };

/* For compatibility reasons we must still know the retrieve_list from */
/* 1.2.x so we can convert the old list.                               */
#define OLD_MAX_FTP_DATE_LENGTH 15
struct old_retrieve_list
       {
          char  file_name[MAX_FILENAME_LENGTH];
          char  date[OLD_MAX_FTP_DATE_LENGTH];
          char  retrieved;
          char  in_list;
          off_t size;
       };
struct old_int_retrieve_list
       {
          char file_name[MAX_FILENAME_LENGTH];
          char date[OLD_MAX_FTP_DATE_LENGTH];
          char retrieved;
          char in_list;
          int  size;
       };

/* Runtime array. */
#ifdef OLD_RT_ARRAY
#define RT_ARRAY(name, rows, columns, type)                              \
        {                                                                \
           int macro_row_counter;                                        \
                                                                         \
           if ((name = (type **)calloc((rows), sizeof(type *))) == NULL) \
           {                                                             \
              system_log(FATAL_SIGN, __FILE__, __LINE__,                 \
                         "calloc() error : %s", strerror(errno));        \
              exit(INCORRECT);                                           \
           }                                                             \
                                                                         \
           if (((name)[0] = (type *)calloc(((rows) * (columns)),         \
                                           sizeof(type))) == NULL)       \
           {                                                             \
              system_log(FATAL_SIGN, __FILE__, __LINE__,                 \
                         "calloc() error : %s", strerror(errno));        \
              exit(INCORRECT);                                           \
           }                                                             \
                                                                         \
           for (macro_row_counter = 1; macro_row_counter < (rows); macro_row_counter++) \
              (name)[macro_row_counter] = ((name)[0] + (macro_row_counter * (columns)));\
        }
#else
#define RT_ARRAY(name, rows, columns, type)                              \
        {                                                                \
           int macro_row_counter;                                        \
                                                                         \
           if ((name = (type **)malloc(((rows) * sizeof(type *)))) == NULL)\
           {                                                             \
              system_log(FATAL_SIGN, __FILE__, __LINE__,                 \
                         "malloc() error : %s", strerror(errno));        \
              exit(INCORRECT);                                           \
           }                                                             \
                                                                         \
           if (((name)[0] = (type *)malloc(((rows) * (columns) *         \
                                           sizeof(type)))) == NULL)      \
           {                                                             \
              system_log(FATAL_SIGN, __FILE__, __LINE__,                 \
                         "malloc() error : %s", strerror(errno));        \
              exit(INCORRECT);                                           \
           }                                                             \
                                                                         \
           for (macro_row_counter = 1; macro_row_counter < (rows); macro_row_counter++) \
              (name)[macro_row_counter] = ((name)[0] + (macro_row_counter * (columns)));\
        }
#endif
#define FREE_RT_ARRAY(name) \
        {                   \
           free((name)[0]); \
           free((name));    \
        }
#define REALLOC_RT_ARRAY(name, rows, columns, type)                         \
        {                                                                   \
           int  macro_row_counter;                                          \
           char *macro_ptr = (name)[0];                                     \
                                                                            \
           if (((name) = (type **)realloc((name), (rows) * sizeof(type *))) == NULL)\
           {                                                                \
              system_log(FATAL_SIGN, __FILE__, __LINE__,                    \
                         "realloc() error : %s", strerror(errno));          \
              exit(INCORRECT);                                              \
           }                                                                \
                                                                            \
           if (((name)[0] = (type *)realloc(macro_ptr,                      \
                             (((rows) * (columns)) * sizeof(type)))) == NULL)\
           {                                                                \
              system_log(FATAL_SIGN, __FILE__, __LINE__,                    \
                         "realloc() error : %s", strerror(errno));          \
              exit(INCORRECT);                                              \
           }                                                                \
                                                                            \
           for (macro_row_counter = 1; macro_row_counter < (rows); macro_row_counter++) \
              (name)[macro_row_counter] = ((name)[0] + (macro_row_counter * (columns)));\
        }

/* Runtime pointer array. */
#define RT_P_ARRAY(name, rows, columns, type)                               \
        {                                                                   \
           register int macro_i;                                            \
                                                                            \
           if (((name) = (type ***)malloc((rows) * sizeof(type **))) == NULL)\
           {                                                                \
              system_log(FATAL_SIGN, __FILE__, __LINE__,                    \
                         "malloc() error : %s", strerror(errno));           \
              exit(INCORRECT);                                              \
           }                                                                \
           if (((name)[0] = (type **)malloc((rows) * (columns) * sizeof(type *))) == NULL)\
           {                                                                \
              system_log(FATAL_SIGN, __FILE__, __LINE__,                    \
                         "malloc() error : %s", strerror(errno));           \
              exit(INCORRECT);                                              \
           }                                                                \
           for (macro_i = 0; macro_i < (rows); macro_i++)                   \
           {                                                                \
              (name)[macro_i] = (name)[0] + (macro_i * (columns));          \
           }                                                                \
        }
#define FREE_RT_P_ARRAY(name)   \
        {                       \
           free((name));        \
           free((name)[0]);     \
        }

/* Macro that does do a strncpy without filling up rest with binary zeros. */
#define STRNCPY(dest, src, n)                          \
        {                                              \
           register int macro_i;                       \
                                                       \
           for (macro_i = 0; macro_i < (n); macro_i++) \
           {                                           \
              (dest)[macro_i] = (src)[macro_i];        \
              if ((src)[macro_i] == '\0')              \
              {                                        \
                 break;                                \
              }                                        \
           }                                           \
        }

/* Macro that positions pointer just after binary zero. */
#define NEXT(ptr)                 \
        {                         \
           while (*(ptr) != '\0') \
           {                      \
              (ptr)++;            \
           }                      \
           (ptr)++;               \
        }

#ifdef LOCK_DEBUG
#define ABS_REDUCE_QUEUE(fra_pos, files, bytes)              \
        {                                                    \
           unsigned int tmp_files;                           \
                                                             \
           lock_region_w(fra_fd,                             \
                         (char *)&fra[(fra_pos)].files_queued - (char *)fra, __FILE__, __LINE__);\
           tmp_files = fra[(fra_pos)].files_queued;          \
           fra[(fra_pos)].files_queued -= (files);           \
           if (fra[(fra_pos)].files_queued > tmp_files)      \
           {                                                 \
              system_log(DEBUG_SIGN, __FILE__, __LINE__,     \
                         "Files queued overflowed (%u - %u) for FRA pos %d.", \
                         tmp_files, (files), (fra_pos));     \
              fra[(fra_pos)].files_queued = 0;               \
           }                                                 \
           if ((fra[(fra_pos)].files_queued == 0) &&         \
               (fra[(fra_pos)].dir_flag & FILES_IN_QUEUE))   \
           {                                                 \
              fra[(fra_pos)].dir_flag ^= FILES_IN_QUEUE;     \
           }                                                 \
           fra[(fra_pos)].bytes_in_queue -= (bytes);         \
           if (fra[(fra_pos)].bytes_in_queue < 0)            \
           {                                                 \
              system_log(DEBUG_SIGN, __FILE__, __LINE__,     \
                         "Bytes queued overflowed for FRA pos %d.",\
                         (fra_pos));                         \
              fra[(fra_pos)].bytes_in_queue = 0;             \
           }                                                 \
           unlock_region(fra_fd,                             \
                         (char *)&fra[(fra_pos)].files_queued - (char *)fra, __FILE__, __LINE__);\
        }
#else
#define ABS_REDUCE_QUEUE(fra_pos, files, bytes)              \
        {                                                    \
           unsigned int tmp_files;                           \
                                                             \
           lock_region_w(fra_fd,                             \
                         (char *)&fra[(fra_pos)].files_queued - (char *)fra);\
           tmp_files = fra[(fra_pos)].files_queued;          \
           fra[(fra_pos)].files_queued -= (files);           \
           if (fra[(fra_pos)].files_queued > tmp_files)      \
           {                                                 \
              system_log(DEBUG_SIGN, __FILE__, __LINE__,     \
                         "Files queued overflowed (%u - %u) for FRA pos %d.", \
                         tmp_files, (files), (fra_pos));     \
              fra[(fra_pos)].files_queued = 0;               \
           }                                                 \
           if ((fra[(fra_pos)].files_queued == 0) &&         \
               (fra[(fra_pos)].dir_flag & FILES_IN_QUEUE))   \
           {                                                 \
              fra[(fra_pos)].dir_flag ^= FILES_IN_QUEUE;     \
           }                                                 \
           fra[(fra_pos)].bytes_in_queue -= (bytes);         \
           if (fra[(fra_pos)].bytes_in_queue < 0)            \
           {                                                 \
              system_log(DEBUG_SIGN, __FILE__, __LINE__,     \
                         "Bytes queued overflowed for FRA pos %d.",\
                         (fra_pos));                         \
              fra[(fra_pos)].bytes_in_queue = 0;             \
           }                                                 \
           unlock_region(fra_fd,                             \
                         (char *)&fra[(fra_pos)].files_queued - (char *)fra);\
        }
#endif
#define SET_DIR_STATUS(flag, current_time, start_event_handle, end_event_handle, status)\
        {                                       \
           if ((flag) & DIR_DISABLED)           \
           {                                    \
              (status) = DISABLED;              \
           }                                    \
           else if ((flag) & DIR_STOPPED)       \
                {                               \
                   (status) = DISCONNECTED;     \
                }                               \
           else if ((flag) & DIR_ERROR_SET)     \
                {                               \
                   if (((flag) & DIR_ERROR_OFFLINE) ||\
                       (((flag) & DIR_ERROR_OFFL_T) &&\
                        (((start_event_handle) == 0) ||\
                         ((current_time) >= (start_event_handle))) &&\
                        (((end_event_handle) == 0) ||\
                         ((current_time) <= (end_event_handle)))))\
                   {                            \
                      (status) = ERROR_OFFLINE_ID;\
                   }                            \
                   else if (((flag) & DIR_ERROR_ACKN) ||\
                            (((flag) & DIR_ERROR_ACKN_T) &&\
                             (((start_event_handle) == 0) ||\
                              ((current_time) >= (start_event_handle))) &&\
                             (((end_event_handle) == 0) ||\
                              ((current_time) <= (end_event_handle)))))\
                        {                       \
                           (status) = ERROR_ACKNOWLEDGED_ID;\
                        }                       \
                        else                    \
                        {                       \
                           (status) = NOT_WORKING2;\
                        }                       \
                }                               \
           else if ((flag) & WARN_TIME_REACHED) \
                {                               \
                   (status) = WARNING_ID;       \
                }                               \
                else                            \
                {                               \
                   (status) = NORMAL_STATUS;    \
                }                               \
        }

#define ABS_REDUCE_GLOBAL(value)\
        {                  \
           (value) -= 1;   \
           if ((value) < 0)\
           {               \
              (value) = 0; \
           }               \
        }

/* Macro to show how many files and bytes where send/received. */
#if SIZEOF_OFF_T == 4
# define WHAT_DONE_BUFFER(length, buffer, how, file_size_done, no_of_files_done)\
        {                                                      \
           if ((file_size_done) >= EXABYTE)                    \
           {                                                   \
              (length) = snprintf((buffer), (length), "%.3f EiB (%lu bytes) %s in %d file(s).",\
                               (double)(file_size_done) / EXABYTE,\
                               (file_size_done), (how),        \
                               (no_of_files_done));            \
           }                                                   \
           else if ((file_size_done) >= PETABYTE)              \
                {                                              \
                   (length) = snprintf((buffer), (length), "%.3f PiB (%lu bytes) %s in %d file(s).",\
                                    (double)(file_size_done) / PETABYTE,\
                                    (file_size_done), (how),   \
                                    (no_of_files_done));       \
                }                                              \
           else if ((file_size_done) >= TERABYTE)              \
                {                                              \
                   (length) = snprintf((buffer), (length), "%.3f TiB (%lu bytes) %s in %d file(s).",\
                                    (double)(file_size_done) / TERABYTE,\
                                    (file_size_done), (how),   \
                                    (no_of_files_done));       \
                }                                              \
           else if ((file_size_done) >= GIGABYTE)              \
                {                                              \
                   (length) = snprintf((buffer), (length), "%.3f GiB (%lu bytes) %s in %d file(s).",\
                                    (double)(file_size_done) / GIGABYTE,\
                                    (file_size_done), (how),   \
                                    (no_of_files_done));       \
                }                                              \
           else if ((file_size_done) >= MEGABYTE)              \
                {                                              \
                   (length) = snprintf((buffer), (length), "%.3f MiB (%lu bytes) %s in %d file(s).",\
                                    (double)(file_size_done) / MEGABYTE,\
                                    (file_size_done), (how),   \
                                    (no_of_files_done));       \
                }                                              \
           else if ((file_size_done) >= KILOBYTE)              \
                {                                              \
                   (length) = snprintf((buffer), (length), "%.3f KiB (%lu bytes) %s in %d file(s).",\
                                    (double)(file_size_done) / KILOBYTE,\
                                    (file_size_done), (how),   \
                                    (no_of_files_done));       \
                }                                              \
                else                                           \
                {                                              \
                   (length) = snprintf((buffer), (length), "%lu bytes %s in %d file(s).",  \
                                    (file_size_done), (how),   \
                                    (no_of_files_done));       \
                }                                              \
        }
# define WHAT_DONE(how, file_size_done, no_of_files_done)      \
        {                                                      \
           if ((file_size_done) >= EXABYTE)                    \
           {                                                   \
              trans_log(INFO_SIGN, NULL, 0, NULL, NULL,        \
                        "%.3f EiB (%ld bytes) %s in %d file(s).",\
                        (double)(file_size_done) / EXABYTE,    \
                        (pri_off_t)(file_size_done), (how),    \
                        (no_of_files_done));                   \
           }                                                   \
           else if ((file_size_done) >= PETABYTE)              \
                {                                              \
                   trans_log(INFO_SIGN, NULL, 0, NULL, NULL,   \
                             "%.3f PiB (%ld bytes) %s in %d file(s).",\
                             (double)(file_size_done) / PETABYTE,\
                             (pri_off_t)(file_size_done),      \
                             (how), (no_of_files_done));       \
                }                                              \
           else if ((file_size_done) >= TERABYTE)              \
                {                                              \
                   trans_log(INFO_SIGN, NULL, 0, NULL, NULL,   \
                             "%.3f TiB (%ld bytes) %s in %d file(s).",\
                             (double)(file_size_done) / TERABYTE,\
                             (pri_off_t)(file_size_done),      \
                             (how), (no_of_files_done));       \
                }                                              \
           else if ((file_size_done) >= GIGABYTE)              \
                {                                              \
                   trans_log(INFO_SIGN, NULL, 0, NULL, NULL,   \
                             "%.3f GiB (%ld bytes) %s in %d file(s).",\
                             (double)(file_size_done) / GIGABYTE,\
                             (pri_off_t)(file_size_done),      \
                             (how), (no_of_files_done));       \
                }                                              \
           else if ((file_size_done) >= MEGABYTE)              \
                {                                              \
                   trans_log(INFO_SIGN, NULL, 0, NULL, NULL,   \
                             "%.3f MiB (%ld bytes) %s in %d file(s).",\
                             (double)(file_size_done) / MEGABYTE,\
                             (pri_off_t)(file_size_done),      \
                             (how), (no_of_files_done));       \
                }                                              \
           else if ((file_size_done) >= KILOBYTE)              \
                {                                              \
                   trans_log(INFO_SIGN, NULL, 0, NULL, NULL,   \
                             "%.3f KiB (%ld bytes) %s in %d file(s).",\
                             (double)(file_size_done) / KILOBYTE,\
                             (pri_off_t)(file_size_done),      \
                             (how), (no_of_files_done));       \
                }                                              \
                else                                           \
                {                                              \
                   trans_log(INFO_SIGN, NULL, 0, NULL, NULL,   \
                             "%ld bytes %s in %d file(s).",    \
                             (pri_off_t)(file_size_done),      \
                             (how), (no_of_files_done));       \
                }                                              \
        }
#else
# define WHAT_DONE_BUFFER(length, buffer, how, file_size_done, no_of_files_done)\
        {                                                      \
           if ((file_size_done) >= EXABYTE)                    \
           {                                                   \
              (length) = snprintf((buffer), (length), "%.3f EiB (%llu bytes) %s in %d file(s).",\
                               (double)(file_size_done) / EXABYTE,\
                               (file_size_done), (how), (no_of_files_done));\
           }                                                   \
           else if ((file_size_done) >= PETABYTE)              \
                {                                              \
                   (length) = snprintf((buffer), (length), "%.3f PiB (%llu bytes) %s in %d file(s).",\
                                    (double)(file_size_done) / PETABYTE,\
                                    (file_size_done), (how),   \
                                    (no_of_files_done));       \
                }                                              \
           else if ((file_size_done) >= TERABYTE)              \
                {                                              \
                   (length) = snprintf((buffer), (length), "%.3f TiB (%llu bytes) %s in %d file(s).",\
                                    (double)(file_size_done) / TERABYTE,\
                                    (file_size_done), (how),   \
                                    (no_of_files_done));       \
                }                                              \
           else if ((file_size_done) >= GIGABYTE)              \
                {                                              \
                   (length) = snprintf((buffer), (length), "%.3f GiB (%llu bytes) %s in %d file(s).",\
                                    (double)(file_size_done) / GIGABYTE,\
                                    (file_size_done), (how),   \
                                    (no_of_files_done));       \
                }                                              \
           else if ((file_size_done) >= MEGABYTE)              \
                {                                              \
                   (length) = snprintf((buffer), (length), "%.3f MiB (%llu bytes) %s in %d file(s).",\
                                    (double)(file_size_done) / MEGABYTE,\
                                    (file_size_done), (how),   \
                                    (no_of_files_done));       \
                }                                              \
           else if ((file_size_done) >= KILOBYTE)              \
                {                                              \
                   (length) = snprintf((buffer), (length), "%.3f KiB (%llu bytes) %s in %d file(s).",\
                                    (double)(file_size_done) / KILOBYTE,\
                                    (file_size_done), (how),   \
                                    (no_of_files_done));       \
                }                                              \
                else                                           \
                {                                              \
                   (length) = snprintf((buffer), (length), "%llu bytes %s in %d file(s).",\
                                    (file_size_done), (how),   \
                                    (no_of_files_done));       \
                }                                              \
        }
# define WHAT_DONE(how, file_size_done, no_of_files_done)      \
        {                                                      \
           if ((file_size_done) >= EXABYTE)                    \
           {                                                   \
              trans_log(INFO_SIGN, NULL, 0, NULL, NULL,        \
                        "%.3f EiB (%lld bytes) %s in %d file(s).",\
                        (double)(file_size_done) / EXABYTE,    \
                        (pri_off_t)(file_size_done), (how),    \
                        (no_of_files_done));                   \
           }                                                   \
           else if ((file_size_done) >= PETABYTE)              \
                {                                              \
                   trans_log(INFO_SIGN, NULL, 0, NULL, NULL,   \
                             "%.3f PiB (%lld bytes) %s in %d file(s).",\
                             (double)(file_size_done) / PETABYTE,\
                             (pri_off_t)(file_size_done), (how),\
                             (no_of_files_done));              \
                }                                              \
           else if ((file_size_done) >= TERABYTE)              \
                {                                              \
                   trans_log(INFO_SIGN, NULL, 0, NULL, NULL,   \
                             "%.3f TiB (%lld bytes) %s in %d file(s).",\
                             (double)(file_size_done) / TERABYTE,\
                             (pri_off_t)(file_size_done), (how),\
                             (no_of_files_done));              \
                }                                              \
           else if ((file_size_done) >= GIGABYTE)              \
                {                                              \
                   trans_log(INFO_SIGN, NULL, 0, NULL, NULL,   \
                             "%.3f GiB (%lld bytes) %s in %d file(s).",\
                             (double)(file_size_done) / GIGABYTE,\
                             (pri_off_t)(file_size_done), (how),\
                             (no_of_files_done));              \
                }                                              \
           else if ((file_size_done) >= MEGABYTE)              \
                {                                              \
                   trans_log(INFO_SIGN, NULL, 0, NULL, NULL,   \
                             "%.3f MiB (%lld bytes) %s in %d file(s).",\
                             (double)(file_size_done) / MEGABYTE,\
                             (pri_off_t)(file_size_done), (how),\
                             (no_of_files_done));              \
                }                                              \
           else if ((file_size_done) >= KILOBYTE)              \
                {                                              \
                   trans_log(INFO_SIGN, NULL, 0, NULL, NULL,   \
                             "%.3f KiB (%lld bytes) %s in %d file(s).",\
                             (double)(file_size_done) / KILOBYTE,\
                             (pri_off_t)(file_size_done), (how),\
                             (no_of_files_done));              \
                }                                              \
                else                                           \
                {                                              \
                   trans_log(INFO_SIGN, NULL, 0, NULL, NULL,   \
                             "%lld bytes %s in %d file(s).",   \
                             (pri_off_t)(file_size_done),      \
                             (how), (no_of_files_done));       \
                }                                              \
        }
#endif

/* Macro to check if we can avoid a strcmp or strncmp. */
#define CHECK_STRCMP(a, b)  (*(a) != *(b) ? (int)((unsigned char) *(a) - (unsigned char) *(b)) : my_strcmp((a), (b)))
#define CHECK_STRNCMP(a, b, c)  (*(a) != *(b) ? (int)((unsigned char) *(a) - (unsigned char) *(b)) : strncmp((a), (b), (c)))

/* Function prototypes. */
#ifdef WITH_ERROR_QUEUE
extern char         *convert_error_queue(int, char *, size_t *, char *,
                                         unsigned char, unsigned char);
#endif
extern char         *convert_ls_data(int, char *, off_t *, int, char *,
                                     unsigned char, unsigned char),
#ifdef WITH_DUP_CHECK
                    *eval_dupcheck_options(char *, time_t *, unsigned int *,
                                           int *),
#endif
                    *get_definition(char *, char *, char *, int),
                    *get_error_str(int),
                    *get_progname(char *),
                    *lock_proc(int, int),
                    *llposi(char *, const size_t, char *, const size_t),
                    *lposi(char *, char *, const size_t),
                    *posi(char *, char *);
extern unsigned int get_afd_status_struct_size(void),
                    get_checksum(const unsigned int, char *, int),
#ifdef HAVE_HW_CRC32
                    get_checksum_crc32c(const unsigned int, char *, size_t, int),
                    get_str_checksum_crc32c(char *, int),
#else
                    get_checksum_crc32c(const unsigned int, char *, size_t),
                    get_str_checksum_crc32c(char *),
#endif
                    get_checksum_murmur3(const unsigned int,
                                         const unsigned char *, size_t),
                    get_str_checksum_murmur3(char *),
                    get_str_checksum(char *),
                    url_evaluate(char *, unsigned int *, char *,
                                 unsigned char *, char *,
#ifdef WITH_SSH_FINGERPRINT
                                 char *, char *,
#endif
                                 char *, int, char*, int *, char *, char **,
                                 time_t *, char *, unsigned char *,
                                 unsigned char *, char *, unsigned char *,
                                 char *);
extern int          assemble(char *, char *, int, char *, int, unsigned int,
                             int, unsigned int, int *, off_t *),
                    attach_afd_status(int *, int),
#ifdef WITH_IP_DB
                    attach_ip_db(void),
#endif
                    attach_jis(void),
#ifdef WITH_ERROR_QUEUE
                    attach_error_queue(void),
                    check_error_queue(unsigned int, int, time_t, int),
                    detach_error_queue(void),
                    host_check_error_queue(unsigned int, time_t, int),
                    print_error_queue(FILE *),
                    remove_from_error_queue(unsigned int,
                                            struct filetransfer_status *,
                                            int, int),
                    update_time_error_queue(unsigned int, time_t),
#endif
                    afw2wmo(char *, int *, char **, char *),
#ifdef _PRODUCTION_LOG
                    bin_file_chopper(char *, int *, off_t *, char *,
                                     char, time_t, unsigned int,
                                     unsigned int, unsigned int,
                                     unsigned int, clock_t, char *, char *),
#else
                    bin_file_chopper(char *, int *, off_t *, char *, char),
#endif
                    bittest(unsigned char *, int),
                    check_additional_lock_filters(char const *),
                    check_afd_heartbeat(long, int),
                    check_create_path(char *, mode_t, char **, int, int,
                                      char *),
                    check_afd_database(void),
                    check_dir(char *, int),
                    check_fra(int),
                    check_fsa(int, char *),
                    check_lock(char *, char),
                    check_msa(void),
                    check_msg_name(char *),
                    check_disabled_dirs(void),
                    check_time_str(char *, FILE *),
                    check_typesize_data(int *, FILE *, int),
                    coe_open(char *, int, ...),
                    convert_grib2wmo(char *, off_t *, char *),
#ifdef HAVE_STATX
                    copy_file(char *, char *, struct statx *),
#else
                    copy_file(char *, char *, struct stat *),
#endif
                    create_message(unsigned int, char *, char *),
                    create_name(char *, int, signed char, time_t, unsigned int,
                                unsigned int *, int *, char *, int, int),
                    create_remote_dir(char *, char *, char *, char *, char *,
                                      char *, int *),
                    detach_afd_status(void),
#ifdef WITH_IP_DB
                    detach_ip_db(void),
#endif
                    detach_jis(void),
#ifdef HAVE_HW_CRC32
                    detect_cpu_crc32(void),
#endif
#ifndef HAVE_EACCESS
                    eaccess(char *, int),
#endif
                    eval_host_config(int *, char *, struct host_list **,
                                     unsigned int *, FILE *, int),
                    eval_timeout(int),
                    eval_time_str(char *, struct bd_time_entry *, FILE *),
                    exec_cmd(char *, char **, int, char *, int,
#ifdef HAVE_SETPRIORITY
                             int,
#endif
                             char *, struct timeval *, double *, clock_t,
                             time_t, int, int),
                    expand_filter(char *, char *, time_t),
                    expand_path(char *, char *),
                    extract(char *, char *, char *,
#ifdef _PRODUCTION_LOG
                            time_t, unsigned int, unsigned int,
                            unsigned int, unsigned int, char *,
#endif
                            int, int, int *, off_t *),
                    fra_attach(void),
                    fra_attach_features(void),
                    fra_attach_features_passive(void),
                    fra_attach_passive(void),
                    fra_detach(void),
                    fsa_attach(char *),
                    fsa_attach_features(char *),
                    fsa_attach_features_passive(int, char *),
                    fsa_attach_passive(int, char *),
                    fsa_check_id_changed(int),
                    fsa_detach(int),
                    get_afd_name(char *),
                    get_afd_path(int *, char **, char *),
#ifdef WITH_IP_DB
                    get_and_reset_store_ip(void),
                    get_current_ip_hl(char **, char **),
                    get_store_ip(void),
#endif
                    get_arg(int *, char **, char *, char *, int),
                    get_argb(int *, char **, char *, char **),
                    get_arg_array(int *, char **, char *, char ***, int *),
                    get_arg_array_all(int *, char **, char ***, int *),
                    get_arg_int_array(int *, char **, char *, unsigned int **, int *),
                    get_current_jid_list(void),
                    get_dir_number(char *, unsigned int, long *),
                    get_dir_id_position(struct fileretrieve_status *,
                                        unsigned int, int),
                    get_dir_position(struct fileretrieve_status *, char *, int),
                    get_file_checksum(int, char *, int, int, unsigned int *),
                    get_file_checksum_crc32c(int, char *, int, int,
#ifdef HAVE_HW_CRC32
                                             unsigned int *, int),
#else
                                             unsigned int *),
#endif
                    get_file_checksum_murmur3(int, char *, size_t, size_t,
                                              unsigned int *),
                    get_host_id_position(struct filetransfer_status *,
                                         unsigned int, int),
                    get_host_position(struct filetransfer_status *,
                                      char *, int),
                    get_mon_path(int *, char **, char *),
                    get_permissions(char **, char *, char *),
                    get_pw(char *, char *, int),
                    get_rule(char *, int),
                    get_system_data(struct system_data *),
#ifdef WITH_DUP_CHECK
                    isdup(char *, char *, off_t, unsigned int, time_t, int, int,
# ifdef HAVE_HW_CRC32
                          int,
# endif
                          int, int),
                    isdup_rm(char *, char *, off_t, unsigned int, int,
# ifdef HAVE_HW_CRC32
                             int,
# endif
                             int, int),
#endif
                    is_msgname(const char *),
                    jid_attach(int, char *),
                    jid_detach(int),
                    lock_file(char *, int),
#ifdef LOCK_DEBUG
                    lock_region(int, off_t, char *, int),
#else
                    lock_region(int, off_t),
#endif
#ifdef WITH_IP_DB
                    lookup_ip_from_ip_db(char *, char *, int),
#endif
                    make_fifo(char *),
                    move_file(char *, char *),
                    msa_attach(void),
                    msa_attach_passive(void),
                    msa_detach(void),
                    my_power(int, int),
                    my_strcmp(const char *, const char *),
                    my_strncpy(char *, const char *, const size_t),
                    next_counter(int, int *, int),
                    open_counter_file(char *, int **),
#ifdef WITHOUT_FIFO_RW_SUPPORT
                    open_fifo_rw(char *, int *, int *),
#endif
                    pmatch(char const *, char const *, time_t *),
#ifdef WITH_IP_DB
                    print_ip_db(FILE *, char *),
#endif
                    read_job_ids(char *, int *, struct job_id_data **),
                    rec(int, char *, char *, ...),
                    rec_rmdir(char *),
#ifdef WITH_UNLINK_DELAY
                    remove_dir(char *, int),
#else
                    remove_dir(char *),
#endif
                    remove_files(char *, char *),
#ifdef WITH_IP_DB
                    remove_from_ip_db(char *),
#endif
#ifdef LOCK_DEBUG
                    rlock_region(int, off_t, char *, int),
#else
                    rlock_region(const int, const off_t),
#endif
                    search_insert_alias_name(char *, char *, int),
                    send_cmd(char, int),
#ifdef WITH_SYSTEMD
                    send_start_afd(char *, long),
                    shutdown_afd(char *, long, int, int),
#else
                    shutdown_afd(char *, long, int),
#endif
                    startup_afd(void),
                    url_compare(char *, char *),
                    wmo2ascii(char *, char *, off_t *),
                    write_system_data(struct afd_status *, int, int),
                    write_typesize_data(void),
                    xor_decrypt(unsigned char *, int, unsigned char *),
                    xor_encrypt(unsigned char *, int, unsigned char *);
extern off_t        bin_file_convert(char *, off_t, int, char *, unsigned int),
                    dwdtiff2gts(char *, char *),
                    gts2tiff(char *, char *),
                    iso8859_2ascii(char *, char *, off_t),
                    read_file(char *, char **),
                    read_file_no_cr(char *, char **, int, char *, int),
                    tiff2gts(char *, char *);
extern size_t       my_strlcpy(char *, const char *, const size_t);
extern ssize_t      readn(int, void *, int, long),
                    writen(int, const void *, size_t, ssize_t);
#ifdef WITH_TIMEZONE
extern time_t       calc_next_time(struct bd_time_entry *, char *, time_t, char *, int),
#else
extern time_t       calc_next_time(struct bd_time_entry *, time_t, char *, int),
#endif
                    calc_next_time_array(int, struct bd_time_entry *,
#ifdef WITH_TIMEZONE
                                         char *,
#endif
                                         time_t, char *, int),
                    datestr2unixtime(char *, int *),
                    write_host_config(int, char *, struct host_list *);
#ifdef WITH_ERROR_QUEUE
extern void         add_to_error_queue(unsigned int,
                                       struct filetransfer_status *, int,
                                       int, int, time_t),
                    *attach_buf(char *, int *, size_t *, char *, mode_t, int),
#else
extern void         *attach_buf(char *, int *, size_t *, char *, mode_t, int),
#endif
#ifdef WITH_IP_DB
                    add_to_ip_db(char *, char *),
#endif
                    alloc_jid(char *),
#ifdef WITH_AFD_MON
                    attach_adl(char *),
                    attach_ahl(char *),
                    attach_atd(char *),
                    detach_adl(void),
                    detach_ahl(void),
                    detach_atd(void),
#endif
                    bitset(unsigned char *, int),
                    change_alias_order(char **, int),
                    change_name(char *, char *, char *, char *,
                                int, int *, int **, unsigned int),
                    check_every_fra_disable_all_flag(void),
                    check_fake_user(int *, char **, char *, char *),
                    check_fra_disable_all_flag(unsigned int, int),
                    check_permissions(void),
                    close_counter_file(int, int **),
                    clr_fl(int, int),
                    count_files(char *, unsigned int *, off_t *),
                    daemon_init(char *),
                    dealloc_jid(void),
                    delete_log_ptrs(struct delete_log *),
                    error_action(char *, char *, int, int),
                    event_log(time_t, unsigned int, unsigned int,
                              unsigned int, char *, ...),
                    extract_cus(char *, time_t *, unsigned int *,
                                unsigned int *),
#ifdef MULTI_FS_SUPPORT
                    delete_stale_extra_work_dir_links(int,
                                                      struct extra_work_dirs *),
                    free_extra_work_dirs(int, struct extra_work_dirs **),
                    get_extra_work_dirs(char *, int *,
                                        struct extra_work_dirs **, int),
#endif
                    get_additional_locked_files(int *, int *, char **),
                    get_alias_names(void),
                    get_dir_options(unsigned int, struct dir_options *),
                    get_dc_result_str(char *, int, int, int *, int *),
                    get_file_mask_list(unsigned int, int *, char **),
                    get_hc_result_str(char *, int, int, int *, int *),
                    get_local_remote_part(unsigned int, char *),
                    get_log_type_data(char *),
                    get_log_number(int *, int, char *, int, char *),
                    get_max_log_values(int *, char *, int, off_t *, char *,
                                       off_t, char *),
#ifdef LINUX
                    get_proc_name_from_pid(pid_t, char *),
#endif
                    get_rename_rules(int),
#ifdef _PRODUCTION_LOG
                    get_sum_cpu_usage(struct rusage *, struct timeval *),
#endif
                    get_user(char *, char *, int),
                    ia_trans_log(char *, char *, int, int, char *, ...),
                    inform_fd_about_fsa_change(void),
                    init_fifos_afd(void),
                    initialize_db(int, int *, int),
#ifdef WITH_DUP_CHECK
                    isdup_detach(void),
#endif
#ifdef LOCK_DEBUG
                    lock_region_w(int, off_t, char *, int),
#else
                    lock_region_w(int, off_t),
#endif
#ifdef _MAINTAINER_LOG
                    maintainer_log(char *, char *, int, char *, ...),
#endif
#ifdef HAVE_STATX
                    *map_file(char *, int *, off_t *, struct statx *, int, ...),
#else
                    *map_file(char *, int *, off_t *, struct stat *, int, ...),
#endif
                    mode_t2str(mode_t, char *),
                    *mmap_resize(int, void *, size_t),
                    my_fillncpy(char *, const char *, const char, const size_t),
                    my_usleep(unsigned long),
                    next_counter_no_lock(int *, int),
#ifdef _OUTPUT_LOG
# ifdef WITHOUT_FIFO_RW_SUPPORT
                    output_log_fd(int *, int *, char *),
# else
                    output_log_fd(int *, char *),
# endif
                    output_log_ptrs(unsigned int **, unsigned int **, char **,
                                    char **, unsigned short **,
                                    unsigned short **, off_t **,
                                    unsigned short **, size_t *, clock_t **,
                                    char **, char *, int, int, char *),
#endif
#ifdef _PRODUCTION_LOG
                    production_log(time_t, unsigned int, unsigned int,
                                   unsigned int, unsigned int, unsigned int,
                                   unsigned int, double, time_t,
                                   long int, char *, ...),
#endif
                    remove_job_files(char *, int,
#ifdef _DELETE_LOG
                                     unsigned int, char *, char,
#endif
                                     off_t, char *, int),
                    remove_nnn_files(unsigned int),
                    reshuffel_log_files(int, char *, char *, int, int),
                    t_hostname(char *, char *),
#ifdef WITH_SETUID_PROGS
                    set_afd_euid(char *),
#endif
                    set_fl(int, int),
#ifdef WITH_IP_DB
                    set_store_ip(int),
#endif
                    store_real_hostname(char *, char *),
                    system_log(char *, char *, int, char *, ...),
#ifdef LOCK_DEBUG
                    unlock_region(int, off_t, char *, int),
#else
                    unlock_region(const int, const off_t),
#endif
                    unmap_data(int, void **),
                    update_db_log(char *, char *, int, FILE *, unsigned int *,
                                  char *, ...),
                    url_decode(char *, char *),
                    url_encode(char *, char *),
                    url_path_encode(char *, char *),
                    url_get_error(int, char *, int),
                    url_insert_password(char *, char *),
#ifdef WITH_ERROR_QUEUE
                    validate_error_queue(int, unsigned int *, int,
                                         struct filetransfer_status *, int),
#endif
                    wmoheader_from_grib(char *, char *, char *);
#ifdef _FIFO_DEBUG
extern void         show_fifo_data(char, char *, char *, int, char *, int);
#endif
#ifndef HAVE_MMAP
extern caddr_t      mmap_emu(caddr_t, size_t, int, int, char *, off_t);
extern int          msync_emu(char *),
                    munmap_emu(char *);
#endif
extern mode_t       str2mode_t(char *);

#endif /* __afddefs_h */
