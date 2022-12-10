/*
 *  send_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   send_files - sends files from the AFD queue to some host
 **
 ** SYNOPSIS
 **   void send_files(int no_selected, int *select_list)
 **
 ** DESCRIPTION
 **   send_files() will send all files selected in the show_queue
 **   dialog to some destination, with the help of the xsend_file
 **   dialog. Only files that are in the queue will be send.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   07.02.2005 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <Xm/Xm.h>
#include <Xm/List.h>
#include "mafd_ctrl.h"
#include "show_queue.h"

/* External global variables. */
extern Display                 *display;
extern Widget                  listbox_w,
                               statusbox_w;
extern XT_PTR_TYPE             toggles_set;
extern char                    font_name[],
                               *p_work_dir;
extern struct queued_file_list *qfl;
extern struct queue_tmp_buf    *qtb;
extern struct sol_perm         perm;


/*############################## send_files() ###########################*/
void
send_files(int no_selected, int *select_list)
{
   int                 fd,
                       i,
                       length = 0,
                       limit_reached = 0,
                       not_found = 0,
                       to_do = 0;    /* Number still to be done. */
   off_t               dnb_size;
   uid_t               euid, /* Effective user ID. */
                       ruid; /* Real user ID. */
   char                *args[7],
                       file_name_file[MAX_PATH_LENGTH],
                       fullname[MAX_PATH_LENGTH + 1 + MAX_HOSTNAME_LENGTH + 1 + MAX_FILENAME_LENGTH + 1],
                       user_message[256];
   FILE                *fp;
#ifdef HAVE_STATX
   struct statx        stat_buf;
#else
   struct stat         stat_buf;
#endif
   struct dir_name_buf *dnb;
   static int          user_limit = 0;
   static unsigned int counter = 0;

   if ((perm.send_limit > 0) && (user_limit >= perm.send_limit))
   {
      (void)sprintf(user_message, "User limit (%d) for sending reached!",
                    perm.send_limit);
      show_message(statusbox_w, user_message);
      return;
   }

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

#if SIZEOF_PID_T == 4
   (void)sprintf(file_name_file, ".file_name_file.%d.%d",
#else
   (void)sprintf(file_name_file, ".file_name_file.%lld.%d",
#endif
                 (pri_pid_t)getpid(), counter);
   counter++;

   euid = geteuid();
   ruid = getuid();
   if (euid != ruid)
   {
      if (seteuid(ruid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s (%s %d)\n",
                       ruid, strerror(errno), __FILE__, __LINE__);
      }
   }
   if ((fp = fopen(file_name_file, "w")) == NULL)
   {
      (void)fprintf(stderr, "Failed to fopen() %s : %s (%s %d)\n",
                    file_name_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (euid != ruid)
   {
      if (seteuid(euid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s (%s %d)\n",
                       euid, strerror(errno), __FILE__, __LINE__);
      }
   }

   for (i = 0; i < no_selected; i++)
   {
      if (((qfl[select_list[i] - 1].queue_type == SHOW_OUTPUT) &&
           (toggles_set & SHOW_OUTPUT)) ||
          (qfl[select_list[i] - 1].queue_type == SHOW_UNSENT_OUTPUT))
      {
         if (qfl[select_list[i] - 1].queue_tmp_buf_pos != -1)
         {
            if ((perm.send_limit > 0) &&
                ((user_limit + to_do) >= perm.send_limit))
            {
               limit_reached++;
            }
            else
            {
               (void)sprintf(fullname, "%s%s%s/%s/%s",
                             p_work_dir, AFD_FILE_DIR, OUTGOING_DIR,
                             qtb[qfl[select_list[i] - 1].queue_tmp_buf_pos].msg_name,
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
               else
               {
                  (void)fprintf(fp, "%s|%s\n",
                                fullname, qfl[select_list[i] - 1].file_name);
                  to_do++;
               }
            }
         }
      }
      else if (qfl[select_list[i] - 1].queue_type == SHOW_TIME_JOBS)
           {
              if ((perm.send_limit > 0) &&
                  ((user_limit + to_do) >= perm.send_limit))
              {
                 limit_reached++;
              }
              else
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
                 else
                 {
                    (void)fprintf(fp, "%s\n", fullname);
                    to_do++;
                 }
              }
           }
      else if ((qfl[select_list[i] - 1].queue_type != SHOW_RETRIEVES) &&
               (qfl[select_list[i] - 1].queue_type != SHOW_PENDING_RETRIEVES))
           {
              if ((perm.send_limit > 0) &&
                  ((user_limit + to_do) >= perm.send_limit))
              {
                 limit_reached++;
              }
              else
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
                 else
                 {
                    (void)fprintf(fp, "%s\n", fullname);
                    to_do++;
                 }
              }
           }
   }

   if (fclose(fp) == EOF)
   {
      (void)fprintf(stderr, "Failed to fclose() %s : %s (%s %d)\n",
                    file_name_file, strerror(errno), __FILE__, __LINE__);
   }

   if (munmap(((char *)dnb - AFD_WORD_OFFSET), dnb_size) == -1)
   {
      (void)xrec(INFO_DIALOG, "munmap() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
   }

   if (euid != ruid)
   {
      if (seteuid(ruid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s (%s %d)\n",
                       ruid, strerror(errno), __FILE__, __LINE__);
      }
   }
   if (to_do > 0)
   {
      char progname[XSEND_FILE_LENGTH + 1];

      (void)strcpy(progname, XSEND_FILE);
      args[0] = progname;
      args[1] = WORK_DIR_ID;
      args[2] = p_work_dir;
      args[3] = "-f";
      args[4] = font_name;
      args[5] = file_name_file;
      args[6] = NULL;
      make_xprocess(progname, progname, args, -1);
   }
   else
   {
      (void)unlink(file_name_file);
   }
   if (euid != ruid)
   {
      if (seteuid(euid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s (%s %d)\n",
                       euid, strerror(errno), __FILE__, __LINE__);
      }
   }

   /* Show user a summary of what was done. */
   user_message[0] = ' ';
   user_message[1] = '\0';
   if (to_do > 0)
   {
      if (to_do == 1)
      {
         length = sprintf(user_message, "1 file to be send");
      }
      else
      {
         length = sprintf(user_message, "%d files to be send", to_do);
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
   if (limit_reached > 0)
   {
      if (length > 0)
      {
         length += sprintf(&user_message[length], ", %d not send due to limit",
                           limit_reached);
      }
      else
      {
         length = sprintf(user_message, "%d not send due to limit",
                          limit_reached);
      }
   }
   show_message(statusbox_w, user_message);

   return;
}
