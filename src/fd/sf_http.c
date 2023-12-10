/*
 *  sf_http.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   sf_http - send files via HTTP
 **
 ** SYNOPSIS
 **   sf_http <work dir> <job no.> <FSA id> <FSA pos> <msg name> [options]
 **
 **   options
 **       --version        Version
 **       -a <age limit>   The age limit for the files being send.
 **       -A               Disable archiving of files.
 **       -o <retries>     Old/Error message and number of retries.
 **       -r               Resend from archive (job from show_olog).
 **       -t               Temp toggle.
 **
 ** DESCRIPTION
 **   sf_http sends the given files to the defined recipient via HTTP.
 **
 **   In the message file will be the data it needs about the
 **   remote host in the following format:
 **       [destination]
 **       <scheme>://<user>:<password>@<host>:<port>/<url-path>
 **
 **       [options]
 **       <a list of FD options, terminated by a newline>
 **
 **   If the archive flag is set, each file will be archived after it
 **   has been send successful.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has
 **   occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.12.2005 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                     /* fprintf(), snprintf()          */
#include <string.h>                    /* strcpy(), strcat(), strcmp(),  */
                                       /* strerror()                     */
#include <stdlib.h>                    /* malloc(), free(), abort()      */
#include <ctype.h>                     /* isdigit(), isalpha()           */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _OUTPUT_LOG
# include <sys/times.h>                /* times(), struct tms            */
#endif
#include <fcntl.h>
#include <signal.h>                    /* signal()                       */
#include <unistd.h>                    /* unlink(), close()              */
#include <errno.h>
#include "fddefs.h"
#include "httpdefs.h"
#include "version.h"

/* Global variables. */
unsigned int               special_flag = 0;
int                        counter_fd = -1,     /* NOT USED */
                           event_log_fd = STDERR_FILENO,
                           exitflag = IS_FAULTY_VAR,
                           files_to_delete,     /* NOT USED */
#ifdef HAVE_HW_CRC32
                           have_hw_crc32 = NO,
#endif
#ifdef _MAINTAINER_LOG
                           maintainer_log_fd = STDERR_FILENO,
#endif
                           no_of_hosts,    /* This variable is not used */
                                           /* in this module.           */
                           *p_no_of_hosts = NULL,
                           fsa_fd = -1,
                           fsa_id,
                           fsa_pos_save = NO,
                           prev_no_of_files_done = 0,
                           simulation_mode = NO,
                           sys_log_fd = STDERR_FILENO,
                           transfer_log_fd = STDERR_FILENO,
                           trans_db_log_fd = STDERR_FILENO,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                           trans_db_log_readfd,
                           transfer_log_readfd,
#endif
                           trans_rename_blocked = NO,
                           timeout_flag,
                           *unique_counter;
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
#ifdef _WITH_BURST_2
unsigned int               burst_2_counter = 0;
#endif
#ifdef HAVE_MMAP
off_t                      fsa_size;
#endif
off_t                      *file_size_buffer = NULL;
time_t                     *file_mtime_buffer = NULL;
u_off_t                    prev_file_size_done = 0;
long                       transfer_timeout;
char                       *p_work_dir = NULL,
                           tr_hostname[MAX_HOSTNAME_LENGTH + 2],
                           line_buffer[MAX_RET_MSG_LENGTH],
                           msg_str[MAX_RET_MSG_LENGTH],
                           *del_file_name_buffer = NULL, /* NOT USED */
                           *file_name_buffer = NULL;
struct filetransfer_status *fsa = NULL;
struct job                 db;
struct rule                *rule;
#ifdef _DELETE_LOG
struct delete_log          dl;
#endif
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local global variables. */
static int                 files_send,
                           files_to_send,
                           local_file_counter;
static off_t               local_file_size,
                           *p_file_size_buffer;

/* Local function prototypes. */
static void                sf_http_exit(void),
                           sig_bus(int),
                           sig_segv(int),
                           sig_kill(int),
                           sig_exit(int);

/* #define _SIMULATE_SLOW_TRANSFER 2L */


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
#ifdef _WITH_BURST_2
   int              cb2_ret;
#endif
   int              current_toggle,
                    end_length,
                    exit_status = TRANSFER_SUCCESS,
                    fd,
                    features,
                    header_length,
                    j,
                    length_type_indicator,
                    loops,
                    status,
                    rest,
                    blocksize,
                    *wmo_counter,
                    wmo_counter_fd = -1;
   unsigned int     values_changed = 0;
#ifdef WITH_ARCHIVE_COPY_INFO
   unsigned int     archived_copied = 0;
#endif
   off_t            file_size,
                    no_of_bytes;
#ifdef _OUTPUT_LOG
   clock_t          end_time = 0,
                    start_time = 0;
   struct tms       tmsdummy;
#endif
   clock_t          clktck;
   time_t           connected,
#ifdef _WITH_BURST_2
                    diff_time,
#endif
                    end_transfer_time_file,
                    *p_file_mtime_buffer,
                    start_transfer_time_file = 0,
                    last_update_time,
                    now;
   char             *p_file_name_buffer,
                    *p_remote_filename,
                    *buffer,
                    fullname[MAX_PATH_LENGTH + 1],
                    file_path[MAX_PATH_LENGTH],
                    remote_filename[MAX_RECIPIENT_LENGTH + MAX_FILENAME_LENGTH];
#ifdef HAVE_STATX
   struct statx     stat_buf;
#else
   struct stat      stat_buf;
#endif
   struct job       *p_db;
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
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "sigaction() error : %s", strerror(errno));
      exit(INCORRECT);
   }
#endif

   /* Do some cleanups when we exit. */
   if (atexit(sf_http_exit) != 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not register exit function : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Initialise variables. */
   local_file_counter = 0;
   files_to_send = init_sf(argc, argv, file_path, HTTP_FLAG);
   p_db = &db;
   if ((clktck = sysconf(_SC_CLK_TCK)) <= 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not get clock ticks per second : %s",
                 strerror(errno));
      exit(INCORRECT);
   }
   if (fsa->trl_per_process > 0)
   {
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
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "signal() error : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Now determine the real hostname. */
   if (fsa->real_hostname[1][0] == '\0')
   {
      (void)strcpy(db.hostname, fsa->real_hostname[0]);
      current_toggle = HOST_ONE;
   }
   else
   {
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
         (void)strcpy(db.hostname, fsa->real_hostname[(current_toggle - 1)]);
      }
      if (((db.special_flag & TRANS_RENAME_PRIMARY_ONLY) &&
           (current_toggle == HOST_TWO)) ||
          ((db.special_flag & TRANS_RENAME_SECONDARY_ONLY) &&
           (current_toggle == HOST_ONE)))
      {
         trans_rename_blocked = YES;
         db.trans_rename_rule[0] = '\0';
      }
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
   features = 0;
#ifdef WITH_SSL
   if (fsa->protocol_options & TLS_STRICT_VERIFY)
   {
      features |= PROT_OPT_TLS_STRICT_VERIFY;
   }
   if (fsa->protocol_options & TLS_LEGACY_RENEGOTIATION)
   {
      features |= PROT_OPT_TLS_LEGACY_RENEGOTIATION;
   }
#endif
   if (fsa->protocol_options & NO_EXPECT)
   {
      features |= PROT_OPT_NO_EXPECT;
   }
   status = http_connect(db.hostname, db.http_proxy, db.port, db.user,
                         db.password, db.auth, 0, features,
#ifdef WITH_SSL
                         db.ssh_protocol, db.service, db.region, db.tls_auth,
#endif
                         db.sndbuf_size, db.rcvbuf_size, fsa->debug);
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
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                   "HTTP connection to %s at port %d failed (%d).",
                   db.hostname, db.port, status);
      }
      else
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                   "HTTP connection to HTTP proxy %s at port %d failed (%d).",
                   db.http_proxy, db.port, status);
      }
      exit(eval_timeout(CONNECT_ERROR));
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
   connected = time(NULL);

   /* Inform FSA that we have finished connecting and */
   /* will now start to transfer data.                */
   if (gsf_check_fsa(p_db) != NEITHER)
   {
#ifdef LOCK_DEBUG
      lock_region_w(fsa_fd, db.lock_offset + LOCK_CON, __FILE__, __LINE__);
#else
      lock_region_w(fsa_fd, db.lock_offset + LOCK_CON);
#endif
      fsa->job_status[(int)db.job_no].connect_status = HTTP_ACTIVE;
      fsa->job_status[(int)db.job_no].no_of_files = files_to_send;
      fsa->connections += 1;
#ifdef LOCK_DEBUG
      unlock_region(fsa_fd, db.lock_offset + LOCK_CON, __FILE__, __LINE__);
#else
      unlock_region(fsa_fd, db.lock_offset + LOCK_CON);
#endif
   }

   /* Allocate buffer to read data from the source file. */
   if ((buffer = malloc(blocksize + 1 + 4 /* For bulletin end. */)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      exit(ALLOC_ERROR);
   }

   if (db.special_flag & WITH_SEQUENCE_NUMBER)
   {
      char counter_file_name[MAX_FILENAME_LENGTH];

      (void)snprintf(counter_file_name, MAX_FILENAME_LENGTH,
                     "/%s.%d", db.host_alias, db.port);
      if ((wmo_counter_fd = open_counter_file(counter_file_name,
                                              &wmo_counter)) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to open counter file `%s'.", counter_file_name);
      }
   }

#ifdef _WITH_BURST_2
   do
   {
      if (burst_2_counter > 0)
      {
         if (fsa->debug > NORMAL_MODE)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
# ifdef WITH_SSL
                         "%s Bursting. [values_changed=%u]", (db.tls_auth == NO) ? "HTTP" : "HTTPS",
# else
                         "HTTP Bursting. [values_changed=%u]",
# endif
                         values_changed);
         }
         if (values_changed & USER_CHANGED)
         {
            http_reset_authentication(db.auth);
         }
      }

#endif

      /* Send all files. */
      p_file_name_buffer = file_name_buffer;
      p_file_size_buffer = file_size_buffer;
      p_file_mtime_buffer = file_mtime_buffer;
      last_update_time = time(NULL);
      local_file_size = 0;
      for (files_send = 0; files_send < files_to_send; files_send++)
      {
         (void)snprintf(fullname, MAX_PATH_LENGTH + 1,
                        "%s/%s", file_path, p_file_name_buffer);

#ifdef WITH_DUP_CHECK
# ifndef FAST_SF_DUPCHECK
         if ((db.dup_check_timeout > 0) &&
             (isdup(fullname, p_file_name_buffer, *p_file_size_buffer,
                    db.crc_id, db.dup_check_timeout, db.dup_check_flag, NO,
#  ifdef HAVE_HW_CRC32
                    have_hw_crc32,
#  endif
                    YES, YES) == YES))
         {
            time_t       file_mtime;
#  ifdef HAVE_STATX
            struct statx stat_buf;
#  else
            struct stat  stat_buf;
#  endif

            now = time(NULL);
            if (file_mtime_buffer == NULL)
            {
#  ifdef HAVE_STATX
               if (statx(0, fullname, AT_STATX_SYNC_AS_STAT,
                         STATX_MTIME, &stat_buf) == -1)
#  else
               if (stat(fullname, &stat_buf) == -1)
#  endif
               {
                  file_mtime = now;
               }
               else
               {
#  ifdef HAVE_STATX
                  file_mtime = stat_buf.stx_mtime.tv_sec;
#  else
                  file_mtime = stat_buf.st_mtime;
#  endif
               }
            }
            else
            {
               file_mtime = *p_file_mtime_buffer;
            }
            handle_dupcheck_delete(SEND_FILE_HTTP, fsa->host_alias, fullname,
                                   p_file_name_buffer, *p_file_size_buffer,
                                   file_mtime, now);
            if (db.dup_check_flag & DC_DELETE)
            {
               local_file_size += *p_file_size_buffer;
               local_file_counter += 1;
               if (now >= (last_update_time + LOCK_INTERVAL_TIME))
               {
                  last_update_time = now;
                  update_tfc(local_file_counter, local_file_size,
                             p_file_size_buffer, files_to_send,
                             files_send, now);
                  local_file_size = 0;
                  local_file_counter = 0;
               }
            }
         }
         else
         {
# endif
#endif
         if (gsf_check_fsa(p_db) != NEITHER)
         {
            fsa->job_status[(int)db.job_no].file_size_in_use = *p_file_size_buffer;
            (void)strcpy(fsa->job_status[(int)db.job_no].file_name_in_use,
                         p_file_name_buffer);
         }

         if (db.special_flag & FILE_NAME_IS_HEADER)
         {
            int  header_length = 0,
                 space_count = 0;
            char *ptr = p_file_name_buffer;

            for (;;)
            {
               while ((*ptr != '_') && (*ptr != '-') && (*ptr != ' ') &&
                      (*ptr != '\0') && (*ptr != '.') && (*ptr != ';'))
               {
                  header_length++; ptr++;
               }
               if ((*ptr == '\0') || (*ptr == '.') || (*ptr == ';'))
               {
                  break;
               }
               else
               {
                  if (space_count == 2)
                  {
                     if ((isalpha((int)(*(ptr + 1)))) &&
                         (isalpha((int)(*(ptr + 2)))) &&
                         (isalpha((int)(*(ptr + 3)))))  
                     {
                        header_length += 4;
                     }
                     break;
                  }
                  else
                  {
                     header_length++; ptr++; space_count++;
                  }
               }
            } /* for (;;) */

            if (wmo_counter_fd > 0)
            {
               file_size = 4 + 6 + header_length + *p_file_size_buffer + 4;
            }
            else
            {
               file_size = 4 + header_length + *p_file_size_buffer + 4;
            }

            if (db.auth == AUTH_AWS4_HMAC_SHA256)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "FILE_NAME_IS_HEADER is not yet implemented for AUTH_AWS4_HMAC_SHA256.");
               rm_dupcheck_crc(fullname, p_file_name_buffer,
                               *p_file_size_buffer);
               http_quit();
               exit(INCORRECT);
            }
         }
         else
         {
            file_size = *p_file_size_buffer;
         }
         p_remote_filename = NULL;
         if (db.trans_rename_rule[0] != '\0')
         {
            register int k;

            for (k = 0; k < rule[db.trans_rule_pos].no_of_rules; k++)
            {
               if (pmatch(rule[db.trans_rule_pos].filter[k],
                          p_file_name_buffer, NULL) == 0)
               {
                  p_remote_filename = remote_filename;
                  change_name(p_file_name_buffer,
                              rule[db.trans_rule_pos].filter[k],
                              rule[db.trans_rule_pos].rename_to[k],
                              p_remote_filename,
                              (MAX_RECIPIENT_LENGTH + MAX_FILENAME_LENGTH) - (p_remote_filename - remote_filename),
                              &counter_fd, &unique_counter, db.id.job);
                  break;
               }
            }
         }
         else if (db.cn_filter != NULL)
              {
                 if (pmatch(db.cn_filter, p_file_name_buffer, NULL) == 0)
                 {
                    p_remote_filename = remote_filename;
                    change_name(p_file_name_buffer, db.cn_filter,
                                db.cn_rename_to, p_remote_filename,
                                (MAX_RECIPIENT_LENGTH + MAX_FILENAME_LENGTH) - (p_remote_filename - remote_filename),
                                &counter_fd, &unique_counter, db.id.job);
                 }
              }
         if (p_remote_filename == NULL)
         {
            p_remote_filename = p_file_name_buffer;
         }
         if ((status = http_put(db.target_dir, p_remote_filename, fullname,
                                file_size, NULL,
#ifdef _WITH_BURST_2
                                ((files_send == 0) && (burst_2_counter == 0)) ? YES : NO)) != SUCCESS)
#else
                                (files_send == 0) ? YES : NO)) != SUCCESS)
#endif
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      (status == INCORRECT) ? NULL : msg_str,
                      "Failed to open remote file `%s' (%d).",
                      p_remote_filename, status);
            http_quit();
            exit(eval_timeout(OPEN_REMOTE_ERROR));
         }
         else
         {
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                            "Open remote file `%s'.", p_remote_filename);
            }
         }

         /* Open local file. */
#ifdef O_LARGEFILE
         if ((fd = open(fullname, O_RDONLY | O_LARGEFILE)) == -1)
#else
         if ((fd = open(fullname, O_RDONLY)) == -1)
#endif
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                      "Failed to open local file `%s' : %s",
                      fullname, strerror(errno));
            rm_dupcheck_crc(fullname, p_file_name_buffer, *p_file_size_buffer);
            http_quit();
            exit(OPEN_LOCAL_ERROR);
         }
         if (fsa->debug > NORMAL_MODE)
         {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                            "Open local file `%s'", fullname);
         }

#ifdef _OUTPUT_LOG
         if (db.output_log == YES)
         {
            start_time = times(&tmsdummy);
         }
#endif

         /*
          * When the contents does not contain a bulletin header
          * it must be stored in the file name.
          */
         if (db.special_flag & FILE_NAME_IS_HEADER)
         {
            int  space_count;
            char *ptr = p_file_name_buffer;

            length_type_indicator = 10;
            buffer[length_type_indicator] = 1; /* SOH */
            buffer[length_type_indicator + 1] = '\015'; /* CR */
            buffer[length_type_indicator + 2] = '\015'; /* CR */
            buffer[length_type_indicator + 3] = '\012'; /* LF */
            header_length = 4;
            space_count = 0;

            if (wmo_counter_fd > 0)
            {
               if (next_counter(wmo_counter_fd, wmo_counter,
                                MAX_WMO_COUNTER) < 0)
               {
                  close_counter_file(wmo_counter_fd, &wmo_counter);
                  wmo_counter_fd = -1;
                  wmo_counter = NULL;
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to get next WMO counter.");
               }
               else
               {
                  if (*wmo_counter < 10)
                  {
                     buffer[length_type_indicator + header_length] = '0';
                     buffer[length_type_indicator + header_length + 1] = '0';
                     buffer[length_type_indicator + header_length + 2] = *wmo_counter + '0';
                  }
                  else if (*wmo_counter < 100)
                       {
                          buffer[length_type_indicator + header_length] = '0';
                          buffer[length_type_indicator + header_length + 1] = (*wmo_counter / 10) + '0';
                          buffer[length_type_indicator + header_length + 2] = (*wmo_counter % 10) + '0';
                       }
                  else if (*wmo_counter < 1000)
                       {
                          buffer[length_type_indicator + header_length] = ((*wmo_counter / 100) % 10) + '0';
                          buffer[length_type_indicator + header_length + 1] = ((*wmo_counter / 10) % 10) + '0';
                          buffer[length_type_indicator + header_length + 2] = (*wmo_counter % 10) + '0';
                       }
                  buffer[length_type_indicator + header_length + 3] = '\015'; /* CR */
                  buffer[length_type_indicator + header_length + 4] = '\015'; /* CR */
                  buffer[length_type_indicator + header_length + 5] = '\012'; /* LF */
                  header_length += 6;
               }
            } /* if (wmo_counter_fd > 0) */

            for (;;)
            {
               while ((*ptr != '_') && (*ptr != '-') && (*ptr != ' ') &&
                      (*ptr != '\0') && (*ptr != '.') && (*ptr != ';'))
               {
                  buffer[length_type_indicator + header_length] = *ptr;
                  header_length++; ptr++;
               }
               if ((*ptr == '\0') || (*ptr == '.') || (*ptr == ';'))
               {
                  break;
               }
               else
               {
                  if (space_count == 2)
                  {
                     if ((isalpha((int)(*(ptr + 1)))) &&
                         (isalpha((int)(*(ptr + 2)))) &&
                         (isalpha((int)(*(ptr + 3)))))
                     {
                        buffer[length_type_indicator + header_length] = ' ';
                        buffer[length_type_indicator + header_length + 1] = *(ptr + 1);
                        buffer[length_type_indicator + header_length + 2] = *(ptr + 2);
                        buffer[length_type_indicator + header_length + 3] = *(ptr + 3);
                        header_length += 4;
                     }
                     break;
                  }
                  else
                  {
                     buffer[length_type_indicator + header_length] = ' ';
                     header_length++; ptr++; space_count++;
                  }
               }
            } /* for (;;) */
            buffer[length_type_indicator + header_length] = '\015'; /* CR */
            buffer[length_type_indicator + header_length + 1] = '\015'; /* CR */
            buffer[length_type_indicator + header_length + 2] = '\012'; /* LF */
            header_length += 3;
            end_length = 4;
         }
         else
         {
            header_length = 0;
            end_length = 0;
            length_type_indicator = 0;
         }

         /* Read local and write remote file. */
         no_of_bytes = 0;
         loops = (length_type_indicator + header_length + *p_file_size_buffer) / blocksize;
         rest = (length_type_indicator + header_length + *p_file_size_buffer) % blocksize;

         if (db.special_flag & FILE_NAME_IS_HEADER)
         {
            if (rest == 0)
            {
               loops--;
               rest = blocksize;
            }

            /* Write length and type indicator. */
            (void)snprintf(buffer, blocksize + 1 + 4, "%08lu",
                           (unsigned long)(*p_file_size_buffer + header_length + end_length));
            if (db.transfer_mode == 'I')
            {
               buffer[length_type_indicator - 2] = 'B';
               buffer[length_type_indicator - 1] = 'I';
            }
            else if (db.transfer_mode == 'A')
                 {
                    buffer[length_type_indicator - 2] = 'A';
                    buffer[length_type_indicator - 1] = 'N';
                 }
                 else
                 {
                    buffer[length_type_indicator - 2] = 'F';
                    buffer[length_type_indicator - 1] = 'X';
                 }
         }

         if (fsa->trl_per_process > 0)
         {
            init_limit_transfer_rate();
         }
         if (fsa->protocol_options & TIMEOUT_TRANSFER)
         {
            start_transfer_time_file = time(NULL);
         }

         for (;;)
         {
            for (j = 0; j < loops; j++)
            {
#ifdef _SIMULATE_SLOW_TRANSFER
               (void)sleep(_SIMULATE_SLOW_TRANSFER);
#endif
               if ((status = read(fd,
                                  (buffer + length_type_indicator + header_length),
                                  (blocksize - length_type_indicator - header_length))) != (blocksize - length_type_indicator - header_length))
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Could not read() local file `%s' : %s",
                            fullname, strerror(errno));
                  rm_dupcheck_crc(fullname, p_file_name_buffer,
                                  *p_file_size_buffer);
                  http_quit();
                  exit(READ_LOCAL_ERROR);
               }
               if ((status = http_write(buffer, NULL, blocksize)) != SUCCESS)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Failed to write block from file `%s' to remote port %d [%d].",
                            p_file_name_buffer, db.port, status);
                  rm_dupcheck_crc(fullname, p_file_name_buffer,
                                  *p_file_size_buffer);
                  http_quit();
                  exit(eval_timeout(WRITE_REMOTE_ERROR));
               }
               if (fsa->trl_per_process > 0)
               {
                  limit_transfer_rate(blocksize, fsa->trl_per_process,
                                      clktck);
               }

               no_of_bytes += blocksize;

               if (gsf_check_fsa(p_db) != NEITHER)
               {
                  fsa->job_status[(int)db.job_no].file_size_in_use_done = no_of_bytes;
                  fsa->job_status[(int)db.job_no].file_size_done += blocksize;
                  fsa->job_status[(int)db.job_no].bytes_send += blocksize;
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
                           rm_dupcheck_crc(fullname, p_file_name_buffer,
                                           *p_file_size_buffer);
                           http_quit();
                           exitflag = 0;
                           exit(STILL_FILES_TO_SEND);
                        }
                     }
                  }
               }
               if (length_type_indicator > 0)
               {
                  length_type_indicator = 0;
                  header_length = 0;
               }
            } /* for (j = 0; j < loops; j++) */

            if (rest > 0)
            {
               if ((status = read(fd,
                                  (buffer + length_type_indicator + header_length),
                                  (rest - length_type_indicator - header_length))) != (rest - length_type_indicator - header_length))
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Could not read() local file `%s' : %s",
                            fullname, strerror(errno));
                  rm_dupcheck_crc(fullname, p_file_name_buffer,
                                  *p_file_size_buffer);
                  http_quit();
                  exit(READ_LOCAL_ERROR);
               }
               if (end_length == 4)
               {
                  buffer[rest] = '\015';
                  buffer[rest + 1] = '\015';
                  buffer[rest + 2] = '\012';
                  buffer[rest + 3] = 3;  /* ETX */
               }
               if ((status = http_write(buffer, NULL, rest + end_length)) != SUCCESS)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Failed to write rest of file %s to remote port %d [%d].",
                            p_file_name_buffer, db.port, status);
                  rm_dupcheck_crc(fullname, p_file_name_buffer,
                                  *p_file_size_buffer);
                  http_quit();
                  exit(eval_timeout(WRITE_REMOTE_ERROR));
               }
               if (fsa->trl_per_process > 0)
               {
                  limit_transfer_rate(rest + end_length,
                                      fsa->trl_per_process, clktck);
               }

               no_of_bytes += rest + end_length;

               if (gsf_check_fsa(p_db) != NEITHER)
               {
                  fsa->job_status[(int)db.job_no].file_size_in_use_done = no_of_bytes;
                  fsa->job_status[(int)db.job_no].file_size_done += rest;
                  fsa->job_status[(int)db.job_no].bytes_send += rest;
               }
            }

            /*
             * Since there are always some users sending files to the
             * AFD not in dot notation, lets check here if this is really
             * the EOF.
             * If not lets continue so long until we hopefully have reached
             * the EOF.
             * NOTE: This is NOT a fool proof way. There must be a better
             *       way!
             */
#ifdef HAVE_STATX
            if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                      STATX_SIZE, &stat_buf) == -1)
#else
            if (fstat(fd, &stat_buf) == -1)
#endif
            {
               (void)rec(transfer_log_fd, DEBUG_SIGN,
#ifdef HAVE_STATX
                         "Hmmm. Failed to statx() `%s' : %s (%s %d)\n",
#else
                         "Hmmm. Failed to fstat() `%s' : %s (%s %d)\n",
#endif
                         fullname, strerror(errno), __FILE__, __LINE__);
               break;
            }
            else
            {
#ifdef HAVE_STATX
               if (stat_buf.stx_size > *p_file_size_buffer)
#else
               if (stat_buf.st_size > *p_file_size_buffer)
#endif
               {
                  char *sign;

                  if (db.special_flag & SILENT_NOT_LOCKED_FILE)
                  {
                     sign = DEBUG_SIGN;
                  }
                  else
                  {
                     sign = WARN_SIGN;
                  }
#ifdef HAVE_STATX
                  loops = (stat_buf.stx_size - *p_file_size_buffer) / blocksize;
                  rest = (stat_buf.stx_size - *p_file_size_buffer) % blocksize;
                  *p_file_size_buffer = stat_buf.stx_size;
#else
                  loops = (stat_buf.st_size - *p_file_size_buffer) / blocksize;
                  rest = (stat_buf.st_size - *p_file_size_buffer) % blocksize;
                  *p_file_size_buffer = stat_buf.st_size;
#endif

                  /*
                   * Give a warning in the system log, so some action
                   * can be taken against the originator.
                   */
                  receive_log(sign, __FILE__, __LINE__, 0L, db.id.job,
                              "File `%s' for host %s was DEFINITELY send without any locking. #%x",
                              p_file_name_buffer, fsa->host_dsp_name, db.id.job);
               }
               else
               {
                  break;
               }
            }
         } /* for (;;) */

         if (fsa->debug > NORMAL_MODE)
         {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
#if SIZEOF_OFF_T == 4
                            "Wrote %ld bytes", (pri_off_t)no_of_bytes);
#else
                            "Wrote %lld bytes", (pri_off_t)no_of_bytes);
#endif
         }

#ifdef _OUTPUT_LOG
         if (db.output_log == YES)
         {
            end_time = times(&tmsdummy);
         }
#endif
         /* Close local file. */
         if (close(fd) == -1)
         {
            (void)rec(transfer_log_fd, WARN_SIGN,
                      "%-*s[%c]: Failed to close() local file %s : %s (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, db.job_no + '0',
                      p_file_name_buffer, strerror(errno),
                      __FILE__, __LINE__);
            /*
             * Since we usually do not send more then 100 files and
             * sf_http() will exit(), there is no point in stopping
             * the transmission.
             */
         }

         if ((status = http_put_response()) != SUCCESS)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      (status == INCORRECT) ? NULL : msg_str,
                      "Failed to PUT remote file `%s' (%d).",
                      p_remote_filename, status);
            if (status == CONNECTION_REOPENED)
            {
               exit_status = STILL_FILES_TO_SEND;
            }
            else
            {
               exit_status = OPEN_REMOTE_ERROR;
            }
            rm_dupcheck_crc(fullname, p_file_name_buffer, *p_file_size_buffer);
            http_quit();
            exit(eval_timeout(exit_status));
         }

         /* Update FSA, one file transmitted. */
         if (gsf_check_fsa(p_db) != NEITHER)
         {
            fsa->job_status[(int)db.job_no].file_name_in_use[0] = '\0';
            fsa->job_status[(int)db.job_no].no_of_files_done++;
            fsa->job_status[(int)db.job_no].file_size_in_use = 0;
            fsa->job_status[(int)db.job_no].file_size_in_use_done = 0;
            local_file_size += *p_file_size_buffer;
            local_file_counter += 1;

            now = time(NULL);
            if (now >= (last_update_time + LOCK_INTERVAL_TIME))
            {
               last_update_time = now;
               update_tfc(local_file_counter, local_file_size,
                          p_file_size_buffer, files_to_send,
                          files_send, now);
               local_file_size = 0;
               local_file_counter = 0;
            }
         }

#ifdef _WITH_TRANS_EXEC
         if (db.special_flag & TRANS_EXEC)
         {
            trans_exec(file_path, fullname, p_file_name_buffer, clktck);
         }
#endif /* _WITH_TRANS_EXEC */

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
                               (db.tls_auth == NO) ? HTTP : HTTPS,
# else
                               HTTP,
# endif
                               &db.output_log);
            }
         }
#endif

         /* Now archive file if necessary. */
         if ((db.archive_time > 0) &&
             (p_db->archive_dir[0] != FAILED_TO_CREATE_ARCHIVE_DIR))
         {
#ifdef WITH_ARCHIVE_COPY_INFO
            int ret;
#endif

            /*
             * By telling the function archive_file() that this
             * is the first time to archive a file for this job
             * (in struct p_db) it does not always have to check
             * whether the directory has been created or not. And
             * we ensure that we do not create duplicate names
             * when adding db.archive_time to msg_name.
             */
#ifdef WITH_ARCHIVE_COPY_INFO
            if ((ret = archive_file(file_path, p_file_name_buffer, p_db)) < 0)
#else
            if (archive_file(file_path, p_file_name_buffer, p_db) < 0)
#endif
            {
               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               "Failed to archive file `%s'",
                               p_file_name_buffer);
               }

               /*
                * NOTE: We _MUST_ delete the file we just send,
                *       else the file directory will run full!
                */
               if (unlink(fullname) == -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Could not unlink() local file `%s' after sending it successfully : %s",
                             fullname, strerror(errno));
               }

#ifdef _OUTPUT_LOG
               if (db.output_log == YES)
               {
                  (void)memcpy(ol_file_name, db.p_unique_name, db.unl);
                  if ((db.trans_rename_rule[0] != '\0') ||
                      (db.cn_filter != NULL))
                  {
                     *ol_file_name_length = (unsigned short)snprintf(ol_file_name + db.unl,
                                                                     MAX_FILENAME_LENGTH + 1 + MAX_FILENAME_LENGTH + 2,
                                                                     "%s%c%s",
                                                                     p_file_name_buffer,
                                                                     SEPARATOR_CHAR,
                                                                     p_remote_filename) + db.unl;
                     if (*ol_file_name_length >= (MAX_FILENAME_LENGTH + 1 + MAX_FILENAME_LENGTH + 2 + db.unl))
                     {
                        *ol_file_name_length = MAX_FILENAME_LENGTH + 1 + MAX_FILENAME_LENGTH + 2 + db.unl;
                     }
                  }
                  else
                  {
                     (void)strcpy(ol_file_name + db.unl, p_file_name_buffer);
                     *ol_file_name_length = (unsigned short)strlen(ol_file_name);
                     ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
                     ol_file_name[*ol_file_name_length + 1] = '\0';
                     (*ol_file_name_length)++;
                  }
                  *ol_file_size = *p_file_size_buffer;
                  *ol_job_number = fsa->job_status[(int)db.job_no].job_id;
                  *ol_retries = db.retries;
                  *ol_unl = db.unl;
                  *ol_transfer_time = end_time - start_time;
                  *ol_archive_name_length = 0;
                  *ol_output_type = OT_NORMAL_DELIVERED + '0';
                  ol_real_size = *ol_file_name_length + ol_size;
                  if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "write() error : %s", strerror(errno));
                  }
               }
#endif
            }
            else
            {
               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                               "Archived file `%s'", p_file_name_buffer);
               }
#ifdef WITH_ARCHIVE_COPY_INFO
               if (ret == DATA_COPIED)
               {
                  archived_copied++;
               }
#endif

#ifdef _OUTPUT_LOG
               if (db.output_log == YES)
               {
                  (void)memcpy(ol_file_name, db.p_unique_name, db.unl);
                  if ((db.trans_rename_rule[0] != '\0') ||
                      (db.cn_filter != NULL))
                  {
                     *ol_file_name_length = (unsigned short)snprintf(ol_file_name + db.unl,
                                                                     MAX_FILENAME_LENGTH + 1 + MAX_FILENAME_LENGTH + 2,
                                                                     "%s%c%s",
                                                                     p_file_name_buffer,
                                                                     SEPARATOR_CHAR,
                                                                     p_remote_filename) + db.unl;
                     if (*ol_file_name_length >= (MAX_FILENAME_LENGTH + 1 + MAX_FILENAME_LENGTH + 2 + db.unl))
                     {
                        *ol_file_name_length = MAX_FILENAME_LENGTH + 1 + MAX_FILENAME_LENGTH + 2 + db.unl;
                     }
                  }
                  else
                  {
                     (void)strcpy(ol_file_name + db.unl, p_file_name_buffer);
                     *ol_file_name_length = (unsigned short)strlen(ol_file_name);
                     ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
                     ol_file_name[*ol_file_name_length + 1] = '\0';
                     (*ol_file_name_length)++;
                  }
                  (void)strcpy(&ol_file_name[*ol_file_name_length + 1],
                               &db.archive_dir[db.archive_offset]);
                  *ol_file_size = *p_file_size_buffer;
                  *ol_job_number = fsa->job_status[(int)db.job_no].job_id;
                  *ol_retries = db.retries;
                  *ol_unl = db.unl;
                  *ol_transfer_time = end_time - start_time;
                  *ol_archive_name_length = (unsigned short)strlen(&ol_file_name[*ol_file_name_length + 1]);
                  *ol_output_type = OT_NORMAL_DELIVERED + '0';
                  ol_real_size = *ol_file_name_length +
                                 *ol_archive_name_length + 1 + ol_size;
                  if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "write() error : %s", strerror(errno));
                  }
               }
#endif
            }
         }
         else
         {
#ifdef WITH_UNLINK_DELAY
            int unlink_loops = 0;

try_again_unlink:
#endif
            /* Delete the file we just have send. */
            if (unlink(fullname) == -1)
            {
#ifdef WITH_UNLINK_DELAY
               if ((errno == EBUSY) && (unlink_loops < 20))
               {
                  (void)my_usleep(100000L);
                  unlink_loops++;
                  goto try_again_unlink;
               }
#endif
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not unlink() local file %s after sending it successfully : %s",
                          fullname, strerror(errno));
            }

#ifdef _OUTPUT_LOG
            if (db.output_log == YES)
            {
               (void)memcpy(ol_file_name, db.p_unique_name, db.unl);
               if ((db.trans_rename_rule[0] != '\0') ||
                   (db.cn_filter != NULL))
               {
                  *ol_file_name_length = (unsigned short)snprintf(ol_file_name + db.unl,
                                                                  MAX_FILENAME_LENGTH + 1 + MAX_FILENAME_LENGTH + 2,
                                                                  "%s%c%s",
                                                                  p_file_name_buffer,
                                                                  SEPARATOR_CHAR,
                                                                  p_remote_filename) + db.unl;
                  if (*ol_file_name_length >= (MAX_FILENAME_LENGTH + 1 + MAX_FILENAME_LENGTH + 2 + db.unl))
                  {
                     *ol_file_name_length = MAX_FILENAME_LENGTH + 1 + MAX_FILENAME_LENGTH + 2 + db.unl;
                  }
               }
               else
               {
                  (void)strcpy(ol_file_name + db.unl, p_file_name_buffer);
                  *ol_file_name_length = (unsigned short)strlen(ol_file_name);
                  ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
                  ol_file_name[*ol_file_name_length + 1] = '\0';
                  (*ol_file_name_length)++;
               }
               *ol_file_size = *p_file_size_buffer;
               *ol_job_number = fsa->job_status[(int)db.job_no].job_id;
               *ol_retries = db.retries;
               *ol_unl = db.unl;
               *ol_transfer_time = end_time - start_time;
               *ol_archive_name_length = 0;
               *ol_output_type = OT_NORMAL_DELIVERED + '0';
               ol_real_size = *ol_file_name_length + ol_size;
               if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "write() error : %s", strerror(errno));
               }
            }
#endif
         }

         /*
          * After each successful transfer set error
          * counter to zero, so that other jobs can be
          * started.
          */
         if (gsf_check_fsa(p_db) != NEITHER)
         {
            unset_error_counter_fsa(fsa_fd, transfer_log_fd, p_work_dir,
                                    fsa, (struct job *)&db);
#ifdef WITH_ERROR_QUEUE
            if (fsa->host_status & ERROR_QUEUE_SET)
            {
               remove_from_error_queue(db.id.job, fsa, db.fsa_pos, fsa_fd);
            }
#endif
            if (fsa->host_status & HOST_ACTION_SUCCESS)
            {
               error_action(fsa->host_alias, "start", HOST_SUCCESS_ACTION,
                            transfer_log_fd);
            }
         }
#ifdef WITH_DUP_CHECK
# ifndef FAST_SF_DUPCHECK
         }
# endif
#endif

         p_file_name_buffer += MAX_FILENAME_LENGTH;
         p_file_size_buffer++;
         if (file_mtime_buffer != NULL)
         {
            p_file_mtime_buffer++;
         }
      } /* for (files_send = 0; files_send < files_to_send; files_send++) */

#ifdef WITH_ARCHIVE_COPY_INFO
      if (archived_copied > 0)
      {
         trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
                   "Copied %u files to archive.", archived_copied);
         archived_copied = 0;
      }
#endif

      if (local_file_counter)
      {
         if (gsf_check_fsa(p_db) != NEITHER)
         {
            update_tfc(local_file_counter, local_file_size,
                       p_file_size_buffer, files_to_send, files_send,
                       time(NULL));
            local_file_size = 0;
            local_file_counter = 0;
         }
      }

      /*
       * Remove file directory, but only when all files have
       * been transmitted.
       */
      if ((files_to_send == files_send) || (files_to_send < 1))
      {
         if (rmdir(file_path) < 0)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to remove directory %s : %s",
                       file_path, strerror(errno));
         }
      }
      else
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "There are still %d files for %s. Will NOT remove this job!",
                    files_to_send - files_send, file_path);
         exit_status = STILL_FILES_TO_SEND;
      }

#ifdef _WITH_BURST_2
      burst_2_counter++;
      diff_time = time(NULL) - connected;
      if (((fsa->protocol_options & KEEP_CONNECTED_DISCONNECT) &&
           (db.keep_connected > 0) && (diff_time > db.keep_connected)) ||
          ((db.disconnect > 0) && (diff_time > db.disconnect)))
      {
         cb2_ret = NO;
         break;
      }
   } while ((cb2_ret = check_burst_sf(file_path, &files_to_send, 0,
# ifdef _WITH_INTERRUPT_JOB
                                      0,
# endif
# ifdef _OUTPUT_LOG
                                      &ol_fd,
# endif
# ifndef AFDBENCH_CONFIG
                                      NULL,
# endif
                                      &values_changed)) == YES);
   burst_2_counter--;

   if (cb2_ret == NEITHER)
   {
      exit_status = STILL_FILES_TO_SEND;
   }
#endif /* _WITH_BURST_2 */

   free(buffer);

   /* Disconnect from remote port. */
   http_quit();
   if ((fsa != NULL) && (fsa_pos_save == YES) && (fsa->debug > NORMAL_MODE))
   {
      trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                   "Disconnected from port %d.", db.port);
   }

   if (wmo_counter_fd > 0)
   {
      close_counter_file(wmo_counter_fd, &wmo_counter);
   }

   if ((exit_status != STILL_FILES_TO_SEND) &&
       (fsa->job_status[(int)db.job_no].unique_name[1] != '\0') &&
       (fsa->job_status[(int)db.job_no].unique_name[0] != '\0') &&
       (fsa->job_status[(int)db.job_no].unique_name[2] > 7) &&
       (strncmp(fsa->job_status[(int)db.job_no].unique_name,
                db.msg_name, MAX_MSG_NAME_LENGTH) != 0))
   {
      /* Check for a burst miss. */
      if (check_job_dir_empty(fsa->job_status[(int)db.job_no].unique_name,
                              file_path) == NO)
      {
         exit_status = STILL_FILES_TO_SEND;
      }
   }

   exitflag = 0;
   exit(exit_status);
}


/*++++++++++++++++++++++++++++ sf_http_exit() +++++++++++++++++++++++++++*/
static void
sf_http_exit(void)
{
   if ((fsa != NULL) && (db.fsa_pos >= 0) && (fsa_pos_save == YES))
   {
      int     diff_no_of_files_done;
      u_off_t diff_file_size_done;

      if (local_file_counter)
      {                      
         if (gsf_check_fsa((struct job *)&db) != NEITHER)
         {
            update_tfc(local_file_counter, local_file_size,
                       p_file_size_buffer, files_to_send, files_send,
                       time(NULL));
         }
      }

      diff_no_of_files_done = fsa->job_status[(int)db.job_no].no_of_files_done -
                              prev_no_of_files_done;
      diff_file_size_done = fsa->job_status[(int)db.job_no].file_size_done -
                            prev_file_size_done;
      if ((diff_file_size_done > 0) || (diff_no_of_files_done > 0))
      {
         int  length;
#ifdef _WITH_BURST_2
         char buffer[MAX_INT_LENGTH + 5 + MAX_OFF_T_LENGTH + 16 + MAX_INT_LENGTH + 11 + MAX_INT_LENGTH + 1];

         length = MAX_INT_LENGTH + 5 + MAX_OFF_T_LENGTH + 16 + MAX_INT_LENGTH + 11 + MAX_INT_LENGTH + 1;
#else
         char buffer[MAX_INT_LENGTH + 5 + MAX_OFF_T_LENGTH + 16 + MAX_INT_LENGTH + 1];

         length = MAX_INT_LENGTH + 5 + MAX_OFF_T_LENGTH + 16 + MAX_INT_LENGTH + 1;
#endif

         WHAT_DONE_BUFFER(length, buffer, "send",
                          diff_file_size_done, diff_no_of_files_done);
#ifdef _WITH_BURST_2
         if (burst_2_counter == 1)
         {
            (void)strcpy(&buffer[length], " [BURST]");
         }
         else if (burst_2_counter > 1)
              {
                 (void)snprintf(buffer + length,
#ifdef _WITH_BURST_2
                                MAX_INT_LENGTH + 5 + MAX_OFF_T_LENGTH + 16 + MAX_INT_LENGTH + 11 + MAX_INT_LENGTH + 1 - length,
#else
                                MAX_INT_LENGTH + 5 + MAX_OFF_T_LENGTH + 16 + MAX_INT_LENGTH + 1 - length,
#endif
                                " [BURST * %u]", burst_2_counter);
              }
#endif /* _WITH_BURST_2 */
         trans_log(INFO_SIGN, NULL, 0, NULL, NULL, "%s #%x",
                   buffer, db.id.job);
      }
      reset_fsa((struct job *)&db, exitflag, 0, 0);
      fsa_detach_pos(db.fsa_pos);
   }

   free(file_name_buffer);
   free(file_size_buffer);

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
   reset_fsa((struct job *)&db, IS_FAULTY_VAR, 0, 0);
   system_log(DEBUG_SIGN, __FILE__, __LINE__,
             "Aaarrrggh! Received SIGSEGV. Remove the programmer who wrote this!");
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   reset_fsa((struct job *)&db, IS_FAULTY_VAR, 0, 0);
   system_log(DEBUG_SIGN, __FILE__, __LINE__, "Uuurrrggh! Received SIGBUS.");
   abort();
}


/*++++++++++++++++++++++++++++++ sig_kill() +++++++++++++++++++++++++++++*/
static void
sig_kill(int signo)
{
   exitflag = 0;
   if ((fsa != NULL) && (fsa_pos_save == YES) &&
       (fsa->job_status[(int)db.job_no].unique_name[2] == 5))
   {
      exit(SUCCESS);
   }
   else
   {
      exit(GOT_KILLED);
   }
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   exit(INCORRECT);
}
