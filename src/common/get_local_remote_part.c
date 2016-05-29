/*
 *  get_local_remote_part.c - Part of AFD, an automatic file distribution
 *                            program.
 *  Copyright (c) 2016 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_local_remote_part - gets the local remote part of a directory
 **
 ** SYNOPSIS
 **   void get_local_remote_part(unsigned int dir_id, char *local_remote_part)
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
 **   29.05.2016 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

/* External global variables. */
extern char *p_work_dir;


/*+++++++++++++++++++++++ get_local_remote_part() +++++++++++++++++++++++*/
void
get_local_remote_part(unsigned int dir_id, char *local_remote_part)
{
   int         fd;
   char        file[MAX_PATH_LENGTH];
   struct stat stat_buf;

   (void)strcpy(file, p_work_dir);
   (void)strcat(file, FIFO_DIR);
   (void)strcat(file, DIR_NAME_FILE);
   if ((fd = open(file, O_RDONLY)) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to open() `%s' : %s", file, strerror(errno));
      exit(INCORRECT);
   }

   if (fstat(fd, &stat_buf) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to fstat() `%s' : %s", file, strerror(errno));
      (void)close(fd);
      exit(INCORRECT);
   }

   if (stat_buf.st_size != 0)
   {
      int                 i,
                          *no_of_dir_names;
      char                *ptr,
                          *rptr,
                          search_str[AFD_FILE_DIR_LENGTH + INCOMING_DIR_LENGTH + 1];
      struct dir_name_buf *dnb;

#ifdef HAVE_MMAP
      if ((ptr = mmap(0, stat_buf.st_size, PROT_READ, MAP_SHARED,
                      fd, 0)) == (caddr_t) -1)
#else
      if ((ptr = mmap_emu(0, stat_buf.st_size, PROT_READ,
                          MAP_SHARED, file, 0)) == (caddr_t) -1)
#endif
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to mmap() `%s' : %s", file, strerror(errno));
         (void)close(fd);
         exit(INCORRECT);
      }
      no_of_dir_names = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      dnb = (struct dir_name_buf *)ptr;
      (void)snprintf(search_str, AFD_FILE_DIR_LENGTH + INCOMING_DIR_LENGTH + 1,
                     "%s%s", AFD_FILE_DIR, INCOMING_DIR);

      local_remote_part[0] = '\0';
      for (i = 0; i < *no_of_dir_names; i++)
      {
         if (dnb[i].dir_id == dir_id)
         {
            if ((rptr = lposi(dnb[i].dir_name, search_str,
                              AFD_FILE_DIR_LENGTH + INCOMING_DIR_LENGTH)) == NULL)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Hmm, unable to locate %s in %s for dir ID %x.",
                          search_str, dnb[i].dir_name, dir_id);
            }
            else
            {
               int length = (rptr - dnb[i].dir_name) - AFD_FILE_DIR_LENGTH -
                            INCOMING_DIR_LENGTH - 1;

               (void)memcpy(local_remote_part, dnb[i].dir_name, length);
               local_remote_part[length] = '\0';
            }
            i = *no_of_dir_names;
         }
      }

      ptr -= AFD_WORD_OFFSET;
#ifdef HAVE_MMAP
      if (munmap(ptr, stat_buf.st_size) == -1)
#else
      if (munmap_emu(ptr) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to munmap() `%s' : %s", file, strerror(errno));
      }
   } /* if (stat_buf.st_size != 0) */

   if (close(fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Failed to close() `%s' : %s", file, strerror(errno));
   }

   return;
}
