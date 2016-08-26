/*
 *  rename_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2016 Deutscher Wetterdienst (DWD),
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
 **   rename_files - renames files from one directory to another
 **                  unique directory
 **
 ** SYNOPSIS
 **   int rename_files(char                   *src_file_path,
 **                    char                   *dest_file_path,
 **                    int                    dest_file_path_length,
 **                    int                    files_moved,
 **                    struct instant_db      *p_db,
 **                    time_t                 current_time,
 **                    int                    unique_number,
 **                    unsigned int           *split_job_counter,
 **                    char                   *unique_name,
 **                    off_t                  *file_size_renamed)
 **
 ** DESCRIPTION
 **   The function rename_files() renames MAX_FILES_TO_PROCESS from the
 **   directory 'src_file_path' to the AFD file directory. To keep files
 **   of each job unique it has to create a unique directory name. This
 **   name is later also used to create the message name.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to rename the user files. Otherwise
 **   it returns the number of files that were renamed, the total size of
 **   all files 'file_size_renamed' and the unique name 'unique_name'
 **   that was generated.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   05.12.1996 H.Kiehl Created
 **   18.10.1997 H.Kiehl Introduced inverse filtering.
 **   16.04.2001 H.Kiehl Remove file_offset and no_of_files.
 **   24.09.2004 H.Kiehl Added split job counter.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* rename()                           */
#include <string.h>                /* strcpy(), strlen(), strerror()     */
#include <stdlib.h>                /* exit()                             */
#include <sys/types.h>
#include <unistd.h>                /* link(), sleep()                    */
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
#ifndef _WITH_PTHREAD
extern off_t           *file_size_pool;
extern char            *file_name_buffer,
                       **file_name_pool;
#endif


/*########################## rename_files() #############################*/
int
rename_files(char                   *src_file_path,
             char                   *dest_file_path,
             int                    dest_file_path_length,
             int                    files_moved,
             struct instant_db      *p_db,
             time_t                 current_time,
             int                    unique_number,
             unsigned int           *split_job_counter,
             char                   *unique_name, /* Storage to return unique name. */
#if defined (_MAINTAINER_LOG) && defined (SHOW_FILE_MOVING)
             char                   *caller,
             int                    line,
#endif
             off_t                  *file_size_renamed)
{
   int          files_renamed = 0;
#ifndef _WITH_PTHREAD
   register int i,
                j;
   char         *p_file_name,
                *p_src = NULL,
                *p_dest = NULL,
                *p_dest_end = NULL;

   *file_size_renamed = 0;
   p_file_name = file_name_buffer;

   for (i = 0; i < MAX_FILES_TO_PROCESS; i++)
   {
      /* Only create a unique name and the corresponding */
      /* directory when we have found a file that is to  */
      /* distributed.                                    */
      if (p_src == NULL)
      {
         /* Create a new message name and directory. */
         if (create_name(dest_file_path, dest_file_path_length, p_db->priority,
                         current_time, p_db->job_id, split_job_counter,
                         &unique_number, unique_name, MAX_FILENAME_LENGTH - 1,
                         -1) < 0)
         {
            if (errno == ENOSPC)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "DISK FULL!!! Will retry in %d second interval.",
                          DISK_FULL_RESCAN_TIME);

               while (errno == ENOSPC)
               {
                  (void)sleep(DISK_FULL_RESCAN_TIME);
                  errno = 0;
                  if (create_name(dest_file_path, dest_file_path_length,
                                  p_db->priority, current_time, p_db->job_id,
                                  split_job_counter, &unique_number,
                                  unique_name, MAX_FILENAME_LENGTH - 1, -1) < 0)
                  {
                     if (errno != ENOSPC)
                     {
                        system_log(FATAL_SIGN, __FILE__, __LINE__,
                                   "Failed to create a unique name : %s",
                                   strerror(errno));
                        exit(INCORRECT);
                     }
                  }
               }

               system_log(INFO_SIGN, __FILE__, __LINE__,
                          "Continuing after disk was full.");
            }
            else
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to create a unique name : %s",
                          strerror(errno));
               exit(INCORRECT);
            }
         }
         p_src = src_file_path + strlen(src_file_path);
         p_dest_end = dest_file_path + strlen(dest_file_path);
         (void)strcpy(p_dest_end, (unique_name - 1));
         p_dest = p_dest_end + strlen((unique_name - 1));
         *(p_dest++) = '/';
         *p_dest = '\0';
      }

      (void)strcpy(p_src, p_file_name);
      (void)strcpy(p_dest, p_file_name);

      /* Rename the file. */
      if (rename(src_file_path, dest_file_path) < 0)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to rename() file %s to %s : %s",
                    src_file_path, dest_file_path, strerror(errno));
      }
      else
      {
#if defined (_MAINTAINER_LOG) && defined (SHOW_FILE_MOVING)
         maintainer_log(DEBUG_SIGN, NULL, 0,
                        "rename_files() [%s %d]: `%s' -> `%s'",
                        caller, line, src_file_path, dest_file_path);
#endif
         files_renamed++;
         for (j = 0; j < files_moved; j++)
         {
            if (CHECK_STRCMP(p_file_name, file_name_pool[j]) == 0)
            {
               *file_size_renamed += file_size_pool[j];
               break;
            }
         }
      }
      p_file_name += MAX_FILENAME_LENGTH;
   } /* for (i = 0; i < MAX_FILES_TO_PROCESS; i++) */

   /* Keep source and destination directories clean so */
   /* that other functions can work with them.         */
   if (p_dest_end != NULL)
   {
      *p_dest_end = '\0';
   }
   if (p_src != NULL)
   {
      *p_src = '\0';
   }
#endif /* !_WITH_PTHREAD */

   return(files_renamed);
}
