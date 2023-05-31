/*
 *  init_sf.c - Part of AFD, an automatic file distribution program.
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

DESCR__S_M3
/*
 ** NAME
 **   init_sf - initialises all variables for all sf_xxx (sf - send file)
 **
 ** SYNOPSIS
 **   int init_sf(int argc, char *argv[], char *file_path, int protocol)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None. It will exit with INCORRECT when an error has
 **   occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.12.1996 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                     /* fprintf()                      */
#include <string.h>                    /* strcpy(), strcat(), strerror() */
#include <stdlib.h>                    /* calloc() in RT_ARRAY()         */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>                     /* open()                         */
#include <unistd.h>                    /* getpid()                       */
#include <errno.h>
#include "fddefs.h"
#include "ftpdefs.h"
#include "smtpdefs.h"
#include "httpdefs.h"
#include "ssh_commondefs.h"
#ifdef _WITH_WMO_SUPPORT
# include "wmodefs.h"
#endif

/* External global variables. */
extern int                        exitflag,
                                  fsa_fd,
                                  no_of_hosts,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                                  transfer_log_readfd,
#endif
                                  transfer_log_fd;
extern long                       transfer_timeout;
extern char                       *p_work_dir,
                                  tr_hostname[];
extern struct filetransfer_status *fsa;
extern struct job                 db;
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif

/* Global variables. */
int                               no_of_rule_headers;


/*############################# init_sf() ###############################*/
int
init_sf(int argc, char *argv[], char *file_path, int protocol)
{
   int        files_to_send = 0,
              status;
   off_t      file_size_to_send = 0;
   char       gbuf[MAX_PATH_LENGTH];      /* Generic buffer.         */
   struct job *p_db;

   /* Initialise variables. */
   p_db = &db;
   (void)memset(&db, 0, sizeof(struct job));
   if (protocol & FTP_FLAG)
   {
      db.port = DEFAULT_FTP_PORT;
   }
#ifdef _WITH_DE_MAIL_SUPPORT
   else if ((protocol & SMTP_FLAG) || (protocol & DE_MAIL_FLAG))
#else
   else if (protocol & SMTP_FLAG)
#endif
        {
           db.port = DEFAULT_SMTP_PORT;
        }
   else if (protocol & SFTP_FLAG)
        {
           db.port = SSH_PORT_UNSET;
        }
   else if (protocol & HTTP_FLAG)
        {
           db.port = DEFAULT_HTTP_PORT;
        }
#ifdef _WITH_SCP_SUPPORT
   else if (protocol & SCP_FLAG)
        {
           db.port = SSH_PORT_UNSET;
           db.chmod = FILE_MODE;
        }
#endif
#ifdef _WITH_WMO_SUPPORT
   else if (protocol & WMO_FLAG)
        {
           db.port = DEFAULT_WMO_PORT;
        }
#endif
        else
        {
           db.port = -1;
        }
/* db.disconnect = 0; */
   db.reply_to = NULL;
   db.group_to = NULL;
   db.default_from = NULL;
   db.from = NULL;
   db.default_charset = NULL;
   db.charset = NULL;
   db.recipient = NULL;
   db.fsa_pos = INCORRECT;
   db.fra_pos = -2;
   db.transfer_mode = DEFAULT_TRANSFER_MODE;
   db.toggle_host = NO;
   db.resend = NO;
   db.protocol = protocol;
   db.special_ptr = NULL;
   db.subject = NULL;
   db.exec_cmd = NULL;
   db.group_mail_domain = NULL;
#ifdef _WITH_TRANS_EXEC
   db.trans_exec_cmd = NULL;
   db.trans_exec_timeout = DEFAULT_EXEC_TIMEOUT;
   db.set_trans_exec_lock = NO;
# ifdef HAVE_SETPRIORITY
/* db.afd_config_mtime = 0L; */
# endif
#endif
/* db.special_flag = 0; */
/* db.mode_flag = 0; */
   db.archive_time = DEFAULT_ARCHIVE_TIME;
   db.age_limit = DEFAULT_AGE_LIMIT;
/* db.archive_offset = 0; */
#ifdef _OUTPUT_LOG
   db.output_log = YES;
#endif
   db.lock = DEFAULT_LOCK;
/* db.smtp_server[0] = '\0'; */
/* db.http_proxy[0] = '\0'; */
   db.chmod_str[0] = '\0';
   db.dir_mode = 0;
   db.dir_mode_str[0] = '\0';
/* db.trans_rename_rule[0] = '\0'; */
/* db.user_home_dir = NULL; */
   db.index_file = NULL;
/* db.password[0] = '\0'; */
/* db.user_rename_rule[0] = '\0'; */
   db.lock_file_name = NULL;
/* db.rename_file_busy = '\0'; */
   db.group_list = NULL;
   db.no_of_rhardlinks = 0;
   db.hardlinks = NULL;
   db.no_of_rsymlinks = 0;
   db.symlinks = NULL;
/* db.no_of_restart_files = 0; */
   db.restart_file = NULL;
   db.user_id = -1;
   db.group_id = -1;
   db.filename_pos_subject = -1;
/* db.subject_rename_rule[0] = '\0'; */
#ifdef WITH_SSL
   db.tls_auth = NO;
#endif
/* db.ssh_protocol = 0; */
#ifdef WITH_SSH_FINGERPRINT
/* db.ssh_fingerprint[0] = '\0'; */
/* db.key_type = 0; */
#endif
   (void)strcpy(db.lock_notation, DOT_NOTATION);
/* db.sndbuf_size = 0; */
/* db.rcvbuf_size = 0; */
#ifdef _DELETE_LOG
   dl.fd = -1;
#endif
#ifdef WITH_DUP_CHECK
/* db.crc_id = 0; */
/* db.trans_dup_check_timeout = 0L; */
/* db.trans_dup_check_flag = 0; */
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
   db.de_mail_options = DEFAULT_CONFIRMATION;
   db.de_mail_privat_id_length = 0;
   db.de_mail_privat_id = NULL;
   db.de_mail_sender = NULL;
   db.demcd_log = YES;
#endif
   db.my_pid = getpid();

   if ((status = eval_input_sf(argc, argv, p_db)) < 0)
   {
      send_proc_fin(NO);
      exit(-status);
   }
   if (protocol & EXEC_FLAG)
   {
      if (check_exec_type(db.exec_cmd))
      {
         db.special_flag |= EXEC_ONCE_ONLY;
      }
   }
   if ((protocol & FTP_FLAG) && (db.mode_flag == 0))
   {
      if (fsa->protocol_options & FTP_PASSIVE_MODE)
      {
         db.mode_flag = PASSIVE_MODE;
         if (fsa->protocol_options & FTP_EXTENDED_MODE)
         {
            /* Write "extended passive" */
            db.mode_str[0] = 'e'; db.mode_str[1] = 'x'; db.mode_str[2] = 't';
            db.mode_str[3] = 'e'; db.mode_str[4] = 'n'; db.mode_str[5] = 'd';
            db.mode_str[6] = 'e'; db.mode_str[7] = 'd'; db.mode_str[8] = ' ';
            db.mode_str[9] = 'p'; db.mode_str[10] = 'a';
            db.mode_str[11] = 's'; db.mode_str[12] = 's';
            db.mode_str[13] = 'i'; db.mode_str[14] = 'v';
            db.mode_str[15] = 'e'; db.mode_str[16] = '\0';
         }
         else
         {
            if (fsa->protocol_options & FTP_ALLOW_DATA_REDIRECT)
            {
               /* Write "passive (with redirect)" */
               db.mode_str[0] = 'p'; db.mode_str[1] = 'a'; db.mode_str[2] = 's';
               db.mode_str[3] = 's'; db.mode_str[4] = 'i'; db.mode_str[5] = 'v';
               db.mode_str[6] = 'e'; db.mode_str[7] = ' '; db.mode_str[8] = '(';
               db.mode_str[9] = 'w'; db.mode_str[10] = 'i';
               db.mode_str[11] = 't'; db.mode_str[12] = 'h';
               db.mode_str[13] = ' '; db.mode_str[14] = 'r';
               db.mode_str[15] = 'e'; db.mode_str[16] = 'd';
               db.mode_str[17] = 'i'; db.mode_str[18] = 'r';
               db.mode_str[19] = 'e'; db.mode_str[20] = 'c';
               db.mode_str[21] = 't'; db.mode_str[22] = ')';
               db.mode_str[23] = '\0';
               db.mode_flag |= ALLOW_DATA_REDIRECT;
            }
            else
            {
               /* Write "passive" */
               db.mode_str[0] = 'p'; db.mode_str[1] = 'a'; db.mode_str[2] = 's';
               db.mode_str[3] = 's'; db.mode_str[4] = 'i'; db.mode_str[5] = 'v';
               db.mode_str[6] = 'e'; db.mode_str[7] = '\0';
            }
         }
      }
      else
      {
         db.mode_flag = ACTIVE_MODE;
         if (fsa->protocol_options & FTP_EXTENDED_MODE)
         {
            /* Write "extended active" */
            db.mode_str[0] = 'e'; db.mode_str[1] = 'x'; db.mode_str[2] = 't';
            db.mode_str[3] = 'e'; db.mode_str[4] = 'n'; db.mode_str[5] = 'd';
            db.mode_str[6] = 'e'; db.mode_str[7] = 'd'; db.mode_str[8] = ' ';
            db.mode_str[9] = 'a'; db.mode_str[10] = 'c'; db.mode_str[11] = 't';
            db.mode_str[12] = 'i'; db.mode_str[13] = 'v'; db.mode_str[14] = 'e';
            db.mode_str[15] = '\0';
         }
         else
         {
            /* Write "active" */
            db.mode_str[0] = 'a'; db.mode_str[1] = 'c'; db.mode_str[2] = 't';
            db.mode_str[3] = 'i'; db.mode_str[4] = 'v'; db.mode_str[5] = 'e';
            db.mode_str[6] = '\0';
         }
      }
      if (fsa->protocol_options & FTP_EXTENDED_MODE)
      {
         db.mode_flag |= EXTENDED_MODE;
      }
   }
   else
   {
      db.mode_str[0] = '\0';
   }
   if (fsa->protocol_options & FTP_IGNORE_BIN)
   {
      db.transfer_mode = 'N';
   }
   if (fsa->protocol_options & USE_SEQUENCE_LOCKING)
   {
      db.special_flag |= SEQUENCE_LOCKING;
   }
   if ((fsa->keep_connected > 0) &&
       ((fsa->special_flag & KEEP_CON_NO_SEND) == 0))
   {
      db.keep_connected = fsa->keep_connected;
   }
   else
   {
      db.keep_connected = 0;
   }
   if (db.sndbuf_size <= 0)
   {
      db.sndbuf_size = fsa->socksnd_bufsize;
   }
   if (db.rcvbuf_size <= 0)
   {
      db.rcvbuf_size = fsa->sockrcv_bufsize;
   }
#ifdef WITH_SSL
   if ((fsa->protocol & HTTP_FLAG) && (fsa->protocol & SSL_FLAG) &&
       (db.port == DEFAULT_HTTP_PORT))
   {
      db.port = DEFAULT_HTTPS_PORT;
   }
#endif

   /* Open/create log fifos. */
   (void)strcpy(gbuf, p_work_dir);
   (void)strcat(gbuf, FIFO_DIR);
   (void)strcat(gbuf, TRANSFER_LOG_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(gbuf, &transfer_log_readfd, &transfer_log_fd) == -1)
#else
   if ((transfer_log_fd = open(gbuf, O_RDWR)) == -1)
#endif
   {
      if (errno == ENOENT)
      {
         if ((make_fifo(gbuf) == SUCCESS) &&
#ifdef WITHOUT_FIFO_RW_SUPPORT
             (open_fifo_rw(gbuf, &transfer_log_readfd, &transfer_log_fd) == -1))
#else
             ((transfer_log_fd = open(gbuf, O_RDWR)) == -1))
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not open fifo `%s' : %s",
                       TRANSFER_LOG_FIFO, strerror(errno));
         }
      }
      else
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not open fifo `%s' : %s",
                    TRANSFER_LOG_FIFO, strerror(errno));
      }
   }

   (void)strcpy(tr_hostname, fsa->host_dsp_name);
   if (db.toggle_host == YES)
   {
      if (fsa->host_toggle == HOST_ONE)
      {
         tr_hostname[(int)fsa->toggle_pos] = fsa->host_toggle_str[HOST_TWO];
      }
      else
      {
         tr_hostname[(int)fsa->toggle_pos] = fsa->host_toggle_str[HOST_ONE];
      }
   }

   /*
    * Open the AFD counter file. This is needed when trans_rename is
    * used or user renaming (SMTP).
    */
   if ((db.trans_rename_rule[0] != '\0') ||
       (db.user_rename_rule[0] != '\0') ||
       (db.subject_rename_rule[0] != '\0') ||
       ((db.special_flag & ADD_MAIL_HEADER) &&
        (db.special_ptr != NULL) && (*db.special_ptr != '/')))
   {
      get_rename_rules(NO);
      if (db.trans_rename_rule[0] != '\0')
      {
         if ((db.trans_rule_pos = get_rule(db.trans_rename_rule,
                                           no_of_rule_headers)) < 0)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                      "Could NOT find rule %s. Ignoring the option \"%s\".",
                      db.trans_rename_rule, TRANS_RENAME_ID);
            db.trans_rename_rule[0] = '\0';
         }
      }
      if (db.user_rename_rule[0] != '\0')
      {
         if ((db.user_rule_pos = get_rule(db.user_rename_rule,
                                          no_of_rule_headers)) < 0)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Could NOT find rule %s. Ignoring this option.",
                       db.user_rename_rule);
            db.user_rename_rule[0] = '\0';
         }
      }
      if (db.subject_rename_rule[0] != '\0')
      {
         if ((db.subject_rule_pos = get_rule(db.subject_rename_rule,
                                             no_of_rule_headers)) < 0)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Could NOT find rule %s. Ignoring the option \"%s\".",
                       db.subject_rename_rule, SUBJECT_ID);
            db.subject_rename_rule[0] = '\0';
         }
      }
      if ((db.special_flag & ADD_MAIL_HEADER) &&
          (db.special_ptr != NULL) && (*db.special_ptr != '/'))
      {
         if ((db.mail_header_rule_pos = get_rule(db.special_ptr,
                                                 no_of_rule_headers)) < 0)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Could NOT find rule %s. Ignoring the option \"%s\".",
                       db.special_ptr, ADD_MAIL_HEADER_ID);
            db.special_flag &= ~ADD_MAIL_HEADER;
            free(db.special_ptr);
            db.special_ptr = NULL;
         }
      }
   }

   db.lock_offset = AFD_WORD_OFFSET +
                    (db.fsa_pos * sizeof(struct filetransfer_status));
   if ((files_to_send = get_file_names(file_path, &file_size_to_send)) < 1)
   {
      int ret;

      /*
       * It could be that all files where to old to be send. If this is
       * the case, no need to go on.
       */
#ifdef WITH_UNLINK_DELAY
      if ((ret = remove_dir(file_path, 0)) < 0)
#else
      if ((ret = remove_dir(file_path)) < 0)
#endif
      {
         if (ret == FILE_IS_DIR)
         {
            if (rec_rmdir(file_path) < 0)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to rec_rmdir() %s", file_path);
            }
            else
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Removed directory/directories in %s", file_path);
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to remove directory %s", file_path);
         }
      }
      exitflag = 0;
      exit(NO_FILES_TO_SEND);
   }

   /*
    * For bursting we need to set the following active values.
    * Otherwise if during a burst all files are deleted, the
    * following burst will then think those values are set.
    */
   (void)strcpy(db.active_user, db.user);
   (void)strcpy(db.active_target_dir, db.target_dir);
   db.active_transfer_mode = db.transfer_mode;
#ifdef WITH_SSL
   db.active_auth = db.tls_auth;
#endif

   /* Do we want to display the status? */
   if (gsf_check_fsa((struct job *)&db) != NEITHER)
   {
#ifdef LOCK_DEBUG
      rlock_region(fsa_fd, db.lock_offset, __FILE__, __LINE__);
#else
      rlock_region(fsa_fd, db.lock_offset);
#endif
      fsa->job_status[(int)db.job_no].file_size = file_size_to_send;
      fsa->job_status[(int)db.job_no].file_size_done = 0;
      fsa->job_status[(int)db.job_no].connect_status = CONNECTING;
      fsa->job_status[(int)db.job_no].job_id = db.id.job;
      fsa->job_status[(int)db.job_no].file_name_in_use[MAX_FILENAME_LENGTH - 1] = 2;
#ifdef LOCK_DEBUG
      unlock_region(fsa_fd, db.lock_offset, __FILE__, __LINE__);
#else
      unlock_region(fsa_fd, db.lock_offset);
#endif

      /* Set the timeout value. */
      transfer_timeout = fsa->transfer_timeout;
   }

   return(files_to_send);
}
