/*
 *  get_max_log_values.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 - 2020 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_max_log_values - gets the maximum number of logfiles and size
 **                        to keep from AFD_CONFIG file
 **
 ** SYNOPSIS
 **   void get_max_log_values(int   *max_log_file_number,
 **                           char  *max_number_def,
 **                           int   default_number,
 **                           off_t *max_log_file_size,
 **                           char  *max_size_def,
 **                           off_t default_size,
 **                           char  *main_config_file)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   Returns the number of logfiles to keep.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.01.2002 H.Kiehl Created
 **   16.01.2012 H.Kiehl Also get the max size.
 **
 */
DESCR__E_M3

#include <stdio.h>                    /* snprintf()                      */
#include <stdlib.h>                   /* free(), strtoul(), atoi()       */
#include <unistd.h>                   /* F_OK                            */

/* External global variables. */
extern char *p_work_dir;


/*######################## get_max_log_values() #########################*/
void
get_max_log_values(int   *max_log_file_number,
                   char  *max_number_def,
                   int   default_number,
                   off_t *max_log_file_size,
                   char  *max_size_def,
                   off_t default_size,
                   char  *main_config_file)
{
   char *buffer,
        config_file[MAX_PATH_LENGTH];

   (void)snprintf(config_file, MAX_PATH_LENGTH, "%s%s%s",
                  p_work_dir, ETC_DIR, main_config_file);
   if ((eaccess(config_file, F_OK) == 0) &&
       (read_file_no_cr(config_file, &buffer, YES, __FILE__, __LINE__) != INCORRECT))
   {
      char value[MAX_OFF_T_LENGTH];

      if (get_definition(buffer, max_number_def,
                         value, MAX_INT_LENGTH) != NULL)
      {
         *max_log_file_number = atoi(value);
         if ((*max_log_file_number < 1) || (*max_log_file_number > 599))
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("Incorrect value (%d, must be more then 1 but less then 600) set in AFD_CONFIG for %s. Setting to default %d."),
                       *max_log_file_number, max_number_def, default_number);
            *max_log_file_number = default_number;
         }
      }
      if ((max_log_file_size != NULL) && (max_size_def != NULL))
      {
         if (get_definition(buffer, max_size_def,
                            value, MAX_OFF_T_LENGTH) != NULL)
         {
#ifdef HAVE_STRTOULL
# if SIZEOF_OFF_T == 4
            if ((*max_log_file_size = (off_t)strtoul(value, NULL, 10)) != ULONG_MAX)
# else
            if ((*max_log_file_size = (off_t)strtoull(value, NULL, 10)) != ULLONG_MAX)
# endif
#else
            if ((*max_log_file_size = (off_t)strtoul(value, NULL, 10)) != ULONG_MAX)
#endif
            {
               if (*max_log_file_size < 1024)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             _("Incorrect value (%d, must be more then 1024) set in AFD_CONFIG for %s. Setting to default %d."),
                             *max_log_file_number, max_number_def, default_number);
                  *max_log_file_size = default_size;
               }
            }
            else
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                          _("Value to large for %s, setting default size %ld"),
#else
                          _("Value to large for %s, setting default size %lld"),
#endif
                          max_size_def, (pri_off_t)default_size);
               *max_log_file_size = default_size;
            }
         }
      }
      free(buffer);
   }
   return;
}
