/*
 *  reset_fsa.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   reset_fsa - 
 **
 ** SYNOPSIS
 **   reset_fsa [-w <working directory>] hostname|position
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
 **   02.11.1997 H.Kiehl Created
 **   11.06.2002 H.Kiehl Reset some more values in struct status.
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr            */
#include <string.h>                      /* strcpy(), strerror()         */
#include <stdlib.h>                      /* atoi()                       */
#include <ctype.h>                       /* isdigit()                    */
#include <sys/types.h>
#include <unistd.h>                      /* STDERR_FILENO                */
#include <errno.h>
#include "version.h"

/* Global variables .*/
int                        sys_log_fd = STDERR_FILENO,   /* Not used!    */
                           fsa_fd = -1,
                           fsa_id,
                           no_of_hosts = 0;
#ifdef HAVE_MMAP
off_t                      fsa_size;
#endif
char                       *p_work_dir;
struct filetransfer_status *fsa;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void                usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ reset_fsa() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int  i,
        position = -1;
   char hostname[MAX_HOSTNAME_LENGTH + 1],
        work_dir[MAX_PATH_LENGTH];

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if (argc == 2)
   {
      if (isdigit((int)(argv[1][0])) != 0)
      {
         position = atoi(argv[1]);
      }
      else
      {
         t_hostname(argv[1], hostname);
      }
   }
   else
   {
      usage(argv[0]);
      exit(INCORRECT);
   }

   if ((i = fsa_attach("reset_fsa")) != SUCCESS)
   {
      if (i == INCORRECT_VERSION)
      {
         (void)fprintf(stderr,
                       _("ERROR   : This program is not able to attach to the FSA due to incorrect version. (%s %d)\n"),
                       __FILE__, __LINE__);
      }
      else
      {
         if (i < 0)
         {
            (void)fprintf(stderr,
                          _("ERROR   : Failed to attach to FSA. (%s %d)\n"),
                          __FILE__, __LINE__);
         }
         else
         {
            (void)fprintf(stderr,
                          _("ERROR   : Failed to attach to FSA : %s (%s %d)\n"),
                          strerror(i), __FILE__, __LINE__);
         }
      }
      exit(INCORRECT);
   }

   if (position < 0)
   {
      if ((position = get_host_position(fsa, hostname, no_of_hosts)) < 0)
      {
         (void)fprintf(stderr,
                       _("ERROR   : Could not find host %s in FSA. (%s %d)\n"),
                       hostname, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   fsa[position].total_file_counter = 0;
   fsa[position].total_file_size = 0;
   fsa[position].host_status = 0;
   fsa[position].debug = 0;
   for (i = 0; i < fsa[position].allowed_transfers; i++)
   {
      fsa[position].job_status[i].connect_status = DISCONNECT;
      fsa[position].job_status[i].no_of_files = 0;
      fsa[position].job_status[i].no_of_files_done = 0;
      fsa[position].job_status[i].file_size = 0L;
      fsa[position].job_status[i].file_size_done = 0L;
      fsa[position].job_status[i].file_size_in_use = 0L;
      fsa[position].job_status[i].file_size_in_use_done = 0L;
      fsa[position].job_status[i].file_name_in_use[0] = '\0';
#ifdef _WITH_BURST_2
      fsa[position].job_status[i].unique_name[0] = '\0';
#endif
   }

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{                    
   (void)fprintf(stderr,
                 _("SYNTAX  : %s [-w working directory] hostname|position\n"),
                 progname);
   return;                                                                 
}
