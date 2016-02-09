/*
 *  wmo2ascii.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 - 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   wmo2ascii - removes SOH, ETX and the two carriage returns from
 **               the given file
 **
 ** SYNOPSIS
 **   int wmo2ascii(char *filename, off_t *filesize)
 **
 ** DESCRIPTION
 **   The function wmo2ascii will remove SOH, ETX and the two carriage
 **   returns from the given WMO file. It will convert the following
 **   text from:
 **
 **       <SOH><CR><CR><LF>nnn<CR><CR><LF>
 **       WMO header<CR><CR><LF>WMO message<CR><CR><LF><ETX>
 **
 **   to:
 **
 **       WMO header<LF>WMO message<LF>
 **
 **   in addition <CR><CR><LF> will be changed to <LF>.
 **   The file name will not change.
 **
 ** RETURN VALUES
 **   On failure INCORRECT is returned otherwise SUCCESS will be
 **   returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.07.2002 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>            /* rename()                                */
#include <string.h>           /* strerror()                              */
#include <stdlib.h>           /* malloc(), free()                        */
#include <ctype.h>            /* isdigit()                               */
#include <unistd.h>           /* read(), write(), close(), unlink()      */
#include <sys/types.h>
#include <sys/stat.h>         /* fstat()                                 */
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"


/*############################# wmo2ascii() #############################*/
int
wmo2ascii(char *file_path, char *p_file_name, off_t *filesize)
{
   int  from_fd,
        ret = INCORRECT;
   char from[MAX_PATH_LENGTH];

   *filesize = 0;
   (void)snprintf(from, MAX_PATH_LENGTH, "%s/%s", file_path, p_file_name);
   if ((from_fd = open(from, O_RDONLY)) == -1)
   {
      receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                  _("wmo2ascii(): Failed to open() `%s' : %s"),
                  from, strerror(errno));
   }
   else
   {
      struct stat stat_buf;

      if (fstat(from_fd, &stat_buf) == -1)
      {
         receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                     _("wmo2ascii(): Failed to stat() `%s' : %s"),
                     from, strerror(errno));
      }
      else
      {
         if (stat_buf.st_size > 0)
         {
            int  to_fd;
            char to[MAX_PATH_LENGTH];

            (void)snprintf(to, MAX_PATH_LENGTH, "%s/.wmo2ascii", file_path);
            if ((to_fd = open(to, O_WRONLY | O_CREAT | O_TRUNC,
                              stat_buf.st_mode)) == -1)
            {
               receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                           _("wmo2ascii(): Failed to open() `%s' : %s"),
                           to, strerror(errno));
            }
            else
            {
               char *read_buffer,
                    *write_buffer;

               if (((read_buffer = malloc(stat_buf.st_blksize + 1)) == NULL) ||
                   ((write_buffer = malloc(stat_buf.st_blksize + 1)) == NULL))
               {
                  receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                              _("wmo2ascii(): malloc() error : %s"),
                              strerror(errno));
                  free(read_buffer);
               }
               else
               {
                  ssize_t bytes_buffered;

                  if ((bytes_buffered = read(from_fd, read_buffer,
                                             stat_buf.st_blksize)) == -1)
                  {
                     receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                 _("wmo2ascii(): Failed to read() `%s' : %s"),
                                 from, strerror(errno));
                  }
                  else
                  {
                     off_t length,
                           length_done;
                     char  *read_ptr = read_buffer,
                           *ptr_end = read_buffer + bytes_buffered,
                           *write_ptr = write_buffer;

                     /*
                      * Check if there is a message counter, if yes
                      * remove it as well.
                      */
                     if ((bytes_buffered > 10) &&
                         (*read_ptr == '\001') &&
                         (*(read_ptr + 1) == '\015') &&
                         (*(read_ptr + 2) == '\015') &&
                         (*(read_ptr + 3) == '\012'))
                     {
                        read_ptr += 4;
                        if ((isdigit((int)(*read_ptr))) &&
                            (isdigit((int)(*(read_ptr + 1)))) &&
                            (isdigit((int)(*(read_ptr + 2)))))
                        {
                           char *tmp_ptr = read_ptr + 3;

                           while (tmp_ptr < ptr_end)
                           {
                              if (isdigit((int)(*tmp_ptr)) == 0)
                              {
                                 break;
                              }
                              tmp_ptr++;
                           }
                           if ((*tmp_ptr == '\015') &&
                               (*(tmp_ptr + 1) == '\015') &&
                               (*(tmp_ptr + 2) == '\012'))
                           {
                              read_ptr = tmp_ptr + 3;
                           }
                        }
                     }

                     while (read_ptr < ptr_end)
                     {
                        if ((*read_ptr != '\001') && (*read_ptr != '\003') &&
                            (*read_ptr != '\015'))
                        {
                           *write_ptr = *read_ptr;
                           write_ptr++;
                        }
                        read_ptr++;
                     }

                     length = write_ptr - write_buffer;
                     if (writen(to_fd, write_buffer, length,
                                stat_buf.st_blksize) != length)
                     {
                        receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                    _("wmo2ascii(): Failed to writen() `%s' : %s"),
                                    to, strerror(errno));
                     }
                     else
                     {
                        length_done = length;
                        while (bytes_buffered == stat_buf.st_blksize)
                        {
                           if ((bytes_buffered = read(from_fd, read_buffer,
                                                      stat_buf.st_blksize)) == -1)
                           {
                              receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                          _("wmo2ascii(): Failed to read() `%s' : %s"),
                                          from, strerror(errno));
                              (void)close(from_fd);
                              (void)close(to_fd);
                              (void)unlink(to);
                              free(read_buffer);
                              free(write_buffer);
                              return(ret);
                           }
                           read_ptr = read_buffer,
                           ptr_end = read_buffer + bytes_buffered,
                           write_ptr = write_buffer;
                           while (read_ptr < ptr_end)
                           {
                              if ((*read_ptr != '\001') &&
                                  (*read_ptr != '\003') &&
                                  (*read_ptr != '\015'))
                              {
                                 *write_ptr = *read_ptr;
                                 write_ptr++;
                              }
                              read_ptr++;
                           }
                           length = write_ptr - write_buffer;
                           if (writen(to_fd, write_buffer, length,
                                      stat_buf.st_blksize) != length)
                           {
                              receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                          _("wmo2ascii(): Failed to writen() `%s' : %s"),
                                          to, strerror(errno));
                              (void)close(from_fd);
                              (void)close(to_fd);
                              (void)unlink(to);
                              free(read_buffer);
                              free(write_buffer);
                              return(ret);
                           }
                           length_done += length;
                        }
                        if (unlink(from) == -1)
                        {
                           receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                       _("wmo2ascii(): Failed to unlink() `%s' : %s"),
                                       from, strerror(errno));
                        }
                        if (rename(to, from) == -1)
                        {
                           receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                       _("wmo2ascii(): Failed to rename() `%s' to `%s' : %s"),
                                       to, from, strerror(errno));
                        }
                        else
                        {
                           ret = SUCCESS;
                           *filesize = length_done;
                        }
                     }
                  }
                  free(read_buffer);
                  free(write_buffer);
               }
               if (close(to_fd) == -1)
               {
                  receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                              _("wmo2ascii(): Failed to close() `%s' : %s"),
                              to, strerror(errno));
               }
            }
         }
      }
      if (close(from_fd) == -1)
      {
         receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                     _("wmo2ascii(): Failed to close() `%s' : %s"),
                     from, strerror(errno));
      }
   }

   return(ret);
}
