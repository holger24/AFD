/*
 *  compare_dir_local.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2010 - 2022 Deutscher Wetterdienst (DWD),
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
 **   compare_dir_local - compares local directory content with
 **                       that from where data comes
 **
 ** SYNOPSIS
 **   void compare_dir_local(void)
 **
 ** DESCRIPTION
 **   Compares local directory content with the directory content
 **   from the source directory. Files found in local directory
 **   but are not listed in source
 **   an array so that these jobs can be accessed faster.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.01.2010 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>           /* strerror()                              */
#include <stdlib.h>           /* realloc(), free()                       */
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>           /* Definition of AT_* constants            */
#endif
#include <sys/stat.h>         /* stat(), S_ISREG()                       */
#include <dirent.h>           /* opendir(), closedir(), readdir(), DIR,  */
                              /* struct dirent                           */
#include <unistd.h>
#include <errno.h>
#include "fddefs.h"

/* External global variables. */
extern int                        fra_fd,
                                  fra_id,
                                  no_of_dirs,
                                  no_of_listed_files,
                                  rl_fd;
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif
extern struct job                 db;
extern struct retrieve_list       *rl;
extern struct fileretrieve_status *fra;
extern struct filetransfer_status *fsa;


/*$$$$$$$$$$$$$$$$$$$$$$$$$ compare_dir_local() $$$$$$$$$$$$$$$$$$$$$$$$$*/
void
compare_dir_local(void)
{
   static unsigned int dir_id = 0,
                       prev_job_id = 0;
   static int          no_of_files;
   static char         *files = NULL;
   int                 deleted_files = 0,
                       gotcha,
                       i,
                       j,
                       ret;
   off_t               deleted_size = 0;
   char                fullname[MAX_PATH_LENGTH],
                       *p_file,
                       *work_ptr;
   DIR                 *dp;
   struct dirent       *p_dir;
#ifdef HAVE_STATX
   struct statx        stat_buf;
#else
   struct stat         stat_buf;
#endif

   if (fra_fd == -1)
   {
      if (fra_attach() != SUCCESS)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__, "Failed to attach to FRA.");
         exit(INCORRECT);
      }
   }
   if (db.id.job != prev_job_id)
   {
      int                no_of_job_ids;
      struct job_id_data *jd = NULL;

      prev_job_id = db.id.job;
      if (read_job_ids(NULL, &no_of_job_ids, &jd) == INCORRECT)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__, "Failed to read JID.");
         exit(INCORRECT);
      }
      for (i = 0; i < no_of_job_ids; i++)
      {
         if (jd[i].job_id == db.id.job)
         {
            break;
         }
      }
      if (i == no_of_job_ids)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to locate job #%x.", db.id.job);
         exit(INCORRECT);
      }
      dir_id = jd[i].dir_id;
      if ((db.fra_pos = get_dir_id_position(fra, dir_id, no_of_dirs)) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to locate dir_id @%x in the FRA.", dir_id);
         exit(INCORRECT);
      }
      if (files != NULL)
      {
         free(files);
         files = NULL;
      }
      get_file_mask_list(jd[i].file_mask_id, &no_of_files, &files);
      if (files == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to read file mask %x for job %x.",
                    jd[i].file_mask_id, jd[i].job_id);
         exit(INCORRECT);
      }

      (void)free(jd);
   }

   if (rl_fd == -1)
   {
      if (attach_ls_data(fra, db.special_flag, YES) == INCORRECT)
      {
         exit(INCORRECT);
      }
   }

   if ((dp = opendir(db.target_dir)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to opendir() `%s' : %s",
                 db.target_dir, strerror(errno));
      exit(INCORRECT);
   }
   (void)strcpy(fullname, db.target_dir);
   work_ptr = fullname + strlen(fullname);
   *work_ptr++ = '/';
   *work_ptr = '\0';

   while ((p_dir = readdir(dp)) != NULL)
   {
      if (p_dir->d_name[0] != '.')
      {
         gotcha = NO;
         p_file = files;
         for (j = 0; j < no_of_files; j++)
         {
            if ((ret = pmatch(p_file, p_dir->d_name,  NULL)) == 0)
            {
               gotcha = YES;
               break;
            }
            else if (ret == 1)
                 {
                    /* This file is NOT wanted. */
                    break;
                 }
            NEXT(p_file);
         }
         if (gotcha == YES)
         {
            (void)strcpy(work_ptr, p_dir->d_name);
#ifdef HAVE_STATX
            if (statx(0, fullname, AT_STATX_SYNC_AS_STAT,
                      STATX_SIZE | STATX_MODE, &stat_buf) == -1)
#else
            if (stat(fullname, &stat_buf) == -1)
#endif
            {
               if (errno != ENOENT)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                             "Can't statx() file `%s' : %s",
#else
                             "Can't stat() file `%s' : %s",
#endif
                             fullname, strerror(errno));
               }
               continue;
            }

            /* Sure it is a normal file? */
#ifdef HAVE_STATX
            if (S_ISREG(stat_buf.stx_mode))
#else
            if (S_ISREG(stat_buf.st_mode))
#endif
            {
               gotcha = NO;
               for (i = 0; i < no_of_listed_files; i++)
               {
                  if (CHECK_STRCMP(p_dir->d_name, rl[i].file_name) == 0)
                  {
                     gotcha = YES;
                     break;
                  }
               }
               if (gotcha == NO)
               {
                  if (unlink(fullname) == -1)
                  {
                     if (errno != ENOENT)
                     {
                        trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  "Failed to unlink() `%s' : %s",
                                  fullname, strerror(errno));
                     }
                  }
                  else
                  {
#ifdef _DELETE_LOG
                     size_t dl_real_size;
#endif

                     deleted_files++;
#ifdef HAVE_STATX
                     deleted_size += stat_buf.stx_size;
#else
                     deleted_size += stat_buf.st_size;
#endif
#ifdef _DELETE_LOG
                     if (dl.fd == -1)
                     {
                        delete_log_ptrs(&dl);
                     }
                     (void)strcpy(dl.file_name, p_dir->d_name);
                     (void)snprintf(dl.host_name, MAX_HOSTNAME_LENGTH + 4 + 1,
                                    "%-*s %03x",
                                    MAX_HOSTNAME_LENGTH, fsa->host_alias,
                                    MIRROR_REMOVE);
# ifdef HAVE_STATX
                     *dl.file_size = stat_buf.stx_size;
# else
                     *dl.file_size = stat_buf.st_size;
# endif
                     *dl.job_id = db.id.job;
                     *dl.dir_id = dir_id;
                     *dl.input_time = db.creation_time;
                     *dl.split_job_counter = db.split_job_counter;
                     *dl.unique_number = db.unique_number;
                     *dl.file_name_length = strlen(p_dir->d_name);
                     dl_real_size = *dl.file_name_length + dl.size +
                                    snprintf((dl.file_name + *dl.file_name_length + 1),
                                             MAX_FILENAME_LENGTH + 1,
                                             "%s%c(%s %d)",
                                             SEND_FILE_LOC, SEPARATOR_CHAR,
                                             __FILE__, __LINE__);
                     if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "write() error : %s", strerror(errno));
                     }
#endif
                  }
               }
            }
         }
      }
   } /* while ((p_dir = readdir(dp)) != NULL) */

   if (deleted_files)
   {
      WHAT_DONE("deleted", deleted_size, deleted_files);
   }

   if (errno == EBADF)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to readdir() `%s' : %s",
                 db.target_dir, strerror(errno));
   }

   /* Don't forget to close the directory. */
   if (closedir(dp) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed  to closedir() `%s' : %s",
                 db.target_dir, strerror(errno));
   }

   return;
}
