/*
 *  pmatch.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   pmatch - checks string if it matches a certain pattern specified
 **            with wild cards
 **
 ** SYNOPSIS
 **   int pmatch(char const *p_filter, char const *p_file, time_t *pmatch_time)
 **
 ** DESCRIPTION
 **   The function pmatch() checks if 'p_file' matches 'p_filter'.
 **   'p_filter' may have the wild cards '*' and '?' anywhere and
 **   in any order. Where '*' matches any string and '?' matches
 **   any single character.
 **
 ** RETURN VALUES
 **   Returns 0 when pattern matches, if this file is definitly
 **   not wanted 1 and otherwise -1.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   03.10.1995 H.Kiehl  Created
 **   18.10.1997 H.Kiehl  Introduced inverse filtering.
 **   03.02.2000 H.Lepper Fix the case "*xxx*??" sdsfxxxbb
 **   29.03.2006 H.Kiehl  Added support for expanding filters.
 **   01.04.2007 H.Kiehl  Fix the case "r10013*00" r10013301000.
 **   29.05.2007 H.Kiehl  Fix the case "*7????" abcd77abcd
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                    /* strncmp()                      */
#include <stdlib.h>                    /* malloc()                       */
#include <ctype.h>                     /* isdigit()                      */
#include <time.h>                      /* time(), strftime()             */
#include <unistd.h>                    /* gethostname()                  */


/* Local variables. */
static char *tmp_filter = NULL;

/* Local function prototypes. */
static char *find(char *, register char *, register int);
static void expand_filter(char *, time_t);


/*################################ pmatch() #############################*/
int
pmatch(char const *p_filter, char const *p_file, time_t *pmatch_time)
{
   register int  length;
   register char *p_gap_file = NULL,
                 *p_gap_filter,
                 *ptr = p_filter,
                 *p_start,
                 *p_start_file = p_file,
                 *p_tmp = NULL,
                 buffer;

   if (*ptr == '!')
   {
      ptr++;
   }
   p_start = ptr;
   while (*ptr != '\0')
   {
      length = 0;
      p_tmp = ptr;
      switch (*ptr)
      {
         case '*' :
            ptr++;
            while ((*ptr != '*') && (*ptr != '?') && (*ptr != '%') &&
                   (*ptr != '\0'))
            {
               length++;
               ptr++;
            }
            if (length == 0)
            {
               p_gap_filter = ptr;
               while (*ptr == '*')
               {
                  ptr++;
               }
               if (*ptr != '\0')
               {
                  ptr = p_gap_filter;
               }
               if ((*ptr == '*') || (*ptr == '?') || (*ptr == '%'))
               {
                  p_gap_file = p_file + 1;
                  p_gap_filter = p_tmp;
                  break;
               }
               else
               {
                  return((*p_filter != '!') ? 0 : 1);
               }
            }
            buffer = *ptr;
            if ((p_file = find(p_file, p_tmp + 1,
                               (buffer == '\0') ? length + 1 : length)) == NULL)
            {
               return(-1);
            }
            else
            {
               if (*ptr == '?')
               {
                  p_gap_file = p_file - length + 1;
                  p_gap_filter = p_tmp;
               }
               if ((*ptr == '\0') && (*p_file != '\0'))
               {
                  ptr = p_tmp;
               }
            }
            if ((buffer == '\0') && (*p_file == '\0'))
            {
               return((*p_filter != '!') ? 0 : 1);
            }
            break;

         case '?' :
            if (*(p_file++) == '\0')
            {
               return(-1);
            }
            if ((*(++ptr) == '\0') && (*p_file == '\0'))
            {
               return((*p_filter != '!') ? 0 : 1);
            }
            if ((*ptr == '\0') && (p_gap_file != NULL))
            {
               p_file = p_gap_file;
               ptr = p_gap_filter;
            }
            break;

         default  :
            if ((*ptr == '%') && ((*(ptr + 1) == 't') || (*(ptr + 1) == 'T') ||
                (*(ptr + 1) == 'h')) &&
                ((ptr == p_start) || ((*(ptr - 1) != '\\'))))
            {
               time_t check_time;

               if (pmatch_time == NULL)
               {
                  check_time = time(NULL);
               }
               else
               {
                  check_time = *pmatch_time;
               }
               expand_filter(p_start, check_time);
               if (tmp_filter == NULL)
               {
                  /* We failed to allocate memory, lets ignore expanding. */
                  goto error_expand;
               }
               else
               {
                  ptr = tmp_filter;
                  p_start = tmp_filter;
                  p_gap_file = NULL;
                  p_file = p_start_file;
               }
            }
            else
            {
error_expand:
               while ((*ptr != '*') && (*ptr != '?') && (*ptr != '%') &&
                      (*ptr != '\0'))
               {
                  length++;
                  ptr++;
               }
               if (CHECK_STRNCMP(p_file, p_tmp, length) != 0)
               {
                  if (p_gap_file != NULL)
                  {
                     p_file = p_gap_file;
                     ptr = p_gap_filter;
                     break;
                  }
                  return(-1);
               }
               p_file += length;
               if ((*ptr == '\0') && (*p_file == '\0'))
               {
                  return((*p_filter != '!') ? 0 : 1);
               }
            }
            break;
      }
   }

   return(-1);
}


/*++++++++++++++++++++++++++++++++ find() +++++++++++++++++++++++++++++++*/
/*
 * Description  : Searches in "search_text" for "search_string". If
 *                found, it returns the address of the last character
 *                in "search_string".
 * Input values : char *search_text
 *                char *search_string
 *                int  string_length
 * Return value : char *search_text when it finds search_text otherwise
 *                NULL is returned.
 */
static char *
find(char          *search_text,
     register char *search_string,
     register int  string_length)
{
   register int hit = 0;

   while (*search_text != '\0')
   {
      if (*(search_text++) == *(search_string++))
      {
         if (++hit == string_length)
         {
            return(search_text);
         }
      }
      else
      {
         search_string -= (hit + 1);
         if (hit != 0)
         {
            search_text -= hit;
            hit = 0;
         }
      }
   }
   if ((*search_string == '\0') && (*search_text == *search_string))
   {
      if (++hit == string_length)
      {
         return(search_text);
      }
   }

   return(NULL);
}


/*++++++++++++++++++++++++++++ expand_filter() ++++++++++++++++++++++++++*/
static void
expand_filter(char *orig_filter, time_t check_time)
{
   if ((tmp_filter != NULL) ||
       ((tmp_filter = malloc(MAX_FILENAME_LENGTH)) != NULL))
   {
      time_t time_modifier = 0;
      char   *rptr,
             time_mod_sign = '+',
             *wptr;

      rptr = orig_filter;
      wptr = tmp_filter;
      while ((*rptr != '\0') && (wptr < (tmp_filter + MAX_FILENAME_LENGTH - 1)))
      {
         if ((*rptr == '%') && ((*(rptr + 1) == 't') || (*(rptr + 1) == 'T') ||
             (*(rptr + 1) == 'h')) &&
             ((rptr == orig_filter) || ((*(rptr - 1) != '\\'))))
         {
            if (*(rptr + 1) == 't')
            {
               time_t time_buf;

               time_buf = check_time;

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
               switch (*(rptr + 2))
               {
                  case 'a': /* Short day of the week 'Tue'. */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%a", localtime(&time_buf));
                     break;
                  case 'b': /* Short month 'Jan'. */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%b", localtime(&time_buf));
                     break;
                  case 'j': /* Day of year [001,366]. */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%j", localtime(&time_buf));
                     break;
                  case 'i': /* Day of month [1,31]. */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%d", localtime(&time_buf));
                     if (*(wptr - 2) == '\0')
                     {
                        *(wptr - 2) = *(wptr - 1);
                        *(wptr - 1) = '\0';
                        wptr -= 1;
                     }
                     break;
                  case 'd': /* Day of month [01,31]. */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%d", localtime(&time_buf));
                     break;
                  case 'M': /* Minute [00,59]. */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%M", localtime(&time_buf));
                     break;
                  case 'J': /* Month [1,12]. */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%m", localtime(&time_buf));
                     if (*(wptr - 2) == '\0')
                     {
                        *(wptr - 2) = *(wptr - 1);
                        *(wptr - 1) = '\0';
                        wptr -= 1;
                     }
                     break;
                  case 'm': /* Month [01,12]. */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%m", localtime(&time_buf));
                     break;
                  case 'y': /* Year 2 chars [01,99]. */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%y", localtime(&time_buf));
                     break;
                  case 'o': /* Hour [0,23]. */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%H", localtime(&time_buf));
                     if (*(wptr - 2) == '\0')
                     {
                        *(wptr - 2) = *(wptr - 1);
                        *(wptr - 1) = '\0';
                        wptr -= 1;
                     }
                     break;
                  case 'H': /* Hour [00,23]. */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%H", localtime(&time_buf));
                     break;
                  case 'S': /* Second [00,59]. */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%S", localtime(&time_buf));
                     break;
                  case 'Y': /* Year 4 chars 2002. */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%Y", localtime(&time_buf));
                     break;
                  case 'A': /* Long day of the week 'Tuesday'. */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%A", localtime(&time_buf));
                     break;
                  case 'B': /* Month 'January'. */
                     wptr += strftime(wptr,
                                      (tmp_filter + MAX_FILENAME_LENGTH - wptr),
                                      "%B", localtime(&time_buf));
                     break;
                  case 'U': /* Unix time. */
                     wptr += snprintf(wptr,
                                      MAX_FILENAME_LENGTH - (wptr - tmp_filter),
#if SIZEOF_TIME_T == 4
                                      "%ld",
#else
                                      "%lld",
#endif
                                      (pri_time_t)time_buf);
                     break;
                  default :
                     *wptr = '%';
                     *(wptr + 1) = 't';
                     *(wptr + 2) = *(rptr + 2);
                     wptr += 3;
                     break;
               }
               rptr += 3;
            }
            else if (*(rptr + 1) == 'T')
                 {
                    int  m,
                         time_unit;
                    char string[MAX_INT_LENGTH + 1];

                    rptr += 2;
                    switch (*rptr)
                    {
                       case '+' :
                       case '-' :
                       case '*' :
                       case '/' :
                       case '%' :
                          time_mod_sign = *rptr;
                          rptr++;
                          break;
                       default  :
                          time_mod_sign = '+';
                          break;
                    }
                    m = 0;
                    while ((isdigit((int)(*rptr))) && (m < MAX_INT_LENGTH))
                    {
                       string[m++] = *rptr++;
                    }
                    if ((m > 0) && (m < MAX_INT_LENGTH))
                    {
                       string[m] = '\0';
                       time_modifier = atoi(string);
                    }
                    else
                    {
                       if (m == MAX_INT_LENGTH)
                       {
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     _("The time modifier specified in the filter %s is to long."),
                                     orig_filter);
                       }
                       else
                       {
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     _("There is no time modifier specified in filter %s"),
                                     orig_filter);
                       }
                       time_modifier = 0;
                    }
                    switch (*rptr)
                    {
                       case 'S' : /* Second. */
                          time_unit = 1;
                          rptr++;
                          break;
                       case 'M' : /* Minute. */
                          time_unit = 60;
                          rptr++;
                          break;
                       case 'H' : /* Hour. */
                          time_unit = 3600;
                          rptr++;
                          break;
                       case 'd' : /* Day. */
                          time_unit = 86400;
                          rptr++;
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
                 else /* It must be the hostname modifier. */
                 {
#ifdef HAVE_GETHOSTNAME
                    char hostname[40];

                    if (gethostname(hostname, 40) == -1)
                    {
#endif
                       char *p_hostname;

                       if ((p_hostname = getenv("HOSTNAME")) != NULL)
                       {
                          wptr += snprintf(wptr,
                                           MAX_FILENAME_LENGTH - (wptr - tmp_filter),
                                           "%s", p_hostname);
                       }
                       else
                       {
                          *wptr = '%';
                          *(wptr + 1) = 'h';
                          wptr += 2;
                       }
#ifdef HAVE_GETHOSTNAME
                    }
                    else
                    {
                       wptr += snprintf(wptr,
                                        MAX_FILENAME_LENGTH - (wptr - tmp_filter),
                                        "%s", hostname);
                    }
#endif
                    rptr += 2;
                 }
         }
         else
         {
            *wptr = *rptr;
            wptr++; rptr++;
         }
      }
      *wptr = '\0';
   }
   return;
}
