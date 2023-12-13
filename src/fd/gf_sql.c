/*
 *  gf_sql.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2011 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   gf_sql - gets data via SQL statements
 **
 ** SYNOPSIS
 **   gf_sql <work dir> <job no.> <FSA id> <FSA pos> <dir alias> [options]
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
 **   18.01.2011 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                     /* fprintf(), snprintf()          */
#include <string.h>                    /* strcpy(), strcat(), strcmp(),  */
                                       /* strerror()                     */
#include <stdlib.h>                    /* malloc(), free(), abort()      */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>                    /* signal()                       */
#include <unistd.h>                    /* close(), getpid()              */
#include <errno.h>
#include "sqldefs.h"
#include "fddefs.h"
#include "version.h"

int                        *current_no_of_listed_files = NULL,
                           event_log_fd = STDERR_FILENO,
                           exitflag = IS_FAULTY_VAR,
                           files_to_retrieve_shown = 0,
                           fra_fd = -1,
                           fra_id,
                           fsa_fd = -1,
                           fsa_id,
                           fsa_pos_save = NO,
#ifdef HAVE_HW_CRC32
                           have_hw_crc32 = NO,
#endif
#ifdef _MAINTAINER_LOG
                           maintainer_log_fd = STDERR_FILENO,
#endif
                           no_of_dirs = 0,
                           no_of_hosts = 0,
                           *p_no_of_dirs = NULL,
                           *p_no_of_hosts = NULL,
                           no_of_listed_files,
                           rl_fd = -1,
                           trans_db_log_fd = STDERR_FILENO,
                           transfer_log_fd = STDERR_FILENO,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                           trans_db_log_readfd,
                           transfer_log_readfd,
#endif
                           sys_log_fd = STDERR_FILENO,
                           timeout_flag;
off_t                      file_size_to_retrieve_shown = 0,
                           rl_size = 0;
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
struct filetransfer_status *fsa;
struct fileretrieve_status *fra;
struct job                 db;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static int                 sql_timeup(void);
static void                gf_sql_exit(void),
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
                    fd,
                    files_retrieved = 0,
                    files_to_retrieve = 0,
                    i,
                    local_file_length,
                    more_files_in_list,
                    status;
   off_t            bytes_done,
                    content_length,
                    file_size_retrieved = 0,
                    file_size_to_retrieve = 0,
                    tmp_content_length;
   time_t           end_transfer_time_file,
                    start_transfer_time_file;
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
   if (atexit(gf_sql_exit) != 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not register exit function : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Initialise variables. */
   init_gf(argc, argv, SQL_FLAG);
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
       (signal(SIGTERM, sig_kill) == SIG_ERR) ||
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
      }
      else
      {
         (void)strcpy(db.hostname, fsa->real_hostname[HOST_ONE - 1]);
      }
   }
   else
   {
      (void)strcpy(db.hostname,
                   fsa->real_hostname[(int)(fsa->host_toggle - 1)]);
   }

   if (fsa->debug > NORMAL_MODE)
   {
      trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                   "Trying to connect to %s at port %d.",
                   db.hostname, db.port);
   }

   /* Connect to remote SQL-server. */
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
   if ((status = sql_connect(db.hostname, db.port,
#ifdef WITH_SSL
                             db.user, db.password, db.tls_auth, db.sndbuf_size, db.rcvbuf_size)) != SUCCESS)
#else
                             db.user, db.password, db.sndbuf_size, db.rcvbuf_size)) != SUCCESS)
#endif
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                "SQL connection to %s at port %d failed (%d).",
                db.hostname, db.port, status);
      exit(CONNECT_ERROR);
   }
   else
   {
      if (fsa->debug > NORMAL_MODE)
      {
#ifdef WITH_SSL
         char *p_msg_str;

         if ((db.tls_auth == YES) || (db.tls_auth == BOTH))
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

   fsa->job_status[(int)db.job_no].connect_status = SQL_RETRIEVE_ACTIVE;
   if (db.special_flag & DISTRIBUTED_HELPER_JOB)
   {
      /*
       * If we are a helper job, lets NOT stay connected and do a
       * full directory scan.
       */
      db.keep_connected = 0;
   }
   more_files_in_list = NO;
   do
   {
      read_sql_statements();
      send_sql_statements();
      get_sql_data();
      if ((files_to_retrieve = get_remote_file_names_http(&file_size_to_retrieve,
                                                          &more_files_in_list)) > 0)
      {
         /* Inform FSA that we have finished connecting and */
         /* will now start to retrieve data.                */
         files_to_retrieve = 1;
         file_size_to_retrieve = 0;
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

         (void)gsf_check_fra((struct job *)&db);
         if ((db.fra_pos == INCORRECT) || (db.fsa_pos == INCORRECT))
         {
            /*
             * Looks as if this source/destination is no longer in our database.
             */
            (void)sql_quit();
            reset_values(files_retrieved, file_size_retrieved,
                         files_to_retrieve, file_size_to_retrieve,
                         (struct job *)&db);
            return(SUCCESS);
         }

         /* Get directory where files are to be stored and */
         /* prepare some pointers for the file names.      */
         if (create_remote_dir(fra->url, fra->retrieve_work_dir, NULL, NULL,
                               NULL, local_file,
                               &local_file_length) == INCORRECT)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to determine local incoming directory for <%s>.",
                       fra->dir_alias);
            sql_quit();
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
            sql_quit();
            exit(ALLOC_ERROR);
         }

                  if (fsa->debug > NORMAL_MODE)
                  {
                     trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                                  "Opened HTTP connection for file %s.",
                                  rl[i].file_name);
                  }

                  if (offset > 0)
                  {
                     if (content_length > 0)
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
                        fd = open(local_tmp_file, O_WRONLY | O_CREAT |
                                  O_LARGEFILE, FILE_MODE);
#else
                        fd = open(local_tmp_file, O_WRONLY | O_CREAT, FILE_MODE);
#endif
                     }
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
                     sql_quit();
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
                                     "Opened local file %s.", local_tmp_file);
                     }
                  }

                  if (gsf_check_fsa((struct job *)&db) != NEITHER)
                  {
                     fsa->job_status[(int)db.job_no].file_size_in_use = 0;
                     (void)strcpy(fsa->job_status[(int)db.job_no].file_name_in_use,
                                  local_tmp_file);
                  }
                  else if (db.fsa_pos == INCORRECT)
                       {
                          /*
                           * Looks as if this host is no longer in our database.
                           */
                          (void)sql_quit();
                          reset_values(files_retrieved, file_size_retrieved,
                                       files_to_retrieve, file_size_to_retrieve,
                                       (struct job *)&db);
                          return(SUCCESS);
                       }

                  bytes_done = 0;
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
                     do
                     {
                        if ((status = sql_read(buffer, blocksize)) == INCORRECT)
                        {
                           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                                     "Failed to read from remote database");
                           reset_values(files_retrieved, file_size_retrieved,
                                        files_to_retrieve, file_size_to_retrieve,
                                        (struct job *)&db);
                           sql_quit();
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
                                        local_tmp_file, strerror(errno));
                              sql_quit();
                              reset_values(files_retrieved, file_size_retrieved,
                                           files_to_retrieve,
                                           file_size_to_retrieve,
                                           (struct job *)&db);
                              exit(WRITE_LOCAL_ERROR);
                           }
                           bytes_done += status;
                        }

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
                                              "Transfer timeout reached for `%s' after %ld seconds.",
#else
                                              "Transfer timeout reached for `%s' after %lld seconds.",
#endif
                                              fsa->job_status[(int)db.job_no].file_name_in_use,
                                              (pri_time_t)(end_transfer_time_file - start_transfer_time_file));
                                    sql_quit();
                                    exit(STILL_FILES_TO_SEND);
                                 }
                              }
                           }
                        }
                        else if (db.fsa_pos == INCORRECT)
                             {
                                /*
                                 * Looks as if this host is no longer in our
                                 * database.
                                 */
                                (void)sql_quit();
                                reset_values(files_retrieved,
                                             file_size_retrieved,
                                             files_to_retrieve,
                                             file_size_to_retrieve,
                                             (struct job *)&db);
                                return(SUCCESS);
                             }
                     } while ((status != 0) && (bytes_done < content_length));
                  }

                  /* Close the local file. */
                  if (close(fd) == -1)
                  {
                     trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Failed to close() local file %s.", local_tmp_file);
                  }
                  else
                  {
                     if (fsa->debug > NORMAL_MODE)
                     {
                        trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                                     "Closed local file %s.", local_tmp_file);
                     }
                  }

                  /* Check if remote data is to be deleted. */
                  if (fra->remove == YES)
                  {
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
                     }
#endif

                     if ((rl[i].size != content_length) && (content_length > 0))
                     {
                        fsa->total_file_size += (content_length - rl[i].size);
                        file_size_to_retrieve_shown += (content_length - rl[i].size);
                        fsa->job_status[(int)db.job_no].file_size += (content_length - rl[i].size);
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

                     (void)gsf_check_fra((struct job *)&db);
                     if (db.fra_pos != INCORRECT)
                     {
                        if (fra->error_counter > 0)
                        {
#ifdef LOCK_DEBUG
                           lock_region_w(fra_fd, db.fra_lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
                           lock_region_w(fra_fd, db.fra_lock_offset + LOCK_EC);
#endif
                           fra->error_counter = 0;
                           if (fra->dir_flag & DIR_ERROR_SET)
                           {
                              int  receive_log_fd = -1;
#ifdef WITHOUT_FIFO_RW_SUPPORT
                              int  receive_log_readfd;
#endif
                              char receive_log_fifo[MAX_PATH_LENGTH];

                              (void)strcpy(receive_log_fifo, p_work_dir);
                              (void)strcat(receive_log_fifo, FIFO_DIR);
                              (void)strcat(receive_log_fifo, RECEIVE_LOG_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
                              if (open_fifo_rw(receive_log_fifo, &receive_log_readfd,
                                               &receive_log_fd) == -1)
#else
                              if ((receive_log_fd = open(receive_log_fifo, O_RDWR)) == -1)
#endif
                              {
                                 if (errno == ENOENT)
                                 {
                                    if ((make_fifo(receive_log_fifo) == SUCCESS) &&
#ifdef WITHOUT_FIFO_RW_SUPPORT
                                        (open_fifo_rw(receive_log_fifo,
                                                      &receive_log_readfd,
                                                      &receive_log_fd) == -1))
#else
                                        ((receive_log_fd = open(receive_log_fifo,
                                                                O_RDWR)) == -1))
#endif
                                    {
                                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                  "Could not open fifo <%s> : %s",
                                                  RECEIVE_LOG_FIFO, strerror(errno));
                                    }
                                 }
                                 else
                                 {
                                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                                               "Could not open fifo %s : %s",
                                               RECEIVE_LOG_FIFO, strerror(errno));
                                 }
                              }

                              fra->dir_flag &= ~DIR_ERROR_SET;
                              SET_DIR_STATUS(fra->dir_flag, time(NULL),
                                             fra->start_event_handle,
                                             fra->end_event_handle,
                                             fra->dir_status);
                              error_action(fra->dir_alias, "stop",
                                           DIR_ERROR_ACTION,
                                           receive_log_fd);
                              event_log(0L, EC_DIR, ET_EXT, EA_ERROR_END, "%s",
                                        fra->dir_alias);
                              (void)close(receive_log_fd);
#ifdef WITHOUT_FIFO_RW_SUPPORT
                              (void)close(receive_log_readfd);
#endif
                           }
#ifdef LOCK_DEBUG
                           unlock_region(fra_fd, db.fra_lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
                           unlock_region(fra_fd, db.fra_lock_offset + LOCK_EC);
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
                        fsa->host_status &= ~AUTO_PAUSE_QUEUE_STAT;
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
                         * Since we have successfully retrieved data, no
                         * need to have the queue stopped anymore.
                         */
                        if (fsa->host_status & AUTO_PAUSE_QUEUE_STAT)
                        {
                           char sign[LOG_SIGN_LENGTH];

                           error_action(fsa->host_alias, "stop",
                                        HOST_ERROR_ACTION,
                                        transfer_log_fd);
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
                                     HOST_SUCCESS_ACTION, transfer_log_fd);
                     }
                  }

                  /*
                   * If the file size is not the same as the one when we did
                   * the remote ls command, give a warning in the transfer log
                   * so some action can be taken against the originator.
                   */
                  if ((content_length > 0) &&
                      ((bytes_done + offset) != content_length))
                  {
                     trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
#if SIZEOF_OFF_T == 4
                               "File size of file %s changed from %ld to %ld when it was retrieved.",
#else
                               "File size of file %s changed from %lld to %lld when it was retrieved.",
#endif
                               rl[i].file_name, (pri_off_t)content_length,
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
                     rl[i].assigned = 0;
                  }

         files_retrieved++;
         file_size_retrieved += bytes_done;

         reset_values(files_retrieved, file_size_retrieved,
                      files_to_retrieve, file_size_to_retrieve,
                      (struct job *)&db);

         /* Free memory for the read buffer. */
         free(buffer);
         if (chunkbuffer != NULL)
         {
            free(chunkbuffer);
         }
      }
      else if ((files_to_retrieve == 0) && (fsa->error_counter > 0))
           {
              unset_error_counter_fsa(fsa_fd, transfer_log_fd, p_work_dir,
                                      fsa, (struct job *)&db);
           }
   } while (((*(unsigned char *)((char *)p_no_of_hosts + AFD_FEATURE_FLAG_OFFSET_START) & DISABLE_RETRIEVE) == 0) &&
            ((more_files_in_list == YES) ||
             ((db.keep_connected > 0) && (sql_timeup() == SUCCESS))));

   if ((fsa != NULL) && (db.fsa_pos >= 0) && (fsa_pos_save == YES))
   {
      fsa->job_status[(int)db.job_no].connect_status = CLOSING_CONNECTION;
   }
   sql_quit();
   if ((db.fsa_pos != INCORRECT) && (fsa->debug > NORMAL_MODE))
   {
      trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL, "Logged out.");
   }

   exitflag = 0;
   exit(TRANSFER_SUCCESS);
}


/*++++++++++++++++++++++++++++ gf_sql_exit() ++++++++++++++++++++++++++++*/
static void
gf_sql_exit(void)
{
   if ((fsa != NULL) && (db.fsa_pos >= 0) && (fsa_pos_save == YES))
   {
      WHAT_DONE("retrieved", fsa->job_status[(int)db.job_no].file_size_done,
                fsa->job_status[(int)db.job_no].no_of_files_done);
      reset_fsa((struct job *)&db, exitflag, files_to_retrieve_shown,
                file_size_to_retrieve_shown);
      fsa_detach_pos(db.fsa_pos);
   }
   if ((fra != NULL) && (db.fra_pos >= 0) && (p_no_of_dirs != NULL))
   {
      fra_detach_pos(db.fra_pos);
   }

   send_proc_fin(NO);
   if (sys_log_fd != STDERR_FILENO)
   {
      (void)close(sys_log_fd);
   }

   return;
}


/*++++++++++++++++++++++++++++ sql_timeup() +++++++++++++++++++++++++++++*/
static int
sql_timeup(void)
{
   time_t now,
          timeup;

   (void)gsf_check_fra((struct job *)&db);
   if (db.fra_pos == INCORRECT)                 
   {
      return(INCORRECT);
   }
   if (fra->keep_connected > 0)
   {
      db.keep_connected = fra->keep_connected;
   }
   else if ((fsa->keep_connected > 0) &&
            ((fsa->special_flag & KEEP_CON_NO_FETCH) == 0))
        {
           db.keep_connected = fsa->keep_connected;
        }
        else
        {
           db.keep_connected = 0;
           return(INCORRECT);
        }
   now = time(NULL);
   timeup = now + db.keep_connected;
   if (db.no_of_time_entries == 0)
   {
      fra->next_check_time = now + db.remote_file_check_interval;
   }
   else
   {
      fra->next_check_time = calc_next_time_array(db.no_of_time_entries,
                                                  db.te,
#ifdef WITH_TIMEZONE
                                                  db.timezone,
#endif
                                                  now,
                                                  __FILE__, __LINE__);
   }
   if (fra->next_check_time > timeup)
   {
      return(INCORRECT);
   }
   else
   {
      if (fra->next_check_time < now)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_TIME_T == 4
                    "BUG in calc_next_time(): next_check_time (%ld) < now (%ld)",
#else
                    "BUG in calc_next_time(): next_check_time (%lld) < now (%lld)",
#endif
                    (pri_time_t)fra->next_check_time,
                    (pri_time_t)now);
         return(INCORRECT);
      }
      else
      {
         timeup = fra->next_check_time;
      }
   }
   if (gsf_check_fsa((struct job *)&db) != NEITHER)
   {
      time_t sleeptime = 0;

      if (fsa->protocol_options & STAT_KEEPALIVE)
      {
         sleeptime = fsa->transfer_timeout - 5;
      }
      if (sleeptime < 1)
      {
         sleeptime = DEFAULT_NOOP_INTERVAL;
      }
      if ((now + sleeptime) > timeup)
      {
         sleeptime = timeup - now;
      }
      fsa->job_status[(int)db.job_no].unique_name[2] = 5;
      do
      {
         (void)sleep(sleeptime);
         (void)gsf_check_fra((struct job *)&db);
         if ((db.fra_pos == INCORRECT) || (db.fsa_pos == INCORRECT))
         {
            return(INCORRECT);
         }
         if (gsf_check_fsa((struct job *)&db) == NEITHER)
         {
            if (db.fsa_pos == INCORRECT)
            {
               return(INCORRECT);
            }
            break;
         }
         if (fsa->job_status[(int)db.job_no].unique_name[2] == 6)
         {
            fsa->job_status[(int)db.job_no].unique_name[2] = '\0';
            return(INCORRECT);
         }
         now = time(NULL);
         if ((now + sleeptime) > timeup)
         {
            sleeptime = timeup - now;
         }
      } while (timeup > now);
   }

   return(SUCCESS);
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
