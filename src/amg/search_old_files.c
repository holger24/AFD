/*
 *  search_old_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2022 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   search_old_files - searches in all user directories for old
 **                      files
 **
 ** SYNOPSIS
 **   void search_old_files(time_t current_time)
 **
 ** DESCRIPTION
 **   This function checks the user directory for any old files.
 **   Old depends on the value of OLD_FILE_TIME. If it discovers
 **   files older then OLD_FILE_TIME it will report this in the
 **   system log. When _DELETE_OLD_FILES is defined these files will
 **   be deleted.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   09.06.1997 H.Kiehl Created
 **   21.02.1998 H.Kiehl Added delete logging.
 **                      Search queue directories as well.
 **   04.01.2001 H.Kiehl Don't always check what the current time is.
 **   20.07.2001 H.Kiehl Added new option to search queue for old files,
 **                      this was WRONGLY done with the "delete unknown files"
 **                      option!
 **   22.05.2002 H.Kiehl Separate old file times for unknown and queued files.
 **   22.09.2003 H.Kiehl If old_file_time is 0, do not delete incoming dot
 **                      files.
 **   04.02.2004 H.Kiehl Don't delete files that are to be distributed!
 **   28.11.2004 H.Kiehl Report old files with a leading dot.
 **                      Optionaly delete old locked files.
 **   12.02.2006 H.Kiehl Change meaning of "delete old locked files" to
 **                      all locked files, not only those to be distributed.
 **   06.08.2009 H.Kiehl Implemented cleaning of stopped directories.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* NULL                               */
#include <string.h>                /* strcpy(), strerror()               */
#include <time.h>                  /* time()                             */
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>                /* Definition of AT_* constants       */
#endif
#include <sys/stat.h>              /* stat(), S_ISREG()                  */
#include <dirent.h>                /* opendir(), closedir(), readdir(),  */
                                   /* DIR, struct dirent                 */
#include <unistd.h>                /* write(), unlink()                  */
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int                        fra_fd,
                                  no_of_local_dirs,
                                  no_of_hosts;
extern struct directory_entry     *de;
extern struct filetransfer_status *fsa;
extern struct fileretrieve_status *fra,
                                  *p_fra;
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif

/* Local function prototypes. */
static int                        is_dir_with_changing_date(char *);


/*######################### search_old_files() ##########################*/
void
search_old_files(time_t current_time)
{
   int           i, j, k,
                 delete_file,
                 junk_files,
                 file_counter,
                 queued_files,
#ifdef _DELETE_LOG
                 reason,
#endif
                 ret;
   time_t        diff_time,
                 pmatch_time;
   char          *work_ptr,
                 tmp_dir[MAX_PATH_LENGTH];
   off_t         queued_size_deleted,
                 file_size;
#ifdef HAVE_STATX
   struct statx  stat_buf;
#else
   struct stat   stat_buf;
#endif
   DIR           *dp;
   struct dirent *p_dir;

   for (i = 0; i < no_of_local_dirs; i++)
   {
      if ((de[i].dir != NULL) &&
          ((fra[de[i].fra_pos].dir_flag & DIR_DISABLED) == 0))
      {
         (void)strcpy(tmp_dir, de[i].dir);

         if ((dp = opendir(tmp_dir)) == NULL)
         {
            if ((errno != ENOENT) && (errno != EACCES))
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Can't access directory %s : %s",
                          tmp_dir, strerror(errno));
            }
         }
         else
         {
            file_counter        = 0;
            file_size           = 0;
            junk_files          = 0;
            queued_files        = 0;
            queued_size_deleted = 0;

            work_ptr = tmp_dir + strlen(tmp_dir);
            *work_ptr++ = '/';
            *work_ptr = '\0';

            errno = 0;
            while ((p_dir = readdir(dp)) != NULL)
            {
               /* Ignore "." and "..". */
               if (((p_dir->d_name[0] == '.') && (p_dir->d_name[1] == '\0')) ||
                   ((p_dir->d_name[0] == '.') && (p_dir->d_name[1] == '.') &&
                   (p_dir->d_name[2] == '\0')))
               {
                  continue;
               }

               (void)strcpy(work_ptr, p_dir->d_name);
#ifdef HAVE_STATX
               if (statx(0, tmp_dir, AT_STATX_SYNC_AS_STAT,
                         STATX_SIZE | STATX_MODE | STATX_MTIME,
                         &stat_buf) == -1)
#else
               if (stat(tmp_dir, &stat_buf) < 0)
#endif
               {
                  /*
                   * Since this is a very low priority function lets not
                   * always report when we fail to stat() a file. Maybe the
                   * the user wants to keep some files.
                   */
                  continue;
               }

               /* Sure it is a normal file? */
#ifdef HAVE_STATX
               if (S_ISREG(stat_buf.stx_mode))
#else
               if (S_ISREG(stat_buf.st_mode))
#endif
               {
                  int changing_date_dir = NO;

                  /*
                   * Regardless of what the delete_files_flag is set, also
                   * delete old files that are of zero length or have a
                   * leading dot.
                   */
#ifdef HAVE_STATX
                  diff_time = current_time - stat_buf.stx_mtime.tv_sec;
#else
                  diff_time = current_time - stat_buf.st_mtime;
#endif
                  if (diff_time < 0)
                  {
                     diff_time = 0;
                  }
                  if (((p_dir->d_name[0] == '.') &&
                       (diff_time > 3600L) &&
                       ((fra[de[i].fra_pos].unknown_file_time == 0) ||
                        ((fra[de[i].fra_pos].delete_files_flag & OLD_LOCKED_FILES) &&
                         (diff_time > fra[de[i].fra_pos].locked_file_time)) ||
                        ((fra[de[i].fra_pos].fsa_pos != -1) &&
                         (diff_time > (DEFAULT_OLD_FILE_TIME * 3600)) &&
                         ((changing_date_dir = is_dir_with_changing_date(fra[de[i].fra_pos].url)) == YES)))) ||
                      ((diff_time > 5L) &&
                       (diff_time > fra[de[i].fra_pos].unknown_file_time)))
                  {
                     if ((fra[de[i].fra_pos].delete_files_flag & UNKNOWN_FILES) ||
                         (p_dir->d_name[0] == '.'))
                     {
                        if (p_dir->d_name[0] == '.')
                        {
                           if ((fra[de[i].fra_pos].delete_files_flag & OLD_LOCKED_FILES) &&
                                (diff_time > fra[de[i].fra_pos].locked_file_time))
                           {
                              delete_file = YES;
#ifdef _DELETE_LOG
                              if (fra[de[i].fra_pos].in_dc_flag & UNKNOWN_FILES_IDC)
                              {
                                 reason = DEL_OLD_LOCKED_FILE;
                              }
                              else
                              {
                                 reason = DEL_OLD_LOCKED_FILE_GLOB;
                              }
#endif
                           }
                           else if ((fra[de[i].fra_pos].fsa_pos != -1) &&
                                    (changing_date_dir == YES))
                                {
                                   delete_file = YES;
#ifdef _DELETE_LOG
                                   reason = DEL_OLD_LOCKED_FILE;
#endif
                                }
                                else
                                {
                                   delete_file = NO;
                                }
                        }
                        else
                        {
                           if (de[i].flag & ALL_FILES)
                           {
                              delete_file = NO;
                           }
                           else
                           {
                              delete_file = YES;
#ifdef _DELETE_LOG
                              if (fra[de[i].fra_pos].in_dc_flag & UNKNOWN_FILES_IDC)
                              {
                                 reason = DEL_UNKNOWN_FILE;
                              }
                              else
                              {
                                 reason = DEL_UNKNOWN_FILE_GLOB;
                              }
#endif
                              if (de[i].paused_dir == NULL)
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
                              for (j = 0; j < de[i].nfg; j++)
                              {
                                 for (k = 0; ((j < de[i].nfg) && (k < de[i].fme[j].nfm)); k++)
                                 {
                                    if ((ret = pmatch(de[i].fme[j].file_mask[k],
                                                      p_dir->d_name,
                                                      &pmatch_time)) == 0)
                                    {
                                       delete_file = NO;
                                       j = de[i].nfg;
                                    }
                                    else if (ret == 1)
                                         {
                                            break;
                                         }
                                 }
                              }
                           }
                        }
                        if (delete_file == YES)
                        {
                           if (unlink(tmp_dir) == -1)
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                         "Failed to unlink() %s : %s",
                                         tmp_dir, strerror(errno));
                           }
                           else
                           {
#ifdef _DELETE_LOG
                              size_t dl_real_size;

                              (void)strcpy(dl.file_name, p_dir->d_name);
                              (void)snprintf(dl.host_name,
                                             MAX_HOSTNAME_LENGTH + 4 + 1,
                                             "%-*s %03x",
                                             MAX_HOSTNAME_LENGTH, "-", reason);
# ifdef HAVE_STATX
                              *dl.file_size = stat_buf.stx_size;
# else
                              *dl.file_size = stat_buf.st_size;
# endif
                              *dl.dir_id = de[i].dir_id;
                              *dl.job_id = 0;
                              *dl.input_time = 0L;
                              *dl.split_job_counter = 0;
                              *dl.unique_number = 0;
                              *dl.file_name_length = strlen(p_dir->d_name);
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
                                            "write() error : %s",
                                            strerror(errno));
                              }
#endif
                              file_counter++;
#ifdef HAVE_STATX
                              file_size += stat_buf.stx_size;
#else
                              file_size += stat_buf.st_size;
#endif

                              if ((fra[de[i].fra_pos].delete_files_flag & UNKNOWN_FILES) == 0)
                              {
                                 junk_files++;
                              }
                           }
                        }
                        else if (fra[de[i].fra_pos].report_unknown_files == YES)
                             {
                                if ((fra[de[i].fra_pos].dir_flag & DIR_STOPPED) == 0)
                                {
                                   file_counter++;
#ifdef HAVE_STATX
                                   file_size += stat_buf.stx_size;
#else
                                   file_size += stat_buf.st_size;
#endif
                                }
                             }
                     }
                     else if (fra[de[i].fra_pos].report_unknown_files == YES)
                          {
                             if ((fra[de[i].fra_pos].dir_flag & DIR_STOPPED) == 0)
                             {
                                file_counter++;
#ifdef HAVE_STATX
                                file_size += stat_buf.stx_size;
#else
                                file_size += stat_buf.st_size;
#endif
                             }
                             delete_file = NO;
                          }
                          else
                          {
                             delete_file = NO;
                          }
                  }
                  else
                  {
                     delete_file = NO;
                  }
                  if ((delete_file == NO) && (p_dir->d_name[0] != '.') &&
                      (fra[de[i].fra_pos].dir_flag & DIR_STOPPED) &&
                      (fra[de[i].fra_pos].delete_files_flag & QUEUED_FILES) &&
                      (diff_time > fra[de[i].fra_pos].queued_file_time))
                  {
                     if (unlink(tmp_dir) == -1)
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   "Failed to unlink() %s : %s",
                                   tmp_dir, strerror(errno));
                     }
                     else
                     {
#ifdef _DELETE_LOG
                        size_t dl_real_size;

                        (void)strcpy(dl.file_name, p_dir->d_name);
                        (void)snprintf(dl.host_name,
                                       MAX_HOSTNAME_LENGTH + 4 + 1,
                                       "%-*s %03x",
                                       MAX_HOSTNAME_LENGTH, "-",
                                       (fra[de[i].fra_pos].in_dc_flag & QUEUED_FILES_IDC) ?  DEL_QUEUED_FILE : DEL_QUEUED_FILE_GLOB);
# ifdef HAVE_STATX
                        *dl.file_size = stat_buf.stx_size;
# else
                        *dl.file_size = stat_buf.st_size;
# endif
                        *dl.dir_id = de[i].dir_id;
                        *dl.job_id = 0;
                        *dl.input_time = 0L;
                        *dl.split_job_counter = 0;
                        *dl.unique_number = 0;
                        *dl.file_name_length = strlen(p_dir->d_name);
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
                                      "write() error : %s",
                                      strerror(errno));
                        }
#endif
                        queued_files++;
#ifdef HAVE_STATX
                        queued_size_deleted += stat_buf.stx_size;
#else
                        queued_size_deleted += stat_buf.st_size;
#endif
                     }
                  }
               }
                    /*
                     * Search queue directories for old files!
                     */
               else if ((fra[de[i].fra_pos].delete_files_flag & QUEUED_FILES) &&
                        (p_dir->d_name[0] == '.') &&
#ifdef HAVE_STATX
                        (S_ISDIR(stat_buf.stx_mode))
#else
                        (S_ISDIR(stat_buf.st_mode))
#endif
                       )
                    {
                       int pos;

                       if (((pos = get_host_position(fsa,
                                                     &p_dir->d_name[1],
                                                     no_of_hosts)) != INCORRECT) &&
                           ((fsa[pos].host_status & DO_NOT_DELETE_DATA) == 0))
                       {
                          DIR *dp_2;

                          if ((dp_2 = opendir(tmp_dir)) == NULL)
                          {
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        "Can't access directory %s : %s",
                                        tmp_dir, strerror(errno));
                          }
                          else
                          {
                             int   files_deleted = 0;
                             off_t file_size_deleted = 0;
                             char  *work_ptr_2;

                             work_ptr_2 = tmp_dir + strlen(tmp_dir);
                             *work_ptr_2++ = '/';
                             *work_ptr_2 = '\0';

                             errno = 0;
                             while ((p_dir = readdir(dp_2)) != NULL)
                             {
                                /* Ignore "." and "..". */
                                if (p_dir->d_name[0] == '.')
                                {
                                   continue;
                                }

                                (void)strcpy(work_ptr_2, p_dir->d_name);
#ifdef HAVE_STATX
                                if (statx(0, tmp_dir, AT_STATX_SYNC_AS_STAT,
                                          STATX_SIZE | STATX_MODE | STATX_MTIME,
                                          &stat_buf) == -1)
#else
                                if (stat(tmp_dir, &stat_buf) < 0)
#endif
                                {
                                   /*
                                    * Since this is a very low priority function lets not
                                    * always report when we fail to stat() a file. Maybe the
                                    * the user wants to keep some files.
                                    */
                                   continue;
                                }

                                /* Sure it is a normal file? */
#ifdef HAVE_STATX
                                if (S_ISREG(stat_buf.stx_mode))
#else
                                if (S_ISREG(stat_buf.st_mode))
#endif
                                {
#ifdef HAVE_STATX
                                   diff_time = current_time - stat_buf.stx_mtime.tv_sec;
#else
                                   diff_time = current_time - stat_buf.st_mtime;
#endif
                                   if (diff_time < 0)
                                   {
                                      diff_time = 0;
                                   }
                                   if (diff_time > fra[de[i].fra_pos].queued_file_time)
                                   {
                                      if (unlink(tmp_dir) == -1)
                                      {
                                         system_log(WARN_SIGN, __FILE__, __LINE__,
                                                    "Failed to unlink() %s : %s",
                                                    tmp_dir, strerror(errno));
                                      }
                                      else
                                      {
#ifdef _DELETE_LOG
                                         size_t dl_real_size;

                                         (void)strcpy(dl.file_name, p_dir->d_name);
                                         (void)snprintf(dl.host_name,
                                                        MAX_HOSTNAME_LENGTH + 4 + 1,
                                                        "%-*s %03x",
                                                        MAX_HOSTNAME_LENGTH,
                                                        fsa[pos].host_alias,
                                                        (fra[de[i].fra_pos].in_dc_flag & QUEUED_FILES_IDC) ?  DEL_QUEUED_FILE : DEL_QUEUED_FILE_GLOB);
# ifdef HAVE_STATX
                                         *dl.file_size = stat_buf.stx_size;
# else
                                         *dl.file_size = stat_buf.st_size;
# endif
                                         *dl.dir_id = de[i].dir_id;
                                         *dl.job_id = 0;
                                         *dl.input_time = 0L;
                                         *dl.split_job_counter = 0;
                                         *dl.unique_number = 0;
                                         *dl.file_name_length = strlen(p_dir->d_name);
                                         dl_real_size = *dl.file_name_length + dl.size +
                                                        snprintf((dl.file_name + *dl.file_name_length + 1),
                                                                 MAX_FILENAME_LENGTH + 1,
# if SIZEOF_TIME_T == 4
                                                                 "%s%c>%ld (%s %d)",
# else
                                                                 "%s%c>%lld (%s %d)",
# endif
                                                                 DIR_CHECK,
                                                                 SEPARATOR_CHAR,
                                                                 (pri_time_t)diff_time,
                                                                 __FILE__, __LINE__);
                                         if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                                         {
                                            system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                       "write() error : %s",
                                                       strerror(errno));
                                         }
#endif
                                         files_deleted++;
#ifdef HAVE_STATX
                                         file_size_deleted += stat_buf.stx_size;
#else
                                         file_size_deleted += stat_buf.st_size;
#endif
                                      }
                                   }
                                }
                                errno = 0;
                             } /* while ((p_dir = readdir(dp_2)) != NULL) */

                             if (errno)
                             {
                                system_log(ERROR_SIGN, __FILE__, __LINE__,
                                           "Could not readdir() %s : %s",
                                           tmp_dir, strerror(errno));
                             }

                             if (files_deleted > 0)
                             {
                                queued_files += files_deleted;
                                queued_size_deleted += file_size_deleted;
                                ABS_REDUCE_QUEUE(de[i].fra_pos, files_deleted,
                                                 file_size_deleted);
                             }

                             /* Don't forget to close the directory. */
                             if (closedir(dp_2) < 0)
                             {
                                system_log(ERROR_SIGN, __FILE__, __LINE__,
                                           "Could not close directory %s : %s",
                                           tmp_dir, strerror(errno));
                             }
                          }
                       }
                    }
               errno = 0;
            }

            /*
             * NOTE: The ENOENT is when it catches a file that is just
             *       being renamed (lock DOT).
             */
            if ((errno) && (errno != ENOENT))
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not readdir() %s : %s",
                          tmp_dir, strerror(errno));
            }

            /* Don't forget to close the directory. */
            if (closedir(dp) < 0)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not close directory %s : %s",
                          tmp_dir, strerror(errno));
            }

            /* Remove file name from directory name. */
            *(work_ptr - 1) = '\0';

            /* Tell user there are old files in this directory. */
            if (((file_counter - junk_files) > 0) &&
                (fra[de[i].fra_pos].report_unknown_files == YES) &&
                ((fra[de[i].fra_pos].delete_files_flag & UNKNOWN_FILES) == 0))
            {
               p_fra = &fra[de[i].fra_pos];
               if (file_size >= GIGABYTE)
               {
                  receive_log(WARN_SIGN, NULL, 0, current_time,
                              "There are %d (%d GiB) old (>%dh) files in %s. @%x",
                              file_counter - junk_files,
                              (int)(file_size / 1073741824),
                              fra[de[i].fra_pos].unknown_file_time / 3600,
                              tmp_dir, de[i].dir_id);
               }
               else if (file_size >= MEGABYTE)
                    {
                       receive_log(WARN_SIGN, NULL, 0, current_time,
                                   "There are %d (%d MiB) old (>%dh) files in %s. @%x",
                                   file_counter - junk_files,
                                   (int)(file_size / 1048576),
                                   fra[de[i].fra_pos].unknown_file_time / 3600,
                                   tmp_dir, de[i].dir_id);
                    }
               else if (file_size >= KILOBYTE)
                    {
                       receive_log(WARN_SIGN, NULL, 0, current_time,
                                   "There are %d (%d KiB) old (>%dh) files in %s. @%x",
                                   file_counter - junk_files, (int)(file_size / 1024),
                                   fra[de[i].fra_pos].unknown_file_time / 3600,
                                   tmp_dir, de[i].dir_id);
                    }
                    else
                    {
                       receive_log(WARN_SIGN, NULL, 0, current_time,
#if SIZEOF_OFF_T == 4
                                   "There are %d (%ld bytes) old (>%dh) files in %s. @%x",
#else
                                   "There are %d (%lld bytes) old (>%dh) files in %s. @%x",
#endif
                                   file_counter - junk_files,
                                   (pri_off_t)file_size,
                                   fra[de[i].fra_pos].unknown_file_time / 3600,
                                   tmp_dir, de[i].dir_id);
                    }
            }
            if (junk_files > 0)
            {
               p_fra = &fra[de[i].fra_pos];
               receive_log(DEBUG_SIGN, NULL, 0, current_time,
                           "Deleted %d file(s) (>%dh) that where of length 0 or had a leading dot, in %s. @%x",
                           junk_files,
                           fra[de[i].fra_pos].unknown_file_time / 3600,
                           tmp_dir, de[i].dir_id);
            }
            if (queued_files > 0)
            {
               p_fra = &fra[de[i].fra_pos];
               if (queued_size_deleted >= GIGABYTE)
               {
                  receive_log(DEBUG_SIGN, NULL, 0, current_time,
                              "Deleted %d (%d GiB) queued file(s), in %s. @%x",
                              queued_files,
                              (int)(queued_size_deleted / 1073741824),
                              tmp_dir, de[i].dir_id);
               }
               else if (queued_size_deleted >= MEGABYTE)
                    {
                       receive_log(DEBUG_SIGN, NULL, 0, current_time,
                                   "Deleted %d (%d MiB) queued file(s), in %s. @%x",
                                   queued_files,
                                   (int)(queued_size_deleted / 1048576),
                                   tmp_dir, de[i].dir_id);
                    }
               else if (queued_size_deleted >= KILOBYTE)
                    {
                       receive_log(DEBUG_SIGN, NULL, 0, current_time,
                                   "Deleted %d (%d KiB) queued file(s), in %s. @%x",
                                   queued_files,
                                   (int)(queued_size_deleted / 1024),
                                   tmp_dir, de[i].dir_id);
                    }
                    else
                    {
                       receive_log(DEBUG_SIGN, NULL, 0, current_time,
#if SIZEOF_OFF_T == 4
                                   "Deleted %d (%ld bytes) queued file(s), in %s. @%x",
#else
                                   "Deleted %d (%lld bytes) queued file(s), in %s. @%x",
#endif
                                   queued_files, (pri_off_t)queued_size_deleted,
                                   tmp_dir, de[i].dir_id);
                    }
            }
         }
      }
   }

   return;
}


/*--------------------- is_dir_with_changing_date() ---------------------*/
static int
is_dir_with_changing_date(char *dir)
{
   int i = 0;

   while ((i < MAX_RECIPIENT_LENGTH) && (*(dir + i) != '\0'))
   {
      if ((*(dir + i) == '%') &&
          ((*(dir + i + 1) == 't') || (*(dir + i + 1) == 'T')))
      {
         return(YES);
      }
      if (*(dir + i) == '\\')
      {
         i++;
      }
      i++;
   }

   return(NO);
}
