/*
 *  send_log_cmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   send_log_cmd - sends which logs we like to receive
 **
 ** SYNOPSIS
 **   int send_log_cmd(int afd_no, char *log_data_buffer, int *bytes_buffered)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.08.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>            /* fprintf()                               */
#include <string.h>
#include <stdlib.h>           /* exit()                                  */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>         /* struct timeval                          */
#ifdef HAVE_FCNTL_H
#include <fcntl.h>            /* O_RDWR, O_CREAT, O_WRONLY, etc          */
#endif
#include <unistd.h>           /* select(), read(), write()               */
#include <errno.h>
#include "mondefs.h"
#include "logdefs.h"
#include "afdddefs.h"

/* #define DEBUG_LOG_CMD */

/* External global variables. */
extern int                    sock_fd,
                              timeout_flag;
extern long                   tcp_timeout;
extern char                   *p_work_dir;
extern struct mon_status_area *msa;

/* Local functions prototypes. */
static void                   init_log_values(char *, char *, char *, off_t *);


/*########################### send_log_cmd() ############################*/
int
send_log_cmd(int afd_no, char *log_data_buffer, int *bytes_buffered)
{
   int   buf_length,
         cmd_length;
   off_t log_file_size;
   char  cmd_buffer[3 + 1 + 2 + 1 + (NO_OF_LOGS * (MAX_INT_LENGTH + 1 + MAX_LONG_LONG_LENGTH + 1 + MAX_LONG_LONG_LENGTH + 1))],
         remote_log_inode[MAX_LONG_LONG_LENGTH];

   (void)strcpy(cmd_buffer, LOG_CMD);
   cmd_length = LOG_CMD_LENGTH;
   buf_length = 3 + 1 + 2 + 1 + (NO_OF_LOGS * (MAX_INT_LENGTH + 1 + MAX_LONG_LONG_LENGTH + 1 + MAX_LONG_LONG_LENGTH + 1));
   if ((msa[afd_no].log_capabilities & AFDD_SYSTEM_LOG) &&
       (msa[afd_no].options & AFDD_SYSTEM_LOG))
   {
      init_log_values(SYSTEM_LOG_NAME, msa[afd_no].afd_alias, remote_log_inode,
                      &log_file_size);
#if SIZEOF_OFF_T == 4
      cmd_length += snprintf(&cmd_buffer[cmd_length], buf_length - cmd_length, " LS 0 %s %ld",
#else
      cmd_length += snprintf(&cmd_buffer[cmd_length], buf_length - cmd_length, " LS 0 %s %lld",
#endif
                                     remote_log_inode,
                                     (pri_off_t)log_file_size);
   }
   if ((msa[afd_no].log_capabilities & AFDD_EVENT_LOG) &&
       (msa[afd_no].options & AFDD_EVENT_LOG))
   {
      init_log_values(EVENT_LOG_NAME, msa[afd_no].afd_alias, remote_log_inode,
                      &log_file_size);
#if SIZEOF_OFF_T == 4
      cmd_length += snprintf(&cmd_buffer[cmd_length], buf_length - cmd_length, " LE 0 %s %ld",
#else
      cmd_length += snprintf(&cmd_buffer[cmd_length], buf_length - cmd_length, " LE 0 %s %lld",
#endif
                                     remote_log_inode,
                                     (pri_off_t)log_file_size);
   }
   if ((msa[afd_no].log_capabilities & AFDD_RECEIVE_LOG) &&
       (msa[afd_no].options & AFDD_RECEIVE_LOG))
   {
      init_log_values(RECEIVE_LOG_NAME, msa[afd_no].afd_alias, remote_log_inode,
                      &log_file_size);
#if SIZEOF_OFF_T == 4
      cmd_length += snprintf(&cmd_buffer[cmd_length], buf_length - cmd_length, " LR 0 %s %ld",
#else
      cmd_length += snprintf(&cmd_buffer[cmd_length], buf_length - cmd_length, " LR 0 %s %lld",
#endif
                                     remote_log_inode,
                                     (pri_off_t)log_file_size);
   }
   if ((msa[afd_no].log_capabilities & AFDD_TRANSFER_LOG) &&
       (msa[afd_no].options & AFDD_TRANSFER_LOG))
   {
      init_log_values(TRANSFER_LOG_NAME, msa[afd_no].afd_alias,
                      remote_log_inode, &log_file_size);
#if SIZEOF_OFF_T == 4
      cmd_length += snprintf(&cmd_buffer[cmd_length], buf_length - cmd_length, " LT 0 %s %ld",
#else
      cmd_length += snprintf(&cmd_buffer[cmd_length], buf_length - cmd_length, " LT 0 %s %lld",
#endif
                                     remote_log_inode,
                                     (pri_off_t)log_file_size);
   }
   if ((msa[afd_no].log_capabilities & AFDD_TRANSFER_DEBUG_LOG) &&
       (msa[afd_no].options & AFDD_TRANSFER_DEBUG_LOG))
   {
      init_log_values(TRANS_DB_LOG_NAME, msa[afd_no].afd_alias,
                      remote_log_inode, &log_file_size);
#if SIZEOF_OFF_T == 4
      cmd_length += snprintf(&cmd_buffer[cmd_length], buf_length - cmd_length, " LB 0 %s %ld",
#else
      cmd_length += snprintf(&cmd_buffer[cmd_length], buf_length - cmd_length, " LB 0 %s %lld",
#endif
                                     remote_log_inode,
                                     (pri_off_t)log_file_size);
   }
#ifdef _INPUT_LOG
   if ((msa[afd_no].log_capabilities & AFDD_INPUT_LOG) &&
       (msa[afd_no].options & AFDD_INPUT_LOG))
   {
      init_log_values(INPUT_BUFFER_FILE, msa[afd_no].afd_alias,
                      remote_log_inode, &log_file_size);
# if SIZEOF_OFF_T == 4
      cmd_length += snprintf(&cmd_buffer[cmd_length], buf_length - cmd_length, " LI 0 %s %ld",
# else
      cmd_length += snprintf(&cmd_buffer[cmd_length], buf_length - cmd_length, " LI 0 %s %lld",
# endif
                                     remote_log_inode,
                                     (pri_off_t)log_file_size);
   }
#endif
#ifdef _DISTRIBUTION_LOG
   if ((msa[afd_no].log_capabilities & AFDD_DISTRIBUTION_LOG) &&
       (msa[afd_no].options & AFDD_DISTRIBUTION_LOG))
   {
      init_log_values(DISTRIBUTION_BUFFER_FILE, msa[afd_no].afd_alias,
                      remote_log_inode, &log_file_size);
# if SIZEOF_OFF_T == 4
      cmd_length += snprintf(&cmd_buffer[cmd_length], buf_length - cmd_length, " LU 0 %s %ld",
# else
      cmd_length += snprintf(&cmd_buffer[cmd_length], buf_length - cmd_length, " LU 0 %s %lld",
# endif
                                     remote_log_inode,
                                     (pri_off_t)log_file_size);
   }
#endif
#ifdef _PRODUCTION_LOG
   if ((msa[afd_no].log_capabilities & AFDD_PRODUCTION_LOG) &&
       (msa[afd_no].options & AFDD_PRODUCTION_LOG))
   {
      init_log_values(PRODUCTION_BUFFER_FILE, msa[afd_no].afd_alias,
                      remote_log_inode, &log_file_size);
# if SIZEOF_OFF_T == 4
      cmd_length += snprintf(&cmd_buffer[cmd_length], buf_length - cmd_length, " LP 0 %s %ld",
# else
      cmd_length += snprintf(&cmd_buffer[cmd_length], buf_length - cmd_length, " LP 0 %s %lld",
# endif
                                     remote_log_inode,
                                     (pri_off_t)log_file_size);
   }
#endif
#ifdef _OUTPUT_LOG
   if ((msa[afd_no].log_capabilities & AFDD_OUTPUT_LOG) &&
       (msa[afd_no].options & AFDD_OUTPUT_LOG))
   {
      init_log_values(OUTPUT_BUFFER_FILE, msa[afd_no].afd_alias,
                      remote_log_inode, &log_file_size);
# if SIZEOF_OFF_T == 4
      cmd_length += snprintf(&cmd_buffer[cmd_length], buf_length - cmd_length, " LO 0 %s %ld",
# else
      cmd_length += snprintf(&cmd_buffer[cmd_length], buf_length - cmd_length, " LO 0 %s %lld",
# endif
                                     remote_log_inode,
                                     (pri_off_t)log_file_size);
   }
#endif
#ifdef _DELETE_LOG
   if ((msa[afd_no].log_capabilities & AFDD_DELETE_LOG) &&
       (msa[afd_no].options & AFDD_DELETE_LOG))
   {
      init_log_values(DELETE_BUFFER_FILE, msa[afd_no].afd_alias,
                      remote_log_inode, &log_file_size);
# if SIZEOF_OFF_T == 4
      cmd_length += snprintf(&cmd_buffer[cmd_length], buf_length - cmd_length, " LD 0 %s %ld",
# else
      cmd_length += snprintf(&cmd_buffer[cmd_length], buf_length - cmd_length, " LD 0 %s %lld",
# endif
                                     remote_log_inode,
                                     (pri_off_t)log_file_size);
   }
#endif
   if ((msa[afd_no].log_capabilities & AFDD_JOB_DATA) &&
       (msa[afd_no].options & AFDD_JOB_DATA))
   {
      init_log_values(JOB_ID_DATA_FILE, msa[afd_no].afd_alias,
                      remote_log_inode, NULL);
      cmd_length += snprintf(&cmd_buffer[cmd_length], buf_length - cmd_length, " JD 0 %s",
                                     remote_log_inode);
   }
   if (cmd_length > LOG_CMD_LENGTH)
   {
      int            status;
      fd_set         rwset;
      struct timeval timeout;

      FD_ZERO(&rwset);
      FD_SET(sock_fd, &rwset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = tcp_timeout;

      status = select(sock_fd + 1, NULL, &rwset, NULL, &timeout);

      if ((status > 0) && (FD_ISSET(sock_fd, &rwset)))
      {
         if ((cmd_length + 2) < buf_length)
         {
            cmd_buffer[cmd_length] = '\r';
            cmd_buffer[cmd_length + 1] = '\n';
            cmd_length += 2;
         }
         else
         {
            cmd_buffer[buf_length - 2] = '\r';
            cmd_buffer[buf_length - 1] = '\n';
            cmd_length = buf_length;
         }
         if (write(sock_fd, cmd_buffer, cmd_length) != cmd_length)
         {
            mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                    "write() error : %s", strerror(errno));
            return(INCORRECT);
         }

         /* Lets catch the 211- reply if command was succesfull. */
         FD_ZERO(&rwset);
         FD_SET(sock_fd, &rwset);
         timeout.tv_usec = 0L;
         timeout.tv_sec = tcp_timeout;

         status = select(sock_fd + 1, &rwset, NULL, NULL, &timeout);

         if ((status > 0) && (FD_ISSET(sock_fd, &rwset)))
         {
            if ((status = read(sock_fd, log_data_buffer,
                               MAX_LOG_DATA_BUFFER)) > 0)
            {
               if ((log_data_buffer[0] == '2') && (log_data_buffer[1] == '1') &&
                   (log_data_buffer[2] == '1') && (log_data_buffer[3] == '-'))
               {
                  int i;

                  i = 4;
                  while ((log_data_buffer[i] != '\r') && (i < status))
                  {
                     i++;
                  }
                  if ((log_data_buffer[i] == '\r') && ((i + 1) < status) &&
                      (log_data_buffer[i + 1] == '\n'))
                  {
                     i += 2;
                     *bytes_buffered = status - i;
                     if (*bytes_buffered > 0)
                     {
                        (void)memmove(log_data_buffer, &log_data_buffer[i],
                                      *bytes_buffered);
                     }
#ifdef DEBUG_LOG_CMD
                     cmd_buffer[cmd_length - 2] = '\0';
                     mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, NULL,
                             "W-> %s", cmd_buffer);
#endif
                     return(SUCCESS);
                  }
               }
            }
            else if (status == 0)
                 {
                    mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, NULL,
                            "Remote hangup!");
                 }
                 else
                 {
                    mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                            "Failed reading reply from %s command : %s",
                            LOG_CMD, strerror(errno));
                 }
         }
         else if (status == 0)
              {
                 timeout_flag = ON;
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "select() error : %s", strerror(errno));
                 exit(INCORRECT);
              }
      }
      else if (status == 0)
           {
              timeout_flag = ON;
           }
           else
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "select() error : %s", strerror(errno));
              exit(INCORRECT);
           }
   }

   return(INCORRECT);
}


/*++++++++++++++++++++++++++ init_log_values() ++++++++++++++++++++++++++*/
static void
init_log_values(char  *log_name,
                char  *afd_alias,
                char  *remote_log_inode,
                off_t *log_file_size)
{
   int  fd;
   char current_log_no_str[MAX_INT_LENGTH],
        fullname[MAX_PATH_LENGTH],
        *p_end;

   p_end = fullname + snprintf(fullname, MAX_PATH_LENGTH, "%s%s/%s/%s",
                               p_work_dir, RLOG_DIR, afd_alias, log_name);
   (void)strcpy(p_end, REMOTE_INODE_EXTENSION);
   if ((fd = open(fullname, O_RDONLY)) == -1)
   {
      if (errno != ENOENT)
      {
         mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                 "Failed to open() `%s' : %s", fullname, strerror(errno));
      }
      remote_log_inode[0] = '0';
      remote_log_inode[1] = '\0';
      current_log_no_str[0] = '0';
      current_log_no_str[1] = '\0';
   }
   else
   {
      char buffer[MAX_INODE_LOG_NO_LENGTH];

      if (read(fd, buffer, MAX_INODE_LOG_NO_LENGTH) < 0)
      {
         mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                 "Failed to read() from `%s' : %s", fullname, strerror(errno));
         remote_log_inode[0] = '0';
         remote_log_inode[1] = '\0';
         current_log_no_str[0] = '0';
         current_log_no_str[1] = '\0';
      }
      else
      {
         int i = 0;

         while ((i < MAX_LONG_LONG_LENGTH) && (buffer[i] != ' '))
         {
            remote_log_inode[i] = buffer[i];
            i++;
         }
         if ((i > 0) && (i < MAX_LONG_LONG_LENGTH))
         {
            int j;

            remote_log_inode[i] = '\0';
            j = 0;
            i++;
            while ((i < (MAX_INODE_LOG_NO_LENGTH - 1)) && (buffer[i] != '\n') &&
                   (j < (MAX_INT_LENGTH - 1)))
            {
               current_log_no_str[j] = buffer[i];
               i++; j++;
            }
            if (buffer[i] == '\n')
            {
               current_log_no_str[j] = '\0';
            }
            else
            {
               mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                       "Failed to find the log number!");
               current_log_no_str[0] = '0';
               current_log_no_str[1] = '\0';
            }
         }
         else
         {
            mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                    "Failed to locate the log number! (%d)", i);
            remote_log_inode[0] = '0';
            remote_log_inode[1] = '\0';
            current_log_no_str[0] = '0';
            current_log_no_str[1] = '\0';
         }
      }
      if (close(fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Failed to close() `%s' : %s", fullname, strerror(errno));
      }
   }
   if (log_file_size != NULL)
   {
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat stat_buf;
#endif

      (void)strcpy(p_end, current_log_no_str);
#ifdef HAVE_STATX
      if (statx(0, fullname, AT_STATX_SYNC_AS_STAT,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (stat(fullname, &stat_buf) == -1)
#endif
      {
         if (errno != ENOENT)
         {
            mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                    "Failed to stat() `%s' : %s", fullname, strerror(errno));
         }
         *log_file_size = 0;
      }
      else
      {
#ifdef HAVE_STATX
         *log_file_size = stat_buf.stx_size;
#else
         *log_file_size = stat_buf.st_size;
#endif
      }
   }

   return;
}
