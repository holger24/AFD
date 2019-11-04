/*
 *  jid_attach.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2019 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   void jid_attach(void)
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
void
jid_attach(int writeable)
{
   char        *ptr,
               jid_file[MAX_PATH_LENGTH];
   struct stat stat_buf;

   (void)strcpy(jid_file, p_work_dir);
   (void)strcat(jid_file, FIFO_DIR);
   (void)strcat(jid_file, JOB_ID_DATA_FILE);
   if ((jid_fd = coe_open(jid_file,
                          (writeable == YES) ? O_RDWR : O_RDONLY)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to open() `%s' : %s"),
                 jid_file, strerror(errno));
      exit(INCORRECT);
   }
   if (fstat(jid_fd, &stat_buf) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to fstat() `%s' : %s"),
                 jid_file, strerror(errno));
      (void)close(jid_fd);
      exit(INCORRECT);
   }
#ifdef HAVE_MMAP
   if ((ptr = mmap(NULL, stat_buf.st_size,
                   (writeable == YES) ? (PROT_READ | PROT_WRITE) : PROT_READ,
                   MAP_SHARED, jid_fd, 0)) == (caddr_t) -1)
#else
   if ((ptr = mmap_emu(NULL, stat_buf.st_size,
                       (writeable == YES) ? (PROT_READ | PROT_WRITE) : PROT_READ,
                       MAP_SHARED, afd_mon_status_file, 0)) == (caddr_t) -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("mmap() error : %s"), strerror(errno));
      (void)close(jid_fd);
      exit(INCORRECT);
   }
   if (close(jid_fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 _("Failed to close() %s : %s"), jid_file, strerror(errno));
   }
   if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_JID_VERSION)
   {
      (void)fprintf(stderr, _("Incorrect JID version (data=%d current=%d)!\n"),
                    *(ptr + SIZEOF_INT + 1 + 1 + 1), CURRENT_JID_VERSION);
      exit(INCORRECT);
   }
   no_of_job_ids = *(int *)ptr;
   ptr += AFD_WORD_OFFSET;
   jid = (struct job_id_data *)ptr;
   jid_size = stat_buf.st_size;

   return;
}
