/*
 *  confirmation_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2015 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   confirmation_log - logs all file names distributed by the AFD.
 **
 ** SYNOPSIS
 **   confirmation_log [--version][-w <working directory>]
 **
 ** DESCRIPTION
 **   This function reads from the fifo CONFIRMATION_LOG_FIFO any file name
 **   that was distributed by sf_xxx(). The data in the fifo has the
 **   following structure:
 **   <TD><FS><NR><JN><UNL><FNL><ANL><HN>\0<LFN>[ /<RFN>]\0[<AD>\0]
 **     |   |   |   |   |    |    |    |     |       |       |
 **     |   |   |   |   |    |    |    |     |   +---+       |
 **     |   |   |   |   |    |    |    |     |   |           V
 **     |   |   |   |   |    |    |    |     |   |   If ANL >0 this
 **     |   |   |   |   |    |    |    |     |   |   contains a \0
 **     |   |   |   |   |    |    |    |     |   |   terminated string of
 **     |   |   |   |   |    |    |    |     |   |   the Archive Directory.
 **     |   |   |   |   |    |    |    |     |   +-> Remote file name.
 **     |   |   |   |   |    |    |    |     +-----> Local file name.
 **     |   |   |   |   |    |    |    +-----------> \0 terminated string
 **     |   |   |   |   |    |    |                  of the Host Name,
 **     |   |   |   |   |    |    |                  output type, current
 **     |   |   |   |   |    |    |                  toggle and protocol.
 **     |   |   |   |   |    |    +----------------> An unsigned short
 **     |   |   |   |   |    |                       holding the Archive
 **     |   |   |   |   |    |                       Name Length if the
 **     |   |   |   |   |    |                       file has been archived.
 **     |   |   |   |   |    |                       If not, ANL = 0.
 **     |   |   |   |   |    +---------------------> Unsigned short holding
 **     |   |   |   |   |                            the File Name Length.
 **     |   |   |   |   +--------------------------> Unsigned short holding
 **     |   |   |   |                                the Unique name length.
 **     |   |   |   +------------------------------> Unsigned int holding
 **     |   |   |                                    the Job Number.
 **     |   |   +----------------------------------> Number of retries.
 **     |   +--------------------------------------> File Size of type
 **     |                                            off_t.
 **     +------------------------------------------> Transfer Duration of
 **                                                  type clock_t.
 **
 **   This data is then written to the confirmation log file in the following
 **   format:
 **
 **   40bae084  btx      0 0 1|dat.txt|[/...]|14a0|0.03|0|4a|40bae085_184_0[ 79095820F6][|btx/emp/0/9_863042081_0239]
 **      |       |       | | |    |      |     |     |  |  |            |        |               |
 |       |       |  +----+ | |    |      |     |     |  |  |            |     +--+               |
 **      |       |  | +----+ |    |      |     |     |  |  +-----+      |     |       +----------+
 **      |       |  | |      |    |      |     |     |  +----+   |      |     |       |
 **   Transfer Host | | Transfer Local Remote File Transfer  |  Job   Unique Mail  Archive
 **    time    name | |   type   file   file  size duration  | number   ID    ID  directory
 **           +-----+ |          name   name                 |
 **           |       |                                      |
 **         Output Current                           Number of retries
 **         type   toggle
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   04.05.2015 H.Kiehl Created
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
FILE       *confirmation_file = NULL;
int        sys_log_fd = STDERR_FILENO;
char       *iobuf = NULL,
           *p_work_dir = NULL;
const char *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void confirmation_log_exit(void),
            sig_exit(int);
/* #define _TEST_FIFO_BUFFER */
#ifdef _TEST_FIFO_BUFFER
static void show_buffer(char *, int);
#endif


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int            bytes_buffered = 0,
                  log_number = 0,
                  n,
                  length,
                  max_confirmation_log_files = MAX_CONFIRMATION_LOG_FILES,
                  no_of_buffered_writes = 0,
                  check_size,
                  status,
                  log_fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
   int            writefd;
#endif
   off_t          *file_size;
   time_t         next_file_time,
                  now;
   unsigned int   *job_number,
                  *retries;
   clock_t        clktck,
                  *transfer_duration;
   long           fifo_size;
   char           *p_end,
                  *fifo_buffer,
                  *p_host_name,
                  *p_file_name,
                  *work_dir,
                  current_log_file[MAX_PATH_LENGTH],
                  log_file[MAX_PATH_LENGTH],
                  unique_string[MAX_ADD_FNL + 1 + MAX_MAIL_ID_LENGTH + 1];
   unsigned short *archive_name_length,
                  *file_name_length,
                  *unl;
   fd_set         rset;
   struct timeval timeout;
#ifdef HAVE_STATX
   struct statx   stat_buf;
#else
   struct stat    stat_buf;
#endif

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
   (void)strcat(log_file, CONFIRMATION_LOG_FIFO);
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
                    "Failed to open() fifo %s : %s", log_file, strerror(errno));
         exit(INCORRECT);
      }
   }

   /*
    * Lets determine the largest offset so the 'structure'
    * is aligned correctly.
    */
   n = sizeof(clock_t);
   if (sizeof(off_t) > n)
   {
      n = sizeof(off_t);
   }
   if (sizeof(unsigned int) > n)
   {
      n = sizeof(unsigned int);
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
   if (fifo_size < (n + n + n + n + MAX_HOSTNAME_LENGTH + 6 +
                    1 + sizeof(unsigned short) + sizeof(unsigned short) +
                    sizeof(unsigned short) + MAX_FILENAME_LENGTH +
                    MAX_FILENAME_LENGTH + 2 + MAX_FILENAME_LENGTH))
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Fifo is NOT large enough to ensure atomic writes!");
      fifo_size = n + n + n + n + MAX_HOSTNAME_LENGTH + 6 +
                  1 + sizeof(unsigned short) + sizeof(unsigned short) +
                  sizeof(unsigned short) + MAX_FILENAME_LENGTH +
                  MAX_FILENAME_LENGTH + 2 + MAX_FILENAME_LENGTH;
   }

   /* Now lets allocate memory for the fifo buffer. */
   if ((fifo_buffer = malloc((size_t)fifo_size)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not allocate memory for the fifo buffer : %s",
                 strerror(errno));
      exit(INCORRECT);
   }

   /*
    * Get clock ticks per second, so we can calculate the transfer time.
    */
   if ((clktck = sysconf(_SC_CLK_TCK)) <= 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not get clock ticks per second : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Get the maximum number of logfiles we keep for history. */
   get_max_log_values(&max_confirmation_log_files,
                      MAX_CONFIRMATION_LOG_FILES_DEF,
                      MAX_CONFIRMATION_LOG_FILES, NULL, NULL, 0,
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
    * Lets open the confirmation file name buffer. If it does not yet exists
    * create it.
    */
   get_log_number(&log_number,
                  (max_confirmation_log_files - 1),
                  CONFIRMATION_BUFFER_FILE,
                  CONFIRMATION_BUFFER_FILE_LENGTH,
                  NULL);
   (void)snprintf(current_log_file, MAX_PATH_LENGTH, "%s%s/%s0",
                  work_dir, LOG_DIR, CONFIRMATION_BUFFER_FILE);
   p_end = log_file + snprintf(log_file, MAX_PATH_LENGTH, "%s%s/%s",
                               work_dir, LOG_DIR, CONFIRMATION_BUFFER_FILE);

   /* Calculate time when we have to start a new file. */
   next_file_time = (time(NULL) / SWITCH_FILE_TIME) * SWITCH_FILE_TIME +
                    SWITCH_FILE_TIME;

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
         if (log_number < (max_confirmation_log_files - 1))
         {
            log_number++;
         }
         if (max_confirmation_log_files > 1)
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

   confirmation_file = open_log_file(current_log_file);

#ifdef WITH_LOG_TYPE_DATA
   # Write log type data.
   (void)fprintf(confirmation_file, "#!# %d %d\n",
                 LOG_DATE_LENGTH, MAX_HOSTNAME_LENGTH);
#endif /* WITH_LOG_TYPE_DATA */

   /* Position pointers in fifo so that we only need to read */
   /* the data as they are in the fifo.                      */
   transfer_duration = (clock_t *)fifo_buffer;
   file_size = (off_t *)(fifo_buffer + n);
   retries = (unsigned int *)(fifo_buffer + n + n);
   job_number = (unsigned int *)(fifo_buffer + n + n + n);
   unl = (unsigned short *)(fifo_buffer + n + n + n + n);
   file_name_length = (unsigned short *)(fifo_buffer + n + n + n  + n +
                                         sizeof(unsigned short));
   archive_name_length = (unsigned short *)(fifo_buffer + n + n + n + n +
                                            sizeof(unsigned short) +
                                            sizeof(unsigned short));
   p_host_name = (char *)(fifo_buffer + n + n + n +
                          n + sizeof(unsigned short) + sizeof(unsigned short) + sizeof(unsigned short));
   p_file_name = (char *)(fifo_buffer + n + n + n + n +
                          sizeof(unsigned short) + sizeof(unsigned short) +
                          sizeof(unsigned short) + MAX_HOSTNAME_LENGTH + 6 + 1);
   check_size = n + n + n + n +
                sizeof(unsigned short) + sizeof(unsigned short) +
                sizeof(unsigned short) + MAX_HOSTNAME_LENGTH + 6 + 1 + 1;

   /* Do some cleanups when we exit. */
   if (atexit(confirmation_log_exit) != 0)
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
    * confirmation log.
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
            (void)fflush(confirmation_file);
            no_of_buffered_writes = 0;
         }

         /* Check if we have to create a new log file. */
         if (time(&now) > next_file_time)
         {
            if (log_number < (max_confirmation_log_files - 1))
            {
               log_number++;
            }
            if (fclose(confirmation_file) == EOF)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "fclose() error : %s", strerror(errno));
            }
            confirmation_file = NULL;
            if (max_confirmation_log_files > 1)
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
            confirmation_file = open_log_file(current_log_file);

#ifdef WITH_LOG_TYPE_DATA
            # Write log type data.
            (void)fprintf(confirmation_file, "#!# %d %d\n",
                          LOG_DATE_LENGTH, MAX_HOSTNAME_LENGTH);
#endif /* WITH_LOG_TYPE_DATA */
            next_file_time = (now / SWITCH_FILE_TIME) * SWITCH_FILE_TIME +
                             SWITCH_FILE_TIME;
         }
      }
      else if (FD_ISSET(log_fd, &rset))
           {
              /*
               * It is accurate enough to look at the time once only,
               * even though we are writting in a loop to the confirmation
               * file.
               */
              now = time(NULL);

              /*
               * Aaaah. Some new data has arrived. Lets write this
               * data to the file name confirmation log. The data in the 
               * fifo always has the following format:
               *
               *   <transfer duration><file size><job number><host name>
               *   <file name length><file name + archive dir>
               */
              if ((n = read(log_fd, &fifo_buffer[bytes_buffered],
                            fifo_size - bytes_buffered)) > 0)
              {
                 n += bytes_buffered;
#ifdef _TEST_FIFO_BUFFER
                 if (bytes_buffered > 0)
                 {
                    system_log(DEBUG_SIGN, __FILE__, __LINE__,
                               "After confirmation_log buffer overflow (n = %d)", n);
                    show_buffer(fifo_buffer, n);
                 }
#endif
                 bytes_buffered = 0;
                 do
                 {
                    /*
                     * Lets check that we have everything in the
                     * buffer.
                     */
                    if ((n < (check_size - 1)) ||
                        (n < (check_size + *file_name_length)) ||
                        ((*archive_name_length != 0) &&
                         (n < (check_size + *file_name_length + *archive_name_length + 1))))
                    {
#ifdef _TEST_FIFO_BUFFER
                       system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                  "confirmation_log buffer overflow (n = %d)", n);
                       show_buffer(fifo_buffer, n);
#endif
                       length = n;
                       bytes_buffered = n;
                    }
                    else
                    {
                       if (*unl > (MAX_ADD_FNL + 1 + MAX_MAIL_ID_LENGTH))
                       {
                          unique_string[0] = unique_string[2] = unique_string[4] = '0';
                          unique_string[1] = unique_string[3] = '_';
                          unique_string[5] = '\0';
                          system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                     "unique name offset is %d long, thus longer then %d",
                                     *unl, MAX_ADD_FNL + 1 + MAX_MAIL_ID_LENGTH);
                       }
                       else
                       {
                          (void)memcpy(unique_string, p_file_name, *unl);
                          unique_string[*unl] = '\0';
                       }
                       if (*archive_name_length > 0)
                       {
                          (void)fprintf(confirmation_file,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                        "%-*lx %s%c%s%c%lx%c%.2f%c%x%c%x%c%s%c%s\n",
# else
                                        "%-*llx %s%c%s%c%lx%c%.2f%c%x%c%x%c%s%c%s\n",
# endif
#else
# if SIZEOF_TIME_T == 4
                                        "%-*lx %s%c%s%c%llx%c%.2f%c%x%c%x%c%s%c%s\n",
# else
                                        "%-*llx %s%c%s%c%llx%c%.2f%c%x%c%x%c%s%c%s\n",
# endif
#endif
                                        LOG_DATE_LENGTH, (pri_time_t)now,
                                        p_host_name,
                                        SEPARATOR_CHAR,
                                        p_file_name + *unl,
                                        SEPARATOR_CHAR,
                                        (pri_off_t)*file_size,
                                        SEPARATOR_CHAR,
                                        *transfer_duration / (double)clktck,
                                        SEPARATOR_CHAR,
                                        *retries,
                                        SEPARATOR_CHAR,
                                        *job_number,
                                        SEPARATOR_CHAR,
                                        unique_string,
                                        SEPARATOR_CHAR,
                                        &p_file_name[*file_name_length + 1]);
                          length = check_size + *file_name_length +
                                   *archive_name_length + 1;
                       }
                       else
                       {
                          (void)fprintf(confirmation_file,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                        "%-*lx %s%c%s%c%lx%c%.2f%c%x%c%x%c%s\n",
# else
                                        "%-*llx %s%c%s%c%lx%c%.2f%c%x%c%x%c%s\n",
# endif
#else
# if SIZEOF_TIME_T == 4
                                        "%-*lx %s%c%s%c%llx%c%.2f%c%x%c%x%c%s\n",
# else
                                        "%-*llx %s%c%s%c%llx%c%.2f%c%x%c%x%c%s\n",
# endif
#endif
                                        LOG_DATE_LENGTH, (pri_time_t)now,
                                        p_host_name,
                                        SEPARATOR_CHAR,
                                        p_file_name + *unl,
                                        SEPARATOR_CHAR,
                                        (pri_off_t)*file_size,
                                        SEPARATOR_CHAR,
                                        *transfer_duration / (double)clktck,
                                        SEPARATOR_CHAR,
                                        *retries,
                                        SEPARATOR_CHAR,
                                        *job_number,
                                        SEPARATOR_CHAR,
                                        unique_string);
                          length = check_size + *file_name_length;
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
                    (void)fflush(confirmation_file);
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
                 if (log_number < (max_confirmation_log_files - 1))
                 {
                    log_number++;
                 }
                 if (fclose(confirmation_file) == EOF)
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "fclose() error : %s", strerror(errno));
                 }
                 confirmation_file = NULL;
                 if (max_confirmation_log_files > 1)
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
                 confirmation_file = open_log_file(current_log_file);

#ifdef WITH_LOG_TYPE_DATA
                 # Write log type data.
                 (void)fprintf(confirmation_file, "#!# %d %d\n",
                               LOG_DATE_LENGTH, MAX_HOSTNAME_LENGTH);
#endif /* WITH_LOG_TYPE_DATA */
                 next_file_time = (now / SWITCH_FILE_TIME) * SWITCH_FILE_TIME + SWITCH_FILE_TIME;
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


#ifdef _TEST_FIFO_BUFFER
# define MAX_CHARS_IN_LINE 60
/*++++++++++++++++++++++++++++++ show_buffer() ++++++++++++++++++++++++++*/
static void
show_buffer(char *buffer, int buffer_length)
{
   int  line_length;
   char *ptr = buffer,
        line[MAX_CHARS_IN_LINE + 10];

   do
   {
      line_length = 0;
      do
      {
         if (*ptr < ' ')
         {
            /* Yuck! Not printable. */
            line_length += snprintf(&line[line_length],
                                    MAX_CHARS_IN_LINE + 10 - line_length,
# ifdef _SHOW_HEX
                                    "<%x>", (int)*ptr);
# else
                                    "<%d>", (int)*ptr);
# endif
         }
         else
         {
            line[line_length] = *ptr;
            line_length++;
         }

         ptr++; buffer_length--;
      } while ((line_length <= MAX_CHARS_IN_LINE) && (buffer_length > 0));

      line[line_length] = '\0';
      system_log(DEBUG_SIGN, NULL, 0, "%s", line);
   } while (buffer_length > 0);

   return;
}
#endif


/*++++++++++++++++++++++++ confirmation_log_exit() ++++++++++++++++++++++*/
static void
confirmation_log_exit(void)
{
   if (confirmation_file != NULL)
   {
      (void)fflush(confirmation_file);
      if (fclose(confirmation_file) == EOF)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "fclose() error : %s", strerror(errno));

      confirmation_file = NULL;
   }

   return;
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   exit(INCORRECT);
}
