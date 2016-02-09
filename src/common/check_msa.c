/*
 *  check_msa.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_msa - checks if MSA has been updated
 **
 ** SYNOPSIS
 **   int check_msa(void)
 **
 ** DESCRIPTION
 **   This function checks if the MSA (Monitor Status Area) with the
 **   ID 'msa_id' is still in use. If not it will detach from the old
 **   memory mapped area and attach to the new one with the function
 **   msa_attach().
 **
 ** RETURN VALUES
 **   Returns NO if the MSA is still in use. Returns YES if a
 **   new MSA has been created. It will then also return new
 **   values for 'msa_id' and 'no_of_afds'.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.10.1998 H.Kiehl Created
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
#include "mondefs.h"

/* Global variables. */
extern int                    msa_id;
#ifdef HAVE_MMAP
extern off_t                  msa_size;
#endif
extern char                   *p_work_dir;
extern struct mon_status_area *msa;


/*############################ check_msa() ##############################*/
int
check_msa(void)
{
   if (msa != NULL)
   {
      char *ptr;

      ptr = (char *)msa;
      ptr -= AFD_WORD_OFFSET;

      if (*(int *)ptr == STALE)
      {
#ifdef HAVE_MMAP
         if (munmap(ptr, msa_size) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to munmap() from MSA [msa_id = %d msa_size = %d] : %s"),
                       msa_id, msa_size, strerror(errno));
         }
#else
         if (munmap_emu(ptr) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to munmap_emu() from MSA (%d) : %s"),
                       msa_id, strerror(errno));
         }
#endif

         if (msa_attach() < 0)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to attach to MSA."));
            exit(INCORRECT);
         }

         return(YES);
      }
   }

   return(NO);
}
