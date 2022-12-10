/*
 *  get_file_names.c - Part of AFD, an automatic file distribution program.
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

DESCR__S_M3
/*
 ** NAME
 **   get_file_names - gets the name of all files in a directory
 **
 ** SYNOPSIS
 **   int get_file_names(char *file_path, off_t *file_size_to_send)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   The number of files, total file size it has found in the directory
 **   and the directory where the files have been found. If all files
 **   are deleted due to age limit, it will return -1. Otherwise if
 **   an error occurs it will exit.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   31.12.1996 H.Kiehl Created
 **   31.01.1997 H.Kiehl Support for age limit.
 **   14.10.1998 H.Kiehl Free unused memory.
 **   03.02.2001 H.Kiehl Sort the files by date.
 **   21.08.2001 H.Kiehl Return -1 if all files have been deleted due
 **                      to age limit.
 **   20.12.2004 H.Kiehl Addition of delete file name buffer.
 **   14.02.2009 H.Kiehl Log to OUTPUT_LOG when we delete a file.
 **
 */
DESCR__E_M3

#include <stdio.h>                     /* NULL                           */
#include <string.h>                    /* strcpy(), strcat(), strcmp(),  */
                                       /* strerror()                     */
#include <stdlib.h>                    /* realloc(), malloc(), free()    */
#include <time.h>                      /* time()                         */
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>                    /* opendir(), closedir(), DIR,    */
                                       /* readdir(), struct dirent       */
#include <fcntl.h>
#include <unistd.h>                    /* unlink(), getpid()             */
#include <errno.h>
#include "fddefs.h"

/* Global variables. */
extern int                        counter_fd,
                                  files_to_delete,
                                  fsa_fd,
                                  *unique_counter;
#ifdef HAVE_HW_CRC32
extern int                        have_hw_crc32;
#endif
#ifdef _OUTPUT_LOG
extern int                        ol_fd;
# ifdef WITHOUT_FIFO_RW_SUPPORT
extern int                        ol_readfd;
# endif
extern unsigned int               *ol_job_number,
                                  *ol_retries;
extern char                       *ol_data,
                                  *ol_file_name,
                                  *ol_output_type;
extern unsigned short             *ol_archive_name_length,
                                  *ol_file_name_length,
                                  *ol_unl;
extern off_t                      *ol_file_size;
extern size_t                     ol_size,
                                  ol_real_size;
extern clock_t                    *ol_transfer_time;
#endif
extern off_t                      *file_size_buffer;
extern time_t                     *file_mtime_buffer;
extern char                       *p_work_dir,
                                  tr_hostname[],
                                  *del_file_name_buffer,
                                  *file_name_buffer;
extern struct filetransfer_status *fsa;
extern struct job                 db;
#ifdef WITH_DUP_CHECK
extern struct rule                *rule;
#endif
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif

#if defined (_DELETE_LOG) || defined (_OUTPUT_LOG)
/* Local function prototypes. */
static void                       log_data(char *,
#ifdef HAVE_STATX
                                           struct statx *,
#else
                                           struct stat *,
#endif
# ifdef WITH_DUP_CHECK
                                           int,
# endif
# ifdef _OUTPUT_LOG
                                           char,
# endif
                                           time_t);
#endif


/*########################## get_file_names() ###########################*/
int
get_file_names(char *file_path, off_t *file_size_to_send)
{
#ifdef WITH_DUP_CHECK
   int           dup_counter = 0;
   off_t         dup_counter_size = 0;
#endif
   time_t        diff_time,
                 now;
   int           files_not_send = 0,
                 files_to_send = 0,
                 new_size,
                 offset;
   off_t         file_size_not_send = 0,
                 *p_file_size;
   time_t        *p_file_mtime;
   char          fullname[MAX_PATH_LENGTH],
                 *p_del_file_name,
                 *p_file_name,
                 *p_source_file;
#ifdef HAVE_STATX
   struct statx  stat_buf;
#else
   struct stat   stat_buf;
#endif
   struct dirent *p_dir;
   DIR           *dp;

   /*
    * Create directory name in which we can find the files for this job.
    */
   (void)strcpy(file_path, p_work_dir);
   (void)strcat(file_path, AFD_FILE_DIR);
   (void)strcat(file_path, OUTGOING_DIR);
   (void)strcat(file_path, "/");
   (void)strcat(file_path, db.msg_name);
   db.p_unique_name = db.msg_name;
#ifdef MULTI_FS_SUPPORT
   while ((*db.p_unique_name != '/') && (*db.p_unique_name != '\0'))
   {
      db.p_unique_name++; /* Away with the filesystem ID. */
   }
   if (*db.p_unique_name == '/')
   {
      db.p_unique_name++;
#endif
      while ((*db.p_unique_name != '/') && (*db.p_unique_name != '\0'))
      {
         db.p_unique_name++; /* Away with the job ID. */
      }
      if (*db.p_unique_name == '/')
      {
         db.p_unique_name++;
         while ((*db.p_unique_name != '/') && (*db.p_unique_name != '\0'))
         {
            db.p_unique_name++; /* Away with the dir number. */
         }
         if (*db.p_unique_name == '/')
         {
            int  i;
            char str_num[MAX_TIME_T_HEX_LENGTH + 1];

            db.p_unique_name++;
            i = 0;
            while ((i < MAX_TIME_T_HEX_LENGTH) &&
                   (*(db.p_unique_name + i) != '_') &&
                   (*(db.p_unique_name + i) != '\0'))
            {
               str_num[i] = *(db.p_unique_name + i);
               i++;
            }
            if ((*(db.p_unique_name + i) != '_') || (i == 0))
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not determine message name from `%s'. #%x",
                          db.msg_name, db.id.job);
               exit(SYNTAX_ERROR);
            }
            str_num[i] = '\0';
            db.creation_time = (time_t)str2timet(str_num, NULL, 16);
            db.unl = i + 1;
            i = 0;
            while ((i < MAX_INT_HEX_LENGTH) &&
                   (*(db.p_unique_name + db.unl + i) != '_') &&
                   (*(db.p_unique_name + db.unl + i) != '\0'))
            {
               str_num[i] = *(db.p_unique_name + db.unl + i);
               i++;
            }
            if ((*(db.p_unique_name + db.unl + i) != '_') || (i == 0))
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not determine message name from `%s'. #%x",
                          db.msg_name, db.id.job);
               exit(SYNTAX_ERROR);
            }
            str_num[i] = '\0';
            db.unique_number = (unsigned int)strtoul(str_num, NULL, 16);
            db.unl = db.unl + i + 1;
            i = 0;
            while ((i < MAX_INT_HEX_LENGTH) &&
                   (*(db.p_unique_name + db.unl + i) != '\0'))
            {
               str_num[i] = *(db.p_unique_name + db.unl + i);
               i++;
            }
            if (i == 0)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not determine message name from `%s'. #%x",
                          db.msg_name, db.id.job);
               exit(SYNTAX_ERROR);
            }
            str_num[i] = '\0';
            db.split_job_counter = (unsigned int)strtoul(str_num, NULL, 16);
            db.unl = db.unl + i;
            if (db.unl == 0)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not determine message name from `%s'. #%x",
                          db.msg_name, db.id.job);
               exit(SYNTAX_ERROR);
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not determine message name from `%s'. #%x",
                       db.msg_name, db.id.job);
            exit(SYNTAX_ERROR);
         }
      }
      else
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not determine message name from `%s'. #%x",
                    db.msg_name, db.id.job);
         exit(SYNTAX_ERROR);
      }
#ifdef MULTI_FS_SUPPORT
   }
   else
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not determine message name from `%s'. #%x",
                 db.msg_name, db.id.job);
      exit(SYNTAX_ERROR);
   }
#endif

   if ((dp = opendir(file_path)) == NULL)
   {
      system_log((errno != ENOENT) ? ERROR_SIGN : WARN_SIGN, __FILE__, __LINE__,
                 "Could not opendir() %s [%s %d] : %s #%x",
                 file_path, db.host_alias, (int)db.job_no, strerror(errno),
                 db.id.job);
      exit(OPEN_FILE_DIR_ERROR);
   }

   /* Prepare pointers and directory name. */
   (void)strcpy(fullname, file_path);
   p_source_file = fullname + strlen(fullname);
   *p_source_file++ = '/';
   if (file_name_buffer != NULL)
   {
      free(file_name_buffer);
      file_name_buffer = NULL;
   }
   p_file_name = file_name_buffer;
   if (file_size_buffer != NULL)
   {
      free(file_size_buffer);
      file_size_buffer = NULL;
   }
   p_file_size = file_size_buffer;
   if (file_mtime_buffer != NULL)
   {
      free(file_mtime_buffer);
      file_mtime_buffer = NULL;
   }
   p_file_mtime = file_mtime_buffer;
   if (del_file_name_buffer != NULL)
   {
      free(del_file_name_buffer);
      del_file_name_buffer = NULL;
   }
   p_del_file_name = del_file_name_buffer;
   files_to_delete = 0;
   now = time(NULL);

   /*
    * Now let's determine the number of files that have to be
    * transmitted and the total size.
    */
   errno = 0;
   while ((p_dir = readdir(dp)) != NULL)
   {
      if (
#ifdef LINUX
          (p_dir->d_type != DT_REG) ||
#endif
          ((p_dir->d_name[0] == '.') && (p_dir->d_name[1] == '\0')) ||
          ((p_dir->d_name[0] == '.') && (p_dir->d_name[1] == '.') &&
          (p_dir->d_name[2] == '\0')))
      {
         continue;
      }
      (void)strcpy(p_source_file, p_dir->d_name);
#ifdef HAVE_STATX
      if (statx(0, fullname, AT_STATX_SYNC_AS_STAT,
# ifndef LINUX
                STATX_MODE |
# endif
                STATX_SIZE | STATX_MTIME, &stat_buf) == -1)
#else
      if (stat(fullname, &stat_buf) == -1)
#endif
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                    "Can't statx() file `%s' : %s #%x",
#else
                    "Can't stat() file `%s' : %s #%x",
#endif
                    fullname, strerror(errno), db.id.job);
         continue;
      }

#ifndef LINUX
      /* Sure it is a normal file? */
# ifdef HAVE_STATX
      if (S_ISREG(stat_buf.stx_mode))
# else
      if (S_ISREG(stat_buf.st_mode))
# endif
#endif
      {
#ifdef WITH_DUP_CHECK
         int is_duplicate = NO;
#endif
         int remove_file = NO;

#ifdef HAVE_STATX
         if (now < stat_buf.stx_mtime.tv_sec)
#else
         if (now < stat_buf.st_mtime)
#endif
         {
            diff_time = 0L;
         }
         else
         {
#ifdef HAVE_STATX
            diff_time = now - stat_buf.stx_mtime.tv_sec;
#else
            diff_time = now - stat_buf.st_mtime;
#endif
         }

         /* Don't send files older then age_limit! */
         if ((db.age_limit > 0) &&
             ((fsa->host_status & DO_NOT_DELETE_DATA) == 0) &&
             (diff_time > db.age_limit))
         {
            remove_file = YES;
         }
         else
         {
#ifdef WITH_DUP_CHECK
            if (((db.age_limit > 0) &&
                 ((fsa->host_status & DO_NOT_DELETE_DATA) == 0) &&
                 (diff_time > db.age_limit)) ||
                ((db.dup_check_timeout > 0) &&
# ifdef FAST_SF_DUPCHECK
                 ((db.special_flag & OLD_ERROR_JOB) == 0) &&
                 (((is_duplicate = isdup(fullname, p_dir->d_name,
#  ifdef HAVE_STATX
                                         stat_buf.stx_size,
#  else
                                         stat_buf.st_size,
#  endif
                                         db.crc_id,
                                         db.dup_check_timeout,
                                         db.dup_check_flag,
                                         NO,
#  ifdef HAVE_HW_CRC32
                                         have_hw_crc32,
#  endif
                                         YES, YES)) == YES) &&
                  ((db.dup_check_flag & DC_DELETE) ||
                   (db.dup_check_flag & DC_STORE)))
# else
                 (((is_duplicate = isdup(fullname, p_dir->d_name,
#  ifdef HAVE_STATX
                                         stat_buf.stx_size,
#  else
                                         stat_buf.st_size,
#  endif
                                         db.crc_id,
                                         0,
                                         db.dup_check_flag,
                                         YES,
#  ifdef HAVE_HW_CRC32
                                         have_hw_crc32,
#  endif
                                         YES, YES)) == YES) &&
                  ((db.dup_check_flag & DC_DELETE) ||
                   (db.dup_check_flag & DC_STORE)))
# endif
                ))
             {
                remove_file = YES;
             }
             else
             {
                if ((db.trans_dup_check_timeout > 0) &&
                    ((db.special_flag & OLD_ERROR_JOB) == 0))
                {
                   register int k;
                   char         tmp_filename[MAX_PATH_LENGTH];

                   tmp_filename[0] = '\0';
                   for (k = 0; k < rule[db.trans_rule_pos].no_of_rules; k++)
                   {
                      if (pmatch(rule[db.trans_rule_pos].filter[k],
                                 p_dir->d_name, NULL) == 0)
                      {
                         change_name(p_dir->d_name,
                                     rule[db.trans_rule_pos].filter[k],
                                     rule[db.trans_rule_pos].rename_to[k],
                                     tmp_filename, MAX_PATH_LENGTH,
                                     &counter_fd, &unique_counter, db.id.job);
                         break;
                      }
                   }
                   if (tmp_filename[0] != '\0')
                   {
                      if (((is_duplicate = isdup(fullname, tmp_filename,
# ifdef HAVE_STATX
                                                 stat_buf.stx_size,
# else
                                                 stat_buf.st_size,
# endif
                                                 db.crc_id,
                                                 db.trans_dup_check_timeout,
                                                 db.trans_dup_check_flag,
                                                 NO,
# ifdef HAVE_HW_CRC32
                                                 have_hw_crc32,
# endif
                                                 YES, YES)) == YES) &&
                          ((db.trans_dup_check_flag & DC_DELETE) ||
                           (db.trans_dup_check_flag & DC_STORE)))
                      {
                         remove_file = YES;
                      }
                   }
                }
             }
#endif
         }

         if (remove_file == YES)
         {
            char file_to_remove[MAX_FILENAME_LENGTH];

            file_to_remove[0] = '\0';
            if (db.no_of_restart_files > 0)
            {
               int  ii;
               char initial_filename[MAX_FILENAME_LENGTH];

               if ((db.lock == DOT) || (db.lock == DOT_VMS))
               {
                  (void)strcpy(initial_filename, db.lock_notation);
                  (void)strcat(initial_filename, p_dir->d_name);
               }
               else
               {
                  (void)strcpy(initial_filename, p_dir->d_name);
               }

               for (ii = 0; ii < db.no_of_restart_files; ii++)
               {
                  if ((CHECK_STRCMP(db.restart_file[ii], initial_filename) == 0) &&
                      (append_compare(db.restart_file[ii], fullname) == YES))
                  {
                     (void)strcpy(file_to_remove, db.restart_file[ii]);
                     remove_append(db.id.job, db.restart_file[ii]);
                     break;
                  }
               }
            }
#ifdef WITH_DUP_CHECK
            if (is_duplicate == YES)
            {
               dup_counter++;
# ifdef HAVE_STATX
               dup_counter_size += stat_buf.stx_size;
# else
               dup_counter_size += stat_buf.st_size;
# endif
               if (db.dup_check_flag & DC_WARN)
               {
                  trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "File `%s' is duplicate. #%x",
                            p_dir->d_name, db.id.job);
               }
            }
            if ((is_duplicate == YES) && (db.dup_check_flag & DC_STORE))
            {
               char *p_end,
                    save_dir[MAX_PATH_LENGTH];

               p_end = save_dir +
                       snprintf(save_dir, MAX_PATH_LENGTH, "%s%s%s/%x/",
                                p_work_dir, AFD_FILE_DIR, STORE_DIR, db.id.job);
               if ((mkdir(save_dir, DIR_MODE) == -1) && (errno != EEXIST))
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Failed to mkdir() `%s' : %s",
                             save_dir, strerror(errno));
                  if (unlink(fullname) == -1)
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "Failed to unlink() file `%s' due to duplicate check : %s #%x",
                                p_dir->d_name, strerror(errno), db.id.job);
                  }
# if defined (_DELETE_LOG) || defined (_OUTPUT_LOG)
                  else
                  {
                     log_data(p_dir->d_name, &stat_buf, YES,
#  ifdef _OUTPUT_LOG
                              OT_DUPLICATE_DELETE + '0',
#  endif
                              now);
                  }
# endif
               }
               else
               {
                  (void)strcpy(p_end, p_dir->d_name);
                  if (rename(fullname, save_dir) == -1)
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "Failed to rename() `%s' to `%s' : %s #%x",
                                fullname, save_dir, strerror(errno), db.id.job);
                     if (unlink(fullname) == -1)
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   "Failed to unlink() file `%s' due to duplicate check : %s #%x",
                                   p_dir->d_name, strerror(errno), db.id.job);
                     }
# if defined (_DELETE_LOG) || defined (_OUTPUT_LOG)
                     else
                     {
                        log_data(p_dir->d_name, &stat_buf, YES,
#  ifdef _OUTPUT_LOG
                                 OT_DUPLICATE_STORED + '0',
#  endif
                                 now);
                     }
# endif
                  }
               }
               files_not_send++;
# ifdef HAVE_STATX
               file_size_not_send += stat_buf.stx_size;
# else
               file_size_not_send += stat_buf.st_size;
# endif
            }
            else
            {
#endif /* WITH_DUP_CHECK */
               if (unlink(fullname) == -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to unlink() file `%s' due to age : %s #%x",
                             p_dir->d_name, strerror(errno), db.id.job);
               }
               else
               {
#if defined (_DELETE_LOG) || defined (_OUTPUT_LOG)
                  log_data(p_dir->d_name, &stat_buf,
# ifdef WITH_DUP_CHECK
                           is_duplicate,
#  ifdef _OUTPUT_LOG
                           (is_duplicate == YES) ? (OT_DUPLICATE_DELETE + '0') : (OT_AGE_LIMIT_DELETE + '0'),
#  endif
# else
#  ifdef _OUTPUT_LOG
                           OT_AGE_LIMIT_DELETE + '0',
#  endif
# endif
                           now);
#endif
#ifndef _DELETE_LOG
                  if ((db.protocol & FTP_FLAG) || (db.protocol & SFTP_FLAG))
                  {
                     if (db.no_of_restart_files > 0)
                     {
                        int append_file_number = -1,
                            ii;

                        for (ii = 0; ii < db.no_of_restart_files; ii++)
                        {
                           if (CHECK_STRCMP(db.restart_file[ii], p_dir->d_name) == 0)
                           {
                              append_file_number = ii;
                              break;
                           }
                        }
                        if (append_file_number != -1)
                        {
                           remove_append(db.id.job,
                                         db.restart_file[append_file_number]);
                        }
                     }
                  }
#endif
                  if (file_to_remove[0] != '\0')
                  {
                     if ((files_to_delete % 20) == 0)
                     {
                        /* Increase the space for the delete file name buffer. */
                        new_size = ((files_to_delete / 20) + 1) *
                                   20 * MAX_FILENAME_LENGTH;
                        offset = p_del_file_name - del_file_name_buffer;
                        if ((del_file_name_buffer = realloc(del_file_name_buffer,
                                                            new_size)) == NULL)
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      "Could not realloc() memory : %s #%x",
                                      strerror(errno), db.id.job);
                           exit(ALLOC_ERROR);
                        }
                        p_del_file_name = del_file_name_buffer + offset;
                     }
                     (void)strcpy(p_del_file_name, file_to_remove);
                     p_del_file_name += MAX_FILENAME_LENGTH;
                     files_to_delete++;
                  }

                  files_not_send++;
#ifdef HAVE_STATX
                  file_size_not_send += stat_buf.stx_size;
#else
                  file_size_not_send += stat_buf.st_size;
#endif
               }
#ifdef WITH_DUP_CHECK
            }
#endif
         }
         else
         {
#ifdef WITH_DUP_CHECK
            if ((is_duplicate == YES) && (db.dup_check_flag & DC_WARN))
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "File `%s' is duplicate. #%x", p_dir->d_name, db.id.job);
            }
#endif
            if ((files_to_send % 20) == 0)
            {
               /* Increase the space for the file name buffer. */
               new_size = ((files_to_send / 20) + 1) *
                          20 * MAX_FILENAME_LENGTH;
               offset = p_file_name - file_name_buffer;
               if ((file_name_buffer = realloc(file_name_buffer,
                                               new_size)) == NULL)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Could not realloc() memory : %s #%x",
                             strerror(errno), db.id.job);
                  exit(ALLOC_ERROR);
               }
               p_file_name = file_name_buffer + offset;

               /* Increase the space for the file size buffer. */
               new_size = ((files_to_send / 20) + 1) * 20 * sizeof(off_t);
               offset = (char *)p_file_size - (char *)file_size_buffer;
               if ((file_size_buffer = realloc(file_size_buffer,
                                               new_size)) == NULL)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Could not realloc() memory : %s #%x",
                             strerror(errno), db.id.job);
                  exit(ALLOC_ERROR);
               }
               p_file_size = (off_t *)((char *)file_size_buffer + offset);

               if ((fsa->protocol_options & SORT_FILE_NAMES) ||
#ifdef WITH_EUMETSAT_HEADERS
                   (db.special_flag & ADD_EUMETSAT_HEADER) ||
#endif
                   (fsa->protocol_options & KEEP_TIME_STAMP))
               {
                  /* Increase the space for the file mtime buffer. */
                  new_size = ((files_to_send / 20) + 1) * 20 * sizeof(time_t);
                  offset = (char *)p_file_mtime - (char *)file_mtime_buffer;
                  if ((file_mtime_buffer = realloc(file_mtime_buffer,
                                                   new_size)) == NULL)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Could not realloc() memory : %s #%x",
                                strerror(errno), db.id.job);
                     exit(ALLOC_ERROR);
                  }
                  p_file_mtime = (time_t *)((char *)file_mtime_buffer + offset);
               }
            }

            /* Sort the files, newest must be last (FIFO). */
            if ((fsa->protocol_options & SORT_FILE_NAMES) &&
                (files_to_send > 1) &&
#ifdef HAVE_STATX
                (*(p_file_mtime - 1) > stat_buf.stx_mtime.tv_sec)
#else
                (*(p_file_mtime - 1) > stat_buf.st_mtime)
#endif
               )
            {
               int    i;
               off_t  *sp_file_size = p_file_size - 1;
               time_t *sp_mtime = p_file_mtime - 1;
               char   *sp_file_name = p_file_name - MAX_FILENAME_LENGTH;

               for (i = files_to_send; i > 0; i--)
               {
#ifdef HAVE_STATX
                  if (*sp_mtime <= stat_buf.stx_mtime.tv_sec)
#else
                  if (*sp_mtime <= stat_buf.st_mtime)
#endif
                  {
                     break;
                  }
                  sp_mtime--; sp_file_size--;
                  sp_file_name -= MAX_FILENAME_LENGTH;
               }
               (void)memmove(sp_mtime + 2, sp_mtime + 1,
                             (files_to_send - i) * sizeof(time_t));
#ifdef HAVE_STATX
               *(sp_mtime + 1) = stat_buf.stx_mtime.tv_sec;
#else
               *(sp_mtime + 1) = stat_buf.st_mtime;
#endif
               (void)memmove(sp_file_size + 2, sp_file_size + 1,
                             (files_to_send - i) * sizeof(off_t));
#ifdef HAVE_STATX
               *(sp_file_size + 1) = stat_buf.stx_size;
#else
               *(sp_file_size + 1) = stat_buf.st_size;
#endif
               (void)memmove(sp_file_name + (2 * MAX_FILENAME_LENGTH),
                             sp_file_name + MAX_FILENAME_LENGTH,
                             (files_to_send - i) * MAX_FILENAME_LENGTH);
               (void)strcpy(sp_file_name + MAX_FILENAME_LENGTH,
                            p_dir->d_name);
            }
            else
            {
               (void)strcpy(p_file_name, p_dir->d_name);
#ifdef HAVE_STATX
               *p_file_size = stat_buf.stx_size;
#else
               *p_file_size = stat_buf.st_size;
#endif
               if (file_mtime_buffer != NULL)
               {
#ifdef HAVE_STATX
                  *p_file_mtime = stat_buf.stx_mtime.tv_sec;
#else
                  *p_file_mtime = stat_buf.st_mtime;
#endif
               }
            }
            p_file_name += MAX_FILENAME_LENGTH;
            p_file_size++;
            p_file_mtime++;
            files_to_send++;
#ifdef HAVE_STATX
            *file_size_to_send += stat_buf.stx_size;
#else
            *file_size_to_send += stat_buf.st_size;
#endif
         }
      }
      errno = 0;
   } /* while ((p_dir = readdir(dp)) != NULL) */

   if (errno)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not readdir() `%s' : %s #%x",
                 file_path, strerror(errno), db.id.job);
   }

#ifdef WITH_DUP_CHECK
   isdup_detach();
#endif

   if ((file_mtime_buffer != NULL) &&
#ifdef WITH_EUMETSAT_HEADERS
       ((db.special_flag & ADD_EUMETSAT_HEADER) == 0) &&
#endif
       ((fsa->protocol_options & KEEP_TIME_STAMP) == 0))
   {
      free(file_mtime_buffer);
      file_mtime_buffer = NULL;
   }

   if (files_not_send > 0)
   {
      /* Total file counter. */
#ifdef LOCK_DEBUG
      lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
      lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC);
#endif
      fsa->total_file_counter -= files_not_send;
#ifdef _VERIFY_FSA
      if (fsa->total_file_counter < 0)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Total file counter for host `%s' less then zero. Correcting to 0. #%x",
                    fsa->host_dsp_name, db.id.job);
         fsa->total_file_counter = 0;
      }
#endif

      /* Total file size. */
      fsa->total_file_size -= file_size_not_send;
#ifdef _VERIFY_FSA
      if (fsa->total_file_size < 0)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Total file size for host `%s' overflowed. Correcting to 0. #%x",
                    fsa->host_dsp_name, db.id.job);
         fsa->total_file_size = 0;
      }
      else if ((fsa->total_file_counter == 0) &&
               (fsa->total_file_size > 0))
           {
              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                         "fc for host `%s' is zero but fs is not zero. Correcting to 0. #%x",
                         fsa->host_dsp_name, db.id.job);
              fsa->total_file_size = 0;
           }
#endif

      if ((fsa->total_file_counter == 0) && (fsa->total_file_size == 0) &&
          (fsa->error_counter > 0))
      {
         int j;

#ifdef LOCK_DEBUG
         lock_region_w(fsa_fd, db.lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
         lock_region_w(fsa_fd, db.lock_offset + LOCK_EC);
#endif
         fsa->error_counter = 0;

         /*
          * Remove the error condition (NOT_WORKING) from all jobs
          * of this host.
          */
         for (j = 0; j < fsa->allowed_transfers; j++)
         {
            if ((j != db.job_no) &&
                (fsa->job_status[j].connect_status == NOT_WORKING))
            {
               fsa->job_status[j].connect_status = DISCONNECT;
            }
         }
         fsa->error_history[0] = 0;
         fsa->error_history[1] = 0;
#ifdef LOCK_DEBUG
         unlock_region(fsa_fd, db.lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
         unlock_region(fsa_fd, db.lock_offset + LOCK_EC);
#endif
      }
#ifdef LOCK_DEBUG
      unlock_region(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
      unlock_region(fsa_fd, db.lock_offset + LOCK_TFC);
#endif
#ifdef WITH_DUP_CHECK
      if (dup_counter > 0)
      {
         trans_log(INFO_SIGN, NULL, 0, NULL, NULL,
# if SIZEOF_OFF_T == 4
                   "Deleted %d duplicate file(s) (%ld bytes). #%x",
# else
                   "Deleted %d duplicate file(s) (%lld bytes). #%x",
# endif
                   dup_counter, (pri_off_t)dup_counter_size, db.id.job);
      }
      if ((files_not_send - dup_counter) > 0)
      {
         trans_log(INFO_SIGN, NULL, 0, NULL, NULL,
# if SIZEOF_OFF_T == 4
                   "Deleted %d file(s) (%ld bytes) due to age. #%x",
# else
                   "Deleted %d file(s) (%lld bytes) due to age. #%x",
# endif
                   files_not_send - dup_counter,
                   (pri_off_t)(file_size_not_send - dup_counter_size),
                   db.id.job);
      }
#else
      trans_log(INFO_SIGN, NULL, 0, NULL, NULL,
# if SIZEOF_OFF_T == 4
                "Deleted %d file(s) (%ld bytes) due to age. #%x",
# else
                "Deleted %d file(s) (%lld bytes) due to age. #%x",
# endif
                files_not_send, (pri_off_t)file_size_not_send,
                db.id.job);
#endif
#ifdef WITH_ERROR_QUEUE
      if ((files_to_send == 0) && (fsa->host_status & ERROR_QUEUE_SET) &&
          (check_error_queue(db.id.job, -1, 0, 0) == YES))
      {
         (void)remove_from_error_queue(db.id.job, fsa, db.fsa_pos, fsa_fd);
      }
#endif
   }

   if (closedir(dp) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not closedir() `%s' : %s #%x",
                 file_path, strerror(errno), db.id.job);
      exit(OPEN_FILE_DIR_ERROR);
   }

   /*
    * If we just return zero here when all files have been deleted
    * due to age and sf_xxx is bursting it does not know if this
    * is an error situation or not. So return -1 if we deleted all
    * files.
    */
   if ((files_to_send == 0) && (files_not_send > 0))
   {
      files_to_send = -1;
   }
   return(files_to_send);
}


#if defined (_DELETE_LOG) || defined (_OUTPUT_LOG)
/*++++++++++++++++++++++++++++++ log_data() +++++++++++++++++++++++++++++*/
static void
log_data(char         *d_name,
#ifdef HAVE_STATX
         struct statx *stat_buf,
#else
         struct stat  *stat_buf,
#endif
# ifdef WITH_DUP_CHECK
         int          is_duplicate,
# endif
# ifdef _OUTPUT_LOG
         char         output_type,
# endif
         time_t       now)
{
# ifdef _DELETE_LOG
   int    prog_name_length;
   size_t dl_real_size;
   char   str_diff_time[2 + MAX_LONG_LENGTH + 6 + MAX_LONG_LENGTH + 12 + MAX_LONG_LENGTH + 30 + 1];
# endif

# ifdef _OUTPUT_LOG
   if (db.output_log == YES)
   {
      if (ol_fd == -2)
      {
#  ifdef WITHOUT_FIFO_RW_SUPPORT
         output_log_fd(&ol_fd, &ol_readfd, &db.output_log);
#  else
         output_log_fd(&ol_fd, &db.output_log);
#  endif
      }
      if (ol_fd > -1)
      {
         if (ol_data == NULL)
         {
            int current_toggle,
                protocol;

            if (db.protocol & FTP_FLAG)
            {
#  ifdef WITH_SSL
               if (db.tls_auth == NO)
               {
                  protocol = FTP;
               }
               else
               {
                  protocol = FTPS;
               }
#  else
               protocol = FTP;
#  endif
            }
            else if (db.protocol & LOC_FLAG)
                 {
                    protocol = LOC;
                 }
            else if (db.protocol & EXEC_FLAG)
                 {
                    protocol = EXEC;
                 }
            else if (db.protocol & HTTP_FLAG)
                 {
#  ifdef WITH_SSL
                    if (db.tls_auth == NO)
                    {
                       protocol = HTTP;
                    }
                    else
                    {
                       protocol = HTTPS;
                    }
#  else
                    protocol = HTTP;
#  endif
                 }
            else if (db.protocol & SFTP_FLAG)
                 {
                    protocol = SFTP;
                 }
#  ifdef _WITH_SCP_SUPPORT
            else if (db.protocol & SCP_FLAG)
                 {
                    protocol = SCP;
                 }
#  endif
#  ifdef _WITH_WMO_SUPPORT
            else if (db.protocol & WMO_FLAG)
                 {
                    protocol = WMO;
                 }
#  endif
#  ifdef _WITH_MAP_SUPPORT
            else if (db.protocol & MAP_FLAG)
                 {
                    protocol = MAP;
                 }
#  endif
#  ifdef _WITH_DFAX_SUPPORT
            else if (db.protocol & DFAX_FLAG)
                 {
                    protocol = DFAX;
                 }
#  endif
            else if (db.protocol & SMTP_FLAG)
                 {
                    protocol = SMTP;
                 }
#  ifdef _WITH_DE_MAIL_SUPPORT
            else if (db.protocol & DE_MAIL_FLAG)
                 {
                    protocol = DE_MAIL;
                 }
#  endif
                 else
                 {
                    system_log(DEBUG_SIGN, __FILE__, __LINE__,
                               "Unknown protocol flag %d, setting FTP. #%x",
                               db.protocol, db.id.job);
                    protocol = FTP;
                 }

            if (fsa->real_hostname[1][0] == '\0')
            {
               current_toggle = HOST_ONE;
            }
            else
            {
               if (db.toggle_host == YES)
               {
                  if (fsa->host_toggle == HOST_ONE)
                  {
                     current_toggle = HOST_TWO;
                  }
                  else
                  {
                     current_toggle = HOST_ONE;
                  }
               }
               else
               {
                  current_toggle = (int)fsa->host_toggle;
               }
            }

            output_log_ptrs(&ol_retries,
                            &ol_job_number,
                            &ol_data,   /* Pointer to buffer. */
                            &ol_file_name,
                            &ol_file_name_length,
                            &ol_archive_name_length,
                            &ol_file_size,
                            &ol_unl,
                            &ol_size,
                            &ol_transfer_time,
                            &ol_output_type,
                            db.host_alias,
                            (current_toggle - 1),
                            protocol,
                            &db.output_log);
         }
         (void)memcpy(ol_file_name, db.p_unique_name, db.unl);
         (void)strcpy(ol_file_name + db.unl, d_name);
         *ol_file_name_length = (unsigned short)strlen(ol_file_name);
         ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
         ol_file_name[*ol_file_name_length + 1] = '\0';
         (*ol_file_name_length)++;
#  ifdef HAVE_STATX
         *ol_file_size = stat_buf->stx_size;
#  else
         *ol_file_size = stat_buf->st_size;
#  endif
         *ol_job_number = db.id.job;
         *ol_retries = db.retries;
         *ol_unl = db.unl;
         *ol_transfer_time = 0L;
         *ol_archive_name_length = 0;
         *ol_output_type = output_type;
         ol_real_size = *ol_file_name_length + ol_size;
         if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "write() error : %s #%x", strerror(errno), db.id.job);
         }
      }
   }
# endif

# ifdef _DELETE_LOG
   if (dl.fd == -1)
   {
      delete_log_ptrs(&dl);
   }
   (void)strcpy(dl.file_name, d_name);
   (void)snprintf(dl.host_name, MAX_HOSTNAME_LENGTH + 4 + 1, "%-*s %03x",
                  MAX_HOSTNAME_LENGTH, fsa->host_alias,
#  ifdef WITH_DUP_CHECK
                  (is_duplicate == YES) ? DUP_OUTPUT : AGE_OUTPUT);
#  else
                  AGE_OUTPUT);
#  endif
#  ifdef HAVE_STATX
   *dl.file_size = stat_buf->stx_size;
#  else
   *dl.file_size = stat_buf->st_size;
#  endif
   *dl.job_id = db.id.job;
   *dl.dir_id = 0;
   *dl.input_time = db.creation_time;
   *dl.split_job_counter = db.split_job_counter;
   *dl.unique_number = db.unique_number;
   *dl.file_name_length = strlen(d_name);
#  ifdef WITH_DUP_CHECK
   if (is_duplicate == YES)
   {
      str_diff_time[0] = '\0';
   }
   else
   {
      time_t diff_time;

#   ifdef HAVE_STATX
      if (now < stat_buf->stx_mtime.tv_sec)
#   else
      if (now < stat_buf->st_mtime)
#   endif
      {
         diff_time = 0L;
      }
      else
      {
#   ifdef HAVE_STATX
         diff_time = now - stat_buf->stx_mtime.tv_sec;
#   else
         diff_time = now - stat_buf->st_mtime;
#   endif
      }
#  endif
      (void)snprintf(str_diff_time,
                     2 + MAX_LONG_LENGTH + 6 + MAX_LONG_LENGTH + 12 + MAX_LONG_LENGTH + 30 + 1,
#  if SIZEOF_TIME_T == 4
                     "%c>%ld [now=%ld file_mtime=%ld] (%s %d)",
#  else
                     "%c>%lld [now=%lld file_mtime=%lld] (%s %d)",
#  endif
                     SEPARATOR_CHAR, (pri_time_t)diff_time, (pri_time_t)now,
#  ifdef HAVE_STATX
                     (pri_time_t)stat_buf->stx_mtime.tv_sec,
#  else
                     (pri_time_t)stat_buf->st_mtime,
#  endif
                     __FILE__, __LINE__);
#  ifdef WITH_DUP_CHECK
   }
#  endif
   if (db.protocol & FTP_FLAG)
   {
      if (db.no_of_restart_files > 0)
      {
         int append_file_number = -1,
             ii;

         for (ii = 0; ii < db.no_of_restart_files; ii++)
         {
            if (CHECK_STRCMP(db.restart_file[ii], d_name) == 0)
            {
               append_file_number = ii;
               break;
            }
         }
         if (append_file_number != -1)
         {
            remove_append(db.id.job,
                          db.restart_file[append_file_number]);
         }
      }
      prog_name_length = snprintf((dl.file_name + *dl.file_name_length + 1),
                                  MAX_FILENAME_LENGTH + 1,
                                  "%s%s", SEND_FILE_FTP, str_diff_time);
   }
   else if (db.protocol & LOC_FLAG)
        {
           prog_name_length = snprintf((dl.file_name + *dl.file_name_length + 1),
                                       MAX_FILENAME_LENGTH + 1,
                                       "%s%s", SEND_FILE_LOC, str_diff_time);
        }
   else if (db.protocol & EXEC_FLAG)
        {
           prog_name_length = snprintf((dl.file_name + *dl.file_name_length + 1),
                                       MAX_FILENAME_LENGTH + 1,
                                       "%s%s", SEND_FILE_EXEC, str_diff_time);
        }
   else if (db.protocol & HTTP_FLAG)
        {
           prog_name_length = snprintf((dl.file_name + *dl.file_name_length + 1),
                                       MAX_FILENAME_LENGTH + 1,
                                       "%s%s", SEND_FILE_HTTP, str_diff_time);
        }
   else if (db.protocol & SFTP_FLAG)
        {
           if (db.no_of_restart_files > 0)
           {
              int append_file_number = -1,
                  ii;

              for (ii = 0; ii < db.no_of_restart_files; ii++)
              {
                 if (CHECK_STRCMP(db.restart_file[ii], d_name) == 0)
                 {
                    append_file_number = ii;
                    break;
                 }
              }
              if (append_file_number != -1)
              {
                 remove_append(db.id.job,
                               db.restart_file[append_file_number]);
              }
           }
           prog_name_length = snprintf((dl.file_name + *dl.file_name_length + 1),
                                       MAX_FILENAME_LENGTH + 1,
                                       "%s%s", SEND_FILE_SFTP, str_diff_time);
        }
#  ifdef _WITH_SCP_SUPPORT
   else if (db.protocol & SCP_FLAG)
        {
           prog_name_length = snprintf((dl.file_name + *dl.file_name_length + 1),
                                       MAX_FILENAME_LENGTH + 1,
                                       "%s%s", SEND_FILE_SCP, str_diff_time);
        }
#  endif
#  ifdef _WITH_WMO_SUPPORT
   else if (db.protocol & WMO_FLAG)
        {
           prog_name_length = snprintf((dl.file_name + *dl.file_name_length + 1),
                                       MAX_FILENAME_LENGTH + 1,
                                       "%s%s", SEND_FILE_WMO, str_diff_time);
        }
#  endif
#  ifdef _WITH_MAP_SUPPORT
   else if (db.protocol & MAP_FLAG)
        {
           prog_name_length = snprintf((dl.file_name + *dl.file_name_length + 1),
                                       MAX_FILENAME_LENGTH + 1,
                                       "%s%s", SEND_FILE_MAP, str_diff_time);
        }
#  endif
#  ifdef _WITH_DFAX_SUPPORT
   else if (db.protocol & DFAX_FLAG)
        {
           prog_name_length = snprintf((dl.file_name + *dl.file_name_length + 1),
                                       MAX_FILENAME_LENGTH + 1,
                                       "%s%s", SEND_FILE_DFAX, str_diff_time);
        }
#  endif
#  ifdef _WITH_DE_MAIL_SUPPORT
   else if ((db.protocol & SMTP_FLAG) || (db.protocol & DE_MAIL_FLAG))
#  else
   else if (db.protocol & SMTP_FLAG)
#  endif
        {
           prog_name_length = snprintf((dl.file_name + *dl.file_name_length + 1),
                                       MAX_FILENAME_LENGTH + 1,
                                       "%s%s", SEND_FILE_SMTP, str_diff_time);
        }
        else
        {
           prog_name_length = snprintf((dl.file_name + *dl.file_name_length + 1),
                                       MAX_FILENAME_LENGTH + 1,
                                       "sf_???%s", str_diff_time);
        }

   dl_real_size = *dl.file_name_length + dl.size +
                  prog_name_length;
   if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "write() error : %s #%x", strerror(errno), db.id.job);
   }
# endif

   return;
}
#endif
