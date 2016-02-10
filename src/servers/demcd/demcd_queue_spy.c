/*
 *  demcd_queue_spy.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2015 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   demcd_queue_spy - shows the messages queued by De Mail
 **                     Confirmation Daemon
 **
 ** SYNOPSIS
 **   demcd_queue_spy [-w <AFD work dir>] [--version]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.09.2015 H.Kiehl Created
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
#include "demcddefs.h"
#include "version.h"

/* Global variables. */
int        sys_log_fd = STDERR_FILENO;
char       *p_work_dir = NULL;
const char *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int                    fd,
                          *no_demcd_queued;
   char                   file[MAX_PATH_LENGTH],
                          *ptr,
                          work_dir[MAX_PATH_LENGTH];
   struct stat            stat_buf;
   struct demcd_queue_buf *dqb;

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD. */
   if (get_afd_path(&argc, argv, work_dir) < 0) 
   {
      exit(INCORRECT);
   }

   (void)sprintf(file, "%s%s%s", work_dir, FIFO_DIR, DEMCD_QUEUE_FILE);
   if ((fd = open(file, O_RDWR)) == -1)
   {
      (void)fprintf(stderr, _("Failed to open() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (fstat(fd, &stat_buf) == -1)
   {
      (void)fprintf(stderr, _("Failed to fstat() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if ((ptr = mmap(NULL, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                   MAP_SHARED, fd, 0)) == (caddr_t)-1)
   {
      (void)fprintf(stderr, _("Failed to mmap() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   no_demcd_queued = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   dqb = (struct demcd_queue_buf *)ptr;

   if (*no_demcd_queued > 0)
   {
      int i;

      (void)fprintf(stdout,
                    "%-11s %-15s %-10s %-*s %-*s CT  de-mail-privat-id\n",
                    "Log time", "File name", "Size",
                    (MAX_HOSTNAME_LENGTH > 10) ? MAX_HOSTNAME_LENGTH : 10,
                    "Alias name", MAX_INT_HEX_LENGTH, "Job ID");
      for (i = 0; i < *no_demcd_queued; i++)
      {
#if SIZEOF_TIME_T == 4
# if SIZEOF_OFF_T == 4
         (void)fprintf(stdout, "%-11ld %-15s %-10ld %-*s %-*x %-3d %s\n",
# else
         (void)fprintf(stdout, "%-11ld %-15s %-10lld %-*s %-*x %-3d %s\n",
# endif
#else
# if SIZEOF_OFF_T == 4
         (void)fprintf(stdout, "%-11lld %-15s %-10ld %-*s %-*x %-3d %s\n",
# else
         (void)fprintf(stdout, "%-11lld %-15s %-10lld %-*s %-*x %-3d %s\n",
# endif
#endif
                       (pri_time_t)dqb[i].log_time, dqb[i].file_name,
                       (pri_off_t)dqb[i].file_size,
                       (MAX_HOSTNAME_LENGTH > 10) ? MAX_HOSTNAME_LENGTH : 10,
                       dqb[i].alias_name, MAX_INT_HEX_LENGTH, dqb[i].jid,
                       dqb[i].confirmation_type, dqb[i].de_mail_privat_id);
      }
   }
   else
   {
      (void)fprintf(stdout, _("No messages queued.\n"));
   }

   ptr -= AFD_WORD_OFFSET;
   if (munmap(ptr, stat_buf.st_size) == -1)
   {
      (void)fprintf(stderr, _("Failed to munmap() %s : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
   }
   (void)close(fd);

   exit(INCORRECT);
}
