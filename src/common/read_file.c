/*
 *  read_file.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   read_file - reads the contents of a file into a buffer
 **
 ** SYNOPSIS
 **   off_t read_file(char *filename, char **buffer)
 **
 ** DESCRIPTION
 **   The function reads the contents of the file filename into a
 **   buffer for which it allocates memory. The caller of this function
 **   is responsible for releasing the memory.
 **
 ** RETURN VALUES
 **   The size of file filename when successful, otherwise INCORRECT
 **   will be returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.12.1997 H.Kiehl Created
 **   01.08.1999 H.Kiehl Return the size of the file.
 **   19.07.2001 H.Kiehl Removed fstat().
 **   07.09.2008 H.Kiehl Added fstat() to get best buffer size to read
 **                      from source file.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>               /* calloc(), free()                    */
#include <unistd.h>               /* close(), read()                     */
#include <sys/types.h>
#include <sys/stat.h>             /* fstat()                             */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>


/*############################# read_file() #############################*/
off_t
read_file(char *filename, char **buffer)
{
   int   fd;
   off_t bytes_buffered;

   /* Open file. */
   if ((fd = open(filename, O_RDONLY)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not open() `%s' : %s"), filename, strerror(errno));
      bytes_buffered = INCORRECT;
   }
   else
   {
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat stat_buf;
#endif

      /* Get best IO size to read file. */
#ifdef HAVE_STATX
      if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                0, &stat_buf) == -1)
#else
      if (fstat(fd, &stat_buf) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                    _("Could not statx() `%s' : %s"),
#else
                    _("Could not fstat() `%s' : %s"),
#endif
                    filename, strerror(errno));
         bytes_buffered = INCORRECT;
      }
      else
      {
         /* Allocate enough memory for the contents of the file. */
#ifdef HAVE_STATX
         if ((*buffer = malloc(stat_buf.stx_blksize + 1)) == NULL)
#else
         if ((*buffer = malloc(stat_buf.st_blksize + 1)) == NULL)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Could not malloc() memory : %s"), strerror(errno));
            bytes_buffered = INCORRECT;
         }
         else
         {
            int  bytes_read;
            char *read_ptr,
                 *tmp_buffer;

            bytes_buffered = 0;
            read_ptr = *buffer;

            /* Read file into buffer. */
            do
            {
#ifdef HAVE_STATX
               if ((bytes_read = read(fd, read_ptr, stat_buf.stx_blksize)) == -1)
#else
               if ((bytes_read = read(fd, read_ptr, stat_buf.st_blksize)) == -1)
#endif
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Failed to read `%s' : %s"),
                             filename, strerror(errno));
                  (void)close(fd);
                  free(*buffer);
                  *buffer = NULL;
                  return(INCORRECT);
               }
               bytes_buffered += bytes_read;
#ifdef HAVE_STATX
               if (bytes_read == stat_buf.stx_blksize)
#else
               if (bytes_read == stat_buf.st_blksize)
#endif
               {
#ifdef HAVE_STATX
                  if ((tmp_buffer = realloc(*buffer,
                                            bytes_buffered + stat_buf.stx_blksize + 1)) == NULL)
#else
                  if ((tmp_buffer = realloc(*buffer,
                                            bytes_buffered + stat_buf.st_blksize + 1)) == NULL)
#endif
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                _("Could not realloc() memory : %s"),
                                strerror(errno));
                     (void)close(fd);
                     free(*buffer);
                     *buffer = NULL;
                     return(INCORRECT);
                  }
                  *buffer = tmp_buffer;
                  read_ptr = *buffer + bytes_buffered;
               }
#ifdef HAVE_STATX
            } while (bytes_read == stat_buf.stx_blksize);
#else
            } while (bytes_read == stat_buf.st_blksize);
#endif
            (*buffer)[bytes_buffered] = '\0';
         }
      }

      /* Close file. */
      if (close(fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("close() error : %s"), strerror(errno));
      }
   }

   return(bytes_buffered);
}
