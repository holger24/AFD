/*
 *  gts2tiff.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2012 Deutscher Wetterdienst (DWD),
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
 **   gts2tiff - changes GTS (T4) file to a TIFF file
 **
 ** SYNOPSIS
 **   gts2tiff <T4 coded file>
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
 **   13.10.1996 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>            /* fprintf()                               */
#include <string.h>           /* strcpy(), strlen()                      */
#include <stdlib.h>           /* exit()                                  */
#include <unistd.h>           /* STDERR_FILENO                           */
#include "amgdefs.h"          /* prototype of gts2tiff()                 */

/* Global variables. */
int        receive_log_fd = STDERR_FILENO,
           sys_log_fd = STDERR_FILENO;
#ifndef HAVE_MMAP
char       *p_work_dir = NULL;
#endif
const char *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   char *ptr,
        path[MAX_PATH_LENGTH],
        filename[MAX_FILENAME_LENGTH];

   if (argc == 2)
   {
      (void)my_strncpy(path, argv[1], MAX_PATH_LENGTH);
      ptr = path + strlen(path);
      while (path != ptr)
      {
         if (*(ptr--) == '/')
         {
            break;
         }
      }
      if (*ptr == '/')
      {
         (void)strcpy(filename, ptr + 1);
         *ptr = '\0';
      }
      else
      {
         (void)strcpy(filename, path);
         path[0] = '\0';
      }
   }
   else
   {
      (void)fprintf(stderr, _("Usage: %s <T4 coded file>\n"), argv[0]);
      exit(INCORRECT);
   }

   if (gts2tiff(path, filename) < 0)
   {
      (void)fprintf(stderr, _("ERROR   : Conversion failed.\n"));
      exit(INCORRECT);
   }

   exit(SUCCESS);
}
