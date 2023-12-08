/*
 *  amgdefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __amgdefs_h
#define __amgdefs_h

#include <stdio.h>
#include <sys/types.h>
#include <sys/times.h>

#ifndef IGNORE_DUPLICATE_JOB_IDS
/* Set this to check for duplicate job entries. */
# define WITH_GOTCHA_LIST
#endif

/* Debug definitions. Only turn on when you know. Produces lots of data. */
/* #define SHOW_FILE_MOVING */

/* Definitions to be read from the AFD_CONFIG file. */
#define AMG_DIR_RESCAN_TIME_DEF    "AMG_DIR_RESCAN_TIME"
#define MAX_NO_OF_DIR_CHECKS_DEF   "MAX_NO_OF_DIR_CHECKS"
#define MAX_PROCESS_PER_DIR_DEF    "MAX_PROCESS_PER_DIR"
#define IGNORE_FIRST_ERRORS_DEF    "IGNORE_FIRST_ERRORS"

/* Definitions of default values. */
#define DEFAULT_PRIORITY           '9'  /* When priority is not          */
                                        /* specified, assume this value. */

/* Definitions of maximum values. */
#define MAX_CHECK_FILE_DIRS        152  /* AMG should only check the file*/
                                        /* directory if it is less then  */
                                        /* this value. Don't forget to   */
                                        /* add 2 for "." and "..".       */
#define MAX_HOLDING_TIME           24   /* The maximum time that a file  */
                                        /* can be held before it gets    */
                                        /* deleted.                      */
#define MAX_DIR_OPTION_LENGTH      20   /* The size of the buffer where  */
                                        /* the time in hours, what is to */
                                        /* be done and if it should be   */
                                        /* reported if an unknown file   */
                                        /* appears in the source         */
                                        /* directory.                    */
#define MAX_GROUP_NAME_LENGTH      256  /* The maximum group name length */
                                        /* for any group.                */
#define MAX_DETACH_TIME            1    /* The maximum time that eval_-  */
                                        /* database will wait for all    */
                                        /* jobs to detach from the FSA.  */
#define MAX_FILES_TO_PROCESS       3    /* The maximum number of files   */
                                        /* that the AMG may copy in one  */
                                        /* go if extract option is set.  */
#ifdef WITH_ONETIME
# define MAX_NO_OF_ONETIME_DIRS    10   /* Number of onetime directories */
                                        /* we can handle in one go.      */
#endif
#if defined (_INPUT_LOG) || defined (_OUTPUT_LOG)
#define MAX_NO_OF_DB_PER_FILE      200  /* The maximum number of         */
                                        /* database that can be stored   */
                                        /* in a database history file.   */
#endif
#ifdef WITH_ONETIME
# define OT_DC_STEP_SIZE           10   /* Number of onetime configs     */
                                        /* stored in one cycle.          */
#endif
#define DG_BUFFER_STEP_SIZE        2    /* The steps in which the buffer */
                                        /* is increased for the          */
                                        /* destination group.            */
#define FG_BUFFER_STEP_SIZE        4    /* The steps in which the buffer */
                                        /* is increased for the          */
                                        /* file group . May NOT be less  */
                                        /* then 2!                       */
#define DIR_NAME_BUF_SIZE          20   /* Buffers allocated for the     */
                                        /* dir_name_buf structure.       */
#define JOB_ID_DATA_STEP_SIZE      50   /* Buffers allocated for the     */
                                        /* job_id_data structure.        */
#define TIME_JOB_STEP_SIZE         10
#define FILE_NAME_STEP_SIZE        20
#define FILE_BUFFER_STEP_SIZE      50
#define ORPHANED_PROC_STEP_SIZE     5
#ifdef WITH_INOTIFY
# define INOTIFY_FL_STEP_SIZE      80
#endif
#define RECIPIENT_STEP_SIZE        10
#define FILE_MASK_STEP_SIZE       256

/* Definitions of identifiers in options. */
#define TIME_NO_COLLECT_ID         "time no collect"
#define TIME_NO_COLLECT_ID_LENGTH  (sizeof(TIME_NO_COLLECT_ID) - 1)
#define TIME_NO_COLLECT_ID_FLAG    16
/* NOTE: TIME_ID has already been defined in afddefs.h for [dir options]. */
#define TIME_ID_FLAG               8
/* NOTE: TIMEZONE_ID has already been defined in afddefs.h for [dir options]. */
#define TIMEZONE_ID_FLAG           32
#define PRIORITY_ID                "priority"
#define PRIORITY_ID_LENGTH         (sizeof(PRIORITY_ID) - 1)
#define RENAME_ID                  "rename"
#define RENAME_ID_LENGTH           (sizeof(RENAME_ID) - 1)
#define RENAME_ID_FLAG             1
#define SRENAME_ID                 "srename"
#define SRENAME_ID_LENGTH          (sizeof(SRENAME_ID) - 1)
#define SRENAME_ID_FLAG            2
#define EXEC_ID                    "exec"
#define EXEC_ID_LENGTH             (sizeof(EXEC_ID) - 1)
#define EXEC_ID_FLAG               4
#define BASENAME_ID                "basename" /* If we want to send only */
                                              /* the basename of the     */
                                              /* file.                   */
#define BASENAME_ID_LENGTH         (sizeof(BASENAME_ID) - 1)
#define BASENAME_ID_FLAG           64
#define EXTENSION_ID               "extension"/* If we want to send      */
                                              /* files without extension.*/
#define EXTENSION_ID_LENGTH        (sizeof(EXTENSION_ID) - 1)
#define EXTENSION_ID_FLAG          128
#define ADD_PREFIX_ID              "prefix add"
#define ADD_PREFIX_ID_LENGTH       (sizeof(ADD_PREFIX_ID) - 1)
#define ADD_PREFIX_ID_FLAG         256
#define DEL_PREFIX_ID              "prefix del"
#define DEL_PREFIX_ID_LENGTH       (sizeof(DEL_PREFIX_ID) - 1)
#define DEL_PREFIX_ID_FLAG         512
#define TOUPPER_ID                 "toupper"
#define TOUPPER_ID_LENGTH          (sizeof(TOUPPER_ID) - 1)
#define TOUPPER_ID_FLAG            1024
#define TOLOWER_ID                 "tolower"
#define TOLOWER_ID_LENGTH          (sizeof(TOLOWER_ID) - 1)
#define TOLOWER_ID_FLAG            2048
#define FAX2GTS_ID                 "fax2gts"
#define FAX2GTS_ID_LENGTH          (sizeof(FAX2GTS_ID) - 1)
#define FAX2GTS_ID_FLAG            4096
/* NOTE: TIFF2GTS_ID defined in afddefs.h */
#define TIFF2GTS_ID_LENGTH         (sizeof(TIFF2GTS_ID) - 1)
#define TIFF2GTS_ID_FLAG           8192
#define GTS2TIFF_ID                "gts2tiff"
#define GTS2TIFF_ID_LENGTH         (sizeof(GTS2TIFF_ID) - 1)
#define GTS2TIFF_ID_FLAG           16384
#define GRIB2WMO_ID                "grib2wmo"
#define GRIB2WMO_ID_LENGTH         (sizeof(GRIB2WMO_ID) - 1)
#define GRIB2WMO_ID_FLAG           32768
#define EXTRACT_ID                 "extract"
#define EXTRACT_ID_LENGTH          (sizeof(EXTRACT_ID) - 1)
#define EXTRACT_ID_FLAG            65536
#define ASSEMBLE_ID                "assemble"
#define ASSEMBLE_ID_LENGTH         (sizeof(ASSEMBLE_ID) - 1)
#define ASSEMBLE_ID_FLAG           131072
#define WMO2ASCII_ID               "wmo2ascii"
#define WMO2ASCII_ID_LENGTH        (sizeof(WMO2ASCII_ID) - 1)
#define WMO2ASCII_ID_FLAG          262144
/* NOTE: DELETE_ID and DELETE_ID_LENGTH defined in afddefs.h */
#define DELETE_ID_FLAG             524288
#define CONVERT_ID                 "convert"
#define CONVERT_ID_LENGTH          (sizeof(CONVERT_ID) - 1)
#define CONVERT_ID_FLAG            1048576
#define LCHMOD_ID                  "lchmod"
#define LCHMOD_ID_LENGTH           (sizeof(LCHMOD_ID) - 1)
#define LCHMOD_ID_FLAG             2097152
#ifdef _WITH_AFW2WMO
# define AFW2WMO_ID                "afw2wmo"
# define AFW2WMO_ID_LENGTH         (sizeof(AFW2WMO_ID) - 1)
# define AFW2WMO_ID_FLAG           4194304
# define LOCAL_OPTION_POOL_SIZE    23
#else
# define LOCAL_OPTION_POOL_SIZE    22
#endif

/* Definitions for types of time options. */
#define NO_TIME                    0     /* No time options at all.      */
#define SEND_COLLECT_TIME          1     /* Only send during this time.  */
                                         /* Data outside this time will  */
                                         /* be stored and send when the  */
                                         /* next send time slot arrives. */
#define SEND_NO_COLLECT_TIME       2     /* Only send data during this   */
                                         /* time. Data outside this time */
                                         /* will NOT be stored and will  */
                                         /* be deleted!                  */

#define LOCALE_DIR  0
#define REMOTE_DIR  1

/* Definitions for assembling, converting and extracting WMO bulletins/files. */
#define TWO_BYTE                   1
#define FOUR_BYTE_LBF              2
#define FOUR_BYTE_HBF              3
#define FOUR_BYTE_MSS              4
#define FOUR_BYTE_MRZ              5
#define FOUR_BYTE_GRIB             6
#define WMO_STANDARD               7
#define ASCII_STANDARD             8
#define BINARY_STANDARD            9
#define FOUR_BYTE_DWD              10
#define SOHETX2WMO0                11
#define SOHETX2WMO1                12
#define SOHETX                     13
#define ONLY_WMO                   14
#define SOHETXWMO                  15
#define MRZ2WMO                    16
#define ZCZC_NNNN                  17
#define DOS2UNIX                   18
#define UNIX2DOS                   19
#define LF2CRCRLF                  20
#define CRCRLF2LF                  21
#define ISO8859_2ASCII             22
#define WMO_WITH_DUMMY_MESSAGE     23
#define WMO_STANDARD_CHK           24
#define SP_CHAR                    25

/* Definitions for the different extract options. */
#define EXTRACT_ADD_SOH_ETX          1
#define EXTRACT_ADD_CRC_CHECKSUM     2
#define EXTRACT_ADD_UNIQUE_NUMBER    4
#define EXTRACT_REPORTS              8
#define EXTRACT_REMOVE_WMO_HEADER    16
#define EXTRACT_EXTRA_REPORT_HEADING 32
#define EXTRACT_SHOW_REPORT_TYPE     64
#define EXTRACT_ADD_FULL_DATE        128
#define EXTRACT_ADD_BUL_ORIG_FILE    256
#define USE_EXTERNAL_MSG_RULES       512
#define EXTRACT_ADD_ADDITIONAL_INFO  1024
#define DEFAULT_EXTRACT_OPTIONS      (EXTRACT_ADD_SOH_ETX | EXTRACT_ADD_CRC_CHECKSUM)

#ifdef WITH_ONETIME
/* Definitions for the different onetime job types. */
# define OT_CONFIG_TYPE              1
# define OT_LIST_TYPE                2
#endif

/* Definition of fifos for the AMG to communicate. */
/* with the above jobs.                           */
#define DC_CMD_FIFO                "/dc_cmd.fifo"
#define DC_RESP_FIFO               "/dc_resp.fifo"
#ifdef WITH_INOTIFY
# define DC_FIFOS                  1
# define IC_FIFOS                  2
# define IC_CMD_FIFO               "/ic_cmd.fifo"
# define IC_RESP_FIFO              "/ic_resp.fifo"
#endif
#ifdef WITH_ONETIME
# define OT_JOB_FIFO               "/ot_job.fifo"
#endif

/* Definitions of the process names that are started */
/* by the AMG main process.                          */
#define DC_PROC_NAME               "dir_check"
#ifdef WITH_ONETIME
# define OT_PROC_NAME              "onetime_check"
#endif

/* Return values for function eval_dir_config() */
#define NO_VALID_ENTRIES           -2

/* Miscellaneous definitions. */
#define PTR_BUF_SIZE               50  /* The initial size of the pointer*/
                                       /* array and the step size by     */
                                       /* which it can increase. This    */
                                       /* can be large since pointers    */
                                       /* don't need a lot of space.     */
#define OPTION_BUF_SIZE            10  /* When allocating memory for the */
                                       /* option buffer, this is the step*/
                                       /* size by which the memory will  */
                                       /* increase when memory is used   */
                                       /* up. (* MAX_OPTION_LENGTH)      */
#define JOB_TIMEOUT                60L /* This is how long AMG waits (in */
                                       /* seconds) for a reply from a job*/
                                       /* (ITT, OOT or IT) before it     */
                                       /* receives a timeout error.      */
                                       /* NOTE: Must be larger than 5!   */
#define DIR_CHECK_TIME           1500  /* Time (every 25min) when to     */
                                       /* check in file directory for    */
                                       /* jobs without the corresponding */
                                       /* message.                       */
#define MESSAGE_BUF_STEP_SIZE     500

#define OLD_FILE_SEARCH_INTERVAL 3600  /* Search for old files (1 hour). */
#ifdef WITH_INOTIFY
# define DEL_UNK_INOTIFY_FILE_TIME 300L/* Interval at which inotify dirs */
#endif
                                       /* will be checked for deleting   */
                                       /* unknown files.                 */
#define NO_OF_HOST_DB_PARAMETERS    8

#ifdef _WITH_PTHREAD
/* Structure that holds data to be send passed to a function. */
struct data_t
       {
          int           i;
          char          **file_name_pool;
          unsigned char *file_length_pool;
          off_t         *file_size_pool;
          time_t        *file_mtime_pool;
       };
#endif /* _WITH_PTHREAD */

/* Structure holding pointers to all relevant information */
/* in the global shared memory regions.                   */
#define PRIORITY_PTR_POS            0
#define DIRECTORY_PTR_POS           1
#define ALIAS_NAME_PTR_POS          2
#define NO_OF_FILES_PTR_POS         3
#define FILE_PTR_POS                4
#define NO_LOCAL_OPTIONS_PTR_POS    5
#define LOCAL_OPTIONS_PTR_POS       6
#define LOCAL_OPTIONS_FLAG_PTR_POS  7
#define NO_STD_OPTIONS_PTR_POS      8
#define STD_OPTIONS_PTR_POS         9
#define RECIPIENT_PTR_POS           10
#define SCHEME_PTR_POS              11
#define HOST_ALIAS_PTR_POS          12
#define DIR_CONFIG_ID_PTR_POS       13
#define MAX_DATA_PTRS               14
struct p_array
       {
          long   ptr[MAX_DATA_PTRS];  /* Pointer offset to the following */
                           /* information:                               */
                           /*    0  - priority              char x       */
                           /*    1  - directory             char x[]\0   */
                           /*    2  - alias name            char x[]\0   */
                           /*    3  - no. of files          char x[]\0   */
                           /*    4  - file                  char x[]\0   */
                           /*    5  - no. loc. options      char x[]\0   */
                           /*    6  - loc. options          char x[][]\0 */
                           /*    7  - local options flag    char x[]\0   */
                           /*    8  - no. std. options      char x[]\0   */
                           /*    9  - std. options          char x[][]\0 */
                           /*   10  - recipient             char x[]\0   */
                           /*   11  - scheme                char x[]\0   */
                           /*   12  - host alias            char x[]\0   */
                           /*   13  - DIR_CONFIG ID         char x[]\0   */
       };

/* Structure that holds one directory entry of the AMG database. */
struct recipient_group
       {
          char         recipient[MAX_RECIPIENT_LENGTH + 1];
          char         real_hostname[MAX_REAL_HOSTNAME_LENGTH + 1];
          char         host_alias[MAX_HOSTNAME_LENGTH + 1];
          unsigned int scheme;
       };

struct dest_group
       {
          char                   dest_group_name[MAX_GROUP_NAME_LENGTH];
          char                   options[MAX_NO_OPTIONS + 1][MAX_OPTION_LENGTH];
          struct recipient_group *rec;
          int                    rc;        /* Recipient counter. */
          int                    oc;        /* Option counter.    */
       };

struct file_group
       {
          char              file_group_name[MAX_GROUP_NAME_LENGTH];
          char              *files;
          struct dest_group *dest;
          int               fbl;            /* File buffer length. */
          int               fc;             /* File counter.       */
          int               dgc;            /* Destination group counter.*/
       };

struct dir_group
       {
          char              location[MAX_PATH_LENGTH + MAX_RECIPIENT_LENGTH];
          char              orig_dir_name[MAX_PATH_LENGTH];
                                            /* Directory name as it is   */
                                            /* in DIR_CONFIG.            */
          char              url[MAX_RECIPIENT_LENGTH];
          char              alias[MAX_DIR_ALIAS_LENGTH + 1];
          char              real_hostname[MAX_REAL_HOSTNAME_LENGTH];
          char              host_alias[MAX_HOSTNAME_LENGTH + 1];
          char              option[MAX_DIR_OPTION_LENGTH + 1];
                                            /* This is the old way of    */
                                            /* specifying options for a  */
                                            /* directory.                */
          char              dir_options[MAX_OPTION_LENGTH + 1];
                                            /* As of version 1.2.x this  */
                                            /* is the new place where    */
                                            /* options are being         */
                                            /* specified.                */
          char              type;           /* Either local or remote.   */
          unsigned int      protocol;
          unsigned int      scheme;
          unsigned int      dir_config_id;
          int               location_length;
          int               fgc;            /* file group counter        */
          struct file_group *file;
       };

struct dir_data
       {
          char          dir_name[MAX_PATH_LENGTH];
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
          char          wait_for_filename[MAX_WAIT_FOR_LENGTH]; /* Wait  */
                                            /* for the given file name|  */
                                            /* pattern before we take    */
                                            /* files from this directory.*/
          char          retrieve_work_dir[MAX_FILENAME_LENGTH];
                                            /* Storage for the local     */
                                            /* remote directory.         */
          char          ls_data_alias[MAX_DIR_ALIAS_LENGTH + 1];
                                            /* This allows the user to   */
                                            /* specify the same alias for*/
                                            /* two or more retrieve dirs.*/
                                            /* Usefull when the dir name */
                                            /* has the %T time modifier  */
                                            /* and the time frame is     */
                                            /* across a new day.         */
          char          timezone[MAX_TIMEZONE_LENGTH + 1];
                                            /* The name of the timezone  */
                                            /* is stored here when set.  */
          struct bd_time_entry te[MAX_FRA_TIME_ENTRIES];
          unsigned char no_of_time_entries;
          unsigned char remove;             /* Should the files be       */
                                            /* removed when they are     */
                                            /* being retrieved.          */
          unsigned char stupid_mode;        /* If set it will NOT        */
                                            /* collect information about */
                                            /* files that where found in */
                                            /* directory. So that when   */
                                            /* remove is not set we will */
                                            /* not always collect the    */
                                            /* same files. This ensures  */
                                            /* that files are collected  */
                                            /* only once.                */
          unsigned char delete_files_flag;  /* UNKNOWN_FILES: All unknown*/
                                            /* files will be deleted.    */
                                            /* QUEUED_FILES: Queues will */
                                            /* also be checked for old   */
                                            /* files.                    */
          unsigned char report_unknown_files;
          unsigned char force_reread;
#ifndef _WITH_PTHREAD
          unsigned char important_dir;
#endif
          unsigned char do_not_parallelize; /* When fetching files       */
                                            /* disable the parallelization.*/
          unsigned char do_not_move;        /* Force copy files into the */
                                            /* pool directory.           */
          unsigned char accept_dot_files;   /* Whether we should pick up */
                                            /* files starting with a     */
                                            /* leading dot. Default is   */
                                            /* NO.                       */
          unsigned char do_not_get_dir_list;/* The directory should not  */
                                            /* be scanned, instead look  */
                                            /* for the exact files from  */
                                            /* [files] entry. This is    */
                                            /* currently only useful for */
                                            /* HTTP, where directory     */
                                            /* listing is not supported  */
                                            /* or wanted.                */
          unsigned char url_creates_file_name;/* The given URL creates   */
                                            /* the file name when the    */
                                            /* request is issued. This   */
                                            /* is currently only useful  */
                                            /* for HTTP.                 */
          unsigned char url_with_index_file_name;/* The URL is with an   */
                                            /* index file name. This is  */
                                            /* used for non standard HTTP*/
                                            /* index file names.         */
          unsigned char no_delimiter;       /* For AWS listing do not add*/
                                            /* a delimiter. Thus showing */
                                            /* the content of all sub    */
                                            /* directories.              */
          unsigned char keep_path;          /* When storing the filename */
                                            /* from an AWS listing do not*/
                                            /* remove the path from the  */
                                            /* filename.                 */
          unsigned char dont_create_source_dir;
          unsigned char create_source_dir;  /* Create the source         */
                                            /* directory if it does not  */
                                            /* exist.                    */
          unsigned char one_process_just_scaning; /* One process just for*/
                                            /* getting the directory     */
                                            /* listing.                  */
          mode_t        dir_mode;           /* The permissions which a   */
                                            /* newly created source      */
                                            /* directory should have.    */
          char          priority;           /* Priority for this         */
                                            /* directory.                */
          unsigned int  protocol;           /* Transfer protocol that    */
                                            /* is being used.            */
          int           unknown_file_time;  /* After how many hours can  */
                                            /* a unknown file be deleted.*/
          int           queued_file_time;   /* After how many hours can  */
                                            /* a queued file be deleted. */
          int           locked_file_time;   /* After how many hours can  */
                                            /* a locked file be deleted. */
          int           unreadable_file_time;/* After how many hours can */
                                            /* a unreadable file be      */
                                            /* deleted.                  */
          int           end_character;
          unsigned int  in_dc_flag;         /* Flag to indicate which of */
                                            /* the options have been     */
                                            /* stored in DIR_CONFIG. This*/
                                            /* is useful for restoring   */
                                            /* the DIR_CONFIG from       */
                                            /* scratch.                  */
          unsigned int  accumulate;         /* How many files need to    */
                                            /* accumulate before start   */
                                            /* sending from this dir.    */
          unsigned int  ignore_file_time;   /* Ignore files which are    */
                                            /* older, equal or newer     */
                                            /* the given time in sec.    */
          unsigned int  gt_lt_sign;         /* The sign for the following*/
                                            /* variables:                */
                                            /*     ignore_size           */
                                            /*     ignore_file_time      */
                                            /* These are bit masked      */
                                            /* for each variable.        */
                                            /*+---+---------------------+*/
                                            /*|Bit|       Meaning       |*/
                                            /*+---+---------------------+*/
                                            /*| 1 | ISIZE_EQUAL         |*/
                                            /*| 2 | ISIZE_LESS_THEN     |*/
                                            /*| 3 | ISIZE_GREATER_THEN  |*/
                                            /*+---+---------------------+*/
                                            /*| 4 | IFTIME_EQUAL        |*/
                                            /*| 5 | IFTIME_LESS_THEN    |*/
                                            /*| 6 | IFTIME_GREATER_THEN |*/
                                            /*+---+---------------------+*/
                                            /*| * | Rest not used.      |*/
                                            /*+---+---------------------+*/
          unsigned int  keep_connected;     /* After all files have been */
                                            /* retrieved, the time to    */
                                            /* stay connected.           */
#ifdef WITH_DUP_CHECK
          unsigned int  dup_check_flag;     /* Flag storing the type of  */
                                            /* check that is to be done  */
                                            /* and what type of CRC to   */
                                            /* use:                      */
                                            /*+------+------------------+*/
                                            /*|Bit(s)|     Meaning      |*/
                                            /*+------+------------------+*/
                                            /*|18-32 | Not used.        |*/
                                            /*|   17 | DC_CRC32C        |*/
                                            /*|   16 | DC_CRC32         |*/
                                            /*| 4-15 | Not used.        |*/
                                            /*|    3 | DC_FILE_CONT_NAME|*/
                                            /*|    2 | DC_FILE_CONTENT  |*/
                                            /*|    1 | DC_FILENAME_ONLY |*/
                                            /*+------+------------------+*/
#endif
          unsigned int  max_copied_files;   /* Maximum number of files   */
                                            /* that we copy in one go.   */
          unsigned int  dir_id;             /* CRC-32 checksum of this   */
                                            /* directory entry.          */
          unsigned int  dir_config_id;      /* To find out from which    */
                                            /* DIR_CONFIG this job comes.*/
#ifdef WITH_INOTIFY
          unsigned int  inotify_flag;       /* Which inotify options are */
                                            /* set:                      */
                                            /*+----+--------------------+*/
                                            /*|Bit |      Meaning       |*/
                                            /*+----+--------------------+*/
                                            /*|  5 | INOTIFY_ATTRIB_FLAG|*/
                                            /*|  4 | INOTIFY_DELETE_FLAG|*/
                                            /*|  3 | INOTIFY_CREATE_FLAG|*/
                                            /*|  2 | INOTIFY_CLOSE_FLAG |*/
                                            /*|  1 | INOTIFY_RENAME_FLAG|*/
                                            /*+------+------------------+*/
#endif
          int           fsa_pos;            /* FSA position, only used   */
                                            /* for retrieving files.     */
          int           dir_pos;            /* Position of this directory*/
                                            /* in the directory name     */
                                            /* buffer.                   */
          int           max_process;
          int           max_errors;         /* Max errors before we ring */
                                            /* the alarm bells.          */
#ifdef WITH_DUP_CHECK
          time_t        dup_check_timeout;  /* When the stored CRC for   */
                                            /* duplicate checks are no   */
                                            /* longer valid. Value is in */
                                            /* seconds.                  */
#endif
          time_t        info_time;          /* Time when to inform that  */
                                            /* the directory has not     */
                                            /* received any data.        */
          time_t        warn_time;          /* Time when to warn that the*/
                                            /* directory has not received*/
                                            /* any data.                 */
          off_t         max_copied_file_size; /* The maximum number of   */
                                            /* bytes that we copy in one */
                                            /* go.                       */
          off_t         accumulate_size;    /* Ditto, only here we look  */
                                            /* at the size.              */
          off_t         ignore_size;        /* Ignore any files less     */
                                            /* then, equal or greater    */
                                            /* then the given size.      */
       };

/* Definitions for a flag in the structure instant_db (lfs) or */
/* the structure directory_entry (flag).                       */
#define IN_SAME_FILESYSTEM  1
#define ALL_FILES           2
#define RENAME_ONE_JOB_ONLY 4
#define GO_PARALLEL         8
#define SPLIT_FILE_LIST     16
#define DO_NOT_LINK_FILES   32
#define DELETE_ALL_FILES    64
#define EXPAND_DIR          128

/* Structure that holds a single job for process dir_check. */
struct instant_db
       {
#ifdef MULTI_FS_SUPPORT
          int           ewl_pos;       /* Position in extra work dir.    */
#endif
          unsigned int  job_id;        /* Since each host can have       */
                                       /* different type of jobs (other  */
                                       /* user, different directory,     */
                                       /* other options, etc), each of   */
                                       /* these is identified by this    */
                                       /* number. This job number is     */
                                       /* always unique even when re-    */
                                       /* reading the DIR_CONFIG file.   */
          unsigned int  dir_config_id; /* To find out from which         */
                                       /* DIR_CONFIG this job comes.     */
          unsigned int  loptions_flag; /* Flag showing which local       */
                                       /* options are set.               */
          char          str_job_id[MAX_INT_HEX_LENGTH];
          char          *dir;          /* Directory that has to be       */
                                       /* monitored.                     */
          unsigned int  dir_id;        /* Contains the directory ID!!!   */
          unsigned int  lfs;           /* Flag to indicate if this       */
                                       /* directory belongs to the same  */
                                       /* filesystem as the working      */
                                       /* directory of the AFD, i.e if   */
                                       /* we have to fork or not.        */
          char          time_option_type;/* The type of time option:     */
                                       /* SEND_COLLECT_TIME,             */
                                       /* SEND_NO_COLLECT_TIME and       */
                                       /* NO_TIME.                       */
          time_t        next_start_time;
          struct bd_time_entry *te;
          unsigned int  no_of_time_entries;
          char          timezone[MAX_TIMEZONE_LENGTH + 1];
                                       /* The name of the timezone is    */
                                       /* stored here when set.          */
          unsigned int  file_mask_id;  /* CRC-32 checksum of file masks. */
          int           no_of_files;   /* Number of files to be send.    */
          int           fbl;           /* File mask buffer length.       */
          char          *files;        /* The name of the file(s) to be  */
                                       /* send.                          */
          int           no_of_loptions;/* Holds number of local          */
                                       /* options.                       */
          char          *loptions;     /* Storage of local options.      */
          int           no_of_soptions;/* Holds number of standard       */
                                       /* options.                       */
          char          *soptions;     /* Storage of standard options.   */
          char          *recipient;    /* Storage of recipients.         */
          char          *host_alias;   /* Alias of hostname of recipient.*/
          unsigned int  recipient_id;  /* CRC-32 checksum of recipient.  */
          unsigned int  host_id;       /* CRC-32 checksum of host_alias. */
          unsigned int  protocol;
          unsigned int  age_limit;
          char          dup_paused_dir;/* Flag to indicate that this     */
                                       /* paused directory is already    */
                                       /* in the list and does not have  */
                                       /* to be checked again by         */
                                       /* check_paused_dir().            */
          char          paused_dir[MAX_PATH_LENGTH];
                                       /* Stores absolute path of paused */
                                       /* or queued hosts.               */
          int           position;      /* Location of this host in the   */
                                       /* FSA.                           */
          int           fra_pos;       /* Location of this job in the    */
                                       /* FRA.                           */
          char          priority;      /* Priority of job.               */
       };

/* Structure that holds all file information for one directory. */
struct file_mask_entry
       {
          int  nfm;                          /* Number of file masks     */
          int  dest_count;                   /* Number of destinations   */
                                             /* for this directory.      */
          int  *pos;                         /* This holds the equivalent*/
                                             /* position in the struct   */
                                             /* instant_db.              */
          char **file_mask;
       };
struct directory_entry
       {
          unsigned int           dir_id;     /* When reading a new       */
                                             /* DIR_CONFIG file this     */
                                             /* number is used to back-  */
                                             /* trace where a certain    */
                                             /* file comes from in the   */
                                             /* input log.               */
          int                    nfg;        /* Number of file groups    */
          int                    fra_pos;
          int                    rl_fd;      /* Retrieve list file       */
                                             /* descriptor.              */
          off_t                  rl_size;    /* Size of retrieve list.   */
          time_t                 search_time;/* Used when a new file     */
                                             /* arrives and we already   */
                                             /* have checked the         */
                                             /* directory and it is      */
                                             /* still the same second.   */
#ifdef MULTI_FS_SUPPORT
          int                    ewl_pos;    /* Position in extra work dir.*/
          dev_t                  dev;        /* Device number of file-   */
                                             /* system.                  */
#endif
          int                    *no_of_listed_files;
          struct retrieve_list   *rl;
          struct file_mask_entry *fme;
          unsigned int           flag;       /* Flag to if all files in  */
                                             /* this directory are to be */
                                             /* distributed (ALL_FILES)  */
                                             /* or if this directory is  */
                                             /* in the same filesystem   */
                                             /* (IN_SAME_FILESYSTEM).    */
          char                   *alias;     /* Alias name of the        */
                                             /* directory.               */
          char                   *dir;       /* Pointer to directory     */
                                             /* name.                    */
          char                   *paused_dir;/* Holds the full paused    */
                                             /* REMOTE directory.        */
       };

struct message_buf
       {
          char bin_msg_name[MAX_BIN_MSG_LENGTH];
       };

struct dc_proc_list
       {
          pid_t        pid;
          int          fra_pos;
          unsigned int job_id;
       };

struct dc_filter_list
       {
          int  length;
          char *dc_filter;
          char is_filter;
       };

#define DIR_CONFIG_NAME_STEP_SIZE 10
struct dir_config_buf
       {
          unsigned int  dc_id;
          time_t        dc_old_time;
          off_t         size;             /* Note: Not always set. */
          char          *dir_config_file;
          char          is_filter;
          char          in_list;
#ifdef WITH_ONETIME
          unsigned char type;
#endif
       };

struct fork_job_data
       {
          unsigned int job_id;
          unsigned int forks;
          clock_t      user_time;
          clock_t      system_time;
       };

#ifdef _DISTRIBUTION_LOG
struct file_dist_list
       {
          int           no_of_dist;
          unsigned int  *jid_list;
          unsigned char *proc_cycles;
       };
#endif

#ifdef WITH_INOTIFY
struct inotify_watch_list
       {
          int   wd;              /* Watch descriptor.                    */
          int   de_pos;          /* struct directory_entry position.     */
          int   no_of_files;     /* Number of file name stored.          */
          int   cur_fn_length;   /* Current file name length.            */
          int   alloc_fn_length; /* Allocated mem size for file_name.    */
          char  *file_name;      /* Chain of file names each terminated  */
                                 /* by '\0'.                             */
          short *fnl;            /* File name length list.               */
       };
#endif

#define BUL_TYPE_INP   1 /* input (inp)   */
#define BUL_TYPE_IGN   2 /* ignore (ign)  */
#define BUL_TYPE_CMP   3 /* compile (cmp) */
#define BUL_SPEC_DUP   1 /* duplicate check */
struct wmo_bul_list
       {
          char          TTAAii[7];
          char          CCCC[5];
          char          BTIME[9];
          char          ITIME[9];
          char          type;        /* 1 - input (inp)          */
                                     /* 2 - ignore (ign)         */
                                     /* 3 - compile (cmp)        */
          char          spec;
          short         rss;         /* Report Sub Specification */
                                     /* -1 is no report          */
       };
#define RT_NOT_DEFINED  0
#define RT_TEXT         1 /* TEXT        */
#define RT_CLIMAT       2 /* CLIMAT      */
#define RT_TAF          3 /* TAF         */
#define RT_METAR        4 /* METAR       */
#define RT_SPECIAL_01   5 /* SPECIAL-01  */
#define RT_SPECIAL_02   6 /* SPECIAL-02  */
#define RT_SPECIAL_03   7 /* SPECIAL-03  */
#define RT_SPECIAL_66   8 /* SPECIAL-66  */
#define RT_SYNOP        9 /* SYNOP       */
#define RT_SYNOP_SHIP  10 /* SYNOP-SHIP  */
#define RT_UPPER_AIR   11 /* UPPER-AIR   */
#define RT_ATEXT       12 /* AIR TEXT    */
#define RT_SYNOP_MOBIL 13 /* SYNOP-MOBIL */
#define STID_IIiii      1
#define STID_CCCC       2
struct wmo_rep_list
       {
          char          TT[2];
          char          BTIME[6];
          char          ITIME[6];
          char          mimj[2];
          char          wid[2];      /* Wind Indicator           */
          unsigned char rt;          /* Report type.             */
          char          stid;        /* D - IIiii                */
                                     /* L - CCCC                 */
          short         rss;         /* Report Sub Specification */
       };

/* Function prototypes. */
extern int    amg_zombie_check(pid_t *, int),
              check_full_dc_name_changes(void),
              check_group_list_mtime(void),
#ifdef HAVE_STATX
              check_list(struct directory_entry *, char *, struct statx *),
#else
              check_list(struct directory_entry *, char *, struct stat *),
#endif
              check_option(char *, FILE *),
              com(char, char *, int),
              convert(char *, char *, int, int, unsigned int, unsigned int,
                      off_t *),
#ifdef _WITH_PTHREAD
              check_files(struct directory_entry *, char *, int, char *,
                          int, int *, time_t, int *, off_t *, time_t *,
                          char **, unsigned char *,
# if defined (_MAINTAINER_LOG) && defined (SHOW_FILE_MOVING)
                          char *, int,
# endif
                          off_t *),
              count_pool_files(int *, char *, off_t *, time_t *, char **,
                               unsigned char *),
              link_files(char *, char *, int, time_t, off_t *, char **,
# if defined (_MAINTAINER_LOG) && defined (SHOW_FILE_MOVING)
                         char *, int,
# endif
                         struct directory_entry *, struct instant_db *,
                         time_t *, unsigned int *, int, int, int, char *,
                         off_t *),
              save_files(char *, char *, time_t, unsigned int, off_t *,
                         time_t *, char **,
# if defined (_MAINTAINER_LOG) && defined (SHOW_FILE_MOVING)
                         char *, int,
# endif
                         struct directory_entry *,
# ifdef _DISTRIBUTION_LOG
                         struct instant_db *, int, int, char, int, int),
# else
                         struct instant_db *, int, int, char, int),
# endif
#else
              check_files(struct directory_entry *, char *, int, char *,
                          int, int *, time_t, int *,
# if defined (_MAINTAINER_LOG) && defined (SHOW_FILE_MOVING)
                          char *, int,
# endif
                          off_t *),
              count_pool_files(int *, char *),
              link_files(char *, char *, int, time_t,
# if defined (_MAINTAINER_LOG) && defined (SHOW_FILE_MOVING)
                         char *, int,
# endif
                         struct directory_entry *,
                         struct instant_db *, unsigned int *, int, int, int,
                         char *, off_t *),
              save_files(char *, char *, time_t, unsigned int,
# if defined (_MAINTAINER_LOG) && defined (SHOW_FILE_MOVING)
                         char *, int,
# endif
                         struct directory_entry *, struct instant_db *,
# ifdef _DISTRIBUTION_LOG
                         int, int, char, int, int),
# else
                         int, int, char, int),
# endif
#endif
#ifdef WITH_INOTIFY
              check_inotify_files(struct inotify_watch_list *,
                                  struct directory_entry *, char *, int *,
                                  time_t,
# if defined (_MAINTAINER_LOG) && defined (SHOW_FILE_MOVING)
                                  char *, int,
# endif
                                  off_t *),
#endif
              check_process_list(int),
              create_db(FILE *, int),
#ifdef WITH_ONETIME
              eval_dir_config(off_t, unsigned int *, FILE *, int, int *),
#else
              eval_dir_config(off_t, unsigned int *, FILE *, int *),
#endif
              eval_dir_options(int, char, char *, FILE *),
              get_last_char(char *, off_t),
              handle_options(int, time_t, unsigned int, unsigned int,
                             char *, int *, off_t *),
              in_time(time_t, unsigned int, struct bd_time_entry *),
              lookup_dir_id(char *, char *),
              lookup_fra_pos(char *),
              next_dir_group_name(char *, int *, char *),
              next_recipient_group_name(char *),
              rename_files(char *, char *, int, int, struct instant_db *,
                           time_t, int, unsigned int *, char *,
# if defined (_MAINTAINER_LOG) && defined (SHOW_FILE_MOVING)
                           char *, int,
# endif
                           off_t *),
              reread_dir_config(int, off_t, time_t *, int, int, size_t,
                                int, int, int*, unsigned int *, FILE *, pid_t,
                                struct host_list *),
              reread_host_config(time_t *, int *, int *, size_t *,
                                 struct host_list **, unsigned int *,
                                 FILE *, int),
              timezone_name_check(const char *);
extern pid_t  make_process_amg(char *, char *, int, int, mode_t, pid_t);
extern off_t  fax2gts(char *, char *, int);
extern char   *check_paused_dir(struct directory_entry *, int *, int *, int *),
              *convert_fsa(int, char *, off_t *, int, unsigned char,
                           unsigned char),
              *convert_fra(int, char *, off_t *, int, unsigned char,
                           unsigned char),
              *convert_jid(int, char *, size_t *, int, unsigned char,
                           unsigned char),
              *get_com_action_str(int),
              *next(char *);

extern void   add_file_mask(char *, struct dir_group *),
#ifdef MULTI_FS_SUPPORT
              check_file_dir(time_t, dev_t, char *, int),
#else
              check_file_dir(time_t, char *, int),
#endif
              check_file_pool_mem(int),
              check_old_time_jobs(int, char *),
              clear_msg_buffer(void),
              clear_pool_dir(void),
              create_fsa(void),
              create_fra(int),
              create_sa(int),
#ifdef WITH_INOTIFY
              del_unknown_inotify_files(time_t),
#endif
#ifdef _DISTRIBUTION_LOG
              dis_log(unsigned char, time_t, unsigned int, unsigned int,
                      char *, int, off_t, int, unsigned int **, unsigned char *,
                      unsigned int),
#endif
              enter_time_job(int),
              eval_bul_rep_config(char *, char *, int),
              free_dir_group_name(void),
              free_group_list_mtime(void),
              free_recipient_group_name(void),
              get_file_group(char *, int, struct dir_group *, int *),
              get_full_dc_names(char *, off_t *),
              handle_time_jobs(time_t),
              init_dir_check(int, char **, time_t *,
#ifdef WITH_ONETIME
                             int *,
# ifdef WITHOUT_FIFO_RW_SUPPORT
                             int *,
# endif
#endif
#ifdef WITH_INOTIFY
                             int *,
#endif
                             int *, int *, int *),
              init_dir_group_name(char *, int *, char *, int),
              init_recipient_group_name(char *, char *, int),
              init_dis_log(void),
              init_group_list_mtime(void),
              init_job_data(void),
              init_msg_buffer(void),
              lookup_dc_id(struct dir_config_buf **, int),
              lookup_file_mask_id(struct instant_db *, int),
              lookup_job_id(struct instant_db *, unsigned int *, char *, int),
              receive_log(char *, char *, int, time_t, char *, ...),
              release_dis_log(void),
              remove_old_ls_data_files(void),
              remove_pool_directory(char *, unsigned int),
#ifdef _DELETE_LOG
              remove_time_dir(char *, char *, int, unsigned int, unsigned int,
                              int, char *, int),
#else
              remove_time_dir(char *, char *, int, unsigned int),
#endif
              rm_removed_files(struct directory_entry *, int, char *),
              search_old_files(time_t),
              send_message(char *,
#ifdef MULTI_FS_SUPPORT
                           dev_t,
#endif
                           char *, unsigned int, unsigned int,
                           time_t, int,
#ifdef _WITH_PTHREAD
# if defined (_DELETE_LOG) || defined (_PRODUCTION_LOG)
                           off_t *, char **, unsigned char *,
# endif
#endif
                           int, int, off_t, int),
              show_shm(FILE *),
              sort_time_job(void),
              store_file_mask(char *, struct dir_group *),
              store_passwd(char *, char *, char *);
#endif /* __amgdefs_h */
