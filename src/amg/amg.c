/*
 *  amg.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2024 Deutscher Wetterdienst (DWD),
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
 **   amg - creates messages for the FD (File Distributor)
 **
 ** SYNOPSIS
 **   amg [-r          rescan time
 **        -w          working directory
 **        --version   show current version]
 **
 ** DESCRIPTION
 **   The AMG (Automatic Message Generator) searches certain directories
 **   for files to then generate a message for the process FD (File
 **   Distributor). The directories where the AMG must search are
 **   specified in the DIR_CONFIG file. When it generates the message
 **   it also moves all the files from the 'user' directory to a unique
 **   directory, so the FD just needs to send all files which are in
 **   this directory. Since the message name and the directory name are
 **   the same, the FD will need no further information to get the
 **   files.
 **
 **   These 'user'-directories are scanned every DEFAULT_RESCAN_TIME
 **   (5 seconds) time. It also checks if there are any changes made to
 **   the DIR_CONFIG or HOST_CONFIG file. If so, it will reread them,
 **   stop all its process, create a new shared memory area and restart
 **   all jobs again (only if the DIR_CONFIG changes). Thus, it is not
 **   necessary to stop the AFD when entering a new host entry or removing
 **   one.
 **
 **   The AMG is also able to receive commands via the AFD_CMD_FIFO
 **   fifo from the AFD. So far only one command is recognised: STOP.
 **   This is used when the user wants to stop only the AMG or when
 **   the AFD is shutdown.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **   When it failes to start any of its jobs because they cannot
 **   access there shared memory area it will exit and return the value
 **   3. So the process init_afd can restart it.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.08.1995 H.Kiehl Created
 **   31.08.1997 H.Kiehl Remove check for HOST_CONFIG file.
 **   19.11.1997 H.Kiehl Return of the HOST_CONFIG file!
 **   17.03.2003 H.Kiehl Support for reading multiple DIR_CONFIG files.
 **   16.07.2005 H.Kiehl Made old_file_time and delete_files_flag
 **                      configurable via AFD_CONFIG.
 **   14.02.2011 H.Kiehl Added onetime support.
 **   27.10.2012 H.Kiehl On Linux check if hardlink protection is enabled
 **                      and if this is the case, warn user and tell him
 **                      what he can do.
 **   18.02.2023 H.Kiehl Attach to FSA and/or FRA after eval_dir_config().
 **
 */
DESCR__E_M1

#include <stdio.h>                    /* fprintf(), snprintf()           */
#include <string.h>                   /* strcpy(), strcat(), strerror()  */
#include <stdlib.h>                   /* atexit(), abort()               */
#include <time.h>                     /* ctime(), time()                 */
#include <sys/types.h>                /* fdset                           */
#include <sys/stat.h>
#include <sys/time.h>                 /* struct timeval                  */
#ifdef HAVE_SETPRIORITY
# include <sys/resource.h>            /* setpriority()                   */
#endif
#include <sys/wait.h>                 /* waitpid()                       */
#ifdef HAVE_MMAP
# include <sys/mman.h>                /* mmap(), munmap()                */
#endif
#include <signal.h>                   /* kill(), signal()                */
#include <unistd.h>                   /* select(), read(), write(),      */
                                      /* close(), unlink(), sleep(),     */
                                      /* fpathconf()                     */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>                   /* O_RDWR, O_CREAT, O_WRONLY, etc  */
                                      /* open()                          */
#endif
#include <errno.h>
#include "amgdefs.h"
#include "version.h"

/* Global variables. */
#ifdef _DEBUG
FILE                       *p_debug_file;
#endif
int                        alfbl,    /* Additional locked file buffer length */
                           alfc,     /* Additional locked file counter       */
                           create_source_dir = DEFAULT_CREATE_SOURCE_DIR_DEF,
                           create_source_dir_disabled = NO,
                           dnb_fd,
                           data_length, /* The size of data for one job. */
                           default_no_parallel_jobs = DEFAULT_NO_PARALLEL_JOBS,
                           default_delete_files_flag = 0,
                           default_max_errors = DEFAULT_MAX_ERRORS,
                           default_old_file_time = -1,
                           default_retry_interval = DEFAULT_RETRY_INTERVAL,
                           default_successful_retries = DEFAULT_SUCCESSFUL_RETRIES,
                           default_transfer_blocksize = DEFAULT_TRANSFER_BLOCKSIZE,
                           event_log_fd = STDERR_FILENO,
#ifdef _MAINTAINER_LOG
                           maintainer_log_fd = STDERR_FILENO,
#endif
                           max_process_per_dir = MAX_PROCESS_PER_DIR,
                           *no_of_dir_names,
                           no_of_dirs,
                           no_of_hosts,
                           no_of_local_dir,  /* Number of local          */
                                             /* directories found in the */
                                             /* DIR_CONFIG file.         */
                           no_of_dc_filters,
                           no_of_dir_configs,
#ifdef WITH_ONETIME
                           no_of_ot_dir_configs = 0,
#endif
                           no_of_job_ids = 0,
                           fra_fd = -1,
                           fra_id,
                           fsa_fd = -1,
                           fsa_id,
                           jid_fd = -1,
                           remove_unused_hosts = NO,
                           sys_log_fd = STDERR_FILENO,
                           stop_flag = 0;
unsigned int               default_error_offline_flag = DEFAULT_FSA_HOST_STATUS,
#ifdef WITH_INOTIFY
                           default_inotify_flag = DEFAULT_INOTIFY_FLAG,
#endif
                           max_copied_files = MAX_COPIED_FILES;
long                       default_transfer_timeout = DEFAULT_TRANSFER_TIMEOUT;
pid_t                      dc_pid;      /* dir_check pid                 */
off_t                      fra_size,
                           fsa_size,
                           jid_size,
                           max_copied_file_size = MAX_COPIED_FILE_SIZE * MAX_COPIED_FILE_SIZE_UNIT;
mode_t                     create_source_dir_mode = DIR_MODE;
time_t                     default_warn_time = DEFAULT_DIR_WARN_TIME;
time_t                     default_info_time = DEFAULT_DIR_INFO_TIME;
#ifdef HAVE_MMAP
off_t                      afd_active_size;
#endif
unsigned char              ignore_first_errors = 0;
char                       *alfiles = NULL, /* Additional locked files */
                           *host_config_file,
                           *p_work_dir,
                           *pid_list;
struct host_list           *hl;
struct fileretrieve_status *fra;
struct filetransfer_status *fsa = NULL;
struct job_id_data         *jid = NULL;
struct afd_status          *p_afd_status;
struct dir_name_buf        *dnb = NULL;
struct dc_filter_list      *dcfl;
struct dir_config_buf      *dc_dcl = NULL;
#ifdef WITH_ONETIME
struct dir_config_buf      *ot_dcl = NULL;
#endif
#ifdef _DELETE_LOG
struct delete_log          dl;
#endif
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void                amg_exit(void),
                           get_afd_config_value(int *, int *, int *, mode_t *,
                                                unsigned int *, off_t *,
                                                int *, int *, int *,
#ifdef WITH_INOTIFY
                                                unsigned int *,
#endif
                                                time_t *, time_t *, int *,
                                                int *, int *, int *, int *,
                                                long *, unsigned int *, int *,
                                                int *),
                           notify_dir_check(void),
                           sig_segv(int),
                           sig_bus(int),
                           sig_exit(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int              i,
                    amg_cmd_fd = 0,          /* File descriptor of the   */
                                             /* used by the controlling  */
                                             /* program AFD.             */
#ifdef WITHOUT_FIFO_RW_SUPPORT
                    amg_cmd_writefd,
                    db_update_writefd,
#endif
                    db_update_fd = 0,        /* If the dialog for creat- */
                                             /* ing a new database has a */
                                             /* new database it can      */
                                             /* notify AMG over this     */
                                             /* fifo.                    */
                    dc_names_can_change,
                    max_fd = 0,              /* Biggest file descriptor. */
                    afd_active_fd,           /* File descriptor to file  */
                                             /* holding all active pid's */
                                             /* of the AFD.              */
                    status = 0,
                    fd,
                    rescan_time = DEFAULT_RESCAN_TIME,
                    max_no_proc = MAX_NO_OF_DIR_CHECKS,
                    max_shutdown_time = MAX_SHUTDOWN_TIME,
                    using_groups = NO;
   time_t           hc_old_time;
   size_t           fifo_size;
   char             *fifo_buffer,
                    work_dir[MAX_PATH_LENGTH],
                    *ptr;
   fd_set           rset;
   struct timeval   timeout;
#ifdef HAVE_STATX
   struct statx     stat_buf;
#else
   struct stat      stat_buf;
#endif
#ifdef SA_FULLDUMP
   struct sigaction sact;
#endif

   CHECK_FOR_VERSION(argc, argv);

#ifdef _DEBUG
   /* Open debug file for writing. */
   if ((p_debug_file = fopen("amg.debug", "w")) == NULL)
   {
      (void)fprintf(stderr, _("ERROR   : Could not fopen() `%s' : %s (%s %d)\n"),
                    "amg.debug", strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#endif

#ifdef SA_FULLDUMP
   /*
    * When dumping core sure we do a FULL core dump!
    */
   sact.sa_handler = SIG_DFL;
   sact.sa_flags = SA_FULLDUMP;
   sigemptyset(&sact.sa_mask);
   if (sigaction(SIGSEGV, &sact, NULL) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("sigaction() error : %s"), strerror(errno));
      exit(INCORRECT);
   }
#endif

   /* Do some cleanups when we exit. */
   if (atexit(amg_exit) != 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("Could not register exit function : %s"), strerror(errno));
      exit(INCORRECT);
   }
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("Could not set signal handler : %s"), strerror(errno));
      exit(INCORRECT);
   }

   /* Check syntax if necessary. */
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;
   (void)umask(0);

   /*
    * Lock AMG so no other AMG can be started!
    */
   if ((ptr = lock_proc(AMG_LOCK_ID, NO)) != NULL)
   {
      (void)fprintf(stderr, _("Process AMG already started by %s : (%s %d)\n"),
                    ptr, __FILE__, __LINE__);
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Process AMG already started by %s"), ptr);
      _exit(INCORRECT);
   }
   else
   {
      int   first_time = NO;
      off_t db_size;
      char  afd_active_file[MAX_PATH_LENGTH],
            amg_cmd_fifo[MAX_PATH_LENGTH],
            counter_file[MAX_PATH_LENGTH],
            db_update_fifo[MAX_PATH_LENGTH],
            dc_cmd_fifo[MAX_PATH_LENGTH],
            dc_resp_fifo[MAX_PATH_LENGTH];

      if ((host_config_file = malloc((strlen(work_dir) + strlen(ETC_DIR) +
                                      strlen(DEFAULT_HOST_CONFIG_FILE) + 1))) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("malloc() error : %s"), strerror(errno));
         _exit(INCORRECT);
      }
      (void)strcpy(host_config_file, work_dir);
      (void)strcat(host_config_file, ETC_DIR);
      (void)strcat(host_config_file, DEFAULT_HOST_CONFIG_FILE);

      /* Initialise variables with default values. */
      (void)strcpy(amg_cmd_fifo, work_dir);
      (void)strcat(amg_cmd_fifo, FIFO_DIR);
      (void)strcpy(dc_cmd_fifo, amg_cmd_fifo);
      (void)strcat(dc_cmd_fifo, DC_CMD_FIFO);
      (void)strcpy(dc_resp_fifo, amg_cmd_fifo);
      (void)strcat(dc_resp_fifo, DC_RESP_FIFO);
      (void)strcpy(db_update_fifo, amg_cmd_fifo);
      (void)strcat(db_update_fifo, DB_UPDATE_FIFO);
      (void)strcpy(counter_file, amg_cmd_fifo);
      (void)strcat(counter_file, COUNTER_FILE);
      (void)strcpy(afd_active_file, amg_cmd_fifo);
      (void)strcat(afd_active_file, AFD_ACTIVE_FILE);
      (void)strcat(amg_cmd_fifo, AMG_CMD_FIFO);

      if (attach_afd_status(NULL, WAIT_AFD_STATUS_ATTACH) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("Failed to attach to AFD status shared area."));
         exit(INCORRECT);
      }

      /*
       * We need to write the pid of dir_check to the AFD_ACTIVE file.
       * Otherwise it can happen that two or more dir_check's run at
       * the same time, when init_afd was killed.
       */
      if ((afd_active_fd = coe_open(afd_active_file, O_RDWR)) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Failed to open() `%s' : %s"),
                    afd_active_file, strerror(errno));
         pid_list = NULL;
      }
      else
      {
#ifdef HAVE_STATX
         if (statx(afd_active_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                   STATX_SIZE, &stat_buf) < 0)
#else
         if (fstat(afd_active_fd, &stat_buf) < 0)
#endif
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                       _("Failed to statx() `%s' : %s"),
#else
                       _("Failed to fstat() `%s' : %s"),
#endif
                       afd_active_file, strerror(errno));
            (void)close(afd_active_fd);
            pid_list = NULL;
         }
         else
         {
#ifdef HAVE_MMAP
            if ((pid_list = mmap(NULL,
# ifdef HAVE_STATX
                                 stat_buf.stx_size,
# else
                                 stat_buf.st_size,
# endif
                                 (PROT_READ | PROT_WRITE), MAP_SHARED,
                                 afd_active_fd, 0)) == (caddr_t) -1)
#else
            if ((pid_list = mmap_emu(NULL,
# ifdef HAVE_STATX
                                     stat_buf.stx_size,
# else
                                     stat_buf.st_size,
# endif
                                     (PROT_READ | PROT_WRITE), MAP_SHARED,
                                     afd_active_file, 0)) == (caddr_t) -1)
#endif
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          _("mmap() error : %s"), strerror(errno));
               pid_list = NULL;
            }
#ifdef HAVE_STATX
            afd_active_size = stat_buf.stx_size;
#else
            afd_active_size = stat_buf.st_size;
#endif

            if (close(afd_active_fd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          _("Failed to close() `%s' : %s"),
                          afd_active_file, strerror(errno));
            }

            /*
             * Before starting to activate new process make sure there is
             * no old process still running.
             */
            if (*(pid_t *)(pid_list + ((DC_NO + 1) * sizeof(pid_t))) > 0)
            {
               if (kill(*(pid_t *)(pid_list + ((DC_NO + 1) * sizeof(pid_t))), SIGINT) == 0)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             _("Had to kill() an old %s job!"), DC_PROC_NAME);
               }
            }
         }
      }

      /*
       * Create and initialize AMG counter file. Do it here to
       * avoid having two dir_checks trying to do the same.
       */
#ifdef HAVE_STATX
      if ((statx(0, counter_file, AT_STATX_SYNC_AS_STAT, 0, &stat_buf) == -1) &&
          (errno == ENOENT))
#else
      if ((stat(counter_file, &stat_buf) == -1) && (errno == ENOENT))
#endif
      {
         /*
          * Lets assume when there is no counter file that this is the
          * first time that AFD is started.
          */
         first_time = YES;
      }
      if ((fd = coe_open(counter_file, O_RDWR | O_CREAT,
#ifdef GROUP_CAN_WRITE
                         S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) == -1)
#else
                         S_IRUSR | S_IWUSR)) == -1)
#endif
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("Failed to open() `%s' : %s"),
                    counter_file, strerror(errno));
         exit(INCORRECT);
      }

      /* Initialise counter file with zero. */
      if (write(fd, &status, sizeof(int)) != sizeof(int))
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("Could not initialise `%s' : %s"),
                    counter_file, strerror(errno));
         exit(INCORRECT);
      }
      if (close(fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("close() error : %s"), strerror(errno));
      }

      /* If process AFD and AMG_DIALOG have not yet been created */
      /* we create the fifos needed to communicate with them.    */
#ifdef HAVE_STATX
      if ((statx(0, amg_cmd_fifo, AT_STATX_SYNC_AS_STAT,
                 STATX_MODE, &stat_buf) == -1) ||
          (!S_ISFIFO(stat_buf.stx_mode)))
#else
      if ((stat(amg_cmd_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
#endif
      {
         if (make_fifo(amg_cmd_fifo) < 0)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       _("Failed to create fifo `%s'."), amg_cmd_fifo);
            exit(INCORRECT);
         }
      }
#ifdef HAVE_STATX
      if ((statx(0, db_update_fifo, AT_STATX_SYNC_AS_STAT,
                 STATX_MODE, &stat_buf) == -1) ||
          (!S_ISFIFO(stat_buf.stx_mode)))
#else
      if ((stat(db_update_fifo, &stat_buf) == -1) ||
          (!S_ISFIFO(stat_buf.st_mode)))
#endif
      {
         if (make_fifo(db_update_fifo) < 0)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       _("Failed to create fifo `%s'."), db_update_fifo);
            exit(INCORRECT);
         }
      }

      /* Open fifo to AFD to receive commands. */
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(amg_cmd_fifo, &amg_cmd_fd, &amg_cmd_writefd) == -1)
#else
      if ((amg_cmd_fd = coe_open(amg_cmd_fifo, O_RDWR)) == -1)
#endif
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("Failed to open() `%s' : %s"),
                    amg_cmd_fifo, strerror(errno));
         exit(INCORRECT);
      }

      /* Open fifo for edit_hc and edit_dc so they can */
      /* inform the AMG about any changes.             */
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(db_update_fifo, &db_update_fd, &db_update_writefd) == -1)
#else
      if ((db_update_fd = coe_open(db_update_fifo, O_RDWR)) == -1)
#endif
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("Could not open() `%s' : %s"),
                    db_update_fifo, strerror(errno));
         exit(INCORRECT);
      }

      get_afd_config_value(&rescan_time, &max_no_proc, &max_process_per_dir,
                           &create_source_dir_mode, &max_copied_files,
                           &max_copied_file_size, &default_delete_files_flag,
                           &default_old_file_time, &remove_unused_hosts,
#ifdef WITH_INOTIFY
                           &default_inotify_flag,
#endif
                           &default_info_time, &default_warn_time,
                           &default_no_parallel_jobs, &default_max_errors,
                           &default_retry_interval, &default_transfer_blocksize,
                           &default_successful_retries,
                           &default_transfer_timeout,
                           &default_error_offline_flag, &create_source_dir,
                           &max_shutdown_time);

      /* Determine the size of the fifo buffer and allocate buffer. */
      if ((i = (int)fpathconf(db_update_fd, _PC_PIPE_BUF)) < 0)
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
                    _("malloc() error [%d bytes] : %s"),
                    fifo_size, strerror(errno));
         exit(INCORRECT);
      }

      /* Find largest file descriptor. */
      if (amg_cmd_fd > db_update_fd)
      {
         max_fd = amg_cmd_fd + 1;
      }
      else
      {
         max_fd = db_update_fd + 1;
      }

      /* Evaluate HOST_CONFIG file. */
      hl = NULL;
      if ((eval_host_config(&no_of_hosts, host_config_file,
                            &hl, NULL, NULL, first_time) == NO_ACCESS) &&
          (first_time == NO))
      {
         /*
          * Try get the host information from the current FSA.
          */
         if (fsa_attach_passive(YES, AMG) == SUCCESS)
         {
            size_t new_size = ((no_of_hosts / HOST_BUF_SIZE) + 1) *
                              HOST_BUF_SIZE * sizeof(struct host_list);

            if ((hl = realloc(hl, new_size)) == NULL)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          _("Could not reallocate memory for host list : %s"),
                          strerror(errno));
               exit(INCORRECT);
            }

            for (i = 0; i < no_of_hosts; i++)
            {
               (void)memcpy(hl[i].host_alias, fsa[i].host_alias, MAX_HOSTNAME_LENGTH + 1);
               (void)memcpy(hl[i].real_hostname[0], fsa[i].real_hostname[0], MAX_REAL_HOSTNAME_LENGTH);
               (void)memcpy(hl[i].real_hostname[1], fsa[i].real_hostname[1], MAX_REAL_HOSTNAME_LENGTH);
               (void)memcpy(hl[i].host_toggle_str, fsa[i].host_toggle_str, 5);
               (void)memcpy(hl[i].proxy_name, fsa[i].proxy_name, MAX_PROXY_NAME_LENGTH);
               (void)memset(hl[i].fullname, 0, MAX_FILENAME_LENGTH);
               hl[i].allowed_transfers   = fsa[i].allowed_transfers;
               hl[i].max_errors          = fsa[i].max_errors;
               hl[i].retry_interval      = fsa[i].retry_interval;
               hl[i].transfer_blksize    = fsa[i].block_size;
               hl[i].successful_retries  = fsa[i].max_successful_retries;
               hl[i].file_size_offset    = fsa[i].file_size_offset;
               hl[i].transfer_timeout    = fsa[i].transfer_timeout;
               hl[i].protocol            = fsa[i].protocol;
               hl[i].transfer_rate_limit = fsa[i].transfer_rate_limit;
               hl[i].socksnd_bufsize     = fsa[i].socksnd_bufsize;
               hl[i].sockrcv_bufsize     = fsa[i].sockrcv_bufsize;
               hl[i].keep_connected      = fsa[i].keep_connected;
               hl[i].warn_time           = fsa[i].warn_time;
#ifdef WITH_DUP_CHECK
               hl[i].dup_check_flag      = fsa[i].dup_check_flag;
               hl[i].dup_check_timeout   = fsa[i].dup_check_timeout;
#endif
               hl[i].protocol_options    = fsa[i].protocol_options;
               hl[i].protocol_options2   = fsa[i].protocol_options2;
               hl[i].host_status = 0;
               if (fsa[i].host_status & HOST_ERROR_OFFLINE_STATIC)
               {
                  hl[i].host_status |= HOST_ERROR_OFFLINE_STATIC;
               }
               if (fsa[i].special_flag & HOST_DISABLED)
               {
                  hl[i].host_status |= HOST_CONFIG_HOST_DISABLED;
               }
               if ((fsa[i].special_flag & HOST_IN_DIR_CONFIG) == 0)
               {
                  hl[i].host_status |= HOST_NOT_IN_DIR_CONFIG;
               }
               if (fsa[i].special_flag & KEEP_CON_NO_SEND)
               {
                  hl[i].protocol_options |= KEEP_CON_NO_SEND_2;
               }
               if (fsa[i].special_flag & KEEP_CON_NO_FETCH)
               {
                  hl[i].protocol_options |= KEEP_CON_NO_FETCH_2;
               }
               if (fsa[i].host_status & STOP_TRANSFER_STAT)
               {
                  hl[i].host_status |= STOP_TRANSFER_STAT;
               }
               if (fsa[i].host_status & PAUSE_QUEUE_STAT)
               {
                  hl[i].host_status |= PAUSE_QUEUE_STAT;
               }
               if (fsa[i].host_toggle == HOST_TWO)
               {
                  hl[i].host_status |= HOST_TWO_FLAG;
               }
               if (fsa[i].host_status & DO_NOT_DELETE_DATA)
               {
                  hl[i].host_status |= DO_NOT_DELETE_DATA;
               }
               if (fsa[i].host_status & SIMULATE_SEND_MODE)
               {
                  hl[i].host_status |= SIMULATE_SEND_MODE;
               }
            }

            (void)fsa_detach(NO);
         }
      }

      db_size = 0;
      dc_names_can_change = NO;
      for (i = 0; i < no_of_dc_filters; i++)
      {
         if (dcfl[i].is_filter == NO)
         {
            /* Get the size of the database file. */
#ifdef HAVE_STATX
            if (statx(0, dcfl[i].dc_filter, AT_STATX_SYNC_AS_STAT,
                      STATX_SIZE | STATX_MTIME, &stat_buf) == -1)
#else
            if (stat(dcfl[i].dc_filter, &stat_buf) == -1)
#endif
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          _("Could not get size + time of database file `%s' : %s"),
                          dcfl[i].dc_filter, strerror(errno));
            }
            else
            {
               /* Check space for dc_dcl structure. */
               if ((no_of_dir_configs % DIR_CONFIG_NAME_STEP_SIZE) == 0)
               {
                  size_t new_size;

                  new_size = ((no_of_dir_configs / DIR_CONFIG_NAME_STEP_SIZE) + 1) * DIR_CONFIG_NAME_STEP_SIZE * sizeof(struct dir_config_buf);

                  if ((dc_dcl = realloc(dc_dcl, new_size)) == NULL)
                  {
                     system_log(FATAL_SIGN, __FILE__, __LINE__,
                                "Could not realloc() memory : %s",
                                strerror(errno));
                     exit(INCORRECT);
                  }
               }

               /* Since this is the first time round and this */
               /* is the time of the actual database, we      */
               /* store its value here in dc_old_time.        */
               if ((dc_dcl[no_of_dir_configs].dir_config_file = malloc(dcfl[i].length)) == NULL)
               {
                  system_log(FATAL_SIGN, __FILE__, __LINE__,
                             "Could not malloc() %d bytes : %s",
                             dcfl[i].length, strerror(errno));
                  exit(INCORRECT);
               }
               (void)memcpy(dc_dcl[no_of_dir_configs].dir_config_file,
                            dcfl[i].dc_filter, dcfl[i].length);
#ifdef HAVE_STATX
               db_size += stat_buf.stx_size;
               dc_dcl[no_of_dir_configs].dc_old_time = stat_buf.stx_mtime.tv_sec;
#else
               db_size += stat_buf.st_size;
               dc_dcl[no_of_dir_configs].dc_old_time = stat_buf.st_mtime;
#endif
               dc_dcl[no_of_dir_configs].is_filter = NO;
               no_of_dir_configs++;
            }
         }
         else
         {
            get_full_dc_names(dcfl[i].dc_filter, &db_size);
            dc_names_can_change = YES;
         }
      }
      if (db_size < 12)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("There is no valid data in DIR_CONFIG %s."),
                    (no_of_dir_configs > 1) ? "files" : "file");
         exit(INCORRECT);
      }
      lookup_dc_id(&dc_dcl, no_of_dir_configs);

      /*
       * If necessary inform FD that AMG is (possibly) about to change
       * the FSA. This is needed when we start/stop the AMG by hand.
       */
      p_afd_status->amg_jobs |= REREADING_DIR_CONFIG;
      inform_fd_about_fsa_change();

      /* Evaluate database. */
#ifdef WITH_ONETIME
      if (eval_dir_config(db_size, NULL, NULL, NO, &using_groups) != SUCCESS)
#else
      if (eval_dir_config(db_size, NULL, NULL, &using_groups) != SUCCESS)
#endif
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("Could not find any valid entries in database %s"),
                    (no_of_dir_configs > 1) ? "files" : "file");
         exit(INCORRECT);
      }
      if (using_groups == YES)
      {
         init_group_list_mtime();
      }

      /* Lets check and see if create_source_dir was set via afdcfg. */
      if (fsa_attach_passive(YES, AMG) == SUCCESS)
      {
         if (*(unsigned char *)((char *)fsa - AFD_FEATURE_FLAG_OFFSET_END) & DISABLE_CREATE_SOURCE_DIR)
         {
            if ((create_source_dir != DEFAULT_CREATE_SOURCE_DIR_DEF) &&
                (DEFAULT_CREATE_SOURCE_DIR_DEF == NO))
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Overriding AFD_CONFIG value %s, setting it to NO due to afdcfg setting.",
                          CREATE_SOURCE_DIR_DEF);
            }
            create_source_dir = NO;
            create_source_dir_disabled = YES;
         }
         else
         {
            if ((create_source_dir != DEFAULT_CREATE_SOURCE_DIR_DEF) &&
                (DEFAULT_CREATE_SOURCE_DIR_DEF == YES))
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Overriding AFD_CONFIG value %s, setting it to YES due to afdcfg setting.",
                          CREATE_SOURCE_DIR_DEF);
            }
            create_source_dir = YES;
         }
         (void)fsa_detach(NO);
      }

      /*
       * Since there might have been an old FSA which has more information
       * then the HOST_CONFIG lets rewrite this file using the information
       * from both HOST_CONFIG and old FSA. That what is found in the
       * HOST_CONFIG will always have a higher priority.
       */
      hc_old_time = write_host_config(no_of_hosts, host_config_file, hl);
      system_log(INFO_SIGN, NULL, 0,
                 _("Found %d hosts in HOST_CONFIG."), no_of_hosts);

      /*
       * Before we start any programs copy any files that are in the
       * pool directory back to their original directories (if they
       * still exist).
       */
#ifdef _DELETE_LOG
      delete_log_ptrs(&dl);
#endif
      clear_pool_dir();
#ifdef _DELETE_LOG
      if ((dl.fd != -1) && (dl.data != NULL))
      {
         free(dl.data);
         (void)close(dl.fd);
# ifdef WITHOUT_FIFO_RW_SUPPORT
         if (dl.readfd != -1)
         {
            (void)close(dl.readfd);
         }
# endif
      }
#endif

      /* Free dir name buffer which is no longer needed. */
      if (dnb != NULL)
      {
         unmap_data(dnb_fd, (void *)&dnb);
      }

      /* First create the fifo to communicate with other process. */
      (void)unlink(dc_cmd_fifo);
      if (make_fifo(dc_cmd_fifo) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("Could not create fifo `%s'."), dc_cmd_fifo);
         exit(INCORRECT);
      }
      (void)unlink(dc_resp_fifo);
      if (make_fifo(dc_resp_fifo) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("Could not create fifo `%s'."), dc_resp_fifo);
         exit(INCORRECT);
      }
   }

   /*
    * When starting, ensure that ALL_DISABLED is still
    * correct in fra[].dir_flag
    */
   check_every_fra_disable_all_flag();

   /* Start process dir_check if database has information. */
   if (data_length > 0)
   {
      dc_pid = make_process_amg(work_dir, DC_PROC_NAME, rescan_time,
                                max_no_proc,
                                (create_source_dir == YES) ? create_source_dir_mode : 0,
                                0);
      if (pid_list != NULL)
      {
         *(pid_t *)(pid_list + ((DC_NO + 1) * sizeof(pid_t))) = dc_pid;
      }
   }
   else
   {
      /* Process not started. */
      dc_pid = NOT_RUNNING;
   }

   /* Note time when AMG is started. */
   system_log(INFO_SIGN, NULL, 0, _("Starting %s (%s)"), AMG, PACKAGE_VERSION);
   system_log(DEBUG_SIGN, NULL, 0,
              _("AMG Configuration: Maximum shutdown time     %d (0.1 sec)"),
              max_shutdown_time);
   system_log(DEBUG_SIGN, NULL, 0,
              _("AMG Configuration: Directory scan interval   %d (sec)"),
              rescan_time);
   system_log(DEBUG_SIGN, NULL, 0,
              _("AMG Configuration: Max process               %d"), max_no_proc);
   system_log(DEBUG_SIGN, NULL, 0,
              _("AMG Configuration: Max process per directory %d"),
              max_process_per_dir);
#ifdef HAVE_HW_CRC32
    system_log(DEBUG_SIGN, NULL, 0,
               "CRC32 CPU support: %s",
               (detect_cpu_crc32() == YES) ? "Yes" : "No");
#endif
   if (ignore_first_errors > 0)
   {
      system_log(DEBUG_SIGN, NULL, 0,
                 _("AMG Configuration: Errors/warning offline    %d"),
                 (int)ignore_first_errors);
   }
   if (default_delete_files_flag != 0)
   {
      char tmp_str[38];

      ptr = tmp_str;
      if (default_delete_files_flag & UNKNOWN_FILES)
      {
         ptr += snprintf(ptr, 38, "UNKNOWN ");
      }
      if (default_delete_files_flag & QUEUED_FILES)
      {
         ptr += snprintf(ptr, 38 - (ptr - tmp_str), "QUEUED ");
      }
      if (default_delete_files_flag & OLD_LOCKED_FILES)
      {
         ptr += snprintf(ptr, 38 - (ptr - tmp_str), "LOCKED ");
      }
      if (default_delete_files_flag & OLD_RLOCKED_FILES)
      {
         ptr += snprintf(ptr, 38 - (ptr - tmp_str), "RLOCKED ");
      }
      if (default_delete_files_flag & OLD_ILOCKED_FILES)
      {
         ptr += snprintf(ptr, 38 - (ptr - tmp_str), "ILOCKED");
      }
      system_log(DEBUG_SIGN, NULL, 0,
                 _("AMG Configuration: Default delete file flag  %s"), tmp_str);
      if (default_old_file_time == -1)
      {
         system_log(DEBUG_SIGN, NULL, 0,
                    _("AMG Configuration: Default old file time     %d"),
                    DEFAULT_OLD_FILE_TIME);
      }
      else
      {
         system_log(DEBUG_SIGN, NULL, 0,
                    _("AMG Configuration: Default old file time     %d"),
                    default_old_file_time);
      }
   }
   system_log(DEBUG_SIGN, NULL, 0,
              _("AMG Configuration: Default max copied files  %u"),
              max_copied_files);
   system_log(DEBUG_SIGN, NULL, 0,
#if SIZEOF_OFF_T == 4
              _("AMG Configuration: Def max copied file size  %ld (bytes)"),
#else
              _("AMG Configuration: Def max copied file size  %lld (bytes)"),
#endif
              (pri_off_t)max_copied_file_size);
   system_log(DEBUG_SIGN, NULL, 0,
              _("AMG Configuration: Remove unused hosts       %s"),
              (remove_unused_hosts == NO) ? _("No") : _("Yes"));
   if (alfc > 0)
   {
      char *buffer;

      system_log(DEBUG_SIGN, NULL, 0,
                 _("AMG Configuration: No. of locked file filters %d"), alfc);
      if ((buffer = malloc(alfbl)) == NULL)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Failed to malloc() %d bytes : %s"),
                    alfbl, strerror(errno));
      }
      else
      {
         char *p_file = alfiles,
              *ptr = buffer;

         for (i = 0; i < alfc; i++)
         {
            while (*p_file != '\0')
            {
               *ptr = *p_file;
               ptr++; p_file++;
            }
            *ptr = '|';
            ptr++; p_file++;
         }
         *(ptr - 1) = '\0';
         system_log(DEBUG_SIGN, NULL, 0,
                    _("AMG Configuration: Add. locked file filters  %s"),
                    buffer);
         free(buffer);
      }
   }

   /* Check if the database has been changed. */
   FD_ZERO(&rset);
   for (;;)
   {
      /* Initialise descriptor set and timeout. */
      FD_SET(amg_cmd_fd, &rset);
      FD_SET(db_update_fd, &rset);
      timeout.tv_usec = 0;
      timeout.tv_sec = rescan_time;

      /* Wait for message x seconds and then continue. */
      status = select(max_fd, &rset, NULL, NULL, &timeout);

      /* Did we get a message from the AFD control */
      /* fifo to shutdown the AMG?                 */
      if ((status > 0) && (FD_ISSET(amg_cmd_fd, &rset)))
      {
         if ((status = read(amg_cmd_fd, fifo_buffer, 10)) > 0)
         {
            /* Show user we got shutdown message. */
            system_log(INFO_SIGN, NULL, 0, _("%s shutting down ...."), AMG);

            /* Do not forget to stop all running jobs. */
            if (dc_pid > 0)
            {
               int j;

               if (com(SHUTDOWN, __FILE__, __LINE__) == INCORRECT)
               {
                  system_log(INFO_SIGN, NULL, 0,
                             _("Giving it another try ..."));
                  (void)com(SHUTDOWN, __FILE__, __LINE__);
               }

               /* Wait for the child to terminate. */
               for (j = 0; j < max_shutdown_time;  j++)
               {
                  if (waitpid(dc_pid, NULL, WNOHANG) == dc_pid)
                  {
                     dc_pid = NOT_RUNNING;
                     break;
                  }
                  else
                  {
                     my_usleep(100000L);
                  }
               }
               if (dc_pid != NOT_RUNNING)
               {
                  if (kill(dc_pid, SIGKILL) != -1)
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                                _("Killed %s (%d) the hard way!"),
#else
                                _("Killed %s (%lld) the hard way!"),
#endif
                                DIR_CHECK, (pri_pid_t)dc_pid);
                     dc_pid = NOT_RUNNING;

                     my_usleep(100000);
                     (void)waitpid(dc_pid, NULL, WNOHANG);
                  }
               }
            }
            if (using_groups == YES)
            {
               free_group_list_mtime();
            }

            stop_flag = 1;

            break;
         }
         else if (status == -1)
              {
                 system_log(FATAL_SIGN, __FILE__, __LINE__,
                            _("Failed to read() from `%s' : %s"),
                            AMG_CMD_FIFO, strerror(errno));
                 exit(INCORRECT);
              }
              else /* == 0 */
              {
                 system_log(FATAL_SIGN, __FILE__, __LINE__,
                            _("Hmm, reading zero from %s."), AMG_CMD_FIFO);
                 exit(INCORRECT);
              }
      }
           /* Did we receive a message from the edit_hc or */
           /* edit_dc dialog?                              */
      else if ((status > 0) && (FD_ISSET(db_update_fd, &rset)))
           {
              int n;

              if ((n = read(db_update_fd, fifo_buffer, fifo_size)) > 0)
              {
                 int count = 0;

#ifdef _FIFO_DEBUG
                 show_fifo_data('R', DB_UPDATE_FIFO, fifo_buffer, n, __FILE__, __LINE__);
#endif
                 while (count < n)
                 {
                    switch (fifo_buffer[count])
                    {
                       case HOST_CONFIG_UPDATE :
                          /* HOST_CONFIG updated by edit_hc. */
                          if (fsa_attach(AMG) != SUCCESS)
                          {
                             system_log(FATAL_SIGN, __FILE__, __LINE__,
                                        _("Could not attach to FSA!"));
                             exit(INCORRECT);
                          }

                          for (i = 0; i < no_of_hosts; i++)
                          {
                             (void)memcpy(hl[i].host_alias, fsa[i].host_alias, MAX_HOSTNAME_LENGTH + 1);
                             (void)memcpy(hl[i].real_hostname[0], fsa[i].real_hostname[0], MAX_REAL_HOSTNAME_LENGTH);
                             (void)memcpy(hl[i].real_hostname[1], fsa[i].real_hostname[1], MAX_REAL_HOSTNAME_LENGTH);
                             (void)memcpy(hl[i].host_toggle_str, fsa[i].host_toggle_str, 5);
                             (void)memcpy(hl[i].proxy_name, fsa[i].proxy_name, MAX_PROXY_NAME_LENGTH);
                             (void)memset(hl[i].fullname, 0, MAX_FILENAME_LENGTH);
                             hl[i].allowed_transfers   = fsa[i].allowed_transfers;
                             hl[i].max_errors          = fsa[i].max_errors;
                             hl[i].retry_interval      = fsa[i].retry_interval;
                             hl[i].transfer_blksize    = fsa[i].block_size;
                             hl[i].successful_retries  = fsa[i].max_successful_retries;
                             hl[i].file_size_offset    = fsa[i].file_size_offset;
                             hl[i].transfer_timeout    = fsa[i].transfer_timeout;
                             hl[i].protocol            = fsa[i].protocol;
                             hl[i].transfer_rate_limit = fsa[i].transfer_rate_limit;
                             hl[i].socksnd_bufsize     = fsa[i].socksnd_bufsize;
                             hl[i].sockrcv_bufsize     = fsa[i].sockrcv_bufsize;
                             hl[i].keep_connected      = fsa[i].keep_connected;
                             hl[i].warn_time           = fsa[i].warn_time;
#ifdef WITH_DUP_CHECK
                             hl[i].dup_check_flag      = fsa[i].dup_check_flag;
                             hl[i].dup_check_timeout   = fsa[i].dup_check_timeout;
#endif
                             hl[i].protocol_options    = fsa[i].protocol_options;
                             hl[i].protocol_options2   = fsa[i].protocol_options2;
                             hl[i].host_status = 0;
                             if (fsa[i].host_status & HOST_ERROR_OFFLINE_STATIC)
                             {
                                hl[i].host_status |= HOST_ERROR_OFFLINE_STATIC;
                             }
                             if (fsa[i].special_flag & HOST_DISABLED)
                             {
                                hl[i].host_status |= HOST_CONFIG_HOST_DISABLED;
                             }
                             if ((fsa[i].special_flag & HOST_IN_DIR_CONFIG) == 0)
                             {
                                hl[i].host_status |= HOST_NOT_IN_DIR_CONFIG;
                             }
                             if (fsa[i].host_status & STOP_TRANSFER_STAT)
                             {
                                hl[i].host_status |= STOP_TRANSFER_STAT;
                             }
                             if (fsa[i].host_status & PAUSE_QUEUE_STAT)
                             {
                                hl[i].host_status |= PAUSE_QUEUE_STAT;
                             }
                             if (fsa[i].host_toggle == HOST_TWO)
                             {
                                hl[i].host_status |= HOST_TWO_FLAG;
                             }
                             if (fsa[i].host_status & DO_NOT_DELETE_DATA)
                             {
                                hl[i].host_status |= DO_NOT_DELETE_DATA;
                             }
                             if (fsa[i].host_status & SIMULATE_SEND_MODE)
                             {
                                hl[i].host_status |= SIMULATE_SEND_MODE;
                             }
                          }

                          /* Increase HOST_CONFIG counter so others */
                          /* can see there was a change.            */
                          (*(unsigned char *)((char *)fsa - AFD_WORD_OFFSET + SIZEOF_INT))++;
                          (void)fsa_detach(YES);

                          notify_dir_check();
                          hc_old_time = write_host_config(no_of_hosts, host_config_file, hl);
                          system_log(INFO_SIGN, __FILE__, __LINE__,
                                     _("Updated HOST_CONFIG file."));
                          break;

                       case DIR_CONFIG_UPDATE :
                          /* DIR_CONFIG updated by edit_dc. */
                          system_log(INFO_SIGN, __FILE__, __LINE__,
                                     _("This function has not yet been implemented."));
                          break;

                       case REREAD_HOST_CONFIG :
                       case REREAD_HOST_CONFIG_VERBOSE1 :
                       case REREAD_HOST_CONFIG_VERBOSE2 :
                          {
                             count++;
                             if ((n - count) < SIZEOF_PID_T)
                             {
                                system_log(ERROR_SIGN, __FILE__, __LINE__,
                                           _("Unable to handle request since we only have %d bytes buffered but need %d. Discarding buffer!"),
                                           (n - count), SIZEOF_PID_T);
                                count = n;
                             }
                             else
                             {
                                int          hc_result,
                                             db_update_reply_fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
                                int          db_update_reply_readfd;
#endif
                                unsigned int hc_warn_counter;
                                pid_t        ret_pid;
                                char         db_update_reply_fifo[MAX_PATH_LENGTH],
                                             flag = fifo_buffer[count - 1],
                                             uc_reply_name[MAX_PATH_LENGTH];
                                FILE         *uc_reply_fp = NULL;

                                (void)memcpy(&ret_pid, &fifo_buffer[count],
                                             SIZEOF_PID_T);
                                count += (SIZEOF_PID_T - 1);
                                (void)snprintf(db_update_reply_fifo, MAX_PATH_LENGTH,
#if SIZEOF_PID_T == 4
                                               "%s%s%s%d",
#else
                                               "%s%s%s%lld",
#endif
                                               p_work_dir, FIFO_DIR,
                                               DB_UPDATE_REPLY_FIFO,
                                               (pri_pid_t)ret_pid);
#ifdef WITHOUT_FIFO_RW_SUPPORT
                                if (open_fifo_rw(db_update_reply_fifo,
                                                 &db_update_reply_readfd,
                                                 &db_update_reply_fd) == -1)
#else
                                if ((db_update_reply_fd = open(db_update_reply_fifo,
                                                               O_RDWR)) == -1)
#endif
                                {
                                   if (errno != ENOENT)
                                   {
                                      system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                 _("Failed to open() `%s' : %s"),
                                                 db_update_reply_fifo,
                                                 strerror(errno));
                                   }
                                   /*
                                    * Lets continue even if we are not able
                                    * to send a responce to the process that
                                    * initiated the update.
                                    */
                                }

                                /*
                                 * Check if user wants more information, that
                                 * is REREAD_HOST_CONFIG_VERBOSE1 or more.
                                 */
                                if (flag > REREAD_HOST_CONFIG)
                                {
                                   (void)snprintf(uc_reply_name,
                                                  MAX_PATH_LENGTH,
#if SIZEOF_PID_T == 4
                                                  "%s%s%s.%d",
#else
                                                  "%s%s%s.%lld",
#endif
                                                  p_work_dir, FIFO_DIR,
                                                  DB_UPDATE_REPLY_DEBUG_FILE,
                                                  (pri_pid_t)ret_pid);

                                   /* Ensure file will be created as 644. */
                                   (void)umask(S_IWGRP | S_IWOTH);
                                   if ((uc_reply_fp = fopen(uc_reply_name,
                                                            "w")) == NULL)
                                   {
                                      system_log(WARN_SIGN, __FILE__, __LINE__,
                                                 _("Failed to fopen() `%s' : %s"),
                                                 uc_reply_name,
                                                 strerror(errno));
                                   }
                                   (void)umask(0);
                                }
                                else
                                {
                                   uc_reply_name[0] = '\0';
                                }

                                hc_warn_counter = 0;
                                hc_result = reread_host_config(&hc_old_time,
                                                               NULL, NULL, NULL,
                                                               NULL,
                                                               &hc_warn_counter,
                                                               uc_reply_fp,
                                                               YES);
                                if (hc_result == NO_CHANGE_IN_HOST_CONFIG)
                                {
                                   event_log(0L, EC_GLOB, ET_MAN, EA_REREAD_HOST_CONFIG,
                                             "no change in HOST_CONFIG");
                                }
                                else if (hc_result == HOST_CONFIG_RECREATED)
                                     {
                                        event_log(0L, EC_GLOB, ET_MAN, EA_REREAD_HOST_CONFIG,
                                                  "recreated HOST_CONFIG");
                                     }
                                     else
                                     {
                                        event_log(0L, EC_GLOB, ET_MAN, EA_REREAD_HOST_CONFIG,
                                                  "with %d warnings",
                                                  hc_warn_counter);
                                     }

                                /*
                                 * Do not forget to start dir_check if we have
                                 * stopped it!
                                 */
                                if (dc_pid == NOT_RUNNING)
                                {
                                   dc_pid = make_process_amg(work_dir,
                                                             DC_PROC_NAME,
                                                             rescan_time,
                                                             max_no_proc,
                                                             (create_source_dir == YES) ? create_source_dir_mode : 0,
                                                             0);
                                   if (pid_list != NULL)
                                   {
                                      *(pid_t *)(pid_list + ((DC_NO + 1) * sizeof(pid_t))) = dc_pid;
                                   }
                                   system_log(INFO_SIGN, __FILE__, __LINE__,
                                              _("Restarted %s."), DC_PROC_NAME);
                                }

                                if (flag > REREAD_HOST_CONFIG)
                                {
                                   if (uc_reply_fp != NULL)
                                   {
                                      (void)fflush(uc_reply_fp);
                                      if (fclose(uc_reply_fp) == EOF)
                                      {
                                         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                    "Failed to fclose() `%s' : %s",
                                                    uc_reply_name,
                                                    strerror(errno));
                                      }
                                   }
                                }

                                if (db_update_reply_fd != -1)
                                {
                                   char reply_buffer[MAX_UHC_RESPONCE_LENGTH];

                                   if ((dc_pid <= 0) &&
                                       ((hc_result == HOST_CONFIG_DATA_CHANGED) ||
                                        (hc_result == HOST_CONFIG_DATA_ORDER_CHANGED) ||
                                        (hc_result == HOST_CONFIG_ORDER_CHANGED)))
                                   {
                                      hc_result = HOST_CONFIG_UPDATED_DC_PROBLEMS;
                                   }
                                   (void)memcpy(reply_buffer, &hc_result,
                                                SIZEOF_INT);
                                   (void)memcpy(&reply_buffer[SIZEOF_INT],
                                                &hc_warn_counter, SIZEOF_INT);
                                   if (write(db_update_reply_fd, reply_buffer,
                                             MAX_UHC_RESPONCE_LENGTH) != MAX_UHC_RESPONCE_LENGTH)
                                   {
                                      system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                 _("Failed to write() reply for reread HOST_CONFIG request : %s"),
                                                 strerror(errno));
                                   }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                                   if (close(db_update_reply_readfd) == -1)
                                   {
                                      system_log(WARN_SIGN, __FILE__, __LINE__,
                                                 _("close() error : %s"),
                                                 strerror(errno));
                                   }
#endif
                                   if (close(db_update_reply_fd) == -1)
                                   {
                                      system_log(WARN_SIGN, __FILE__, __LINE__,
                                                 _("close() error : %s"),
                                                 strerror(errno));
                                   }
                                }
                                check_every_fra_disable_all_flag();
                             }
                          }
                          break;

                       case REREAD_DIR_CONFIG :
                       case REREAD_DIR_CONFIG_VERBOSE1 :
                       case REREAD_DIR_CONFIG_VERBOSE2 :
                          {
                             count++;
                             if ((n - count) < SIZEOF_PID_T)
                             {
                                system_log(ERROR_SIGN, __FILE__, __LINE__,
                                           _("Unable to handle request since we only have %d bytes buffered but need %d. Discarding buffer!"),
                                           (n - count), SIZEOF_PID_T);
                                count = n;
                             }
                             else
                             {
                                int          dc_changed = NO,
                                             db_update_reply_fd,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                                             db_update_reply_readfd,
#endif
                                             dc_result,
                                             hc_result,
                                             stat_error_set;
                                unsigned int dc_warn_counter,
                                             hc_warn_counter;
                                off_t        db_size = 0;
                                pid_t        ret_pid;
                                char         db_update_reply_fifo[MAX_PATH_LENGTH],
                                             flag = fifo_buffer[count - 1],
                                             uc_reply_name[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
                                struct statx stat_buf;
#else
                                struct stat  stat_buf;
#endif
                                FILE         *uc_reply_fp = NULL;

                                (void)memcpy(&ret_pid, &fifo_buffer[count],
                                             SIZEOF_PID_T);
                                count += (SIZEOF_PID_T - 1);
                                (void)snprintf(db_update_reply_fifo, MAX_PATH_LENGTH,
#if SIZEOF_PID_T == 4
                                               "%s%s%s%d",
#else
                                               "%s%s%s%lld",
#endif
                                               p_work_dir, FIFO_DIR,
                                               DB_UPDATE_REPLY_FIFO,
                                               (pri_pid_t)ret_pid);
#ifdef WITHOUT_FIFO_RW_SUPPORT
                                if (open_fifo_rw(db_update_reply_fifo,
                                                 &db_update_reply_readfd,
                                                 &db_update_reply_fd) == -1)
#else
                                if ((db_update_reply_fd = open(db_update_reply_fifo,
                                                               O_RDWR)) == -1)
#endif
                                {
                                   if (errno != ENOENT)
                                   {
                                      system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                 _("Failed to open() `%s' : %s"),
                                                 db_update_reply_fifo,
                                                 strerror(errno));
                                   }
                                   /*
                                    * Lets continue even if we are not able
                                    * to send a responce to the process that
                                    * initiated the update.
                                    */
                                }

                                /*
                                 * Check if user wants more information, that
                                 * is REREAD_DIR_CONFIG_VERBOSE1 or more.
                                 */
                                if (flag > REREAD_DIR_CONFIG)
                                {
                                   (void)snprintf(uc_reply_name,
                                                  MAX_PATH_LENGTH,
#if SIZEOF_PID_T == 4
                                                  "%s%s%s.%d",
#else
                                                  "%s%s%s.%lld",
#endif
                                                  p_work_dir, FIFO_DIR,
                                                  DB_UPDATE_REPLY_DEBUG_FILE,
                                                  (pri_pid_t)ret_pid);

                                   /* Ensure file will be created as 644. */
                                   (void)umask(S_IWGRP | S_IWOTH);
                                   if ((uc_reply_fp = fopen(uc_reply_name,
                                                            "w")) == NULL)
                                   {
                                      system_log(WARN_SIGN, __FILE__, __LINE__,
                                                 _("Failed to fopen() `%s' : %s"),
                                                 uc_reply_name,
                                                 strerror(errno));
                                   }
                                   (void)umask(0);
                                }
                                else
                                {
                                   uc_reply_name[0] = '\0';
                                }

                                hc_warn_counter = 0;
                                hc_result = NO_CHANGE_IN_HOST_CONFIG;
                                dc_warn_counter = 0;
                                dc_result = DIR_CONFIG_NOTHING_DONE;
                                stat_error_set = NO;

                                if (dc_names_can_change == YES)
                                {
                                   /* First lets check if a change really took place. */
                                   dc_changed = check_full_dc_name_changes();

                                   if (dc_changed == YES)
                                   {
                                      lookup_dc_id(&dc_dcl, no_of_dir_configs);
                                   }
                                }

                                for (i = 0; i < no_of_dir_configs; i++)
                                {
                                   if (dc_dcl[i].in_list == NEITHER)
                                   {
                                      db_size += dc_dcl[i].size;
                                      dc_dcl[i].in_list = YES;
                                      dc_changed = YES;
                                   }
                                   else
                                   {
#ifdef HAVE_STATX
                                      if (statx(0, dc_dcl[i].dir_config_file,
                                                AT_STATX_SYNC_AS_STAT,
                                                STATX_SIZE | STATX_MTIME,
                                                &stat_buf) == -1)
#else
                                      if (stat(dc_dcl[i].dir_config_file,
                                               &stat_buf) == -1)
#endif
                                      {
                                         system_log(WARN_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                                                    _("Failed to statx() `%s' : %s"),
#else
                                                    _("Failed to stat() `%s' : %s"),
#endif
                                                    dc_dcl[i].dir_config_file,
                                                    strerror(errno));
                                         stat_error_set = YES;
                                      }
                                      else
                                      {
#ifdef HAVE_STATX
                                         if (dc_dcl[i].dc_old_time != stat_buf.stx_mtime.tv_sec)
#else
                                         if (dc_dcl[i].dc_old_time != stat_buf.st_mtime)
#endif
                                         {
                                            if ((flag > REREAD_DIR_CONFIG_VERBOSE1) &&
                                                (uc_reply_fp != NULL))
                                            {
                                               (void)fprintf(uc_reply_fp,
#if SIZEOF_TIME_T == 4
                                                             "%s [!%x] %s modification time changed %ld -> %ld\n",
#else
                                                             "%s [!%x] %s modification time changed %lld -> %lld\n",
#endif
                                                             DEBUG_SIGN,
                                                             dc_dcl[i].dc_id,
                                                             dc_dcl[i].dir_config_file,
                                                             (pri_time_t)dc_dcl[i].dc_old_time,
#ifdef HAVE_STATX
                                                             (pri_time_t)stat_buf.stx_mtime.tv_sec
#else
                                                             (pri_time_t)stat_buf.st_mtime
#endif
                                                            );
                                            }
#ifdef HAVE_STATX
                                            dc_dcl[i].dc_old_time = stat_buf.stx_mtime.tv_sec;
#else
                                            dc_dcl[i].dc_old_time = stat_buf.st_mtime;
#endif
                                            dc_changed = YES;
                                         }
#ifdef HAVE_STATX
                                         db_size += stat_buf.stx_size;
#else
                                         db_size += stat_buf.st_size;
#endif
                                      }
                                   }
                                }

                                if (using_groups == YES)
                                {
                                   if (check_group_list_mtime() == YES)
                                   {
                                      dc_changed = YES;
                                   }
                                }

                                if (db_size > 0)
                                {
                                   if (dc_changed == YES)
                                   {
                                      int              old_no_of_hosts = 0,
                                                       old_using_groups = using_groups,
                                                       rewrite_host_config = NO;
                                      size_t           old_size = 0;
                                      struct host_list *old_hl = NULL;

                                      /* Set flag to indicate that we are */
                                      /* rereading the DIR_CONFIG.        */
                                      p_afd_status->amg_jobs |= REREADING_DIR_CONFIG;
                                      inform_fd_about_fsa_change();

                                      /* Better check if there was a change in HOST_CONFIG. */
                                      hc_result = reread_host_config(&hc_old_time,
                                                                     &old_no_of_hosts,
                                                                     &rewrite_host_config,
                                                                     &old_size, &old_hl,
                                                                     &hc_warn_counter,
                                                                     uc_reply_fp,
                                                                     NO);
                                      event_log(0L, EC_GLOB, ET_AUTO, EA_REREAD_HOST_CONFIG,
                                                "with %d warnings", hc_warn_counter);

                                      dc_result = reread_dir_config(dc_changed,
                                                                    db_size,
                                                                    &hc_old_time,
                                                                    old_no_of_hosts,
                                                                    rewrite_host_config,
                                                                    old_size,
                                                                    rescan_time,
                                                                    max_no_proc,
                                                                    &using_groups,
                                                                    &dc_warn_counter,
                                                                    uc_reply_fp,
                                                                    ret_pid,
                                                                    old_hl);
                                      event_log(0L, EC_GLOB, ET_MAN, EA_REREAD_DIR_CONFIG,
                                                "with %d warnings", dc_warn_counter);
                                      if ((old_using_groups == YES) &&
                                          (using_groups == NO))
                                      {
                                         free_group_list_mtime();
                                      }
                                      else if ((old_using_groups == NO) &&
                                               (using_groups == YES))
                                           {
                                              init_group_list_mtime();
                                           }
                                   }
                                   else
                                   {
                                      if (no_of_dir_configs > 1)
                                      {
                                         system_log(INFO_SIGN, NULL, 0,
                                                    _("There is no change in all DIR_CONFIG's."));
                                      }
                                      else
                                      {
                                         system_log(INFO_SIGN, NULL, 0,
                                                    _("There is no change in DIR_CONFIG."));
                                      }
                                      dc_result = NO_CHANGE_IN_DIR_CONFIG;
                                      event_log(0L, EC_GLOB, ET_MAN, EA_REREAD_DIR_CONFIG,
                                                "no change in DIR_CONFIG");
                                   }
                                }
                                else
                                {
                                   if (stat_error_set == NO)
                                   {
                                      if (no_of_dir_configs > 1)
                                      {
                                         system_log(WARN_SIGN, NULL, 0,
                                                    _("All DIR_CONFIG files are empty."));
                                      }
                                      else
                                      {
                                         system_log(WARN_SIGN, NULL, 0,
                                                    _("DIR_CONFIG file is empty."));
                                      }
                                      dc_result = DIR_CONFIG_EMPTY;
                                      event_log(0L, EC_GLOB, ET_MAN, EA_REREAD_DIR_CONFIG,
                                                "DIR_CONFIG is empty");
                                   }
                                   else
                                   {
                                      dc_result = DIR_CONFIG_ACCESS_ERROR;
                                      event_log(0L, EC_GLOB, ET_MAN, EA_REREAD_DIR_CONFIG,
                                                "no access to DIR_CONFIG");
                                   }
                                }

                                if (flag > REREAD_DIR_CONFIG)
                                {
                                   if (uc_reply_fp != NULL)
                                   {
                                      (void)fflush(uc_reply_fp);
                                      if (fclose(uc_reply_fp) == EOF)
                                      {
                                         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                    "Failed to fclose() `%s' : %s",
                                                    uc_reply_name,
                                                    strerror(errno));
                                      }
                                   }
                                }

                                if (db_update_reply_fd != -1)
                                {
                                   char reply_buffer[MAX_UDC_RESPONCE_LENGTH];

                                   (void)memcpy(reply_buffer, &hc_result,
                                                SIZEOF_INT);
                                   (void)memcpy(&reply_buffer[SIZEOF_INT],
                                                &hc_warn_counter, SIZEOF_INT);
                                   (void)memcpy(&reply_buffer[SIZEOF_INT + SIZEOF_INT],
                                                &dc_result, SIZEOF_INT);
                                   (void)memcpy(&reply_buffer[SIZEOF_INT + SIZEOF_INT + SIZEOF_INT],
                                                &dc_warn_counter, SIZEOF_INT);
                                   if (write(db_update_reply_fd, reply_buffer,
                                             MAX_UDC_RESPONCE_LENGTH) != MAX_UDC_RESPONCE_LENGTH)
                                   {
                                      system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                 _("Failed to write() reply for reread DIR_CONFIG request : %s"),
                                                 strerror(errno));
                                   }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                                   if (close(db_update_reply_readfd) == -1)
                                   {
                                      system_log(WARN_SIGN, __FILE__, __LINE__,
                                                 _("close() error : %s"),
                                                 strerror(errno));
                                   }
#endif
                                   if (close(db_update_reply_fd) == -1)
                                   {
                                      system_log(WARN_SIGN, __FILE__, __LINE__,
                                                 _("close() error : %s"),
                                                 strerror(errno));
                                   }
                                }
                             }
                          }
                          break;

                       default : 
                          /* Assume we are reading garbage. */
                          system_log(INFO_SIGN, __FILE__, __LINE__,
                                     _("Reading garbage (%d) on fifo %s"),
                                     (int)fifo_buffer[count], DB_UPDATE_FIFO);
                          break;
                    }
                    count++;
                 } /* while (count < n) */
              }
           }
           /* Did we get a timeout. */
      else if (status == 0)
           {
              /*
               * Check if the HOST_CONFIG file still exists. If not recreate
               * it from the internal current host_list structure.
               */
#ifdef HAVE_STATX
              if (statx(0, host_config_file, AT_STATX_SYNC_AS_STAT,
                        0, &stat_buf) == -1)
#else
              if (stat(host_config_file, &stat_buf) == -1)
#endif
              {
                 if (errno == ENOENT)
                 {
                    system_log(INFO_SIGN, NULL, 0,
                               _("Recreating HOST_CONFIG file with %d hosts."),
                               no_of_hosts);
                    hc_old_time = write_host_config(no_of_hosts,
                                                    host_config_file, hl);
                 }
                 else
                 {
                    system_log(FATAL_SIGN, __FILE__, __LINE__,
                               _("Failed to stat() `%s' : %s"),
                               host_config_file, strerror(errno));
                    exit(INCORRECT);
                 }
              }
#ifdef WITH_ONETIME
              check_onetime_config();
#endif
           }
           else 
           {
              system_log(FATAL_SIGN, __FILE__, __LINE__,
                         _("select() error : %s"), strerror(errno));
              exit(INCORRECT);
           }

      /* Check if any process died. */
      if (dc_pid > 0) 
      {
         if ((amg_zombie_check(&dc_pid, WNOHANG) == YES) &&
             (data_length > 0))
         {
            /* So what do we do now? */
            /* For now lets only tell the user that the job died. */
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Job %s has died!"), DC_PROC_NAME);

            dc_pid = make_process_amg(work_dir, DC_PROC_NAME,
                                      rescan_time, max_no_proc,
                                      (create_source_dir == YES) ? create_source_dir_mode : 0,
                                      0);
            if (pid_list != NULL)
            {
               *(pid_t *)(pid_list + ((DC_NO + 1) * sizeof(pid_t))) = dc_pid;
            }
            system_log(INFO_SIGN, __FILE__, __LINE__,
                       _("Restarted %s."), DC_PROC_NAME);
         }
      } /* if (dc_pid > 0) */
   } /* for (;;) */

#ifdef _DEBUG
   /* Don't forget to close debug file. */
   (void)fclose(p_debug_file);
#endif

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++ get_afd_config_value() ++++++++++++++++++++++*/
static void
get_afd_config_value(int          *rescan_time,
                     int          *max_no_proc,
                     int          *max_process_per_dir,
                     mode_t       *create_source_dir_mode,
                     unsigned int *max_copied_files,
                     off_t        *max_copied_file_size,
                     int          *default_delete_files_flag,
                     int          *default_old_file_time,
                     int          *remove_unused_hosts,
#ifdef WITH_INOTIFY
                     unsigned int *default_inotify_flag,
#endif
                     time_t       *default_info_time,
                     time_t       *default_warn_time,
                     int          *default_no_parallel_jobs,
                     int          *default_max_errors,
                     int          *default_retry_interval,
                     int          *default_transfer_blocksize,
                     int          *default_successful_retries,
                     long         *default_transfer_timeout,
                     unsigned int *default_error_offline_flag,
                     int          *create_source_dir,
                     int          *max_shutdown_time)
{
   char *buffer,
        config_file[MAX_PATH_LENGTH];

   (void)snprintf(config_file, MAX_PATH_LENGTH, "%s%s%s",
                  p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   if ((eaccess(config_file, F_OK) == 0) &&
       (read_file_no_cr(config_file, &buffer, YES, __FILE__, __LINE__) != INCORRECT))
   {
      size_t length,
             max_length;
      char   *ptr,
             value[MAX_ADD_LOCKED_FILES_LENGTH];

#ifdef HAVE_SETPRIORITY
      if (get_definition(buffer, AMG_PRIORITY_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         if (setpriority(PRIO_PROCESS, 0, atoi(value)) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Failed to set priority to %d : %s",
                       atoi(value), strerror(errno));
         }
      }
#endif
      if (get_definition(buffer, AMG_DIR_RESCAN_TIME_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         *rescan_time = atoi(value);
         if (*rescan_time < 1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("Incorrect value (%d) set in AFD_CONFIG for %s. Setting to default %d."),
                       *rescan_time, AMG_DIR_RESCAN_TIME_DEF,
                       DEFAULT_RESCAN_TIME);
            *rescan_time = DEFAULT_RESCAN_TIME;
         }
      }
      if (get_definition(buffer, MAX_NO_OF_DIR_CHECKS_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         *max_no_proc = atoi(value);
         if ((*max_no_proc < 1) || (*max_no_proc > 10240))
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("Incorrect value (%d) set in AFD_CONFIG for %s. Setting to default %d."),
                       *max_no_proc, MAX_NO_OF_DIR_CHECKS_DEF,
                       MAX_NO_OF_DIR_CHECKS);
            *max_no_proc = MAX_NO_OF_DIR_CHECKS;
         }
      }
      if (get_definition(buffer, MAX_PROCESS_PER_DIR_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         *max_process_per_dir = atoi(value);
         if ((*max_process_per_dir < 1) || (*max_process_per_dir > 10240))
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("Incorrect value (%d) set in AFD_CONFIG for %s. Setting to default %d."),
                       *max_process_per_dir, MAX_PROCESS_PER_DIR_DEF,
                       MAX_PROCESS_PER_DIR);
            *max_process_per_dir = MAX_PROCESS_PER_DIR;
         }
         if (*max_process_per_dir > *max_no_proc)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("%s (%d) may not be larger than %s (%d) in AFD_CONFIG. Setting to %d."),
                       MAX_PROCESS_PER_DIR_DEF, *max_process_per_dir, 
                       MAX_NO_OF_DIR_CHECKS_DEF, *max_no_proc, *max_no_proc);
            *max_process_per_dir = MAX_PROCESS_PER_DIR;
         }
      }
#ifdef WITH_INOTIFY
      if (get_definition(buffer, DEFAULT_INOTIFY_FLAG_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         *default_inotify_flag = (unsigned int)atoi(value);
         if (*default_inotify_flag > (INOTIFY_RENAME_FLAG | INOTIFY_CLOSE_FLAG | INOTIFY_CREATE_FLAG | INOTIFY_DELETE_FLAG | INOTIFY_ATTRIB_FLAG))
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("Incorrect value (%u) set in AFD_CONFIG for %s. Setting to default %u."),
                       *default_inotify_flag, DEFAULT_INOTIFY_FLAG_DEF,
                       DEFAULT_INOTIFY_FLAG);
            *default_inotify_flag = DEFAULT_INOTIFY_FLAG;
         }
      }
#endif
      if (get_definition(buffer, IGNORE_FIRST_ERRORS_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         if (atoi(value) > 255)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("Incorrect value (%d) set in AFD_CONFIG for %s. Setting to default 0."),
                       atoi(value), AMG_DIR_RESCAN_TIME_DEF);
            ignore_first_errors = 0;
         }
         else
         {
            ignore_first_errors = (unsigned char)atoi(value);
         }
      }
      else
      {
         ignore_first_errors = 0;
      }
      if (get_definition(buffer, CREATE_SOURCE_DIR_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         if (((value[0] == 'n') || (value[0] == 'N')) &&
             ((value[1] == 'o') || (value[1] == 'O')) &&
             ((value[2] == '\0') || (value[2] == ' ') || (value[2] == '\t')))
         {
            *create_source_dir = NO;
         }
         else if (((value[0] == 'y') || (value[0] == 'Y')) &&
                  ((value[1] == 'e') || (value[1] == 'E')) &&
                  ((value[2] == 's') || (value[2] == 'S')) &&
                  ((value[3] == '\0') || (value[3] == ' ') || (value[3] == '\t')))
              {
                 *create_source_dir = YES;
              }
              else
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            _("Only YES or NO (and not `%s') are possible for %s in AFD_CONFIG. Setting to default: %s"),
                            value, CREATE_SOURCE_DIR_DEF,
                            (*create_source_dir == YES) ? "YES" : "NO");
              }
      }
      if (get_definition(buffer, CREATE_SOURCE_DIR_MODE_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         *create_source_dir_mode = (unsigned int)atoi(value);
         if ((*create_source_dir_mode <= 700) ||
             (*create_source_dir_mode > 7777))
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("Invalid mode %u set in AFD_CONFIG for %s. Setting to default %d."),
                       *create_source_dir_mode, CREATE_SOURCE_DIR_MODE_DEF,
                       DIR_MODE);
            *create_source_dir_mode = DIR_MODE;
         }
         else /* Convert octal to decimal. */
         {
            int kk,
                ki = 1,
                ko = 0,
                oct_mode;

            oct_mode = *create_source_dir_mode;
            while (oct_mode > 0)
            {
               kk = oct_mode % 10;
               ko = ko + (kk * ki);
               ki = ki * 8;
               oct_mode = oct_mode / 10;
            }
            *create_source_dir_mode = ko;
         }
      }
      if (get_definition(buffer, REMOVE_UNUSED_HOSTS_DEF, NULL, 0) != NULL)
      {
         *remove_unused_hosts = YES;
      }
      if (get_definition(buffer, MAX_COPIED_FILE_SIZE_DEF,
                         value, MAX_INT_LENGTH) != NULL)  
      {
         /* The value is given in megabytes, so convert to bytes. */
         *max_copied_file_size = atoi(value) * 1024;
         if ((*max_copied_file_size < 1) || (*max_copied_file_size > 2097152000))
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("The specified variable for %s in AFD_CONFIG is not int the allowed range > 0 and < 1048576000, setting to default %d."),
                       MAX_COPIED_FILE_SIZE_DEF, MAX_COPIED_FILE_SIZE);
            *max_copied_file_size = MAX_COPIED_FILE_SIZE * MAX_COPIED_FILE_SIZE_UNIT;
         }
      }
      else
      {
         *max_copied_file_size = MAX_COPIED_FILE_SIZE * MAX_COPIED_FILE_SIZE_UNIT;
      }
      if (get_definition(buffer, MAX_COPIED_FILES_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         *max_copied_files = atoi(value);
         if (*max_copied_files < 1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("The specified variable for %s in AFD_CONFIG is less then 0, setting to default %d."),
                       MAX_COPIED_FILES_DEF, MAX_COPIED_FILES);
            *max_copied_files = MAX_COPIED_FILES;
         }
         else if (*max_copied_files > MAX_FILE_BUFFER_SIZE)
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            _("The specified variable for %s in AFD_CONFIG is more then the allowed maximum %d, setting to maximum %d."),
                            MAX_COPIED_FILES_DEF, MAX_FILE_BUFFER_SIZE,
                            MAX_FILE_BUFFER_SIZE);
                 *max_copied_files = MAX_FILE_BUFFER_SIZE;
              }
      }
      else
      {
         *max_copied_files = MAX_COPIED_FILES;
      }
      if (get_definition(buffer, DEFAULT_OLD_FILE_TIME_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         *default_old_file_time = atoi(value);
         if ((*default_old_file_time < 1) || (*default_old_file_time > 596523))
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("Incorrect value (%d) set in AFD_CONFIG for %s. Setting to default %d."),
                       *default_old_file_time, DEFAULT_OLD_FILE_TIME_DEF,
                       DEFAULT_OLD_FILE_TIME);
            *default_old_file_time = DEFAULT_OLD_FILE_TIME;
         }
      }
      if (get_definition(buffer, ADDITIONAL_LOCKED_FILES_DEF,
                         value, MAX_ADD_LOCKED_FILES_LENGTH) != NULL)
      {
         ptr = value;
         length = 0;
         alfc = 0;
         while (*ptr != '\0')
         {
            while ((*ptr == ' ') && (*ptr == '\t'))
            {
               ptr++;
            }
            if (*ptr != '!')
            {
               length++;
            }
            while ((*ptr != '|') && (*ptr != '\0'))
            {
               ptr++; length++;
            }
            while (*ptr == '|')
            {
               ptr++;
            }
            alfc++;
            length++;
         }

         if (alfc > 0)
         {
            if ((alfiles = malloc(length)) == NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("malloc() error : %s"), strerror(errno));
               alfbl = 0;
            }
            else
            {
               char *p_file = alfiles;

               ptr = value;
               while (*ptr != '\0')
               {
                  while ((*ptr == ' ') && (*ptr == '\t'))
                  {
                     ptr++;
                  }
                  if (*ptr != '!')
                  {
                     *p_file = '!';
                     p_file++;
                  }
                  while ((*ptr != '|') && (*ptr != '\0'))
                  {
                     *p_file = *ptr;
                     ptr++; p_file++;
                  }
                  while (*ptr == '|')
                  {
                     ptr++;
                  }
                  *p_file = '\0';
                  p_file++;
               }
               alfbl = length;
            }
         }
      }
      else
      {
         alfc = 0;
         alfbl = 0;
         length = 0; /* Silence compiler. */
      }
      if (get_definition(buffer, DEFAULT_DIR_INFO_TIME_DEF,
                         value, MAX_LONG_LENGTH) != NULL)
      {
         *default_info_time = (time_t)atol(value);
         if (*default_info_time < 0)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("A value less then 0 for AFD_CONFIG variable %s is not possible, setting default %ld"),
                       DEFAULT_DIR_INFO_TIME_DEF, DEFAULT_DIR_INFO_TIME);
            *default_info_time = DEFAULT_DIR_INFO_TIME;
         }
      }
      if (get_definition(buffer, DEFAULT_DIR_WARN_TIME_DEF,
                         value, MAX_LONG_LENGTH) != NULL)
      {
         *default_warn_time = (time_t)atol(value);
         if (*default_warn_time < 0)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("A value less then 0 for AFD_CONFIG variable %s is not possible, setting default %ld"),
                       DEFAULT_DIR_WARN_TIME_DEF, DEFAULT_DIR_WARN_TIME);
            *default_warn_time = DEFAULT_DIR_WARN_TIME;
         }
      }
      if (get_definition(buffer, DEFAULT_DELETE_FILES_FLAG_DEF,
                         config_file, MAX_PATH_LENGTH) != NULL)
      {
         ptr = config_file;
         do
         {
            while (((*ptr == ' ') || (*ptr == '\t') || (*ptr == ',')) &&
                   ((ptr + 1) < (config_file + MAX_PATH_LENGTH - 1)))
            {
               ptr++;
            }
            /* Unknown */
            if (((ptr + 7) < (config_file + MAX_PATH_LENGTH - 1)) &&
                ((*ptr == 'U') || (*ptr == 'u')) &&
                ((*(ptr + 1) == 'N') || (*(ptr + 1) == 'n')) &&
                ((*(ptr + 2) == 'K') || (*(ptr + 2) == 'k')) &&
                ((*(ptr + 3) == 'N') || (*(ptr + 3) == 'n')) &&
                ((*(ptr + 4) == 'O') || (*(ptr + 4) == 'o')) &&
                ((*(ptr + 5) == 'W') || (*(ptr + 5) == 'w')) &&
                ((*(ptr + 6) == 'N') || (*(ptr + 6) == 'n')) &&
                ((*(ptr + 7) == ' ') || (*(ptr + 7) == '\t') ||
                 (*(ptr + 7) == ',') || (*(ptr + 7) == '\0')))
            {
               ptr += 7;
               if ((*default_delete_files_flag & UNKNOWN_FILES) == 0)
               {
                  *default_delete_files_flag |= UNKNOWN_FILES;
               }
            }
                 /* Queued */
            else if (((ptr + 6) < (config_file + MAX_PATH_LENGTH - 1)) &&
                     ((*ptr == 'Q') || (*ptr == 'q')) &&
                     ((*(ptr + 1) == 'U') || (*(ptr + 1) == 'u')) &&
                     ((*(ptr + 2) == 'E') || (*(ptr + 2) == 'e')) &&
                     ((*(ptr + 3) == 'U') || (*(ptr + 3) == 'u')) &&
                     ((*(ptr + 4) == 'E') || (*(ptr + 4) == 'e')) &&
                     ((*(ptr + 5) == 'D') || (*(ptr + 5) == 'd')) &&
                     ((*(ptr + 6) == ' ') || (*(ptr + 6) == '\t') ||
                      (*(ptr + 6) == ',') || (*(ptr + 6) == '\0')))
                 {
                    ptr += 6;
                    if ((*default_delete_files_flag & QUEUED_FILES) == 0)
                    {
                       *default_delete_files_flag |= QUEUED_FILES;
                    }
                 }
                 /* Locked */
            else if (((ptr + 6) < (config_file + MAX_PATH_LENGTH - 1)) &&
                     ((*ptr == 'L') || (*ptr == 'l')) &&
                     ((*(ptr + 1) == 'O') || (*(ptr + 1) == 'o')) &&
                     ((*(ptr + 2) == 'C') || (*(ptr + 2) == 'c')) &&
                     ((*(ptr + 3) == 'K') || (*(ptr + 3) == 'k')) &&
                     ((*(ptr + 4) == 'E') || (*(ptr + 4) == 'e')) &&
                     ((*(ptr + 5) == 'D') || (*(ptr + 5) == 'd')) &&
                     ((*(ptr + 6) == ' ') || (*(ptr + 6) == '\t') ||
                      (*(ptr + 6) == ',') || (*(ptr + 6) == '\0')))
                 {
                    ptr += 6;
                    if ((*default_delete_files_flag & OLD_LOCKED_FILES) == 0)
                    {
                       *default_delete_files_flag |= OLD_LOCKED_FILES;
                    }
                 }
                 /* RLocked */
            else if (((ptr + 6) < (config_file + MAX_PATH_LENGTH - 1)) &&
                     ((*ptr == 'R') || (*ptr == 'r')) &&
                     ((*(ptr + 1) == 'L') || (*(ptr + 1) == 'l')) &&
                     ((*(ptr + 2) == 'O') || (*(ptr + 2) == 'o')) &&
                     ((*(ptr + 3) == 'C') || (*(ptr + 3) == 'c')) &&
                     ((*(ptr + 4) == 'K') || (*(ptr + 4) == 'k')) &&
                     ((*(ptr + 5) == 'E') || (*(ptr + 5) == 'e')) &&
                     ((*(ptr + 6) == 'D') || (*(ptr + 6) == 'd')) &&
                     ((*(ptr + 7) == ' ') || (*(ptr + 7) == '\t') ||
                      (*(ptr + 7) == ',') || (*(ptr + 7) == '\0')))
                 {
                    ptr += 7;
                    if ((*default_delete_files_flag & OLD_RLOCKED_FILES) == 0)
                    {
                       *default_delete_files_flag |= OLD_RLOCKED_FILES;
                    }
                 }
                 /* ILocked */
            else if (((ptr + 6) < (config_file + MAX_PATH_LENGTH - 1)) &&
                     ((*ptr == 'I') || (*ptr == 'i')) &&
                     ((*(ptr + 1) == 'L') || (*(ptr + 1) == 'l')) &&
                     ((*(ptr + 2) == 'O') || (*(ptr + 2) == 'o')) &&
                     ((*(ptr + 3) == 'C') || (*(ptr + 3) == 'c')) &&
                     ((*(ptr + 4) == 'K') || (*(ptr + 4) == 'k')) &&
                     ((*(ptr + 5) == 'E') || (*(ptr + 5) == 'e')) &&
                     ((*(ptr + 6) == 'D') || (*(ptr + 6) == 'd')) &&
                     ((*(ptr + 7) == ' ') || (*(ptr + 7) == '\t') ||
                      (*(ptr + 7) == ',') || (*(ptr + 7) == '\0')))
                 {
                    ptr += 7;
                    if ((*default_delete_files_flag & OLD_ILOCKED_FILES) == 0)
                    {
                       *default_delete_files_flag |= OLD_ILOCKED_FILES;
                    }
                 }
                 else
                 {
                    while ((*ptr != ' ') && (*ptr != '\t') &&
                           (*ptr != ',') && (*ptr != '\0') &&
                           ((ptr + 1) < (config_file + MAX_PATH_LENGTH - 1)))
                    {
                       ptr++;
                    }
                 }
         } while (*ptr != '\0');
      }
      if (get_definition(buffer, DEFAULT_NO_PARALLEL_JOBS_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         *default_no_parallel_jobs = atoi(value);
         if ((*default_no_parallel_jobs < 1) ||
             (*default_no_parallel_jobs > MAX_NO_PARALLEL_JOBS))
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("Incorrect value (%d) set in AFD_CONFIG for %s. Must be between 1 and %d. Setting to default %d."),
                       *default_no_parallel_jobs, DEFAULT_NO_PARALLEL_JOBS_DEF,
                       MAX_NO_PARALLEL_JOBS, DEFAULT_NO_PARALLEL_JOBS);
            *default_no_parallel_jobs = DEFAULT_NO_PARALLEL_JOBS;
         }
      }
      if (get_definition(buffer, DEFAULT_MAX_ERRORS_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         *default_max_errors = atoi(value);
         if (*default_max_errors < 1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("Incorrect value (%d) set in AFD_CONFIG for %s. Must be greater then 0. Setting to default %d."),
                       *default_max_errors, DEFAULT_MAX_ERRORS_DEF,
                       DEFAULT_MAX_ERRORS);
            *default_max_errors = DEFAULT_MAX_ERRORS;
         }
      }
      if (get_definition(buffer, DEFAULT_RETRY_INTERVAL_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         *default_retry_interval = atoi(value);
         if (*default_retry_interval < 1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("Incorrect value (%d) set in AFD_CONFIG for %s. Must be greater then 0. Setting to default %d."),
                       *default_retry_interval, DEFAULT_RETRY_INTERVAL_DEF,
                       DEFAULT_RETRY_INTERVAL);
            *default_retry_interval = DEFAULT_RETRY_INTERVAL;
         }
      }
      if (get_definition(buffer, DEFAULT_TRANSFER_BLOCKSIZE_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         *default_transfer_blocksize = atoi(value);
         if ((*default_transfer_blocksize < 1) &&
             ((*default_transfer_blocksize % 256) != 0))
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("Incorrect value (%d) set in AFD_CONFIG for %s. Must be greater then 0 and divisible by 256. Setting to default %d."),
                       *default_transfer_blocksize,
                       DEFAULT_TRANSFER_BLOCKSIZE_DEF,
                       DEFAULT_TRANSFER_BLOCKSIZE);
            *default_transfer_blocksize = DEFAULT_TRANSFER_BLOCKSIZE;
         }
      }
      if (get_definition(buffer, DEFAULT_SUCCESSFUL_RETRIES_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         *default_successful_retries = atoi(value);
         if (*default_successful_retries < 1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("Incorrect value (%d) set in AFD_CONFIG for %s. Must be greater then 0. Setting to default %d."),
                       *default_successful_retries, DEFAULT_SUCCESSFUL_RETRIES_DEF,
                       DEFAULT_SUCCESSFUL_RETRIES);
            *default_successful_retries = DEFAULT_SUCCESSFUL_RETRIES;
         }
      }
      if (get_definition(buffer, DEFAULT_TRANSFER_TIMEOUT_DEF,
                         value, MAX_LONG_LENGTH) != NULL)
      {
         *default_transfer_timeout = atol(value);
         if (*default_transfer_timeout < 0)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("A value less then 0 for AFD_CONFIG variable %s is not possible, setting default %ld"),
                       DEFAULT_TRANSFER_TIMEOUT_DEF, DEFAULT_TRANSFER_TIMEOUT);
            *default_transfer_timeout = DEFAULT_TRANSFER_TIMEOUT;
         }
      }
      if (get_definition(buffer, DEFAULT_ERROR_OFFLINE_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         if (((value[0] == 'n') || (value[0] == 'N')) &&
             ((value[1] == 'o') || (value[1] == 'O')) &&
             ((value[2] == '\0') || (value[2] == ' ') || (value[2] == '\t')))
         {
            *default_error_offline_flag = 0;
         }
         else if (((value[0] == 'y') || (value[0] == 'Y')) &&
                  ((value[1] == 'e') || (value[1] == 'E')) &&
                  ((value[2] == 's') || (value[2] == 'S')) &&
                  ((value[3] == '\0') || (value[3] == ' ') || (value[3] == '\t')))
              {
                 *default_error_offline_flag = HOST_ERROR_OFFLINE_STATIC;
              }
              else
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            _("Only YES or NO (and not `%s') are possible for %s in AFD_CONFIG. Setting to default: %s"),
                            value, DEFAULT_ERROR_OFFLINE_DEF,
                            (*default_error_offline_flag == YES) ? "YES" : "NO");
              }
      }
      ptr = buffer;
      no_of_dc_filters = 0;
      max_length = 0;
      while ((ptr = get_definition(ptr, DIR_CONFIG_NAME_DEF,
                                   config_file, MAX_PATH_LENGTH)) != NULL)
      {
         length = strlen(config_file) + 1;
         if (length > max_length)
         {
            max_length = length;
         }
         no_of_dc_filters++;
      }
      if ((no_of_dc_filters > 0) && (max_length > 0))
      {
         int i,
             ii;

         if ((dcfl = malloc(no_of_dc_filters * sizeof(struct dc_filter_list))) == NULL)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       _("Failed to malloc() %d bytes : %s"),
                       no_of_dc_filters * sizeof(struct dc_filter_list),
                       strerror(errno));
            exit(INCORRECT);
         }
         ptr = buffer;
         for (i = 0; i < no_of_dc_filters; i++)
         {
            ptr = get_definition(ptr, DIR_CONFIG_NAME_DEF, config_file,
                                 MAX_PATH_LENGTH);
            if (config_file[0] != '/')
            {
               if (config_file[0] == '~')
               {
                  char *p_path,
                       user[MAX_USER_NAME_LENGTH + 1];

                  if (config_file[1] == '/')
                  {
                     p_path = &config_file[2];
                     user[0] = '\0';
                  }
                  else
                  {
                     int j = 0;

                     p_path = &config_file[1];
                     while ((*(p_path + j) != '/') && (*(p_path + j) != '\0') &&
                            (j < MAX_USER_NAME_LENGTH))
                     {
                        user[j] = *(p_path + j);
                        j++;
                     }
                     if (j >= MAX_USER_NAME_LENGTH)
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   _("User name to long for %s definition %s. User name may be %d bytes long."),
                                   DIR_CONFIG_NAME_DEF, config_file,
                                   MAX_USER_NAME_LENGTH);
                     }
                     user[j] = '\0';
                  }
                  (void)expand_path(user, p_path);
                  dcfl[i].length = strlen(p_path) + 1;
                  (void)memmove(config_file, p_path, length);
               }
               else
               {
                  char tmp_config_file[MAX_PATH_LENGTH];

                  (void)strcpy(tmp_config_file, config_file);
                  dcfl[i].length = snprintf(config_file, MAX_PATH_LENGTH, "%s%s/%s",
                                            p_work_dir, ETC_DIR,
                                            tmp_config_file) + 1;
               }
            }
            else
            {
               dcfl[i].length = strlen(config_file) + 1;
            }
            if ((dcfl[i].dc_filter = malloc(dcfl[i].length)) == NULL)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          _("Failed to malloc() %d bytes : %s"),
                          dcfl[i].length, strerror(errno));
               exit(INCORRECT);
            }
            (void)memcpy(dcfl[i].dc_filter, config_file, dcfl[i].length);
            dcfl[i].is_filter = NO;
            for (ii = 0; ii < dcfl[i].length; ii++)
            {
               if ((config_file[ii] == '*') || (config_file[ii] == '?') ||
                   (config_file[ii] == '['))
               {
                  dcfl[i].is_filter = YES;
                  ii = dcfl[i].length;
               }
            }
         }
      }
      else
      {
         length = snprintf(config_file, MAX_PATH_LENGTH, "%s%s%s",
                           p_work_dir, ETC_DIR, DEFAULT_DIR_CONFIG_FILE) + 1;
         no_of_dc_filters = 1;
         if ((dcfl = malloc(sizeof(struct dc_filter_list))) == NULL)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       _("Failed to malloc() %d bytes : %s"),
                       sizeof(struct dc_filter_list), strerror(errno));
            exit(INCORRECT);
         }
         if ((dcfl[0].dc_filter = malloc(length)) == NULL)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       _("Failed to malloc() %d bytes : %s"),
                       length, strerror(errno));
            exit(INCORRECT);
         }
         dcfl[0].length = length;
         (void)strcpy(dcfl[0].dc_filter, config_file);
         dcfl[0].is_filter = NO;
      }
      if (get_definition(buffer, MAX_SHUTDOWN_TIME_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         *max_shutdown_time = atoi(value);
         if (*max_shutdown_time < 2)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "%s is to low (%d < 2), setting default %d.",
                       MAX_SHUTDOWN_TIME_DEF, *max_shutdown_time,
                       MAX_SHUTDOWN_TIME);
            *max_shutdown_time = MAX_SHUTDOWN_TIME;
         }
      }
      else
      {
         *max_shutdown_time = MAX_SHUTDOWN_TIME;
      }
      free(buffer);
   }
   else
   {
      size_t length;

      length = snprintf(config_file, MAX_PATH_LENGTH, "%s%s%s",
                        p_work_dir, ETC_DIR, DEFAULT_DIR_CONFIG_FILE) + 1;
      no_of_dc_filters = 1;
      if ((dcfl = malloc(sizeof(struct dc_filter_list))) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("Failed to malloc() %d bytes : %s"),
                    sizeof(struct dc_filter_list), strerror(errno));
         exit(INCORRECT);
      }
      if ((dcfl[0].dc_filter = malloc(length)) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("Failed to malloc() %d bytes : %s"),
                    length, strerror(errno));
         exit(INCORRECT);
      }
      dcfl[0].length = length;
      (void)strcpy(dcfl[0].dc_filter, config_file);
      dcfl[0].is_filter = NO;
   }

   return;
}


/*+++++++++++++++++++++++++++ notify_dir_check() ++++++++++++++++++++++++*/
static void
notify_dir_check(void)
{
   int  fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
   int  readfd;
#endif
   char fifo_name[MAX_PATH_LENGTH];

   (void)snprintf(fifo_name, MAX_PATH_LENGTH, "%s%s%s",
                  p_work_dir, FIFO_DIR, IP_FIN_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(fifo_name, &readfd, &fd) == -1)
#else
   if ((fd = open(fifo_name, O_RDWR)) == -1)
#endif
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 _("Failed to open() `%s' : %s"), fifo_name, strerror(errno));
   }
   else
   {
      pid_t pid = -1;

      if (write(fd, &pid, sizeof(pid_t)) != sizeof(pid_t))
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Failed to write() to `%s' : %s"),
                    fifo_name, strerror(errno));
      }
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (close(readfd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("close() error : %s"), strerror(errno));
      }
#endif
      if (close(fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("close() error : %s"), strerror(errno));
      }
   }

   return;
}


/*++++++++++++++++++++++++++++++ amg_exit() +++++++++++++++++++++++++++++*/
static void
amg_exit(void)
{
   system_log(INFO_SIGN, NULL, 0, "Stopped %s.", AMG);

   /* Kill all jobs that where started. */
   if (dc_pid > 0)
   {
      if (kill(dc_pid, SIGINT) < 0)
      {
         if (errno != ESRCH)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                       _("Failed to kill process %s with pid %d : %s"),
#else
                       _("Failed to kill process %s with pid %lld : %s"),
#endif
                       DC_PROC_NAME, (pri_pid_t)dc_pid, strerror(errno));
         }
      }
   }

   if (pid_list != NULL)
   {
#ifdef HAVE_MMAP
      (void)munmap((void *)pid_list, afd_active_size);
#else
      (void)munmap_emu((void *)pid_list);
#endif
   }

   if (stop_flag == 0)
   {
      char counter_file[MAX_PATH_LENGTH];

      (void)snprintf(counter_file, MAX_PATH_LENGTH, "%s%s%s",
                     p_work_dir, FIFO_DIR, COUNTER_FILE);
      (void)unlink(counter_file);
   }

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   system_log(FATAL_SIGN, __FILE__, __LINE__,
              _("Aaarrrggh! Received SIGSEGV."));
   amg_exit();
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   system_log(FATAL_SIGN, __FILE__, __LINE__, _("Uuurrrggh! Received SIGBUS."));
   amg_exit();
   abort();
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   (void)fprintf(stderr,
#if SIZEOF_PID_T == 4
                 "%s terminated by signal %d (%d)\n",
#else
                 "%s terminated by signal %d (%lld)\n",
#endif
                 AMG, signo, (pri_pid_t)getpid());

   exit(INCORRECT);
}
