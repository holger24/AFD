/*
 *  lookup_file_mask_id.c - Part of AFD, an automatic file distribution
 *                          program.
 *  Copyright (c) 2003 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   lookup_file_mask_id - searches for a file mask ID
 **
 ** SYNOPSIS
 **   void lookup_file_mask_id(struct instant_db *p_db, int fbl)
 **
 ** DESCRIPTION
 **   The function lookup_file_mask_id() searches for the file mask
 **   list described in 'p_db' in the file mask database (fmd).
 **   If it is found, the file_mask_id is returned in 'p_db'.
 **   If it is not found it the file mask is appended to the
 **   file mask database. The file mask database has the following
 **   structure:
 **
 **    It starts with a 16 byte
 **      AFD_WORD_OFFSET
 **
 **    and one or file mask lists
 **      int            number of file mask
 **      int            max single file mask length
 **      int            total file mask length
 **      unsigned int   file mask ID
 **      unsigned char  number of fill bytes
 **      char[]         file mask list where each file mask is terminate
 **                     by a '\0'
 **      char           redundant CRC char
 **      char[]         fill bytes if necsessary
 **
 **   Functions/programs depending on this structure are:
 **
 **      amg/init_job_data.c
 **      fd/init_msg_buffer.c
 **      tools/file_mask_list_spy.c
 **      tools/get_dc_data.c
 **      tools/job_list_spy.c
 **      UI/Motif/common/get_file_mask_list.c
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   28.12.2003 H.Kiehl Created
 **   22.06.2006 H.Kiehl Catch the case when *no_of_file_masks is larger
 **                      then the actual number stored.
 **
 */
DESCR__E_M3

#include <string.h>          /* strcmp(), strcpy(), strlen(), strerror() */
#include <stdlib.h>          /* free()                                   */
#include <sys/types.h>
#include <utime.h>           /* utime()                                  */
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int   fmd_fd,
             *no_of_file_masks;
extern off_t fmd_size;
extern char  *fmd,
             *fmd_end;


/*######################## lookup_file_mask_id() ########################*/
void
lookup_file_mask_id(struct instant_db *p_db, int fbl)
{
   int    buf_size,
          count,
          fml_offset,
          i,
          mask_offset,
          offset;
   size_t new_size;
   char   *buffer,
          *p_buf_crc,
          *ptr,
          *tmp_ptr;

   ptr = fmd;
   fml_offset = sizeof(int) + sizeof(int);
   mask_offset = fml_offset + sizeof(int) + sizeof(unsigned int) +
                 sizeof(unsigned char);
   for (i = 0; i < *no_of_file_masks; i++)
   {
      if ((*(int *)ptr == p_db->no_of_files) &&
          (*(int *)(ptr + fml_offset) == fbl) &&
          (memcmp((ptr + mask_offset), p_db->files, (size_t)fbl) == 0))
      {
         p_db->file_mask_id = *(unsigned int *)(ptr + fml_offset + sizeof(int));
         return;
      }
      tmp_ptr = ptr;
      ptr += (mask_offset + *(int *)(ptr + fml_offset) + sizeof(char) +
              *(ptr + mask_offset - 1));

      /*
       * The following check is necessary on Linux x86_64. I don't
       * know why this is happening that *no_of_file_masks is to
       * large. So far it only happens on a 4 CPU System with a large
       * DIR_CONFIG. This needs to be fixed properly.
       */
      if ((ptr > fmd_end) || (ptr < tmp_ptr))
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                    "File name database file is corrupted (i=%d *no_of_file_masks=%d fmd_size=%ld ptr=%lx fmd_end=%lx tmp_ptr=%lx). Trying to correct this.",
#else
                    "File name database file is corrupted (i=%d *no_of_file_masks=%d fmd_size=%lld ptr=%llx fmd_end=%llx tmp_ptr=%llx). Trying to correct this.",
#endif
                    i, *no_of_file_masks, (pri_off_t)fmd_size, ptr, fmd_end, tmp_ptr);
         *no_of_file_masks = i;
         fmd_size -= (fmd_end - tmp_ptr);
         break;
      }
   }

   /*
    * This is a brand new job! Append this to the file_mask structure.
    * Resize the file mask database and reset pointers and values.
    */
   new_size = mask_offset + fbl + 1 + fmd_size;
   if ((new_size % sizeof(int)) != 0) /* Determine fill bytes. */
   {
      i = (((new_size / sizeof(int)) + 1) * sizeof(int)) - new_size;
      new_size += i;
   }
   else
   {
      i = 0;
   }
   if ((ptr = mmap_resize(fmd_fd, fmd - AFD_WORD_OFFSET,
                          new_size)) == (caddr_t) -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to mmap_resize() %d bytes : %s",
                 new_size, strerror(errno));
      exit(INCORRECT);
   }
   no_of_file_masks = (int *)ptr;
   fmd_end = ptr + new_size;
   fmd = ptr + AFD_WORD_OFFSET;
   ptr += fmd_size;
   fmd_size = new_size;

   /* Fill the new area with data. */
   buf_size = sizeof(int) + sizeof(int) + fbl + 1;
   if ((buffer = malloc(buf_size)) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() %d bytes : %s", buf_size, strerror(errno));
      exit(INCORRECT);
   }
   p_buf_crc = &buffer[buf_size - 1];
   *p_buf_crc = 0;
   *(int *)buffer = p_db->no_of_files;
   *(int *)ptr = p_db->no_of_files;
   *(int *)(ptr + sizeof(int)) = 0;
   *(int *)(buffer + sizeof(int)) = fbl;
   *(int *)(ptr + fml_offset) = fbl;
   *(ptr + mask_offset - 1) = i;
   tmp_ptr = p_db->files;
   offset = 0;
   for (i = 0; i < p_db->no_of_files; i++)
   {
      count = 0;
      while (*tmp_ptr != '\0')
      {
         *(ptr + mask_offset + offset) = *tmp_ptr;
         *(buffer + sizeof(int) + sizeof(int) + offset) = *tmp_ptr;
         count++; offset++; tmp_ptr++;
      }
      if (*(int *)(ptr + sizeof(int)) < (count + 1))
      {
         *(int *)(ptr + sizeof(int)) = count + 1;
      }
      *(ptr + mask_offset + offset) = *tmp_ptr;
      *(buffer + sizeof(int) + sizeof(int) + offset) = *tmp_ptr;
      offset++; tmp_ptr++;
   }

   /*
    * Lets generate a new checksum for this job. To ensure that the
    * checksum is unique check that it does not appear anywhere in
    * struct job_id_data.
    */
   p_db->file_mask_id = get_checksum(INITIAL_CRC, buffer, buf_size);
   tmp_ptr = fmd;
   for (i = 0; i < *no_of_file_masks; i++)
   {
      if (*(unsigned int *)(tmp_ptr + fml_offset + sizeof(int)) == p_db->file_mask_id)
      {
         unsigned int new_file_mask_id;

         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Hmmm, same checksum (%x) for two different file mask entries!",
                    p_db->file_mask_id);
         do
         {
            (*p_buf_crc)++;
            if ((unsigned char)(*p_buf_crc) > 254)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Unable to produce a different checksum for `%x'. There are two different file mask with the same checksum!",
                          p_db->file_mask_id);
               break;
            }
         } while ((new_file_mask_id = get_checksum(INITIAL_CRC, buffer,
                                                   buf_size)) == p_db->file_mask_id);

         if (new_file_mask_id != p_db->file_mask_id)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Was able to get a file mask ID `%x' instead of `%x' after %d tries.",
                       new_file_mask_id, p_db->file_mask_id, (int)(*p_buf_crc));
            *(ptr + mask_offset + fbl) = *p_buf_crc;
            p_db->file_mask_id = new_file_mask_id;
         }
         break;
      }
      tmp_ptr += (mask_offset + *(int *)(tmp_ptr + fml_offset) + sizeof(char) +
                  *(tmp_ptr + mask_offset - 1));
   }
   free(buffer);
   *(unsigned int *)(ptr + fml_offset + sizeof(int)) = p_db->file_mask_id;
   (*no_of_file_masks)++;

   return;
}
