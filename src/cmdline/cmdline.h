/*
 *  cmdline.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2004 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __cmdline_h
#define __cmdline_h

#ifdef _STANDALONE_
/* indicators for start and end of module description for man pages */
# define DESCR__S_M1             /* Start for User Command Man Page. */
# define DESCR__E_M1             /* End for User Command Man Page.   */
# define DESCR__S_M3             /* Start for Subroutines Man Page.  */
# define DESCR__E_M3             /* End for Subroutines Man Page.    */

# define NO                         0
# define YES                        1
# define BOTH                       3
# define ON                         1
# define OFF                        0
# define INCORRECT                  -1
# define SUCCESS                    0

# define INFO_SIGN                  "<I>"
# define WARN_SIGN                  "<W>"
# define ERROR_SIGN                 "<E>"
# define FATAL_SIGN                 "<F>"           /* donated by Paul M. */
# define DEBUG_SIGN                 "<D>"

/* Some default definitions. */
# define DEFAULT_TRANSFER_TIMEOUT   120L
# define DEFAULT_TRANSFER_BLOCKSIZE 4096

/* Definitions for maximum values. */
# define MAX_HOSTNAME_LENGTH        8
# define MAX_USER_NAME_LENGTH       80
# define MAX_PROXY_NAME_LENGTH      80
# define MAX_FILENAME_LENGTH        256
# define MAX_PATH_LENGTH            1024
# define MAX_LINE_LENGTH            2048

/* Definitions for different exit status. */
# define TRANSFER_SUCCESS           0
# define CONNECT_ERROR              1
# define USER_ERROR                 2
# define TYPE_ERROR                 4
# define LIST_ERROR                 5
# ifdef WITH_SSL
#  define AUTH_ERROR                9
# endif
# define OPEN_REMOTE_ERROR          10
# define WRITE_REMOTE_ERROR         11
# define CLOSE_REMOTE_ERROR         12
# define MOVE_REMOTE_ERROR          13
# define CHDIR_ERROR                14
# define STAT_ERROR                 17
# define TIMEOUT_ERROR              20
# define READ_REMOTE_ERROR          22
# define SIZE_ERROR                 23
# define CONNECTION_RESET_ERROR     28
# define CONNECTION_REFUSED_ERROR   29
# define OPEN_LOCAL_ERROR           30
# define READ_LOCAL_ERROR           31
# define ALLOC_ERROR                35
# define SYNTAX_ERROR               60
# define SET_BLOCKSIZE_ERROR        65

/* Runtime array */
# define RT_ARRAY(name, rows, columns, type)                                 \
         {                                                                   \
            int   row_counter;                                               \
                                                                             \
            if (((name) = (type **)calloc((rows), sizeof(type *))) == NULL)  \
            {                                                                \
               (void)rec(sys_log_fd, FATAL_SIGN, "calloc() error : %s (%s %d)\n", \
                         strerror(errno), __FILE__, __LINE__);               \
               exit(INCORRECT);                                              \
            }                                                                \
                                                                             \
            if (((name)[0] = (type *)calloc(((rows) * (columns)), sizeof(type))) == NULL) \
            {                                                                \
               (void)rec(sys_log_fd, FATAL_SIGN, "calloc() error : %s (%s %d)\n", \
                         strerror(errno), __FILE__, __LINE__);               \
               exit(INCORRECT);                                              \
            }                                                                \
                                                                             \
            for (row_counter = 1; row_counter < (rows); row_counter++)       \
               (name)[row_counter] = ((name)[0] + (row_counter * (columns)));\
         }
# define FREE_RT_ARRAY(name)         \
         {                           \
            free((name)[0]);         \
            free(name);              \
         }
# define REALLOC_RT_ARRAY(name, rows, columns, type)                         \
         {                                                                   \
            int  row_counter;                                                \
            char *ptr = (name)[0];                                           \
                                                                             \
            if (((name) = (type **)realloc((name), (rows) * sizeof(type *))) == NULL) \
            {                                                                \
               (void)rec(sys_log_fd, FATAL_SIGN,                             \
                         "realloc() error : %s (%s %d)\n",                   \
                         strerror(errno), __FILE__, __LINE__);               \
               exit(INCORRECT);                                              \
            }                                                                \
                                                                             \
            if (((name)[0] = (type *)realloc(ptr,                            \
                              (((rows) * (columns)) * sizeof(type)))) == NULL) \
            {                                                                \
               (void)rec(sys_log_fd, FATAL_SIGN,                             \
                         "realloc() error : %s (%s %d)\n",                   \
                         strerror(errno), __FILE__, __LINE__);               \
               exit(INCORRECT);                                              \
            }                                                                \
                                                                             \
            for (row_counter = 1; row_counter < (rows); row_counter++)       \
               (name)[row_counter] = ((name)[0] + (row_counter * (columns)));\
         }

/* Function prototypes */
extern void my_usleep(unsigned long),
            t_hostname(char *, char *),
            trans_log(char *, char *, int, char *, char *, char *, ...);
extern int  rec(int, char *, char *, ...);
#endif /* _STANDALONE_ */

#ifndef MAX_RET_MSG_LENGTH
# define MAX_RET_MSG_LENGTH  4096
#endif
#define TRANSFER_MODE        1
#define RETRIEVE_MODE        2
#define TEST_MODE            3
#define FILE_NAME_FILE_ERROR 40

/* Structure holding all filenames that are to be retrieved. */
struct filename_list
       {            
          char  file_name[MAX_FILENAME_LENGTH];
          off_t size;
       };

/* Structure that holds all data for one job. */
struct data
       {
          int          port;             /* TCP port.                      */
          int          age_limit;        /* If date of file is older then  */
                                         /* age limit, file gets removed.  */
          int          blocksize;        /* FTP transfer block size.       */
          int          no_of_files;      /* The number of files to be send.*/
          int          dummy_size;       /* File size of files to be       */
                                         /* transfered in test mode.       */
          int          sndbuf_size;      /* Socket send buffer size.       */
          int          rcvbuf_size;      /* Socket receive buffer size.    */
#ifdef WITH_SSL
          int          strict;
          int          legacy_renegotiation;
#endif
          long         transfer_timeout; /* When to timeout the            */
                                         /* transmitting job.              */
          char         *charset;         /* Character set to be used for   */
                                         /* mail.                          */
          char         *subject;         /* Subject of the mail.           */
          char         *reply_to;        /* The address where the recipient*/
                                         /* sends the reply.               */
          char         *from;            /* The address who send this mail.*/
          char         **filename;       /* Pointer to array that holds    */
                                         /* all file names to be send.     */
          char         **realname;       /* Pointer to array that holds    */
                                         /* the real names.                */
          char         hostname[MAX_FILENAME_LENGTH];
          char         user[MAX_USER_NAME_LENGTH];
          char         password[MAX_USER_NAME_LENGTH];
          char         proxy_name[MAX_PROXY_NAME_LENGTH + 1];
          char         remote_dir[MAX_PATH_LENGTH];
          char         chmod_str[5];     /* String mode value for FTP.     */
          char         dir_mode_str[5];  /* String mode value when         */
                                         /* creating directories for FTP.  */
          char         smtp_server[MAX_USER_NAME_LENGTH];
                                         /* Target directory on the remote */
                                         /* side.                          */
          char         lock_notation[MAX_FILENAME_LENGTH];
                                         /* Here the user can specify      */
                                         /* the notation of the locking on */
                                         /* the remote side.               */
          char         mode_str[17];     /* Can be either: active,         */
                                         /* passive, extended active,      */
                                         /* extended passive or passive    */
                                         /* (with redirect).               */
#ifdef WITH_SSH_FINGERPRINT
          char          ssh_fingerprint[MAX_FINGERPRINT_LENGTH + 1];
          char          key_type;
#endif
          mode_t       chmod;            /* The permissions that the       */
                                         /* file should have.              */
          mode_t       dir_mode;         /* The permissions that the       */
                                         /* directory should have.         */
          char         transfer_mode;    /* Transfer mode, A (ASCII) or I  */
                                         /* (Image, binary). (Default I)   */
          char         ftp_mode;         /* How the ftp data mode is       */
                                         /* initiated, either active or    */
                                         /* passive. Default active.       */
          char         acknowledge;      /* Acknowledge each message.[awmo]*/
#ifdef WITH_SSL
          char         implicit_ftps;
          char         tls_auth;         /* TLS/SSL authentification.      */
                                         /*  NO   - NO authentification.   */
                                         /*  YES  - Only control           */
                                         /*         connection.            */
                                         /*  BOTH - Control and data       */
                                         /*         connection.            */
#endif
          unsigned char ssh_protocol;    /* SSH protocol version to use.   */
          char         create_target_dir;/* Create the target directory if */
                                         /* does not exist.                */
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
          char         keepalive;        /* Send STAT during transfer to   */
                                         /* keep control connection alive. */
#endif
          char         exec_mode;        /* In which mode axxx is to be    */
                                         /* run. Currently three modes     */
                                         /* have been implemented:         */
                                         /*  transfer - files are being    */
                                         /*             transfered to      */
                                         /*             another host (push)*/
                                         /*  retrieve - files are being    */
                                         /*             fetched from       */
                                         /*             another host (pull)*/
                                         /*  test     - aftp itself        */
                                         /*             creates the files  */
                                         /*             to be send to      */
                                         /*             another host.      */
          char         lock;             /* The type of lock on the remote */
                                         /* site. Their are several        */
                                         /* possibilities:                 */
                                         /*   DOT      - send file name    */
                                         /*              starting with dot.*/
                                         /*              Then rename file. */
                                         /*   DOT_VMS  - Same as DOT but   */
                                         /*              only for VMS      */
                                         /*              systems.          */
                                         /*   OFF      - No locking, to    */
                                         /*              disable default   */
                                         /*              behaviour.        */
                                         /*   LOCKFILE - Send a lock file  */
                                         /*              and after transfer*/
                                         /*              delete lock file. */
          char         verbose;          /* Flag to set verbose option.    */
          char         remove;           /* Remove file flag.              */
          char         append;           /* Search for append file (only   */
                                         /* for retrieving).               */
          char         special_flag;     /* Special flag to indicate the   */
                                         /* following: ATTACH_FILE         */
                                         /*            FILE_NAME_IS_SUBJECT*/
                                         /*            FILE_NAME_IS_USER   */
                                         /*            FILE_NAME_IS_HEADER */
                                         /*            WITH_SEQUENCE_NUMBER*/
                                         /*            WMO_CHECK_ACKNOWLEDGE*/
                                         /*            ONLY_TEST           */
          signed char  file_size_offset; /* When doing an ls on the remote */
                                         /* site, this is the position     */
                                         /* where to find the size of the  */
                                         /* file. If it is less than 0, it */
                                         /* means that we do not want to   */
                                         /* append files that have been    */
                                         /* partly send.                   */
       };

/* Function prototypes. */
extern int  eval_filename_file(char *, struct data *);
extern void eval_config_file(char *, struct data *);

#endif /* __cmdline_h */
