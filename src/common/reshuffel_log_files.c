/*
 *  reshuffel_log_files.c - Part of AFD, an automatic file distribution
 *                          program.
 *  Copyright (c) 1997 - 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   reshuffel_log_files -
 **
 ** SYNOPSIS
 **   void reshuffel_log_files(int  log_number,
 **                            char *log_file,
 **                            char *p_end,
 **                            int  shift,
 **                            int  shift_offset)
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
 **   11.01.1997 H.Kiehl Created
 **   22.03.2007 H.Kiehl Added shift factor.
 **
 */
DESCR__E_M3

#include <stdio.h>             /* snprintf()                             */
#include <string.h>            /* strcpy(), strerror()                   */
#include <unistd.h>            /* rename()                               */
#include <errno.h>


/*######################## reshuffel_log_files() ########################*/
void
reshuffel_log_files(int  log_number,
                    char *log_file,
                    char *p_end,
                    int  shift,
                    int  shift_offset)
{
   register int i;
   char         dst[MAX_PATH_LENGTH];

   do
   {
      for (i = log_number; i > shift_offset; i--)
      {
         (void)snprintf(p_end, MAX_PATH_LENGTH - (p_end - log_file), "%d", i);
         (void)my_strncpy(dst, log_file, MAX_PATH_LENGTH);
         (void)snprintf(p_end, MAX_PATH_LENGTH - (p_end - log_file),
                        "%d", i - 1);

         if (rename(log_file, dst) < 0)
         {
            if (errno == ENOSPC)
            {
               int error_flag = NO;

               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("DISK FULL!!! Will retry in %d second interval."),
                          DISK_FULL_RESCAN_TIME);

               while (errno == ENOSPC)
               {
                  (void)sleep(DISK_FULL_RESCAN_TIME);
                  errno = 0;
                  if (rename(log_file, dst) < 0)
                  {
                     if (errno != ENOSPC)
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   _("Failed to rename() `%s' to `%s' : %s"),
                                   log_file, dst, strerror(errno));
                        error_flag = YES;
                        break;
                     }
                  }
               }
               if (error_flag == NO)
               {
                  system_log(INFO_SIGN, __FILE__, __LINE__,
                             _("Continuing after disk was full."));
               }
            }
            else if (errno != ENOENT)
                 {
                    system_log(WARN_SIGN, __FILE__, __LINE__,
                               _("Failed to rename() `%s' to `%s' : %s"),
                               log_file, dst, strerror(errno));
                 }
         }
      }
      shift--;
   } while (shift > 0);

   return;
}
