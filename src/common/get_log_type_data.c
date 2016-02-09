/*
 *  get_log_type_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_log_type_data - reads the log data types from given pointer
 **
 ** SYNOPSIS
 **   void get_log_type_data(char *ptr)
 **
 ** DESCRIPTION
 **   Reads the log data types fron the given pointer ptr. Currently
 **   it reads the following values:
 **         LOG_DATE_LENGTH, MAX_HOSTNAME_LENGTH
 **
 ** RETURN VALUES
 **   If it finds any good values it returns them in the global
 **   variables.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   23.03.2014 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdlib.h>          /* atoi()                                   */
#include <ctype.h>           /* isdigit()                                */

/* Global variables. */
extern int log_date_length,
           max_hostname_length;


/*######################### get_log_type_data() #########################*/
void
get_log_type_data(char *ptr)
{
   int  i = 0;
   char value[MAX_INT_LENGTH + 1];

   while (*ptr == ' ')
   {
      ptr++;
   }
   while ((isdigit((int)(*ptr))) && (i < MAX_INT_LENGTH))
   {
      value[i] = *ptr;
      ptr++; i++;
   }
   if ((i > 0) && (i < MAX_INT_LENGTH))
   {
      value[i] = '\0';
      log_date_length = atoi(value);
      if (log_date_length > (MAX_LINE_LENGTH / 4))
      {
         log_date_length = LOG_DATE_LENGTH;
      }
   }
   while (*ptr != ' ')
   {
      ptr++;
   }
   while (*ptr == ' ')
   {
      ptr++;
   }
   i = 0;
   while ((isdigit((int)(*ptr))) && (i < MAX_INT_LENGTH))
   {
      value[i] = *ptr;
      ptr++; i++;
   }
   if ((i > 0) && (i < MAX_INT_LENGTH))
   {
      value[i] = '\0';
      max_hostname_length = atoi(value);
      if (max_hostname_length > (MAX_LINE_LENGTH / 4))
      {
         max_hostname_length = MAX_HOSTNAME_LENGTH;
      }
   }

   return;
}
