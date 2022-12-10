/*
 *  attach_afd_mon_status.c - Part of AFD, an automatic file distribution
 *                            program.
 *  Copyright (c) 2005 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   attach_afd_mon_status - attaches to the AFD_MON status area
 **
 ** SYNOPSIS
 **   int attach_afd_mon_status(void)
 **
 ** DESCRIPTION
 **   The function attach_afd_mon_status() attaches to the AFD_MON
 **   status area.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to attache to the AFD_MON status
 **   area, otherwise it will return SUCCESS and the pointer
 **   'p_afd_status' which points to the attached shared memory
 **   area.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   08.06.2005 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                    /* strcpy(), strcat(), strerror() */
#include <unistd.h>                    /* close(), read()                */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>                 /* mmap()                         */
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>                    /* O_RDONLY                       */
#endif
#include <errno.h>
#include "mondefs.h"

/* External global variables. */
extern char                  *p_work_dir;
extern struct afd_mon_status *p_afd_mon_status;


/*####################### attach_afd_mon_status() #######################*/
int
attach_afd_mon_status(void)
{
   int          fd,
                loop_counter;
   char         *ptr,
                afd_mon_status_file[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   (void)strcpy(afd_mon_status_file, p_work_dir);
   (void)strcat(afd_mon_status_file, FIFO_DIR);
   (void)strcat(afd_mon_status_file, AFD_MON_STATUS_FILE);
   loop_counter = 0;
   while ((fd = coe_open(afd_mon_status_file, O_RDWR)) < 0)
   {
      my_usleep(80000L);
      loop_counter++;
      if (loop_counter > 1000) /* => 2 seconds */
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to open() `%s' : %s"),
                    afd_mon_status_file, strerror(errno));
         return(INCORRECT);
      }
   }
#ifdef HAVE_STATX
   if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(fd, &stat_buf) == -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                 _("Failed to statx() `%s' : %s"),
#else
                 _("Failed to fstat() `%s' : %s"),
#endif
                 afd_mon_status_file, strerror(errno));
      (void)close(fd);
      return(INCORRECT);
   }
#ifdef HAVE_MMAP
   if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                   stat_buf.stx_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
# else
                   stat_buf.st_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
# endif
                   fd, 0)) == (caddr_t) -1)
#else
   if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                       stat_buf.stx_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
# else
                       stat_buf.st_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
# endif
                       afd_mon_status_file, 0)) == (caddr_t) -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("mmap() error : %s"), strerror(errno));
      (void)close(fd);
      return(INCORRECT);
   }
   if (close(fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 _("close() error : %s"), strerror(errno));
   }
   p_afd_mon_status = (struct afd_mon_status *)ptr;

   return(SUCCESS);
}
