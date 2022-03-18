/*
 *  remove_old_ls_data_files.c - Part of AFD, an automatic file distribution
 *                               program.
 *  Copyright (c) 2006 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   remove_old_ls_data_files - removes old ls data files
 **
 ** SYNOPSIS
 **   void remove_old_ls_data_files(void)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   23.08.2006 H.Kiehl Created
 **   26.03.2021 H.Kiehl We also need to check the ls_data_alias variable.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>           /* strerror()                              */
#include <sys/types.h>
#include <dirent.h>           /* opendir(), readdir(), closedir()        */
#include <unistd.h>           /* unlink()                                */
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int                        no_of_local_dirs;
extern char                       *p_work_dir;
extern struct directory_entry     *de;
extern struct fileretrieve_status *fra;


/*###################### remove_old_ls_data_files() #####################*/
void
remove_old_ls_data_files(void)
{
   char listfile[MAX_PATH_LENGTH],
        *ptr;
   DIR  *dp;

   ptr = listfile + snprintf(listfile, MAX_PATH_LENGTH,
                             "%s%s%s%s/", p_work_dir, AFD_FILE_DIR,
                             INCOMING_DIR, LS_DATA_DIR);
   if ((dp = opendir(listfile)) == NULL)
   {
      system_log((errno == ENOENT) ? DEBUG_SIGN : WARN_SIGN, __FILE__, __LINE__,
                 "Failed to opendir() `%s' to remove old ls data files : %s",
                 listfile, strerror(errno));
   }
   else
   {
      int           gotcha,
                    i,
                    removed = 0;
      struct dirent *p_dir;

      errno = 0;
      while ((p_dir = readdir(dp)) != NULL)
      {
#ifdef LINUX
         if ((p_dir->d_type == DT_REG) && (p_dir->d_name[0] != '.'))
#else
         if (p_dir->d_name[0] != '.')
#endif
         {
            gotcha = NO;
            for (i = 0; i < no_of_local_dirs; i++)
            {
               if ((CHECK_STRCMP(fra[de[i].fra_pos].dir_alias, p_dir->d_name) == 0) ||
                   (CHECK_STRCMP(fra[de[i].fra_pos].ls_data_alias, p_dir->d_name) == 0))
               {
                  gotcha = YES;
                  break;
               }
            }
            if (gotcha == NO)
            {
               (void)strcpy(ptr, p_dir->d_name);
               if (unlink(listfile) == -1)
               {
                  char *sign;

                  if (errno == ENOENT)
                  {
                     sign = DEBUG_SIGN;
                  }
                  else
                  {
                     sign = WARN_SIGN;
                  }
                  system_log(sign, __FILE__, __LINE__,
                             "Failed to unlink() file %s : %s",
                             listfile, strerror(errno));
               }
               else
               {
#ifdef VERBOSE_OUTPUT
                  system_log(DEBUG_SIGN, NULL, 0,
                             "Removed old ls data for %s", p_dir->d_name);
#endif
                  removed++;
               }
            }
         }
         errno = 0;
      }
      if (errno)
      {
         *ptr = '\0';
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Could not readdir() `%s' : %s", listfile, strerror(errno));
      }
      if (closedir(dp) == -1)
      {
         *ptr = '\0';
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Could not closedir() `%s' : %s",
                    listfile, strerror(errno));
      }
      if (removed > 0)
      {
         system_log(DEBUG_SIGN, NULL, 0,
                    "Removed %d old ls data files.", removed);
      }
   }
   return;
}
