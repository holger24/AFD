/*
 *  eval_input_as.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2012 Deutscher Wetterdienst (DWD),
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
 **   eval_input_as - checks syntax of input for process afd_stat
 **
 ** SYNOPSIS
 **   void eval_input_as(int  argc,
 **                      char *argv[],
 **                      char *work_dir,
 **                      char *statistic_file_name,
 **                      char *istatistic_file_name)
 **
 ** DESCRIPTION
 **   This module checks whether the syntax is correct. So far it
 **   accepts the following parameters:
 **    -w <working directory>                   working directory of the AFD
 **    -f <input stat file> <output stat file>  path and name of the statistic
 **                                             files
 **
 ** RETURN VALUES
 **   If any of the above parameters have been specified it returns
 **   them in the appropriate variable (work_dir, statistic_file_name,
 **   istatistic_file_name).
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.10.1996 H.Kiehl Created
 **   23.02.2003 H.Kiehl Added input statistics.
 **
 */
DESCR__E_M3

#include <stdio.h>                        /* stderr, fprintf()           */
#include <stdlib.h>                       /* exit(), atoi()              */
#include <string.h>                       /* strcpy()                    */
#include "statdefs.h"

/* Global variables */
extern int  other_file;

/* local functions */
static void usage(void);


/*########################## eval_input_as() ############################*/
void
eval_input_as(int  argc,
              char *argv[],
              char *work_dir,
              char *statistic_file_name,
              char *istatistic_file_name)
{
   int correct = YES;       /* Was input/syntax correct?      */

   /**********************************************************/
   /* The following while loop checks for the parameters:    */
   /*                                                        */
   /*         - w : Working directory of the AFD.            */
   /*         - f : Path and name of the input and output    */
   /*               statistic file names.                    */
   /*                                                        */
   /**********************************************************/
   while ((--argc > 0) && ((*++argv)[0] == '-'))
   {
      switch (*(argv[0] + 1))
      {
         case 'f': /* Name of the statistic file */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr,
                             "ERROR  : You did not specify the name of the input and output statistics files.\n");
               correct = NO;
            }
            else if ((argc == 2) || (*(argv + 2)[0] == '-'))
                 {
                    (void)fprintf(stderr,
                                  "ERROR  : You did not specify the name of the output statistics file.\n");
                    correct = NO;
                 }
                 else
                 {
                    (void)my_strncpy(istatistic_file_name, argv[1],
                                     MAX_FILENAME_LENGTH + 1);
                    (void)my_strncpy(statistic_file_name, argv[2],
                                     MAX_FILENAME_LENGTH + 1);
                    other_file = YES;
                    argv++;
                    argv++;
                    argc--;
                    argc--;
                 }
            break;

         case 'w': /* Working directory of the AFD */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr,
                             "ERROR  : You did not specify the working directory of the AFD.\n");
               correct = NO;
            }
            else
            {
               while ((--argc > 0) && ((*++argv)[0] != '-'))
               {
                  (void)my_strncpy(work_dir, argv[0], MAX_PATH_LENGTH);
               }
               argv--;
               argc++;
            }
            break;

         default : /* Unknown parameter */

            (void)fprintf(stderr, "ERROR  : Unknown parameter %c. (%s %d)\n",
                          *(argv[0] + 1), __FILE__, __LINE__);
            correct = NO;
            break;

      } /* switch (*(argv[0] + 1)) */
   }

   /* If input is not correct show syntax */
   if (correct == NO)
   {
      usage();
      exit(0);
   }

   return;
} /* eval_input_stat() */


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(void)
{
   (void)fprintf(stderr,
                 "SYNTAX  : afd_stat [options]\n");
   (void)fprintf(stderr,
                 "            -w <work dir>                          Working directory of the AFD.\n");
   (void)fprintf(stderr,
                 "            -f <input filename> <output filename>  Path and name of the statistics files.\n");
   (void)fprintf(stderr,
                 "            --version                              Show current version.\n");

   return;
}
