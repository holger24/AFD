/*
 *  callbacks.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2023 Holger Kiehl <Holger.Kiehl@@dwd.de>
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
 **   callbacks - all callback functions for module show_queue
 **
 ** SYNOPSIS
 **   void toggled(Widget w, XtPointer client_data, XtPointer call_data)
 **   void radio_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void item_selection(Widget w, XtPointer client_data, XtPointer call_data)
 **   void info_click(Widget w, XtPointer client_data, XEvent *event)
 **   void search_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void select_all_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void close_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void delete_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void save_input(Widget w, XtPointer client_data, XtPointer call_data)
 **   void scrollbar_moved(Widget w, XtPointer client_data, XtPointer call_data)
 **   void send_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void view_button(Widget w, XtPointer client_data, XtPointer call_data)
 **
 ** DESCRIPTION
 **   The function toggled() is used to set the bits in the global
 **   variable toggles_set. The following bits can be set: SHOW_INPUT,
 **   SHOW_OUTPUT, SHOW_RETRIEVES, SHOW_UNSENT_INPUT, SHOW_UNSENT_OUTPUT,
 **   SHOW_PENDING_RETRIEVES and SHOW_TIME_JOBS.
 **
 **   Function item_selection() calculates a new summary string of
 **   the items that are currently selected and displays them.
 **
 **   The famous 'AFD Info Click' is done by info_click(). When clicking on
 **   an item with the middle or right mouse button in the list widget,
 **   it list the following information: file name, directory, filter,
 **   recipient, AMG-options, FD-options, priority, job ID and archive
 **   directory.
 **
 **   search_button() activates the search in the internal queues of AFD.
 **   When pressed the label of the button changes to 'Stop'. Now the user
 **   has the chance to stop the search. During the search only the
 **   list widget and the Stop button can be used.
 **
 **   delete_button() deletes all selected files from the AFD queue.
 **
 **   send_button() sends all selected files to some destination.
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
 **   21.07.2001 H.Kiehl Created
 **   23.11.2003 H.Kiehl Disallow user to change window width even if
 **                      window manager allows this, but allow to change
 **                      the height.
 **   07.02.2005 H.Kiehl Added sending files from queue.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <ctype.h>          /* isdigit()                                 */
#include <stdlib.h>         /* atol(), free(), strtoul(), exit()         */
#include <Xm/Xm.h>
#include <Xm/List.h>
#include <Xm/Text.h>
#include <errno.h>
#include "motif_common_defs.h"
#include "show_queue.h"

/* External global variables. */
extern Display                    *display;
extern Widget                     appshell,
                                  listbox_w,
                                  headingbox_w,
                                  select_all_button_w,
                                  statusbox_w,
                                  summarybox_w,
                                  view_button_w;
extern Window                     main_window;
extern int                        char_width,
                                  file_name_length,
                                  items_selected,
                                  no_of_search_dirs,
                                  no_of_search_dirids,
                                  no_of_search_file_names,
                                  no_of_search_hosts,
                                  queue_tmp_buf_entries,
                                  *search_dir_length,
                                  special_button_flag;
extern XT_PTR_TYPE                toggles_set;
extern unsigned int               *search_dirid,
                                  total_no_files;
extern double                     total_file_size;
extern time_t                     start_time_val,
                                  end_time_val;
extern size_t                     search_file_size;
extern char                       multi_search_separator,
                                  *search_dir_filter,
                                  **search_file_name,
                                  **search_dir,
                                  **search_recipient,
                                  **search_user;
extern struct queued_file_list    *qfl;
extern struct queue_tmp_buf       *qtb;
extern struct fileretrieve_status *fra;

/* Global variables. */
int                               gt_lt_sign,
                                  max_x,
                                  max_y;
char                              search_file_size_str[20],
                                  summary_str[MAX_OUTPUT_LINE_LENGTH + SHOW_LONG_FORMAT + 5],
                                  total_summary_str[MAX_OUTPUT_LINE_LENGTH + SHOW_LONG_FORMAT + 5];

/* Local global variables. */
static int                        scrollbar_moved_flag;


/*############################## toggled() ##############################*/
void
toggled(Widget w, XtPointer client_data, XtPointer call_data)
{
   toggles_set ^= (XT_PTR_TYPE)client_data;

   return;
}


/*########################## item_selection() ###########################*/
void
item_selection(Widget w, XtPointer client_data, XtPointer call_data)
{
   XmListCallbackStruct *cbs = (XmListCallbackStruct *)call_data;

   if (cbs->reason == XmCR_EXTENDED_SELECT)
   {
      int          i;
      unsigned int no_files_selected;
      double       file_size_selected;

      no_files_selected = cbs->selected_item_count;
      file_size_selected = 0.0;
      for (i = 0; i < cbs->selected_item_count; i++)
      {
         file_size_selected += qfl[(cbs->selected_item_positions[i] - 1)].size;
      }

      /* Show summary. */
      if (cbs->selected_item_count > 0)
      {
         show_summary(no_files_selected, file_size_selected);
      }
      else
      {
         show_summary(total_no_files, total_file_size);
      }
      items_selected = YES;
   }

   return;
}


/*########################### radio_button() ############################*/
void
radio_button(Widget w, XtPointer client_data, XtPointer call_data)
{
#if SIZEOF_LONG == 4
   int new_file_name_length = (int)client_data;
#else
   union intlong
         {
            int  ival[2];
            long lval;
         } il;
   int   byte_order = 1,
         new_file_name_length;

   il.lval = (long)client_data;
   if (*(char *)&byte_order == 1)
   {
      new_file_name_length = il.ival[0];
   }
   else
   {
      new_file_name_length = il.ival[1];
   }
#endif

   if (new_file_name_length != file_name_length)
   {
      Window       root_return;
      int          x,
                   y,
                   no_of_items;
      Dimension    window_width;
      unsigned int width,
                   window_height,
                   border,
                   depth;

      file_name_length = new_file_name_length;

      XGetGeometry(display, main_window, &root_return, &x, &y, &width, &window_height, &border, &depth);
      if (file_name_length == SHOW_SHORT_FORMAT)
      {
         XmTextSetString(headingbox_w, HEADING_LINE_SHORT);
      }
      else if (file_name_length == SHOW_MEDIUM_FORMAT)
           {
              XmTextSetString(headingbox_w, HEADING_LINE_MEDIUM);
           }
           else
           {
              XmTextSetString(headingbox_w, HEADING_LINE_LONG);
           }

      window_width = char_width *
                     (MAX_OUTPUT_LINE_LENGTH + file_name_length + 6);
      XtVaSetValues(appshell,
                    XmNminWidth, window_width,
                    XmNmaxWidth, window_width,
                    NULL);
      XResizeWindow(display, main_window, window_width, window_height);

      XtVaGetValues(listbox_w, XmNitemCount, &no_of_items, NULL);
      if (no_of_items > 0)
      {
         scrollbar_moved_flag = NO;
         XmListDeleteAllItems(listbox_w);
         if ((total_no_files > 0) && (qfl != NULL))
         {
            display_data();
         }

         /* Only position to last item when scrollbar was NOT moved! */
         if (scrollbar_moved_flag == NO)
         {
            XmListSetBottomPos(listbox_w, 0);
         }
      }
   }

   return;
}


/*############################ info_click() #############################*/
void
info_click(Widget w, XtPointer client_data, XEvent *event)
{
   if ((event->xbutton.button == Button2) ||
       (event->xbutton.button == Button3))
   {
      int  pos = XmListYToPos(w, event->xbutton.y),
           max_pos;

      /* Check if pos is valid. */
      XtVaGetValues(w, XmNitemCount, &max_pos, NULL);
      if ((max_pos > 0) && (pos > 0) && (pos <= max_pos))
      {
         int  with_search_function;
         char *text = NULL;

         /* Format information in a human readable text. */
         if ((qfl[pos - 1].queue_type == SHOW_OUTPUT) ||
             (qfl[pos - 1].queue_type == SHOW_TIME_JOBS))
         {
            format_output_info(&text, pos - 1);
            with_search_function = NO;
         }
         else if ((qfl[pos - 1].queue_type == SHOW_RETRIEVES) ||
                  (qfl[pos - 1].queue_type == SHOW_PENDING_RETRIEVES))
              {
                 format_retrieve_info(&text, pos - 1);
                 with_search_function = YES;
              }
              else /* SHOW_INPUT */
              {
                 format_input_info(&text, pos - 1);
                 with_search_function = YES;
              }

         if (text != NULL)
         {
            /* Show the information. */
            show_info(text, with_search_function);
            free(text);
         }
      }
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


/*########################## search_button() ############################*/
void
search_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   if (special_button_flag == SEARCH_BUTTON)
   {
      scrollbar_moved_flag = NO;
      XmListDeleteAllItems(listbox_w);
      if (qfl != NULL)
      {
         int i;

         for (i = 0; i < total_no_files; i++)
         {
            free(qfl[i].file_name);
         }
         free(qfl);
         qfl = NULL;
      }
      if (qtb != NULL)
      {
         int i;

         for (i = 0; i < queue_tmp_buf_entries; i++)
         {
            if (qtb[i].qfl_pos != NULL)
            {
               free(qtb[i].qfl_pos);
            }
         }
         free(qtb);
         qtb = NULL;
         queue_tmp_buf_entries = 0;
      }
      get_data();

      /* Only position to last item when scrollbar was NOT moved! */
      if (scrollbar_moved_flag == NO)
      {
         XmListSetBottomPos(listbox_w, 0);
      }
   }
   else
   {
      special_button_flag = STOP_BUTTON_PRESSED;
   }

   return;
}


/*######################## select_all_button() ##########################*/
void
select_all_button(Widget w, XtPointer client_data, XtPointer call_data)
{
#ifdef VERY_SLOW_ALWAYS_WORKING_WRITE
   register int i;

   XtVaSetValues(listbox_w, XmNselectionPolicy, XmMULTIPLE_SELECT, NULL);
   for (i = 1; i <= total_no_files; i++)
   {
      if (XmListPosSelected(listbox_w, i) == False)
      {
         XmListSelectPos(listbox_w, i, False);
      }
   }
   XtVaSetValues(listbox_w, XmNselectionPolicy, XmEXTENDED_SELECT, NULL);
#else
   XtCallActionProc(listbox_w, "ListKbdSelectAll", NULL, NULL, 0);
#endif
   show_summary(total_no_files, total_file_size);
   items_selected = YES;

   return;
}


/*############################ view_button() ############################*/
void
view_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   int no_selected,
       *select_list;

   reset_message(statusbox_w);
   if (XmListGetSelectedPos(listbox_w, &select_list, &no_selected) == True)
   {
      view_files(no_selected, select_list);
      XtFree((char *)select_list);
   }
   else
   {
      show_message(statusbox_w, "No file selected!");
   }

   return;
}


/*########################### send_button() #############################*/
void
send_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   int no_selected,
       *select_list;

   reset_message(statusbox_w);
   if (XmListGetSelectedPos(listbox_w, &select_list, &no_selected) == True)
   {
      send_files(no_selected, select_list);
      XtFree((char *)select_list);
   }
   else
   {
      show_message(statusbox_w, "No file selected!");
   }

   return;
}


/*########################## delete_button() ############################*/
void
delete_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   int no_selected,
       *select_list;

   reset_message(statusbox_w);
   if (XmListGetSelectedPos(listbox_w, &select_list, &no_selected) == True)
   {
      delete_files(no_selected, select_list);
      XtFree((char *)select_list);
   }
   else
   {
      show_message(statusbox_w, "No file selected!");
   }

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
   if (qfl != NULL)
   {
      int i;

      for (i = 0; i < total_no_files; i++)
      {
         free(qfl[i].file_name);
      }
      free(qfl);
   }
   exit(0);
}


/*############################# save_input() ############################*/
void
save_input(Widget w, XtPointer client_data, XtPointer call_data)
{
   int         extra_sign = 0;
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
 
      case FILE_NAME_NO_ENTER :
      case FILE_NAME :
         {
            int  i = 0,
                 ii = 0,
                 nots = 0;
            char *ptr;

            if (no_of_search_file_names != 0)
            {
               FREE_RT_ARRAY(search_file_name);
               no_of_search_file_names = 0;
            }
            ptr = value;
            for (;;)
            {
               while ((*ptr == ' ') || (*ptr == '\t'))
               {
                  ptr++;
               }
               if (*ptr == '!')
               {
                  nots++;
               }
               while ((*ptr != '\0') && (*ptr != multi_search_separator))
               {
                  if (*ptr == '\\')
                  {
                     ptr++;
                  }
                  ptr++;
               }
               no_of_search_file_names++;
               if (*ptr == '\0')
               {
                  if (ptr == value)
                  {
                     no_of_search_file_names = 0;
                  }
                  break;
               }
               ptr++;
            }
            if (no_of_search_file_names > 0)
            {
               if (nots == no_of_search_file_names)
               {
                  RT_ARRAY(search_file_name, no_of_search_file_names + 1,
                           (MAX_PATH_LENGTH + 1), char);
               }
               else
               {
                  RT_ARRAY(search_file_name, no_of_search_file_names,
                           (MAX_PATH_LENGTH + 1), char);
               }

               ptr = value;
               for (;;)
               {
                  i = 0;
                  while ((*ptr != '\0') && (*ptr != multi_search_separator))
                  {
                     if (*ptr == '\\')
                     {
                        ptr++;
                     }
                     search_file_name[ii][i] = *ptr;
                     ptr++; i++;
                  }
                  search_file_name[ii][i] = '\0';
                  if (*ptr == multi_search_separator)
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

               if (nots == no_of_search_file_names)
               {
                  ii++;
                  search_file_name[ii][0] = '*';
                  search_file_name[ii][1] = '\0';
                  no_of_search_file_names++;
               }
            } /* if (no_of_search_file_names > 0) */
         }
         reset_message(statusbox_w);
         if (type == FILE_NAME)
         {
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         break;

      case DIRECTORY_NAME_NO_ENTER :
      case DIRECTORY_NAME :
         {
            int  is_dir_id,
                 length,
                 max_dir_length = 0,
                 max_dirid_length = 0;
            char *ptr;

            if (no_of_search_dirs != 0)
            {
               FREE_RT_ARRAY(search_dir);
               free(search_dir_length);
               search_dir_length = NULL;
               free(search_dir_filter);
               search_dir_filter = NULL;
               no_of_search_dirs = 0;
            }
            if (no_of_search_dirids != 0)
            {
               free(search_dirid);
               no_of_search_dirids = 0;
            }
            ptr = value;
            for (;;)
            {
               while (*ptr == ' ')
               {
                  if (*ptr == '\\')
                  {
                     ptr++;
                  }
                  ptr++;
               }
               if (*ptr == '\0')
               {
                  if (ptr == value)
                  {
                     no_of_search_dirs = 0;
                     no_of_search_dirids = 0;
                  }
                  break;
               }
               if ((*ptr == '#') || (*ptr == '@'))
               {
                  is_dir_id = YES;
                  ptr++;
               }
               else
               {
                  is_dir_id = NO;
               }
               length = 0;
               while ((*ptr != '\0') && (*ptr != ','))
               {
                  if (*ptr == '\\')
                  {
                     ptr++;
                  }
                  ptr++; length++;
               }
               if (is_dir_id == YES)
               {
                  no_of_search_dirids++;
                  if (length > max_dirid_length)
                  {
                     max_dirid_length = length;
                  }
               }
               else
               {
                  no_of_search_dirs++;
                  if (length > max_dir_length)
                  {
                     max_dir_length = length;
                  }
                  if ((search_dir_length = realloc(search_dir_length,
                                                   (no_of_search_dirs * sizeof(int)))) == NULL)
                  {
                     (void)fprintf(stderr, "realloc() error : %s (%s %d)\n",
                                   strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
                  search_dir_length[(no_of_search_dirs - 1)] = length;
               }
               if (*ptr == '\0')
               {
                  if (ptr == value)
                  {
                     no_of_search_dirs = 0;
                     no_of_search_dirids = 0;
                  }
                  break;
               }
               ptr++;
            }
            if ((no_of_search_dirs > 0) || (no_of_search_dirids > 0))
            {
               int  ii_dirs = 0,
                    ii_dirids = 0;
               char *str_search_dirid = NULL,
                    *p_dir;

               if (no_of_search_dirs > 0)
               {
                  RT_ARRAY(search_dir, no_of_search_dirs,
                           (max_dir_length + 1), char);
                  if ((search_dir_filter = malloc(no_of_search_dirs)) == NULL)
                  {
                     (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                                   strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
               }
               if (no_of_search_dirids > 0)
               {
                  if ((search_dirid = malloc(no_of_search_dirids * sizeof(unsigned int))) == NULL)
                  {
                     (void)fprintf(stderr, "Failed to malloc() %d bytes : %s (%s %d)\n",
                                   no_of_search_dirids * (int)sizeof(unsigned int),
                                   strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
                  if ((str_search_dirid = malloc((max_dirid_length + 1))) == NULL)
                  {
                     (void)fprintf(stderr, "Failed to malloc() %d bytes : %s (%s %d)\n",
                                   max_dirid_length + 1,
                                   strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
               }

               ptr = value;
               for (;;)
               {
                  while ((*ptr == ' ') || (*ptr == '\t'))
                  {
                     if (*ptr == '\\')
                     {
                        ptr++;
                     }
                     ptr++;
                  }
                  if (*ptr == '#')
                  {
                     p_dir = str_search_dirid;
                     ptr++;

                     while ((*ptr != '\0') && (*ptr != ','))
                     {
                        *p_dir = *ptr;
                        ptr++; p_dir++;
                     }
                     *p_dir = '\0';
                     search_dirid[ii_dirids] = (unsigned int)strtoul(str_search_dirid, NULL, 16);
                     if (*ptr == ',')
                     {
                        ptr++;
                        ii_dirids++;
                     }
                     else
                     {
                        break;
                     }
                  }
                  else if (*ptr == '@')
                       {
                          p_dir = str_search_dirid;
                          ptr++;

                          while ((*ptr != '\0') && (*ptr != ','))
                          {
                             *p_dir = *ptr;
                             ptr++; p_dir++;
                          }
                          *p_dir = '\0';
                          if (get_dir_id(str_search_dirid,
                                         &search_dirid[ii_dirids]) == INCORRECT)
                          {
                             no_of_search_dirids--;
                             if (*ptr == ',')
                             {
                                ptr++;
                             }
                             else
                             {
                                break;
                             }
                          }
                          else
                          {
                             if (*ptr == ',')
                             {
                                ptr++;
                                ii_dirids++;
                             }
                             else
                             {
                                break;
                             }
                          }
                       }
                       else
                       {
                          p_dir = search_dir[ii_dirs];

                          search_dir_filter[ii_dirs] = NO;
                          while ((*ptr != '\0') && (*ptr != ','))
                          {
                             if (*ptr == '\\')
                             {
                                ptr++;
                             }
                             else
                             {
                                if ((*ptr == '?') || (*ptr == '*') || (*ptr == '['))
                                {
                                   search_dir_filter[ii_dirs] = YES;
                                }
                             }
                             *p_dir = *ptr;
                             ptr++; p_dir++;
                          }
                          *p_dir = '\0';
                          if (*ptr == ',')
                          {
                             ptr++;
                             ii_dirs++;
                          }
                          else
                          {
                             break;
                          }
                       }
               } /* for (;;) */
               if (fra != NULL)
               {
                  (void)fra_detach();
                  fra = NULL;
               }
               free(str_search_dirid);
            }
         }
         reset_message(statusbox_w);
         if (type == DIRECTORY_NAME)
         {
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         break;

      case FILE_LENGTH_NO_ENTER :
      case FILE_LENGTH :
         if (value[0] == '\0')
         {
            search_file_size = -1;
         }
         else
         {
            if (isdigit((int)value[0]))
            {
               gt_lt_sign = EQUAL_SIGN;
            }
            else if (value[0] == '=')
                 {
                    extra_sign = 1;
                    gt_lt_sign = EQUAL_SIGN;
                 }
            else if (value[0] == '<')
                 {
                    extra_sign = 1;
                    gt_lt_sign = LESS_THEN_SIGN;
                 }
            else if (value[0] == '>')
                 {
                    extra_sign = 1;
                    gt_lt_sign = GREATER_THEN_SIGN;
                 }
            else if (value[0] == '!')
                 {
                    extra_sign = 1;
                    gt_lt_sign = NOT_SIGN;
                 }
                 else
                 {
                    show_message(statusbox_w, FILE_SIZE_FORMAT);
                    XtFree(value);
                    return;
                 }
            search_file_size = (size_t)atol(value + extra_sign);
            (void)strcpy(search_file_size_str, value);
         }
         reset_message(statusbox_w);
         if (type == FILE_LENGTH)
         {
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         break;

      case RECIPIENT_NAME_NO_ENTER : /* Read the recipient. */
      case RECIPIENT_NAME :
         {
            int  i = 0,
                 ii = 0;
            char *ptr_start;

            if (no_of_search_hosts != 0)
            {
               FREE_RT_ARRAY(search_recipient);
               FREE_RT_ARRAY(search_user);
               no_of_search_hosts = 0;
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
               no_of_search_hosts++;
               if (*ptr == '\0')
               {
                  if (ptr == value)
                  {
                     no_of_search_hosts = 0;
                  }
                  break;
               }
               ptr++;
            }
            if (no_of_search_hosts > 0)
            {
               RT_ARRAY(search_recipient, no_of_search_hosts,
                        (MAX_RECIPIENT_LENGTH + 1), char);
               RT_ARRAY(search_user, no_of_search_hosts,
                        (MAX_RECIPIENT_LENGTH + 1), char);

               ptr = value;
               for (;;)
               {
                  ptr_start = ptr;
                  i = 0;
                  while ((*ptr != '\0') && (*ptr != ',') && (*ptr != '@'))
                  {
                     if (*ptr == '\\')
                     {
                        ptr++;
                     }
                     search_user[ii][i] = *ptr;
                     ptr++; i++;
                  }
                  if (*ptr == '@')
                  {
                     ptr++;
                     search_user[ii][i] = '\0';
                     ptr_start = ptr;;
                     while ((*ptr != '\0') && (*ptr != ','))
                     {
                        if (*ptr == '\\')
                        {
                           ptr++;
                        }
                        ptr++;
                     }
                  }
                  else
                  {
                     search_user[ii][0] = '\0';
                  }
                  if (*ptr == ',')
                  {
                     *ptr = '\0';
                     (void)strcpy(search_recipient[ii], ptr_start);
                     ii++; ptr++;
                     while ((*ptr == ' ') || (*ptr == '\t'))
                     {
                        ptr++;
                     }
                  }
                  else
                  {
                     (void)strcpy(search_recipient[ii], ptr_start);
                     break;
                  }
               } /* for (;;) */
            } /* if (no_of_search_hosts > 0) */
         }
         reset_message(statusbox_w);
         if (type == RECIPIENT_NAME)
         {
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         break;

      default :
         (void)fprintf(stderr, "ERROR   : Impossible! (%s %d)\n",
                       __FILE__, __LINE__);
         exit(INCORRECT);
   }
   XtFree(value);

   return;
}
