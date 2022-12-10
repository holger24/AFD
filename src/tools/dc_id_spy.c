/*
 *  dc_id_spy.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2004 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   dc_id_spy - shows all DIR_CONFIG names
 **
 ** SYNOPSIS
 **   dc_id_spy [-w <AFD work dir>] [--version]
 **
 ** DESCRIPTION
 **   This program shows all DIR_CONFIG names, where they are located
 **   and the ID.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   08.01.2004 H.Kiehl Created
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

/* Global variables. */
int        sys_log_fd = STDERR_FILENO;
char       *p_work_dir = NULL;
const char *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int                    fd,
                          *no_of_dc_ids;
   char                   file[MAX_PATH_LENGTH + FIFO_DIR_LENGTH + DC_LIST_FILE_LENGTH],
                          *ptr,
                          work_dir[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx           stat_buf;
#else
   struct stat            stat_buf;
#endif
   struct dir_config_list *dcl;

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD. */
   if (get_afd_path(&argc, argv, work_dir) < 0) 
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   (void)sprintf(file, "%s%s%s", work_dir, FIFO_DIR, DC_LIST_FILE);
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
   no_of_dc_ids = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   dcl = (struct dir_config_list *)ptr;

   if (*no_of_dc_ids > 0)
   {
      int i;

      (void)fprintf(stdout, _("No of DIR_CONFIG ID's : %d\n"), *no_of_dc_ids);
      (void)fprintf(stdout, _("Id         DIR_CONFIG\n"));
      for (i = 0; i < *no_of_dc_ids; i++)
      {
         (void)fprintf(stdout, "%-10x %s\n",
                       dcl[i].dc_id, dcl[i].dir_config_file);
      }
   }
   else
   {
      (void)fprintf(stdout, _("No DIR_CONFIG's.\n"));
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
      (void)fprintf(stderr, "Failed to munmap() `%s' : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
   }

   exit(INCORRECT);
}
