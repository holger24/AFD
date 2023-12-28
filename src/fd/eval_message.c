/*
 *  eval_message.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2023 Deutscher Wetterdienst (DWD),
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

#include "afddefs.h"

DESCR__S_M3
/*
 ** NAME
 **   eval_message - reads the recipient string
 **
 ** SYNOPSIS
 **   int eval_message(char *message_name, struct job *p_db)
 **
 ** DESCRIPTION
 **   Reads and evaluates the message file 'message_name'. If this
 **   message is faulty it is moved to the faulty directory.
 **
 ** RETURN VALUES
 **   Returns INCORRECT if it fails to evaluate the message file.
 **   On success it returns SUCCESS and the following structure:
 **   struct job *p_db   - The structure in which we store the
 **                        following values:
 **
 **                             - user
 **                             - password
 **                             - hostname
 **                             - directory
 **                             - send as email
 **                             - lock type
 **                             - archive time
 **                             - trans_rename_rule
 **                             - user_rename_rule
 **                             - user_id
 **                             - group_id
 **                             - FTP transfer mode
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   07.11.1995 H.Kiehl Created
 **   16.09.1997 H.Kiehl Support for FSS ready files.
 **   08.10.1997 H.Kiehl Included renaming with directories on remote site.
 **   30.11.1997 H.Kiehl Take last RESTART_FILE_ID in message.
 **   25.01.1998 H.Kiehl Allow for more then one restart file.
 **   24.06.1998 H.Kiehl Added option for secure ftp.
 **   26.04.1999 H.Kiehl Added option no login
 **   24.08.1999 H.Kiehl Enhanced option "attach file" to support trans-
 **                      renaming.
 **   18.03.2000 H.Kiehl Added option FTP transfer mode.
 **   03.11.2000 H.Kiehl Chmod option for FTP as well.
 **   27.01.2002 H.Kiehl Added reply-to option.
 **   02.07.2002 H.Kiehl Added from option.
 **   03.07.2002 H.Kiehl Added charset option.
 **   30.07.2002 H.Kiehl Check if subject string contains a %t to insert
 **                      a date.
 **   05.01.2003 H.Kiehl Allow user to specify the filename of the lock
 **                      file.
 **   15.07.2003 H.Kiehl Added option "rename file busy"
 **   28.08.2004 H.Kiehl Added options "create target dir" and "do no
 **                      create target dir"
 **   14.06.2005 H.Kiehl Added "dupcheck" option.
 **   21.09.2005 H.Kiehl Added "sequence locking" option.
 **   12.01.2006 H.Kiehl Added "login site" option.
 **   16.02.2006 H.Kiehl Added "socket send/receive buffer" option.
 **   04.06.2006 H.Kiehl Added option 'file name is target'.
 **   05.08.2006 H.Kiehl For option 'subject' added possibility to insert
 **                      the filename or part of it.
 **   11.02.2006 H.Kiehl Evaluate options for pexec command.
 **   23.01.2010 H.Kiehl Add support to mirror source.
 **   13.10.2010 H.Kiehl Added %T time modifier for subject.
 **   26.11.2010 H.Kiehl Do not go into an endless loop when we hit an
 **                      unknown pexec option.
 **   04.11.2011 H.Kiehl Added support for scheduling priorities.
 **   11.01.2014 H.Kiehl Added support for trans_rename with dupcheck.
 **   14.05.2017 H.Kiehl When storing mail address allow user to place
 **                      the current host name with the %H and %h option.
 **   06.07.2019 H.Kiehl Added support for trans_srename.
 **   22.06.2020 H.Kiehl Added option 'show no to line'.
 **   29.06.2020 H.Kiehl Added option 'group-to'.
 **   29.06.2023 H.Kiehl Ignore FD option 'ageing'.
 **
 */
DESCR__E_M3

#include <stdio.h>                /* stderr, fprintf()                   */
#include <stdlib.h>               /* atoi(), malloc(), free(), strtoul() */
#include <string.h>               /* strcmp(), strncmp(), strerror()     */
#include <ctype.h>                /* isascii()                           */
#include <time.h>                 /* time(), strftime(), localtime()     */
#include <sys/types.h>
#ifdef HAVE_SETPRIORITY
# include <sys/time.h>
# include <sys/resource.h>        /* getpriority()                       */
# include <sys/stat.h>            /* stat()                              */
#endif
#include <grp.h>                  /* getgrnam()                          */
#include <pwd.h>                  /* getpwnam()                          */
#include <unistd.h>               /* read(), close(), setuid()           */
#include <fcntl.h>                /* O_RDONLY, etc                       */
#ifdef WITH_EUMETSAT_HEADERS
# include <netdb.h>               /* gethostbyname()                     */
#endif
#include <errno.h>
#include "fddefs.h"
#include "ftpdefs.h"

/* External global variables. */
extern int  transfer_log_fd,
            trans_rename_blocked;
extern char *p_work_dir;

/* Local function prototypes. */
static void store_mode(char *, struct job *, char *, int);
static char *store_mail_address(char *, char **, char *, unsigned int);


#define ARCHIVE_FLAG                1
#define AGE_LIMIT_FLAG              2
#define LOCK_FLAG                   4
#define TRANS_RENAME_FLAG           8
#define CHMOD_FLAG                  16
#define CHOWN_FLAG                  32
#define OUTPUT_LOG_FLAG             64
#define RESTART_FILE_FLAG           128
#define FILE_NAME_IS_HEADER_FLAG    256
#define SUBJECT_FLAG                512
#define FORCE_COPY_FLAG             1024
#define FILE_NAME_IS_SUBJECT_FLAG   2048
#define REPLY_TO_FLAG               4096
#define FROM_FLAG                   8192
#define CHECK_ANSI_FLAG             16384
#define CHECK_REPLY_FLAG            32768
#define WITH_SEQUENCE_NUMBER_FLAG   65536
#define ATTACH_FILE_FLAG            131072
#define ADD_MAIL_HEADER_FLAG        262144
#define FTP_EXEC_FLAG               524288
#define CHARSET_FLAG                1048576
#define FILE_NAME_IS_USER_FLAG      2097152
#ifdef WITH_EUMETSAT_HEADERS
# define EUMETSAT_HEADER_FLAG       4194304
#endif
#define RENAME_FILE_BUSY_FLAG       8388608
#define CHANGE_FTP_MODE_FLAG        16777216
#define ATTACH_ALL_FILES_FLAG       33554432
#ifdef _WITH_TRANS_EXEC
# define TRANS_EXEC_FLAG            67108864
#endif
#define LOCK_POSTFIX_FLAG           134217728
#define CREATE_TARGET_DIR_FLAG      268435456
#define DONT_CREATE_TARGET_DIR_FLAG 536870912
#ifdef WITH_DUP_CHECK
# define DUPCHECK_FLAG              1073741824
#endif
#define SEQUENCE_LOCKING_FLAG       2147483648U
#define LOGIN_SITE_FLAG             1
#define SOCK_SND_BUF_SIZE_FLAG      2
#define SOCK_RCV_BUF_SIZE_FLAG      4
#define FILE_NAME_IS_TARGET_FLAG    8
#define MIRROR_DIR_FLAG             16
#define SHOW_ALL_GROUP_MEMBERS_FLAG 32
#define CHECK_REMOTE_SIZE_FLAG      64
#define HIDE_ALL_GROUP_MEMBERS_FLAG 128
#define SHOW_NO_TO_LINE_FLAG        256
#ifdef _WITH_DE_MAIL_SUPPORT
# define CONF_OF_RETRIEVE_FLAG      512
#endif
#define SILENT_DEF_NO_LOCK_FLAG     1024
#define TRANS_SRENAME_FLAG          2048
#define GROUP_TO_FLAG               4096
#define REMOTE_HARDLINK_FLAG        8192
#define REMOTE_SYMLINK_FLAG         16384


#define MAX_HUNK                    4096


/*############################ eval_message() ###########################*/
int
eval_message(char *message_name, struct job *p_db)
{
   int          bytes_buffered,
                fd,
                n;
   unsigned int used = 0; /* Used to see whether option has already been set. */
   char         *ptr,
                *msg_buf;

   /* Store message to buffer. */
   if ((fd = open(message_name, O_RDONLY)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to open() %s : %s", message_name, strerror(errno));
      exit(NO_MESSAGE_FILE);
   }

   /* Allocate memory to store message. */
   if ((msg_buf = malloc(MAX_HUNK + 1)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not malloc() memory to read message : %s",
                 strerror(errno));
      exit(INCORRECT);
   }

   ptr = msg_buf;
   bytes_buffered = 0;
   do
   {
      if ((n = read(fd, ptr, MAX_HUNK)) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to read() %s : %s", message_name, strerror(errno));
         exit(INCORRECT);
      }
      bytes_buffered += n;
      if (n == MAX_HUNK)
      {
         if ((msg_buf = realloc(msg_buf, bytes_buffered + MAX_HUNK + 1)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not realloc() memory to read message : %s",
                       strerror(errno));
            exit(INCORRECT);
         }
         ptr = msg_buf + bytes_buffered;
      }
   } while (n == MAX_HUNK);

   msg_buf[bytes_buffered] = '\0';
   if (close(fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }

   /*
    * First let's evaluate the recipient.
    */
   if ((ptr = lposi(msg_buf, DESTINATION_IDENTIFIER,
                    DESTINATION_IDENTIFIER_LENGTH)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Message %s is faulty.", message_name);

      /* Don't forget to free memory we have allocated. */
      free(msg_buf);

      /* It's obvious that the message is corrupt. */
      return(INCORRECT);
   }
   if (p_db->recipient == NULL)
   {
      if ((p_db->recipient = malloc(MAX_RECIPIENT_LENGTH + 1)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "malloc() error : %s", strerror(errno));
         exit(INCORRECT);
      }
   }
   n = 0;
   while ((n < MAX_RECIPIENT_LENGTH) && (*ptr != '\0') && (*ptr != '\n'))
   {
      p_db->recipient[n] = *ptr;
      ptr++; n++;
   }
   p_db->recipient[n] = '\0';

   if (eval_recipient(p_db->recipient, p_db, message_name, 0L) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__, "eval_recipient() failed.");

      /* Don't forget to free memory we have allocated. */
      free(msg_buf);

      return(INCORRECT);
   }

   if (*ptr != '\0')
   {
      /*
       * Now lets see which options have been set.
       */
      if ((ptr = lposi(ptr, OPTION_IDENTIFIER,
                       OPTION_IDENTIFIER_LENGTH)) != NULL)
      {
         unsigned int used2 = 0;
         char         byte_buf,
                      *end_ptr;

         while (*ptr != '\0')
         {
            if (((used & ARCHIVE_FLAG) == 0) &&
                (CHECK_STRNCMP(ptr, ARCHIVE_ID, ARCHIVE_ID_LENGTH) == 0))
            {
               used |= ARCHIVE_FLAG;
               ptr += ARCHIVE_ID_LENGTH;
               while ((*ptr == ' ') || (*ptr == '\t'))
               {
                  ptr++;
               }
               end_ptr = ptr;
               while ((isdigit((int)(*end_ptr))) && (*end_ptr != '\n') &&
                      (*end_ptr != '\0'))
               {
                 end_ptr++;
               }
               byte_buf = *end_ptr;
               *end_ptr = '\0';
               if (p_db->archive_time != -1)
               {
                  int unit;

                  switch (byte_buf)
                  {
                     case '\n' :
                     case '\0' : /* Default unit. */
                        unit = DEFAULT_ARCHIVE_UNIT;
                        break;

                     case 'd' : /* Days. */
                        unit = 86400;
                        break;

                     case 'h' : /* Hours. */
                        unit = 3600;
                        break;

                     case 'm' : /* Minutes. */
                        unit = 60;
                        break;

                     case 's' : /* Seconds. */
                        unit = 1;
                        break;

                     default : /* Unknown. */
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   "Unknown unit type `%c' (%d) for %s option. Taking default. #%x",
                                   byte_buf, (int)byte_buf, ARCHIVE_ID,
                                   p_db->id.job);
                        unit = DEFAULT_ARCHIVE_UNIT;
                        break;
                  }
                  p_db->archive_time = atoi(ptr) * unit;
               }
               *end_ptr = byte_buf;
               ptr = end_ptr;
               while ((*ptr != '\n') && (*ptr != '\0'))
               {
                  ptr++;
               }
               while (*ptr == '\n')
               {
                  ptr++;
               }
            }
            else if (((used & AGE_LIMIT_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, AGE_LIMIT_ID,
                                    AGE_LIMIT_ID_LENGTH) == 0))
                 {
                    used |= AGE_LIMIT_FLAG;
                    ptr += AGE_LIMIT_ID_LENGTH;
                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }
                    end_ptr = ptr;
                    while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                    {
                      end_ptr++;
                    }
                    byte_buf = *end_ptr;
                    *end_ptr = '\0';
                    p_db->age_limit = (unsigned int)atoi(ptr);
                    *end_ptr = byte_buf;
                    ptr = end_ptr;
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & LOCK_POSTFIX_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, LOCK_POSTFIX_ID,
                                    LOCK_POSTFIX_ID_LENGTH) == 0))
                 {
                    size_t length = 0;

                    used |= LOCK_POSTFIX_FLAG;
                    ptr += LOCK_POSTFIX_ID_LENGTH;
                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }
                    end_ptr = ptr;
                    while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                    {
                       end_ptr++;
                       length++;
                    }
                    if (length > 0)
                    {
                       if (length > (LOCK_NOTATION_LENGTH - 1))
                       {
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     "Lock postfix notation `%s' in message `%s' is %d bytes long, it may only be %d bytes long. #%x",
                                     LOCK_POSTFIX_ID, message_name, length,
                                     LOCK_NOTATION_LENGTH, p_db->id.job);
                          p_db->lock = OFF;
                       }
                       else
                       {
                          p_db->lock = POSTFIX;
                          memcpy(p_db->lock_notation, ptr, length);
                          p_db->lock_notation[length] = '\0';
                       }
                    }
                    else
                    {
                       p_db->lock = OFF;
                       system_log(WARN_SIGN, __FILE__, __LINE__,
                                  "No postfix found for option `%s' in message `%s'. #%x",
                                  LOCK_POSTFIX_ID, message_name, p_db->id.job);
                    }
                    ptr = end_ptr;
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & LOCK_FLAG) == 0) &&
                     ((CHECK_STRNCMP(ptr, LOCK_ID, LOCK_ID_LENGTH) == 0) ||
                      (CHECK_STRNCMP(ptr, ULOCK_ID, ULOCK_ID_LENGTH) == 0)))
                 {
                    used |= LOCK_FLAG;
                    if (*ptr == 'u')
                    {
                       p_db->special_flag |= UNIQUE_LOCKING;
                       ptr += ULOCK_ID_LENGTH;
                    }
                    else
                    {
                       ptr += LOCK_ID_LENGTH;
                    }
                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }
                    end_ptr = ptr;
                    while ((*end_ptr != '\n') && (*end_ptr != '\0') &&
                           (*end_ptr != ' ') && (*end_ptr != '\t'))
                    {
                       end_ptr++;
                    }
                    byte_buf = *end_ptr;
                    *end_ptr = '\0';
                    if (CHECK_STRCMP(ptr, LOCK_DOT) == 0)
                    {
                       p_db->lock = DOT;
                    }
                    else if (CHECK_STRCMP(ptr, LOCK_DOT_VMS) == 0)
                         {
                            p_db->lock = DOT_VMS;
                         }
                    else if (CHECK_STRCMP(ptr, LOCK_FILE) == 0)
                         {
                            if ((p_db->lock_file_name = malloc(MAX_RECIPIENT_LENGTH + 1 +
                                                               MAX_LOCK_FILENAME_LENGTH)) == NULL)
                            {
                               system_log(ERROR_SIGN, __FILE__, __LINE__,
                                          "Failed to malloc() %d bytes : %s #%x",
                                          (MAX_PATH_LENGTH + MAX_LOCK_FILENAME_LENGTH),
                                          strerror(errno), p_db->id.job);
                               exit(ALLOC_ERROR);
                            }
                            p_db->lock = LOCKFILE;
                            ptr += strlen(LOCK_FILE);
                            *end_ptr = byte_buf;
                            while ((*ptr == ' ') || (*ptr == '\t'))
                            {
                               ptr++;
                            }
                            n = 0;
                            while ((p_db->target_dir[n] != '\0') && (n < MAX_RECIPIENT_LENGTH))
                            {
                               p_db->lock_file_name[n] = p_db->target_dir[n];
                               n++;
                            }
                            if ((n > 0) && (p_db->lock_file_name[n - 1] != '/'))
                            {
                               p_db->lock_file_name[n] = '/';
                               n++;
                            }
                            while ((*ptr != '\n') && (*ptr != '\0') &&
                                   (n < MAX_LOCK_FILENAME_LENGTH))
                            {
                               p_db->lock_file_name[n] = *ptr;
                               ptr++; n++;
                            }
                            p_db->lock_file_name[n] = '\0';
                            if (n == MAX_LOCK_FILENAME_LENGTH)
                            {
                               system_log(WARN_SIGN, __FILE__, __LINE__,
                                          "The lock file name has been truncated, since we can only store %d bytes. #%x",
                                          MAX_LOCK_FILENAME_LENGTH,
                                          p_db->id.job);
                            }
                            else if (n == 0)
                                 {
                                    (void)strcpy(p_db->lock_file_name,
                                                 LOCK_FILENAME);
                                 }
                         }
                    else if (CHECK_STRCMP(ptr, LOCK_OFF) == 0)
                         {
                            p_db->lock = OFF;
                            p_db->special_flag &= ~UNIQUE_LOCKING;
                         }
#ifdef WITH_READY_FILES
                    else if (CHECK_STRCMP(ptr, LOCK_READY_A_FILE) == 0)
                         {
                            p_db->lock = READY_A_FILE;
                         }
                    else if (CHECK_STRCMP(ptr, LOCK_READY_B_FILE) == 0)
                         {
                            p_db->lock = READY_B_FILE;
                         }
#endif
                         else
                         {
                            size_t length;

                            length = strlen(ptr);
                            if (length > (LOCK_NOTATION_LENGTH - 1))
                            {
                               system_log(WARN_SIGN, __FILE__, __LINE__,
                                          "Lock notation `%s' in message `%s' is %d bytes long, it may only be %d bytes long. #%x",
                                          LOCK_ID, message_name, length,
                                          LOCK_NOTATION_LENGTH, p_db->id.job);
                               p_db->lock = OFF;
                            }
                            else
                            {
                               p_db->lock = DOT;
                               (void)strcpy(p_db->lock_notation, ptr);
                            }
                         }
                    *end_ptr = byte_buf;
                    ptr = end_ptr;
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & TRANS_RENAME_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, TRANS_RENAME_ID,
                                    TRANS_RENAME_ID_LENGTH) == 0))
                 {
                    used |= TRANS_RENAME_FLAG;
                    ptr += TRANS_RENAME_ID_LENGTH;
                    if (trans_rename_blocked == NO)
                    {
                       while ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       end_ptr = ptr;
                       n = 0;
                       while ((*end_ptr != '\n') && (*end_ptr != ' ') &&
                              (*end_ptr != '\0') &&
                              (n < MAX_RULE_HEADER_LENGTH))
                       {
                         p_db->trans_rename_rule[n] = *end_ptr;
                         end_ptr++; n++;
                       }
                       p_db->trans_rename_rule[n] = '\0';
                       if (n == MAX_RULE_HEADER_LENGTH)
                       {
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     "Rule header for trans_rename option %s to long. #%x",
                                     p_db->trans_rename_rule, p_db->id.job);
                          p_db->trans_rename_rule[0] = '\0';
                       }
                       else if (n == 0)
                            {
                               system_log(WARN_SIGN, __FILE__, __LINE__,
                                          "No rule header specified in message %x.",
                                          p_db->id.job);
                            }
                            else
                            {
                               while (*end_ptr == ' ')
                               {
                                  end_ptr++;
                               }
                               /* primary_only */
                               if ((*end_ptr == 'p') &&
                                   (*(end_ptr + 1) == 'r') &&
                                   (*(end_ptr + 2) == 'i') &&
                                   (*(end_ptr + 3) == 'm') &&
                                   (*(end_ptr + 4) == 'a') &&
                                   (*(end_ptr + 5) == 'r') &&
                                   (*(end_ptr + 6) == 'y') &&
                                   (*(end_ptr + 7) == '_') &&
                                   (*(end_ptr + 8) == 'o') &&
                                   (*(end_ptr + 9) == 'n') &&
                                   (*(end_ptr + 10) == 'l') &&
                                   (*(end_ptr + 11) == 'y'))
                               {
                                  p_db->special_flag |= TRANS_RENAME_PRIMARY_ONLY;
                                  end_ptr += 11;
                               }
                                    /* secondary_only */
                               else if ((*end_ptr == 's') &&
                                        (*(end_ptr + 1) == 'e') &&
                                        (*(end_ptr + 2) == 'c') &&
                                        (*(end_ptr + 3) == 'o') &&
                                        (*(end_ptr + 4) == 'n') &&
                                        (*(end_ptr + 5) == 'd') &&
                                        (*(end_ptr + 6) == 'a') &&
                                        (*(end_ptr + 7) == 'r') &&
                                        (*(end_ptr + 8) == 'y') &&
                                        (*(end_ptr + 9) == '_') &&
                                        (*(end_ptr + 10) == 'o') &&
                                        (*(end_ptr + 11) == 'n') &&
                                        (*(end_ptr + 12) == 'l') &&
                                        (*(end_ptr + 13) == 'y'))
                                    {
                                       p_db->special_flag |= TRANS_RENAME_SECONDARY_ONLY;
                                       end_ptr += 13;
                                    }
#ifdef WITH_DUP_CHECK
                               else if (CHECK_STRNCMP(end_ptr, DUPCHECK_ID, DUPCHECK_ID_LENGTH) == 0)
                                    {
                                       end_ptr = eval_dupcheck_options(end_ptr,
                                                                       &p_db->trans_dup_check_timeout,
                                                                       &p_db->trans_dup_check_flag,
                                                                       NULL);
                                       p_db->crc_id = p_db->id.job;
# ifdef DEBUG_DUP_CHECK
                                       system_log(DEBUG_SIGN, __FILE__, __LINE__,
#  if SIZEOF_TIME_T == 4
                                                  "crc_id=%x timeout=%ld flag=%u",
#  else
                                                  "crc_id=%x timeout=%lld flag=%u",
#  endif
                                                  p_db->crc_id,
                                                  (pri_time_t)p_db->trans_dup_check_timeout,
                                                  p_db->trans_dup_check_flag);
# endif
                                    }
#endif
                            }
                       ptr = end_ptr;
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used2 & TRANS_SRENAME_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, TRANS_SRENAME_ID,
                                    TRANS_SRENAME_ID_LENGTH) == 0))
                 {
                    used2 |= TRANS_SRENAME_FLAG;
                    ptr += TRANS_SRENAME_ID_LENGTH;
                    if (trans_rename_blocked == NO)
                    {
                       while ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       end_ptr = ptr;
                       n = 0;
                       while ((*end_ptr != '\n') && (*end_ptr != ' ') &&
                              (*end_ptr != '\0'))
                       {
                          end_ptr++; n++;
                       }
                       if ((*end_ptr == ' ') && (n > 0))
                       {
                          if ((p_db->cn_filter = malloc(n + 1)) == NULL)
                          {
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        "malloc() error : %s",
                                        strerror(errno));
                          }
                          else
                          {
                             (void)memcpy(p_db->cn_filter, ptr, n);
                             p_db->cn_filter[n] = '\0';

                             while (*end_ptr == ' ')
                             {
                                end_ptr++;
                             }
                             ptr = end_ptr;
                             n = 0;
                             while ((*end_ptr != '\n') && (*end_ptr != ' ') &&
                                    (*end_ptr != '\0'))
                             {
                                end_ptr++; n++;
                             }
                             if (n > 0)
                             {
                                if ((p_db->cn_rename_to = malloc(n + 1)) == NULL)
                                {
                                   system_log(WARN_SIGN, __FILE__, __LINE__,
                                              "malloc() error : %s",
                                              strerror(errno));
                                   free(p_db->cn_filter);
                                   p_db->cn_filter = NULL;
                                }
                                else
                                {
                                   (void)memcpy(p_db->cn_rename_to, ptr, n);
                                   p_db->cn_rename_to[n] = '\0';

                                   while (*end_ptr == ' ')
                                   {
                                      end_ptr++;
                                   }
                                   /* primary_only */
                                   if ((*end_ptr == 'p') &&
                                       (*(end_ptr + 1) == 'r') &&
                                       (*(end_ptr + 2) == 'i') &&
                                       (*(end_ptr + 3) == 'm') &&
                                       (*(end_ptr + 4) == 'a') &&
                                       (*(end_ptr + 5) == 'r') &&
                                       (*(end_ptr + 6) == 'y') &&
                                       (*(end_ptr + 7) == '_') &&
                                       (*(end_ptr + 8) == 'o') &&
                                       (*(end_ptr + 9) == 'n') &&
                                       (*(end_ptr + 10) == 'l') &&
                                       (*(end_ptr + 11) == 'y'))
                                   {
                                      p_db->special_flag |= TRANS_RENAME_PRIMARY_ONLY;
                                      end_ptr += 11;
                                   }
                                        /* secondary_only */
                                   else if ((*end_ptr == 's') &&
                                            (*(end_ptr + 1) == 'e') &&
                                            (*(end_ptr + 2) == 'c') &&
                                            (*(end_ptr + 3) == 'o') &&
                                            (*(end_ptr + 4) == 'n') &&
                                            (*(end_ptr + 5) == 'd') &&
                                            (*(end_ptr + 6) == 'a') &&
                                            (*(end_ptr + 7) == 'r') &&
                                            (*(end_ptr + 8) == 'y') &&
                                            (*(end_ptr + 9) == '_') &&
                                            (*(end_ptr + 10) == 'o') &&
                                            (*(end_ptr + 11) == 'n') &&
                                            (*(end_ptr + 12) == 'l') &&
                                            (*(end_ptr + 13) == 'y'))
                                        {
                                           p_db->special_flag |= TRANS_RENAME_SECONDARY_ONLY;
                                           end_ptr += 13;
                                        }
#ifdef WITH_DUP_CHECK
                                   else if (CHECK_STRNCMP(end_ptr, DUPCHECK_ID, DUPCHECK_ID_LENGTH) == 0)
                                        {
                                           end_ptr = eval_dupcheck_options(end_ptr,
                                                                           &p_db->trans_dup_check_timeout,
                                                                           &p_db->trans_dup_check_flag,
                                                                           NULL);
                                           p_db->crc_id = p_db->id.job;
# ifdef DEBUG_DUP_CHECK
                                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
#  if SIZEOF_TIME_T == 4
                                                      "crc_id=%x timeout=%ld flag=%u",
#  else
                                                      "crc_id=%x timeout=%lld flag=%u",
#  endif
                                                      p_db->crc_id,
                                                      (pri_time_t)p_db->trans_dup_check_timeout,
                                                      p_db->trans_dup_check_flag);
# endif
                                        }
#endif
                                }
                             }
                             else
                             {
                                system_log(WARN_SIGN, __FILE__, __LINE__,
                                           "No rename to part specified for trans_srename option. #%x",
                                           p_db->id.job);
                                free(p_db->cn_filter);
                                p_db->cn_filter = NULL;
                             }
                          }
                       }
                       else
                       {
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     "No %s part specified for trans_srename option. #%x",
                                     (n == 0) ? "filter" : "rename to",
                                     p_db->id.job);
                       }

                       ptr = end_ptr;
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & CREATE_TARGET_DIR_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, CREATE_TARGET_DIR_ID,
                                    CREATE_TARGET_DIR_ID_LENGTH) == 0))
                 {
                    used |= CREATE_TARGET_DIR_FLAG;
                    p_db->special_flag |= CREATE_TARGET_DIR;
                    ptr += CREATE_TARGET_DIR_ID_LENGTH;
                    if ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       store_mode(ptr, p_db, CREATE_TARGET_DIR_ID,
                                  CREATE_TARGET_DIR_FLAG);
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & DONT_CREATE_TARGET_DIR_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, DONT_CREATE_TARGET_DIR,
                                    DONT_CREATE_TARGET_DIR_LENGTH) == 0))
                 {
                    used |= DONT_CREATE_TARGET_DIR_FLAG;
                    if (p_db->special_flag & CREATE_TARGET_DIR)
                    {
                       p_db->special_flag ^= CREATE_TARGET_DIR;
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & CHMOD_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, CHMOD_ID, CHMOD_ID_LENGTH) == 0))
                 {
                    used |= CHMOD_FLAG;
                    ptr += CHMOD_ID_LENGTH;
                    store_mode(ptr, p_db, CHMOD_ID, CHMOD_FLAG);
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & CHOWN_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, CHOWN_ID, CHOWN_ID_LENGTH) == 0))
                 {
                    used |= CHOWN_FLAG;
                    ptr += CHOWN_ID_LENGTH;
                    if (p_db->protocol & LOC_FLAG)
                    {
                       int lookup_id = NO;

                       while ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       end_ptr = ptr;
                       while ((*end_ptr != ' ') && (*end_ptr != ':') &&
                              (*end_ptr != '.') && (*end_ptr != '\n') &&
                              (*end_ptr != '\0'))
                       {
                          if (isdigit((int)(*end_ptr)) == 0)
                          {
                             lookup_id = YES;
                          }
                          end_ptr++;
                       }
                       byte_buf = *end_ptr;
                       *end_ptr = '\0';
                       if (lookup_id == YES)
                       {
                          struct passwd *pw;

                          if ((pw = getpwnam(ptr)) == NULL)
                          {
                             (void)rec(transfer_log_fd, ERROR_SIGN,
                                       "getpwnam() error for user %s : %s #%x (%s %d)\n",
                                       ptr, strerror(errno), p_db->id.job,
                                       __FILE__, __LINE__);
                          }
                          else
                          {
                             p_db->user_id = pw->pw_uid;
                          }
                       }
                       else
                       {
                          p_db->user_id = atoi(ptr);
                       }
                       p_db->special_flag |= CHANGE_UID_GID;
                       *end_ptr = byte_buf;
                       ptr = end_ptr;
                       if ((*ptr == ' ') || (*ptr == ':') || (*ptr == '.'))
                       {
                          ptr++; end_ptr++;
                          lookup_id = NO;
                          while ((*end_ptr != ' ') && (*end_ptr != ':') &&
                                 (*end_ptr != '.') && (*end_ptr != '\n') &&
                                 (*end_ptr != '\0'))
                          {
                             if (isdigit((int)(*end_ptr)) == 0)
                             {
                                lookup_id = YES;
                             }
                             end_ptr++;
                          }
                          byte_buf = *end_ptr;
                          *end_ptr = '\0';
                          if (lookup_id == YES)
                          {
                             struct group *gr;

                             if ((gr = getgrnam(ptr)) == NULL)
                             {
                                (void)rec(transfer_log_fd, ERROR_SIGN,
                                          "getgrnam() error for group %s : %s #%x (%s %d)\n",
                                          ptr, strerror(errno), p_db->id.job,
                                          __FILE__, __LINE__);
                             }
                             else
                             {
                                p_db->group_id = gr->gr_gid;
                             }
                          }
                          else
                          {
                             p_db->group_id = atoi(ptr);
                          }
                          *end_ptr = byte_buf;
                          ptr = end_ptr;
                       }
                    }
                    else
                    {
                       while ((*ptr != '\n') && (*ptr != '\0'))
                       {
                          ptr++;
                       }
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used2 & MIRROR_DIR_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, MIRROR_DIR_ID,
                                    MIRROR_DIR_ID_LENGTH) == 0))
                 {
                    used2 |= MIRROR_DIR_FLAG;
                    p_db->special_flag |= MIRROR_DIR;
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used2 & SHOW_ALL_GROUP_MEMBERS_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, SHOW_ALL_GROUP_MEMBERS_ID,
                                    SHOW_ALL_GROUP_MEMBERS_ID_LENGTH) == 0))
                 {
                    used2 |= SHOW_ALL_GROUP_MEMBERS_FLAG;
                    p_db->special_flag |= SHOW_ALL_GROUP_MEMBERS;
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used2 & HIDE_ALL_GROUP_MEMBERS_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, HIDE_ALL_GROUP_MEMBERS_ID,
                                    HIDE_ALL_GROUP_MEMBERS_ID_LENGTH) == 0))
                 {
                    used2 |= HIDE_ALL_GROUP_MEMBERS_FLAG;
                    p_db->special_flag |= HIDE_ALL_GROUP_MEMBERS;
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used2 & SHOW_NO_TO_LINE_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, SHOW_NO_TO_LINE_ID,
                                    SHOW_NO_TO_LINE_ID_LENGTH) == 0))
                 {
                    used2 |= SHOW_NO_TO_LINE_FLAG;
                    p_db->special_flag |= SMTP_GROUP_NO_TO_LINE;
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used2 & CHECK_REMOTE_SIZE_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, MATCH_REMOTE_SIZE_ID,
                                    MATCH_REMOTE_SIZE_ID_LENGTH) == 0))
                 {
                    used2 |= CHECK_REMOTE_SIZE_FLAG;
                    p_db->special_flag |= MATCH_REMOTE_SIZE;
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used2 & SILENT_DEF_NO_LOCK_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, SILENT_NOT_LOCKED_FILE_ID,
                                    SILENT_NOT_LOCKED_FILE_ID_LENGTH) == 0))
                 {
                    used2 |= SILENT_DEF_NO_LOCK_FLAG;
                    p_db->special_flag |= SILENT_NOT_LOCKED_FILE;
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & OUTPUT_LOG_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, OUTPUT_LOG_ID,
                                    OUTPUT_LOG_ID_LENGTH) == 0))
                 {
                    used |= OUTPUT_LOG_FLAG;
#ifdef _OUTPUT_LOG
                    p_db->output_log = NO;
#endif
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used2 & REMOTE_HARDLINK_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, REMOTE_HARDLINK_ID,
                                    REMOTE_HARDLINK_ID_LENGTH) == 0))
                 {
                    int length = 0,
                        max_length = 0;

                    used2 |= REMOTE_HARDLINK_FLAG;
                    ptr += REMOTE_HARDLINK_ID_LENGTH;
                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }

                    /*
                     * First determine the number of file names and the
                     * length of the longest file name, so we can allocate
                     * the necessary memory.
                     */
                    p_db->no_of_rhardlinks = 0;
                    end_ptr = ptr;
                    while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                    {
                      if (*end_ptr == ' ')
                      {
                         if ((end_ptr != ptr) && (*(end_ptr - 1) == '\\'))
                         {
                            /* Name with space. */;
                         }
                         else
                         {
                            *end_ptr = '\0';
                            p_db->no_of_rhardlinks++;
                            if (length > max_length)
                            {
                               max_length = length;
                            }
                         }
                      }
                      end_ptr++;
                      length++;
                    }
                    if (length > max_length)
                    {
                       max_length = length;
                    }
                    if (max_length > 0)
                    {
                       int  i, j;
                       char tmp_char = *end_ptr;

                       *end_ptr = '\0';
                       p_db->no_of_rhardlinks++;
                       max_length++;
                       RT_ARRAY(p_db->hardlinks, p_db->no_of_rhardlinks,
                                max_length, char);

                       for (i = 0; i < p_db->no_of_rhardlinks; i++)
                       {
                          j = 0;
                          while ((j < max_length) && (*ptr != '\0'))
                          {
                             if (*ptr == '\\')
                             {
                                ptr++;
                             }
                             else
                             {
                                p_db->hardlinks[i][j] = *ptr;
                                ptr++; j++;
                             }
                          }
                          p_db->hardlinks[i][j] = '\0';
                          ptr++;
                       }
                       *end_ptr = tmp_char;
                    }
                    ptr = end_ptr + 1;
                 }
            else if (((used2 & REMOTE_SYMLINK_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, REMOTE_SYMLINK_ID,
                                    REMOTE_SYMLINK_ID_LENGTH) == 0))
                 {
                    int length = 0,
                        max_length = 0;

                    used2 |= REMOTE_SYMLINK_FLAG;
                    ptr += REMOTE_SYMLINK_ID_LENGTH;
                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }

                    /*
                     * First determine the number of file names and the
                     * length of the longest file name, so we can allocate
                     * the necessary memory.
                     */
                    p_db->no_of_rsymlinks = 0;
                    end_ptr = ptr;
                    while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                    {
                      if (*end_ptr == ' ')
                      {
                         if ((end_ptr != ptr) && (*(end_ptr - 1) == '\\'))
                         {
                            /* Name with space. */;
                         }
                         else
                         {
                            *end_ptr = '\0';
                            p_db->no_of_rsymlinks++;
                            if (length > max_length)
                            {
                               max_length = length;
                            }
                         }
                      }
                      end_ptr++;
                      length++;
                    }
                    if (length > max_length)
                    {
                       max_length = length;
                    }
                    if (max_length > 0)
                    {
                       char tmp_char = *end_ptr;
                       int  i, j;

                       *end_ptr = '\0';
                       p_db->no_of_rsymlinks++;
                       max_length++;
                       RT_ARRAY(p_db->symlinks, p_db->no_of_rsymlinks,
                                max_length, char);

                       for (i = 0; i < p_db->no_of_rsymlinks; i++)
                       {
                          j = 0;
                          while ((j < max_length) && (*ptr != '\0'))
                          {
                             if (*ptr == '\\')
                             {
                                ptr++;
                             }
                             else
                             {
                                p_db->symlinks[i][j] = *ptr;
                                ptr++; j++;
                             }
                          }
                          p_db->symlinks[i][j] = '\0';
                          ptr++;
                       }
                       *end_ptr = tmp_char;
                    }
                    ptr = end_ptr + 1;
                 }
            else if (((used & RESTART_FILE_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, RESTART_FILE_ID,
                                    RESTART_FILE_ID_LENGTH) == 0))
                 {
                    int length = 0,
                        max_length = 0;

                    used |= RESTART_FILE_FLAG;
                    ptr += RESTART_FILE_ID_LENGTH;
                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }

                    /*
                     * First determine the number of file names and the
                     * length of the longest file name, so we can allocate
                     * the necessary memory.
                     */
                    p_db->no_of_restart_files = 0;
                    end_ptr = ptr;
                    while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                    {
                      if (*end_ptr == ' ')
                      {
                         *end_ptr = '\0';
                         p_db->no_of_restart_files++;
                         if (length > max_length)
                         {
                            max_length = length;
                         }
                      }
                      end_ptr++;
                      length++;
                    }
                    if (length > max_length)
                    {
                       max_length = length;
                    }
                    if (max_length > 0)
                    {
                       int  i;
                       char tmp_char = *end_ptr,
                            *tmp_ptr;

                       *end_ptr = '\0';
                       p_db->no_of_restart_files++;
                       max_length++;
                       RT_ARRAY(p_db->restart_file, p_db->no_of_restart_files,
                                max_length + 2, char);
                       /*
                        * NOTE: The + 2 is only for compatibility for
                        *       the older restart option which did
                        *       not store the file date.
                        */

                       for (i = 0; i < p_db->no_of_restart_files; i++)
                       {
                          (void)strcpy(p_db->restart_file[i], ptr);
                          tmp_ptr = p_db->restart_file[i];
                          while ((*tmp_ptr != '|') && (*tmp_ptr != '\0'))
                          {
                             tmp_ptr++;
                          }
                          if (*tmp_ptr == '|')
                          {
                             *tmp_ptr = '\0';
                          }
                          else
                          {
                             /*
                              * This is only for compatibility for
                              * the older restart option which did
                              * not store the file date.
                              */
                             *(tmp_ptr + 1) = '0';
                             *(tmp_ptr + 2) = '\0';
                          }
                          NEXT(ptr);
                       }
                       *end_ptr = tmp_char;
                    }
                    ptr = end_ptr;
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
#ifdef WITH_DUP_CHECK
            else if (((used & DUPCHECK_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, DUPCHECK_ID, DUPCHECK_ID_LENGTH) == 0))
                 {
                    used |= DUPCHECK_FLAG;
                    ptr = eval_dupcheck_options(ptr, &p_db->dup_check_timeout,
                                                &p_db->dup_check_flag, NULL);
                    p_db->crc_id = p_db->id.job;
# ifdef DEBUG_DUP_CHECK
                    system_log(DEBUG_SIGN, __FILE__, __LINE__,
#  if SIZEOF_TIME_T == 4
                               "crc_id=%x timeout=%ld flag=%u",
#  else
                               "crc_id=%x timeout=%lld flag=%u",
#  endif
                               p_db->crc_id,
                               (pri_time_t)p_db->dup_check_timeout,
                               p_db->dup_check_flag);
# endif
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
#endif
            else if (((used & FILE_NAME_IS_HEADER_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, FILE_NAME_IS_HEADER_ID,
                                    FILE_NAME_IS_HEADER_ID_LENGTH) == 0))
                 {
                    used |= FILE_NAME_IS_HEADER_FLAG;
                    p_db->special_flag |= FILE_NAME_IS_HEADER;
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & SEQUENCE_LOCKING_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, SEQUENCE_LOCKING_ID,
                                    SEQUENCE_LOCKING_ID_LENGTH) == 0))
                 {
                    used |= SEQUENCE_LOCKING_FLAG;
                    p_db->special_flag |= SEQUENCE_LOCKING;
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & SUBJECT_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, SUBJECT_ID, SUBJECT_ID_LENGTH) == 0))
                 {
                    used |= SUBJECT_FLAG;
#ifdef _WITH_DE_MAIL_SUPPORT
                    if ((p_db->protocol & SMTP_FLAG) ||
                        (p_db->protocol & DE_MAIL_FLAG))
#else
                    if (p_db->protocol & SMTP_FLAG)
#endif
                    {
                       off_t length = 0;

                       ptr += SUBJECT_ID_LENGTH;
                       while ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       if (*ptr == '"')
                       {
                          char *ptr_start;

                          ptr++;
                          ptr_start = ptr;
#ifdef WITH_ASCII_ONLY_SUBJECT
                          while ((*ptr != '"') && (*ptr != '\n') &&
                                 (*ptr != '\0') && (isascii(*ptr)))
#else
                          while ((*ptr != '"') && (*ptr != '\n') &&
                                 (*ptr != '\0'))
#endif
                          {
                             ptr++;
                          }
                          if (*ptr == '"')
                          {
                             length = ptr - ptr_start + 1;

                             if ((p_db->subject = malloc(length)) != NULL)
                             {
                                (void)memcpy(p_db->subject, ptr_start, length - 1);
                                p_db->subject[length - 1] = '\0';
                                p_db->special_flag |= MAIL_SUBJECT;
                             }
                             else
                             {
                                system_log(WARN_SIGN, __FILE__, __LINE__,
                                           "malloc() error : %s",
                                           strerror(errno));
                             }
                             ptr++;
                          }
                          else
                          {
#ifdef WITH_ASCII_ONLY_SUBJECT
                             if ((*ptr == '\n') || (*ptr == '\0'))
                             {
#endif
                                system_log(WARN_SIGN, __FILE__, __LINE__,
                                           "Subject line not terminated with a \" sign, igoring %s option. #%x",
                                           SUBJECT_ID, p_db->id.job);
#ifdef WITH_ASCII_ONLY_SUBJECT
                             }
                             else
                             {
                                system_log(WARN_SIGN, __FILE__, __LINE__,
                                           "Subject line contains an illegal character (integer value = %d)that does not fit into the 7-bit ASCII character set, igoring %s option. #%x",
                                           (int)((unsigned char)*ptr),
                                           SUBJECT_ID, p_db->id.job);
                                while ((*ptr != '\n') && (*ptr != '\0'))
                                {
                                   ptr++;
                                }
                                while (*ptr == '\n')
                                {
                                   ptr++;
                                }
                             }
#endif
                          }
                       }
                       else if (*ptr == '/')
                            {
                               char *file_name = ptr,
                                    tmp_char;

                               while ((*ptr != '\n') && (*ptr != '\0') &&
                                      (*ptr != ' ') && (*ptr != '\t'))
                               {
                                  if (*ptr == '\\')
                                  {
                                     ptr++;
                                  }
                                  ptr++;
                               }
                               tmp_char = *ptr;
                               *ptr = '\0';
                               if ((length = read_file_no_cr(file_name,
                                                             &p_db->subject,
                                                             NO, __FILE__,
                                                             __LINE__)) != INCORRECT)
                               {
                                  p_db->special_flag |= MAIL_SUBJECT;
                                  if (p_db->subject[length - 1] == '\n')
                                  {
                                     p_db->subject[length - 1] = '\0';
                                  }
                               }
                               *ptr = tmp_char;
                            }

                       /* Check if we need to insert some other info. */
                       if ((length > 0) && (p_db->subject != NULL))
                       {
                          off_t new_length = length;
                          char  *search_ptr = p_db->subject;

                          while (*search_ptr != '\0')
                          {
                             if (*search_ptr == '\\')
                             {
                                search_ptr++;
                             }
                             else
                             {
                                if (*search_ptr == '%')
                                {
                                   search_ptr++;
                                   if (*search_ptr == 't')
                                   {
                                      search_ptr++;
                                      switch (*search_ptr)
                                      {
                                         case 'a': /* short day of the week 'Tue' */
                                         case 'b': /* short month 'Jan' */
                                         case 'j': /* day of year [001,366] */
                                            new_length += 3;
                                            break;
                                         case 'd': /* day of month [01,31] */
                                         case 'M': /* minute [00,59] */
                                         case 'm': /* month [01,12] */
                                         case 'y': /* year 2 chars [01,99] */
                                         case 'H': /* hour [00,23] */
                                         case 'S': /* second [00,59] */
                                            new_length += 2;
                                            break;
                                         case 'Y': /* year 4 chars 1997 */
                                            new_length += 4;
                                            break;
                                         case 'A': /* long day of the week 'Tuesday' */
                                         case 'B': /* month 'January' */
                                         case 'U': /* Unix time. */
                                            new_length += 20;
                                            break;
                                      }
                                   }
                                   /* The %T parameter does not cause any */
                                   /* extra length, so we can ignore it   */
                                   /* here.                               */
                                   else if (*search_ptr == 's')
                                        {
                                           if (p_db->filename_pos_subject == -1)
                                           {
                                              p_db->filename_pos_subject = search_ptr - 1 - p_db->subject;
                                           }
                                           else
                                           {
                                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                                         "It is only possible to place the filename in subject once only. #%x",
                                                         p_db->id.job);
                                           }
                                        }
                                }
                             }
                             search_ptr++;
                          }
                          if (new_length > length)
                          {
                             char *tmp_buffer;

                             if ((tmp_buffer = malloc(new_length)) == NULL)
                             {
                                system_log(WARN_SIGN, __FILE__, __LINE__,
                                           "malloc() error : %s",
                                           strerror(errno));
                             }
                             else
                             {
                                int    number;
                                time_t current_time,
                                       time_buf,
                                       time_modifier = 0;
                                char   *read_ptr = p_db->subject,
                                       time_mod_sign = '+',
                                       *tmp_subject,
                                       *write_ptr = tmp_buffer;

                                current_time = time(NULL);
                                while (*read_ptr != '\0')
                                {
                                   if (*read_ptr == '\\')
                                   {
                                      read_ptr++;
                                      *write_ptr = *read_ptr;
                                      read_ptr++; write_ptr++;
                                   }
                                   else
                                   {
                                      if (*read_ptr == '%')
                                      {
                                         read_ptr++;
                                         if (*read_ptr == 't')
                                         {
                                            time_buf = current_time;
                                            if (time_modifier > 0)
                                            {
                                               switch (time_mod_sign)
                                               {
                                                  case '-' :
                                                     time_buf = time_buf - time_modifier;
                                                     break;
                                                  case '*' :
                                                     time_buf = time_buf * time_modifier;
                                                     break;
                                                  case '/' :
                                                     time_buf = time_buf / time_modifier;
                                                     break;
                                                  case '%' :
                                                     time_buf = time_buf % time_modifier;
                                                     break;
                                                  case '+' :
                                                  default  :
                                                     time_buf = time_buf + time_modifier;
                                                     break;
                                               }
                                            }
                                            read_ptr++;
                                            switch (*read_ptr)
                                            {
                                               case 'a': /* short day of the week 'Tue' */
                                                  number = strftime(write_ptr,
                                                                    new_length - (write_ptr - tmp_buffer),
                                                                    "%a", localtime(&time_buf));
                                                  break;
                                               case 'b': /* short month 'Jan' */
                                                  number = strftime(write_ptr,
                                                                    new_length - (write_ptr - tmp_buffer),
                                                                    "%b", localtime(&time_buf));
                                                  break;
                                               case 'j': /* day of year [001,366] */
                                                  number = strftime(write_ptr,
                                                                    new_length - (write_ptr - tmp_buffer),
                                                                    "%j", localtime(&time_buf));
                                                  break;
                                               case 'i': /* day of month [1,31] */
                                                  number = strftime(write_ptr,
                                                                    new_length - (write_ptr - tmp_buffer),
                                                                    "%d", localtime(&time_buf));
                                                  if (*write_ptr == '0')
                                                  {
                                                     *write_ptr = *(write_ptr + 1);
                                                     *(write_ptr + 1) = '\0';
                                                     number = 1;
                                                  }
                                                  break;
                                               case 'd': /* day of month [01,31] */
                                                  number = strftime(write_ptr,
                                                                    new_length - (write_ptr - tmp_buffer),
                                                                    "%d", localtime(&time_buf));
                                                  break;
                                               case 'M': /* minute [00,59] */
                                                  number = strftime(write_ptr,
                                                                    new_length - (write_ptr - tmp_buffer),
                                                                    "%M", localtime(&time_buf));
                                                  break;
                                               case 'J': /* month [1,12] */
                                                  number = strftime(write_ptr,
                                                                    new_length - (write_ptr - tmp_buffer),
                                                                    "%m", localtime(&time_buf));
                                                  if (*write_ptr == '0')
                                                  {
                                                     *write_ptr = *(write_ptr + 1);
                                                     *(write_ptr + 1) = '\0';
                                                     number = 1;
                                                  }
                                                  break;
                                               case 'm': /* month [01,12] */
                                                  number = strftime(write_ptr,
                                                                    new_length - (write_ptr - tmp_buffer),
                                                                    "%m", localtime(&time_buf));
                                                  break;
                                               case 'R': /* Sunday week number [00,53]. */
                                                  number = strftime(write_ptr,
                                                                    new_length - (write_ptr - tmp_buffer),
                                                                    "%U", localtime(&time_buf));
                                                  break;
                                               case 'w': /* Weekday [0=Sunday,6]. */
                                                  number = strftime(write_ptr,
                                                                    new_length - (write_ptr - tmp_buffer),
                                                                    "%w", localtime(&time_buf));
                                                  break;
                                               case 'W': /* Monday week number [00,53]. */
                                                  number = strftime(write_ptr,
                                                                    new_length - (write_ptr - tmp_buffer),
                                                                    "%W", localtime(&time_buf));
                                                  break;
                                               case 'y': /* year 2 chars [01,99] */
                                                  number = strftime(write_ptr,
                                                                    new_length - (write_ptr - tmp_buffer),
                                                                    "%y", localtime(&time_buf));
                                                  break;
                                               case 'o': /* hour [0,23] */
                                                  number = strftime(write_ptr,
                                                                    new_length - (write_ptr - tmp_buffer),
                                                                    "%H", localtime(&time_buf));
                                                  if (*write_ptr == '0')
                                                  {
                                                     *write_ptr = *(write_ptr + 1);
                                                     *(write_ptr + 1) = '\0';
                                                     number = 1;
                                                  }
                                                  break;
                                               case 'H': /* hour [00,23] */
                                                  number = strftime(write_ptr,
                                                                    new_length - (write_ptr - tmp_buffer),
                                                                    "%H", localtime(&time_buf));
                                                  break;
                                               case 'S': /* second [00,59] */
                                                  number = strftime(write_ptr,
                                                                    new_length - (write_ptr - tmp_buffer),
                                                                    "%S", localtime(&time_buf));
                                                  break;
                                               case 'Y': /* year 4 chars 1997 */
                                                  number = strftime(write_ptr,
                                                                    new_length - (write_ptr - tmp_buffer),
                                                                    "%Y", localtime(&time_buf));
                                                  break;
                                               case 'A': /* long day of the week 'Tuesday' */
                                                  number = strftime(write_ptr,
                                                                    new_length - (write_ptr - tmp_buffer),
                                                                    "%A", localtime(&time_buf));
                                                  break;
                                               case 'B': /* month 'January' */
                                                  number = strftime(write_ptr,
                                                                    new_length - (write_ptr - tmp_buffer),
                                                                    "%B", localtime(&time_buf));
                                                  break;
                                               case 'U': /* Unix time. */
                                                  number = snprintf(write_ptr,
                                                                    new_length - (write_ptr - tmp_buffer),
#if SIZEOF_TIME_T == 4
                                                                    "%ld",
#else
                                                                    "%lld",
#endif
                                                                    (pri_time_t)time_buf);
                                                  break;
                                               default:
                                                  number = 0;
                                                  *write_ptr = '%';
                                                  *(write_ptr + 1) = 't';
                                                  *(write_ptr + 2) = *read_ptr;
                                                  write_ptr += 3;
                                                  break;
                                            }
                                            write_ptr += number;
                                            read_ptr++;
                                         }
                                         else if (*read_ptr == 'T')
                                              {
                                                 int  j = 0,
                                                      time_unit;
                                                 char string[MAX_INT_LENGTH + 1];

                                                 read_ptr++;
                                                 switch (*read_ptr)
                                                 {
                                                    case '+' :
                                                    case '-' :
                                                    case '*' :
                                                    case '/' :
                                                    case '%' :
                                                       time_mod_sign = *read_ptr;
                                                       read_ptr++;
                                                       break;
                                                    default  :
                                                       time_mod_sign = '+';
                                                       break;
                                                 }
                                                 while ((isdigit((int)(*read_ptr))) &&
                                                        (j < MAX_INT_LENGTH))
                                                 {
                                                    string[j++] = *read_ptr++;
                                                 }
                                                 if ((j > 0) &&
                                                     (j < MAX_INT_LENGTH))
                                                 {
                                                    string[j] = '\0';
                                                    time_modifier = atoi(string);
                                                 }
                                                 else
                                                 {
                                                    if (j == MAX_INT_LENGTH)
                                                    {
                                                       while (isdigit((int)(*read_ptr)))
                                                       {
                                                          read_ptr++;
                                                       }
                                                    }
                                                    time_modifier = 0;
                                                 }
                                                 switch (*read_ptr)
                                                 {
                                                    case 'S' : /* Second. */
                                                       time_unit = 1;
                                                       read_ptr++;
                                                       break;
                                                    case 'M' : /* Minute. */
                                                       time_unit = 60;
                                                       read_ptr++;
                                                       break;
                                                    case 'H' : /* Hour. */
                                                       time_unit = 3600;
                                                       read_ptr++;
                                                       break;
                                                    case 'd' : /* Day. */
                                                       time_unit = 86400;
                                                       read_ptr++;
                                                       break;
                                                    default :
                                                       time_unit = 1;
                                                       break;
                                                 }
                                                 if (time_modifier > 0)
                                                 {
                                                    time_modifier = time_modifier * time_unit;
                                                 }
                                              }
                                              else
                                              {
                                                 *write_ptr = '%';
                                                 write_ptr++;
                                              }
                                      }
                                      else
                                      {
                                         *write_ptr = *read_ptr;
                                         read_ptr++; write_ptr++;
                                      }
                                   }
                                }
                                *write_ptr = '\0';
                                if ((tmp_subject = realloc(p_db->subject,
                                                           new_length)) == NULL)
                                {
                                   system_log(WARN_SIGN, __FILE__, __LINE__,
                                              "realloc() error : %s",
                                              strerror(errno));
                                   free(p_db->subject);
                                   p_db->subject = NULL;
                                }
                                else
                                {
                                   p_db->subject = tmp_subject;
                                   (void)memcpy(p_db->subject, tmp_buffer,
                                                new_length);
                                }

                                free(tmp_buffer);
                             }
                          }
                       }

                       /* Check if we there is a rename rule header. */
                       if (p_db->filename_pos_subject != -1)
                       {
                          while ((*ptr == ' ') || (*ptr == '\t'))
                          {
                             ptr++;
                          }
                          if ((*ptr != '\n') && (*ptr != '\0'))
                          {
                             int n = 0;

                             while ((*ptr != '\n') && (*ptr != '\0') &&
                                    (n < MAX_RULE_HEADER_LENGTH))
                             {
                               p_db->subject_rename_rule[n] = *ptr;
                               ptr++; n++;
                             }
                             p_db->subject_rename_rule[n] = '\0';
                             if (n == MAX_RULE_HEADER_LENGTH)
                             {
                                system_log(WARN_SIGN, __FILE__, __LINE__,
                                           "Rule header for subject option %s to long. #%x",
                                           p_db->subject_rename_rule,
                                           p_db->id.job);
                             }
                          }
                       }
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & FORCE_COPY_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, FORCE_COPY_ID,
                                    FORCE_COPY_ID_LENGTH) == 0))
                 {
                    used |= FORCE_COPY_FLAG;
                    if (p_db->protocol & LOC_FLAG)
                    {
                       p_db->special_flag |= FORCE_COPY;
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & FILE_NAME_IS_SUBJECT_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, FILE_NAME_IS_SUBJECT_ID,
                                    FILE_NAME_IS_SUBJECT_ID_LENGTH) == 0))
                 {
                    used |= FILE_NAME_IS_SUBJECT_FLAG;
#ifdef _WITH_DE_MAIL_SUPPORT
                    if ((p_db->protocol & SMTP_FLAG) ||
                        (p_db->protocol & DE_MAIL_FLAG))
#else
                    if (p_db->protocol & SMTP_FLAG)
#endif
                    {
                       p_db->special_flag |= FILE_NAME_IS_SUBJECT;
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & REPLY_TO_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, REPLY_TO_ID, REPLY_TO_ID_LENGTH) == 0))
                 {
                    used |= REPLY_TO_FLAG;
#ifdef _WITH_DE_MAIL_SUPPORT
                    if ((p_db->protocol & SMTP_FLAG) ||
                        (p_db->protocol & DE_MAIL_FLAG))
#else
                    if (p_db->protocol & SMTP_FLAG)
#endif
                    {
                       ptr += REPLY_TO_ID_LENGTH;
                       while ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       ptr = store_mail_address(ptr, &p_db->reply_to,
                                                REPLY_TO_ID, p_db->id.job);
                    }
                    else
                    {
                       while ((*ptr != '\n') && (*ptr != '\0'))
                       {
                          ptr++;
                       }
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used2 & GROUP_TO_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, GROUP_TO_ID, GROUP_TO_ID_LENGTH) == 0))
                 {
                    used2 |= GROUP_TO_FLAG;
#ifdef _WITH_DE_MAIL_SUPPORT
                    if ((p_db->protocol & SMTP_FLAG) ||
                        (p_db->protocol & DE_MAIL_FLAG))
#else
                    if (p_db->protocol & SMTP_FLAG)
#endif
                    {
                       ptr += GROUP_TO_ID_LENGTH;
                       while ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       ptr = store_mail_address(ptr, &p_db->group_to,
                                                GROUP_TO_ID, p_db->id.job);
                    }
                    else
                    {
                       while ((*ptr != '\n') && (*ptr != '\0'))
                       {
                          ptr++;
                       }
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
#ifdef _WITH_DE_MAIL_SUPPORT
            else if (((used & CONF_OF_RETRIEVE_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, CONF_OF_RETRIEVE_ID,
                                    CONF_OF_RETRIEVE_ID_LENGTH) == 0))
                 {
                    used |= CONF_OF_RETRIEVE_FLAG;
                    if (p_db->protocol & DE_MAIL_FLAG)
                    {
                       p_db->de_mail_options |= CONF_OF_RETRIEVE;
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
#endif
            else if (((used & FROM_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, FROM_ID, FROM_ID_LENGTH) == 0))
                 {
                    used |= FROM_FLAG;
#ifdef _WITH_DE_MAIL_SUPPORT
                    if ((p_db->protocol & SMTP_FLAG) ||
                        (p_db->protocol & DE_MAIL_FLAG))
#else
                    if (p_db->protocol & SMTP_FLAG)
#endif
                    {
                       ptr += FROM_ID_LENGTH;
                       while ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       ptr = store_mail_address(ptr, &p_db->from, FROM_ID,
                                                p_db->id.job);
                    }
                    else
                    {
                       while ((*ptr != '\n') && (*ptr != '\0'))
                       {
                          ptr++;
                       }
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & CHECK_ANSI_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, ENCODE_ANSI_ID,
                                    ENCODE_ANSI_ID_LENGTH) == 0))
                 {
                    used |= CHECK_ANSI_FLAG;
#ifdef _WITH_DE_MAIL_SUPPORT
                    if ((p_db->protocol & SMTP_FLAG) ||
                        (p_db->protocol & DE_MAIL_FLAG))
#else
                    if (p_db->protocol & SMTP_FLAG)
#endif
                    {
                       p_db->special_flag |= ENCODE_ANSI;
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
#ifdef _WITH_WMO_SUPPORT
            else if (((used & CHECK_REPLY_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, CHECK_REPLY_ID,
                                    CHECK_REPLY_ID_LENGTH) == 0))
                 {
                    used |= CHECK_REPLY_FLAG;
                    if (p_db->protocol & WMO_FLAG)
                    {
                       p_db->special_flag |= WMO_CHECK_ACKNOWLEDGE;
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & WITH_SEQUENCE_NUMBER_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, WITH_SEQUENCE_NUMBER_ID,
                                    WITH_SEQUENCE_NUMBER_ID_LENGTH) == 0))
                 {
                    used |= WITH_SEQUENCE_NUMBER_FLAG;
                    if (p_db->protocol & WMO_FLAG)
                    {
                       p_db->special_flag |= WITH_SEQUENCE_NUMBER;
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
#endif /* _WITH_WMO_SUPPORT */
            else if (((used & ATTACH_FILE_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, ATTACH_FILE_ID,
                                    ATTACH_FILE_ID_LENGTH) == 0))
                 {
                    used |= ATTACH_FILE_FLAG;
                    ptr += ATTACH_FILE_ID_LENGTH;
#ifdef _WITH_DE_MAIL_SUPPORT
                    if ((p_db->protocol & SMTP_FLAG) ||
                        (p_db->protocol & DE_MAIL_FLAG))
#else
                    if (p_db->protocol & SMTP_FLAG)
#endif
                    {
                       p_db->special_flag |= ATTACH_FILE;

                       while ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       if ((*ptr != '\n') && (*ptr != '\0'))
                       {
                          int n = 0;

                          while ((*ptr != '\n') && (*ptr != '\0') &&
                                 (n < MAX_RULE_HEADER_LENGTH))
                          {
                            p_db->trans_rename_rule[n] = *ptr;
                            ptr++; n++;
                          }
                          p_db->trans_rename_rule[n] = '\0';
                          if (n == MAX_RULE_HEADER_LENGTH)
                          {
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        "Rule header for trans_rename option %s to long. #%x",
                                        p_db->trans_rename_rule, p_db->id.job);
                             p_db->trans_rename_rule[0] = '\0';
                          }
                          else if (n == 0)
                               {
                                  system_log(WARN_SIGN, __FILE__, __LINE__,
                                             "No rule header specified in message %x.",
                                             p_db->id.job);
                               }
                       }
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & ADD_MAIL_HEADER_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, ADD_MAIL_HEADER_ID,
                                    ADD_MAIL_HEADER_ID_LENGTH) == 0))
                 {
#ifdef _WITH_DE_MAIL_SUPPORT
                    if ((p_db->protocol & SMTP_FLAG) ||
                        (p_db->protocol & DE_MAIL_FLAG))
#else
                    if (p_db->protocol & SMTP_FLAG)
#endif
                    {
                       int length = 0;

                       used |= ADD_MAIL_HEADER_FLAG;
                       p_db->special_flag |= ADD_MAIL_HEADER;
                       ptr += ADD_MAIL_HEADER_ID_LENGTH;
                       while ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       while (*ptr == '"')
                       {
                          ptr++;
                       }
                       end_ptr = ptr;
                       while ((*end_ptr != '\n') && (*end_ptr != '\0') &&
                              (*end_ptr != '"'))
                       {
                         end_ptr++;
                         length++;
                       }
                       if (length > 0)
                       {
                          byte_buf = *end_ptr;
                          *end_ptr = '\0';
                          if ((p_db->special_ptr = malloc(length + 1)) == NULL)
                          {
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        "Failed to malloc() memory, will ignore mail header file : %s",
                                        strerror(errno));
                          }
                          else
                          {
                             (void)memcpy(p_db->special_ptr, ptr, length);
                             *(p_db->special_ptr + length) = '\0';
                          }
                          *end_ptr = byte_buf;
                       }
                       ptr = end_ptr;
                    }
                    else
                    {
                       while ((*ptr != '\n') && (*ptr != '\0'))
                       {
                          ptr++;
                       }
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & FTP_EXEC_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, FTP_EXEC_CMD,
                                    FTP_EXEC_CMD_LENGTH) == 0))
                 {
                    used |= FTP_EXEC_FLAG;
                    if (p_db->protocol & FTP_FLAG)
                    {
                       int length = 0;

                       ptr += FTP_EXEC_CMD_LENGTH;
                       while ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       end_ptr = ptr;
                       while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                       {
                         end_ptr++;
                         length++;
                       }
                       if (length > 0)
                       {
                          p_db->special_flag |= EXEC_FTP;
                          byte_buf = *end_ptr;
                          *end_ptr = '\0';
                          if ((p_db->special_ptr = malloc(length + 1)) == NULL)
                          {
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        "Failed to malloc() memory, will ignore %s option : %s",
                                        FTP_EXEC_CMD, strerror(errno));
                          }
                          else
                          {
                             (void)strcpy(p_db->special_ptr, ptr);
                          }
                          *end_ptr = byte_buf;
                       }
                       ptr = end_ptr;
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used2 & LOGIN_SITE_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, LOGIN_SITE_CMD,
                                    LOGIN_SITE_CMD_LENGTH) == 0))
                 {
                    used2 |= LOGIN_SITE_FLAG;
                    if (p_db->protocol & FTP_FLAG)
                    {
                       int length = 0;

                       ptr += LOGIN_SITE_CMD_LENGTH;
                       while ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       end_ptr = ptr;
                       while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                       {
                         end_ptr++;
                         length++;
                       }
                       if (length > 0)
                       {
                          p_db->special_flag |= LOGIN_EXEC_FTP;
                          byte_buf = *end_ptr;
                          *end_ptr = '\0';
                          if ((p_db->special_ptr = malloc(length + 1)) == NULL)
                          {
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        "Failed to malloc() memory, will ignore %s option : %s",
                                        LOGIN_SITE_CMD, strerror(errno));
                          }
                          else
                          {
                             (void)strcpy(p_db->special_ptr, ptr);
                          }
                          *end_ptr = byte_buf;
                       }
                       ptr = end_ptr;
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & CHARSET_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, CHARSET_ID, CHARSET_ID_LENGTH) == 0))
                 {
                    used |= CHARSET_FLAG;
#ifdef _WITH_DE_MAIL_SUPPORT
                    if ((p_db->protocol & SMTP_FLAG) ||
                        (p_db->protocol & DE_MAIL_FLAG))
#else
                    if (p_db->protocol & SMTP_FLAG)
#endif
                    {
                       size_t length = 0;

                       ptr += CHARSET_ID_LENGTH;
                       while ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       end_ptr = ptr;
                       while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                       {
                         end_ptr++;
                         length++;
                       }
                       if ((p_db->charset = malloc(length + 1)) == NULL)
                       {
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     "Failed to malloc() memory, will ignore charset option : %s #%x",
                                     strerror(errno), p_db->id.job);
                       }
                       else
                       {
                          (void)memcpy(p_db->charset, ptr, length);
                          p_db->charset[length] = '\0';
                       }
                       ptr = end_ptr;
                    }
                    else
                    {
                       while ((*ptr != '\n') && (*ptr != '\0'))
                       {
                          ptr++;
                       }
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & FILE_NAME_IS_USER_FLAG) == 0) &&
                     ((used2 & FILE_NAME_IS_TARGET_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, FILE_NAME_IS_USER_ID,
                                    FILE_NAME_IS_USER_ID_LENGTH) == 0))
                 {
                    used |= FILE_NAME_IS_USER_FLAG;
#ifdef _WITH_DE_MAIL_SUPPORT
                    if ((p_db->protocol & SMTP_FLAG) ||
                        (p_db->protocol & DE_MAIL_FLAG))
#else
                    if (p_db->protocol & SMTP_FLAG)
#endif
                    {
                       p_db->special_flag |= FILE_NAME_IS_USER;
                       ptr += FILE_NAME_IS_USER_ID_LENGTH;
                       while ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       end_ptr = ptr;
                       while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                       {
                         end_ptr++;
                       }
                       byte_buf = *end_ptr;
                       *end_ptr = '\0';
                       (void)strcpy(p_db->user_rename_rule, ptr);
                       *end_ptr = byte_buf;
                       ptr = end_ptr;
                    }
                    else
                    {
                       while ((*ptr != '\n') && (*ptr != '\0'))
                       {
                          ptr++;
                       }
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used2 & FILE_NAME_IS_TARGET_FLAG) == 0) &&
                     ((used & FILE_NAME_IS_USER_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, FILE_NAME_IS_TARGET_ID,
                                    FILE_NAME_IS_TARGET_ID_LENGTH) == 0))
                 {
                    used2 |= FILE_NAME_IS_TARGET_FLAG;
#ifdef _WITH_DE_MAIL_SUPPORT
                    if ((p_db->protocol & SMTP_FLAG) ||
                        (p_db->protocol & DE_MAIL_FLAG))
#else
                    if (p_db->protocol & SMTP_FLAG)
#endif
                    {
                       p_db->special_flag |= FILE_NAME_IS_TARGET;
                       ptr += FILE_NAME_IS_TARGET_ID_LENGTH;
                       while ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       end_ptr = ptr;
                       while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                       {
                         end_ptr++;
                       }
                       byte_buf = *end_ptr;
                       *end_ptr = '\0';
                       (void)strcpy(p_db->user_rename_rule, ptr);
                       *end_ptr = byte_buf;
                       ptr = end_ptr;
                    }
                    else
                    {
                       while ((*ptr != '\n') && (*ptr != '\0'))
                       {
                          ptr++;
                       }
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
#ifdef WITH_EUMETSAT_HEADERS
            else if (((used & EUMETSAT_HEADER_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, EUMETSAT_HEADER_ID,
                                    EUMETSAT_HEADER_ID_LENGTH) == 0))
                 {
                    int length;

                    used |= EUMETSAT_HEADER_FLAG;
                    ptr += EUMETSAT_HEADER_ID_LENGTH;
                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }
                    if ((p_db->special_ptr = malloc(4 + 1)) == NULL)
                    {
                       system_log(WARN_SIGN, __FILE__, __LINE__,
                                  "Failed to malloc() memory, will ignore option %s : %s #%x",
                                  EUMETSAT_HEADER_ID, strerror(errno),
                                  p_db->id.job);
                    }
                    else
                    {
                       char str_num[5];

                       while ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       length = 0;
                       while ((*ptr != ' ') && (*ptr != '\t') &&
                              (length < 4) && (*ptr != '\0') &&
                              (*ptr != '\n'))
                       {
                         str_num[length] = *ptr;
                         ptr++; length++;
                       }
                       if ((length == 0) || (length == 4) ||
                           (*ptr == '\0'))
                       {
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     "Missing/incorrect DestEnvId. Ignoring option %s. #%x",
                                     EUMETSAT_HEADER_ID, p_db->id.job);
                          free(p_db->special_ptr);
                          p_db->special_ptr = NULL;
                       }
                       else
                       {
                          int number;

                          str_num[length] = '\0';
                          number = atoi(str_num);
                          if (number > 255)
                          {
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        "DestEnvId to large (%d). Ignoring option %s. #%x",
                                        number, EUMETSAT_HEADER_ID,
                                        p_db->id.job);
                             free(p_db->special_ptr);
                             p_db->special_ptr = NULL;
                          }
                          else
                          {
                             char local_host[256];

                             /*
                              * CPU ID is IP number of current host.
                              */
                             if (gethostname(local_host, 255) == -1)
                             {
                                system_log(WARN_SIGN, __FILE__, __LINE__,
                                           "Failed to gethostname() : %s",
                                           strerror(errno));
                             }
                             else
                             {
                                register struct hostent *p_host;

                                if ((p_host = gethostbyname(local_host)) == NULL)
                                {
                                   system_log(WARN_SIGN, __FILE__, __LINE__,
                                              "Failed to gethostbyname() of local host : %s",
                                              strerror(errno));
                                }
                                else
                                {
                                   unsigned char *ip_ptr = (unsigned char *)p_host->h_addr;

                                   p_db->special_ptr[0] = *ip_ptr;
                                   p_db->special_ptr[1] = *(ip_ptr + 1);
                                   p_db->special_ptr[2] = *(ip_ptr + 2);
                                   p_db->special_ptr[3] = *(ip_ptr + 3);

                                   p_db->special_ptr[4] = (unsigned char)number;
                                   p_db->special_flag |= ADD_EUMETSAT_HEADER;
                                }
                             }
                          }
                       }
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
#endif /* WITH_EUMETSAT_HEADERS */
            else if (((used & RENAME_FILE_BUSY_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, RENAME_FILE_BUSY_ID,
                                    RENAME_FILE_BUSY_ID_LENGTH) == 0))
                 {
                    used |= RENAME_FILE_BUSY_FLAG;
                    ptr += RENAME_FILE_BUSY_ID_LENGTH;
                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }
                    if (isascii(*ptr))
                    {
                       p_db->rename_file_busy = *ptr;
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & CHANGE_FTP_MODE_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, PASSIVE_FTP_MODE,
                                    PASSIVE_FTP_MODE_LENGTH) == 0))
                 {
                    used |= CHANGE_FTP_MODE_FLAG;
                    if (p_db->protocol & FTP_FLAG)
                    {
                       p_db->mode_flag = PASSIVE_MODE;
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & CHANGE_FTP_MODE_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, ACTIVE_FTP_MODE,
                                    ACTIVE_FTP_MODE_LENGTH) == 0))
                 {
                    used |= CHANGE_FTP_MODE_FLAG;
                    if (p_db->protocol & FTP_FLAG)
                    {
                       p_db->mode_flag = ACTIVE_MODE;
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & ATTACH_ALL_FILES_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, ATTACH_ALL_FILES_ID,
                                    ATTACH_ALL_FILES_ID_LENGTH) == 0))
                 {
                    used |= ATTACH_ALL_FILES_FLAG;
                    used |= ATTACH_FILE_FLAG;
                    ptr += ATTACH_ALL_FILES_ID_LENGTH;
#ifdef _WITH_DE_MAIL_SUPPORT
                    if ((p_db->protocol & SMTP_FLAG) ||
                        (p_db->protocol & DE_MAIL_FLAG))
#else
                    if (p_db->protocol & SMTP_FLAG)
#endif
                    {
                       p_db->special_flag |= ATTACH_FILE;
                       p_db->special_flag |= ATTACH_ALL_FILES;

                       while ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       if ((*ptr != '\n') && (*ptr != '\0'))
                       {
                          int n = 0;

                          while ((*ptr != '\n') && (*ptr != '\0') &&
                                 (n < MAX_RULE_HEADER_LENGTH))
                          {
                            p_db->trans_rename_rule[n] = *ptr;
                            ptr++; n++;
                          }
                          p_db->trans_rename_rule[n] = '\0';
                          if (n == MAX_RULE_HEADER_LENGTH)
                          {
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        "Rule header for trans_rename option %s to long. #%x",
                                        p_db->trans_rename_rule, p_db->id.job);
                             p_db->trans_rename_rule[0] = '\0';
                          }
                          else if (n == 0)
                               {
                                  system_log(WARN_SIGN, __FILE__, __LINE__,
                                             "No rule header specified in message %x.",
                                             p_db->id.job);
                               }
                       }
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
#ifdef _WITH_TRANS_EXEC
            else if (((used & TRANS_EXEC_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, TRANS_EXEC_ID,
                                    TRANS_EXEC_ID_LENGTH) == 0))
                 {
                    int exec_timeout_set = NO,
                        length = 0;

                    used |= TRANS_EXEC_FLAG;
                    ptr += TRANS_EXEC_ID_LENGTH;
                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }

                    while (*ptr == '-')
                    {
                       ptr++;
                       switch (*ptr)
                       {
                          case 't' : /* Timeup pexec command. */
                             {
                                int  i;
                                char str_number[MAX_INT_LENGTH];

                                ptr++;
                                while ((*ptr == ' ') || (*ptr == '\t'))
                                {
                                   ptr++;
                                }
                                i = 0;
                                while ((isdigit((int)(*ptr))) &&
                                       (i < MAX_INT_LENGTH))
                                {
                                   str_number[i] = *ptr;
                                   i++; ptr++;
                                }
                                if (i > 0)
                                {
                                   if (i < MAX_INT_LENGTH)
                                   {
                                      str_number[i] = '\0';
                                      p_db->trans_exec_timeout = str2timet(str_number,
                                                                           (char **)NULL, 10);
                                      exec_timeout_set = YES;
                                      while ((*ptr == ' ') || (*ptr == '\t'))
                                      {
                                         ptr++;
                                      }
                                   }
                                   else
                                   {
                                      while ((*ptr != ' ') && (*ptr != '\t') &&
                                             (*ptr != '\n') && (*ptr != '\0'))
                                      {
                                         ptr++;
                                      }
                                      system_log(WARN_SIGN, __FILE__, __LINE__,
                                                 "pexec timeout value to long in message %s. #%x",
                                                 message_name, p_db->id.job);
                                   }
                                }
                             }
                             break;

                          case 'l':
                          case 'L': /* Set lock so this can only run once. */
                             ptr++;
                             while ((*ptr == ' ') || (*ptr == '\t'))
                             {
                                ptr++;
                             }
                             p_db->set_trans_exec_lock = YES;
                             break;

                          case 'r': /* Execute in target directory. */
                             ptr++;
                             while ((*ptr == ' ') || (*ptr == '\t'))
                             {
                                ptr++;
                             }
                             p_db->special_flag |= EXECUTE_IN_TARGET_DIR;
                             break;

                          default: /* Unknown, lets ignore this. Do as if we */
                                   /* did not know about any options.        */
                             ptr++;
                             while ((*ptr == ' ') || (*ptr == '\t'))
                             {
                                ptr++;
                             }
                             break;
                       }
                    }

                    end_ptr = ptr;
                    while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                    {
                      end_ptr++;
                      length++;
                    }
                    if (length > 0)
                    {
                       p_db->special_flag |= TRANS_EXEC;
                       byte_buf = *end_ptr;
                       *end_ptr = '\0';
                       if ((p_db->trans_exec_cmd = malloc(length + 1)) == NULL)
                       {
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     "Failed to malloc() memory, will ignore %s option : %s #%x",
                                     TRANS_EXEC_ID, strerror(errno),
                                     p_db->id.job);
                       }
                       else
                       {
                          (void)strcpy(p_db->trans_exec_cmd, ptr);
                          if (exec_timeout_set == NO)
                          {
                             char *buffer,
                                  config_file[MAX_PATH_LENGTH];

                             (void)snprintf(config_file, MAX_PATH_LENGTH,
                                            "%s%s%s", p_work_dir, ETC_DIR,
                                            AFD_CONFIG_FILE);
                             if ((eaccess(config_file, F_OK) == 0) &&
                                 (read_file_no_cr(config_file, &buffer, YES,
                                                  __FILE__, __LINE__) != INCORRECT))
                             {
                                char value[MAX_INT_LENGTH];

                                if (get_definition(buffer, EXEC_TIMEOUT_DEF,
                                                   value, MAX_INT_LENGTH) != NULL)
                                {
                                   p_db->trans_exec_timeout = atol(value);
                                }
                                else
                                {
                                   p_db->trans_exec_timeout = DEFAULT_EXEC_TIMEOUT;
                                }
                                free(buffer);
                             }
                             else
                             {
                                p_db->trans_exec_timeout = DEFAULT_EXEC_TIMEOUT;
                             }
                          }
# ifdef HAVE_SETPRIORITY
                          errno = 0;
                          if (((p_db->current_priority = getpriority(PRIO_PROCESS, 0)) == -1) &&
                              (errno != 0))
                          {
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        "Failed to getpriority() : %s", strerror(errno));
                             p_db->current_priority = 0;
                          }
                          else
                          {
                             char         config_file[MAX_PATH_LENGTH];
#  ifdef HAVE_STATX
                             struct statx stat_buf;
#  else
                             struct stat  stat_buf;
#  endif

                             (void)snprintf(config_file, MAX_PATH_LENGTH,
                                            "%s%s%s", p_work_dir, ETC_DIR,
                                            AFD_CONFIG_FILE);
                             if ((eaccess(config_file, F_OK) == 0) &&
#  ifdef HAVE_STATX
                                 (statx(0, config_file, AT_STATX_SYNC_AS_STAT,
                                        STATX_MTIME, &stat_buf) == 0)
#  else
                                 (stat(config_file, &stat_buf) == 0)
#  endif
                                )
                             {
#  ifdef HAVE_STATX
                                if (stat_buf.stx_mtime.tv_sec != p_db->afd_config_mtime)
#  else
                                if (stat_buf.st_mtime != p_db->afd_config_mtime)
#  endif
                                {
                                   char *buffer = NULL;

                                   if (read_file_no_cr(config_file, &buffer, YES,
                                                       __FILE__, __LINE__) != INCORRECT)
                                   {
                                      char value[MAX_INT_LENGTH + 1];

#  ifdef HAVE_STATX
                                      p_db->afd_config_mtime = stat_buf.stx_mtime.tv_sec;
#  else
                                      p_db->afd_config_mtime = stat_buf.st_mtime;
#  endif
                                      if (get_definition(buffer, EXEC_BASE_PRIORITY_DEF,
                                                         value, MAX_INT_LENGTH) != NULL)
                                      {
                                         p_db->exec_base_priority = atoi(value);
                                      }
                                      else
                                      {
                                         p_db->exec_base_priority = NO_PRIORITY;
                                      }
                                      if (get_definition(buffer, ADD_AFD_PRIORITY_DEF,
                                                         value, MAX_INT_LENGTH) != NULL)
                                      {
                                         if (((value[0] == 'n') || (value[0] == 'N')) &&
                                             ((value[1] == 'o') || (value[1] == 'O')) &&
                                             ((value[2] == '\0') || (value[2] == ' ') ||
                                              (value[2] == '\t')))
                                         {
                                            p_db->add_afd_priority = NO;
                                         }
                                         else if (((value[0] == 'y') || (value[0] == 'Y')) &&
                                                  ((value[1] == 'e') || (value[1] == 'E')) &&
                                                  ((value[2] == 's') || (value[2] == 'S')) &&
                                                  ((value[3] == '\0') ||
                                                   (value[3] == ' ') ||
                                                   (value[3] == '\t')))
                                              {
                                                 p_db->add_afd_priority = YES;
                                              }
                                              else
                                              {
                                                 p_db->add_afd_priority = DEFAULT_ADD_AFD_PRIORITY_DEF;
                                                 system_log(WARN_SIGN, __FILE__, __LINE__,
                                                            "Only YES or NO (and not `%s') are possible for %s in AFD_CONFIG. Setting to default: %s",
                                                            value, ADD_AFD_PRIORITY_DEF,
                                                            (p_db->add_afd_priority == YES) ? "YES" : "NO");
                                              }
                                      }
                                      else
                                      {
                                         p_db->add_afd_priority = DEFAULT_ADD_AFD_PRIORITY_DEF;
                                      }
                                      if (get_definition(buffer, MAX_NICE_VALUE_DEF,
                                                         value, MAX_INT_LENGTH) != NULL)
                                      {
                                         p_db->max_sched_priority = atoi(value);
                                      }
                                      else
                                      {
                                         p_db->max_sched_priority = DEFAULT_MAX_NICE_VALUE;
                                      }
                                      if (get_definition(buffer, MIN_NICE_VALUE_DEF,
                                                         value, MAX_INT_LENGTH) != NULL)
                                      {
                                         p_db->min_sched_priority = atoi(value);
                                      }
                                      else
                                      {
                                         p_db->min_sched_priority = DEFAULT_MIN_NICE_VALUE;
                                      }
                                      free(buffer);
                                   }
                                   else
                                   {
                                      p_db->exec_base_priority = NO_PRIORITY;
                                      p_db->add_afd_priority = DEFAULT_ADD_AFD_PRIORITY_DEF;
                                      p_db->max_sched_priority = DEFAULT_MAX_NICE_VALUE;
                                      p_db->min_sched_priority = DEFAULT_MIN_NICE_VALUE;
                                   }
                                }
                             }
                             else
                             {
                                p_db->exec_base_priority = NO_PRIORITY;
                                p_db->add_afd_priority = DEFAULT_ADD_AFD_PRIORITY_DEF;
                                p_db->max_sched_priority = DEFAULT_MAX_NICE_VALUE;
                                p_db->min_sched_priority = DEFAULT_MIN_NICE_VALUE;
                             }
                          }
# endif /* HAVE_SETPRIORITY */
                       }
                       *end_ptr = byte_buf;
                    }
                    ptr = end_ptr;
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
#endif /* _WITH_TRANS_EXEC */
            else if (((used2 & SOCK_SND_BUF_SIZE_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, SOCKET_SEND_BUFFER_ID,
                                    SOCKET_SEND_BUFFER_ID_LENGTH) == 0))
                 {
                    used2 |= SOCK_SND_BUF_SIZE_FLAG;
                    ptr += SOCKET_SEND_BUFFER_ID_LENGTH;
                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }
                    end_ptr = ptr;
                    while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                    {
                      end_ptr++;
                    }
                    byte_buf = *end_ptr;
                    *end_ptr = '\0';
                    p_db->sndbuf_size = atoi(ptr);
                    *end_ptr = byte_buf;
                    ptr = end_ptr;
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used2 & SOCK_RCV_BUF_SIZE_FLAG) == 0) &&
                     (CHECK_STRNCMP(ptr, SOCKET_RECEIVE_BUFFER_ID,
                                    SOCKET_RECEIVE_BUFFER_ID_LENGTH) == 0))
                 {
                    used2 |= SOCK_RCV_BUF_SIZE_FLAG;
                    ptr += SOCKET_RECEIVE_BUFFER_ID_LENGTH;
                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }
                    end_ptr = ptr;
                    while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                    {
                      end_ptr++;
                    }
                    byte_buf = *end_ptr;
                    *end_ptr = '\0';
                    p_db->rcvbuf_size = atoi(ptr);
                    *end_ptr = byte_buf;
                    ptr = end_ptr;
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (CHECK_STRNCMP(ptr, AGEING_ID, AGEING_ID_LENGTH) == 0)
                 {
                    /* This option is for process FD, so ignore it. */
                    ptr += AGEING_ID_LENGTH;
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
                 else
                 {
                    /* Ignore this option. */
                    end_ptr = ptr;
                    while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                    {
                       end_ptr++;
                    }
                    byte_buf = *end_ptr;
                    *end_ptr = '\0';
                    system_log(WARN_SIGN, __FILE__, __LINE__,
                               "Unknown or duplicate option <%s> in message %s #%x",
                               ptr, message_name, p_db->id.job);
                    *end_ptr = byte_buf;
                    ptr = end_ptr;
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
         } /* while (*ptr != '\0') */
      }
   } /* if (byte_buf != '\0') */

   /* Don't forget to free memory we have allocated. */
   free(msg_buf);

   if (((used & FROM_FLAG) == 0) && (p_db->default_from != NULL))
   {
      size_t length;

      length = strlen(p_db->default_from) + 1;
      free(p_db->from);
      if ((p_db->from = malloc(length + 1)) == NULL)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to malloc() memory, will ignore default from option : %s",
                    strerror(errno));
      }
      else
      {
         (void)memcpy(p_db->from, p_db->default_from, length);
         p_db->from[length] = '\0';
      }
   }

   return(SUCCESS);
}


/*++++++++++++++++++++++++++++ store_mode() +++++++++++++++++++++++++++++*/
static void
store_mode(char *ptr, struct job *p_db, char *option, int type)
{
   if ((p_db->protocol & LOC_FLAG) || (p_db->protocol & SCP_FLAG) ||
       (p_db->protocol & SFTP_FLAG))
   {
      int  n;
      char *end_ptr;

      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }
      end_ptr = ptr;
      while ((*end_ptr != '\n') && (*end_ptr != '\0') &&
             (*end_ptr != ' ') && (*end_ptr != '\t'))
      {
         end_ptr++;
      }
      n = end_ptr - ptr;
      if ((n == 3) || (n == 4))
      {
         int    error_flag = NO;
         mode_t mode;

         mode = 0;
         if (n == 4)
         {
            switch (*ptr)
            {
               case '7' :
                  mode |= S_ISUID | S_ISGID | S_ISVTX;
                  break;
               case '6' :
                  mode |= S_ISUID | S_ISGID;
                  break;
               case '5' :
                  mode |= S_ISUID | S_ISVTX;
                  break;
               case '4' :
                  mode |= S_ISUID;
                  break;
               case '3' :
                  mode |= S_ISGID | S_ISVTX;
                  break;
               case '2' :
                  mode |= S_ISGID;
                  break;
               case '1' :
                  mode |= S_ISVTX;
                  break;
               case '0' : /* Nothing to be done here. */
                  break;
               default :
                  error_flag = YES;
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Incorrect parameter for %s option %c%c%c%c",
                             option, ptr, ptr + 1, ptr + 2, ptr + 3);
                  break;
            }
            ptr++;
         }
         switch (*ptr)
         {
            case '7' :
               mode |= S_IRUSR | S_IWUSR | S_IXUSR;
               break;
            case '6' :
               mode |= S_IRUSR | S_IWUSR;
               break;
            case '5' :
               mode |= S_IRUSR | S_IXUSR;
               break;
            case '4' :
               mode |= S_IRUSR;
               break;
            case '3' :
               mode |= S_IWUSR | S_IXUSR;
               break;
            case '2' :
               mode |= S_IWUSR;
               break;
            case '1' :
               mode |= S_IXUSR;
               break;
            case '0' : /* Nothing to be done here. */
               break;
            default :
               error_flag = YES;
               if (n == 4)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Incorrect parameter for %s option %c%c%c%c",
                             option, ptr - 1, ptr, ptr + 1, ptr + 2);
               }
               else
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Incorrect parameter for %s option %c%c%c",
                             option, ptr, ptr + 1, ptr + 2);
               }
               break;
         }
         ptr++;
         switch (*ptr)
         {
            case '7' :
               mode |= S_IRGRP | S_IWGRP | S_IXGRP;
               break;
            case '6' :
               mode |= S_IRGRP | S_IWGRP;
               break;
            case '5' :
               mode |= S_IRGRP | S_IXGRP;
               break;
            case '4' :
               mode |= S_IRGRP;
               break;
            case '3' :
               mode |= S_IWGRP | S_IXGRP;
               break;
            case '2' :
               mode |= S_IWGRP;
               break;
            case '1' :
               mode |= S_IXGRP;
               break;
            case '0' : /* Nothing to be done here. */
               break;
            default :
               error_flag = YES;
               if (n == 4)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Incorrect parameter for %s option %c%c%c%c",
                             option, ptr - 2, ptr - 1, ptr, ptr + 1);
               }
               else
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Incorrect parameter for %s option %c%c%c",
                             option, ptr - 1, ptr, ptr + 1);
               }
               break;
         }
         ptr++;
         switch (*ptr)
         {
            case '7' :
               mode |= S_IROTH | S_IWOTH | S_IXOTH;
               break;
            case '6' :
               mode |= S_IROTH | S_IWOTH;
               break;
            case '5' :
               mode |= S_IROTH | S_IXOTH;
               break;
            case '4' :
               mode |= S_IROTH;
               break;
            case '3' :
               mode |= S_IWOTH | S_IXOTH;
               break;
            case '2' :
               mode |= S_IWOTH;
               break;
            case '1' :
               mode |= S_IXOTH;
               break;
            case '0' : /* Nothing to be done here. */
               break;
            default :
               error_flag = YES;
               if (n == 4)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Incorrect parameter for %s option %c%c%c%c",
                             option, ptr - 3, ptr - 2, ptr - 1, ptr);
               }
               else
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Incorrect parameter for %s option %c%c%c",
                             option, ptr - 2, ptr - 1, ptr);
               }
               break;
         }
         if (error_flag == NO)
         {
            ptr -= (n - 1);
            if (type == CREATE_TARGET_DIR_FLAG)
            {
               p_db->dir_mode = mode;
               if (p_db->protocol & SFTP_FLAG)
               {
                  p_db->dir_mode_str[0] = *ptr;
                  p_db->dir_mode_str[1] = *(ptr + 1);
                  p_db->dir_mode_str[2] = *(ptr + 2);
                  if (n == 4)
                  {
                     p_db->dir_mode_str[3] = *(ptr + 3);
                     p_db->dir_mode_str[4] = '\0';
                  }
                  else
                  {
                     p_db->dir_mode_str[3] = '\0';
                  }
               }
            }
            else
            {
               p_db->chmod = mode;
               if (p_db->protocol & SFTP_FLAG)
               {
                  p_db->chmod_str[0] = *ptr;
                  p_db->chmod_str[1] = *(ptr + 1);
                  p_db->chmod_str[2] = *(ptr + 2);
                  if (n == 4)
                  {
                     p_db->chmod_str[3] = *(ptr + 3);
                     p_db->chmod_str[4] = '\0';
                  }
                  else
                  {
                     p_db->chmod_str[3] = '\0';
                  }
               }
               p_db->special_flag |= CHANGE_PERMISSION;
            }
         }
         else
         {
            /* Mode could not be determined, so lets ignore it. */
            /* The user got a warning above.                    */
            if (type == CREATE_TARGET_DIR_FLAG)
            {
               p_db->dir_mode = 0;
               p_db->dir_mode_str[0] = '\0';
            }
            else
            {
               p_db->chmod = 0;
               p_db->chmod_str[0] = '\0';
               p_db->special_flag &= ~CHANGE_PERMISSION;
            }
         }
      }
      ptr = end_ptr;
   }
   else if (p_db->protocol & FTP_FLAG)
        {
           int  n;
           char *p_mode;

           if (type == CREATE_TARGET_DIR_FLAG)
           {
              p_mode = p_db->dir_mode_str;
           }
           else
           {
              p_mode = p_db->chmod_str;
           }

           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           n = 0;
           while ((*ptr != '\n') && (*ptr != '\0') &&
                  (n < 4) && (isdigit((int)(*ptr))))
           {
              p_mode[n] = *ptr;
              ptr++; n++;
           }
           if (n > 1)
           {
              p_mode[n] = '\0';
           }
           else
           {
              system_log(WARN_SIGN, __FILE__, __LINE__,
                         "Incorrect parameter for %s option, ignoring it.",
                         option);
              p_mode[0] = '\0';
           }
        }

   return;
}


/*++++++++++++++++++++++++ store_mail_address() +++++++++++++++++++++++++*/
static char *
store_mail_address(char         *ptr,
                   char         **mail_address,
                   char         *option,
                   unsigned int job_id)
{
   size_t length = 0;
   char   buffer[256];

   while ((length < 255) && (*ptr != '\n') && (*ptr != '\0'))
   {
      if ((*ptr == '%') && ((length == 0) || (*(ptr - 1) != '\\')) &&
          ((*(ptr + 1) == 'H') || (*(ptr + 1) == 'h')))
      {
         char hostname[40];

         if (gethostname(hostname, 40) == -1)
         {
            char *p_hostname;

            if ((p_hostname = getenv("HOSTNAME")) != NULL)
            {
               int i;

               (void)my_strncpy(hostname, p_hostname, 40);
               if (*(ptr + 1) == 'H')
               {
                  i = 0;
                  while ((hostname[i] != '\0') && (hostname[i] != '.'))
                  {
                     i++;
                  }
                  if (hostname[i] == '.')
                  {
                     hostname[i] = '\0';
                  }
               }
               else
               {
                  i = strlen(hostname);
               }
               if ((length + i + 1) > 255)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Storage for storing hostname in mail address not large enough (%d > %d). #%x",
                             (length + i + 1), 255, job_id);
                  buffer[length] = '%';
                  buffer[length + 1] = *(ptr + 1);
                  length += 2;
               }
               else
               {
                  (void)strcpy(&buffer[length], hostname);
                  length += i;
               }
            }
            else
            {
               buffer[length] = '%';
               buffer[length + 1] = *(ptr + 1);
               length += 2;
            }
         }
         else
         {
            int i;

            if (*(ptr + 1) == 'H')
            {
               i = 0;
               while ((hostname[i] != '\0') && (hostname[i] != '.'))
               {
                  i++;
               }
               if (hostname[i] == '.')
               {
                  hostname[i] = '\0';
               }
            }
            else
            {
               i = strlen(hostname);
            }
            if ((length + i + 1) > 255)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Storage for storing hostname in mail address not large enough (%d > %d). #%x",
                          (length + i + 1), 255, job_id);
               buffer[length] = '%';
               buffer[length + 1] = *(ptr + 1);
               length += 2;
            }
            else
            {
               (void)strcpy(&buffer[length], hostname);
               length += i;
            }
         }
         ptr += 2;
      }
      else
      {
         buffer[length] = *ptr;
         ptr++; length++;
      }
   }

   /* Discard global value from AFD_CONFIG when set. */
   if (*mail_address != NULL)
   {
      free(*mail_address);
      *mail_address = NULL;
   }
   if ((*mail_address = malloc(length + 1)) == NULL)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() memory, will ignore %s option : %s",
                 option, strerror(errno));
   }
   else
   {
      (void)memcpy(*mail_address, buffer, length);
      (*mail_address)[length] = '\0';
   }

   return(ptr);
}
