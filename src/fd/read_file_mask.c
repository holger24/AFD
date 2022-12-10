/*
 *  read_file_mask.c - Part of AFD, an automatic file distribution
 *                     program.
 *  Copyright (c) 2000 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   read_file_mask - reads all file masks from a file
 **
 ** SYNOPSIS
 **   int read_file_mask(char *dir_alias, int *nfg, struct file_mask **fml)
 **
 ** DESCRIPTION
 **   The function read_file_mask reads all file masks from the
 **   file:
 **       $AFD_WORK_DIR/files/incoming/file_mask/<dir_alias>
 **   It reads the data into a buffer that this function will
 **   create.
 **
 ** RETURN VALUES
 **   Returns SUCCESS when it manages to store the data. Otherwise
 **   INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   15.08.2000 H.Kiehl Created
 **   28.12.2003 H.Kiehl Made number of file mask unlimited.
 **
 */
DESCR__E_M3


#include <stdio.h>            /* snprintf()                              */
#include <string.h>           /* strerror()                              */
#include <stdlib.h>           /* malloc()                                */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "fddefs.h"

/* #define DEBUG_FILE_LIST */

/* External global variables. */
extern char *p_work_dir;


/*########################### read_file_mask() ##########################*/
int
read_file_mask(char *dir_alias, int *nfg, struct file_mask **fml)
{
   int          fd,
                i;
   char         *buffer,
                file_mask_file[MAX_PATH_LENGTH],
                *ptr;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   if (snprintf(file_mask_file, MAX_PATH_LENGTH, "%s%s%s%s/%s",
                p_work_dir, AFD_FILE_DIR, INCOMING_DIR, FILE_MASK_DIR,
                dir_alias) >= MAX_PATH_LENGTH)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Storage for file_mask_file not large (%d bytes) enough!",
                 MAX_PATH_LENGTH);
      return(INCORRECT);
   }

   if ((fd = lock_file(file_mask_file, ON)) < 0)
   {
      return(fd);
   }
#ifdef HAVE_STATX
   if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(fd, &stat_buf) == -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                 "Failed to statx() `%s' : %s",
#else
                 "Failed to fstat() `%s' : %s",
#endif
                 file_mask_file, strerror(errno));
      (void)close(fd);
      return(INCORRECT);
   }
#ifdef HAVE_STATX
   if (stat_buf.stx_size == 0)
#else
   if (stat_buf.st_size == 0)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "File mask file `%s' is empty!", file_mask_file);
      (void)close(fd);
      return(INCORRECT);
   }
#ifdef HAVE_STATX
   if ((buffer = malloc(stat_buf.stx_size)) == NULL)
#else
   if ((buffer = malloc(stat_buf.st_size)) == NULL)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                 "Failed to malloc() %ld bytes : %s",
#else
                 "Failed to malloc() %lld bytes : %s",
#endif
#ifdef HAVE_STATX
                 (pri_off_t)stat_buf.stx_size,
#else
                 (pri_off_t)stat_buf.st_size,
#endif
                 strerror(errno));
      (void)close(fd);
      return(INCORRECT);
   }
#ifdef HAVE_STATX
   if (read(fd, buffer, stat_buf.stx_size) != stat_buf.stx_size)
#else
   if (read(fd, buffer, stat_buf.st_size) != stat_buf.st_size)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                 "Failed to read() %ld bytes from `%s' : %s",
#else
                 "Failed to read() %lld bytes from `%s' : %s",
#endif
#ifdef HAVE_STATX
                 (pri_off_t)stat_buf.stx_size,
#else
                 (pri_off_t)stat_buf.st_size,
#endif
                 file_mask_file, strerror(errno));
      free(buffer);
      (void)close(fd);
      return(INCORRECT);
   }
   ptr = buffer;
   *nfg = *(int *)ptr;
   ptr += sizeof(int);
   if ((*fml = malloc(*nfg * sizeof(struct file_mask))) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() %d bytes : %s",
                 (*nfg * sizeof(struct file_mask)), strerror(errno));
      free(buffer);
      (void)close(fd);
      return(INCORRECT);
   }

   for (i = 0; i < *nfg; i++)
   {
      (*fml)[i].fc = *(int *)ptr;
      ptr += sizeof(int);
      (*fml)[i].fbl = *(int *)ptr;
      ptr += sizeof(int);
      if (((*fml)[i].file_list = malloc((*fml)[i].fbl)) == NULL)
      {
         int k;

         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to malloc() %d bytes : %s",
                    (*fml)[i].fbl, strerror(errno));
         for (k = 0; k < i; k++)
         {
            free((*fml)[k].file_list);
         }
         free(buffer);
         (void)close(fd);
         return(INCORRECT);
      }
      (void)memcpy((*fml)[i].file_list, ptr, (*fml)[i].fbl);
      ptr += (*fml)[i].fbl;
   }
#ifdef DEBUG_FILE_LIST
   for (i = 0; i < *nfg; i++)
   {
      ptr = (*fml)[i].file_list;
      for (j = 0; j < (*fml)[i].fc; j++)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "%d %d: %s", i, j, ptr);
         NEXT(ptr);
      }
   }
#endif
   free(buffer);
   if (close(fd) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__, "Failed to close() `%s' : %s",
                 file_mask_file, strerror(errno));
   }

   return(SUCCESS);
}
