/*
 *  error_history.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2004 - 2017 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   09.03.2017 H.Kiehl Added support for groups.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "mafd_ctrl.h"
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Label.h>

#define EL_STEP_SIZE 3


/* External global variables. */
extern int                        no_of_hosts;
extern Display                    *display;
extern Widget                     appshell;
extern XmFontList                 fontlist;
extern unsigned long              color_pool[];
extern unsigned int               glyph_height,
                                  glyph_width;
extern char                       *p_work_dir;
extern struct line                *connect_data;
extern struct filetransfer_status *fsa;

/* Local global variables. */
static Widget                     error_shell = NULL;

/* Local function prototypes. */
static void                       eh_input(Widget, XtPointer, XEvent *);


/*######################## popup_error_history() ########################*/
void
popup_error_history(int x_root, int y_root, int host_no)
{
   int  display_height,
        total_error_list_length,
        i,
        lines,
        max_lines,
        max_length,
        str_length;
   char *error_list;

   if (error_shell != NULL)
   {
      destroy_error_history();
   }

   /* Lets determine how many lines we are able to display. */
   display_height = DisplayHeight(display, DefaultScreen(display));
   max_lines = display_height / glyph_height;

   if (connect_data[host_no].type == GROUP_IDENTIFIER)
   {
      int host_alias_shown,
          j;
      struct error_list
             {  
                char host_alias[MAX_HOSTNAME_LENGTH + 1];
                char error_str[MAX_ERROR_STR_LENGTH + 1];
             } *el;

      if ((el = malloc((EL_STEP_SIZE * sizeof(struct error_list)))) == NULL)
      {
         (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }

      max_length = lines = 0;
      for (i = host_no + 1; ((i < no_of_hosts) &&
                             (connect_data[i].type != GROUP_IDENTIFIER) &&
                             (lines < max_lines)); i++)
      {
         host_alias_shown = NO;
         if (connect_data[i].error_counter > 0)
         {
            for (j = 0; ((j < ERROR_HISTORY_LENGTH) && (lines < max_lines)); j++)
            {
               if ((lines != 0) && ((lines % EL_STEP_SIZE) == 0))
               {
                  size_t new_size = ((lines / EL_STEP_SIZE) + 1) *
                                    EL_STEP_SIZE * sizeof(struct error_list);

                  if ((el = realloc(el, new_size)) == NULL)
                  {
                     (void)fprintf(stderr, "realloc() error : %s (%s %d)\n",
                                   strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
               }
               if (host_alias_shown == NO)
               {
                  (void)strcpy(el[lines].host_alias, connect_data[i].hostname);
                  host_alias_shown = YES;
               }
               else
               {
                  el[lines].host_alias[0] = '\0';
               }
               (void)strcpy(el[lines].error_str,
                            get_error_str(fsa[i].error_history[j]));
               str_length = strlen(el[lines].error_str);
               if (str_length > max_length)
               {
                  max_length = str_length;
               }
               lines++;
            }
         }
      }

      if (lines > 0)
      {
         total_error_list_length = (lines * (MAX_HOSTNAME_LENGTH + 1 +
                                             max_length + 1)) + 1;
         if ((error_list = malloc(total_error_list_length)) == NULL)
         {
            (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }

         str_length = 0;
         for (i = 0; i < lines; i++)
         {
            str_length += sprintf(error_list + str_length, "%-*s %-*s\n",
                                  MAX_HOSTNAME_LENGTH, el[i].host_alias,
                                  max_length, el[i].error_str);
         }
         error_list[str_length - 1] = '\0';
      }
      free((char *)el);
   }
   else
   {
      if (connect_data[host_no].error_counter > 0)
      {
         struct error_list
                {
                   char error_str[MAX_ERROR_STR_LENGTH + 1];
                } *el;

         if ((el = malloc((EL_STEP_SIZE * sizeof(struct error_list)))) == NULL)
         {
            (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }

         max_length = lines = 0;
         for (i = 0; ((i < ERROR_HISTORY_LENGTH) && (lines < max_lines)); i++)
         {
            if ((lines != 0) && ((lines % EL_STEP_SIZE) == 0))
            {
               size_t new_size = ((lines / EL_STEP_SIZE) + 1) *
                                 EL_STEP_SIZE * sizeof(struct error_list);

               if ((el = realloc(el, new_size)) == NULL)
               {
                  (void)fprintf(stderr, "realloc() error : %s (%s %d)\n",
                                strerror(errno), __FILE__, __LINE__);
                  exit(INCORRECT);
               }
            }
            (void)strcpy(el[lines].error_str,
                         get_error_str(fsa[host_no].error_history[i]));
            str_length = strlen(el[lines].error_str);
            if (str_length > max_length)
            {
               max_length = str_length;
            }
            lines++;
         }

         if (lines > 0)
         {
            total_error_list_length = (lines * (max_length + 1)) + 1;
            if ((error_list = malloc(total_error_list_length)) == NULL)
            {
               (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }

            str_length = 0;
            for (i = 0; i < lines; i++)
            {
               str_length += sprintf(error_list + str_length, "%-*s\n",
                                     max_length, el[i].error_str);
            }
            error_list[str_length - 1] = '\0';
         }
         free((char *)el);
      }
   }

   if (error_list != NULL)
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
