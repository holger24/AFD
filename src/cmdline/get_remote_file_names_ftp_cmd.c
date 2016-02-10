/*
 *  get_remote_file_names_ftp_cmd.c - Part of AFD, an automatic file
 *                                    distribution program.
 *  Copyright (c) 2000 - 2015 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_remote_file_names_ftp_cmd - retrieves filename, size and date
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
 **   20.08.2000 H.Kiehl Created
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
#include "ftpdefs.h"

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
static int                  check_list(char *, off_t *);


/*################### get_remote_file_names_ftp_cmd() ###################*/
int
get_remote_file_names_ftp_cmd(off_t *file_size_to_retrieve)
{
   int  files_to_retrieve = 0,
        i,
        status,
        type;
   char *nlist = NULL,
        *p_end,
        *p_list;

   /*
    * Get a directory listing from the remote site so we can see
    * what files are there.
    */
#ifdef WITH_SSL
   if (db.auth == BOTH)
   {
      type = NLIST_CMD | BUFFERED_LIST | ENCRYPT_DATA;
   }
   else
   {
#endif
      type = NLIST_CMD | BUFFERED_LIST;
#ifdef WITH_SSL
   }
#endif
   if ((status = ftp_list(db.ftp_mode, type, &nlist)) != SUCCESS)
   {
      if (status == 550)
      {
         trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   _("Failed to send NLST command (%d)."), status);
         (void)ftp_quit();
         exit(TRANSFER_SUCCESS);
      }
      else
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   _("Failed to send NLST command (%d)."), status);
         (void)ftp_quit();
         exit(LIST_ERROR);
      }
   }
   else
   {
      if (db.verbose == YES)
      {
         trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   _("Send NLST command."));
      }
   }

   /*
    * Some systems return 550 for the NLST command when no files
    * are found, others return 125 (ie. success) but do not return
    * any data. So check here if this is the second case.
    */
   if (nlist == NULL)
   {
      trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
                _("No files found (%d)."), status);
      (void)ftp_quit();
      exit(TRANSFER_SUCCESS);
   }

   /* Reduce the list to what is really required. */
   *file_size_to_retrieve = 0;
   p_list = nlist;
   do
   {
      p_end = p_list;
      while ((*p_end != '\n') && (*p_end != '\r') && (*p_end != '\0'))
      {
         p_end++;
      }
      if (*p_end != '\0')
      {
         *p_end = '\0';
         for (i = 0; i < db.no_of_files; i++)
         {
            if ((pmatch(db.filename[i], p_list, NULL) == 0) &&
                (check_list(p_list, file_size_to_retrieve) == 0))
            {
               files_to_retrieve++;
               break;
            }
         }
         p_list = p_end + 1;
         while ((*p_list == '\r') || (*p_list == '\n'))
         {
            p_list++;
         }
      }
      else
      {
         p_list = p_end;
      }
   } while (*p_list != '\0');

   free(nlist);

   return(files_to_retrieve);
}


/*+++++++++++++++++++++++++++++ check_list() ++++++++++++++++++++++++++++*/
static int
check_list(char *file, off_t *file_size_to_retrieve)
{
   static int check_size = YES;

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
         (void)ftp_quit();
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
              (void)ftp_quit();
              exit(ALLOC_ERROR);
           }
           no_of_listed_files = (int *)ptr;
           ptr += AFD_WORD_OFFSET;
           rl = (struct filename_list *)ptr;
        }

   (void)strcpy(rl[*no_of_listed_files].file_name, file);
   if (check_size == YES)
   {
      int   status;
      off_t size;

      if ((status = ftp_size(file, &size)) == SUCCESS)
      {
         rl[*no_of_listed_files].size = size;
         *file_size_to_retrieve += size;
      }
      else if (timeout_flag == ON)
           {
              /* We have lost connection. */
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                        _("Failed to get date of file `%s'."), file);
              (void)ftp_quit();
              exit(TIMEOUT_ERROR);
           }
      else if ((status == 500) || (status == 502))
           {
              check_size = NO;
              rl[*no_of_listed_files].size = -1;
           }
           else
           {
              rl[*no_of_listed_files].size = -1;
           }
   }
   else
   {
      rl[*no_of_listed_files].size = -1;
   }
   (*no_of_listed_files)++;

   return(0);
}
