/*
 *  get_remote_file_names_http.c - Part of AFD, an automatic file distribution
 *                                 program.
 *  Copyright (c) 2006 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_remote_file_names_http - retrieves filename, size and date
 **
 ** SYNOPSIS
 **   int get_remote_file_names_http(off_t *file_size_to_retrieve,
 **                                  int   *more_files_in_list)
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
 **   09.08.2006 H.Kiehl Created
 **   02.12.2009 H.Kiehl Added support for NOAA type HTML file listing.
 **   15.03.2011 H.Kiehl Added HTML list type listing.
 **   03.09.2017 H.Kiehl Added option to get only appended part.
 **   10.09.2017 H.Kiehl Added support for nginx HTML listing.
 **   29.07.2019 H.Kiehl Added check if ls_data file is changed while we
 **                      work with it.
 **   13.08.2021 H.Kiehl Introduce parameter exact_date. Note, it
 **                      can be that this is not usefull since we
 **                      assume http_head() will always return the
 **                      seconds. Lets, see how this works and if
 **                      it increases the HEAD calls, remove it.
 **
 */
DESCR__E_M3

/* #define DUMP_DIR_LIST_TO_DISK */

#include <stdio.h>
#include <string.h>                /* strerror(), memmove()              */
#include <stdlib.h>                /* malloc(), realloc(), free()        */
#include <time.h>                  /* time(), mktime()                   */ 
#include <ctype.h>                 /* isdigit()                          */
#include <sys/time.h>              /* struct tm                          */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef DUMP_DIR_LIST_TO_DISK
# include <fcntl.h>
#endif
#include <unistd.h>                /* unlink()                           */
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include "httpdefs.h"
#include "fddefs.h"


#define STORE_HTML_STRING(html_str, str_len, max_str_length, end_char)\
        {                                                          \
           str_len = 0;                                            \
           while ((str_len < ((max_str_length) - 1)) && (*ptr != end_char) &&\
                  (*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))\
           {                                                       \
              if (*ptr == '&')                                     \
              {                                                    \
                 ptr++;                                            \
                 if ((*(ptr + 1) == 'u') && (*(ptr + 2) == 'm') && \
                     (*(ptr + 3) == 'l') && (*(ptr + 4) == ';'))   \
                 {                                                 \
                    switch (*ptr)                                  \
                    {                                              \
                       case 'a': (html_str)[str_len++] = 228;      \
                                 break;                            \
                       case 'A': (html_str)[str_len++] = 196;      \
                                 break;                            \
                       case 'e': (html_str)[str_len++] = 235;      \
                                 break;                            \
                       case 'E': (html_str)[str_len++] = 203;      \
                                 break;                            \
                       case 'i': (html_str)[str_len++] = 239;      \
                                 break;                            \
                       case 'I': (html_str)[str_len++] = 207;      \
                                 break;                            \
                       case 'o': (html_str)[str_len++] = 246;      \
                                 break;                            \
                       case 'O': (html_str)[str_len++] = 214;      \
                                 break;                            \
                       case 'u': (html_str)[str_len++] = 252;      \
                                 break;                            \
                       case 'U': (html_str)[str_len++] = 220;      \
                                 break;                            \
                       case 's': (html_str)[str_len++] = 223;      \
                                 break;                            \
                       case 'y': (html_str)[str_len++] = 255;      \
                                 break;                            \
                       case 'Y': (html_str)[str_len++] = 195;      \
                                 break;                            \
                       default : /* Just ignore it. */             \
                                 break;                            \
                    }                                              \
                    ptr += 5;                                      \
                    continue;                                      \
                 }                                                 \
                 else if ((*ptr == 's') && (*(ptr + 1) == 'z') &&  \
                          (*(ptr + 2) == 'l') && (*(ptr + 3) == 'i') &&\
                          (*(ptr + 4) == 'g') && (*(ptr + 5) == ';'))\
                      {                                            \
                         (html_str)[str_len++] = 223;              \
                         ptr += 6;                                 \
                         continue;                                 \
                      }                                            \
                 else if ((*ptr == 'a') && (*(ptr + 1) == 'm') &&  \
                          (*(ptr + 2) == 'p') && (*(ptr + 3) == ';'))\
                      {                                            \
                         (html_str)[str_len++] = 38;               \
                         ptr += 4;                                 \
                         continue;                                 \
                      }                                            \
                 else if ((*ptr == 'd') && (*(ptr + 1) == 'e') &&  \
                          (*(ptr + 2) == 'g') && (*(ptr + 3) == ';'))\
                      {                                            \
                         (html_str)[str_len++] = 176;              \
                         ptr += 4;                                 \
                         continue;                                 \
                      }                                            \
                 else if ((*ptr == 'g') && (*(ptr + 1) == 't') &&  \
                          (*(ptr + 2) == ';'))                     \
                      {                                            \
                         (html_str)[str_len++] = '>';              \
                         ptr += 3;                                 \
                         continue;                                 \
                      }                                            \
                 else if ((*ptr == 'l') && (*(ptr + 1) == 't') &&  \
                          (*(ptr + 2) == ';'))                     \
                      {                                            \
                         (html_str)[str_len++] = '<';              \
                         ptr += 3;                                 \
                         continue;                                 \
                      }                                            \
                      else                                         \
                      {                                            \
                         while ((*ptr != ';') && (*ptr != '<') &&  \
                                (*ptr != '\n') && (*ptr != '\r') &&\
                                (*ptr != '\0'))                    \
                         {                                         \
                            ptr++;                                 \
                         }                                         \
                         if (*ptr != ';')                          \
                         {                                         \
                            break;                                 \
                         }                                         \
                      }                                            \
              }                                                    \
              (html_str)[str_len] = *ptr;                          \
              str_len++; ptr++;                                    \
           }                                                       \
           (html_str)[str_len] = '\0';                             \
        }
#define STORE_HTML_DATE()                                          \
        {                                                          \
           int i = 0,                                              \
               space_counter = 0;                                  \
                                                                   \
           while ((i < (MAX_FILENAME_LENGTH - 1)) && (*ptr != '<') &&\
                  (*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))\
           {                                                       \
              if (*ptr == ' ')                                     \
              {                                                    \
                 if (space_counter == 1)                           \
                 {                                                 \
                    while (*ptr == ' ')                            \
                    {                                              \
                       ptr++;                                      \
                    }                                              \
                    break;                                         \
                 }                                                 \
                 space_counter++;                                  \
              }                                                    \
              if (*ptr == '&')                                     \
              {                                                    \
                 ptr++;                                            \
                 if ((*(ptr + 1) == 'u') && (*(ptr + 2) == 'm') && \
                     (*(ptr + 3) == 'l') && (*(ptr + 4) == ';'))   \
                 {                                                 \
                    switch (*ptr)                                  \
                    {                                              \
                       case 'a': date_str[i++] = 228;              \
                                 break;                            \
                       case 'A': date_str[i++] = 196;              \
                                 break;                            \
                       case 'o': date_str[i++] = 246;              \
                                 break;                            \
                       case 'O': date_str[i++] = 214;              \
                                 break;                            \
                       case 'u': date_str[i++] = 252;              \
                                 break;                            \
                       case 'U': date_str[i++] = 220;              \
                                 break;                            \
                       case 's': date_str[i++] = 223;              \
                                 break;                            \
                       default : /* Just ignore it. */             \
                                 break;                            \
                    }                                              \
                    ptr += 5;                                      \
                    continue;                                      \
                 }                                                 \
                 else                                              \
                 {                                                 \
                    while ((*ptr != ';') && (*ptr != '<') &&       \
                           (*ptr != '\n') && (*ptr != '\r') &&     \
                           (*ptr != '\0'))                         \
                    {                                              \
                       ptr++;                                      \
                    }                                              \
                    if (*ptr != ';')                               \
                    {                                              \
                       break;                                      \
                    }                                              \
                 }                                                 \
              }                                                    \
              date_str[i] = *ptr;                                  \
              i++; ptr++;                                          \
           }                                                       \
           date_str[i] = '\0';                                     \
        }

/* External global variables. */
extern int                        *current_no_of_listed_files,
                                  exitflag,
                                  no_of_listed_files,
                                  rl_fd,
                                  timeout_flag;
extern char                       msg_str[],
                                  *p_work_dir;
extern off_t                      rl_size;
extern struct job                 db;
extern struct retrieve_list       *rl;
extern struct fileretrieve_status *fra;
extern struct filetransfer_status *fsa;

/* Local global variables. */
static int                        cached_i,
                                  nfg;           /* Number of file mask. */
static time_t                     current_time;
static struct file_mask           *fml = NULL;

/* Local function prototypes. */
static int                        check_list(char *, int, time_t, int, off_t,
                                             off_t, int *, off_t *, int *),
                                  check_name(char *, int, off_t, time_t,
                                             unsigned int *, off_t *),
                                  eval_html_dir_list(char *, off_t, int *,
                                                     off_t *, int *,
                                                     unsigned int *, off_t *,
                                                     unsigned int *, off_t *,
                                                     int *);
static off_t                      convert_size(char *, off_t *);
#ifdef WITH_ATOM_FEED_SUPPORT
static time_t                     extract_feed_date(char *);
#endif


/*#################### get_remote_file_names_http() #####################*/
int
get_remote_file_names_http(off_t *file_size_to_retrieve,
                           int   *more_files_in_list)
{
   int files_to_retrieve = 0,
       i = 0;

   *file_size_to_retrieve = 0;
#ifdef NEW_FRA
   if (fra->dir_options & URL_CREATES_FILE_NAME)
#else
   if (fra->dir_flag & URL_CREATES_FILE_NAME)
#endif
   {
      /* Add this file to the list. */
      if (rl_fd == -1)
      {
         if (attach_ls_data(fra, db.special_flag, YES) == INCORRECT)
         {
            http_quit();
            exit(INCORRECT);
         }
      }
      if ((no_of_listed_files != 0) &&
          ((no_of_listed_files % RETRIEVE_LIST_STEP_SIZE) == 0))
      {
         char   *ptr;
         size_t new_size = (((no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                            RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                            AFD_WORD_OFFSET;

         ptr = (char *)rl - AFD_WORD_OFFSET;
#ifdef DO_NOT_PARALLELIZE_ALL_FETCH
         if ((fra->stupid_mode == YES) || (fra->remove == YES))
         {
            if ((ptr = realloc(ptr, new_size)) == NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "realloc() error : %s", strerror(errno));
               http_quit();
               exit(INCORRECT);
            }
         }
         else
         {
#endif
            if ((ptr = mmap_resize(rl_fd, ptr, new_size)) == (caddr_t) -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "mmap_resize() error : %s", strerror(errno));
               http_quit();
               exit(INCORRECT);
            }
            rl_size = new_size;
#ifdef DO_NOT_PARALLELIZE_ALL_FETCH
         }
#endif
         if (no_of_listed_files < 0)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Hmmm, no_of_listed_files = %d", no_of_listed_files);
               no_of_listed_files = 0;
         }
         *(int *)ptr = no_of_listed_files;
         current_no_of_listed_files = (int *)ptr;
         ptr += AFD_WORD_OFFSET;
         rl = (struct retrieve_list *)ptr;
      }

      rl[0].file_name[0] = '\0';
#ifdef _WITH_EXTRA_CHECK
      rl[0].extra_data[0] = '\0';
#endif
      rl[0].retrieved = NO;
      rl[0].assigned = (unsigned char)db.job_no + 1;
      rl[0].in_list = YES;
      rl[0].special_flag = 0;
      rl[0].file_mtime = -1;
      rl[0].got_date = NO;
      rl[0].size = -1;
      rl[0].prev_size = 0;
      no_of_listed_files = 1;
      *current_no_of_listed_files = 1;
      *more_files_in_list = NO;

      return(1);
   }
#ifdef DO_NOT_PARALLELIZE_ALL_FETCH
   if ((*more_files_in_list == YES) ||
       (db.special_flag & DISTRIBUTED_HELPER_JOB) ||
       ((db.special_flag & OLD_ERROR_JOB) && (db.retries < 30) &&
        (fra->stupid_mode != YES) && (fra->remove != YES)))
#else
   if (rl_fd == -1)
   {
try_attach_again:
      if (attach_ls_data(fra, db.special_flag, YES) == INCORRECT)
      {
         http_quit();
         exit(INCORRECT);
      }
      if ((db.special_flag & DISTRIBUTED_HELPER_JOB) &&
          ((fra->stupid_mode == YES) || (fra->remove == YES)))
      {
# ifdef LOCK_DEBUG
         if (rlock_region(rl_fd, LOCK_RETR_PROC,
                          __FILE__, __LINE__) == LOCK_IS_SET)
# else
         if (rlock_region(rl_fd, LOCK_RETR_PROC) == LOCK_IS_SET)
# endif
         {
            if (i == 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Hmm, lock is set. Assume ls_data file was just modified. Lets try it again. (job_no=%d fsa_pos=%d)",
                          (int)db.job_no, db.fsa_pos);
            }
            else
            {
               if (i == 30)
               {
                  trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Have waited %d seconds, but unable to get a lock. Terminating.",
                            (i * 100000) / 1000000);
                  http_quit();
                  exit(SUCCESS);
               }
               my_usleep(100000L);
            }
            detach_ls_data(NO);
            i++;
            goto try_attach_again;
         }
      }
   }

   if ((*more_files_in_list == YES) ||
       (db.special_flag & DISTRIBUTED_HELPER_JOB) ||
       ((db.special_flag & OLD_ERROR_JOB) && (db.retries < 30)))
#endif
   {
#ifdef DO_NOT_PARALLELIZE_ALL_FETCH
      if (rl_fd == -1)
      {
         if (attach_ls_data(fra, db.special_flag, YES) == INCORRECT)
         {
            http_quit();
            exit(INCORRECT);
         }
      }
#endif
      *more_files_in_list = NO;
      for (i = 0; i < no_of_listed_files; i++)
      {
         if (*current_no_of_listed_files != no_of_listed_files)
         {
            if (i >= *current_no_of_listed_files)
            {
               trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "no_of_listed_files has been reduced (%d -> %d)!",
                         no_of_listed_files, *current_no_of_listed_files);
               no_of_listed_files = *current_no_of_listed_files;
               break;
            }
         }
         if ((rl[i].retrieved == NO) && (rl[i].assigned == 0))
         {
#ifdef DO_NOT_PARALLELIZE_ALL_FETCH
            if ((fra->stupid_mode == YES) || (fra->remove == YES) ||
                ((files_to_retrieve < fra->max_copied_files) &&
                 (*file_size_to_retrieve < fra->max_copied_file_size)))
#else
            if ((files_to_retrieve < fra->max_copied_files) &&
                (*file_size_to_retrieve < fra->max_copied_file_size))
#endif
            {
               /* Lock this file in list. */
#ifdef LOCK_DEBUG
               if (lock_region(rl_fd, (off_t)(LOCK_RETR_FILE + i),
                               __FILE__, __LINE__) == LOCK_IS_NOT_SET)
#else
               if (lock_region(rl_fd, (off_t)(LOCK_RETR_FILE + i)) == LOCK_IS_NOT_SET)
#endif
               {
                  if ((rl[i].file_mtime == -1) || (rl[i].size == -1))
                  {
                     int status;

                     if ((status = http_head(db.target_dir,
                                             rl[i].file_name, &rl[i].size,
                                             &rl[i].file_mtime)) == SUCCESS)
                     {
                        if (fsa->debug > NORMAL_MODE)
                        {
                           trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                        "Date for %s is %ld, size = %ld bytes.",
# else
                                        "Date for %s is %lld, size = %ld bytes.",
# endif
#else
# if SIZEOF_TIME_T == 4
                                        "Date for %s is %ld, size = %lld bytes.",
# else
                                        "Date for %s is %lld, size = %lld bytes.",
# endif
#endif
                                        rl[i].file_name,
                                        (pri_time_t)rl[i].file_mtime, 
                                        (pri_off_t)rl[i].size);
                        }
                     }
                     else
                     {
                        trans_log((timeout_flag == ON) ? ERROR_SIGN : DEBUG_SIGN,
                                  __FILE__, __LINE__, NULL,
                                  (status == INCORRECT) ? NULL : msg_str,
                                  "Failed to get date and size of data %s (%d).",
                                  rl[i].file_name, status);
                        if (timeout_flag != OFF)
                        {
                           http_quit();
                           exit(DATE_ERROR);
                        }
                     }
                  }
                  if (rl[i].file_mtime == -1)
                  {
                     rl[i].got_date = NO;
                  }
                  else
                  {
                     rl[i].got_date = YES;
                  }

                  if ((fra->ignore_size == -1) ||
                      ((fra->gt_lt_sign & ISIZE_EQUAL) &&
                       (fra->ignore_size == rl[i].size)) ||
                      ((fra->gt_lt_sign & ISIZE_LESS_THEN) &&
                       (fra->ignore_size < rl[i].size)) ||
                      ((fra->gt_lt_sign & ISIZE_GREATER_THEN) &&
                       (fra->ignore_size > rl[i].size)))
                  {
                     if ((rl[i].got_date == NO) ||
                         (fra->ignore_file_time == 0))
                     {
                        files_to_retrieve++;
                        if (rl[i].size > 0)
                        {
                           if ((fra->stupid_mode == APPEND_ONLY) &&
                               (rl[i].size > rl[i].prev_size))
                           {
                              *file_size_to_retrieve += (rl[i].size - rl[i].prev_size);
                           }
                           else
                           {
                              *file_size_to_retrieve += rl[i].size;
                           }
                        }
#ifdef NEW_FRA
                        if (((fra->dir_options & ONE_PROCESS_JUST_SCANNING) == 0) ||
#else
                        if (((fra->dir_flag & ONE_PROCESS_JUST_SCANNING) == 0) ||
#endif
                            (db.special_flag & DISTRIBUTED_HELPER_JOB))
                        {
                           rl[i].assigned = (unsigned char)db.job_no + 1;
                        }
                        else
                        {
                           *more_files_in_list = YES;
                        }
                     }
                     else
                     {
                        time_t diff_time;

                        diff_time = current_time - rl[i].file_mtime;
                        if (((fra->gt_lt_sign & IFTIME_EQUAL) &&
                             (fra->ignore_file_time == diff_time)) ||
                            ((fra->gt_lt_sign & IFTIME_LESS_THEN) &&
                             (fra->ignore_file_time < diff_time)) ||
                            ((fra->gt_lt_sign & IFTIME_GREATER_THEN) &&
                             (fra->ignore_file_time > diff_time)))
                        {
                           files_to_retrieve++;
                           if (rl[i].size > 0)
                           {
                              if ((fra->stupid_mode == APPEND_ONLY) &&
                                  (rl[i].size > rl[i].prev_size))
                              {
                                 *file_size_to_retrieve += (rl[i].size - rl[i].prev_size);
                              }
                              else
                              {
                                 *file_size_to_retrieve += rl[i].size;
                              }
                           }
#ifdef NEW_FRA
                           if (((fra->dir_options & ONE_PROCESS_JUST_SCANNING) == 0) ||
#else
                           if (((fra->dir_flag & ONE_PROCESS_JUST_SCANNING) == 0) ||
#endif
                               (db.special_flag & DISTRIBUTED_HELPER_JOB))
                           {
                              rl[i].assigned = (unsigned char)db.job_no + 1;
                           }
                           else
                           {
                              *more_files_in_list = YES;
                           }
                        }
                     }
#ifdef DEBUG_ASSIGNMENT
                     trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
# if SIZEOF_OFF_T == 4
                               "%s assigned %d: file_name=%s assigned=%d size=%ld",
# else
                               "%s assigned %d: file_name=%s assigned=%d size=%lld",
# endif
                               (fra->ls_data_alias[0] == '\0') ? fra->dir_alias : fra->ls_data_alias,
                               i, rl[i].file_name, (int)rl[i].assigned,
                               (pri_off_t)rl[i].size);
#endif /* DEBUG_ASSIGNMENT */
                  }
#ifdef LOCK_DEBUG
                  unlock_region(rl_fd, (off_t)(LOCK_RETR_FILE + i),
                                __FILE__, __LINE__);
#else
                  unlock_region(rl_fd, (off_t)(LOCK_RETR_FILE + i));
#endif
               }
            }
            else
            {
               *more_files_in_list = YES;
               break;
            }
         }
      }
   }
   else
   {
      int       j,
                status;
      time_t    now;
      struct tm *p_tm;

      /* Get all file masks for this directory. */
      if ((j = read_file_mask(fra->dir_alias, &nfg, &fml)) != SUCCESS)
      {
         if (j == LOCKFILE_NOT_THERE)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to set lock in file masks for %s, because the file is not there.",
                       fra->dir_alias);
         }
         else if (j == LOCK_IS_SET)
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to get the file masks for %s, because lock is already set.",
                            fra->dir_alias);
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to get the file masks for %s. (%d)",
                            fra->dir_alias, j);
              }
         free(fml);
         http_quit();
         exit(INCORRECT);
      }

#ifdef DO_NOT_PARALLELIZE_ALL_FETCH
      if ((fra->stupid_mode == YES) || (fra->remove == YES))
      {
         if (reset_ls_data() == INCORRECT)
         {
            http_quit();
            exit(INCORRECT);
         }
      }
      else
      {
         if (rl_fd == -1)
         {
            if (attach_ls_data(fra, db.special_flag, YES) == INCORRECT)
            {
               http_quit();
               exit(INCORRECT);
            }
         }
      }
#else
      if (rl_fd == -1)
      {
         if (attach_ls_data(fra, db.special_flag, YES) == INCORRECT)
         {
            http_quit();
            exit(INCORRECT);
         }
      }
      if ((fra->stupid_mode == YES) || (fra->remove == YES))
      {
         /*
          * If all files from the previous listing have been
          * collected, lets reset the ls_data structure or otherwise
          * it keeps on growing forever.
          */
# ifdef LOCK_DEBUG
         if (lock_region(rl_fd, LOCK_RETR_PROC,
                         __FILE__, __LINE__) == LOCK_IS_NOT_SET)
# else   
         if (lock_region(rl_fd, LOCK_RETR_PROC) == LOCK_IS_NOT_SET)
# endif
         {
            if (reset_ls_data() == INCORRECT)
            {
               http_quit();
               exit(INCORRECT);
            }
         }
# ifdef LOCK_DEBUG
         unlock_region(rl_fd, LOCK_RETR_PROC, __FILE__, __LINE__);
# else      
         unlock_region(rl_fd, LOCK_RETR_PROC);
# endif
      }
#endif

      if ((fra->ignore_file_time != 0) ||
          (fra->delete_files_flag & UNKNOWN_FILES) ||
          (fra->delete_files_flag & OLD_RLOCKED_FILES))
      {
         current_time = 0;
         p_tm = gmtime(&current_time);
         current_time = mktime(p_tm);
         if (current_time != 0)
         {
            /*
             * Note: Current system not GMT, assume server returns GMT
             *       so we need to convert this to GMT.
             */
            current_time = time(NULL);
            now = current_time;
            p_tm = gmtime(&current_time);
            current_time = mktime(p_tm);
         }
         else
         {
            current_time = time(NULL);
            now = current_time;
         }
      }
      else
      {
         now = 0;
      }

      /*
       * First determine if user wants to try and get a filename
       * listing. This can be done by setting the diretory option
       * 'do not get dir list' in DIR_CONFIG.
       */
#ifdef NEW_FRA
      if ((fra->dir_options & DONT_GET_DIR_LIST) == 0)
#else
      if ((fra->dir_flag & DONT_GET_DIR_LIST) == 0)
#endif
      {
         int          listing_complete = YES;
         unsigned int files_deleted = 0,
                      list_length = 0;
         off_t        bytes_buffered,
                      content_length,
                      file_size_deleted = 0,
                      list_size = 0;
#ifdef _WITH_EXTRA_CHECK
         char         etag[MAX_EXTRA_LS_DATA_LENGTH + 1];
#endif
         char         *listbuffer = NULL;

         do
         {
            bytes_buffered = 0;
            content_length = 0;
#ifdef _WITH_EXTRA_CHECK
            etag[0] = '\0';
#endif
            if (((status = http_get(db.target_dir, "", NULL,
#ifdef _WITH_EXTRA_CHECK
                                    etag,
#endif
                                    &content_length, 0)) != SUCCESS) &&
                (status != CHUNKED))
            {
#ifdef RESET_LS_DATA_ON_ERROR
               if (!((timeout_flag == ON) || (timeout_flag == CON_RESET) ||
                     (timeout_flag == CON_REFUSED)))
               {
                  if (reset_ls_data() != SUCCESS)
                  {
                     http_quit();
                     exit(INCORRECT);
                  }
               }
#endif
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         (status == INCORRECT) ? NULL : msg_str,
                         "Failed to open remote directory %s (%d).",
                         db.target_dir, status);
               http_quit();
               exit(eval_timeout(OPEN_REMOTE_ERROR));
            }
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                            "Opened HTTP connection for directory %s%s.",
                            db.target_dir,
                            (listing_complete == YES) ? "" : " (continue reading list)");
            }
            listing_complete = YES;

            if (status == SUCCESS)
            {
               int read_length;

               if (content_length > MAX_HTTP_DIR_BUFFER)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
#if SIZEOF_OFF_T == 4
                            "Directory buffer length is only for %d bytes, remote system wants to send %ld bytes. If needed increase MAX_HTTP_DIR_BUFFER.",
#else
                            "Directory buffer length is only for %d bytes, remote system wants to send %lld bytes. If needed increase MAX_HTTP_DIR_BUFFER.",
#endif
                            MAX_HTTP_DIR_BUFFER, (pri_off_t)content_length);
                  http_quit();
                  exit(ALLOC_ERROR);
               } else if (content_length == 0)
                      {
                         content_length = MAX_HTTP_DIR_BUFFER;
                      }

               if ((listbuffer = malloc(content_length + 1)) == NULL)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                             "Failed to malloc() %ld bytes : %s",
#else
                             "Failed to malloc() %lld bytes : %s",
#endif
                             (pri_off_t)(content_length + 1), strerror(errno));
                  http_quit();
                  exit(ALLOC_ERROR);
               }
               do
               {
                  if ((content_length - (bytes_buffered + fsa->block_size)) >= 0)
                  {
                     read_length = fsa->block_size;
                  }
                  else
                  {
                     read_length = content_length - bytes_buffered;
                  }
                  if ((status = http_read(&listbuffer[bytes_buffered],
                                          read_length)) == INCORRECT)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               (status == INCORRECT) ? NULL : msg_str,
                               "Failed to read from remote directory listing for %s (%d)",
                               db.target_dir, status);
                     free(listbuffer);
                     http_quit();
                     exit(eval_timeout(READ_REMOTE_ERROR));
                  }
                  else if (status > 0)
                       {
                          bytes_buffered += status;
                          if (bytes_buffered == content_length)
                          {
                             status = 0;
                          }
                          else if (bytes_buffered > content_length)
                               {
                                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
#if SIZEOF_OFF_T == 4
                                             "Maximum directory buffer length (%ld bytes) reached.",
#else
                                             "Maximum directory buffer length (%lld bytes) reached.",
#endif
                                             (pri_off_t)content_length);
                                  status = 0;
                               }
                       }
               } while (status != 0);
            }
            else /* status == CHUNKED */
            {
               int  chunksize;
               char *chunkbuffer = NULL;

               chunksize = fsa->block_size + 4;
               if ((chunkbuffer = malloc(chunksize)) == NULL)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to malloc() %d bytes : %s",
                             chunksize, strerror(errno));
                  http_quit();
                  exit(ALLOC_ERROR);
               }
               do
               {
                  if ((status = http_chunk_read(&chunkbuffer,
                                                &chunksize)) == INCORRECT)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               (status == INCORRECT) ? NULL : msg_str,
                               "Failed to read from remote directory listing for %s",
                               db.target_dir);
                     free(chunkbuffer);
                     http_quit();
                     exit(eval_timeout(READ_REMOTE_ERROR));
                  }
                  else if (status > 0)
                       {
                          if (listbuffer == NULL)
                          {
                             if ((listbuffer = malloc(status)) == NULL)
                             {
                                system_log(ERROR_SIGN, __FILE__, __LINE__,
                                           "Failed to malloc() %d bytes : %s",
                                           status, strerror(errno));
                                free(chunkbuffer);
                                http_quit();
                                exit(ALLOC_ERROR);
                             }
                          }
                          else
                          {
                             if (bytes_buffered > MAX_HTTP_DIR_BUFFER)
                             {
                                system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                                           "Directory length buffer is only for %d bytes, remote system wants to send %ld bytes. If needed increase MAX_HTTP_DIR_BUFFER.",
#else
                                           "Directory length buffer is only for %d bytes, remote system wants to send %lld bytes. If needed increase MAX_HTTP_DIR_BUFFER.",
#endif
                                           MAX_HTTP_DIR_BUFFER,
                                           (pri_off_t)content_length);
                                http_quit();
                                free(listbuffer);
                                free(chunkbuffer);
                                exit(ALLOC_ERROR);
                             }
                             if ((listbuffer = realloc(listbuffer,
                                                       bytes_buffered + status)) == NULL)
                             {
                                system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                                           "Failed to realloc() %ld bytes : %s",
#else
                                           "Failed to realloc() %lld bytes : %s",
#endif
                                           (pri_off_t)(bytes_buffered + status),
                                           strerror(errno));
                                free(chunkbuffer);
                                http_quit();
                                exit(ALLOC_ERROR);
                             }
                          }
                          (void)memcpy(&listbuffer[bytes_buffered], chunkbuffer,
                                       status);
                          bytes_buffered += status;
                       }
               } while (status != HTTP_LAST_CHUNK);

               if ((listbuffer = realloc(listbuffer, bytes_buffered + 1)) == NULL)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                             "Failed to realloc() %ld bytes : %s",
#else
                             "Failed to realloc() %lld bytes : %s",
#endif
                             (pri_off_t)(bytes_buffered + 1), strerror(errno));
                  free(chunkbuffer);
                  http_quit();
                  exit(ALLOC_ERROR);
               }

               free(chunkbuffer);
            }

            if (bytes_buffered > 0)
            {
#ifdef DUMP_DIR_LIST_TO_DISK
               int  fd;
               char dump_file_name[4 + 1 + 10 + MAX_REAL_HOSTNAME_LENGTH + 5 + 1];

               (void)snprintf(dump_file_name,
                              4 + 1 + 10 + MAX_REAL_HOSTNAME_LENGTH + 5 + 1,
                              "/tmp/http_list.%s.dump", fsa->host_dsp_name);
               if ((fd = open(dump_file_name, (O_WRONLY | O_CREAT | O_TRUNC),
                              FILE_MODE)) == -1)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Failed to open() `%s' : %s",
                             dump_file_name, strerror(errno));
               }
               else
               {
                  if (write(fd, listbuffer, bytes_buffered) != bytes_buffered)
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "Failed to write() to `%s' : %s",
                                dump_file_name, strerror(errno));
                  }
                  if (close(fd) == -1)
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "Failed to close() `%s' : %s",
                                dump_file_name, strerror(errno));
                  }
               }
#endif
               listbuffer[bytes_buffered] = '\0';
               if (eval_html_dir_list(listbuffer, bytes_buffered,
                                      &files_to_retrieve, file_size_to_retrieve,
                                      more_files_in_list, &list_length,
                                      &list_size, &files_deleted,
                                      &file_size_deleted,
                                      &listing_complete) == INCORRECT)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Failed to evaluate HTML directory listing.");
               }
            }
            free(listbuffer);
            listbuffer = NULL;
         } while (listing_complete == NO);

#ifdef WITH_SSL
         http_set_marker("", 0);
#endif

         if ((files_to_retrieve > 0) || (fsa->debug > NORMAL_MODE))
         {
            if (files_deleted > 0)
            {
               trans_log(DEBUG_SIGN, NULL, 0, NULL, NULL,
#if SIZEOF_OFF_T == 4
                         "%d files %ld bytes found for retrieving %s[%u files with %ld bytes in %s (deleted %u files with %ld bytes)]. @%x",
#else
                         "%d files %lld bytes found for retrieving %s[%u files with %lld bytes in %s (deleted %u files with %lld bytes)]. @%x",
#endif
                         files_to_retrieve, (pri_off_t)(*file_size_to_retrieve),
                         (*more_files_in_list == YES) ? "(+) " : "",
                         list_length, (pri_off_t)list_size,
                         (db.target_dir[0] == '\0') ? "home dir" : db.target_dir,
                         files_deleted, (pri_off_t)file_size_deleted, db.id.dir);
            }
            else
            {
               trans_log(DEBUG_SIGN, NULL, 0, NULL, NULL,
#if SIZEOF_OFF_T == 4
                         "%d files %ld bytes found for retrieving %s[%u files with %ld bytes in %s]. @%x",
#else
                         "%d files %lld bytes found for retrieving %s[%u files with %lld bytes in %s]. @%x",
#endif
                         files_to_retrieve, (pri_off_t)(*file_size_to_retrieve),
                         (*more_files_in_list == YES) ? "(+) " : "",
                         list_length, (pri_off_t)list_size,
                         (db.target_dir[0] == '\0') ? "home dir" : db.target_dir,
                         db.id.dir);
            }
         }
      }
      else /* Just copy the file mask list. */
      {
         char *p_mask,
              tmp_mask[MAX_FILENAME_LENGTH];

         if (now == 0)
         {
            now = time(NULL);
         }

         cached_i = -1;
         for (i = 0; i < nfg; i++)
         {
            p_mask = fml[i].file_list;
            for (j = 0; j < fml[i].fc; j++)
            {
               /*
                * We cannot just take the mask as is. We must check if we
                * need to expand the mask and then use the expansion.
                */
               (void)expand_filter(p_mask, tmp_mask, now);
               check_list(tmp_mask, strlen(tmp_mask), DS2UT_NONE, -1, 0, -1,
                          &files_to_retrieve, file_size_to_retrieve,
                          more_files_in_list);
               NEXT(p_mask);
            }
         }
         if ((files_to_retrieve > 0) || (fsa->debug > NORMAL_MODE))
         {
            trans_log(DEBUG_SIGN, NULL, 0, NULL, NULL,
#if SIZEOF_OFF_T == 4
                      "%d files %ld bytes found for retrieving in %s [%s]. @%x",
#else
                      "%d files %lld bytes found for retrieving in %s [%s]. @%x",
#endif
                      files_to_retrieve, (pri_off_t)(*file_size_to_retrieve),
                      (db.target_dir[0] == '\0') ? "home dir" : db.target_dir,
                      DO_NOT_GET_DIR_LIST_ID, db.id.dir);
         }
      }

      /* Free file mask list. */
      for (i = 0; i < nfg; i++)
      {
         free(fml[i].file_list);
      }
      free(fml);

      /*
       * Remove all files from the remote_list structure that are not
       * in the current directory listing.
       */
      if ((fra->stupid_mode != YES) && (fra->remove == NO))
      {
         int    files_removed = 0,
                i;
         size_t move_size;

         for (i = 0; i < (no_of_listed_files - files_removed); i++)
         {
            if (rl[i].in_list == NO)
            {
               int j = i;

               while ((j < (no_of_listed_files - files_removed)) &&
                      (rl[j].in_list == NO))
               {
                  j++;
               }
               if (j != (no_of_listed_files - files_removed))
               {
                  move_size = (no_of_listed_files - files_removed - j) *
                              sizeof(struct retrieve_list);
                  (void)memmove(&rl[i], &rl[j], move_size);
               }
               files_removed += (j - i);
            }
         }

         if (files_removed > 0)
         {
            int    tmp_current_no_of_listed_files = no_of_listed_files;
            size_t new_size,
                   old_size;

            no_of_listed_files -= files_removed;
            if (no_of_listed_files < 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Hmmm, no_of_listed_files = %d", no_of_listed_files);
               no_of_listed_files = 0;
            }
            if (no_of_listed_files == 0)
            {
               new_size = (RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                          AFD_WORD_OFFSET;
            }
            else
            {
               new_size = (((no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                           RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                          AFD_WORD_OFFSET;
            }
            old_size = (((tmp_current_no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                        RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                       AFD_WORD_OFFSET;

            if (old_size != new_size)
            {
               char *ptr;

               ptr = (char *)rl - AFD_WORD_OFFSET;
#ifdef DO_NOT_PARALLELIZE_ALL_FETCH
               if ((fra->stupid_mode == YES) || (fra->remove == YES))
               {
                  if ((ptr = realloc(ptr, new_size)) == NULL)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "realloc() error : %s", strerror(errno));
                     http_quit();
                     exit(INCORRECT);
                  }
               }
               else
               {
#endif
                  if ((ptr = mmap_resize(rl_fd, ptr, new_size)) == (caddr_t) -1)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "mmap_resize() error : %s", strerror(errno));
                     http_quit();
                     exit(INCORRECT);
                  }
                  rl_size = new_size;
#ifdef DO_NOT_PARALLELIZE_ALL_FETCH
               }
#endif
               if (no_of_listed_files < 0)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Hmmm, no_of_listed_files = %d", no_of_listed_files);
                  no_of_listed_files = 0;
               }
               current_no_of_listed_files = (int *)ptr;
               ptr += AFD_WORD_OFFSET;
               rl = (struct retrieve_list *)ptr;
            }
            *(int *)((char *)rl - AFD_WORD_OFFSET) = no_of_listed_files;
         }
      }
   }

   return(files_to_retrieve);
}


/*++++++++++++++++++++++++ eval_html_dir_list() +++++++++++++++++++++++++*/
static int
eval_html_dir_list(char         *html_buffer,
                   off_t        bytes_buffered,
                   int          *files_to_retrieve,
                   off_t        *file_size_to_retrieve,
                   int          *more_files_in_list,
                   unsigned int *list_length,
                   off_t        *list_size,
                   unsigned int *files_deleted,
                   off_t        *file_size_deleted,
                   int          *listing_complete)
{
   char *ptr;

   *listing_complete = YES;
#ifdef WITH_ATOM_FEED_SUPPORT
   /*
    * First test if we have an atom or rss feed. If that is not the case
    * lets test for a directory listing.
    */
   if ((html_buffer[0] == '<') && (html_buffer[1] == '?') &&
       (html_buffer[2] == 'x') && (html_buffer[3] == 'm') &&
       (html_buffer[4] == 'l') && (html_buffer[5] == ' '))
   {
      char *tmp_ptr;

      /* For now we just implement atom feeds. */
      if ((ptr = lposi(&html_buffer[6], "<entry>", 7)) == NULL)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                   "Unknown feed type. Terminating.");
         return(INCORRECT);
      }
      tmp_ptr = ptr;
      if ((ptr = lposi(&html_buffer[6], "<updated>", 7)) == NULL)
      {
      }
      if (ptr < tmp_ptr)
      {
         feed_time = extract_feed_date(ptr);
      }
   }
   else
   {
#endif
      cached_i = -1;
      if ((ptr = llposi(html_buffer, (size_t)bytes_buffered, "<h1>", 4)) == NULL)
      {
         if ((ptr = llposi(html_buffer, (size_t)bytes_buffered, "<PRE>", 5)) == NULL)
         {
            if ((ptr = llposi(html_buffer, (size_t)bytes_buffered,
                              "<?xml version=\"", 15)) == NULL)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Unknown HTML directory listing. Please send author a link so that this can be implemented.");
               return(INCORRECT);
            }
            else
            {
               if ((ptr = llposi(ptr, (size_t)bytes_buffered,
                                 "<IsTruncated>", 13)) == NULL)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Unknown HTML directory listing. Please send author a link so that this can be implemented.");
                  return(INCORRECT);
               }
               else
               {
                  int    date_str_length,
                         exact_date = DS2UT_NONE,
                         file_name_length = -1;
                  off_t  bytes_buffered_original = bytes_buffered,
                         exact_size,
                         file_size;
                  time_t file_mtime;
                  char   date_str[MAX_FILENAME_LENGTH],
                         *end_ptr = html_buffer + bytes_buffered,
#ifdef _WITH_EXTRA_CHECK
                         etag[MAX_EXTRA_LS_DATA_LENGTH],
#endif
                         file_name[MAX_FILENAME_LENGTH],
                         size_str[MAX_FILENAME_LENGTH];

                  /* true */
                  if ((*(ptr - 1) == 't') && (*ptr == 'r') &&
                      (*(ptr + 1) == 'u') && (*(ptr + 2) == 'e') &&
                      (*(ptr + 3) == '<'))
                  {
                     *listing_complete = NO;
                     ptr += 2;
                  }

                  ptr = html_buffer;
                  while ((ptr = llposi(ptr, (size_t)bytes_buffered,
                                       "<Contents><Key>", 15)) != NULL)
                  {
                     ptr--;
                     file_name_length = 0;
                     while ((file_name_length < MAX_FILENAME_LENGTH) &&
                            (*ptr != '<') && (*ptr != '\r') && (*ptr != '\0'))
                     {
                        file_name[file_name_length] = *ptr;
                        file_name_length++; ptr++;
                     }
                     if (*ptr == '<')
                     {
                        file_name[file_name_length] = '\0';
                        ptr++;
                        /* /Key><LastModified> */
                        if ((*ptr == '/') && (*(ptr + 1) == 'K') &&
                            (*(ptr + 2) == 'e') && (*(ptr + 3) == 'y') &&
                            (*(ptr + 4) == '>') && (*(ptr + 5) == '<') &&
                            (*(ptr + 6) == 'L') && (*(ptr + 7) == 'a') &&
                            (*(ptr + 8) == 's') && (*(ptr + 9) == 't') &&
                            (*(ptr + 10) == 'M') && (*(ptr + 11) == 'o') &&
                            (*(ptr + 12) == 'd') && (*(ptr + 13) == 'i') &&
                            (*(ptr + 14) == 'f') && (*(ptr + 15) == 'i') &&
                            (*(ptr + 16) == 'e') && (*(ptr + 17) == 'd') &&
                            (*(ptr + 18) == '>'))
                        {
                           ptr += 19;
                           date_str_length = 0;
                           while ((date_str_length < MAX_FILENAME_LENGTH) &&
                                  (*ptr != '<') && (*ptr != '\r') &&
                                  (*ptr != '\0'))
                           {
                              date_str[date_str_length] = *ptr;
                              date_str_length++; ptr++;
                           }
                           if (*ptr == '<')
                           {
                              date_str[date_str_length] = '\0';
                              file_mtime = datestr2unixtime(date_str,
                                                            &exact_date);
                              ptr++;
                              /* /LastModified><ETag> */
                              if ((*ptr == '/') && (*(ptr + 1) == 'L') &&
                                  (*(ptr + 2) == 'a') && (*(ptr + 3) == 's') &&
                                  (*(ptr + 4) == 't') && (*(ptr + 5) == 'M') &&
                                  (*(ptr + 6) == 'o') && (*(ptr + 7) == 'd') &&
                                  (*(ptr + 8) == 'i') && (*(ptr + 9) == 'f') &&
                                  (*(ptr + 10) == 'i') &&
                                  (*(ptr + 11) == 'e') &&
                                  (*(ptr + 12) == 'd') &&
                                  (*(ptr + 13) == '>') &&
                                  (*(ptr + 14) == '<') &&
                                  (*(ptr + 15) == 'E') &&
                                  (*(ptr + 16) == 'T') &&
                                  (*(ptr + 17) == 'a') &&
                                  (*(ptr + 18) == 'g') && (*(ptr + 19) == '>'))
                              {
                                 ptr += 20;
                                 date_str_length = 0;
#ifdef _WITH_EXTRA_CHECK
                                 while ((date_str_length < MAX_EXTRA_LS_DATA_LENGTH) &&
                                        (*ptr != '<') && (*ptr != '\r') &&
                                        (*ptr != '\0'))
                                 {
                                    etag[date_str_length] = *ptr;
                                    date_str_length++; ptr++;
                                 }
#else
                                 while ((*ptr != '<') && (*ptr != '\r') &&
                                        (*ptr != '\0'))
                                 {
                                    ptr++;
                                 }
#endif
                                 if (*ptr == '<')
                                 {
#ifdef _WITH_EXTRA_CHECK
                                    etag[date_str_length] = '\0';
#endif
                                    ptr++;
                                    /* /ETag><Size> */
                                    if ((*ptr == '/') && (*(ptr + 1) == 'E') &&
                                        (*(ptr + 2) == 'T') &&
                                        (*(ptr + 3) == 'a') &&
                                        (*(ptr + 4) == 'g') &&
                                        (*(ptr + 5) == '>') &&
                                        (*(ptr + 6) == '<') &&
                                        (*(ptr + 7) == 'S') &&
                                        (*(ptr + 8) == 'i') &&
                                        (*(ptr + 9) == 'z') &&
                                        (*(ptr + 10) == 'e') &&
                                        (*(ptr + 11) == '>'))
                                    {
                                       ptr += 12;
                                       date_str_length = 0;

                                       while ((date_str_length < MAX_FILENAME_LENGTH) &&
                                              (*ptr != '<') && (*ptr != '\r') &&
                                              (*ptr != '\0'))
                                       {
                                          size_str[date_str_length] = *ptr;
                                          date_str_length++; ptr++;
                                       }
                                       if (*ptr == '<')
                                       {
                                          size_str[date_str_length] = '\0';
                                          exact_size = convert_size(size_str,
                                                                    &file_size);
                                          if (fsa->debug > DEBUG_MODE)
                                          {
                                             trans_db_log(DEBUG_SIGN, NULL, 0,
                                                          NULL,
#if SIZEOF_OFF_T == 4
                                                          "eval_html_dir_list(): filename=%s length=%d mtime=%lld exact=%d size=%ld exact=%ld",
#else
                                                          "eval_html_dir_list(): filename=%s length=%d mtime=%lld exact=%d size=%lld exact=%lld",
#endif
                                                          file_name,
                                                          file_name_length,
                                                          file_mtime, exact_date,
                                                          (pri_off_t)file_size,
                                                          (pri_off_t)exact_size);
                                          }
                                          (*list_length)++;
                                          if (file_size > 0)
                                          {
                                             (*list_size) += file_size;
                                          }
                                          if (check_name(file_name,
                                                         file_name_length,
                                                         file_size, file_mtime,
                                                         files_deleted,
                                                         file_size_deleted) == YES)
                                          {
                                             (void)check_list(file_name,
                                                              file_name_length,
                                                              file_mtime,
                                                              exact_date,
                                                              exact_size,
                                                              file_size,
                                                              files_to_retrieve,
                                                              file_size_to_retrieve,
                                                              more_files_in_list);
                                          }
                                       }
                                       else
                                       {
                                          trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                                    "eval_html_dir_list", NULL,
                                                    "Unable to store size (length=%d char=%d).",
                                                    file_name_length, (int)*ptr);
                                          *listing_complete = YES;
                                          return(INCORRECT);
                                       }
                                    }
                                    else
                                    {
                                       trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                                 "eval_html_dir_list", NULL,
                                                 "No matching /ETag><Size> found.");
                                       *listing_complete = YES;
                                       return(INCORRECT);
                                    }
                                 }
                                 else
                                 {
                                    trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                              "eval_html_dir_list", NULL,
                                              "Unable to store etag (length=%d char=%d).",
                                              file_name_length, (int)*ptr);
                                    *listing_complete = YES;
                                    return(INCORRECT);
                                 }
                              }
                              else
                              {
                                 trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                           "eval_html_dir_list", NULL,
                                           "No matching /LastModified><ETag> found.");
                                 *listing_complete = YES;
                                 return(INCORRECT);
                              }
                           }
                           else
                           {
                              trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                        "eval_html_dir_list", NULL,
                                        "Unable to store date (length=%d char=%d).",
                                        file_name_length, (int)*ptr);
                              *listing_complete = YES;
                              return(INCORRECT);
                           }
                        }
                        else
                        {
                           trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "eval_html_dir_list", NULL,
                                     "No matching /Key><LastModified> found.");
                           *listing_complete = YES;
                           return(INCORRECT);
                        }
                     }
                     else
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "eval_html_dir_list", NULL,
                                  "Unable to store file name (length=%d char=%d).",
                                  file_name_length, (int)*ptr);
                        *listing_complete = YES;
                        return(INCORRECT);
                     }
                     bytes_buffered = end_ptr - ptr;
                  } /* while <Contents><Key> */

                  if (file_name_length == -1)
                  {
                     *listing_complete = YES;

                     /* Bucket is empty or we have some new */
                     /* listing type. So check if KeyCount  */
                     /* is zero.                            */
                     if ((ptr = llposi(html_buffer,
                                       (size_t)bytes_buffered_original,
                                       "<KeyCount>0</KeyCount>", 22)) == NULL)
                     {
                        /* No <Contents><Key> found! */
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  "Unknown HTML directory listing. Please send author a link so that this can be implemented.");
                        return(INCORRECT);
                     }
                  }

                  if (*listing_complete == NO)
                  {
                     int  marker_name_length;
                     char marker_name[24];

                     if (db.ssh_protocol == '1')
                     {
                        (void)strcpy(marker_name, "<NextMarker>");
                        marker_name_length = 12;
                     }
                     else
                     {
                        (void)strcpy(marker_name, "<NextContinuationToken>");
                        marker_name_length = 23;
                     }
                     if ((ptr = llposi(html_buffer,
                                       (size_t)bytes_buffered_original,
                                       marker_name,
                                       marker_name_length)) != NULL)
                     {
                        ptr--;
                        file_name_length = 0;
                        while ((file_name_length < MAX_FILENAME_LENGTH) &&
                               (*ptr != '<'))
                        {
                           file_name[file_name_length] = *ptr;
                           ptr++; file_name_length++;
                        }
                        file_name[file_name_length] = '\0';
                     }
                     else
                     {
                        if (db.ssh_protocol != '1')
                        {
                           trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                     NULL, html_buffer,
                                     "<IsTruncated> is true, but could not locate a <NextContinuationToken>!");
                           *listing_complete = YES;
                           return(INCORRECT);
                        }
                     }
                     http_set_marker(file_name, file_name_length);
                  }
               }
            }
         }
         else
         {
            while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
            {
               ptr++;
            }
            while ((*ptr == '\n') || (*ptr == '\r'))
            {
               ptr++;
            }
            if ((*ptr == '<') && (*(ptr + 1) == 'H') && (*(ptr + 2) == 'R'))
            {
               int    exact_date = DS2UT_NONE,
                      file_name_length;
               time_t file_mtime;
               off_t  exact_size,
                      file_size;
               char   date_str[MAX_FILENAME_LENGTH],
                      file_name[MAX_FILENAME_LENGTH],
                      size_str[MAX_FILENAME_LENGTH];

               /* Ignore HR line. */
               while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
               {
                  ptr++;
               }
               while ((*ptr == '\n') || (*ptr == '\r'))
               {
                  ptr++;
               }

               /* Ignore the two directory lines. */
               while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
               {
                  ptr++;
               }
               while ((*ptr == '\n') || (*ptr == '\r'))
               {
                  ptr++;
               }
               while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
               {
                  ptr++;
               }
               while ((*ptr == '\n') || (*ptr == '\r'))
               {
                  ptr++;
               }

               while (*ptr == '<')
               {
                  while (*ptr == '<')
                  {
                     ptr++;
                     while ((*ptr != '>') && (*ptr != '\n') &&
                            (*ptr != '\r') && (*ptr != '\0'))
                     {
                        ptr++;
                     }
                     if (*ptr == '>')
                     {
                        ptr++;
                        while (*ptr == ' ')
                        {
                           ptr++;
                        }
                     }
                  }

                  if ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                  {
                     /* Store file name. */
                     STORE_HTML_STRING(file_name, file_name_length,
                                       MAX_FILENAME_LENGTH, '<');

                     if (*ptr == '<')
                     {
                        while (*ptr == '<')
                        {
                           ptr++;
                           while ((*ptr != '>') && (*ptr != '\n') &&
                                  (*ptr != '\r') && (*ptr != '\0'))
                           {
                              ptr++;
                           }
                           if (*ptr == '>')
                           {
                              ptr++;
                              while (*ptr == ' ')
                              {
                                 ptr++;
                              }
                           }
                        }
                     }
                     if ((*ptr != '\n') && (*ptr != '\r') &&
                         (*ptr != '\0'))
                     {
                        while (*ptr == ' ')
                        {
                           ptr++;
                        }

                        /* Store date string. */
                        STORE_HTML_DATE();
                        file_mtime = datestr2unixtime(date_str, &exact_date);

                        if (*ptr == '<')
                        {
                           while (*ptr == '<')
                           {
                              ptr++;
                              while ((*ptr != '>') && (*ptr != '\n') &&
                                     (*ptr != '\r') && (*ptr != '\0'))
                              {
                                 ptr++;
                              }
                              if (*ptr == '>')
                              {
                                 ptr++;
                                 while (*ptr == ' ')
                                 {
                                    ptr++;
                                 }
                              }
                           }
                        }
                        if ((*ptr != '\n') && (*ptr != '\r') &&
                            (*ptr != '\0'))
                        {
                           int str_len;

                           /* Store size string. */
                           STORE_HTML_STRING(size_str, str_len,
                                             MAX_FILENAME_LENGTH, '<');
                           exact_size = convert_size(size_str,
                                                     &file_size);
                        }
                        else
                        {
                           exact_size = -1;
                           file_size = -1;
                        }
                     }
                     else
                     {
                        file_mtime = -1;
                        exact_size = -1;
                        file_size = -1;
                     }
                     if (fsa->debug > DEBUG_MODE)
                     {
                        trans_db_log(DEBUG_SIGN, NULL, 0, NULL,
#if SIZEOF_OFF_T == 4
                                     "eval_html_dir_list(): filename=%s length=%d mtime=%lld exact=%d size=%ld exact=%ld",
#else
                                     "eval_html_dir_list(): filename=%s length=%d mtime=%lld exact=%d size=%lld exact=%lld",
#endif
                                     file_name, file_name_length, file_mtime,
                                     exact_date, (pri_off_t)file_size,
                                     (pri_off_t)exact_size);
                     }

                     (*list_length)++;
                     if (file_size > 0)
                     {
                        (*list_size) += file_size;
                     }
                     if (check_name(file_name, file_name_length,
                                    file_size, file_mtime, files_deleted,
                                    file_size_deleted) != YES)
                     {
                        file_name[0] = '\0';
                     }
                  }
                  else
                  {
                     file_name[0] = '\0';
                     file_mtime = -1;
                     exact_size = -1;
                     file_size = -1;
                     break;
                  }

                  if (file_name[0] != '\0')
                  {
                     (void)check_list(file_name, file_name_length,
                                      file_mtime, exact_date, exact_size,
                                      file_size, files_to_retrieve,
                                      file_size_to_retrieve,
                                      more_files_in_list);
                  }

                  /* Go to end of line. */
                  while ((*ptr != '\n') && (*ptr != '\r') &&
                         (*ptr != '\0'))
                  {
                     ptr++;
                  }
                  while ((*ptr == '\n') || (*ptr == '\r'))
                  {
                     ptr++;
                  }
               }
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Unknown HTML directory listing. Please send author a link so that this can be implemented.");
               return(INCORRECT);
            }
         }
      }
      else
      {
         int    exact_date = DS2UT_NONE,
                file_name_length;
         time_t file_mtime;
         off_t  exact_size,
                file_size;
         char   date_str[MAX_FILENAME_LENGTH],
                file_name[MAX_FILENAME_LENGTH],
                size_str[MAX_FILENAME_LENGTH];

         while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
         {
            ptr++;
         }
         while ((*ptr == '\n') || (*ptr == '\r'))
         {
            ptr++;
         }
         while ((*ptr == ' ') || (*ptr == '\t'))
         {
            ptr++;
         }
         if (*ptr == '<')
         {
            /* Table type listing. */
            if ((*(ptr + 1) == 't') && (*(ptr + 2) == 'a') &&
                (*(ptr + 3) == 'b') && (*(ptr + 4) == 'l') &&
                (*(ptr + 5) == 'e') && (*(ptr + 6) == '>'))
            {
               ptr += 7;

               /* Ignore the two heading lines. */
               while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
               {
                  ptr++;
               }
               while ((*ptr == '\n') || (*ptr == '\r'))
               {
                  ptr++;
               }
               while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
               {
                  ptr++;
               }
               while ((*ptr == '\n') || (*ptr == '\r'))
               {
                  ptr++;
               }
               if ((*ptr == ' ') && (*(ptr + 1) == ' ') &&
                   (*(ptr + 2) == ' ') && (*(ptr + 3) == '<') &&
                   (*(ptr + 4) == 't') && (*(ptr + 5) == 'r') &&
                   (*(ptr + 6) == '>'))
               {
                  ptr += 7;

                  /* Ignore the two heading lines. */
                  while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                  {
                     ptr++;
                  }
                  while ((*ptr == '\n') || (*ptr == '\r'))
                  {
                     ptr++;
                  }
                  while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                  {
                     ptr++;
                  }
                  while ((*ptr == '\n') || (*ptr == '\r'))
                  {
                     ptr++;
                  }
               }

               if ((*ptr == '<') && (*(ptr + 1) == 't') &&
                   (*(ptr + 2) == 'r') && (*(ptr + 3) == '>') &&
                   (*(ptr + 4) == '<') && (*(ptr + 5) == 't') &&
                   (*(ptr + 6) == 'd'))
               {
                  /* Read line by line. */
                  do
                  {
                     file_name_length = 0;
                     ptr += 6;
                     while ((*ptr != '>') &&
                            (*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                     {
                        ptr++;
                     }
                     if (*ptr == '>')
                     {
                        ptr++;
                        while (*ptr == '<')
                        {
                           ptr++;
                           if ((*ptr == 'a') && (*(ptr + 1) == ' ') &&
                               (*(ptr + 2) == 'h') && (*(ptr + 3) == 'r') &&
                               (*(ptr + 4) == 'e') && (*(ptr + 5) == 'f') &&
                               (*(ptr + 6) == '=') && (*(ptr + 7) == '"'))
                           {
                              char *p_start;

                              ptr += 8;
                              p_start = ptr;

                              /* Go to end of href statement and cut out */
                              /* the file name.                          */
                              while ((*ptr != '"') && (*ptr != '\n') &&
                                     (*ptr != '\r') && (*ptr != '\0'))
                              {
                                 ptr++;
                              }
                              if (*ptr == '"')
                              {
                                 char *tmp_ptr = ptr;

                                 ptr--;
                                 while ((*ptr != '/') && (ptr != p_start))
                                 {
                                    ptr--;
                                 }
                                 while (*ptr == '/')
                                 {
                                    ptr++;
                                 }

                                 /* Store file name. */
                                 STORE_HTML_STRING(file_name, file_name_length,
                                                   MAX_FILENAME_LENGTH, '"');

                                 ptr = tmp_ptr + 1;
                              }

                              while ((*ptr != '>') && (*ptr != '\n') &&
                                     (*ptr != '\r') && (*ptr != '\0'))
                              {
                                 ptr++;
                              }
                              if (*ptr == '>')
                              {
                                 ptr++;
                              }
                           }
                           else
                           {
                              while ((*ptr != '>') && (*ptr != '\n') &&
                                     (*ptr != '\r') && (*ptr != '\0'))
                              {
                                 ptr++;
                              }
                              if (*ptr == '>')
                              {
                                 ptr++;
                              }
                           }
                        }
                        if ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                        {
                           /* Ensure we do not already have the file name from */
                           /* the href statement before.                       */
                           if (file_name_length == 0)
                           {
                              /* Store file name. */
                              STORE_HTML_STRING(file_name, file_name_length,
                                                MAX_FILENAME_LENGTH, '<');
                           }
                           else
                           {
                              while ((*ptr != '<') && (*ptr != '\n') &&
                                     (*ptr != '\r') && (*ptr != '\0'))
                              {
                                 ptr++;
                              }
                           }

                           while (*ptr == '<')
                           {
                              ptr++;
                              while ((*ptr != '>') && (*ptr != '\n') &&
                                     (*ptr != '\r') && (*ptr != '\0'))
                              {
                                 ptr++;
                              }
                              if (*ptr == '>')
                              {
                                 ptr++;
                              }
                           }
                           if ((*ptr != '\n') && (*ptr != '\r') &&
                               (*ptr != '\0'))
                           {
                              int str_len;

                              while (*ptr == ' ')
                              {
                                 ptr++;
                              }

                              /* Store date string. */
                              STORE_HTML_STRING(date_str, str_len,
                                                MAX_FILENAME_LENGTH, '<');
                              file_mtime = datestr2unixtime(date_str,
                                                             &exact_date);

                              while (*ptr == '<')
                              {
                                 ptr++;
                                 while ((*ptr != '>') && (*ptr != '\n') &&
                                        (*ptr != '\r') && (*ptr != '\0'))
                                 {
                                    ptr++;
                                 }
                                 if (*ptr == '>')
                                 {
                                    ptr++;
                                 }
                              }
                              if ((*ptr != '\n') && (*ptr != '\r') &&
                                  (*ptr != '\0'))
                              {
                                 /* Store size string. */
                                 STORE_HTML_STRING(size_str, str_len,
                                                   MAX_FILENAME_LENGTH, '<');
                                 exact_size = convert_size(size_str,
                                                           &file_size);
                              }
                              else
                              {
                                 exact_size = -1;
                                 file_size = -1;
                              }
                           }
                           else
                           {
                              file_mtime = -1;
                              exact_size = -1;
                              file_size = -1;
                           }
                           if (fsa->debug > DEBUG_MODE)
                           {
                              trans_db_log(DEBUG_SIGN, NULL, 0, NULL,
#if SIZEOF_OFF_T == 4
                                           "eval_html_dir_list(): filename=%s length=%d mtime=%lld exact=%d size=%ld exact=%ld",
#else
                                           "eval_html_dir_list(): filename=%s length=%d mtime=%lld exact=%d size=%lld exact=%lld",
#endif
                                           file_name, file_name_length,
                                           file_mtime, exact_date,
                                           (pri_off_t)file_size,
                                           (pri_off_t)exact_size);
                           }

                           (*list_length)++;
                           if (file_size > 0)
                           {
                              (*list_size) += file_size;
                           }

                           if (check_name(file_name, file_name_length,
                                          file_size, file_mtime, files_deleted,
                                          file_size_deleted) != YES)
                           {
                              file_name[0] = '\0';
                           }
                        }
                        else
                        {
                           file_name[0] = '\0';
                           file_mtime = -1;
                           exact_size = -1;
                           file_size = -1;
                        }

                        if (file_name[0] != '\0')
                        {
                           (void)check_list(file_name, file_name_length,
                                            file_mtime, exact_date, exact_size,
                                            file_size, files_to_retrieve,
                                            file_size_to_retrieve,
                                            more_files_in_list);
                        }
                     }

                     /* Go to end of line. */
                     while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                     {
                        ptr++;
                     }
                     while ((*ptr == '\n') || (*ptr == '\r'))
                     {
                        ptr++;
                     }
                  } while ((*ptr == '<') && (*(ptr + 1) == 't') &&
                           (*(ptr + 2) == 'r') && (*(ptr + 3) == '>') &&
                           (*(ptr + 4) == '<') && (*(ptr + 5) == 't') &&
                           (*(ptr + 6) == 'd'));
               }
               else
               {
                  if ((*ptr == ' ') && (*(ptr + 1) == ' ') &&
                      (*(ptr + 2) == ' ') && (*(ptr + 3) == '<') &&
                      (*(ptr + 4) == 't') && (*(ptr + 5) == 'r') &&
                      (*(ptr + 6) == '>'))
                  {
                     ptr += 7;
                     while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                     {
                        ptr++;
                     }
                     while ((*ptr == '\n') || (*ptr == '\r'))
                     {
                        ptr++;
                     }
                     while ((*ptr == ' ') || (*ptr == '\t'))
                     {
                        ptr++;
                     }
                     if ((*ptr == '<') && (*(ptr + 1) == '/') &&
                         (*(ptr + 2) == 't') && (*(ptr + 3) == 'a') &&
                         (*(ptr + 4) == 'b') && (*(ptr + 5) == 'l') &&
                         (*(ptr + 6) == 'e') && (*(ptr + 7) == '>'))
                     {
                        trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  "Directory empty.");
                        return(SUCCESS);
                     }
                  }
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Unknown HTML directory listing. Please send author a link so that this can be implemented.");
                  return(INCORRECT);
               }
            }
                 /* Pre type listing. */
            else if (((*(ptr + 1) == 'p') && (*(ptr + 4) == '>')) || /* <pre> */
                     ((*(ptr + 1) == 'a') && (*(ptr + 2) == ' ') &&  /* <a href= */
                      (*(ptr + 3) == 'h') && (*(ptr + 7) == '=')))
                 {
                    if ((*(ptr + 1) == 'p') && (*(ptr + 4) == '>'))
                    {
                       /* Ignore heading line. */
                       while ((*ptr != '\n') && (*ptr != '\r') &&
                              (*ptr != '\0'))
                       {
                          ptr++;
                       }
                       while ((*ptr == '\n') || (*ptr == '\r'))
                       {
                          ptr++;
                       }
                    }

                    while (*ptr == '<')
                    {
                       file_name[0] = '\0';
                       file_name_length = 0;
                       while (*ptr == '<')
                       {
                          ptr++;
                          if ((*ptr == 'a') && (*(ptr + 1) == ' ') &&
                           (*(ptr + 2) == 'h') && (*(ptr + 3) == 'r') &&
                           (*(ptr + 4) == 'e') && (*(ptr + 5) == 'f') &&
                           (*(ptr + 6) == '=') && (*(ptr + 7) == '"'))
                          {
                             ptr += 8;
                             STORE_HTML_STRING(file_name, file_name_length,
                                               MAX_FILENAME_LENGTH, '"');
                          }
                          else
                          {
                             while ((*ptr != '>') && (*ptr != '\n') &&
                                    (*ptr != '\r') && (*ptr != '\0'))
                             {
                                ptr++;
                             }
                          }
                          if (*ptr == '>')
                          {
                             ptr++;
                             while (*ptr == ' ')
                             {
                                ptr++;
                             }
                          }
                       }

                       if ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                       {
                          if (file_name[0] == '\0')
                          {
                             /* Store file name. */
                             STORE_HTML_STRING(file_name, file_name_length,
                                               MAX_FILENAME_LENGTH, '<');
                          }
                          else
                          {
                             /* Away with the shown, maybe cut off filename. */
                             while ((*ptr != '<') && (*ptr != '\n') &&
                                    (*ptr != '\r') && (*ptr != '\0'))
                             {
                                ptr++;
                             }
                          }

                          if (*ptr == '<')
                          {
                             while (*ptr == '<')
                             {
                                ptr++;
                                while ((*ptr != '>') && (*ptr != '\n') &&
                                       (*ptr != '\r') && (*ptr != '\0'))
                                {
                                   ptr++;
                                }
                                if (*ptr == '>')
                                {
                                   ptr++;
                                   while (*ptr == ' ')
                                   {
                                      ptr++;
                                   }
                                }
                             }
                          }
                          if ((*ptr != '\n') && (*ptr != '\r') &&
                              (*ptr != '\0'))
                          {
                             while (*ptr == ' ')
                             {
                                ptr++;
                             }

                             /* Store date string. */
                             STORE_HTML_DATE();
                             file_mtime = datestr2unixtime(date_str,
                                                           &exact_date);

                             if (*ptr == '<')
                             {
                                while (*ptr == '<')
                                {
                                   ptr++;
                                   while ((*ptr != '>') && (*ptr != '\n') &&
                                          (*ptr != '\r') && (*ptr != '\0'))
                                   {
                                      ptr++;
                                   }
                                   if (*ptr == '>')
                                   {
                                      ptr++;
                                      while (*ptr == ' ')
                                      {
                                         ptr++;
                                      }
                                   }
                                }
                             }
                             if ((*ptr != '\n') && (*ptr != '\r') &&
                                 (*ptr != '\0'))
                             {
                                int str_len;

                                /* Store size string. */
                                STORE_HTML_STRING(size_str, str_len,
                                                  MAX_FILENAME_LENGTH, '<');
                                exact_size = convert_size(size_str,
                                                          &file_size);
                             }
                             else
                             {
                                exact_size = -1;
                                file_size = -1;
                             }
                          }
                          else
                          {
                             file_mtime = -1;
                             exact_size = -1;
                             file_size = -1;
                          }
                          if (fsa->debug > DEBUG_MODE)
                          {
                             trans_db_log(DEBUG_SIGN, NULL, 0, NULL,
#if SIZEOF_OFF_T == 4
                                          "eval_html_dir_list(): filename=%s length=%d mtime=%lld exact=%d size=%ld exact=%ld",
#else
                                          "eval_html_dir_list(): filename=%s length=%d mtime=%lld exact=%d size=%lld exact=%lld",
#endif
                                          file_name, file_name_length,
                                          file_mtime, exact_date,
                                          (pri_off_t)file_size,
                                          (pri_off_t)exact_size);
                          }

                          (*list_length)++;
                          if (file_size > 0)
                          {
                             (*list_size) += file_size;
                          }

                          if (check_name(file_name, file_name_length,
                                         file_size, file_mtime, files_deleted,
                                         file_size_deleted) != YES)
                          {
                             file_name[0] = '\0';
                          }
                       }
                       else
                       {
                          file_name[0] = '\0';
                          file_mtime = -1;
                          exact_size = -1;
                          file_size = -1;
                          break;
                       }

                       if (file_name[0] != '\0')
                       {
                          (void)check_list(file_name, file_name_length,
                                           file_mtime, exact_date, exact_size,
                                           file_size, files_to_retrieve,
                                           file_size_to_retrieve,
                                           more_files_in_list);
                       }

                       /* Go to end of line. */
                       while ((*ptr != '\n') && (*ptr != '\r') &&
                              (*ptr != '\0'))
                       {
                          ptr++;
                       }
                       while ((*ptr == '\n') || (*ptr == '\r'))
                       {
                          ptr++;
                       }
                    }
                 }
                 /* List type listing. */
            else if ((*(ptr + 1) == 'u') && (*(ptr + 3) == '>'))
                 {
                    /* Ignore first line. */
                    while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while ((*ptr == '\n') || (*ptr == '\r'))
                    {
                       ptr++;
                    }

                    while (*ptr == '<')
                    {
                       while (*ptr == '<')
                       {
                          ptr++;
                          while ((*ptr != '>') && (*ptr != '\n') &&
                                 (*ptr != '\r') && (*ptr != '\0'))
                          {
                             ptr++;
                          }
                          if (*ptr == '>')
                          {
                             ptr++;
                             while (*ptr == ' ')
                             {
                                ptr++;
                             }
                          }
                       }

                       if ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                       {
                          /* Store file name. */
                          STORE_HTML_STRING(file_name, file_name_length,
                                            MAX_FILENAME_LENGTH, '<');

                          if (check_name(file_name, file_name_length,
                                         -1, -1, files_deleted,
                                         file_size_deleted) == YES)
                          {
                             file_mtime = -1;
                             exact_size = -1;
                             file_size = -1;
                             if (fsa->debug > DEBUG_MODE)
                             {
                                trans_db_log(DEBUG_SIGN, NULL, 0, NULL,
#if SIZEOF_OFF_T == 4
                                             "eval_html_dir_list(): filename=%s length=%d mtime=%lld exact=%d size=%ld exact=%ld",
#else
                                             "eval_html_dir_list(): filename=%s length=%d mtime=%lld exact=%d size=%lld exact=%lld",
#endif
                                             file_name, file_name_length,
                                             file_mtime, exact_date,
                                             (pri_off_t)file_size,
                                             (pri_off_t)exact_size);
                             }
                          }
                          else
                          {
                             file_name[0] = '\0';
                          }

                          (*list_length)++;
                          if (file_size > 0)
                          {
                             (*list_size) += file_size;
                          }
                       }
                       else
                       {
                          file_name[0] = '\0';
                          file_mtime = -1;
                          exact_size = -1;
                          file_size = -1;
                          break;
                       }

                       if (file_name[0] != '\0')
                       {
                          (void)check_list(file_name, file_name_length,
                                           file_mtime, exact_date, exact_size,
                                           file_size, files_to_retrieve,
                                           file_size_to_retrieve,
                                           more_files_in_list);
                       }

                       /* Go to end of line. */
                       while ((*ptr != '\n') && (*ptr != '\r') &&
                              (*ptr != '\0'))
                       {
                          ptr++;
                       }
                       while ((*ptr == '\n') || (*ptr == '\r'))
                       {
                          ptr++;
                       }
                    }
                 }
                 else
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                              "Unknown HTML directory listing. Please send author a link so that this can be implemented.");
                    return(INCORRECT);
                 }
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                      "Unknown HTML directory listing. Please send author a link so that this can be implemented.");
            return(INCORRECT);
         }
      }
#ifdef WITH_ATOM_FEED_SUPPORT
   }
#endif

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++++ check_list() ++++++++++++++++++++++++++++*/
static int
check_list(char   *file,
           int    file_name_length,
           time_t file_mtime,
           int    exact_date,
           off_t  exact_size,
           off_t  file_size,
           int    *files_to_retrieve,
           off_t  *file_size_to_retrieve,
           int    *more_files_in_list)
{
   int i,
       start_i;

   if (file_name_length >= (MAX_FILENAME_LENGTH - 1))
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                "Remote file name `%s' is to long, it may only be %d bytes long.",
                file, (MAX_FILENAME_LENGTH - 1));
      return(1);
   }

   if ((cached_i != -1) && ((cached_i + 1) < no_of_listed_files) &&
       (CHECK_STRCMP(rl[cached_i + 1].file_name, file) == 0))
   {
      start_i = cached_i + 1;
   }
   else
   {
      start_i = 0;
   }

   if ((fra->stupid_mode == YES) || (fra->remove == YES))
   {
      for (i = start_i; i < no_of_listed_files; i++)
      {
         if (CHECK_STRCMP(rl[i].file_name, file) == 0)
         {
            cached_i = i;
            rl[i].in_list = YES;
            if (((rl[i].assigned == 0) || (rl[i].retrieved == YES)) &&
                (((db.special_flag & OLD_ERROR_JOB) == 0) ||
#ifdef LOCK_DEBUG
                   (lock_region(rl_fd, (off_t)(LOCK_RETR_FILE + i),
                                __FILE__, __LINE__) == LOCK_IS_NOT_SET)
#else
                   (lock_region(rl_fd, (off_t)(LOCK_RETR_FILE + i)) == LOCK_IS_NOT_SET)
#endif
                  ))
            {
               int ret;

               if (((file_mtime == -1) || (exact_date != DS2UT_SECOND)) &&
                   (fra->ignore_file_time != 0) &&
#ifdef NEW_FRA
                   ((fra->dir_options & DONT_GET_DIR_LIST) == 0))
#else
                   ((fra->dir_flag & DONT_GET_DIR_LIST) == 0))
#endif
               {
                  int status;

                  if ((status = http_head(db.target_dir, file,
                                          &file_size, &file_mtime)) == SUCCESS)
                  {
                     exact_size = 1;
                     if (fsa->debug > NORMAL_MODE)
                     {
                        trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                     "Date for %s is %ld, size = %ld bytes.",
# else
                                     "Date for %s is %lld, size = %ld bytes.",
# endif
#else
# if SIZEOF_TIME_T == 4
                                     "Date for %s is %ld, size = %lld bytes.",
# else
                                     "Date for %s is %lld, size = %lld bytes.",
# endif
#endif
                                     file, (pri_time_t)file_mtime,
                                     (pri_off_t)file_size);
                     }
                  }
                  else
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               (status == INCORRECT) ? NULL : msg_str,
                               "Failed to get date and size of file %s (%d).",
                               file, status);
                     if (timeout_flag != OFF)
                     {
                        http_quit();
                        exit(DATE_ERROR);
                     }
                  }
               }
               rl[i].size = file_size;
               rl[i].prev_size = 0;
               rl[i].file_mtime = file_mtime;
               if (file_mtime == -1)
               {
                  rl[i].got_date = NO;
               }
               else
               {
                  rl[i].got_date = YES;
               }

               if ((fra->ignore_size == -1) ||
                   ((fra->gt_lt_sign & ISIZE_EQUAL) &&
                    (fra->ignore_size == rl[i].size)) ||
                   ((fra->gt_lt_sign & ISIZE_LESS_THEN) &&
                    (fra->ignore_size < rl[i].size)) ||
                   ((fra->gt_lt_sign & ISIZE_GREATER_THEN) &&
                    (fra->ignore_size > rl[i].size)))
               {
                  if (fra->ignore_file_time == 0)
                  {
                     *files_to_retrieve += 1;
                     if (rl[i].size > 0)
                     {
                        *file_size_to_retrieve += rl[i].size;
                     }
#ifdef DO_NOT_PARALLELIZE_ALL_FETCH
                     if ((fra->stupid_mode == YES) || (fra->remove == YES) ||
                         ((*files_to_retrieve < fra->max_copied_files) &&
                          (*file_size_to_retrieve < fra->max_copied_file_size)))
#else
                     if ((*files_to_retrieve < fra->max_copied_files) &&
                         (*file_size_to_retrieve < fra->max_copied_file_size))
#endif
                     {
                        rl[i].retrieved = NO;
#ifdef NEW_FRA
                        if (((fra->dir_options & ONE_PROCESS_JUST_SCANNING) == 0) ||
#else
                        if (((fra->dir_flag & ONE_PROCESS_JUST_SCANNING) == 0) ||
#endif
                            (db.special_flag & DISTRIBUTED_HELPER_JOB))
                        {
                           rl[i].assigned = (unsigned char)db.job_no + 1;
                        }
                        else
                        {
                           rl[i].assigned = 0;
                           *more_files_in_list = YES;
                        }
                     }
                     else
                     {
                        rl[i].assigned = 0;
                        *files_to_retrieve -= 1;
                        if (rl[i].size > 0)
                        {
                           *file_size_to_retrieve -= rl[i].size;
                        }
                        *more_files_in_list = YES;
                     }
                     ret = 0;
                  }
                  else
                  {
                     time_t diff_time;

                     diff_time = current_time - rl[i].file_mtime;
                     if (((fra->gt_lt_sign & IFTIME_EQUAL) &&
                          (fra->ignore_file_time == diff_time)) ||
                         ((fra->gt_lt_sign & IFTIME_LESS_THEN) &&
                          (fra->ignore_file_time < diff_time)) ||
                         ((fra->gt_lt_sign & IFTIME_GREATER_THEN) &&
                          (fra->ignore_file_time > diff_time)))
                     {
                        *files_to_retrieve += 1;
                        if (rl[i].size > 0)
                        {
                           *file_size_to_retrieve += rl[i].size;
                        }
#ifdef DO_NOT_PARALLELIZE_ALL_FETCH
                        if ((fra->stupid_mode == YES) ||
                            (fra->remove == YES) ||
                            ((*files_to_retrieve < fra->max_copied_files) &&
                             (*file_size_to_retrieve < fra->max_copied_file_size)))
#else
                        if ((*files_to_retrieve < fra->max_copied_files) &&
                            (*file_size_to_retrieve < fra->max_copied_file_size))
#endif
                        {
                           rl[i].retrieved = NO;
#ifdef NEW_FRA
                           if (((fra->dir_options & ONE_PROCESS_JUST_SCANNING) == 0) ||
#else
                           if (((fra->dir_flag & ONE_PROCESS_JUST_SCANNING) == 0) ||
#endif
                               (db.special_flag & DISTRIBUTED_HELPER_JOB))
                           {
                              rl[i].assigned = (unsigned char)db.job_no + 1;
                           }
                           else
                           {
                              rl[i].assigned = 0;
                              *more_files_in_list = YES;
                           }
                        }
                        else
                        {
                           rl[i].assigned = 0;
                           *files_to_retrieve -= 1;
                           if (rl[i].size > 0)
                           {
                              *file_size_to_retrieve -= rl[i].size;
                           }
                           *more_files_in_list = YES;
                        }
                        ret = 0;
                     }
                     else
                     {
                        ret = 1;
                     }
                  }
#ifdef DEBUG_ASSIGNMENT
                  trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
# if SIZEOF_OFF_T == 4
                            "%s assigned %d: file_name=%s assigned=%d size=%ld",
# else
                            "%s assigned %d: file_name=%s assigned=%d size=%lld",
# endif
                            (fra->ls_data_alias[0] == '\0') ? fra->dir_alias : fra->ls_data_alias,
                            i, rl[i].file_name, (int)rl[i].assigned,
                            (pri_off_t)rl[i].size);
#endif /* DEBUG_ASSIGNMENT */
               }
               else
               {
                  ret = 1;
               }
               if (db.special_flag & OLD_ERROR_JOB)
               {
#ifdef LOCK_DEBUG
                  unlock_region(rl_fd, (off_t)(LOCK_RETR_FILE + i), __FILE__, __LINE__);
#else
                  unlock_region(rl_fd, (off_t)(LOCK_RETR_FILE + i));
#endif
               }
               return(ret);
            }
            else
            {
               return(1);
            }
         }
      } /* for (i = 0; i < no_of_listed_files; i++) */
   }
   else
   {
      /* Check if this file is in the list. */
      for (i = start_i; i < no_of_listed_files; i++)
      {
         if (CHECK_STRCMP(rl[i].file_name, file) == 0)
         {
            cached_i = i;
            rl[i].in_list = YES;
            if ((rl[i].assigned != 0) ||
                ((fra->stupid_mode == GET_ONCE_ONLY) &&
                 ((rl[i].special_flag & RL_GOT_EXACT_SIZE_DATE) ||
                  (rl[i].retrieved == YES))))
            {
               if ((rl[i].retrieved == NO) && (rl[i].assigned == 0))
               {
#ifdef NEW_FRA
                  if (((fra->dir_options & ONE_PROCESS_JUST_SCANNING) == 0) ||
#else
                  if (((fra->dir_flag & ONE_PROCESS_JUST_SCANNING) == 0) ||
#endif
                      (db.special_flag & DISTRIBUTED_HELPER_JOB))
                  {
                     rl[i].assigned = (unsigned char)db.job_no + 1;
                  }
                  else
                  {
                     rl[i].assigned = 0;
                     *more_files_in_list = YES;
                  }
                  *files_to_retrieve += 1;
               }
               return(1);
            }

            if (((db.special_flag & OLD_ERROR_JOB) == 0) ||
#ifdef LOCK_DEBUG
                (lock_region(rl_fd, (off_t)(LOCK_RETR_FILE + i),
                             __FILE__, __LINE__) == LOCK_IS_NOT_SET))
#else
                (lock_region(rl_fd, (off_t)(LOCK_RETR_FILE + i)) == LOCK_IS_NOT_SET))
#endif
            {
               int   status;
               off_t prev_size = 0;

               /* Try to get remote date and size. */
#ifdef NEW_FRA
               if (((fra->dir_options & DONT_GET_DIR_LIST) == 0) &&
#else
               if (((fra->dir_flag & DONT_GET_DIR_LIST) == 0) &&
#endif
                   ((file_mtime == -1) || (exact_date != DS2UT_SECOND) ||
                    (file_size == -1) || (exact_size != 1)))
               {
                  if ((status = http_head(db.target_dir, file,
                                          &file_size, &file_mtime)) == SUCCESS)
                  {
                     exact_size = 1;
                     if (fsa->debug > NORMAL_MODE)
                     {
                        trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                     "Date for %s is %ld, size = %ld bytes.",
# else
                                     "Date for %s is %lld, size = %ld bytes.",
# endif
#else
# if SIZEOF_TIME_T == 4
                                     "Date for %s is %ld, size = %lld bytes.",
# else
                                     "Date for %s is %lld, size = %lld bytes.",
# endif
#endif
                                     file, (pri_time_t)file_mtime, 
                                     (pri_off_t)file_size);
                     }
                  }
                  else
                  {
                     trans_log((timeout_flag == ON) ? ERROR_SIGN : DEBUG_SIGN,
                               __FILE__, __LINE__, NULL,
                               (status == INCORRECT) ? NULL : msg_str,
                               "Failed to get date and size of file %s (%d).",
                               file, status);
                     if (timeout_flag != OFF)
                     {
                        http_quit();
                        exit(DATE_ERROR);
                     }
                  }
               }
               if (file_mtime == -1)
               {
                  rl[i].got_date = NO;
                  rl[i].retrieved = NO;
                  rl[i].assigned = 0;
                  rl[i].file_mtime = file_mtime;
               }
               else
               {
                  rl[i].got_date = YES;
                  if (rl[i].file_mtime != file_mtime)
                  {
                     rl[i].file_mtime = file_mtime;
                     rl[i].retrieved = NO;
                     rl[i].assigned = 0;
                  }
               }
               if (file_size == -1)
               {
                  rl[i].size = file_size;
                  rl[i].prev_size = 0;
                  rl[i].retrieved = NO;
                  rl[i].assigned = 0;
               }
               else
               {
                  if (rl[i].size != file_size)
                  {
                     prev_size = rl[i].size;
                     rl[i].size = file_size;
                     rl[i].retrieved = NO;
                     rl[i].assigned = 0;
                  }
               }

               if (rl[i].retrieved == NO)
               {
                  if ((fra->ignore_size == -1) ||
                      ((fra->gt_lt_sign & ISIZE_EQUAL) &&
                       (fra->ignore_size == rl[i].size)) ||
                      ((fra->gt_lt_sign & ISIZE_LESS_THEN) &&
                       (fra->ignore_size < rl[i].size)) ||
                      ((fra->gt_lt_sign & ISIZE_GREATER_THEN) &&
                       (fra->ignore_size > rl[i].size)))
                  {
                     off_t size_to_retrieve;

                     if ((rl[i].got_date == NO) ||
                         (fra->ignore_file_time == 0))
                     {
                        if (rl[i].size == -1)
                        {
                           size_to_retrieve = 0;
                        }
                        else
                        {
                           if ((fra->stupid_mode == APPEND_ONLY) &&
                               (rl[i].size > prev_size))
                           {
                              size_to_retrieve = rl[i].size - prev_size;
                           }
                           else
                           {
                              size_to_retrieve = rl[i].size;
                           }
                        }
                        rl[i].prev_size = prev_size;
#ifdef DO_NOT_PARALLELIZE_ALL_FETCH
                        if ((fra->stupid_mode == YES) ||
                            (fra->remove == YES) ||
                            (((*files_to_retrieve + 1) < fra->max_copied_files) &&
                             ((*file_size_to_retrieve + size_to_retrieve) < fra->max_copied_file_size)))
#else
                        if (((*files_to_retrieve + 1) < fra->max_copied_files) &&
                            ((*file_size_to_retrieve + size_to_retrieve) < fra->max_copied_file_size))
#endif
                        {
#ifdef NEW_FRA
                           if (((fra->dir_options & ONE_PROCESS_JUST_SCANNING) == 0) ||
#else
                           if (((fra->dir_flag & ONE_PROCESS_JUST_SCANNING) == 0) ||
#endif
                               (db.special_flag & DISTRIBUTED_HELPER_JOB))
                           {
                              rl[i].assigned = (unsigned char)db.job_no + 1;
                           }
                           else
                           {
                              rl[i].assigned = 0;
                              *more_files_in_list = YES;
                           }
                           *file_size_to_retrieve += size_to_retrieve;
                           *files_to_retrieve += 1;
                        }
                        else
                        {
                           rl[i].assigned = 0;
                           *more_files_in_list = YES;
                        }
                        status = 0;
                     }
                     else
                     {
                        time_t diff_time;

                        diff_time = current_time - rl[i].file_mtime;
                        if (((fra->gt_lt_sign & IFTIME_EQUAL) &&
                             (fra->ignore_file_time == diff_time)) ||
                            ((fra->gt_lt_sign & IFTIME_LESS_THEN) &&
                             (fra->ignore_file_time < diff_time)) ||
                            ((fra->gt_lt_sign & IFTIME_GREATER_THEN) &&
                             (fra->ignore_file_time > diff_time)))
                        {
                           if (rl[i].size == -1)
                           {
                              size_to_retrieve = 0;
                           }
                           else
                           {
                              if ((fra->stupid_mode == APPEND_ONLY) &&
                                  (rl[i].size > prev_size))
                              {
                                 size_to_retrieve = rl[i].size - prev_size;
                              }
                              else
                              {
                                 size_to_retrieve = rl[i].size;
                              }
                           }
                           rl[i].prev_size = prev_size;
#ifdef DO_NOT_PARALLELIZE_ALL_FETCH
                           if ((fra->stupid_mode == YES) ||
                               (fra->remove == YES) ||
                               (((*files_to_retrieve + 1) < fra->max_copied_files) &&
                                ((*file_size_to_retrieve + size_to_retrieve) < fra->max_copied_file_size)))
#else
                           if (((*files_to_retrieve + 1) < fra->max_copied_files) &&
                               ((*file_size_to_retrieve + size_to_retrieve) < fra->max_copied_file_size))
#endif
                           {
#ifdef NEW_FRA
                              if (((fra->dir_options & ONE_PROCESS_JUST_SCANNING) == 0) ||
#else
                              if (((fra->dir_flag & ONE_PROCESS_JUST_SCANNING) == 0) ||
#endif
                                  (db.special_flag & DISTRIBUTED_HELPER_JOB))
                              {
                                 rl[i].assigned = (unsigned char)db.job_no + 1;
                              }
                              else
                              {
                                 rl[i].assigned = 0;
                                 *more_files_in_list = YES;
                              }
                              *file_size_to_retrieve += size_to_retrieve;
                              *files_to_retrieve += 1;
                           }
                           else
                           {
                              rl[i].assigned = 0;
                              *more_files_in_list = YES;
                           }
                           status = 0;
                        }
                        else
                        {
                           status = 1;
                        }
                     }
#ifdef DEBUG_ASSIGNMENT
                     trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
# if SIZEOF_OFF_T == 4
                               "%s assigned %d: file_name=%s assigned=%d size=%ld",
# else
                               "%s assigned %d: file_name=%s assigned=%d size=%lld",
# endif
                               (fra->ls_data_alias[0] == '\0') ? fra->dir_alias : fra->ls_data_alias,
                               i, rl[i].file_name, (int)rl[i].assigned,
                               (pri_off_t)rl[i].size);
#endif /* DEBUG_ASSIGNMENT */
                  }
                  else
                  {
                     status = 1;
                  }
               }
               else
               {
                  status = 1;
               }
               if (db.special_flag & OLD_ERROR_JOB)
               {
#ifdef LOCK_DEBUG
                  unlock_region(rl_fd, (off_t)(LOCK_RETR_FILE + i),
                                __FILE__, __LINE__);
#else
                  unlock_region(rl_fd, (off_t)(LOCK_RETR_FILE + i));
#endif
               }
               return(status);
            }
            else
            {
               return(1);
            }
         }
      } /* for (i = 0; i < no_of_listed_files; i++) */
   }

   /* Add this file to the list. */
   if ((no_of_listed_files != 0) &&
       ((no_of_listed_files % RETRIEVE_LIST_STEP_SIZE) == 0))
   {
      char   *ptr;
      size_t new_size = (((no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                         RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                         AFD_WORD_OFFSET;

      ptr = (char *)rl - AFD_WORD_OFFSET;
#ifdef DO_NOT_PARALLELIZE_ALL_FETCH
      if ((fra->stupid_mode == YES) || (fra->remove == YES))
      {
         if ((ptr = realloc(ptr, new_size)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "realloc() error : %s", strerror(errno));
            http_quit();
            exit(INCORRECT);
         }
      }
      else
      {
#endif
         if ((ptr = mmap_resize(rl_fd, ptr, new_size)) == (caddr_t) -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "mmap_resize() error : %s", strerror(errno));
            http_quit();
            exit(INCORRECT);
         }
         rl_size = new_size;
#ifdef DO_NOT_PARALLELIZE_ALL_FETCH
      }
#endif
      if (no_of_listed_files < 0)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Hmmm, no_of_listed_files = %d", no_of_listed_files);
         no_of_listed_files = 0;
      }
      *(int *)ptr = no_of_listed_files;
      current_no_of_listed_files = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      rl = (struct retrieve_list *)ptr;
   }
   (void)my_strncpy(rl[no_of_listed_files].file_name, file, MAX_FILENAME_LENGTH);
#ifdef _WITH_EXTRA_CHECK
   rl[no_of_listed_files].extra_data[0] = '\0';
#endif
   rl[no_of_listed_files].retrieved = NO;
   rl[no_of_listed_files].in_list = YES;
   rl[no_of_listed_files].special_flag = 0;

#ifdef NEW_FRA
   if (((fra->dir_options & DONT_GET_DIR_LIST) == 0) &&
#else
   if (((fra->dir_flag & DONT_GET_DIR_LIST) == 0) &&
#endif
       ((file_mtime == -1) || (exact_date != DS2UT_SECOND) ||
        (file_size == -1) || (exact_size != 1)))
   {
      int status;

      if ((status = http_head(db.target_dir, file,
                              &file_size, &file_mtime)) == SUCCESS)
      {
         if (fsa->debug > NORMAL_MODE)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                         "Date for %s is %ld, size = %ld bytes.",
# else
                         "Date for %s is %lld, size = %ld bytes.",
# endif
#else
# if SIZEOF_TIME_T == 4
                         "Date for %s is %ld, size = %lld bytes.",
# else
                         "Date for %s is %lld, size = %lld bytes.",
# endif
#endif
                         file, file_mtime, file_size);
         }
      }
      else
      {
         trans_log((timeout_flag == ON) ? ERROR_SIGN : DEBUG_SIGN,
                   __FILE__, __LINE__, NULL,
                   (status == INCORRECT) ? NULL : msg_str,
                   "Failed to get date and size of file %s (%d).",
                   file, status);
         if (timeout_flag != OFF)
         {
            http_quit();
            exit(DATE_ERROR);
         }
      }
   }
   rl[no_of_listed_files].file_mtime = file_mtime;
   rl[no_of_listed_files].size = file_size;
   rl[no_of_listed_files].prev_size = 0;
   if (file_mtime == -1)
   {
      rl[no_of_listed_files].got_date = NO;
   }
   else
   {
      rl[no_of_listed_files].got_date = YES;
   }
   if ((file_mtime != -1) && (file_size != -1))
   {
      rl[no_of_listed_files].special_flag |= RL_GOT_EXACT_SIZE_DATE;
   }

   if ((fra->ignore_size == -1) ||
       ((fra->gt_lt_sign & ISIZE_EQUAL) &&
        (fra->ignore_size == rl[no_of_listed_files].size)) ||
       ((fra->gt_lt_sign & ISIZE_LESS_THEN) &&
        (fra->ignore_size < rl[no_of_listed_files].size)) ||
       ((fra->gt_lt_sign & ISIZE_GREATER_THEN) &&
        (fra->ignore_size > rl[no_of_listed_files].size)))
   {
      if ((rl[no_of_listed_files].got_date == NO) ||
          (fra->ignore_file_time == 0))
      {
         *files_to_retrieve += 1;
         if (file_size > 0)
         {
            *file_size_to_retrieve += file_size;
         }
         no_of_listed_files++;
      }
      else
      {
         time_t diff_time;

         diff_time = current_time - rl[no_of_listed_files].file_mtime;
         if (((fra->gt_lt_sign & IFTIME_EQUAL) &&
              (fra->ignore_file_time == diff_time)) ||
             ((fra->gt_lt_sign & IFTIME_LESS_THEN) &&
              (fra->ignore_file_time < diff_time)) ||
             ((fra->gt_lt_sign & IFTIME_GREATER_THEN) &&
              (fra->ignore_file_time > diff_time)))
         {
            *files_to_retrieve += 1;
            if (file_size > 0)
            {
               *file_size_to_retrieve += file_size;
            }
            no_of_listed_files++;
         }
         else
         {
            return(1);
         }
      }
#ifdef DO_NOT_PARALLELIZE_ALL_FETCH
      if ((fra->stupid_mode == YES) || (fra->remove == YES) ||
          ((*files_to_retrieve < fra->max_copied_files) &&
           (*file_size_to_retrieve < fra->max_copied_file_size)))
#else
      if ((*files_to_retrieve < fra->max_copied_files) &&
          (*file_size_to_retrieve < fra->max_copied_file_size))
#endif
      {
#ifdef NEW_FRA
         if (((fra->dir_options & ONE_PROCESS_JUST_SCANNING) == 0) ||
#else
         if (((fra->dir_flag & ONE_PROCESS_JUST_SCANNING) == 0) ||
#endif
             (db.special_flag & DISTRIBUTED_HELPER_JOB))
         {
            rl[no_of_listed_files - 1].assigned = (unsigned char)db.job_no + 1;
         }
         else
         {
            rl[no_of_listed_files - 1].assigned = 0;
            *more_files_in_list = YES;
         }
      }
      else
      {
         rl[no_of_listed_files - 1].assigned = 0;
         *files_to_retrieve -= 1;
         if (rl[no_of_listed_files - 1].size > 0)
         {
            *file_size_to_retrieve -= rl[no_of_listed_files - 1].size;
         }
         *more_files_in_list = YES;
      }
      *(int *)((char *)rl - AFD_WORD_OFFSET) = no_of_listed_files;
#ifdef DEBUG_ASSIGNMENT
      trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
# if SIZEOF_OFF_T == 4
                "%s assigned %d: file_name=%s assigned=%d size=%ld",
# else
                "%s assigned %d: file_name=%s assigned=%d size=%lld",
# endif
                (fra->ls_data_alias[0] == '\0') ? fra->dir_alias : fra->ls_data_alias,
                i, rl[i].file_name, (int)rl[i].assigned, (pri_off_t)rl[i].size);
#endif /* DEBUG_ASSIGNMENT */
      return(0);
   }
   else
   {
      return(1);
   }
}


#ifdef WITH_ATOM_FEED_SUPPORT
/*+++++++++++++++++++++++++ extract_feed_date() +++++++++++++++++++++++++*/
static time_t
extract_feed_date(char *time_str)
{
   if ((isdigit(time_str[0])) && (isdigit(time_str[1])) &&
       (isdigit(time_str[2])) && (isdigit(time_str[3])))
   {
      char      tmp_str[5];
      struct tm bd_time;

      /* Evaluate year. */
      tmp_str[0] = time_str[0];
      tmp_str[1] = time_str[1];
      tmp_str[2] = time_str[2];
      tmp_str[3] = time_str[3];
      tmp_str[4] = '\0';
      bd_time.tm_year = atoi(tmp_str) - 1900;

      if ((time_str[4] == '-') && (isdigit(time_str[5])) &&
          (isdigit(time_str[6])))
      {
         /* Get month. */
         tmp_str[0] = time_str[5];
         tmp_str[1] = time_str[6];
         tmp_str[2] = '\0';
         bd_time.tm_month = atoi(tmp_str) - 1;

         if ((time_str[7] == '-') && (isdigit(time_str[8])) &&
             (isdigit(time_str[9])))
         {
            /* Get day of month. */
            tmp_str[0] = time_str[8];
            tmp_str[1] = time_str[9];
            bd_time.tm_day = atoi(tmp_str);

            if ((time_str[10] == 'T') && (isdigit(time_str[11])) &&
                (isdigit(time_str[12])))
            {
               /* Get hour. */
               tmp_str[0] = time_str[11];
               tmp_str[1] = time_str[12];
               bd_time.tm_hour = atoi(tmp_str);

               if ((time_str[13] == ':') && (isdigit(time_str[14])) &&
                   (isdigit(time_str[15])))
               {
                  /* Get minute. */
                  tmp_str[0] = time_str[14];
                  tmp_str[1] = time_str[15];
                  bd_time.tm_min = atoi(tmp_str);

                  if ((time_str[16] == ':') && (isdigit(time_str[17])) &&
                      (isdigit(time_str[18])))
                  {
                     int pos = 19,
                         timezone_offset;

                     /* Get seconds. */
                     tmp_str[0] = time_str[17];
                     tmp_str[1] = time_str[18];
                     bd_time.tm_sec = atoi(tmp_str);

                     /* We only do full seconds, so ignore fractional part. */
                     if (time_str[pos] == '.')
                     {
                        pos++;
                        while (isdgit(time_str[pos]))
                        {
                           pos++;
                        }
                     }

                     if (((time_str[pos] == '+') || (time_str[pos] == '-')) &&
                         (isdigit(time_str[pos + 1])) &&
                         (isdigit(time_str[pos + 2])) &&
                         (time_str[pos + 3] == ':') &&
                         (isdigit(time_str[pos + 4])) &&
                         (isdigit(time_str[pos + 5])))
                     {
                        tmp_str[0] = time_str[pos + 1];
                        tmp_str[1] = time_str[pos + 2];
                        timezone_offset = atoi(tmp_str) * 3600;

                        tmp_str[0] = time_str[pos + 4];
                        tmp_str[1] = time_str[pos + 5];
                        timezone_offset += (atoi(tmp_str) * 60);
                        if (time_str[pos] == '-')
                        {
                           timezone_offset = -timezone_offset;
                        }
                     }
                     else
                     {
                        timezone_offset = 0;
                     }
                     bd_time.tm_isdst = 0;

                     return(mktime(&bd_time) + timezone_offset);
                  }
               }
            }
         }
      }
   }

   return(0);
}
#endif


/*---------------------------- check_name() -----------------------------*/
static int
check_name(char         *file_name,
           int          file_name_length,
           off_t        file_size,
           time_t       file_mtime,
           unsigned int *files_deleted,
           off_t        *file_size_deleted)
{
   int  gotcha = NO;
   char *p_mask;

#ifdef NEW_FRA
   if ((file_name[0] != '.') || (fra->dir_options & ACCEPT_DOT_FILES))
#else
   if ((file_name[0] != '.') || (fra->dir_flag & ACCEPT_DOT_FILES))
#endif
   {
      int i,
          j,
          status;

      if (fra->dir_flag & ALL_DISABLED)
      {
         if (fra->remove == YES)
         {
            delete_remote_file(HTTP, file_name, file_name_length,
#ifdef _DELETE_LOG
                               DELETE_HOST_DISABLED, 0, 0, 0,
#endif
                               files_deleted, file_size_deleted, file_size);
         }
      }
      else
      {
         for (i = 0; i < nfg; i++)
         {
            p_mask = fml[i].file_list;
            for (j = 0; j < fml[i].fc; j++)
            {
               if ((status = pmatch(p_mask, file_name, NULL)) == 0)
               {
                  gotcha = YES;
                  break;
               }
               else if (status == 1)
                    {
                       /* This file is definitly NOT wanted! */
                       /* Lets skip the rest of this group.  */
                       break;
                    }
               NEXT(p_mask);
            }
            if (gotcha == YES)
            {
               break;
            }
         }

         if ((gotcha == NO) && (status != 0) &&
             (fra->delete_files_flag & UNKNOWN_FILES))
         {
            time_t diff_time = current_time - file_mtime;

            if ((fra->unknown_file_time == -2) ||
                ((file_mtime != -1) && /* We do not have the time! */
                 (diff_time > fra->unknown_file_time) &&
                 (diff_time > DEFAULT_TRANSFER_TIMEOUT)))
            {
               /*
                * Before we delete the file make sure that some
                * helper job is currently getting this file.
                */
               if ((fra->stupid_mode == YES) || (fra->remove == YES))
               {
                  for (i = 0; i < no_of_listed_files; i++)
                  {
                     if (rl[i].assigned != 0)
                     {
                        gotcha = YES;
                        break;
                     }
                  }
               }
               if (gotcha == NO)
               {
                  delete_remote_file(HTTP, file_name, file_name_length,
#ifdef _DELETE_LOG
                                     (fra->in_dc_flag & UNKNOWN_FILES_IDC) ?  DEL_UNKNOWN_FILE : DEL_UNKNOWN_FILE_GLOB,
                                     diff_time, current_time, file_mtime,
#endif
                                     files_deleted, file_size_deleted,
                                     file_size);
               }
            }
         }
      }
   }
   else
   {
      if ((file_name[1] != '\0') && (file_name[1] != '.') &&
          (file_mtime != -1))
      {
         if ((fra->delete_files_flag & OLD_RLOCKED_FILES) &&
             (fra->locked_file_time != -1))
         {
            time_t diff_time = current_time - file_mtime;

            if (diff_time < 0)
            {
               diff_time = 0;
            }
            if ((diff_time > fra->locked_file_time) &&
                (diff_time > DEFAULT_TRANSFER_TIMEOUT))
            {
               delete_remote_file(HTTP, file_name, file_name_length,
#ifdef _DELETE_LOG
                                  (fra->in_dc_flag & OLD_LOCKED_FILES_IDC) ? DEL_OLD_LOCKED_FILE : DEL_OLD_RLOCKED_FILE_GLOB,
                                  diff_time, current_time, file_mtime,
#endif
                                  files_deleted, file_size_deleted,
                                  file_size);
            }
         }
      }
   }

   return(gotcha);
}


/*------------------------- convert_size() ------------------------------*/
static off_t
convert_size(char *size_str, off_t *size)
{
   off_t exact_size;
   char  *ptr,
         *ptr_start;

   ptr = size_str;
   while (*ptr == ' ')
   {
      ptr++;
   }
   ptr_start = ptr;

   while (isdigit((int)*ptr))
   {
      ptr++;
   }
   if (*ptr == '.')
   {
      ptr++;
      while (isdigit((int)*ptr))
      {
         ptr++;
      }
   }
   if (ptr != ptr_start)
   {
      switch (*ptr)
      {
         case 'K': /* Kilobytes. */
            exact_size = KILOBYTE;
            break;
         case 'M': /* Megabytes. */
            exact_size = MEGABYTE;
            break;
         case 'G': /* Gigabytes. */
            exact_size = GIGABYTE;
            break;
         case 'T': /* Terabytes. */
            exact_size = TERABYTE;
            break;
         case 'P': /* Petabytes. */
            exact_size = PETABYTE;
            break;
         case 'E': /* Exabytes. */
            exact_size = EXABYTE;
            break;
         default :
            exact_size = 1;
            break;
      }
      *size = (off_t)(strtod(ptr_start, (char **)NULL) * exact_size);
   }
   else
   {
      *size = -1;
      exact_size = -1;
   }

   return(exact_size);
}
