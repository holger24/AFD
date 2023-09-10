/*
 *  init_text.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   16.03.1996 H.Kiehl Created
 **   31.05.1997 H.Kiehl Added debug toggle.
 **   27.12.2003 H.Kiehl Added trace toggle.
 **   20.09.2022 H.Kiehl Replace unprintable characters with dot (.) sign.
 **   24.08.2023 H.Kiehl Read file in hunks advertised by system
 **                      (st_blksize).
 **   27.08.2023 H.Kiehl If file is larger then what XmTextGetMaxLength()
 **                      returns, just read the last number of bytes of
 **                      the returned value.
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>     /* calloc(), free()                              */
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>     /* Definition of AT_* constants                  */
#endif
#include <sys/stat.h>
#include <unistd.h>     /* lseek()                                       */
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <errno.h>
#include "mafd_ctrl.h"
#include "mshow_log.h"
#include "logdefs.h"

#define HUNK_SIZE 268435456 /* 256 MiB */

extern Display        *display;
extern XtAppContext   app;
extern Cursor         cursor1;
extern Widget         appshell,
                      log_output;
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
static void           read_text(ino_t *);


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
            ino_t inode_no;

            read_text(&inode_no);
            if ((log_type_flag != TRANSFER_LOG_TYPE) &&
                (log_type_flag != RECEIVE_LOG_TYPE) && (i == 0))
            {
               current_inode_no = inode_no;
            }
         }
      }
   }
   else
   {
      ino_t inode_no;

      read_text(&inode_no);
      if ((log_type_flag != TRANSFER_LOG_TYPE) &&
          (log_type_flag != RECEIVE_LOG_TYPE) && (current_log_number == 0))
      {
         current_inode_no = inode_no;
      }
   }
   XmTextShowPosition(log_output, wpr_position);

   return;
}


/*############################## read_text() ############################*/
static void
read_text(ino_t *inode_no)
{
   int                  fd = fileno(p_log_file);
   char                 *src = NULL,
                        *dst = NULL,
                        *ptr,
                        *ptr_dst,
                        *ptr_start,
                        *ptr_line;
#ifdef HAVE_STATX
   struct statx         stat_buf;
#else
   struct stat          stat_buf;
#endif
   XSetWindowAttributes attrs;
   XEvent               event;

#ifdef HAVE_STATX
   if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(fd, &stat_buf) == -1)
#endif
   {
      (void)xrec(FATAL_DIALOG, "Failed to access log file : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }

#ifdef HAVE_STATX
   if (stat_buf.stx_size > 0)
#else
   if (stat_buf.st_size > 0)
#endif
   {
      int     length,
              last = MISS,
              max_hunk = XmTextGetMaxLength(log_output);
      char    *read_ptr;
      ssize_t bytes_read;
      size_t  block_length,
              read_size;
      off_t   bytes_buffered = 0,
              length_to_show = 0,
              size,
              tmp_total_length = 0;

      /* XmText cannot display size bigger then max_hunk. */
#ifdef HAVE_STATX
      if (stat_buf.stx_size > max_hunk)
#else
      if (stat_buf.st_size > max_hunk)
#endif
      {
#ifdef HAVE_STATX
         if (lseek(fd, stat_buf.stx_size - (max_hunk - 4096), SEEK_SET) == -1)
#else
         if (lseek(fd, stat_buf.st_size - (max_hunk - 4096), SEEK_SET) == -1)
#endif
         {
            (void)xrec(FATAL_DIALOG, "Failed to lseek() in log file : %s (%s %d)",
                       strerror(errno), __FILE__, __LINE__);
            return;
         }
         size = max_hunk - 4096;
      }
      else
      {
#ifdef HAVE_STATX
         size = stat_buf.stx_size;
#else
         size = stat_buf.st_size;
#endif
      }

      /* Change cursor to indicate we are doing something. */
      attrs.cursor = cursor1;
      XChangeWindowAttributes(display, XtWindow(appshell), CWCursor, &attrs);
      XFlush(display);

      /* Copy file to memory. */
      if ((src = malloc(size)) == NULL)
      {
         (void)xrec(FATAL_DIALOG, "malloc() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }
      read_ptr = src;
      while (size > 0)
      {
         if (size > HUNK_SIZE)
         {
            read_size = HUNK_SIZE;
         }
         else
         {
            read_size = size;
         }
         if ((bytes_read = read(fd, read_ptr, read_size)) == -1)
         {
            int tmp_errno = errno;

            free(src);
            (void)xrec(FATAL_DIALOG, "read() error : %s (%s %d)",
                       strerror(tmp_errno), __FILE__, __LINE__);
            return;
         }
         bytes_buffered += bytes_read;
         read_ptr += bytes_read;
         size -= bytes_read;
      }
      if ((dst = malloc(bytes_buffered + 1)) == NULL)
      {
         free(src);
         free(dst);
#if SIZEOF_OFF_T == 4
         (void)xrec(FATAL_DIALOG, "malloc() error [%d bytes] : %s (%s %d)",
#else
         (void)xrec(FATAL_DIALOG, "malloc() error [%lld bytes] : %s (%s %d)",
#endif
                    (pri_off_t)(bytes_buffered + 1), strerror(errno),
                    __FILE__, __LINE__);
         return;
      }

      ptr_start = ptr = src;
      ptr_dst = dst;
      block_length = 0;
      *ptr_dst = '\0';

      if (no_of_hosts > 0)
      {
         int i;

         while (tmp_total_length < bytes_buffered)
         {
            length = 0;
            ptr_line = ptr;
            while ((*ptr != '\n') &&
                   ((tmp_total_length + length) < bytes_buffered))
            {
               /* Remove unprintable characters. */
               if ((unsigned char)(*ptr) < ' ')
               {
                  *ptr = '.';
               }
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
                    (((toggles_set_parallel_jobs - 1) != (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) - 48)) &&
                    (toggles_set_parallel_jobs != 0))))
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
                     length_to_show += length;
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
                     length_to_show += length;
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
         while (tmp_total_length < bytes_buffered)
         {
            length = 0;
            ptr_line = ptr;
            while ((*ptr != '\n') &&
                   ((tmp_total_length + length) < bytes_buffered))
            {
               /* Remove unprintable characters. */
               if ((unsigned char)(*ptr) < ' ')
               {
                  *ptr = '.';
               }
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
                    (((toggles_set_parallel_jobs - 1) != (*(ptr_line + LOG_SIGN_POSITION + MAX_HOSTNAME_LENGTH + 4) - 48)) &&
                    (toggles_set_parallel_jobs != 0))))
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
                  length_to_show += length;
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
                  length_to_show += length;
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

      free(src);

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
      wpr_position += length_to_show;
      total_length += length_to_show;
      free((void *)dst);

      attrs.cursor = None;
      XChangeWindowAttributes(display, XtWindow(appshell), CWCursor, &attrs);
      XFlush(display);

      /* Get rid of all events that have occurred. */
      XSync(display, False);
      while (XCheckMaskEvent(display,
                             ButtonPressMask | ButtonReleaseMask |
                             ButtonMotionMask | PointerMotionMask |
                             KeyPressMask, &event) == TRUE)
      {
         /* do nothing */;
      }
   }

   if (inode_no != NULL)
   {
#ifdef HAVE_STATX
      *inode_no = stat_buf.stx_ino;
#else
      *inode_no = stat_buf.st_ino;
#endif
   }

   return;
}
