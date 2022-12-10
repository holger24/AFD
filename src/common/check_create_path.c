/*
 *  check_create_path.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2004 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_create_path - checks if the path exists, if not it is created
 **
 ** SYNOPSIS
 **   int check_create_path(char   *path,
 **                         mode_t permissions,
 **                         char   **error_ptr,
 **                         int    create_dir,
 **                         int    check_write_access,
 **                         char   *created_path)
 **
 ** DESCRIPTION
 **   The function check_create_path() checks if the given path exists
 **   and if not it is created recursively.
 **
 ** RETURN VALUES
 **   Returns SUCCESS when the directory does already exist and access
 **   permissions are correct. It returns the following other values:
 **   CREATED_DIR - if it had to create one or more elements of the directory.
 **   NO_ACCESS   - is returned when it does not have access permissions.
 **   STAT_ERROR  - if stat() fails.
 **   MKDIR_ERROR - fails to create the path.
 **   CHOWN_ERROR - the chown() call failed.
 **   ALLOC_ERROR - fails to allocate memory.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.03.2004 H.Kiehl Created.
 **   31.10.2006 H.Kiehl Fully restore original diretory and instead return
 **                      position in directory name where we think it is
 **                      wrong.
 **   17.11.2006 H.Kiehl Additional parameter to control creation of
 **                      directory.
 **   14.04.2009 H.Kiehl We do not need write access for directories where
 **                      we do not delete files. Added parameter
 **                      check_write_access for this case.
 **
 */
DESCR__E_M3

#include <string.h>
#include <stdlib.h>
#ifdef HAVE_STATX
# include <fcntl.h>                     /* Definition of AT_* constants */
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>


/*######################## check_create_path() ##########################*/
int
check_create_path(char   *path,
                  mode_t permissions,
                  char   **error_ptr,
                  int    create_dir,
                  int    check_write_access,
                  char   *created_path)
{
   int mode,
       ret = SUCCESS;

   if (check_write_access == YES)
   {
      mode = R_OK | W_OK | X_OK;
   }
   else
   {
      mode = R_OK | X_OK;
   }
   *error_ptr = NULL;
   if (eaccess(path, mode) < 0)
   {
      if ((errno == ENOENT) && (create_dir == YES))
      {
         int   ii = 0,
               new_size,
               dir_length,
               dir_counter = 0,
               do_chown = NO,
               failed_to_create_dir = NO,
               error_condition = 0;
         uid_t owner;
         gid_t group;
         char  **dir_ptr = NULL,
               **tmp_ptr;

         do
         {
            if ((ii % 10) == 0)
            {
               new_size = ((ii / 10) + 1) * 10 * sizeof(char *);
               tmp_ptr = dir_ptr;
               if ((tmp_ptr = realloc(dir_ptr, new_size)) == NULL)
               {
                  free(dir_ptr);
                  return(ALLOC_ERROR);
               }
               dir_ptr = tmp_ptr;
            }
            dir_length = strlen(path);
            dir_ptr[ii] = path + dir_length;
            while (dir_length > 0)
            {
               dir_ptr[ii]--; dir_length--;
               if ((*dir_ptr[ii] == '/') && (*(dir_ptr[ii] + 1) != '\0'))
               {
                  *dir_ptr[ii] = '\0';
                  dir_counter++; ii++;
                  break;
               }
            }
            if (dir_length <= 0)
            {
               break;
            }
         } while (((error_condition = eaccess(path, mode)) < 0) &&
                  (errno == ENOENT));

         if ((error_condition < 0) && (errno != ENOENT))
         {
            if (ii > 0)
            {
               int i;

               *error_ptr = dir_ptr[ii - 1];
               for (i = 0; i < ii; i++)
               {
                  *dir_ptr[i] = '/';
               }
            }
            free(dir_ptr);
            return(NO_ACCESS);
         }

         if (permissions == 0)
         {
#ifdef HAVE_STATX
            struct statx stat_buf;
#else
            struct stat  stat_buf;
#endif

#ifdef HAVE_STATX
            if (statx(0, path, AT_STATX_SYNC_AS_STAT,
                      STATX_MODE | STATX_UID | STATX_GID, &stat_buf) == -1)
#else
            if (stat(path, &stat_buf) == -1)
#endif
            {
               if (ii > 0)
               {
                  int i;

                  *error_ptr = dir_ptr[ii - 1];
                  for (i = 0; i < ii; i++)
                  {
                     *dir_ptr[i] = '/';
                  }
               }
               free(dir_ptr);
               return(STAT_ERROR);
            }
#ifdef HAVE_STATX
            permissions = stat_buf.stx_mode;
            owner = stat_buf.stx_uid;
            group = stat_buf.stx_gid;
#else
            permissions = stat_buf.st_mode;
            owner = stat_buf.st_uid;
            group = stat_buf.st_gid;
#endif
            do_chown = YES;
         }

         /*
          * Try to create all missing directory names recursively.
          */
         for (ii = (dir_counter - 1); ii >= 0; ii--)
         {
            *dir_ptr[ii] = '/';
            if (failed_to_create_dir == NO)
            {
               if ((mkdir(path, permissions) == -1) && (errno != EEXIST))
               {
                  failed_to_create_dir = YES;
               }
               else
               {
                  if (do_chown == YES)
                  {
                     if (chown(path, owner, group) == -1)
                     {
                        ret = CHOWN_ERROR;
                     }
                     else
                     {
                        ret = SUCCESS;
                     }
                  }
               }
            }
            else
            {
               if ((*error_ptr == NULL) && (ii > 0))
               {
                  *error_ptr = dir_ptr[ii];
               }
            }
         }
         if (failed_to_create_dir == NO)
         {
            ret = CREATED_DIR;
            if (created_path != NULL)
            {
               (void)strcpy(created_path, dir_ptr[0] + 1);
            }
         }
         else
         {
            ret = MKDIR_ERROR;
         }
         free(dir_ptr);
      }
      else
      {
         ret = NO_ACCESS;
      }
   }
   return(ret);
}
