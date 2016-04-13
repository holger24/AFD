/*
 *  test_time.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2016 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   test_time -
 **
 ** SYNOPSIS
 **   test_time <crontab like time entry 1> [<crontab like time entry n>]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   04.05.1999 H.Kiehl Created
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


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ test_time() $$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   time_t current_time,
          next_time;
   char   str_time[32];

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
      (void)fprintf(stderr,
                    "Usage: %s [-f <current unix time>] <crontab like time entry 1> [<crontab like time entry n>]\n",
                    argv[0]);
      exit(INCORRECT);
   }

   if (argc == 2)
   {
      struct bd_time_entry te;

      if (eval_time_str(argv[1], &te) == INCORRECT)
      {
         exit(INCORRECT);
      }

      next_time = calc_next_time(&te, "Europe/Madrid", current_time, __FILE__, __LINE__);
   }
   else
   {
      int                  i;
      size_t               new_size;
      struct bd_time_entry *te;

      new_size = (argc - 1) * sizeof(struct bd_time_entry);
      if ((te = malloc(new_size)) == NULL)
      {
         (void)fprintf(stderr, "Failed to malloc() %d bytes : %s",
                       new_size, strerror(errno));
         exit(INCORRECT);
      }

      for (i = 0; i < (argc - 1); i++)
      {
         if (eval_time_str(argv[i + 1], &te[i]) == INCORRECT)
         {
            exit(INCORRECT);
         }
      }
      next_time = calc_next_time_array((argc - 1), te,
#ifdef WITH_TIMEZONE
                                       "Europe/Madrid",
#endif
                                       current_time, __FILE__, __LINE__);
   }

#if SIZEOF_TIME_T == 4
   (void)fprintf(stdout, "%ld -> %s",
#else
   (void)fprintf(stdout, "%lld -> %s",
#endif
                 (pri_time_t)next_time, ctime(&next_time));

   exit(SUCCESS);
}
