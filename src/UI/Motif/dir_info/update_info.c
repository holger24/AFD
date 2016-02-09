/*
 *  update_info.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2013 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   update_info - updates any information that changes for module
 **                 dir_info
 **
 ** SYNOPSIS
 **   void update_info(Widget w)
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
 **   05.08.2000 H.Kiehl Created
 **   20.07.2001 H.Kiehl Show if unknown and/or queued input files are
 **                      to be deleted.
 **   22.05.2002 H.Kiehl Separate old file times for unknown and queued files.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>           /* strcmp()                                */
#include <stdlib.h>           /* exit()                                  */
#include <time.h>             /* strftime(), localtime()                 */
#include <Xm/Xm.h>
#include <Xm/Text.h>
#include "mafd_ctrl.h"
#include "dir_info.h"

/* External global variables. */
extern int                        fra_pos,
                                  no_of_dirs,
                                  view_passwd;
extern char                       label_l[NO_OF_LABELS_PER_ROW][22],
                                  label_r[NO_OF_LABELS_PER_ROW][22],
#ifdef WITH_DUP_CHECK
                                  dupcheck_label_str[],
#endif
                                  *p_work_dir;
extern Display                    *display;
extern XtIntervalId               interval_id_dir;
extern XtAppContext               app;
extern Widget                     dirname_text_w,
#ifdef WITH_DUP_CHECK
                                  dup_check_w,
#endif
                                  label_l_widget[],
                                  label_r_widget[],
                                  text_wl[],
                                  text_wr[],
                                  url_text_w;
extern struct fileretrieve_status *fra;
extern struct prev_values         prev;


/*############################ update_info() ############################*/
void
update_info(Widget w)
{
   signed char flush = NO;
   char        str_line[MAX_PATH_LENGTH + 1],
               tmp_str_line[MAX_PATH_LENGTH + 1];

   /* Check if FRA changed. */
   if (check_fra(NO) == YES)
   {
      int i;

      fra_pos = -1;
      for (i = 0; i < no_of_dirs; i++)
      {
         if (strcmp(fra[i].dir_alias, prev.dir_alias) == 0)
         {
            fra_pos = i;
            break;
         }
      }
      if (fra_pos == -1)
      {
         (void)xrec(FATAL_DIALOG,
                    "Hmmm, looks like dir alias %s is gone. Terminating! (%s %d)",
                    prev.dir_alias, __FILE__, __LINE__);
         return;
      }
   }

   if (fra[fra_pos].host_alias[0] != '\0')
   {
      if (strcmp(prev.url, fra[fra_pos].url) != 0)
      {
         (void)strcpy(prev.url, fra[fra_pos].url);
         (void)strcpy(prev.display_url, prev.url);
         if (view_passwd == YES)
         {
            insert_passwd(prev.display_url);
         }
         (void)sprintf(str_line, "%*s",
                       MAX_DIR_INFO_STRING_LENGTH, prev.display_url);
         XmTextSetString(url_text_w, str_line);
         flush = YES;
      }
   }

   /*
    * Right side.
    */
   if (prev.stupid_mode != fra[fra_pos].stupid_mode)
   {
      prev.stupid_mode = fra[fra_pos].stupid_mode;
      if (prev.stupid_mode == YES)
      {
         (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, "Yes");
      }
      else if (prev.stupid_mode == GET_ONCE_ONLY)
           {
              (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, "Once only");
           }
           else
           {
              (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, "No");
           }
      XmTextSetString(text_wl[STUPID_MODE_POS], str_line);
      flush = YES;
   }
   if (prev.force_reread != fra[fra_pos].force_reread)
   {
      prev.force_reread = fra[fra_pos].force_reread;
      if (prev.force_reread == YES)
      {
         (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, "Yes");
      }
      else
      {
         (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, "No");
      }
      XmTextSetString(text_wl[FORCE_REREAD_POS], str_line);
      flush = YES;
   }
   if (prev.accumulate != fra[fra_pos].accumulate)
   {
      prev.accumulate = fra[fra_pos].accumulate;
      if (prev.accumulate == 0)
      {
         (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, "Not set");
      }
      else
      {
         (void)sprintf(str_line, "%*u", DIR_INFO_LENGTH_L, prev.accumulate);
      }
      XmTextSetString(text_wl[ACCUMULATE_POS], str_line);
      flush = YES;
   }
   if (prev.delete_files_flag != fra[fra_pos].delete_files_flag)
   {
      prev.delete_files_flag = fra[fra_pos].delete_files_flag;
      if ((prev.delete_files_flag & UNKNOWN_FILES) == 0)
      {
         (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, "Not set");
      }
      else
      {
         (void)sprintf(str_line, "%*d", DIR_INFO_LENGTH_L,
                       prev.unknown_file_time / 3600);
      }
      XmTextSetString(text_wl[DELETE_UNKNOWN_POS], str_line);
      if ((prev.delete_files_flag & QUEUED_FILES) == 0)
      {
         (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, "Not set");
      }
      else
      {
         (void)sprintf(str_line, "%*d", DIR_INFO_LENGTH_L,
                       prev.queued_file_time / 3600);
      }
      XmTextSetString(text_wl[DELETE_QUEUED_POS], str_line);
      if ((prev.delete_files_flag & OLD_LOCKED_FILES) == 0)
      {
         (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, "Not set");
      }
      else
      {
         (void)sprintf(str_line, "%*d", DIR_INFO_LENGTH_R,
                       prev.locked_file_time / 3600);
      }
      XmTextSetString(text_wr[DELETE_LOCKED_FILES_POS], str_line);
      flush = YES;
   }
   if (prev.ignore_file_time != fra[fra_pos].ignore_file_time)
   {
      prev.ignore_file_time = fra[fra_pos].ignore_file_time;
      if (prev.ignore_file_time == 0)
      {
         (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, "Not set");
      }
      else
      {
         char sign_char,
              str_value[MAX_INT_LENGTH + 1];

         if (prev.gt_lt_sign & IFTIME_LESS_THEN)
         {
            sign_char = '<';
         }
         else if (prev.gt_lt_sign & IFTIME_GREATER_THEN)
              {
                 sign_char = '>';
              }
              else
              {
                 sign_char = ' ';
              }
         (void)sprintf(str_value, "%c%u", sign_char, prev.ignore_file_time);
         (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, str_value);
      }
      XmTextSetString(text_wl[IGNORE_FILE_TIME_POS], str_line);
      flush = YES;
   }
   if (prev.end_character != fra[fra_pos].end_character)
   {
      prev.end_character = fra[fra_pos].end_character;
      if (prev.end_character == -1)
      {
         (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, "Not set");
      }
      else
      {
         (void)sprintf(str_line, "%*d", DIR_INFO_LENGTH_L, prev.end_character);
      }
      XmTextSetString(text_wl[END_CHARACTER_POS], str_line);
      flush = YES;
   }
   if (prev.bytes_received != fra[fra_pos].bytes_received)
   {
      prev.bytes_received = fra[fra_pos].bytes_received;
#if SIZEOF_OFF_T == 4
      (void)sprintf(str_line, "%*lu",
#else
      (void)sprintf(str_line, "%*llu",
#endif
                    DIR_INFO_LENGTH_L, (pri_off_t)prev.bytes_received);
      XmTextSetString(text_wl[BYTES_RECEIVED_POS], str_line);
      flush = YES;
   }
   if (prev.last_retrieval != fra[fra_pos].last_retrieval)
   {
      prev.last_retrieval = fra[fra_pos].last_retrieval;
      (void)strftime(tmp_str_line, MAX_DIR_INFO_STRING_LENGTH, "%d.%m.%Y  %H:%M:%S",
                     localtime(&prev.last_retrieval));
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, tmp_str_line);
      XmTextSetString(text_wl[LAST_RETRIEVAL_POS], str_line);
      flush = YES;
   }

   /*
    * Left side.
    */
   if (prev.dir_id != fra[fra_pos].dir_id)
   {
      prev.dir_id = fra[fra_pos].dir_id;
      (void)sprintf(str_line, "%*x", DIR_INFO_LENGTH_R, prev.dir_id);
      XmTextSetString(text_wr[DIRECTORY_ID_POS], str_line);
      flush = YES;
   }
   if (prev.remove != fra[fra_pos].remove)
   {
      prev.remove = fra[fra_pos].remove;
      if (prev.remove == YES)
      {
         (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, "Yes");
      }
      else
      {
         (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, "No");
      }
      XmTextSetString(text_wr[REMOVE_FILES_POS], str_line);
      flush = YES;
   }
   if (CHECK_STRCMP(prev.wait_for_filename, fra[fra_pos].wait_for_filename))
   {
      if (prev.wait_for_filename[0] == '\0')
      {
         (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, "Not set");
         prev.wait_for_filename[0] = '\0';
      }
      else
      {
         (void)sprintf(str_line, "%*s",
                       DIR_INFO_LENGTH_R, prev.wait_for_filename);
         (void)strcpy(prev.wait_for_filename, fra[fra_pos].wait_for_filename);
      }
      XmTextSetString(text_wr[WAIT_FOR_FILENAME_POS], str_line);
      flush = YES;
   }
   if (prev.accumulate_size != fra[fra_pos].accumulate_size)
   {
      prev.accumulate_size = fra[fra_pos].accumulate_size;
      if (prev.accumulate_size == 0)
      {
         (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, "Not set");
      }
      else
      {
#if SIZEOF_OFF_T == 4
         (void)sprintf(str_line, "%*ld",
#else
         (void)sprintf(str_line, "%*lld",
#endif
                       DIR_INFO_LENGTH_R, (pri_off_t)prev.accumulate_size);
      }
      XmTextSetString(text_wr[ACCUMULATE_SIZE_POS], str_line);
      flush = YES;
   }
   if (prev.report_unknown_files != fra[fra_pos].report_unknown_files)
   {
      prev.report_unknown_files = fra[fra_pos].report_unknown_files;
      if (prev.report_unknown_files == YES)
      {
         (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, "Yes");
      }
      else
      {
         (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, "No");
      }
      XmTextSetString(text_wr[REPORT_UNKNOWN_FILES_POS], str_line);
      flush = YES;
   }
   if (prev.ignore_size != fra[fra_pos].ignore_size)
   {
      prev.ignore_size = fra[fra_pos].ignore_size;
      if (prev.ignore_size == -1)
      {
         (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, "Not set");
      }
      else
      {
         char sign_char,
              str_value[MAX_INT_LENGTH + MAX_INT_LENGTH + 1];

         if (prev.gt_lt_sign & ISIZE_LESS_THEN)
         {
            sign_char = '<';
         }
         else if (prev.gt_lt_sign & ISIZE_GREATER_THEN)
              {
                 sign_char = '>';
              }
              else
              {
                 sign_char = ' ';
              }
#if SIZEOF_OFF_T == 4
         (void)sprintf(str_value, "%c%ld",
#else
         (void)sprintf(str_value, "%c%lld",
#endif
                       sign_char, (pri_off_t)prev.ignore_size);
         (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, str_value);
      }
      XmTextSetString(text_wr[IGNORE_SIZE_POS], str_line);
      flush = YES;
   }
   if (prev.max_copied_files != fra[fra_pos].max_copied_files)
   {
      prev.max_copied_files = fra[fra_pos].max_copied_files;
      (void)sprintf(str_line, "%*u", DIR_INFO_LENGTH_R, prev.max_copied_files);
      XmTextSetString(text_wr[MAX_COPIED_FILES_POS], str_line);
      flush = YES;
   }
   if (prev.files_received != fra[fra_pos].files_received)
   {
      prev.files_received = fra[fra_pos].files_received;
      (void)sprintf(str_line, "%*u", DIR_INFO_LENGTH_R, prev.files_received);
      XmTextSetString(text_wr[FILES_RECEIVED_POS], str_line);
      flush = YES;
   }
   if (prev.no_of_time_entries != fra[fra_pos].no_of_time_entries)
   {
      if (prev.no_of_time_entries > 0)
      {
         (void)strftime(tmp_str_line, MAX_DIR_INFO_STRING_LENGTH,
                        "%d.%m.%Y  %H:%M:%S",
                        localtime(&prev.next_check_time));
         (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, tmp_str_line);
      }
      else
      {
         (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, "No time entry.");
      }
      XmTextSetString(text_wr[NEXT_CHECK_TIME_POS], str_line);
      prev.no_of_time_entries = fra[fra_pos].no_of_time_entries;
   }
   else if ((prev.no_of_time_entries > 0) &&
            (prev.next_check_time != fra[fra_pos].next_check_time))
        {
           prev.next_check_time = fra[fra_pos].next_check_time;
           (void)strftime(tmp_str_line, MAX_DIR_INFO_STRING_LENGTH,
                          "%d.%m.%Y  %H:%M:%S",
                          localtime(&prev.next_check_time));
           (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, tmp_str_line);
           XmTextSetString(text_wr[NEXT_CHECK_TIME_POS], str_line);
           flush = YES;
        }

#ifdef WITH_DUP_CHECK
   if ((prev.dup_check_flag != fra[fra_pos].dup_check_flag) ||
       (prev.dup_check_timeout != fra[fra_pos].dup_check_timeout))
   {
      size_t   length;
      XmString text;

      prev.dup_check_flag = fra[fra_pos].dup_check_flag;
      prev.dup_check_timeout = fra[fra_pos].dup_check_timeout;

      if (prev.dup_check_flag == 0)
      {
         length = sprintf(dupcheck_label_str, "Duplicate check : Not set.");
      }
      else
      {
         length = sprintf(dupcheck_label_str, "Duplicate check : ");
         if (prev.dup_check_flag & DC_FILENAME_ONLY)
         {
            length += sprintf(&dupcheck_label_str[length], "Filename");
         }
         else if (prev.dup_check_flag & DC_FILE_CONTENT)
              {
                 length += sprintf(&dupcheck_label_str[length], "File content");
              }
         else if (prev.dup_check_flag & DC_FILE_CONT_NAME)
              {
                 length += sprintf(&dupcheck_label_str[length],
                                   "File content and name");
              }
         else if (prev.dup_check_flag & DC_NAME_NO_SUFFIX)
              {
                 length += sprintf(&dupcheck_label_str[length],
                                   "Filename no suffix");
              }
         else if (prev.dup_check_flag & DC_FILENAME_AND_SIZE)
              {
                 length += sprintf(&dupcheck_label_str[length],
                                   "Filename and size");
              }
              else
              {
                 length += sprintf(&dupcheck_label_str[length], "Unknown");
              }
         if (prev.dup_check_flag & DC_CRC32)
         {
            length += sprintf(&dupcheck_label_str[length], ", CRC32");
         }
         else if (prev.dup_check_flag & DC_CRC32C)
              {
                 length += sprintf(&dupcheck_label_str[length], ", CRC32C");
              }
              else
              {
                 length += sprintf(&dupcheck_label_str[length], "Unknown");
              }
         if ((prev.dup_check_flag & DC_DELETE) ||
             (prev.dup_check_flag & DC_STORE))
         {
            if (prev.dup_check_flag & DC_DELETE)
            {
               length += sprintf(&dupcheck_label_str[length], ", Delete");
            }
            else
            {
               length += sprintf(&dupcheck_label_str[length], ", Store");
            }
            if (prev.dup_check_flag & DC_WARN)
            {
               length += sprintf(&dupcheck_label_str[length], " + Warn");
            }
         }
         else
         {
            if (prev.dup_check_flag & DC_WARN)
            {
               length += sprintf(&dupcheck_label_str[length], ", Warn");
            }
         }
         if (prev.dup_check_timeout != 0)
         {
#if SIZEOF_TIME_T == 4
            length += sprintf(&dupcheck_label_str[length], ", timeout=%ld",
#else
            length += sprintf(&dupcheck_label_str[length], ", timeout=%lld",
#endif
                              (pri_time_t)prev.dup_check_timeout);
         }
      }
      text = XmStringCreateLocalized(dupcheck_label_str);
      XtVaSetValues(dup_check_w, XmNlabelString, text, NULL);
      XmStringFree(text);
      flush = YES;
   }
#endif /* WITH_DUP_CHECK */

   if (flush == YES)
   {
      XFlush(display);
   }

   /* Call update_info() after UPDATE_INTERVAL ms. */
   interval_id_dir = XtAppAddTimeOut(app, UPDATE_INTERVAL,
                                     (XtTimerCallbackProc)update_info,
                                     NULL);

   return;
}
