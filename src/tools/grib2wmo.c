/*
 *  grib2wmo.c - Part of AFD, an automatic file distribution program.
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

DESCR__S_M1
/*
 ** NAME
 **   grib2wmo - changes GRIB file to a WMO container file
 **
 ** SYNOPSIS
 **   grib2wmo <file name>
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   15.02.2003 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>            /* fprintf()                               */
#include <string.h>           /* strerror()                              */
#include <ctype.h>            /* isalpha()                               */
#include <stdlib.h>           /* exit()                                  */
#include <unistd.h>           /* STDERR_FILENO                           */
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>           /* Definition of AT_* constants            */
#endif
#include <sys/stat.h>
#include <errno.h>

/* Global variables. */
int receive_log_fd = STDERR_FILENO,
    sys_log_fd = STDERR_FILENO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int ret;

   if ((argc >= 2) && (argc < 4))
   {
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat stat_buf;
#endif

#ifdef HAVE_STATX
      if (statx(0, argv[1], AT_STATX_SYNC_AS_STAT, 0, &stat_buf) == -1)
#else
      if (stat(argv[1], &stat_buf) == -1)
#endif
      {
         (void)fprintf(stderr, _("Failed to access `%s' : %s\n"),
                       argv[1], strerror(errno));
         ret = INCORRECT;
      }
      else
      {
         char  *p_cccc;
         off_t size;

         size = 0;
         if ((argc == 3) && (isalpha((int)(argv[2][0]))) &&
             (isalpha((int)(argv[2][1]))) && (isalpha((int)(argv[2][2]))) &&
             (isalpha((int)(argv[2][3]))) && (argv[2][4] == '\0'))
         {
            p_cccc = argv[2];
         }
         else
         {
            p_cccc = NULL;
         }
         if (convert_grib2wmo(argv[1], &size, p_cccc) == INCORRECT)
         {
            (void)fprintf(stderr, _("Failed to convert %s\n"), argv[1]);
            ret = INCORRECT;
         }
         else
         {
#if SIZEOF_OFF_T == 4
            (void)fprintf(stdout, _("converted %s, new size is %ld\n"),
#else
            (void)fprintf(stdout, _("converted %s, new size is %lld\n"),
#endif
                          argv[1], (pri_off_t)size);
            ret = SUCCESS;
         }
      }
   }
   else
   {
      (void)fprintf(stderr, _("Usage: %s <file name> [CCCC]\n"), argv[0]);
      ret = INCORRECT;
   }

   exit(ret);
}
