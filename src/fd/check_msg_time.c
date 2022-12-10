/*
 *  check_msg_time.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2022 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_msg_time - searches message directory for any changes
 **
 ** SYNOPSIS
 **   void check_msg_time(void)
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
 **   29.05.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#ifdef HAVE_STATX
# include <fcntl.h>           /* Definition of AT_* constants */
#endif
#include <sys/stat.h>
#include <unistd.h>
#include "fddefs.h"

/* External global variables. */
extern int                  *no_msg_cached;
extern char                 *p_msg_dir,
                            msg_dir[];
extern struct msg_cache_buf *mdb;


/*########################### check_msg_time() ##########################*/
void
check_msg_time(void)
{
   register int i;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   for (i = 0; i < *no_msg_cached; i++)
   {
      (void)snprintf(p_msg_dir, MAX_PATH_LENGTH - (p_msg_dir - msg_dir),
                     "%x", mdb[i].job_id);
#ifdef HAVE_STATX
      if (statx(0, msg_dir, AT_STATX_SYNC_AS_STAT,
                STATX_SIZE | STATX_MTIME, &stat_buf) != -1)
#else
      if (stat(msg_dir, &stat_buf) != -1)
#endif
      {
#ifdef HAVE_STATX
         if (stat_buf.stx_mtime.tv_sec > mdb[i].msg_time)
#else
         if (stat_buf.st_mtime > mdb[i].msg_time)
#endif
         {
#ifdef HAVE_STATX
            (void)get_job_data(mdb[i].job_id, i,
                               stat_buf.stx_mtime.tv_sec, stat_buf.stx_size);
#else
            (void)get_job_data(mdb[i].job_id, i,
                               stat_buf.st_mtime, stat_buf.st_size);
#endif
         }
      }
   }
   *p_msg_dir = '\0';

   return;
}
