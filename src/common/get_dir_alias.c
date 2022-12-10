/*
 *  get_dir_alias.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2010 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_dir_alias - gets the directory alias name for a given job ID
 **
 ** SYNOPSIS
 **   void get_dir_alias(unsigned int job_id, char *dir_alias)
 **
 ** DESCRIPTION
 **   The function get_dir_alias() searches for the given job_id
 **   the relevant directory alias.
 **
 ** RETURN VALUES
 **   On the directory alias is returned in dir_alias. Otherwise
 **   an empty string will be returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.12.2010 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                    /* snprintf()                      */
#include <string.h>                   /* strcpy(), strcat(), strerror()  */
#include <sys/types.h>
#include <sys/stat.h>                 /* fstat()                         */
#include <fcntl.h>                    /* open(), close()                 */
#include <unistd.h>
#include <sys/mman.h>                 /* mmap(), munmap()                */
#include <errno.h>

/* External global variables. */
extern int                        fra_fd,
                                  no_of_dirs;
extern char                       *p_work_dir;
extern struct fileretrieve_status *fra;


/*########################### get_dir_alias() ###########################*/
void
get_dir_alias(unsigned int job_id, char *dir_alias)
{
   int          fd;
   unsigned int dir_id = 0;
   char         fullname[MAX_PATH_LENGTH];

   dir_alias[0] = '\0';
   (void)snprintf(fullname, MAX_PATH_LENGTH, "%s%s%s",
                  p_work_dir, FIFO_DIR, JOB_ID_DATA_FILE);
   if ((fd = open(fullname, O_RDONLY)) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to open() `%s' : %s", fullname, strerror(errno));
   }
   else
   {
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat  stat_buf;
#endif

#ifdef HAVE_STATX
      if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(fd, &stat_buf) == -1)
#endif
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                    "Failed to statx() `%s' : %s",
#else
                    "Failed to fstat() `%s' : %s",
#endif
                    fullname, strerror(errno));
      }
      else
      {
#ifdef HAVE_STATX
         if (stat_buf.stx_size > 0)
#else
         if (stat_buf.st_size > 0)
#endif
         {
            char *ptr;

            if ((ptr = mmap(NULL,
#ifdef HAVE_STATX
                            stat_buf.stx_size, PROT_READ,
#else
                            stat_buf.st_size, PROT_READ,
#endif
                            MAP_SHARED, fd, 0)) == (caddr_t) -1)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to mmap() to `%s' : %s",
                          fullname, strerror(errno));
            }
            else
            {
               if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_JID_VERSION)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Incorrect JID version (data=%d current=%d)!",
                             *(ptr + SIZEOF_INT + 1 + 1 + 1),
                             CURRENT_JID_VERSION);
               }
               else
               {
                  int                i,
                                     *no_of_job_ids;
                  struct job_id_data *jd;

                  no_of_job_ids = (int *)ptr;
                  ptr += AFD_WORD_OFFSET;
                  jd = (struct job_id_data *)ptr;

                  for (i = 0; i < *no_of_job_ids; i++)
                  {
                     if (job_id == jd[i].job_id)
                     {
                        dir_id = jd[i].dir_id;
                        break;
                     }
                  }
                  ptr -= AFD_WORD_OFFSET;
               }

               /* Don't forget to unmap from job_id_data structure. */
#ifdef HAVE_STATX
               if (munmap(ptr, stat_buf.stx_size) == -1)
#else
               if (munmap(ptr, stat_buf.st_size) == -1)
#endif
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "munmap() error : %s", strerror(errno));
               }
            }
         }
         else
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "File `%s' is empty! Terminating, don't know what to do :-(",
                       fullname);
         }
      }
      if (close(fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Failed to close() `%s' : %s", fullname, strerror(errno));
      }
   }

   if (dir_id != 0)
   {
      int attached = NO,
          i;

      if (fra_fd == -1)
      {
         if (fra_attach_passive() != SUCCESS)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to attach to FRA.");
            return;
         }
         attached = YES;
      }
      else
      {
         (void)check_fra(YES);
      }

      for (i = 0; i < no_of_dirs; i++)
      {
         if (dir_id == fra[i].dir_id)
         {
            (void)strcpy(dir_alias, fra[i].dir_alias);
            break;
         }
      }

      if (attached == YES)
      {
         fra_detach();
      }
   }

   return;
}
