/*
 *  calc_next_time.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2017 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   calc_next_time - function to calculate the next time from a crontab
 **                    like entry
 **
 ** SYNOPSIS
 **   time_t calc_next_time(struct bd_time_entry *te,
 **                         char                 *timezone,
 **                         time_t               current_time,
 **                         char                 *source_file,
 **                         int                  source_line);
 **   time_t calc_next_time_array(int                  no_of_entries,
 **                               struct bd_time_entry *te,
 **                               char                 *timezone,
 **                               time_t               current_time,
 **                               char                 *source_file,
 **                               int                  source_line);
 **
 ** DESCRIPTION
 **   The function calc_next_time() calculates from the structure
 **   bd_time_entry te the next time as a time_t value.
 **
 **   The function calc_next_time_array() uses calc_next_time() to
 **   calculate the lowest time from an array of time entries.
 **
 **   Here some example time zone identifiers: Etc/GMT, Europe/Berlin,
 **   Asia/Shanghai, Australia/Sydney, CET, Africa/Maputo, Asia/Tehran,
 **   Asia/Tokyo, Brazil/East, Canada/Central, Cuba, Europe/Dublin,
 **   Europe/Bratislava, Europe/Moscow, GB, Iceland, Indian/Mauritius, etc.
 **
 ** RETURN VALUES
 **   The function will return the next time as a time_t value
 **   when successful. On error it will return 0.
 **
 ** BUGS
 **   It does NOT handle the case "* * 31 2 *" (returns March 2nd).
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   04.05.1999 H.Kiehl Created
 **   13.11.2006 H.Kiehl Added function calc_next_time_array().
 **   18.11.2006 H.Kiehl It did not handle the case when tm_min=59
 **                      correctly. Fixed.
 **   15.06.2012 H.Kiehl If we hit an error show more details and the
 **                      the source line from where we have been called.
 **   17.06.2014 H.Kiehl Add possibility to specify the timezone.
 **
 */
DESCR__E_M3

#include <stdlib.h>                 /* exit()                            */
#include <time.h>
#ifdef TM_IN_SYS_TIME
# include <sys/time.h>
#endif
#ifdef WITH_TIMEZONE
# include <string.h>
# include <errno.h>
#endif
#include "bit_array.h"

/* Local function prototypes. */
static int  check_day(struct bd_time_entry *, struct tm *),
            check_month(struct bd_time_entry *, struct tm *),
            get_greatest_dom(int, int);
#ifdef WITH_TIMEZONE
static void reset_environment(char *, char *, int);
#endif


/*###################### calc_next_time_array() #########################*/
time_t
calc_next_time_array(int                  no_of_entries,
                     struct bd_time_entry *te,
#ifdef WITH_TIMEZONE
                     char                 *timezone,
#endif
                     time_t               current_time,
                     char                 *source_file,
                     int                  source_line)
{
   int    i;
   time_t new_time = 0,
          tmp_time;

   for (i = 0; i < no_of_entries; i++)
   {
      tmp_time = calc_next_time(&te[i],
#ifdef WITH_TIMEZONE
                                timezone,
#endif
                                current_time, source_file, source_line);
      if ((tmp_time < new_time) || (new_time == 0))
      {
         new_time = tmp_time;
      }
   }
   if (new_time < current_time)
   {
      new_time = current_time;
   }

   return(new_time);
}


/*######################### calc_next_time() ############################*/
time_t
calc_next_time(struct bd_time_entry *te,
#ifdef WITH_TIMEZONE
               char                 *timezone,
#endif
               time_t               current_time,
               char                 *source_file,
               int                  source_line)
{
   int       gotcha,
             i;
#ifdef WITH_TIMEZONE
   int       reset_env;
   char      *p_env = NULL;
   time_t    new_time;
#endif
   struct tm *bd_time;     /* Broken-down time */

   if (te->month == TIME_EXTERNAL)
   {
#if SIZEOF_TIME_T == 4
      return(LONG_MAX);
#else
# ifdef LLONG_MAX
      return(LLONG_MAX);
# else
      return(LONG_MAX);
# endif
#endif
   }
   current_time += 60;
#ifdef WITH_TIMEZONE
   if ((timezone != NULL) && (timezone[0] != '\0'))
   {
      if ((p_env = getenv("TZ")) != NULL)
      {
         if (my_strcmp(p_env, timezone) != 0)
         {
            if (setenv("TZ", timezone, 1) == -1)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to setenv() TZ to `%s' : %s",
                          timezone, strerror(errno));
               reset_env = NO;
            }
            else
            {
               tzset();
               reset_env = YES;
            }
         }
         else
         {
            reset_env = NO;
         }
      }
      else
      {
         if (setenv("TZ", timezone, 1) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to setenv() TZ to `%s' : %s",
                       timezone, strerror(errno));
            reset_env = NO;
         }
         else
         {
            tzset();
            reset_env = YES;
         }
      }
   }
   else
   {
      reset_env = NO;
   }
#endif /* WITH_TIMEZONE */
   if ((bd_time = localtime(&current_time)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "localtime() error : %s", strerror(errno));
      return(0);
   }

   if (check_month(te, bd_time) == INCORRECT)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
#ifdef HAVE_LONG_LONG
                 "Broken time entry %lld %lld %u %u %d %d called from %s %d",
                 te->continuous_minute, te->minute,
#else
                 "Broken time entry %d%d%d%d%d%d%d%d %d%d%d%d%d%d%d%d %u %u %d %d called from %s %d",
                 (int)te->continuous_minute[0], (int)te->continuous_minute[1],
                 (int)te->continuous_minute[2], (int)te->continuous_minute[3],
                 (int)te->continuous_minute[4], (int)te->continuous_minute[5],
                 (int)te->continuous_minute[6], (int)te->continuous_minute[7],
                 (int)te->minute[0], (int)te->minute[1], (int)te->minute[2], (int)te->minute[3],
                 (int)te->minute[4], (int)te->minute[5], (int)te->minute[6], (int)te->minute[7],
#endif
                 te->hour, te->day_of_month, (int)(te->month),
                 (int)(te->day_of_week), source_file, source_line);
#ifdef WITH_TIMEZONE
      reset_environment(timezone, p_env, reset_env);
#endif
      return(0);
   }

   if (check_day(te, bd_time) == INCORRECT)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
#ifdef HAVE_LONG_LONG
                 "Broken time entry %lld %lld %u %u %d %d called from %s %d",
                 te->continuous_minute, te->minute,
#else
                 "Broken time entry %d%d%d%d%d%d%d%d %d%d%d%d%d%d%d%d %u %u %d %d called from %s %d",
                 (int)te->continuous_minute[0], (int)te->continuous_minute[1],
                 (int)te->continuous_minute[2], (int)te->continuous_minute[3],
                 (int)te->continuous_minute[4], (int)te->continuous_minute[5],
                 (int)te->continuous_minute[6], (int)te->continuous_minute[7],
                 (int)te->minute[0], (int)te->minute[1], (int)te->minute[2], (int)te->minute[3],
                 (int)te->minute[4], (int)te->minute[5], (int)te->minute[6], (int)te->minute[7],
#endif
                 te->hour, te->day_of_month, (int)(te->month),
                 (int)(te->day_of_week), source_file, source_line);
#ifdef WITH_TIMEZONE
      reset_environment(timezone, p_env, reset_env);
#endif
      return(0);
   }

#ifdef TIME_WITH_SECOND
   /* Evaluate second (0-59) [0-59] */
# ifdef HAVE_LONG_LONG
   if (((ALL_MINUTES & te->second) == ALL_MINUTES) ||
       ((ALL_MINUTES & te->continuous_second) == ALL_MINUTES))
   {
      /* Leave second as is. */;
   }
   else
   {
      gotcha = NO;
      for (i = bd_time->tm_sec; i < 60; i++)
      {
         if ((te->second & bit_array_long[i]) ||
             (te->continuous_second & bit_array_long[i]))
         {
            gotcha = YES;
            break;
         }
      }
      if (gotcha == NO)
      {
         for (i = 0; i < bd_time->tm_sec; i++)
         {
            if ((te->second & bit_array_long[i]) ||
                (te->continuous_second & bit_array_long[i]))
            {
               bd_time->tm_min++;
               gotcha = YES;
               break;
            }
         }
      }
      if (gotcha == NO)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to locate any valid second!?"));
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
#  ifdef HAVE_LONG_LONG
                    "Broken time entry %lld %lld %u %u %d %d called from %s %d",
                    te->continuous_second, te->second,
#  else
                    "Broken time entry %d%d%d%d%d%d%d%d %d%d%d%d%d%d%d%d %u %u %d %d called from %s %d",
                    (int)te->continuous_second[0], (int)te->continuous_second[1],
                    (int)te->continuous_second[2], (int)te->continuous_second[3],
                    (int)te->continuous_second[4], (int)te->continuous_second[5],
                    (int)te->continuous_second[6], (int)te->continuous_second[7],
                    (int)te->second[0], (int)te->second[1], (int)te->second[2], (int)te->second[3],
                    (int)te->second[4], (int)te->second[5], (int)te->second[6], (int)te->second[7],
#  endif
                    te->hour, te->day_of_month, (int)(te->month),
                    (int)(te->day_of_week), source_file, source_line);
#  ifdef WITH_TIMEZONE
         reset_environment(timezone, p_env, reset_env);
#  endif
         return(0);
      }
      bd_time->tm_sec = i;
   }
# else
   gotcha = NO;
   for (i = bd_time->tm_sec; i < 60; i++)
   {
      if (bittest(te->second, i) || bittest(te->continuous_second, i))
      {
         gotcha = YES;
         break;
      }
   }
   if (gotcha == NO)
   {
      for (i = 0; i < bd_time->tm_sec; i++)
      {
         if (bittest(te->second, i) || bittest(te->continuous_second, i))
         {
            bd_time->tm_min++;
            gotcha = YES;
            break;
         }
      }
   }
   if (gotcha == NO)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to locate any valid minute!?"));
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
#  ifdef HAVE_LONG_LONG
                 "Broken time entry %lld %lld %u %u %d %d called from %s %d",
                 te->continuous_second, te->second,
#  else
                 "Broken time entry %d%d%d%d%d%d%d%d %d%d%d%d%d%d%d%d %u %u %d %d called from %s %d",
                 (int)te->continuous_second[0], (int)te->continuous_second[1],
                 (int)te->continuous_second[2], (int)te->continuous_second[3],
                 (int)te->continuous_second[4], (int)te->continuous_second[5],
                 (int)te->continuous_second[6], (int)te->continuous_second[7],
                 (int)te->second[0], (int)te->second[1], (int)te->second[2], (int)te->second[3],
                 (int)te->second[4], (int)te->second[5], (int)te->second[6], (int)te->second[7],
#  endif
                 te->hour, te->day_of_month, (int)(te->month),
                 (int)(te->day_of_week), source_file, source_line);
#  ifdef WITH_TIMEZONE
      reset_environment(timezone, p_env, reset_env);
#  endif
      return(0);
   }
   bd_time->tm_min = i;
# endif
#endif /* TIME_WITH_SECOND */

   /* Evaluate minute (0-59) [0-59] */
#ifdef HAVE_LONG_LONG
   if (((ALL_MINUTES & te->minute) == ALL_MINUTES) ||
       ((ALL_MINUTES & te->continuous_minute) == ALL_MINUTES))
   {
      /* Leave minute as is. */;
   }
   else
   {
      gotcha = NO;
      for (i = bd_time->tm_min; i < 60; i++)
      {
         if ((te->minute & bit_array_long[i]) ||
             (te->continuous_minute & bit_array_long[i]))
         {
            gotcha = YES;
            break;
         }
      }
      if (gotcha == NO)
      {
         for (i = 0; i < bd_time->tm_min; i++)
         {
            if ((te->minute & bit_array_long[i]) ||
                (te->continuous_minute & bit_array_long[i]))
            {
               bd_time->tm_hour++;
               gotcha = YES;
               break;
            }
         }
      }
      if (gotcha == NO)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to locate any valid minute!?"));
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
# ifdef HAVE_LONG_LONG
                    "Broken time entry %lld %lld %u %u %d %d called from %s %d",
                    te->continuous_minute, te->minute,
# else
                    "Broken time entry %d%d%d%d%d%d%d%d %d%d%d%d%d%d%d%d %u %u %d %d called from %s %d",
                    (int)te->continuous_minute[0], (int)te->continuous_minute[1],
                    (int)te->continuous_minute[2], (int)te->continuous_minute[3],
                    (int)te->continuous_minute[4], (int)te->continuous_minute[5],
                    (int)te->continuous_minute[6], (int)te->continuous_minute[7],
                    (int)te->minute[0], (int)te->minute[1], (int)te->minute[2], (int)te->minute[3],
                    (int)te->minute[4], (int)te->minute[5], (int)te->minute[6], (int)te->minute[7],
# endif
                    te->hour, te->day_of_month, (int)(te->month),
                    (int)(te->day_of_week), source_file, source_line);
# ifdef WITH_TIMEZONE
         reset_environment(timezone, p_env, reset_env);
# endif
         return(0);
      }
      bd_time->tm_min = i;
   }
#else
   gotcha = NO;
   for (i = bd_time->tm_min; i < 60; i++)
   {
      if (bittest(te->minute, i) || bittest(te->continuous_minute, i))
      {
         gotcha = YES;
         break;
      }
   }
   if (gotcha == NO)
   {
      for (i = 0; i < bd_time->tm_min; i++)
      {
         if (bittest(te->minute, i) || bittest(te->continuous_minute, i))
         {
            bd_time->tm_hour++;
            gotcha = YES;
            break;
         }
      }
   }
   if (gotcha == NO)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to locate any valid minute!?"));
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
# ifdef HAVE_LONG_LONG
                 "Broken time entry %lld %lld %u %u %d %d called from %s %d",
                 te->continuous_minute, te->minute,
# else
                 "Broken time entry %d%d%d%d%d%d%d%d %d%d%d%d%d%d%d%d %u %u %d %d called from %s %d",
                 (int)te->continuous_minute[0], (int)te->continuous_minute[1],
                 (int)te->continuous_minute[2], (int)te->continuous_minute[3],
                 (int)te->continuous_minute[4], (int)te->continuous_minute[5],
                 (int)te->continuous_minute[6], (int)te->continuous_minute[7],
                 (int)te->minute[0], (int)te->minute[1], (int)te->minute[2], (int)te->minute[3],
                 (int)te->minute[4], (int)te->minute[5], (int)te->minute[6], (int)te->minute[7],
# endif
                 te->hour, te->day_of_month, (int)(te->month),
                 (int)(te->day_of_week), source_file, source_line);
# ifdef WITH_TIMEZONE
      reset_environment(timezone, p_env, reset_env);
# endif
      return(0);
   }
   bd_time->tm_min = i;
#endif

   /* Evaluate hour (0-23) [0-23] */
   if ((ALL_HOURS & te->hour) != ALL_HOURS)
   {
      gotcha = NO;
      for (i = bd_time->tm_hour; i < 24; i++)
      {
         if (te->hour & bit_array[i])
         {
            gotcha = YES;
            break;
         }
      }
      if (gotcha == NO)
      {
         for (i = 0; i < bd_time->tm_hour; i++)
         {
            if (te->hour & bit_array[i])
            {
               bd_time->tm_mday++;
               bd_time->tm_wday++;
               if (bd_time->tm_wday == 7)
               {
                  bd_time->tm_wday = 0;
               }
               if (check_day(te, bd_time) == INCORRECT)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
#ifdef HAVE_LONG_LONG
                             "Broken time entry %lld %lld %u %u %d %d called from %s %d",
                             te->continuous_minute, te->minute,
#else
                             "Broken time entry %d%d%d%d%d%d%d%d %d%d%d%d%d%d%d%d %u %u %d %d called from %s %d",
                             (int)te->continuous_minute[0], (int)te->continuous_minute[1],
                             (int)te->continuous_minute[2], (int)te->continuous_minute[3],
                             (int)te->continuous_minute[4], (int)te->continuous_minute[5],
                             (int)te->continuous_minute[6], (int)te->continuous_minute[7],
                             (int)te->minute[0], (int)te->minute[1], (int)te->minute[2], (int)te->minute[3],
                             (int)te->minute[4], (int)te->minute[5], (int)te->minute[6], (int)te->minute[7],
#endif
                             te->hour, te->day_of_month, (int)(te->month),
                             (int)(te->day_of_week), source_file, source_line);
#ifdef WITH_TIMEZONE
                  reset_environment(timezone, p_env, reset_env);
#endif
                  return(0);
               }
               gotcha = YES;
               break;
            }
         }
      }
      if (gotcha == NO)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to locate any valid hour!?"));
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
#ifdef HAVE_LONG_LONG
                    "Broken time entry %lld %lld %u %u %d %d called from %s %d",
                    te->continuous_minute, te->minute,
#else
                    "Broken time entry %d%d%d%d%d%d%d%d %d%d%d%d%d%d%d%d %u %u %d %d called from %s %d",
                    (int)te->continuous_minute[0], (int)te->continuous_minute[1],
                    (int)te->continuous_minute[2], (int)te->continuous_minute[3],
                    (int)te->continuous_minute[4], (int)te->continuous_minute[5],
                    (int)te->continuous_minute[6], (int)te->continuous_minute[7],
                    (int)te->minute[0], (int)te->minute[1], (int)te->minute[2], (int)te->minute[3],
                    (int)te->minute[4], (int)te->minute[5], (int)te->minute[6], (int)te->minute[7],
#endif
                    te->hour, te->day_of_month, (int)(te->month),
                    (int)(te->day_of_week), source_file, source_line);
#ifdef WITH_TIMEZONE
         reset_environment(timezone, p_env, reset_env);
#endif
         return(0);
      }
      if (bd_time->tm_hour != i)
      {
         bd_time->tm_hour = i;
#ifdef HAVE_LONG_LONG
         if (((ALL_MINUTES & te->minute) == ALL_MINUTES) ||
             ((ALL_MINUTES & te->continuous_minute) == ALL_MINUTES))
         {
            bd_time->tm_min = 0;
         }
         else
         {
            for (i = 0; i < bd_time->tm_min; i++)
            {
               if ((te->minute & bit_array_long[i]) ||
                   (te->continuous_minute & bit_array_long[i]))
               {
                  break;
               }
            }
            bd_time->tm_min = i;
         }
#else
         for (i = 0; i < bd_time->tm_min; i++)
         {
            if (bittest(te->minute, i) || bittest(te->continuous_minute, i))
            {
               break;
            }
         }
         bd_time->tm_min = i;
#endif
      }
   }
   bd_time->tm_sec = 0;
#ifdef WITH_TIMEZONE
   new_time = mktime(bd_time);
   reset_environment(timezone, p_env, reset_env);

   return(new_time);
#else
   return(mktime(bd_time));
#endif
}


/*++++++++++++++++++++++++++++ check_month() ++++++++++++++++++++++++++++*/
static int
check_month(struct bd_time_entry *te, struct tm *bd_time)
{
   /* Evaluate month (1-12) [0-11] */
   if ((ALL_MONTH & te->month) != ALL_MONTH)
   {
      int gotcha,
          i;

      gotcha = NO;
      for (i = bd_time->tm_mon; i < 12; i++)
      {
         if (te->month & bit_array[i])
         {
            gotcha = YES;
            break;
         }
      }
      if (gotcha == NO)
      {
         for (i = 0; i < bd_time->tm_mon; i++)
         {
            if (te->month & bit_array[i])
            {
               bd_time->tm_year++;
               gotcha = YES;
               break;
            }
         }
      }
      if (gotcha == NO)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to locate any valid month!?"));
         return(INCORRECT);
      }
      if (bd_time->tm_mon != i)
      {
         bd_time->tm_mon = i;
         bd_time->tm_mday = 1;
         bd_time->tm_hour = 0;
         bd_time->tm_min = 0;
         if (te->day_of_week != ALL_DAY_OF_WEEK)
         {
            time_t time_val;

            time_val = mktime(bd_time);
            if ((bd_time = localtime(&time_val)) == NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "check_month(): localtime() error : %s", strerror(errno));
               return(INCORRECT);
            }
         }
      }
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++++ check_day() +++++++++++++++++++++++++++++*/
static int
check_day(struct bd_time_entry *te, struct tm *bd_time)
{
   int gotcha,
       i;

   if ((te->day_of_week != ALL_DAY_OF_WEEK) &&
       (te->day_of_month != ALL_DAY_OF_MONTH))
   {
      int dow = bd_time->tm_wday,
          greatest_dom,
          years_searched = bd_time->tm_year;

      gotcha = NO;
      do
      {
         greatest_dom = get_greatest_dom(bd_time->tm_mon, bd_time->tm_year + 1900);
         for (i = (bd_time->tm_mday - 1); i < greatest_dom; i++)
         {
            if ((te->day_of_month & bit_array[i]) &&
                (te->day_of_week & bit_array[dow]))
            {
               gotcha = YES;
               break;
            }
            dow++;
            if (dow == 7)
            {
               dow = 0;
            }
         }
         if (gotcha == NO)
         {
            /* Ensure that we do not go into an endless loop! */
            if ((bd_time->tm_year - years_searched) >= 2000)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Searched 2000 years, giving up."));
               return(INCORRECT);
            }

            bd_time->tm_mon++;
            if (bd_time->tm_mon == 12)
            {
               bd_time->tm_mon = 0;
               bd_time->tm_year++;
            }
            bd_time->tm_wday = dow;
            if (check_month(te, bd_time) == INCORRECT)
            {
               return(INCORRECT);
            }
            dow = bd_time->tm_wday;
            bd_time->tm_hour = 0;
            bd_time->tm_min = 0;
         }
      } while (gotcha == NO);
      if (gotcha == NO)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to locate any valid day of month!?"));
         return(INCORRECT);
      }
      if (bd_time->tm_mday != (i + 1))
      {
         bd_time->tm_mday = i + 1;
         bd_time->tm_hour = 0;
         bd_time->tm_min = 0;
      }
   }
   else
   {
      /* Evaluate day of week (1-7) [0-6] */
      if ((ALL_DAY_OF_WEEK & te->day_of_week) != ALL_DAY_OF_WEEK)
      {
         gotcha = NO;
         if (bd_time->tm_wday == 0)
         {
            if (te->day_of_week & bit_array[6])
            {
               gotcha = YES;
            }
         }
         else
         {
            if (te->day_of_week & bit_array[bd_time->tm_wday - 1])
            {
               gotcha = YES;
            }
         }
         if (gotcha == NO)
         {
            int offset_counter = 1,
                wday;

            /*
             * It's not this day of the week. Let's find the next possible
             * day.
             */
            wday = bd_time->tm_wday + 1;
            if (wday > 6)
            {
               wday = 0;
            }
            for (i = wday; i < 7; i++)
            {
               if (i == 0)
               {
                  if (te->day_of_week & bit_array[6])
                  {
                     gotcha = YES;
                     break;
                  }
               }
               else
               {
                  if (te->day_of_week & bit_array[i - 1])
                  {
                     gotcha = YES;
                     break;
                  }
               }
               offset_counter++;
            }
            if (gotcha == NO)
            {
               if (te->day_of_week & bit_array[6])
               {
                  gotcha = YES;
               }
               else
               {
                  offset_counter++;
                  for (i = 1; i < wday; i++)
                  {
                     if (te->day_of_week & bit_array[i - 1])
                     {
                        gotcha = YES;
                        break;
                     }
                     offset_counter++;
                  }
               }
            }
            if (gotcha == NO)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to locate any valid day of week!?"));
               return(INCORRECT);
            }
            if (offset_counter > 0)
            {
               bd_time->tm_mday += offset_counter;
            }
            bd_time->tm_hour = 0;
            bd_time->tm_min = 0;
         }
      }

      /* Evaluate day of month (1-31) [1-31] */
      if ((ALL_DAY_OF_MONTH & te->day_of_month) != ALL_DAY_OF_MONTH)
      {
         gotcha = NO;
         for (i = (bd_time->tm_mday - 1); i < 31; i++)
         {
            if (te->day_of_month & bit_array[i])
            {
               gotcha = YES;
               break;
            }
         }
         if (gotcha == NO)
         {
            for (i = 0; i < (bd_time->tm_mday - 1); i++)
            {
               if (te->day_of_month & bit_array[i])
               {
                  bd_time->tm_mon++;
                  gotcha = YES;
                  break;
               }
            }
         }
         if (gotcha == NO)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to locate any valid day of month!?"));
            return(INCORRECT);
         }
         if (bd_time->tm_mday != (i + 1))
         {
            bd_time->tm_mday = i + 1;
            bd_time->tm_hour = 0;
            bd_time->tm_min = 0;
         }
      }
   }

   return(SUCCESS);
}


#ifdef WITH_TIMEZONE
/*+++++++++++++++++++++++++++++ check_day() +++++++++++++++++++++++++++++*/
static void
reset_environment(char *timezone, char *p_env, int reset_env)
{
   if (timezone != NULL)
   {
      if (p_env == NULL)
      {
         if (unsetenv("TZ") == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Failed to unsetenv() TZ : %s", strerror(errno));
         }
         else
         {
            tzset();
         }
      }
      else
      {
         if (reset_env == YES)
         {
            if (setenv("TZ", p_env, 1) == -1)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to setenv() TZ to `%s' : %s",
                          p_env, strerror(errno));
            }
            else
            {
               tzset();
            }
         }
      }
   }

   return;
}
#endif


/*-------------------------- get_greatest_dom() -------------------------*/
static int
get_greatest_dom(int month, int year)
{
   int dom;

   switch (month)
   {
      case 0:
         dom = 31;
         break;
      case 1:
         if (((year % 4) == 0) &&
             (((year % 100) != 0) || ((year % 400) == 0)))
         {
            dom = 29;
         }
         else
         {
            dom = 28;
         }
         break;
      case 2:
         dom = 31;
         break;
      case 3:
         dom = 30;
         break;
      case 4:
         dom = 31;
         break;
      case 5:
         dom = 30;
         break;
      case 6:
         dom = 31;
         break;
      case 7:
         dom = 31;
         break;
      case 8:
         dom = 30;
         break;
      case 9:
         dom = 31;
         break;
      case 10:
         dom = 30;
         break;
      case 11:
         dom = 31;
         break;
      default :
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("Get a new operating system!"));
         exit(INCORRECT);
   }

   return(dom);
}
