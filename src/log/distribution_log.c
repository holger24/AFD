/*
 *  distribution_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2023 Deutscher Wetterdienst (DWD),
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

DESCR__S_M1
/*
 ** NAME
 **   distribution_log - logs all file names that are picked up by the AFD.
 **
 ** SYNOPSIS
 **   distribution_log [--version][-w <working directory>]
 **
 ** DESCRIPTION
 **   This function reads from the fifo DISTRIBUTION_LOG_FIFO any file name
 **   that has been picked up and distributed by the AMG. The data in the
 **   fifo has the following structure:
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
 **   This data is then written to the distribution log file in the following
 **   format:
 **
 **   40bae084   0-1|dat.txt|40bae083|e8c13458|ba2366|20b50|556a367_0,4e327a5_3
 **      |        |   |        |       |        |      |    |
 **      |        |   |        |       |        |      |    +-> Jobs that
 **      |        |   |        |       |        |      |        received the
 **      |        |   |        |       |        |      |        file. The last
 **      |        |   |        |       |        |      |        number says how
 **      |        |   |        |       |        |      |        many times it
 **      |        |   |        |       |        |      |        appears in
 **      |        |   |        |       |        |      |        PRODUCTION_LOG.
 **      |        |   |        |       |        |      +------> File size.
 **      |        |   |        |       |        +-------------> Unique number.
 **      |        |   |        |       +----------------------> Directory
 **      |        |   |        |                                Identifier.
 **      |        |   |        +------------------------------> Pickup time.
 **      |        |   +---------------------------------------> File name.
 **      |        +-------------------------------------------> First digit
 **      |                                                      Distribution
 **      |                                                      type:
 **      |                                                       0 - Normal
 **      |                                                       1 - Time job
 **      |                                                       2 - Queue
 **      |                                                           stopped
 **      |                                                       3 - Disabled
 **      |                                                       4 - Age limit
 **      |                                                            delete
 **      |                                                       5 - Dupcheck
 **      |                                                      Second digit
 **      |                                                      is number of
 **      |                                                      distribution
 **      |                                                      types used.
 **      +----------------------------------------------------> Date when
 **                                                             logged.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   08.04.2008 H.Kiehl Created
 **   09.02.2017 H.Kiehl Flush buffers when we exit.
 **
 */
DESCR__E_M1

#include <stdio.h>           /* fopen(), fflush()                        */
#include <string.h>          /* strcpy(), strcat(), strerror(), memcpy() */
#include <stdlib.h>          /* malloc(), atexit()                       */
#include <time.h>            /* time()                                   */
#include <sys/types.h>       /* fdset                                    */
#include <sys/stat.h>
#include <sys/time.h>        /* struct timeval                           */
#include <unistd.h>          /* fpathconf(), sysconf()                   */
#include <fcntl.h>           /* O_RDWR, open()                           */
#include <signal.h>          /* signal()                                 */
#include <errno.h>
#include "logdefs.h"
#include "version.h"


/* External global variables. */
FILE       *distribution_file = NULL;
int        sys_log_fd = STDERR_FILENO;
char       *iobuf = NULL,
           *p_work_dir = NULL;
const char *sys_log_name = SYSTEM_LOG_FIFO;


/* Local function prototypes. */
static void distribution_log_exit(void),
            sig_exit(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int                  bytes_buffered = 0,
                        check_size,
                        i,
                        j,
                        length,
                        lines_buffered = 0,
                        log_fd,
                        log_number = 0,
                        max_distribution_log_files = MAX_DISTRIBUTION_LOG_FILES,
                        n,
                        no_of_buffered_writes = 0,
                        offset_type,
                        required_length,
                        status;
#ifdef WITHOUT_FIFO_RW_SUPPORT
   int                  writefd;
#endif
   time_t               *input_time,
                        next_file_time,
                        next_segmented_buffer_time,
                        now;
   off_t                *file_size;
   unsigned int         *dir_number,
                        *filename_length,
                        *jid_list,
                        *jobs_queued,
                        *no_of_distribution_types,
                        *unique_number;
   long                 fifo_size;
   char                 current_log_file[MAX_PATH_LENGTH],
                        *fifo_buffer,
                        filename[MAX_FILENAME_LENGTH + 1],
                        log_file[MAX_PATH_LENGTH],
                        *p_end,
                        *work_dir;
   fd_set               rset;
   struct timeval       timeout;
#ifdef HAVE_STATX
   struct statx         stat_buf;
#else
   struct stat          stat_buf;
#endif
   struct buffered_line *bl = NULL;

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, log_file) < 0)
   {
      exit(INCORRECT);
   }
   if ((work_dir = malloc((strlen(log_file) + 1))) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() memory : %s",
                 strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   (void)strcpy(work_dir, log_file);
   p_work_dir = work_dir;

   /* Create and open fifos that we need. */
   (void)strcat(log_file, FIFO_DIR);
   (void)strcat(log_file, DISTRIBUTION_LOG_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(log_file, &log_fd, &writefd) == -1)
#else
   if ((log_fd = open(log_file, O_RDWR)) == -1)
#endif
   {
      if (errno == ENOENT)
      {
         if (make_fifo(log_file) == SUCCESS)
         {
#ifdef WITHOUT_FIFO_RW_SUPPORT
            if (open_fifo_rw(log_file, &log_fd, &writefd) == -1)
#else
            if ((log_fd = open(log_file, O_RDWR)) == -1)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to open() fifo %s : %s",
                          log_file, strerror(errno));
               exit(INCORRECT);
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to create fifo %s.", log_file);
            exit(INCORRECT);
         }
      }
      else
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to open() fifo %s : %s",
                    log_file, strerror(errno));
         exit(INCORRECT);
      }
   }

   /*
    * Lets determine the largest offset so the 'structure'
    * is aligned correctly.
    */
   length = sizeof(off_t);
   if (sizeof(time_t) > length)
   {
      length = sizeof(time_t);
   }

   /*
    * Determine the size of the fifo buffer. Then create a buffer
    * large enough to hold the data from a fifo.
    */
   if ((fifo_size = fpathconf(log_fd, _PC_PIPE_BUF)) < 0)
   {
      /* If we cannot determine the size of the fifo set default value. */
      fifo_size = DEFAULT_FIFO_SIZE;
   }
   if (fifo_size < (length + length + sizeof(int) + sizeof(int) + sizeof(int) +
                    sizeof(unsigned int) + sizeof(int) + sizeof(unsigned int) +
                    sizeof(char) + sizeof(char) + sizeof(char) + sizeof(char) +
                    MAX_FILENAME_LENGTH))
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Fifo is NOT large enough to ensure atomic writes!");
      fifo_size = length +               /* Input time. */
                  length +               /* File size. */
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
                  MAX_FILENAME_LENGTH;
   }

   /* Now lets allocate memory for the fifo buffer. */
   if ((fifo_buffer = malloc((size_t)fifo_size)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not allocate memory for the fifo buffer : %s",
                 strerror(errno));
      exit(INCORRECT);
   }

   /* Get the maximum number of logfiles we keep for history. */
   get_max_log_values(&max_distribution_log_files,
                      MAX_DISTRIBUTION_LOG_FILES_DEF,
                      MAX_DISTRIBUTION_LOG_FILES, NULL, NULL, 0,
                      AFD_CONFIG_FILE);

   /*
    * Set umask so that all log files have the permission 644.
    * If we do not set this here fopen() will create files with
    * permission 666 according to POSIX.1.
    */
#ifdef GROUP_CAN_WRITE
   (void)umask(S_IWOTH);
#else
   (void)umask(S_IWGRP | S_IWOTH);
#endif

   /*
    * Lets open the distribution file name buffer. If it does not yet exists
    * create it.
    */
   get_log_number(&log_number, (max_distribution_log_files - 1),
                  DISTRIBUTION_BUFFER_FILE, DISTRIBUTION_BUFFER_FILE_LENGTH,
                  NULL);
   (void)snprintf(current_log_file, MAX_PATH_LENGTH, "%s%s/%s0",
                  work_dir, LOG_DIR, DISTRIBUTION_BUFFER_FILE);
   p_end = log_file + snprintf(log_file, MAX_PATH_LENGTH, "%s%s/%s",
                               work_dir, LOG_DIR, DISTRIBUTION_BUFFER_FILE);

   /* Calculate time when we have to start a new file. */
   next_file_time = (time(&now) / SWITCH_FILE_TIME) * SWITCH_FILE_TIME +
                    SWITCH_FILE_TIME;

   /* Calculate time when we have to check the */
   /* segment buffer for old entries.          */
   next_segmented_buffer_time = (now / SEGMENTED_BUFFER_CHECK_INTERVAL) *
                                SEGMENTED_BUFFER_CHECK_INTERVAL +
                                SEGMENTED_BUFFER_CHECK_INTERVAL;

   /* Is current log file already too old? */
#ifdef HAVE_STATX
   if (statx(0, current_log_file, AT_STATX_SYNC_AS_STAT,
             STATX_MTIME, &stat_buf) == 0)
#else
   if (stat(current_log_file, &stat_buf) == 0)
#endif
   {
#ifdef HAVE_STATX
      if (stat_buf.stx_mtime.tv_sec < (next_file_time - SWITCH_FILE_TIME))
#else
      if (stat_buf.st_mtime < (next_file_time - SWITCH_FILE_TIME))
#endif
      {
         if (log_number < (max_distribution_log_files - 1))
         {
            log_number++;
         }
         if (max_distribution_log_files > 1)
         {
            reshuffel_log_files(log_number, log_file, p_end, 0, 0);
         }
         else
         {
            if (unlink(current_log_file) == -1)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to unlink() current log file `%s' : %s",
                          current_log_file, strerror(errno));
            }
         }
      }
   }

#ifdef WITH_LOG_CACHE
   distribution_file = open_log_file(current_log_file, NULL, NULL, NULL);
#else
   distribution_file = open_log_file(current_log_file);
#endif
#ifdef WITH_LOG_TYPE_DATA
   # Write log type data.
   (void)fprintf(distribution_file, "#!# %d %d\n",
                 LOG_DATE_LENGTH, MAX_HOSTNAME_LENGTH);
#endif /* WITH_LOG_TYPE_DATA */

   /* Position pointers in fifo so that we only need to read */
   /* the data as they are in the fifo.                      */
   input_time = (time_t *)(fifo_buffer);
   file_size = (off_t *)(fifo_buffer + length);
   dir_number = (unsigned int *)(fifo_buffer + length + length);
   unique_number = (unsigned int *)(fifo_buffer + length + length +
                                    sizeof(int));
   filename_length = (unsigned int *)(fifo_buffer + length + length +
                                      sizeof(int) + sizeof(int));
   no_of_distribution_types = (unsigned int *)(fifo_buffer + length + length +
                                               sizeof(int) + sizeof(int) +
                                               sizeof(int));
   jobs_queued = (unsigned int *)(fifo_buffer + length + length + sizeof(int) +
                                  sizeof(int) + sizeof(int) + sizeof(int));
   jid_list = (unsigned int *)(fifo_buffer + length + length + sizeof(int) +
                               sizeof(int) + sizeof(int) + sizeof(int) +
                               sizeof(int));
   check_size = length + length + sizeof(int) + sizeof(int) + sizeof(int) +
                sizeof(int) + sizeof(int);

   /* Do some cleanups when we exit. */
   if (atexit(distribution_log_exit) != 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not register exit function : %s"), strerror(errno));
   }

   /* Initialise signal handlers. */
   if ((signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR) ||
       (signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR))
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "signal() error : %s", strerror(errno));
   }

   /*
    * Now lets wait for data to be written to the file name
    * distribution log.
    */
   FD_ZERO(&rset);
   for (;;)
   {
      /* Initialise descriptor set and timeout. */
      FD_SET(log_fd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = 3L;

      /* Wait for message x seconds and then continue. */
      status = select(log_fd + 1, &rset, NULL, NULL, &timeout);

      if (status == 0)
      {
         if (no_of_buffered_writes > 0)
         {
            (void)fflush(distribution_file);
            no_of_buffered_writes = 0;
         }

         /* Check if we have to create a new log file. */
         if (time(&now) > next_file_time)
         {
            if (log_number < (max_distribution_log_files - 1))
            {
               log_number++;
            }
            if (fclose(distribution_file) == EOF)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "fclose() error : %s", strerror(errno));
            }
            distribution_file = NULL;
            if (max_distribution_log_files > 1)
            {
               reshuffel_log_files(log_number, log_file, p_end, 0, 0);
            }
            else
            {
               if (unlink(current_log_file) == -1)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Failed to unlink() current log file `%s' : %s",
                             current_log_file, strerror(errno));
               }
            }
#ifdef WITH_LOG_CACHE
            distribution_file = open_log_file(current_log_file, NULL, NULL, NULL);
#else
            distribution_file = open_log_file(current_log_file);
#endif

#ifdef WITH_LOG_TYPE_DATA
            # Write log type data.
            (void)fprintf(distribution_file, "#!# %d %d\n",
                          LOG_DATE_LENGTH, MAX_HOSTNAME_LENGTH);
#endif /* WITH_LOG_TYPE_DATA */
            next_file_time = (now / SWITCH_FILE_TIME) * SWITCH_FILE_TIME +
                             SWITCH_FILE_TIME;
         }
         if ((lines_buffered > 0) && (now > next_segmented_buffer_time))
         {
            int lines_removed = 0;

            for (i = 0; i < lines_buffered; i++)
            {
               if ((now - bl[i].entry_time) > MAX_HOLD_TIME_SEGMENTED_LINE)
               {
                  /* Remove this line and free its buffer. */
                  free(bl[i].line);
                  if ((lines_buffered > 1) && ((i + 1) < lines_buffered))
                  {
                     (void)memmove(&bl[i], &bl[i + 1],
                                   ((lines_buffered - (i + 1)) * sizeof(struct buffered_line)));
                  }
                  lines_removed++;
                  lines_buffered--;
                  i--;
               }
            }
            if (lines_removed > 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Removed %d old segmented line(s) from buffer!",
                          lines_removed);
            }
            next_segmented_buffer_time = (now / SEGMENTED_BUFFER_CHECK_INTERVAL) *
                                         SEGMENTED_BUFFER_CHECK_INTERVAL +
                                         SEGMENTED_BUFFER_CHECK_INTERVAL;
         }
      }
      else if (FD_ISSET(log_fd, &rset))
           {
              now = time(NULL);

              /*
               * Aaaah. Some new data has arrived. Lets write this
               * data to the file name distribution log. The data in the 
               * fifo always has the following format:
               *
               *   <file size><dir number><unique number><file name>
               */
              if ((n = read(log_fd, &fifo_buffer[bytes_buffered],
                            fifo_size - bytes_buffered)) > 0)
              {
                 n += bytes_buffered;
                 bytes_buffered = 0;
                 do
                 {
                    if (n < check_size)
                    {
                       length = n;
                       bytes_buffered = n;
                    }
                    else
                    {
                       offset_type = check_size +
                                     (*jobs_queued * sizeof(unsigned int));
                       required_length = offset_type +
                                         sizeof(char) + sizeof(char) +
                                         sizeof(char) +
                                         (*jobs_queued * sizeof(char)) +
                                         *filename_length;
                       if (n < required_length)
                       {
                          length = n;
                          bytes_buffered = n;
                       }
                       else
                       {
                          (void)memcpy(filename,
                                       (fifo_buffer + offset_type + sizeof(char) + sizeof(char) + sizeof(char) + (*jobs_queued * sizeof(char))),
                                       *filename_length);
                          filename[*filename_length] = '\0';
                          if ((*(fifo_buffer + offset_type + sizeof(char)) == 1) ||
                              (lines_buffered >= MAX_SEGMENTED_LINES_BUFFERED))
                          {
                             (void)fprintf(distribution_file,
#if SIZEOF_TIME_T == 4
# if SIZEOF_OFF_T == 4
                                           "%-*lx %x-%x%c%s%c%lx%c%x%c%x%c%lx%c%x_%x",
# else
                                           "%-*lx %x-%x%c%s%c%lx%c%x%c%x%c%llx%c%x_%x",
# endif
#else
# if SIZEOF_OFF_T == 4
                                           "%-*llx %x-%x%c%s%c%llx%c%x%c%x%c%lx%c%x_%x",
# else
                                           "%-*llx %x-%x%c%s%c%llx%c%x%c%x%c%llx%c%x_%x",
# endif
#endif
                                           LOG_DATE_LENGTH, (pri_time_t)now,
                                           *(fifo_buffer + offset_type),
                                           *no_of_distribution_types,
                                           SEPARATOR_CHAR,
                                           filename,
                                           SEPARATOR_CHAR,
                                           (pri_time_t)*input_time,
                                           SEPARATOR_CHAR,
                                           *dir_number,
                                           SEPARATOR_CHAR,
                                           *unique_number,
                                           SEPARATOR_CHAR,
                                           (pri_off_t)*file_size,
                                           SEPARATOR_CHAR,
                                           jid_list[0],
                                           (unsigned int)*(fifo_buffer + offset_type + sizeof(char) + sizeof(char) + sizeof(char)));
                             for (i = 1; i < *jobs_queued; i++)
                             {
                                (void)fprintf(distribution_file, ",%x_%x",
                                              jid_list[i],
                                              (unsigned int)*(fifo_buffer + offset_type + sizeof(char) + sizeof(char) + sizeof(char) + (i * sizeof(char))));
                             }
                             (void)fprintf(distribution_file, "\n");
                          }
                          else /* We have a segmented message. */
                          {
                             if (*(fifo_buffer + offset_type + sizeof(char) + sizeof(char)) == 0)
                             {
                                if (bl == NULL)
                                {
                                   if ((bl = malloc(((MAX_SEGMENTED_LINES_BUFFERED + 1) * sizeof(struct buffered_line)))) == NULL)
                                   {
                                      system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                 "Failed to malloc() memory : %s",
                                                 strerror(errno), __FILE__, __LINE__);
                                      exit(INCORRECT);
                                   }
                                }
                                bl[lines_buffered].buffer_length = LOG_DATE_LENGTH + 2 + 1 + *filename_length + 1 + MAX_INT_HEX_LENGTH + 1 + MAX_INT_HEX_LENGTH + 1 + MAX_INT_HEX_LENGTH + 1 + (*jobs_queued * (MAX_INT_HEX_LENGTH + 3 + 1)) + 1;
                                bl[lines_buffered].did = *dir_number;
                                bl[lines_buffered].unique_number = *unique_number;
                                bl[lines_buffered].entry_time = time(NULL);
                                if ((bl[lines_buffered].line = malloc(bl[lines_buffered].buffer_length)) == NULL)
                                {
                                   system_log(ERROR_SIGN, __FILE__, __LINE__,
                                              "Failed to malloc() %d bytes : %s",
                                              bl[lines_buffered].buffer_length,
                                              strerror(errno));
                                   exit(INCORRECT);
                                }
                                bl[lines_buffered].line_offset = LOG_DATE_LENGTH +
                                                                 sprintf(bl[lines_buffered].line + LOG_DATE_LENGTH,
#if SIZEOF_TIME_T == 4
# if SIZEOF_OFF_T == 4
                                                                         " %x-%x%c%s%c%lx%c%x%c%x%c%lx%c%x_%x",
# else
                                                                         " %x-%x%c%s%c%lx%c%x%c%x%c%llx%c%x_%x",
# endif
#else
# if SIZEOF_OFF_T == 4
                                                                         " %x-%x%c%s%c%llx%c%x%c%x%c%lx%c%x_%x",
# else
                                                                         " %x-%x%c%s%c%llx%c%x%c%x%c%llx%c%x_%x",
# endif
#endif
                                                                         *(fifo_buffer + offset_type),
                                                                         *no_of_distribution_types,
                                                                         SEPARATOR_CHAR,
                                                                         filename,
                                                                         SEPARATOR_CHAR,
                                                                         (pri_time_t)*input_time,
                                                                         SEPARATOR_CHAR,
                                                                         *dir_number,
                                                                         SEPARATOR_CHAR,
                                                                         *unique_number,
                                                                         SEPARATOR_CHAR,
                                                                         (pri_off_t)*file_size,
                                                                         SEPARATOR_CHAR,
                                                                         jid_list[0],
                                                                         (unsigned int)*(fifo_buffer + offset_type + sizeof(char) + sizeof(char) + sizeof(char)));
                                for (i = 1; i < *jobs_queued; i++)
                                {
                                   bl[lines_buffered].line_offset += sprintf(&bl[lines_buffered].line[bl[lines_buffered].line_offset],
                                                                             ",%x_%x",
                                                                             jid_list[i],
                                                                             (unsigned int)*(fifo_buffer + offset_type + sizeof(char) + sizeof(char) + sizeof(char) + (i * sizeof(char))));
                                }
                                lines_buffered++;
                             }
                             else
                             {
                                /* Append data to an existing buffer. */
                                for (i = 0; i < lines_buffered; i++)
                                {
                                   if ((bl[i].did == *dir_number) &&
                                       (bl[i].unique_number = *unique_number))
                                   {
                                      break;
                                   }
                                }
                                if (i == lines_buffered)
                                {
                                   system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                              "Failed to locate job in buffer, discarding data!");
                                }
                                else
                                {
                                   for (j = 0; j < *jobs_queued; j++)
                                   {
                                      bl[i].line_offset += sprintf(&bl[i].line[bl[i].line_offset],
                                                                   ",%x_%x",
                                                                   jid_list[j],
                                                                   (unsigned int)*(fifo_buffer + offset_type + sizeof(char) + sizeof(char) + sizeof(char) + (j * sizeof(char))));
                                   }

                                   /* Is this the last segment? */
                                   if ((*(fifo_buffer + offset_type + sizeof(char)) - 1) ==
                                       *(fifo_buffer + offset_type + sizeof(char) + sizeof(char)))
                                   {
                                      /* Insert current time. */
                                      j = sprintf(bl[i].line,
#if SIZEOF_TIME_T == 4
                                                  "%-*lx",
#else
                                                  "%-*llx",
#endif
                                                  LOG_DATE_LENGTH, (pri_time_t)now);
                                      bl[i].line[j] = ' ';

                                      /* Write all segments to log. */
                                      (void)fprintf(distribution_file, "%s\n", bl[i].line);

                                      /* Remove this line and free its buffer. */
                                      free(bl[i].line);
                                      if ((lines_buffered > 1) && ((i + 1) < lines_buffered))
                                      {
                                         (void)memmove(&bl[i], &bl[i + 1],
                                                       ((lines_buffered - (i + 1)) * sizeof(struct buffered_line)));
                                      }

                                      lines_buffered--;
                                   }
                                }
                             }
                          }
                          length = required_length;
                       }
                    }
                    n -= length;
                    if (n > 0)
                    {
                       (void)memmove(fifo_buffer, &fifo_buffer[length], n);
                    }
                    no_of_buffered_writes++;
                 } while (n > 0);

                 if (no_of_buffered_writes > BUFFERED_WRITES_BEFORE_FLUSH_SLOW)
                 {
                    (void)fflush(distribution_file);
                    no_of_buffered_writes = 0;
                 }
              }
              else if (n < 0)
                   {
                      system_log(FATAL_SIGN, __FILE__, __LINE__,
                                 "read() error (%d) : %s", n, strerror(errno));
                      exit(INCORRECT);
                   }

              /*
               * Since we can receive a constant stream of data
               * on the fifo, we might never get that select() returns 0.
               * Thus we always have to check if it is time to create
               * a new log file.
               */
              if (now > next_file_time)
              {
                 if (log_number < (max_distribution_log_files - 1))
                 {
                    log_number++;
                 }
                 if (fclose(distribution_file) == EOF)
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "fclose() error : %s", strerror(errno));
                 }
                 distribution_file = NULL;
                 if (max_distribution_log_files > 1)
                 {
                    reshuffel_log_files(log_number, log_file, p_end, 0, 0);
                 }
                 else
                 {
                    if (unlink(current_log_file) == -1)
                    {
                       system_log(WARN_SIGN, __FILE__, __LINE__,
                                  "Failed to unlink() current log file `%s' : %s",
                                  current_log_file, strerror(errno));
                    }
                 }
#ifdef WITH_LOG_CACHE
                 distribution_file = open_log_file(current_log_file, NULL, NULL, NULL);
#else
                 distribution_file = open_log_file(current_log_file);
#endif

#ifdef WITH_LOG_TYPE_DATA
                 # Write log type data.
                 (void)fprintf(distribution_file, "#!# %d %d\n",
                               LOG_DATE_LENGTH, MAX_HOSTNAME_LENGTH);
#endif /* WITH_LOG_TYPE_DATA */
                 next_file_time = (now / SWITCH_FILE_TIME) * SWITCH_FILE_TIME +
                                  SWITCH_FILE_TIME;
              }
              if ((lines_buffered > 0) && (now > next_segmented_buffer_time))
              {
                 int lines_removed = 0;

                 for (i = 0; i < lines_buffered; i++)
                 {
                    if ((now - bl[i].entry_time) > MAX_HOLD_TIME_SEGMENTED_LINE)
                    {
                       /* Remove this line and free its buffer. */
                       free(bl[i].line);
                       if ((lines_buffered > 1) && ((i + 1) < lines_buffered))
                       {
                          (void)memmove(&bl[i], &bl[i + 1],
                                        ((lines_buffered - (i + 1)) * sizeof(struct buffered_line)));
                       }
                       lines_removed++;
                       lines_buffered--;
                       i--;
                    }
                 }
                 if (lines_removed > 0)
                 {
                    system_log(DEBUG_SIGN, __FILE__, __LINE__,
                               "Removed %d old segmented line(s) from buffer!",
                               lines_removed);
                 }
                 next_segmented_buffer_time = (now / SEGMENTED_BUFFER_CHECK_INTERVAL) *
                                              SEGMENTED_BUFFER_CHECK_INTERVAL +
                                              SEGMENTED_BUFFER_CHECK_INTERVAL;
              }
           }
           else
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Select error : %s", strerror(errno));
              exit(INCORRECT);
           }
   } /* for (;;) */

   /* Should never come to this point. */
   exit(SUCCESS);
}


/*+++++++++++++++++++++++ distribution_log_exit() +++++++++++++++++++++++*/
static void
distribution_log_exit(void)
{
   if (distribution_file != NULL)
   {
      (void)fflush(distribution_file);
      if (fclose(distribution_file) == EOF)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "fclose() error : %s", strerror(errno));
      }
      distribution_file = NULL;
   }

   return;
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
  int ret;

   (void)fprintf(stderr,
#if SIZEOF_PID_T == 4
                 "%s terminated by signal %d (%d)\n",
#else
                 "%s terminated by signal %d (%lld)\n",
#endif
                 DISTRIBUTION_LOG_PROCESS, signo, (pri_pid_t)getpid());
   if ((signo == SIGINT) || (signo == SIGTERM))
   {
      ret = SUCCESS;
   }
   else
   {
      ret = INCORRECT;
   }

   exit(ret);
}
