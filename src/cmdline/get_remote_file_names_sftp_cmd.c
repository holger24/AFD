/*
 *  get_remote_file_names_sftp_cmd.c - Part of AFD, an automatic file
 *                                     distribution program.
 *  Copyright (c) 2015 - 2018 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#include "aftpdefs.h"

DESCR__S_M3
/*
 ** NAME
 **   get_remote_file_names_sftp_cmd - retrieves filename, size and date
 **
 ** SYNOPSIS
 **   int get_remote_file_names_ftp_cmd(off_t *file_size_to_retrieve)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   On success the number of files that are to be retrieved. On error
 **   it will exit.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.06.2015 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                /* strcpy(), strerror(), memmove()    */
#include <stdlib.h>                /* malloc(), realloc(), free()        */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include "sftpdefs.h"

#define REMOTE_LIST_STEP_SIZE 10

/* External global variables */
extern int                  *no_of_listed_files,
                            sys_log_fd,
                            timeout_flag,
                            trans_db_log_fd;
extern char                 msg_str[],
                            *p_work_dir;
extern struct data          db;
extern struct filename_list *rl;

/* Local function prototypes. */
static int                  check_list(char *, off_t, off_t *);


/*##################### get_remote_file_names_sftp_cmd() ####################*/
int
get_remote_file_names_sftp_cmd(off_t *file_size_to_retrieve)
{
   int  files_to_retrieve = 0,
        i,
        status;

   /*
    * Get a directory listing from the remote site so we can see
    * what files are there.
    */
   *file_size_to_retrieve = 0;
   if ((status = sftp_open_dir("")) == SUCCESS)
   {
      char        filename[MAX_FILENAME_LENGTH];
      struct stat stat_buf;

      if (db.verbose == YES)
      {
         trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   _("Opened remote directory."));
      }
      while ((status = sftp_readdir(filename, &stat_buf)) == SUCCESS)
      {
         if ((((filename[0] == '.') && (filename[1] == '\0')) ||
              ((filename[0] == '.') && (filename[1] == '.') &&
               (filename[2] == '\0'))) ||
             (S_ISREG(stat_buf.st_mode) == 0))
         {
            continue;
         }
         else
         {
            for (i = 0; i < db.no_of_files; i++)
            {
               if ((pmatch(db.filename[i], filename, NULL) == 0) &&
                           (check_list(filename, stat_buf.st_size,
                                       file_size_to_retrieve) == 0))
               {
                  files_to_retrieve++;
                  break;
               }
            }
         }
      }

      if (sftp_close_dir() == INCORRECT)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   _("Failed to close remote directory."));
      }
      else
      {
         if (db.verbose == YES)
         {
            trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
                      _("Closed remote directory."));
         }
      }
   }
   else
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                _("Failed to open remote directory for reading (%d)."), status);
      sftp_quit();
      exit(LIST_ERROR);
   }

   return(files_to_retrieve);
}


/*+++++++++++++++++++++++++++++ check_list() ++++++++++++++++++++++++++++*/
static int
check_list(char *file, off_t size, off_t *file_size_to_retrieve)
{
   if (rl == NULL)
   {
      size_t rl_size;
      char   *ptr;

      rl_size = (REMOTE_LIST_STEP_SIZE * sizeof(struct filename_list)) +
                AFD_WORD_OFFSET;
      if ((ptr = malloc(rl_size)) == NULL)
      {
         (void)rec(sys_log_fd, ERROR_SIGN, _("malloc() error : %s (%s %d)\n"),
                   strerror(errno), __FILE__, __LINE__);
         sftp_quit();
         exit(ALLOC_ERROR);
      }
      no_of_listed_files = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      rl = (struct filename_list *)ptr;
      *no_of_listed_files = 0;
   }
   else if ((*no_of_listed_files != 0) &&
            ((*no_of_listed_files % REMOTE_LIST_STEP_SIZE) == 0))
        {
           char   *ptr;
           size_t rl_size = (((*no_of_listed_files / REMOTE_LIST_STEP_SIZE) + 1) *
                             REMOTE_LIST_STEP_SIZE * sizeof(struct filename_list)) +
                            AFD_WORD_OFFSET;

           ptr = (char *)rl - AFD_WORD_OFFSET;
           if ((ptr = realloc(ptr, rl_size)) == NULL)
           {
              (void)rec(sys_log_fd, ERROR_SIGN,
                        _("realloc() error : %s (%s %d)\n"),
                        strerror(errno), __FILE__, __LINE__);
              sftp_quit();
              exit(ALLOC_ERROR);
           }
           no_of_listed_files = (int *)ptr;
           ptr += AFD_WORD_OFFSET;
           rl = (struct filename_list *)ptr;
        }

   (void)strcpy(rl[*no_of_listed_files].file_name, file);
   rl[*no_of_listed_files].size = size;
   *file_size_to_retrieve += size;
   (*no_of_listed_files)++;

   return(0);
}
