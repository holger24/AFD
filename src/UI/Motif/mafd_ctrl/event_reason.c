/*
 *  event_reason.c - Part of AFD, an automatic file distribution program.
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
 **   event_reason - functions for showing event reason for host
 **
 ** SYNOPSIS
 **   void popup_event_reason(int x_root, int y_root, int host_no)
 **   void destroy_event_reason(void)
 **
 ** DESCRIPTION
 **   The function popup_event_reason() pops up a window showing
 **   the event reason.
 **   destroy_event_reason() destroys this window.
 **
 ** RETURN VALUES
 **   None
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   09.08.2007 H.Kiehl Created
 **   10.05.2014 H.Kiehl If someone has manually set a reason via
 **                      the handle_event dialog, always show this
 **                      reason before we show the static reason.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>       /* strtol()                                    */
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>    /* mmap(), munmap()                            */
# ifndef MAP_FILE         /* Required for BSD          */
#  define MAP_FILE 0      /* All others do not need it */
# endif
#endif
#include <errno.h>
#include "mafd_ctrl.h"
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include "logdefs.h"


/* External global variables. */
extern Display                    *display;
extern Widget                     appshell;
extern XmFontList                 fontlist;
extern unsigned long              color_pool[];
extern unsigned int               glyph_height,
                                  glyph_width;
extern char                       *p_work_dir;
extern struct filetransfer_status *fsa;

/* Local global variables. */
static Widget                     event_reason_shell = NULL;

/* Local function prototypes. */
static void                       er_input(Widget, XtPointer, XEvent *),
                                  get_event_reason(char *, char *);


/*######################## popup_event_reason() #########################*/
void
popup_event_reason(int x_root, int y_root, int host_no)
{
   if (event_reason_shell != NULL)
   {
      destroy_event_reason();
   }

   if ((fsa[host_no].host_status & HOST_ERROR_ACKNOWLEDGED) ||
       (fsa[host_no].host_status & HOST_ERROR_OFFLINE) ||
       (fsa[host_no].host_status & HOST_ERROR_ACKNOWLEDGED_T) ||
       (fsa[host_no].host_status & HOST_ERROR_OFFLINE_T) ||
       ((fsa[host_no].host_status & HOST_ERROR_OFFLINE_STATIC) &&
        (fsa[host_no].error_counter > fsa[host_no].max_errors)))
   {
      char event_reason[MAX_USER_NAME_LENGTH + 2 + MAX_EVENT_REASON_LENGTH + 1];

      event_reason[0] = '\0';
      if ((fsa[host_no].host_status & HOST_ERROR_ACKNOWLEDGED) ||
          (fsa[host_no].host_status & HOST_ERROR_OFFLINE) ||
          (fsa[host_no].host_status & HOST_ERROR_ACKNOWLEDGED_T) ||
          (fsa[host_no].host_status & HOST_ERROR_OFFLINE_T))
      {
         get_event_reason(event_reason, fsa[host_no].host_alias);
      }
      if ((event_reason[0] == '\0') &&
          (fsa[host_no].host_status & HOST_ERROR_OFFLINE_STATIC) &&
          (fsa[host_no].error_counter > fsa[host_no].max_errors))
      {
         (void)strcpy(event_reason, STATIC_EVENT_REASON);
      }

      if (event_reason[0] != '\0')
      {
         Widget   event_reason_label,
                  form;
         XmString x_string;
         int      display_height,
                  display_width,
                  lines,
                  max_length,
                  over_hang,
                  str_length;
         char     *ptr;

         /* Lets determine how many lines we are able to display. */
         display_height = DisplayHeight(display, DefaultScreen(display));
         max_length = lines = 0;
         ptr = event_reason;

         while (*ptr != '\0')
         {
            str_length = 0;
            while ((*(ptr + str_length) != '\n') &&
                   (*(ptr + str_length) != '\0'))
            {
               str_length++;
            }
            if (*(ptr + str_length) == '\n')
            {
               str_length++;
            }
            lines++;
            if (str_length > max_length)
            {
               max_length = str_length;
            }
            ptr += str_length;
         }

         event_reason_shell = XtVaCreatePopupShell("event_reason_shell",
                                      topLevelShellWidgetClass, appshell,
                                      XtNoverrideRedirect,      True,
                                      XtNallowShellResize,      True,
                                      XtNmappedWhenManaged,     False,
                                      XtNsensitive,             True,
                                      XtNwidth,                 1,
                                      XtNheight,                1,
                                      XtNborderWidth,           0,
                                      NULL);
         XtManageChild(event_reason_shell);
         XtAddEventHandler(event_reason_shell,
                           ButtonPressMask | Button1MotionMask, False,
                           (XtEventHandler)er_input, NULL);
         form = XtVaCreateWidget("event_reason_box",
                                 xmFormWidgetClass, event_reason_shell, NULL);
         XtManageChild(form);

         display_width = DisplayWidth(display, DefaultScreen(display));
         over_hang = display_width - (x_root + (max_length * glyph_width));
         if (over_hang < 0)
         {
            x_root += over_hang;
         }
         over_hang = display_height - (y_root + (lines * glyph_height));
         if (over_hang < 0)
         {
            y_root += over_hang;
         }
         XMoveResizeWindow(display, XtWindow(event_reason_shell),
                           x_root, y_root, max_length * glyph_width,
                           lines * glyph_height);

         x_string = XmStringCreateLocalized(event_reason);
         event_reason_label = XtVaCreateWidget("event_reason_label",
                                     xmLabelWidgetClass, form,
                                     XmNfontList,        fontlist,
                                     XmNlabelString,     x_string,
                                     XtNbackground,      color_pool[WHITE],
                                     XtNforeground,      color_pool[BLACK],
                                     NULL);
         XtManageChild(event_reason_label);
         XmStringFree(x_string);
         XtAddEventHandler(event_reason_label,
                           ButtonPressMask | LeaveWindowMask, False,
                           (XtEventHandler)destroy_event_reason, NULL);
         XtPopup(event_reason_shell, XtGrabNone);
         XRaiseWindow(display, XtWindow(event_reason_shell));
      }
   }
   else
   {
      destroy_event_reason();
   }

   return;
}


/*####################### destroy_event_reason() ########################*/
void
destroy_event_reason(void)
{
   if (event_reason_shell != NULL)
   {
      XtDestroyWidget(event_reason_shell);
      event_reason_shell = NULL;
   }

   return;
}


/*+++++++++++++++++++++++++++++ er_input() ++++++++++++++++++++++++++++++*/
static void
er_input(Widget w, XtPointer client_data, XEvent *event)
{
   destroy_event_reason();
   return;
}


/*++++++++++++++++++++++++++ get_event_reason() +++++++++++++++++++++++++*/
static void
get_event_reason(char *reason_str, char *host_alias)
{
   int          fd,
                i,
                max_event_log_files,
                type;
   char         log_file[MAX_PATH_LENGTH],
                *p_log_file,
                *ptr,
                *src;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   /* Always reset reason_str, so we do not display incoreect data. */
   reason_str[0] = '\0';

   /* Get the maximum event log file number. */
   (void)snprintf(log_file, MAX_PATH_LENGTH, "%s%s%s",
                  p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   if ((eaccess(log_file, F_OK) == 0) &&
       (read_file_no_cr(log_file, &src, YES, __FILE__, __LINE__) != INCORRECT))
   {
      char value[MAX_INT_LENGTH + 1];

      if (get_definition(src, MAX_EVENT_LOG_FILES_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         max_event_log_files = atoi(value);
      }
      else
      {
         max_event_log_files = MAX_EVENT_LOG_FILES;
      }
      free(src);
   }
   else
   {
      max_event_log_files = MAX_EVENT_LOG_FILES;
   }

   /* Prepare log file name. */
   i = snprintf(log_file, MAX_PATH_LENGTH, "%s%s/%s",
                p_work_dir, LOG_DIR, EVENT_LOG_NAME);
   if (i > MAX_PATH_LENGTH)
   {
      (void)xrec(FATAL_DIALOG, "Buffer to small %d > %d (%s %d)",
                 i, MAX_PATH_LENGTH, __FILE__, __LINE__);
      return;
   }
   p_log_file = log_file + i;

   for (i = 0; i < max_event_log_files; i++)
   {
      (void)snprintf(p_log_file, MAX_PATH_LENGTH - (p_log_file - log_file),
                     "%d", i);
#ifdef HAVE_STATX
      if (statx(0, log_file, AT_STATX_SYNC_AS_STAT,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (stat(log_file, &stat_buf) == -1)
#endif
      {
         if (errno == ENOENT)
         {
            /* For some reason the file is not there. So lets */
            /* assume we have found nothing.                  */;
         }
         else
         {
            (void)xrec(WARN_DIALOG, "Failed to access %s : %s (%s %d)",
                       log_file, strerror(errno), __FILE__, __LINE__);
         }
         return;
      }
#ifdef HAVE_STATX
      if (stat_buf.stx_size == 0)
#else
      if (stat_buf.st_size == 0)
#endif
      {
         return;
      }

      if ((fd = open(log_file, O_RDONLY)) == -1)
      {
         (void)xrec(FATAL_DIALOG, "Failed to open() %s : %s (%s %d)",
                    log_file, strerror(errno), __FILE__, __LINE__);
         return;
      }
#ifdef HAVE_MMAP
      if ((src = mmap(0,
# ifdef HAVE_STATX
                      stat_buf.stx_size, PROT_READ,
# else
                      stat_buf.st_size, PROT_READ,
# endif
                      (MAP_FILE | MAP_SHARED), fd, 0)) == (caddr_t) -1)
      {
         (void)xrec(FATAL_DIALOG, "Failed to mmap() %s : %s (%s %d)",
                    log_file, strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return;
      }
#else
# ifdef HAVE_STATX
      if ((src = malloc(stat_buf.stx_size)) == NULL)
# else
      if ((src = malloc(stat_buf.st_size)) == NULL)
# endif
      {
         (void)xrec(FATAL_DIALOG, "malloc() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return;
      }
# ifdef HAVE_STATX
      if (read(fd, src, stat_buf.stx_size) != stat_buf.stx_size)
# else
      if (read(fd, src, stat_buf.st_size) != stat_buf.st_size)
# endif
      {
         (void)xrec(FATAL_DIALOG, "Failed to read() from %s : %s (%s %d)",
                    log_file, strerror(errno), __FILE__, __LINE__);
         free(src);
         (void)close(fd);
         return;
      }
#endif
      if (close(fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "close() error : %s", strerror(errno));
      }
#ifdef HAVE_STATX
      ptr = src + stat_buf.stx_size - 2;
#else
      ptr = src + stat_buf.st_size - 2;
#endif

      /*
       * Lets first search for an EA_OFFLINE or EA_ACKNOWLEDGE event
       * for this host.
       */
      while (ptr > src)
      {
         while ((ptr > src) && (*ptr != '\n'))
         {
            ptr--;
         }
         if ((ptr > src) && (*ptr == '\n') && (*(ptr + 1) != ' '))
         {
            ptr++;
            HEX_CHAR_TO_INT((*(ptr + LOG_DATE_LENGTH + 1)));
            if (type == EC_HOST)
            {
               HEX_CHAR_TO_INT((*(ptr + LOG_DATE_LENGTH + 3)));
               if (type == ET_MAN)
               {
                  int          k;
                  unsigned int event_action_no;
                  char         str_number[MAX_INT_LENGTH + 1],
                               *tmp_ptr;

                  tmp_ptr = ptr;
                  ptr += LOG_DATE_LENGTH + 5;
                  k = 0;
                  do
                  {
                     str_number[k] = *(ptr + k);
                     k++;
                  } while ((*(ptr + k) != SEPARATOR_CHAR) &&
                           (*(ptr + k) != '\n') && (k < MAX_INT_LENGTH));
                  str_number[k] = '\0';
                  event_action_no = (unsigned int)strtoul(str_number, NULL, 16);
                  if ((event_action_no == EA_ACKNOWLEDGE) ||
                      (event_action_no == EA_OFFLINE))
                  {
                     ptr += k;
                     if (*ptr == SEPARATOR_CHAR)
                     {
                        ptr++;
                        k = 0;
                        while ((*(ptr + k) != SEPARATOR_CHAR) &&
                               (*(ptr + k) != '\n') &&
                               (*(ptr + k) == *(host_alias + k)))
                        {
                           k++;
                        }
                        if (((*(ptr + k) == SEPARATOR_CHAR) ||
                             (*(ptr + k) == '\n')) &&
                            (*(host_alias + k) == '\0'))
                        {
                           ptr += k;
                           if (*ptr == SEPARATOR_CHAR)
                           {
                              ptr++;
                              k = 0;

                              /* Store user. */
                              while ((*(ptr + k) != SEPARATOR_CHAR) &&
                                     (*(ptr + k) != '\n'))
                              {
                                 *(reason_str + k) = *(ptr + k);
                                 k++;
                              }
                              if (*(ptr + k) == SEPARATOR_CHAR)
                              {
                                 int j;

                                 *(reason_str + k) = '\n';
                                 k += 1;
                                 j = k;
                                 while (*(ptr + k) != '\n')
                                 {
                                    if (*(ptr + k) == '%')
                                    {
                                       char hex_char[3];

                                       hex_char[0] = *(ptr + k + 1);
                                       hex_char[1] = *(ptr + k + 2);
                                       hex_char[2] = '\0';
                                       *(reason_str + j) = (char)strtol(hex_char, NULL, 16);
                                       k += 3;
                                       j += 1;
                                    }
                                    else
                                    {
                                       *(reason_str + j) = *(ptr + k);
                                       j++; k++;
                                    }
                                 }
                                 *(reason_str + j) = '\0';
                              }
                              else
                              {
                                 *(reason_str + k) = '\0';
                              }

                              /* Free all memory we have allocated. */
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
                              if (munmap(src, stat_buf.stx_size) < 0)
# else
                              if (munmap(src, stat_buf.st_size) < 0)
# endif
                              {
                                 (void)xrec(ERROR_DIALOG,
                                            "munmap() error : %s (%s %d)",
                                            strerror(errno),
                                            __FILE__, __LINE__);
                              }                                                  
#else
                              free(src);
#endif

                              return;
                           }
                           else
                           {
                              ptr = tmp_ptr - 2;
                           }
                        }
                        else
                        {
                           ptr = tmp_ptr - 2;
                        }
                     }
                     else
                     {
                        ptr = tmp_ptr - 2;
                     }
                  }
                  else
                  {
                     ptr = tmp_ptr - 2;
                  }
               }
               else
               {
                  ptr -= 2;
               }
            }
            else
            {
               ptr -= 2;
            }
         }
         else
         {
            ptr--;
         }
      }

      /* Free all memory we have allocated. */
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
      if (munmap(src, stat_buf.stx_size) < 0)
# else
      if (munmap(src, stat_buf.st_size) < 0)
# endif
      {
         (void)xrec(ERROR_DIALOG, "munmap() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
      }                                                  
#else
      free(src);
#endif
   }

   return;
}
