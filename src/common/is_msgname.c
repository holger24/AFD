/*
 *  is_msgname.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   is_msgname - checks if the supplied name is a AFD message name
 **
 ** SYNOPSIS
 **   int is_msgname(const char *name)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   Will return SUCCESS when 'name' is an AFD message name.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   13.01.2003 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <ctype.h>              /* isxdigit()                            */


/*########################### is_msgname() ##############################*/
int
is_msgname(const char *name)
{
   int i = 0;

   while ((isxdigit((int)(name[i]))) && (i < 9))
   {
      i++; /* Away with job ID. */
   }
   if (name[i] == '/')
   {
      int offset;

      i++;
      offset = i;
      while ((isxdigit((int)(name[i]))) && ((i - offset) < 9))
      {
         i++; /* Away with dir number. */
      }
      if (name[i] == '/')
      {
         i++;
         offset = i;
         while ((isxdigit((int)(name[i]))) && ((i - offset) < 9))
         {
            i++; /* Away with date. */
         }
         if (name[i] == '_')
         {
            i++;
            offset = i;
            while ((isxdigit((int)(name[i]))) && ((i - offset) < 9))
            {
               i++; /* Away with unique number. */
            }
            if (name[i] == '_')
            {
               i++;
               offset = i;
               while ((isxdigit((int)(name[i]))) && ((i - offset) < 9))
               {
                  i++; /* Away with split job counter. */
               }
               if (name[i] == '\0')
               {
                  return(SUCCESS);
               }
            }
         }
      }
   }

   return(INCORRECT);
}
