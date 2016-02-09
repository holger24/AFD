/*
 *  iso8859_2ascii.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2010 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   iso8859_2ascii - converts ISO8859_1 to ASCII
 **
 ** SYNOPSIS
 **   off_t iso8859_2ascii(char *src, char *dst, off_t size)
 **
 ** DESCRIPTION
 **   The function iso8859_2ascii() will convert ISO8859_1
 **   (e4, f6, fc and df) to normal ASCII characters. These
 **   characters will be converted as follows:
 **     e4 -> ae       c4 -> Ae | AE | A E
 **     f6 -> oe       d6 -> Oe | OE | O E
 **     fc -> ue       dc -> Ue | UE | U E
 **     df -> ss
 **
 ** RETURN VALUES
 **   Returns the number of bytes written to the destination
 **   buffer dst.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.10.2010 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <ctype.h>            /* isupper()                               */
#include "amgdefs.h"


/*########################### iso8859_2ascii() ##########################*/
off_t
iso8859_2ascii(char *src, char *dst, off_t size)
{
   unsigned char *p_dst,
                 *p_src;
   off_t         i, j;

   p_src = (unsigned char *)src;
   p_dst = (unsigned char *)dst;
   for (i = 0; i < size; i++)
   {
      switch (*(p_src + i))
      {
         case 228 : /* ae */
            *p_dst = 'a';
            *(p_dst + 1) = 'e';
            p_dst += 2;
            break;
         case 246 : /* oe */
            *p_dst = 'o';
            *(p_dst + 1) = 'e';
            p_dst += 2;
            break;
         case 252 : /* ue */
            *p_dst = 'u';
            *(p_dst + 1) = 'e';
            p_dst += 2;
            break;
         case 223 : /* ss */
            *p_dst = 's';
            *(p_dst + 1) = 's';
            p_dst += 2;
            break;
         case 196 : /* Ae */
            j = 0;
            do
            {
               if ((i + j) < (size - 1))
               {
                  if (*(p_src + i + j + 1) > ' ')
                  {
                     if (isupper(*(p_src + i + j + 1)))
                     {
                        if (*(p_src + i + j + 1) == ' ')
                        {
                           *p_dst = 'A';
                           *(p_dst + 1) = ' ';
                           *(p_dst + 2) = 'E';
                           p_dst += 3;
                           j++;
                        }
                        else
                        {
                           *p_dst = 'A';
                           *(p_dst + 1) = 'E';
                           p_dst += 2;
                        }
                     }
                     else
                     {
                        *p_dst = 'A';
                        *(p_dst + 1) = 'e';
                        p_dst += 2;
                     }
                     j++;
                     break;
                  }
                  else
                  {
                     j++;
                     if ((i + j) >= (size - 1))
                     {
                        *p_dst = 'A';
                        *(p_dst + 1) = 'e';
                        p_dst += 2;
                        break;
                     }
                  }
               }
               else
               {
                  *p_dst = 'A';
                  *(p_dst + 1) = 'e';
                  p_dst += 2;
                  j++;
                  break;
               }
            } while (*(p_src + i + j));
            break;
         case 214 : /* Oe */
            j = 0;
            do
            {
               if ((i + j) < (size - 1))
               {
                  if (*(p_src + i + j + 1) > ' ')
                  {
                     if (isupper(*(p_src + i + j + 1)))
                     {
                        if (*(p_src + i + j + 1) == ' ')
                        {
                           *p_dst = 'O';
                           *(p_dst + 1) = ' ';
                           *(p_dst + 2) = 'E';
                           p_dst += 3;
                           j++;
                        }
                        else
                        {
                           *p_dst = 'O';
                           *(p_dst + 1) = 'E';
                           p_dst += 2;
                        }
                     }
                     else
                     {
                        *p_dst = 'O';
                        *(p_dst + 1) = 'e';
                        p_dst += 2;
                     }
                     j++;
                     break;
                  }
                  else
                  {
                     j++;
                     if ((i + j) >= (size - 1))
                     {
                        *p_dst = 'O';
                        *(p_dst + 1) = 'e';
                        p_dst += 2;
                        break;
                     }
                  }
               }
               else
               {
                  *p_dst = 'O';
                  *(p_dst + 1) = 'e';
                  p_dst += 2;
                  j++;
                  break;
               }
            } while (*(p_src + i + j));
            break;
         case 220 : /* Ue */
            j = 0;
            do
            {
               if ((i + j) < (size - 1))
               {
                  if (*(p_src + i + j + 1) > ' ')
                  {
                     if (isupper(*(p_src + i + j + 1)))
                     {
                        if (*(p_src + i + j + 1) == ' ')
                        {
                           *p_dst = 'U';
                           *(p_dst + 1) = ' ';
                           *(p_dst + 2) = 'E';
                           p_dst += 3;
                           j++;
                        }
                        else
                        {
                           *p_dst = 'U';
                           *(p_dst + 1) = 'E';
                           p_dst += 2;
                        }
                     }
                     else
                     {
                        *p_dst = 'U';
                        *(p_dst + 1) = 'e';
                        p_dst += 2;
                     }
                     j++;
                     break;
                  }
                  else
                  {
                     j++;
                     if ((i + j) >= (size - 1))
                     {
                        *p_dst = 'U';
                        *(p_dst + 1) = 'e';
                        p_dst += 2;
                        break;
                     }
                  }
               }
               else
               {
                  *p_dst = 'U';
                  *(p_dst + 1) = 'e';
                  p_dst += 2;
                  j++;
                  break;
               }
            } while (*(p_src + i + j));
            break;
         default  : /* All other characters. */
            *p_dst = *(p_src + i);
            p_dst += 1;
            break;
      }
   }

   return((off_t)(p_dst - (unsigned char *)dst));
}
