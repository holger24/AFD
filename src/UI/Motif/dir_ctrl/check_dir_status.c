/*
 *  check_dir_status.c - Part of AFD, an automatic file distribution
 *                       program.
 *  Copyright (c) 2000 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_dir_status - checks the if status of each directory for
 **                      any change
 **
 ** SYNOPSIS
 **   void check_dir_status(Widget w)
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
 **   31.08.2000 H.Kiehl Created
 **   05.05.2002 H.Kiehl Show the number files currently in the directory.
 **
 */
DESCR__E_M3

#include <stdio.h>               /* sprintf()                            */
#include <string.h>              /* strcmp(), strcpy(), memcpy()         */
#include <stdlib.h>              /* calloc(), realloc(), free()          */
#include <time.h>                /* time()                               */
#include <math.h>                /* log10()                              */
#include <sys/times.h>           /* times()                              */
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <Xm/Xm.h>
#include "dir_ctrl.h"

/* External global variables. */
extern Display                    *display;
extern XtAppContext               app;
extern char                       line_style,
                                  *p_work_dir;
extern float                      max_bar_length;
extern int                        bar_thickness_3,
                                  glyph_width,
                                  no_of_columns,
                                  no_of_dirs,
                                  no_selected,
                                  redraw_time_line;
extern time_t                     now;
extern clock_t                    clktck;
extern struct dir_line            *connect_data;
extern struct fileretrieve_status *fra;
extern struct tms                 tmsdummy;

/* Local function prototypes. */
static int                        check_fra_data(char *),
                                  check_disp_data(char *, int);


/*######################### check_dir_status() ##########################*/
void
check_dir_status(Widget w)
{
   signed char   flush;
   int           i,
                 x,
                 y,
                 pos,
                 prev_no_of_dirs = no_of_dirs,
                 location_where_changed,
                 new_bar_length,
                 old_bar_length,
                 redo_warn_time_bar,
                 redraw_everything = NO;
   u_off_t       bytes_received;
   unsigned int  files_received,
                 prev_dir_flag;
   time_t        delta_time,
                 end_time;

   /* Initialise variables. */
   location_where_changed = no_of_dirs + 10;
   flush = NO;
   now = time(NULL);

   /*
    * See if a directory has been added or removed from the FRA.
    * If it changed resize the window.
    */
   if (check_fra(NO) == YES)
   {
      unsigned int    new_bar_length;
      size_t          new_size = no_of_dirs * sizeof(struct dir_line);
      struct dir_line *new_connect_data,
                      *tmp_connect_data;

      if ((new_connect_data = calloc(no_of_dirs,
                                     sizeof(struct dir_line))) == NULL)
      {
         (void)xrec(FATAL_DIALOG, "calloc() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }

      /*
       * First try to copy the connect data from the old structure
       * so long as the directories are the same.
       */
      for (i = 0, location_where_changed = 0;
           i < prev_no_of_dirs; i++, location_where_changed++)
      {
         if (strcmp(connect_data[i].dir_alias, fra[i].dir_alias) == 0)
         {
            (void)memcpy(&new_connect_data[i], &connect_data[i],
                         sizeof(struct dir_line));
         }
         else
         {
            break;
         }
      }

      for (i = location_where_changed; i < no_of_dirs; i++)
      {
         if ((pos = check_disp_data(fra[i].dir_alias,
                                    prev_no_of_dirs)) != INCORRECT)
         {
            (void)memcpy(&new_connect_data[i], &connect_data[pos],
                         sizeof(struct dir_line));
         }
         else /* A directory host has been added. */
         {
            /* Initialise values for new host. */
            (void)strcpy(new_connect_data[i].dir_alias, fra[i].dir_alias);
            (void)sprintf(new_connect_data[i].dir_display_str, "%-*s",
                          MAX_DIR_ALIAS_LENGTH, new_connect_data[i].dir_alias);
            new_connect_data[i].dir_status = fra[i].dir_status;
            new_connect_data[i].bytes_received = fra[i].bytes_received;
            new_connect_data[i].files_received = fra[i].files_received;
            new_connect_data[i].dir_flag = fra[i].dir_flag;
            new_connect_data[i].files_in_dir = fra[i].files_in_dir;
            new_connect_data[i].files_queued = fra[i].files_queued;
            new_connect_data[i].bytes_in_dir = fra[i].bytes_in_dir;
            new_connect_data[i].bytes_in_queue = fra[i].bytes_in_queue;
            new_connect_data[i].max_process = fra[i].max_process;
            new_connect_data[i].no_of_process = fra[i].no_of_process;
            new_connect_data[i].max_errors = fra[i].max_errors;
            new_connect_data[i].error_counter = fra[i].error_counter;
            CREATE_FC_STRING(new_connect_data[i].str_files_in_dir,
                             new_connect_data[i].files_in_dir);
            CREATE_FS_STRING(new_connect_data[i].str_bytes_in_dir,
                             new_connect_data[i].bytes_in_dir);
            CREATE_FC_STRING(new_connect_data[i].str_files_queued,
                             new_connect_data[i].files_queued);
            CREATE_FS_STRING(new_connect_data[i].str_bytes_queued,
                             new_connect_data[i].bytes_in_queue);
            CREATE_EC_STRING(new_connect_data[i].str_np,
                             new_connect_data[i].no_of_process);
            CREATE_EC_STRING(new_connect_data[i].str_ec,
                             new_connect_data[i].error_counter);
            new_connect_data[i].last_retrieval = fra[i].last_retrieval;
            new_connect_data[i].start_event_handle = fra[i].start_event_handle;
            new_connect_data[i].end_event_handle = fra[i].end_event_handle;
            if (*(unsigned char *)((char *)fra - AFD_FEATURE_FLAG_OFFSET_END) & DISABLE_DIR_WARN_TIME)
            {
               new_connect_data[i].warn_time = 0;
            }
            else
            {
               new_connect_data[i].warn_time = fra[i].warn_time;
            }
            new_connect_data[i].bytes_per_sec = 0;
            new_connect_data[i].prev_bytes_per_sec = 0;
            new_connect_data[i].str_tr[0] = new_connect_data[i].str_tr[1] = ' ';
            new_connect_data[i].str_tr[2] = '0';
            new_connect_data[i].str_tr[3] = 'B';
            new_connect_data[i].str_tr[4] = '\0';
            new_connect_data[i].average_tr = 0.0;
            new_connect_data[i].max_average_tr = 0.0;
            new_connect_data[i].files_per_sec = 0;
            new_connect_data[i].prev_files_per_sec = 0;
            new_connect_data[i].str_fr[0] = ' ';
            new_connect_data[i].str_fr[1] = '0';
            new_connect_data[i].str_fr[2] = '.';
            new_connect_data[i].str_fr[3] = '0';
            new_connect_data[i].str_fr[4] = '\0';
            new_connect_data[i].average_fr = 0.0;
            new_connect_data[i].max_average_fr = 0.0;
            new_connect_data[i].bar_length[BYTE_RATE_BAR_NO] = 0;
            if (new_connect_data[i].warn_time < 1)
            {
               new_connect_data[i].scale = 0.0;
               new_connect_data[i].bar_length[TIME_UP_BAR_NO] = 0;
            }
            else
            {
               new_connect_data[i].scale = max_bar_length / new_connect_data[i].warn_time;
               new_bar_length = (now - new_connect_data[i].last_retrieval) * new_connect_data[i].scale;
               if (new_bar_length > 0)
               {
                  if (new_bar_length >= max_bar_length)
                  {
                     new_connect_data[i].bar_length[TIME_UP_BAR_NO] = max_bar_length;
                  }
                  else
                  {
                     new_connect_data[i].bar_length[TIME_UP_BAR_NO] = new_bar_length;
                  }
               }
               else
               {
                  new_connect_data[i].bar_length[TIME_UP_BAR_NO] = 0;
               }
            }
            new_connect_data[i].bar_length[FILE_RATE_BAR_NO] = 0;
            new_connect_data[i].start_time = times(&tmsdummy);
            new_connect_data[i].inverse = OFF;
            new_connect_data[i].expose_flag = NO;

            /*
             * If this line has been selected in the old
             * connect_data structure, we have to make sure
             * that this host has not been deleted. If it
             * is deleted reduce the select counter!
             */
            if ((i < prev_no_of_dirs) && (connect_data[i].inverse == ON))
            {
               if ((pos = check_fra_data(connect_data[i].dir_alias)) == INCORRECT)
               {
                  /* Host has been deleted. */
                  no_selected--;
               }
            }
         }
      } /* for (i = location_where_changed; i < no_of_dirs; i++) */

      /*
       * Ensure that we really have checked all directories in old
       * structure.
       */
      if (prev_no_of_dirs > no_of_dirs)
      {
         while (i < prev_no_of_dirs)
         {
            if (connect_data[i].inverse == ON)
            {
               if ((pos = check_fra_data(connect_data[i].dir_alias)) == INCORRECT)
               {
                  /* Host has been deleted. */
                  no_selected--;
               }
            }
            i++;
         }
      }

      if ((tmp_connect_data = realloc(connect_data, new_size)) == NULL)
      {
         int tmp_errno = errno;

         free(connect_data);
         (void)xrec(FATAL_DIALOG, "realloc() error : %s (%s %d)",
                    strerror(tmp_errno), __FILE__, __LINE__);
         return;
      }
      connect_data = tmp_connect_data;

      /* Activate the new connect_data structure. */
      (void)memcpy(&connect_data[0], &new_connect_data[0],
                   no_of_dirs * sizeof(struct dir_line));

      free(new_connect_data);

      /* Resize window if necessary. */
      if ((redraw_everything = resize_dir_window()) == YES)
      {
         if (no_of_columns != 0)
         {
            location_where_changed = 0;
         }
      }

      /* When no. of directories have been reduced, then delete */
      /* removed directories from end of list.                  */
      for (i = prev_no_of_dirs; i > no_of_dirs; i--)
      {
         draw_dir_blank_line(i - 1);
      }

      /* Make sure changes are drawn!!! */
      flush = YES;
   } /* if (check_fra(NO) == YES) */

   /* Change information for each directory if necessary. */
   for (i = 0; i < no_of_dirs; i++)
   {
      x = y = -1;
      redo_warn_time_bar = NO;

      if (connect_data[i].dir_status != fra[i].dir_status)
      {
         connect_data[i].dir_status = fra[i].dir_status;

         locate_xy(i, &x, &y);
         draw_dir_identifier(i, x, y);
         flush = YES;
      }

      if (connect_data[i].max_process != fra[i].max_process)
      {
         connect_data[i].max_process = fra[i].max_process;
      }

      if (connect_data[i].dir_flag != fra[i].dir_flag)
      {
         prev_dir_flag = connect_data[i].dir_flag;
         connect_data[i].dir_flag = fra[i].dir_flag;
         if (x == -1)
         {
            locate_xy(i, &x, &y);
         }
         if (((prev_dir_flag & MAX_COPIED) == 0) &&
             (connect_data[i].dir_flag & MAX_COPIED))
         {
            draw_dir_full_marker(i, x, y, YES);
         }
         else if ((prev_dir_flag & MAX_COPIED) &&
                  ((connect_data[i].dir_flag & MAX_COPIED) == 0))
              {
                 draw_dir_full_marker(i, x, y, NO);
              }

         flush = YES;
      }

      if (connect_data[i].max_errors != fra[i].max_errors)
      {
         connect_data[i].max_errors = fra[i].max_errors;
      }

      if (*(unsigned char *)((char *)fra - AFD_FEATURE_FLAG_OFFSET_END) & DISABLE_DIR_WARN_TIME)
      {
         if (connect_data[i].warn_time != 0)
         {
            connect_data[i].scale = 0.0;
            connect_data[i].warn_time = 0;
            redo_warn_time_bar = YES;
         }
      }
      else
      {
         if (connect_data[i].warn_time != fra[i].warn_time)
         {
            connect_data[i].warn_time = fra[i].warn_time;
            if (connect_data[i].warn_time < 1)
            {
               connect_data[i].scale = 0.0;
            }
            else
            {
               connect_data[i].scale = max_bar_length / connect_data[i].warn_time;
            }
            redo_warn_time_bar = YES;
         }
      }

      end_time = times(&tmsdummy);
      if ((delta_time = (end_time - connect_data[i].start_time)) == 0)
      {
         delta_time = 1;
      }
      connect_data[i].start_time = end_time;

      /*
       * Byte Rate Bar
       */
      bytes_received = 0;
      if (connect_data[i].bytes_received != fra[i].bytes_received)
      {
         if (fra[i].bytes_received < connect_data[i].bytes_received)
         {
            bytes_received = fra[i].bytes_received;
         }
         else
         {
            bytes_received = fra[i].bytes_received -
                             connect_data[i].bytes_received;
         }
         connect_data[i].bytes_received = fra[i].bytes_received;
      }
      if (bytes_received > 0)
      {
         connect_data[i].bytes_per_sec = (float)(bytes_received) /
                                         delta_time * clktck;

         if (line_style != CHARACTERS_ONLY)
         {
            /* Arithmetischer Mittelwert. */
            connect_data[i].average_tr = (connect_data[i].average_tr +
                                         connect_data[i].bytes_per_sec) / 2.0;
            if (connect_data[i].average_tr > connect_data[i].max_average_tr)
            {
               connect_data[i].max_average_tr = connect_data[i].average_tr;
            }
         }
      }
      else
      {
         connect_data[i].bytes_per_sec = 0;
         if ((line_style != CHARACTERS_ONLY) &&
             (connect_data[i].average_tr > 0.0))
         {
            /* Arithmetischer Mittelwert. */
            connect_data[i].average_tr = (connect_data[i].average_tr +
                                         connect_data[i].bytes_per_sec) / 2.0;
            if (connect_data[i].average_tr > connect_data[i].max_average_tr)
            {      
               connect_data[i].max_average_tr = connect_data[i].average_tr;
            }
         }
      }
      files_received = 0;
      if (connect_data[i].files_received != fra[i].files_received)
      {
         if (fra[i].files_received < connect_data[i].files_received)
         {
            files_received = fra[i].files_received;
         }
         else
         {
            files_received = fra[i].files_received -
                             connect_data[i].files_received;
         }
         connect_data[i].files_received = fra[i].files_received;
      }
      if (files_received > 0)
      {
         connect_data[i].files_per_sec = (float)(files_received) /
                                         delta_time * clktck;

         if (line_style != CHARACTERS_ONLY)
         {
            /* Arithmetischer Mittelwert. */
            connect_data[i].average_fr = (connect_data[i].average_fr +
                                         connect_data[i].files_per_sec) / 2.0;
            if (connect_data[i].average_fr > connect_data[i].max_average_fr)
            {
               connect_data[i].max_average_fr = connect_data[i].average_fr;
            }
         }
      }
      else
      {
         connect_data[i].files_per_sec = 0;
         if ((line_style != CHARACTERS_ONLY) &&
             (connect_data[i].average_fr > 0.0))
         {
            /* Arithmetischer Mittelwert. */
            connect_data[i].average_fr = (connect_data[i].average_fr +
                                         connect_data[i].files_per_sec) / 2.0;
            if (connect_data[i].average_fr > connect_data[i].max_average_fr)
            {      
               connect_data[i].max_average_fr = connect_data[i].average_fr;
            }
         }
      }

      /*
       * CHARACTER INFORMATION
       * =====================
       *
       * If in character mode see if any change took place,
       * if so, redraw only those characters.
       */
      if (line_style != BARS_ONLY)
      {
         /*
          * Number of files in directory.
          */
         if (connect_data[i].files_in_dir != fra[i].files_in_dir)
         {
            connect_data[i].files_in_dir = fra[i].files_in_dir;
            if (x == -1)
            {
               locate_xy(i, &x, &y);
            }
            CREATE_FC_STRING(connect_data[i].str_files_in_dir,
                             connect_data[i].files_in_dir);
            if (i < location_where_changed)
            {
               draw_dir_chars(i, FILES_IN_DIR, x, y);
               flush = YES;
            }
         }

         /*
          * Number of bytes in directory.
          */
         if (connect_data[i].bytes_in_dir != fra[i].bytes_in_dir)
         {
            connect_data[i].bytes_in_dir = fra[i].bytes_in_dir;
            if (x == -1)
            {
               locate_xy(i, &x, &y);
            }
            CREATE_FS_STRING(connect_data[i].str_bytes_in_dir,
                             connect_data[i].bytes_in_dir);
            if (i < location_where_changed)
            {
               draw_dir_chars(i, BYTES_IN_DIR, x, y);
               flush = YES;
            }
         }

         /*
          * Number of files queued.
          */
         if (connect_data[i].files_queued != fra[i].files_queued)
         {
            connect_data[i].files_queued = fra[i].files_queued;
            if (x == -1)
            {
               locate_xy(i, &x, &y);
            }
            CREATE_FC_STRING(connect_data[i].str_files_queued,
                             connect_data[i].files_queued);
            if (i < location_where_changed)
            {
               draw_dir_chars(i, FILES_QUEUED, x, y);
               flush = YES;
            }
         }

         /*
          * Number of bytes queued.
          */
         if (connect_data[i].bytes_in_queue != fra[i].bytes_in_queue)
         {
            connect_data[i].bytes_in_queue = fra[i].bytes_in_queue;
            if (x == -1)
            {
               locate_xy(i, &x, &y);
            }
            CREATE_FS_STRING(connect_data[i].str_bytes_queued,
                             connect_data[i].bytes_in_queue);
            if (i < location_where_changed)
            {
               draw_dir_chars(i, BYTES_QUEUED, x, y);
               flush = YES;
            }
         }

         /*
          * Number of process for this directory.
          */
         if (connect_data[i].no_of_process != fra[i].no_of_process)
         {
            connect_data[i].no_of_process = fra[i].no_of_process;
            if (x == -1)
            {
               locate_xy(i, &x, &y);
            }
            CREATE_EC_STRING(connect_data[i].str_np,
                             connect_data[i].no_of_process);
            if (i < location_where_changed)
            {
               draw_dir_chars(i, NO_OF_DIR_PROCESS, x, y);
               flush = YES;
            }
         }

         /*
          * Byte rate.
          */
         if (connect_data[i].bytes_per_sec != connect_data[i].prev_bytes_per_sec)
         {
            connect_data[i].prev_bytes_per_sec = connect_data[i].bytes_per_sec;
            if (x == -1)
            {
               locate_xy(i, &x, &y);
            }
            CREATE_FS_STRING(connect_data[i].str_tr,
                             connect_data[i].bytes_per_sec);
            if (i < location_where_changed)
            {
               draw_dir_chars(i, BYTE_RATE, x, y);
               flush = YES;
            }
         }

         /*
          * File rate.
          */
         if (connect_data[i].files_per_sec != connect_data[i].prev_files_per_sec)
         {
            connect_data[i].prev_files_per_sec = connect_data[i].files_per_sec;
            if (x == -1)
            {
               locate_xy(i, &x, &y);
            }
            CREATE_FR_STRING(connect_data[i].str_fr,
                             connect_data[i].files_per_sec);
            if (i < location_where_changed)
            {
               draw_dir_chars(i, FILE_RATE, x, y);
               flush = YES;
            }
         }

         /*
          * Error Counter.
          */
         if (connect_data[i].error_counter != fra[i].error_counter)
         {
            connect_data[i].error_counter = fra[i].error_counter;
            if (x == -1)
            {
               locate_xy(i, &x, &y);
            }
            CREATE_EC_STRING(connect_data[i].str_ec,
                             connect_data[i].error_counter);
            if (i < location_where_changed)
            {
               draw_dir_chars(i, DIR_ERRORS, x, y);
               flush = YES;
            }
         }
      } /* if (line_style != BARS_ONLY) */

      /*
       * BAR INFORMATION
       * ===============
       */
      if (line_style != CHARACTERS_ONLY)
      {
         if (connect_data[i].last_retrieval != fra[i].last_retrieval)
         {
            connect_data[i].last_retrieval = fra[i].last_retrieval;
         }

         if (connect_data[i].average_tr > 1.0)
         {
            if (connect_data[i].max_average_tr < 2.0)
            {
               new_bar_length = (log10(connect_data[i].average_tr) *
                                 max_bar_length /
                                 log10((double) 2.0));
            }
            else
            {
               new_bar_length = (log10(connect_data[i].average_tr) *
                                 max_bar_length /
                                 log10(connect_data[i].max_average_tr));
            }
         }
         else
         {
            new_bar_length = 0;
         }
         if ((connect_data[i].bar_length[BYTE_RATE_BAR_NO] != new_bar_length) &&
             (new_bar_length < max_bar_length))
         {
            old_bar_length = connect_data[i].bar_length[BYTE_RATE_BAR_NO];
            connect_data[i].bar_length[BYTE_RATE_BAR_NO] = new_bar_length;

            if (i < location_where_changed)
            {
               if (x == -1)
               {
                  locate_xy(i, &x, &y);
               }

               if (old_bar_length < new_bar_length)
               {
                  draw_dir_bar(i, 1, BYTE_RATE_BAR_NO, x, y);
               }
               else
               {
                  draw_dir_bar(i, -1, BYTE_RATE_BAR_NO, x, y);
               }

               if (flush != YES)
               {
                  flush = YUP;
               }
            }
         }
         else if ((new_bar_length >= max_bar_length) &&
                  (connect_data[i].bar_length[BYTE_RATE_BAR_NO] < max_bar_length))
              {
                 connect_data[i].bar_length[BYTE_RATE_BAR_NO] = max_bar_length;
                 if (i < location_where_changed)
                 {
                    if (x == -1)
                    {
                       locate_xy(i, &x, &y);
                    }

                    draw_dir_bar(i, 1, BYTE_RATE_BAR_NO, x, y);

                    if (flush != YES)
                    {
                       flush = YUP;
                    }
                 }
              }

         if (((connect_data[i].warn_time > 0) &&
              (connect_data[i].scale > 0.0)) ||
             (redo_warn_time_bar == YES))
         {
            new_bar_length = (now - connect_data[i].last_retrieval) * connect_data[i].scale;
            if (new_bar_length > 0)
            {
               if (new_bar_length >= max_bar_length)
               {
                  new_bar_length = max_bar_length;
               }
            }
            else
            {
               new_bar_length = 0;
            }
            if (new_bar_length != connect_data[i].bar_length[TIME_UP_BAR_NO])
            {
               old_bar_length = connect_data[i].bar_length[TIME_UP_BAR_NO];
               connect_data[i].bar_length[TIME_UP_BAR_NO] = new_bar_length;
               if (i < location_where_changed)
               {
                  if (x == -1)
                  {
                     locate_xy(i, &x, &y);
                  }

                  if (old_bar_length < new_bar_length)
                  {
                     draw_dir_bar(i, 1, TIME_UP_BAR_NO, x, y + bar_thickness_3);
                  }
                  else
                  {
                     draw_dir_bar(i, -1, TIME_UP_BAR_NO, x, y + bar_thickness_3);
                  }

                  if (flush != YES)
                  {
                     flush = YUP;
                  }
               }
            }
         }

         /*
          * File Rate Bar
          */
         if (connect_data[i].average_fr > 1.0)
         {
            if (connect_data[i].max_average_fr < 2.0)
            {
               new_bar_length = (log10(connect_data[i].average_fr) *
                                 max_bar_length /
                                 log10((double) 2.0));
            }
            else
            {
               new_bar_length = (log10(connect_data[i].average_fr) *
                                 max_bar_length /
                                 log10(connect_data[i].max_average_fr));
            }
         }
         else
         {
            new_bar_length = 0;
         }
         if ((connect_data[i].bar_length[FILE_RATE_BAR_NO] != new_bar_length) &&
             (new_bar_length < max_bar_length))
         {
            old_bar_length = connect_data[i].bar_length[FILE_RATE_BAR_NO];
            connect_data[i].bar_length[FILE_RATE_BAR_NO] = new_bar_length;

            if (i < location_where_changed)
            {
               if (x == -1)
               {
                  locate_xy(i, &x, &y);
               }

               if (old_bar_length < new_bar_length)
               {
                  draw_dir_bar(i, 1, FILE_RATE_BAR_NO, x, y + bar_thickness_3 + bar_thickness_3);
               }
               else
               {
                  draw_dir_bar(i, -1, FILE_RATE_BAR_NO, x, y + bar_thickness_3 + bar_thickness_3);
               }

               if (flush != YES)
               {
                  flush = YUP;
               }
            }
         }
         else if ((new_bar_length >= max_bar_length) &&
                  (connect_data[i].bar_length[FILE_RATE_BAR_NO] < max_bar_length))
              {
                 connect_data[i].bar_length[FILE_RATE_BAR_NO] = max_bar_length;
                 if (i < location_where_changed)
                 {
                    if (x == -1)
                    {
                       locate_xy(i, &x, &y);
                    }

                    draw_dir_bar(i, 1, FILE_RATE_BAR_NO, x, y + bar_thickness_3 + bar_thickness_3);

                    if (flush != YES)
                    {
                       flush = YUP;
                    }
                 }
              }
      }

      /* Redraw the line. */
      if (i >= location_where_changed)
      {
#ifdef _DEBUG
         (void)fprintf(stderr, "count_channels: i = %d\n", i);
#endif
         flush = YES;
         draw_dir_line_status(i, 1);
      }
   }

   if (redraw_everything == YES)
   {
      redraw_all();
      flush = YES;
   }

   /* Make sure all changes are shown. */
   if ((flush == YES) || (flush == YUP))
   {
      XFlush(display);

      if (flush != YUP)
      {
         redraw_time_line = MIN_DIR_REDRAW_TIME;
      }
   }
   else
   {
      if (redraw_time_line < MAX_DIR_REDRAW_TIME)
      {
         redraw_time_line += DIR_REDRAW_STEP_TIME;
      }
#ifdef _DEBUG
      (void)fprintf(stderr, "count_channels: Redraw time = %d\n", redraw_time_line);
#endif
   }

   /* Redraw every redraw_time_line ms. */
   (void)XtAppAddTimeOut(app, redraw_time_line,
                         (XtTimerCallbackProc)check_dir_status, w);
 
   return;
}


/*+++++++++++++++++++++++++++ check_fra_data() ++++++++++++++++++++++++++*/
static int
check_fra_data(char *dir_alias)
{
   register int i;

   for (i = 0; i < no_of_dirs; i++)
   {
      if (strcmp(fra[i].dir_alias, dir_alias) == 0)
      {
         return(i);
      }
   }

   return(INCORRECT);
}


/*++++++++++++++++++++++++++ check_disp_data() ++++++++++++++++++++++++++*/
static int
check_disp_data(char *dir_alias, int prev_no_of_dirs)
{
   register int i;

   for (i = 0; i < prev_no_of_dirs; i++)
   {
      if (strcmp(connect_data[i].dir_alias, dir_alias) == 0)
      {
         return(i);
      }
   }

   return(INCORRECT);
}
