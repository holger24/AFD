/*
 *  check_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_log - Checks if there is any new data to be displayed
 **
 ** SYNOPSIS
 **   void check_log(Widget w)
 **
 ** DESCRIPTION
 **   The function check_log() always checks 'p_log_file' for any
 **   new data to be displayed.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.03.1996 H.Kiehl Created
 **   31.05.1997 H.Kiehl Added debug toggle.
 **   22.11.2003 H.Kiehl Do not show each individual line, buffer the
 **                      information and then show in one block.
 **   27.12.2003 H.Kiehl Added trace toggle.
 **   26.03.2004 H.Kiehl Handle implementations with sticky EOF behaviour.
 **   20.09.2022 H.Kiehl Replace unprintable characters with dot (.) sign.
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>                     /* Definition of AT_* constants */
#endif
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <errno.h>
#include "mshow_log.h"
#include "logdefs.h"

extern Display        *display;
extern XtAppContext   app;
extern XtIntervalId   interval_id_host;
extern XmTextPosition wpr_position;
extern Cursor         cursor1,
                      cursor2;
extern Widget         appshell,
                      counterbox,
                      log_scroll_bar;
extern int            current_log_number,
                      line_counter,
                      log_type_flag,
                      no_of_hosts;
extern XT_PTR_TYPE    toggles_set;
extern off_t          max_logfile_size,
                      total_length;
extern unsigned int   toggles_set_parallel_jobs;
extern ino_t          current_inode_no;
extern char           log_dir[MAX_PATH_LENGTH],
                      log_name[MAX_FILENAME_LENGTH],
                      **hosts;
extern FILE           *p_log_file;
static int            first_time = YES;

/* Local function prototype. */
static void           display_data(Widget, int *, int *, int *,
                                   unsigned int *, char *);

#define MAX_LINES_IN_ONE_GO 2000


/*############################# check_log() #############################*/
void
check_log(Widget w)
{
#ifdef _SLOW_COUNTER
   static int    old_line_counter = 0;
#endif
   static size_t incomplete_line_length = 0;
   static char   *incomplete_line = NULL;
   int           locked = 0,
                 lock_counter = 1,
                 cursor_counter = 1;
   char          str_line[MAX_LINE_COUNTER_DIGITS + 1];

   if (p_log_file != NULL)
   {
      int          i,
                   max_lines = 0;
      size_t       length,
                   line_length = MAX_LINE_LENGTH + 1;
      unsigned int chars_buffered = 0;
      char         *line = NULL,
                   *line_buffer;

      if (((line_buffer = malloc(MAX_LINE_LENGTH * MAX_LINES_IN_ONE_GO)) == NULL) ||
          ((line = malloc(MAX_LINE_LENGTH + 1)) == NULL))
      {
         (void)xrec(FATAL_DIALOG, "malloc() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }
      if (no_of_hosts > 0)
      {
#ifdef HAVE_GETLINE
         while ((length = getline(&line, &line_length, p_log_file)) != -1)
#else
         while (fgets(line, line_length, p_log_file) != NULL)
#endif
         {
#ifndef HAVE_GETLINE
            length = strlen(line);
#endif
            /* Check if line contains unprintable characters. */
            i = 0;
            while (i < length)
            {
               if ((line[i] < ' ') && (line[i] != '\n'))
               {
                  line[i] = '.';
               }
               i++;
            }

            if ((length > 0) && (line[length - 1] == '\n'))
            {
               if (incomplete_line != NULL)
               {
                  char *tmp_buffer = NULL;

                  if ((tmp_buffer = malloc(length + 1)) == NULL)
                  {
                     (void)xrec(FATAL_DIALOG, "malloc() error : %s (%s %d)",
                                strerror(errno), __FILE__, __LINE__);
                     return;
                  }
                  memcpy(tmp_buffer, line, length + 1);
                  if ((line = realloc(line, (line_length + incomplete_line_length))) == NULL)
                  {
                     (void)xrec(FATAL_DIALOG, "realloc() error : %s (%s %d)",
                                strerror(errno), __FILE__, __LINE__);
                     return;
                  }
                  line_length += incomplete_line_length;
                  memcpy(line, incomplete_line, incomplete_line_length);
                  free(incomplete_line);
                  incomplete_line = NULL;
                  memcpy(&line[incomplete_line_length], tmp_buffer, length + 1);
                  free(tmp_buffer);
                  length += incomplete_line_length;
                  incomplete_line_length = 0;
               }
               total_length += length;
               if ((log_type_flag == TRANSFER_LOG_TYPE) ||
                   (log_type_flag == TRANS_DB_LOG_TYPE))
               {
                  if ((length > (LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4)) &&
                      ((((toggles_set & SHOW_INFO) == 0) && (line[LOG_SIGN_POSITION] == 'I')) ||
                       (((toggles_set & SHOW_WARN) == 0) && (line[LOG_SIGN_POSITION] == 'W')) ||
                       (((toggles_set & SHOW_ERROR) == 0) && (line[LOG_SIGN_POSITION] == 'E')) ||
                       (((toggles_set & SHOW_FATAL) == 0) && (line[LOG_SIGN_POSITION] == 'F')) ||
                       (((toggles_set & SHOW_OFFLINE) == 0) && (line[LOG_SIGN_POSITION] == 'O')) ||
                       (((toggles_set & SHOW_DEBUG) == 0) && (line[LOG_SIGN_POSITION] == 'D')) ||
                       (((toggles_set & SHOW_TRACE) == 0) && (line[LOG_SIGN_POSITION] == 'T')) ||
                       (((toggles_set_parallel_jobs - 1) != (line[LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4] - 48)) &&
                       (toggles_set_parallel_jobs != 0))))
                  {
                     continue;
                  }
               }
               else
               {
                  if ((length > LOG_SIGN_POSITION) &&
                      ((((toggles_set & SHOW_INFO) == 0) && (line[LOG_SIGN_POSITION] == 'I')) ||
                       (((toggles_set & SHOW_CONFIG) == 0) && (line[LOG_SIGN_POSITION] == 'C')) ||
                       (((toggles_set & SHOW_WARN) == 0) && (line[LOG_SIGN_POSITION] == 'W')) ||
                       (((toggles_set & SHOW_ERROR) == 0) && (line[LOG_SIGN_POSITION] == 'E')) ||
                       (((toggles_set & SHOW_OFFLINE) == 0) && (line[LOG_SIGN_POSITION] == 'O')) ||
                       (((toggles_set & SHOW_FATAL) == 0) && (line[LOG_SIGN_POSITION] == 'F')) ||
                       (((toggles_set & SHOW_DEBUG) == 0) && (line[LOG_SIGN_POSITION] == 'D'))))
                  {
                     continue;
                  }
               }

               for (i = 0; i < no_of_hosts; i++)
               {
                  if (log_filter(hosts[i], &line[16]) == 0)
                  {
                     memcpy(&line_buffer[chars_buffered], line, length);
                     chars_buffered += length;
                     line_counter++;
                     max_lines++;

                     break;
                  }
               }
               if (max_lines >= MAX_LINES_IN_ONE_GO)
               {
                  max_lines = 0;
                  display_data(w, &lock_counter, &cursor_counter, &locked,
                               &chars_buffered, line_buffer);
               }
            }
            else
            {
               if ((incomplete_line = realloc(incomplete_line,
                                              (incomplete_line_length + length))) == NULL)
               {
                  (void)xrec(FATAL_DIALOG, "realloc() error : %s (%s %d)",
                             strerror(errno), __FILE__, __LINE__);
                  return;
               }
               (void)memcpy(&incomplete_line[incomplete_line_length],
                            line, length);
               incomplete_line_length += length;
            }
         }
      }
      else /* We are searching for ALL hosts. */
      {

#ifdef HAVE_GETLINE
         while ((length = getline(&line, &line_length, p_log_file)) != -1)
#else
         while (fgets(line, line_length, p_log_file) != NULL)
#endif
         {
#ifndef HAVE_GETLINE
            length = strlen(line);
#endif
            /* Check if line contains unprintable characters. */
            i = 0;
            while (i < length)
            {
               if ((line[i] < ' ') && (line[i] != '\n'))
               {
                  line[i] = '.';
               }
               i++;
            }

            if ((length > 0) && (line[length - 1] == '\n'))
            {
               if (incomplete_line != NULL)
               {
                  char *tmp_buffer = NULL;

                  if ((tmp_buffer = malloc(length + 1)) == NULL)
                  {
                     (void)xrec(FATAL_DIALOG, "malloc() error : %s (%s %d)",
                                strerror(errno), __FILE__, __LINE__);
                     return;
                  }
                  memcpy(tmp_buffer, line, length + 1);
                  if ((line = realloc(line, (line_length + incomplete_line_length))) == NULL)
                  {
                     (void)xrec(FATAL_DIALOG, "realloc() error : %s (%s %d)",
                                strerror(errno), __FILE__, __LINE__);
                     return;
                  }
                  line_length += incomplete_line_length;
                  memcpy(line, incomplete_line, incomplete_line_length);
                  free(incomplete_line);
                  incomplete_line = NULL;
                  memcpy(&line[incomplete_line_length], tmp_buffer, length + 1);
                  free(tmp_buffer);
                  length += incomplete_line_length;
                  incomplete_line_length = 0;
               }
               total_length += length;
               if ((log_type_flag == TRANSFER_LOG_TYPE) ||
                   (log_type_flag == TRANS_DB_LOG_TYPE))
               {
                  if ((length > (LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4)) &&
                      ((((toggles_set & SHOW_INFO) == 0) && (line[LOG_SIGN_POSITION] == 'I')) ||
                       (((toggles_set & SHOW_WARN) == 0) && (line[LOG_SIGN_POSITION] == 'W')) ||
                       (((toggles_set & SHOW_ERROR) == 0) && (line[LOG_SIGN_POSITION] == 'E')) ||
                       (((toggles_set & SHOW_FATAL) == 0) && (line[LOG_SIGN_POSITION] == 'F')) ||
                       (((toggles_set & SHOW_OFFLINE) == 0) && (line[LOG_SIGN_POSITION] == 'O')) ||
                       (((toggles_set & SHOW_DEBUG) == 0) && (line[LOG_SIGN_POSITION] == 'D')) ||
                       (((toggles_set & SHOW_TRACE) == 0) && (line[LOG_SIGN_POSITION] == 'T')) ||
                       (((toggles_set_parallel_jobs - 1) != (line[LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4] - 48)) &&
                       (toggles_set_parallel_jobs != 0))))
                  {
                     continue;
                  }
               }
               else
               {
                  if ((length > LOG_SIGN_POSITION) &&
                      ((((toggles_set & SHOW_INFO) == 0) && (line[LOG_SIGN_POSITION] == 'I')) ||
                       (((toggles_set & SHOW_CONFIG) == 0) && (line[LOG_SIGN_POSITION] == 'C')) ||
                       (((toggles_set & SHOW_WARN) == 0) && (line[LOG_SIGN_POSITION] == 'W')) ||
                       (((toggles_set & SHOW_ERROR) == 0) && (line[LOG_SIGN_POSITION] == 'E')) ||
                       (((toggles_set & SHOW_FATAL) == 0) && (line[LOG_SIGN_POSITION] == 'F')) ||
                       (((toggles_set & SHOW_OFFLINE) == 0) && (line[LOG_SIGN_POSITION] == 'O')) ||
                       (((toggles_set & SHOW_DEBUG) == 0) && (line[LOG_SIGN_POSITION] == 'D'))))
                  {
                     continue;
                  }
               }
               memcpy(&line_buffer[chars_buffered], line, length);
               chars_buffered += length;
               line_counter++;
               max_lines++;
               if (max_lines >= MAX_LINES_IN_ONE_GO)
               {
                  max_lines = 0;
                  display_data(w, &lock_counter, &cursor_counter, &locked,
                               &chars_buffered, line_buffer);
               }
            }
            else
            {
               if ((incomplete_line = realloc(incomplete_line,
                                              (incomplete_line_length + length))) == NULL)
               {
                  (void)xrec(FATAL_DIALOG, "realloc() error : %s (%s %d)",
                             strerror(errno), __FILE__, __LINE__);
                  return;
               }
               (void)memcpy(&incomplete_line[incomplete_line_length],
                            line, length);
               incomplete_line_length += length;
            }
         }
      }
      if (chars_buffered > 0)
      {
         max_lines = 0;
         display_data(w, &lock_counter, &cursor_counter, &locked,
                      &chars_buffered, line_buffer);
      }

      /* Required for implementations (BSD) with sticky EOF. */
      clearerr(p_log_file);

      free(line);
      free(line_buffer);
   }

   /* Has a new log file been created? */
   if ((((log_type_flag == TRANS_DB_LOG_TYPE) &&
         (total_length > max_logfile_size)) ||
        (((log_type_flag == SYSTEM_LOG_TYPE) ||
#ifdef _MAINTAINER_LOG
          (log_type_flag == MAINTAINER_LOG_TYPE) ||
#endif
          (log_type_flag == MON_SYSTEM_LOG_TYPE)) &&
         (total_length > max_logfile_size))) &&
       (current_log_number == 0))
   {
      char         log_file[MAX_PATH_LENGTH + 1 + MAX_FILENAME_LENGTH + 2];
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat  stat_buf;
#endif

      /*
       * When disk is full the process system_log/transfer_log will not
       * be able to start a new log file. We must check if this is the
       * case by looking at the inode number of the 'new' log file and
       * compare this with the old one. If the inodes are the same, we
       * know that the log process has failed to create a new log file.
       */
      (void)sprintf(log_file, "%s/%s0", log_dir, log_name);
#ifdef HAVE_STATX
      if ((statx(0, log_file, AT_STATX_SYNC_AS_STAT,
                 STATX_INO, &stat_buf) != -1) &&
          (stat_buf.stx_ino != current_inode_no))
#else
      if ((stat(log_file, &stat_buf) != -1) &&
          (stat_buf.st_ino != current_inode_no))
#endif
      {
         /* Yup, time to change the log file! */
         if (p_log_file != NULL)
         {
            (void)fclose(p_log_file);
            p_log_file = NULL;
         }

         /* Lets see if there is a new log file. */
         if ((p_log_file = fopen(log_file, "r")) != NULL)
         {
#ifdef _SLOW_COUNTER
            old_line_counter = 0;
#endif
            line_counter = 0;
            wpr_position = 0;
            total_length = 0;
            XmTextSetInsertionPosition(w, wpr_position);
            XmTextSetString(w, NULL);  /* Clears all old entries. */
            (void)sprintf(str_line, "%*d", MAX_LINE_COUNTER_DIGITS, 0);
            XmTextSetString(counterbox, str_line);
#ifdef HAVE_STATX
            current_inode_no = stat_buf.stx_ino;
#else
            current_inode_no = stat_buf.st_ino;
#endif
         }
      }
   }

   /* Reset cursor and ignore any events that might have occurred. */
   if (locked == 1)
   {
      XSetWindowAttributes attrs;
      XEvent               event;

      attrs.cursor = None;
      XChangeWindowAttributes(display, XtWindow(appshell),
                              CWCursor, &attrs);
      XFlush(display);

      /* Get rid of all events that have occurred. */
      while (XCheckMaskEvent(XtDisplay(appshell),
                             ButtonPressMask | ButtonReleaseMask |
                             ButtonMotionMask | PointerMotionMask |
                             KeyPressMask, &event) == True)
      {
         /* Do nothing. */;
      }
   }

#ifdef _SLOW_COUNTER
   if (old_line_counter != line_counter)
   {
      old_line_counter = line_counter;

      (void)sprintf(str_line, "%*d", MAX_LINE_COUNTER_DIGITS, line_counter);
      XmTextSetString(counterbox, str_line);
   }
#endif

   first_time = NO;
   interval_id_host = XtAppAddTimeOut(app, LOG_TIMEOUT,
                                      (XtTimerCallbackProc)check_log, w);

   return;
}


/*---------------------------- display_data() ---------------------------*/
static void
display_data(Widget       w,
             int          *lock_counter,
             int          *cursor_counter,
             int          *locked,
             unsigned int *chars_buffered,
             char         *line_buffer)
{
   static int           tflag = 0;
   int                  current_value,
                        max_value,
                        slider_size;
#ifndef _SLOW_COUNTER
   char                 str_line[MAX_LINE_COUNTER_DIGITS + 1];
#endif
   XSetWindowAttributes attrs;

   if ((*lock_counter % 10) == 0)
   {
      if (*locked == 0)
      {
         *locked = 1;
         attrs.cursor = cursor2;
         XChangeWindowAttributes(display, XtWindow(appshell), CWCursor, &attrs);
      }
      XFlush(display);
      XmUpdateDisplay(appshell);
   }
   if ((*cursor_counter % FALLING_SAND_SPEED) == 0)
   {
      if (tflag == 0)
      {
         tflag = 1;
         attrs.cursor = cursor1;
      }
      else
      {
         tflag = 0;
         attrs.cursor = cursor2;
      }
      XChangeWindowAttributes(display, XtWindow(appshell), CWCursor, &attrs);
   }
   (*lock_counter)++; (*cursor_counter)++;

   /*
    * When searching in a log file, by moving slider up/down, it is
    * annoying when new log information arrives and the cursor jumps to
    * the bottom to show the new text. Thus lets disable auto scrolling
    * when we are not at the end of the text.
    */
   XtVaGetValues(log_scroll_bar,
                 XmNvalue,      &current_value,
                 XmNmaximum,    &max_value,
                 XmNsliderSize, &slider_size,
                 NULL);

   line_buffer[*chars_buffered] = '\0';
   XmTextInsert(w, wpr_position, line_buffer);
   wpr_position += *chars_buffered;
   XtVaSetValues(w, XmNcursorPosition, wpr_position, NULL);
   *chars_buffered = 0;

   if (((max_value - slider_size) <= (current_value + 1)) ||
        (first_time == YES))
   {
      XmTextShowPosition(w, wpr_position);
   }

#ifndef _SLOW_COUNTER
   (void)sprintf(str_line, "%*d", MAX_LINE_COUNTER_DIGITS, line_counter);
   XmTextSetString(counterbox, str_line);
#endif

   return;
}
