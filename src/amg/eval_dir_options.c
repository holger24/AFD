/*
 *  eval_dir_options.c - Part of AFD, an automatic file distribution
 *                       program.
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

DESCR__S_M3
/*
 ** NAME
 **   eval_dir_options - evaluates the directory options
 **
 ** SYNOPSIS
 **   int eval_dir_options(int  dir_pos, char *dir_options, FILE *cmd_fp)
 **
 ** DESCRIPTION
 **   Reads and evaluates the directory options from one directory
 **   of the DIR_CONFIG file. It currently knows the following options:
 **
 **        delete unknown files [<value in hours>]
 **        delete queued files [<value in hours>]
 **        delete old locked files <value in hours>
 **        do not delete unknown files           [DEFAULT]
 **        report unknown files                  [DEFAULT]
 **        do not report unknown files
 **        old file time <value in hours>        [DEFAULT 24]
 **        end character <decimal number>
 **        ignore size [=|>|<] <decimal number>
 **        ignore file time [=|>|<] <decimal number>
 **        important dir
 **        time * * * * *
 **        timezone <zone name>
 **        keep connected <value in seconds>
 **        create source dir[ <mode>]
 **        do not create source dir
 **        do not get dir list
 **        do not remove
 **        url creates file name
 **        url with index file name
 **        store retrieve list [once]
 **        priority <value>                      [DEFAULT 9]
 **        force reread [local|remote]
 **        ls data filename <file name>
 **        max process <value>                   [DEFAULT 10]
 **        max files <value>                     [DEFAULT ?]
 **        max size <value>                      [DEFAULT ?]
 **        max errors <value>                    [DEFAULT 10]
 **        wait for <file name|pattern>
 **        warn time <value in seconds>
 **        accumulate <value>
 **        accumulate size <value>
 **        do not parallelize
 **        dupcheck[ <timeout in secs>[ <check type>[ <action>[ <CRC type>]]]]
 **        accept dot files
 **        inotify <value>                       [DEFAULT 0]
 **        one process just scaning
 **
 ** RETURN VALUES
 **   0 when no problems are found when evaluating the directory options.
 **   Otherwise, the number of problems found will be returned. Regardless
 **   of problems, it tries to fill up the structure dir_data with values
 **   that are ok.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.03.2000 H.Kiehl Created
 **   12.08.2000 H.Kiehl Addition of priority.
 **   31.08.2000 H.Kiehl Addition of option "force rereads".
 **   20.07.2001 H.Kiehl New option "delete queued files".
 **   22.05.2002 H.Kiehl Separate old file times for unknown and queued files.
 **   14.08.2002 H.Kiehl Added "ignore size" option.
 **   06.02.2003 H.Kiehl Added "max files" and "max size" option.
 **   16.08.2003 H.Kiehl Added "wait for", accumulate and "accumulate size"
 **                      options.
 **   02.09.2004 H.Kiehl Added "ignore file time" option.
 **   28.11.2004 H.Kiehl Added "delete old locked files" option.
 **   07.06.2005 H.Kiehl Added "dupcheck" option.
 **   30.06.2006 H.Kiehl Added "accept dot files" option.
 **   19.08.2006 H.Kiehl Added "do not get dir list" option.
 **   10.11.2006 H.Kiehl Added "warn time" option.
 **   13.11.2006 H.Kiehl Added "keep connected" option.
 **   24.02.2007 H.Kiehl Added inotify support.
 **   28.05.2012 H.Kiehl Added "create source dir" option.
 **   14.02.2013 H.Kiehl Remove support for old style directory options,
 **                      directly behind the directory name itself.
 **   15.12.2013 H.Kiehl Added "max errors" and "do not parallelize" option.
 **   11.04.2016 H.Kiehl Added timezone option.
 **   22.04.2016 H.Kiehl Added "local remote dir" option.
 **   23.06.2016 H.Kiehl Function now returns SUCCESS if no problems where
 **                      encountered during evaluation.
 **   18.03.2017 H.Kiehl Added "ls data filename" to allow user to set
 **                      this name.
 **   11.05.2017 H.Kiehl Added parameter cmd_fp, so we can show errors
 **                      and warnings directly when udc is called.
 **   29.10.2022 H.Kiehl Added "url with index file name" option.
 **   13.01.2024 H.Kiehl Extended 'store retrieve list' with 'once not exact'.
 **
 */
DESCR__E_M3

#include <stdio.h>                /* stderr, fprintf()                   */
#include <stdlib.h>               /* atoi(), malloc(), free(), strtoul() */
#include <string.h>               /* strcmp(), strncmp(), strerror()     */
#include <ctype.h>                /* isdigit()                           */
#include <unistd.h>               /* read(), close(), setuid()           */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>               /* O_RDONLY, etc                       */
#endif
#include <errno.h>
#include "amgdefs.h"
#include "version.h"

/* External global variables. */
extern char            *p_work_dir;
extern int             default_delete_files_flag,
#ifdef WITH_INOTIFY
                       default_inotify_flag,
#endif
                       default_old_file_time,
                       max_process_per_dir;
extern unsigned int    max_copied_files;
extern time_t          default_info_time;
extern time_t          default_warn_time;
extern off_t           max_copied_file_size;
extern struct dir_data *dd;

#define DEL_UNKNOWN_FILES_FLAG           1
#define OLD_FILE_TIME_FLAG               2
#define DONT_REP_UNKNOWN_FILES_FLAG      4
#define DIRECTORY_PRIORITY_FLAG          8
#define END_CHARACTER_FLAG               16
#define MAX_PROCESS_FLAG                 32
#define DO_NOT_REMOVE_FLAG               64
#define STORE_RETRIEVE_LIST_FLAG         128
#define DEL_QUEUED_FILES_FLAG            256
#define DONT_DEL_UNKNOWN_FILES_FLAG      512
#define REP_UNKNOWN_FILES_FLAG           1024
#define FORCE_REREAD_FLAG                2048
#define IMPORTANT_DIR_FLAG               4096
#define IGNORE_SIZE_FLAG                 8192
#define MAX_FILES_FLAG                   16384
#define MAX_SIZE_FLAG                    32768
#define WAIT_FOR_FILENAME_FLAG           65536
#define ACCUMULATE_FLAG                  131072
#define ACCUMULATE_SIZE_FLAG             262144
#define IGNORE_FILE_TIME_FLAG            524288
#define DEL_OLD_LOCKED_FILES_FLAG        1048576
#ifdef WITH_DUP_CHECK
# define DUPCHECK_FLAG                   2097152
#endif
#define ACCEPT_DOT_FILES_FLAG            4194304
#define DO_NOT_GET_DIR_LIST_FLAG         8388608
#define DIR_WARN_TIME_FLAG               16777216
#define KEEP_CONNECTED_FLAG              33554432
#ifdef WITH_INOTIFY
# define INOTIFY_FLAG                    67108864
#endif
#define CREATE_SOURCE_DIR_FLAG           134217728
#define DONT_CREATE_SOURCE_DIR_FLAG      268435456
#define DIR_INFO_TIME_FLAG               536870912
#define MAX_ERRORS_FLAG                  1073741824
#define DO_NOT_PARALLELIZE_FLAG          2147483648
#define DO_NOT_MOVE_FLAG                 1
#define DEL_UNREADABLE_FILES_FLAG        2
#define TIMEZONE_FLAG                    4
#define LS_DATA_FILENAME_FLAG            8
#define LOCAL_REMOTE_DIR_FLAG            16
#define ONE_PROCESS_JUST_SCANNING_FLAG   32
#define URL_CREATES_FILE_NAME_FLAG       64
#define NO_DELIMITER_FLAG                128
#define KEEP_PATH_FLAG                   256
#define URL_WITH_INDEX_FILE_NAME_FLAG    512


/*########################## eval_dir_options() #########################*/
int
eval_dir_options(int dir_pos, char type, char *dir_options, FILE *cmd_fp)
{
   int          old_file_time,
                problems_found = 0,
                to_many_time_option_warn = YES;
   unsigned int used = 0,  /* Used to see whether option has */
                           /* already been set.              */
                used2 = 0;
   char         *ptr,
                *end_ptr = NULL,
                byte_buf;

   /* Set some default directory options. */
   if (default_old_file_time == -1)
   {
      old_file_time = DEFAULT_OLD_FILE_TIME * 3600;
   }
   else
   {
      old_file_time = default_old_file_time * 3600;
   }
   dd[dir_pos].delete_files_flag = default_delete_files_flag;
   if ((type == REMOTE_DIR) &&
       (dd[dir_pos].delete_files_flag & OLD_LOCKED_FILES))
   {
      dd[dir_pos].delete_files_flag &= ~OLD_LOCKED_FILES;
   }
   dd[dir_pos].unknown_file_time = -1;
   dd[dir_pos].queued_file_time = -1;
   dd[dir_pos].locked_file_time = -1;
   dd[dir_pos].unreadable_file_time = -1;
   dd[dir_pos].report_unknown_files = YES;
   dd[dir_pos].end_character = -1;
#ifndef _WITH_PTHREAD
   dd[dir_pos].important_dir = NO;
#endif
   dd[dir_pos].no_of_time_entries = 0;
   dd[dir_pos].max_process = max_process_per_dir;
   dd[dir_pos].remove = YES;
   dd[dir_pos].stupid_mode = YES;
   dd[dir_pos].priority = DEFAULT_PRIORITY;
   dd[dir_pos].force_reread = NO;
   dd[dir_pos].gt_lt_sign = 0;
   dd[dir_pos].ignore_size = -1;
   dd[dir_pos].ignore_file_time = 0;
   dd[dir_pos].max_copied_files = max_copied_files;
   dd[dir_pos].max_copied_file_size = max_copied_file_size;
   dd[dir_pos].wait_for_filename[0] = '\0';
   dd[dir_pos].accumulate = 0;
   dd[dir_pos].accumulate_size = 0;
#ifdef WITH_DUP_CHECK
   dd[dir_pos].dup_check_flag = 0;
   dd[dir_pos].dup_check_timeout = 0L;
#endif
   dd[dir_pos].accept_dot_files = NO;
   dd[dir_pos].do_not_get_dir_list = NO;
   dd[dir_pos].url_creates_file_name = NO;
   dd[dir_pos].url_with_index_file_name = NO;
   dd[dir_pos].create_source_dir = NO;
   dd[dir_pos].max_errors = 10;
   dd[dir_pos].info_time = default_info_time;
   dd[dir_pos].timezone[0] = '\0';
   dd[dir_pos].ls_data_alias[0] = '\0';
   dd[dir_pos].warn_time = default_warn_time;
   dd[dir_pos].keep_connected = DEFAULT_KEEP_CONNECTED_TIME;
#ifdef WITH_INOTIFY
   dd[dir_pos].inotify_flag = default_inotify_flag;
#endif
   dd[dir_pos].dont_create_source_dir = NO;
   dd[dir_pos].create_source_dir = NO;
   dd[dir_pos].dir_mode = 0;
   dd[dir_pos].do_not_parallelize = NO;
   dd[dir_pos].do_not_move = NO;
   dd[dir_pos].retrieve_work_dir[0] = '\0';
   dd[dir_pos].one_process_just_scaning = NO;

   /*
    * Now for the new directory options.
    */
   ptr = dir_options;
   while (*ptr != '\0')
   {
      if (((used & DEL_UNKNOWN_FILES_FLAG) == 0) &&
          (strncmp(ptr, DEL_UNKNOWN_FILES_ID, DEL_UNKNOWN_FILES_ID_LENGTH) == 0))
      {
         used |= DEL_UNKNOWN_FILES_FLAG;
         ptr += DEL_UNKNOWN_FILES_ID_LENGTH;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            int  length = 0;
            char number[MAX_INT_LENGTH + 1];

            while ((*ptr == ' ') || (*ptr == '\t'))
            {
               ptr++;
            }
            if ((*ptr == '-') && (*(ptr + 1) == '1'))
            {
               dd[dir_pos].unknown_file_time = -2;
               ptr += 2;
               while ((*ptr == ' ') || (*ptr == '\t'))
               {
                  ptr++;
               }
            }
            else
            {
               while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
               {
                  number[length] = *ptr;
                  ptr++; length++;
               }
               if ((length > 0) && (length != MAX_INT_LENGTH))
               {
                  if ((length == 1) && (number[0] == '0'))
                  {
                     dd[dir_pos].unknown_file_time = 0;
                  }
                  else
                  {
                     number[length] = '\0';
                     dd[dir_pos].unknown_file_time = atoi(number) * 3600;
                  }
                  while ((*ptr == ' ') || (*ptr == '\t'))
                  {
                     ptr++;
                  }
               }
            }
         }
         while ((*ptr != '\n') && (*ptr != '\0'))
         {
            ptr++;
         }
         if ((dd[dir_pos].delete_files_flag & UNKNOWN_FILES) == 0)
         {
            dd[dir_pos].delete_files_flag |= UNKNOWN_FILES;
         }
         dd[dir_pos].in_dc_flag |= UNKNOWN_FILES_IDC;
      }
#ifdef WITH_INOTIFY
      else if (((used & INOTIFY_FLAG) == 0) &&
               (strncmp(ptr, INOTIFY_FLAG_ID, INOTIFY_FLAG_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_INT_LENGTH + 1];

              used |= INOTIFY_FLAG;
              ptr += INOTIFY_FLAG_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_INT_LENGTH))
              {
                 number[length] = '\0';
                 dd[dir_pos].inotify_flag = (unsigned int)atoi(number);
                 if (dd[dir_pos].inotify_flag > (INOTIFY_RENAME_FLAG | INOTIFY_CLOSE_FLAG | INOTIFY_CREATE_FLAG | INOTIFY_DELETE_FLAG | INOTIFY_ATTRIB))
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "Incorrect parameter %u for directory option `%s' for directory `%s'. Resetting to %u.",
                                  dd[dir_pos].inotify_flag, INOTIFY_FLAG_ID,
                                  dd[dir_pos].dir_name, default_inotify_flag);
                    dd[dir_pos].inotify_flag = default_inotify_flag;
                    problems_found++;
                 }
                 else
                 {
                    dd[dir_pos].in_dc_flag |= INOTIFY_FLAG_IDC;
                 }
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }
              else
              {
                 if (length > 0)
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "Value to long for directory option `%s' for directory `%s'.",
                                  INOTIFY_FLAG_ID, dd[dir_pos].dir_name);
                    update_db_log(WARN_SIGN, NULL, 0, cmd_fp, NULL,
                                  "May only be %d bytes long.",
                                  MAX_INT_LENGTH);
                 }
                 else
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "No value found or syntax wrong for directory option `%s' for directory `%s'.",
                                  INOTIFY_FLAG_ID, dd[dir_pos].dir_name);
                 }
                 problems_found++;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
#endif
      else if (((used & OLD_FILE_TIME_FLAG) == 0) &&
               (strncmp(ptr, OLD_FILE_TIME_ID, OLD_FILE_TIME_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_INT_LENGTH + 1];

              used |= OLD_FILE_TIME_FLAG;
              ptr += OLD_FILE_TIME_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_INT_LENGTH))
              {
                 number[length] = '\0';
                 old_file_time = atoi(number) * 3600;
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }
              else
              {
                 if (length > 0)
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "Value to long for directory option `%s' for directory `%s'.",
                                  OLD_FILE_TIME_ID, dd[dir_pos].dir_name);
                    update_db_log(WARN_SIGN, NULL, 0, cmd_fp, NULL,
                                  "May only be %d bytes long.",
                                  MAX_INT_LENGTH);
                 }
                 else
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "No value found or syntax wrong for directory option `%s' for directory `%s'.",
                                  OLD_FILE_TIME_ID, dd[dir_pos].dir_name);
                 }
                 problems_found++;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & DIRECTORY_PRIORITY_FLAG) == 0) &&
               (strncmp(ptr, PRIORITY_ID, PRIORITY_ID_LENGTH) == 0))
           {
              used |= DIRECTORY_PRIORITY_FLAG;
              ptr += PRIORITY_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\n'))
              {
                 ptr++;
              }
              if (isdigit((int)(*ptr)))
              {
                 dd[dir_pos].priority = *ptr;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & DONT_REP_UNKNOWN_FILES_FLAG) == 0) &&
               (strncmp(ptr, DONT_REP_UNKNOWN_FILES_ID, DONT_REP_UNKNOWN_FILES_ID_LENGTH) == 0))
           {
              used |= DONT_REP_UNKNOWN_FILES_FLAG;
              ptr += DONT_REP_UNKNOWN_FILES_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].report_unknown_files = NO;
              dd[dir_pos].in_dc_flag |= DONT_REPUKW_FILES_IDC;
           }
      else if (((used & END_CHARACTER_FLAG) == 0) &&
               (strncmp(ptr, END_CHARACTER_ID, END_CHARACTER_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_INT_LENGTH + 1];

              used |= END_CHARACTER_FLAG;
              ptr += END_CHARACTER_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_INT_LENGTH))
              {
                 number[length] = '\0';
                 dd[dir_pos].end_character = atoi(number);
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }
              else
              {
                 if (length > 0)
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "Value to long for directory option `%s' for directory `%s'.",
                                  END_CHARACTER_ID, dd[dir_pos].dir_name);
                    update_db_log(WARN_SIGN, NULL, 0, cmd_fp, NULL,
                                  "May only be %d bytes long.",
                                  MAX_INT_LENGTH);
                 }
                 else
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "No value found or syntax wrong for directory option `%s' for directory `%s'.",
                                  END_CHARACTER_ID, dd[dir_pos].dir_name);
                 }
                 problems_found++;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & MAX_PROCESS_FLAG) == 0) &&
               (strncmp(ptr, MAX_PROCESS_ID, MAX_PROCESS_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_INT_LENGTH + 1];

              used |= MAX_PROCESS_FLAG;
              ptr += MAX_PROCESS_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_INT_LENGTH))
              {
                 number[length] = '\0';
                 dd[dir_pos].max_process = atoi(number);
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 dd[dir_pos].in_dc_flag |= MAX_PROCESS_IDC;
              }
              else
              {
                 if (length > 0)
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "Value to long for directory option `%s' for directory `%s'.",
                                  MAX_PROCESS_ID, dd[dir_pos].dir_name);
                    update_db_log(WARN_SIGN, NULL, 0, cmd_fp, NULL,
                                  "May only be %d bytes long.",
                                  MAX_INT_LENGTH);
                 }
                 else
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "No value found or syntax wrong for directory option `%s' for directory `%s'.",
                                  MAX_PROCESS_ID, dd[dir_pos].dir_name);
                 }
                 problems_found++;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & MAX_ERRORS_FLAG) == 0) &&
               (strncmp(ptr, MAX_ERRORS_ID, MAX_ERRORS_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_INT_LENGTH + 1];

              used |= MAX_ERRORS_FLAG;
              ptr += MAX_ERRORS_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_INT_LENGTH))
              {
                 number[length] = '\0';
                 dd[dir_pos].max_errors = atoi(number);
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 dd[dir_pos].in_dc_flag |= MAX_ERRORS_IDC;
              }
              else
              {
                 if (length > 0)
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "Value to long for directory option `%s' for directory `%s'.",
                                  MAX_ERRORS_ID, dd[dir_pos].dir_name);
                    update_db_log(WARN_SIGN, NULL, 0, cmd_fp, NULL,
                                  "May only be %d bytes long.",
                                  MAX_INT_LENGTH);
                 }
                 else
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "No value found or syntax wrong for directory option `%s' for directory `%s'.",
                                  MAX_ERRORS_ID, dd[dir_pos].dir_name);
                 }
                 problems_found++;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if ((strncmp(ptr, TIME_ID, TIME_ID_LENGTH) == 0) &&
               (*(ptr + TIME_ID_LENGTH) != '\0') &&
               ((*(ptr + TIME_ID_LENGTH) == ' ') ||
                (*(ptr + TIME_ID_LENGTH) == '\t')))
           {
              if (dd[dir_pos].no_of_time_entries < MAX_FRA_TIME_ENTRIES)
              {
                 char tmp_char;

                 ptr += TIME_ID_LENGTH;
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 end_ptr = ptr;
                 while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                 {
                    end_ptr++;
                 }
                 tmp_char = *end_ptr;
                 *end_ptr = '\0';
                 if (eval_time_str(ptr,
                                   &dd[dir_pos].te[dd[dir_pos].no_of_time_entries],
                                   cmd_fp) == SUCCESS)
                 {
                    dd[dir_pos].no_of_time_entries++;
                 }
                 else
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "Invalid %s string <%s>, for directory `%s'.",
                                  TIME_ID, ptr, dd[dir_pos].dir_name);
                    problems_found++;
                 }
                 *end_ptr = tmp_char;
                 ptr = end_ptr;
                 while ((*ptr != '\n') && (*ptr != '\0'))
                 {
                    ptr++;
                 }
              }
              else
              {
                 /* Ignore this option. */
                 end_ptr = ptr + TIME_ID_LENGTH;
                 while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                 {
                    end_ptr++;
                 }
                 if (to_many_time_option_warn == YES)
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "Only %d %s options may be set in DIR_CONFIG file for directory `%s'. Ignoring option.",
                                  MAX_FRA_TIME_ENTRIES, TIME_ID,
                                  dd[dir_pos].dir_name);
                    to_many_time_option_warn = NO;
                    problems_found++;
                 }
                 ptr = end_ptr;
              }
           }
      else if (((used & DO_NOT_REMOVE_FLAG) == 0) &&
               (strncmp(ptr, DO_NOT_REMOVE_ID, DO_NOT_REMOVE_ID_LENGTH) == 0))
           {
              used |= DO_NOT_REMOVE_FLAG;
              ptr += DO_NOT_REMOVE_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].remove = NO;
           }
      else if (((used & STORE_RETRIEVE_LIST_FLAG) == 0) &&
               (strncmp(ptr, STORE_RETRIEVE_LIST_ID, STORE_RETRIEVE_LIST_ID_LENGTH) == 0))
           {
              used |= STORE_RETRIEVE_LIST_FLAG;
              ptr += STORE_RETRIEVE_LIST_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              /* once */
              if ((*ptr == 'o') && (*(ptr + 1) == 'n') &&
                  (*(ptr + 2) == 'c') && (*(ptr + 3) == 'e') &&
                  ((*(ptr + 4) == '\n') || (*(ptr + 4) == '\0')))
              {
                 dd[dir_pos].stupid_mode = GET_ONCE_ONLY;
                 ptr += 4;
              }
                   /* once not exact */
              else if ((*ptr == 'o') && (*(ptr + 1) == 'n') &&
                       (*(ptr + 2) == 'c') && (*(ptr + 3) == 'e') &&
                       ((*(ptr + 4) == ' ') || (*(ptr + 4) == '\t')) &&
                       (*(ptr + 5) == 'n') && (*(ptr + 6) == 'o') &&
                       (*(ptr + 7) == 't') &&
                       ((*(ptr + 8) == ' ') || (*(ptr + 8) == '\t')) &&
                       (*(ptr + 9) == 'e') && (*(ptr + 10) == 'x') &&
                       (*(ptr + 11) == 'a') && (*(ptr + 12) == 'c') &&
                       (*(ptr + 13) == 't') &&
                       ((*(ptr + 14) == '\n') || (*(ptr + 14) == '\0')))
                   {
                      dd[dir_pos].stupid_mode = GET_ONCE_NOT_EXACT;
                      ptr += 4;
                   }
                   /* append */
              else if ((*ptr == 'a') && (*(ptr + 1) == 'p') &&
                       (*(ptr + 2) == 'p') && (*(ptr + 3) == 'e') &&
                       (*(ptr + 4) == 'n') && (*(ptr + 5) == 'd') &&
                       ((*(ptr + 6) == '\n') || (*(ptr + 6) == '\0')))
                   {
                      dd[dir_pos].stupid_mode = APPEND_ONLY;
                      ptr += 6;
                   }
                   else
                   {
                      dd[dir_pos].stupid_mode = NO;
                   }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & STORE_RETRIEVE_LIST_FLAG) == 0) &&
               (strncmp(ptr, STORE_REMOTE_LIST, STORE_REMOTE_LIST_LENGTH) == 0))
           {
              used |= STORE_RETRIEVE_LIST_FLAG;
              ptr += STORE_REMOTE_LIST_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              if ((*ptr == 'o') && (*(ptr + 1) == 'n') &&
                  (*(ptr + 2) == 'c') && (*(ptr + 3) == 'e') &&
                  ((*(ptr + 4) == '\n') || (*(ptr + 4) == '\0')))
              {
                 dd[dir_pos].stupid_mode = GET_ONCE_ONLY;
                 ptr += 4;
              }
              else
              {
                 dd[dir_pos].stupid_mode = NO;
              }
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "The directory option 'store remote list' is depreciated! Please use 'store retrieve list' instead.");
              problems_found++;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & DEL_QUEUED_FILES_FLAG) == 0) &&
               (strncmp(ptr, DEL_QUEUED_FILES_ID, DEL_QUEUED_FILES_ID_LENGTH) == 0))
           {
              used |= DEL_QUEUED_FILES_FLAG;
              ptr += DEL_QUEUED_FILES_ID_LENGTH;
              if ((*ptr == ' ') || (*ptr == '\t'))
              {
                 int  length = 0;
                 char number[MAX_INT_LENGTH + 1];

                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
                 {
                    number[length] = *ptr;
                    ptr++; length++;
                 }
                 if ((length > 0) && (length != MAX_INT_LENGTH))
                 {
                    number[length] = '\0';
                    dd[dir_pos].queued_file_time = atoi(number) * 3600;
                 }
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              if ((dd[dir_pos].delete_files_flag & QUEUED_FILES) == 0)
              {
                 dd[dir_pos].delete_files_flag |= QUEUED_FILES;
              }
              dd[dir_pos].in_dc_flag |= QUEUED_FILES_IDC;
           }
      else if (((used & DEL_OLD_LOCKED_FILES_FLAG) == 0) &&
               (strncmp(ptr, DEL_OLD_LOCKED_FILES_ID, DEL_OLD_LOCKED_FILES_ID_LENGTH) == 0))
           {
              used |= DEL_OLD_LOCKED_FILES_FLAG;
              ptr += DEL_OLD_LOCKED_FILES_ID_LENGTH;
              if ((*ptr == ' ') || (*ptr == '\t'))
              {
                 int  length = 0;
                 char number[MAX_INT_LENGTH + 1];

                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
                 {
                    number[length] = *ptr;
                    ptr++; length++;
                 }
                 if ((length > 0) && (length != MAX_INT_LENGTH))
                 {
                    number[length] = '\0';
                    dd[dir_pos].locked_file_time = atoi(number) * 3600;
                 }
                 if (type == REMOTE_DIR)
                 {
                    if ((dd[dir_pos].delete_files_flag & OLD_RLOCKED_FILES) == 0)
                    {
                       dd[dir_pos].delete_files_flag |= OLD_RLOCKED_FILES;
                    }
                 }
                 else
                 {
                    if ((dd[dir_pos].delete_files_flag & OLD_LOCKED_FILES) == 0)
                    {
                       dd[dir_pos].delete_files_flag |= OLD_LOCKED_FILES;
                    }
                 }
                 dd[dir_pos].in_dc_flag |= OLD_LOCKED_FILES_IDC;
              }
              else
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "No time given for directory option `%s' for directory `%s'.",
                               DEL_OLD_LOCKED_FILES_ID, dd[dir_pos].dir_name);
                 problems_found++;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & DONT_DEL_UNKNOWN_FILES_FLAG) == 0) &&
               (strncmp(ptr, DONT_DEL_UNKNOWN_FILES_ID, DONT_DEL_UNKNOWN_FILES_ID_LENGTH) == 0))
           {
              used |= DONT_DEL_UNKNOWN_FILES_FLAG;
              ptr += DONT_DEL_UNKNOWN_FILES_ID_LENGTH;
              dd[dir_pos].in_dc_flag |= DONT_DELUKW_FILES_IDC;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              /*
               * This is dafault, actually this option is not needed.
               */
           }
      else if (((used & REP_UNKNOWN_FILES_FLAG) == 0) &&
               (strncmp(ptr, REP_UNKNOWN_FILES_ID, REP_UNKNOWN_FILES_ID_LENGTH) == 0))
           {
              used |= REP_UNKNOWN_FILES_FLAG;
              ptr += REP_UNKNOWN_FILES_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].report_unknown_files = YES;
              dd[dir_pos].in_dc_flag |= REPUKW_FILES_IDC;
           }
#ifdef WITH_DUP_CHECK
      else if (((used & DUPCHECK_FLAG) == 0) &&
               (strncmp(ptr, DUPCHECK_ID, DUPCHECK_ID_LENGTH) == 0))
           {
              used |= DUPCHECK_FLAG;
              ptr = eval_dupcheck_options(ptr, &dd[dir_pos].dup_check_timeout,
                                          &dd[dir_pos].dup_check_flag, NULL);

           }
#endif
      else if (((used2 & DEL_UNREADABLE_FILES_FLAG) == 0) &&
               (strncmp(ptr, DEL_UNREADABLE_FILES_ID, DEL_UNREADABLE_FILES_ID_LENGTH) == 0))
           {
              used2 |= DEL_UNREADABLE_FILES_FLAG;
              ptr += DEL_UNREADABLE_FILES_ID_LENGTH;
              if ((*ptr == ' ') || (*ptr == '\t'))
              {
                 int  length = 0;
                 char number[MAX_INT_LENGTH + 1];

                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
                 {
                    number[length] = *ptr;
                    ptr++; length++;
                 }
                 if ((length > 0) && (length != MAX_INT_LENGTH))
                 {
                    number[length] = '\0';
                    dd[dir_pos].unreadable_file_time = atoi(number) * 3600;
                 }
                 if ((dd[dir_pos].delete_files_flag & UNREADABLE_FILES) == 0)
                 {
                    dd[dir_pos].delete_files_flag |= UNREADABLE_FILES;
                 }
                 dd[dir_pos].in_dc_flag |= UNREADABLE_FILES_IDC;
              }
              else
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "No time given for directory option `%s' for directory `%s'.",
                               DEL_UNREADABLE_FILES_ID, dd[dir_pos].dir_name);
                 problems_found++;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & DO_NOT_PARALLELIZE_FLAG) == 0) &&
               (strncmp(ptr, DO_NOT_PARALLELIZE_ID, DO_NOT_PARALLELIZE_ID_LENGTH) == 0))
           {
              used |= DO_NOT_PARALLELIZE_FLAG;
              ptr += DO_NOT_PARALLELIZE_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].do_not_parallelize = YES;
           }
      else if (((used2 & DO_NOT_MOVE_FLAG) == 0) &&
               (strncmp(ptr, FORCE_COPY_ID, FORCE_COPY_ID_LENGTH) == 0))
           {
              used2 |= DO_NOT_MOVE_FLAG;
              ptr += FORCE_COPY_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].do_not_move = YES;
           }
      else if (((used & ACCEPT_DOT_FILES_FLAG) == 0) &&
               (strncmp(ptr, ACCEPT_DOT_FILES_ID, ACCEPT_DOT_FILES_ID_LENGTH) == 0))
           {
              used |= ACCEPT_DOT_FILES_FLAG;
              ptr += ACCEPT_DOT_FILES_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].accept_dot_files = YES;
           }
      else if (((used & DO_NOT_GET_DIR_LIST_FLAG) == 0) &&
               (strncmp(ptr, DO_NOT_GET_DIR_LIST_ID, DO_NOT_GET_DIR_LIST_ID_LENGTH) == 0))
           {
              used |= DO_NOT_GET_DIR_LIST_FLAG;
              ptr += DO_NOT_GET_DIR_LIST_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].do_not_get_dir_list = YES;
           }
      else if (((used2 & URL_CREATES_FILE_NAME_FLAG) == 0) &&
               (strncmp(ptr, URL_CREATES_FILE_NAME_ID, URL_CREATES_FILE_NAME_ID_LENGTH) == 0))
           {
              used2 |= URL_CREATES_FILE_NAME_FLAG;
              ptr += URL_CREATES_FILE_NAME_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].url_creates_file_name = YES;
           }
      else if (((used2 & URL_WITH_INDEX_FILE_NAME_FLAG) == 0) &&
               (strncmp(ptr, URL_WITH_INDEX_FILE_NAME_ID, URL_WITH_INDEX_FILE_NAME_ID_LENGTH) == 0))
           {
              used2 |= URL_WITH_INDEX_FILE_NAME_FLAG;
              ptr += URL_WITH_INDEX_FILE_NAME_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].url_with_index_file_name = YES;
           }
      else if (((used2 & NO_DELIMITER_FLAG) == 0) &&
               (strncmp(ptr, NO_DELIMITER_ID, NO_DELIMITER_ID_LENGTH) == 0))
           {
              used2 |= NO_DELIMITER_FLAG;
              ptr += NO_DELIMITER_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].no_delimiter = YES;
           }
      else if (((used2 & KEEP_PATH_FLAG) == 0) &&
               (strncmp(ptr, KEEP_PATH_ID, KEEP_PATH_ID_LENGTH) == 0))
           {
              used2 |= KEEP_PATH_FLAG;
              ptr += KEEP_PATH_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].keep_path = YES;
           }
      else if (((used & DONT_CREATE_SOURCE_DIR_FLAG) == 0) &&
               (strncmp(ptr, DONT_CREATE_SOURCE_DIR_ID, DONT_CREATE_SOURCE_DIR_ID_LENGTH) == 0))
           {
              used |= DONT_CREATE_SOURCE_DIR_FLAG;
              ptr += DONT_CREATE_SOURCE_DIR_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].dont_create_source_dir = YES;
           }
      else if (((used & CREATE_SOURCE_DIR_FLAG) == 0) &&
               (strncmp(ptr, CREATE_SOURCE_DIR_ID, CREATE_SOURCE_DIR_ID_LENGTH) == 0))
           {
              int n;

              used |= CREATE_SOURCE_DIR_FLAG;
              ptr += CREATE_SOURCE_DIR_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              end_ptr = ptr;
              while ((*end_ptr != '\n') && (*end_ptr != '\0') &&
                     (*end_ptr != ' ') && (*end_ptr != '\t'))
              {
                 end_ptr++;
              }
              n = end_ptr - ptr;
              if ((n == 3) || (n == 4))
              {
                 dd[dir_pos].dir_mode = 0;
                 if (n == 4)
                 {
                    switch (*ptr)
                    {
                       case '7' :
                          dd[dir_pos].dir_mode |= S_ISUID | S_ISGID | S_ISVTX;
                          break;
                       case '6' :
                          dd[dir_pos].dir_mode |= S_ISUID | S_ISGID;
                          break;
                       case '5' :
                          dd[dir_pos].dir_mode |= S_ISUID | S_ISVTX;
                          break;
                       case '4' :
                          dd[dir_pos].dir_mode |= S_ISUID;
                          break;
                       case '3' :
                          dd[dir_pos].dir_mode |= S_ISGID | S_ISVTX;
                          break;
                       case '2' :
                          dd[dir_pos].dir_mode |= S_ISGID;
                          break;
                       case '1' :
                          dd[dir_pos].dir_mode |= S_ISVTX;
                          break;
                       case '0' : /* Nothing to be done here. */
                          break;
                       default :
                          update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        "Incorrect parameter for directory option `%s' %c%c%c%c",
                                        CREATE_SOURCE_DIR_ID, ptr,
                                        ptr + 1, ptr + 2, ptr + 3);
                           problems_found++;
                          break;
                    }
                    ptr++;
                 }
                 switch (*ptr)
                 {
                    case '7' :
                       dd[dir_pos].dir_mode |= S_IRUSR | S_IWUSR | S_IXUSR;
                       break;
                    case '6' :
                       dd[dir_pos].dir_mode |= S_IRUSR | S_IWUSR;
                       break;
                    case '5' :
                       dd[dir_pos].dir_mode |= S_IRUSR | S_IXUSR;
                       break;
                    case '4' :
                       dd[dir_pos].dir_mode |= S_IRUSR;
                       break;
                    case '3' :
                       dd[dir_pos].dir_mode |= S_IWUSR | S_IXUSR;
                       break;
                    case '2' :
                       dd[dir_pos].dir_mode |= S_IWUSR;
                       break;
                    case '1' :
                       dd[dir_pos].dir_mode |= S_IXUSR;
                       break;
                    case '0' : /* Nothing to be done here. */
                       break;
                    default :
                       if (n == 4)
                       {
                          update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        "Incorrect parameter for directory option `%s' %c%c%c%c",
                                        CREATE_SOURCE_DIR_ID, ptr - 1, ptr,
                                        ptr + 1, ptr + 2);
                       }
                       else
                       {
                          update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        "Incorrect parameter for directory option `%s' %c%c%c",
                                        CREATE_SOURCE_DIR_ID, ptr, ptr + 1,
                                        ptr + 2);
                       }
                       problems_found++;
                       break;
                 }
                 ptr++;
                 switch (*ptr)
                 {
                    case '7' :
                       dd[dir_pos].dir_mode |= S_IRGRP | S_IWGRP | S_IXGRP;
                       break;
                    case '6' :
                       dd[dir_pos].dir_mode |= S_IRGRP | S_IWGRP;
                       break;
                    case '5' :
                       dd[dir_pos].dir_mode |= S_IRGRP | S_IXGRP;
                       break;
                    case '4' :
                       dd[dir_pos].dir_mode |= S_IRGRP;
                       break;
                    case '3' :
                       dd[dir_pos].dir_mode |= S_IWGRP | S_IXGRP;
                       break;
                    case '2' :
                       dd[dir_pos].dir_mode |= S_IWGRP;
                       break;
                    case '1' :
                       dd[dir_pos].dir_mode |= S_IXGRP;
                       break;
                    case '0' : /* Nothing to be done here. */
                       break;
                    default :
                       if (n == 4)
                       {
                          update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        "Incorrect parameter for directory option `%s' %c%c%c%c",
                                        CREATE_SOURCE_DIR_ID, ptr - 2, ptr - 1,
                                        ptr, ptr + 1);
                       }
                       else
                       {
                          update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        "Incorrect parameter for directory option `%s' %c%c%c",
                                        CREATE_SOURCE_DIR_ID, ptr - 1, ptr,
                                        ptr + 1);
                       }
                       problems_found++;
                       break;
                 }
                 ptr++;
                 switch (*ptr)
                 {
                    case '7' :
                       dd[dir_pos].dir_mode |= S_IROTH | S_IWOTH | S_IXOTH;
                       break;
                    case '6' :
                       dd[dir_pos].dir_mode |= S_IROTH | S_IWOTH;
                       break;
                    case '5' :
                       dd[dir_pos].dir_mode |= S_IROTH | S_IXOTH;
                       break;
                    case '4' :
                       dd[dir_pos].dir_mode |= S_IROTH;
                       break;
                    case '3' :
                       dd[dir_pos].dir_mode |= S_IWOTH | S_IXOTH;
                       break;
                    case '2' :
                       dd[dir_pos].dir_mode |= S_IWOTH;
                       break;
                    case '1' :
                       dd[dir_pos].dir_mode |= S_IXOTH;
                       break;
                    case '0' : /* Nothing to be done here. */
                       break;
                    default :
                       if (n == 4)
                       {
                          update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        "Incorrect parameter for directory option `%s' %c%c%c%c",
                                        CREATE_SOURCE_DIR_ID, ptr - 3, ptr - 2,
                                        ptr - 1, ptr);
                       }
                       else
                       {
                          update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        "Incorrect parameter for directory option `%s' %c%c%c",
                                        CREATE_SOURCE_DIR_ID, ptr - 2, ptr - 1,
                                        ptr);
                       }
                       problems_found++;
                       break;
                 }
              }
              ptr = end_ptr;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].create_source_dir = YES;
              dd[dir_pos].in_dc_flag |= CREATE_SRC_DIR_IDC;
           }
      else if (((used2 & LS_DATA_FILENAME_FLAG) == 0) &&
               (strncmp(ptr, LS_DATA_FILENAME_ID,
                        LS_DATA_FILENAME_ID_LENGTH) == 0))
           {
              int  length = 0;

              used2 |= LS_DATA_FILENAME_FLAG;
              ptr += LS_DATA_FILENAME_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((length < MAX_DIR_ALIAS_LENGTH) && (*ptr != '\n') &&
                     (*ptr != '\0'))
              {
                 dd[dir_pos].ls_data_alias[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_DIR_ALIAS_LENGTH))
              {
                 dd[dir_pos].ls_data_alias[length] = '\0';
              }
              else
              {
                 dd[dir_pos].ls_data_alias[0] = '\0';
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "For directory option `%s' for directory `%s', the value is to long. May only be %d bytes long.",
                               LS_DATA_FILENAME_ID, dd[dir_pos].dir_name,
                               MAX_DIR_ALIAS_LENGTH);
                 problems_found++;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used2 & TIMEZONE_FLAG) == 0) &&
               (strncmp(ptr, TIMEZONE_ID, TIMEZONE_ID_LENGTH) == 0))
           {
              int  length = 0;

              used2 |= TIMEZONE_FLAG;
              ptr += TIMEZONE_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((length < MAX_TIMEZONE_LENGTH) && (*ptr != '\n') &&
                     (*ptr != '\0'))
              {
                 dd[dir_pos].timezone[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_TIMEZONE_LENGTH))
              {
                 dd[dir_pos].timezone[length] = '\0';
#ifdef TZDIR
                 if (timezone_name_check(dd[dir_pos].timezone) == INCORRECT)
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "Unable to find specified timezone (%s) in %s",
                                  dd[dir_pos].timezone, TZDIR);
                    problems_found++;
                    dd[dir_pos].timezone[0] = '\0';
                 }
#endif
              }
              else
              {
                 dd[dir_pos].timezone[0] = '\0';
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "For directory option `%s' for directory `%s', the value is to long. May only be %d bytes long. Please contact maintainer (%s) if this is a valid timezone.",
                               TIMEZONE_ID, dd[dir_pos].dir_name,
                               MAX_TIMEZONE_LENGTH, AFD_MAINTAINER);
                 problems_found++;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & DIR_INFO_TIME_FLAG) == 0) &&
               (strncmp(ptr, DIR_INFO_TIME_ID, DIR_INFO_TIME_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_LONG_LENGTH + 1];

              used |= DIR_INFO_TIME_FLAG;
              ptr += DIR_INFO_TIME_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_LONG_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_LONG_LENGTH))
              {
                 number[length] = '\0';
                 dd[dir_pos].info_time = (time_t)atol(number);
                 if (dd[dir_pos].info_time < 0)
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
#if SIZEOF_TIME_T == 4
                                  "A value less then 0 for directory option `%s' for directory `%s' is no possible, setting default %ld.",
#else
                                  "A value less then 0 for directory option `%s' for directory `%s' is no possible, setting default %lld.",
#endif
                                  DIR_INFO_TIME_ID, dd[dir_pos].dir_name,
                                  (pri_time_t)default_info_time);
                    problems_found++;
                    dd[dir_pos].info_time = default_info_time;
                 }
                 else
                 {
                    dd[dir_pos].in_dc_flag |= INFO_TIME_IDC;
                 }
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }
              else
              {
                 if (length > 0)
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "Value to long for directory option `%s' for directory `%s'.",
                                  DIR_INFO_TIME_ID, dd[dir_pos].dir_name);
                    update_db_log(WARN_SIGN, NULL, 0, cmd_fp, NULL,
                                  "May only be %d bytes long.",
                                  MAX_LONG_LENGTH);
                 }
                 else
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "No value found or syntax wrong for directory option `%s' for directory `%s'.",
                                  DIR_INFO_TIME_ID, dd[dir_pos].dir_name);
                 }
                 problems_found++;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & DIR_WARN_TIME_FLAG) == 0) &&
               (strncmp(ptr, DIR_WARN_TIME_ID, DIR_WARN_TIME_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_LONG_LENGTH + 1];

              used |= DIR_WARN_TIME_FLAG;
              ptr += DIR_WARN_TIME_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_LONG_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_LONG_LENGTH))
              {
                 number[length] = '\0';
                 dd[dir_pos].warn_time = (time_t)atol(number);
                 if (dd[dir_pos].warn_time < 0)
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
#if SIZEOF_TIME_T == 4
                                  "A value less then 0 for directory option `%s' for directory `%s' is no possible, setting default %ld.",
#else
                                  "A value less then 0 for directory option `%s' for directory `%s' is no possible, setting default %lld.",
#endif
                                  DIR_WARN_TIME_ID, dd[dir_pos].dir_name,
                                  (pri_time_t)default_warn_time);
                    problems_found++;
                    dd[dir_pos].warn_time = default_warn_time;
                 }
                 else
                 {
                    dd[dir_pos].in_dc_flag |= WARN_TIME_IDC;
                 }
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }
              else
              {
                 if (length > 0)
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "Value to long for directory option `%s' for directory `%s'.",
                                  DIR_WARN_TIME_ID, dd[dir_pos].dir_name);
                    update_db_log(WARN_SIGN, NULL, 0, cmd_fp, NULL,
                                  "May only be %d bytes long.",
                                  MAX_LONG_LENGTH);
                 }
                 else
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "No value found or syntax wrong for directory option `%s' for directory `%s'.",
                                  DIR_WARN_TIME_ID, dd[dir_pos].dir_name);
                 }
                 problems_found++;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & KEEP_CONNECTED_FLAG) == 0) &&
               (strncmp(ptr, KEEP_CONNECTED_ID, KEEP_CONNECTED_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_INT_LENGTH + 1];

              used |= KEEP_CONNECTED_FLAG;
              ptr += KEEP_CONNECTED_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_INT_LENGTH))
              {
                 number[length] = '\0';
                 dd[dir_pos].keep_connected = (unsigned int)atoi(number);
                 dd[dir_pos].in_dc_flag |= KEEP_CONNECTED_IDC;
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }
              else
              {
                 if (length > 0)
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "Value to long for directory option `%s' for directory `%s'.",
                                  KEEP_CONNECTED_ID, dd[dir_pos].dir_name);
                    update_db_log(WARN_SIGN, NULL, 0, cmd_fp, NULL,
                                  "May only be %d bytes long.",
                                  MAX_INT_LENGTH);
                 }
                 else
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "No value found or syntax wrong for directory option `%s' for directory `%s'.",
                                  KEEP_CONNECTED_ID, dd[dir_pos].dir_name);
                 }
                 problems_found++;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & WAIT_FOR_FILENAME_FLAG) == 0) &&
               (strncmp(ptr, WAIT_FOR_FILENAME_ID, WAIT_FOR_FILENAME_ID_LENGTH) == 0))
           {
              int length = 0;

              used |= WAIT_FOR_FILENAME_FLAG;
              ptr += WAIT_FOR_FILENAME_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((*ptr != '\n') && (*ptr != '\0') && (*ptr != ' ') &&
                     (*ptr != '\t') && (length < MAX_WAIT_FOR_LENGTH))
              {
                 if (*ptr == '\\')
                 {
                    ptr++;
                 }
                 dd[dir_pos].wait_for_filename[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_WAIT_FOR_LENGTH))
              {
                 dd[dir_pos].wait_for_filename[length] = '\0';
              }
              else
              {
                 dd[dir_pos].wait_for_filename[0] = '\0';
                 if (length > 0)
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "File name or pattern to long for directory option `%s' for directory `%s'.",
                                  WAIT_FOR_FILENAME_ID, dd[dir_pos].dir_name);
                    update_db_log(WARN_SIGN, NULL, 0, cmd_fp, NULL,
                                  "May only be %d bytes long.",
                                  MAX_WAIT_FOR_LENGTH);
                 }
                 else
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "No file name or pattern for directory option `%s' for directory `%s'.",
                                  WAIT_FOR_FILENAME_ID, dd[dir_pos].dir_name);
                 }
                 problems_found++;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & ACCUMULATE_FLAG) == 0) &&
               (strncmp(ptr, ACCUMULATE_ID, ACCUMULATE_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_INT_LENGTH + 1];

              used |= ACCUMULATE_FLAG;
              ptr += ACCUMULATE_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_INT_LENGTH))
              {
                 number[length] = '\0';
                 dd[dir_pos].accumulate = atoi(number);
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }
              else
              {
                 if (length > 0)
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "Value to long for directory option `%s' for directory `%s'.",
                                  ACCUMULATE_ID, dd[dir_pos].dir_name);
                    update_db_log(WARN_SIGN, NULL, 0, cmd_fp, NULL,
                                  "May only be %d bytes long.",
                                  MAX_INT_LENGTH);
                 }
                 else
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "No value found or syntax wrong for directory option `%s' for directory `%s'.",
                                  ACCUMULATE_ID, dd[dir_pos].dir_name);
                 }
                 problems_found++;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & ACCUMULATE_SIZE_FLAG) == 0) &&
               (strncmp(ptr, ACCUMULATE_SIZE_ID, ACCUMULATE_SIZE_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_OFF_T_LENGTH + 1];

              used |= ACCUMULATE_SIZE_FLAG;
              ptr += ACCUMULATE_SIZE_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_OFF_T_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_OFF_T_LENGTH))
              {
                 number[length] = '\0';
                 dd[dir_pos].accumulate_size = (off_t)str2offt(number, NULL, 10);
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }
              else
              {
                 if (length > 0)
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "Value to long for directory option `%s' for directory `%s'.",
                                  ACCUMULATE_SIZE_ID, dd[dir_pos].dir_name);
                    update_db_log(WARN_SIGN, NULL, 0, cmd_fp, NULL,
                                  "May only be %d bytes long.",
                                  MAX_OFF_T_LENGTH);
                 }
                 else
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "No value found or syntax wrong for directory option `%s' for directory `%s'.",
                                  ACCUMULATE_SIZE_ID, dd[dir_pos].dir_name);
                 }
                 problems_found++;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & FORCE_REREAD_FLAG) == 0) &&
               (strncmp(ptr, FORCE_REREAD_REMOTE_ID,
                        FORCE_REREAD_REMOTE_ID_LENGTH) == 0))
           {
              used |= FORCE_REREAD_FLAG;
              ptr += FORCE_REREAD_REMOTE_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].force_reread = REMOTE_ONLY;
           }
      else if (((used & FORCE_REREAD_FLAG) == 0) &&
               (strncmp(ptr, FORCE_REREAD_LOCAL_ID,
                        FORCE_REREAD_LOCAL_ID_LENGTH) == 0))
           {
              used |= FORCE_REREAD_FLAG;
              ptr += FORCE_REREAD_LOCAL_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].force_reread = LOCAL_ONLY;
           }
      else if (((used & FORCE_REREAD_FLAG) == 0) &&
               (strncmp(ptr, FORCE_REREAD_ID, FORCE_REREAD_ID_LENGTH) == 0))
           {
              used |= FORCE_REREAD_FLAG;
              ptr += FORCE_REREAD_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].force_reread = YES;
           }
      else if (((used & IGNORE_SIZE_FLAG) == 0) &&
               (strncmp(ptr, IGNORE_SIZE_ID, IGNORE_SIZE_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_OFF_T_LENGTH + 1];

              used |= IGNORE_SIZE_FLAG;
              ptr += IGNORE_SIZE_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              if (*ptr == '>')
              {
                 dd[dir_pos].gt_lt_sign |= ISIZE_GREATER_THEN;
                 ptr++;
              }
              else if (*ptr == '<')
                   {
                      dd[dir_pos].gt_lt_sign |= ISIZE_LESS_THEN;
                      ptr++;
                   }
              else if ((*ptr == '=') || (isdigit((int)(*ptr))))
                   {
                      dd[dir_pos].gt_lt_sign |= ISIZE_EQUAL;
                      if (*ptr == '=')
                      {
                         ptr++;
                      }
                   }
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_OFF_T_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_OFF_T_LENGTH))
              {
                 number[length] = '\0';
#ifdef HAVE_STRTOLL
# if SIZEOF_OFF_T == 4
                 if ((dd[dir_pos].ignore_size = (off_t)strtol(number, NULL, 10)) == LONG_MAX)
# else
#  ifdef LLONG_MAX
                 if ((dd[dir_pos].ignore_size = (off_t)strtoll(number, NULL, 10)) == LLONG_MAX)
#  else
                 if ((dd[dir_pos].ignore_size = (off_t)strtoll(number, NULL, 10)) == LONG_MAX)
#  endif
# endif
#else
                 if ((dd[dir_pos].ignore_size = (off_t)strtol(number, NULL, 10)) == LONG_MAX)
#endif
                 {
                    dd[dir_pos].ignore_size = -1;
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "Value %s for option <%s> in DIR_CONFIG for directory `%s', to large causing overflow. Ignoring.",
                                  number, dd[dir_pos].dir_name, IGNORE_SIZE_ID);
                    problems_found++;
                 }
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }
              else
              {
                 if (length > 0)
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "Value to long for directory option `%s' for directory `%s'.",
                                  IGNORE_SIZE_ID, dd[dir_pos].dir_name);
                    update_db_log(WARN_SIGN, NULL, 0, cmd_fp, NULL,
                                  "May only be %d bytes long.",
                                  MAX_OFF_T_LENGTH);
                 }
                 else
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "No value found or syntax wrong for directory option `%s' for directory `%s'.",
                                  IGNORE_SIZE_ID, dd[dir_pos].dir_name);
                 }
                 problems_found++;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & IGNORE_FILE_TIME_FLAG) == 0) &&
               (strncmp(ptr, IGNORE_FILE_TIME_ID, IGNORE_FILE_TIME_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_INT_LENGTH + 1];

              used |= IGNORE_FILE_TIME_FLAG;
              ptr += IGNORE_FILE_TIME_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              if (*ptr == '>')
              {
                 dd[dir_pos].gt_lt_sign |= IFTIME_GREATER_THEN;
                 ptr++;
              }
              else if (*ptr == '<')
                   {
                      dd[dir_pos].gt_lt_sign |= IFTIME_LESS_THEN;
                      ptr++;
                   }
              else if ((*ptr == '=') || (isdigit((int)(*ptr))))
                   {
                      dd[dir_pos].gt_lt_sign |= IFTIME_EQUAL;
                      if (*ptr == '=')
                      {
                         ptr++;
                      }
                   }
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_INT_LENGTH))
              {
                 number[length] = '\0';
                 dd[dir_pos].ignore_file_time = (unsigned int)strtoul(number, NULL, 10);
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }
              else
              {
                 if (length > 0)
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "Value to long for directory option `%s' for directory `%s'.",
                                  IGNORE_FILE_TIME_ID, dd[dir_pos].dir_name);
                    update_db_log(WARN_SIGN, NULL, 0, cmd_fp, NULL,
                                  "May only be %d bytes long.",
                                  MAX_INT_LENGTH);
                 }
                 else
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "No value found or syntax wrong for directory option `%s' for directory `%s'.",
                                  IGNORE_FILE_TIME_ID, dd[dir_pos].dir_name);
                 }
                 problems_found++;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & MAX_FILES_FLAG) == 0) &&
               (strncmp(ptr, MAX_FILES_ID, MAX_FILES_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_INT_LENGTH + 1];

              used |= MAX_FILES_FLAG;
              ptr += MAX_FILES_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_INT_LENGTH))
              {
                 number[length] = '\0';
                 dd[dir_pos].max_copied_files = (unsigned int)strtoul(number, NULL, 10);
                 dd[dir_pos].in_dc_flag |= MAX_CP_FILES_IDC;
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }
              else
              {
                 if (length > 0)
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "Value to long for directory option `%s' for directory `%s'.",
                                  MAX_FILES_ID, dd[dir_pos].dir_name);
                    update_db_log(WARN_SIGN, NULL, 0, cmd_fp, NULL,
                                  "May only be %d bytes long.",
                                  MAX_INT_LENGTH);
                 }
                 else
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "No value found or syntax wrong for directory option `%s' for directory `%s'.",
                                  MAX_FILES_ID, dd[dir_pos].dir_name);
                 }
                 problems_found++;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used & MAX_SIZE_FLAG) == 0) &&
               (strncmp(ptr, MAX_SIZE_ID, MAX_SIZE_ID_LENGTH) == 0))
           {
              int  length = 0;
              char number[MAX_OFF_T_LENGTH + 1];

              used |= MAX_SIZE_FLAG;
              ptr += MAX_SIZE_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((isdigit((int)(*ptr))) && (length < MAX_OFF_T_LENGTH))
              {
                 number[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_OFF_T_LENGTH))
              {
                 number[length] = '\0';
                 dd[dir_pos].max_copied_file_size = (off_t)str2offt(number, NULL, 10) * MAX_COPIED_FILE_SIZE_UNIT;
                 dd[dir_pos].in_dc_flag |= MAX_CP_FILE_SIZE_IDC;
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }
              else
              {
                 if (length > 0)
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "Value to long for directory option `%s' for directory `%s'.",
                                  MAX_SIZE_ID, dd[dir_pos].dir_name);
                    update_db_log(WARN_SIGN, NULL, 0, cmd_fp, NULL,
                                  "May only be %d bytes long.",
                                  MAX_OFF_T_LENGTH);
                 }
                 else
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "No value found or syntax wrong for directory option `%s' for directory `%s'.",
                                  MAX_SIZE_ID, dd[dir_pos].dir_name);
                 }
                 problems_found++;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
#ifndef _WITH_PTHREAD
      else if (((used & IMPORTANT_DIR_FLAG) == 0) &&
               (strncmp(ptr, IMPORTANT_DIR_ID, IMPORTANT_DIR_ID_LENGTH) == 0))
           {
              used |= IMPORTANT_DIR_FLAG;
              ptr += IMPORTANT_DIR_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].important_dir = YES;
           }
#endif
      else if (((used2 & LOCAL_REMOTE_DIR_FLAG) == 0) &&
               (strncmp(ptr, LOCAL_REMOTE_DIR_ID, LOCAL_REMOTE_DIR_ID_LENGTH) == 0))
           {
              int length = 0;

              used2 |= LOCAL_REMOTE_DIR_FLAG;
              ptr += LOCAL_REMOTE_DIR_ID_LENGTH;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              while ((*ptr != '\n') && (*ptr != '\0') && (*ptr != ' ') &&
                     (*ptr != '\t') && (length < MAX_FILENAME_LENGTH))
              {
                 if (*ptr == '\\')
                 {
                    ptr++;
                 }
                 dd[dir_pos].retrieve_work_dir[length] = *ptr;
                 ptr++; length++;
              }
              if ((length > 0) && (length != MAX_FILENAME_LENGTH))
              {
                 dd[dir_pos].retrieve_work_dir[length] = '\0';
                 dd[dir_pos].in_dc_flag |= LOCAL_REMOTE_DIR_IDC;
              }
              else
              {
                 dd[dir_pos].retrieve_work_dir[0] = '\0';
                 if (length > 0)
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "Directory option `%s' for directory `%s' to long.",
                                  LOCAL_REMOTE_DIR_ID, dd[dir_pos].dir_name);
                    update_db_log(WARN_SIGN, NULL, 0, cmd_fp, NULL,
                                  "May only be %d bytes long.", MAX_PATH_LENGTH);
                 }
                 else
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "No directory name for directory option `%s' for directory `%s'.",
                                  LOCAL_REMOTE_DIR_ID, dd[dir_pos].dir_name);
                 }
                 problems_found++;
              }
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
      else if (((used2 & ONE_PROCESS_JUST_SCANNING_FLAG) == 0) &&
               (strncmp(ptr, ONE_PROCESS_JUST_SCANNING_ID, ONE_PROCESS_JUST_SCANNING_ID_LENGTH) == 0))
           {
              used2 |= ONE_PROCESS_JUST_SCANNING_FLAG;
              ptr += ONE_PROCESS_JUST_SCANNING_ID_LENGTH;
              while ((*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
              dd[dir_pos].one_process_just_scaning = YES;
           }
           else
           {
              /* Ignore this option. */
              end_ptr = ptr;
              while ((*end_ptr != '\n') && (*end_ptr != '\0'))
              {
                 end_ptr++;
              }
              byte_buf = *end_ptr;
              *end_ptr = '\0';
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "Unknown or duplicate option <%s> in DIR_CONFIG file for directory `%s'.",
                            ptr, dd[dir_pos].dir_name);
              problems_found++;
              *end_ptr = byte_buf;
              ptr = end_ptr;
           }

      while (*ptr == '\n')
      {
         ptr++;
      }
   } /* while (*ptr != '\0') */

   if (dd[dir_pos].unknown_file_time == -1)
   {
      dd[dir_pos].unknown_file_time = old_file_time;
   }
   if (dd[dir_pos].queued_file_time == -1)
   {
      dd[dir_pos].queued_file_time = old_file_time;
   }
   if (dd[dir_pos].locked_file_time == -1)
   {
      dd[dir_pos].locked_file_time = old_file_time;
   }
   if (dd[dir_pos].retrieve_work_dir[0] == '\0')
   {
      (void)strcpy(dd[dir_pos].retrieve_work_dir, p_work_dir);
   }

   return(problems_found);
}
