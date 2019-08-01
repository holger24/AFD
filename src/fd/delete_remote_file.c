/*
 *  delete_remote_file.c - Part of AFD, an automatic file distribution
 *                         program.
 *  Copyright (c) 2014 - 2019 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   delete_remote_file - deletes a remote file
 **
 ** SYNOPSIS
 **   void delete_remote_file(int          type,
 **                           char         *file_name,
 **                           int          namelen,
 **                           unsigned int *files_deleted,
 **                           off_t        *file_size_deleted,
 **                           off_t        file_size)
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
 **   18.07.2014 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "fddefs.h"
#include "httpdefs.h"

/* External global variables. */
extern char                       msg_str[];
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif
extern struct job                 db;
extern struct filetransfer_status *fsa;
extern struct fileretrieve_status *fra;


/*######################### delete_remote_file() ########################*/
void
delete_remote_file(int          type,
                   char         *file_name,
                   int          namelen,
#ifdef _DELETE_LOG
                   int          delete_reason,
#endif
                   unsigned int *files_deleted,
                   off_t        *file_size_deleted,
                   off_t        file_size)
{
   if (delete_wrapper(file_name) == SUCCESS)
   {
#ifdef _DELETE_LOG
      char   procname[MAX_PROCNAME_LENGTH + 1];
      size_t dl_real_size;
#endif

      if (files_deleted != NULL)
      {
         (*files_deleted)++;
      }
      if ((file_size != -1) || (file_size_deleted != NULL))
      {
         *file_size_deleted += file_size;
      }

      if (fsa->debug > NORMAL_MODE)
      {
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                      "Deleted remote file %s.", file_name);
      }
#ifdef _DELETE_LOG
      if (type == FTP)
      {
         (void)strcpy(procname, GET_FILE_FTP);
      }
      else if (type == SFTP)
           {
              (void)strcpy(procname, GET_FILE_SFTP);
           }
      else if (type == HTTP)
           {
              (void)strcpy(procname, GET_FILE_HTTP);
           }
           else
           {
              trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                        "Unknown protocol type %d, cannot delete remote file %s",
                        type, file_name);
              return;
           }

      if (dl.fd == -1)
      {
         delete_log_ptrs(&dl);
      }
      (void)strcpy(dl.file_name, file_name);
      (void)snprintf(dl.host_name,
                     MAX_HOSTNAME_LENGTH + 4 + 1,
                     "%-*s %03x",
                     MAX_HOSTNAME_LENGTH, fsa->host_alias, delete_reason);
      if (file_size == -1)
      {
         *dl.file_size = 0;
      }
      else
      {
         *dl.file_size = file_size;
      }
      *dl.job_id = 0;
      *dl.dir_id = fra->dir_id;
      *dl.input_time = db.creation_time;
      *dl.split_job_counter = db.split_job_counter;
      *dl.unique_number = db.unique_number;
      *dl.file_name_length = namelen;
      dl_real_size = *dl.file_name_length + dl.size +
                     snprintf((dl.file_name + *dl.file_name_length + 1),
                              MAX_FILENAME_LENGTH + 1,
                              "%s%c(%s %d)",
                              procname, SEPARATOR_CHAR, __FILE__, __LINE__);
      if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "write() error : %s", strerror(errno));
      }
#endif
   }

   return;
}
