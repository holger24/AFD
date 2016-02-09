/*
 *  locate_host.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2007 Deutscher Wetterdienst (DWD),
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
 **   locate_host - gets the position in the array which holds the host
 **
 ** SYNOPSIS
 **   int locate_host(struct afdstat *ptr, char *host, int no_of_hosts)
 **   int locate_host_year(struct afd_year_stat *ptr, char *host, int no_of_hosts)
 **
 ** DESCRIPTION
 **   This function gets the position of host 'host' in the
 **   output statistic structure. This area consists of 'no_of_hosts'
 **   structures, i.e. for each host a structure of statistics
 **   exists.
 **
 ** RETURN VALUES
 **   Returns the position of the host 'host' in the structure pointed
 **   to by 'ptr'. If the host is not found, INCORRECT is returned
 **   instead.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   18.09.1996 H.Kiehl Created
 **   08.12.1999 H.Kiehl Added function locate_host_year().
 **
 */
DESCR__E_M3

#include <string.h>                 /* strcmp()                          */
#include "statdefs.h"


/*########################### locate_host() #############################*/
int
locate_host(struct afdstat *ptr,
            char           *host,
            int            no_of_hosts)
{
   int position;

   for (position = 0; position < no_of_hosts; position++)
   {
      if (strcmp(ptr[position].hostname, host) == 0)
      {
         return(position);
      }
   }

   /* Host not found in struct */
   return(INCORRECT);
}


/*######################### locate_host_year() ##########################*/
int
locate_host_year(struct afd_year_stat *ptr,
                 char                 *host,
                 int                  no_of_hosts)
{
   int position;

   for (position = 0; position < no_of_hosts; position++)
   {
      if (strcmp(ptr[position].hostname, host) == 0)
      {
         return(position);
      }
   }

   /* Host not found in struct */
   return(INCORRECT);
}
