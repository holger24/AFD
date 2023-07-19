/*
 *  init_gf.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   init_gf - initialises all variables for all gf_xxx (gf - get file)
 **
 ** SYNOPSIS
 **   void init_gf(int argc, char *argv[], int protocol)
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
 **   17.08.2000 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                     /* fprintf(), snprintf()          */
#include <string.h>                    /* strcpy(), strcat(), strerror() */
#include <stdlib.h>                    /* calloc() in RT_ARRAY()         */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>                    /* getpid()                       */
#include <errno.h>
#include "ftpdefs.h"
#include "fddefs.h"
#include "httpdefs.h"
#include "ssh_commondefs.h"
#ifdef _WITH_WMO_SUPPORT
# include "wmodefs.h"
#endif

/* External global variables. */
extern int                        fsa_fd,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                                  transfer_log_readfd,
#endif
                                  transfer_log_fd;
extern long                       transfer_timeout;
extern char                       host_deleted,
                                  *p_work_dir,
                                  tr_hostname[];
extern struct filetransfer_status *fsa;
extern struct fileretrieve_status *fra;
extern struct job                 db;


/*############################# init_gf() ##############################*/
void
init_gf(int argc, char *argv[], int protocol)
{
   int        status;
   time_t     next_check_time;
   char       gbuf[MAX_PATH_LENGTH];      /* Generic buffer.         */
   struct job *p_db;

   /* Initialise variables. */
   p_db = &db;
   (void)memset(&db, 0, sizeof(struct job));
   if (protocol & FTP_FLAG)
   {
      db.port = DEFAULT_FTP_PORT;
   }
   else if (protocol & HTTP_FLAG)
        {
           db.port = DEFAULT_HTTP_PORT;
        }
   else if (protocol & SFTP_FLAG)
        {
           db.port = SSH_PORT_UNSET;
        }
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
   db.fsa_pos = INCORRECT;
   db.fra_pos = INCORRECT;
   db.recipient = NULL;
   db.transfer_mode = DEFAULT_TRANSFER_MODE;
   db.toggle_host = NO;
   db.protocol = protocol;
   db.special_ptr = NULL;
/* db.special_flag = 0; */
/* db.mode_flag = 0; */
   db.dir_mode = 0;
   db.dir_mode_str[0] = '\0';
   db.user_home_dir = NULL;
   db.index_file = NULL;
/* db.password[0] = '\0'; */
#ifdef WITH_SSH_FINGERPRINT
/* db.ssh_fingerprint[0] = '\0'; */
/* db.key_type = 0; */
#endif
#ifdef WITH_SSL
   db.tls_auth = NO;
#endif
#ifdef _OUTPUT_LOG
   db.output_log = YES;
#endif
/* db.http_proxy[0] = '\0'; */
/* db.ssh_protocol = 0; */
/* db.sndbuf_size = 0; */
/* db.rcvbuf_size = 0; */
   db.my_pid = getpid();
   db.remote_file_check_interval = DEFAULT_REMOTE_FILE_CHECK_INTERVAL;

   if ((status = eval_input_gf(argc, argv, p_db)) < 0)
   {
      send_proc_fin(NO);
      exit(-status);
   }
   if (fra_attach_pos(db.fra_pos) != SUCCESS)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to attach to FRA position %d.", db.fra_pos);
      exit(INCORRECT);
   }
   db.fra_lock_offset = AFD_WORD_OFFSET +
                        (db.fra_pos * sizeof(struct fileretrieve_status));
   if ((db.special_flag & OLD_ERROR_JOB) &&
       ((fra->queued == 1) ||
        (((db.special_flag & DISTRIBUTED_HELPER_JOB) == 0) &&
         (fra->dir_options & ONE_PROCESS_JUST_SCANNING))))
   {
      /* No need to do any locking in get_remote_file_names_xxx(). */
      db.special_flag &= ~OLD_ERROR_JOB;
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
        }
   db.no_of_time_entries = fra->no_of_time_entries;
   if (db.no_of_time_entries == 0)
   {
      if ((db.te = malloc(sizeof(struct bd_time_entry))) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not malloc() memory : %s", strerror(errno));
         exit(ALLOC_ERROR);
      }
      db.te_malloc = YES;
      if (eval_time_str("* * * * *", db.te, NULL) != SUCCESS)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to evaluate time string [* * * * *].");
         exit(INCORRECT);
      }
      db.timezone[0] = '\0';
   }
   else
   {
      db.te_malloc = NO;
      db.te = &fra->te[0];
      (void)strcpy(db.timezone, fra->timezone);
   }
#ifdef WITH_SSL
   if ((fsa->protocol & HTTP_FLAG) && (fsa->protocol & SSL_FLAG) &&
       (db.port == DEFAULT_HTTP_PORT))
   {
      db.port = DEFAULT_HTTPS_PORT;
   }
#endif
   if (fsa->protocol_options & FTP_IGNORE_BIN)
   {
      db.transfer_mode = 'N';
   }
   if (db.sndbuf_size <= 0)
   {
      db.sndbuf_size = fsa->socksnd_bufsize;
   }
   if (db.rcvbuf_size <= 0)
   {
      db.rcvbuf_size = fsa->sockrcv_bufsize;
   }

   if ((fsa->error_counter > 0) && (fra->no_of_time_entries > 0))
   {
      next_check_time = fra->next_check_time;
   }
   else
   {
      next_check_time = 0;
   }
   if ((protocol & HTTP_FLAG) && (fra->dir_options & URL_WITH_INDEX_FILE_NAME))
   {
      if ((db.index_file = malloc(MAX_RECIPIENT_LENGTH)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not malloc() memory for index file : %s",
                    strerror(errno));
         exit(ALLOC_ERROR);
      }
   }
   if (eval_recipient(fra->url, &db, NULL,
                       next_check_time) == INCORRECT)
   {
      system_log(ERROR_SIGN, __FILE__,  __LINE__, "eval_recipient() failed.");
      exit(INCORRECT);
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
   if ((fsa->keep_connected > 0) &&
       ((fsa->special_flag & KEEP_CON_NO_FETCH) == 0))
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

   /* Set the transfer timeout value. */
   transfer_timeout = fsa->transfer_timeout;

   db.lock_offset = AFD_WORD_OFFSET +
                    (db.fsa_pos * sizeof(struct filetransfer_status));
   if (gsf_check_fsa((struct job *)&db) != NEITHER)
   {
#ifdef LOCK_DEBUG
      rlock_region(fsa_fd, db.lock_offset, __FILE__, __LINE__);
#else
      rlock_region(fsa_fd, db.lock_offset);
#endif
      fsa->job_status[(int)db.job_no].file_size = 0;
      fsa->job_status[(int)db.job_no].file_size_done = 0;
      fsa->job_status[(int)db.job_no].connect_status = CONNECTING;
      fsa->job_status[(int)db.job_no].job_id = db.id.dir;
      fsa->job_status[(int)db.job_no].file_name_in_use[MAX_FILENAME_LENGTH - 1] = 2;
#ifdef LOCK_DEBUG
      unlock_region(fsa_fd, db.lock_offset, __FILE__, __LINE__);
#else
      unlock_region(fsa_fd, db.lock_offset);
#endif
   }

   return;
}
