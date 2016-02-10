/*
 *  demcd_log_fd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2015 Deutscher Wetterdienst (DWD),
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
 **   demcd_log_fd - initialises filedescripter for fifo to demcd process
 **
 ** SYNOPSIS
 **   void demcd_log_fd(int *demcd_fd)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   When successful it opens the fifo to the demcd process.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   04.09.2015 H.Kiehl Created
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
extern char *p_work_dir;


/*############################ demcd_log_fd() ###########################*/
void
#ifdef WITHOUT_FIFO_RW_SUPPORT
demcd_log_fd(int *demcd_fd, int *demcd_readfd)
#else
demcd_log_fd(int *demcd_fd)
#endif
{
   char demcd_fifo[MAX_PATH_LENGTH];

   (void)strcpy(demcd_fifo, p_work_dir);
   (void)strcat(demcd_fifo, FIFO_DIR);
   (void)strcat(demcd_fifo, DEMCD_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(demcd_fifo, demcd_readfd, demcd_fd) == -1)
#else
   if ((*demcd_fd = coe_open(demcd_fifo, O_RDWR)) == -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s",
                 demcd_fifo, strerror(errno));
   }

   return;
}
