/*
 *  monitor_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2023 Deutscher Wetterdienst (DWD),
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
 **   monitor_log - logs all monitor activities of AFD_MON
 **
 ** SYNOPSIS
 **   monitor_log [--version][-w <working directory>]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   31.08.1998 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>           /* fopen(), fflush()                        */
#include <string.h>          /* strcpy(), strcat(), strerror(), memcpy() */
#include <stdlib.h>          /* malloc(), atexit()                       */
#include <time.h>            /* time()                                   */
#include <sys/types.h>       /* fdset                                    */
#include <sys/stat.h>
#include <sys/time.h>        /* struct timeval, time()                   */
#include <unistd.h>          /* fpathconf(), sysconf()                   */
#include <fcntl.h>           /* O_RDWR, open()                           */
#include <signal.h>          /* signal()                                 */
#include <errno.h>
#include "logdefs.h"
#include "mondefs.h"
#include "version.h"


/* External global variables. */
FILE                  *monitor_file = NULL;
int                   sys_log_fd = STDERR_FILENO;
char                  *iobuf = NULL,
                      *p_work_dir = NULL;
struct afd_mon_status *p_afd_mon_status;
const char            *sys_log_name = MON_SYS_LOG_FIFO;

/* Local function prototypes. */
static void           monitor_log_exit(void),
                      sig_exit(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int            bytes_buffered = 0,
                  count,
                  dup_msg = 0,
                  length,
                  log_number = 0,
                  log_pos,
                  max_mon_log_files = MAX_MON_LOG_FILES,
                  monitor_fd,
                  n,
                  no_of_buffered_writes = 0,
                  prev_length = 0,
                  status;
#ifdef WITHOUT_FIFO_RW_SUPPORT
   int            writefd;
#endif
   unsigned int   *p_log_counter;
   time_t         next_file_time,
                  now;
   long           fifo_size;
   char           *ptr,
                  *p_end,
                  *fifo_buffer,
                  *p_log_fifo,
                  *work_dir,
                  current_log_file[MAX_PATH_LENGTH],
                  log_file[MAX_PATH_LENGTH],
                  msg_str[MAX_LINE_LENGTH + 1],
                  prev_msg_str[MAX_LINE_LENGTH + 1];
   fd_set         rset;
   struct timeval timeout;
#ifdef HAVE_STATX
   struct statx   stat_buf;
#else
   struct stat    stat_buf;
#endif

   CHECK_FOR_VERSION(argc, argv);

   if (get_mon_path(&argc, argv, log_file) < 0)
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
   (void)strcat(log_file, MON_LOG_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(log_file, &monitor_fd, &writefd) == -1)
#else
   if ((monitor_fd = open(log_file, O_RDWR)) == -1)
#endif
   {
      if (errno == ENOENT)
      {
         if (make_fifo(log_file) == SUCCESS)
         {
#ifdef WITHOUT_FIFO_RW_SUPPORT
            if (open_fifo_rw(log_file, &monitor_fd, &writefd) == -1)
#else
            if ((monitor_fd = open(log_file, O_RDWR)) == -1)
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
    * Determine the size of the fifo buffer. Then create a buffer
    * large enough to hold the data from a fifo.
    */
   if ((fifo_size = fpathconf(monitor_fd, _PC_PIPE_BUF)) < 0)
   {
      /* If we cannot determine the size of the fifo set default value. */
      fifo_size = DEFAULT_FIFO_SIZE;
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
   get_max_log_values(&max_mon_log_files, MAX_MON_LOG_FILES_DEF,
                      MAX_MON_LOG_FILES, NULL, NULL, 0, MON_CONFIG_FILE);

   /* Attach to the AFD_MON Status Area and position pointers. */
   if (attach_afd_mon_status() < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to attach to AFD_MON status area.");
      exit(INCORRECT);
   }
   p_log_counter = (unsigned int *)&p_afd_mon_status->mon_log_ec;
   log_pos = p_afd_mon_status->mon_log_ec % LOG_FIFO_SIZE;
   p_log_fifo = &p_afd_mon_status->mon_log_fifo[0];

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
    * Lets open the monitor log file. If it does not yet exists
    * create it.
    */
   get_log_number(&log_number,
                  (max_mon_log_files - 1),
                  MON_LOG_NAME,
                  MON_LOG_NAME_LENGTH,
                  NULL);
   (void)snprintf(current_log_file, MAX_PATH_LENGTH, "%s%s/%s0",
                  work_dir, LOG_DIR, MON_LOG_NAME);
   p_end = log_file + snprintf(log_file, MAX_PATH_LENGTH, "%s%s/%s",
                               work_dir, LOG_DIR, MON_LOG_NAME);

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
         if (log_number < (max_mon_log_files - 1))
         {
            log_number++;
         }
         if (max_mon_log_files > 1)
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
   monitor_file = open_log_file(current_log_file, NULL, NULL, NULL);
#else
   monitor_file = open_log_file(current_log_file);
#endif

   /* Do some cleanups when we exit. */
   if (atexit(monitor_log_exit) != 0)
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
    * Now lets wait for data to be written to the monitor log.
    */
   FD_ZERO(&rset);
   for (;;)
   {
      /* Initialise descriptor set and timeout. */
      FD_SET(monitor_fd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = 1L;

      /* Wait for message x seconds and then continue. */
      status = select(monitor_fd + 1, &rset, NULL, NULL, &timeout);

      if (status == 0)
      {
         if (no_of_buffered_writes > 0)
         {
            (void)fflush(monitor_file);
            no_of_buffered_writes = 0;
         }

         /* Check if we have to create a new log file. */
         if (time(&now) > next_file_time)
         {
            if (log_number < (max_mon_log_files - 1))
            {
               log_number++;
            }
            if (fclose(monitor_file) == EOF)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "fclose() error : %s", strerror(errno));
            }
            monitor_file = NULL;
            if (max_mon_log_files > 1)
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
            monitor_file = open_log_file(current_log_file, NULL, NULL, NULL);
#else
            monitor_file = open_log_file(current_log_file);
#endif
            next_file_time = (now / SWITCH_FILE_TIME) * SWITCH_FILE_TIME +
                             SWITCH_FILE_TIME;
         }
      }
      else if (FD_ISSET(monitor_fd, &rset))
           {
              now = time(NULL);

              /*
               * Aaaah. Some new data has arrived. Lets write this
               * data to the monitor log. The data in the 
               * fifo has no special format.
               */
              ptr = fifo_buffer;
              if ((n = read(monitor_fd, &fifo_buffer[bytes_buffered],
                            fifo_size - bytes_buffered)) > 0)
              {
#ifdef _FIFO_DEBUG
                 show_fifo_data('R', MONITOR_LOG_FIFO, fifo_buffer, n + bytes_buffered, __FILE__, __LINE__);
#endif
                 if (bytes_buffered != 0)
                 {
                    n += bytes_buffered;
                    bytes_buffered = 0;
                 }

                 /* Now evaluate all data read from fifo, byte after byte. */
                 count = 0;
                 while (count < n)
                 {
                    length = 0;
                    while ((length < (MAX_LINE_LENGTH - 1)) &&
                           ((count + length) < n) &&
                           (*(ptr + length) != '\n') &&
                           (*(ptr + length) != '\0'))
                    {
                       msg_str[length] = *(ptr + length);
                       length++;
                    }
                    ptr += length;
                    count += length;
                    if (((count < n) && (*ptr == '\n')) ||
                        (length >= (MAX_LINE_LENGTH - 1)))
                    {
                       ptr++; count++;
                       msg_str[length++] = '\n';
                       msg_str[length] = '\0';

                       if (p_log_counter != NULL)
                       {
                          if (log_pos == LOG_FIFO_SIZE)
                          {
                             log_pos = 0;
                          }
                          switch (msg_str[LOG_SIGN_POSITION])
                          {
                             case 'I' : /* Info */
                                p_log_fifo[log_pos] = INFO_ID;
                                break;
                             case 'O' : /* Error/Warn Offline, NOT vissible!!! */
                                break;
                             case 'W' : /* Warn */
                                p_log_fifo[log_pos] = WARNING_ID;
                                break;
                             case 'C' : /* Config */
                                p_log_fifo[log_pos] = CONFIG_ID;
                                break;
                             case 'E' : /* Error */
                                p_log_fifo[log_pos] = ERROR_ID;
                                break;
                             case 'D' : /* Debug is NOT made vissible!!! */
                                break;
                             case 'F' : /* Faulty */
                                p_log_fifo[log_pos] = FAULTY_ID;
                                break;
                             default  : /* Unknown */
                                p_log_fifo[log_pos] = CHAR_BACKGROUND;
                                break;
                          }
                          if ((msg_str[LOG_SIGN_POSITION] != 'D') &&
                              (msg_str[LOG_SIGN_POSITION] != 'O'))
                          {
                             log_pos++;
                             (*p_log_counter)++;
                          }
                       }

                       if (length == prev_length)
                       {
                          if (CHECK_STRCMP(msg_str, prev_msg_str) == 0)
                          {
                             dup_msg++;
                          }
                          else
                          {
                             if (dup_msg > 0)
                             {
                                if (dup_msg == 1)
                                {
                                   (void)fprintf(monitor_file, "%s",
                                                 prev_msg_str);
                                }
                                else
                                {
                                   (void)fprint_dup_msg(monitor_file,
                                                        dup_msg,
                                                        &prev_msg_str[LOG_SIGN_POSITION - 1],
                                                        &prev_msg_str[LOG_SIGN_POSITION + 3],
                                                        MAX_AFDNAME_LENGTH,
                                                        now);
                                }
                                dup_msg = 0;
                             }

                             (void)fprintf(monitor_file, "%s", msg_str);
                             no_of_buffered_writes++;
                             if (no_of_buffered_writes > BUFFERED_WRITES_BEFORE_FLUSH_FAST)
                             {
                                (void)fflush(monitor_file);
                                no_of_buffered_writes = 0;
                             }
                             (void)strcpy(prev_msg_str, msg_str);
                             prev_length = length;
                          }
                       }
                       else
                       {
                          if (dup_msg > 0)
                          {
                             if (dup_msg == 1)
                             {
                                (void)fprintf(monitor_file, "%s",
                                              prev_msg_str);
                             }
                             else
                             {
                                (void)fprint_dup_msg(monitor_file,
                                                     dup_msg,
                                                     &prev_msg_str[LOG_SIGN_POSITION - 1],
                                                     &prev_msg_str[LOG_SIGN_POSITION + 3],
                                                     MAX_AFDNAME_LENGTH,
                                                     now);
                             }
                             dup_msg = 0;
                          }

                          (void)fprintf(monitor_file, "%s", msg_str);
                          no_of_buffered_writes++;
                          if (no_of_buffered_writes > BUFFERED_WRITES_BEFORE_FLUSH_FAST)
                          {
                             (void)fflush(monitor_file);
                             no_of_buffered_writes = 0;
                          }
                          (void)strcpy(prev_msg_str, msg_str);
                          prev_length = length;
                       }

                       if ((length - 1) >= (MAX_LINE_LENGTH - 1))
                       {
                          system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                     "Line to long, truncated it!");
                          while (((count + length) < n) &&
                                 (*(ptr + length) != '\n') &&
                                 (*(ptr + length) != '\0'))
                          {
                             length++;
                          }
                          ptr += length;
                          count += length;
                       }
                    }
                    else
                    {
                       (void)memcpy(fifo_buffer, msg_str, length);
                       bytes_buffered = length;
                       count = n;
                    }
                 } /* while (count < n) */
              }

              /*
               * Since we can receive a constant stream of data
               * on the fifo, we might never get that select() returns 0.
               * Thus we always have to check if it is time to create
               * a new log file.
               */
              if (now > next_file_time)
              {
                 if (log_number < (max_mon_log_files - 1))
                 {
                    log_number++;
                 }
                 if (fclose(monitor_file) == EOF)
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "fclose() error : %s", strerror(errno));
                 }
                 monitor_file = NULL;
                 if (max_mon_log_files > 1)
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
                 monitor_file = open_log_file(current_log_file, NULL, NULL, NULL);
#else
                 monitor_file = open_log_file(current_log_file);
#endif
                 next_file_time = (now / SWITCH_FILE_TIME) * SWITCH_FILE_TIME +
                                  SWITCH_FILE_TIME;
              }
           }
           else
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "select() error : %s", strerror(errno));
              exit(INCORRECT);
           }
   } /* for (;;) */

   /* Should never come to this point. */
   exit(SUCCESS);
}


/*++++++++++++++++++++++++++ monitor_log_exit() +++++++++++++++++++++++++*/
static void
monitor_log_exit(void)
{
   if (monitor_file != NULL)
   {
      (void)fflush(monitor_file);
      if (fclose(monitor_file) == EOF)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "fclose() error : %s", strerror(errno));
      }
      monitor_file = NULL;
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
                 MONITOR_LOG, signo, (pri_pid_t)getpid());
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
