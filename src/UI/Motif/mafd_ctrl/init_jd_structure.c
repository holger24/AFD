/*
 *  init_jd_structure.c - Part of AFD, an automatic file distribution
 *                        program.
 *  Copyright (c) 1998 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   init_jd_structure - initialises the job_data structure with values
 **
 ** SYNOPSIS
 **   void init_jd_structure(struct job_data *p_jd, int select_no, int job_no)
 **
 ** DESCRIPTION
 **   The function init_jd_structure() fills the job_data structure with
 **   data from the FSA and line structure.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.01.1998 H.Kiehl Created
 **   05.03.2010 H.Kiehl Get priority so we can show it.
 **
 */
DESCR__E_M3

#include <string.h>
#include <stdlib.h>
#include "mafd_ctrl.h"

/* External global variables. */
extern Widget                     appshell;
extern int                        filename_display_length;
extern float                      max_bar_length;
extern struct line                *connect_data;
extern struct filetransfer_status *fsa;


/*######################## init_jd_structure() ##########################*/
void
init_jd_structure(struct job_data *p_jd, int select_no, int job_no)
{
   (void)strcpy(p_jd->hostname, connect_data[select_no].hostname);
   p_jd->host_id = connect_data[select_no].host_id;
   (void)strcpy(p_jd->host_display_str,
                connect_data[select_no].host_display_str);
   p_jd->job_id = fsa[select_no].job_status[job_no].job_id;
   p_jd->priority[0] = (char)get_job_priority(p_jd->job_id);
   p_jd->priority[1] = '\0';
   p_jd->file_name_in_use[MAX_FILENAME_LENGTH] = '\0';
   if (fsa[select_no].job_status[job_no].file_name_in_use[0] == '\0')
   {
      (void)memset(p_jd->file_name_in_use, ' ', MAX_FILENAME_LENGTH);
      p_jd->filename_compare_length = 0;
   }
   else
   {
      (void)my_strncpy(p_jd->file_name_in_use,
                       fsa[select_no].job_status[job_no].file_name_in_use,
                       filename_display_length + 1);
      p_jd->filename_compare_length = strlen(p_jd->file_name_in_use);
      if (p_jd->filename_compare_length < filename_display_length)
      {
         (void)memset(p_jd->file_name_in_use + p_jd->filename_compare_length, ' ',
                      filename_display_length - p_jd->filename_compare_length);
      }
   }

   if (fsa[select_no].special_flag & HOST_DISABLED)
   {
      p_jd->stat_color_no = WHITE;
   }
   else if ((fsa[select_no].special_flag & HOST_IN_DIR_CONFIG) == 0)
        {
           p_jd->stat_color_no = DEFAULT_BG;
        }
   else if (fsa[select_no].error_counter >= fsa[select_no].max_errors)
        {
           p_jd->stat_color_no = NOT_WORKING2;
        }
   else if (fsa[select_no].job_status[job_no].no_of_files > 0)
        {
           p_jd->stat_color_no = TRANSFER_ACTIVE;
        }
        else
        {
           p_jd->stat_color_no = NORMAL_STATUS;
        }
   p_jd->special_flag = fsa[select_no].special_flag;

   p_jd->file_size_in_use = fsa[select_no].job_status[job_no].file_size_in_use;
   CREATE_FS_STRING(p_jd->str_fs_use, p_jd->file_size_in_use);
   p_jd->file_size_in_use_done = fsa[select_no].job_status[job_no].file_size_in_use_done;
   CREATE_FS_STRING(p_jd->str_fs_use_done, p_jd->file_size_in_use_done);
   p_jd->no_of_files = fsa[select_no].job_status[job_no].no_of_files;
   CREATE_FC_STRING(p_jd->str_fc, p_jd->no_of_files);
   p_jd->no_of_files_done = fsa[select_no].job_status[job_no].no_of_files_done;
   CREATE_FC_STRING(p_jd->str_fc_done, p_jd->no_of_files_done);
   p_jd->file_size = fsa[select_no].job_status[job_no].file_size;
   CREATE_FS_STRING(p_jd->str_fs, p_jd->file_size);
   p_jd->file_size_done = fsa[select_no].job_status[job_no].file_size_done;
   CREATE_FS_STRING(p_jd->str_fs_done, p_jd->file_size_done);

   if (p_jd->file_size_in_use == 0)
   {
      p_jd->scale[CURRENT_FILE_SIZE_BAR_NO] = 1.0;
      p_jd->rotate = -2;
   }
   else
   {
      p_jd->scale[CURRENT_FILE_SIZE_BAR_NO] = max_bar_length / p_jd->file_size_in_use;
      p_jd->rotate = -1;
   }
   if (p_jd->no_of_files == 0)
   {
      p_jd->scale[NO_OF_FILES_DONE_BAR_NO] = 1.0;
   }
   else
   {
      p_jd->scale[NO_OF_FILES_DONE_BAR_NO] = max_bar_length / p_jd->no_of_files;
   }
   if (p_jd->file_size == 0)
   {
      p_jd->scale[FILE_SIZE_DONE_BAR_NO] = 1.0;
   }
   else
   {
      p_jd->scale[FILE_SIZE_DONE_BAR_NO] = max_bar_length / p_jd->file_size;
   }
   p_jd->bar_length[CURRENT_FILE_SIZE_BAR_NO] = p_jd->file_size_in_use_done * p_jd->scale[CURRENT_FILE_SIZE_BAR_NO];
   p_jd->bar_length[NO_OF_FILES_DONE_BAR_NO] = p_jd->no_of_files_done * p_jd->scale[NO_OF_FILES_DONE_BAR_NO];
   p_jd->bar_length[FILE_SIZE_DONE_BAR_NO] = p_jd->file_size_done * p_jd->scale[FILE_SIZE_DONE_BAR_NO];

   p_jd->expose_flag = NO;
   p_jd->connect_status = fsa[select_no].job_status[job_no].connect_status;
   p_jd->job_no = job_no;
   p_jd->fsa_no = select_no;

   return;
}
