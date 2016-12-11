/*
 *  get_sum_cpu_usage.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2016 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_sum_cpu_usage - function calcultes CPU time
 **
 ** SYNOPSIS
 **   void get_sum_cpu_usage(struct rusage *ru_start, struct timeval *cpu_usage)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   Retuns CPU time in struct timeval cpu_usage.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   29.11.2016 H.Kiehl Created.
 **
 */
DESCR__E_M3

#include <sys/time.h>
#include <sys/resource.h>


/*######################### get_sum_cpu_usage() #########################*/
void
get_sum_cpu_usage(struct rusage *ru_start, struct timeval *cpu_usage)
{
   struct rusage  ru;
   struct timeval start,
                  end;

   (void)getrusage(RUSAGE_SELF, &ru);
   start.tv_usec = ru_start->ru_utime.tv_usec + ru_start->ru_stime.tv_usec;
   start.tv_sec = ru_start->ru_utime.tv_sec + ru_start->ru_stime.tv_sec;
   end.tv_usec = ru.ru_utime.tv_usec + ru.ru_stime.tv_usec;
   end.tv_sec = ru.ru_utime.tv_sec + ru.ru_stime.tv_sec;
   if (end.tv_usec > start.tv_usec)
   {
      end.tv_usec += 1000000L;
      end.tv_sec -= 1;
   }
   cpu_usage->tv_usec = end.tv_usec - start.tv_usec;
   cpu_usage->tv_sec = end.tv_sec - start.tv_sec;
   if (cpu_usage->tv_usec > 1000000L)
   {
      cpu_usage->tv_usec -= 1000000L;
      cpu_usage->tv_sec++;
   }

   /*
    * In cas this function is called in a loop, lets store the
    * current values in the start structure. It saves the caller
    * of this function to call getrusage() again.
    */
   ru_start->ru_utime.tv_sec = ru.ru_utime.tv_sec;
   ru_start->ru_utime.tv_usec = ru.ru_utime.tv_usec;
   ru_start->ru_stime.tv_sec = ru.ru_stime.tv_sec;
   ru_start->ru_stime.tv_usec = ru.ru_stime.tv_usec;

   return;
}
