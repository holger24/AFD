/*
 *  handle_dupcheck_delete.c - Part of AFD, an automatic file distribution
 *                             program.
 *  Copyright (c) 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   handle_dupcheck_delete - handles logging and removal of data
 **
 ** SYNOPSIS
 **   void handle_dupcheck_delete(char *fullname, char *file_name, off_t file_size)
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
 **   28.11.2022 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>         /* rename()                                   */
#include <string.h>        /* strcpy()                                   */
#include <unistd.h>        /* write(), unlink()                          */
#include <sys/stat.h>      /* mkdir()                                    */
#include <errno.h>
#include "fddefs.h"

/* External global variables. */
extern char              *p_work_dir;
extern struct delete_log dl;
extern struct job        db;


/*####################### handle_dupcheck_delete() ######################*/
void
handle_dupcheck_delete(char  *proc_name,
                       char  *host_alias,
                       char  *fullname,
                       char  *file_name,
                       off_t  file_size,
                       time_t file_mtime,
                       time_t now)
{
#ifdef WITH_DUP_CHECK
   if (db.dup_check_flag & DC_DELETE)
   {
# ifdef _DELETE_LOG
      time_t diff_time;
      size_t dl_real_size;

      if (now < file_mtime)
      {
         diff_time = 0L;
      }
      else
      {
         diff_time = now - file_mtime;
      }
      if (dl.fd == -1)
      {
         delete_log_ptrs(&dl);
      }
      (void)strcpy(dl.file_name, file_name);
      (void)snprintf(dl.host_name, MAX_HOSTNAME_LENGTH + 4 + 1,
                     "%-*s %03x", MAX_HOSTNAME_LENGTH, host_alias, DUP_OUTPUT);
      *dl.file_size = file_size;
      *dl.job_id = db.id.job;
      *dl.dir_id = 0;
      *dl.input_time = db.creation_time;
      *dl.split_job_counter = db.split_job_counter;
      *dl.unique_number = db.unique_number;
      *dl.file_name_length = strlen(file_name);
      dl_real_size = snprintf((dl.file_name + *dl.file_name_length + 1),
                              MAX_FILENAME_LENGTH + 1,
#  if SIZEOF_TIME_T == 4
                              "%s%c>%ld (%s %d)",
#  else
                              "%s%c>%lld (%s %d)",
#  endif
                              proc_name, SEPARATOR_CHAR, (pri_time_t)diff_time,
                              __FILE__, __LINE__);
      if (dl_real_size > (MAX_FILENAME_LENGTH + 1))
      {
         dl_real_size = MAX_FILENAME_LENGTH + 1;
      }
      dl_real_size = *dl.file_name_length + dl.size + dl_real_size;
      if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "write() error : %s", strerror(errno));
      }
# endif
      if (unlink(fullname) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to unlink() duplicate file `%s' : %s",
                    fullname, strerror(errno));
      }
   }
   else if (db.dup_check_flag & DC_WARN)
        {
           trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                     "File `%s' is duplicate. #%x", file_name, db.id.job);
        }
   else if (db.dup_check_flag & DC_STORE)
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
                            "Failed to unlink() duplicate file `%s' : %s",
                            fullname, strerror(errno));
              }
           }
           else
           {
              (void)strcpy(p_end, file_name);
              if (rename(fullname, save_dir) == -1)
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to rename() `%s' to `%s' : %s #%x",
                            fullname, save_dir, strerror(errno), db.id.job);
                 if (unlink(fullname) == -1)
                 {
                    system_log(WARN_SIGN, __FILE__, __LINE__,
                               "Failed to unlink() duplicate file `%s'",
                               fullname, strerror(errno));
                 }
              }
           }
        }
#endif

   return;
}
