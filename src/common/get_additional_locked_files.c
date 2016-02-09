/*
 *  get_additional_locked_files.c - Part of AFD, an automatic file
 *                                  distribution program.
 *  Copyright (c) 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_additional_locked_files - gets the list of additional locked
 **                                 files from AFD_CONFIG
 **
 ** SYNOPSIS
 **   void get_additional_locked_files(int *alfc, int *alfbl, char **alfiles)
 **
 ** DESCRIPTION
 **   Extracts a list of additional locked files from the AFD_CONFIG file.
 **
 ** RETURN VALUES
 **   Returns a the number of filters in alfc and if alfiles is not
 **   NULL, return a list of filters.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   26.04.2014 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <unistd.h>        /* eaccess()                                  */
#include <stdlib.h>        /* malloc(), free()                           */
#include <errno.h>

/* External global variables. */
extern char *p_work_dir;


/*##################### get_additional_locked_files() ###################*/
void
get_additional_locked_files(int *alfc, int *alfbl, char **alfiles)
{
   char *buffer = NULL,
        config_name[MAX_PATH_LENGTH];

   (void)snprintf(config_name, MAX_PATH_LENGTH, "%s%s%s",
                  p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   if ((eaccess(config_name, F_OK) == 0) &&
       (read_file_no_cr(config_name, &buffer, __FILE__, __LINE__) != INCORRECT))
   {
      char alf_list[MAX_ADD_LOCKED_FILES_LENGTH];

      if (get_definition(buffer, ADDITIONAL_LOCKED_FILES_DEF,
                         alf_list, MAX_ADD_LOCKED_FILES_LENGTH) != NULL)
      {
         int  length = 0;
         char *ptr = alf_list;

         *alfc = 0;
         while (*ptr != '\0')
         {
            while ((*ptr == ' ') && (*ptr == '\t'))
            {
               ptr++;
            }
            if (*ptr != '!')
            {
               length++;
            }
            while ((*ptr != '|') && (*ptr != '\0'))
            {
               ptr++; length++;
            }
            if (*ptr == '|')
            {
               while (*ptr == '|')
               {
                  ptr++;
               }
               (*alfc)++;
               length++;
            }
         }
         if (length > 0)
         {
            (*alfc)++;
            length++;
         }

         if (alfiles != NULL)
         {
            if ((*alfiles = malloc(length)) == NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("malloc() error : %s"), strerror(errno));
               if (alfbl != NULL)
               {
                  *alfbl = 0;
               }
            }
            else
            {
               char *p_file = *alfiles;

               ptr = alf_list;
               while (*ptr != '\0')
               {
                  while ((*ptr == ' ') && (*ptr == '\t'))
                  {
                     ptr++;
                  }
                  if (*ptr != '!')
                  {
                     *p_file = '!';
                     p_file++;
                  }
                  while ((*ptr != '|') && (*ptr != '\0'))
                  {
                     *p_file = *ptr;
                     ptr++; p_file++;
                  }
                  if (*ptr == '|')
                  {
                     while (*ptr == '|')
                     {
                        ptr++;
                     }
                     *p_file = '\0';
                     p_file++;
                  }
               }

               if (alfbl != NULL)
               {
                  *alfbl = length;
               }
            }
         }
      }
      else
      {
         *alfc = 0;
         if (alfbl != NULL)
         {
            *alfbl = 0;
         }
      }
   }
   else
   {
      *alfc = 0;
      if (alfbl != NULL)
      {
         *alfbl = 0;
      }
   }
   free(buffer);

   return;
}
