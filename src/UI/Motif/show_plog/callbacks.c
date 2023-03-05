/*
 *  callbacks.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2016 - 2023 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   callbacks - all callback functions for module show_plog
 **
 ** SYNOPSIS
 **   void confirmation_toggle(Widget w, XtPointer client_data, XtPointer call_data)
 **   void continues_toggle(Widget w, XtPointer client_data, XtPointer call_data)
 **   void radio_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void item_selection(Widget w, XtPointer client_data, XtPointer call_data)
 **   void info_click(Widget w, XtPointer client_data, XEvent *event)
 **   void search_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void select_all_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void close_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void save_input(Widget w, XtPointer client_data, XtPointer call_data)
 **   void scrollbar_moved(Widget w, XtPointer client_data, XtPointer call_data)
 **   void set_ratio_mode(Widget w, XtPointer client_data, XtPointer call_data)
 **   void set_sensitive(void)
 **
 ** DESCRIPTION
 **   The function toggled() is used to set the bits in the global
 **   variable toggles_set. The following bits can be set: SHOW_FTP,
 **   SHOW_SMTP and SHOW_FILE.
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
 **   search_button() activates the search in the production log. When
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
 **   14.09.2016 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <ctype.h>          /* isdigit()                                 */
#include <stdlib.h>         /* atol(), malloc(), free(), exit()          */
#include <limits.h>         /* INT_MAX                                   */
#include <Xm/Xm.h>
#include <Xm/List.h>
#include <Xm/Text.h>
#include <Xm/LabelP.h>
#include <errno.h>
#include "mafd_ctrl.h"
#include "show_plog.h"
#include "permission.h"

/* External global variables. */
extern Display                    *display;
extern Widget                     appshell,
                                  command_w,
                                  cont_togglebox_w,
                                  directory_w,
                                  end_time_w,
                                  file_length_w,
                                  headingbox_w,
                                  job_id_w,
                                  listbox_w,
                                  new_file_name_w,
                                  new_file_size_w,
                                  orig_file_name_w,
                                  orig_file_size_w,
                                  print_button_w,
                                  prod_time_w,
                                  return_code_w,
                                  scrollbar_w,
                                  select_all_button_w,
                                  selectionbox_w,
                                  special_button_w,
                                  start_time_w,
                                  statusbox_w,
                                  summarybox_w;
extern Window                     main_window;
extern int                        continues_toggle_set,
                                  items_selected,
                                  no_of_search_dirs,
                                  no_of_search_dirids,
                                  no_of_search_hosts,
                                  no_of_search_jobids,
                                  no_of_search_new_file_names,
                                  no_of_search_orig_file_names,
                                  no_of_search_production_cmd,
                                  file_name_length,
                                  *search_dir_length,
                                  ratio_mode,
                                  search_return_code,
                                  special_button_flag,
                                  sum_line_length,
                                  no_of_log_files,
                                  char_width;
extern unsigned int               all_list_items,
                                  *search_dirid,
                                  *search_jobid;
extern XT_PTR_TYPE                toggles_set;
extern time_t                     start_time_val,
                                  end_time_val;
extern size_t                     search_new_file_size,
                                  search_orig_file_size;
extern double                     search_cpu_time,
                                  search_prod_time;
extern char                       header_line[],
                                  multi_search_separator,
                                  **search_production_cmd,
                                  **search_new_file_name,
                                  **search_orig_file_name,
                                  **search_dir,
                                  *search_dir_filter,
                                  search_return_code_str[],
                                  **search_recipient,
                                  **search_user;
extern struct item_list           *il;
extern struct sol_perm            perm;
extern struct fileretrieve_status *fra;

/* Global variables. */
int                               do_search_return_code,
                                  gt_lt_sign_new,
                                  gt_lt_sign_orig,
                                  gt_lt_sign_ct,
                                  gt_lt_sign_pt,
                                  gt_lt_sign_rc,
                                  max_x,
                                  max_y;
char                              search_cpu_time_str[MAX_DISPLAYED_CPU_TIME + 2],
                                  search_new_file_size_str[20],
                                  search_orig_file_size_str[20],
                                  search_prod_time_str[MAX_DISPLAYED_PROD_TIME + 2],
                                  summary_str[MAX_PRODUCTION_LINE_LENGTH + SHOW_LONG_FORMAT + SHOW_LONG_FORMAT + 5 + 1],
                                  total_summary_str[MAX_PRODUCTION_LINE_LENGTH + SHOW_LONG_FORMAT + SHOW_LONG_FORMAT + 5 + 1];
struct info_data                  id;

/* Local global variables. */
static int                        scrollbar_moved_flag;


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


/*########################## item_selection() ###########################*/
void
item_selection(Widget w, XtPointer client_data, XtPointer call_data)
{
#ifdef _SMART_SELECTION
   static time_t        first_date_found,
                        last_date_found,
                        prev_first_date_found,
                        prev_last_date_found;
   static unsigned int  total_no_files;
   static double        cpu_time,
                        new_file_size,
                        orig_file_size,
                        prod_time;
   XmListCallbackStruct *cbs = (XmListCallbackStruct *)call_data;

   if (cbs->reason == XmCR_EXTENDED_SELECT)
   {
      char selection_summary_str[MAX_PRODUCTION_LINE_LENGTH + SHOW_LONG_FORMAT + 1 + SHOW_LONG_FORMAT + 5];

      if (cbs->selection_type == XmINITIAL)
      {
         /*
          * Initial selection
          */
         total_no_files = cbs->selected_item_count;
         if (get_sum_data((cbs->item_position - 1),
                          &first_date_found, &orig_file_size, &new_file_size,
                          &prod_time, &cpu_time) == INCORRECT)
         {
            return;
         }
         last_date_found = first_date_found;
      }
      else if (cbs->selection_type == XmMODIFICATION)
           {
              int    i;
              time_t date;
              double current_cpu_time,
                     current_new_file_size,
                     current_orig_file_size,
                     current_prod_time;

              /*
               * Modification of selection. Have to recalculate
               * everything.
               */
              total_no_files = cbs->selected_item_count;
              orig_file_size = new_file_size = prod_time = cpu_time = 0.0;
              first_date_found = -1;
              for (i = 0; i < cbs->selected_item_count; i++)
              {
                 if (get_sum_data((cbs->selected_item_positions[i] - 1), &date,
                                  &current_orig_file_size,
                                  &current_new_file_size, &current_prod_time,
                                  &current_cpu_time) == INCORRECT)
                 {
                    return;
                 }
                 if (first_date_found == -1)
                 {
                    first_date_found = date;
                 }
                 orig_file_size += current_orig_file_size;
                 new_file_size += current_new_file_size;
                 prod_time += current_prod_time;
                 cpu_time += current_cpu_time;
              }
              last_date_found = date;
           }
           else
           {
              time_t date;
              double current_new_file_size,
                     current_orig_file_size,
                     current_prod_time;

              /*
               * Additional selection. Add or subtract this selection to
               * current selection list.
               */
              if (get_sum_data((cbs->item_position - 1),
                               &date, &current_orig_file_size,
                               &current_new_file_size, &current_prod_time,
                               &current_cpu_time) == INCORRECT)
              {
                 return;
              }
              if (XmListPosSelected(listbox_w, cbs->item_position) == True)
              {
                 orig_file_size += current_orig_file_size;
                 new_file_size += current_new_file_size;
                 prod_time += current_prod_time;
                 cpu_time += current_cpu_time;
                 total_no_files++;
                 if (last_date_found < date)
                 {
                    prev_last_date_found = last_date_found;
                    last_date_found = date;
                 }
                 if (date < first_date_found)
                 {
                    prev_first_date_found = first_date_found;
                    first_date_found = date;
                 }
              }
              else
              {
                 orig_file_size -= current_orig_file_size;
                 new_file_size -= current_new_file_size;
                 prod_time -= current_prod_time;
                 cpu_time -= current_cpu_time;
                 total_no_files--;
                 if (date == first_date_found)
                 {
                    first_date_found = prev_first_date_found;
                    if (total_no_files == 1)
                    {
                       last_date_found = first_date_found;
                    }
                 }
                 else if (date == last_date_found)
                      {
                         last_date_found = prev_last_date_found;
                         if (total_no_files == 1)
                         {
                            first_date_found = last_date_found;
                         }
                      }
              }
           }
#else
   static time_t        first_date_found,
                        last_date_found;
   static unsigned int  total_no_files;
   static double        cpu_time,
                        new_file_size,
                        orig_file_size,
                        prod_time;
   XmListCallbackStruct *cbs = (XmListCallbackStruct *)call_data;

   if (cbs->reason == XmCR_EXTENDED_SELECT)
   {
      int    i;
      time_t date;
      double current_cpu_time,
             current_new_file_size,
             current_orig_file_size,
             current_prod_time;

      total_no_files = cbs->selected_item_count;
      orig_file_size = new_file_size = prod_time = cpu_time = 0.0;
      first_date_found = -1;
      for (i = 0; i < cbs->selected_item_count; i++)
      {
         if (get_sum_data((cbs->selected_item_positions[i] - 1), &date,
                          &current_orig_file_size, &current_new_file_size,
                          &current_prod_time, &current_cpu_time) == INCORRECT)
         {
            return;
         }
         if (first_date_found == -1)
         {
            first_date_found = date;
         }
         orig_file_size += current_orig_file_size;
         new_file_size += current_new_file_size;
         prod_time += current_prod_time;
         cpu_time += current_cpu_time;
      }
      last_date_found = date;
#endif

      /* Show summary. */
      if (cbs->selected_item_count > 0)
      {
         calculate_summary(summary_str, first_date_found, last_date_found,
                           total_no_files, orig_file_size, new_file_size,
                           prod_time, cpu_time);
      }
      else
      {
         (void)strcpy(summary_str, total_summary_str);
      }
      SHOW_SUMMARY_DATA();
      
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
      sum_line_length = sprintf(header_line, "%s%-*s %*s %-*s %s",
                                DATE_TIME_HEADER,
                                file_name_length, ORIG_FILE_NAME_HEADER,
                                MAX_DISPLAYED_FILE_SIZE, ORIG_FILE_SIZE_HEADER,
                                file_name_length, NEW_FILE_NAME_HEADER,
                                REST_HEADER);
      XmTextSetString(headingbox_w, header_line);

      window_width = char_width *
                     (MAX_PRODUCTION_LINE_LENGTH + file_name_length + 1 + file_name_length + 5);
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
         get_data();

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
      if ((max_pos > 0) && (pos <= max_pos))
      {
         char *text = NULL;

         /* Initialize text an data area. */
         id.no_of_files = 0;
         id.no_of_loptions = 0;
         id.no_of_soptions = 0;
         id.original_filename[0] = '\0';
         id.new_filename[0] = '\0';
         id.files = NULL;
#ifdef _WITH_DYNAMIC_MEMORY
         id.loptions = NULL;
#endif
         id.soptions = NULL;

         /* Get the information. */
         get_info(pos);
         get_info_free();

         /* Format information in a human readable text. */
         format_info(&text);

         /* Show the information. */
         show_info(text, NO);

         /* Free all memory that was allocated in get_info(). */
         free(text);
         free(id.files);
#ifdef _WITH_DYNAMIC_MEMORY
         if (id.loptions != NULL)
         {
            FREE_RT_ARRAY(id.loptions);
         }
#else
         free((void *)id.soptions);
#endif
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
      XtSetSensitive(cont_togglebox_w, False);
      XtSetSensitive(selectionbox_w, False);
      XtSetSensitive(start_time_w, False);
      XtSetSensitive(end_time_w, False);
      XtSetSensitive(orig_file_name_w, False);
      XtSetSensitive(new_file_name_w, False);
      XtSetSensitive(directory_w, False);
      XtSetSensitive(command_w, False);
      XtSetSensitive(orig_file_size_w, False);
      XtSetSensitive(new_file_size_w, False);
      XtSetSensitive(prod_time_w, False);
      XtSetSensitive(job_id_w, False);
      XtSetSensitive(return_code_w, False);
      XtSetSensitive(select_all_button_w, False);
      XtSetSensitive(print_button_w, False);

      scrollbar_moved_flag = NO;
      XtUnmanageChild(listbox_w);
      XmListDeleteAllItems(listbox_w);
      get_data();
      XtManageChild(listbox_w);

      /* Only position to last item when scrollbar was NOT moved! */
      if (scrollbar_moved_flag == NO)
      {
         XmListSetBottomPos(listbox_w, 0);
      }
   }
   else
   {
      set_sensitive();
      special_button_flag = STOP_BUTTON_PRESSED;
   }

   return;
}


/*######################## select_all_button() ##########################*/
void
select_all_button(Widget nw, XtPointer client_data, XtPointer call_data)
{
#ifdef VERY_SLOW_ALWAYS_WORKING_WRITE
   register int i;

   XtVaSetValues(listbox_w, XmNselectionPolicy, XmMULTIPLE_SELECT, NULL);
   for (i = 1; i <= all_list_items; i++)
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
   (void)strcpy(summary_str, total_summary_str);
   SHOW_SUMMARY_DATA();
   items_selected = YES;

   return;
}


/*########################### set_sensitive() ###########################*/
void
set_sensitive(void)
{
   XtSetSensitive(cont_togglebox_w, True);
   XtSetSensitive(selectionbox_w, True);
   XtSetSensitive(start_time_w, True);
   XtSetSensitive(end_time_w, True);
   XtSetSensitive(orig_file_name_w, True);
   XtSetSensitive(new_file_name_w, True);
   XtSetSensitive(directory_w, True);
   XtSetSensitive(command_w, True);
   XtSetSensitive(orig_file_size_w, True);
   XtSetSensitive(new_file_size_w, True);
   XtSetSensitive(prod_time_w, True);
   XtSetSensitive(job_id_w, True);
   XtSetSensitive(return_code_w, True);
   XtSetSensitive(select_all_button_w, True);
   XtSetSensitive(print_button_w, True);

   return;
}


/*########################## set_ratio_mode() ###########################*/
void
set_ratio_mode(Widget w, XtPointer client_data, XtPointer call_data)
{
#if SIZEOF_LONG == 4
   ratio_mode = (int)client_data;
#else
   union uintlong
         {
            unsigned int ival[2];
            long         lval;
         } uil;
   int   byte_order = 1;

   uil.lval = (long)client_data;
   if (*(char *)&byte_order == 1)
   {
      ratio_mode = uil.ival[0];
   }
   else
   {
      ratio_mode = uil.ival[1];
   }
#endif

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

      case ORIG_FILE_NAME_NO_ENTER :
      case ORIG_FILE_NAME :
         {
            int i = 0,
                ii = 0,
                nots = 0;

            if (no_of_search_orig_file_names != 0)
            {
               FREE_RT_ARRAY(search_orig_file_name);
               no_of_search_orig_file_names = 0;
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
               no_of_search_orig_file_names++;
               if (*ptr == '\0')
               {
                  if (ptr == value)
                  {
                     no_of_search_orig_file_names = 0;
                  }
                  break;
               }
               ptr++;
            }
            if (no_of_search_orig_file_names > 0)
            {
               if (nots == no_of_search_orig_file_names)
               {
                  RT_ARRAY(search_orig_file_name,
                           no_of_search_orig_file_names + 1,
                           (MAX_PATH_LENGTH + 1), char);
               }
               else
               {
                  RT_ARRAY(search_orig_file_name, no_of_search_orig_file_names,
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
                     search_orig_file_name[ii][i] = *ptr;
                     ptr++; i++;
                  }
                  search_orig_file_name[ii][i] = '\0';
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

               if (nots == no_of_search_orig_file_names)
               {
                  ii++;
                  search_orig_file_name[ii][0] = '*';
                  search_orig_file_name[ii][1] = '\0';
                  no_of_search_orig_file_names++;
               }
            } /* if (no_of_search_orig_file_names > 0) */
         }
         reset_message(statusbox_w);
         if (type == ORIG_FILE_NAME)
         {
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         break;

      case NEW_FILE_NAME_NO_ENTER :
      case NEW_FILE_NAME :
         {
            int i = 0,
                ii = 0,
                nots = 0;

            if (no_of_search_new_file_names != 0)
            {
               FREE_RT_ARRAY(search_new_file_name);
               no_of_search_new_file_names = 0;
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
               no_of_search_new_file_names++;
               if (*ptr == '\0')
               {
                  if (ptr == value)
                  {
                     no_of_search_new_file_names = 0;
                  }
                  break;
               }
               ptr++;
            }
            if (no_of_search_new_file_names > 0)
            {
               if (nots == no_of_search_new_file_names)
               {
                  RT_ARRAY(search_new_file_name,
                           no_of_search_new_file_names + 1,
                           (MAX_PATH_LENGTH + 1), char);
               }
               else
               {
                  RT_ARRAY(search_new_file_name, no_of_search_new_file_names,
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
                     search_new_file_name[ii][i] = *ptr;
                     ptr++; i++;
                  }
                  search_new_file_name[ii][i] = '\0';
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

               if (nots == no_of_search_new_file_names)
               {
                  ii++;
                  search_new_file_name[ii][0] = '*';
                  search_new_file_name[ii][1] = '\0';
                  no_of_search_new_file_names++;
               }
            } /* if (no_of_search_new_file_names > 0) */
         }
         reset_message(statusbox_w);
         if (type == NEW_FILE_NAME)
         {
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         break;

      case DIRECTORY_NAME_NO_ENTER :
      case DIRECTORY_NAME :
         {
            int is_dir_id,
                length,
                max_dir_length = 0,
                max_dirid_length = 0;

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
               search_dirid = NULL;
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
               char *p_dir,
                    *str_search_dirid = NULL;

               if (no_of_search_dirs > 0)
               {
                  RT_ARRAY(search_dir, no_of_search_dirs,
                           (max_dir_length + 2), char);
                  if ((search_dir_filter = malloc(no_of_search_dirs)) == NULL)
                  {
                     (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                                   strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
               }
               if (no_of_search_dirids > 0)
               {
                  if ((search_dirid = malloc((no_of_search_dirids * sizeof(int)))) == NULL)
                  {
                     (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                                   strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
                  if ((str_search_dirid = malloc((max_dirid_length + 1))) == NULL)
                  {
                     (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
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

      case COMMAND_NAME_NO_ENTER :
      case COMMAND_NAME :
         {
            int length,
                max_production_cmd_length = 0,
                not_counter = 0;

            if (no_of_search_production_cmd != 0)
            {
               free(search_production_cmd);
               search_production_cmd = NULL;
               no_of_search_production_cmd = 0;
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
                     no_of_search_production_cmd = 0;
                  }
                  break;
               }
               length = 0;
               if (*ptr == '!')
               {
                  not_counter++;
               }
               while ((*ptr != '\0') && (*ptr != ','))
               {
                  if (*ptr == '\\')
                  {
                     ptr++;
                  }
                  ptr++; length++;
               }
               no_of_search_production_cmd++;
               if (length > max_production_cmd_length)
               {
                  max_production_cmd_length = length;
               }
               if (*ptr == '\0')
               {
                  if (ptr == value)
                  {
                     no_of_search_production_cmd = 0;
                  }
                  break;
               }
               ptr++;
            }
            if (no_of_search_production_cmd > 0)
            {
               int add_all = NO,
                   i,
                   ii = 0;

               if (no_of_search_production_cmd == not_counter)
               {
                  no_of_search_production_cmd++;
                  add_all = YES;
               }
               RT_ARRAY(search_production_cmd, no_of_search_production_cmd,
                        (max_production_cmd_length + 1), char);
               ptr = value;
               for (;;)
               {
                  i = 0;
                  while ((*ptr == ' ') || (*ptr == '\t'))
                  {
                     if (*ptr == '\\')
                     {
                        ptr++;
                     }
                     ptr++;
                  }
                  if (*ptr == '\0')
                  {
                     break;
                  }

                  while ((*ptr != '\0') && (*ptr != ','))
                  {
                     if (*ptr == '\\')
                     {
                        ptr++;
                     }
                     search_production_cmd[ii][i] = *ptr;
                     ptr++; i++;
                  }
                  search_production_cmd[ii][i] = '\0';
                  ii++;
                  if (*ptr == ',')
                  {
                     ptr++;
                  }
                  else
                  {
                     break;
                  }
               } /* for (;;) */
               no_of_search_production_cmd = ii;
               if (add_all == YES)
               {
                  search_production_cmd[ii][0] = '*';
                  search_production_cmd[ii][1] = '\0';
                  no_of_search_production_cmd++;
               }
            }
         }
         reset_message(statusbox_w);
         if (type == COMMAND_NAME)
         {
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         break;

      case ORIG_FILE_SIZE_NO_ENTER :
      case ORIG_FILE_SIZE :
         if (value[0] == '\0')
         {
            search_orig_file_size = -1;
         }
         else
         {
            if (isdigit((int)value[0]))
            {
               gt_lt_sign_orig = EQUAL_SIGN;
            }
            else if (value[0] == '=')
                 {
                    extra_sign = 1;
                    gt_lt_sign_orig = EQUAL_SIGN;
                 }
            else if (value[0] == '<')
                 {
                    extra_sign = 1;
                    gt_lt_sign_orig = LESS_THEN_SIGN;
                 }
            else if (value[0] == '>')
                 {
                    extra_sign = 1;
                    gt_lt_sign_orig = GREATER_THEN_SIGN;
                 }
            else if (value[0] == '!')
                 {
                    extra_sign = 1;
                    gt_lt_sign_orig = NOT_SIGN;
                 }
                 else
                 {
                    show_message(statusbox_w, FILE_SIZE_FORMAT);
                    XtFree(value);
                    return;
                 }
            search_orig_file_size = (size_t)atol(value + extra_sign);
            (void)strcpy(search_orig_file_size_str, value);
         }
         reset_message(statusbox_w);
         if (type == ORIG_FILE_SIZE)
         {
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         break;

      case NEW_FILE_SIZE_NO_ENTER :
      case NEW_FILE_SIZE :
         if (value[0] == '\0')
         {
            search_new_file_size = -1;
         }
         else
         {
            if (isdigit((int)value[0]))
            {
               gt_lt_sign_new = EQUAL_SIGN;
            }
            else if (value[0] == '=')
                 {
                    extra_sign = 1;
                    gt_lt_sign_new = EQUAL_SIGN;
                 }
            else if (value[0] == '<')
                 {
                    extra_sign = 1;
                    gt_lt_sign_new = LESS_THEN_SIGN;
                 }
            else if (value[0] == '>')
                 {
                    extra_sign = 1;
                    gt_lt_sign_new = GREATER_THEN_SIGN;
                 }
            else if (value[0] == '!')
                 {
                    extra_sign = 1;
                    gt_lt_sign_new = NOT_SIGN;
                 }
                 else
                 {
                    show_message(statusbox_w, FILE_SIZE_FORMAT);
                    XtFree(value);
                    return;
                 }
            search_new_file_size = (size_t)atol(value + extra_sign);
            (void)strcpy(search_new_file_size_str, value);
         }
         reset_message(statusbox_w);
         if (type == NEW_FILE_SIZE)
         {
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         break;

      case RECIPIENT_NAME_NO_ENTER : /* Read the recipient. */
      case RECIPIENT_NAME : /* Read the recipient. */
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
                  while ((*ptr != '\0') && (*ptr != '@') && (*ptr != ','))
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
                     int tmp_i = i;

                     search_user[ii][i] = *ptr;
                     ptr++; i++;
                     ptr_start = ptr;
                     while ((*ptr != '\0') && (*ptr != '@') && (*ptr != ','))
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
                        ptr_start = ptr;
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
                        search_user[ii][tmp_i] = '\0';
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

      case JOB_ID_NO_ENTER :
      case JOB_ID :
         {
            int length,
                max_jobid_length = 0;

            if (no_of_search_jobids != 0)
            {
               free(search_jobid);
               search_jobid = NULL;
               no_of_search_jobids = 0;
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
                     no_of_search_jobids = 0;
                  }
                  break;
               }
               if (*ptr == '#')
               {
                  ptr++;
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
               no_of_search_jobids++;
               if (length > max_jobid_length)
               {
                  max_jobid_length = length;
               }
               if (*ptr == '\0')
               {
                  if (ptr == value)
                  {
                     no_of_search_jobids = 0;
                  }
                  break;
               }
               ptr++;
            }
            if (no_of_search_jobids > 0)
            {
               int  ii_jobids = 0;
               char *p_job,
                    *str_search_jobid = NULL;

               if (no_of_search_jobids > 0)
               {
                  if ((search_jobid = malloc((no_of_search_jobids * sizeof(int)))) == NULL)
                  {
                     (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                                   strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
                  if ((str_search_jobid = malloc((max_jobid_length + 1))) == NULL)
                  {
                     (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
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
                  p_job = str_search_jobid;
                  if (*ptr == '#')
                  {
                     ptr++;
                  }

                  while ((*ptr != '\0') && (*ptr != ','))
                  {
                     *p_job = *ptr;
                     ptr++; p_job++;
                  }
                  *p_job = '\0';
                  search_jobid[ii_jobids] = (unsigned int)strtoul(str_search_jobid, NULL, 16);
                  if (*ptr == ',')
                  {
                     ptr++;
                     ii_jobids++;
                  }
                  else
                  {
                     break;
                  }
               } /* for (;;) */
               free(str_search_jobid);
            }
         }
         reset_message(statusbox_w);
         if (type == JOB_ID)
         {
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         break;

      case RETURN_CODE_NO_ENTER :
      case RETURN_CODE :
         if (value[0] == '\0')
         {
            do_search_return_code = NO;
         }
         else
         {
            if ((isdigit((int)value[0])) || (value[0] == '-'))
            {
               gt_lt_sign_rc = EQUAL_SIGN;
            }
            else if (value[0] == '=')
                 {
                    extra_sign = 1;
                    gt_lt_sign_rc = EQUAL_SIGN;
                 }
            else if (value[0] == '<')
                 {
                    extra_sign = 1;
                    gt_lt_sign_rc = LESS_THEN_SIGN;
                 }
            else if (value[0] == '>')
                 {
                    extra_sign = 1;
                    gt_lt_sign_rc = GREATER_THEN_SIGN;
                 }
            else if (value[0] == '!')
                 {
                    extra_sign = 1;
                    gt_lt_sign_rc = NOT_SIGN;
                 }
                 else
                 {
                    show_message(statusbox_w, RETURN_CODE_FORMAT);
                    XtFree(value);
                    return;
                 }
            search_return_code = (size_t)atol(value + extra_sign);
            do_search_return_code = YES;
            (void)strcpy(search_return_code_str, value);
         }
         reset_message(statusbox_w);
         if (type == RETURN_CODE)
         {
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         break;

      case PROD_TIME_NO_ENTER :
      case PROD_TIME :
         if (value[0] == '\0')
         {
            search_prod_time = -1.0;
         }
         else
         {
            if (isdigit((int)value[0]))
            {
               gt_lt_sign_pt = EQUAL_SIGN;
            }
            else if (value[0] == '=')
                 {
                    extra_sign = 1;
                    gt_lt_sign_pt = EQUAL_SIGN;
                 }
            else if (value[0] == '<')
                 {
                    extra_sign = 1;
                    gt_lt_sign_pt = LESS_THEN_SIGN;
                 }
            else if (value[0] == '>')
                 {
                    extra_sign = 1;
                    gt_lt_sign_pt = GREATER_THEN_SIGN;
                 }
            else if (value[0] == '!')
                 {
                    extra_sign = 1;
                    gt_lt_sign_pt = NOT_SIGN;
                 }
                 else
                 {
                    show_message(statusbox_w, PROD_TIME_FORMAT);
                    XtFree(value);
                    return;
                 }
            search_prod_time = strtod(value + extra_sign, NULL);
            (void)strcpy(search_prod_time_str, value);
         }
         reset_message(statusbox_w);
         if (type == PROD_TIME)
         {
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         break;

      case CPU_TIME_NO_ENTER :
      case CPU_TIME :
         if (value[0] == '\0')
         {
            search_cpu_time = -1.0;
         }
         else
         {
            if (isdigit((int)value[0]))
            {
               gt_lt_sign_pt = EQUAL_SIGN;
            }
            else if (value[0] == '=')
                 {
                    extra_sign = 1;
                    gt_lt_sign_ct = EQUAL_SIGN;
                 }
            else if (value[0] == '<')
                 {
                    extra_sign = 1;
                    gt_lt_sign_ct = LESS_THEN_SIGN;
                 }
            else if (value[0] == '>')
                 {
                    extra_sign = 1;
                    gt_lt_sign_ct = GREATER_THEN_SIGN;
                 }
            else if (value[0] == '!')
                 {
                    extra_sign = 1;
                    gt_lt_sign_ct = NOT_SIGN;
                 }
                 else
                 {
                    show_message(statusbox_w, CPU_TIME_FORMAT);
                    XtFree(value);
                    return;
                 }
            search_cpu_time = strtod(value + extra_sign, NULL);
            (void)strcpy(search_cpu_time_str, value);
         }
         reset_message(statusbox_w);
         if (type == CPU_TIME)
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
