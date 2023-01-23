/*
 *  write_afd_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2006 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   write_afd_log - writes data to the relevant log files
 **
 ** SYNOPSIS
 **   void write_afd_log(int          log_type,
 **                      unsigned int options,
 **                      unsigned int packet_length,
 **                      char         *buffer)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.12.2006 H.Kiehl Created
 **   22.01.2023 H.Kiehl Use writen() instead of write() so logs
 **                      can be on a network filesystem.
 **                      Add print_log_type() to make add more information
 **                      in error message.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>        /* strcpy()                                   */
#include <ctype.h>         /* isdigit()                                  */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <unistd.h>
#include <errno.h>
#include "mondefs.h"
#include "logdefs.h"
#include "afdddefs.h"

/* Global variables. */
extern int                    log_fd[];
extern char                   log_dir[],
                              *p_log_dir;
extern unsigned int           log_flags[];
extern struct mon_status_area *msa;

/* Local function prototypes. */
static char                   *print_log_type(int);
static void                   add_log_number(char *, int),
                              get_log_name(int, char *);


/*########################### write_afd_log() ###########################*/
void
write_afd_log(int          afd_no,
              int          log_type,
              unsigned int options,
              unsigned int packet_length,
              char         *buffer)
{
   char log_name[MAX_LOG_NAME_LENGTH + 1];

   if ((log_fd[log_type] == -1) &&
       (msa[afd_no].log_capabilities & log_flags[log_type]) &&
       (msa[afd_no].options & log_flags[log_type]))
   {
      get_log_name(log_type, log_name);
      (void)strcpy(p_log_dir, log_name);
      if ((log_fd[log_type] = open(log_dir, O_WRONLY | O_APPEND | O_CREAT,
                                   FILE_MODE)) == -1)
      {
         if (errno == ENOENT)
         {
            if ((log_fd[log_type] = open(log_dir, O_WRONLY | O_CREAT,
                                         FILE_MODE)) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to open() `%s' : %s",
                          log_dir, strerror(errno));
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to open() `%s' : %s",
                       log_dir, strerror(errno));
         }
      }
   }
   if (log_fd[log_type] != -1)
   {
      /* Currently we know nothing about compression. If compression */
      /* is used, lets just drop the package.                        */
      if (options == 0)
      {
         if (writen(log_fd[log_type], buffer, packet_length,
                    packet_length) != packet_length)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to write() %u bytes to %s log : %s",
                       packet_length, print_log_type(log_type),
                       strerror(errno));
         }
      }
      else
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Hmm, receiving some options (%u).", options);
      }
   }

   return;
}


/*+++++++++++++++++++++++++ print_log_type() ++++++++++++++++++++++++++++*/
static char *
print_log_type(int log_type)
{
   switch (log_type)
   {
#ifdef _OUTPUT_LOG
      case OUT_LOG_POS : return("output");
#endif
#ifdef _INPUT_LOG
      case INP_LOG_POS : return("input");
#endif
      case TRA_LOG_POS : return("transfer");
      case REC_LOG_POS : return("receive");
#ifdef _DISTRIBUTION_LOG
      case DIS_LOG_POS : return("distribution");
#endif
#ifdef _PRODUCTION_LOG
      case PRO_LOG_POS : return("production");
#endif
#ifdef _DELETE_LOG
      case DEL_LOG_POS : return("delete");
#endif
      case SYS_LOG_POS : return("system");
      case EVE_LOG_POS : return("event");
      case TDB_LOG_POS : return("transfer debug");
   }

   return("unknown");
}


/*++++++++++++++++++++++++++ get_log_name() +++++++++++++++++++++++++++++*/
static void
get_log_name(int log_type, char *log_name)
{
   int log_name_length;

   switch (log_type)
   {
#ifdef _OUTPUT_LOG
      case OUT_LOG_POS :
         (void)strcpy(log_name, OUTPUT_BUFFER_FILE);
         log_name_length = OUTPUT_BUFFER_FILE_LENGTH;
         break;
#endif
#ifdef _INPUT_LOG
      case INP_LOG_POS :
         (void)strcpy(log_name, INPUT_BUFFER_FILE);
         log_name_length = INPUT_BUFFER_FILE_LENGTH;
         break;
#endif
      case TRA_LOG_POS :
         (void)strcpy(log_name, TRANSFER_LOG_NAME);
         log_name_length = TRANSFER_LOG_NAME_LENGTH;
         break;
      case REC_LOG_POS :
         (void)strcpy(log_name, RECEIVE_LOG_NAME);
         log_name_length = RECEIVE_LOG_NAME_LENGTH;
         break;
#ifdef _DISTRIBUTION_LOG
      case DIS_LOG_POS :
         (void)strcpy(log_name, DISTRIBUTION_BUFFER_FILE);
         log_name_length = DISTRIBUTION_BUFFER_FILE_LENGTH;
         break;
#endif
#ifdef _PRODUCTION_LOG
      case PRO_LOG_POS :
         (void)strcpy(log_name, PRODUCTION_BUFFER_FILE);
         log_name_length = PRODUCTION_BUFFER_FILE_LENGTH;
         break;
#endif
#ifdef _DELETE_LOG
      case DEL_LOG_POS :
         (void)strcpy(log_name, DELETE_BUFFER_FILE);
         log_name_length = DELETE_BUFFER_FILE_LENGTH;
         break;
#endif
      case SYS_LOG_POS :
         (void)strcpy(log_name, SYSTEM_LOG_NAME);
         log_name_length = SYSTEM_LOG_NAME_LENGTH;
         break;
      case EVE_LOG_POS :
         (void)strcpy(log_name, EVENT_LOG_NAME);
         log_name_length = EVENT_LOG_NAME_LENGTH;
         break;
      case TDB_LOG_POS :
         (void)strcpy(log_name, TRANS_DB_LOG_NAME);
         log_name_length = TRANS_DB_LOG_NAME_LENGTH;
         break;
      default : /* Unknown log type */
         log_name[0] = '\0';
         return;
   }
   add_log_number(log_name, log_name_length);

   return;
}


/*-------------------------- add_log_number() ---------------------------*/
static void
add_log_number(char *log_name, int log_name_length)
{
   int fd;

   (void)sprintf(p_log_dir, "%s%s", log_name, REMOTE_INODE_EXTENSION);
   if ((fd = open(log_dir, O_RDONLY)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to open() `%s' : %s", log_dir, strerror(errno));
   }
   else
   {
      char buffer[MAX_INODE_LOG_NO_LENGTH];

      if (read(fd, buffer, MAX_INODE_LOG_NO_LENGTH) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to read() from `%s' : %s",
                    log_dir, strerror(errno));
      }
      else
      {
         int i = 0;

         while ((i < MAX_INODE_LOG_NO_LENGTH) && (buffer[i] != ' '))
         {
            i++;
         }
         if ((i > 0) && (i < MAX_INODE_LOG_NO_LENGTH))
         {
            int orig_i;

            i++;
            orig_i = i;
            while ((i < MAX_INODE_LOG_NO_LENGTH) && (isdigit((int)(buffer[i]))))
            {
               log_name[log_name_length] = buffer[i];
               log_name_length++;
               i++;
            }
            if (i == orig_i)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to find the log number!");
            }
            else
            {
               log_name[log_name_length] = '\0';
               if (close(fd) == -1)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Failed to close() `%s' : %s",
                             log_dir, strerror(errno));
               }
               return;
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to locate the log number! (%d)", i);
         }
      }
      if (close(fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Failed to close() `%s' : %s", log_dir, strerror(errno));
      }
   }
   log_name[log_name_length] = '0';
   log_name[log_name_length + 1] = '\0';

   return;
}
