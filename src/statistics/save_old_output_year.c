/*
 *  save_old_output_year.c - Part of AFD, an automatic file distribution
 *                           program.
 *  Copyright (c) 1998 - 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   save_old_output_year - saves all output information of one year
 **                          into a separate file
 **
 ** SYNOPSIS
 **   void save_old_out_put_year(int new_year)
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
 **   10.05.1998 H.Kiehl Created
 **   21.02.2003 H.Kiehl Put in version number at beginning of structure.
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
extern int            no_of_hosts;
extern char           statistic_file[MAX_PATH_LENGTH],
                      new_statistic_file[MAX_PATH_LENGTH];
extern struct afdstat *stat_db;


/*$$$$$$$$$$$$$$$$$$$$$$$$$ save_old_output_year() $$$$$$$$$$$$$$$$$$$$$$*/
void
save_old_output_year(int new_year)
{
   int                  i,
                        old_year_fd;
   size_t               size = DAYS_PER_YEAR * sizeof(struct statistics),
                        old_year_stat_size;
   char                 new_file[MAX_PATH_LENGTH],
                        *ptr;
   struct afd_year_stat *old_stat_db = NULL;

   system_log(INFO_SIGN, __FILE__, __LINE__,
              "Saving output statistics for year %d", new_year - 1);

   /*
    * Rename the file we are currently mapped to, to the new
    * year.
    */
   (void)strcpy(new_file, statistic_file);
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
   if (rename(statistic_file, new_file) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to rename() %s to %s : %s",
                 statistic_file, new_file, strerror(errno));
      return;
   }
   ptr = new_statistic_file + strlen(new_statistic_file) - 1;
   while ((*ptr != '.') && (ptr != new_statistic_file))
   {
      ptr--;
   }
   if (*ptr == '.')
   {
      ptr++;
   }
   (void)sprintf(ptr, "%d", new_year);

   old_year_stat_size = AFD_WORD_OFFSET +
                        (no_of_hosts * sizeof(struct afd_year_stat));

   /* Database file does not yet exist, so create it */
   if ((old_year_fd = open(statistic_file, (O_RDWR | O_CREAT | O_TRUNC),
                           FILE_MODE)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not open() %s : %s",
                 statistic_file, strerror(errno));
      (void)strcpy(statistic_file, new_file);
      return;
   }
#ifdef HAVE_MMAP
   if (lseek(old_year_fd, old_year_stat_size - 1, SEEK_SET) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not seek() on %s : %s",
                 statistic_file, strerror(errno));
      (void)strcpy(statistic_file, new_file);
      (void)close(old_year_fd);
      return;
   }
   if (write(old_year_fd, "", 1) != 1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not write() to %s : %s",
                 statistic_file, strerror(errno));
      (void)strcpy(statistic_file, new_file);
      (void)close(old_year_fd);
      return;
   }
   if ((ptr = mmap(NULL, old_year_stat_size, (PROT_READ | PROT_WRITE),
                   (MAP_FILE | MAP_SHARED), old_year_fd, 0)) == (caddr_t) -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not mmap() file %s : %s",
                 statistic_file, strerror(errno));
      (void)strcpy(statistic_file, new_file);
      (void)close(old_year_fd);
      return;
   }
   if (close(old_year_fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }
#else
   if ((ptr = malloc(old_year_stat_size)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      (void)strcpy(statistic_file, new_file);
      (void)close(old_year_fd);
      return;
   }
#endif
   *(int*)ptr = no_of_hosts;
   *(ptr + sizeof(int) + 1 + 1 + 1) = CURRENT_YEAR_STAT_VERSION;
   old_stat_db = (struct afd_year_stat *)(ptr + AFD_WORD_OFFSET);
   (void)memset(old_stat_db, 0, old_year_stat_size - AFD_WORD_OFFSET);

   /*
    * Now copy the data from the old year.
    */
   for (i = 0; i < no_of_hosts; i++)
   {
      (void)strcpy(old_stat_db[i].hostname, stat_db[i].hostname);
      old_stat_db[i].start_time = stat_db[i].start_time;
      (void)memcpy(&old_stat_db[i].year, &stat_db[i].year, size);
   }

#ifdef HAVE_MMAP
   if (munmap(ptr, old_year_stat_size) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to munmap() %s : %s",
                 statistic_file, strerror(errno));
   }
#else
   if (write(old_year_fd, ptr, old_year_stat_size) != old_year_stat_size)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not write() to %s : %s",
                 statistic_file, strerror(errno));
      (void)strcpy(statistic_file, new_file);
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

   (void)strcpy(statistic_file, new_file);

   return;
}
