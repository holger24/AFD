/*
 *  check_disabled_dirs.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2020 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_disabled_dirs - check if there are any disabled dirs
 **
 ** SYNOPSIS
 **   int check_disabled_dirs(void)
 **
 ** DESCRIPTION
 **   This function checks if there are any disabled directories.
 **
 ** RETURN VALUES
 **   When it finds the DISABLED_DIR_FILE file and is able to read this
 **   it stores the disabled directory names.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.03.2020 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>             /* snprintf()                             */
#include <string.h>            /* strerror()                             */
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>            /* Definition of AT_* constants           */
#endif
#include <sys/stat.h>
#include <stdlib.h>            /* free()                                 */
#include <errno.h>

#define SDL_STEP_SIZE 20

/* External global variables. */
extern int  no_of_disabled_dirs;
extern char *p_work_dir,
            **disabled_dirs;


/*######################### check_disabled_dirs() #######################*/
int
check_disabled_dirs(void)
{
   static time_t disabled_dir_mtime = 0;
   char          disabled_dir_name[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx  stat_buf;
#else
   struct stat   stat_buf;
#endif

   if (snprintf(disabled_dir_name, MAX_PATH_LENGTH, "%s%s/%s",
                p_work_dir, ETC_DIR, DISABLED_DIR_FILE) >= MAX_PATH_LENGTH)
   {
      return(NO);
   }

#ifdef HAVE_STATX
   if (statx(0, disabled_dir_name, AT_STATX_SYNC_AS_STAT,
             STATX_MTIME, &stat_buf) == -1)
#else
   if (stat(disabled_dir_name, &stat_buf) == -1)
#endif
   {
      if (errno == ENOENT)
      {
         disabled_dir_mtime = 0;
         if (disabled_dirs != NULL)
         {
            FREE_RT_ARRAY(disabled_dirs);
            disabled_dirs = NULL;
            no_of_disabled_dirs = 0;

            return(YES);
         }
      }
      else
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                    _("Failed to statx() %s : %s"),
#else
                    _("Failed to stat() %s : %s"),
#endif
                    disabled_dir_name, strerror(errno));
      }
      return(NO);
   }

#ifdef HAVE_STATX
   if (stat_buf.stx_mtime.tv_sec != disabled_dir_mtime)
#else
   if (stat_buf.st_mtime != disabled_dir_mtime)
#endif
   {
      char *buffer;

      if (disabled_dirs != NULL)
      {
         FREE_RT_ARRAY(disabled_dirs);
         disabled_dirs = NULL;
         no_of_disabled_dirs = 0;
      }

      if (read_file_no_cr(disabled_dir_name, &buffer, NO,
                          __FILE__, __LINE__) != INCORRECT)
      {
         int  i;
         char *ptr = buffer;

         while (*ptr != '\0')
         {
            do
            {
               while (*ptr == '\n')
               {
                  ptr++;
               }
               while ((*ptr == ' ') || (*ptr == '\t'))
               {
                  ptr++;
               }
               if (*ptr == '#')
               {
                  while ((*ptr != '\n') && (*ptr != '\0'))
                  {
                     ptr++;
                  }
               }
               else
               {
                  if ((no_of_disabled_dirs == 0) && (disabled_dirs == NULL))
                  {
                     RT_ARRAY(disabled_dirs, SDL_STEP_SIZE,
                              MAX_DIR_ALIAS_LENGTH + 1, char);
                  }
                  else
                  {
                     if ((no_of_disabled_dirs % SDL_STEP_SIZE) == 0)
                     {
                        REALLOC_RT_ARRAY(disabled_dirs,
                                       no_of_disabled_dirs + SDL_STEP_SIZE,
                                       MAX_DIR_ALIAS_LENGTH + 1, char);
                     }
                  }
                  i = 0;
                  while ((i < MAX_DIR_ALIAS_LENGTH) &&
                         (*ptr != '\n') && (*ptr != '\0'))
                  {
                     disabled_dirs[no_of_disabled_dirs][i] = *ptr;
                     i++; ptr++;
                  }
                  if (i == MAX_DIR_ALIAS_LENGTH)
                  {
                     while ((*ptr != '\n') && (*ptr != '\0'))
                     {
                        ptr++;
                     }
                  }
                  if (i > 0)
                  {
                     disabled_dirs[no_of_disabled_dirs][i] = '\0';
                     no_of_disabled_dirs++;
                  }
               }
            } while (*ptr == '\n');
         }

         free(buffer);
      }
#ifdef HAVE_STATX
      disabled_dir_mtime = stat_buf.stx_mtime.tv_sec;
#else
      disabled_dir_mtime = stat_buf.st_mtime;
#endif

      return(YES);
   }

   return(NO);
}
