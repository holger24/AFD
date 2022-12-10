/*
 *  get_display_file.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2022 Deutscher Wetterdienst (DWD),
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
 **   get_display_data - writes the contents of a file to a socket
 **
 ** SYNOPSIS
 **   int get_display_data(SSL  *,
 **                        char *search_file,
 **                        int  log_number,
 **                        char *search_string,
 **                        int  start_line,
 **                        int  no_of_lines,
 **                        int  show_time,
 **                        int  file_no)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.06.1997 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                     /* strerror()                    */
#include <stdlib.h>                     /* malloc(), free()              */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <openssl/ssl.h>
#include <unistd.h>                     /* read(), write(), close()      */
#include <errno.h>
#include "afddsdefs.h"
#include "server_common_defs.h"

/* Local global variables. */
static struct fd_cache fc[MAX_AFDD_LOG_FILES];


/*########################## get_display_data() #########################*/
int
get_display_data(SSL  *ssl,
                 char *search_file,
                 int  log_number,
                 char *search_string,
                 int  start_line,
                 int  no_of_lines,
                 int  show_time,
                 int  file_no)
{
   int          from_fd = -1,
                length;
   size_t       hunk,
                left;
   char         *buffer;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   /* Open source file. */
   length = strlen(search_file);
   (void)snprintf(&search_file[length], MAX_PATH_LENGTH - length, "%d", file_no);

   if (start_line != NOT_SET)
   {
#ifdef HAVE_STATX
      if (statx(0, search_file, AT_STATX_SYNC_AS_STAT,
                STATX_INO | STATX_SIZE, &stat_buf) != 0)
#else
      if (stat(search_file, &stat_buf) != 0)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to access %s : %s", search_file, strerror(errno));
         return(INCORRECT);
      }
#ifdef HAVE_STATX
      if (stat_buf.stx_ino == fc[log_number].st_ino)
#else
      if (stat_buf.st_ino == fc[log_number].st_ino)
#endif
      {
         from_fd = fc[log_number].fd;
      }
      else
      {
         if (fc[log_number].fd != -1)
         {
            if (close(fc[log_number].fd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Failed to close() log file %d : %s",
                          log_number, strerror(errno));
            }
            fc[log_number].fd = -1;
            fc[log_number].st_ino = 0;
         }
      }
   }

   if (from_fd == -1)
   {
      if ((from_fd = open(search_file, O_RDONLY)) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to open() %s : %s", search_file, strerror(errno));
         return(INCORRECT);
      }

      if (start_line != NOT_SET)
      {
         fc[log_number].fd = from_fd;
#ifdef HAVE_STATX
         fc[log_number].st_ino = stat_buf.stx_ino;
#else
         fc[log_number].st_ino = stat_buf.st_ino;
#endif
      }
      else
      {
#ifdef HAVE_STATX
         if (statx(from_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                   STATX_SIZE, &stat_buf) != 0)
#else
         if (fstat(from_fd, &stat_buf) != 0)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to access %s : %s",
                       search_file, strerror(errno));
            (void)close(from_fd);
            return(INCORRECT);
         }
#ifdef HAVE_STATX
         else if (stat_buf.stx_size == 0)
#else
         else if (stat_buf.st_size == 0)
#endif
              {
                 (void)command(ssl, "500 File %s is empty.", search_file);
                 (void)close(from_fd);
                 return(SUCCESS);
              }
      }
   }

#ifdef HAVE_STATX
   left = hunk = stat_buf.stx_size;
#else
   left = hunk = stat_buf.st_size;
#endif

   if (hunk > HUNK_MAX)
   {
      hunk = HUNK_MAX;
   }

   if ((buffer = malloc(hunk)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() memory : %s", strerror(errno));
      (void)close(from_fd);
      return(INCORRECT);
   }

   (void)command(ssl, "211- Command successful");

   while (left > 0)
   {
      if (read(from_fd, buffer, hunk) != hunk)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to read() %s : %s", search_file, strerror(errno));
         free(buffer);
         (void)close(from_fd);
         return(INCORRECT);
      }

      if (ssl_write(ssl, buffer, hunk) != hunk)
      {
         free(buffer);
         (void)close(from_fd);
         return(INCORRECT);
      }
      left -= hunk;
      if (left < hunk)
      {
         hunk = left;
      }
   }

   (void)command(ssl, "200 End of data");
   free(buffer);
   if (close(from_fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 _("Failed to close() %s : %s"), search_file, strerror(errno));
   }

   return(SUCCESS);
}


/*######################## init_get_display_data() ######################*/
void
init_get_display_data(void)
{
   int i;

   for (i = 0; i < MAX_AFDD_LOG_FILES; i++)
   {
      fc[i].fd = -1;
      fc[i].st_ino = 0;
   }

   return;
}


/*####################### close_get_display_data() ######################*/
void
close_get_display_data(void)
{
   int i;

   for (i = 0; i < MAX_AFDD_LOG_FILES; i++)
   {
      if (fc[i].fd != -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Failed to close() log file %d : %s",
                    i, strerror(errno));
      }
      else
      {
         fc[i].fd = -1;
      }
      fc[i].st_ino = 0;
   }

   return;
}
