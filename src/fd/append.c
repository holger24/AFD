/*
 *  append.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   append - functions for appending files in FTP+SFTP
 **
 ** SYNOPSIS
 **   void log_append(struct job *p_db, char *file_name, char *source_file_name)
 **   void remove_append(unsigned int job_id, char *file_name)
 **   void remove_all_appends(unsigned int job_id)
 **   int  append_compare(char *append_data, char *fullname)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None. If we fail to log the fail name, all that happens the next
 **   time is that the complete file gets send again.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   24.01.1998 H.Kiehl Created
 **   21.09.1998 H.Kiehl Added function remove_all_appends()
 **   23.08.2001 H.Kiehl The date of the file is now also used to decide
 **                      if we append a file. For this reason the new
 **                      function append_compare() was added.
 **
 */
DESCR__E_M3

#include <stdio.h>                /* snprintf()                          */
#include <string.h>               /* strcmp(), strerror()                */
#include <unistd.h>               /* close(), ftruncate()                */
#include <stdlib.h>               /* malloc(), free()                    */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "fddefs.h"

/* External global variables. */
extern char *p_work_dir;


/*############################ log_append() #############################*/
void
log_append(struct job *p_db, char *file_name, char *source_file_name)
{
   int          fd;
   size_t       buf_size;
   off_t        msg_file_size;
   char         *buffer,
                *ptr,
                msg[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   (void)snprintf(msg, MAX_PATH_LENGTH, "%s%s/%x",
                  p_work_dir, AFD_MSG_DIR, p_db->id.job);

   if ((fd = lock_file(msg, ON)) < 0)
   {
      return;
   }
#ifdef HAVE_STATX
   if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(fd, &stat_buf) == -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                 "Failed to statx() message %s : %s",
#else
                 "Failed to fstat() message %s : %s",
#endif
                 msg, strerror(errno));
      (void)close(fd);
      return;
   }
#ifdef HAVE_STATX
   buf_size = stat_buf.stx_size + OPTION_IDENTIFIER_LENGTH +
#else
   buf_size = stat_buf.st_size + OPTION_IDENTIFIER_LENGTH +
#endif
              RESTART_FILE_ID_LENGTH + strlen(file_name) + 20 + 4;
   if ((buffer = malloc(buf_size + 1)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      (void)close(fd);
      return;
   }
#ifdef HAVE_STATX
   if (read(fd, buffer, stat_buf.stx_size) != stat_buf.stx_size)
#else
   if (read(fd, buffer, stat_buf.st_size) != stat_buf.st_size)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to read() message %s : %s", msg, strerror(errno));
      free(buffer);
      (void)close(fd);
      return;
   }
#ifdef HAVE_STATX
   buffer[stat_buf.stx_size] = '\0';
   msg_file_size = stat_buf.stx_size;
#else
   buffer[stat_buf.st_size] = '\0';
   msg_file_size = stat_buf.st_size;
#endif

   /* Get the date of the current file. */
   (void)snprintf(msg, MAX_PATH_LENGTH, "%s%s%s/%s/%s",
                  p_work_dir, AFD_FILE_DIR,
                  OUTGOING_DIR, p_db->msg_name, source_file_name);
#ifdef HAVE_STATX
   if (statx(0, msg, AT_STATX_SYNC_AS_STAT, STATX_MTIME, &stat_buf) == -1)
#else
   if (stat(msg, &stat_buf) == -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                 "Failed to statx() %s : %s",
#else
                 "Failed to stat() %s : %s",
#endif
                 msg, strerror(errno));
      (void)close(fd);
      free(buffer);
      return;
   }

   /* First determine if there is an option identifier. */
   if ((ptr = lposi(buffer, OPTION_IDENTIFIER,
                    OPTION_IDENTIFIER_LENGTH)) == NULL)
   {
      /* Add the option and restart identifier. */
      ptr = buffer + msg_file_size;
      ptr += snprintf(ptr, buf_size - msg_file_size, "\n%s\n%s",
                      OPTION_IDENTIFIER, RESTART_FILE_ID);
   }
   else
   {
      char *tmp_ptr;

      /* Check if the append option is already in the message. */
      if ((tmp_ptr = lposi(ptr, RESTART_FILE_ID,
                           RESTART_FILE_ID_LENGTH)) != NULL)
      {
         char *end_ptr,
              file_and_date_str[MAX_FILENAME_LENGTH + MAX_TIME_T_LENGTH],
              tmp_char;

         while (*tmp_ptr == ' ')
         {
            tmp_ptr++;
         }
         (void)snprintf(file_and_date_str,
                        MAX_FILENAME_LENGTH + MAX_TIME_T_LENGTH,
#if SIZEOF_TIME_T == 4
                        "%s|%ld\n",
#else
                        "%s|%lld\n",
#endif
#ifdef HAVE_STATX
                        file_name, (pri_time_t)stat_buf.stx_mtime.tv_sec
#else
                        file_name, (pri_time_t)stat_buf.st_mtime
#endif
                       );

         /* Now check if the file name is already in the list. */
         do
         {
            end_ptr = tmp_ptr;
            while ((*end_ptr != '|') && (*end_ptr != ' ') &&
                   (*end_ptr != '\n'))
            {
               end_ptr++;
            }
            tmp_char = *end_ptr;
            *end_ptr = '\0';
            if (CHECK_STRCMP(tmp_ptr, file_name) == 0)
            {
               if (tmp_char == '|')
               {
                  int  r_length = 0,
                       w_length;
                  char *end_ptr_2,
                       tmp_char_2;

                  end_ptr_2 = end_ptr + 1;
                  while ((*end_ptr_2 != ' ') && (*end_ptr_2 != '\n'))
                  {
                     end_ptr_2++;
                     r_length++;
                  }
                  tmp_char_2 = *end_ptr_2;
                  w_length = snprintf(end_ptr + 1, MAX_TIME_T_LENGTH,
#if SIZEOF_TIME_T == 4
                                      "%ld",
#else
                                      "%lld",
#endif
#ifdef HAVE_STATX
                                      (pri_time_t)stat_buf.stx_mtime.tv_sec
#else
                                      (pri_time_t)stat_buf.st_mtime
#endif
                                     );
                  if (w_length > r_length)
                  {
                     if (w_length > r_length)
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                  "Uurrgghhhh, w_length [%d] is more then one character longer then r_length [%d]",
                                  w_length, r_length);
                        *(end_ptr + 1 + w_length) = '\n';
                        *(end_ptr + 1 + w_length + 1) = '\0';
                     }
                  }
                  else if (w_length < r_length)
                       {
                          (void)memmove(end_ptr + 1 + w_length + 1,
                                        end_ptr + 1 + r_length + 1,
                                        msg_file_size - (end_ptr + 1 + r_length + 1 - buffer));
                          *(end_ptr + 1 + w_length) = tmp_char_2;
                          buffer[msg_file_size - (r_length - w_length)] = '\0';
                       }
                       else
                       {
                          *(end_ptr + 1 + w_length) = tmp_char_2;
                       }
                  *end_ptr = tmp_char;
                  buf_size = strlen(buffer);
                  if (lseek(fd, 0, SEEK_SET) == -1)
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "Failed to lseek() in message %x : %s",
                                p_db->id.job, strerror(errno));
                  }
                  else
                  {
                     if (write(fd, buffer, buf_size) != buf_size)
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   "Failed to write() to message %x : %s",
                                   p_db->id.job, strerror(errno));
                     }
                     else
                     {
                        if (msg_file_size > buf_size)
                        {
                           if (ftruncate(fd, buf_size) == -1)
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                         "Failed to ftruncate() message %x : %s",
                                         p_db->id.job, strerror(errno));
                           }
                        }
                     }
                  }
               }

               free(buffer);
               if (close(fd) == -1)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "close() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
               }
               return;
            }
            else
            {
               *end_ptr = tmp_char;
               while ((*end_ptr != ' ') && (*end_ptr != '\n'))
               {
                  end_ptr++;
               }
            }
            tmp_ptr = end_ptr;
            while (*tmp_ptr == ' ')
            {
               tmp_ptr++;
            }
         } while (*tmp_ptr != '\n');

         ptr = tmp_ptr;
      }
      else
      {
         ptr = buffer + msg_file_size;
         ptr += snprintf(ptr, buf_size - msg_file_size, "%s", RESTART_FILE_ID);
      }
   }

   /* Append the file name. */
   *(ptr++) = ' ';
   ptr += snprintf(ptr, buf_size - msg_file_size,
#if SIZEOF_TIME_T == 4
# ifdef HAVE_STATX
                   "%s|%ld\n", file_name, (pri_time_t)stat_buf.stx_mtime.tv_sec
# else
                   "%s|%ld\n", file_name, (pri_time_t)stat_buf.st_mtime
# endif
#else
# ifdef HAVE_STATX
                   "%s|%lld\n", file_name, (pri_time_t)stat_buf.stx_mtime.tv_sec
# else
                   "%s|%lld\n", file_name, (pri_time_t)stat_buf.st_mtime
# endif
#endif
                  );
   *ptr = '\0';
   buf_size = strlen(buffer);

   if (lseek(fd, 0, SEEK_SET) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to lseek() in message %x : %s",
                 p_db->id.job, strerror(errno));
   }
   else
   {
      if (write(fd, buffer, buf_size) != buf_size)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to write() to message %x : %s",
                    p_db->id.job, strerror(errno));
      }
      else
      {
         if (msg_file_size > buf_size)
         {
            if (ftruncate(fd, buf_size) == -1)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to ftruncate() message %x : %s",
                          p_db->id.job, strerror(errno));
            }
         }
      }
   }
   free(buffer);
   if (close(fd) < 0)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to close() %x : %s", p_db->id.job, strerror(errno));
   }

   return;
}


/*########################## remove_append() ############################*/
void
remove_append(unsigned int job_id, char *file_name)
{
   int          fd;
   size_t       length;
   time_t       file_date;
   char         *buffer,
                *ptr,
                *tmp_ptr,
                msg[MAX_PATH_LENGTH],
                *search_str;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   (void)snprintf(msg, MAX_PATH_LENGTH, "%s%s/%x", p_work_dir, AFD_MSG_DIR, job_id);

   if ((fd = lock_file(msg, ON)) < 0)
   {
      return;
   }
#ifdef HAVE_STATX
   if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(fd, &stat_buf) == -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                 "Failed to statx() message %s : %s",
#else
                 "Failed to fstat() message %s : %s",
#endif
                 msg, strerror(errno));
      (void)close(fd);
      return;
   }
#ifdef HAVE_STATX
   if ((buffer = malloc(stat_buf.stx_size + 1)) == NULL)
#else
   if ((buffer = malloc(stat_buf.st_size + 1)) == NULL)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      (void)close(fd);
      return;
   }
#ifdef HAVE_STATX
   if (read(fd, buffer, stat_buf.stx_size) != stat_buf.stx_size)
#else
   if (read(fd, buffer, stat_buf.st_size) != stat_buf.st_size)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to read() message %s : %s", msg, strerror(errno));
      free(buffer);
      (void)close(fd);
      return;
   }
#ifdef HAVE_STATX
   buffer[stat_buf.stx_size] = '\0';
#else
   buffer[stat_buf.st_size] = '\0';
#endif

   /* Retrieve file date which is stored just behind the file name. */
   ptr = file_name;
   while (*ptr != '\0')
   {
      ptr++;
   }
   ptr++;
   file_date = atol(ptr);

   if ((ptr = lposi(buffer, RESTART_FILE_ID, RESTART_FILE_ID_LENGTH)) == NULL)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Failed to locate <%s> identifier in message %s.",
                 RESTART_FILE_ID, msg);
      free(buffer);
      (void)close(fd);
      return;
   }

   if ((search_str = malloc(MAX_FILENAME_LENGTH + MAX_TIME_T_LENGTH)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      free(buffer);
      (void)close(fd);
      return;
   }
   length = snprintf(search_str, MAX_FILENAME_LENGTH + MAX_TIME_T_LENGTH,
#if SIZEOF_TIME_T == 4
                     "%s|%ld", file_name, (pri_time_t)file_date);
#else
                     "%s|%lld", file_name, (pri_time_t)file_date);
#endif

   /* Locate the file name. */
   for (;;)
   {
      if ((tmp_ptr = lposi(ptr, search_str, length)) == NULL)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Failed to locate <%s> in restart option of message %s.",
                    search_str, msg);
         free(buffer);
         free(search_str);
         (void)close(fd);
         return;
      }
      if ((*(tmp_ptr - 1) == ' ') || (*(tmp_ptr - 1) == '\n') ||
          (*(tmp_ptr - 1) == '\0'))
      {
         break;
      }
      else
      {
         ptr = tmp_ptr;
      }
   }
   tmp_ptr--;

   length = strlen(search_str);
   free(search_str);
   if (((tmp_ptr - length) == ptr) && (*tmp_ptr == '\n'))
   {
      /* This is the only file name, so lets remove the option. */
      while (*ptr != '\n')
      {
         ptr--;     /* Go to the beginning of the option. */
      }
      *(ptr + 1) = '\0';
   }
   else
   {
      if (*tmp_ptr == '\n')
      {
         ptr = tmp_ptr - length - 1 /* To remove the space sign. */;
         *ptr = '\n';
         *(ptr + 1) = '\0';
      }
      else
      {
         int n = strlen((tmp_ptr + 1));

         (void)memmove((tmp_ptr - length), (tmp_ptr + 1), n);
         ptr = tmp_ptr - length + n;
         *ptr = '\0'; /* To remove any junk after the part we just moved. */
      }
   }
   length = strlen(buffer);

   if (lseek(fd, 0, SEEK_SET) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to lseek() %s : %s", msg, strerror(errno));
   }
   else
   {
      if (write(fd, buffer, length) != length)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to write() to %s : %s", msg, strerror(errno));
      }
      else
      {
#ifdef HAVE_STATX
         if (stat_buf.stx_size > length)
#else
         if (stat_buf.st_size > length)
#endif
         {
            if (ftruncate(fd, length) == -1)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to ftruncate() %s : %s",
                          msg, strerror(errno));
            }
         }
      }
   }
   free(buffer);
   if (close(fd) < 0)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to close() %s : %s", msg, strerror(errno));
   }

   return;
}


/*######################## remove_all_appends() #########################*/
void
remove_all_appends(unsigned int job_id)
{
   int         fd;
   size_t      length;
   char        *buffer,
               *ptr,
               msg[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat stat_buf;
#endif

   (void)snprintf(msg, MAX_PATH_LENGTH, "%s%s/%x", p_work_dir, AFD_MSG_DIR, job_id);

   if ((fd = lock_file(msg, ON)) < 0)
   {
      return;
   }
#ifdef HAVE_STATX
   if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(fd, &stat_buf) == -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                 "Failed to statx() message %s : %s",
#else
                 "Failed to fstat() message %s : %s",
#endif
                 msg, strerror(errno));
      (void)close(fd);
      return;
   }
#ifdef HAVE_STATX
   if ((buffer = malloc(stat_buf.stx_size + 1)) == NULL)
#else
   if ((buffer = malloc(stat_buf.st_size + 1)) == NULL)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      (void)close(fd);
      return;
   }
#ifdef HAVE_STATX
   if (read(fd, buffer, stat_buf.stx_size) != stat_buf.stx_size)
#else
   if (read(fd, buffer, stat_buf.st_size) != stat_buf.st_size)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to read() message %s : %s", msg, strerror(errno));
      free(buffer);
      (void)close(fd);
      return;
   }
#ifdef HAVE_STATX
   buffer[stat_buf.stx_size] = '\0';
#else
   buffer[stat_buf.st_size] = '\0';
#endif

   if ((ptr = lposi(buffer, RESTART_FILE_ID, RESTART_FILE_ID_LENGTH)) == NULL)
   {
      /*
       * It can very well happen that the restart identifier has been
       * deleted by another process. So there is no need to put this
       * into the SYSTEM_LOG.
       */
      free(buffer);
      (void)close(fd);
      return;
   }

   /* Lets remove the restart option. */
   while (*ptr != '\n')
   {
      ptr--;     /* Go to the beginning of the option. */
   }
   *(ptr + 1) = '\0';
   length = strlen(buffer);

   if (lseek(fd, 0, SEEK_SET) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to lseek() %s : %s", msg, strerror(errno));
   }
   else
   {
      if (write(fd, buffer, length) != length)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to write() to %s : %s", msg, strerror(errno));
      }
      else
      {
#ifdef HAVE_STATX
         if (stat_buf.stx_size > length)
#else
         if (stat_buf.st_size > length)
#endif
         {
            if (ftruncate(fd, length) == -1)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to ftruncate() %s : %s",
                          msg, strerror(errno));
            }
         }
      }
   }
   free(buffer);
   if (close(fd) < 0)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to close() %s : %s", msg, strerror(errno));
   }
   trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
             "Hmm. Removed all append options for #%x.", job_id);

   return;
}


/*############################ append_compare() #########################*/
int
append_compare(char *append_data, char *fullname)
{
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat stat_buf;
#endif

#ifdef HAVE_STATX
   if (statx(0, fullname, AT_STATX_SYNC_AS_STAT, STATX_MTIME, &stat_buf) == -1)
#else
   if (stat(fullname, &stat_buf) == -1)
#endif
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                 "Failed to statx() %s : %s",
#else
                 "Failed to stat() %s : %s",
#endif
                 fullname, strerror(errno));
   }
   else
   {
      char *ptr;

      ptr = append_data;
      while (*ptr != '\0')
      {
         ptr++;
      }
      ptr++;
#ifdef HAVE_STATX
      if (stat_buf.stx_mtime.tv_sec == atol(ptr))
#else
      if (stat_buf.st_mtime == atol(ptr))
#endif
      {
         return(YES);
      }
   }
   return(NO);
}
