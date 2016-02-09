/*
 *  output_log_fd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   output_log_fd - initialises filedescripter for output log
 **
 ** SYNOPSIS
 **   void output_log_fd(int            *ol_fd)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   When successful it opens the fifo to the output log.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.09.2008 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "fddefs.h"

/* External global variables. */
extern char       *p_work_dir;
extern struct job db;


/*############################ output_log_fd() ##########################*/
void
#ifdef WITHOUT_FIFO_RW_SUPPORT
output_log_fd(int *ol_fd, int *ol_readfd)
#else
output_log_fd(int *ol_fd)
#endif
{
   char output_log_fifo[MAX_PATH_LENGTH];

   (void)strcpy(output_log_fifo, p_work_dir);
   (void)strcat(output_log_fifo, FIFO_DIR);
   (void)strcat(output_log_fifo, OUTPUT_LOG_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(output_log_fifo, ol_readfd, ol_fd) == -1)
#else
   if ((*ol_fd = coe_open(output_log_fifo, O_RDWR)) == -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s",
                 output_log_fifo, strerror(errno));
      db.output_log = NO;
   }

   return;
}
