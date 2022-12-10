/*
 *  queue_spy.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2022 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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

DESCR__S_M1
/*
 ** NAME
 **   queue_spy - shows the messages queued by the FD
 **
 ** SYNOPSIS
 **   queue_spy [-w <AFD work dir>] [--version]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   03.02.1998 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>                 /* exit()                            */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>                  /* open()                            */
#include <unistd.h>                 /* fstat(), lseek(), write()         */
#include <sys/mman.h>               /* mmap()                            */
#include <errno.h>
#include "fddefs.h"
#include "version.h"

/* Global variables. */
int        sys_log_fd = STDERR_FILENO;
char       *p_work_dir = NULL;
const char *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int              fd,
                    *no_msg_queued;
   char             file[MAX_PATH_LENGTH + FIFO_DIR_LENGTH + MSG_QUEUE_FILE_LENGTH],
                    *ptr,
                    work_dir[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx     stat_buf;
#else
   struct stat      stat_buf;
#endif
   struct queue_buf *qb;

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD. */
   if (get_afd_path(&argc, argv, work_dir) < 0) 
   {
      exit(INCORRECT);
   }

   (void)sprintf(file, "%s%s%s", work_dir, FIFO_DIR, MSG_QUEUE_FILE);
   if ((fd = open(file, O_RDWR)) == -1)
   {
      (void)fprintf(stderr, _("Failed to open() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

#ifdef HAVE_STATX
   if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(fd, &stat_buf) == -1)
#endif
   {
      (void)fprintf(stderr, _("Failed to access `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if ((ptr = mmap(NULL,
#ifdef HAVE_STATX
                   stat_buf.stx_size, (PROT_READ | PROT_WRITE),
#else
                   stat_buf.st_size, (PROT_READ | PROT_WRITE),
#endif
                   MAP_SHARED, fd, 0)) == (caddr_t)-1)
   {
      (void)fprintf(stderr, _("Failed to mmap() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   no_msg_queued = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   qb = (struct queue_buf *)ptr;

   if (*no_msg_queued > 0)
   {
      int i;

#ifdef _WITH_BURST_MISS_CHECK
      (void)fprintf(stdout,
                    "Message number    Pid    time        Pos    FC  FS         Retr CP RHFB Message name\n");
#else
      (void)fprintf(stdout,
                    "Message number    Pid    time        Pos    FC  FS         Retr CP RHF Message name\n");
#endif
      for (i = 0; i < *no_msg_queued; i++)
      {
#if SIZEOF_PID_T == 4
         (void)fprintf(stdout, "%-17.0f %-6d ",
                       qb[i].msg_number, (pri_pid_t)qb[i].pid);
#else
         (void)fprintf(stdout, "%-17.0f %-6lld ",
                       qb[i].msg_number, (pri_pid_t)qb[i].pid);
#endif
#if SIZEOF_TIME_T == 4
         (void)fprintf(stdout, "%-11ld ", (pri_time_t)qb[i].creation_time);
#else
         (void)fprintf(stdout, "%-11lld ", (pri_time_t)qb[i].creation_time);
#endif
#if SIZEOF_OFF_T == 4
         (void)fprintf(stdout, "%-6d %-3u %-10ld %-4u %-3d%c%c%c%c %s\n",
#else
         (void)fprintf(stdout, "%-6d %-3u %-10lld %-4u %-3d%c%c%c%c %s\n",
#endif
                       qb[i].pos, qb[i].files_to_send,
                       (pri_off_t)qb[i].file_size_to_send,
                       qb[i].retries, qb[i].connect_pos,
                       (qb[i].special_flag & RESEND_JOB) ? 'R' : ' ',
                       (qb[i].special_flag & HELPER_JOB) ? 'H' : ' ',
                       (qb[i].special_flag & FETCH_JOB) ? 'F' : ' ',
#ifdef _WITH_BURST_MISS_CHECK
                       (qb[i].special_flag & QUEUED_FOR_BURST) ? 'B' : ' ',
#else
                       '\0',
#endif
                       qb[i].msg_name);
      }
   }
   else
   {
      (void)fprintf(stdout, _("No messages queued.\n"));
   }

   ptr -= AFD_WORD_OFFSET;
#ifdef HAVE_STATX
   if (munmap(ptr, stat_buf.stx_size) == -1)
#else
   if (munmap(ptr, stat_buf.st_size) == -1)
#endif
   {
      (void)fprintf(stderr, _("Failed to munmap() %s : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
   }
   (void)close(fd);

   exit(INCORRECT);
}
