/*
 *  read_afd_istat_db.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   read_afd_istat_db - maps to AFD input statistic file
 **
 ** SYNOPSIS
 **   void read_afd_istat_db(int no_of_dirs)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   23.02.2003 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>          /* strcpy(), strerror(), memcpy(), memset() */
#include <time.h>            /* time()                                   */
#include <sys/time.h>        /* struct tm                                */
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>          /* exit(), malloc(), free()                 */
#ifdef HAVE_MMAP
# include <sys/mman.h>       /* mmap(), munmap()                         */
#endif
#include <fcntl.h>
#include <unistd.h>          /* close(), lseek(), write()                */
#include <errno.h>
#include "statdefs.h"

#ifdef HAVE_MMAP
# ifndef MAP_FILE    /* Required for BSD          */
#  define MAP_FILE 0 /* All others do not need it */
# endif
#endif

/* Global external variables. */
extern int                        locki_fd;
extern size_t                     istat_db_size;
extern char                       istatistic_file[],
                                  new_istatistic_file[];
extern struct afdistat            *istat_db;
extern struct fileretrieve_status *fra;


/*$$$$$$$$$$$$$$$$$$$$$$$$$ read_afd_istat_db() $$$$$$$$$$$$$$$$$$$$$$$$$*/
void
read_afd_istat_db(int no_of_dirs)
{
   int             old_status_fd = -1,
                   new_status_fd,
                   i;
   time_t          now;
   static int      no_of_old_dirs = 0;
   size_t          size = sizeof(struct istatistics),
                   old_istat_db_size = 0;
   char            *old_ptr = NULL, /* silence compiler */
                   *ptr;
   struct afdistat *old_istat_db;
#ifdef HAVE_STATX
   struct statx    stat_buf;
#else
   struct stat     stat_buf;
#endif
   struct tm       *p_ts;

   /*
    * Check if there is memory with an old database. If not
    * this is the first time and the memory and file have to
    * be created.
    */
   if (istat_db == (struct afdistat *)NULL)
   {
      /*
       * Now see if an old statistics file exists. If it does, mmap
       * to this file, so the data can be used to create the new
       * statistics file. If it does not exist, don't bother, since
       * we only need to initialize all values to zero.
       */
#ifdef HAVE_STATX
      if ((statx(0, istatistic_file, AT_STATX_SYNC_AS_STAT,
                 STATX_SIZE, &stat_buf) == -1) || (stat_buf.stx_size == 0))
#else
      if ((stat(istatistic_file, &stat_buf) == -1) || (stat_buf.st_size == 0))
#endif
      {
#ifdef HAVE_STATX
         if (stat_buf.stx_size == 0)
#else
         if (stat_buf.st_size == 0)
#endif
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Hmm..., old input statistic file is empty.");
         }
         else
         {
            if (errno != ENOENT)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to stat() %s : %s",
                          istatistic_file, strerror(errno));
               exit(INCORRECT);
            }
         }
         old_istat_db = NULL;
      }
      else /* An old statistics database file exists */
      {
         if ((locki_fd = lock_file(istatistic_file, OFF)) == LOCK_IS_SET)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Another process is currently using file %s",
                       istatistic_file);
            exit(INCORRECT);
         }
         if ((old_status_fd = open(istatistic_file, O_RDWR)) < 0)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to open() %s : %s",
                       istatistic_file, strerror(errno));
            exit(INCORRECT);
         }

#ifdef HAVE_MMAP
         if ((old_ptr = mmap(NULL,
# ifdef HAVE_STATX
                             stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                             stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                             (MAP_FILE | MAP_SHARED),
                             old_status_fd, 0)) == (caddr_t) -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not mmap() file %s : %s",
                       istatistic_file, strerror(errno));
            (void)close(old_status_fd);
            exit(INCORRECT);
         }
#else
# ifdef HAVE_STATX
         if ((old_ptr = malloc(stat_buf.stx_size)) == NULL)
# else
         if ((old_ptr = malloc(stat_buf.st_size)) == NULL)
# endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "malloc() error : %s", strerror(errno));
            exit(INCORRECT);
         }
# ifdef HAVE_STATX
         if (read(old_status_fd, old_ptr, stat_buf.stx_size) != stat_buf.stx_size)
# else
         if (read(old_status_fd, old_ptr, stat_buf.st_size) != stat_buf.st_size)
# endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to read() %s : %s",
                       istatistic_file, strerror(errno));
            free(old_ptr);
            (void)close(old_status_fd);
            exit(INCORRECT);
         }
#endif
         old_istat_db = (struct afdistat *)(old_ptr + AFD_WORD_OFFSET);
#ifdef HAVE_STATX
         old_istat_db_size = stat_buf.stx_size;
#else
         old_istat_db_size = stat_buf.st_size;
#endif
         no_of_old_dirs = (old_istat_db_size - AFD_WORD_OFFSET) /
                          sizeof(struct afdistat);
      }
   }
   else /* An old statistics database in memory does exist. */
   {
      /*
       * Store the following values so we can unmap the
       * old region later.
       */
      old_istat_db = istat_db;
      old_istat_db_size = istat_db_size;
      old_ptr = (char *)istat_db - AFD_WORD_OFFSET;
   }

   istat_db_size = AFD_WORD_OFFSET + (no_of_dirs * sizeof(struct afdistat));

   /* Create new temporary file. */
   if ((new_status_fd = open(new_istatistic_file, (O_RDWR | O_CREAT | O_TRUNC),
                             FILE_MODE)) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not open() %s : %s",
                 new_istatistic_file, strerror(errno));
      exit(INCORRECT);
   }
#ifdef HAVE_MMAP
   if (lseek(new_status_fd, istat_db_size - 1, SEEK_SET) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_SIZE_T == 4
                 "Could not seek() to %d [no_of_dirs=%d] on %s : %s",
#else
                 "Could not seek() to %lld [no_of_dirs=%d] on %s : %s",
#endif
                 (pri_size_t)(istat_db_size - 1), no_of_dirs,
                 new_istatistic_file, strerror(errno));
      exit(INCORRECT);
   }
   if (write(new_status_fd, "", 1) != 1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not write() to %s : %s",
                 new_istatistic_file, strerror(errno));
      exit(INCORRECT);
   }
   if ((ptr = mmap(NULL, istat_db_size, (PROT_READ | PROT_WRITE),
                   (MAP_FILE | MAP_SHARED),
                   new_status_fd, 0)) == (caddr_t) -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not mmap() file %s : %s",
                 new_istatistic_file, strerror(errno));
      exit(INCORRECT);
   }
#else
   if ((ptr = malloc(istat_db_size)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      exit(INCORRECT);
   }
#endif
   *(int *)ptr = no_of_dirs;
   *(ptr + sizeof(int) + 1 + 1 + 1) = CURRENT_STAT_VERSION;
   istat_db = (struct afdistat *)(ptr + AFD_WORD_OFFSET);
   (void)memset(istat_db, 0, (istat_db_size - AFD_WORD_OFFSET));

   if ((no_of_old_dirs < 1) && (old_istat_db != NULL))
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Failed to find any old hosts! [%d %d Bytes]",
                 no_of_old_dirs, old_istat_db_size);
   }

   /*
    * Now compare the old data with the FSA that was just read.
    */
   now = time(NULL);
   p_ts = gmtime(&now);
   if ((old_istat_db == NULL) || (no_of_old_dirs < 1))
   {
      for (i = 0; i < no_of_dirs; i++)
      {
         (void)strcpy(istat_db[i].dir_alias, fra[i].dir_alias);
         istat_db[i].start_time = now;
         if (p_ts->tm_yday >= DAYS_PER_YEAR)
         {
            istat_db[i].day_counter = 0;
         }
         else
         {
            istat_db[i].day_counter = p_ts->tm_yday;
         }
         istat_db[i].hour_counter = p_ts->tm_hour;
         istat_db[i].sec_counter = ((p_ts->tm_min * 60) + p_ts->tm_sec) / STAT_RESCAN_TIME;
         (void)memset(&istat_db[i].year, 0, (DAYS_PER_YEAR * size));
         (void)memset(&istat_db[i].day, 0, (HOURS_PER_DAY * size));
         (void)memset(&istat_db[i].hour, 0, (SECS_PER_HOUR * size));
         istat_db[i].prev_nfr = fra[i].files_received;
         istat_db[i].prev_nbr = fra[i].bytes_received;
      }
   }
   else
   {
      int position;

      for (i = 0; i < no_of_dirs; i++)
      {
         if ((position = locate_dir(old_istat_db,
                                    fra[i].dir_alias,
                                    no_of_old_dirs)) < 0)
         {
            (void)strcpy(istat_db[i].dir_alias, fra[i].dir_alias);
            istat_db[i].start_time = now;
            if (p_ts->tm_yday >= DAYS_PER_YEAR)
            {
               istat_db[i].day_counter = 0;
            }
            else
            {
               istat_db[i].day_counter = p_ts->tm_yday;
            }
            istat_db[i].hour_counter = p_ts->tm_hour;
            istat_db[i].sec_counter = ((p_ts->tm_min * 60) + p_ts->tm_sec) / STAT_RESCAN_TIME;
            (void)memset(&istat_db[i].year, 0, (DAYS_PER_YEAR * size));
            (void)memset(&istat_db[i].day, 0, (HOURS_PER_DAY * size));
            (void)memset(&istat_db[i].hour, 0, (SECS_PER_HOUR * size));
            istat_db[i].prev_nfr = fra[i].files_received;
            istat_db[i].prev_nbr = fra[i].bytes_received;
         }
         else
         {
            (void)memcpy(&istat_db[i], &old_istat_db[position],
                         sizeof(struct afdistat));
         }
      }
   }

   if (old_istat_db != NULL)
   {
#ifdef HAVE_MMAP
      if (munmap(old_ptr, old_istat_db_size) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to munmap() %s : %s",
                    istatistic_file, strerror(errno));
      }
#else
      free(old_ptr);
#endif
      if (locki_fd > -1)
      {
         if (close(locki_fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "close() error : %s", strerror(errno));
         }
      }
   }

#ifndef HAVE_MMAP
   if (write(new_status_fd, istat_db, istat_db_size) != istat_db_size)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not write() to %s : %s",
                 new_istatistic_file, strerror(errno));
      exit(INCORRECT);
   }
#endif
   if (close(new_status_fd) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }

   if (rename(new_istatistic_file, istatistic_file) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to rename() %s to %s : %s",
                 new_istatistic_file, istatistic_file, strerror(errno));
      exit(INCORRECT);
   }

   if ((locki_fd = lock_file(istatistic_file, OFF)) < 0)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to lock to file `%s' [%d]", istatistic_file, locki_fd);
      exit(INCORRECT);
   }

   /* Store current number of hosts. */
   no_of_old_dirs = no_of_dirs;

   if (old_status_fd != -1)
   {
      if (close(old_status_fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "close() error : %s", strerror(errno));
      }
   }

   return;
}
