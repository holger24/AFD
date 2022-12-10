/*
 *  attach_afd_status.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   attach_afd_status - attaches to the AFD status area
 **
 ** SYNOPSIS
 **   int attach_afd_status(int *fd, int timeout)
 **
 ** DESCRIPTION
 **   The function attach_afd_status() reads the shared memory ID
 **   from the file AFD_STATUS_FILE and attaches to this shared
 **   memory area.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to attache to the AFD status
 **   area, otherwise it will return SUCCESS and the pointer
 **   'p_afd_status' which points to the attached shared memory
 **   area.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.02.1996 H.Kiehl Created
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

#define AAS_SLEEP_INTERVAL 80000L

/* External global variables. */
extern char              *p_work_dir;
extern struct afd_status *p_afd_status;


/*######################### attach_afd_status() #########################*/
int
attach_afd_status(int *fd, int timeout)
{
   int          local_fd,
                loop_counter,
                max_loops,
                *ptr_fd;
   char         *ptr,
                afd_status_file[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   if (fd == NULL)
   {
      ptr_fd = &local_fd;
   }
   else
   {
      ptr_fd = fd;
   }

   (void)snprintf(afd_status_file, MAX_PATH_LENGTH, "%s%s/%s.%x",
                  p_work_dir, FIFO_DIR, AFD_STATUS_FILE, get_afd_status_struct_size());
   loop_counter = 0;
   max_loops = (timeout * 100) / (AAS_SLEEP_INTERVAL / 10000);
   while ((*ptr_fd = coe_open(afd_status_file, O_RDWR)) < 0)
   {
      my_usleep(AAS_SLEEP_INTERVAL);
      loop_counter++;
      if (loop_counter > max_loops)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to open() `%s' : %s"),
                    afd_status_file, strerror(errno));
         return(INCORRECT);
      }
   }
#ifdef HAVE_STATX
   if (statx(*ptr_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(*ptr_fd, &stat_buf) == -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                 _("Failed to statx() `%s' : %s"),
#else
                 _("Failed to fstat() `%s' : %s"),
#endif
                 afd_status_file, strerror(errno));
      (void)close(*ptr_fd);
      return(INCORRECT);
   }
#ifdef HAVE_STATX
   if (stat_buf.stx_size != sizeof(struct afd_status))
#else
   if (stat_buf.st_size != sizeof(struct afd_status))
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                 _("Incorrect size, `%s' is %ld bytes and not %u bytes."),
#else
                 _("Incorrect size, `%s' is %lld bytes and not %u bytes."),
#endif
#ifdef HAVE_STATX
                 afd_status_file, (pri_off_t)stat_buf.stx_size,
#else
                 afd_status_file, (pri_off_t)stat_buf.st_size,
#endif
                 sizeof(struct afd_status));
      (void)close(*ptr_fd);
      return(INCORRECT);
   }
#ifdef HAVE_MMAP
   if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                   stat_buf.stx_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
# else
                   stat_buf.st_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
# endif
                   *ptr_fd, 0)) == (caddr_t) -1)
#else
   if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                       stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                       stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                       MAP_SHARED, afd_status_file, 0)) == (caddr_t) -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("mmap() error : %s"), strerror(errno));
      (void)close(*ptr_fd);
      return(INCORRECT);
   }
   if (fd == NULL)
   {
      if (close(*ptr_fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("close() error : %s"), strerror(errno));
      }
   }
   p_afd_status = (struct afd_status *)ptr;

   return(SUCCESS);
}
