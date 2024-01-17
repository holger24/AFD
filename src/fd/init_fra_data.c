/*
 *  init_fra_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2024 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   init_fra_data - initializes all data from the FRA needed by FD
 **
 ** SYNOPSIS
 **   void init_fra_data(void)
 **
 ** DESCRIPTION
 **   Counts the number of retrieve jobs in the FRA and prepares
 **   an array so that these jobs can be accessed faster.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.08.2000 H.Kiehl Created
 **   17.01.2024 H.Kiehl Print out the number of remote directories
 **                      here, so we always can see how this value
 **                      changes, without restarting FD.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>           /* strerror()                              */
#include <stdlib.h>           /* realloc(), free()                       */
#include <errno.h>
#include "fddefs.h"

/* External global variables. */
extern int                        no_of_dirs,
                                  no_of_retrieves,
                                  *retrieve_list;
extern struct fileretrieve_status *fra;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$ init_fra_data() $$$$$$$$$$$$$$$$$$$$$$$$$$$*/
void
init_fra_data(void)
{
   register int i;

   if (retrieve_list != NULL)
   {
      free(retrieve_list);
      retrieve_list = NULL;
      no_of_retrieves = 0;
   }
   for (i = 0; i < no_of_dirs; i++)
   {
      if ((fra[i].protocol == FTP) || (fra[i].protocol == HTTP) ||
          (fra[i].protocol == SFTP) || (fra[i].protocol == EXEC))
      {
         if ((no_of_retrieves % 10) == 0)
         {
            size_t new_size = ((no_of_retrieves / 10) + 1) * 10 * sizeof(int);
            int    *tmp_retrieve_list;

            if ((tmp_retrieve_list = realloc(retrieve_list, new_size)) == (int *)NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_SIZE_T == 4
                          "realloc() error [%d bytes] : %s",
#else
                          "realloc() error [%lld bytes] : %s",
#endif
                          (pri_size_t)new_size, strerror(errno));

               free(retrieve_list);
               retrieve_list = NULL;
               no_of_retrieves = 0;
               return;
            }
            retrieve_list = tmp_retrieve_list;
         }
         retrieve_list[no_of_retrieves] = i;
         no_of_retrieves++;
      }
   }
   system_log(DEBUG_SIGN, NULL, 0,
              "FD configuration: Number of remote directories  %d",
              no_of_retrieves);

   return;
}
