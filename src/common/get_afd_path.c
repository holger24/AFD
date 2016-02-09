/*
 *  get_afd_path.c - Part of AFD, an automatic file distribution program.
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
 **   get_afd_path - gets the working directory of the AFD
 **
 ** SYNOPSIS
 **   int get_afd_path(int *argc, char *argv[], char *work_dir)
 **
 ** DESCRIPTION
 **   The function get_afd_path() gets the working directory of
 **   the AFD. It does this by first looking if the working
 **   directory is not passed on with the '-w' parameter in
 **   'argv'. If this is not the case it tries to get the work-
 **   ing directory from the environment variable WD_ENV_NAME
 **   (AFD_WORK_DIR). It will check if the directory exists and
 **   if not it tries to create the directory.
 **   If the environment variable is not set it will return
 **   INCORRECT.
 **
 ** RETURN VALUES
 **   SUCCESS and the working directory of the AFD in 'work_dir',
 **   if it finds a valid directory. Otherwise INCORRECT is
 **   returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   19.02.1996 H.Kiehl Created
 **   13.01.1997 H.Kiehl When AFD_WORK_DIR environment is not set,
 **                      take the home directory + .afd.
 **   22.11.1997 H.Kiehl Only accept AFD_WORK_DIR environment variable
 **                      and the command line option -w.
 **   31.08.2002 H.Kiehl Fix buffer overflow vulnerably when storing
 **                      AFD_WORK_DIR environment variable for work_dir.
 **   04.02.2011 H.Kiehl Added ARGV_DEBUG to see how we are called.
 **
 */
DESCR__E_M3

#include <stdio.h>             /* fprintf(), NULL                        */
#include <string.h>            /* strcpy()                               */
#include <stdlib.h>            /* getenv()                               */
#include <unistd.h>            /* R_OK, X_OK                             */

#define ARGV_DEBUG


/*########################### get_afd_path() ############################*/
int
get_afd_path(int *argc, char *argv[], char *work_dir)
{
   /* See if directory is passed as argument. */
   if (get_arg(argc, argv, WORK_DIR_ID, work_dir, MAX_PATH_LENGTH) == INCORRECT)
   {
      char *ptr;

      /* Check if the environment variable is set. */
      if ((ptr = getenv(WD_ENV_NAME)) != NULL)
      {
         if (my_strncpy(work_dir, ptr, MAX_PATH_LENGTH) != SUCCESS)
         {
            (void)fprintf(stderr,
                          _("ERROR   : Buffer for storing working directory is to short!\n"));
            return(INCORRECT);
         }
      }
      else
      {
#ifdef ARGV_DEBUG
         int i;
#endif

         (void)fprintf(stderr,
                       _("ERROR   : Failed to determine AFD working directory!\n"));
         (void)fprintf(stderr,
                       _("          No option %s or environment variable %s set.\n"),
                       WORK_DIR_ID, WD_ENV_NAME);
#ifdef ARGV_DEBUG
         (void)fprintf(stderr, "DEBUG   : ");
         for (i = 0; i < *argc; i++)
         {
            (void)fprintf(stderr, "%s ", argv[i]);
         }
         (void)fprintf(stderr, "\n");
#endif
         return(INCORRECT);
      }
   }

   if (check_dir(work_dir, R_OK | X_OK) == SUCCESS)
   {
      return(SUCCESS);
   }
   (void)fprintf(stderr,
                 _("ERROR   : Failed to create AFD working directory %s.\n"),
                 work_dir);
   return(INCORRECT);
}
