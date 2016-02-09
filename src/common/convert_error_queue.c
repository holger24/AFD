/*
 *  convert_error_queue.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2009 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   convert_error_queue - converts AFD error queue from an old format to
 **                         a new one
 **
 ** SYNOPSIS
 **   char *convert_error_queue(int           old_eq_fd,
 **                             char          *old_eq_file,
 **                             size_t        *old_eq_size,
 **                             char          *old_eq_ptr,
 **                             unsigned char old_version,
 **                             unsigned char new_version)
 **
 ** DESCRIPTION
 **   When there is a change in AFD error queue structure this function
 **   converts from the old structure to the new one. Currently it
 **   can only convert from version 0 to version 1.
 **
 ** RETURN VALUES
 **   When successful it returns a pointer to the converted structure,
 **   otherwise NULL is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.06.2009 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                /* strcpy(), strerror()               */
#include <stdlib.h>                /* malloc(), free()                   */
#include <sys/types.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>
#endif
#include <errno.h>

/* Version 0 */
#define AFD_WORD_OFFSET_0      (SIZEOF_INT + 4 + SIZEOF_INT + 4)
struct error_queue_0
       {
          unsigned int job_id;
          unsigned int no_to_be_queued;
          unsigned int host_id;
          unsigned int special_flag;
       };

/* Version 1 */
#define AFD_WORD_OFFSET_1      (SIZEOF_INT + 4 + SIZEOF_INT + 4)
struct error_queue_1
       {
          time_t       next_retry_time;  /* New */
          unsigned int job_id;
          unsigned int no_to_be_queued;
          unsigned int host_id;
          unsigned int special_flag;
       };



/*######################## convert_error_queue() ########################*/
char *
convert_error_queue(int           old_eq_fd,
                    char          *old_eq_file,
                    size_t        *old_eq_size,
                    char          *old_eq_ptr,
                    unsigned char old_version,
                    unsigned char new_version)
{
   char *ptr = old_eq_ptr;

   if ((old_version == 0) && (new_version == 1))
   {
      int                  i,
                           no_of_old_error_ids;
      size_t               new_size;
      struct error_queue_0 *old_eq;
      struct error_queue_1 *new_eq;

      no_of_old_error_ids = *(int *)old_eq_ptr;
      new_size = no_of_old_error_ids * sizeof(struct error_queue_1);
      if (no_of_old_error_ids > 0)
      {
         ptr += AFD_WORD_OFFSET_0;
         old_eq = (struct error_queue_0 *)ptr;
         if ((ptr = malloc(new_size)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "malloc() error [%d %d] : %s",
                       no_of_old_error_ids, new_size, strerror(errno));
#ifdef HAVE_MMAP
            if (munmap(old_eq_ptr, *old_eq_size) == -1)
#else
            if (munmap_emu(old_eq_ptr) == -1)
#endif
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to munmap() %s : %s",
                          old_eq_file, strerror(errno));
            }
            *old_eq_size = -1;
            return(NULL);
         }
         (void)memset(ptr, 0, new_size);
         new_eq = (struct error_queue_1 *)ptr;

         /*
          * Copy all the old data into the new region.
          */
         for (i = 0; i < no_of_old_error_ids; i++)
         {
            new_eq[i].next_retry_time = 0;
            new_eq[i].job_id          = old_eq[i].job_id;
            new_eq[i].no_to_be_queued = old_eq[i].no_to_be_queued;
            new_eq[i].host_id         = old_eq[i].host_id;
            new_eq[i].special_flag    = old_eq[i].special_flag;
         }

         ptr = (char *)old_eq;
         ptr -= AFD_WORD_OFFSET_0;

         if (*old_eq_size != (new_size + AFD_WORD_OFFSET_1))
         {
            /*
             * Resize the old retrieve list to the size of new one and then copy
             * the new structure into it. Then update the retrieve list version
             * number.
             */
            if ((ptr = mmap_resize(old_eq_fd, ptr,
                                   new_size + AFD_WORD_OFFSET_1)) == (caddr_t) -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to mmap_resize() %s : %s",
                          old_eq_file, strerror(errno));
               free((void *)new_eq);
               return(NULL);
            }
         }

         ptr += AFD_WORD_OFFSET_1;
         (void)memcpy(ptr, new_eq, new_size);
         free((void *)new_eq);    
         ptr -= AFD_WORD_OFFSET_1; 
      }
      else
      {
         if (*old_eq_size != (new_size + AFD_WORD_OFFSET_1))
         {
            /*
             * Resize the old retrieve list to the size of new one and then copy
             * the new structure into it. Then update the retrieve list version
             * number.
             */
            if ((ptr = mmap_resize(old_eq_fd, ptr,
                                   new_size + AFD_WORD_OFFSET_1)) == (caddr_t) -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to mmap_resize() %s : %s",
                          old_eq_file, strerror(errno));
               return(NULL);
            }
         }
      }

      *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
      *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
      *(int *)(ptr + SIZEOF_INT + 4) = 0;            /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
      *old_eq_size = new_size + AFD_WORD_OFFSET_1;

      system_log(INFO_SIGN, NULL, 0,
                 "Converted error queue from version %d to %d.",
                 (int)old_version, (int)new_version);
   }
   else
   {
      system_log(ERROR_SIGN, NULL, 0,
                 "Don't know how to convert a version %d of error queue to version %d.",
                 old_version, new_version);
      ptr = NULL;
   }

   return(ptr);
}
