/*
 *  callbacks.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   callbacks - all callback functions for module show_olog
 **
 ** SYNOPSIS
 **   void radio_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void item_selection(Widget w, XtPointer client_data, XtPointer call_data)
 **   void info_click(Widget w, XtPointer client_data, XEvent *event)
 **   void search_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void select_all_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void resend_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void close_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void save_input(Widget w, XtPointer client_data, XtPointer call_data)
 **   void scrollbar_moved(Widget w, XtPointer client_data, XtPointer call_data)
 **
 ** DESCRIPTION
 **   The famous 'AFD Info Click' is done by info_click(). When clicking on
 **   an item with the middle or right mouse button in the list widget,
 **   it list the following information: file name, directory, filter,
 **   recipient, AMG-options, FD-options, priority, and job ID.
 **
 **   Function item_selection() calculates a new summary string of
 **   the items that are currently selected and displays them.
 **
 **   search_button() activates the search in the output log. When
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
 **   27.05.1997 H.Kiehl Created
 **   15.02.1999 H.Kiehl Multiple recipients.
 **   23.11.2003 H.Kiehl Disallow user to change window width even if
 **                      window manager allows this, but allow to change
 **                      height.
 **   09.08.2009 H.Kiehl Add support for showing where file was
 **                      distributed.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <ctype.h>          /* isdigit()                                 */
#include <stdlib.h>         /* atol(), malloc(), free(), exit()          */
#include <time.h>           /* localtime(), mktime(), strftime()         */
#include <sys/time.h>       /* struct tm                                 */
#include <Xm/Xm.h>
#include <Xm/List.h>
#include <Xm/Text.h>
#include <Xm/LabelP.h>
#include <errno.h>
#include "mafd_ctrl.h"
#include "show_ilog.h"

/* #define SHOW_ALDA_CMD */

/* External global variables. */
extern Display                    *display;
extern Widget                     cont_togglebox_w,
                                  directory_w,
                                  end_time_w,
                                  file_length_w,
                                  file_name_w,
                                  headingbox_w,
                                  listbox_w,
                                  print_button_w,
                                  radiobox_w,
                                  recipient_w,
                                  select_all_button_w,
                                  send_button_w,
                                  start_time_w,
                                  statusbox_w,
                                  summarybox_w,
                                  appshell;
extern Window                     main_window;
extern int                        char_width,
                                  continues_toggle_set,
                                  file_name_length,
                                  no_of_log_files,
                                  no_of_search_dirs,
                                  no_of_search_dirids,
                                  no_of_search_file_names,
                                  no_of_search_hosts,
                                  *search_dir_length,
                                  special_button_flag,
                                  sum_line_length;
extern unsigned int               all_list_items,
                                  *search_dirid;
extern time_t                     start_time_val,
                                  end_time_val;
extern size_t                     search_file_size;
extern char                       font_name[],
                                  header_line[],
                                  multi_search_separator,
                                  *p_work_dir,
                                  **search_file_name,
                                  **search_dir,
                                  *search_dir_filter,
                                  **search_recipient,
                                  **search_user;
extern struct item_list           *il;
extern struct fileretrieve_status *fra;

/* Global variables. */
int                               gt_lt_sign,
                                  max_x,
                                  max_y;
char                              search_file_size_str[20],
                                  summary_str[MAX_OUTPUT_LINE_LENGTH + SHOW_LONG_FORMAT + 5 + 1],
                                  total_summary_str[MAX_OUTPUT_LINE_LENGTH + SHOW_LONG_FORMAT + 5 + 1];
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
      sum_line_length = sprintf(header_line, "%s%-*s  %s",
                                DATE_TIME_HEADER, file_name_length,
                                FILE_NAME_HEADER, REST_HEADER);
      XmTextSetString(headingbox_w, header_line);

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


/*########################## item_selection() ###########################*/
void
item_selection(Widget w, XtPointer client_data, XtPointer call_data)
{
   static time_t        first_date_found,
                        last_date_found;
   static unsigned int  total_no_files;
   static double        file_size;
   XmListCallbackStruct *cbs = (XmListCallbackStruct *)call_data;

   if (cbs->reason == XmCR_EXTENDED_SELECT)
   {
      int    i;
      time_t date;
      double current_file_size;

      total_no_files = cbs->selected_item_count;
      file_size = 0.0;
      first_date_found = -1;
      for (i = 0; i < cbs->selected_item_count; i++)
      {
         if (get_sum_data((cbs->selected_item_positions[i] - 1),
                          &date, &current_file_size) == INCORRECT)
         {
            return;
         }
         if (first_date_found == -1)
         {
            first_date_found = date;
         }
         file_size += current_file_size;
      }
      last_date_found = date;

      /* Show summary. */
      if (cbs->selected_item_count > 0)
      {
         calculate_summary(summary_str, first_date_found, last_date_found,
                           total_no_files, file_size);
      }
      else
      {
         (void)strcpy(summary_str, total_summary_str);
      }
      SHOW_SUMMARY_DATA();
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
         int  i;
         char *text = NULL;

         /* Initialize text an data area. */
         id.count  = 0;
         id.dir[0] = '\0';
         id.dbe    = NULL;

         /* Get the information. */
         get_info(pos);
         get_info_free();

         if (event->xbutton.button == Button2)
         {
            char      alda_cmd[MAX_PATH_LENGTH + MAX_FILENAME_LENGTH + MAX_INT_HEX_LENGTH + MAX_INT_LENGTH + 1],
                      alda_time_str_end[9],
                      alda_time_str_start[9];
            time_t    time_val;
            struct tm *p_ts;

            time_val = id.arrival_time;
            p_ts = localtime(&time_val);
            if (p_ts == NULL)
            {
               (void)fprintf(stderr, "localtime() error : %s\n",
                             strerror(errno));
               exit(INCORRECT);
            }
            if (strftime(alda_time_str_start, sizeof(alda_time_str_start),
                         "%m%d%H%M", p_ts) == 0)
            {
               (void)fprintf(stderr, "strftime() error : %s\n",
                             strerror(errno));
               exit(INCORRECT);
            }
            time_val += 60;
            p_ts = localtime(&time_val);
            if (p_ts == NULL)
            {
               (void)fprintf(stderr, "localtime() error : %s\n",
                             strerror(errno));
               exit(INCORRECT);
            }
            if (strftime(alda_time_str_end, sizeof(alda_time_str_end),
                         "%m%d%H%M", p_ts) == 0)
            {
               (void)fprintf(stderr, "strftime() error : %s\n",
                             strerror(errno));
               exit(INCORRECT);
            }
            /* -o Host alias|Realhostname|Finalname|Size|HRsize|Delivery time|Duration|JID|Retries|Split Job Number|Archive Dir|Delete time|Job ID|Disttype|UJID entries|UJID list|Reason ID|User/program|Additional reason */
            (void)sprintf(alda_cmd,
                          "%s -f -g %s -t %s-%s -u %x -d \\#%x -SI %s -o '%%OH|%%Oh|%%OE|%%xOSB|%%.3OSA|%%xOTu|%%ODX|%%xOJ|%%xOe|%%xOL|%%OA|%%xDTu|%%xDJ|%%Pf|%%PF|%%xPJ|%%xUY|%%xUn|%%xUj,|%%xDr|%%DW|%%DA' %s",
                          ALDA_CMD, MAX_ALDA_DIFF_TIME_STR, alda_time_str_start,
                          alda_time_str_end, id.unique_number, id.dir_id,
                          id.file_size, id.file_name);
#ifdef SHOW_ALDA_CMD
            (void)fprintf(stdout, "%s\n", alda_cmd);
#endif
            if ((exec_cmd(alda_cmd, &text, -1, NULL, 0,
#ifdef HAVE_SETPRIORITY
                          NO_PRIORITY,
#endif
                          "", NULL, NULL, 0, 0L, NO, NO) != 0) ||
                (text == NULL))
            {
               (void)fprintf(stderr, "Failed to execute command: %s\n",
                             alda_cmd);
               (void)fprintf(stderr, "See SYSTEM_LOG for more information.\n");
               exit(INCORRECT);
            }
            eval_alda_data(text);
            free(text);
            text = NULL;

            /* Format information in a human readable text. */
            format_info(&text, YES);
         }
         else
         {
            /* Format information in a human readable text. */
            format_info(&text, NO);
         }

         /* Show the information. */
         show_info(text, YES);

         /* Free all memory that was allocated in get_info(). */
         free(text);

         for (i = 0; i < id.count; i++)
         {
            free((void *)id.dbe[i].files);
            if (id.dbe[i].soptions != NULL)
            {
               free((void *)id.dbe[i].soptions);
               id.dbe[i].soptions = NULL;
            }
         }
         if (id.count > 0)
         {
            free((void *)id.dbe);
            id.dbe = NULL;
            id.count = 0;
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
      XtSetSensitive(cont_togglebox_w, False);
      XtSetSensitive(start_time_w, False);
      XtSetSensitive(end_time_w, False);
      XtSetSensitive(file_name_w, False);
      XtSetSensitive(directory_w, False);
      XtSetSensitive(file_length_w, False);
      XtSetSensitive(recipient_w, False);
      XtSetSensitive(select_all_button_w, False);
      XtSetSensitive(radiobox_w, False);
      XtSetSensitive(print_button_w, False);

      scrollbar_moved_flag = NO;
      XmListDeleteAllItems(listbox_w);
      get_data();

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
select_all_button(Widget w, XtPointer client_data, XtPointer call_data)
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

   return;
}


/*########################### set_sensitive() ###########################*/
void
set_sensitive(void)
{
   XtSetSensitive(cont_togglebox_w, True);
   XtSetSensitive(start_time_w, True);
   XtSetSensitive(end_time_w, True);
   XtSetSensitive(file_name_w, True);
   XtSetSensitive(directory_w, True);
   XtSetSensitive(file_length_w, True);
   XtSetSensitive(recipient_w, True);
   XtSetSensitive(select_all_button_w, True);
   XtSetSensitive(radiobox_w, True);
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
   int         extra_sign = 0;
   XT_PTR_TYPE type = (XT_PTR_TYPE)client_data;
   char        *value = XmTextGetString(w);

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
      case RECIPIENT_NAME : /* Read the recipient. */
         {
            int  i = 0,
                 ii = 0;
            char *ptr,
                 *ptr_start;

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
                  while ((*ptr != '\0') &&
                         (*ptr != '@') &&
                         (*ptr != ','))
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
