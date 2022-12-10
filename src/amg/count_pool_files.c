/*
 *  count_pool_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   count_pool_files - counts the number of files in the pool directory
 **
 ** SYNOPSIS
 **   int count_pool_files(int *dir_pos, char *pool_dir)
 **
 ** DESCRIPTION
 **   The function count_pool_files() counts the files in a pool directory.
 **   This is useful in a situation when the disk is full and dir_check
 **   dies with a SIGBUS when trying to copy files via mmap.
 **
 ** RETURN VALUES
 **   Returns the number of files in the directory and the directory
 **   number.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.04.2001 H.Kiehl Created
 **   28.08.2003 H.Kiehl Modified to take account for CRC-32 directory
 **                      ID's.
 **   08.08.2009 H.Kiehl Handle case when file_*_pool are not large enough.
 **
 */
DESCR__E_M3

#include <stdio.h>                /* sprintf()                            */
#include <stdlib.h>               /* malloc(), free()                     */
#include <string.h>               /* strcpy(), strerror()                 */
#include <ctype.h>                /* isxdigit()                           */
#include <sys/stat.h>             /* stat(), S_ISREG()                    */
#include <unistd.h>               /* read(), close(), rmdir()             */
#include <dirent.h>               /* opendir(), closedir(), readdir(),    */
                                  /* DIR, struct dirent                   */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int                        no_of_local_dirs;
#ifndef _WITH_PTHREAD
extern off_t                      *file_size_pool;
extern time_t                     *file_mtime_pool;
extern char                       **file_name_pool;
extern unsigned char              *file_length_pool;
extern struct fileretrieve_status *fra;
#endif
extern struct directory_entry     *de;


/*######################### count_pool_files() #########################*/
int
#ifndef _WITH_PTHREAD
count_pool_files(int *dir_pos, char *pool_dir)
#else
count_pool_files(int           *dir_pos,
                 char          *pool_dir,
                 off_t         *file_size_pool,
                 time_t        *file_mtime_pool,
                 char          **file_name_pool,
                 unsigned char *file_length_pool)
#endif
{
   int    file_counter = 0;
   size_t length;

   /* First determine the directory number. */
   if ((length = strlen(pool_dir)) > 0)
   {
      char *ptr;

      length -= 2;
      ptr = pool_dir + length;
      while ((*ptr != '_') && (isxdigit((int)(*ptr))) && (length > 0))
      {
         ptr--; length--;
      }
      if (*ptr == '_')
      {
         unsigned int dir_id;

         ptr++;
         errno = 0;
         dir_id = (unsigned int)strtoul(ptr, (char **)NULL, 16);
         if (errno == ERANGE)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "`%s' does not look like a normal pool directory.",
                       pool_dir);
         }
         else
         {
            int gotcha = NO,
                i;

            for (i = 0; i < no_of_local_dirs; i++)
            {
               if (de[i].dir_id == dir_id)
               {
                  gotcha = YES;
                  break;
               }
            }
            if (gotcha == YES)
            {
               DIR *dp;

               *dir_pos = i;
               if ((dp = opendir(pool_dir)) == NULL)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Failed to opendir() `%s' : %s",
                             pool_dir, strerror(errno));
               }
               else
               {
                  char          *work_ptr;
#ifdef HAVE_STATX
                  struct statx  stat_buf;
#else
                  struct stat   stat_buf;
#endif
                  struct dirent *p_dir;

                  work_ptr = pool_dir + strlen(pool_dir);
                  *(work_ptr++) = '/';
                  errno = 0;
                  while ((p_dir = readdir(dp)) != NULL)
                  {
                     if (p_dir->d_name[0] != '.')
                     {
                        (void)strcpy(work_ptr, p_dir->d_name);
#ifdef HAVE_STATX
                        if (statx(0, pool_dir, AT_STATX_SYNC_AS_STAT,
                                  STATX_SIZE | STATX_MTIME, &stat_buf) == -1)
#else
                        if (stat(pool_dir, &stat_buf) == -1)
#endif
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                                      "Failed to statx() `%s' : %s",
#else
                                      "Failed to stat() `%s' : %s",
#endif
                                      pool_dir, strerror(errno));
                        }
                        else
                        {
                           check_file_pool_mem(file_counter + 1);
                           file_length_pool[file_counter] = strlen(p_dir->d_name);
                           (void)memcpy(file_name_pool[file_counter],
                                        p_dir->d_name,
                                        (size_t)(file_length_pool[file_counter] + 1));
#ifdef HAVE_STATX
                           file_size_pool[file_counter] = stat_buf.stx_size;
                           file_mtime_pool[file_counter] = stat_buf.stx_mtime.tv_sec;
#else
                           file_size_pool[file_counter] = stat_buf.st_size;
                           file_mtime_pool[file_counter] = stat_buf.st_mtime;
#endif
                           file_counter++;
                        }
                     }
                     errno = 0;
                  }
                  work_ptr[-1] = '\0';

                  if (errno)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Could not readdir() `%s' : %s",
                                pool_dir, strerror(errno));
                  }
                  if (closedir(dp) == -1)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Could not close directory `%s' : %s",
                                pool_dir, strerror(errno));
                  }
                  if (file_counter == 0)
                  {
                     /*
                      * If there are no files remove the directoy, handle_dir()
                      * will not do either, so it must be done here.
                      */
                     if (rmdir(pool_dir) == -1)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "Could not rmdir() `%s' : %s",
                                   pool_dir, strerror(errno));
                     }
                  }
               }
            }
            else
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Directory ID `%x' no longer in system, removing this job.",
                          dir_id);
               if (rec_rmdir(pool_dir) == -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Could not rec_rmdir() `%s' : %s",
                             pool_dir, strerror(errno));
               }
            }
         }
      }
   }
   else
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "`%s' does not look like a normal pool directory.", pool_dir);
   }

   return(file_counter);
}
