/*
 *  seek_cache_position.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2009 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   seek_cache_position - Use cache file to seek in log file
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
 **   19.08.2009 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>                      /* Definition of AT_* constants */
#endif
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>                   /* mmap(), munmap()             */
#endif
#include <unistd.h>
#include <errno.h>
#include "aldadefs.h"

#define MAX_ALDA_CACHE_READ_SIZE 10485760 /* 10 MiB */

/* External global variables. */
extern int cache_step_size;


/*######################## seek_cache_position() ########################*/
void
seek_cache_position(struct log_file_data *log, time_t search_time)
{
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat stat_buf;
#endif

   search_time -= 1;
#ifdef HAVE_STATX
   if (statx(log->cache_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == 0)
#else
   if (fstat(log->cache_fd, &stat_buf) == 0)
#endif
   {
      int   left,
            no_of_entries,
            read_size,
            right;
      off_t pos;

      read_size = cache_step_size + cache_step_size;
#ifdef HAVE_STATX
      no_of_entries = stat_buf.stx_size / read_size;
#else
      no_of_entries = stat_buf.st_size / read_size;
#endif
      left = 1;
      right = no_of_entries;

#ifdef HAVE_STATX
      if (stat_buf.stx_size > MAX_ALDA_CACHE_READ_SIZE)
#else
      if (stat_buf.st_size > MAX_ALDA_CACHE_READ_SIZE)
#endif
      {
         char   buffer[128];
         time_t *time_val;
         off_t  *offset_val;

         time_val = (time_t *)buffer;
         offset_val = (off_t *)(buffer + cache_step_size);
         while (right >= left)
         {
            pos = (left + right) / 2;
            if (lseek(log->cache_fd, ((pos - 1) * read_size), SEEK_SET) == -1)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to lseek() in %s : %s",
                          log->log_cache_dir, strerror(errno));
               return;
            }
            if (read(log->cache_fd, buffer, read_size) != read_size)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to read() %d bytes in %s : %s",
                          read_size, log->log_cache_dir, strerror(errno));
               return;
            }
            if (search_time < *time_val)
            {
               right = pos - 1;
            }
            else
            {
               left = pos + 1;
            }
            if (search_time == *time_val)
            {
               break;
            }
         }
         if (fseeko(log->fp, *offset_val, SEEK_SET) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to fseeko() in %s : %s",
                       log->log_dir, strerror(errno));
         }
         else
         {
            log->bytes_read = *offset_val;
         }
      }
      else
      {
         char   *ptr;
         time_t *time_val;
         off_t  *offset_val;

#ifdef HAVE_MMAP
         if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                         stat_buf.stx_size, PROT_READ, MAP_SHARED,
# else
                         stat_buf.st_size, PROT_READ, MAP_SHARED,
# endif
                         log->cache_fd, 0)) == (caddr_t) -1)
#else
         if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                             stat_buf.stx_size, PROT_READ, MAP_SHARED,
# else
                             stat_buf.st_size, PROT_READ, MAP_SHARED,
# endif
                             log->log_cache_dir, 0)) == (caddr_t) -1)
#endif
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to mmap() to %s : %s",
                       log->log_cache_dir, strerror(errno));
            return;
         }

         while (right >= left)
         {
            pos = (left + right) / 2;
            time_val = (time_t *)(ptr + ((pos - 1) * read_size));
            if (search_time < *time_val)
            {
               right = pos - 1;
            }
            else
            {
               left = pos + 1;
            }
            if (search_time == *time_val)
            {
               break;
            }
         }
         offset_val = (off_t *)(ptr + ((pos - 1) * read_size) + cache_step_size);
         if (fseeko(log->fp, *offset_val, SEEK_SET) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to fseeko() in %s : %s",
                       log->log_dir, strerror(errno));
         }
         else
         {
            log->bytes_read = *offset_val;
         }
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
         if (munmap(ptr, stat_buf.stx_size) == -1)
# else
         if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
         if (munmap_emu(ptr) == -1)
#endif
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to munmap() from %s : %s",
                       log->log_cache_dir, strerror(errno));
         }
      }
   }

   return;
}
