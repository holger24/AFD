/*
 *  sf_smtp.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   sf_smtp - send data via SMTP
 **
 ** SYNOPSIS
 **   sf_smtp <work dir> <job no.> <FSA id> <FSA pos> <msg name> [options]
 **
 **   options
 **       --version              Version
 **       -a <age limit>         The age limit for the files being send.
 **       -A                     Disable archiving of files.
 **       -C <charset>           Default charset.
 **       -g <group mail domain> Default group mail domain.
 **       -o <retries>           Old/Error message and number of retries.
 **       -r                     Resend from archive (job from show_olog).
 **       -s <SMTP server>       Server where to send the mails.
 **       -t                     Temp toggle.
 **
 ** DESCRIPTION
 **   sf_smtp sends the given files to the defined recipient via SMTP
 **   It does so by using it's own SMTP-client.
 **
 **   In the message file will be the data it needs about the
 **   remote host in the following format:
 **       [destination]
 **       <sheme>://<user>:<password>@<host>:<port>/<url-path>
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
 **   14.12.1996 H.Kiehl Created
 **   27.01.1997 H.Kiehl Include support for output logging.
 **   08.05.1997 H.Kiehl Logging archive directory.
 **   09.08.1997 H.Kiehl Write some more detailed logging when
 **                      an error has occurred.
 **   29.08.1997 H.Kiehl Support for 'real' host names.
 **   03.06.1998 H.Kiehl Added subject option.
 **   28.07.1998 H.Kiehl Added 'file name is user' option.
 **   27.11.1998 H.Kiehl Added attaching file.
 **   29.03.1999 H.Kiehl Local user name is LOGNAME.
 **   24.08.1999 H.Kiehl Enhanced option "attach file" to support trans-
 **                      renaming.
 **   08.07.2000 H.Kiehl Cleaned up log output to reduce code size.
 **   19.02.2002 H.Kiehl Sending a single mail to multiple users.
 **   02.07.2002 H.Kiehl Added from option.
 **   03.07.2002 H.Kiehl Added charset option.
 **   04.07.2002 H.Kiehl Add To header by default.
 **   13.06.2004 H.Kiehl Added transfer rate limit.
 **   04.06.2006 H.Kiehl Added option 'file name is target'.
 **   05.08.2006 H.Kiehl For option 'subject' added possibility to insert
 **                      the filename or part of it.
 **   22.01.2009 H.Kiehl When adding the filename to the subject, add all
 **                      filenames if we have more then one file.
 **   24.08.2017 H.Kiehl Added option to specify the default charset.
 **   22.06.2020 H.Kiehl Added option to set default group mail domanin.
 **   29.06.2020 H.Kiehl Added option group-to.
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
# include <sys/times.h>                /* times(), struct tms            */
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
# include <sys/time.h>                 /* gettimeofday()                 */
#endif
#include <fcntl.h>
#include <signal.h>                    /* signal()                       */
#include <unistd.h>                    /* unlink(), close()              */
#ifndef HAVE_GETTEXT
# include <locale.h>                   /* setlocale()                    */
#endif
#include <errno.h>
#include "fddefs.h"
#include "smtpdefs.h"
#include "version.h"

/* Note: WITH_MAILER_IDENTIFIER is not defined since it has security */
/*       implications.                                               */

/* Global variables. */
unsigned int               special_flag = 0;
int                        counter_fd = -1,
                           event_log_fd = STDERR_FILENO,
                           exitflag = IS_FAULTY_VAR,
                           files_to_delete, /* NOT USED. */
#ifdef HAVE_HW_CRC32
                           have_hw_crc32 = NO,
#endif
#ifdef _MAINTAINER_LOG
                           maintainer_log_fd = STDERR_FILENO,
#endif
                           no_of_hosts,   /* This variable is not used   */
                                          /* in this module.             */
                           fsa_fd = -1,
                           fsa_id,
                           fsa_pos_save = NO,
                           line_length = 0, /* encode_base64()           */
                           *p_no_of_hosts = NULL,
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
#endif /* _OUTPUT_LOG */
#ifdef _WITH_DE_MAIL_SUPPORT
int                        demcd_fd = -2;
# ifdef WITHOUT_FIFO_RW_SUPPORT
int                        demcd_readfd = -2;
# endif
unsigned int               *demcd_job_number;
char                       *demcd_data = NULL,
                           *demcd_file_name;
unsigned char              *demcd_confirmation_type;
unsigned short             *demcd_file_name_length,
                           *demcd_unl;
off_t                      *demcd_file_size;
size_t                     demcd_size,
                           demcd_real_size;
#endif /* _WITH_DE_MAIL_SUPPORT */
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
                           msg_str[MAX_RET_MSG_LENGTH],
                           tr_hostname[MAX_HOSTNAME_LENGTH + 2],
                           *del_file_name_buffer = NULL, /* NOT USED. */
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
#ifdef _WITH_DE_MAIL_SUPPORT
static void                gen_message_id(char *),
                           gen_privat_id(char *);
#endif
static void                sf_smtp_exit(void),
                           sig_bus(int),
                           sig_segv(int),
                           sig_kill(int),
                           sig_exit(int);

/* #define _SIMULATE_SLOW_TRANSFER 2L */


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int              buffer_size = 0,
                    encode_buffer_size = 0,
                    exit_status = TRANSFER_SUCCESS,
                    fd,
                    j,
                    status,
                    loops,
                    rest,
                    mail_header_size = 0,
#ifdef _OUTPUT_LOG
                    current_toggle,
                    mail_id_length,
#endif
                    blocksize,
                    *unique_counter,
                    write_size;
#ifdef WITH_ARCHIVE_COPY_INFO
   unsigned int     archived_copied = 0;
#endif
   off_t            no_of_bytes;
   time_t           connected,
#ifdef _WITH_BURST_2
                    diff_time,
#endif
                    end_transfer_time_file,
                    *p_file_mtime_buffer,
                    start_transfer_time_file = 0,
                    last_update_time,
                    now;
#ifdef _WITH_BURST_2
   int              cb2_ret;
   unsigned int     values_changed = 0;
#endif
   char             *smtp_buffer = NULL,
                    *p_file_name_buffer = NULL,
                    host_name[256],
                    local_user[MAX_FILENAME_LENGTH + MAX_FILENAME_LENGTH],
                    multipart_boundary[MAX_FILENAME_LENGTH],
                    remote_user[MAX_FILENAME_LENGTH],
                    *buffer = NULL,
                    *buffer_ptr,
                    *encode_buffer = NULL,
                    final_filename[MAX_FILENAME_LENGTH],
                    fullname[MAX_PATH_LENGTH + 1],
                    file_path[MAX_PATH_LENGTH],
                    *extra_mail_header_buffer = NULL,
#ifdef _OUTPUT_LOG
                    mail_id[1 + MAX_MAIL_ID_LENGTH + 1],
#endif
                    *mail_header_buffer = NULL;
   clock_t          clktck;
#ifdef HAVE_STATX
   struct statx     stat_buf;
#else
   struct stat      stat_buf;
#endif
   struct job       *p_db;
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
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "sigaction() error : %s", strerror(errno));
      exit(INCORRECT);
   }
#endif

   /* Do some cleanups when we exit. */
   if (atexit(sf_smtp_exit) != 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not register exit function : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Initialise variables. */
   local_file_counter = 0;
   files_to_send = init_sf(argc, argv, file_path, SMTP_FLAG);
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

         /*
          * Blocksize must be large enough to accommodate two or three lines
          * since we write stuff like From: etc in one hunk.
          */
         if (blocksize < 256)
         {
            blocksize = 256;
         }
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
       (signal(SIGPIPE, SIG_IGN) == SIG_ERR))  /* Ignore SIGPIPE! */
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "signal() error : %s", strerror(errno));
      exit(INCORRECT);
   }

   /*
    * The extra buffer is needed to convert LF's to CRLF.
    */
   if ((smtp_buffer = malloc(((blocksize * 2) + 1))) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      exit(ALLOC_ERROR);
   }

   if (db.smtp_server[0] == '\0')
   {
      (void)my_strncpy(db.smtp_server, SMTP_HOST_NAME,
                       MAX_REAL_HOSTNAME_LENGTH);
#ifdef _OUTPUT_LOG
      current_toggle = HOST_ONE;
#endif
   }
   else
   {
      if (db.special_flag & SMTP_SERVER_NAME_IN_MESSAGE)
      {
         if (db.toggle_host == YES)
         {
            if (fsa->host_toggle == HOST_ONE)
            {
               (void)my_strncpy(db.smtp_server,
                                fsa->real_hostname[HOST_TWO - 1],
                                MAX_REAL_HOSTNAME_LENGTH);
#ifdef _OUTPUT_LOG
               current_toggle = HOST_TWO;
#endif
            }
            else
            {
               (void)my_strncpy(db.smtp_server,
                                fsa->real_hostname[HOST_ONE - 1],
                                MAX_REAL_HOSTNAME_LENGTH);
#ifdef _OUTPUT_LOG
               current_toggle = HOST_ONE;
#endif
            }
         }
         else
         {
            (void)my_strncpy(db.smtp_server,
                             fsa->real_hostname[(int)(fsa->host_toggle - 1)],
                             MAX_REAL_HOSTNAME_LENGTH);
#ifdef _OUTPUT_LOG
            current_toggle = (int)fsa->host_toggle;
#endif
         }
      }
#ifdef _OUTPUT_LOG
      else
      {
         if (db.toggle_host == YES)
         {
            if (fsa->host_toggle == HOST_ONE)
            {
               current_toggle = HOST_TWO;
            }
            else
            {
               current_toggle = HOST_ONE;
            }
         }
         else
         {
            current_toggle = (int)fsa->host_toggle;
         }
      }
#endif
   }

#ifdef _OUTPUT_LOG
   if (db.output_log == YES)
   {
# ifdef WITHOUT_FIFO_RW_SUPPORT
      output_log_fd(&ol_fd, &ol_readfd, &db.output_log);
# else
      output_log_fd(&ol_fd, &db.output_log);
# endif
      output_log_ptrs(&ol_retries, &ol_job_number, &ol_data, &ol_file_name,
                      &ol_file_name_length, &ol_archive_name_length,
                      &ol_file_size, &ol_unl, &ol_size, &ol_transfer_time,
                      &ol_output_type, db.host_alias, (current_toggle - 1),
#  ifdef _WITH_DE_MAIL_SUPPORT
                      (db.protocol & DE_MAIL_FLAG) ? DE_MAIL : SMTP,
#  else
                      SMTP,
#  endif
                      &db.output_log);
   }
#endif


#ifdef TEST_WITHOUT_SENDING
   if ((db.special_flag & SMTP_SERVER_NAME_IN_MESSAGE) == 0)
   {
      if (db.toggle_host == YES)
      {
         if (fsa->host_toggle == HOST_ONE)
         {
            (void)my_strncpy(db.hostname, fsa->real_hostname[HOST_TWO - 1],
                             MAX_REAL_HOSTNAME_LENGTH);
         }
         else
         {
            (void)my_strncpy(db.hostname, fsa->real_hostname[HOST_ONE - 1],
                             MAX_REAL_HOSTNAME_LENGTH);
         }
      }
      else
      {
         (void)my_strncpy(db.hostname,
                          fsa->real_hostname[(int)(fsa->host_toggle - 1)],
                          MAX_REAL_HOSTNAME_LENGTH);
      }
   }
   if (((db.special_flag & FILE_NAME_IS_USER) == 0) &&
       ((db.special_flag & FILE_NAME_IS_TARGET) == 0) &&
       (db.group_list == NULL))
   {
      (void)snprintf(remote_user, MAX_FILENAME_LENGTH,
                     "%s@%s", db.user, db.hostname);
   }
   else
   {
      remote_user[0] = '\0';
   }
   system_log(DEBUG_SIGN, __FILE__, __LINE__,
              "Connecting to %s, mail address : %s",
              db.smtp_server, remote_user);
   if (rec_rmdir(file_path) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to remove directory `%s' : %s",
                 file_path, strerror(errno));
   }

   exitflag = 0;
   exit(TRANSFER_SUCCESS);
#endif /* TEST_WITHOUT_SENDING */

   if (gethostname(host_name, 255) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "gethostname() error : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Connect to remote SMTP-server. */
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
   if ((status = smtp_connect(db.smtp_server, db.port,
                              db.sndbuf_size)) != SUCCESS)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                "SMTP connection to <%s> at port %d failed (%d).",
                db.smtp_server, db.port, status);
      exit(eval_timeout(CONNECT_ERROR));
   }
   else
   {
      if (fsa->debug > NORMAL_MODE)
      {
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                      "Connected to <%s> at port %d.",
                      db.smtp_server, db.port);
      }
   }
   connected = time(NULL);

#ifdef _WITH_BURST_2
   do
   {
      if (burst_2_counter > 0)
      {
         (void)memcpy(fsa->job_status[(int)db.job_no].unique_name,
                      db.msg_name, MAX_MSG_NAME_LENGTH);
         fsa->job_status[(int)db.job_no].job_id = db.id.job;
         if (fsa->debug > NORMAL_MODE)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL, "SMTP Bursting.");
         }
      }
#endif /* _WITH_BURST_2 */

#ifdef _WITH_BURST_2
      if ((burst_2_counter == 0) || (values_changed & AUTH_CHANGED))
      {
#endif
         status = smtp_ehlo(host_name);

         if (status == 502)
         {
            if (db.smtp_auth == SMTP_AUTH_NONE)
            {
               if ((status = smtp_helo(host_name)) != SUCCESS)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                            "Failed to send EHLO and HELO to <%s> (%d).",
                            db.smtp_server, status);
                  (void)smtp_quit();
                  exit(eval_timeout(CONNECT_ERROR));
               }
               else
               {
                  if (fsa->debug > NORMAL_MODE)
                  {
                     trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                  "Send HELO.");
                  }
               }
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         "Failed to send EHLO to <%s> (%d).",
                         db.smtp_server, status);
               (void)smtp_quit();
               exit(eval_timeout(CONNECT_ERROR));
            }
         }
         else if (status == SUCCESS)
              {
                 if (fsa->debug > NORMAL_MODE)
                 {
                    trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                                 "Send EHLO.");
                 }
              }
              else
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                           "Failed to send EHLO to <%s> (%d).",
                           db.smtp_server, status);
                 (void)smtp_quit();
                 exit(eval_timeout(CONNECT_ERROR));
              }

#ifdef WITH_SSL
         /* Try negotiate SMARTTLS. */
         if ((status = smtp_smarttls((fsa->protocol_options & TLS_STRICT_VERIFY) ? YES : NO,
                                     (fsa->protocol_options & TLS_LEGACY_RENEGOTIATION) ? YES : NO)) == SUCCESS)
         {
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                            "SSL/TSL connection to server `%s' successful.",
                            db.smtp_server);
            }

            /*
             * RFC-2487 requires that we discard all knowledge from the
             * previous EHLO command issue the EHLO command again.
             */
            status = smtp_ehlo(host_name);

            if (status == 502)
            {
               if (db.smtp_auth == SMTP_AUTH_NONE)
               {
                  if ((status = smtp_helo(host_name)) != SUCCESS)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                               "Failed to send EHLO and HELO to <%s> (%d).",
                               db.smtp_server, status);
                     (void)smtp_quit();
                     exit(eval_timeout(CONNECT_ERROR));
                  }
                  else
                  {
                     if (fsa->debug > NORMAL_MODE)
                     {
                        trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                     "Send HELO.");
                     }
                  }
               }
               else
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                            "Failed to send EHLO again to <%s> (%d).",
                            db.smtp_server, status);
                  (void)smtp_quit();
                  exit(eval_timeout(CONNECT_ERROR));
               }
            }
            else if (status == SUCCESS)
                 {
                    if (fsa->debug > NORMAL_MODE)
                    {
                       trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                                    "Send EHLO again.");
                    }
                 }
                 else
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                              "Failed to send EHLO again to <%s> (%d).",
                              db.smtp_server, status);
                    (void)smtp_quit();
                    exit(eval_timeout(CONNECT_ERROR));
                 }
         }
         else if (status == NEITHER)
              {
                 if (fsa->debug > NORMAL_MODE)
                 {
                    trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                 "Server `%s' not supporting SSL/TSL connection.",
                                 db.smtp_server);
                 }
              }
              else
              {
                 trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, msg_str,
                           "SSL/TSL connection to server `%s' failed. Sending unencrypted.",
                           db.smtp_server);
              }
#endif /* WITH_SSL */

         if (db.smtp_auth != SMTP_AUTH_NONE)
         {
            /* Perform SMTP authentication. */
            if ((status = smtp_auth(db.smtp_auth, db.smtp_user,
                                    db.password)) != SUCCESS)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Failed to auth login again to <%s> (%d).",
                         db.smtp_server, status);
               (void)smtp_quit();
               exit(eval_timeout(CONNECT_ERROR));
            }
            else
            {
               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                               "AUTH login again as %s",
                               db.smtp_user);
               }
            }
         }
#ifdef _WITH_BURST_2
      }
#endif

      /* Inform FSA that we have finished connecting. */
#ifdef _WITH_BURST_2
      if ((burst_2_counter == 0) && (gsf_check_fsa(p_db) != NEITHER))
#else
      if (gsf_check_fsa(p_db) != NEITHER)
#endif
      {
#ifdef LOCK_DEBUG
         lock_region_w(fsa_fd, db.lock_offset + LOCK_CON, __FILE__, __LINE__);
#else
         lock_region_w(fsa_fd, db.lock_offset + LOCK_CON);
#endif
         fsa->job_status[(int)db.job_no].connect_status = SMTP_ACTIVE;
         fsa->job_status[(int)db.job_no].no_of_files = files_to_send;
         fsa->connections += 1;
#ifdef LOCK_DEBUG
         unlock_region(fsa_fd, db.lock_offset + LOCK_CON, __FILE__, __LINE__);
#else
         unlock_region(fsa_fd, db.lock_offset + LOCK_CON);
#endif
      }

      /* Prepare local and remote user name. */
      if (db.from != NULL)
      {
         (void)my_strncpy(local_user, db.from, MAX_FILENAME_LENGTH);
      }
      else
      {
         char *ptr;

         if ((ptr = getenv("LOGNAME")) != NULL)
         {
            (void)snprintf(local_user,
                           MAX_FILENAME_LENGTH + MAX_FILENAME_LENGTH,
                           "%s@%s", ptr, host_name);
         }
         else
         {
            (void)snprintf(local_user,
                           MAX_FILENAME_LENGTH + MAX_FILENAME_LENGTH,
                           "%s@%s", AFD_USER_NAME, host_name);
         }
      }

      if ((db.special_flag & SMTP_SERVER_NAME_IN_MESSAGE) == 0)
      {
         if (db.toggle_host == YES)
         {
            if (fsa->host_toggle == HOST_ONE)
            {
               (void)my_strncpy(db.hostname, fsa->real_hostname[HOST_TWO - 1],
                                MAX_REAL_HOSTNAME_LENGTH);
            }
            else
            {
               (void)my_strncpy(db.hostname, fsa->real_hostname[HOST_ONE - 1],
                                MAX_REAL_HOSTNAME_LENGTH);
            }
         }
         else
         {
            (void)my_strncpy(db.hostname,
                             fsa->real_hostname[(int)(fsa->host_toggle - 1)],
                             MAX_REAL_HOSTNAME_LENGTH);
         }
      }
      if (((db.special_flag & FILE_NAME_IS_USER) == 0) &&
          ((db.special_flag & FILE_NAME_IS_TARGET) == 0) &&
          (db.group_list == NULL))
      {
         (void)snprintf(remote_user, MAX_FILENAME_LENGTH,
                        "%s@%s", db.user, db.hostname);
      }

#ifdef _WITH_BURST_2
      if (burst_2_counter == 0)
      {
#endif
         /* Allocate buffer to read data from the source file. */
         if ((buffer = malloc(blocksize + 2 + 1)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to malloc() %d bytes : %s",
                       blocksize + 2 + 1, strerror(errno));
            exit(ALLOC_ERROR);
         }
         buffer_size = blocksize + 2 + 1;
#ifdef _WITH_BURST_2
      }
#endif
      if ((db.special_flag & ATTACH_FILE) && (encode_buffer == NULL))
      {
         encode_buffer_size = (2 * (blocksize + 1)) + 1;
         if ((encode_buffer = malloc(encode_buffer_size)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to malloc() %d bytes : %s",
                       encode_buffer_size,  strerror(errno));
            exit(ALLOC_ERROR);
         }

         /*
          * When encoding in base64 is done the blocksize must be
          * divideable by three!!!!
          */
         blocksize = blocksize - (blocksize % 3);
         if (blocksize == 0)
         {
            blocksize = 3;
         }
      }

      /* Read mail header file. */
      multipart_boundary[0] = '\0';
      if (db.special_flag & ADD_MAIL_HEADER)
      {
         int  mail_fd;
         char mail_header_file[MAX_PATH_LENGTH];

         if (db.special_ptr == NULL)
         {
            /*
             * Try read default mail header file for this host.
             */
            if (snprintf(mail_header_file, MAX_PATH_LENGTH, "%s%s/%s%s", p_work_dir, ETC_DIR, MAIL_HEADER_IDENTIFIER, fsa->host_alias) >= MAX_PATH_LENGTH)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Path to mail header directory to long!");
               exit(ALLOC_ERROR);
            }
         }
         else
         {
            /*
             * If the path does not start with a / lets assume we
             * want to try a rename rule.
             */
            if (*db.special_ptr != '/')
            {
               register int k;
               char         *ptr;

               k = snprintf(mail_header_file, MAX_PATH_LENGTH, "%s%s%s/",
                            p_work_dir, ETC_DIR, MAIL_HEADER_DIR);
               if (k >= MAX_PATH_LENGTH)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Path to mail header directory to long (%d > %d)",
                            k, MAX_PATH_LENGTH);
                  exit(ALLOC_ERROR);
               }
               ptr = mail_header_file + k;
               for (k = 0; k < rule[db.mail_header_rule_pos].no_of_rules; k++)
               {
                  if (pmatch(rule[db.mail_header_rule_pos].filter[k],
                             p_file_name_buffer, NULL) == 0)
                  {
                     change_name(p_file_name_buffer,
                                 rule[db.mail_header_rule_pos].filter[k],
                                 rule[db.mail_header_rule_pos].rename_to[k],
                                 ptr,
                                 MAX_PATH_LENGTH - (ptr - mail_header_file),
                                 &counter_fd, &unique_counter, db.id.job);
                     break;
                  }
               }
               if (*ptr == '\0')
               {
                  mail_header_file[0] = '\0';
               }
            }
            else
            {
               mail_header_file[0] = '\0';
            }

            if (mail_header_file[0] == '\0')
            {
               /*
                * Try read user specified mail header file for this host.
                */
               (void)my_strncpy(mail_header_file, db.special_ptr,
                                MAX_PATH_LENGTH);
            }
         }

         if ((mail_fd = open(mail_header_file, O_RDONLY)) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to open() mail header file %s : %s",
                       mail_header_file, strerror(errno));
         }
         else
         {
#ifdef HAVE_STATX
            if (statx(mail_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                      STATX_SIZE, &stat_buf) == -1)
#else
            if (fstat(mail_fd, &stat_buf) == -1)
#endif
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                          "Failed to statx() mail header file %s : %s",
#else
                          "Failed to fstat() mail header file %s : %s",
#endif
                          mail_header_file, strerror(errno));
            }
            else
            {
#ifdef HAVE_STATX
               if (stat_buf.stx_size == 0)
#else
               if (stat_buf.st_size == 0)
#endif
               {
                  trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "mail header file %s is empty.");
               }
               else
               {
#ifdef HAVE_STATX
                  if (stat_buf.stx_size <= 204800)
#else
                  if (stat_buf.st_size <= 204800)
#endif
                  {
#ifdef HAVE_STATX
                     if ((mail_header_buffer = malloc(stat_buf.stx_size + 1)) == NULL)
#else
                     if ((mail_header_buffer = malloc(stat_buf.st_size + 1)) == NULL)
#endif
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   "Failed to malloc() buffer for mail header file : %s",
                                   strerror(errno));
                     }
                     else
                     {
#ifdef HAVE_STATX
                        if ((extra_mail_header_buffer = malloc(((2 * stat_buf.stx_size) + 1))) == NULL)
#else
                        if ((extra_mail_header_buffer = malloc(((2 * stat_buf.st_size) + 1))) == NULL)
#endif
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      "Failed to malloc() buffer for mail header file : %s",
                                      strerror(errno));
                           free(mail_header_buffer);
                           mail_header_buffer = NULL;
                        }
                        else
                        {
#ifdef HAVE_STATX
                           mail_header_size = stat_buf.stx_size;
                           if (read(mail_fd, mail_header_buffer,
                                    mail_header_size) != stat_buf.stx_size)
#else
                           mail_header_size = stat_buf.st_size;
                           if (read(mail_fd, mail_header_buffer,
                                    mail_header_size) != stat_buf.st_size)
#endif
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                         "Failed to read() mail header file %s : %s",
                                         mail_header_file, strerror(errno));
                              free(mail_header_buffer);
                              mail_header_buffer = NULL;
                           }
                           else
                           {
                              mail_header_buffer[mail_header_size] = '\0';

                              /* If we are attaching a file we have to */
                              /* do a multipart mail.                  */
                              if (db.special_flag & ATTACH_FILE)
                              {
                                 if (snprintf(multipart_boundary,
                                              MAX_FILENAME_LENGTH, "----%s", db.msg_name) >= MAX_FILENAME_LENGTH)
                                 {
                                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                              "Failed to store multipart boundary because buffer is to small!");
                                    (void)smtp_quit();
                                    exit(ALLOC_ERROR);
                                 }
                              }
                           }
                        }
                     }
                  }
                  else
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                                "Mail header file %s to large (%ld bytes). Allowed are 204800 bytes.",
#else
                                "Mail header file %s to large (%lld bytes). Allowed are 204800 bytes.",
#endif
                                mail_header_file,
#ifdef HAVE_STATX
                                (pri_off_t)stat_buf.stx_size
#else
                                (pri_off_t)stat_buf.st_size
#endif
                               );
                  }
               }
            }
            if (close(mail_fd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "close() error : %s", strerror(errno));
            }
         }
      } /* if (db.special_flag & ADD_MAIL_HEADER) */

      if ((db.special_flag & ATTACH_ALL_FILES) &&
          (multipart_boundary[0] == '\0'))
      {
         if (snprintf(multipart_boundary, MAX_FILENAME_LENGTH, "----%s", db.msg_name) >= MAX_FILENAME_LENGTH)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                      "Failed to store multipart boundary because buffer is to small!");
            (void)smtp_quit();
            exit(ALLOC_ERROR);
         }
      }

      /* Send all files. */
      p_file_name_buffer = file_name_buffer;
      p_file_size_buffer = file_size_buffer;
      p_file_mtime_buffer = file_mtime_buffer;
      last_update_time = time(NULL);
      local_file_size = 0;
      for (files_send = 0; files_send < files_to_send; files_send++)
      {
         if (((db.special_flag & ATTACH_ALL_FILES) == 0) || (files_send == 0))
         {
            /* Send local user name. */
            if ((status = smtp_user(local_user)) != SUCCESS)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         "Failed to send local user <%s> (%d).",
                         local_user, status);
               (void)smtp_quit();
               exit(eval_timeout(USER_ERROR));
            }
            else
            {
               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                               "Entered local user name <%s>.", local_user);
               }
            }

            if (db.special_flag & FILE_NAME_IS_USER)
            {
               if (db.user_rename_rule[0] != '\0')
               {
                  register int k;

                  for (k = 0; k < rule[db.user_rule_pos].no_of_rules; k++)
                  {
                     if (pmatch(rule[db.user_rule_pos].filter[k],
                                p_file_name_buffer, NULL) == 0)
                     {
                        change_name(p_file_name_buffer,
                                    rule[db.user_rule_pos].filter[k],
                                    rule[db.user_rule_pos].rename_to[k],
                                    db.user, MAX_USER_NAME_LENGTH,
                                    &counter_fd, &unique_counter, db.id.job);
                        break;
                     }
                  }
               }
               else
               {
                  (void)my_strncpy(db.user, p_file_name_buffer,
                                   MAX_USER_NAME_LENGTH);
               }
               (void)snprintf(remote_user, MAX_FILENAME_LENGTH,
                              "%s@%s", db.user, db.hostname);
            }
            else if (db.special_flag & FILE_NAME_IS_TARGET)
                 {
                    register int k;
                    char         *ptr;

                    if (db.user_rename_rule[0] != '\0')
                    {
                       for (k = 0; k < rule[db.user_rule_pos].no_of_rules; k++)
                       {
                          if (pmatch(rule[db.user_rule_pos].filter[k],
                                     p_file_name_buffer, NULL) == 0)
                          {
                             change_name(p_file_name_buffer,
                                         rule[db.user_rule_pos].filter[k],
                                         rule[db.user_rule_pos].rename_to[k],
                                         remote_user, MAX_FILENAME_LENGTH,
                                         &counter_fd, &unique_counter,
                                         db.id.job);
                             break;
                          }
                       }
                    }
                    else
                    {
                       (void)my_strncpy(remote_user, p_file_name_buffer,
                                        MAX_FILENAME_LENGTH);
                    }
                    k = 0;
                    ptr = remote_user;
                    while ((*ptr != '@') && (*ptr != '\0'))
                    {
                       db.user[k] = *ptr;
                       k++; ptr++;
                    }
                    if (*ptr == '@')
                    {
                       db.user[k] = '\0';
                    }
                    else
                    {
                       db.user[0] = '\0';
                       trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                                 "File name `%s' is not a mail address!",
                                 remote_user);
                    }
                 }
   
            /* Send remote user name. */
            if (db.group_list == NULL)
            {
               if ((status = smtp_rcpt(remote_user)) != SUCCESS)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                            "Failed to send remote user <%s> (%d).",
                            remote_user, status);

                  /*
                   * Eliminate the job if the user is not accepted by
                   * the remote SMTP server.
                   */
                  if ((status == 550) &&
                      (lposi(msg_str, "Recipient address rejected", 26) != NULL))
                  {
                     (void)smtp_quit();

#ifdef _OUTPUT_LOG
                     if (db.output_log == YES)
                     {
                        if (ol_fd == -2)
                        {
#  ifdef WITHOUT_FIFO_RW_SUPPORT
                           output_log_fd(&ol_fd, &ol_readfd, &db.output_log);
#  else                                                      
                           output_log_fd(&ol_fd, &db.output_log);
#  endif
                        }
                        if (ol_fd > -1)
                        {
                           if (ol_data == NULL)
                           {
                              output_log_ptrs(&ol_retries,
                                              &ol_job_number,
                                              &ol_data,   /* Pointer to buffer. */
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
#  ifdef _WITH_DE_MAIL_SUPPORT
                                              (db.protocol & DE_MAIL_FLAG) ? DE_MAIL : SMTP,
#  else
                                              SMTP,
#  endif
                                              &db.output_log);
                           }
                           (void)memcpy(ol_file_name, db.p_unique_name, db.unl);
                           (void)strcpy(ol_file_name + db.unl, p_file_name_buffer);
                           *ol_file_name_length = (unsigned short)strlen(ol_file_name);
                           ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
                           ol_file_name[*ol_file_name_length + 1] = '\0';
                           (*ol_file_name_length)++;
                           *ol_file_size = *p_file_size_buffer;
                           *ol_job_number = db.id.job;
                           *ol_retries = db.retries;
                           *ol_unl = db.unl;
                           *ol_transfer_time = 0L;
                           *ol_archive_name_length = 0;
                           *ol_output_type = OT_ADRESS_REJ_DELETE + '0';
                           ol_real_size = *ol_file_name_length + ol_size;
                           if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
                           {
                              system_log(ERROR_SIGN, __FILE__, __LINE__,
                                         "write() error : %s", strerror(errno));
                           }
                        }
                     }
#endif

#ifdef _DELETE_LOG
                     if (dl.fd == -1)
                     {
                        delete_log_ptrs(&dl);
                     }
                     *dl.input_time = db.creation_time;
                     *dl.split_job_counter = db.split_job_counter;
                     *dl.unique_number = db.unique_number;
#endif
                     remove_job_files(file_path, db.fsa_pos,
#ifdef _DELETE_LOG
                                      db.id.job,
                                      SEND_FILE_SMTP, RECIPIENT_REJECTED,
#endif
                                      db.lock_offset, __FILE__, __LINE__);
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Removed job since recipient is not accepted by SMTP-server %s at port %d.",
                               db.smtp_server, db.port);

                     exitflag = 0;
                     exit(TRANSFER_SUCCESS);
                  }
                  else
                  {
                     (void)smtp_quit();
                     exit(eval_timeout(REMOTE_USER_ERROR));
                  }
               }
               else
               {
                  if (fsa->debug > NORMAL_MODE)
                  {
                     trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                  "Remote user <%s> accepted by SMTP-server.",
                                  remote_user);
                  }
               }
            }
            else
            {
               int k,
                   rejected_user = 0;

               for (k = 0; k < db.no_listed; k++)
               {
                  if ((status = smtp_rcpt(db.group_list[k])) != SUCCESS)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                               "Failed to send remote user <%s> (%d).",
                               db.group_list[k], status);
                     if ((status == 550) &&
                         (lposi(msg_str, "Recipient address rejected",
                                26) != NULL))
                     {
                        rejected_user++;
                     }
                     else
                     {
                        (void)smtp_quit();
                        exit(eval_timeout(REMOTE_USER_ERROR));
                     }
                  }
                  else
                  {
                     if (fsa->debug > NORMAL_MODE)
                     {
                        trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                     "Remote user <%s> accepted by SMTP-server.",
                                     db.group_list[k]);
                     }
                  }
               }
               if (rejected_user == db.no_listed)
               {
                  /*
                   * Eliminate the job if all users are not accepted by
                   * the remote SMTP server.
                   */
#ifdef _DELETE_LOG
                  if (dl.fd == -1)
                  {
                     delete_log_ptrs(&dl);
                  }
                  *dl.input_time = db.creation_time;
                  *dl.split_job_counter = db.split_job_counter;
                  *dl.unique_number = db.unique_number;
#endif
                  remove_job_files(file_path, db.fsa_pos,
#ifdef _DELETE_LOG
                                   db.id.job, SEND_FILE_SMTP, RECIPIENT_REJECTED,
#endif
                                   db.lock_offset, __FILE__, __LINE__);
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Removed job since recipients are not accepted by SMTP-server %s at port %d.",
                            db.smtp_server, db.port);

                  exitflag = 0;
                  exit(TRANSFER_SUCCESS);
               }
            }

            /* Enter data mode. */
            if ((status = smtp_open()) != SUCCESS)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         "Failed to set DATA mode (%d).", status);
               (void)smtp_quit();
               exit(eval_timeout(DATA_ERROR));
            }
            else
            {
               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                               "Set DATA mode.");
               }
            }
         } /* if (((db.special_flag & ATTACH_ALL_FILES) == 0) || (files_send == 0)) */

         /* Get the the name of the file we want to send next. */
         (void)my_strncpy(final_filename, p_file_name_buffer,
                          MAX_FILENAME_LENGTH);
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
            handle_dupcheck_delete(SEND_FILE_SMTP, fsa->host_alias, fullname,
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

         /* Open local file. */
#ifdef O_LARGEFILE
         if ((fd = open(fullname, O_RDONLY | O_LARGEFILE)) < 0)
#else
         if ((fd = open(fullname, O_RDONLY)) < 0)
#endif
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                      "Failed to open() local file `%s' : %s",
                      fullname, strerror(errno));
            rm_dupcheck_crc(fullname, p_file_name_buffer, *p_file_size_buffer);
            (void)smtp_close();
            (void)smtp_quit();
            exit(OPEN_LOCAL_ERROR);
         }
         else
         {
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                            "Open local file `%s'", fullname);
            }
         }

#ifdef _OUTPUT_LOG
         if (db.output_log == YES)
         {
            start_time = times(&tmsdummy);
         }
#endif

         /* Write status to FSA? */
         if (gsf_check_fsa(p_db) != NEITHER)
         {
            fsa->job_status[(int)db.job_no].file_size_in_use = *p_file_size_buffer;
            (void)my_strncpy(fsa->job_status[(int)db.job_no].file_name_in_use,
                             final_filename, MAX_FILENAME_LENGTH);
         }

         /* Read (local) and write (remote) file */
#ifdef _OUTPUT_LOG
         mail_id_length = 0;
#endif
         no_of_bytes = 0;
         loops = *p_file_size_buffer / blocksize;
         rest = *p_file_size_buffer % blocksize;

         if (((db.special_flag & ATTACH_ALL_FILES) == 0) || (files_send == 0))
         {
            time_t current_time;
            size_t length;
            int    added_content_type = NO;

            /* Write Date: field to header. */
            current_time = time(NULL);
            (void)setlocale(LC_TIME, "C");
            length = strftime(buffer, buffer_size,
                              "Date: %a, %d %b %Y %T %z\r\n",
                              localtime(&current_time));
            (void)setlocale(LC_TIME, "");
            if (length == 0)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Failed to store Date with strftime().");
               rm_dupcheck_crc(fullname, p_file_name_buffer,
                               *p_file_size_buffer);
               rm_dupcheck_crc(fullname, p_file_name_buffer,
                               *p_file_size_buffer);
               (void)smtp_quit();
               exit(ALLOC_ERROR);
            }
            if (smtp_write(buffer, NULL, length) < 0)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Failed to write Date to SMTP-server.");
               rm_dupcheck_crc(fullname, p_file_name_buffer,
                               *p_file_size_buffer);
               (void)smtp_quit();
               exit(eval_timeout(WRITE_REMOTE_ERROR));
            }
            no_of_bytes = length;

            if (db.from != NULL)
            {
               length = snprintf(buffer, buffer_size,
                                 "From: %s\r\n", db.from);
               if (length >= buffer_size)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Buffer length for mail header to small!");
                  rm_dupcheck_crc(fullname, p_file_name_buffer,
                                  *p_file_size_buffer);
                  (void)smtp_quit();
                  exit(ALLOC_ERROR);
               }
               if (smtp_write(buffer, NULL, length) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Failed to write From to SMTP-server.");
                  rm_dupcheck_crc(fullname, p_file_name_buffer,
                                  *p_file_size_buffer);
                  (void)smtp_quit();
                  exit(eval_timeout(WRITE_REMOTE_ERROR));
               }
               no_of_bytes += length;
            }

            if (db.reply_to != NULL)
            {
               length = snprintf(buffer, buffer_size,
                                 "Reply-To: %s\r\n", db.reply_to);
               if (length >= buffer_size)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Buffer length for mail header to small!");
                  rm_dupcheck_crc(fullname, p_file_name_buffer,
                                  *p_file_size_buffer);
                  (void)smtp_quit();
                  exit(ALLOC_ERROR);
               }
               if (smtp_write(buffer, NULL, length) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Failed to write Reply-To to SMTP-server.");
                  rm_dupcheck_crc(fullname, p_file_name_buffer,
                                  *p_file_size_buffer);
                  (void)smtp_quit();
                  exit(eval_timeout(WRITE_REMOTE_ERROR));
               }
               no_of_bytes += length;
            }

#ifdef _WITH_DE_MAIL_SUPPORT
            if (db.protocol & DE_MAIL_FLAG)
            {
               /* De mail must have a Message-ID. */
               gen_message_id(host_name);
               length = snprintf(buffer, buffer_size,
                                 "Message-ID: %s\r\n", db.message_id);
               if (length >= buffer_size)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Buffer length for Message-ID to small!");
                  rm_dupcheck_crc(fullname, p_file_name_buffer,
                                  *p_file_size_buffer);
                  (void)smtp_quit();
                  exit(ALLOC_ERROR);
               }
               if (smtp_write(buffer, NULL, length) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Failed to write Message-ID to SMTP-server.");
                  rm_dupcheck_crc(fullname, p_file_name_buffer,
                                  *p_file_size_buffer);
                  (void)smtp_quit();
                  exit(eval_timeout(WRITE_REMOTE_ERROR));
               }
               no_of_bytes += length;

               gen_privat_id(host_name);
               length = snprintf(buffer, buffer_size,
                                 "X-de-mail-privat-id: %s\r\n",
                                 db.de_mail_privat_id);
               if (length >= buffer_size)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Buffer length for de-mail header to small!");
                  rm_dupcheck_crc(fullname, p_file_name_buffer,
                                  *p_file_size_buffer);
                  (void)smtp_quit();
                  exit(ALLOC_ERROR);
               }
               if (smtp_write(buffer, NULL, length) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Failed to write de-mail header to SMTP-server.");
                  rm_dupcheck_crc(fullname, p_file_name_buffer,
                                  *p_file_size_buffer);
                  (void)smtp_quit();
                  exit(eval_timeout(WRITE_REMOTE_ERROR));
               }
               no_of_bytes += length;

               if (db.de_mail_options & CONF_OF_DISPATCH)
               {
                  length = snprintf(buffer, buffer_size,
                                    "X-de-mail-confirmation-of-dispatch: yes\r\n");
                  if (length >= buffer_size)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Buffer length for de-mail header to small!");
                     rm_dupcheck_crc(fullname, p_file_name_buffer,
                                     *p_file_size_buffer);
                     (void)smtp_quit();
                     exit(ALLOC_ERROR);
                  }
                  if (smtp_write(buffer, NULL, length) < 0)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Failed to write de-mail header to SMTP-server.");
                     rm_dupcheck_crc(fullname, p_file_name_buffer,
                                     *p_file_size_buffer);
                     (void)smtp_quit();
                     exit(eval_timeout(WRITE_REMOTE_ERROR));
                  }
                  no_of_bytes += length;
               }

               if (db.de_mail_options & CONF_OF_RECEIPT)
               {
                  length = snprintf(buffer, buffer_size,
                                    "X-de-mail-confirmation-of-receipt: yes\r\n");
                  if (length >= buffer_size)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Buffer length for de-mail confirmation-of-receipt to small!");
                     rm_dupcheck_crc(fullname, p_file_name_buffer,
                                     *p_file_size_buffer);
                     (void)smtp_quit();
                     exit(ALLOC_ERROR);
                  }
                  if (smtp_write(buffer, NULL, length) < 0)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Failed to write de-mail confirmation-of-receipt to SMTP-server.");
                     rm_dupcheck_crc(fullname, p_file_name_buffer,
                                     *p_file_size_buffer);
                     (void)smtp_quit();
                     exit(eval_timeout(WRITE_REMOTE_ERROR));
                  }
                  no_of_bytes += length;
               }

               if (db.de_mail_options & CONF_OF_RETRIEVE)
               {
                  length = snprintf(buffer, buffer_size,
                                    "X-de-mail-confirmation-of-retrieve: yes\r\n");
                  if (length >= buffer_size)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Buffer length for de-mail confirmation-of-retrieve to small!");
                     rm_dupcheck_crc(fullname, p_file_name_buffer,
                                     *p_file_size_buffer);
                     (void)smtp_quit();
                     exit(ALLOC_ERROR);
                  }
                  if (smtp_write(buffer, NULL, length) < 0)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Failed to write de-mail confirmation-of-retrieve to SMTP-server.");
                     rm_dupcheck_crc(fullname, p_file_name_buffer,
                                     *p_file_size_buffer);
                     (void)smtp_quit();
                     exit(eval_timeout(WRITE_REMOTE_ERROR));
                  }
                  no_of_bytes += length;
               }
            }
#endif

            /* Send file name as subject if wanted. */
            if (db.special_flag & MAIL_SUBJECT)
            {
               if (db.filename_pos_subject == -1)
               {
                  length = snprintf(buffer, buffer_size, "%s", db.subject);
                  if (length >= buffer_size)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Buffer length for mail header to small!");
                     rm_dupcheck_crc(fullname, p_file_name_buffer,
                                     *p_file_size_buffer);
                     (void)smtp_quit();
                     exit(ALLOC_ERROR);
                  }
               }
               else
               {
                  db.subject[db.filename_pos_subject] = '\0';
                  length = snprintf(buffer, buffer_size, "%s", db.subject);
                  if (length >= buffer_size)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Buffer length for mail header to small!");
                     rm_dupcheck_crc(fullname, p_file_name_buffer,
                                     *p_file_size_buffer);
                     (void)smtp_quit();
                     exit(ALLOC_ERROR);
                  }
                  if (db.subject_rename_rule[0] == '\0')
                  {
                     if ((length + strlen(final_filename) + 2) < buffer_size)
                     {
                        length += snprintf(&buffer[length], buffer_size,
                                           "%s", final_filename);
                        if (length >= buffer_size)
                        {
                           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                     "Buffer length for mail header to small!");
                           rm_dupcheck_crc(fullname, p_file_name_buffer,
                                           *p_file_size_buffer);
                           (void)smtp_quit();
                           exit(ALLOC_ERROR);
                        }
                     }
                  }
                  else
                  {
                     int  k;
                     char new_filename[MAX_FILENAME_LENGTH];

                     new_filename[0] = '\0';
                     for (k = 0; k < rule[db.subject_rule_pos].no_of_rules; k++)
                     {
                        if (pmatch(rule[db.subject_rule_pos].filter[k],
                                   final_filename, NULL) == 0)
                        {
                           change_name(final_filename,
                                       rule[db.subject_rule_pos].filter[k],
                                       rule[db.subject_rule_pos].rename_to[k],
                                       new_filename, MAX_FILENAME_LENGTH,
                                       &counter_fd, &unique_counter, db.id.job);
                           break;
                        }
                     }
                     if (new_filename[0] == '\0')
                     {
                        (void)my_strncpy(new_filename, final_filename,
                                         MAX_FILENAME_LENGTH);
                     }
                     if ((length + strlen(new_filename) + 2) < buffer_size)
                     {
                        length += snprintf(&buffer[length], buffer_size,
                                           "%s", new_filename);
                        if (length >= buffer_size)
                        {
                           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                     "Buffer length for mail header to small!");
                           rm_dupcheck_crc(fullname, p_file_name_buffer,
                                           *p_file_size_buffer);
                           (void)smtp_quit();
                           exit(ALLOC_ERROR);
                        }
                     }
                  }
                  if ((db.special_flag & ATTACH_ALL_FILES) &&
                      (files_to_send > 1))
                  {
                     int  filenames_to_add;
                     char *p_tmp_file_name_buffer;

                     p_tmp_file_name_buffer = file_name_buffer + MAX_FILENAME_LENGTH;
                     for (filenames_to_add = 1;
                          filenames_to_add < files_to_send; filenames_to_add++)
                     {
                        if (db.subject_rename_rule[0] == '\0')
                        {
                           if ((length + 2 + strlen(p_tmp_file_name_buffer) + 2) < buffer_size)
                           {
                              length += snprintf(&buffer[length], buffer_size,
                                                 ", %s",
                                                 p_tmp_file_name_buffer);
                              if (length >= buffer_size)
                              {
                                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                           "Buffer length for mail header to small!");
                                 rm_dupcheck_crc(fullname, p_file_name_buffer,
                                                 *p_file_size_buffer);
                                 (void)smtp_quit();
                                 exit(ALLOC_ERROR);
                              }
                           }
                           else
                           {
                              filenames_to_add = files_to_send;
                           }
                        }
                        else
                        {
                           int  k;
                           char new_filename[MAX_FILENAME_LENGTH];

                           new_filename[0] = '\0';
                           for (k = 0; k < rule[db.subject_rule_pos].no_of_rules; k++)
                           {
                              if (pmatch(rule[db.subject_rule_pos].filter[k],
                                         p_tmp_file_name_buffer, NULL) == 0)
                              {
                                 change_name(p_tmp_file_name_buffer,
                                             rule[db.subject_rule_pos].filter[k],
                                             rule[db.subject_rule_pos].rename_to[k],
                                             new_filename, MAX_FILENAME_LENGTH,
                                             &counter_fd, &unique_counter,
                                             db.id.job);
                                 break;
                              }
                           }
                           if (new_filename[0] == '\0')
                           {
                              (void)my_strncpy(new_filename, p_tmp_file_name_buffer,
                                               MAX_FILENAME_LENGTH);
                           }
                           if ((length + 2 + strlen(new_filename) + 2) < buffer_size)
                           {
                              length += snprintf(&buffer[length], buffer_size,
                                                 ", %s", new_filename);
                              if (length >= buffer_size)
                              {
                                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                           "Buffer length for mail header to small!");
                                 rm_dupcheck_crc(fullname, p_file_name_buffer,
                                                 *p_file_size_buffer);
                                 (void)smtp_quit();
                                 exit(ALLOC_ERROR);
                              }
                           }
                           else
                           {
                              filenames_to_add = files_to_send;
                           }
                        }
                        p_tmp_file_name_buffer += MAX_FILENAME_LENGTH;
                     }
                  }
                  if (db.subject[db.filename_pos_subject + 2] != '\0')
                  {
                     if ((length + strlen(&db.subject[db.filename_pos_subject + 2]) + 2) < buffer_size)
                     {
                        length += snprintf(&buffer[length], buffer_size, "%s",
                                           &db.subject[db.filename_pos_subject + 2]);
                        if (length >= buffer_size)
                        {
                           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                     "Buffer length for mail header to small!");
                           rm_dupcheck_crc(fullname, p_file_name_buffer,
                                           *p_file_size_buffer);
                           (void)smtp_quit();
                           exit(ALLOC_ERROR);
                        }
                     }
                  }
                  db.subject[db.filename_pos_subject] = '%';
               }
               if (smtp_write_subject(buffer, &length, (db.charset == NULL) ? db.default_charset : db.charset) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Failed to write subject to SMTP-server.");
                  rm_dupcheck_crc(fullname, p_file_name_buffer,
                                  *p_file_size_buffer);
                  (void)smtp_quit();
                  exit(eval_timeout(WRITE_REMOTE_ERROR));
               }
               no_of_bytes += length;
            }
            else if (db.special_flag & FILE_NAME_IS_SUBJECT)
                 {
                    length = strlen(final_filename);
                    if (smtp_write_subject(final_filename, &length, (db.charset == NULL) ? db.default_charset : db.charset) < 0)
                    {
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                 "Failed to write the filename as subject to SMTP-server.");
                       rm_dupcheck_crc(fullname, p_file_name_buffer,
                                       *p_file_size_buffer);
                       (void)smtp_quit();
                       exit(eval_timeout(WRITE_REMOTE_ERROR));
                    }
                    no_of_bytes += length;
                 }

            if (db.group_list == NULL)
            {
               length = snprintf(buffer, buffer_size,
                                 "To: %s\r\n", remote_user);
               if (length >= buffer_size)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Buffer length for mail header to small!");
                  rm_dupcheck_crc(fullname, p_file_name_buffer,
                                  *p_file_size_buffer);
                  (void)smtp_quit();
                  exit(ALLOC_ERROR);
               }
            }
            else
            {
               if (((db.special_flag & SHOW_ALL_GROUP_MEMBERS) == 0) &&
                   ((db.special_flag & HIDE_ALL_GROUP_MEMBERS) == 0))
               {
                  char *p_group_name;

                  if (p_db->user[0] == MAIL_GROUP_IDENTIFIER)
                  {
                     p_group_name = &p_db->user[1];
                  }
                  else
                  {
                     p_group_name = p_db->hostname;
                  }

                  if (db.no_listed == 1)
                  {
                     length = snprintf(buffer, buffer_size,
                                       "To: %s\r\n", db.group_list[0]);
                  }
                  else
                  {
                     if (db.special_flag & SMTP_GROUP_NO_TO_LINE)
                     {
                        length = 0;
                     }
                     else
                     {
                        if (db.group_to == NULL)
                        {
                           /* This is tricky: To: group name */
                           if (p_db->group_mail_domain == NULL)
                           {
                              length = snprintf(buffer, buffer_size,
                                                "To: %s\r\n", p_group_name);
                           }
                           else
                           {
                              length = snprintf(buffer, buffer_size,
                                                "To: %s@%s\r\n",
                                                p_group_name,
                                                p_db->group_mail_domain);
                           }
                        }
                        else
                        {
                           length = snprintf(buffer, buffer_size,
                                             "To: %s\r\n", db.group_to);
                        }
                     }
                  }
                  if (length >= buffer_size)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Buffer length for mail header to small!");
                     rm_dupcheck_crc(fullname, p_file_name_buffer,
                                     *p_file_size_buffer);
                     (void)smtp_quit();
                     exit(ALLOC_ERROR);
                  }
               }
               else
               {
                  int k;

                  if  (db.special_flag & HIDE_ALL_GROUP_MEMBERS)
                  {
                     length = snprintf(buffer, buffer_size,
                                       "To: %s\r\nBcc: %s",
                                       db.group_list[0], db.group_list[0]);
                  }
                  else
                  {
                     length = snprintf(buffer, buffer_size,
                                    "To: %s", db.group_list[0]);
                  }
                  if (length >= buffer_size)
                  {
                     buffer_size += MAX_RECIPIENT_LENGTH;
                     if ((buffer = realloc(buffer, buffer_size)) == NULL)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  "Failed to realloc() %d bytes : %s",
                                  buffer_size, strerror(errno));
                        rm_dupcheck_crc(fullname, p_file_name_buffer,
                                        *p_file_size_buffer);
                        (void)smtp_quit();
                        exit(ALLOC_ERROR);
                     }
                     if  (db.special_flag & HIDE_ALL_GROUP_MEMBERS)
                     {
                        length = snprintf(buffer, buffer_size,
                                          "To: %s\r\nBcc: %s",
                                          db.group_list[0], db.group_list[0]);
                     }
                     else
                     {
                        length = snprintf(buffer, buffer_size,
                                          "To: %s", db.group_list[0]);
                     }
                     if (length >= buffer_size)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  "Buffer length for mail header to small (%d)!",
                                  buffer_size);
                        rm_dupcheck_crc(fullname, p_file_name_buffer,
                                        *p_file_size_buffer);
                        (void)smtp_quit();
                        exit(ALLOC_ERROR);
                     }
                  }
                  for (k = 1; k < db.no_listed; k++)
                  {
                     if ((length + MAX_RECIPIENT_LENGTH) > buffer_size)
                     {
                        buffer_size += MAX_RECIPIENT_LENGTH;
                        if ((buffer = realloc(buffer, buffer_size)) == NULL)
                        {
                           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                     "Failed to realloc() %d bytes : %s",
                                     buffer_size, strerror(errno));
                           rm_dupcheck_crc(fullname, p_file_name_buffer,
                                           *p_file_size_buffer);
                           (void)smtp_quit();
                           exit(ALLOC_ERROR);
                        }
                     }
                     length += snprintf(buffer + length, buffer_size - length,
                                        ", %s", db.group_list[k]);
                     if (length >= buffer_size)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  "Buffer length for mail header to small (%d)!",
                                  buffer_size);
                        rm_dupcheck_crc(fullname, p_file_name_buffer,
                                        *p_file_size_buffer);
                        (void)smtp_quit();
                        exit(ALLOC_ERROR);
                     }
                  }
                  if ((length + 2) > buffer_size)
                  {
                     buffer_size = length + 2;
                     if ((buffer = realloc(buffer, buffer_size)) == NULL)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  "Failed to realloc() %d bytes : %s",
                                  buffer_size, strerror(errno));
                        rm_dupcheck_crc(fullname, p_file_name_buffer,
                                        *p_file_size_buffer);
                        (void)smtp_quit();
                        exit(ALLOC_ERROR);
                     }
                  }
                  buffer[length] = '\r';
                  buffer[length + 1] = '\n';
                  length += 2;
               }
            }
            if (length > 0)
            {
               if (smtp_write(buffer, NULL, length) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Failed to write To header to SMTP-server.");
                  rm_dupcheck_crc(fullname, p_file_name_buffer,
                                  *p_file_size_buffer);
                  (void)smtp_quit();
                  exit(eval_timeout(WRITE_REMOTE_ERROR));
               }
               no_of_bytes += length;
            }

            /* Send MIME information. */
            if (db.special_flag & ATTACH_FILE)
            {
               if (multipart_boundary[0] != '\0')
               {
                  length = snprintf(buffer, buffer_size,
                                    "MIME-Version: 1.0 (produced by AFD %s)\r\nContent-Type: multipart/mixed; boundary=\"%s\"\r\n",
                                    PACKAGE_VERSION, multipart_boundary);
                  if (length >= buffer_size)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Buffer length for mail header to small (%d)!",
                               buffer_size);
                     rm_dupcheck_crc(fullname, p_file_name_buffer,
                                     *p_file_size_buffer);
                     (void)smtp_quit();
                     exit(ALLOC_ERROR);
                  }
                  buffer_ptr = buffer;
               }
               else
               {
                  if (db.trans_rename_rule[0] != '\0')
                  {
                     int  k;
                     char content_type[MAX_CONTENT_TYPE_LENGTH],
                          new_filename[MAX_FILENAME_LENGTH];

                     new_filename[0] = '\0';
                     for (k = 0; k < rule[db.trans_rule_pos].no_of_rules; k++)
                     {
                        if (pmatch(rule[db.trans_rule_pos].filter[k],
                                   final_filename, NULL) == 0)
                        {
                           change_name(final_filename,
                                       rule[db.trans_rule_pos].filter[k],
                                       rule[db.trans_rule_pos].rename_to[k],
                                       new_filename, MAX_FILENAME_LENGTH,
                                       &counter_fd, &unique_counter, db.id.job);
                           break;
                        }
                     }
                     if (new_filename[0] == '\0')
                     {
                        (void)my_strncpy(new_filename, final_filename,
                                         MAX_FILENAME_LENGTH);
                     }
                     get_content_type(new_filename, content_type, YES);
                     length = snprintf(encode_buffer, encode_buffer_size,
                                       "MIME-Version: 1.0 (produced by AFD %s)\r\nContent-Type: %s; name=\"%s\"\r\nContent-Transfer-Encoding: BASE64\r\nContent-Disposition: attachment; filename=\"%s\"\r\n",
                                       PACKAGE_VERSION, content_type,
                                       new_filename, new_filename);
                  }
                  else
                  {
                     char content_type[MAX_CONTENT_TYPE_LENGTH];

                     get_content_type(final_filename, content_type, YES);
                     length = snprintf(encode_buffer, encode_buffer_size,
                                       "MIME-Version: 1.0 (produced by AFD %s)\r\nContent-Type: %s; name=\"%s\"\r\nContent-Transfer-Encoding: BASE64\r\nContent-Disposition: attachment; filename=\"%s\"\r\n",
                                       PACKAGE_VERSION, content_type,
                                       final_filename, final_filename);
                  }
                  if (length >= encode_buffer_size)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Buffer length for mail header to small (%d)!",
                               buffer_size);
                     rm_dupcheck_crc(fullname, p_file_name_buffer,
                                     *p_file_size_buffer);
                     (void)smtp_quit();
                     exit(ALLOC_ERROR);
                  }
                  buffer_ptr = encode_buffer;
               }
               added_content_type = YES;

               if (smtp_write(buffer_ptr, NULL, length) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Failed to write start of multipart boundary to SMTP-server.");
                  rm_dupcheck_crc(fullname, p_file_name_buffer,
                                  *p_file_size_buffer);
                  (void)smtp_quit();
                  exit(eval_timeout(WRITE_REMOTE_ERROR));
               }
               no_of_bytes += length;
            } /* if (db.special_flag & ATTACH_FILE) */
            else if ((db.charset != NULL) || (db.default_charset != NULL))
                 {
                    length = snprintf(buffer, buffer_size,
                                      "MIME-Version: 1.0 (produced by AFD %s)\r\nContent-Type: TEXT/plain; charset=%s\r\nContent-Transfer-Encoding: 8BIT\r\n",
                                      PACKAGE_VERSION,
                                      (db.charset == NULL) ? db.default_charset : db.charset);
                    if (length >= buffer_size)
                    {
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                 "Buffer length for mail header to small (%d)!",
                                 buffer_size);
                       rm_dupcheck_crc(fullname, p_file_name_buffer,
                                       *p_file_size_buffer);
                       (void)smtp_quit();
                       exit(ALLOC_ERROR);
                    }
                    added_content_type = YES;

                    if (smtp_write(buffer, NULL, length) < 0)
                    {
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                 "Failed to write MIME header with charset to SMTP-server.");
                       rm_dupcheck_crc(fullname, p_file_name_buffer,
                                       *p_file_size_buffer);
                       (void)smtp_quit();
                       exit(eval_timeout(WRITE_REMOTE_ERROR));
                    }
                    no_of_bytes += length;
                 }

            /* Write the mail header. */
            if (mail_header_buffer != NULL)
            {
               length = 0;

               if (db.special_flag & ATTACH_FILE)
               {
                  /* Write boundary. */
                  if ((db.charset == NULL) && (db.default_charset == NULL))
                  {
                     length = snprintf(encode_buffer, encode_buffer_size,
                                       "\r\n--%s\r\nContent-Type: TEXT/plain; charset=US-ASCII\r\n\r\n",
                                       multipart_boundary);
                  }
                  else
                  {
                     length = snprintf(encode_buffer, encode_buffer_size,
                                       "\r\n--%s\r\nContent-Type: TEXT/plain; charset=%s\r\nContent-Transfer-Encoding: 8BIT\r\n\r\n",
                                       multipart_boundary,
                                       (db.charset == NULL) ? db.default_charset : db.charset);
                  }
                  if (length >= encode_buffer_size)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Buffer length for mail header to small (%d)!",
                               encode_buffer_size);
                     rm_dupcheck_crc(fullname, p_file_name_buffer,
                                     *p_file_size_buffer);
                     (void)smtp_quit();
                     exit(ALLOC_ERROR);
                  }
                  added_content_type = YES;

                  if (smtp_write(encode_buffer, NULL, length) < 0)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Failed to write the Content-Type (TEXT/plain) to SMTP-server.");
                     rm_dupcheck_crc(fullname, p_file_name_buffer,
                                     *p_file_size_buffer);
                     (void)smtp_quit();
                     exit(eval_timeout(WRITE_REMOTE_ERROR));
                  }
                  no_of_bytes += length;
               } /* if (db.special_flag & ATTACH_FILE) */

               /* Now lets write the message. */
               extra_mail_header_buffer[0] = '\n';
               if (db.special_flag & ENCODE_ANSI)
               {
                  if (smtp_write_iso8859(mail_header_buffer,
                                         extra_mail_header_buffer,
                                         mail_header_size) < 0)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Failed to write the mail header content to SMTP-server.");
                     rm_dupcheck_crc(fullname, p_file_name_buffer,
                                     *p_file_size_buffer);
                     (void)smtp_quit();
                     exit(eval_timeout(WRITE_REMOTE_ERROR));
                  }
               }
               else
               {
                  if (smtp_write(mail_header_buffer, extra_mail_header_buffer,
                                 mail_header_size) < 0)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Failed to write the mail header content to SMTP-server.");
                     rm_dupcheck_crc(fullname, p_file_name_buffer,
                                     *p_file_size_buffer);
                     (void)smtp_quit();
                     exit(eval_timeout(WRITE_REMOTE_ERROR));
                  }
               }
               no_of_bytes += length;

               if (db.special_flag & ATTACH_FILE)
               {
                  /* Write boundary. */
                  if (db.trans_rename_rule[0] != '\0')
                  {
                     int  k;
                     char content_type[MAX_CONTENT_TYPE_LENGTH],
                          new_filename[MAX_FILENAME_LENGTH];

                     new_filename[0] = '\0';
                     for (k = 0; k < rule[db.trans_rule_pos].no_of_rules; k++)
                     {
                        if (pmatch(rule[db.trans_rule_pos].filter[k],
                                   final_filename, NULL) == 0)
                        {
                           change_name(final_filename,
                                       rule[db.trans_rule_pos].filter[k],
                                       rule[db.trans_rule_pos].rename_to[k],
                                       new_filename, MAX_FILENAME_LENGTH,
                                       &counter_fd, &unique_counter, db.id.job);
                           break;
                        }
                     }
                     if (new_filename[0] == '\0')
                     {
                        (void)my_strncpy(new_filename, final_filename,
                                         MAX_FILENAME_LENGTH);
                     }
                     get_content_type(new_filename, content_type, YES);
                     length = snprintf(encode_buffer, encode_buffer_size,
                                       "\r\n--%s\r\nContent-Type: %s; name=\"%s\"\r\nContent-Transfer-Encoding: BASE64\r\nContent-Disposition: attachment; filename=\"%s\"\r\n",
                                       multipart_boundary, content_type,
                                       new_filename, new_filename);
                  }
                  else
                  {
                     char content_type[MAX_CONTENT_TYPE_LENGTH];

                     get_content_type(final_filename, content_type, YES);
                     length = snprintf(encode_buffer, encode_buffer_size,
                                       "\r\n--%s\r\nContent-Type: %s; name=\"%s\"\r\nContent-Transfer-Encoding: BASE64\r\nContent-Disposition: attachment; filename=\"%s\"\r\n",
                                       multipart_boundary, content_type,
                                       final_filename, final_filename);
                  }
                  added_content_type = YES;
                  if (length >= encode_buffer_size)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Buffer length for mail header to small (%d)!",
                               encode_buffer_size);
                     rm_dupcheck_crc(fullname, p_file_name_buffer,
                                     *p_file_size_buffer);
                     (void)smtp_quit();
                     exit(ALLOC_ERROR);
                  }

                  if (smtp_write(encode_buffer, NULL, length) < 0)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Failed to write the Content-Type to SMTP-server.");
                     rm_dupcheck_crc(fullname, p_file_name_buffer,
                                     *p_file_size_buffer);
                     (void)smtp_quit();
                     exit(eval_timeout(WRITE_REMOTE_ERROR));
                  }
                  no_of_bytes += length;
               } /* if (db.special_flag & ATTACH_FILE) */
            } /* if (mail_header_buffer != NULL) */
            else if (added_content_type == NO)
                 {
                    char content_type[MAX_CONTENT_TYPE_LENGTH],
                         add_header[14 + MAX_CONTENT_TYPE_LENGTH + 2 + 1];

                    /* Write Content Type. */
                    if (db.trans_rename_rule[0] != '\0')
                    {
                       int  k;
                       char new_filename[MAX_FILENAME_LENGTH];

                       new_filename[0] = '\0';
                       for (k = 0; k < rule[db.trans_rule_pos].no_of_rules; k++)
                       {
                          if (pmatch(rule[db.trans_rule_pos].filter[k],
                                     final_filename, NULL) == 0)
                          {
                             change_name(final_filename,
                                         rule[db.trans_rule_pos].filter[k],
                                         rule[db.trans_rule_pos].rename_to[k],
                                         new_filename, MAX_FILENAME_LENGTH,
                                         &counter_fd, &unique_counter, db.id.job);
                             break;
                          }
                       }
                       if (new_filename[0] == '\0')
                       {
                          (void)my_strncpy(new_filename, final_filename,
                                           MAX_FILENAME_LENGTH);
                       }
                       get_content_type(new_filename, content_type, NO);
                    }
                    else
                    {
                       get_content_type(final_filename, content_type, NO);
                    }
                    length = snprintf(add_header, 14 + MAX_CONTENT_TYPE_LENGTH + 2 + 1,
                                      "Content-Type: %s\r\n",
                                      content_type);
                    if (length >= (14 + MAX_CONTENT_TYPE_LENGTH + 2 + 1))
                    {
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
#if SIZEOF_SIZE_T == 4
                                 "Buffer length for content type to small (%d > %d)!",
#else
                                 "Buffer length for content type to small (%lld > %d)!",
#endif
                                 (pri_size_t)length,
                                 (14 + MAX_CONTENT_TYPE_LENGTH + 2 + 1));
                        rm_dupcheck_crc(fullname, p_file_name_buffer,
                                        *p_file_size_buffer);
                       (void)smtp_quit();
                       exit(ALLOC_ERROR);
                    }

                    if (smtp_write(add_header, NULL, length) < 0)
                    {
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                 "Failed to write the Content-Type to SMTP-server.");
                        rm_dupcheck_crc(fullname, p_file_name_buffer,
                                        *p_file_size_buffer);
                       (void)smtp_quit();
                       exit(eval_timeout(WRITE_REMOTE_ERROR));
                    }
                    no_of_bytes += length;
                 }

#ifdef WITH_MAILER_IDENTIFIER
            /* Write mailer name. */
            length = snprintf(buffer, buffer_size, "X-Mailer: AFD %s\r\n", PACKAGE_VERSION);
            if (length >= buffer_size)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Buffer length for mail header to small (%d)!",
                         buffer_size);
               rm_dupcheck_crc(fullname, p_file_name_buffer,
                               *p_file_size_buffer);
               (void)smtp_quit();
               exit(ALLOC_ERROR);
            }
            if (smtp_write(buffer, NULL, length) < 0)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Failed to write Reply-To to SMTP-server.");
               rm_dupcheck_crc(fullname, p_file_name_buffer,
                               *p_file_size_buffer);
               (void)smtp_quit();
               exit(eval_timeout(WRITE_REMOTE_ERROR));
            }
            no_of_bytes += length;
#endif

            /*
             * We need a second CRLF to indicate end of header. The stuff
             * that follows is the message body.
             */
            if (smtp_write("\r\n", NULL, 2) < 0)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Failed to write second CRLF to indicate end of header.");
               rm_dupcheck_crc(fullname, p_file_name_buffer,
                               *p_file_size_buffer);
               (void)smtp_quit();
               exit(eval_timeout(WRITE_REMOTE_ERROR));
            }
         } /* if (((db.special_flag & ATTACH_ALL_FILES) == 0) || (files_send == 0)) */

         if ((db.special_flag & ATTACH_ALL_FILES) &&
             ((mail_header_buffer == NULL) || (files_send != 0)))
         {
            int  length = 0;
            char content_type[MAX_CONTENT_TYPE_LENGTH];

            /* Write boundary. */
            if (db.trans_rename_rule[0] != '\0')
            {
               int  k;
               char new_filename[MAX_FILENAME_LENGTH];

               new_filename[0] = '\0';
               for (k = 0; k < rule[db.trans_rule_pos].no_of_rules; k++)
               {
                  if (pmatch(rule[db.trans_rule_pos].filter[k],
                             final_filename, NULL) == 0)
                  {
                     change_name(final_filename,
                                 rule[db.trans_rule_pos].filter[k],
                                 rule[db.trans_rule_pos].rename_to[k],
                                 new_filename, MAX_FILENAME_LENGTH,
                                 &counter_fd, &unique_counter, db.id.job);
                     break;
                  }
               }
               if (new_filename[0] == '\0')
               {
                  (void)my_strncpy(new_filename, final_filename,
                                   MAX_FILENAME_LENGTH);
               }
               get_content_type(new_filename, content_type, YES);
               if (files_send == 0)
               {
#ifdef WITH_MAILER_IDENTIFIER
                  length = snprintf(encode_buffer, encode_buffer_size,
                                    "\r\n--%s\r\nContent-Type: %s; name=\"%s\"\r\nContent-Transfer-Encoding: BASE64\r\nContent-Disposition: attachment; filename=\"%s\"\r\nX-Mailer: AFD %s\r\n\r\n",
                                    multipart_boundary, content_type,
                                    new_filename, new_filename,
                                    PACKAGE_VERSION);
#else
                  length = snprintf(encode_buffer, encode_buffer_size,
                                    "\r\n--%s\r\nContent-Type: %s; name=\"%s\"\r\nContent-Transfer-Encoding: BASE64\r\nContent-Disposition: attachment; filename=\"%s\"\r\n\r\n",
                                    multipart_boundary, content_type,
                                    new_filename, new_filename);
#endif
               }
               else
               {
#ifdef WITH_MAILER_IDENTIFIER
                  length = snprintf(encode_buffer, encode_buffer_size,
                                    "\r\n\r\n--%s\r\nContent-Type: %s; name=\"%s\"\r\nContent-Transfer-Encoding: BASE64\r\nContent-Disposition: attachment; filename=\"%s\"\r\nX-Mailer: AFD %s\r\n\r\n",
                                    multipart_boundary, content_type,
                                    new_filename, new_filename,
                                    PACKAGE_VERSION);
#else
                  length = snprintf(encode_buffer, encode_buffer_size,
                                    "\r\n\r\n--%s\r\nContent-Type: %s; name=\"%s\"\r\nContent-Transfer-Encoding: BASE64\r\nContent-Disposition: attachment; filename=\"%s\"\r\n\r\n",
                                    multipart_boundary, content_type,
                                    new_filename, new_filename);
#endif
               }
            }
            else
            {
               get_content_type(final_filename, content_type, YES);
               if (files_send == 0)
               {
#ifdef WITH_MAILER_IDENTIFIER
                  length = snprintf(encode_buffer, encode_buffer_size,
                                    "\r\n--%s\r\nContent-Type: %s; name=\"%s\"\r\nContent-Transfer-Encoding: BASE64\r\nContent-Disposition: attachment; filename=\"%s\"\r\nX-Mailer: AFD %s\r\n\r\n",
                                    multipart_boundary, content_type,
                                    final_filename, final_filename,
                                    PACKAGE_VERSION);
#else
                  length = snprintf(encode_buffer, encode_buffer_size,
                                    "\r\n--%s\r\nContent-Type: %s; name=\"%s\"\r\nContent-Transfer-Encoding: BASE64\r\nContent-Disposition: attachment; filename=\"%s\"\r\n\r\n",
                                    multipart_boundary, content_type,
                                    final_filename, final_filename);
#endif
               }
               else
               {
#ifdef WITH_MAILER_IDENTIFIER
                  length = snprintf(encode_buffer, encode_buffer_size,
                                    "\r\n\r\n--%s\r\nContent-Type: %s; name=\"%s\"\r\nContent-Transfer-Encoding: BASE64\r\nContent-Disposition: attachment; filename=\"%s\"\r\nX-Mailer: AFD %s\r\n\r\n",
                                    multipart_boundary, content_type,
                                    final_filename, final_filename,
                                    PACKAGE_VERSION);
#else
                  length = snprintf(encode_buffer, encode_buffer_size,
                                    "\r\n\r\n--%s\r\nContent-Type: %s; name=\"%s\"\r\nContent-Transfer-Encoding: BASE64\r\nContent-Disposition: attachment; filename=\"%s\"\r\n\r\n",
                                    multipart_boundary, content_type,
                                    final_filename, final_filename);
#endif
               }
            }
            if (length >= encode_buffer_size)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Buffer length for mail header to small (%d)!",
                         buffer_size);
               rm_dupcheck_crc(fullname, p_file_name_buffer,
                               *p_file_size_buffer);
               (void)smtp_quit();
               exit(ALLOC_ERROR);
            }

            if (smtp_write(encode_buffer, NULL, length) < 0)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Failed to write the Content-Type to SMTP-server.");
               rm_dupcheck_crc(fullname, p_file_name_buffer,
                               *p_file_size_buffer);
               (void)smtp_quit();
               exit(eval_timeout(WRITE_REMOTE_ERROR));
            }

            no_of_bytes += length;
         }
         if (smtp_buffer != NULL)
         {
            smtp_buffer[0] = '\n';
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
#ifdef _SIMULATE_SLOW_TRANSFER
            (void)sleep(_SIMULATE_SLOW_TRANSFER);
#endif
            for (j = 0; j < loops; j++)
            {
               if (read(fd, buffer, blocksize) != blocksize)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Failed to read() %s : %s",
                            fullname, strerror(errno));
                  rm_dupcheck_crc(fullname, p_file_name_buffer,
                                  *p_file_size_buffer);
                  (void)smtp_close();
                  (void)smtp_quit();
                  exit(READ_LOCAL_ERROR);
               }
               if (db.special_flag & ATTACH_FILE)
               {
                  write_size = encode_base64((unsigned char *)buffer,
                                             blocksize,
                                             (unsigned char *)encode_buffer,
                                             YES);
                  if (smtp_write(encode_buffer, NULL, write_size) < 0)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Failed to write data from the source file to the SMTP-server.");
                     rm_dupcheck_crc(fullname, p_file_name_buffer,
                                     *p_file_size_buffer);
                     (void)smtp_quit();
                     exit(eval_timeout(WRITE_REMOTE_ERROR));
                  }
               }
               else
               {
                  if (db.special_flag & ENCODE_ANSI)
                  {
                     if (smtp_write_iso8859(buffer, smtp_buffer, blocksize) < 0)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  "Failed to write data from the source file to the SMTP-server.");
                        rm_dupcheck_crc(fullname, p_file_name_buffer,
                                        *p_file_size_buffer);
                        (void)smtp_quit();
                        exit(eval_timeout(WRITE_REMOTE_ERROR));
                     }
                  }
                  else
                  {
                     if (smtp_write(buffer, smtp_buffer, blocksize) < 0)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  "Failed to write data from the source file to the SMTP-server.");
                        rm_dupcheck_crc(fullname, p_file_name_buffer,
                                        *p_file_size_buffer);
                        (void)smtp_quit();
                        exit(eval_timeout(WRITE_REMOTE_ERROR));
                     }
                  }
                  write_size = blocksize;
               }
               if (fsa->trl_per_process > 0)
               {
                  limit_transfer_rate(write_size, fsa->trl_per_process, clktck);
               }

               no_of_bytes += write_size;

               if (gsf_check_fsa(p_db) != NEITHER)
               {
                  fsa->job_status[(int)db.job_no].file_size_in_use_done = no_of_bytes;
                  fsa->job_status[(int)db.job_no].file_size_done += write_size;
                  fsa->job_status[(int)db.job_no].bytes_send += write_size;
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
                           (void)smtp_quit();
                           exitflag = 0;
                           exit(STILL_FILES_TO_SEND);
                        }
                     }
                  }
               }
            }
            if (rest > 0)
            {
               if (read(fd, buffer, rest) != rest)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Failed to read() rest from %s : %s",
                            fullname, strerror(errno));
                  rm_dupcheck_crc(fullname, p_file_name_buffer,
                                  *p_file_size_buffer);
                  (void)smtp_close();
                  (void)smtp_quit();
                  exit(READ_LOCAL_ERROR);
               }
               if (db.special_flag & ATTACH_FILE)
               {
                  write_size = encode_base64((unsigned char *)buffer, rest,
                                             (unsigned char *)encode_buffer,
                                             YES);
                  if (smtp_write(encode_buffer, NULL, write_size) < 0)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Failed to write the rest data from the source file to the SMTP-server.");
                     rm_dupcheck_crc(fullname, p_file_name_buffer,
                                     *p_file_size_buffer);
                     (void)smtp_quit();
                     exit(eval_timeout(WRITE_REMOTE_ERROR));
                  }
               }
               else
               {
                  if (db.special_flag & ENCODE_ANSI)
                  {
                     if (smtp_write_iso8859(buffer, smtp_buffer, rest) < 0)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  "Failed to write the rest data from the source file to the SMTP-server.");
                        rm_dupcheck_crc(fullname, p_file_name_buffer,
                                        *p_file_size_buffer);
                        (void)smtp_quit();
                        exit(eval_timeout(WRITE_REMOTE_ERROR));
                     }
                  }
                  else
                  {
                     if (smtp_write(buffer, smtp_buffer, rest) < 0)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  "Failed to write the rest data from the source file to the SMTP-server.");
                        rm_dupcheck_crc(fullname, p_file_name_buffer,
                                        *p_file_size_buffer);
                        (void)smtp_quit();
                        exit(eval_timeout(WRITE_REMOTE_ERROR));
                     }
                  }
                  write_size = rest;
               }
               if (fsa->trl_per_process > 0)
               {
                  limit_transfer_rate(write_size, fsa->trl_per_process, clktck);
               }

               no_of_bytes += write_size;

               if (gsf_check_fsa(p_db) != NEITHER)
               {
                  fsa->job_status[(int)db.job_no].file_size_in_use_done = no_of_bytes;
                  fsa->job_status[(int)db.job_no].file_size_done += write_size;
                  fsa->job_status[(int)db.job_no].bytes_send += write_size;
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
                         "Hmmm. Failed to statx() %s : %s (%s %d)\n",
#else
                         "Hmmm. Failed to fstat() %s : %s (%s %d)\n",
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
                   * Give a warning in the receive log, so some action
                   * can be taken against the originator.
                   */
                  receive_log(sign, __FILE__, __LINE__, 0L, db.id.job,
                              "File %s for host %s was DEFINITELY send without any locking. #%x",
                              final_filename, fsa->host_dsp_name, db.id.job);
               }
               else
               {
                  break;
               }
            }
         } /* for (;;) */

         /* Write boundary end if necessary. */
         if (((db.special_flag & ATTACH_ALL_FILES) == 0) ||
             (files_send == (files_to_send - 1)))
         {
            if ((db.special_flag & ATTACH_FILE) && (multipart_boundary[0] != '\0'))
            {
               int length;

               /* Write boundary. */
               length = snprintf(buffer, buffer_size, "\r\n--%s--\r\n", multipart_boundary);
               if (length >= buffer_size)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Buffer length for mail header to small (%d)!",
                            buffer_size);
                  rm_dupcheck_crc(fullname, p_file_name_buffer,
                                  *p_file_size_buffer);
                  (void)smtp_quit();
                  exit(ALLOC_ERROR);
               }
               if (smtp_write(buffer, NULL, length) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Failed to write end of multipart boundary to SMTP-server.");
                  rm_dupcheck_crc(fullname, p_file_name_buffer,
                                  *p_file_size_buffer);
                  (void)smtp_quit();
                  exit(eval_timeout(WRITE_REMOTE_ERROR));
               }

               no_of_bytes += length;
            } /* if ((db.special_flag & ATTACH_FILE) && (multipart_boundary[0] != '\0')) */
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
                      final_filename, strerror(errno), __FILE__, __LINE__);
            /*
             * Since we usually do not send more then 100 files and
             * sf_smtp() will exit(), there is no point in stopping
             * the transmission.
             */
         }

         if (((db.special_flag & ATTACH_ALL_FILES) == 0) ||
             (files_send == (files_to_send - 1)))
         {
            /* Close remote file. */
            if ((status = smtp_close()) != SUCCESS)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         "Failed to close data mode (%d).", status);
               rm_dupcheck_crc(fullname, p_file_name_buffer,
                               *p_file_size_buffer);
               (void)smtp_quit();
               exit(eval_timeout(CLOSE_REMOTE_ERROR));
            }
            else
            {
               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                               "Closing data mode.");
               }

#ifdef _OUTPUT_LOG
               /*
                * Try get queue ID under which the server has queued
                * the mail. Unfortunatly I do not know if there is some
                * standard way, so lets first implement the one we
                * know: 250 2.0.0 Ok: queued as 79095820F6<0D><0A>
                        250 Ok: queued as 79095820F6<0D><0A>
                */
               if ((msg_str[0] == '2') && (msg_str[1] == '5') &&
                   (msg_str[2] == '0') && (msg_str[3] == ' '))
               {
                  int pos_offset;

                  if ((msg_str[4] == 'O') && (msg_str[5] == 'k') &&
                      (msg_str[6] == ':') && (msg_str[7] == ' ') &&
                      (msg_str[8] == 'q') && (msg_str[9] == 'u') &&
                      (msg_str[10] == 'e') && (msg_str[11] == 'u') &&
                      (msg_str[12] == 'e') && (msg_str[13] == 'd') &&
                      (msg_str[14] == ' ') && (msg_str[15] == 'a') &&
                      (msg_str[16] == 's') && (msg_str[17] == ' '))
                  {
                     pos_offset = 18;
                  }
                  else if ((msg_str[4] == '2') && (msg_str[5] == '.') &&
                           (msg_str[6] == '0') && (msg_str[7] == '.') &&
                           (msg_str[8] == '0') && (msg_str[9] == ' ') &&
                           (msg_str[10] == 'O') && (msg_str[11] == 'k') &&
                           (msg_str[12] == ':') && (msg_str[13] == ' ') &&
                           (msg_str[14] == 'q') && (msg_str[15] == 'u') &&
                           (msg_str[16] == 'e') && (msg_str[17] == 'u') &&
                           (msg_str[18] == 'e') && (msg_str[19] == 'd') &&
                           (msg_str[20] == ' ') && (msg_str[21] == 'a') &&
                           (msg_str[22] == 's') && (msg_str[23] == ' '))
                       {
                          pos_offset = 24;
                       }
                       else
                       {
                          pos_offset = 0;
                       }

                  if (pos_offset > 0)
                  {
                     int pos = pos_offset;

                     mail_id[0] = ' ';
                     while ((msg_str[pos] != 13) && (msg_str[pos] != 10) &&
                            ((pos - pos_offset) < MAX_MAIL_ID_LENGTH))
                     {
                        mail_id[1 + pos - pos_offset] = msg_str[pos];
                        pos++;
                     }
                     mail_id[1 + pos - pos_offset] = '\0';
                     mail_id_length = pos - pos_offset;
                  }
                  else
                  {
                     mail_id[0] = '\0';
                     mail_id_length = 0;
                  }
               }
               else
               {
                  mail_id[0] = '\0';
                  mail_id_length = 0;
               }
#endif
            }
         }

         /* Tell user via FSA a file has been mailed. */
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
#endif

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
                               &ol_data,              /* Pointer to buffer. */
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
# ifdef _WITH_DE_MAIL_SUPPORT
                               (db.protocol & DE_MAIL_FLAG) ? DE_MAIL : SMTP,
# else
                               SMTP,
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
                               "Failed to archive file `%s'", final_filename);
               }

               /*
                * NOTE: We _MUST_ delete the file we just send,
                *       else the file directory will run full!
                */
               if ((unlink(fullname) < 0) && (errno != ENOENT))
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Could not unlink() local file `%s' after sending it successfully : %s",
                             strerror(errno), fullname);
               }

#ifdef _OUTPUT_LOG
               if (db.output_log == YES)
               {
                  (void)memcpy(ol_file_name, db.p_unique_name, db.unl);
                  if (mail_id_length > 0)
                  {
                     (void)memcpy(ol_file_name + db.unl, mail_id, mail_id_length);
                     *ol_unl = db.unl + mail_id_length;
                  }
                  else
                  {
                     *ol_unl = db.unl;
                  }
                  (void)strcpy(ol_file_name + *ol_unl, p_file_name_buffer);
                  *ol_file_name_length = (unsigned short)strlen(ol_file_name);
                  ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
                  ol_file_name[*ol_file_name_length + 1] = '\0';
                  (*ol_file_name_length)++;
                  *ol_file_size = *p_file_size_buffer;
                  *ol_job_number = fsa->job_status[(int)db.job_no].job_id;
                  *ol_retries = db.retries;
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
                               "Archived file `%s'", final_filename);
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
                  if (mail_id_length > 0)
                  {
                     (void)memcpy(ol_file_name + db.unl, mail_id, mail_id_length);
                     *ol_unl = db.unl + mail_id_length;
                  }
                  else
                  {
                     *ol_unl = db.unl;
                  }
                  (void)strcpy(ol_file_name + *ol_unl, p_file_name_buffer);
                  *ol_file_name_length = (unsigned short)strlen(ol_file_name);
                  ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
                  ol_file_name[*ol_file_name_length + 1] = '\0';
                  (*ol_file_name_length)++;
                  (void)strcpy(&ol_file_name[*ol_file_name_length + 1], &db.archive_dir[db.archive_offset]);
                  *ol_file_size = *p_file_size_buffer;
                  *ol_job_number = fsa->job_status[(int)db.job_no].job_id;
                  *ol_retries = db.retries;
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
            if (unlink(fullname) < 0)
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
                          strerror(errno), fullname);
            }

#ifdef _OUTPUT_LOG
            if (db.output_log == YES)
            {
               (void)memcpy(ol_file_name, db.p_unique_name, db.unl);
               if (mail_id_length > 0)
               {
                  (void)memcpy(ol_file_name + db.unl, mail_id, mail_id_length);
                  *ol_unl = db.unl + mail_id_length;
               }
               else
               {
                  *ol_unl = db.unl;
               }
               (void)strcpy(ol_file_name + *ol_unl, p_file_name_buffer);
               *ol_file_name_length = (unsigned short)strlen(ol_file_name);
               ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
               ol_file_name[*ol_file_name_length + 1] = '\0';
               (*ol_file_name_length)++;
               *ol_file_size = *p_file_size_buffer;
               *ol_job_number = fsa->job_status[(int)db.job_no].job_id;
               *ol_retries = db.retries;
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

#ifdef _WITH_DE_MAIL_SUPPORT
         if ((db.protocol & DE_MAIL_FLAG) &&
             ((db.de_mail_options & CONF_OF_DISPATCH) ||
              (db.de_mail_options & CONF_OF_RECEIPT) ||
              (db.de_mail_options & CONF_OF_RETRIEVE)))
         {
            if (demcd_fd == -2)
            {
# ifdef WITHOUT_FIFO_RW_SUPPORT
               demcd_log_fd(&demcd_fd, &demcd_readfd);
# else
               demcd_log_fd(&demcd_fd);
# endif
               if ((demcd_fd > -1) && (demcd_data == NULL))
               {
                  demcd_log_ptrs(&demcd_job_number,
                                 &demcd_data,              /* Pointer to buffer. */
                                 &demcd_file_name,
                                 &demcd_file_name_length,
                                 &demcd_file_size,
                                 &demcd_unl,
                                 &demcd_size,
                                 &demcd_confirmation_type,  
                                 db.host_alias);
               }
            }
            (void)memcpy(demcd_file_name, db.de_mail_privat_id, db.de_mail_privat_id_length);
            *demcd_unl = db.de_mail_privat_id_length;
            (void)strcpy(demcd_file_name + *demcd_unl, p_file_name_buffer);
            *demcd_file_name_length = (unsigned short)strlen(demcd_file_name);
            demcd_file_name[*demcd_file_name_length] = '\0';
            (*demcd_file_name_length)++;
            *demcd_file_size = *p_file_size_buffer;
            *demcd_job_number = fsa->job_status[(int)db.job_no].job_id;
            *demcd_confirmation_type = db.de_mail_options;
            demcd_real_size = *demcd_file_name_length + demcd_size;
            if (write(demcd_fd, demcd_data, demcd_real_size) != demcd_real_size)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "write() error : %s", strerror(errno));
            }
         }
#endif

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

      /* Free all memory. */
      if (encode_buffer != NULL)
      {
         free(encode_buffer);
         encode_buffer = NULL;
      }
      if (mail_header_buffer != NULL)
      {
         free(mail_header_buffer);
         mail_header_buffer = NULL;
      }
      if (extra_mail_header_buffer != NULL)
      {
         free(extra_mail_header_buffer);
         extra_mail_header_buffer = NULL;
      }

      /*
       * Remove file directory.
       */
      if (rmdir(file_path) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to remove directory `%s' : %s",
                    file_path, strerror(errno));
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

   /* Logout again. */
   if ((status = smtp_quit()) != SUCCESS)
   {
      trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, msg_str,
                "Failed to disconnect from SMTP-server (%d).", status);

      /*
       * Since all files have been transferred successful it is
       * not necessary to indicate an error in the status display.
       * It's enough when we say in the Transfer log that we failed
       * to log out.
       */
   }
   else
   {
      if ((fsa != NULL) && (fsa_pos_save == YES) && (fsa->debug > NORMAL_MODE))
      {
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str, "Logged out.");
      }
   }

   /* Don't need the ASCII buffer. */
   free(smtp_buffer);

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


#ifdef _WITH_DE_MAIL_SUPPORT
/*+++++++++++++++++++++++++++ gen_message_id() ++++++++++++++++++++++++++*/
static void
gen_message_id(char *host_name)
{
   static int         rand_initialized = NO;
   unsigned long long milliseconds_since_epoch;
   struct timeval     tv;

   if (db.message_id == NULL)
   {
      if ((db.message_id = malloc(MAX_LONG_LONG_HEX_LENGTH + 1 + MAX_INT_HEX_LENGTH + 1 + 1 + 255 + 1)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "malloc() error : %s", strerror(errno));
         exit(ALLOC_ERROR);
      }
   }
   if (gettimeofday(&tv, NULL) == -1)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                "gettimeofday() error : %s", strerror(errno));
      milliseconds_since_epoch = time(NULL);
   }
   else
   {
      milliseconds_since_epoch = (unsigned long long)(tv.tv_sec) * 1000 +
                                 (unsigned long long)(tv.tv_usec) / 1000;
   }
   if (rand_initialized == NO)
   {
      srand(time(NULL));
   }
   (void)snprintf(db.message_id,
                  MAX_LONG_LONG_HEX_LENGTH + 1 + MAX_INT_HEX_LENGTH + 1 + 255 + 1,
                  "%llx.%x@%s",
                  milliseconds_since_epoch, rand(), host_name);

    return;
}


/*+++++++++++++++++++++++++++ gen_privat_id() +++++++++++++++++++++++++++*/
static void
gen_privat_id(char *host_name)
{
   int length;

   if (db.de_mail_privat_id == NULL)
   {
      if ((db.de_mail_privat_id = malloc(MAX_INT_HEX_LENGTH + 1 + MAX_INT_HEX_LENGTH + 1 + 1 + MAX_MSG_NAME_LENGTH + 1)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "malloc() error : %s", strerror(errno));
         exit(ALLOC_ERROR);
      }
   }
   length = snprintf(db.de_mail_privat_id,
                     MAX_INT_HEX_LENGTH + 1 + MAX_INT_HEX_LENGTH + 1 + 1,
                     "%x-%x-",
                     get_checksum_crc32c(INITIAL_CRC,
                                         p_work_dir,
#ifdef HAVE_HW_CRC32
                                         (int)strlen(p_work_dir), have_hw_crc32),
#else
                                         (int)strlen(p_work_dir)),
#endif
                     get_checksum_crc32c(INITIAL_CRC,
                                         host_name,
#ifdef HAVE_HW_CRC32
                                         (int)strlen(host_name), have_hw_crc32));
#else
                                         (int)strlen(host_name)));
#endif
    (void)memcpy(db.de_mail_privat_id + length, db.p_unique_name, db.unl);
    db.de_mail_privat_id_length = length + db.unl;
    db.de_mail_privat_id[length + db.unl] = '\0';

    return;
}
#endif


/*++++++++++++++++++++++++++++ sf_smtp_exit() +++++++++++++++++++++++++++*/
static void
sf_smtp_exit(void)
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

         WHAT_DONE_BUFFER(length, buffer, "mailed",
                          diff_file_size_done, diff_no_of_files_done);
#ifdef _WITH_BURST_2
         if (burst_2_counter == 1)
         {
            buffer[length] = ' '; buffer[length + 1] = '[';
            buffer[length + 2] = 'B'; buffer[length + 3] = 'U';
            buffer[length + 4] = 'R'; buffer[length + 5] = 'S';
            buffer[length + 6] = 'T'; buffer[length + 7] = ']';
            buffer[length + 8] = '\0';
         }
         else if (burst_2_counter > 1)
              {
                 (void)snprintf(&buffer[length],
                                MAX_INT_LENGTH + 5 + MAX_OFF_T_LENGTH + 16 + MAX_INT_LENGTH + 11 + MAX_INT_LENGTH + 1 - length,
                                " [BURST * %u]", burst_2_counter);
              }
#endif
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
