/*
 *  aftp.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2022 Deutscher Wetterdienst (DWD),
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

#include "aftpdefs.h"

DESCR__S_M1
/*
 ** NAME
 **   aftp - send files via FTP automaticaly
 **
 ** SYNOPSIS
 **   [r]aftp [options] [file 1 ... file n]
 **
 **   options
 **     --version                       - Show current version
 **     -A                              - Retrieve rest of file.
 **     -a <file size offset>           - offset of file name when doing a
 **                                       list command on the remote side.
 **     -b <block size>                 - FTP block size in bytes.
 **     -C[ <mode>]                     - If target directory does not exist
 **                                       create it. The optional mode can be
 **                                       used to set the permission of this
 **                                       directory.
 **     -c <config file>                - Use this configuration file instead
 **                                       of the -dpmu options.
 **     -d <remote directory>           - Directory where file(s) are to be
 **                                       stored.
 **     -f <filename file>              - List of filenames to send.
 **     -h <hostname|IP number>         - Recipient hostname or IP number.
 **     -I                              - Use implicit FTPS. Works only with
 **                                       -z or -Z.
 **     -k                              - Keep control connection alive.
 **     -l <DOT | DOT_VMS | OFF | xyz.> - How to lock the file on the remote
 **                                       site.
 **     -m <A | I | D>                  - FTP transfer mode, ASCII, binary or
 **                                       dos. Default is binary.
 **     -p <port number>                - Remote port number of FTP-server.
 **     -P <proxy>                      - Use the given proxy procedure
 **                                       to login.
 **     -u <user> <password>            - Remote user name and password.
 **     -r                              - Remove transmitted file.
 **     -s <file size>                  - File size of file to be transfered.
 **     -t <timout>                     - FTP timeout in seconds.
 **     -v                              - Verbose. Shows all FTP commands
 **                                       and the reply from the remote server.
 **     -X                              - Use extended mode active or passive
 **                                       (-x) mode.
 **     -x                              - Use passive mode instead of active
 **                                       mode.
 **     -y                              - Use TLS legacy renegotiation.
 **     -Y                              - Use strict SSL/TLS verification.
 **     -z                              - With SSL/TLS authentification for
 **                                       control connection.
 **     -Z                              - With SSL/TLS authentification for
 **                                       control and data connection.
 **     -?                              - Usage.
 **
 ** DESCRIPTION
 **   aftp sends the given files to the defined recipient via FTP.
 **   It does so by using it's own FTP-client.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   24.09.1997 H.Kiehl Created
 **   23.03.1999 H.Kiehl Integrated aftp into AFD source tree.
 **   17.10.2000 H.Kiehl Added support to retrieve files from a remote
 **                      host.
 **   14.09.2001 H.Kiehl Added ftp_keepalive() command.
 **   20.10.2001 H.Kiehl Added append option for partial retrieve.
 **   11.04.2004 H.Kiehl Added TLS/SSL support.
 **   07.02.2005 H.Kiehl Added DOT_VMS and DOS mode.
 **                      Do not exit when we fail to open local file.
 **   19.03.2006 H.Kiehl Added proxy support.
 **   05.03.2008 H.Kiehl Added chmod support.
 **   16.03.2009 H.Kiehl Added gettext supoort.
 **   30.03.2012 H.Kiehl Inform when we created a directory.
 **   06.03.2020 H.Kiehl Implement implicit FTPS.
 **   02.08.2022 H.Kiehl Added parameter -y to use TLS legacy renegotiation.
 **
 */
DESCR__E_M1

#include <stdio.h>                     /* fprintf(), sprintf()           */
#include <string.h>                    /* strcpy(), strcat(), strcmp(),  */
                                       /* strerror()                     */
#include <stdlib.h>                    /* malloc(), free()               */
#include <time.h>                      /* time()                         */
#include <ctype.h>                     /* isdigit()                      */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>                    /* signal()                       */
#include <unistd.h>                    /* unlink(), close()              */
#include <errno.h>
#include "ftpdefs.h"
#include "version.h"

/* Global variables. */
unsigned int         special_flag = 0;
int                  *no_of_listed_files,
                     sigpipe_flag,
                     simulation_mode = NO,
                     sys_log_fd = STDERR_FILENO,
                     transfer_log_fd = STDERR_FILENO,
                     timeout_flag;
#ifdef WITH_IP_DB
int                  use_ip_db = NO;
#endif
long                 transfer_timeout;
char                 host_deleted = NO,
                     msg_str[MAX_RET_MSG_LENGTH],
                     *p_work_dir = NULL;
struct data          db;
struct filename_list *rl;
const char           *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void          aftp_exit(void),
                     sig_bus(int),
                     sig_segv(int),
                     sig_pipe(int),
                     sig_exit(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int          exit_status = SUCCESS,
                fd = -1,
                status,
                no_of_files_done = 0;
   off_t        file_size_done = 0,
                file_size_to_retrieve,
                local_file_size,
                no_of_bytes;
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
   time_t       keep_alive_time;
#endif
   char         *ascii_buffer = NULL,
                append_count = 0,
                *buffer,
                *created_path = NULL,
                *file_ptr,
                initial_filename[MAX_FILENAME_LENGTH],
                final_filename[MAX_FILENAME_LENGTH];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

#ifdef HAVE_GETTEXT
   (void)setlocale(LC_ALL, "");
   bindtextdomain(PACKAGE, LOCALEDIR);
   textdomain(PACKAGE);
#endif
   CHECK_FOR_VERSION(argc, argv);

   /* Do some cleanups when we exit. */
   if (atexit(aftp_exit) != 0)
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
   init_aftp(argc, argv, &db);
   msg_str[0] = '\0';

   /* Set FTP timeout value. */
   transfer_timeout = db.transfer_timeout;

   /*
    * In ASCII-mode an extra buffer is needed to convert LF's
    * to CRLF. By creating this buffer the function ftp_write()
    * knows it has to send the data in ASCII-mode.
    */
   if ((db.transfer_mode == 'A') || (db.transfer_mode == 'D'))
   {
      if (db.transfer_mode == 'D')
      {
         db.transfer_mode = 'I';
      }
      if ((ascii_buffer = malloc(((db.blocksize * 2) + 1))) == NULL)
      {
         (void)rec(sys_log_fd, ERROR_SIGN, _("malloc() error : %s (%s %d)\n"),
                   strerror(errno), __FILE__, __LINE__);
         exit(ALLOC_ERROR);
      }
   }

   sigpipe_flag = timeout_flag = OFF;

   /* Connect to remote FTP-server. */
#ifdef WITH_SSL
   if (((db.tls_auth == YES) || (db.tls_auth == BOTH)) &&
       (db.implicit_ftps == YES))
   {
      status = ftp_connect(db.hostname, db.port, YES, db.strict,
                           db.legacy_renegotiation);
   }
   else
   {
      status = ftp_connect(db.hostname, db.port, NO, NO, NO);
   }
#else
   status = ftp_connect(db.hostname, db.port);
#endif
   if ((status != SUCCESS) && (status != 230))
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                _("FTP %s connection to %s at port %d failed (%d)."),
                db.mode_str, db.hostname, db.port, status);
      exit(eval_timeout(CONNECT_ERROR));
   }
   else
   {
      if (db.verbose == YES)
      {
         if (status == 230)
         {
            trans_log(INFO_SIGN, NULL, 0, NULL, msg_str,
                      _("Connected (%s). No login required."), db.mode_str);
         }
         else
         {
            trans_log(INFO_SIGN, NULL, 0, NULL, msg_str,
                      _("Connected (%s)."), db.mode_str);
         }
      }

      if (db.special_flag & CREATE_TARGET_DIR)
      {
         if ((created_path = malloc(2048)) == NULL)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      _("malloc() error : %s (%s %d)\n"),
                      strerror(errno), __FILE__, __LINE__);
         }
         else
         {
            created_path[0] = '\0';
         }
      }
   }

#ifdef WITH_SSL
   if (((db.tls_auth == YES) || (db.tls_auth == BOTH)) &&
       (db.implicit_ftps != YES))
   {
      if (ftp_ssl_auth(db.strict, db.legacy_renegotiation) == INCORRECT)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   _("SSL/TSL connection to server `%s' failed."), db.hostname);
         (void)ftp_quit();
         exit(AUTH_ERROR);
      }
      else
      {
         if (db.verbose == YES)
         {
            trans_log(INFO_SIGN, NULL, 0, NULL, msg_str,
                      _("Authentification successful."));
         }
      }
   }
#endif

   /* Login */
   if (status != 230)
   {
      if (db.proxy_name[0] == '\0')
      {
         /* Send user name. */
         if ((status = ftp_user(db.user)) != SUCCESS)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                      _("Failed to send user `%s' (%d)."), db.user, status);
            (void)ftp_quit();
            exit(eval_timeout(USER_ERROR));
         }
         else
         {
            if (db.verbose == YES)
            {
               trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         _("Entered user name `%s'."), db.user);
            }
         }

         /* Send password (if required). */
         if (status != 230)
         {
            if ((status = ftp_pass(db.password)) != SUCCESS)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         _("Failed to send password for user `%s' (%d)."),
                         db.user, status);
               (void)ftp_quit();
               exit(eval_timeout(PASSWORD_ERROR));
            }
            else
            {
               if (db.verbose == YES)
               {
                  trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                            _("Logged in as `%s'."), db.user);
               }
            }
         } /* if (status != 230) */
      }
      else
      {
         int  i,
              status = 0;
         char buffer[MAX_USER_NAME_LENGTH],
              *proxy_ptr = db.proxy_name,
              *ptr;

         do
         {
            if (*proxy_ptr == '$')
            {
               ptr = proxy_ptr + 2;
               switch (*(proxy_ptr + 1))
               {
                  case 'a' :
                  case 'A' :
                  case 'u' :
                  case 'U' : /* Enter user name. */
                     i = 0;
                     while ((*ptr != ';') && (*ptr != '$') && (*ptr != '\0') &&
                            (i < MAX_USER_NAME_LENGTH))
                     {
                        if (*ptr == '\\')
                        {
                           ptr++;
                        }
                        buffer[i] = *ptr;
                        ptr++; i++;
                     }
                     if (i == MAX_USER_NAME_LENGTH)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  _("User name in proxy definition is to long (> %d)."),
                                  MAX_USER_NAME_LENGTH - 1);
                        (void)ftp_quit();
                        exit(USER_ERROR);
                     }
                     buffer[i] = '\0';
                     if (buffer[0] == '\0')
                     {
                        (void)strcpy(buffer, db.user);
                     }

                     if ((*(proxy_ptr + 1) == 'U') || (*(proxy_ptr + 1) == 'u'))
                     {
                        /* Send user name. */
                        if (((status = ftp_user(buffer)) != SUCCESS) && (status != 230))
                        {
                           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                                     _("Failed to send user `%s' (%d) [Proxy]."),
                                     buffer, status);
                           (void)ftp_quit();
                           exit(USER_ERROR);
                        }
                        else
                        {
                           if (db.verbose == YES)
                           {
                              if (status != 230)
                              {
                                 trans_log(INFO_SIGN, NULL, 0, NULL, msg_str,
                                           _("Entered user name `%s' [Proxy]."),
                                           buffer);
                              }
                              else
                              {
                                 trans_log(INFO_SIGN, NULL, 0, NULL, msg_str,
                                           _("Entered user name `%s' [Proxy]. No password required, logged in."),
                                           buffer);
                              }
                           }
                        }
                     }
                     else
                     {
                        /* Send account name. */
                        if (((status = ftp_account(buffer)) != SUCCESS) && (status != 230))
                        {
                           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                                     _("Failed to send account `%s' (%d) [Proxy]."),
                                     buffer, status);
                           (void)ftp_quit();
                           exit(USER_ERROR);
                        }
                        else
                        {
                           if (db.verbose == YES)
                           {
                              if (status != 230)
                              {
                                 trans_log(INFO_SIGN, NULL, 0, NULL, msg_str,
                                           _("Entered account name `%s' [Proxy]."),
                                           buffer);
                              }
                              else
                              {
                                 trans_log(INFO_SIGN, NULL, 0, NULL, msg_str,
                                           _("Entered account name `%s' [Proxy]. No password required, logged in."),
                                           buffer);
                              }
                           }
                        }
                     }

                     /* Don't forget to position the proxy pointer. */
                     if (*ptr == ';')
                     {
                        proxy_ptr = ptr + 1;
                     }
                     else
                     {
                        proxy_ptr = ptr;
                     }
                     break;

                  case 'p' :
                  case 'P' : /* Enter passwd. */
                     i = 0;
                     while ((*ptr != ';') && (*ptr != '$') && (*ptr != '\0') &&
                            (i < MAX_USER_NAME_LENGTH))
                     {
                        if (*ptr == '\\')
                        {
                           ptr++;
                        }
                        buffer[i] = *ptr;
                        ptr++; i++;
                     }
                     if (i == MAX_USER_NAME_LENGTH)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  _("Password in proxy definition is to long (> %d)."),
                                  MAX_USER_NAME_LENGTH - 1);
                        (void)ftp_quit();
                        exit(USER_ERROR);
                     }
                     buffer[i] = '\0';
                     if (buffer[0] == '\0')
                     {
                        (void)strcpy(buffer, db.password);
                     }

                     /* Maybe the passwd is not required, so make sure! */
                     if (status != 230)
                     {
                        if ((status = ftp_pass(buffer)) != SUCCESS)
                        {
                           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                                     _("Failed to send password (%d)."), status);
                           (void)ftp_quit();
                           exit(PASSWORD_ERROR);
                        }
                        else
                        {
                           if (db.verbose == YES)
                           {
                              trans_log(INFO_SIGN, NULL, 0, NULL, msg_str,
                                        _("Entered password."));
                           }
                        }
                     }

                     /* Don't forget to position the proxy pointer. */
                     if (*ptr == ';')
                     {
                        proxy_ptr = ptr + 1;
                     }
                     else
                     {
                        proxy_ptr = ptr;
                     }
                     break;

                  default : /* Syntax error in proxy format. */
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               _("Syntax error in proxy string `%s'."),
                               db.proxy_name);
                     (void)ftp_quit();
                     exit(USER_ERROR);
               }
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         _("Syntax error in proxy string `%s'."),
                         db.proxy_name);
               (void)ftp_quit();
               exit(USER_ERROR);
            }
         } while (*proxy_ptr != '\0');
      }
   } /* if (status != 230) */

#ifdef WITH_SSL
   if (db.tls_auth > NO)
   {
      if (ftp_ssl_init(db.tls_auth) == INCORRECT)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   _("SSL/TSL initialisation failed."));
         (void)ftp_quit();
         exit(AUTH_ERROR);
      }
      else
      {
         if (db.verbose == YES)
         {
            trans_log(INFO_SIGN, NULL, 0, NULL, msg_str,
                      _("SSL/TLS initialisation successful."));
         }
      }
   }
#endif

   /* Set transfer mode. */
   if ((status = ftp_type(db.transfer_mode)) != SUCCESS)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                _("Failed to set transfer mode to %c (%d)."),
                db.transfer_mode, status);
      (void)ftp_quit();
      exit(eval_timeout(TYPE_ERROR));
   }
   else
   {
      if (db.verbose == YES)
      {
         trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   _("Changed transfer mode to %c"), db.transfer_mode);
      }
   }

   /* Change directory if necessary. */
   if (db.remote_dir[0] != '\0')
   {
      if ((status = ftp_cd(db.remote_dir, db.create_target_dir,
                           db.dir_mode_str, created_path)) != SUCCESS)
      {
         if (db.create_target_dir == YES)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                      _("Failed to change/create directory to %s (%d)."),
                      db.remote_dir, status);
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                      _("Failed to change directory to %s (%d)."),
                      db.remote_dir, status);
         }
         (void)ftp_quit();
         exit(eval_timeout(CHDIR_ERROR));
      }
      else
      {
         if (db.verbose == YES)
         {
            trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                      _("Changed directory to %s"), db.remote_dir);
         }
         if ((created_path != NULL) && (created_path[0] != '\0'))
         {
            trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
                      "Created directory `%s'.", created_path);
            created_path[0] = '\0';
         }
      }
   } /* if (db.remote_dir[0] != '\0') */

   /* Allocate buffer to read data from the source file. */
   if ((buffer = malloc(db.blocksize + 4)) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, _("malloc() error : %s (%s %d)\n"),
                strerror(errno), __FILE__, __LINE__);
      exit(ALLOC_ERROR);
   }

   if (db.exec_mode == RETRIEVE_MODE)
   {
      if (get_remote_file_names_ftp_cmd(&file_size_to_retrieve) > 0)
      {
         int   i;
         off_t offset;
         char  local_file[MAX_PATH_LENGTH];

         local_file[0] = '.';
         for (i = 0; i < *no_of_listed_files; i++)
         {
            (void)strcpy(&local_file[1], rl[i].file_name);
            if (db.append == YES)
            {
#ifdef HAVE_STATX
               if (statx(0, rl[i].file_name, AT_STATX_SYNC_AS_STAT,
                         STATX_SIZE, &stat_buf) == -1)
#else
               if (stat(rl[i].file_name, &stat_buf) == -1)
#endif
               {
#ifdef HAVE_STATX
                  if (statx(0, local_file, AT_STATX_SYNC_AS_STAT,
                            STATX_SIZE, &stat_buf) == -1)
#else
                  if (stat(local_file, &stat_buf) == -1)
#endif
                  {
                     offset = 0;
                  }
                  else
                  {
#ifdef HAVE_STATX
                     offset = stat_buf.stx_size;
#else
                     offset = stat_buf.st_size;
#endif
                  }
               }
               else
               {
#ifdef HAVE_STATX
                  offset = stat_buf.stx_size;
#else
                  offset = stat_buf.st_size;
#endif
                  if (offset > 0)
                  {
                     if (rename(rl[i].file_name, local_file) == -1)
                     {
                        offset = 0;
                     }
                  }
               }
            }
            else
            {
#ifdef HAVE_STATX
               if (statx(0, local_file, AT_STATX_SYNC_AS_STAT,
                         STATX_SIZE, &stat_buf) == -1)
#else
               if (stat(local_file, &stat_buf) == -1)
#endif
               {
                  offset = 0;
               }
               else
               {
#ifdef HAVE_STATX
                  offset = stat_buf.stx_size;
#else
                  offset = stat_buf.st_size;
#endif
               }
            }
            if (((status = ftp_data(rl[i].file_name, offset, db.ftp_mode,
                                    DATA_READ, db.rcvbuf_size,
                                    db.create_target_dir, db.dir_mode_str,
                                    created_path)) != SUCCESS) &&
                (status != -550))
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         _("Failed to open remote file %s (%d)."),
                         rl[i].file_name, status);
               (void)ftp_quit();
               exit(eval_timeout(OPEN_REMOTE_ERROR));
            }
            if (status == -550) /* ie. file has been deleted or is NOT a file. */
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         _("Failed to open remote file %s (%d)."),
                         rl[i].file_name, status);
            }
            else /* status == SUCCESS */
            {
               off_t bytes_done;

               if (db.verbose == YES)
               {
                  trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                            _("Opened data connection for file %s."),
                            rl[i].file_name);
               }
               if ((created_path != NULL) && (created_path[0] != '\0'))
               {
                  trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Created directory `%s'.", created_path);
                  created_path[0] = '\0';
               }
#ifdef WITH_SSL
               if (db.tls_auth == BOTH)         
               {
                  if (ftp_auth_data() == INCORRECT)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                               _("TSL/SSL data connection to server `%s' failed."),
                               db.hostname);
                     (void)ftp_quit();
                     exit(eval_timeout(AUTH_ERROR));
                  }
                  else
                  {
                     if (db.verbose == YES)
                     {
                        trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                                  _("Authentification successful."));
                     }
                  }
               }
#endif

               if (offset > 0)
               {
                  fd = open(local_file, O_WRONLY | O_APPEND);
               }
               else
               {
                  fd = open(local_file, O_WRONLY | O_CREAT, FILE_MODE);
               }
               if (fd == -1)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            _("Failed to open local file %s : %s"),
                            local_file, strerror(errno));
                  (void)ftp_quit();
                  exit(OPEN_LOCAL_ERROR);
               }
               else
               {
                  if (db.verbose == YES)
                  {
                     trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
                               _("Opened local file %s."), local_file);
                  }
               }
               bytes_done = 0;
               do
               {
                  if ((status = ftp_read(buffer, db.blocksize)) == INCORRECT)
                  {
                     if ((sigpipe_flag == ON) && (status != EPIPE))
                     {
                        (void)ftp_get_reply();
                     }
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               ((sigpipe_flag == ON) && (status != EPIPE)) ? msg_str : NULL,
                               _("Failed to read from remote file %s (%d)"),
                               rl[i].file_name, status);
                     if (status == EPIPE)
                     {
                        trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  _("Hmm. Pipe is broken. Will NOT send a QUIT."));
                     }
                     else
                     {
                        (void)ftp_quit();
                     }
                     exit(eval_timeout(READ_REMOTE_ERROR));
                  }
                  else if (status > 0)
                       {
                          if (write(fd, buffer, status) != status)
                          {
                             trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                       _("Failed to write() to file %s : %s"),
                                       local_file, strerror(errno));
                             (void)ftp_quit();
                             exit(WRITE_LOCAL_ERROR);
                          }
                          bytes_done += status;
                       }
               } while (status != 0);

               /* Close the FTP data connection. */
               if ((status = ftp_close_data()) != SUCCESS)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                            _("Failed to close data connection (%d)."), status);
                  (void)ftp_quit();
                  exit(eval_timeout(CLOSE_REMOTE_ERROR));
               }
               else
               {
                  if (db.verbose == YES)
                  {
                     trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                               _("Closed data connection for file %s."),
                               rl[i].file_name);
                  }
               }

               /* Close the local file. */
               if ((fd != -1) && (close(fd) == -1))
               {
                  trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                            _("Failed to close() local file %s."), local_file);
               }
               else
               {
                  if (db.verbose == YES)
                  {
                     trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
                               _("Closed local file %s."), local_file);
                  }
               }
               /* Check if remote file is to be deleted. */
               if (db.remove == YES)
               {
                  if ((status = ftp_dele(rl[i].file_name)) != SUCCESS)
                  {
                     trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, msg_str,
                               _("Failed to delete remote file %s (%d)."),
                               rl[i].file_name, status);
                  }
                  else
                  {
                     if (db.verbose == YES)
                     {
                        trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                                  _("Deleted remote file %s."), rl[i].file_name);
                     }
                  }
               }

               /*
                * If the file size is not the same as the one when we did
                * the remote ls command, give a warning in the transfer log
                * so some action can be taken against the originator.
                */
               if ((rl[i].size != -1) && ((bytes_done + offset) != rl[i].size))
               {
                  trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                            _("File size of file %s changed from %u to %u when it was retrieved."),
                            rl[i].file_name, rl[i].size, bytes_done + offset);
                  rl[i].size = bytes_done;
               }

               /* Rename the file to indicate that download is done. */
               if (rename(local_file, &local_file[1]) == -1)
               {
                  trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                            _("Failed to rename() %s to %s : %s"),
                            local_file, &local_file[1], strerror(errno));
               }
               else
               {
                  no_of_files_done++;
                  trans_log(INFO_SIGN, NULL, 0, NULL, NULL,
#if SIZEOF_OFF_T == 4
                            _("Retrieved %s [%ld bytes]"),
#else
                            _("Retrieved %s [%lld bytes]"),
#endif
                            rl[i].file_name, (pri_off_t)bytes_done);
                  file_size_done += bytes_done;
                  if (offset > 0)
                  {
                     append_count++;
                  }
               }
            }
         }
      }
#if SIZEOF_OFF_T == 4
      (void)sprintf(msg_str, _("%ld bytes retrieved in %d file(s)."),
#else
      (void)sprintf(msg_str, _("%lld bytes retrieved in %d file(s)."),
#endif
                    (pri_off_t)file_size_done, no_of_files_done);

       if (append_count == 1)
       {
          (void)strcat(msg_str, _(" [APPEND]"));
       }
       else if (append_count > 1)
            {
               char tmp_buffer[40];

               (void)sprintf(tmp_buffer, _(" [APPEND * %d]"), append_count);
               (void)strcat(msg_str, tmp_buffer);
            }
      trans_log(INFO_SIGN, NULL, 0, NULL, NULL, "%s", msg_str);
      msg_str[0] = '\0';
   }
   else /* Send data. */
   {
      int    files_send = 0,
             local_file_not_found = 0,
             loops,
             rest;
      size_t length;
      off_t  append_offset = 0;
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
      int    keep_alive_timeout = transfer_timeout - 5;

      if (db.keepalive == YES)
      {
         if (keep_alive_timeout < MIN_KEEP_ALIVE_INTERVAL)
         {
            keep_alive_timeout = MIN_KEEP_ALIVE_INTERVAL;
         }
      }
#endif

      /* Send all files. */
      for (files_send = 0; files_send < db.no_of_files; files_send++)
      {
         if (db.exec_mode == TEST_MODE)
         {
            (void)sprintf(final_filename, "%s%010d",
                          db.filename[0], files_send);
         }
         else
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
         }

         /* Send file in dot notation? */
         if ((db.lock == DOT) || (db.lock == DOT_VMS))
         {
            (void)strcpy(initial_filename, db.lock_notation);
            (void)strcat(initial_filename, final_filename);
         }
         else
         {
            (void)strcpy(initial_filename, final_filename);
         }

         if (db.exec_mode == TEST_MODE)
         {
            local_file_size = db.dummy_size;
         }
         else
         {
            /* Open local file. */
#ifdef O_LARGEFILE
            if ((fd = open(db.filename[files_send], O_RDONLY | O_LARGEFILE)) == -1)
#else
            if ((fd = open(db.filename[files_send], O_RDONLY)) == -1)
#endif
            {
               if (db.verbose == YES)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            _("Failed to open() local file %s : %s"),
                            db.filename[files_send], strerror(errno));
               }
               local_file_not_found++;

               /* Lets just continue with the next file. */
               continue;
            }

#ifdef HAVE_STATX
            if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                      STATX_SIZE | STATX_MODE, &stat_buf) == -1)
#else
            if (fstat(fd, &stat_buf) == -1)
#endif
            {
               if (db.verbose == YES)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            _("Failed to access local file %s"),
                            db.filename[files_send]);
               }
               WHAT_DONE("send", file_size_done, no_of_files_done);
               (void)ftp_quit();
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
                               _("Local file %s is not a regular file."),
                               db.filename[files_send]);
                  }
                  local_file_not_found++;
                  (void)close(fd);

                  /* Lets just continue with the next file. */
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
#if SIZEOF_OFF_T == 4
                         _("Opened local file %s with %ld byte."),
#else
                         _("Opened local file %s with %lld byte."),
#endif
                         db.filename[files_send], (pri_off_t)local_file_size);
            }

            /*
             * Check if the file has not already been partly
             * transmitted. If so, lets first get the size of the
             * remote file, to append it.
             */
            append_offset = 0;
            if (db.file_size_offset != -1)
            {
               if (db.file_size_offset == AUTO_SIZE_DETECT)
               {
                  off_t remote_size;

                  if ((status = ftp_size(initial_filename, &remote_size)) != SUCCESS)
                  {
                     trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, msg_str,
                               _("Failed to send SIZE command for file %s (%d)."),
                               initial_filename, status);
                     if (timeout_flag == ON)
                     {
                        timeout_flag = OFF;
                     }
                  }
                  else
                  {
                     append_offset = remote_size;
                     if (db.verbose == YES)
                     {
                        trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
#if SIZEOF_OFF_T == 4
                                  _("Remote size of %s is %ld."),
#else
                                  _("Remote size of %s is %lld."),
#endif
                                  initial_filename, (pri_off_t)remote_size);
                     }
                  }
               }
               else
               {
                  int  type;
                  char line_buffer[MAX_RET_MSG_LENGTH];

#ifdef WITH_SSL
                  if (db.tls_auth == BOTH)
                  {
                     type = LIST_CMD | ENCRYPT_DATA;
                  }
                  else
                  {
#endif
                     type = LIST_CMD;
#ifdef WITH_SSL
                  }
#endif
                  if ((status = ftp_list(db.ftp_mode, type, initial_filename,
                                         line_buffer)) != SUCCESS)
                  {
                     trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, msg_str,
                               _("Failed to send LIST command for file %s (%d)."),
                               initial_filename, status);
                     if (timeout_flag == ON)
                     {
                        timeout_flag = OFF;
                     }
                  }
                  else
                  {
                     int  space_count = 0;
                     char *p_end_line,
                          *ptr = line_buffer;

                     /*
                      * Cut out remote file size, from ls command.
                      */
                     p_end_line = line_buffer + strlen(line_buffer);
                     do
                     {
                        while ((*ptr != ' ') && (*ptr != '\t') &&
                               (ptr < p_end_line))
                        {
                           ptr++;
                        }
                        if ((*ptr == ' ') || (*ptr == '\t'))
                        {
                           space_count++;
                           while (((*ptr == ' ') || (*ptr == '\t')) &&
                                  (ptr < p_end_line))
                           {
                              ptr++;
                           }
                        }
                        else
                        {
                           (void)rec(sys_log_fd, WARN_SIGN,
                                     _("The <file size offset> for host %s is to large! (%s %d)\n"),
                                     db.hostname, __FILE__, __LINE__);
                           space_count = -1;
                           break;
                        }
                     } while (space_count != db.file_size_offset);

                     if ((space_count > -1) &&
                         (space_count == db.file_size_offset))
                     {
                        char *p_end = ptr;

                        while ((isdigit((int)(*p_end) != 0)) &&
                               (p_end < p_end_line))
                        {
                           p_end++;
                        }
                        *p_end = '\0';
                        append_offset = atoi(ptr);
                     }
                  }
               }
               if (append_offset > 0)
               {
                  if ((local_file_size - append_offset) > 0)
                  {
                     if (lseek(fd, append_offset, SEEK_SET) < 0)
                     {
                        append_offset = 0;
                        trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  _("Failed to seek() in %s (Ignoring append): %s"),
                                  final_filename, strerror(errno));
                        if (db.verbose == YES)
                        {
                           trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                                     _("Failed to seek() in %s (Ignoring append): %s"),
                                     final_filename, strerror(errno));
                        }
                     }
                     else
                     {
                        append_count++;
                        if (db.verbose == YES)
                        {
                           trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
                                     _("Appending file %s."), final_filename);
                        }
                     }
                  }
                  else
                  {
                     append_offset = 0;
                  }
               }
            }
         }

         /* Open file on remote site. */
         if ((status = ftp_data(initial_filename, append_offset,
                                db.ftp_mode, DATA_WRITE, db.sndbuf_size,
                                db.create_target_dir, db.dir_mode_str,
                                created_path)) != SUCCESS)
         {
            WHAT_DONE("send", file_size_done, no_of_files_done);
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                      _("Failed to open remote file %s (%d)."),
                      initial_filename, status);
            (void)ftp_quit();
            exit(eval_timeout(OPEN_REMOTE_ERROR));
         }
         else
         {
            if (db.verbose == YES)
            {
               trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         _("Open remote file %s"), initial_filename);
            }
            if ((created_path != NULL) && (created_path[0] != '\0'))
            {
               trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Created directory `%s'.", created_path);
               created_path[0] = '\0';
            }
         }
#ifdef WITH_SSL
         if (db.tls_auth == BOTH)
         {
            if (ftp_auth_data() == INCORRECT)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         _("TSL/SSL data connection to server `%s' failed."),
                         db.hostname);
               (void)ftp_quit();
               exit(eval_timeout(AUTH_ERROR));
            }
            else
            {
               if (db.verbose == YES)
               {
                  trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                            _("Authentification successful."));
               }
            }
         }
#endif

#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
         if (db.keepalive == YES)
         {
            keep_alive_time = time(NULL);
         }
         else
         {
            keep_alive_time = 0; /* Silence compiler. */
         }
#endif

         /* Read (local) and write (remote) file. */
         no_of_bytes = 0;
         loops = (local_file_size - append_offset) / db.blocksize;
         rest = (local_file_size - append_offset) % db.blocksize;
         if (ascii_buffer != NULL)
         {
            ascii_buffer[0] = 0;
         }

         if (db.exec_mode == TRANSFER_MODE)
         {
            int j;

            for (;;)
            {
               for (j = 0; j < loops; j++)
               {
                  if (read(fd, buffer, db.blocksize) != db.blocksize)
                  {
                     if (db.verbose == YES)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  _("Could not read local file %s : %s"),
                                  final_filename, strerror(errno));
                     }
                     WHAT_DONE("send", file_size_done, no_of_files_done);
                     (void)ftp_quit();
                     exit(READ_LOCAL_ERROR);
                  }

                  if ((status = ftp_write(buffer, ascii_buffer,
                                          db.blocksize)) != SUCCESS)
                  {
                     /*
                      * It could be that we have received a SIGPIPE
                      * signal. If this is the case there might be data
                      * from the remote site on the control connection.
                      * Try to read this data into the global variable
                      * 'msg_str'.
                      */
                     if (sigpipe_flag == ON)
                     {
                        (void)ftp_get_reply();
                     }
                     WHAT_DONE("send", file_size_done, no_of_files_done);
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               (sigpipe_flag == ON) ? msg_str : NULL,
#if SIZEOF_OFF_T == 4
                               _("Failed to write to remote file %s after writing %ld bytes."),
#else
                               _("Failed to write to remote file %s after writing %lld bytes."),
#endif
                               initial_filename, (pri_off_t)file_size_done);
                     if (status == EPIPE)
                     {
                        trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  _("Hmm. Pipe is broken. Will NOT send a QUIT."));
                     }
                     else
                     {
                        (void)ftp_quit();
                     }
                     exit(eval_timeout(WRITE_REMOTE_ERROR));
                  }

                  file_size_done += db.blocksize;
                  no_of_bytes += db.blocksize;

#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
                  if (db.keepalive == YES)
                  {
                     time_t tmp_time = time(NULL);

                     if ((tmp_time - keep_alive_time) >= keep_alive_timeout)
                     {
                        keep_alive_time = tmp_time;
                        if ((status = ftp_keepalive()) != SUCCESS)
                        {
                           trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, msg_str,
                                     _("Failed to send STAT command (%d)."),
                                     status);
                           if (timeout_flag == ON)
                           {
                              timeout_flag = OFF;
                           }
                        }
                        else if (db.verbose == YES)
                             {
                                trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                                          _("Send STAT command."));
                             }
                     }
                  }
#endif /* FTP_CTRL_KEEP_ALIVE_INTERVAL */
               }
               if (rest > 0)
               {
                  if (read(fd, buffer, rest) != rest)
                  {
                     if (db.verbose == YES)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  _("Could not read local file %s : %s"),
                                  final_filename, strerror(errno));
                     }
                     WHAT_DONE("send", file_size_done, no_of_files_done);
                     (void)ftp_quit();
                     exit(READ_LOCAL_ERROR);
                  }
                  if ((status = ftp_write(buffer, ascii_buffer,
                                          rest)) != SUCCESS)
                  {
                     /*
                      * It could be that we have received a SIGPIPE
                      * signal. If this is the case there might be data
                      * from the remote site on the control connection.
                      * Try to read this data into the global variable
                      * 'msg_str'.
                      */
                     if (sigpipe_flag == ON)
                     {
                        (void)ftp_get_reply();
                     }
                     WHAT_DONE("send", file_size_done, no_of_files_done);
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               (sigpipe_flag == ON) ? msg_str : NULL,
                               _("Failed to write rest to remote file %s"),
                               initial_filename);
                     if (status == EPIPE)
                     {
                        trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  _("Hmm. Pipe is broken. Will NOT send a QUIT."));
                     }
                     else
                     {
                        (void)ftp_quit();
                     }
                     exit(eval_timeout(WRITE_REMOTE_ERROR));
                  }

                  file_size_done += rest;
                  no_of_bytes += rest;
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
                     loops = (stat_buf.stx_size - local_file_size) / db.blocksize;
                     rest = (stat_buf.stx_size - local_file_size) % db.blocksize;
                     local_file_size = stat_buf.stx_size;
#else
                     loops = (stat_buf.st_size - local_file_size) / db.blocksize;
                     rest = (stat_buf.st_size - local_file_size) % db.blocksize;
                     local_file_size = stat_buf.st_size;
#endif

                     /*
                      * Give a warning in the system log, so some action
                      * can be taken against the originator.
                      */
                     (void)rec(sys_log_fd, WARN_SIGN,
                               _("Someone is still writting to file %s. (%s %d)\n"),
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

            /* Close local file. */
            if (close(fd) < 0)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                         _("Failed to close() local file %s : %s"),
                         final_filename, strerror(errno));

               /*
                * Hmm. Lets hope this does not happen to offen. Else
                * we will have too many file descriptors open.
                */
            }
         }
         else /* TEST_MODE, write dummy files. */
         {
            int j;

            for (j = 0; j < loops; j++)
            {
               if ((status = ftp_write(buffer, ascii_buffer,
                                       db.blocksize)) != SUCCESS)
               {
                  /*
                   * It could be that we have received a SIGPIPE
                   * signal. If this is the case there might be data
                   * from the remote site on the control connection.
                   * Try to read this data into the global variable
                   * 'msg_str'.
                   */
                  if (sigpipe_flag == ON)
                  {
                     (void)ftp_get_reply();
                  }
                  WHAT_DONE("send", file_size_done, no_of_files_done);
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                            (sigpipe_flag == ON) ? msg_str : NULL,
#if SIZEOF_OFF_T == 4
                            _("Failed to write to remote file %s after writing %ld bytes."),
#else
                            _("Failed to write to remote file %s after writing %lld bytes."),
#endif
                            initial_filename, (pri_off_t)file_size_done);
                  if (status == EPIPE)
                  {
                     trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
                               _("Hmm. Pipe is broken. Will NOT send a QUIT."));
                  }
                  else
                  {
                     (void)ftp_quit();
                  }
                  exit(eval_timeout(WRITE_REMOTE_ERROR));
               }

               file_size_done += db.blocksize;
               no_of_bytes += db.blocksize;
            }
            if (rest > 0)
            {
               if ((status = ftp_write(buffer, ascii_buffer, rest)) != SUCCESS)
               {
                  /*
                   * It could be that we have received a SIGPIPE
                   * signal. If this is the case there might be data
                   * from the remote site on the control connection.
                   * Try to read this data into the global variable
                   * 'msg_str'.
                   */
                  if (sigpipe_flag == ON)
                  {
                     (void)ftp_get_reply();
                  }
                  WHAT_DONE("send", file_size_done, no_of_files_done);
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                            (sigpipe_flag == ON) ? msg_str : NULL,
                            _("Failed to write rest to remote file %s"),
                            initial_filename);
                  if (status == EPIPE)
                  {
                     trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
                               _("Hmm. Pipe is broken. Will NOT send a QUIT."));
                  }
                  else
                  {
                     (void)ftp_quit();
                  }
                  exit(eval_timeout(WRITE_REMOTE_ERROR));
               }

               file_size_done += rest;
               no_of_bytes += rest;
            }
         }

         /* Close remote file. */
         if ((status = ftp_close_data()) != SUCCESS)
         {
            /*
             * Closing files that have zero length is not possible
             * on some systems. So if this is the case lets not count
             * this as an error. Just ignore it, but send a message in
             * the transfer log, so the user sees that he is trying
             * to send files with zero length.
             */
            if ((local_file_size > 0) || (timeout_flag == ON))
            {
               WHAT_DONE("send", file_size_done, no_of_files_done);
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         _("Failed to close remote file %s"), initial_filename);
               (void)ftp_quit();
               exit(eval_timeout(CLOSE_REMOTE_ERROR));
            }
            else
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, msg_str,
#if SIZEOF_OFF_T == 4
                         _("Failed to close remote file %s (%d). Ignoring since file size is %ld."),
#else
                         _("Failed to close remote file %s (%d). Ignoring since file size is %lld."),
#endif
                         initial_filename, status, (pri_off_t)local_file_size);
            }
         }
         else
         {
            if (db.verbose == YES)
            {
               trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         _("Closed remote file %s"), initial_filename);
            }
         }

         if (db.chmod_str[0] != '\0')
         {
            if ((status = ftp_chmod(initial_filename,
                                    db.chmod_str)) != SUCCESS)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         _("Failed to chmod remote file `%s' to %s (%d)."),
                         initial_filename, db.chmod_str, status);
               if (timeout_flag == ON)
               {
                  timeout_flag = OFF;
               }
            }
            else if (db.verbose == YES)
                 {
                    trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                              _("Changed mode of remote file `%s' to %s"),
                              initial_filename, db.chmod_str);
                 }

         }

         if (db.verbose == YES)
         {
            int  type;
            char line_buffer[MAX_RET_MSG_LENGTH];

#ifdef WITH_SSL
            if (db.tls_auth == BOTH)
            {
               type = LIST_CMD | ENCRYPT_DATA;
            }
            else
            {
#endif
               type = LIST_CMD;
#ifdef WITH_SSL
            }
#endif
            if ((status = ftp_list(db.ftp_mode, type, initial_filename,
                                   line_buffer)) != SUCCESS)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         _("Failed to list remote file %s (%d)."),
                         initial_filename, status);
               if (timeout_flag == ON)
               {
                  timeout_flag = OFF;
               }
            }
            else
            {
               trans_log(INFO_SIGN, NULL, 0, NULL, NULL, "%s", line_buffer);
               trans_log(INFO_SIGN, NULL, 0, NULL, NULL,
#if SIZEOF_OFF_T == 4
                         _("Local file size of %s is %ld"),
#else
                         _("Local file size of %s is %lld"),
#endif
#ifdef HAVE_STATX
                         final_filename, (pri_off_t)stat_buf.stx_size
#else
                         final_filename, (pri_off_t)stat_buf.st_size
#endif
                        );
            }
         }

         /* If we used dot notation, don't forget to rename. */
         if ((db.lock == DOT) || (db.lock == DOT_VMS))
         {
            if (db.lock == DOT_VMS)
            {
               (void)strcat(final_filename, DOT_NOTATION);
            }
            if ((status = ftp_move(initial_filename, final_filename, 0,
                                   db.create_target_dir,
                                   db.dir_mode_str,
                                   created_path)) != SUCCESS)
            {
               WHAT_DONE("send", file_size_done, no_of_files_done);
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         _("Failed to move remote file %s to %s (%d)"),
                         initial_filename, final_filename, status);
               (void)ftp_quit();
               exit(eval_timeout(MOVE_REMOTE_ERROR));
            }
            else
            {
               if (db.verbose == YES)
               {
                  trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                            _("Renamed remote file %s to %s"),
                            initial_filename, final_filename);
               }
               if ((created_path != NULL) && (created_path[0] != '\0'))
               {
                  trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Created directory `%s'.", created_path);
                  created_path[0] = '\0';   
               }
            }
         }

#ifdef WITH_READY_FILES
         if ((db.lock == READY_A_FILE) || (db.lock == READY_B_FILE))
         {
            int  rdy_length;
            char file_type,
                 ready_file_name[MAX_FILENAME_LENGTH],
                 ready_file_buffer[MAX_PATH_LENGTH + 25];

            /* Generate the name for the ready file. */
            (void)sprintf(ready_file_name, ".%s_rdy", final_filename);

            /* Open ready file on remote site */
            if ((status = ftp_data(ready_file_name, append_offset,
                                   db.ftp_mode, DATA_WRITE, db.sndbuf_size,
                                   NO, NULL, NULL)) != SUCCESS)
            {
               WHAT_DONE("send", file_size_done, no_of_files_done);
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         _("Failed to open remote ready file %s (%d)."),
                         ready_file_name, status);
               (void)ftp_quit();
               exit(eval_timeout(OPEN_REMOTE_ERROR));
            }
            else
            {
               if (db.verbose == YES)
               {
                  trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                            _("Open remote ready file %s"), ready_file_name);
               }
            }
#ifdef WITH_SSL
            if (db.tls_auth == BOTH)
            {
               if (ftp_auth_data() == INCORRECT)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                            _("TSL/SSL data connection to server `%s' failed."),
                            db.hostname);
                  (void)ftp_quit();
                  exit(eval_timeout(AUTH_ERROR));
               }
               else
               {
                  if (db.verbose == YES)
                  {
                     trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                               _("Authentification successful."));
                  }
               }
            }
#endif

            /* Create contents for ready file. */
            if (db.lock == READY_A_FILE)
            {
               file_type = 'A';
            }
            else
            {
               file_type = 'B';
            }
            rdy_length = sprintf(ready_file_buffer,
                                 "%s %c U\n$$end_of_ready_file\n",
                                 initial_filename, file_type);

            /* Write remote ready file in one go. */
            if ((status = ftp_write(ready_file_buffer, NULL,
                                    rdy_length)) != SUCCESS)
            {
               /*
                * It could be that we have received a SIGPIPE
                * signal. If this is the case there might be data
                * from the remote site on the control connection.
                * Try to read this data into the global variable
                * 'msg_str'.
                */
               if (sigpipe_flag == ON)
               {
                  (void)ftp_get_reply();
               }
               WHAT_DONE("send", file_size_done, no_of_files_done);
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         (sigpipe_flag == ON) ? msg_str : NULL,
                         _("Failed to write to remote ready file %s (%d)."),
                         ready_file_name, status);
               if (status == EPIPE)
               {
                  trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
                            _("Hmm. Pipe is broken. Will NOT send a QUIT."));
               }
               else
               {
                  (void)ftp_quit();
               }
               exit(eval_timeout(WRITE_REMOTE_ERROR));
            }

            /* Close remote ready file. */
            if ((status = ftp_close_data()) != SUCCESS)
            {
               WHAT_DONE("send", file_size_done, no_of_files_done);
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         _("Failed to close remote ready file %s (%d)."),
                         ready_file_name, status);
               (void)ftp_quit();
               exit(eval_timeout(CLOSE_REMOTE_ERROR));
            }
            else
            {
               if (db.verbose == YES)
               {
                  trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                            _("Closed remote ready file %s"), ready_file_name);
               }
            }

            /* Remove leading dot from ready file. */
            if ((status = ftp_move(ready_file_name, &ready_file_name[1], 0,
                                   db.create_target_dir,
                                   db.dir_mode_str,
                                   created_path)) != SUCCESS)
            {
               WHAT_DONE("send", file_size_done, no_of_files_done);
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         _("Failed to move remote ready file %s to %s (%d)"),
                         ready_file_name, &ready_file_name[1], status);
               (void)ftp_quit();
               exit(eval_timeout(MOVE_REMOTE_ERROR));
            }
            else
            {
               if (db.verbose == YES)
               {
                  trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                            _("Renamed remote ready file %s to %s"),
                            ready_file_name, &ready_file_name[1]);
               }
               if ((created_path != NULL) && (created_path[0] != '\0'))
               {
                  trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Created directory `%s'.", created_path);
                  created_path[0] = '\0';   
               }
            }
         }
#endif /* WITH_READY_FILES */

         no_of_files_done++;
         trans_log(INFO_SIGN, NULL, 0, NULL, NULL,
#if SIZEOF_OFF_T == 4
                   _("Send %s [%ld bytes]"),
#else
                   _("Send %s [%lld bytes]"),
#endif
#ifdef HAVE_STATX
                   final_filename, (pri_off_t)stat_buf.stx_size
#else
                   final_filename, (pri_off_t)stat_buf.st_size
#endif
                  );

         if ((db.remove == YES) && (db.exec_mode == TRANSFER_MODE))
         {
            /* Delete the file we just have send. */
            if (unlink(db.filename[files_send]) < 0)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         _("Could not unlink() local file %s after sending it successfully : %s"),
                         strerror(errno), db.filename[files_send]);
            }
            else
            {
               if (db.verbose == YES)
               {
                  trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
                            _("Removed orginal file %s"),
                            db.filename[files_send]);
               }
            }
         }
      } /* for (files_send = 0; files_send < db.no_of_files; files_send++) */

#if SIZEOF_OFF_T == 4
      (void)sprintf(msg_str, _("%ld bytes send in %d file(s)."),
#else
      (void)sprintf(msg_str, _("%lld bytes send in %d file(s)."),
#endif
                    (pri_off_t)file_size_done, no_of_files_done);

       if (append_count == 1)
       {
          (void)strcat(msg_str, _(" [APPEND]"));
       }
       else if (append_count > 1)
            {
               char tmp_buffer[40];

               (void)sprintf(tmp_buffer, _(" [APPEND * %d]"), append_count);
               (void)strcat(msg_str, tmp_buffer);
            }
      trans_log(INFO_SIGN, NULL, 0, NULL, NULL, "%s", msg_str);
      msg_str[0] = '\0';

      if ((local_file_not_found == db.no_of_files) && (db.no_of_files > 0))
      {
         exit_status = OPEN_LOCAL_ERROR;
      }
   }

   free(buffer);

   /* Logout again. */
   if ((status = ftp_quit()) != SUCCESS)
   {
      trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                _("Failed to disconnect from remote host (%d)."), status);
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
   free(ascii_buffer);

   exit(exit_status);
}


/*++++++++++++++++++++++++++++++ aftp_exit() ++++++++++++++++++++++++++++*/
static void
aftp_exit(void)
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
