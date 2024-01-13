/*
 *  get_remote_file_names_ftp.c - Part of AFD, an automatic file distribution
 *                                program.
 *  Copyright (c) 2000 - 2024 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_remote_file_names_ftp - retrieves filename, size and date
 **
 ** SYNOPSIS
 **   int get_remote_file_names_ftp(off_t        *file_size_to_retrieve,
 **                                 int          *more_files_in_list,
 **                                 unsigned int ftp_options)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   On success the number of files that are to be retrieved. On error
 **   it will exit.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   20.08.2000 H.Kiehl   Created
 **   15.07.2002 H.Kiehl   Option to ignore files which have a certain size.
 **   10.04.2005 H.Kiehl   Detect older version 1.2.x ls data types and try
 **                        to convert them.
 **   24.04.2012 S.Knudsen Ignore ./ that some FTP Servers return after
 **                        a NLST command.
 **   22.01.2017 H.Kiehl   Ensure we do not return the -1 if we do not
 **                        get the size.
 **   03.09.2017 H.Kiehl   Added option to get only appended part.
 **   14.10.2017 H.Kiehl   If we know via FEAT that SIZE and MDTM are
 **                        supported, lets not assume for the rest of the
 **                        session that this command is not supported.
 **   29.07.2019 H.Kiehl   Added check if ls_data file is changed while we
 **                        work with it.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                /* strerror(), memmove()              */
#include <stdlib.h>                /* malloc(), realloc(), free()        */
#include <time.h>                  /* time(), mktime()                   */ 
#include <sys/time.h>              /* struct tm                          */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>                /* unlink()                           */
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include "ftpdefs.h"
#include "fddefs.h"


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
static int                        check_date = YES,
                                  check_size = YES,
                                  get_date;
static time_t                     current_time;

/* Local function prototypes. */
static void                       do_scan(int *, off_t *, int *, unsigned int);
static int                        check_list(char *, int *, time_t *, off_t *,
                                             int *, unsigned int);


/*##################### get_remote_file_names_ftp() #####################*/
int
get_remote_file_names_ftp(off_t        *file_size_to_retrieve,
                          int          *more_files_in_list,
                          unsigned int ftp_options)
{
   int files_to_retrieve = 0,
       i = 0;

   *file_size_to_retrieve = 0;
   if (((fra->stupid_mode == GET_ONCE_ONLY) ||
        (fra->stupid_mode == GET_ONCE_NOT_EXACT)) &&
       (fra->ignore_file_time == 0))
   {
      get_date = NO;
   }
   else
   {
      get_date = YES;
   }
#ifdef DO_NOT_PARALLELIZE_ALL_FETCH
   if ((*more_files_in_list == YES) ||
       (db.special_flag & DISTRIBUTED_HELPER_JOB) ||
       ((db.special_flag & OLD_ERROR_JOB) && (db.retries < 30) &&
        (fra->stupid_mode != YES) &&
        (fra->remove != YES)))
#else
   if (rl_fd == -1)
   {
try_attach_again:
      if (attach_ls_data(fra, db.special_flag, YES) == INCORRECT)
      {
         (void)ftp_quit();
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
                  (void)ftp_quit();
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
            (void)ftp_quit();
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
            if ((fra->stupid_mode == YES) ||
                (fra->remove == YES) ||
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
                  if ((check_date == YES) && (rl[i].got_date == NO) &&
                      (get_date == YES))
                  {
                     if ((fra->stupid_mode == YES) || (fra->remove == YES))
                     {
                        if (fra->ignore_file_time != 0)
                        {
                           int    status;
                           time_t file_mtime;

                           if ((status = ftp_date(rl[i].file_name,
                                                  &file_mtime)) == SUCCESS)
                           {
                              rl[i].special_flag |= RL_GOT_EXACT_DATE;
                              rl[i].file_mtime = file_mtime;
                              rl[i].got_date = YES;
                              if (fsa->debug > NORMAL_MODE)
                              {
                                 trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                              "Date for %s is %ld.",
                                              rl[i].file_name, file_mtime);
                              }
                           }
                           else if ((status == 500) || (status == 502))
                                {
                                   if ((ftp_options & FTP_OPTION_MDTM) == 0)
                                   {
                                      check_date = NO;
                                   }
                                   rl[i].got_date = NO;
                                   if (fsa->debug > NORMAL_MODE)
                                   {
                                      trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                                   "Date command MDTM not supported [%d]",
                                                   status);
                                   }
                                }
                                else
                                {
                                   trans_log((timeout_flag == ON) ? ERROR_SIGN : DEBUG_SIGN,
                                             __FILE__, __LINE__, NULL, msg_str,
                                             "Failed to get date of file %s.",
                                             rl[i].file_name);
                                   if (timeout_flag != OFF)
                                   {
#ifdef LOCK_DEBUG
                                      unlock_region(rl_fd,
                                                    (off_t)(LOCK_RETR_FILE + i),
                                                    __FILE__, __LINE__);
#else
                                      unlock_region(rl_fd,
                                                    (off_t)(LOCK_RETR_FILE + i));
#endif
                                      (void)ftp_quit();
                                      exit(DATE_ERROR);
                                   }
                                   rl[i].got_date = NO;
                                }
                        }
                        else
                        {
                           rl[i].got_date = NO;
                        }
                     }
                     else
                     {
                        int    status;
                        time_t file_mtime;

                        if ((status = ftp_date(rl[i].file_name,
                                               &file_mtime)) == SUCCESS)
                        {
                           rl[i].special_flag |= RL_GOT_EXACT_DATE;
                           rl[i].file_mtime = file_mtime;
                           rl[i].got_date = YES;
                           if (fsa->debug > NORMAL_MODE)
                           {
                              trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                           "Date for %s is %ld.",
                                           rl[i].file_name, file_mtime);
                           }
                        }
                        else if ((status == 500) || (status == 502))
                             {
                                if ((ftp_options & FTP_OPTION_MDTM) == 0)
                                {
                                   check_date = NO;
                                }
                                rl[i].got_date = NO;
                                if (fsa->debug > NORMAL_MODE)
                                {
                                   trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                                "Date command MDTM not supported [%d]",
                                                status);
                                }
                             }
                             else
                             {
                                trans_log((timeout_flag == ON) ? ERROR_SIGN : DEBUG_SIGN,
                                          __FILE__, __LINE__, NULL, msg_str,
                                          "Failed to get date of file %s.",
                                          rl[i].file_name);
                                if (timeout_flag != OFF)
                                {
#ifdef LOCK_DEBUG
                                   unlock_region(rl_fd,
                                                 (off_t)(LOCK_RETR_FILE + i),
                                                 __FILE__, __LINE__);
#else
                                   unlock_region(rl_fd,
                                                 (off_t)(LOCK_RETR_FILE + i));
#endif
                                   (void)ftp_quit();
                                   exit(DATE_ERROR);
                                }
                                rl[i].got_date = NO;
                             }
                     }
                  }
                  else
                  {
                     rl[i].got_date = NO;
                  }

                  if ((check_size == YES) && (rl[i].size == -1))
                  {
                     int   status;
                     off_t size;

                     if ((status = ftp_size(rl[i].file_name, &size)) == SUCCESS)
                     {
                        rl[i].special_flag |= RL_GOT_EXACT_SIZE;
                        rl[i].size = size;
                        if (fsa->debug > NORMAL_MODE)
                        {
                           trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                        "Size for %s is %d.",
                                        rl[i].file_name, size);
                        }
                     }
                     else if ((status == 500) || (status == 502))
                          {
                             if ((ftp_options & FTP_OPTION_SIZE) == 0)
                             {
                                check_size = NO;
                             }
                             rl[i].size = -1;
                             if (fsa->debug > NORMAL_MODE)
                             {
                                trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                             "Size command SIZE not supported [%d]",
                                             status);
                             }
                          }
                          else
                          {
                             trans_log((timeout_flag == ON) ? ERROR_SIGN : DEBUG_SIGN,
                                       __FILE__, __LINE__, NULL, msg_str,
                                       "Failed to get size of file %s.",
                                       rl[i].file_name);
                             if (timeout_flag != OFF)
                             {
#ifdef LOCK_DEBUG
                                unlock_region(rl_fd,
                                              (off_t)(LOCK_RETR_FILE + i),
                                              __FILE__, __LINE__);
#else
                                unlock_region(rl_fd,
                                              (off_t)(LOCK_RETR_FILE + i));
#endif
                                (void)ftp_quit();
                                exit(SIZE_ERROR);
                             }
                             rl[i].size = -1;
                          }
                  }

                  if ((fra->ignore_size == -1) ||
                      ((fra->gt_lt_sign & ISIZE_EQUAL) &&
                       (fra->ignore_size != rl[i].size)) ||
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
                        if (((fra->dir_options & ONE_PROCESS_JUST_SCANNING) == 0) ||
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
                             (fra->ignore_file_time != diff_time)) ||
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
                           if (((fra->dir_options & ONE_PROCESS_JUST_SCANNING) == 0) ||
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
      } /* for (i = 0; i < no_of_listed_files; i++) */
#ifndef DO_NOT_PARALLELIZE_ALL_FETCH
      if ((files_to_retrieve == 0) && (db.special_flag & OLD_ERROR_JOB) &&
          ((db.special_flag & DISTRIBUTED_HELPER_JOB) == 0))
      {
         do_scan(&files_to_retrieve, file_size_to_retrieve, more_files_in_list,
                 ftp_options);
      }
#endif
   }
   else
   {
      do_scan(&files_to_retrieve, file_size_to_retrieve, more_files_in_list,
              ftp_options);
   }

   return(files_to_retrieve);
}


/*+++++++++++++++++++++++++++++++ do_scan() +++++++++++++++++++++++++++++*/
static void
do_scan(int          *files_to_retrieve,
        off_t        *file_size_to_retrieve,
        int          *more_files_in_list,
        unsigned int ftp_options)
{
   unsigned int     files_deleted = 0,
                    list_length = 0;
   int              gotcha,
                    i,
                    j,
                    nfg,           /* Number of file mask. */
                    status,
                    type;
   char             *nlist = NULL,
                    *p_end,
                    *p_list,
                    *p_mask;
   struct file_mask *fml = NULL;
   struct tm        *p_tm;

   /*
    * Get a directory listing from the remote site so we can see
    * what files are there.
    */
#ifdef WITH_SSL
   if (db.tls_auth == BOTH)
   {
      if ((fra->delete_files_flag & OLD_RLOCKED_FILES) &&
          (fra->locked_file_time != -1))
      {
         type = FNLIST_CMD | BUFFERED_LIST | ENCRYPT_DATA;
      }
      else
      {
         type = NLIST_CMD | BUFFERED_LIST | ENCRYPT_DATA;
      }
   }
   else
   {
#endif
      if ((fra->delete_files_flag & OLD_RLOCKED_FILES) &&
          (fra->locked_file_time != -1))
      {
         type = FNLIST_CMD | BUFFERED_LIST;
      }
      else
      {
         type = NLIST_CMD | BUFFERED_LIST;
      }
#ifdef WITH_SSL
   }
#endif
   if ((status = ftp_list(db.mode_flag, type, &nlist)) != SUCCESS)
   {
      if ((status == 550) || (status == 450))
      {
         trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   "Failed to send NLST command (%d).", status);
         *files_to_retrieve = 0;
         return;
      }
      else if (status == 226)
           {
              trans_log(INFO_SIGN, NULL, 0, NULL, msg_str,
                        "No files found (%d).", status);
              *files_to_retrieve = 0;
              return;
           }
           else
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                        "Failed to send NLST command (%d).", status);
              (void)ftp_quit();
              exit(LIST_ERROR);
           }
   }
   else
   {
      if (fsa->debug > NORMAL_MODE)
      {
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                      "Send NLST command.");
      }
   }

   /*
    * Some systems return 550 for the NLST command when no files
    * are found, others return 125 (ie. success) but do not return
    * any data. So check here if this is the second case.
    */
   if (nlist == NULL)
   {
      if (fsa->debug > NORMAL_MODE)
      {
         trans_log(DEBUG_SIGN, NULL, 0, NULL, NULL,
                   "0 files 0 bytes found for retrieving [0 files in %s]. @%x",
                   (db.target_dir[0] == '\0') ? "home dir" : db.target_dir,
                   db.id.dir);
      }
      *files_to_retrieve = 0;
      return;
   }

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
                         "Failed to get the file masks for %s, because lock is already set",
                         fra->dir_alias);
           }
           else
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to get the file masks for %s. (%d)",
                         fra->dir_alias, j);
           }
      free(fml);
      (void)ftp_quit();
      exit(INCORRECT);
   }

#ifdef DO_NOT_PARALLELIZE_ALL_FETCH
   if ((fra->stupid_mode == YES) || (fra->remove == YES))
   {
      if (reset_ls_data() == INCORRECT)
      {
         (void)ftp_quit();
         exit(INCORRECT);
      }
   }
   else
   {
      if (rl_fd == -1)
      {
         if (attach_ls_data(fra, db.special_flag, YES) == INCORRECT)
         {
            (void)ftp_quit();
            exit(INCORRECT);
         }
      }
   }
#else
   if (rl_fd == -1)
   {
      if (attach_ls_data(fra, db.special_flag, YES) == INCORRECT)
      {
         (void)ftp_quit();
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
            (void)ftp_quit();
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
         p_tm = gmtime(&current_time);
         current_time = mktime(p_tm);
      }
      else
      {
         current_time = time(NULL);
      }
   }

   /* Reduce the list to what is really required. */
   p_list = nlist;
   do
   {
      p_end = p_list;
      while ((*p_end != '\n') && (*p_end != '\r') && (*p_end != '\0'))
      {
         p_end++;
      }
      if (*p_end != '\0')
      {
         /* Some FTP Servers (WARDFTP) return ./filename in responce */
         /* to a NLST command. Lets ignore the ./.                   */
         if ((*p_list == '.') && (*(p_list + 1) == '/'))
         {
            p_list += 2;
         }
         if ((*p_list != '.') || (fra->dir_options & ACCEPT_DOT_FILES))
         {
            *p_end = '\0';
            list_length++;

            /* Check that the file name is not to long! */
            if ((p_end - p_list) >= (MAX_FILENAME_LENGTH - 1))
            {
               /* File name to long! */
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Remote file name `%s' is to long, it may only be %d bytes long.",
                         p_list, (MAX_FILENAME_LENGTH - 1));
            }
            else
            {
               if (fra->dir_flag & ALL_DISABLED)
               {
                  if (fra->remove == YES)
                  {
                     delete_remote_file(FTP, p_list, strlen(p_list),
#ifdef _DELETE_LOG
                                        DELETE_HOST_DISABLED, 0, 0, 0,
#endif
                                        &files_deleted, NULL, -1);
                  }
               }
               else
               {
                  time_t file_mtime = 0; /* Silence compiler. */

                  gotcha = NO;
                  for (i = 0; i < nfg; i++)
                  {
                     p_mask = fml[i].file_list;
                     for (j = 0; j < fml[i].fc; j++)
                     {
                        if ((status = pmatch(p_mask, p_list, NULL)) == 0)
                        {
                           if (check_list(p_list, files_to_retrieve,
                                          &file_mtime,
                                          file_size_to_retrieve,
                                          more_files_in_list,
                                          ftp_options) == 0)
                           {
                              gotcha = YES;
                           }
                           else
                           {
                              gotcha = NEITHER;
                           }
                           break;
                        }
                        else if (status == 1)
                             {
                                /* This file is definitly NOT wanted! */
                                /* Lets skip the rest of this group.  */
                                break;
                             }
#ifdef SHOW_FILTER_MISSES
                        if ((status == -1) ||
                            (fsa->debug > NORMAL_MODE))
                        {
                           char tmp_mask[MAX_FILENAME_LENGTH];

                           if (expand_filter(p_mask, tmp_mask, time(NULL)) == YES)
                           {
                              trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                                           "%s (%s) not fitting %s",
                                           p_mask, tmp_mask, p_list);
                           }
                           else
                           {
                              trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                                           "%s not fitting %s",
                                           p_mask, p_list);
                           }
                        }
#endif
                        NEXT(p_mask);
                     }
                     if ((gotcha == YES) || (gotcha == NEITHER))
                     {
                        break;
                     }
                  }

                  if ((gotcha == NO) && (status != 0) &&
                      (file_mtime > 0) &&
                      (fra->delete_files_flag & UNKNOWN_FILES))
                  {
                     time_t diff_time = current_time - file_mtime;

                     if ((fra->unknown_file_time == -2) ||
                         (file_mtime <= 0) ||
                         ((diff_time > fra->unknown_file_time) &&
                          (diff_time > DEFAULT_TRANSFER_TIMEOUT)))
                     {
                        delete_remote_file(FTP, p_list, strlen(p_list),
#ifdef _DELETE_LOG
                                           (fra->in_dc_flag & UNKNOWN_FILES_IDC) ?  DEL_UNKNOWN_FILE : DEL_UNKNOWN_FILE_GLOB,
                                           diff_time, current_time, file_mtime,
#endif
                                           &files_deleted, NULL, -1);
                     }
                  }
               }
            }
         }
         else
         {
            if ((*p_list == '.') && (*(p_list + 1) != '.') &&
                (*(p_list + 1) != '\r') && (*(p_list + 1) != '\n') &&
                ((p_end - p_list) < MAX_FILENAME_LENGTH) &&
                (fra->delete_files_flag & OLD_RLOCKED_FILES) &&
                (fra->locked_file_time != -1))
            {
               time_t file_mtime = 0;

               *p_end = '\0';
               if ((status = ftp_date(p_list, &file_mtime)) == SUCCESS)
               {
                  time_t diff_time = current_time - file_mtime;

                  if (fsa->debug > NORMAL_MODE)
                  {
                     trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                  "Date for %s is %ld.", p_list, file_mtime);
                  }

                  if (diff_time < 0)
                  {
                     diff_time = 0;
                  }
                  if ((diff_time > fra->locked_file_time) &&
                      (diff_time > DEFAULT_TRANSFER_TIMEOUT))
                  {
                     delete_remote_file(FTP, p_list, strlen(p_list),
#ifdef _DELETE_LOG
                                        (fra->in_dc_flag & OLD_LOCKED_FILES_IDC) ? DEL_OLD_LOCKED_FILE : DEL_OLD_RLOCKED_FILE_GLOB,
                                        diff_time, current_time, file_mtime,
#endif
                                        &files_deleted, NULL, -1);
                  }
               }
            }
         }
         p_list = p_end + 1;
         while ((*p_list == '\r') || (*p_list == '\n'))
         {
            p_list++;
         }
      }
      else
      {
         p_list = p_end;
      }
   } while (*p_list != '\0');

   /* Free file mask list. */
   free(nlist);
   for (i = 0; i < nfg; i++)
   {
      free(fml[i].file_list);
   }
   free(fml);

   if ((*files_to_retrieve > 0) || (fsa->debug > NORMAL_MODE))
   {
      if (files_deleted > 0)
      {
         trans_log(DEBUG_SIGN, NULL, 0, NULL, NULL,
#if SIZEOF_OFF_T == 4
                   "%d files %ld bytes found for retrieving %s[%u files in %s (deleted %u files)]. @%x",
#else
                   "%d files %lld bytes found for retrieving %s[%u files in %s (deleted %u files)]. @%x",
#endif
                   *files_to_retrieve, (pri_off_t)(*file_size_to_retrieve),
                   (*more_files_in_list == YES) ? "(+) " : "", list_length,
                   (db.target_dir[0] == '\0') ? "home dir" : db.target_dir,
                   files_deleted, db.id.dir);
      }
      else
      {
         trans_log(DEBUG_SIGN, NULL, 0, NULL, NULL,
#if SIZEOF_OFF_T == 4
                   "%d files %ld bytes found for retrieving %s[%u files in %s]. @%x",
#else
                   "%d files %lld bytes found for retrieving %s[%u files in %s]. @%x",
#endif
                   *files_to_retrieve, (pri_off_t)(*file_size_to_retrieve),
                   (*more_files_in_list == YES) ? "(+) " : "", list_length,
                   (db.target_dir[0] == '\0') ? "home dir" : db.target_dir,
                   db.id.dir);
      }
   }

   /*
    * Remove all files from the remote_list structure that are not
    * in the current nlist buffer.
    */
   if ((fra->stupid_mode != YES) && (fra->remove == NO))
   {
      int    files_removed = 0,
             i;
      size_t move_size;

      for (i = 0; i < (no_of_listed_files - files_removed); i++)
      {
         if (i >= *current_no_of_listed_files)
         {
            trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
                      "no_of_listed_files has been reduced (%d -> %d)!",
                      no_of_listed_files, *current_no_of_listed_files);
            no_of_listed_files = *current_no_of_listed_files;
            break;
         }
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
                  (void)ftp_quit();
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
                  (void)ftp_quit();
                  exit(INCORRECT);
               }
               rl_size = new_size;
#ifdef DO_NOT_PARALLELIZE_ALL_FETCH
            }
#endif
            current_no_of_listed_files = (int *)ptr;
            ptr += AFD_WORD_OFFSET;
            rl = (struct retrieve_list *)ptr;
         }
         *(int *)((char *)rl - AFD_WORD_OFFSET) = no_of_listed_files;
      }
   }

   return;
}


/*+++++++++++++++++++++++++++++ check_list() ++++++++++++++++++++++++++++*/
static int
check_list(char         *file,
           int          *files_to_retrieve,
           time_t       *file_mtime,
           off_t        *file_size_to_retrieve,
           int          *more_files_in_list,
           unsigned int ftp_options)
{
   int i,
       status;

   /* Check if this file is in the list. */
   if ((fra->stupid_mode == YES) || (fra->remove == YES))
   {
      for (i = 0; i < no_of_listed_files; i++)
      {
         if (i >= *current_no_of_listed_files)
         {
            trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
                      "no_of_listed_files has been reduced (%d -> %d)!",
                      no_of_listed_files, *current_no_of_listed_files);
            no_of_listed_files = *current_no_of_listed_files;
            break;
         }
         if (CHECK_STRCMP(rl[i].file_name, file) == 0)
         {
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
               rl[i].prev_size = 0;

               /* Try to get remote date. */
               if ((check_date == YES) && (fra->ignore_file_time != 0))
               {
                  time_t file_mtime;

                  if ((status = ftp_date(file, &file_mtime)) == SUCCESS)
                  {
                     rl[i].special_flag |= RL_GOT_EXACT_DATE;
                     rl[i].got_date = YES;
                     rl[i].file_mtime = file_mtime;
                     if (fsa->debug > NORMAL_MODE)
                     {
                        trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                     "Date for %s is %ld.", file, file_mtime);
                     }
                  }
                  else if ((status == 500) || (status == 502))
                       {
                          if ((ftp_options & FTP_OPTION_MDTM) == 0)
                          {
                             check_date = NO;
                          }
                          rl[i].got_date = NO;
                          if (fsa->debug > NORMAL_MODE)
                          {
                             trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                          "Date command MDTM not supported [%d]",
                                          status);
                          }
                       }
                       else
                       {
                          trans_log((timeout_flag == ON) ? ERROR_SIGN : DEBUG_SIGN,
                                    __FILE__, __LINE__, NULL, msg_str,
                                    "Failed to get date of file %s.", file);
                          if (timeout_flag != OFF)
                          {
                             (void)ftp_quit();
                             exit(DATE_ERROR);
                          }
                          rl[i].got_date = NO;
                       }
               }
               else
               {
                  rl[i].got_date = NO;
               }

               if (check_size == YES)
               {
                  off_t size;

                  if ((status = ftp_size(file, &size)) == SUCCESS)
                  {
                     rl[i].special_flag |= RL_GOT_EXACT_SIZE;
                     rl[i].size = size;
                     if (fsa->debug > NORMAL_MODE)
                     {
                        trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                     "Size for %s is %d.", file, size);
                     }
                  }
                  else if ((status == 500) || (status == 502))
                       {
                          if ((ftp_options & FTP_OPTION_SIZE) == 0)
                          {
                             check_size = NO;
                          }
                          if (fsa->debug > NORMAL_MODE)
                          {
                             trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                          "Size command SIZE not supported [%d]",
                                          status);
                          }
                       }
                       else
                       {
                          trans_log((timeout_flag == ON) ? ERROR_SIGN : DEBUG_SIGN,
                                    __FILE__, __LINE__, NULL, msg_str,
                                    "Failed to get size of file %s.", file);
                          if (timeout_flag != OFF)
                          {
                             (void)ftp_quit();
                             exit(SIZE_ERROR);
                          }
                       }
               }

               if ((fra->ignore_size == -1) ||
                   ((fra->gt_lt_sign & ISIZE_EQUAL) &&
                    (fra->ignore_size != rl[i].size)) ||
                   ((fra->gt_lt_sign & ISIZE_LESS_THEN) &&
                    (fra->ignore_size < rl[i].size)) ||
                   ((fra->gt_lt_sign & ISIZE_GREATER_THEN) &&
                    (fra->ignore_size > rl[i].size)))
               {
                  if ((rl[i].got_date == NO) || (fra->ignore_file_time == 0))
                  {
                     *file_mtime = -1;
                     if (rl[i].size > 0)
                     {
                        *file_size_to_retrieve += rl[i].size;
                     }
                     *files_to_retrieve += 1;
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
                        if (((fra->dir_options & ONE_PROCESS_JUST_SCANNING) == 0) ||
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
                        *more_files_in_list = YES;
                        if (rl[i].size > 0)
                        {
                           *file_size_to_retrieve -= rl[i].size;
                        }
                        *files_to_retrieve -= 1;
                        rl[i].assigned = 0;
                     }
                     status = 0;
                  }
                  else
                  {
                     time_t diff_time;

                     *file_mtime = rl[i].file_mtime;
                     diff_time = current_time - rl[i].file_mtime;
                     if (((fra->gt_lt_sign & IFTIME_EQUAL) &&
                          (fra->ignore_file_time != diff_time)) ||
                         ((fra->gt_lt_sign & IFTIME_LESS_THEN) &&
                          (fra->ignore_file_time < diff_time)) ||
                         ((fra->gt_lt_sign & IFTIME_GREATER_THEN) &&
                          (fra->ignore_file_time > diff_time)))
                     {
                        *file_size_to_retrieve += rl[i].size;
                        *files_to_retrieve += 1;
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
                           if (((fra->dir_options & ONE_PROCESS_JUST_SCANNING) == 0) ||
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
                           *more_files_in_list = YES;
                           if (rl[i].size > 0)
                           {
                              *file_size_to_retrieve -= rl[i].size;
                           }
                           *files_to_retrieve -= 1;
                           rl[i].assigned = 0;
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
   else
   {
      for (i = 0; i < no_of_listed_files; i++)
      {
         if (i >= *current_no_of_listed_files)
         {
            trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
                      "no_of_listed_files has been reduced (%d -> %d)!",
                      no_of_listed_files, *current_no_of_listed_files);
            no_of_listed_files = *current_no_of_listed_files;
            break;
         }
         if (CHECK_STRCMP(rl[i].file_name, file) == 0)
         {
            rl[i].in_list = YES;
            if ((rl[i].assigned != 0) ||
                (((fra->stupid_mode == GET_ONCE_ONLY) ||
                  (fra->stupid_mode == GET_ONCE_NOT_EXACT)) &&
                 ((rl[i].special_flag & RL_GOT_SIZE_DATE) ||
                  (rl[i].retrieved == YES))))
            {
               if ((rl[i].retrieved == NO) && (rl[i].assigned == 0))
               {
                  if (((fra->dir_options & ONE_PROCESS_JUST_SCANNING) == 0) ||
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
               off_t prev_size = 0;

               /* Try to get remote date. */
               if ((check_date == YES) && (get_date == YES))
               {
                  time_t file_mtime;

                  if ((status = ftp_date(file, &file_mtime)) == SUCCESS)
                  {
                     rl[i].got_date = YES;
                     if (rl[i].file_mtime != file_mtime)
                     {
                        rl[i].special_flag |= RL_GOT_EXACT_DATE;
                        rl[i].file_mtime = file_mtime;
                        rl[i].retrieved = NO;
                        rl[i].assigned = 0;
                     }
                     if (fsa->debug > NORMAL_MODE)
                     {
                        trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                     "Date for %s is %ld.", file, file_mtime);
                     }
                  }
                  else if ((status == 500) || (status == 502) ||
                           (status == 550))
                       {
                          if ((ftp_options & FTP_OPTION_MDTM) == 0)
                          {
                             check_date = NO;
                          }
                          rl[i].got_date = NO;
                          if (fsa->debug > NORMAL_MODE)
                          {
                             trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                          "Date command MDTM not supported [%d]",
                                          status);
                          }
                       }
                       else
                       {
                          trans_log((timeout_flag == ON) ? ERROR_SIGN : DEBUG_SIGN,
                                    __FILE__, __LINE__, NULL, msg_str,
                                    "Failed to get date of file %s.", file);
                          if (timeout_flag != OFF)
                          {
                             (void)ftp_quit();
                             exit(DATE_ERROR);
                          }
                          rl[i].got_date = NO;
                       }
               }
               else
               {
                  rl[i].got_date = NO;
               }

               /* Try to get remote size. */
               if ((check_size == YES) &&
                   (((fra->stupid_mode != GET_ONCE_ONLY) &&
                     (fra->stupid_mode != GET_ONCE_NOT_EXACT)) ||
                    (rl[i].size == -1)))
               {
                  off_t size;

                  if ((status = ftp_size(file, &size)) == SUCCESS)
                  {
                     if (rl[i].size != size)
                     {
                        rl[i].special_flag |= RL_GOT_EXACT_SIZE;
                        prev_size = rl[i].size;
                        rl[i].size = size;
                        rl[i].retrieved = NO;
                        rl[i].assigned = 0;
                     }
                     if (fsa->debug > NORMAL_MODE)
                     {
                        trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                     "Size for %s is %d.", file, size);
                     }
                  }
                  else if ((status == 500) || (status == 502))
                       {
                          if ((ftp_options & FTP_OPTION_SIZE) == 0)
                          {
                             check_size = NO;
                          }
                          if (fsa->debug > NORMAL_MODE)
                          {
                             trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                          "Size command SIZE not supported [%d]",
                                          status);
                          }
                       }
                       else
                       {
                          trans_log((timeout_flag == ON) ? ERROR_SIGN : DEBUG_SIGN,
                                    __FILE__, __LINE__, NULL, msg_str,
                                    "Failed to get size of file %s.", file);
                          if (timeout_flag != OFF)
                          {
                             (void)ftp_quit();
                             exit(SIZE_ERROR);
                          }
                          if ((ftp_options & FTP_OPTION_SIZE) == 0)
                          {
                             check_size = NO;
                          }
                       }
               }

               if (rl[i].retrieved == NO)
               {
                  if ((fra->ignore_size == -1) ||
                      ((fra->gt_lt_sign & ISIZE_EQUAL) &&
                       (fra->ignore_size != rl[i].size)) ||
                      ((fra->gt_lt_sign & ISIZE_LESS_THEN) &&
                       (fra->ignore_size < rl[i].size)) ||
                      ((fra->gt_lt_sign & ISIZE_GREATER_THEN) &&
                       (fra->ignore_size > rl[i].size)))
                  {
                     off_t size_to_retrieve;

                     if ((rl[i].got_date == NO) ||
                         (fra->ignore_file_time == 0))
                     {
                        *file_mtime = -1;
                        if (rl[i].size > 0)
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
                        else
                        {
                           size_to_retrieve = 0;
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
                           if (((fra->dir_options & ONE_PROCESS_JUST_SCANNING) == 0) ||
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
                           *more_files_in_list = YES;
                           rl[i].assigned = 0;
                        }
                        status = 0;
                     }
                     else
                     {
                        time_t diff_time;

                        *file_mtime = rl[i].file_mtime;
                        diff_time = current_time - rl[i].file_mtime;
                        if (((fra->gt_lt_sign & IFTIME_EQUAL) &&
                             (fra->ignore_file_time != diff_time)) ||
                            ((fra->gt_lt_sign & IFTIME_LESS_THEN) &&
                             (fra->ignore_file_time < diff_time)) ||
                            ((fra->gt_lt_sign & IFTIME_GREATER_THEN) &&
                             (fra->ignore_file_time > diff_time)))
                        {
                           if (rl[i].size > 0)
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
                           else
                           {
                              size_to_retrieve = 0;
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
                              if (((fra->dir_options & ONE_PROCESS_JUST_SCANNING) == 0) ||
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
                              *more_files_in_list = YES;
                              rl[i].assigned = 0;
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
                  unlock_region(rl_fd, (off_t)(LOCK_RETR_FILE + i), __FILE__, __LINE__);
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
            (void)ftp_quit();
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
            (void)ftp_quit();
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
   rl[no_of_listed_files].retrieved = NO;
   rl[no_of_listed_files].in_list = YES;
   rl[no_of_listed_files].special_flag = 0;

   if ((check_date == YES) && (get_date == YES))
   {
      if ((fra->stupid_mode == YES) || (fra->remove == YES))
      {
         if (fra->ignore_file_time != 0)
         {
            time_t file_mtime;

            if ((status = ftp_date(file, &file_mtime)) == SUCCESS)
            {
               rl[no_of_listed_files].special_flag |= RL_GOT_EXACT_DATE;
               rl[no_of_listed_files].file_mtime = file_mtime;
               rl[no_of_listed_files].got_date = YES;
               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                               "Date for %s is %ld.", file, file_mtime);
               }
            }
            else if ((status == 500) || (status == 502))
                 {
                    if ((ftp_options & FTP_OPTION_MDTM) == 0)
                    {
                       check_date = NO;
                    }
                    rl[no_of_listed_files].got_date = NO;
                    if (fsa->debug > NORMAL_MODE)
                    {
                       trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                    "Date command MDTM not supported [%d]",
                                    status);
                    }
                 }
                 else
                 {
                    trans_log((timeout_flag == ON) ? ERROR_SIGN : DEBUG_SIGN,
                              __FILE__, __LINE__, NULL, msg_str,
                              "Failed to get date of file %s.", file);
                    if (timeout_flag != OFF)
                    {
                       (void)ftp_quit();
                       exit(DATE_ERROR);
                    }
                    rl[no_of_listed_files].got_date = NO;
                 }
         }
         else
         {
            rl[no_of_listed_files].got_date = NO;
         }
      }
      else
      {
         time_t file_mtime;

         if ((status = ftp_date(file, &file_mtime)) == SUCCESS)
         {
            rl[no_of_listed_files].special_flag |= RL_GOT_EXACT_DATE;
            rl[no_of_listed_files].file_mtime = file_mtime;
            rl[no_of_listed_files].got_date = YES;
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                            "Date for %s is %ld.", file, file_mtime);
            }
         }
         else if ((status == 500) || (status == 502))
              {
                 if ((ftp_options & FTP_OPTION_MDTM) == 0)
                 {
                    check_date = NO;
                 }
                 rl[no_of_listed_files].got_date = NO;
                 if (fsa->debug > NORMAL_MODE)
                 {
                    trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                 "Date command MDTM not supported [%d]",
                                 status);
                 }
              }
              else
              {
                 trans_log((timeout_flag == ON) ? ERROR_SIGN : DEBUG_SIGN,
                           __FILE__, __LINE__, NULL, msg_str,
                           "Failed to get date of file %s.", file);
                 if (timeout_flag != OFF)
                 {
                    (void)ftp_quit();
                    exit(DATE_ERROR);
                 }
                 rl[no_of_listed_files].got_date = NO;
              }
      }
   }
   else
   {
      rl[no_of_listed_files].got_date = NO;
   }

   if (check_size == YES)
   {
      off_t size;

      if ((status = ftp_size(file, &size)) == SUCCESS)
      {
         rl[no_of_listed_files].size = size;
         rl[no_of_listed_files].special_flag |= RL_GOT_EXACT_SIZE;
         *file_size_to_retrieve += size;
         *files_to_retrieve += 1;
         if (fsa->debug > NORMAL_MODE)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                         "Size for %s is %d.", file, size);
         }
      }
      else if ((status == 500) || (status == 502))
           {
              if ((ftp_options & FTP_OPTION_SIZE) == 0)
              {
                 check_size = NO;
              }
              rl[no_of_listed_files].size = -1;
              if (fsa->debug > NORMAL_MODE)
              {
                 trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                              "Size command SIZE not supported [%d]", status);
              }
           }
           else
           {
              trans_log((timeout_flag == ON) ? ERROR_SIGN : DEBUG_SIGN,
                        __FILE__, __LINE__, NULL, msg_str,
                        "Failed to get size of file %s.", file);
              if (timeout_flag != OFF)
              {
                 (void)ftp_quit();
                 exit(DATE_ERROR);
              }
              rl[no_of_listed_files].size = -1;
           }
   }
   else
   {
      rl[no_of_listed_files].size = -1;
   }
   rl[no_of_listed_files].prev_size = 0;

   if (rl[no_of_listed_files].got_date == NO)
   {
      *file_mtime = -1;
   }
   else
   {
      *file_mtime = rl[no_of_listed_files].file_mtime;
   }
   if ((*file_mtime != -1) && (rl[no_of_listed_files].size != -1))
   {
      rl[no_of_listed_files].special_flag |= RL_GOT_SIZE_DATE;
   }
   if ((fra->ignore_size == -1) ||
       ((fra->gt_lt_sign & ISIZE_EQUAL) &&
        (fra->ignore_size != rl[no_of_listed_files].size)) ||
       ((fra->gt_lt_sign & ISIZE_LESS_THEN) &&
        (fra->ignore_size < rl[no_of_listed_files].size)) ||
       ((fra->gt_lt_sign & ISIZE_GREATER_THEN) &&
        (fra->ignore_size > rl[no_of_listed_files].size)))
   {
      if ((rl[no_of_listed_files].got_date == NO) ||
          (fra->ignore_file_time == 0))
      {
         no_of_listed_files++;
      }
      else
      {
         time_t diff_time;

         diff_time = current_time - rl[no_of_listed_files].file_mtime;
         if (((fra->gt_lt_sign & IFTIME_EQUAL) &&
              (fra->ignore_file_time != diff_time)) ||
             ((fra->gt_lt_sign & IFTIME_LESS_THEN) &&
              (fra->ignore_file_time < diff_time)) ||
             ((fra->gt_lt_sign & IFTIME_GREATER_THEN) &&
              (fra->ignore_file_time > diff_time)))
         {
            no_of_listed_files++;
         }
         else
         {
            if (rl[no_of_listed_files].size > 0)
            {
               *file_size_to_retrieve -= rl[no_of_listed_files].size;
            }
            *files_to_retrieve -= 1;
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
         if (((fra->dir_options & ONE_PROCESS_JUST_SCANNING) == 0) ||
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
         if (rl[no_of_listed_files - 1].size > 0)
         {
            *file_size_to_retrieve -= rl[no_of_listed_files - 1].size;
         }
         *files_to_retrieve -= 1;
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
      if (rl[no_of_listed_files].size > 0)
      {
         *file_size_to_retrieve -= rl[no_of_listed_files].size;
      }
      *files_to_retrieve -= 1;
      return(1);
   }
}
