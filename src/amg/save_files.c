/*
 *  save_files.c - Part of AFD, an automatic file distribution program.
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
 **   save_files - saves files from user directory
 **
 ** SYNOPSIS
 **   int save_files(char                   *src_path,
 **                  char                   *dest_path,
 **                  time_t                 current_time,
 **                  unsigned int           age_limit,
 **                  struct directory_entry *p_de,
 **                  struct instant_db      *p_db,
 **                  int                    pos_in_fm,
 **                  int                    no_of_files,
 **                  char                   link_flag
 **                  int                    time_job)
 **
 ** DESCRIPTION
 **   When the queue has been stopped for a host, this function saves
 **   all files in the user directory into the directory .<hostname>
 **   so that no files are lost for this host. This function is also
 **   used to save time jobs.
 **
 ** RETURN VALUES
 **   Returns SUCCESS when all files have been saved. Otherwise
 **   INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   03.03.1996 H.Kiehl Created
 **   15.09.1997 H.Kiehl Remove files when they already do exist.
 **   17.10.1997 H.Kiehl When pool is a different file system files
 **                      should be copied.
 **   18.10.1997 H.Kiehl Introduced inverse filtering.
 **   09.06.2002 H.Kiehl Return number of files and size that have been
 **                      saved.
 **   28.01.2002 H.Kiehl Forgot to update files and bytes received and
 **                      reverted above change.
 **   13.07.2003 H.Kiehl Don't link/copy files that are older then the value
 **                      specified in 'age-limit' option.
 **   16.12.2010 H.Kiehl When we fail to create the target directory,
 **                      log in DELETE_LOG the files we failed to save.
 **   28.03.2012 H.Kiehl Handle cross link errors in case we use
 **                      mount with bind option in linux.
 **   26.05.2013 H.Kiehl When pool is a different file system and we
 **                      copy files, we must check for files we will
 **                      overwrite, otherwise files_queued and
 **                      bytes_in_queue will be wrong.
 **   25.02.2022 H.Kiehl On Linux we must check for hardlink protection.
 **                      If that is the case, copy the file.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* sprintf()                          */
#include <string.h>                /* strcmp(), strerror(), strcpy(),    */
                                   /* strcat(), strlen()                 */
#include <sys/types.h>
#include <sys/stat.h>              /* stat(), S_ISDIR()                  */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>                /* S_IRUSR, ...                       */
#endif
#include <unistd.h>                /* link(), mkdir(), unlink()          */
#include <errno.h>
#include "amgdefs.h"


/* External global variables. */
extern int                        fra_fd;
#ifndef _WITH_PTHREAD
extern off_t                      *file_size_pool;
extern time_t                     *file_mtime_pool;
extern char                       **file_name_pool;
extern unsigned char              *file_length_pool;
#endif
#ifdef LINUX
extern char                       hardlinks_protected;
#endif
extern struct filetransfer_status *fsa;
extern struct fileretrieve_status *fra;
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif
#ifdef _DISTRIBUTION_LOG
extern unsigned int               max_jobs_per_file;
extern struct file_dist_list      **file_dist_pool;
#endif


/*########################### save_files() ##############################*/
int
save_files(char                   *src_path,
           char                   *dest_path,
           time_t                 current_time,
           unsigned int           age_limit,
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
           struct directory_entry *p_de,
           struct instant_db      *p_db,
           int                    pos_in_fm,
           int                    no_of_files,
           char                   link_flag,
#ifdef _DISTRIBUTION_LOG
           int                    dist_type,
#endif
           int                    time_job)
{
   register int i,
                j;
   int          files_deleted = 0,
                files_saved = 0,
                retstat;
   off_t        file_size_deleted = 0,
                file_size_saved = 0;
   char         *p_src,
                *p_dest;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

#ifdef HAVE_STATX
   if ((statx(0, dest_path, AT_STATX_SYNC_AS_STAT,
              STATX_SIZE | STATX_MODE, &stat_buf) < 0) ||
       (S_ISDIR(stat_buf.stx_mode) == 0))
#else
   if ((stat(dest_path, &stat_buf) < 0) || (S_ISDIR(stat_buf.st_mode) == 0))
#endif
   {
      /*
       * Only the AFD may read and write in this directory!
       */
      if (mkdir(dest_path, DIR_MODE) < 0)
      {
#ifdef _DELETE_LOG
         int tmp_errno = errno;

#endif
         if (errno != EEXIST)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not mkdir() `%s' to save files : %s",
                       dest_path, strerror(errno));
#ifdef _DELETE_LOG
            for (i = 0; i < no_of_files; i++)
            {
               for (j = 0; j < p_de->fme[pos_in_fm].nfm; j++)
               {
                  if ((retstat = pmatch(p_de->fme[pos_in_fm].file_mask[j],
                                        file_name_pool[i],
                                        &file_mtime_pool[i])) == 0)
                  {
                     size_t dl_real_size;

                     (void)memcpy(dl.file_name, file_name_pool[i],
                                  (size_t)(file_length_pool[i] + 1));
                     (void)snprintf(dl.host_name, MAX_HOSTNAME_LENGTH + 4 + 1,
                                    "%-*s %03x", MAX_HOSTNAME_LENGTH,
                                    p_db->host_alias, MKDIR_QUEUE_ERROR);
                     *dl.file_size = file_size_pool[i];
                     *dl.dir_id = p_de->dir_id;
                     *dl.job_id = p_db->job_id;
                     *dl.input_time = 0L;
                     *dl.split_job_counter = 0;
                     *dl.unique_number = 0;
                     *dl.file_name_length = file_length_pool[i];
                     dl_real_size = *dl.file_name_length + dl.size +
                                    snprintf((dl.file_name + *dl.file_name_length + 1),
                                             MAX_FILENAME_LENGTH + 1,
                                             "%s%c%s (%s %d)",
                                             DIR_CHECK, SEPARATOR_CHAR,
                                             strerror(tmp_errno),
                                             __FILE__, __LINE__);
                     if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "write() error : %s", strerror(errno));
                     }

                     /* No need to test any further filters. */
                     break;
                  }
                  else if (retstat == 1)
                       {
                          /*
                           * This file is definitely NOT wanted, no matter what the
                           * following filters say.
                           */
                          break;
                       }
               }
            }
#endif
            errno = 0;
            return(INCORRECT);
         }

         /*
          * For now lets assume that another process has created
          * this directory just a little bit faster then this
          * process.
          */
      }
   }

   p_src = src_path + strlen(src_path);
   p_dest = dest_path + strlen(dest_path);
   *p_dest++ = '/';

   for (i = 0; i < no_of_files; i++)
   {
      for (j = 0; j < p_de->fme[pos_in_fm].nfm; j++)
      {
         /*
          * Actually we could just read the source directory
          * and rename all files to the new directory.
          * This however involves several system calls (opendir(),
          * readdir(), closedir()), ie high system load. This
          * we hopefully reduce by using the array file_name_pool
          * and pmatch() to get the names we need. Let's see
          * how things work.
          */
         if ((retstat = pmatch(p_de->fme[pos_in_fm].file_mask[j],
                               file_name_pool[i], &file_mtime_pool[i])) == 0)
         {
            int diff_time = current_time - file_mtime_pool[i];

            if (diff_time < 0)
            {
               diff_time = 0;
            }
            (void)memcpy(p_src, file_name_pool[i],
                         (size_t)(file_length_pool[i] + 1));
            if ((age_limit > 0) &&
                ((fsa[p_db->position].host_status & DO_NOT_DELETE_DATA) == 0) &&
                (diff_time > age_limit))
            {
#ifdef _DELETE_LOG
               size_t dl_real_size;

               (void)memcpy(dl.file_name, file_name_pool[i],
                            (size_t)(file_length_pool[i] + 1));
               (void)snprintf(dl.host_name, MAX_HOSTNAME_LENGTH + 4 + 1,
                              "%-*s %03x",
                              MAX_HOSTNAME_LENGTH, p_db->host_alias, AGE_INPUT);
               *dl.file_size = file_size_pool[i];
               *dl.dir_id = p_de->dir_id;
               *dl.job_id = p_db->job_id;
               *dl.input_time = 0L;
               *dl.split_job_counter = 0;
               *dl.unique_number = 0;
               *dl.file_name_length = file_length_pool[i];
               dl_real_size = *dl.file_name_length + dl.size +
                              snprintf((dl.file_name + *dl.file_name_length + 1),
                                       MAX_FILENAME_LENGTH + 1,
                                       "%s%c>%d (%s %d)",
                                       DIR_CHECK, SEPARATOR_CHAR,
                                       diff_time, __FILE__, __LINE__);
               if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "write() error : %s", strerror(errno));
               }
#endif
               if (p_de->flag & RENAME_ONE_JOB_ONLY)
               {
                  if (unlink(src_path) == -1)
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "Failed to unlink() file `%s' : %s",
                                src_path, strerror(errno));
                  }
               }
#ifdef _DISTRIBUTION_LOG
               dist_type = AGE_LIMIT_DELETE_DIS_TYPE;
#endif
            }
            else
            {
               (void)memcpy(p_dest, file_name_pool[i],
                            (size_t)(file_length_pool[i] + 1));

#if defined (_MAINTAINER_LOG) && defined (SHOW_FILE_MOVING)
               maintainer_log(DEBUG_SIGN, NULL, 0,
                              "save_files() [%s %d]: `%s' -> `%s'",
                              caller, line, src_path, dest_path);
#endif
               if (link_flag & IN_SAME_FILESYSTEM)
               {
                  if (p_de->flag & RENAME_ONE_JOB_ONLY)
                  {
#ifdef HAVE_STATX
                     if (statx(0, dest_path, AT_STATX_SYNC_AS_STAT,
                               STATX_SIZE, &stat_buf) != -1)
#else
                     if (stat(dest_path, &stat_buf) != -1)
#endif
                     {
                        if (unlink(dest_path) == -1)
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      "Failed to unlink() file `%s' : %s",
                                      dest_path, strerror(errno));
                        }
                        else
                        {
                           files_deleted++;
#ifdef HAVE_STATX
                           file_size_deleted += stat_buf.stx_size;
#else
                           file_size_deleted += stat_buf.st_size;
#endif
                        }
                     }
                     if ((retstat = rename(src_path, dest_path)) == -1)
                     {
                        if (errno == EXDEV)
                        {
                           link_flag &= ~IN_SAME_FILESYSTEM;
                           goto cross_link_error;
                        }
                        else
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      "Failed to rename() file `%s' to `%s'",
                                      src_path, dest_path);
                           errno = 0;
#ifdef _DISTRIBUTION_LOG
                           dist_type = ERROR_DIS_TYPE;
#endif
                        }
                     }
                     else
                     {
                        files_saved++;
                        file_size_saved += file_size_pool[i];
                     }
                  }
                  else
                  {
#ifdef LINUX
                     if (((hardlinks_protected == YES) &&
                         (access(src_path, W_OK) != 0)))
                     {
try_copy_file:
                        if ((retstat = copy_file(src_path, dest_path,
                                                 NULL)) < 0)
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      "Failed to copy file %s to %s",
                                      src_path, dest_path);
                        }
                        else
                        {
                           files_saved++;
                           file_size_saved += file_size_pool[i];
                        }
                     }
                     else
                     {
#endif
                        if ((retstat = link(src_path, dest_path)) == -1)
                        {
#ifdef LINUX
                           if ((errno == EPERM) &&
                               (hardlinks_protected == NEITHER))
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                         "Hardlinks are protected! You need to unset this in /proc/sys/fs/protected_hardlinks. Otherwise AFD must copy files!");
                              hardlinks_protected = YES;

                              goto try_copy_file;
                           }
#endif
                           if (errno == EEXIST)
                           {
                              off_t del_file_size = 0;

#ifdef HAVE_STATX
                              if (statx(0, dest_path, AT_STATX_SYNC_AS_STAT,
                                        STATX_SIZE, &stat_buf) == -1)
#else
                              if (stat(dest_path, &stat_buf) == -1)
#endif
                              {
                                 system_log(WARN_SIGN, __FILE__, __LINE__,
                                            "Failed to stat() %s : %s",
                                            dest_path, strerror(errno));
                              }
                              else
                              {
#ifdef HAVE_STATX
                                 del_file_size = stat_buf.stx_size;
#else
                                 del_file_size = stat_buf.st_size;
#endif
                              }

                              /*
                               * A file with the same name already exists.
                               * Remove this file and try to link again.
                               */
                              if (unlink(dest_path) == -1)
                              {
                                 system_log(WARN_SIGN, __FILE__, __LINE__,
                                            "Failed to unlink() file `%s' : %s",
                                            dest_path, strerror(errno));
                                 errno = 0;
#ifdef _DISTRIBUTION_LOG
                                 dist_type = ERROR_DIS_TYPE;
#endif
                              }
                              else
                              {
                                 files_deleted++;
                                 file_size_deleted += del_file_size;
                                 if ((retstat = link(src_path, dest_path)) == -1)
                                 {
                                    if (errno == EXDEV)
                                    {
                                       link_flag &= ~IN_SAME_FILESYSTEM;
                                       goto cross_link_error;
                                    }
                                    system_log(WARN_SIGN, __FILE__, __LINE__,
                                               "Failed to link file `%s' to `%s' : %s",
                                               src_path, dest_path,
                                               strerror(errno));
                                    errno = 0;
#ifdef _DISTRIBUTION_LOG
                                    dist_type = ERROR_DIS_TYPE;
#endif
                                 }
                                 else
                                 {
                                    files_saved++;
                                    file_size_saved += file_size_pool[i];
                                 }
                              }
                           }
                           else if (errno == EXDEV)
                                {
                                   link_flag &= ~IN_SAME_FILESYSTEM;
                                   goto cross_link_error;
                                }
                                else
                                {
                                   system_log(WARN_SIGN, __FILE__, __LINE__,
                                              "Failed to link file `%s' to `%s' : %s",
                                              src_path, dest_path,
                                              strerror(errno));
                                   errno = 0;
#ifdef _DISTRIBUTION_LOG
                                   dist_type = ERROR_DIS_TYPE;
#endif
                                }
                        }
                        else
                        {
                           files_saved++;
                           file_size_saved += file_size_pool[i];
                        }
#ifdef LINUX
                     }
#endif
                  }
               }
               else
               {
                  if ((time_job == NO) &&
#ifdef HAVE_STATX
                      (statx(0, dest_path, AT_STATX_SYNC_AS_STAT,
                             STATX_SIZE, &stat_buf) == 0)
#else
                      (stat(dest_path, &stat_buf) == 0)
#endif
                     )
                  {
                     files_deleted++;
#ifdef HAVE_STATX
                     file_size_deleted += stat_buf.stx_size;
#else
                     file_size_deleted += stat_buf.st_size;
#endif
                  }
cross_link_error:
                  if ((retstat = copy_file(src_path, dest_path, NULL)) < 0)
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "Failed to copy file `%s' to `%s'",
                                src_path, dest_path);
                     errno = 0;
#ifdef _DISTRIBUTION_LOG
                     dist_type = ERROR_DIS_TYPE;
#endif
                  }
                  else
                  {
                     files_saved++;
                     file_size_saved += file_size_pool[i];
                     if (p_de->flag & RENAME_ONE_JOB_ONLY)
                     {
                        if (unlink(src_path) == -1)
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      "Failed to unlink() file `%s' : %s",
                                      src_path, strerror(errno));
                           errno = 0;
                        }
                     }
                  }
               }
            }
#ifdef _DISTRIBUTION_LOG
            if ((dist_type < NO_OF_DISTRIBUTION_TYPES) &&
                (file_dist_pool[i][dist_type].no_of_dist < max_jobs_per_file))
            {
               file_dist_pool[i][dist_type].jid_list[file_dist_pool[i][dist_type].no_of_dist] = p_db->job_id;
               file_dist_pool[i][dist_type].proc_cycles[file_dist_pool[i][dist_type].no_of_dist] = (unsigned char)(p_db->no_of_loptions - p_db->no_of_time_entries);
               file_dist_pool[i][dist_type].no_of_dist++;
            }
#endif

            /* No need to test any further filters. */
            break;
         }
         else if (retstat == 1)
              {
                 /*
                  * This file is definitely NOT wanted, no matter what the
                  * following filters say.
                  */
                 break;
              }
      }
   }
   *(p_dest - 1) = '\0';
   *p_src = '\0';

   if (time_job == NO)
   {
      int   files_changed;
      off_t size_changed;

      files_changed = files_saved - files_deleted;
      size_changed = file_size_saved - file_size_deleted;

      if ((files_changed != 0) || (size_changed != 0))
      {
         lock_region_w(fra_fd,
#ifdef LOCK_DEBUG
                       (char *)&fra[p_de->fra_pos].files_queued - (char *)fra, __FILE__, __LINE__);
#else
                       (char *)&fra[p_de->fra_pos].files_queued - (char *)fra);
#endif
         if ((fra[p_de->fra_pos].dir_flag & FILES_IN_QUEUE) == 0)
         {
            fra[p_de->fra_pos].dir_flag |= FILES_IN_QUEUE;
         }
         fra[p_de->fra_pos].files_queued += files_changed;
         fra[p_de->fra_pos].bytes_in_queue += size_changed;
         unlock_region(fra_fd,
#ifdef LOCK_DEBUG
                       (char *)&fra[p_de->fra_pos].files_queued - (char *)fra, __FILE__, __LINE__);
#else
                       (char *)&fra[p_de->fra_pos].files_queued - (char *)fra);
#endif
      }
   }

   return(SUCCESS);
}
