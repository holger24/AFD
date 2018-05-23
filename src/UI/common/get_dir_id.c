/*
 *  get_dir_id.c - Part of AFD, an automatic file distribution
 *                 program.
 *  Copyright (c) 2018 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_dir_id - gets the dir ID from the die alias
 **
 ** SYNOPSIS
 **   int get_dir_id(char *alias, unsigned int *dir_id)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS when it managed to find the directory ID and returns
 **   it in dir_id. Otherwise INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.05.2018 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>      /* NULL                                          */
#include <string.h>     /* strcmp()                                      */
#include "ui_common_defs.h"

/* External global variables. */
extern int                        no_of_dirs;
extern struct fileretrieve_status *fra;


/*######################## get_current_jid_list() #######################*/
int
get_dir_id(char *alias, unsigned int *dir_id)
{
   int i;

   if (fra == NULL)
   {
      (void)fra_attach_passive();
   }
   for (i = 0; i < no_of_dirs; i++)
   {
      if (strcmp(alias, fra[i].dir_alias) == 0)
      {
         *dir_id = fra[i].dir_id;

         return(SUCCESS);
      }
   }
   *dir_id = 0;

   return(INCORRECT);
}
