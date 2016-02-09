/*
 *  error_history.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2004 - 2008 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   error_history - functions for showing error history of a host
 **
 ** SYNOPSIS
 **   void popup_error_history(int x_root, int y_root, int host_no)
 **   void destroy_error_history(void)
 **
 ** DESCRIPTION
 **   The function popup_error_history() pops up a window showing
 **   the error history.
 **   destroy_error_history() destroys this window.
 **
 ** RETURN VALUES
 **   None
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.06.2004 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "gafd_ctrl.h"
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Label.h>


/* External global variables. */
extern Display                    *display;
extern Widget                     appshell;
extern unsigned long              color_pool[];
extern unsigned int               glyph_height,
                                  glyph_width;
extern char                       *p_work_dir;
extern struct filetransfer_status *fsa;

/* Local global variables. */
static Widget                     error_shell = NULL;

/* Local function prototypes. */
static void                       eh_input(Widget, XtPointer, XEvent *);


/*######################## popup_error_history() ########################*/
void
popup_error_history(int x_root, int y_root, int host_no)
{
   if (error_shell != NULL)
   {
      destroy_error_history();
   }

   if (fsa[host_no].error_history[0] != 0)
   {
      Widget   error_label,
               form;
      XmString x_string;
      int      display_height,
               display_width,
               error_list_length,
               i,
               length,
               lines,
               max_lines,
               max_length,
               over_hang,
               str_length;
      char     *error_list;

      /* Lets determine how many lines we are able to display. */
      display_height = DisplayHeight(display, DefaultScreen(display));
      max_lines = display_height / glyph_height;
   
      error_list_length = ERROR_HISTORY_LENGTH *
                          (5 + 1 + MAX_ERROR_STR_LENGTH + 1);
      if ((error_list = malloc(error_list_length)) == NULL)
      {
         (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      length = max_length = lines = 0;

      str_length = sprintf(error_list + length, "[%d] %s\n",
                           (int)fsa[host_no].error_history[0],
                           get_error_str(fsa[host_no].error_history[0]));
      if (str_length > max_length)
      {
         max_length = str_length;
      }
      length += str_length;
      lines++;
      for (i = 1; i < ERROR_HISTORY_LENGTH; i++)
      {
         if ((fsa[host_no].error_history[i] == 0) || (lines >= max_lines))
         {
            i = ERROR_HISTORY_LENGTH;
         }
         else
         {
            str_length = sprintf(error_list + length, "[%d] %s\n",
                                 (int)fsa[host_no].error_history[i],
                                 get_error_str(fsa[host_no].error_history[i]));
            if (str_length > max_length)
            {
               max_length = str_length;
            }
            length += str_length;
            lines++;
         }
      }

      error_shell = XtVaCreatePopupShell("error_history_shell",
                                      topLevelShellWidgetClass, appshell,
                                      XtNoverrideRedirect,      True,
                                      XtNallowShellResize,      True,
                                      XtNmappedWhenManaged,     False,
                                      XtNsensitive,             True,
                                      XtNwidth,                 1,
                                      XtNheight,                1,
                                      XtNborderWidth,           0,
                                      NULL);
      XtManageChild(error_shell);
      XtAddEventHandler(error_shell, ButtonPressMask | Button1MotionMask,
                        False, (XtEventHandler)eh_input, NULL);
      form = XtVaCreateWidget("error_box",
                              xmFormWidgetClass, error_shell, NULL);
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
      XMoveResizeWindow(display, XtWindow(error_shell),
                        x_root, y_root, max_length * glyph_width, lines * glyph_height);

      error_list[length - 1] = '\0';
      x_string = XmStringCreateLocalized(error_list);
      error_label = XtVaCreateWidget("error_label",
                                  xmLabelWidgetClass, form,
                                  XmNlabelString,     x_string,
                                  XtNbackground,      color_pool[WHITE],
                                  XtNforeground,      color_pool[BLACK],
                                  NULL);
      XtManageChild(error_label);
      XmStringFree(x_string);
      XtAddEventHandler(error_label, ButtonPressMask|LeaveWindowMask, False,
                        (XtEventHandler)destroy_error_history, NULL);
      XtPopup(error_shell, XtGrabNone);
      XRaiseWindow(display, XtWindow(error_shell));
      free(error_list);
   }
   else
   {
      destroy_error_history();
   }

   return;
}


/*####################### destroy_error_history() #######################*/
void
destroy_error_history(void)
{
   if (error_shell != NULL)
   {
      XtDestroyWidget(error_shell);
      error_shell = NULL;
   }

   return;
}


/*+++++++++++++++++++++++++++++ eh_input() ++++++++++++++++++++++++++++++*/
static void
eh_input(Widget w, XtPointer client_data, XEvent *event)
{
   destroy_error_history();
   return;
}
