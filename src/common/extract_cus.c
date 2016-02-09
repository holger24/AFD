/*
 *  extract_cus.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 - 2010 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   extract_cus - Extracts creation time, unique_number and
 **                 split_job_counter from the message name
 **
 ** SYNOPSIS
 **   void extract_cus(char         *msg_name,
 **                    time_t       *creation_time,
 **                    unsigned int *split_job_counter,
 **                    unsigned int *unique_number)
 **
 ** DESCRIPTION
 **   This function extracts the creation time, unique_number and
 **   split_job_counter from the message name which has the
 **   following structure:
 **    <job_id>/<counter>/<creation_time>_<unique_no>_<split_job_counter>_
 **
 ** RETURN VALUES
 **   On success it returns the values for creation_time, split_job_counter,
 **   and unique_number. On error these values will be set to zero.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.03.2008 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdlib.h>


/*########################### extract_cus() #############################*/
void
extract_cus(char         *msg_name,
            time_t       *creation_time,
            unsigned int *split_job_counter,
            unsigned int *unique_number)
{
   register int i = 0;

   while ((msg_name[i] != '/') && (msg_name[i] != '\0'))
   {
      i++;
   }
   if (msg_name[i] == '/')
   {
      i++;
      while ((msg_name[i] != '/') && (msg_name[i] != '\0'))
      {
         i++;
      }
      if (msg_name[i] == '/')
      {
         register int j = 0;
         char         str_number[MAX_TIME_T_HEX_LENGTH];

         i++;
         while ((msg_name[i] != '_') && (msg_name[i] != '\0') &&
                (j < MAX_TIME_T_HEX_LENGTH))
         {
            str_number[j] = msg_name[i];
            i++; j++;
         }
         if ((msg_name[i] == '_') && (j > 0) && (j != MAX_TIME_T_HEX_LENGTH))
         {
            i++;
            str_number[j] = '\0';
            *creation_time = (time_t)str2timet(str_number, (char **)NULL, 16);

            j = 0;
            while ((msg_name[i] != '_') && (msg_name[i] != '\0') &&
                   (j < MAX_INT_HEX_LENGTH))
            {
               str_number[j] = msg_name[i];
               i++; j++;
            }
            if ((msg_name[i] == '_') && (j > 0) && (j != MAX_INT_HEX_LENGTH))
            {
               i++;
               str_number[j] = '\0';
               *unique_number = (unsigned int)strtoul(str_number, (char **)NULL,
                                                      16);
               j = 0;
               while ((msg_name[i] != '_') && (msg_name[i] != '\0') &&
                      (j < MAX_INT_HEX_LENGTH))
               {
                  str_number[j] = msg_name[i];
                  i++; j++;
               }
               if (((msg_name[i] == '\0') || (msg_name[i] == '_')) &&
                   (j > 0) && (j != MAX_INT_HEX_LENGTH))
               {
                  str_number[j] = '\0';
                  *split_job_counter = (unsigned int)strtoul(str_number,
                                                             (char **)NULL, 16);
                  return;
               }
            }
         }
      }
   }
   *creation_time = 0L;
   *unique_number = 0;
   *split_job_counter = 0;

   return;
}
