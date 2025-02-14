/*
 *  check_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2025 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_files - moves all files that are to be distributed
 **                 to a temporary directory
 **
 ** SYNOPSIS
 **   int check_files(struct directory_entry *p_de,
 **                   char                   *src_file_path,
 **                   int                    use_afd_file_dir,
 **                   char                   *tmp_file_dir,
 **                   int                    count_files,
 **                   int                    *unique_number,
 **                   time_t                 current_time,
 **                   int                    *rescan_dir,
 **                   off_t                  *total_file_size)
 **
 ** DESCRIPTION
 **   The function check_files() searches the directory 'p_de->dir'
 **   for files 'p_de->fme[].file_mask[]'. If it finds them they get
 **   moved to a unique directory of the following format:
 **            nnnnnnnnnn_llll
 **                |       |
 **                |       +-------> Counter which is set to zero
 **                |                 after each second. This
 **                |                 ensures that no message can
 **                |                 have the same name in one
 **                |                 second.
 **                +---------------> creation time in seconds
 **
 **   check_files() will only copy 'max_copied_files' (100) or
 **   'max_copied_file_size' bytes. If there are more files or the
 **   size limit has been reached, these will be handled with the next
 **   call to check_files(). Otherwise it will take too long before files
 **   get transmitted and other directories get their turn. Since local
 **   copying is always faster than copying something to a remote machine,
 **   the AMG will have finished its task before the FD has finished.
 **
 ** RETURN VALUES
 **   Returns the number of files that have been copied and the directory
 **   where the files have been moved in 'tmp_file_dir'. If it fails it
 **   will return INCORRECT.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.10.1995 H.Kiehl Created
 **   18.10.1997 H.Kiehl Introduced inverse filtering.
 **   04.03.1999 H.Kiehl When copying files don't only limit the number
 **                      of files, also limit the size that may be copied.
 **   05.06.1999 H.Kiehl Option to check for last character in file.
 **   25.04.2001 H.Kiehl Check if we need to move or copy a file.
 **   27.08.2001 H.Kiehl Extra parameter to enable or disable counting
 **                      of files on input, so we don't count files twice
 **                      when queue is opened.
 **   03.05.2002 H.Kiehl Count total number of files and bytes in directory
 **                      regardless if they are to be retrieved or not.
 **   12.07.2002 H.Kiehl Check if we can already delete any old files that
 **                      have no distribution rule.
 **   15.07.2002 H.Kiehl Option to ignore files which have a certain size.
 **   06.02.2003 H.Kiehl Put max_copied_files and max_copied_file_size into
 **                      FRA.
 **   13.07.2003 H.Kiehl Store mtime of file.
 **   16.08.2003 H.Kiehl Added options to start using the given directory
 **                      when a certain file has arrived and/or a certain
 **                      number of files and/or the files in the directory
 **                      have a certain size.
 **   02.09.2004 H.Kiehl Added option "igore file time".
 **   17.04.2005 H.Kiehl When it is a paused directory do not wait for
 **                      a certain file, file size or make any date check.
 **   01.06.2005 H.Kiehl Build in check for duplicate files.
 **   15.03.2006 H.Kiehl When checking if we have sufficient file permissions
 **                      we must check supplementary groups as well.
 **   29.03.2006 H.Kiehl Support for pattern matching with expanding
 **                      filters.
 **   10.09.2009 H.Kiehl Tell caller of function it needs to rescan directory
 **                      if file size or time has not yet been reached.
 **   20.05.2013 H.Kiehl Simplified code by deleting lots of duplicate code.
 **   21.08.2021 H.Kiehl When all host are disabled and 'do not remove' is
 **                      set, don't delete the files for local dirs.
 **   27.11.2022 H.Kiehl Use statx() when available.
 **   16.06.2023 H.Kiehl Do not delete UNKNOWN_FILES inotify files
 **                      immediately by default. del_unknown_inotify_files()
 **                      will remove those files.
 **   13.02.2025 H.Kiehl If host is disabled, we must do a pattern match
 **                      because if the locking procedure is not dot
 **                      we will delete files to early.
 **   14.02.2025 H.Kiehl Improve above fix by just searching for not (!)
 **                      sign. All other patterns should match.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* fprintf(), sprintf()               */
#include <string.h>                /* strcmp(), strcpy(), strerror()     */
#include <stdlib.h>                /* exit()                             */
#include <unistd.h>                /* write(), lseek(), close(), access()*/
#ifdef _WITH_PTHREAD
# include <pthread.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>              /* stat(), S_ISREG()                  */
#include <dirent.h>                /* opendir(), closedir(), readdir(),  */
                                   /* DIR, struct dirent                 */
#if defined (LINUX) && defined (DIR_CHECK_CAP_CHOWN)
# include <sys/capability.h>
#endif
#ifdef HAVE_MMAP
# include <sys/mman.h>
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "amgdefs.h"


/* External global variables. */
extern int                        afd_file_dir_length,
                                  alfc, /* Additional locked file counter*/
                                  *amg_counter,
                                  fra_fd, /* Needed by ABS_REDUCE_QUEUE()*/
                                  receive_log_fd;
#ifdef HAVE_HW_CRC32
extern int                        have_hw_crc32;
#endif
#ifdef _POSIX_SAVED_IDS
extern int                        no_of_sgids;
extern uid_t                      afd_uid;
extern gid_t                      afd_gid,
                                  *afd_sgids;
#endif
#ifdef _INPUT_LOG
extern int                        il_fd,
                                  *il_unique_number;
extern unsigned int               *il_dir_number;
extern size_t                     il_size;
extern off_t                      *il_file_size;
extern time_t                     *il_time;
extern char                       *il_file_name,
                                  *il_data;
#endif /* _INPUT_LOG */
#ifdef WITH_DUP_CHECK
extern char                       *p_work_dir;
#endif
#ifndef _WITH_PTHREAD
extern off_t                      *file_size_pool;
extern time_t                     *file_mtime_pool;
extern char                       **file_name_pool;
extern unsigned char              *file_length_pool;
#endif
#ifdef _WITH_PTHREAD
extern pthread_mutex_t            fsa_mutex;
#endif
extern struct fileretrieve_status *fra;
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif
#ifdef MULTI_FS_SUPPORT
extern struct extra_work_dirs     *ewl;
#endif
extern char                       *afd_file_dir;
#if defined (LINUX) && defined (DIR_CHECK_CAP_CHOWN)
extern cap_t                      caps;
extern int                        can_do_chown,
                                  hardlinks_protected_set;
#endif

/* Local function prototypes. */
#ifdef _POSIX_SAVED_IDS
static int                        check_sgids(gid_t);
#endif


/*########################### check_files() #############################*/
int
check_files(struct directory_entry *p_de,
            char                   *src_file_path,
            int                    use_afd_file_dir,
            char                   *tmp_file_dir,  /* Return directory   */
                                                   /* where files are    */
                                                   /* stored.            */
            int                    count_files,
            int                    *unique_number,
            time_t                 current_time,
            int                    *rescan_dir,
#ifdef _WITH_PTHREAD
            off_t                  *file_size_pool,
            time_t                 *file_mtime_pool,
            char                   **file_name_pool,
            unsigned char          *file_length_pool,
#endif
#if defined (_MAINTAINER_LOG) && defined (SHOW_FILE_MOVING)
            char                   *caller,
            int                    line,
#endif
            off_t                  *total_file_size)
{
   int           files_copied = 0,
                 files_in_dir = 0,
                 full_scan = YES,
                 g_what_done = 0,
                 i,
                 ret,
                 set_error_counter = NO; /* Indicator to tell that we */
                                         /* set the fra error_counter */
                                         /* when we are called.       */
   unsigned int  split_job_counter = 0;
   off_t         bytes_in_dir = 0;
   time_t        diff_time = 0, /* Silence compiler. */
                 pmatch_time;
   char          fullname[MAX_PATH_LENGTH],
                 *ptr = NULL,
                 *work_ptr;
#ifdef HAVE_STATX
   struct statx  stat_buf;
#else
   struct stat   stat_buf;
#endif
   DIR           *dp;
   struct dirent *p_dir;
#ifdef _INPUT_LOG
   size_t        il_real_size;
#endif

   (void)strcpy(fullname, src_file_path);
   work_ptr = fullname + strlen(fullname);
   *work_ptr++ = '/';
   *work_ptr = '\0';
   *rescan_dir = NO;

   /*
    * Check if this is the special case when we have a dummy remote dir
    * and its queue is stopped. In this case it is just necessary to
    * move the files to the paused directory which is in tmp_file_dir
    * or visa versa.
    */
   if (use_afd_file_dir == YES)
   {
      tmp_file_dir[0] = '\0';
   }
   else
   {
      if (count_files == PAUSED_REMOTE)
      {
         (void)strcpy(tmp_file_dir, p_de->paused_dir);
         ptr = tmp_file_dir + strlen(tmp_file_dir);
         *ptr = '/';
         ptr++;
         *ptr = '\0';

         /* If remote paused directory does not exist, create it. */
#ifdef HAVE_STATX
         if ((statx(0, tmp_file_dir, AT_STATX_SYNC_AS_STAT,
                    STATX_MODE, &stat_buf) == -1) ||
             (S_ISDIR(stat_buf.stx_mode) == 0))
#else
         if ((stat(tmp_file_dir, &stat_buf) < 0) ||
             (S_ISDIR(stat_buf.st_mode) == 0))
#endif
         {
            /*
             * Only the AFD may read and write in this directory!
             */
#ifdef GROUP_CAN_WRITE
            if (mkdir(tmp_file_dir, (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP)) < 0)
#else
            if (mkdir(tmp_file_dir, (S_IRUSR | S_IWUSR | S_IXUSR)) < 0)
#endif
            {
               if (errno != EEXIST)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Could not mkdir() `%s' to save files : %s"),
                             tmp_file_dir, strerror(errno));
                  errno = 0;
                  return(INCORRECT);
               }
            }
         }
      }
      else
      {
         (void)strcpy(tmp_file_dir, p_de->dir);
         ptr = tmp_file_dir + strlen(tmp_file_dir);
         *ptr = '/';
         ptr++;
      }
   }

#ifdef _DEBUG
   system_log(DEBUG_SIGN, __FILE__, __LINE__, "Scanning: %s", fullname);
#endif
   if ((dp = opendir(fullname)) == NULL)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, current_time,
                  _("Failed to opendir() `%s' : %s @%x"),
                  fullname, strerror(errno), p_de->dir_id);
      if (fra[p_de->fra_pos].fsa_pos == -1)
      {
         lock_region_w(fra_fd,
#ifdef LOCK_DEBUG
                       (char *)&fra[p_de->fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                       (char *)&fra[p_de->fra_pos].error_counter - (char *)fra);
#endif
         fra[p_de->fra_pos].error_counter += 1;
         if ((fra[p_de->fra_pos].error_counter >= fra[p_de->fra_pos].max_errors) &&
             ((fra[p_de->fra_pos].dir_flag & DIR_ERROR_SET) == 0))
         {
            fra[p_de->fra_pos].dir_flag |= DIR_ERROR_SET;
            SET_DIR_STATUS(fra[p_de->fra_pos].dir_flag,
                           current_time,
                           fra[p_de->fra_pos].start_event_handle,
                           fra[p_de->fra_pos].end_event_handle,
                           fra[p_de->fra_pos].dir_status);
            error_action(p_de->alias, "start", DIR_ERROR_ACTION,
                         receive_log_fd);
            event_log(0L, EC_DIR, ET_EXT, EA_ERROR_START, "%s", p_de->alias);
         }
         unlock_region(fra_fd,
#ifdef LOCK_DEBUG
                       (char *)&fra[p_de->fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                       (char *)&fra[p_de->fra_pos].error_counter - (char *)fra);
#endif
      }
      return(INCORRECT);
   }

   /*
    * See if we have to wait for a certain file name before we
    * can check this directory.
    */
   if ((fra[p_de->fra_pos].wait_for_filename[0] != '\0') &&
       (count_files != NO))
   {
      int          gotcha = NO;
      unsigned int dummy_files_in_dir = 0;
      off_t        dummy_bytes_in_dir = 0;

      while ((p_dir = readdir(dp)) != NULL)
      {
         if ((p_dir->d_name[0] != '.') &&
#ifdef LINUX
             (p_dir->d_type == DT_REG) &&
#endif
             ((alfc < 1) || ((check_additional_lock_filters(p_dir->d_name)) == 0)))
         {
            (void)strcpy(work_ptr, p_dir->d_name);
#ifdef HAVE_STATX
            if (statx(0, fullname,
                      AT_STATX_SYNC_AS_STAT,
                      STATX_SIZE |
# if defined (_POSIX_SAVED_IDS) || !defined (LINUX)
                      STATX_MODE |
# endif
# ifdef _POSIX_SAVED_IDS
                      STATX_UID | STATX_ATIME |
# endif
                      STATX_MTIME,
                      &stat_buf) == -1)
#else
            if (stat(fullname, &stat_buf) < 0)
#endif
            {
               if (errno != ENOENT)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                             _("Failed to statx() `%s' : %s"),
#else
                             _("Failed to stat() `%s' : %s"),
#endif
                             fullname, strerror(errno));
               }
               continue;
            }

            if ((fra[p_de->fra_pos].ignore_file_time != 0) &&
                (fra[p_de->fra_pos].fsa_pos == -1))
            {
#ifdef HAVE_STATX
               diff_time = current_time - stat_buf.stx_mtime.tv_sec;
#else
               diff_time = current_time - stat_buf.st_mtime;
#endif
            }

#ifndef LINUX
            /* Sure it is a normal file? */
# ifdef HAVE_STATX
            if (S_ISREG(stat_buf.stx_mode))
# else
            if (S_ISREG(stat_buf.st_mode))
# endif
            {
#endif
               dummy_files_in_dir++;
#ifdef HAVE_STATX
               dummy_bytes_in_dir += stat_buf.stx_size;
#else
               dummy_bytes_in_dir += stat_buf.st_size;
#endif
               if (((fra[p_de->fra_pos].ignore_size == -1) ||
                    ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_EQUAL) &&
#ifdef HAVE_STATX
                     (fra[p_de->fra_pos].ignore_size != stat_buf.stx_size)) ||
#else
                     (fra[p_de->fra_pos].ignore_size != stat_buf.st_size)) ||
#endif
                    ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_LESS_THEN) &&
#ifdef HAVE_STATX
                     (fra[p_de->fra_pos].ignore_size < stat_buf.stx_size)) ||
#else
                     (fra[p_de->fra_pos].ignore_size < stat_buf.st_size)) ||
#endif
                    ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_GREATER_THEN) &&
#ifdef HAVE_STATX
                     (fra[p_de->fra_pos].ignore_size > stat_buf.stx_size))) &&
#else
                     (fra[p_de->fra_pos].ignore_size > stat_buf.st_size))) &&
#endif
                   ((fra[p_de->fra_pos].ignore_file_time == 0) ||
                    (fra[p_de->fra_pos].fsa_pos != -1) || /* Time+size check only local dirs! */
                     ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_EQUAL) &&
                      (fra[p_de->fra_pos].ignore_file_time != diff_time)) ||
                    ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_LESS_THEN) &&
                     (fra[p_de->fra_pos].ignore_file_time < diff_time)) ||
                    ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_GREATER_THEN) &&
                     (fra[p_de->fra_pos].ignore_file_time > diff_time))))
               {
#ifdef _POSIX_SAVED_IDS
# ifdef HAVE_STATX
                  if ((stat_buf.stx_mode & S_IROTH) ||
                      ((stat_buf.stx_gid == afd_gid) && (stat_buf.stx_mode & S_IRGRP)) ||
                      ((stat_buf.stx_uid == afd_uid) && (stat_buf.stx_mode & S_IRUSR)) ||
                      ((stat_buf.stx_mode & S_IRGRP) && (no_of_sgids > 0) &&
                       (check_sgids(stat_buf.stx_gid) == YES)))
# else
                  if ((stat_buf.st_mode & S_IROTH) ||
                      ((stat_buf.st_gid == afd_gid) && (stat_buf.st_mode & S_IRGRP)) ||
                      ((stat_buf.st_uid == afd_uid) && (stat_buf.st_mode & S_IRUSR)) ||
                      ((stat_buf.st_mode & S_IRGRP) && (no_of_sgids > 0) &&
                       (check_sgids(stat_buf.st_gid) == YES)))
# endif
#else
                  if ((eaccess(fullname, R_OK) == 0))
#endif
                  {
                     if ((ret = pmatch(fra[p_de->fra_pos].wait_for_filename,
                                       p_dir->d_name, &current_time)) == 0)
                     {
                        if ((fra[p_de->fra_pos].end_character == -1) ||
#ifdef HAVE_STATX
                            (fra[p_de->fra_pos].end_character == get_last_char(fullname, stat_buf.stx_size))
#else
                            (fra[p_de->fra_pos].end_character == get_last_char(fullname, stat_buf.st_size))
#endif
                           )
                        {
                           gotcha = YES;
                        }
                        else
                        {
                           if (fra[p_de->fra_pos].end_character != -1)
                           {
                              p_de->search_time -= 5;
                           }
                        }
                        break;
                     } /* If file in file mask. */
                  }
               }
#ifndef LINUX
            }
#endif
         }
      } /* while ((p_dir = readdir(dp)) != NULL) */

      if (gotcha == NO)
      {
         files_in_dir = dummy_files_in_dir;
         bytes_in_dir = dummy_bytes_in_dir;
         goto done;
      }
      else
      {
         rewinddir(dp);
      }
   }

   /*
    * See if we have to wait for a certain number of files or
    * total file size before we can check this directory.
    */
   if (((fra[p_de->fra_pos].accumulate != 0) ||
        (fra[p_de->fra_pos].accumulate_size != 0)) &&
       (count_files != NO))
   {
      int          gotcha = NO;
      unsigned int accumulate = 0,
                   dummy_files_in_dir = 0;
      off_t        accumulate_size = 0,
                   dummy_bytes_in_dir = 0;

      while (((p_dir = readdir(dp)) != NULL) && (gotcha == NO))
      {
         if ((p_dir->d_name[0] != '.') &&
#ifdef LINUX
             (p_dir->d_type == DT_REG) &&
#endif
             ((alfc < 1) || ((check_additional_lock_filters(p_dir->d_name)) == 0)))
         {
            (void)strcpy(work_ptr, p_dir->d_name);
#ifdef HAVE_STATX
            if (statx(0, fullname,
                      AT_STATX_SYNC_AS_STAT,
                      STATX_SIZE |
# if defined (_POSIX_SAVED_IDS) || !defined (LINUX)
                      STATX_MODE |
# endif
# ifdef _POSIX_SAVED_IDS
                      STATX_UID | STATX_ATIME |
# endif
                      STATX_MTIME,
                      &stat_buf) == -1)
#else
            if (stat(fullname, &stat_buf) < 0)
#endif
            {
               if (errno != ENOENT)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                             _("Failed to statx() `%s' : %s"),
#else
                             _("Failed to stat() `%s' : %s"),
#endif
                             fullname, strerror(errno));
               }
               continue;
            }

            if ((fra[p_de->fra_pos].ignore_file_time != 0) &&
                (fra[p_de->fra_pos].fsa_pos == -1))
            {
#ifdef HAVE_STATX
               diff_time = current_time - stat_buf.stx_mtime.tv_sec;
#else
               diff_time = current_time - stat_buf.st_mtime;
#endif
            }

#ifndef LINUX
            /* Sure it is a normal file? */
# ifdef HAVE_STATX
            if (S_ISREG(stat_buf.stx_mode))
# else
            if (S_ISREG(stat_buf.st_mode))
# endif
            {
#endif
               dummy_files_in_dir++;
#ifdef HAVE_STATX
               dummy_bytes_in_dir += stat_buf.stx_size;
#else
               dummy_bytes_in_dir += stat_buf.st_size;
#endif
               if (((fra[p_de->fra_pos].ignore_size == -1) ||
                    ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_EQUAL) &&
#ifdef HAVE_STATX
                     (fra[p_de->fra_pos].ignore_size != stat_buf.stx_size)) ||
#else
                     (fra[p_de->fra_pos].ignore_size != stat_buf.st_size)) ||
#endif
                    ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_LESS_THEN) &&
#ifdef HAVE_STATX
                     (fra[p_de->fra_pos].ignore_size < stat_buf.stx_size)) ||
#else
                     (fra[p_de->fra_pos].ignore_size < stat_buf.st_size)) ||
#endif
                    ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_GREATER_THEN) &&
#ifdef HAVE_STATX
                     (fra[p_de->fra_pos].ignore_size > stat_buf.stx_size))) &&
#else
                     (fra[p_de->fra_pos].ignore_size > stat_buf.st_size))) &&
#endif
                   ((fra[p_de->fra_pos].ignore_file_time == 0) ||
                    (fra[p_de->fra_pos].fsa_pos != -1) || /* Time+size check only local dirs! */
                     ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_EQUAL) &&
                      (fra[p_de->fra_pos].ignore_file_time != diff_time)) ||
                    ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_LESS_THEN) &&
                     (fra[p_de->fra_pos].ignore_file_time < diff_time)) ||
                    ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_GREATER_THEN) &&
                     (fra[p_de->fra_pos].ignore_file_time > diff_time))))
               {
#ifdef _POSIX_SAVED_IDS
# ifdef HAVE_STATX
                  if ((stat_buf.stx_mode & S_IROTH) ||
                      ((stat_buf.stx_gid == afd_gid) && (stat_buf.stx_mode & S_IRGRP)) ||
                      ((stat_buf.stx_uid == afd_uid) && (stat_buf.stx_mode & S_IRUSR)) ||
                      ((stat_buf.stx_mode & S_IRGRP) && (no_of_sgids > 0) &&
                       (check_sgids(stat_buf.stx_gid) == YES)))
# else
                  if ((stat_buf.st_mode & S_IROTH) ||
                      ((stat_buf.st_gid == afd_gid) && (stat_buf.st_mode & S_IRGRP)) ||
                      ((stat_buf.st_uid == afd_uid) && (stat_buf.st_mode & S_IRUSR)) ||
                      ((stat_buf.st_mode & S_IRGRP) && (no_of_sgids > 0) &&
                       (check_sgids(stat_buf.st_gid) == YES)))
# endif
#else
                  if ((eaccess(fullname, R_OK) == 0))
#endif
                  {
                     if (p_de->flag & ALL_FILES)
                     {
                        if ((fra[p_de->fra_pos].fsa_pos != -1) ||
                            (fra[p_de->fra_pos].stupid_mode == YES) ||
                            (fra[p_de->fra_pos].remove == YES) ||
                            (check_list(p_de, p_dir->d_name, &stat_buf) > -1))
                        {
                           if ((fra[p_de->fra_pos].end_character == -1) ||
#ifdef HAVE_STATX
                               (fra[p_de->fra_pos].end_character == get_last_char(fullname, stat_buf.stx_size))
#else
                               (fra[p_de->fra_pos].end_character == get_last_char(fullname, stat_buf.st_size))
#endif
                              )
                           {
                              if (fra[p_de->fra_pos].accumulate != 0)
                              {
                                 accumulate++;
                              }
                              if (fra[p_de->fra_pos].accumulate_size != 0)
                              {
#ifdef HAVE_STATX
                                 accumulate_size += stat_buf.stx_size;
#else
                                 accumulate_size += stat_buf.st_size;
#endif
                              }
                              if (((fra[p_de->fra_pos].accumulate != 0) &&
                                   (accumulate >= fra[p_de->fra_pos].accumulate)) ||
                                  ((fra[p_de->fra_pos].accumulate_size != 0) &&
                                   (accumulate_size >= fra[p_de->fra_pos].accumulate_size)))
                              {
                                 gotcha = YES;
                              }
                           }
                           else
                           {
                              if (fra[p_de->fra_pos].end_character != -1)
                              {
                                 p_de->search_time -= 5;
                              }
                           }
                        }
                     }
                     else
                     {
                        int j;

                        /* Filter out only those files we need for this directory. */
                        if (p_de->paused_dir == NULL)
                        {
                           pmatch_time = current_time;
                        }
                        else
                        {
#ifdef HAVE_STATX
                           pmatch_time = stat_buf.stx_mtime.tv_sec;
#else
                           pmatch_time = stat_buf.st_mtime;
#endif
                        }
                        for (i = 0; i < p_de->nfg; i++)
                        {
                           for (j = 0; ((i < p_de->nfg) && (j < p_de->fme[i].nfm)); j++)
                           {
                              if ((ret = pmatch(p_de->fme[i].file_mask[j],
                                                p_dir->d_name, &pmatch_time)) == 0)
                              {
                                 if ((fra[p_de->fra_pos].fsa_pos != -1) ||
                                     (fra[p_de->fra_pos].stupid_mode == YES) ||
                                     (fra[p_de->fra_pos].remove == YES) ||
                                     (check_list(p_de, p_dir->d_name, &stat_buf) > -1))
                                 {
                                    if ((fra[p_de->fra_pos].end_character == -1) ||
#ifdef HAVE_STATX
                                        (fra[p_de->fra_pos].end_character == get_last_char(fullname, stat_buf.stx_size))
#else
                                        (fra[p_de->fra_pos].end_character == get_last_char(fullname, stat_buf.st_size))
#endif
                                       )
                                    {
                                       if (fra[p_de->fra_pos].accumulate != 0)
                                       {
                                          accumulate++;
                                       }
                                       if (fra[p_de->fra_pos].accumulate_size != 0)
                                       {
#ifdef HAVE_STATX
                                          accumulate_size += stat_buf.stx_size;
#else
                                          accumulate_size += stat_buf.st_size;
#endif
                                       }
                                       if ((accumulate >= fra[p_de->fra_pos].accumulate) ||
                                           (accumulate_size >= fra[p_de->fra_pos].accumulate_size))
                                       {
                                          gotcha = YES;
                                       }
                                    }
                                    else
                                    {
                                       if (fra[p_de->fra_pos].end_character != -1)
                                       {
                                          p_de->search_time -= 5;
                                       }
                                    }
                                 }

                                 /*
                                  * Since the file is in the file pool,
                                  * it is not necessary to test all other
                                  * masks.
                                  */
                                 i = p_de->nfg;
                              } /* If file in file mask. */
                              else if (ret == 1)
                                   {
                                      /*
                                       * This file is definitely NOT wanted,
                                       * for this file group! However, the
                                       * next file group might state that it
                                       * does want this file. So only ignore
                                       * all entries for this file group!
                                       */
                                      break;
                                   }
                           } /* for (j = 0; ((i < p_de->nfg) && (j < p_de->fme[i].nfm)); j++) */
                        } /* for (i = 0; i < p_de->nfg; i++) */
                     }
                  }
               }
#ifndef LINUX
            }
#endif
         }
      } /* while ((p_dir = readdir(dp)) != NULL) */

      if (gotcha == NO)
      {
         files_in_dir = dummy_files_in_dir;
         bytes_in_dir = dummy_bytes_in_dir;
         goto done;
      }
      else
      {
         rewinddir(dp);
      }
   }

   /*
    * Now lets check the directory for valied matching files.
    */
   while ((p_dir = readdir(dp)) != NULL)
   {
      if ((p_dir->d_name[0] == '.') ||
#ifdef LINUX
          (p_dir->d_type != DT_REG) ||
#endif
          ((alfc > 0) && (check_additional_lock_filters(p_dir->d_name))))
      {
         errno = 0;
         continue;
      }

      (void)strcpy(work_ptr, p_dir->d_name);
#ifdef HAVE_STATX
      if (statx(0, fullname,
                AT_STATX_SYNC_AS_STAT,
                STATX_SIZE | STATX_MODE |
# ifdef _POSIX_SAVED_IDS
                STATX_UID |
# endif
                STATX_ATIME | STATX_MTIME,
                &stat_buf) == -1)
#else
      if (stat(fullname, &stat_buf) < 0)
#endif
      {
         if (errno != ENOENT)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                       _("Failed to statx() `%s' : %s"),
#else
                       _("Failed to stat() `%s' : %s"),
#endif
                       fullname, strerror(errno));
         }
         errno = 0;
         continue;
      }

      if ((fra[p_de->fra_pos].ignore_file_time != 0) &&
          (fra[p_de->fra_pos].fsa_pos == -1))
      {
#ifdef HAVE_STATX
         diff_time = current_time - stat_buf.stx_mtime.tv_sec;
#else
         diff_time = current_time - stat_buf.st_mtime;
#endif
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
         size_t file_name_length = strlen(p_dir->d_name);

         files_in_dir++;
#ifdef HAVE_STATX
         bytes_in_dir += stat_buf.stx_size;
#else
         bytes_in_dir += stat_buf.st_size;
#endif
         if ((count_files == NO) || /* Paused dir. */
             (fra[p_de->fra_pos].fsa_pos != -1) || /* Time+size check only local dirs! */
             (((fra[p_de->fra_pos].ignore_size == -1) ||
               ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_EQUAL) &&
#ifdef HAVE_STATX
                (fra[p_de->fra_pos].ignore_size != stat_buf.stx_size)) ||
#else
                (fra[p_de->fra_pos].ignore_size != stat_buf.st_size)) ||
#endif
               ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_LESS_THEN) &&
#ifdef HAVE_STATX
                (fra[p_de->fra_pos].ignore_size < stat_buf.stx_size)) ||
#else
                (fra[p_de->fra_pos].ignore_size < stat_buf.st_size)) ||
#endif
               ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_GREATER_THEN) &&
#ifdef HAVE_STATX
                (fra[p_de->fra_pos].ignore_size > stat_buf.stx_size))) &&
#else
                (fra[p_de->fra_pos].ignore_size > stat_buf.st_size))) &&
#endif
              ((fra[p_de->fra_pos].ignore_file_time == 0) ||
                ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_EQUAL) &&
                 (fra[p_de->fra_pos].ignore_file_time != diff_time)) ||
               ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_LESS_THEN) &&
                (fra[p_de->fra_pos].ignore_file_time < diff_time)) ||
               ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_GREATER_THEN) &&
                (fra[p_de->fra_pos].ignore_file_time > diff_time)))))
         {
#ifdef _POSIX_SAVED_IDS
# ifdef HAVE_STATX
            if ((stat_buf.stx_mode & S_IROTH) ||
                ((stat_buf.stx_gid == afd_gid) && (stat_buf.stx_mode & S_IRGRP)) ||
                ((stat_buf.stx_uid == afd_uid) && (stat_buf.stx_mode & S_IRUSR)) ||
                ((stat_buf.stx_mode & S_IRGRP) && (no_of_sgids > 0) &&
                 (check_sgids(stat_buf.stx_gid) == YES)))
# else
            if ((stat_buf.st_mode & S_IROTH) ||
                ((stat_buf.st_gid == afd_gid) && (stat_buf.st_mode & S_IRGRP)) ||
                ((stat_buf.st_uid == afd_uid) && (stat_buf.st_mode & S_IRUSR)) ||
                ((stat_buf.st_mode & S_IRGRP) && (no_of_sgids > 0) &&
                 (check_sgids(stat_buf.st_gid) == YES)))
# endif
#else
            if ((eaccess(fullname, R_OK) == 0))
#endif
            {
               if (fra[p_de->fra_pos].dir_flag & ALL_DISABLED)
               {
                  if ((fra[p_de->fra_pos].remove == YES) ||
                      (fra[p_de->fra_pos].fsa_pos != -1))
                  {
                     int gotcha;

                     if (p_de->flag & ALL_FILES)
                     {
                        gotcha = YES;
                     }
                     else
                     {
                        int j;

                        /* Filter out only those files we need for this directory. */
                        gotcha = NO;
                        if (p_de->paused_dir == NULL)
                        {
                           pmatch_time = current_time;
                        }
                        else
                        {
#ifdef HAVE_STATX
                           pmatch_time = stat_buf.stx_mtime.tv_sec;
#else
                           pmatch_time = stat_buf.st_mtime;
#endif
                        }
                        for (i = 0; i < p_de->nfg; i++)
                        {
                           for (j = 0; ((i < p_de->nfg) &&
                                        (j < p_de->fme[i].nfm)); j++)
                           {
                              if ((p_de->fme[i].file_mask[j][0] != '!') ||
                                  ((ret = pmatch(p_de->fme[i].file_mask[j],
                                                 p_dir->d_name,
                                                 &pmatch_time)) == 0))
                              {
                                 /*
                                  * This file is wanted, for this file
                                  * group! However, the next file group
                                  * might state that it does want this
                                  * file. So only ignore all entries
                                  * for this file group!
                                  */
                                 gotcha = YES;
                                 break;
                              }
                              else if (ret == 1)
                                   {
                                      /*
                                       * This file is definitely NOT wanted.
                                       * So it can be a temp file name
                                       * used for locking.
                                       */
                                      gotcha = NO;
                                      i = p_de->nfg;
                                   }
                           }
                        } /* for (i = 0; i < p_de->nfg; i++) */
                     }

                     if (gotcha == YES)
                     {
                        if (unlink(fullname) == -1)
                        {
                           if (errno != ENOENT)
                           {
                              system_log(ERROR_SIGN, __FILE__, __LINE__,
                                         _("Failed to unlink() file `%s' : %s"),
                                         fullname, strerror(errno));
                           }
                        }
                        else
                        {
#ifdef _DELETE_LOG
                           size_t        dl_real_size;
#endif
#ifdef _DISTRIBUTION_LOG
                           unsigned int  dummy_job_id,
                                         *p_dummy_job_id;
                           unsigned char dummy_proc_cycles;

                           dummy_job_id = 0;
                           p_dummy_job_id = &dummy_job_id;
                           dummy_proc_cycles = 0;
                           dis_log(DISABLED_DIS_TYPE, current_time, p_de->dir_id,
                                   0, p_dir->d_name, file_name_length,
# ifdef HAVE_STATX
                                   stat_buf.stx_size, 1, &p_dummy_job_id,
# else
                                   stat_buf.st_size, 1, &p_dummy_job_id,
# endif
                                   &dummy_proc_cycles, 1);
#endif
#ifdef _DELETE_LOG
                           (void)my_strncpy(dl.file_name, p_dir->d_name,
                                            file_name_length + 1);
                           (void)snprintf(dl.host_name,
                                          MAX_HOSTNAME_LENGTH + 4 + 1,
                                          "%-*s %03x",
                                          MAX_HOSTNAME_LENGTH, "-",
                                          DELETE_HOST_DISABLED);
# ifdef HAVE_STATX
                           *dl.file_size = stat_buf.stx_size;
# else
                           *dl.file_size = stat_buf.st_size;
# endif
                           *dl.dir_id = p_de->dir_id;
                           *dl.job_id = 0;
                           *dl.input_time = current_time;
                           *dl.split_job_counter = 0;
                           *dl.unique_number = 0;
                           *dl.file_name_length = file_name_length;
                           dl_real_size = *dl.file_name_length + dl.size +
                                          snprintf((dl.file_name + *dl.file_name_length + 1),
                                                   MAX_FILENAME_LENGTH + 1,
                                                   "%s%c(%s %d)",
                                                   DIR_CHECK, SEPARATOR_CHAR,
                                                   __FILE__, __LINE__);
                           if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                           {
                              system_log(ERROR_SIGN, __FILE__, __LINE__,
                                         _("write() error : %s"),
                                         strerror(errno));
                           }
#endif
                           files_in_dir--;
#ifdef HAVE_STATX
                           bytes_in_dir -= stat_buf.stx_size;
#else
                           bytes_in_dir -= stat_buf.st_size;
#endif
                        }
                     }
                  }
               }
               else
               {
                  int gotcha;

                  if (p_de->flag & ALL_FILES)
                  {
                     gotcha = YES;
                  }
                  else
                  {
                     int j;

                     /* Filter out only those files we need for this directory. */
                     gotcha = NO;
                     if (p_de->paused_dir == NULL)
                     {
                        pmatch_time = current_time;
                     }
                     else
                     {
#ifdef HAVE_STATX
                        pmatch_time = stat_buf.stx_mtime.tv_sec;
#else
                        pmatch_time = stat_buf.st_mtime;
#endif
                     }
                     for (i = 0; i < p_de->nfg; i++)
                     {
                        for (j = 0; ((i < p_de->nfg) &&
                                     (j < p_de->fme[i].nfm)); j++)
                        {
                           if ((ret = pmatch(p_de->fme[i].file_mask[j],
                                             p_dir->d_name, &pmatch_time)) == 0)
                           {
                              gotcha = YES;
                              i = p_de->nfg;
                           } /* If file in file mask. */
                           else if (ret == 1)
                                {
                                   /*
                                    * This file is definitely NOT wanted, for
                                    * this file group! However, the next file
                                    * group might state that it does want this
                                    * file. So only ignore all entries for
                                    * this file group!
                                    */
                                   break;
                                }
                        }
                     } /* for (i = 0; i < p_de->nfg; i++) */
                  }

                  if (gotcha == YES)
                  {
                     int rl_pos = -1;
#ifdef WITH_DUP_CHECK
                     int is_duplicate = NO;

                     if ((count_files == NO) || /* Paused directories may */
                         (count_files == PAUSED_REMOTE) || /* not be      */
                                                /* checked again!!!       */
                         (fra[p_de->fra_pos].dup_check_timeout == 0L) ||
                         (((is_duplicate = isdup(fullname, NULL,
# ifdef HAVE_STATX
                                                 stat_buf.stx_size,
# else
                                                 stat_buf.st_size,
# endif
                                                 p_de->dir_id,
                                                 fra[p_de->fra_pos].dup_check_timeout,
                                                 fra[p_de->fra_pos].dup_check_flag,
                                                 NO,
# ifdef HAVE_HW_CRC32
                                                 have_hw_crc32,
# endif
                                                 YES, NO)) == NO) ||
                          (((fra[p_de->fra_pos].dup_check_flag & DC_DELETE) == 0) &&
                           ((fra[p_de->fra_pos].dup_check_flag & DC_STORE) == 0))))
                     {
                        if ((is_duplicate == YES) &&
                            (fra[p_de->fra_pos].dup_check_flag & DC_WARN))
                        {
                           receive_log(WARN_SIGN, NULL, 0, current_time,
                                       _("File %s is duplicate. @%x"),
                                       p_dir->d_name, p_de->dir_id);
                        }
#endif
                     if ((fra[p_de->fra_pos].fsa_pos != -1) ||
                         (fra[p_de->fra_pos].stupid_mode == YES) ||
                         (fra[p_de->fra_pos].remove == YES) ||
                         ((rl_pos = check_list(p_de, p_dir->d_name, &stat_buf)) > -1))
                     {
                        if ((fra[p_de->fra_pos].end_character == -1) ||
#ifdef HAVE_STATX
                            (fra[p_de->fra_pos].end_character == get_last_char(fullname, stat_buf.stx_size))
#else
                            (fra[p_de->fra_pos].end_character == get_last_char(fullname, stat_buf.st_size))
#endif
                           )
                        {
                           int what_done;

                           if (tmp_file_dir[0] == '\0')
                           {
#ifdef MULTI_FS_SUPPORT
                              (void)strcpy(tmp_file_dir, ewl[p_de->ewl_pos].afd_file_dir);
                              (void)strcpy(tmp_file_dir + ewl[p_de->ewl_pos].afd_file_dir_length,
                                           AFD_TMP_DIR);
                              ptr = tmp_file_dir + ewl[p_de->ewl_pos].afd_file_dir_length + AFD_TMP_DIR_LENGTH;
#else
                              (void)strcpy(tmp_file_dir, afd_file_dir);
                              (void)strcpy(tmp_file_dir + afd_file_dir_length,
                                           AFD_TMP_DIR);
                              ptr = tmp_file_dir + afd_file_dir_length + AFD_TMP_DIR_LENGTH;
#endif
                              *(ptr++) = '/';
                              *ptr = '\0';

                              /* Create a unique name. */
                              next_counter_no_lock(amg_counter, MAX_MSG_PER_SEC);
                              *unique_number = *amg_counter;
                              if ((ret = create_name(tmp_file_dir,
#ifdef MULTI_FS_SUPPORT
                                                     ewl[p_de->ewl_pos].afd_file_dir_length + AFD_TMP_DIR_LENGTH,
#else
                                                     afd_file_dir_length + AFD_TMP_DIR_LENGTH,
#endif
                                                     NO_PRIORITY,
                                                     current_time, p_de->dir_id,
                                                     &split_job_counter,
                                                     unique_number, ptr,
                                                     MAX_PATH_LENGTH - (ptr - tmp_file_dir),
                                                     -1)) < 0)
                              {
                                 if (errno == ENOSPC)
                                 {
                                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                                               _("DISK FULL!!! Will retry in %d second interval."),
                                               DISK_FULL_RESCAN_TIME);

                                    while (errno == ENOSPC)
                                    {
                                       (void)sleep(DISK_FULL_RESCAN_TIME);
                                       errno = 0;
                                       next_counter_no_lock(amg_counter,
                                                            MAX_MSG_PER_SEC);
                                       *unique_number = *amg_counter;
                                       if ((ret = create_name(tmp_file_dir,
#ifdef MULTI_FS_SUPPORT
                                                              ewl[p_de->ewl_pos].afd_file_dir_length + AFD_TMP_DIR_LENGTH,
#else
                                                              afd_file_dir_length + AFD_TMP_DIR_LENGTH,
#endif
                                                              NO_PRIORITY,
                                                              current_time,
                                                              p_de->dir_id,
                                                              &split_job_counter,
                                                              unique_number,
                                                              ptr,
                                                              MAX_PATH_LENGTH - (ptr - tmp_file_dir),
                                                              -1)) < 0)
                                       {
                                          if (errno != ENOSPC)
                                          {
                                             system_log(FATAL_SIGN, __FILE__, __LINE__,
                                                        _("Failed to create a unique name in %s [%d] : %s"),
                                                        tmp_file_dir, ret,
                                                        strerror(errno));
                                             exit(INCORRECT);
                                          }
                                       }
                                    }
                                    system_log(INFO_SIGN, __FILE__, __LINE__,
                                               _("Continuing after disk was full."));

                                    /*
                                     * If the disk is full, lets stop copying/moving
                                     * files so we can send data as quickly as possible
                                     * to get more free disk space.
                                     */
                                    full_scan = NO;
                                    break;
                                 }
                                 else
                                 {
                                    system_log(FATAL_SIGN, __FILE__, __LINE__,
                                               _("Failed to create a unique name in %s [%d] : %s"),
                                               tmp_file_dir, ret,
                                               strerror(errno));
                                    exit(INCORRECT);
                                 }
                              }

                              while (*ptr != '\0')
                              {
                                 ptr++;
                              }
                              *(ptr++) = '/';
                              *ptr = '\0';
                           } /* if (tmp_file_dir[0] == '\0') */

                           /* Generate name for the new file. */
                           (void)strcpy(ptr, p_dir->d_name);

#if defined (_MAINTAINER_LOG) && defined (SHOW_FILE_MOVING)
                           maintainer_log(DEBUG_SIGN, NULL, 0,
                                          "check_files() [%s %d]: `%s' -> `%s'",
                                          caller, line, fullname, tmp_file_dir);
#endif
                           if ((fra[p_de->fra_pos].remove == YES) ||
                               (count_files == NO) ||  /* Paused directory. */
                               (fra[p_de->fra_pos].protocol != LOC))
                           {
                              if ((p_de->flag & IN_SAME_FILESYSTEM) &&
                                  ((fra[p_de->fra_pos].dir_options & DO_NOT_MOVE) == 0))
                              {
                                 ret = move_file(fullname, tmp_file_dir);
                                 if (ret == DATA_COPIED)
                                 {
                                    what_done = g_what_done = DATA_COPIED;
                                    ret = SUCCESS;
                                 }
                                 else
                                 {
                                    what_done = DATA_MOVED;
#if defined (LINUX) && defined (DIR_CHECK_CAP_CHOWN)
                                    if ((hardlinks_protected_set == YES) &&
                                        ((can_do_chown == YES) ||
                                         (can_do_chown == NEITHER)) &&
# ifdef HAVE_STATX
                                        (stat_buf.stx_uid != afd_uid)
# else
                                        (stat_buf.st_uid != afd_uid)
# endif
                                       )
                                    {
                                       if (can_do_chown == NEITHER)
                                       {
                                          cap_value_t cap_value[1];

                                          cap_value[0] = CAP_CHOWN;
                                          cap_set_flag(caps, CAP_EFFECTIVE, 1,
                                                       cap_value, CAP_SET);
                                          if (cap_set_proc(caps) == -1)
                                          {
                                             receive_log(WARN_SIGN, __FILE__,
                                                         __LINE__, current_time,
                                                         "cap_set_proc() error : %s",
                                                         strerror(errno));
                                             can_do_chown = PERMANENT_INCORRECT;
                                          }
                                          else
                                          {
                                             can_do_chown = YES;
                                          }
                                       }
                                       if (can_do_chown == YES)
                                       {
                                          if (chown(tmp_file_dir, afd_uid, -1) == -1)
                                          {
                                             receive_log(WARN_SIGN, __FILE__,
                                                         __LINE__, current_time,
                                                         "chown() error : %s",
                                                         strerror(errno));
                                          }
                                       }
                                       else
                                       {
                                          receive_log(WARN_SIGN, __FILE__,
                                                      __LINE__, current_time,
                                                      "chown of %s is not possible (can_do_chown=%d)",
                                                      tmp_file_dir,
                                                      can_do_chown);
                                       }
                                    }
#endif
                                 }
                              }
                              else
                              {
                                 what_done = g_what_done = DATA_COPIED;
                                 if ((ret = copy_file(fullname, tmp_file_dir,
                                                      &stat_buf)) == SUCCESS)
                                 {
                                    if ((ret = unlink(fullname)) == -1)
                                    {
                                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                  _("Failed to unlink() file `%s' : %s"),
                                                  fullname, strerror(errno));

                                       if (errno == ENOENT)
                                       {
                                          ret = SUCCESS;
                                       }
                                       else
                                       {
                                          /*
                                           * Delete target file otherwise we
                                           * might end up in an endless loop.
                                           */
                                          (void)unlink(tmp_file_dir);
                                       }
                                    }
                                 }
                              }
                           }
                           else
                           {
                              /* Leave original files in place. */
                              ret = copy_file(fullname, tmp_file_dir, &stat_buf);
                              what_done = g_what_done = DATA_COPIED;
                           }
                           if (ret != SUCCESS)
                           {
                              char reason_str[23];

                              if (errno == ENOENT)
                              {
                                 int  tmp_errno = errno;
                                 char tmp_char = *ptr;

                                 *ptr = '\0';
                                 if ((access(fullname, F_OK) == -1) &&
                                     (errno == ENOENT))
                                 {
                                    (void)strcpy(reason_str,
                                                 "(source missing) ");
                                 }
                                 else if ((access(tmp_file_dir, F_OK) == -1) &&
                                          (errno == ENOENT))
                                      {
                                         (void)strcpy(reason_str,
                                                      "(destination missing) ");
                                      }
                                      else
                                      {
                                         reason_str[0] = '\0';
                                      }
                                 *ptr = tmp_char;
                                 errno = tmp_errno;
                              }
                              else
                              {
                                 reason_str[0] = '\0';
                              }
                              receive_log(ERROR_SIGN, __FILE__, __LINE__, current_time,
                                          _("Failed (%d) to %s file `%s' to `%s' %s: %s @%x"),
                                          ret,
                                          (what_done == DATA_MOVED) ? "move" : "copy",
                                          fullname, tmp_file_dir, reason_str,
                                          strerror(errno), p_de->dir_id);
                              lock_region_w(fra_fd,
#ifdef LOCK_DEBUG
                                            (char *)&fra[p_de->fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                                            (char *)&fra[p_de->fra_pos].error_counter - (char *)fra);
#endif
                              fra[p_de->fra_pos].error_counter += 1;
                              if ((fra[p_de->fra_pos].error_counter >= fra[p_de->fra_pos].max_errors) &&
                                  ((fra[p_de->fra_pos].dir_flag & DIR_ERROR_SET) == 0))
                              {
                                 fra[p_de->fra_pos].dir_flag |= DIR_ERROR_SET;
                                 SET_DIR_STATUS(fra[p_de->fra_pos].dir_flag,
                                                current_time,
                                                fra[p_de->fra_pos].start_event_handle,
                                                fra[p_de->fra_pos].end_event_handle,
                                                fra[p_de->fra_pos].dir_status);
                                 error_action(p_de->alias, "start",
                                              DIR_ERROR_ACTION,
                                              receive_log_fd);
                                 event_log(0L, EC_DIR, ET_EXT, EA_ERROR_START, "%s", p_de->alias);
                              }
                              unlock_region(fra_fd,
#ifdef LOCK_DEBUG
                                            (char *)&fra[p_de->fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                                            (char *)&fra[p_de->fra_pos].error_counter - (char *)fra);
#endif
                              set_error_counter = YES;

#ifdef WITH_DUP_CHECK
                              /*
                               * We have already stored the CRC value for
                               * this file but failed pick up the file.
                               * So we must remove the CRC value!
                               */
                              if ((fra[p_de->fra_pos].dup_check_timeout > 0L) &&
                                  (is_duplicate == NO))
                              {
                                 (void)isdup(fullname, NULL,
# ifdef HAVE_STATX
                                             stat_buf.stx_size,
# else
                                             stat_buf.st_size,
# endif
                                             p_de->dir_id,
                                             fra[p_de->fra_pos].dup_check_timeout,
                                             fra[p_de->fra_pos].dup_check_flag,
                                             YES,
# ifdef HAVE_HW_CRC32
                                             have_hw_crc32,
# endif
                                             YES, NO);
                              }
#endif
                           }
                           else
                           {
                              /* Store file name of file that has just been  */
                              /* moved. So one does not always have to walk  */
                              /* with the directory pointer through all      */
                              /* files every time we want to know what files */
                              /* are available. Hope this will reduce the    */
                              /* system time of the process dir_check.       */
                              check_file_pool_mem(files_copied + 1);
                              if (rl_pos > -1)
                              {
                                 p_de->rl[rl_pos].retrieved = YES;
                              }
                              file_length_pool[files_copied] = file_name_length;
                              (void)memcpy(file_name_pool[files_copied],
                                           p_dir->d_name,
                                           (size_t)(file_length_pool[files_copied] + 1));
#ifdef HAVE_STATX
                              file_mtime_pool[files_copied] = stat_buf.stx_mtime.tv_sec;
                              file_size_pool[files_copied] = stat_buf.stx_size;
#else
                              file_mtime_pool[files_copied] = stat_buf.st_mtime;
                              file_size_pool[files_copied] = stat_buf.st_size;
#endif

#ifdef _INPUT_LOG
                              if ((count_files == YES) ||
                                  (count_files == PAUSED_REMOTE))
                              {
                                 /* Log the file name in the input log. */
                                 (void)memcpy(il_file_name, p_dir->d_name,
                                              (size_t)(file_length_pool[files_copied] + 1));
# ifdef HAVE_STATX
                                 *il_file_size = stat_buf.stx_size;
# else
                                 *il_file_size = stat_buf.st_size;
# endif
                                 *il_time = current_time;
                                 *il_dir_number = p_de->dir_id;
                                 *il_unique_number = *unique_number;
                                 il_real_size = (size_t)file_length_pool[files_copied] + il_size;
                                 if (write(il_fd, il_data, il_real_size) != il_real_size)
                                 {
                                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                                               _("write() error : %s"),
                                               strerror(errno));

                                    /*
                                     * Since the input log is not critical, no
                                     * need to exit here.
                                     */
                                 }
                              }
#endif

#ifdef HAVE_STATX
                              *total_file_size += stat_buf.stx_size;
#else
                              *total_file_size += stat_buf.st_size;
#endif
                              if ((++files_copied >= fra[p_de->fra_pos].max_copied_files) ||
                                  (*total_file_size >= fra[p_de->fra_pos].max_copied_file_size))
                              {
                                 full_scan = NO;
                                 break;
                              }
                           }
                        }
                        else
                        {
                           if (fra[p_de->fra_pos].end_character != -1)
                           {
                              p_de->search_time -= 5;
                           }
                        }
                     }
#ifdef WITH_DUP_CHECK
                     }
                     else
                     {
                        if (is_duplicate == YES)
                        {
# ifdef _INPUT_LOG
                           if ((count_files == YES) ||
                               (count_files == PAUSED_REMOTE))
                           {
                              /* Log the file name in the input log. */
                              (void)strcpy(il_file_name, p_dir->d_name);
# ifdef HAVE_STATX
                              *il_file_size = stat_buf.stx_size;
# else
                              *il_file_size = stat_buf.st_size;
# endif
                              *il_time = current_time;
                              *il_dir_number = p_de->dir_id;
                              *il_unique_number = *unique_number;
                              il_real_size = file_name_length + il_size;
                              if (write(il_fd, il_data, il_real_size) != il_real_size)
                              {
                                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                                            _("write() error : %s"),
                                            strerror(errno));

                                 /*
                                  * Since the input log is not critical, no
                                  * need to exit here.
                                  */
                              }
                           }
# endif
                           if (fra[p_de->fra_pos].dup_check_flag & DC_DELETE)
                           {
                              if (unlink(fullname) == -1)
                              {
                                 system_log(WARN_SIGN, __FILE__, __LINE__,
                                            _("Failed to unlink() `%s' : %s"),
                                            fullname, strerror(errno));
                              }
                              else
                              {
# ifdef _DELETE_LOG
                                 size_t        dl_real_size;
# endif
# ifdef _DISTRIBUTION_LOG
                                 unsigned int  dummy_job_id,
                                               *p_dummy_job_id;
                                 unsigned char dummy_proc_cycles;

                                 dummy_job_id = 0;
                                 p_dummy_job_id = &dummy_job_id;
                                 dummy_proc_cycles = 0;
                                 dis_log(DUPCHECK_DIS_TYPE, current_time,
                                         p_de->dir_id, *unique_number,
                                         p_dir->d_name, file_name_length,
#  ifdef HAVE_STATX
                                         stat_buf.stx_size, 1, &p_dummy_job_id,
#  else
                                         stat_buf.st_size, 1, &p_dummy_job_id,
#  endif
                                         &dummy_proc_cycles, 1);
# endif
# ifdef _DELETE_LOG
                                 (void)my_strncpy(dl.file_name, p_dir->d_name,
                                                  file_name_length + 1);
                                 (void)snprintf(dl.host_name,
                                                MAX_HOSTNAME_LENGTH + 4 + 1,
                                                "%-*s %03x",
                                                MAX_HOSTNAME_LENGTH, "-",
                                                DUP_INPUT);
#  ifdef HAVE_STATX
                                 *dl.file_size = stat_buf.stx_size;
#  else
                                 *dl.file_size = stat_buf.st_size;
#  endif
                                 *dl.dir_id = p_de->dir_id;
                                 *dl.job_id = 0;
                                 *dl.input_time = current_time;
                                 *dl.split_job_counter = split_job_counter;
                                 *dl.unique_number = *unique_number;
                                 *dl.file_name_length = file_name_length;
                                 dl_real_size = *dl.file_name_length + dl.size +
                                                snprintf((dl.file_name + *dl.file_name_length + 1),
                                                         MAX_FILENAME_LENGTH + 1,
                                                         "%s%c(%s %d)",
                                                         DIR_CHECK,
                                                         SEPARATOR_CHAR,
                                                         __FILE__, __LINE__);
                                 if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                                 {
                                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                                               _("write() error : %s"),
                                               strerror(errno));
                                 }
# endif
                                 files_in_dir--;
# ifdef HAVE_STATX
                                 bytes_in_dir -= stat_buf.stx_size;
# else
                                 bytes_in_dir -= stat_buf.st_size;
# endif
                              }
                           }
                           else if (fra[p_de->fra_pos].dup_check_flag & DC_STORE)
                                {
                                   char *p_end,
                                        save_dir[MAX_PATH_LENGTH];

                                   p_end = save_dir +
                                           snprintf(save_dir, MAX_PATH_LENGTH,
                                                    "%s%s%s/%x/",
                                                    p_work_dir, AFD_FILE_DIR,
                                                    STORE_DIR, p_de->dir_id);
                                   if ((mkdir(save_dir, DIR_MODE) == -1) &&
                                       (errno != EEXIST))
                                   {
                                      system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                 _("Failed to mkdir() `%s' : %s"),
                                                 save_dir, strerror(errno));
                                      (void)unlink(fullname);
                                   }
                                   else
                                   {
                                      (void)strcpy(p_end, p_dir->d_name);
                                      if (rename(fullname, save_dir) == -1)
                                      {
                                         system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                    _("Failed to rename() `%s' to `%s' : %s"),
                                                    fullname, save_dir,
                                                    strerror(errno));
                                         (void)unlink(fullname);
                                      }
                                   }
                                   files_in_dir--;
# ifdef HAVE_STATX
                                   bytes_in_dir -= stat_buf.stx_size;
# else
                                   bytes_in_dir -= stat_buf.st_size;
# endif
                                }
                           if (fra[p_de->fra_pos].dup_check_flag & DC_WARN)
                           {
                              receive_log(WARN_SIGN, NULL, 0, current_time,
                                          _("File %s is duplicate. @%x"),
                                          p_dir->d_name, p_de->dir_id);
                           }
                        }
                     }
#endif /* WITH_DUP_CHECK */
                  }
                  else /* gotcha == NO */
                  {
                     if (fra[p_de->fra_pos].delete_files_flag & UNKNOWN_FILES)
                     {
#ifdef HAVE_STATX
                        diff_time = current_time - stat_buf.stx_mtime.tv_sec;
#else
                        diff_time = current_time - stat_buf.st_mtime;
#endif
                        if ((fra[p_de->fra_pos].unknown_file_time == -2) ||
                            ((diff_time > fra[p_de->fra_pos].unknown_file_time) &&
                             (diff_time > DEFAULT_TRANSFER_TIMEOUT)))
                        {
                           if (unlink(fullname) == -1)
                           {
                              if (errno != ENOENT)
                              {
                                 system_log(WARN_SIGN, __FILE__, __LINE__,
                                            _("Failed to unlink() `%s' : %s"),
                                            fullname, strerror(errno));
                              }
                           }
                           else
                           {
#ifdef _DELETE_LOG
                              size_t dl_real_size;

                              (void)my_strncpy(dl.file_name, p_dir->d_name,
                                               file_name_length + 1);
                              (void)snprintf(dl.host_name,
                                             MAX_HOSTNAME_LENGTH + 4 + 1,
                                             "%-*s %03x",
                                             MAX_HOSTNAME_LENGTH, "-",
                                             (fra[p_de->fra_pos].in_dc_flag & UNKNOWN_FILES_IDC) ?  DEL_UNKNOWN_FILE : DEL_UNKNOWN_FILE_GLOB);
# ifdef HAVE_STATX
                              *dl.file_size = stat_buf.stx_size;
# else
                              *dl.file_size = stat_buf.st_size;
# endif
                              *dl.dir_id = p_de->dir_id;
                              *dl.job_id = 0;
                              *dl.input_time = 0L;
                              *dl.split_job_counter = 0;
                              *dl.unique_number = 0;
                              *dl.file_name_length = file_name_length;
                              dl_real_size = *dl.file_name_length + dl.size +
                                             snprintf((dl.file_name + *dl.file_name_length + 1),
                                                      MAX_FILENAME_LENGTH + 1,
# if SIZEOF_TIME_T == 4
                                                      "%s%c>%ld (%s %d)",
# else
                                                      "%s%c>%lld (%s %d)",
# endif
                                                      DIR_CHECK, SEPARATOR_CHAR,
                                                      (pri_time_t)diff_time,
                                                      __FILE__, __LINE__);
                              if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                              {
                                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                                            _("write() error : %s"),
                                            strerror(errno));
                              }
#endif
                              files_in_dir--;
#ifdef HAVE_STATX
                              bytes_in_dir -= stat_buf.stx_size;
#else
                              bytes_in_dir -= stat_buf.st_size;
#endif
                           }
                        }
                     }
                  }
               }
            }
         }
         else
         {
            if ((fra[p_de->fra_pos].delete_files_flag & UNKNOWN_FILES) &&
                ((fra[p_de->fra_pos].ignore_size != -1) ||
                 ((fra[p_de->fra_pos].ignore_file_time != 0) &&
                  ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_GREATER_THEN) ||
                   (fra[p_de->fra_pos].gt_lt_sign & IFTIME_EQUAL)))) &&
#ifdef HAVE_STATX
                ((current_time - stat_buf.stx_mtime.tv_sec) > fra[p_de->fra_pos].unknown_file_time)
#else
                ((current_time - stat_buf.st_mtime) > fra[p_de->fra_pos].unknown_file_time)
#endif
               )
            {
               if (unlink(fullname) == -1)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             _("Failed to unlink() `%s' : %s"),
                             fullname, strerror(errno));
               }
               else
               {
#ifdef _DELETE_LOG
                  size_t dl_real_size;

                  (void)my_strncpy(dl.file_name, p_dir->d_name,
                                   file_name_length + 1);
                  (void)snprintf(dl.host_name, MAX_HOSTNAME_LENGTH + 4 + 1,
                                 "%-*s %03x",
                                 MAX_HOSTNAME_LENGTH, "-",
                                 (fra[p_de->fra_pos].in_dc_flag & UNKNOWN_FILES_IDC) ?  DEL_UNKNOWN_FILE : DEL_UNKNOWN_FILE_GLOB);
# ifdef HAVE_STATX
                  *dl.file_size = stat_buf.stx_size;
# else
                  *dl.file_size = stat_buf.st_size;
# endif
                  *dl.dir_id = p_de->dir_id;
                  *dl.job_id = 0;
                  *dl.input_time = 0L;
                  *dl.split_job_counter = 0;
                  *dl.unique_number = 0;
                  *dl.file_name_length = file_name_length;
                  dl_real_size = *dl.file_name_length + dl.size +
                                 snprintf((dl.file_name + *dl.file_name_length + 1),
                                          MAX_FILENAME_LENGTH + 1,
# if SIZEOF_TIME_T == 4
                                          "%s%c>%ld (%s %d)",
# else
                                          "%s%c>%lld (%s %d)",
# endif
                                          DIR_CHECK, SEPARATOR_CHAR,
# ifdef HAVE_STATX
                                          (pri_time_t)(current_time - stat_buf.stx_mtime.tv_sec),
# else
                                          (pri_time_t)(current_time - stat_buf.st_mtime),
# endif
                                          __FILE__, __LINE__);
                  if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                _("write() error : %s"), strerror(errno));
                  }
#endif
                  files_in_dir--;
#ifdef HAVE_STATX
                  bytes_in_dir -= stat_buf.stx_size;
#else
                  bytes_in_dir -= stat_buf.st_size;
#endif
               }
            }
            else if (((fra[p_de->fra_pos].ignore_file_time != 0) &&
                      (((fra[p_de->fra_pos].gt_lt_sign & IFTIME_LESS_THEN) &&
                        (diff_time <= fra[p_de->fra_pos].ignore_file_time)) ||
                       ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_EQUAL) &&
                        (diff_time < fra[p_de->fra_pos].ignore_file_time)))) ||
                     ((fra[p_de->fra_pos].ignore_size != -1) &&
                      ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_LESS_THEN) ||
                       (fra[p_de->fra_pos].gt_lt_sign & ISIZE_EQUAL)) &&
#ifdef HAVE_STATX
                      (stat_buf.stx_size < fra[p_de->fra_pos].ignore_size)
#else
                      (stat_buf.st_size < fra[p_de->fra_pos].ignore_size)
#endif
                     ))
                 {
                    *rescan_dir = YES;
                 }
         }
      } /* if (S_ISREG(stat_buf.st_mode)) */
      errno = 0;
   } /* while ((p_dir = readdir(dp)) != NULL) */

done:

   /* When using readdir() it can happen that it always returns     */
   /* NULL, due to some error. We want to know if this is the case. */
   if (errno == EBADF)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 _("Failed to readdir() `%s' : %s"), fullname, strerror(errno));
   }

#if defined (LINUX) && defined (DIR_CHECK_CAP_CHOWN)
   /* Give up chown capability. */
   if (can_do_chown == YES)
   {
      cap_value_t cap_value[1];

      cap_value[0] = CAP_CHOWN;
      cap_set_flag(caps, CAP_EFFECTIVE, 1, cap_value, CAP_CLEAR);
      if (cap_set_proc(caps) == -1)
      {
         receive_log(WARN_SIGN, __FILE__, __LINE__, current_time,
                     "cap_set_proc() error : %s", strerror(errno));
         can_do_chown = NO;
      }
      else
      {
         can_do_chown = NEITHER;
      }
   }
#endif

   /* So that we return only the directory name where */
   /* the files have been stored.                    */
   if (ptr != NULL)
   {
      *ptr = '\0';
   }

   /* Don't forget to close the directory. */
   if (closedir(dp) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed  to closedir() `%s' : %s"),
                 src_file_path, strerror(errno));
   }

#ifdef WITH_DUP_CHECK
   isdup_detach();
#endif

   if (p_de->rl_fd > -1)
   {
      *work_ptr = '\0';
      rm_removed_files(p_de, full_scan, fullname);
      if (close(p_de->rl_fd) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Failed to close() ls_data file for %s : %s"),
                    fra[p_de->fra_pos].dir_alias, strerror(errno));
      }
      p_de->rl_fd = -1;
      if (p_de->rl != NULL)
      {
         ptr = (char *)p_de->rl;
         ptr -= AFD_WORD_OFFSET;
#ifdef HAVE_MMAP
         if (munmap(ptr, p_de->rl_size) == -1)
#else
         if (munmap_emu(ptr) == -1)
#endif
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("Failed to munmap() from ls_data file %s : %s"),
                       fra[p_de->fra_pos].dir_alias, strerror(errno));
         }
         p_de->rl = NULL;
      }
   }

#ifdef _WITH_PTHREAD
   if ((ret = pthread_mutex_lock(&fsa_mutex)) != 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "pthread_mutex_lock() error : %s", strerror(ret));
   }
#endif
   if ((files_copied >= fra[p_de->fra_pos].max_copied_files) ||
       (*total_file_size >= fra[p_de->fra_pos].max_copied_file_size))
   {
      if (count_files == YES)
      {
         if (fra[p_de->fra_pos].files_in_dir < files_in_dir)
         {
            fra[p_de->fra_pos].files_in_dir = files_in_dir;
         }
         if (fra[p_de->fra_pos].bytes_in_dir < bytes_in_dir)
         {
            fra[p_de->fra_pos].bytes_in_dir = bytes_in_dir;
         }
      }
      if ((fra[p_de->fra_pos].dir_flag & MAX_COPIED) == 0)
      {
         fra[p_de->fra_pos].dir_flag |= MAX_COPIED;
      }
   }
   else
   {
      if (count_files == YES)
      {
         fra[p_de->fra_pos].files_in_dir = files_in_dir;
         fra[p_de->fra_pos].bytes_in_dir = bytes_in_dir;
      }
      if ((fra[p_de->fra_pos].dir_flag & MAX_COPIED))
      {
         fra[p_de->fra_pos].dir_flag &= ~MAX_COPIED;
      }
#ifdef WITH_INOTIFY
      if (fra[p_de->fra_pos].dir_flag & INOTIFY_NEEDS_SCAN)
      {
         fra[p_de->fra_pos].dir_flag &= ~INOTIFY_NEEDS_SCAN;
      }
#endif
   }

   if (files_copied > 0)
   {
      if ((count_files == YES) || (count_files == PAUSED_REMOTE))
      {
         fra[p_de->fra_pos].files_received += files_copied;
         fra[p_de->fra_pos].bytes_received += *total_file_size;
         fra[p_de->fra_pos].last_retrieval = current_time;
         if (fra[p_de->fra_pos].dir_flag & INFO_TIME_REACHED)
         {
            fra[p_de->fra_pos].dir_flag &= ~INFO_TIME_REACHED;
            SET_DIR_STATUS(fra[p_de->fra_pos].dir_flag,
                           current_time,
                           fra[p_de->fra_pos].start_event_handle,
                           fra[p_de->fra_pos].end_event_handle,
                           fra[p_de->fra_pos].dir_status);
            error_action(p_de->alias, "stop", DIR_INFO_ACTION,
                         receive_log_fd);
            event_log(0L, EC_DIR, ET_AUTO, EA_INFO_TIME_UNSET, "%s",
                      fra[p_de->fra_pos].dir_alias);
         }
         if (fra[p_de->fra_pos].dir_flag & WARN_TIME_REACHED)
         {
            fra[p_de->fra_pos].dir_flag &= ~WARN_TIME_REACHED;
            SET_DIR_STATUS(fra[p_de->fra_pos].dir_flag,
                           current_time,
                           fra[p_de->fra_pos].start_event_handle,
                           fra[p_de->fra_pos].end_event_handle,
                           fra[p_de->fra_pos].dir_status);
            error_action(p_de->alias, "stop", DIR_WARN_ACTION,
                         receive_log_fd);
            event_log(0L, EC_DIR, ET_AUTO, EA_WARN_TIME_UNSET, "%s",
                      fra[p_de->fra_pos].dir_alias);
         }
         if (g_what_done == DATA_COPIED)
         {
            receive_log(INFO_SIGN, NULL, 0, current_time,
#if SIZEOF_OFF_T == 4
                        _("Received %d files with %ld bytes. {C} @%x"),
#else
                        _("Received %d files with %lld bytes. {C} @%x"),
#endif
                        files_copied, (pri_off_t)*total_file_size, p_de->dir_id);
         }
         else
         {
            receive_log(INFO_SIGN, NULL, 0, current_time,
#if SIZEOF_OFF_T == 4
                        _("Received %d files with %ld bytes. @%x"),
#else
                        _("Received %d files with %lld bytes. @%x"),
#endif
                        files_copied, (pri_off_t)*total_file_size, p_de->dir_id);
         }
      }
      else
      {
         ABS_REDUCE_QUEUE(p_de->fra_pos, files_copied, *total_file_size);
      }
   }
#ifdef REPORT_EMPTY_DIR_SCANS
   else
   {
      if ((count_files == YES) || (count_files == PAUSED_REMOTE))
      {
         receive_log(INFO_SIGN, NULL, 0, current_time,
                     _("Received 0 files with 0 bytes. @%x"), p_de->dir_id);
      }
   }
#endif
#ifdef _WITH_PTHREAD
   if ((ret = pthread_mutex_unlock(&fsa_mutex)) != 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("pthread_mutex_unlock() error : %s"), strerror(ret));
   }
#endif

   if ((set_error_counter == NO) && (fra[p_de->fra_pos].error_counter > 0) &&
       (fra[p_de->fra_pos].fsa_pos == -1))
   {
      lock_region_w(fra_fd,
#ifdef LOCK_DEBUG
                    (char *)&fra[p_de->fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                    (char *)&fra[p_de->fra_pos].error_counter - (char *)fra);
#endif
      fra[p_de->fra_pos].error_counter = 0;
      if (fra[p_de->fra_pos].dir_flag & DIR_ERROR_SET)
      {
         fra[p_de->fra_pos].dir_flag &= ~DIR_ERROR_SET;
         SET_DIR_STATUS(fra[p_de->fra_pos].dir_flag,
                        current_time,
                        fra[p_de->fra_pos].start_event_handle,
                        fra[p_de->fra_pos].end_event_handle,
                        fra[p_de->fra_pos].dir_status);
         error_action(p_de->alias, "stop", DIR_ERROR_ACTION,
                      receive_log_fd);
         event_log(0L, EC_DIR, ET_EXT, EA_ERROR_END, "%s", p_de->alias);
      }
      unlock_region(fra_fd,
#ifdef LOCK_DEBUG
                    (char *)&fra[p_de->fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                    (char *)&fra[p_de->fra_pos].error_counter - (char *)fra);
#endif
   }

   return(files_copied);
}


#ifdef _POSIX_SAVED_IDS
/*++++++++++++++++++++++++++++ check_sgids() ++++++++++++++++++++++++++++*/
static int
check_sgids(gid_t file_gid)
{
   int i;

   for (i = 0; i < no_of_sgids; i++)
   {
      if (file_gid == afd_sgids[i])
      {
         return(YES);
      }
   }
   return(NO);
}
#endif
