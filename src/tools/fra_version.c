/*
 *  fra_version.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 - 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   fra_version - manipulate version number of FRA
 **
 ** SYNOPSIS
 **   fra_version [--version] [-w <working directory>] [<version number>]
 **
 ** DESCRIPTION
 **   This program shows the version of FRA or when a version number
 **   is given, sets this version number.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   09.10.2005 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr            */
#include <string.h>                      /* strcpy(), strerror()         */
#include <stdlib.h>                      /* atoi()                       */
#include <ctype.h>                       /* isdigit()                    */
#include <time.h>                        /* ctime()                      */
#include <sys/types.h>
#include <unistd.h>                      /* STDERR_FILENO                */
#include <errno.h>
#include "version.h"

/* Global variables. */
int                        sys_log_fd = STDERR_FILENO,   /* Not used!    */
                           fra_id,
                           fra_fd = -1,
                           no_of_dirs = 0;
#ifdef HAVE_MMAP
off_t                      fra_size;
#endif
char                       *p_work_dir;
struct fileretrieve_status *fra;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void                usage(void);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$ fra_version() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int  ret,
        set_version;
   char *ptr,
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
         set_version = atoi(argv[1]);
      }
      else
      {
         usage();
         exit(INCORRECT);
      }
   }
   else
   {
      set_version = -1;
   }

   if ((ret = fra_attach()) != SUCCESS)
   {
      if (ret == INCORRECT_VERSION)
      {
         (void)fprintf(stderr,
                       _("ERROR   : This program is not able to attach to the FRA due to incorrect version. (%s %d)\n"),
                       __FILE__, __LINE__);
      }
      else
      {
         if (ret < 0)
         {
            (void)fprintf(stderr,
                          _("ERROR   : Failed to attach to FRA. (%s %d)\n"),
                          __FILE__, __LINE__);
         }
         else
         {
            (void)fprintf(stderr,
                          _("ERROR   : Failed to attach to FRA : %s (%s %d)\n"),
                          strerror(ret), __FILE__, __LINE__);
         }
      }
      exit(INCORRECT);
   }
   ptr = (char *)fra;
   ptr -= AFD_WORD_OFFSET;
   if (set_version > -1)
   {
      int current_version = (int)(*(ptr + SIZEOF_INT + 1 + 1 + 1));

      *(ptr + SIZEOF_INT + 1 + 1 + 1) = set_version;
      (void)fprintf(stdout, _("Changed FRA version number from %d to %d\n"),
                    current_version, set_version);
   }
   else
   {
      (void)fprintf(stdout, _("Current FRA version: %d\n"),
                    (int)(*(ptr + SIZEOF_INT + 1 + 1 + 1)));
   }
   (void)fra_detach();

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/ 
static void
usage(void)
{
   (void)fprintf(stderr,
                 _("SYNTAX  : fra_version [--version] [-w working directory] [<version number>]\n"));
   return;
}
