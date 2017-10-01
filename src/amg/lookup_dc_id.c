/*
 *  lookup_dc_id.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2004 - 2017 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   lookup_dc_id - searches for a DIR_CONFIG ID
 **
 ** SYNOPSIS
 **   void lookup_dc_id(struct dir_config_buf **dcl, int dcl_counter)
 **
 ** DESCRIPTION
 **   This function searches in the DIR_CONFIG database for the
 **   DIR_CONFIG's given by 'dcl'. New DIR_CONFIG's are added to the
 **   the database.
 **
 ** RETURN VALUES
 **   Returns the DIR_CONFIG ID for each DIR_CONFIG in 'dcl'.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   05.01.2004 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>           /* strcmp(), strcpy(), strerror()          */
#include <stdlib.h>           /* exit()                                  */
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern char *p_work_dir;


/*############################ lookup_dc_id() ###########################*/
void
lookup_dc_id(struct dir_config_buf **dcl, int dcl_counter)
{
   register int           i, j;
   int                    dcl_fd,
                          found,
                          *no_of_dir_configs;
   size_t                 length,
                          new_size;
   char                   file[MAX_PATH_LENGTH],
                          *ptr;
   struct dir_config_list *mdcl;

   (void)strcpy(file, p_work_dir);
   (void)strcat(file, FIFO_DIR);
   (void)strcat(file, DC_LIST_FILE);
   new_size = dcl_counter * sizeof(struct dir_config_list);
   if ((ptr = attach_buf(file, &dcl_fd, &new_size,
                         "AMG", FILE_MODE, NO)) == (caddr_t) -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to mmap() `%s' : %s", file, strerror(errno));
      exit(INCORRECT);
   }
   no_of_dir_configs = (int *)ptr;
   mdcl = (struct dir_config_list *)(ptr + AFD_WORD_OFFSET);
   if (*no_of_dir_configs == 0)
   {
      *(ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
      *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
      *(ptr + SIZEOF_INT + 1 + 1 + 1) = CURRENT_DCID_VERSION;
      (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
   }
   for (j = 0; j < dcl_counter; j++)
   {
      (*dcl)[j].dc_id = 0;
   }
   found = 0;
   for (i = 0; ((i < *no_of_dir_configs) && (found < dcl_counter)); i++)
   {
      for (j = 0; j < dcl_counter; j++)
      {
         if (((*dcl)[j].dc_id == 0) &&
             (CHECK_STRCMP(mdcl[i].dir_config_file, (*dcl)[j].dir_config_file) == 0))
         {
            (*dcl)[j].dc_id = mdcl[i].dc_id;
            found++;
            break;
         }
      }
   }

   if (found < dcl_counter)
   {
      int k,
          left;

      left = dcl_counter - found;
      ptr = (char *)mdcl - AFD_WORD_OFFSET;
      new_size = (*no_of_dir_configs + left) * sizeof(struct dir_config_list);
      if ((ptr = mmap_resize(dcl_fd, ptr, new_size)) == (caddr_t) -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to mmap_resize() to %d bytes : %s",
                    new_size, strerror(errno));
         exit(INCORRECT);
      }
      no_of_dir_configs = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      mdcl = (struct dir_config_list *)ptr;

      j = 0;
      for (i = 0; i < left; i++)
      {
         while ((*dcl)[j].dc_id != 0)
         {
            j++;
         }
         length = strlen((*dcl)[j].dir_config_file);
         (void)memcpy(mdcl[*no_of_dir_configs].dir_config_file,
                      (*dcl)[j].dir_config_file, length);
         mdcl[*no_of_dir_configs].dir_config_file[length] = '\0';
         mdcl[*no_of_dir_configs].dir_config_file[length + 1] = 0;
         (*dcl)[j].dc_id = get_checksum(INITIAL_CRC,
                                        mdcl[*no_of_dir_configs].dir_config_file,
                                        length + 2);
         for (k = 0; k < *no_of_dir_configs; k++)
         {
            if (mdcl[k].dc_id == (*dcl)[j].dc_id)
            {
               unsigned int new_dc_id = 0;

               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Hmmm, same checksum (%x) for two different DIR_CONFIG's!",
                          (*dcl)[j].dc_id);
               do
               {
                  mdcl[*no_of_dir_configs].dir_config_file[length + 1]++;
                  if ((unsigned char)(mdcl[*no_of_dir_configs].dir_config_file[length + 1]) > 254)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Unable to produce a different checksum for `%x'. There are two different DIR_CONFIG's with the same checksum!",
                                (*dcl)[j].dc_id);
                     break;
                  }
               } while ((new_dc_id = get_checksum(INITIAL_CRC,
                                                  mdcl[*no_of_dir_configs].dir_config_file,
                                                  length + 2)) == (*dcl)[j].dc_id);

               if (new_dc_id != (*dcl)[j].dc_id)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Was able to get a new DIR_CONFIG ID `%x' instead of `%x' after %d tries.",
                             new_dc_id, (*dcl)[j].dc_id,
                             (int)(mdcl[*no_of_dir_configs].dir_config_file[length + 1]));
                  (*dcl)[j].dc_id = new_dc_id;
               }
            }
         }
         mdcl[*no_of_dir_configs].dc_id = (*dcl)[j].dc_id;
         j++;
         (*no_of_dir_configs)++;
      }
   }
   unmap_data(dcl_fd, (void *)&mdcl);

   return;
}
