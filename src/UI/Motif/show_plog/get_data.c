/*
 *  get_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2016, 2017 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_data - searches production log files for data
 **
 ** SYNOPSIS
 **   void get_data(void)
 **
 ** DESCRIPTION
 **   This function searches for the selected data in the production
 **   log file of the AFD. The following things can be selected:
 **   start & end time, file name, file length, directory and
 **   recipient. Only selected data will be shown in the list
 **   widget.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.09.2016 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>        /* snprintf()                                  */
#include <string.h>       /* strcmp(), strerror()                        */
#include <stdlib.h>       /* malloc(), realloc(), free(), strtoul(), labs()*/
#include <time.h>         /* time()                                      */
#ifdef TM_IN_SYS_TIME
# include <sys/time.h>
#endif
#include <unistd.h>       /* close()                                     */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>    /* mmap(), munmap()                            */
# ifndef MAP_FILE         /* Required for BSD          */
#  define MAP_FILE 0      /* All others do not need it */
# endif
#endif
#include <errno.h>

#include <Xm/Xm.h>
#include <Xm/List.h>
#include <Xm/Text.h>
#include <Xm/LabelP.h>
#include "logdefs.h"
#include "mafd_ctrl.h"
#include "show_plog.h"

/* #define _MACRO_DEBUG */

/* External global variables. */
extern Display          *display;
extern Window           main_window;
extern XtAppContext     app;
extern Widget           appshell, /* CHECK_INTERRUPT() */
                        listbox_w,
                        scrollbar_w,
                        statusbox_w,
                        special_button_w,
                        summarybox_w;
extern int              continues_toggle_set,
                        do_search_return_code,
                        file_name_toggle_set,
                        radio_set,
                        gt_lt_sign_ct,
                        gt_lt_sign_new,
                        gt_lt_sign_orig,
                        gt_lt_sign_pt,
                        gt_lt_sign_rc,
                        log_date_length,
                        max_production_log_files,
                        no_of_search_dirs,
                        no_of_search_dirids,
                        no_of_search_hosts,
                        no_of_search_jobids,
                        ratio_mode,
                        *search_dir_length,
                        search_return_code,
                        special_button_flag,
                        file_name_length,
                        no_of_log_files;
extern unsigned int     all_list_items,
                        *search_dirid,
                        *search_jobid;
extern XT_PTR_TYPE      toggles_set;
extern size_t           search_orig_file_size,
                        search_new_file_size;
extern time_t           start_time_val,
                        end_time_val;
extern double           search_cpu_time,
                        search_prod_time;
extern char             *p_work_dir,
                        search_new_file_name[],
                        search_orig_file_name[],
                        search_production_cmd[],
                        **search_recipient,
                        **search_dir,
                        *search_dir_filter,
                        **search_user,
                        summary_str[],
                        total_summary_str[];
extern struct item_list *il;
extern struct info_data id;
extern struct sol_perm  perm;

/* Local global variables. */
static unsigned int     total_no_files,
                        unprintable_chars;
static int              interval_id_set = NO,
                        last_file_no,
                        log_fd = -1;
static time_t           local_start_time,
                        local_end_time,
                        first_date_found,
                        start;
static off_t            log_offset;
static ino_t            log_inode;
static double           cpu_time,
                        new_file_size,
                        orig_file_size,
                        prod_time;
static char             *p_orig_file_name,
                        *p_orig_file_size,
                        *p_new_file_name,
                        *p_new_file_size,
                        *p_ratio,
                        *p_command,
                        *p_cpu_time,
                        *p_rc,
                        *p_prod_time,
                        log_file[MAX_PATH_LENGTH],
                        *p_log_file,
                        line[MAX_PRODUCTION_LINE_LENGTH + SHOW_LONG_FORMAT + SHOW_LONG_FORMAT + 1];
static XmStringTable    str_list;
static XtIntervalId     interval_id_log;

/* Local function prototypes. */
static char   *search_time(char *, time_t, time_t, time_t, size_t);
static void   check_log_updates(Widget),
              display_data(int, time_t, time_t),
              end_log_updates(void),
              extract_data(char *, int, int),
              collect_data(register char *, char *, int, char *, off_t);

#define REALLOC_OFFSET_BUFFER()                                   \
        {                                                         \
           if ((item_counter == 0) || (item_counter > prev_item_counter)) \
           {                                                      \
              int new_int_size,                                   \
                  new_char_size,                                  \
                  new_off_t_size;                                 \
                                                                  \
              new_char_size = item_counter + LINES_BUFFERED + 1;  \
              new_int_size = new_char_size * sizeof(int);         \
              new_off_t_size = new_char_size * sizeof(off_t);     \
              prev_item_counter = item_counter;                   \
                                                                  \
              if (((il[file_no].offset = realloc(il[file_no].offset, new_int_size)) == NULL) ||          \
                  ((il[file_no].line_offset = realloc(il[file_no].line_offset, new_off_t_size)) == NULL))\
              {                                                   \
                 (void)xrec(FATAL_DIALOG, "realloc() error : %s (%s %d)",\
                            strerror(errno), __FILE__, __LINE__); \
                 return;                                          \
              }                                                   \
           }                                                      \
        }
#define IGNORE_ENTRY()            \
        {                         \
           while (*ptr != '\n')   \
              ptr++;              \
           ptr++; i--;            \
           continue;              \
        }
#define CHECK_LIST_LIMIT()                                          \
        {                                                           \
           if ((perm.list_limit > 0) && (item_counter > perm.list_limit))\
           {                                                        \
              char msg_buffer[40];                                  \
                                                                    \
              (void)snprintf(msg_buffer, 40, "List limit (%d) reached!",\
                             perm.list_limit);                      \
              show_message(statusbox_w, msg_buffer);                \
              break;                                                \
           }                                                        \
        }



/*############################### get_data() ############################*/
void
get_data(void)
{
   int         i,
               j,
               start_file_no = -1,
               end_file_no = -1;
   time_t      end;
   char        status_message[MAX_MESSAGE_LENGTH];
   struct stat stat_buf;
   XmString    xstr;

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
   i = snprintf(log_file, MAX_PATH_LENGTH, "%s%s/%s",
                p_work_dir, LOG_DIR, PRODUCTION_BUFFER_FILE);
   if (i >= MAX_PATH_LENGTH)
   {
      (void)xrec(FATAL_DIALOG, "Buffer to small %d >= %d (%s %d)",
                 i, MAX_PATH_LENGTH, __FILE__, __LINE__);
      return;
   }
   p_log_file = log_file + i;
   no_of_log_files = max_production_log_files;

   local_start_time = start_time_val;
   local_end_time   = end_time_val;

   for (i = 0; i < no_of_log_files; i++)
   {
      (void)snprintf(p_log_file, MAX_PATH_LENGTH - (p_log_file - log_file),
                     "%d", i);
      if (stat(log_file, &stat_buf) == 0)
      {
         if (((stat_buf.st_mtime + SWITCH_FILE_TIME) >= local_start_time) ||
             (start_file_no == -1))
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
         else if ((stat_buf.st_mtime >= local_end_time) || (end_file_no == -1))
              {
                 end_file_no = i;
              }
      }
   }
   no_of_log_files = start_file_no - end_file_no + 1;

   if ((str_list = (XmStringTable)XtMalloc(LINES_BUFFERED * sizeof(XmString))) == NULL)
   {
      (void)xrec(FATAL_DIALOG, "XtMalloc() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }

   /* Allocate memory for item list. */
   if (il == NULL)
   {
      if ((il = malloc(max_production_log_files * sizeof(struct item_list))) == NULL)
      {
         (void)xrec(FATAL_DIALOG, "malloc() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }
      for (i = 0; i < max_production_log_files; i++)
      {
         il[i].fp = NULL;
         il[i].no_of_items = 0;
         il[i].line_offset = NULL;
         il[i].offset = NULL;
      }
   }
   else
   {
      for (i = 0; i < max_production_log_files; i++)
      {
         if (il[i].fp != NULL)
         {
            if (fclose(il[i].fp) == EOF)
            {
               (void)fprintf(stderr, "fclose() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
            }
            il[i].fp = NULL;
         }
         il[i].no_of_items = 0;
         if (il[i].line_offset != NULL)
         {
            free(il[i].line_offset);
            il[i].line_offset = NULL;
         }
         if (il[i].offset != NULL)
         {
            free(il[i].offset);
            il[i].offset = NULL;
         }
      }
   }

   /* Initialise all pointer in line. */
   p_orig_file_name = line + 16;
   p_orig_file_size = p_orig_file_name + file_name_length + 1;
   p_new_file_name  = p_orig_file_size + MAX_DISPLAYED_FILE_SIZE + 1;
   p_new_file_size  = p_new_file_name + file_name_length + 1;
   p_ratio          = p_new_file_size + MAX_DISPLAYED_FILE_SIZE + 1;
   p_command        = p_ratio + MAX_DISPLAYED_RATIO + 1;
   p_rc             = p_command + MAX_DISPLAYED_COMMAND + 1;
   p_prod_time      = p_rc + MAX_DISPLAYED_RC + 1;
   p_cpu_time       = p_prod_time + MAX_DISPLAYED_PROD_TIME + 1;
   *(line + MAX_PRODUCTION_LINE_LENGTH + file_name_length + file_name_length) = '\0';

   special_button_flag = STOP_BUTTON;
   xstr = XmStringCreateLtoR("Stop", XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);

   summary_str[0] = ' ';
   summary_str[1] = '\0';
   SHOW_SUMMARY_DATA();
   (void)strcpy(status_message, "Searching  -");
   SHOW_MESSAGE();
   CHECK_INTERRUPT();

   start = time(NULL);
   orig_file_size = new_file_size = prod_time = cpu_time = 0.0;
   total_no_files = 0;
   first_date_found = -1;
   unprintable_chars = 0;
   j = 0;
   for (i = start_file_no;
        ((i >= end_file_no) && (special_button_flag != STOP_BUTTON_PRESSED));
        i--, j++)
   {
      (void)snprintf(p_log_file, MAX_PATH_LENGTH - (p_log_file - log_file),
                     "%d", i);
      (void)extract_data(log_file, j, i);
      if ((perm.list_limit > 0) && (total_no_files >= perm.list_limit))
      {
         break;
      }
   }
   end = time(NULL);

   if ((continues_toggle_set == NO) || (log_fd == -1) ||
       ((end > end_time_val) && (end_time_val != -1)))
   {
      set_sensitive();
      if ((perm.list_limit == 0) || (total_no_files < perm.list_limit))
      {
         /*
          * Do not show search time when list limit is reached.
          * Otherwise we overwrite the warning that list limit is
          * reached.
          */
         if (total_no_files == 0)
         {
            i = snprintf(status_message, MAX_MESSAGE_LENGTH, "No data found. ");
         }
         else
         {
            i = 0;
         }
         i += snprintf(status_message + i, MAX_MESSAGE_LENGTH - i,
#if SIZEOF_TIME_T == 4
                       "Search time: %lds", (pri_time_t)(end - start));
#else
                       "Search time: %llds", (pri_time_t)(end - start));
#endif
         if (unprintable_chars > 0)
         {
            (void)snprintf(status_message + i, MAX_MESSAGE_LENGTH - i,
                           " (%u unprintable chars!)", unprintable_chars);
         }
         SHOW_MESSAGE();
      }

      special_button_flag = SEARCH_BUTTON;
      xstr = XmStringCreateLtoR("Search", XmFONTLIST_DEFAULT_TAG);
      XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
      XmStringFree(xstr);
      XtFree((char *)str_list);
   }
   else
   {
      interval_id_set = YES;
      interval_id_log = XtAppAddTimeOut(app, LOG_CHECK_INTERVAL,
                                        (XtTimerCallbackProc)check_log_updates,
                                        listbox_w);
   }

   return;
}


/*+++++++++++++++++++++++++++++ extract_data() ++++++++++++++++++++++++++*/
static void
extract_data(char *current_log_file, int file_no, int log_no)
{
   int           fd;
   time_t        earliest_entry,
                 latest_entry;
   register char *ptr,
                 *ptr_start;
   char          *src,
                 *tmp_ptr,
                 *ptr_end;
   struct stat   stat_buf;

   /* Check if file is there and get its size. */
   if (stat(current_log_file, &stat_buf) < 0)
   {
      if (errno == ENOENT)
      {
         /* For some reason the file is not there. So lets */
         /* assume we have found nothing.                  */
         return;
      }
      else
      {
         (void)xrec(WARN_DIALOG, "Failed to stat() %s : %s (%s %d)",
                    current_log_file, strerror(errno), __FILE__, __LINE__);
         return;
      }
   }

   /* Make sure there is data in the log file. */
   if (stat_buf.st_size == 0)
   {
      return;
   }

   if ((fd = open(current_log_file, O_RDONLY)) == -1)
   {
      (void)xrec(FATAL_DIALOG, "Failed to open() %s : %s (%s %d)",
                 current_log_file, strerror(errno), __FILE__, __LINE__);
      return;
   }
   if ((il[file_no].fp = fdopen(fd, "r")) == NULL)
   {
      (void)xrec(FATAL_DIALOG, "fdopen() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }
#ifdef HAVE_MMAP
   if ((src = mmap(NULL, stat_buf.st_size, PROT_READ,
                   (MAP_FILE | MAP_SHARED), fd, 0)) == (caddr_t) -1)
   {
      (void)xrec(FATAL_DIALOG, "Failed to mmap() %s : %s (%s %d)",
                 current_log_file, strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      return;
   }
#else
   if ((src = malloc(stat_buf.st_size)) == NULL)
   {
      (void)xrec(FATAL_DIALOG, "malloc() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      return;
   }
   if (read(fd, src, stat_buf.st_size) != stat_buf.st_size)
   {
      (void)xrec(FATAL_DIALOG, "Failed to read() from %s : %s (%s %d)",
                 current_log_file, strerror(errno), __FILE__, __LINE__);
      free(src);
      (void)close(fd);
      return;
   }
#endif

   /*
    * Now we have the source data in the buffer 'src'. Lets
    * search for the stuff the user wants to see. First
    * collect this data in the buffer 'dst' and only after
    * we have found all data will 'dst' be displayed. The
    * destination buffer 'dst' will be realloced in sizes
    * 10 * MAX_PRODUCTION_LINE_LENGTH characters. Hope this will reduce
    * memory usage a little bit.
    */

   /* Get latest entry. */
   tmp_ptr = src + stat_buf.st_size - 2;
   do
   {
      while ((*tmp_ptr != '\n') && (src != tmp_ptr))
      {
         tmp_ptr--;
      }
      if (*tmp_ptr == '\n')
      {
         ptr = tmp_ptr + 1;
         if (*ptr == '#')
         {
            tmp_ptr -= 1;
         }
      }
      else
      {
         if (*tmp_ptr == '#')
         {
            /* Free all memory we have allocated. */
#ifdef HAVE_MMAP
            if (munmap(src, stat_buf.st_size) < 0)
            {
               (void)xrec(ERROR_DIALOG, "munmap() error : %s (%s %d)",
                          strerror(errno), __FILE__, __LINE__);
            }
#else
            free(src);
#endif
            return;
         }
         ptr = tmp_ptr;
      }
   } while ((*ptr == '#') && (src != tmp_ptr));

   if (*ptr == '#')
   {
      /* Free all memory we have allocated. */
#ifdef HAVE_MMAP
      if (munmap(src, stat_buf.st_size) < 0)
      {
         (void)xrec(ERROR_DIALOG, "munmap() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
      }
#else
      free(src);
#endif
      return;
   }
   latest_entry = (time_t)str2timet(ptr, NULL, 16);

   /* Get earliest entry. */
   ptr = src;
   while (*ptr == '#')
   {
      while (*ptr != '\n')
      {
         if (ptr == (src + stat_buf.st_size))
         {
            /* Free all memory we have allocated. */
#ifdef HAVE_MMAP
            if (munmap(src, stat_buf.st_size) < 0)
            {
               (void)xrec(ERROR_DIALOG, "munmap() error : %s (%s %d)",
                          strerror(errno), __FILE__, __LINE__);
            }
#else
            free(src);
#endif
            return;
         }
         ptr++;
      }
      ptr++;
   }
   earliest_entry = (time_t)str2timet(ptr, NULL, 16);

   if (local_start_time == -1)
   {
      ptr_start = src;

      ptr_end = search_time(src, local_end_time, earliest_entry,
                            latest_entry, stat_buf.st_size);
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
         ptr_start = search_time(src, local_start_time, earliest_entry,
                                 latest_entry, stat_buf.st_size);
      }

      ptr_end = search_time(src, local_end_time, earliest_entry,
                            latest_entry, stat_buf.st_size);
   }

   if (ptr_start == ptr_end)
   {
      /* Free all memory we have allocated. */
#ifdef HAVE_MMAP
      if (munmap(src, stat_buf.st_size) < 0)
      {
         (void)xrec(ERROR_DIALOG, "munmap() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
      }
#else
      free(src);
#endif
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
      if (lseek(log_fd, stat_buf.st_size, SEEK_SET) == (off_t)-1)
      {
         (void)xrec(FATAL_DIALOG, "Failed to lssek() in %s : %s (%s %d)",
                    current_log_file, strerror(errno), __FILE__, __LINE__);
         return;
      }
      log_offset = stat_buf.st_size;
      log_inode = stat_buf.st_ino;
      last_file_no = file_no;
   }

   /*
    * So, start and end are found. Now lets do the real search,
    * ie search for specific file names, recipient, etc.
    */
   ptr = ptr_start;
   collect_data(ptr_start, ptr_end, file_no, src, 0);

   /* Free all memory we have allocated. */
   get_info_free();
#ifdef HAVE_MMAP
   if (munmap(src, stat_buf.st_size) < 0)
   {
      (void)xrec(ERROR_DIALOG, "munmap() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
   }
#else
   free(src);
#endif

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
      struct stat stat_buf;

      if (fstat(log_fd, &stat_buf) == -1)
      {
         (void)xrec(FATAL_DIALOG, "fstat() error: %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      }
      if (log_inode != stat_buf.st_ino)
      {
         /*
          * This seems from the point of programming the simplest. It
          * is not efficient, but since the logs are rotated daily once
          * seems an acceptable solution.
          */
         XmListDeleteAllItems(listbox_w);
         get_data();
         XmListSetBottomPos(listbox_w, 0);
         return;
      }
      if (stat_buf.st_size > log_offset)
      {
         off_t diff_size;
         char  *ptr_start,
               *ptr_end;

         diff_size = stat_buf.st_size - log_offset;
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

         collect_data(ptr_start, ptr_end, last_file_no, ptr_start, log_offset);

         get_info_free();
         free(ptr_start);
         log_offset = stat_buf.st_size;
         XmListSetBottomPos(listbox_w, 0);
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
                                        listbox_w);
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

   if (total_no_files != 0)
   {
      ptr = status_message + snprintf(status_message, MAX_MESSAGE_LENGTH,
                                      "Search+Wait time: ");
   }
   else
   {
      ptr = status_message + snprintf(status_message, MAX_MESSAGE_LENGTH,
                                      "No data found. Search+Wait time: ");
   }
   if ((ptr - status_message) >= MAX_MESSAGE_LENGTH)
   {
      (void)xrec(FATAL_DIALOG, "Buffer to small %d >= %d (%s %d)",
                 (ptr - status_message), MAX_MESSAGE_LENGTH,
                 __FILE__, __LINE__);
      return;
   }
   diff_time = time(NULL) - start;
   if (diff_time > 3600)
   {
      int hours, left, min;

      hours = diff_time / 3600;
      left  = diff_time % 3600;
      min   = left / 60;
      left  = left % 60;
      (void)snprintf(ptr, MAX_MESSAGE_LENGTH - (ptr - status_message),
                     "%dh %dm %ds", hours, min, left);
   }
   else if (diff_time > 60)
        {
           int left, min;

           min  = diff_time / 60;
           left = diff_time % 60;
           (void)snprintf(ptr, MAX_MESSAGE_LENGTH - (ptr - status_message),
                          "%dm %ds", min, left);
        }
        else
        {
           (void)snprintf(ptr, MAX_MESSAGE_LENGTH - (ptr - status_message),
#if SIZEOF_TIME_T == 4
                          "%lds", (pri_time_t)diff_time);
#else
                          "%llds", (pri_time_t)diff_time);
#endif
        }

   SHOW_MESSAGE();

   special_button_flag = SEARCH_BUTTON;
   xstr = XmStringCreateLtoR("Search", XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);
   XtFree((char *)str_list);

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
            ptr -= log_date_length + 1 + file_name_length + 1 + file_name_length + 3;
            while ((ptr >= src) && (*ptr != '\n'))
            {
               ptr--;
            }
            bs_ptr = ptr - 1;
            ptr++;
            if (*ptr == '#')
            {
               time_val = search_time_val;
            }
            else
            {
               time_val = (time_t)str2timet(ptr, NULL, 16);
            }
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
            ptr += log_date_length + 1 + file_name_length + 1 + file_name_length + 3;
            while (*ptr != '\n')
            {
               ptr++;
            }
            ptr++;
            if (*ptr == '#')
            {
               time_val = search_time_val - 1;
            }
            else
            {
               time_val = (time_t)str2timet(ptr, NULL, 16);
            }
         } while ((time_val < search_time_val) && (ptr < (src + size)));
         while (*ptr != '\n')
         {
            ptr--;
         }
      }
      return(ptr + 1);
   }
}


/*----------------------------- collect_data() --------------------------*/
/*                              --------------                           */
/* Description: Reads everyting from ptr to ptr_end.                     */
/*-----------------------------------------------------------------------*/
static void
collect_data(register char *ptr,
             char          *ptr_end,
             int           file_no,
             char          *p_start_log_file,
             off_t         offset)
{
   register int i,
                j;
   int          item_counter = il[file_no].no_of_items,
                onefoureigth_or_greater,
                prev_item_counter = il[file_no].no_of_items,
#ifndef LESSTIF_WORKAROUND
                unmanaged,
#endif
                loops = 0;
   time_t       prev_time_val = 0L,
                now;
   char         numeric_str[MAX_DOUBLE_LENGTH + 1],
                *ptr_start_line;
   struct tm    *p_ts;

#ifndef LESSTIF_WORKAROUND
   if (item_counter == 0)
   {
      XtUnmanageChild(listbox_w);
      unmanaged = YES;
   }
   else
   {
      unmanaged = NO;
   }
#endif
   do
   {
      for (i = 0; ((i < LINES_BUFFERED) && (ptr < ptr_end)); i++)
      {
         /* Allocate memory for offset to job ID. */
         REALLOC_OFFSET_BUFFER();

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

         if (*ptr == '#')
         {
            if ((*(ptr + 1) == '!') && (*(ptr + 2) == '#'))
            {
               get_log_type_data(ptr + 3);
            }
            IGNORE_ENTRY();
         }

         ptr_start_line = ptr;

         il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);

         (void)memset(line, ' ', MAX_PRODUCTION_LINE_LENGTH + file_name_length + file_name_length);
         id.time_when_produced = (time_t)str2timet(ptr_start_line, NULL, 16);
         if (first_date_found == -1)
         {
            first_date_found = id.time_when_produced;
         }
         p_ts = localtime(&id.time_when_produced);
         CONVERT_TIME();

         /* Away with the date. */
         ptr += log_date_length;

         /*
          * First production log was without a ratio. As of
          * Version 1.4 we inserted a ratio before the original
          * file name.
          */
         j = 0;
         while ((*ptr != ':') && (*ptr != '_') &&
                (*ptr != '\n') && (j < MAX_DOUBLE_LENGTH))
         {
            numeric_str[j] = *ptr;
            j++; ptr++;
         }
         if (*ptr == ':')
         {
            /* This is the 1.4.x version PRODUCTION_LOG. */
            numeric_str[j] = '\0';
            id.ratio_1 = (int)strtoul(numeric_str, NULL, 16);
            ptr++; /* Away with : */
            j = 0;
            while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n') &&
                   (j < MAX_DOUBLE_LENGTH))
            {
               numeric_str[j] = *ptr;
               j++; ptr++;
            }
            if (*ptr == SEPARATOR_CHAR)
            {
               numeric_str[j] = '\0';
               id.ratio_2 = (int)strtoul(numeric_str, NULL, 16);
               ptr++; /* Away with SEPARATOR_CHAR */
               j = 0;
               while ((*ptr != '.') && (*ptr != '_') &&
                      (*ptr != SEPARATOR_CHAR) && (*ptr != '\n') &&
                      (j < MAX_DOUBLE_LENGTH))
               {
                  numeric_str[j] = *ptr;
                  j++; ptr++;
               }
            }
            else
            {
               IGNORE_ENTRY();
            }
         }
         else
         {
            id.ratio_1 = -1;
            id.ratio_2 = -1;
         }

         if ((ratio_mode == ANY_RATIO) ||
             ((ratio_mode == ONE_TO_ONE_RATIO) && (id.ratio_1 == 1) &&
              (id.ratio_2 == 1)) ||
             ((ratio_mode == ONE_TO_NONE_RATIO) && (id.ratio_1 == 1) &&
              (id.ratio_2 == 0)) ||
             ((ratio_mode == ONE_TO_N_RATIO) && (id.ratio_1 == 1) &&
              (id.ratio_2 > 1)) ||
             ((ratio_mode == N_TO_ONE_RATIO) && (id.ratio_1 > 1) &&
              (id.ratio_2 == 1)) ||
             ((ratio_mode == N_TO_N_RATIO) && (id.ratio_1 > 1) &&
              (id.ratio_2 > 1)))
         {
            /*
             * As of version 1.4.8 we added two more fields: production time
             * + CPU usage and original file size.
             */
            if ((*ptr == '.') || (*ptr == SEPARATOR_CHAR))
            {
               if (*ptr == SEPARATOR_CHAR)
               {
                  id.production_time = 0.0;
                  id.cpu_time = -1.0;
                  ptr++;
               }
               else
               {
                  numeric_str[j] = *ptr;
                  j++; ptr++;
                  while ((*ptr != '.') && (*ptr != SEPARATOR_CHAR) &&
                         (*ptr != '\n') && (j < MAX_DOUBLE_LENGTH))
                  {
                     numeric_str[j] = *ptr;
                     j++; ptr++;
                  }
                  if (j == MAX_DOUBLE_LENGTH)
                  {
                     IGNORE_ENTRY();
                  }
                  else
                  {
                     numeric_str[j] = '\0';
                     id.production_time = strtod(numeric_str, NULL);
                     ptr++; /* Away with '.' or SEPARATOR_CHAR */
                     if (*(ptr - 1)  == '.')
                     {
                        long int cpu_usec;
                        time_t   cpu_sec;

                        /* Get CPU seconds. */
                        j = 0;
                        while ((*ptr != '.') && (*ptr != SEPARATOR_CHAR) &&
                               (*ptr != '\n') && (j < MAX_INT_HEX_LENGTH))
                        {
                           numeric_str[j] = *ptr;
                           j++; ptr++;
                        }
                        if (j == MAX_INT_HEX_LENGTH)
                        {
                           while ((*ptr != '.') && (*ptr != SEPARATOR_CHAR) &&
                                  (*ptr != '\n'))
                           {
                              ptr++;
                           }
                           cpu_sec = 0L;
                        }
                        else
                        {
                           numeric_str[j] = '\0';
                           cpu_sec = (time_t)str2timet(numeric_str, NULL, 16);
                        }
                        if ((*ptr == '.') || (*ptr == SEPARATOR_CHAR))
                        {
                           ptr++;
                        }

                        if (*(ptr - 1)  == '.')
                        {
                           /* Get CPU usecs. */
                           j = 0;
                           while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n') &&
                                  (j < MAX_INT_HEX_LENGTH))
                           {
                              numeric_str[j] = *ptr;
                              j++; ptr++;
                           }
                           if (j == MAX_INT_HEX_LENGTH)
                           {
                              while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
                              {
                                 ptr++;
                              }
                              cpu_usec = 0L;
                           }
                           else
                           {
                              numeric_str[j] = '\0';
                              cpu_usec = strtol(numeric_str, NULL, 16);
                           }
                           if (*ptr == SEPARATOR_CHAR)
                           {
                              ptr++;
                           }
                        }
                        else
                        {
                           cpu_usec = 0L;
                        }
                        id.cpu_time = (double)(cpu_sec + (cpu_usec / (double)1000000));
                     }
                     else
                     {
                        id.cpu_time = -1.0;
                     }
                  }
               }
               if (((search_prod_time == -1.0) ||
                    (((gt_lt_sign_pt == EQUAL_SIGN) &&
                      (id.production_time == search_prod_time)) ||
                     ((gt_lt_sign_pt == LESS_THEN_SIGN) && 
                      (id.production_time < search_prod_time)) ||
                     ((gt_lt_sign_pt == GREATER_THEN_SIGN) &&
                      (id.production_time > search_prod_time)) ||
                     ((gt_lt_sign_pt == NOT_SIGN) &&
                      (id.production_time != search_prod_time)))) &&
                   ((search_cpu_time == -1.0) ||
                    (((gt_lt_sign_ct == EQUAL_SIGN) &&
                      (id.cpu_time == search_cpu_time)) ||
                     ((gt_lt_sign_ct == LESS_THEN_SIGN) && 
                      (id.cpu_time < search_cpu_time)) ||
                     ((gt_lt_sign_ct == GREATER_THEN_SIGN) &&
                      (id.cpu_time > search_cpu_time)) ||
                     ((gt_lt_sign_ct == NOT_SIGN) &&
                      (id.cpu_time != search_cpu_time)))))
               {
                  j = 0;
                  while ((*ptr != '_') && (*ptr != '\n') &&
                         (j < MAX_DOUBLE_LENGTH))
                  {
                     numeric_str[j] = *ptr;
                     j++; ptr++;
                  }
                  onefoureigth_or_greater = YES;
                  numeric_str[j] = '\0';
                  id.input_time = (time_t)str2timet(numeric_str, NULL, 16);
               }
               else
               {
                  IGNORE_ENTRY();
               }
            }
            else
            {
               onefoureigth_or_greater = NO;

               numeric_str[j] = '\0';
               id.input_time = (time_t)str2timet(numeric_str, NULL, 16);
               id.production_time = id.time_when_produced - id.input_time;
               id.cpu_time = -1.0;
               if (((search_prod_time == -1.0) ||
                    (((gt_lt_sign_pt == EQUAL_SIGN) &&
                      (id.production_time == search_prod_time)) ||
                     ((gt_lt_sign_pt == LESS_THEN_SIGN) && 
                      (id.production_time < search_prod_time)) ||
                     ((gt_lt_sign_pt == GREATER_THEN_SIGN) &&
                      (id.production_time > search_prod_time)) ||
                     ((gt_lt_sign_pt == NOT_SIGN) &&
                      (id.production_time != search_prod_time)))) &&
                   ((search_cpu_time == -1.0) ||
                    (((gt_lt_sign_ct == EQUAL_SIGN) &&
                      (id.cpu_time == search_cpu_time)) ||
                     ((gt_lt_sign_ct == LESS_THEN_SIGN) && 
                      (id.cpu_time < search_cpu_time)) ||
                     ((gt_lt_sign_ct == GREATER_THEN_SIGN) &&
                      (id.cpu_time > search_cpu_time)) ||
                     ((gt_lt_sign_ct == NOT_SIGN) &&
                      (id.cpu_time != search_cpu_time)))))
               {
                  /* Nothing to do. */;
               }
               else
               {
                  IGNORE_ENTRY();
               }
            }

            /* Away with unique number + split job counter. */
            while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
            {
               ptr++;
            }
            if (*ptr == SEPARATOR_CHAR)
            {
               ptr++;
            }

            /* Store directory ID. */
            j = 0;
            while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n') &&
                   (j < MAX_DOUBLE_LENGTH))
            {
               numeric_str[j] = *ptr;
               j++; ptr++;
            }
            if (*ptr == SEPARATOR_CHAR)
            {
               numeric_str[j] = '\0';
               id.dir_id = (unsigned int)strtoul(numeric_str, NULL, 16);
               ptr++; /* Away with SEPARATOR_CHAR */
            }
            else
            {
               IGNORE_ENTRY();
            }

            /* Store job ID. */
            j = 0;
            while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n') &&
                   (j < MAX_DOUBLE_LENGTH))
            {
               numeric_str[j] = *ptr;
               j++; ptr++;
            }
            if (*ptr == SEPARATOR_CHAR)
            {
               numeric_str[j] = '\0';
               id.job_id = (unsigned int)strtoul(numeric_str, NULL, 16);
            }
            else
            {
               IGNORE_ENTRY();
            }

            if ((no_of_search_dirs > 0) || (no_of_search_dirids > 0))
            {
               int gotcha = NO,
                   kk;

               if (no_of_search_dirs == 0)
               {
                  for (kk = 0; kk < no_of_search_dirids; kk++)
                  {
                     if (search_dirid[kk] == id.dir_id)
                     {
                        gotcha = YES;
                        break;
                     }
                  }
               }
               if (gotcha == NO)
               {
                  size_t length;

                  id.dir[0] = '\0';
                  get_info(GOT_DIR_ID_DIR_ONLY);
                  length = strlen((char *)id.dir);
                  id.dir[length] = SEPARATOR_CHAR;
                  id.dir[length + 1] = '\0';

                  for (kk = 0; kk < no_of_search_dirs; kk++)
                  {
                     if (search_dir_filter[kk] == YES)
                     {
                        if (sfilter(search_dir[kk], (char *)id.dir,
                                    SEPARATOR_CHAR) == 0)
                        {
                           gotcha = YES;
                           break;
                        }
                     }
                     else
                     {
                        if (search_dir_length[kk] == length)
                        {
                           id.dir[length] = '\0';
                           if (strncmp(search_dir[kk], (char *)id.dir,
                                       length) == 0)
                           {
                              gotcha = YES;
                              break;
                           }
                           else
                           {
                              id.dir[length] = SEPARATOR_CHAR;
                           }
                        }
                     }
                  }
                  if (gotcha == NO)
                  {
                     IGNORE_ENTRY();
                  }
               }
            }

            if (no_of_search_jobids > 0)
            {
               int gotcha = NO,
                   kk;

               for (kk = 0; kk < no_of_search_jobids; kk++)
               {
                  if (id.job_id == search_jobid[kk])
                  {
                     gotcha = YES;
                     break;
                  }
               }
               if (gotcha == NO)
               {
                  IGNORE_ENTRY();
               }
            }

            if (no_of_search_hosts > 0)
            {
               int gotcha = NO,
                   kk;

               get_info(GOT_JOB_ID_HOST_ONLY);

               for (kk = 0; kk < no_of_search_hosts; kk++)
               {
                  if (sfilter(search_recipient[kk], id.host_alias,
                              SEPARATOR_CHAR) == 0)
                  {
                     if (search_user[kk][0] != '\0')
                     {
                        char *at_ptr;

                        id.user[0] = '\0';
                        id.mail_destination[0] = '\0';
                        get_info(GOT_JOB_ID_USER_ONLY);

                        if (id.mail_destination[0] != '\0')
                        {
                           at_ptr = search_user[kk];

                           while ((*at_ptr != ' ') && (*at_ptr != '@') &&
                                  (*at_ptr != '\0'))
                           {
                              at_ptr++;
                           }
                           if (*at_ptr == '@')
                           {
                              at_ptr = id.mail_destination;
                           }
                           else
                           {
                              at_ptr = (char *)id.user;
                           }
                        }
                        else
                        {
                           at_ptr = (char *)id.user;
                        }
                        if (sfilter(search_user[kk], at_ptr, ' ') == 0)
                        {
                           gotcha = YES;
                        }
                     }
                     else
                     {
                        gotcha = YES;
                     }
                     break;
                  }
               }
               if (gotcha == NO)
               {
                  IGNORE_ENTRY();
               }
            }

            /* Check the original filename. */
            ptr++; /* Away with SEPARATOR_CHAR */
            if ((search_orig_file_name[0] == '\0') ||
                ((search_orig_file_name[0] == '*') &&
                 (search_orig_file_name[1] == '\0')) ||
                (sfilter(search_orig_file_name, ptr, SEPARATOR_CHAR) == 0))
            {
               j = 0;
               while ((*(ptr + j) != SEPARATOR_CHAR) && (j < file_name_length))
               {
                  if ((unsigned char)(*(ptr + j)) < ' ')
                  {
                     *(p_orig_file_name + j) = '?';
                     unprintable_chars++;
                  }
                  else
                  {
                     *(p_orig_file_name + j) = *(ptr + j);
                  }
                  j++;
               }
               ptr += j;
            }
            else
            {
               IGNORE_ENTRY();
            }

            /* If necessary, ignore rest of file name. */
            while (*ptr != SEPARATOR_CHAR)
            {
               ptr++;
            }
            ptr++;

            if (onefoureigth_or_greater == YES)
            {
               /* Store original file size. */
               j = 0;
               while ((*ptr != SEPARATOR_CHAR) &&
                      (*ptr != '\n') && (j < MAX_DOUBLE_LENGTH))
               {
                  numeric_str[j] = *ptr;
                  j++; ptr++;
               }
               if (*ptr == SEPARATOR_CHAR)
               {
                  numeric_str[j] = '\0';
                  id.orig_file_size = (off_t)str2offt(numeric_str, NULL, 16);

                  if ((search_orig_file_size == -1) || (j == 0) ||
                      ((gt_lt_sign_orig == EQUAL_SIGN) &&
                       (id.orig_file_size == search_orig_file_size)) ||
                      ((gt_lt_sign_orig == LESS_THEN_SIGN) &&
                       (id.orig_file_size < search_orig_file_size)) ||
                      ((gt_lt_sign_orig == GREATER_THEN_SIGN) &&
                       (id.orig_file_size > search_orig_file_size)) ||
                      ((gt_lt_sign_orig == NOT_SIGN) &&
                       (id.orig_file_size != search_orig_file_size)))
                  {
                     ptr++; /* Away with SEPARATOR_CHAR */
                  }
               }
               else
               {
                  if (j == MAX_DOUBLE_LENGTH)
                  {
                     while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
                     {
                        ptr++;
                     }
                     ptr++; /* Away with SEPARATOR_CHAR */
                     id.orig_file_size = 0;
                  }
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
            }
            else
            {
               id.orig_file_size = -1;
            }

            /* Check new filename. */
            if ((search_new_file_name[0] == '\0') ||
                ((search_new_file_name[0] == '*') &&
                 (search_new_file_name[1] == '\0')) ||
                (sfilter(search_new_file_name, ptr, SEPARATOR_CHAR) == 0))
            {
               j = 0;
               while ((*(ptr + j) != SEPARATOR_CHAR) && (j < file_name_length))
               {
                  if ((unsigned char)(*(ptr + j)) < ' ')
                  {
                     *(p_new_file_name + j) = '?';
                     unprintable_chars++;
                  }
                  else
                  {
                     *(p_new_file_name + j) = *(ptr + j);
                  }
                  j++;
               }
               ptr += j;
            }
            else
            {
               IGNORE_ENTRY();
            }

            /* If necessary, ignore rest of file name. */
            while (*ptr != SEPARATOR_CHAR)
            {
               ptr++;
            }
            ptr++;

            /* Store new file size. */
            j = 0;
            while ((*ptr != SEPARATOR_CHAR) &&
                   (*ptr != '\n') && (j < MAX_DOUBLE_LENGTH))
            {
               numeric_str[j] = *ptr;
               j++; ptr++;
            }
            if (*ptr == SEPARATOR_CHAR)
            {
               if (j == 0)
               {
                  id.new_file_size = -1;
               }
               else
               {
                  numeric_str[j] = '\0';
                  id.new_file_size = (off_t)str2offt(numeric_str, NULL, 16);
               }

               if ((search_new_file_size == -1) ||
                   ((gt_lt_sign_new == EQUAL_SIGN) &&
                    (id.new_file_size == search_new_file_size)) ||
                   ((gt_lt_sign_new == LESS_THEN_SIGN) &&
                    (id.new_file_size < search_new_file_size)) ||
                   ((gt_lt_sign_new == GREATER_THEN_SIGN) &&
                    (id.new_file_size > search_new_file_size)) ||
                   ((gt_lt_sign_new == NOT_SIGN) &&
                    (id.new_file_size != search_new_file_size)))
               {
                  ptr++; /* Away with SEPARATOR_CHAR */
               }
               else
               {
                  IGNORE_ENTRY();
               }
            }
            else
            {
               if (j == MAX_DOUBLE_LENGTH)
               {
                  while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
                  {
                     ptr++;
                  }
                  ptr++;
                  id.new_file_size = 0;
               }
               else
               {
                  IGNORE_ENTRY();
               }
            }

            /* Store return code. */
            j = 0;
            while ((*ptr != SEPARATOR_CHAR) &&
                   (*ptr != '\n') && (j < MAX_DOUBLE_LENGTH))
            {
               numeric_str[j] = *ptr;
               j++; ptr++;
            }
            if (*ptr == SEPARATOR_CHAR)
            {
               numeric_str[j] = '\0';
               id.return_code = atoi(numeric_str);
            }
            else
            {
               id.return_code = 0;
               while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
               {
                  ptr++;
               }
            }
            if ((do_search_return_code == NO) ||
                ((gt_lt_sign_rc == EQUAL_SIGN) &&
                 (id.return_code == search_return_code)) ||
                ((gt_lt_sign_rc == LESS_THEN_SIGN) &&
                 (id.return_code < search_return_code)) ||
                ((gt_lt_sign_rc == GREATER_THEN_SIGN) &&
                 (id.return_code > search_return_code)) ||
                ((gt_lt_sign_rc == NOT_SIGN) &&
                 (id.return_code != search_return_code)))
            {
               if (*ptr == SEPARATOR_CHAR)
               {
                  ptr++; /* Away with SEPARATOR_CHAR */
               }
            }
            else
            {
               IGNORE_ENTRY();
            }

            /* Check command executed. */
            if ((search_production_cmd[0] == '\0') ||
                ((search_production_cmd[0] == '*') &&
                 (search_production_cmd[1] == '\0')) ||
                (sfilter(search_production_cmd, ptr, '\n') == 0))
            {
               j = 0;
               while ((*(ptr + j) != '\n') && (j < MAX_DISPLAYED_COMMAND))
               {
                  if ((unsigned char)(*(ptr + j)) < ' ')
                  {
                     *(p_command + j) = '?';
                     unprintable_chars++;
                  }
                  else
                  {
                     *(p_command + j) = *(ptr + j);
                  }
                  j++;
               }
               ptr += j;
            }
            else
            {
               IGNORE_ENTRY();
            }

            /* If necessary, ignore rest of command. */
            while (*ptr != '\n')
            {
               ptr++;
            }
            ptr++;
         }
         else
         {
            IGNORE_ENTRY();
         }

         /* Show ratio. */
         if ((id.ratio_1 == 1) || (id.ratio_1 < 0))
         {
            char first_char;

            if (id.ratio_1 == 1)
            {
               first_char = '1';
            }
            else
            {
               first_char = '?';
            }

            if (id.ratio_2 == 1)
            {
               *(p_ratio + MAX_DISPLAYED_RATIO - 3) = first_char;
               *(p_ratio + MAX_DISPLAYED_RATIO - 2) = ':';
               *(p_ratio + MAX_DISPLAYED_RATIO - 1) = '1';
            }
            else if (id.ratio_2 < 10)
                 {
                    *(p_ratio + MAX_DISPLAYED_RATIO - 3) = first_char;
                    *(p_ratio + MAX_DISPLAYED_RATIO - 2) = ':';
                    *(p_ratio + MAX_DISPLAYED_RATIO - 1) = id.ratio_2 + '0';
                 }
            else if (id.ratio_2 < 100)
                 {
                    *(p_ratio + MAX_DISPLAYED_RATIO - 4) = first_char;
                    *(p_ratio + MAX_DISPLAYED_RATIO - 3) = ':';
                    *(p_ratio + MAX_DISPLAYED_RATIO - 2) = (id.ratio_2 / 10) + '0';
                    *(p_ratio + MAX_DISPLAYED_RATIO - 1) = (id.ratio_2 % 10)  + '0';
                 }
            else if (id.ratio_2 < 1000)
                 {
                    *(p_ratio + MAX_DISPLAYED_RATIO - 5) = first_char;
                    *(p_ratio + MAX_DISPLAYED_RATIO - 4) = ':';
                    *(p_ratio + MAX_DISPLAYED_RATIO - 3) = (id.ratio_2 / 100) + '0';
                    *(p_ratio + MAX_DISPLAYED_RATIO - 2) = ((id.ratio_2 / 10) % 10)  + '0';
                    *(p_ratio + MAX_DISPLAYED_RATIO - 1) = (id.ratio_2 % 10)  + '0';
                 }
            else if (id.ratio_2 < 10000)
                 {
                    *(p_ratio + MAX_DISPLAYED_RATIO - 6) = first_char;
                    *(p_ratio + MAX_DISPLAYED_RATIO - 5) = ':';
                    *(p_ratio + MAX_DISPLAYED_RATIO - 4) = (id.ratio_2 / 1000) + '0';
                    *(p_ratio + MAX_DISPLAYED_RATIO - 3) = ((id.ratio_2 / 100) % 10) + '0';
                    *(p_ratio + MAX_DISPLAYED_RATIO - 2) = ((id.ratio_2 / 10) % 10)  + '0';
                    *(p_ratio + MAX_DISPLAYED_RATIO - 1) = (id.ratio_2 % 10)  + '0';
                 }
            else if (id.ratio_2 < 100000)
                 {
                    *(p_ratio + MAX_DISPLAYED_RATIO - 7) = first_char;
                    *(p_ratio + MAX_DISPLAYED_RATIO - 6) = ':';
                    *(p_ratio + MAX_DISPLAYED_RATIO - 5) = (id.ratio_2 / 10000) + '0';
                    *(p_ratio + MAX_DISPLAYED_RATIO - 4) = ((id.ratio_2 / 1000) % 10) + '0';
                    *(p_ratio + MAX_DISPLAYED_RATIO - 3) = ((id.ratio_2 / 100) % 10) + '0';
                    *(p_ratio + MAX_DISPLAYED_RATIO - 2) = ((id.ratio_2 / 10) % 10)  + '0';
                    *(p_ratio + MAX_DISPLAYED_RATIO - 1) = (id.ratio_2 % 10)  + '0';
                 }
            else if (id.ratio_2 < 0)
                 {
                    *(p_ratio + MAX_DISPLAYED_RATIO - 3) = first_char;
                    *(p_ratio + MAX_DISPLAYED_RATIO - 2) = ':';
                    *(p_ratio + MAX_DISPLAYED_RATIO - 1) = '?';
                 }
                 else
                 {
                    *(p_ratio + MAX_DISPLAYED_RATIO - 3) = first_char;
                    *(p_ratio + MAX_DISPLAYED_RATIO - 2) = ':';
                    *(p_ratio + MAX_DISPLAYED_RATIO - 1) = '>';
                 }
         }
         else
         {
            if ((id.ratio_2 == 1) || (id.ratio_2 < 0))
            {
               if (id.ratio_2 == 1)
               {
                  *(p_ratio + MAX_DISPLAYED_RATIO - 1) = '1';
               }
               else
               {
                  *(p_ratio + MAX_DISPLAYED_RATIO - 1) = '?';
               }
               *(p_ratio + MAX_DISPLAYED_RATIO - 2) = ':';

               if (id.ratio_1 < 10)
               {
                  *(p_ratio + MAX_DISPLAYED_RATIO - 3) = id.ratio_1 + '0';
               }
               else if (id.ratio_1 < 100)
                    {
                       *(p_ratio + MAX_DISPLAYED_RATIO - 3) = (id.ratio_1 % 10) + '0';
                       *(p_ratio + MAX_DISPLAYED_RATIO - 4) = (id.ratio_1 / 10) + '0';
                    }
               else if (id.ratio_1 < 1000)
                    {
                       *(p_ratio + MAX_DISPLAYED_RATIO - 3) = (id.ratio_1 % 10) + '0';
                       *(p_ratio + MAX_DISPLAYED_RATIO - 4) = ((id.ratio_1 / 10) % 10) + '0';
                       *(p_ratio + MAX_DISPLAYED_RATIO - 5) = (id.ratio_1 / 100) + '0';
                    }
               else if (id.ratio_1 < 10000)
                    {
                       *(p_ratio + MAX_DISPLAYED_RATIO - 3) = (id.ratio_1 % 10) + '0';
                       *(p_ratio + MAX_DISPLAYED_RATIO - 4) = ((id.ratio_1 / 10) % 10) + '0';
                       *(p_ratio + MAX_DISPLAYED_RATIO - 5) = ((id.ratio_1 / 100) % 10) + '0';
                       *(p_ratio + MAX_DISPLAYED_RATIO - 6) = (id.ratio_1 / 1000) + '0';
                    }
               else if (id.ratio_1 < 100000)
                    {
                       *(p_ratio + MAX_DISPLAYED_RATIO - 3) = (id.ratio_1 % 10) + '0';
                       *(p_ratio + MAX_DISPLAYED_RATIO - 4) = ((id.ratio_1 / 10) % 10) + '0';
                       *(p_ratio + MAX_DISPLAYED_RATIO - 5) = ((id.ratio_1 / 100) % 10) + '0';
                       *(p_ratio + MAX_DISPLAYED_RATIO - 6) = ((id.ratio_1 / 1000) % 10) + '0';
                       *(p_ratio + MAX_DISPLAYED_RATIO - 7) = (id.ratio_1 / 10000) + '0';
                    }
                    else
                    {
                       *(p_ratio + MAX_DISPLAYED_RATIO - 3) = '>';
                    }
            }
            else
            {
               *(p_ratio + MAX_DISPLAYED_RATIO - 1) = 'n';
               *(p_ratio + MAX_DISPLAYED_RATIO - 2) = ':';
               *(p_ratio + MAX_DISPLAYED_RATIO - 3) = 'n';
            }
         }

         /* Show original file size. */
         if (id.orig_file_size == -1)
         {
            *(p_orig_file_size + MAX_DISPLAYED_FILE_SIZE - 1) = '?';
         }
         else
         {
            print_file_size(p_orig_file_size, id.orig_file_size);
            orig_file_size += id.orig_file_size;
         }

         /* Show new file size. */
         if (id.new_file_size != -1)
         {
            print_file_size(p_new_file_size, id.new_file_size);
            new_file_size += id.new_file_size;
         }

         /* Show return code. */
         if (id.return_code == 0)
         {
            *(p_rc + MAX_DISPLAYED_RC - 1) = '0';
         }
         else if (id.return_code < 0)
              {
                 if (id.return_code > -10)
                 {
                    *(p_rc + MAX_DISPLAYED_RC - 1) = -id.return_code + '0';
                    *(p_rc + MAX_DISPLAYED_RC - 2) = '-';
                 }
                 else if (id.return_code > -100)
                      {
                         *(p_rc + MAX_DISPLAYED_RC - 1) = (-id.return_code % 10) + '0';
                         *(p_rc + MAX_DISPLAYED_RC - 2) = (-id.return_code / 10) + '0';
                         *(p_rc + MAX_DISPLAYED_RC - 3) = '-';
                      }
                 else if (id.return_code > -1000)
                      {
                         *(p_rc + MAX_DISPLAYED_RC - 1) = (-id.return_code % 10) + '0';
                         *(p_rc + MAX_DISPLAYED_RC - 2) = ((-id.return_code / 10) % 10) + '0';
                         *(p_rc + MAX_DISPLAYED_RC - 3) = (-id.return_code / 100) + '0';
                         *(p_rc + MAX_DISPLAYED_RC - 4) = '-';
                      }
                      else
                      {
                         *(p_rc + MAX_DISPLAYED_RC - 1) = '>';
                         *(p_rc + MAX_DISPLAYED_RC - 2) = '-';
                      }
              }
         else if (id.return_code < 10)
              {
                 *(p_rc + MAX_DISPLAYED_RC - 1) = id.return_code + '0';
              }
         else if (id.return_code < 100)
              {
                 *(p_rc + MAX_DISPLAYED_RC - 2) = (id.return_code / 10) + '0';
                 *(p_rc + MAX_DISPLAYED_RC - 1) = (id.return_code % 10) + '0';
              }
         else if (id.return_code < 1000)
              {
                 *(p_rc + MAX_DISPLAYED_RC - 3) = (id.return_code / 100) + '0';
                 *(p_rc + MAX_DISPLAYED_RC - 2) = ((id.return_code / 10) % 10) + '0';
                 *(p_rc + MAX_DISPLAYED_RC - 1) = (id.return_code % 10) + '0';
              }
              else
              {
                 *(p_rc + MAX_DISPLAYED_RC - 1) = '?';
              }

         /* Show production time. */
         if (id.production_time == 0.000)
         {
             *(p_prod_time + MAX_DISPLAYED_PROD_TIME - 1) = '0';
             *(p_prod_time + MAX_DISPLAYED_PROD_TIME - 2) = '0';
             *(p_prod_time + MAX_DISPLAYED_PROD_TIME - 3) = '0';
             *(p_prod_time + MAX_DISPLAYED_PROD_TIME - 4) = '.';
             *(p_prod_time + MAX_DISPLAYED_PROD_TIME - 5) = '0';
         }
         else if (id.production_time < 0.0)
              {
                 *(p_prod_time + MAX_DISPLAYED_PROD_TIME - 1) = '?';
              }
         else if (id.production_time < 10000000.0)
              {
                 if (id.production_time < 1000.0)
                 {
                    (void)snprintf(p_prod_time, MAX_DISPLAYED_PROD_TIME + 1,
                                   "%*.3f", MAX_DISPLAYED_PROD_TIME,
                                   id.production_time);
                 }
                 else
                 {
                    (void)snprintf(p_prod_time, MAX_DISPLAYED_PROD_TIME + 1,
                                   "%*.0f", MAX_DISPLAYED_PROD_TIME,
                                   id.production_time);
                 }
                 *(p_prod_time + MAX_DISPLAYED_PROD_TIME) = ' ';
              }
              else
              {
                 *(p_prod_time + MAX_DISPLAYED_PROD_TIME - 1) = '>';
              }

         /* Show cpu time. */
         if (id.cpu_time == 0.000000)
         {
             *(p_cpu_time + MAX_DISPLAYED_CPU_TIME - 1) = '0';
             *(p_cpu_time + MAX_DISPLAYED_CPU_TIME - 2) = '0';
             *(p_cpu_time + MAX_DISPLAYED_CPU_TIME - 3) = '0';
             *(p_cpu_time + MAX_DISPLAYED_CPU_TIME - 4) = '.';
             *(p_cpu_time + MAX_DISPLAYED_CPU_TIME - 5) = '0';
         }
         else if (id.cpu_time < 0.0)
              {
                 *(p_cpu_time + MAX_DISPLAYED_CPU_TIME - 1) = '?';
              }
         else if (id.cpu_time < 10000000.0)
              {
                 if (id.cpu_time < 1000.0)
                 {
                    (void)snprintf(p_cpu_time, MAX_DISPLAYED_CPU_TIME + 1,
                                   "%*.3f", MAX_DISPLAYED_CPU_TIME,
                                   id.cpu_time);
                 }
                 else
                 {
                    (void)snprintf(p_cpu_time, MAX_DISPLAYED_CPU_TIME + 1,
                                   "%*.0f", MAX_DISPLAYED_CPU_TIME,
                                   id.cpu_time);
                 }
              }
              else
              {
                 *(p_cpu_time + MAX_DISPLAYED_PROD_TIME - 1) = '>';
              }

         prod_time += id.production_time;
         if (id.cpu_time != -1.0)
         {
            cpu_time += id.cpu_time;
         }
         str_list[i] = XmStringCreateLocalized(line);
         item_counter++;
      }

      loops++;

      /* Display what we have in buffer. */
      display_data(i, first_date_found, id.time_when_produced);

      /* Check if user has done anything. */
      if ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL)
      {
         prev_time_val = now;
         CHECK_INTERRUPT();
      }

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

#ifndef LESSTIF_WORKAROUND
   if (unmanaged == YES)
   {
      XtManageChild(listbox_w);
   }
#endif

   il[file_no].no_of_items = item_counter;

   return;
}


/*--------------------------- display_data() ----------------------------*/
static void
display_data(int    i,
             time_t first_date_found,
             time_t time_when_produced)
{
   register int j;
   static int   rotate;
   char         status_message[MAX_MESSAGE_LENGTH];
   XmString     xstr;
   XExposeEvent xeev;
   Dimension    w, h;

   XmListAddItemsUnselected(listbox_w, str_list, i, 0);
   for (j = 0; j < i; j++)
   {
      XmStringFree(str_list[j]);
   }
   total_no_files += i;
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

   calculate_summary(summary_str, first_date_found, time_when_produced,
                     total_no_files, orig_file_size, new_file_size,
                     prod_time, cpu_time);
   (void)strcpy(total_summary_str, summary_str);
   all_list_items = total_no_files;

   xeev.type = Expose;
   xeev.display = display;
   xeev.window = main_window;
   xeev.x = 0; xeev.y = 0;
   XtVaGetValues(summarybox_w, XmNwidth, &w, XmNheight, &h, NULL);
   xeev.width = w; xeev.height = h;
   xstr = XmStringCreateLtoR(summary_str, XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(summarybox_w, XmNlabelString, xstr, NULL);
   (XtClass(summarybox_w))->core_class.expose(summarybox_w, (XEvent *)&xeev, NULL);
   XmStringFree(xstr);
   xstr = XmStringCreateLtoR(status_message, XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(statusbox_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);

   return;
}
