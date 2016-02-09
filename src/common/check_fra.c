/*
 *  check_fra.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_fra - checks if FRA has been updated
 **
 ** SYNOPSIS
 **   int check_fra(int passive)
 **
 ** DESCRIPTION
 **   This function checks if the FRA (Fileretrieve Status Area)
 **   which is a memory mapped area, is still in use. If not
 **   it will detach from the old memory area and attach
 **   to the new one with the function fra_attach().
 **
 ** RETURN VALUES
 **   Returns NO if the FRA is still in use. Returns YES if a
 **   new FRA has been created. It will then also return new
 **   values for 'fra_id' and 'no_of_dirs'.
 **
 ** SEE ALSO
 **   fra_attach(), fra_attach_passive()
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.08.2000 H.Kiehl Created
 **   30.07.2006 H.Kiehl Option to only attach in read only mode (passive).
 **
 */
DESCR__E_M3

#include <stdio.h>                       /* stderr, NULL                 */
#include <string.h>                      /* strerror()                   */
#include <stdlib.h>                      /* exit()                       */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>                   /* munmap()                     */
#endif
#include <errno.h>

/* Global variables. */
extern int                        fra_id;
#ifdef HAVE_MMAP
extern off_t                      fra_size;
#endif
extern char                       *p_work_dir;
extern struct fileretrieve_status *fra;


/*############################ check_fra() ##############################*/
int
check_fra(int passive)
{
   if (fra != NULL)
   {
      char *ptr;

      ptr = (char *)fra;
      ptr -= AFD_WORD_OFFSET;

      if (*(int *)ptr == STALE)
      {
#ifdef HAVE_MMAP
         if (munmap(ptr, fra_size) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to munmap() from FRA [fra_id = %d fra_size = %d] : %s"),
                       fra_id, fra_size, strerror(errno));
         }
#else
         if (munmap_emu(ptr) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to munmap_emu() from FRA (%d) : %s"),
                       fra_id, strerror(errno));
         }
#endif

         if (passive == YES)
         {
            if (fra_attach_passive() != SUCCESS)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Passive attach to FRA failed."));
               exit(INCORRECT);
            }
         }
         else
         {
            if (fra_attach() != SUCCESS)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to attach to FRA."));
               exit(INCORRECT);
            }
         }

         return(YES);
      }
   }

   return(NO);
}
