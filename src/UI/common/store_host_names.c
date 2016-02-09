/*
 *  store_host_names.c - Part of AFD, an automatic file distribution
 *                       program.
 *  Copyright (c) 1997 - 2007 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   store_host_names - creates a buffer and stores host names from
 **                      the afd.users file
 **
 ** SYNOPSIS
 **   int store_host_names(char ***buffer, char *data)
 **
 ** DESCRIPTION
 **   This function extracts the host names from 'data' and creates
 **   an array 'buffer' to store them. The host names must be separated
 **   by a space or a new line and space, and terminated by a comma.
 **   To save space when creating the array, it first counts the
 **   number of host names and the longest host name, befor it allocates
 **   memory for the buffer.
 **   The caller of this function is responsible in freeing the memory
 **   that is being allocated here.
 **
 ** RETURN VALUES
 **   When it does not find a host name or encounters an error it returns
 **   NO_PERMISSION. Otherwise it returns the number of host names stored
 **   in the array.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.05.1997 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>            /* strerror()                             */
#include <stdlib.h>            /* calloc()                               */
#include <errno.h>
#include "permission.h"
#include "ui_common_defs.h"


/*############################ store_host_names() #######################*/
int
store_host_names(char ***buffer, char *data)
{
   int  count = 0,
        length,
        max_length = 0;
   char *ptr = data;

   while ((*ptr != '\0') && (*ptr != ','))
   {
      length = 0;
      while ((*ptr != ' ') && (*ptr != '\t') &&
             (*ptr != ',') && (*ptr != '\0'))
      {
         ptr++;
         length++;
      }
      if (length > max_length)
      {
         max_length = length;
      }
      if (length > 0)
      {
         count++;
      }
      while ((*ptr == ' ') || (*ptr == '\t') || (*ptr == ','))
      {
         ptr++;
      }
   }

   if (count > 0)
   {
      int i,
          j;

      max_length++; /* For '\0' at end of string. */
      RT_ARRAY(*buffer, count, max_length, char);

      ptr = data;
      for (i = 0; i < count; i++)
      {
         j = 0;
         while ((*ptr != ' ') && (*ptr != '\t') &&
                (*ptr != ',') && (*ptr != '\0'))
         {
            (*buffer)[i][j] = *ptr;
            j++; ptr++;
         }
         if (j > 0)
         {
            (*buffer)[i][j] = '\0';
         }
         while ((*ptr == ' ') || (*ptr == '\t') || (*ptr == ','))
         {
            ptr++;
         }
      }
   }
   else
   {
      count = NO_PERMISSION;
   }

   return(count);
}
