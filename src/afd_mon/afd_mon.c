/*
 *  afd_mon.c - Part of AFD, an automatic file distribution program.
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
 **   afd_mon - monitors remote AFD's
 **
 ** SYNOPSIS
 **   afd_mon [--version] [-w <working directory>] [-nd] [-sn <name>]
 **        --version      Prints current version and copyright
 **        -w <work dir>  Working directory of the AFD.
 **        -C             Start with all checks done by cmdline mafd.
 **        -nd            Do not start as daemon process.
 **        -sn <name>     Provide a service name.
 **
 ** DESCRIPTION
 **   The AFD (Automatic File Distributor) monitor, checks and controls
 **   the activity of remote AFD's.
 **
 **   For this it needs to contact the AFDD (AFD Daemon) on a regular
 **   interval, which can be specified in the AFD_MON_DB file for each
 **   host separatly. This file is a normal ASCII Text file which is
 **   constantly checked for any changes. Thus it is not neccessary to
 **   always restart the afd_mon in order for the change to take effect.
 **
 **   The following parametes are read from the AFD_MON_DB file:
 **      AFD alias name       - the alias name of the remote AFD.
 **      host name            - the true hostname where the AFD is
 **                             active.
 **      port                 - the port number of the AFDD.
 **      interval             - the interval (in seconds) at which
 **                             the remote AFDD is contacted.
 **      command              - the command to execute remote programs
 **                             where the AFD is active.
 **
 **   For each remote AFD specified in the AFD_MON_DB file, afd_mon
 **   will monitor the following parameters:
 **      - status of the AMG
 **      - status of the FD
 **      - status of the archive_watch
 **      - number of jobs in the queue
 **      - number of active transfers
 **      - status of what is in the system
 **      - number of hosts that are red
 **      - number of files send in this interval
 **      - number of bytes still to be send
 **      - number of errors
 **      - AFD version
 **      - AFD working directory
 **
 **   To view these activities there is an X interface called mon_ctrl.
 **
 **   NOTE: Calculation of the week number (current_week, new_week), was
 **         taken from glibc-20060511T1325 (time/strftime_l.c).
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.09.1997 H.Kiehl Created
 **   08.06.2005 H.Kiehl Added afd_mon_status structure.
 **   14.02.2015 H.Kiehl In system log show total no_of_hosts, no_of_dirs
 **                      and no_of_jobs.
 **   01.08.2020 H.Kiehl Added support for systemd.
 **   22.04.2023 H.Kiehl Add support for aldad.
 **
 */
DESCR__E_M1

#include <stdio.h>            /* fprintf()                               */
#include <string.h>           /* strcpy(), strcat(), strerror()          */
#include <stdlib.h>           /* atexit(), exit(), atoi()                */
#include <signal.h>           /* kill(), signal()                        */
#include <ctype.h>            /* isdigit()                               */
#include <time.h>             /* time()                                  */
#include <sys/types.h>
#ifdef WITH_SYSTEMD
# include <systemd/sd-daemon.h>  /* sd_notify()                          */
#endif
#include <sys/wait.h>         /* waitpid()                               */
#include <sys/time.h>         /* struct timeval                          */
#ifdef HAVE_MMAP
# include <sys/mman.h>        /* mmap()                                  */
#endif
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <unistd.h>           /* lseek(), write(), exit(), getpid(),     */
                              /* fpathconf()                             */
#include <errno.h>
#include "mondefs.h"
#include "sumdefs.h"
#include "version.h"


/* Global variables. */
int                    daemon_log_fd = -1,
                       got_shuttdown_message = NO,
                       in_child = NO,
                       mon_cmd_fd,
                       mon_log_fd = STDERR_FILENO,
                       mon_resp_fd = -1,
                       msa_fd = -1,
                       msa_id,
                       no_of_afds,
                       probe_only_fd,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                       mon_cmd_writefd,
                       mon_log_readfd,
                       mon_resp_readfd,
                       probe_only_readfd,
#endif
                       sleep_sys_log_fd = -1,
                       started_as_daemon,
#ifdef WITH_SYSTEMD
                       systemd_watchdog_enabled = 0,
#endif
                       sys_log_fd = STDERR_FILENO,
                       timeout_flag;
long                   tcp_timeout = 120L;     /* not used (mon_log()) */
off_t                  msa_size;
size_t                 proc_list_size;
pid_t                  aldad_pid,
                       mon_log_pid,
                       own_pid,
                       sys_log_pid;
time_t                 afd_mon_db_time;
char                   afd_mon_db_file[MAX_PATH_LENGTH],
                       afd_mon_status_file[MAX_PATH_LENGTH],
                       mon_active_file[MAX_PATH_LENGTH],
                       mon_cmd_fifo[MAX_PATH_LENGTH],
                       *p_mon_alias,
                       probe_only_fifo[MAX_PATH_LENGTH],
                       *p_work_dir,
                       *service_name;
struct afd_mon_status  *p_afd_mon_status = NULL;
struct mon_status_area *msa;
struct process_list    *pl = NULL;
const char             *sys_log_name = MON_SYS_LOG_FIFO;

/* Local function prototypes. */
static void            afd_mon_exit(void),
                       eval_cmd_buffer(char *, int, int *, int *),
                       get_sum_data(int),
                       mon_active(void),
#ifdef NEW_MSA
                       print_data(int, int, unsigned int, double,
                                  unsigned int, double, unsigned int,
                                  unsigned int, double),
#else
                       print_data(int, int, unsigned int, u_off_t,
                                  unsigned int, u_off_t, unsigned int,
                                  unsigned int, u_off_t),
#endif
                       sig_bus(int),
                       sig_exit(int),
                       sig_segv(int),
                       start_afdmon(int *),
                       usage(char *),
                       zombie_check(time_t);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int            bytes_buffered,
                  current_month,
                  current_week,
                  current_year,
                  group_elements,
                  i,
                  new_day = NO,
                  new_month,
                  new_week,
                  new_year,
                  startup_with_check,
                  status;
   unsigned int   new_total_no_of_hosts,
                  new_total_no_of_dirs,
                  new_total_no_of_jobs,
                  total_no_of_hosts = 0,
                  total_no_of_dirs = 0,
                  total_no_of_jobs = 0;
   time_t         afd_mon_db_check_time,
                  new_day_sum_time,
                  new_hour_sum_time,
                  now;
   size_t         fifo_size;
   char           *fifo_buffer,
                  *ptr,
                  work_dir[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx   stat_buf;
#else
   struct stat    stat_buf;
#endif
   struct timeval timeout;
   struct tm      *p_ts;
   fd_set         rset;

   CHECK_FOR_VERSION(argc, argv);
   if ((get_arg(&argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      usage(argv[0]);
      exit(SUCCESS);
   }

   if (get_mon_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if (get_arg(&argc, argv, "-C", NULL, 0) == SUCCESS)
   {
      startup_with_check = YES;
   }
   else
   {
      startup_with_check = NO;
   }
   if (get_argb(&argc, argv, "-sn", &service_name) != SUCCESS)
   {
      service_name = NULL;
   }

   /* Check if this directory does exists, if not create it. */
   if (check_dir(work_dir, R_OK | W_OK | X_OK) < 0)
   {
      exit(INCORRECT);
   }

   (void)umask(0);

   /* Now check if the log directory has been created. */
   ptr = p_work_dir + strlen(p_work_dir);
   (void)strcpy(ptr, LOG_DIR);
   if (check_dir(p_work_dir, R_OK | W_OK | X_OK) < 0)
   {
      exit(INCORRECT);
   }
   (void)strcpy(ptr, RLOG_DIR);
   if (check_dir(p_work_dir, R_OK | W_OK | X_OK) < 0)
   {
      exit(INCORRECT);
   }
   *ptr = '\0';

   /* Initialise variables. */
   (void)strcpy(afd_mon_status_file, work_dir);
   (void)strcat(afd_mon_status_file, FIFO_DIR);
   (void)strcpy(mon_active_file, afd_mon_status_file);
   (void)strcat(mon_active_file, MON_ACTIVE_FILE);
   (void)strcat(afd_mon_status_file, AFD_MON_STATUS_FILE);
   (void)strcpy(afd_mon_db_file, work_dir);
   (void)strcat(afd_mon_db_file, ETC_DIR);
   (void)strcat(afd_mon_db_file, AFD_MON_CONFIG_FILE);

   /* Determine the size of the fifo buffer and allocate buffer. */
   if ((i = (int)fpathconf(mon_cmd_fd, _PC_PIPE_BUF)) < 0)
   {
      /* If we cannot determine the size of the fifo set default value. */
      fifo_size = DEFAULT_FIFO_SIZE;
   }
   else
   {
      fifo_size = i;
   }
   if ((fifo_buffer = malloc(fifo_size)) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error [%d bytes] : %s",
                 fifo_size, strerror(errno));
   }

   if (startup_with_check == YES)
   {
      char auto_block_file[MAX_PATH_LENGTH],
           sys_log_fifo[MAX_PATH_LENGTH];

      (void)strcpy(auto_block_file, work_dir);
      (void)strcat(auto_block_file, ETC_DIR);
      (void)strcat(auto_block_file, AFDMON_BLOCK_FILE);

      /* Check if starting of AFD is currently disabled.  */
      if (eaccess(auto_block_file, F_OK) == 0)
      {
         (void)fprintf(stderr,
                       _("AFD_MON is currently disabled by system manager.\n"));
         exit(AFD_DISABLED_BY_SYSADM);
      }

      (void)strcpy(sys_log_fifo, work_dir);
      (void)strcat(sys_log_fifo, FIFO_DIR);
      if (check_dir(sys_log_fifo, R_OK | W_OK | X_OK) < 0)
      {
         exit(INCORRECT);
      }
      (void)strcat(sys_log_fifo, MON_SYS_LOG_FIFO);
#ifdef HAVE_STATX
      if ((statx(0, sys_log_fifo, AT_STATX_SYNC_AS_STAT,
                 STATX_MODE, &stat_buf) == -1) ||
          (!S_ISFIFO(stat_buf.stx_mode)))
#else
      if ((stat(sys_log_fifo, &stat_buf) == -1) ||
          (!S_ISFIFO(stat_buf.st_mode)))
#endif
      {
         if (make_fifo(sys_log_fifo) < 0)
         {
            (void)fprintf(stderr,
                          "ERROR   : Could not create fifo %s. (%s %d)\n",
                          sys_log_fifo, __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }

      if (check_afdmon_database() == -1)
      {
         (void)fprintf(stderr,
                       _("ERROR   : Cannot read AFD_MON_CONFIG file : %s\nUnable to start AFD_MON.\n"),
                       strerror(errno));
         exit(INCORRECT);
      }
   }

   if (init_fifos_mon() == INCORRECT)
   {
      (void)fprintf(stderr,
                    "ERROR   : Failed to initialize fifos. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Make sure that no other afd_monitor is running in this directory. */
   if (((status = check_mon(10L)) == ACKN) || (status == ACKN_STOPPED))
   {
      (void)fprintf(stderr, "Another %s is active, terminating.\n", AFD_MON);
      exit(0);
   }

   /* Do some cleanups when we exit. */
   if (atexit(afd_mon_exit) != 0)
   {
      (void)fprintf(stderr,
                    "Could not register exit handler : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGPIPE, SIG_IGN) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not set signal handlers : %s", strerror(errno));
      exit(INCORRECT);
   }

   /*
    * Read AFD_MON_DB file and create MSA (Monitor Status Area).
    */
#ifdef HAVE_STATX
   if (statx(0, afd_mon_db_file, AT_STATX_SYNC_AS_STAT,
             STATX_MTIME, &stat_buf) == -1)
#else
   if (stat(afd_mon_db_file, &stat_buf) == -1)
#endif
   {
      (void)fprintf(stderr,
                    "ERROR   : Could not access %s : %s (%s %d)\n",
                    afd_mon_db_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef HAVE_STATX
   afd_mon_db_time = stat_buf.stx_mtime.tv_sec;
#else
   afd_mon_db_time = stat_buf.st_mtime;
#endif
   create_msa();

   /* Check if -nd is provided. */
   if ((argc == 2) && (argv[1][0] == '-') && (argv[1][1] == 'n') &&
       (argv[1][2] == 'd'))
   {
      char   *buffer = NULL;
      size_t length;

      /* DO NOT START AS DAEMON!!! */;
      started_as_daemon = NO;

      if (service_name == NULL)
      {
         length = 35 + AFD_LENGTH;
      }
      else
      {
         length = 40 + AFD_LENGTH + strlen(service_name);
      }
      if ((buffer = malloc(length + 1)) != NULL)
      {
         (void)memset(buffer, '=', length);
         buffer[length] = '\0';
         now = time(NULL);
         if (service_name == NULL)
         {
            (void)fprintf(stderr, _("%s\n%.24s : Started %s\n"),
                          buffer, ctime(&now), AFD_MON);
         }
         else
         {
            (void)fprintf(stderr, _("%s\n%.24s : Started %s for %s\n"),
                          buffer, ctime(&now), AFD_MON, service_name);
         }
         (void)memset(buffer, '-', length);
         (void)fprintf(stderr, "%s\n", buffer);
         free(buffer);
      }
   }
   else
   {
      daemon_init(AFD_MON);
      started_as_daemon = YES;
   }
   own_pid = getpid();

   start_afdmon(&group_elements);

#ifdef WITH_SYSTEMD
   if (started_as_daemon == NO)
   {
      if ((systemd_watchdog_enabled = sd_watchdog_enabled(0, NULL)) > 0)
      {
         system_log(INFO_SIGN, NULL, 0,
                    "Enabling systemd watchdog.");
      }
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Calling sd_notifyf(READY=1) ...");
      sd_notifyf(0, "READY=1\n"
                 "STATUS=All process up\n"
                 "MAINPID=%lu",
                 (unsigned long)own_pid);
   }
#endif

   now = p_afd_mon_status->start_time;
   afd_mon_db_check_time = ((now / 10) * 10) + 10;
   new_hour_sum_time = ((now / 3600) * 3600) + 3600;
   new_day_sum_time = ((now / 86400) * 86400) + 86400;
   p_ts = localtime(&now);
   current_week = (p_ts->tm_yday - (p_ts->tm_wday - 1 + 7) % 7 + 7) / 7;
   current_month = p_ts->tm_mon;
   current_year = p_ts->tm_year + 1900;

   bytes_buffered = 0;
   FD_ZERO(&rset);
   for (;;)
   {
#ifdef WITH_SYSTEMD
      UPDATE_HEARTBEAT();
#endif

      /* Initialise descriptor set and timeout. */
      FD_SET(mon_cmd_fd, &rset);
      now = time(NULL);
      timeout.tv_usec = 0L;
      if (group_elements == 0)
      {
         timeout.tv_sec = 2;
      }
      else
      {
         timeout.tv_sec = ((now / AFD_MON_RESCAN_TIME) * AFD_MON_RESCAN_TIME) +
                          AFD_MON_RESCAN_TIME - now;
      }

      /* Wait for message x seconds and then continue. */
      status = select(mon_cmd_fd + 1, &rset, NULL, NULL, &timeout);

      if ((status == 0) && (got_shuttdown_message == NO) &&
          ((now = time(NULL)) >= new_hour_sum_time))
      {
         get_sum_data(HOUR_SUM);

         /* See if we can do the day summary. */
         if (new_hour_sum_time >= new_day_sum_time)
         {
            new_day = YES;
            get_sum_data(DAY_SUM);
            new_day_sum_time = ((new_hour_sum_time / 86400) * 86400) + 86400;

            p_ts = localtime(&new_hour_sum_time);
            new_week = (p_ts->tm_yday - (p_ts->tm_wday - 1 + 7) % 7 + 7) / 7;
            new_month = p_ts->tm_mon;
            new_year = p_ts->tm_year + 1900;

            /* See if we have reached a new week. */
            if (current_week != new_week)
            {
               get_sum_data(WEEK_SUM);
               current_week = new_week;
            }

            /* See if we have reached a new month. */
            if (current_month != new_month)
            {
               get_sum_data(MONTH_SUM);
               current_month = new_month;
            }

            /* See if we have reached a new year. */
            if (current_year != new_year)
            {
               get_sum_data(YEAR_SUM);
               current_year = new_year;
            }
         }

         new_hour_sum_time = ((new_hour_sum_time / 3600) * 3600) + 3600;
      }

      if ((status == 0) && (got_shuttdown_message == NO) &&
          (now >= afd_mon_db_check_time))
      {
#ifdef HAVE_STATX
         if (statx(0, afd_mon_db_file, AT_STATX_SYNC_AS_STAT,
                   STATX_MTIME, &stat_buf) == -1)
#else
         if (stat(afd_mon_db_file, &stat_buf) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not access %s : %s",
                       afd_mon_db_file, strerror(errno));
            exit(INCORRECT);
         }
#ifdef HAVE_STATX
         if (stat_buf.stx_mtime.tv_sec != afd_mon_db_time)
#else
         if (stat_buf.st_mtime != afd_mon_db_time)
#endif
         {
            system_log(INFO_SIGN, NULL, 0, "Rereading AFD_MON_CONFIG.");
#ifdef HAVE_STATX
            afd_mon_db_time = stat_buf.stx_mtime.tv_sec;
#else
            afd_mon_db_time = stat_buf.st_mtime;
#endif
#ifdef WITH_SYSTEMD
            UPDATE_HEARTBEAT();
#endif

            /* Kill all process. */
            stop_process(-1, NO);

            if (msa_detach() != SUCCESS)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to detach from MSA.");
            }
            create_msa();
#ifdef WITH_SYSTEMD
            UPDATE_HEARTBEAT();
#endif

            if (msa_attach() != SUCCESS)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to attach to MSA.");
               exit(INCORRECT);
            }

            /* Start all process. */
            start_all();
#ifdef WITH_SYSTEMD
            UPDATE_HEARTBEAT();
#endif

            mon_active();
         }

         /* Check if total number directories, hosts and/or no_of_jobs */
         /* has changed.                                               */
         new_total_no_of_hosts = new_total_no_of_dirs = new_total_no_of_jobs = 0;
         group_elements = 0;
         for (i = 0; i < no_of_afds; i++)
         {
            if (msa[i].rcmd[0] == '\0')
            {
               group_elements++;
            }
            else
            {
               new_total_no_of_hosts += msa[i].no_of_hosts;
               new_total_no_of_dirs += msa[i].no_of_dirs;
               new_total_no_of_jobs += msa[i].no_of_jobs;
            }
         }
         if ((new_total_no_of_hosts != total_no_of_hosts) ||
             (new_total_no_of_dirs != total_no_of_dirs) ||
             (new_total_no_of_jobs != total_no_of_jobs))
         {
            system_log(INFO_SIGN, NULL, 0,
                       "Totals : no_of_hosts = %u, no_of_dirs = %u, no_of_jobs = %u",
                       new_total_no_of_hosts, new_total_no_of_dirs,
                       new_total_no_of_jobs);
            total_no_of_hosts = new_total_no_of_hosts;
            total_no_of_dirs = new_total_no_of_dirs;
            total_no_of_jobs = new_total_no_of_jobs;
            new_total_no_of_hosts = new_total_no_of_dirs = new_total_no_of_jobs = 0;
         }

         afd_mon_db_check_time = ((now / 10) * 10) + 10;
      }

      if ((status > 0) && (FD_ISSET(mon_cmd_fd, &rset)))
      {
         int  n;

         if ((n = read(mon_cmd_fd, &fifo_buffer[bytes_buffered],
                       fifo_size - bytes_buffered)) > 0)
         {
            eval_cmd_buffer(fifo_buffer, n, &bytes_buffered, &group_elements);
         }
      }
      else if (status == 0)
           {
              if (got_shuttdown_message == NO)
              {
                 if (group_elements > 0)
                 {
                    update_group_summary(new_day);
                    new_day = NO;
                 }

                 /* Check if any process terminated for whatever reason. */
                 zombie_check(now);
              }
           }
           else
           {
              system_log(FATAL_SIGN, __FILE__, __LINE__,
                         "select() error (mon_cmd_fd=%d) : %s",
                         mon_cmd_fd, strerror(errno));
              exit(INCORRECT);
           }
   } /* for (;;) */

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++ eval_cmd_buffer() ++++++++++++++++++++++++++*/
static void
eval_cmd_buffer(char *buffer,
                int  bytes_read,
                int  *bytes_buffered,
                int  *group_elements)
{
   int count,
       pos;

   if (*bytes_buffered > 0)
   {
      bytes_read += *bytes_buffered;
      *bytes_buffered = 0;
   }

   /* Now evaluate all data read from fifo, byte after byte. */
   count = 0;
   do
   {
      switch (buffer[count])
      {
         case SHUTDOWN_ALL : /* Shutdown AFD_MON. */
         case SHUTDOWN  : /* Shutdown mon process. */
            got_shuttdown_message = YES;
            if (p_afd_mon_status != NULL)
            {
               p_afd_mon_status->afd_mon = SHUTDOWN;
            }

            if ((buffer[count] == SHUTDOWN_ALL) ||
                (started_as_daemon == YES))
            {
               /* Shutdown of other process is handled by */
               /* the exit handler.                       */
               exit(SUCCESS);
            }
            else
            {
               /* Kill all process. */
               stop_process(-1, YES);
               count++;
#ifdef WITH_SYSTEMD
               sd_notify(0, "STATUS=Stopped on user request\n");
#endif
               break;
            }

         case START : /* Start afd_mon. */
            got_shuttdown_message = NO;
            start_afdmon(group_elements);
#ifdef WITH_SYSTEMD
            if (started_as_daemon == NO)
            {
               sd_notify(0, "STATUS=All process up\n");
            }
#endif
            if (send_cmd(ACKN, probe_only_fd) < 0)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "Was not able to send acknowledge via fifo.");
               exit(INCORRECT);
            }
            count++;
            break;

         case IS_ALIVE  : /* Somebody wants to know whether an */
                          /* AFDMON process is running in this */
                          /* directory.                        */
            {
               char cmd;

               if (sleep_sys_log_fd == -1)
               {
                  cmd = ACKN;
               }
               else
               {
                  cmd = ACKN_STOPPED;
               }
               if (send_cmd(cmd, probe_only_fd) < 0)
               {
                  system_log(FATAL_SIGN, __FILE__, __LINE__,
                             "Was not able to send acknowledge via fifo.");
                  exit(INCORRECT);
               }
            }
            count++;
            break;

         case GOT_LC : /* Got log capabilities. */
            count++;
            if ((bytes_read - count) >= SIZEOF_INT)
            {
               (void)memcpy(&pos, &buffer[count], SIZEOF_INT);
               if (pos < no_of_afds)
               {
                  if (pl[pos].log_pid > 0)
                  {
                     stop_log_process(pos);
                  }
                  if (((msa[pos].log_capabilities & AFDD_SYSTEM_LOG) &&
                       (msa[pos].options & AFDD_SYSTEM_LOG)) ||
                      ((msa[pos].log_capabilities & AFDD_EVENT_LOG) &&
                       (msa[pos].options & AFDD_EVENT_LOG)) ||
                      ((msa[pos].log_capabilities & AFDD_RECEIVE_LOG) &&
                       (msa[pos].options & AFDD_RECEIVE_LOG)) ||
                      ((msa[pos].log_capabilities & AFDD_TRANSFER_LOG) &&
                       (msa[pos].options & AFDD_TRANSFER_LOG)) ||
                      ((msa[pos].log_capabilities & AFDD_TRANSFER_DEBUG_LOG) &&
                       (msa[pos].options & AFDD_TRANSFER_DEBUG_LOG)) ||
                      ((msa[pos].log_capabilities & AFDD_INPUT_LOG) &&
                       (msa[pos].options & AFDD_INPUT_LOG)) ||
                      ((msa[pos].log_capabilities & AFDD_DISTRIBUTION_LOG) &&
                       (msa[pos].options & AFDD_DISTRIBUTION_LOG)) ||
                      ((msa[pos].log_capabilities & AFDD_PRODUCTION_LOG) &&
                       (msa[pos].options & AFDD_PRODUCTION_LOG)) ||
                      ((msa[pos].log_capabilities & AFDD_OUTPUT_LOG) &&
                       (msa[pos].options & AFDD_OUTPUT_LOG)) ||
                      ((msa[pos].log_capabilities & AFDD_DELETE_LOG) &&
                       (msa[pos].options & AFDD_DELETE_LOG)) ||
                      ((msa[pos].log_capabilities & AFDD_JOB_DATA) &&
                       (msa[pos].options & AFDD_JOB_DATA)))
                  {
                     start_log_process(pos, msa[pos].log_capabilities);
                  }
               }
               count += SIZEOF_INT;
            }
            else
            {
               *bytes_buffered = bytes_read - (count - 1);
               (void)memmove(buffer, &buffer[count - 1], *bytes_buffered);

               return;
            }
            break;

         case DISABLE_MON : /* Disable monitoring of an AFD. */
            count++;
            if ((bytes_read - count) >= SIZEOF_INT)
            {
               (void)memcpy(&pos, &buffer[count], SIZEOF_INT);
               if ((pos < no_of_afds) && (pl[pos].mon_pid > 0))
               {
                  msa[pos].connect_status = DISABLED;
                  stop_process(pos, NO);
               }
               count += SIZEOF_INT;
            }
            else
            {
               *bytes_buffered = bytes_read - (count - 1);
               (void)memmove(buffer, &buffer[count - 1], *bytes_buffered);

               return;
            }
            break;

         case ENABLE_MON : /* Enable monitoring of an AFD. */
            count++;
            if ((bytes_read - count) >= SIZEOF_INT)
            {
               (void)memcpy(&pos, &buffer[count], SIZEOF_INT);
               if ((pos < no_of_afds) && (pl[pos].mon_pid == 0))
               {
                  msa[pos].connect_status = DISCONNECTED;
                  if ((pl[pos].mon_pid = start_process(MON_PROC, pos)) != INCORRECT)
                  {
                     pl[pos].start_time = time(NULL);
                  }
               }
               count += SIZEOF_INT;
            }
            else
            {
               *bytes_buffered = bytes_read - (count - 1);
               (void)memmove(buffer, &buffer[count - 1], *bytes_buffered);

               return;
            }
            break;

         default        : /* Reading garbage from fifo. */
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Reading garbage on fifo %s [%d]. Ignoring.",
                       MON_CMD_FIFO, (int)buffer[count]);
            break;
      }
   } while (count < bytes_read);

   return;
}


/*+++++++++++++++++++++++++++ zombie_check() ++++++++++++++++++++++++++++*/
static void
zombie_check(time_t now)
{
   int   faulty = NO,
         i,
         status;
   pid_t ret;

   /*
    * Check if log process are still active.
    */
   if ((ret = waitpid(sys_log_pid, &status, WNOHANG)) == sys_log_pid)
   {
      int ret_stat;

      p_afd_mon_status->mon_sys_log = OFF;
      if ((ret_stat = WIFEXITED(status)))
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "System log of %s terminated with %d.", AFD_MON, ret_stat);
         sys_log_pid = 0;
      }
      else if (WIFSIGNALED(status))
           {
              /* Abnormal termination. */
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Abnormal termination of system log process of %s, caused by signal %d.",
                         AFD_MON, WTERMSIG(status));
              sys_log_pid = 0;
           }

      /* Restart log process. */
      system_log(INFO_SIGN, NULL, 0, "Restart %s system log process.", AFD_MON);
      if ((sys_log_pid = start_process(MON_SYS_LOG, -1)) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not start system log process for AFD_MON.");
         exit(INCORRECT);
      }
      p_afd_mon_status->mon_sys_log = ON;
   }
   else if (ret == -1)
        {
           system_log(ERROR_SIGN, __FILE__, __LINE__,
                      "waitpid() error : %s", strerror(errno));
        }
   if ((ret = waitpid(mon_log_pid, &status, WNOHANG)) == mon_log_pid)
   {
      int ret_stat;

      p_afd_mon_status->mon_log = OFF;
      if ((ret_stat = WIFEXITED(status)))
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Monitor log of %s terminated with %d.", AFD_MON, ret_stat);
         mon_log_pid = 0;
      }
      else if (WIFSIGNALED(status))
           {
              /* Abnormal termination. */
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Abnormal termination of monitor log process of %s, caused by signal %d.",
                         AFD_MON, WTERMSIG(status));
              mon_log_pid = 0;
           }

      /* Restart log process. */
      system_log(INFO_SIGN, NULL, 0,
                 "Restart %s monitor log process.", AFD_MON);
      if ((mon_log_pid = start_process(MONITOR_LOG, -1)) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not start monitor log process for AFD_MON.");
         exit(INCORRECT);
      }
      p_afd_mon_status->mon_log = ON;
   }
   else if (ret == -1)
        {
           system_log(ERROR_SIGN, __FILE__, __LINE__,
                      "waitpid() error : %s", strerror(errno));
        }
   if ((ret = waitpid(aldad_pid, &status, WNOHANG)) == aldad_pid)
   {
      int ret_stat;

      p_afd_mon_status->aldad = OFF;
      if ((ret_stat = WIFEXITED(status)))
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "ALDA daemon of %s terminated with %d.", AFD_MON, ret_stat);
         aldad_pid = 0;
      }
      else if (WIFSIGNALED(status))
           {
              /* Abnormal termination. */
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Abnormal termination of ALDA daemon of %s, caused by signal %d.",
                         AFD_MON, WTERMSIG(status));
              aldad_pid = 0;
           }

      /* Restart log process. */
      system_log(INFO_SIGN, NULL, 0,
                 "Restart %s aldad process.", AFD_MON);
      if ((aldad_pid = start_process(ALDAD, -2)) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not start ALDA daemon process for AFD_MON.");
         exit(INCORRECT);
      }
      p_afd_mon_status->aldad = ON;
   }
   else if (ret == -1)
        {
           system_log(ERROR_SIGN, __FILE__, __LINE__,
                      "waitpid() error : %s", strerror(errno));
        }

   /*
    * Now check if all mon processes are still alive.
    */
   for (i = 0; i < no_of_afds; i++)
   {
      if (pl[i].mon_pid > 0)
      {
         if ((ret = waitpid(pl[i].mon_pid, &status, WNOHANG)) == pl[i].mon_pid)
         {
            if (WIFEXITED(status))
            {
               switch (WEXITSTATUS(status))
               {
                  case SUCCESS :
                     faulty = NO;
                     pl[i].mon_pid = 0;
                     pl[i].start_time = 0;
                     pl[i].number_of_restarts = 0;
                     break;

                  default   : /* Unkown error */
                     faulty = YES;
                     pl[i].mon_pid = 0;
                     break;
               }
            }
            else if (WIFSIGNALED(status))
                 {
                    /* Abnormal termination. */
                    system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                               "Abnormal termination of process %d monitoring %s, caused by signal %d.",
#else
                               "Abnormal termination of process %lld monitoring %s, caused by signal %d.",
#endif
                               (pri_pid_t)pl[i].mon_pid, pl[i].afd_alias,
                               WTERMSIG(status));
                    faulty = YES;
                    pl[i].mon_pid = 0;
                 }
         }
         else if (ret == -1)
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                            "waitpid() %d (pos %d) error : %s",
#else
                            "waitpid() %lld (pos %d) error : %s",
#endif
                            (pri_pid_t)pl[i].mon_pid, i, strerror(errno));
              }

         if ((faulty == YES) && (msa[i].connect_status != DISABLED))
         {
            if (pl[i].number_of_restarts < 20)
            {
               /* Restart monitor process. */
               if ((pl[i].mon_pid = start_process(MON_PROC, i)) != INCORRECT)
               {
                  if (now > (pl[i].start_time + 5))
                  {
                     pl[i].number_of_restarts = 0;
                  }
                  else
                  {
                     pl[i].number_of_restarts++;
                  }
                  pl[i].start_time = now;
               }
            }
            else
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "To many restarts of mon process for %s. Will NOT try to start it again.",
                          pl[i].afd_alias);
            }
            faulty = NO;
         }
      } /* if (pl[i].mon_pid > 0) */

      if (pl[i].log_pid > 0)
      {
         if ((ret = waitpid(pl[i].log_pid, &status, WNOHANG)) == pl[i].log_pid)
         {
            if (WIFEXITED(status))
            {
               switch (WEXITSTATUS(status))
               {
                  case REMOTE_HANGUP     :
                  case LOG_DATA_TIMEOUT  :
                  case FAILED_LOG_CMD    :
                  case LOG_CONNECT_ERROR :
                     pl[i].next_retry_time_log = now + RETRY_INTERVAL;
                     pl[i].log_pid = -1;
                     break;

                  case MISSED_PACKET :
                     start_log_process(i, msa[i].log_capabilities);
                     break;

                  default   : /* Unkown error */
                     system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                                "Termination of process %d receiving log data from %s (%d).",
#else
                                "Termination of process %lld receiving log data from %s (%d).",
#endif
                                (pri_pid_t)pl[i].log_pid, pl[i].afd_alias,
                                WEXITSTATUS(status));
                     pl[i].next_retry_time_log = now + RETRY_INTERVAL;
                     pl[i].log_pid = -1;
                     break;
               }
            }
            else if (WIFSIGNALED(status))
                 {
                    /* Abnormal termination. */
                    system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                               "Abnormal termination of process %d receiving log data from %s, caused by signal %d.",
#else
                               "Abnormal termination of process %lld receiving log data from %s, caused by signal %d.",
#endif
                               (pri_pid_t)pl[i].log_pid, pl[i].afd_alias,
                               WTERMSIG(status));
                    pl[i].next_retry_time_log = now + RETRY_INTERVAL;
                    pl[i].log_pid = -1;
                 }
         }
         else if (ret == -1)
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                            "waitpid() %d (pos %d) error : %s",
#else
                            "waitpid() %lld (pos %d) error : %s",
#endif
                            (pri_pid_t)pl[i].log_pid, i, strerror(errno));
              }
      } /* if (pl[i].log_pid > 0) */

      /*
       * Lets check if we need to restart any log process at a given
       * time.
       */
      if ((msa[i].connect_status != DISABLED) && (pl[i].log_pid == -1) &&
          (pl[i].next_retry_time_log != 0L) &&
          (now > pl[i].next_retry_time_log))
      {
         start_log_process(i, msa[i].log_capabilities);
      }
   } /* for (i = 0; i < no_of_afds; i++) */

   return;
}


/*++++++++++++++++++++++++++++ start_afdmon() +++++++++++++++++++++++++++*/
void
start_afdmon(int *group_elements)
{
   if ((p_afd_mon_status == NULL) || (p_afd_mon_status->afd_mon != ON))
   {
      int         fd,
                  i,
                  old_afd_mon_stat;
      char        hostname[64],
                  *ptr;
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat stat_buf;
#endif

#ifdef HAVE_STATX
      if (statx(0, afd_mon_db_file, AT_STATX_SYNC_AS_STAT,
                STATX_MTIME, &stat_buf) == -1)
#else
      if (stat(afd_mon_db_file, &stat_buf) == -1)
#endif
      {
         (void)fprintf(stderr,
                       "ERROR   : Could not stat() %s : %s (%s %d)\n",
                       afd_mon_db_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
#ifdef HAVE_STATX
      if (afd_mon_db_time != stat_buf.stx_mtime.tv_sec)
#else
      if (afd_mon_db_time != stat_buf.st_mtime)
#endif
      {
#ifdef HAVE_STATX
         afd_mon_db_time = stat_buf.stx_mtime.tv_sec;
#else
         afd_mon_db_time = stat_buf.st_mtime;
#endif
         create_msa();
      }
#ifdef WITH_SYSTEMD
      UPDATE_HEARTBEAT();
#endif

#ifdef HAVE_STATX
      if ((statx(0, afd_mon_status_file, AT_STATX_SYNC_AS_STAT,
                 STATX_SIZE, &stat_buf) == -1) ||
          (stat_buf.stx_size != sizeof(struct afd_mon_status)))
#else
      if ((stat(afd_mon_status_file, &stat_buf) == -1) ||
          (stat_buf.st_size != sizeof(struct afd_mon_status)))
#endif
      {
         if (errno != ENOENT)
         {
            (void)fprintf(stderr, "Failed to access %s : %s (%s %d)\n",
                          afd_mon_status_file, strerror(errno),
                          __FILE__, __LINE__);
            exit(INCORRECT);
         }
         if ((fd = coe_open(afd_mon_status_file, O_RDWR | O_CREAT | O_TRUNC,
#ifdef GROUP_CAN_WRITE
                            S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) == -1)
#else
                            S_IRUSR | S_IWUSR)) == -1)
#endif
         {
            (void)fprintf(stderr, "Failed to create %s : %s (%s %d)\n",
                          afd_mon_status_file, strerror(errno),
                          __FILE__, __LINE__);
            exit(INCORRECT);
         }
         if (lseek(fd, sizeof(struct afd_mon_status) - 1, SEEK_SET) == -1)
         {
            (void)fprintf(stderr, "Could not seek() on %s : %s (%s %d)\n",
                          afd_mon_status_file, strerror(errno),
                          __FILE__, __LINE__);
            exit(INCORRECT);
         }
         if (write(fd, "", 1) != 1)
         {
            (void)fprintf(stderr, "write() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         old_afd_mon_stat = NO;
      }
      else
      {
         if ((fd = coe_open(afd_mon_status_file, O_RDWR)) == -1)
         {
            (void)fprintf(stderr, "Failed to create %s : %s (%s %d)\n",
                          afd_mon_status_file, strerror(errno),
                          __FILE__, __LINE__);
            exit(INCORRECT);
         }
         old_afd_mon_stat = YES;
      }
#ifdef HAVE_MMAP
      if ((ptr = mmap(0, sizeof(struct afd_mon_status), (PROT_READ | PROT_WRITE),
                      MAP_SHARED, fd, 0)) == (caddr_t) -1)
#else
      if ((ptr = mmap_emu(0, sizeof(struct afd_mon_status),
                          (PROT_READ | PROT_WRITE),
                          MAP_SHARED, afd_mon_status_file, 0)) == (caddr_t) -1)
#endif
      {
         (void)fprintf(stderr, "mmap() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      if (close(fd) == -1)
      {
         (void)fprintf(stderr, "close() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
      }
      p_afd_mon_status = (struct afd_mon_status *)ptr;
      if (old_afd_mon_stat == NO)
      {
         (void)memset(p_afd_mon_status, 0, sizeof(struct afd_mon_status));
      }
      p_afd_mon_status->afd_mon     = ON;
      p_afd_mon_status->mon_sys_log = 0;
      p_afd_mon_status->mon_log     = 0;
#ifdef WITH_SYSTEMD
      UPDATE_HEARTBEAT();
#endif

      /* Start log process for afd_monitor */
      if ((sys_log_pid = start_process(MON_SYS_LOG, -1)) < 0)
      {
         (void)fprintf(stderr,
                       "ERROR   : Could not start system log process for AFD_MON. (%s %d)\n",
                       __FILE__, __LINE__);
         exit(INCORRECT);
      }
      if (daemon_log_fd != -1)
      {
         (void)close(daemon_log_fd);
         daemon_log_fd = -1;
      }
      if (sleep_sys_log_fd != -1)
      {
         sys_log_fd = sleep_sys_log_fd;
         sleep_sys_log_fd = -1;
      }
      p_afd_mon_status->mon_sys_log = ON;
      if ((mon_log_pid = start_process(MONITOR_LOG, -1)) < 0)
      {
         (void)fprintf(stderr,
                       "ERROR   : Could not start monitor log process for AFD_MON. (%s %d)\n",
                       __FILE__, __LINE__);
         exit(INCORRECT);
      }
      p_afd_mon_status->mon_log = ON;
      if ((aldad_pid = start_process(ALDAD, -2)) < 0)
      {
         (void)fprintf(stderr,
                       "ERROR   : Could not start ALDA daemon for AFD_MON. (%s %d)\n",
                       __FILE__, __LINE__);
         exit(INCORRECT);
      }
      p_afd_mon_status->aldad = ON;

      p_afd_mon_status->start_time = time(NULL);
      system_log(INFO_SIGN, NULL, 0,
                 "=================> STARTUP <=================");
      if (gethostname(hostname, 64) == 0)
      {
         char      dstr[26];
         struct tm *p_ts;

         p_ts = localtime(&p_afd_mon_status->start_time);
         strftime(dstr, 26, "%a %h %d %H:%M:%S %Y", p_ts);
         system_log(CONFIG_SIGN, NULL, 0, "Starting on <%s> %s", hostname, dstr);
      }
      system_log(INFO_SIGN, NULL, 0,
                 "Starting %s (%s)", AFD_MON, PACKAGE_VERSION);

      if (msa_attach() != SUCCESS)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__, "Failed to attach to MSA.");
         exit(INCORRECT);
      }

      *group_elements = 0;
      for (i = 0; i < no_of_afds; i++)
      {
         if (msa[i].rcmd[0] == '\0')
         {
            (*group_elements)++;
         }
      }
#ifdef WITH_SYSTEMD
      UPDATE_HEARTBEAT();
#endif

      /* Start all process. */
      start_all();

      /* Log all pid's in MON_ACTIVE file. */
      mon_active();
#ifdef WITH_SYSTEMD
      UPDATE_HEARTBEAT();
#endif
   }
   else
   {
      *group_elements = 0;
   }

   return;
}


/*++++++++++++++++++++++++++++ mon_active() +++++++++++++++++++++++++++++*/
/*                             ------------                              */
/* Description: Writes all pid's of processes that have been started by  */
/*              the afd_mon process to the MON_ACTIVE file. So that when */
/*              afd_mon is killed, all it's child process get eliminated */
/*              before starting it again.                                */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void
mon_active(void)
{
   int    i,
          mon_active_fd;
   size_t size;
   char   *buffer,
          *ptr;

   if ((mon_active_fd = open(mon_active_file, (O_RDWR | O_CREAT | O_TRUNC),
#ifdef GROUP_CAN_WRITE
                             (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP))) < 0)
#else
                             (S_IRUSR | S_IWUSR))) < 0)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to create %s : %s", mon_active_file, strerror(errno));
      exit(INCORRECT);
   }
   size = ((4 + no_of_afds + no_of_afds) * sizeof(pid_t)) + sizeof(int) + 1;
   if ((buffer = malloc(size)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Write data to buffer. */
   ptr = buffer;
   *(pid_t *)ptr = own_pid;
   ptr += sizeof(pid_t);
   *(pid_t *)ptr = sys_log_pid;
   ptr += sizeof(pid_t);
   *(pid_t *)ptr = mon_log_pid;
   ptr += sizeof(pid_t);
   *(pid_t *)ptr = aldad_pid;
   ptr += sizeof(pid_t);
   *(int *)ptr = no_of_afds;
   ptr += sizeof(int);
   for (i = 0; i < no_of_afds; i++)
   {
      *(pid_t *)ptr = pl[i].mon_pid;
      ptr += sizeof(pid_t);
      *(pid_t *)ptr = pl[i].log_pid;
      ptr += sizeof(pid_t);
   }

   /* Write buffer into file. */
   if (write(mon_active_fd, buffer, size) != size)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "write() error in %s : %s", mon_active_file, strerror(errno));
      exit(INCORRECT);
   }
   if (close(mon_active_fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }
   free(buffer);

   return;
}


/*+++++++++++++++++++++++++++ get_sum_data() ++++++++++++++++++++++++++++*/
static void
get_sum_data(int sum_type)
{
   int          i;
   unsigned int diff_files_received,
                diff_files_send,
                diff_connections,
                diff_total_errors,
                total_files_received = 0,
                total_files_send = 0,
                total_connections = 0,
                total_total_errors = 0;
#ifdef NEW_MSA
   u_off_t      diff_bytes_received,
                diff_bytes_send,
                diff_log_bytes_received,
                total_bytes_received = 0,
                total_bytes_send = 0,
                total_log_bytes_received = 0;
#else
   u_off_t      diff_bytes_received,
                diff_bytes_send,
                diff_log_bytes_received,
                total_bytes_received = 0,
                total_bytes_send = 0,
                total_log_bytes_received = 0;
#endif

   for (i = 0; i < no_of_afds; i++)
   {
      p_mon_alias = msa[i].afd_alias;
      if (msa[i].files_received[CURRENT_SUM] >= msa[i].files_received[sum_type])
      {
         diff_files_received = msa[i].files_received[CURRENT_SUM] - msa[i].files_received[sum_type];
      }
      else
      {
         mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, NULL,
                 "files_received overflowed (%u < %u)! Correcting.",
                 msa[i].files_received[CURRENT_SUM],
                 msa[i].files_received[sum_type]);
         diff_files_received = 0;
      }
      if (msa[i].bytes_received[CURRENT_SUM] >= msa[i].bytes_received[sum_type])
      {
         diff_bytes_received = msa[i].bytes_received[CURRENT_SUM] - msa[i].bytes_received[sum_type];
      }
      else
      {
         mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, NULL,
#if SIZEOF_OFF_T == 4
                 "bytes_received overflowed (%ld < %ld)! Correcting.",
#else
                 "bytes_received overflowed (%lld < %lld)! Correcting.",
#endif
                 msa[i].bytes_received[CURRENT_SUM],
                 msa[i].bytes_received[sum_type]);
         diff_bytes_received = 0;
      }
      if (msa[i].files_send[CURRENT_SUM] >= msa[i].files_send[sum_type])
      {
         diff_files_send = msa[i].files_send[CURRENT_SUM] - msa[i].files_send[sum_type];
      }
      else
      {
         mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, NULL,
                 "files_send overflowed (%u < %u)! Correcting.",
                 msa[i].files_send[CURRENT_SUM], msa[i].files_send[sum_type]);
         diff_files_send = 0;
      }
      if (msa[i].bytes_send[CURRENT_SUM] >= msa[i].bytes_send[sum_type])
      {
         diff_bytes_send = msa[i].bytes_send[CURRENT_SUM] - msa[i].bytes_send[sum_type];
      }
      else
      {
         mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, NULL,
#ifdef NEW_MSA
                 "bytes_send overflowed (%.0lf < %.0lf)! Correcting.",
#else
# if SIZEOF_OFF_T == 4
                 "bytes_send overflowed (%ld < %ld)! Correcting.",
# else
                 "bytes_send overflowed (%lld < %lld)! Correcting.",
# endif
#endif
                 msa[i].bytes_send[CURRENT_SUM], msa[i].bytes_send[sum_type]);
         diff_bytes_send = 0;
      }
      if (msa[i].connections[CURRENT_SUM] >= msa[i].connections[sum_type])
      {
         diff_connections = msa[i].connections[CURRENT_SUM] - msa[i].connections[sum_type];
      }
      else
      {
         mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, NULL,
                 "connections overflowed (%u < %u)! Correcting.",
                 msa[i].connections[CURRENT_SUM], msa[i].connections[sum_type]);
         diff_connections = 0;
      }
      if (msa[i].total_errors[CURRENT_SUM] >= msa[i].total_errors[sum_type])
      {
         diff_total_errors = msa[i].total_errors[CURRENT_SUM] - msa[i].total_errors[sum_type];
      }
      else
      {
         mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, NULL,
                 "total_errors overflowed (%u < %u)! Correcting.",
                 msa[i].total_errors[CURRENT_SUM],
                 msa[i].total_errors[sum_type]);
         diff_total_errors = 0;
      }
      if (msa[i].log_bytes_received[CURRENT_SUM] >= msa[i].log_bytes_received[sum_type])
      {
         diff_log_bytes_received = msa[i].log_bytes_received[CURRENT_SUM] - msa[i].log_bytes_received[sum_type];
      }
      else
      {
         mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, NULL,
#if SIZEOF_OFF_T == 4
                 "log_bytes_received overflowed (%ld < %ld)! Correcting.",
#else
                 "log_bytes_received overflowed (%lld < %lld)! Correcting.",
#endif
                 msa[i].log_bytes_received[CURRENT_SUM],
                 msa[i].log_bytes_received[sum_type]);
         diff_log_bytes_received = 0;
      }

      print_data(YES, sum_type, diff_files_received,
                 diff_bytes_received, diff_files_send,
                 diff_bytes_send, diff_connections, diff_total_errors,
                 diff_log_bytes_received);
      msa[i].files_received[sum_type] = msa[i].files_received[CURRENT_SUM];
      msa[i].bytes_received[sum_type] = msa[i].bytes_received[CURRENT_SUM];
      msa[i].files_send[sum_type] = msa[i].files_send[CURRENT_SUM];
      msa[i].bytes_send[sum_type] = msa[i].bytes_send[CURRENT_SUM];
      msa[i].connections[sum_type] = msa[i].connections[CURRENT_SUM];
      msa[i].total_errors[sum_type] = msa[i].total_errors[CURRENT_SUM];
      msa[i].log_bytes_received[sum_type] = msa[i].log_bytes_received[CURRENT_SUM];

      total_files_received += diff_files_received;
      total_bytes_received += diff_bytes_received;
      total_files_send += diff_files_send;
      total_bytes_send += diff_bytes_send;
      total_connections += diff_connections;
      total_total_errors += diff_total_errors;
      total_log_bytes_received += diff_log_bytes_received;
   }
   print_data(NO, sum_type, total_files_received,
              total_bytes_received, total_files_send,
              total_bytes_send, total_connections, total_total_errors,
              total_log_bytes_received);

   return;
}


/*----------------------------- print_data() ----------------------------*/
static void
#ifdef NEW_MSA
print_data(int          mon_log_fd,
           int          log_type,
           unsigned int files_received,
           double       bytes_received,
           unsigned int files_send,
           double       bytes_send,
           unsigned int connections,
           unsigned int total_errors,
           double       log_bytes_received)
#else
print_data(int          mon_log_fd,
           int          log_type,
           unsigned int files_received,
           u_off_t      bytes_received,
           unsigned int files_send,
           u_off_t      bytes_send,
           unsigned int connections,
           unsigned int total_errors,
           u_off_t      log_bytes_received)
#endif
{
   int  length;
   char line[108 + (4 * MAX_INT_LENGTH) + (3 * MAX_LONG_LONG_LENGTH)];

   length = sprintf(line, "--%s sum-- Input: %u files ",
                    sum_stat_type[log_type], files_received);
   if (bytes_received < KILOBYTE)
   {
#ifdef NEW_MSA
      length += sprintf(&line[length], "%.0lf bytes | Output: %u files ",
#else
# if SIZEOF_OFF_T == 4
      length += sprintf(&line[length], "%lu bytes | Output: %u files ",
# else
      length += sprintf(&line[length], "%llu bytes | Output: %u files ",
# endif
#endif
                        bytes_received, files_send);
   }
   else if (bytes_received < MEGABYTE)
        {
           length += sprintf(&line[length], "%.2f KB | Output: %u files ",
                             (double)bytes_received / F_KILOBYTE, files_send);
        }
   else if (bytes_received < GIGABYTE)
        {
           length += sprintf(&line[length], "%.2f MB | Output: %u files ",
                             (double)bytes_received / F_MEGABYTE, files_send);
        }
   else if (bytes_received < TERABYTE)
        {
           length += sprintf(&line[length], "%.2f GB | Output: %u files ",
                             (double)bytes_received / F_GIGABYTE, files_send);
        }
   else if (bytes_received < PETABYTE)
        {
           length += sprintf(&line[length], "%.2f TB | Output: %u files ",
                             (double)bytes_received / F_TERABYTE, files_send);
        }
   else if (bytes_received < EXABYTE)
        {
           length += sprintf(&line[length], "%.2f PB | Output: %u files ",
                             (double)bytes_received / F_PETABYTE, files_send);
        }
        else
        {
           length += sprintf(&line[length], "%.2f EB | Output: %u files ",
                             (double)bytes_received / F_EXABYTE, files_send);
        }
   if (bytes_send < KILOBYTE)
   {
#ifdef NEW_MSA
      length += sprintf(&line[length], "%.0lf bytes %u connections %u errors",
#else
# if SIZEOF_OFF_T == 4
      length += sprintf(&line[length], "%lu bytes %u connections %u errors",
# else
      length += sprintf(&line[length], "%llu bytes %u connections %u errors",
# endif
#endif
                        bytes_send, connections, total_errors);
   }
   else if (bytes_send < MEGABYTE)
        {
           length += sprintf(&line[length], "%.2f KB %u connections %u errors",
                             (double)bytes_send / F_KILOBYTE,
                             connections, total_errors);
        }
   else if (bytes_send < GIGABYTE)
        {
           length += sprintf(&line[length], "%.2f MB %u connections %u errors",
                             (double)bytes_send / F_MEGABYTE,
                             connections, total_errors);
        }
   else if (bytes_send < TERABYTE)
        {
           length += sprintf(&line[length], "%.2f GB %u connections %u errors",
                             (double)bytes_send / F_GIGABYTE,
                             connections, total_errors);
        }
   else if (bytes_send < PETABYTE)
        {
           length += sprintf(&line[length], "%.2f TB %u connections %u errors",
                             (double)bytes_send / F_TERABYTE,
                             connections, total_errors);
        }
   else if (bytes_send < EXABYTE)
        {
           length += sprintf(&line[length], "%.2f PB %u connections %u errors",
                             (double)bytes_send / F_PETABYTE,
                             connections, total_errors);
        }
        else
        {
           length += sprintf(&line[length], "%.2f EB %u connections %u errors",
                             (double)bytes_send / F_EXABYTE,
                             connections, total_errors);
        }
   if (log_bytes_received < KILOBYTE)
   {
#ifdef NEW_MSA
      length += sprintf(&line[length], " | Log data received: %.0lf bytes",
#else
# if SIZEOF_OFF_T == 4
      length += sprintf(&line[length], " | Log data received: %lu bytes",
# else
      length += sprintf(&line[length], " | Log data received: %llu bytes",
# endif
#endif
                        log_bytes_received);
   }
   else if (log_bytes_received < MEGABYTE)
        {
           length += sprintf(&line[length], " | Log data received: %.2f KB",
                             (double)log_bytes_received / F_KILOBYTE);
        }
   else if (log_bytes_received < GIGABYTE)
        {
           length += sprintf(&line[length], " | Log data received: %.2f MB",
                             (double)log_bytes_received / F_MEGABYTE);
        }
   else if (log_bytes_received < TERABYTE)
        {
           length += sprintf(&line[length], " | Log data received: %.2f GB",
                             (double)log_bytes_received / F_GIGABYTE);
        }
   else if (log_bytes_received < PETABYTE)
        {
           length += sprintf(&line[length], " | Log data received: %.2f TB",
                             (double)log_bytes_received / F_TERABYTE);
        }
   else if (log_bytes_received < EXABYTE)
        {
           length += sprintf(&line[length], " | Log data received: %.2f PB",
                             (double)log_bytes_received / F_PETABYTE);
        }
        else
        {
           length += sprintf(&line[length], " | Log data received: %.2f EB",
                             (double)log_bytes_received / F_EXABYTE);
        }
   if (mon_log_fd == YES)
   {
      mon_log(INFO_SIGN, NULL, 0, 0L, NULL, "%s", line);
   }
   else
   {
      system_log(INFO_SIGN, NULL, 0, "%s", line);
   }

   return;
}


/*++++++++++++++++++++++++++++ afd_mon_exit() +++++++++++++++++++++++++++*/
static void
afd_mon_exit(void)
{
   if (in_child == NO)
   {
#ifdef WITH_SYSTEMD
      if (started_as_daemon != YES)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Calling sd_notifyf(STOPPING=1) ...");
         sd_notify(0, "STOPPING=1\n");
      }
#endif

      /* Kill any job still active! */
      stop_process(-1, got_shuttdown_message);

      if (unlink(mon_active_file) == -1)
      {
         (void)fprintf(stderr,
                       _("Failed to unlink() `%s' : %s (%s %d)\n"),
                       mon_active_file, strerror(errno), __FILE__, __LINE__);
      }

#ifdef WITH_SYSTEMD
      sd_notify(0, "STATUS=Terminated\n");
#endif
   }

   return;
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{
   (void)fprintf(stderr,
                 _("SYNTAX  : %s[ -w working directory]\n"), progname);
   (void)fprintf(stderr,
                 _("                    -nd        Do not start as daemon process.\n"));
   (void)fprintf(stderr,
                 _("                    -C         Start with all checks done by cmdline mafd.\n"));
   (void)fprintf(stderr,
                 _("                    -sn <name> Provide a service name.\n"));
   (void)fprintf(stderr,
                 _("                    --version  Show version number.\n"));

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   system_log(FATAL_SIGN, __FILE__, __LINE__, "Aaarrrggh! Received SIGSEGV.");
   afd_mon_exit();

   /* Dump core so we know what happened. */
   abort();                                 
}          


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   system_log(FATAL_SIGN, __FILE__, __LINE__, "Uuurrrggh! Received SIGBUS.");
   afd_mon_exit();

   /* Dump core so we know what happened. */
   abort();
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   if (signo == SIGTERM)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__, _("Received SIGTERM!"));
   }
   else if (signo == SIGINT)
        {
           system_log(DEBUG_SIGN, __FILE__, __LINE__, _("Received SIGINT!"));
        }
        else
        {
           system_log(DEBUG_SIGN, __FILE__, __LINE__, _("Received %d!"), signo);
        }

   exit(INCORRECT);
}
