/*
 *  create_name.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2014 Deutscher Wetterdienst (DWD),
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
 **   create_name - creates a unique name for the AFD
 **
 ** SYNOPSIS
 **   int create_name(char           *p_path,   - the path where the job
 **                                               can be found by the FD.
 **                   signed char    priority,  - the priority of the job
 **                   time_t         time_val,  - Date value in seconds
 **                   unsigned int   id,        - job or dir ID
 **                   unsigned int   *split_job_counter,
 **                   int            *unique_number,
 **                   char           *msg_name, - Storage for the message
 **                                               name
 **                   int            max_msg_name_length,
 **                   int            counter_fd)
 **
 ** DESCRIPTION
 **   Generates name for message and directory for each job.
 **   The syntax will be as follows if id is the dir ID:
 **      nnnnnnnnnn_llll_jjj
 **          |       |    |
 **          |       |    +--> Directory Identifier
 **          |       +-------> Counter that wraps around. This
 **          |                 ensures that no message can have
 **          |                 the same name in one second.
 **          +---------------> creation time in seconds
 **
 **   And if id is the job ID
 **
 **      jjj/d/x_nnnnnnnnnn_llll_ssss
 **       |  | |     |       |    |
 **       |  | |     |       |    +-------> Split Job Counter.
 **       |  | |     |       +------------> Counter that wraps around. This
 **       |  | |     |                      ensures that no message can have
 **       |  | |     |                      the same name in one second.
 **       |  | |     +--------------------> Creation time in seconds.
 **       |  | +--------------------------> Priority of the job.
 **       |  +----------------------------> Directory number.
 **       +-------------------------------> Job Identifier.
 **
 **   When priority is set to NO_PRIORITY, x_ is omitted in the
 **   directory name and a directory is created in the AFD_TMP_DIR
 **   (pool).
 **
 ** RETURN VALUES
 **   Returns SUCCESS when it has created a new directory. Otherwise
 **   INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   04.10.1995 H.Kiehl Created
 **   11.05.1997 H.Kiehl The caller of the function is now responsible
 **                      for the storage area of the message name.
 **   19.01.1998 H.Kiehl Added job ID to name.
 **   24.09.2004 H.Kiehl Added split job counter.
 **
 */
DESCR__E_M3

#include <stdio.h>                    /* fprintf()                       */
#include <string.h>                   /* strcpy(), strcat(), strerror()  */
#include <unistd.h>                   /* read(), write(), close()        */
#include <sys/types.h>
#include <sys/stat.h>                 /* mkdir()                         */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>


/*############################ create_name() ############################*/
int
create_name(char           *p_path,     /* Path where the new directory  */
                                        /* is to be created.             */
            signed char    priority,    /* Priority of the job.          */
            time_t         time_val,    /* Date value in seconds.        */
            unsigned int   id,          /* Job or dir ID.                */
            unsigned int   *split_job_counter,
            int            *unique_number,
            char           *msg_name,   /* Storage to return msg name.   */
            int            max_msg_name_length,
            int            counter_fd)
{
   long dirs_left = 10000L;
   char *ptr,
        tmpname[MAX_PATH_LENGTH];

   if (counter_fd != -1)
   {
      if (next_counter(counter_fd, unique_number, MAX_MSG_PER_SEC) < 0)
      {
         return(INCORRECT);
      }
   }

   /* Now try to create directory. */
   (void)strcpy(tmpname, p_path);
   ptr = tmpname + strlen(tmpname);
   if (*(ptr - 1) != '/')
   {
      *(ptr++) = '/';
   }
   for (;;)
   {
      if (priority == NO_PRIORITY)
      {
         (void)snprintf(msg_name, max_msg_name_length,
#if SIZEOF_TIME_T == 4
                        "%lx_%x_%x_%x",
#else
                        "%llx_%x_%x_%x",
#endif
                        (pri_time_t)time_val, *unique_number,
                        *split_job_counter, id); /* NOTE: dir ID is inserted here! */
      }
      else
      {
         int dir_no;

         if ((dir_no = get_dir_number(p_path, id, &dirs_left)) == INCORRECT)
         {
            *msg_name = '\0';
            return(INCORRECT);
         }
         (void)snprintf(msg_name, max_msg_name_length,
#if SIZEOF_TIME_T == 4
                        "%x/%x/%lx_%x_%x",
#else
                        "%x/%x/%llx_%x_%x",
#endif
                        id, dir_no, (pri_time_t)time_val, *unique_number,
                        *split_job_counter);
      }
      (void)my_strncpy(ptr, msg_name, MAX_PATH_LENGTH - (ptr - tmpname));
      if (mkdir(tmpname, DIR_MODE) == -1)
      {
         if ((errno == EMLINK) || (errno == ENOSPC))
         {
            *msg_name = '\0';
            return(INCORRECT);
         }
         (*split_job_counter)++;
         if (*split_job_counter == (unsigned int)dirs_left)
         {
            /*
             * We have tried enough values and none where
             * successful. So we don't end up in an endless loop
             * lets return here.
             */
            *msg_name = '\0';
            return(INCORRECT);
         }
      }
      else
      {
         break;
      }
   }

   return(SUCCESS);
}
