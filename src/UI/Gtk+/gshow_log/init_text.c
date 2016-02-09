/*
 *  init_text.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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

DESCR__S_M1
/*
 ** NAME
 **   init_text - Initializes text to be displayed
 **
 ** SYNOPSIS
 **   void init_text(void)
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
 **   01.01.2009 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>     /* calloc(), free()                              */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>  /* mmap(), munmap()                              */
#endif
#include <unistd.h>

#include <gtk/gtk.h>
#include <errno.h>
#include "gafd_ctrl.h"
#include "gshow_log.h"
#include "logdefs.h"

extern GtkWidget      *log_output;
extern XmTextPosition wpr_position;
extern int            current_log_number,
                      log_type_flag,
                      line_counter,
                      max_log_number,
                      no_of_hosts;
extern XT_PTR_TYPE    toggles_set;
extern char           **hosts,
                      log_dir[],
                      log_name[];
extern off_t          total_length;
extern ino_t          current_inode_no;
extern unsigned int   toggles_set_parallel_jobs;
extern FILE           *p_log_file;

/* Local function prototypes. */
static void           read_text(void);


/*############################## init_text() ############################*/
void
init_text(void)
{
   wpr_position = 0;
   XmTextSetString(log_output, NULL);
   XmTextSetInsertionPosition(log_output, 0);
   if (current_log_number == -1)
   {
      int  i;
      char log_file[MAX_PATH_LENGTH];

      for (i = max_log_number; i >= 0; i--)
      {
         if (p_log_file != NULL)
         {
            (void)fclose(p_log_file);
         }
         (void)sprintf(log_file, "%s/%s%d", log_dir, log_name, i);
         if ((p_log_file = fopen(log_file, "r")) == NULL)
         {
            if (errno != ENOENT)
            {
               (void)xrec(FATAL_DIALOG, "Could not fopen() %s : %s (%s %d)",
                          log_file, strerror(errno), __FILE__, __LINE__);
               return;
            }
         }
         else
         {
            read_text();
            if ((log_type_flag != TRANSFER_LOG_TYPE) &&
                (log_type_flag != RECEIVE_LOG_TYPE) &&
                (i == 0))
            {
               struct stat stat_buf;

               if (fstat(fileno(p_log_file), &stat_buf) == -1)
               {
                  (void)fprintf(stderr,
                                "ERROR   : Could not fstat() %s : %s (%s %d)\n",
                                log_file, strerror(errno), __FILE__, __LINE__);
                  exit(INCORRECT);
               }
               current_inode_no = stat_buf.st_ino;
            }
         }
      }
   }
   else
   {
      read_text();
   }
   XmTextShowPosition(log_output, wpr_position);

   return;
}


/*############################## read_text() ############################*/
static void
read_text(void)
{
   int                  i,
                        fd = fileno(p_log_file),
                        length,
                        block_length,
                        last = MISS;
   off_t                tmp_total_length = 0;
   char                 *src = NULL,
                        *dst = NULL,
                        *ptr,
                        *ptr_dst,
                        *ptr_start,
                        *ptr_line;
   struct stat          stat_buf;
   XSetWindowAttributes attrs;
   XEvent               event;

   if (fstat(fd, &stat_buf) < 0)
   {
      (void)xrec(FATAL_DIALOG, "fstat() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }

   if (stat_buf.st_size > 0)
   {
      /* Copy file to memory. */
#ifdef HAVE_MMAP
      if (lseek(fd, stat_buf.st_size, SEEK_SET) == -1)
      {
         (void)xrec(FATAL_DIALOG, "lseek() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }
      if ((src = mmap(NULL, stat_buf.st_size, PROT_READ, MAP_SHARED,
                      fd, 0)) == (caddr_t) -1)
      {
         (void)xrec(FATAL_DIALOG, "mmap() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }
#else
      if ((src = malloc(stat_buf.st_size)) == NULL)
      {
         (void)xrec(FATAL_DIALOG, "malloc() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }
      if (read(fd, src, stat_buf.st_size) != stat_buf.st_size)
      {
         int tmp_errno = errno;

         (void)free(src);
         (void)xrec(FATAL_DIALOG, "read() error : %s (%s %d)",
                    strerror(tmp_errno), __FILE__, __LINE__);
         return;
      }
#endif
      if ((dst = malloc(stat_buf.st_size + 1)) == NULL)
      {
         (void)xrec(FATAL_DIALOG, "malloc() error [%d bytes] : %s (%s %d)",
                    stat_buf.st_size + 1, strerror(errno), __FILE__, __LINE__);
         return;
      }

      ptr_start = ptr = src;
      ptr_dst = dst;
      block_length = 0;
      *ptr_dst = '\0';

      if (no_of_hosts > 0)
      {
         while (tmp_total_length < stat_buf.st_size)
         {
            length = 0;
            ptr_line = ptr;
            while ((*ptr != '\n') && (*ptr != '\0'))
            {
               length++; ptr++;
            }
            length++; ptr++;
            tmp_total_length += length;
            if ((log_type_flag == TRANSFER_LOG_TYPE) ||
                (log_type_flag == TRANS_DB_LOG_TYPE))
            {
               if ((length > (LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4)) &&
                   ((((toggles_set & SHOW_INFO) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'I')) ||
                    (((toggles_set & SHOW_WARN) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'W')) ||
                    (((toggles_set & SHOW_ERROR) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'E')) ||
                    (((toggles_set & SHOW_FATAL) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'F')) ||
                    (((toggles_set & SHOW_OFFLINE) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'O')) ||
                    (((toggles_set & SHOW_DEBUG) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'D')) ||
                    (((toggles_set & SHOW_TRACE) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'T')) ||
#ifdef _TOGGLED_PROC_SELECTION
                    (((toggles_set_parallel_jobs & 1) == 0) && (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) == '0')) ||
                    (((toggles_set_parallel_jobs & 2) == 0) && (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) == '1')) ||
                    (((toggles_set_parallel_jobs & 4) == 0) && (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) == '2')) ||
                    (((toggles_set_parallel_jobs & 8) == 0) && (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) == '3')) ||
                    (((toggles_set_parallel_jobs & 16) == 0) && (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) == '4'))))
#else
                    (((toggles_set_parallel_jobs - 1) != (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) - 48)) &&
                    (toggles_set_parallel_jobs != 0))))
#endif
               {
                  if (last == HIT)
                  {
                     (void)memcpy(ptr_dst, ptr_start, block_length);
                     *(ptr_dst + block_length) = '\0';
                     ptr_dst += block_length;
                     block_length = 0;
                  }
                  last = MISS;
               }
               else
               {
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     if (log_filter(hosts[i], ptr_line + 16) == 0)
                     {
                        i = no_of_hosts + 1;
                     }
                  }
                  if (i == (no_of_hosts + 2))
                  {
                     if (last == MISS)
                     {
                        ptr_start = ptr - length;
                     }
                     block_length += length;
                     line_counter++;
                     last = HIT;
                  }
                  else
                  {
                     if (last == HIT)
                     {
                        memcpy(ptr_dst, ptr_start, block_length);
                        *(ptr_dst + block_length) = '\0';
                        ptr_dst += block_length;
                        block_length = 0;
                     }
                     last = MISS;
                  }
               }
            }
            else
            {
               if ((length > LOG_SIGN_POSITION) &&
                   ((((toggles_set & SHOW_INFO) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'I')) ||
                    (((toggles_set & SHOW_CONFIG) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'C')) ||
                    (((toggles_set & SHOW_WARN) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'W')) ||
                    (((toggles_set & SHOW_ERROR) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'E')) ||
                    (((toggles_set & SHOW_FATAL) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'F')) ||
                    (((toggles_set & SHOW_OFFLINE) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'O')) ||
                    (((toggles_set & SHOW_DEBUG) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'D'))))
               {
                  if (last == HIT)
                  {
                     (void)memcpy(ptr_dst, ptr_start, block_length);
                     *(ptr_dst + block_length) = '\0';
                     ptr_dst += block_length;
                     block_length = 0;
                  }
                  last = MISS;
               }
               else
               {
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     if (log_filter(hosts[i], ptr_line + 16) == 0)
                     {
                        i = no_of_hosts + 1;
                     }
                  }
                  if (i == (no_of_hosts + 2))
                  {
                     if (last == MISS)
                     {
                        ptr_start = ptr - length;
                     }
                     block_length += length;
                     line_counter++;
                     last = HIT;
                  }
                  else
                  {
                     if (last == HIT)
                     {
                        memcpy(ptr_dst, ptr_start, block_length);
                        *(ptr_dst + block_length) = '\0';
                        ptr_dst += block_length;
                        block_length = 0;
                     }
                     last = MISS;
                  }
               }
            }
         }
      }
      else
      {
         while (tmp_total_length < stat_buf.st_size)
         {
            length = 0;
            ptr_line = ptr;
            while ((*ptr != '\n') && (*ptr != '\0'))
            {
               length++; ptr++;
            }
            length++; ptr++;
            tmp_total_length += length;
            if ((log_type_flag == TRANSFER_LOG_TYPE) ||
                (log_type_flag == TRANS_DB_LOG_TYPE))
            {
               if ((length > (LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4)) &&
                   ((((toggles_set & SHOW_INFO) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'I')) ||
                    (((toggles_set & SHOW_WARN) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'W')) ||
                    (((toggles_set & SHOW_ERROR) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'E')) ||
                    (((toggles_set & SHOW_FATAL) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'F')) ||
                    (((toggles_set & SHOW_OFFLINE) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'O')) ||
                    (((toggles_set & SHOW_DEBUG) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'D')) ||
                    (((toggles_set & SHOW_TRACE) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'T')) ||
#ifdef _TOGGLED_PROC_SELECTION
                    (((toggles_set_parallel_jobs & 1) == 0) && (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) == '0')) ||
                    (((toggles_set_parallel_jobs & 2) == 0) && (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) == '1')) ||
                    (((toggles_set_parallel_jobs & 4) == 0) && (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) == '2')) ||
                    (((toggles_set_parallel_jobs & 8) == 0) && (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) == '3')) ||
                    (((toggles_set_parallel_jobs & 16) == 0) && (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) == '4'))))
#else
                    (((toggles_set_parallel_jobs - 1) != (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) - 48)) &&
                    (toggles_set_parallel_jobs != 0))))
#endif
               {
                  if (last == HIT)
                  {
                     memcpy(ptr_dst, ptr_start, block_length);
                     *(ptr_dst + block_length) = '\0';
                     ptr_dst += block_length;
                     block_length = 0;
                  }
                  last = MISS;
               }
               else
               {
                  if (last == MISS)
                  {
                     ptr_start = ptr - length;
                  }
                  block_length += length;
                  line_counter++;
                  last = HIT;
               }
            }
            else
            {
               if ((length > LOG_SIGN_POSITION) &&
                   ((((toggles_set & SHOW_INFO) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'I')) ||
                    (((toggles_set & SHOW_CONFIG) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'C')) ||
                    (((toggles_set & SHOW_WARN) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'W')) ||
                    (((toggles_set & SHOW_ERROR) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'E')) ||
                    (((toggles_set & SHOW_FATAL) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'F')) ||
                    (((toggles_set & SHOW_OFFLINE) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'O')) ||
                    (((toggles_set & SHOW_DEBUG) == 0) && (*(ptr_line + LOG_SIGN_POSITION) == 'D'))))
               {
                  if (last == HIT)
                  {
                     memcpy(ptr_dst, ptr_start, block_length);
                     *(ptr_dst + block_length) = '\0';
                     ptr_dst += block_length;
                     block_length = 0;
                  }
                  last = MISS;
               }
               else
               {
                  if (last == MISS)
                  {
                     ptr_start = ptr - length;
                  }
                  block_length += length;
                  line_counter++;
                  last = HIT;
               }
            }
         }
      }
      if (block_length > 0)
      {
         memcpy(ptr_dst, ptr_start, block_length);
         *(ptr_dst + block_length) = '\0';
      }

#ifdef HAVE_MMAP
      if (munmap(src, stat_buf.st_size) < 0)
      {
         (void)xrec(WARN_DIALOG, "munmap() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
      }
#else
      free(src);
#endif

      if (wpr_position == 0)
      {
         XmTextSetString(log_output, dst);
      }
      else
      {
#ifndef LESSTIF_WORKAROUND
         XtUnmanageChild(log_output);
#endif
         XmTextInsert(log_output, wpr_position, dst);
#ifndef LESSTIF_WORKAROUND
         XtManageChild(log_output);
#endif
      }
      wpr_position += tmp_total_length;
      total_length += tmp_total_length;
      free((void *)dst);
   }

   return;
}
