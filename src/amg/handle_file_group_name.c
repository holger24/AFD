/*
 *  handle_file_group_name.c - Part of AFD, an automatic file distribution
 *                             program.
 *  Copyright (c) 2015 - 2018 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_file_group       - get all file filters for a given group
 **
 ** SYNOPSIS
 **   void get_file_group(char             *group_name,
 **                       int              file_group_type,
 **                       struct dir_group *dir,
 **                       int              *total_length)
 **
 ** DESCRIPTION
 **   This set of functions handles group names for files
 **   in DIR_CONFIG file. The group name is resolved via the file
 **   $AFD_WORK_DIR/etc/group.list.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   21.03.2015 H.Kiehl Created
 **   29.05.2018 H.Kiehl Implement storing of file group type.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* fprintf(), sprintf()               */
#include <string.h>                /* strcmp(), strcpy(), strerror()     */
#include <stdlib.h>                /* exit()                             */
#include <sys/types.h>
#include <sys/stat.h>              /* stat(), S_ISREG()                  */
#include <dirent.h>                /* opendir(), closedir(), readdir(),  */
                                   /* DIR, struct dirent                 */
#include <errno.h>
#include "amgdefs.h"


/* External global variables. */
extern char *p_work_dir;


/*########################## get_file_group() ###########################*/
void
get_file_group(char             *group_name,
               int              file_group_type, 
               struct dir_group *dir,
               int              *total_length)
{
   off_t file_size;
   char  *file_group_buffer = NULL,
         group_file[MAX_PATH_LENGTH];

   if (file_group_type == YES)
   {
      (void)snprintf(group_file, MAX_PATH_LENGTH, "%s%s%s%s/%s",
                     p_work_dir, ETC_DIR, GROUP_NAME_DIR,
                     FILE_GROUP_NAME, group_name);
   }
   else
   {
      (void)snprintf(group_file, MAX_PATH_LENGTH, "%s%s%s",
                     p_work_dir, ETC_DIR, GROUP_FILE);
   }
   if ((file_size = read_file_no_cr(group_file, &file_group_buffer, YES,
                                    __FILE__, __LINE__)) < 1)
   {
      if (file_size == INCORRECT)
      {
         /* read_file_no_cr() already printed the error message. */;
      }
      else
      {
         if (file_group_type == YES)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                       "Group file %s is empty (%ld).",
#else
                       "Group file %s is empty (%lld).",
#endif
                       group_file, (pri_off_t)file_size);
         }
         else
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                       "No elements found in group [%s] in file %s (%ld).",
#else
                       "No elements found in group [%s] in file %s (%lld).",
#endif
                       group_name, group_file, (pri_off_t)file_size);
         }
      }
      return;
   }

   if (file_group_buffer != NULL)
   {
      int  length;
      char *ptr;

      if (file_group_type == YES)
      {
         ptr = file_group_buffer;

         length = 0;
         do
         {
            ptr++;
            if (*ptr == '#')
            {
               while ((*ptr != '\n') && (*ptr != '\0'))
               {
                  ptr++;
               }
               if (length > 0)
               {
                  dir->file[dir->fgc].files[*total_length + length] = '\0';
                  *total_length += length + 1;
                  dir->file[dir->fgc].fc++;
                  length = 0;
               }
            }
            else if ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    /* Ignore spaces! */;
                 }
                 else
                 {
                    if ((*ptr == '\n') || (*ptr == '\0'))
                    {
                       dir->file[dir->fgc].files[*total_length + length] = '\0';
                       *total_length += length + 1;
                       dir->file[dir->fgc].fc++;
                       length = 0;
                    }
                    else
                    {
                       dir->file[dir->fgc].files[*total_length + length] = *ptr;
                       length++;
                       if ((*total_length + length + 1) >= dir->file[dir->fgc].fbl)
                       {
                          dir->file[dir->fgc].fbl += FILE_MASK_STEP_SIZE;
                          if ((dir->file[dir->fgc].files = realloc(dir->file[dir->fgc].files,
                                                                   dir->file[dir->fgc].fbl)) == NULL)
                          {
                             system_log(FATAL_SIGN, __FILE__, __LINE__,
                                        "Could not realloc() memory : %s",
                                        strerror(errno));
                             exit(INCORRECT);
                          }
                       }
                    }
                 }

            if ((*ptr == '\n') &&
                ((*(ptr + 1) == '\n') || (*(ptr + 1) == '\0') ||
                 (*(ptr + 1) == '[')))
            {
               break;
            }
         } while (*ptr != '\0');
      }
      else
      {
         char group_id[1 + MAX_GROUPNAME_LENGTH + 2 + 1];

         length = snprintf(group_id, 1 + MAX_GROUPNAME_LENGTH + 2 + 1,
                           "\n[%s]", group_name);
         if ((ptr = lposi(file_group_buffer, group_id, length)) == NULL)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to locate group [%s] in group file %s",
                       group_name, group_file);
            free(file_group_buffer);

            return;
         }

         ptr--;
         while ((*ptr != '\n') && (*ptr != '\0'))
         {
            ptr++;
         }
         if (*ptr == '\n')
         {
            length = 0;
            do
            {
               ptr++;
               if (*ptr == '#')
               {
                  while ((*ptr != '\n') && (*ptr != '\0'))
                  {
                     ptr++;
                  }
                  if (length > 0)
                  {
                     dir->file[dir->fgc].files[*total_length + length] = '\0';
                     *total_length += length + 1;
                     dir->file[dir->fgc].fc++;
                     length = 0;
                  }
               }
               else if ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       /* Ignore spaces! */;
                    }
                    else
                    {
                       if ((*ptr == '\n') || (*ptr == '\0'))
                       {
                          dir->file[dir->fgc].files[*total_length + length] = '\0';
                          *total_length += length + 1;
                          dir->file[dir->fgc].fc++;
                          length = 0;
                       }
                       else
                       {
                          dir->file[dir->fgc].files[*total_length + length] = *ptr;
                          length++;
                          if ((*total_length + length + 1) >= dir->file[dir->fgc].fbl)
                          {
                             dir->file[dir->fgc].fbl += FILE_MASK_STEP_SIZE;
                             if ((dir->file[dir->fgc].files = realloc(dir->file[dir->fgc].files,
                                                                      dir->file[dir->fgc].fbl)) == NULL)
                             {
                                system_log(FATAL_SIGN, __FILE__, __LINE__,
                                           "Could not realloc() memory : %s",
                                           strerror(errno));
                                exit(INCORRECT);
                             }
                          }
                       }
                    }

               if ((*ptr == '\n') &&
                   ((*(ptr + 1) == '\n') || (*(ptr + 1) == '\0') ||
                    (*(ptr + 1) == '[')))
               {
                  break;
               }
            } while (*ptr != '\0');
         }
         else
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "No group elements found for group %s.", group_name);
         }
      }

      free(file_group_buffer);
   }

   return;
}
