/*
 *  fra_attach_pos.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   fra_attach_pos - attaches to the FRA at the given position
 **
 ** SYNOPSIS
 **   int  fra_attach_pos(int pos)
 **   void fra_detach_pos(int pos)
 **
 ** DESCRIPTION
 **   Attaches to the memory mapped area of the FRA (File Retrieve
 **   Status Area), but only to the given position.
 **
 ** RETURN VALUES
 **   SUCCESS when attaching to the FRA successful and sets the
 **   global pointer 'fra' to the start of the FRA structure element at
 **   'pos'. If the FRA is not found or is marked as stale,
 **   WRONG_FRA_FILE is returned. Otherwise INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.07.2019 H.Kiehl Copied from fsa_attach_pos.c and then adapted.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                      /* strerror(), strlen()         */
#include <unistd.h>                      /* close(), read()              */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>                   /* mmap(), munmap()             */
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>                      /* fcntl()                      */
#endif
#include <errno.h>
#include "fddefs.h"

/* Global variables. */
extern int                        fra_fd,
                                  fra_id,
                                  *p_no_of_dirs;
#ifdef HAVE_MMAP
extern off_t                      fra_size;
#endif
extern char                       *p_work_dir;
extern struct fileretrieve_status *fra;

/* Local global variables. */
static int                        map_offset;

#define WRONG_FRA_FILE INCORRECT


/*########################## fra_attach_pos() ###########################*/
int
fra_attach_pos(int pos)
{
   int  ret = SUCCESS;
   char fra_stat_file[MAX_PATH_LENGTH];

   /* Get absolute path of FRA_ID_FILE. */
   (void)snprintf(fra_stat_file, MAX_PATH_LENGTH, "%s%s%s.%d",
                  p_work_dir, FIFO_DIR, FRA_STAT_FILE, fra_id);
   if ((fra_fd = open(fra_stat_file, O_RDWR)) == -1)
   {
      if (errno != ENOENT)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to open() %s : %s", fra_stat_file, strerror(errno));
         ret = INCORRECT;
      }
      else
      {
         int fd;

         (void)snprintf(fra_stat_file, MAX_PATH_LENGTH, "%s%s%s",
                        p_work_dir, FIFO_DIR, FRA_ID_FILE);
         if ((fd = open(fra_stat_file, O_RDWR)) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to open() %s : %s",
                       fra_stat_file, strerror(errno));
            ret = INCORRECT;
         }
         else
         {
            struct flock wlock = {F_WRLCK, SEEK_SET, 0, 1};

            if (fcntl(fd, F_SETLKW, &wlock) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to lock %s : %s",
                          fra_stat_file, strerror(errno));
               ret = INCORRECT;
            }
            else
            {
               int          read_stat,
                            tmp_errno;
               struct flock ulock = {F_UNLCK, SEEK_SET, 0, 1};

               read_stat = read(fd, &fra_id, sizeof(int));
               tmp_errno = errno;
               if (fcntl(fd, F_SETLKW, &ulock) == -1)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Failed to unlock %s : %s",
                             fra_stat_file, strerror(errno));
               }
               if (close(fd) == -1)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Failed to close() %s : %s",
                             fra_stat_file, strerror(errno));
               }
               if (read_stat == INCORRECT)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to read() %s : %s",
                             fra_stat_file, strerror(tmp_errno));
                  ret = INCORRECT;
               }
               else
               {
                  (void)snprintf(fra_stat_file, MAX_PATH_LENGTH, "%s%s%s.%d",
                                 p_work_dir, FIFO_DIR, FRA_STAT_FILE, fra_id);
                  if ((fra_fd = open(fra_stat_file, O_RDWR)) == -1)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Failed to open() %s : %s",
                                fra_stat_file, strerror(errno));
                     ret = INCORRECT;
                  }
               }
            }
         }
      }
   }

   if (ret == SUCCESS)
   {
      char *ptr;

#ifdef HAVE_MMAP
      if ((ptr = mmap(NULL, AFD_WORD_OFFSET, PROT_READ,
# ifdef _HPUX
                      MAP_PRIVATE, fra_fd, 0)) == (caddr_t) -1)
# else
                      MAP_SHARED, fra_fd, 0)) == (caddr_t) -1)
# endif
#else
      if ((ptr = mmap_emu(NULL, AFD_WORD_OFFSET, PROT_READ,
                          MAP_SHARED, fra_stat_file, 0)) == (caddr_t) -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to mmap() %s : %s",
                    fra_stat_file, strerror(errno));
         ret = INCORRECT;
      }
      else
      {
         p_no_of_dirs = (int *)ptr;
         if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_FRA_VERSION)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "This code is compiled for of FRA version %d, but the FRA we try to attach is %d.\n",
                       CURRENT_FRA_VERSION,
                       (int)(*(ptr + SIZEOF_INT + 1 + 1 + 1)));
            ret = WRONG_FRA_FILE;
         }
         else
         {
            if (*p_no_of_dirs <= 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Hmmm, number of dirs is %d. How can this be?",
                          *p_no_of_dirs);
               ret = WRONG_FRA_FILE;
            }
            else
            {
               if (pos < *p_no_of_dirs)
               {
                  int pagesize;

                  pagesize = *(int *)(ptr + SIZEOF_INT + 4);
                  if ((pagesize < 0) || (pagesize == 0))
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "Hmmm, pagesize is %d", pagesize);
                     if ((pagesize = (int)sysconf(_SC_PAGESIZE)) == -1)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "Failed to determine the pagesize with sysconf() : %s",
                                   strerror(errno));
                        ret = INCORRECT;
                     }
                  }

                  if (pagesize > 0)
                  {
                     int page_offset,
                         start;

                     start = AFD_WORD_OFFSET + (pos * sizeof(struct fileretrieve_status));
                     page_offset = (start / pagesize) * pagesize;
                     map_offset = start - page_offset;
#ifdef HAVE_MMAP
                     fra_size = sizeof(struct fileretrieve_status) + map_offset;
                     if ((ptr = mmap(NULL, fra_size, (PROT_READ | PROT_WRITE),
                                     MAP_SHARED, fra_fd,
                                     page_offset)) == (caddr_t) -1)
#else
                     if ((ptr = mmap_emu(NULL, sizeof(struct fileretrieve_status) + map_offset,
                                         (PROT_READ | PROT_WRITE),
                                         MAP_SHARED, fra_stat_file,
                                         page_offset)) == (caddr_t) -1)
#endif
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "mmap() error : %s", strerror(errno));
                        ret = INCORRECT;
                     }
                     else
                     {
                        fra = (struct fileretrieve_status *)(ptr + map_offset);
                        ret = SUCCESS;
                     }
                  }
               }
               else
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Hmm, pos %d is equal or beyond no_of_dirs %d. Assume we are in wrong FRA.",
                             pos, *p_no_of_dirs);
                  ret = WRONG_FRA_FILE;
               }
            }
         }
      }

      /*
       * NOTE: Leave fra_fd open, we need it for locking certain elements
       *       in the FRA.
       */
   }
   return(ret);
}


/*########################## fra_detach_pos() ###########################*/
void
fra_detach_pos(int pos)
{
   if (fra_fd > 0)
   {
      if (close(fra_fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "close() error : %s", strerror(errno));
      }
      fra_fd = -1;
   }

   if (fra != NULL)
   {
      char *ptr;

      ptr = (char *)p_no_of_dirs;
#ifdef HAVE_MMAP
      if (munmap(ptr, AFD_WORD_OFFSET) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to munmap() no_of_dirs from FRA : %s",
                    strerror(errno));
      }
      ptr = (char *)fra - map_offset;
      if (munmap(ptr, fra_size) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
# if SIZEOF_OFF_T == 4
                    "Failed to munmap() from FRA position %d [fra_size = %ld] : %s",
# else
                    "Failed to munmap() from FRA position %d [fra_size = %lld] : %s",
# endif
                    pos, (pri_off_t)fra_size, strerror(errno));
      }
#else
      if (munmap_emu(ptr) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to munmap_emu() no_of_dirs from FRA : %s",
                    strerror(errno));
      }
      ptr = (char *)fra;
      if (munmap_emu(ptr) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to munmap_emu() from FRA position %d : %s",
                    pos, strerror(errno));
      }
#endif
      fra = NULL;
   }

   return;
}
