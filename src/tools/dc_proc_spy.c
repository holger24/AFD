/*
 *  dc_proc_spy.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   dc_proc_spy - shows all current process data
 **
 ** SYNOPSIS
 **   dc_proc_spy [-w <AFD work dir>] [--version]
 **
 ** DESCRIPTION
 **   This program shows all process that are sre started by dir_check.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.03.2008 H.Kiehl Created
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
#ifdef HAVE_MMAP
# include <sys/mman.h>              /* mmap()                            */
#endif
#include <errno.h>
#include "version.h"
#include "amgdefs.h"

/* Global variables. */
int        sys_log_fd = STDERR_FILENO;
char       *p_work_dir = NULL;
const char *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int                 fd,
                       *no_of_process;
   char                file[MAX_PATH_LENGTH],
                       *ptr,
                       work_dir[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx        stat_buf;
#else
   struct stat         stat_buf;
#endif
   struct dc_proc_list *dcpl;

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD. */
   if (get_afd_path(&argc, argv, work_dir) < 0) 
   {
      exit(INCORRECT);
   }

   (void)sprintf(file, "%s%s%s", work_dir, FIFO_DIR, DCPL_FILE_NAME);
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

#ifdef HAVE_MMAP
   if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                   stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                   stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                   MAP_SHARED, fd, 0)) == (caddr_t)-1)
#else
   if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                       stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                       stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                       MAP_SHARED, file, 0)) == (caddr_t)-1)
#endif
   {
      (void)fprintf(stderr, _("Failed to mmap() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (close(fd) == -1)
   {
      (void)fprintf(stderr, _("Failed to close() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
   }
   no_of_process = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   dcpl = (struct dc_proc_list *)ptr;

   if (*no_of_process > 0)
   {
      int i;

      (void)fprintf(stdout, _("No of dir_config process : %d\n"), *no_of_process);
      (void)fprintf(stdout, "Pid        fra_pos    Job ID\n");
      for (i = 0; i < *no_of_process; i++)
      {
         (void)fprintf(stdout, "%-10x %-10d %-10x\n",
                       dcpl[i].pid, dcpl[i].fra_pos, dcpl[i].job_id);
      }
   }
   else
   {
      (void)fprintf(stdout, _("No proccess currently active by dir_check.\n"));
   }

   ptr -= AFD_WORD_OFFSET;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
   if (munmap(ptr, stat_buf.stx_size) == -1)
# else
   if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
   if (munmap(ptr) == -1)
#endif
   {
      (void)fprintf(stderr, _("Failed to munmap() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
   }

   exit(INCORRECT);
}
