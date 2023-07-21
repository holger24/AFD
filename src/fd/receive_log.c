/*
 *  receive_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   receive_log - writes formated log output to receive log
 **
 ** SYNOPSIS
 **   void receive_log(char         *sign,
 **                    char         *file,
 **                    int          line,
 **                    time_t       current_time,
 **                    unsigned int job_id,
 **                    char         *fmt, ...)
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
 **   29.08.2000 H.Kiehl Created
 **   20.12.2010 H.Kiehl Adapted for sf_xxx process, so they too can log
 **                      to receive log.
 **
 */
DESCR__E_M3

#include <stdio.h>                    /* snprintf()                      */
#include <string.h>                   /* strcpy(), strerror()            */
#include <stdarg.h>                   /* va_start(), va_end()            */
#include <time.h>                     /* time(), localtime()             */
#include <sys/time.h>                 /* struct tm                       */
#include <sys/types.h>
#include <sys/stat.h>                 /* fstat()                         */
#include <fcntl.h>                    /* open(), close()                 */
#include <unistd.h>                   /* write()                         */
#include <sys/mman.h>                 /* mmap(), munmap()                */
#include <errno.h>
#include "fddefs.h"

/* Global variables. */
int                        fra_fd,
                           fra_id,
                           no_of_dirs;
off_t                      fra_size;
struct fileretrieve_status *fra;

/* External global variables. */
extern char                *p_work_dir;

/* Local function prototypes. */
static void                get_dir_alias(unsigned, char *);


/*########################### receive_log() #############################*/
void
receive_log(char         *sign,
            char         *file,
            int          line,
            time_t       current_time,
            unsigned int job_id,
            char         *fmt, ...)
{
   int       receive_log_fd = -1,
             tmp_errno = errno;
#ifdef WITHOUT_FIFO_RW_SUPPORT
   int       receive_log_readfd;
#endif
   char      dir_alias[MAX_DIR_ALIAS_LENGTH + 1],
             *ptr;
   size_t    length = DIR_ALIAS_OFFSET;
   char      buf[MAX_LINE_LENGTH + MAX_LINE_LENGTH + 1];
   va_list   ap;
   struct tm *p_ts;

   get_dir_alias(job_id, dir_alias);
   if (dir_alias[0] == '\0')
   {
      errno = tmp_errno;
      return;
   }
   ptr = dir_alias;

   if (p_work_dir != NULL)
   {
      char receive_log_fifo[MAX_PATH_LENGTH];

      (void)strcpy(receive_log_fifo, p_work_dir);
      (void)strcat(receive_log_fifo, FIFO_DIR);
      (void)strcat(receive_log_fifo, RECEIVE_LOG_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(receive_log_fifo, &receive_log_readfd,
                       &receive_log_fd) == -1)
#else
      if ((receive_log_fd = open(receive_log_fifo, O_RDWR)) == -1)
#endif
      {
         if (errno == ENOENT)
         {
            if ((make_fifo(receive_log_fifo) == SUCCESS) &&
#ifdef WITHOUT_FIFO_RW_SUPPORT
                (open_fifo_rw(receive_log_fifo, &receive_log_readfd,
                              &receive_log_fd) == -1))
#else
                ((receive_log_fd = open(receive_log_fifo, O_RDWR)) == -1))
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not open fifo <%s> : %s",
                          RECEIVE_LOG_FIFO, strerror(errno));
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not open fifo %s : %s",
                       RECEIVE_LOG_FIFO, strerror(errno));
         }
      }
   }
   if (receive_log_fd == -1)
   {
      errno = tmp_errno;
      return;
   }

   if (current_time == 0L)
   {
      current_time = time(NULL);
   }
   p_ts = localtime(&current_time);
   if (p_ts == NULL)
   {
      buf[0]  = '?';
      buf[1]  = '?';
      buf[3]  = '?';
      buf[4]  = '?';
      buf[6]  = '?';
      buf[7]  = '?';
      buf[9]  = '?';
      buf[10] = '?';
   }
   else
   {
      buf[0]  = (p_ts->tm_mday / 10) + '0';
      buf[1]  = (p_ts->tm_mday % 10) + '0';
      buf[3]  = (p_ts->tm_hour / 10) + '0';
      buf[4]  = (p_ts->tm_hour % 10) + '0';
      buf[6]  = (p_ts->tm_min / 10) + '0';
      buf[7]  = (p_ts->tm_min % 10) + '0';
      buf[9]  = (p_ts->tm_sec / 10) + '0';
      buf[10] = (p_ts->tm_sec % 10) + '0';
   }
   buf[2]  = ' ';
   buf[5]  = ':';
   buf[8]  = ':';
   buf[11] = ' ';
   buf[12] = sign[0];
   buf[13] = sign[1];
   buf[14] = sign[2];
   buf[15] = ' ';

   while (*ptr != '\0')
   {
      buf[length] = *ptr;
      ptr++; length++;
   }
   while ((length - DIR_ALIAS_OFFSET) < MAX_DIR_ALIAS_LENGTH)
   {
      buf[length] = ' ';
      length++;
   }
   buf[length]     = ':';
   buf[length + 1] = ' ';
   length += 2;

   va_start(ap, fmt);
   length += vsnprintf(&buf[length],
                       (MAX_LINE_LENGTH + MAX_LINE_LENGTH) - length, fmt, ap);
   va_end(ap);

   if (length > (MAX_LINE_LENGTH + MAX_LINE_LENGTH))
   {
      length = MAX_LINE_LENGTH + MAX_LINE_LENGTH;
   }

   if ((file == NULL) || (line == 0) ||
       (length >= (MAX_LINE_LENGTH + MAX_LINE_LENGTH)))
   {
      buf[length] = '\n';
      length += 1;
   }
   else
   {
      length += snprintf(&buf[length],
                         (MAX_LINE_LENGTH + MAX_LINE_LENGTH) - length,
                         " (%s %d)\n", file, line);
      if (length > (MAX_LINE_LENGTH + MAX_LINE_LENGTH))
      {
         length = MAX_LINE_LENGTH + MAX_LINE_LENGTH;
         buf[length] = '\n';
         length += 1;
      }
   }

   if (write(receive_log_fd, buf, length) != length)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "write() error : %s", strerror(errno));
   }

   if (close(receive_log_fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }
   errno = tmp_errno;

   return;
}


/*+++++++++++++++++++++++++++ get_dir_alias() +++++++++++++++++++++++++++*/
static void
get_dir_alias(unsigned int job_id, char *dir_alias)
{
   int          fd;
   unsigned int dir_id = 0;
   char         fullname[MAX_PATH_LENGTH];

   dir_alias[0] = '\0';
   (void)snprintf(fullname, MAX_PATH_LENGTH, "%s%s%s",
                  p_work_dir, FIFO_DIR, JOB_ID_DATA_FILE);
   if ((fd = open(fullname, O_RDONLY)) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to open() `%s' : %s", fullname, strerror(errno));
   }
   else
   {
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat  stat_buf;
#endif

#ifdef HAVE_STATX
      if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(fd, &stat_buf) == -1)
#endif
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                    "Failed to statx() `%s' : %s",
#else
                    "Failed to fstat() `%s' : %s",
#endif
                    fullname, strerror(errno));
      }
      else
      {
#ifdef HAVE_STATX
         if (stat_buf.stx_size > 0)
#else
         if (stat_buf.st_size > 0)
#endif
         {
            char *ptr;

            if ((ptr = mmap(NULL,
#ifdef HAVE_STATX
                            stat_buf.stx_size, PROT_READ,
#else
                            stat_buf.st_size, PROT_READ,
#endif
                            MAP_SHARED, fd, 0)) == (caddr_t) -1)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to mmap() to `%s' : %s",
                          fullname, strerror(errno));
            }
            else
            {
               if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_JID_VERSION)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Incorrect JID version (data=%d current=%d)!",
                             *(ptr + SIZEOF_INT + 1 + 1 + 1),
                             CURRENT_JID_VERSION);
               }
               else
               {
                  int                i,
                                     *no_of_job_ids;
                  struct job_id_data *jd;

                  no_of_job_ids = (int *)ptr;
                  ptr += AFD_WORD_OFFSET;
                  jd = (struct job_id_data *)ptr;

                  for (i = 0; i < *no_of_job_ids; i++)
                  {
                     if (job_id == jd[i].job_id)
                     {
                        dir_id = jd[i].dir_id;
                        break;
                     }
                  }
                  ptr -= AFD_WORD_OFFSET;
               }

               /* Don't forget to unmap from job_id_data structure. */
#ifdef HAVE_STATX
               if (munmap(ptr, stat_buf.stx_size) == -1)
#else
               if (munmap(ptr, stat_buf.st_size) == -1)
#endif
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "munmap() error : %s", strerror(errno));
               }
            }
         }
         else
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "File `%s' is empty! Terminating, don't know what to do :-(",
                       fullname);
         }
      }
      if (close(fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Failed to close() `%s' : %s", fullname, strerror(errno));
      }
   }

   if (dir_id != 0)
   {
      int attached = NO,
          i;

      if (fra_fd == -1)
      {
         if (fra_attach_passive() != SUCCESS)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to attach to FRA.");
            return;
         }
         attached = YES;
      }
      else
      {
         (void)check_fra(YES);
      }

      for (i = 0; i < no_of_dirs; i++)
      {
         if (dir_id == fra[i].dir_id)
         {
            (void)strcpy(dir_alias, fra[i].dir_alias);
            break;
         }
      }

      if (attached == YES)
      {
         fra_detach();
      }
   }

   return;
}
