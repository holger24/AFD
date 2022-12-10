/*
 *  remove_job_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   remove_job_files - removes a complete job of the AFD
 **
 ** SYNOPSIS
 **   void remove_job_files(char           *del_dir,
 **                         int            fsa_pos,
 **                         unsigned int   job_id,
 **                         char           *proc,
 **                         char           reason,
 **                         off_t          sf_lock_offset,
 **                         char           *caller_file,
 **                         int            caller_line)
 **
 ** DESCRIPTION
 **   Deletes all files from the given AFD job.
 **   NOTE: Before calling this function, the caller MUST set the
 **         following values:
 **              *dl.input_time
 **              *dl.unique_number
 **              *dl.split_job_counter
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   19.01.1998 H.Kiehl Created
 **   30.06.2016 H.Kiehl Added file name and line number of caller if we
 **                      encounter an error.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                /* strcpy(), strerror()               */
#include <unistd.h>                /* rmdir(), unlink()                  */
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>                /* Definition of AT_* constants       */
#endif
#include <sys/stat.h>              /* struct stat                        */
#include <dirent.h>                /* opendir(), closedir(), readdir(),  */
                                   /* DIR, struct dirent                 */
#include <errno.h>

/* External global variables. */
extern int                        fsa_fd;
extern struct filetransfer_status *fsa;
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif


/*########################## remove_job_files() #########################*/
void
remove_job_files(char           *del_dir,
                 int            fsa_pos,
#ifdef _DELETE_LOG
                 unsigned int   job_id,
                 char           *proc,
                 char           reason,
#endif
                 off_t          sf_lock_offset,
                 char           *caller_file,
                 int            caller_line)
{
   int                        number_deleted = 0;
   off_t                      file_size_deleted = 0;
   char                       *ptr;
   DIR                        *dp;
   struct dirent              *p_dir;
#ifdef HAVE_STATX
   struct statx               stat_buf;
#else
   struct stat                stat_buf;
#endif
   struct filetransfer_status *p_fsa;

   if ((dp = opendir(del_dir)) == NULL)
   {
      if (errno != ENOENT)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to opendir() `%s' : %s [%s %d]"),
                    del_dir, strerror(errno), caller_file, caller_line);
      }
      return;
   }
   ptr = del_dir + strlen(del_dir);
   *(ptr++) = '/';

   if (sf_lock_offset == -1)
   {
      p_fsa = &fsa[fsa_pos];
   }
   else
   {
      p_fsa = fsa;
   }

   errno = 0;
   while ((p_dir = readdir(dp)) != NULL)
   {
#ifdef LINUX
      if (p_dir->d_name[0] == '.')
      {
         continue;
      }
      else if (p_dir->d_type == DT_DIR)
           {
              (void)strcpy(ptr, p_dir->d_name);
              system_log(WARN_SIGN, __FILE__, __LINE__,
                         _("UUUPS! A directory [%s]! Whats that doing here? Deleted %d files. [host_alias=%s] [%s %d]"),
                         del_dir, number_deleted,
                         (fsa_pos > -1) ? p_fsa->host_alias : "-",
                         caller_file, caller_line);

              /* Lets bail out. If del_dir contains garbage we should stop */
              /* the deleting of files.                                    */
              if (closedir(dp) == -1)
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            _("Could not closedir() `%s' : %s [%s %d]"),
                            del_dir, strerror(errno), caller_file, caller_line);
              }
              return;
           }
#else
      if (p_dir->d_name[0] == '.')
      {
         continue;
      }
#endif
      (void)strcpy(ptr, p_dir->d_name);

#ifdef HAVE_STATX
      if (statx(0, del_dir, AT_STATX_SYNC_AS_STAT,
# ifndef LINUX
                STATX_MODE |
# endif
                STATX_SIZE, &stat_buf) == -1)
#else
      if (stat(del_dir, &stat_buf) == -1)
#endif
      {
         if (errno != ENOENT)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                       _("Failed to statx() `%s' : %s [%s %d]"),
#else
                       _("Failed to stat() `%s' : %s [%s %d]"),
#endif
                       del_dir, strerror(errno), caller_file, caller_line);
            if (unlink(del_dir) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to unlink() file `%s' : %s [%s %d]"),
                          del_dir, strerror(errno), caller_file, caller_line);
            }
         }
      }
      else
      {
#ifndef LINUX
# ifdef HAVE_STATX
         if (!S_ISDIR(stat_buf.stx_mode))
# else
         if (!S_ISDIR(stat_buf.st_mode))
# endif
         {
#endif
            if (unlink(del_dir) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to unlink() file `%s' : %s [%s %d]"),
                          del_dir, strerror(errno), caller_file, caller_line);
            }
            else
            {
#ifdef _DELETE_LOG
               size_t dl_real_size;
#endif

               number_deleted++;
#ifdef HAVE_STATX
               file_size_deleted += stat_buf.stx_size;
#else
               file_size_deleted += stat_buf.st_size;
#endif
#ifdef _DELETE_LOG
               (void)strcpy(dl.file_name, p_dir->d_name);
               (void)snprintf(dl.host_name, MAX_HOSTNAME_LENGTH + 4 + 1,
                              "%-*s %03x",
                              MAX_HOSTNAME_LENGTH,
                              (fsa_pos > -1) ? p_fsa->host_alias : "-",
                              reason);
# ifdef HAVE_STATX
               *dl.file_size = stat_buf.stx_size;
# else
               *dl.file_size = stat_buf.st_size;
# endif
               *dl.job_id = job_id;
               *dl.dir_id = 0;
               /* NOTE: input_time, split_job_counter and unique_number */
               /*       are set before this function is called!         */
               *dl.file_name_length = strlen(p_dir->d_name);
               dl_real_size = *dl.file_name_length + dl.size +
                              snprintf((dl.file_name + *dl.file_name_length + 1),
                                       MAX_FILENAME_LENGTH + 1,
                                       "%s%c(%s %d)",
                                       proc, SEPARATOR_CHAR, __FILE__, __LINE__);
               if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("write() error : %s [%s %d]"),
                             strerror(errno), caller_file, caller_line);
               }
#endif
            }
#ifndef LINUX
         }
         else
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("UUUPS! Not a regular file [%s]! Whats that doing here? Deleted %d files. [host_alias=%s] [%s %d]"),
                       del_dir, number_deleted,
                       (fsa_pos > -1) ? p_fsa->host_alias : "-",
                       caller_file, caller_line);

            /* Lets bail out. If del_dir contains garbage we should stop */
            /* the deleting of files.                                    */
            if (closedir(dp) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Could not closedir() `%s' : %s [%s %d]"),
                          del_dir, strerror(errno), caller_file, caller_line);
            }
            return;
         }
#endif
      }
      errno = 0;
   }

   *(ptr - 1) = '\0';
   if (errno)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not readdir() `%s' : %s [%s %d]"),
                 del_dir, strerror(errno), caller_file, caller_line);
   }
   if (closedir(dp) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not closedir() `%s' : %s [%s %d]"),
                 del_dir, strerror(errno), caller_file, caller_line);
   }
   if (rmdir(del_dir) == -1)
   {
      if ((errno == ENOTEMPTY) || (errno == EEXIST))
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("Failed to rmdir() `%s' because there is still data in it, deleting everything in this directory. [%s %d]"),
                    del_dir, caller_file, caller_line);
         (void)rec_rmdir(del_dir);
      }
      else
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not rmdir() `%s' : %s [%s %d]"),
                    del_dir, strerror(errno), caller_file, caller_line);
      }
   }

   if ((number_deleted > 0) && (fsa_pos != -1))
   {
      off_t lock_offset;

      /* Total file counter. */
      if (sf_lock_offset == -1)
      {
         lock_offset = AFD_WORD_OFFSET +
                       (fsa_pos * sizeof(struct filetransfer_status));
      }
      else
      {
         lock_offset = sf_lock_offset;
      }
#ifdef LOCK_DEBUG
      lock_region_w(fsa_fd, lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
      lock_region_w(fsa_fd, lock_offset + LOCK_TFC);
#endif
      p_fsa->total_file_counter -= number_deleted;
#ifdef _VERIFY_FSA
      if (p_fsa->total_file_counter < 0)
      {
         system_log(INFO_SIGN, __FILE__, __LINE__,
                    _("Total file counter for host `%s' less then zero. Correcting. [%s %d]"),
                    p_fsa->host_dsp_name, caller_file, caller_line);
         p_fsa->total_file_counter = 0;
      }
#endif

      /* Total file size. */
      p_fsa->total_file_size -= file_size_deleted;
#ifdef _VERIFY_FSA
      if (p_fsa->total_file_size < 0)
      {
         system_log(INFO_SIGN, __FILE__, __LINE__,
                    _("Total file size for host `%s' overflowed. Correcting. [%s %d]"),
                    p_fsa->host_dsp_name, caller_file, caller_line);
         p_fsa->total_file_size = 0;
      }
      else if ((p_fsa->total_file_counter == 0) &&
               (p_fsa->total_file_size > 0))
           {
              system_log(INFO_SIGN, __FILE__, __LINE__,
                         _("fc for host `%s' is zero but fs is not zero. Correcting. [%s %d]"),
                         p_fsa->host_dsp_name, caller_file, caller_line);
              p_fsa->total_file_size = 0;
           }
#endif

      /*
       * If we have removed all files and error counter is larger then
       * zero, reset it to zero. Otherwise, when the queue is stopped
       * automatically it will not retry to send files.
       */
      if ((p_fsa->total_file_size == 0) &&
          (p_fsa->total_file_counter == 0))
      {
         int i;

         p_fsa->error_history[0] = 0;
         p_fsa->error_history[1] = 0;
         if (p_fsa->error_counter != 0)
         {
            p_fsa->error_counter = 0;
         }
         for (i = 0; i < p_fsa->allowed_transfers; i++)
         {
            if (p_fsa->job_status[i].connect_status == NOT_WORKING)
            {
               p_fsa->job_status[i].connect_status = DISCONNECT;
            }
         }
      }
#ifdef LOCK_DEBUG
      unlock_region(fsa_fd, lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
      unlock_region(fsa_fd, lock_offset + LOCK_TFC);
#endif
   }

   return;
}
