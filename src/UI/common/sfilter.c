/*
 *  sfilter.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   sfilter - checks string if it matches a certain pattern specified
 **             with wild cards
 **
 ** SYNOPSIS
 **   int sfilter(char *p_filter, char *p_file, char separator_char)
 **
 ** DESCRIPTION
 **   The function sfilter() checks if 'p_file' matches 'p_filter'.
 **   'p_filter' may have the wild cards '*' and '?' anywhere and
 **   in any order. Where '*' matches any string and '?' matches
 **   any single character. It also may list characters enclosed in
 **   []. A pair of characters separated by a hyphen denotes a range
 **   expression; any character that sorts between those two characters
 **   is matched. If the first character following the [ is a ! then
 **   any character not enclosed is matched.
 **
 **   The code was taken from wildmatch() by Karl Heuer.
 **
 **   This function only differs from pmatch() in that it expects
 **   p_file to be terminated by a 'separator_char'.
 **
 ** RETURN VALUES
 **   Returns 0 when pattern matches, if this file is definitly
 **   not wanted 1 and otherwise -1.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.04.1997 H.Kiehl Created
 **   01.04.2007 H.Kiehl Fix the case "r10013*00" r10013301000.
 **   30.05.2007 H.Kiehl Replaced code with wildmatch() code from
 **                      Karl Heuer.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include "ui_common_defs.h"

/* Local function prototypes. */
static int sfilter2(char const *, char const *, char);


/*############################## sfilter() ##############################*/
int
sfilter(char const *p_filter, char const *p_file, char separator_char)
{
   int ret;

   ret = sfilter2((*p_filter == '!') ? p_filter + 1 : p_filter, p_file,
                  separator_char);
   if (*p_filter == '!')
   {
      if (ret == 0)
      {
         ret = 1;
      }
      else if (ret == -1)
           {
              ret = 0;
           }
   }

   return(ret);
}


/*++++++++++++++++++++++++++++++ sfilter2() +++++++++++++++++++++++++++++*/
static int
sfilter2(char const *p, char const *s, char separator_char)
{
   register char c;

   while ((c = *p++) != '\0')
   {
      if (c == '*')
      {
         if (*p == '\0')
         {
            return(0); /* Optimize common case. */
         }
         do
         {
            if (sfilter2(p, s, separator_char) == 0)
            {
               return(0);
            }
         } while (*s++ != separator_char);
         return(-1);
      }
      else if (c == '?')
           {
               if (*s++ == separator_char)
               {
                  return(-1);
               }
           }
      else if (c == '[')
           {
              register int wantit;
              register int seenit = NO;

              if (*p == '!')
              {
                 wantit = NO;
                 ++p;
              }
              else
              {
                 wantit = YES;
              }
              c = *p++;
              do
              {
                 if (c == '\0')
                 {
                    return(-1);
                 }
                 if ((*p == '-') && (p[1] != '\0'))
                 {
                    if ((*s >= c) && (*s <= p[1]))
                    {
                       seenit = YES;
                    }
                    p += 2;
                 }
                 else
                 {
                    if (c == *s)
                    {
                       seenit = YES;
                    }
                 }
              } while ((c = *p++) != ']');

              if (wantit != seenit)
              {
                 return(-1);
              }
              ++s;
           }
      else if (c == '\\')
           {
              if ((*p == '\0') || (*p++ != *s++))
              {
                 return(-1);
              }
           }
           else
           {
              if (c != *s++)
              {
                 return(-1);
              }
           }
   }

   return((*s == separator_char) ? 0 : -1);
}
