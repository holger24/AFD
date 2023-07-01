/*
 *  datestr2unixtime.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2006 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   datestr2unixtime - converts a date string to unix time
 **
 ** SYNOPSIS
 **   time_t datestr2unixtime(char *date_str, int *accuracy)
 **
 ** DESCRIPTION
 **   The function datestr2unixtime() converts the string 'date_str'
 **   to a unix time. It is able to convert the following seven time
 **   strings:
 **     1) RFC822 (HTTP/1.1)     Fri, 3 Oct 1997 02:15:31 GMT
 **     2) RFC850 (pre HTTP/1.1) Friday, 03-Oct-97 02:15:31 GMT
 **     3) asctime()             Fri Oct  3 02:15:31 1997
 **     4) HTML directory list   03-Oct-1997 02:15
 **     5) HTML directory list   2019-07-28 00:03
 **     6) HTML directory list   28-07-2019 00:03
 **     7) AWS ISO 8601          2019-07-28T00:03:29.429Z
 **
 **   Note that in format 2) we do NOT evaluate the year, instead we
 **   just take the current year.
 **
 **   FIXME: Currently I do not know how to handle the timezone. So we
 **          ignore it here and hope for the best!
 **
 ** RETURN VALUES
 **   Returns the unix time when it finds one of the mentioned
 **   patterns, otherwise -1 is returned. If the caller provides
 **   the space for accuracy, it returns the accuracy of the time:
 **        DS2UT_SECOND for seconds
 **        DS2UT_MINUTE for minutes
 **        DS2UT_DAY    for day
 **        DS2UT_NONE   no date found
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.08.2006 H.Kiehl Created
 **   12.08.2021 H.Kiehl Add AWS ISO 8601.
 **   13.08.2021 H.Kiehl Added parameter accuracy.
 **   30.06.2023 H.Kiehl Add another HTML directory list: 28-07-2019 00:03
 */
DESCR__E_M3

#include <ctype.h>
#include <time.h>
#include <sys/time.h>      /* struct tm                                  */

#define GET_MONTH()                                                        \
        {                                                                  \
           if ((*ptr == 'J') && (*(ptr + 1) == 'a') && (*(ptr + 2) == 'n'))\
           {                                                               \
              tp->tm_mon = 0;                                              \
           }                                                               \
           else if ((*ptr == 'F') && (*(ptr + 1) == 'e') && (*(ptr + 2) == 'b'))\
                {                                                          \
                   tp->tm_mon = 1;                                         \
                }                                                          \
           else if ((*ptr == 'M') && (*(ptr + 2) == 'r'))                  \
                {                                                          \
                   tp->tm_mon = 2;                                         \
                }                                                          \
           else if ((*ptr == 'A') && (*(ptr + 1) == 'p') && (*(ptr + 2) == 'r'))\
                {                                                          \
                   tp->tm_mon = 3;                                         \
                }                                                          \
           else if ((*ptr == 'M') && (*(ptr + 1) == 'a') &&                \
                    ((*(ptr + 2) == 'y') || (*(ptr + 2) == 'i')))          \
                {                                                          \
                   tp->tm_mon = 4;                                         \
                }                                                          \
           else if ((*ptr == 'J') && (*(ptr + 1) == 'u') && (*(ptr + 2) == 'n'))\
                {                                                          \
                   tp->tm_mon = 5;                                         \
                }                                                          \
           else if ((*ptr == 'J') && (*(ptr + 1) == 'u') && (*(ptr + 2) == 'l'))\
                {                                                          \
                   tp->tm_mon = 6;                                         \
                }                                                          \
           else if ((*ptr == 'A') && (*(ptr + 1) == 'u') && (*(ptr + 2) == 'g'))\
                {                                                          \
                   tp->tm_mon = 7;                                         \
                }                                                          \
           else if ((*ptr == 'S') && (*(ptr + 1) == 'e') && (*(ptr + 2) == 'p'))\
                {                                                          \
                   tp->tm_mon = 8;                                         \
                }                                                          \
           else if ((*ptr == 'O') && (*(ptr + 2) == 't') &&                \
                    ((*(ptr + 1) == 'c') || (*(ptr + 1) == 'k')))          \
                {                                                          \
                   tp->tm_mon = 9;                                         \
                }                                                          \
           else if ((*ptr == 'N') && (*(ptr + 1) == 'o') && (*(ptr + 2) == 'v'))\
                {                                                          \
                   tp->tm_mon = 10;                                        \
                }                                                          \
           else if ((*ptr == 'D') && (*(ptr + 1) == 'e') &&                \
                    ((*(ptr + 2) == 'c') || (*(ptr + 2) == 'z')))          \
                {                                                          \
                   tp->tm_mon = 11;                                        \
                }                                                          \
        }


/*######################### datestr2unixtime() ##########################*/
time_t
datestr2unixtime(char *date_str, int *accuracy)
{
   time_t    current_time;
   char      *ptr;
   struct tm *tp;

   ptr = date_str;
   current_time = time(NULL);
   tp = gmtime(&current_time);

   /* RFC 822 format: Fri, 3 Oct 1997 02:15:31 GMT */
   if ((isalpha((int)(*ptr))) && (isalpha((int)(*(ptr + 1)))) &&
       (isalpha((int)(*(ptr + 2)))) && (*(ptr + 3) == ',') &&
       (*(ptr + 4) == ' '))
   {
      ptr += 5;

      if (((isdigit((int)(*ptr))) && ((*(ptr + 1) == ' ') ||
           ((isdigit((int)(*(ptr + 1)))) && (*(ptr + 2) == ' ')))) ||
          ((*ptr == ' ') && (isdigit((int)(*(ptr + 1)))) &&
           (*(ptr + 2) == ' ')))
      {
         if (*(ptr + 1) == ' ')
         {
            tp->tm_mday = *ptr - '0';
            ptr += 2;
         }
         else
         {
            if (*ptr == ' ')
            {
               tp->tm_mday = *(ptr + 1) - '0';
            }
            else
            {
               tp->tm_mday = ((*ptr - '0') * 10) + *(ptr + 1) - '0';
            }
            ptr += 3;
         }
         GET_MONTH();

         if (*(ptr + 3) == ' ')
         {
            ptr += 4;
            if ((isdigit((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))) &&
                (isdigit((int)(*(ptr + 2)))) && (isdigit((int)(*(ptr + 3)))) &&
                (*(ptr + 4) == ' '))
            {
               tp->tm_year = (((*ptr - '0') * 1000) +
                              ((*(ptr + 1) - '0') * 100) +
                              ((*(ptr + 2) - '0') * 10) +
                              (*(ptr + 3) - '0')) - 1900;
               ptr += 5;
               if ((isdigit((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))) &&
                   (*(ptr + 2) == ':'))
               {
                  tp->tm_hour = ((*ptr - '0') * 10) + *(ptr + 1) - '0';
                  ptr += 3;
                  if ((isdigit((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))))
                  {
                     tp->tm_min = ((*ptr - '0') * 10) + *(ptr + 1) - '0';
                     ptr += 2;
                     if ((*ptr == ':') && (isdigit((int)(*(ptr + 1)))) &&
                         (isdigit((int)(*(ptr + 2)))))
                     {
                        tp->tm_sec = ((*(ptr + 1) - '0') * 10) +
                                     *(ptr + 2) - '0';
                        if (accuracy != NULL)
                        {
                           *accuracy = DS2UT_SECOND;
                        }
                     }
                     else
                     {
                        tp->tm_sec = 0;
                        if (accuracy != NULL)
                        {
                           *accuracy = DS2UT_MINUTE;
                        }
                     }

                     /*
                      * FIXME: Currently I do not know how to handle the time
                      *        zone. So we ignore it here and hope for the
                      *        best!
                      */
                     return(mktime(tp));
                  }
               }
            }
         }
      }
   }
        /* asctime() format : Fri Oct  3 02:15:31 1997 */
   else if ((isalpha((int)(*ptr))) && (isalpha((int)(*(ptr + 1)))) &&
            (isalpha((int)(*(ptr + 2)))) && (*(ptr + 3) == ' ') &&
            (isalpha((int)(*(ptr + 4)))) && (isalpha((int)(*(ptr + 5)))) &&
            (isalpha((int)(*(ptr + 6)))) && (*(ptr + 7) == ' '))
        {
           ptr += 4;
           GET_MONTH();
  
           if (*(ptr + 3) == ' ')
           {
              ptr += 4;
              if (((isdigit((int)(*ptr))) && ((*(ptr + 1) == ' ') ||
                   ((isdigit((int)(*(ptr + 1)))) && (*(ptr + 2) == ' ')))) ||
                  ((*ptr == ' ') && (isdigit((int)(*(ptr + 1)))) &&
                   (*(ptr + 2) == ' ')))
              {
                 if (*(ptr + 1) == ' ')
                 {
                    tp->tm_mday = *ptr - '0';
                    ptr += 2;
                 }
                 else
                 {
                    if (*ptr == ' ')
                    {
                       tp->tm_mday = *(ptr + 1) - '0';
                    }
                    else
                    {
                       tp->tm_mday = ((*ptr - '0') * 10) + *(ptr + 1) - '0';
                    }
                    ptr += 3;
                 }
                 if ((isdigit((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))) &&
                     (*(ptr + 2) == ':'))
                 {
                    tp->tm_hour = ((*ptr - '0') * 10) + *(ptr + 1) - '0';
                    ptr += 3;
                    if ((isdigit((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))))
                    {
                       tp->tm_min = ((*ptr - '0') * 10) + *(ptr + 1) - '0';
                       ptr += 2;
                       if ((*ptr == ':') && (isdigit((int)(*(ptr + 1)))) &&
                           (isdigit((int)(*(ptr + 2)))))
                       {
                          tp->tm_sec = ((*(ptr + 1) - '0') * 10) +
                                       *(ptr + 2) - '0';
                          ptr += 3;
                          if (accuracy != NULL)
                          {
                             *accuracy = DS2UT_SECOND;
                          }
                       }
                       else
                       {
                          tp->tm_sec = 0;
                          if (accuracy != NULL)
                          {
                             *accuracy = DS2UT_MINUTE;
                          }
                       }
                       if ((*ptr == ' ') && (isdigit((int)(*(ptr + 1)))) &&
                           (isdigit((int)(*(ptr + 2)))) &&
                           (isdigit((int)(*(ptr + 3)))) &&
                           (isdigit((int)(*(ptr + 4)))))
                       {
                          tp->tm_year = (((*(ptr + 1) - '0') * 1000) +
                                         ((*(ptr + 2) - '0') * 100) +
                                         ((*(ptr + 3) - '0') * 10) +
                                         (*(ptr + 4) - '0')) - 1900;
                       }

                       /*
                        * FIXME: Currently I do not know how to handle the time
                        *        zone. So we ignore it here and hope for the
                        *        best!
                        */
                       return(mktime(tp));
                    }
                 }
              }
           }
        }
        /* HTML directory list: 03-Oct-1997 02:15 */
   else if ((isdigit((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))) &&
            (*(ptr + 2) == '-') &&
            (isalpha((int)(*(ptr + 3)))) && (isalpha((int)(*(ptr + 4)))) &&
            (isalpha((int)(*(ptr + 5)))) &&
            (*(ptr + 6) == '-'))
        {
           tp->tm_mday = ((*ptr - '0') * 10) + *(ptr + 1) - '0';
           ptr += 3;

           GET_MONTH();

           if ((*(ptr + 3) == '-') && (isdigit((int)(*(ptr + 4)))) &&
               (isdigit((int)(*(ptr + 5)))) && (isdigit((int)(*(ptr + 6)))) &&
               (isdigit((int)(*(ptr + 7)))) && (*(ptr + 8) == ' '))
           {
              tp->tm_year = (((*(ptr + 4) - '0') * 1000) +
                             ((*(ptr + 5) - '0') * 100) +
                             ((*(ptr + 6) - '0') * 10) +
                             (*(ptr + 7) - '0')) - 1900;
              ptr += 9;
              if ((isdigit((int)(*ptr))) && (*(ptr + 2) == ':') &&
                  (isdigit((int)(*(ptr + 1)))))
              {
                 tp->tm_hour = ((*ptr - '0') * 10) + *(ptr + 1) - '0';
                 ptr += 3;
                 if ((isdigit((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))))
                 {
                    tp->tm_min = ((*ptr - '0') * 10) + *(ptr + 1) - '0';
                    ptr += 2;
                    if ((*ptr == ':') && (isdigit((int)(*(ptr + 1)))) &&
                        (isdigit((int)(*(ptr + 2)))))
                    {
                       tp->tm_sec = ((*(ptr + 1) - '0') * 10) +
                                    *(ptr + 2) - '0';
                       if (accuracy != NULL)
                       {
                          *accuracy = DS2UT_SECOND;
                       }
                    }
                    else
                    {
                       tp->tm_sec = 0;
                       if (accuracy != NULL)
                       {
                          *accuracy = DS2UT_MINUTE;
                       }
                    }
                    return(mktime(tp));
                 }
              }
           }
        }
        /* AWS ISO 8601: 2019-07-28T00:03:29.429Z */
        /*               2019-07-28T00:03:29Z     */
   else if ((isdigit((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))) &&
            (isdigit((int)(*(ptr + 2)))) && (isdigit((int)(*(ptr + 3)))) &&
            (*(ptr + 4) == '-') && (isdigit((int)(*(ptr + 5)))) &&
            (isdigit((int)(*(ptr + 6)))) && (*(ptr + 7) == '-') &&
            (isdigit((int)(*(ptr + 8)))) && (isdigit((int)(*(ptr + 9)))) &&
            (*(ptr + 10) == 'T') && (isdigit((int)(*(ptr + 11)))) &&
            (isdigit((int)(*(ptr + 12)))) && (*(ptr + 13) == ':') &&
            (isdigit((int)(*(ptr + 14)))) && (isdigit((int)(*(ptr + 15)))) &&
            (*(ptr + 16) == ':') && (isdigit((int)(*(ptr + 17)))) &&
            (isdigit((int)(*(ptr + 18)))) &&
            (((*(ptr + 19) == '.') &&
              (isdigit((int)(*(ptr + 20)))) && (isdigit((int)(*(ptr + 21)))) &&
              (isdigit((int)(*(ptr + 22)))) && (*(ptr + 23) == 'Z')) ||
             (*(ptr + 19) == 'Z')))
        {
           tp->tm_year = (((*ptr - '0') * 1000) +
                          ((*(ptr + 1) - '0') * 100) +
                          ((*(ptr + 2) - '0') * 10) +
                          (*(ptr + 3) - '0')) - 1900;
           tp->tm_mon = ((*(ptr + 5) - '0') * 10) + *(ptr + 6) - '0' - 1;
           tp->tm_mday = ((*(ptr + 8) - '0') * 10) + *(ptr + 9) - '0';
           tp->tm_hour = ((*(ptr + 11) - '0') * 10) + *(ptr + 12) - '0';
           tp->tm_min = ((*(ptr + 14) - '0') * 10) + *(ptr + 15) - '0';
           tp->tm_sec = ((*(ptr + 17) - '0') * 10) + *(ptr + 18) - '0';
           if (accuracy != NULL)
           {
              *accuracy = DS2UT_SECOND;
           }
           return(mktime(tp));
        }
        /* HTML directory list: 2019-07-28 00:03 */
   else if ((isdigit((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))) &&
            (isdigit((int)(*(ptr + 2)))) && (isdigit((int)(*(ptr + 3)))) &&
            (*(ptr + 4) == '-'))
        {
           tp->tm_year = (((*ptr - '0') * 1000) +
                          ((*(ptr + 1) - '0') * 100) +
                          ((*(ptr + 2) - '0') * 10) +
                          (*(ptr + 3) - '0')) - 1900;
           if (*(ptr + 4) == '-')
           {
              tp->tm_mon = ((*(ptr + 5) - '0') * 10) + *(ptr + 6) - '0' - 1;
              if (*(ptr + 7) == '-')
              {
                 tp->tm_mday = ((*(ptr + 8) - '0') * 10) + *(ptr + 9) - '0';
                 if (*(ptr + 10) == ' ')
                 {
                    ptr += 11;
                    if ((isdigit((int)(*ptr))) && (*(ptr + 2) == ':') &&
                        (isdigit((int)(*(ptr + 1)))))
                    {
                       tp->tm_hour = ((*ptr - '0') * 10) + *(ptr + 1) - '0';
                       ptr += 3;
                       if ((isdigit((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))))
                       {
                          tp->tm_min = ((*ptr - '0') * 10) + *(ptr + 1) - '0';
                          ptr += 2;
                          if ((*ptr == ':') && (isdigit((int)(*(ptr + 1)))) &&
                              (isdigit((int)(*(ptr + 2)))))
                          {
                             tp->tm_sec = ((*(ptr + 1) - '0') * 10) +
                                          *(ptr + 2) - '0';
                             if (accuracy != NULL)
                             {
                                *accuracy = DS2UT_SECOND;
                             }
                          }
                          else
                          {
                             tp->tm_sec = 0;
                             if (accuracy != NULL)
                             {
                                *accuracy = DS2UT_MINUTE;
                             }
                          }
                          return(mktime(tp));
                       }
                    }
                 }
                 else
                 {
                    tp->tm_hour = tp->tm_min = tp->tm_sec = 0;
                    if (accuracy != NULL)
                    {
                       *accuracy = DS2UT_DAY;
                    }
                    return(mktime(tp));
                 }
              }
           }
        }
        /* HTML directory list: 28-07-2019 00:03 */
   else if ((isdigit((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))) &&
            (*(ptr + 2) == '-') &&
            (isdigit((int)(*(ptr + 3)))) && (isdigit((int)(*(ptr + 4)))) &&
            (*(ptr + 5) == '-') &&
            (isdigit((int)(*(ptr + 6)))) && (isdigit((int)(*(ptr + 7)))) &&
            (isdigit((int)(*(ptr + 8)))) && (isdigit((int)(*(ptr + 9)))) &&
            (*(ptr + 10) == ' '))
        {
           tp->tm_mday = ((*ptr - '0') * 10) + *(ptr + 1) - '0';
           tp->tm_mon = ((*(ptr + 3) - '0') * 10) + *(ptr + 4) - '0' - 1;
           tp->tm_year = (((*(ptr + 6) - '0') * 1000) +
                          ((*(ptr + 7) - '0') * 100) +
                          ((*(ptr + 8) - '0') * 10) +
                          (*(ptr + 9) - '0')) - 1900;
           if (*(ptr + 10) == ' ')
           {
              ptr += 11;
              if ((isdigit((int)(*ptr))) && (*(ptr + 2) == ':') &&
                  (isdigit((int)(*(ptr + 1)))))
              {
                 tp->tm_hour = ((*ptr - '0') * 10) + *(ptr + 1) - '0';
                 ptr += 3;
                 if ((isdigit((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))))
                 {
                    tp->tm_min = ((*ptr - '0') * 10) + *(ptr + 1) - '0';
                    ptr += 2;
                    if ((*ptr == ':') && (isdigit((int)(*(ptr + 1)))) &&
                        (isdigit((int)(*(ptr + 2)))))
                    {
                       tp->tm_sec = ((*(ptr + 1) - '0') * 10) +
                                    *(ptr + 2) - '0';
                       if (accuracy != NULL)
                       {
                          *accuracy = DS2UT_SECOND;
                       }
                    }
                    else
                    {
                       tp->tm_sec = 0;
                       if (accuracy != NULL)
                       {
                          *accuracy = DS2UT_MINUTE;
                       }
                    }
                    return(mktime(tp));
                 }
              }
           }
           else
           {
              tp->tm_hour = tp->tm_min = tp->tm_sec = 0;
              if (accuracy != NULL)
              {
                 *accuracy = DS2UT_DAY;
              }
              return(mktime(tp));
           }
        }
        else
        {
           int i = 0;

           /* Check for RFC850 format: Friday, 03-Oct-97 02:15:31 GMT */
           while ((isalpha((int)(*(ptr + i)))) && (i < 9))
           {
              i++;
           }
           if ((*(ptr + i) == ',') && (i <= 9) && (i > 0) &&
               (*(ptr + i + 1) == ' '))
           {
              ptr += i + 2;
              if ((isdigit((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))) &&
                  (*(ptr + 2) == '-'))
              {
                 tp->tm_mday = ((*ptr - '0') * 10) + *(ptr + 1) - '0';
                 ptr += 3;

                 GET_MONTH();

                 if ((*(ptr + 3) == '-') && (isdigit((int)(*(ptr + 4)))) &&
                     (isdigit((int)(*(ptr + 5)))) && (*(ptr + 6) == ' '))
                 {
                    /*
                     * NOTE: We ignore the year since it only has two digits.
                     *       Lets not try to play any guessing games in
                     *       finding the correct centry. So lets just take
                     *       the year from current_time. Lets see how this
                     *       works.
                     */
                    ptr += 7;
                    if ((isdigit((int)(*ptr))) && (*(ptr + 2) == ':') &&
                        (isdigit((int)(*(ptr + 1)))))
                    {
                       tp->tm_hour = ((*ptr - '0') * 10) + *(ptr + 1) - '0';
                       ptr += 3;
                       if ((isdigit((int)(*ptr))) &&
                           (isdigit((int)(*(ptr + 1)))))
                       {
                          tp->tm_min = ((*ptr - '0') * 10) + *(ptr + 1) - '0';
                          ptr += 2;
                          if ((*ptr == ':') && (isdigit((int)(*(ptr + 1)))) &&
                              (isdigit((int)(*(ptr + 2)))))
                          {
                             tp->tm_sec = ((*(ptr + 1) - '0') * 10) +
                                          *(ptr + 2) - '0';
                             if (accuracy != NULL)
                             {
                                *accuracy = DS2UT_SECOND;
                             }
                          }
                          else
                          {
                             tp->tm_sec = 0;
                             if (accuracy != NULL)
                             {
                                *accuracy = DS2UT_MINUTE;
                             }
                          }

                          /*
                           * FIXME: Currently I do not know how to handle the
                           *        timezone. So we ignore it here and hope for
                           *        the best!
                           */
                          return(mktime(tp));
                       }
                    }
                 }
              }
           }
        }

   if (accuracy != NULL)
   {
      *accuracy = DS2UT_NONE;
   }
   return(-1);
}
