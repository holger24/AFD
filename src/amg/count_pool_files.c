/*
 *  count_pool_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
extern unsigned int               max_file_buffer;
extern off_t                      *file_size_pool;
extern time_t                     *file_mtime_pool;
extern char                       **file_name_pool;
extern unsigned char              *file_length_pool;
extern struct fileretrieve_status *fra;
#endif
extern struct directory_entry     *de;
#ifdef _DISTRIBUTION_LOG
extern unsigned int               max_jobs_per_file;
# ifndef _WITH_PTHREAD
extern struct file_dist_list      **file_dist_pool;
# endif
#endif


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
                  struct stat   stat_buf;
                  struct dirent *p_dir;

                  work_ptr = pool_dir + strlen(pool_dir);
                  *(work_ptr++) = '/';
                  errno = 0;
                  while ((p_dir = readdir(dp)) != NULL)
                  {
                     if (p_dir->d_name[0] != '.')
                     {
                        (void)strcpy(work_ptr, p_dir->d_name);
                        if (stat(pool_dir, &stat_buf) == -1)
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      "Failed to stat() `%s' : %s",
                                      pool_dir, strerror(errno));
                        }
                        else
                        {
#ifndef _WITH_PTHREAD
                           if ((file_counter + 1) > max_file_buffer)
                           {
# ifdef _DISTRIBUTION_LOG
                              int          k, m;
                              size_t       tmp_val;
                              unsigned int prev_max_file_buffer = max_file_buffer;
# endif

                              if ((file_counter + 1) > fra[de[*dir_pos].fra_pos].max_copied_files)
                              {
                                 system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                            _("Hmmm, file_counter %d is larger then max_copied_files %u."),
                                            file_counter + 1,
                                            fra[de[*dir_pos].fra_pos].max_copied_files);
                                 max_file_buffer = file_counter + 1;
                              }
                              else
                              {
                                 if ((max_file_buffer + FILE_BUFFER_STEP_SIZE) >= fra[de[*dir_pos].fra_pos].max_copied_files)
                                 {
                                    max_file_buffer = fra[de[*dir_pos].fra_pos].max_copied_files;
                                 }
                                 else
                                 {
                                    max_file_buffer += FILE_BUFFER_STEP_SIZE;
                                 }
                              }
                              REALLOC_RT_ARRAY(file_name_pool, max_file_buffer,
                                               MAX_FILENAME_LENGTH, char);
                              if ((file_length_pool = realloc(file_length_pool,
                                                              max_file_buffer * sizeof(unsigned char))) == NULL)
                              {
                                 system_log(FATAL_SIGN, __FILE__, __LINE__,
                                            _("realloc() error : %s"),
                                            strerror(errno));
                                 exit(INCORRECT);
                              }
                              if ((file_mtime_pool = realloc(file_mtime_pool,
                                                             max_file_buffer * sizeof(off_t))) == NULL)
                              {
                                 system_log(FATAL_SIGN, __FILE__, __LINE__,
                                            _("realloc() error : %s"),
                                            strerror(errno));
                                 exit(INCORRECT);
                              }
                              if ((file_size_pool = realloc(file_size_pool,
                                                            max_file_buffer * sizeof(off_t))) == NULL)
                              {
                                 system_log(FATAL_SIGN, __FILE__, __LINE__,
                                            _("realloc() error : %s"),
                                            strerror(errno));
                                 exit(INCORRECT);
                              }
# ifdef _DISTRIBUTION_LOG
#  ifdef RT_ARRAY_STRUCT_WORKING
                              REALLOC_RT_ARRAY(file_dist_pool, max_file_buffer,
                                               NO_OF_DISTRIBUTION_TYPES,
                                               struct file_dist_list);
#  else
                              if ((file_dist_pool = (struct file_dist_list **)realloc(file_dist_pool, max_file_buffer * NO_OF_DISTRIBUTION_TYPES * sizeof(struct file_dist_list *))) == NULL)
                              {
                                 system_log(FATAL_SIGN, __FILE__, __LINE__,
                                            _("realloc() error : %s"),
                                            strerror(errno));
                                 exit(INCORRECT);
                              }
                              if ((file_dist_pool[0] = (struct file_dist_list *)realloc(file_dist_pool[0], max_file_buffer * NO_OF_DISTRIBUTION_TYPES * sizeof(struct file_dist_list))) == NULL)
                              {
                                 system_log(FATAL_SIGN, __FILE__, __LINE__,
                                            _("realloc() error : %s"),
                                            strerror(errno));
                                 exit(INCORRECT);
                              }
                              for (k = 0; k < max_file_buffer; k++)
                              {
                                 file_dist_pool[k] = file_dist_pool[0] + (k * NO_OF_DISTRIBUTION_TYPES);
                              }
#  endif
                              tmp_val = max_jobs_per_file * sizeof(unsigned int);
                              for (k = prev_max_file_buffer; k < max_file_buffer; k++)
                              {
                                 for (m = 0; m < NO_OF_DISTRIBUTION_TYPES; m++)
                                 {
                                    if (((file_dist_pool[k][m].jid_list = malloc(tmp_val)) == NULL) ||
                                        ((file_dist_pool[k][m].proc_cycles = malloc(max_jobs_per_file)) == NULL))
                                    {
                                       system_log(FATAL_SIGN, __FILE__, __LINE__,
                                                  _("malloc() error : %s"),
                                                  strerror(errno));
                                       exit(INCORRECT);
                                    }
                                    file_dist_pool[k][m].no_of_dist = 0;
                                 }
                              }
# endif
                           }
#endif
                           file_length_pool[file_counter] = strlen(p_dir->d_name);
                           (void)memcpy(file_name_pool[file_counter],
                                        p_dir->d_name,
                                        (size_t)(file_length_pool[file_counter] + 1));
                           file_size_pool[file_counter] = stat_buf.st_size;
                           file_mtime_pool[file_counter] = stat_buf.st_mtime;
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
