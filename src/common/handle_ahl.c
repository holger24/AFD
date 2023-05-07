/*
 *  handle_ahl.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2009 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   handle_ahl - attaches or detaches to AHL (AFD Host List)
 **
 ** SYNOPSIS
 **   (void)attach_ahl(char *alias)
 **   (void)detach_ahl(void)
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
 **   07.12.2009 H.Kiehl Created
 **   23.04.2023 H.Kiehl Close file descriptor in detach_ahl().
 **
 */
DESCR__E_M1

#include <stdio.h>                     /* fprintf()                      */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>                    /* close()                        */
#include <fcntl.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>                 /* munmap()                       */
#endif
#include <errno.h>
#include "aldadefs.h"
#include "mondefs.h"

/* Local global variables. */
static int                  ahl_fd = -1;
static off_t                ahl_size;

/* External global variables. */
extern unsigned int         ahl_entries;
extern char                 *p_work_dir;
extern struct afd_host_list *ahl;


/*############################ attach_ahl() #############################*/
void
attach_ahl(char *alias)
{
   char file_name[MAX_PATH_LENGTH],
        *ptr;

   (void)sprintf(file_name, "%s%s%s%s",
                 p_work_dir, FIFO_DIR, AHL_FILE_NAME, alias);
   if ((ptr = map_file(file_name, &ahl_fd, &ahl_size, NULL,
                       O_RDONLY)) == (caddr_t) -1)
   {
      (void)fprintf(stderr, "Failed to map_file() %s (%s %d)\n",
                    file_name, __FILE__, __LINE__);
      ahl = NULL;
      ahl_fd = -1;
   }
   else
   {
      ahl = (struct afd_host_list *)ptr;
      ahl_entries = ahl_size / sizeof(struct afd_host_list);
   }

   return;
}


/*############################ detach_ahl() #############################*/
void
detach_ahl(void)
{
   if (ahl != NULL)
   {
#ifdef HAVE_MMAP
      if (munmap((void *)ahl, ahl_size) == -1)
#else
      if (munmap_emu((void *)ahl) < 0)
#endif
      {
         (void)fprintf(stderr,
                       "Failed to munmap() from AHL file : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
      }
      ahl = NULL;
      ahl_entries = 0;
      if (ahl_fd != -1)
      {
         if (close(ahl_fd) == -1)
         {
            (void)fprintf(stderr,
                          "Failed to close() AHL file : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
         }
         ahl_fd = -1;
      }
   }

   return;
}
