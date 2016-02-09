/*
 *  gsf_check_fra.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 - 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   gsf_check_fra - checks if FRA has been changed
 **
 ** SYNOPSIS
 **   int gsf_check_fra(void)
 **
 ** DESCRIPTION
 **   Checks if the FRA has changed. If it did change it attaches
 **   the new FRA, search for the directory, detaches and then attaches
 **   again to the new FRA.
 **
 ** RETURN VALUES
 **   Returns NO when FRA did not change. It will return YES when it
 **   has changed and has successfully attached to the new FRA. If it
 **   fails to attach to the new FRA, NEITHER is returned. Also, when
 **   the FRA has changed and it has mapped to the new directory
 **   position db.fra_pos will be set. When it fails to map it
 **   will set db.fra_pos to INCORRECT.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   08.03.2008 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>             /* strerror()                            */
#include <stdlib.h>             /* malloc()                              */
#ifdef HAVE_MMAP
# include <sys/mman.h>          /* munmap()                              */
#endif
#include <errno.h>
#include "fddefs.h"

/* Global variables. */
extern int                        no_of_dirs;
#ifdef HAVE_MMAP
extern off_t                      fra_size;
#endif
extern struct fileretrieve_status *fra;
extern struct job                 db;


/*########################### gsf_check_fra() ###########################*/
int
gsf_check_fra(void)
{
   int ret;

   if (db.fra_pos == INCORRECT)
   {
      ret = NEITHER;
   }
   else
   {
      if (fra != NULL)
      {
         char *ptr;

         ptr = (char *)fra;
         ptr -= AFD_WORD_OFFSET;

         if (*(int *)ptr == STALE)
         {
            unsigned char prev_no_of_time_entries;

            prev_no_of_time_entries = fra[db.fra_pos].no_of_time_entries;

            /* Detach from FRA. */
#ifdef HAVE_MMAP
            if (munmap(((char *)fra - AFD_WORD_OFFSET), fra_size) != -1)
#else
            if (munmap_emu((void *)((char *)fra - AFD_WORD_OFFSET)) != -1)
#endif
            {
               fra = NULL;
               if (fra_attach() == SUCCESS)
               {
                  db.fra_pos = get_dir_id_position(fra, db.id.dir, no_of_dirs);
                  if (db.fra_pos != INCORRECT)
                  {
                     if (fra[db.fra_pos].no_of_time_entries == 0)
                     {
                        if (prev_no_of_time_entries > 0)
                        {
                           if ((db.te = malloc(sizeof(struct bd_time_entry))) == NULL)
                           {
                              system_log(ERROR_SIGN, __FILE__, __LINE__,
                                         "Could not malloc() memory : %s",
                                         strerror(errno));
                              exit(ALLOC_ERROR);
                           }
                           if (eval_time_str("* * * * *", &db.te[0]) != SUCCESS)
                           {
                              system_log(ERROR_SIGN, __FILE__, __LINE__,
                                         "Failed to evaluate time string.");
                              exit(INCORRECT);
                           }
                        }
                     }
                     else
                     {
                        if (prev_no_of_time_entries == 0)
                        {
                           free(db.te);
                        }
                        db.te = &fra[db.fra_pos].te[0];
                     }
                     ret = YES;
                  }
                  else
                  {
                     ret = NEITHER;
                  }
               }
               else
               {
                  db.fra_pos = INCORRECT;
                  ret = NEITHER;
               }
            }
            else
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to unmap from FRA : %s", strerror(errno));
               ret = NEITHER;
            }
         }
         else
         {
            ret = NO;
         }
      }
      else
      {
         ret = NO;
      }
   }
   return(ret);
}
