/*
 *  com.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2023 Deutscher Wetterdienst (DWD),
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
 **   com - sends a command to the dir_check or fr command fifo
 **
 ** SYNOPSIS
 **   int com(char action, char *file, int line)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS when transmission of command <action> was successful,
 **   otherwise INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.08.1995 H.Kiehl Created
 **   26.03.1998 H.Kiehl Put this function into a separate file.
 **   27.09.2017 H.Kiehl Allow other process to send a BUSY_WORKING,
 **                      meaning we will not timeout and instead
 **                      keep waiting.
 **   08.12.2023 H.Kiehl Show file and line of calling process if
 **                      there is an error.
 **
 */
DESCR__E_M3

#include <string.h>
#include <stdlib.h>                /* exit()                              */
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>                /* read(), write(), close()            */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "amgdefs.h"


/* External global variables. */
extern char *p_work_dir;


/*################################ com() ################################*/
int
com(char action, char *file, int line)
{
   int            write_fd,
                  read_fd,
                  ret;
   char           buffer[10],
                  com_fifo[MAX_PATH_LENGTH],
                  *ptr;
   fd_set         rset;
   struct timeval timeout;

   /* Build the fifo names. */
   (void)strcpy(com_fifo, p_work_dir);
   (void)strcat(com_fifo, FIFO_DIR);
   ptr = com_fifo + strlen(com_fifo);

   /* Open fifo to send command to job. */
   (void)strcpy(ptr, DC_CMD_FIFO);
   if ((write_fd = open(com_fifo, O_RDWR)) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s [%s %d]",
                 com_fifo, strerror(errno), file, line);
      exit(INCORRECT);
   }

   /* Open fifo to wait for answer from job. */
   (void)strcpy(ptr, DC_RESP_FIFO);
   if ((read_fd = open(com_fifo, (O_RDONLY | O_NONBLOCK))) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s [%s %d]",
                 com_fifo, strerror(errno), file, line);
      exit(INCORRECT);
   }

   /* Write command to command fifo. */
   if (write(write_fd, &action, 1) != 1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not write to fifo %s : %s [%s %d]",
                 DC_CMD_FIFO, strerror(errno), file, line);
      exit(INCORRECT);
   }

   /* Initialise descriptor set and timeout. */
   FD_ZERO(&rset);
   for (;;)
   {
      FD_SET(read_fd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = JOB_TIMEOUT;

      /* Wait for message x seconds and then continue. */
      ret = select((read_fd + 1), &rset, NULL, NULL, &timeout);

      if ((ret > 0) && (FD_ISSET(read_fd, &rset)))
      {
         if ((ret = read(read_fd, buffer, 10)) > 0)
         {
            if (buffer[ret - 1] == BUSY_WORKING)
            {
#ifdef _MAINTAINER_LOG
               maintainer_log(DEBUG_SIGN, NULL, 0,
                              "com() received BUSY_WORKING.");
#endif
               continue;
            }
            if (buffer[ret - 1] != ACKN)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Received garbage (%d) while reading from fifo. [%s %d]",
                          buffer[ret - 1], file, line);
            }
            ret = SUCCESS;
         }
         else
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Read problems (%d) : %s [%s %d]",
                       ret, strerror(errno), file, line);
            ret = INCORRECT;
         }
         break;
      }
      else if (ret < 0)
           {
              system_log(FATAL_SIGN, __FILE__, __LINE__,
                         "select() error : %s [%s %d]",
                         strerror(errno), file, line);
              exit(INCORRECT);
           }
      else if (ret == 0)
           {
              /* The other side does not answer. */
              system_log(WARN_SIGN, __FILE__, __LINE__,
                         "Did not receive any reply from %s for the command %s (%d). [%s %d]",
                         DC_PROC_NAME, get_com_action_str(action), action,
                         file, line);
              ret = INCORRECT;
              break;
           }
           else /* Impossible! Unknown error. */
           {
              system_log(FATAL_SIGN, __FILE__, __LINE__,
                         "Ouch! What now? [%s %d] @!$(%.", file, line);
              exit(INCORRECT);
           }
   }

   if ((close(write_fd) == -1) || (close(read_fd) == -1))
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s [%s %d]",
                 strerror(errno), file, line);
   }

   return(ret);
}
