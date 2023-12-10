/*
 *  fsa_attach_pos.c - Part of AFD, an automatic file distribution program.
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
 **   fsa_attach_pos - attaches to the FSA at the given position
 **
 ** SYNOPSIS
 **   int  fsa_attach_pos(int pos)
 **   void fsa_detach_pos(int pos)
 **
 ** DESCRIPTION
 **   Attaches to the memory mapped area of the FSA (File Transfer
 **   Status Area), but only to the given position.
 **
 ** RETURN VALUES
 **   SUCCESS when attaching to the FSA successful and sets the
 **   global pointer 'fsa' to the start of the FSA structure element at
 **   'pos'. If the FSA is not found or is marked as stale,
 **   WRONG_FSA_FILE is returned. Otherwise INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   09.02.2003 H.Kiehl Created
 **   06.04.2005 H.Kiehl Added fsa_detach_pos().
 **   04.06.2007 H.Kiehl Added fix for HPUX which does not allow more
 **                      then one mmap() on the same file.
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
extern int                        fsa_fd,
                                  fsa_id,
                                  fsa_pos_save,
                                  *p_no_of_hosts;
#ifdef HAVE_MMAP
extern off_t                      fsa_size;
#endif
extern char                       *p_work_dir;
extern struct filetransfer_status *fsa;

/* Local global variables. */
static int                        map_offset;

#define WRONG_FSA_FILE INCORRECT


/*########################## fsa_attach_pos() ###########################*/
int
fsa_attach_pos(int pos)
{
   int  ret = SUCCESS;
   char fsa_stat_file[MAX_PATH_LENGTH];

   /* Get absolute path of FSA_ID_FILE. */
   (void)snprintf(fsa_stat_file, MAX_PATH_LENGTH, "%s%s%s.%d",
                  p_work_dir, FIFO_DIR, FSA_STAT_FILE, fsa_id);
   if ((fsa_fd = open(fsa_stat_file, O_RDWR)) == -1)
   {
      if (errno != ENOENT)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to open() %s : %s", fsa_stat_file, strerror(errno));
         ret = INCORRECT;
      }
      else
      {
         int fd;

         (void)snprintf(fsa_stat_file, MAX_PATH_LENGTH, "%s%s%s",
                        p_work_dir, FIFO_DIR, FSA_ID_FILE);
         if ((fd = open(fsa_stat_file, O_RDWR)) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to open() %s : %s",
                       fsa_stat_file, strerror(errno));
            ret = INCORRECT;
         }
         else
         {
            struct flock wlock = {F_WRLCK, SEEK_SET, 0, 1};

            if (fcntl(fd, F_SETLKW, &wlock) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to lock %s : %s",
                          fsa_stat_file, strerror(errno));
               ret = INCORRECT;
            }
            else
            {
               int          read_stat,
                            tmp_errno;
               struct flock ulock = {F_UNLCK, SEEK_SET, 0, 1};

               read_stat = read(fd, &fsa_id, sizeof(int));
               tmp_errno = errno;
               if (fcntl(fd, F_SETLKW, &ulock) == -1)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Failed to unlock %s : %s",
                             fsa_stat_file, strerror(errno));
               }
               if (close(fd) == -1)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Failed to close() %s : %s",
                             fsa_stat_file, strerror(errno));
               }
               if (read_stat == INCORRECT)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to read() %s : %s",
                             fsa_stat_file, strerror(tmp_errno));
                  ret = INCORRECT;
               }
               else
               {
                  (void)snprintf(fsa_stat_file, MAX_PATH_LENGTH, "%s%s%s.%d",
                                 p_work_dir, FIFO_DIR, FSA_STAT_FILE, fsa_id);
                  if ((fsa_fd = open(fsa_stat_file, O_RDWR)) == -1)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Failed to open() %s : %s",
                                fsa_stat_file, strerror(errno));
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
                      MAP_PRIVATE, fsa_fd, 0)) == (caddr_t) -1)
# else
                      MAP_SHARED, fsa_fd, 0)) == (caddr_t) -1)
# endif
#else
      if ((ptr = mmap_emu(NULL, AFD_WORD_OFFSET, PROT_READ,
                          MAP_SHARED, fsa_stat_file, 0)) == (caddr_t) -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to mmap() %s : %s",
                    fsa_stat_file, strerror(errno));
         ret = INCORRECT;
      }
      else
      {
         p_no_of_hosts = (int *)ptr;
         if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_FSA_VERSION)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "This code is compiled for of FSA version %d, but the FSA we try to attach is %d.\n",
                       CURRENT_FSA_VERSION,
                       (int)(*(ptr + SIZEOF_INT + 1 + 1 + 1)));
            ret = WRONG_FSA_FILE;
         }
         else
         {
            if (*p_no_of_hosts <= 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Hmmm, number of hosts is %d. How can this be?",
                          *p_no_of_hosts);
               ret = WRONG_FSA_FILE;
            }
            else
            {
               if (pos < *p_no_of_hosts)
               {
                  int pagesize;

                  pagesize = *(int *)(ptr + SIZEOF_INT + 4);
                  if (pagesize < 0)
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

                     start = AFD_WORD_OFFSET + (pos * sizeof(struct filetransfer_status));
                     page_offset = (start / pagesize) * pagesize;
                     map_offset = start - page_offset;
#ifdef HAVE_MMAP
                     fsa_size = sizeof(struct filetransfer_status) + map_offset;
                     if ((ptr = mmap(NULL, fsa_size, (PROT_READ | PROT_WRITE),
                                     MAP_SHARED, fsa_fd,
                                     page_offset)) == (caddr_t) -1)
#else
                     if ((ptr = mmap_emu(NULL, sizeof(struct filetransfer_status) + map_offset,
                                         (PROT_READ | PROT_WRITE),
                                         MAP_SHARED, fsa_stat_file,
                                         page_offset)) == (caddr_t) -1)
#endif
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "mmap() error : %s", strerror(errno));
                        ret = INCORRECT;
                     }
                     else
                     {
                        fsa = (struct filetransfer_status *)(ptr + map_offset);
                        fsa_pos_save = YES;
                        ret = SUCCESS;
                     }
                  }
               }
               else
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Hmm, pos %d is equal or beyond no_of_hosts %d. Assume we are in wrong FSA.",
                             pos, *p_no_of_hosts);
                  ret = WRONG_FSA_FILE;
               }
            }
         }
      }

      /*
       * NOTE: Leave fsa_fd open, we need it for locking certain elements
       *       in the FSA.
       */
   }
   return(ret);
}


/*########################## fsa_detach_pos() ###########################*/
void
fsa_detach_pos(int pos)
{
   if (fsa_fd > 0)
   {
      if (close(fsa_fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "close() error : %s", strerror(errno));
      }
      fsa_fd = -1;
   }

   if (fsa != NULL)
   {
      char *ptr;

      ptr = (char *)p_no_of_hosts;
#ifdef HAVE_MMAP
      if (munmap(ptr, AFD_WORD_OFFSET) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to munmap() no_of_hosts from FSA : %s",
                    strerror(errno));
      }
      ptr = (char *)fsa - map_offset;
      if (munmap(ptr, fsa_size) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
# if SIZEOF_OFF_T == 4
                    "Failed to munmap() from FSA position %d [fsa_size = %ld] : %s",
# else
                    "Failed to munmap() from FSA position %d [fsa_size = %lld] : %s",
# endif
                    pos, (pri_off_t)fsa_size, strerror(errno));
      }
#else
      if (munmap_emu(ptr) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to munmap_emu() no_of_hosts from FSA : %s",
                    strerror(errno));
      }
      ptr = (char *)fsa;
      if (munmap_emu(ptr) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to munmap_emu() from FSA position %d : %s",
                    pos, strerror(errno));
      }
#endif
      fsa = NULL;
   }

   return;
}
