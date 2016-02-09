/*
 *  check_queue_space.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_queue_space - checks if queue is still large enough
 **
 ** SYNOPSIS
 **   void check_queue_space(void)
 **
 ** DESCRIPTION
 **   The function check_queue_space() checks if the queue for queuing
 **   in the FD is still large enough or if its time again to reduce
 **   its size. It will resize the queue accordingly.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.08.2000 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "fddefs.h"

/* External global variables. */
extern int              *no_msg_queued,
                        qb_fd;
extern struct queue_buf *qb;


/*######################### check_queue_space() #########################*/
void
check_queue_space(void)
{
   if ((*no_msg_queued != 0) &&
       ((*no_msg_queued % MSG_QUE_BUF_SIZE) == 0))
   {
      char   *ptr;
      size_t new_size;

      new_size = (((*no_msg_queued / MSG_QUE_BUF_SIZE) + 1) *
                 MSG_QUE_BUF_SIZE * sizeof(struct queue_buf)) +
                 AFD_WORD_OFFSET;
      ptr = (char *)qb - AFD_WORD_OFFSET;
      if ((ptr = mmap_resize(qb_fd, ptr, new_size)) == (caddr_t) -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "mmap() error : %s", strerror(errno));
         exit(INCORRECT);
      }
      no_msg_queued = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      qb = (struct queue_buf *)ptr;
   }
   return;
}
