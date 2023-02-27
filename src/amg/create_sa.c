/*
 *  create_sa.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   create_sa - creates the FSA and FRA of the AFD
 **
 ** SYNOPSIS
 **   void create_sa(int no_of_dirs)
 **
 ** DESCRIPTION
 **   This function creates the FSA (Filetransfer Status Area) and
 **   FRA (File Retrieve Area). See the functions create_fsa() and
 **   create_fra() for more details.

 ** RETURN VALUES
 **   None. Will exit with incorrect if any of the system call will
 **   fail.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.03.2000 H.Kiehl Created
 **   25.02.2023 H.Kiehl Added recovery of afdcfg values.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                 /* strcpy(), strcat(), strerror()    */
#include <stdlib.h>                 /* exit()                            */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>                 /* close()                           */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "amgdefs.h"


/* External global variables. */
extern char *p_work_dir;

/* Global variables. */
int         first_time = YES;

/* Local function prototypes. */
static void afdcfg_recover_status(void);


/*############################# create_sa() #############################*/
void
create_sa(int no_of_dirs)
{
   create_fsa();
   create_fra(no_of_dirs);
   afdcfg_recover_status();

   /* If this is the first time that the FSA is */
   /* created, notify AFD that we are done.     */
   if (first_time == YES)
   {
      int          afd_cmd_fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
      int          afd_cmd_readfd;
#endif
      char         afd_cmd_fifo[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat  stat_buf;
#endif

      (void)strcpy(afd_cmd_fifo, p_work_dir);
      (void)strcat(afd_cmd_fifo, FIFO_DIR);
      (void)strcat(afd_cmd_fifo, AFD_CMD_FIFO);

      /*
       * Check if fifos have been created. If not create and open them.
       */
#ifdef HAVE_STATX
      if ((statx(0, afd_cmd_fifo, AT_STATX_SYNC_AS_STAT,
                 STATX_MODE, &stat_buf) == -1) ||
          (!S_ISFIFO(stat_buf.stx_mode)))
#else
      if ((stat(afd_cmd_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
#endif
      {
         if (make_fifo(afd_cmd_fifo) < 0)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Failed to create fifo %s.", afd_cmd_fifo);
            exit(INCORRECT);
         }
      }
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(afd_cmd_fifo, &afd_cmd_readfd, &afd_cmd_fd) == -1)
#else
      if ((afd_cmd_fd = open(afd_cmd_fifo, O_RDWR)) == -1)
#endif
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not open fifo %s : %s",
                    afd_cmd_fifo, strerror(errno));
         exit(INCORRECT);
      }
      if (send_cmd(AMG_READY, afd_cmd_fd) < 0)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Was not able to send AMG_READY to %s.", AFD);
      }
      first_time = NO;
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (close(afd_cmd_readfd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "close() error : %s", strerror(errno));
      }
#endif
      if (close(afd_cmd_fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "close() error : %s", strerror(errno));
      }
   }

   return;
}


/*+++++++++++++++++++++++ afdcfg_recover_status() +++++++++++++++++++++++*/
static void
afdcfg_recover_status(void)
{
   char afdcfg_recover_name[MAX_PATH_LENGTH];

   (void)snprintf(afdcfg_recover_name, MAX_PATH_LENGTH, "%s%s%s",
                  p_work_dir, FIFO_DIR, AFDCFG_RECOVER);
   if (access(afdcfg_recover_name, R_OK) == 0)
   {
      char exec_cmd[MAX_PATH_LENGTH + MAX_PATH_LENGTH];
      FILE *fp;

      /* Call 'afdcfg --recover_status' */
      (void)snprintf(exec_cmd, MAX_PATH_LENGTH + MAX_PATH_LENGTH,
                     "%s -w %s --recover_status %s 2>&1",
                     AFDCFG, p_work_dir, afdcfg_recover_name);
      if ((fp = popen(exec_cmd, "r")) == NULL)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to popen() `%s' : %s", exec_cmd, strerror(errno));
      }
      else
      {
         int status = 0;

         exec_cmd[0] = '\0';
         while (fgets(exec_cmd, MAX_PATH_LENGTH, fp) != NULL)
         {
            ;
         }
         if (exec_cmd[0] != '\0')
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "%s printed : `%s'", AFDCFG, exec_cmd);
            status = 1;
         }
         if (ferror(fp))
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "ferror() error : %s", strerror(errno));
            status |= 2;
         }
         if (pclose(fp) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Failed to pclose() : %s", strerror(errno));
         }
         if (status == 0)
         {
            system_log(INFO_SIGN, NULL, 0, "Recovered afdcfg values.");
         }
      }
   }

   return;
}
