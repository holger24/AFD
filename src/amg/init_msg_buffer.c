/*
 *  init_msg_buffer.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   init_msg_buffer - initializes the message buffer cache
 **
 ** SYNOPSIS
 **   void init_msg_buffer(void)
 **
 ** DESCRIPTION
 **   This function mmaps to MESSAGE_BUF_FILE, that caches all messages
 **   when FD is not running.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.01.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>          /* strcpy(), strcat(), strerror()           */
#include <stdlib.h>          /* exit()                                   */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int                mb_fd,
                          msg_fifo_fd,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                          msg_fifo_readfd,
#endif
                          *no_msg_buffered;
extern char               *p_work_dir;
extern struct message_buf *mb;


/*########################## init_msg_buffer() ##########################*/
void
init_msg_buffer(void)
{
   char         *ptr,
                message_buf_file[MAX_PATH_LENGTH],
                msg_fifo[MAX_PATH_LENGTH];
   size_t       new_size;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   (void)strcpy(message_buf_file, p_work_dir);
   (void)strcat(message_buf_file, FIFO_DIR);
   (void)strcpy(msg_fifo, message_buf_file);
   (void)strcat(msg_fifo, MSG_FIFO);
   (void)strcat(message_buf_file, MESSAGE_BUF_FILE);

   /* Attach to message buffer. */
   new_size = (MESSAGE_BUF_STEP_SIZE * sizeof(struct message_buf)) +
              AFD_WORD_OFFSET;
   if ((ptr = attach_buf(message_buf_file, &mb_fd, &new_size,
#ifdef GROUP_CAN_WRITE
                         NULL, (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP),
                         NO)) == (caddr_t) -1)
#else
                         NULL, (S_IRUSR | S_IWUSR), NO)) == (caddr_t) -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to mmap() to %s : %s",
                 message_buf_file, strerror(errno));
      exit(INCORRECT);
   }
   no_msg_buffered = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   mb = (struct message_buf *)ptr;

#ifdef HAVE_STATX
   if ((statx(0, msg_fifo, AT_STATX_SYNC_AS_STAT,
              STATX_MODE, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.stx_mode)))
#else
   if ((stat(msg_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
#endif
   {
      if (make_fifo(msg_fifo) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not create fifo %s.", msg_fifo);
         exit(INCORRECT);
      }
   }
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(msg_fifo, &msg_fifo_readfd, &msg_fifo_fd) == -1)
#else
   if ((msg_fifo_fd = open(msg_fifo, O_RDWR)) == -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s", msg_fifo, strerror(errno));
      exit(INCORRECT);
   }

   return;
}
