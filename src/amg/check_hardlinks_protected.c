/*
 *  check_hardlinks_protected.c - Part of AFD, an automatic file
 *                                distribution program.
 *  Copyright (c) 2024 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_hardlinks_protected - checks if hardlinks are protected.
 **
 ** SYNOPSIS
 **   int check_hardlinks_protected(void)
 **
 ** DESCRIPTION
 **   This function checks if /proc/sys/fs/protected_hardlinks is
 **   set on the system.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.05.2024 H.Kiehl Created
 **   02.12.2024 H.Kiehl When we fail to open() the proc file return
 **                      PERMANENT_INCORRECT, so we do not fall over it
 **                      again.
 **
 */
DESCR__E_M3

#include <stdlib.h>            /* atoi()                                 */
#include <string.h>            /* strerror()                             */
#include <fcntl.h>             /* open()                                 */
#include <unistd.h>            /* read(), close()                        */
#include <errno.h>

#define PROC_FILE "/proc/sys/fs/protected_hardlinks"

/* External global variables. */
extern int force_set_hardlinks_protected;


/*##################### check_hardlinks_protected() #####################*/
int
check_hardlinks_protected(void)
{
   int fd,
       ret;

   if ((fd = open(PROC_FILE, O_RDONLY)) == -1)
   {
      if ((force_set_hardlinks_protected == YES) && (errno == EPERM))
      {
         /*
          * Assume earlier kernel where this value can only be read
          * as root. Give admin a possibility to always have this
          * function return 1 (ie. hardlinks are protected).
          */
         ret = 1;
      }
      else
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to open() %s : %s", PROC_FILE, strerror(errno));
         ret = PERMANENT_INCORRECT;
      }
   }
   else
   {
      char buffer[MAX_INT_LENGTH + 1];

      if ((ret = read(fd, buffer, MAX_INT_LENGTH)) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to read() %s : %s", PROC_FILE, strerror(errno));
      }
      else
      {
         buffer[ret] = '\0';
         ret = atoi(buffer);
      }
      if (close(fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Failed to close() %s : %s", PROC_FILE, strerror(errno));
      }
   }

   return(ret);
}
