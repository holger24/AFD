/*
 *  fra_view.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2024 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#include "afddefs.h"

DESCR__S_M1
/*
 ** NAME
 **   fra_view - shows all information in the FRA about a specific
 **              directory
 **
 ** SYNOPSIS
 **   fra_view [--version] [-w <working directory>] position|dir-id|dir-alias
 **
 ** DESCRIPTION
 **   This program shows all information about a specific directory in
 **   the FRA.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   31.03.2000 H.Kiehl Created
 **   20.07.2001 H.Kiehl Show which input files are to be deleted, unknown
 **                      and/or queued.
 **   03.05.2002 H.Kiehl Show number of files, bytes and queues.
 **   22.05.2002 H.Kiehl Separate old file times for unknown and queued files.
 **   17.08.2003 H.Kiehl wait_for_filename, accumulate and accumulate_size.
 **   09.02.2005 H.Kiehl Added additional time entry structure.
 **   07.06.2005 H.Kiehl Added dupcheck entries.
 **   05.10.2005 H.Kiehl Added in_dc_flag entry.
 **   28.05.2012 H.Kiehl Added dir_mode and 'create source dir' support.
 **   26.01.2019 H.Kiehl Added dir_time.
 **   22.07.2019 H.Kiehl Added posibility to specify as search value
 **                      the dir-id.
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr            */
#include <string.h>                      /* strcpy(), strerror()         */
#include <stdlib.h>                      /* atoi(), strtoul()            */
#include <ctype.h>                       /* isdigit()                    */
#include <time.h>                        /* ctime()                      */
#include <sys/types.h>
#include <unistd.h>                      /* STDERR_FILENO                */
#include <errno.h>
#include "version.h"

#define SHOW_STOPPED_DIRS  1
#define SHOW_DISABLED_DIRS 2

/* Global variables. */
int                        sys_log_fd = STDERR_FILENO,   /* Not used!    */
                           fra_id,
                           fra_fd = -1,
                           no_of_dirs = 0;
#ifdef HAVE_MMAP
off_t                      fra_size;
#endif
char                       *p_work_dir;
struct fileretrieve_status *fra;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void                convert2bin(char *, unsigned char),
                           show_time_entry(struct bd_time_entry *),
                           usage(void);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ fra_view() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int          i,
                last = 0,
                mode = 0,
                position = -1;
   unsigned int dir_id = 0;
   char         dir_alias[MAX_DIR_ALIAS_LENGTH + 1],
                *ptr,
                work_dir[MAX_PATH_LENGTH];

   if ((get_arg(&argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      usage();
      exit(SUCCESS);
   }

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if (get_arg(&argc, argv, "-s", NULL, 0) == SUCCESS)
   {
      mode = SHOW_STOPPED_DIRS;
   }
   if (get_arg(&argc, argv, "-d", NULL, 0) == SUCCESS)
   {
      mode |= SHOW_DISABLED_DIRS;
   }

   /* Do not start if binary dataset matches the one stort on disk. */
   if (check_typesize_data(NULL, stdout, NO) > 0)
   {
      (void)fprintf(stderr,
                    "The compiled binary does not match stored database.\n");
      (void)fprintf(stderr,
                    "Initialize database with the command : afd -i\n");
      exit(INCORRECT);
   }

   if (argc == 2)
   {
      i = 0;
      while (isdigit((int)((*(argv + 1))[i])))
      {
         i++;
      }
      if ((*(argv + 1))[i] == '\0')
      {
         position = atoi(argv[1]);
         last = position + 1;
      }
      else
      {
         i = 0;
         while (isxdigit((int)((*(argv + 1))[i])))
         {
            i++;
         }
         if ((*(argv + 1))[i] == '\0')
         {
            dir_id = (unsigned int)strtoul(argv[1], (char **)NULL, 16);
         }
         else
         {
            (void)my_strlcpy(dir_alias, argv[1], MAX_DIR_ALIAS_LENGTH);
         }
      }
   }
   else if (argc == 1)
        {
           position = -2;
        }
        else
        {
           usage();
           exit(INCORRECT);
        }

   if ((i = fra_attach_passive()) != SUCCESS)
   {
      if (i == INCORRECT_VERSION)
      {
         (void)fprintf(stderr,
                       _("ERROR   : This program is not able to attach to the FRA due to incorrect version. (%s %d)\n"),
                       __FILE__, __LINE__);
      }
      else
      {
         if (i < 0)
         {
            (void)fprintf(stderr,
                          _("ERROR   : Failed to attach to FRA. (%s %d)\n"),
                          __FILE__, __LINE__);
         }
         else
         {
            (void)fprintf(stderr,
                          _("ERROR   : Failed to attach to FRA : %s (%s %d)\n"),
                          strerror(i), __FILE__, __LINE__);
         }
      }
      exit(INCORRECT);
   }

   if (position == -1)
   {
      if (dir_id == 0)
      {
         if ((position = get_dir_position(fra, dir_alias, no_of_dirs)) < 0)
         {
            (void)fprintf(stderr,
                          _("WARNING : Could not find directory %s in FRA. (%s %d)\n"),
                          dir_alias, __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }
      else
      {
         if ((position = get_dir_id_position(fra, dir_id, no_of_dirs)) < 0)
         {
            /* As a fallback to old behaviour, try it as alias. */
            (void)strcpy(dir_alias, argv[1]);
            if ((position = get_dir_position(fra, dir_alias, no_of_dirs)) < 0)
            {
               (void)fprintf(stderr,
                             _("WARNING : Could not find directory ID %x in FRA. (%s %d)\n"),
                             dir_id, __FILE__, __LINE__);
               exit(INCORRECT);
            }
         }
      }
      last = position + 1;
   }
   else if (position == -2)
        {
           last = no_of_dirs;
           position = 0;
        }
   else if (position >= no_of_dirs)
        {
           /* Hmm, maybe the user meant a alias-host-id? */
           dir_id = (unsigned int)strtoul(argv[1], (char **)NULL, 16);
           if ((position = get_dir_id_position(fra, dir_id, no_of_dirs)) < 0)
           {
              (void)fprintf(stderr,
                            _("WARNING : There are only %d directories in the FRA. (%s %d)\n"),
                            no_of_dirs, __FILE__, __LINE__);
              exit(INCORRECT);
           }
           last = position + 1;
        }

   ptr =(char *)fra;
   ptr -= AFD_WORD_OFFSET;
   if (mode == 0)
   {
      (void)fprintf(stdout,
                    _("     Number of directories: %d   FRA ID: %d  Struct Version: %d  Pagesize: %d\n\n"),
                    no_of_dirs, fra_id, (int)(*(ptr + SIZEOF_INT + 1 + 1 + 1)),
                    *(int *)(ptr + SIZEOF_INT + 4));
      for (i = position; i < last; i++)
      {
         (void)fprintf(stdout, "=============================> %s (%d) <=============================\n",
                       fra[i].dir_alias, i);
         (void)fprintf(stdout, "Directory alias      : %s\n", fra[i].dir_alias);
         (void)fprintf(stdout, "Directory ID         : %x\n", fra[i].dir_id);
         (void)fprintf(stdout, "URL                  : %s\n", fra[i].url);
#if SIZEOF_TIME_T == 4
         (void)fprintf(stdout, "Dir mtime            : %ld\n", (pri_time_t)fra[i].dir_mtime);
#else
         (void)fprintf(stdout, "Dir mtime            : %lld\n", (pri_time_t)fra[i].dir_mtime);
#endif
         (void)fprintf(stdout, "ls data alias        : %s\n", fra[i].ls_data_alias);
         (void)fprintf(stdout, "Retrieve work dir    : %s\n", fra[i].retrieve_work_dir);
         (void)fprintf(stdout, "Host alias           : %s\n", fra[i].host_alias);
         (void)fprintf(stdout, "Wait for             : %s\n", fra[i].wait_for_filename);
         (void)fprintf(stdout, "FSA position         : %d\n", fra[i].fsa_pos);
         (void)fprintf(stdout, "Priority             : %c\n", fra[i].priority);
         (void)fprintf(stdout, "Number of process    : %d\n", fra[i].no_of_process);
         (void)fprintf(stdout, "Max number of process: %d\n", fra[i].max_process);
#if SIZEOF_OFF_T == 4
         (void)fprintf(stdout, "Bytes received       : %lu\n", fra[i].bytes_received);
#else
         (void)fprintf(stdout, "Bytes received       : %llu\n", fra[i].bytes_received);
#endif
         (void)fprintf(stdout, "Files received       : %u\n", fra[i].files_received);
         (void)fprintf(stdout, "Files in directory   : %u\n", fra[i].files_in_dir);
#if SIZEOF_OFF_T == 4
         (void)fprintf(stdout, "Bytes in directory   : %ld\n", (pri_off_t)fra[i].bytes_in_dir);
#else
         (void)fprintf(stdout, "Bytes in directory   : %lld\n", (pri_off_t)fra[i].bytes_in_dir);
#endif
         (void)fprintf(stdout, "Files in queue(s)    : %u\n", fra[i].files_queued);
#if SIZEOF_OFF_T == 4
         (void)fprintf(stdout, "Bytes in queue(s)    : %ld\n", (pri_off_t)fra[i].bytes_in_queue);
#else
         (void)fprintf(stdout, "Bytes in queue(s)    : %lld\n", (pri_off_t)fra[i].bytes_in_queue);
#endif
#if SIZEOF_OFF_T == 4
         (void)fprintf(stdout, "Accumulate size      : %ld\n", (pri_off_t)fra[i].accumulate_size);
#else
         (void)fprintf(stdout, "Accumulate size      : %lld\n", (pri_off_t)fra[i].accumulate_size);
#endif
         (void)fprintf(stdout, "Accumulate           : %u\n", fra[i].accumulate);
         (void)fprintf(stdout, "gt_lt_sign           : %u\n", fra[i].gt_lt_sign);
         (void)fprintf(stdout, "Create Dir Mode      : %o\n", fra[i].dir_mode);
         (void)fprintf(stdout, "Max errors           : %d\n", fra[i].max_errors);
         (void)fprintf(stdout, "Error counter        : %u\n", fra[i].error_counter);
#if SIZEOF_TIME_T == 4
         (void)fprintf(stdout, "Info time            : %ld\n", (pri_time_t)fra[i].info_time);
#else
         (void)fprintf(stdout, "Info time            : %lld\n", (pri_time_t)fra[i].info_time);
#endif
#if SIZEOF_TIME_T == 4
         (void)fprintf(stdout, "Warn time            : %ld\n", (pri_time_t)fra[i].warn_time);
#else
         (void)fprintf(stdout, "Warn time            : %lld\n", (pri_time_t)fra[i].warn_time);
#endif
         (void)fprintf(stdout, "Keep connected       : %u\n", fra[i].keep_connected);
         if (fra[i].ignore_size == -1)
         {
            (void)fprintf(stdout, "Ignore size          : -1\n");
         }
         else
         {
            if (fra[i].gt_lt_sign & ISIZE_EQUAL)
            {
#if SIZEOF_OFF_T == 4
               (void)fprintf(stdout, "Ignore size          : %ld\n",
#else
               (void)fprintf(stdout, "Ignore size          : %lld\n",
#endif
                             (pri_off_t)fra[i].ignore_size);
            }
            else if (fra[i].gt_lt_sign & ISIZE_LESS_THEN)
                 {
#if SIZEOF_OFF_T == 4
                    (void)fprintf(stdout, "Ignore size          : < %ld\n",
#else
                    (void)fprintf(stdout, "Ignore size          : < %lld\n",
#endif
                                  (pri_off_t)fra[i].ignore_size);
                 }
            else if (fra[i].gt_lt_sign & ISIZE_GREATER_THEN)
                 {
#if SIZEOF_OFF_T == 4
                    (void)fprintf(stdout, "Ignore size          : > %ld\n",
#else
                    (void)fprintf(stdout, "Ignore size          : > %lld\n",
#endif
                                  (pri_off_t)fra[i].ignore_size);
                 }
                 else
                 {
#if SIZEOF_OFF_T == 4
                    (void)fprintf(stdout, "Ignore size          : ? %ld\n",
#else
                    (void)fprintf(stdout, "Ignore size          : ? %lld\n",
#endif
                                  (pri_off_t)fra[i].ignore_size);
                 }
         }
         if (fra[i].ignore_file_time == 0)
         {
            (void)fprintf(stdout, "Ignore file time     : 0\n");
         }
         else
         {
            if (fra[i].gt_lt_sign & IFTIME_EQUAL)
            {
               (void)fprintf(stdout, "Ignore file time     : %u\n",
                             fra[i].ignore_file_time);
            }
            else if (fra[i].gt_lt_sign & IFTIME_LESS_THEN)
                 {
                    (void)fprintf(stdout, "Ignore file time     : < %u\n",
                                  fra[i].ignore_file_time);
                 }
            else if (fra[i].gt_lt_sign & IFTIME_GREATER_THEN)
                 {
                    (void)fprintf(stdout, "Ignore file time     : > %u\n",
                                  fra[i].ignore_file_time);
                 }
                 else
                 {
                    (void)fprintf(stdout, "Ignore file time     : ? %u\n",
                                  fra[i].ignore_file_time);
                 }
         }
         (void)fprintf(stdout, "Max files            : %u\n",
                       fra[i].max_copied_files);
#if SIZEOF_OFF_T == 4
         (void)fprintf(stdout, "Max size             : %ld\n",
#else
         (void)fprintf(stdout, "Max size             : %lld\n",
#endif
                       (pri_off_t)fra[i].max_copied_file_size);
         if (fra[i].dir_status == NORMAL_STATUS)
         {
            (void)fprintf(stdout, "Directory status(%3d): NORMAL STATUS\n",
                          fra[i].dir_status);
         }
         else if (fra[i].dir_status == DIRECTORY_ACTIVE)
              {
                 (void)fprintf(stdout, "Directory status(%3d): DIRECTORY ACTIVE\n",
                               fra[i].dir_status);
              }
         else if (fra[i].dir_status == WARNING_ID)
              {
                 (void)fprintf(stdout, "Directory status(%3d): WARN TIME REACHED\n",
                               fra[i].dir_status);
              }
         else if (fra[i].dir_status == NOT_WORKING2)
              {
                 (void)fprintf(stdout, "Directory status(%3d): NOT WORKING\n",
                               fra[i].dir_status);
              }
         else if (fra[i].dir_status == DISCONNECTED) /* STOPPED */
              {
                 (void)fprintf(stdout, "Directory status(%3d): STOPPED\n",
                               fra[i].dir_status);
              }
         else if (fra[i].dir_status == DISABLED)
              {
                 (void)fprintf(stdout, "Directory status(%3d): DISABLED\n",
                               fra[i].dir_status);
              }
              else
              {
                 (void)fprintf(stdout, "Directory status(%3d): UNKNOWN\n",
                               fra[i].dir_status);
              }
         if (fra[i].dir_flag == 0)
         {
            (void)fprintf(stdout, "Directory flag(    0): None\n");
         }
         else
         {
            (void)fprintf(stdout, "Directory flag(%5d): ", fra[i].dir_flag);
            if (fra[i].dir_flag & MAX_COPIED)
            {
               (void)fprintf(stdout, "MAX_COPIED ");
            }
            if (fra[i].dir_flag & FILES_IN_QUEUE)
            {
               (void)fprintf(stdout, "FILES_IN_QUEUE ");
            }
            if (fra[i].dir_flag & LINK_NO_EXEC)
            {
               (void)fprintf(stdout, "LINK_NO_EXEC ");
            }
            if (fra[i].dir_flag & DIR_DISABLED)
            {
               (void)fprintf(stdout, "DIR_DISABLED ");
            }
            if (fra[i].dir_flag & DIR_DISABLED_STATIC)
            {
               (void)fprintf(stdout, "DIR_DISABLED_STATIC ");
            }
            if (fra[i].dir_flag & DIR_ERROR_SET)
            {
               (void)fprintf(stdout, "DIR_ERROR_SET ");
            }
            if (fra[i].dir_flag & WARN_TIME_REACHED)
            {
               (void)fprintf(stdout, "WARN_TIME_REACHED ");
            }
            if (fra[i].dir_flag & DIR_ERROR_ACKN)
            {
               (void)fprintf(stdout, "DIR_ERROR_ACKN ");
            }
            if (fra[i].dir_flag & DIR_ERROR_OFFLINE)
            {
               (void)fprintf(stdout, "DIR_ERROR_OFFLINE ");
            }
            if (fra[i].dir_flag & DIR_ERROR_ACKN_T)
            {
               (void)fprintf(stdout, "DIR_ERROR_ACKN_T ");
            }
            if (fra[i].dir_flag & DIR_ERROR_OFFL_T)
            {
               (void)fprintf(stdout, "DIR_ERROR_OFFL_T ");
            }
            if (fra[i].dir_flag & DIR_STOPPED)
            {
               (void)fprintf(stdout, "DIR_STOPPED ");
            }
#ifdef WITH_INOTIFY
            if (fra[i].dir_flag & INOTIFY_NEEDS_SCAN)
            {
               (void)fprintf(stdout, "INOTIFY_NEEDS_SCAN ");
            }
#endif
            if (fra[i].dir_flag & ALL_DISABLED)
            {
               (void)fprintf(stdout, "ALL_DISABLED ");
            }
            if (fra[i].dir_flag & INFO_TIME_REACHED)
            {
               (void)fprintf(stdout, "INFO_TIME_REACHED ");
            }
            (void)fprintf(stdout, "\n");
         }
         if (fra[i].dir_options == 0)
         {
            (void)fprintf(stdout, "Dir options   (    0): None\n");
         }
         else
         {
            (void)fprintf(stdout, "Dir options   (%5d): ", fra[i].dir_options);
            if (fra[i].dir_options & ACCEPT_DOT_FILES)
            {
               (void)fprintf(stdout, "ACCEPT_DOT_FILES ");
            }
            if (fra[i].dir_options & DONT_GET_DIR_LIST)
            {
               (void)fprintf(stdout, "DONT_GET_DIR_LIST ");
            }
            if (fra[i].dir_options & URL_CREATES_FILE_NAME)
            {
               (void)fprintf(stdout, "URL_CREATES_FILE_NAME ");
            }
            if (fra[i].dir_options & URL_WITH_INDEX_FILE_NAME)
            {
               (void)fprintf(stdout, "URL_WITH_INDEX_FILE_NAME ");
            }
            if (fra[i].dir_options & NO_DELIMITER)
            {
               (void)fprintf(stdout, "NO_DELIMITER ");
            }
            if (fra[i].dir_options & KEEP_PATH)
            {
               (void)fprintf(stdout, "KEEP_PATH ");
            }
#ifdef WITH_INOTIFY
            if (fra[i].dir_options & INOTIFY_RENAME)
            {
               (void)fprintf(stdout, "INOTIFY_RENAME ");
            }
            if (fra[i].dir_options & INOTIFY_CLOSE)
            {
               (void)fprintf(stdout, "INOTIFY_CLOSE ");
            }
            if (fra[i].dir_options & INOTIFY_CREATE)
            {
               (void)fprintf(stdout, "INOTIFY_CREATE ");
            }
            if (fra[i].dir_options & INOTIFY_DELETE)
            {
               (void)fprintf(stdout, "INOTIFY_DELETE ");
            }
            if (fra[i].dir_options & INOTIFY_ATTRIB)
            {
               (void)fprintf(stdout, "INOTIFY_ATTRIB ");
            }
#endif
            if (fra[i].dir_options & DO_NOT_PARALLELIZE)
            {
               (void)fprintf(stdout, "DO_NOT_PARALLELIZE ");
            }
            if (fra[i].dir_options & DO_NOT_MOVE)
            {
               (void)fprintf(stdout, "DO_NOT_MOVE ");
            }
            if (fra[i].dir_options & ONE_PROCESS_JUST_SCANNING)
            {
               (void)fprintf(stdout, "ONE_PROCESS_JUST_SCANNING ");
            }
            (void)fprintf(stdout, "\n");
         }
         if (fra[i].in_dc_flag == 0)
         {
            (void)fprintf(stdout, "In DIR_CONFIG flag   : None\n");
         }
         else
         {
            (void)fprintf(stdout, "In DIR_CONFIG flag   : ");
            if (fra[i].in_dc_flag & DIR_ALIAS_IDC)
            {
               (void)fprintf(stdout, "DIR_ALIAS ");
            }
            if (fra[i].in_dc_flag & UNKNOWN_FILES_IDC)
            {
               (void)fprintf(stdout, "UNKNOWN_FILES ");
            }
            if (fra[i].in_dc_flag & QUEUED_FILES_IDC)
            {
               (void)fprintf(stdout, "QUEUED_FILES ");
            }
            if (fra[i].in_dc_flag & OLD_LOCKED_FILES_IDC)
            {
               (void)fprintf(stdout, "OLD_LOCKED_FILES ");
            }
            if (fra[i].in_dc_flag & REPUKW_FILES_IDC)
            {
               (void)fprintf(stdout, "REPORT_UNKNOWN_FILES ");
            }
            if (fra[i].in_dc_flag & DONT_REPUKW_FILES_IDC)
            {
               (void)fprintf(stdout, "DONT_REPORT_UNKNOWN_FILES ");
            }
#ifdef WITH_INOTIFY
            if (fra[i].in_dc_flag & INOTIFY_FLAG_IDC)
            {
               (void)fprintf(stdout, "INOTIFY_FLAG ");
            }
#endif
            if (fra[i].in_dc_flag & MAX_CP_FILES_IDC)
            {
               (void)fprintf(stdout, "MAX_COPIED_FILES ");
            }
            if (fra[i].in_dc_flag & MAX_CP_FILE_SIZE_IDC)
            {
               (void)fprintf(stdout, "MAX_COPIED_FILE_SIZE ");
            }
            if (fra[i].in_dc_flag & WARN_TIME_IDC)
            {
               (void)fprintf(stdout, "WARN_TIME ");
            }
            if (fra[i].in_dc_flag & KEEP_CONNECTED_IDC)
            {
               (void)fprintf(stdout, "KEEP_CONNECTED ");
            }
            if (fra[i].in_dc_flag & MAX_PROCESS_IDC)
            {
               (void)fprintf(stdout, "MAX_PROCESS ");
            }
            if (fra[i].in_dc_flag & INFO_TIME_IDC)
            {
               (void)fprintf(stdout, "INFO_TIME ");
            }
            if (fra[i].in_dc_flag & MAX_ERRORS_IDC)
            {
               (void)fprintf(stdout, "MAX_ERRORS ");
            }
            if (fra[i].in_dc_flag & UNREADABLE_FILES_IDC)
            {
               (void)fprintf(stdout, "UNREADABLE_FILES ");
            }
            if (fra[i].in_dc_flag & LOCAL_REMOTE_DIR_IDC)
            {
               (void)fprintf(stdout, "LOCAL_REMOTE_DIR ");
            }
            if (fra[i].in_dc_flag & CREATE_SRC_DIR_IDC)
            {
               (void)fprintf(stdout, "CREATE_SRC_DIR ");
            }
            (void)fprintf(stdout, "\n");
         }
#ifdef WITH_DUP_CHECK
         if (fra[i].dup_check_timeout == 0L)
         {
            (void)fprintf(stdout, "Dupcheck timeout     : Disabled\n");
         }
         else
         {
#if SIZEOF_TIME_T == 4
            (void)fprintf(stdout, "Dupcheck timeout     : %ld\n",
#else
            (void)fprintf(stdout, "Dupcheck timeout     : %lld\n",
#endif
                          (pri_time_t)fra[i].dup_check_timeout);
            (void)fprintf(stdout, "Dupcheck flag        : ");
            if (fra[i].dup_check_flag & DC_FILENAME_ONLY)
            {
               (void)fprintf(stdout, "FILENAME_ONLY ");
            }
            else if (fra[i].dup_check_flag & DC_FILENAME_AND_SIZE)
                 {
                    (void)fprintf(stdout, "NAME_AND_SIZE ");
                 }
            else if (fra[i].dup_check_flag & DC_NAME_NO_SUFFIX)
                 {
                    (void)fprintf(stdout, "NAME_NO_SUFFIX ");
                 }
            else if (fra[i].dup_check_flag & DC_FILE_CONTENT)
                 {
                    (void)fprintf(stdout, "FILE_CONTENT ");
                 }
            else if (fra[i].dup_check_flag & DC_FILE_CONT_NAME)
                 {
                    (void)fprintf(stdout, "FILE_NAME_CONT ");
                 }
                 else
                 {
                    (void)fprintf(stdout, "UNKNOWN_TYPE ");
                 }
            if (fra[i].dup_check_flag & DC_DELETE)
            {
               (void)fprintf(stdout, "DELETE ");
            }
            else if (fra[i].dup_check_flag & DC_STORE)
                 {
                    (void)fprintf(stdout, "STORE ");
                 }
            else if (fra[i].dup_check_flag & DC_WARN)
                 {
                    (void)fprintf(stdout, "WARN ");
                 }
            if (fra[i].dup_check_flag & DC_CRC32)
            {
               (void)fprintf(stdout, "CRC32");
            }
            else if (fra[i].dup_check_flag & DC_CRC32C)
                 {
                    (void)fprintf(stdout, "CRC32C");
                 }
            else if (fra[i].dup_check_flag & DC_MURMUR3)
                 {
                    (void)fprintf(stdout, "MURMUR3");
                 }
                 else
                 {
                    (void)fprintf(stdout, "UNKNOWN_CRC");
                 }
            (void)fprintf(stdout, "\n");
         }
#endif
         if (fra[i].force_reread == YES)
         {
            (void)fprintf(stdout, "Force reread         : YES\n");
         }
         else if (fra[i].force_reread == REMOTE_ONLY)
              {
                 (void)fprintf(stdout, "Force reread         : REMOTE_ONLY\n");
              }
         else if (fra[i].force_reread == LOCAL_ONLY)
              {
                 (void)fprintf(stdout, "Force reread         : LOCAL_ONLY\n");
              }
              else
              {
                 (void)fprintf(stdout, "Force reread         : NO\n");
              }
         (void)fprintf(stdout, "Queued               : %d\n", (int)fra[i].queued);
         if (fra[i].remove == NO)
         {
            (void)fprintf(stdout, "Remove files         : NO\n");
         }
         else
         {
            (void)fprintf(stdout, "Remove files         : YES\n");
         }
         if (fra[i].stupid_mode == NO)
         {
            (void)fprintf(stdout, "Stupid mode          : NO\n");
         }
         else if (fra[i].stupid_mode == GET_ONCE_ONLY)
              {
                 (void)fprintf(stdout, "Stupid mode          : GET_ONCE_ONLY\n");
              }
         else if (fra[i].stupid_mode == GET_ONCE_NOT_EXACT)
              {
                 (void)fprintf(stdout, "Stupid mode          : GET_ONCE_NOT_EXACT\n");
              }
         else if (fra[i].stupid_mode == APPEND_ONLY)
              {
                 (void)fprintf(stdout, "Stupid mode          : APPEND_ONLY\n");
              }
              else
              {
                 (void)fprintf(stdout, "Stupid mode          : YES\n");
              }
         (void)fprintf(stdout, "Protocol (%4d)      : ", fra[i].protocol);
         if (fra[i].protocol == FTP)
         {
            (void)fprintf(stdout, "FTP\n");
         }
         else if (fra[i].protocol == LOC)
              {
                 (void)fprintf(stdout, "LOC\n");
              }
         else if (fra[i].protocol == SFTP)
              {
                 (void)fprintf(stdout, "SFTP\n");
              }
         else if (fra[i].protocol == HTTP)
              {
                 (void)fprintf(stdout, "HTTP\n");
              }
         else if (fra[i].protocol == SMTP)
              {
                 (void)fprintf(stdout, "SMTP\n");
              }
         else if (fra[i].protocol == EXEC)
              {
                 (void)fprintf(stdout, "EXEC\n");
              }
#ifdef _WITH_WMO_SUPPORT
         else if (fra[i].protocol == WMO)
              {
                 (void)fprintf(stdout, "WMO\n");
              }
#endif
              else
              {
                 (void)fprintf(stdout, "Unknown\n");
              }
         if (fra[i].delete_files_flag == 0)
         {
            (void)fprintf(stdout, "Delete input files   : NO\n");
         }
         else
         {
            (void)fprintf(stdout, "Delete input files   : ");
            if (fra[i].delete_files_flag & UNKNOWN_FILES)
            {
               (void)fprintf(stdout, "UNKNOWN ");
            }
            if (fra[i].delete_files_flag & UNREADABLE_FILES)
            {
               (void)fprintf(stdout, "UNREADABLE_FILES ");
            }
            if (fra[i].delete_files_flag & QUEUED_FILES)
            {
               (void)fprintf(stdout, "QUEUED ");
            }
            if (fra[i].delete_files_flag & OLD_RLOCKED_FILES)
            {
               (void)fprintf(stdout, "RLOCKED\n");
            }
            if (fra[i].delete_files_flag & OLD_LOCKED_FILES)
            {
               (void)fprintf(stdout, "LOCKED\n");
            }
            else
            {
               (void)fprintf(stdout, "\n");
            }
            if (fra[i].delete_files_flag & UNKNOWN_FILES)
            {
               if (fra[i].unknown_file_time == -2)
               {
                  (void)fprintf(stdout, "Unknown file time (h): Immediately\n");
               }
               else
               {
                  (void)fprintf(stdout, "Unknown file time (h): %d\n",
                                fra[i].unknown_file_time / 3600);
               }
            }
            if (fra[i].delete_files_flag & UNREADABLE_FILES)
            {
               (void)fprintf(stdout, "Unreadable file time : %d (h)\n",
                             fra[i].unreadable_file_time / 3600);
            }
            if (fra[i].delete_files_flag & QUEUED_FILES)
            {
               (void)fprintf(stdout, "Queued file time (h) : %d\n",
                             fra[i].queued_file_time / 3600);
            }
            if ((fra[i].delete_files_flag & OLD_LOCKED_FILES) ||
                (fra[i].delete_files_flag & OLD_RLOCKED_FILES))
            {
               (void)fprintf(stdout, "Old lck file time (h): %d\n",
                             fra[i].locked_file_time / 3600);
            }
         }
         if (fra[i].report_unknown_files == NO)
         {
            (void)fprintf(stdout, "Report unknown files : NO\n");
         }
         else
         {
            (void)fprintf(stdout, "Report unknown files : YES\n");
         }
         if (fra[i].important_dir == NO)
         {
            (void)fprintf(stdout, "Important directory  : NO\n");
         }
         else
         {
            (void)fprintf(stdout, "Important directory  : YES\n");
         }
         if (fra[i].end_character == -1)
         {
            (void)fprintf(stdout, "End character        : NONE\n");
         }
         else
         {
            (void)fprintf(stdout, "End character        : %d\n",
                          fra[i].end_character);
         }
         if (fra[i].no_of_time_entries == 0)
         {
            (void)fprintf(stdout, "Time option          : NO\n");
         }
         else
         {
            int j;

            if (fra[i].timezone[0] == '\0')
            {
               (void)fprintf(stdout, "Timezone             : Not set, taking system default\n");
            }
            else
            {
               (void)fprintf(stdout, "Timezone             : %s\n",
                             fra[i].timezone);
            }
            (void)fprintf(stdout, "Time option          : %d\n",
                          (int)fra[i].no_of_time_entries);

#if SIZEOF_TIME_T == 4
            if (fra[i].next_check_time == LONG_MAX)
#else
# ifdef LLONG_MAX
            if (fra[i].next_check_time == LLONG_MAX)
# else
            if (fra[i].next_check_time == LONG_MAX)
# endif
#endif
            {
               (void)fprintf(stdout, "Next check time      : <external>\n");
            }
            else
            {
               (void)fprintf(stdout, "Next check time      : %s",
                             ctime(&fra[i].next_check_time));
               for (j = 0; j < fra[i].no_of_time_entries; j++)
               {
                  show_time_entry(&fra[i].te[j]);
               }
            }
         }
         show_time_entry(&fra[i].ate);
         (void)fprintf(stdout, "Last retrieval       : %s",
                       ctime(&fra[i].last_retrieval));
      }
   }
   else
   {
      for (i = position; i < last; i++)
      {
         if (mode == SHOW_DISABLED_DIRS)
         {
            (void)fprintf(stdout, "%s|%s\n",
                          fra[i].dir_alias,
                          (fra[i].dir_status == DISABLED) ? "Disabled" : "Enabled");
         }
         else if (mode == SHOW_STOPPED_DIRS)
              {
                 (void)fprintf(stdout, "%s|%s\n",
                               fra[i].dir_alias,
                               (fra[i].dir_status == DISCONNECTED) ? "Stopped" : "Enabled");
              }
              else
              {
                 (void)fprintf(stdout, "%s|%s|%s\n",
                               fra[i].dir_alias,
                               (fra[i].dir_status == DISCONNECTED) ? "Stopped" : "Enabled",
                               (fra[i].dir_status == DISABLED) ? "Disabled" : "Enabled");
              }
      }
   }

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++ show_time_entry() ++++++++++++++++++++++++++*/
static void
show_time_entry(struct bd_time_entry *te)
{
   int  byte_order = 1;
   char binstr[72],
        *ptr;

#ifdef HAVE_LONG_LONG
   ptr = (char *)&te->minute;
   if (*(char *)&byte_order == 1)
   {
      ptr += 7;
   }
   convert2bin(binstr, *ptr);
   binstr[8] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[9], *ptr);
   binstr[17] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[18], *ptr);
   binstr[26] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[27], *ptr);
   binstr[35] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[36], *ptr);
   binstr[44] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[45], *ptr);
   binstr[53] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[54], *ptr);
   binstr[62] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[63], *ptr);
   binstr[71] = '\0';
   (void)fprintf(stdout, "Minute (long long)   : %s\n", binstr);
   (void)fprintf(stdout, "Continues (long long): %s\n", binstr);
#else
   ptr = (char *)&te->minute[7];
   convert2bin(binstr, *ptr);
   binstr[8] = ' ';
   ptr--;
   convert2bin(&binstr[9], *ptr);
   binstr[17] = ' ';
   ptr--;
   convert2bin(&binstr[18], *ptr);
   binstr[26] = ' ';
   ptr--;
   convert2bin(&binstr[27], *ptr);
   binstr[35] = ' ';
   ptr--;
   convert2bin(&binstr[36], *ptr);
   binstr[44] = ' ';
   ptr--;
   convert2bin(&binstr[45], *ptr);
   binstr[53] = ' ';
   ptr--;
   convert2bin(&binstr[54], *ptr);
   binstr[62] = ' ';
   ptr--;
   convert2bin(&binstr[63], *ptr);
   binstr[71] = '\0';
   (void)fprintf(stdout, "Minute (uchar[8])    : %s\n", binstr);
   (void)fprintf(stdout, "Continues (uchar[8]) : %s\n", binstr);
#endif /* HAVE_LONG_LONG */
   ptr = (char *)&te->hour;
   if (*(char *)&byte_order == 1)
   {
      ptr += 3;
   }
   convert2bin(binstr, *ptr);
   binstr[8] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[9], *ptr);
   binstr[17] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[18], *ptr);
   binstr[26] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[27], *ptr);
   binstr[35] = '\0';
   (void)fprintf(stdout, "Hour (uint)          : %s\n", binstr);
   ptr = (char *)&te->day_of_month;
   if (*(char *)&byte_order == 1)
   {
      ptr += 3;
   }
   convert2bin(binstr, *ptr);
   binstr[8] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[9], *ptr);
   binstr[17] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[18], *ptr);
   binstr[26] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[27], *ptr);
   binstr[35] = '\0';
   (void)fprintf(stdout, "Day of month (uint)  : %s\n", binstr);
   ptr = (char *)&te->month;
   if (*(char *)&byte_order == 1)
   {
      ptr++;
   }
   convert2bin(binstr, *ptr);
   binstr[8] = ' ';
   if (*(char *)&byte_order == 1)
   {
      ptr--;
   }
   else
   {
      ptr++;
   }
   convert2bin(&binstr[9], *ptr);
   binstr[17] = '\0';
   (void)fprintf(stdout, "Month (short)        : %s\n", binstr);
   convert2bin(binstr, te->day_of_week);
   binstr[8] = '\0';
   (void)fprintf(stdout, "Day of week (uchar)  : %s\n", binstr);

   return;
}


/*--------------------------- convert2bin() ----------------------------*/ 
static void
convert2bin(char *buf, unsigned char val)
{
   register int i;

   for (i = 0; i < 8; i++)
   {
      if (val & 1)
      {
         buf[7 - i] = '1';
      }
      else
      {
         buf[7 - i] = '0';
      }
      val = val >> 1;
   }

   return;
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/ 
static void
usage(void)
{
   (void)fprintf(stderr,
                 _("SYNTAX  : fra_view [--version] [-w working directory] position|dir-id|dir-alias\n"));
   return;
}
