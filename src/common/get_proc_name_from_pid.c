/*
 *  get_proc_name_from_pid.c - Part of AFD, an automatic file distribution
 *                            program.
 *  Copyright (c) 2018 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_proc_name_from_pid - gets the proccess name from pid
 **
 ** SYNOPSIS
 **   void get_proc_name_from_pid(pid_t pid, char *proc_name)
 **
 ** DESCRIPTION
 **   This function determines the process name from the pid.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   26.11.2018 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>             /* fopen(), fclose(), snprintf(), fread() */


/*######################## get_proc_name_from_pid() #####################*/
void
get_proc_name_from_pid(pid_t pid, char *proc_name)
{
#if SIZEOF_PID_T == 4
   char proc_file[6 + MAX_LONG_LENGTH + 8 + 1];
#else
   char proc_file[6 + MAX_LONG_LONG_LENGTH + 8 + 1];
#endif
   FILE *fp;

   (void)snprintf(proc_file,
#if SIZEOF_PID_T == 4
                  6 + MAX_LONG_LENGTH + 8 + 1,
                  "/proc/%d/cmdline",
#else
                  6 + MAX_LONG_LONG_LENGTH + 8 + 1,
                  "/proc/%lld/cmdline",
#endif
                  (pri_pid_t)pid);
   if ((fp = fopen(proc_file, "r")) != NULL)
   {
      size_t size;

      size = fread(proc_name, sizeof(char), sizeof(proc_file), fp);
      if (size > 0)
      {
         if (proc_name[size - 1] == '\0')
         {
            proc_name[size - 1] = '\0';
         }
      }
      else
      {
         proc_name[0] = '\0';
      }

      (void)fclose(fp);
   }
   else
   {
      proc_name[0] = '\0';
   }

   return;
}
