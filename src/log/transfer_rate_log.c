/*
 *  transfer_rate_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2017 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
#include <string.h>          /* strcpy(), strcat(), strerror()           */
#include <stdlib.h>          /* malloc(), atexit()                       */
#include <time.h>            /* time()                                   */
#include <sys/types.h>       /* fdset                                    */
#ifdef HAVE_STATX
# include <fcntl.h>          /* Definition of AT_* constants             */
#endif
#include <sys/stat.h>
#include <sys/time.h>        /* struct timeval, time()                   */
#include <unistd.h>          /* select()                                 */
#include <signal.h>          /* signal()                                 */
#include <errno.h>
#include "logdefs.h"
#include "version.h"

struct prev_rate
       {
          u_off_t      bytes_send;
          unsigned int host_id;
       };
#ifdef WITH_IP_DB
struct prev_rate_ip
       {
          u_off_t      bytes_send;
          u_off_t      tmp_bytes_send;
          char         ip_str[MAX_AFD_INET_ADDRSTRLEN];
       };
#endif

/* External global variables. */
FILE                       *transfer_rate_file = NULL;
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
static int                 *fsa_ip_pos;
static struct prev_rate    *pr = NULL;
#ifdef WITH_IP_DB
static struct prev_rate_ip *prip = NULL;

/* Local function prototypes. */
static void                 get_ip_data(int *);
#endif
static void                 sig_exit(int),
                            transfer_rate_log_exit(void);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int            do_flush,
#ifdef WITH_IP_DB
                  fsa_ip_counter,
                  prev_no_of_hosts,
#endif
                  gotcha,
                  i,
                  j,
                  log_number = 0,
                  max_transfer_rate_log_files = MAX_TRANSFER_RATE_LOG_FILES,
                  old_no_of_hosts,
                  status;
   time_t         next_file_time,
                  now,
                  prev_time,
                  time_elapsed;
   u_off_t        bytes_send,
                  rate,
                  tmp_bytes_send;
   char           current_log_file[MAX_PATH_LENGTH],
                  log_file[MAX_PATH_LENGTH],
                  *p_end,
                  *work_dir;
#ifdef HAVE_STATX
   struct statx   stat_buf;
#else
   struct stat    stat_buf;
#endif
   struct timeval timeout;

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
   get_max_log_values(&max_transfer_rate_log_files,
                      MAX_TRANSFER_RATE_LOG_FILES_DEF,
                      MAX_TRANSFER_RATE_LOG_FILES,
                      NULL, NULL, 0, AFD_CONFIG_FILE);

   /* Attach to the FSA. */
   if (fsa_attach_passive(NO, TRLOG) != SUCCESS)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__, _("Failed to attach to FSA."));
      exit(INCORRECT);
   }
#ifdef WITH_IP_DB
   prev_no_of_hosts = no_of_hosts;
#endif

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
#ifdef WITH_IP_DB
   if ((fsa_ip_pos = malloc((no_of_hosts * sizeof(int)))) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__, _("malloc() error"));
      exit(INCORRECT);
   }
   get_ip_data(&fsa_ip_counter);
#endif /* WITH_IP_DB */

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
   prev_time = now;

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

   /* Write information usefull for evaluating this file. */
   (void)fprintf(transfer_rate_file,
#if SIZEOF_TIME_T == 4
                 "*|%lx|Start|interval=%d|version=%d\n",
#else
                 "*|%llx|Start|interval=%d|version=%d\n",
#endif
                 (pri_time_t)time(NULL), TRANSFER_RATE_LOG_INTERVAL,
                 TRANSFER_RATE_LOG_VERSION);
   (void)fflush(transfer_rate_file);


   /* Do some cleanups when we exit. */
   if (atexit(transfer_rate_log_exit) != 0)
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
         transfer_rate_file = NULL;
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

         /* Write information usefull for evaluating this file. */
         (void)fprintf(transfer_rate_file,
#if SIZEOF_TIME_T == 4
                       "*|%lx|Reshuffel|interval=%d\n",
#else
                       "*|%llx|Reshuffel|interval=%d\n",
#endif
                       (pri_time_t)now, TRANSFER_RATE_LOG_INTERVAL);
         (void)fflush(transfer_rate_file);
         next_file_time = (now / SWITCH_FILE_TIME) * SWITCH_FILE_TIME +
                          SWITCH_FILE_TIME;
      }

      timeout.tv_sec = ((now / TRANSFER_RATE_LOG_INTERVAL) *
                        TRANSFER_RATE_LOG_INTERVAL) +
                       TRANSFER_RATE_LOG_INTERVAL - now;
      if (timeout.tv_sec == 0)
      {
         timeout.tv_usec = 50000L;
      }
      else
      {
         timeout.tv_usec = 0;
      }

      if ((status = select(0, (fd_set *)0, (fd_set *)0, (fd_set *)0, &timeout)) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("select() error : %s"), strerror(errno));
         exit(INCORRECT);
      }
      if (status == 0)
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
#ifdef WITH_IP_DB
            if (no_of_hosts != prev_no_of_hosts)
            {
               if ((fsa_ip_pos = realloc(fsa_ip_pos,
                                         (no_of_hosts * sizeof(int)))) == NULL)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("realloc() error"));
                  exit(INCORRECT);
               }
               prev_no_of_hosts = no_of_hosts;
            }

            /*
             * Instead of finding out what has changed, just lets redo
             * the complete structure with the IP information.
             */
            get_ip_data(&fsa_ip_counter);
#endif
         }
         if (fsa != NULL)
         {
            time_elapsed = now - prev_time;
            if (time_elapsed > 0)
            {
               do_flush = NO;
#ifdef WITH_IP_DB
               for (i = 0; i < fsa_ip_counter; i++)
               {
                  prip[i].tmp_bytes_send = 0;
               }
#endif
               for (i = 0; i < no_of_hosts; i++)
               {
                  bytes_send = fsa[i].bytes_send;
                  if (bytes_send > pr[i].bytes_send)
                  {
                     tmp_bytes_send = bytes_send - pr[i].bytes_send;
                     rate = tmp_bytes_send / time_elapsed;
                     (void)fprintf(transfer_rate_file,
#if SIZEOF_TIME_T == 4
                                   "%lx|A|%s|%llu\n",
#else
                                   "%llx|A|%s|%llu\n",
#endif
                                   (pri_time_t)now, fsa[i].host_dsp_name, rate);
                     do_flush = YES;
                  }
                  pr[i].bytes_send = bytes_send;
#ifdef WITH_IP_DB
                  if ((fsa_ip_counter > 0) && (fsa_ip_pos[i] != -1))
                  {
                     prip[fsa_ip_pos[i]].tmp_bytes_send += bytes_send;
                  }
#endif
               }
#ifdef WITH_IP_DB
               for (i = 0; i < fsa_ip_counter; i++)
               {
                  if (prip[i].tmp_bytes_send > prip[i].bytes_send)
                  {
                     tmp_bytes_send = prip[i].tmp_bytes_send - prip[i].bytes_send;
                     rate = tmp_bytes_send / time_elapsed;
                     (void)fprintf(transfer_rate_file,
# if SIZEOF_TIME_T == 4
                                   "%lx|I|%s|%llu\n",
# else
                                   "%llx|I|%s|%llu\n",
# endif
                                   (pri_time_t)now, prip[i].ip_str, rate);
                     do_flush = YES;
                  }
                  prip[i].bytes_send = prip[i].tmp_bytes_send;
               }
#endif /* WITH_IP_DB */
               if (do_flush == YES)
               {
                  (void)fflush(transfer_rate_file);
               }
            }
            prev_time = now;
         }
      }
   } /* for (;;) */

   /* Should never come to this point. */
   exit(SUCCESS);
}


#ifdef WITH_IP_DB
/*++++++++++++++++++++++++++++ get_ip_data() ++++++++++++++++++++++++++++*/
static void
get_ip_data(int *fsa_ip_counter)
{
   int  i,
        j,
        no_of_ips;
   char *ip_hl = NULL,
        *ip_ips = NULL;

   if (prip != NULL)
   {
      free(prip);
      prip = NULL;
   }
   *fsa_ip_counter = 0;

   if ((no_of_ips = get_current_ip_hl(&ip_hl, &ip_ips)) > 0)
   {
      int  gotcha,
           k;
      char *p_ip_hl,
           *p_ip_ips;

      if ((prip = malloc((no_of_hosts * sizeof(struct prev_rate_ip)))) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__, _("malloc() error"));
         exit(INCORRECT);
      }
      for (i = 0; i < no_of_hosts; i++)
      {
         p_ip_hl = ip_hl;
         p_ip_ips = ip_ips;
         for (j = 0; j < no_of_ips; j++)
         {
            fsa_ip_pos[i] = -1;
            if ((my_strcmp(fsa[i].real_hostname[0], p_ip_hl) == 0) ||
                ((fsa[i].real_hostname[1][0] != '\0') &&
                 (my_strcmp(fsa[i].real_hostname[1], p_ip_hl) == 0)))
            {
               gotcha = NO;
               for (k = 0; k < *fsa_ip_counter; k++)
               {
                  if (my_strcmp(p_ip_ips, prip[k].ip_str) == 0)
                  {
                     gotcha = YES;
                     break;
                  }
               }
               if (gotcha == YES)
               {
                  fsa_ip_pos[i] = k;
                  prip[k].bytes_send += fsa[i].bytes_send;
               }
               else
               {
                  if ((*fsa_ip_counter % 10) == 0)
                  {
                     size_t new_size = ((*fsa_ip_counter / 10) + 1) * 10 *
                                       sizeof(struct prev_rate_ip);

                     if ((prip = realloc(prip, new_size)) == NULL)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   _("realloc() error"));
                        exit(INCORRECT);
                     }
                  }
                  fsa_ip_pos[i] = *fsa_ip_counter;
                  (void)strcpy(prip[*fsa_ip_counter].ip_str, p_ip_ips);
                  prip[*fsa_ip_counter].bytes_send = fsa[i].bytes_send;
                  prip[*fsa_ip_counter].tmp_bytes_send = 0;
                  (*fsa_ip_counter)++;
               }
               break;
            }
            p_ip_hl += MAX_REAL_HOSTNAME_LENGTH;
            p_ip_ips += MAX_AFD_INET_ADDRSTRLEN;
         }
      }
   }
   free(ip_hl);
   ip_hl = NULL;
   free(ip_ips);
   ip_ips = NULL;

   return;
}
#endif /* WITH_IP_DB */


/*+++++++++++++++++++++++ transfer_rate_log_exit() ++++++++++++++++++++++*/
static void
transfer_rate_log_exit(void)
{
   if (transfer_rate_file != NULL)
   {
      (void)fprintf(transfer_rate_file,
#if SIZEOF_TIME_T == 4
                    "*|%lx|Stop\n", (pri_time_t)time(NULL));
#else
                    "*|%llx|Stop\n", (pri_time_t)time(NULL));
#endif
      (void)fflush(transfer_rate_file);
      if (fclose(transfer_rate_file) == EOF)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "fclose() error : %s", strerror(errno));
      }
      transfer_rate_file = NULL;
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
                 TRLOG, signo, (pri_pid_t)getpid());
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
