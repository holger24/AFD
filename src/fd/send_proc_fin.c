/*
 *  send_proc_fin.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008, 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   send_proc_fin - sends FD a signal via fifo that process terminates
 **
 ** SYNOPSIS
 **   void send_proc_fin(int more_data)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.04.2008 H.Kiehl Created
 **   17.06.2009 H.Kiehl Added parameter to ask for more data.
 **
 */
DESCR__E_M3

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "fddefs.h"

/* External global variables. */
extern char       *p_work_dir;
extern struct job db;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$ send_proc_fin() $$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
void
send_proc_fin(int more_data)
{
   if (p_work_dir != NULL)
   {
      int  fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
      int  readfd;
#endif
      char sf_fin_fifo[MAX_PATH_LENGTH];

      (void)strcpy(sf_fin_fifo, p_work_dir);
      (void)strcat(sf_fin_fifo, FIFO_DIR);
      (void)strcat(sf_fin_fifo, SF_FIN_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(sf_fin_fifo, &readfd, &fd) == -1)
#else
      if ((fd = open(sf_fin_fifo, O_RDWR)) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not open fifo `%s' : %s",
                    sf_fin_fifo, strerror(errno));
      }
      else
      {
         pid_t pid;
#ifdef _FIFO_DEBUG
         char  cmd[2];

         /* Tell FD we are finished. */
         cmd[0] = ACKN; cmd[1] = '\0';
         show_fifo_data('W', "sf_fin", cmd, 1, __FILE__, __LINE__);
#endif
         if (more_data == NO)
         {
            pid = db.my_pid;
         }
         else
         {
            pid = -db.my_pid;
         }
         if (write(fd, &pid, sizeof(pid_t)) != sizeof(pid_t))
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "write() error : %s", strerror(errno));
         }
#ifdef WITHOUT_FIFO_RW_SUPPORT
         (void)close(readfd);
#endif
         (void)close(fd);
      }
   }

   return;
}
