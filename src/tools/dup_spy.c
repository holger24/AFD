/*
 *  dup_spy.c - Part of AFD, an automatic file distribution program.
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

DESCR__S_M1
/*
 ** NAME
 **   dup_spy - shows all duplicate for the given id
 **
 ** SYNOPSIS
 **   dup_spy [-w <AFD work dir>] [--version] <job|dir-id>
 **
 ** DESCRIPTION
 **   This program shows all duplicates for the given job or directory ID.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.06.2005 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>                 /* exit(), strtoul()                 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>                  /* open()                            */
#include <unistd.h>                 /* fstat(), lseek(), write()         */
#include <time.h>                   /* strftime(), localtime()           */
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
   unsigned int   id;
   int            fd,
                  *no_of_crcs;
   char           file[MAX_PATH_LENGTH + AFD_FILE_DIR_LENGTH + CRC_DIR_LENGTH + MAX_INT_HEX_LENGTH],
                  *ptr,
                  work_dir[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx   stat_buf;
#else
   struct stat    stat_buf;
#endif
   struct crc_buf *cdb;

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD. */
   if (get_afd_path(&argc, argv, work_dir) < 0) 
   {
      exit(INCORRECT);
   }

   if (argc != 2)
   {
      (void)fprintf(stderr,
                    _("Usage: %s [-w <AFD work dir>] [--version] <job|dir-id>\n"),
                    argv[0]);
      exit(INCORRECT);
   }
   else
   {
      id = (unsigned int)strtoul(argv[1], NULL, 16);
   }

   (void)sprintf(file, "%s%s%s/%x", work_dir, AFD_FILE_DIR, CRC_DIR, id);
   if ((fd = open(file, O_RDONLY)) == -1)
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
                   stat_buf.stx_size, PROT_READ,
# else
                   stat_buf.st_size, PROT_READ,
# endif
                   MAP_SHARED, fd, 0)) == (caddr_t)-1)
#else
   if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                       stat_buf.stx_size, PROT_READ,
# else
                       stat_buf.st_size, PROT_READ,
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
   no_of_crcs = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   cdb = (struct crc_buf *)ptr;

   if (*no_of_crcs > 0)
   {
      int  i;
      char time_str[25];

      (void)strftime(time_str, 25, "%c",
                     localtime((time_t *)(ptr - AFD_WORD_OFFSET + SIZEOF_INT + 4)));
      (void)fprintf(stdout, "No of CRC's : %d\n", *no_of_crcs);
#if SIZEOF_TIME_T == 4
      (void)fprintf(stdout, "Check time  : %s (%ld)\n",
#else
      (void)fprintf(stdout, "Check time  : %s (%lld)\n",
#endif
                    time_str,
                    (pri_time_t)(*(time_t *)(ptr - AFD_WORD_OFFSET + SIZEOF_INT + 4)));
      (void)fprintf(stdout, "CRC         Timeout                                Flag\n");
      for (i = 0; i < *no_of_crcs; i++)
      {
         (void)strftime(time_str, 25, "%c", localtime(&cdb[i].timeout));
#if SIZEOF_TIME_T == 4
         (void)fprintf(stdout, "%-10x  %s %-12ld ",
#else
         (void)fprintf(stdout, "%-10x  %s %-12lld ",
#endif
                       cdb[i].crc, time_str, (pri_time_t)cdb[i].timeout);
         if (cdb[i].flag & DC_FILENAME_ONLY)
         {
            (void)fprintf(stdout, " FILENAME_ONLY");
         }
         if (cdb[i].flag & DC_FILE_CONTENT)
         {
            (void)fprintf(stdout, " FILE_CONTENT");
         }
         if (cdb[i].flag & DC_FILE_CONT_NAME)
         {
            (void)fprintf(stdout, " FILE_CONT_NAME");
         }
         if (cdb[i].flag & DC_NAME_NO_SUFFIX)
         {
            (void)fprintf(stdout, " NAME_NO_SUFFIX");
         }
         if (cdb[i].flag & DC_FILENAME_AND_SIZE)
         {
            (void)fprintf(stdout, " FILENAME_AND_SIZE");
         }
         if (cdb[i].flag & DC_CRC32)
         {
            (void)fprintf(stdout, " CRC32");
         }
         if (cdb[i].flag & DC_CRC32C)
         {
            (void)fprintf(stdout, " CRC32C");
         }
         if (cdb[i].flag & DC_MURMUR3)
         {
            (void)fprintf(stdout, " MURMUR3");
         }
         if (cdb[i].flag & DC_DELETE)
         {
            (void)fprintf(stdout, " DELETE");
         }
         if (cdb[i].flag & DC_STORE)
         {
            (void)fprintf(stdout, " STORE");
         }
         if (cdb[i].flag & DC_WARN)
         {
            (void)fprintf(stdout, " WARN");
         }
         if (cdb[i].flag & TIMEOUT_IS_FIXED)
         {
            (void)fprintf(stdout, " TIMEOUT_IS_FIXED");
         }
         if (cdb[i].flag & USE_RECIPIENT_ID)
         {
            (void)fprintf(stdout, " USE_RECIPIENT_ID\n");
         }
         else
         {
            (void)fprintf(stdout, "\n");
         }
      }
   }
   else
   {
      (void)fprintf(stdout, "No CRC's.\n");
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
