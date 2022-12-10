/*
 *  display_file.c - Part of AFD, an automatic file distribution program.
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
 **   display_file - writes the contents of a file to a socket
 **
 ** SYNOPSIS
 **   void display_file(SSL *ssl)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   24.06.1997 H.Kiehl Created
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
#include <unistd.h>                     /* read(), write(), close()      */
#include <errno.h>
#include "afddsdefs.h"
#include "server_common_defs.h"

/* External global variables. */
extern char *p_work_dir;


/*############################ display_file() ###########################*/
void
display_file(SSL *ssl)
{
   int from_fd;

   /* Open source file. */
   if ((from_fd = open(p_work_dir, O_RDONLY)) < 0)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to open() %s : %s", p_work_dir, strerror(errno));
   }
   else
   {
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat stat_buf;
#endif

#ifdef HAVE_STATX
      if (statx(from_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) != 0)
#else
      if (fstat(from_fd, &stat_buf) != 0)
#endif
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to access %s : %s", p_work_dir, strerror(errno));
      }
      else
      {
         size_t hunk,
                left;
         char   *buffer;

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
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to malloc() memory : %s", strerror(errno));
         }
         else
         {
            (void)command(ssl, "211- Command successful");

            while (left > 0)
            {
               if (read(from_fd, buffer, hunk) != hunk)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Failed to read() %s : %s",
                             p_work_dir, strerror(errno));
                  free(buffer);
                  (void)close(from_fd);
                  return;
               }

               if (ssl_write(ssl, buffer, hunk) != hunk)
               {
                  free(buffer);
                  (void)close(from_fd);
                  return;
               }
               left -= hunk;
               if (left < hunk)
               {
                  hunk = left;
               }
            }

            (void)command(ssl, "200 End of data");
            free(buffer);
         }
      }
      if (close(from_fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("Failed to close() %s : %s"),
                    p_work_dir, strerror(errno));
      }
   }

   return;
}
