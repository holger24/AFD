/*
 *  get_job_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_job_data - appends a message to the msg_cache_buf structure
 **
 ** SYNOPSIS
 **   int get_job_data(unsigned int job_id,
 **                    int          mdb_position,
 **                    time_t       msg_mtime,
 **                    off_t        msg_size)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS when it managed to store the message, otherwise INCORRECT
 **   is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   19.01.1998 H.Kiehl Created
 **   31.01.2005 H.Kiehl Store the port as well.
 **   15.04.2008 H.Kiehl Accept url's without @ sign such as http://idefix.
 **   27.06.2023 H.Kiehl Add support for ageing.
 **
 */
DESCR__E_M3

#include <stdio.h>           /* snprintf()                               */
#include <string.h>          /* strcpy(), strcmp(), strerror()           */
#include <stdlib.h>          /* malloc(), free(), atoi()                 */
#include <ctype.h>           /* isdigit()                                */
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>          /* fstat(), read(), close(), unlink()       */
#include <fcntl.h>           /* open()                                   */
#include <errno.h>
#include "fddefs.h"

/* External global variables. */
extern int                        default_ageing,
                                  mdb_fd,
                                  no_of_hosts,
                                  *no_msg_cached;
extern char                       msg_dir[],
                                  *p_msg_dir;
extern struct filetransfer_status *fsa;
extern struct msg_cache_buf       *mdb;


/*########################### get_job_data() ############################*/
int
get_job_data(unsigned int job_id,
             int          mdb_position,
             time_t       msg_mtime,
             off_t        msg_size)
{
   unsigned int error_mask,
                scheme;
   int          fd,
                port,
                pos;
   char         protocol,
                *file_buf,
                *ptr,
                *p_start,
                *tmp_ptr,
                real_hostname[MAX_REAL_HOSTNAME_LENGTH + 1],
                host_alias[MAX_HOSTNAME_LENGTH + 1],
                user[MAX_USER_NAME_LENGTH + 1],
                smtp_server[MAX_REAL_HOSTNAME_LENGTH + 1];

   (void)snprintf(p_msg_dir, MAX_PATH_LENGTH - (p_msg_dir - msg_dir), "%x",
                  job_id);

retry:
   if ((fd = open(msg_dir, O_RDONLY)) == -1)
   {
      if (errno == ENOENT)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Hmmm. No message for job `%x'. Will try recreate it.",
                    job_id);
         if (recreate_msg(job_id) < 0)
         {
            return(INCORRECT);
         }
         goto retry;
      }
      else
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to open() %s : %s",
                    msg_dir, strerror(errno));
         return(INCORRECT);
      }
   }
   if (mdb_position == -1)
   {
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat stat_buf;
#endif

#ifdef HAVE_STATX
      if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE | STATX_MTIME, &stat_buf) == -1)
#else
      if (fstat(fd, &stat_buf) == -1)
#endif
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                    "Failed to statx() %s : %s",
#else
                    "Failed to fstat() %s : %s",
#endif
                    msg_dir, strerror(errno));
         (void)close(fd);
         return(INCORRECT);
      }
#ifdef HAVE_STATX
      msg_size = stat_buf.stx_size;
      msg_mtime = stat_buf.stx_mtime.tv_sec;
#else
      msg_size = stat_buf.st_size;
      msg_mtime = stat_buf.st_mtime;
#endif
   }
   if ((file_buf = malloc(msg_size + 1)) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error [%d bytes] : %s",
                 msg_size + 1, strerror(errno));
      (void)close(fd);
      exit(INCORRECT);
   }
   if (read(fd, file_buf, msg_size) != msg_size)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to read() %s : %s", msg_dir, strerror(errno));
      (void)close(fd);
      free(file_buf);
      return(INCORRECT);
   }
   file_buf[msg_size] = '\0';
   if (close(fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }

   /*
    * First let's evaluate the recipient.
    */
   if ((ptr = lposi(file_buf, DESTINATION_IDENTIFIER,
                    DESTINATION_IDENTIFIER_LENGTH)) == NULL)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Removing %s. It is not a message.", msg_dir);
      if (unlink(msg_dir) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to unlink() %s : %s", msg_dir, strerror(errno));
      }
      free(file_buf);
      if (recreate_msg(job_id) < 0)
      {
         return(INCORRECT);
      }
      goto retry;
   }

   /* Lets determine the recipient. */
   p_start = ptr;
   while ((*ptr != '\n') && (*ptr != '\0'))
   {
      ptr++;
   }
   if (*ptr == '\n')
   {
      *ptr = '\0';
      ptr++;
   }
   if ((error_mask = url_evaluate(p_start, &scheme, user, NULL, NULL,
#ifdef WITH_SSH_FINGERPRINT
                                  NULL, NULL,
#endif
                                  NULL, NO, real_hostname, &port, NULL, NULL,
                                  NULL, NULL, NULL, NULL, NULL, NULL,
                                  smtp_server)) > 3)
   {
      char error_msg[MAX_URL_ERROR_MSG];

      url_get_error(error_mask, error_msg, MAX_URL_ERROR_MSG);
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Removing %s. Could not decode URL `%s' : %s",
                 msg_dir, p_start, error_msg);
      if (unlink(msg_dir) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to unlink() %s : %s", msg_dir, strerror(errno));
      }
      free(file_buf);
      return(INCORRECT);
   }

   if (scheme & FTP_FLAG)
   {
      protocol = FTP;
   }
   else if (scheme & LOC_FLAG)
        {
           protocol = LOC;
        }
   else if (scheme & SMTP_FLAG)
        {
           protocol = SMTP;
        }
   else if (scheme & SFTP_FLAG)
        {
           protocol = SFTP;
        }
   else if (scheme & HTTP_FLAG)
        {
           protocol = HTTP;
        }
   else if (scheme & EXEC_FLAG)
        {
           protocol = EXEC;
        }
#ifdef _WITH_SCP_SUPPORT
   else if (scheme & SCP_FLAG)
        {
           protocol = SCP;
        }
#endif
#ifdef _WITH_WMO_SUPPORT
   else if (scheme & WMO_FLAG)
        {
           protocol = WMO;
        }
#endif
#ifdef _WITH_MAP_SUPPORT
   else if (scheme & MAP_FLAG)
        {
           protocol = MAP;
        }
#endif
#ifdef _WITH_DFAX_SUPPORT
   else if (scheme & DFAX_FLAG)
        {
           protocol = DFAX;
        }
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
   else if (scheme & DE_MAIL_FLAG)
        {
           protocol = DE_MAIL;
        }
#endif
        else
        {
           system_log(WARN_SIGN, __FILE__, __LINE__,
                      "Removing %s because of unknown scheme [%s].",
                      msg_dir, p_start);
           if (unlink(msg_dir) == -1)
           {
              system_log(WARN_SIGN, __FILE__, __LINE__,
                         "Failed to unlink() %s : %s",
                         msg_dir, strerror(errno));
           }
           free(file_buf);
           return(INCORRECT);
        }

   if (user[0] == '\0')
   {
      if (real_hostname[0] == MAIL_GROUP_IDENTIFIER)
      {
         fd = 0;
         while (real_hostname[fd + 1] != '\0')
         {
            real_hostname[fd] = real_hostname[fd + 1];
            fd++;
         }
         real_hostname[fd] = '\0';
      }
   }
#ifdef _WITH_DE_MAIL_SUPPORT
   if (((scheme & SMTP_FLAG) || (scheme & DE_MAIL_FLAG)) &&
       (smtp_server[0] != '\0'))
#else
   if ((scheme & SMTP_FLAG) && (smtp_server[0] != '\0'))
#endif
   {
      fd = 0;
      while (smtp_server[fd] != '\0')
      {
         real_hostname[fd] = smtp_server[fd];
         fd++;
      }
      real_hostname[fd] = '\0';
   }
   t_hostname(real_hostname, host_alias);

   if (host_alias[0] != '\0')
   {
      if ((pos = get_host_position(fsa, host_alias, no_of_hosts)) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Failed to locate host %s in FSA [%s]. Ignoring!",
                    host_alias, msg_dir);
         free(file_buf);
         return(INCORRECT);
      }
   }
   else
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Removing %s. Could not locate host name.", msg_dir);
      if (unlink(msg_dir) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to unlink() %s : %s", msg_dir, strerror(errno));
      }
      free(file_buf);
      return(INCORRECT);
   }
   if (mdb_position == -1)
   {
      (*no_msg_cached)++;
      if ((*no_msg_cached != 0) &&
          ((*no_msg_cached % MSG_CACHE_BUF_SIZE) == 0))
      {
         size_t new_size;

         new_size = (((*no_msg_cached / MSG_CACHE_BUF_SIZE) + 1) *
                    MSG_CACHE_BUF_SIZE * sizeof(struct msg_cache_buf)) +
                    AFD_WORD_OFFSET;
         tmp_ptr = (char *)mdb - AFD_WORD_OFFSET;
         if ((tmp_ptr = mmap_resize(mdb_fd, tmp_ptr,
                                    new_size)) == (caddr_t) -1)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "mmap() error : %s", strerror(errno));
            exit(INCORRECT);
         }
         no_msg_cached = (int *)tmp_ptr;
         tmp_ptr += AFD_WORD_OFFSET;
         mdb = (struct msg_cache_buf *)tmp_ptr;
      }
      (void)strcpy(mdb[*no_msg_cached - 1].host_name, host_alias);
      mdb[*no_msg_cached - 1].fsa_pos = pos;
      mdb[*no_msg_cached - 1].job_id = job_id;
      tmp_ptr = ptr;
      if ((ptr = lposi(ptr, AGE_LIMIT_ID, AGE_LIMIT_ID_LENGTH)) == NULL)
      {
         mdb[*no_msg_cached - 1].age_limit = 0;
      }
      else
      {
         int  k = 0;
         char age_limit_str[MAX_INT_LENGTH + 1];

         while ((*ptr == ' ') || (*ptr == '\t'))
         {
            ptr++;
         }
         while (isdigit((int)(*ptr)))
         {
            age_limit_str[k] = *ptr;
            ptr++; k++;
         }
         if (k > 0)
         {
            age_limit_str[k] = '\0';
            mdb[*no_msg_cached - 1].age_limit = atoi(age_limit_str);
         }
         else
         {
            mdb[*no_msg_cached - 1].age_limit = 0;
         }
      }
      if ((ptr = lposi(tmp_ptr, AGEING_ID, AGEING_ID_LENGTH)) == NULL)
      {
         mdb[*no_msg_cached - 1].ageing = default_ageing;
      }
      else
      {
         int  k = 0;
         char ageing_str[MAX_INT_LENGTH + 1];

         while ((*ptr == ' ') || (*ptr == '\t'))
         {
            ptr++;
         }
         while (isdigit((int)(*ptr)))
         {
            ageing_str[k] = *ptr;
            ptr++; k++;
         }
         if (k > 0)
         {
            ageing_str[k] = '\0';
            mdb[*no_msg_cached - 1].ageing = atoi(ageing_str);
            if ((mdb[*no_msg_cached - 1].ageing < MIN_AGEING_VALUE) ||
                (mdb[*no_msg_cached - 1].ageing > MAX_AGEING_VALUE))
            {
               mdb[*no_msg_cached - 1].ageing = default_ageing;
            }
         }
         else
         {
            mdb[*no_msg_cached - 1].ageing = default_ageing;
         }
      }
      mdb[*no_msg_cached - 1].type = protocol;
      mdb[*no_msg_cached - 1].port = port;
      mdb[*no_msg_cached - 1].msg_time = msg_mtime;
      mdb[*no_msg_cached - 1].last_transfer_time = 0L;
   }
   else
   {
      (void)strcpy(mdb[mdb_position].host_name, host_alias);
      mdb[mdb_position].fsa_pos = pos;
      mdb[mdb_position].job_id = job_id;
      tmp_ptr = ptr;
      if ((ptr = lposi(ptr, AGE_LIMIT_ID, AGE_LIMIT_ID_LENGTH)) == NULL)
      {
         mdb[mdb_position].age_limit = 0;
      }
      else
      {
         int  k = 0;
         char age_limit_str[MAX_INT_LENGTH + 1];

         while ((*ptr == ' ') || (*ptr == '\t'))
         {
            ptr++;
         }
         while (isdigit((int)(*ptr)))
         {
            age_limit_str[k] = *ptr;
            ptr++; k++;
         }
         if (k > 0)
         {
            age_limit_str[k] = '\0';
            mdb[mdb_position].age_limit = atoi(age_limit_str);
         }
         else
         {
            mdb[mdb_position].age_limit = 0;
         }
      }
      if ((ptr = lposi(tmp_ptr, AGEING_ID, AGEING_ID_LENGTH)) == NULL)
      {
         mdb[mdb_position].ageing = default_ageing;
      }
      else
      {
         int  k = 0;
         char ageing_str[MAX_INT_LENGTH + 1];

         while ((*ptr == ' ') || (*ptr == '\t'))
         {
            ptr++;
         }
         while (isdigit((int)(*ptr)))
         {
            ageing_str[k] = *ptr;
            ptr++; k++;
         }
         if (k > 0)
         {
            ageing_str[k] = '\0';
            mdb[mdb_position].ageing = atoi(ageing_str);
            if ((mdb[mdb_position].ageing < MIN_AGEING_VALUE) ||
                (mdb[mdb_position].ageing > MAX_AGEING_VALUE))
            {
               mdb[mdb_position].ageing = default_ageing;
            }
         }
         else
         {
            mdb[mdb_position].ageing = default_ageing;
         }
      }
      mdb[mdb_position].type = protocol;
      mdb[mdb_position].port = port;
      mdb[mdb_position].msg_time = msg_mtime;
      /*
       * NOTE: Do NOT initialize last_transfer_time! This could
       *       lead to a to early deletion of a job.
       */
   }

   free(file_buf);

   return(SUCCESS);
}
