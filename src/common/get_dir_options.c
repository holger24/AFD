/*
 *  get_dir_options.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2024 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_dir_options - gets directory options and store them in a structure
 **
 ** SYNOPSIS
 **   void get_dir_options(unsigned int dir_id, struct dir_options *d_o)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.07.2001 H.Kiehl Created
 **   22.05.2002 H.Kiehl Separate old file times for unknown and queued files.
 **   03.09.2004 H.Kiehl Added "ignore size" and "ignore file time" options.
 **   17.08.2005 H.Kiehl Move this function to common section
 **                      (from Motif/common).
 **   20.12.2006 H.Kiehl Added dupcheck option filename without last
 **                      suffix.
 **   16.02.2007 H.Kiehl Added 'warn time' option.
 **   24.02.2007 H.Kiehl Added 'inotify' option.
 **   24.11.2012 H.Kiehl Added support for another CRC type.
 **   13.01.2024 H.Kiehl Extended 'store retrieve list' with 'once not exact'.
 **   17.01.2024 H.Kiehl Extended 'store retrieve list' with 'not exact'.
 **   02.04.2024 H.Kiehl Added 'get dir list' option.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"
#include "bit_array.h"

/* External global variables. */
extern int                        fra_fd,
                                  no_of_dirs;
extern struct fileretrieve_status *fra;


/*+++++++++++++++++++++++++++ get_dir_options() +++++++++++++++++++++++++*/
void                                                                
get_dir_options(unsigned int dir_id, struct dir_options *d_o)
{
   register int i;
   int          attached = NO;

   if (fra_fd == -1)
   {
      /* Attach to FRA to get directory options. */
      if (fra_attach_passive() != SUCCESS)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to attach to FRA."));
         return;
      }
      attached = YES;
   }
   else
   {
      (void)check_fra(YES);
   }

   d_o->url[0] = '\0';
   d_o->no_of_dir_options = 0;
   for (i = 0; i < no_of_dirs; i++)
   {
      if (dir_id == fra[i].dir_id)
      {
         (void)strcpy(d_o->dir_alias, fra[i].dir_alias);
         if (fra[i].delete_files_flag != 0)
         {
            if ((fra[i].delete_files_flag & UNKNOWN_FILES) &&
                (fra[i].in_dc_flag & UNKNOWN_FILES_IDC))
            {
               if (fra[i].unknown_file_time == -2)
               {
                  (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                                 MAX_OPTION_LENGTH,
                                 "%s -1",
                                 DEL_UNKNOWN_FILES_ID);
               }
               else
               {
                  (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                                 MAX_OPTION_LENGTH,
                                 "%s %d",
                                 DEL_UNKNOWN_FILES_ID,
                                 fra[i].unknown_file_time / 3600);
               }
               d_o->no_of_dir_options++;
               if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
               {
                  goto done;
               }
            }
            if ((fra[i].delete_files_flag & QUEUED_FILES) &&
                (fra[i].in_dc_flag & QUEUED_FILES_IDC))
            {
               (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                              MAX_OPTION_LENGTH,
                              "%s %d",
                              DEL_QUEUED_FILES_ID,
                              fra[i].queued_file_time / 3600);
               d_o->no_of_dir_options++;
               if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
               {
                  goto done;
               }
            }
            if (((fra[i].delete_files_flag & OLD_LOCKED_FILES) ||
                 (fra[i].delete_files_flag & OLD_RLOCKED_FILES)) &&
                (fra[i].in_dc_flag & OLD_LOCKED_FILES_IDC))
            {
               (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                              MAX_OPTION_LENGTH,
                              "%s %d",
                              DEL_OLD_LOCKED_FILES_ID,
                              fra[i].locked_file_time / 3600);
               d_o->no_of_dir_options++;
               if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
               {
                  goto done;
               }
            }
            if ((fra[i].delete_files_flag & UNREADABLE_FILES) &&
                (fra[i].in_dc_flag & UNREADABLE_FILES_IDC))
            {
               (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                              MAX_OPTION_LENGTH,
                              "%s %d",
                              DEL_UNREADABLE_FILES_ID,
                              fra[i].unreadable_file_time / 3600);
               d_o->no_of_dir_options++;
               if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
               {
                  goto done;
               }
            }
         }
         else
         {
            if (fra[i].in_dc_flag & DONT_DELUKW_FILES_IDC)
            {
               (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                            DONT_DEL_UNKNOWN_FILES_ID);
               d_o->no_of_dir_options++;
               if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
               {
                  goto done;
               }
            }
         }
         if ((fra[i].report_unknown_files == NO) &&
             (fra[i].in_dc_flag & DONT_REPUKW_FILES_IDC))
         {
            (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                         DONT_REP_UNKNOWN_FILES_ID);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if ((fra[i].report_unknown_files == YES) &&
             (fra[i].in_dc_flag & REPUKW_FILES_IDC))
         {
            (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                         REP_UNKNOWN_FILES_ID);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
#ifndef _WITH_PTHREAD
         if (fra[i].important_dir == YES)
         {
            (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                         IMPORTANT_DIR_ID);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
#endif
         if ((fra[i].in_dc_flag & INFO_TIME_IDC) &&
             (fra[i].info_time != DEFAULT_DIR_INFO_TIME))
         {
            (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                           MAX_OPTION_LENGTH,
#if SIZEOF_TIME_T == 4
                           "%s %ld",
#else
                           "%s %lld",
#endif
                           DIR_INFO_TIME_ID, (pri_time_t)fra[i].info_time);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if ((fra[i].in_dc_flag & WARN_TIME_IDC) &&
             (fra[i].warn_time != DEFAULT_DIR_WARN_TIME))
         {
            (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                           MAX_OPTION_LENGTH,
#if SIZEOF_TIME_T == 4
                           "%s %ld",
#else
                           "%s %lld",
#endif
                           DIR_WARN_TIME_ID, (pri_time_t)fra[i].warn_time);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if ((fra[i].in_dc_flag & KEEP_CONNECTED_IDC) &&
             (fra[i].keep_connected != DEFAULT_KEEP_CONNECTED_TIME))
         {
            (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                           MAX_OPTION_LENGTH,
                           "%s %u",
                           KEEP_CONNECTED_ID, fra[i].keep_connected);
            d_o->no_of_dir_options++;
         }
#ifdef WITH_INOTIFY
         if (fra[i].in_dc_flag & INOTIFY_FLAG_IDC)
         {
            unsigned int flag = 0;

            if (fra[i].dir_options & INOTIFY_RENAME)
            {
               flag += INOTIFY_RENAME_FLAG;
            }
            if (fra[i].dir_options & INOTIFY_CLOSE)
            {
               flag += INOTIFY_CLOSE_FLAG;
            }
            if (fra[i].dir_options & INOTIFY_CREATE)
            {
               flag += INOTIFY_CREATE_FLAG;
            }
            if (fra[i].dir_options & INOTIFY_DELETE)
            {
               flag += INOTIFY_DELETE_FLAG;
            }
            if (fra[i].dir_options & INOTIFY_ATTRIB)
            {
               flag += INOTIFY_ATTRIB_FLAG;
            }
            (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                           MAX_OPTION_LENGTH,
                           "%s %u", INOTIFY_FLAG_ID, flag);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
#endif
         if ((fra[i].in_dc_flag & KEEP_CONNECTED_IDC) &&
             (fra[i].max_process != MAX_PROCESS_PER_DIR))
         {
            (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                           MAX_OPTION_LENGTH,
                           "%s %d",
                           MAX_PROCESS_ID, fra[i].max_process);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if (fra[i].in_dc_flag & MAX_ERRORS_IDC)
         {
            (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                           MAX_OPTION_LENGTH,
                           "%s %d",
                           MAX_ERRORS_ID, fra[i].max_errors);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if ((fra[i].max_copied_files != MAX_COPIED_FILES) &&
             (fra[i].in_dc_flag & MAX_CP_FILES_IDC))
         {
            (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                           MAX_OPTION_LENGTH,
                           "%s %d",
                           MAX_FILES_ID, fra[i].max_copied_files);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if ((fra[i].max_copied_file_size != (MAX_COPIED_FILE_SIZE * 1024)) &&
             (fra[i].in_dc_flag & MAX_CP_FILE_SIZE_IDC))
         {
            (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                           MAX_OPTION_LENGTH,
#if SIZEOF_OFF_T == 4
                           "%s %ld",
#else
                           "%s %lld",
#endif
                           MAX_SIZE_ID,
                           (pri_off_t)(fra[i].max_copied_file_size / 1024));
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if (fra[i].ignore_size != -1)
         {
            char gt_lt_sign;

            if (fra[i].gt_lt_sign & ISIZE_GREATER_THEN)
            {
               gt_lt_sign = '>';
            }
            else if (fra[i].gt_lt_sign & ISIZE_LESS_THEN)
                 {
                    gt_lt_sign = '<';
                 }
                 else
                 {
                    gt_lt_sign = '=';
                 }
            if (gt_lt_sign == '=')
            {
               (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                              MAX_OPTION_LENGTH,
#if SIZEOF_OFF_T == 4
                              "%s %ld",
#else
                              "%s %lld",
#endif
                              IGNORE_SIZE_ID, (pri_off_t)fra[i].ignore_size);
            }
            else
            {
               (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                              MAX_OPTION_LENGTH,
#if SIZEOF_OFF_T == 4
                              "%s %c%ld",
#else
                              "%s %c%lld",
#endif
                              IGNORE_SIZE_ID, gt_lt_sign,
                              (pri_off_t)fra[i].ignore_size);
            }
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if (fra[i].priority != DEFAULT_PRIORITY)
         {
            (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                           MAX_OPTION_LENGTH,
                           "%s %c",
                           PRIORITY_ID, fra[i].priority);
            d_o->no_of_dir_options++;
         }
         if (fra[i].wait_for_filename[0] != '\0')
         {
            (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                           MAX_OPTION_LENGTH,
                           "%s %s",
                           WAIT_FOR_FILENAME_ID, fra[i].wait_for_filename);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if (fra[i].accumulate != 0)
         {
            (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                           MAX_OPTION_LENGTH,
                           "%s %d",
                           ACCUMULATE_ID, fra[i].accumulate);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if (fra[i].accumulate_size != 0)
         {
            (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                           MAX_OPTION_LENGTH,
#if SIZEOF_OFF_T == 4
                           "%s %ld",
#else
                           "%s %lld",
#endif
                           ACCUMULATE_SIZE_ID,
                           (pri_off_t)fra[i].accumulate_size);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if (fra[i].stupid_mode == NO)
         {
            (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                         STORE_RETRIEVE_LIST_ID);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         else if (fra[i].stupid_mode == NOT_EXACT)
              {
                 (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                              STORE_RETRIEVE_LIST_ID);
                 (void)strcat(d_o->aoptions[d_o->no_of_dir_options], " not exact");
                 d_o->no_of_dir_options++;
                 if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
                 {
                    goto done;
                 }
              }
         else if (fra[i].stupid_mode == GET_ONCE_ONLY)
              {
                 (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                              STORE_RETRIEVE_LIST_ID);
                 (void)strcat(d_o->aoptions[d_o->no_of_dir_options], " once");
                 d_o->no_of_dir_options++;
                 if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
                 {
                    goto done;
                 }
              }
         else if (fra[i].stupid_mode == GET_ONCE_NOT_EXACT)
              {
                 (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                              STORE_RETRIEVE_LIST_ID);
                 (void)strcat(d_o->aoptions[d_o->no_of_dir_options], " once not exact");
                 d_o->no_of_dir_options++;
                 if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
                 {
                    goto done;
                 }
              }
         else if (fra[i].stupid_mode == APPEND_ONLY)
              {
                 (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                              STORE_RETRIEVE_LIST_ID);
                 (void)strcat(d_o->aoptions[d_o->no_of_dir_options], " append");
                 d_o->no_of_dir_options++;
                 if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
                 {
                    goto done;
                 }
              }
         if (fra[i].ls_data_alias[0] != '\0')
         {
            (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                           MAX_OPTION_LENGTH,
                           "%s %s", LS_DATA_FILENAME_ID, fra[i].ls_data_alias);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if (fra[i].dir_options & GET_DIR_LIST_HREF)
         {
            (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                         GET_DIR_LIST_ID);
            (void)strcat(d_o->aoptions[d_o->no_of_dir_options], " href");
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if (fra[i].dir_options & DIR_ZERO_SIZE)
         {
            (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                         DIR_ZERO_SIZE_ID);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         else if (fra[i].dir_options & DONT_GET_DIR_LIST)
              {
                 if (fra[i].in_dc_flag & GET_DIR_LIST_IDC)
                 {
                    (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                                 GET_DIR_LIST_ID);
                    (void)strcat(d_o->aoptions[d_o->no_of_dir_options], " no");
                 }
                 else
                 {
                    (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                                 DO_NOT_GET_DIR_LIST_ID);
                 }
                 d_o->no_of_dir_options++;
                 if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
                 {
                    goto done;
                 }
              }
         if (fra[i].dir_options & URL_CREATES_FILE_NAME)
         {
            (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                         URL_CREATES_FILE_NAME_ID);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if (fra[i].dir_options & URL_WITH_INDEX_FILE_NAME)
         {
            (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                         URL_WITH_INDEX_FILE_NAME_ID);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if (fra[i].dir_options & NO_DELIMITER)
         {
            (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                         NO_DELIMITER_ID);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if (fra[i].dir_options & KEEP_PATH)
         {
            (void)strcpy(d_o->aoptions[d_o->no_of_dir_options], KEEP_PATH_ID);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if (fra[i].dir_options & ONE_PROCESS_JUST_SCANNING)
         {
            (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                         ONE_PROCESS_JUST_SCANNING_ID);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if (fra[i].in_dc_flag & CREATE_SRC_DIR_IDC)
         {
            if (fra[i].dir_mode == 0)
            {
               (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                            CREATE_SOURCE_DIR_ID);
            }
            else
            {
               int  length;
               char str_number[MAX_INT_LENGTH];

               length = snprintf(str_number, MAX_INT_LENGTH, "%04o",
                                 fra[i].dir_mode);
               (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                              MAX_OPTION_LENGTH,
                              "%s %s",
                              CREATE_SOURCE_DIR_ID, &str_number[length - 4]);
            }
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if (fra[i].remove == NO)
         {
            (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                         DO_NOT_REMOVE_ID);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if (fra[i].dir_options & ACCEPT_DOT_FILES)
         {
            (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                         ACCEPT_DOT_FILES_ID);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if (fra[i].dir_options & DO_NOT_PARALLELIZE)
         {
            (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                         DO_NOT_PARALLELIZE_ID);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if (fra[i].dir_options & DO_NOT_MOVE)
         {
            (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                         FORCE_COPY_ID);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if (fra[i].force_reread == YES)
         {
            (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                         FORCE_REREAD_ID);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         else if (fra[i].force_reread == REMOTE_ONLY)
              {
                 (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                              FORCE_REREAD_REMOTE_ID);
                 d_o->no_of_dir_options++;
                 if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
                 {
                    goto done;
                 }
              }
         else if (fra[i].force_reread == LOCAL_ONLY)
              {
                 (void)strcpy(d_o->aoptions[d_o->no_of_dir_options],
                              FORCE_REREAD_LOCAL_ID);
                 d_o->no_of_dir_options++;
                 if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
                 {
                    goto done;
                 }
              }
         if (fra[i].end_character != -1)
         {
            (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                           MAX_OPTION_LENGTH,
                           "%s %d", END_CHARACTER_ID, fra[i].end_character);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if (fra[i].ignore_file_time != 0)
         {
            char gt_lt_sign;

            if (fra[i].gt_lt_sign & IFTIME_GREATER_THEN)
            {
               gt_lt_sign = '>';
            }
            else if (fra[i].gt_lt_sign & IFTIME_LESS_THEN)
                 {
                    gt_lt_sign = '<';
                 }
                 else
                 {
                    gt_lt_sign = '=';
                 }
            if (gt_lt_sign == '=')
            {
               (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                              MAX_OPTION_LENGTH,
                              "%s %u",
                              IGNORE_FILE_TIME_ID, fra[i].ignore_file_time);
            }
            else
            {
               (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                              MAX_OPTION_LENGTH,
                              "%s %c%u", IGNORE_FILE_TIME_ID, gt_lt_sign,
                              fra[i].ignore_file_time);
            }
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if (fra[i].timezone[0] != '\0')
         {
            (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                           MAX_OPTION_LENGTH,
                           "%s %s", TIMEZONE_ID, fra[i].timezone);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if (fra[i].no_of_time_entries > 0)
         {
            int           j,
                          length,
                          ii;
#ifndef HAVE_LONG_LONG
            unsigned char full_minute[8];
#endif

            for (j = 0; j < fra[i].no_of_time_entries; j++)
            {
               length = snprintf(d_o->aoptions[d_o->no_of_dir_options],
                                 MAX_OPTION_LENGTH,
                                 "%s ", TIME_ID);

               if (fra[i].te[j].month == TIME_EXTERNAL)
               {
                  d_o->aoptions[d_o->no_of_dir_options][length] = 'e';
                  d_o->aoptions[d_o->no_of_dir_options][length + 1] = 'x';
                  d_o->aoptions[d_o->no_of_dir_options][length + 2] = 't';
                  d_o->aoptions[d_o->no_of_dir_options][length + 3] = 'e';
                  d_o->aoptions[d_o->no_of_dir_options][length + 4] = 'r';
                  d_o->aoptions[d_o->no_of_dir_options][length + 5] = 'n';
                  d_o->aoptions[d_o->no_of_dir_options][length + 6] = 'a';
                  d_o->aoptions[d_o->no_of_dir_options][length + 7] = 'l';
                  length += 8;
               }
               else
               {
                  /* Minute. */
#ifdef HAVE_LONG_LONG
                  if (fra[i].te[j].minute == ALL_MINUTES)
#else
                  full_minute[0] = full_minute[1] = full_minute[2] = full_minute[3] = 0;
                  full_minute[4] = full_minute[5] = full_minute[6] = full_minute[7] = 0;
                  for (ii = 0; ii < 60; ii++)
                  {
                     bitset(full_minute, ii);
                  }
                  if (memcmp(fra[i].te[j].minute, full_minute, 8) == 0)
#endif
                  {
                     d_o->aoptions[d_o->no_of_dir_options][length] = '*';
                     d_o->aoptions[d_o->no_of_dir_options][length + 1] = ' ';
                     length += 2;
                  }
                  else
                  {
                     for (ii = 0; ii < 60; ii++)
                     {
#ifdef HAVE_LONG_LONG
                        if (fra[i].te[j].minute & bit_array_long[ii])
#else
                        if (bittest(fra[i].te[j].minute, ii) == YES)
#endif
                        {
                           if (d_o->aoptions[d_o->no_of_dir_options][length - 1] == ' ')
                           {
                              length += snprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                                 MAX_OPTION_LENGTH - length,
                                                 "%d", ii);
                           }
                           else
                           {
                              length += snprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                                 MAX_OPTION_LENGTH - length,
                                                 ",%d", ii);
                           }
                        }
                     }
                     d_o->aoptions[d_o->no_of_dir_options][length] = ' ';
                     length++;
                  }

                  /* Hour. */
                  if (fra[i].te[j].hour == ALL_HOURS)
                  {
                     d_o->aoptions[d_o->no_of_dir_options][length] = '*';
                     d_o->aoptions[d_o->no_of_dir_options][length + 1] = ' ';
                     length += 2;
                  }
                  else
                  {
                     for (ii = 0; ii < 24; ii++)
                     {
                        if (fra[i].te[j].hour & bit_array[ii])
                        {
                           if (d_o->aoptions[d_o->no_of_dir_options][length - 1] == ' ')
                           {
                              length += snprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                                 MAX_OPTION_LENGTH - length,
                                                 "%d", ii);
                           }
                           else
                           {
                              length += snprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                                 MAX_OPTION_LENGTH - length,
                                                 ",%d", ii);
                           }
                        }
                     }
                     d_o->aoptions[d_o->no_of_dir_options][length] = ' ';
                     length++;
                  }

                  /* Day of Month. */
                  if (fra[i].te[j].day_of_month == ALL_DAY_OF_MONTH)
                  {
                     d_o->aoptions[d_o->no_of_dir_options][length] = '*';
                     d_o->aoptions[d_o->no_of_dir_options][length + 1] = ' ';
                     length += 2;
                  }
                  else
                  {
                     for (ii = 1; ii < 32; ii++)
                     {
                        if (fra[i].te[j].day_of_month & bit_array[ii - 1])
                        {
                           if (d_o->aoptions[d_o->no_of_dir_options][length - 1] == ' ')
                           {
                              length += snprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                                 MAX_OPTION_LENGTH - length,
                                                 "%d", ii);
                           }
                           else
                           {
                              length += snprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                                 MAX_OPTION_LENGTH - length,
                                                 ",%d", ii);
                           }
                        }
                     }
                     d_o->aoptions[d_o->no_of_dir_options][length] = ' ';
                     length++;
                  }

                  /* Month. */
                  if (fra[i].te[j].month == ALL_MONTH)
                  {
                     d_o->aoptions[d_o->no_of_dir_options][length] = '*';
                     d_o->aoptions[d_o->no_of_dir_options][length + 1] = ' ';
                     length += 2;
                  }
                  else
                  {
                     for (ii = 1; ii < 13; ii++)
                     {
                        if (fra[i].te[j].month & bit_array[ii - 1])
                        {
                           if (d_o->aoptions[d_o->no_of_dir_options][length - 1] == ' ')
                           {
                              length += snprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                                 MAX_OPTION_LENGTH - length,
                                                 "%d", ii);
                           }
                           else
                           {
                              length += snprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                                 MAX_OPTION_LENGTH - length,
                                                 ",%d", ii);
                           }
                        }
                     }
                     d_o->aoptions[d_o->no_of_dir_options][length] = ' ';
                     length++;
                  }

                  /* Day of Week. */
                  if (fra[i].te[j].day_of_week == ALL_DAY_OF_WEEK)
                  {
                     d_o->aoptions[d_o->no_of_dir_options][length] = '*';
                     d_o->aoptions[d_o->no_of_dir_options][length + 1] = ' ';
                     length += 2;
                  }
                  else
                  {
                     for (ii = 1; ii < 8; ii++)
                     {
                        if (fra[i].te[j].day_of_week & bit_array[ii - 1])
                        {
                           if (d_o->aoptions[d_o->no_of_dir_options][length - 1] == ' ')
                           {
                              length += snprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                                 MAX_OPTION_LENGTH - length,
                                                 "%d", ii);
                           }
                           else
                           {
                              length += snprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                                 MAX_OPTION_LENGTH - length,
                                                 ",%d", ii);
                           }
                        }
                     }
                     d_o->aoptions[d_o->no_of_dir_options][length] = ' ';
                     length++;
                  }
               }
               d_o->aoptions[d_o->no_of_dir_options][length] = '\0';
               d_o->no_of_dir_options++;
               if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
               {
                  goto done;
               }
            } /* for (j = 0; j < fra[i].no_of_time_entries; j++) */
         }
#ifdef WITH_DUP_CHECK
         if (fra[i].dup_check_flag != 0)
         {
            int length;

            if (fra[i].dup_check_flag & DC_FILENAME_ONLY)
            {
               length = snprintf(d_o->aoptions[d_o->no_of_dir_options],
                                 MAX_OPTION_LENGTH,
# if SIZEOF_TIME_T == 4
                                 "%s %ld %d",
# else
                                 "%s %lld %d",
# endif
                                 DUPCHECK_ID,
                                 (pri_time_t)fra[i].dup_check_timeout,
                                 DC_FILENAME_ONLY_BIT);
            }
            else if (fra[i].dup_check_flag & DC_FILENAME_AND_SIZE)
                 {
                    length = snprintf(d_o->aoptions[d_o->no_of_dir_options],
                                      MAX_OPTION_LENGTH,
# if SIZEOF_TIME_T == 4
                                      "%s %ld %d",
# else
                                      "%s %lld %d",
# endif
                                      DUPCHECK_ID,
                                      (pri_time_t)fra[i].dup_check_timeout,
                                      DC_FILENAME_AND_SIZE_BIT);
                 }
            else if (fra[i].dup_check_flag & DC_NAME_NO_SUFFIX)
                 {
                    length = snprintf(d_o->aoptions[d_o->no_of_dir_options],
                                      MAX_OPTION_LENGTH,
# if SIZEOF_TIME_T == 4
                                      "%s %ld %d",
# else
                                      "%s %lld %d",
# endif
                                      DUPCHECK_ID,
                                      (pri_time_t)fra[i].dup_check_timeout,
                                      DC_NAME_NO_SUFFIX_BIT);
                 }
            else if (fra[i].dup_check_flag & DC_FILE_CONTENT)
                 {
                    length = snprintf(d_o->aoptions[d_o->no_of_dir_options],
                                      MAX_OPTION_LENGTH,
# if SIZEOF_TIME_T == 4
                                      "%s %ld %d",
# else
                                      "%s %lld %d",
# endif
                                      DUPCHECK_ID,
                                      (pri_time_t)fra[i].dup_check_timeout,
                                      DC_FILE_CONTENT_BIT);
                 }
                 else
                 {
                    length = snprintf(d_o->aoptions[d_o->no_of_dir_options],
                                      MAX_OPTION_LENGTH,
# if SIZEOF_TIME_T == 4
                                      "%s %ld %d",
# else
                                      "%s %lld %d",
# endif
                                      DUPCHECK_ID,
                                      (pri_time_t)fra[i].dup_check_timeout,
                                      DC_FILE_CONT_NAME_BIT);
                 }
            if (fra[i].dup_check_flag & DC_DELETE)
            {
               if (fra[i].dup_check_flag & DC_WARN)
               {
                  length += snprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                     MAX_OPTION_LENGTH - length,
                                     " %d", DC_DELETE_WARN_BIT);
               }
               else
               {
                  length += snprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                     MAX_OPTION_LENGTH - length,
                                     " %d", DC_DELETE_BIT);
               }
            }
            else if (fra[i].dup_check_flag & DC_STORE)
                 {
                    if (fra[i].dup_check_flag & DC_WARN)
                    {
                       length += snprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                          MAX_OPTION_LENGTH - length,
                                          " %d",
                                          DC_STORE_WARN_BIT);
                    }
                    else
                    {
                       length += snprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                          MAX_OPTION_LENGTH - length,
                                          " %d",
                                          DC_STORE_BIT);
                    }
                 }
                 else
                 {
                    length += snprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                       MAX_OPTION_LENGTH - length,
                                       " %d",
                                       DC_WARN_BIT);
                 }
            if (fra[i].dup_check_flag & DC_CRC32C)
            {
               length += snprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                  MAX_OPTION_LENGTH - length,
                                  " %d", DC_CRC32C_BIT);
            }
            else if (fra[i].dup_check_flag & DC_MURMUR3)
                 {
                    length += snprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                       MAX_OPTION_LENGTH - length,
                                       " %d", DC_MURMUR3_BIT);
                 }
                 else
                 {
                    length += snprintf(&d_o->aoptions[d_o->no_of_dir_options][length],
                                       MAX_OPTION_LENGTH - length,
                                       " %d", DC_CRC32_BIT);
                 }
            d_o->aoptions[d_o->no_of_dir_options][length] = '\0';
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
#endif /* WITH_DUP_CHECK */
         if (fra[i].in_dc_flag & LOCAL_REMOTE_DIR_IDC)
         {
            (void)snprintf(d_o->aoptions[d_o->no_of_dir_options],
                           LOCAL_REMOTE_DIR_ID_LENGTH + 1 + MAX_OPTION_LENGTH,
                           "%s %s",
                           LOCAL_REMOTE_DIR_ID, fra[i].retrieve_work_dir);
            d_o->no_of_dir_options++;
            if (d_o->no_of_dir_options >= MAX_NO_OPTIONS)
            {
               goto done;
            }
         }
         if ((fra[i].protocol == FTP) || (fra[i].protocol == HTTP) ||
             (fra[i].protocol == SFTP))
         {
            (void)strcpy(d_o->url, fra[i].url);
         }
         break;
      }
   }

done:

   if (attached == YES)
   {
      fra_detach();
   }

   return;
}
