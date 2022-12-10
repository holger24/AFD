/*
 *  get_full_date.c - Part of AFD, an automatic file distribution program.
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

DESCR__S_M1
/*
 ** NAME
 **   get_full_date - gets the full creation date of a file
 **
 ** SYNOPSIS
 **   get_full_date [--version] [-w <AFD working directory>] <file>
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   -2 when usage is wrong otherwise the return value from stat()
 **   is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.05.1998 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#ifdef HAVE_STATX
# include <fcntl.h>           /* Definition of AT_* constants            */
#endif
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>


/*############################## main() #################################*/
int
main(int argc, char *argv[])
{
   int status = 0;

   if (argc == 2)
   {
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat stat_buf;
#endif

#ifdef HAVE_STATX
      if ((status = statx(0, argv[1], AT_STATX_SYNC_AS_STAT,
                          STATX_MTIME, &stat_buf)) == -1)
#else
      if ((status = stat(argv[1], &stat_buf)) == -1)
#endif
      {
         (void)fprintf(stderr, _("Failed to access `%s' : %s\n"),
                       argv[1], strerror(errno));
      }
      else
      {
#ifdef HAVE_STATX
         (void)fprintf(stdout, "%s --> %s",
                        argv[1], ctime(&stat_buf.stx_mtime.tv_sec));
#else
         (void)fprintf(stdout, "%s --> %s",
                        argv[1], ctime(&stat_buf.st_mtime));
#endif
      }
   }
   else
   {
      (void)fprintf(stderr, _("Usage: %s <file/dir name>\n"), argv[0]);
      status = -2;
   }

   exit(status);
}
