/*
 *  remove_job_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **                         off_t          sf_lock_offset)
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
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                /* strcpy(), strerror()               */
#include <unistd.h>                /* rmdir(), unlink()                  */
#include <sys/types.h>
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
#ifdef _DELETE_LOG
remove_job_files(char           *del_dir,
                 int            fsa_pos,
                 unsigned int   job_id,
                 char           *proc,
                 char           reason,
                 off_t          sf_lock_offset)
#else
remove_job_files(char *del_dir, int fsa_pos, off_t sf_lock_offset)
#endif
{
   int                        number_deleted = 0;
   off_t                      file_size_deleted = 0;
   char                       *ptr;
   DIR                        *dp;
   struct dirent              *p_dir;
   struct stat                stat_buf;
   struct filetransfer_status *p_fsa;

   if ((dp = opendir(del_dir)) == NULL)
   {
      if (errno != ENOENT)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to opendir() `%s' : %s"),
                    del_dir, strerror(errno));
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
      if (p_dir->d_name[0] == '.')
      {
         continue;
      }
      (void)strcpy(ptr, p_dir->d_name);

      if (stat(del_dir, &stat_buf) == -1)
      {
         if (errno != ENOENT)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("Failed to stat() `%s' : %s"),
                       del_dir, strerror(errno));
            if (unlink(del_dir) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to unlink() file `%s' : %s"),
                          del_dir, strerror(errno));
            }
         }
      }
      else
      {
         if (!S_ISDIR(stat_buf.st_mode))
         {
            if (unlink(del_dir) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to unlink() file `%s' : %s"),
                          del_dir, strerror(errno));
            }
            else
            {
#ifdef _DELETE_LOG
               size_t dl_real_size;
#endif

               number_deleted++;
               file_size_deleted += stat_buf.st_size;
#ifdef _DELETE_LOG
               (void)strcpy(dl.file_name, p_dir->d_name);
               if (fsa_pos > -1)
               {
                  (void)snprintf(dl.host_name, MAX_HOSTNAME_LENGTH + 4 + 1,
                                 "%-*s %03x",
                                 MAX_HOSTNAME_LENGTH,
                                 p_fsa->host_alias,
                                 reason);
               }
               else
               {
                  (void)snprintf(dl.host_name, MAX_HOSTNAME_LENGTH + 4 + 1,
                                 "%-*s %03x",
                                 MAX_HOSTNAME_LENGTH,
                                 "-",
                                 reason);
               }
               *dl.file_size = stat_buf.st_size;
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
                             _("write() error : %s"), strerror(errno));
               }
#endif
            }
         }
         else
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("UUUPS! A directory [%s]! Whats that doing here? Deleted %d files. [host_alias=%s]"),
                       del_dir, number_deleted,
                       (fsa_pos > -1) ? p_fsa->host_alias : "-");

            /* Lets bail out. If del_dir contains garbage we should stop */
            /* the deleting of files.                                    */
            if (closedir(dp) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Could not closedir() `%s' : %s"),
                          del_dir, strerror(errno));
            }
            return;
         }
      }
      errno = 0;
   }

   *(ptr - 1) = '\0';
   if (errno)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not readdir() `%s' : %s"), del_dir, strerror(errno));
   }
   if (closedir(dp) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not closedir() `%s' : %s"), del_dir, strerror(errno));
   }
   if (rmdir(del_dir) == -1)
   {
      if ((errno == ENOTEMPTY) || (errno == EEXIST))
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("Failed to rmdir() `%s' because there is still data in it, deleting everything in this directory."),
                    del_dir);
         (void)rec_rmdir(del_dir);
      }
      else
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not rmdir() `%s' : %s"), del_dir, strerror(errno));
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
                    _("Total file counter for host `%s' less then zero. Correcting."),
                    p_fsa->host_dsp_name);
         p_fsa->total_file_counter = 0;
      }
#endif

      /* Total file size. */
      p_fsa->total_file_size -= file_size_deleted;
#ifdef _VERIFY_FSA
      if (p_fsa->total_file_size < 0)
      {
         system_log(INFO_SIGN, __FILE__, __LINE__,
                    _("Total file size for host `%s' overflowed. Correcting."),
                    p_fsa->host_dsp_name);
         p_fsa->total_file_size = 0;
      }
      else if ((p_fsa->total_file_counter == 0) &&
               (p_fsa->total_file_size > 0))
           {
              system_log(INFO_SIGN, __FILE__, __LINE__,
                         _("fc for host `%s' is zero but fs is not zero. Correcting."),
                         p_fsa->host_dsp_name);
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
