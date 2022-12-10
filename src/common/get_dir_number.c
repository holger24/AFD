/*
 *  get_dir_number.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_dir_number - gets a directory number
 **
 ** SYNOPSIS
 **   int get_dir_number(char *directory, unsigned int id, long *dirs_left)
 **
 ** DESCRIPTION
 **   The function get_dir_number() looks in 'directory' for a
 **   free directory. If it does not find one it tries to create
 **   a new one. It starts from zero to the maximum number
 **   of links that may be created in a directory.
 **
 ** RETURN VALUES
 **   Returns a directory number where files may be stored. Otherwise
 **   INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.03.2005 H.Kiehl Created
 **   04.02.2010 H.Kiehl When we create a directory and it already
 **                      exists, do not return INCORRECT.
 **
 */
DESCR__E_M3

#include <stdio.h>                    /* snprintf()                      */
#include <string.h>                   /* strerror()                      */
#include <unistd.h>                   /* mkdir(), pathconf()             */
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>                   /* Definition of AT_* constants */
#endif
#include <sys/stat.h>                 /* stat()                          */
#include <errno.h>

/* Local variables. */
static long link_max = 0L;


/*########################### get_dir_number() ##########################*/
int
get_dir_number(char *directory, unsigned int id, long *dirs_left)
{
   int          i;
   size_t       length;
   char         fulldir[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   if (link_max == 0L)
   {
      if ((link_max = pathconf(directory, _PC_LINK_MAX)) == -1)
      {
#ifdef REDUCED_LINK_MAX
         link_max = REDUCED_LINK_MAX;
#else
         link_max = _POSIX_LINK_MAX;
#endif
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("pathconf() error for _PC_LINK_MAX in %s : %s"),
                    directory, strerror(errno));
      }
   }
   length = strlen(directory);
   (void)memcpy(fulldir, directory, length);
   if (fulldir[length - 1] != '/')
   {
      fulldir[length] = '/';
      length++;
   }

   for (i = 0; i < link_max; i++)
   {
      (void)snprintf(&fulldir[length], MAX_PATH_LENGTH - length,
                     "%x/%x", id, i);
#ifdef HAVE_STATX
      if (statx(0, fulldir, AT_STATX_SYNC_AS_STAT,
                STATX_NLINK, &stat_buf) == -1)
#else
      if (stat(fulldir, &stat_buf) == -1)
#endif
      {
         if (errno == ENOENT)
         {
#ifdef HAVE_STATX
            if (statx(0, directory, AT_STATX_SYNC_AS_STAT, 0, &stat_buf) == -1)
#else
            if (stat(directory, &stat_buf) == -1)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                          _("Failed to statx() `%s' : %s"),
#else
                          _("Failed to stat() `%s' : %s"),
#endif
                          directory, strerror(errno));
               return(INCORRECT);
            }

            (void)snprintf(&fulldir[length], MAX_PATH_LENGTH - length,
                           "%x", id);
            errno = 0;
#ifdef HAVE_STATX
            if ((statx(0, fulldir, AT_STATX_SYNC_AS_STAT,
                       0, &stat_buf) == -1) && (errno != ENOENT))
#else
            if ((stat(fulldir, &stat_buf) == -1) && (errno != ENOENT))
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                          _("Failed to statx() `%s' : %s"),
#else
                          _("Failed to stat() `%s' : %s"),
#endif
                          fulldir, strerror(errno));
               return(INCORRECT);
            }
            if (errno == ENOENT)
            {
               if (mkdir(fulldir, DIR_MODE) < 0)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Failed to mkdir() `%s' : %s"),
                             fulldir, strerror(errno));
                  return(INCORRECT);
               }
               else
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             _("Hmm, created directory `%s'"), fulldir);
               }
            }
            (void)snprintf(&fulldir[length], MAX_PATH_LENGTH - length,
                           "%x/%x", id, i);
            if (mkdir(fulldir, DIR_MODE) < 0)
            {
               /*
                * For now lets not return INCORRECT when a directory
                * does already exist. It looks as if this can happen
                * when we have a time job. Process dir_check does not
                * behave well when we return INCORRECT here.
                */
               if (errno == EEXIST)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             _("Failed to mkdir() `%s' : %s"),
                             fulldir, strerror(errno));
               }
               else
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Failed to mkdir() `%s' : %s"),
                             fulldir, strerror(errno));
                  return(INCORRECT);
               }
            }
            if (dirs_left != NULL)
            {
               if (link_max > 10000L)
               {
                  *dirs_left = 10000L;
               }
               else
               {
                  *dirs_left = link_max;
               }
            }
            return(i);
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                       _("Failed to statx() `%s' : %s"),
#else
                       _("Failed to stat() `%s' : %s"),
#endif
                       fulldir, strerror(errno));
            return(INCORRECT);
         }
      }
#ifdef HAVE_STATX
      else if (stat_buf.stx_nlink < link_max)
#else
      else if (stat_buf.st_nlink < link_max)
#endif
           {
              if (dirs_left != NULL)
              {
#ifdef HAVE_STATX
                 *dirs_left = link_max - stat_buf.stx_nlink;
#else
                 *dirs_left = link_max - stat_buf.st_nlink;
#endif
                 if (*dirs_left > 10000L)
                 {
                    *dirs_left = 10000L;
                 }
              }
              return(i);
           }
   }

   system_log(ERROR_SIGN, __FILE__, __LINE__,
              _("Directory `%s/%x' is full (%ld). Unable to create new jobs for it."),
              directory, id, link_max);
   return(INCORRECT);
}
