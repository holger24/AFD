/*
 *  open_log_file.c - Part of AFD, an automatic file distribution program.
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

DESCR__S_M3
/*
 ** NAME
 **   open_log_file - opens a file or reading and writing and creates
 **                   it if does not exist
 **
 ** SYNOPSIS
 **   FILE *open_log_file(char *log_file_name)
 **
 ** DESCRIPTION
 **   This function tries to open and create (if it does not exist)
 **   the file log_file_name. If it fails to do so because the disk
 **   is full, it will continue trying until it succeeds.
 **
 ** RETURN VALUES
 **   Upon successful completion open_log_file() returns a FILE
 **   pointer.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.11.1998 H.Kiehl Created
 **   14.08.2009 H.Kiehl Added support for log cache file.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>               /* exit()                              */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>               /* sleep()                             */
#include <errno.h>
#include "logdefs.h"

/* External global variables. */
extern char *iobuf;


/*########################### open_log_file() ###########################*/
FILE *
#ifdef WITH_LOG_CACHE
open_log_file(char  *log_file_name,
              char  *current_log_cache_file,
              int   *log_cache_fd,
              off_t *log_pos)
#else
open_log_file(char *log_file_name)
#endif
{
   static FILE *log_file;

   if ((log_file = fopen(log_file_name, "a+")) == NULL)
   {
      if (errno == ENOSPC)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "DISK FULL!!! Will retry in %d second interval.",
                    DISK_FULL_RESCAN_TIME);

         while (errno == ENOSPC)
         {
            (void)sleep(DISK_FULL_RESCAN_TIME);
            errno = 0;
            if ((log_file = fopen(log_file_name, "a+")) == NULL)
            {
               if (errno != ENOSPC)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Could not fopen() %s : %s",
                             log_file_name, strerror(errno));
                  exit(INCORRECT);
               }
            }
         }
         system_log(INFO_SIGN, __FILE__, __LINE__,
                    "Continuing after disk was full.");
      }
      else
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not fopen() %s : %s",
                    log_file_name, strerror(errno));
         exit(INCORRECT);
      }
   }
#ifdef WITH_LOG_CACHE
   else
   {
      if (current_log_cache_file != NULL)
      {
         int          fd;
# ifdef HAVE_STATX
         struct statx stat_buf;
# else
         struct stat  stat_buf;
# endif

         fd = fileno(log_file);
# ifdef HAVE_STATX
         if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                   STATX_SIZE, &stat_buf) == -1)
# else
         if (fstat(fd, &stat_buf) == -1)
# endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
# ifdef HAVE_STATX
                       "Failed to statx() %s : %s",
# else
                       "Failed to fstat() %s : %s",
# endif
                       log_file_name, strerror(errno));
         }
         else
         {
# ifdef HAVE_STATX
            *log_pos = stat_buf.stx_size;
# else
            *log_pos = stat_buf.st_size;
# endif
         }
      }
   }
#endif

   /*
    * Increase the buffer of lib C I/O routines to have better disk
    * utilization, since we do a lot of small writes in transfer, receive,
    * input and output log programms.
    */
   if (iobuf == NULL)
   {
      if ((iobuf = malloc(262144 + 8)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "malloc() error : %s", strerror(errno));
      }
   }
   if (iobuf != NULL)
   {
      errno = 0;
      if (setvbuf(log_file, iobuf, _IOFBF, 262144 + 8) != 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to change I/O-buffer to 262144 : %s",
                    strerror(errno));
      }
   }

#ifdef WITH_LOG_CACHE
   if (current_log_cache_file != NULL)
   {
      if ((*log_cache_fd = open(current_log_cache_file,
                                (O_WRONLY | O_CREAT | O_APPEND),
                                FILE_MODE)) == -1)
      {
         if (errno == ENOSPC)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "DISK FULL!!! Will retry in %d second interval.",
                       DISK_FULL_RESCAN_TIME);

            while (errno == ENOSPC)
            {
               (void)sleep(DISK_FULL_RESCAN_TIME);
               errno = 0;
               if ((*log_cache_fd = open(current_log_cache_file,
                                         (O_WRONLY | O_CREAT | O_APPEND),
                                         FILE_MODE)) == -1)
               {
                  if (errno != ENOSPC)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Could not open() %s : %s",
                                current_log_cache_file, strerror(errno));
                     exit(INCORRECT);
                  }
               }
            }
            system_log(INFO_SIGN, __FILE__, __LINE__,
                       "Continuing after disk was full.");
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not open() %s : %s",
                       current_log_cache_file, strerror(errno));
            exit(INCORRECT);
         }
      }
   }
#endif

   return(log_file);
}
