/*
 *  eval_input_ss.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2020 Deutscher Wetterdienst (DWD),
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
 **   eval_input_ss - checks syntax of input for process show_stat
 **
 ** SYNOPSIS
 **   void eval_input_ss(int  argc,
 **                      char *argv[],
 **                      char *status_file_name,
 **                      int  *show_day,
 **                      int  *show_day_summary,
 **                      int  *show_hour,
 **                      int  *show_hour_summary,
 **                      int  *show_min_range,
 **                      int  *show_min,
 **                      int  *show_min_summary,
 **                      int  *show_year,
 **                      int  *arg_counter,
 **                      int  *show_time_stamp,
 **                      int  *display_format,
 **                      int  input,
 **                      int  *options)
 **
 ** DESCRIPTION
 **   This module checks whether the syntax is correct. So far it
 **   accepts the following parameters:
 **           -w <work dir>       working directory of the AFD
 **           -f <statistic file> path and name of the statistic file
 **           -o <file name>      Output file name.
 **           -d [<x>]            show all information of day minus x
 **           -D                  show total summary on a per day basis
 **           -h [<x>]            show all information of hour minus x
 **           -H                  show total summary of last 24 hours
 **           -mr <x>             show last x minutes
 **           -m [<x>]            show all information of minute minus x
 **           -M [<x>]            show total summary of last hour
 **           -y <x>              show all information of year minus x
 **           -t[u]               put in a time stamp for time the output
 **                               is valid. With the u the time is shown
 **                               in unix time.
 **           -C                  Output in CSV format.
 **           -N                  Show directory name not alias.
 **           -n                  Show alias and directory name.
 **           -R                  Only show remote dirs.
 **           -T                  Numeric total only.
 **
 ** RETURN VALUES
 **   If any of the above parameters have been specified it returns
 **   them in the appropriate variable (status_file_name, show_day,
 **   show_year, arg_counter).
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   19.09.1996 H.Kiehl Created
 **   19.04.1998 H.Kiehl Added -D option.
 **   26.05.1998 H.Kiehl Added -H option.
 **   09.08.2002 H.Kiehl Added -M option.
 **   16.02.2003 H.Kiehl Added -m and -mr options.
 **   19.02.2003 H.Kiehl Added -t[u] option.
 **   23.07.2006 H.Kiehl Added -T option.
 **   30.04.2018 H.Kiehl Added -C, -N and -o option.
 **   05.11.2018 H.Kiehl Added -n option.
 **   04.07.2019 H.Kiehl Improve detection of alias name or number.
 **   02.02.2020 H.Kiehl Added -rd option.
 **
 */
DESCR__E_M3

#include <stdio.h>                        /* stderr, fprintf()           */
#include <stdlib.h>                       /* exit(), atoi()              */
#include <string.h>                       /* strcpy()                    */
#include <ctype.h>                        /* isdigit()                   */
#include <unistd.h>                       /* STDERR_FILENO               */
#include <errno.h>                        /* RT_ARRAY                    */
#include "statdefs.h"

/* Global variables */
extern char **arglist;
extern int  sys_log_fd;                   /* Used by macro RT_ARRAY      */

/* local functions */
static void usage(int);


/*########################## eval_input_ss() ############################*/
void
eval_input_ss(int  argc,
              char *argv[],
              char *status_file_name,
              char *output_file_name,
              int  *show_day,
              int  *show_day_summary,
              int  *show_hour,
              int  *show_hour_summary,
              int  *show_min_range,
              int  *show_min,
              int  *show_min_summary,
              int  *show_year,
              int  *arg_counter,
              int  *show_time_stamp,
              int  *display_format,
              int  *show_alias,
              int  input,
              int  *options)
{
   int correct = YES;       /* Was input/syntax correct?      */

   /**********************************************************/
   /* The following while loop checks for the parameters:    */
   /*                                                        */
   /*         - f : Path and name of the statistic file.     */
   /*         - d : Show all information of day minus x.     */
   /*         - D : Show total summary on a per day basis.   */
   /*         - h : Show all information of hour minus x.    */
   /*         - H : Show total summary of last 24 hours.     */
   /*         - mr: Show information of last x minutes.      */
   /*         - m : Show all information of minute minus x.  */
   /*         - M : Show total summary of last hour.         */
   /*         - t : Put in a timestamp.                      */
   /*         - y : Show all information of year minus x.    */
   /*                                                        */
   /**********************************************************/
   output_file_name[0] = '\0';
   if ((argc > 1) && (argv[1][0] == '-'))
   {
      while ((--argc > 0) && ((*++argv)[0] == '-'))
      {
         switch (*(argv[0] + 1))
         {
            case 'f': /* Name of the statistic file. */

               if ((argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  (void)fprintf(stderr,
                                "ERROR  : You did not specify the name of the statistics file.\n");
                  correct = NO;
               }
               else
               {
                  (void)my_strncpy(status_file_name, argv[1],
                                   MAX_FILENAME_LENGTH + 1);
                  argv++;
                  argc--;
               }
               break;

            case 'o': /* Output file name. */

               if ((argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  (void)fprintf(stderr,
                                "ERROR  : You did not specify the name of the output file.\n");
                  correct = NO;
               }
               else
               {
                  (void)my_strncpy(output_file_name, argv[0],
                                   MAX_PATH_LENGTH + 1);
                  argv--;
                  argc++;
               }
               break;

            case 'd': /* Show day minus x */

               if ((argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  *show_day = 0;
               }
               else
               {
                  int i = 0;

                  while (isdigit((int)((*(argv + 1))[i])))
                  {
                     i++;
                  }
                  if ((*(argv + 1))[i] == '\0')
                  {
                     *show_day = atoi(*(argv + 1));
                     argc--;
                     argv++;
                  }
                  else
                  {
                     *show_day = 0;
                  }
               }
               break;

            case 'D': /* Show summary information on a per day basis. */

               if ((argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  *show_day_summary = 0;
               }
               else
               {
                  (void)fprintf(stderr,
                                "ERROR  : Can only show summary on a per day basis.\n");
                  correct = NO;
                  argc--;
                  argv++;
               }
               break;

            case 'h': /* Show hour minus x */

               if ((argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  *show_hour = 0;
               }
               else
               {
                  int i = 0;

                  while (isdigit((int)((*(argv + 1))[i])))
                  {
                     i++;
                  }
                  if ((*(argv + 1))[i] == '\0')
                  {
                     *show_hour = atoi(*(argv + 1));
                     argc--;
                     argv++;
                  }
                  else
                  {
                     *show_hour = 0;
                  }
               }
               break;

            case 'H': /* Show summary information of last 24 hours. */

               if ((argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  *show_hour_summary = 0;
               }
               else
               {
                  (void)fprintf(stderr,
                                "ERROR  : Can only show summary of last 24 hours.\n");
                  correct = NO;
                  argc--;
                  argv++;
               }
               break;

            case 'm':

               if ((*(argv[0] + 2) == 'r') && (*(argv[0] + 3) == '\0'))
               {
                  /* Show last x minutes. */
                  if ((argc == 1) || (*(argv + 1)[0] == '-'))
                  {
                     *show_min_range = 0;
                  }
                  else
                  {
                     int i = 0;

                     while (isdigit((int)((*(argv + 1))[i])))
                     {
                        i++;
                     }
                     if ((*(argv + 1))[i] == '\0')
                     {
                        *show_min_range = atoi(*(argv + 1));
                        if (*show_min_range > 60)
                        {
                           (void)fprintf(stderr,
                                         "WARN   : Setting to 60, value given %d is to high. (%s %d)\n",
                                         *show_min_range, __FILE__, __LINE__);
                           *show_min_range = 60;
                        }
                        argc--;
                        argv++;
                     }
                     else
                     {
                        *show_min_range = 0;
                     }
                  }
               }
               else /* Show hour minus x */
               {
                  if ((argc == 1) || (*(argv + 1)[0] == '-'))
                  {
                     *show_min = 0;
                  }
                  else
                  {
                     int i = 0;

                     while (isdigit((int)((*(argv + 1))[i])))
                     {
                        i++;
                     }
                     if ((*(argv + 1))[i] == '\0')
                     {
                        *show_min = atoi(*(argv + 1));
                        if (*show_min > 60)
                        {
                           (void)fprintf(stderr,
                                         "WARN   : Setting to 60, value given %d is to high. (%s %d)\n",
                                         *show_min, __FILE__, __LINE__);
                           *show_min = 60;
                        }
                        argc--;
                        argv++;
                     }
                     else
                     {
                        *show_min = 0;
                     }
                  }
               }
               break;

            case 'M': /* Show summary information of last hour. */

               if ((argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  *show_min_summary = 0;
               }
               else
               {
                  int i = 0;

                  while (isdigit((int)((*(argv + 1))[i])))
                  {
                     i++;
                  }
                  if ((*(argv + 1))[i] == '\0')
                  {
                     *show_min_summary = atoi(*(argv + 1));
                     if (*show_min_summary > 60)
                     {
                        (void)fprintf(stderr,
                                      "WARN   : Setting to 60, value given %d is to high. (%s %d)\n",
                                      *show_min_summary, __FILE__, __LINE__);
                        *show_min_summary = 60;
                     }
                  }
                  else
                  {
                     *show_min_summary = 0;
                  }
               }
               argc--;
               argv++;
               break;

            case 't': /* Put in timestamps for which time the output is valid */

               if ((*(argv[0] + 2) == 'u') && (*(argv[0] + 3) == '\0'))
               {
                  *show_time_stamp = 2;
               }
               else
               {
                  *show_time_stamp = 1;
               }
               break;

            case 'T': /* Show numeric total only. */

               *display_format = NUMERIC_TOTAL_ONLY;
               break;

            case 'C': /* Show CSV format. */

               *display_format = CSV_FORMAT;
               break;

            case 'N': /* Show directory name. */

               *show_alias = NO;
               break;

            case 'n': /* Show alias and directory name. */

               *show_alias = BOTH;
               break;

            case 'y': /* Show year minus x */

               if ((argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  *show_year = 0;
               }
               else
               {
                  int i = 0;

                  while (isdigit((int)((*(argv + 1))[i])))
                  {
                     i++;
                  }
                  if ((*(argv + 1))[i] == '\0')
                  {
                     *show_year = atoi(*(argv + 1));
                     argc--;
                     argv++;
                  }
                  else
                  {
                     *show_year = 0;
                  }
               }
               break;

            case 'R' : /* Only show remote dirs. */

               *options |= ONLY_SHOW_REMOTE_DIRS;
               break;

            default : /* Unknown parameter */

               (void)fprintf(stderr,
                             "ERROR  : Unknown parameter %c. (%s %d)\n",
                             *(argv[0] + 1), __FILE__, __LINE__);
               correct = NO;
               break;

         } /* switch (*(argv[0] + 1)) */
      }
   }
   else
   {
      argc--; argv++;
   }

   /*
    * Now lets collect all the host/dir names and store them somewhere
    * save and snug ;-)
    */
   if (argc > 0)
   {
      int i;

      *arg_counter = argc;
      RT_ARRAY(arglist, argc, MAX_FILENAME_LENGTH, char);
      for (i = 0; i < argc; i++)
      {
         (void)my_strncpy(arglist[i], argv[0], MAX_FILENAME_LENGTH);
         argv++;
      }
   }

   /* If input is not correct show syntax */
   if (correct == NO)
   {
      usage(input);
      exit(0);
   }

   return;
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(int input)
{
   if (input == YES)
   {
      (void)fprintf(stderr, "SYNTAX  : show_istat [options] [dir 1 ....]\n");
   }
   else
   {
      (void)fprintf(stderr, "SYNTAX  : show_stat [options] [hostname 1 ....]\n");
   }
   (void)fprintf(stderr, "           -w <work dir> Working directory of the AFD.\n");
   (void)fprintf(stderr, "           -f <name>     Path and name of the statistics file.\n");
   (void)fprintf(stderr, "           -o <name>     Path and name of the output file.\n");
   (void)fprintf(stderr, "           -d [<x>]      Show information of all days [or day minus x].\n");
   (void)fprintf(stderr, "           -D            Show total summary on a per day basis.\n");
   (void)fprintf(stderr, "           -h [<x>]      Show information of all hours [or hour minus x].\n");
   (void)fprintf(stderr, "           -H            Show total summary of last 24 hours.\n");
   (void)fprintf(stderr, "           -mr <x>       Show summary of last x minutes.\n");
   (void)fprintf(stderr, "           -m [<x>]      Show information of all minutes [or minute minus x].\n");
   (void)fprintf(stderr, "           -M [<x>]      Show total summary of last hour.\n");
   (void)fprintf(stderr, "           -t[u]         Put in a timestamp for when this output is valid.\n");
   (void)fprintf(stderr, "           -C            Format output in CSV format.\n");
   if (input == YES)
   {
      (void)fprintf(stderr, "           -N            Show directory name not alias.\n");
      (void)fprintf(stderr, "           -n            Show alias and directory name.\n");
      (void)fprintf(stderr, "           -R            Only show remote dirs.\n");
   }
   (void)fprintf(stderr, "           -T            Show numeric total only.\n");
   (void)fprintf(stderr, "           -y [<x>]      Show information of all years [or year minus x].\n");
   (void)fprintf(stderr, "           --version     Show current version.\n");

   return;
}
