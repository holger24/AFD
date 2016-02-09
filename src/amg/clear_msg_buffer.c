/*
 *  clear_msg_buffer.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2008 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   clear_msg_buffer - writes all messages buffered to the FD_MSG_FIFO
 **
 ** SYNOPSIS
 **   void clear_msg_buffer(void)
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
 **   21.03.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>          /* strerror()                               */
#include <stdlib.h>          /* exit()                                   */
#include <unistd.h>          /* write()                                  */
#ifdef _WITH_PTHREAD
# include <pthread.h>
#endif
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int                mb_fd,
                          msg_fifo_fd,
                          *no_msg_buffered;
extern struct afd_status  *p_afd_status;
extern struct message_buf *mb;
#ifdef _WITH_PTHREAD
extern pthread_mutex_t    fsa_mutex;
#endif


/*######################### clear_msg_buffer() ##########################*/
void
clear_msg_buffer(void)
{
   register int i;
#ifdef _WITH_PTHREAD
   int          rtn;
#endif

#ifdef _WITH_PTHREAD
   if ((rtn = pthread_mutex_lock(&fsa_mutex)) != 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "pthread_mutex_lock() error : %s", strerror(rtn));
   }
#endif
   /* Empty message buffer first. */
#ifdef LOCK_DEBUG
   lock_region_w(mb_fd, 0, __FILE__, __LINE__);
#else
   lock_region_w(mb_fd, 0);
#endif
   for (i = 0; i < *no_msg_buffered; i++)
   {
      if (write(msg_fifo_fd, mb[i].bin_msg_name, MAX_BIN_MSG_LENGTH) != MAX_BIN_MSG_LENGTH)
      {
         /* Remove those messages from buffer that we */
         /* already have send.                        */
         if (i > 0)
         {
            (void)memmove(&mb[0], &mb[i],
                          (*no_msg_buffered - i) * sizeof(struct message_buf));
            *no_msg_buffered -= i;
         }

         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to write() to message FIFO : %s", strerror(errno));
         exit(INCORRECT);
      }
   }

   /* Reduce the buffer if necessary. */
   if (*no_msg_buffered > MESSAGE_BUF_STEP_SIZE)
   {
      char   *ptr;
      size_t new_size = (MESSAGE_BUF_STEP_SIZE *
                         sizeof(struct message_buf)) + AFD_WORD_OFFSET;

      ptr = (char *)mb - AFD_WORD_OFFSET;
      if ((ptr = mmap_resize(mb_fd, ptr, new_size)) == (caddr_t) -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "mmap() error : %s", strerror(errno));
         exit(INCORRECT);
      }
      no_msg_buffered = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      mb = (struct message_buf *)ptr;
   }

   *no_msg_buffered = 0;
#ifdef LOCK_DEBUG
   unlock_region(mb_fd, 0, __FILE__, __LINE__);
#else
   unlock_region(mb_fd, 0);
#endif
#ifdef _WITH_PTHREAD
   if ((rtn = pthread_mutex_unlock(&fsa_mutex)) != 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "pthread_mutex_unlock() error : %s", strerror(rtn));
   }
#endif

   return;
}
