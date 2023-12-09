/*
 *  dir_check.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   dir_check - waits for files to appear in user directories to
 **               create a job for the FD
 **
 ** SYNOPSIS
 **   dir_check [--version]              - Show version.
 **             <work_dir>               - Working directory of AFD.
 **             <rescan time>            - The time interval when to check
 **                                        whether any directories have changed.
 **             <no of process>          - The maximum number that it may fork
 **                                        to copy files.
 **             <no_of_local_dirs>       - The number of 'user' directories
 **                                        specified in the DIR_CONFIG file
 **                                        and are local.
 **             <create source dir mode> - When creating source dirs, what
 **                                        mode they should have.
 **
 ** DESCRIPTION
 **   This program waits for files to appear in the user directory
 **   to create a job for the FD (File Distributor). A job always
 **   consists of a directory which holds all files to be send and
 **   a message which tells the FD what to to with the job.
 **
 **   If the user directory is not in the same file system as dir_check,
 **   it will fork to copy the files from the user directory to the
 **   local AFD directory. Thus slow copies will not slow down the
 **   process of generating new jobs for the FD. This is important
 **   when the user directory is mounted via NFS.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has
 **   occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.10.1995 H.Kiehl Created
 **   02.01.1997 H.Kiehl Added logging of file names that are found by
 **                      by the AFD.
 **   01.02.1997 H.Kiehl Read renaming rule file.
 **   19.04.1997 H.Kiehl Add support for writing messages in one memory
 **                      mapped file.
 **   04.03.1999 H.Kiehl When copying files don't only limit the number
 **                      of files, also limit the size that may be copied.
 **   09.10.1999 H.Kiehl Added new feature of important directories.
 **   04.12.2000 H.Kiehl Report the exact time when scanning of directories
 **                      took so long.
 **   12.04.2001 H.Kiehl Check pool directory for unfinished jobs.
 **   09.02.2002 H.Kiehl Wait for all children to terminate before exiting.
 **   16.05.2002 H.Kiehl Removed shared memory stuff.
 **   18.09.2003 H.Kiehl Check if time goes backward.
 **   01.03.2008 H.Kiehl check_file_dir() is now performed by dir_check.
 **   01.02.2010 H.Kiehl Option to set system wide force reread interval.
 **
 */
DESCR__E_M1

/* #define WITH_MEMCHECK */

#include <stdio.h>                 /* fprintf(), sprintf()               */
#include <string.h>                /* strcpy(), strcat(), strcmp(),      */
                                   /* memcpy(), strerror(), strlen()     */
#include <stdlib.h>                /* atoi(), calloc(), malloc(), free(),*/
                                   /* exit(), abort()                    */
#include <time.h>                  /* time(), strftime()                 */
#include <sys/types.h>
#ifdef WITH_INOTIFY
# include <sys/inotify.h>
#endif
#ifdef HAVE_WAIT4
# include <sys/resource.h>         /* struct rusage                      */
#endif
#include <sys/wait.h>              /* waitpid()                          */
#ifdef HAVE_STATX
# include <sys/sysmacros.h>        /* makedev()                          */
#endif
#include <sys/stat.h>
#include <sys/time.h>              /* struct timeval                     */
#ifdef HAVE_MMAP
# include <sys/mman.h>             /* munmap()                           */
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef WITH_MEMCHECK
# include <mcheck.h>
#endif
#include <unistd.h>                /* fork(), rmdir(), getuid(),         */
                                   /* getgid(), sysconf()                */
#include <dirent.h>                /* opendir(), closedir(), readdir(),  */
                                   /* DIR, struct dirent                 */
#include <setjmp.h>                /* sigsetjmp(), siglongjmp()          */
#include <signal.h>                /* signal()                           */
#include <errno.h>
#ifdef _WITH_PTHREAD
# include <pthread.h>
#endif
#include "amgdefs.h"
#include "version.h"


/* Global variables. */
int                        afd_file_dir_length,
                           afd_status_fd,
                           alfbl, /* Additional locked file buffer length */
                           alfc,  /* Additional locked file counter       */
                           dcpl_fd = -1,
                           event_log_fd = STDERR_FILENO,
#ifdef HAVE_SETPRIORITY
                           add_afd_priority = DEFAULT_ADD_AFD_PRIORITY_DEF,
                           current_priority = 0,
                           exec_base_priority = NO_PRIORITY,
                           max_sched_priority = DEFAULT_MAX_NICE_VALUE,
                           min_sched_priority = DEFAULT_MIN_NICE_VALUE,
#endif
                           force_check = NO,
                           fra_id,         /* ID of FRA.                 */
                           fra_fd = -1,    /* Needed by fra_attach().    */
                           fsa_id,         /* ID of FSA.                 */
                           fsa_fd = -1,    /* Needed by fsa_attach().    */
#ifdef HAVE_HW_CRC32
                           have_hw_crc32,
#endif
#ifdef WITH_INOTIFY
                           inotify_fd,
                           no_of_inotify_dirs,
#endif
#ifdef _MAINTAINER_LOG
                           maintainer_log_fd = STDERR_FILENO,
#endif
                           max_process = MAX_NO_OF_DIR_CHECKS,
                           msg_fifo_fd,
                           no_of_dirs = 0,
                           no_fork_jobs,
                           no_of_hosts = 0,
                           no_of_orphaned_procs,
                           *no_of_process,
                           *no_of_file_masks,
                           *no_msg_buffered,
                           no_of_time_jobs,
#ifdef MULTI_FS_SUPPORT
                           no_of_extra_work_dirs,
#else
                           outgoing_file_dir_length,
#endif
                           mb_fd,
                           fd_cmd_fd,
                           full_scan_timeout,
                           one_dir_copy_timeout,
#ifndef _WITH_PTHREAD
                           dir_check_timeout,
#endif
                           no_of_bc_entries,
                           no_of_rc_entries,
                           no_of_jobs,
                           no_of_local_dirs,/* No. of directories in the */
                                            /* DIR_CONFIG file that are  */
                                            /* local.                    */
                           *amg_counter,
                           amg_counter_fd,  /* File descriptor for AMG   */
                                            /* counter file.             */
                           fin_fd,
                           no_of_rule_headers = 0,
#ifdef _PRODUCTION_LOG
                           production_log_fd = STDERR_FILENO,
#endif
#ifdef WITHOUT_FIFO_RW_SUPPORT
                           dc_cmd_writefd,
                           dc_resp_readfd,
                           del_time_job_writefd,
                           fin_writefd,
                           msg_fifo_readfd,
                           receive_log_readfd,
#endif
                           receive_log_fd = STDERR_FILENO,
                           sys_log_fd = STDERR_FILENO,
                           *time_job_list = NULL;
unsigned int               default_age_limit,
                           force_reread_interval;
mode_t                     default_create_source_dir_mode = 0;
time_t                     default_exec_timeout;
clock_t                    clktck;
off_t                      amg_data_size;
pid_t                      *opl;
#ifdef HAVE_MMAP
off_t                      fra_size,
                           fsa_size;
#endif
#ifdef _WITH_PTHREAD
pthread_mutex_t            fsa_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t                  *thread;
#else
unsigned int               max_file_buffer;
time_t                     *file_mtime_pool;
off_t                      *file_size_buffer = NULL,
                           *file_size_pool;
#endif
#ifdef _POSIX_SAVED_IDS
int                        no_of_sgids;
uid_t                      afd_uid;
gid_t                      afd_gid,
                           *afd_sgids;
#endif
char                       *afd_file_dir,
                           *alfiles = NULL, /* Additional locked files */
                           *bul_file = NULL,
#ifndef _WITH_PTHREAD
                           *file_name_buffer = NULL,
                           **file_name_pool,
#endif
                           first_time = YES,
#ifdef LINUX
                           hardlinks_protected = NEITHER,
#endif
#ifndef MULTI_FS_SUPPORT
                           outgoing_file_dir[MAX_PATH_LENGTH],
                           *p_time_dir_id,
                           time_dir[MAX_PATH_LENGTH],
#endif
                           *p_mmap = NULL,
                           *p_work_dir,
                           *rep_file = NULL;
#ifndef _WITH_PTHREAD
unsigned char              *file_length_pool;
#endif
struct dc_proc_list        *dcpl;      /* Dir Check Process List.*/
struct directory_entry     *de;
struct instant_db          *db = NULL;
#ifdef MULTI_FS_SUPPORT
struct extra_work_dirs     *ewl;
#endif
struct filetransfer_status *fsa;
struct fileretrieve_status *fra,
                           *p_fra;
struct afd_status          *p_afd_status;
struct rule                *rule;
struct message_buf         *mb;
#ifdef WITH_INOTIFY
struct inotify_watch_list  *iwl = NULL;
#endif
struct fork_job_data       *fjd = NULL;
struct wmo_bul_list        *bcdb = NULL; /* Bulletin Configuration Database */
struct wmo_rep_list        *rcdb = NULL; /* Bulletin Configuration Database */
#ifdef _DELETE_LOG
struct delete_log          dl;
#endif
#ifdef _WITH_PTHREAD
struct data_t              *p_data;
#endif
#ifdef _INPUT_LOG
int                        il_fd,
                           *il_unique_number;
unsigned int               *il_dir_number;
size_t                     il_size;
off_t                      *il_file_size;
time_t                     *il_time;
char                       *il_file_name,
                           *il_data;
#endif /* _INPUT_LOG */
#ifdef _DISTRIBUTION_LOG
unsigned int               max_jobs_per_file;
struct file_dist_list      **file_dist_pool;
#endif
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local variables. */
static int                 in_child = NO;
static sigjmp_buf          env_alrm;

/* Local function prototypes. */
#ifdef _WITH_PTHREAD
static void                *do_one_dir(void *);
#endif
static void                add_to_proc_stat(unsigned int),
                           check_fifo(int, int),
                           check_orphaned_procs(time_t),
                           check_pool_dir(time_t),
                           sig_alarm(int),
#ifdef WITH_SYSTEMD
                           sig_exit(int),
                           terminate_subprocess(void),
#endif
                           sig_handler(int);
static pid_t               get_one_zombie(pid_t, time_t);
static int                 get_process_pos(pid_t),
#ifdef _WITH_PTHREAD
                           handle_dir(int, time_t *, char *, char *, off_t *, time_t *, char **);
#else
# ifdef WITH_INOTIFY
                           handle_dir(int, time_t *, char *, char *, struct inotify_watch_list *, int *);
# else
                           handle_dir(int, time_t *, char *, char *, int *);
# endif
#endif

/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int              check_time = YES,
                    del_time_job_fd,
                    fifo_size,
                    i,
#ifdef WITH_INOTIFY
                    ievent_buf_length,
#endif
                    last_fdc_pos = 0,
                    last_fpdc_pos = 0,
                    max_fd,               /* Largest file descriptor.    */
                    n,
#ifdef _WITH_PTHREAD
                    rtn,
#else
                    fdc = 0,
                    fpdc,
                    *full_dir,
                    *full_paused_dir,
#endif
                    read_fd,
                    status,
                    write_fd;
   unsigned int     average_diff_time = 0,
#ifdef MAX_DIFF_TIME
                    max_diff_time_counter = 0,
#endif
                    no_of_dir_searches = 0;
   time_t           diff_time,
                    max_diff_time = 0L,
                    max_diff_time_time = 0L,
                    next_dir_check_time,
#ifdef WITH_INOTIFY
                    last_dir_scan_time = 0L,
                    next_inotify_unknown_file_time,
#endif
                    next_rename_rule_check_time,
                    next_report_time,
                    next_search_time,
                    next_time_check,
                    now,
                    rescan_time = DEFAULT_RESCAN_TIME,
                    sleep_time;
   char             *fifo_buffer,
#ifdef WITH_INOTIFY
                    *ibuffer,
#endif
#ifndef _WITH_PTHREAD
                    *p_paused_host,
#endif
                    work_dir[MAX_PATH_LENGTH];
   fd_set           rset;
#ifdef _WITH_PTHREAD
   void             *statusp;
#endif
#ifdef SA_FULLDUMP
   struct sigaction sact;
#endif
   struct timeval   timeout;

#ifdef WITH_MEMCHECK
   mtrace();
#endif
   CHECK_FOR_VERSION(argc, argv);

   (void)umask(0);
   p_work_dir = work_dir;
   init_dir_check(argc, argv, &rescan_time,
#ifdef WITH_ONETIME
                  &ot_job_fd,
# ifdef WITHOUT_FIFO_RW_SUPPORT
                  &ot_job_readfd,
# endif
#endif
#ifdef WITH_INOTIFY
                  &ievent_buf_length,
#endif
                  &read_fd, &write_fd, &del_time_job_fd);

#ifdef SA_FULLDUMP
   /*
    * When dumping core ensure we do a FULL core dump!
    */
   sact.sa_handler = SIG_DFL;
   sact.sa_flags = SA_FULLDUMP;
   sigemptyset(&sact.sa_mask);
   if (sigaction(SIGSEGV, &sact, NULL) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "sigaction() error : %s", strerror(errno));
      exit(INCORRECT);
   }
#endif

   if ((signal(SIGSEGV, sig_handler) == SIG_ERR) ||
#ifdef WITH_SYSTEMD
       (signal(SIGINT, sig_exit) == SIG_ERR) ||
#endif
       (signal(SIGBUS, sig_handler) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not set signal handler : %s", strerror(errno));
      exit(INCORRECT);
   }

#ifdef _INPUT_LOG
   n = sizeof(off_t);
   if (sizeof(time_t) > n)
   {
      n = sizeof(time_t);
   }
   if (sizeof(unsigned int) > n)
   {
      n = sizeof(unsigned int);
   }
   il_size = n + n + n + n + MAX_FILENAME_LENGTH + sizeof(char);
   if ((il_data = malloc(il_size)) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() %d bytes : %s", il_size, strerror(errno));
      exit(INCORRECT);
   }
   il_size = n + n + n + n + sizeof(char);
   /* NOTE: + sizeof(char) is for '\0' at end of file name! */
   il_file_size = (off_t *)(il_data);
   il_time = (time_t *)(il_data + n);
   il_dir_number = (unsigned int *)(il_data + n + n);
   il_unique_number = (int *)(il_data + n + n + n);
   il_file_name = (char *)(il_data + n + n + n + n);
#endif /* _INPUT_LOG */

#ifndef _WITH_PTHREAD
   if (((full_dir = malloc((no_of_local_dirs * sizeof(int)))) == NULL) ||
       ((full_paused_dir = malloc((no_of_local_dirs * sizeof(int)))) == NULL))
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      exit(INCORRECT);
   }
#endif
#ifdef WITH_INOTIFY
   if (no_of_inotify_dirs > 0)
   {
      if ((ibuffer = malloc(ievent_buf_length)) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to malloc() %d bytes : %s",
                    ievent_buf_length, strerror(errno));
         exit(INCORRECT);
      }
   }
   else
   {
      ibuffer = NULL;
   }
#endif

   /*
    * Determine the size of the fifo buffer. Then create a buffer
    * large enough to hold the data from a fifo.
    */
   if ((i = (int)fpathconf(fin_fd, _PC_PIPE_BUF)) < 0)
   {
      /* If we cannot determine the size of the fifo set default value. */
      fifo_size = DEFAULT_FIFO_SIZE;                                     
   }
   else
   {
      fifo_size = i;
   }

   /* Allocate a buffer for reading data from FIFO's. */
   if ((fifo_buffer = malloc(fifo_size)) == NULL)       
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() %d bytes : %s",
                 fifo_size, strerror(errno));                   
      exit(INCORRECT);
   }
#ifdef _DISTRIBUTION_LOG
   init_dis_log();
#endif

   if ((clktck = sysconf(_SC_CLK_TCK)) <= 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not get clock ticks per second : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Find largest file descriptor. */
   max_fd = del_time_job_fd;
   if (read_fd > max_fd)
   {
      max_fd = read_fd;
   }
   if (fin_fd > max_fd)
   {
      max_fd = fin_fd;
   }
#ifdef WITH_ONETIME
   if (ot_job_fd > max_fd)
   {
      max_fd = ot_job_fd;
   }
#endif
#ifdef WITH_INOTIFY
   if ((inotify_fd != -1) && (inotify_fd > max_fd))
   {
      max_fd = inotify_fd;
   }
#endif
   max_fd++;

   now = time(NULL);
   next_time_check = (now / TIME_CHECK_INTERVAL) *
                      TIME_CHECK_INTERVAL + TIME_CHECK_INTERVAL;
   next_search_time = (now / OLD_FILE_SEARCH_INTERVAL) *
                      OLD_FILE_SEARCH_INTERVAL + OLD_FILE_SEARCH_INTERVAL;
   next_rename_rule_check_time = (now / READ_RULES_INTERVAL) *
                                 READ_RULES_INTERVAL + READ_RULES_INTERVAL;
   next_report_time = (now / REPORT_DIR_TIME_INTERVAL) *
                      REPORT_DIR_TIME_INTERVAL + REPORT_DIR_TIME_INTERVAL;
   next_dir_check_time = (now / DIR_CHECK_TIME) * DIR_CHECK_TIME +
                         DIR_CHECK_TIME;
#ifdef WITH_INOTIFY
   if (no_of_inotify_dirs > 0)
   {
      next_inotify_unknown_file_time = (now / DEL_UNK_INOTIFY_FILE_TIME) *
                                       DEL_UNK_INOTIFY_FILE_TIME +
                                       DEL_UNK_INOTIFY_FILE_TIME;
   }
#endif
#ifdef HAVE_HW_CRC32
   have_hw_crc32 = detect_cpu_crc32();
#endif

   /* Tell user we are starting dir_check. */
   system_log(INFO_SIGN, NULL, 0, "Starting %s (%s)",
              DIR_CHECK, PACKAGE_VERSION);

   system_log(DEBUG_SIGN, NULL, 0, "%s: Number of time jobs   : %d",
              DC_PROC_NAME, no_of_time_jobs);
   if (no_of_time_jobs > 0)
   {
      size_t length = 0;
      char   buffer[80];

      /* Show which jobs are time jobs. */
      for (i = 0; i < no_of_time_jobs; i++)
      {
         length += snprintf(&buffer[length], 80 - length, "#%x ",
                            db[time_job_list[i]].job_id);
         if (length > 51)
         {
            system_log(DEBUG_SIGN, NULL, 0, "%s", buffer);
            length = 0;
         }
      }
      if (length != 0)
      {
         system_log(DEBUG_SIGN, NULL, 0, "%s", buffer);
      }
   }
   if (force_reread_interval)
   {
      system_log(DEBUG_SIGN, NULL, 0, "%s: Force reread interval : %u seconds",
                 DC_PROC_NAME, force_reread_interval);
   }
#ifdef WITH_INOTIFY
   system_log(DEBUG_SIGN, NULL, 0, "%s: Inotfiy directories   : %d",
              DC_PROC_NAME, no_of_inotify_dirs);
#endif
#ifdef MULTI_FS_SUPPORT
   if (no_of_extra_work_dirs > 1)
   {
      system_log(DEBUG_SIGN, NULL, 0, "%s: Extra filesystem dirs : %d",
                 DC_PROC_NAME, no_of_extra_work_dirs - 1);
      for (i = 1; i < no_of_extra_work_dirs; i++)
      {
         system_log(DEBUG_SIGN, NULL, 0, "   %s (%xh)",
                    ewl[i].dir_name, ewl[i].dev);
      }
   }
#endif

   /*
    * Before we start lets make sure that there are no old
    * jobs in the pool directory.
    */
   check_pool_dir(now);

   /*
    * If needed read bulletin and report configuration tables.
    */
   if ((bul_file != NULL) && (rep_file != NULL))
   {
      eval_bul_rep_config(bul_file, rep_file, YES);
   }

   /*
    * The following loop checks all user directories for new
    * files to arrive. When we fork to copy files from directories
    * not in the same file system as the AFD, watch the fin_fd
    * to see when the child has done its job.
    */
   for (;;)
   {
      if (check_time == NO)
      {
         check_time = YES;
      }
      else
      {
         now = time(NULL);
      }
      if (now >= next_rename_rule_check_time)
      {
         get_rename_rules(YES);
         if (no_of_orphaned_procs > 0)
         {
            check_orphaned_procs(now);
         }
         if ((bul_file != NULL) && (rep_file != NULL))
         {
            eval_bul_rep_config(bul_file, rep_file, YES);
         }
         next_rename_rule_check_time = (now / READ_RULES_INTERVAL) *
                                       READ_RULES_INTERVAL + READ_RULES_INTERVAL;
      }
      if (now >= next_search_time)
      {
         while (get_one_zombie(-1, now) > 0)
         {
            /* Do nothing. */;
         }
         search_old_files(now);
         next_search_time = (time(&now) / OLD_FILE_SEARCH_INTERVAL) *
                            OLD_FILE_SEARCH_INTERVAL + OLD_FILE_SEARCH_INTERVAL;
      }
      if (now >= next_time_check)
      {
         handle_time_jobs(now);
         next_time_check = (time(&now) / TIME_CHECK_INTERVAL) *
                            TIME_CHECK_INTERVAL + TIME_CHECK_INTERVAL;
      }
      if ((p_afd_status->fd == ON) &&
          ((force_check == YES) || (now >= next_dir_check_time)))
      {
#ifdef MULTI_FS_SUPPORT
         for (i = 0; i < no_of_extra_work_dirs; i++)
         {
            if (ewl[i].dir_name != NULL)
            {
               check_file_dir(time(NULL), ewl[i].dev,
                              ewl[i].outgoing_file_dir,
                              ewl[i].outgoing_file_dir_length);
            }
         }
#else
         check_file_dir(now, outgoing_file_dir, outgoing_file_dir_length);
#endif
         next_dir_check_time = ((time(&now) / DIR_CHECK_TIME) * DIR_CHECK_TIME) +
                               DIR_CHECK_TIME;
         force_check = NO;
      }
#ifdef WITH_INOTIFY
      if ((no_of_inotify_dirs > 0) && (now >= next_inotify_unknown_file_time))
      {
         del_unknown_inotify_files(now);
         next_inotify_unknown_file_time = ((time(&now) / DEL_UNK_INOTIFY_FILE_TIME) * DEL_UNK_INOTIFY_FILE_TIME) +
                                          DEL_UNK_INOTIFY_FILE_TIME;
      }
#endif
      if (now >= next_report_time)
      {
#ifdef MAX_DIFF_TIME
         if (max_diff_time > MAX_DIFF_TIME)
         {
#endif
            char time_str[10];

            average_diff_time = average_diff_time / no_of_dir_searches;
            (void)strftime(time_str, 10, "%H:%M:%S",
                           localtime(&max_diff_time_time));
#ifdef MAX_DIFF_TIME
            system_log(DEBUG_SIGN, NULL, 0,
                       "Directory search times for %d dirs AVG: %u COUNT: %u MAX: %ld (at %s) SEARCHES: %u",
                       no_of_local_dirs, average_diff_time,
                       max_diff_time_counter, max_diff_time, time_str,
                       no_of_dir_searches);
#else
            system_log(DEBUG_SIGN, NULL, 0,
                       "Directory search times for %d dirs AVG: %u MAX: %ld (at %s) SEARCHES: %u",
                       no_of_local_dirs, average_diff_time,
                       max_diff_time, time_str, no_of_dir_searches);
#endif
#ifdef MAX_DIFF_TIME
         }
#endif
         average_diff_time = 0;
         max_diff_time_counter = 0;
         max_diff_time = max_diff_time_time = 0L;
         no_of_dir_searches = 0;
         next_report_time = (now / REPORT_DIR_TIME_INTERVAL) *
                            REPORT_DIR_TIME_INTERVAL +
                            REPORT_DIR_TIME_INTERVAL;
      }
      if (first_time == YES)
      {
         sleep_time = 0L;
         first_time = NO;
      }
      else
      {
         sleep_time = ((now / rescan_time) * rescan_time) + rescan_time - now;
#ifdef WITH_INOTIFY
         if ((inotify_fd != -1) && ((now - last_dir_scan_time) > rescan_time))
         {
            sleep_time = 0L;
         }
#endif
      }

      /* Initialise descriptor set and timeout. */
      FD_ZERO(&rset);
      FD_SET(fin_fd, &rset);
      FD_SET(read_fd, &rset);
      FD_SET(del_time_job_fd, &rset);
#ifdef WITH_ONETIME
      FD_SET(ot_job_fd, &rset);
#endif
#ifdef WITH_INOTIFY
      if (inotify_fd != -1)
      {
         FD_SET(inotify_fd, &rset);
      }
#endif
      timeout.tv_usec = 50000L;
      timeout.tv_sec = sleep_time;

      /* Wait for message x seconds and then continue. */
      if ((status = select(max_fd, &rset, NULL, NULL, &timeout)) == -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "select() error : %s", strerror(errno));
         exit(INCORRECT);
      }

      if (status > 0)
      {
         if (FD_ISSET(read_fd, &rset))
         {
            check_fifo(read_fd, write_fd);
         }

#ifdef WITH_ONETIME
         if (FD_ISSET(ot_job_fd, &rset))
         {
            if ((n = read(ot_job_fd, fifo_buffer, fifo_size)) >= sizeof(unsigned int))
            {
               unsigned int onetime_jid;

               do
               {
                  onetime_jid = *(unsigned int *)&fifo_buffer[bytes_done];
                  handle_onetime_job(onetime_jid);
                  bytes_done += sizeof(unsigned int);
               } while ((n > bytes_done) &&
                      ((n - bytes_done) >= sizeof(unsigned int)));
              }
            if ((n > 0) && ((n - bytes_done) > 0))
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Reading garbage from fifo [%d]", (n - bytes_done));
            }
            else if (n == -1)
                 {
                    system_log(WARN_SIGN, __FILE__, __LINE__,
                               "read() error while reading from %s : %s",
                               OT_JOB_FIFO, strerror(errno));
                 }
         }
#endif

         if (FD_ISSET(fin_fd, &rset))
         {
            int bytes_done = 0,
                tmp_errno;

            if (signal(SIGALRM, sig_alarm) == SIG_ERR)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to set signal handler : %s", strerror(errno));
               exit(INCORRECT);
            }
            if (sigsetjmp(env_alrm, 1) != 0)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "read() timeout (25s), avoiding deadlock!");
               continue;
            }
            (void)alarm(25L);
            n = read(fin_fd, fifo_buffer, fifo_size);
            tmp_errno = errno;
            (void)alarm(0);

            if (n >= sizeof(pid_t))
            {
               pid_t pid;

               do
               {
                  pid = *(pid_t *)&fifo_buffer[bytes_done];
                  if (pid == -1)
                  {
                     if (check_fsa(NO, DIR_CHECK) == YES)
                     {
                        /*
                         * When edit_hc changes the order in the FSA it will
                         * also have to change the FSA. Since the database of
                         * this program depends on the FSA we have reread it.
                         * There should be no change such as a new host
                         * or a new directory entry.
                         */
                        if (create_db(NULL, write_fd) != no_of_jobs)
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      "Unexpected change in database! Terminating.");
                           exit(INCORRECT);
                        }
                     }
                  }
                  else
                  {
                     (void)get_one_zombie(pid, now);
                  }
                  bytes_done += sizeof(pid_t);
               } while ((n > bytes_done) &&
                        ((n - bytes_done) >= sizeof(pid_t)));
            }
            if ((n > 0) && ((n - bytes_done) > 0))
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Reading garbage from fifo [%d]", (n - bytes_done));
            }
            else if (n == -1)
                 {
                    system_log(WARN_SIGN, __FILE__, __LINE__,
                               "read() error while reading from %s : %s",
                               IP_FIN_FIFO, strerror(tmp_errno));
                 }
         }

#ifdef WITH_INOTIFY
         if ((inotify_fd != -1) && (FD_ISSET(inotify_fd, &rset)))
         {
            n = read(inotify_fd, ibuffer, ievent_buf_length);
            if (n == -1)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "read() error while reading inotify event : %s",
                          strerror(errno));
            }
            else if (n > 0)
                 {
                    int                  j,
                                         valid_events = 0;
                    struct inotify_event *event;

                    i = 0;
                    while (i < n)
                    {
                       event = (struct inotify_event *)&ibuffer[i];
                       if (event->name[0] != '.')
                       {
                          if (event->mask & IN_Q_OVERFLOW)
                          {
                             if (event->wd == -1)
                             {
                                /*
                                 * Now we are unable to determine the directory
                                 * where it occured.
                                 */
                                system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                           "Inotify event queue overflowed, directory unknown, so will scan all.");
                                for (j = 0; j < no_of_inotify_dirs; j++)
                                {
                                   fra[de[iwl[j].de_pos].fra_pos].dir_flag |= INOTIFY_NEEDS_SCAN;
                                }
                             }
                             else
                             {
                                for (j = 0; j < no_of_inotify_dirs; j++)
                                {
                                   if (event->wd == iwl[j].wd)
                                   {
                                      p_fra = &fra[de[iwl[j].de_pos].fra_pos];
                                      receive_log(DEBUG_SIGN, __FILE__, __LINE__, now,
                                                 "Inotify event queue for directory %s overflowed. @%x",
                                                 de[iwl[j].de_pos].dir, de[iwl[j].de_pos].dir_id);
                                      fra[de[iwl[j].de_pos].fra_pos].dir_flag |= INOTIFY_NEEDS_SCAN;
                                      j = no_of_inotify_dirs;
                                   }
                                }
                             }
                          }
                          if ((event->mask & IN_ISDIR) == 0)
                          {
                             for (j = 0; j < no_of_inotify_dirs; j++)
                             {
                                if (event->wd == iwl[j].wd)
                                {
                                   if ((iwl[j].no_of_files % INOTIFY_FL_STEP_SIZE) == 0)
                                   {
                                      size_t new_size;

                                      new_size = ((iwl[j].no_of_files / INOTIFY_FL_STEP_SIZE) + 1) *
                                                 INOTIFY_FL_STEP_SIZE * sizeof(short);
                                      if ((iwl[j].fnl = realloc(iwl[j].fnl, new_size)) == NULL)
                                      {
                                         system_log(FATAL_SIGN, __FILE__, __LINE__,
# if SIZEOF_SIZE_T == 4
                                                    "Could not reallocate memory [%d bytes] for file name length list : %s",
# else
                                                    "Could not reallocate memory [%lld bytes] for file name length list : %s",
# endif
                                                    (pri_size_t)new_size, strerror(errno));
                                         exit(INCORRECT);
                                      }
                                   }
                                   iwl[j].fnl[iwl[j].no_of_files] = strlen(event->name);
                                   if ((iwl[j].cur_fn_length + iwl[j].fnl[iwl[j].no_of_files] + 1) > iwl[j].alloc_fn_length)
                                   {
                                      iwl[j].alloc_fn_length += (iwl[j].fnl[iwl[j].no_of_files] + 1 + (10 * MAX_FILENAME_LENGTH));
                                      if ((iwl[j].file_name = realloc(iwl[j].file_name, iwl[j].alloc_fn_length)) == NULL)
                                      {
                                         system_log(FATAL_SIGN, __FILE__, __LINE__,
                                                    "Could not reallocate memory [%d bytes] for file name length list : %s",
                                                    iwl[j].alloc_fn_length, strerror(errno));
                                         exit(INCORRECT);
                                      }
                                   }
                                   (void)strcpy(&iwl[j].file_name[iwl[j].cur_fn_length], event->name);
                                   iwl[j].cur_fn_length += iwl[j].fnl[iwl[j].no_of_files] + 1;
                                   iwl[j].no_of_files++;
                                   valid_events++;
                                }
                             }
                          }
                       }
                       else if ((event->mask & IN_DELETE) &&
                                ((event->mask & IN_ISDIR) == 0))
                            {
                               for (j = 0; j < no_of_inotify_dirs; j++)
                               {
                                  if (event->wd == iwl[j].wd)
                                  {
                                     /*
                                      * Assume this is the case where for
                                      * example SFTP does a not atomic
                                      * rename. That is create and delete
                                      * event.
                                      */
                                     if ((iwl[j].no_of_files % INOTIFY_FL_STEP_SIZE) == 0)
                                     {
                                        size_t new_size;

                                        new_size = ((iwl[j].no_of_files / INOTIFY_FL_STEP_SIZE) + 1) *
                                                   INOTIFY_FL_STEP_SIZE * sizeof(short);
                                        if ((iwl[j].fnl = realloc(iwl[j].fnl, new_size)) == NULL)
                                        {
                                           system_log(FATAL_SIGN, __FILE__, __LINE__,
# if SIZEOF_SIZE_T == 4
                                                      "Could not reallocate memory [%d bytes] for file name length list : %s",
# else
                                                      "Could not reallocate memory [%lld bytes] for file name length list : %s",
# endif
                                                      (pri_size_t)new_size, strerror(errno));
                                           exit(INCORRECT);
                                        }
                                     }
                                     iwl[j].fnl[iwl[j].no_of_files] = strlen(&event->name[1]);
                                     if ((iwl[j].cur_fn_length + iwl[j].fnl[iwl[j].no_of_files] + 1) > iwl[j].alloc_fn_length)
                                     {
                                        iwl[j].alloc_fn_length += (iwl[j].fnl[iwl[j].no_of_files] + 1 + (10 * MAX_FILENAME_LENGTH));
                                        if ((iwl[j].file_name = realloc(iwl[j].file_name, iwl[j].alloc_fn_length)) == NULL)
                                        {
                                           system_log(FATAL_SIGN, __FILE__, __LINE__,
                                                      "Could not reallocate memory [%d bytes] for file name length list : %s",
                                                      iwl[j].alloc_fn_length, strerror(errno));
                                           exit(INCORRECT);
                                        }
                                     }
                                     (void)strcpy(&iwl[j].file_name[iwl[j].cur_fn_length], &event->name[1]);
                                     iwl[j].cur_fn_length += iwl[j].fnl[iwl[j].no_of_files] + 1;
                                     iwl[j].no_of_files++;
                                     valid_events++;
                                     break;
                                  }
                               }
                            }
                       i += (sizeof(struct inotify_event)) + event->len;
                    } /* while (i < n) */

                    if (valid_events > 0)
                    {
                       for (i = 0; ((i < no_of_inotify_dirs) && (valid_events > 0)); i++)
                       {
                          if (iwl[i].no_of_files > 0)
                          {
                             valid_events -= iwl[i].no_of_files;
                             if ((handle_dir(iwl[i].de_pos, NULL, NULL, NULL,
                                             &iwl[i], NULL) == YES) &&
                                 ((fra[de[iwl[i].de_pos].fra_pos].remove == YES) ||
                                  (fra[de[iwl[i].de_pos].fra_pos].stupid_mode != YES)))
                             {
                                /* Ensure that this directory is not already */
                                /* in the list.                              */
                                for (j = 0; j < fdc; j++)
                                {
                                   if (full_dir[j] == iwl[i].de_pos)
                                   {
                                      break;
                                   }
                                }
                                if ((j == fdc) && (fdc < no_of_local_dirs))
                                {
                                   full_dir[fdc] = iwl[i].de_pos;
                                   fdc++;
                                }
                             }
                          }
                       }
                    }
                 }
         }
#endif /* WITH_INOTIFY */

         if (FD_ISSET(del_time_job_fd, &rset))
         {
            /*
             * User disabled a host, all time jobs must be removed
             * for this host.
             */
            if ((n = read(del_time_job_fd, fifo_buffer, fifo_size)) > 0)
            {
               int  bytes_done = 0;
               char *p_host_name = fifo_buffer;

               do
               {
                  for (i = 0; i < no_of_time_jobs; i++)
                  {
                     if (CHECK_STRCMP(p_host_name,
                                      db[time_job_list[i]].host_alias) == 0)
                     {
#ifdef MULTI_FS_SUPPORT
                        (void)strcpy(ewl[db[time_job_list[i]].ewl_pos].p_time_dir_id,
#else
                        (void)strcpy(p_time_dir_id,
#endif
                                     db[time_job_list[i]].str_job_id);
#ifdef _DELETE_LOG
                        remove_time_dir(p_host_name,
# ifdef MULTI_FS_SUPPORT
                                        ewl[db[time_job_list[i]].ewl_pos].time_dir,
# else
                                        time_dir,
# endif
                                        NO, db[time_job_list[i]].job_id,
                                        db[time_job_list[i]].dir_id,
                                        USER_DEL, __FILE__, __LINE__);
#else
                        remove_time_dir(p_host_name,
# ifdef MULTI_FS_SUPPORT
                                        ewl[db[time_job_list[i]].ewl_pos].time_dir,
# else
                                        time_dir,
# endif
                                        NO, db[time_job_list[i]].job_id);
#endif
#ifdef MULTI_FS_SUPPORT
                        *ewl[db[time_job_list[i]].ewl_pos].p_time_dir_id = '\0';
#else
                        *p_time_dir_id = '\0';
#endif
                     }
                  }

                  while ((*p_host_name != '\0') && (bytes_done < n))
                  {
                     p_host_name++;
                     bytes_done++;
                  }
                  if ((*p_host_name == '\0') && (bytes_done < n))
                  {
                     p_host_name++;
                     bytes_done++;
                  }
               } while (n > bytes_done);
            }
         }
      }

      if (status == 0)
      {
#ifdef AFDBENCH_CONFIG
      if ((p_afd_status->amg_jobs & PAUSE_DISTRIBUTION) == 0)
      {
#endif
         char         *error_ptr;         /* Pointer showing where we */
                                          /* fail to see that the     */
                                          /* directory is available   */
                                          /* for us.                  */
         time_t       start_time = now + sleep_time;
#ifdef HAVE_STATX
         struct statx dir_stat_buf;
#else
         struct stat  dir_stat_buf;
#endif

         if (check_fsa(NO, DIR_CHECK) == YES)
         {
            /*
             * When edit_hc changes the order in the FSA it will also
             * have to change it. Since the database of this program
             * depends on the FSA we have reread the shared memory
             * section. There should be no change such as a new host
             * or a new directory entry.
             */
            if (create_db(NULL, write_fd) != no_of_jobs)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Unexpected change in database! Terminating.");
               exit(INCORRECT);
            }
         }

         /*
          * If there are messages in the queue, check if we
          * can pass them to the FD. If we don't do it here
          * the messages will be stuck in the queue until
          * a new file enters the system.
          */
         if ((p_afd_status->fd == ON) && (*no_msg_buffered > 0))
         {
            clear_msg_buffer();
         }

#ifdef _WITH_PTHREAD
         /*
          * Create a threat for each directory we have to read.
          * Lets use the peer model.
          */
         for (i = 0; i < no_of_local_dirs; i++)
         {
            if (((fra[de[i].fra_pos].dir_flag & DIR_DISABLED) == 0) &&
                ((fra[de[i].fra_pos].dir_flag & DIR_STOPPED) == 0) &&
# ifdef WITH_INOTIFY
                ((fra[de[i].fra_pos].dir_flag & INOTIFY_NEEDS_SCAN) ||
                 (fra[de[i].fra_pos].force_reread == YES) ||
                 (fra[de[i].fra_pos].force_reread == LOCAL_ONLY) ||
                 (((fra[de[i].fra_pos].dir_options & INOTIFY_RENAME) == 0) &&
                  ((fra[de[i].fra_pos].dir_options & INOTIFY_CLOSE) == 0) &&
                  ((fra[de[i].fra_pos].dir_options & INOTIFY_CREATE) == 0) &&
                  ((fra[de[i].fra_pos].dir_options & INOTIFY_DELETE) == 0) &&
                  ((fra[de[i].fra_pos].dir_options & INOTIFY_ATTRIB) == 0))) &&
# endif
                ((fra[de[i].fra_pos].fsa_pos != -1) ||
                 (fra[de[i].fra_pos].no_of_time_entries == 0) ||
                 (fra[de[i].fra_pos].next_check_time <= start_time)))
            {
               if ((rtn = pthread_create(&thread[i], NULL, do_one_dir,
                                         (void *)&p_data[i])) != 0)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "pthread_create() error : %s", strerror(rtn));
               }
            }
            else
            {
               thread[i] = NULL;
            }
         }

         for (i = 0; i < no_of_local_dirs; i++)
         {
            if (tread[i] != NULL)
            {
               int j;

               if ((rtn = pthread_join(thread[i], &statusp)) != 0)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "pthread_join() error : %s", strerror(rtn));
               }
               if (statusp == PTHREAD_CANCELED)
               {
                  system_log(INFO_SIGN, __FILE__, __LINE__,
                             "Thread has been cancelled.");
               }
               for (j = 0; j < fra[de[i].fra_pos].max_copied_files; j++)
               {
                  p_data[i].file_name_pool[j][0] = '\0';
               }

               if ((fra[de[i].fra_pos].fsa_pos == -1) &&
                   (fra[de[i].fra_pos].no_of_time_entries > 0))
               {
                  fra[de[i].fra_pos].next_check_time = calc_next_time_array(fra[de[i].fra_pos].no_of_time_entries,
                                                                            &fra[de[i].fra_pos].te[0],
# ifdef WITH_TIMEZONE
                                                                            fra[de[i].fra_pos].timezone,
# endif
                                                                            start_time,
                                                                            __FILE__, __LINE__);
               }
            }
         }

         /* Check if any process is finished. */
         if (*no_of_process > 0)
         {
            while (get_one_zombie(-1, now) > 0)
            {
               /* Do nothing. */;
            }
         }

         /*
          * When starting and all directories are full with
          * files, it will take far too long before the dir_check
          * checks if it has to stop. So lets check the fifo
          * every time we have checked a directory.
          */
         check_fifo(read_fd, write_fd);
#else /* !_WITH_PTHREAD */
         /*
          * Since it can take very long until we have travelled
          * through all directories lets always check the time
          * and ensure we do not take too long.
          */
         fpdc = 0;
         for (i = 0; i < no_of_local_dirs; i++)
         {
            if (((fra[de[i].fra_pos].dir_flag & DIR_DISABLED) == 0) &&
                ((fra[de[i].fra_pos].dir_flag & DIR_STOPPED) == 0) &&
# ifdef WITH_INOTIFY
                ((fra[de[i].fra_pos].dir_flag & INOTIFY_NEEDS_SCAN) ||
                 (fra[de[i].fra_pos].force_reread == YES) ||
                 (fra[de[i].fra_pos].force_reread == LOCAL_ONLY) ||
                 (fra[de[i].fra_pos].dir_flag & FILES_IN_QUEUE) ||
                 (((fra[de[i].fra_pos].dir_options & INOTIFY_RENAME) == 0) &&
                  ((fra[de[i].fra_pos].dir_options & INOTIFY_CLOSE) == 0) &&
                  ((fra[de[i].fra_pos].dir_options & INOTIFY_CREATE) == 0) &&
                  ((fra[de[i].fra_pos].dir_options & INOTIFY_DELETE) == 0) &&
                  ((fra[de[i].fra_pos].dir_options & INOTIFY_ATTRIB) == 0))) &&
# endif
                ((fra[de[i].fra_pos].fsa_pos != -1) ||
                 (fra[de[i].fra_pos].no_of_time_entries == 0) ||
                 (fra[de[i].fra_pos].next_check_time <= start_time)))
            {
# ifdef HAVE_STATX
               if (statx(0, de[i].dir, AT_STATX_SYNC_AS_STAT,
                         STATX_NLINK | STATX_MTIME, &dir_stat_buf) == -1)
# else
               if (stat(de[i].dir, &dir_stat_buf) == -1)
# endif
               {
                  int ret;

                  if ((errno == ENOENT) &&
                      ((fra[de[i].fra_pos].dir_mode != 0) ||
                       (default_create_source_dir_mode > 0)))
                  {
                     if ((ret = check_create_path(de[i].dir,
                                                  (default_create_source_dir_mode > 0) ? default_create_source_dir_mode : fra[de[i].fra_pos].dir_mode,
                                                  &error_ptr, YES, YES,
                                                  NULL)) == CREATED_DIR)
                     {
                        p_fra = &fra[de[i].fra_pos];
                        receive_log(INFO_SIGN, __FILE__, __LINE__, start_time,
                                    "Created source directory `%s', since it has been removed. @%x",
                                    de[i].dir, de[i].dir_id);

                        /* Since it is new, there cannot be any files. */
                        continue;
                     }
                  }
                  p_fra = &fra[de[i].fra_pos];
                  receive_log(ERROR_SIGN, __FILE__, __LINE__, start_time,
                             "Can't access directory entry %d %s : %s @%x",
                             i, de[i].dir, strerror(errno), de[i].dir_id);
                  if (fra[de[i].fra_pos].fsa_pos == -1)
                  {
                     lock_region_w(fra_fd,
# ifdef LOCK_DEBUG
                                   (char *)&fra[de[i].fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
# else
                                   (char *)&fra[de[i].fra_pos].error_counter - (char *)fra);
# endif
                     fra[de[i].fra_pos].error_counter += 1;
                     if ((fra[de[i].fra_pos].error_counter >= fra[de[i].fra_pos].max_errors) &&
                         ((fra[de[i].fra_pos].dir_flag & DIR_ERROR_SET) == 0))
                     {
                        fra[de[i].fra_pos].dir_flag |= DIR_ERROR_SET;
                        SET_DIR_STATUS(fra[de[i].fra_pos].dir_flag,
                                       now,
                                       fra[de[i].fra_pos].start_event_handle,
                                       fra[de[i].fra_pos].end_event_handle,
                                       fra[de[i].fra_pos].dir_status);
                     }
                     unlock_region(fra_fd,
# ifdef LOCK_DEBUG
                                   (char *)&fra[de[i].fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
# else
                                   (char *)&fra[de[i].fra_pos].error_counter - (char *)fra);
# endif
                  }
               }
               else
               {
                  int pdf = NO;  /* Paused dir flag. */

                  /*
                   * Handle any new files that have arrived.
                   */
                  if ((fra[de[i].fra_pos].force_reread == YES) ||
                      (fra[de[i].fra_pos].force_reread == LOCAL_ONLY) ||
                      ((force_reread_interval) &&
                       ((now - de[i].search_time) > force_reread_interval)) ||
# ifdef HAVE_STATX
                      (dir_stat_buf.stx_mtime.tv_sec >= de[i].search_time)
# else
                      (dir_stat_buf.st_mtime >= de[i].search_time)
# endif
                     )
                  {
                     /* The directory time has changed. New files */
                     /* have arrived!                             */
                     /* NOTE: Directories where we may not remove */
                     /*       are NOT counted as full. If we do   */
                     /*       so we might end up in an endless    */
                     /*       loop.                               */
# ifdef WITH_MULTI_DIR_SCANS
#  ifdef HAVE_STATX
                     if ((handle_dir(i, &dir_stat_buf.stx_mtime.tv_sec, NULL, NULL,
#  else
                     if ((handle_dir(i, &dir_stat_buf.st_mtime, NULL, NULL,
#  endif
# else
                     if ((handle_dir(i, NULL, NULL, NULL,
# endif
# ifdef WITH_INOTIFY
                                     NULL,
# endif
                                     &pdf) == YES) &&
                         ((fra[de[i].fra_pos].remove == YES) ||
                          (fra[de[i].fra_pos].stupid_mode != YES)))
                     {
                        full_dir[fdc] = i;
                        fdc++;
                     }
                  }
# ifdef REPORT_UNCHANGED_TIMESTAMP
                  else
                  {
                     p_fra = &fra[de[i].fra_pos];
                     receive_log(INFO_SIGN, NULL, 0, start_time,
                                 "Directory timestamp unchanged. @%x",
                                 de[i].dir_id);
                  }
# endif

                  /*
                   * Handle any paused hosts in this directory.
                   * We do NOT check the pdf flag here since it is
                   * very unlikely that the paused status has changed
                   * so quickly.
                   */
# ifdef HAVE_STATX
                  if (dir_stat_buf.stx_nlink > 2)
# else
                  if (dir_stat_buf.st_nlink > 2)
# endif
                  {
                     int dest_count = 0,
                         nfg = 0;

                     while ((p_paused_host = check_paused_dir(&de[i],
                                                              &nfg,
                                                              &dest_count,
                                                              &pdf)) != NULL)
                     {
                        if (handle_dir(i, &start_time, p_paused_host,
                                       NULL,
# ifdef WITH_INOTIFY
                                       NULL,
# endif
                                       NULL) == YES)
                        {
                           full_paused_dir[fpdc] = i;
                           fpdc++;
                        }
                        pdf = YES;
                     }
                  }
                  if ((pdf == NO) &&
                      (fra[de[i].fra_pos].dir_flag & FILES_IN_QUEUE) &&
                      (fra[de[i].fra_pos].dir_status != DIRECTORY_ACTIVE))
                  {
                     fra[de[i].fra_pos].dir_flag &= ~FILES_IN_QUEUE;
                     if (fra[de[i].fra_pos].files_queued > 0)
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   "Hmm, the number of files in %s [%d] should be 0 but currently is %d. Resetting.",
                                   fra[de[i].fra_pos].dir_alias,
                                   de[i].fra_pos,
                                   fra[de[i].fra_pos].files_queued);
                        fra[de[i].fra_pos].files_queued = 0;
                     }
                     if (fra[de[i].fra_pos].bytes_in_queue > 0)
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
# if SIZEOF_OFF_T == 4
                                   "Hmm, the number of bytes in %s [%d] should be 0 but currently is %ld. Resetting.",
# else
                                   "Hmm, the number of bytes in %s [%d] should be 0 but currently is %lld. Resetting.",
# endif
                                   fra[de[i].fra_pos].dir_alias,
                                   de[i].fra_pos,
                                   (pri_off_t)fra[de[i].fra_pos].bytes_in_queue);
                        fra[de[i].fra_pos].bytes_in_queue = 0;
                     }
                  }
               }

               /* Check if any process is finished. */
               if (*no_of_process > 0)
               {
                  while (get_one_zombie(-1, now) > 0)
                  {
                     /* Do nothing. */;
                  }
               }

               if ((fra[de[i].fra_pos].fsa_pos == -1) &&
                   (fra[de[i].fra_pos].no_of_time_entries > 0) &&
                   ((fdc == 0) || (full_dir[fdc - 1] != i)))
               {
                  fra[de[i].fra_pos].next_check_time = calc_next_time_array(fra[de[i].fra_pos].no_of_time_entries,
                                                                            &fra[de[i].fra_pos].te[0],
# ifdef WITH_TIMEZONE
                                                                            fra[de[i].fra_pos].timezone,
# endif
                                                                            start_time,
                                                                            __FILE__, __LINE__);
               }
            }
            if (((*(unsigned char *)((char *)fra - AFD_FEATURE_FLAG_OFFSET_END) & DISABLE_DIR_WARN_TIME) == 0) &&
                ((fra[de[i].fra_pos].dir_flag & WARN_TIME_REACHED) == 0) &&
                (fra[de[i].fra_pos].warn_time > 0) &&
                ((start_time - fra[de[i].fra_pos].last_retrieval) > fra[de[i].fra_pos].warn_time))
            {
               fra[de[i].fra_pos].dir_flag |= WARN_TIME_REACHED;
               SET_DIR_STATUS(fra[de[i].fra_pos].dir_flag,
                              now,
                              fra[de[i].fra_pos].start_event_handle,
                              fra[de[i].fra_pos].end_event_handle,
                              fra[de[i].fra_pos].dir_status);
               p_fra = &fra[de[i].fra_pos];
               receive_log(WARN_SIGN, NULL, 0, start_time,
                           "Warn time (%ld) for directory `%s' reached. @%x",
                           fra[de[i].fra_pos].warn_time, de[i].dir,
                           de[i].dir_id);
               error_action(de[i].alias, "start", DIR_WARN_ACTION,
                           receive_log_fd);
               event_log(0L, EC_DIR, ET_AUTO, EA_WARN_TIME_SET, "%s",
                         fra[de[i].fra_pos].dir_alias);
            }
            if (((*(unsigned char *)((char *)fra - AFD_FEATURE_FLAG_OFFSET_END) & DISABLE_DIR_WARN_TIME) == 0) &&
                ((fra[de[i].fra_pos].dir_flag & INFO_TIME_REACHED) == 0) &&
                (fra[de[i].fra_pos].info_time > 0) &&
                ((start_time - fra[de[i].fra_pos].last_retrieval) > fra[de[i].fra_pos].info_time))
            {
               fra[de[i].fra_pos].dir_flag |= INFO_TIME_REACHED;
               SET_DIR_STATUS(fra[de[i].fra_pos].dir_flag,
                              now,
                              fra[de[i].fra_pos].start_event_handle,
                              fra[de[i].fra_pos].end_event_handle,
                              fra[de[i].fra_pos].dir_status);
               p_fra = &fra[de[i].fra_pos];
               receive_log(INFO_SIGN, NULL, 0, start_time,
                           "Info time (%ld) for directory `%s' reached.",
                           fra[de[i].fra_pos].info_time, de[i].dir);
               error_action(de[i].alias, "start", DIR_INFO_ACTION,
                            receive_log_fd);
               event_log(0L, EC_DIR, ET_AUTO, EA_INFO_TIME_SET, "%s",
                         fra[de[i].fra_pos].dir_alias);
            }
         } /* for (i = 0; i < no_of_local_dirs; i++) */

         /* Check if time went backwards. */
         now = time(NULL);
         if ((now < start_time) && ((start_time - now) > 0))
         {
            for (i = 0; i < no_of_local_dirs; i++)
            {
               if (de[i].search_time > now)
               {
                  de[i].search_time = now - 1;
               }
            }
            system_log(((start_time - now) > 5) ? WARN_SIGN : DEBUG_SIGN,
                       __FILE__, __LINE__,
                       "Time went backwards %d seconds.",
                       (int)(start_time - now));
         }

         diff_time = now - start_time;
         if (diff_time > max_diff_time)
         {
            max_diff_time = diff_time;
            max_diff_time_time = now;
         }
# ifdef MAX_DIFF_TIME
         if (diff_time >= MAX_DIFF_TIME)
         {
            max_diff_time_counter++;
         }
# endif
         average_diff_time += diff_time;
         no_of_dir_searches++;
         if ((fdc == 0) && (fpdc == 0))
         {
            check_time = NO;
         }
         else
         {
            now = time(NULL);
            diff_time = now - start_time;
            if ((full_scan_timeout == 0) ||
                (diff_time < full_scan_timeout))
            {
               int j,
                   ret;

               while (fdc > 0)
               {
                  now = time(NULL);
                  diff_time = now - start_time;

                  /*
                   * When starting and all directories are full with
                   * files, it will take far too long before dir_check
                   * checks if it has to stop. So lets check the fifo
                   * every time we have checked a directory.
                   */
                  if (diff_time > 5)
                  {
                     check_fifo(read_fd, write_fd);
                  }

                  /*
                   * Now lets check all those directories that
                   * still have files but we stopped the
                   * handling for this directory because of
                   * a certain limit.
                   */
                  for (i = last_fdc_pos; i < fdc; i++)
                  {
                     now = time(NULL);
                     do
                     {
                        if ((ret = handle_dir(full_dir[i], NULL, NULL, NULL,
# ifdef WITH_INOTIFY
                                              NULL,
# endif
                                              NULL)) == NO)
                        {
                           if (fra[de[full_dir[i]].fra_pos].dir_flag & MAX_COPIED)
                           {
                              fra[de[full_dir[i]].fra_pos].dir_flag ^= MAX_COPIED;
                           }
                           if (i < fdc)
                           {
                              (void)memmove(&full_dir[i], &full_dir[i + 1],
                                            ((fdc - i) * sizeof(int)));
                           }
                           fdc--;
                           i--;
                        }
                        diff_time = time(NULL) - now;
                     } while ((ret == YES) &&
                              (diff_time < one_dir_copy_timeout) &&
                              ((full_scan_timeout == 0) ||
                               (diff_time < full_scan_timeout)));
                     if ((full_scan_timeout != 0) &&
                         (diff_time >= full_scan_timeout))
                     {
                        last_fdc_pos = i;
                        for (i = 0; i < fdc; i++)
                        {
                           if ((fra[de[full_dir[i]].fra_pos].fsa_pos == -1) &&
                               (fra[de[full_dir[i]].fra_pos].no_of_time_entries > 0))
                           {
                              fra[de[full_dir[i]].fra_pos].next_check_time = now - 5;
                              de[full_dir[i]].search_time = 0;
                           }
                        }
                        fdc = 0;
                     }
                     else
                     {
                        if ((i > -1) && (fdc > 0))
                        {
                           if ((fra[de[full_dir[i]].fra_pos].fsa_pos == -1) &&
                               (fra[de[full_dir[i]].fra_pos].no_of_time_entries > 0))
                           {
                              fra[de[full_dir[i]].fra_pos].next_check_time = now - 5;
                              de[full_dir[i]].search_time = 0;
                           }
                        }
                        if ((diff_time >= one_dir_copy_timeout) &&
                            (ret == YES))
                        {
                           first_time = YES;
                           if (i < fdc)
                           {
                              (void)memmove(&full_dir[i],
                                            &full_dir[i + 1],
                                            ((fdc - i) * sizeof(int)));
                           }
                           fdc--;
                           i--;
                        }
                     }
                  }
                  for (j = i; j < last_fdc_pos; j++)
                  {
                     now = time(NULL);
                     do
                     {
                        if ((ret = handle_dir(full_dir[j], NULL, NULL, NULL,
# ifdef WITH_INOTIFY
                                              NULL,
# endif
                                              NULL)) == NO)
                        {
                           if (fra[de[full_dir[j]].fra_pos].dir_flag & MAX_COPIED)
                           {
                              fra[de[full_dir[j]].fra_pos].dir_flag ^= MAX_COPIED;
                           }
                           if (j < fdc)
                           {
                              (void)memmove(&full_dir[j],
                                            &full_dir[j + 1],
                                            ((fdc - j) * sizeof(int)));
                           }
                           fdc--;
                           j--;
                        }
                        diff_time = time(NULL) - now;
                     } while ((ret == YES) &&
                              (diff_time < one_dir_copy_timeout) &&
                              ((full_scan_timeout == 0) ||
                               (diff_time < full_scan_timeout)));
                     if ((full_scan_timeout != 0) &&
                         (diff_time >= full_scan_timeout))
                     {
                        last_fdc_pos = j;
                        for (j = 0; j < fdc; j++)
                        {
                           if ((fra[de[full_dir[j]].fra_pos].fsa_pos == -1) &&
                               (fra[de[full_dir[j]].fra_pos].no_of_time_entries > 0))
                           {
                              fra[de[full_dir[j]].fra_pos].next_check_time = 0;
                              de[full_dir[j]].search_time = 0;
                           }
                        }
                        fdc = 0;
                     }
                     else
                     {
                        if ((j > -1) && (fdc > 0))
                        {
                           if ((fra[de[full_dir[j]].fra_pos].fsa_pos == -1) &&
                               (fra[de[full_dir[j]].fra_pos].no_of_time_entries > 0))
                           {
                              fra[de[full_dir[j]].fra_pos].next_check_time = 0;
                              de[full_dir[j]].search_time = 0;
                           }
                        }
                        if ((diff_time >= one_dir_copy_timeout) && (ret == YES))
                        {
                           first_time = YES;
                           if (j < fdc)
                           {
                              (void)memmove(&full_dir[j],
                                            &full_dir[j + 1],
                                            ((fdc - j) * sizeof(int)));
                           }
                           fdc--;
                           j--;
                        }
                     }
                  }
               } /* while (fdc > 0) */

               if ((fdc == 0) && ((full_scan_timeout == 0) ||
                   (diff_time < full_scan_timeout)))
               {
                  last_fdc_pos = 0;
               }

               if ((full_scan_timeout == 0) ||
                   (diff_time < full_scan_timeout))
               {
                  while (fpdc > 0)
                  {
                     now = time(NULL);
                     diff_time = now - start_time;

                     /*
                      * When starting and all directories are full with
                      * files, it will take far too long before dir_check
                      * checks if it has to stop. So lets check the fifo
                      * every time we have checked a directory.
                      */
                     if (diff_time > 5)
                     {
                        check_fifo(read_fd, write_fd);
                     }

                     /*
                      * Now lets check all those directories that
                      * still have files but we stopped the
                      * handling for this directory because of
                      * a certain limit.
                      */
                     for (i = last_fpdc_pos; i < fpdc; i++)
                     {
                        now = time(NULL);
                        do
                        {
                           if ((ret = handle_dir(full_paused_dir[i],
                                                 &now, NULL, NULL,
# ifdef WITH_INOTIFY
                                                 NULL,
# endif
                                                 NULL)) == NO)
                           {
                              if (i < fpdc)
                              {
                                 (void)memmove(&full_paused_dir[i],
                                               &full_paused_dir[i + 1],
                                               ((fpdc - i) * sizeof(int)));
                              }
                              fpdc--;
                              i--;
                           }
                           diff_time = time(NULL) - now;
                        } while ((ret == YES) &&
                                 (diff_time < one_dir_copy_timeout) &&
                                 ((full_scan_timeout == 0) ||
                                  (diff_time < full_scan_timeout)));
                        if ((full_scan_timeout != 0) &&
                            (diff_time >= full_scan_timeout))
                        {
                           fpdc = 0;
                           last_fpdc_pos = i;
                        }
                        else
                        {
                           if ((diff_time >= one_dir_copy_timeout) &&
                               (ret == YES))
                           {
                              first_time = YES;
                              if (i < fpdc)
                              {
                                 (void)memmove(&full_paused_dir[i],
                                               &full_paused_dir[i + 1],
                                               ((fpdc - i) * sizeof(int)));
                              }
                              fpdc--;
                              i--;
                           }
                        }
                     }
                     for (j = i; j < last_fpdc_pos; j++)
                     {
                        now = time(NULL);
                        do
                        {
                           if ((ret = handle_dir(full_paused_dir[j],
                                                 &now, NULL, NULL,
# ifdef WITH_INOTIFY
                                                 NULL,
# endif
                                                 NULL)) == NO)
                           {
                              if (j < fpdc)
                              {
                                 (void)memmove(&full_paused_dir[j],
                                               &full_paused_dir[j + 1],
                                               ((fpdc - j) * sizeof(int)));
                              }
                              fpdc--;
                              j--;
                           }
                           diff_time = time(NULL) - now;
                        } while ((ret == YES) &&
                                 (diff_time < one_dir_copy_timeout) &&
                                 ((full_scan_timeout == 0) ||
                                  (diff_time < full_scan_timeout)));
                        if ((full_scan_timeout != 0) &&
                            (diff_time >= full_scan_timeout))
                        {
                           fpdc = 0;
                           last_fpdc_pos = j;
                        }
                        else
                        {
                           if ((diff_time >= one_dir_copy_timeout) &&
                               (ret == YES))
                           {
                              first_time = YES;
                              if (j < fpdc)
                              {
                                 (void)memmove(&full_paused_dir[j],
                                               &full_paused_dir[j + 1],
                                               ((fpdc - j) * sizeof(int)));
                              }
                              fpdc--;
                              j--;
                           }
                        }
                     }
                  } /* while (fpdc > 0) */

                  if ((fpdc == 0) && ((full_scan_timeout == 0) ||
                      (diff_time < full_scan_timeout)))
                  {
                     last_fpdc_pos = 0;
                  }
               }
            }
            else
            {
               for (i = 0; i < fdc; i++)
               {
                  if ((fra[de[full_dir[i]].fra_pos].fsa_pos == -1) &&
                      (fra[de[full_dir[i]].fra_pos].no_of_time_entries > 0))
                  {
                     fra[de[full_dir[i]].fra_pos].next_check_time = now - 5;
                  }
               }
               fdc = 0;
            }

            /* Check if any process is finished. */
            if (*no_of_process > 0)
            {
               while (get_one_zombie(-1, now) > 0)
               {
                  /* Do nothing. */;
               }
            }
         }
# ifdef WITH_INOTIFY
         if (inotify_fd != -1)
         {
            last_dir_scan_time = time(NULL);
         }
# endif
#endif /* !_WITH_PTHREAD */
#ifdef AFDBENCH_CONFIG
      }
#endif
      }
   } /* for (;;) */

   /* Unmap from pid array. */
   if (dcpl_fd > 0)
   {
      (void)close(dcpl_fd);
   }
   if (dcpl != NULL)
   {
#ifdef HAVE_MMAP
      size_t dcpl_size;

      dcpl_size = (max_process * sizeof(struct dc_proc_list)) + AFD_WORD_OFFSET;
      if (munmap(((char *)dcpl - AFD_WORD_OFFSET), dcpl_size) == -1)
#else
      if (munmap_emu((void *)((char *)dcpl - AFD_WORD_OFFSET)) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to munmap() from %s : %s",
                    DCPL_FILE_NAME, strerror(errno));
         exit(INCORRECT);
      }
      dcpl = NULL;
   }
   if (opl != NULL)
   {
      free(opl);
      opl = NULL;
      no_of_orphaned_procs = 0;
   }
   for (i = 0; i < no_of_local_dirs; i++)
   {
      int j;

      for (j = 0; j < de[i].nfg; j++)
      {
         free(de[i].fme[j].pos);
         free(de[i].fme[j].file_mask);
      }
      free(de[i].fme);
      if (de[i].paused_dir != NULL)
      {
         free(de[i].paused_dir);
      }
   }
   free(de);

   exit(SUCCESS);
}


#ifdef _WITH_PTHREAD
/*++++++++++++++++++++++++++++ do_one_dir() +++++++++++++++++++++++++++++*/
void *
do_one_dir(void *arg)
{
   time_t        now,
                 start_time;
   char          *p_paused_host;
   struct data_t *data = (struct data_t *)arg;
# ifdef HAVE_STATX
   struct statx  dir_stat_buf;

   if (statx(0, de[data->i].dir, AT_STATX_SYNC_AS_STAT,
             STATX_MTIME | STATX_NLINK, &dir_stat_buf) < 0)
# else
   struct stat   dir_stat_buf;

   if (stat(de[data->i].dir, &dir_stat_buf) < 0)
# endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Can't access directory %s : %s",
                 de[data->i].dir, strerror(errno));
      return(NO);
   }

   start_time = time(&now);

   /*
    * Handle any new files that have arrived.
    */
   if ((fra[de[data->i].fra_pos].force_reread == YES) ||
       (fra[de[data->i].fra_pos].force_reread == LOCAL_ONLY) ||
# ifdef HAVE_STATX
       (dir_stat_buf.stx_mtime.tv_sec >= de[data->i].search_time)
# else
       (dir_stat_buf.st_mtime >= de[data->i].search_time)
# endif
      )
   {
      /* The directory time has changed. New files */
      /* have arrived!                             */
      while (handle_dir(data->i, &now, NULL, NULL,
                        data->file_size_pool, data->file_mtime_pool,
                        data->file_name_pool, data->file_length_pool) == YES)
      {
         /*
          * There are still files left in the directory
          * lets continue handling files until we reach
          * some timeout.
          */
         if ((time(&now) - start_time) > one_dir_copy_timeout)
         {
            /*
             * Since there are still files left in the
             * directory lets manipulate the variable
             * first_time so we rescan the directories
             * immediately.
             */
            first_time = YES;
            break;
         }
      }
   }

   /*
    * Handle any paused hosts in this directory.
    */
# ifdef HAVE_STATX
   if (dir_stat_buf.stx_nlink > 2)
# else
   if (dir_stat_buf.st_nlink > 2)
# endif
   {
      int dest_count = 0,
          nfg = 0;

      if ((p_paused_host = check_paused_dir(&de[data->i], &nfg,
                                            &dest_count, NULL)) != NULL)
      {
         now = time(NULL);
         while (handle_dir(data->i, &now, p_paused_host, NULL,
                           data->file_size_pool,
                           data->file_mtime_pool, data->file_name_pool,
                           data->file_length_pool) == YES)
         {
            /*
             * There are still files left in the directory
             * lets continue handling files until we reach
             * some timeout.
             */
            if ((time(&now) - start_time) > one_dir_copy_timeout)
            {
               /*
                * Since there are still files left in the
                * directory lets manipulate the variable
                * first_time so we rescan the directories
                * immediately.
                */
               first_time = YES;
               break;
            }
         }
      }
   }

   if (((*(unsigned char *)((char *)fra - AFD_FEATURE_FLAG_OFFSET_END) & DISABLE_DIR_WARN_TIME) == 0) &&
       ((fra[de[data->i].fra_pos].dir_flag & INFO_TIME_REACHED) == 0) &&
       ((start_time - fra[de[data->i].fra_pos].last_retrieval) > fra[de[data->i].fra_pos].info_time))
   {
      fra[de[data->i].fra_pos].dir_flag |= INFO_TIME_REACHED;
      SET_DIR_STATUS(fra[de[data->i].fra_pos].dir_flag,
                     now,
                     fra[de[data->i].fra_pos].start_event_handle,
                     fra[de[data->i].fra_pos].end_event_handle,
                     fra[de[data->i].fra_pos].dir_status);
      p_fra = &fra[de[data->i].fra_pos];
      receive_log(INFO_SIGN, NULL, 0, start_time,
                  "Info time (%ld) for directory `%s' reached. @%x",
                  fra[de[data->i].fra_pos].info_time, de[data->i].dir,
                  de[data->i].dir_id);
      error_action(de[i].alias, "start", DIR_INFO_ACTION, receive_log_fd);
   }
   if (((*(unsigned char *)((char *)fra - AFD_FEATURE_FLAG_OFFSET_END) & DISABLE_DIR_WARN_TIME) == 0) &&
       ((fra[de[data->i].fra_pos].dir_flag & WARN_TIME_REACHED) == 0) &&
       ((start_time - fra[de[data->i].fra_pos].last_retrieval) > fra[de[data->i].fra_pos].warn_time))
   {
      fra[de[data->i].fra_pos].dir_flag |= WARN_TIME_REACHED;
      SET_DIR_STATUS(fra[de[data->i].fra_pos].dir_flag,
                     now,
                     fra[de[data->i].fra_pos].start_event_handle,
                     fra[de[data->i].fra_pos].end_event_handle,
                     fra[de[data->i].fra_pos].dir_status);
      p_fra = &fra[de[data->i].fra_pos];
      receive_log(WARN_SIGN, NULL, 0, start_time,
                  "Warn time (%ld) for directory `%s' reached. @%x",
                  fra[de[data->i].fra_pos].warn_time, de[data->i].dir,
                  de[data->i].dir_id);
      error_action(de[i].alias, "start", DIR_WARN_ACTION, receive_log_fd);
   }

   return(NULL);
}
#endif


/*++++++++++++++++++++++++ check_pool_dir() +++++++++++++++++++++++++++++*/
static void
check_pool_dir(time_t now)
{
   char pool_dir[MAX_PATH_LENGTH];
   DIR  *dp;

   (void)snprintf(pool_dir, MAX_PATH_LENGTH, "%s%s%s",
                  p_work_dir, AFD_FILE_DIR, AFD_TMP_DIR);

   if ((dp = opendir(pool_dir)) == NULL)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to opendir() %s : %s", pool_dir, strerror(errno));
   }
   else
   {
      int           dir_counter = 0;
      char          *work_ptr;
#ifdef MULTI_FS_SUPPORT
      char          str_dev_self[MAX_INT_HEX_LENGTH + 1];
# ifdef HAVE_STATX
      struct statx  stat_buf;
# else
      struct stat   stat_buf;
# endif
#endif
      struct dirent *p_dir;

#ifdef MULTI_FS_SUPPORT
# ifdef HAVE_STATX
      if (statx(0, pool_dir, AT_STATX_SYNC_AS_STAT, 0, &stat_buf) == -1)
# else
      if (stat(pool_dir, &stat_buf) == -1)
# endif
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Failed to stat() `%s' : %s", pool_dir, strerror(errno));
         str_dev_self[0] = '\0';
      }
      else
      {
         (void)snprintf(str_dev_self, MAX_INT_HEX_LENGTH, "%x",
# ifdef HAVE_STATX
                        (unsigned int)makedev(stat_buf.stx_dev_major, stat_buf.stx_dev_minor)
# else
                        (unsigned int)stat_buf.st_dev
# endif
                       );
      }
#endif
      work_ptr = pool_dir + strlen(pool_dir);
      *(work_ptr++) = '/';
      errno = 0;
      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] != '.')
         {
            (void)strcpy(work_ptr, p_dir->d_name);
#ifdef MULTI_FS_SUPPORT
# ifdef HAVE_STATX
            if ((statx(0, pool_dir, AT_STATX_SYNC_AS_STAT | AT_SYMLINK_NOFOLLOW,
                       STATX_MODE, &stat_buf) != -1) &&
                (S_ISLNK(stat_buf.stx_mode)))
# else
            if ((lstat(pool_dir, &stat_buf) != -1) &&
                (S_ISLNK(stat_buf.st_mode)))
# endif
            {
               if (strcmp(str_dev_self, p_dir->d_name) != 0)
               {
                  DIR *dp2;

                  (void)strcat(work_ptr, "/");
                  if ((dp2 = opendir(pool_dir)) == NULL)
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "Failed to opendir() %s : %s",
                                pool_dir, strerror(errno));
                  }
                  else
                  {
                     char          *work_ptr2 = work_ptr + strlen(p_dir->d_name) + 1;
                     struct dirent *p_dir2;

                     errno = 0;
                     while ((p_dir2 = readdir(dp2)) != NULL)
                     {
                        if (p_dir2->d_name[0] != '.')
                        {
                           (void)strcpy(work_ptr2, p_dir2->d_name);
                           (void)strcat(work_ptr2, "/");

                           system_log(DEBUG_SIGN, NULL, 0,
                                      "Handle unfinished job in extra dir `%s'",
                                      pool_dir);
# ifdef _WITH_PTHREAD
                           (void)handle_dir(-1, &now, NULL, pool_dir,
                                            data->file_size_pool, data->file_mtime_pool,
                                            data->file_name_pool, data->file_length_pool);
# else
                           (void)handle_dir(-1, &now, NULL, pool_dir,
#  ifdef WITH_INOTIFY
                                            NULL,
#  endif
                                            NULL);
# endif
                           dir_counter++;
                        }
                        errno = 0;
                     }
                     work_ptr2[-1] = '\0';

                     if (errno)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "Could not readdir() %s : %s",
                                   pool_dir, strerror(errno));
                     }
                     if (closedir(dp2) == -1)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "Could not close directory %s : %s",
                                   pool_dir, strerror(errno));
                     }
                  }
               }
            }
            else
            {
#endif /* MULTI_FS_SUPPORT */
               system_log(DEBUG_SIGN, NULL, 0,
                          "Handle unfinished job in `%s'", pool_dir);
               (void)strcat(work_ptr, "/");
#ifdef _WITH_PTHREAD
               (void)handle_dir(-1, &now, NULL, pool_dir,
                                data->file_size_pool, data->file_mtime_pool,
                                data->file_name_pool, data->file_length_pool);
#else
               (void)handle_dir(-1, &now, NULL, pool_dir,
# ifdef WITH_INOTIFY
                                NULL,
# endif
                                NULL);
#endif
               dir_counter++;
#ifdef MULTI_FS_SUPPORT
            }
#endif
         }
         errno = 0;
      }
      work_ptr[-1] = '\0';

      if (errno)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not readdir() %s : %s", pool_dir, strerror(errno));
      }
      if (closedir(dp) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not close directory %s : %s",
                    pool_dir, strerror(errno));
      }
      if (dir_counter > 0)
      {
         system_log(INFO_SIGN, NULL, 0,
                    "Handled %d unfinished jobs in the pool directory.",
                    dir_counter);
      }
   }
   return;
}


/*+++++++++++++++++++++++++++ handle_dir() ++++++++++++++++++++++++++++++*/
static int
#ifdef _WITH_PTHREAD
handle_dir(int           dir_pos,
           time_t        *now,
           char          *host_name,
           char          *pool_dir,
           off_t         *file_size_pool,
           time_t        *file_mtime_pool,
           char          **file_name_pool,
           unsigned char *file_length_pool)
#else
handle_dir(int                       dir_pos,
           time_t                    *now,
           char                      *host_name,
           char                      *pool_dir,
# ifdef WITH_INOTIFY
           struct inotify_watch_list *p_iwl,
# endif
           int                       *pdf)
#endif
{
   if ((pool_dir != NULL) || /* NOTE: this must be first since if this is */
                             /*       not NULL, dir_pos is -1!!!!         */
       (fra[de[dir_pos].fra_pos].dir_flag & LINK_NO_EXEC) ||
       ((*no_of_process < max_process) &&
        ((pool_dir != NULL) ||
         (fra[de[dir_pos].fra_pos].no_of_process < fra[de[dir_pos].fra_pos].max_process))))
   {
      int        files_moved,
                 unique_number;
      off_t      file_size_linked,
                 total_file_size;
      time_t     current_time;
      char       orig_file_path[MAX_PATH_LENGTH],
                 src_file_dir[MAX_PATH_LENGTH],
                 unique_name[MAX_FILENAME_LENGTH];
#ifdef _WITH_PTHREAD
                 time_dir[MAX_PATH_LENGTH],
                 *p_time_dir_id,
#endif
#ifdef _DEBUG_THREAD
      char       debug_file[MAX_FILENAME_LENGTH];
      FILE       *p_debug_file;

      (void)snprintf(debug_file, MAX_FILENAME_LENGTH,
                     "thread-debug.%d", (int)&thread[dir_pos]);
#endif

#ifdef _WITH_PTHREAD
      (void)strcpy(time_dir, afd_file_dir);
      (void)strcat(time_dir, AFD_TIME_DIR);
      (void)strcat(time_dir, "/");
      p_time_dir_id = time_dir + strlen(time_dir);
#endif
      total_file_size = 0;

      if (pool_dir == NULL)
      {
         int rescan_dir;

         (void)strcpy(src_file_dir, de[dir_pos].dir);

         if ((host_name == NULL) &&
             (fra[de[dir_pos].fra_pos].fsa_pos != -1) &&
             (fsa[fra[de[dir_pos].fra_pos].fsa_pos].host_status & PAUSE_QUEUE_STAT))
         {
            /*
             * This is a remote directory that is paused. We just need
             * to move all the files to the paused directory.
             */
            char paused_dir[MAX_PATH_LENGTH];

            fra[de[dir_pos].fra_pos].dir_status = DIRECTORY_ACTIVE;
            if (now == NULL)
            {
               current_time = time(NULL);
            }
            else
            {
               current_time = *now;
            }
            files_moved = check_files(&de[dir_pos],
                                      src_file_dir,
                                      NO,
                                      paused_dir,
                                      PAUSED_REMOTE,
                                      &unique_number,
                                      current_time,
                                      &rescan_dir,
#ifdef _WITH_PTHREAD
                                      file_size_pool,
                                      file_mtime_pool,
                                      file_name_pool,
                                      file_length_pool,
#endif
#if defined (_MAINTAINER_LOG) && defined (SHOW_FILE_MOVING)
                                      __FILE__,
                                      __LINE__,
#endif
                                      &total_file_size);
            if (files_moved > 0)
            {
               lock_region_w(fra_fd,
#ifdef LOCK_DEBUG
                             (char *)&fra[de[dir_pos].fra_pos].files_queued - (char *)fra, __FILE__, __LINE__);
#else
                             (char *)&fra[de[dir_pos].fra_pos].files_queued - (char *)fra);
#endif
               if ((fra[de[dir_pos].fra_pos].dir_flag & FILES_IN_QUEUE) == 0)
               {
                  fra[de[dir_pos].fra_pos].dir_flag |= FILES_IN_QUEUE;
               }
               fra[de[dir_pos].fra_pos].files_queued += files_moved;
               fra[de[dir_pos].fra_pos].bytes_in_queue += total_file_size;
               unlock_region(fra_fd,
#ifdef LOCK_DEBUG
                             (char *)&fra[de[dir_pos].fra_pos].files_queued - (char *)fra, __FILE__, __LINE__);
#else
                             (char *)&fra[de[dir_pos].fra_pos].files_queued - (char *)fra);
#endif
               fra[de[dir_pos].fra_pos].files_received -= files_moved;
               fra[de[dir_pos].fra_pos].bytes_received -= total_file_size;
#ifndef _WITH_PTHREAD
               if (pdf != NULL)
               {
                  *pdf = YES;
               }
#endif
            }
            if ((pool_dir == NULL) &&
                (fra[de[dir_pos].fra_pos].no_of_process == 0) &&
                (fra[de[dir_pos].fra_pos].dir_status == DIRECTORY_ACTIVE))
            {
               SET_DIR_STATUS(fra[de[dir_pos].fra_pos].dir_flag,
                              *now,
                              fra[de[dir_pos].fra_pos].start_event_handle,
                              fra[de[dir_pos].fra_pos].end_event_handle,
                              fra[de[dir_pos].fra_pos].dir_status);
            }

            if (((files_moved >= fra[de[dir_pos].fra_pos].max_copied_files) ||
                 (total_file_size >= fra[de[dir_pos].fra_pos].max_copied_file_size)) &&
                (files_moved != INCORRECT))
            {
               return(YES);
            }
            else
            {
               return(NO);
            }
         }
         else
         {
            time_t orig_search_time;

            if (now == NULL)
            {
               current_time = time(NULL);
            }
            else
            {
               current_time = *now;
            }
            if (host_name == NULL)
            {
               orig_search_time = de[dir_pos].search_time;
               de[dir_pos].search_time = current_time;
            }
            else
            {
               (void)strcat(src_file_dir, "/.");
               (void)strcat(src_file_dir, host_name);
               orig_search_time = 0; /* Silence compiler. */
            }
            p_fra = &fra[dir_pos];

            fra[de[dir_pos].fra_pos].dir_status = DIRECTORY_ACTIVE;
            if ((host_name != NULL) && (fra[de[dir_pos].fra_pos].fsa_pos != -1))
            {
               /* Local paused directory. */
               files_moved = check_files(&de[dir_pos],
                                         src_file_dir,
                                         YES,
                                         orig_file_path,
                                         NO,
                                         &unique_number,
                                         current_time,
                                         &rescan_dir,
#ifdef _WITH_PTHREAD
                                         file_size_pool,
                                         file_mtime_pool,
                                         file_name_pool,
                                         file_length_pool,
#endif
#if defined (_MAINTAINER_LOG) && defined (SHOW_FILE_MOVING)
                                         __FILE__,
                                         __LINE__,
#endif
                                         &total_file_size);
            }
            else
            {
#ifdef WITH_INOTIFY
               if ((p_iwl == NULL) || (host_name != NULL))
               {
#endif
                  p_afd_status->dir_scans++;
                  files_moved = check_files(&de[dir_pos],
                                            src_file_dir,
                                            YES,
                                            orig_file_path,
                                            (host_name == NULL) ? YES : NO,
                                            &unique_number,
                                            current_time,
                                            &rescan_dir,
#ifdef _WITH_PTHREAD
                                            file_size_pool,
                                            file_mtime_pool,
                                            file_name_pool,
                                            file_length_pool,
#endif
#if defined (_MAINTAINER_LOG) && defined (SHOW_FILE_MOVING)
                                            __FILE__,
                                            __LINE__,
#endif
                                            &total_file_size);
#ifdef WITH_INOTIFY
               }
               else
               {
                  p_afd_status->inotify_events++;
                  files_moved = check_inotify_files(p_iwl, &de[p_iwl->de_pos],
                                                    orig_file_path,
                                                    &unique_number,
                                                    current_time,
# if defined (_MAINTAINER_LOG) && defined (SHOW_FILE_MOVING)
                                                    __FILE__,
                                                    __LINE__,
# endif
                                                    &total_file_size);
               }
#endif
               if (((files_moved == INCORRECT) || (rescan_dir == YES)) &&
                   (host_name == NULL))
               {
                  /* Set back search time otherwise we will not        */
                  /* try to rescan the directory after error recovery. */
                  de[dir_pos].search_time = orig_search_time;
               }
            }
         }
      }
      else
      {
         (void)strcpy(orig_file_path, pool_dir);
#ifdef _WITH_PTHREAD
         files_moved = count_pool_files(&dir_pos, pool_dir,
                                        file_size_pool, file_mtime_pool,
                                        file_name_pool, file_length_pool);
#else
         files_moved = count_pool_files(&dir_pos, pool_dir);
#endif
         /*
          * NOTE: If dir_pos is -1 count_pool_files() did
          *       not find the original files. Lets not
          *       set p_fra. This is actually not needed
          *       since files_moved is 0 so it will not
          *       go through any code where p_fra is needed.
          *       But just to be save.
          */
         if (dir_pos != -1)
         {
            p_fra = &fra[dir_pos];
         }
         if (now == NULL)
         {
            current_time = time(NULL);
         }
         else
         {
            current_time = *now;
         }
         unique_number = *amg_counter;
      }
      if (files_moved > 0)
      {
         int          files_linked,
                      j,
                      k;
         unsigned int split_job_counter;
#ifdef _DISTRIBUTION_LOG
         unsigned int no_of_distribution_types;
#endif

         unique_name[0] = '/';
#ifdef _DEBUG_THREAD
         if ((p_debug_file = fopen(debug_file, "a")) == NULL)
         {
            (void)fprintf(stderr, "ERROR   : Could not open %s : %s (%s %d)\n",
                          debug_file, strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         (void)fprintf(p_debug_file, "%ld %s\n", current_time, de[dir_pos].dir);
         (void)fprintf(p_debug_file, "<%s> <%s> <%s> <%s>\n", file_name_pool[0], file_name_pool[1], file_name_pool[2], file_name_pool[3]);
         (void)fprintf(p_debug_file, "files_moved = %d\n", files_moved);

         (void)fclose(p_debug_file);
#endif
         for (j = 0; j < de[dir_pos].nfg; j++)
         {
            for (k = 0; k < de[dir_pos].fme[j].dest_count; k++)
            {
#ifdef IGNORE_DUPLICATE_JOB_IDS
               if ((db[de[dir_pos].fme[j].pos[k]].job_id != 0) &&
                   ((host_name == NULL) ||
                    (CHECK_STRCMP(host_name, db[de[dir_pos].fme[j].pos[k]].host_alias) == 0)))
#else
               if ((host_name == NULL) ||
                   (CHECK_STRCMP(host_name, db[de[dir_pos].fme[j].pos[k]].host_alias) == 0))
#endif
               {
#ifdef WITH_ERROR_QUEUE
                  if (((fsa[db[de[dir_pos].fme[j].pos[k]].position].host_status & PAUSE_QUEUE_STAT) == 0) &&
                      ((fsa[db[de[dir_pos].fme[j].pos[k]].position].special_flag & HOST_DISABLED) == 0) &&
                      ((((fsa[db[de[dir_pos].fme[j].pos[k]].position].host_status & ERROR_QUEUE_SET) == 0) &&
                        ((fsa[db[de[dir_pos].fme[j].pos[k]].position].host_status & AUTO_PAUSE_QUEUE_STAT) == 0)) ||
                       ((fsa[db[de[dir_pos].fme[j].pos[k]].position].host_status & ERROR_QUEUE_SET) &&
                        (check_error_queue(db[de[dir_pos].fme[j].pos[k]].job_id, (MAX_NO_PARALLEL_JOBS + 2), 0, 0) == NO))) &&
                      ((fsa[db[de[dir_pos].fme[j].pos[k]].position].host_status & DANGER_PAUSE_QUEUE_STAT) == 0))
#else
                  if (((fsa[db[de[dir_pos].fme[j].pos[k]].position].host_status & PAUSE_QUEUE_STAT) == 0) &&
                      ((fsa[db[de[dir_pos].fme[j].pos[k]].position].host_status & AUTO_PAUSE_QUEUE_STAT) == 0) &&
                      ((fsa[db[de[dir_pos].fme[j].pos[k]].position].host_status & DANGER_PAUSE_QUEUE_STAT) == 0) &&
                      ((fsa[db[de[dir_pos].fme[j].pos[k]].position].special_flag & HOST_DISABLED) == 0))
#endif
                  {
                     if ((db[de[dir_pos].fme[j].pos[k]].time_option_type == NO_TIME) ||
                         ((db[de[dir_pos].fme[j].pos[k]].time_option_type == SEND_COLLECT_TIME) &&
                          (db[de[dir_pos].fme[j].pos[k]].next_start_time <= current_time)) ||
                         ((db[de[dir_pos].fme[j].pos[k]].time_option_type == SEND_NO_COLLECT_TIME) &&
                          (in_time(current_time,
                                   db[de[dir_pos].fme[j].pos[k]].no_of_time_entries,
                                   db[de[dir_pos].fme[j].pos[k]].te) == YES)))
                     {
                        split_job_counter = 0;
                        if ((files_linked = link_files(orig_file_path,
#ifdef MULTI_FS_SUPPORT
                                                       ewl[de[dir_pos].ewl_pos].outgoing_file_dir,
                                                       ewl[de[dir_pos].ewl_pos].outgoing_file_dir_length,
#else
                                                       outgoing_file_dir,
                                                       outgoing_file_dir_length,
#endif
                                                       current_time,
#ifdef _WITH_PTHREAD
                                                       file_size_pool,
                                                       file_mtime_pool,
                                                       file_name_pool,
                                                       file_length_pool,
#endif
#if defined (_MAINTAINER_LOG) && defined (SHOW_FILE_MOVING)
                                                       __FILE__,
                                                       __LINE__,
#endif
                                                       &de[dir_pos],
                                                       &db[de[dir_pos].fme[j].pos[k]],
                                                       &split_job_counter,
                                                       unique_number,
                                                       j,
                                                       files_moved,
                                                       &unique_name[1],
                                                       &file_size_linked)) > 0)
                        {
#ifndef _WITH_PTHREAD
                           if ((db[de[dir_pos].fme[j].pos[k]].lfs & GO_PARALLEL) &&
                               (*no_of_process < max_process))
                           {
                              int   pfd1[2],
                                    pfd2[2];
                              pid_t pid;

                              if ((pipe(pfd1) < 0) || (pipe(pfd2) < 0))
                              {
                                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                                            "pipe() error : %s", strerror(errno));
                              }

                              switch (pid = fork())
                              {
                                 case -1 : /* ERROR, process creation not possible. */

                                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                                               "Could not fork() : %s",
                                               strerror(errno));
                                    if ((close(pfd1[0]) == -1) || (close(pfd1[1]) == -1) ||
                                        (close(pfd2[0]) == -1) || (close(pfd2[1]) == -1))
                                    {
                                       system_log(WARN_SIGN, __FILE__, __LINE__,
                                                  "close() error : %s", strerror(errno));
                                    }

                                    /*
                                     * Exiting is not the right thing to do here!
                                     * Better just do what we would do if we do not
                                     * fork. This will make the handling of files
                                     * a lot slower, but better then exiting.
                                     */
# ifdef MULTI_FS_SUPPORT
                                    send_message(ewl[de[dir_pos].ewl_pos].outgoing_file_dir,
                                                 ewl[de[dir_pos].ewl_pos].dev,
# else
                                    send_message(outgoing_file_dir,
# endif
                                                 unique_name,
                                                 split_job_counter,
                                                 unique_number,
                                                 current_time,
                                                 de[dir_pos].fme[j].pos[k],
# ifdef _WITH_PTHREAD
#  ifdef _DELETE_LOG
                                                 file_size_pool,
                                                 file_name_pool,
                                                 file_length_pool,
#  endif
# endif
                                                 files_moved,
                                                 files_linked,
                                                 file_size_linked,
                                                 YES);
                                    break;

                                 case  0 : /* Child process. */
                                    {
                                       pid_t pid;

# ifdef WITH_MEMCHECK
                                       muntrace();
# endif
                                       in_child = YES;
                                       if (write(pfd2[1], "c", 1) != 1)
                                       {
                                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                     "write() error : %s", strerror(errno));
                                       }
                                       if (read(pfd1[0], &pid, 1) != 1)
                                       {
                                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                     "read() error : %s", strerror(errno));
                                       }
                                       if ((close(pfd1[0]) == -1) || (close(pfd1[1]) == -1) ||
                                           (close(pfd2[0]) == -1) || (close(pfd2[1]) == -1))
                                       {
                                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                                     "close() error : %s", strerror(errno));
                                       }
                                       if ((db[de[dir_pos].fme[j].pos[k]].lfs & SPLIT_FILE_LIST) &&
                                           (files_linked > MAX_FILES_TO_PROCESS))
                                       {
                                          int          ii,
                                                       split_files_renamed,
                                                       loops = files_linked / MAX_FILES_TO_PROCESS;
                                          unsigned int tmp_split_job_counter;
                                          off_t        split_file_size_renamed,
                                                       *tmp_file_size_buffer = NULL;
                                          char         *tmp_file_name_buffer = NULL,
                                                       tmp_unique_name[MAX_FILENAME_LENGTH],
                                                       src_file_path[MAX_PATH_LENGTH];

# ifdef MULTI_FS_SUPPORT
                                          (void)strcpy(src_file_path, ewl[de[dir_pos].ewl_pos].outgoing_file_dir);
# else
                                          (void)strcpy(src_file_path, outgoing_file_dir);
# endif
                                          (void)strcat(src_file_path, unique_name);
                                          (void)strcat(src_file_path, "/");
                                          tmp_unique_name[0] = '/';

                                          if (loops > 0)
                                          {
                                             if ((tmp_file_name_buffer = malloc(files_linked * MAX_FILENAME_LENGTH)) == NULL)
                                             {
                                                system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                           "malloc() error : %s",
                                                           strerror(errno));
                                                exit(INCORRECT);
                                             }
                                             (void)memcpy(tmp_file_name_buffer, file_name_buffer, (files_linked * MAX_FILENAME_LENGTH));

                                             if ((tmp_file_size_buffer = malloc(files_linked * sizeof(off_t))) == NULL)
                                             {
                                                system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                           "malloc() error : %s",
                                                           strerror(errno));
                                                exit(INCORRECT);
                                             }
                                             (void)memcpy(tmp_file_size_buffer, file_size_buffer, (files_linked * sizeof(off_t)));
                                          }

                                          /*
                                           * If there are lots of files in the directory,
                                           * it can take quit a while before any files get
                                           * distributed. So lets only do MAX_FILES_TO_PROCESS
                                           * at one time. The distribution of files is in
                                           * most cases what takes most the time. So always
                                           * try to start the distribution as early as
                                           * possible.
                                           */
                                          for (ii = 0; ii < loops; ii++)
                                          {
                                             if (ii > 0)
                                             {
                                                int file_offset;

                                                file_offset = ii * MAX_FILES_TO_PROCESS * MAX_FILENAME_LENGTH;

                                                /*
                                                 * It can happen that handle_options() called
                                                 * by send_message() frees file_name_buffer
                                                 * and sets it to NULL, because all files
                                                 * where deleted.
                                                 */
                                                if (file_name_buffer == NULL)
                                                {
                                                   if ((file_name_buffer = malloc((files_linked * MAX_FILENAME_LENGTH))) == NULL)
                                                   {
                                                      system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                                 "malloc() error : %s",
                                                                 strerror(errno));
                                                      exit(INCORRECT);
                                                   }
                                                }
                                                (void)memcpy(file_name_buffer, (tmp_file_name_buffer + file_offset), (MAX_FILES_TO_PROCESS * MAX_FILENAME_LENGTH));

                                                if (file_size_buffer == NULL)
                                                {
                                                   if ((file_size_buffer = malloc((files_linked * sizeof(off_t)))) == NULL)
                                                   {
                                                      system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                                 "malloc() error : %s",
                                                                 strerror(errno));
                                                      exit(INCORRECT);
                                                   }
                                                }
                                                (void)memcpy(file_size_buffer, ((char *)tmp_file_size_buffer + (ii * MAX_FILES_TO_PROCESS * sizeof(off_t))), (MAX_FILES_TO_PROCESS * sizeof(off_t)));
                                             }
                                             tmp_split_job_counter = split_job_counter + ii + 1;
                                             if ((split_files_renamed = rename_files(src_file_path,
# ifdef MULTI_FS_SUPPORT
                                                                                     ewl[de[dir_pos].ewl_pos].outgoing_file_dir,
                                                                                     ewl[de[dir_pos].ewl_pos].outgoing_file_dir_length,
# else
                                                                                     outgoing_file_dir,
                                                                                     outgoing_file_dir_length,
# endif
                                                                                     files_moved,
                                                                                     &db[de[dir_pos].fme[j].pos[k]],
                                                                                     current_time,
                                                                                     unique_number,
                                                                                     &tmp_split_job_counter,
                                                                                     &tmp_unique_name[1],
# if defined (_MAINTAINER_LOG) && defined (SHOW_FILE_MOVING)
                                                                                     __FILE__,
                                                                                     __LINE__,
# endif
                                                                                     &split_file_size_renamed)) > 0)
                                             {
# ifdef MULTI_FS_SUPPORT
                                                send_message(ewl[de[dir_pos].ewl_pos].outgoing_file_dir,
                                                             ewl[de[dir_pos].ewl_pos].dev,
# else
                                                send_message(outgoing_file_dir,
# endif
                                                             tmp_unique_name,
                                                             tmp_split_job_counter,
                                                             unique_number,
                                                             current_time,
                                                             de[dir_pos].fme[j].pos[k],
# ifdef _WITH_PTHREAD
#  ifdef _DELETE_LOG
                                                             file_size_pool,
                                                             file_name_pool,
                                                             file_length_pool,
#  endif
# endif
                                                             files_moved,
                                                             split_files_renamed,
                                                             split_file_size_renamed,
                                                             YES);
                                             } /* if (split_files_renamed > 0) */

                                             file_size_linked -= split_file_size_renamed;
                                             files_linked -= split_files_renamed;
                                          } /* for (ii = 0; ii < loops; ii++) */

                                          if (files_linked > 0)
                                          {
                                             if (loops > 0)
                                             {
                                                int file_offset;

                                                file_offset = loops * MAX_FILES_TO_PROCESS * MAX_FILENAME_LENGTH;

                                                /*
                                                 * It can happen that handle_options() called
                                                 * by send_message() frees file_name_buffer
                                                 * and sets it to NULL, because all files
                                                 * where deleted.
                                                 */
                                                if (file_name_buffer == NULL)
                                                {
                                                   if ((file_name_buffer = malloc((files_linked * MAX_FILENAME_LENGTH))) == NULL)
                                                   {
                                                      system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                                 "malloc() error : %s",
                                                                 strerror(errno));
                                                      exit(INCORRECT);
                                                   }
                                                }
                                                (void)memcpy(file_name_buffer, (tmp_file_name_buffer + file_offset), (files_linked * MAX_FILENAME_LENGTH));

                                                if (file_size_buffer == NULL)
                                                {
                                                   if ((file_size_buffer = malloc((files_linked * sizeof(off_t)))) == NULL)
                                                   {
                                                      system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                                 "malloc() error : %s",
                                                                 strerror(errno));
                                                      exit(INCORRECT);
                                                   }
                                                }
                                                (void)memcpy(file_size_buffer, ((char *)tmp_file_size_buffer + (loops * MAX_FILES_TO_PROCESS * sizeof(off_t))), (files_linked * sizeof(off_t)));
                                             }
# ifdef MULTI_FS_SUPPORT
                                             send_message(ewl[de[dir_pos].ewl_pos].outgoing_file_dir,
                                                          ewl[de[dir_pos].ewl_pos].dev,
# else
                                             send_message(outgoing_file_dir,
# endif
                                                          unique_name,
                                                          split_job_counter,
                                                          unique_number,
                                                          current_time,
                                                          de[dir_pos].fme[j].pos[k],
# ifdef _WITH_PTHREAD
#  ifdef _DELETE_LOG
                                                          file_size_pool,
                                                          file_name_pool,
                                                          file_length_pool,
#  endif
# endif
                                                          files_moved,
                                                          files_linked,
                                                          file_size_linked,
                                                          YES);
                                          }
                                          else
                                          {
                                             char fullname[MAX_PATH_LENGTH + MAX_FILENAME_LENGTH + 1];

                                             /*
                                              * It is an even number so we must
                                              * delete the last directory.
                                              */
                                             (void)snprintf(fullname,
                                                            MAX_PATH_LENGTH + MAX_FILENAME_LENGTH + 1,
                                                            "%s%s",
# ifdef MULTI_FS_SUPPORT
                                                            ewl[de[dir_pos].ewl_pos].outgoing_file_dir,
# else
                                                            outgoing_file_dir,
# endif
                                                            unique_name);
                                             if (rmdir(fullname) == -1)
                                             {
                                                if ((errno != EEXIST) && (errno != ENOTEMPTY))
                                                {
                                                   system_log(WARN_SIGN, __FILE__, __LINE__,
                                                              "Failed to rmdir() %s : %s",
                                                              fullname, strerror(errno));
                                                }
# ifdef _MAINTAINER_LOG
                                                else
                                                {
                                                   maintainer_log(WARN_SIGN, __FILE__, __LINE__,
                                                                  "Failed to rmdir() %s : %s",
                                                                  fullname, strerror(errno));
                                                }
# endif
                                             }
                                          }
                                          free(tmp_file_name_buffer);
                                          free(tmp_file_size_buffer);
                                       }
                                       else
                                       {
# ifdef MULTI_FS_SUPPORT
                                          send_message(ewl[de[dir_pos].ewl_pos].outgoing_file_dir,
                                                       ewl[de[dir_pos].ewl_pos].dev,
# else
                                          send_message(outgoing_file_dir,
# endif
                                                       unique_name,
                                                       split_job_counter,
                                                       unique_number,
                                                       current_time,
                                                       de[dir_pos].fme[j].pos[k],
# ifdef _WITH_PTHREAD
#  ifdef _DELETE_LOG
                                                       file_size_pool,
                                                       file_name_pool,
                                                       file_length_pool,
#  endif
# endif
                                                       files_moved,
                                                       files_linked,
                                                       file_size_linked,
                                                       YES);
                                       }

                                       /*
                                        * Tell parent process we have completed
                                        * the task.
                                        */
                                       pid = getpid();
# ifdef WITHOUT_FIFO_RW_SUPPORT
                                       if (write(fin_writefd, &pid, sizeof(pid_t)) != sizeof(pid_t))
# else
                                       if (write(fin_fd, &pid, sizeof(pid_t)) != sizeof(pid_t))
# endif
                                       {
                                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                     "Could not write() to fifo %s : %s",
                                                     IP_FIN_FIFO, strerror(errno));
                                       }
                                    }
                                    exit(SUCCESS);

                                 default : /* Parent process. */
                                    {
                                       char c;

                                       if (write(pfd1[1], "p", 1) != 1)
                                       {
                                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                     "write() error : %s", strerror(errno));
                                       }
                                       if (read(pfd2[0], &c, 1) != 1)
                                       {
                                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                     "read() error : %s", strerror(errno));
                                       }
                                       if ((close(pfd1[0]) == -1) || (close(pfd1[1]) == -1) ||
                                           (close(pfd2[0]) == -1) || (close(pfd2[1]) == -1))
                                       {
                                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                                     "close() error : %s", strerror(errno));
                                       }

                                       dcpl[*no_of_process].pid = pid;
                                       dcpl[*no_of_process].fra_pos = de[dir_pos].fra_pos;
# ifdef WITH_ONETIME
                                       if (onetime == YES)
                                       {
                                          dcpl[*no_of_process].job_id = ONETIME_JOB_ID;
                                       }
                                       else
                                       {
# endif
                                          dcpl[*no_of_process].job_id = db[de[dir_pos].fme[j].pos[k]].job_id;
# ifdef WITH_ONETIME
                                       }
# endif
                                       fra[de[dir_pos].fra_pos].no_of_process++;
                                       (*no_of_process)++;
                                       p_afd_status->amg_fork_counter++;
                                    }
                                    break;
                              } /* switch () */
                           }
                           else
                           {
                              if ((db[de[dir_pos].fme[j].pos[k]].lfs & GO_PARALLEL) &&
                                  (*no_of_process >= max_process))
                              {
                                 system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                            "Unable to fork() since maximum number (%d) for process dir_check reached. [Job ID = #%x]",
                                            max_process, db[de[dir_pos].fme[j].pos[k]].job_id);
                              }
#endif /* !_WITH_PTHREAD */
                              /*
                               * No need to fork() since files are in same
                               * file system and it is not necessary to split
                               * large number of files to smaller ones.
                               */
#ifdef MULTI_FS_SUPPORT
                              send_message(ewl[de[dir_pos].ewl_pos].outgoing_file_dir,
                                           ewl[de[dir_pos].ewl_pos].dev,
#else
                              send_message(outgoing_file_dir,
#endif
                                           unique_name,
                                           split_job_counter,
                                           unique_number,
                                           current_time,
                                           de[dir_pos].fme[j].pos[k],
#ifdef _WITH_PTHREAD
# ifdef _DELETE_LOG
                                           file_size_pool,
                                           file_name_pool,
                                           file_length_pool,
# endif
#endif
                                           files_moved,
                                           files_linked,
                                           file_size_linked,
                                           YES);
#ifndef _WITH_PTHREAD
                           }
#endif
                        } /* if (files_linked > 0) */
#ifndef _WITH_PTHREAD
                        else
                        {
                           if (file_name_buffer != NULL)
                           {
                              free(file_name_buffer);
                              file_name_buffer = NULL;
                           }
                           if (file_size_buffer != NULL)
                           {
                              free(file_size_buffer);
                              file_size_buffer = NULL;
                           }
                        }
#endif
                     }
                     else /* Oueue files since they are to be sent later. */
                     {
                        if ((db[de[dir_pos].fme[j].pos[k]].time_option_type == SEND_COLLECT_TIME) &&
                            ((fsa[db[de[dir_pos].fme[j].pos[k]].position].special_flag & HOST_DISABLED) == 0))
                        {
#ifdef MULTI_FS_SUPPORT
                           (void)strcpy(ewl[de[dir_pos].ewl_pos].p_time_dir_id,
#else
                           (void)strcpy(p_time_dir_id,
#endif
                                        db[de[dir_pos].fme[j].pos[k]].str_job_id);
                           if (save_files(orig_file_path,
#ifdef MULTI_FS_SUPPORT
                                          ewl[de[dir_pos].ewl_pos].time_dir,
#else
                                          time_dir,
#endif
                                          current_time,
                                          db[de[dir_pos].fme[j].pos[k]].age_limit,
#ifdef _WITH_PTHREAD
                                          file_size_pool,
                                          file_mtime_pool,
                                          file_name_pool,
                                          file_length_pool,
#endif
#if defined (_MAINTAINER_LOG) && defined (SHOW_FILE_MOVING)
                                          __FILE__,
                                          __LINE__,
#endif
                                          &de[dir_pos],
                                          &db[de[dir_pos].fme[j].pos[k]],
                                          j,
                                          files_moved,
                                          IN_SAME_FILESYSTEM,
#ifdef _DISTRIBUTION_LOG
                                          TIME_JOB_DIS_TYPE,
#endif
                                          YES) < 0)
                           {
                              system_log(ERROR_SIGN, __FILE__, __LINE__,
                                         "Failed to queue files for host %s",
                                         db[de[dir_pos].fme[j].pos[k]].host_alias);
                           }
#ifdef MULTI_FS_SUPPORT
                           *ewl[de[dir_pos].ewl_pos].p_time_dir_id = '\0';
#else
                           *p_time_dir_id = '\0';
#endif
                        }
                     }
                  }
                  else /* Queue is stopped, so queue the data. */
                  {
                     if ((fsa[db[de[dir_pos].fme[j].pos[k]].position].special_flag & HOST_DISABLED) == 0)
                     {
                        /* Queue is paused for this host, so lets save the */
                        /* files somewhere snug and save.                  */
                        if (save_files(orig_file_path,
                                       db[de[dir_pos].fme[j].pos[k]].paused_dir,
                                       current_time,
                                       db[de[dir_pos].fme[j].pos[k]].age_limit,
#ifdef _WITH_PTHREAD
                                       file_size_pool,
                                       file_mtime_pool,
                                       file_name_pool,
                                       file_length_pool,
#endif
#if defined (_MAINTAINER_LOG) && defined (SHOW_FILE_MOVING)
                                       __FILE__,
                                       __LINE__,
#endif
                                       &de[dir_pos],
                                       &db[de[dir_pos].fme[j].pos[k]],
                                       j,
                                       files_moved,
                                       db[de[dir_pos].fme[j].pos[k]].lfs,
#ifdef _DISTRIBUTION_LOG
                                       QUEUE_STOPPED_DIS_TYPE,
#endif
                                       NO) < 0)
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      "Failed to queue files for host %s",
                                      db[de[dir_pos].fme[j].pos[k]].host_alias);
                        }
#ifndef _WITH_PTHREAD
                        else
                        {
                           if (pdf != NULL)
                           {
                              *pdf = YES;
                           }
                        }
#endif
                     }
#ifdef _DISTRIBUTION_LOG
                     else
                     {
                        int kk;

                        if (de[dir_pos].flag & ALL_FILES)
                        {
                           for (kk = 0; kk < files_moved; kk++)
                           {
                              if (file_dist_pool[kk][DISABLED_DIS_TYPE].no_of_dist < max_jobs_per_file)
                              {
                                 file_dist_pool[kk][DISABLED_DIS_TYPE].jid_list[file_dist_pool[kk][DISABLED_DIS_TYPE].no_of_dist] = db[de[dir_pos].fme[j].pos[k]].job_id;
                                 file_dist_pool[kk][DISABLED_DIS_TYPE].proc_cycles[file_dist_pool[kk][DISABLED_DIS_TYPE].no_of_dist] = 0;
                                 file_dist_pool[kk][DISABLED_DIS_TYPE].no_of_dist++;
                              }
                           }
                        }
                        else
                        {
                           int    n,
                                  ret;
                           time_t pmatch_time = current_time;

                           for (kk = 0; kk < files_moved; kk++)
                           {
                              for (n = 0; n < de[dir_pos].fme[j].nfm; n++)
                              {
                                 if ((ret = pmatch(de[dir_pos].fme[j].file_mask[n],
                                                   file_name_pool[kk],
                                                   &pmatch_time)) == 0)
                                 {
                                    if (file_dist_pool[kk][DISABLED_DIS_TYPE].no_of_dist < max_jobs_per_file)
                                    {
                                       file_dist_pool[kk][DISABLED_DIS_TYPE].jid_list[file_dist_pool[kk][DISABLED_DIS_TYPE].no_of_dist] = db[de[dir_pos].fme[j].pos[k]].job_id;
                                       file_dist_pool[kk][DISABLED_DIS_TYPE].proc_cycles[file_dist_pool[kk][DISABLED_DIS_TYPE].no_of_dist] = 0;
                                       file_dist_pool[kk][DISABLED_DIS_TYPE].no_of_dist++;
                                    }
                                 }
                                 else if (ret == 1)
                                      {
                                         break;
                                      }
                              }
                           }
                        }
                     }
#endif
                  }
               }
            }
         }

#ifdef _DISTRIBUTION_LOG
         for (j = 0; j < files_moved; j++)
         {
            no_of_distribution_types = 0;
            for (k = 0; k < NO_OF_DISTRIBUTION_TYPES; k++)
            {
               if (file_dist_pool[j][k].no_of_dist > 0)
               {
                  no_of_distribution_types++;
               }
            }
            for (k = 0; k < NO_OF_DISTRIBUTION_TYPES; k++)
            {
               if (file_dist_pool[j][k].no_of_dist > 0)
               {
                  dis_log((unsigned char)k, current_time, de[dir_pos].dir_id,
                          unique_number, file_name_pool[j], file_length_pool[j],
                          file_size_pool[j], file_dist_pool[j][k].no_of_dist,
                          &file_dist_pool[j][k].jid_list,
                          file_dist_pool[j][k].proc_cycles,
                          no_of_distribution_types);
                  file_dist_pool[j][k].no_of_dist = 0;
               }
            }
         }
#endif

         /* Time to remove all files in orig_file_path. */
         if ((de[dir_pos].flag & RENAME_ONE_JOB_ONLY) &&
             ((fsa[db[de[dir_pos].fme[0].pos[0]].position].special_flag & HOST_DISABLED) == 0))
         {
            if (rmdir(orig_file_path) == -1)
            {
               if ((errno == ENOTEMPTY) || (errno == EEXIST))
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Hmm, strange! The directory %s should be empty!",
                             orig_file_path);
#ifdef WITH_UNLINK_DELAY
                  if (remove_dir(orig_file_path, 5) < 0)
#else
                  if (remove_dir(orig_file_path) < 0)
#endif
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "Failed to remove %s", orig_file_path);
                  }
               }
               else
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Failed to rmdir() %s : %s",
                             orig_file_path, strerror(errno));
               }
            }
         }
         else
         {
#ifdef WITH_UNLINK_DELAY
            if (remove_dir(orig_file_path, 5) < 0)
#else
            if (remove_dir(orig_file_path) < 0)
#endif
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to remove %s", orig_file_path);
            }
         }
      } /* if (files_moved > 0) */

      if ((pool_dir == NULL) &&
          (fra[de[dir_pos].fra_pos].no_of_process == 0) &&
          (fra[de[dir_pos].fra_pos].dir_status == DIRECTORY_ACTIVE))
      {
         SET_DIR_STATUS(fra[de[dir_pos].fra_pos].dir_flag,
                        *now,
                        fra[de[dir_pos].fra_pos].start_event_handle,
                        fra[de[dir_pos].fra_pos].end_event_handle,
                        fra[de[dir_pos].fra_pos].dir_status);
      }

      /* In case of an empty directory, remove it! */
      if (host_name != NULL)
      {
         if (rmdir(src_file_dir) == -1)
         {
            if ((errno != EEXIST) && (errno != ENOTEMPTY))
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to rmdir() %s : %s",
                          src_file_dir, strerror(errno));
            }
         }
         else
         {
            /*
             * We have to return NO even if we have copied 'max_copied_files'
             * since there are no files left!
             */
            return(NO);
         }
      }

      if ((dir_pos != -1) &&
          ((files_moved >= fra[de[dir_pos].fra_pos].max_copied_files) ||
           (total_file_size >= fra[de[dir_pos].fra_pos].max_copied_file_size)) &&
          (files_moved != INCORRECT))
      {
         return(YES);
      }
      else
      {
         return(NO);
      }
   }
   else
   {
      if (*no_of_process >= max_process)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Unable to handle directory %s since maximum number of process (%d) for process dir_check reached. @%x",
                    de[dir_pos].dir, max_process, de[dir_pos].dir_id);
      }
      else if (fra[de[dir_pos].fra_pos].no_of_process >= fra[de[dir_pos].fra_pos].max_process)
           {
              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                         "Unable to handle directory since maximum number of process (%d) reached for directory %s @%x",
                         fra[de[dir_pos].fra_pos].max_process, de[dir_pos].dir,
                         de[dir_pos].dir_id);
           }
      return(NO);
   }
}


/*+++++++++++++++++++++++++++ get_one_zombie() ++++++++++++++++++++++++++*/
static pid_t
get_one_zombie(pid_t cpid, time_t now)
{
   int           status;
   pid_t         pid;
#ifdef HAVE_WAIT4
   struct rusage ru;
#endif

   /* Is there a zombie? */
#ifdef HAVE_WAIT4
   if ((pid = wait4(cpid, &status, (cpid == -1) ? WNOHANG : 0, &ru)) > 0)
#else
   if ((pid = waitpid(cpid, &status, (cpid == -1) ? WNOHANG : 0)) > 0)
#endif
   {
      int pos;

      if (WIFEXITED(status))
      {
         switch (WEXITSTATUS(status))
         {
            case 0 : /* Ordinary end of process. */
#ifdef HAVE_WAIT4
                     /* User CPU time. */
                     p_afd_status->amg_child_utime.tv_usec += ru.ru_utime.tv_usec;
                     if (p_afd_status->amg_child_utime.tv_usec > 1000000L)
                     {
                        p_afd_status->amg_child_utime.tv_sec++;
                        p_afd_status->amg_child_utime.tv_usec -= 1000000L;
                     }
                     p_afd_status->amg_child_utime.tv_sec += ru.ru_utime.tv_sec;

                     /* System CPU time. */
                     p_afd_status->amg_child_stime.tv_usec += ru.ru_stime.tv_usec;
                     if (p_afd_status->amg_child_stime.tv_usec > 1000000L)
                     {
                        p_afd_status->amg_child_stime.tv_sec++;
                        p_afd_status->amg_child_stime.tv_usec -= 1000000L;
                     }
                     p_afd_status->amg_child_stime.tv_sec += ru.ru_stime.tv_sec;
#endif
                     break;
            case 1 : /* No files found. */
                     break;

            default: /* Unknown error. */
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Unknown return status (%d) of process dir_check.",
                                WEXITSTATUS(status));
                     break;
         }
      }
      else if (WIFSIGNALED(status))
           {
              /* Abnormal termination. */
              system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                         "Abnormal termination of forked process dir_check (%d), caused by signal %d.",
#else
                         "Abnormal termination of forked process dir_check (%lld), caused by signal %d.",
#endif
                         (pri_pid_t)pid, WTERMSIG(status));
           }
      else if (WIFSTOPPED(status))
           {
              /* Child stopped. */;
              system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                         "Process dir_check (%d) has been put to sleep.",
#else
                         "Process dir_check (%lld) has been put to sleep.",
#endif
                         (pri_pid_t)pid);
              return(INCORRECT);
           }

      /* Update table. */
      if ((pos = get_process_pos(pid)) == -1)
      {
         int i;

         system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                    "Failed to locate process %d in array.",
#else
                    "Failed to locate process %lld in array.",
#endif
                    (pri_pid_t)pid);

         /* For debug process print internal process list. */
         for (i = 0; i < *no_of_process; i++)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                       "dcpl[%d]: pid=%d fra_pos=%d jid=%x fra[%d].no_of_process=%d",
#else
                       "dcpl[%d]: pid=%lld fra_pos=%d jid=%x fra[%d].no_of_process=%d",
#endif
                       i, (pri_pid_t)dcpl[i].pid, dcpl[i].fra_pos,
                       dcpl[i].job_id, dcpl[i].fra_pos,
                       fra[dcpl[i].fra_pos].no_of_process);
         }
      }
      else
      {
         (*no_of_process)--;
         add_to_proc_stat(dcpl[pos].job_id);
         if (fra[dcpl[pos].fra_pos].no_of_process > 0)
         {
            fra[dcpl[pos].fra_pos].no_of_process--;
         }
         if ((fra[dcpl[pos].fra_pos].no_of_process == 0) &&
             (fra[dcpl[pos].fra_pos].dir_status == DIRECTORY_ACTIVE))
         {
            SET_DIR_STATUS(fra[dcpl[pos].fra_pos].dir_flag,
                           now,
                           fra[dcpl[pos].fra_pos].start_event_handle,
                           fra[dcpl[pos].fra_pos].end_event_handle,
                           fra[dcpl[pos].fra_pos].dir_status);
         }
         if (pos < *no_of_process)
         {
            (void)memmove(&dcpl[pos], &dcpl[pos + 1],
                          ((*no_of_process - pos) * sizeof(struct dc_proc_list)));
         }
         dcpl[*no_of_process].pid = -1;
         dcpl[*no_of_process].fra_pos = -1;
      }
   }
   return(pid);
}


/*+++++++++++++++++++++++ check_orphaned_procs() ++++++++++++++++++++++++*/
static void
check_orphaned_procs(time_t now)
{
   int i, j;

   for (i = 0; i < no_of_orphaned_procs; i++)
   {
      if (opl[i] > 0)
      {
         if (kill(opl[i], 0) == -1)
         {
            /* We can now remove this process. */
            for (j = 0; j < *no_of_process; j++)
            {
               if (dcpl[j].pid == opl[i])
               {
                  (*no_of_process)--;
                  if (fra[dcpl[j].fra_pos].no_of_process > 0)
                  {
                     fra[dcpl[j].fra_pos].no_of_process--;
                  }
                  if ((fra[dcpl[j].fra_pos].no_of_process == 0) &&
                      (fra[dcpl[j].fra_pos].dir_status == DIRECTORY_ACTIVE))
                  {
                     SET_DIR_STATUS(fra[dcpl[j].fra_pos].dir_flag,
                                    now,
                                    fra[dcpl[j].fra_pos].start_event_handle,
                                    fra[dcpl[j].fra_pos].end_event_handle,
                                    fra[dcpl[j].fra_pos].dir_status);
                  }
                  if (j < *no_of_process)
                  {
                     (void)memmove(&dcpl[j], &dcpl[j + 1],
                                   ((*no_of_process - j) * sizeof(struct dc_proc_list)));
                  }
                  dcpl[*no_of_process].pid = -1;
                  dcpl[*no_of_process].fra_pos = -1;
                  break;
               }
            }
            no_of_orphaned_procs--;
            if (i < no_of_orphaned_procs)
            {
               (void)memmove(&opl[i], &opl[i + 1],
                             ((no_of_orphaned_procs - i) * sizeof(pid_t)));
            }
            opl[no_of_orphaned_procs] = -1;
            i--;
         }
      }
      else
      {
         no_of_orphaned_procs--;
         if (i < no_of_orphaned_procs)
         {
            (void)memmove(&opl[i], &opl[i + 1],
                          ((no_of_orphaned_procs - i) * sizeof(pid_t)));
         }
         opl[no_of_orphaned_procs] = -1;
         i--;
      }
   }
   if (no_of_orphaned_procs == 0)
   {
      free(opl);
      opl = NULL;
   }

   return;
}



/*------------------------- get_process_pos() ---------------------------*/
static int
get_process_pos(pid_t pid)
{
   register int i;

   for (i = 0; i < *no_of_process; i++)
   {
      if (dcpl[i].pid == pid)
      {
         return(i);
      }
   }

   return(-1);
}


/*------------------------- add_to_proc_stat() --------------------------*/
static void
add_to_proc_stat(unsigned int job_id)
{
   int               i;
   struct tms        tval;
   static struct tms old_tval;

   for (i = 0; i < no_fork_jobs; i++)
   {
      if (job_id == fjd[i].job_id)
      {
         (void)times(&tval);
         fjd[i].user_time += (tval.tms_cutime - old_tval.tms_cutime);
         fjd[i].system_time += (tval.tms_cstime - old_tval.tms_cstime);
         fjd[i].forks++;
         old_tval = tval;
         return;
      }
   }
   return;
}


/*+++++++++++++++++++++++++++ check_fifo() ++++++++++++++++++++++++++++++*/
static void
check_fifo(int read_fd, int write_fd)
{
   int  n;
   char buffer[20];
#ifdef _FIFO_DEBUG
   char cmd[2];
#endif

   /* Read the message. */
   if ((n = read(read_fd, buffer, 20)) > 0)
   {
      int count = 0,
          i, j;
#ifdef _WITH_PTHREAD
# ifdef _DISTRIBUTION_LOG
      int k;
# endif
#endif

#ifdef _FIFO_DEBUG
      show_fifo_data('R', "ip_cmd", buffer, n, __FILE__, __LINE__);
#endif
      while (count < n)
      {
         switch (buffer[count])
         {
#ifdef WITH_DIR_CHECK_RESTART
            case HALT :
               pause_flag = YES;
               *time_buf = AMG_TIMEOUT;
               count += 2;

               /* Set flag to indicate that the the dir_check is NOT active. */
               if (p_afd_status->amg_jobs & INST_JOB_ACTIVE)
               {
                  p_afd_status->amg_jobs ^= INST_JOB_ACTIVE;
               }

#ifdef _FIFO_DEBUG
               cmd[0] = ACKN; cmd[1] = '\0';
               show_fifo_data('W', "ip_resp", cmd, 1, __FILE__, __LINE__);
#endif
               if (send_cmd(ACKN, write_fd) < 0)
               {
                  system_log(FATAL_SIGN, __FILE__, __LINE__,
                            "Could not write to fifo %s : %s (%s %d)",
                            ip_resp_fifo, strerror(errno));
                  exit(INCORRECT);
               }
               return(HALT);
#endif /* WITH_DIR_CHECK_RESTART */

            case STOP     :
            case SHUTDOWN :
#ifdef WITH_SYSTEMD
               if (buffer[count] == SHUTDOWN)
               {
                  terminate_subprocess();
               }
#endif
               (void)fprintf(stderr,
                             "%s terminated by fifo message %s.\n",
                             DIR_CHECK,
                             get_com_action_str((int)buffer[count]));
#ifdef SHOW_EXEC_TIMES
               for (i = 0; i < no_fork_jobs; i++)
               {
                  if (fjd[i].forks > 0)
                  {
                     system_log(DEBUG_SIGN, NULL, 0,
                                "CPU clock times for exec option:");
                     system_log(DEBUG_SIGN, NULL, 0,
                                "Job ID     Forks      User       System     Total");
                     for (j = i; j < no_fork_jobs; j++)
                     {
                        if (fjd[j].forks > 0)
                        {
                           system_log(DEBUG_SIGN, NULL, 0,
                                      "%-10x %-10d %-10d %-10d %-10d",
                                      fjd[j].job_id, fjd[j].forks,
                                      fjd[j].user_time, fjd[j].system_time,
                                      fjd[j].user_time + fjd[j].system_time);
                        }
                     }
                     i = j;
                  }
               }
#endif
               if (p_mmap != NULL)
               {
#ifdef HAVE_MMAP
                  if (munmap(p_mmap, amg_data_size) == -1)
#else
                  if (munmap_emu((void *)p_mmap) == -1)
#endif
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "Failed to munmap() from %s : %s",
                                AMG_DATA_FILE, strerror(errno));
                  }
                  p_mmap = NULL;
               }

               /* Free memory for pid, time and file name array. */
               if (dcpl_fd > 0)
               {
                  (void)close(dcpl_fd);
               }
               if (dcpl != NULL)
               {
#ifdef HAVE_MMAP
                  size_t dcpl_size;

                  dcpl_size = (max_process * sizeof(struct dc_proc_list)) + AFD_WORD_OFFSET;
                  if (munmap(((char *)dcpl - AFD_WORD_OFFSET), dcpl_size) == -1)
#else
                  if (munmap_emu((void *)((char *)dcpl - AFD_WORD_OFFSET)) == -1)
#endif
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Failed to munmap() from %s : %s",
                                DCPL_FILE_NAME, strerror(errno));
                  }
                  dcpl = NULL;
               }
               if (opl != NULL)
               {
                  free(opl);
                  opl = NULL;
                  no_of_orphaned_procs = 0;
               }
               for (i = 0; i < no_of_local_dirs; i++)
               {
                  for (j = 0; j < de[i].nfg; j++)
                  {
                     free(de[i].fme[j].pos);
                     de[i].fme[j].pos = NULL;
                     free(de[i].fme[j].file_mask);
                     de[i].fme[j].file_mask = NULL;
                  }
                  free(de[i].fme);
                  de[i].fme = NULL;
                  de[i].nfg = 0;
                  if (de[i].paused_dir != NULL)
                  {
                     free(de[i].paused_dir);
                     de[i].paused_dir = NULL;
                  }
                  if (de[i].rl_fd != -1)
                  {
                     if (close(de[i].rl_fd) == -1)
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   "Failed to close() retrieve list file for directory ID %x: %s",
                                   de[i].dir_id, strerror(errno));
                     }
                     de[i].rl_fd = -1;
                  }
                  if (de[i].rl != NULL)
                  {
                     char *ptr;

                     ptr = (char *)de[i].rl - AFD_WORD_OFFSET;
                     if (munmap(ptr, de[i].rl_size) == -1)
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   "Failed to munmap() from retrieve list file for directory ID %x: %s",
                                   de[i].dir_id, strerror(errno));
                     }
                     de[i].rl = NULL;
                  }
               }
               free(de);
               for (i = 0; i < no_of_jobs; i++)
               {
                  if (db[i].te != NULL)
                  {
                     free(db[i].te);
                     db[i].te = NULL;
                  }
               }
               free(db);
               if (time_job_list != NULL)
               {
                  free(time_job_list);
                  time_job_list = NULL;
               }
#ifdef _WITH_PTHREAD
               free(thread);
               free(p_data);
               for (i = 0; i < no_of_local_dirs; i++)
               {
                  FREE_RT_ARRAY(p_data[i].file_name_pool);
                  free(p_data[i].file_length_pool);
                  free(p_data[i].file_mtime_pool);
                  free(p_data[i].file_size_pool);
# ifdef _DISTRIBUTION_LOG
                  for (k = 0; k < max_file_buffer; k++)
                  {
                     for (j = 0; j < NO_OF_DISTRIBUTION_TYPES; j++)
                     {
                        free(p_data[i].file_dist_pool[k][j].jid_list);
                        free(p_data[i].file_dist_pool[k][j].proc_cycles);
                     }
                  }
#  ifdef RT_ARRAY_STRUCT_WORKING
                  FREE_RT_ARRAY(p_data[i].file_dist_pool);
#  else
                  free(p_data[i].file_dist_pool[0]);
                  free(p_data[i].file_dist_pool);
#  endif
# endif
               }
#else
               FREE_RT_ARRAY(file_name_pool);
               free(file_length_pool);
               free(file_mtime_pool);
               free(file_size_pool);
# ifdef _DISTRIBUTION_LOG
               for (i = 0; i < max_file_buffer; i++)
               {
                  for (j = 0; j < NO_OF_DISTRIBUTION_TYPES; j++)
                  {
                     free(file_dist_pool[i][j].jid_list);
                     free(file_dist_pool[i][j].proc_cycles);
                  }
               }
#  ifdef RT_ARRAY_STRUCT_WORKING
               FREE_RT_ARRAY(file_dist_pool);
#  else
               free(file_dist_pool[0]);
               free(file_dist_pool);
#  endif
# endif
#endif
#ifdef WITH_ERROR_QUEUE
               if (detach_error_queue() == INCORRECT)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Failed to detach from error queue.");
               }
#endif
#ifdef _DISTRIBUTION_LOG
               release_dis_log();
#endif

               if (fjd != NULL)
               {
                  free(fjd);
                  fjd = NULL;
               }
               system_log(INFO_SIGN, NULL, 0, "Stopped %s (%s)",
                          DIR_CHECK, PACKAGE_VERSION);

               /* Unmap from AFD status area. */
               detach_afd_status();

#ifdef _FIFO_DEBUG
               cmd[0] = ACKN; cmd[1] = '\0';
               show_fifo_data('W', "ip_resp", cmd, 1, __FILE__, __LINE__);
#endif
               if (send_cmd(ACKN, write_fd) < 0)
               {
                  system_log(FATAL_SIGN, __FILE__, __LINE__,
                             "Could not write to fifo %s : %s",
                             DC_CMD_FIFO, strerror(errno));
                  exit(INCORRECT);
               }
               close_counter_file(amg_counter_fd, &amg_counter);
               exit(SUCCESS);

            case SR_EXEC_STAT: /* Show exec statistics in SYSTEM_LOG + reset. */
               {
                  int gotcha = NO;

                  for (i = 0; i < no_fork_jobs; i++)
                  {
                     if (fjd[i].forks > 0)
                     {
                        int j;

                        system_log(DEBUG_SIGN, NULL, 0,
                                   "CPU clock times for exec option:");
                        system_log(DEBUG_SIGN, NULL, 0,
                                   "Job ID     Forks      User       System     Total");
                        gotcha = YES;
                        for (j = i; j < no_fork_jobs; j++)
                        {
                           if (fjd[j].forks > 0)
                           {
                              system_log(DEBUG_SIGN, NULL, 0,
                                         "%-10x %-10u %-10u %-10u %-10u",
                                         fjd[j].job_id, fjd[j].forks,
                                         fjd[j].user_time, fjd[j].system_time,
                                         fjd[j].user_time + fjd[j].system_time);
                              fjd[j].forks = 0;
                              fjd[j].user_time = 0;
                              fjd[j].system_time = 0;
                           }
                        }
                        i = j;
                     }
                  }
                  if (gotcha == NO)
                  {
                     system_log(DEBUG_SIGN, NULL, 0,
                                "There are no exec forks.");
                  }
               }
               break;

            case SEARCH_OLD_FILES :
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Force check for old files started.");
               search_old_files(time(NULL));
               break;

            case CHECK_FILE_DIR :
               force_check = YES;
               break;

            case DATA_READY :
#ifdef _FIFO_DEBUG
               cmd[0] = ACKN; cmd[1] = '\0';
               show_fifo_data('W', "ip_resp", cmd, 1, __FILE__, __LINE__);
#endif
               if (send_cmd(ACKN, write_fd) < 0)
               {
                  system_log(FATAL_SIGN, __FILE__, __LINE__,
                             "Could not write to fifo %s : %s",
                             DC_CMD_FIFO, strerror(errno));
                  exit(INCORRECT);
               }
               break;

            default    : /* Most properly we are reading garbage. */
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "Reading garbage (%d) on fifo %s.",
                          buffer[count], DC_CMD_FIFO);
               exit(INCORRECT);
         }
         count++;
      } /* while (count < n) */
   }

   return;
}


#ifdef WITH_SYSTEMD
/*+++++++++++++++++++++++ terminate_subprocess() ++++++++++++++++++++++++*/
static void
terminate_subprocess(void)
{
   if ((dcpl != NULL) && (*no_of_process > 0))
   {
      int           i,
                    max_shutdown_time;
      char          config_file[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
      struct statx  stat_buf;
#else
      struct stat   stat_buf;
#endif

      (void)snprintf(config_file, MAX_PATH_LENGTH, "%s%s%s",
                     p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
#ifdef HAVE_STATX 
      if (statx(0, config_file, AT_STATX_SYNC_AS_STAT,
                STATX_MTIME, &stat_buf) == -1)
#else          
      if (stat(config_file, &stat_buf) == -1)
#endif
      {
         max_shutdown_time = MAX_SHUTDOWN_TIME;
      }
      else
      {
         char *buffer;

         if ((eaccess(config_file, F_OK) == 0) &&
             (read_file_no_cr(config_file, &buffer, YES, __FILE__, __LINE__) != INCORRECT))
         {
            char value[MAX_INT_LENGTH + 1];

            if (get_definition(buffer, MAX_SHUTDOWN_TIME_DEF,
                               value, MAX_INT_LENGTH) != NULL)
            {
               max_shutdown_time = atoi(value);
               if (max_shutdown_time < MIN_SHUTDOWN_TIME)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "%s is to low (%d < %d), setting default %d.",
                             MAX_SHUTDOWN_TIME_DEF, max_shutdown_time,
                             MIN_SHUTDOWN_TIME, MAX_SHUTDOWN_TIME);
                  max_shutdown_time = MAX_SHUTDOWN_TIME;
               }
            }
            else
            {
               max_shutdown_time = MAX_SHUTDOWN_TIME;
            }
            free(buffer);
         }
         else
         {
            max_shutdown_time = MAX_SHUTDOWN_TIME;
         }
      }

      system_log(INFO_SIGN, NULL, 0,
                 "%s got termination message STOP, waiting for %d process to terminate.",
                 DIR_CHECK, *no_of_process);

      for (i = 0; i < (max_shutdown_time - MIN_SHUTDOWN_TIME); i++)
      {
         while (get_one_zombie(-1, time(NULL)) > 0)
         {
            /* Do nothing. */;
         }
         if (*no_of_process > 0)
         {
            my_usleep(100000L);
         }
      }
      if (*no_of_process > 0)
      {
         system_log(WARN_SIGN, NULL, 0,
                    "There are still %d process left executing. Data can be lost.",
                    *no_of_process);

         for (i = 0; i < *no_of_process; i++)
         {
            system_log(DEBUG_SIGN, NULL, 0,
# if SIZEOF_PID_T == 4
                       "Lost process %d: pid=%d jid= #%x",
# else
                       "Lost process %d: pid=%lld jid= #%x",
# endif
                       i, (pri_pid_t)dcpl[i].pid, dcpl[i].job_id);
         }

         /*
          * The left over process will be killed by systemd
          * (KillMode=control-group).
          */
      }
   }

   return;
}
#endif /* WITH_SYSTEMD */


/*++++++++++++++++++++++++++++ sig_handler() ++++++++++++++++++++++++++++*/
static void
sig_handler(int signo)
{
   if (in_child == YES)
   {
      pid_t pid;

      pid = getpid();
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (write(fin_writefd, &pid, sizeof(pid_t)) != sizeof(pid_t))
#else
      if (write(fin_fd, &pid, sizeof(pid_t)) != sizeof(pid_t))
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not write() to fifo %s : %s",
                    IP_FIN_FIFO, strerror(errno));
      }
   }
   else
   {
      /* Unset flag to indicate that the the dir_check is NOT active. */
      p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
   }

   if (signo == SIGBUS)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Uuurrrggh! Received SIGBUS. Dump programmers!");
   }
   else if (signo == SIGSEGV)
        {
           system_log(FATAL_SIGN, __FILE__, __LINE__,
                      "Aaarrrggh! Received SIGSEGV.");
        }
        else
        {
           system_log(FATAL_SIGN, __FILE__, __LINE__,
                      "Received signal %d.", signo);
        }

   abort();
}


/*+++++++++++++++++++++++++++++ sig_alarm() +++++++++++++++++++++++++++++*/
static void
sig_alarm(int signo)
{
   siglongjmp(env_alrm, 1);
}


#ifdef WITH_SYSTEMD
/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   terminate_subprocess();

   (void)fprintf(stderr,
#if SIZEOF_PID_T == 4
                 "%s terminated by signal %d (%d)\n",
#else
                 "%s terminated by signal %d (%lld)\n",
#endif
                 DIR_CHECK, signo, (pri_pid_t)getpid());

   exit(SUCCESS);
}
#endif
