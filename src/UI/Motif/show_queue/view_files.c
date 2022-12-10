/*
 *  view_files.c - Part of AFD, an automatic file distribution program.
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
 **   view_files - views files from AFD queue
 **
 ** SYNOPSIS
 **   void view_files(int no_selected, int *select_list)
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
 **   25.04.2007 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>         /* strerror()                                */
#include <stdlib.h>         /* calloc(), free()                          */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>       /* mmap(), munmap()                          */
#include <fcntl.h>          /* open()                                    */
#include <unistd.h>         /* close()                                   */
#include <errno.h>
#include <Xm/Xm.h>
#include <Xm/List.h>
#include "mafd_ctrl.h"
#include "show_queue.h"


/* External global variables. */
extern Display                 *display;
extern Widget                  appshell, /* CHECK_INTERRUPT() */
                               listbox_w,
                               scrollbar_w,
                               special_button_w,
                               statusbox_w;
extern XT_PTR_TYPE             toggles_set;
extern int                     special_button_flag;
extern char                    *p_work_dir;
extern struct queued_file_list *qfl;
extern struct queue_tmp_buf    *qtb;
extern struct sol_perm         perm;


/*############################## view_files() ###########################*/
void
view_files(int no_selected, int *select_list)
{
   int                 fd,
                       i,
                       length = 0,
                       no_done = 0,  /* Number done.             */
                       not_found = 0,
                       select_done = 0,
                       *select_done_list;
   off_t               dnb_size;
   char                fullname[MAX_PATH_LENGTH + 1 + MAX_HOSTNAME_LENGTH + 1 + MAX_FILENAME_LENGTH + 1],
                       user_message[256];
#ifdef HAVE_STATX
   struct statx        stat_buf;
#else
   struct stat         stat_buf;
#endif
   struct dir_name_buf *dnb;
   XmString            xstr;

   if ((select_done_list = calloc(no_selected, sizeof(int))) == NULL)
   {
      (void)xrec(FATAL_DIALOG, "calloc() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }

   /* Map to directory name buffer. */
   (void)sprintf(fullname, "%s%s%s", p_work_dir, FIFO_DIR, DIR_NAME_FILE);
   if ((fd = open(fullname, O_RDONLY)) == -1)
   {
      (void)xrec(ERROR_DIALOG, "Failed to open() `%s' : %s (%s %d)",
                 fullname, strerror(errno), __FILE__, __LINE__);
      free((void *)select_done_list);
      return;
   }
#ifdef HAVE_STATX
   if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(fd, &stat_buf) == -1)
#endif
   {
      (void)xrec(ERROR_DIALOG, "Failed to access `%s' : %s (%s %d)",
                 fullname, strerror(errno), __FILE__, __LINE__);
      free((void *)select_done_list);
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
         (void)xrec(ERROR_DIALOG, "Failed to mmap() to `%s' : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
         free((void *)select_done_list);
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
      (void)xrec(ERROR_DIALOG,
                 "Dirname database file is empty. (%s %d)", __FILE__, __LINE__);
      free((void *)select_done_list);
      (void)close(fd);
      return;
   }

   /* Block all input and change button name. */
   special_button_flag = STOP_BUTTON;
   xstr = XmStringCreateLtoR("Stop", XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);
   CHECK_INTERRUPT();

   /*
    * View file by file, until we reached MAX_VIEW_DATA_WINDOWS.
    */
   for (i = 0; i < no_selected; i++)
   {
      /* Check if the file is still in the queue. */
      fullname[0] = '\0';
      if (((qfl[select_list[i] - 1].queue_type == SHOW_OUTPUT) &&
           (toggles_set & SHOW_OUTPUT)) ||
          (qfl[select_list[i] - 1].queue_type == SHOW_UNSENT_OUTPUT))
      {
         if (qfl[select_list[i] - 1].queue_tmp_buf_pos != -1)
         {
            (void)sprintf(fullname, "%s%s%s/%s/%s",
                          p_work_dir, AFD_FILE_DIR, OUTGOING_DIR,
                          qtb[qfl[select_list[i] - 1].queue_tmp_buf_pos].msg_name,
                          qfl[select_list[i] - 1].file_name);
#ifdef HAVE_STATX
            if (statx(0, fullname, AT_STATX_SYNC_AS_STAT, 0, &stat_buf) == -1)
#else
            if (stat(fullname, &stat_buf) == -1)
#endif
            {
               not_found++;
            }
         }
      }
      else if (qfl[select_list[i] - 1].queue_type == SHOW_TIME_JOBS)
           {
              (void)sprintf(fullname, "%s%s%s/%x/%s",
                            p_work_dir, AFD_FILE_DIR, AFD_TIME_DIR,
                            qfl[select_list[i] - 1].job_id,
                            qfl[select_list[i] - 1].file_name);
#ifdef HAVE_STATX
              if (statx(0, fullname, AT_STATX_SYNC_AS_STAT,
                        0, &stat_buf) == -1)
#else
              if (stat(fullname, &stat_buf) == -1)
#endif
              {
                 not_found++;
              }
           }
      else if ((qfl[select_list[i] - 1].queue_type != SHOW_RETRIEVES) &&
               (qfl[select_list[i] - 1].queue_type != SHOW_PENDING_RETRIEVES))
           {
              if (qfl[select_list[i] - 1].hostname[0] == '\0')
              {
                 (void)sprintf(fullname, "%s/%s",
                               dnb[qfl[select_list[i] - 1].dir_id_pos].dir_name,
                               qfl[select_list[i] - 1].file_name);
              }
              else
              {
                 (void)sprintf(fullname, "%s/.%s/%s",
                               dnb[qfl[select_list[i] - 1].dir_id_pos].dir_name,
                               qfl[select_list[i] - 1].hostname,
                               qfl[select_list[i] - 1].file_name);
              }
#ifdef HAVE_STATX
              if (statx(0, fullname, AT_STATX_SYNC_AS_STAT,
                        0, &stat_buf) == -1)
#else
              if (stat(fullname, &stat_buf) == -1)
#endif
              {
                 not_found++;
              }
           }

      if (fullname[0] != '\0')
      {
         view_data(fullname, (char *)qfl[select_list[i] - 1].file_name);
         no_done++;
         select_done_list[select_done] = select_list[i];
         select_done++;
         if (select_done >= MAX_VIEW_DATA_WINDOWS)
         {
            i = no_selected;
         }
      }

      CHECK_INTERRUPT();
      if (special_button_flag == STOP_BUTTON_PRESSED)
      {
         break;
      }
   } /* for (i = 0; i < no_selected; i++) */

   if (munmap(((char *)dnb - AFD_WORD_OFFSET), dnb_size) == -1)
   {
      (void)xrec(INFO_DIALOG, "munmap() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
   }

   if (select_done > 0)
   {
      for (i = 0; i < select_done; i++)
      {
         XmListDeselectPos(listbox_w, select_done_list[i]);
      }
   }
   free((void *)select_done_list);

   /* Show user a summary of what was done. */
   if (no_done > 0)
   {
      if (no_done == 1)
      {
         length = sprintf(user_message, "1 file shown");
      }
      else
      {
         length = sprintf(user_message, "%d files shown", no_done);
      }
   }
   if (not_found > 0)
   {
      if (length > 0)
      {
         length += sprintf(&user_message[length], ", %d not found", not_found);
      }
      else
      {
         length = sprintf(user_message, "%d not found", not_found);
      }
   }
   show_message(statusbox_w, user_message);

   /* Button back to normal. */
   special_button_flag = SEARCH_BUTTON;
   xstr = XmStringCreateLtoR("Search", XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);

   return;
}
