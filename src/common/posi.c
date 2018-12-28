/*
 *  posi.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2018 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   posi - searches for a string and positions pointer to end of
 **          this string
 **
 ** SYNOPSIS
 **   char *posi(char *search_text, char *search_string)
 **   char *lposi(char *search_text, char *search_string,
 **               const size_t string_length)
 **   char *llposi(char *search_text, const size_t text_length,
 **                char *search_string, const size_t string_length)
 **
 ** DESCRIPTION
 **   This small function searches in the string 'search_text' for
 **   'search_string'. If it finds this string it positions the pointer
 **   'search_text' just after this string.
 **
 ** RETURN VALUES
 **   NULL when 'search_string' could not be found in 'search_text'.
 **   Else it will return the pointer 'search_text' just after
 **   'search_text'.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   13.07.1996 H.Kiehl Created
 **   06.05.1997 H.Kiehl Found and fixed \n\n bug!!
 **   04.03.2009 H.Kiehl Added function lposi().
 **   28.12.2018 H.Kiehl Addwd function llposi().
 **
 */
DESCR__E_M3

#include <string.h>                           /* strlen()                */
#include <stddef.h>                           /* NULL                    */


/*################################ posi() ###############################*/
char *
posi(char *search_text, char *search_string)
{
   int    hit = 0;
   size_t string_length;

   string_length = strlen(search_string);

   while (*search_text != '\0')
   {
      if (*(search_text++) == *(search_string++))
      {
         if (++hit == string_length)
         {
            return(++search_text);
         }
      }
      else
      {
         if ((hit == 1) &&
             (*(search_string - 2) == *(search_text - 1)))
         {
            search_string--;
         }
         else
         {
            search_string -= hit + 1;
            hit = 0;
         }
      }
   }

   return(NULL); /* Found nothing. */
}


/*############################### lposi() ###############################*/
char *
lposi(char *search_text, char *search_string, const size_t string_length)
{
   int hit = 0;

   while (*search_text != '\0')
   {
      if (*(search_text++) == *(search_string++))
      {
         if (++hit == string_length)
         {
            return(++search_text);
         }
      }
      else
      {
         if ((hit == 1) &&
             (*(search_string - 2) == *(search_text - 1)))
         {
            search_string--;
         }
         else
         {
            search_string -= hit + 1;
            hit = 0;
         }
      }
   }

   return(NULL); /* Found nothing. */
}


/*############################## llposi() ###############################*/
char *
llposi(char         *search_text,
       const size_t text_length,
       char         *search_string,
       const size_t string_length)
{
   int hit = 0;
   char *search_text_end = search_text + text_length;

   while (search_text <= search_text_end)
   {
      if (*(search_text++) == *(search_string++))
      {
         if (++hit == string_length)
         {
            return(++search_text);
         }
      }
      else
      {
         if ((hit == 1) &&
             (*(search_string - 2) == *(search_text - 1)))
         {
            search_string--;
         }
         else
         {
            search_string -= hit + 1;
            hit = 0;
         }
      }
   }

   return(NULL); /* Found nothing. */
}
