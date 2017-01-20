/*
 *  transfer_rate_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2017 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   transfer_rate_log - logs all transfer rate activity of the AFD
 **
 ** SYNOPSIS
 **   transfer_rate_log [--version][-w <working directory>]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.01.2017 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>           /* fopen(), fflush()                        */
#include <string.h>          /* strcpy(), strcat(), strerror(), memcpy() */
#include <stdlib.h>          /* malloc()                                 */
#include <time.h>            /* time()                                   */
#include <sys/types.h>       /* fdset                                    */
#include <sys/stat.h>
#include <sys/time.h>        /* struct timeval, time()                   */
#include <unistd.h>          /* fpathconf(), sysconf()                   */
#include <fcntl.h>           /* O_RDWR, open()                           */
#include <signal.h>          /* signal()                                 */
#include <errno.h>
#include "logdefs.h"
#include "version.h"

struct prev_rate
       {
          u_off_t      bytes_send;
          unsigned int host_id;
       };

/* External global variables. */
int                        fsa_fd = -1,
                           fsa_id,
                           no_of_hosts = 0,
                           sys_log_fd = STDERR_FILENO;
#ifdef HAVE_MMAP
off_t                      fsa_size;
#endif
char                       *iobuf = NULL,
                           *p_work_dir = NULL;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;
struct filetransfer_status *fsa;

/* Local global variables. */
static struct prev_rate    *pr = NULL;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   FILE        *transfer_rate_file;
   int         gotcha,
               i,
               j,
               log_number = 0,
               max_transfer_rate_log_files = MAX_TRANSFER_RATE_LOG_FILES,
               old_no_of_hosts,
               sleep_time;
   time_t      next_check_time,
               next_file_time,
               now,
               prev_time,
               time_elapsed;
   u_off_t     bytes_send,
               rate,
               tmp_bytes_send,
               total_bytes_send;
   char        current_log_file[MAX_PATH_LENGTH],
               log_file[MAX_PATH_LENGTH],
               *p_end,
               time_str[12],
               *work_dir;
   struct tm   *p_ts;
   struct stat stat_buf;

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

   /* Get the maximum number of logfiles we keep for history. */
   get_max_log_values(&max_transfer_rate_log_files, MAX_TRANSFER_LOG_FILES_DEF,
                      MAX_TRANSFER_LOG_FILES, NULL, NULL, 0);

   /* Attach to the FSA. */
   if (fsa_attach_passive(NO, TRLOG) != SUCCESS)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__, _("Failed to attach to FSA."));
      exit(INCORRECT);
   }

   /* Allocate memory to cache previous values. */
   if ((pr = malloc((no_of_hosts * sizeof(struct prev_rate)))) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__, _("malloc() error"));
      exit(INCORRECT);
   }
   for (i = 0; i < no_of_hosts; i++)
   {
      pr[i].host_id = fsa[i].host_id;
      pr[i].bytes_send = fsa[i].bytes_send;
   }

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
    * Lets open the transfer rate log file. If it does not yet exists
    * create it.
    */
   get_log_number(&log_number,
                  (max_transfer_rate_log_files - 1),
                  TRANSFER_RATE_LOG_NAME,
                  TRANSFER_RATE_LOG_NAME_LENGTH,
                  NULL);
   (void)snprintf(current_log_file, MAX_PATH_LENGTH, "%s%s/%s0",
                  work_dir, LOG_DIR, TRANSFER_RATE_LOG_NAME);
   p_end = log_file + snprintf(log_file, MAX_PATH_LENGTH, "%s%s/%s",
                               work_dir, LOG_DIR, TRANSFER_RATE_LOG_NAME);

   /* Calculate time when we have to start a new file. */
   next_file_time = (time(&now) / SWITCH_FILE_TIME) * SWITCH_FILE_TIME +
                    SWITCH_FILE_TIME;

   /* Calculate time when to log the transfer rate. */
   next_check_time = (now / TRANSFER_RATE_LOG_INTERVAL) *
                     TRANSFER_RATE_LOG_INTERVAL +
                     TRANSFER_RATE_LOG_INTERVAL - 1;

   /* Is current log file already too old? */
   if (stat(current_log_file, &stat_buf) == 0)
   {
      if (stat_buf.st_mtime < (next_file_time - SWITCH_FILE_TIME))
      {
         if (log_number < (max_transfer_rate_log_files - 1))
         {
            log_number++;
         }
         if (max_transfer_rate_log_files > 1)
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
   transfer_rate_file = open_log_file(current_log_file, NULL, NULL, NULL);
#else
   transfer_rate_file = open_log_file(current_log_file);
#endif

   /* Ignore any SIGTERM + SIGHUP signal. */
   if ((signal(SIGTERM, SIG_IGN) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "signal() error : %s", strerror(errno));
   }

   time_str[2]  = ' ';
   time_str[5]  = ':';
   time_str[8]  = ':';
   time_str[11] = '\0';

   /*
    * Now lets wait for data to be written to the transfer log.
    */
   for (;;)
   {
      /* Check if we have to create a new log file. */
      if (time(&now) > next_file_time)
      {
         if (log_number < (max_transfer_rate_log_files - 1))
         {
            log_number++;
         }
         if (fclose(transfer_rate_file) == EOF)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "fclose() error : %s", strerror(errno));
         }
         if (max_transfer_rate_log_files > 1)
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
         transfer_rate_file = open_log_file(current_log_file, NULL, NULL, NULL);
#else
         transfer_rate_file = open_log_file(current_log_file);
#endif
         next_file_time = (now / SWITCH_FILE_TIME) * SWITCH_FILE_TIME +
                          SWITCH_FILE_TIME;
         sleep_time = next_file_time - now;
         if (sleep_time < 0)
         {
            sleep_time = 0L;
         }
      }
      else
      {
         sleep_time = 0L;
      }
      if (now > next_check_time)
      {
         old_no_of_hosts = no_of_hosts;
         if (check_fsa(YES, TRLOG) == YES)
         {
            struct prev_rate *new_pr;

            if ((new_pr = malloc((no_of_hosts * sizeof(struct prev_rate)))) == NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__, _("malloc() error"));
               exit(INCORRECT);
            }

            for (i = 0; i < no_of_hosts; i++)
            {
               new_pr[i].host_id = fsa[i].host_id;
               gotcha = NO;
               for (j = i; j < old_no_of_hosts; j++)
               {
                  if (new_pr[i].host_id == pr[j].host_id)
                  {
                     new_pr[i].host_id =  pr[j].bytes_send;
                     gotcha = YES;
                     break;
                  }
               }
               if (gotcha == NO)
               {
                  for (j = 0; j < i; j++)
                  {
                     if (new_pr[i].host_id == pr[j].host_id)
                     {
                        new_pr[i].host_id =  pr[j].bytes_send;
                        gotcha = YES;
                        break;
                     }
                  }
                  if (gotcha == NO)
                  {
                     new_pr[i].bytes_send = fsa[i].bytes_send;
                  }
               }
            }
            free(pr);
            pr = new_pr;
         }
         if (fsa != NULL)
         {
            time_elapsed = now - prev_time;
            if (time_elapsed > 0)
            {
               p_ts         = localtime(&now);
               time_str[0]  = (p_ts->tm_mday / 10) + '0';
               time_str[1]  = (p_ts->tm_mday % 10) + '0';
               time_str[3]  = (p_ts->tm_hour / 10) + '0';
               time_str[4]  = (p_ts->tm_hour % 10) + '0';
               time_str[6]  = (p_ts->tm_min / 10) + '0';
               time_str[7]  = (p_ts->tm_min % 10) + '0';
               time_str[9]  = (p_ts->tm_sec / 10) + '0';
               time_str[10] = (p_ts->tm_sec % 10) + '0';
               total_bytes_send = 0;
               for (i = 0; i < no_of_hosts; i++)
               {
                  bytes_send = fsa[i].bytes_send;
                  if (bytes_send > pr[i].bytes_send)
                  {
                     tmp_bytes_send = bytes_send - pr[i].bytes_send;
                     rate = tmp_bytes_send / time_elapsed;
                     (void)fprintf(transfer_rate_file, "%s %s %llu\n",
                                   time_str, fsa[i].host_dsp_name, rate);
                     total_bytes_send += tmp_bytes_send;
                  }
                  pr[i].bytes_send = bytes_send;
               }
               if (total_bytes_send > 0)
               {
                  rate = total_bytes_send / time_elapsed;
                  (void)fprintf(transfer_rate_file, "%s **** %llu\n",
                                time_str, rate);
                  (void)fflush(transfer_rate_file);
               }
            }
            prev_time = now;
         }
         next_check_time = (now / TRANSFER_RATE_LOG_INTERVAL) *
                           TRANSFER_RATE_LOG_INTERVAL +
                           TRANSFER_RATE_LOG_INTERVAL - 1;
         if (sleep_time == 0L)
         {
            sleep_time = next_check_time - now;
            if (sleep_time < 0)
            {
               sleep_time = 0L;
            }
         }
         else
         {
            if ((sleep_time > (next_check_time - now)) &&
                ((next_check_time - now) > 0))
            {
               sleep_time = next_check_time - now;
            }

         }
      }
      (void)sleep((unsigned int)sleep_time);
   } /* for (;;) */

   /* Should never come to this point. */
   exit(SUCCESS);
}
