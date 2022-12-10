/*
 *  awmo.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2010 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#include "awmodefs.h"

DESCR__S_M1
/*
 ** NAME
 **   awmo - send files via WMO socket procedure automaticaly
 **
 ** SYNOPSIS
 **   [t]awmo [options] [file 1 ... file n]
 **
 **   options
 **     --version                       - Show current version
 **     -a                              - Wait for acknowledge from server.
 **     -b <block size>                 - WMO block size in bytes.
 **     -f <filename file>              - List of filenames to send.
 **     -h <hostname|IP number>         - Recipient hostname or IP number.
 **     -H                              - File name is header.
 **     -m <A | I | F>                  - WMO transfer type, ASCII, binary or
 **                                       Fax. Default is binary.
 **     -p <port number>                - Remote port number of WMO-server.
 **     -r                              - Remove transmitted file.
 **     -s <file size>                  - File size of file to be transfered.
 **     -t <timout>                     - WMO timeout in seconds.
 **     -v                              - Verbose. Shows all WMO commands
 **                                       and the reply from the remote server.
 **     -?                              - Usage.
 **
 ** DESCRIPTION
 **   awmo sends the given files to the defined recipient via WMO socket
 **   procedure.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   13.11.2010 H.Kiehl Created
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
#include "wmodefs.h"
#include "version.h"

/* Global variables. */
unsigned int         special_flag = 0;
int                  *no_of_listed_files,
                     simulation_mode = NO,
                     sys_log_fd = STDERR_FILENO,
                     transfer_log_fd = STDERR_FILENO,
                     timeout_flag,
                     sigpipe_flag;
long                 transfer_timeout;
char                 host_deleted = NO,
                     msg_str[MAX_RET_MSG_LENGTH],
                     *p_work_dir = NULL;
struct data          db;
struct filename_list *rl;
const char           *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void          awmo_exit(void),
                     sig_bus(int),
                     sig_segv(int),
                     sig_pipe(int),
                     sig_exit(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int          exit_status = SUCCESS,
                status,
                *wmo_counter,
                wmo_counter_fd = -1;
   off_t        local_file_size,
                no_of_bytes;
   char         *ascii_buffer = NULL,
                *buffer,
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
   if (atexit(awmo_exit) != 0)
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
   init_awmo(argc, argv, &db);
   msg_str[0] = '\0';

   /* Set WMO timeout value. */
   transfer_timeout = db.transfer_timeout;

   sigpipe_flag = timeout_flag = OFF;

   if (db.special_flag & WITH_SEQUENCE_NUMBER)
   {
      char counter_file_name[1 + MAX_FILENAME_LENGTH + MAX_INT_LENGTH + 1];

      (void)sprintf(counter_file_name, "/%s.%d", db.hostname, db.port);
      if ((wmo_counter_fd = open_counter_file(counter_file_name, &wmo_counter)) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to open counter file `%s'.", counter_file_name);
      }
   }

   /* Connect to remote WMO-server. */
   if ((status = wmo_connect(db.hostname, db.port, db.sndbuf_size)) != SUCCESS)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                _("WMO connection to <%s> at port %d failed (%d)."),
                db.hostname, db.port, status);
      exit(eval_timeout(CONNECT_ERROR));
   }
   else
   {
      if (db.verbose == YES)
      {
         trans_log(INFO_SIGN, NULL, 0, NULL, msg_str,
                    _("Connected to %s at port %d."), db.hostname, db.port);
      }
   }

   /* Allocate buffer to read data from the source file. */
   if ((buffer = malloc(db.blocksize + 1 + 4 /* For bulletin end. */)) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, _("malloc() error : %s (%s %d)\n"),
                strerror(errno), __FILE__, __LINE__);
      exit(ALLOC_ERROR);
   }

   {
      int    end_length,
             fd = -1,
             files_send = 0,
             header_length = 0,
             j,
             length_type_indicator,
             local_file_not_found = 0,
             loops,
             no_of_files_done = 0,
             rest;
      size_t length;
      off_t  file_size_done = 0;

      /* Send all files. */
      for (files_send = 0; files_send < db.no_of_files; files_send++)
      {
         length_type_indicator = 10;
         if (db.exec_mode == TEST_MODE)
         {
            (void)sprintf(final_filename, "%s%010d",
                          db.filename[0], files_send);
            local_file_size = db.dummy_size;
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

            /*
             * When the contents does not contain a bulletin header
             * it must be stored in the file name.
             */
            if (db.special_flag & FILE_NAME_IS_HEADER)
            {
               int  space_count;
               char *ptr = file_ptr;

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
               else
               {
                  header_length = 0;
               }

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
               wmo_quit();
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
                         db.filename[files_send], local_file_size);
            }
         }

         /* Read (local) and write (remote) data. */
         no_of_bytes = 0;
         loops = (length_type_indicator + header_length + local_file_size) / db.blocksize;
         rest = (length_type_indicator + header_length + local_file_size) % db.blocksize;
         if (ascii_buffer != NULL)
         {
            ascii_buffer[0] = 0;
         }

         if (db.exec_mode == TRANSFER_MODE)
         {
            if ((db.special_flag & FILE_NAME_IS_HEADER) && (rest == 0))
            {
               loops--;
               rest = db.blocksize;
            }

            /* Write length and type indicator. */
            (void)sprintf(buffer, "%08lu",
                          (unsigned long)(local_file_size + header_length + end_length));
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

            for (;;)
            {
               for (j = 0; j < loops; j++)
               {
                  if (read(fd,
                           (buffer + length_type_indicator + header_length),
                           (db.blocksize - length_type_indicator - header_length)) != (db.blocksize - length_type_indicator - header_length))
                  {
                     if (db.verbose == YES)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  _("Could not read local file %s : %s"),
                                  final_filename, strerror(errno));
                     }
                     WHAT_DONE("send", file_size_done, no_of_files_done);
                     wmo_quit();
                     exit(READ_LOCAL_ERROR);
                  }

                  if ((status = wmo_write(buffer, db.blocksize)) != SUCCESS)
                  {
                     WHAT_DONE("send", file_size_done, no_of_files_done);
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
#if SIZEOF_OFF_T == 4
                               _("Failed to write to remote file %s after writing %ld bytes."),
#else
                               _("Failed to write to remote file %s after writing %lld bytes."),
#endif
                               initial_filename, (pri_off_t)file_size_done);
                     wmo_quit();
                     exit(eval_timeout(WRITE_REMOTE_ERROR));
                  }

                  file_size_done += db.blocksize;
                  no_of_bytes += db.blocksize;

                  if (length_type_indicator > 0)
                  {
                     length_type_indicator = 0;
                     header_length = 0;
                  }
               } /* for (j = 0; j < loops; j++) */

               if (rest > 0)
               {
                  if (read(fd,
                           (buffer + length_type_indicator + header_length),
                           (rest - length_type_indicator - header_length)) != (rest - length_type_indicator - header_length))
                  {
                     if (db.verbose == YES)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  _("Could not read local file %s : %s"),
                                  final_filename, strerror(errno));
                     }
                     WHAT_DONE("send", file_size_done, no_of_files_done);
                     wmo_quit();
                     exit(READ_LOCAL_ERROR);
                  }
                  if (end_length == 4)
                  {     
                     buffer[rest] = '\015';
                     buffer[rest + 1] = '\015'; 
                     buffer[rest + 2] = '\012'; 
                     buffer[rest + 3] = 3;  /* ETX */
                  }
                  if ((status = wmo_write(buffer, rest + end_length)) != SUCCESS)
                  {
                     WHAT_DONE("send", file_size_done, no_of_files_done);
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               _("Failed to write rest to remote file %s"),
                               initial_filename);
                     wmo_quit();
                     exit(eval_timeout(WRITE_REMOTE_ERROR));
                  }

                  file_size_done += rest + end_length;
                  no_of_bytes += rest + end_length;
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
            for (j = 0; j < loops; j++)
            {
               if ((status = wmo_write(buffer, db.blocksize)) != SUCCESS)
               {
                  WHAT_DONE("send", file_size_done, no_of_files_done);
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
#if SIZEOF_OFF_T == 4
                            _("Failed to write to remote file %s after writing %ld bytes."),
#else
                            _("Failed to write to remote file %s after writing %lld bytes."),
#endif
                            initial_filename, (pri_off_t)file_size_done);
                  wmo_quit();
                  exit(eval_timeout(WRITE_REMOTE_ERROR));
               }

               file_size_done += db.blocksize;
               no_of_bytes += db.blocksize;
            }
            if (rest > 0)
            {
               if ((status = wmo_write(buffer, rest)) != SUCCESS)
               {
                  WHAT_DONE("send", file_size_done, no_of_files_done);
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            _("Failed to write rest to remote file %s"),
                            initial_filename);
                  wmo_quit();
                  exit(eval_timeout(WRITE_REMOTE_ERROR));
               }

               file_size_done += rest;
               no_of_bytes += rest;
            }
         }

         if (db.special_flag & WMO_CHECK_ACKNOWLEDGE)
         {
            int ret;

            if ((ret = wmo_check_reply()) == INCORRECT)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Failed to receive reply from port %d.", db.port);
               wmo_quit();
               exit(eval_timeout(CHECK_REPLY_ERROR));
            }
            else if (ret == NEGATIV_ACKNOWLEDGE)
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                              "Received negative acknowledge from remote port %d for file %s.",
                              db.port, final_filename);
                 }
         }

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

      trans_log(INFO_SIGN, NULL, 0, NULL, NULL,
#if SIZEOF_OFF_T == 4
                _("%ld bytes send in %d file(s)."),
#else
                _("%lld bytes send in %d file(s)."),
#endif
                (pri_off_t)file_size_done, no_of_files_done);

      if ((local_file_not_found == db.no_of_files) && (db.no_of_files > 0))
      {
         exit_status = OPEN_LOCAL_ERROR;
      }
   }

   free(buffer);

   /* Logout again. */
   wmo_quit();
   if (db.verbose == YES)
   {
      trans_log(INFO_SIGN, __FILE__, __LINE__, NULL, msg_str,
                _("Logged out."));
   }

   /* Don't need the ASCII buffer. */
   free(ascii_buffer);

   exit(exit_status);
}


/*++++++++++++++++++++++++++++++ awmo_exit() ++++++++++++++++++++++++++++*/
static void
awmo_exit(void)
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
