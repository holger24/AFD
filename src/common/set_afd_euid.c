/*
 *  set_afd_euid.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 - 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   set_afd_euid - Sets the effective uid to the uid that AFD_WORK_DIR
 **                  belongs to
 **
 ** SYNOPSIS
 **   void set_afd_euid(char *work_dir)
 **
 ** DESCRIPTION
 **   This function gets the user ID of work_dir and sets the effective
 **   user ID to this value.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   03.03.2002 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>            /* strerror()                             */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>


/*########################### set_afd_euid() ############################*/
void
set_afd_euid(char *work_dir)
{
   struct stat stat_buf;

   if (stat(work_dir, &stat_buf) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to stat() `%s' : %s"), work_dir, strerror(errno));
   }
   else
   {
      uid_t euid;

      euid = geteuid();
      if (euid != stat_buf.st_uid)
      {
#ifdef _HPUX
         if (setreuid(-1, stat_buf.st_uid) == -1)
#else
         if (seteuid(stat_buf.st_uid) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to seteuid() to %d : %s"),
                       stat_buf.st_uid, strerror(errno));
         }
      }
   }

   return;
}
