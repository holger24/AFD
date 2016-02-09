/*
 *  check_host_permissions.c - Part of AFD, an automatic file distribution
 *                             program.
 *  Copyright (c) 2005 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_host_permissions - checks if we have permissions for given
 **                            host
 **
 ** SYNOPSIS
 **   int check_host_permissions(char *host, char **list, int no_in_list)
 **
 ** DESCRIPTION
 **   This function checks if the host is in the given list.
 **
 ** RETURN VALUES
 **   Returns SUCCESS if host is in the list or list is empty (NULL).
 **   Otherwise INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.04.2005 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>
#include <ctype.h>
#include "ui_common_defs.h"


/*####################### check_host_permissions() ######################*/
int
check_host_permissions(char *host, char **list, int no_in_list)
{
   int ret;

   if ((list == NULL) || (no_in_list < 1))
   {
      ret = SUCCESS;
   }
   else
   {
      int i;

      for (i = 0; i < no_in_list; i++)
      {
         if (strcmp(host, list[i]) == 0)
         {
            ret = SUCCESS;
            break;
         }
      }
      if (i == no_in_list)
      {
         ret = INCORRECT;
      }
   }
   return(ret);
}
