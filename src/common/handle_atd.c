/*
 *  handle_atd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2014 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   handle_atd - attaches or detaches to ATD (AFD Typesize Data)
 **
 ** SYNOPSIS
 **   (void)attach_atd(char *alias)
 **   (void)detach_atd(void)
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
 **   23.03.2014 H.Kiehl Created
 **   23.04.2023 H.Kiehl Close file descriptor in detach_atd().
 **
 */
DESCR__E_M1

#include <stdio.h>                     /* fprintf()                      */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>                    /* access(), close()              */
#ifdef HAVE_MMAP
# include <sys/mman.h>                 /* munmap()                       */
#endif
#include <errno.h>
#include "aldadefs.h"
#include "mondefs.h"

/* Local global variables. */
static int                      atd_fd = -1;
static off_t                    atd_size;

/* External global variables. */
extern char                     *p_work_dir;
extern struct afd_typesize_data *atd;


/*############################ attach_atd() #############################*/
void
attach_atd(char *alias)
{
   char file_name[MAX_PATH_LENGTH],
        *ptr;

   (void)sprintf(file_name, "%s%s%s%s",
                 p_work_dir, FIFO_DIR, ATD_FILE_NAME, alias);
   if (access(file_name, R_OK) == 0)
   {
      if ((ptr = map_file(file_name, &atd_fd, &atd_size, NULL,
                          O_RDONLY)) == (caddr_t) -1)
      {
         (void)fprintf(stderr, "Failed to map_file() %s (%s %d)\n",
                       file_name, __FILE__, __LINE__);
         atd = NULL;
         atd_fd = -1;
      }
      else
      {
         atd = (struct afd_typesize_data *)ptr;
      }
   }
   else
   {
      if (errno != ENOENT)
      {
         (void)fprintf(stderr, "Failed to access() %s : %s (%s %d)\n",
                       file_name, strerror(errno), __FILE__, __LINE__);
      }
      atd = NULL;
   }

   return;
}


/*############################ detach_atd() #############################*/
void
detach_atd(void)
{
   if (atd != NULL)
   {
#ifdef HAVE_MMAP
      if (munmap((void *)atd, atd_size) == -1)
#else
      if (munmap_emu((void *)atd) < 0)
#endif
      {
         (void)fprintf(stderr,
                       "Failed to munmap() from ATD file : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
      }
      atd = NULL;
      if (atd_fd != -1)
      {
         if (close(atd_fd) == -1)
         {
            (void)fprintf(stderr,
                          "Failed to close() ATD file : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
         }
         atd_fd = -1;
      }
   }

   return;
}
