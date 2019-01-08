/*
 *  get_remote_file_names_sftp.c - Part of AFD, an automatic file distribution
 *                                 program.
 *  Copyright (c) 2006 - 2019 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_remote_file_names_sftp - retrieves filename, size and date
 **
 ** SYNOPSIS
 **   int get_remote_file_names_sftp(off_t *file_size_to_retrieve,
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
 **   01.05.2006 H.Kiehl Created
 **   03.09.2017 H.Kiehl Added option to get only appended part.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                /* strcpy(), strerror(), memmove()    */
#include <stdlib.h>                /* malloc(), realloc(), free()        */
#include <time.h>                  /* time(), mktime(), strftime()       */ 
#ifdef TM_IN_SYS_TIME
# include <sys/time.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>                /* unlink()                           */
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include "sftpdefs.h"
#include "fddefs.h"


/* External global variables. */
extern int                        exitflag,
                                  *no_of_listed_files,
                                  rl_fd,
                                  timeout_flag;
extern char                       msg_str[],
                                  *p_work_dir;
extern struct job                 db;
extern struct retrieve_list       *rl;
extern struct fileretrieve_status *fra;
extern struct filetransfer_status *fsa;

/* Local global variables. */
static time_t                     current_time;

/* Local function prototypes. */
static int                        check_list(char *, struct stat *, int *,
                                  off_t *, int *);


/*#################### get_remote_file_names_sftp() #####################*/
int
get_remote_file_names_sftp(off_t *file_size_to_retrieve,
                           int   *more_files_in_list)
{
   int files_to_retrieve = 0,
       i;

   *file_size_to_retrieve = 0;
   if ((*more_files_in_list == YES) ||
       (db.special_flag & DISTRIBUTED_HELPER_JOB) ||
       ((db.special_flag & OLD_ERROR_JOB) && (db.retries < 30) &&
        (fra[db.fra_pos].stupid_mode != YES) &&
        (fra[db.fra_pos].remove != YES)))
   {
      if (rl_fd == -1)
      {
         if (attach_ls_data(db.fra_pos, db.fsa_pos, db.special_flag, YES) == INCORRECT)
         {
            sftp_quit();
            exit(INCORRECT);
         }
      }
      *more_files_in_list = NO;
      for (i = 0; i < *no_of_listed_files; i++)
      {
         if ((rl[i].retrieved == NO) && (rl[i].assigned == 0))
         {
            if ((fra[db.fra_pos].stupid_mode == YES) ||
                (fra[db.fra_pos].remove == YES) ||
                ((files_to_retrieve < fra[db.fra_pos].max_copied_files) &&
                 (*file_size_to_retrieve < fra[db.fra_pos].max_copied_file_size)))
            {
               /* Lock this file in list. */
#ifdef LOCK_DEBUG
               if (lock_region(rl_fd, (off_t)i, __FILE__, __LINE__) == LOCK_IS_NOT_SET)
#else
               if (lock_region(rl_fd, (off_t)i) == LOCK_IS_NOT_SET)
#endif
               {
                  if ((fra[db.fra_pos].ignore_size == -1) ||
                      ((fra[db.fra_pos].gt_lt_sign & ISIZE_EQUAL) &&
                       (fra[db.fra_pos].ignore_size == rl[i].size)) ||
                      ((fra[db.fra_pos].gt_lt_sign & ISIZE_LESS_THEN) &&
                       (fra[db.fra_pos].ignore_size < rl[i].size)) ||
                      ((fra[db.fra_pos].gt_lt_sign & ISIZE_GREATER_THEN) &&
                       (fra[db.fra_pos].ignore_size > rl[i].size)))
                  {
                     if ((rl[i].got_date == NO) ||
                         (fra[db.fra_pos].ignore_file_time == 0))
                     {
                        files_to_retrieve++;
                        if ((fra[db.fra_pos].stupid_mode == APPEND_ONLY) &&
                            (rl[i].size > rl[i].prev_size))
                        {
                           *file_size_to_retrieve += (rl[i].size - rl[i].prev_size);
                        }
                        else
                        {
                           *file_size_to_retrieve += rl[i].size;
                        }
                        rl[i].assigned = (unsigned char)db.job_no + 1;
                     }
                     else
                     {
                        time_t diff_time;

                        diff_time = current_time - rl[i].file_mtime;
                        if (((fra[db.fra_pos].gt_lt_sign & IFTIME_EQUAL) &&
                             (fra[db.fra_pos].ignore_file_time == diff_time)) ||
                            ((fra[db.fra_pos].gt_lt_sign & IFTIME_LESS_THEN) &&
                             (fra[db.fra_pos].ignore_file_time < diff_time)) ||
                            ((fra[db.fra_pos].gt_lt_sign & IFTIME_GREATER_THEN) &&
                             (fra[db.fra_pos].ignore_file_time > diff_time)))
                        {
                           files_to_retrieve++;
                           if ((fra[db.fra_pos].stupid_mode == APPEND_ONLY) &&
                               (rl[i].size > rl[i].prev_size))
                           {
                              *file_size_to_retrieve += (rl[i].size - rl[i].prev_size);
                           }
                           else
                           {
                              *file_size_to_retrieve += rl[i].size;
                           }
                           rl[i].assigned = (unsigned char)db.job_no + 1;
                        }
                     }
                  }
#ifdef LOCK_DEBUG
                  unlock_region(rl_fd, (off_t)i, __FILE__, __LINE__);
#else
                  unlock_region(rl_fd, (off_t)i);
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
      unsigned int     files_deleted = 0,
                       list_length = 0;
      int              gotcha,
                       j,
                       nfg,           /* Number of file mask. */
                       status;
      char             filename[MAX_FILENAME_LENGTH],
                       *p_mask;
      off_t            file_size_deleted = 0,
                       list_size = 0;
      struct file_mask *fml = NULL;
      struct tm        *p_tm;
      struct stat      stat_buf;

      /* Get all file masks for this directory. */
      if ((j = read_file_mask(fra[db.fra_pos].dir_alias, &nfg, &fml)) == INCORRECT)
      {
         if (j == LOCKFILE_NOT_THERE)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to set lock in file masks, because the file is not there.");
         }
         else if (j == LOCK_IS_SET)
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to get the file masks, because lock is already set");
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to get the file masks. (%d)", j);
              }
         if (fml != NULL)
         {
            free(fml);
         }
         sftp_quit();
         exit(INCORRECT);
      }

      if ((fra[db.fra_pos].stupid_mode == YES) ||
          (fra[db.fra_pos].remove == YES))
      {
         if (reset_ls_data(db.fra_pos) == INCORRECT)
         {
            sftp_quit();
            exit(INCORRECT);
         }
      }
      else
      {
         if (rl_fd == -1)
         {
            if (attach_ls_data(db.fra_pos, db.fsa_pos, db.special_flag, YES) == INCORRECT)
            {
               sftp_quit();
               exit(INCORRECT);
            }
         }
      }

      if ((fra[db.fra_pos].ignore_file_time != 0) ||
          (fra[db.fra_pos].delete_files_flag & UNKNOWN_FILES))
      {
         /* Note: FTP returns GMT so we need to convert this to GMT! */
         current_time = time(NULL);
         p_tm = gmtime(&current_time);
         current_time = mktime(p_tm);
      }

      /*
       * Get a directory listing from the remote site so we can see
       * what files are there.
       */
      if ((status = sftp_open_dir("")) == SUCCESS)
      {
         if (fsa->debug > NORMAL_MODE)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                         "Opened remote directory.");
         }
         while ((status = sftp_readdir(filename, &stat_buf)) == SUCCESS)
         {
            if ((((filename[0] == '.') && (filename[1] == '\0')) ||
                 ((filename[0] == '.') && (filename[1] == '.') &&
                  (filename[2] == '\0'))) ||
                (((fra[db.fra_pos].dir_flag & ACCEPT_DOT_FILES) == 0) &&
                 (filename[0] == '.')) ||
                (S_ISREG(stat_buf.st_mode) == 0))
            {
               continue;
            }
            else
            {
               int namelen = strlen(filename);

               list_length++;
               list_size += stat_buf.st_size;
               if (namelen >= (MAX_FILENAME_LENGTH - 1))
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Remote file name `%s' is to long (%d), it may only be %d bytes long.",
                            filename, namelen, (MAX_FILENAME_LENGTH - 1));
                  continue;
               }
               if (fsa->debug > NORMAL_MODE)
               {
                  time_t mtime;
                  char   dstr[26],
                         mstr[11];

                  mtime = stat_buf.st_mtime;
                  p_tm = gmtime(&mtime);
                  (void)strftime(dstr, 26, "%a %h %d %H:%M:%S %Y", p_tm);
                  mode_t2str(stat_buf.st_mode, mstr);
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
#if SIZEOF_OFF_T == 4
                               "%s %s size=%ld uid=%u gid=%u mode=%o %s",
#else
                               "%s %s size=%lld uid=%u gid=%u mode=%o %s",
#endif
                               mstr, dstr, (pri_off_t)stat_buf.st_size,
                               (stat_buf.st_mode & ~S_IFMT),
                               (unsigned int)stat_buf.st_uid,
                               (unsigned int)stat_buf.st_gid, filename);
               }

               if (fra[db.fra_pos].dir_flag == ALL_DISABLED)
               {
                  delete_remote_file(SFTP, filename, namelen,
#ifdef _DELETE_LOG
                                     DELETE_HOST_DISABLED,
#endif
                                     &files_deleted, &file_size_deleted,
                                     stat_buf.st_size);
               }
               else
               {
                  gotcha = NO;
                  for (i = 0; i < nfg; i++)
                  {
                     p_mask = fml[i].file_list;
                     for (j = 0; j < fml[i].fc; j++)
                     {
                        if (((status = pmatch(p_mask, filename, NULL)) == 0) &&
                            (check_list(filename, &stat_buf, &files_to_retrieve,
                                        file_size_to_retrieve,
                                        more_files_in_list) == 0))
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
                      (fra[db.fra_pos].delete_files_flag & UNKNOWN_FILES))
                  {
                     time_t diff_time = current_time - stat_buf.st_mtime;

                     if ((fra[db.fra_pos].unknown_file_time == -2) ||
                         ((diff_time > fra[db.fra_pos].unknown_file_time) &&
                          (diff_time > DEFAULT_TRANSFER_TIMEOUT)))
                     {
                        delete_remote_file(SFTP, filename, namelen,
#ifdef _DELETE_LOG
                                           DEL_UNKNOWN_FILE,
#endif
                                           &files_deleted, &file_size_deleted,
                                           stat_buf.st_size);
                     }
                  }
               }
            }
         }
         if (status == INCORRECT)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                      "Failed to read remote directory.");
            sftp_quit();
            exit(LIST_ERROR);
         }
         if (sftp_close_dir() == INCORRECT)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                      "Failed to close remote directory.");
         }
         else
         {
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                            "Closed remote directory.");
            }
         }
      }
      else
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   "Failed to open remote directory for reading.");
         sftp_quit();
         exit(LIST_ERROR);
      }

      /* Free file mask list. */
      for (i = 0; i < nfg; i++)
      {
         free(fml[i].file_list);
      }
      free(fml);

      if (files_deleted > 0)
      {
         trans_log(DEBUG_SIGN, NULL, 0, NULL, NULL,
#if SIZEOF_OFF_T == 4
                   "%d files %ld bytes found for retrieving [%u files with %ld bytes in %s (deleted %u files with %ld bytes)]. @%x",
#else
                   "%d files %lld bytes found for retrieving [%u files with %lld bytes in %s (deleted %u files with %lld bytes)]. @%x",
#endif
                   files_to_retrieve, (pri_off_t)(*file_size_to_retrieve),
                   list_length, (pri_off_t)list_size,
                   (db.target_dir[0] == '\0') ? "home dir" : db.target_dir,
                   files_deleted, (pri_off_t)file_size_deleted, db.id.dir);
      }
      else
      {
         trans_log(DEBUG_SIGN, NULL, 0, NULL, NULL,
#if SIZEOF_OFF_T == 4
                   "%d files %ld bytes found for retrieving [%u files with %ld bytes in %s]. @%x",
#else
                   "%d files %lld bytes found for retrieving [%u files with %lld bytes in %s]. @%x",
#endif
                   files_to_retrieve, (pri_off_t)(*file_size_to_retrieve),
                   list_length, (pri_off_t)list_size,
                   (db.target_dir[0] == '\0') ? "home dir" : db.target_dir,
                   db.id.dir);
      }

      /*
       * Remove all files from the remote_list structure that are not
       * in the current buffer.
       */
      if ((files_to_retrieve > 0) && (fra[db.fra_pos].stupid_mode != YES) &&
          (fra[db.fra_pos].remove == NO))
      {
         int    files_removed = 0,
                i;
         size_t move_size;

         for (i = 0; i < (*no_of_listed_files - files_removed); i++)
         {
            if (rl[i].in_list == NO)
            {
               int j = i;

               while ((rl[j].in_list == NO) &&
                      (j < (*no_of_listed_files - files_removed)))
               {
                  j++;
               }
               if (j != (*no_of_listed_files - files_removed))
               {
                  move_size = (*no_of_listed_files - files_removed - j) *
                              sizeof(struct retrieve_list);
                  (void)memmove(&rl[i], &rl[j], move_size);
               }
               files_removed += (j - i);
            }
         }

         if (files_removed > 0)
         {
            int    current_no_of_listed_files = *no_of_listed_files;
            size_t new_size,
                   old_size;

            *no_of_listed_files -= files_removed;
            if (*no_of_listed_files < 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Hmmm, no_of_listed_files = %d", *no_of_listed_files);
               *no_of_listed_files = 0;
            }
            if (*no_of_listed_files == 0)
            {
               new_size = (RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                          AFD_WORD_OFFSET;
            }
            else
            {
               new_size = (((*no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                           RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                          AFD_WORD_OFFSET;
            }
            old_size = (((current_no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                        RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                       AFD_WORD_OFFSET;

            if (old_size != new_size)
            {
               char   *ptr;

               ptr = (char *)rl - AFD_WORD_OFFSET;
               if ((fra[db.fra_pos].stupid_mode == YES) ||
                   (fra[db.fra_pos].remove == YES))
               {
                  if ((ptr = realloc(ptr, new_size)) == NULL)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "realloc() error : %s", strerror(errno));
                     sftp_quit();
                     exit(INCORRECT);
                  }
               }
               else
               {
                  if ((ptr = mmap_resize(rl_fd, ptr, new_size)) == (caddr_t) -1)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "mmap_resize() error : %s", strerror(errno));
                     sftp_quit();
                     exit(INCORRECT);
                  }
               }
               no_of_listed_files = (int *)ptr;
               ptr += AFD_WORD_OFFSET;
               rl = (struct retrieve_list *)ptr;
               if (*no_of_listed_files < 0)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Hmmm, no_of_listed_files = %d", *no_of_listed_files);
                  *no_of_listed_files = 0;
               }
            }
         }
      }
   }

   return(files_to_retrieve);
}


/*+++++++++++++++++++++++++++++ check_list() ++++++++++++++++++++++++++++*/
static int
check_list(char        *file,
           struct stat *p_stat_buf,
           int         *files_to_retrieve,
           off_t       *file_size_to_retrieve,
           int         *more_files_in_list)
{
   int i;

   if ((fra[db.fra_pos].stupid_mode == YES) ||
       (fra[db.fra_pos].remove == YES))
   {
      for (i = 0; i < *no_of_listed_files; i++)
      {
         if (CHECK_STRCMP(rl[i].file_name, file) == 0)
         {
            rl[i].in_list = YES;
            if (((rl[i].assigned == 0) || (rl[i].retrieved == YES)) &&
                (((db.special_flag & OLD_ERROR_JOB) == 0) ||
#ifdef LOCK_DEBUG
                   (lock_region(rl_fd, (off_t)i, __FILE__, __LINE__) == LOCK_IS_NOT_SET)
#else
                   (lock_region(rl_fd, (off_t)i) == LOCK_IS_NOT_SET)
#endif
                  ))
            {
               int ret;

               rl[i].file_mtime = p_stat_buf->st_mtime;
               rl[i].got_date = YES;
               rl[i].size = p_stat_buf->st_size;
               rl[i].prev_size = 0;

               if ((fra[db.fra_pos].ignore_size == -1) ||
                   ((fra[db.fra_pos].gt_lt_sign & ISIZE_EQUAL) &&
                    (fra[db.fra_pos].ignore_size == rl[i].size)) ||
                   ((fra[db.fra_pos].gt_lt_sign & ISIZE_LESS_THEN) &&
                    (fra[db.fra_pos].ignore_size < rl[i].size)) ||
                   ((fra[db.fra_pos].gt_lt_sign & ISIZE_GREATER_THEN) &&
                    (fra[db.fra_pos].ignore_size > rl[i].size)))
               {
                  if (fra[db.fra_pos].ignore_file_time == 0)
                  {
                     *file_size_to_retrieve += rl[i].size;
                     *files_to_retrieve += 1;
                     if ((fra[db.fra_pos].stupid_mode == YES) ||
                         (fra[db.fra_pos].remove == YES) ||
                         ((*files_to_retrieve < fra[db.fra_pos].max_copied_files) &&
                          (*file_size_to_retrieve < fra[db.fra_pos].max_copied_file_size)))
                     {
                        rl[i].retrieved = NO;
                        rl[i].assigned = (unsigned char)db.job_no + 1;
                     }
                     else
                     {
                        rl[i].assigned = 0;
                        *file_size_to_retrieve -= rl[i].size;
                        *files_to_retrieve -= 1;
                        *more_files_in_list = YES;
                     }
                     ret = 0;
                  }
                  else
                  {
                     time_t diff_time;

                     diff_time = current_time - rl[i].file_mtime;
                     if (((fra[db.fra_pos].gt_lt_sign & IFTIME_EQUAL) &&
                          (fra[db.fra_pos].ignore_file_time == diff_time)) ||
                         ((fra[db.fra_pos].gt_lt_sign & IFTIME_LESS_THEN) &&
                          (fra[db.fra_pos].ignore_file_time < diff_time)) ||
                         ((fra[db.fra_pos].gt_lt_sign & IFTIME_GREATER_THEN) &&
                          (fra[db.fra_pos].ignore_file_time > diff_time)))
                     {
                        *file_size_to_retrieve += rl[i].size;
                        *files_to_retrieve += 1;
                        if ((fra[db.fra_pos].stupid_mode == YES) ||
                            (fra[db.fra_pos].remove == YES) ||
                            ((*files_to_retrieve < fra[db.fra_pos].max_copied_files) &&
                             (*file_size_to_retrieve < fra[db.fra_pos].max_copied_file_size)))
                        {
                           rl[i].retrieved = NO;
                           rl[i].assigned = (unsigned char)db.job_no + 1;
                        }
                        else
                        {
                           rl[i].assigned = 0;
                           *file_size_to_retrieve -= rl[i].size;
                           *files_to_retrieve -= 1;
                           *more_files_in_list = YES;
                        }
                        ret = 0;
                     }
                     else
                     {
                        ret = 1;
                     }
                  }
               }
               else
               {
                  ret = 1;
               }
               if (db.special_flag & OLD_ERROR_JOB)
               {
#ifdef LOCK_DEBUG
                  unlock_region(rl_fd, (off_t)i, __FILE__, __LINE__);
#else
                  unlock_region(rl_fd, (off_t)i);
#endif
               }
               return(ret);
            }
            else
            {
               return(1);
            }
         }
      } /* for (i = 0; i < *no_of_listed_files; i++) */
   }
   else
   {
      /* Check if this file is in the list. */
      for (i = 0; i < *no_of_listed_files; i++)
      {
         if (CHECK_STRCMP(rl[i].file_name, file) == 0)
         {
            rl[i].in_list = YES;
            if ((rl[i].assigned != 0) ||
                ((fra[db.fra_pos].stupid_mode == GET_ONCE_ONLY) &&
                 (rl[i].retrieved == YES)))
            {
               return(1);
            }

            if (((db.special_flag & OLD_ERROR_JOB) == 0) ||
#ifdef LOCK_DEBUG
                (lock_region(rl_fd, (off_t)i, __FILE__, __LINE__) == LOCK_IS_NOT_SET)
#else
                (lock_region(rl_fd, (off_t)i) == LOCK_IS_NOT_SET)
#endif
               )
            {
               int   ret;
               off_t prev_size = 0;

               if (rl[i].file_mtime != p_stat_buf->st_mtime)
               {
                  rl[i].file_mtime = p_stat_buf->st_mtime;
                  rl[i].retrieved = NO;
                  rl[i].assigned = 0;
               }
               rl[i].got_date = YES;
               if (rl[i].size != p_stat_buf->st_size)
               {
                  prev_size = rl[i].size;
                  rl[i].size = p_stat_buf->st_size;
                  rl[i].retrieved = NO;
                  rl[i].assigned = 0;
               }
               if (rl[i].retrieved == NO)
               {
                  if ((fra[db.fra_pos].ignore_size == -1) ||
                      ((fra[db.fra_pos].gt_lt_sign & ISIZE_EQUAL) &&
                       (fra[db.fra_pos].ignore_size == rl[i].size)) ||
                      ((fra[db.fra_pos].gt_lt_sign & ISIZE_LESS_THEN) &&
                       (fra[db.fra_pos].ignore_size < rl[i].size)) ||
                      ((fra[db.fra_pos].gt_lt_sign & ISIZE_GREATER_THEN) &&
                       (fra[db.fra_pos].ignore_size > rl[i].size)))
                  {
                     off_t size_to_retrieve;

                     if ((rl[i].got_date == NO) ||
                         (fra[db.fra_pos].ignore_file_time == 0))
                     {
                        if ((fra[db.fra_pos].stupid_mode == APPEND_ONLY) &&
                            (rl[i].size > prev_size))
                        {
                           size_to_retrieve = rl[i].size - prev_size;
                        }
                        else
                        {
                           size_to_retrieve = rl[i].size;
                        }
                        rl[i].prev_size = prev_size;
                        if ((fra[db.fra_pos].stupid_mode == YES) ||
                            (fra[db.fra_pos].remove == YES) ||
                            (((*files_to_retrieve + 1) < fra[db.fra_pos].max_copied_files) &&
                             ((*file_size_to_retrieve + size_to_retrieve) < fra[db.fra_pos].max_copied_file_size)))
                        {
                           rl[i].assigned = (unsigned char)db.job_no + 1;
                           *file_size_to_retrieve += size_to_retrieve;
                           *files_to_retrieve += 1;
                        }
                        else
                        {
                           rl[i].assigned = 0;
                           *more_files_in_list = YES;
                        }
                        ret = 0;
                     }
                     else
                     {
                        time_t diff_time;

                        diff_time = current_time - rl[i].file_mtime;
                        if (((fra[db.fra_pos].gt_lt_sign & IFTIME_EQUAL) &&
                             (fra[db.fra_pos].ignore_file_time == diff_time)) ||
                            ((fra[db.fra_pos].gt_lt_sign & IFTIME_LESS_THEN) &&
                             (fra[db.fra_pos].ignore_file_time < diff_time)) ||
                            ((fra[db.fra_pos].gt_lt_sign & IFTIME_GREATER_THEN) &&
                             (fra[db.fra_pos].ignore_file_time > diff_time)))
                        {
                           if ((fra[db.fra_pos].stupid_mode == APPEND_ONLY) &&
                               (rl[i].size > prev_size))
                           {
                              size_to_retrieve = rl[i].size - prev_size;
                           }
                           else
                           {
                              size_to_retrieve = rl[i].size;
                           }
                           rl[i].prev_size = prev_size;
                           if ((fra[db.fra_pos].stupid_mode == YES) ||
                               (fra[db.fra_pos].remove == YES) ||
                               (((*files_to_retrieve + 1) < fra[db.fra_pos].max_copied_files) &&
                                ((*file_size_to_retrieve  + size_to_retrieve) < fra[db.fra_pos].max_copied_file_size)))
                           {
                              rl[i].assigned = (unsigned char)db.job_no + 1;
                              *file_size_to_retrieve += size_to_retrieve;
                              *files_to_retrieve += 1;
                           }
                           else
                           {
                              rl[i].assigned = 0;
                              *more_files_in_list = YES;
                           }
                           ret = 0;
                        }
                        else
                        {
                           ret = 1;
                        }
                     }
                  }
                  else
                  {
                     ret = 1;
                  }
               }
               else
               {
                  ret = 1;
               }
               if (db.special_flag & OLD_ERROR_JOB)
               {
#ifdef LOCK_DEBUG
                  unlock_region(rl_fd, (off_t)i, __FILE__, __LINE__);
#else
                  unlock_region(rl_fd, (off_t)i);
#endif
               }
               return(ret);
            }
            else
            {
               return(1);
            }
         }
      } /* for (i = 0; i < *no_of_listed_files; i++) */
   }

   /* Add this file to the list. */
   if ((*no_of_listed_files != 0) &&
       ((*no_of_listed_files % RETRIEVE_LIST_STEP_SIZE) == 0))
   {
      char   *ptr;
      size_t new_size = (((*no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                         RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                         AFD_WORD_OFFSET;

      ptr = (char *)rl - AFD_WORD_OFFSET;
      if ((fra[db.fra_pos].stupid_mode == YES) ||
          (fra[db.fra_pos].remove == YES))
      {
         if ((ptr = realloc(ptr, new_size)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "realloc() error : %s", strerror(errno));
            sftp_quit();
            exit(INCORRECT);
         }
      }
      else
      {
         if ((ptr = mmap_resize(rl_fd, ptr, new_size)) == (caddr_t) -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "mmap_resize() error : %s", strerror(errno));
            sftp_quit();
            exit(INCORRECT);
         }
      }
      no_of_listed_files = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      rl = (struct retrieve_list *)ptr;
      if (*no_of_listed_files < 0)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Hmmm, no_of_listed_files = %d", *no_of_listed_files);
         *no_of_listed_files = 0;
      }
   }
   (void)strcpy(rl[*no_of_listed_files].file_name, file);
   rl[*no_of_listed_files].retrieved = NO;
   rl[*no_of_listed_files].in_list = YES;
   rl[*no_of_listed_files].size = p_stat_buf->st_size;
   rl[*no_of_listed_files].prev_size = 0;
   rl[*no_of_listed_files].file_mtime = p_stat_buf->st_mtime;
   rl[*no_of_listed_files].got_date = YES;

   if ((fra[db.fra_pos].ignore_size == -1) ||
       ((fra[db.fra_pos].gt_lt_sign & ISIZE_EQUAL) &&
        (fra[db.fra_pos].ignore_size == rl[*no_of_listed_files].size)) ||
       ((fra[db.fra_pos].gt_lt_sign & ISIZE_LESS_THEN) &&
        (fra[db.fra_pos].ignore_size < rl[*no_of_listed_files].size)) ||
       ((fra[db.fra_pos].gt_lt_sign & ISIZE_GREATER_THEN) &&
        (fra[db.fra_pos].ignore_size > rl[*no_of_listed_files].size)))
   {
      if ((rl[*no_of_listed_files].got_date == NO) ||
          (fra[db.fra_pos].ignore_file_time == 0))
      {
         *file_size_to_retrieve += p_stat_buf->st_size;
         *files_to_retrieve += 1;
         (*no_of_listed_files)++;
      }
      else
      {
         time_t diff_time;

         diff_time = current_time - rl[*no_of_listed_files].file_mtime;
         if (((fra[db.fra_pos].gt_lt_sign & IFTIME_EQUAL) &&
              (fra[db.fra_pos].ignore_file_time == diff_time)) ||
             ((fra[db.fra_pos].gt_lt_sign & IFTIME_LESS_THEN) &&
              (fra[db.fra_pos].ignore_file_time < diff_time)) ||
             ((fra[db.fra_pos].gt_lt_sign & IFTIME_GREATER_THEN) &&
                 (fra[db.fra_pos].ignore_file_time > diff_time)))
         {
            *file_size_to_retrieve += p_stat_buf->st_size;
            *files_to_retrieve += 1;
            (*no_of_listed_files)++;
         }
         else
         {
            return(1);
         }
      }
      if ((fra[db.fra_pos].stupid_mode == YES) ||
          (fra[db.fra_pos].remove == YES) ||
          ((*files_to_retrieve < fra[db.fra_pos].max_copied_files) &&
           (*file_size_to_retrieve < fra[db.fra_pos].max_copied_file_size)))
      {
         rl[(*no_of_listed_files) - 1].assigned = (unsigned char)db.job_no + 1;
      }
      else
      {
         rl[(*no_of_listed_files) - 1].assigned = 0;
         *file_size_to_retrieve -= p_stat_buf->st_size;
         *files_to_retrieve -= 1;
         *more_files_in_list = YES;
      }
      return(0);
   }
   else
   {
      return(1);
   }
}
