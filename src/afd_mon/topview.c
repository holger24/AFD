/*
 *  topview.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2015 Deutscher Wetterdienst (DWD),
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
 **   topview - shows the top file rate or transfer rate
 **
 ** SYNOPSIS
 **   topview [-w <working directory>] [-f]|[-t][-p] [afd1 ... afdn]
 **
 ** DESCRIPTION
 **   This program shows the top file rate (-f), the top transfer rate
 **   (-t), or the top number of process (-p) of all AFD's in the MSA.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.02.2000 H.Kiehl Created
 **   13.09.2000 H.Kiehl Addition of top number of transfers.
 **   26.09.2013 H.Kiehl Show more accurate transfer rate numbers.
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
#include "mondefs.h"
#include "version.h"


int                    sys_log_fd = STDERR_FILENO,   /* Not used!    */
                       msa_id,
                       msa_fd = -1,
                       no_of_afds = 0;
off_t                  msa_size;
char                   *p_work_dir;
struct mon_status_area *msa;
const char             *sys_log_name = MON_SYS_LOG_FIFO;

/* Local functions. */
static void            print_data(int, int),
                       usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ msa_view() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int  i, j,
        show_afds = 0,
        show = 0;
   char work_dir[MAX_PATH_LENGTH];

   CHECK_FOR_VERSION(argc, argv);

   if (get_mon_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if (argc > 1)
   {
      if (argv[1][0] == '-')
      {
         if ((argv[1][1] == 'f') && (argv[1][2] == '\0'))
         {
            show = 0;
            argc--; argv++;
         }
         else if ((argv[1][1] == 't') && (argv[1][2] == '\0'))
              {
                 show = 1;
                 argc--; argv++;
              }
         else if ((argv[1][1] == 'p') && (argv[1][2] == '\0'))
              {
                 show = 2;
                 argc--; argv++;
              }
              else
              {
                 usage(argv[0]);
              }
      }
      if (argc > 1)
      {
         show_afds = argc - 1;
      }
   }

   if ((j = msa_attach_passive()) < 0)
   {
      if (j == INCORRECT_VERSION)
      {
         (void)fprintf(stderr, "ERROR   : This program is not able to attach to the MSA due to incorrect version. (%s %d)\n",
                       __FILE__, __LINE__);
      }
      else
      {
         (void)fprintf(stderr, "ERROR   : Failed to attach to MSA. (%s %d)\n",
                       __FILE__, __LINE__);
      }
      exit(INCORRECT);
   }

   if (show == 0)
   {
      (void)fprintf(stdout, "TOP file rates (per second) for %d AFD's:\n\n",
                    no_of_afds);
   }
   else if (show == 1)
        {
           (void)fprintf(stdout,
                         "TOP transfer rates (per second) for %d AFD's:\n\n",
                         no_of_afds);
        }
        else
        {
           (void)fprintf(stdout,
                         "TOP number of transfers for %d AFD's:\n\n",
                         no_of_afds);
        }
   (void)fprintf(stdout, "%-*s  ", MAX_AFDNAME_LENGTH, "AFD-name");
   if (show == 1)
   {
      (void)fprintf(stdout, "     0");
      for (j = 1; j < STORAGE_TIME; j++)
      {
         (void)fprintf(stdout, " %6d", j);
      }
      (void)fprintf(stdout, "\n==============================================================\n");
   }
   else
   {
      for (j = 0; j < STORAGE_TIME; j++)
      {
         (void)fprintf(stdout, " %4d", j);
      }
      (void)fprintf(stdout, "\n=================================================\n");
   }

   if (show_afds > 0)
   {
      for (i = 0; i < show_afds; i++)
      {
         for (j = 0; j < no_of_afds; j++)
         {
            if (my_strcmp(argv[1 + i], msa[j].afd_alias) == 0)
            {
               print_data(j, show);
               break;
            }
         }
      }
   }
   else
   {
      for (i = 0; i < no_of_afds; i++)
      {
         print_data(i, show);
      }
   }

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++ print_data() +++++++++++++++++++++++++++++++*/
static void
print_data(int pos, int show)
{
   int j;

   (void)fprintf(stdout, "%-*s :", MAX_AFDNAME_LENGTH, msa[pos].afd_alias);
   if (show == 0)
   {
      for (j = 0; j < STORAGE_TIME; j++)
      {
         (void)fprintf(stdout, " %4u", msa[pos].top_fr[j]);
      }
   }
   else if (show == 1)
        {
           char size_str[7];

           for (j = 0; j < STORAGE_TIME; j++)
           {
              PRINT_SIZE_STR(msa[pos].top_tr[j], size_str);
              (void)fprintf(stdout, " %s", size_str);
           }
        }
        else
        {
           for (j = 0; j < STORAGE_TIME; j++)
           {
              (void)fprintf(stdout, " %4d", msa[pos].top_no_of_transfers[j]);
           }
        }
   (void)fprintf(stdout, "\n");

   return;
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/ 
static void
usage(char *progname)
{
   (void)fprintf(stderr,
                 "Usage: %s [-w <working directory>] [-f]|[-t][-p] [afd1 ... afdn]\n",
                 progname);
   exit(INCORRECT);
}
