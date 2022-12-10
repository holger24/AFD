/*
 *  format_info.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   format_info - puts data from a structure into a human readable
 **                 form
 **
 ** SYNOPSIS
 **   void format_output_info(char **text, int pos)
 **   void format_input_info(char **text, int pos)
 **   void format_retrieve_info(char **text, int pos)
 **
 ** DESCRIPTION
 **   The function format_output_info() formats data from the various
 **   structures to the following form:
 **         File name  : xxxxxxx.xx
 **         Msg dir    : 3_991243800_118
 **         Directory  : /aaa/bbb/ccc
 **         Dir Alias  : abc
 **         Dir-ID     : 12fd45
 **         Filter     : filter_1
 **                      filter_2
 **                      filter_n
 **         Recipient  : ftp://donald:secret@hollywood//home/user
 **         AMG-options: option_1
 **                      option_2
 **                      option_n
 **         FD-options : option_1
 **                      option_2
 **                      option_n
 **         Priority   : 5
 **         Job-ID     : 4323121
 **
 **    format_input_info() does it slightly differently:
 **         File name  : xxxxxxx.xx
 **         Hostname   : esoc
 **         Dir-ID     : 12fd45
 **         Dir Alias  : abc
 **         Directory  : /aaa/bbb/ccc
 **         =====================================================
 **         Filter     : filter_1
 **                      filter_2    
 **                      filter_n
 **         Recipient  : ftp://donald:secret@hollywood//home/user
 **         AMG-options: option_1
 **                      option_2
 **                      option_n
 **         FD-options : option_1
 **                      option_2
 **                      option_n
 **         Priority   : 5
 **         -----------------------------------------------------
 **                                  .
 **                                  .
 **                                 etc.
 **
 **    format_retrieve_info() looks as follows:
 **         Hostname   : esoc
 **         Dir-ID     : 12fd45
 **         Dir Alias  : abc
 **         Directory  : /aaa/bbb/ccc
 **
 **
 ** RETURN VALUES
 **   Returns the formated text.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   29.07.2001 H.Kiehl Created
 **   11.06.2006 H.Kiehl Show retrieve information.
 **   28.09.2006 H.Kiehl Show directory alias for all casses.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* sprintf()                          */
#include <stdlib.h>                /* atoi(), malloc(), realloc(), free()*/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include "show_queue.h"

#define MAX_INPUT_INFO_SIZE (3 * MEGABYTE)


/* External global variables. */
extern int                     max_x,
                               max_y;
extern char                    *p_work_dir;
extern struct sol_perm         perm;
extern struct queued_file_list *qfl;


/*######################## format_output_info() #########################*/
void
format_output_info(char **text, int pos)
{
   int buffer_length = 8192,
       count,
       length = 0;

   if ((*text = malloc(buffer_length)) == NULL)
   {
      (void)xrec(FATAL_DIALOG, "Failed to malloc() %d bytes : %s (%s %d)",
                 buffer_length, strerror(errno), __FILE__, __LINE__);
      return;
   }
   max_x = sprintf(*text, "File name  : ");
   while (qfl[pos].file_name[length] != '\0')
   {
      if (qfl[pos].file_name[length] < ' ')
      {
         *(*text + max_x) = '?';
      }
      else
      {
         *(*text + max_x) = qfl[pos].file_name[length];
      }
      length++; max_x++;
   }
   *(*text + max_x) = '\n';
   max_x++;
   length = max_x;

   if (qfl[pos].queue_type == SHOW_TIME_JOBS)
   {
      /* Show time dir. */
#ifdef MULTI_FS_SUPPORT
      count = sprintf(*text + length, "Time dir   : %s%s%s/%s/%x\n",
                      p_work_dir, AFD_FILE_DIR, AFD_TIME_DIR,
                      qfl[pos].msg_name, qfl[pos].job_id);
#else
      count = sprintf(*text + length, "Time dir   : %s%s%s/%x\n",
                      p_work_dir, AFD_FILE_DIR, AFD_TIME_DIR, qfl[pos].job_id);
#endif
   }
   else
   {
      /* Show message name. */
      count = sprintf(*text + length, "Msg name   : %s%s%s/%s\n",
                      p_work_dir, AFD_FILE_DIR, OUTGOING_DIR,
                      qfl[pos].msg_name);
   }
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y = 2;

   if (qfl[pos].job_id != 0)
   {
      int                fd,
                         i,
                         jd_pos = -1,
                         no_of_jobs;
      off_t              jd_size;
      char               fullname[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
      struct statx       stat_buf;
#else
      struct stat        stat_buf;
#endif
      struct job_id_data *jd;
      struct dir_options d_o;

      /* Map to job ID data file. */
      (void)sprintf(fullname, "%s%s%s", p_work_dir, FIFO_DIR,
                    JOB_ID_DATA_FILE);
      if ((fd = open(fullname, O_RDONLY)) == -1)             
      {
         (void)xrec(ERROR_DIALOG, "Failed to open() %s : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
         return;
      }
#ifdef HAVE_STATX
      if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(fd, &stat_buf) == -1)
#endif
      {
         (void)xrec(ERROR_DIALOG, "Failed to access %s : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return;
      }
#ifdef HAVE_STATX
      if (stat_buf.stx_size > 0)
#else
      if (stat_buf.st_size > 0)
#endif
      {
         char *ptr;

         if ((ptr = mmap(NULL,
#ifdef HAVE_STATX
                         stat_buf.stx_size, PROT_READ,
#else
                         stat_buf.st_size, PROT_READ,
#endif
                         MAP_SHARED, fd, 0)) == (caddr_t) -1)
         {
            (void)xrec(ERROR_DIALOG, "Failed to mmap() to %s : %s (%s %d)",
                       fullname, strerror(errno), __FILE__, __LINE__);
            (void)close(fd);
            return;
         }
         if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_JID_VERSION)
         {
            (void)xrec(ERROR_DIALOG,
                       "Incorrect JID version (data=%d current=%d)!",
                       *(ptr + SIZEOF_INT + 1 + 1 + 1), CURRENT_JID_VERSION);
            (void)close(fd);
            return;
         }
#ifdef HAVE_STATX
         jd_size = stat_buf.stx_size;
#else
         jd_size = stat_buf.st_size;
#endif
         no_of_jobs = *(int *)ptr;
         ptr += AFD_WORD_OFFSET;
         jd = (struct job_id_data *)ptr;
         (void)close(fd);
      }
      else
      {
         (void)xrec(ERROR_DIALOG, "Job ID database file is empty. (%s %d)",
                    __FILE__, __LINE__);
         (void)close(fd);
         return;
      }
      for (i = 0; i < no_of_jobs; i++)
      {
         if (qfl[pos].job_id == jd[i].job_id)
         {
            jd_pos = i;
            break;
         }
      }
      if (jd_pos != -1)
      {
         int                 no_of_file_masks;
         off_t               dnb_size;
         char                *p_file_list,
                             *ptr,
                             recipient[MAX_RECIPIENT_LENGTH];
         struct dir_name_buf *dnb;

         /* Map to directory name buffer. */
         (void)sprintf(fullname, "%s%s%s", p_work_dir, FIFO_DIR,
                       DIR_NAME_FILE);
         if ((fd = open(fullname, O_RDONLY)) == -1)
         {
            (void)xrec(ERROR_DIALOG, "Failed to open() <%s> : %s (%s %d)",
                       fullname, strerror(errno), __FILE__, __LINE__);
            return;
         }
#ifdef HAVE_STATX
         if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                   STATX_SIZE, &stat_buf) == -1)
#else
         if (fstat(fd, &stat_buf) == -1)
#endif
         {
            (void)xrec(ERROR_DIALOG, "Failed to access <%s> : %s (%s %d)",
                       fullname, strerror(errno), __FILE__, __LINE__);
            (void)close(fd);
            return;
         }
#ifdef HAVE_STATX
         if (stat_buf.stx_size > 0)
#else
         if (stat_buf.st_size > 0)
#endif
         {
            if ((ptr = mmap(NULL,
#ifdef HAVE_STATX
                            stat_buf.stx_size, PROT_READ,
#else
                            stat_buf.st_size, PROT_READ,
#endif
                            MAP_SHARED, fd, 0)) == (caddr_t) -1)
            {
               (void)xrec(ERROR_DIALOG, "Failed to mmap() to <%s> : %s (%s %d)",
                          fullname, strerror(errno), __FILE__, __LINE__);
               (void)close(fd);
               return;
            }
#ifdef HAVE_STATX
            dnb_size = stat_buf.stx_size;
#else
            dnb_size = stat_buf.st_size;
#endif
            ptr += AFD_WORD_OFFSET;
            dnb = (struct dir_name_buf *)ptr;
            (void)close(fd);
         }
         else
         {
            (void)xrec(ERROR_DIALOG, "Dirname database file is empty. (%s %d)",
                       __FILE__, __LINE__);
            (void)close(fd);
            return;
         }

         count = sprintf(*text + length, "Directory  : %s\n",
                         dnb[qfl[pos].dir_id_pos].dir_name);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }

         count = sprintf(*text + length, "Dir Alias  : %s\n",
                         qfl[pos].dir_alias);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }

         count = sprintf(*text + length, "Dir-ID     : %x\n", qfl[pos].dir_id);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }

         get_dir_options(qfl[pos].dir_id, &d_o);
         if (d_o.no_of_dir_options > 0)
         {
            count = sprintf(*text + length, "DIR-options: %s\n",
                            d_o.aoptions[0]);
            length += count;
            if (count > max_x)
            {
               max_x = count;
            }
            max_y++;
            for (i = 1; i < d_o.no_of_dir_options; i++)
            {
               count = sprintf(*text + length, "             %s\n",
                               d_o.aoptions[i]);
               length += count;
               if (count > max_x)
               {
                  max_x = count;
               }
               max_y++;
            }
         }

         get_file_mask_list(jd[jd_pos].file_mask_id, &no_of_file_masks,
                            &p_file_list);
         if (p_file_list != NULL)
         {
            ptr = p_file_list;
            count = sprintf(*text + length, "Filter     : %s\n", ptr);
            length += count;
            NEXT(ptr);
            if (count > max_x)
            {
               max_x = count;
            }
            max_y += 4;
            for (i = 1; i < no_of_file_masks; i++)
            {
               if (length > (buffer_length - 1024))
               {
                  buffer_length += 8192;
                  if (buffer_length > (10 * MEGABYTE))
                  {
                     (void)xrec(INFO_DIALOG,
                                "Buffer for writting DIR_CONFIG data is larger then 10 Megabyte. DIR_CONFIG data incomplete. (%s %d)",
                                __FILE__, __LINE__);
                     return;
                  }
                  if ((*text = realloc(*text, buffer_length)) == NULL)
                  {
                     (void)xrec(FATAL_DIALOG,
                                "Failed to realloc() %d bytes : %s (%s %d)",
                                buffer_length, strerror(errno),
                                __FILE__, __LINE__);
                     return;
                  }
               }
               count = sprintf(*text + length, "             %s\n", ptr);
               length += count;
               NEXT(ptr);
               if (count > max_x)
               {
                  max_x = count;
               }
               max_y++;
            }
            free(p_file_list);
         }
         else
         {
            max_y += 3;
         }

         /* Print recipient. */
         (void)strcpy(recipient, jd[jd_pos].recipient);
         if (perm.view_passwd == YES)
         {
            insert_passwd(recipient);
         }
         count = sprintf(*text + length, "Recipient  : %s\n", recipient);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
         if (jd[jd_pos].no_of_loptions > 0)
         {
            char *p_tmp;

            p_tmp = jd[jd_pos].loptions;
            count = sprintf(*text + length, "AMG-options: %s\n", p_tmp);
            NEXT(p_tmp);
            length += count;
            if (count > max_x)
            {
               max_x = count;
            }
            max_y++;
            for (i = 1; i < jd[jd_pos].no_of_loptions; i++)
            {
               count = sprintf(*text + length, "             %s\n", p_tmp);
               NEXT(p_tmp);
               length += count;
               if (count > max_x)
               {
                  max_x = count;
               }
               max_y++;
            }
         }
         if (jd[jd_pos].no_of_soptions == 1)
         {
            count = sprintf(*text + length, "FD-options : %s\n",
                            jd[jd_pos].soptions);
            length += count;
            if (count > max_x)
            {
               max_x = count;
            }
            max_y++;
         }
         else if (jd[jd_pos].no_of_soptions > 1)
              {
                 int  first = YES;
                 char *p_start,
                      *p_end,
                      *p_soptions;

                 if ((p_soptions = malloc(MAX_OPTION_LENGTH)) == NULL)
                 {
                    (void)xrec(ERROR_DIALOG, "malloc() erro : %s (%s %d)",
                               strerror(errno), __FILE__, __LINE__);
                    (void)close(fd);
                    return;
                 }
                 (void)memcpy(p_soptions, jd[jd_pos].soptions,
                              MAX_OPTION_LENGTH);
                 p_start = p_end = p_soptions;
                 do
                 {
                    while ((*p_end != '\n') && (*p_end != '\0'))
                    {
                       p_end++;
                    }
                    if (*p_end == '\n')
                    {
                       *p_end = '\0';
                       if (first == YES)
                       {
                          first = NO;
                          count = sprintf(*text + length, "FD-options : %s\n",
                                          p_start);
                       }
                       else
                       {
                          count = sprintf(*text + length, "             %s\n",
                                          p_start);
                       }
                       length += count;
                       if (count > max_x)
                       {
                          max_x = count;
                       }
                       max_y++;
                       p_end++;
                       p_start = p_end;
                    }
                 } while (*p_end != '\0');
                 count = sprintf(*text + length, "             %s\n", p_start);
                 length += count;
                 if (count > max_x)
                 {
                    max_x = count;
                 }
                 max_y++;
                 free(p_soptions);
              }
         count = sprintf(*text + length, "Priority   : %c\n",
                         jd[jd_pos].priority);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;

         if (munmap(((char *)dnb - AFD_WORD_OFFSET), dnb_size) == -1)
         {
            (void)xrec(INFO_DIALOG, "munmap() error : %s (%s %d)",
                       strerror(errno), __FILE__, __LINE__);
         }
      }
      if (munmap(((char *)jd - AFD_WORD_OFFSET), jd_size) == -1)
      {
         (void)xrec(INFO_DIALOG, "munmap() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
      }
   }

   count = sprintf(*text + length, "Job-ID     : %x", qfl[pos].job_id);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y++;

   return;
}


/*######################### format_input_info() #########################*/
void
format_input_info(char **text, int pos)
{
   int                 count,
                       fd,
                       jobs_found = 0,
                       length = 0;
   off_t               dnb_size;
   char                fullname[MAX_PATH_LENGTH],
                       *p_begin_underline = NULL,
                       **p_array = NULL;
#ifdef HAVE_STATX
   struct statx        stat_buf;
#else
   struct stat         stat_buf;
#endif
   struct dir_name_buf *dnb;

   if ((*text = malloc(MAX_INPUT_INFO_SIZE)) == NULL)
   {
      (void)xrec(FATAL_DIALOG, "Failed to malloc() %d bytes : %s (%s %d)",
                 MAX_INPUT_INFO_SIZE, strerror(errno), __FILE__, __LINE__);
      return;
   }
   max_x = sprintf(*text, "File name  : ");
   while (qfl[pos].file_name[length] != '\0')
   {
      if (qfl[pos].file_name[length] < ' ')
      {
         *(*text + max_x) = '?';
      }
      else
      {
         *(*text + max_x) = qfl[pos].file_name[length];
      }
      length++; max_x++;
   }
   *(*text + max_x) = '\n';
   max_x++;
   length = max_x;

   /* Show hostname. */
   count = sprintf(*text + length, "Hostname   : %s\n", qfl[pos].hostname);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y = 2;

   /* Map to directory name buffer. */
   (void)sprintf(fullname, "%s%s%s", p_work_dir, FIFO_DIR,
                 DIR_NAME_FILE);
   if ((fd = open(fullname, O_RDONLY)) == -1)
   {
      (void)xrec(ERROR_DIALOG, "Failed to open() <%s> : %s (%s %d)",
                 fullname, strerror(errno), __FILE__, __LINE__);
      return;
   }
#ifdef HAVE_STATX
   if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(fd, &stat_buf) == -1)
#endif
   {
      (void)xrec(ERROR_DIALOG, "Failed to access <%s> : %s (%s %d)",
                 fullname, strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      return;
   }
#ifdef HAVE_STATX
   if (stat_buf.stx_size > 0)
#else
   if (stat_buf.st_size > 0)
#endif
   {
      char *ptr;

      if ((ptr = mmap(NULL,
#ifdef HAVE_STATX
                      stat_buf.stx_size, PROT_READ,
#else
                      stat_buf.st_size, PROT_READ,
#endif
                      MAP_SHARED, fd, 0)) == (caddr_t) -1)
      {
         (void)xrec(ERROR_DIALOG, "Failed to mmap() to <%s> : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return;
      }
#ifdef HAVE_STATX
      dnb_size = stat_buf.stx_size;
#else
      dnb_size = stat_buf.st_size;
#endif
      ptr += AFD_WORD_OFFSET;
      dnb = (struct dir_name_buf *)ptr;
      (void)close(fd);
   }
   else
   {
      (void)xrec(ERROR_DIALOG, "Dirname database file is empty. (%s %d)",
                 __FILE__, __LINE__);
      (void)close(fd);
      return;
   }

   count = sprintf(*text + length, "Dir-ID     : %x\n", qfl[pos].dir_id);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }

   count = sprintf(*text + length, "Dir Alias  : %s\n", qfl[pos].dir_alias);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y += 2;

   if (dnb[qfl[pos].dir_id_pos].dir_name[0] != '\0')
   {
      int                i,
                         no_of_jobs;
      off_t              jd_size;
      struct job_id_data *jd;
      struct dir_options d_o;

      count = sprintf(*text + length, "Directory  : %s\n",
                      dnb[qfl[pos].dir_id_pos].dir_name);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;

      get_dir_options(qfl[pos].dir_id, &d_o);
      if (d_o.no_of_dir_options > 0)
      {
         count = sprintf(*text + length, "DIR-options: %s\n",
                         d_o.aoptions[0]);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
         for (i = 1; i < d_o.no_of_dir_options; i++)
         {
            count = sprintf(*text + length, "             %s\n",
                            d_o.aoptions[i]);
            length += count;
            if (count > max_x)
            {
               max_x = count;
            }
            max_y++;
         }
      }
      p_begin_underline = *text + length;

      /* Map to job ID data file. */
      (void)sprintf(fullname, "%s%s%s", p_work_dir, FIFO_DIR,
                    JOB_ID_DATA_FILE);
      if ((fd = open(fullname, O_RDONLY)) == -1)
      {
         (void)xrec(ERROR_DIALOG, "Failed to open() %s : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
         return;
      }
#ifdef HAVE_STATX
      if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(fd, &stat_buf) == -1)
#endif
      {
         (void)xrec(ERROR_DIALOG, "Failed to access %s : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return;
      }
#ifdef HAVE_STATX
      if (stat_buf.stx_size > 0)
#else
      if (stat_buf.st_size > 0)
#endif
      {
         char *ptr;

         if ((ptr = mmap(NULL,
#ifdef HAVE_STATX
                         stat_buf.stx_size, PROT_READ,
#else
                         stat_buf.st_size, PROT_READ,
#endif
                         MAP_SHARED, fd, 0)) == (caddr_t) -1)
         {
            (void)xrec(ERROR_DIALOG, "Failed to mmap() to %s : %s (%s %d)",
                       fullname, strerror(errno), __FILE__, __LINE__);
            (void)close(fd);
            return;
         }
         if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_JID_VERSION)
         {
            (void)xrec(ERROR_DIALOG,
                       "Incorrect JID version (data=%d current=%d)!",
                       *(ptr + SIZEOF_INT + 1 + 1 + 1), CURRENT_JID_VERSION);
            (void)close(fd);
            return;
         }
#ifdef HAVE_STATX
         jd_size = stat_buf.stx_size;
#else
         jd_size = stat_buf.st_size;
#endif
         no_of_jobs = *(int *)ptr;
         ptr += AFD_WORD_OFFSET;
         jd = (struct job_id_data *)ptr;
         (void)close(fd);
      }
      else
      {
         (void)xrec(ERROR_DIALOG, "Job ID database file is empty. (%s %d)",
                    __FILE__, __LINE__);
         (void)close(fd);
         return;
      }

      for (i = 0; i < no_of_jobs; i++)
      {
         if ((length + 1024) > MAX_INPUT_INFO_SIZE)
         {
            (void)xrec(WARN_DIALOG,
                       "Not enough memory to show all data. (%s %d)",
                       __FILE__, __LINE__);
            goto memory_full;
         }
         if ((qfl[pos].dir_id_pos == jd[i].dir_id_pos) &&
             ((qfl[pos].hostname[0] == '\0') ||
              ((qfl[pos].hostname[0] != '\0') &&
               (my_strcmp(qfl[pos].hostname, jd[i].host_alias) == 0))))
         {
            int  no_of_file_masks;
            char *p_file_list;

            get_file_mask_list(jd[i].file_mask_id, &no_of_file_masks,
                               &p_file_list);
            if (p_file_list != NULL)
            {
               int  gotcha = NO,
                    j;
               char *p_file;

               p_file = p_file_list;
               for (j = 0; j < no_of_file_masks; j++)
               {
                  if (sfilter(p_file, (char *)qfl[pos].file_name, 0) == 0)
                  {
                     gotcha = YES;
                     break;
                  }
                  NEXT(p_file);
               }

               if (gotcha == YES)
               {
                  char recipient[MAX_RECIPIENT_LENGTH];

                  if ((length + 1024) > MAX_INPUT_INFO_SIZE)
                  {
                     (void)xrec(WARN_DIALOG,
                                "Not enough memory to show all data. (%s %d)",
                                __FILE__, __LINE__);
                     goto memory_full;
                  }
                  if (jobs_found == 0)
                  {
                     if ((p_array = malloc(1 * sizeof(char *))) == NULL)
                     {
                        (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                                      strerror(errno), __FILE__, __LINE__);
                        exit(INCORRECT);
                     }
                  }
                  else
                  {
                     if ((p_array = realloc(p_array,
                                            ((jobs_found + 1) * sizeof(char *)))) == NULL)
                     {
                        (void)fprintf(stderr, "realloc() error : %s (%s %d)\n",
                                      strerror(errno), __FILE__, __LINE__);
                        exit(INCORRECT);
                     }
                  }
                  p_file = p_file_list;
                  count = sprintf(*text + length, "Filter     : %s\n", p_file);
                  NEXT(p_file);
                  length += count;
                  if (count > max_x)
                  {
                     max_x = count;
                  }
                  max_y++;
                  for (j = 1; j < no_of_file_masks; j++)
                  {
                     count = sprintf(*text + length, "             %s\n",
                                     p_file);
                     NEXT(p_file);
                     length += count;
                     if (count > max_x)
                     {
                        max_x = count;
                     }
                     max_y++;
                  }

                  /* Print recipient. */
                  (void)strcpy(recipient, jd[i].recipient);
                  if (perm.view_passwd == YES)
                  {
                     insert_passwd(recipient);
                  }
                  count = sprintf(*text + length, "Recipient  : %s\n",
                                  recipient);
                  length += count;
                  if (count > max_x)
                  {
                     max_x = count;
                  }
                  max_y++;
                  if (jd[i].no_of_loptions > 0)
                  {
                     char *p_tmp;

                     p_tmp = jd[i].loptions;
                     count = sprintf(*text + length, "AMG-options: %s\n",
                                     p_tmp);
                     NEXT(p_tmp);
                     length += count;
                     if (count > max_x)
                     {
                        max_x = count;
                     }
                     max_y++;
                     for (j = 1; j < jd[i].no_of_loptions; j++)
                     {
                        count = sprintf(*text + length, "             %s\n",
                                        p_tmp);
                        NEXT(p_tmp);
                        length += count;
                        if (count > max_x)
                        {
                           max_x = count;
                        }
                        max_y++;
                     }
                  }
                  if (jd[i].no_of_soptions == 1)
                  {
                     count = sprintf(*text + length, "FD-options : %s\n",
                                     jd[i].soptions);
                     length += count;
                     if (count > max_x)
                     {
                        max_x = count;
                     }
                     max_y++;
                  }
                  else if (jd[i].no_of_soptions > 1)
                       {
                          int  first = YES;
                          char *p_start,
                               *p_end,
                               *p_soptions;

                          if ((p_soptions = malloc(MAX_OPTION_LENGTH)) == NULL)
                          {
                             (void)xrec(ERROR_DIALOG,
                                        "malloc() erro : %s (%s %d)",
                                        strerror(errno), __FILE__, __LINE__);
                             (void)close(fd);
                             free((void *)p_array);
                             return;
                          }
                          (void)memcpy(p_soptions, jd[i].soptions,
                                       MAX_OPTION_LENGTH);
                          p_start = p_end = p_soptions;
                          do
                          {
                             while ((*p_end != '\n') && (*p_end != '\0'))
                             {
                                p_end++;
                             }
                             if (*p_end == '\n')
                             {
                                *p_end = '\0';
                                if (first == YES)
                                {
                                   first = NO;
                                   count = sprintf(*text + length,
                                                   "FD-options : %s\n",
                                                   p_start);
                                }
                                else
                                {
                                   count = sprintf(*text + length,
                                                   "             %s\n",
                                                   p_start);
                                }
                                length += count;
                                if (count > max_x)
                                {
                                   max_x = count;
                                }
                                max_y++;
                                p_end++;
                                p_start = p_end;
                             }
                          } while (*p_end != '\0');
                          count = sprintf(*text + length, "             %s\n",
                                          p_start);
                          length += count;
                          if (count > max_x)
                          {
                             max_x = count;
                          }
                          max_y++;
                          free(p_soptions);
                       }
                  count = sprintf(*text + length, "Priority   : %c\n",
                                  jd[i].priority);
                  length += count;
                  if (count > max_x)
                  {
                     max_x = count;
                  }
                  max_y++;

                  p_array[jobs_found] = *text + length;
                  jobs_found++;
               }
               free(p_file_list);
            }
         }
      }

memory_full:
      if (munmap(((char *)jd - AFD_WORD_OFFSET), jd_size) == -1)
      {
         (void)xrec(INFO_DIALOG, "munmap() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
      }
      *(*text + length - 1) = '\0';
   }
   if (munmap(((char *)dnb - AFD_WORD_OFFSET), dnb_size) == -1)
   {
      (void)xrec(INFO_DIALOG, "munmap() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
   }

   if (jobs_found > 0)
   {
      int i,
          j;

      (void)memmove(p_begin_underline + max_x + 1, p_begin_underline,
                    (length - (p_begin_underline - *text) + 1));
      (void)memset(p_begin_underline, '=', max_x);
      *(p_begin_underline + max_x) = '\n';
      max_y++;

      for (i = 0; i < (jobs_found - 1); i++)
      {
         for (j = i; j < (jobs_found - 1); j++)
         {
            p_array[j] += (max_x + 1);
         }
         length += (max_x + 1);
         (void)memmove(p_array[i] + max_x + 1, p_array[i],
                       (length - (p_array[i] - *text) + 1));
         (void)memset(p_array[i], '-', max_x);
         *(p_array[i] + max_x) = '\n';
         max_y++;
      }
   }

   if (jobs_found > 0)
   {
      free((void *)p_array);
   }

   return;
}


/*####################### format_retrieve_info() ########################*/
void
format_retrieve_info(char **text, int pos)
{
   int                 count,
                       fd,
                       length;
   off_t               dnb_size;
   char                fullname[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx        stat_buf;
#else
   struct stat         stat_buf;
#endif
   struct dir_name_buf *dnb;

   /* Map to directory name buffer. */
   (void)sprintf(fullname, "%s%s%s", p_work_dir, FIFO_DIR,
                 DIR_NAME_FILE);
   if ((fd = open(fullname, O_RDONLY)) == -1)
   {
      (void)xrec(ERROR_DIALOG, "Failed to open() <%s> : %s (%s %d)",
                 fullname, strerror(errno), __FILE__, __LINE__);
      return;
   }
#ifdef HAVE_STATX
   if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(fd, &stat_buf) == -1)
#endif
   {
      (void)xrec(ERROR_DIALOG, "Failed to access <%s> : %s (%s %d)",
                 fullname, strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      return;
   }
#ifdef HAVE_STATX
   if (stat_buf.stx_size > 0)
#else
   if (stat_buf.st_size > 0)
#endif
   {
      char *ptr;

      if ((ptr = mmap(NULL,
#ifdef HAVE_STATX
                      stat_buf.stx_size, PROT_READ,
#else
                      stat_buf.st_size, PROT_READ,
#endif
                      MAP_SHARED, fd, 0)) == (caddr_t) -1)
      {
         (void)xrec(ERROR_DIALOG, "Failed to mmap() to <%s> : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return;
      }
#ifdef HAVE_STATX
      dnb_size = stat_buf.stx_size;
#else
      dnb_size = stat_buf.st_size;
#endif
      ptr += AFD_WORD_OFFSET;
      dnb = (struct dir_name_buf *)ptr;
      (void)close(fd);
   }
   else
   {
      (void)xrec(ERROR_DIALOG, "Dirname database file is empty. (%s %d)",
                 __FILE__, __LINE__);
      (void)close(fd);
      return;
   }

   if ((*text = malloc(8192)) == NULL)
   {
      (void)xrec(FATAL_DIALOG, "Failed to malloc() 8192 bytes : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }

   /* Show hostname. */
   max_x = sprintf(*text, "Hostname   : %s\n", qfl[pos].hostname);
   length = max_x;

   count = sprintf(*text + length, "Dir-ID     : %x\n", qfl[pos].dir_id);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }

   count = sprintf(*text + length, "Dir Alias  : %s\n", qfl[pos].dir_alias);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y = 3;

   if (dnb[qfl[pos].dir_id_pos].dir_name[0] != '\0')
   {
      struct dir_options d_o;

      count = sprintf(*text + length, "Directory  : %s\n",
                      dnb[qfl[pos].dir_id_pos].dir_name);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;

      get_dir_options(qfl[pos].dir_id, &d_o);
      if (d_o.no_of_dir_options > 0)
      {
         int i;

         count = sprintf(*text + length, "DIR-options: %s\n",
                         d_o.aoptions[0]);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
         for (i = 1; i < d_o.no_of_dir_options; i++)
         {
            count = sprintf(*text + length, "             %s\n",
                            d_o.aoptions[i]);
            length += count;
            if (count > max_x)
            {
               max_x = count;
            }
            max_y++;
         }
      }
   }
   if (munmap(((char *)dnb - AFD_WORD_OFFSET), dnb_size) == -1)
   {
      (void)xrec(INFO_DIALOG, "munmap() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
   }

   return;
}
