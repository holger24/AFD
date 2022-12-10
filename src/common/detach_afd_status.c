/*
 *  detach_afd_status.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
#ifdef HAVE_STATX
# include <fcntl.h>                    /* Definition of AT_* constants   */
#endif
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
   char         afd_status_file[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   (void)snprintf(afd_status_file, MAX_PATH_LENGTH, "%s%s/%s.%x",
                  p_work_dir, FIFO_DIR, AFD_STATUS_FILE,
                  get_afd_status_struct_size());
#ifdef HAVE_STATX
   if (statx(0, afd_status_file, AT_STATX_SYNC_AS_STAT,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (stat(afd_status_file, &stat_buf) == -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                 _("Failed to statx() `%s' : %s"),
#else
                 _("Failed to stat() `%s' : %s"),
#endif
                 afd_status_file, strerror(errno));
      return(INCORRECT);
   }
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
   if (munmap((void *)p_afd_status, stat_buf.stx_size) == -1)
# else
   if (munmap((void *)p_afd_status, stat_buf.st_size) == -1)
# endif
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
