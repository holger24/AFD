/*
 *  init_dir_check.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2023 Deutscher Wetterdienst (DWD),
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
 **   init_dir_check - initialize variables and fifos for process
 **                    dir_check
 **
 ** SYNOPSIS
 **   void init_dir_check(int    argc,
 **                       char   *argv[],
 **                       time_t *rescan_time,
 **                       int    *ievent_buf_length,
 **                       int    *read_fd,
 **                       int    *write_fd,
 **                       int    *del_time_job_fd)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   On succcess it returns the rescan_time and the file descriptors
 **   read_fd, write_fd and fin_fd.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   06.04.1999 H.Kiehl Created
 **   12.01.2000 H.Kiehl Added receive log.
 **   16.05.2002 H.Kiehl Removed shared memory stuff.
 **   01.02.2010 H.Kiehl Option to set system wide force reread interval.
 **
 */
DESCR__E_M1

#include <stdio.h>                 /* fprintf(), sprintf()               */
#ifdef WITH_INOTIFY
# include <stdint.h>
# include <ctype.h>                /* isdigit()                          */
#endif
#include <string.h>                /* strcpy(), strcat(), strerror()     */
#include <stdlib.h>                /* atoi(), calloc(), malloc(), free(),*/
                                   /* exit()                             */
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>                /* kill()                             */
#ifdef HAVE_SETPRIORITY
# include <sys/time.h>
# include <sys/resource.h>         /* getpriority()                      */
#endif
#ifdef WITH_INOTIFY
# include <sys/inotify.h>          /* inotify_init()                     */
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <unistd.h>                /* geteuid(), getegid()               */
#include <errno.h>
#ifdef _WITH_PTHREAD
# include <pthread.h>
#endif
#include "amgdefs.h"


/* Global variables. */
extern int                        afd_file_dir_length,
                                  afd_status_fd,
                                  alfbl,    /* Additional locked file buffer length */
                                  alfc,     /* Additional locked file counter       */
                                  dcpl_fd,
#ifdef HAVE_SETPRIORITY
                                  add_afd_priority,
                                  current_priority,
                                  exec_base_priority,
                                  max_sched_priority,
                                  min_sched_priority,
#endif
                                  max_process,
#ifdef MULTI_FS_SUPPORT
                                  no_of_extra_work_dirs,
#else
                                  outgoing_file_dir_length,
#endif
                                  *no_of_process,
#ifndef _WITH_PTHREAD
                                  dir_check_timeout,
#endif
                                  full_scan_timeout,
#ifdef WITH_INOTIFY
                                  inotify_fd,
                                  no_of_inotify_dirs,
#endif
                                  one_dir_copy_timeout,
                                  no_of_jobs,
                                  no_of_local_dirs, /* No. of directories in*/
                                                    /* the DIR_CONFIG file  */
                                                    /* that are local.      */
                                  no_of_orphaned_procs,
                                  *amg_counter,
                                  amg_counter_fd,   /* File descriptor for  */
                                                    /* AMG counter file.    */
                                  fin_fd,
#ifdef _INPUT_LOG
                                  il_fd,
#endif
#ifdef WITHOUT_FIFO_RW_SUPPORT
                                  dc_cmd_writefd,
                                  dc_resp_readfd,
                                  del_time_job_writefd,
                                  fin_writefd,
                                  receive_log_readfd,
#endif
                                  receive_log_fd;
extern mode_t                     default_create_source_dir_mode;
extern time_t                     default_exec_timeout;
extern unsigned int               default_age_limit,
                                  force_reread_interval;
extern pid_t                      *opl;
#ifdef _WITH_PTHREAD
extern pthread_t                  *thread;
#else
extern unsigned int               max_file_buffer;
extern time_t                     *file_mtime_pool;
extern off_t                      *file_size_pool;
#endif
#ifdef _POSIX_SAVED_IDS
extern int                        no_of_sgids;
extern uid_t                      afd_uid;
extern gid_t                      afd_gid,
                                  *afd_sgids;
#endif
extern char                       *afd_file_dir,
                                  *alfiles, /* Additional locked files */
                                  *bul_file,
#ifndef _WITH_PTHREAD
                                  **file_name_pool,
#endif
#ifndef MULTI_FS_SUPPORT
                                  outgoing_file_dir[],
                                  *p_time_dir_id,
                                  time_dir[],
#endif
                                  *p_work_dir,
                                  *rep_file;
#ifndef _WITH_PTHREAD
extern unsigned char              *file_length_pool;
#endif
#ifdef MULTI_FS_SUPPORT
extern struct extra_work_dirs     *ewl;
#endif
extern struct dc_proc_list        *dcpl;      /* Dir Check Process List. */
extern struct directory_entry     *de;
extern struct afd_status          *p_afd_status;
extern struct instant_db          *db;
extern struct fileretrieve_status *fra;
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif
#ifdef _DISTRIBUTION_LOG
extern struct file_dist_list      **file_dist_pool;
#endif
#ifdef _WITH_PTHREAD
extern struct data_t              *p_data;
#endif
#ifdef WITH_INOTIFY
extern struct inotify_watch_list  *iwl;
#endif

/* Local function prototypes. */
static void                       get_afd_config_value(void),
#ifdef WITH_INOTIFY
                                  get_max_queued_events(int *),
#endif
                                  usage(void);


/*########################### init_dir_check() ##########################*/
void
init_dir_check(int    argc,
               char   *argv[],
               time_t *rescan_time,
#ifdef WITH_ONETIME
               int    *ot_job_fd,
# ifdef WITHOUT_FIFO_RW_SUPPORT
               int    *ot_job_readfd,
# endif
#endif
#ifdef WITH_INOTIFY
               int    *ievent_buf_length,
#endif
               int    *read_fd,
               int    *write_fd,
               int    *del_time_job_fd)
{
   int         i;
#ifdef _WITH_PTHREAD
# ifdef _DISTRIBUTION_LOG
   int         j;
# endif
#endif
   size_t      size;
   pid_t       udc_pid;
   char        del_time_job_fifo[MAX_PATH_LENGTH],
#ifdef _INPUT_LOG
               input_log_fifo[MAX_PATH_LENGTH],
#endif
               dc_cmd_fifo[MAX_PATH_LENGTH],
               dcpl_data_file[MAX_PATH_LENGTH],
               dc_resp_fifo[MAX_PATH_LENGTH],
               fin_fifo[MAX_PATH_LENGTH],
#ifdef WITH_ONETIME
               ot_job_fifo[MAX_PATH_LENGTH],
#endif
               other_fifo[MAX_PATH_LENGTH],
               *p_other_fifo,
               *ptr,
               receive_log_fifo[MAX_PATH_LENGTH],
               udc_reply_name[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat stat_buf;
#endif
   FILE        *udc_reply_fp = NULL;

   /* Get call-up parameters. */
   if (argc != 7)
   {
      usage();
   }
   else
   {
      (void)my_strncpy(p_work_dir, argv[1], MAX_PATH_LENGTH);
      *rescan_time = atoi(argv[2]);
      max_process = atoi(argv[3]);
      no_of_local_dirs = atoi(argv[4]);
      default_create_source_dir_mode = (mode_t)strtoul(argv[5], (char **)NULL, 10);
#if SIZEOF_PID_T == 4
      udc_pid = atoi(argv[6]);
#else
# ifdef HAVE_STRTOLL
      udc_pid = (pid_t)strtoll(argv[6], (char **)NULL, 10);
# else
      udc_pid = (pid_t)strtoul(argv[6], (char **)NULL, 10);
# endif
#endif
   }

#ifdef _POSIX_SAVED_IDS
   /* User and group ID. */
   afd_uid = geteuid();
   afd_gid = getegid();
   no_of_sgids = getgroups(0, NULL);
   if (no_of_sgids > 0)
   {
      if ((afd_sgids = malloc((no_of_sgids * sizeof(gid_t)))) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to malloc() %d bytes : %s",
                    (no_of_sgids * sizeof(gid_t)), strerror(errno));
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Unable to check supplementary groups!");
         no_of_sgids = 0;
      }
      else
      {
         if (getgroups(no_of_sgids, afd_sgids) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "getgroups() error : %s", strerror(errno));
            no_of_sgids = 0;
         }
      }
   }
   else
   {
      no_of_sgids = 0;
      afd_sgids = NULL;
   }
#endif

#ifdef HAVE_SETPRIORITY
   errno = 0;
   if (((current_priority = getpriority(PRIO_PROCESS, 0)) == -1) &&
       (errno != 0))
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to getpriority() : %s", strerror(errno));
      current_priority = 0;
   }
#endif

   /* Allocate memory for the array containing all file names to  */
   /* be send for every directory section in the DIR_CONFIG file. */
#ifdef WITH_ONETIME
   if ((de = malloc((no_of_local_dirs + MAX_NO_OF_ONETIME_DIRS) * sizeof(struct directory_entry))) == NULL)
#else
   if ((de = malloc(no_of_local_dirs * sizeof(struct directory_entry))) == NULL)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error [%d bytes] : %s",
                 no_of_local_dirs * sizeof(struct directory_entry),
                 strerror(errno));
      exit(INCORRECT);
   }
   afd_file_dir_length = strlen(p_work_dir) + AFD_FILE_DIR_LENGTH;
   if ((afd_file_dir = malloc(afd_file_dir_length + 1)) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Initialise variables. */
   (void)strcpy(afd_file_dir, p_work_dir);
   (void)strcat(afd_file_dir, AFD_FILE_DIR);
#ifndef MULTI_FS_SUPPORT
   (void)strcpy(outgoing_file_dir, afd_file_dir);
   (void)strcpy(outgoing_file_dir + afd_file_dir_length, OUTGOING_DIR);
   outgoing_file_dir_length = afd_file_dir_length + OUTGOING_DIR_LENGTH;
   (void)strcpy(time_dir, afd_file_dir);
   (void)strcpy(time_dir + afd_file_dir_length, AFD_TIME_DIR);
   time_dir[afd_file_dir_length + AFD_TIME_DIR_LENGTH] = '/';
   time_dir[afd_file_dir_length + AFD_TIME_DIR_LENGTH + 1] = '\0';
   p_time_dir_id = time_dir + afd_file_dir_length + AFD_TIME_DIR_LENGTH + 1;
#endif
   (void)strcpy(dc_cmd_fifo, p_work_dir);
   (void)strcat(dc_cmd_fifo, FIFO_DIR);
   (void)strcpy(other_fifo, dc_cmd_fifo);
   p_other_fifo = other_fifo + strlen(other_fifo);
   (void)strcpy(fin_fifo, dc_cmd_fifo);
   (void)strcat(fin_fifo, IP_FIN_FIFO);
#ifdef _INPUT_LOG
   (void)strcpy(input_log_fifo, dc_cmd_fifo);
   (void)strcat(input_log_fifo, INPUT_LOG_FIFO);
#endif
   (void)strcpy(dc_resp_fifo, dc_cmd_fifo);
   (void)strcat(dc_resp_fifo, DC_RESP_FIFO);
   (void)strcpy(del_time_job_fifo, dc_cmd_fifo);
   (void)strcat(del_time_job_fifo, DEL_TIME_JOB_FIFO);
   (void)strcpy(receive_log_fifo, dc_cmd_fifo);
   (void)strcat(receive_log_fifo, RECEIVE_LOG_FIFO);
   (void)strcpy(dcpl_data_file, dc_cmd_fifo);
   (void)strcat(dcpl_data_file, DCPL_FILE_NAME);
#ifdef WITH_ONETIME
   (void)strcpy(ot_job_fifo, dc_cmd_fifo);
   (void)strcat(ot_job_fifo, OT_JOB_FIFO);
#endif
   (void)strcat(dc_cmd_fifo, DC_CMD_FIFO);
   init_msg_buffer();

#ifdef _DELETE_LOG
   delete_log_ptrs(&dl);
#endif

   /*
    * We need to attach to AFD status area to see if the FD is
    * up running. If not we will very quickly fill up the
    * message fifo to the FD.
    */
   if (attach_afd_status(&afd_status_fd, WAIT_AFD_STATUS_ATTACH) < 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to attach to AFD status area.");
      exit(INCORRECT);
   }
   p_afd_status->amg_jobs &= ~CHECK_FILE_DIR_ACTIVE;

   get_afd_config_value();

#ifdef _WITH_PTHREAD
   if ((thread = malloc(no_of_local_dirs * sizeof(pthread_t))) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      exit(INCORRECT);
   }
   if ((p_data = malloc(no_of_local_dirs * sizeof(struct data_t))) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      exit(INCORRECT);
   }
   for (i = 0; i < no_of_local_dirs; i++)
   {
      p_data[i].i = i;
      de[i].fme   = NULL;
      de[i].rl_fd = -1;
      de[i].rl    = NULL;
   }
#else
# ifdef WITH_ONETIME
   for (i = 0; i < (no_of_local_dirs + MAX_NO_OF_ONETIME_DIRS); i++)
# else
   for (i = 0; i < no_of_local_dirs; i++)
# endif
   {
      de[i].fme   = NULL;
      de[i].rl_fd = -1;
      de[i].rl    = NULL;
   }
   RT_ARRAY(file_name_pool, max_file_buffer, MAX_FILENAME_LENGTH, char);
   if ((file_length_pool = malloc(max_file_buffer * sizeof(unsigned char))) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error [%d bytes] : %s",
                 max_file_buffer * sizeof(unsigned char), strerror(errno));
      exit(INCORRECT);
   }
   if ((file_mtime_pool = malloc(max_file_buffer * sizeof(off_t))) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error [%d bytes] : %s",
                 max_file_buffer * sizeof(off_t), strerror(errno));
      exit(INCORRECT);
   }
   if ((file_size_pool = malloc(max_file_buffer * sizeof(off_t))) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error [%d bytes] : %s",
                 max_file_buffer * sizeof(off_t), strerror(errno));
      exit(INCORRECT);
   }
# ifdef _DISTRIBUTION_LOG
#  ifdef RT_ARRAY_STRUCT_WORKING
   RT_ARRAY(file_dist_pool, max_file_buffer, NO_OF_DISTRIBUTION_TYPES,
            (struct file_dist_list));
#  else
   if ((file_dist_pool = (struct file_dist_list **)malloc(max_file_buffer * sizeof(struct file_dist_list *))) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error [%d bytes] : %s",
                 max_file_buffer * sizeof(struct file_dist_list *),
                 strerror(errno));
      exit(INCORRECT);
   }
   if ((file_dist_pool[0] = (struct file_dist_list *)malloc(max_file_buffer * NO_OF_DISTRIBUTION_TYPES * sizeof(struct file_dist_list))) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error [%d bytes] : %s",
                 max_file_buffer * NO_OF_DISTRIBUTION_TYPES * sizeof(struct file_dist_list),
                 strerror(errno));
      exit(INCORRECT);
   }
   for (i = 1; i < max_file_buffer; i++)
   {
      file_dist_pool[i] = file_dist_pool[0] + (i * NO_OF_DISTRIBUTION_TYPES);
   }
#  endif
# endif
#endif

   /* Check if we want to write some information for udc. */
   if (udc_pid > 0)
   {
      (void)snprintf(udc_reply_name, MAX_PATH_LENGTH,
#if SIZEOF_PID_T == 4
                     "%s%s%s.%d",
#else
                     "%s%s%s.%lld",
#endif
                     p_work_dir, FIFO_DIR, DB_UPDATE_REPLY_DEBUG_FILE,
                     (pri_pid_t)udc_pid);

      /* Ensure file will be created as 644. */
      (void)umask(S_IWGRP | S_IWOTH);
      if ((udc_reply_fp = fopen(udc_reply_name, "a")) == NULL)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Failed to fopen() `%s' : %s"), 
                    udc_reply_name, strerror(errno));
      }
      (void)umask(0);
   }

   /* Open receive log fifo. */
#ifdef HAVE_STATX
   if ((statx(0, receive_log_fifo, AT_STATX_SYNC_AS_STAT,
              STATX_MODE, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.stx_mode)))
#else
   if ((stat(receive_log_fifo, &stat_buf) == -1) ||
       (!S_ISFIFO(stat_buf.st_mode)))
#endif
   {
      if (make_fifo(receive_log_fifo) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to create fifo %s.", receive_log_fifo);
         exit(INCORRECT);
      }
   }
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(receive_log_fifo, &receive_log_readfd, &receive_log_fd) == -1)
#else
   if ((receive_log_fd = coe_open(receive_log_fifo, O_RDWR)) == -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s",
                 receive_log_fifo, strerror(errno));
      exit(INCORRECT);
   }

#ifdef WITH_ONETIME
   /* Open onetime command fifo. */
# ifdef HAVE_STATX
   if ((statx(0, ot_job_fifo, AT_STATX_SYNC_AS_STAT,
              STATX_MODE, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
# else
   if ((stat(ot_job_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
# endif
   {
      if (make_fifo(ot_job_fifo) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to create fifo %s.", ot_job_fifo);
         exit(INCORRECT);
      }
   }
# ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(ot_job_fifo, &ot_job_readfd, &ot_job_fd) == -1)
# else
   if ((ot_job_fd = coe_open(ot_job_fifo, O_RDWR)) == -1)
# endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s",
                 ot_job_fifo, strerror(errno));
      exit(INCORRECT);
   }
#endif

   /* Check if the queue list fifos exist, if not create them. */
   (void)strcpy(p_other_fifo, QUEUE_LIST_READY_FIFO);
#ifdef HAVE_STATX
   if ((statx(0, other_fifo, AT_STATX_SYNC_AS_STAT,
              STATX_MODE, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.stx_mode)))
#else
   if ((stat(other_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
#endif
   {
      if (make_fifo(other_fifo) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to create fifo %s.", other_fifo);
         exit(INCORRECT);
      }
   }
   (void)strcpy(p_other_fifo, QUEUE_LIST_DONE_FIFO);
#ifdef HAVE_STATX
   if ((statx(0, other_fifo, AT_STATX_SYNC_AS_STAT,
              STATX_MODE, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.stx_mode)))
#else
   if ((stat(other_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
#endif
   {
      if (make_fifo(other_fifo) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to create fifo %s.", other_fifo);
         exit(INCORRECT);
      }
   }

   /* Open counter file so we can create unique names for each job. */
   if ((amg_counter_fd = open_counter_file(AMG_COUNTER_FILE, &amg_counter)) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open counter file : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Get the fra_id and no of directories of the FRA. */
   if (fra_attach() != SUCCESS)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__, "Failed to attach to FRA.");
      exit(INCORRECT);
   }

   /* Get the fsa_id and no of host of the FSA. */
   if (fsa_attach(DIR_CHECK) != SUCCESS)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__, "Failed to attach to FSA.");
      exit(INCORRECT);
   }

   /* Open fifos to communicate with AMG. */
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(dc_resp_fifo, &dc_resp_readfd, write_fd) == -1)
#else
   if ((*write_fd = coe_open(dc_resp_fifo, O_RDWR)) == -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s", dc_resp_fifo, strerror(errno));
      exit(INCORRECT);
   }

   /* Open fifo to wait for answer from job. */
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(dc_cmd_fifo, read_fd, &dc_cmd_writefd) == -1)
#else
   if ((*read_fd = coe_open(dc_cmd_fifo, O_RDWR | O_NONBLOCK)) == -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s", dc_cmd_fifo, strerror(errno));
      exit(INCORRECT);
   }
#ifdef WITHOUT_FIFO_RW_SUPPORT
   set_fl(*read_fd, O_NONBLOCK);
#endif

   /*
    * Create and open fifo for process copying/moving files.
    * The child will tell the parent when it is finished via
    * this fifo.
    */
#ifdef HAVE_STATX
   if ((statx(0, fin_fifo, AT_STATX_SYNC_AS_STAT,
              STATX_MODE, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.stx_mode)))
#else
   if ((stat(fin_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
#endif
   {
      if (make_fifo(fin_fifo) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not create fifo %s.", fin_fifo);
         exit(INCORRECT);
      }
   }
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(fin_fifo, &fin_fd, &fin_writefd) == -1)
#else
   if ((fin_fd = coe_open(fin_fifo, O_RDWR)) == -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s", fin_fifo, strerror(errno));
      exit(INCORRECT);
   }
#ifdef HAVE_STATX
   if ((statx(0, del_time_job_fifo, AT_STATX_SYNC_AS_STAT,
              STATX_MODE, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.stx_mode)))
#else
   if ((stat(del_time_job_fifo, &stat_buf) == -1) ||
       (!S_ISFIFO(stat_buf.st_mode)))
#endif
   {
      if (make_fifo(del_time_job_fifo) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not create fifo %s.", del_time_job_fifo);
         exit(INCORRECT);
      }
   }
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(del_time_job_fifo, del_time_job_fd, &del_time_job_writefd) == -1)
#else
   if ((*del_time_job_fd = coe_open(del_time_job_fifo, O_RDWR)) == -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s",
                 del_time_job_fifo, strerror(errno));
      exit(INCORRECT);
   }

   /* Now create the internal database of this process. */
   no_of_jobs = create_db(udc_reply_fp, *write_fd);

   if (udc_reply_fp != NULL)
   {
      (void)fflush(udc_reply_fp);
      if (fclose(udc_reply_fp) == EOF)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Failed to fclose() `%s' : %s",
                    udc_reply_name, strerror(errno));
      }
   }

#ifdef _WITH_PTHREAD
   for (i = 0; i < no_of_local_dirs; i++)
   {
      RT_ARRAY(p_data[i].file_name_pool, fra[i].max_copied_files,
               MAX_FILENAME_LENGTH, char);
      if ((p_data[i].file_length_pool = malloc(fra[i].max_copied_files * sizeof(unsigned char))) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "malloc() error [%d bytes] : %s",
                    fra[i].max_copied_files * sizeof(unsigned char),
                    strerror(errno));
         exit(INCORRECT);
      }
      if ((p_data[i].file_mtime_pool = malloc(fra[i].max_copied_files * sizeof(off_t))) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "malloc() error : %s", strerror(errno));
         exit(INCORRECT);
      }
      if ((p_data[i].file_size_pool = malloc(fra[i].max_copied_files * sizeof(off_t))) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "malloc() error : %s", strerror(errno));
         exit(INCORRECT);
      }
# ifdef _DISTRIBUTION_LOG
#  ifdef RT_ARRAY_STRUCT_WORKING
      RT_ARRAY(p_data[i].file_dist_pool, fra[i].max_copied_files,
               NO_OF_DISTRIBUTION_TYPES, (struct file_dist_list));
#  else
      if ((p_data[i].file_dist_pool = (struct file_dist_list **)malloc(fra[i].max_copied_files * sizeof(struct file_dist_list *))) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "malloc() error [%d bytes] : %s",
                    fra[i].max_copied_files * sizeof(struct file_dist_list *),
                    strerror(errno));
         exit(INCORRECT);
      }
      if ((p_data[i].file_dist_pool[0] = (struct file_dist_list *)malloc(fra[i].max_copied_files * NO_OF_DISTRIBUTION_TYPES * sizeof(struct file_dist_list))) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "malloc() error [%d bytes] : %s",
                    fra[i].max_copied_files * NO_OF_DISTRIBUTION_TYPES * sizeof(struct file_dist_list),
                    strerror(errno));
         exit(INCORRECT);
      }
      for (j = 1; j < fra[i].max_copied_files; j++)
      {
         p_data[i].file_dist_pool[j] = p_data[i].file_dist_pool[0] + (j * NO_OF_DISTRIBUTION_TYPES);
      }
#  endif
# endif
   }
#endif

   /* Attach to process ID array. */
   size = (max_process * sizeof(struct dc_proc_list)) + AFD_WORD_OFFSET;
   if ((ptr = attach_buf(dcpl_data_file, &dcpl_fd, &size,
                         DIR_CHECK, FILE_MODE, NO)) == (caddr_t) -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to mmap() `%s' : %s", dcpl_data_file, strerror(errno));
      exit(INCORRECT);
   }
   no_of_process = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   dcpl = (struct dc_proc_list *)ptr;

   /* Initialize, but don't overwrite existing process! */
   for (i = *no_of_process; i < max_process; i++)
   {
      dcpl[i].fra_pos = -1;
      dcpl[i].pid = -1;
   }

   /* Check if the process in the list still exist. */
   no_of_orphaned_procs = 0;
   opl = NULL;
   if (*no_of_process > 0)
   {
      for (i = 0; i < *no_of_process; i++)
      {
         if (dcpl[i].pid > 0)
         {
            if (kill(dcpl[i].pid, 0) == -1)
            {
               /* The process no longer exist, so lets remove it. */
               (*no_of_process)--;
               if (i < *no_of_process)
               {
                  (void)memmove(&dcpl[i], &dcpl[i + 1],
                                ((*no_of_process - i) * sizeof(struct dc_proc_list)));
               }
               dcpl[*no_of_process].pid = -1;
               dcpl[*no_of_process].fra_pos = -1;
               i--;
            }
            else
            {
               /*
                * The process still exists. We now need to check if
                * the job still exist and if fsa_pos is still correct.
                */
#ifdef WITH_ONETIME
               if (dcpl[i].job_id == ONETIME_JOB_ID)
               {
                  dcpl[i].fra_pos = 
                  if ((no_of_orphaned_procs % ORPHANED_PROC_STEP_SIZE) == 0)
                  {
                     size = ((no_of_orphaned_procs / ORPHANED_PROC_STEP_SIZE) + 1) *
                            ORPHANED_PROC_STEP_SIZE * sizeof(pid_t);

                     if ((opl = realloc(opl, size)) == NULL)
                     {
                        system_log(FATAL_SIGN, __FILE__, __LINE__,
                                   "Failed to realloc() %d bytes : %s",
                                   size, strerror(errno));
                        exit(INCORRECT);
                     }
                  }
                  fra[dcpl[i].fra_pos].no_of_process++;
                  opl[no_of_orphaned_procs] = dcpl[i].pid;
                  no_of_orphaned_procs++;
               }
               else
               {
#endif
                  int gotcha,
                      j;

                  gotcha = NO;
                  for (j = 0; j < no_of_jobs; j++)
                  {
                     if (db[j].job_id == dcpl[i].job_id)
                     {
                        if (db[j].fra_pos != dcpl[i].fra_pos)
                        {
                           dcpl[i].fra_pos = db[j].fra_pos;
                        }
                        if ((no_of_orphaned_procs % ORPHANED_PROC_STEP_SIZE) == 0)
                        {
                           size = ((no_of_orphaned_procs / ORPHANED_PROC_STEP_SIZE) + 1) *
                                  ORPHANED_PROC_STEP_SIZE * sizeof(pid_t);

                           if ((opl = realloc(opl, size)) == NULL)
                           {
                              system_log(FATAL_SIGN, __FILE__, __LINE__,
                                         "Failed to realloc() %d bytes : %s",
                                         size, strerror(errno));
                              exit(INCORRECT);
                           }
                        }
                        fra[db[j].fra_pos].no_of_process++;
                        opl[no_of_orphaned_procs] = dcpl[i].pid;
                        no_of_orphaned_procs++;
                        gotcha = YES;
                        break;
                     }
                  }
                  if (gotcha == NO)
                  {
                     /*
                      * The process is no longer in the current job list.
                      * We may not kill this process, since we do NOT
                      * know if this is one of our process that we
                      * started. But lets remove it from our job list.
                      */
                     (*no_of_process)--;
                     if (i < *no_of_process)
                     {
                        (void)memmove(&dcpl[i], &dcpl[i + 1],
                                      ((*no_of_process - i) * sizeof(struct dc_proc_list)));
                     }
                     dcpl[*no_of_process].pid = -1;
                     dcpl[*no_of_process].fra_pos = -1;
                     i--;
                  }
#ifdef WITH_ONETIME
               }
#endif
            }
         }
         else
         {
            /* Hmm, looks like garbage. Lets remove this. */
            (*no_of_process)--;
            if (i < *no_of_process)
            {
               (void)memmove(&dcpl[i], &dcpl[i + 1],
                             ((*no_of_process - i) * sizeof(struct dc_proc_list)));
            }
            dcpl[*no_of_process].pid = -1;
            dcpl[*no_of_process].fra_pos = -1;
            i--;
         }
      }
   }
   if (no_of_orphaned_procs)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Hmm, there are %d orphaned process.", no_of_orphaned_procs);
   }

#ifdef _INPUT_LOG
# ifdef HAVE_STATX
   if ((statx(0, input_log_fifo, AT_STATX_SYNC_AS_STAT,
              STATX_MODE, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.stx_mode)))
# else
   if ((stat(input_log_fifo, &stat_buf) == -1) ||
       (!S_ISFIFO(stat_buf.st_mode)))
# endif
   {
      if (make_fifo(input_log_fifo) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                   "Could not create fifo %s.", input_log_fifo);
         exit(INCORRECT);
      }
   }
   if ((il_fd = coe_open(input_log_fifo, O_RDWR)) < 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                "Could not open fifo %s : %s",
                input_log_fifo, strerror(errno));
      exit(INCORRECT);
   }
#endif

   get_rename_rules(YES);

#ifdef WITH_ERROR_QUEUE
   if (attach_error_queue() == INCORRECT)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to attach to the error queue!");
   }
#endif

   remove_old_ls_data_files();

#ifdef WITH_INOTIFY
   get_max_queued_events(ievent_buf_length);
   no_of_inotify_dirs = 0;
   if (*ievent_buf_length > 0)
   {
      for (i = 0; i < no_of_local_dirs; i++)
      {
         if (((fra[de[i].fra_pos].dir_options & INOTIFY_RENAME) ||
              (fra[de[i].fra_pos].dir_options & INOTIFY_CLOSE) ||
              (fra[de[i].fra_pos].dir_options & INOTIFY_CREATE) ||
              (fra[de[i].fra_pos].dir_options & INOTIFY_DELETE) ||
              (fra[de[i].fra_pos].dir_options & INOTIFY_ATTRIB)) &&
             ((fra[de[i].fra_pos].no_of_time_entries == 0) ||
              (fra[de[i].fra_pos].host_alias[0] != '\0')) &&
             ((fra[de[i].fra_pos].force_reread == NO) ||
              (fra[de[i].fra_pos].force_reread == REMOTE_ONLY)))
         {
            no_of_inotify_dirs++;

            /* At start always scan directory! */
            fra[de[i].fra_pos].dir_flag |= INOTIFY_NEEDS_SCAN;
         }
      }
      if (no_of_inotify_dirs > 0)
      {
         int      j;
         uint32_t mask;

         if ((iwl = malloc(no_of_inotify_dirs * sizeof(struct inotify_watch_list))) == NULL)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Failed to malloc() %d bytes : %s",
                       (no_of_inotify_dirs * sizeof(int)), strerror(errno));
            exit(INCORRECT);
         }
         if ((inotify_fd = inotify_init()) == -1)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Failed to inotify_init() : %s", strerror(errno));
            exit(INCORRECT);
         }
         j = 0;
         for (i = 0; i < no_of_local_dirs; i++)
         {
            if (((fra[de[i].fra_pos].dir_options & INOTIFY_RENAME) ||
                 (fra[de[i].fra_pos].dir_options & INOTIFY_CLOSE) ||
                 (fra[de[i].fra_pos].dir_options & INOTIFY_CREATE) ||
                 (fra[de[i].fra_pos].dir_options & INOTIFY_DELETE) ||
                 (fra[de[i].fra_pos].dir_options & INOTIFY_ATTRIB)) &&
                ((fra[de[i].fra_pos].no_of_time_entries == 0) ||
                 (fra[de[i].fra_pos].host_alias[0] != '\0')) &&
                ((fra[de[i].fra_pos].force_reread == NO) ||
                 (fra[de[i].fra_pos].force_reread == REMOTE_ONLY)))
            {
               mask = 0;
               if (fra[de[i].fra_pos].dir_options & INOTIFY_RENAME)
               {
                  mask |= IN_MOVED_TO;
               }
               if (fra[de[i].fra_pos].dir_options & INOTIFY_CLOSE)
               {
                  mask |= IN_CLOSE_WRITE;
               }
               if (fra[de[i].fra_pos].dir_options & INOTIFY_CREATE)
               {
                  mask |= IN_CREATE;
               }
               if (fra[de[i].fra_pos].dir_options & INOTIFY_DELETE)
               {
                  mask |= IN_DELETE;
               }
               if (fra[de[i].fra_pos].dir_options & INOTIFY_ATTRIB)
               {
                  mask |= IN_ATTRIB;
               }

               if ((iwl[j].wd = inotify_add_watch(inotify_fd, de[i].dir, mask)) == -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "inotify_add_watch() error for `%s' : %s",
                             de[i].dir, strerror(errno));
               }
               else
               {
                  iwl[j].de_pos = i;
                  iwl[j].no_of_files = 0;
                  iwl[j].cur_fn_length = 0;
                  iwl[j].alloc_fn_length = 0;
                  iwl[j].file_name = NULL;
                  iwl[j].fnl = NULL;
                  j++;
               }
               if (j > no_of_inotify_dirs)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "How can this be? This is an error of the programmer!");
                  break;
               }
            }
         }
         no_of_inotify_dirs = j;
      }
      else
      {
         inotify_fd = -1;
      }
   }
   else
   {
      inotify_fd = -1;
   }
#endif

   return;
}


#ifdef WITH_INOTIFY
# define INOTIFY_MAX_QUEUED_EVENTS "/proc/sys/fs/inotify/max_queued_events"

/*++++++++++++++++++++++++ get_max_queued_events() ++++++++++++++++++++++*/
static void
get_max_queued_events(int *ievent_buf_length)
{
   int fd;

   if ((fd = open(INOTIFY_MAX_QUEUED_EVENTS, O_RDONLY)) == -1)
   {
      if (errno != ENOENT)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to open() %s : %s",
                    INOTIFY_MAX_QUEUED_EVENTS, strerror(errno));
      }
      *ievent_buf_length = 0; /* No inotify support! */
   }
   else
   {
      int  ret;
      char buffer[MAX_INT_LENGTH + 1];

      if ((ret = read(fd, buffer, MAX_INT_LENGTH)) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to read() from %s : %s",
                    INOTIFY_MAX_QUEUED_EVENTS, strerror(errno));
         *ievent_buf_length = 0; /* No inotify support! */
      }
      else
      {
         int  i = 0;
         char *ptr,
              str_number[MAX_INT_LENGTH + 1];

         buffer[ret] = '\0';
         ptr = buffer;
         while ((*ptr == ' ') || (*ptr == '\t'))
         {
            ptr++;
         }
         while ((isdigit(*ptr)) && (i < MAX_INT_LENGTH) && (*ptr != '\0'))
         {
            str_number[i] = *ptr;
            i++; ptr++;
         }
         if ((i > 0) && (i < MAX_INT_LENGTH))
         {
            str_number[i] = '\0';
            *ievent_buf_length = atoi(str_number) *
                                 ((sizeof(struct inotify_event)) + 16);
         }
         else
         {
            *ievent_buf_length = 0; /* No inotify support! */
         }
      }
      if (close(fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Failed to close() %s : %s",
                    INOTIFY_MAX_QUEUED_EVENTS, strerror(errno));
      }
   }

   return;
}
#endif


/*+++++++++++++++++++++++++ get_afd_config_value() ++++++++++++++++++++++*/
static void
get_afd_config_value(void)
{
   char *buffer,
        config_file[MAX_PATH_LENGTH],
        *ptr;

   (void)snprintf(config_file, MAX_PATH_LENGTH, "%s%s%s",
                  p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   if ((eaccess(config_file, F_OK) == 0) &&
       (read_file_no_cr(config_file, &buffer, YES, __FILE__, __LINE__) != INCORRECT))
   {
      char value[MAX_ADD_LOCKED_FILES_LENGTH];

      if (get_definition(buffer, ONE_DIR_COPY_TIMEOUT_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         one_dir_copy_timeout = atoi(value);
         if ((one_dir_copy_timeout < 2) || (one_dir_copy_timeout > 3600))
         {
            one_dir_copy_timeout = ONE_DIR_COPY_TIMEOUT;
         }
      }
      else
      {
         one_dir_copy_timeout = ONE_DIR_COPY_TIMEOUT;
      }
      if (get_definition(buffer, FULL_SCAN_TIMEOUT_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         full_scan_timeout = atoi(value);
         if ((full_scan_timeout < 0) || (full_scan_timeout > 3600))
         {
            full_scan_timeout = FULL_SCAN_TIMEOUT;
         }
      }
      else
      {
         full_scan_timeout = FULL_SCAN_TIMEOUT;
      }
      if (get_definition(buffer, FORCE_REREAD_INTERVAL_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         force_reread_interval = atoi(value);
      }
      else
      {
         force_reread_interval = FORCE_REREAD_INTERVAL;
      }
#ifndef _WITH_PTHREAD
      if (get_definition(buffer, DIR_CHECK_TIMEOUT_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         dir_check_timeout = atoi(value);
         if ((dir_check_timeout < 2) || (dir_check_timeout > 3600))
         {
            dir_check_timeout = DIR_CHECK_TIMEOUT;
         }
      }
      else
      {
         dir_check_timeout = DIR_CHECK_TIMEOUT;
      }
      if (get_definition(buffer, MAX_COPIED_FILES_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         max_file_buffer = atoi(value);
         if (max_file_buffer < 1)
         {
            max_file_buffer = MAX_COPIED_FILES;
         }
         else if (max_file_buffer > MAX_FILE_BUFFER_SIZE)
              {
                 max_file_buffer = MAX_FILE_BUFFER_SIZE;
              }
      }
      else
      {
         max_file_buffer = MAX_COPIED_FILES;
      }
#endif
      if (get_definition(buffer, DEFAULT_AGE_LIMIT_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         default_age_limit = (unsigned int)atoi(value);
      }
      else
      {
         default_age_limit = DEFAULT_AGE_LIMIT;
      }
      if (get_definition(buffer, EXEC_TIMEOUT_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         default_exec_timeout = atol(value);
      }
      else
      {
         default_exec_timeout = DEFAULT_EXEC_TIMEOUT;
      }
      if (get_definition(buffer, ADDITIONAL_LOCKED_FILES_DEF,
                         value, MAX_ADD_LOCKED_FILES_LENGTH) != NULL)
      {
         int length = 0;

         ptr = value;
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
      }
#ifdef HAVE_SETPRIORITY
      if (get_definition(buffer, EXEC_BASE_PRIORITY_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         exec_base_priority = atoi(value);
      }
      if (get_definition(buffer, ADD_AFD_PRIORITY_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         if (((value[0] == 'n') || (value[0] == 'N')) &&
             ((value[1] == 'o') || (value[1] == 'O')) &&
             ((value[2] == '\0') || (value[2] == ' ') || (value[2] == '\t')))
         {
            add_afd_priority = NO;
         }
         else if (((value[0] == 'y') || (value[0] == 'Y')) &&
                  ((value[1] == 'e') || (value[1] == 'E')) &&
                  ((value[2] == 's') || (value[2] == 'S')) &&
                  ((value[3] == '\0') || (value[3] == ' ') || (value[3] == '\t')))
              {
                 add_afd_priority = YES;
              }
              else
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Only YES or NO (and not `%s') are possible for %s in AFD_CONFIG. Setting to default: %s",
                            value, ADD_AFD_PRIORITY_DEF,
                            (add_afd_priority == YES) ? "YES" : "NO");
              }
      }
      if (get_definition(buffer, MAX_NICE_VALUE_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         max_sched_priority = atoi(value);
      }
      if (get_definition(buffer, MIN_NICE_VALUE_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         min_sched_priority = atoi(value);
      }
#endif
      if (get_definition(buffer, BUL_RULE_FILE_NAME_DEF,
                         value, MAX_PATH_LENGTH) != NULL)
      {
         int length;

         length = strlen(p_work_dir) + ETC_DIR_LENGTH + 1 + strlen(value) + 1;

         if ((bul_file = malloc(length)) == NULL)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "malloc() error, failed to allocate %d bytes for bulletin file name : %s",
                       length, strerror(errno));
         }
         else
         {
            (void)snprintf(bul_file, length,
                           "%s%s/%s", p_work_dir, ETC_DIR, value);
         }
      }
      if (get_definition(buffer, REP_RULE_FILE_NAME_DEF,
                         value, MAX_PATH_LENGTH) != NULL)
      {
         int length;

         length = strlen(p_work_dir) + ETC_DIR_LENGTH + 1 + strlen(value) + 1;

         if ((rep_file = malloc(length)) == NULL)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "malloc() error, failed to allocate %d bytes for report file name : %s",
                       length, strerror(errno));
         }
         else
         {
            (void)snprintf(rep_file, length,
                           "%s%s/%s", p_work_dir, ETC_DIR, value);
         }
      }
#ifdef MULTI_FS_SUPPORT
      get_extra_work_dirs(buffer, &no_of_extra_work_dirs, &ewl, YES);
#endif

      free(buffer);
   }
   else
   {
      one_dir_copy_timeout = ONE_DIR_COPY_TIMEOUT;
#ifndef _WITH_PTHREAD
      dir_check_timeout = DIR_CHECK_TIMEOUT;
#endif
      default_age_limit = DEFAULT_AGE_LIMIT;
      max_file_buffer = MAX_COPIED_FILES;
      default_exec_timeout = DEFAULT_EXEC_TIMEOUT;
#ifdef MULTI_FS_SUPPORT
      no_of_extra_work_dirs = 0;
      ewl = NULL;
#endif
   }

   return;
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(void)
{
   (void)fprintf(stderr,
                 "SYNTAX  : dir_check [--version] <AFD working directory> <rescan time> <no. of process> <no. of dirs> <create source dir mode> <udc pid>\n");
   exit(INCORRECT);
}
