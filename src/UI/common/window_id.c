/*
 *  window_id.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2004 - 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   handle_setup_file - reads or writes the initial setup file
 **
 ** SYNOPSIS
 **   void   write_window_id(Window w, pid_t pid, char *progname)
 **   Window get_window_id(pid_t pid, char *progname)
 **   void   remove_window_id(pid_t pid, char *progname)
 **   void   check_window_ids(char *progname)
 **
 ** DESCRIPTION
 **   write_window_id() writes the window ID and pid to a common
 **   file so that controling window can raise the window w when
 **   it is called again.
 **
 **   The function get_window_id() searches in the list for the
 **   given pid and if found tests if the process is still active, if
 **   it is active it will return its window ID if not it will return
 **   zero.
 **
 **   remove_window_id() removes the window ID from the common
 **   window id file.
 **
 **   check_window_ids() will go through the current window list and
 **   check if all process are still active. If not it will just remove
 **   them from the list.
 **
 **   Since all function lock the setup file, there is no
 **   problem when two users with the same home directory read
 **   or write that file.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   05.08.2004 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                  /* strcpy(), strcat(), strcmp()     */
#include <stdlib.h>                  /* getenv()                         */
#include <sys/types.h>
#include <signal.h>                  /* kill()                           */
#include <sys/stat.h>
#include <unistd.h>                  /* ftruncate(), geteuid(), getuid() */
#include <fcntl.h>
#include <errno.h>
#include "ui_common_defs.h"

#define AFD_WINDOW_ID_FILE ".afd_window_ids"

/* External global variables. */
extern int  sys_log_fd;
extern char *p_work_dir;

/* Local global variables. */
static char window_id_file[MAX_PATH_LENGTH] = { 0 };

/* Local function prototypes. */
static void get_window_id_name(void);


/*########################### write_window_id() #########################*/
void
write_window_id(Window w, pid_t pid, char *progname)
{
   int               fd,
                     *no_of_windows,
                     tmp_sys_log_fd;
   size_t            new_size;
   uid_t             euid, /* Effective user ID. */
                     ruid; /* Real user ID. */
   char              *ptr;
   struct window_ids *wl;

   if (window_id_file[0] == '\0')
   {
      get_window_id_name();
   }
   new_size = (DEFAULT_WINDOW_ID_STEPSIZE * sizeof(struct window_ids)) +
              AFD_WORD_OFFSET;
   euid = geteuid();
   ruid = getuid();
   if (euid != ruid)
   {
      if (seteuid(ruid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s\n",
                       ruid, strerror(errno));
      }
   }
   tmp_sys_log_fd = sys_log_fd;
   sys_log_fd = STDERR_FILENO;
   ptr = attach_buf(window_id_file, &fd, &new_size, progname,
                    S_IRUSR | S_IWUSR, YES);
   sys_log_fd = tmp_sys_log_fd;
   if (euid != ruid)
   {
      if (seteuid(euid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s\n",
                       euid, strerror(errno));
      }
   }
   if (ptr == (caddr_t) -1)
   {
      (void)fprintf(stderr, "Failed to mmap() to %s : %s\n",
                    window_id_file, strerror(errno));
      window_id_file[0] = '\0';
      return;
   }
   no_of_windows = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   wl = (struct window_ids *)ptr;

   if ((*no_of_windows != 0) &&
       ((*no_of_windows % DEFAULT_WINDOW_ID_STEPSIZE) == 0))
   {
      new_size = (((*no_of_windows / DEFAULT_WINDOW_ID_STEPSIZE) + 1) *
                   DEFAULT_WINDOW_ID_STEPSIZE * sizeof(struct window_ids)) +
                 AFD_WORD_OFFSET;
      ptr -= AFD_WORD_OFFSET;
      if ((ptr = mmap_resize(fd, ptr, new_size)) == (caddr_t) -1)
      {
         (void)fprintf(stderr, "Failed to mmap_resize() file %s : %s\n",
                       window_id_file, strerror(errno));
         exit(INCORRECT);
      }
      no_of_windows = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      wl = (struct window_ids *)ptr;
   }
   wl[*no_of_windows].pid = pid;
   wl[*no_of_windows].window_id = w;
   (*no_of_windows)++;

   unmap_data(fd, (void *)&wl);

   return;
}


/*############################ get_window_id() ##########################*/
Window
get_window_id(pid_t pid, char *progname)
{
   int               fd,
                     i,
                     *no_of_windows,
                     tmp_sys_log_fd;
   size_t            new_size;
   uid_t             euid, /* Effective user ID. */
                     ruid; /* Real user ID. */
   char              *ptr;
   Window            window_id = 0L;
   struct window_ids *wl;

   if (window_id_file[0] == '\0')
   {
      get_window_id_name();
   }
   new_size = (DEFAULT_WINDOW_ID_STEPSIZE * sizeof(struct window_ids)) +
              AFD_WORD_OFFSET;
   euid = geteuid();
   ruid = getuid();
   if (euid != ruid)
   {
      if (seteuid(ruid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s\n",
                       ruid, strerror(errno));
      }
   }
   tmp_sys_log_fd = sys_log_fd;
   sys_log_fd = STDERR_FILENO;
   ptr = attach_buf(window_id_file, &fd, &new_size, progname,
                    S_IRUSR | S_IWUSR, YES);
   sys_log_fd = tmp_sys_log_fd;
   if (euid != ruid)
   {
      if (seteuid(euid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s\n",
                       euid, strerror(errno));
      }
   }
   if (ptr == (caddr_t) -1)
   {
      (void)fprintf(stderr, "Failed to mmap() to %s : %s\n",
                    window_id_file, strerror(errno));
      window_id_file[0] = '\0';
      return(window_id);
   }
   no_of_windows = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   wl = (struct window_ids *)ptr;

   for (i = 0; i < *no_of_windows; i++)
   {
      if (wl[i].pid == pid)
      {
         if (kill(wl[i].pid, 0) == -1)
         {
            size_t move_size;

            move_size = (*no_of_windows - (i + 1)) * sizeof(struct window_ids);
            (void)memmove(&wl[i], &wl[i + 1], move_size);
            (*no_of_windows)--;
            i--;

            if ((*no_of_windows != 0) &&
                ((*no_of_windows % DEFAULT_WINDOW_ID_STEPSIZE) == 0))
            {
               new_size = ((*no_of_windows / DEFAULT_WINDOW_ID_STEPSIZE) *
                            DEFAULT_WINDOW_ID_STEPSIZE * sizeof(struct window_ids)) +
                          AFD_WORD_OFFSET;
               ptr -= AFD_WORD_OFFSET;
               if ((ptr = mmap_resize(fd, ptr, new_size)) == (caddr_t) -1)
               {
                  (void)fprintf(stderr, "Failed to mmap_resize() file %s : %s\n",
                                window_id_file, strerror(errno));
                  exit(INCORRECT);
               }
               no_of_windows = (int *)ptr;
               ptr += AFD_WORD_OFFSET;
               wl = (struct window_ids *)ptr;
            }
         }
         else
         {
            window_id = wl[i].window_id;
         }
         break;
      }
   }
   unmap_data(fd, (void *)&wl);

   return(window_id);
}


/*########################## remove_window_id() #########################*/
void
remove_window_id(pid_t pid, char *progname)
{
   int               fd,
                     *no_of_windows,
                     i,
                     tmp_sys_log_fd;
   size_t            new_size;
   uid_t             euid, /* Effective user ID. */
                     ruid; /* Real user ID. */
   char              *ptr;
   struct window_ids *wl;

   if (window_id_file[0] == '\0')
   {
      get_window_id_name();
   }
   new_size = (DEFAULT_WINDOW_ID_STEPSIZE * sizeof(struct window_ids)) +
              AFD_WORD_OFFSET;
   euid = geteuid();
   ruid = getuid();
   if (euid != ruid)
   {
      if (seteuid(ruid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s\n",
                       ruid, strerror(errno));
      }
   }
   tmp_sys_log_fd = sys_log_fd;
   sys_log_fd = STDERR_FILENO;
   ptr = attach_buf(window_id_file, &fd, &new_size, progname,
                    S_IRUSR | S_IWUSR, YES);
   sys_log_fd = tmp_sys_log_fd;
   if (euid != ruid)
   {
      if (seteuid(euid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s\n",
                       euid, strerror(errno));
      }
   }
   if (ptr == (caddr_t) -1)
   {
      (void)fprintf(stderr, "Failed to mmap() to %s : %s\n",
                    window_id_file, strerror(errno));
      window_id_file[0] = '\0';
      return;
   }
   no_of_windows = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   wl = (struct window_ids *)ptr;

   for (i = 0; i < *no_of_windows; i++)
   {
      if (wl[i].pid == pid)
      {
         size_t move_size;

         move_size = (*no_of_windows - (i + 1)) * sizeof(struct window_ids);
         (void)memmove(&wl[i], &wl[i + 1], move_size);
         (*no_of_windows)--;

         if ((*no_of_windows != 0) &&
             ((*no_of_windows % DEFAULT_WINDOW_ID_STEPSIZE) == 0))
         {
            new_size = ((*no_of_windows / DEFAULT_WINDOW_ID_STEPSIZE) *
                         DEFAULT_WINDOW_ID_STEPSIZE * sizeof(struct window_ids)) +
                       AFD_WORD_OFFSET;
            ptr -= AFD_WORD_OFFSET;
            if ((ptr = mmap_resize(fd, ptr, new_size)) == (caddr_t) -1)
            {
               (void)fprintf(stderr, "Failed to mmap_resize() file %s : %s\n",
                             window_id_file, strerror(errno));
               exit(INCORRECT);
            }
            no_of_windows = (int *)ptr;
            ptr += AFD_WORD_OFFSET;
            wl = (struct window_ids *)ptr;
         }
         break;
      }
   }
   unmap_data(fd, (void *)&wl);

   return;
}


/*########################## check_window_ids() #########################*/
void
check_window_ids(char *progname)
{
   int               fd,
                     i,
                     *no_of_windows,
                     tmp_sys_log_fd;
   size_t            new_size;
   uid_t             euid, /* Effective user ID. */
                     ruid; /* Real user ID. */
   char              *ptr;
   struct window_ids *wl;

   if (window_id_file[0] == '\0')
   {
      get_window_id_name();
   }
   new_size = (DEFAULT_WINDOW_ID_STEPSIZE * sizeof(struct window_ids)) +
              AFD_WORD_OFFSET;
   euid = geteuid();
   ruid = getuid();
   if (euid != ruid)
   {
      if (seteuid(ruid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s\n",
                       ruid, strerror(errno));
      }
   }
   tmp_sys_log_fd = sys_log_fd;
   sys_log_fd = STDERR_FILENO;
   ptr = attach_buf(window_id_file, &fd, &new_size, progname,
                    S_IRUSR | S_IWUSR, YES);
   sys_log_fd = tmp_sys_log_fd;
   if (euid != ruid)
   {
      if (seteuid(euid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s\n",
                       euid, strerror(errno));
      }
   }
   if (ptr == (caddr_t) -1)
   {
      (void)fprintf(stderr, "Failed to mmap() to %s : %s\n",
                    window_id_file, strerror(errno));
      window_id_file[0] = '\0';
      return;
   }
   no_of_windows = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   wl = (struct window_ids *)ptr;

   for (i = 0; i < *no_of_windows; i++)
   {
      if (kill(wl[i].pid, 0) == -1)
      {
         size_t move_size;

         move_size = (*no_of_windows - (i + 1)) * sizeof(struct window_ids);
         (void)memmove(&wl[i], &wl[i + 1], move_size);
         (*no_of_windows)--;
         i--;

         if ((*no_of_windows != 0) &&
             ((*no_of_windows % DEFAULT_WINDOW_ID_STEPSIZE) == 0))
         {
            new_size = ((*no_of_windows / DEFAULT_WINDOW_ID_STEPSIZE) *
                         DEFAULT_WINDOW_ID_STEPSIZE * sizeof(struct window_ids)) +
                       AFD_WORD_OFFSET;
            ptr -= AFD_WORD_OFFSET;
            if ((ptr = mmap_resize(fd, ptr, new_size)) == (caddr_t) -1)
            {
               (void)fprintf(stderr, "Failed to mmap_resize() file %s : %s\n",
                             window_id_file, strerror(errno));
               exit(INCORRECT);
            }
            no_of_windows = (int *)ptr;
            ptr += AFD_WORD_OFFSET;
            wl = (struct window_ids *)ptr;
         }
      }
   }
   unmap_data(fd, (void *)&wl);

   return;
}


/*++++++++++++++++++++++++ get_window_id_name() +++++++++++++++++++++++++*/
static void
get_window_id_name(void)
{
   char *ptr;

   if ((ptr = getenv("HOME")) == NULL)
   {
      (void)strcpy(window_id_file, AFD_WINDOW_ID_FILE);
   }
   else
   {
      (void)snprintf(window_id_file, MAX_PATH_LENGTH, "%s/%s",
                     ptr, AFD_WINDOW_ID_FILE);
   }
   return;
}
