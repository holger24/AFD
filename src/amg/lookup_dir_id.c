/*
 *  lookup_dir_id.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   lookup_dir_id - searches for a directory number
 **
 ** SYNOPSIS
 **   int lookup_dir_id(char *dir_name, char *orig_dir_name)
 **
 ** DESCRIPTION
 **   This function searches in the structure dir_name_buf for the
 **   directory name 'dir_name'. If it does not find this directory
 **   it adds it to the structure.
 **
 ** RETURN VALUES
 **   Returns the directory ID.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   21.01.1998 H.Kiehl Created
 **   27.03.2000 H.Kiehl Return position in struct dir_name_buf
 **                      and not the directory ID.
 **   28.08.2003 H.Kiehl Directory ID is now a CRC-32 checksum.
 **   05.01.2004 H.Kiehl Also store directory name as it is in DIR_CONFIG.
 **
 */
DESCR__E_M3

#include <string.h>           /* strcmp(), strcpy(), strerror()          */
#include <stdlib.h>           /* exit()                                  */
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int                 dnb_fd,
                           *no_of_dir_names;
extern struct dir_name_buf *dnb;


/*########################### lookup_dir_id() ###########################*/
int
lookup_dir_id(char *dir_name, char *orig_dir_name)
{
   register int i;
   size_t       buf_size,
                length,
                orig_length;
   char         *buffer,
                *p_buf_crc;

   for (i = 0; i < *no_of_dir_names; i++)
   {
      if ((CHECK_STRCMP(dnb[i].orig_dir_name, orig_dir_name) == 0) &&
          (CHECK_STRCMP(dnb[i].dir_name, dir_name) == 0))
      {
         return(i);
      }
   }

   /*
    * This is a new directory, append it to the directory list
    * structure.
    */
   if ((*no_of_dir_names != 0) &&
       ((*no_of_dir_names % DIR_NAME_BUF_SIZE) == 0))
   {
      char   *ptr;
      size_t new_size = (((*no_of_dir_names / DIR_NAME_BUF_SIZE) + 1) *
                        DIR_NAME_BUF_SIZE * sizeof(struct dir_name_buf)) +
                        AFD_WORD_OFFSET;

      ptr = (char *)dnb - AFD_WORD_OFFSET;
      if ((ptr = mmap_resize(dnb_fd, ptr, new_size)) == (caddr_t) -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to mmap_resize() to %d bytes : %s",
                    new_size, strerror(errno));
         exit(INCORRECT);
      }
      no_of_dir_names = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      dnb = (struct dir_name_buf *)ptr;
   }

   /* Determine the directory ID. */
   length = strlen(dir_name);
   orig_length = strlen(orig_dir_name);
   buf_size = length + orig_length + 3;
   if ((buffer = malloc(buf_size)) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() %d bytes : %s", buf_size, strerror(errno));
      exit(INCORRECT);
   }
   (void)memcpy(dnb[*no_of_dir_names].dir_name, dir_name, length);
   (void)memcpy(buffer, dir_name, length);
   dnb[*no_of_dir_names].dir_name[length] = '\0';
   dnb[*no_of_dir_names].dir_name[length + 1] = 0;
   buffer[length] = '\0';
   (void)memcpy(dnb[*no_of_dir_names].orig_dir_name, orig_dir_name, orig_length);
   dnb[*no_of_dir_names].orig_dir_name[orig_length] = '\0';
   (void)memcpy(&buffer[length + 1], orig_dir_name, orig_length);
   buffer[length + 1 + orig_length] = '\0';
   p_buf_crc = &buffer[length + 1 + orig_length + 1];
   *p_buf_crc = 0;
   dnb[*no_of_dir_names].dir_id = get_checksum(INITIAL_CRC, buffer, buf_size);
   for (i = 0; i < *no_of_dir_names; i++)
   {
      if (dnb[i].dir_id == dnb[*no_of_dir_names].dir_id)
      {
         unsigned int new_did_number;

         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Hmmm, same checksum (%x) for two different directories!",
                    dnb[*no_of_dir_names].dir_id);
         do
         {
            (*p_buf_crc)++;
            if ((unsigned char)(*p_buf_crc) > 254)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Unable to produce a different checksum for `%x'. There are two different directories with the same checksum!",
                          dnb[*no_of_dir_names].dir_id);
               break;
            }
         } while ((new_did_number = get_checksum(INITIAL_CRC, buffer,
                                                 buf_size)) == dnb[*no_of_dir_names].dir_id);

         if (new_did_number != dnb[*no_of_dir_names].dir_id)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Was able to get a new directory ID `%x' instead of `%x' after %d tries.",
                       new_did_number, dnb[*no_of_dir_names].dir_id,
                       (int)(*p_buf_crc));
            dnb[*no_of_dir_names].dir_name[length + 1] = *p_buf_crc;
            dnb[*no_of_dir_names].dir_id = new_did_number;
         }
         break;
      }
   }
   free(buffer);
   (*no_of_dir_names)++;

   return((*no_of_dir_names - 1));
}
