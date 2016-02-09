/*
 *  locate_dir.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   locate_dir - gets the position in the array which holds the directory
 **
 ** SYNOPSIS
 **   int locate_dir(struct afdistat *ptr, char *dir, int no_of_dirs)
 **   int locate_dir_year(struct afd_year_istat *ptr, char *dir, int no_of_dirs)
 **
 ** DESCRIPTION
 **   This function gets the position of directory 'dir' in the
 **   input statistic structure. This area consists of 'no_of_dirs'
 **   structures, i.e. for each directory a structure of statistics
 **   exists.
 **
 ** RETURN VALUES
 **   Returns the position of the directory 'dir' in the structure pointed
 **   to by 'ptr'. If the directory is not found, INCORRECT is returned
 **   instead.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   23.02.2003 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>                 /* strcmp()                          */
#include "statdefs.h"


/*############################ locate_dir() #############################*/
int
locate_dir(struct afdistat *ptr,
           char            *dir,
           int             no_of_dirs)
{
   int position;

   for (position = 0; position < no_of_dirs; position++)
   {
      if (strcmp(ptr[position].dir_alias, dir) == 0)
      {
         return(position);
      }
   }

   /* Directory not found in struct */
   return(INCORRECT);
}


/*########################## locate_dir_year() ##########################*/
int
locate_dir_year(struct afd_year_istat *ptr,
                char                  *dir,
                int                   no_of_dirs)
{
   int position;

   for (position = 0; position < no_of_dirs; position++)
   {
      if (strcmp(ptr[position].dir_alias, dir) == 0)
      {
         return(position);
      }
   }

   /* Directory not found in struct */
   return(INCORRECT);
}
