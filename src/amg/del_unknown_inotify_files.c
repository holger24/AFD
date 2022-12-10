/*
 *  del_unknown_inotify_files.c - Part of AFD, an automatic file
 *                                distribution program.
 *  Copyright (c) 2013 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   del_unknown_inotify_files - 
 **
 ** SYNOPSIS
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   28.05.2013 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* snprintf(), sprintf()              */
#include <string.h>                /* strcpy(), strerror()               */
#include <stdlib.h>                /* exit()                             */
#include <unistd.h>                /* unlink(), write()                  */
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>                /* Definition of AT_* constants       */
#endif
#include <sys/stat.h>              /* stat(), S_ISREG()                  */
#include <dirent.h>                /* opendir(), closedir(), readdir(),  */
                                   /* DIR, struct dirent                 */
#include <errno.h>
#include "amgdefs.h"


/* External global variables. */
extern int                        no_of_inotify_dirs;
extern struct inotify_watch_list  *iwl;
extern struct directory_entry     *de;
extern struct fileretrieve_status *fra;
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif


/*#################### del_unknown_inotify_files() ######################*/
void
del_unknown_inotify_files(time_t current_time)
{
   int i;

   for (i = 0; i < no_of_inotify_dirs; i++)
   {
      if ((fra[de[iwl[i].de_pos].fra_pos].delete_files_flag & UNKNOWN_FILES) &&
          (fra[de[iwl[i].de_pos].fra_pos].unknown_file_time != -2) &&
          ((de[iwl[i].de_pos].flag & ALL_FILES) == 0))
      {
         DIR *dp;

         if ((dp = opendir(de[iwl[i].de_pos].dir)) == NULL)
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, current_time,
                        _("Failed to opendir() `%s' : %s"),
                        de[iwl[i].de_pos].dir, strerror(errno));
         }
         else
         {
            char          fullname[MAX_PATH_LENGTH],
                          *work_ptr;
            struct dirent *p_dir;

            (void)strcpy(fullname, de[iwl[i].de_pos].dir);
            work_ptr = fullname + strlen(fullname);
            *work_ptr++ = '/';

            while ((p_dir = readdir(dp)) != NULL)
            {
#ifdef LINUX
               if ((p_dir->d_type == DT_REG) && (p_dir->d_name[0] != '.'))
#else
               if (p_dir->d_name[0] != '.')
#endif
               {
                  int gotcha = NO,
                      j,
                      k,
                      ret;

                  for (j = 0; j < de[iwl[i].de_pos].nfg; j++)
                  {
                     for (k = 0; ((j < de[iwl[i].de_pos].nfg) && (k < de[iwl[i].de_pos].fme[j].nfm)); k++)
                     {
                        if ((ret = pmatch(de[iwl[i].de_pos].fme[j].file_mask[k],
                                          p_dir->d_name, &current_time)) == 0)
                        {
                           gotcha = YES;
                           j = de[iwl[i].de_pos].nfg;
                        }
                        else if (ret == 1)
                             {
                                break;
                             }
                     }
                  }
                  if (gotcha == NO)
                  {
#ifdef HAVE_STATX
                     struct statx stat_buf;
#else
                     struct stat  stat_buf;
#endif
                     (void)strcpy(work_ptr, p_dir->d_name);
#ifdef HAVE_STATX
                     if (statx(0, fullname, AT_STATX_SYNC_AS_STAT,
# ifndef LINUX
                               STATX_MODE |
# endif
                               STATX_SIZE | STATX_MTIME, &stat_buf) == -1)
#else
                     if (stat(fullname, &stat_buf) == -1)
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
                     }
                     else
                     {
#ifndef LINUX
                        /* Sure it is a normal file? */
# ifdef HAVE_STATX
                        if (S_ISREG(stat_buf.stx_mode))
# else
                        if (S_ISREG(stat_buf.st_mode))
# endif
#endif
                        {
#ifdef HAVE_STATX
                           time_t diff_time = current_time - stat_buf.stx_mtime.tv_sec;
#else
                           time_t diff_time = current_time - stat_buf.st_mtime;
#endif

                           if ((diff_time > fra[de[iwl[i].de_pos].fra_pos].unknown_file_time) &&
                               (diff_time > DEFAULT_TRANSFER_TIMEOUT))
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
#ifdef _DELETE_LOG
                              else
                              {
                                 size_t file_name_length = strlen(p_dir->d_name),
                                        dl_real_size;

                                 (void)my_strncpy(dl.file_name, p_dir->d_name,
                                                  file_name_length + 1);
                                 (void)snprintf(dl.host_name,
                                                MAX_HOSTNAME_LENGTH + 4 + 1,
                                                "%-*s %03x",
                                                MAX_HOSTNAME_LENGTH, "-",
                                                (fra[de[iwl[i].de_pos].fra_pos].in_dc_flag & UNKNOWN_FILES_IDC) ?  DEL_UNKNOWN_FILE : DEL_UNKNOWN_FILE_GLOB);
# ifdef HAVE_STATX
                                 *dl.file_size = stat_buf.stx_size;
# else
                                 *dl.file_size = stat_buf.st_size;
# endif
                                 *dl.dir_id = de[iwl[i].de_pos].dir_id;
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
                                                         DIR_CHECK,
                                                         SEPARATOR_CHAR,
                                                         (pri_time_t)diff_time,
                                                         __FILE__, __LINE__);
                                 if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                                 {
                                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                                               _("write() error : %s"),
                                               strerror(errno));
                                 }
                              }
#endif
                           }
                        } /* if (S_ISREG(stat_buf.st_mode)) */
                     }
                  } /* gotcha == NO */
               }
            } /* while ((p_dir = readdir(dp)) != NULL) */

            /* When using readdir() it can happen that it always returns     */
            /* NULL, due to some error. We want to know if this is the case. */
            if (errno == EBADF)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          _("Failed to readdir() `%s' : %s"),
                          de[iwl[i].de_pos].dir, strerror(errno));
            }

            /* Don't forget to close the directory. */
            if (closedir(dp) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed  to closedir() `%s' : %s"),
                          de[iwl[i].de_pos].dir, strerror(errno));
            }
         }
      }
   }

   return;
}
