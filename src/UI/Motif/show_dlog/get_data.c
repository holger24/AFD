/*
 *  get_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_data - searches output log files for data
 **
 ** SYNOPSIS
 **   void get_data(void)
 **
 ** DESCRIPTION
 **   This function searches for the selected data in the output
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
 **   22.02.1998 H.Kiehl Created
 **   17.02.1999 H.Kiehl Multiple recipients.
 **   13.04.2002 H.Kiehl Separator definable via SEPARATOR_CHAR.
 **   15.06.2005 H.Kiehl Added duplicate check.
 **   15.08.2005 H.Kiehl Added Exec type.
 **
 */
DESCR__E_M3


#include <stdio.h>        /* sprintf()                                   */
#include <string.h>       /* strerror()                                  */
#include <stdlib.h>       /* malloc(), realloc(), free(), strtod()       */
#include <time.h>         /* time()                                      */
#include <sys/time.h>     /* struct tm                                   */
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
#include "show_dlog.h"
#include "sdr_str.h"

/* #define _MACRO_DEBUG */

/* External global variables. */
extern Display          *display;
extern Window           main_window;
extern Widget           appshell, /* CHECK_INTERRUPT() */
                        listbox_w,
                        scrollbar_w,
                        special_button_w,
                        statusbox_w,
                        summarybox_w;
extern int              file_name_toggle_set,
                        radio_set,
                        gt_lt_sign,
                        special_button_flag,
                        file_name_length,
                        log_date_length,
                        max_delete_log_files,
                        max_hostname_length,
                        no_of_log_files,
                        no_of_search_dirs,
                        no_of_search_dirids,
                        no_of_search_file_names,
                        no_of_search_hosts,
                        *search_dir_length;
extern unsigned int     all_list_items,
                        *search_dirid;
extern XT_PTR_TYPE      dr_toggles_set;
extern size_t           search_file_size;
extern time_t           start_time_val,
                        end_time_val;
extern char             *p_work_dir,
                        **search_file_name,
                        **search_dir,
                        *search_dir_filter,
                        **search_recipient,
                        summary_str[],
                        total_summary_str[];
extern struct item_list *il;
extern struct info_data id;
extern struct sol_perm  perm;

/* Local global variables. */
static unsigned int     total_no_files,
                        unprintable_chars;
static time_t           local_start_time,
                        local_end_time,
                        first_date_found;
static off_t            file_size;
static double           trans_time;
static char             line[MAX_OUTPUT_LINE_LENGTH + SHOW_LONG_FORMAT + 2],
                        log_file[MAX_PATH_LENGTH],
                        *p_delete_reason,
                        *p_file_name,
                        *p_file_size,
                        *p_host_name,
                        *p_log_file,
                        *p_proc_user;
static XmStringTable    str_list;

/* Local function prototypes. */
static char             *search_time(char *, time_t, time_t, time_t, size_t);
static void             display_data(int, time_t, time_t),
                        extract_data(char *, int),
                        no_criteria(register char *, char *, int, char *),
                        file_name_only(register char *, char *, int, char *),
                        file_size_only(register char *, char *, int, char *),
                        file_name_and_size(register char *, char *, int,
                                           char *),
                        recipient_only(register char *, char *, int, char *),
                        file_name_and_recipient(register char *, char *, int,
                                                char *),
                        file_size_and_recipient(register char *, char *, int,
                                                char *),
                        file_name_size_recipient(register char *, char *, int,
                                                 char *);

#define REALLOC_OFFSET_BUFFER()                                  \
        {                                                        \
           if (((loops + 1) * LINES_BUFFERED - item_counter) <= LINES_BUFFERED)\
           {                                                     \
              int new_int_size,                                  \
                  new_char_size;                                 \
                                                                 \
              new_char_size = (loops + 1) * LINES_BUFFERED;      \
              new_int_size = new_char_size * sizeof(int);        \
                                                                 \
              if (((il[file_no].offset = realloc(il[file_no].offset, new_int_size)) == NULL) ||        \
                  ((il[file_no].line_offset = realloc(il[file_no].line_offset, new_int_size)) == NULL))\
              {                                                  \
                 (void)xrec(FATAL_DIALOG, "realloc() error : %s (%s %d)",\
                            strerror(errno), __FILE__, __LINE__);\
                 return;                                         \
              }                                                  \
           }                                                     \
        }
#define IGNORE_ENTRY()         \
        {                      \
           while (*ptr != '\n')\
              ptr++;           \
           ptr++; i--;         \
           continue;           \
        }
#define INSERT_TIME_REASON(reason_pos)                     \
        {                                                  \
           (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length + 1);\
           time_when_transmitted = (time_t)str2timet(ptr_start_line, NULL, 16);\
           if (first_date_found == -1)                     \
           {                                               \
              first_date_found = time_when_transmitted;    \
           }                                               \
           if ((p_ts = localtime(&time_when_transmitted)) == NULL)\
           {                                               \
              IGNORE_ENTRY();                              \
           }                                               \
           else                                            \
           {                                               \
              CONVERT_TIME();                              \
           }                                               \
           (void)memcpy(p_delete_reason, sdrstr[(reason_pos)], MAX_REASON_LENGTH);\
        }
#define COMMON_BLOCK()                                     \
        {                                                  \
           int  count = 0;                                 \
           char job_id_str[MAX_INT_HEX_LENGTH + 1];        \
                                                           \
           ptr++;                                          \
           il[file_no].offset[item_counter] = (int)(ptr - p_start_log_file);\
                                                           \
           while ((*ptr != '\n') && (*ptr != SEPARATOR_CHAR) &&\
                  (count < MAX_INT_HEX_LENGTH))            \
           {                                               \
              job_id_str[count] = *ptr;                    \
              count++; ptr++;                              \
           }                                               \
           if (*ptr != SEPARATOR_CHAR)                     \
           {                                               \
              IGNORE_ENTRY();                              \
           }                                               \
           job_id_str[count] = '\0';                       \
           if (id.offset)                                  \
           {                                               \
              ptr++;                                       \
              id.job_id = (unsigned int)strtoul(job_id_str, NULL, 16);\
              count = 0;                                   \
              while ((*ptr != '\n') && (*ptr != SEPARATOR_CHAR) &&\
                     (count < MAX_INT_HEX_LENGTH))         \
              {                                            \
                 job_id_str[count] = *ptr;                 \
                 count++; ptr++;                           \
              }                                            \
              if (*ptr != SEPARATOR_CHAR)                  \
              {                                            \
                 IGNORE_ENTRY();                           \
              }                                            \
              job_id_str[count] = '\0';                    \
              id.dir_id = (unsigned int)strtoul(job_id_str, NULL, 16);\
              ptr++;                                       \
              while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))\
              {                                            \
                 ptr++; /* Ignore unique ID. */            \
              }                                            \
           }                                               \
           else                                            \
           {                                               \
              unsigned int tmp_id;                         \
                                                           \
              tmp_id = (unsigned int)strtoul(job_id_str, NULL, 16);\
              if ((id.delete_reason_no == AGE_OUTPUT) ||   \
                  (id.delete_reason_no == NO_MESSAGE_FILE_DEL) ||\
                  (id.delete_reason_no == DUP_OUTPUT))     \
              {                                            \
                 id.job_id = tmp_id;                       \
                 id.dir_id = 0;                            \
              }                                            \
              else                                         \
              {                                            \
                 id.job_id = 0;                            \
                 id.dir_id = tmp_id;                       \
              }                                            \
           }                                               \
                                                           \
           if ((no_of_search_dirs > 0) || (no_of_search_dirids > 0))\
           {                                               \
              int gotcha = NO,                             \
                  kk;                                      \
                                                           \
              id.dir[0] = '\0';                            \
              get_info(GOT_JOB_ID_DIR_ONLY);               \
              count = strlen((char *)id.dir);              \
              if (id.dir[count - 1] != SEPARATOR_CHAR)     \
              {                                            \
                 id.dir[count] = SEPARATOR_CHAR;           \
                 id.dir[count + 1] = '\0';                 \
              }                                            \
              else                                         \
              {                                            \
                 count--;                                  \
              }                                            \
                                                           \
              for (kk = 0; kk < no_of_search_dirids; kk++) \
              {                                            \
                 if (search_dirid[kk] == id.dir_id)        \
                 {                                         \
                    gotcha = YES;                          \
                    break;                                 \
                 }                                         \
              }                                            \
              if (gotcha == NO)                            \
              {                                            \
                 for (kk = 0; kk < no_of_search_dirs; kk++)\
                 {                                         \
                    if (search_dir_filter[kk] == YES)      \
                    {                                      \
                       if (sfilter(search_dir[kk], (char *)id.dir, SEPARATOR_CHAR) == 0)\
                       {                                   \
                          gotcha = YES;                    \
                          break;                           \
                       }                                   \
                    }                                      \
                    else                                   \
                    {                                      \
                       if (search_dir_length[kk] == count) \
                       {                                   \
                          if (strncmp(search_dir[kk], (char *)id.dir, count) == 0)\
                          {                                \
                             gotcha = YES;                 \
                             break;                        \
                          }                                \
                       }                                   \
                    }                                      \
                 }                                         \
              }                                            \
              if (gotcha == NO)                            \
              {                                            \
                 IGNORE_ENTRY();                           \
              }                                            \
           }                                               \
                                                           \
           if (*ptr == SEPARATOR_CHAR)                     \
           {                                               \
              ptr++;                                       \
              j = 0;                                       \
              while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n') &&\
                     (j < MAX_PROC_USER_LENGTH))           \
              {                                            \
                 *(p_proc_user + j) = *ptr;                \
                 ptr++; j++;                               \
              }                                            \
           }                                               \
           while (*ptr != '\n')                            \
           {                                               \
              ptr++;                                       \
           }                                               \
           item_counter++;                                 \
           str_list[i] = XmStringCreateLocalized(line);    \
           ptr++;                                          \
        }
#define CHECK_LIST_LIMIT()                                 \
        {                                                  \
           if ((perm.list_limit > 0) && (item_counter > perm.list_limit))\
           {                                               \
              char msg_buffer[40];                         \
                                                           \
              (void)sprintf(msg_buffer, "List limit (%d) reached!",\
                            perm.list_limit);              \
              show_message(statusbox_w, msg_buffer);       \
              break;                                       \
           }                                               \
        }
#define FILE_SIZE_AND_RECIPIENT(reason_pos)                            \
        {                                                              \
           int ii;                                                     \
                                                                       \
           for (ii = 0; ii < no_of_search_hosts; ii++)                 \
           {                                                           \
              if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)\
              {                                                        \
                 break;                                                \
              }                                                        \
           }                                                           \
           if (ii == no_of_search_hosts)                               \
           {                                                           \
              IGNORE_ENTRY();                                          \
           }                                                           \
           else                                                        \
           {                                                           \
              ptr += log_date_length + 1 + max_hostname_length + 3 + id.offset;\
              while (*ptr != SEPARATOR_CHAR)                           \
              {                                                        \
                 ptr++;                                                \
              }                                                        \
              ptr++;                                                   \
              if (*ptr == '/')                                         \
              {                                                        \
                 /* Ignore the remote file name. */                    \
                 while (*ptr != SEPARATOR_CHAR)                        \
                 {                                                     \
                    ptr++;                                             \
                 }                                                     \
                 ptr++;                                                \
              }                                                        \
                                                                       \
              j = 0;                                                   \
              while (*ptr != SEPARATOR_CHAR)                           \
              {                                                        \
                 ptr++; j++;                                           \
              }                                                        \
              tmp_file_size = (off_t)str2offt((ptr - j), NULL, 16);    \
              if ((gt_lt_sign == EQUAL_SIGN) &&                        \
                  (tmp_file_size == search_file_size))                 \
              {                                                        \
                 (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length + 1);\
                 (void)memcpy(p_delete_reason, sdrstr[(reason_pos)], MAX_REASON_LENGTH);\
                                                                       \
                 /* Write file size. */                                \
                 print_file_size(p_file_size, tmp_file_size);          \
              }                                                        \
              else if ((gt_lt_sign == LESS_THEN_SIGN) &&               \
                       (tmp_file_size < search_file_size))             \
                   {                                                   \
                      (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length + 1);\
                      (void)memcpy(p_delete_reason, sdrstr[(reason_pos)], MAX_REASON_LENGTH);\
                                                                       \
                      /* Write file size. */                           \
                      print_file_size(p_file_size, tmp_file_size);     \
                   }                                                   \
              else if ((gt_lt_sign == GREATER_THEN_SIGN) &&            \
                       (tmp_file_size > search_file_size))             \
                   {                                                   \
                      (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length + 1);\
                      (void)memcpy(p_delete_reason, sdrstr[(reason_pos)], MAX_REASON_LENGTH);\
                                                                       \
                      /* Write file size. */                           \
                      print_file_size(p_file_size, tmp_file_size);     \
                   }                                                   \
              else if ((gt_lt_sign == NOT_SIGN) &&                     \
                       (tmp_file_size != search_file_size))            \
                   {                                                   \
                      (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length + 1);\
                      (void)memcpy(p_delete_reason, sdrstr[(reason_pos)], MAX_REASON_LENGTH);\
                                                                       \
                      /* Write file size. */                           \
                      print_file_size(p_file_size, tmp_file_size);     \
                   }                                                   \
                   else                                                \
                   {                                                   \
                      IGNORE_ENTRY();                                  \
                   }                                                   \
           }                                                           \
        }

#define FILE_NAME_AND_RECIPIENT(reason_pos)                            \
        {                                                              \
            int ii;                                                    \
                                                                       \
            for (ii = 0; ii < no_of_search_hosts; ii++)                \
            {                                                          \
               if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)\
               {                                                       \
                  break;                                               \
               }                                                       \
            }                                                          \
            if (ii == no_of_search_hosts)                              \
            {                                                          \
               IGNORE_ENTRY();                                         \
            }                                                          \
            else                                                       \
            {                                                          \
               int iii,                                                \
                   match_found = -1;                                   \
                                                                       \
               ptr += log_date_length + 1 + max_hostname_length + 3 + id.offset;\
               for (iii = 0; iii < no_of_search_file_names; iii++)     \
               {                                                       \
                  if ((ret = sfilter(search_file_name[iii], ptr, SEPARATOR_CHAR)) == 0)\
                  {                                                    \
                     if (search_file_name[iii][0] != '!')              \
                     {                                                 \
                        il[file_no].line_offset[item_counter] = (int)(ptr_start_line + log_date_length + 1 + max_hostname_length + 3 + id.offset - p_start_log_file);\
                        INSERT_TIME_REASON((reason_pos));              \
                        j = 0;                                         \
                        while ((*(ptr + j) != SEPARATOR_CHAR) && (j < file_name_length))\
                        {                                              \
                           if ((unsigned char)(*(ptr + j)) < ' ')      \
                           {                                           \
                              *(p_file_name + j) = '?';                \
                              unprintable_chars++;                     \
                           }                                           \
                           else                                        \
                           {                                           \
                              *(p_file_name + j) = *(ptr + j);         \
                           }                                           \
                           j++;                                        \
                        }                                              \
                        ptr += j;                                      \
                        match_found = iii;                             \
                        break;                                         \
                     }                                                 \
                  }                                                    \
                  else if (ret == 1)                                   \
                       {                                               \
                          break;                                       \
                       }                                               \
               }                                                       \
               if (match_found == -1)                                  \
               {                                                       \
                  IGNORE_ENTRY();                                      \
               }                                                       \
            }                                                          \
        }

#define FILE_SIZE_ONLY(reason_pos)                                     \
        {                                                              \
           ptr += log_date_length + 1 + max_hostname_length + 3 + id.offset;\
           while (*ptr != SEPARATOR_CHAR)                              \
           {                                                           \
              ptr++;                                                   \
           }                                                           \
           ptr++;                                                      \
           if (*ptr == '/')                                            \
           {                                                           \
              /* Ignore the remote file name. */                       \
              while (*ptr != SEPARATOR_CHAR)                           \
              {                                                        \
                 ptr++;                                                \
              }                                                        \
              ptr++;                                                   \
           }                                                           \
                                                                       \
           j = 0;                                                      \
           while (*ptr != SEPARATOR_CHAR)                              \
           {                                                           \
              ptr++; j++;                                              \
           }                                                           \
           tmp_file_size = (off_t)str2offt((ptr - j), NULL, 16);       \
           if ((gt_lt_sign == EQUAL_SIGN) &&                           \
               (tmp_file_size == search_file_size))                    \
           {                                                           \
              (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length + 1);\
              (void)memcpy(p_delete_reason, sdrstr[(reason_pos)], MAX_REASON_LENGTH);\
                                                                       \
              /* Write file size. */                                   \
              print_file_size(p_file_size, tmp_file_size);             \
           }                                                           \
           else if ((gt_lt_sign == LESS_THEN_SIGN) &&                  \
                    (tmp_file_size < search_file_size))                \
                {                                                      \
                   (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length + 1);\
                   (void)memcpy(p_delete_reason, sdrstr[(reason_pos)], MAX_REASON_LENGTH);\
                                                                       \
                   /* Write file size. */                              \
                   print_file_size(p_file_size, tmp_file_size);        \
                }                                                      \
           else if ((gt_lt_sign == GREATER_THEN_SIGN) &&               \
                    (tmp_file_size > search_file_size))                \
                {                                                      \
                   (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length + 1);\
                   (void)memcpy(p_delete_reason, sdrstr[(reason_pos)], MAX_REASON_LENGTH);\
                                                                       \
                   /* Write file size. */                              \
                   print_file_size(p_file_size, tmp_file_size);        \
                }                                                      \
           else if ((gt_lt_sign == NOT_SIGN) &&                        \
                    (tmp_file_size != search_file_size))               \
                {                                                      \
                   (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length + 1);\
                   (void)memcpy(p_delete_reason, sdrstr[(reason_pos)], MAX_REASON_LENGTH);\
                                                                       \
                   /* Write file size. */                              \
                   print_file_size(p_file_size, tmp_file_size);        \
                }                                                      \
                else                                                   \
                {                                                      \
                   IGNORE_ENTRY();                                     \
                }                                                      \
        }


/*############################### get_data() ############################*/
void
get_data(void)
{
   int          i,
                j,
                end_file_no = -1,
                start_file_no = -1;
   time_t       end,
                start;
   char         status_message[MAX_MESSAGE_LENGTH];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif
   XmString     xstr;

   /* Prepare log file name. */
   p_log_file = log_file;
   no_of_log_files = max_delete_log_files;
   p_log_file += sprintf(log_file, "%s%s/%s", p_work_dir, LOG_DIR,
                         DELETE_BUFFER_FILE);

   local_start_time = start_time_val;
   local_end_time   = end_time_val;

   for (i = 0; i < no_of_log_files; i++)
   {
      (void)sprintf(p_log_file, "%d", i);
#ifdef HAVE_STATX
      if (statx(0, log_file, AT_STATX_SYNC_AS_STAT,
                STATX_MTIME, &stat_buf) == 0)
#else
      if (stat(log_file, &stat_buf) == 0)
#endif
      {
#ifdef HAVE_STATX
         if (((stat_buf.stx_mtime.tv_sec + SWITCH_FILE_TIME) >= local_start_time) ||
             (start_file_no == -1))
#else
         if (((stat_buf.st_mtime + SWITCH_FILE_TIME) >= local_start_time) ||
             (start_file_no == -1))
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

   if ((str_list = (XmStringTable)XtMalloc(LINES_BUFFERED * sizeof(XmString))) == NULL)
   {
      (void)xrec(FATAL_DIALOG, "XtMalloc() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }

   /* Allocate memory for item list. */
   no_of_log_files = start_file_no - end_file_no + 1;
   if ((il = malloc((no_of_log_files + 1) * sizeof(struct item_list))) == NULL)
   {
      (void)xrec(FATAL_DIALOG, "malloc() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }
   for (i = 0; i < no_of_log_files; i++)
   {
      il[i].fp = NULL;
      il[i].no_of_items = 0;
      il[i].offset = NULL;
      il[i].line_offset = NULL;
   }

   /* Initialise all pointer in line. */
   p_file_name     = line + 16;
   p_file_size     = p_file_name + file_name_length + 1;
   p_host_name     = p_file_size + MAX_DISPLAYED_FILE_SIZE + 2;
   p_delete_reason = p_host_name + MAX_HOSTNAME_LENGTH + 1;
   p_proc_user     = p_delete_reason + MAX_REASON_LENGTH + 1;
   *(line + MAX_OUTPUT_LINE_LENGTH + file_name_length) = '\0';

   special_button_flag = STOP_BUTTON;
   xstr = XmStringCreateLtoR("Stop", XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);

   summary_str[0]  = ' ';
   summary_str[1]  = '\0';
   SHOW_SUMMARY_DATA();
   (void)strcpy(status_message, "Searching  -");
   SHOW_MESSAGE();
   CHECK_INTERRUPT();

   start = time(NULL);
   file_size = 0;
   trans_time = 0.0;
   total_no_files = 0;
   first_date_found = -1;
   unprintable_chars = 0;
   j = 0;
   for (i = start_file_no;
        ((i >= end_file_no) && (special_button_flag != STOP_BUTTON_PRESSED));
        i--, j++)
   {
      (void)sprintf(p_log_file, "%d", i);
      (void)extract_data(log_file, j);
      if ((perm.list_limit > 0) && (total_no_files >= perm.list_limit))
      {
         break;
      }
   }
   end = time(NULL);

   if ((perm.list_limit == 0) || (total_no_files < perm.list_limit))
   {
      if (total_no_files == 0)
      {
         i = sprintf(status_message, "No data found. ");
      }
      else
      {
         i = 0;
      }
#if SIZEOF_TIME_T == 4
      i += sprintf(status_message + i, "Search time: %lds",
#else
      i += sprintf(status_message + i, "Search time: %llds",
#endif
                    (pri_time_t)(end - start));
      if (unprintable_chars > 0)
      {
         (void)sprintf(status_message + i, " (%u unprintable chars!)",
                       unprintable_chars);
      }
      SHOW_MESSAGE();
   }

   special_button_flag = SEARCH_BUTTON;
   xstr = XmStringCreateLtoR("Search", XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);
   XtFree((char *)str_list);

   return;
}


/*+++++++++++++++++++++++++++++ extract_data() ++++++++++++++++++++++++++*/
static void
extract_data(char *current_log_file, int file_no)
{
   int           fd;
   time_t        earliest_entry,
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
             STATX_SIZE, &stat_buf) == -1)
#else
   if (stat(current_log_file, &stat_buf) == -1)
#endif
   {
      if (errno == ENOENT)
      {
         /* For some reason the file is not there. So lets */
         /* assume we have found nothing.                  */
         return;
      }
      else
      {
         (void)xrec(WARN_DIALOG, "Failed to access %s : %s (%s %d)",
                    current_log_file, strerror(errno), __FILE__, __LINE__);
         return;
      }
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

   if ((fd = open(current_log_file, O_RDONLY)) < 0)
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

   /*
    * Now we have the source data in the buffer 'src'. Lets
    * search for the stuff the user wants to see. First
    * collect this data in the buffer 'dst' and only after
    * we have found all data will 'dst' be displayed. The
    * destination buffer 'dst' will be realloced in sizes
    * 10 * MAX_OUTPUT_LINE_LENGTH characters. Hope this will reduce
    * memory usage a little bit.
    */

   /* Get latest entry. */
#ifdef HAVE_STATX
   tmp_ptr = src + stat_buf.stx_size - 2;
#else
   tmp_ptr = src + stat_buf.st_size - 2;
#endif
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
         ptr = tmp_ptr;
      }
   } while ((*ptr == '#') && (src != tmp_ptr));

   if (*ptr == '#')
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
   latest_entry = (time_t)str2timet(ptr, NULL, 16);

   /* Get earliest entry. */
   ptr = src;
   while (*ptr == '#')
   {
      while (*ptr != '\n')
      {
#ifdef HAVE_STATX
         if (ptr == (src + stat_buf.stx_size))
#else
         if (ptr == (src + stat_buf.st_size))
#endif
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
         ptr++;
      }
      ptr++;
   }
   earliest_entry = (time_t)str2timet(ptr, NULL, 16);

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
   ptr = ptr_start;
   if ((no_of_search_file_names == 0) && (search_file_size == -1) &&
       (no_of_search_hosts == 0))
   {
      no_criteria(ptr_start, ptr_end, file_no, src);
   }
   else if ((no_of_search_file_names != 0) && (search_file_size == -1) &&
            (no_of_search_hosts == 0))
        {
           file_name_only(ptr_start, ptr_end, file_no, src);
        }
   else if ((no_of_search_file_names == 0) && (search_file_size != -1) &&
            (no_of_search_hosts == 0))
        {
           file_size_only(ptr_start, ptr_end, file_no, src);
        }
   else if ((no_of_search_file_names != 0) && (search_file_size != -1) &&
            (no_of_search_hosts == 0))
        {
           file_name_and_size(ptr_start, ptr_end, file_no, src);
        }
   else if ((no_of_search_file_names == 0) && (search_file_size == -1) &&
            (no_of_search_hosts != 0))
        {
           recipient_only(ptr_start, ptr_end, file_no, src);
        }
   else if ((no_of_search_file_names != 0) && (search_file_size == -1) &&
            (no_of_search_hosts != 0))
        {
           file_name_and_recipient(ptr_start, ptr_end, file_no, src);
        }
   else if ((no_of_search_file_names == 0) && (search_file_size != -1) &&
            (no_of_search_hosts != 0))
        {
           file_size_and_recipient(ptr_start, ptr_end, file_no, src);
        }
   else if ((no_of_search_file_names != 0) && (search_file_size != -1) &&
            (no_of_search_hosts != 0))
        {
           file_name_size_recipient(ptr_start, ptr_end, file_no, src);
        }
        else
        {
           (void)xrec(FATAL_DIALOG, "What's this!? Impossible! (%s %d)",
                      __FILE__, __LINE__);
           return;
        }

   /* Free all memory we have allocated. */
   get_info_free();
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
   else if ((search_time_val > 0) && (earliest_entry > search_time_val))
        {
           return(src);
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
                 ptr -= log_date_length + 1 + max_hostname_length + 3;
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
                 ptr += log_date_length + 1 + max_hostname_length + 3;
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


/*------------------------------ no_criteria() --------------------------*/
/*                               -------------                           */
/* Description: Reads everyting from ptr to ptr_end. It only checks if   */
/*              the transfer type is the correct one.                    */
/*-----------------------------------------------------------------------*/
static void
no_criteria(register char *ptr,
            char          *ptr_end,
            int           file_no,
            char          *p_start_log_file)
{
   register int i,
                j;
   int          item_counter = 0,
                loops = 0;
   time_t       now,
                prev_time_val = 0L,
                time_when_transmitted = 0L;
   off_t        tmp_file_size;
   char         hex_dr_str[4],
                *ptr_start_line;
   struct tm    *p_ts;

   /* The easiest case! */
   hex_dr_str[3] = '\0';
   do
   {
      /* Allocate memory for offset to job ID. */
      REALLOC_OFFSET_BUFFER();

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

         if (*ptr == '#')
         {
            if ((*(ptr + 1) == '!') && (*(ptr + 2) == '#'))
            {
               get_log_type_data(ptr + 3);
            }
            IGNORE_ENTRY();
         }

         ptr_start_line = ptr;

         if (*(ptr_start_line + log_date_length + 1 + max_hostname_length + 2) == SEPARATOR_CHAR)
         {
            id.delete_reason_no = (int)(*(ptr_start_line + log_date_length + 1 + max_hostname_length + 1) - '0');
            id.offset = 0;
         }
         else
         {
            hex_dr_str[0] = *(ptr_start_line + log_date_length + 1 + max_hostname_length + 1);
            hex_dr_str[1] = *(ptr_start_line + log_date_length + 1 + max_hostname_length + 2);
            hex_dr_str[2] = *(ptr_start_line + log_date_length + 1 + max_hostname_length + 3);
            if (hex_dr_str[0] == '0')
            {
               hex_dr_str[0] = ' ';
               if (hex_dr_str[1] == '0')
               {
                  hex_dr_str[1] = ' ';
               }
            }
            id.delete_reason_no = (int)strtol(hex_dr_str, NULL, 16);
            id.offset = 2;
         }
         if ((id.delete_reason_no > MAX_DELETE_REASONS) ||
             ((dr_toggles_set & (1 << id.delete_reason_no)) == 0))
         {
            IGNORE_ENTRY();
         }
#ifdef _MACRO_DEBUG
         (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length + 1);
         time_when_transmitted = (time_t)str2timet(ptr_start_line, NULL, 16);
         if (first_date_found == -1)
         {
            first_date_found = time_when_transmitted;
         }
         if ((p_ts = localtime(&time_when_transmitted)) == NULL)
         {
            IGNORE_ENTRY();
         }
         CONVERT_TIME();
         (void)memcpy(p_delete_reason, sdrstr[id.delete_reason_no],
                      MAX_REASON_LENGTH);
#else
         INSERT_TIME_REASON(id.delete_reason_no);
#endif

         il[file_no].line_offset[item_counter] = (int)(ptr_start_line + log_date_length + 1 + max_hostname_length + 3 + id.offset - p_start_log_file);
         ptr += log_date_length + 1 + max_hostname_length + 3 + id.offset;
         j = 0;
         while ((*(ptr + j) != SEPARATOR_CHAR) && (j < file_name_length))
         {
            if ((unsigned char)(*(ptr + j)) < ' ')
            {
               *(p_file_name + j) = '?';
               unprintable_chars++;
            }
            else
            {
               *(p_file_name + j) = *(ptr + j);
            }
            j++;
         }
         ptr += j;

         (void)memcpy(p_host_name, ptr_start_line + log_date_length + 1,
                      (MAX_HOSTNAME_LENGTH > max_hostname_length) ? max_hostname_length : MAX_HOSTNAME_LENGTH);

         /* If necessary, ignore rest of file name. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;

         /* Write file size. */
         j = 0;
         while (*(ptr + j) != SEPARATOR_CHAR)
         {
            j++;
         }
         ptr += j;
         tmp_file_size = (off_t)str2offt((ptr - j), NULL, 16);
         print_file_size(p_file_size, tmp_file_size);

         /* Write transfer duration, job ID and additional reason. */
         /* Also check if we have to check for directory name.     */
#ifndef _MACRO_DEBUG
         COMMON_BLOCK();
#else
         {
            int  count = 0;
            char job_id_str[MAX_INT_HEX_LENGTH + 1];

            ptr++;
            il[file_no].offset[item_counter] = (int)(ptr - p_start_log_file);

            while ((*ptr != '\n') && (*ptr != SEPARATOR_CHAR) &&
                   (count < MAX_INT_HEX_LENGTH))
            {
               job_id_str[count] = *ptr;
               count++; ptr++;
            }
            if (*ptr != SEPARATOR_CHAR)
            {
               IGNORE_ENTRY();
            }
            job_id_str[count] = '\0';
            if (id.offset)
            {
               ptr++;
               id.job_id = (unsigned int)strtoul(job_id_str, NULL, 16);
               count = 0;
               while ((*ptr != '\n') && (*ptr != SEPARATOR_CHAR) &&
                      (count < MAX_INT_HEX_LENGTH))
               {
                  job_id_str[count] = *ptr;
                  count++; ptr++;
               }
               if (*ptr != SEPARATOR_CHAR)
               {
                  IGNORE_ENTRY();
               }
               job_id_str[count] = '\0';
               id.dir_id = (unsigned int)strtoul(job_id_str, NULL, 16);
               ptr++;
               while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
               {
                  ptr++; /* Ignore unique ID. */
               }
            }
            else
            {
               unsigned int tmp_id;

               tmp_id = (unsigned int)strtoul(job_id_str, NULL, 16);
               if ((id.delete_reason_no == AGE_OUTPUT) ||
                   (id.delete_reason_no == NO_MESSAGE_FILE_DEL) ||
                   (id.delete_reason_no == DUP_OUTPUT))
               {
                  id.job_id = tmp_id;
                  id.dir_id = 0;
               }
               else
               {
                  id.job_id = 0;
                  id.dir_id = tmp_id;
               }
            }

            if ((no_of_search_dirs > 0) || (no_of_search_dirids > 0))
            {
               int gotcha = NO,
                   kk;

               id.dir[0] = '\0';
               get_info(GOT_JOB_ID_DIR_ONLY);
               count = strlen((char *)id.dir);
               if (id.dir[count - 1] != SEPARATOR_CHAR)
               {
                  id.dir[count] = SEPARATOR_CHAR;
                  id.dir[count + 1] = '\0';
               }
               else
               {
                  count--;
               }

               for (kk = 0; kk < no_of_search_dirids; kk++)
               {
                  if (search_dirid[kk] == id.dir_id)
                  {
                     gotcha = YES;
                     break;
                  }
               }
               if (gotcha == NO)
               {
                  for (kk = 0; kk < no_of_search_dirs; kk++)
                  {
                     if (search_dir_filter[kk] == YES)
                     {
                        if (sfilter(search_dir[kk], (char *)id.dir, SEPARATOR_CHAR) == 0)
                        {
                           gotcha = YES;
                           break;
                        }
                     }
                     else
                     {
                        if (search_dir_length[kk] == count)
                        {
                           if (strncmp(search_dir[kk], (char *)id.dir, count) == 0)
                           {
                              gotcha = YES;
                              break;
                           }
                        }
                     }
                  }
               }
               if (gotcha == NO)
               {
                  IGNORE_ENTRY();
               }
            }

            if (*ptr == SEPARATOR_CHAR)
            {
               ptr++;
               j = 0;
               while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n') &&
                      (j < MAX_PROC_USER_LENGTH))
               {
                  *(p_proc_user + j) = *ptr;
                  ptr++; j++;
               }
            }
            while (*ptr != '\n')
            {
               ptr++;
            }
            item_counter++;
            str_list[i] = XmStringCreateLocalized(line);
            ptr++;
         }
#endif
         file_size += tmp_file_size;
      }

      loops++;

      /* Display what we have in buffer. */
      display_data(i, first_date_found, time_when_transmitted);

      /* Check if user has done anything. */
      if ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL)
      {
         prev_time_val = now;
         CHECK_INTERRUPT();
      }

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

   il[file_no].no_of_items = item_counter;

   return;
}


/*---------------------------- file_name_only() -------------------------*/
/*                             ----------------                          */
/* Description: Reads everyting from ptr to ptr_end and search for the   */
/*              file name search_file_name. It also checks if the        */
/*              transfer type is the correct one.                        */
/*-----------------------------------------------------------------------*/
static void
file_name_only(register char *ptr,
               char          *ptr_end,
               int           file_no,
               char          *p_start_log_file)
{
   register int i,
                iii,
                j,
                match_found;
   int          item_counter = 0,
                loops = 0,
                ret;
   time_t       now,
                prev_time_val = 0L,
                time_when_transmitted = 0L;
   double       tmp_file_size;
   char         hex_dr_str[4],
                *ptr_start_line;
   struct tm    *p_ts;

   hex_dr_str[3] = '\0';
   do
   {
      /* Allocate memory for offset to job ID. */
      REALLOC_OFFSET_BUFFER();

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

         if (*ptr == '#')
         {
            if ((*(ptr + 1) == '!') && (*(ptr + 2) == '#'))
            {
               get_log_type_data(ptr + 3);
            }
            IGNORE_ENTRY();
         }

         ptr_start_line = ptr;

         if (*(ptr_start_line + log_date_length + 1 + max_hostname_length + 2) == SEPARATOR_CHAR)
         {
            id.delete_reason_no = (int)(*(ptr_start_line + log_date_length + 1 + max_hostname_length + 1) - '0');
            id.offset = 0;
         }
         else
         {
            hex_dr_str[0] = *(ptr_start_line + log_date_length + 1 + max_hostname_length + 1);
            hex_dr_str[1] = *(ptr_start_line + log_date_length + 1 + max_hostname_length + 2);
            hex_dr_str[2] = *(ptr_start_line + log_date_length + 1 + max_hostname_length + 3);
            if (hex_dr_str[0] == '0')
            {
               hex_dr_str[0] = ' ';
               if (hex_dr_str[1] == '0')
               {
                  hex_dr_str[1] = ' ';
               }
            }
            id.delete_reason_no = (int)strtol(hex_dr_str, NULL, 16);
            id.offset = 2;
         }
         if ((id.delete_reason_no > MAX_DELETE_REASONS) ||
             ((dr_toggles_set & (1 << id.delete_reason_no)) == 0))
         {
            IGNORE_ENTRY();
         }

         ptr += log_date_length + 1 + max_hostname_length + 3 + id.offset;
         match_found = -1;
         for (iii = 0; iii < no_of_search_file_names; iii++)
         {
            if ((ret = sfilter(search_file_name[iii], ptr, SEPARATOR_CHAR)) == 0)
            {
               if (search_file_name[iii][0] != '!')
               {
                  il[file_no].line_offset[item_counter] = (int)(ptr_start_line + log_date_length + 1 + max_hostname_length + 3 + id.offset - p_start_log_file);
                  INSERT_TIME_REASON(id.delete_reason_no);
                  j = 0;
                  while ((*(ptr + j) != SEPARATOR_CHAR) && (j < file_name_length))
                  {
                     if ((unsigned char)(*(ptr + j)) < ' ')
                     {
                        *(p_file_name + j) = '?';
                        unprintable_chars++;
                     }
                     else
                     {
                        *(p_file_name + j) = *(ptr + j);
                     }
                     j++;
                  }
                  ptr += j;
                  match_found = iii;
                  break;
               }
            }
            else if (ret == 1)
                 {
                    break;
                 }
         }
         if (match_found == -1)
         {
            IGNORE_ENTRY();
         }

         (void)memcpy(p_host_name, ptr_start_line + log_date_length + 1,
                      (MAX_HOSTNAME_LENGTH > max_hostname_length) ? max_hostname_length : MAX_HOSTNAME_LENGTH);

         /* If necessary, ignore rest of file name. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;

         /* Write file size. */
         j = 0;
         while (*(ptr + j) != SEPARATOR_CHAR)
         {
            j++;
         }
         ptr += j;
         tmp_file_size = (off_t)str2offt((ptr - j), NULL, 16);
         print_file_size(p_file_size, (off_t)tmp_file_size);

         /* Write transfer duration, job ID and additional reason. */
         /* Also check if we have to check for directory name.     */
         COMMON_BLOCK();
         file_size += tmp_file_size;
      }

      loops++;

      /* Display what we have in buffer. */
      display_data(i, first_date_found, time_when_transmitted);

      /* Check if user has done anything. */
      if ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL)
      {
         prev_time_val = now;
         CHECK_INTERRUPT();
      }

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

   il[file_no].no_of_items = item_counter;

   return;
}


/*---------------------------- file_size_only() -------------------------*/
/*                             ----------------                          */
/* Description: Reads everyting from ptr to ptr_end and search for any   */
/*              file that is <, >, or = search_file_size. It also checks */
/*              if the transfer type is the correct one.                 */
/*-----------------------------------------------------------------------*/
static void
file_size_only(register char *ptr,
               char          *ptr_end,
               int           file_no,
               char          *p_start_log_file)
{
   register int i,
                j;
   int          item_counter = 0,
                loops = 0;
   time_t       now,
                prev_time_val = 0L,
                time_when_transmitted = 0L;
   double       tmp_file_size;
   char         hex_dr_str[4],
                *ptr_start_line;
   struct tm    *p_ts;

   hex_dr_str[3] = '\0';
   do
   {
      /* Allocate memory for offset to job ID. */
      REALLOC_OFFSET_BUFFER();

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

         if (*ptr == '#')
         {
            if ((*(ptr + 1) == '!') && (*(ptr + 2) == '#'))
            {
               get_log_type_data(ptr + 3);
            }
            IGNORE_ENTRY();
         }

         ptr_start_line = ptr;

         if (*(ptr_start_line + log_date_length + 1 + max_hostname_length + 2) == SEPARATOR_CHAR)
         {
            id.delete_reason_no = (int)(*(ptr_start_line + log_date_length + 1 + max_hostname_length + 1) - '0');
            id.offset = 0;
         }
         else
         {
            hex_dr_str[0] = *(ptr_start_line + log_date_length + 1 + max_hostname_length + 1);
            hex_dr_str[1] = *(ptr_start_line + log_date_length + 1 + max_hostname_length + 2);
            hex_dr_str[2] = *(ptr_start_line + log_date_length + 1 + max_hostname_length + 3);
            if (hex_dr_str[0] == '0')
            {
               hex_dr_str[0] = ' ';
               if (hex_dr_str[1] == '0')
               {
                  hex_dr_str[1] = ' ';
               }
            }
            id.delete_reason_no = (int)strtol(hex_dr_str, NULL, 16);
            id.offset = 2;
         }
         if ((id.delete_reason_no > MAX_DELETE_REASONS) ||
             ((dr_toggles_set & (1 << id.delete_reason_no)) == 0))
         {
            IGNORE_ENTRY();
         }
         FILE_SIZE_ONLY(id.delete_reason_no);

         ptr = ptr_start_line + log_date_length + 1 + max_hostname_length + 3 + id.offset;
         il[file_no].line_offset[item_counter] = (int)(ptr - p_start_log_file);
         time_when_transmitted = (time_t)str2timet(ptr_start_line, NULL, 16);
         if (first_date_found == -1)
         {
            first_date_found = time_when_transmitted;
         }
         if ((p_ts = localtime(&time_when_transmitted)) == NULL)
         {
            IGNORE_ENTRY();
         }
         CONVERT_TIME();
         j = 0;
         while ((*(ptr + j) != SEPARATOR_CHAR) && (j < file_name_length))
         {
            if ((unsigned char)(*(ptr + j)) < ' ')
            {
               *(p_file_name + j) = '?';
               unprintable_chars++;
            }
            else
            {
               *(p_file_name + j) = *(ptr + j);
            }
            j++;
         }
         ptr += j;
         (void)memcpy(p_host_name, ptr_start_line + log_date_length + 1,
                      (MAX_HOSTNAME_LENGTH > max_hostname_length) ? max_hostname_length : MAX_HOSTNAME_LENGTH);

         /* If necessary, ignore rest of file name. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;

         /* File size is already stored. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }

         /* Write transfer duration, job ID and additional reason. */
         /* Also check if we have to check for directory name.     */
         COMMON_BLOCK();
         file_size += tmp_file_size;
      }

      loops++;

      /* Display what we have in buffer. */
      display_data(i, first_date_found, time_when_transmitted);

      /* Check if user has done anything. */
      if ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL)
      {
         prev_time_val = now;
         CHECK_INTERRUPT();
      }

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

   il[file_no].no_of_items = item_counter;

   return;
}


/*-------------------------- file_name_and_size() -----------------------*/
/*                           --------------------                        */
/* Description: Reads everyting from ptr to ptr_end and search for the   */
/*              file name search_file_name that is <, >, or =            */
/*              search_file_size. It also checks if the transfer type is */
/*              the correct one.                                         */
/*-----------------------------------------------------------------------*/
static void
file_name_and_size(register char *ptr,
                   char          *ptr_end,
                   int           file_no,
                   char          *p_start_log_file)
{
   register int i,
                iii,
                j,
                match_found;
   int          item_counter = 0,
                loops = 0,
                ret;
   time_t       now,
                prev_time_val = 0L,
                time_when_transmitted = 0L;
   double       tmp_file_size;
   char         hex_dr_str[4],
                *ptr_start_line;
   struct tm    *p_ts;

   hex_dr_str[3] = '\0';
   do
   {
      /* Allocate memory for offset to job ID. */
      REALLOC_OFFSET_BUFFER();

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

         if (*ptr == '#')
         {
            if ((*(ptr + 1) == '!') && (*(ptr + 2) == '#'))
            {
               get_log_type_data(ptr + 3);
            }
            IGNORE_ENTRY();
         }

         ptr_start_line = ptr;

         if (*(ptr_start_line + log_date_length + 1 + max_hostname_length + 2) == SEPARATOR_CHAR)
         {
            id.delete_reason_no = (int)(*(ptr_start_line + log_date_length + 1 + max_hostname_length + 1) - '0');
            id.offset = 0;
         }
         else
         {
            hex_dr_str[0] = *(ptr_start_line + log_date_length + 1 + max_hostname_length + 1);
            hex_dr_str[1] = *(ptr_start_line + log_date_length + 1 + max_hostname_length + 2);
            hex_dr_str[2] = *(ptr_start_line + log_date_length + 1 + max_hostname_length + 3);
            if (hex_dr_str[0] == '0')
            {
               hex_dr_str[0] = ' ';
               if (hex_dr_str[1] == '0')
               {
                  hex_dr_str[1] = ' ';
               }
            }
            id.delete_reason_no = (int)strtol(hex_dr_str, NULL, 16);
            id.offset = 2;
         }
         if ((id.delete_reason_no > MAX_DELETE_REASONS) ||
             ((dr_toggles_set & (1 << id.delete_reason_no)) == 0))
         {
            IGNORE_ENTRY();
         }
         ptr += log_date_length + 1 + max_hostname_length + 3 + id.offset;
         match_found = -1;
         for (iii = 0; iii < no_of_search_file_names; iii++)
         {
            if ((ret = sfilter(search_file_name[iii], ptr, SEPARATOR_CHAR)) == 0)
            {
               if (search_file_name[iii][0] != '!')
               {
                  match_found = iii;
                  break;
               }
            }
            else if (ret == 1)
                 {
                    break;
                 }
         }
         if (match_found == -1)
         {
            IGNORE_ENTRY();
         }

         il[file_no].line_offset[item_counter] = (int)(ptr_start_line + log_date_length + 1 + max_hostname_length + 3 + id.offset - p_start_log_file);

         /* If necessary, ignore rest of file name. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;

         /* Write file size. */
         j = 0;
         while (*(ptr + j) != SEPARATOR_CHAR)
         {
            j++;
         }
         ptr += j;
         tmp_file_size = (off_t)str2offt((ptr - j), NULL, 16);
         if ((gt_lt_sign == EQUAL_SIGN) &&
             (tmp_file_size != search_file_size))
         {
            IGNORE_ENTRY();
         }
         else if ((gt_lt_sign == LESS_THEN_SIGN) &&
                  (tmp_file_size >= search_file_size))
              {
                 IGNORE_ENTRY();
              }
         else if ((gt_lt_sign == GREATER_THEN_SIGN) &&
                  (tmp_file_size <= search_file_size))
              {
                 IGNORE_ENTRY();
              }
         else if ((gt_lt_sign == NOT_SIGN) &&
                  (tmp_file_size == search_file_size))
              {
                 IGNORE_ENTRY();
              }

         ptr = ptr_start_line + log_date_length + 1 + max_hostname_length + 3 + id.offset;
         (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length + 1);
         (void)memcpy(p_host_name, ptr_start_line + log_date_length + 1,
                      (MAX_HOSTNAME_LENGTH > max_hostname_length) ? max_hostname_length : MAX_HOSTNAME_LENGTH);
         time_when_transmitted = (time_t)str2timet(ptr_start_line, NULL, 16);
         if (first_date_found == -1)
         {
            first_date_found = time_when_transmitted;
         }
         if ((p_ts = localtime(&time_when_transmitted)) == NULL)
         {
            IGNORE_ENTRY();
         }
         CONVERT_TIME();
         (void)memcpy(p_delete_reason, sdrstr[id.delete_reason_no],
                      MAX_REASON_LENGTH);
         j = 0;
         while ((*(ptr + j) != SEPARATOR_CHAR) && (j < file_name_length))
         {
            if ((unsigned char)(*(ptr + j)) < ' ')
            {
               *(p_file_name + j) = '?';
               unprintable_chars++;
            }
            else
            {
               *(p_file_name + j) = *(ptr + j);
            }
            j++;
         }
         ptr += j;
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;

         /* Ignore file size. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }

         print_file_size(p_file_size, (off_t)tmp_file_size);

         /* Write transfer duration, job ID and additional reason. */
         /* Also check if we have to check for directory name.     */
         COMMON_BLOCK();
         file_size += tmp_file_size;
      }

      loops++;

      /* Display what we have in buffer. */
      display_data(i, first_date_found, time_when_transmitted);

      /* Check if user has done anything. */
      if ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL)
      {
         prev_time_val = now;
         CHECK_INTERRUPT();
      }

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

   il[file_no].no_of_items = item_counter;

   return;
}


/*--------------------------- recipient_only() --------------------------*/
/*                            ----------------                           */
/* Description: Reads everyting from ptr to ptr_end and search for the   */
/*              recipient 'search_recipient. It also checks if the       */
/*              transfer type is the correct one.                        */
/*-----------------------------------------------------------------------*/
static void
recipient_only(register char *ptr,
               char          *ptr_end,
               int           file_no,
               char          *p_start_log_file)
{
   register int i,
                j;
   int          item_counter = 0,
                loops = 0;
   time_t       now,
                prev_time_val = 0L,
                time_when_transmitted = 0L;
   double       tmp_file_size;
   char         hex_dr_str[4],
                *ptr_start_line;
   struct tm    *p_ts;

   hex_dr_str[3] = '\0';
   do
   {
      /* Allocate memory for offset to job ID. */
      REALLOC_OFFSET_BUFFER();

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

         if (*ptr == '#')
         {
            if ((*(ptr + 1) == '!') && (*(ptr + 2) == '#'))
            {
               get_log_type_data(ptr + 3);
            }
            IGNORE_ENTRY();
         }

         ptr_start_line = ptr;

         if (*(ptr_start_line + log_date_length + 1 + max_hostname_length + 2) == SEPARATOR_CHAR)
         {
            id.delete_reason_no = (int)(*(ptr_start_line + log_date_length + 1 + max_hostname_length + 1) - '0');
            id.offset = 0;
         }
         else
         {
            hex_dr_str[0] = *(ptr_start_line + log_date_length + 1 + max_hostname_length + 1);
            hex_dr_str[1] = *(ptr_start_line + log_date_length + 1 + max_hostname_length + 2);
            hex_dr_str[2] = *(ptr_start_line + log_date_length + 1 + max_hostname_length + 3);
            if (hex_dr_str[0] == '0')
            {
               hex_dr_str[0] = ' ';
               if (hex_dr_str[1] == '0')
               {
                  hex_dr_str[1] = ' ';
               }
            }
            id.delete_reason_no = (int)strtol(hex_dr_str, NULL, 16);
            id.offset = 2;
         }
         if ((id.delete_reason_no > MAX_DELETE_REASONS) ||
             ((dr_toggles_set & (1 << id.delete_reason_no)) == 0))
         {
            IGNORE_ENTRY();
         }

         for (j = 0; j < no_of_search_hosts; j++)
         {
            if (sfilter(search_recipient[j], ptr_start_line + log_date_length + 1, ' ') == 0)
            {
               INSERT_TIME_REASON(id.delete_reason_no);
               break;
            }
         }
         if (j == no_of_search_hosts)
         {
            IGNORE_ENTRY();
         }

         il[file_no].line_offset[item_counter] = (int)(ptr_start_line + log_date_length + 1 + max_hostname_length + 3 + id.offset - p_start_log_file);
         ptr += log_date_length + 1 + max_hostname_length + 3 + id.offset;
         j = 0;
         while ((*(ptr + j) != SEPARATOR_CHAR) && (j < file_name_length))
         {
            if ((unsigned char)(*(ptr + j)) < ' ')
            {
               *(p_file_name + j) = '?';
               unprintable_chars++;
            }
            else
            {
               *(p_file_name + j) = *(ptr + j);
            }
            j++;
         }
         ptr += j;

         (void)memcpy(p_host_name, ptr_start_line + log_date_length + 1,
                      (MAX_HOSTNAME_LENGTH > max_hostname_length) ? max_hostname_length : MAX_HOSTNAME_LENGTH);

         /* If necessary, ignore rest of file name. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;

         /* Write file size. */
         j = 0;
         while (*(ptr + j) != SEPARATOR_CHAR)
         {
            j++;
         }
         ptr += j;
         tmp_file_size = (off_t)str2offt((ptr - j), NULL, 16);
         print_file_size(p_file_size, tmp_file_size);

         /* Write transfer duration, job ID and additional reason. */
         /* Also check if we have to check for directory name.     */
         COMMON_BLOCK();
         file_size += tmp_file_size;
      }

      loops++;

      /* Display what we have in buffer. */
      display_data(i, first_date_found, time_when_transmitted);

      /* Check if user has done anything. */
      if ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL)
      {
         prev_time_val = now;
         CHECK_INTERRUPT();
      }

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

   il[file_no].no_of_items = item_counter;

   return;
}


/*----------------------- file_name_and_recipient() ---------------------*/
static void
file_name_and_recipient(register char *ptr,
                        char          *ptr_end,
                        int           file_no,
                        char          *p_start_log_file)
{
   register int i,
                j;
   int          item_counter = 0,
                loops = 0,
                ret;
   time_t       now,
                prev_time_val = 0L,
                time_when_transmitted = 0L;
   double       tmp_file_size;
   char         hex_dr_str[4],
                *ptr_start_line;
   struct tm    *p_ts;

   hex_dr_str[3] = '\0';
   do
   {
      /* Allocate memory for offset to job ID. */
      REALLOC_OFFSET_BUFFER();

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

         if (*ptr == '#')
         {
            if ((*(ptr + 1) == '!') && (*(ptr + 2) == '#'))
            {
               get_log_type_data(ptr + 3);
            }
            IGNORE_ENTRY();
         }

         ptr_start_line = ptr;

         if (*(ptr_start_line + log_date_length + 1 + max_hostname_length + 2) == SEPARATOR_CHAR)
         {
            id.delete_reason_no = (int)(*(ptr_start_line + log_date_length + 1 + max_hostname_length + 1) - '0');
            id.offset = 0;
         }
         else
         {
            hex_dr_str[0] = *(ptr_start_line + log_date_length + 1 + max_hostname_length + 1);
            hex_dr_str[1] = *(ptr_start_line + log_date_length + 1 + max_hostname_length + 2);
            hex_dr_str[2] = *(ptr_start_line + log_date_length + 1 + max_hostname_length + 3);
            if (hex_dr_str[0] == '0')
            {
               hex_dr_str[0] = ' ';
               if (hex_dr_str[1] == '0')
               {
                  hex_dr_str[1] = ' ';
               }
            }
            id.delete_reason_no = (int)strtol(hex_dr_str, NULL, 16);
            id.offset = 2;
         }
         if ((id.delete_reason_no > MAX_DELETE_REASONS) ||
             ((dr_toggles_set & (1 << id.delete_reason_no)) == 0))
         {
            IGNORE_ENTRY();
         }
         FILE_NAME_AND_RECIPIENT(id.delete_reason_no);

         (void)memcpy(p_host_name, ptr_start_line + log_date_length + 1,
                      (MAX_HOSTNAME_LENGTH > max_hostname_length) ? max_hostname_length : MAX_HOSTNAME_LENGTH);

         /* If necessary, ignore rest of file name. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;

         /* Write file size. */
         j = 0;
         while (*(ptr + j) != SEPARATOR_CHAR)
         {
            j++;
         }
         ptr += j;
         tmp_file_size = (off_t)str2offt((ptr - j), NULL, 16);
         print_file_size(p_file_size, tmp_file_size);

         /* Write transfer duration, job ID and additional reason. */
         /* Also check if we have to check for directory name.     */
         COMMON_BLOCK();
         file_size += tmp_file_size;
      }

      loops++;

      /* Display what we have in buffer. */
      display_data(i, first_date_found, time_when_transmitted);

      /* Check if user has done anything. */
      if ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL)
      {
         prev_time_val = now;
         CHECK_INTERRUPT();
      }

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

   il[file_no].no_of_items = item_counter;

   return;
}


/*----------------------- file_size_and_recipient() ---------------------*/
static void
file_size_and_recipient(register char *ptr,
                        char          *ptr_end,
                        int           file_no,
                        char          *p_start_log_file)
{
   register int i,
                j;
   int          item_counter = 0,
                loops = 0;
   time_t       now,
                prev_time_val = 0L,
                time_when_transmitted = 0L;
   double       tmp_file_size;
   char         hex_dr_str[4],
                *ptr_start_line;
   struct tm    *p_ts;

   hex_dr_str[3] = '\0';
   do
   {
      /* Allocate memory for offset to job ID. */
      REALLOC_OFFSET_BUFFER();

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

         if (*ptr == '#')
         {
            if ((*(ptr + 1) == '!') && (*(ptr + 2) == '#'))
            {
               get_log_type_data(ptr + 3);
            }
            IGNORE_ENTRY();
         }

         ptr_start_line = ptr;

         if (*(ptr_start_line + log_date_length + 1 + max_hostname_length + 2) == SEPARATOR_CHAR)
         {
            id.delete_reason_no = (int)(*(ptr_start_line + log_date_length + 1 + max_hostname_length + 1) - '0');
            id.offset = 0;
         }
         else
         {
            hex_dr_str[0] = *(ptr_start_line + log_date_length + 1 + max_hostname_length + 1);
            hex_dr_str[1] = *(ptr_start_line + log_date_length + 1 + max_hostname_length + 2);
            hex_dr_str[2] = *(ptr_start_line + log_date_length + 1 + max_hostname_length + 3);
            if (hex_dr_str[0] == '0')
            {
               hex_dr_str[0] = ' ';
               if (hex_dr_str[1] == '0')
               {
                  hex_dr_str[1] = ' ';
               }
            }
            id.delete_reason_no = (int)strtol(hex_dr_str, NULL, 16);
            id.offset = 2;
         }
         if ((id.delete_reason_no > MAX_DELETE_REASONS) ||
             ((dr_toggles_set & (1 << id.delete_reason_no)) == 0))
         {
            IGNORE_ENTRY();
         }
         FILE_SIZE_AND_RECIPIENT(id.delete_reason_no);

         ptr = ptr_start_line + log_date_length + 1 + max_hostname_length + 3 + id.offset;
         il[file_no].line_offset[item_counter] = (int)(ptr - p_start_log_file);
         time_when_transmitted = (time_t)str2timet(ptr_start_line, NULL, 16);
         if (first_date_found == -1)
         {
            first_date_found = time_when_transmitted;
         }
         if ((p_ts = localtime(&time_when_transmitted)) == NULL)
         {
            IGNORE_ENTRY();
         }
         CONVERT_TIME();
         j = 0;
         while ((*(ptr + j) != SEPARATOR_CHAR) && (j < file_name_length))
         {
            if ((unsigned char)(*(ptr + j)) < ' ')
            {
               *(p_file_name + j) = '?';
               unprintable_chars++;
            }
            else
            {
               *(p_file_name + j) = *(ptr + j);
            }
            j++;
         }
         ptr += j;
         (void)memcpy(p_host_name, ptr_start_line + log_date_length + 1,
                      (MAX_HOSTNAME_LENGTH > max_hostname_length) ? max_hostname_length : MAX_HOSTNAME_LENGTH);

         /* If necessary, ignore rest of file name. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;

         /* File size is already stored. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }

         /* Write transfer duration, job ID and additional reason. */
         /* Also check if we have to check for directory name.     */
         COMMON_BLOCK();
         file_size += tmp_file_size;
      }

      loops++;

      /* Display what we have in buffer. */
      display_data(i, first_date_found, time_when_transmitted);

      /* Check if user has done anything. */
      if ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL)
      {
         prev_time_val = now;
         CHECK_INTERRUPT();
      }

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

   il[file_no].no_of_items = item_counter;

   return;
}


/*---------------------- file_name_size_recipient() ---------------------*/
static void
file_name_size_recipient(register char *ptr,
                         char          *ptr_end,
                         int           file_no,
                         char          *p_start_log_file)
{
   register int i,
                iii,
                j,
                match_found;
   int          item_counter = 0,
                loops = 0,
                ret;
   time_t       now,
                prev_time_val = 0L,
                time_when_transmitted = 0L;
   double       tmp_file_size;
   char         hex_dr_str[4],
                *ptr_start_line;
   struct tm    *p_ts;

   hex_dr_str[3] = '\0';
   do
   {
      /* Allocate memory for offset to job ID. */
      REALLOC_OFFSET_BUFFER();

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

         if (*ptr == '#')
         {
            if ((*(ptr + 1) == '!') && (*(ptr + 2) == '#'))
            {
               get_log_type_data(ptr + 3);
            }
            IGNORE_ENTRY();
         }

         ptr_start_line = ptr;

         if (*(ptr_start_line + log_date_length + 1 + max_hostname_length + 2) == SEPARATOR_CHAR)
         {
            id.delete_reason_no = (int)(*(ptr_start_line + log_date_length + 1 + max_hostname_length + 1) - '0');
            id.offset = 0;
         }
         else
         {
            hex_dr_str[0] = *(ptr_start_line + log_date_length + 1 + max_hostname_length + 1);
            hex_dr_str[1] = *(ptr_start_line + log_date_length + 1 + max_hostname_length + 2);
            hex_dr_str[2] = *(ptr_start_line + log_date_length + 1 + max_hostname_length + 3);
            if (hex_dr_str[0] == '0')
            {
               hex_dr_str[0] = ' ';
               if (hex_dr_str[1] == '0')
               {
                  hex_dr_str[1] = ' ';
               }
            }
            id.delete_reason_no = (int)strtol(hex_dr_str, NULL, 16);
            id.offset = 2;
         }
         if ((id.delete_reason_no > MAX_DELETE_REASONS) ||
             ((dr_toggles_set & (1 << id.delete_reason_no)) == 0))
         {
            IGNORE_ENTRY();
         }

         for (j = 0; j < no_of_search_hosts; j++)
         {
            if (sfilter(search_recipient[j], ptr_start_line + log_date_length + 1, ' ') == 0)
            {
               break;
            }
         }
         if (j == no_of_search_hosts)
         {
            IGNORE_ENTRY();
         }
         else
         {
            ptr += log_date_length + 1 + max_hostname_length + 3 + id.offset;
            match_found = -1;
            for (iii = 0; iii < no_of_search_file_names; iii++)
            {
               if ((ret = sfilter(search_file_name[iii], ptr, SEPARATOR_CHAR)) == 0)
               {
                  if (search_file_name[iii][0] != '!')
                  {
                     match_found = iii;
                     break;
                  }
               }
               else if (ret == 1)
                    {
                       break;
                    }
            }
            if (match_found == -1)
            {
               IGNORE_ENTRY();
            }
         }

         il[file_no].line_offset[item_counter] = (int)(ptr_start_line + log_date_length + 1 + max_hostname_length + 3 + id.offset - p_start_log_file);

         /* If necessary, ignore rest of file name. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;

         /* Get file size. */
         j = 0;
         while (*(ptr + j) != SEPARATOR_CHAR)
         {
            j++;
         }
         ptr += j;
         tmp_file_size = (off_t)str2offt((ptr - j), NULL, 16);
         if ((gt_lt_sign == EQUAL_SIGN) &&
             (tmp_file_size != search_file_size))
         {
            IGNORE_ENTRY();
         }
         else if ((gt_lt_sign == LESS_THEN_SIGN) &&
                  (tmp_file_size >= search_file_size))
              {
                 IGNORE_ENTRY();
              }
         else if ((gt_lt_sign == GREATER_THEN_SIGN) &&
                  (tmp_file_size <= search_file_size))
              {
                 IGNORE_ENTRY();
              }
         else if ((gt_lt_sign == NOT_SIGN) &&
                  (tmp_file_size == search_file_size))
              {
                 IGNORE_ENTRY();
              }

         ptr = ptr_start_line + log_date_length + 1 + max_hostname_length + 3 + id.offset;
         (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length + 1);
         (void)memcpy(p_host_name, ptr_start_line + log_date_length + 1,
                      (MAX_HOSTNAME_LENGTH > max_hostname_length) ? max_hostname_length : MAX_HOSTNAME_LENGTH);
         time_when_transmitted = (time_t)str2timet(ptr_start_line, NULL, 16);
         if (first_date_found == -1)
         {
            first_date_found = time_when_transmitted;
         }
         if ((p_ts = localtime(&time_when_transmitted)) == NULL)
         {
            IGNORE_ENTRY();
         }
         CONVERT_TIME();
         (void)memcpy(p_delete_reason, sdrstr[id.delete_reason_no],
                      MAX_REASON_LENGTH);

         j = 0;
         while ((*(ptr + j) != SEPARATOR_CHAR) && (j < file_name_length))
         {
            if ((unsigned char)(*(ptr + j)) < ' ')
            {
               *(p_file_name + j) = '?';
               unprintable_chars++;
            }
            else
            {
               *(p_file_name + j) = *(ptr + j);
            }
            j++;
         }
         ptr += j;
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;

         /* Write file size. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         print_file_size(p_file_size, (off_t)tmp_file_size);

         /* Write transfer duration, job ID and additional reason. */
         /* Also check if we have to check for directory name.     */
         COMMON_BLOCK();
         file_size += tmp_file_size;
      }

      loops++;

      /* Display what we have in buffer. */
      display_data(i, first_date_found, time_when_transmitted);

      /* Check if user has done anything. */
      if ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL)
      {
         prev_time_val = now;
         CHECK_INTERRUPT();
      }

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

   il[file_no].no_of_items = item_counter;

   return;
}


/*--------------------------- display_data() ----------------------------*/
static void
display_data(int    i,
             time_t first_date_found,
             time_t time_when_transmitted)
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

   calculate_summary(summary_str, first_date_found, time_when_transmitted,
                     total_no_files, file_size);
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
