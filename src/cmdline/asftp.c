/*
 *  asftp.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2015 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#include "asftpdefs.h"

DESCR__S_M1
/*
 ** NAME
 **   asftp - send files via SFTP automaticaly
 **
 ** SYNOPSIS
 **   [r]aSftp [options] [file 1 ... file n]
 **
 **   options
 **     --version                       - Show current version
 **     -A                              - Retrieve rest of file.
 **     -a                              - Append file.
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
 **     -l <DOT | DOT_VMS | OFF | xyz.> - How to lock the file on the remote
 **                                       site.
 **     -o <mode>                       - Changes the permission of each
 **                                       file distributed.
 **     -p <port number>                - Remote port number of FTP-server.
 **     -u <user> <password>            - Remote user name and password.
 **     -r                              - Remove transmitted file.
 **     -s <file size>                  - File size of file to be transfered.
 **     -t <timout>                     - FTP timeout in seconds.
 **     -v                              - Verbose. Shows all FTP commands
 **                                       and the reply from the remote server.
 **     -?                              - Usage.
 **
 ** DESCRIPTION
 **   asftp sends the given files to the defined recipient via SFTP.
 **   It does so by using it's own SFTP-client, but it does use the
 **   SSH-client to do the encrytion.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   04.05.2015 H.Kiehl Created
 **   30.08.2022 H.Kiehl Fix error where files are not fully uploaded.
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
#include "sftpdefs.h"
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
static void          asftp_exit(void),
                     sig_bus(int),
                     sig_segv(int),
                     sig_exit(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int         buffer_offset,
               exit_status = SUCCESS,
               fd = -1,
               status,
               no_of_files_done = 0;
   off_t       file_size_done = 0,
               file_size_to_retrieve,
               local_file_size,
               no_of_bytes;
   char        *ascii_buffer = NULL,
               append_count = 0,
               *buffer,
               *created_path = NULL,
               *file_ptr,
               initial_filename[MAX_FILENAME_LENGTH],
               final_filename[MAX_FILENAME_LENGTH];
   struct stat stat_buf;

#ifdef HAVE_GETTEXT
   (void)setlocale(LC_ALL, "");
   bindtextdomain(PACKAGE, LOCALEDIR);
   textdomain(PACKAGE);
#endif
   CHECK_FOR_VERSION(argc, argv);

   /* Do some cleanups when we exit. */
   if (atexit(asftp_exit) != 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                _("Could not register exit function : %s (%s %d)\n"),
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      (void)rec(sys_log_fd, FATAL_SIGN, _("signal() error : %s (%s %d)\n"),
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Initialise variables. */
   init_asftp(argc, argv, &db);
   msg_str[0] = '\0';

   /* Set SFTP timeout value. */
   transfer_timeout = db.transfer_timeout;

   sigpipe_flag = timeout_flag = OFF;

   /* Connect to remote SFTP-server. */
   if ((status = sftp_connect(db.hostname, db.port, db.ssh_protocol,
                              0, db.user,
#ifdef WITH_SSH_FINGERPRINT
                              db.ssh_fingerprint,
#endif
                              db.password, db.verbose)) != SUCCESS)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                _("SFTP as user %s connection to %s at port %d failed (%d)."),
                db.user, db.hostname, db.port, status);
      exit(eval_timeout(CONNECT_ERROR));
   }
   else
   {
      if (db.verbose == YES)
      {
         trans_log(INFO_SIGN, NULL, 0, NULL, msg_str,
                   _("Connected. Agreed on SFTP version %u."), sftp_version());
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

   /* Change directory if necessary. */
   if (db.remote_dir[0] != '\0')
   {
      if ((status = sftp_cd(db.remote_dir, db.create_target_dir,
                            db.dir_mode, created_path)) != SUCCESS)
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
         sftp_quit();
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
      if (get_remote_file_names_sftp_cmd(&file_size_to_retrieve) > 0)
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
               if (stat(rl[i].file_name, &stat_buf) == -1)
               {
                  if (stat(local_file, &stat_buf) == -1)
                  {
                     offset = 0;
                  }
                  else
                  {
                     offset = stat_buf.st_size;
                  }
               }
               else
               {
                  offset = stat_buf.st_size;
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
               if (stat(local_file, &stat_buf) == -1)
               {
                  offset = 0;
               }
               else
               {
                  offset = stat_buf.st_size;
               }
            }
            if ((status = sftp_open_file(SFTP_READ_FILE, rl[i].file_name,
                                         offset, NULL, db.blocksize,
                                         &buffer_offset)) != SUCCESS)
            {
               if (status == SSH_FX_NO_SUCH_FILE)
               {
                  trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, msg_str,
                            _("Failed to open remote file %s (%d)."),
                            rl[i].file_name, status);
               }
               else
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                            _("Failed to open remote file %s (%d)."),
                            rl[i].file_name, status);
                  sftp_quit();
                  exit(eval_timeout(OPEN_REMOTE_ERROR));
               }
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
                  sftp_quit();
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
                  if ((status = sftp_read(buffer, db.blocksize)) == INCORRECT)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               _("Failed to read from remote file %s (%d)"),
                               rl[i].file_name, status);
                     sftp_quit();
                     exit(eval_timeout(READ_REMOTE_ERROR));
                  }
                  else if (status > 0)
                       {
                          if (write(fd, buffer, status) != status)
                          {
                             trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                       _("Failed to write() to file %s : %s"),
                                       local_file, strerror(errno));
                             sftp_quit();
                             exit(WRITE_LOCAL_ERROR);
                          }
                          bytes_done += status;
                       }
               } while (status != 0);

               /* Close remote file. */
               if ((status = sftp_close_file()) != SUCCESS)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                            _("Failed to close data connection (%d)."), status);
                  sftp_quit();
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
                  if ((status = sftp_dele(rl[i].file_name)) != SUCCESS)
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
             local_file_not_found = 0;
      size_t length;
      off_t  append_offset = 0;

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

            if (fstat(fd, &stat_buf) == -1)
            {
               if (db.verbose == YES)
               {
                  trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
                            _("Failed to fstat() local file %s"),
                            db.filename[files_send]);
               }
               WHAT_DONE("send", file_size_done, no_of_files_done);
               sftp_quit();
               exit(STAT_ERROR);
            }
            else
            {
               if (!S_ISREG(stat_buf.st_mode))
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
            local_file_size = stat_buf.st_size;
            if (db.verbose == YES)
            {
               trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, NULL,
                         _("Open local file %s"), db.filename[files_send]);
            }

            /*
             * Check if the file has not already been partly
             * transmitted. If so, lets first get the size of the
             * remote file, to append it.
             */
            append_offset = 0;
            if (db.append == YES)
            {
               struct stat stat_buf;

               if ((status = sftp_stat(initial_filename, &stat_buf)) != SUCCESS)
               {
                  trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, msg_str,
                            _("Failed to stat() file %s (%d)."),
                            initial_filename, status);
                  if (timeout_flag == ON)
                  {
                     timeout_flag = OFF;
                  }
               }
               else
               {
                  append_offset = stat_buf.st_size;
                  if (db.verbose == YES)
                  {
                     trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
#if SIZEOF_OFF_T == 4
                               _("Remote size of %s is %ld."),
#else
                               _("Remote size of %s is %lld."),
#endif
                               initial_filename, (pri_off_t)stat_buf.st_size);
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
         if ((status = sftp_open_file(SFTP_WRITE_FILE, initial_filename,
                                      append_offset,
                                      (db.chmod_str[0] == '\0') ? NULL : &db.chmod,
                                      db.blocksize, &buffer_offset)) != SUCCESS)
         {
            WHAT_DONE("send", file_size_done, no_of_files_done);
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                      _("Failed to open remote file %s (%d)."),
                      initial_filename, status);
            sftp_quit();
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

         /* Read (local) and write (remote) file. */
         no_of_bytes = 0;
         if (ascii_buffer != NULL)
         {
            ascii_buffer[0] = 0;
         }

         if (db.exec_mode == TRANSFER_MODE)
         {
            int bytes_buffered;

            do
            {
               if ((bytes_buffered = read(fd, buffer, db.blocksize - buffer_offset)) < 0)
               {
                  if (db.verbose == YES)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               _("Could not read local file `%s' [%d] : %s"),
                               final_filename, bytes_buffered,
                               strerror(errno));
                  }
                  WHAT_DONE("send", file_size_done, no_of_files_done);
                  sftp_quit();
                  exit(READ_LOCAL_ERROR);
               }

               if (bytes_buffered > 0)
               {
                  if ((status = sftp_write(buffer,
                                           bytes_buffered)) != SUCCESS)
                  {
                     WHAT_DONE("send", file_size_done, no_of_files_done);
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
#if SIZEOF_OFF_T == 4
                               _("Failed to write %d bytes to remote file %s after writing %ld bytes."),
#else
                               _("Failed to write %d bytes to remote file %s after writing %lld bytes."),
#endif
                               bytes_buffered, initial_filename,
                               (pri_off_t)file_size_done);
                     sftp_quit();
                     exit(eval_timeout(WRITE_REMOTE_ERROR));
                  }

                  file_size_done += db.blocksize;
                  no_of_bytes += db.blocksize;
               } /* if (bytes_buffered > 0) */
            } while (bytes_buffered == (db.blocksize - buffer_offset));

            /*
             * Since there are always some users sending files to the
             * AFD not in dot notation, lets check here if this is really
             * the EOF.
             */
            if (stat(db.filename[files_send], &stat_buf) == 0)
            {
               if (stat_buf.st_size != local_file_size)
               {
                  /*
                   * Give a warning, so some action can be taken against
                   * the originator.
                   */
                  (void)rec(sys_log_fd, WARN_SIGN,
#if SIZEOF_OFF_T == 4
                            _("Someone is still writting to file %s. Size changed from %ld to %ld. (%s %d)\n"),
#else
                            _("Someone is still writting to file %s. Size changed from %ld to %ld. (%s %d)\n"),
#endif
                            db.filename[files_send],
                            (pri_off_t)local_file_size,
                            (pri_off_t)stat_buf.st_size,
                            __FILE__, __LINE__);
               }
            }

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
            int j,
                loops,
                rest;

            loops = (local_file_size - append_offset) / db.blocksize;
            rest = (local_file_size - append_offset) % db.blocksize;
            for (j = 0; j < loops; j++)
            {
               if ((status = sftp_write(buffer,
                                        db.blocksize - buffer_offset)) != SUCCESS)
               {
                  WHAT_DONE("send", file_size_done, no_of_files_done);
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
#if SIZEOF_OFF_T == 4
                            _("Failed to write to remote file %s after writing %ld bytes."),
#else
                            _("Failed to write to remote file %s after writing %lld bytes."),
#endif
                            initial_filename, (pri_off_t)file_size_done);
                  sftp_quit();
                  exit(eval_timeout(WRITE_REMOTE_ERROR));
               }

               file_size_done += db.blocksize;
               no_of_bytes += db.blocksize;
            }
            if (rest > 0)
            {
               if ((status = sftp_write(buffer, rest)) != SUCCESS)
               {
                  WHAT_DONE("send", file_size_done, no_of_files_done);
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            _("Failed to write rest to remote file %s"),
                            initial_filename);
                  sftp_quit();
                  exit(eval_timeout(WRITE_REMOTE_ERROR));
               }

               file_size_done += rest;
               no_of_bytes += rest;
            }
         }

         if (sftp_flush() != SUCCESS)
         {
            WHAT_DONE("send", file_size_done, no_of_files_done);
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                      _("Failed to flush remaining to remote file %s"),
                      initial_filename);
            sftp_quit();
            exit(eval_timeout(WRITE_REMOTE_ERROR));
         }

         /* Close remote file. */
         if ((status = sftp_close_file()) != SUCCESS)
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
               sftp_quit();
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
            if ((status = sftp_chmod(initial_filename,
                                     db.chmod)) != SUCCESS)
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
            struct stat stat_buf;

            if ((status = sftp_stat(initial_filename,
                                    &stat_buf)) != SUCCESS)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         _("Failed to stat() remote file %s (%d)."),
                         initial_filename, status);
               if (timeout_flag == ON)
               {
                  timeout_flag = OFF;
               }
            }
            else
            {
               trans_log(INFO_SIGN, NULL, 0, NULL, NULL,
#if SIZEOF_OFF_T == 4
                         _("Local file size of %s is %ld"),
#else
                         _("Local file size of %s is %lld"),
#endif
                         final_filename, (pri_off_t)stat_buf.st_size);
            }
         }

         /* If we used dot notation, don't forget to rename. */
         if ((db.lock == DOT) || (db.lock == DOT_VMS))
         {
            if (db.lock == DOT_VMS)
            {
               (void)strcat(final_filename, DOT_NOTATION);
            }
            if ((status = sftp_move(initial_filename, final_filename,
                                    db.create_target_dir,
                                    db.dir_mode, created_path)) != SUCCESS)
            {
               WHAT_DONE("send", file_size_done, no_of_files_done);
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         _("Failed to move remote file %s to %s (%d)"),
                         initial_filename, final_filename, status);
               sftp_quit();
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

         no_of_files_done++;
         trans_log(INFO_SIGN, NULL, 0, NULL, NULL,
#if SIZEOF_OFF_T == 4
                   _("Send %s [%ld bytes]"),
#else
                   _("Send %s [%lld bytes]"),
#endif
                   final_filename, (pri_off_t)stat_buf.st_size);

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
   sftp_quit();
   if (db.verbose == YES)
   {
      trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str, _("Logged out."));
   }

   exit(exit_status);
}


/*+++++++++++++++++++++++++++++ asftp_exit() ++++++++++++++++++++++++++++*/
static void
asftp_exit(void)
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
