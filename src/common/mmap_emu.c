/*
 *  mmap_emu.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1994 - 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#include <stdio.h>         /* snprintf()                                 */
#include <string.h>        /* strlen()                                   */
#include <stdlib.h>        /* atoi()                                     */
#include <unistd.h>        /* unlink(), open(), read(), write(), close(),*/
                           /* getpid()                                   */
#include <sys/types.h>
#include <sys/stat.h>      /* mkfifo()                                   */
#include <sys/ipc.h>
#include <sys/shm.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "mmap_emu.h"

/* External global variables. */
extern char *p_work_dir;


/*++++++++++++++++++++++++++++ mmap_emu() +++++++++++++++++++++++++++++++*/
caddr_t
mmap_emu(caddr_t addr,
         size_t  len,
         int     prot,
         int     flag,
         char    *filename,
         off_t   off)
{
   caddr_t  shmptr = (caddr_t) -1;
#ifndef HAVE_MMAP
   int      i,
            read_fd,
            write_fd,
            shmid;
   size_t   buf_length;
   char     *ptr,
            fifoname[MAX_PATH_LENGTH],
            request_fifo[MAX_PATH_LENGTH],
            strshmid[15],
            buf[BUFSIZE];

   (void)snprintf(fifoname, MAX_PATH_LENGTH,
#if SIZEOF_PID_T == 4
                  "%s%s/shmid.%d.fifo",
#else
                  "%s%s/shmid.%lld.fifo",
#endif
                  p_work_dir, FIFO_DIR, (pri_pid_t)getpid());
   (void)snprintf(request_fifo, MAX_PATH_LENGTH, "%s%s%s",
                  p_work_dir, FIFO_DIR, REQUEST_FIFO);
   if (mkfifo(fifoname, FILE_MODE) < 0)
   {
      return((caddr_t) -1);
   }

   errno = 0;
   while (((write_fd = open(request_fifo, 1)) < 0) &&
          ((errno == 0) || (errno == ENOENT)))
   {
      errno = 0;
      (void)my_usleep(10000L);
   }
   if (errno != 0)
   {
      (void)unlink(fifoname);
      (void)close(write_fd);
      return((caddr_t) -1);
   }
   if ((read_fd = open(fifoname, O_NDELAY)) < 0)
   {
      (void)unlink(fifoname);
      (void)close(write_fd);
      return((caddr_t) -1);
   }

   (void)snprintf(buf, BUFSIZE, "1\t%s\t%d\t%s\n", filename, len, fifoname);
#ifdef _MMAP_EMU_DEBUG
   (void)fprintf(stderr,"MMAP_EMU  : %s", buf);
#endif
   buf_length = strlen((char *)buf);
   if (write(write_fd, buf, buf_length) != buf_length)
   {
      (void)unlink(fifoname);
      (void)close(read_fd);
      (void)close(write_fd);
      return((caddr_t) -1);
   }

   while (read(read_fd, buf, BUFSIZE) <= 0)
      /* Do nothing. */;

   ptr = buf;
   i = 0;
   while (*ptr != '\n')
      strshmid[i++] = *(ptr++);
   strshmid[i] = '\0';
   shmid = atoi((char *)strshmid);

#ifdef _MMAP_EMU_DEBUG
   (void)fprintf(stderr,"MMAP_EMU  : Trying to attach\n");
#endif
   if ((shmptr = (caddr_t)shmat(shmid, 0, 0)) == (caddr_t) -1)
   {
      (void)unlink(fifoname);
      (void)close(read_fd);
      (void)close(write_fd);
      return((caddr_t) -1);
   }
#ifdef _MMAP_EMU_DEBUG
   (void)fprintf(stderr,"MMAP_EMU  : Attach succesful\n");
#endif

   (void)unlink(fifoname);
   (void)close(read_fd);
   (void)close(write_fd);
#endif /* HAVE_MMAP */

   return(shmptr + MAX_PATH_LENGTH);
}
