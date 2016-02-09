/*
 *  check_tv_status.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_tv_status - checks the status of the detailed transfer
 **                     view window
 **
 ** SYNOPSIS
 **   void check_tv_status(Widget w)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   07.01.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>        /* memset(), strncmp(), strncpy(), strlen()   */
#include <X11/Xlib.h>
#include "gafd_ctrl.h"

extern Display                    *display;
extern Window                     detailed_window;
extern XtAppContext               app;
extern XtIntervalId               interval_id_tv;
extern char                       line_style;
extern float                      max_bar_length;
extern int                        filename_display_length,
                                  glyph_width,
                                  no_of_jobs_selected,
                                  bar_thickness_3,
                                  x_offset_proc;
extern struct job_data            *jd;
extern struct filetransfer_status *fsa;


/*########################## check_tv_status() ##########################*/
void
check_tv_status(Widget w)
{
   unsigned char new_color;
   signed char   flush;
   int           i,
                 x,
                 y,
                 new_bar_length;
   static int    redraw_time_tv = MIN_REDRAW_TIME;

   /* Initialise variables. */
   flush = NO;

   /* Change information for each remote host if necessary. */
   for (i = 0; i < no_of_jobs_selected; i++)
   {
      x = y = -1;

      /*
       * HOST NAME
       * =========
       */
      if (jd[i].special_flag != fsa[jd[i].fsa_no].special_flag)
      {
         jd[i].special_flag = fsa[jd[i].fsa_no].special_flag;

         tv_locate_xy(i, &x, &y);
         draw_tv_dest_identifier(i, x, y);
         flush = YES;
      }

      /* Did any significant change occur in the status */
      /* for this host?                                 */
      if (fsa[jd[i].fsa_no].special_flag & HOST_DISABLED)
      {
         new_color = WHITE;
      }
      else if ((fsa[jd[i].fsa_no].special_flag & HOST_IN_DIR_CONFIG) == 0)
           {
              new_color = DEFAULT_BG;
           }
      else if (fsa[jd[i].fsa_no].error_counter >= fsa[jd[i].fsa_no].max_errors)
           {
              new_color = NOT_WORKING2;
           }
      else if (fsa[jd[i].fsa_no].job_status[jd[i].job_no].no_of_files > 0)
           {
              new_color = TRANSFER_ACTIVE; /* Transferring files. */
           }
           else
           {
              new_color = NORMAL_STATUS; /* Nothing to do but connection active. */
           }

      if (jd[i].stat_color_no != new_color)
      {
         jd[i].stat_color_no = new_color;

         if (x == -1)
         {
            tv_locate_xy(i, &x, &y);
         }
         draw_tv_dest_identifier(i, x, y);
         flush = YES;
      }

      /*
       * JOB NUMBER
       * ==========
       */
      if (jd[i].connect_status != fsa[jd[i].fsa_no].job_status[jd[i].job_no].connect_status)
      {
         if (x == -1)
         {
            tv_locate_xy(i, &x, &y);
         }

         jd[i].connect_status = fsa[jd[i].fsa_no].job_status[jd[i].job_no].connect_status;
         draw_tv_job_number(i, x, y);
         flush = YES;
      }

      /*
       * FILE NAME
       * =========
       */
      if (fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_name_in_use[0] == '\0')
      {
          if (jd[i].file_name_in_use[0] != ' ')
          {
             if (x == -1)
             {
                tv_locate_xy(i, &x, &y);
             }
             jd[i].filename_compare_length = 0;
             (void)memset(jd[i].file_name_in_use, ' ',
                          MAX_FILENAME_LENGTH - 1);
             draw_file_name(i, x, y);
             flush = YES;
          }
      }
      else
      {
         jd[i].filename_compare_length = strlen(fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_name_in_use);
         if (jd[i].filename_compare_length > filename_display_length)
         {
            jd[i].filename_compare_length = filename_display_length;
         }
         if (strncmp(jd[i].file_name_in_use,
                     fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_name_in_use,
                     jd[i].filename_compare_length) != 0)
         {
            if (x == -1)
            {
               tv_locate_xy(i, &x, &y);
            }

            (void)my_strncpy(jd[i].file_name_in_use,
                             fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_name_in_use,
                             jd[i].filename_compare_length + 1);
            if (jd[i].filename_compare_length < filename_display_length)
            {
               (void)memset(jd[i].file_name_in_use + jd[i].filename_compare_length, ' ',
                            filename_display_length - jd[i].filename_compare_length);
            }
            draw_file_name(i, x, y);
            flush = YES;
         }
      }

      /*
       * ROTATING DASH
       * =============
       */
#ifdef _WITH_MAP_SUPPORT
      if (fsa[jd[i].fsa_no].job_status[jd[i].job_no].connect_status != MAP_ACTIVE)
      {
#endif
         if (jd[i].file_size_in_use_done != fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_size_in_use_done)
         {
            if (x == -1)
            {
               tv_locate_xy(i, &x, &y);
            }
            jd[i].rotate++;
            if (fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_size_in_use_done == fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_size_in_use)
            {
               jd[i].rotate = -2;
            }
            draw_rotating_dash(i, x, y);
         }
#ifdef _WITH_MAP_SUPPORT
      }
#endif

      /*
       * CHARACTER INFORMATION
       * =====================
       *
       * If in character mode see if any change took place,
       * if so, redraw only those characters.
       */
      if (line_style & SHOW_CHARACTERS)
      {
         /*
          * File size in use.
          */
         if (jd[i].file_size_in_use != fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_size_in_use)
         {
            char tmp_string[5];

            if (x == -1)
            {
               tv_locate_xy(i, &x, &y);
            }

            if ((line_style & SHOW_BARS) == 0)
            {
               jd[i].file_size_in_use = fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_size_in_use;
            }
            CREATE_FS_STRING(tmp_string, fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_size_in_use);

            if ((tmp_string[2] != jd[i].str_fs_use[2]) ||
                (tmp_string[1] != jd[i].str_fs_use[1]) ||
                (tmp_string[0] != jd[i].str_fs_use[0]) ||
                (tmp_string[3] != jd[i].str_fs_use[3]))
            {
               jd[i].str_fs_use[0] = tmp_string[0];
               jd[i].str_fs_use[1] = tmp_string[1];
               jd[i].str_fs_use[2] = tmp_string[2];
               jd[i].str_fs_use[3] = tmp_string[3];
               draw_tv_chars(i, FILE_SIZE_IN_USE, x, y);
               flush = YES;
            }
         }

         /*
          * File size in use done.
          */
         if (jd[i].file_size_in_use_done != fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_size_in_use_done)
         {
            char tmp_string[5];

            if (x == -1)
            {
               tv_locate_xy(i, &x, &y);
            }

            if ((line_style & SHOW_BARS) == 0)
            {
               jd[i].file_size_in_use_done = fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_size_in_use_done;
            }
            CREATE_FS_STRING(tmp_string, fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_size_in_use_done);

            if ((tmp_string[2] != jd[i].str_fs_use_done[2]) ||
                (tmp_string[1] != jd[i].str_fs_use_done[1]) ||
                (tmp_string[0] != jd[i].str_fs_use_done[0]) ||
                (tmp_string[3] != jd[i].str_fs_use_done[3]))
            {
               jd[i].str_fs_use_done[0] = tmp_string[0];
               jd[i].str_fs_use_done[1] = tmp_string[1];
               jd[i].str_fs_use_done[2] = tmp_string[2];
               jd[i].str_fs_use_done[3] = tmp_string[3];
               draw_tv_chars(i, FILE_SIZE_IN_USE_DONE, x, y);
               flush = YES;
            }
         }

         /*
          * Number of files.
          */
         if (jd[i].no_of_files != fsa[jd[i].fsa_no].job_status[jd[i].job_no].no_of_files)
         {
            if (x == -1)
            {
               tv_locate_xy(i, &x, &y);
            }

            if ((line_style & SHOW_BARS) == 0)
            {
               jd[i].no_of_files = fsa[jd[i].fsa_no].job_status[jd[i].job_no].no_of_files;
            }
            CREATE_FC_STRING(jd[i].str_fc, fsa[jd[i].fsa_no].job_status[jd[i].job_no].no_of_files);

            draw_tv_chars(i, NUMBER_OF_FILES, x, y);
            flush = YES;
         }

         /*
          * Number of files done.
          */
         if (jd[i].no_of_files_done != fsa[jd[i].fsa_no].job_status[jd[i].job_no].no_of_files_done)
         {
            if (x == -1)
            {
               tv_locate_xy(i, &x, &y);
            }

            if ((line_style & SHOW_BARS) == 0)
            {
               jd[i].no_of_files_done = fsa[jd[i].fsa_no].job_status[jd[i].job_no].no_of_files_done;
            }
            CREATE_FC_STRING(jd[i].str_fc_done, fsa[jd[i].fsa_no].job_status[jd[i].job_no].no_of_files_done);

            draw_tv_chars(i, NUMBER_OF_FILES_DONE, x, y);
            flush = YES;
         }

         /*
          * File size.
          */
         if (jd[i].file_size != fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_size)
         {
            char tmp_string[5];

            if (x == -1)
            {
               tv_locate_xy(i, &x, &y);
            }

            if ((line_style & SHOW_BARS) == 0)
            {
               jd[i].file_size = fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_size;
            }
            CREATE_FS_STRING(tmp_string, fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_size);

            if ((tmp_string[2] != jd[i].str_fs[2]) ||
                (tmp_string[1] != jd[i].str_fs[1]) ||
                (tmp_string[0] != jd[i].str_fs[0]) ||
                (tmp_string[3] != jd[i].str_fs[3]))
            {
               jd[i].str_fs[0] = tmp_string[0];
               jd[i].str_fs[1] = tmp_string[1];
               jd[i].str_fs[2] = tmp_string[2];
               jd[i].str_fs[3] = tmp_string[3];
               draw_tv_chars(i, FILE_SIZE, x, y);
               flush = YES;
            }
         }

         /*
          * File size done.
          */
         if (jd[i].file_size_done != fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_size_done)
         {
            char tmp_string[5];

            if (x == -1)
            {
               tv_locate_xy(i, &x, &y);
            }

            if ((line_style & SHOW_BARS) == 0)
            {
               jd[i].file_size_done = fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_size_done;
            }
            CREATE_FS_STRING(tmp_string, fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_size_done);

            if ((tmp_string[2] != jd[i].str_fs_done[2]) ||
                (tmp_string[1] != jd[i].str_fs_done[1]) ||
                (tmp_string[0] != jd[i].str_fs_done[0]) ||
                (tmp_string[3] != jd[i].str_fs_done[3]))
            {
               jd[i].str_fs_done[0] = tmp_string[0];
               jd[i].str_fs_done[1] = tmp_string[1];
               jd[i].str_fs_done[2] = tmp_string[2];
               jd[i].str_fs_done[3] = tmp_string[3];
               draw_tv_chars(i, FILE_SIZE_DONE, x, y);
               flush = YES;
            }
         }
      } /* if (line_style != BARS_ONLY) */

      /*
       * BAR INFORMATION
       * ===============
       */
      if (line_style & SHOW_BARS)
      {
         /*
          * File size in use done bar.
          */
         if (jd[i].file_size_in_use != fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_size_in_use)
         {
            jd[i].file_size_in_use = fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_size_in_use;
            if (jd[i].file_size_in_use == 0)
            {
               jd[i].scale[CURRENT_FILE_SIZE_BAR_NO] = 1.0;
            }
            else
            {
               jd[i].scale[CURRENT_FILE_SIZE_BAR_NO] = max_bar_length / jd[i].file_size_in_use;
            }
         }

         if (jd[i].file_size_in_use_done != fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_size_in_use_done)
         {
            jd[i].file_size_in_use_done = fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_size_in_use_done;
            if (jd[i].file_size_in_use_done == 0)
            {
               new_bar_length = 0;
            }
            else if (jd[i].file_size_in_use_done >= jd[i].file_size_in_use)
                 {
                    new_bar_length = max_bar_length;
                 }
                 else
                 {
                    new_bar_length = jd[i].file_size_in_use_done * jd[i].scale[CURRENT_FILE_SIZE_BAR_NO];
                 }
            if (jd[i].bar_length[CURRENT_FILE_SIZE_BAR_NO] != new_bar_length)
            {
               if (x == -1)
               {
                  tv_locate_xy(i, &x, &y);
               }

               if (jd[i].bar_length[CURRENT_FILE_SIZE_BAR_NO] < new_bar_length)
               {
                  jd[i].bar_length[CURRENT_FILE_SIZE_BAR_NO] = new_bar_length;
                  draw_tv_bar(i, 1, CURRENT_FILE_SIZE_BAR_NO, x, y);
               }
               else
               {
                  jd[i].bar_length[CURRENT_FILE_SIZE_BAR_NO] = new_bar_length;
                  draw_tv_bar(i, -1, CURRENT_FILE_SIZE_BAR_NO, x, y);
               }
               flush = YES;
            }
         }

         /*
          * Number of files done bar.
          */
         if (jd[i].no_of_files != fsa[jd[i].fsa_no].job_status[jd[i].job_no].no_of_files)
         {
            jd[i].no_of_files = fsa[jd[i].fsa_no].job_status[jd[i].job_no].no_of_files;
            if (jd[i].no_of_files == 0)
            {
               jd[i].scale[NO_OF_FILES_DONE_BAR_NO] = 1.0;
            }
            else
            {
               jd[i].scale[NO_OF_FILES_DONE_BAR_NO] = max_bar_length / jd[i].no_of_files;
            }
         }

         if (jd[i].no_of_files_done != fsa[jd[i].fsa_no].job_status[jd[i].job_no].no_of_files_done)
         {
            jd[i].no_of_files_done = fsa[jd[i].fsa_no].job_status[jd[i].job_no].no_of_files_done;
            if (jd[i].no_of_files_done == 0)
            {
               new_bar_length = 0;
            }
            else if (jd[i].no_of_files_done >= jd[i].no_of_files)
                 {
                    new_bar_length = max_bar_length;
                 }
                 else
                 {
                    new_bar_length = jd[i].no_of_files_done * jd[i].scale[NO_OF_FILES_DONE_BAR_NO];
                 }
            if (jd[i].bar_length[NO_OF_FILES_DONE_BAR_NO] != new_bar_length)
            {
               if (x == -1)
               {
                  tv_locate_xy(i, &x, &y);
               }

               if (jd[i].bar_length[NO_OF_FILES_DONE_BAR_NO] < new_bar_length)
               {
                  jd[i].bar_length[NO_OF_FILES_DONE_BAR_NO] = new_bar_length;
                  draw_tv_bar(i, 1, NO_OF_FILES_DONE_BAR_NO,
                              x, y + bar_thickness_3);
               }
               else
               {
                  jd[i].bar_length[NO_OF_FILES_DONE_BAR_NO] = new_bar_length;
                  draw_tv_bar(i, -1, NO_OF_FILES_DONE_BAR_NO,
                              x, y + bar_thickness_3);
               }
               flush = YES;
            }
         }

         /*
          * File size done bar.
          */
         if (jd[i].file_size != fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_size)
         {
            jd[i].file_size = fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_size;
            if (jd[i].file_size == 0)
            {
               jd[i].scale[FILE_SIZE_DONE_BAR_NO] = 1.0;
            }
            else
            {
               jd[i].scale[FILE_SIZE_DONE_BAR_NO] = max_bar_length / jd[i].file_size;
            }
         }

         if (jd[i].file_size_done != fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_size_done)
         {
            jd[i].file_size_done = fsa[jd[i].fsa_no].job_status[jd[i].job_no].file_size_done;
            if (jd[i].file_size_done == 0)
            {
               new_bar_length = 0;
            }
            else if (jd[i].file_size_done >= jd[i].file_size)
                 {
                    new_bar_length = max_bar_length;
                 }
                 else
                 {
                    new_bar_length = jd[i].file_size_done * jd[i].scale[FILE_SIZE_DONE_BAR_NO];
                 }
            if (jd[i].bar_length[FILE_SIZE_DONE_BAR_NO] != new_bar_length)
            {
               if (x == -1)
               {
                  tv_locate_xy(i, &x, &y);
               }

               if (jd[i].bar_length[FILE_SIZE_DONE_BAR_NO] < new_bar_length)
               {
                  jd[i].bar_length[FILE_SIZE_DONE_BAR_NO] = new_bar_length;
                  draw_tv_bar(i, 1, FILE_SIZE_DONE_BAR_NO,
                              x, y + bar_thickness_3 + bar_thickness_3);
               }
               else
               {
                  jd[i].bar_length[FILE_SIZE_DONE_BAR_NO] = new_bar_length;
                  draw_tv_bar(i, -1, FILE_SIZE_DONE_BAR_NO,
                              x, y + bar_thickness_3 + bar_thickness_3);
               }
               flush = YES;
            }
         }
      } /* if (line_style & SHOW_BARS) */
   } /* for (i = 0; i < no_of_jobs_selected; i++) */

   /* Make sure all changes are shown. */
   if ((flush == YES) || (flush == YUP))
   {
      XFlush(display);

      if (flush != YUP)
      {
         redraw_time_tv = MIN_TV_REDRAW_TIME;
      }
   }
   else
   {
      if (redraw_time_tv < MAX_TV_REDRAW_TIME)
      {
         redraw_time_tv += TV_REDRAW_STEP_TIME;
      }
#ifdef _DEBUG
      (void)fprintf(stderr, "count_channels: Redraw time = %d\n",
                    redraw_time_tv);
#endif
   }

   /* Redraw every redraw_time_host ms. */
   interval_id_tv = XtAppAddTimeOut(app, redraw_time_tv,
                                    (XtTimerCallbackProc)check_tv_status, w);

   return;
}
