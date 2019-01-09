/*
 *  init_ls_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2019 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   init_ls_data - initializes ls data structure for retrieve jobs
 **
 ** SYNOPSIS
 **   void init_ls_data(void)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   06.01.2019 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include "fddefs.h"

/* Global variables. */
int                               *no_of_listed_files,
                                  rl_fd = -1;
time_t                            *dir_mtime;
struct retrieve_list              *rl = NULL;

/* External global variables. */
extern int                        no_of_retrieves,
                                  *retrieve_list;
extern struct fileretrieve_status *fra;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$ init_ls_data() $$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
void
init_ls_data(void)
{
   register int i, j;

   for (i = 0; i < no_of_retrieves; i++)
   {
      if (attach_ls_data(retrieve_list[i], fra[retrieve_list[i]].fsa_pos,
                         DISTRIBUTED_HELPER_JOB | OLD_ERROR_JOB, NO) == SUCCESS)
      {
         for (j = 0; j < *no_of_listed_files; j++)
         {
            rl[j].assigned = 0;
         }
         detach_ls_data(NO);
      }
   }
   return;
}
