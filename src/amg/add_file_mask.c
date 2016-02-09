/*
 *  add_file_mask.c - Part of AFD, an automatic file distribution
 *                    program.
 *  Copyright (c) 2015 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   add_file_mask - stores all file mask to a file
 **
 ** SYNOPSIS
 **   void add_file_mask(char *dir_alias, struct dir_group *dir)
 **
 ** DESCRIPTION
 **   The function add_file_mask adds all file mask for the
 **   directory 'dir_alias' into the file:
 **       $AFD_WORK_DIR/files/incoming/filters/<dir_alias>
 **   For each file group it stores all file mask in a block
 **   where each file mask is separated by a binary zero.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.11.2015 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>            /* sprintf()                               */
#include <string.h>           /* strerror(), memcpy()                    */
#include <stdlib.h>           /* malloc(), free()                        */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int  alfbl,               /* Additional locked file buffer length */
            alfc;                /* Additional locked file counter       */
extern char *alfiles,            /* Additional locked files              */
            *p_work_dir;


/*########################### add_file_mask() ###########################*/
void
add_file_mask(char *dir_alias, struct dir_group *dir)
{
   int         align_offset,
               fbl,
               fc,
               fd,
               i,
               mod;
   size_t      length;
   char        *buffer,
               file_mask_file[MAX_PATH_LENGTH];
   struct stat stat_buf;

   (void)snprintf(file_mask_file, MAX_PATH_LENGTH, "%s%s%s%s/%s",
                  p_work_dir, AFD_FILE_DIR,
                  INCOMING_DIR, FILE_MASK_DIR, dir_alias);

   if ((stat(file_mask_file, &stat_buf) == -1) && (errno == ENOENT))
   {
      if ((fd = coe_open(file_mask_file, (O_RDWR | O_CREAT | O_TRUNC),
#ifdef GROUP_CAN_WRITE
                         (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP))) == -1)
#else
                         (S_IRUSR | S_IWUSR))) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to coe_open() `%s' : %s",
                    file_mask_file, strerror(errno));
         return;
      }
#ifdef LOCK_DEBUG
      lock_region_w(fd, (off_t)0, __FILE__, __LINE__);
#else
      lock_region_w(fd, (off_t)0);
#endif
      i = 0;
   }
   else
   {
      if ((fd = lock_file(file_mask_file, ON)) < 0)
      {
         return;
      }
      if (read(fd, &i, sizeof(int)) != sizeof(int))
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to read() %d bytes from `%s' : %s",
                    sizeof(int), file_mask_file, strerror(errno));
         (void)close(fd);
         return;
      }

      /* Go to begining of file. */
      if (lseek(fd, 0, SEEK_SET) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to lseek() to begining of `%s' : %s",
                    file_mask_file, strerror(errno));
         (void)close(fd);
         return;
      }
   }
   i += dir->fgc;

   if (write(fd, &i, sizeof(int)) != sizeof(int))
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to write() %d bytes to `%s' : %s",
                 sizeof(int), file_mask_file, strerror(errno));
      (void)close(fd);
      return;
   }
   if (lseek(fd, 0, SEEK_END) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to lseek() to begining of `%s' : %s",
                 file_mask_file, strerror(errno));
      (void)close(fd);
      return;
   }
   for (i = 0; i < dir->fgc; i++)
   {
      length = sizeof(int) + sizeof(int) + alfbl + dir->file[i].fbl;
      mod = length % 4;
      if (mod != 0)
      {
         align_offset = sizeof(int) - mod;
         length += align_offset;
      }
      else
      {
         align_offset = 0;
      }
      fbl = alfbl + dir->file[i].fbl + align_offset;
      if ((buffer = malloc(length)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to malloc() %d bytes : %s",
                    length, strerror(errno));
         break;
      }
      fc = alfc + dir->file[i].fc;
      (void)memcpy(buffer, &fc, sizeof(int));
      (void)memcpy(buffer + sizeof(int), &fbl, sizeof(int));
      (void)memcpy(buffer + sizeof(int) + sizeof(int), alfiles, alfbl);
      (void)memcpy(buffer + sizeof(int) + sizeof(int) + alfbl,
                   dir->file[i].files, dir->file[i].fbl);
      if (write(fd, buffer, length) != length)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to write() %d bytes to `%s' : %s",
                    length, file_mask_file, strerror(errno));
         free(buffer);
         break;
      }
      free(buffer);
   }
   if (close(fd) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__, "Failed to close() `%s' : %s",
                file_mask_file, strerror(errno));
   }

   return;
}
