/*
 *  input_log.c - Part of AFD, an automatic file distribution program.
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
 **   input_log - logs all file names that are picked up by the AFD.
 **
 ** SYNOPSIS
 **   input_log [--version][-w <working directory>]
 **
 ** DESCRIPTION
 **   This function reads from the fifo INPUT_LOG_FIFO any file name
 **   that has been picked up by the AMG. The data in the fifo has the
 **   following structure:
 **       <FS><FT><DN><UN><FN>\0
 **         |   |   |   |   |
 **         |   |   |   |   +--------------> \0 terminated string of
 **         |   |   |   |                    the File Name.
 **         |   |   |   +------------------> int holding a unique number.
 **         |   |   +----------------------> Unsigned int holding the
 **         |   |                            Directory Number.
 **         |   +--------------------------> File Time of type time_t.
 **         +------------------------------> File Size of type off_t.
 **
 **   This data is then written to the input log file in the following
 **   format:
 **
 **        40bae083   dat.txt|20b50|e8c13458|ba2366
 **           |          |     |       |        |
 **           |          |     |       |        |
 **           |          |     |       |        |
 **        Pickup      File  File  Directory  Unique
 **         time       name  size  Identifier number
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.02.1997 H.Kiehl Created
 **   07.01.2001 H.Kiehl Build in some checks when fifo buffer overflows.
 **   14.06.2001 H.Kiehl Removed the above unneccessary checks.
 **   13.04.2002 H.Kiehl Added SEPARATOR_CHAR.
 **   27.09.2004 H.Kiehl Addition of unique number.
 **   26.03.2008 H.Kiehl Receive input time via fifo.
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
FILE       *input_file = NULL;
int        sys_log_fd = STDERR_FILENO;
char       *iobuf = NULL,
           *p_work_dir = NULL;
const char *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void input_log_exit(void),
            sig_exit(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int            bytes_buffered = 0,
                  check_size,
                  length,
                  log_fd,
                  log_number = 0,
                  max_input_log_files = MAX_INPUT_LOG_FILES,
                  n,
                  no_of_buffered_writes = 0,
                  status,
                  *unique_number;
#ifdef WITHOUT_FIFO_RW_SUPPORT
   int            writefd;
#endif
   off_t          *file_size;
   time_t         *file_time,
                  next_file_time,
                  now;
   unsigned int   *dir_number;
   long           fifo_size;
   char           current_log_file[MAX_PATH_LENGTH],
                  *fifo_buffer,
                  log_file[MAX_PATH_LENGTH],
                  *p_end,
                  *p_file_name,
                  *work_dir;
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
   (void)strcat(log_file, INPUT_LOG_FIFO);
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
   n = sizeof(off_t);
   if (sizeof(time_t) > n)
   {
      n = sizeof(time_t);
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
   if (fifo_size < (n + n + n + n + MAX_FILENAME_LENGTH + sizeof(char)))
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Fifo is NOT large enough to ensure atomic writes!");
      fifo_size = n + n + n + n + MAX_FILENAME_LENGTH + sizeof(char);
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
   get_max_log_values(&max_input_log_files, MAX_INPUT_LOG_FILES_DEF,
                      MAX_INPUT_LOG_FILES, NULL, NULL, 0, AFD_CONFIG_FILE);

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
    * Lets open the input file name buffer. If it does not yet exists
    * create it.
    */
   get_log_number(&log_number,
                  (max_input_log_files - 1),
                  INPUT_BUFFER_FILE,
                  INPUT_BUFFER_FILE_LENGTH,
                  NULL);
   (void)snprintf(current_log_file, MAX_PATH_LENGTH, "%s%s/%s0",
                  work_dir, LOG_DIR, INPUT_BUFFER_FILE);
   p_end = log_file + snprintf(log_file, MAX_PATH_LENGTH, "%s%s/%s",
                               work_dir, LOG_DIR, INPUT_BUFFER_FILE);

   /* Calculate time when we have to start a new file. */
   next_file_time = (time(NULL) / SWITCH_FILE_TIME) * SWITCH_FILE_TIME + SWITCH_FILE_TIME;

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
         if (log_number < (max_input_log_files - 1))
         {
            log_number++;
         }
         if (max_input_log_files > 1)
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
   input_file = open_log_file(current_log_file, NULL, NULL, NULL);
#else
   input_file = open_log_file(current_log_file);
#endif

#ifdef WITH_LOG_TYPE_DATA
   # Write log type data.
   (void)fprintf(input_file, "#!# %d %d\n",
                 LOG_DATE_LENGTH, MAX_HOSTNAME_LENGTH);
#endif /* WITH_LOG_TYPE_DATA */

   /* Position pointers in fifo so that we only need to read */
   /* the data as they are in the fifo.                      */
   file_size = (off_t *)(fifo_buffer);
   file_time = (time_t *)(fifo_buffer + n);
   dir_number = (unsigned int *)(fifo_buffer + n + n);
   unique_number = (int *)(fifo_buffer + n + n + n);
   p_file_name = (char *)(fifo_buffer + n + n + n + n);
   check_size = n + n + n + n + 1;

   /* Do some cleanups when we exit. */
   if (atexit(input_log_exit) != 0)
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
    * input log.
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
            (void)fflush(input_file);
            no_of_buffered_writes = 0;
         }

         /* Check if we have to create a new log file. */
         if (time(&now) > next_file_time)
         {
            if (log_number < (max_input_log_files - 1))
            {
               log_number++;
            }
            if (fclose(input_file) == EOF)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "fclose() error : %s", strerror(errno));
            }
            input_file = NULL;
            if (max_input_log_files > 1)
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
            input_file = open_log_file(current_log_file, NULL, NULL, NULL);
#else
            input_file = open_log_file(current_log_file);
#endif

#ifdef WITH_LOG_TYPE_DATA
            # Write log type data.
            (void)fprintf(input_file, "#!# %d %d\n",
                          LOG_DATE_LENGTH, MAX_HOSTNAME_LENGTH);
#endif /* WITH_LOG_TYPE_DATA */
            next_file_time = (now / SWITCH_FILE_TIME) * SWITCH_FILE_TIME +
                             SWITCH_FILE_TIME;
         }
      }
      else if (FD_ISSET(log_fd, &rset))
           {
              now = time(NULL);

              /*
               * Aaaah. Some new data has arrived. Lets write this
               * data to the file name input log. The data in the 
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
                    if (n < (check_size - 1))
                    {
                       length = n;
                       bytes_buffered = n;
                    }
                    else
                    {
                       length = strlen(p_file_name);
                       if (n < (check_size + length))
                       {
                          length = n;
                          bytes_buffered = n;
                       }
                       else
                       {
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                          (void)fprintf(input_file, "%-*lx %s%c%lx%c%x%c%x\n",
# else
                          (void)fprintf(input_file, "%-*llx %s%c%lx%c%x%c%x\n",
# endif
#else
# if SIZEOF_TIME_T == 4
                          (void)fprintf(input_file, "%-*lx %s%c%llx%c%x%c%x\n",
# else
                          (void)fprintf(input_file, "%-*llx %s%c%llx%c%x%c%x\n",
# endif
#endif
                                        LOG_DATE_LENGTH, (pri_time_t)*file_time,
                                        p_file_name,
                                        SEPARATOR_CHAR,
                                        (pri_off_t)*file_size,
                                        SEPARATOR_CHAR,
                                        *dir_number,
                                        SEPARATOR_CHAR,
                                        *unique_number);
                          length = length + check_size;
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
                    (void)fflush(input_file);
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
                 if (log_number < (max_input_log_files - 1))
                 {
                    log_number++;
                 }
                 if (fclose(input_file) == EOF)
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "fclose() error : %s", strerror(errno));
                 }
                 input_file = NULL;
                 if (max_input_log_files > 1)
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
                 input_file = open_log_file(current_log_file, NULL, NULL, NULL);
#else
                 input_file = open_log_file(current_log_file);
#endif

#ifdef WITH_LOG_TYPE_DATA
                 # Write log type data.
                 (void)fprintf(input_file, "#!# %d %d\n",
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


/*+++++++++++++++++++++++++++ input_log_exit() ++++++++++++++++++++++++++*/
static void
input_log_exit(void)
{
   if (input_file != NULL)
   {
      (void)fflush(input_file);
      if (fclose(input_file) == EOF)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "fclose() error : %s", strerror(errno));
      }
      input_file = NULL;
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
                 INPUT_LOG_PROCESS, signo, (pri_pid_t)getpid());
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
