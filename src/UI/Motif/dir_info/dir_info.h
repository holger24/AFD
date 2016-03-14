/*
 *  dir_info.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2016 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __dir_info_h
#define __dir_info_h

#include "motif_common_defs.h"

#define MAX_DIR_INFO_STRING_LENGTH 60
#define DIR_INFO_TEXT_WIDTH_L      15
#define DIR_INFO_TEXT_WIDTH_R      18
#define DIR_INFO_LENGTH_L          20
#define DIR_INFO_LENGTH_R          20

/* Definitions for the left labels. */
#define ALIAS_DIR_NAME_POS         0
#define STUPID_MODE_POS            1
#define FORCE_REREAD_POS           2
#define ACCUMULATE_POS             3
#define DELETE_UNKNOWN_POS         4
#define DELETE_QUEUED_POS          5
#define IGNORE_FILE_TIME_POS       6
#define END_CHARACTER_POS          7
#define BYTES_RECEIVED_POS         8
#define LAST_RETRIEVAL_POS         9

/* Definitions for the right labels. */
#define DIRECTORY_ID_POS           0
#define REMOVE_FILES_POS           1
#define WAIT_FOR_FILENAME_POS      2
#define ACCUMULATE_SIZE_POS        3
#define REPORT_UNKNOWN_FILES_POS   4
#define DELETE_LOCKED_FILES_POS    5
#define IGNORE_SIZE_POS            6
#define MAX_COPIED_FILES_POS       7
#define FILES_RECEIVED_POS         8
#define NEXT_CHECK_TIME_POS        9

#define NO_OF_LABELS_PER_ROW       10


#define UPDATE_INTERVAL            1000
#define FILE_UPDATE_INTERVAL       4

struct prev_values
       {
          char          real_dir_name[MAX_PATH_LENGTH];
          char          host_alias[MAX_HOSTNAME_LENGTH + 1];
          char          dir_alias[MAX_DIR_ALIAS_LENGTH + 1];
          char          display_url[MAX_RECIPIENT_LENGTH];
          char          url[MAX_RECIPIENT_LENGTH];
          char          wait_for_filename[MAX_WAIT_FOR_LENGTH];
          u_off_t       bytes_received;
          off_t         ignore_size;
          off_t         accumulate_size;
          time_t        last_retrieval;
          time_t        next_check_time;
#ifdef WITH_DUP_CHECK
          time_t        dup_check_timeout;
          unsigned int  dup_check_flag;
#endif
          unsigned int  dir_id;
          unsigned int  accumulate;
          unsigned int  ignore_file_time;
          unsigned int  gt_lt_sign;
          unsigned int  files_received;
          unsigned int  max_copied_files;
          int           dir_pos;
          int           unknown_file_time;
          int           queued_file_time;
          int           locked_file_time;
          int           end_character;
          unsigned char no_of_time_entries;
          unsigned char delete_files_flag;
          unsigned char stupid_mode;
          unsigned char remove;
          char          force_reread;
          unsigned char report_unknown_files;
       };

/* Function prototypes. */
extern void close_button(Widget, XtPointer, XtPointer),
            save_button(Widget, XtPointer, XtPointer),
            update_info(Widget);

#endif /* __dir_info_h */
