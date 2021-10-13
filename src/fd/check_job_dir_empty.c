/*
 *  check_job_dir_empty.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2021 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_job_dir_empty - checks if files are still in job diretory
 **
 ** SYNOPSIS
 **   int check_job_dir_empty(char *unique_name, char *file_path)
 **
 ** DESCRIPTION
 **   During a burst it can happen that we miss a SIGUSR1 signal
 **   and if sf_xxx exit we have a job without a message in the
 **   system.
 **
 ** RETURN VALUES
 **   Returns YES if the job directory is empty or we could not
 **   open the directory. If it is not empty NO is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   26.09.2021 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                     /* NULL                           */
#include <string.h>                    /* strcpy(), strcat(), strerror() */
#include <sys/types.h>
#include <dirent.h>                    /* opendir(), closedir(), DIR,    */
                                       /* readdir(), struct dirent       */
#include <errno.h>
#include "fddefs.h"

/* Global variables. */
extern char       *p_work_dir;
extern struct job db;


/*####################### check_job_dir_empty() #########################*/
int
check_job_dir_empty(char *unique_name, char *file_path)
{
   int dir_empty = YES;
   DIR *dp;

   /*
    * Create directory name in which we can find the files for this job.
    */
   (void)strcpy(file_path, p_work_dir);
   (void)strcat(file_path, AFD_FILE_DIR);
   (void)strcat(file_path, OUTGOING_DIR);
   (void)strcat(file_path, "/");
   (void)strcat(file_path, unique_name);

   if ((dp = opendir(file_path)) != NULL)
   {
      struct dirent *p_dir;

      /*
       * Now let's see if there are any files left.
       */
      errno = 0;
      while ((p_dir = readdir(dp)) != NULL)
      {
         if (((p_dir->d_name[0] == '.') && (p_dir->d_name[1] == '\0')) ||
             ((p_dir->d_name[0] == '.') && (p_dir->d_name[1] == '.') &&
             (p_dir->d_name[2] == '\0')))
         {
            continue;
         }
         dir_empty = NO;
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Hmm, %s still has data. #%x", unique_name, db.id.job);
         errno = 0;
      } /* while ((p_dir = readdir(dp)) != NULL) */

      if (errno)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Could not readdir() `%s' : %s #%x",
                    file_path, strerror(errno), db.id.job);
      }

      if (closedir(dp) < 0)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Could not closedir() `%s' : %s #%x",
                    file_path, strerror(errno), db.id.job);
      }
   }
#ifdef _MAINTAINER_LOG
   maintainer_log(WARN_SIGN, __FILE__, __LINE__,
                  "check_job_dir_empty() called for %s (dir_empty=%d) #%x",
                  unique_name, dir_empty, db.id.job);
#endif

   return(dir_empty);
}
