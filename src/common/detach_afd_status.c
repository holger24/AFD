/*
 *  detach_afd_status.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   detach_afd_status - detach from the AFD status area
 **
 ** SYNOPSIS
 **   int detach_afd_status(void)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to detach from the AFD status
 **   area, otherwise it will return SUCCESS.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.07.2001 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                    /* strcpy(), strcat(), strerror() */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>                 /* munmap()                       */
#endif
#include <errno.h>

/* External global variables. */
extern char              *p_work_dir;
extern struct afd_status *p_afd_status;


/*######################### detach_afd_status() #########################*/
int
detach_afd_status(void)
{
   char        afd_status_file[MAX_PATH_LENGTH];
   struct stat stat_buf;

   (void)strcpy(afd_status_file, p_work_dir);
   (void)strcat(afd_status_file, FIFO_DIR);
   (void)strcat(afd_status_file, STATUS_SHMID_FILE);
   if (stat(afd_status_file, &stat_buf) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to stat() `%s' : %s"),
                 afd_status_file, strerror(errno));
      return(INCORRECT);
   }
#ifdef HAVE_MMAP
   if (munmap((void *)p_afd_status, stat_buf.st_size) == -1)
#else
   if (munmap_emu((void *)p_afd_status) < 0)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("munmap() error : %s"), strerror(errno));
      return(INCORRECT);
   }

   return(SUCCESS);
}
