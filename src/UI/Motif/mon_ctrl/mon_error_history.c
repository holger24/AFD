/*
 *  mon_error_history.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2004 - 2015 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   mon_error_history - functions for showing all error hosts
 **
 ** SYNOPSIS
 **   void popup_error_history(int x_root, int y_root, int afd_no)
 **   void destroy_error_history(void)
 **
 ** DESCRIPTION
 **   The function popup_error_history() pops up a window showing all
 **   hosts that have errors and the last error codes.
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
#include "fddefs.h"
#include "mon_ctrl.h"
#include "mondefs.h"
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Label.h>


/* External global variables. */
extern Display                *display;
extern Widget                 appshell;
extern XmFontList             fontlist;
extern unsigned long          color_pool[];
extern unsigned int           glyph_height,
                              glyph_width;
extern char                   *p_work_dir;
extern struct mon_status_area *msa;

/* Local global variables. */
static Widget                 error_shell = NULL;

/* Local function prototypes. */
static void                   eh_input(Widget, XtPointer, XEvent *);


/*######################## popup_error_history() ########################*/
void
popup_error_history(int x_root, int y_root, int afd_no)
{
   int                  display_height,
                        error_hosts,
                        i, j,
                        length,
                        lines,
                        max_lines,
                        max_length,
                        str_length,
                        total_error_list_length,
                        one_error_list_length;
   char                 ahl_file[MAX_PATH_LENGTH],
                        *error_list;
   struct afd_host_list *ahl;

   if (error_shell != NULL)
   {
      destroy_error_history();
   }

   /* Lets determine how many lines we are able to display. */
   display_height = DisplayHeight(display, DefaultScreen(display));
   max_lines = display_height / glyph_height;

   (void)sprintf(ahl_file, "%s%s%s%s",
                 p_work_dir, FIFO_DIR, AHL_FILE_NAME, msa[afd_no].afd_alias);
   (void)read_file(ahl_file, (char **)&ahl);
   total_error_list_length = ERROR_HISTORY_LENGTH *
                             (MAX_HOSTNAME_LENGTH + 1 + 5 + 1 +
                              MAX_ERROR_STR_LENGTH + 1);
   one_error_list_length = total_error_list_length;
   if ((error_list = malloc(total_error_list_length)) == NULL)
   {
      (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   error_hosts = length = max_length = str_length = lines = 0;
   for (i = 0; ((i < msa[afd_no].no_of_hosts) && (lines < max_lines)); i++)
   {
      if ((ahl[i].error_history[0] != TRANSFER_SUCCESS) &&
          (ahl[i].error_history[0] != OPEN_FILE_DIR_ERROR) &&
          ((msa[afd_no].ec > 0) || (msa[afd_no].host_error_counter > 0)))
      {
         if (error_hosts != 0)
         {
            total_error_list_length += one_error_list_length;
            if ((error_list = realloc(error_list,
                                      total_error_list_length)) == NULL)
            {
               (void)fprintf(stderr, "realloc() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
         }
         str_length = sprintf(error_list + length, "%-*s [%d] %s\n",
                              MAX_HOSTNAME_LENGTH, ahl[i].host_alias,
                              (int)ahl[i].error_history[0],
                              get_error_str(ahl[i].error_history[0]));
         if (str_length > max_length)
         {
            max_length = str_length;
         }
         length += str_length;
         lines++;
         for (j = 1; j < ERROR_HISTORY_LENGTH; j++)
         {
            if ((ahl[i].error_history[j] == 0) || (lines >= max_lines))
            {
               j = ERROR_HISTORY_LENGTH;
            }
            else
            {
               str_length = sprintf(error_list + length, "%-*s [%d] %s\n",
                                    MAX_HOSTNAME_LENGTH, "",
                                    (int)ahl[i].error_history[j],
                                    get_error_str(ahl[i].error_history[j]));
               if (str_length > max_length)
               {
                  max_length = str_length;
               }
               length += str_length;
               lines++;
            }
         }
         error_hosts++;
      }
   }
   free((char *)ahl);
   if (error_hosts > 0)
   {
      Widget   error_label,
               form;
      XmString x_string;
      int      display_width,
               over_hang;

      error_shell = XtVaCreatePopupShell("error_history_shell",
                                      topLevelShellWidgetClass, appshell,
                                      XtNoverrideRedirect,      True,
                                      XtNallowShellResize,      True,
                                      XtNmappedWhenManaged,     False,
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
                                  XmNfontList,        fontlist,
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
   }
   else
   {
      destroy_error_history();
   }
   free(error_list);

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
