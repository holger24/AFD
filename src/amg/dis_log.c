/*
 *  dis_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   dis_log - writes disribution log data to process distribution_log via fifo
 **
 ** SYNOPSIS
 **   void init_dis_log(void)
 **   void dis_log(unsigned char dis_type,
 **                time_t        input_time,
 **                unsigned int  dir_id,
 **                unsigned int  unique_number,
 **                char          *filename,
 **                int           filename_length,
 **                off_t         file_size,
 **                int           no_of_queued_jobs,
 **                unsigned int  **job_id,
 **                unsigned char *proc_cycles,
 **                unsigned int  no_of_distribution_types)
 **   void release_dis_log(void)
 **
 ** DESCRIPTION
 **   The function init_dis_log() opens the distribution log fifo
 **   and allocates the required memory.
 **
 **   dis_log() writes the distribution data a fifo in the following
 **   format:
 **
 **     Distributed file name (char array).  <-----------------------------------+
 **     Array containing number of times job <--------------------+              |
 **     is preprocessed (unsigned char).                          |              |
 **     Segment number (unsigned char).      <----------------+   |              |
 **     Number of segments (unsigned char).  <------------+   |   |              |
 **     Distribution type (unsigned char).   <--------+   |   |   |              |
 **                                                   |   |   |   |              |
 **   <IT><FS><DID><UN><FNL><ND><NJ><JID 0>...<JID n><DT><NS><SN><NP 0>...<NP n><FN>
 **     |   |   |    |   |    |   |   |
 **     |   |   |    |   |    |   |   +--> Array containing Job ID's that
 **     |   |   |    |   |    |   |        received the given file (unsigned
 **     |   |   |    |   |    |   |        int array).
 **     |   |   |    |   |    |   +------> Number of job ID's in array (int).
 **     |   |   |    |   |    +----------> Number of distribution types.
 **     |   |   |    |   +---------------> File name length (int).
 **     |   |   |    +-------------------> Unique number (unsigned int).
 **     |   |   +------------------------> Directory ID (unsigned int).
 **     |   +----------------------------> File size (off_t).
 **     +--------------------------------> Input time (time_t)
 **
 **   Function release_dis_log() closes the distribution log fifo and
 **   releases all allocated memory.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   29.04.2008 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>           /* strerror()                              */
#include <stdlib.h>           /* realloc()                               */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern char         *p_work_dir;

/* Local global variables. */
static int          dis_log_fd = -1,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                    dis_log_readfd,
#endif
                    fix_length;
static long         dis_log_buffer_size;
static unsigned int *p_did,
                    *p_filename_length,
                    *p_jobs_queued,
                    *p_no_of_distribution_types,
                    *p_unique_number;
static time_t       *p_input_time;
static off_t        *p_file_size;
static char         *dis_log_buffer = NULL,
                    *p_jid_list;


/*########################## init_dis_log() #############################*/
void
init_dis_log(void)
{
   if (dis_log_fd == -1)
   {
      char dis_log_fifo[MAX_PATH_LENGTH];

      (void)strcpy(dis_log_fifo, p_work_dir);
      (void)strcat(dis_log_fifo, FIFO_DIR);
      (void)strcat(dis_log_fifo, DISTRIBUTION_LOG_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(dis_log_fifo, &dis_log_readfd, &dis_log_fd) == -1)
#else
      if ((dis_log_fd = open(dis_log_fifo, O_RDWR)) == -1)
#endif
      {
         if (errno == ENOENT)
         {
            if ((make_fifo(dis_log_fifo) == SUCCESS) &&
#ifdef WITHOUT_FIFO_RW_SUPPORT
                (open_fifo_rw(dis_log_fifo, &dis_log_readfd, &dis_log_fd) == -1))
#else
                ((dis_log_fd = open(dis_log_fifo, O_RDWR)) == -1))
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not open fifo `%s' : %s",
                          dis_log_fifo, strerror(errno));
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not open fifo `%s' : %s",
                       dis_log_fifo, strerror(errno));
         }
      }
   }
   if (dis_log_buffer == NULL)
   {
      int offset;

      if ((dis_log_buffer_size = fpathconf(dis_log_fd, _PC_PIPE_BUF)) < 0)
      {
         dis_log_buffer_size = DEFAULT_FIFO_SIZE;
      }
      offset = sizeof(off_t);
      if (sizeof(time_t) > offset)
      {
         offset = sizeof(time_t);
      }
      if (dis_log_buffer_size < (offset + offset + sizeof(int) + sizeof(int) +
                                 sizeof(unsigned int) + sizeof(int) +
                                 sizeof(unsigned int) + sizeof(char) +
                                 sizeof(char) + sizeof(char) +
                                 sizeof(char) + MAX_FILENAME_LENGTH))
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Fifo is NOT large enough to ensure atomic writes!");
         dis_log_buffer_size = offset +               /* Input time. */
                               offset +               /* File size. */
                               sizeof(int) +          /* Directory ID. */
                               sizeof(int) +          /* Unique number. */
                               sizeof(int) +          /* Filename length. */
                               sizeof(unsigned int) + /* No. of distr. types. */
                               sizeof(int) +          /* Jobs queued. */
                               sizeof(unsigned int) + /* JID list. */
                               sizeof(char) +         /* Distribution type. */
                               sizeof(char) +         /* Number of segments. */
                               sizeof(char) +         /* Segment number. */
                               sizeof(char) +         /* Processing list. */
                               MAX_FILENAME_LENGTH;   /* Filename. */
      }
      if ((dis_log_buffer = malloc((size_t)dis_log_buffer_size)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not allocate memory for the fifo buffer : %s",
                    strerror(errno));
         exit(INCORRECT);
      }
      p_input_time = (time_t *)(dis_log_buffer);
      p_file_size = (off_t *)(dis_log_buffer + offset);
      p_did = (unsigned int *)(dis_log_buffer + offset + offset);
      p_unique_number = (unsigned int *)(dis_log_buffer + offset + offset +
                                         sizeof(int));
      p_filename_length = (unsigned int *)(dis_log_buffer + offset + offset +
                                           sizeof(int) + sizeof(int));
      p_no_of_distribution_types = (unsigned int *)(dis_log_buffer + offset +
                                                    offset + sizeof(int) +
                                                    sizeof(int) + sizeof(int));
      p_jobs_queued = (unsigned int *)(dis_log_buffer + offset + offset +
                                       sizeof(int) + sizeof(int) + sizeof(int) +
                                       sizeof(int));
      fix_length = offset + offset + sizeof(int) + sizeof(int) + sizeof(int) +
                   sizeof(int) + sizeof(int);
      p_jid_list = (char *)(dis_log_buffer + fix_length);
   }

   return;
}


/*############################# dis_log() ###############################*/
void
dis_log(unsigned char dis_type,
        time_t        input_time,
        unsigned int  dir_id,
        unsigned int  unique_number,
        char          *filename,
        int           filename_length,
        off_t         file_size,
        int           no_of_queued_jobs,
        unsigned int  **job_id,
        unsigned char *proc_cycles,
        unsigned int  no_of_distribution_types)
{
   int full_length,
       offset;

   *p_input_time = input_time;
   *p_did = dir_id;
   *p_unique_number = unique_number;
   *p_filename_length = filename_length;
   *p_file_size = file_size;
   *p_no_of_distribution_types = no_of_distribution_types;

   full_length = fix_length + (no_of_queued_jobs * sizeof(unsigned int)) +
                 sizeof(char) + sizeof(char) + sizeof(char) +
                 (no_of_queued_jobs * sizeof(char)) + filename_length +
                 sizeof(char);

   /* Check if we need to split up the message in several segments. */
   /* We do this check prevent interleaving of writes.              */
   if (full_length > dis_log_buffer_size)
   {
      int i,
          loops,
          max_entries,
          rest,
          total_loops;

      /* Lets determine how many queued job entries fit into one message. */
      max_entries = (dis_log_buffer_size - (fix_length + sizeof(char) +
                     sizeof(char) + sizeof(char) + filename_length +
                     sizeof(char))) / (sizeof(unsigned int) + sizeof(char));
      loops = no_of_queued_jobs / max_entries;
      rest = no_of_queued_jobs % max_entries;
      total_loops = loops;
      if (rest)
      {
         total_loops++;
      }
      *p_jobs_queued = max_entries;
      for (i = 0; i < loops; i++)
      {
         offset = max_entries * sizeof(unsigned int);
         (void)memcpy(p_jid_list, (char *)*job_id, offset);
         *(p_jid_list + offset) = dis_type;
         *(p_jid_list + offset + 1) = total_loops;
         *(p_jid_list + offset + 2) = i;
         (void)memcpy(p_jid_list + offset + 3, proc_cycles, max_entries);
         (void)memcpy(p_jid_list + offset + 3 + max_entries, filename,
                      filename_length);
         offset += 3 + max_entries + filename_length + fix_length;
         if (write(dis_log_fd, dis_log_buffer, offset) != offset)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to write() %d bytes : %s",
                       offset, strerror(errno));
         }
      }
      *p_jobs_queued = rest;
      if (rest)
      {
         offset = rest * sizeof(unsigned int);
         (void)memcpy(p_jid_list, (char *)*job_id, rest);
         *(p_jid_list + offset) = dis_type;
         *(p_jid_list + offset + 1) = total_loops;
         *(p_jid_list + offset + 2) = i;
         (void)memcpy(p_jid_list + offset + 3, proc_cycles, rest);
         (void)memcpy(p_jid_list + offset + 3 + rest, filename,
                      filename_length);
         offset += 3 + rest + filename_length + fix_length;
         if (write(dis_log_fd, dis_log_buffer, offset) != offset)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to write() %d bytes : %s",
                       offset, strerror(errno));
         }
      }
   }
   else
   {
      *p_jobs_queued = no_of_queued_jobs;
      offset = no_of_queued_jobs * sizeof(unsigned int);
      (void)memcpy(p_jid_list, (char *)*job_id, offset);
      *(p_jid_list + offset) = dis_type;
      *(p_jid_list + offset + 1) = 1;
      *(p_jid_list + offset + 2) = 0;
      (void)memcpy(p_jid_list + offset + 3, proc_cycles, no_of_queued_jobs);
      (void)memcpy(p_jid_list + offset + 3 + no_of_queued_jobs, filename,
                   filename_length);
      offset += 3 + no_of_queued_jobs + filename_length + fix_length;
      if (write(dis_log_fd, dis_log_buffer, offset) != offset)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to write() %d bytes : %s",
                    offset, strerror(errno));
      }
   }

   return;
}


/*########################## release_dis_log() ##########################*/
void
release_dis_log(void)
{
   if (dis_log_fd != -1)
   {
      if (close(dis_log_fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "close() error : %s", strerror(errno));
      }
      dis_log_fd = -1;
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (close(dis_log_readfd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "close() error : %s", strerror(errno));
      }
      dis_log_readfd = -1;
#endif
   }
   if (dis_log_buffer != NULL)
   {
      (void)free(dis_log_buffer);
      dis_log_buffer = NULL;
   }

   return;
}
