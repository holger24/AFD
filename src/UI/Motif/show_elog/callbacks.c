/*
 *  callbacks.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 - 2012 Holger Kiehl <Holger.Kiehl@@dwd.de>
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
 **   callbacks - all callback functions for module show_elog
 **
 ** SYNOPSIS
 **   void continues_toggle(Widget w, XtPointer client_data, XtPointer call_data)
 **   void toggled(Widget w, XtPointer client_data, XtPointer call_data)
 **   void search_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void close_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void event_action_list(Widget w, XtPointer client_data, XtPointer call_data)
 **   void save_input(Widget w, XtPointer client_data, XtPointer call_data)
 **   void scrollbar_moved(Widget w, XtPointer client_data, XtPointer call_data)
 **
 ** DESCRIPTION
 **   The function toggled() is used to set the bits in the global
 **   variable toggles_set. The following bits can be set: SHOW_CLASS_GLOBAL,
 **   SHOW_CLASS_DIRECTORY, SHOW_CLASS_PRODUCTION, SHOW_CLASS_HOST,
 **   SHOW_TYPE_EXTERNAL, SHOW_TYPE_MANUAL and SHOW_TYPE_AUTO.
 **
 **   search_button() activates the search in the event log. When
 **   pressed the label of the button changes to 'Stop'. Now the user
 **   has the chance to stop the search. During the search only the
 **   list widget and the Stop button can be used.
 **
 **   close_button() will terminate the program.
 **
 **   save_input() evaluates the input for start and end time.
 **
 **   scrollbar_moved() sets a flag that the scrollbar has been moved so
 **   we do NOT position to the last item in the list.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.07.2007 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <stdlib.h>         /* exit()                                    */
#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <errno.h>
#include "motif_common_defs.h"
#include "show_elog.h"

/* External global variables. */
extern Display        *display;
extern Widget         appshell,
                      class_togglebox_w,
                      cont_togglebox_w,
                      dir_alias_w,
                      dir_label_w,
                      end_time_w,
                      headingbox_w,
                      host_alias_w,
                      host_label_w,
                      outputbox_w,
                      print_button_w,
                      start_time_w,
                      statusbox_w,
                      search_w,
                      selectionbox_w,
                      type_togglebox_w;
extern XmTextPosition wpr_position;
extern Window         main_window;
extern int            continues_toggle_set,
                      items_selected,
                      no_of_search_dir_alias,
                      no_of_search_host_alias,
                      special_button_flag;
extern XT_PTR_TYPE    toggles_set;
extern time_t         start_time_val,
                      end_time_val;
extern size_t         search_file_size;
extern char           search_add_info[],
                      **search_dir_alias,
                      **search_host_alias;

/* Local global variables. */
static int            scrollbar_moved_flag;


/*############################## toggled() ##############################*/
void
toggled(Widget w, XtPointer client_data, XtPointer call_data)
{
   toggles_set ^= (XT_PTR_TYPE)client_data;
   if (toggles_set & SHOW_CLASS_DIRECTORY)
   {
      XtSetSensitive(dir_label_w, True);
      XtSetSensitive(dir_alias_w, True);
   }
   else
   {
      XtSetSensitive(dir_label_w, False);
      XtSetSensitive(dir_alias_w, False);
   }
   if (toggles_set & SHOW_CLASS_HOST)
   {
      XtSetSensitive(host_label_w, True);
      XtSetSensitive(host_alias_w, True);
   }
   else
   {
      XtSetSensitive(host_label_w, False);
      XtSetSensitive(host_alias_w, False);
   }

   return;
}


/*########################## continues_toggle() #########################*/
void
continues_toggle(Widget w, XtPointer client_data, XtPointer call_data)
{
   if (continues_toggle_set == NO)
   {
      continues_toggle_set = YES;
   }
   else
   {
      continues_toggle_set = NO;
   }
   return;
}


/*######################### scrollbar_moved() ###########################*/
void
scrollbar_moved(Widget w, XtPointer client_data, XtPointer call_data)
{
   scrollbar_moved_flag = YES;

   return;
}


/*######################## event_action_list() ##########################*/
void
event_action_list(Widget w, XtPointer client_data, XtPointer call_data)
{
   return;
}


/*########################## search_button() ############################*/
void
search_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   if (special_button_flag == SEARCH_BUTTON)
   {
      XtSetSensitive(cont_togglebox_w, False);
      XtSetSensitive(class_togglebox_w, False);
      XtSetSensitive(type_togglebox_w, False);
      XtSetSensitive(selectionbox_w, False);
      XtSetSensitive(start_time_w, False);
      XtSetSensitive(end_time_w, False);
      XtSetSensitive(host_alias_w, False);
      XtSetSensitive(dir_alias_w, False);
      XtSetSensitive(search_w, False);
      XtSetSensitive(print_button_w, False);

      scrollbar_moved_flag = NO;
      XmTextSetString(outputbox_w, NULL);
      XmTextSetInsertionPosition(outputbox_w, 0);
      get_data();

      /* Only position to last item when scrollbar was NOT moved! */
      if (scrollbar_moved_flag == NO)
      {
         XtVaSetValues(outputbox_w, XmNcursorPosition, wpr_position, NULL);
         XmTextShowPosition(outputbox_w, wpr_position);
      }
   }
   else
   {
      set_sensitive();
      special_button_flag = STOP_BUTTON_PRESSED;
   }

   return;
}


/*########################### set_sensitive() ###########################*/
void
set_sensitive(void)
{
   XtSetSensitive(cont_togglebox_w, True);
   XtSetSensitive(class_togglebox_w, True);
   XtSetSensitive(type_togglebox_w, True);
   XtSetSensitive(selectionbox_w, True);
   XtSetSensitive(start_time_w, True);
   XtSetSensitive(end_time_w, True);
   XtSetSensitive(host_alias_w, True);
   XtSetSensitive(dir_alias_w, True);
   XtSetSensitive(search_w, True);
   XtSetSensitive(print_button_w, True);

   return;
}


/*############################ print_button() ###########################*/
void
print_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   reset_message(statusbox_w);
   print_data(w, client_data, call_data);

   return;
}


/*########################### close_button() ############################*/
void
close_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   exit(0);
}


/*############################# save_input() ############################*/
void
save_input(Widget w, XtPointer client_data, XtPointer call_data)
{
   XT_PTR_TYPE type = (XT_PTR_TYPE)client_data;
   char        *ptr,
               *value = XmTextGetString(w);

   switch (type)
   {
      case START_TIME_NO_ENTER : 
         if (value[0] == '\0')
         {
            start_time_val = -1;
         }
         else if (eval_time(value, w, &start_time_val, START_TIME) < 0)
              {
                 show_message(statusbox_w, TIME_FORMAT);
                 XtFree(value);
                 return;
              }
         reset_message(statusbox_w);
         break;

      case START_TIME :
         if (eval_time(value, w, &start_time_val, START_TIME) < 0)
         {
            show_message(statusbox_w, TIME_FORMAT);
         }
         else
         {
            reset_message(statusbox_w);
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         break;

      case END_TIME_NO_ENTER : 
         if (value[0] == '\0')
         {
            end_time_val = -1;
         }
         else if (eval_time(value, w, &end_time_val, END_TIME) < 0)
              {
                 show_message(statusbox_w, TIME_FORMAT);
                 XtFree(value);
                 return;
              }
         reset_message(statusbox_w);
         break;

      case END_TIME :
         if (eval_time(value, w, &end_time_val, END_TIME) < 0)
         {
            show_message(statusbox_w, TIME_FORMAT);
         }
         else
         {
            reset_message(statusbox_w);
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         break;
 
      case HOST_ALIAS_NO_ENTER : /* Read the recipient. */
      case HOST_ALIAS :
         {
            int i = 0,
                ii = 0;

            if (no_of_search_host_alias != 0)
            {
               FREE_RT_ARRAY(search_host_alias);
               no_of_search_host_alias = 0;
            }
            ptr = value;
            for (;;)
            {
               while ((*ptr != '\0') && (*ptr != ','))
               {
                  if (*ptr == '\\')
                  {
                     ptr++;
                  }
                  ptr++;
               }
               no_of_search_host_alias++;
               if (*ptr == '\0')
               {
                  if (ptr == value)
                  {
                     no_of_search_host_alias = 0;
                  }
                  break;
               }
               ptr++;
            }
            if (no_of_search_host_alias > 0)
            {
               RT_ARRAY(search_host_alias, no_of_search_host_alias,
                        (MAX_HOSTNAME_LENGTH + 1), char);

               ptr = value;
               for (;;)
               {
                  i = 0;
                  while ((*ptr != '\0') && (*ptr != ','))
                  {
                     if (*ptr == '\\')
                     {
                        ptr++;
                     }
                     search_host_alias[ii][i] = *ptr;
                     ptr++; i++;
                  }
                  search_host_alias[ii][i] = '\0';
                  if (*ptr == ',')
                  {
                     ii++; ptr++;
                     while ((*ptr == ' ') || (*ptr == '\t'))
                     {
                        ptr++;
                     }
                  }
                  else
                  {
                     break;
                  }
               } /* for (;;) */
            } /* if (no_of_search_host_alias > 0) */
         }
         reset_message(statusbox_w);
         if (type == HOST_ALIAS)
         {
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         break;

      case DIR_ALIAS_NO_ENTER :
      case DIR_ALIAS :
         {
            int i = 0,
                ii = 0;

            if (no_of_search_dir_alias != 0)
            {
               FREE_RT_ARRAY(search_dir_alias);
               no_of_search_dir_alias = 0;
            }
            ptr = value;
            for (;;)
            {
               while ((*ptr != '\0') && (*ptr != ','))
               {
                  if (*ptr == '\\')
                  {
                     ptr++;
                  }
                  ptr++;
               }
               no_of_search_dir_alias++;
               if (*ptr == '\0')
               {
                  if (ptr == value)
                  {
                     no_of_search_dir_alias = 0;
                  }
                  break;
               }
               ptr++;
            }
            if (no_of_search_dir_alias > 0)
            {
               RT_ARRAY(search_dir_alias, no_of_search_dir_alias,
                        (MAX_DIR_ALIAS_LENGTH + 1), char);

               ptr = value;
               for (;;)
               {
                  i = 0;
                  while ((*ptr != '\0') && (*ptr != ','))
                  {
                     if (*ptr == '\\')
                     {
                        ptr++;
                     }
                     search_dir_alias[ii][i] = *ptr;
                     ptr++; i++;
                  }
                  search_dir_alias[ii][i] = '\0';
                  if (*ptr == ',')
                  {
                     ii++; ptr++;
                     while ((*ptr == ' ') || (*ptr == '\t'))
                     {
                        ptr++;
                     }
                  }
                  else
                  {
                     break;
                  }
               } /* for (;;) */
            } /* if (no_of_search_dir_alias > 0) */
         }
         reset_message(statusbox_w);
         if (type == DIR_ALIAS)
         {
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         break;

      case SEARCH_ADD_INFO_NO_ENTER :
         if (value[0] != '\0')
         {
            size_t length;

            (void)strcpy(&search_add_info[1], value);
            length = strlen(value);
            search_add_info[1 + length] = '*';
            search_add_info[1 + length + 1] = '\0';
         }
         else
         {
            search_add_info[1] = '\0';
         }
         reset_message(statusbox_w);
         break;

      case SEARCH_ADD_INFO :
         if (value[0] != '\0')
         {
            size_t length;

            (void)strcpy(&search_add_info[1], value);
            length = strlen(value);
            search_add_info[1 + length] = '*';
            search_add_info[1 + length + 1] = '\0';
         }
         else
         {
            search_add_info[1] = '\0';
         }
         reset_message(statusbox_w);
         XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         break;

      default :
         (void)fprintf(stderr, "ERROR   : Impossible! (%s %d)\n",
                       __FILE__, __LINE__);
         exit(INCORRECT);
   }
   XtFree(value);

   return;
}
