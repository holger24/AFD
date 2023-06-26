/*
 *  jid_attach.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2019 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   jid_attach - attaches to job ID data (JID)
 **
 ** SYNOPSIS
 **   int jid_attach(int writeable, char *who)
 **
 ** DESCRIPTION
 **   The function jid_attach() attaches to the job ID data (JID).
 **
 ** RETURN VALUES
 **   None. It exits when it fails, otherwise it attaches to the
 **   job ID data file.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.11.2019 H.Kiehl Created
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
#include "mondefs.h"

/* External global variables. */
extern int                jid_fd,
                          no_of_job_ids;
#ifdef HAVE_MMAP
extern off_t              jid_size;
#endif
extern char               *p_work_dir;
extern struct job_id_data *jid;


/*############################# jid_attach() ############################*/
int
jid_attach(int writeable, char *who)
{
   char         *ptr,
                jid_file[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   (void)strcpy(jid_file, p_work_dir);
   (void)strcat(jid_file, FIFO_DIR);
   (void)strcat(jid_file, JOB_ID_DATA_FILE);
   if ((jid_fd = coe_open(jid_file,
                          (writeable == YES) ? O_RDWR : O_RDONLY)) == -1)
   {
      if (errno == ENOENT)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("Failed to open() `%s' : %s"),
                    jid_file, strerror(errno));
      }
      else
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to open() `%s' : %s"),
                    jid_file, strerror(errno));
      }
      return(INCORRECT);
   }
#ifdef HAVE_STATX
   if (statx(jid_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(jid_fd, &stat_buf) == -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                 _("Failed to statx() `%s' : %s"),
#else
                 _("Failed to fstat() `%s' : %s"),
#endif
                 jid_file, strerror(errno));
      (void)close(jid_fd);
      jid_fd = -1;
      return(INCORRECT);
   }
#ifdef HAVE_MMAP
   if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                   stat_buf.stx_size,
# else
                   stat_buf.st_size,
# endif
                   (writeable == YES) ? (PROT_READ | PROT_WRITE) : PROT_READ,
                   MAP_SHARED, jid_fd, 0)) == (caddr_t) -1)
#else
   if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                       stat_buf.stx_size,
# else
                       stat_buf.st_size,
# endif
                       (writeable == YES) ? (PROT_READ | PROT_WRITE) : PROT_READ,
                       MAP_SHARED, jid_file, 0)) == (caddr_t) -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("mmap() error : %s"), strerror(errno));
      (void)close(jid_fd);
      jid_fd = -1;
      return(INCORRECT);
   }
   if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_JID_VERSION)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "This code is compiled for JID version %d, but the JID we try to attach is %d [%s].",
                 CURRENT_JID_VERSION, *(ptr + SIZEOF_INT + 1 + 1 + 1), who);
      (void)close(jid_fd);
      jid_fd = -1;
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
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to munmap() JID [%s] : %s"),
                    who, strerror(errno));
      }
      return(INCORRECT_VERSION);
   }
   no_of_job_ids = *(int *)ptr;
   ptr += AFD_WORD_OFFSET;
   jid = (struct job_id_data *)ptr;
#ifdef HAVE_STATX
   jid_size = stat_buf.stx_size;
#else
   jid_size = stat_buf.st_size;
#endif

   return(SUCCESS);
}
