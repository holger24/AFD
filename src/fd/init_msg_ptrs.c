/*
 *  init_msg_ptrs.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2015 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   init_msg_ptrs - initialises and sets data pointers for message
 **                   pointers
 **
 ** SYNOPSIS
 **   void init_msg_ptrs(time_t         **creation_time,
 **                      unsigned int   **job_id,
 **                      unsigned int   **split_job_counter,
 **                      unsigned int   **files_to_send,
 **                      off_t          **file_size_to_send,
 **                      dev_t          **dev,
 **                      unsigned short **dir_no,
 **                      unsigned int   **unique_number,
 **                      char           **msg_priority,
 **                      char           **originator,
 **                      char           **msg_buffer)
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
 **   18.04.1998 H.Kiehl Created
 **   30.10.2002 H.Kiehl Added originator.
 **   24.09.2004 H.Kiehl Added split job counter.
 **
 */
DESCR__E_M3

#include <string.h>                  /* strerror()                       */
#include <stdlib.h>                  /* malloc()                         */
#include <errno.h>
#include "fddefs.h"


/*########################### init_msg_ptrs() ###########################*/
void
init_msg_ptrs(time_t         **creation_time,
              unsigned int   **job_id,
              unsigned int   **split_job_counter,
              unsigned int   **files_to_send,
              off_t          **file_size_to_send,
#ifdef MULTI_FS_SUPPORT
              dev_t          **dev,
#endif
              unsigned short **dir_no,
              unsigned int   **unique_number,
              char           **msg_priority,
              char           **originator,
              char           **msg_buffer)
{
   if ((*msg_buffer = malloc(MAX_BIN_MSG_LENGTH)) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error [%d bytes] : %s",
                 MAX_BIN_MSG_LENGTH, strerror(errno));
      exit(INCORRECT);
   }
   *creation_time     = (time_t *)*msg_buffer;
#ifdef MULTI_FS_SUPPORT
# if SIZEOF_TIME_T == 4
   *dev               = (dev_t *)(*msg_buffer + sizeof(time_t));
   *job_id            = (unsigned int *)(*msg_buffer + sizeof(time_t) +
                                         sizeof(dev_t));
   *split_job_counter = (unsigned int *)(*msg_buffer + sizeof(time_t) +
                                         sizeof(dev_t) + sizeof(unsigned int));
   *files_to_send     = (unsigned int *)(*msg_buffer + sizeof(time_t) +
                                         sizeof(dev_t) + sizeof(unsigned int) +
                                         sizeof(unsigned int));
   *file_size_to_send = (off_t *)(*msg_buffer + sizeof(time_t) + sizeof(dev_t) +
                                  sizeof(unsigned int) + sizeof(unsigned int) +
                                  sizeof(unsigned int));
# else
   *file_size_to_send = (off_t *)(*msg_buffer + sizeof(time_t));
   *dev               = (dev_t *)(*msg_buffer + sizeof(time_t) + sizeof(off_t));
   *job_id            = (unsigned int *)(*msg_buffer + sizeof(time_t) +
                                         sizeof(off_t) + sizeof(dev_t));
   *split_job_counter = (unsigned int *)(*msg_buffer + sizeof(time_t) +
                                         sizeof(off_t) + sizeof(dev_t) +
                                         sizeof(unsigned int));
   *files_to_send     = (unsigned int *)(*msg_buffer + sizeof(time_t) +
                                         sizeof(off_t) + sizeof(dev_t) +
                                         sizeof(unsigned int) +
                                         sizeof(unsigned int));
# endif
   *unique_number     = (unsigned int *)(*msg_buffer + sizeof(time_t) +
                                         sizeof(dev_t) + sizeof(unsigned int) +
                                         sizeof(unsigned int) +
                                         sizeof(unsigned int) +
                                         sizeof(off_t));
   *dir_no            = (unsigned short *)(*msg_buffer + sizeof(time_t) +
                                           sizeof(dev_t) +
                                           sizeof(unsigned int) +
                                           sizeof(unsigned int) +
                                           sizeof(unsigned int) +
                                           sizeof(off_t) +
                                           sizeof(unsigned int));
   *msg_priority      = (char *)(*msg_buffer + sizeof(time_t) + sizeof(dev_t) +
                                 sizeof(unsigned int) + sizeof(unsigned int) +
                                 sizeof(unsigned int) + sizeof(off_t) +
                                 sizeof(unsigned int) +
                                 sizeof(unsigned short));
   *originator        = (char *)(*msg_buffer + sizeof(time_t) + sizeof(dev_t) +
                                 sizeof(unsigned int) + sizeof(unsigned int) +
                                 sizeof(unsigned int) + sizeof(off_t) +
                                 sizeof(unsigned int) +
                                 sizeof(unsigned short) + sizeof(char));
#else
# if SIZEOF_TIME_T == 4
   *job_id            = (unsigned int *)(*msg_buffer + sizeof(time_t));
   *split_job_counter = (unsigned int *)(*msg_buffer + sizeof(time_t) +
                                         sizeof(unsigned int));
   *files_to_send     = (unsigned int *)(*msg_buffer + sizeof(time_t) +
                                         sizeof(unsigned int) +
                                         sizeof(unsigned int));
   *file_size_to_send = (off_t *)(*msg_buffer + sizeof(time_t) +
                                  sizeof(unsigned int) + sizeof(unsigned int) +
                                  sizeof(unsigned int));
# else
   *file_size_to_send = (off_t *)(*msg_buffer + sizeof(time_t));
   *job_id            = (unsigned int *)(*msg_buffer + sizeof(time_t) +
                                         sizeof(off_t));
   *split_job_counter = (unsigned int *)(*msg_buffer + sizeof(time_t) +
                                         sizeof(off_t) + sizeof(unsigned int));
   *files_to_send     = (unsigned int *)(*msg_buffer + sizeof(time_t) +
                                         sizeof(off_t) + sizeof(unsigned int) +
                                         sizeof(unsigned int));
# endif
   *unique_number     = (unsigned int *)(*msg_buffer + sizeof(time_t) +
                                         sizeof(unsigned int) +
                                         sizeof(unsigned int) +
                                         sizeof(unsigned int) +
                                         sizeof(off_t));
   *dir_no            = (unsigned short *)(*msg_buffer + sizeof(time_t) +
                                           sizeof(unsigned int) +
                                           sizeof(unsigned int) +
                                           sizeof(unsigned int) +
                                           sizeof(off_t) +
                                           sizeof(unsigned int));
   *msg_priority      = (char *)(*msg_buffer + sizeof(time_t) +
                                 sizeof(unsigned int) + sizeof(unsigned int) +
                                 sizeof(unsigned int) + sizeof(off_t) +
                                 sizeof(unsigned int) +
                                 sizeof(unsigned short));
   *originator        = (char *)(*msg_buffer + sizeof(time_t) +
                                 sizeof(unsigned int) + sizeof(unsigned int) +
                                 sizeof(unsigned int) + sizeof(off_t) +
                                 sizeof(unsigned int) +
                                 sizeof(unsigned short) + sizeof(char));
#endif

   return;
}
