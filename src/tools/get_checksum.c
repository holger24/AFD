/*
 *  get_checksum.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2009, 2010 Deutscher Wetterdienst (DWD),
 *                           Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_checksum - calculates CRC32 checksum of the given string
 **
 ** SYNOPSIS
 **   get_checksum <arbitary string>
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   On success returns the CRC32 checksum.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.12.2009 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>            /* fprintf()                               */
#include <unistd.h>           /* STDERR_FILENO                           */

/* Global variables. */
int        sys_log_fd = STDERR_FILENO;
char       *p_work_dir = NULL;
const char *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   if (argc != 2)
   {
      (void)fprintf(stderr, _("Usage: %s <arbitary string>]\n"), argv[0]);
      return(INCORRECT);
   }
   (void)fprintf(stdout, "CRC32=%x\n", get_str_checksum(argv[1]));

   return(SUCCESS);
}
