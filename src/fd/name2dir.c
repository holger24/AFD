/*
 *  name2dir.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2024 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **    name2dir - replaces given separator characters with directory
 **               separator
 **
 ** SYNOPSIS
 **    void name2dir(char separator_char,
 **                  char *file_name,
 **                  char *dir_name,
 **                  int  max_dir_name_length)
 **
 ** DESCRIPTION
 **   name2dir converts a given filename into directory name by
 **   replacing all 'separator_char' characters it finds in
 **   'file_name' with a / and stores this in 'dir_name' variable.
 **
 ** RETURN VALUES
 **   Returns directory name in 'dir_name'.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   15.12.2024 H.Kiehl Created
 **
 */
DESCR__S_M3

#include "fddefs.h"


/*############################# name2dir() ##############################*/
void
name2dir(char separator_char,
         char *file_name,
         char *dir_name,
         int  max_dir_name_length)
{
   int count = 0;

   while ((count < max_dir_name_length) && (file_name[count] != '\0'))
   {
      if (file_name[count] == separator_char)
      {
         dir_name[count] = '/';
      }
      else
      {
         dir_name[count] = file_name[count];
      }
      count++;
   }
   dir_name[count] = '\0';

   return;
}
