/*
 *  eval_time.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2012 Deutscher Wetterdienst (DWD),
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
 **   eval_time - evaluates the time field
 **
 ** SYNOPSIS
 **   int eval_time(char *numeric_str, Widget w, time_t *value, int time_type)
 **
 ** DESCRIPTION
 **   Function eval_time() evaluates what time the user enters in the
 **   test widget. The following time formats are possible:
 **      MMDDhhmm
 **      DDhhmm
 **      hhmm
 **      -DDhhmm
 **      -hhmm
 **      -mm
 **   where
 **      MM   - Month of the year      [01 - 12]
 **      DD   - Day of the month       [01 - 31]
 **      hh   - Hours after midnight   [00 - 23]
 **      mm   - Minutes after the hour [00 - 59]
 **
 **   The relative form always starts with a -.
 **
 ** RETURN VALUES
 **   SUCCESS when the time entered is correct. Otherwise INCORECCT is
 **   returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.04.1997 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdlib.h>         /* atoi()                                    */
#include <string.h>         /* strlen()                                  */
#include <ctype.h>          /* isdigit()                                 */
#include <time.h>           /* time(), localtime(), mktime(), strftime() */
#ifdef TM_IN_SYS_TIME                                                      
# include <sys/time.h>
#endif
#include <sys/types.h>

#include <Xm/Xm.h>
#include <Xm/Text.h>
#include "motif_common_defs.h"


/*############################# eval_time() #############################*/
int
eval_time(char *numeric_str, Widget w, time_t *value, int time_type)
{
   int    length = strlen(numeric_str),
          min,
          hour;
   time_t time_val = time(NULL);
   char   str[3];

   switch (length)
   {
      case 0 : /* Assume user means current time. */
               {
                  char time_str[9];

                  if (time_type == START_TIME)
                  {
                     time_val -= (time_val % 86400);
                  }
                  (void)strftime(time_str, 9, "%m%d%H%M", localtime(&time_val));
                  XmTextSetString(w, time_str);
               }
               return(time_val);
      case 3 :
      case 4 :
      case 5 :
      case 6 :
      case 7 :
      case 8 : break;
      default: return(INCORRECT);
   }

   if (numeric_str[0] == '-')
   {
      if ((!isdigit((int)numeric_str[1])) || (!isdigit((int)numeric_str[2])))
      {
         return(INCORRECT);
      }

      if (length == 3) /* -mm */
      {
         str[0] = numeric_str[1];
         str[1] = numeric_str[2];
         str[2] = '\0';
         min = atoi(str);
         if ((min < 0) || (min > 59))
         {
            return(INCORRECT);
         }

         *value = time_val - (min * 60);
      }
      else if (length == 5) /* -hhmm */
           {
              if ((!isdigit((int)numeric_str[3])) ||
                  (!isdigit((int)numeric_str[4])))
              {
                 return(INCORRECT);
              }

              str[0] = numeric_str[1];
              str[1] = numeric_str[2];
              str[2] = '\0';
              hour = atoi(str);
              if ((hour < 0) || (hour > 23))
              {
                 return(INCORRECT);
              }
              str[0] = numeric_str[3];
              str[1] = numeric_str[4];
              min = atoi(str);
              if ((min < 0) || (min > 59))
              {
                 return(INCORRECT);
              }

              *value = time_val - (min * 60) - (hour * 3600);
           }
      else if (length == 7) /* -DDhhmm */
           {
              int days;

              if ((!isdigit((int)numeric_str[3])) ||
                  (!isdigit((int)numeric_str[4])) ||
                  (!isdigit((int)numeric_str[5])) ||
                  (!isdigit((int)numeric_str[6])))
              {
                 return(INCORRECT);
              }

              str[0] = numeric_str[1];
              str[1] = numeric_str[2];
              str[2] = '\0';
              days = atoi(str);
              str[0] = numeric_str[3];
              str[1] = numeric_str[4];
              str[2] = '\0';
              hour = atoi(str);
              if ((hour < 0) || (hour > 23))
              {
                 return(INCORRECT);
              }
              str[0] = numeric_str[5];
              str[1] = numeric_str[6];
              min = atoi(str);
              if ((min < 0) || (min > 59))
              {
                 return(INCORRECT);
              }

              *value = time_val - (min * 60) - (hour * 3600) - (days * 86400);
           }
           else
           {
              return(INCORRECT);
           }

      return(SUCCESS);
   }

   if ((!isdigit((int)numeric_str[0])) || (!isdigit((int)numeric_str[1])) ||
       (!isdigit((int)numeric_str[2])) || (!isdigit((int)numeric_str[3])))
   {
      return(INCORRECT);
   }

   str[0] = numeric_str[0];
   str[1] = numeric_str[1];
   str[2] = '\0';

   if (length == 4) /* hhmm */
   {
      struct tm *bd_time;     /* Broken-down time. */

      hour = atoi(str);
      if ((hour < 0) || (hour > 23))
      {
         return(INCORRECT);
      }
      str[0] = numeric_str[2];
      str[1] = numeric_str[3];
      min = atoi(str);
      if ((min < 0) || (min > 59))
      {
         return(INCORRECT);
      }
      bd_time = localtime(&time_val);
      bd_time->tm_sec  = 0;
      bd_time->tm_min  = min;
      bd_time->tm_hour = hour;

      *value = mktime(bd_time);
   }
   else if (length == 6) /* DDhhmm */
        {
           int       day;
           struct tm *bd_time;     /* Broken-down time. */

           if ((!isdigit((int)numeric_str[4])) ||
               (!isdigit((int)numeric_str[5])))
           {
              return(INCORRECT);
           }
           day = atoi(str);
           if ((day < 0) || (day > 31))
           {
              return(INCORRECT);
           }
           str[0] = numeric_str[2];
           str[1] = numeric_str[3];
           hour = atoi(str);
           if ((hour < 0) || (hour > 23))
           {
              return(INCORRECT);
           }
           str[0] = numeric_str[4];
           str[1] = numeric_str[5];
           min = atoi(str);
           if ((min < 0) || (min > 59))
           {
              return(INCORRECT);
           }
           bd_time = localtime(&time_val);
           bd_time->tm_sec  = 0;
           bd_time->tm_min  = min;
           bd_time->tm_hour = hour;
           bd_time->tm_mday = day;

           *value = mktime(bd_time);
        }
        else /* MMDDhhmm */
        {
           int       month,
                     day;
           struct tm *bd_time;     /* Broken-down time. */

           if ((!isdigit((int)numeric_str[4])) ||
               (!isdigit((int)numeric_str[5])) ||
               (!isdigit((int)numeric_str[6])) ||
               (!isdigit((int)numeric_str[7])))
           {
              return(INCORRECT);
           }
           month = atoi(str);
           if ((month < 0) || (month > 12))
           {
              return(INCORRECT);
           }
           str[0] = numeric_str[2];
           str[1] = numeric_str[3];
           day = atoi(str);
           if ((day < 0) || (day > 31))
           {
              return(INCORRECT);
           }
           str[0] = numeric_str[4];
           str[1] = numeric_str[5];
           hour = atoi(str);
           if ((hour < 0) || (hour > 23))
           {
              return(INCORRECT);
           }
           str[0] = numeric_str[6];
           str[1] = numeric_str[7];
           min = atoi(str);
           if ((min < 0) || (min > 59))
           {
              return(INCORRECT);
           }
           bd_time = localtime(&time_val);
           bd_time->tm_sec  = 0;
           bd_time->tm_min  = min;
           bd_time->tm_hour = hour;
           bd_time->tm_mday = day;
           if ((bd_time->tm_mon == 0) && (month == 12))
           {
              bd_time->tm_year -= 1;
           }
           bd_time->tm_mon  = month - 1;

           *value = mktime(bd_time);
        }

   return(SUCCESS);
}
