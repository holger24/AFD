/*
 *  truncate_old_year_stat.c - Part of AFD, an automatic file distribution
 *                             program.
 *  Copyright (c) 2020 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   truncate_old_year_stat - truncate unused data from year statistic
 **                            file
 **
 ** SYNOPSIS
 **   truncate_old_year_stat <file name>
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   Returns 0 on success.  Otherwise -1 is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   05.10.2020 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>          /* strcpy(), strerror(), memcpy(), memset() */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
#include <sys/mman.h>        /* mmap(), munmap()                         */
#endif
#include <stdlib.h>          /* exit()                                   */
#include <fcntl.h>
#include <unistd.h>          /* close(), lseek(), write()                */
#include <errno.h>
#include "statdefs.h"
#include "version.h"

#ifdef HAVE_MMAP
#ifndef MAP_FILE    /* Required for BSD          */
#define MAP_FILE 0  /* All others do not need it */
#endif
#endif

/* Global external variables. */
int             no_of_dirs,
                no_of_hosts,
                sys_log_fd = STDERR_FILENO;
char            istatistic_file[MAX_PATH_LENGTH],
                new_istatistic_file[MAX_PATH_LENGTH],
                new_statistic_file[MAX_PATH_LENGTH],
                *p_work_dir,
                statistic_file[MAX_PATH_LENGTH];
struct afdistat *istat_db;
struct afdstat  *stat_db;
const char      *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$ truncate_old_year_stat() $$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int          fd;
   char         new_file[MAX_PATH_LENGTH],
                *ptr,
                tmp_char,
                work_dir[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }

   if (argc != 2)
   {
      (void)fprintf(stderr, "Usage: %s <statistic filename to convert>\n",
                    argv[0]);
      exit(INCORRECT);
   }
   new_istatistic_file[0] = '\0';
   new_statistic_file[0] = '\0';

   /* Determine if we need to convert output or input statistics. */
   if (posi(ISTATISTIC_FILE_NAME, argv[1]) != NULL)
   {
      (void)strcpy(new_file, argv[1]);
      (void)strcpy(istatistic_file, argv[1]);
      ptr = istatistic_file + strlen(istatistic_file) - 1;
      while ((ptr > istatistic_file) && (*ptr != '/'))
      {
         ptr--;
      }
      if (*ptr == '/')
      {
         ptr++;
         tmp_char = *ptr;
         *ptr = '.';
         ptr++;
         while (*ptr != '\0')
         {
            *ptr = tmp_char;
            ptr++;
            tmp_char = *ptr;
         }
         *ptr = '\0';
      }
      else
      {
         istatistic_file[0] = '.';
         (void)strcpy(&istatistic_file[1], argv[1]);
      }
      statistic_file[0] = '\0';
   }
   else if (posi(ISTATISTIC_FILE_NAME, argv[1]) == NULL)
        {
           (void)strcpy(new_file, argv[1]);
           (void)strcpy(statistic_file, argv[1]);
           ptr = statistic_file + strlen(statistic_file) - 1;
           while ((ptr > statistic_file) && (*ptr != '/'))
           {
              ptr--;
           }
           if (*ptr == '/')
           {
              ptr++;
              tmp_char = *ptr;
              *ptr = '.';
              ptr++;
              while (*ptr != '\0')
              {
                 *ptr = tmp_char;
                 ptr++;
                 tmp_char = *ptr;
              }
              *ptr = '\0';
           }
           else
           {
              statistic_file[0] = '.';
              (void)strcpy(&statistic_file[1], argv[1]);
           }
           istatistic_file[0] = '\0';
        }
        else
        {
           (void)fprintf(stderr, "Unknown statistic filename %s.\n",
                         argv[1]);
           exit(INCORRECT);
        }

#ifdef HAVE_STATX
   if ((statx(0, new_file, AT_STATX_SYNC_AS_STAT,
              STATX_SIZE, &stat_buf) == -1) || (stat_buf.stx_size == 0))
#else
   if ((stat(new_file, &stat_buf) == -1) || (stat_buf.st_size == 0))
#endif
   {
#ifdef HAVE_STATX
      if (stat_buf.stx_size == 0)
#else
      if (stat_buf.st_size == 0)
#endif
      {
         (void)fprintf(stderr, "File is empty.\n");
      }
      else
      {
         (void)fprintf(stderr, "Failed to access %s : %s\n",
                       new_file, strerror(errno));
      }
      exit(INCORRECT);
   }
   if ((fd = open(new_file, O_RDWR)) < 0)
   {
      (void)fprintf(stderr, "Failed to open() %s : %s\n",
                    new_file, strerror(errno));
      exit(INCORRECT);
   }
#ifdef HAVE_MMAP
   if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                   stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                   stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                   (MAP_FILE | MAP_SHARED), fd, 0)) == (caddr_t) -1)
   {
      (void)fprintf(stderr,
                    "Could not mmap() file %s : %s\n",
                    new_file, strerror(errno));
      (void)close(fd);
      exit(INCORRECT);
   }
#else
# ifdef HAVE_STATX
   if ((ptr = malloc(stat_buf.stx_size)) == NULL)
# else
   if ((ptr = malloc(stat_buf.st_size)) == NULL)
# endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      exit(INCORRECT);
   }
# ifdef HAVE_STATX
   if (read(fd, ptr, stat_buf.stx_size) != stat_buf.stx_size)
# else
   if (read(fd, ptr, stat_buf.st_size) != stat_buf.st_size)
# endif
   {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to read() %s : %s",
                 new_file, strerror(errno));
      free(ptr);
      (void)close(fd);
      exit(INCORRECT);
   }
#endif
   if (istatistic_file[0] == '\0')
   {
      if (*(ptr + sizeof(int) + 1 + 1 + 1) != CURRENT_STAT_VERSION)
      {
         (void)fprintf(stderr, "Unable to convert this version.\n");
         exit(INCORRECT);
      }
      stat_db = (struct afdstat *)(ptr + AFD_WORD_OFFSET);
#ifdef HAVE_STATX
      no_of_hosts = (stat_buf.stx_size - AFD_WORD_OFFSET) /
                    sizeof(struct afdstat);
#else
      no_of_hosts = (stat_buf.st_size - AFD_WORD_OFFSET) /
                    sizeof(struct afdstat);
#endif

      save_old_output_year(-1);
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
      if (munmap(ptr, stat_buf.stx_size) == -1)
# else
      if (munmap(ptr, stat_buf.st_size) == -1)
# endif
      {
         (void)fprintf(stderr, "Failed to munmap() %s : %s\n",
                       new_file, strerror(errno));
      }
#else
# ifdef HAVE_STATX
      if (write(fd, ptr, stat_buf.stx_size) != stat_buf.stx_size)
# else
      if (write(fd, ptr, stat_buf.st_size) != stat_buf.st_size)
# endif
      {
         (void)fprintf(stderr, "Could not write() to %s : %s\n",
                       new_file, strerror(errno));
      }
      if (close(fd) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                  "close() error : %s", strerror(errno));
      }
      free(ptr);
#endif
      (void)unlink(new_file);
      if (rename(statistic_file, new_file) == -1)
      {
         (void)fprintf(stderr, "Failed to rename %s to %s : %s\n",
                       statistic_file, new_file, strerror(errno));
      }
   }
   else
   {
      if (*(ptr + sizeof(int) + 1 + 1 + 1) != CURRENT_ISTAT_VERSION)
      {
         (void)fprintf(stderr, "Unable to convert this version.\n");
         exit(INCORRECT);
      }
      istat_db = (struct afdistat *)(ptr + AFD_WORD_OFFSET);
#ifdef HAVE_STATX
      no_of_dirs = (stat_buf.stx_size - AFD_WORD_OFFSET) /
                   sizeof(struct afdistat);
#else
      no_of_dirs = (stat_buf.st_size - AFD_WORD_OFFSET) /
                   sizeof(struct afdistat);
#endif

      save_old_input_year(-1);
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
      if (munmap(ptr, stat_buf.stx_size) == -1)
# else
      if (munmap(ptr, stat_buf.st_size) == -1)
# endif
      {
         (void)fprintf(stderr, "Failed to munmap() %s : %s\n",
                       new_file, strerror(errno));
      }
#else
# ifdef HAVE_STATX
      if (write(fd, ptr, stat_buf.stx_size) != stat_buf.stx_size)
# else
      if (write(fd, ptr, stat_buf.st_size) != stat_buf.st_size)
# endif
      {
         (void)fprintf(stderr, "Could not write() to %s : %s\n",
                       new_file, strerror(errno));
      }
      if (close(fd) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                  "close() error : %s", strerror(errno));
      }
      free(ptr);
#endif
      (void)unlink(new_file);
      if (rename(istatistic_file, new_file) == -1)
      {
         (void)fprintf(stderr, "Failed to rename %s to %s : %s\n",
                       istatistic_file, new_file, strerror(errno));
      }
   }

   (void)fprintf(stdout, "Truncated statistic file %s\n", argv[1]);

   exit(SUCCESS);
}
