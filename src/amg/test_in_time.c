/*
 *  test_in_time.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2017 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   test_in_time -
 **
 ** SYNOPSIS
 **   test_in_time <crontab like time entry 1> [<crontab like time entry n>]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.05.2001 H.Kiehl Created
 **   13.11.2006 H.Kiehl Allow for multiple time entries.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include "amgdefs.h"

/* Global variables. */
int        sys_log_fd = STDERR_FILENO;
char       *p_work_dir = NULL;
const char *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ test_in_time() $$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int                  i;
   size_t               new_size;
   time_t               current_time;
   char                 str_time[32];
   struct bd_time_entry *te;

   if (get_arg(&argc, argv, "-f", str_time, 32) == SUCCESS)
   {
      current_time = atol(str_time);
   }
   else
   {
      current_time = time(NULL);
   }
   if (argc < 2)
   {
      (void)fprintf(stderr, "Usage: %s [-f <current unix time>] <crontab like time entry 1> [<crontab like time entry n>]\n",
                    argv[0]);
      exit(INCORRECT);
   }

   new_size = (argc - 1) * sizeof(struct bd_time_entry);
   if ((te = malloc(new_size)) == NULL)
   {
      (void)fprintf(stderr, "Failed to malloc() %d bytes : %s",
                    new_size, strerror(errno));
      exit(INCORRECT);
   }
   for (i = 0; i < (argc - 1); i++)
   {
      if (eval_time_str(argv[i + 1], &te[i], NULL) == INCORRECT)
      {
         exit(INCORRECT);
      }
   }
   if (in_time(current_time, (argc - 1), te) == YES)
   {
      (void)fprintf(stdout, "IS in time : %s", ctime(&current_time));
   }
   else
   {
      (void)fprintf(stdout, "IS NOT in time : %s", ctime(&current_time));
   }

   exit(SUCCESS);
}
