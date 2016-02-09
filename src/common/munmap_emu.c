/*
 *  munmap_emu.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1994 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **
 ** SYNOPSIS
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   29.10.1994 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>         /* fprintf()                                  */
#include <string.h>        /* strlen()                                   */
#include <unistd.h>        /* write(), close()                           */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "mmap_emu.h"

/* External global variables. */
extern char *p_work_dir;


/*++++++++++++++++++++++++++++ munmap_emu() +++++++++++++++++++++++++++++*/
int
munmap_emu(char *shmptr)
{
#ifndef HAVE_MMAP
   int      i = 0,
            write_fd;
   size_t   buf_length;
   char     *ptr,
            buf[BUFSIZE],
            request_fifo[MAX_PATH_LENGTH];

   (void)strcpy(request_fifo, p_work_dir);
   (void)strcat(request_fifo, FIFO_DIR);
   (void)strcat(request_fifo, REQUEST_FIFO);
   if ((write_fd = open(request_fifo, 1)) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to open() `%s' : %s"),
                 request_fifo, strerror(errno));
      return(-1);
   }

   buf[i++] = '3';
   buf[i++] = '\t';
   shmptr -= MAX_PATH_LENGTH;
   ptr = shmptr;
   while ((*shmptr != '\n') && (i < BUFSIZE))
   {
      buf[i++] = *(shmptr++);
   }
   if (i == BUFSIZE)
   {
      (void)close(write_fd);
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not extract the filename."));
      return(-1);
   }
   if (shmdt(ptr) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("shmdt() error : %s"), strerror(errno));
      (void)close(write_fd);
      return(-1);
   }
   buf[i++] = '\n';
   buf[i] = '\0';
#ifdef _MMAP_EMU_DEBUG
   (void)fprintf(stderr,"munmap_emu(): %s", buf);
#endif
   buf_length = strlen((char *)buf);
   if (write(write_fd, buf, buf_length) != buf_length)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("write() fifo error : %s"), strerror(errno));
      (void)close(write_fd);
      return(-1);
   }

   if (close(write_fd) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to close() `%s' : %s"),
                 request_fifo, strerror(errno));
   }
#endif /* HAVE_MMAP */

   return(0);
}
