/*
 *  sftpdefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 - 2025 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __sftpdefs_h
#define __sftpdefs_h

#define DEFAULT_ADD_SFTP_HEADER_LENGTH  1024
#define MIN_SFTP_BLOCKSIZE              32768     /*  32 KBytes */
#define MAX_SFTP_BLOCKSIZE              262144    /* 256 KBytes */
#define MAX_PENDING_WRITE_BUFFER        786432    /* 768 KBytes */
#define INITIAL_SFTP_MSG_LENGTH         (MIN_SFTP_BLOCKSIZE + DEFAULT_ADD_SFTP_HEADER_LENGTH)
#define MAX_PENDING_WRITES              (MAX_PENDING_WRITE_BUFFER / 16384)
#define SFTP_DEFAULT_MAX_OPEN_REQUEST   64
#define MAX_PENDING_READS               SFTP_DEFAULT_MAX_OPEN_REQUEST
#define MAX_SFTP_REPLY_BUFFER           (SFTP_DEFAULT_MAX_OPEN_REQUEST + 10)
#define SFTP_READ_STEP_SIZE             4

#define SFTP_WRITE_FILE                 1 /* Open file for writting. */
#define SFTP_READ_FILE                  2 /* Open file for reading.  */
#define SFTP_DO_SINGLE_READS            -4
#define SFTP_EOF                        -5

#define SSH_FILEXFER_VERSION                6

/* Command types. */
#define SSH_FXP_INIT                        1          /* 3 - 6+  */
#define SSH_FXP_VERSION                     2          /* 3 - 6+  */
#define SSH_FXP_OPEN                        3          /* 3 - 6+  */
#define SSH_FXP_CLOSE                       4          /* 3 - 6+  */
#define SSH_FXP_READ                        5          /* 3 - 6+  */
#define SSH_FXP_WRITE                       6          /* 3 - 6+  */
#define SSH_FXP_LSTAT                       7          /* 3 - 6+  */
#define SSH_FXP_FSTAT                       8          /* 3 - 6+  */
#define SSH_FXP_SETSTAT                     9          /* 3 - 6+  */
#define SSH_FXP_FSETSTAT                    10         /* 3 - 6+  */
#define SSH_FXP_OPENDIR                     11         /* 3 - 6+  */
#define SSH_FXP_READDIR                     12         /* 3 - 6+  */
#define SSH_FXP_REMOVE                      13         /* 3 - 6+  */
#define SSH_FXP_MKDIR                       14         /* 3 - 6+  */
#define SSH_FXP_RMDIR                       15         /* 3 - 6+  */
#define SSH_FXP_REALPATH                    16         /* 3 - 6+  */
#define SSH_FXP_STAT                        17         /* 3 - 6+  */
#define SSH_FXP_RENAME                      18         /* 3 - 6+  */
#define SSH_FXP_READLINK                    19         /* 3 - 6+  */
#define SSH_FXP_SYMLINK                     20         /* 3       */
#define SSH_FXP_LINK                        21         /* 6+      */
#define SSH_FXP_BLOCK                       22         /* 6+      */
#define SSH_FXP_UNBLOCK                     23         /* 6+      */

/* Command response types. */
#define SSH_FXP_STATUS                      101        /* 3 - 6+  */
#define SSH_FXP_HANDLE                      102        /* 3 - 6+  */
#define SSH_FXP_DATA                        103        /* 3 - 6+  */
#define SSH_FXP_NAME                        104        /* 3 - 6+  */
#define SSH_FXP_ATTRS                       105        /* 3 - 6+  */

#define SSH_FXP_EXTENDED                    200        /* 3 - 6+  */
#define SSH_FXP_EXTENDED_REPLY              201        /* 3 - 6+  */

/* Possible flags for renaming. */
#define SSH_FXF_RENAME_OVERWRITE            0x00000001 /* 6+      */
#define SSH_FXF_RENAME_ATOMIC               0x00000002 /* 6+      */
#define SSH_FXF_RENAME_NATIVE               0x00000004 /* 6+      */

/* Flags for opening a file. */
#define SSH_FXF_ACCESS_DISPOSITION          0x00000007 /* 6+      */
#define     SSH_FXF_CREATE_NEW              0x00000000 /* 6+      */
#define     SSH_FXF_CREATE_TRUNCATE         0x00000001 /* 6+      */
#define     SSH_FXF_OPEN_EXISTING           0x00000002 /* 6+      */
#define     SSH_FXF_OPEN_OR_CREATE          0x00000003 /* 6+      */
#define     SSH_FXF_TRUNCATE_EXISTING       0x00000004 /* 6+      */
#define SSH_FXF_APPEND_DATA                 0x00000008 /* 6+      */
#define SSH_FXF_APPEND_DATA_ATOMIC          0x00000010 /* 6+      */
#define SSH_FXF_TEXT_MODE                   0x00000020 /* 6+      */
#define SSH_FXF_BLOCK_READ                  0x00000040 /* 6+      */
#define SSH_FXF_BLOCK_WRITE                 0x00000080 /* 6+      */
#define SSH_FXF_BLOCK_DELETE                0x00000100 /* 6+      */
#define SSH_FXF_BLOCK_ADVISORY              0x00000200 /* 6+      */
#define SSH_FXF_NOFOLLOW                    0x00000400 /* 6+      */
#define SSH_FXF_DELETE_ON_CLOSE             0x00000800 /* 6+      */
#define SSH_FXF_ACCESS_AUDIT_ALARM_INFO     0x00001000 /* 6+      */
#define SSH_FXF_ACCESS_BACKUP               0x00002000 /* 6+      */
#define SSH_FXF_BACKUP_STREAM               0x00004000 /* 6+      */
#define SSH_FXF_OVERRIDE_OWNER              0x00008000 /* 6+      */

/* Definitions for older protocol versions. */
#define SSH_FXF_READ                        0x00000001 /* 3       */
#define SSH_FXF_WRITE                       0x00000002 /* 3       */
#define SSH_FXF_APPEND                      0x00000004 /* 3       */
#define SSH_FXF_CREAT                       0x00000008 /* 3       */
#define SSH_FXF_TRUNC                       0x00000010 /* 3       */
#define SSH_FXF_EXCL                        0x00000020 /* 3       */

/* Access mask. */
#define ACE4_READ_DATA                      0x00000001 /* 6+      */
#define ACE4_LIST_DIRECTORY                 0x00000001 /* 6+      */
#define ACE4_WRITE_DATA                     0x00000002 /* 6+      */
#define ACE4_ADD_FILE                       0x00000002 /* 6+      */
#define ACE4_APPEND_DATA                    0x00000004 /* 6+      */
#define ACE4_ADD_SUBDIRECTORY               0x00000004 /* 6+      */
#define ACE4_READ_NAMED_ATTRS               0x00000008 /* 6+      */
#define ACE4_WRITE_NAMED_ATTRS              0x00000010 /* 6+      */
#define ACE4_EXECUTE                        0x00000020 /* 6+      */
#define ACE4_DELETE_CHILD                   0x00000040 /* 6+      */
#define ACE4_READ_ATTRIBUTES                0x00000080 /* 6+      */
#define ACE4_WRITE_ATTRIBUTES               0x00000100 /* 6+      */
#define ACE4_DELETE                         0x00010000 /* 6+      */
#define ACE4_READ_ACL                       0x00020000 /* 6+      */
#define ACE4_WRITE_ACL                      0x00040000 /* 6+      */
#define ACE4_WRITE_OWNER                    0x00080000 /* 6+      */
#define ACE4_SYNCHRONIZE                    0x00100000 /* 6+      */

/* Types of files. */
#define SSH_FILEXFER_TYPE_REGULAR           1          /* 6+      */
#define SSH_FILEXFER_TYPE_DIRECTORY         2          /* 6+      */
#define SSH_FILEXFER_TYPE_SYMLINK           3          /* 6+      */
#define SSH_FILEXFER_TYPE_SPECIAL           4          /* 6+      */
#define SSH_FILEXFER_TYPE_UNKNOWN           5          /* 6+      */
#define SSH_FILEXFER_TYPE_SOCKET            6          /* 6+      */
#define SSH_FILEXFER_TYPE_CHAR_DEVICE       7          /* 6+      */
#define SSH_FILEXFER_TYPE_BLOCK_DEVICE      8          /* 6+      */
#define SSH_FILEXFER_TYPE_FIFO              9          /* 6+      */

/* File attribute flags. */                            /* Version */
#define SSH_FILEXFER_ATTR_SIZE              0x00000001 /* 3 - 6+  */
#define SSH_FILEXFER_ATTR_UIDGID            0x00000002 /* 3       */
#define SSH_FILEXFER_ATTR_PERMISSIONS       0x00000004 /* 3 - 6+  */
#define SSH_FILEXFER_ATTR_ACMODTIME         0x00000008 /* 3       */
#define SSH_FILEXFER_ATTR_ACCESSTIME        0x00000008 /* 6+      */
#define SSH_FILEXFER_ATTR_CREATETIME        0x00000010 /* 6+      */
#define SSH_FILEXFER_ATTR_MODIFYTIME        0x00000020 /* 6+      */
#define SSH_FILEXFER_ATTR_ACL               0x00000040 /* 6+      */
#define SSH_FILEXFER_ATTR_OWNERGROUP        0x00000080 /* 6+      */
#define SSH_FILEXFER_ATTR_SUBSECOND_TIMES   0x00000100 /* 6+      */
#define SSH_FILEXFER_ATTR_BITS              0x00000200 /* 6+      */
#define SSH_FILEXFER_ATTR_ALLOCATION_SIZE   0x00000400 /* 6+      */
#define SSH_FILEXFER_ATTR_TEXT_HINT         0x00000800 /* 6+      */
#define SSH_FILEXFER_ATTR_MIME_TYPE         0x00001000 /* 6+      */
#define SSH_FILEXFER_ATTR_LINK_COUNT        0x00002000 /* 6+      */
#define SSH_FILEXFER_ATTR_UNTRANSLATED_NAME 0x00004000 /* 6+      */
#define SSH_FILEXFER_ATTR_CTIME             0x00008000 /* 6+      */
#define SSH_FILEXFER_ATTR_EXTENDED          0x80000000 /* 3 - 6+  */

/* Error codes. */
#define SSH_FX_OK                           0          /* 3 - 6+  */
#define SSH_FX_EOF                          1          /* 3 - 6+  */
#define SSH_FX_NO_SUCH_FILE                 2          /* 3 - 6+  */
#define SSH_FX_PERMISSION_DENIED            3          /* 3 - 6+  */
#define SSH_FX_FAILURE                      4          /* 3 - 6+  */
#define SSH_FX_BAD_MESSAGE                  5          /* 3 - 6+  */
#define SSH_FX_NO_CONNECTION                6          /* 3 - 6+  */
#define SSH_FX_CONNECTION_LOST              7          /* 3 - 6+  */
#define SSH_FX_OP_UNSUPPORTED               8          /* 3 - 6+  */
#define SSH_FX_INVALID_HANDLE               9          /* 6+      */
#define SSH_FX_NO_SUCH_PATH                 10         /* 6+      */
#define SSH_FX_FILE_ALREADY_EXISTS          11         /* 6+      */
#define SSH_FX_WRITE_PROTECT                12         /* 6+      */
#define SSH_FX_NO_MEDIA                     13         /* 6+      */
#define SSH_FX_NO_SPACE_ON_FILESYSTEM       14         /* 6+      */
#define SSH_FX_QUOTA_EXCEEDED               15         /* 6+      */
#define SSH_FX_UNKNOWN_PRINCIPAL            16         /* 6+      */
#define SSH_FX_LOCK_CONFLICT                17         /* 6+      */
#define SSH_FX_DIR_NOT_EMPTY                18         /* 6+      */
#define SSH_FX_NOT_A_DIRECTORY              19         /* 6+      */
#define SSH_FX_INVALID_FILENAME             20         /* 6+      */
#define SSH_FX_LINK_LOOP                    21         /* 6+      */
#define SSH_FX_CANNOT_DELETE                22         /* 6+      */
#define SSH_FX_INVALID_PARAMETER            23         /* 6+      */
#define SSH_FX_FILE_IS_A_DIRECTORY          24         /* 6+      */
#define SSH_FX_BYTE_RANGE_LOCK_CONFLICT     25         /* 6+      */
#define SSH_FX_BYTE_RANGE_LOCK_REFUSED      26         /* 6+      */
#define SSH_FX_DELETE_PENDING               27         /* 6+      */
#define SSH_FX_FILE_CORRUPT                 28         /* 6+      */
#define SSH_FX_OWNER_INVALID                29         /* 6+      */
#define SSH_FX_GROUP_INVALID                30         /* 6+      */
#define SSH_FX_NO_MATCHING_BYTE_RANGE_LOCK  31         /* 6+      */

/* Definitions for different extensions. */
#define OPENSSH_POSIX_RENAME_EXT        "posix-rename@openssh.com"
#define OPENSSH_POSIX_RENAME_EXT_LENGTH (sizeof(OPENSSH_POSIX_RENAME_EXT) - 1)
#define OPENSSH_STATFS_EXT              "statvfs@openssh.com"
#define OPENSSH_STATFS_EXT_LENGTH       (sizeof(OPENSSH_STATFS_EXT) - 1)
#define OPENSSH_FSTATFS_EXT             "fstatvfs@openssh.com"
#define OPENSSH_FSTATFS_EXT_LENGTH      (sizeof(OPENSSH_FSTATFS_EXT) - 1)
#define OPENSSH_HARDLINK_EXT            "hardlink@openssh.com"
#define OPENSSH_HARDLINK_EXT_LENGTH     (sizeof(OPENSSH_HARDLINK_EXT) - 1)
#define OPENSSH_FSYNC_EXT               "fsync@openssh.com"
#define OPENSSH_FSYNC_EXT_LENGTH        (sizeof(OPENSSH_FSYNC_EXT) - 1)
#define OPENSSH_LSETSTAT_EXT            "lsetstat@openssh.com"
#define OPENSSH_LSETSTAT_EXT_LENGTH     (sizeof(OPENSSH_LSETSTAT_EXT) - 1)
#define OPENSSH_LIMITS_EXT              "limits@openssh.com"
#define OPENSSH_LIMITS_EXT_LENGTH       (sizeof(OPENSSH_LIMITS_EXT) - 1)
#define OPENSSH_EXPAND_PATH_EXT         "expand-path@openssh.com"
#define OPENSSH_EXPAND_PATH_EXT_LENGTH  (sizeof(OPENSSH_EXPAND_PATH_EXT) - 1)
#define COPY_DATA_EXT                   "copy-data"
#define COPY_DATA_EXT_LENGTH            (sizeof(COPY_DATA_EXT) - 1)
#define SUPPORTED2_EXT                  "supported2"
#define SUPPORTED2_EXT_LENGTH           (sizeof(SUPPORTED2_EXT) - 1)

/* Definitions for support2 structure (Version 6). */
#define S2_SUPPORTED_ATTRIBUTE_MASK           "supported-attribute-mask"
#define S2_SUPPORTED_ATTRIBUTE_MASK_LENGTH    (sizeof(S2_SUPPORTED_ATTRIBUTE_MASK) - 1)
#define S2_SUPPORTED_ATTRIBUTE_BITS           "supported-attribute-bits"
#define S2_SUPPORTED_ATTRIBUTE_BITS_LENGTH    (sizeof(S2_SUPPORTED_ATTRIBUTE_BITS) - 1)
#define S2_SUPPORTED_OPEN_FLAGS               "supported-open-flags"
#define S2_SUPPORTED_OPEN_FLAGS_LENGTH        (sizeof(S2_SUPPORTED_OPEN_FLAGS) - 1)
#define S2_SUPPORTED_ACCESS_MASK              "supported-access-mask"
#define S2_SUPPORTED_ACCESS_MASK_LENGTH       (sizeof(S2_SUPPORTED_ACCESS_MASK) - 1)
#define S2_MAX_READ_SIZE                      "max-read-size"
#define S2_MAX_READ_SIZE_LENGTH               (sizeof(S2_MAX_READ_SIZE) - 1)
#define S2_SUPPORTED_OPEN_BLOCK_VECTOR        "supported-open-block-vector"
#define S2_SUPPORTED_OPEN_BLOCK_VECTOR_LENGTH (sizeof(S2_SUPPORTED_OPEN_BLOCK_VECTOR) - 1)
#define S2_SUPPORTED_BLOCK_VECTOR             "supported-block-vector"
#define S2_SUPPORTED_BLOCK_VECTOR_LENGTH      (sizeof(S2_SUPPORTED_BLOCK_VECTOR) - 1)
#define S2_ATTRIB_EXTENSION_NAME              "attrib-extension-name"
#define S2_ATTRIB_EXTENSION_NAME_LENGTH       (sizeof(S2_ATTRIB_EXTENSION_NAME) - 1)
#define S2_EXTENSION_NAME                     "extension-name"
#define S2_EXTENSION_NAME_LENGTH              (sizeof(S2_EXTENSION_NAME) - 1)

/* Different modes for function show_sftp_cmd() */
#define SSC_HANDLED     1
#define SSC_TO_BUFFER   2
#define SSC_FROM_BUFFER 3
#define SSC_DELETED     4

/* Local function return definitions. */
#define SFTP_BLOCKSIZE_CHANGED    3

/* Storage for whats returned in SSH_FXP_NAME reply. */
struct name_list
       {
          char         *name;
          struct stat  stat_buf;
          unsigned int stat_flag;
       };

struct openssh_sftp_limits
       {
          u_long_64 max_packet_length; /* Used */
          u_long_64 max_read_length;   /* Used */
          u_long_64 max_write_length;  /* Used */
          u_long_64 max_open_handles;  /* Used */
       };

struct stored_messages
       {
          unsigned int request_id;
          unsigned int message_length;
          char         *sm_buffer;      /* Stored message buffer. */
       };

struct supported2
       {
          unsigned int   supported_attribute_mask;    /* Unused */
          unsigned int   supported_attribute_bits;    /* Unused */
          unsigned int   supported_open_flags;        /* Unused */
          unsigned int   supported_access_mask;       /* Unused */
          unsigned int   max_read_size;               /* Unused */
          unsigned short supported_open_block_vector; /* Unused */
          unsigned short supported_block_vector;      /* Unused */
          unsigned int   attrib_extension_count;      /* Unused */
          unsigned int   extension_count;             /* Unused */
       };

struct sftp_connect_data
       {
          unsigned int               version;
          unsigned int               request_id;
          unsigned int               max_open_handles;
          unsigned int               stored_replies;
          unsigned int               file_handle_length;
          unsigned int               dir_handle_length;
          unsigned int               stat_flag;
          unsigned int               pending_write_id[MAX_PENDING_WRITES];
          unsigned int               pending_read_id[MAX_PENDING_READS];
          unsigned int               reads_todo;
          unsigned int               reads_done;
          unsigned int               nl_pos;     /* Name list position. */
          unsigned int               nl_length;  /* Name list length. */
          unsigned int               max_sftp_msg_length;
          int                        pending_write_counter;
          int                        max_pending_writes;
          int                        max_pending_reads;
          int                        current_max_pending_reads;
          int                        pending_id_read_pos;
          int                        pending_id_end_pos;
          int                        reads_queued;
          int                        reads_low_water_mark;
          int                        blocksize;
          off_t                      file_offset;
          off_t                      bytes_to_do;
          char                       *cwd;           /* Current working dir. */
          char                       *file_handle;
          char                       *dir_handle;
          struct name_list           *nl;
          struct stat                stat_buf;
          struct stored_messages     sm[MAX_SFTP_REPLY_BUFFER];
          struct openssh_sftp_limits oss_limits;
          struct supported2          supports;      /* Version 6 */
          char                       debug;
          char                       pipe_broken;
          unsigned char              posix_rename;  /* Used   */
          unsigned char              statvfs;       /* Unused */
          unsigned char              fstatvfs;      /* Unused */
          unsigned char              hardlink;      /* Used   */
          unsigned char              fsync;         /* Unused */
          unsigned char              lsetstat;      /* Unused */
          unsigned char              limits;        /* Used   */
          unsigned char              expand_path;   /* Unused */
          unsigned char              copy_data;     /* Unused */
          unsigned char              unknown;       /* Used   */
       };


/* Function prototypes. */
extern unsigned int sftp_version(void);
extern int          sftp_cd(char *, int, mode_t, char *),
                    sftp_chmod(char *, mode_t),
                    sftp_close_dir(void),
                    sftp_close_file(void),
                    sftp_connect(char *, int, unsigned char, int,
#ifndef FORCE_SFTP_NOOP
                                 int,
#endif
                                 char *,
#ifdef WITH_SSH_FINGERPRINT
                                 char *, char *, char),
#else
                                 char *, char),
#endif
                    sftp_dele(char *),
                    sftp_flush(void),
                    sftp_hardlink(char *, char *, int, mode_t, char *),
                    sftp_max_read_length(void),
                    sftp_max_write_length(void),
                    sftp_mkdir(char *, mode_t),
                    sftp_move(char *, char *, int, mode_t, char *),
                    sftp_multi_read_catch(char *),
                    sftp_multi_read_dispatch(void),
                    sftp_multi_read_eof(void),
                    sftp_multi_read_init(int, off_t),
                    sftp_noop(void),
                    sftp_open_dir(char *),
                    sftp_open_file(int, char *, off_t, mode_t *, int,
                                   mode_t, char *, int, int *),
                    sftp_pwd(void),
                    sftp_read(char *, int),
                    sftp_readdir(char *, struct stat *),
                    sftp_set_blocksize(int *),
                    sftp_set_file_time(char *, time_t, time_t),
                    sftp_stat(char *, struct stat *),
                    sftp_symlink(char *, char *, int, mode_t, char *),
                    sftp_write(char *, int);
extern void         sftp_features(void),
                    sftp_multi_read_discard(int),
                    sftp_quit(void);

#endif /* __sftpdefs_h */
