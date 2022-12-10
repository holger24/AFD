/*
 *  cache_spy.c - Part of AFD, an automatic file distribution program.
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
 **   cache_spy - shows all messages currently being cached by the FD
 **
 ** SYNOPSIS
 **   cache_spy [-w <AFD work dir>] [--version]
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
 **   09.04.2002 H.Kiehl Added cache pos.
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
   int                  fd,
                        *no_msg_cached;
   char                 file[MAX_PATH_LENGTH + FIFO_DIR_LENGTH + MSG_CACHE_FILE_LENGTH],
                        *ptr,
                        work_dir[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx         stat_buf;
#else
   struct stat          stat_buf;
#endif
   struct msg_cache_buf *mb;

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD. */
   if (get_afd_path(&argc, argv, work_dir) < 0) 
   {
      exit(INCORRECT);
   }

   (void)sprintf(file, "%s%s%s", work_dir, FIFO_DIR, MSG_CACHE_FILE);
   if ((fd = open(file, O_RDWR)) == -1)
   {
      (void)fprintf(stderr,
                    _("Failed to open() `%s' : %s (%s %d)\n"),
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
      (void)fprintf(stderr,
                    _("Failed to access `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if ((ptr = mmap(NULL,
#ifdef HAVE_STATX
                   stat_buf.stx_size, (PROT_READ | PROT_WRITE),
#else
                   stat_buf.st_size, (PROT_READ | PROT_WRITE),
#endif
                   MAP_SHARED, fd, 0)) == (caddr_t) -1)
   {
      (void)fprintf(stderr,
                    _("Failed to mmap() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   no_msg_cached = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   mb = (struct msg_cache_buf *)ptr;

   if (*no_msg_cached > 0)
   {
      int i;

      (void)fprintf(stdout, "Pos  Hostname FSA-pos Job-ID      msg-time    last-trans  Age-limit Typ inFSA Port\n");
      for (i = 0; i < *no_msg_cached; i++)
      {
#if SIZEOF_TIME_T == 4
         (void)fprintf(stdout, "%-4d %-*s %-7d %-11x %-11ld %-11ld %-9u %-3d %-4d %d\n",
#else
         (void)fprintf(stdout, "%-4d %-*s %-7d %-11x %-11lld %-11lld %-9u %-3d %-4d %d\n",
#endif
                       i, MAX_HOSTNAME_LENGTH, mb[i].host_name,
                       mb[i].fsa_pos,
                       mb[i].job_id,
                       (pri_time_t)mb[i].msg_time,
                       (pri_time_t)mb[i].last_transfer_time,
                       mb[i].age_limit,
                       (int)mb[i].type,
                       (int)mb[i].in_current_fsa,
                       mb[i].port);
      }
   }
   else
   {
      (void)fprintf(stdout, _("No messages cached.\n"));
   }

   ptr -= AFD_WORD_OFFSET;
#ifdef HAVE_STATX
   if (munmap(ptr, stat_buf.stx_size) == -1)
#else
   if (munmap(ptr, stat_buf.st_size) == -1)
#endif
   {
      (void)fprintf(stderr,
                    _("Failed to munmap() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
   }
   (void)close(fd);

   exit(INCORRECT);
}
