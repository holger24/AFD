/*
 *  read_current_msg_list.c - Part of AFD, an automatic file distribution
 *                            program.
 *  Copyright (c) 2004 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   read_current_msg_list - reads the current message list to a buffer
 **
 ** SYNOPSIS
 **   int read_current_msg_list(unsigned int **cml, int *no_of_current_msg)
 **
 ** DESCRIPTION
 **   The function read_current_msg_list() reads the current job
 **   ID's and stores them in cml and the number of them in no_of_current_msg.
 **   It allocates memory for cml so the caller is responsible to
 **   free it.
 **
 ** RETURN VALUES
 **   When successful it returns SUCCESS and the array cml with
 **   the current job ID's and no_of_current_msg with the number
 **   of current job ID's. Otherwise INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   29.12.2004 H.Kiehl Created
 **   07.04.2005 H.Kiehl Make this function more robust when new FSA is
 **                      created.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>     /* strcpy(), strcat(), strerror()                */
#include <stdlib.h>     /* malloc(), free()                              */
#include <unistd.h>     /* sleep()                                       */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "fddefs.h"


/* External global variables. */
extern char              *p_work_dir;
extern struct afd_status *p_afd_status;


/*####################### read_current_msg_list() #######################*/
int
read_current_msg_list(unsigned int **cml, int *no_of_current_msg)
{
   int          fd,
                sleep_counter;
   size_t       size;
   ssize_t      bytes_read;
   char         current_msg_list_file[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   (void)strcpy(current_msg_list_file, p_work_dir);
   (void)strcat(current_msg_list_file, FIFO_DIR);
   (void)strcat(current_msg_list_file, CURRENT_MSG_LIST_FILE);
   if ((fd = open(current_msg_list_file, O_RDWR)) == -1) /* WR for locking. */
   {
      if (errno == ENOENT)
      {
         sleep_counter = 0;
         do
         {
            (void)my_usleep(100000L);
            errno = 0;
            sleep_counter++;
            if (((fd = open(current_msg_list_file, O_RDWR)) == -1) &&
                ((errno != ENOENT) || (sleep_counter > 100)))
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "Failed to open() `%s' : %s",
                          current_msg_list_file, strerror(errno));
               return(INCORRECT);
            }
         } while (errno == ENOENT);
      }
      else
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to open() `%s' : %s",
                    current_msg_list_file, strerror(errno));
         return(INCORRECT);
      }
   }

#ifdef HAVE_STATX
   if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(fd, &stat_buf) == -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                 "Failed to statx() `%s' : %s",
#else
                 "Failed to fstat() `%s' : %s",
#endif
                 current_msg_list_file, strerror(errno));
      exit(INCORRECT);
   }
   sleep_counter = 0;
#ifdef HAVE_STATX
   while (stat_buf.stx_size == 0)
#else
   while (stat_buf.st_size == 0)
#endif
   {
      (void)sleep(1);
#ifdef HAVE_STATX
      if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(fd, &stat_buf) == -1)
#endif
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                    "Failed to statx() `%s' : %s",
#else
                    "Failed to fstat() `%s' : %s",
#endif
                    current_msg_list_file, strerror(errno));
         exit(INCORRECT);
      }
      sleep_counter++;
      if (sleep_counter > 10)
      {
         break;
      }
   }

   /*
    * If we lock the file to early init_job_data() of the AMG does not
    * get the time to fill all data into the current_msg_list_file.
    */
   sleep_counter = 0;
   while (p_afd_status->amg_jobs & WRITTING_JID_STRUCT)
   {
      (void)my_usleep(100000L);
      sleep_counter++;
      if (sleep_counter > 100)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Timeout arrived for waiting for AMG to finish writting to JID structure.");
         exit(INCORRECT);
      }
   }
#ifdef LOCK_DEBUG
   lock_region_w(fd, 0, __FILE__, __LINE__);
#else
   lock_region_w(fd, 0);
#endif
   if ((bytes_read = read(fd, no_of_current_msg, sizeof(int))) != sizeof(int))
   {
      if (bytes_read == -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to read() from `%s' [%d] : %s",
                    current_msg_list_file, fd, strerror(errno));
      }
      else
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
#if SIZEOF_SSIZE_T == 4
                    "Failed to read() %d bytes from `%s' [%d]. Bytes read = %d.",
#else
                    "Failed to read() %d bytes from `%s' [%d]. Bytes read = %lld.",
#endif
                    sizeof(int), current_msg_list_file, fd,
                    (pri_ssize_t)bytes_read);
      }
      (void)close(fd);
      return(INCORRECT);
   }
   size = *no_of_current_msg * sizeof(int);
   if ((*cml = malloc(size)) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() %d bytes : %s", size, strerror(errno));
      (void)close(fd);
      return(INCORRECT);
   }
   if ((bytes_read = read(fd, *cml, size)) != size)
   {
      if (bytes_read == -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to read() from `%s' [%d] : %s",
                    current_msg_list_file, fd, strerror(errno));
      }
      else
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
#if SIZEOF_SSIZE_T == 4
                    "Failed to read() %d bytes from `%s' [%d]. Bytes read = %d.",
#else
                    "Failed to read() %d bytes from `%s' [%d]. Bytes read = %lld.",
#endif
                    size, current_msg_list_file, fd, (pri_ssize_t)bytes_read);
      }
      (void)close(fd);
      free(*cml);
      return(INCORRECT);
   }
   if (close(fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__, "Failed to close() `%s' : %s",
                 current_msg_list_file, strerror(errno));
   }

   return(SUCCESS);
}
