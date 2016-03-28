/*
 *  afdsetup.h - Part of AFD, an automatic file distribution program.
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

#ifndef __afdsetup_h
#define __afdsetup_h

/*************************************************************************/
/*        Definitions that enable/disable features of the AFD            */
/*************************************************************************/

/*-----------------------------------------------------------------------*
 * With the following set of options you can enable/disable certain
 * features of the AFD. But note, the more features you enable will
 * increase memory usage and in some cases may harm performance.
 *
 * _WITH_BURST_2                 - Before a sf_xxx process exits it asks
 *                                 FD if it has another job which it can
 *                                 handle. This reduces the number of
 *                                 forks by FD and network latency
 *                                 considerably.
 * _LOG_REMOVE_INFO              - If set, every time an archive is
 *                                 removed a log entry is made in the
 *                                 system log. This can be very annoying
 *                                 if you have lots of files in the
 *                                 archive. So it's best to have this
 *                                 disabled.
 * _WITH_SHUTDOWN                - When FTP/SMTP closes the data connection
 *                                 it is common practice to shutdown() the
 *                                 connection.
 * _WITH_SEND                    - When writing to sockets we use the send()
 *                                 command when this option is set.
 *                                 Otherwise we simply use write().
 * _WITH_PTHREAD                 - If your system supports POSIX threads
 *                                 enable this. Best leave this disabled
 *                                 since it did not bring any performance
 *                                 gains, just complicated the code.
 * _WITH_SCP_SUPPORT             - Support for copying files via the SCP
 *                                 protocol. This requires a local ssh
 *                                 client.
 * DO_NOT_INFORM_ABOUT_OVERWRITE - If you do not want to be informed when
 *                                 an existing files is overwritten, then
 *                                 enable this.
 * DO_NOT_ARCHIVE_UNIQUE_PART    - This will not store the unique part of
 *                                 the file name in the archive. Do NOT
 *                                 activate this! Only activate this in
 *                                 benchmarking to have comparible
 *                                 results to AFD releases prior to 1.3.0.
 *                                 Otherwise the results are just not
 *                                 comparible.
 *-----------------------------------------------------------------------*/
#define _WITH_BURST_2
/* #define _LOG_REMOVE_INFO */
#define _WITH_SHUTDOWN
/* #define _WITH_SEND */
/* #define _WITH_PTHREAD */
#define _WITH_AFW2WMO
#define _WITH_SCP_SUPPORT
#define WITH_SSH_FINGERPRINT
#ifdef WITH_SSH_FINGERPRINT
# define WITH_REMOVE_FROM_KNOWNHOSTS
#endif
#define FTP_CTRL_KEEP_ALIVE_INTERVAL
#ifdef AFDBENCH_CONFIG
# ifdef _WITH_AFW2WMO
#  undef _WITH_AFW2WMO
# endif
# define DO_NOT_INFORM_ABOUT_OVERWRITE
# define DO_NOT_ARCHIVE_UNIQUE_PART
#endif

/*-----------------------------------------------------------------------*
 * These following options are only for the dialogs of the AFD.
 *
 * _AUTO_REPOSITION          - When set, the main window of afd_ctrl will
 *                             be automatically repositioned when the
 *                             right or bottom side of the 'afd_ctrl'
 *                             window touches the right or bottom of the
 *                             screen. This will be done until the top or
 *                             left side of the afd_ctrl window reaches the
 *                             top or left side of the screen.
 * _SQUARE_LED               - Makes the debug LED square. Otherwise the
 *                             LED is round.
 * _SLOW_COUNTER             - If this value is set the line counter of
 *                             the dialog 'show_log' is only updated when
 *                             the log file does not increase. Gives a
 *                             slight performance boost when lots of log
 *                             entries are being made.
 *-----------------------------------------------------------------------*/
#define _AUTO_REPOSITION
#define _SQUARE_LED
#define _NO_LED_FRAME
#define _SLOW_COUNTER



/*************************************************************************/
/*        Debug/test features of the AFD. (Should be disabled)           */
/*************************************************************************/

/*-----------------------------------------------------------------------*
 * Can only be used with dir_check. This tells the process dir_check to
 * print the data of structure directory_entry to stdout, for every
 * directory entry.
 *-----------------------------------------------------------------------*/
/* #define _TEST_FILE_TABLE */

/*-----------------------------------------------------------------------*
 * Used to enable general debug options. All debugging information will
 * be written to stdout.
 *-----------------------------------------------------------------------*/
/* #define _DEBUG */

/*-----------------------------------------------------------------------*
 * When enabled all communication done via fifo will be written to
 * stdout.
 *-----------------------------------------------------------------------*/
/* #define _FIFO_DEBUG */




/*************************************************************************/
/*      Global definitions for the AFD (Automatic File Distributor)      */
/*************************************************************************/

/*-----------------------------------------------------------------------*
 * Some commonly used maximum values.
 * NOTE: If you change MAX_LINE_LENGTH ensure that you change
 *       MAX_LOG_DATA_BUFFER so that it is still divisible by
 *       MAX_LINE_LENGTH and there is no rest (see afdd/afdddefs.h).
 *-----------------------------------------------------------------------*/
#define MAX_PATH_LENGTH     1024
#define MAX_LINE_LENGTH     2048 /* See note above! */

/*-----------------------------------------------------------------------*
 * The interval time (in seconds) at which the process 'init_afd'
 * checks if all its child processes are still running and checks how
 * many directories (jobs) are currently in the AFD file directory.
 * This is also the minimum update time of the heartbeat counter.
 * DEFAULT: 1
 *-----------------------------------------------------------------------*/
#define AFD_RESCAN_TIME 1

/*-----------------------------------------------------------------------*
 * When the disk is full, this is the time interval (in seconds) at
 * which the AFD will retry at that point where it was stopped.
 * DEFAULT: 20
 *-----------------------------------------------------------------------*/
#define DISK_FULL_RESCAN_TIME 20

/*-----------------------------------------------------------------------*
 * The default file and directory creation modes.
 *-----------------------------------------------------------------------*/
#ifdef GROUP_CAN_WRITE
# define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)
#else
# define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#endif
#define DIR_MODE  (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | \
                   S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH)

/*-----------------------------------------------------------------------*
 * The interval at which a new archive will be created and when its
 * time deleted again. This value can be used to reduce the number of
 * archives. The higher this value will be, the smaller will be the
 * number of archive directories created. But note that the higher it
 * will be, it will increase the the possibility that files will be
 * over written in that archive directory. The lowest value is 1.
 * DEFAULT: 240 (=> 4 minute)
 *-----------------------------------------------------------------------*/
#ifdef AFDBENCH_CONFIG
# define ARCHIVE_STEP_TIME 10
#else
# define ARCHIVE_STEP_TIME 240
#endif

/*-----------------------------------------------------------------------*
 * The following definitions are for logging input, output and debug
 * file names:
 *
 * SWITCH_FILE_TIME      - When to start a new I/O log file. If you for
 *                         example want to create a new log file every
 *                         day then use 86400. Or you want to do it
 *                         once a week then you have to put 604800 here.
 *-----------------------------------------------------------------------*/
#define SWITCH_FILE_TIME 86400

/*-----------------------------------------------------------------------*
 * The following definitions are used to control the length and interval
 * of the log history for the receive-, system- and transfer-log.
 * HISTORY_LOG_INTERVAL - At what interval the log history is shuffled
 *                        one block to the left in the mon_ctrl dialog.
 *                        DEFAULT: 3600
 * MAX_LOG_HISTORY      - The number of history blocks to be stored.
 *                        DEFAULT: 48
 *-----------------------------------------------------------------------*/
#define HISTORY_LOG_INTERVAL 3600
#define MAX_LOG_HISTORY      48

/*-----------------------------------------------------------------------*
 * How old (in seconds) a file my be before it gets deleted. DEFAULT 0
 *-----------------------------------------------------------------------*/
#define DEFAULT_AGE_LIMIT 0


/*-----------------------------------------------------------------------*
 * Process dir_check (part of AMG) sometimes needs to look at the queue
 * list of FD. It needs to do this while there is no activity in this
 * queue and for this they communicate via fifos to ensure that this
 * is the case. The following two values are timeouts (in seconds) in
 * case either of them do not respond.
 * QUEUE_LIST_READY_TIMEOUT - How long dir_check should wait for FD to
 *                            respond.
 * QUEUE_LIST_DONE_TIMEOUT  - How long FD should wait for dir_check to
 *                            respond.
 *-----------------------------------------------------------------------*/
#define QUEUE_LIST_READY_TIMEOUT 90
#define QUEUE_LIST_DONE_TIMEOUT  90

/*-----------------------------------------------------------------------*
 * The interval in seconds at which we check the AFD_CONFIG file for
 * changes. Do not set this to low, it will burn unnecessary CPU time.
 * DEFAULT: 20
 *-----------------------------------------------------------------------*/
#define STAT_INTERVAL 20


/*************************************************************************/
/*      Definitions for the AMG (Automatic Message Generator)            */
/*************************************************************************/

/*-----------------------------------------------------------------------*
 * The interval in seconds at which the AMG should check if it is time
 * to handle any time jobs in the queue.
 * DEFAULT: 60
 *-----------------------------------------------------------------------*/
#define TIME_CHECK_INTERVAL 60

/*-----------------------------------------------------------------------*
 * REPORT_DIR_TIME_INTERVAL - The interval in seconds at which the AMG
 *                            should report the time it takes for
 *                            scanning one full sets of directories.
 *                            DEFAULT: 3600
 * MAX_DIFF_TIME            - If you don't want it report every interval
 *                            but only when it reaches a certain value
 *                            define this.
 *                            DEFAULT: 30L
 *-----------------------------------------------------------------------*/
#ifdef AFDBENCH_CONFIG
# define REPORT_DIR_TIME_INTERVAL 240
# define MAX_DIFF_TIME            20L
#else
# define REPORT_DIR_TIME_INTERVAL 3600
# define MAX_DIFF_TIME            30L
#endif

/*-----------------------------------------------------------------------*
 * The rescan time of the user directories in the DIR_CONFIG file. Don't
 * pick a small value here if your system shows high system load when
 * working with files or directories (eg. fault tolerant systems).
 * This is also the interval time that the AMG checks if the DIR_CONFIG
 * or HOST_CONFIG file have been changed. The unit is second.
 * DEFAULT: 5
 *-----------------------------------------------------------------------*/
#define DEFAULT_RESCAN_TIME 5

/*-----------------------------------------------------------------------*
 * The default interval in seconds at which a directory is scanned even
 * if the time stamp did not change. A value of zero will disable this
 * option.
 * DEFAULT: 0
 *-----------------------------------------------------------------------*/
#define FORCE_REREAD_INTERVAL 0

/*-----------------------------------------------------------------------*
 * The following entries are used to set the maximum values for the
 * DIR_CONFIG file. They are as followed:
 *
 * MAX_NO_OPTIONS       - Number of options that may be defined for one
 *                        destination.
 * MAX_RECIPIENT_LENGTH - Maximum length that one recipient may have,
 *                        including the directory entry.
 * MAX_FILE_MASK_BUFFER - Maximum number of bytes that may be used to
 *                        store the file masks for one file group entry.
 *                        Each file mask will be terminated by a new-
 *                        line.
 *-----------------------------------------------------------------------*/
#define MAX_NO_OPTIONS       15
#define MAX_RECIPIENT_LENGTH 256
#define MAX_OPTION_LENGTH    256
#define MAX_FILE_MASK_BUFFER 4096

/*-----------------------------------------------------------------------*
 * ONE_DIR_COPY_TIMEOUT - When AMG has collected MAX_COPIED_FILES, it
 *                        will try to continue to copy from this directory
 *                        until it reaches ONE_DIR_COPY_TIMEOUT timeout.
 *                        DEFAULT: 5 (seconds)
 * FULL_SCAN_TIMEOUT    - The timeout for scanning all directories. If
 *                        set to 0 this is diasbled. When this is set
 *                        it will only take effect if during a scan there
 *                        are directories which still have files to be
 *                        distributed but could not because of
 *                        MAX_COPIED_FILES or some other limits. If
 *                        FULL_SCAN_TIMEOUT has not been reached, it
 *                        will try to continue with the full directories
 *                        until FULL_SCAN_TIMEOUT is reached.
 * DIR_CHECK_TIMEOUT    - Sometimes it can take very long for dir_check
 *                        to go once through all directories and it will
 *                        take too long until important files get send.
 *                        This is only effective when the administrator
 *                        has marked one or more directories with an
 *                        asterix '*'.
 *                        DEFAULT: 60 (seconds)
 *                        NOTE: This is only a quick hack until a better
 *                              solution is found.
 *-----------------------------------------------------------------------*/
#define ONE_DIR_COPY_TIMEOUT 5
#define FULL_SCAN_TIMEOUT    0
#ifndef _WITH_PTHREAD
# define DIR_CHECK_TIMEOUT   60
#endif

/*-----------------------------------------------------------------------*
 * MAX_NO_OF_DIR_CHECKS - Maximum number of times that the process
 *                        dir_check may fork itself.
 *                        DEFAULT: 20
 * MAX_PROCESS_PER_DIR  - The maximum number of times that the process
 *                        dir_check may fork itself per directory. This
 *                        number must be less then or equal to
 *                        MAX_NO_OF_DIR_CHECKS!
 *                        DEFAULT: 10
 *-----------------------------------------------------------------------*/
#ifdef AFDBENCH_CONFIG
# define MAX_NO_OF_DIR_CHECKS 30
# define MAX_PROCESS_PER_DIR  15
#else
# define MAX_NO_OF_DIR_CHECKS 20
# define MAX_PROCESS_PER_DIR  10
#endif

/*-----------------------------------------------------------------------*
 * When no archive time is specified in the DIR_CONFIG file this is it's
 * default value. If you expect to distribute lot's of files its best to
 * set this value to zero. Because when you forget to enter a value
 * it can very quickly fill up your disk. The unit depends on
 * DEFAULT_ARCHIVE_UNIT when no unit is specified.
 * DEFAULT: 0
 *-----------------------------------------------------------------------*/
#define DEFAULT_ARCHIVE_TIME 0

/*-----------------------------------------------------------------------*
 * The number of seconds that AMG waits for a reply from FD, when AMG
 * informs FD that it is about to change the FSA.
 * DEFAULT: 20
 *-----------------------------------------------------------------------*/
#define FD_REPLY_TIMEOUT 20




/*************************************************************************/
/*            Definitions for the FD (File Distributor)                  */
/*                                                                       */
/*   NOTE: some of the values are set by the AMG.                        */
/*************************************************************************/

/*-----------------------------------------------------------------------*
 * The time interval at which the FD looks in the message directory to
 * see if a new message has arrived, or checks if it has to handle any
 * error messages.
 * DEFAULT: 10
 *-----------------------------------------------------------------------*/
#define FD_RESCAN_TIME 10L

/*-----------------------------------------------------------------------*
 * The time interval at which the FD scans remote directories for files
 * that must be retrieved.
 * DEFAULT: 60
 *-----------------------------------------------------------------------*/
#define DEFAULT_REMOTE_FILE_CHECK_INTERVAL 60

/*-----------------------------------------------------------------------*
 * The time interval in seconds at which each sf_xxx and gf_xxx process
 * locks a region in FSA and updates parameters such as the number of
 * files * and the size that has been transmitted. This helps to reduce
 * the number of locks generated by AFD. However, the higher this
 * value is the higher will be the interval at which these values will
 * be updated in afd_ctrl dialog.
 * DEFAULT: 1
 *-----------------------------------------------------------------------*/
#ifdef AFDBENCH_CONFIG
# define LOCK_INTERVAL_TIME 3
#else
# define LOCK_INTERVAL_TIME 1
#endif

/*-----------------------------------------------------------------------*
 * If this is defined, AFD will try to use the type-of-service field
 * in the IP header for a TCP socket. For FTP this would minimize delay
 * on the control connection and maximize throughput on the data
 * connection.
 * To disable just make a comment of this.
 *-----------------------------------------------------------------------*/
#define _WITH_TOS

/*-----------------------------------------------------------------------*
 * The number of simultaneous connections/transfers for input and
 * output:
 *
 * MAX_DEFAULT_CONNECTIONS      - default when no value is set in
 *                                AFD_CONFIG file (default 50).
 * MAX_CONFIGURABLE_CONNECTIONS - the maximum configurable value in
 *                                AFD_CONFIG file (default 1024). If
 *                                a higher value is specified it will
 *                                default to MAX_DEFAULT_CONNECTIONS.
 *-----------------------------------------------------------------------*/
#define MAX_DEFAULT_CONNECTIONS      50
#define MAX_CONFIGURABLE_CONNECTIONS 1024

/*-----------------------------------------------------------------------*
 * If the number of messages queued is very large and most messages
 * belong to a host that is very slow, new messages will only be
 * processed very slowly when the whole queue is scanned each time a
 * new message arrives or a job has terminated. The following parameters
 * can reduce system load and make FD much more responsive in such
 * situations.
 *
 * MAX_QUEUED_BEFORE_CECKED   - the maximum number of messages that
 *                              must be queued before we start
 *                              counting the loops.
 *                              DEFAULT: 4000
 * ELAPSED_LOOPS_BEFORE_CHECK - the number of loops before the whole
 *                              queue is being checked.
 *                              DEFAULT: 20
 *-----------------------------------------------------------------------*/
#define MAX_QUEUED_BEFORE_CECKED   4000
#define ELAPSED_LOOPS_BEFORE_CHECK 20

/*-----------------------------------------------------------------------*
 * The default retry time (in seconds) when a transfer has failed.
 * Don't set this value to low! If the connection is very bad to the
 * remote site, it can happen that you succeed to connect but then
 * fail to transmit the file and in most cases will then also fail to
 * disconnect properly. And if this continues too fast the remote
 * host will have lots of ftp daemons that still think they are connected.
 * DEFAULT: 300 (=> 5 min)
 *-----------------------------------------------------------------------*/
#define DEFAULT_RETRY_INTERVAL 300

/*-----------------------------------------------------------------------*
 * The default block size when transmitting files.
 * DEFAULT: 4096
 *-----------------------------------------------------------------------*/
#define DEFAULT_TRANSFER_BLOCKSIZE 4096

/*-----------------------------------------------------------------------*
 * The minimum and maximum block size that may be specified in the
 * edit_hc dialog.  Experiment with these values to see if they have
 * any big impact on your system.
 * DEFAULT: min 256, max 8388608
 *-----------------------------------------------------------------------*/
#define MIN_TRANSFER_BLOCKSIZE 256
#define MAX_TRANSFER_BLOCKSIZE 8388608

/*-----------------------------------------------------------------------*
 * If a transfer of a big file gets interrupted, it is annoying to
 * always retransmit the complete file. So files will be appended when
 * we already have send a part of that file. However it does not make
 * sense to do this for very small files, or when we only have transfered
 * a couple of bytes. With this definition you can define when a file
 * should be appended.
 *-----------------------------------------------------------------------*/
#define MAX_SEND_BEFORE_APPEND 102400

/*-----------------------------------------------------------------------*
 * Define this if you still have an old SSH server that can fall back to
 * RSH but you don't want this to happen.
 *-----------------------------------------------------------------------*/
/* #define _WITH_FALLBACK_TO_RSH_NO */


/*************************************************************************/
/* End of user definable things. Please do not edit below here.          */
/*************************************************************************/

#ifdef _WITH_PTHREAD
#define _REENTRANT
#endif
#endif /* __afdsetup_h */
