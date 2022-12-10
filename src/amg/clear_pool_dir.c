/*
 *  clear_pool_dir.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2022 Deutscher Wetterdienst (DWD),
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
 **   clear_pool_dir - moves files left in the pool directory back
 **                    to their original directories
 **
 ** SYNOPSIS
 **   void clear_pool_dir(void)
 **
 ** DESCRIPTION
 **   The function clear_pool_dir() tries to moves files back to
 **   their original directories from the pool directory that have
 **   been left after a crash. If it cannot determine the original
 **   directory or this directory simply does not exist, these files
 **   will be deleted.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.05.1998 H.Kiehl Created
 **   28.08.2003 H.Kiehl Adapted to CRC-32 directory ID's.
 **   21.01.2009 H.Kiehl No need to open DIR_NAME_FILE twice.
 **   03.09.2013 H.Kiehl When we move files back report this to system log.
 **
 */
DESCR__E_M3

#include <stdio.h>                /* sprintf()                            */
#include <stdlib.h>               /* malloc(), free()                     */
#include <string.h>               /* strcpy(), strerror()                 */
#include <ctype.h>                /* isdigit()                            */
#include <sys/stat.h>             /* stat(), S_ISREG()                    */
#include <unistd.h>               /* read(), close(), rmdir(), R_OK ...   */
#include <dirent.h>               /* opendir(), closedir(), readdir(),    */
                                  /* DIR, struct dirent                   */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int                 *no_of_dir_names;
extern struct dir_name_buf *dnb;
extern char                *p_work_dir;

/* Local function prototypes. */
static int                 get_source_dir(char *, char *, unsigned int *);
static void                move_files_back(char *, char *);


/*########################## clear_pool_dir() ##########################*/
void
clear_pool_dir(void)
{
   char         pool_dir[MAX_PATH_LENGTH],
                orig_dir[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   (void)snprintf(pool_dir, MAX_PATH_LENGTH,
                  "%s%s%s", p_work_dir, AFD_FILE_DIR, AFD_TMP_DIR);

#ifdef HAVE_STATX
   if (statx(0, pool_dir, AT_STATX_SYNC_AS_STAT, 0, &stat_buf) == -1)
#else
   if (stat(pool_dir, &stat_buf) == -1)
#endif
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                 "Failed to statx() %s : %s",
#else
                 "Failed to stat() %s : %s",
#endif
                 pool_dir, strerror(errno));
   }
   else
   {
      DIR *dp;

      if ((dp = opendir(pool_dir)) == NULL)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to opendir() %s : %s", pool_dir, strerror(errno));
      }
      else
      {
         int           dnb_fd;
         unsigned int  dir_id;
         char          *work_ptr;
         struct dirent *p_dir;

         work_ptr = pool_dir + strlen(pool_dir);
         *(work_ptr++) = '/';
         *work_ptr = '\0';

         if (dnb == NULL)
         {
            size_t size = (DIR_NAME_BUF_SIZE * sizeof(struct dir_name_buf)) +
                          AFD_WORD_OFFSET;
            char   dir_name_file[MAX_PATH_LENGTH],
                   *p_dir_buf;

            /*
             * Map to the directory name database.
             */
            (void)strcpy(dir_name_file, p_work_dir);
            (void)strcat(dir_name_file, FIFO_DIR);
            (void)strcat(dir_name_file, DIR_NAME_FILE);
            if ((p_dir_buf = attach_buf(dir_name_file, &dnb_fd,
                                        &size, NULL, FILE_MODE, NO)) == (caddr_t) -1)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to mmap() to %s : %s",
                          dir_name_file, strerror(errno));
               (void)close(dnb_fd);
               (void)closedir(dp);

               return;
            }
            no_of_dir_names = (int *)p_dir_buf;
            p_dir_buf += AFD_WORD_OFFSET;
            dnb = (struct dir_name_buf *)p_dir_buf;
         }

         errno = 0;
         while ((p_dir = readdir(dp)) != NULL)
         {
            if (p_dir->d_name[0] == '.')
            {
               continue;
            }

            (void)strcpy(work_ptr, p_dir->d_name);
#ifdef MULTI_FS_SUPPORT
# ifdef HAVE_STATX
            if ((statx(0, pool_dir, AT_STATX_SYNC_AS_STAT | AT_SYMLINK_NOFOLLOW,
                       STATX_MODE, &stat_buf) != -1) &&
                (S_ISLNK(stat_buf.stx_mode) == 0))
# else
            if ((lstat(pool_dir, &stat_buf) != -1) &&
                (S_ISLNK(stat_buf.st_mode) == 0))
# endif
            {
#endif
               if (get_source_dir(p_dir->d_name, orig_dir, &dir_id) == INCORRECT)
               {
                  /* Remove it, no matter what it is. */
#ifdef _DELETE_LOG
                  remove_pool_directory(pool_dir, dir_id);
#else
                  (void)rec_rmdir(pool_dir);
#endif
               }
               else
               {
                  move_files_back(pool_dir, orig_dir);
               }
#ifdef MULTI_FS_SUPPORT
            }
#endif
            errno = 0;
         }

         if (errno)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not readdir() %s : %s",
                       pool_dir, strerror(errno));
         }
         if (closedir(dp) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not close directory %s : %s",
                       pool_dir, strerror(errno));
         }
      }
   }

   return;
}


/*+++++++++++++++++++++++++++ get_source_dir() ++++++++++++++++++++++++++*/
static int
get_source_dir(char *dir_name, char *orig_dir, unsigned int *dir_id)
{
   register int i = 0;

   *dir_id = 0;
   while ((dir_name[i] != '\0') && (dir_name[i] != '_'))
   {
      if (isxdigit((int)dir_name[i]) == 0)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Unable to determine the diretory ID for `%s'.", dir_name);
         return(INCORRECT);
      }
      i++;
   }
   if (dir_name[i] == '_')
   {
      i++;
      while ((dir_name[i] != '\0') && (dir_name[i] != '_'))
      {
         if (isxdigit((int)dir_name[i]) == 0)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Unable to determine the diretory ID for `%s'.",
                       dir_name);
            return(INCORRECT);
         }
         i++;
      }
      if (dir_name[i] == '_')
      {
         i++;
         while ((dir_name[i] != '\0') && (dir_name[i] != '_'))
         {
            if (isxdigit((int)dir_name[i]) == 0)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Unable to determine the diretory ID for `%s'.",
                          dir_name);
               return(INCORRECT);
            }
            i++;
         }
         if (dir_name[i] == '_')
         {
            int start;

            start = ++i;
            while (dir_name[i] != '\0')
            {
               if (isxdigit((int)dir_name[i]) == 0)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Unable to determine the diretory ID for `%s'.",
                             dir_name);
                  return(INCORRECT);
               }
               i++;
            }
            errno = 0;
            *dir_id = (unsigned int)strtoul(&dir_name[start], (char **)NULL, 16);
            if (errno == ERANGE)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Unable to determine the diretory ID for `%s'.",
                          dir_name);
               return(INCORRECT);
            }

            for (i = 0; i < *no_of_dir_names; i++)
            {
               if (*dir_id == dnb[i].dir_id)
               {
                  /*
                   * Before we say this is it, check if it still does
                   * exist!
                   */
                  if (eaccess(dnb[i].dir_name, R_OK | W_OK | X_OK) == -1)
                  {
                     system_log(INFO_SIGN, __FILE__, __LINE__,
                                "Cannot move files back to %s : %s",
                                dnb[i].dir_name, strerror(errno));
                     return(INCORRECT);
                  }

                  (void)strcpy(orig_dir, dnb[i].dir_name);
                  return(SUCCESS);
               }
            }
         } /* if (dir_name[i] == '_') */
      } /* if (dir_name[i] == '_') */
   } /* if (dir_name[i] == '_') */

   return(INCORRECT);
}


/*+++++++++++++++++++++++++++ move_files_back() +++++++++++++++++++++++++*/
static void
move_files_back(char *pool_dir, char *orig_dir)
{
   DIR *dp;

   if ((dp = opendir(pool_dir)) == NULL)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to opendir() %s : %s", pool_dir, strerror(errno));
   }
   else
   {
      int           ret;
      unsigned int  files_moved = 0;
      char          *orig_ptr,
                    *pool_ptr;
      struct dirent *p_dir;

      pool_ptr = pool_dir + strlen(pool_dir);
      if (*(pool_ptr - 1) != '/')
      {
         *(pool_ptr++) = '/';
      }
      orig_ptr = orig_dir + strlen(orig_dir);
      if (*(orig_ptr - 1) != '/')
      {
         *(orig_ptr++) = '/';
      }

      errno = 0;
      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] == '.')
         {
            continue;
         }

         (void)strcpy(pool_ptr, p_dir->d_name);
         (void)strcpy(orig_ptr, p_dir->d_name);

         if (((ret = move_file(pool_dir, orig_dir)) < 0) || (ret == 2))
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to move_file() %s to %s : %s [%d]",
                       pool_dir, orig_dir, strerror(errno), ret);
         }
         else
         {
            files_moved++;
         }
         errno = 0;
      }
      *pool_ptr = '\0';
      *orig_ptr = '\0';

      if (errno)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not readdir() %s : %s", pool_dir, strerror(errno));
      }
      if (closedir(dp) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not close directory %s : %s",
                    pool_dir, strerror(errno));
      }

      /*
       * After all files have been moved back to their original
       * directory, remove the directory from the pool directory.
       */
      if (rmdir(pool_dir) == -1)
      {
         if ((errno == ENOTEMPTY) || (errno == EEXIST))
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Hmm. Directory %s is not empty?! Will remove it!",
                       pool_dir);
            (void)rec_rmdir(pool_dir);
         }
         else
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Could not remove directory %s : %s",
                       pool_dir, strerror(errno));
         }
      }
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Moved %u files back to %s", files_moved, orig_dir);
   }

   return;
}
