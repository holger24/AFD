/*
 *  limit_transfer_rate.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2004 - 2006 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   limit_transfer_rate - limits the transfer rate by pausing
 **
 ** SYNOPSIS
 **   void limit_transfer_rate(int bytes, off_t limit_rate, clock_t clktck)
 **   void init_limit_transfer_rate(void)
 **
 ** DESCRIPTION
 **   This function limits the transfer rate to 'limit_rate'. Before each
 **   file init_limit_transfer_rate() must be called to initialize some
 **   static variables.
 **
 **   Most of this was taken from the wget code (1.9.1) by Hrvoje Niksic.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   23.06.2004 H.Kiehl Created
 **
 */
DESCR__E_M3

#ifdef HAVE_GETTIMEOFDAY
# include <sys/time.h>
#else
# include <sys/times.h>
#endif
#include <unistd.h>
#include <errno.h>
#include "fddefs.h"

/* Loclal global variables. */
static off_t          chunk_bytes;
static double         chunk_start,
                      elapsed_last,
                      elapsed_pre_start,
                      sleep_adjust;
#ifdef HAVE_GETTIMEOFDAY
static struct timeval start;
#else
static clock_t        start;
#endif

/* Local function prototypes. */
static double         time_elapsed(clock_t);


/*######################## limit_transfer_rate() ########################*/
void
limit_transfer_rate(int bytes, off_t limit_rate, clock_t clktck)
{
   double delta_time,
          elapsed_time,
          expected;

   elapsed_time = time_elapsed(clktck);
   delta_time = elapsed_time - chunk_start;
   chunk_bytes += bytes;

   /* Calculate the time it should take to download at the given rate. */
   expected = 1000.0 * (double)chunk_bytes / (double)limit_rate;

   if (expected > delta_time)
   {
      double sleep_time;

      sleep_time = expected - delta_time + sleep_adjust;
      if (sleep_time >= (double)(2 * clktck))
      {
         double t0, t1;

         t0 = elapsed_time;
         (void)my_usleep((unsigned long)(1000 * sleep_time));
         t1 = time_elapsed(clktck);

         sleep_adjust = sleep_time - (t1 - t0);
         elapsed_time = t1;
      }
      else
      {
         return;
      }
   }
   chunk_bytes = 0;
   chunk_start = elapsed_time;

   return;
}


/*###################### init_limit_transfer_rate() #####################*/
void
init_limit_transfer_rate(void)
{
#ifdef HAVE_GETTIMEOFDAY
   if (gettimeofday(&start, NULL) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "gettimeofday() error : %s", strerror(errno));
   }
#else
   struct tms tmsdummy;

   start = times(&tmsdummy);
#endif
   chunk_start = 0.0;
   chunk_bytes = 0;
   elapsed_last = 0.0;
   elapsed_pre_start = 0.0;

   return;
}


/*+++++++++++++++++++++++++++ time_elapsed() ++++++++++++++++++++++++++++*/
static double
time_elapsed(clock_t clktck)
{
   double         elapsed_time;
#ifdef HAVE_GETTIMEOFDAY
   struct timeval now;

   if (gettimeofday(&now, NULL) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "gettimeofday() error : %s", strerror(errno));
   }
   elapsed_time = elapsed_pre_start +
                  ((double)(now.tv_sec - start.tv_sec) * 1000.0) +
                  ((double)(now.tv_usec - start.tv_usec) / 1000.0);
#else
   clock_t        now;
   struct tms     tmsdummy;

   now = times(&tmsdummy);
   elapsed_time = elapsed_pre_start +
                  ((double)(now - start) * (1000.0 / (double)clktck));
#endif
   if (elapsed_time < elapsed_last)
   {
      start = now;
      elapsed_pre_start = elapsed_last;
      elapsed_time = elapsed_last;
   }
   else
   {
      elapsed_last = elapsed_time;
   }

   return(elapsed_time);
}
