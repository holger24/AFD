/*
 *  get_hostname.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2004 - 2014 Deutscher Wetterdienst (DWD),
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
 **   get_hostname - shows the current real hostname
 **
 ** SYNOPSIS
 **   get_hostname [--version] [-w working directory] hostalias
 **
 ** DESCRIPTION
 **   This program shows the current real hostname.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.11.2004 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr, stdout    */
#include <string.h>                      /* strcpy(), strerror()         */
#include <stdlib.h>                      /* atoi()                       */
#include <ctype.h>                       /* isdigit()                    */
#include <time.h>                        /* ctime()                      */
#include <sys/types.h>
#include <unistd.h>                      /* STDERR_FILENO                */
#include <errno.h>
#include "version.h"

/* Local functions. */
static void usage(void);

int                        sys_log_fd = STDERR_FILENO,   /* Not used!    */
                           fsa_id,
                           fsa_fd = -1,
                           no_of_hosts = 0;
#ifdef HAVE_MMAP
off_t                      fsa_size;
#endif
char                       *p_work_dir;
struct filetransfer_status *fsa;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$ get_hostname() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int  position = -1,
        ret;
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
      t_hostname(argv[1], hostname);
   }
   else
   {
      usage();
      exit(INCORRECT);
   }

   if ((ret = fsa_attach_passive(NO, "get_hostname")) != SUCCESS)
   {
      if (ret == INCORRECT_VERSION)
      {
         (void)fprintf(stderr,
                       _("ERROR   : This program is not able to attach to the FSA due to incorrect version. (%s %d)\n"),
                       __FILE__, __LINE__);
      }
      else
      {
         if (ret < 0)
         {
            (void)fprintf(stderr,
                          _("ERROR   : Failed to attach to FSA. (%s %d)\n"),
                          __FILE__, __LINE__);
         }
         else
         {
            (void)fprintf(stderr,
                          _("ERROR   : Failed to attach to FSA : %s (%s %d)\n"),
                          strerror(ret), __FILE__, __LINE__);
         }
      }
      exit(INCORRECT);
   }

   if ((position = get_host_position(fsa, hostname, no_of_hosts)) < 0)
   {
      (void)fprintf(stderr,
                    _("ERROR   : Could not find host `%s' in FSA. (%s %d)\n"),
                    hostname, __FILE__, __LINE__);
      exit(INCORRECT);
   }

   (void)fprintf(stdout, "%s\n",
                 fsa[position].real_hostname[(int)fsa[position].host_toggle - 1]);

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/ 
static void
usage(void)
{
   (void)fprintf(stderr,
                 _("SYNTAX  : get_hostname [--version] [-w working directory] hostalias\n"));
   return;
}
