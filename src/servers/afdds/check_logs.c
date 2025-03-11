/*
 *  check_logs.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2006 - 2025 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_logs - Checks for changes in any of the specified logs
 **
 ** SYNOPSIS
 **   long check_logs(time_t now)
 **
 ** DESCRIPTION
 **   This function prints all the specified log data in the following
 **   format:
 **     L? <options> <packet no.> <packet length>
 **      S - System
 **      E - Event
 **      R - Retrieve
 **      T - Transfer
 **      B - Transfer Debug
 **      I - Input
 **      P - Production
 **      O - Output
 **      D - Delete
 **     JD - Job data
 **
 ** RETURN VALUES
 **   Returns the interval in seconds that this function should be called.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.11.2006 H.Kiehl Created
 **   13.04.2007 H.Kiehl If we do not send log data for a given time
 **                      send an empty log message.
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>
#endif
#include <unistd.h>
#include <errno.h>
#include "logdefs.h"
#include "afddsdefs.h"

/* #define DEBUG_LOG_CMD */

/* External global variables. */
extern int            cmd_sd,
                      log_defs;
extern char           *line_buffer,
                      log_dir[],
                      *log_buffer,
                      *p_log_dir;
extern SSL            *cmdssl;
extern struct logdata ld[];

/* Local function prototypes. */
static int            get_log_inode(char *, char *, int, char *, ino_t,
                                    ino_t *, int *, off_t *),
                      log_write(char *, int);


/*############################# check_logs() ############################*/
long
check_logs(time_t now)
{
   int           i,
                 length;
   unsigned int  chars_buffered,
                 chars_buffered_log;
   long          log_interval;
   static time_t last_log_write_time;
   char          buffer[2 + 1 + MAX_LONG_LONG_LENGTH + 1 + MAX_INT_LENGTH + 3],
                 line[MAX_LINE_LENGTH + 1];
#ifdef HAVE_STATX
   struct statx  stat_buf;
#else
   struct stat   stat_buf;
#endif

   /* Initialize some variables. */
   chars_buffered_log = 0;

   for (i = 0; i < (NO_OF_LOGS - 1); i++)
   {
      if (log_defs & ld[i].log_flag)
      {
         if (ld[i].fp == NULL)
         {
            ino_t inode_in_use;

            if (get_log_inode(log_dir, ld[i].log_name,
                              ld[i].log_name_length,
                              ld[i].log_inode_cmd,
                              ld[i].current_log_inode,
                              &inode_in_use, &ld[i].current_log_no,
                              &ld[i].offset) == SUCCESS)
            {
               int fd;

               if (inode_in_use != 0)
               {
                  ld[i].current_log_inode = inode_in_use;
               }
               if ((fd = open(log_dir, O_RDONLY)) == -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Failed to open() `%s' : %s"),
                             log_dir, strerror(errno));
               }
               else
               {
#ifdef HAVE_STATX
                  if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                            STATX_SIZE, &stat_buf) == -1)
#else
                  if (fstat(fd, &stat_buf) == -1)
#endif
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                _("Failed to access `%s' : %s"),
                                log_dir, strerror(errno));
                     (void)close(fd);
                  }
                  else
                  {
                     off_t seek_offset;

#ifdef HAVE_STATX
                     if (stat_buf.stx_size < ld[i].offset)
#else
                     if (stat_buf.st_size < ld[i].offset)
#endif
                     {
#ifdef HAVE_STATX
                        seek_offset = stat_buf.stx_size;
#else
                        seek_offset = stat_buf.st_size;
#endif
                     }
                     else
                     {
                        seek_offset = ld[i].offset;
                     }
                     if (lseek(fd, seek_offset, SEEK_SET) == -1)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                                   _("Failed to lseek() %ld bytes in `%s' : %s"),
#else
                                   _("Failed to lseek() %lld bytes in `%s' : %s"),
#endif
                                   (pri_off_t)seek_offset, log_dir,
                                   strerror(errno));
                        (void)close(fd);
                     }
                     else
                     {
                        if ((ld[i].fp = fdopen(fd, "r")) == NULL)
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      _("Failed to fdopen() `%s' : %s"),
                                      log_dir, strerror(errno));
                           (void)close(fd);
                        }
                     }
                  }
               }
            }
         }
         if ((ld[i].fp != NULL) &&
             ((chars_buffered_log + MAX_LINE_LENGTH + MAX_LOG_COMMAND_LENGTH) < MAX_LOG_DATA_BUFFER))
         {
            chars_buffered = 0;
            while (((chars_buffered_log + chars_buffered + MAX_LINE_LENGTH + MAX_LOG_COMMAND_LENGTH) < MAX_LOG_DATA_BUFFER) &&
                   (fgets(line, MAX_LINE_LENGTH, ld[i].fp) != NULL))
            {
               length = strlen(line);
               (void)memcpy(&line_buffer[chars_buffered], line, length);
               chars_buffered += length;
            }
            clearerr(ld[i].fp);
            if (chars_buffered > 0)
            {
               chars_buffered_log += snprintf(&log_buffer[chars_buffered_log],
                                              MAX_LOG_DATA_BUFFER - chars_buffered_log,
                                              "%s %u %u %u", /* MAX_LOG_COMMAND_LENGTH */
                                              ld[i].log_data_cmd,
                                              ld[i].options,
                                              ld[i].packet_no,
                                              chars_buffered) + 1;
               if (chars_buffered < (MAX_LOG_DATA_BUFFER - chars_buffered_log))
               {
                  (void)memcpy(&log_buffer[chars_buffered_log], line_buffer,
                               chars_buffered);
               }
               else
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Log buffer to small (%d < %d)"),
                             chars_buffered,
                             MAX_LOG_DATA_BUFFER - chars_buffered_log);
                  exit(INCORRECT);
               }
               chars_buffered_log += chars_buffered;
#ifdef DEBUG_LOG_CMD
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "W-> %s %u %u %u [%s]",
                          ld[i].log_data_cmd, ld[i].options,
                          ld[i].packet_no, chars_buffered,
                          ld[i].log_name);
#endif
               ld[i].packet_no++;
            }
            else
            {
               /*
                * We are not reading any data. This can be normal
                * or we need to continue reading in another log
                * file because the current one is full or has been
                * scheduled to be renamed. There can be two different
                * cases. One if the log number is not zero, then we
                * must decrement log number and continue reading
                * until we reach zero. The other is that it is
                * already zero. In this case we must just check
                * if the current log file has not been renamed to
                * one.
                */
               if (ld[i].current_log_no == 0)
               {
                  /*
                   * Check if a new current log file has been
                   * opened.
                   */
                  (void)strcpy(p_log_dir, ld[i].log_name);
                  *(p_log_dir + ld[i].log_name_length) = '0';
                  *(p_log_dir + ld[i].log_name_length + 1) = '\0';
#ifdef HAVE_STATX
                  if (statx(0, log_dir, AT_STATX_SYNC_AS_STAT,
                            STATX_INO, &stat_buf) == 0)
#else
                  if (stat(log_dir, &stat_buf) == 0)
#endif
                  {
#ifdef HAVE_STATX
                     if (stat_buf.stx_ino != ld[i].current_log_inode)
#else
                     if (stat_buf.st_ino != ld[i].current_log_inode)
#endif
                     {
                        if (fclose(ld[i].fp) == EOF)
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      _("Failed to fclose() log file : %s"),
                                      strerror(errno));
                        }
                        if ((ld[i].fp = fopen(log_dir, "r")) == NULL)
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      _("Failed to fopen() `%s' : %s"),
                                      log_dir, strerror(errno));
                        }
                        else
                        {
#ifdef HAVE_STATX
                           ld[i].current_log_inode = stat_buf.stx_ino;
#else
                           ld[i].current_log_inode = stat_buf.st_ino;
#endif
                           length = snprintf(buffer,
                                             2 + 1 + MAX_LONG_LONG_LENGTH + 1 + MAX_INT_LENGTH + 3,
#if SIZEOF_INO_T == 4
                                             "%s %ld 0\r\n",
#else
                                             "%s %lld 0\r\n",
#endif
                                             ld[i].log_inode_cmd,
#ifdef HAVE_STATX
                                             (pri_ino_t)stat_buf.stx_ino
#else
                                             (pri_ino_t)stat_buf.st_ino
#endif
                                            );
                           if (length > (2 + 1 + MAX_LONG_LONG_LENGTH + 1 + MAX_INT_LENGTH + 3))
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                         "Buffer to small (%d > %d).",
                                         length,
                                         2 + 1 + MAX_LONG_LONG_LENGTH + 1 + MAX_INT_LENGTH + 3);
                              length = 2 + 1 + MAX_LONG_LONG_LENGTH + 1 + MAX_INT_LENGTH + 3;
                              buffer[length - 2] = '\r';
                              buffer[length - 1] = '\n';
                           }
                           if (log_write(buffer, length) == INCORRECT)
                           {
                              exit(INCORRECT);
                           }
#ifdef DEBUG_LOG_CMD
                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
# if SIZEOF_INO_T == 4
                                      "W-> %s %ld 0",
# else
                                      "W-> %s %lld 0",
# endif
                                      ld[i].log_inode_cmd,
# ifdef HAVE_STATX
                                      (pri_ino_t)stat_buf.stx_ino
# else
                                      (pri_ino_t)stat_buf.st_ino
# endif
                                     );
#endif
                        }
                     }
                  }
               }
               else
               {
                  /* Open the next log file. */
                  if (fclose(ld[i].fp) == EOF)
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                _("Failed to fclose() log file : %s"),
                                strerror(errno));
                  }
                  ld[i].fp = NULL;
                  do
                  {
                     ld[i].current_log_no--;
                     (void)snprintf(p_log_dir,
                                    MAX_PATH_LENGTH - (p_log_dir - log_dir),
                                    "%s%d", ld[i].log_name,
                                    ld[i].current_log_no);
                     if ((ld[i].fp = fopen(log_dir, "r")) != NULL)
                     {
#ifdef HAVE_STATX
                        if (statx(fileno(ld[i].fp), "",
                                  AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                                  STATX_INO, &stat_buf) == -1)
#else
                        if (fstat(fileno(ld[i].fp), &stat_buf) == -1)
#endif
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      _("Failed to access `%s' : %s"),
                                      log_dir, strerror(errno));
                           if (fclose(ld[i].fp) == EOF)
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                         _("Failed to fclose() log file : %s"),
                                         strerror(errno));
                           }
                           ld[i].fp = NULL;
                        }
                        else
                        {
#ifdef HAVE_STATX
                           ld[i].current_log_inode = stat_buf.stx_ino;
#else
                           ld[i].current_log_inode = stat_buf.st_ino;
#endif
                           length = snprintf(buffer,
                                             2 + 1 + MAX_LONG_LONG_LENGTH + 1 + MAX_INT_LENGTH + 3,
#if SIZEOF_INO_T == 4
                                             "%s %ld %d\r\n",
#else
                                             "%s %lld %d\r\n",
#endif
                                             ld[i].log_inode_cmd,
#ifdef HAVE_STATX
                                             (pri_ino_t)stat_buf.stx_ino,
#else
                                             (pri_ino_t)stat_buf.st_ino,
#endif
                                             ld[i].current_log_no);
                           if (length > (2 + 1 + MAX_LONG_LONG_LENGTH + 1 + MAX_INT_LENGTH + 3))
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                         "Buffer to small (%d > %d).",
                                         length,
                                         2 + 1 + MAX_LONG_LONG_LENGTH + 1 + MAX_INT_LENGTH + 3);
                              length = 2 + 1 + MAX_LONG_LONG_LENGTH + 1 + MAX_INT_LENGTH + 3;
                              buffer[length - 2] = '\r';
                              buffer[length - 1] = '\n';
                           }
                           if (log_write(buffer, length) == INCORRECT)
                           {
                              exit(INCORRECT);
                           }
#ifdef DEBUG_LOG_CMD
                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
# if SIZEOF_INO_T == 4
                                      "W-> %s %ld %d",
# else
                                      "W-> %s %lld %d",
# endif
                                      ld[i].log_inode_cmd,
# ifdef HAVE_STATX
                                      (pri_ino_t)stat_buf.stx_ino,
# else
                                      (pri_ino_t)stat_buf.st_ino,
# endif
                                      ld[i].current_log_no);
#endif
                        }
                     }
                  } while ((ld[i].current_log_no != 0) &&
                           (ld[i].fp == NULL));
               }
            }
         }
      }
   }

   if (chars_buffered_log > 0)
   {
      if (log_write(log_buffer, chars_buffered_log) == INCORRECT)
      {
         exit(INCORRECT);
      }
      last_log_write_time = now;

      /*
       * So that we do not read the logs at AFDD_LOG_CHECK_INTERVAL time
       * when the buffer is full, lets always return the check interval
       * to the calling process. Otherwise we will only be able to read
       * data at MAX_LOG_DATA_BUFFER / AFDD_LOG_CHECK_INTERVAL bytes
       * per second.
       */
      if ((chars_buffered_log + MAX_LINE_LENGTH + MAX_LOG_COMMAND_LENGTH) >= MAX_LOG_DATA_BUFFER)
      {
         log_interval = 0;
      }
      else
      {
         log_interval = AFDD_LOG_CHECK_INTERVAL;
      }
   }
   else
   {
      if ((last_log_write_time + LOG_WRITE_INTERVAL) < now)
      {
         buffer[0] = 'L';
         buffer[1] = 'N';
         buffer[2] = '\r';
         buffer[3] = '\n';
         if (log_write(buffer, 4) == INCORRECT)
         {
            exit(INCORRECT);
         }
#ifdef DEBUG_LOG_CMD
         system_log(DEBUG_SIGN, __FILE__, __LINE__, "Send LN.");
#endif
         last_log_write_time = now;
      }
      log_interval = AFDD_LOG_CHECK_INTERVAL;
   }

   return(log_interval);
}


/*+++++++++++++++++++++++++++ get_log_inode() +++++++++++++++++++++++++++*/
static int
get_log_inode(char  *log_dir,
              char  *log_name,
              int   log_name_length,
              char  *log_inode_cmd,
              ino_t current_inode,
              ino_t *inode_in_use,
              int   *current_log_no,
              off_t *offset)
{
   int  length;
   char buffer[2 + 1 + MAX_LONG_LONG_LENGTH + 1 + MAX_INT_LENGTH + 1];

   *inode_in_use = 0;
   *current_log_no = -1;
   if (current_inode != 0)
   {
      DIR *dp;

      *p_log_dir = '\0';
      if ((dp = opendir(log_dir)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to opendir() `%s' : %s"), log_dir, strerror(errno));
         return(INCORRECT);
      }
      else
      {
#ifdef HAVE_STATX
         struct statx  stat_buf;
#else
         struct stat   stat_buf;
#endif
         struct dirent *p_dir;

         errno = 0;
         while ((p_dir = readdir(dp)) != NULL)
         {
            if (p_dir->d_name[0] != '.')
            {
               if (strncmp(p_dir->d_name, log_name, log_name_length) == 0)
               {
                  (void)strcpy(p_log_dir, p_dir->d_name);
#ifdef HAVE_STATX
                  if (statx(0, log_dir, AT_STATX_SYNC_AS_STAT,
                            STATX_MODE | STATX_INO, &stat_buf) == -1)
#else
                  if (stat(log_dir, &stat_buf) == -1)
#endif
                  {
                     if (errno != ENOENT)
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   _("Can't access file `%s' : %s"),
                                   log_dir, strerror(errno));
                     }
                  }
                  else
                  {
                     /* Sure it is a normal file? */
#ifdef HAVE_STATX
                     if (S_ISREG(stat_buf.stx_mode))
#else
                     if (S_ISREG(stat_buf.st_mode))
#endif
                     {
#ifdef HAVE_STATX
                        if (stat_buf.stx_ino == current_inode)
#else
                        if (stat_buf.st_ino == current_inode)
#endif
                        {
                           char *ptr;

                           ptr = p_dir->d_name;
                           while (*ptr != '\0')
                           {
                              ptr++;
                           }
                           if (ptr != p_dir->d_name)
                           {
                              ptr--;
                              while (isdigit((int)(*ptr)))
                              {
                                 ptr--;
                              }
                              if (*ptr == '.')
                              {
                                 ptr++;
                                 *current_log_no = atoi(ptr);
                              }
                           }
                           if (*current_log_no == -1)
                           {
                              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                         _("Hmm, unable to determine the log number for `%s'."),
                                         p_dir->d_name);

                              /* Since we could NOT open the original */
                              /* log data file, we must reset offset. */
                              *offset = 0;
                           }
                           else
                           {
                              *inode_in_use = current_inode;
                           }
                           break;
                        }
                     }
                  }
               }
            }
         }
         if (errno)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("readdir() error : %s"), strerror(errno));
         }

         if (closedir(dp) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("closedir() error : %s"), strerror(errno));
         }
      }
   }

   if ((*inode_in_use == 0) || (*current_log_no == -1))
   {
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat stat_buf;
#endif

      (void)strcpy(p_log_dir, log_name);
      *(p_log_dir + log_name_length) = '0';
      *(p_log_dir + log_name_length + 1) = '\0';
#ifdef HAVE_STATX
      if (statx(0, log_dir, AT_STATX_SYNC_AS_STAT,
                STATX_INO, &stat_buf) == -1)
#else
      if (stat(log_dir, &stat_buf) == -1)
#endif
      {
         if (errno == ENOENT)
         {
            FILE *fp;

            /*
             * We use fopen() here since it sets the permission
             * according to umask which is simpler then using
             * open(). The process system_log, output_log, etc
             * also do it this way.
             */
            if ((fp = fopen(log_dir, "a")) == NULL)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          _("Failed to fopen() `%s' : %s"),
                          log_dir, strerror(errno));
               return(INCORRECT);
            }
            else
            {
#ifdef HAVE_STATX
               if (statx(fileno(fp), "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                         STATX_INO, &stat_buf) == -1)
#else
               if (fstat(fileno(fp), &stat_buf) == -1)
#endif
               {
                  system_log(FATAL_SIGN, __FILE__, __LINE__,
                             _("Failed to access `%s' : %s"),
                             log_dir, strerror(errno));
                  (void)fclose(fp);
                  return(INCORRECT);
               }
               else
               {
#ifdef HAVE_STATX
                  *inode_in_use = stat_buf.stx_ino;
#else
                  *inode_in_use = stat_buf.st_ino;
#endif
                  *current_log_no = 0;
               }
               if (fclose(fp) == EOF)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             _("Failed to fclose() `%s' : %s"),
                             log_dir, strerror(errno));
               }
            }
         }
         else
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       _("Failed to stat() `%s' : %s"),
                       log_dir, strerror(errno));
            return(INCORRECT);
         }
      }
      else
      {
#ifdef HAVE_STATX
         *inode_in_use = stat_buf.stx_ino;
#else
         *inode_in_use = stat_buf.st_ino;
#endif
         *offset = 0;
         *current_log_no = 0;
      }
   }

   /*
    * Lets always inform the remote node which inode and log number
    * we are currently using. Since it will not know the correct
    * log number.
    */
   length = snprintf(buffer,
                     2 + 1 + MAX_LONG_LONG_LENGTH + 1 + MAX_INT_LENGTH + 1,
#if SIZEOF_INO_T == 4
                     "%s %ld %d\r\n",
#else
                     "%s %lld %d\r\n",
#endif
                     log_inode_cmd, (pri_ino_t)*inode_in_use, *current_log_no);
   if (length > (2 + 1 + MAX_LONG_LONG_LENGTH + 1 + MAX_INT_LENGTH + 1))
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Buffer to small (%d > %d).",
                 length,
                 2 + 1 + MAX_LONG_LONG_LENGTH + 1 + MAX_INT_LENGTH + 1);
      length = 2 + 1 + MAX_LONG_LONG_LENGTH + 1 + MAX_INT_LENGTH + 1;
      buffer[length - 2] = '\r';
      buffer[length - 1] = '\n';
   }
   if (log_write(buffer, length) == INCORRECT)
   {
      exit(INCORRECT);
   }
#ifdef DEBUG_LOG_CMD
   system_log(DEBUG_SIGN, __FILE__, __LINE__,
# if SIZEOF_INO_T == 4
              "W-> %s %ld %d",
# else
              "W-> %s %lld %d",
# endif
              log_inode_cmd, (pri_ino_t)*inode_in_use, *current_log_no);
#endif

   return(SUCCESS);
}


/*----------------------------- log_write() -----------------------------*/
static int
log_write(char *block, int size)
{
   int            status;
   fd_set         wset;
   struct timeval timeout;

   /* Initialise descriptor set. */
   FD_ZERO(&wset);
   FD_SET(cmd_sd, &wset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = AFDD_CMD_TIMEOUT;

   /* Wait for message x seconds and then continue. */
   status = select(cmd_sd + 1, NULL, &wset, NULL, &timeout);

   if (status == 0)
   {
      /* Timeout has arrived. */
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 _("log_write(): Log data connection timeout (%ld)."),
                 AFDD_CMD_TIMEOUT);
      return(INCORRECT);
   }
   else if (FD_ISSET(cmd_sd, &wset))
        {
           if ((status = ssl_write(cmdssl, block, size)) != size)
           {
              return(INCORRECT);
           }
        }
        else
        {
           system_log(ERROR_SIGN, __FILE__, __LINE__,
                      _("log_write(): select() error : %s"), strerror(errno));
           return(INCORRECT);
        }

   return(SUCCESS);
}
