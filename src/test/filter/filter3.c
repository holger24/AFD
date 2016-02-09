/*
 *  filter.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#include <stdio.h>


/*################################ filter() #############################*/
int
filter(char *p_filter, char *p_file)
{
   register int inverse = 0,
                sc = 0;
   char         *tmp_p_filter[10] = { NULL, NULL, NULL, NULL, NULL,
                                      NULL, NULL, NULL, NULL, NULL },
                *tmp_p_file[10];

   if (*p_filter == '!')
   {
      p_filter++;
      inverse = 1;
   }

   for (;;)
   {
      switch (*p_filter)
      {
         case '*' :
            if (*(p_filter + 1) == '\0')
            {
               return((inverse == 0) ? 0 : 1);
            }
            if (tmp_p_filter[sc] == NULL)
            {
               tmp_p_filter[sc] = p_filter;
               tmp_p_file[sc] = p_file;
               do
               {
                  p_filter++;
               } while (*p_filter == '*');
               sc++;
            }
            else
            {
               if (*p_file != '\0')
               {
                  p_filter = tmp_p_filter[sc];
                  p_file = tmp_p_file[sc];
                  p_file++;
                  tmp_p_filter[sc] = NULL;
               }
               else
               {
                  return((inverse == 0) ? 1 : 0);
               }
            }
            break;

         case '?' :
            if (*p_file == '\0')
            {
               if ((sc == 0) || (tmp_p_filter[sc - 1] == NULL))
               {
                  return((inverse == 0) ? 1 : 0);
               }
               else
               {
                  sc--;
                  p_filter = tmp_p_filter[sc];
                  p_file = tmp_p_file[sc];
                  break;
               }
            }
            p_filter++;
            p_file++;
            break;

         case '\0' :
            if (*p_file == *p_filter)
            {
               return((inverse == 0) ? 0 : 1);
            }
            else
            {
               if ((sc == 0) || (tmp_p_filter[sc - 1] == NULL))
               {
                  return((inverse == 0) ? 1 : 0);
               }
               else
               {
                  sc--;
                  p_filter = tmp_p_filter[sc];
                  p_file = tmp_p_file[sc];
                  break;
               }
            }

         default :
            if (*p_filter != *p_file)
            {
               if ((sc == 0) || (tmp_p_filter[sc - 1] == NULL))
               {
                  return((inverse == 0) ? 1 : 0);
               }
               else
               {
                  sc--;
                  p_filter = tmp_p_filter[sc];
                  p_file = tmp_p_file[sc];
                  break;
               }
            }
            p_filter++;
            p_file++;
            break;
      }
   }
}
