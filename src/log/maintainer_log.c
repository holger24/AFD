/*
 *  maintainer_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2014 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   maintainer_log - logs all system activities that are of interest
 **                    for the maintainer
 **
 ** SYNOPSIS
 **   maintainer_log [--version][-w <working directory>]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.11.2014 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                 /* fprintf(), fclose(), stderr,       */
                                   /* strerror()                         */
#include <string.h>                /* strcmp(), strcpy(), strcat()       */
#include <stdlib.h>                /* getenv(), exit(), abort(), malloc()*/
#include <unistd.h>                /* fpathconf(), STDERR_FILENO         */
#include <signal.h>                /* signal()                           */
#include <sys/types.h>
#include <sys/stat.h>              /* stat()                             */
#include <fcntl.h>                 /* O_RDWR, O_CREAT, O_WRONLY, open()  */
#include <errno.h>
#include "logdefs.h"
#include "version.h"

/* Global variables. */
int          bytes_buffered = 0,
             sys_log_fd = STDERR_FILENO;
unsigned int *p_log_counter = NULL, /* Disable writing to AFD status area. */
             total_length;
long         fifo_size;
char         *fifo_buffer,
             *msg_str,
             *prev_msg_str,
             *p_log_his = NULL,
             *p_log_fifo,
             *progname,
             *p_work_dir;
const char   *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void  sig_bus(int),
             sig_segv(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int          log_number = 0,
                log_stat = START,
                max_maintainer_log_files = MAX_MAINTAINER_LOG_FILES,
                maintainer_log_fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
   int          writefd;
#endif
   off_t        max_maintainer_logfile_size = MAX_MAINTAINER_LOGFILE_SIZE;
   char         *p_end = NULL,
                *work_dir,
                log_file[MAX_PATH_LENGTH],
                current_log_file[MAX_PATH_LENGTH];
   FILE         *p_log_file;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD. */
   if (get_afd_path(&argc, argv, log_file) < 0)
   {
      exit(INCORRECT);
   }
   progname = get_progname(argv[0]);
   if ((work_dir = malloc((strlen(log_file) + 1))) == NULL)
   {
      (void)fprintf(stderr,
                    "ERROR   : Failed to malloc() memory : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   (void)strcpy(work_dir, log_file);
   p_work_dir = work_dir;

   /* Initialise variables for fifo stuff. */
   (void)strcat(log_file, FIFO_DIR);
   (void)strcat(log_file, MAINTAINER_LOG_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(log_file, &maintainer_log_fd, &writefd) == -1)
#else
   if ((maintainer_log_fd = open(log_file, O_RDWR)) == -1)
#endif
   {
      (void)fprintf(stderr, "ERROR   : Could not open fifo %s : %s (%s %d)\n",
                    log_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if ((fifo_size = fpathconf(maintainer_log_fd, _PC_PIPE_BUF)) < 0)
   {
      /* If we cannot determine the size of the fifo set default value. */
      fifo_size = DEFAULT_FIFO_SIZE;
   }
   if (((fifo_buffer = malloc((size_t)fifo_size)) == NULL) ||
       ((msg_str = malloc((size_t)fifo_size)) == NULL) ||
       ((prev_msg_str = malloc((size_t)fifo_size)) == NULL))
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "malloc() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Initialise signal handlers. */
   if ((signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "signal() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Get the maximum number of logfiles we keep for history. */
   get_max_log_values(&max_maintainer_log_files, MAX_MAINTAINER_LOG_FILES_DEF,
                      MAX_MAINTAINER_LOG_FILES, &max_maintainer_logfile_size,
                      MAX_MAINTAINER_LOGFILE_SIZE_DEF,
                      MAX_MAINTAINER_LOGFILE_SIZE, AFD_CONFIG_FILE);

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

   get_log_number(&log_number, (max_maintainer_log_files - 1),
                  MAINTAINER_LOG_NAME, MAINTAINER_LOG_NAME_LENGTH, NULL);
   (void)snprintf(current_log_file, MAX_PATH_LENGTH, "%s%s/%s0",
                  p_work_dir, LOG_DIR, MAINTAINER_LOG_NAME);
   p_end = log_file + snprintf(log_file, MAX_PATH_LENGTH, "%s%s/%s",
                               p_work_dir, LOG_DIR, MAINTAINER_LOG_NAME);

   while (log_stat == START)
   {
#ifdef HAVE_STATX
      if (statx(0, current_log_file, AT_STATX_SYNC_AS_STAT,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (stat(current_log_file, &stat_buf) == -1)
#endif
      {
         /* The log file does not yet exist. */
         total_length = 0;
      }
      else
      {
         /* Check if we have to start a new log file */
         /* because the current one is large enough. */
#ifdef HAVE_STATX
         if (stat_buf.stx_size > max_maintainer_logfile_size)
#else
         if (stat_buf.st_size > max_maintainer_logfile_size)
#endif
         {
            if (log_number < (max_maintainer_log_files - 1))
            {
               log_number++;
            }
            if (max_maintainer_log_files > 1)
            {
               reshuffel_log_files(log_number, log_file, p_end, 0, 0);
            }
            else
            {
               if (unlink(current_log_file) == -1)
               {
                  (void)fprintf(stderr,
                                "WARNING : Failed to unlink() current log file `%s' : %s (%s %d)\n",
                                current_log_file, strerror(errno),
                                __FILE__, __LINE__);
               }
            }
            total_length = 0;
         }
         else
         {
#ifdef HAVE_STATX
            total_length = stat_buf.stx_size;
#else
            total_length = stat_buf.st_size;
#endif
         }
      }

      /* Open system log file for writing. */
      if ((p_log_file = fopen(current_log_file, "a+")) == NULL)
      {
         (void)fprintf(stderr, "ERROR   : Could not open %s : %s (%s %d)\n",
                       current_log_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }

      log_stat = logger(p_log_file, max_maintainer_logfile_size,
                        maintainer_log_fd, MAINTAINER_LOG_RESCAN_TIME);

      if (fclose(p_log_file) == EOF)
      {
         (void)rec(sys_log_fd, DEBUG_SIGN, "fclose() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
   }

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   (void)fprintf(stderr, "Aaarrrggh! Received SIGSEGV. (%s %d)\n",
             __FILE__, __LINE__);
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   (void)fprintf(stderr, "Uuurrrggh! Received SIGBUS. (%s %d)\n",
             __FILE__, __LINE__);
   abort();
}
