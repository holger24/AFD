/*
 *  remove_nnn_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2013 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   remove_nnn_files - removes nnn counter files
 **
 ** SYNOPSIS
 **   void remove_nnn_files(unsigned int host_id)
 **
 ** DESCRIPTION
 **   Removes any possible NNN counter files that hve been created
 **   via the assemble() and convert() functions.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   19.08.2013 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>             /* strcpy(), strlen()                    */
#include <unistd.h>             /* rmdir(), unlink()                     */
#include <errno.h>

/* External global variables. */
extern char *p_work_dir;


/*########################## remove_nnn_files() #########################*/
void
remove_nnn_files(unsigned int host_id)
{
   char fullname[MAX_PATH_LENGTH];

   (void)snprintf(fullname, MAX_PATH_LENGTH, "%s%s%s.%x",
                  p_work_dir, FIFO_DIR, NNN_FILE, host_id);
   if ((unlink(fullname) == -1) && (errno != ENOENT))
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to unlink() `%s' : %s",
                 fullname, strerror(errno));
   }
   (void)snprintf(fullname, MAX_PATH_LENGTH, "%s%s%s.%x",
                  p_work_dir, FIFO_DIR, NNN_ASSEMBLE_FILE, host_id);
   if ((unlink(fullname) == -1) && (errno != ENOENT))
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to unlink() `%s' : %s",
                 fullname, strerror(errno));
   }

   return;
}
