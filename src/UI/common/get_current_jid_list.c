/*
 *  get_current_jid_list.c - Part of AFD, an automatic file distribution
 *                           program.
 *  Copyright (c) 1999 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_current_jid_list - creates a list of current job ID's
 **
 ** SYNOPSIS
 **   int get_current_jid_list(void)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS when it managed to create an array with the current job
 **   ID's, otherwise INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.02.1999 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>      /* NULL                                          */
#include <string.h>     /* strerror()                                    */
#include <unistd.h>     /* close()                                       */
#include <sys/mman.h>   /* mmap()                                        */
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>     /* malloc(), free()                              */
#include <fcntl.h>
#include <errno.h>
#include "ui_common_defs.h"

/* External global variables. */
extern unsigned int *current_jid_list;
extern int          no_of_current_jobs;
extern char         *p_work_dir;


/*######################## get_current_jid_list() #######################*/
int
get_current_jid_list(void)
{
   int          fd;
   char         file[MAX_PATH_LENGTH],
                *ptr,
                *tmp_ptr;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   (void)snprintf(file, MAX_PATH_LENGTH, "%s%s%s",
                  p_work_dir, FIFO_DIR, CURRENT_MSG_LIST_FILE);
   if ((fd = open(file, O_RDONLY)) == -1)
   {
      (void)xrec(ERROR_DIALOG,
                 "Failed to open() %s. Will not be able to get all information. : %s (%s %d)",
                 file, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

#ifdef HAVE_STATX
   if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(fd, &stat_buf) == -1)
#endif
   {
      (void)xrec(ERROR_DIALOG,
                 "Failed to access %s. Will not be able to get all information. : %s (%s %d)",
                 file, strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      return(INCORRECT);
   }

   if ((ptr = mmap(NULL,
#ifdef HAVE_STATX
                   stat_buf.stx_size, PROT_READ,
#else
                   stat_buf.st_size, PROT_READ,
#endif
                   MAP_SHARED, fd, 0)) == (caddr_t)-1)
   {
      (void)xrec(ERROR_DIALOG,
                 "Failed to mmap() to %s. Will not be able to get all information. : %s (%s %d)",
                 file, strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      return(INCORRECT);
   }
   tmp_ptr = ptr;
   no_of_current_jobs = *(int *)ptr;
   ptr += sizeof(int);

   if (no_of_current_jobs > 0)
   {
      int i;

      if ((current_jid_list = malloc(no_of_current_jobs * sizeof(unsigned int))) == NULL)
      {
         (void)xrec(ERROR_DIALOG,
                    "Failed to malloc() memory. Will not be able to get all information. : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return(INCORRECT);
      }
      for (i = 0; i < no_of_current_jobs; i++)
      {
         current_jid_list[i] = *(unsigned int *)ptr;
         ptr += sizeof(unsigned int);
      }
   }

#ifdef HAVE_STATX
   if (munmap(tmp_ptr, stat_buf.stx_size) == -1)
#else
   if (munmap(tmp_ptr, stat_buf.st_size) == -1)
#endif
   {
      (void)xrec(ERROR_DIALOG, "Failed to munmap() %s : %s (%s %d)",
                 file, strerror(errno), __FILE__, __LINE__);
   }
   if (close(fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }

   return(SUCCESS);
}
