/*
 *  production_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   production_log - logs all file names that are being renamed or
 **                    where the content is changed
 **
 ** SYNOPSIS
 **   production_log [--version][-w <working directory>]
 **
 ** DESCRIPTION
 **   This function reads from the fifo PRODUCTION_LOG_FIFO any file name
 **   that has been renamed or where the content was changed by AFD. The
 **   data in the fifo has the following structure:
 **
 **       <ML><RR><UDN>|<DID>|<JID>|<OFN>|<NFL>[|<CMD>]\n
 **         |   |   |     |     |     |     |      |
 **         |   |   |     |     |     |     |      +-------> Command executed.
 **         |   |   |     |     |     |     +--------------> New filename.
 **         |   |   |     |     |     +--------------------> Original File Name.
 **         |   |   |     |     +--------------------------> Job ID.
 **         |   |   |     +--------------------------------> Directory ID.
 **         |   |   +--------------------------------------> Unique ID.
 **         |   +------------------------------------------> Ratio
 **         |                                                relationship.
 **         +----------------------------------------------> The length of this
 **                                                          message of type
 **                                                          unsigned short.
 **
 **   This data is then written to the production log file in the following
 **   format:
 **
 **     414d2c4b  1:1|414d2c49_2a_0|682d6409|d1fe5d48|dat|dat1.bz2|24fa43|0|bzip2 %s
 **        |       |         |          |       |      |     |        |   |   |
 **        |       +-+       |       +--+     +-+      |     +--+     |   +-+ +-----+
 **        |         |       |       |        |        |        |     |     |       |
 **     Date of    Ratio   Unique Directory  Job   Original    New   New  Return Command
 **    this entry relation   ID      ID      ID    filename filename size  code  executed
 **                ship
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.03.2001 H.Kiehl Created
 **   15.01.2008 H.Kiehl Added job ID, size and return code field.
 **   28.03.2008 H.Kiehl Added directory ID.
 **   28.10.2008 H.Kiehl Added ratio relationship.
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
FILE       *production_file = NULL;
int        sys_log_fd = STDERR_FILENO;
char       *iobuf = NULL,
           *p_work_dir = NULL;
const char *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void production_log_exit(void),
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
                  n,
                  max_production_log_files = MAX_PRODUCTION_LOG_FILES,
                  no_of_buffered_writes = 0,
                  status;
#ifdef WITHOUT_FIFO_RW_SUPPORT
   int            writefd;
#endif
   time_t         next_file_time,
                  now;
   long           fifo_size;
   char           *p_end,
                  *fifo_buffer,
                  *work_dir,
                  current_log_file[MAX_PATH_LENGTH],
                  log_file[MAX_PATH_LENGTH];
   unsigned short *msg_length;
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
   (void)strcat(log_file, PRODUCTION_LOG_FIFO);
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
    * Determine the size of the fifo buffer. Then create a buffer
    * large enough to hold the data from a fifo.
    */
   if ((fifo_size = fpathconf(log_fd, _PC_PIPE_BUF)) < 0)
   {
      /* If we cannot determine the size of the fifo set default value. */
      fifo_size = DEFAULT_FIFO_SIZE;
   }
   if (fifo_size < (sizeof(short) +
                    2 + MAX_INT_LENGTH + 6 + MAX_INT_LENGTH + 1 + 1 +
                    MAX_INT_LENGTH + 1 + (2 * MAX_FILENAME_LENGTH)))
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Fifo is NOT large enough to ensure atomic writes!");
      fifo_size = sizeof(short)+
                  2 + MAX_INT_LENGTH + 6 + MAX_INT_LENGTH + 1 + 1 +
                  MAX_INT_LENGTH + 1 + (2 * MAX_FILENAME_LENGTH);
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
   get_max_log_values(&max_production_log_files, MAX_PRODUCTION_LOG_FILES_DEF,
                      MAX_PRODUCTION_LOG_FILES, NULL, NULL, 0, AFD_CONFIG_FILE);

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
    * Lets open the production file name buffer. If it does not yet exists
    * create it.
    */
   get_log_number(&log_number, (max_production_log_files - 1),
                  PRODUCTION_BUFFER_FILE, PRODUCTION_BUFFER_FILE_LENGTH, NULL);
   (void)snprintf(current_log_file, MAX_PATH_LENGTH, "%s%s/%s0",
                  work_dir, LOG_DIR, PRODUCTION_BUFFER_FILE);
   p_end = log_file + snprintf(log_file, MAX_PATH_LENGTH, "%s%s/%s",
                               work_dir, LOG_DIR, PRODUCTION_BUFFER_FILE);

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
         if (log_number < (max_production_log_files - 1))
         {
            log_number++;
         }
         if (max_production_log_files > 1)
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
   production_file = open_log_file(current_log_file, NULL, NULL, NULL);
#else
   production_file = open_log_file(current_log_file);
#endif

#ifdef WITH_LOG_TYPE_DATA
   # Write log type data.
   (void)fprintf(production_file, "#!# %d %d\n",
                 LOG_DATE_LENGTH, MAX_HOSTNAME_LENGTH);
#endif /* WITH_LOG_TYPE_DATA */

   msg_length = (unsigned short *)fifo_buffer;
   check_size = 2 + MAX_INT_LENGTH + 6 + MAX_INT_LENGTH + 1 + 1 +
                MAX_INT_LENGTH + 1;

   /* Do some cleanups when we exit. */
   if (atexit(production_log_exit) != 0)
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
    * Now lets wait for data to be written to the production log.
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
            (void)fflush(production_file);
            no_of_buffered_writes = 0;
         }

         /* Check if we have to create a new log file. */
         if (time(&now) > next_file_time)
         {
            if (log_number < (max_production_log_files - 1))
            {
               log_number++;
            }
            if (fclose(production_file) == EOF)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "fclose() error : %s", strerror(errno));
            }
            production_file = NULL;
            if (max_production_log_files > 1)
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
            production_file = open_log_file(current_log_file, NULL, NULL, NULL);
#else
            production_file = open_log_file(current_log_file);
#endif

#ifdef WITH_LOG_TYPE_DATA
            # Write log type data.
            (void)fprintf(production_file, "#!# %d %d\n",
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
               * data to the production log. The data in the 
               * fifo always has the following format:
               *
               * <unique dir name>|<original filename>|<new filename 1>|...
               * <new filename n>\0
               */
              if (bytes_buffered >= fifo_size)
              {
                 system_log(DEBUG_SIGN, __FILE__, __LINE__,
                            "Hmmm, bytes_buffered (%d) >= fifo_size (%d). Must be reading garbage, discarding buffer.",
                            bytes_buffered, fifo_size);
                 bytes_buffered = 0;
              }
              if ((n = read(log_fd, &fifo_buffer[bytes_buffered],
                            fifo_size - bytes_buffered)) > 0)
              {
                 n += bytes_buffered;
                 bytes_buffered = 0;
                 do
                 {
                    if ((n < (check_size - 1)) || (n < *msg_length))
                    {
                       length = n;
                       bytes_buffered = n;
                    }
                    else
                    {
#if SIZEOF_TIME_T == 4
                       (void)fprintf(production_file, "%-*lx%s\n",
#else
                       (void)fprintf(production_file, "%-*llx%s\n",
#endif
                                     LOG_DATE_LENGTH, (pri_time_t)now,
                                     &fifo_buffer[sizeof(short)]);
                       length = *msg_length;
                    }
                    n -= length;
                    if (n > 0)
                    {
                       (void)memmove(fifo_buffer, &fifo_buffer[length], n);
                    }
                    no_of_buffered_writes++;
                 } while ((n > 0) && (length != 0));

                 if (no_of_buffered_writes > BUFFERED_WRITES_BEFORE_FLUSH_SLOW)
                 {
                    (void)fflush(production_file);
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
              if (time(&now) > next_file_time)
              {
                 if (log_number < (max_production_log_files - 1))
                 {
                    log_number++;
                 }
                 if (fclose(production_file) == EOF)
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "fclose() error : %s", strerror(errno));
                 }
                 production_file = NULL;
                 if (max_production_log_files > 1)
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
                 production_file = open_log_file(current_log_file, NULL, NULL, NULL);
#else
                 production_file = open_log_file(current_log_file);
#endif

#ifdef WITH_LOG_TYPE_DATA
                 # Write log type data.
                 (void)fprintf(production_file, "#!# %d %d\n",
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


/*++++++++++++++++++++++++ production_log_exit() ++++++++++++++++++++++++*/
static void
production_log_exit(void)
{
   if (production_file != NULL)
   {
      (void)fflush(production_file);
      if (fclose(production_file) == EOF)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "fclose() error : %s", strerror(errno));
      }
      production_file = NULL;
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
                 PRODUCTION_LOG_PROCESS, signo, (pri_pid_t)getpid());
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
