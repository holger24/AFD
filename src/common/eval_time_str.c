/*
 *  eval_time_str.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   eval_time_str - evaluates crontab like time entry
 **
 ** SYNOPSIS
 **   int eval_time_str(char *time_str, struct bd_time_entry *te, FILE *cmd_fp);
 **   int check_time_str(char *time_str, FILE *cmd_fp);
 **
 ** DESCRIPTION
 **   Evaluates the time and date fields as follows:
 **
 **   field         | <minute> <hour> <day of month> <month> <day of week>
 **   --------------+-----------------------------------------------------
 **   allowed values|  0-59    0-23       1-31        1-12        1-7
 **
 **   These values are stored by the function eval_time_str() into the
 **   structure bd_time_entry te as a bit array. That is for example the
 **   minute value 15 the 15th bit is set by this function.
 **
 ** RETURN VALUES
 **   SUCCESS when time_str was successfully evaluated. Otherwise
 **   INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   29.04.1999 H.Kiehl Created
 **   21.05.2007 H.Kiehl Added check_time_str() to just check the time
 **                      string supplied.
 **   14.11.2013 H.Kiehl Added support for 'time external'.
 **   27.09.2022 H.Kiehl When giving a range, check that the second
 **                      number is not less then the first number.
 **
 */
DESCR__E_M3

#include <string.h>                 /* memset()                          */
#include <ctype.h>                  /* isdigit()                         */
#include "bit_array.h"

/* Local function prototypes. */
static char *get_time_number(char *, char *, FILE *);


/*########################## eval_time_str() ############################*/
int
eval_time_str(char *time_str, struct bd_time_entry *te, FILE *cmd_fp)
{
   int  continuous = YES,
        first_number = -1,
        number,
        step_size = 0;
   char *ptr = time_str,
        str_number[3];

   memset(te, 0, sizeof(struct bd_time_entry));
   if ((time_str[0] == 'e') && (time_str[1] == 'x') && (time_str[2] == 't') &&
       (time_str[3] == 'e') && (time_str[4] == 'r') && (time_str[5] == 'n') &&
       (time_str[6] == 'a') && (time_str[7] == 'l'))
   {
      te->month = TIME_EXTERNAL;
      return(SUCCESS);
   }

   /* Evaluate 'minute' field (0-59). */
   for (;;)
   {
      if ((step_size != 0) ||
          ((ptr = get_time_number(ptr, str_number, cmd_fp)) != NULL))
      {
         if (*ptr == ',')
         {
            if (str_number[0] == '*')
            {
               if ((str_number[1] == '\0') && (first_number == -1))
               {
                  if ((step_size == 1) && (continuous == YES))
                  {
#ifdef HAVE_LONG_LONG
                     te->continuous_minute = ALL_MINUTES;
#else
                     int i;

                     for (i = 0; i < 60; i++)
                     {
                        bitset(te->continuous_minute, i);
                     }
#endif
                  }
                  else
                  {
#ifdef HAVE_LONG_LONG
                     te->minute = ALL_MINUTES;
#else
                     int i;

                     for (i = 0; i < 60; i++)
                     {
                        bitset(te->minute, i);
                     }
#endif
                     continuous = YES;
                  }
               }
               else
               {
                  update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                _("Combination of '*' and other numeric values in minute field not possible. Ignoring time entry."));
                  return(INCORRECT);
               }
            }
            else
            {
               if (str_number[1] == '\0')
               {
                  number = str_number[0] - '0';
               }
               else
               {
                  if (str_number[0] > '5')
                  {
                     update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                   _("Possible values for minute field : 0-59. Ignoring time entry!"));
                     return(INCORRECT);
                  }
                  number = ((str_number[0] - '0') * 10) + str_number[1] - '0';
               }
               if (first_number == -1)
               {
#ifdef HAVE_LONG_LONG
                  te->minute |= bit_array_long[number];
#else
                  bitset(te->minute, number);
#endif
               }
               else
               {
                  if (number < first_number)
                  {
                     update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                   _("In minute field the second number (%d) cannot be less then the first number (%d) when specifying a range. Ignoring time entry!"),
                                   number, first_number);
                     return(INCORRECT);
                  }
                  else
                  {
                     int i;

                     if ((step_size == 1) && (continuous == YES))
                     {
                        for (i = first_number; i <= number; i = i + step_size)
                     {
#ifdef HAVE_LONG_LONG
                           te->continuous_minute |= bit_array_long[i];
#else
                           bitset(te->continuous_minute, i);
#endif
                        }
                        step_size = 0;
                     }
                     else
                     {
                        if (step_size == 0)
                        {
                           for (i = first_number; i <= number; i++)
                           {
#ifdef HAVE_LONG_LONG
                              te->minute |= bit_array_long[i];
#else
                              bitset(te->minute, i);
#endif
                           }
                        }
                        else
                        {
                           for (i = first_number; i <= number; i = i + step_size)
                        {
#ifdef HAVE_LONG_LONG
                              te->minute |= bit_array_long[i];
#else
                              bitset(te->minute, i);
#endif
                           }
                           step_size = 0;
                        }
                     }
                  }
                  first_number = -1;
               }
            }
            ptr++;
         }
         else if (*ptr == '-')
              {
                 if (str_number[1] == '\0')
                 {
                    first_number = str_number[0] - '0';
                 }
                 else
                 {
                    if (str_number[0] > '5')
                    {
                       update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Possible values for minute field : 0-59. Ignoring time entry!"));
                       return(INCORRECT);
                    }
                    first_number = ((str_number[0] - '0') * 10) +
                                   str_number[1] - '0';
                 }
                 ptr++;
              }
         else if ((*ptr == ' ') || (*ptr == '\t'))
              {
                 if (str_number[0] == '*')
                 {
                    if (str_number[1] == '\0')
                    {
                       int i = 0;

                       if ((step_size == 1) && (continuous == YES))
                       {
                          do
                          {
#ifdef HAVE_LONG_LONG
                             te->continuous_minute |= bit_array_long[i];
#else
                             bitset(te->continuous_minute, i);
#endif
                             i = i + step_size;
                          } while (i < 60);
                       }
                       else
                       {
                          if (step_size == 0)
                          {
                             step_size = 1;
                          }
                          do
                          {
#ifdef HAVE_LONG_LONG
                             te->minute |= bit_array_long[i];
#else
                             bitset(te->minute, i);
#endif
                             i = i + step_size;
                          } while (i < 60);
                          continuous = YES;
                       }
                       step_size = 0;
                    }
                    else
                    {
                       update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Combination of '*' and other numeric values in minute field not possible. Ignoring time entry."));
                       return(INCORRECT);
                    }
                 }
                 else
                 {
                    if (str_number[1] == '\0')
                    {
                       number = str_number[0] - '0';
                    }
                    else
                    {
                       if (str_number[0] > '5')
                       {
                          update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        _("Possible values for minute field : 0-59. Ignoring time entry!"));
                          return(INCORRECT);
                       }
                       number = ((str_number[0] - '0') * 10) +
                                str_number[1] - '0';
                    }
                    if (first_number == -1)
                    {
#ifdef HAVE_LONG_LONG
                       te->minute |= bit_array_long[number];
#else
                       bitset(te->minute, number);
#endif
                    }
                    else
                    {
                       if (number < first_number)
                       {
                          update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        _("In minute field the second number (%d) cannot be less then the first number (%d) when specifying a range. Ignoring time entry!"),
                                        number, first_number);
                          return(INCORRECT);
                       }
                       else
                       {
                          int i;

                          if ((step_size == 1) && (continuous == YES))
                          {
                             for (i = first_number; i <= number; i = i + step_size)
                             {
#ifdef HAVE_LONG_LONG
                                te->continuous_minute |= bit_array_long[i];
#else
                                bitset(te->continuous_minute, i);
#endif
                             }
                             step_size = 0;
                          }
                          else
                          {
                             if (step_size == 0)
                             {
                                for (i = first_number; i <= number; i++)
                                {
#ifdef HAVE_LONG_LONG
                                   te->minute |= bit_array_long[i];
#else
                                   bitset(te->minute, i);
#endif
                                }
                             }
                             else
                             {
                                for (i = first_number; i <= number; i = i + step_size)
                                {
#ifdef HAVE_LONG_LONG
                                   te->minute |= bit_array_long[i];
#else
                                   bitset(te->minute, i);
#endif
                                }
                                step_size = 0;
                             }
                             continuous = YES;
                          }
                       }
                       first_number = -1;
                    }
                 }
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 break;
              }
         else if (*ptr == '\0')
              {
                 update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               _("Premature end of time entry. Ignoring time entry."));
                 return(INCORRECT);
              }
         else if (*ptr == '/')
              {
                 char tmp_str_number[3];

                 tmp_str_number[0] = str_number[0];
                 tmp_str_number[1] = str_number[1];
                 tmp_str_number[2] = str_number[2];
                 ptr++;
                 if ((ptr = get_time_number(ptr, str_number, cmd_fp)) == NULL)
                 {
                    return(INCORRECT);
                 }
                 if (str_number[1] == '\0')
                 {
                    if (isdigit((int)str_number[0]) == 0)
                    {
                       update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Division by non numeric value <%c>. Ignoring time entry."),
                                     str_number[0]);
                       return(INCORRECT);
                    }
                    step_size = str_number[0] - '0';
                 }
                 else
                 {
                    step_size = ((str_number[0] - '0') * 10) +
                                str_number[1] - '0';
                 }
                 if ((step_size == 0) || (step_size > 59))
                 {
                    update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  _("Invalid step size %d in minute field. Ignoring time entry."),
                                  step_size);
                    return(INCORRECT);
                 }
                 if (step_size == 1)
                 {
                    continuous = NO;
                 }
                 str_number[0] = tmp_str_number[0];
                 str_number[1] = tmp_str_number[1];
                 str_number[2] = tmp_str_number[2];
              }
              else
              {
                 update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               _("Unable to handle time entry `%s'. Ignoring time entry."),
                               time_str);
                 return(INCORRECT);
              }
      }
      else
      {
         return(INCORRECT);
      }
   } /* for (;;) */

   /* Evaluate 'hour' field (0-23). */
   step_size = 0;
   for (;;)
   {
      if ((step_size != 0) ||
          ((ptr = get_time_number(ptr, str_number, cmd_fp)) != NULL))
      {
         if (*ptr == ',')
         {
            if (str_number[0] == '*')
            {
               if ((str_number[1] == '\0') && (first_number == -1))
               {
                  te->hour = ALL_HOURS;
               }
               else
               {
                  update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                _("Combination of '*' and other numeric values in hour field not possible. Ignoring time entry."));
                  return(INCORRECT);
               }
            }
            else
            {
               if (str_number[1] == '\0')
               {
                  number = str_number[0] - '0';
               }
               else
               {
                  if ((str_number[0] > '2') ||
                      ((str_number[0] == '2') && (str_number[1] > '3')))
                  {
                     update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                   _("Possible values for hour field : 0-23. Ignoring time entry!"));
                     return(INCORRECT);
                  }
                  number = ((str_number[0] - '0') * 10) + str_number[1] - '0';
               }
               if (first_number == -1)
               {
                  te->hour |= bit_array[number];
               }
               else
               {
                  if (number < first_number)
                  {
                     update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                   _("In hour field the second number (%d) cannot be less then the first number (%d) when specifying a range. Ignoring time entry!"),
                                   number, first_number);
                     return(INCORRECT);
                  }
                  else
                  {
                     int i;

                     if (step_size == 0)
                     {
                        for (i = first_number; i <= number; i++)
                        {
                           te->hour |= bit_array[i];
                        }
                     }
                     else
                     {
                        for (i = first_number; i <= number; i = i + step_size)
                        {
                           te->hour |= bit_array[i];
                        }
                        step_size = 0;
                     }
                  }
                  first_number = -1;
               }
            }
            ptr++;
         }
         else if (*ptr == '-')
              {
                 if (str_number[1] == '\0')
                 {
                    first_number = str_number[0] - '0';
                 }
                 else
                 {
                    if ((str_number[0] > '2') ||
                        ((str_number[0] == '2') && (str_number[1] > '3')))
                    {
                       update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Possible values for hour field : 0-23. Ignoring time entry!"));
                       return(INCORRECT);
                    }
                    first_number = ((str_number[0] - '0') * 10) +
                                   str_number[1] - '0';
                 }
                 ptr++;
              }
         else if ((*ptr == ' ') || (*ptr == '\t'))
              {
                 if (str_number[0] == '*')
                 {
                    if (str_number[1] == '\0')
                    {
                       if (step_size == 0)
                       {
                          te->hour = ALL_HOURS;
                       }
                       else
                       {
                          int i;

                          for (i = 0; i < 24; i = i + step_size)
                          {
                             te->hour |= bit_array[i];
                          }
                          step_size = 0;
                       }
                    }
                    else
                    {
                       update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Combination of '*' and other numeric values in hour field not possible. Ignoring time entry."));
                       return(INCORRECT);
                    }
                 }
                 else
                 {
                    if (str_number[1] == '\0')
                    {
                       number = str_number[0] - '0';
                    }
                    else
                    {
                       if ((str_number[0] > '2') ||
                           ((str_number[0] == '2') && (str_number[1] > '3')))
                       {
                          update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        _("Possible values for hour field : 0-23. Ignoring time entry!"));
                          return(INCORRECT);
                       }
                       number = ((str_number[0] - '0') * 10) +
                                str_number[1] - '0';
                    }
                    if (first_number == -1)
                    {
                       te->hour |= bit_array[number];
                    }
                    else
                    {
                       if (number < first_number)
                       {
                          update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        _("In hour field the second number (%d) cannot be less then the first number (%d) when specifying a range. Ignoring time entry!"),
                                        number, first_number);
                          return(INCORRECT);
                       }
                       else
                       {
                          int i;

                          if (step_size == 0)
                          {
                             for (i = first_number; i <= number; i++)
                             {
                                te->hour |= bit_array[i];
                             }
                          }
                          else
                          {
                             for (i = first_number; i <= number; i = i + step_size)
                             {
                                te->hour |= bit_array[i];
                             }
                             step_size = 0;
                          }
                       }
                       first_number = -1;
                    }
                 }
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 break;
              }
         else if (*ptr == '\0')
              {
                 update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               _("Premature end of time entry. Ignoring time entry."));
                 return(INCORRECT);
              }
         else if (*ptr == '/')
              {
                 char tmp_str_number[3];

                 tmp_str_number[0] = str_number[0];
                 tmp_str_number[1] = str_number[1];
                 tmp_str_number[2] = str_number[2];
                 ptr++;
                 if ((ptr = get_time_number(ptr, str_number, cmd_fp)) == NULL)
                 {
                    return(INCORRECT);
                 }
                 if (str_number[1] == '\0')
                 {
                    if (isdigit((int)str_number[0]) == 0)
                    {
                       update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Division by non numeric value <%c>. Ignoring time entry."),
                                     str_number[0]);
                       return(INCORRECT);
                    }
                    step_size = str_number[0] - '0';
                 }
                 else
                 {
                    step_size = ((str_number[0] - '0') * 10) +
                                str_number[1] - '0';
                 }
                 if ((step_size == 0) || (step_size > 23))
                 {
                    update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  _("Invalid step size %d in hour field. Ignoring time entry."),
                                  step_size);
                    return(INCORRECT);
                 }
                 str_number[0] = tmp_str_number[0];
                 str_number[1] = tmp_str_number[1];
                 str_number[2] = tmp_str_number[2];
              }
              else
              {
                 update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               _("Unable to handle time entry `%s'. Ignoring time entry."),
                               time_str);
                 return(INCORRECT);
              }
      }
      else
      {
         return(INCORRECT);
      }
   } /* for (;;) */
   
   /* Evaluate 'day of month' field (1-31). */
   step_size = 0;
   for (;;)
   {
      if ((step_size != 0) ||
          ((ptr = get_time_number(ptr, str_number, cmd_fp)) != NULL))
      {
         if (*ptr == ',')
         {
            if (str_number[0] == '*')
            {
               if ((str_number[1] == '\0') && (first_number == -1))
               {
                  te->day_of_month = ALL_DAY_OF_MONTH;
               }
               else
               {
                  update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                _("Combination of '*' and other numeric values in day of month field not possible. Ignoring time entry."));
                  return(INCORRECT);
               }
            }
            else
            {
               if (str_number[1] == '\0')
               {
                  if (str_number[0] == '0')
                  {
                     update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                   _("Possible values for day of month field : 1-31. Ignoring time entry!"));
                     return(INCORRECT);
                  }
                  number = str_number[0] - '0';
               }
               else
               {
                  if ((str_number[0] > '3') ||
                      ((str_number[0] == '3') && (str_number[1] > '1')))
                  {
                     update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                   _("Possible values for day of month field : 1-31. Ignoring time entry!"));
                     return(INCORRECT);
                  }
                  number = ((str_number[0] - '0') * 10) + str_number[1] - '0';
               }
               if (first_number == -1)
               {
                  te->day_of_month |= bit_array[number - 1];
               }
               else
               {
                  if (number < first_number)
                  {
                     update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                   _("In day of month field the second number (%d) cannot be less then the first number (%d) when specifying a range. Ignoring time entry!"),
                                   number, first_number);
                     return(INCORRECT);
                  }
                  else
                  {
                     int i;

                     if (step_size == 0)
                     {
                        for (i = first_number; i <= number; i++)
                        {
                           te->day_of_month |= bit_array[i - 1];
                        }
                     }
                     else
                     {
                        for (i = first_number; i <= number; i = i + step_size)
                        {
                           te->day_of_month |= bit_array[i - 1];
                        }
                        step_size = 0;
                     }
                  }
                  first_number = -1;
               }
            }
            ptr++;
         }
         else if (*ptr == '-')
              {
                 if (str_number[1] == '\0')
                 {
                    if (str_number[0] == '0')
                    {
                       update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Possible values for day of month field : 1-31. Ignoring time entry!"));
                       return(INCORRECT);
                    }
                    first_number = str_number[0] - '0';
                 }
                 else
                 {
                    if ((str_number[0] > '3') ||
                        ((str_number[0] == '3') && (str_number[1] > '1')))
                    {
                       update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Possible values for day of month field : 1-31. Ignoring time entry!"));
                       return(INCORRECT);
                    }
                    first_number = ((str_number[0] - '0') * 10) +
                                   str_number[1] - '0';
                 }
                 ptr++;
              }
         else if ((*ptr == ' ') || (*ptr == '\t'))
              {
                 if (str_number[0] == '*')
                 {
                    if (str_number[1] == '\0')
                    {
                       if (step_size == 0)
                       {
                          te->day_of_month = ALL_DAY_OF_MONTH;
                       }
                       else
                       {
                          int i;

                          for (i = 0; i <= 31; i = i + step_size)
                          {
                             te->day_of_month |= bit_array[i];
                          }
                          step_size = 0;
                       }
                    }
                    else
                    {
                       update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Combination of '*' and other numeric values in day of month field not possible. Ignoring time entry."));
                       return(INCORRECT);
                    }
                 }
                 else
                 {
                    if (str_number[1] == '\0')
                    {
                       if (str_number[0] == '0')
                       {
                          update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        _("Possible values for day of month field : 1-31. Ignoring time entry!"));
                          return(INCORRECT);
                       }
                       number = str_number[0] - '0';
                    }
                    else
                    {
                       if ((str_number[0] > '3') ||
                           ((str_number[0] == '3') && (str_number[1] > '1')))
                       {
                          update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        _("Possible values for day of month field : 1-31. Ignoring time entry!"));
                          return(INCORRECT);
                       }
                       number = ((str_number[0] - '0') * 10) +
                                str_number[1] - '0';
                    }
                    if (first_number == -1)
                    {
                       te->day_of_month |= bit_array[number - 1];
                    }
                    else
                    {
                       if (number < first_number)
                       {
                          update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        _("In day of month field the second number (%d) cannot be less then the first number (%d) when specifying a range. Ignoring time entry!"),
                                        number, first_number);
                          return(INCORRECT);
                       }
                       else
                       {
                          int i;

                          if (step_size == 0)
                          {
                             for (i = first_number; i <= number; i++)
                             {
                                te->day_of_month |= bit_array[i - 1];
                             }
                          }
                          else
                          {
                             for (i = first_number; i <= number; i = i + step_size)
                             {
                                te->day_of_month |= bit_array[i - 1];
                             }
                             step_size = 0;
                          }
                       }
                       first_number = -1;
                    }
                 }
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 break;
              }
         else if (*ptr == '\0')
              {
                 update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               _("Premature end of time entry. Ignoring time entry."));
                 return(INCORRECT);
              }
         else if (*ptr == '/')
              {
                 char tmp_str_number[3];

                 tmp_str_number[0] = str_number[0];
                 tmp_str_number[1] = str_number[1];
                 tmp_str_number[2] = str_number[2];
                 ptr++;
                 if ((ptr = get_time_number(ptr, str_number, cmd_fp)) == NULL)
                 {
                    return(INCORRECT);
                 }
                 if (str_number[1] == '\0')
                 {
                    if (isdigit((int)str_number[0]) == 0)
                    {
                       update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Division by non numeric value <%c>. Ignoring time entry."),
                                     str_number[0]);
                       return(INCORRECT);
                    }
                    step_size = str_number[0] - '0';
                 }
                 else
                 {
                    step_size = ((str_number[0] - '0') * 10) +
                                str_number[1] - '0';
                 }
                 if ((step_size == 0) || (step_size > 31))
                 {
                    update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  _("Invalid step size %d in day of month field. Ignoring time entry."),
                                  step_size);
                    return(INCORRECT);
                 }
                 str_number[0] = tmp_str_number[0];
                 str_number[1] = tmp_str_number[1];
                 str_number[2] = tmp_str_number[2];
              }
              else
              {
                 update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               _("Unable to handle time entry `%s'. Ignoring time entry."),
                               time_str);
                 return(INCORRECT);
              }
      }
      else
      {
         return(INCORRECT);
      }
   } /* for (;;) */
   
   /* Evaluate 'month' field (1-12). */
   step_size = 0;
   for (;;)
   {
      if ((step_size != 0) ||
          ((ptr = get_time_number(ptr, str_number, cmd_fp)) != NULL))
      {
         if (*ptr == ',')
         {
            if (str_number[0] == '*')
            {
               if ((str_number[1] == '\0') && (first_number == -1))
               {
                  te->month = ALL_MONTH;
               }
               else
               {
                  update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                _("Combination of '*' and other numeric values in time string field not possible. Ignoring time entry."));
                  return(INCORRECT);
               }
            }
            else
            {
               if (str_number[1] == '\0')
               {
                  if (str_number[0] == '0')
                  {
                     update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                   _("Possible values for month field : 1-12. Ignoring time entry!"));
                     return(INCORRECT);
                  }
                  number = str_number[0] - '0';
               }
               else
               {
                  if ((str_number[0] > '1') || (str_number[1] > '2'))
                  {
                     update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                   _("Possible values for month field : 1-12. Ignoring time entry!"));
                     return(INCORRECT);
                  }
                  number = ((str_number[0] - '0') * 10) + str_number[1] - '0';
               }
               if (first_number == -1)
               {
                  te->month |= bit_array[number - 1];
               }
               else
               {
                  if (number < first_number)
                  {
                     update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                   _("In month field the second number (%d) cannot be less then the first number (%d) when specifying a range. Ignoring time entry!"),
                                   number, first_number);
                     return(INCORRECT);
                  }
                  else
                  {
                     int i;

                     if (step_size == 0)
                     {
                        for (i = first_number; i <= number; i++)
                        {
                           te->month |= bit_array[i - 1];
                        }
                     }
                     else
                     {
                        for (i = first_number; i <= number; i = i + step_size)
                        {
                           te->month |= bit_array[i - 1];
                        }
                        step_size = 0;
                     }
                  }
                  first_number = -1;
               }
            }
            ptr++;
         }
         else if (*ptr == '-')
              {
                 if (str_number[1] == '\0')
                 {
                    if (str_number[0] == '0')
                    {
                       update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Possible values for month field : 1-12. Ignoring time entry!"));
                       return(INCORRECT);
                    }
                    first_number = str_number[0] - '0';
                 }
                 else
                 {
                    if ((str_number[0] > '1') || (str_number[1] > '2'))
                    {
                       update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Possible values for month field : 1-12. Ignoring time entry!"));
                       return(INCORRECT);
                    }
                    first_number = ((str_number[0] - '0') * 10) +
                                   str_number[1] - '0';
                 }
                 ptr++;
              }
         else if ((*ptr == ' ') || (*ptr == '\t'))
              {
                 if (str_number[0] == '*')
                 {
                    if (str_number[1] == '\0')
                    {
                       if (step_size == 0)
                       {
                          te->month = ALL_MONTH;
                       }
                       else
                       {
                          int i;

                          for (i = 0; i <= 12; i = i + step_size)
                          {
                             te->month |= bit_array[i];
                          }
                          step_size = 0;
                       }
                    }
                    else
                    {
                       update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Combination of '*' and other numeric values in time string field not possible. Ignoring time entry."));
                       return(INCORRECT);
                    }
                 }
                 else
                 {
                    if (str_number[1] == '\0')
                    {
                       if (str_number[0] == '0')
                       {
                          update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        _("Possible values for month field : 1-12. Ignoring time entry!"));
                          return(INCORRECT);
                       }
                       number = str_number[0] - '0';
                    }
                    else
                    {
                       if ((str_number[0] > '1') || (str_number[1] > '2'))
                       {
                          update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        _("Possible values for month field : 1-12. Ignoring time entry!"));
                          return(INCORRECT);
                       }
                       number = ((str_number[0] - '0') * 10) +
                                str_number[1] - '0';
                    }
                    if (first_number == -1)
                    {
                       te->month |= bit_array[number - 1];
                    }
                    else
                    {
                       if (number < first_number)
                       {
                          update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        _("In month field the second number (%d) cannot be less then the first number (%d) when specifying a range. Ignoring time entry!"),
                                        number, first_number);
                          return(INCORRECT);
                       }
                       else
                       {
                          int i;

                          if (step_size == 0)
                          {
                             for (i = first_number; i <= number; i++)
                             {
                                te->month |= bit_array[i - 1];
                             }
                          }
                          else
                          {
                             for (i = first_number; i <= number; i = i + step_size)
                             {
                                te->month |= bit_array[i - 1];
                             }
                             step_size = 0;
                          }
                       }
                       first_number = -1;
                    }
                 }
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 break;
              }
         else if (*ptr == '\0')
              {
                 update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               _("Premature end of time entry. Ignoring time entry."));
                 return(INCORRECT);
              }
         else if (*ptr == '/')
              {
                 char tmp_str_number[3];

                 tmp_str_number[0] = str_number[0];
                 tmp_str_number[1] = str_number[1];
                 tmp_str_number[2] = str_number[2];
                 ptr++;
                 if ((ptr = get_time_number(ptr, str_number, cmd_fp)) == NULL)
                 {
                    return(INCORRECT);
                 }
                 if (str_number[1] == '\0')
                 {
                    if (isdigit((int)str_number[0]) == 0)
                    {
                       update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Division by non numeric value <%c>. Ignoring time entry."),
                                     str_number[0]);
                       return(INCORRECT);
                    }
                    step_size = str_number[0] - '0';
                 }
                 else
                 {
                    step_size = ((str_number[0] - '0') * 10) +
                                str_number[1] - '0';
                 }
                 if ((step_size == 0) || (step_size > 12))
                 {
                    update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  _("Invalid step size %d in month field. Ignoring time entry."),
                                  step_size);
                    return(INCORRECT);
                 }
                 str_number[0] = tmp_str_number[0];
                 str_number[1] = tmp_str_number[1];
                 str_number[2] = tmp_str_number[2];
              }
              else
              {
                 update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               _("Unable to handle time entry `%s'. Ignoring time entry."),
                               time_str);
                 return(INCORRECT);
              }
      }
      else
      {
         return(INCORRECT);
      }
   } /* for (;;) */

   /* Evaluate 'day of week' field (1-7). */
   step_size = 0;
   for (;;)
   {
      if ((step_size != 0) ||
          ((ptr = get_time_number(ptr, str_number, cmd_fp)) != NULL))
      {
         if (*ptr == ',')
         {
            if (str_number[0] == '*')
            {
               if ((str_number[1] == '\0') && (first_number == -1))
               {
                  te->day_of_week = ALL_DAY_OF_WEEK;
               }
               else
               {
                  update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                _("Combination of '*' and other numeric values in time string field not possible. Ignoring time entry."));
                  return(INCORRECT);
               }
            }
            else
            {
               if (str_number[1] == '\0')
               {
                  if ((str_number[0] < '1') || (str_number[0] > '7'))
                  {
                     update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                   _("Possible values for day of week field : 1-7. Ignoring time entry!"));
                     return(INCORRECT);
                  }
                  number = str_number[0] - '0';
               }
               else
               {
                  update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                _("Possible values for day of week field : 1-7. Ignoring time entry!"));
                  return(INCORRECT);
               }
               if (first_number == -1)
               {
                  te->day_of_week |= bit_array[number - 1];
               }
               else
               {
                  if (number < first_number)
                  {
                     update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                   _("In day of week field the second number (%d) cannot be less then the first number (%d) when specifying a range. Ignoring time entry!"),
                                   number, first_number);
                     return(INCORRECT);
                  }
                  else
                  {
                     int i;

                     if (step_size == 0)
                     {
                        for (i = first_number; i <= number; i++)
                        {
                           te->day_of_week |= bit_array[i - 1];
                        }
                     }
                     else
                     {
                        for (i = first_number; i <= number; i = i + step_size)
                        {
                           te->day_of_week |= bit_array[i - 1];
                        }
                        step_size = 0;
                     }
                  }
                  first_number = -1;
               }
            }
            ptr++;
         }
         else if (*ptr == '-')
              {
                 if (str_number[1] == '\0')
                 {
                    if ((str_number[0] < '1') || (str_number[0] > '7'))
                    {
                       update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Possible values for day of week field : 1-7. Ignoring time entry!"));
                       return(INCORRECT);
                    }
                    first_number = str_number[0] - '0';
                 }
                 else
                 {
                    update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  _("Possible values for day of week field : 1-7. Ignoring time entry!"));
                    return(INCORRECT);
                 }
                 ptr++;
              }
         else if ((*ptr == ' ') || (*ptr == '\t') || (*ptr == '\0'))
              {
                 if (str_number[0] == '*')
                 {
                    if (str_number[1] == '\0')
                    {
                       if (step_size == 0)
                       {
                          te->day_of_week = ALL_DAY_OF_WEEK;
                       }
                       else
                       {
                          int i;

                          for (i = 0; i <= 7; i = i + step_size)
                          {
                             te->day_of_week |= bit_array[i];
                          }
                          step_size = 0;
                       }
                    }
                    else
                    {
                       update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Combination of '*' and other numeric values in time string field not possible. Ignoring time entry."));
                       return(INCORRECT);
                    }
                 }
                 else
                 {
                    if (str_number[1] == '\0')
                    {
                       if ((str_number[0] < '1') || (str_number[0] > '7'))
                       {
                          update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        _("Possible values for day of week field : 1-7. Ignoring time entry!"));
                          return(INCORRECT);
                       }
                       number = str_number[0] - '0';
                    }
                    else
                    {
                       update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Possible values for day of week field : 1-7. Ignoring time entry!"));
                       return(INCORRECT);
                    }
                    if (first_number == -1)
                    {
                       te->day_of_week |= bit_array[number - 1];
                    }
                    else
                    {
                       if (number < first_number)
                       {
                          update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        _("In day of week field the second number (%d) cannot be less then the first number (%d) when specifying a range. Ignoring time entry!"),
                                        number, first_number);
                          return(INCORRECT);
                       }
                       else
                       {
                          int i;

                          if (step_size == 0)
                          {
                             for (i = first_number; i <= number; i++)
                             {
                                te->day_of_week |= bit_array[i - 1];
                             }
                          }
                          else
                          {
                             for (i = first_number; i <= number; i = i + step_size)
                             {
                                te->day_of_week |= bit_array[i - 1];
                             }
                             step_size = 0;
                          }
                       }
                       first_number = -1;
                    }
                 }
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 break;
              }
         else if (*ptr == '/')
              {
                 char tmp_str_number[3];

                 tmp_str_number[0] = str_number[0];
                 tmp_str_number[1] = str_number[1];
                 tmp_str_number[2] = str_number[2];
                 ptr++;
                 if ((ptr = get_time_number(ptr, str_number, cmd_fp)) == NULL)
                 {
                    return(INCORRECT);
                 }
                 if (str_number[1] == '\0')
                 {
                    if (isdigit((int)str_number[0]) == 0)
                    {
                       update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Division by non numeric value <%c>. Ignoring time entry."),
                                     str_number[0]);
                       return(INCORRECT);
                    }
                    step_size = str_number[0] - '0';
                 }
                 else
                 {
                    update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  _("Invalid step size %d in day of week field. Ignoring time entry."),
                                  step_size);
                    return(INCORRECT);
                 }
                 if ((step_size == 0) || (step_size > 7))
                 {
                    update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  _("Invalid step size %d in day of week field. Ignoring time entry."),
                                  step_size);
                    return(INCORRECT);
                 }
                 str_number[0] = tmp_str_number[0];
                 str_number[1] = tmp_str_number[1];
                 str_number[2] = tmp_str_number[2];
              }
              else
              {
                 update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               _("Unable to handle time entry `%s'. Ignoring time entry."),
                               time_str);
                 return(INCORRECT);
              }
      }
      else
      {
         return(INCORRECT);
      }
   } /* for (;;) */

   return(SUCCESS);
}


/*########################## check_time_str() ###########################*/
int
check_time_str(char *time_str, FILE *cmd_fp)
{
   int  continuous = YES,
        first_number = -1,
        number,
        step_size = 0;
   char *ptr = time_str,
        str_number[3];

   if ((time_str[0] == 'e') && (time_str[1] == 'x') && (time_str[2] == 't') &&
       (time_str[3] == 'e') && (time_str[4] == 'r') && (time_str[5] == 'n') &&
       (time_str[6] == 'a') && (time_str[7] == 'l'))
   {
      return(SUCCESS);
   }

   /* Evaluate 'minute' field (0-59). */
   for (;;)
   {
      if ((step_size != 0) ||
          ((ptr = get_time_number(ptr, str_number, cmd_fp)) != NULL))
      {
         if (*ptr == ',')
         {
            if (str_number[0] == '*')
            {
               if ((str_number[1] == '\0') && (first_number == -1))
               {
                  if ((step_size == 1) && (continuous == YES))
                  {
                     /* continuous_minute = ALL_MINUTES */;
                  }
                  else
                  {
                     /* minute = ALL_MINUTES */;
                     continuous = YES;
                  }
               }
               else
               {
                  update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                _("Combination of '*' and other numeric values in minute field not possible."));
                  return(INCORRECT);
               }
            }
            else
            {
               if (str_number[1] != '\0')
               {
                  if (str_number[0] > '5')
                  {
                     update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                   _("Possible values for minute field : 0-59."));
                     return(INCORRECT);
                  }
                  number = ((str_number[0] - '0') * 10) + str_number[1] - '0';
               }
               else
               {
                  number = str_number[0] - '0';
               }
               if (first_number != -1)
               {
                  if (number < first_number)
                  {
                     update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                   _("In minute field the second number (%d) cannot be less then the first number (%d) when specifying a range. Ignoring time entry!"),
                                   number, first_number);
                     return(INCORRECT);
                  }
                  step_size = 0;
                  first_number = -1;
               }
            }
            ptr++;
         }
         else if (*ptr == '-')
              {
                 if (str_number[1] == '\0')
                 {
                    first_number = str_number[0] - '0';
                 }
                 else
                 {
                    if (str_number[0] > '5')
                    {
                       update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Possible values for minute field : 0-59."));
                       return(INCORRECT);
                    }
                    first_number = ((str_number[0] - '0') * 10) +
                                   str_number[1] - '0';
                 }
                 ptr++;
              }
         else if ((*ptr == ' ') || (*ptr == '\t'))
              {
                 if (str_number[0] == '*')
                 {
                    if (str_number[1] == '\0')
                    {
                       if ((step_size == 1) && (continuous == YES))
                       {
                          /* continuous_minute = ALL_MINUTES */;
                       }
                       else
                       {
                          /* minute = ALL_MINUTES */;
                          continuous = YES;
                       }
                       step_size = 0;
                    }
                    else
                    {
                       update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Combination of '*' and other numeric values in minute field not possible."));
                       return(INCORRECT);
                    }
                 }
                 else
                 {
                    if (str_number[1] != '\0')
                    {
                       if (str_number[0] > '5')
                       {
                          update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        _("Possible values for minute field : 0-59."));
                          return(INCORRECT);
                       }
                       number = ((str_number[0] - '0') * 10) + str_number[1] - '0';
                    }
                    else
                    {
                       number = str_number[0] - '0';
                    }
                    if (first_number != -1)
                    {
                       if (number < first_number)
                       {
                          update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        _("In minute field the second number (%d) cannot be less then the first number (%d) when specifying a range. Ignoring time entry!"),
                                        number, first_number);
                          return(INCORRECT);
                       }
                       if ((step_size == 1) && (continuous == YES))
                       {
                          /* store continuous_minute */;
                       }
                       else
                       {
                          /* store minute */;
                          continuous = YES;
                       }
                       step_size = 0;
                       first_number = -1;
                    }
                 }
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 break;
              }
         else if (*ptr == '\0')
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               _("Premature end of time entry."));
                 return(INCORRECT);
              }
         else if (*ptr == '/')
              {
                 char tmp_str_number[3];

                 tmp_str_number[0] = str_number[0];
                 tmp_str_number[1] = str_number[1];
                 tmp_str_number[2] = str_number[2];
                 ptr++;
                 if ((ptr = get_time_number(ptr, str_number, cmd_fp)) == NULL)
                 {
                    return(INCORRECT);
                 }
                 if (str_number[1] == '\0')
                 {
                    if (isdigit((int)str_number[0]) == 0)
                    {
                       update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Division by non numeric value <%c>."),
                                     str_number[0]);
                       return(INCORRECT);
                    }
                    step_size = str_number[0] - '0';
                 }
                 else
                 {
                    step_size = ((str_number[0] - '0') * 10) +
                                str_number[1] - '0';
                 }
                 if ((step_size == 0) || (step_size > 59))
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  _("Invalid step size %d in minute field."),
                                  step_size);
                    return(INCORRECT);
                 }
                 if (step_size == 1)
                 {
                    continuous = NO;
                 }
                 str_number[0] = tmp_str_number[0];
                 str_number[1] = tmp_str_number[1];
                 str_number[2] = tmp_str_number[2];
              }
              else
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               _("Unable to handle time entry `%s'."), time_str);
                 return(INCORRECT);
              }
      }
      else
      {
         return(INCORRECT);
      }
   } /* for (;;) */

   /* Evaluate 'hour' field (0-23). */
   step_size = 0;
   for (;;)
   {
      if ((step_size != 0) ||
          ((ptr = get_time_number(ptr, str_number, cmd_fp)) != NULL))
      {
         if (*ptr == ',')
         {
            if (str_number[0] == '*')
            {
               if ((str_number[1] == '\0') && (first_number == -1))
               {
                  /* ALL_HOURS */;
               }
               else
               {
                  update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                _("Combination of '*' and other numeric values in hour field not possible."));
                  return(INCORRECT);
               }
            }
            else
            {
               if (str_number[1] != '\0')
               {
                  if ((str_number[0] > '2') ||
                      ((str_number[0] == '2') && (str_number[1] > '3')))
                  {
                     update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                   _("Possible values for hour field : 0-23."));
                     return(INCORRECT);
                  }
                  number = ((str_number[0] - '0') * 10) + str_number[1] - '0';
               }
               else
               {
                  number = str_number[0] - '0';
               }
               if (first_number != -1)
               {
                  if (number < first_number)
                  {
                     update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                   _("In hour field the second number (%d) cannot be less then the first number (%d) when specifying a range. Ignoring time entry!"),
                                   number, first_number);
                     return(INCORRECT);
                  }
                  step_size = 0;
                  first_number = -1;
               }
            }
            ptr++;
         }
         else if (*ptr == '-')
              {
                 if (str_number[1] == '\0')
                 {
                    first_number = str_number[0] - '0';
                 }
                 else
                 {
                    if ((str_number[0] > '2') ||
                        ((str_number[0] == '2') && (str_number[1] > '3')))
                    {
                       update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Possible values for hour field : 0-23."));
                       return(INCORRECT);
                    }
                    first_number = ((str_number[0] - '0') * 10) +
                                   str_number[1] - '0';
                 }
                 ptr++;
              }
         else if ((*ptr == ' ') || (*ptr == '\t'))
              {
                 if (str_number[0] == '*')
                 {
                    if (str_number[1] == '\0')
                    {
                       step_size = 0;
                    }
                    else
                    {
                       update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Combination of '*' and other numeric values in hour field not possible."));
                       return(INCORRECT);
                    }
                 }
                 else
                 {
                    if (str_number[1] != '\0')
                    {
                       if ((str_number[0] > '2') ||
                           ((str_number[0] == '2') && (str_number[1] > '3')))
                       {
                          update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        _("Possible values for hour field : 0-23."));
                          return(INCORRECT);
                       }
                       number = ((str_number[0] - '0') * 10) + str_number[1] - '0';
                    }
                    else
                    {
                       number = str_number[0] - '0';
                    }
                    if (first_number != -1)
                    {
                       if (number < first_number)
                       {
                          update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        _("In hour field the second number (%d) cannot be less then the first number (%d) when specifying a range. Ignoring time entry!"),
                                        number, first_number);
                          return(INCORRECT);
                       }
                       step_size = 0;
                       first_number = -1;
                    }
                 }
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 break;
              }
         else if (*ptr == '\0')
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               _("Premature end of time entry."));
                 return(INCORRECT);
              }
         else if (*ptr == '/')
              {
                 char tmp_str_number[3];

                 tmp_str_number[0] = str_number[0];
                 tmp_str_number[1] = str_number[1];
                 tmp_str_number[2] = str_number[2];
                 ptr++;
                 if ((ptr = get_time_number(ptr, str_number, cmd_fp)) == NULL)
                 {
                    return(INCORRECT);
                 }
                 if (str_number[1] == '\0')
                 {
                    if (isdigit((int)str_number[0]) == 0)
                    {
                       update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Division by non numeric value <%c>."),
                                     str_number[0]);
                       return(INCORRECT);
                    }
                    step_size = str_number[0] - '0';
                 }
                 else
                 {
                    step_size = ((str_number[0] - '0') * 10) +
                                str_number[1] - '0';
                 }
                 if ((step_size == 0) || (step_size > 23))
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  _("Invalid step size %d in hour field."),
                                  step_size);
                    return(INCORRECT);
                 }
                 str_number[0] = tmp_str_number[0];
                 str_number[1] = tmp_str_number[1];
                 str_number[2] = tmp_str_number[2];
              }
              else
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               _("Unable to handle time entry `%s'."), time_str);
                 return(INCORRECT);
              }
      }
      else
      {
         return(INCORRECT);
      }
   } /* for (;;) */
   
   /* Evaluate 'day of month' field (1-31). */
   step_size = 0;
   for (;;)
   {
      if ((step_size != 0) ||
          ((ptr = get_time_number(ptr, str_number, cmd_fp)) != NULL))
      {
         if (*ptr == ',')
         {
            if (str_number[0] == '*')
            {
               if ((str_number[1] == '\0') && (first_number == -1))
               {
                  /* ALL_DAY_OF_MONTH */;
               }
               else
               {
                  update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                _("Combination of '*' and other numeric values in day of month field not possible."));
                  return(INCORRECT);
               }
            }
            else
            {
               if (str_number[1] == '\0')
               {
                  if (str_number[0] == '0')
                  {
                     update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                   _("Possible values for day of month field : 1-31."));
                     return(INCORRECT);
                  }
                  number = str_number[0] - '0';
               }
               else
               {
                  if ((str_number[0] > '3') ||
                      ((str_number[0] == '3') && (str_number[1] > '1')))
                  {
                     update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                   _("Possible values for day of month field : 1-31."));
                     return(INCORRECT);
                  }
                  number = ((str_number[0] - '0') * 10) + str_number[1] - '0';
               }
               if (first_number != -1)
               {
                  if (number < first_number)
                  {
                     update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                   _("In day of month field the second number (%d) cannot be less then the first number (%d) when specifying a range. Ignoring time entry!"),
                                   number, first_number);
                     return(INCORRECT);
                  }
                  step_size = 0;
                  first_number = -1;
               }
            }
            ptr++;
         }
         else if (*ptr == '-')
              {
                 if (str_number[1] == '\0')
                 {
                    if (str_number[0] == '0')
                    {
                       update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Possible values for day of month field : 1-31."));
                       return(INCORRECT);
                    }
                    first_number = str_number[0] - '0';
                 }
                 else
                 {
                    if ((str_number[0] > '3') ||
                        ((str_number[0] == '3') && (str_number[1] > '1')))
                    {
                       update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Possible values for day of month field : 1-31."));
                       return(INCORRECT);
                    }
                    first_number = ((str_number[0] - '0') * 10) +
                                   str_number[1] - '0';
                 }
                 ptr++;
              }
         else if ((*ptr == ' ') || (*ptr == '\t'))
              {
                 if (str_number[0] == '*')
                 {
                    if (str_number[1] == '\0')
                    {
                       step_size = 0;
                    }
                    else
                    {
                       update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Combination of '*' and other numeric values in day of month field not possible."));
                       return(INCORRECT);
                    }
                 }
                 else
                 {
                    if (str_number[1] == '\0')
                    {
                       if (str_number[0] == '0')
                       {
                          update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        _("Possible values for day of month field : 1-31."));
                          return(INCORRECT);
                       }
                       number = str_number[0] - '0';
                    }
                    else
                    {
                       if ((str_number[0] > '3') ||
                           ((str_number[0] == '3') && (str_number[1] > '1')))
                       {
                          update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        _("Possible values for day of month field : 1-31."));
                          return(INCORRECT);
                       }
                       number = ((str_number[0] - '0') * 10) + str_number[1] - '0';
                    }
                    if (first_number != -1)
                    {
                       if (number < first_number)
                       {
                          update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        _("In day of month field the second number (%d) cannot be less then the first number (%d) when specifying a range. Ignoring time entry!"),
                                        number, first_number);
                          return(INCORRECT);
                       }
                       step_size = 0;
                       first_number = -1;
                    }
                 }
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 break;
              }
         else if (*ptr == '\0')
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               _("Premature end of time entry."));
                 return(INCORRECT);
              }
         else if (*ptr == '/')
              {
                 char tmp_str_number[3];

                 tmp_str_number[0] = str_number[0];
                 tmp_str_number[1] = str_number[1];
                 tmp_str_number[2] = str_number[2];
                 ptr++;
                 if ((ptr = get_time_number(ptr, str_number, cmd_fp)) == NULL)
                 {
                    return(INCORRECT);
                 }
                 if (str_number[1] == '\0')
                 {
                    if (isdigit((int)str_number[0]) == 0)
                    {
                       update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Division by non numeric value <%c>."),
                                     str_number[0]);
                       return(INCORRECT);
                    }
                    step_size = str_number[0] - '0';
                 }
                 else
                 {
                    step_size = ((str_number[0] - '0') * 10) +
                                str_number[1] - '0';
                 }
                 if ((step_size == 0) || (step_size > 31))
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  _("Invalid step size %d in day of month field."),
                                  step_size);
                    return(INCORRECT);
                 }
                 str_number[0] = tmp_str_number[0];
                 str_number[1] = tmp_str_number[1];
                 str_number[2] = tmp_str_number[2];
              }
              else
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               _("Unable to handle time entry `%s'."), time_str);
                 return(INCORRECT);
              }
      }
      else
      {
         return(INCORRECT);
      }
   } /* for (;;) */
   
   /* Evaluate 'month' field (1-12). */
   step_size = 0;
   for (;;)
   {
      if ((step_size != 0) ||
          ((ptr = get_time_number(ptr, str_number, cmd_fp)) != NULL))
      {
         if (*ptr == ',')
         {
            if (str_number[0] == '*')
            {
               if ((str_number[1] == '\0') && (first_number == -1))
               {
                  /* ALL_MONTH */;
               }
               else
               {
                  update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                _("Combination of '*' and other numeric values in time string field not possible."));
                  return(INCORRECT);
               }
            }
            else
            {
               if (str_number[1] == '\0')
               {
                  if (str_number[0] == '0')
                  {
                     update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                   _("Possible values for month field : 1-12."));
                     return(INCORRECT);
                  }
                  number = str_number[0] - '0';
               }
               else
               {
                  if ((str_number[0] > '1') || (str_number[1] > '2'))
                  {
                     update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                   _("Possible values for month field : 1-12."));
                     return(INCORRECT);
                  }
                  number = ((str_number[0] - '0') * 10) + str_number[1] - '0';
               }
               if (first_number != -1)
               {
                  if (number < first_number)
                  {
                     update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                   _("In month field the second number (%d) cannot be less then the first number (%d) when specifying a range. Ignoring time entry!"),
                                   number, first_number);
                     return(INCORRECT);
                  }
                  step_size = 0;
                  first_number = -1;
               }
            }
            ptr++;
         }
         else if (*ptr == '-')
              {
                 if (str_number[1] == '\0')
                 {
                    if (str_number[0] == '0')
                    {
                       update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Possible values for month field : 1-12."));
                       return(INCORRECT);
                    }
                    first_number = str_number[0] - '0';
                 }
                 else
                 {
                    if ((str_number[0] > '1') || (str_number[1] > '2'))
                    {
                       update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Possible values for month field : 1-12."));
                       return(INCORRECT);
                    }
                    first_number = ((str_number[0] - '0') * 10) +
                                   str_number[1] - '0';
                 }
                 ptr++;
              }
         else if ((*ptr == ' ') || (*ptr == '\t'))
              {
                 if (str_number[0] == '*')
                 {
                    if (str_number[1] == '\0')
                    {
                       step_size = 0;
                    }
                    else
                    {
                       update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Combination of '*' and other numeric values in time string field not possible."));
                       return(INCORRECT);
                    }
                 }
                 else
                 {
                    if (str_number[1] == '\0')
                    {
                       if (str_number[0] == '0')
                       {
                          update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        _("Possible values for month field : 1-12."));
                          return(INCORRECT);
                       }
                       number = str_number[0] - '0';
                    }
                    else
                    {
                       if ((str_number[0] > '1') || (str_number[1] > '2'))
                       {
                          update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        _("Possible values for month field : 1-12."));
                          return(INCORRECT);
                       }
                       number = ((str_number[0] - '0') * 10) + str_number[1] - '0';
                    }
                    if (first_number != -1)
                    {
                       if (number < first_number)
                       {
                          update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        _("In month field the second number (%d) cannot be less then the first number (%d) when specifying a range. Ignoring time entry!"),
                                        number, first_number);
                          return(INCORRECT);
                       }
                       step_size = 0;
                       first_number = -1;
                    }
                 }
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 break;
              }
         else if (*ptr == '\0')
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               _("Premature end of time entry."));
                 return(INCORRECT);
              }
         else if (*ptr == '/')
              {
                 char tmp_str_number[3];

                 tmp_str_number[0] = str_number[0];
                 tmp_str_number[1] = str_number[1];
                 tmp_str_number[2] = str_number[2];
                 ptr++;
                 if ((ptr = get_time_number(ptr, str_number, cmd_fp)) == NULL)
                 {
                    return(INCORRECT);
                 }
                 if (str_number[1] == '\0')
                 {
                    if (isdigit((int)str_number[0]) == 0)
                    {
                       update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Division by non numeric value <%c>."),
                                     str_number[0]);
                       return(INCORRECT);
                    }
                    step_size = str_number[0] - '0';
                 }
                 else
                 {
                    step_size = ((str_number[0] - '0') * 10) +
                                str_number[1] - '0';
                 }
                 if ((step_size == 0) || (step_size > 12))
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  _("Invalid step size %d in month field."),
                                  step_size);
                    return(INCORRECT);
                 }
                 str_number[0] = tmp_str_number[0];
                 str_number[1] = tmp_str_number[1];
                 str_number[2] = tmp_str_number[2];
              }
              else
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               _("Unable to handle time entry `%s'."), time_str);
                 return(INCORRECT);
              }
      }
      else
      {
         return(INCORRECT);
      }
   } /* for (;;) */

   /* Evaluate 'day of week' field (1-7). */
   step_size = 0;
   for (;;)
   {
      if ((step_size != 0) ||
          ((ptr = get_time_number(ptr, str_number, cmd_fp)) != NULL))
      {
         if (*ptr == ',')
         {
            if (str_number[0] == '*')
            {
               if ((str_number[1] == '\0') && (first_number == -1))
               {
                  /* ALL_DAY_OF_WEEK */;
               }
               else
               {
                  update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                _("Combination of '*' and other numeric values in time string field not possible."));
                  return(INCORRECT);
               }
            }
            else
            {
               if (str_number[1] == '\0')
               {
                  if ((str_number[0] < '1') || (str_number[0] > '7'))
                  {
                     update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                   _("Possible values for day of week field : 1-7."));
                     return(INCORRECT);
                  }
                  number = str_number[0] - '0';
               }
               else
               {
                  update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                _("Possible values for day of week field : 1-7."));
                  return(INCORRECT);
               }
               if (first_number != -1)
               {
                  if (number < first_number)
                  {
                     update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                   _("In day of week field the second number (%d) cannot be less then the first number (%d) when specifying a range. Ignoring time entry!"),
                                   number, first_number);
                     return(INCORRECT);
                  }
                  step_size = 0;
                  first_number = -1;
               }
            }
            ptr++;
         }
         else if (*ptr == '-')
              {
                 if (str_number[1] == '\0')
                 {
                    if ((str_number[0] < '1') || (str_number[0] > '7'))
                    {
                       update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Possible values for day of week field : 1-7."));
                       return(INCORRECT);
                    }
                    first_number = str_number[0] - '0';
                 }
                 else
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  _("Possible values for day of week field : 1-7."));
                    return(INCORRECT);
                 }
                 ptr++;
              }
         else if ((*ptr == ' ') || (*ptr == '\t') || (*ptr == '\0'))
              {
                 if (str_number[0] == '*')
                 {
                    if (str_number[1] == '\0')
                    {
                       step_size = 0;
                    }
                    else
                    {
                       update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Combination of '*' and other numeric values in time string field not possible."));
                       return(INCORRECT);
                    }
                 }
                 else
                 {
                    if (str_number[1] == '\0')
                    {
                       if ((str_number[0] < '1') || (str_number[0] > '7'))
                       {
                          update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        _("Possible values for day of week field : 1-7."));
                          return(INCORRECT);
                       }
                       number = str_number[0] - '0';
                    }
                    else
                    {
                       update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Possible values for day of week field : 1-7."));
                       return(INCORRECT);
                    }
                    if (first_number != -1)
                    {
                       if (number < first_number)
                       {
                          update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        _("In day of week field the second number (%d) cannot be less then the first number (%d) when specifying a range. Ignoring time entry!"),
                                        number, first_number);
                          return(INCORRECT);
                       }
                       step_size = 0;
                       first_number = -1;
                    }
                 }
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 break;
              }
         else if (*ptr == '/')
              {
                 char tmp_str_number[3];

                 tmp_str_number[0] = str_number[0];
                 tmp_str_number[1] = str_number[1];
                 tmp_str_number[2] = str_number[2];
                 ptr++;
                 if ((ptr = get_time_number(ptr, str_number, cmd_fp)) == NULL)
                 {
                    return(INCORRECT);
                 }
                 if (str_number[1] == '\0')
                 {
                    if (isdigit((int)str_number[0]) == 0)
                    {
                       update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     _("Division by non numeric value <%c>."),
                                     str_number[0]);
                       return(INCORRECT);
                    }
                    step_size = str_number[0] - '0';
                 }
                 else
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  _("Invalid step size %d in day of week field."),
                                  step_size);
                    return(INCORRECT);
                 }
                 if ((step_size == 0) || (step_size > 7))
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  _("Invalid step size %d in day of week field."),
                                  step_size);
                    return(INCORRECT);
                 }
                 str_number[0] = tmp_str_number[0];
                 str_number[1] = tmp_str_number[1];
                 str_number[2] = tmp_str_number[2];
              }
              else
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               _("Unable to handle time entry `%s'."), time_str);
                 return(INCORRECT);
              }
      }
      else
      {
         return(INCORRECT);
      }
   } /* for (;;) */

   return(SUCCESS);
}


/*++++++++++++++++++++++++++ get_time_number() ++++++++++++++++++++++++++*/
static char *
get_time_number(char *ptr, char *str_number, FILE *cmd_fp)
{
   int i = 0;

   while ((*ptr != ' ') && (*ptr != ',') && (*ptr != '-') &&
          (*ptr != '\0') && (*ptr != '\t') && (i < 3) && (*ptr != '/'))
   {
      if ((isdigit((int)(*ptr)) == 0) && (*ptr != '*'))
      {
         if ((*ptr > ' ') && (*ptr < '?'))
         {
            update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                          _("Invalid character %c [%d] in time string! Ignoring time entry."),
                          *ptr, *ptr);
         }
         else
         {
            update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                          _("Invalid character [%d] in time string! Ignoring time entry."),
                          *ptr);
         }
         return(NULL);
      }
      str_number[i] = *ptr;
      ptr++; i++;
   }

   if (i == 0)
   {
      update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                    _("Hmm, no values entered. Ignoring time entry."));
      return(NULL);
   }
   else if (i > 2)
        {
           update_db_log(ERROR_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                         _("Hmm, number with more then two digits. Ignoring time entry."));
           return(NULL);
        }

   str_number[i] = '\0';
   return(ptr);
}
