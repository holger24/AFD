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
 **   void display_file(FILE *p_data)
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
#include "afdddefs.h"

/* External global variables. */
extern char *p_work_dir;


/*############################ display_file() ###########################*/
void
display_file(FILE *p_data)
{
   int          fd,
                from_fd;
   size_t       hunk,
                left;
   char         *buffer;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   /* Open source file. */
   if ((from_fd = open(p_work_dir, O_RDONLY)) < 0)
   {
      (void)fprintf(p_data, "500 Failed to open() %s : %s (%s %d)\r\n",
                    p_work_dir, strerror(errno), __FILE__, __LINE__);
      return;
   }

#ifdef HAVE_STATX
   if (statx(from_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) != 0)
#else
   if (fstat(from_fd, &stat_buf) != 0)
#endif
   {
      (void)fprintf(p_data, "500 Failed to access %s : %s (%s %d)\r\n",
                    p_work_dir, strerror(errno), __FILE__, __LINE__);
      (void)close(from_fd);
      return;
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
      (void)fprintf(p_data, "500 Failed to malloc() memory : %s (%s %d)\r\n",
                    strerror(errno), __FILE__, __LINE__);
         (void)close(from_fd);
      return;
   }

   (void)fprintf(p_data, "211- Command successful\r\n");
   (void)fflush(p_data);
   fd = fileno(p_data);

   while (left > 0)
   {
      if (read(from_fd, buffer, hunk) != hunk)
      {
         (void)fprintf(p_data, "500 Failed to read() %s : %s (%s %d)\r\n",
                       p_work_dir, strerror(errno), __FILE__, __LINE__);
         free(buffer);
         (void)close(from_fd);
         return;
      }

      if (write(fd, buffer, hunk) != hunk)
      {
         (void)fprintf(p_data, "520 write() error : %s (%s %d)\r\n",
                       strerror(errno), __FILE__, __LINE__);
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

   (void)fprintf(p_data, "200 End of data\r\n");
   free(buffer);
   if (close(from_fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 _("Failed to close() %s : %s"), p_work_dir, strerror(errno));
   }

   return;
}
