/*
 *  amg_zombie_check.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2022 Deutscher Wetterdienst (DWD),
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
 **   amg_zombie_check - checks if any process terminated that was
 **                      started by the AMG
 **
 ** SYNOPSIS
 **   int amg_zombie_check(pid_t *proc_id, int option)
 **
 ** DESCRIPTION
 **   The function amg_zombie_check() checks if any process is finished
 **   (zombie), if this is the case it is killed with waitpid().
 **
 ** RETURN VALUES
 **   Returns YES when the status of a process has changed (except
 **   when it has been put to sleep). Otherwise NO is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.08.1995 H.Kiehl Created
 **   27.03.1998 H.Kiehl Put this function into a separate file.
 **   08.04.2002 H.Kiehl Added saving of old core files.
 **
 */
DESCR__E_M3

#include <stdio.h>                    /* rename()                        */
#include <string.h>                   /* strerror()                      */
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>                   /* Definition of AT_* constants    */
#endif
#include <sys/stat.h>
#include <sys/wait.h>                 /* waitpid()                       */
#include <time.h>
#include <errno.h>
#include "amgdefs.h"

#define NO_OF_SAVED_CORE_FILES 5

#ifdef NO_OF_SAVED_CORE_FILES
extern char *p_work_dir;
#endif


/*########################## amg_zombie_check() #########################*/
int
amg_zombie_check(pid_t *proc_id, int option)
{
   int status,
       table_changed = NO;

   /* Is process a zombie? */
   if (waitpid(*proc_id, &status, option) > 0)
   {
      int exit_status = 0;

      if (WIFEXITED(status))
      {
         exit_status = WEXITSTATUS(status);
         if (exit_status == 0)
         {
            exit_status = NOT_RUNNING;
         }
         else
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("Process returned %d"), exit_status);
            exit_status = DIED;
         }
      }
      else if (WIFSIGNALED(status))
           {
              /* Abnormal termination. */
#ifdef NO_OF_SAVED_CORE_FILES
              static int no_of_saved_cores = 0;

              if (no_of_saved_cores < NO_OF_SAVED_CORE_FILES)
              {
                 char         core_file[MAX_PATH_LENGTH];
# ifdef HAVE_STATX
                 struct statx stat_buf;
# else
                 struct stat  stat_buf;
# endif

                 (void)snprintf(core_file, MAX_PATH_LENGTH, "%s/core", p_work_dir);
# ifdef HAVE_STATX
                 if (statx(0, core_file, AT_STATX_SYNC_AS_STAT,
                           0, &stat_buf) != -1)
# else
                 if (stat(core_file, &stat_buf) != -1)
# endif
                 {
                    char new_core_file[MAX_PATH_LENGTH + 43];

                    (void)snprintf(new_core_file, MAX_PATH_LENGTH + 43,
# if SIZEOF_TIME_T == 4
                                   "%s.%s.%ld.%d",
# else
                                   "%s.%s.%lld.%d",
# endif
                                   core_file, DC_PROC_NAME,
                                   (pri_time_t)time(NULL), no_of_saved_cores);
                    if (rename(core_file, new_core_file) == -1)
                    {
                       system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                  _("Failed to rename() `%s' to `%s' : %s"),
                                  core_file, new_core_file, strerror(errno));
                    }
                    else
                    {
                       no_of_saved_cores++;
                    }
                 }
              }
#endif /* NO_OF_SAVED_CORE_FILES */
              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                         _("Abnormal termination caused by signal %d"),
                         WTERMSIG(status));
              exit_status = DIED;
           }
      else if (WIFSTOPPED(status))
           {
              /* Child stopped. */
              exit_status = STOPPED;
           }

      /* Update table. */
      if (exit_status < STOPPED)
      {
         table_changed = YES;
         *proc_id = exit_status;
      }
   }

   return(table_changed);
}
