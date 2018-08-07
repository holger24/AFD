/*
 *  gf_http.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2018 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   gf_http - gets files via HTTP
 **
 ** SYNOPSIS
 **   gf_http <work dir> <job no.> <FSA id> <FSA pos> <dir alias> [options]
 **
 **   options
 **      --version        Version Number
 **      -d               Distributed helper job.
 **      -o <retries>     Old/Error message and number of retries.
 **      -t               Temp toggle.
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has
 **   occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   18.11.2003 H.Kiehl Created
 **   13.06.2004 H.Kiehl Added transfer rate limit.
 **   18.08.2006 H.Kiehl Added handling of directory listing.
 **
 */
DESCR__E_M1

#include <stdio.h>                     /* fprintf(), snprintf()          */
#include <string.h>                    /* strcpy(), strcat(), strcmp(),  */
                                       /* strerror()                     */
#include <stdlib.h>                    /* malloc(), free(), abort()      */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _OUTPUT_LOG
# include <sys/times.h>                /* times()                        */
#endif
#include <fcntl.h>
#include <signal.h>                    /* signal()                       */
#include <unistd.h>                    /* close(), getpid()              */
#include <errno.h>
#include "httpdefs.h"
#include "fddefs.h"
#include "version.h"

/* Global variables. */
unsigned int               special_flag = 0;
int                        event_log_fd = STDERR_FILENO,
                           exitflag = IS_FAULTY_VAR,
                           files_to_retrieve_shown = 0,
                           fra_fd = -1,
                           fra_id,
                           fsa_fd = -1,
                           fsa_id,
#ifdef HAVE_HW_CRC32
                           have_hw_crc32 = NO,
#endif
#ifdef _MAINTAINER_LOG
                           maintainer_log_fd = STDERR_FILENO,
#endif
                           no_of_dirs = 0,
                           no_of_hosts = 0,
                           *no_of_listed_files,
                           *p_no_of_hosts = NULL,
                           prev_no_of_files_done = 0,
                           rl_fd = -1,
                           simulation_mode = NO,
                           sys_log_fd = STDERR_FILENO,
                           trans_db_log_fd = STDERR_FILENO,
                           transfer_log_fd = STDERR_FILENO,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                           trans_db_log_readfd,
                           transfer_log_readfd,
#endif
                           timeout_flag;
#ifdef WITH_IP_DB
int                        use_ip_db = YES;
#endif
#ifdef _OUTPUT_LOG
int                        ol_fd = -2;
# ifdef WITHOUT_FIFO_RW_SUPPORT
int                        ol_readfd = -2;
# endif
unsigned int               *ol_job_number,
                           *ol_retries;
char                       *ol_data = NULL,
                           *ol_file_name,
                           *ol_output_type;
unsigned short             *ol_archive_name_length,
                           *ol_file_name_length,
                           *ol_unl;
off_t                      *ol_file_size;
size_t                     ol_size,
                           ol_real_size;
clock_t                    *ol_transfer_time;
#endif
off_t                      file_size_to_retrieve_shown = 0;
u_off_t                    prev_file_size_done = 0;
#ifdef _WITH_BURST_2
unsigned int               burst_2_counter = 0;
#endif
#ifdef HAVE_MMAP
off_t                      fra_size,
                           fsa_size;
#endif
long                       transfer_timeout;
clock_t                    clktck;
char                       msg_str[MAX_RET_MSG_LENGTH],
                           *p_work_dir = NULL,
                           tr_hostname[MAX_HOSTNAME_LENGTH + 2];
struct retrieve_list       *rl;
#ifdef _DELETE_LOG
struct delete_log          dl;
#endif
struct filetransfer_status *fsa;
struct fileretrieve_status *fra;
struct job                 db;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void                gf_http_exit(void),
                           sig_bus(int),
                           sig_segv(int),
                           sig_kill(int),
                           sig_exit(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int              adjust_rl_size,
                    blocksize,
                    chunksize,
                    current_toggle,
                    exit_status = TRANSFER_SUCCESS,
                    fd,
                    files_retrieved = 0,
                    files_to_retrieve = 0,
                    i,
                    in_burst_loop = NO,
                    local_file_length,
                    more_files_in_list,
                    status;
   unsigned int     loop_counter;
#ifdef _WITH_BURST_2
   int              cb2_ret = NO;
   unsigned int     values_changed = 0;
#endif
   off_t            bytes_done,
                    content_length,
                    file_size_retrieved = 0,
                    file_size_to_retrieve = 0,
                    tmp_content_length;
   time_t           connected,
#ifdef _WITH_BURST_2
                    diff_time,
#endif
                    end_transfer_time_file,
                    start_transfer_time_file = 0;
   char             *buffer,
                    *chunkbuffer = NULL,
                    local_file[MAX_PATH_LENGTH],
                    local_tmp_file[MAX_PATH_LENGTH],
                    *p_local_file,
                    *p_local_tmp_file;
   struct stat      stat_buf;
#ifdef SA_FULLDUMP
   struct sigaction sact;
#endif
#ifdef _OUTPUT_LOG
   clock_t          end_time = 0,
                    start_time = 0;
   struct tms       tmsdummy;
#endif

   CHECK_FOR_VERSION(argc, argv);

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
                 "sigaction() error : %s", strerror(errno));
      exit(INCORRECT);
   }
#endif

   /* Do some cleanups when we exit. */
   if (atexit(gf_http_exit) != 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not register exit function : %s", strerror(errno));
      exit(INCORRECT);
   }

#ifdef _DELETE_LOG
   dl.fd = -1;
#endif

   /* Initialise variables. */
   init_gf(argc, argv, HTTP_FLAG);
   msg_str[0] = '\0';
   if (fsa->trl_per_process > 0)
   {
      if ((clktck = sysconf(_SC_CLK_TCK)) <= 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not get clock ticks per second : %s",
                    strerror(errno));
         exit(INCORRECT);
      }
      if (fsa->trl_per_process < fsa->block_size)
      {
         blocksize = fsa->trl_per_process;
      }
      else
      {
         blocksize = fsa->block_size;
      }
   }
   else
   {
      blocksize = fsa->block_size;
   }

   if ((signal(SIGINT, sig_kill) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, SIG_IGN) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR) ||
       (signal(SIGPIPE, SIG_IGN) == SIG_ERR))
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "signal() error : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Now determine the real hostname. */
   if (db.toggle_host == YES)
   {
      if (fsa->host_toggle == HOST_ONE)
      {
         (void)strcpy(db.hostname, fsa->real_hostname[HOST_TWO - 1]);
         current_toggle = HOST_TWO;
      }
      else
      {
         (void)strcpy(db.hostname, fsa->real_hostname[HOST_ONE - 1]);
         current_toggle = HOST_ONE;
      }
   }
   else
   {
      current_toggle = (int)fsa->host_toggle;
      (void)strcpy(db.hostname,
                   fsa->real_hostname[(int)(fsa->host_toggle - 1)]);
   }

   if (fsa->debug > NORMAL_MODE)
   {
      trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                   "Trying to connect to %s at port %d.",
                   db.hostname, db.port);
   }

   /* Connect to remote HTTP-server. */
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
   if (fsa->protocol_options & AFD_TCP_KEEPALIVE)
   {
      timeout_flag = transfer_timeout - 5;
      if (timeout_flag < MIN_KEEP_ALIVE_INTERVAL)
      {
         timeout_flag = MIN_KEEP_ALIVE_INTERVAL;
      }
   }
#else
   timeout_flag = OFF;
#endif
#ifdef WITH_IP_DB
   set_store_ip((fsa->host_status & STORE_IP) ? YES : NO);
#endif
   status = http_connect(db.hostname, db.http_proxy,
                         db.port, db.user, db.password,
#ifdef WITH_SSL
                         db.auth,
                         (fsa->protocol_options & TLS_STRICT_VERIFY) ? YES : NO,
#endif
                         db.sndbuf_size, db.rcvbuf_size);
#ifdef WITH_IP_DB
   if (get_and_reset_store_ip() == DONE)
   {
      fsa->host_status &= ~STORE_IP;
   }
#endif
   if (status != SUCCESS)
   {
      if (db.http_proxy[0] == '\0')
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   "HTTP connection to %s at port %d failed (%d). [%s]",
                   db.hostname, db.port, status, fra[db.fra_pos].dir_alias);
      }
      else
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   "HTTP connection to HTTP proxy %s at port %d failed (%d). [%s]",
                   db.http_proxy, db.port, status, fra[db.fra_pos].dir_alias);
      }
      exit(CONNECT_ERROR);
   }
   else
   {
      if (fsa->debug > NORMAL_MODE)
      {
#ifdef WITH_SSL
         char *p_msg_str;

         if ((db.auth == YES) || (db.auth == BOTH))
         {
            p_msg_str = msg_str;
         }
         else
         {
            p_msg_str = NULL;
         }
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, p_msg_str, "Connected.");
#else
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL, "Connected.");
#endif
      }
   }
   connected = time(NULL);

#ifdef _WITH_BURST_2
   do
   {
      if (in_burst_loop == YES)
      {
         fsa->job_status[(int)db.job_no].job_id = db.id.dir;
         if (fsa->debug > NORMAL_MODE)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
# ifdef WITH_SSL
                         "%s Bursting. [values_changed=%u]", (db.auth == NO) ? "HTTP" : "HTTPS",
# else
                         "HTTP Bursting. [values_changed=%u]",
# endif
                         values_changed);
         }
      }

      if ((in_burst_loop == NO) || (values_changed & TARGET_DIR_CHANGED))
      {
#endif /* _WITH_BURST_2 */
         if ((status = http_options(db.hostname, db.target_dir)) != SUCCESS)
         {
            trans_log((timeout_flag == ON) ? ERROR_SIGN : DEBUG_SIGN,
                      __FILE__, __LINE__, NULL, msg_str,
                      "Failed to get options (%d).", status);
            if (timeout_flag == ON)
            {
               http_quit();
               exit(eval_timeout(OPEN_REMOTE_ERROR));
            }
         }
         else
         {
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                            "Got HTTP server options.");
            }
         }
#ifdef _WITH_BURST_2
      }
#endif /* _WITH_BURST_2 */

      fsa->job_status[(int)db.job_no].connect_status = HTTP_RETRIEVE_ACTIVE;
      if (db.special_flag & DISTRIBUTED_HELPER_JOB)
      {
         /*
          * If we are a helper job, lets NOT stay connected and do a
          * full directory scan.
          */
         db.keep_connected = 0;
      }
      more_files_in_list = NO;
      loop_counter = 0;
      do
      {
         if ((files_to_retrieve = get_remote_file_names_http(&file_size_to_retrieve,
                                                             &more_files_in_list)) > 0)
         {
            int diff_no_of_files_done;

            if ((more_files_in_list == YES) &&
                ((fra[db.fra_pos].dir_flag & DO_NOT_PARALLELIZE) == 0) &&
                (fsa->active_transfers < fsa->allowed_transfers))
            {
               /* Tell fd that he may start some more helper jobs that */
               /* help fetching files.                                 */
               send_proc_fin(YES);
            }

            /* Inform FSA that we have finished connecting and */
            /* will now start to retrieve data.                */
            if (gsf_check_fsa((struct job *)&db) != NEITHER)
            {
               fsa->job_status[(int)db.job_no].no_of_files += files_to_retrieve;
               fsa->job_status[(int)db.job_no].file_size += file_size_to_retrieve;

               /* Number of connections. */
               fsa->connections += 1;

               /* Total file counter. */
#ifdef LOCK_DEBUG
               lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
               lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC);
#endif
               fsa->total_file_counter += files_to_retrieve;
               fsa->total_file_size += file_size_to_retrieve;
#ifdef LOCK_DEBUG
               unlock_region(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
               unlock_region(fsa_fd, db.lock_offset + LOCK_TFC);
#endif
               files_to_retrieve_shown += files_to_retrieve;
               file_size_to_retrieve_shown += file_size_to_retrieve;
            }

            (void)gsf_check_fra();
            if ((db.fra_pos == INCORRECT) || (db.fsa_pos == INCORRECT))
            {
               /*
                * Looks as if this directory/host is no longer in our database.
                */
               trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Database changed, exiting.");
               (void)http_quit();
               reset_values(files_retrieved, file_size_retrieved,
                            files_to_retrieve, file_size_to_retrieve,
                            (struct job *)&db);
               exit(TRANSFER_SUCCESS);
            }

            /* Get directory where files are to be stored and */
            /* prepare some pointers for the file names.      */
            if (create_remote_dir(fra[db.fra_pos].url,
                                  fra[db.fra_pos].retrieve_work_dir, NULL,
                                  NULL, NULL, local_file,
                                  &local_file_length) == INCORRECT)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to determine local incoming directory for <%s>.",
                          fra[db.fra_pos].dir_alias);
               http_quit();
               reset_values(files_retrieved, file_size_retrieved,
                            files_to_retrieve, file_size_to_retrieve,
                            (struct job *)&db);
               exit(INCORRECT);
            }
            else
            {
               local_file[local_file_length - 1] = '/';
               local_file[local_file_length] = '\0';
               (void)strcpy(local_tmp_file, local_file);
               p_local_file = &local_file[local_file_length];
               p_local_tmp_file = &local_tmp_file[local_file_length];
               *p_local_tmp_file = '.';
               p_local_tmp_file++;
            }

            /* Allocate buffer to read data from the source file. */
            if ((buffer = malloc(blocksize + 4)) == NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to malloc() %d bytes : %s",
                          blocksize + 4, strerror(errno));
               http_quit();
               exit(ALLOC_ERROR);
            }

            /* Retrieve all files. */
            for (i = 0; i < *no_of_listed_files; i++)
            {
               if ((rl[i].retrieved == NO) &&
                   (rl[i].assigned == ((unsigned char)db.job_no + 1)))
               {
                  int   prev_download_exists = NO;
                  off_t offset;

                  if (rl[i].file_name[0] != '.')
                  {
                     (void)strcpy(p_local_tmp_file, rl[i].file_name);
                  }
                  else
                  {
                     (void)strcpy(p_local_file, rl[i].file_name);
                  }
                  if (fsa->file_size_offset != -1)
                  {
                     if (stat(local_tmp_file, &stat_buf) == -1)
                     {
                        if (fra[db.fra_pos].stupid_mode == APPEND_ONLY)
                        {
                           offset = rl[i].prev_size;
                        }
                        else
                        {
                           offset = 0;
                        }
                     }
                     else
                     {
                        offset = stat_buf.st_size;
                     }
                  }
                  else
                  {
                     if (fra[db.fra_pos].stupid_mode == APPEND_ONLY)
                     {
                        offset = rl[i].prev_size;
                     }
                     else
                     {
                        offset = 0;
                     }
                  }

                  if (rl[i].size == -1)
                  {
                     content_length = 0;
                  }
                  else
                  {
                     content_length = rl[i].size;
                  }
                  tmp_content_length = rl[i].size;

#ifdef _OUTPUT_LOG 
                  if (db.output_log == YES)
                  {
                     start_time = times(&tmsdummy);
                  }
#endif
                  if (((status = http_get(db.hostname, db.target_dir,
                                          rl[i].file_name,
                                          &tmp_content_length, offset)) != SUCCESS) &&
                      (status != CHUNKED) && (status != NOTHING_TO_FETCH) &&
                      (status != 301) && (status != 400) && (status != 404))
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                               "Failed to open remote file %s in %s (%d).",
                               rl[i].file_name, fra[db.fra_pos].dir_alias, status);
                     (void)http_quit();
                     exit(eval_timeout(OPEN_REMOTE_ERROR));
                  }
                  if (tmp_content_length != content_length)
                  {
                     content_length = tmp_content_length;
                     adjust_rl_size = NO;
                  }
                  else
                  {
                     adjust_rl_size = YES;
                  }
                  if ((status == 301) || /* Moved Permanently. */
                      (status == 400) || /* Bad Requeuest. */
                      (status == 404))   /* Not Found. */
                  {
                     bytes_done = 0;
                     trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                               "Failed to open remote file %s in %s (%d).",
                               rl[i].file_name, fra[db.fra_pos].dir_alias, status);

                     /*
                      * Mark this file as retrieved or else we will always
                      * fall over this file.
                      */
                     rl[i].retrieved = YES;

                     if (gsf_check_fsa((struct job *)&db) != NEITHER)
                     {
#ifdef LOCK_DEBUG
                        lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
                        lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC);
#endif
                        fsa->job_status[(int)db.job_no].file_name_in_use[0] = '\0';
                        fsa->job_status[(int)db.job_no].file_size_in_use = 0;
                        fsa->job_status[(int)db.job_no].file_size_in_use_done = 0;

                        /* Total file counter. */
                        fsa->total_file_counter -= 1;
                        files_to_retrieve_shown -= 1;
#ifdef _VERIFY_FSA
                        if (fsa->total_file_counter < 0)
                        {
                           int tmp_val;

                           tmp_val = files_to_retrieve - (files_retrieved + 1);
                           if (tmp_val < 0)
                           {
                              tmp_val = 0;
                           }
                           trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
                                     "Total file counter less then zero. Correcting to %d.",
                                     tmp_val);
                           fsa->total_file_counter = tmp_val;
                           files_to_retrieve_shown = tmp_val;
                        }
#endif

                        /* Total file size. */
                        if (rl[i].size > 0)
                        {
                           fsa->total_file_size -= (rl[i].size - offset);
                           file_size_to_retrieve_shown -= (rl[i].size - offset);
#ifdef _VERIFY_FSA
                           if (fsa->total_file_size < 0)
                           {
                              off_t new_size = file_size_to_retrieve - file_size_retrieved;

                              if (new_size < 0)
                              {
                                 new_size = 0;
                              }
                              fsa->total_file_size = new_size;
                              file_size_to_retrieve_shown = new_size;
                              trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
# if SIZEOF_OFF_T == 4
                                        "Total file size overflowed. Correcting to %ld.",
# else
                                        "Total file size overflowed. Correcting to %lld.",
# endif
                                        (pri_off_t)fsa->total_file_size);
                           }
                           else if ((fsa->total_file_counter == 0) &&
                                    (fsa->total_file_size > 0))
                                {
                                      trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
# if SIZEOF_OFF_T == 4
                                                "fc is zero but fs is not zero (%ld). Correcting.",
# else
                                                "fc is zero but fs is not zero (%lld). Correcting.",
# endif
                                                (pri_off_t)fsa->total_file_size);
                                   fsa->total_file_size = 0;
                                   file_size_to_retrieve_shown = 0;
                                }
#endif
                        }

#ifdef LOCK_DEBUG
                        unlock_region(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
                        unlock_region(fsa_fd, db.lock_offset + LOCK_TFC);
#endif
                     }
                     else if (db.fsa_pos == INCORRECT)
                          {
                             /*
                              * Looks as if this host is no longer in our
                              * database.
                              */
                             trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
                                       "Database changed, exiting.");
                             (void)http_quit();
                             reset_values(files_retrieved, file_size_retrieved,
                                          files_to_retrieve, file_size_to_retrieve,
                                          (struct job *)&db);
                             exit(TRANSFER_SUCCESS);
                          }
                  }
                  else /* status == SUCCESS | CHUNKED | NOTHING_TO_FETCH */
                  {
                     if (fsa->debug > NORMAL_MODE)
                     {
                        trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                                     "Opened HTTP connection for file %s.",
                                     rl[i].file_name);
                     }

                     if (prev_download_exists == YES)
                     {
#ifdef O_LARGEFILE
                        fd = open(local_tmp_file, O_WRONLY | O_APPEND |
                                  O_LARGEFILE);
#else
                        fd = open(local_tmp_file, O_WRONLY | O_APPEND);
#endif
                     }
                     else
                     {
#ifdef O_LARGEFILE
                        fd = open(local_tmp_file, O_WRONLY | O_CREAT | O_LARGEFILE,
                                  FILE_MODE);
#else
                        fd = open(local_tmp_file, O_WRONLY | O_CREAT, FILE_MODE);
#endif
                     }
                     if (fd == -1)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  "Failed to open local file %s : %s",
                                  local_tmp_file, strerror(errno));
                        http_quit();
                        reset_values(files_retrieved, file_size_retrieved,
                                     files_to_retrieve, file_size_to_retrieve,
                                     (struct job *)&db);
                        exit(OPEN_LOCAL_ERROR);
                     }
                     else
                     {
                        if (fsa->debug > NORMAL_MODE)
                        {
                           trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                                        "Opened local file %s [status=%d].",
                                        local_tmp_file, status);
                        }
                     }

                     if (gsf_check_fsa((struct job *)&db) != NEITHER)
                     {
                        if (content_length == -1)
                        {
                           if (rl[i].size == -1)
                           {
                              fsa->job_status[(int)db.job_no].file_size_in_use = 0;
                           }
                           else
                           {
                              fsa->job_status[(int)db.job_no].file_size_in_use = rl[i].size;
                           }
                        }
                        else
                        {
                           fsa->job_status[(int)db.job_no].file_size_in_use = content_length;
                        }
                        (void)strcpy(fsa->job_status[(int)db.job_no].file_name_in_use,
                                     rl[i].file_name);
                     }
                     else if (db.fsa_pos == INCORRECT)
                          {
                             /*
                              * Looks as if this host is no longer in our
                              * database.
                              */
                             trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
                                       "Database changed, exiting.");
                             (void)http_quit();
                             (void)close(fd);
                             (void)unlink(local_tmp_file);
                             reset_values(files_retrieved, file_size_retrieved,
                                          files_to_retrieve, file_size_to_retrieve,
                                          (struct job *)&db);
                             exit(TRANSFER_SUCCESS);
                          }

                     bytes_done = 0;
                     if (status != NOTHING_TO_FETCH)
                     {
                        if (fsa->trl_per_process > 0)
                        {
                           init_limit_transfer_rate();
                        }
                        if (fsa->protocol_options & TIMEOUT_TRANSFER)
                        {
                           start_transfer_time_file = time(NULL);
                        }

                        if (status == SUCCESS)
                        {
                           if (content_length == -1)
                           {
                              do
                              {
#ifdef WITH_DEBUG_HTTP_READ
                                 if (fsa->debug > NORMAL_MODE)
                                 {
                                    trans_db_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
# if SIZEOF_OFF_T == 4
                                                 "Reading blocksize %d (bytes_done=%ld).",
# else
                                                 "Reading blocksize %d (bytes_done=%ld).",
# endif
                                                 blocksize,
                                                 (pri_off_t)bytes_done);
                                 }
#endif
                                 if ((status = http_read(buffer, blocksize)) <= 0)
                                 {
                                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                                              "Failed to read from remote file %s in %s (%d)",
                                              rl[i].file_name,
                                              fra[db.fra_pos].dir_alias,
                                              status);
                                    reset_values(files_retrieved,
                                                 file_size_retrieved,
                                                 files_to_retrieve,
                                                 file_size_to_retrieve,
                                                 (struct job *)&db);
                                    http_quit();
                                    if (bytes_done == 0)
                                    {
                                       (void)unlink(local_tmp_file);
                                    }
                                    exit(eval_timeout(READ_REMOTE_ERROR));
                                 }
                                 if (fsa->trl_per_process > 0)
                                 {
                                    limit_transfer_rate(status,
                                                        fsa->trl_per_process,
                                                        clktck);
                                 }
                                 if (status > 0)
                                 {
                                    if (write(fd, buffer, status) != status)
                                    {
                                       trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                                 "Failed to write() to file %s : %s",
                                                 local_tmp_file,
                                                 strerror(errno));
                                       http_quit();
                                       reset_values(files_retrieved,
                                                    file_size_retrieved,
                                                    files_to_retrieve,
                                                    file_size_to_retrieve,
                                                    (struct job *)&db);
                                       if (bytes_done == 0)
                                       {
                                          (void)unlink(local_tmp_file);
                                       }
                                       exit(WRITE_LOCAL_ERROR);
                                    }
                                    bytes_done += status;
                                 }
#ifdef WITH_DEBUG_HTTP_READ
                                 if (fsa->debug > NORMAL_MODE)
                                 {
                                    trans_db_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
# if SIZEOF_OFF_T == 4
                                                 "Blocksize read = %d (bytes_done=%ld)",
# else
                                                 "Blocksize read = %d (bytes_done=%lld)",
# endif
                                                 status, (pri_off_t)bytes_done);
                                 }
#endif

                                 if (gsf_check_fsa((struct job *)&db) != NEITHER)
                                 {
                                    fsa->job_status[(int)db.job_no].file_size_in_use_done = bytes_done;
                                    fsa->job_status[(int)db.job_no].file_size_done += status;
                                    fsa->job_status[(int)db.job_no].bytes_send += status;
                                    if (fsa->protocol_options & TIMEOUT_TRANSFER)
                                    {
                                       end_transfer_time_file = time(NULL);
                                       if (end_transfer_time_file < start_transfer_time_file)
                                       {
                                          start_transfer_time_file = end_transfer_time_file;
                                       }
                                       else
                                       {
                                          if ((end_transfer_time_file - start_transfer_time_file) > transfer_timeout)
                                          {
                                             trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
#if SIZEOF_TIME_T == 4
                                                       "Transfer timeout reached for `%s' in %s after %ld seconds.",
#else
                                                       "Transfer timeout reached for `%s' in %s after %lld seconds.",
#endif
                                                       fsa->job_status[(int)db.job_no].file_name_in_use,
                                                       fra[db.fra_pos].dir_alias,
                                                       (pri_time_t)(end_transfer_time_file - start_transfer_time_file));
                                             http_quit();
                                             exit(STILL_FILES_TO_SEND);
                                          }
                                       }
                                    }
                                 }
                                 else if (db.fsa_pos == INCORRECT)
                                      {
                                         /*
                                          * Looks as if this host is no longer
                                          * in our database.
                                          */
                                         trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
                                                   "Database changed, exiting.");
                                         (void)http_quit();
                                         (void)close(fd);
                                         (void)unlink(local_tmp_file);
                                         reset_values(files_retrieved,
                                                      file_size_retrieved,
                                                      files_to_retrieve,
                                                      file_size_to_retrieve,
                                                      (struct job *)&db);
                                         exit(TRANSFER_SUCCESS);
                                      }
                              } while (status != 0);
                           }
                           else
                           {
                              int hunk_size;

                              while (bytes_done != content_length)
                              {
                                 hunk_size = content_length - bytes_done;
                                 if (hunk_size > blocksize)
                                 {
                                    hunk_size = blocksize;
                                 }
#ifdef WITH_DEBUG_HTTP_READ
                                 if (fsa->debug > NORMAL_MODE)
                                 {
                                    trans_db_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
# if SIZEOF_OFF_T == 4
                                                 "Reading blocksize %d (bytes_done=%ld).",
# else
                                                 "Reading blocksize %d (bytes_done=%ld).",
# endif
                                                 hunk_size, (pri_off_t)bytes_done);
                                 }
#endif
                                 if ((status = http_read(buffer, hunk_size)) <= 0)
                                 {
                                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                                              "Failed to read from remote file %s in %s (%d)",
                                              rl[i].file_name,
                                              fra[db.fra_pos].dir_alias,
                                              status);
                                    reset_values(files_retrieved,
                                                 file_size_retrieved,
                                                 files_to_retrieve,
                                                 file_size_to_retrieve,
                                                 (struct job *)&db);
                                    http_quit();
                                    if (bytes_done == 0)
                                    {
                                       (void)unlink(local_tmp_file);
                                    }
                                    exit(eval_timeout(READ_REMOTE_ERROR));
                                 }
                                 if (fsa->trl_per_process > 0)
                                 {
                                    limit_transfer_rate(status, fsa->trl_per_process,
                                                        clktck);
                                 }
                                 if (status > 0)
                                 {
                                    if (write(fd, buffer, status) != status)
                                    {
                                       trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                                 "Failed to write() to file %s : %s",
                                                 local_tmp_file,
                                                 strerror(errno));
                                       http_quit();
                                       reset_values(files_retrieved,
                                                    file_size_retrieved,
                                                    files_to_retrieve,
                                                    file_size_to_retrieve,
                                                    (struct job *)&db);
                                       if (bytes_done == 0)
                                       {
                                          (void)unlink(local_tmp_file);
                                       }
                                       exit(WRITE_LOCAL_ERROR);
                                    }
                                    bytes_done += status;
                                 }
#ifdef WITH_DEBUG_HTTP_READ
                                 if (fsa->debug > NORMAL_MODE)
                                 {
                                    trans_db_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
# if SIZEOF_OFF_T == 4
                                                 "Blocksize read = %d (bytes_done=%ld)",
# else
                                                 "Blocksize read = %d (bytes_done=%lld)",
# endif
                                                 status, (pri_off_t)bytes_done);
                                 }
#endif

                                 if (gsf_check_fsa((struct job *)&db) != NEITHER)
                                 {
                                    fsa->job_status[(int)db.job_no].file_size_in_use_done = bytes_done;
                                    fsa->job_status[(int)db.job_no].file_size_done += status;
                                    fsa->job_status[(int)db.job_no].bytes_send += status;
                                    if (fsa->protocol_options & TIMEOUT_TRANSFER)
                                    {
                                       end_transfer_time_file = time(NULL);
                                       if (end_transfer_time_file < start_transfer_time_file)
                                       {
                                          start_transfer_time_file = end_transfer_time_file;
                                       }
                                       else
                                       {
                                          if ((end_transfer_time_file - start_transfer_time_file) > transfer_timeout)
                                          {
                                             trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
#if SIZEOF_TIME_T == 4
                                                       "Transfer timeout reached for `%s' in %s after %ld seconds.",
#else
                                                       "Transfer timeout reached for `%s' in %s after %lld seconds.",
#endif
                                                       fsa->job_status[(int)db.job_no].file_name_in_use,
                                                       fra[db.fra_pos].dir_alias,
                                                       (pri_time_t)(end_transfer_time_file - start_transfer_time_file));
                                             http_quit();
                                             exit(STILL_FILES_TO_SEND);
                                          }
                                       }
                                    }
                                 }
                                 else if (db.fsa_pos == INCORRECT)
                                      {
                                         /*
                                          * Looks as if this host is no longer
                                          * in our database.
                                          */
                                         trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
                                                   "Database changed, exiting.");
                                         (void)http_quit();
                                         (void)close(fd);
                                         (void)unlink(local_tmp_file);
                                         reset_values(files_retrieved,
                                                      file_size_retrieved,
                                                      files_to_retrieve,
                                                      file_size_to_retrieve,
                                                      (struct job *)&db);
                                         exit(TRANSFER_SUCCESS);
                                      }
                              }
                           }
                        }
                        else /* We need to read data in chunks dictated by the server. */
                        {
                           if (chunkbuffer == NULL)
                           {
                              if ((chunkbuffer = malloc(blocksize + 4)) == NULL)
                              {
                                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                                            "Failed to malloc() %d bytes : %s",
                                            blocksize + 4, strerror(errno));
                                 http_quit();
                                 (void)unlink(local_tmp_file);
                                 exit(ALLOC_ERROR);
                              }
                              chunksize = blocksize + 4;
                           }
                           do
                           {
                              if ((status = http_chunk_read(&chunkbuffer,
                                                            &chunksize)) == INCORRECT)
                              {
                                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                                           "Failed to read from remote file %s in %s",
                                           rl[i].file_name, fra[db.fra_pos].dir_alias);
                                 reset_values(files_retrieved, file_size_retrieved,
                                              files_to_retrieve, file_size_to_retrieve,
                                              (struct job *)&db);
                                 http_quit();
                                 if (bytes_done == 0)
                                 {
                                    (void)unlink(local_tmp_file);
                                 }
                                 exit(eval_timeout(READ_REMOTE_ERROR));
                              }
                              if (fsa->trl_per_process > 0)
                              {
                                 limit_transfer_rate(status, fsa->trl_per_process,
                                                     clktck);
                              }
                              if (status > 0)
                              {
                                 if (write(fd, chunkbuffer, status) != status)
                                 {
                                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                              "Failed to write() to file %s : %s",
                                              local_tmp_file, strerror(errno));
                                    http_quit();
                                    reset_values(files_retrieved, file_size_retrieved,
                                                 files_to_retrieve,
                                                 file_size_to_retrieve,
                                                 (struct job *)&db);
                                    if (bytes_done == 0)
                                    {
                                       (void)unlink(local_tmp_file);
                                    }
                                    exit(WRITE_LOCAL_ERROR);
                                 }
                                 bytes_done += status;
                              }

                              if (gsf_check_fsa((struct job *)&db) != NEITHER)
                              {
                                 fsa->job_status[(int)db.job_no].file_size_in_use_done = bytes_done;
                                 fsa->job_status[(int)db.job_no].file_size_done += status;
                                 fsa->job_status[(int)db.job_no].bytes_send += status;
                              }
                              else if (db.fsa_pos == INCORRECT)
                                   {
                                      /*
                                       * Looks as if this host is no longer
                                       * in our database.
                                       */
                                      trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
                                                "Database changed, exiting.");
                                      (void)http_quit();
                                      (void)close(fd);
                                      (void)unlink(local_tmp_file);
                                      reset_values(files_retrieved,
                                                   file_size_retrieved,
                                                   files_to_retrieve,
                                                   file_size_to_retrieve,
                                                   (struct job *)&db);
                                      exit(TRANSFER_SUCCESS);
                                   }
                           } while (status != HTTP_LAST_CHUNK);
                        }
                     } /* if (status != NOTHING_TO_FETCH) */

#ifdef _OUTPUT_LOG
                     if (db.output_log == YES)
                     {
                        end_time = times(&tmsdummy);
                     }
#endif

                     /* Close the local file. */
                     if (close(fd) == -1)
                     {
                        trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  "Failed to close() local file %s.",
                                  local_tmp_file);
                     }
                     else
                     {
                        if (fsa->debug > NORMAL_MODE)
                        {
                           trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                                        "Closed local file %s.", local_tmp_file);
                        }
                     }

                     /* Check if remote file is to be deleted. */
                     if (fra[db.fra_pos].remove == YES)
                     {
                        if ((status = http_del(db.hostname, db.target_dir,
                                               rl[i].file_name)) != SUCCESS)
                        {
                           trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, msg_str,
                                     "Failed to delete remote file %s in %s (%d).",
                                     rl[i].file_name,
                                     fra[db.fra_pos].dir_alias, status);
                        }
                        else
                        {
                           if (fsa->debug > NORMAL_MODE)
                           {
                              trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                           "Deleted remote file %s in %s.",
                                           rl[i].file_name,
                                           fra[db.fra_pos].dir_alias);
                           }
                        }
                     }

                     if (gsf_check_fsa((struct job *)&db) != NEITHER)
                     {
#ifdef LOCK_DEBUG
                        lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
                        lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC);
#endif
                        fsa->job_status[(int)db.job_no].file_name_in_use[0] = '\0';
                        fsa->job_status[(int)db.job_no].no_of_files_done++;
                        fsa->job_status[(int)db.job_no].file_size_in_use = 0;
                        fsa->job_status[(int)db.job_no].file_size_in_use_done = 0;

                        /* Total file counter. */
                        fsa->total_file_counter -= 1;
                        files_to_retrieve_shown -= 1;
#ifdef _VERIFY_FSA
                        if (fsa->total_file_counter < 0)
                        {
                           int tmp_val;

                           tmp_val = files_to_retrieve - (files_retrieved + 1);
                           if (tmp_val < 0)
                           {
                              tmp_val = 0;
                           }
                           trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
                                     "Total file counter less then zero. Correcting to %d.",
                                     tmp_val);
                           fsa->total_file_counter = tmp_val;
                           files_to_retrieve_shown = tmp_val;
                        }
#endif

                        if ((rl[i].size != content_length) && (content_length > 0))
                        {
                           fsa->total_file_size += content_length;
                           file_size_to_retrieve_shown += content_length;
                           fsa->job_status[(int)db.job_no].file_size += content_length;
                           if (adjust_rl_size == YES)
                           {
                              trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
#if SIZEOF_OFF_T == 4
                                        "content_length (%ld) != rl[i].size (%ld)",
#else
                                        "content_length (%lld) != rl[i].size (%lld)",
#endif
                                        (pri_off_t)content_length,
                                        (pri_off_t)rl[i].size);
                              rl[i].size = content_length;
                           }
                        }

                        /* Total file size. */
                        if (content_length > 0)
                        {
                           fsa->total_file_size -= content_length;
                           file_size_to_retrieve_shown -= content_length;
#ifdef _VERIFY_FSA
                           if (fsa->total_file_size < 0)
                           {
                              off_t new_size = file_size_to_retrieve - file_size_retrieved;

                              if (new_size < 0)
                              {
                                 new_size = 0;
                              }
                              fsa->total_file_size = new_size;
                              file_size_to_retrieve_shown = new_size;
                              trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
# if SIZEOF_OFF_T == 4
                                        "Total file size overflowed. Correcting to %ld.",
# else
                                        "Total file size overflowed. Correcting to %lld.",
# endif
                                        (pri_off_t)fsa->total_file_size);
                           }
                           else if ((fsa->total_file_counter == 0) &&
                                    (fsa->total_file_size > 0))
                                {
                                      trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
# if SIZEOF_OFF_T == 4
                                                "fc is zero but fs is not zero (%ld). Correcting.",
# else
                                                "fc is zero but fs is not zero (%lld). Correcting.",
# endif
                                                (pri_off_t)fsa->total_file_size);
                                   fsa->total_file_size = 0;
                                   file_size_to_retrieve_shown = 0;
                                }
#endif
                        }

                        /* File counter done. */
                        fsa->file_counter_done += 1;

                        /* Number of bytes send. */
                        fsa->bytes_send += bytes_done;

                        /* Update last activity time. */
                        fsa->last_connection = time(NULL);
#ifdef LOCK_DEBUG
                        unlock_region(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
                        unlock_region(fsa_fd, db.lock_offset + LOCK_TFC);
#endif

                        (void)gsf_check_fra();
                        if (db.fra_pos != INCORRECT)
                        {
                           if (fra[db.fra_pos].error_counter > 0)
                           {
                              lock_region_w(fra_fd,
#ifdef LOCK_DEBUG
                                            (char *)&fra[db.fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                                            (char *)&fra[db.fra_pos].error_counter - (char *)fra);
#endif
                              fra[db.fra_pos].error_counter = 0;
                              if (fra[db.fra_pos].dir_flag & DIR_ERROR_SET)
                              {
                                 fra[db.fra_pos].dir_flag &= ~DIR_ERROR_SET;
                                 SET_DIR_STATUS(fra[db.fra_pos].dir_flag,
                                                time(NULL),
                                                fra[db.fra_pos].start_event_handle,
                                                fra[db.fra_pos].end_event_handle,
                                                fra[db.fra_pos].dir_status);
                                 error_action(fra[db.fra_pos].dir_alias, "stop",
                                              DIR_ERROR_ACTION);
                                 event_log(0L, EC_DIR, ET_EXT, EA_ERROR_END, "%s",
                                           fra[db.fra_pos].dir_alias);
                              }
                              unlock_region(fra_fd,
#ifdef LOCK_DEBUG
                                            (char *)&fra[db.fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                                            (char *)&fra[db.fra_pos].error_counter - (char *)fra);
#endif
                           }
                        }

                        if (fsa->error_counter > 0)
                        {
                           int  fd, j;
#ifdef WITHOUT_FIFO_RW_SUPPORT
                           int  readfd;
#endif
                           char fd_wake_up_fifo[MAX_PATH_LENGTH];

#ifdef LOCK_DEBUG
                           lock_region_w(fsa_fd, db.lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
                           lock_region_w(fsa_fd, db.lock_offset + LOCK_EC);
#endif
                           fsa->error_counter = 0;

                           /* Wake up FD! */
                           (void)snprintf(fd_wake_up_fifo, MAX_PATH_LENGTH,
                                          "%s%s%s", p_work_dir,
                                          FIFO_DIR, FD_WAKE_UP_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
                           if (open_fifo_rw(fd_wake_up_fifo, &readfd, &fd) == -1)
#else
                           if ((fd = open(fd_wake_up_fifo, O_RDWR)) == -1)
#endif
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                         "Failed to open() FIFO `%s' : %s",
                                         fd_wake_up_fifo, strerror(errno));
                           }
                           else
                           {
                              char dummy;

                              if (write(fd, &dummy, 1) != 1)
                              {
                                 system_log(WARN_SIGN, __FILE__, __LINE__,
                                            "Failed to write() to FIFO `%s' : %s",
                                            fd_wake_up_fifo, strerror(errno));
                              }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                              if (close(readfd) == -1)
                              {
                                 system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                            "Failed to close() FIFO `%s' (read) : %s",
                                            fd_wake_up_fifo, strerror(errno));
                              }
#endif
                              if (close(fd) == -1)
                              {
                                 system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                            "Failed to close() FIFO `%s' : %s",
                                            fd_wake_up_fifo, strerror(errno));
                              }
                           }

                           /*
                            * Remove the error condition (NOT_WORKING) from all jobs
                            * of this host.
                            */
                           for (j = 0; j < fsa->allowed_transfers; j++)
                           {
                              if ((j != db.job_no) &&
                                  (fsa->job_status[j].connect_status == NOT_WORKING))
                              {
                                 fsa->job_status[j].connect_status = DISCONNECT;
                              }
                           }
                           fsa->error_history[0] = 0;
                           fsa->error_history[1] = 0;
#ifdef LOCK_DEBUG
                           unlock_region(fsa_fd, db.lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
                           unlock_region(fsa_fd, db.lock_offset + LOCK_EC);
#endif

#ifdef LOCK_DEBUG
                           lock_region_w(fsa_fd, db.lock_offset + LOCK_HS, __FILE__, __LINE__);
#else
                           lock_region_w(fsa_fd, db.lock_offset + LOCK_HS);
#endif
                           if (time(NULL) > fsa->end_event_handle)
                           {
                              fsa->host_status &= ~(EVENT_STATUS_FLAGS | AUTO_PAUSE_QUEUE_STAT);
                              if (fsa->end_event_handle > 0L)
                              {
                                 fsa->end_event_handle = 0L;
                              }
                              if (fsa->start_event_handle > 0L)
                              {
                                 fsa->start_event_handle = 0L;
                              }
                           }
                           else
                           {
                              fsa->host_status &= ~(EVENT_STATUS_STATIC_FLAGS | AUTO_PAUSE_QUEUE_STAT);
                           }
#ifdef LOCK_DEBUG
                           unlock_region(fsa_fd, db.lock_offset + LOCK_HS, __FILE__, __LINE__);
#else
                           unlock_region(fsa_fd, db.lock_offset + LOCK_HS);
#endif

                           /*
                            * Since we have successfully retrieved a file, no
                            * need to have the queue stopped anymore.
                            */
                           if (fsa->host_status & AUTO_PAUSE_QUEUE_STAT)
                           {
                              char sign[LOG_SIGN_LENGTH];

                              error_action(fsa->host_alias, "stop", HOST_ERROR_ACTION);
                              event_log(0L, EC_HOST, ET_EXT, EA_ERROR_END, "%s",
                                        fsa->host_alias);
                              if ((fsa->host_status & HOST_ERROR_OFFLINE_STATIC) ||
                                  (fsa->host_status & HOST_ERROR_OFFLINE) ||
                                  (fsa->host_status & HOST_ERROR_OFFLINE_T))
                              {
                                 (void)memcpy(sign, OFFLINE_SIGN, LOG_SIGN_LENGTH);
                              }
                              else
                              {
                                 (void)memcpy(sign, INFO_SIGN, LOG_SIGN_LENGTH);
                              }
                              trans_log(sign, __FILE__, __LINE__, NULL, NULL,
                                        "Starting input queue that was stopped by init_afd.");
                              event_log(0L, EC_HOST, ET_AUTO, EA_START_QUEUE, "%s",
                                        fsa->host_alias);
                           }
                        }

#ifdef WITH_ERROR_QUEUE
                        if (fsa->host_status & ERROR_QUEUE_SET)
                        {
                           remove_from_error_queue(db.id.dir, fsa, db.fsa_pos,
                                                   fsa_fd);
                        }
#endif
                        if (fsa->host_status & HOST_ACTION_SUCCESS)
                        {
                           error_action(fsa->host_alias, "start",
                                        HOST_SUCCESS_ACTION);
                        }
                     }

                     /*
                      * If the file size is not the same as the one when we
                      * did the remote ls command, give a warning in the
                      * transfer log so some action can be taken against
                      * the originator.
                      */
                     if ((content_length > 0) &&
                         ((bytes_done + offset) != content_length))
                     {
                        trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
#if SIZEOF_OFF_T == 4
                                  "File size of file %s in %s changed from %ld to %ld when it was retrieved.",
#else
                                  "File size of file %s in %s changed from %lld to %lld when it was retrieved.",
#endif
                                  rl[i].file_name,
                                  (db.fra_pos == INCORRECT) ? "unknown" : fra[db.fra_pos].dir_alias,
                                  (pri_off_t)content_length,
                                  (pri_off_t)(bytes_done + offset));
                     }

                     /* Rename the file so AMG can grab it. */
                     if (rl[i].file_name[0] == '.')
                     {
                        (void)strcpy(p_local_file, &rl[i].file_name[1]);
                     }
                     else
                     {
                        (void)strcpy(p_local_file, rl[i].file_name);
                     }
                     if (rename(local_tmp_file, local_file) == -1)
                     {
                        trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  "Failed to rename() %s to %s : %s",
                                  local_tmp_file, local_file, strerror(errno));
                     }
                     else
                     {
                        if ((db.fsa_pos != INCORRECT) &&
                            (fsa->debug > NORMAL_MODE))
                        {
                           trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                                        "Renamed local file %s to %s.",
                                        local_tmp_file, local_file);
                        }
                        rl[i].retrieved = YES;
#ifdef _OUTPUT_LOG
                        if (db.output_log == YES)
                        {                        
                           if (ol_fd == -2)
                           {
# ifdef WITHOUT_FIFO_RW_SUPPORT
                              output_log_fd(&ol_fd, &ol_readfd, &db.output_log);
# else                                                          
                              output_log_fd(&ol_fd, &db.output_log);
# endif
                           }
                           if ((ol_fd > -1) && (ol_data == NULL))
                           {
                              output_log_ptrs(&ol_retries,
                                              &ol_job_number,
                                              &ol_data,              /* Pointer to buffer.       */
                                              &ol_file_name,
                                              &ol_file_name_length,
                                              &ol_archive_name_length,
                                              &ol_file_size,
                                              &ol_unl,
                                              &ol_size,
                                              &ol_transfer_time,
                                              &ol_output_type,
                                              db.host_alias,
                                              (current_toggle - 1),
# ifdef WITH_SSL
                                              (db.auth == NO) ? HTTP : HTTPS,
# else
                                              HTTP,
# endif
                                              &db.output_log);
                           }
                           (void)strcpy(ol_file_name, rl[i].file_name);
                           *ol_file_name_length = (unsigned short)strlen(ol_file_name);
                           ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
                           ol_file_name[*ol_file_name_length + 1] = '\0';
                           (*ol_file_name_length)++;
                           *ol_file_size = bytes_done;
                           *ol_job_number = db.id.dir;
                           *ol_retries = db.retries;
                           *ol_unl = 0;
                           *ol_transfer_time = end_time - start_time;
                           *ol_archive_name_length = 0;
                           *ol_output_type = OT_NORMAL_RECEIVED + '0';
                           ol_real_size = *ol_file_name_length + ol_size;
                           if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
                           {
                              system_log(ERROR_SIGN, __FILE__, __LINE__,
                                         "write() error : %s", strerror(errno));
                           }
                        }
#endif /* _OUTPUT_LOG */
                     }
                  }
                  files_retrieved++;
                  file_size_retrieved += bytes_done;
               } /* if (rl[i].retrieved == NO) */

               if ((db.fra_pos == INCORRECT) || (db.fsa_pos == INCORRECT))
               {
                  /* We must stop here if fra_pos or fsa_pos is */
                  /* INCORRECT since we try to access these     */
                  /* structures (FRA/FSA)!                      */
                  trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Database changed, exiting.");
                  (void)http_quit();
                  reset_values(files_retrieved, file_size_retrieved,
                               files_to_retrieve, file_size_to_retrieve,
                               (struct job *)&db);
                  exit(TRANSFER_SUCCESS);
               }
            } /* for (i = 0; i < *no_of_listed_files; i++) */

            diff_no_of_files_done = fsa->job_status[(int)db.job_no].no_of_files_done -
                                    prev_no_of_files_done;
            if (diff_no_of_files_done > 0)
            {
               int     length = MAX_INT_LENGTH + 10 + MAX_OFF_T_LENGTH + 16 + MAX_INT_LENGTH + 8 + 1;
               u_off_t diff_file_size_done;
               char    buffer[MAX_INT_LENGTH + 10 + MAX_OFF_T_LENGTH + 16 + MAX_INT_LENGTH + 8 + 1];

               diff_file_size_done = fsa->job_status[(int)db.job_no].file_size_done -
                                     prev_file_size_done;                            
               WHAT_DONE_BUFFER(length, buffer, "retrieved",
                                diff_file_size_done, diff_no_of_files_done);
               trans_log(INFO_SIGN, NULL, 0, NULL, NULL, "%s @%x",
                         buffer, db.id.dir);
               prev_no_of_files_done = fsa->job_status[(int)db.job_no].no_of_files_done;
               prev_file_size_done = fsa->job_status[(int)db.job_no].file_size_done;
            }

            reset_values(files_retrieved, file_size_retrieved,
                         files_to_retrieve, file_size_to_retrieve,
                         (struct job *)&db);

            /* Free memory for the read buffer. */
            free(buffer);
            if (chunkbuffer != NULL)
            {
               free(chunkbuffer);
            }

#ifdef _WITH_BURST_2
            if (in_burst_loop == YES)
            {
               burst_2_counter++;
            }
#endif
         }
         else if (files_to_retrieve == 0)
              {
                 if (fsa->error_counter > 0)
                 {
                    int  fd, j;
#ifdef WITHOUT_FIFO_RW_SUPPORT
                    int  readfd;
#endif
                    char fd_wake_up_fifo[MAX_PATH_LENGTH];

#ifdef LOCK_DEBUG
                    lock_region_w(fsa_fd, db.lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
                    lock_region_w(fsa_fd, db.lock_offset + LOCK_EC);
#endif
                    fsa->error_counter = 0;

                    /* Wake up FD! */
                    (void)snprintf(fd_wake_up_fifo, MAX_PATH_LENGTH, "%s%s%s",
                                   p_work_dir, FIFO_DIR, FD_WAKE_UP_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
                    if (open_fifo_rw(fd_wake_up_fifo, &readfd, &fd) == -1)
#else
                    if ((fd = open(fd_wake_up_fifo, O_RDWR)) == -1)
#endif
                    {
                       system_log(WARN_SIGN, __FILE__, __LINE__,
                                  "Failed to open() FIFO `%s' : %s",
                                  fd_wake_up_fifo, strerror(errno));
                    }
                    else
                    {
                       char dummy;

                       if (write(fd, &dummy, 1) != 1)
                       {
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     "Failed to write() to FIFO `%s' : %s",
                                     fd_wake_up_fifo, strerror(errno));
                       }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                       if (close(readfd) == -1)
                       {
                          system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                     "Failed to close() FIFO `%s' (read) : %s",
                                     fd_wake_up_fifo, strerror(errno));
                       }
#endif
                       if (close(fd) == -1)
                       {
                          system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                     "Failed to close() FIFO `%s' : %s",
                                     fd_wake_up_fifo, strerror(errno));
                       }
                    }

                    /*
                     * Remove the error condition (NOT_WORKING) from all jobs
                     * of this host.
                     */
                    for (j = 0; j < fsa->allowed_transfers; j++)
                    {
                       if ((j != db.job_no) &&
                           (fsa->job_status[j].connect_status == NOT_WORKING))
                       {
                          fsa->job_status[j].connect_status = DISCONNECT;
                       }
                    }
                    fsa->error_history[0] = 0;
                    fsa->error_history[1] = 0;
#ifdef LOCK_DEBUG
                    unlock_region(fsa_fd, db.lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
                    unlock_region(fsa_fd, db.lock_offset + LOCK_EC);
#endif

#ifdef LOCK_DEBUG
                    lock_region_w(fsa_fd, db.lock_offset + LOCK_HS, __FILE__, __LINE__);
#else
                    lock_region_w(fsa_fd, db.lock_offset + LOCK_HS);
#endif
                    if (time(NULL) > fsa->end_event_handle)
                    {
                       fsa->host_status &= ~(EVENT_STATUS_FLAGS | AUTO_PAUSE_QUEUE_STAT);
                       if (fsa->end_event_handle > 0L)
                       {
                          fsa->end_event_handle = 0L;
                       }
                       if (fsa->start_event_handle > 0L)
                       {
                          fsa->start_event_handle = 0L;
                       }
                    }
                    else
                    {
                       fsa->host_status &= ~(EVENT_STATUS_STATIC_FLAGS | AUTO_PAUSE_QUEUE_STAT);
                    }
#ifdef LOCK_DEBUG
                    unlock_region(fsa_fd, db.lock_offset + LOCK_HS, __FILE__, __LINE__);
#else
                    unlock_region(fsa_fd, db.lock_offset + LOCK_HS);
#endif

                    /*
                     * Since we have successfully retrieved a file, no
                     * need to have the queue stopped anymore.
                     */
                    if (fsa->host_status & AUTO_PAUSE_QUEUE_STAT)
                    {
                       char sign[LOG_SIGN_LENGTH];

                       error_action(fsa->host_alias, "stop", HOST_ERROR_ACTION);
                       event_log(0L, EC_HOST, ET_EXT, EA_ERROR_END, "%s",
                                 fsa->host_alias);
                       if ((fsa->host_status & HOST_ERROR_OFFLINE_STATIC) ||
                           (fsa->host_status & HOST_ERROR_OFFLINE) ||
                           (fsa->host_status & HOST_ERROR_OFFLINE_T))
                       {
                          (void)memcpy(sign, OFFLINE_SIGN, LOG_SIGN_LENGTH);
                       }
                       else
                       {
                          (void)memcpy(sign, INFO_SIGN, LOG_SIGN_LENGTH);
                       }
                       trans_log(sign, __FILE__, __LINE__, NULL, NULL,
                                 "Starting input queue that was stopped by init_afd.");
                       event_log(0L, EC_HOST, ET_AUTO, EA_START_QUEUE, "%s",
                                 fsa->host_alias);
                    }
                 }

#ifdef WITH_ERROR_QUEUE
                 if (fsa->host_status & ERROR_QUEUE_SET)
                 {
                    remove_from_error_queue(db.id.dir, fsa, db.fsa_pos,
                                            fsa_fd);
                 }
#endif

                 (void)gsf_check_fra();
                 if (db.fra_pos == INCORRECT)
                 {
                    /* We must stop here if fra_pos is INCORRECT  */
                    /* since we try to access this structure FRA! */
                    trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
                              "Database changed, exiting.");
                    (void)http_quit();
                    reset_values(files_retrieved, file_size_retrieved,
                                 files_to_retrieve, file_size_to_retrieve,
                                 (struct job *)&db);
                    exit(TRANSFER_SUCCESS);
                 }
                 if (fra[db.fra_pos].error_counter > 0)
                 {
                    lock_region_w(fra_fd,
#ifdef LOCK_DEBUG
                                  (char *)&fra[db.fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                                  (char *)&fra[db.fra_pos].error_counter - (char *)fra);
#endif
                    fra[db.fra_pos].error_counter = 0;
                    if (fra[db.fra_pos].dir_flag & DIR_ERROR_SET)
                    {
                       fra[db.fra_pos].dir_flag &= ~DIR_ERROR_SET;
                       SET_DIR_STATUS(fra[db.fra_pos].dir_flag,
                                      time(NULL),
                                      fra[db.fra_pos].start_event_handle,
                                      fra[db.fra_pos].end_event_handle,
                                      fra[db.fra_pos].dir_status);
                       error_action(fra[db.fra_pos].dir_alias, "stop",
                                    DIR_ERROR_ACTION);
                       event_log(0L, EC_DIR, ET_EXT, EA_ERROR_END, "%s",
                                 fra[db.fra_pos].dir_alias);
                    }
                    unlock_region(fra_fd,
#ifdef LOCK_DEBUG
                                  (char *)&fra[db.fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                                  (char *)&fra[db.fra_pos].error_counter - (char *)fra);
#endif
                 }
              }

         loop_counter++;
      } while (((*(unsigned char *)((char *)p_no_of_hosts + AFD_FEATURE_FLAG_OFFSET_START) & DISABLE_RETRIEVE) == 0) &&
               (((fsa->protocol_options & DISABLE_BURSTING) == 0) ||
                (loop_counter == 1)) &&
               (more_files_in_list == YES));

#ifdef _WITH_BURST_2
      in_burst_loop = YES;
      diff_time = time(NULL) - connected;
      if (((fsa->protocol_options & KEEP_CONNECTED_DISCONNECT) &&
           (db.keep_connected > 0) && (diff_time > db.keep_connected)) ||
          ((db.disconnect > 0) && (diff_time > db.disconnect)))
      {
         cb2_ret = NO;
         break;
      }
   } while (((db.special_flag & DISTRIBUTED_HELPER_JOB) == 0) &&
            ((*(unsigned char *)((char *)p_no_of_hosts + AFD_FEATURE_FLAG_OFFSET_START) & DISABLE_RETRIEVE) == 0) &&
            (((cb2_ret = check_burst_gf(&values_changed)) == YES) ||
             (cb2_ret == RESCAN_SOURCE)));

   if (cb2_ret == NEITHER)
   {
      exit_status = STILL_FILES_TO_SEND;
   }
#endif /* _WITH_BURST_2 */

   if (db.fsa_pos != INCORRECT)
   {
      fsa->job_status[(int)db.job_no].connect_status = CLOSING_CONNECTION;
   }
   http_quit();
   if ((db.fsa_pos != INCORRECT) && (fsa->debug > NORMAL_MODE))
   {
      trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL, "Logged out.");
   }

   exitflag = 0;
   exit(exit_status);
}


/*++++++++++++++++++++++++++++ gf_http_exit() +++++++++++++++++++++++++++*/
static void
gf_http_exit(void)
{
   if ((fra != NULL) && (db.fra_pos >= 0))
   {
      if ((fra[db.fra_pos].stupid_mode == YES) ||
          (fra[db.fra_pos].remove == YES))
      {
         detach_ls_data(YES);
      }
      else
      {
         detach_ls_data(NO);
      }
   }

   if ((fsa != NULL) && (db.fsa_pos >= 0))
   {
      int     diff_no_of_files_done;
      u_off_t diff_file_size_done;

      diff_no_of_files_done = fsa->job_status[(int)db.job_no].no_of_files_done -
                              prev_no_of_files_done;
      diff_file_size_done = fsa->job_status[(int)db.job_no].file_size_done -
                            prev_file_size_done;
      if ((diff_file_size_done > 0) || (diff_no_of_files_done > 0))
      {
         int  length;
#ifdef _WITH_BURST_2
         char buffer[MAX_INT_LENGTH + 10 + MAX_OFF_T_LENGTH + 16 + MAX_INT_LENGTH + 8 + 11 + MAX_INT_LENGTH + 1];

         length = MAX_INT_LENGTH + 10 + MAX_OFF_T_LENGTH + 16 + MAX_INT_LENGTH + 8 + 11 + MAX_INT_LENGTH + 1;
#else
         char buffer[MAX_INT_LENGTH + 10 + MAX_OFF_T_LENGTH + 16 + MAX_INT_LENGTH + 8 + 1];

         length = MAX_INT_LENGTH + 10 + MAX_OFF_T_LENGTH + 16 + MAX_INT_LENGTH + 8 + 1;
#endif

         WHAT_DONE_BUFFER(length, buffer, "retrieved", diff_file_size_done,
                          diff_no_of_files_done);
#ifdef _WITH_BURST_2
         if (burst_2_counter == 1)
         {
            /* Write " [BURST]" */
            buffer[length] = ' '; buffer[length + 1] = '[';
            buffer[length + 2] = 'B'; buffer[length + 3] = 'U';
            buffer[length + 4] = 'R'; buffer[length + 5] = 'S';
            buffer[length + 6] = 'T'; buffer[length + 7] = ']';
            buffer[length + 8] = '\0';
            length += 8;
         }
         else if (burst_2_counter > 1)
              {
                 length += snprintf(&buffer[length],
                                    MAX_INT_LENGTH + 10 + MAX_OFF_T_LENGTH + 16 + MAX_INT_LENGTH + 8 + 11 + MAX_INT_LENGTH + 1 - length,
                                    " [BURST * %u]", burst_2_counter);
              }
#endif
         trans_log(INFO_SIGN, NULL, 0, NULL, NULL, "%s @%x",
                   buffer, db.id.dir);
      }
      reset_fsa((struct job *)&db, exitflag, files_to_retrieve_shown,
                file_size_to_retrieve_shown);
   }

   send_proc_fin(NO);
   if (sys_log_fd != STDERR_FILENO)
   {
      (void)close(sys_log_fd);
   }

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   reset_fsa((struct job *)&db, IS_FAULTY_VAR, files_to_retrieve_shown,
             file_size_to_retrieve_shown);
   system_log(DEBUG_SIGN, __FILE__, __LINE__,
              "Aaarrrggh! Received SIGSEGV. Remove the programmer who wrote this!");
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   reset_fsa((struct job *)&db, IS_FAULTY_VAR, files_to_retrieve_shown,
             file_size_to_retrieve_shown);
   system_log(DEBUG_SIGN, __FILE__, __LINE__, "Uuurrrggh! Received SIGBUS.");
   abort();
}


/*++++++++++++++++++++++++++++++ sig_kill() +++++++++++++++++++++++++++++*/
static void
sig_kill(int signo)
{
   exitflag = 0;
   exit(GOT_KILLED);
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   exit(INCORRECT);
}
