/*
 *  get_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_data - searches the AFD queues for files
 **
 ** SYNOPSIS
 **   void get_data(void)
 **
 ** DESCRIPTION
 **   This function searches for the selected data in the
 **   queues of the AFD. The following things can be selected:
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
 **   22.07.2001 H.Kiehl Created
 **   03.01.2005 H.Kiehl Adapted to new message names.
 **   11.06.2006 H.Kiehl Show retrieve jobs.
 **   19.11.2012 H.Kiehl Check if retrieve jobs ae in given time frame.
 **
 */
DESCR__E_M3

#include <stdio.h>        /* sprintf()                                   */
#include <string.h>       /* strerror()                                  */
#include <stdlib.h>       /* malloc(), realloc(), free(), strtod(), abs()*/
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
#include <Xm/List.h>
#include <Xm/Text.h>
#include "motif_common_defs.h"
#include "show_queue.h"
#include "fddefs.h"

#define _WITH_HEAPSORT

/* External global variables. */
extern Display                    *display;
extern Widget                     appshell, /* CHECK_INTERRUPT() */
                                  listbox_w,
                                  scrollbar_w,
                                  special_button_w,
                                  statusbox_w,
                                  summarybox_w;
extern int                        file_name_length,
                                  radio_set,
                                  gt_lt_sign,
                                  no_of_dirs,
                                  no_of_search_dirs,
                                  no_of_search_dirids,
                                  no_of_search_file_names,
                                  no_of_search_hosts,
                                  queue_tmp_buf_entries,
                                  *search_dir_length,
                                  special_button_flag;
extern unsigned int               *search_dirid,
                                  total_no_files,
                                  unprintable_chars;
extern XT_PTR_TYPE                toggles_set;
extern size_t                     search_file_size;
extern time_t                     start_time_val,
                                  end_time_val;
extern double                     total_file_size;
extern char                       *p_work_dir,
                                  **search_file_name,
                                  **search_dir,
                                  *search_dir_filter,
                                  **search_recipient,
                                  **search_user,
                                  summary_str[],
                                  total_summary_str[];
extern struct sol_perm            perm;
extern struct queued_file_list    *qfl;
extern struct queue_tmp_buf       *qtb;
extern struct fileretrieve_status *fra;

/* Local global variables. */
static int                        no_of_dnb_dirs,
                                  no_of_jobs;
static char                       limit_reached;
static struct dir_name_buf        *dnb;
static struct job_id_data         *jd;

/* Local function prototypes. */
static void                       get_all_input_files(void),
                                  get_input_files(void),
                                  get_output_files(void),
                                  get_retrieve_jobs(void),
#ifdef MULTI_FS_SUPPORT
                                  get_all_time_jobs(void),
                                  get_time_jobs(char *, char *, char *),
#else
                                  get_time_jobs(void),
#endif
                                  insert_file(char *, char *, char *, char *,
                                              char, char, unsigned int, int,
                                              unsigned int, unsigned int, int),
                                  searching(char *),
#ifdef _WITH_HEAPSORT
                                  sort_data(int);
#else
                                  sort_data(int, int);
#endif
static int                        check_all_file_names(char *),
                                  get_job_id(char *, unsigned int *),
                                  get_pos(unsigned int),
                                  lookup_fra_pos(unsigned int);


/*############################### get_data() ############################*/
void
get_data(void)
{
   int          fd;
   off_t        dnb_size,
                jd_size;
   time_t       end,
                start;
   char         fullname[MAX_PATH_LENGTH],
                status_message[MAX_MESSAGE_LENGTH];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif
   XmString     xstr;

   /* Map to directory name buffer. */
   (void)sprintf(fullname, "%s%s%s", p_work_dir, FIFO_DIR,
                 DIR_NAME_FILE);
   if ((fd = open(fullname, O_RDONLY)) == -1)
   {
      (void)xrec(ERROR_DIALOG, "Failed to open() <%s> : %s (%s %d)",
                 fullname, strerror(errno), __FILE__, __LINE__);
      return;
   }
#ifdef HAVE_STATX
   if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(fd, &stat_buf) == -1)
#endif
   {
      (void)xrec(ERROR_DIALOG, "Failed to access <%s> : %s (%s %d)",
                 fullname, strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      return;
   }
#ifdef HAVE_STATX
   if (stat_buf.stx_size > 0)
#else
   if (stat_buf.st_size > 0)
#endif
   {
      char *ptr;

#ifdef HAVE_MMAP
      if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                      stat_buf.stx_size, PROT_READ,
# else
                      stat_buf.st_size, PROT_READ,
# endif
                      MAP_SHARED, fd, 0)) == (caddr_t) -1)
#else
      if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                          stat_buf.stx_size, PROT_READ,
# else
                          stat_buf.st_size, PROT_READ,
# endif
                          MAP_SHARED, fullname, 0)) == (caddr_t) -1)
#endif
      {
         (void)xrec(ERROR_DIALOG, "Failed to mmap() to <%s> : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return;
      }
#ifdef HAVE_STATX
      dnb_size = stat_buf.stx_size;
#else
      dnb_size = stat_buf.st_size;
#endif
      no_of_dnb_dirs = *(int *)ptr;
      ptr += AFD_WORD_OFFSET;
      dnb = (struct dir_name_buf *)ptr;
      (void)close(fd);
   }
   else
   {
      (void)xrec(ERROR_DIALOG, "Dirname database file is empty. (%s %d)",
                 __FILE__, __LINE__);
      (void)close(fd);
      return;
   }

   /* Map to job ID data file. */
   (void)sprintf(fullname, "%s%s%s", p_work_dir, FIFO_DIR,
                 JOB_ID_DATA_FILE);
   if ((fd = open(fullname, O_RDONLY)) == -1)
   {
      (void)xrec(ERROR_DIALOG, "Failed to open() %s : %s (%s %d)",
                 fullname, strerror(errno), __FILE__, __LINE__);
      return;
   }
#ifdef HAVE_STATX
   if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(fd, &stat_buf) == -1)
#endif
   {
      (void)xrec(ERROR_DIALOG, "Failed to access %s : %s (%s %d)",
                 fullname, strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      return;
   }
#ifdef HAVE_STATX
   if (stat_buf.stx_size > 0)
#else
   if (stat_buf.st_size > 0)
#endif
   {
      char *ptr;

#ifdef HAVE_MMAP
      if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                      stat_buf.stx_size, PROT_READ,
# else
                      stat_buf.st_size, PROT_READ,
# endif
                      MAP_SHARED, fd, 0)) == (caddr_t) -1)
#else
      if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                          stat_buf.stx_size, PROT_READ,
# else
                          stat_buf.st_size, PROT_READ,
# endif
                          MAP_SHARED, fullname, 0)) == (caddr_t) -1)
#endif
      {
         (void)xrec(ERROR_DIALOG, "Failed to mmap() to %s : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return;
      }
      if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_JID_VERSION)
      {
         (void)xrec(ERROR_DIALOG, "Incorrect JID version (data=%d current=%d)!",
                    *(ptr + SIZEOF_INT + 1 + 1 + 1), CURRENT_JID_VERSION);
         (void)close(fd);
         return;
      }
#ifdef HAVE_STATX
      jd_size = stat_buf.stx_size;
#else
      jd_size = stat_buf.st_size;
#endif
      no_of_jobs = *(int *)ptr;
      ptr += AFD_WORD_OFFSET;
      jd = (struct job_id_data *)ptr;
      (void)close(fd);
   }
   else
   {
      (void)xrec(ERROR_DIALOG, "Job ID database file is empty. (%s %d)",
                 __FILE__, __LINE__);
      (void)close(fd);
      return;
   }

   special_button_flag = STOP_BUTTON;
   xstr = XmStringCreateLtoR("Stop", XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);

   (void)memset(summary_str, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);
   summary_str[MAX_OUTPUT_LINE_LENGTH + file_name_length] = '\0';
   XmTextSetString(summarybox_w, summary_str);
   CHECK_INTERRUPT();

   if ((fd = fra_attach_passive()) != SUCCESS)
   {
      if (fd == INCORRECT_VERSION)
      {
         (void)xrec(FATAL_DIALOG,
                    "This program is not able to attach to the FRA due to incorrect version (%s %d)", __FILE__, __LINE__);
      }
      else
      {
         if (fd < 0)
         {
            (void)xrec(FATAL_DIALOG,
                       "Failed to attach to FRA (%s %d)", __FILE__, __LINE__);
         }
         else
         {
            (void)xrec(FATAL_DIALOG,
                       "Failed to attach to FRA : %s (%s %d)",
                       strerror(fd), __FILE__, __LINE__);
         }
      }
   }

   start = time(NULL);
   limit_reached = NO;
   total_file_size = 0.0;
   total_no_files = 0;
   unprintable_chars = 0;
   if ((toggles_set & SHOW_OUTPUT) || (toggles_set & SHOW_UNSENT_OUTPUT))
   {
      get_output_files();
   }
   if (toggles_set & SHOW_INPUT)
   {
      get_input_files();
   }
   if (toggles_set & SHOW_UNSENT_INPUT)
   {
      get_all_input_files();
   }
   if ((toggles_set & SHOW_RETRIEVES) || (toggles_set & SHOW_PENDING_RETRIEVES))
   {
      get_retrieve_jobs();
   }
   if (toggles_set & SHOW_TIME_JOBS)
   {
#ifdef MULTI_FS_SUPPORT
      get_all_time_jobs();
#else
      get_time_jobs();
#endif
   }
   (void)fra_detach();

#ifdef WHEN_WE_KNOW
   if ((total_no_files != 0) || (toggles_set & SHOW_RETRIEVES) ||
       (toggles_set & SHOW_PENDING_RETRIEVES))
#else
   if (total_no_files != 0)
#endif
   {
      (void)strcpy(status_message, "Sorting...");
      SHOW_MESSAGE();
#ifdef _WITH_HEAPSORT
      sort_data(total_no_files);
#else
      sort_data(0, total_no_files - 1);
#endif
      (void)strcpy(status_message, "Displaying...");
      SHOW_MESSAGE();
      display_data();
      end = time(NULL);
#if SIZEOF_TIME_T == 4
      fd = sprintf(status_message, "Search time: %lds",
#else
      fd = sprintf(status_message, "Search time: %llds",
#endif
                   (pri_time_t)(end - start));
      if (unprintable_chars > 0)
      {
         (void)sprintf(status_message + fd, " (%u unprintable chars!)",
                       unprintable_chars);
      }
      SHOW_MESSAGE();
   }
   else
   {
      end = time(NULL);
#if SIZEOF_TIME_T == 4
      fd = sprintf(status_message, "No data found. Search time: %lds",
#else
      fd = sprintf(status_message, "No data found. Search time: %llds",
#endif
                   (pri_time_t)(end - start));
      if (unprintable_chars > 0)
      {
         (void)sprintf(status_message + fd, " (%u unprintable chars!)",
                       unprintable_chars);
      }
      SHOW_MESSAGE();
   }

#ifdef HAVE_MMAP
   if (munmap(((char *)dnb - AFD_WORD_OFFSET), dnb_size) == -1)
#else
   if (munmap_emu(((char *)dnb - AFD_WORD_OFFSET)) == -1)
#endif
   {
      (void)xrec(INFO_DIALOG, "munmap() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
   }
#ifdef HAVE_MMAP
   if (munmap(((char *)jd - AFD_WORD_OFFSET), jd_size) == -1)
#else
   if (munmap_emu(((char *)jd - AFD_WORD_OFFSET)) == -1)
#endif
   {
      (void)xrec(INFO_DIALOG, "munmap() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
   }
   show_summary(total_no_files, total_file_size);

   special_button_flag = SEARCH_BUTTON;
   xstr = XmStringCreateLtoR("Search", XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);

   return;
}


/*+++++++++++++++++++++++++++ get_output_files() ++++++++++++++++++++++++*/
static void
get_output_files(void)
{
   int fd;
   char fullname[MAX_PATH_LENGTH];

   (void)sprintf(fullname, "%s%s%s", p_work_dir, FIFO_DIR, MSG_QUEUE_FILE);
   if ((fd = open(fullname, O_RDWR)) == -1)
   {
      (void)xrec(FATAL_DIALOG, "Failed to open() <%s> : %s (%s %d)",
                 fullname, strerror(errno), __FILE__, __LINE__);
   }
   else
   {
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat stat_buf;
#endif

#ifdef HAVE_STATX
      if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(fd, &stat_buf) == -1)
#endif
      {
         (void)xrec(FATAL_DIALOG, "Failed to access <%s> : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
      }
      else
      {
#ifdef HAVE_STATX
         if (stat_buf.stx_size > 0)
#else
         if (stat_buf.st_size > 0)
#endif
         {
            char *buffer;

#ifdef HAVE_STATX
            if ((buffer = malloc(stat_buf.stx_size)) == NULL)
#else
            if ((buffer = malloc(stat_buf.st_size)) == NULL)
#endif
            {
               (void)xrec(FATAL_DIALOG, "malloc() error : %s (%s %d)",
                          strerror(errno), __FILE__, __LINE__);
            }
            else
            {
#ifdef HAVE_STATX
               if (read(fd, buffer, stat_buf.stx_size) != stat_buf.stx_size)
#else
               if (read(fd, buffer, stat_buf.st_size) != stat_buf.st_size)
#endif
               {
                  (void)xrec(FATAL_DIALOG, "Failed to read() <%s> : %s (%s %d)",
                             fullname, strerror(errno), __FILE__, __LINE__);
               }
               else
               {
                  register int     i;
                  unsigned int     job_id;
                  int              gotcha,
                                   no_msg_queued,
                                   pos,
                                   ret;
                  char             *p_queue_msg,
                                   queue_dir[MAX_PATH_LENGTH];
                  struct queue_buf *qb;

                  p_queue_msg = queue_dir + sprintf(queue_dir, "%s%s%s/",
                                                    p_work_dir, AFD_FILE_DIR,
                                                    OUTGOING_DIR);

                  no_msg_queued = *(int *)buffer;
                  buffer += AFD_WORD_OFFSET;
                  qb = (struct queue_buf *)buffer;

                  for (i = 0; ((i < no_msg_queued) && (limit_reached == NO)); i++)
                  {
                     if ((qb[i].msg_name[0] != '\0') &&
                         (((toggles_set & SHOW_OUTPUT) &&
                           ((toggles_set & SHOW_UNSENT_OUTPUT) ||
                            (qb[i].pid == PENDING))) ||
                          (((toggles_set & SHOW_OUTPUT) == 0) &&
                            (toggles_set & SHOW_UNSENT_OUTPUT) &&
                            (qb[i].pid != PENDING))) &&
                         (get_job_id(qb[i].msg_name, &job_id) != -1) &&
                         ((pos = get_pos(job_id)) != -1))
                     {
                        if (no_of_search_hosts == 0)
                        {
                           gotcha = YES;
                        }
                        else
                        {
                           register int j;

                           gotcha = NO;
                           for (j = 0; j < no_of_search_hosts; j++)
                           {
                              if ((ret = pmatch(search_recipient[j],
                                                jd[pos].host_alias, NULL)) == 0)
                              {
                                 gotcha = YES;
                                 break;
                              }
                              else if (ret == 1)
                                   {
                                      /* This host is NOT wanted. */
                                      break;
                                   }
                           }
                        }

                        if (gotcha == YES)
                        {
                           if ((no_of_search_dirs > 0) ||
                               (no_of_search_dirids > 0))
                           {
                              int    kk;
                              size_t dir_name_length;

                              /* Check if an input directory was specified. */
                              gotcha = NO;
                              for (kk = 0; kk < no_of_search_dirids; kk++)
                              {
                                 if (search_dirid[kk] == dnb[jd[pos].dir_id_pos].dir_id)
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
                                       if (sfilter(search_dir[kk],
                                                   dnb[jd[pos].dir_id_pos].dir_name,
                                                   0) == 0)
                                       {
                                          gotcha = YES;
                                          break;
                                       }
                                    }
                                    else
                                    {
                                       dir_name_length = strlen(dnb[jd[pos].dir_id_pos].dir_name);
                                       if (search_dir_length[kk] == dir_name_length)
                                       {
                                          if (strncmp(search_dir[kk], 
                                                      dnb[jd[pos].dir_id_pos].dir_name,
                                                      dir_name_length) == 0)
                                          {
                                             gotcha = YES;
                                             break;
                                          }
                                       }
                                    }
                                 }
                              }
                           }
                           if (gotcha == YES)
                           {
                              int  fra_pos;
                              char *ptr_file,
                                   queue_typ;

                              ptr_file = p_queue_msg +
                                         sprintf(p_queue_msg, "%s/",
                                         qb[i].msg_name);
                              if (qb[i].pid == PENDING)
                              {
                                 queue_typ = SHOW_OUTPUT;
                              }
                              else
                              {
                                 queue_typ = SHOW_UNSENT_OUTPUT;
                              }
                              fra_pos = lookup_fra_pos(jd[pos].dir_id);
                              insert_file(queue_dir, ptr_file, qb[i].msg_name,
                                          jd[pos].host_alias, queue_typ,
                                          jd[pos].priority, job_id,
                                          jd[pos].dir_id_pos, jd[pos].dir_id,
                                          qb[i].files_to_send, fra_pos);
                           }
                        }
                     }

                     if ((i % 100) == 0)
                     {
                        searching("Output");
                     }
                  }
               }
               free(buffer - AFD_WORD_OFFSET);
            }
         }
      }

      if (close(fd) == -1)
      {
         (void)xrec(INFO_DIALOG, "Failed to close() <%s> : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
      }
   }
   searching("Output");

   return;
}


/*++++++++++++++++++++++++++ get_retrieve_jobs() ++++++++++++++++++++++++*/
static void
get_retrieve_jobs(void)
{
   int fd;
   char fullname[MAX_PATH_LENGTH];

   (void)sprintf(fullname, "%s%s%s", p_work_dir, FIFO_DIR, MSG_QUEUE_FILE);
   if ((fd = open(fullname, O_RDWR)) == -1)
   {
      (void)xrec(FATAL_DIALOG, "Failed to open() <%s> : %s (%s %d)",
                 fullname, strerror(errno), __FILE__, __LINE__);
   }
   else
   {
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat stat_buf;
#endif

#ifdef HAVE_STATX
      if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(fd, &stat_buf) == -1)
#endif
      {
         (void)xrec(FATAL_DIALOG, "Failed to access <%s> : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
      }
      else
      {
#ifdef HAVE_STATX
         if (stat_buf.stx_size > 0)
#else
         if (stat_buf.st_size > 0)
#endif
         {
            char *buffer;

#ifdef HAVE_STATX
            if ((buffer = malloc(stat_buf.stx_size)) == NULL)
#else
            if ((buffer = malloc(stat_buf.st_size)) == NULL)
#endif
            {
               (void)xrec(FATAL_DIALOG, "malloc() error : %s (%s %d)",
                          strerror(errno), __FILE__, __LINE__);
            }
            else
            {
#ifdef HAVE_STATX
               if (read(fd, buffer, stat_buf.stx_size) != stat_buf.stx_size)
#else
               if (read(fd, buffer, stat_buf.st_size) != stat_buf.st_size)
#endif
               {
                  (void)xrec(FATAL_DIALOG, "Failed to read() <%s> : %s (%s %d)",
                             fullname, strerror(errno), __FILE__, __LINE__);
               }
               else
               {
                  register int     i;
                  int              gotcha,
                                   no_msg_queued,
                                   ret;
                  char             queue_dir[MAX_PATH_LENGTH];
                  struct queue_buf *qb;

                  (void)sprintf(queue_dir, "%s%s%s/",
                                p_work_dir, AFD_FILE_DIR, OUTGOING_DIR);

                  no_msg_queued = *(int *)buffer;
                  buffer += AFD_WORD_OFFSET;
                  qb = (struct queue_buf *)buffer;

                  for (i = 0; ((i < no_msg_queued) && (limit_reached == NO)); i++)
                  {
                     if ((qb[i].msg_name[0] == '\0') &&
                         (((toggles_set & SHOW_RETRIEVES) &&
                           (qb[i].pid != PENDING)) ||
                          ((toggles_set & SHOW_PENDING_RETRIEVES) &&
                           (qb[i].pid == PENDING))))
                     {
                        if (no_of_search_hosts == 0)
                        {
                           gotcha = YES;
                        }
                        else
                        {
                           register int j;

                           gotcha = NO;
                           for (j = 0; j < no_of_search_hosts; j++)
                           {
                              if ((ret = pmatch(search_recipient[j],
                                                fra[qb[i].pos].host_alias,
                                                NULL)) == 0)
                              {
                                 gotcha = YES;
                                 break;
                              }
                              else if (ret == 1)
                                   {
                                      /* This host is NOT wanted. */
                                      break;
                                   }
                           }
                        }

                        if (gotcha == YES)
                        {
                           if ((no_of_search_dirs > 0) ||
                               (no_of_search_dirids > 0))
                           {
                              int    kk;
                              size_t dir_name_length;

                              /* Check if an input directory was specified. */
                              gotcha = NO;
                              for (kk = 0; kk < no_of_search_dirids; kk++)
                              {
                                 if (search_dirid[kk] == fra[qb[i].pos].dir_id)
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
                                       if (sfilter(search_dir[kk],
                                                   fra[qb[i].pos].url,
                                                   0) == 0)
                                       {
                                          gotcha = YES;
                                          break;
                                       }
                                    }
                                    else
                                    {
                                       dir_name_length = strlen(fra[qb[i].pos].url);
                                       if (search_dir_length[kk] == dir_name_length)
                                       {
                                          if (strncmp(search_dir[kk], 
                                                      fra[qb[i].pos].url,
                                                      dir_name_length) == 0)
                                          {
                                             gotcha = YES;
                                             break;
                                          }
                                       }
                                    }
                                 }
                              }
                           }

                           if (gotcha == YES)
                           {
                              /* If necessary check if its in the time span. */
                              if (((start_time_val == -1) ||
                                   (qb[i].creation_time >= start_time_val)) &&
                                  ((end_time_val == -1) ||
                                   (qb[i].creation_time <= end_time_val)))
                              {
                                 int  j;
                                 char queue_typ;

                                 if (qb[i].pid == PENDING)
                                 {
                                    queue_typ = SHOW_PENDING_RETRIEVES;
                                 }
                                 else
                                 {
                                    queue_typ = SHOW_RETRIEVES;
                                 }

                                 /* Insert job. */
                                 if ((total_no_files % 50) == 0)
                                 {
                                    size_t new_size = (((total_no_files / 50) + 1) * 50) *
                                                      sizeof(struct queued_file_list);

                                    if (total_no_files == 0)
                                    {
                                       if ((qfl = malloc(new_size)) == NULL)
                                       {
                                          (void)xrec(FATAL_DIALOG,
                                                     "malloc() error : %s (%s %d)",
                                                     strerror(errno),
                                                     __FILE__, __LINE__);
                                          return;
                                       }
                                    }
                                    else
                                    {
                                       struct queued_file_list *tmp_qfl;

                                       if ((tmp_qfl = realloc((char *)qfl, new_size)) == NULL)
                                       {
                                          int tmp_errno = errno;

                                          free(qfl);
                                          (void)xrec(FATAL_DIALOG,
                                                     "realloc() error : %s (%s %d)",
                                                     strerror(tmp_errno),
                                                     __FILE__, __LINE__);
                                          return;
                                       }
                                       qfl = tmp_qfl;
                                    }
                                 }
                                 if ((qfl[total_no_files].file_name = malloc(1)) == NULL)
                                 {
                                    (void)xrec(FATAL_DIALOG,
                                               "malloc() error : %s (%s %d)",
                                               strerror(errno), __FILE__, __LINE__);
                                    return;
                                 }
                                 qfl[total_no_files].msg_number = qb[i].msg_number;
                                 qfl[total_no_files].pos = qb[i].pos;
                                 qfl[total_no_files].job_id = 0;
                                 qfl[total_no_files].dir_id = fra[qb[i].pos].dir_id;
                                 qfl[total_no_files].size = 0;
                                 qfl[total_no_files].mtime = qb[i].creation_time;
                                 qfl[total_no_files].dir_id_pos = 0;
                                 for (j = 0; j < no_of_dnb_dirs; j++)
                                 {
                                    if (qfl[total_no_files].dir_id == dnb[j].dir_id)
                                    {
                                       qfl[total_no_files].dir_id_pos = j;
                                       break;
                                    }
                                 }
                                 qfl[total_no_files].queue_type = queue_typ;
                                 qfl[total_no_files].priority = 0;
                                 (void)strcpy(qfl[total_no_files].hostname,
                                              fra[qb[i].pos].host_alias);
                                 (void)strcpy(qfl[total_no_files].dir_alias,
                                              fra[qb[i].pos].dir_alias);
                                 qfl[total_no_files].file_name[0] = '\0';
                                 qfl[total_no_files].msg_name[0] = '\0';
                                 qfl[total_no_files].queue_tmp_buf_pos = -1;
                                 total_no_files++;
                              }
                           }
                        }
                     }

                     if ((i % 100) == 0)
                     {
                        searching("Retrieve");
                     }
                  }
               }
               free(buffer - AFD_WORD_OFFSET);
            }
         }
      }

      if (close(fd) == -1)
      {
         (void)xrec(INFO_DIALOG, "Failed to close() <%s> : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
      }
   }
   searching("Retrieve");

   return;
}


/*++++++++++++++++++++++++++++ get_input_files() ++++++++++++++++++++++++*/
static void
get_input_files(void)
{
   register int  i, kk;
   int           gotcha;
   size_t        dir_name_length;
   DIR           *dp;
   struct dirent *p_dir;
#ifdef HAVE_STATX
   struct statx  stat_buf;
#else
   struct stat   stat_buf;
#endif

   for (i = 0; ((i < no_of_dnb_dirs) && (limit_reached == NO)); i++)
   {
      if ((no_of_search_dirs > 0) || (no_of_search_dirids > 0))
      {
         gotcha = NO;
         for (kk = 0; kk < no_of_search_dirids; kk++)
         {
            if (search_dirid[kk] == dnb[i].dir_id)
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
                  if (sfilter(search_dir[kk], dnb[i].dir_name, 0) == 0)
                  {
                     gotcha = YES;
                     break;
                  }
               }
               else
               {
                  dir_name_length = strlen(dnb[i].dir_name);
                  if (search_dir_length[kk] == dir_name_length)
                  {
                     if (strncmp(search_dir[kk], dnb[i].dir_name,
                                 dir_name_length) == 0)
                     {
                        gotcha = YES;
                        break;
                     }
                  }
               }
            }
         }
      }
      else
      {
         gotcha = YES;
      }
      if (gotcha == YES)
      {
         if ((dp = opendir(dnb[i].dir_name)) != NULL)
         {
            int fra_pos;

            if ((fra_pos = lookup_fra_pos(dnb[i].dir_id)) != -1)
            {
               while (((p_dir = readdir(dp)) != NULL) && (limit_reached == NO))
               {
                  if ((p_dir->d_name[0] == '.') && (p_dir->d_name[1] != '\0') &&
                      (p_dir->d_name[1] != '.') &&
                      (strlen(&p_dir->d_name[1]) <= MAX_HOSTNAME_LENGTH))
                  {
                     char *p_file,
                          queue_dir[MAX_PATH_LENGTH + 1 + MAX_FILENAME_LENGTH + 2];

                     p_file = queue_dir + sprintf(queue_dir, "%s/%s/",
                                                  dnb[i].dir_name,
                                                  p_dir->d_name);
#ifdef HAVE_STATX
                     if ((statx(0, queue_dir, AT_STATX_SYNC_AS_STAT,
                                STATX_MODE, &stat_buf) != -1) &&
                         (S_ISDIR(stat_buf.stx_mode)))
#else
                     if ((stat(queue_dir, &stat_buf) != -1) &&
                         (S_ISDIR(stat_buf.st_mode)))
#endif
                     {
                        int gotcha;

                        if (no_of_search_hosts == 0)
                        {
                           gotcha = YES;
                        }
                        else
                        {
                           register int j;
                           int          ret;

                           gotcha = NO;
                           for (j = 0; j < no_of_search_hosts; j++)
                           {
                              if ((ret = pmatch(search_recipient[j],
                                                &p_dir->d_name[1], NULL)) == 0)
                              {
                                 gotcha = YES;
                                 break;
                              }
                              else if (ret == 1)
                                   {
                                      /* This host is NOT wanted. */
                                      break;
                                   }
                           }
                        }

                        if (gotcha == YES)
                        {
                           insert_file(queue_dir, p_file, "\0",
                                       &p_dir->d_name[1], SHOW_INPUT, 0, -1,
                                       i, dnb[i].dir_id, 0, fra_pos);
                        }
                     }
                  }
               }
            }
            if (closedir(dp) == -1)
            {
               (void)xrec(INFO_DIALOG, "Failed to closedir() `%s' : %s (%s %d)",
                          dnb[i].dir_name, strerror(errno), __FILE__, __LINE__);
            }
         }
      }
      searching("Input");
   } /* for (i = 0; i < no_of_dnb_dirs; i++) */
   searching("Input");

   return;
}


/*++++++++++++++++++++++++++ get_all_input_files() ++++++++++++++++++++++*/
static void
get_all_input_files(void)
{
   register int i, kk;
   int          gotcha;
   size_t       dir_name_length;

   for (i = 0; ((i < no_of_dnb_dirs) && (limit_reached == NO)); i++)
   {
      if ((no_of_search_dirs > 0) || (no_of_search_dirids > 0))
      {
         gotcha = NO;
         for (kk = 0; kk < no_of_search_dirids; kk++)
         {
            if (search_dirid[kk] == dnb[i].dir_id)
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
                  if (sfilter(search_dir[kk], dnb[i].dir_name, 0) == 0)
                  {
                     gotcha = YES;
                     break;
                  }
               }
               else
               {
                  dir_name_length = strlen(dnb[i].dir_name);
                  if (search_dir_length[kk] == dir_name_length)
                  {
                     if (strncmp(search_dir[kk], dnb[i].dir_name,
                                 dir_name_length) == 0)
                     {
                        gotcha = YES;
                        break;
                     }
                  }
               }
            }
         }
      }
      else
      {
         gotcha = YES;
      }
      if (gotcha == YES)
      {
         int gotcha;

         if (no_of_search_hosts == 0)
         {
            gotcha = YES;
         }
         else
         {
            register int j, k;
            int          ret;

            gotcha = NO;
            for (j = 0; ((j < no_of_jobs) && (gotcha == NO)); j++)
            {
               if (jd[j].dir_id_pos == i)
               {
                  for (k = 0; k < no_of_search_hosts; k++)
                  {
                     if ((ret = pmatch(search_recipient[k],
                                       jd[j].host_alias, NULL)) == 0)
                     {
                        gotcha = YES;
                        break;
                     }
                     else if (ret == 1)
                          {
                             /* This host is NOT wanted. */
                             break;
                          }
                  }
               }
            }
         }
         if (gotcha == YES)
         {
            int  fra_pos;
            char input_dir[MAX_PATH_LENGTH + 2],
                 *ptr_file;

            ptr_file = input_dir + sprintf(input_dir, "%s/",
                       dnb[i].dir_name);
            fra_pos = lookup_fra_pos(dnb[i].dir_id);
            insert_file(input_dir, ptr_file, "\0", "\0", SHOW_UNSENT_INPUT,
                        0, -1, i, dnb[i].dir_id, 0, fra_pos);
         }
      }
      searching("Unsent");
   }
   searching("Unsent");

   return;
}


#ifdef MULTI_FS_SUPPORT
/*++++++++++++++++++++++++++ get_all_time_jobs() ++++++++++++++++++++++++*/
static void
get_all_time_jobs(void)
{
   char          fullname[MAX_PATH_LENGTH],
                 *p_file,
                 *p_queue;
   DIR           *dp;
   struct dirent *p_dir;
# ifdef HAVE_STATX
   struct statx  stat_buf;
# else
   struct stat   stat_buf;
# endif

   p_queue = fullname + sprintf(fullname, "%s%s%s/",
                                p_work_dir, AFD_FILE_DIR, AFD_TIME_DIR);

   if ((dp = opendir(fullname)) != NULL)
   {
      while (((p_dir = readdir(dp)) != NULL) && (limit_reached == NO))
      {
         if (p_dir->d_name[0] != '.')
         {
            p_file = p_queue + sprintf(p_queue, "%s", p_dir->d_name);
# ifdef HAVE_STATX
            if ((statx(0, fullname,
                       AT_STATX_SYNC_AS_STAT | AT_SYMLINK_NOFOLLOW,
                       STATX_MODE, &stat_buf) != -1) &&
                (S_ISLNK(stat_buf.stx_mode)))
# else
            if ((lstat(fullname, &stat_buf) != -1) &&
                (S_ISLNK(stat_buf.st_mode)))
# endif
            {
               get_time_jobs(fullname, p_file, p_dir->d_name);
            }
         }
      }
      if (closedir(dp) == -1)
      {
         (void)xrec(INFO_DIALOG, "Failed to closedir() `%s' : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
      }
   }

   return;
}
#endif /* MULTI_FS_SUPPORT */


/*++++++++++++++++++++++++++++ get_time_jobs() ++++++++++++++++++++++++++*/
static void
#ifdef MULTI_FS_SUPPORT
get_time_jobs(char *fullname, char *p_queue, char *sub_dir)
#else
get_time_jobs(void)
#endif
{
#ifdef MULTI_FS_SUPPORT
   char          *p_file,
                 where[5 + MAX_FILENAME_LENGTH + 1];
#else
   char          fullname[MAX_PATH_LENGTH],
                 *p_file,
                 *p_queue;
#endif
   DIR           *dp;
   struct dirent *p_dir;
#ifdef HAVE_STATX
   struct statx  stat_buf;
#else
   struct stat   stat_buf;
#endif

#ifdef MULTI_FS_SUPPORT
   *p_queue = '/';
   p_queue++;
   *p_queue = '\0';
   where[0] = 'T';
   where[1] = 'i';
   where[2] = 'm';
   where[3] = 'e';
   where[4] = ' ';
   (void)strcpy(&where[5], sub_dir);
   searching(where);
#else
   searching("Time");
   p_queue = fullname + sprintf(fullname, "%s%s%s/",
                                p_work_dir, AFD_FILE_DIR, AFD_TIME_DIR);
#endif

   if ((dp = opendir(fullname)) != NULL)
   {
      int    gotcha,
             pos;
      size_t dir_name_length;

      while (((p_dir = readdir(dp)) != NULL) && (limit_reached == NO))
      {
         if (p_dir->d_name[0] != '.')
         {
            p_file = p_queue + sprintf(p_queue, "%s/", p_dir->d_name);
#ifdef HAVE_STATX
            if ((statx(0, fullname, AT_STATX_SYNC_AS_STAT,
                       STATX_MODE, &stat_buf) != -1) &&
                (S_ISDIR(stat_buf.stx_mode)))
#else
            if ((stat(fullname, &stat_buf) != -1) &&
                (S_ISDIR(stat_buf.st_mode)))
#endif
            {
               unsigned int job_id;

               job_id = (unsigned int)strtoul(p_dir->d_name, NULL, 16);
               if ((pos = get_pos(job_id)) != -1)
               {
                  if (no_of_search_hosts == 0)
                  {
                     gotcha = YES;
                  }
                  else
                  {
                     register int j;
                     int          ret;

                     gotcha = NO;
                     for (j = 0; j < no_of_search_hosts; j++)
                     {
                        if ((ret = pmatch(search_recipient[j],
                                          jd[pos].host_alias, NULL)) == 0)
                        {
                           gotcha = YES;
                           break;
                        }
                        else if (ret == 1)
                             {
                                /* This host is NOT wanted. */
                                break;
                             }
                     }
                  }
               }
               else
               {
                  gotcha = NO;
               }

               if (gotcha == YES)
               {
                  if ((no_of_search_dirs > 0) ||
                      (no_of_search_dirids > 0))
                  {
                     int kk;

                     /* Check if an input directory was specified. */
                     gotcha = NO;
                     for (kk = 0; kk < no_of_search_dirids; kk++)
                     {
                        if (search_dirid[kk] == dnb[jd[pos].dir_id_pos].dir_id)
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
                              if (sfilter(search_dir[kk],
                                          dnb[jd[pos].dir_id_pos].dir_name, 0) == 0)
                              {
                                 gotcha = YES;
                                 break;
                              }
                           }
                           else
                           {
                              dir_name_length = strlen(dnb[jd[pos].dir_id_pos].dir_name);
                              if (search_dir_length[kk] == dir_name_length)
                              {
                                 if (strncmp(search_dir[kk],
                                             dnb[jd[pos].dir_id_pos].dir_name,
                                             dir_name_length) == 0)
                                 {
                                    gotcha = YES;
                                    break;
                                 }
                              }
                           }
                        }
                     }
                  }
                  if (gotcha == YES)
                  {
                     int fra_pos;

                     fra_pos = lookup_fra_pos(jd[pos].dir_id);
#ifdef MULTI_FS_SUPPORT
                     insert_file(fullname, p_file, sub_dir, jd[pos].host_alias,
#else
                     insert_file(fullname, p_file, "\0", jd[pos].host_alias,
#endif
                                 SHOW_TIME_JOBS, jd[pos].priority, job_id,
                                 jd[pos].dir_id_pos, jd[pos].dir_id, 0,
                                 fra_pos);
                  }
               }
            }
         }
      }
      if (closedir(dp) == -1)
      {
         (void)xrec(INFO_DIALOG, "Failed to closedir() `%s' : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
      }
   }
#ifdef MULTI_FS_SUPPORT
   searching(where);
#else
   searching("Time");
#endif

   return;
}

#ifdef _WITH_HEAPSORT
/*++++++++++++++++++++++++++++++ sort_data() ++++++++++++++++++++++++++++*/
/*                               -----------                             */
/* Description: Heapsort found in linux kernel mailing list from         */
/*              Jamie Lokier.                                            */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void
sort_data(int count)
{
   int                     i, j, k;
   struct queued_file_list tmp_qfl;

   for (i = 1; i < count; i++)
   {
      j = i;
      (void)memcpy(&tmp_qfl, &qfl[j], sizeof(struct queued_file_list));
      while ((j > 0) && (tmp_qfl.mtime > qfl[(j - 1) / 2].mtime))
      {
         (void)memcpy(&qfl[j], &qfl[(j - 1) / 2], sizeof(struct queued_file_list));
         j = (j - 1) / 2;
      }
      (void)memcpy(&qfl[j], &tmp_qfl, sizeof(struct queued_file_list));
   }
   for (i = (count - 1); i > 0; i--)
   {
      j = 0;
      k = 1;
      (void)memcpy(&tmp_qfl, &qfl[i], sizeof(struct queued_file_list));
      (void)memcpy(&qfl[i], &qfl[0], sizeof(struct queued_file_list));
      while ((k < i) &&
             ((tmp_qfl.mtime < qfl[k].mtime) ||
              (((k + 1) < i) && (tmp_qfl.mtime < qfl[k + 1].mtime))))
      {
         k += (((k + 1) < i) && (qfl[k + 1].mtime > qfl[k].mtime));
         (void)memcpy(&qfl[j], &qfl[k], sizeof(struct queued_file_list));
         j = k;
         k = 2 * j + 1;
      }
      (void)memcpy(&qfl[j], &tmp_qfl, sizeof(struct queued_file_list));
   }

   return;
}
#else

/*+++++++++++++++++++++++++++++ sort_data() +++++++++++++++++++++++++++++*/
static void
sort_data(int start, int end)
{
   int                     i, j;
   time_t                  center;
   struct queued_file_list tmp_qfl;

   i = start; j = end;
   center = qfl[(start + end)/2].mtime;
   do
   {
      while ((i < end) && (qfl[i].mtime< center))
      {
         i++;
      }
      while ((j > start) && (qfl[j].mtime > center))
      {
         j--;
      }
      if (i <= j)
      {
         (void)memcpy(&tmp_qfl, &qfl[i], sizeof(struct queued_file_list));
         (void)memcpy(&qfl[i], &qfl[j], sizeof(struct queued_file_list));
         (void)memcpy(&qfl[j], &tmp_qfl, sizeof(struct queued_file_list));
         i++; j--;
      }
   } while (i <= j);
   if (start < j)
   {
      sort_data(start, j);
   }
   if (i < end)
   {
      sort_data(i, end);
   }

   return;
}
#endif


#ifdef _SORT_NOT_WORKING
/*+++++++++++++++++++++++++++++ sort_data() +++++++++++++++++++++++++++++*/
static void
sort_data(void)
{
   int                     i, j,
                           start = 0,
                           end = total_no_files - 1;
   time_t                  center;
   struct queued_file_list tmp_qfl;

   i = start; j = end;
   for (;;)
   {
      center = qfl[(start + end)/2].mtime;
      do
      {
         while ((i < end) && (qfl[i].mtime < center))
         {
            i++;
         }
         while ((j > start) && (qfl[j].mtime > center))
         {
            j--;
         }
         if (i <= j)
         {
            (void)memcpy(&tmp_qfl, &qfl[i], sizeof(struct queued_file_list));
            (void)memcpy(&qfl[i], &qfl[j], sizeof(struct queued_file_list));
            (void)memcpy(&qfl[j], &tmp_qfl, sizeof(struct queued_file_list));
            i++; j--;
         }
      } while (i <= j);

      if (start < j)
      {
         i = start;
         end = j;
      }
      else if (i < end)
           {
              start = i;
              j = end;
           }
           else
           {
              break;
           }
   }

   return;
}
#endif /* _SORT_NOT_WORKING */


/*---------------------------- insert_file() ----------------------------*/
static void
insert_file(char         *queue_dir,
            char         *ptr_file,
            char         *msg_name,
            char         *hostname,
            char         queue_type,
            char         priority,
            unsigned int job_id,
            int          dir_id_pos,
            unsigned int dir_id,
            unsigned int files_to_send,
            int          fra_pos)
{
   DIR          *dpfile;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   if ((dpfile = opendir(queue_dir)) != NULL)
   {
      int           i;
      struct dirent *dirp;

      while (((dirp = readdir(dpfile)) != NULL) && (limit_reached == NO))
      {
         if (dirp->d_name[0] != '.')
         {
            /* Check if we need to search for a specific file. */
            if (check_all_file_names(dirp->d_name) != -1)
            {
               (void)strcpy(ptr_file, dirp->d_name);
#ifdef HAVE_STATX
               if ((statx(0, queue_dir, AT_STATX_SYNC_AS_STAT,
                          STATX_MODE | STATX_MTIME | STATX_SIZE,
                          &stat_buf) != -1) && (!S_ISDIR(stat_buf.stx_mode)))
#else
               if ((stat(queue_dir, &stat_buf) != -1) &&
                   (!S_ISDIR(stat_buf.st_mode)))
#endif
               {
                  /* If necessary check if its in the time span. */
#ifdef HAVE_STATX
                  if (((start_time_val == -1) ||
                       (stat_buf.stx_mtime.tv_sec >= start_time_val)) &&
                      ((end_time_val == -1) ||
                       (stat_buf.stx_mtime.tv_sec <= end_time_val)))
#else
                  if (((start_time_val == -1) ||
                       (stat_buf.st_mtime >= start_time_val)) &&
                      ((end_time_val == -1) ||
                       (stat_buf.st_mtime <= end_time_val)))
#endif
                  {
                     /* If necessary check the file size. */
#ifdef HAVE_STATX
                     if ((search_file_size == -1) ||
                         ((search_file_size != -1) &&
                          (((gt_lt_sign == GREATER_THEN_SIGN) &&
                            (stat_buf.stx_size > search_file_size)) ||
                           ((gt_lt_sign == LESS_THEN_SIGN) &&
                            (stat_buf.stx_size < search_file_size)) ||
                           ((gt_lt_sign == NOT_SIGN) &&
                            (stat_buf.stx_size != search_file_size)) ||
                           ((gt_lt_sign == EQUAL_SIGN) &&
                            (stat_buf.stx_size == search_file_size)))))
#else
                     if ((search_file_size == -1) ||
                         ((search_file_size != -1) &&
                          (((gt_lt_sign == GREATER_THEN_SIGN) &&
                            (stat_buf.st_size > search_file_size)) ||
                           ((gt_lt_sign == LESS_THEN_SIGN) &&
                            (stat_buf.st_size < search_file_size)) ||
                           ((gt_lt_sign == NOT_SIGN) &&
                            (stat_buf.st_size != search_file_size)) ||
                           ((gt_lt_sign == EQUAL_SIGN) &&
                            (stat_buf.st_size == search_file_size)))))
#endif
                     {
                        /* Finally we got a file. */
                        if ((total_no_files % 50) == 0)
                        {
                           size_t new_size = (((total_no_files / 50) + 1) * 50) *
                                             sizeof(struct queued_file_list);

                           if (total_no_files == 0)
                           {
                              if ((qfl = malloc(new_size)) == NULL)
                              {
                                 (void)xrec(FATAL_DIALOG,
                                            "malloc() error : %s (%s %d)",
                                            strerror(errno), __FILE__, __LINE__);
                                 return;
                              }
                           }
                           else
                           {
                              struct queued_file_list *tmp_qfl;

                              if ((tmp_qfl = realloc((char *)qfl, new_size)) == NULL)
                              {
                                 int tmp_errno = errno;

                                 free(qfl);
                                 (void)xrec(FATAL_DIALOG,
                                            "realloc() error : %s (%s %d)",
                                            strerror(tmp_errno),
                                            __FILE__, __LINE__);
                                 return;
                              }
                              qfl = tmp_qfl;
                           }
                        }
                        if ((qfl[total_no_files].file_name = malloc(strlen(dirp->d_name) + 1)) == NULL)
                        {
                           (void)xrec(FATAL_DIALOG,
                                      "malloc() error : %s (%s %d)",
                                      strerror(errno), __FILE__, __LINE__);
                           return;
                        }
                        qfl[total_no_files].msg_number = 0.0;
                        qfl[total_no_files].pos = -1;
                        qfl[total_no_files].job_id = job_id;
                        qfl[total_no_files].dir_id = dir_id;
#ifdef HAVE_STATX
                        qfl[total_no_files].size = stat_buf.stx_size;
                        qfl[total_no_files].mtime = stat_buf.stx_mtime.tv_sec;
#else
                        qfl[total_no_files].size = stat_buf.st_size;
                        qfl[total_no_files].mtime = stat_buf.st_mtime;
#endif
                        qfl[total_no_files].dir_id_pos = dir_id_pos;
                        qfl[total_no_files].queue_type = queue_type;
                        qfl[total_no_files].priority = priority;
                        (void)strcpy(qfl[total_no_files].hostname, hostname);
                        if (fra_pos < 0)
                        {
                           qfl[total_no_files].dir_alias[0] = '\0';
                        }
                        else
                        {
                           (void)strcpy(qfl[total_no_files].dir_alias,
                                        fra[fra_pos].dir_alias);
                        }
                        (void)strcpy((char *)qfl[total_no_files].file_name,
                                     dirp->d_name);
                        (void)strcpy(qfl[total_no_files].msg_name, msg_name);
                        if (files_to_send > 0)
                        {
                           if ((queue_tmp_buf_entries % 100) == 0)
                           {
                              size_t new_size = (((queue_tmp_buf_entries / 100) + 1) *
                                                 100) * sizeof(struct queue_tmp_buf);

                              if (queue_tmp_buf_entries == 0)
                              {
                                 if ((qtb = malloc(new_size)) == NULL)
                                 {
                                    (void)xrec(FATAL_DIALOG,
                                               "malloc() error : %s (%s %d)",
                                               strerror(errno),
                                               __FILE__, __LINE__);
                                    return;
                                 }
                              }
                              else
                              {
                                 struct queue_tmp_buf *tmp_qtb;

                                 if ((tmp_qtb = realloc((char *)qtb, new_size)) == NULL)
                                 {
                                    int tmp_errno = errno;

                                    free(qtb);
                                    (void)xrec(FATAL_DIALOG,
                                               "realloc() error : %s (%s %d)",
                                               strerror(tmp_errno),
                                               __FILE__, __LINE__);
                                    return;
                                 }
                                 qtb = tmp_qtb;
                              }
                           }
                           for (i = 0; i < queue_tmp_buf_entries; i++)
                           {
                              if (CHECK_STRCMP(qtb[i].msg_name, msg_name) == 0)
                              {
                                 break;
                              }
                           }
                           qfl[total_no_files].queue_tmp_buf_pos = i;
                           if (i == queue_tmp_buf_entries)
                           {
                              (void)strcpy(qtb[i].msg_name, msg_name);
                              qtb[i].files_to_send = files_to_send;
                              qtb[i].qfl_pos = NULL;
                              qtb[i].files_to_delete = 0;
                              queue_tmp_buf_entries++;
                           }
                        }
                        else
                        {
                           qfl[total_no_files].queue_tmp_buf_pos = -1;
                        }
                        total_no_files++;
#ifdef HAVE_STATX
                        total_file_size += stat_buf.stx_size;
#else
                        total_file_size += stat_buf.st_size;
#endif

                        if ((perm.list_limit > 0) &&
                            (total_no_files > perm.list_limit))
                        {
                           char msg_buffer[40];

                           (void)sprintf(msg_buffer, "List limit (%d) reached!",
                                         perm.list_limit);
                           show_message(statusbox_w, msg_buffer);
                           limit_reached = YES;
                        }
                     }
                  }
               }
            }
         }
      }
      if (closedir(dpfile) == -1)
      {
         *ptr_file = '\0';
         (void)xrec(INFO_DIALOG, "Failed to closedir() `%s' : %s (%s %d)",
                    queue_dir, strerror(errno), __FILE__, __LINE__);
      }
   }
   return;
}


/*------------------------ check_all_file_names() -----------------------*/
static int
check_all_file_names(char *name)
{
   int ret;

   if (no_of_search_file_names == 0)
   {
      ret = 0;
   }
   else
   {
      int i,
          val;

      ret = -1;
      for (i = 0; i < no_of_search_file_names; i++)
      {
         if ((val = pmatch(search_file_name[i], name, NULL)) == 0)
         {
            if (search_file_name[i][0] != '!')
            {
               ret = i;
               break;
            }
         }
         else if (val == 1)
                 {
                    break;
                 }
      }
   }

   return(ret);
}


/*------------------------------- get_job_id() --------------------------*/
static int
get_job_id(char *msg_name, unsigned int *job_id)
{
   int  ret;
   char *ptr = msg_name;
#ifdef MULTI_FS_SUPPORT
   char *p_job_id;
#endif

   *job_id = 0;
#ifdef MULTI_FS_SUPPORT
   while ((*ptr != '/') && (*ptr != '\0'))
   {
      ptr++;
   }
   if (*ptr == '/')
   {
      ptr++;
      p_job_id = ptr;
#endif
      while ((*ptr != '/') && (*ptr != '\0'))
      {
         ptr++;
      }
      if (*ptr == '/')
      {
         errno = 0;
#ifdef MULTI_FS_SUPPORT
         *job_id = (unsigned int)strtoul(p_job_id, (char **)NULL, 16);
#else
         *job_id = (unsigned int)strtoul(msg_name, (char **)NULL, 16);
#endif
         if (errno == ERANGE)
         {
            ret = INCORRECT;
         }
         else
         {
            ret = SUCCESS;
         }
      }
      else
      {
         ret = INCORRECT;
      }
#ifdef MULTI_FS_SUPPORT
   }
   else
   {
      ret = INCORRECT;
   }
#endif
   return(ret);
}


/*--------------------------------- get_pos() ---------------------------*/
static int
get_pos(unsigned int job_id)
{
   register int i;

   for (i = 0; i < no_of_jobs; i++)
   {
      if (job_id == jd[i].job_id)
      {
         return(i);
      }
   }
   return(-1);
}


/*------------------------------- searching() ---------------------------*/
static void
searching(char *where)
{
   static int rotate = 0;
   char       status_message[MAX_MESSAGE_LENGTH];

   if (rotate == 0)
   {
      (void)sprintf(status_message, "Searching %s -", where);
   }
   else if (rotate == 1)
        {
           (void)sprintf(status_message, "Searching %s \\", where);
        }
   else if (rotate == 2)
        {
           (void)sprintf(status_message, "Searching %s |", where);
        }
        else
        {
           (void)sprintf(status_message, "Searching %s /", where);
           rotate = -1;
        }
   rotate++;
   SHOW_MESSAGE();

   return;
}


/*---------------------------- lookup_fra_pos() -------------------------*/
static int
lookup_fra_pos(unsigned int dir_id)
{
   register int i;

   for (i = 0; i < no_of_dirs; i++)
   {
      if (fra[i].dir_id == dir_id)
      {
         return(i);
      }
   }

   return(INCORRECT);
}
