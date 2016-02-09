/*
 *  save_old_input_year.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   save_old_input_year - saves all input information of one year
 **                         into a separate file
 **
 ** SYNOPSIS
 **   void save_old_input_year(int new_year)
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
 **   23.02.2003 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>          /* strcpy(), strerror(), memcpy(), memset() */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
#include <sys/mman.h>        /* mmap(), munmap()                         */
#else
#include <stdlib.h>          /* malloc(), free()                         */
#endif
#include <fcntl.h>
#include <unistd.h>          /* close(), lseek(), write()                */
#include <errno.h>
#include "statdefs.h"

#ifdef HAVE_MMAP
#ifndef MAP_FILE    /* Required for BSD          */
#define MAP_FILE 0  /* All others do not need it */
#endif
#endif

/* Global external variables. */
extern int             no_of_dirs;
extern char            istatistic_file[MAX_PATH_LENGTH],
                       new_istatistic_file[MAX_PATH_LENGTH];
extern struct afdistat *istat_db;


/*$$$$$$$$$$$$$$$$$$$$$$$$$ save_old_input_year() $$$$$$$$$$$$$$$$$$$$$$$*/
void
save_old_input_year(int new_year)
{
   int                   i,
                         old_year_fd;
   size_t                size = DAYS_PER_YEAR * sizeof(struct istatistics),
                         old_year_istat_size;
   char                  new_file[MAX_PATH_LENGTH],
                         *ptr;
   struct afd_year_istat *old_istat_db = NULL;

   system_log(INFO_SIGN, __FILE__, __LINE__,
              "Saving input statistics for year %d", new_year - 1);

   /*
    * Rename the file we are currently mapped to, to the new
    * year.
    */
   (void)strcpy(new_file, istatistic_file);
   ptr = new_file + strlen(new_file) - 1;
   while ((*ptr != '.') && (ptr != new_file))
   {
      ptr--;
   }
   if (*ptr == '.')
   {
      ptr++;
   }
   (void)sprintf(ptr, "%d", new_year);
   if (rename(istatistic_file, new_file) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to rename() %s to %s : %s",
                 istatistic_file, new_file, strerror(errno));
      return;
   }
   ptr = new_istatistic_file + strlen(new_istatistic_file) - 1;
   while ((*ptr != '.') && (ptr != new_istatistic_file))
   {
      ptr--;
   }
   if (*ptr == '.')
   {
      ptr++;
   }
   (void)sprintf(ptr, "%d", new_year);

   old_year_istat_size = AFD_WORD_OFFSET +
                         (no_of_dirs * sizeof(struct afd_year_istat));

   /* Database file does not yet exist, so create it */
   if ((old_year_fd = open(istatistic_file, (O_RDWR | O_CREAT | O_TRUNC),
                           FILE_MODE)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not open() %s : %s", istatistic_file, strerror(errno));
      (void)strcpy(istatistic_file, new_file);
      return;
   }
#ifdef HAVE_MMAP
   if (lseek(old_year_fd, old_year_istat_size - 1, SEEK_SET) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not seek() on %s : %s",
                 istatistic_file, strerror(errno));
      (void)strcpy(istatistic_file, new_file);
      (void)close(old_year_fd);
      return;
   }
   if (write(old_year_fd, "", 1) != 1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not write() to %s : %s",
                 istatistic_file, strerror(errno));
      (void)strcpy(istatistic_file, new_file);
      (void)close(old_year_fd);
      return;
   }
   if ((ptr = mmap(NULL, old_year_istat_size, (PROT_READ | PROT_WRITE),
                   (MAP_FILE | MAP_SHARED), old_year_fd, 0)) == (caddr_t) -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not mmap() file %s : %s",
                 istatistic_file, strerror(errno));
      (void)strcpy(istatistic_file, new_file);
      (void)close(old_year_fd);
      return;
   }
   if (close(old_year_fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }
#else
   if ((ptr = malloc(old_year_istat_size)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      (void)strcpy(istatistic_file, new_file);
      (void)close(old_year_fd);
      return;
   }
#endif
   *(int*)ptr = no_of_dirs;
   *(ptr + sizeof(int) + 1 + 1 + 1) = CURRENT_YEAR_ISTAT_VERSION;
   old_istat_db = (struct afd_year_istat *)(ptr + AFD_WORD_OFFSET);
   (void)memset(old_istat_db, 0, old_year_istat_size - AFD_WORD_OFFSET);

   /*
    * Now copy the data from the old year.
    */
   for (i = 0; i < no_of_dirs; i++)
   {
      (void)strcpy(old_istat_db[i].dir_alias, istat_db[i].dir_alias);
      old_istat_db[i].start_time = istat_db[i].start_time;
      (void)memcpy(&old_istat_db[i].year, &istat_db[i].year, size);
   }

#ifdef HAVE_MMAP
   if (munmap(ptr, old_year_istat_size) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to munmap() %s : %s",
                 istatistic_file, strerror(errno));
   }
#else
   if (write(old_year_fd, ptr, old_year_istat_size) != old_year_istat_size)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not write() to %s : %s",
                 istatistic_file, strerror(errno));
      (void)strcpy(istatistic_file, new_file);
      (void)close(old_year_fd);
      free(ptr);
      return;
   }
   if (close(old_year_fd) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }
   free(ptr);
#endif

   (void)strcpy(istatistic_file, new_file);

   return;
}
