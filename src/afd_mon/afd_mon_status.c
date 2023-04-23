/*
 *  afd_mon_status.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   afd_mon_status - shows status of AFD_MON by printing contents of
 **                    afd_mon_status structure
 **
 ** SYNOPSIS
 **   afd_mon_status [-w <working directory>]
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
 **   10.06.2005 H.Kiehl Created
 **   22.04.2023 H.Kiehl Added process aldad.
 **
 */

#include <stdio.h>
#include <string.h>                   /* strcmp() in CHECK_FOR_VERSION   */
#include <stdlib.h>                   /* exit()                          */
#include <time.h>                     /* ctime()                         */
#include <unistd.h>                   /* STDERR_FILENO                   */
#include "version.h"
#include "mondefs.h"

/* Global variables */
int                   sys_log_fd = STDERR_FILENO;
char                  *p_work_dir;
struct afd_mon_status *p_afd_mon_status;
const char            *sys_log_name = MON_SYS_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$ afd_mon_status() $$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int  i;
   char work_dir[MAX_PATH_LENGTH];

   CHECK_FOR_VERSION(argc, argv);

   if (get_mon_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   /* Attach to the AFD Status Area */
   if (attach_afd_mon_status() < 0)
   {
      (void)fprintf(stderr,
                    "ERROR   : Failed to map to AFD_MON status area. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   (void)fprintf(stdout, "AFD_MON              : %d\n",
                 (int)p_afd_mon_status->afd_mon);
   (void)fprintf(stdout, "Mon Sys Log          : %d\n",
                 (int)p_afd_mon_status->mon_sys_log);
   (void)fprintf(stdout, "Monitor Log          : %d\n",
                 (int)p_afd_mon_status->mon_log);
   (void)fprintf(stdout, "ALDA daemon          : %d\n",
                 (int)p_afd_mon_status->aldad);
   (void)fprintf(stdout, "Monsyslog indicator  : %u <",
                 p_afd_mon_status->mon_sys_log_ec);
   for (i = 0; i < LOG_FIFO_SIZE; i++)
   {
      switch (p_afd_mon_status->mon_sys_log_fifo[i])
      {
         case INFO_ID :
            (void)fprintf(stdout, " I");
            break;
         case ERROR_ID :
            (void)fprintf(stdout, " E");
            break;
         case WARNING_ID :
            (void)fprintf(stdout, " W");
            break;
         case CONFIG_ID :
            (void)fprintf(stdout, " C");
            break;
         case FAULTY_ID :
            (void)fprintf(stdout, " F");
            break;
         default :
            (void)fprintf(stdout, " ?");
            break;
      }
   }
   (void)fprintf(stdout, " >\n");
   (void)fprintf(stdout, "Monlog indicator     : %u <",
                 p_afd_mon_status->mon_log_ec);
   for (i = 0; i < LOG_FIFO_SIZE; i++)
   {
      switch (p_afd_mon_status->mon_log_fifo[i])
      {
         case INFO_ID :
            (void)fprintf(stdout, " I");
            break;
         case CONFIG_ID :
            (void)fprintf(stdout, " C");
            break;
         case ERROR_ID :
            (void)fprintf(stdout, " E");
            break;
         case WARNING_ID :
            (void)fprintf(stdout, " W");
            break;
         case FAULTY_ID :
            (void)fprintf(stdout, " F");
            break;
         default :
            (void)fprintf(stdout, " ?");
            break;
      }
   }
   (void)fprintf(stdout, " >\n");
   (void)fprintf(stdout, "AFD_MON start time   : %s",
                 ctime(&p_afd_mon_status->start_time));

   exit(SUCCESS);
}
