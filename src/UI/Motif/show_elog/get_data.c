/*
 *  get_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_data - searches the AFD event log for entries
 **
 ** SYNOPSIS
 **   void get_data(void)
 **
 ** DESCRIPTION
 **   This function searches for the selected data in the
 **   event log of the AFD. The following things can be selected:
 **   start & end time, alias name, event class, event type and
 **   some general search term.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.07.2007 H.Kiehl Created
 **   16.09.2021 H.Kiehl If we use log_fd in check_log_updates() we never
 **                      detect that the inode changes when logs are rotated.
 **
 */
DESCR__E_M3

#include <stdio.h>        /* sprintf()                                   */
#include <string.h>       /* strerror()                                  */
#include <stdlib.h>       /* malloc(), realloc(), free(), strtod(), labs()*/
#include <time.h>         /* time()                                      */
#include <unistd.h>       /* close()                                     */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>       /* opendir(), closedir(), readdir()            */
#ifdef HAVE_MMAP
# include <sys/mman.h>    /* mmap(), munmap()                            */
# ifndef MAP_FILE         /* Required for BSD          */
#  define MAP_FILE 0      /* All others do not need it */
# endif
#endif
#include <errno.h>

#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <Xm/LabelP.h>
#include "motif_common_defs.h"
#include "show_elog.h"
#include "ea_str.h"
#include "logdefs.h"

#define IGNORE_ENTRY()         \
        {                      \
           while (*ptr != '\n')\
              ptr++;           \
           ptr++; i--;         \
           continue;           \
        }
#define CHECK_LIST_LIMIT()                                          \
        {                                                           \
           if ((perm.list_limit > 0) && (item_counter > perm.list_limit))\
           {                                                        \
              char msg_buffer[40];                                  \
                                                                    \
              (void)sprintf(msg_buffer, "List limit (%d) reached!", \
                            perm.list_limit);                       \
              show_message(statusbox_w, msg_buffer);                \
              break;                                                \
           }                                                        \
        }

/* External global variables. */
extern Display         *display;
extern Window          main_window;
extern XtAppContext    app;
extern Widget          appshell, /* CHECK_INTERRUPT() */
                       outputbox_w,
                       scrollbar_w,
                       statusbox_w,
                       special_button_w;
extern XmTextPosition  wpr_position;
extern int             continues_toggle_set,
                       max_event_log_files,
                       no_of_log_files,
                       no_of_search_dir_alias,
                       no_of_search_host_alias,
                       special_button_flag;
extern unsigned int    ea_toggles_set_1,
                       ea_toggles_set_2,
                       ea_toggles_set_3;
extern XT_PTR_TYPE     toggles_set;
extern time_t          start_time_val,
                       end_time_val;
extern char            *p_work_dir,
                       search_add_info[],
                       **search_dir_alias,
                       **search_host_alias;
extern struct sol_perm perm;

/* Local global variables. */
static int             interval_id_set = NO,
                       item_counter,
                       last_file_no,
                       log_fd = -1;
static unsigned int    total_no_events;
static time_t          local_start_time,
                       local_end_time,
                       start;
static off_t           log_offset;
static ino_t           log_inode;
static char            limit_reached,
                       line[MAX_TEXT_LINE_LENGTH + 2],
                       log_file[MAX_PATH_LENGTH],
                       *p_add_info,
                       *p_alias_name,
                       *p_event_action,
                       *p_event_class,
                       *p_event_type,
                       *p_log_file,
                       *str_list = NULL;
static XtIntervalId    interval_id_log;

/* Local function prototypes. */
static time_t          get_first_time(char *);
static char            *search_time(char *, time_t, time_t, time_t, size_t);
static void            check_log_updates(Widget),
                       display_data(int, unsigned int),
                       end_log_updates(void),
                       extract_data(char *, int, int),
                       search_data(register char *, char *, int, char *, off_t);


/*############################### get_data() ############################*/
void
get_data(void)
{
   int          end_file_no = -1,
                i,
                j,
                start_file_no = -1;
   time_t       end;
   char         status_message[MAX_MESSAGE_LENGTH];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif
   XmString     xstr;

   /* At start always reset these values. */
   if (interval_id_set == YES)
   {
      XtRemoveTimeOut(interval_id_log);
      interval_id_set = NO;
   }
   if (log_fd != -1)
   {
      if (close(log_fd) == -1)
      {
         (void)xrec(FATAL_DIALOG, "close() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }
      else
      {
         log_fd = -1;
      }
   }

   /* Prepare log file name. */
   p_log_file = log_file;
   no_of_log_files = max_event_log_files;
   p_log_file += sprintf(log_file, "%s%s/%s", p_work_dir, LOG_DIR,
                         EVENT_LOG_NAME);

   local_start_time = start_time_val;
   local_end_time   = end_time_val;

   for (i = 0; i < no_of_log_files; i++)
   {
      (void)sprintf(p_log_file, "%d", i);
#ifdef HAVE_STATX
      if (statx(0, log_file, AT_STATX_SYNC_AS_STAT,
                STATX_SIZE | STATX_MTIME, &stat_buf) == 0)
#else
      if (stat(log_file, &stat_buf) == 0)
#endif
      {
#ifdef HAVE_STATX
         if ((start_file_no == - 1) ||
             ((stat_buf.stx_size > 0) &&
              (get_first_time(log_file) > local_start_time)))
#else
         if ((start_file_no == - 1) ||
             ((stat_buf.st_size > 0) &&
              (get_first_time(log_file) > local_start_time)))
#endif
         {
            start_file_no = i;
         }
         if (local_end_time == -1)
         {
            if (end_file_no == -1)
            {
               end_file_no = i;
            }
         }
#ifdef HAVE_STATX
         else if ((stat_buf.stx_mtime.tv_sec >= local_end_time) ||
                  (end_file_no == -1))
#else
         else if ((stat_buf.st_mtime >= local_end_time) || (end_file_no == -1))
#endif
              {
                 end_file_no = i;
              }
      }
   }
   no_of_log_files = start_file_no - end_file_no + 1;

   if (str_list == NULL)
   {
      if ((str_list = malloc(LINES_BUFFERED * (MAX_TEXT_LINE_LENGTH + 2))) == NULL)
      {
         (void)xrec(FATAL_DIALOG, "malloc() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }
   }
   wpr_position = 0;

   /* Initialise line and its pointers. */
   (void)memset(line, ' ', MAX_TEXT_LINE_LENGTH - 1);
   line[MAX_TEXT_LINE_LENGTH] = '\0';
   p_event_class  = line + 20;
   p_event_type   = p_event_class + 2;
   p_alias_name   = p_event_type + 2;
   p_event_action = p_alias_name + MAX_ALIAS_LENGTH + 1;
   p_add_info     = p_event_action + MAX_EVENT_ACTION_LENGTH + 1;

   special_button_flag = STOP_BUTTON;
   xstr = XmStringCreateLtoR("Stop", XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);

   CHECK_INTERRUPT();

   start = time(NULL);
   total_no_events = 0;
   item_counter = 0;
   limit_reached = NO;

   j = 0;
   for (i = start_file_no;
        ((i >= end_file_no) && (special_button_flag != STOP_BUTTON_PRESSED));
        i--, j++)
   {
      (void)sprintf(p_log_file, "%d", i);
      (void)extract_data(log_file, j, i);
      if ((perm.list_limit > 0) && (total_no_events >= perm.list_limit))
      {
         break;
      }
   }

   end = time(NULL);

   if ((continues_toggle_set == NO) || (log_fd == -1) ||
       ((end > end_time_val) && (end_time_val != -1)))
   {
      set_sensitive();
      if ((perm.list_limit == 0) || (total_no_events < perm.list_limit))
      {
         /*
          * Do not show search time when list limit is reached.
          * Otherwise we overwrite the warning that list limit is
          * reached.
          */
         if (total_no_events != 0)
         {
            char pri_str[7];

            pri_str[0] = 'e'; pri_str[1] = 'v'; pri_str[2] = 'e';
            pri_str[3] = 'n'; pri_str[4] = 't';
            if (total_no_events > 1)
            {
               pri_str[5] = 's'; pri_str[6] = '\0';
            }
            else
            {
               pri_str[5] = '\0';
            }
#if SIZEOF_TIME_T == 4
# if SIZEOF_LONG == 4
            (void)sprintf(status_message, "%u %s found (%ld bytes). Search time: %lds",
# else
            (void)sprintf(status_message, "%u %s found (%lld bytes). Search time: %lds",
# endif
#else
# if SIZEOF_LONG == 4
            (void)sprintf(status_message, "%u %s found (%ld bytes). Search time: %llds",
# else
            (void)sprintf(status_message, "%u %s found (%ld bytes). Search time: %llds",
# endif
#endif
                          total_no_events, pri_str, wpr_position,
                          (pri_time_t)(end - start));
         }
         else
         {
#if SIZEOF_TIME_T == 4
            (void)sprintf(status_message, "No data found. Search time: %lds",
#else
            (void)sprintf(status_message, "No data found. Search time: %llds",
#endif
                          (pri_time_t)(end - start));
         }
         SHOW_MESSAGE();
      }

      special_button_flag = SEARCH_BUTTON;
      xstr = XmStringCreateLtoR("Search", XmFONTLIST_DEFAULT_TAG);
      XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
      XmStringFree(xstr);
      free(str_list);
      str_list = NULL;
   }
   else
   {
      interval_id_set = YES;
      interval_id_log = XtAppAddTimeOut(app, LOG_CHECK_INTERVAL,
                                        (XtTimerCallbackProc)check_log_updates,
                                        outputbox_w);
   }

   return;
}


/*+++++++++++++++++++++++++++ get_first_time() ++++++++++++++++++++++++++*/
static time_t
get_first_time(char *log_file)
{
   int    fd;
   time_t first_time;

   if ((fd = open(log_file, O_RDONLY)) == -1)
   {
      (void)fprintf(stderr, "Failed to open() `%s' : %s (%s %d)\n",
                    log_file, strerror(errno), __FILE__, __LINE__);
      first_time = 0L;
   }
   else
   {
      char date_str[LOG_DATE_LENGTH + 1];

      if (read(fd, date_str, LOG_DATE_LENGTH) != LOG_DATE_LENGTH)
      {
         (void)fprintf(stderr,
                       "Failed to read() %d bytes from `%s' : %s (%s %d)\n",
                       LOG_DATE_LENGTH, log_file, strerror(errno),
                       __FILE__, __LINE__);
         first_time = 0L;
      }
      else
      {
         int i = LOG_DATE_LENGTH - 1;

         while (date_str[i] == ' ')
         {
            i--;
         }
         i++;
         date_str[i] = '\0';
         first_time = (time_t)str2timet(date_str, NULL, 16);
      }

      if (close(fd) == -1)
      {
         (void)fprintf(stderr, "Failed to close() `%s' : %s (%s %d)\n",
                       log_file, strerror(errno), __FILE__, __LINE__);
      }
   }

   return(first_time);
}


/*+++++++++++++++++++++++++++++ extract_data() ++++++++++++++++++++++++++*/
static void
extract_data(char *current_log_file, int file_no, int log_no)
{
   int           fd;
   time_t        time_val,
                 earliest_entry,
                 latest_entry;
   register char *ptr,
                 *ptr_start;
   char          *src,
                 *tmp_ptr,
                 *ptr_end;
#ifdef HAVE_STATX
   struct statx  stat_buf;
#else
   struct stat   stat_buf;
#endif

   /* Check if file is there and get its size. */
#ifdef HAVE_STATX
   if (statx(0, current_log_file, AT_STATX_SYNC_AS_STAT,
             STATX_SIZE | STATX_INO, &stat_buf) == -1)
#else
   if (stat(current_log_file, &stat_buf) == -1)
#endif
   {
      if (errno == ENOENT)
      {
         /* For some reason the file is not there. So lets */
         /* assume we have found nothing.                  */;
      }
      else
      {
         (void)xrec(WARN_DIALOG, "Failed to access %s : %s (%s %d)",
                    current_log_file, strerror(errno), __FILE__, __LINE__);
      }
      return;
   }

   if ((fd = open(current_log_file, O_RDONLY)) == -1)
   {
      (void)xrec(FATAL_DIALOG, "Failed to open() %s : %s (%s %d)",
                 current_log_file, strerror(errno), __FILE__, __LINE__);
      return;
   }

   if ((log_no == 0) && ((end_time_val == -1) || (time(NULL) < end_time_val)))
   {
      /*
       * NOTE: We need to have this opened twice so that the function
       *       called with XtAppAddTimeOut() has its own file descriptor
       *       position within this file.
       */
      if ((log_fd = open(current_log_file, O_RDONLY)) == -1)
      {
         (void)xrec(FATAL_DIALOG, "Failed to open() %s : %s (%s %d)",
                    current_log_file, strerror(errno), __FILE__, __LINE__);
         return;
      }
#ifdef HAVE_STATX
      if (lseek(log_fd, stat_buf.stx_size, SEEK_SET) == (off_t)-1)
#else
      if (lseek(log_fd, stat_buf.st_size, SEEK_SET) == (off_t)-1)
#endif
      {
         (void)xrec(FATAL_DIALOG, "Failed to lssek() in %s : %s (%s %d)",
                    current_log_file, strerror(errno), __FILE__, __LINE__);
         return;
      }
#ifdef HAVE_STATX
      log_offset = stat_buf.stx_size;
      log_inode = stat_buf.stx_ino;
#else
      log_offset = stat_buf.st_size;
      log_inode = stat_buf.st_ino;
#endif
      last_file_no = file_no;
   }

   /* Make sure there is data in the log file. */
#ifdef HAVE_STATX
   if (stat_buf.stx_size == 0)
#else
   if (stat_buf.st_size == 0)
#endif
   {
      return;
   }

#ifdef HAVE_MMAP
   if ((src = mmap(NULL,
# ifdef HAVE_STATX
                   stat_buf.stx_size, PROT_READ,
# else
                   stat_buf.st_size, PROT_READ,
# endif
                   (MAP_FILE | MAP_SHARED), fd, 0)) == (caddr_t) -1)
   {
      (void)xrec(FATAL_DIALOG, "Failed to mmap() %s : %s (%s %d)",
                 current_log_file, strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      return;
   }
#else
# ifdef HAVE_STATX
   if ((src = malloc(stat_buf.stx_size)) == NULL)
# else
   if ((src = malloc(stat_buf.st_size)) == NULL)
# endif
   {
      (void)xrec(FATAL_DIALOG, "malloc() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      return;
   }
# ifdef HAVE_STATX
   if (read(fd, src, stat_buf.stx_size) != stat_buf.stx_size)
# else
   if (read(fd, src, stat_buf.st_size) != stat_buf.st_size)
# endif
   {
      (void)xrec(FATAL_DIALOG, "Failed to read() from %s : %s (%s %d)",
                 current_log_file, strerror(errno), __FILE__, __LINE__);
      free(src);
      (void)close(fd);
      return;
   }
#endif

   if (close(fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }

   /*
    * Now we have the source data in the buffer 'src'. Lets
    * search for the stuff the user wants to see. First
    * collect this data in the buffer 'dst' and only after
    * we have found all data will 'dst' be displayed.
    */

   /* Get latest entry. */
#ifdef HAVE_STATX
   tmp_ptr = src + stat_buf.stx_size - 2;
#else
   tmp_ptr = src + stat_buf.st_size - 2;
#endif
   while ((*tmp_ptr != '\n') && (src != tmp_ptr))
   {
      tmp_ptr--;
   }
   if (*tmp_ptr == '\n')
   {
      ptr = tmp_ptr + 1;
   }
   else
   {
      ptr = tmp_ptr;
   }
   time_val = (time_t)str2timet(ptr, NULL, 16);
   latest_entry = time_val;

   /* Get earliest entry. */
   ptr = src;
   time_val = (time_t)str2timet(ptr, NULL, 16);
   earliest_entry = time_val;

   if (local_start_time == -1)
   {
      ptr_start = src;

#ifdef HAVE_STATX
      ptr_end = search_time(src, local_end_time, earliest_entry,
                            latest_entry, stat_buf.stx_size);
#else
      ptr_end = search_time(src, local_end_time, earliest_entry,
                            latest_entry, stat_buf.st_size);
#endif
   }
   else
   {
      /*
       * Search for the first entry of 'local_start_time'. Get the very
       * first time entry and see if this is not already higher than
       * 'local_start_time', ie this is our first entry.
       */
      if (earliest_entry >= local_start_time)
      {
         ptr_start = src;
      }
      else
      {
#ifdef HAVE_STATX
         ptr_start = search_time(src, local_start_time, earliest_entry,
                                 latest_entry, stat_buf.stx_size);
#else
         ptr_start = search_time(src, local_start_time, earliest_entry,
                                 latest_entry, stat_buf.st_size);
#endif
      }

#ifdef HAVE_STATX
      ptr_end = search_time(src, local_end_time, earliest_entry,
                            latest_entry, stat_buf.stx_size);
#else
      ptr_end = search_time(src, local_end_time, earliest_entry,
                            latest_entry, stat_buf.st_size);
#endif
   }

   if (ptr_start == ptr_end)
   {
      /* Free all memory we have allocated. */
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
      if (munmap(src, stat_buf.stx_size) < 0)
# else
      if (munmap(src, stat_buf.st_size) < 0)
# endif
      {
         (void)xrec(ERROR_DIALOG, "munmap() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
      }
#else
      free(src);
#endif
      return;
   }

   /*
    * So, start and end are found. Now lets do the real search,
    * ie search for specific file names, recipient, etc.
    */
   search_data(ptr_start, ptr_end, file_no, src, 0);

   /* Free all memory we have allocated. */
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
   if (munmap(src, stat_buf.stx_size) < 0)
# else
   if (munmap(src, stat_buf.st_size) < 0)
# endif
   {
      (void)xrec(ERROR_DIALOG, "munmap() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
   }
#else
   free(src);
#endif

   return;
}


/*------------------------------ search_data() --------------------------*/
static void
search_data(register char *ptr,
            char          *ptr_end,
            int           file_no,
            char          *p_start_log_file,
            off_t         offset)
{
   register int i,
                j,
                k;
   int          loops = 0,
                type;
   time_t       prev_time_val = 0L,
                now,
                time_when_transmitted = 0L;
   unsigned int event_action_no;
   unsigned int bytes_buffered,
                bytes_written;
   char         *ptr_start_line,
                str_number[MAX_INT_LENGTH + 1];
   struct tm    *p_ts;

   do
   {
      bytes_buffered = 0;
      for (i = 0; ((i < LINES_BUFFERED) && (ptr < ptr_end)); i++)
      {
         if (((i % 200) == 0) &&
             ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL))
         {
            prev_time_val = now;
            CHECK_INTERRUPT();
            if (special_button_flag != STOP_BUTTON)
            {
               loops--;
               break;
            }
         }

         ptr_start_line = ptr;

         /* Evaluate event class. */
         HEX_CHAR_TO_INT((*(ptr_start_line + LOG_DATE_LENGTH + 1)));
         if (type == EC_HOST)
         {
            if (toggles_set & SHOW_CLASS_HOST)
            {
               time_when_transmitted = (time_t)str2timet(ptr_start_line, NULL, 16);
               if ((p_ts = localtime(&time_when_transmitted)) == NULL)
               {
                  IGNORE_ENTRY();
               }
               else
               {
                  CONVERT_TIME_YEAR();
                  *p_event_class = 'H';
               }
            }
            else
            {
               IGNORE_ENTRY();
            }
         }
         else if (type == EC_GLOB)
              {
                 if (toggles_set & SHOW_CLASS_GLOBAL)
                 {
                    time_when_transmitted = (time_t)str2timet(ptr_start_line, NULL, 16);
                    if ((p_ts = localtime(&time_when_transmitted)) == NULL)
                    {
                       IGNORE_ENTRY();
                    }
                    else
                    {
                       CONVERT_TIME_YEAR();
                       *p_event_class = 'G';
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
         else if (type == EC_PROD)
              {
                 if (toggles_set & SHOW_CLASS_PRODUCTION)
                 {
                    time_when_transmitted = (time_t)str2timet(ptr_start_line, NULL, 16);
                    if ((p_ts = localtime(&time_when_transmitted)) == NULL)
                    {
                       IGNORE_ENTRY();
                    }
                    else
                    {
                       CONVERT_TIME_YEAR();
                       *p_event_class = 'P';
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
         else if (type == EC_DIR)
              {
                 if (toggles_set & SHOW_CLASS_DIRECTORY)
                 {
                    time_when_transmitted = (time_t)str2timet(ptr_start_line, NULL, 16);
                    if ((p_ts = localtime(&time_when_transmitted)) == NULL)
                    {
                       IGNORE_ENTRY();
                    }
                    else
                    {
                       CONVERT_TIME_YEAR();
                       *p_event_class = 'D';
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
              else
              {
                 time_when_transmitted = (time_t)str2timet(ptr_start_line, NULL, 16);
                 if ((p_ts = localtime(&time_when_transmitted)) == NULL)
                 {
                    IGNORE_ENTRY();
                 }
                 else
                 {
                    CONVERT_TIME_YEAR();
                    *p_event_class = '?';
                 }
              }

         /* Evaluate event type. */
         HEX_CHAR_TO_INT((*(ptr_start_line + LOG_DATE_LENGTH + 3)));
         if (type == ET_EXT)
         {
            if (toggles_set & SHOW_TYPE_EXTERNAL)
            {
               *p_event_type = 'E';
            }
            else
            {
               IGNORE_ENTRY();
            }
         }
         else if (type == ET_AUTO)
              {
                 if (toggles_set & SHOW_TYPE_AUTO)
                 {
                    *p_event_type = 'A';
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
         else if (type == ET_MAN)
              {
                 if (toggles_set & SHOW_TYPE_MANUAL)
                 {
                    *p_event_type = 'M';
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
              else
              {
                 *p_event_type = '?';
              }

         ptr += LOG_DATE_LENGTH + 5;
         k = 0;
         do
         {
            str_number[k] = *(ptr + k);
            k++;
         } while ((*(ptr + k) != SEPARATOR_CHAR) && (*(ptr + k) != '\n') &&
                  (k < MAX_INT_LENGTH));
         str_number[k] = '\0';
         event_action_no = (unsigned int)strtoul(str_number, NULL, 16);
         if (event_action_no > EA_MAX_EVENT_ACTION)
         {
            /* Hmm, some event we do not know about. */
            IGNORE_ENTRY();
         }
         if (event_action_no < EA_START_TRANSFER)
         {
            if ((ea_toggles_set_1 & (1 << event_action_no)) == 0)
            {
               IGNORE_ENTRY();
            }
         }
         else if (event_action_no < EA_INFO_TIME_SET)
              {
                 if ((ea_toggles_set_2 & (1 << (event_action_no - EA_DISABLE_HOST))) == 0)
                 {
                    IGNORE_ENTRY();
                 }
              }
              else
              {
                 if ((ea_toggles_set_3 & (1 << (event_action_no - EA_DISABLE_CREATE_SOURCE_DIR))) == 0)
                 {
                    IGNORE_ENTRY();
                 }
              }
         ptr += k;
         if ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
         {
            IGNORE_ENTRY();
         }
         k = 0;
         while (eastr[event_action_no][k] != '\0')
         {
            *(p_event_action + k) = eastr[event_action_no][k];
            k++;
         }
         while (k <= (int)MAX_EVENT_ACTION_LENGTH)
         {
            *(p_event_action + k) = ' ';
            k++;
         }
         ptr++;
         k = 0;
         if ((*p_event_class == 'H') || (*p_event_class == 'D'))
         {
            while ((k < MAX_ALIAS_LENGTH) && (*(ptr + k) != SEPARATOR_CHAR) &&
                   (*(ptr + k) != '\n'))
            {
               *(p_alias_name + k) = *(ptr + k);
               k++;
            }
            if ((*p_event_class == 'H') && (no_of_search_host_alias > 0))
            {
               int gotcha = NO,
                   kk,
                   ret;

               *(p_alias_name + k) = '\0';
               for (kk = 0; kk < no_of_search_host_alias; kk++)
               {
                  if (((ret = pmatch(search_host_alias[kk], p_alias_name, NULL)) == 0) ||
                      ((ret == -1) && (search_host_alias[kk][0] == '!')))
                  {
                     gotcha = YES;
                     break;
                  }
               }
               if (gotcha == NO)
               {
                  IGNORE_ENTRY();
               }
               *(p_alias_name + k) = ' ';
            }
            else if ((*p_event_class == 'D') && (no_of_search_dir_alias > 0))
                 {
                    int gotcha = NO,
                        kk,
                        ret;

                    *(p_alias_name + k) = '\0';
                    for (kk = 0; kk < no_of_search_dir_alias; kk++)
                    {
                       if (((ret = pmatch(search_dir_alias[kk], p_alias_name, NULL)) == 0) ||
                           ((ret == -1) && (search_dir_alias[kk][0] == '!')))
                       {
                          gotcha = YES;
                          break;
                       }
                    }
                    if (gotcha == NO)
                    {
                       IGNORE_ENTRY();
                    }
                    *(p_alias_name + k) = ' ';
                 }
            ptr += k;
            while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
            {
               ptr++;
            }
            if (*ptr == SEPARATOR_CHAR)
            {
               ptr++;
            }
         }
         while (k <= MAX_ALIAS_LENGTH)
         {
            *(p_alias_name + k) = ' ';
            k++;
         }

         if (*(ptr - 1) == SEPARATOR_CHAR)
         {
            if ((*p_event_type == 'M') && (event_action_no == EA_SWITCH_HOST))
            {
               k = 0;
               while ((*(ptr + k) != SEPARATOR_CHAR) && (*(ptr + k) != '\n'))
               {
                  *(p_add_info + k) = *(ptr + k);
                  k++;
               }
               j = k;
               if (*(ptr + k) == SEPARATOR_CHAR)
               {
                  *(p_add_info + j) = '\n';
                  (void)memset((p_add_info + j + 1), ' ', LENGTH_TO_ADD_INFO);
                  j += 1 + LENGTH_TO_ADD_INFO;
                  k += 1;
                  while ((*(ptr + k) != SEPARATOR_CHAR) && (*(ptr + k) != '\n'))
                  {
                     *(p_add_info + j) = *(ptr + k);
                     k++; j++;
                  }
                  ptr += k;
                  if (*ptr == SEPARATOR_CHAR)
                  {
                     /* Add reason. */
                     ptr++;
                     *(p_add_info + j) = '\n';
                     (void)memset((p_add_info + j + 1), ' ', LENGTH_TO_ADD_INFO);
                     j += 1 + LENGTH_TO_ADD_INFO;
                     k = 0;
                     while (*(ptr + k) != '\n')
                     {
                        if (*(ptr + k) == '%')
                        {
                           char hex_char[3];

                           hex_char[0] = *(ptr + k + 1);
                           hex_char[1] = *(ptr + k + 2);
                           hex_char[2] = '\0';
                           *(p_add_info + j) = (char)strtol(hex_char, NULL, 16);
                           k += 3;
                           if (*(p_add_info + j) == '\n')
                           {
                              (void)memset((p_add_info + j + 1), ' ',
                                           LENGTH_TO_ADD_INFO);
                              j += 1 + LENGTH_TO_ADD_INFO;
                           }
                           else
                           {
                              j += 1;
                           }
                        }
                        else
                        {
                           *(p_add_info + j) = *(ptr + k);
                           j++; k++;
                        }
                     }
                  }
                  *(p_add_info + j) = '\n';
                  *(p_add_info + j + 1) = '\0';
                  bytes_written = p_add_info - line + j + 1;
               }
               else
               {
                  bytes_written = 0;
               }
            }
            else if ((event_action_no == EA_START_ERROR_QUEUE) ||
                     (event_action_no == EA_STOP_ERROR_QUEUE))
                 {
                    k = 0;
                    *p_add_info = '#';
                    while ((*(ptr + k) != SEPARATOR_CHAR) && (*(ptr + k) != '\n'))
                    {
                       *(p_add_info + k + 1) = *(ptr + k);
                       k++;
                    }
                    ptr += k;
                    if (*ptr == SEPARATOR_CHAR)
                    {
                       char hex_char[MAX_INT_LENGTH + 1];

                       ptr++;
                       *(p_add_info + k + 1) = ' ';
                       j = k + 2;
                       k = 0;
                       while ((*(ptr + k) != '\n') &&
                              (*(ptr + k) != SEPARATOR_CHAR) &&
                              (k < MAX_INT_LENGTH))
                       {
                          hex_char[k] = *(ptr + k);
                          k++;
                       }
                       hex_char[k] = '\0';
                       (void)strcpy((p_add_info + j),
                                     get_error_str((int)strtol(hex_char, NULL, 16)));
                       while (*(p_add_info + j) != '\0')
                       {
                          j++;
                       }
                       while (*(ptr + k) != '\n')
                       {
                          k++;
                       }
                       ptr += k;
                    }
                    else
                    {
                       j = k + 1;
                    }
                    *(p_add_info + j) = '\n';
                    *(p_add_info + j + 1) = '\0';
                    bytes_written = p_add_info - line + j + 1;
                 }
            else if ((event_action_no == EA_AMG_STOP) ||
                     (event_action_no == EA_ENABLE_DIRECTORY) ||
                     (event_action_no == EA_DISABLE_DIRECTORY) ||
                     (event_action_no == EA_START_DIRECTORY) ||
                     (event_action_no == EA_STOP_DIRECTORY) ||
                     ((event_action_no == EA_OFFLINE) && (*p_event_type == 'M')) ||
                     (event_action_no == EA_ACKNOWLEDGE) ||
                     (event_action_no == EA_ENABLE_HOST) ||
                     (event_action_no == EA_UNSET_ACK_OFFL) ||
                     (event_action_no == EA_DISABLE_HOST) ||
                     (event_action_no == EA_START_TRANSFER) ||
                     (event_action_no == EA_STOP_TRANSFER) ||
                     ((event_action_no == EA_START_QUEUE) && (*p_event_type == 'M')) ||
                     ((event_action_no == EA_STOP_QUEUE) && (*p_event_type == 'M')))
                 {
                    /* It is first user and then (maybe) reason, */
                    /* that we need to display.                  */
                    k = 0;
                    while ((*(ptr + k) != SEPARATOR_CHAR) && (*(ptr + k) != '\n'))
                    {
                       *(p_add_info + k) = *(ptr + k);
                       k++;
                    }
                    ptr += k;
                    j = k;
                    if (*ptr == SEPARATOR_CHAR)
                    {
                       /* Add reason. */
                       ptr++;
                       *(p_add_info + j) = '\n';
                       (void)memset((p_add_info + j + 1), ' ', LENGTH_TO_ADD_INFO);
                       j += 1 + LENGTH_TO_ADD_INFO;
                       k = 0;
                       while (*(ptr + k) != '\n')
                       {
                          if (*(ptr + k) == '%')
                          {
                             char hex_char[3];

                             hex_char[0] = *(ptr + k + 1);
                             hex_char[1] = *(ptr + k + 2);
                             hex_char[2] = '\0';
                             *(p_add_info + j) = (char)strtol(hex_char, NULL, 16);
                             k += 3;
                             if (*(p_add_info + j) == '\n')
                             {
                                (void)memset((p_add_info + j + 1), ' ',
                                             LENGTH_TO_ADD_INFO);
                                j += 1 + LENGTH_TO_ADD_INFO;
                             }
                             else
                             {
                                j += 1;
                             }
                          }
                          else
                          {
                             *(p_add_info + j) = *(ptr + k);
                             j++; k++;
                          }
                       }
                    }
                    else
                    {
                       k = 0;
                    }
                    ptr += k;
                    *(p_add_info + j) = '\n';
                    *(p_add_info + j + 1) = '\0';
                    bytes_written = p_add_info - line + j + 1;
                 }
                 else
                 {
                    k = 0;
                    while (*(ptr + k) != '\n')
                    {
                       *(p_add_info + k) = *(ptr + k);
                       k++;
                    }
                    ptr += k;
                    *(p_add_info + k) = '\n';
                    *(p_add_info + k + 1) = '\0';
                    bytes_written = p_add_info - line + k + 1;
                 }

            /* Check if we need search in the additional information. */
            if (search_add_info[1] != '\0')
            {
               if (pmatch(search_add_info, p_add_info, NULL) != 0)
               {
                  IGNORE_ENTRY();
               }
            }
         }
         else
         {
            if (search_add_info[1] != '\0')
            {
               IGNORE_ENTRY();
            }
            *p_add_info = '\n';
            *(p_add_info + 1) = '\0';
            bytes_written = p_add_info - line + 1;
         }

         item_counter++;
         (void)memcpy(&str_list[bytes_buffered], line, bytes_written);
         bytes_buffered += bytes_written;
         ptr++;
      }

      loops++;

      /* Display what we have in buffer. */
      str_list[bytes_buffered] = '\0';
      display_data(i, bytes_buffered);
      bytes_buffered = 0;

      /* Check if user has done anything. */
      if ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL)
      {
         prev_time_val = now;
         CHECK_INTERRUPT();
      }

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

   return;
}


/*++++++++++++++++++++++++++ check_log_updates() ++++++++++++++++++++++++*/
static void
check_log_updates(Widget w)
{
   static int rotate;

   interval_id_set = NO;
   if (special_button_flag == STOP_BUTTON_PRESSED)
   {
      end_log_updates();
      return;
   }

   if ((end_time_val == -1) || (time(NULL) < end_time_val))
   {
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat stat_buf;
#endif

#ifdef HAVE_STATX
      if (statx(0, log_file, AT_STATX_SYNC_AS_STAT,
                STATX_SIZE | STATX_INO, &stat_buf) == -1)
#else
      if (stat(log_file, &stat_buf) == -1)
#endif
      {
         (void)xrec(FATAL_DIALOG, "Failed to access `%s' : %s (%s %d)\n",
                    log_file, strerror(errno), __FILE__, __LINE__);
      }
#ifdef HAVE_STATX
      if (log_inode != stat_buf.stx_ino)
#else
      if (log_inode != stat_buf.st_ino)
#endif
      {
#ifdef HAVE_STATX
         struct statx old_stat_buf;
#else
         struct stat old_stat_buf;
#endif

         /* Don't switch log to early. There might be some last data */
         /* in the old file.                                         */
#ifdef HAVE_STATX
         if (statx(log_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                   STATX_SIZE, &old_stat_buf) == -1)
#else
         if (fstat(log_fd, &old_stat_buf) == -1)
#endif
         {
            (void)xrec(FATAL_DIALOG,
#ifdef HAVE_STATX
                       "statx() error : %s (%s %d)\n",
#else
                       "fstat() error : %s (%s %d)\n",
#endif
                       strerror(errno), __FILE__, __LINE__);
         }
#ifdef HAVE_STATX
         if (old_stat_buf.stx_size > log_offset)
#else
         if (old_stat_buf.st_size > log_offset)
#endif
         {
#ifdef HAVE_STATX
            stat_buf.stx_size = old_stat_buf.stx_size;
#else
            stat_buf.st_size = old_stat_buf.st_size;
#endif
         }
         else
         {
            XmTextSetString(outputbox_w, NULL);
            XmTextSetInsertionPosition(outputbox_w, 0);
            get_data();
            XtVaSetValues(outputbox_w, XmNcursorPosition, wpr_position, NULL);
            XmTextShowPosition(outputbox_w, wpr_position);
            return;
         }
      }
#ifdef HAVE_STATX
      if (stat_buf.stx_size > log_offset)
#else
      if (stat_buf.st_size > log_offset)
#endif
      {
         off_t diff_size;
         char  *ptr_start,
               *ptr_end;

#ifdef HAVE_STATX
         diff_size = stat_buf.stx_size - log_offset;
#else
         diff_size = stat_buf.st_size - log_offset;
#endif
         if ((ptr_start = malloc(diff_size)) == NULL)
         {
#if SIZEOF_OFF_T == 4
            (void)xrec(FATAL_DIALOG, "malloc() error [%ld bytes] : %s (%s %d)",
#else
            (void)xrec(FATAL_DIALOG, "malloc() error [%lld bytes] : %s (%s %d)",
#endif
                       (pri_off_t)diff_size, strerror(errno),
                       __FILE__, __LINE__);
         }
         if (read(log_fd, ptr_start, diff_size) != diff_size)
         {
            (void)xrec(FATAL_DIALOG, "read() error: %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         }
         ptr_end = ptr_start + diff_size;

         search_data(ptr_start, ptr_end, last_file_no, ptr_start, log_offset);

         free(ptr_start);
#ifdef HAVE_STATX
         log_offset = stat_buf.stx_size;
#else
         log_offset = stat_buf.st_size;
#endif
         XtVaSetValues(outputbox_w, XmNcursorPosition, wpr_position, NULL);
         XmTextShowPosition(outputbox_w, wpr_position);
      }
      else
      {
         char status_message[13];

         (void)memset(status_message, ' ', 12);
         status_message[12] = '\0';
         status_message[rotate] = '.';
         rotate++;
         if (rotate == 12)
         {
            rotate = 0;
         }
         SHOW_MESSAGE();
      }
      interval_id_set = YES;
      interval_id_log = XtAppAddTimeOut(app, LOG_CHECK_INTERVAL,
                                        (XtTimerCallbackProc)check_log_updates,
                                        outputbox_w);
   }
   else
   {
      end_log_updates();
   }
   return;
}


/*-------------------------- end_log_updates() --------------------------*/
static void
end_log_updates(void)
{
   time_t   diff_time;
   char     *ptr,
            status_message[MAX_MESSAGE_LENGTH];
   XmString xstr;

   if (total_no_events != 0)
   {
      char pri_str[7];

      pri_str[0] = 'e'; pri_str[1] = 'v'; pri_str[2] = 'e';
      pri_str[3] = 'n'; pri_str[4] = 't';
      if (total_no_events > 1)
      {
         pri_str[5] = 's'; pri_str[6] = '\0';
      }
      else
      {
         pri_str[5] = '\0';
      }
      ptr = status_message + sprintf(status_message,
#if SIZOF_LONG == 4
                                     "%u %s found (%ld bytes). Search+Wait time: ",
#else
                                     "%u %s found (%ld bytes). Search+Wait time: ",
#endif
                                     total_no_events, pri_str, wpr_position);
   }
   else
   {
      ptr = status_message + sprintf(status_message,
                                     "No data found. Search+Wait time: ");
   }
   diff_time = time(NULL) - start;
   if (diff_time > 3600)
   {
      int hours, left, min;

      hours = diff_time / 3600;
      left  = diff_time % 3600;
      min   = left / 60;
      left  = left % 60;
      (void)sprintf(ptr, "%dh %dm %ds", hours, min, left);
   }
   else if (diff_time > 60)
        {
           int left, min;

           min  = diff_time / 60;
           left = diff_time % 60;
           (void)sprintf(ptr, "%dm %ds", min, left);
        }
        else
        {
#if SIZEOF_TIME_T == 4
           (void)sprintf(ptr, "%lds", (pri_time_t)diff_time);
#else
           (void)sprintf(ptr, "%llds", (pri_time_t)diff_time);
#endif
        }

   SHOW_MESSAGE();

   special_button_flag = SEARCH_BUTTON;
   xstr = XmStringCreateLtoR("Search", XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);
   free(str_list);
   str_list = NULL;

   return;
}


/*------------------------------ search_time() --------------------------*/
static char *
search_time(char   *src,
            time_t search_time_val,
            time_t earliest_entry,
            time_t latest_entry,
            size_t size)
{
   char   *ptr,
          *bs_ptr;
   time_t time_val;

   if ((search_time_val == -1) || (latest_entry < search_time_val))
   {
      return(src + size);
   }
   else
   {
      /*
       * YUCK! Now we have to search for it! We know the
       * time of the very first entry and the last entry. So
       * lets see if 'search_time_val' is closer to the beginning
       * or end in our buffer. Thats where we will start our
       * search.
       */
      if (labs(search_time_val - earliest_entry) >
          labs(latest_entry - search_time_val))
      {
         /* Start search from end. */
         bs_ptr = src + size - 2;
         do
         {
            ptr = bs_ptr;
            ptr -= LOG_DATE_LENGTH + 1 + MAX_HOSTNAME_LENGTH + 3;
            while ((ptr >= src) && (*ptr != '\n'))
            {
               ptr--;
            }
            bs_ptr = ptr - 1;
            ptr++;
            time_val = (time_t)str2timet(ptr, NULL, 16);
         } while ((time_val >= search_time_val) && (ptr > src));
         while (*ptr != '\n')
         {
            ptr++;
         }
      }
      else /* Start search from beginning. */
      {
         ptr = src;
         do
         {
            ptr += LOG_DATE_LENGTH + 1 + MAX_HOSTNAME_LENGTH + 3;
            while (*ptr != '\n')
            {
               ptr++;
            }
            ptr++;
            time_val = (time_t)str2timet(ptr, NULL, 16);
         } while ((time_val < search_time_val) && (ptr < (src + size)));
         while (*ptr != '\n')
         {
            ptr--;
         }
      }
      return(ptr + 1);
   }
}


/*--------------------------- display_data() ----------------------------*/
static void
display_data(int i, unsigned int chars_buffered)
{
   static int   rotate;
   char         status_message[MAX_MESSAGE_LENGTH];
   XmString     xstr;
   XExposeEvent xeev;
   Dimension    w, h;

   XmTextInsert(outputbox_w, wpr_position, str_list);
   wpr_position += chars_buffered;
   XmTextShowPosition(outputbox_w, wpr_position);
   total_no_events += i;
   rotate++;
   if (rotate == 0)
   {
      (void)strcpy(status_message, "Searching  -");
   }
   else if (rotate == 1)
        {
           (void)strcpy(status_message, "Searching  \\");
        }
   else if (rotate == 2)
        {
           (void)strcpy(status_message, "Searching  |");
        }
        else
        {
           (void)strcpy(status_message, "Searching  /");
           rotate = -1;
        }

   xeev.type = Expose;
   xeev.display = display;
   xeev.window = main_window;
   xeev.x = 0; xeev.y = 0;
   XtVaGetValues(statusbox_w, XmNwidth, &w, XmNheight, &h, NULL);
   xeev.width = w; xeev.height = h;
   (XtClass(statusbox_w))->core_class.expose(statusbox_w, (XEvent *)&xeev, NULL);
   xstr = XmStringCreateLtoR(status_message, XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(statusbox_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);

   return;
}
