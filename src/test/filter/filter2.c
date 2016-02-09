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


/*################################ filter() #############################*/
int
filter(char *p_filter, char *p_file)
{
   register int inverse = 0;

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
            if ((filter(p_filter + 1, p_file) == 0) ||
                ((*p_file != '\0') && (filter(p_filter, p_file + 1) == 0)))
            {
               return((inverse == 0) ? 0 : 1);
            }
            else
            {
               return((inverse == 0) ? 1 : 0);
            }

         case '?' :
            if (*p_file == '\0')
            {
               return((inverse == 0) ? 1 : 0);
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
               return((inverse == 0) ? 1 : 0);
            }

         default :
            if (*p_filter != *p_file)
            {
               return((inverse == 0) ? 1 : 0);
            }
            p_filter++;
            p_file++;
            break;
      }
   }
}
