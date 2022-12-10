/*
 *  get_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   05.04.1997 H.Kiehl Created
 **   14.01.1998 H.Kiehl Support for remote file name.
 **   13.02.1999 H.Kiehl Multiple recipients.
 **   12.04.2002 H.Kiehl Separator definable via SEPARATOR_CHAR.
 **   10.04.2004 H.Kiehl Added TLS/SSL support.
 **   31.01.2006 H.Kiehl Added SFTP support.
 **   14.02.2009 H.Kiehl Added output type.
 **   15.09.2021 H.Kiehl If we use log_fd in check_log_updates() we never
 **                      detect that the inode changes when logs are rotated.
 **
 */
DESCR__E_M3

#include <stdio.h>        /* snprintf()                                  */
#include <string.h>       /* strcmp(), strerror()                        */
#include <stdlib.h>       /* malloc(), realloc(), free(), strtoul(), labs()*/
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
#include "show_olog.h"

/* #define _MACRO_DEBUG */

/* External global variables. */
extern Display          *display;
extern Window           main_window;
extern XtAppContext     app;
extern Widget           appshell, /* CHECK_INTERRUPT() */
                        close_button_w,
                        listbox_w,
                        scrollbar_w,
                        statusbox_w,
                        special_button_w,
                        summarybox_w;
extern int              continues_toggle_set,
                        file_name_toggle_set,
                        radio_set,
                        gt_lt_sign,
                        gt_lt_sign_tt,
                        log_date_length,
                        max_hostname_length,
                        max_output_log_files,
                        no_of_search_dirs,
                        no_of_search_dirids,
                        no_of_search_file_names,
                        no_of_search_hosts,
                        no_of_search_jobids,
                        *search_dir_length,
                        special_button_flag,
                        file_name_length,
                        no_of_log_files,
#ifdef _WITH_DE_MAIL_SUPPORT
                        view_confirmation,
#endif
                        view_archived_only,
                        view_output_only,
                        view_received_only;
extern unsigned int     all_list_items,
                        *search_dirid,
                        *search_jobid;
extern XT_PTR_TYPE      toggles_set;
extern size_t           search_file_size;
extern time_t           start_time_val,
                        end_time_val;
extern double           search_transport_time;
extern char             *p_work_dir,
                        **search_file_name,
                        **search_dir,
                        *search_dir_filter,
                        **search_recipient,
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
static double           file_size,
                        trans_time;
static char             *p_file_name,
                        *p_host_name,
                        *p_type,
                        *p_file_size,
                        *p_tt,
                        *p_archive_flag,
                        log_file[MAX_PATH_LENGTH],
                        *p_log_file,
                        line[MAX_OUTPUT_LINE_LENGTH + SHOW_LONG_FORMAT + 1];
static XmStringTable    str_list = NULL;
static XtIntervalId     interval_id_log;

/* Local function prototypes. */
static char   *search_time(char *, time_t, time_t, time_t, size_t);
static void   check_log_updates(Widget),
              display_data(int, time_t, time_t),
              end_log_updates(void),
              extract_data(char *, int, int),
              no_criteria(register char *, char *, int, char *, off_t),
              file_name_only(register char *, char *, int, char *, off_t),
              file_size_only(register char *, char *, int, char *, off_t),
              file_name_and_size(register char *, char *, int, char *, off_t),
              recipient_only(register char *, char *, int, char *, off_t),
              file_name_and_recipient(register char *, char *, int, char *, off_t),
              file_size_and_recipient(register char *, char *, int, char *, off_t),
              file_name_size_recipient(register char *, char *, int, char *, off_t);

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
              if (((il[file_no].offset = realloc(il[file_no].offset, new_int_size)) == NULL) ||            \
                  ((il[file_no].line_offset = realloc(il[file_no].line_offset, new_off_t_size)) == NULL) ||\
                  ((il[file_no].archived = realloc(il[file_no].archived, new_char_size)) == NULL))         \
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
#define SET_FILE_NAME_POINTER()                         \
        {                                               \
           ptr += log_date_length + 1 + max_hostname_length + type_offset + 2;\
           if (file_name_toggle_set == REMOTE_FILENAME) \
           {                                            \
              tmp_ptr = ptr;                            \
              while (*tmp_ptr != SEPARATOR_CHAR)        \
              {                                         \
                 tmp_ptr++;                             \
              }                                         \
              if (*(tmp_ptr + 1) != SEPARATOR_CHAR)     \
              {                                         \
                 ptr = tmp_ptr + 1;                     \
                 ptr_is_remote = YES;                   \
              }                                         \
              else                                      \
              {                                         \
                 ptr_is_remote = NO;                    \
              }                                         \
           }                                            \
           else                                         \
           {                                            \
              ptr_is_remote = NO;                       \
           }                                            \
        }
#define INSERT_TIME_TYPE(protocol)                         \
        {                                                  \
           (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
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
              (void)memcpy(p_type, (protocol), 5);         \
           }                                               \
        }
#ifdef _WITH_DE_MAIL_SUPPORT
#define COMMON_BLOCK()\
        {\
           ptr++;\
           while (*ptr != SEPARATOR_CHAR)\
           {\
              ptr++;\
           }\
           tmp_ptr = ptr - 1;\
           j = 0;\
           while ((*tmp_ptr != SEPARATOR_CHAR) && (j < MAX_DISPLAYED_TRANSFER_TIME))\
           {\
              *(p_tt - j) = *tmp_ptr;\
              tmp_ptr--; j++;\
           }\
           if (j == MAX_DISPLAYED_TRANSFER_TIME)\
           {\
              tmp_ptr = ptr - 4;\
              j = 0;\
              while ((*tmp_ptr != SEPARATOR_CHAR) && (j < MAX_DISPLAYED_TRANSFER_TIME))\
              {\
                 *(p_tt - j) = *tmp_ptr;\
                 tmp_ptr--; j++;\
              }\
              if ((j == MAX_DISPLAYED_TRANSFER_TIME) && (*tmp_ptr != SEPARATOR_CHAR))\
              {\
                 *(p_tt - j) = '>';\
                 while (*tmp_ptr != SEPARATOR_CHAR)\
                 {\
                    tmp_ptr--; \
                 }\
              }\
              else\
              {\
                 while (j < MAX_DISPLAYED_TRANSFER_TIME)\
                 {\
                    *(p_tt - j) = ' ';\
                    j++;\
                 }\
              }\
           }\
           tmp_ptr++;\
           ptr++;\
           if (type_offset > 1)\
           {\
              int  count = 0;\
              char retries_str[MAX_INT_HEX_LENGTH + 1];\
\
              while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n') &&\
                     (count < MAX_INT_HEX_LENGTH))\
              {\
                 retries_str[count] = *ptr;\
                 count++; ptr++;\
              }\
              retries_str[count] = '\0';\
              id.retries = (unsigned int)strtoul(retries_str, NULL, 16);\
              while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))\
              {\
                 ptr++;\
              }\
              if (*ptr == SEPARATOR_CHAR)\
              {\
                 ptr++;\
              }\
           }\
           il[file_no].offset[item_counter] = (int)(ptr - p_start_log_file + offset);\
           if ((no_of_search_dirs > 0) || (no_of_search_dirids > 0) ||\
               (no_of_search_jobids > 0) ||\
               ((current_search_host != -1) &&\
                (search_user[current_search_host][0] != '\0')))\
           {\
              int  count = 0;\
              char job_id_str[15 + 1];\
\
              while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n') && (count < 15))\
              {\
                 job_id_str[count] = *ptr;\
                 count++; ptr++;\
              }\
              job_id_str[count] = '\0';\
              id.job_no = (unsigned int)strtoul(job_id_str, NULL, 16);\
              if (no_of_search_jobids > 0)\
              {\
                 int gotcha = NO,\
                     kk;\
\
                 for (kk = 0; kk < no_of_search_jobids; kk++)\
                 {\
                    if (id.job_no == search_jobid[kk])\
                    {\
                       gotcha = YES;\
                       break;\
                    }\
                 }\
                 if (gotcha == NO)\
                 {\
                    IGNORE_ENTRY();\
                 }\
              }\
              if ((current_search_host != -1) &&\
                  (search_user[current_search_host][0] != '\0'))\
              {\
                 char *at_ptr = search_user[current_search_host];\
\
                 id.user[0] = '\0';\
                 id.mail_destination[0] = '\0';\
                 get_info(GOT_JOB_ID_USER_ONLY);\
                 while ((*at_ptr != ' ') && (*at_ptr != '@') && (*at_ptr != '\0'))\
                 {\
                    at_ptr++;\
                 }\
                 if ((*at_ptr == '@') && (id.mail_destination[0] != '\0'))\
                 {\
                    if (sfilter(search_user[current_search_host],\
                                id.mail_destination, ' ') != 0)\
                    {\
                       IGNORE_ENTRY();\
                    }\
                 }\
                 else\
                 {\
                    if (sfilter(search_user[current_search_host], (char *)id.user, ' ') != 0)\
                    {\
                       IGNORE_ENTRY();\
                    }\
                 }\
              }\
              if ((no_of_search_dirs > 0) || (no_of_search_dirids > 0))\
              {\
                 int gotcha = NO,\
                     kk;\
\
                 get_info(GOT_JOB_ID_DIR_ONLY);\
                 count = strlen((char *)id.dir);\
                 if (id.dir[count - 1] != SEPARATOR_CHAR)\
                 {\
                    id.dir[count] = SEPARATOR_CHAR;\
                    id.dir[count + 1] = '\0';\
                 }\
                 else\
                 {\
                    count--;\
                 }\
                 for (kk = 0; kk < no_of_search_dirids; kk++)\
                 {\
                    if (search_dirid[kk] == id.dir_id)\
                    {\
                       gotcha = YES;\
                       break;\
                    }\
                 }\
                 if (gotcha == NO)\
                 {\
                    for (kk = 0; kk < no_of_search_dirs; kk++)\
                    {\
                       if (search_dir_filter[kk] == YES)\
                       {\
                          if (sfilter(search_dir[kk], (char *)id.dir, SEPARATOR_CHAR) == 0)\
                          {\
                             gotcha = YES;\
                             break;\
                          }\
                       }\
                       else\
                       {\
                          if (search_dir_length[kk] == count)\
                          {\
                             if (strncmp(search_dir[kk], (char *)id.dir, count) == 0)\
                             {\
                                gotcha = YES;\
                                break;\
                             }\
                          }\
                       }\
                    }\
                 }\
                 if (gotcha == NO)\
                 {\
                    IGNORE_ENTRY();\
                 }\
              }\
           }\
           else\
           {\
              while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))\
              {\
                 ptr++;\
              }\
           }\
           id.transport_time = strtod(tmp_ptr, NULL);\
           if ((search_transport_time == -1.0) ||\
               (((gt_lt_sign_tt == EQUAL_SIGN) &&\
                 (id.transport_time == search_transport_time)) ||\
                ((gt_lt_sign_tt == LESS_THEN_SIGN) &&\
                 (id.transport_time < search_transport_time)) ||\
                ((gt_lt_sign_tt == GREATER_THEN_SIGN) &&\
                 (id.transport_time > search_transport_time)) ||\
                ((gt_lt_sign_tt == NOT_SIGN) &&\
                 (id.transport_time != search_transport_time))))\
           {\
              trans_time += id.transport_time;\
           }\
           else\
           {\
              IGNORE_ENTRY();\
           }\
           ptr++;\
           while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))\
           {\
              ptr++;\
           }\
           if (*ptr == SEPARATOR_CHAR)\
           {\
              int  sub_dir_counter = 0;\
              char archive_status = 'Y';\
\
              ptr++;\
              while (*ptr != '\n')\
              {\
                 if ((*ptr == '/') && (*(ptr - 1) != '\\'))\
                 {\
                    sub_dir_counter++;\
                    if (sub_dir_counter == ARCHIVE_SUB_DIR_LEVEL)\
                    {\
                       int  cc = 0;\
                       char long_no[MAX_INT_LENGTH];\
\
                       ptr += 1;\
                       while ((*ptr != '_') && (*ptr != '\n') && (cc < MAX_INT_LENGTH))\
                       {\
                          long_no[cc] = *ptr;\
                          cc++; ptr++;\
                       }\
                       if ((*ptr != '\n') && (cc > 0) && (cc < MAX_INT_LENGTH))\
                       {\
                          time_t delete_time;\
\
                          long_no[cc] = '\0';\
                          delete_time = (time_t)str2timet(long_no, (char **)NULL, 16);\
                          if (now > (delete_time + ARCHIVE_STEP_TIME))\
                          {\
                             archive_status = 'D';\
                          }\
                          else if (now > (delete_time - 5))\
                               {\
                                  archive_status = '?';\
                               }\
                       }\
                    }\
                 }\
                 ptr++;\
              }\
              while (*ptr != '\n')\
              {\
                 ptr++;\
              }\
              *p_archive_flag = archive_status;\
              il[file_no].archived[item_counter] = 1;\
           }\
           else\
           {\
              if (confirmation_sign == 0)\
              {\
                 if (id.is_receive_job == YES)\
                 {\
                    *p_archive_flag = '*';\
                 }\
                 else\
                 {\
                    *p_archive_flag = 'N';\
                 }\
              }\
              else\
              {\
                 *p_archive_flag = confirmation_sign;\
              }\
           }\
           if ((*p_archive_flag != 'Y') && (view_archived_only == YES))\
           {\
              IGNORE_ENTRY();\
           }\
           item_counter++;\
           str_list[i] = XmStringCreateLocalized(line);\
           ptr++;\
        }
#else
#define COMMON_BLOCK()\
        {\
           ptr++;\
           while (*ptr != SEPARATOR_CHAR)\
           {\
              ptr++;\
           }\
           tmp_ptr = ptr - 1;\
           j = 0;\
           while ((*tmp_ptr != SEPARATOR_CHAR) && (j < MAX_DISPLAYED_TRANSFER_TIME))\
           {\
              *(p_tt - j) = *tmp_ptr;\
              tmp_ptr--; j++;\
           }\
           if (j == MAX_DISPLAYED_TRANSFER_TIME)\
           {\
              tmp_ptr = ptr - 4;\
              j = 0;\
              while ((*tmp_ptr != SEPARATOR_CHAR) && (j < MAX_DISPLAYED_TRANSFER_TIME))\
              {\
                 *(p_tt - j) = *tmp_ptr;\
                 tmp_ptr--; j++;\
              }\
              if ((j == MAX_DISPLAYED_TRANSFER_TIME) && (*tmp_ptr != SEPARATOR_CHAR))\
              {\
                 *(p_tt - j) = '>';\
                 while (*tmp_ptr != SEPARATOR_CHAR)\
                 {\
                    tmp_ptr--; \
                 }\
              }\
              else\
              {\
                 while (j < MAX_DISPLAYED_TRANSFER_TIME)\
                 {\
                    *(p_tt - j) = ' ';\
                    j++;\
                 }\
              }\
           }\
           tmp_ptr++;\
           ptr++;\
           if (type_offset > 1)\
           {\
              int  count = 0;\
              char retries_str[MAX_INT_HEX_LENGTH + 1];\
\
              while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n') &&\
                     (count < MAX_INT_HEX_LENGTH))\
              {\
                 retries_str[count] = *ptr;\
                 count++; ptr++;\
              }\
              retries_str[count] = '\0';\
              id.retries = (unsigned int)strtoul(retries_str, NULL, 16);\
              while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))\
              {\
                 ptr++;\
              }\
              if (*ptr == SEPARATOR_CHAR)\
              {\
                 ptr++;\
              }\
           }\
           il[file_no].offset[item_counter] = (int)(ptr - p_start_log_file + offset);\
           if ((no_of_search_dirs > 0) || (no_of_search_dirids > 0) ||\
               (no_of_search_jobids > 0) ||\
               ((current_search_host != -1) &&\
                (search_user[current_search_host][0] != '\0')))\
           {\
              int  count = 0;\
              char job_id_str[15 + 1];\
\
              while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n') && (count < 15))\
              {\
                 job_id_str[count] = *ptr;\
                 count++; ptr++;\
              }\
              job_id_str[count] = '\0';\
              id.job_no = (unsigned int)strtoul(job_id_str, NULL, 16);\
              if (no_of_search_jobids > 0)\
              {\
                 int gotcha = NO,\
                     kk;\
\
                 for (kk = 0; kk < no_of_search_jobids; kk++)\
                 {\
                    if (id.job_no == search_jobid[kk])\
                    {\
                       gotcha = YES;\
                       break;\
                    }\
                 }\
                 if (gotcha == NO)\
                 {\
                    IGNORE_ENTRY();\
                 }\
              }\
              if ((current_search_host != -1) &&\
                  (search_user[current_search_host][0] != '\0'))\
              {\
                 char *at_ptr = search_user[current_search_host];\
\
                 id.user[0] = '\0';\
                 id.mail_destination[0] = '\0';\
                 get_info(GOT_JOB_ID_USER_ONLY);\
                 while ((*at_ptr != ' ') && (*at_ptr != '@') && (*at_ptr != '\0'))\
                 {\
                    at_ptr++;\
                 }\
                 if ((*at_ptr == '@') && (id.mail_destination[0] != '\0'))\
                 {\
                    if (sfilter(search_user[current_search_host],\
                                id.mail_destination, ' ') != 0)\
                    {\
                       IGNORE_ENTRY();\
                    }\
                 }\
                 else\
                 {\
                    if (sfilter(search_user[current_search_host], (char *)id.user, ' ') != 0)\
                    {\
                       IGNORE_ENTRY();\
                    }\
                 }\
              }\
              if ((no_of_search_dirs > 0) || (no_of_search_dirids > 0))\
              {\
                 int gotcha = NO,\
                     kk;\
\
                 get_info(GOT_JOB_ID_DIR_ONLY);\
                 count = strlen((char *)id.dir);\
                 if (id.dir[count - 1] != SEPARATOR_CHAR)\
                 {\
                    id.dir[count] = SEPARATOR_CHAR;\
                    id.dir[count + 1] = '\0';\
                 }\
                 else\
                 {\
                    count--;\
                 }\
                 for (kk = 0; kk < no_of_search_dirids; kk++)\
                 {\
                    if (search_dirid[kk] == id.dir_id)\
                    {\
                       gotcha = YES;\
                       break;\
                    }\
                 }\
                 if (gotcha == NO)\
                 {\
                    for (kk = 0; kk < no_of_search_dirs; kk++)\
                    {\
                       if (search_dir_filter[kk] == YES)\
                       {\
                          if (sfilter(search_dir[kk], (char *)id.dir, SEPARATOR_CHAR) == 0)\
                          {\
                             gotcha = YES;\
                             break;\
                          }\
                       }\
                       else\
                       {\
                          if (search_dir_length[kk] == count)\
                          {\
                             if (strncmp(search_dir[kk], (char *)id.dir, count) == 0)\
                             {\
                                gotcha = YES;\
                                break;\
                             }\
                          }\
                       }\
                    }\
                 }\
                 if (gotcha == NO)\
                 {\
                    IGNORE_ENTRY();\
                 }\
              }\
           }\
           else\
           {\
              while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))\
              {\
                 ptr++;\
              }\
           }\
           id.transport_time = strtod(tmp_ptr, NULL);\
           if ((search_transport_time == -1.0) ||\
               (((gt_lt_sign_tt == EQUAL_SIGN) &&\
                 (id.transport_time == search_transport_time)) ||\
                ((gt_lt_sign_tt == LESS_THEN_SIGN) &&\
                 (id.transport_time < search_transport_time)) ||\
                ((gt_lt_sign_tt == GREATER_THEN_SIGN) &&\
                 (id.transport_time > search_transport_time)) ||\
                ((gt_lt_sign_tt == NOT_SIGN) &&\
                 (id.transport_time != search_transport_time))))\
           {\
              trans_time += id.transport_time;\
           }\
           else\
           {\
              IGNORE_ENTRY();\
           }\
           ptr++;\
           while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))\
           {\
              ptr++;\
           }\
           if (*ptr == SEPARATOR_CHAR)\
           {\
              int  sub_dir_counter = 0;\
              char archive_status = 'Y';\
\
              ptr++;\
              while (*ptr != '\n')\
              {\
                 if ((*ptr == '/') && (*(ptr - 1) != '\\'))\
                 {\
                    sub_dir_counter++;\
                    if (sub_dir_counter == ARCHIVE_SUB_DIR_LEVEL)\
                    {\
                       int  cc = 0;\
                       char long_no[MAX_INT_LENGTH];\
\
                       ptr += 1;\
                       while ((*ptr != '_') && (*ptr != '\n') && (cc < MAX_INT_LENGTH))\
                       {\
                          long_no[cc] = *ptr;\
                          cc++; ptr++;\
                       }\
                       if ((*ptr != '\n') && (cc > 0) && (cc < MAX_INT_LENGTH))\
                       {\
                          time_t delete_time;\
\
                          long_no[cc] = '\0';\
                          delete_time = (time_t)str2timet(long_no, (char **)NULL, 16);\
                          if (now > (delete_time + ARCHIVE_STEP_TIME))\
                          {\
                             archive_status = 'D';\
                          }\
                          else if (now > (delete_time - 5))\
                               {\
                                  archive_status = '?';\
                               }\
                       }\
                    }\
                 }\
                 ptr++;\
              }\
              while (*ptr != '\n')\
              {\
                 ptr++;\
              }\
              *p_archive_flag = archive_status;\
              il[file_no].archived[item_counter] = 1;\
           }\
           else\
           {\
              if (id.is_receive_job == YES)\
              {\
                 *p_archive_flag = '*';\
              }\
              else\
              {\
                 *p_archive_flag = 'N';\
              }\
           }\
           if ((*p_archive_flag != 'Y') && (view_archived_only == YES))\
           {\
              IGNORE_ENTRY();\
           }\
           item_counter++;\
           str_list[i] = XmStringCreateLocalized(line);\
           ptr++;\
        }
#endif

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

#ifdef HAVE_STRTOULL
#define FILE_SIZE_AND_RECIPIENT(id_string)                 \
        {                                                  \
           int ii;                                         \
                                                           \
           for (ii = 0; ii < no_of_search_hosts; ii++)     \
           {                                               \
              if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)\
              {                                            \
                 current_search_host = ii;                 \
                 break;                                    \
              }                                            \
           }                                               \
           if (current_search_host != -1)                  \
           {                                               \
              ptr += log_date_length + 1 + max_hostname_length + type_offset + 2;\
              while (*ptr != SEPARATOR_CHAR)               \
              {                                            \
                 ptr++;                                    \
              }                                            \
              ptr++;                                       \
              if (*ptr != SEPARATOR_CHAR)                  \
              {                                            \
                 /* Ignore the remote file name. */        \
                 while (*ptr != SEPARATOR_CHAR)            \
                 {                                         \
                    ptr++;                                 \
                 }                                         \
                 ptr++;                                    \
              }                                            \
              else                                         \
              {                                            \
                 ptr++;                                    \
              }                                            \
                                                           \
              j = 0;                                       \
              while (*ptr != SEPARATOR_CHAR)               \
              {                                            \
                 ptr++; j++;                               \
              }                                            \
              if (j > 15)                                  \
              {                                            \
                 tmp_file_size = strtod("INF", NULL);      \
              }                                            \
              else                                         \
              {                                            \
                 tmp_file_size = (double)strtoull(ptr - j, NULL, 16);\
              }                                            \
              if ((gt_lt_sign == EQUAL_SIGN) &&            \
                  (tmp_file_size == search_file_size))     \
              {                                            \
                 (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                 (void)memcpy(p_type, (id_string), 5);     \
                                                           \
                 /* Write file size. */                    \
                 print_file_size(p_file_size, (off_t)tmp_file_size);\
              }                                            \
              else if ((gt_lt_sign == LESS_THEN_SIGN) &&   \
                       (tmp_file_size < search_file_size)) \
                   {                                       \
                      (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                      (void)memcpy(p_type, (id_string), 5);\
                                                           \
                      /* Write file size. */               \
                      print_file_size(p_file_size, (off_t)tmp_file_size);\
                   }                                       \
              else if ((gt_lt_sign == GREATER_THEN_SIGN) &&\
                       (tmp_file_size > search_file_size)) \
                   {                                       \
                      (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                      (void)memcpy(p_type, (id_string), 5);\
                                                           \
                      /* Write file size. */               \
                      print_file_size(p_file_size, (off_t)tmp_file_size);\
                   }                                       \
              else if ((gt_lt_sign == NOT_SIGN) &&         \
                       (tmp_file_size != search_file_size))\
                   {                                       \
                      (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                      (void)memcpy(p_type, (id_string), 5);\
                                                           \
                      /* Write file size. */               \
                      print_file_size(p_file_size, (off_t)tmp_file_size);\
                   }                                       \
                   else                                    \
                   {                                       \
                      IGNORE_ENTRY();                      \
                   }                                       \
           }                                               \
           else                                            \
           {                                               \
              IGNORE_ENTRY();                              \
           }                                               \
        }
#else
# ifdef LINUX
#define FILE_SIZE_AND_RECIPIENT(id_string)                 \
        {                                                  \
           int ii;                                         \
                                                           \
           for (ii = 0; ii < no_of_search_hosts; ii++)     \
           {                                               \
              if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)\
              {                                            \
                 current_search_host = ii;                 \
                 break;                                    \
              }                                            \
           }                                               \
           if (current_search_host != -1)                  \
           {                                               \
              ptr += log_date_length + 1 + max_hostname_length + type_offset + 2;\
              while (*ptr != SEPARATOR_CHAR)               \
              {                                            \
                 ptr++;                                    \
              }                                            \
              ptr++;                                       \
              if (*ptr != SEPARATOR_CHAR)                  \
              {                                            \
                 /* Ignore the remote file name. */        \
                 while (*ptr != SEPARATOR_CHAR)            \
                 {                                         \
                    ptr++;                                 \
                 }                                         \
                 ptr++;                                    \
              }                                            \
              else                                         \
              {                                            \
                 ptr++;                                    \
              }                                            \
                                                           \
              j = 0;                                       \
              while (*(ptr + j) != SEPARATOR_CHAR)         \
              {                                            \
                 j++;                                      \
              }                                            \
              ptr += j;                                    \
              if (j < 9)                                   \
              {                                            \
                 tmp_file_size = (double)strtoul(ptr - j, NULL, 16);\
              }                                            \
              else                                         \
              {                                            \
                 if (j > 15)                               \
                 {                                         \
                    tmp_file_size = strtod("INF", NULL);   \
                 }                                         \
                 else                                      \
                 {                                         \
                    char tmp_buf[23];                      \
                                                           \
                    tmp_buf[0] = '0'; tmp_buf[1] = 'x';    \
                    (void)memcpy(&tmp_buf[2], ptr - j, j); \
                    tmp_buf[2 + j] = '\0';                 \
                    tmp_file_size = strtod(tmp_buf, NULL); \
                 }                                         \
              }                                            \
              if ((gt_lt_sign == EQUAL_SIGN) &&            \
                  (tmp_file_size == search_file_size))     \
              {                                            \
                 (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                 (void)memcpy(p_type, (id_string), 5);     \
                                                           \
                 /* Write file size. */                    \
                 print_file_size(p_file_size, (off_t)tmp_file_size);\
              }                                            \
              else if ((gt_lt_sign == LESS_THEN_SIGN) &&   \
                       (tmp_file_size < search_file_size)) \
                   {                                       \
                      (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                      (void)memcpy(p_type, (id_string), 5);\
                                                           \
                      /* Write file size. */               \
                      print_file_size(p_file_size, (off_t)tmp_file_size);\
                   }                                       \
              else if ((gt_lt_sign == GREATER_THEN_SIGN) &&\
                       (tmp_file_size > search_file_size)) \
                   {                                       \
                      (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                      (void)memcpy(p_type, (id_string), 5);\
                                                           \
                      /* Write file size. */               \
                      print_file_size(p_file_size, (off_t)tmp_file_size);\
                   }                                       \
              else if ((gt_lt_sign == NOT_SIGN) &&         \
                       (tmp_file_size != search_file_size))\
                   {                                       \
                      (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                      (void)memcpy(p_type, (id_string), 5);\
                                                           \
                      /* Write file size. */               \
                      print_file_size(p_file_size, (off_t)tmp_file_size);\
                   }                                       \
                   else                                    \
                   {                                       \
                      IGNORE_ENTRY();                      \
                   }                                       \
           }                                               \
           else                                            \
           {                                               \
              IGNORE_ENTRY();                              \
           }                                               \
        }
# else
#define FILE_SIZE_AND_RECIPIENT(id_string)                 \
        {                                                  \
           int ii;                                         \
                                                           \
           for (ii = 0; ii < no_of_search_hosts; ii++)     \
           {                                               \
              if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)\
              {                                            \
                 current_search_host = ii;                 \
                 break;                                    \
              }                                            \
           }                                               \
           if (current_search_host != -1)                  \
           {                                               \
              ptr += log_date_length + 1 + max_hostname_length + type_offset + 2;\
              while (*ptr != SEPARATOR_CHAR)               \
              {                                            \
                 ptr++;                                    \
              }                                            \
              ptr++;                                       \
              if (*ptr != SEPARATOR_CHAR)                  \
              {                                            \
                 /* Ignore the remote file name. */        \
                 while (*ptr != SEPARATOR_CHAR)            \
                 {                                         \
                    ptr++;                                 \
                 }                                         \
                 ptr++;                                    \
              }                                            \
              else                                         \
              {                                            \
                 ptr++;                                    \
              }                                            \
                                                           \
              j = 0;                                       \
              while (*(ptr + j) != SEPARATOR_CHAR)         \
              {                                            \
                 j++;                                      \
              }                                            \
              ptr += j;                                    \
              if (j < 9)                                   \
              {                                            \
                 tmp_file_size = (double)strtoul(ptr - j, NULL, 16);\
              }                                            \
              else                                         \
              {                                            \
                 tmp_file_size = strtod("INF", NULL);      \
              }                                            \
              if ((gt_lt_sign == EQUAL_SIGN) &&            \
                  (tmp_file_size == search_file_size))     \
              {                                            \
                 (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                 (void)memcpy(p_type, (id_string), 5);     \
                                                           \
                 /* Write file size. */                    \
                 print_file_size(p_file_size, (off_t)tmp_file_size);\
              }                                            \
              else if ((gt_lt_sign == LESS_THEN_SIGN) &&   \
                       (tmp_file_size < search_file_size)) \
                   {                                       \
                      (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                      (void)memcpy(p_type, (id_string), 5);\
                                                           \
                      /* Write file size. */               \
                      print_file_size(p_file_size, (off_t)tmp_file_size);\
                   }                                       \
              else if ((gt_lt_sign == GREATER_THEN_SIGN) &&\
                       (tmp_file_size > search_file_size)) \
                   {                                       \
                      (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                      (void)memcpy(p_type, (id_string), 5);\
                                                           \
                      /* Write file size. */               \
                      print_file_size(p_file_size, (off_t)tmp_file_size);\
                   }                                       \
              else if ((gt_lt_sign == NOT_SIGN) &&         \
                       (tmp_file_size != search_file_size))\
                   {                                       \
                      (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                      (void)memcpy(p_type, (id_string), 5);\
                                                           \
                      /* Write file size. */               \
                      print_file_size(p_file_size, (off_t)tmp_file_size);\
                   }                                       \
                   else                                    \
                   {                                       \
                      IGNORE_ENTRY();                      \
                   }                                       \
           }                                               \
           else                                            \
           {                                               \
              IGNORE_ENTRY();                              \
           }                                               \
        }
# endif
#endif

#define FILE_NAME_AND_RECIPIENT(toggle_id, id_string)      \
        {                                                  \
            if (toggles_set & (toggle_id))                 \
            {                                              \
               int ii;                                     \
                                                           \
               for (ii = 0; ii < no_of_search_hosts; ii++) \
               {                                           \
                  if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)\
                  {                                        \
                     current_search_host = ii;             \
                     break;                                \
                  }                                        \
               }                                           \
               if (current_search_host != -1)              \
               {                                           \
                  int iii,                                 \
                      match_found = -1;                    \
                                                           \
                  SET_FILE_NAME_POINTER();                 \
                  for (iii = 0; iii < no_of_search_file_names; iii++)\
                  {                                        \
                     if ((ret = sfilter(search_file_name[iii], ptr, SEPARATOR_CHAR)) == 0) \
                     {                                     \
                        if (search_file_name[iii][0] != '!')\
                        {                                  \
                           il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);\
                           INSERT_TIME_TYPE((id_string));  \
                           j = 0;                          \
                           while ((*(ptr + j) != SEPARATOR_CHAR) && (j < file_name_length))\
                           {                               \
                              if ((unsigned char)(*(ptr + j)) < ' ')\
                              {                            \
                                 *(p_file_name + j) = '?'; \
                                 unprintable_chars++;      \
                              }                            \
                              else                         \
                              {                            \
                                 *(p_file_name + j) = *(ptr + j);\
                              }                            \
                              j++;                         \
                           }                               \
                           ptr += j;                       \
                           match_found = iii;              \
                           break;                          \
                        }                                  \
                     }                                     \
                     else if (ret == 1)                    \
                          {                                \
                             break;                        \
                          }                                \
                  }                                        \
                  if (match_found == -1)                   \
                  {                                        \
                     IGNORE_ENTRY();                       \
                  }                                        \
               }                                           \
               else                                        \
               {                                           \
                  IGNORE_ENTRY();                          \
               }                                           \
            }                                              \
            else                                           \
            {                                              \
               IGNORE_ENTRY();                             \
            }                                              \
        }

#ifdef HAVE_STRTOULL
#define FILE_SIZE_ONLY(id_string)                          \
        {                                                  \
           ptr += log_date_length + 1 + max_hostname_length + type_offset + 2;\
           while (*ptr != SEPARATOR_CHAR)                  \
           {                                               \
              ptr++;                                       \
           }                                               \
           ptr++;                                          \
           if (*ptr != SEPARATOR_CHAR)                     \
           {                                               \
              /* Ignore the remote file name. */           \
              while (*ptr != SEPARATOR_CHAR)               \
              {                                            \
                 ptr++;                                    \
              }                                            \
              ptr++;                                       \
           }                                               \
           else                                            \
           {                                               \
              ptr++;                                       \
           }                                               \
                                                           \
           j = 0;                                          \
           while (*(ptr + j) != SEPARATOR_CHAR)            \
           {                                               \
              j++;                                         \
           }                                               \
           ptr += j;                                       \
           if (j > 15)                                     \
           {                                               \
              tmp_file_size = strtod("INF", NULL);         \
           }                                               \
           else                                            \
           {                                               \
              tmp_file_size = (double)strtoull(ptr - j, NULL, 16);\
           }                                               \
           if ((gt_lt_sign == EQUAL_SIGN) &&               \
               (tmp_file_size == search_file_size))        \
           {                                               \
              (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
              (void)memcpy(p_type, (id_string), 5);        \
                                                           \
              /* Write file size. */                       \
              print_file_size(p_file_size, (off_t)tmp_file_size);\
           }                                               \
           else if ((gt_lt_sign == LESS_THEN_SIGN) &&      \
                    (tmp_file_size < search_file_size))    \
                {                                          \
                   (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                   (void)memcpy(p_type, (id_string), 5);   \
                                                           \
                   /* Write file size. */                  \
                   print_file_size(p_file_size, (off_t)tmp_file_size);\
                }                                          \
           else if ((gt_lt_sign == GREATER_THEN_SIGN) &&   \
                    (tmp_file_size > search_file_size))    \
                {                                          \
                   (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                   (void)memcpy(p_type, (id_string), 5);   \
                                                           \
                   /* Write file size. */                  \
                   print_file_size(p_file_size, (off_t)tmp_file_size);\
                }                                          \
           else if ((gt_lt_sign == NOT_SIGN) &&            \
                    (tmp_file_size != search_file_size))   \
                {                                          \
                   (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                   (void)memcpy(p_type, (id_string), 5);   \
                                                           \
                   /* Write file size. */                  \
                   print_file_size(p_file_size, (off_t)tmp_file_size);\
                }                                          \
                else                                       \
                {                                          \
                   IGNORE_ENTRY();                         \
                }                                          \
        }
#else
# ifdef LINUX
#define FILE_SIZE_ONLY(id_string)                          \
        {                                                  \
           ptr += log_date_length + 1 + max_hostname_length + type_offset + 2;\
           while (*ptr != SEPARATOR_CHAR)                  \
           {                                               \
              ptr++;                                       \
           }                                               \
           ptr++;                                          \
           if (*ptr != SEPARATOR_CHAR)                     \
           {                                               \
              /* Ignore the remote file name. */           \
              while (*ptr != SEPARATOR_CHAR)               \
              {                                            \
                 ptr++;                                    \
              }                                            \
              ptr++;                                       \
           }                                               \
           else                                            \
           {                                               \
              ptr++;                                       \
           }                                               \
                                                           \
           j = 0;                                          \
           while (*(ptr + j) != SEPARATOR_CHAR)            \
           {                                               \
              j++;                                         \
           }                                               \
           ptr += j;                                       \
           if (j < 9)                                      \
           {                                               \
              tmp_file_size = (double)strtoul(ptr - j, NULL, 16);\
           }                                               \
           else                                            \
           {                                               \
              if (j > 15)                                  \
              {                                            \
                 tmp_file_size = strtod("INF", NULL);      \
              }                                            \
              else                                         \
              {                                            \
                 char tmp_buf[23];                         \
                                                           \
                 tmp_buf[0] = '0'; tmp_buf[1] = 'x';       \
                 (void)memcpy(&tmp_buf[2], ptr - j, j);    \
                 tmp_buf[2 + j] = '\0';                    \
                 tmp_file_size = strtod(tmp_buf, NULL);    \
              }                                            \
           }                                               \
           if ((gt_lt_sign == EQUAL_SIGN) &&               \
               (tmp_file_size == search_file_size))        \
           {                                               \
              (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
              (void)memcpy(p_type, (id_string), 5);        \
                                                           \
              /* Write file size. */                       \
              print_file_size(p_file_size, (off_t)tmp_file_size);\
           }                                               \
           else if ((gt_lt_sign == LESS_THEN_SIGN) &&      \
                    (tmp_file_size < search_file_size))    \
                {                                          \
                   (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                   (void)memcpy(p_type, (id_string), 5);   \
                                                           \
                   /* Write file size. */                  \
                   print_file_size(p_file_size, (off_t)tmp_file_size);\
                }                                          \
           else if ((gt_lt_sign == GREATER_THEN_SIGN) &&   \
                    (tmp_file_size > search_file_size))    \
                {                                          \
                   (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                   (void)memcpy(p_type, (id_string), 5);   \
                                                           \
                   /* Write file size. */                  \
                   print_file_size(p_file_size, (off_t)tmp_file_size);\
                }                                          \
           else if ((gt_lt_sign == NOT_SIGN) &&            \
                    (tmp_file_size != search_file_size))   \
                {                                          \
                   (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                   (void)memcpy(p_type, (id_string), 5);   \
                                                           \
                   /* Write file size. */                  \
                   print_file_size(p_file_size, (off_t)tmp_file_size);\
                }                                          \
                else                                       \
                {                                          \
                   IGNORE_ENTRY();                         \
                }                                          \
        }
# else
#define FILE_SIZE_ONLY(id_string)                          \
        {                                                  \
           ptr += log_date_length + 1 + max_hostname_length + type_offset + 2;\
           while (*ptr != SEPARATOR_CHAR)                  \
           {                                               \
              ptr++;                                       \
           }                                               \
           ptr++;                                          \
           if (*ptr != SEPARATOR_CHAR)                     \
           {                                               \
              /* Ignore the remote file name. */           \
              while (*ptr != SEPARATOR_CHAR)               \
              {                                            \
                 ptr++;                                    \
              }                                            \
              ptr++;                                       \
           }                                               \
           else                                            \
           {                                               \
              ptr++;                                       \
           }                                               \
                                                           \
           j = 0;                                          \
           while (*(ptr + j) != SEPARATOR_CHAR)            \
           {                                               \
              j++;                                         \
           }                                               \
           ptr += j;                                       \
           if (j < 9)                                      \
           {                                               \
              tmp_file_size = (double)strtoul(ptr - j, NULL, 16);\
           }                                               \
           else                                            \
           {                                               \
              tmp_file_size = strtod("INF", NULL);         \
           }                                               \
           if ((gt_lt_sign == EQUAL_SIGN) &&               \
               (tmp_file_size == search_file_size))        \
           {                                               \
              (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
              (void)memcpy(p_type, (id_string), 5);        \
                                                           \
              /* Write file size. */                       \
              print_file_size(p_file_size, (off_t)tmp_file_size);\
           }                                               \
           else if ((gt_lt_sign == LESS_THEN_SIGN) &&      \
                    (tmp_file_size < search_file_size))    \
                {                                          \
                   (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                   (void)memcpy(p_type, (id_string), 5);   \
                                                           \
                   /* Write file size. */                  \
                   print_file_size(p_file_size, (off_t)tmp_file_size);\
                }                                          \
           else if ((gt_lt_sign == GREATER_THEN_SIGN) &&   \
                    (tmp_file_size > search_file_size))    \
                {                                          \
                   (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                   (void)memcpy(p_type, (id_string), 5);   \
                                                           \
                   /* Write file size. */                  \
                   print_file_size(p_file_size, (off_t)tmp_file_size);\
                }                                          \
           else if ((gt_lt_sign == NOT_SIGN) &&            \
                    (tmp_file_size != search_file_size))   \
                {                                          \
                   (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                   (void)memcpy(p_type, (id_string), 5);   \
                                                           \
                   /* Write file size. */                  \
                   print_file_size(p_file_size, (off_t)tmp_file_size);\
                }                                          \
                else                                       \
                {                                          \
                   IGNORE_ENTRY();                         \
                }                                          \
        }
# endif
#endif


/*############################### get_data() ############################*/
void
get_data(void)
{
   int          i,
                j,
                start_file_no = -1,
                end_file_no = -1;
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

#ifdef DEBUG_PARAMETERS
   (void)fprintf(stderr,
                 "view_output_only=%s view_received_only=%s view_archived_only=%s\n",
                 (view_output_only == YES) ? "YES" : "NO",
                 (view_received_only == YES) ? "YES" : "NO",
                 (view_archived_only == YES) ? "YES" : "NO");
   (void)fprintf(stderr, "no_of_search_hosts=%d\n", no_of_search_hosts);
   for (i = 0; i < no_of_search_hosts; i++)
   {
      (void)fprintf(stderr, "search_recipient[%d]=%s search_user[%d]=%s\n",
                    i, search_recipient[i], i, search_user[i]);
   }
   (void)fprintf(stderr, "no_of_search_dirs=%d\n", no_of_search_dirs);
   for (i = 0; i < no_of_search_dirs; i++)
   {
      (void)fprintf(stderr, "search_dir[%d]=%s filter=%s length=%d\n",
                    i, search_dir[i],
                    (search_dir_filter[i] == NO) ? "NO" : "YES",
                    search_dir_length[i]);
   }
   (void)fprintf(stderr, "no_of_search_dirids=%d\n", no_of_search_dirids);
   for (i = 0; i < no_of_search_dirids; i++)
   {
      (void)fprintf(stderr, "search_dirid[%d]=%x\n", i, search_dirid[i]);
   }
#endif

   /* Prepare log file name. */
   i = snprintf(log_file, MAX_PATH_LENGTH, "%s%s/%s",
                p_work_dir, LOG_DIR, OUTPUT_BUFFER_FILE);
   if (i >= MAX_PATH_LENGTH)
   {
      (void)xrec(FATAL_DIALOG, "Buffer to small %d >= %d (%s %d)",
                 i, MAX_PATH_LENGTH, __FILE__, __LINE__);
      return;
   }
   p_log_file = log_file + i;
   no_of_log_files = max_output_log_files;

   local_start_time = start_time_val;
   local_end_time   = end_time_val;

   for (i = 0; i < no_of_log_files; i++)
   {
      (void)snprintf(p_log_file, MAX_PATH_LENGTH - (p_log_file - log_file),
                     "%d", i);
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
   no_of_log_files = start_file_no - end_file_no + 1;

   if (str_list == NULL)
   {
      if ((str_list = (XmStringTable)XtMalloc(LINES_BUFFERED * sizeof(XmString))) == NULL)
      {
         (void)xrec(FATAL_DIALOG, "XtMalloc() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }
   }

   /* Allocate memory for item list. */
   if (il == NULL)
   {
      if ((il = malloc(max_output_log_files * sizeof(struct item_list))) == NULL)
      {
         (void)xrec(FATAL_DIALOG, "malloc() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }
      for (i = 0; i < max_output_log_files; i++)
      {
         il[i].fp = NULL;
         il[i].no_of_items = 0;
         il[i].line_offset = NULL;
         il[i].offset = NULL;
         il[i].archived = NULL;
      }
   }
   else
   {
      for (i = 0; i < max_output_log_files; i++)
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
         if (il[i].archived != NULL)
         {
            free(il[i].archived);
            il[i].archived = NULL;
         }
      }
   }

   /* Initialise all pointer in line. */
   p_file_name    = line + 16;
   p_host_name    = p_file_name + file_name_length + 1;
   p_type         = p_host_name + MAX_HOSTNAME_LENGTH + 1;
   p_file_size    = p_type + 6 + 1;
   p_tt           = p_file_size + MAX_DISPLAYED_FILE_SIZE + MAX_DISPLAYED_TRANSFER_TIME;
   p_archive_flag = p_tt + 2;
   *(line + MAX_OUTPUT_LINE_LENGTH + file_name_length) = '\0';

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
   file_size = trans_time = 0.0;
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
      str_list = NULL;
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
      no_criteria(ptr_start, ptr_end, file_no, src, 0);
   }
   else if ((no_of_search_file_names != 0) && (search_file_size == -1) &&
            (no_of_search_hosts == 0))
        {
           file_name_only(ptr_start, ptr_end, file_no, src, 0);
        }
   else if ((no_of_search_file_names == 0) && (search_file_size != -1) &&
            (no_of_search_hosts == 0))
        {
           file_size_only(ptr_start, ptr_end, file_no, src, 0);
        }
   else if ((no_of_search_file_names != 0) && (search_file_size != -1) &&
            (no_of_search_hosts == 0))
        {
           file_name_and_size(ptr_start, ptr_end, file_no, src, 0);
        }
   else if ((no_of_search_file_names == 0) && (search_file_size == -1) &&
            (no_of_search_hosts != 0))
        {
           recipient_only(ptr_start, ptr_end, file_no, src, 0);
        }
   else if ((no_of_search_file_names != 0) && (search_file_size == -1) &&
            (no_of_search_hosts != 0))
        {
           file_name_and_recipient(ptr_start, ptr_end, file_no, src, 0);
        }
   else if ((no_of_search_file_names == 0) && (search_file_size != -1) &&
            (no_of_search_hosts != 0))
        {
           file_size_and_recipient(ptr_start, ptr_end, file_no, src, 0);
        }
   else if ((no_of_search_file_names != 0) && (search_file_size != -1) &&
            (no_of_search_hosts != 0))
        {
           file_name_size_recipient(ptr_start, ptr_end, file_no, src, 0);
        }
        else
        {
           (void)xrec(FATAL_DIALOG,
                      "What's this!? Impossible! (%s %d)", __FILE__, __LINE__);
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
                STATX_INO | STATX_SIZE, &stat_buf) == -1)
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
#ifdef HAVE_STATX
            (void)xrec(FATAL_DIALOG, "statx() error : %s (%s %d)\n",
#else
            (void)xrec(FATAL_DIALOG, "fstat() error : %s (%s %d)\n",
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
            XtUnmanageChild(listbox_w);
            XmListDeleteAllItems(listbox_w);
            get_data();
            XtManageChild(listbox_w);
            XmListSetBottomPos(listbox_w, 0);
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

         if ((no_of_search_file_names == 0) && (search_file_size == -1) &&
             (no_of_search_hosts == 0))
         {
            no_criteria(ptr_start, ptr_end, last_file_no,
                        ptr_start, log_offset);
         }
         else if ((no_of_search_file_names != 0) &&
                  (search_file_size == -1) && (no_of_search_hosts == 0))
              {
                 file_name_only(ptr_start, ptr_end, last_file_no, ptr_start,
                                log_offset);
              }
         else if ((no_of_search_file_names == 0) &&
                  (search_file_size != -1) && (no_of_search_hosts == 0))
              {
                 file_size_only(ptr_start, ptr_end, last_file_no, ptr_start,
                                log_offset);
              }
         else if ((no_of_search_file_names != 0) &&
                  (search_file_size != -1) && (no_of_search_hosts == 0))
              {
                 file_name_and_size(ptr_start, ptr_end, last_file_no,
                                    ptr_start, log_offset);
              }
         else if ((no_of_search_file_names == 0) &&
                  (search_file_size == -1) && (no_of_search_hosts != 0))
              {
                 recipient_only(ptr_start, ptr_end, last_file_no, ptr_start,
                                log_offset);
              }
         else if ((no_of_search_file_names != 0) &&
                  (search_file_size == -1) && (no_of_search_hosts != 0))
              {
                 file_name_and_recipient(ptr_start, ptr_end, last_file_no,
                                         ptr_start, log_offset);
              }
         else if ((no_of_search_file_names == 0) &&
                  (search_file_size != -1) && (no_of_search_hosts != 0))
              {
                 file_size_and_recipient(ptr_start, ptr_end, last_file_no,
                                         ptr_start, log_offset);
              }
         else if ((no_of_search_file_names != 0) &&
                  (search_file_size != -1) && (no_of_search_hosts != 0))
              {
                 file_name_size_recipient(ptr_start, ptr_end, last_file_no,
                                          ptr_start, log_offset);
              }
              else
              {
                 (void)xrec(FATAL_DIALOG, "What's this!? Impossible! (%s %d)",
                            __FILE__, __LINE__);
              }

         get_info_free();
         free(ptr_start);
#ifdef HAVE_STATX
         log_offset = stat_buf.stx_size;
#else
         log_offset = stat_buf.st_size;
#endif
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
            char          *p_start_log_file,
            off_t         offset)
{
   register int i,
                j;
   int          current_search_host = -1,
                item_counter = il[file_no].no_of_items,
                prev_item_counter = il[file_no].no_of_items,
                ptr_is_remote,
                loops = 0,
#ifndef LESSTIF_WORKAROUND
                unmanaged,
#endif
                type,
                type_offset;
   time_t       time_when_transmitted = 0L,
                prev_time_val = 0L,
                now;
   double       tmp_file_size;
   char         *tmp_ptr,
                *ptr_start_line;
#ifdef _WITH_DE_MAIL_SUPPORT
   char         confirmation_sign;
#endif
   struct tm    *p_ts;

   /* The easiest case! */
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

         if (((loops % 200) == 0) &&
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

         if (*(ptr + log_date_length + 1 + max_hostname_length + 2) == ' ')
         {
#ifdef ACTIVATE_THIS_AFTER_VERSION_14
            if ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_DELIVERED)) ||
                (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED)))
            {
               type_offset = 5;
               if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED))
               {
                  if ((view_archived_only == YES) || (view_output_only == YES))
                  {
                     IGNORE_ENTRY();
                  }
                  id.is_receive_job = YES;
               }
               else
               {
                  if (view_received_only == YES)
                  {
                     IGNORE_ENTRY();
                  }
                  id.is_receive_job = NO;
               }
# ifdef _WITH_DE_MAIL_SUPPORT
               confirmation_sign = 0;
# endif
            }
            else
            {
# ifdef _WITH_DE_MAIL_SUPPORT
               if ((view_confirmation == YES) &&
                   ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH)) ||
                    (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT)) ||
                    (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE)) ||
                    (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))))
               {
                  if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH))
                  {
                     confirmation_sign = 'd';
                  }
                  else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT))
                       {
                          confirmation_sign = 'r';
                       }
                  else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE))
                       {
                          confirmation_sign = 'R';
                       }
                  else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))
                       {
                          confirmation_sign = 't';
                       }
                       else
                       {
                          confirmation_sign = 0;
                       }
                  type_offset = 5;
                  id.is_receive_job = NO;
               }
               else
               {
                  IGNORE_ENTRY();
               }
# else
               IGNORE_ENTRY();
# endif
            }
#else
            if (*(ptr + log_date_length + 1 + max_hostname_length + 4) == ' ')
            {
               if ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_DELIVERED)) ||
                   (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED)))
               {
                  type_offset = 5;
                  if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED))
                  {
                     if ((view_archived_only == YES) ||
                         (view_output_only == YES))
                     {
                        IGNORE_ENTRY();
                     }
                     id.is_receive_job = YES;
                  }
                  else
                  {
                     if (view_received_only == YES)
                     {
                        IGNORE_ENTRY();
                     }
                     id.is_receive_job = NO;
                  }
# ifdef _WITH_DE_MAIL_SUPPORT
                  confirmation_sign = 0;
# endif
               }
               else
               {
# ifdef _WITH_DE_MAIL_SUPPORT
                  if ((view_confirmation == YES) &&
                      ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH)) ||
                       (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT)) ||
                       (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE)) ||
                       (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))))
                  {
                     if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH))
                     {
                        confirmation_sign = 'd';
                     }
                     else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT))
                          {
                             confirmation_sign = 'r';
                          }
                     else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE))
                          {
                             confirmation_sign = 'R';
                          }
                     else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))
                          {
                             confirmation_sign = 't';
                          }
                          else
                          {
                             confirmation_sign = 0;
                          }
                     type_offset = 5;
                     id.is_receive_job = NO;
                  }
                  else
                  {
                     IGNORE_ENTRY();
                  }
# else
                  IGNORE_ENTRY();
# endif
               }
            }
            else
            {
               type_offset = 3;
               id.is_receive_job = NO;
# ifdef _WITH_DE_MAIL_SUPPORT
               confirmation_sign = 0;
# endif
            }
#endif
         }
         else
         {
            type_offset = 1;
            id.is_receive_job = NO;
#ifdef _WITH_DE_MAIL_SUPPORT
            confirmation_sign = 0;
#endif
         }
         HEX_CHAR_TO_INT((*(ptr_start_line + log_date_length + 1 + max_hostname_length + type_offset)))
         switch (type)
         {
#ifdef _WITH_FTP_SUPPORT
            case FTP:
               if (toggles_set & SHOW_FTP)
               {
                  INSERT_TIME_TYPE(FTP_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_LOC_SUPPORT
            case LOC:
               if (toggles_set & SHOW_FILE)
               {
                  INSERT_TIME_TYPE(FILE_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_FD_EXEC_SUPPORT
            case EXEC:
               if (toggles_set & SHOW_EXEC)
               {
                  INSERT_TIME_TYPE(EXEC_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_HTTP_SUPPORT
            case HTTP:
               if (toggles_set & SHOW_HTTP)
               {
                  INSERT_TIME_TYPE(HTTP_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_SMTP_SUPPORT
            case SMTP:
               if (toggles_set & SHOW_SMTP)
               {
                  INSERT_TIME_TYPE(SMTP_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
            case DE_MAIL:
               if (toggles_set & SHOW_DEMAIL)
               {
                  INSERT_TIME_TYPE(DEMAIL_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_SFTP_SUPPORT
            case SFTP:
               if (toggles_set & SHOW_SFTP)
               {
                  INSERT_TIME_TYPE(SFTP_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_SCP_SUPPORT
            case SCP:
               if (toggles_set & SHOW_SCP)
               {
                  INSERT_TIME_TYPE(SCP_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_WMO_SUPPORT
            case WMO:
               if (toggles_set & SHOW_WMO)
               {
                  INSERT_TIME_TYPE(WMO_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_MAP_SUPPORT
            case MAP:
               if (toggles_set & SHOW_MAP)
               {
                  INSERT_TIME_TYPE(MAP_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_DFAX_SUPPORT
            case DFAX:
               if (toggles_set & SHOW_DFAX)
               {
                  INSERT_TIME_TYPE(DFAX_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef WITH_SSL
# ifdef _WITH_FTP_SUPPORT
            case FTPS:
               if (toggles_set & SHOW_FTPS)
               {
                  INSERT_TIME_TYPE(FTPS_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
# endif
# ifdef _WITH_HTTP_SUPPORT
            case HTTPS:
               if (toggles_set & SHOW_HTTPS)
               {
                  INSERT_TIME_TYPE(HTTPS_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
# endif
# ifdef _WITH_SMTP_SUPPORT
            case SMTPS:
               if (toggles_set & SHOW_SMTPS)
               {
                  INSERT_TIME_TYPE(SMTPS_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
# endif
#endif
            default :
               /* This some unknown type! */
               INSERT_TIME_TYPE(UNKNOWN_ID_STR);
               break;
         }

         il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
         SET_FILE_NAME_POINTER();
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
         if (ptr_is_remote == NO)
         {
            /* Ignore the remote file name. */
            while (*ptr != SEPARATOR_CHAR)
            {
               ptr++;
            }
            ptr++;
         }

         /* Write file size. */
         j = 0;
         while (*(ptr + j) != SEPARATOR_CHAR)
         {
            j++;
         }
         ptr += j;
#ifdef HAVE_STRTOULL
         if (j > 15)
         {
            tmp_file_size = strtod("INF", NULL);
         }
         else
         {
            tmp_file_size = (double)strtoull(ptr - j, NULL, 16);
         }
#else
         if (j < 9)
         {
            tmp_file_size = (double)strtoul(ptr - j, NULL, 16);
         }
         else
         {
# ifdef LINUX
            if (j > 15)
            {
               tmp_file_size = strtod("INF", NULL);
            }
            else
            {
               char tmp_buf[23];

               tmp_buf[0] = '0'; tmp_buf[1] = 'x';
               (void)memcpy(&tmp_buf[2], ptr - j, j);
               tmp_buf[2 + j] = '\0';
               tmp_file_size = strtod(tmp_buf, NULL);
            }
# else
            tmp_file_size = strtod("INF", NULL);
# endif
         }
#endif
         print_file_size(p_file_size, (off_t)tmp_file_size);

         /* Write transfer duration, job ID and archive directory. */
         /* Also check if we have to check for directory name.     */
#ifndef _MACRO_DEBUG
         COMMON_BLOCK();
#else
         ptr++;
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         tmp_ptr = ptr - 1;
         j = 0;
         while ((*tmp_ptr != SEPARATOR_CHAR) && (j < MAX_DISPLAYED_TRANSFER_TIME))
         {
            *(p_tt - j) = *tmp_ptr;
            tmp_ptr--; j++;
         }
         if (j == MAX_DISPLAYED_TRANSFER_TIME)
         {
            tmp_ptr = ptr - 4;
            j = 0;
            while ((*tmp_ptr != SEPARATOR_CHAR) && (j < MAX_DISPLAYED_TRANSFER_TIME))
            {
               *(p_tt - j) = *tmp_ptr;
               tmp_ptr--; j++;
            }
            if ((j == MAX_DISPLAYED_TRANSFER_TIME) && (*tmp_ptr != SEPARATOR_CHAR))
            {
               *(p_tt - j) = '>';
               while (*tmp_ptr != SEPARATOR_CHAR)
               {
                  tmp_ptr--;
               }
            }
            else
            {
               while (j < MAX_DISPLAYED_TRANSFER_TIME)
               {
                *(p_tt - j) = ' ';
                  j++;
               }
            }
         }
         tmp_ptr++;
         ptr++;

         /* Ignore retries. */
         if (type_offset > 1)
         {
            int  count = 0;
            char retries_str[MAX_INT_HEX_LENGTH + 1];

            while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n') &&
                   (count < MAX_INT_HEX_LENGTH))
            {
               retries_str[count] = *ptr;
               count++; ptr++;
            }
            retries_str[count] = '\0';
            id.retries = (unsigned int)strtoul(retries_str, NULL, 16);
            while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
            {
               ptr++;
            }
            if (*ptr == SEPARATOR_CHAR)
            {
               ptr++;
            }
         }

         il[file_no].offset[item_counter] = (int)(ptr - p_start_log_file + offset);

         if ((no_of_search_dirs > 0) || (no_of_search_dirids > 0) ||
             (no_of_search_jobids > 0) ||
             ((current_search_host != -1) &&
              (search_user[current_search_host][0] != '\0')))
         {
            int  count = 0;
            char job_id_str[15 + 1];

            while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n') && (count < 15))
            {
               job_id_str[count] = *ptr;
               count++; ptr++;
            }
            job_id_str[count] = '\0';
            id.job_no = (unsigned int)strtoul(job_id_str, NULL, 16);
            if (no_of_search_jobids > 0)
            {
               int gotcha = NO,
                   kk;

               for (kk = 0; kk < no_of_search_jobids; kk++)
               {
                  if (id.job_no == search_jobid[kk])
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

            if ((current_search_host != -1) &&
                (search_user[current_search_host][0] != '\0'))
            {
               char *at_ptr = search_user[current_search_host];

               id.user[0] = '\0';
               id.mail_destination[0] = '\0';
               get_info(GOT_JOB_ID_USER_ONLY);

               while ((*at_ptr != ' ') && (*at_ptr != '@') && (*at_ptr != '\0'))
               {
                  at_ptr++;
               }
               if ((*at_ptr == '@') && (id.mail_destination[0] != '\0'))
               {
                  if (sfilter(search_user[current_search_host],
                              id.mail_destination, ' ') != 0)
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  if (sfilter(search_user[current_search_host], (char *)id.user, ' ') != 0)
                  {
                     IGNORE_ENTRY();
                  }
               }
            }
            if ((no_of_search_dirs > 0) || (no_of_search_dirids > 0))
            {
               int gotcha = NO,
                   kk;

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
         }
         else
         {
            while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
            {
               ptr++;
            }
         }
         id.transport_time = strtod(tmp_ptr, NULL);
         if ((search_transport_time == -1.0) ||
             (((gt_lt_sign_tt == EQUAL_SIGN) &&
               (id.transport_time == search_transport_time)) ||
              ((gt_lt_sign_tt == LESS_THEN_SIGN) &&
               (id.transport_time < search_transport_time)) ||
              ((gt_lt_sign_tt == GREATER_THEN_SIGN) &&
               (id.transport_time > search_transport_time)) ||
              ((gt_lt_sign_tt == NOT_SIGN) &&
               (id.transport_time != search_transport_time))))
         {
            trans_time += id.transport_time;
         }
         else
         {
            IGNORE_ENTRY();
         }
         ptr++;
         while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
         {
            ptr++;
         }

         if (*ptr == SEPARATOR_CHAR)
         {
            int  sub_dir_counter = 0;
            char archive_status = 'Y';

            ptr++;
            while (*ptr != '\n')
            {
               if ((*ptr == '/') && (*(ptr - 1) != '\\'))
               {
                  sub_dir_counter++;
                  if (sub_dir_counter == ARCHIVE_SUB_DIR_LEVEL)
                  {
                     int  cc = 0;
                     char long_no[MAX_INT_LENGTH];

                     ptr += 1;
                     while ((*ptr != '_') && (*ptr != '\n') && (cc < MAX_INT_LENGTH))
                     {
                        long_no[cc] = *ptr;
                        cc++; ptr++;
                     }
                     if ((*ptr != '\n') && (cc > 0) && (cc < MAX_INT_LENGTH))
                     {
                        time_t delete_time;

                        long_no[cc] = '\0';
                        delete_time = (time_t)str2timet(long_no, (char **)NULL, 16);
                        if (now > (delete_time + ARCHIVE_STEP_TIME))
                        {
                           archive_status = 'D';
                        }
                        else if (now > (delete_time - 5))
                             {
                                archive_status = '?';
                             }
                     }
                  }
               }
               ptr++;
            }
            while (*ptr != '\n')
            {
               ptr++;
            }
            *p_archive_flag = archive_status;
            il[file_no].archived[item_counter] = 1;
         }
         else
         {
# ifdef _WITH_DE_MAIL_SUPPORT
            if (confirmation_sign == 0)
            {
#endif
               if (id.is_receive_job == YES)
               {
                  *p_archive_flag = '*';
               }
               else
               {
                  *p_archive_flag = 'N';
               }
# ifdef _WITH_DE_MAIL_SUPPORT
            }
            else
            {
               *p_archive_flag = confirmation_sign;
            }
#endif
         }
         if ((*p_archive_flag != 'Y') && (view_archived_only == YES))
         {
            IGNORE_ENTRY();
         }
         item_counter++;

         str_list[i] = XmStringCreateLocalized(line);

         ptr++;
#endif /* _MACRO_DEBUG */
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

#ifndef LESSTIF_WORKAROUND
   if (unmanaged == YES)
   {
      XtManageChild(listbox_w);
   }
#endif

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
               char          *p_start_log_file,
               off_t         offset)
{
   register int i,
                j;
   int          current_search_host = -1,
                item_counter = il[file_no].no_of_items,
                prev_item_counter = il[file_no].no_of_items,
                ptr_is_remote,
                loops = 0,
#ifndef LESSTIF_WORKAROUND
                unmanaged,
#endif
                ret,
                type,
                type_offset;
   time_t       now,
                prev_time_val = 0L,
                time_when_transmitted = 0L;
   double       tmp_file_size;
   char         *tmp_ptr,
                *ptr_start_line;
#ifdef _WITH_DE_MAIL_SUPPORT
   char         confirmation_sign;
#endif
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

         if (*(ptr + log_date_length + 1 + max_hostname_length + 2) == ' ')
         {
#ifdef ACTIVATE_THIS_AFTER_VERSION_14
            if ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_DELIVERED)) ||
                (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED)))
            {
               type_offset = 5;
               if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED))
               {
                  if ((view_archived_only == YES) || (view_output_only == YES))
                  {
                     IGNORE_ENTRY();
                  }
                  id.is_receive_job = YES;
               }
               else
               {
                  if (view_received_only == YES)
                  {
                     IGNORE_ENTRY();
                  }
                  id.is_receive_job = NO;
               }
# ifdef _WITH_DE_MAIL_SUPPORT
               confirmation_sign = 0;
# endif
            }
            else
            {
# ifdef _WITH_DE_MAIL_SUPPORT
               if ((view_confirmation == YES) &&
                   ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH)) ||
                    (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT)) ||
                    (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE)) ||
                    (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))))
               {
                  if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH))
                  {
                     confirmation_sign = 'd';
                  }
                  else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT))
                       {
                          confirmation_sign = 'r';
                       }
                  else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE))
                       {
                          confirmation_sign = 'R';
                       }
                  else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))
                       {
                          confirmation_sign = 't';
                       }
                       else
                       {
                          confirmation_sign = 0;
                       }
                  type_offset = 5;
                  id.is_receive_job = NO;
               }
               else
               {
                  IGNORE_ENTRY();
               }
# else
               IGNORE_ENTRY();
# endif
            }
#else
            if (*(ptr + log_date_length + 1 + max_hostname_length + 4) == ' ')
            {
               if ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_DELIVERED)) ||
                   (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED)))
               {
                  type_offset = 5;
                  if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED))
                  {
                     if ((view_archived_only == YES) ||
                         (view_output_only == YES))
                     {
                        IGNORE_ENTRY();
                     }
                     id.is_receive_job = YES;
                  }
                  else
                  {
                     if (view_received_only == YES)
                     {
                        IGNORE_ENTRY();
                     }
                     id.is_receive_job = NO;
                  }
# ifdef _WITH_DE_MAIL_SUPPORT
                  confirmation_sign = 0;
# endif
               }
               else
               {
# ifdef _WITH_DE_MAIL_SUPPORT
                  if ((view_confirmation == YES) &&
                      ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH)) ||
                       (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT)) ||
                       (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE)) ||
                       (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))))
                  {
                     if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH))
                     {
                        confirmation_sign = 'd';
                     }
                     else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT))
                          {
                             confirmation_sign = 'r';
                          }
                     else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE))
                          {
                             confirmation_sign = 'R';
                          }
                     else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))
                          {
                             confirmation_sign = 't';
                          }
                          else
                          {
                             confirmation_sign = 0;
                          }
                     type_offset = 5;
                     id.is_receive_job = NO;
                  }
                  else
                  {
                     IGNORE_ENTRY();
                  }
# else
                  IGNORE_ENTRY();
# endif
               }
            }
            else
            {
               type_offset = 3;
               id.is_receive_job = NO;
# ifdef _WITH_DE_MAIL_SUPPORT
               confirmation_sign = 0;
# endif
            }
#endif
         }
         else
         {
            type_offset = 1;
            id.is_receive_job = NO;
#ifdef _WITH_DE_MAIL_SUPPORT
            confirmation_sign = 0;
#endif
         }
         HEX_CHAR_TO_INT((*(ptr_start_line + log_date_length + 1 + max_hostname_length + type_offset)))
         switch (type)
         {
#ifdef _WITH_FTP_SUPPORT
            case FTP:
               if (toggles_set & SHOW_FTP)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
                     {
                        if (search_file_name[iii][0] != '!')
                        {
                           il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                           INSERT_TIME_TYPE(FTP_ID_STR);
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
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_LOC_SUPPORT
            case LOC:
               if (toggles_set & SHOW_FILE)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
                     {
                        if (search_file_name[iii][0] != '!')
                        {
                           il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                           INSERT_TIME_TYPE(FILE_ID_STR);
                           j = 0;
                           while ((*(ptr + j) != SEPARATOR_CHAR) &&
                                  (j < file_name_length))
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
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_FD_EXEC_SUPPORT
            case EXEC:
               if (toggles_set & SHOW_EXEC)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
                     {
                        if (search_file_name[iii][0] != '!')
                        {
                           il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                           INSERT_TIME_TYPE(EXEC_ID_STR);
                           j = 0;
                           while ((*(ptr + j) != SEPARATOR_CHAR) &&
                                  (j < file_name_length))
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
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_HTTP_SUPPORT
            case HTTP:
               if (toggles_set & SHOW_HTTP)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
                     {
                        if (search_file_name[iii][0] != '!')
                        {
                           il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                           INSERT_TIME_TYPE(HTTP_ID_STR);
                           j = 0;
                           while ((*(ptr + j) != SEPARATOR_CHAR) &&
                                  (j < file_name_length))
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
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_SMTP_SUPPORT
            case SMTP:
               if (toggles_set & SHOW_SMTP)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
                     {
                        if (search_file_name[iii][0] != '!')
                        {
                           il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                           INSERT_TIME_TYPE(SMTP_ID_STR);
                           j = 0;
                           while ((*(ptr + j) != SEPARATOR_CHAR) &&
                                  (j < file_name_length))
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
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
            case DE_MAIL:
               if (toggles_set & SHOW_DEMAIL)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
                     {
                        if (search_file_name[iii][0] != '!')
                        {
                           il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                           INSERT_TIME_TYPE(DEMAIL_ID_STR);
                           j = 0;
                           while ((*(ptr + j) != SEPARATOR_CHAR) &&
                                  (j < file_name_length))
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
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_SFTP_SUPPORT
            case SFTP:
               if (toggles_set & SHOW_SFTP)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
                     {
                        if (search_file_name[iii][0] != '!')
                        {
                           il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                           INSERT_TIME_TYPE(SFTP_ID_STR);
                           j = 0;
                           while ((*(ptr + j) != SEPARATOR_CHAR) &&
                                  (j < file_name_length))
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
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_SCP_SUPPORT
            case SCP:
               if (toggles_set & SHOW_SCP)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
                     {
                        if (search_file_name[iii][0] != '!')
                        {
                           il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                           INSERT_TIME_TYPE(SCP_ID_STR);
                           j = 0;
                           while ((*(ptr + j) != SEPARATOR_CHAR) &&
                                  (j < file_name_length))
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
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_WMO_SUPPORT
            case WMO:
               if (toggles_set & SHOW_WMO)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
                     {
                        if (search_file_name[iii][0] != '!')
                        {
                           il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                           INSERT_TIME_TYPE(WMO_ID_STR);
                           j = 0;
                           while ((*(ptr + j) != SEPARATOR_CHAR) &&
                                  (j < file_name_length))
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
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_MAP_SUPPORT
            case MAP:
               if (toggles_set & SHOW_MAP)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
                     {
                        if (search_file_name[iii][0] != '!')
                        {
                           il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                           INSERT_TIME_TYPE(MAP_ID_STR);
                           j = 0;
                           while ((*(ptr + j) != SEPARATOR_CHAR) &&
                                  (j < file_name_length))
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
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_DFAX_SUPPORT
            case DFAX:
               if (toggles_set & SHOW_DFAX)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name, ptr,
                                        SEPARATOR_CHAR)) == 0)
                     {
                        if (search_file_name[iii][0] != '!')
                        {
                           il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                           INSERT_TIME_TYPE(DFAX_ID_STR);
                           j = 0;
                           while ((*(ptr + j) != SEPARATOR_CHAR) &&
                                  (j < file_name_length))
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
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef WITH_SSL
# ifdef _WITH_FTP_SUPPORT
            case FTPS:
               if (toggles_set & SHOW_FTPS)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
                     {
                        if (search_file_name[iii][0] != '!')
                        {
                           il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                           INSERT_TIME_TYPE(FTPS_ID_STR);
                           j = 0;
                           while ((*(ptr + j) != SEPARATOR_CHAR) &&
                                  (j < file_name_length))
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
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
# endif
# ifdef _WITH_HTTP_SUPPORT
            case HTTPS:
               if (toggles_set & SHOW_HTTPS)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
                     {
                        if (search_file_name[iii][0] != '!')
                        {
                           il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                           INSERT_TIME_TYPE(HTTPS_ID_STR);
                           j = 0;
                           while ((*(ptr + j) != SEPARATOR_CHAR) &&
                                  (j < file_name_length))
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
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
# endif
# ifdef _WITH_SMTP_SUPPORT
            case SMTPS:
               if (toggles_set & SHOW_SMTPS)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
                     {
                        if (search_file_name[iii][0] != '!')
                        {
                           il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                           INSERT_TIME_TYPE(SMTPS_ID_STR);
                           j = 0;
                           while ((*(ptr + j) != SEPARATOR_CHAR) &&
                                  (j < file_name_length))
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
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
# endif
#endif
            default :
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
                     {
                        if (search_file_name[iii][0] != '!')
                        {
                           il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                           INSERT_TIME_TYPE(UNKNOWN_ID_STR);
                           j = 0;
                           while ((*(ptr + j) != SEPARATOR_CHAR) &&
                                  (j < file_name_length))
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
               }
               break;
         }

         (void)memcpy(p_host_name, ptr_start_line + log_date_length + 1,
                      (MAX_HOSTNAME_LENGTH > max_hostname_length) ? max_hostname_length : MAX_HOSTNAME_LENGTH);

         /* If necessary, ignore rest of file name. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;
         if (ptr_is_remote == NO)
         {
            /* Ignore the remote file name. */
            while (*ptr != SEPARATOR_CHAR)
            {
               ptr++;
            }
            ptr++;
         }

         /* Write file size. */
         j = 0;
         while (*(ptr + j) != SEPARATOR_CHAR)
         {
            j++;
         }
         ptr += j;
#ifdef HAVE_STRTOULL
         if (j > 15)
         {
            tmp_file_size = strtod("INF", NULL);
         }
         else
         {
            tmp_file_size = (double)strtoull(ptr - j, NULL, 16);
         }
#else
         if (j < 9)
         {
            tmp_file_size = (double)strtoul(ptr - j, NULL, 16);
         }
         else
         {
# ifdef LINUX
            if (j > 15)
            {
               tmp_file_size = strtod("INF", NULL);
            }
            else
            {
               char tmp_buf[23];

               tmp_buf[0] = '0'; tmp_buf[1] = 'x';
               (void)memcpy(&tmp_buf[2], ptr - j, j);
               tmp_buf[2 + j] = '\0';
               tmp_file_size = strtod(tmp_buf, NULL);
            }
# else
            tmp_file_size = strtod("INF", NULL);
# endif
         }
#endif
         print_file_size(p_file_size, (off_t)tmp_file_size);

         /* Write transfer duration, job ID and archive directory. */
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

#ifndef LESSTIF_WORKAROUND
   if (unmanaged == YES)
   {
      XtManageChild(listbox_w);
   }
#endif

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
               char          *p_start_log_file,
               off_t         offset)
{
   register int i,
                j;
   int          current_search_host = -1,
                item_counter = il[file_no].no_of_items,
                prev_item_counter = il[file_no].no_of_items,
                ptr_is_remote,
                loops = 0,
#ifndef LESSTIF_WORKAROUND
                unmanaged,
#endif
                type,
                type_offset;
   time_t       now,
                prev_time_val = 0L,
                time_when_transmitted = 0L;
   double       tmp_file_size;
   char         *tmp_ptr,
                *ptr_start_line;
#ifdef _WITH_DE_MAIL_SUPPORT
   char         confirmation_sign;
#endif
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

         if (*(ptr + log_date_length + 1 + max_hostname_length + 2) == ' ')
         {
#ifdef ACTIVATE_THIS_AFTER_VERSION_14
            if ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_DELIVERED)) ||
                (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED)))
            {
               type_offset = 5;
               if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED))
               {
                  if ((view_archived_only == YES) || (view_output_only == YES))
                  {
                     IGNORE_ENTRY();
                  }
                  id.is_receive_job = YES;
               }
               else
               {
                  if (view_received_only == YES)
                  {
                     IGNORE_ENTRY();
                  }
                  id.is_receive_job = NO;
               }
# ifdef _WITH_DE_MAIL_SUPPORT
               confirmation_sign = 0;
# endif
            }
            else
            {
# ifdef _WITH_DE_MAIL_SUPPORT
               if ((view_confirmation == YES) &&
                   ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH)) ||
                    (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT)) ||
                    (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE)) ||
                    (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))))
               {
                  if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH))
                  {
                     confirmation_sign = 'd';
                  }
                  else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT))
                       {
                          confirmation_sign = 'r';
                       }
                  else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE))
                       {
                          confirmation_sign = 'R';
                       }
                  else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))
                       {
                          confirmation_sign = 't';
                       }
                       else
                       {
                          confirmation_sign = 0;
                       }
                  type_offset = 5;
                  id.is_receive_job = NO;
               }
               else
               {
                  IGNORE_ENTRY();
               }
# else
               IGNORE_ENTRY();
# endif
            }
#else
            if (*(ptr + log_date_length + 1 + max_hostname_length + 4) == ' ')
            {
               if ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_DELIVERED)) ||
                   (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED)))
               {
                  type_offset = 5;
                  if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED))
                  {
                     if ((view_archived_only == YES) ||
                         (view_output_only == YES))
                     {
                        IGNORE_ENTRY();
                     }
                     id.is_receive_job = YES;
                  }
                  else
                  {
                     if (view_received_only == YES)
                     {
                        IGNORE_ENTRY();
                     }
                     id.is_receive_job = NO;
                  }
# ifdef _WITH_DE_MAIL_SUPPORT
                  confirmation_sign = 0;
# endif
               }
               else
               {
# ifdef _WITH_DE_MAIL_SUPPORT
                  if ((view_confirmation == YES) &&
                      ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH)) ||
                       (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT)) ||
                       (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE)) ||
                       (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))))
                  {
                     if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH))
                     {
                        confirmation_sign = 'd';
                     }
                     else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT))
                          {
                             confirmation_sign = 'r';
                          }
                     else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE))
                          {
                             confirmation_sign = 'R';
                          }
                     else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))
                          {
                             confirmation_sign = 't';
                          }
                          else
                          {
                             confirmation_sign = 0;
                          }
                     type_offset = 5;
                     id.is_receive_job = NO;
                  }
                  else
                  {
                     IGNORE_ENTRY();
                  }
# else
                  IGNORE_ENTRY();
# endif
               }
            }
            else
            {
               type_offset = 3;
               id.is_receive_job = NO;
# ifdef _WITH_DE_MAIL_SUPPORT
               confirmation_sign = 0;
# endif
            }
#endif
         }
         else
         {
            type_offset = 1;
            id.is_receive_job = NO;
#ifdef _WITH_DE_MAIL_SUPPORT
            confirmation_sign = 0;
#endif
         }
         HEX_CHAR_TO_INT((*(ptr_start_line + log_date_length + 1 + max_hostname_length + type_offset)))
         switch (type)
         {
#ifdef _WITH_FTP_SUPPORT
            case FTP:
               if (toggles_set & SHOW_FTP)
               {
                  FILE_SIZE_ONLY(FTP_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_LOC_SUPPORT
            case LOC:
               if (toggles_set & SHOW_FILE)
               {
                  FILE_SIZE_ONLY(FILE_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_FD_EXEC_SUPPORT
            case EXEC:
               if (toggles_set & SHOW_EXEC)
               {
                  FILE_SIZE_ONLY(EXEC_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_LOC_SUPPORT
            case HTTP:
               if (toggles_set & SHOW_HTTP)
               {
                  FILE_SIZE_ONLY(HTTP_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_SMTP_SUPPORT
            case SMTP:
               if (toggles_set & SHOW_SMTP)
               {
                  FILE_SIZE_ONLY(SMTP_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
            case DE_MAIL:
               if (toggles_set & SHOW_DEMAIL)
               {
                  FILE_SIZE_ONLY(DEMAIL_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_SFTP_SUPPORT
            case SFTP:
               if (toggles_set & SHOW_SFTP)
               {
                  FILE_SIZE_ONLY(SFTP_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_SCP_SUPPORT
            case SCP:
               if (toggles_set & SHOW_SCP)
               {
                  FILE_SIZE_ONLY(SCP_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_WMO_SUPPORT
            case WMO:
               if (toggles_set & SHOW_WMO)
               {
                  FILE_SIZE_ONLY(WMO_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_MAP_SUPPORT
            case MAP:
               if (toggles_set & SHOW_MAP)
               {
                  FILE_SIZE_ONLY(MAP_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_DFAX_SUPPORT
            case DFAX:
               if (toggles_set & SHOW_DFAX)
               {
                  FILE_SIZE_ONLY(DFAX_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef WITH_SSL
# ifdef _WITH_FTP_SUPPORT
            case FTPS:
               if (toggles_set & SHOW_FTPS)
               {
                  FILE_SIZE_ONLY(FTPS_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
# endif
# ifdef _WITH_HTTP_SUPPORT
            case HTTPS:
               if (toggles_set & SHOW_HTTPS)
               {
                  FILE_SIZE_ONLY(HTTPS_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
# endif
# ifdef _WITH_SMTP_SUPPORT
            case SMTPS:
               if (toggles_set & SHOW_SMTPS)
               {
                  FILE_SIZE_ONLY(SMTPS_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
# endif
#endif
            default :
               FILE_SIZE_ONLY(UNKNOWN_ID_STR);
               break;
         }

         ptr = ptr_start_line + log_date_length + 1 + max_hostname_length + type_offset + 2;
         il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
         time_when_transmitted = (time_t)str2timet(ptr_start_line, (char **)NULL, 16);
         if (first_date_found == -1)
         {
            first_date_found = time_when_transmitted;
         }
         if ((p_ts = localtime(&time_when_transmitted)) == NULL)
         {
            IGNORE_ENTRY();
         }
         CONVERT_TIME();
         if (file_name_toggle_set == REMOTE_FILENAME)
         {
            tmp_ptr = ptr;
            while (*tmp_ptr != SEPARATOR_CHAR)
            {
               tmp_ptr++;
            }
            if (*(tmp_ptr + 1) != SEPARATOR_CHAR)
            {
               ptr = tmp_ptr + 1;
               ptr_is_remote = YES;
            }
            else
            {
               ptr_is_remote = NO;
            }
         }
         else
         {
            ptr_is_remote = NO;
         }
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
         if (ptr_is_remote == NO)
         {
            /* Ignore the remote file name. */
            while (*ptr != SEPARATOR_CHAR)
            {
               ptr++;
            }
            ptr++;
         }

         /* File size is already stored. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }

         /* Write transfer duration, job ID and archive directory. */
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

#ifndef LESSTIF_WORKAROUND
   if (unmanaged == YES)
   {
      XtManageChild(listbox_w);
   }
#endif

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
                   char          *p_start_log_file,
                   off_t         offset)
{
   register int i,
                j;
   int          current_search_host = -1,
                item_counter = il[file_no].no_of_items,
                prev_item_counter = il[file_no].no_of_items,
                ptr_is_remote,
                loops = 0,
#ifndef LESSTIF_WORKAROUND
                unmanaged,
#endif
                ret,
                type,
                type_offset;
   time_t       now,
                prev_time_val = 0L,
                time_when_transmitted = 0L;
   double       tmp_file_size;
   char         *tmp_ptr,
                *ptr_start_line;
#ifdef _WITH_DE_MAIL_SUPPORT
   char         confirmation_sign;
#endif
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

         if (*(ptr + log_date_length + 1 + max_hostname_length + 2) == ' ')
         {
#ifdef ACTIVATE_THIS_AFTER_VERSION_14
            if ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_DELIVERED)) ||
                (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED)))
            {
               type_offset = 5;
               if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED))
               {
                  if ((view_archived_only == YES) || (view_output_only == YES))
                  {
                     IGNORE_ENTRY();
                  }
                  id.is_receive_job = YES;
               }
               else
               {
                  if (view_received_only == YES)
                  {
                     IGNORE_ENTRY();
                  }
                  id.is_receive_job = NO;
               }
# ifdef _WITH_DE_MAIL_SUPPORT
               confirmation_sign = 0;
# endif
            }
            else
            {
# ifdef _WITH_DE_MAIL_SUPPORT
               if ((view_confirmation == YES) &&
                   ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH)) ||
                    (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT)) ||
                    (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE)) ||
                    (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))))
               {
                  if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH))
                  {
                     confirmation_sign = 'd';
                  }
                  else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT))
                       {
                          confirmation_sign = 'r';
                       }
                  else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE))
                       {
                          confirmation_sign = 'R';
                       }
                  else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))
                       {
                          confirmation_sign = 't';
                       }
                       else
                       {
                          confirmation_sign = 0;
                       }
                  type_offset = 5;
                  id.is_receive_job = NO;
               }
               else
               {
                  IGNORE_ENTRY();
               }
# else
               IGNORE_ENTRY();
# endif
            }
#else
            if (*(ptr + log_date_length + 1 + max_hostname_length + 4) == ' ')
            {
               if ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_DELIVERED)) ||
                   (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED)))
               {
                  type_offset = 5;
                  if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED))
                  {
                     if ((view_archived_only == YES) ||
                         (view_output_only == YES))
                     {
                        IGNORE_ENTRY();
                     }
                     id.is_receive_job = YES;
                  }
                  else
                  {
                     if (view_received_only == YES)
                     {
                        IGNORE_ENTRY();
                     }
                     id.is_receive_job = NO;
                  }
# ifdef _WITH_DE_MAIL_SUPPORT
                  confirmation_sign = 0;
# endif
               }
               else
               {
# ifdef _WITH_DE_MAIL_SUPPORT
                  if ((view_confirmation == YES) &&
                      ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH)) ||
                       (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT)) ||
                       (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE)) ||
                       (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))))
                  {
                     if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH))
                     {
                        confirmation_sign = 'd';
                     }
                     else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT))
                          {
                             confirmation_sign = 'r';
                          }
                     else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE))
                          {
                             confirmation_sign = 'R';
                          }
                     else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))
                          {
                             confirmation_sign = 't';
                          }
                          else
                          {
                             confirmation_sign = 0;
                          }
                     type_offset = 5;
                     id.is_receive_job = NO;
                  }
                  else
                  {
                     IGNORE_ENTRY();
                  }
# else
                  IGNORE_ENTRY();
# endif
               }
            }
            else
            {
               type_offset = 3;
               id.is_receive_job = NO;
# ifdef _WITH_DE_MAIL_SUPPORT
               confirmation_sign = 0;
# endif
            }
#endif
         }
         else
         {
            type_offset = 1;
            id.is_receive_job = NO;
#ifdef _WITH_DE_MAIL_SUPPORT
            confirmation_sign = 0;
#endif
         }
         HEX_CHAR_TO_INT((*(ptr_start_line + log_date_length + 1 + max_hostname_length + type_offset)))
         switch (type)
         {
#ifdef _WITH_FTP_SUPPORT
            case FTP:
               if (toggles_set & SHOW_FTP)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
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
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_LOC_SUPPORT
            case LOC:
               if (toggles_set & SHOW_FILE)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
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
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_FD_EXEC_SUPPORT
            case EXEC:
               if (toggles_set & SHOW_EXEC)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
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
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_HTTP_SUPPORT
            case HTTP:
               if (toggles_set & SHOW_HTTP)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
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
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_SMTP_SUPPORT
            case SMTP:
               if (toggles_set & SHOW_SMTP)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
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
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
            case DE_MAIL:
               if (toggles_set & SHOW_DEMAIL)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
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
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_SFTP_SUPPORT
            case SFTP:
               if (toggles_set & SHOW_SFTP)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
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
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_SCP_SUPPORT
            case SCP:
               if (toggles_set & SHOW_SCP)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
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
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_WMO_SUPPORT
            case WMO:
               if (toggles_set & SHOW_WMO)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
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
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_MAP_SUPPORT
            case MAP:
               if (toggles_set & SHOW_MAP)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
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
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_DFAX_SUPPORT
            case DFAX:
               if (toggles_set & SHOW_DFAX)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
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
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef WITH_SSL
# ifdef _WITH_FTP_SUPPORT
            case FTPS:
               if (toggles_set & SHOW_FTPS)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
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
               else
               {
                  IGNORE_ENTRY();
               }
               break;
# endif
# ifdef _WITH_HTTP_SUPPORT
            case HTTPS:
               if (toggles_set & SHOW_HTTPS)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
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
               else
               {
                  IGNORE_ENTRY();
               }
               break;
# endif
# ifdef _WITH_SMTP_SUPPORT
            case SMTPS:
               if (toggles_set & SHOW_SMTPS)
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
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
               else
               {
                  IGNORE_ENTRY();
               }
               break;
# endif
#endif
            default :
               {
                  int iii,
                      match_found = -1;

                  SET_FILE_NAME_POINTER();
                  for (iii = 0; iii < no_of_search_file_names; iii++)
                  {
                     if ((ret = sfilter(search_file_name[iii], ptr,
                                        SEPARATOR_CHAR)) == 0)
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
               break;
         }

         il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);

         /* If necessary, ignore rest of file name. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;
         if (ptr_is_remote == NO)
         {
            /* Ignore the remote file name. */
            while (*ptr != SEPARATOR_CHAR)
            {
               ptr++;
            }
            ptr++;
         }

         /* Get file size and check if it fits. */
         j = 0;
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++; j++;
         }
#ifdef HAVE_STRTOULL
         if (j > 15)
         {
            tmp_file_size = strtod("INF", NULL);
         }
         else
         {
            tmp_file_size = (double)strtoull(ptr - j, NULL, 16);
         }
#else
         if (j < 9)
         {
            tmp_file_size = (double)strtoul(ptr - j, NULL, 16);
         }
         else
         {
# ifdef LINUX
            if (j > 15)
            {
               tmp_file_size = strtod("INF", NULL);
            }
            else
            {
               char tmp_buf[23];

               tmp_buf[0] = '0'; tmp_buf[1] = 'x';
               (void)memcpy(&tmp_buf[2], ptr - j, j);
               tmp_buf[2 + j] = '\0';
               tmp_file_size = strtod(tmp_buf, NULL);
            }
# else
            tmp_file_size = strtod("INF", NULL);
# endif
         }
#endif
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

         ptr = ptr_start_line + log_date_length + 1 + max_hostname_length + type_offset + 2;
         (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);
         (void)memcpy(p_host_name, ptr_start_line + log_date_length + 1,
                      (MAX_HOSTNAME_LENGTH > max_hostname_length) ? max_hostname_length : MAX_HOSTNAME_LENGTH);
         time_when_transmitted = (time_t)str2timet(ptr_start_line, (char **)NULL, 16);
         if (first_date_found == -1)
         {
            first_date_found = time_when_transmitted;
         }
         if ((p_ts = localtime(&time_when_transmitted)) == NULL)
         {
            IGNORE_ENTRY();
         }
         CONVERT_TIME();
         switch (type)
         {
#ifdef _WITH_FTP_SUPPORT
            case FTP:
               (void)memcpy(p_type, FTP_ID_STR, 5);
               break;
#endif
#ifdef _WITH_LOC_SUPPORT
            case LOC:
               (void)memcpy(p_type, FILE_ID_STR, 5);
               break;
#endif
#ifdef _WITH_FD_EXEC_SUPPORT
            case EXEC:
               (void)memcpy(p_type, EXEC_ID_STR, 5);
               break;
#endif
#ifdef _WITH_HTTP_SUPPORT
            case HTTP:
               (void)memcpy(p_type, HTTP_ID_STR, 5);
               break;
#endif
#ifdef _WITH_SMTP_SUPPORT
            case SMTP:
              (void)memcpy(p_type, SMTP_ID_STR, 5);
               break;
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
            case DE_MAIL:
              (void)memcpy(p_type, DEMAIL_ID_STR, 5);
               break;
#endif
#ifdef _WITH_SFTP_SUPPORT
            case SFTP:
               (void)memcpy(p_type, SFTP_ID_STR, 5);
               break;
#endif
#ifdef _WITH_SCP_SUPPORT
            case SCP:
               (void)memcpy(p_type, SCP_ID_STR, 5);
               break;
#endif
#ifdef _WITH_WMO_SUPPORT
            case WMO:
               (void)memcpy(p_type, WMO_ID_STR, 5);
               break;
#endif
#ifdef _WITH_MAP_SUPPORT
            case MAP:
               (void)memcpy(p_type, MAP_ID_STR, 5);
               break;
#endif
#ifdef _WITH_DFAX_SUPPORT
            case DFAX:
               (void)memcpy(p_type, DFAX_ID_STR, 5);
               break;
#endif
            default :
               (void)memcpy(p_type, UNKNOWN_ID_STR, 5);
               break;
         }
         j = 0;
         if (file_name_toggle_set == REMOTE_FILENAME)
         {
            tmp_ptr = ptr;
            while (*tmp_ptr != SEPARATOR_CHAR)
            {
               tmp_ptr++;
            }
            if (*(tmp_ptr + 1) != SEPARATOR_CHAR)
            {
               ptr = tmp_ptr + 1;
               ptr_is_remote = YES;
            }
            else
            {
               ptr_is_remote = NO;
            }
         }
         else
         {
            ptr_is_remote = NO;
         }
         while ((*ptr != SEPARATOR_CHAR) && (j < file_name_length))
         {
            if ((unsigned char)(*ptr) < ' ')
            {
               *(p_file_name + j) = '?';
               unprintable_chars++;
            }
            else
            {
               *(p_file_name + j) = *ptr;
            }
            ptr++; j++;
         }
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;
         if (ptr_is_remote == NO)
         {
            /* Ignore the remote file name. */
            while (*ptr != SEPARATOR_CHAR)
            {
               ptr++;
            }
            ptr++;
         }

         /* Write file size. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         print_file_size(p_file_size, (off_t)tmp_file_size);

         /* Write transfer duration, job ID and archive directory. */
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

#ifndef LESSTIF_WORKAROUND
   if (unmanaged == YES)
   {
      XtManageChild(listbox_w);
   }
#endif

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
               char          *p_start_log_file,
               off_t         offset)
{
   register int i,
                j;
   int          current_search_host,
                item_counter = il[file_no].no_of_items,
                prev_item_counter = il[file_no].no_of_items,
                ptr_is_remote,
                loops = 0,
#ifndef LESSTIF_WORKAROUND
                unmanaged,
#endif
                type,
                type_offset;
   time_t       now,
                prev_time_val = 0L,
                time_when_transmitted = 0L;
   double       tmp_file_size;
   char         *tmp_ptr,
                *ptr_start_line;
#ifdef _WITH_DE_MAIL_SUPPORT
   char         confirmation_sign;
#endif
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

         current_search_host = -1;
         ptr_start_line = ptr;

         if (*(ptr + log_date_length + 1 + max_hostname_length + 2) == ' ')
         {
#ifdef ACTIVATE_THIS_AFTER_VERSION_14
            if ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_DELIVERED)) ||
                (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED)))
            {
               type_offset = 5;
               if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED))
               {
                  if ((view_archived_only == YES) || (view_output_only == YES))
                  {
                     IGNORE_ENTRY();
                  }
                  id.is_receive_job = YES;
               }
               else
               {
                  if (view_received_only == YES)
                  {
                     IGNORE_ENTRY();
                  }
                  id.is_receive_job = NO;
               }
# ifdef _WITH_DE_MAIL_SUPPORT
               confirmation_sign = 0;
# endif
            }
            else
            {
# ifdef _WITH_DE_MAIL_SUPPORT
               if ((view_confirmation == YES) &&
                   ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH)) ||
                    (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT)) ||
                    (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE)) ||
                    (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))))
               {
                  if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH))
                  {
                     confirmation_sign = 'd';
                  }
                  else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT))
                       {
                          confirmation_sign = 'r';
                       }
                  else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE))
                       {
                          confirmation_sign = 'R';
                       }
                  else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))
                       {
                          confirmation_sign = 't';
                       }
                       else
                       {
                          confirmation_sign = 0;
                       }
                  type_offset = 5;
                  id.is_receive_job = NO;
               }
               else
               {
                  IGNORE_ENTRY();
               }
# else
               IGNORE_ENTRY();
# endif
            }
#else
            if (*(ptr + log_date_length + 1 + max_hostname_length + 4) == ' ')
            {
               if ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_DELIVERED)) ||
                   (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED)))
               {
                  type_offset = 5;
                  if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED))
                  {
                     if ((view_archived_only == YES) ||
                         (view_output_only == YES))
                     {
                        IGNORE_ENTRY();
                     }
                     id.is_receive_job = YES;
                  }
                  else
                  {
                     if (view_received_only == YES)
                     {
                        IGNORE_ENTRY();
                     }
                     id.is_receive_job = NO;
                  }
# ifdef _WITH_DE_MAIL_SUPPORT
                  confirmation_sign = 0;
# endif
               }
               else
               {
# ifdef _WITH_DE_MAIL_SUPPORT
                  if ((view_confirmation == YES) &&
                      ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH)) ||
                       (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT)) ||
                       (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE)) ||
                       (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))))
                  {
                     if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH))
                     {
                        confirmation_sign = 'd';
                     }
                     else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT))
                          {
                             confirmation_sign = 'r';
                          }
                     else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE))
                          {
                             confirmation_sign = 'R';
                          }
                     else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))
                          {
                             confirmation_sign = 't';
                          }
                          else
                          {
                             confirmation_sign = 0;
                          }
                     type_offset = 5;
                     id.is_receive_job = NO;
                  }
                  else
                  {
                     IGNORE_ENTRY();
                  }
# else
                  IGNORE_ENTRY();
# endif
               }
            }
            else
            {
               type_offset = 3;
               id.is_receive_job = NO;
# ifdef _WITH_DE_MAIL_SUPPORT
               confirmation_sign = 0;
# endif
            }
#endif
         }
         else
         {
            type_offset = 1;
            id.is_receive_job = NO;
#ifdef _WITH_DE_MAIL_SUPPORT
            confirmation_sign = 0;
#endif
         }
         HEX_CHAR_TO_INT((*(ptr_start_line + log_date_length + 1 + max_hostname_length + type_offset)))
         switch (type)
         {
#ifdef _WITH_FTP_SUPPORT
            case FTP:
               if (toggles_set & SHOW_FTP)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     INSERT_TIME_TYPE(FTP_ID_STR);
                  }
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_LOC_SUPPORT
            case LOC:
               if (toggles_set & SHOW_FILE)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     INSERT_TIME_TYPE(FILE_ID_STR);
                  }
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_FD_EXEC_SUPPORT
            case EXEC:
               if (toggles_set & SHOW_EXEC)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     INSERT_TIME_TYPE(EXEC_ID_STR);
                  }
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_HTTP_SUPPORT
            case HTTP:
               if (toggles_set & SHOW_HTTP)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     INSERT_TIME_TYPE(HTTP_ID_STR);
                  }
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_SMTP_SUPPORT
            case SMTP:
               if (toggles_set & SHOW_SMTP)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     INSERT_TIME_TYPE(SMTP_ID_STR);
                  }
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
            case DE_MAIL:
               if (toggles_set & SHOW_DEMAIL)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     INSERT_TIME_TYPE(DEMAIL_ID_STR);
                  }
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_SFTP_SUPPORT
            case SFTP:
               if (toggles_set & SHOW_SFTP)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     INSERT_TIME_TYPE(SFTP_ID_STR);
                  }
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_SCP_SUPPORT
            case SCP:
               if (toggles_set & SHOW_SCP)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     INSERT_TIME_TYPE(SCP_ID_STR);
                  }
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_WMO_SUPPORT
            case WMO:
               if (toggles_set & SHOW_WMO)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     INSERT_TIME_TYPE(WMO_ID_STR);
                  }
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_MAP_SUPPORT
            case MAP:
               if (toggles_set & SHOW_MAP)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     INSERT_TIME_TYPE(MAP_ID_STR);
                  }
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_DFAX_SUPPORT
            case DFAX:
               if (toggles_set & SHOW_DFAX)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     INSERT_TIME_TYPE(DFAX_ID_STR);
                  }
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef WITH_SSL
# ifdef _WITH_FTP_SUPPORT
            case FTPS:
               if (toggles_set & SHOW_FTPS)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     INSERT_TIME_TYPE(FTPS_ID_STR);
                  }
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
# endif
# ifdef _WITH_HTTP_SUPPORT
            case HTTPS:
               if (toggles_set & SHOW_HTTPS)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     INSERT_TIME_TYPE(HTTPS_ID_STR);
                  }
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
# endif
# ifdef _WITH_SMTP_SUPPORT
            case SMTPS:
               if (toggles_set & SHOW_SMTPS)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     INSERT_TIME_TYPE(SMTPS_ID_STR);
                  }
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
# endif
#endif
            default :
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     INSERT_TIME_TYPE(UNKNOWN_ID_STR);
                  }
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               break;
         }

         il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
         SET_FILE_NAME_POINTER();
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
         if (ptr_is_remote == NO)
         {
            /* Ignore the remote file name. */
            while (*ptr != SEPARATOR_CHAR)
            {
               ptr++;
            }
            ptr++;
         }

         /* Write file size. */
         j = 0;
         while (*(ptr + j) != SEPARATOR_CHAR)
         {
            j++;
         }
         ptr += j;
#ifdef HAVE_STRTOULL
         if (j > 15)
         {
            tmp_file_size = strtod("INF", NULL);
         }
         else
         {
            tmp_file_size = (double)strtoull(ptr - j, NULL, 16);
         }
#else
         if (j < 9)
         {
            tmp_file_size = (double)strtoul(ptr - j, NULL, 16);
         }
         else
         {
# ifdef LINUX
            if (j > 15)
            {
               tmp_file_size = strtod("INF", NULL);
            }
            else
            {
               char tmp_buf[23];

               tmp_buf[0] = '0'; tmp_buf[1] = 'x';
               (void)memcpy(&tmp_buf[2], ptr - j, j);
               tmp_buf[2 + j] = '\0';
               tmp_file_size = strtod(tmp_buf, NULL);
            }
# else
            tmp_file_size = strtod("INF", NULL);
# endif
         }
#endif
         print_file_size(p_file_size, (off_t)tmp_file_size);

         /* Write transfer duration, job ID and archive directory. */
         /* Also check if we have to check for directory name.     */
#ifndef _MACRO_DEBUG
         COMMON_BLOCK();
#else
         ptr++;
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         tmp_ptr = ptr - 1;
         j = 0;
         while ((*tmp_ptr != SEPARATOR_CHAR) && (j < MAX_DISPLAYED_TRANSFER_TIME))
         {
            *(p_tt - j) = *tmp_ptr;
            tmp_ptr--; j++;
         }
         if (j == MAX_DISPLAYED_TRANSFER_TIME)
         {
            tmp_ptr = ptr - 4;
            j = 0;
            while ((*tmp_ptr != SEPARATOR_CHAR) && (j < MAX_DISPLAYED_TRANSFER_TIME))
            {
               *(p_tt - j) = *tmp_ptr;
               tmp_ptr--; j++;
            }
            if ((j == MAX_DISPLAYED_TRANSFER_TIME) && (*tmp_ptr != SEPARATOR_CHAR))
            {
               *(p_tt - j) = '>';
               while (*tmp_ptr != SEPARATOR_CHAR)
               {
                  tmp_ptr--;
               }
            }
            else
            {
               while (j < MAX_DISPLAYED_TRANSFER_TIME)
               {
                  *(p_tt - j) = ' ';
                  j++;
               }
            }
         }
         tmp_ptr++;
         ptr++;

         /* Ignore retries. */
         if (type_offset > 1)
         {
            int  count = 0;
            char retries_str[MAX_INT_HEX_LENGTH + 1];

            while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n') &&
                   (count < MAX_INT_HEX_LENGTH))
            {
               retries_str[count] = *ptr;
               count++; ptr++;
            }
            retries_str[count] = '\0';
            id.retries = (unsigned int)strtoul(retries_str, NULL, 16);
            while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
            {
               ptr++;
            }
            if (*ptr == SEPARATOR_CHAR)
            {
               ptr++;
            }
         }

         il[file_no].offset[item_counter] = (int)(ptr - p_start_log_file + offset);

         if ((no_of_search_dirs > 0) || (no_of_search_dirids > 0) ||
             (no_of_search_jobids > 0) ||
             ((current_search_host != -1) &&
              (search_user[current_search_host][0] != '\0')))
         {
            int  count = 0;
            char job_id_str[15 + 1];

            while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n') && (count < 15))
            {
               job_id_str[count] = *ptr;
               count++; ptr++;
            }
            job_id_str[count] = '\0';
            id.job_no = (unsigned int)strtoul(job_id_str, NULL, 16);
            if (no_of_search_jobids > 0)
            {
               int gotcha = NO,
                   kk;

               for (kk = 0; kk < no_of_search_jobids; kk++)
               {
                  if (id.job_no == search_jobid[kk])
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

            if ((current_search_host != -1) &&
                (search_user[current_search_host][0] != '\0'))
            {
               char *at_ptr = search_user[current_search_host];

               id.user[0] = '\0';
               id.mail_destination[0] = '\0';
               get_info(GOT_JOB_ID_USER_ONLY);

               while ((*at_ptr != ' ') && (*at_ptr != '@') && (*at_ptr != '\0'))
               {
                  at_ptr++;
               }
               if ((*at_ptr == '@') && (id.mail_destination[0] != '\0'))
               {
                  if (sfilter(search_user[current_search_host],
                              id.mail_destination, ' ') != 0)
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  if (sfilter(search_user[current_search_host], (char *)id.user, ' ') != 0)
                  {
                     IGNORE_ENTRY();
                  }
               }
            }
            if ((no_of_search_dirs > 0) || (no_of_search_dirids > 0))
            {
               int gotcha = NO,
                   kk;

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
         }
         else
         {
            while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
            {
               ptr++;
            }
         }
         id.transport_time = strtod(tmp_ptr, NULL);
         if ((search_transport_time == -1.0) ||
             (((gt_lt_sign_tt == EQUAL_SIGN) &&
               (id.transport_time == search_transport_time)) ||
              ((gt_lt_sign_tt == LESS_THEN_SIGN) &&
               (id.transport_time < search_transport_time)) ||
              ((gt_lt_sign_tt == GREATER_THEN_SIGN) &&
               (id.transport_time > search_transport_time)) ||
              ((gt_lt_sign_tt == NOT_SIGN) &&
               (id.transport_time != search_transport_time))))
         {
            trans_time += id.transport_time;
         }
         else
         {
            IGNORE_ENTRY();
         }
         ptr++;
         while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
         {
            ptr++;
         }

         if (*ptr == SEPARATOR_CHAR)
         {
            int  sub_dir_counter = 0;
            char archive_status = 'Y';

            ptr++;
            while (*ptr != '\n')
            {
               if ((*ptr == '/') && (*(ptr - 1) != '\\'))
               {
                  sub_dir_counter++;
                  if (sub_dir_counter == ARCHIVE_SUB_DIR_LEVEL)
                  {
                     int  cc = 0;
                     char long_no[MAX_INT_LENGTH];

                     ptr += 1;
                     while ((*ptr != '_') && (*ptr != '\n') && (cc < MAX_INT_LENGTH))
                     {
                        long_no[cc] = *ptr;
                        cc++; ptr++;
                     }
                     if ((*ptr != '\n') && (cc > 0) && (cc < MAX_INT_LENGTH))
                     {
                        time_t delete_time;

                        long_no[cc] = '\0';
                        delete_time = (time_t)str2timet(long_no, (char **)NULL, 16);
                        if (now > (delete_time + ARCHIVE_STEP_TIME))
                        {
                           archive_status = 'D';
                        }
                        else if (now > (delete_time - 5))
                             {
                                archive_status = '?';
                             }
                     }
                  }
               }
               ptr++;
            }
            while (*ptr != '\n')
            {
               ptr++;
            }
            *p_archive_flag = archive_status;
            il[file_no].archived[item_counter] = 1;
         }
         else
         {
# ifdef _WITH_DE_MAIL_SUPPORT
            if (confirmation_sign == 0)
            {
#endif
               if (id.is_receive_job == YES)
               {
                  *p_archive_flag = '*';
               }
               else
               {
                  *p_archive_flag = 'N';
               }
# ifdef _WITH_DE_MAIL_SUPPORT
            }
            else
            {
               *p_archive_flag = confirmation_sign;
            }
# endif
         }
         if ((*p_archive_flag != 'Y') && (view_archived_only == YES))
         {
            IGNORE_ENTRY();
         }
         item_counter++;

         str_list[i] = XmStringCreateLocalized(line);

         ptr++;
#endif /* _MACRO_DEBUG */
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

#ifndef LESSTIF_WORKAROUND
   if (unmanaged == YES)
   {
      XtManageChild(listbox_w);
   }
#endif

   il[file_no].no_of_items = item_counter;

   return;
}


/*----------------------- file_name_and_recipient() ---------------------*/
static void
file_name_and_recipient(register char *ptr,
                        char          *ptr_end,
                        int           file_no,
                        char          *p_start_log_file,
                        off_t         offset)
{
   register int i,
                j;
   int          current_search_host,
                item_counter = il[file_no].no_of_items,
                prev_item_counter = il[file_no].no_of_items,
                ptr_is_remote,
                loops = 0,
#ifndef LESSTIF_WORKAROUND
                unmanaged,
#endif
                ret,
                type,
                type_offset;
   time_t       now,
                prev_time_val = 0L,
                time_when_transmitted = 0L;
   double       tmp_file_size;
   char         *tmp_ptr,
                *ptr_start_line;
#ifdef _WITH_DE_MAIL_SUPPORT
   char         confirmation_sign;
#endif
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

         current_search_host = -1;
         ptr_start_line = ptr;

         if (*(ptr + log_date_length + 1 + max_hostname_length + 2) == ' ')
         {
#ifdef ACTIVATE_THIS_AFTER_VERSION_14
            if ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_DELIVERED)) ||
                (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED)))
            {
               type_offset = 5;
               if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED))
               {
                  if ((view_archived_only == YES) || (view_output_only == YES))
                  {
                     IGNORE_ENTRY();
                  }
                  id.is_receive_job = YES;
               }
               else
               {
                  if (view_received_only == YES)
                  {
                     IGNORE_ENTRY();
                  }
                  id.is_receive_job = NO;
               }
# ifdef _WITH_DE_MAIL_SUPPORT
               confirmation_sign = 0;
# endif
            }
            else
            {
# ifdef _WITH_DE_MAIL_SUPPORT
               if ((view_confirmation == YES) &&
                   ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH)) ||
                    (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT)) ||
                    (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE)) ||
                    (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))))
               {
                  if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH))
                  {
                     confirmation_sign = 'd';
                  }
                  else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT))
                       {
                          confirmation_sign = 'r';
                       }
                  else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE))
                       {
                          confirmation_sign = 'R';
                       }
                  else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))
                       {
                          confirmation_sign = 't';
                       }
                       else
                       {
                          confirmation_sign = 0;
                       }
                  type_offset = 5;
                  id.is_receive_job = NO;
               }
               else
               {
                  IGNORE_ENTRY();
               }
# else
               IGNORE_ENTRY();
# endif
            }
#else
            if (*(ptr + log_date_length + 1 + max_hostname_length + 4) == ' ')
            {
               if ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_DELIVERED)) ||
                   (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED)))
               {
                  type_offset = 5;
                  if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED))
                  {
                     if ((view_archived_only == YES) ||
                         (view_output_only == YES))
                     {
                        IGNORE_ENTRY();
                     }
                     id.is_receive_job = YES;
                  }
                  else
                  {
                     if (view_received_only == YES)
                     {
                        IGNORE_ENTRY();
                     }
                     id.is_receive_job = NO;
                  }
# ifdef _WITH_DE_MAIL_SUPPORT
                  confirmation_sign = 0;
# endif
               }
               else
               {
# ifdef _WITH_DE_MAIL_SUPPORT
                  if ((view_confirmation == YES) &&
                      ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH)) ||
                       (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT)) ||
                       (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE)) ||
                       (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))))
                  {
                     if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH))
                     {
                        confirmation_sign = 'd';
                     }
                     else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT))
                          {
                             confirmation_sign = 'r';
                          }
                     else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE))
                          {
                             confirmation_sign = 'R';
                          }
                     else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))
                          {
                             confirmation_sign = 't';
                          }
                          else
                          {
                             confirmation_sign = 0;
                          }
                     type_offset = 5;
                     id.is_receive_job = NO;
                  }
                  else
                  {
                     IGNORE_ENTRY();
                  }
# else
                  IGNORE_ENTRY();
# endif
               }
            }
            else
            {
               type_offset = 3;
               id.is_receive_job = NO;
# ifdef _WITH_DE_MAIL_SUPPORT
               confirmation_sign = 0;
# endif
            }
#endif
         }
         else
         {
            type_offset = 1;
            id.is_receive_job = NO;
#ifdef _WITH_DE_MAIL_SUPPORT
            confirmation_sign = 0;
#endif
         }
         HEX_CHAR_TO_INT((*(ptr_start_line + log_date_length + 1 + max_hostname_length + type_offset)))
         switch (type)
         {
#ifdef _WITH_FTP_SUPPORT
            case FTP:
               FILE_NAME_AND_RECIPIENT(SHOW_FTP, FTP_ID_STR);
               break;
#endif
#ifdef _WITH_LOC_SUPPORT
            case LOC:
               FILE_NAME_AND_RECIPIENT(SHOW_FILE, FILE_ID_STR);
               break;
#endif
#ifdef _WITH_FD_EXEC_SUPPORT
            case EXEC:
               FILE_NAME_AND_RECIPIENT(SHOW_EXEC, EXEC_ID_STR);
               break;
#endif
#ifdef _WITH_HTTP_SUPPORT
            case HTTP:
               FILE_NAME_AND_RECIPIENT(SHOW_HTTP, HTTP_ID_STR);
               break;
#endif
#ifdef _WITH_SMTP_SUPPORT
            case SMTP:
               FILE_NAME_AND_RECIPIENT(SHOW_SMTP, SMTP_ID_STR);
               break;
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
            case DE_MAIL:
               FILE_NAME_AND_RECIPIENT(SHOW_DEMAIL, DEMAIL_ID_STR);
               break;
#endif
#ifdef _WITH_SFTP_SUPPORT
            case SFTP:
               FILE_NAME_AND_RECIPIENT(SHOW_SFTP, SFTP_ID_STR);
               break;
#endif
#ifdef _WITH_SCP_SUPPORT
            case SCP:
               FILE_NAME_AND_RECIPIENT(SHOW_SCP, SCP_ID_STR);
               break;
#endif
#ifdef _WITH_WMO_SUPPORT
            case WMO:
               FILE_NAME_AND_RECIPIENT(SHOW_WMO, WMO_ID_STR);
               break;
#endif
#ifdef _WITH_MAP_SUPPORT
            case MAP:
               FILE_NAME_AND_RECIPIENT(SHOW_MAP, MAP_ID_STR);
               break;
#endif
#ifdef _WITH_DFAX_SUPPORT
            case DFAX:
               FILE_NAME_AND_RECIPIENT(SHOW_DFAX, DFAX_ID_STR);
               break;
#endif
#ifdef WITH_SSL
# ifdef _WITH_FTP_SUPPORT
            case FTPS:
               FILE_NAME_AND_RECIPIENT(SHOW_FTPS, FTPS_ID_STR);
               break;
# endif
# ifdef _WITH_HTTP_SUPPORT
            case HTTPS:
               FILE_NAME_AND_RECIPIENT(SHOW_HTTPS, HTTPS_ID_STR);
               break;
# endif
# ifdef _WITH_SMTP_SUPPORT
            case SMTPS:
               FILE_NAME_AND_RECIPIENT(SHOW_SMTPS, SMTPS_ID_STR);
               break;
# endif
#endif
            default :
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     int iii,
                         match_found = -1;

                     SET_FILE_NAME_POINTER();
                     for (iii = 0; iii < no_of_search_file_names; iii++)
                     {
                        if ((ret = sfilter(search_file_name[iii], ptr,
                                           SEPARATOR_CHAR)) == 0)
                        {
                           if (search_file_name[iii][0] != '!')
                           {
                              il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                              INSERT_TIME_TYPE(UNKNOWN_ID_STR);
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
                  }
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               break;
         }

         (void)memcpy(p_host_name, ptr_start_line + log_date_length + 1,
                      (MAX_HOSTNAME_LENGTH > max_hostname_length) ? max_hostname_length : MAX_HOSTNAME_LENGTH);

         /* If necessary, ignore rest of file name. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;
         if (ptr_is_remote == NO)
         {
            /* Ignore the remote file name. */
            while (*ptr != SEPARATOR_CHAR)
            {
               ptr++;
            }
            ptr++;
         }

         /* Write file size. */
         j = 0;
         while (*(ptr + j) != SEPARATOR_CHAR)
         {
            j++;
         }
         ptr += j;
#ifdef HAVE_STRTOULL
         if (j > 15)
         {
            tmp_file_size = strtod("INF", NULL);
         }
         else
         {
            tmp_file_size = (double)strtoull(ptr - j, NULL, 16);
         }
#else
         if (j < 9)
         {
            tmp_file_size = (double)strtoul(ptr - j, NULL, 16);
         }
         else
         {
# ifdef LINUX
            if (j > 15)
            {
               tmp_file_size = strtod("INF", NULL);
            }
            else
            {
               char tmp_buf[23];

               tmp_buf[0] = '0'; tmp_buf[1] = 'x';
               (void)memcpy(&tmp_buf[2], ptr - j, j);
               tmp_buf[2 + j] = '\0';
               tmp_file_size = strtod(tmp_buf, NULL);
            }
# else
            tmp_file_size = strtod("INF", NULL);
# endif
         }
#endif
         print_file_size(p_file_size, (off_t)tmp_file_size);

         /* Write transfer duration, job ID and archive directory. */
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

#ifndef LESSTIF_WORKAROUND
   if (unmanaged == YES)
   {
      XtManageChild(listbox_w);
   }
#endif

   il[file_no].no_of_items = item_counter;

   return;
}


/*----------------------- file_size_and_recipient() ---------------------*/
static void
file_size_and_recipient(register char *ptr,
                        char          *ptr_end,
                        int           file_no,
                        char          *p_start_log_file,
                        off_t         offset)
{
   register int i,
                j;
   int          current_search_host,
                item_counter = il[file_no].no_of_items,
                prev_item_counter = il[file_no].no_of_items,
                ptr_is_remote,
                loops = 0,
#ifndef LESSTIF_WORKAROUND
                unmanaged,
#endif
                type,
                type_offset;
   time_t       now,
                prev_time_val = 0L,
                time_when_transmitted = 0L;
   double       tmp_file_size;
   char         *tmp_ptr,
                *ptr_start_line;
#ifdef _WITH_DE_MAIL_SUPPORT
   char         confirmation_sign;
#endif
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

         current_search_host = -1;
         ptr_start_line = ptr;

         if (*(ptr + log_date_length + 1 + max_hostname_length + 2) == ' ')
         {
#ifdef ACTIVATE_THIS_AFTER_VERSION_14
            if ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_DELIVERED)) ||
                (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED)))
            {
               type_offset = 5;
               if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED))
               {
                  if ((view_archived_only == YES) || (view_output_only == YES))
                  {
                     IGNORE_ENTRY();
                  }
                  id.is_receive_job = YES;
               }
               else
               {
                  if (view_received_only == YES)
                  {
                     IGNORE_ENTRY();
                  }
                  id.is_receive_job = NO;
               }
# ifdef _WITH_DE_MAIL_SUPPORT
               confirmation_sign = 0;
# endif
            }
            else
            {
# ifdef _WITH_DE_MAIL_SUPPORT
               if ((view_confirmation == YES) &&
                   ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH)) ||
                    (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT)) ||
                    (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE)) ||
                    (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))))
               {
                  if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH))
                  {
                     confirmation_sign = 'd';
                  }
                  else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT))
                       {
                          confirmation_sign = 'r';
                       }
                  else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE))
                       {
                          confirmation_sign = 'R';
                       }
                  else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))
                       {
                          confirmation_sign = 't';
                       }
                       else
                       {
                          confirmation_sign = 0;
                       }
                  type_offset = 5;
                  id.is_receive_job = NO;
               }
               else
               {
                  IGNORE_ENTRY();
               }
# else
               IGNORE_ENTRY();
# endif
            }
#else
            if (*(ptr + log_date_length + 1 + max_hostname_length + 4) == ' ')
            {
               if ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_DELIVERED)) ||
                   (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED)))
               {
                  type_offset = 5;
                  if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED))
                  {
                     if ((view_archived_only == YES) ||
                         (view_output_only == YES))
                     {
                        IGNORE_ENTRY();
                     }
                     id.is_receive_job = YES;
                  }
                  else
                  {
                     if (view_received_only == YES)
                     {
                        IGNORE_ENTRY();
                     }
                     id.is_receive_job = NO;
                  }
# ifdef _WITH_DE_MAIL_SUPPORT
                  confirmation_sign = 0;
# endif
               }
               else
               {
# ifdef _WITH_DE_MAIL_SUPPORT
                  if ((view_confirmation == YES) &&
                      ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH)) ||
                       (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT)) ||
                       (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE)) ||
                       (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))))
                  {
                     if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH))
                     {
                        confirmation_sign = 'd';
                     }
                     else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT))
                          {
                             confirmation_sign = 'r';
                          }
                     else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE))
                          {
                             confirmation_sign = 'R';
                          }
                     else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))
                          {
                             confirmation_sign = 't';
                          }
                          else
                          {
                             confirmation_sign = 0;
                          }
                     type_offset = 5;
                     id.is_receive_job = NO;
                  }
                  else
                  {
                     IGNORE_ENTRY();
                  }
# else
                  IGNORE_ENTRY();
# endif
               }
            }
            else
            {
               type_offset = 3;
               id.is_receive_job = NO;
# ifdef _WITH_DE_MAIL_SUPPORT
               confirmation_sign = 0;
# endif
            }
#endif
         }
         else
         {
            type_offset = 1;
            id.is_receive_job = NO;
#ifdef _WITH_DE_MAIL_SUPPORT
            confirmation_sign = 0;
#endif
         }
         HEX_CHAR_TO_INT((*(ptr_start_line + log_date_length + 1 + max_hostname_length + type_offset)))
         switch (type)
         {
#ifdef _WITH_FTP_SUPPORT
            case FTP:
               if (toggles_set & SHOW_FTP)
               {
                  FILE_SIZE_AND_RECIPIENT(FTP_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_LOC_SUPPORT
            case LOC:
               if (toggles_set & SHOW_FILE)
               {
                  FILE_SIZE_AND_RECIPIENT(FILE_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_FD_EXEC_SUPPORT
            case EXEC:
               if (toggles_set & SHOW_EXEC)
               {
                  FILE_SIZE_AND_RECIPIENT(EXEC_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_HTTP_SUPPORT
            case HTTP:
               if (toggles_set & SHOW_HTTP)
               {
                  FILE_SIZE_AND_RECIPIENT(HTTP_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_SMTP_SUPPORT
            case SMTP:
               if (toggles_set & SHOW_SMTP)
               {
                  FILE_SIZE_AND_RECIPIENT(SMTP_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
            case DE_MAIL:
               if (toggles_set & SHOW_DEMAIL)
               {
                  FILE_SIZE_AND_RECIPIENT(DEMAIL_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_SFTP_SUPPORT
            case SFTP:
               if (toggles_set & SHOW_SFTP)
               {
                  FILE_SIZE_AND_RECIPIENT(SFTP_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_SCP_SUPPORT
            case SCP:
               if (toggles_set & SHOW_SCP)
               {
                  FILE_SIZE_AND_RECIPIENT(SCP_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_WMO_SUPPORT
            case WMO:
               if (toggles_set & SHOW_WMO)
               {
                  FILE_SIZE_AND_RECIPIENT(WMO_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_MAP_SUPPORT
            case MAP:
               if (toggles_set & SHOW_MAP)
               {
                  FILE_SIZE_AND_RECIPIENT(MAP_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_DFAX_SUPPORT
            case DFAX:
               if (toggles_set & SHOW_DFAX)
               {
                  FILE_SIZE_AND_RECIPIENT(DFAX_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef WITH_SSL
# ifdef _WITH_FTP_SUPPORT
            case FTPS:
               if (toggles_set & SHOW_FTPS)
               {
                  FILE_SIZE_AND_RECIPIENT(FTPS_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
# endif
# ifdef _WITH_HTTP_SUPPORT
            case HTTPS:
               if (toggles_set & SHOW_HTTPS)
               {
                  FILE_SIZE_AND_RECIPIENT(HTTPS_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
# endif
# ifdef _WITH_SMTP_SUPPORT
            case SMTPS:
               if (toggles_set & SHOW_SMTPS)
               {
                  FILE_SIZE_AND_RECIPIENT(SMTPS_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
# endif
#endif
            default :
               FILE_SIZE_AND_RECIPIENT(UNKNOWN_ID_STR);
               break;
         }

         ptr = ptr_start_line + log_date_length + 1 + max_hostname_length + type_offset + 2;
         il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
         time_when_transmitted = (time_t)str2timet(ptr_start_line, (char **)NULL, 16);
         if (first_date_found == -1)
         {
            first_date_found = time_when_transmitted;
         }
         if ((p_ts = localtime(&time_when_transmitted)) == NULL)
         {
            IGNORE_ENTRY();
         }
         CONVERT_TIME();
         if (file_name_toggle_set == REMOTE_FILENAME)
         {
            tmp_ptr = ptr;
            while (*tmp_ptr != SEPARATOR_CHAR)
            {
               tmp_ptr++;
            }
            if (*(tmp_ptr + 1) != SEPARATOR_CHAR)
            {
               ptr = tmp_ptr + 1;
               ptr_is_remote = YES;
            }
            else
            {
               ptr_is_remote = NO;
            }
         }
         else
         {
            ptr_is_remote = NO;
         }
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
         if (ptr_is_remote == NO)
         {
            /* Ignore the remote file name. */
            while (*ptr != SEPARATOR_CHAR)
            {
               ptr++;
            }
            ptr++;
         }

         /* File size is already stored. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }

         /* Write transfer duration, job ID and archive directory. */
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

#ifndef LESSTIF_WORKAROUND
   if (unmanaged == YES)
   {
      XtManageChild(listbox_w);
   }
#endif

   il[file_no].no_of_items = item_counter;

   return;
}


/*---------------------- file_name_size_recipient() ---------------------*/
static void
file_name_size_recipient(register char *ptr,
                         char          *ptr_end,
                         int           file_no,
                         char          *p_start_log_file,
                         off_t         offset)
{
   register int i,
                j;
   int          current_search_host,
                item_counter = il[file_no].no_of_items,
                prev_item_counter = il[file_no].no_of_items,
                ptr_is_remote,
                loops = 0,
#ifndef LESSTIF_WORKAROUND
                unmanaged,
#endif
                ret,
                type,
                type_offset;
   time_t       now,
                prev_time_val = 0L,
                time_when_transmitted = 0L;
   double       tmp_file_size;
   char         *tmp_ptr,
                *ptr_start_line;
#ifdef _WITH_DE_MAIL_SUPPORT
   char         confirmation_sign;
#endif
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

         current_search_host = -1;
         ptr_start_line = ptr;

         if (*(ptr + log_date_length + 1 + max_hostname_length + 2) == ' ')
         {
#ifdef ACTIVATE_THIS_AFTER_VERSION_14
            if ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_DELIVERED)) ||
                (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED)))
            {
               type_offset = 5;
               if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED))
               {
                  if ((view_archived_only == YES) || (view_output_only == YES))
                  {
                     IGNORE_ENTRY();
                  }
                  id.is_receive_job = YES;
               }
               else
               {
                  if (view_received_only == YES)
                  {
                     IGNORE_ENTRY();
                  }
                  id.is_receive_job = NO;
               }
# ifdef _WITH_DE_MAIL_SUPPORT
               confirmation_sign = 0;
# endif
            }
            else
            {
# ifdef _WITH_DE_MAIL_SUPPORT
               if ((view_confirmation == YES) &&
                   ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH)) ||
                    (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT)) ||
                    (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE)) ||
                    (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))))
               {
                  if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH))
                  {
                     confirmation_sign = 'd';
                  }
                  else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT))
                       {
                          confirmation_sign = 'r';
                       }
                  else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE))
                       {
                          confirmation_sign = 'R';
                       }
                  else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))
                       {
                          confirmation_sign = 't';
                       }
                       else
                       {
                          confirmation_sign = 0;
                       }
                  type_offset = 5;
                  id.is_receive_job = NO;
               }
               else
               {
                  IGNORE_ENTRY();
               }
# else
               IGNORE_ENTRY();
# endif
            }
#else
            if (*(ptr + log_date_length + 1 + max_hostname_length + 4) == ' ')
            {
               if ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_DELIVERED)) ||
                   (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED)))
               {
                  type_offset = 5;
                  if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_NORMAL_RECEIVED))
                  {
                     if ((view_archived_only == YES) ||
                         (view_output_only == YES))
                     {
                        IGNORE_ENTRY();
                     }
                     id.is_receive_job = YES;
                  }
                  else
                  {
                     if (view_received_only == YES)
                     {
                        IGNORE_ENTRY();
                     }
                     id.is_receive_job = NO;
                  }
# ifdef _WITH_DE_MAIL_SUPPORT
                  confirmation_sign = 0;
# endif
               }
               else
               {
# ifdef _WITH_DE_MAIL_SUPPORT
                  if ((view_confirmation == YES) &&
                      ((*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH)) ||
                       (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT)) ||
                       (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE)) ||
                       (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))))
                  {
                     if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_DISPATCH))
                     {
                        confirmation_sign = 'd';
                     }
                     else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RECEIPT))
                          {
                             confirmation_sign = 'r';
                          }
                     else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_OF_RETRIEVE))
                          {
                             confirmation_sign = 'R';
                          }
                     else if (*(ptr + log_date_length + 1 + max_hostname_length + 1) == ('0' + OT_CONF_TIMEUP))
                          {
                             confirmation_sign = 't';
                          }
                          else
                          {
                             confirmation_sign = 0;
                          }
                     type_offset = 5;
                     id.is_receive_job = NO;
                  }
                  else
                  {
                     IGNORE_ENTRY();
                  }
# else
                  IGNORE_ENTRY();
# endif
               }
            }
            else
            {
               type_offset = 3;
               id.is_receive_job = NO;
# ifdef _WITH_DE_MAIL_SUPPORT
               confirmation_sign = 0;
# endif
            }
#endif
         }
         else
         {
            type_offset = 1;
            id.is_receive_job = NO;
#ifdef _WITH_DE_MAIL_SUPPORT
            confirmation_sign = 0;
#endif
         }
         HEX_CHAR_TO_INT((*(ptr_start_line + log_date_length + 1 + max_hostname_length + type_offset)))
         switch (type)
         {
#ifdef _WITH_FTP_SUPPORT
            case FTP:
               if (toggles_set & SHOW_FTP)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     int iii,
                         match_found = -1;

                     SET_FILE_NAME_POINTER();
                     for (iii = 0; iii < no_of_search_file_names; iii++)
                     {
                        if ((ret = sfilter(search_file_name[iii], ptr,
                                           SEPARATOR_CHAR)) == 0)
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
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_LOC_SUPPORT
            case LOC:
               if (toggles_set & SHOW_FILE)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     int iii,
                         match_found = -1;

                     SET_FILE_NAME_POINTER();
                     for (iii = 0; iii < no_of_search_file_names; iii++)
                     {
                        if ((ret = sfilter(search_file_name[iii], ptr,
                                           SEPARATOR_CHAR)) == 0)
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
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_FD_EXEC_SUPPORT
            case EXEC:
               if (toggles_set & SHOW_EXEC)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     int iii,
                         match_found = -1;

                     SET_FILE_NAME_POINTER();
                     for (iii = 0; iii < no_of_search_file_names; iii++)
                     {
                        if ((ret = sfilter(search_file_name[iii], ptr,
                                           SEPARATOR_CHAR)) == 0)
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
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_HTTP_SUPPORT
            case HTTP:
               if (toggles_set & SHOW_HTTP)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     int iii,
                         match_found = -1;

                     SET_FILE_NAME_POINTER();
                     for (iii = 0; iii < no_of_search_file_names; iii++)
                     {
                        if ((ret = sfilter(search_file_name[iii], ptr,
                                           SEPARATOR_CHAR)) == 0)
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
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_SMTP_SUPPORT
            case SMTP:
               if (toggles_set & SHOW_SMTP)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     int iii,
                         match_found = -1;

                     SET_FILE_NAME_POINTER();
                     for (iii = 0; iii < no_of_search_file_names; iii++)
                     {
                        if ((ret = sfilter(search_file_name[iii], ptr,
                                           SEPARATOR_CHAR)) == 0)
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
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
            case DE_MAIL:
               if (toggles_set & SHOW_DEMAIL)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     int iii,
                         match_found = -1;

                     SET_FILE_NAME_POINTER();
                     for (iii = 0; iii < no_of_search_file_names; iii++)
                     {
                        if ((ret = sfilter(search_file_name[iii], ptr,
                                           SEPARATOR_CHAR)) == 0)
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
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_SFTP_SUPPORT
            case SFTP:
               if (toggles_set & SHOW_SFTP)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     int iii,
                         match_found = -1;

                     SET_FILE_NAME_POINTER();
                     for (iii = 0; iii < no_of_search_file_names; iii++)
                     {
                        if ((ret = sfilter(search_file_name[iii], ptr,
                                           SEPARATOR_CHAR)) == 0)
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
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_SCP_SUPPORT
            case SCP:
               if (toggles_set & SHOW_SCP)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     int iii,
                         match_found = -1;

                     SET_FILE_NAME_POINTER();
                     for (iii = 0; iii < no_of_search_file_names; iii++)
                     {
                        if ((ret = sfilter(search_file_name[iii], ptr,
                                           SEPARATOR_CHAR)) == 0)
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
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_WMO_SUPPORT
            case WMO:
               if (toggles_set & SHOW_WMO)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     int iii,
                         match_found = -1;

                     SET_FILE_NAME_POINTER();
                     for (iii = 0; iii < no_of_search_file_names; iii++)
                     {
                        if ((ret = sfilter(search_file_name[iii], ptr,
                                           SEPARATOR_CHAR)) == 0)
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
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_MAP_SUPPORT
            case MAP:
               if (toggles_set & SHOW_MAP)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     int iii,
                         match_found = -1;

                     SET_FILE_NAME_POINTER();
                     for (iii = 0; iii < no_of_search_file_names; iii++)
                     {
                        if ((ret = sfilter(search_file_name[iii], ptr,
                                           SEPARATOR_CHAR)) == 0)
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
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef _WITH_DFAX_SUPPORT
            case DFAX:
               if (toggles_set & SHOW_DFAX)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     int iii,
                         match_found = -1;

                     SET_FILE_NAME_POINTER();
                     for (iii = 0; iii < no_of_search_file_names; iii++)
                     {
                        if ((ret = sfilter(search_file_name[iii], ptr,
                                           SEPARATOR_CHAR)) == 0)
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
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
#endif
#ifdef WITH_SSL
# ifdef _WITH_FTP_SUPPORT
            case FTPS:
               if (toggles_set & SHOW_FTPS)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     int iii,
                         match_found = -1;

                     SET_FILE_NAME_POINTER();
                     for (iii = 0; iii < no_of_search_file_names; iii++)
                     {
                        if ((ret = sfilter(search_file_name[iii], ptr,
                                           SEPARATOR_CHAR)) == 0)
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
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
# endif
# ifdef _WITH_HTTP_SUPPORT
            case HTTPS:
               if (toggles_set & SHOW_HTTPS)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     int iii,
                         match_found = -1;

                     SET_FILE_NAME_POINTER();
                     for (iii = 0; iii < no_of_search_file_names; iii++)
                     {
                        if ((ret = sfilter(search_file_name[iii], ptr,
                                           SEPARATOR_CHAR)) == 0)
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
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
# endif
# ifdef _WITH_SMTP_SUPPORT
            case SMTPS:
               if (toggles_set & SHOW_SMTPS)
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     int iii,
                         match_found = -1;

                     SET_FILE_NAME_POINTER();
                     for (iii = 0; iii < no_of_search_file_names; iii++)
                     {
                        if ((ret = sfilter(search_file_name[iii], ptr,
                                           SEPARATOR_CHAR)) == 0)
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
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
               break;
# endif
#endif
            default :
               {
                  int ii;

                  for (ii = 0; ii < no_of_search_hosts; ii++)
                  {
                     if (sfilter(search_recipient[ii], ptr_start_line + log_date_length + 1, ' ') == 0)
                     {
                        current_search_host = ii;
                        break;
                     }
                  }
                  if (current_search_host != -1)
                  {
                     int iii,
                         match_found = -1;

                     SET_FILE_NAME_POINTER();
                     for (iii = 0; iii < no_of_search_file_names; iii++)
                     {
                        if ((ret = sfilter(search_file_name[iii], ptr,
                                           SEPARATOR_CHAR)) == 0)
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
                  else
                  {
                     IGNORE_ENTRY();
                  }
               }
               break;
         }

         il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);

         /* If necessary, ignore rest of file name. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;
         if (ptr_is_remote == NO)
         {
            /* Ignore the remote file name. */
            while (*ptr != SEPARATOR_CHAR)
            {
               ptr++;
            }
            ptr++;
         }

         /* Get file size and check if it fits. */
         j = 0;
         while (*(ptr + j) != SEPARATOR_CHAR)
         {
            j++;
         }
         ptr += j;
#ifdef HAVE_STRTOULL
         if (j > 15)
         {
            tmp_file_size = strtod("INF", NULL);
         }
         else
         {
            tmp_file_size = (double)strtoull(ptr - j, NULL, 16);
         }
#else
         if (j < 9)
         {
            tmp_file_size = (double)strtoul(ptr - j, NULL, 16);
         }
         else
         {
# ifdef LINUX
            if (j > 15)
            {
               tmp_file_size = strtod("INF", NULL);
            }
            else
            {
               char tmp_buf[23];

               tmp_buf[0] = '0'; tmp_buf[1] = 'x';
               (void)memcpy(&tmp_buf[2], ptr - j, j);
               tmp_buf[2 + j] = '\0';
               tmp_file_size = strtod(tmp_buf, NULL);
            }
# else
            tmp_file_size = strtod("INF", NULL);
# endif
         }
#endif
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

         ptr = ptr_start_line + log_date_length + 1 + max_hostname_length + type_offset + 2;
         (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);
         (void)memcpy(p_host_name, ptr_start_line + log_date_length + 1,
                      (MAX_HOSTNAME_LENGTH > max_hostname_length) ? max_hostname_length : MAX_HOSTNAME_LENGTH);
         time_when_transmitted = (time_t)str2timet(ptr_start_line, (char **)NULL, 16);
         if (first_date_found == -1)
         {
            first_date_found = time_when_transmitted;
         }
         if ((p_ts = localtime(&time_when_transmitted)) == NULL)
         {
            IGNORE_ENTRY();
         }
         CONVERT_TIME();
         switch (type)
         {
#ifdef _WITH_FTP_SUPPORT
            case FTP:
               (void)memcpy(p_type, FTP_ID_STR, 5);
               break;
#endif
#ifdef _WITH_LOC_SUPPORT
            case LOC:
               (void)memcpy(p_type, FILE_ID_STR, 5);
               break;
#endif
#ifdef _WITH_FD_EXEC_SUPPORT
            case EXEC:
               (void)memcpy(p_type, EXEC_ID_STR, 5);
               break;
#endif
#ifdef _WITH_HTTP_SUPPORT
            case HTTP:
               (void)memcpy(p_type, HTTP_ID_STR, 5);
               break;
#endif
#ifdef _WITH_SMTP_SUPPORT
            case SMTP:
               (void)memcpy(p_type, SMTP_ID_STR, 5);
               break;
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
            case DE_MAIL:
               (void)memcpy(p_type, DEMAIL_ID_STR, 5);
               break;
#endif
#ifdef _WITH_SFTP_SUPPORT
            case SFTP:
               (void)memcpy(p_type, SFTP_ID_STR, 5);
               break;
#endif
#ifdef _WITH_SCP_SUPPORT
            case SCP:
               (void)memcpy(p_type, SCP_ID_STR, 5);
               break;
#endif
#ifdef _WITH_WMO_SUPPORT
            case WMO:
               (void)memcpy(p_type, WMO_ID_STR, 5);
               break;
#endif
#ifdef _WITH_MAP_SUPPORT
            case MAP:
               (void)memcpy(p_type, MAP_ID_STR, 5);
               break;
#endif
#ifdef _WITH_DFAX_SUPPORT
            case DFAX:
               (void)memcpy(p_type, DFAX_ID_STR, 5);
               break;
#endif
#ifdef WITH_SSL
# ifdef _WITH_FTP_SUPPORT
            case FTPS:
               (void)memcpy(p_type, FTPS_ID_STR, 5);
               break;
# endif
# ifdef _WITH_HTTP_SUPPORT
            case HTTPS:
               (void)memcpy(p_type, HTTPS_ID_STR, 5);
               break;
# endif
# ifdef _WITH_SMTP_SUPPORT
            case SMTPS:
               (void)memcpy(p_type, SMTPS_ID_STR, 5);
               break;
# endif
#endif
            default :
               (void)memcpy(p_type, UNKNOWN_ID_STR, 5);
               break;
         }
         if (file_name_toggle_set == REMOTE_FILENAME)
         {
            tmp_ptr = ptr;
            while (*tmp_ptr != SEPARATOR_CHAR)
            {
               tmp_ptr++;
            }
            if (*(tmp_ptr + 1) != SEPARATOR_CHAR)
            {
               ptr = tmp_ptr + 1;
               ptr_is_remote = YES;
            }
            else
            {
               ptr_is_remote = NO;
            }
         }
         else
         {
            ptr_is_remote = NO;
         }
         j = 0;
         while ((*ptr != SEPARATOR_CHAR) && (j < file_name_length))
         {
            if ((unsigned char)(*ptr) < ' ')
            {
               *(p_file_name + j) = '?';
               unprintable_chars++;
            }
            else
            {
               *(p_file_name + j) = *ptr;
            }
            ptr++; j++;
         }
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;
         if (ptr_is_remote == NO)
         {
            /* Ignore the remote file name. */
            while (*ptr != SEPARATOR_CHAR)
            {
               ptr++;
            }
            ptr++;
         }

         /* Write file size. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         print_file_size(p_file_size, (off_t)tmp_file_size);

         /* Write transfer duration, job ID and archive directory. */
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
                     total_no_files, file_size, trans_time);
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
