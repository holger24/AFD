/*
 *  change_name.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2022 Deutscher Wetterdienst (DWD),
 *                            Tobias Freyberg <>
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
 **   change_name - changes a file name according a given rule
 **
 ** SYNOPSIS
 **   void change_name(char         *orig_file_name,
 **                    char         *filter,
 **                    char         *rename_to_rule,
 **                    char         *new_name,
 **                    int          max_new_name_length,
 **                    int          *counter_fd,
 **                    int          **counter,
 **                    unsigned int job_id)
 **
 ** DESCRIPTION
 **   'change_name' takes the 'filter' and analyses the 'orig_file_name'
 **   to chop them in to pieces. Then the pieces are glued together as
 **   described in 'rename_to_rule' and stored in to 'new_name'. To
 **   insert special terms in the new filename the '%' sign is
 **   interpreted as followed:
 **     %ax   - Alternateing character and x can be one of the following:
 **               b  - binary (0 or 1)
 **               dn - character alternates up to n decimal numbers
 **               hn - character alternates up to n hexadecimal numbers
 **     %*n   - address n-th asterisk
 **     %?n   - address n-th question mark
 **     %on   - n-th character from the original filename
 **     %On-m - range n to m of characters from original file
 **             name. n='^' - from the beginning
 **                   m='$' - to the end
 **     %n    - generate a 4 character unique number
 **     %tx   - insert the actual time (x = as strftime())
 **             a - short day "Tue",           A - long day "Tuesday",
 **             b - short month "Jan",         B - long month "January",
 **             d - day of month [01,31],      m - month [01,12],
 **             j - day of the year [001,366], y - year [01,99],
 **             Y - year 1997,                 R - Sunday week number [00,53],
 **             w - weekday [0=Sunday,6],      W - Monday week number [00,53],
 **             H - hour [00,23],              M - minute [00,59],
 **             S - second [00,60],
 **             U - Unix time, number of seconds since 00:00:00 01/01/1970 UTC
 **     %T[+|-|*|/|%]xS|M|H|d - Time modifier
 **            |     |   |
 **            |     |   +----> Time unit: S - second
 **            |     |                     M - minute
 **            |     |                     H - hour
 **            |     |                     d - day
 **            |     +--------> Time modifier
 **            +--------------> How time modifier should be applied.
 **     %h    - insert the hostname
 **     %H    - insert the hostname without domain
 **     %%    - the '%' sign   
 **     \     - are ignored
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   T.Freyberg
 **
 ** HISTORY
 **   03.02.1997 T.Freyberg Created
 **   15.11.1997 H.Kiehl    Insert option day of the year.
 **   07.03.2001 H.Kiehl    Put in some more syntax checks.
 **   09.10.2001 H.Kiehl    Insert option for inserting hostname.
 **   16.06.2002 H.Kiehl    Insert option Unix time.
 **   28.06.2003 H.Kiehl    Added time modifier.
 **   10.04.2005 H.Kiehl    Added binary alternating numbers.
 **   14.02.2008 H.Kiehl    Added decimal and hexadecimal alternating numbers.
 **   09.05.2008 H.Kiehl    For %o or %O check that the original file name
 **                         is long enough.
 **   11.11.2010 H.Kiehl    Added R, w and W to %t parameter.
 **   17.02.2011 H.Kiehl    Allow the following case: nsc????.*  *nsc????.*
 **                         That is more then one asterix if we have only
 **                         one in the filter part.
 **   02.01.2013 H.Kiehl    To be able to check the maximum storage length
 **                         of the new name added the parameter
 **                         max_new_name_length.
 **   08.05.2014 H.Kiehl    When calculating the date for %t option use
 **                         localtime() instead of gmtime(). Thanks to
 **                         Bill Welch reporting this.
 **   12.03.2015 H.Kiehl    Catch the case when there are no * and ? in
 **                         the filter.
 **   10.04.2016 H.Kiehl    Added %H to insert hostname without domain.
 **   24.01.2023 H.Kiehl    Allow up to 20 *.
 **                         Check if we go beyond the number of * and/or
 **                         ?.
 **
 */
DESCR__E_M3

#include <stdio.h>           /*                                          */
#include <string.h>          /* strcpy(), strlen()                       */
#include <stdlib.h>          /* atoi(), getenv()                         */
#include <ctype.h>           /* isdigit()                                */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>          /* gethostname()                            */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include <time.h>

/* #define _DEBUG 1 */
#define MAX_ASTERIX_SIGNS    20
#define MAX_QUESTIONER_SIGNS 50

/* External global variables. */
extern char *p_work_dir;

/* Local function prototypes. */
static int  get_alternate_number(unsigned int);


/*############################# change_name() ###########################*/
void
change_name(char         *orig_file_name,
            char         *filter,
            char         *rename_to_rule,
            char         *new_name,
            int          max_new_name_length,
            int          *counter_fd,
            int          **counter,
            unsigned int job_id)
{
   char   buffer[MAX_FILENAME_LENGTH],
          string[MAX_INT_LENGTH + 1],
          *ptr_asterix[MAX_ASTERIX_SIGNS] =
          {
             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
          },
          *ptr_questioner[MAX_QUESTIONER_SIGNS] =
          {
             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
          },
          *ptr_oldname,
          *ptr_oldname_tmp,
          *ptr_filter,
          *ptr_filter_tmp,
          *ptr_rule = NULL,
          *ptr_newname,
          time_mod_sign = '+',
          *tmp_ptr;
   int    act_asterix = 0,
          act_questioner = 0,
          alternate,
          count_asterix = 0,
          count_questioner = 0,
          end,
          i,
          number,
          original_filename_length = -1,
          pattern_found = NO,
          start,
          tmp_count_questioner;
   time_t time_buf,
          time_modifier = 0;

   /* Copy original filename to a temporary buffer. */
   (void)strcpy(buffer, orig_file_name);

   ptr_oldname = ptr_oldname_tmp = buffer;
   ptr_filter  = ptr_filter_tmp  = filter;

   /* Taking 'orig_file_name' in pieces like 'filter'. */
   while (*ptr_filter != '\0')
   {
      switch (*ptr_filter)
      {
         case '*' : /* '*' in filter detected -> mark position in array. */
            if (count_asterix < MAX_ASTERIX_SIGNS)
            {
               ptr_asterix[count_asterix++] = ptr_oldname;
            }
            else
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "There are more then %d '*' signs in filter %s. Will not change name.",
                          MAX_ASTERIX_SIGNS, filter);
               (void)strcpy(new_name, orig_file_name);

               return;
            }
            ptr_filter++;
            break;

         case '?' : /* '?' in filter -> skip one character in both words */
                    /*                  and mark questioner.             */
            ptr_filter++;
            if (count_questioner < MAX_QUESTIONER_SIGNS)
            {
               ptr_questioner[count_questioner++] = ptr_oldname++;
            }
            else
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "There are more then %d '?' signs in filter %s. Will not change name.",
                          MAX_QUESTIONER_SIGNS, filter, rename_to_rule);
               (void)strcpy(new_name, orig_file_name);

               return;
            }
            break;

         default  : /* Search the char, found in filter, in oldname. */
            pattern_found = NO;
            tmp_count_questioner = 0;

            /* Mark the latest position. */
            ptr_filter_tmp = ptr_filter;
            while (*ptr_filter_tmp != '\0')
            {
               /* Position the orig_file_name pointer to the next      */
               /* place where filter pointer and orig_file_name pointer*/
               /* are equal.                                           */
               while ((*ptr_oldname != *ptr_filter_tmp) &&
                      (*ptr_oldname != '\0'))
               {
                  ptr_oldname++;
               }
               /* Mark the latest position. */
               ptr_oldname_tmp = ptr_oldname;

               /* Test the rest of the pattern. */
               while ((*ptr_filter_tmp != '*') && (*ptr_filter_tmp != '\0'))
               {
                  if ((*ptr_filter_tmp == *ptr_oldname_tmp) ||
                      (*ptr_filter_tmp == '?'))
                  {
                     if (*ptr_filter_tmp == '?')
                     {
                        if ((count_questioner + tmp_count_questioner) < MAX_QUESTIONER_SIGNS)
                        {
                           /* Mark questioner. */
                           ptr_questioner[count_questioner + tmp_count_questioner++] = ptr_oldname_tmp;
                        }
                        else
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      "There are more then %d '?' signs in filter %s. Will not change name.",
                                      MAX_QUESTIONER_SIGNS, filter, rename_to_rule);
                           (void)strcpy(new_name, orig_file_name);

                           return;
                        }
                     }
                     ptr_filter_tmp++;
                     ptr_oldname_tmp++;
                     pattern_found = YES;
                  }
                  else
                  {
                     pattern_found = NO;
                     break;
                  }
               }
               if ((pattern_found == YES) && (*ptr_filter_tmp == '\0') &&
                   (*ptr_oldname_tmp != '\0'))
               {
                  pattern_found = NO;
               }
               if (pattern_found == YES)
               {
                  /* Mark end of string 'ptr_asterix[count_asterix]' */
                  /* and position the pointer to next char.          */
                  *ptr_oldname = '\0';
                  ptr_oldname = ptr_oldname_tmp;
                  ptr_filter = ptr_filter_tmp;
                  count_questioner += tmp_count_questioner;
                  break;
               }
               else
               {
                  if (tmp_count_questioner > 0)
                  {
                     tmp_count_questioner = 0;
                  }
                  if (*ptr_oldname == '\0')
                  {
                     /* Since all do not match and there are no asterix  */
                     /* and/or questioners, no need to continue chopping */
                     /* up filter in different pieces.                   */
                     while (*ptr_filter != '\0')
                     {
                        ptr_filter++;
                     }
                     break;
                  }
                  ptr_filter_tmp = ptr_filter;
                  ptr_oldname++;
               }
            }
            break;
      }
   }

#ifdef _DEBUG
   system_log(INFO_SIGN, NULL, 0, _("found %d *"), count_asterix);
   for (i = 0; i < count_asterix; i++)
   {
      system_log(INFO_SIGN, NULL, 0, "ptr_asterix[%d]=%s", i, ptr_asterix[i]);
   }
   system_log(INFO_SIGN, NULL, 0, _("found %d ?"), count_questioner);
   for (i = 0; i < count_questioner; i++)
   {
      system_log(INFO_SIGN, NULL, 0, "ptr_questioner[%d]=%c",
                 i, *ptr_questioner[i]);
   }
#endif

   /* Create new_name as specified in rename_to_rule. */
   ptr_rule = rename_to_rule;
   ptr_newname = new_name;
   while ((*ptr_rule != '\0') &&
          (((ptr_newname + 1) - new_name) < max_new_name_length))
   {
      /* Work trough the rule and paste the name. */
      switch (*ptr_rule)
      {
         case '%' : /* Locate the '%' -> handle the rule. */
            ptr_rule++;
            switch (*ptr_rule)
            {
               case '*' : /* Insert the addressed ptr_asterix. */
                  ptr_rule++;

                  /* Test for number. */
                  i = 0;
                  while ((isdigit((int)(*(ptr_rule + i)))) &&
                         (i < MAX_INT_LENGTH))
                  {
                     string[i] = *(ptr_rule + i);
                     i++;
                  }
                  ptr_rule += i;
                  string[i] = '\0';
                  number = atoi(string) - 1; /* Human count from 1 and computer from 0. */
                  if ((number >= count_asterix) || (number < 0))
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                _("illegal '*' addressed: %d (%s %s) #%x"),
                                number + 1, filter, rename_to_rule, job_id);
                  }
                  else
                  {
                     tmp_ptr = ptr_newname + strlen(ptr_asterix[number]);
                     if (((tmp_ptr + 1) - new_name) < max_new_name_length)
                     {
                        (void)strcpy(ptr_newname, ptr_asterix[number]);
                        ptr_newname = tmp_ptr;
                     }
                     else
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   "Storage for storing new name not large enough (%d > %d). #%x",
                                   ((tmp_ptr + 1) - new_name),
                                   max_new_name_length, job_id);
                     }
                  }
                  break;

               case '?' : /* Insert the addressed ptr_questioner. */
                  ptr_rule++;

                  /* Test for number. */
                  i = 0;
                  while ((isdigit((int)(*(ptr_rule + i)))) &&
                         (i < MAX_INT_LENGTH))
                  {
                     string[i] = *(ptr_rule + i);
                     i++;
                  }
                  ptr_rule += i;
                  string[i] = '\0';
                  number = atoi(string) - 1; /* Human count from 1 and computer from 0. */
                  if ((number >= count_questioner) || (number < 0))
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                _("illegal '?' addressed: %d (%s %s) #%x"),
                                number + 1, filter, rename_to_rule, job_id);

                  }
                  else
                  {
                     if (((ptr_newname + 1) - new_name) < max_new_name_length)
                     {
                        *ptr_newname = *ptr_questioner[number];
                        ptr_newname++;
                     }
                     else
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   "Storage for storing new name not large enough (%d > %d). #%x",
                                   ((ptr_newname + 1) - new_name),
                                   max_new_name_length, job_id);
                     }
                  }
                  break;

               case 'o' : /* Insert the addressed character from orig_file_name. */
                  ptr_rule++;

                  /* Test for number. */
                  i = 0;
                  while ((isdigit((int)(*(ptr_rule + i)))) &&
                         (i < MAX_INT_LENGTH))
                  {
                     string[i] = *(ptr_rule + i);
                     i++;
                  }
                  if (original_filename_length == -1)
                  {
                     original_filename_length = strlen(orig_file_name);
                  }
                  ptr_rule += i;
                  string[i] = '\0';
                  number = atoi(string) - 1; /* Human count from 1 and computer from 0. */
                  if (number == -1)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                _("No numeric value set for %%o option: (%s %s) #%x"),
                                filter, rename_to_rule, job_id);
                  }
                  else
                  {
                     if (number <= original_filename_length)
                     {
                        if (((ptr_newname + 1) - new_name) < max_new_name_length)
                        {
                           *ptr_newname++ = *(orig_file_name + number);
                        }
                        else
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      "Storage for storing new name not large enough (%d > %d). #%x",
                                      ((ptr_newname + 1) - new_name),
                                      max_new_name_length, job_id);
                        }
                     }
                  }
                  break;

               case 'O' : /* Insert the addressed range of characters from orig_file_name. */
                  ptr_rule++;

                  /* Read from start. */
                  if (*ptr_rule == '^') /* Means start from the beginning. */
                  {
                     ptr_oldname = orig_file_name;
                     ptr_rule++;
                     start = 0;
                  }
                  else /* Read the start number. */
                  {
                     i = 0;
                     while ((isdigit((int)(*(ptr_rule + i)))) &&
                            (i < MAX_INT_LENGTH))
                     {
                        string[i] = *(ptr_rule + i);
                        i++;
                     }
                     ptr_rule += i;
                     string[i] = '\0';
                     start = atoi(string) - 1; /* Human count from 1 and computer from 0. */
                     ptr_oldname = orig_file_name + start;
                  }
                  if (*ptr_rule == '-')
                  {
                     if (original_filename_length == -1)
                     {
                        original_filename_length = strlen(orig_file_name);
                     }
                     if (start <= original_filename_length)
                     {
                        /* Skip the '-'. */
                        ptr_rule++;
                        /* Read the end. */
                        if (*ptr_rule == '$') /* Means up to the end. */
                        {
                           i = strlen(ptr_oldname);
                           if (((ptr_newname + i + 1) - new_name) < max_new_name_length)
                           {
                              (void)strcpy(ptr_newname, ptr_oldname);
                              ptr_newname += i;
                           }
                           else
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                         "Storage for storing new name not large enough (%d > %d). #%x",
                                         ((ptr_newname + i + 1) - new_name),
                                         max_new_name_length, job_id);
                           }
                           ptr_rule++;
                        }
                        else
                        {
                           i = 0;
                           while ((isdigit((int)(*(ptr_rule + i)))) &&
                                  (i < MAX_INT_LENGTH))
                           {
                              string[i] = *(ptr_rule + i);
                              i++;
                           }
                           ptr_rule += i;
                           string[i] = '\0';
                           end = atoi(string) - 1; /* Human count from 1 and computer from 0. */
                           if (end > original_filename_length)
                           {
                              end = original_filename_length;
                           }
                           number = end - start + 1;  /* Including the first and last. */
                           if (number < 0)
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                         _("The start (%d) and end (%d) range do not make sense in rule %s. Start must be larger then end! #%x"),
                                         start, end, rename_to_rule, job_id);
                           }
                           else
                           {
                              if (((ptr_newname + number + 1) - new_name) < max_new_name_length)
                              {
                                 my_strncpy(ptr_newname, ptr_oldname, number + 1);
                                 ptr_newname += number;
                              }
                              else
                              {
                                 system_log(WARN_SIGN, __FILE__, __LINE__,
                                            "Storage for storing new name not large enough (%d > %d). #%x",
                                            ((ptr_newname + number + 1) - new_name),
                                            max_new_name_length, job_id);
                              }
                           }
                        }
                     }
                  }
                  else
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                _("There is no end range specified for rule %s #%x"),
                                rename_to_rule, job_id);
                  }
                  break;

               case 'n' : /* Generate a unique number 4 characters. */
                  ptr_rule++;
                  if (((ptr_newname + 4 + 1) - new_name) < max_new_name_length)
                  {
                     if (*counter_fd == -1)
                     {
                        if ((*counter_fd = open_counter_file(COUNTER_FILE, counter)) < 0)
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      _("Failed to open counter file, ignoring %n. #%x"),
                                      job_id);
                           break;
                        }
                     }
                     (void)next_counter(*counter_fd, *counter, MAX_MSG_PER_SEC);
                     (void)snprintf(ptr_newname, 5, "%04x", **counter);
                     ptr_newname += 4;
                  }
                  else
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "Storage for storing new name not large enough (%d > %d). #%x",
                                ((ptr_newname + 4 + 1) - new_name),
                                max_new_name_length, job_id);
                  }
                  break;

               case 'h' : /* Insert local hostname. */
               case 'H' : /* Insert local hostname. */
                  {
                     char hostname[40];

                     ptr_rule++;
                     if (gethostname(hostname, 40) == -1)
                     {
                        char *p_hostname;

                        if ((p_hostname = getenv("HOSTNAME")) != NULL)
                        {
                           (void)my_strncpy(hostname, p_hostname, 40);
                           if (*ptr_rule == 'H')
                           {
                              i = 0;
                              while ((hostname[i] != '\0') &&
                                     (hostname[i] != '.'))
                              {
                                 i++;
                              }
                              if (hostname[i] == '.')
                              {
                                 hostname[i] = '\0';
                              }
                           }
                           else
                           {
                              i = strlen(hostname);
                           }
                           if (((ptr_newname + i + 1) - new_name) < max_new_name_length)
                           {
                              (void)strcpy(ptr_newname, hostname);
                              ptr_newname += i;
                           }
                           else
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                         "Storage for storing new name not large enough (%d > %d). #%x",
                                         ((ptr_newname + i + 1) - new_name),
                                         max_new_name_length, job_id);
                           }
                        }
                     }
                     else
                     {
                        if (*ptr_rule == 'H')
                        {
                           i = 0;
                           while ((hostname[i] != '\0') && (hostname[i] != '.'))
                           {
                              i++;
                           }
                           if (hostname[i] == '.')
                           {
                              hostname[i] = '\0';
                           }
                        }
                        else
                        {
                           i = strlen(hostname);
                        }
                        if (((ptr_newname + i + 1) - new_name) < max_new_name_length)
                        {
                           (void)strcpy(ptr_newname, hostname);
                           ptr_newname += i;
                        }
                        else
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      "Storage for storing new name not large enough (%d > %d). #%x",
                                      ((ptr_newname + i + 1) - new_name),
                                      max_new_name_length, job_id);
                        }
                     }
                  }
                  break;

               case 'T' : /* Time modifier. */
                  {
                     int time_unit;

                     ptr_rule++;
                     switch (*ptr_rule)
                     {
                        case '+' :
                        case '-' :
                        case '*' :
                        case '/' :
                        case '%' :
                           time_mod_sign = *ptr_rule;
                           ptr_rule++;
                           break;
                        default  :
                           time_mod_sign = '+';
                           break;
                     }
                     i = 0;
                     while ((isdigit((int)(*(ptr_rule + i)))) &&
                            (i < MAX_INT_LENGTH))
                     {
                        string[i] = *(ptr_rule + i);
                        i++;
                     }
                     ptr_rule += i;
                     if ((i > 0) && (i < MAX_INT_LENGTH))
                     {
                        string[i] = '\0';
                        time_modifier = atoi(string);
                     }
                     else
                     {
                        if (i == MAX_INT_LENGTH)
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      _("The time modifier specified for rule %s is to large. #%x"),
                                      rename_to_rule, job_id);
                        }
                        else
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      _("There is no time modifier specified for rule %s #%x"),
                                      rename_to_rule, job_id);
                        }
                        time_modifier = 0;
                     }
                     switch (*ptr_rule)
                     {
                        case 'S' : /* Second */
                           time_unit = 1;
                           ptr_rule++;
                           break;
                        case 'M' : /* Minute */
                           time_unit = 60;
                           ptr_rule++;
                           break;
                        case 'H' : /* Hour */
                           time_unit = 3600;
                           ptr_rule++;
                           break;
                        case 'd' : /* Day */
                           time_unit = 86400;
                           ptr_rule++;
                           break;
                        default :
                           time_unit = 1;
                           break;
                     }
                     if (time_modifier > 0)
                     {
                        time_modifier = time_modifier * time_unit;
                     }
                  }
                  break;

               case 't' : /* Insert the actual time as strftime. */
                  time_buf = time(NULL);
                  if (time_modifier > 0)
                  {
                     switch (time_mod_sign)
                     {
                        case '-' :
                           time_buf = time_buf - time_modifier;
                           break;
                        case '*' :
                           time_buf = time_buf * time_modifier;
                           break;
                        case '/' :
                           time_buf = time_buf / time_modifier;
                           break;
                        case '%' :
                           time_buf = time_buf % time_modifier;
                           break;
                        case '+' :
                        default :
                           time_buf = time_buf + time_modifier;
                           break;
                     }
                  }
                  ptr_rule++;
                  switch (*ptr_rule)
                  {
                     case 'a' : /* Short day of the week 'Tue'. */
                        number = strftime(ptr_newname,
                                          max_new_name_length - (ptr_newname - new_name),
                                          "%a", localtime(&time_buf));
                        break;
                     case 'A' : /* Long day of the week 'Tuesday'. */
                        number = strftime(ptr_newname,
                                          max_new_name_length - (ptr_newname - new_name),
                                          "%A", localtime(&time_buf));
                        break;
                     case 'b' : /* Short month 'Jan'. */
                        number = strftime(ptr_newname,
                                          max_new_name_length - (ptr_newname - new_name),
                                          "%b", localtime(&time_buf));
                        break;
                     case 'B' : /* Month 'January'. */
                        number = strftime(ptr_newname,
                                          max_new_name_length - (ptr_newname - new_name),
                                          "%B", localtime(&time_buf));
                        break;
                     case 'i' : /* Day of month [1,31]. */
                        number = strftime(ptr_newname,
                                          max_new_name_length - (ptr_newname - new_name),
                                          "%d", localtime(&time_buf));
                        if (*ptr_newname == '0')
                        {
                           *ptr_newname = *(ptr_newname + 1);
                           *(ptr_newname + 1) = '\0';
                           number = 1;
                        }
                        break;
                     case 'd' : /* Day of month [01,31]. */
                        number = strftime(ptr_newname,
                                          max_new_name_length - (ptr_newname - new_name),
                                          "%d", localtime(&time_buf));
                        break;
                     case 'j' : /* Day of year [001,366]. */
                        number = strftime(ptr_newname,
                                          max_new_name_length - (ptr_newname - new_name),
                                          "%j", localtime(&time_buf));
                        break;
                     case 'J' : /* Month [1,12]. */
                        number = strftime(ptr_newname,
                                          max_new_name_length - (ptr_newname - new_name),
                                          "%m", localtime(&time_buf));
                        if (*ptr_newname == '0')
                        {
                           *ptr_newname = *(ptr_newname + 1);
                           *(ptr_newname + 1) = '\0';
                           number = 1;
                        }
                        break;
                     case 'm' : /* Month [01,12]. */
                        number = strftime(ptr_newname,
                                          max_new_name_length - (ptr_newname - new_name),
                                          "%m", localtime(&time_buf));
                        break;
                     case 'R' : /* Sunday week number [00,53]. */
                        number = strftime(ptr_newname,
                                          max_new_name_length - (ptr_newname - new_name),
                                          "%U", localtime(&time_buf));
                        break;
                     case 'w' : /* Weekday [0=Sunday,6]. */
                        number = strftime(ptr_newname,
                                          max_new_name_length - (ptr_newname - new_name),
                                          "%w", localtime(&time_buf));
                        break;
                     case 'W' : /* Monday week number [00,53]. */
                        number = strftime(ptr_newname,
                                          max_new_name_length - (ptr_newname - new_name),
                                          "%W", localtime(&time_buf));
                        break;
                     case 'y' : /* Year 2 chars [01,99]. */
                        number = strftime(ptr_newname,
                                          max_new_name_length - (ptr_newname - new_name),
                                          "%y", localtime(&time_buf));
                        break;
                     case 'Y' : /* Year 4 chars 1997. */
                        number = strftime(ptr_newname,
                                          max_new_name_length - (ptr_newname - new_name),
                                          "%Y", localtime(&time_buf));
                        break;
                     case 'o' : /* Hour [0,23]. */
                        number = strftime(ptr_newname,
                                          max_new_name_length - (ptr_newname - new_name),
                                          "%H", localtime(&time_buf));
                        if (*ptr_newname == '0')
                        {
                           *ptr_newname = *(ptr_newname + 1);
                           *(ptr_newname + 1) = '\0';
                           number = 1;
                        }
                        break;
                     case 'H' : /* Hour [00,23]. */
                        number = strftime(ptr_newname,
                                          max_new_name_length - (ptr_newname - new_name),
                                          "%H", localtime(&time_buf));
                        break;
                     case 'M' : /* Minute [00,59]. */
                        number = strftime(ptr_newname,
                                          max_new_name_length - (ptr_newname - new_name),
                                          "%M", localtime(&time_buf));
                        break;
                     case 'S' : /* Second [00,59]. */
                        number = strftime(ptr_newname,
                                          max_new_name_length - (ptr_newname - new_name),
                                          "%S", localtime(&time_buf));
                        break;
                     case 'U' : /* Unix time. */
                        number = snprintf(ptr_newname,
                                          max_new_name_length - (ptr_newname - new_name),
#if SIZEOF_TIME_T == 4
                                          "%ld",
#else
                                          "%lld",
#endif
                                          (pri_time_t)time_buf);
                        break;
                     default  : /* Unknown character - ignore. */
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   _("Illegal time option (%c) in rule %s #%x"),
                                   *ptr_rule, rename_to_rule, job_id);
                        number = 0;
                        break;
                  }
                  ptr_newname += number;
                  ptr_rule++;
                  break;

               case '%' : /* Insert the '%' sign. */
                  if (((ptr_newname + 1 + 1) - new_name) < max_new_name_length)
                  {
                     *ptr_newname = '%';
                     ptr_newname++;
                  }
                  else
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "Storage for storing new name not large enough (%d > %d). #%x",
                                ((ptr_newname + 1 + 1) - new_name),
                                max_new_name_length, job_id);
                  }
                  ptr_rule++;
                  break;

               case 'a' : /* Insert an alternating character. */
                  ptr_rule++;
                  if (((ptr_newname + 1 + 1) - new_name) < max_new_name_length)
                  {
                     if ((alternate = get_alternate_number(job_id)) == INCORRECT)
                     {
                        alternate = 0;
                     }
                     switch (*ptr_rule)
                     {
                        case 'b' : /* Binary. */
                           if ((alternate % 2) == 0)
                           {
                              *ptr_newname = '0';
                           }
                           else
                           {
                                 *ptr_newname = '1';
                           }
                           ptr_rule++;
                           ptr_newname++;
                           break;

                        case 'd' : /* Decimal. */
                           ptr_rule++;
                           if (isdigit((int)*ptr_rule))
                           {
                              *ptr_newname = '0' + (alternate % (*ptr_rule + 1 - '0'));
                              ptr_rule++;
                              ptr_newname++;
                           }
                           else
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                         _("Illegal character (%c - not a decimal digit) found in rule %s #%x"),
                                         *ptr_rule, rename_to_rule, job_id);
                           }
                           break;

                        case 'h' : /* Hexadecimal. */
                           {
                              char ul_char; /* Upper/lower character. */

                              ptr_rule++;
                              if ((*ptr_rule >= '0') && (*ptr_rule <= '9'))
                              {
                                 number = *ptr_rule + 1 - '0';
                                 ul_char = 0; /* To silent compiler warnings. */
                              }
                              else if ((*ptr_rule >= 'A') && (*ptr_rule <= 'F'))
                                   {
                                      number = 10 + *ptr_rule + 1 - 'A';
                                      ul_char = 'A';
                                   }
                              else if ((*ptr_rule >= 'a') && (*ptr_rule <= 'f'))
                                   {
                                      number = 10 + *ptr_rule + 1 - 'a';
                                      ul_char = 'a';
                                   }
                                   else
                                   {
                                      system_log(WARN_SIGN, __FILE__, __LINE__,
                                                 _("Illegal character (%c - not a hexadecimal digit) found in rule %s #%x"),
                                                 *ptr_rule, rename_to_rule,
                                                 job_id);
                                      number = -1;
                                   }

                              if (number != -1)
                              {
                                 number = alternate % number;
                                 if (number >= 10)
                                 {
                                    *ptr_newname = ul_char + number - 10;
                                 }
                                 else
                                 {
                                    *ptr_newname = '0' + number;
                                 }
                                 ptr_rule++;
                                 ptr_newname++;
                              }
                           }
                           break;

                        default : /* Unknown character - ignore. */
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      _("Illegal character (%c) found in rule %s #%x"),
                                      *ptr_rule, rename_to_rule, job_id);
                           ptr_rule++;
                           break;
                     }
                  }
                  else
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "Storage for storing new name not large enough (%d > %d). #%x",
                                ((ptr_newname + 1 + 1) - new_name),
                                max_new_name_length, job_id);
                  }
                  break;

               default  : /* No action specified, write an error message. */
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             _("Illegal character (%d) behind %% sign in rule %s #%x"),
                             *ptr_rule, rename_to_rule, job_id);
                  ptr_rule++;
                  break;
            }
            break;

         case '*' : /* Locate the '*' -> insert the next 'ptr_asterix'. */
            if (act_asterix >= count_asterix)
            {
               if (count_asterix == 1)
               {
                  i = strlen(ptr_asterix[0]);
                  if (((ptr_newname + i + 1) - new_name) < max_new_name_length)
                  {
                     (void)strcpy(ptr_newname, ptr_asterix[0]);
                     ptr_newname += i;
                  }
                  else
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "Storage for storing new name not large enough (%d > %d). #%x",
                                ((ptr_newname + i + 1) - new_name),
                                max_new_name_length, job_id);
                  }
                  act_asterix++;
               }
               else
               {
                  system_log(WARN_SIGN, NULL, 0,
                             _("can not pass more '*' as found before -> ignored. #%x"),
                             job_id);
                  system_log(DEBUG_SIGN, NULL, 0,
                             "orig_file_name = %s | filter = %s | rename_to_rule = %s | new_name = %s",
                             orig_file_name, filter, rename_to_rule, new_name);
               }
            }
            else
            {
               i = strlen(ptr_asterix[act_asterix]);
               if (((ptr_newname + i + 1) - new_name) < max_new_name_length)
               {
                  (void)strcpy(ptr_newname, ptr_asterix[act_asterix]);
                  ptr_newname += i;
               }
               else
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Storage for storing new name not large enough (%d > %d). #%x",
                             ((ptr_newname + i + 1) - new_name),
                             max_new_name_length, job_id);
               }
               act_asterix++;
            }
            ptr_rule++;
            break;

         case '?' : /* Locate the '?' -> insert the next 'ptr_questioner' */
            if (act_questioner == count_questioner)
            {
               system_log(WARN_SIGN, NULL, 0,
                          _("can not pass more '?' as found before -> ignored. #%x"),
                          job_id);
               system_log(DEBUG_SIGN, NULL, 0,
                          "orig_file_name = %s | filter = %s | rename_to_rule = %s | new_name = %s",
                          orig_file_name, filter, rename_to_rule, new_name);
            }
            else
            {
               if (((ptr_newname + 1 + 1) - new_name) < max_new_name_length)
               {
                  *ptr_newname = *ptr_questioner[act_questioner];
                  ptr_newname++;
               }
               else
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Storage for storing new name not large enough (%d > %d). #%x",
                             ((ptr_newname + 1 + 1) - new_name),
                             max_new_name_length, job_id);
               }
               act_questioner++;
            }
            ptr_rule++;
            break;

         case '\\' : /* Ignore. */
            ptr_rule++;
            break;

         default  : /* Found an ordinary character -> append it. */
            if (((ptr_newname + 1 + 1) - new_name) < max_new_name_length)
            {
               *ptr_newname = *ptr_rule;
               ptr_newname++;
            }
            else
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Storage for storing new name not large enough (%d > %d). #%x",
                          ((ptr_newname + 1 + 1) - new_name),
                          max_new_name_length, job_id);
            }
            ptr_rule++;
            break;
      }
   }
   *ptr_newname = '\0';

   return;
}


/*++++++++++++++++++++++++ get_alternate_number() +++++++++++++++++++++++*/
static int
get_alternate_number(unsigned int job_id)
{
   int  fd,
        ret;
   char alternate_file[MAX_PATH_LENGTH];

   (void)snprintf(alternate_file, MAX_PATH_LENGTH, "%s%s%s%x",
                  p_work_dir, FIFO_DIR, ALTERNATE_FILE, job_id);
   if ((fd = open(alternate_file, O_RDWR | O_CREAT, FILE_MODE)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to open() `%s' : %s #%x"),
                 alternate_file, strerror(errno), job_id);
      ret = INCORRECT;
   }
   else
   {
      struct flock wlock = {F_WRLCK, SEEK_SET, (off_t)0, (off_t)1};

      if (fcntl(fd, F_SETLKW, &wlock) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to lock `%s' : %s #%x"),
                    alternate_file, strerror(errno), job_id);
         ret = INCORRECT;
      }
      else
      {
#ifdef HAVE_STATX
         struct statx stat_buf;
#else
         struct stat  stat_buf;
#endif

#ifdef HAVE_STATX
         if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                   STATX_SIZE, &stat_buf) == -1)
#else
         if (fstat(fd, &stat_buf) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                       _("Failed to statx() `%s' : %s #%x"),
#else
                       _("Failed to fstat() `%s' : %s #%x"),
#endif
                       alternate_file, strerror(errno), job_id);
            ret = INCORRECT;
         }
         else
         {
#ifdef HAVE_STATX
            if (stat_buf.stx_size == 0)
#else
            if (stat_buf.st_size == 0)
#endif
            {
               ret = 0;
            }
            else
            {
               if (read(fd, &ret, sizeof(int)) != sizeof(int))
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Failed to read() from `%s' : %s #%x"),
                             alternate_file, strerror(errno), job_id);
                  ret = INCORRECT;
               }
               else
               {
                  if (lseek(fd, 0, SEEK_SET) == -1)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                _("Failed to lseek() in `%s' : %s #%x"),
                                alternate_file, strerror(errno), job_id);
                     ret = INCORRECT;
                  }
                  else
                  {
                     ret++;
                     if (ret < 0)
                     {
                        ret = 0;
                     }
                  }
               }
            }
            if (ret != INCORRECT)
            {
               if (write(fd, &ret, sizeof(int)) != sizeof(int))
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Failed to write() to `%s' : %s #%x"),
                             alternate_file, strerror(errno), job_id);
               }
            }
         }
      }
      if (close(fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("Failed to close() `%s' : %s #%x"),
                    alternate_file, strerror(errno), job_id);
      }
   }
   return(ret);
}
