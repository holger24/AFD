/*
 *  asmtp.c - Part of AFD, an automatic file distribution program.
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

#include "asmtpdefs.h"

DESCR__S_M1
/*
 ** NAME
 **   asmtp - send files via SMTP automaticaly
 **
 ** SYNOPSIS
 **   asmtp [options] [file 1 ... file n]
 **
 **   options
 **       --version               - Show current version
 **       -a <user@host>          - The address where the mail is sent.
 **       -b <block size>         - Transfer block size in bytes.
 **       -e                      - Encode files in BASE64.
 **       -f <filename file>      - List of filenames to send.
 **       -h <hostname|IP number> - Recipient hostname or IP of this mail.
 **       -i <from address>       - Send a from header.
 **       -o <reply-to address>   - Insert a reply-to header.
 **       -p <port number>        - Remote port number of SMTP-server.
 **       -m <mailserver-address> - Mailserver that will send this mail.
 **       -n                      - Filename is subject.
 **       -r                      - Remove transmitted file.
 **       -s <subject>            - Subject of this mail.
 **       -t <timout>             - SMTP timeout in seconds.
 **       -T                      - Only test if the recipient exists.
 **       -u <user>               - The user who should get the mail.
 **       -v                      - Verbose. Shows all SMTP commands
 **                                 and the reply from the remote server.
 **       -x                      - Use TLS legacy renegotiation.
 **       -y                      - File name is user.
 **       -Y                      - Use strict SSL/TLS verification.
 **       -?                      - Display some help.
 **
 ** DESCRIPTION
 **   asmtp sends the given files to the defined recipient via SMTP
 **   It does so by using it's own SMTP-client.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   24.12.2000 H.Kiehl Created
 **   04.08.2002 H.Kiehl Added To:, From: and Reply-To headers.
 **   23.04.2014 H.Kiehl Added possibility to send mails without files.
 **   13.10.2014 H.Kiehl Added support for SMARTTLS.
 **   09.02.2020 H.Kiehl Added -T to test if the recipient exists.
 **
 */
DESCR__E_M1

#include <stdio.h>                     /* fprintf(), snprintf()          */
#include <string.h>                    /* strcpy(), strcat(), strcmp(),  */
                                       /* strerror()                     */
#include <stdlib.h>                    /* malloc(), free()               */
#include <ctype.h>                     /* isdigit()                      */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>                    /* signal()                       */
#include <unistd.h>                    /* unlink(), close()              */
#include <errno.h>
#include "smtpdefs.h"
#include "version.h"

/* Global variables. */
unsigned int         special_flag = 0;
int                  line_length = 0,  /* encode_base64()                */
                     *no_of_listed_files,
                     simulation_mode = NO,
                     sys_log_fd = STDERR_FILENO,
                     transfer_log_fd = STDERR_FILENO,
                     timeout_flag,
                     sigpipe_flag;
long                 transfer_timeout;
char                 msg_str[MAX_RET_MSG_LENGTH],
                     *p_work_dir = NULL;
struct data          db;
struct filename_list *rl;
const char           *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void          asmtp_exit(void),
                     sig_bus(int),
                     sig_segv(int),
                     sig_pipe(int),
                     sig_exit(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int          blocksize,
                buffer_size = 0,
                encode_buffer_size = 0,
                fd,
                j,
                status,
                loops,
                rest,
                files_send = 0,
                no_of_files_done = 0,
                write_size;
   size_t       length;
   off_t        file_size_done = 0,
                local_file_size,
                no_of_bytes;
   char         *buffer,
                *buffer_ptr,
                *encode_buffer = NULL,
                *file_ptr,
                host_name[256],
                local_user[MAX_FILENAME_LENGTH + MAX_FILENAME_LENGTH],
                multipart_boundary[MAX_FILENAME_LENGTH],
                remote_user[MAX_USER_NAME_LENGTH + 1 + MAX_FILENAME_LENGTH + 1],
                final_filename[MAX_FILENAME_LENGTH],
                *ptr,
                *smtp_buffer;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   CHECK_FOR_VERSION(argc, argv);

   /* Do some cleanups when we exit. */
   if (atexit(asmtp_exit) != 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                _("Could not register exit function : %s (%s %d)\n"),
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR) ||
       (signal(SIGPIPE, sig_pipe) == SIG_ERR))
   {
      (void)rec(sys_log_fd, FATAL_SIGN, _("signal() error : %s (%s %d)\n"),
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Initialise variables. */
   init_asmtp(argc, argv, &db);
   msg_str[0] = '\0';
   blocksize = db.blocksize;

   /* Set SMTP timeout value. */
   transfer_timeout = db.transfer_timeout;

   if (db.smtp_server[0] == '\0')
   {
      (void)strcpy(db.smtp_server, SMTP_HOST_NAME);
   }

   /*
    * The extra buffer is needed to convert LF's to CRLF.
    */
   if ((smtp_buffer = malloc(((blocksize * 2) + 1))) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, _("malloc() error : %s (%s %d)\n"),
                strerror(errno), __FILE__, __LINE__);
      exit(ALLOC_ERROR);
   }

   sigpipe_flag = timeout_flag = OFF;

   /* Connect to remote SMTP-server. */
   if ((status = smtp_connect(db.smtp_server, db.port,
                              db.sndbuf_size)) != SUCCESS)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                _("SMTP connection to <%s> at port %d failed (%d)."),
                db.smtp_server, db.port, status);
      exit(eval_timeout(CONNECT_ERROR));
   }
   else
   {
      if (db.verbose == YES)
      {
         trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   _("Connected to <%s> at port %d."), db.smtp_server, db.port);
      }
   }

   /* Now send EHLO. */
   if (gethostname(host_name, 255) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, _("gethostname() error : %s (%s %d)\n"),
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   status = smtp_ehlo(host_name);
   if (status == 502)
   {
      if ((status = smtp_helo(host_name)) != SUCCESS)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   _("Failed to send EHLO and HELO to <%s> (%d)."),
                   db.smtp_server, status);
         (void)smtp_quit();
         exit(eval_timeout(CONNECT_ERROR));
      }
      else
      {
         if (db.verbose == YES)
         {
            trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                      _("Send HELO."));
         }
      }
   }
   else if (status == SUCCESS)
        {
           if (db.verbose == YES)
           {
              trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                        _("Send EHLO."));
           }
        }
        else
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                     _("Failed to send EHLO to <%s> (%d)."),
                     db.smtp_server, status);
           (void)smtp_quit();
           exit(eval_timeout(CONNECT_ERROR));
        }

#ifdef WITH_SSL
   /* Try negotiate SMARTTLS. */
   if ((status = smtp_smarttls(db.strict, db.legacy_renegotiation)) == SUCCESS)
   {
      if (db.verbose == YES)
      {
         trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   "SSL/TSL connection to server `%s' succesful.",
                   db.smtp_server);
      }

      /*
       * RFC-2487 requires that we discard all knowledge from the
       * previous EHLO command issue the EHLO command again.
       */
      status = smtp_ehlo(host_name);
      if (status == 502)
      {
         if ((status = smtp_helo(host_name)) != SUCCESS)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                      _("Failed to send EHLO and HELO to <%s> (%d)."),
                      db.smtp_server, status);
            (void)smtp_quit();
            exit(eval_timeout(CONNECT_ERROR));
         }
         else
         {
            if (db.verbose == YES)
            {
               trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         _("Send HELO."));
            }
         }
      }
      else if (status == SUCCESS)
           {
              if (db.verbose == YES)
              {
                 trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                           _("Send EHLO again."));
              }
           }
           else
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                        _("Failed to send EHLO again to <%s> (%d)."),
                        db.smtp_server, status);
              (void)smtp_quit();
              exit(eval_timeout(CONNECT_ERROR));
           }
   }
   else if (status == NEITHER)
        {
           if (db.verbose == YES)
           {
              trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
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
#endif

   /* Prepare local and remote user name. */
   if (db.from != NULL)
   {
      (void)my_strncpy(local_user, db.from, MAX_FILENAME_LENGTH);
   }
   else
   {
      if ((ptr = getenv("LOGNAME")) != NULL)
      {
         (void)snprintf(local_user, MAX_FILENAME_LENGTH + MAX_FILENAME_LENGTH,
                        "%s@%s", ptr, host_name);
      }
      else
      {
         (void)snprintf(local_user, MAX_FILENAME_LENGTH + MAX_FILENAME_LENGTH,
                        "%s@%s", AFD_USER_NAME, host_name);
      }
   }

   if ((db.special_flag & FILE_NAME_IS_USER) == 0)
   {
      (void)snprintf(remote_user,
                     MAX_USER_NAME_LENGTH + 1 + MAX_FILENAME_LENGTH + 1,
                     "%s@%s", db.user, db.hostname);
   }

   /* Allocate buffer to read data from the source file. */
   if ((buffer = malloc(blocksize + 4)) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, _("malloc() error : %s (%s %d)\n"),
                strerror(errno), __FILE__, __LINE__);
      exit(ALLOC_ERROR);
   }
   buffer_size = blocksize + 4;

   if (db.special_flag & ATTACH_FILE)
   {
      encode_buffer_size = (2 * (blocksize + 1)) + 1;
      if ((encode_buffer = malloc(encode_buffer_size)) == NULL)
      {
         (void)rec(sys_log_fd, ERROR_SIGN, _("malloc() error : %s (%s %d)\n"),
                   strerror(errno), __FILE__, __LINE__);
         exit(ALLOC_ERROR);
      }

      /*
       * When encoding in base64 is done the blocksize must be
       * divideable by three!!!!
       */
      blocksize = blocksize - (blocksize % 3);
   }

   multipart_boundary[0] = '\0';

   if (db.no_of_files == 0)
   {
      /* Send local user name. */
      if ((status = smtp_user(local_user)) != SUCCESS)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   _("Failed to send local user <%s> (%d)."),
                   local_user, status);
         (void)smtp_quit();
         exit(eval_timeout(USER_ERROR));
      }
      else
      {
         if (db.verbose == YES)
         {
            trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                      _("Entered local user name %s."), local_user);
         }
      }

      /* Send remote user name. */
      if ((status = smtp_rcpt(remote_user)) != SUCCESS)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   _("Failed to send remote user <%s> (%d)."),
                   remote_user, status);
         (void)smtp_quit();
         exit(eval_timeout(REMOTE_USER_ERROR));
      }
      else
      {
         if (db.verbose == YES)
         {
            trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                      _("Remote address %s accepted by SMTP-server."),
                      remote_user);
         }
         if (db.special_flag & ONLY_TEST)
         {
            (void)smtp_quit();
            exit(SUCCESS);
         }
      }

      /* Enter data mode. */
      if ((status = smtp_open()) != SUCCESS)
      {                                     
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   _("Failed to set DATA mode (%d)."), status);
         (void)smtp_quit();                                 
         exit(eval_timeout(DATA_ERROR));
      }                   
      else
      {   
         if (db.verbose == YES)
         {                           
            trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                      _("Set DATA mode."));
         }                                                                
      }

      if (db.from != NULL)
      {
         length = snprintf(buffer, buffer_size, "From: %s\r\n", db.from);
         if (length >= buffer_size)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                      "Buffer length for mail header to small!");
            (void)smtp_quit();
            exit(ALLOC_ERROR);
         }
         if (smtp_write(buffer, NULL, length) < 0)
         {
            WHAT_DONE("mailed", file_size_done, no_of_files_done);
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                      _("Failed to write From to SMTP-server."));
            (void)smtp_quit();
            exit(eval_timeout(WRITE_REMOTE_ERROR));
         }
         no_of_bytes = length;
      }
      if (db.reply_to != NULL)
      {
         length = snprintf(buffer, buffer_size,
                           "Reply-To: %s\r\n", db.reply_to);
         if (length >= buffer_size)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                      "Buffer length for mail header to small!");
            (void)smtp_quit();
            exit(ALLOC_ERROR);
         }
         if (smtp_write(buffer, NULL, length) < 0)
         {
            WHAT_DONE("mailed", file_size_done, no_of_files_done);
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                      _("Failed to write Reply-To to SMTP-server."));
            (void)smtp_quit();
            exit(eval_timeout(WRITE_REMOTE_ERROR));
         }
         no_of_bytes += length;
      }

      length = snprintf(buffer, buffer_size, "To: %s\r\n", remote_user);
      if (length >= buffer_size)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                   "Buffer length for mail header to small!");
         (void)smtp_quit();
         exit(ALLOC_ERROR);
      }
      if (smtp_write(buffer, NULL, length) < 0)
      {
         WHAT_DONE("mailed", file_size_done, no_of_files_done);
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                   _("Failed to write To header to SMTP-server."));
         (void)smtp_quit();
         exit(eval_timeout(WRITE_REMOTE_ERROR));
      }
      no_of_bytes += length;

      if (db.subject != NULL)
      {
         length = snprintf(buffer, buffer_size, "%s", db.subject);
         if (length >= buffer_size)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                      "Buffer length for mail header to small!");
            (void)smtp_quit();
            exit(ALLOC_ERROR);
         }
         if (smtp_write_subject(buffer, &length, db.charset) < 0)
         {
            WHAT_DONE("mailed", file_size_done, no_of_files_done);
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                      _("Failed to write subject to SMTP-server."));
            (void)smtp_quit();
            exit(eval_timeout(WRITE_REMOTE_ERROR));
         }
         no_of_bytes += length;
      }
      else if (db.special_flag & FILE_NAME_IS_SUBJECT)
           {
              length = snprintf(buffer, buffer_size, "%s", final_filename);
              if (length >= buffer_size)
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                           "Buffer length for mail header to small!");
                 (void)smtp_quit();
                 exit(ALLOC_ERROR);
              }
              if (smtp_write_subject(buffer, &length, db.charset) < 0)
              {
                 WHAT_DONE("mailed", file_size_done, no_of_files_done);
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                           _("Failed to write the filename as subject to SMTP-server."));
                 (void)smtp_quit();
                 exit(eval_timeout(WRITE_REMOTE_ERROR));
              }
              no_of_bytes += length;
           } /* if (db.special_flag & FILE_NAME_IS_SUBJECT) */

      if (smtp_write("\r\n", NULL, 2) < 0)
      {
         WHAT_DONE("mailed", file_size_done, no_of_files_done);
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                   _("Failed to write carriage return line feed to mark end of header to SMTP-server."));
         (void)smtp_quit();
         exit(eval_timeout(WRITE_REMOTE_ERROR));
      }
      no_of_bytes += 2;

      /* Close remote file. */
      if ((status = smtp_close()) != SUCCESS)
      {
         WHAT_DONE("mailed", file_size_done, no_of_files_done);
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   _("Failed to close DATA mode (%d)."), status);
         (void)smtp_quit();
         exit(eval_timeout(CLOSE_REMOTE_ERROR));
      }
      else
      {
         if (db.verbose == YES)
         {
            trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                      _("Closing DATA mode."));
         }
      }
   }
   else
   {
      /* Send all files. */
      for (files_send = 0; files_send < db.no_of_files; files_send++)
      {
         if ((db.realname != NULL) && (db.realname[files_send][0] != '\0'))
         {
            file_ptr = db.realname[files_send];
         }
         else
         {
            file_ptr = db.filename[files_send];
            length = strlen(file_ptr);
            while (length != 0)
            {
               if (db.filename[files_send][length - 1] == '/')
               {
                  file_ptr = &db.filename[files_send][length];
                  break;
               }
               length--;
            }
         }
         (void)strcpy(final_filename, file_ptr);

         /* Send local user name. */
         if ((status = smtp_user(local_user)) != SUCCESS)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                      _("Failed to send local user <%s> (%d)."),
                      local_user, status);
            (void)smtp_quit();
            exit(eval_timeout(USER_ERROR));
         }
         else
         {
            if (db.verbose == YES)
            {
               trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         _("Entered local user name %s."), local_user);
            }
         }

         /* Send remote user name. */
         if ((status = smtp_rcpt(remote_user)) != SUCCESS)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                      _("Failed to send remote user <%s> (%d)."),
                      remote_user, status);
            (void)smtp_quit();
            exit(eval_timeout(REMOTE_USER_ERROR));
         }
         else
         {
            if (db.verbose == YES)
            {
               trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         _("Remote address %s accepted by SMTP-server."),
                         remote_user);
            }
            if (db.special_flag & ONLY_TEST)
            {
               (void)smtp_quit();
               exit(SUCCESS);
            }
         }

         /* Enter data mode. */
         if ((status = smtp_open()) != SUCCESS)
         {                                     
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                      _("Failed to set DATA mode (%d)."), status);
            (void)smtp_quit();                                 
            exit(eval_timeout(DATA_ERROR));
         }                   
         else
         {   
            if (db.verbose == YES)
            {                           
               trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         _("Set DATA mode."));
            }                                                                
         }

         /* Open local file. */
         if ((fd = open(db.filename[files_send], O_RDONLY)) < 0)
         {
            if (db.verbose == YES)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         _("Failed to open() local file %s"),
                         db.filename[files_send]);
            }

            /* Close remote file. */
            if ((status = smtp_close()) != SUCCESS)
            {
               WHAT_DONE("mailed", file_size_done, no_of_files_done);
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         _("Failed to close DATA mode (%d)."), status);
               (void)smtp_quit();
               exit(eval_timeout(CLOSE_REMOTE_ERROR));
            }
            else
            {
               if (db.verbose == YES)
               {
                  trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                            _("Closing DATA mode."));
               }
            }
            continue;
         }
#ifdef HAVE_STATX
         if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                   STATX_MODE | STATX_SIZE, &stat_buf) == -1)
#else
         if (fstat(fd, &stat_buf) == -1)
#endif
         {
            if (db.verbose == YES)
            {
               trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
                         _("Failed to access local file `%s'"),
                         db.filename[files_send]);
            }
            WHAT_DONE("mailed", file_size_done, no_of_files_done);
            (void)smtp_close();
            (void)smtp_quit();
            exit(STAT_ERROR);
         }
         else
         {
#ifdef HAVE_STATX
            if (!S_ISREG(stat_buf.stx_mode))
#else
            if (!S_ISREG(stat_buf.st_mode))
#endif
            {
               if (db.verbose == YES)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            _("Local file `%s' is not a regular file."),
                            db.filename[files_send]);
               }

               /* Close remote file. */
               if ((status = smtp_close()) != SUCCESS)
               {
                  WHAT_DONE("mailed", file_size_done, no_of_files_done);
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                            _("Failed to close DATA mode (%d)."), status);
                     (void)smtp_quit();
                  exit(eval_timeout(CLOSE_REMOTE_ERROR));
               }
               else
               {
                  if (db.verbose == YES)
                  {
                     trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                               _("Closing DATA mode."));
                  }
               }
               (void)close(fd);
               continue;
            }
         }
#ifdef HAVE_STATX
         local_file_size = stat_buf.stx_size;
#else
         local_file_size = stat_buf.st_size;
#endif
         if (db.verbose == YES)
         {
            trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
                      _("Open local file `%s'"), db.filename[files_send]);
         }

         /* Read (local) and write (remote) file. */
         no_of_bytes = 0;
         loops = local_file_size / blocksize;
         rest = local_file_size % blocksize;

         if (db.from != NULL)
         {
            length = snprintf(buffer, buffer_size, "From: %s\r\n", db.from);
            if (length >= buffer_size)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Buffer length for mail header to small!");
               (void)smtp_quit();
               exit(ALLOC_ERROR);
            }
            if (smtp_write(buffer, NULL, length) < 0)
            {
               WHAT_DONE("mailed", file_size_done, no_of_files_done);
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         _("Failed to write From to SMTP-server."));
               (void)smtp_quit();
               exit(eval_timeout(WRITE_REMOTE_ERROR));
            }
            no_of_bytes = length;
         }
         if (db.reply_to != NULL)
         {
            length = snprintf(buffer, buffer_size,
                              "Reply-To: %s\r\n", db.reply_to);
            if (length >= buffer_size)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Buffer length for mail header to small!");
               (void)smtp_quit();
               exit(ALLOC_ERROR);
            }
            if (smtp_write(buffer, NULL, length) < 0)
            {
               WHAT_DONE("mailed", file_size_done, no_of_files_done);
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         _("Failed to write Reply-To to SMTP-server."));
               (void)smtp_quit();
               exit(eval_timeout(WRITE_REMOTE_ERROR));
            }
            no_of_bytes += length;
         }

         length = snprintf(buffer, buffer_size, "To: %s\r\n", remote_user);
         if (length >= buffer_size)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                      "Buffer length for mail header to small!");
            (void)smtp_quit();
            exit(ALLOC_ERROR);
         }
         if (smtp_write(buffer, NULL, length) < 0)
         {
            WHAT_DONE("mailed", file_size_done, no_of_files_done);
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                      _("Failed to write To header to SMTP-server."));
            (void)smtp_quit();
            exit(eval_timeout(WRITE_REMOTE_ERROR));
         }
         no_of_bytes += length;

         if (db.subject != NULL)
         {
            length = snprintf(buffer, buffer_size, "%s", db.subject);
            if (length >= buffer_size)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Buffer length for mail header to small!");
               (void)smtp_quit();
               exit(ALLOC_ERROR);
            }
            if (smtp_write_subject(buffer, &length, db.charset) < 0)
            {
               WHAT_DONE("mailed", file_size_done, no_of_files_done);
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         _("Failed to write subject to SMTP-server."));
               (void)smtp_quit();
               exit(eval_timeout(WRITE_REMOTE_ERROR));
            }
            no_of_bytes += length;
         }
         else if (db.special_flag & FILE_NAME_IS_SUBJECT)
              {
                 length = snprintf(buffer, buffer_size, "%s", final_filename);
                 if (length >= buffer_size)
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                              "Buffer length for mail header to small!");
                    (void)smtp_quit();
                    exit(ALLOC_ERROR);
                 }
                 if (smtp_write_subject(buffer, &length, db.charset) < 0)
                 {
                    WHAT_DONE("mailed", file_size_done, no_of_files_done);
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                              _("Failed to write the filename as subject to SMTP-server."));
                    (void)smtp_quit();
                    exit(eval_timeout(WRITE_REMOTE_ERROR));
                 }
                 no_of_bytes += length;
              } /* if (db.special_flag & FILE_NAME_IS_SUBJECT) */

         /* Send MIME information. */
         if (db.special_flag & ATTACH_FILE)
         {
            size_t length;

            if (multipart_boundary[0] != '\0')
            {
               length = snprintf(buffer, buffer_size,
                                 "MIME-Version: 1.0 (produced by AFD %s)\r\nContent-Type: MULTIPART/MIXED; BOUNDARY=\"%s\"\r\n",
                                 PACKAGE_VERSION, multipart_boundary);
               if (length >= buffer_size)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Buffer length for mail header to small!");
                  (void)smtp_quit();
                  exit(ALLOC_ERROR);
               }
               buffer_ptr = buffer;
            }
            else
            {
               length = snprintf(encode_buffer, encode_buffer_size,
                                 "MIME-Version: 1.0 (produced by AFD %s)\r\nContent-Type: APPLICATION/octet-stream; name=\"%s\"\r\nContent-Transfer-Encoding: BASE64\r\n\r\n",
                                 PACKAGE_VERSION, final_filename);
               if (length >= encode_buffer_size)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Buffer length for mail header to small!");
                  (void)smtp_quit();
                  exit(ALLOC_ERROR);
               }
               buffer_ptr = encode_buffer;
            }

            if (smtp_write(buffer_ptr, NULL, length) < 0)
            {
               WHAT_DONE("mailed", file_size_done, no_of_files_done);
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         _("Failed to write start of multipart boundary to SMTP-server."));
               (void)smtp_quit();
               exit(eval_timeout(WRITE_REMOTE_ERROR));
            }
            no_of_bytes += length;
         } /* if (db.special_flag & ATTACH_FILE) */
         else if (db.charset != NULL)
              {
                 size_t length;

                 length = snprintf(buffer, buffer_size,
                                   "MIME-Version: 1.0 (produced by AFD %s)\r\nContent-Type: TEXT/plain; charset=%s\r\nContent-Transfer-Encoding: 8BIT\r\n",
                                   PACKAGE_VERSION, db.charset);
                 if (length >= buffer_size)
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                              "Buffer length for mail header to small!");
                    (void)smtp_quit();
                    exit(ALLOC_ERROR);
                 }
                 if (smtp_write(buffer, NULL, length) < 0)
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                              _("Failed to write MIME header with charset to SMTP-server."));
                    (void)smtp_quit();
                    exit(eval_timeout(WRITE_REMOTE_ERROR));
                 }
                 no_of_bytes += length;
              }
              else
              {
                 if (smtp_buffer != NULL)
                 {
                    smtp_buffer[0] = 0;
                 }
              }
         if (smtp_write("\r\n", NULL, 2) < 0)
         {
            WHAT_DONE("mailed", file_size_done, no_of_files_done);
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                      _("Failed to write carriage return line feed to mark end of header to SMTP-server."));
            (void)smtp_quit();
            exit(eval_timeout(WRITE_REMOTE_ERROR));
         }
         no_of_bytes += 2;

         for (;;)
         {
            for (j = 0; j < loops; j++)
            {
               if (read(fd, buffer, blocksize) != blocksize)
               {
                  WHAT_DONE("mailed", file_size_done, no_of_files_done);
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            _("Failed to read() from `%s' : %s"),
                            db.filename[files_send], strerror(errno));
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
                     WHAT_DONE("mailed", file_size_done, no_of_files_done);
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               _("Failed to write data from the source file to the SMTP-server."));
                     (void)smtp_quit();
                     exit(eval_timeout(WRITE_REMOTE_ERROR));
                  }
               }
               else
               {
                  if (smtp_write(buffer, smtp_buffer, blocksize) < 0)
                  {
                     WHAT_DONE("mailed", file_size_done, no_of_files_done);
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               _("Failed to write data from the source file to the SMTP-server."));
                     (void)smtp_quit();
                     exit(eval_timeout(WRITE_REMOTE_ERROR));
                  }
                  write_size = blocksize;
               }
               file_size_done += write_size;
               no_of_bytes += write_size;
            }

            if (rest > 0)
            {
               if (read(fd, buffer, rest) != rest)
               {
                  WHAT_DONE("mailed", file_size_done, no_of_files_done);
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            _("Failed to read() rest from `%s' : %s"),
                            db.filename[files_send], strerror(errno));
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
                     WHAT_DONE("mailed", file_size_done, no_of_files_done);
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               _("Failed to write the rest data from the source file to the SMTP-server."));
                     (void)smtp_quit();
                     exit(eval_timeout(WRITE_REMOTE_ERROR));
                  }
               }
               else
               {
                  if (smtp_write(buffer, smtp_buffer, rest) < 0)
                  {
                     WHAT_DONE("mailed", file_size_done, no_of_files_done);
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               _("Failed to write the rest data from the source file to the SMTP-server."));
                     (void)smtp_quit();
                     exit(eval_timeout(WRITE_REMOTE_ERROR));
                  }
                  write_size = rest;
               }
               file_size_done += write_size;
               no_of_bytes += write_size;
            } /* if (rest > 0) */

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
            if (statx(0, db.filename[files_send], AT_STATX_SYNC_AS_STAT,
                      STATX_SIZE, &stat_buf) == 0)
#else
            if (stat(db.filename[files_send], &stat_buf) == 0)
#endif
            {
#ifdef HAVE_STATX
               if (stat_buf.stx_size > local_file_size)
#else
               if (stat_buf.st_size > local_file_size)
#endif
               {
#ifdef HAVE_STATX
                  loops = (stat_buf.stx_size - local_file_size) / blocksize;
                  rest = (stat_buf.stx_size - local_file_size) % blocksize;
                  local_file_size = stat_buf.stx_size;
#else
                  loops = (stat_buf.st_size - local_file_size) / blocksize;
                  rest = (stat_buf.st_size - local_file_size) % blocksize;
                  local_file_size = stat_buf.st_size;
#endif

                  /*
                   * Give a warning in the system log, so some action
                   * can be taken against the originator.
                   */
                  (void)rec(sys_log_fd, WARN_SIGN,
                            _("Someone is still writting to file `%s'. (%s %d)\n"),
                            db.filename[files_send], __FILE__, __LINE__);
               }
               else
               {
                  break;
               }
            }
            else
            {
               break;
            }
         } /* for (;;) */

         /* Write boundary end if neccessary. */
         if (db.special_flag & ATTACH_FILE)
         {
            size_t length;

            /* Write boundary */
            length = snprintf(buffer, buffer_size,
                              "\r\n--%s--\r\n", multipart_boundary);
            if (length >= buffer_size)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Buffer length for mail header to small!");
               (void)smtp_quit();
               exit(ALLOC_ERROR);
            }

            if (smtp_write(buffer, NULL, length) < 0)
            {
               WHAT_DONE("mailed", file_size_done, no_of_files_done);
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         _("Failed to write end of multipart boundary to SMTP-server."));
               (void)smtp_quit();
               exit(eval_timeout(WRITE_REMOTE_ERROR));
            }
            no_of_bytes += length;
         }

         /* Close local file. */
         if (close(fd) < 0)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                      _("Failed to close() local file `%s' : %s"),
                      final_filename, strerror(errno));

            /*
             * Hmm. Lets hope this does not happen to offen. Else
             * we will have too many file descriptors open.
             */
         }

         /* Close remote file. */
         if ((status = smtp_close()) != SUCCESS)
         {
            WHAT_DONE("mailed", file_size_done, no_of_files_done);
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                      _("Failed to close DATA mode (%d)."), status);
            (void)smtp_quit();
            exit(eval_timeout(CLOSE_REMOTE_ERROR));
         }
         else
         {
            if (db.verbose == YES)
            {
               trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         _("Closing DATA mode."));
            }
         }

         no_of_files_done++;

         if (db.remove == YES)
         {
            /* Delete the file we just have send */
            if (unlink(db.filename[files_send]) < 0)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         _("Could not unlink() local file `%s' after sending it successfully : %s (%s %d)\n"),
                         strerror(errno), db.filename[files_send],
                         __FILE__, __LINE__);
            }
         }
      } /* for (files_send = 0; files_send < db.no_of_files; files_send++) */
   }

   WHAT_DONE("mailed", file_size_done, no_of_files_done);
   msg_str[0] = '\0';
   free(buffer);

   /* Logout again. */
   if ((status = smtp_quit()) != SUCCESS)
   {
      trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, msg_str,
                _("Failed to disconnect from SMTP-server (%d)."), status);
   }
   else
   {
      if (db.verbose == YES)
      {
         trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   _("Logged out."));
      }
   }

   /* Don't need the ASCII buffer. */
   if (db.special_flag == ATTACH_FILE)
   {
      free(encode_buffer);
   }

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++++ asmtp_exit() ++++++++++++++++++++++++++++*/
static void
asmtp_exit(void)
{
   if (db.filename != NULL)
   {
      FREE_RT_ARRAY(db.filename);
   }
   if (db.realname != NULL)
   {
      FREE_RT_ARRAY(db.realname);
   }
   return;
}


/*++++++++++++++++++++++++++++++ sig_pipe() +++++++++++++++++++++++++++++*/
static void
sig_pipe(int signo)
{
   /* Ignore any future signals of this kind. */
   if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, _("signal() error : %s (%s %d)\n"),
                strerror(errno), __FILE__, __LINE__);
   }
   sigpipe_flag = ON;

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   (void)rec(sys_log_fd, DEBUG_SIGN,
             _("Aaarrrggh! Received SIGSEGV. Remove the programmer who wrote this! (%s %d)\n"),
             __FILE__, __LINE__);
   exit(INCORRECT);
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   (void)rec(sys_log_fd, DEBUG_SIGN,
             _("Uuurrrggh! Received SIGBUS. (%s %d)\n"),
             __FILE__, __LINE__);
   exit(INCORRECT);
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   exit(INCORRECT);
}
