/*
 *  mdb_attach.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   mdb_attach - attaches to FD message cache
 **
 ** SYNOPSIS
 **   int mdb_attach(void)
 **
 ** DESCRIPTION
 **   The function mdb_attach() attaches to the FD message cache (MDB).
 **
 ** RETURN VALUES
 **   None. It exits when it fails, otherwise it attaches to the
 **   message cache data file.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   26.06.2023 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                    /* strcpy(), strcat(), strerror() */
#include <unistd.h>                    /* close(), read()                */
#include <stdlib.h>                    /* exit()                         */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>                 /* mmap()                         */
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>                    /* O_RDWR, O_RDONLY               */
#endif
#include <errno.h>
#include "fddefs.h"

/* External global variables. */
extern int                  mdb_fd,
                            *no_msg_cached;
extern char                 *p_work_dir;
extern struct msg_cache_buf *mdb;


/*############################# mdb_attach() ############################*/
int
mdb_attach(void)
{
   int          created = NO;
   off_t        mdb_size;
   char         *ptr,
                mdb_file[MAX_PATH_LENGTH];

   (void)strcpy(mdb_file, p_work_dir);
   (void)strcat(mdb_file, FIFO_DIR);
   (void)strcat(mdb_file, MSG_CACHE_FILE);
   if ((mdb_fd = coe_open(mdb_file, O_RDWR)) == -1)
   {
      if (errno == ENOENT)
      {
         if ((mdb_fd = coe_open(mdb_file, O_RDWR | O_CREAT, FILE_MODE)) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to create `%s' : %s"),
                       mdb_file, strerror(errno));
            return(INCORRECT);
         }
         else
         {
            int  i,
                 loops,
                 rest;
            char buffer[4096];

            /* Create some default data. */
            mdb_size = (MSG_CACHE_BUF_SIZE * sizeof(struct msg_cache_buf)) +
                       AFD_WORD_OFFSET;

            (void)memset(buffer, 0, 4096);
            loops = mdb_size / 4096;
            rest = mdb_size % 4096;
            for (i = 0; i < loops; i++)
            {
               if (write(mdb_fd, buffer, 4096) != 4096)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Failed to write() to `%s' : %s"),
                             mdb_file, strerror(errno));
                  mdb_fd = -1;
                  return(INCORRECT);
               }
            }
            if (rest > 0)
            {
               if (write(mdb_fd, buffer, rest) != rest)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Failed to write() to `%s' : %s"),
                             mdb_file, strerror(errno));
                  mdb_fd = -1;
                  return(INCORRECT);
               }
            }
            if (lseek(mdb_fd, 0, SEEK_SET) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to lseek() in `%s' : %s"),
                          mdb_file, strerror(errno));
               mdb_fd = -1;
               return(INCORRECT);
            }
            created = YES;
         }
      }
      else
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to open() `%s' : %s"),
                    mdb_file, strerror(errno));
         return(INCORRECT);
      }
   }
   else
   {
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat  stat_buf;
#endif

#ifdef HAVE_STATX
      if (statx(mdb_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(mdb_fd, &stat_buf) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                    _("Failed to statx() `%s' : %s"),
#else
                    _("Failed to fstat() `%s' : %s"),
#endif
                    mdb_file, strerror(errno));
         (void)close(mdb_fd);
         mdb_fd = -1;
         return(INCORRECT);
      }
#ifdef HAVE_STATX
      mdb_size = stat_buf.stx_size;
#else
      mdb_size = stat_buf.st_size;
#endif
   }

   /* Lock the file. */
#ifdef LOCK_DEBUG
   if (lock_region(mdb_fd, 0, __FILE__, __LINE__) == LOCK_IS_SET)
#else
   if (lock_region(mdb_fd, 0) == LOCK_IS_SET)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "%s is already locked.", mdb_file);
      return(INCORRECT);
   }

#ifdef HAVE_MMAP
   if ((ptr = mmap(NULL, mdb_size, (PROT_READ | PROT_WRITE),
                   MAP_SHARED, mdb_fd, 0)) == (caddr_t) -1)
#else
   if ((ptr = mmap_emu(NULL, mdb_size, (PROT_READ | PROT_WRITE),
                       MAP_SHARED, mdb_file, 0)) == (caddr_t) -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("mmap() error : %s"), strerror(errno));
      (void)close(mdb_fd);
      mdb_fd = -1;
      return(INCORRECT);
   }
   if (created == YES)
   {
      *(ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
      *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
      *(ptr + SIZEOF_INT + 1 + 1 + 1) = CURRENT_MDB_VERSION;
      (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
   }
   else
   {
      if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_MDB_VERSION)
      {
         int           old_no_msg_cached = *(int *)ptr;
         unsigned char old_version = *(ptr + SIZEOF_INT + 1 + 1 + 1);

#ifdef HAVE_MMAP
         if (munmap(ptr, mdb_size) == -1)
#else
         if (munmap_emu(ptr) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to munmap() MDB [FD] : %s"),
                       strerror(errno));
         }

         if ((ptr = convert_mdb(mdb_fd, mdb_file, &mdb_size,
                                old_no_msg_cached, old_version,
                                CURRENT_MDB_VERSION)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to convert MDB file %s", mdb_file);
            *no_msg_cached = 0;
            (void)close(mdb_fd);
            mdb_fd = -1;

            return(INCORRECT);
         }
      }
   }
   no_msg_cached = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   mdb = (struct msg_cache_buf *)ptr;

   return(SUCCESS);
}
