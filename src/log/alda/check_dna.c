/*
 *  check_dna.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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

DESCR__S_M1
/*
 ** NAME
 **   check_dna - checks if DNA changed, if yes it remaps to this area
 **
 ** SYNOPSIS
 **   void check_dna(void)
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
 **   22.11.2008 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>         /* fprintf()                                  */
#include <string.h>        /* strerror()                                 */
#include <stdlib.h>
#include <unistd.h>        /* close()                                    */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>     /* mmap(), munmap()                           */
#endif
#include <fcntl.h>         /* O_RDONLY                                   */
#include <errno.h>
#include "aldadefs.h"


/* External global variables. */
extern char                 *p_work_dir;
extern struct dir_name_area dna;


/*############################# check_dna() #############################*/
void
check_dna(void)
{
   if (dna.fd > -1)
   {
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat stat_buf;
#endif

      if ((dna.buffer != NULL) &&
          ((*dna.no_of_dir_names != dna.initial_no_of_dir_names) ||
#ifdef HAVE_STATX
           ((statx(dna.fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                   STATX_MTIME, &stat_buf) == 0) && 
            (dna.mtime < stat_buf.stx_mtime.tv_sec))
#else
           ((fstat(dna.fd, &stat_buf) == 0) && 
            (dna.mtime < stat_buf.st_mtime))
#endif
          ))
      {
         if (close(dna.fd) == -1)
         {
            (void)fprintf(stderr, "close() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
         }
         dna.fd = -1;
#ifdef HAVE_MMAP
         if (munmap(dna.buffer, dna.size) == -1)
#else
         if (munmap_emu(dna.buffer) == -1)
#endif
         {
            (void)fprintf(stderr,
                          "Failed to unmap from %s : %s (%s %d)\n",
                          dna.filename, strerror(errno),
                          __FILE__, __LINE__);
         }
         dna.buffer = NULL;
         dna.dnb = NULL;
         dna.no_of_dir_names = NULL;
         dna.size = 0;
         dna.mtime = 0;
         dna.initial_no_of_dir_names = 0;
      }
   }
   if (dna.buffer == NULL)
   {
      if (dna.filename[0] == '\0')
      {
         (void)sprintf(dna.filename, "%s%s%s", p_work_dir, FIFO_DIR,
                       DIR_NAME_FILE);
      }
      if ((dna.fd = open(dna.filename, O_RDONLY)) == -1)
      {
         (void)fprintf(stderr, "Failed to open() %s : %s (%s %d)\n",
                       dna.filename, strerror(errno),
                       __FILE__, __LINE__);
      }
      else
      {
#ifdef HAVE_STATX
         struct statx stat_buf;
#else
         struct stat stat_buf;
#endif

#ifdef HAVE_STATX
         if (statx(dna.fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                   STATX_SIZE | STATX_MTIME, &stat_buf) == -1)
#else
         if (fstat(dna.fd, &stat_buf) == -1)
#endif
         {
            (void)fprintf(stderr,
                          "Failed to access %s : %s (%s %d)\n",
                          dna.filename, strerror(errno),
                          __FILE__, __LINE__);
         }
         else
         {
#ifdef HAVE_STATX
            if (stat_buf.stx_size > 0)
#else
            if (stat_buf.st_size > 0)
#endif
            {
#ifdef HAVE_STATX
               dna.size = stat_buf.stx_size;
               dna.mtime = stat_buf.stx_mtime.tv_sec;
               if ((dna.buffer = mmap(NULL, stat_buf.stx_size,
                                      PROT_READ, MAP_SHARED, dna.fd,
                                      0)) == (caddr_t) -1)
#else
               dna.size = stat_buf.st_size;
               dna.mtime = stat_buf.st_mtime;
               if ((dna.buffer = mmap(NULL, stat_buf.st_size,
                                      PROT_READ, MAP_SHARED, dna.fd,
                                      0)) == (caddr_t) -1)
#endif
               {
                  (void)fprintf(stderr,
                                "Failed to mmap() to %s : %s (%s %d)\n",
                                dna.filename, strerror(errno),
                                __FILE__, __LINE__);
               }
               else
               {
                  dna.no_of_dir_names = (int *)dna.buffer;
                  dna.dnb = (struct dir_name_buf *)(dna.buffer + AFD_WORD_OFFSET);
               }
            }
            else
            {
               (void)fprintf(stderr, "File %s is empty. (%s %d)\n",
                             dna.filename, __FILE__, __LINE__);
            }
         }
      }
   }

   return;
}
