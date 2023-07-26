/*
 *  convert_mdb.c - Part of AFD, an automatic file distribution program.
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

DESCR__S_M1
/*
 ** NAME
 **   convert_mdb - converts the MDB from an old format to new one
 **
 ** SYNOPSIS
 **   char *convert_mdb(int           old_mdb_fd,
 **                     char          *old_msg_cache_buf_file,
 **                     off_t         *old_mdb_size,
 **                     int           old_no_msg_cached,
 **                     unsigned char old_version,
 **                     unsigned char new_version)
 **
 ** DESCRIPTION
 **   When there is a change in the structure msg_cache_buf (MDB) it
 **   tries to convert this to the new structure. It currently only
 **   converts the following versions: 0 to 1.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.06.2023 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                    /* fprintf()                       */
#include <string.h>                   /* strcpy(), strcat(), strerror()  */
#include <stdlib.h>                   /* exit()                          */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>                /* mmap()                          */
#endif
#include <unistd.h>                   /* sysconf()                       */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>                   /* O_RDWR, O_CREAT, O_WRONLY, etc  */
#endif
#include <errno.h>
#include "fddefs.h"

/* Version 0 */
#define AFD_WORD_OFFSET_0          (SIZEOF_INT + 4 + SIZEOF_INT + 4)

struct msg_cache_buf_0
       {
          char         host_name[MAX_HOSTNAME_LENGTH + 1];
          time_t       msg_time;
          time_t       last_transfer_time;
          int          fsa_pos;
          int          port;
          unsigned int job_id;
          unsigned int age_limit;
          char         type;
          char         in_current_fsa;
       };

/* Version 1 */
#define AFD_WORD_OFFSET_1          (SIZEOF_INT + 4 + SIZEOF_INT + 4)

struct msg_cache_buf_1
       {
          char         host_name[MAX_HOSTNAME_LENGTH + 1];
          time_t       msg_time;
          time_t       last_transfer_time;
          int          fsa_pos;
          int          port;
          unsigned int job_id;
          unsigned int age_limit;
          char         ageing;     /* New. */
          char         type;
          char         in_current_fsa;
       };

/* Global external variables. */
extern int default_ageing;


/*############################ convert_mdb() ############################*/
char *
convert_mdb(int           old_mdb_fd,
            char          *old_msg_cache_buf_file,
            off_t         *old_mdb_size,
            int           old_no_msg_cached,
            unsigned char old_version,
            unsigned char new_version)
{
   char         *ptr;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   if ((old_version == 0) && (new_version == 1))
   {
      int                  i;
      size_t               new_size;
      struct msg_cache_buf_0 *old_mdb;
      struct msg_cache_buf_1 *new_mdb;

      /* Get the size of the old MDB file. */
#ifdef HAVE_STATX
      if (statx(old_mdb_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(old_mdb_fd, &stat_buf) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                    "Failed to statx() %s : %s",
#else
                    "Failed to fstat() %s : %s",
#endif
                    old_msg_cache_buf_file, strerror(errno));
         *old_mdb_size = -1;
         return(NULL);
      }
      else
      {
#ifdef HAVE_STATX
         if (stat_buf.stx_size > 0)
#else
         if (stat_buf.st_size > 0)
#endif
         {
#ifdef HAVE_MMAP
            if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                            stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                            stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                            MAP_SHARED, old_mdb_fd, 0)) == (caddr_t) -1)
#else
            if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                                stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                                stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                                MAP_SHARED, old_msg_cache_buf_file,
                                0)) == (caddr_t) -1)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to mmap() to %s : %s",
                          old_msg_cache_buf_file, strerror(errno));
               *old_mdb_size = -1;
               return(NULL);
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "FSA file %s is empty.", old_msg_cache_buf_file);
            *old_mdb_size = -1;
            return(NULL);
         }
      }

      ptr += AFD_WORD_OFFSET_0;
      old_mdb = (struct msg_cache_buf_0 *)ptr;

      new_size = ((old_no_msg_cached / MSG_CACHE_BUF_SIZE) + 1) *
                 MSG_CACHE_BUF_SIZE * sizeof(struct msg_cache_buf_1);
      if ((ptr = malloc(new_size)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "malloc() error [%d %d] : %s",
                    old_no_msg_cached, new_size, strerror(errno));
         ptr = (char *)old_mdb;
         ptr -= AFD_WORD_OFFSET_0;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
         if (munmap(ptr, stat_buf.stx_size) == -1)
# else
         if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
         if (munmap_emu(ptr) == -1)
#endif
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to munmap() %s : %s",
                       old_msg_cache_buf_file, strerror(errno));
         }
         *old_mdb_size = -1;
         return(NULL);
      }
      (void)memset(ptr, 0, new_size);
      new_mdb = (struct msg_cache_buf_1 *)ptr;

      /*
       * Copy all the old data into the new region.
       */
      for (i = 0; i < old_no_msg_cached; i++)
      {
         (void)strcpy(new_mdb[i].host_name, old_mdb[i].host_name);
         new_mdb[i].msg_time           = old_mdb[i].msg_time;
         new_mdb[i].last_transfer_time = old_mdb[i].last_transfer_time;
         new_mdb[i].fsa_pos            = old_mdb[i].fsa_pos;
         new_mdb[i].port               = old_mdb[i].port;
         new_mdb[i].job_id             = old_mdb[i].job_id;
         new_mdb[i].age_limit          = old_mdb[i].age_limit;
         new_mdb[i].ageing             = default_ageing;
         new_mdb[i].type               = old_mdb[i].type;
         new_mdb[i].in_current_fsa     = old_mdb[i].in_current_fsa;
      }

      ptr = (char *)old_mdb;
      ptr -= AFD_WORD_OFFSET_0;

      /*
       * Resize the old MDB to the size of new one and then copy
       * the new structure into it.
       */
      if ((ptr = mmap_resize(old_mdb_fd, ptr, new_size + AFD_WORD_OFFSET_1)) == (caddr_t) -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to mmap_resize() %s : %s",
                    old_msg_cache_buf_file, strerror(errno));
         free((void *)new_mdb);
         return(NULL);
      }
      ptr += AFD_WORD_OFFSET_1;
      (void)memcpy(ptr, new_mdb, new_size);
      free((void *)new_mdb);
      ptr -= AFD_WORD_OFFSET_1;
      *(int *)ptr = old_no_msg_cached;
      *(ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
      *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
      *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
      (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
      *old_mdb_size = new_size + AFD_WORD_OFFSET_1;

      system_log(INFO_SIGN, NULL, 0,
                 "Converted MDB from verion %d to %d.",
                 (int)old_version, (int)new_version);
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
# if SIZEOF_SIZE_T == 4
                 "filesize old: %ld (%d) new: %ld (%d)  Number of msg cached: %d",
# else
                 "filesize old: %ld (%d) new: %lld (%d)  Number of msg cached: %d",
# endif
#else
# if SIZEOF_SIZE_T == 4
                 "filesize old: %lld (%d) new: %ld (%d)  Number of msg cached: %d",
# else
                 "filesize old: %lld (%d) new: %lld (%d)  Number of msg cached: %d",
# endif
#endif
#ifdef HAVE_STATX
                 (pri_off_t)stat_buf.stx_size, (int)sizeof(struct msg_cache_buf_0),
#else
                 (pri_off_t)stat_buf.st_size, (int)sizeof(struct msg_cache_buf_0),
#endif
                 (pri_size_t)new_size + AFD_WORD_OFFSET_1,
                 (int)sizeof(struct msg_cache_buf_1), old_no_msg_cached);
   }
   else
   {
      system_log(ERROR_SIGN, NULL, 0,
                 "Don't know how to convert a version %d MDB to version %d.",
                 old_version, new_version);
      ptr = NULL;
   }

   return(ptr);
}
