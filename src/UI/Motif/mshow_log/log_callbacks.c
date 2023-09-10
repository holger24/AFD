/*
 *  log_callbacks.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   log_callbacks - all callback functions for module show_log
 **
 ** SYNOPSIS
 **   log_callbacks
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.03.1996 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>                 /* exit()                            */
#include <ctype.h>                  /* isxdigit()                        */
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>                 /* Definition of AT_* constants      */
#endif
#include <sys/stat.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <Xm/ScrollBar.h>
#include <errno.h>
#include "mafd_ctrl.h"
#include "mshow_log.h"
#include "logdefs.h"

extern Display        *display;
extern Widget         log_output,
                      counterbox,
                      selectlog,
                      selectscroll;
extern XmTextPosition wpr_position;
extern int            log_type_flag,
                      line_counter,
                      current_log_number;
extern XT_PTR_TYPE    toggles_set;
extern unsigned int   toggles_set_parallel_jobs;
extern off_t          total_length;
extern ino_t          current_inode_no;
extern char           fake_user[],
                      font_name[],
                      **hosts,
                      log_dir[],
                      log_type[],
                      log_name[],
                      profile[],
                      *p_work_dir;
extern FILE           *p_log_file;

static XmTextPosition last_pos = 0;


/*############################## toggled() ##############################*/
void
toggled(Widget w, XtPointer client_data, XtPointer call_data)
{
   toggles_set ^= (XT_PTR_TYPE)client_data;

   return;
}


/*########################### toggled_jobs() ############################*/
void
toggled_jobs(Widget w, XtPointer client_data, XtPointer call_data)
{
#if SIZEOF_LONG == 4
   toggles_set_parallel_jobs = (int)client_data;
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
      toggles_set_parallel_jobs = uil.ival[0];
   }
   else
   {
      toggles_set_parallel_jobs = uil.ival[1];
   }
#endif

   return;
}


/*############################ close_button() ###########################*/
void
close_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   if (hosts != NULL)
   {
      FREE_RT_ARRAY(hosts);
   }
   if (p_log_file != NULL)
   {
      (void)fclose(p_log_file);
   }

   exit(0);
}


/*########################## check_selection() ##########################*/
void
check_selection(Widget w, XtPointer client_data, XtPointer call_data)
{
   char *selected_str;

   if ((selected_str = XmTextGetSelection(w)) != NULL)
   {
      if (((selected_str[0] == '#') || (selected_str[0] == '@')) &&
          (selected_str[1] != '\0'))
      {
         int gotcha = YES,
             i = 1;

         while ((i < (MAX_INT_HEX_LENGTH + 1)) && (selected_str[i] != '\0'))
         {
            if (isxdigit((int)selected_str[i]) == 0)
            {
               if (!((selected_str[i] == ')') && (i > 2)))
               {
                  gotcha = NO;
                  break;
               }
            }
            i++;
         }
         if ((i < 2) || (i > MAX_INT_HEX_LENGTH))
         {
            gotcha = NO;
         }

         if (gotcha == YES)
         {
            int  offset;
            char *args[12],
                 progname[MAX_PROCNAME_LENGTH + 1];

            args[0] = progname;
            args[1] = "-f";
            args[2] = font_name;
            args[3] = WORK_DIR_ID;
            args[4] = p_work_dir;
            args[6] = &selected_str[1];
            if (fake_user[0] != '\0')
            {
               args[7] = "-u";
               args[8] = fake_user;
               offset = 9;
            }
            else
            {
               offset = 7;
            }
            if (profile[0] != '\0')
            {
               args[offset] = "-p";
               args[offset + 1] = profile;
               offset += 2;
            }
            args[offset] = NULL;
            (void)strcpy(progname, VIEW_DC);
            if (selected_str[0] == '#')
            {
               /* Job ID */
               args[5] = "-j";
            }
            else /* (selected_str[0] == '@') */
            {
               /* Directory ID */
               args[5] = "-D";
            }
            make_xprocess(progname, progname, args, -1);
         }
      }

      XtFree(selected_str);
   }

   return;
}


#ifdef _WITH_SEARCH_FUNCTION
/*############################ search_text() ############################*/
void
search_text(Widget w, XtPointer client_data, XtPointer call_data)
{
   char        *ptr,
               *search_str,
               *text_str;
   static char *last_search_str = NULL;

   if (last_pos != 0)
   {
      XmTextClearSelection(log_output, 0);
   }
   if (!(search_str = XmTextGetString(w)) || (!*search_str))
   {
      XtFree(search_str);
      return;
   }
   else
   {
      if (last_search_str == NULL)
      {
         size_t length;

         length = strlen(search_str) + 1;
         if ((last_search_str = malloc(length)) == NULL)
         {
            (void)xrec(FATAL_DIALOG,
                       "Could not malloc() %d Bytes : %s (%s %d)",
                       length, strerror(errno), __FILE__, __LINE__);
            return;
         }
         (void)memcpy(last_search_str, search_str, length);
      }
      else
      {
         if (my_strcmp(last_search_str, search_str) != 0)
         {
            size_t length;

            length = strlen(search_str) + 1;
            last_pos = 0;
            free(last_search_str);
            if ((last_search_str = malloc(length)) == NULL)
            {
               (void)xrec(FATAL_DIALOG,
                          "Could not malloc() %d Bytes : %s (%s %d)",
                          length, strerror(errno), __FILE__, __LINE__);
               return;
            }
            (void)memcpy(last_search_str, search_str, length);
         }
      }
   }
   if (!(text_str = XmTextGetString(log_output)) || (!*text_str))
   {
      XtFree(text_str);
      XtFree(search_str);
      return;
   }
   if ((ptr = posi(text_str + last_pos, search_str)) != NULL)
   {
      size_t         length;
      XmTextPosition pos;

      length = strlen(search_str);
      pos = (XmTextPosition)(ptr - text_str - length - 1);
      XmTextShowPosition(log_output, pos);
      XmTextSetSelection(log_output, pos, pos + length, 0);
      last_pos = pos + length;
   }
   else
   {
      if (last_pos != 0)
      {
         XmTextClearSelection(log_output, 0);
         last_pos = 0;
      }
   }
   XtFree(text_str);
   XtFree(search_str);

   return;
}
#endif


/*########################### update_button() ###########################*/
void
update_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   char str_line[MAX_LINE_COUNTER_DIGITS + 1];

   if (last_pos != 0)
   {
      XmTextClearSelection(log_output, 0);
      last_pos = 0;
   }
   if (p_log_file != NULL)
   {
      (void)fclose(p_log_file);
      p_log_file = NULL;
   }

#ifndef _WITH_SEARCH_FUNCTION
   /* Get slider position so we can determine the current log number. */
   XtVaGetValues(selectscroll, XmNvalue, &current_log_number, NULL);
#endif

   if (current_log_number != -1)
   {
      char log_file[MAX_PATH_LENGTH];

      /* Reread log file. */
      (void)sprintf(log_file, "%s/%s%d", log_dir, log_name, current_log_number);
      if ((p_log_file = fopen(log_file, "r")) == NULL)
      {
         char *ptr,
              error_line[13 + MAX_PATH_LENGTH + 19 + 1];

         if (errno != ENOENT)
         {
            (void)xrec(FATAL_DIALOG, "Could not fopen() %s : %s (%s %d)",
                       log_file, strerror(errno), __FILE__, __LINE__);
            return;
         }

         ptr = log_file + strlen(log_file);

         /* Cut off the path. */
         while ((*ptr != '/') && (ptr != log_file))
         {
            ptr--;
         }
         if (*ptr == '/')
         {
            *ptr = '\0';
            (void)sprintf(error_line,
                          "\n\n\n\n\t\tSorry, %s is not available!\n",
                          ptr + 1);
            *ptr = '/';
         }
         else
         {
            (void)sprintf(error_line,
                          "\n\n\n\n\t\tSorry, %s is not available!\n",
                          ptr);
         }

         XmTextSetInsertionPosition(log_output, 0);
         XmTextSetString(log_output, NULL);  /* Clears all old entries. */
         XmTextSetString(log_output, error_line);
         XFlush(display);

         return;
      }
      if ((log_type_flag != TRANSFER_LOG_TYPE) &&
          (log_type_flag != RECEIVE_LOG_TYPE) &&
          (current_log_number == 0))
      {
#ifdef HAVE_STATX
         struct statx stat_buf;
#else
         struct stat stat_buf;
#endif

#ifdef HAVE_STATX
         if (statx(fileno(p_log_file), "",
                   AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                   STATX_INO, &stat_buf) == -1)
#else
         if (fstat(fileno(p_log_file), &stat_buf) == -1)
#endif
         {
            (void)fprintf(stderr,
                          "ERROR   : Could not access %s : %s (%s %d)\n",
                          log_file, strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
#ifdef HAVE_STATX
         current_inode_no = stat_buf.stx_ino;
#else
         current_inode_no = stat_buf.st_ino;
#endif
      }
   }
   line_counter = 0;
   wpr_position = 0;
   total_length = 0;
   XmTextSetInsertionPosition(log_output, 0);
   XmTextSetString(log_output, NULL);  /* Clears all old entries. */
   init_text();

   (void)sprintf(str_line, "%*d", MAX_LINE_COUNTER_DIGITS, line_counter);
   XmTextSetString(counterbox, str_line);

   return;
}


#ifdef _WITH_SEARCH_FUNCTION
/*########################## toggled_log_no() ###########################*/
void
toggled_log_no(Widget w, XtPointer client_data, XtPointer call_data)
{
#if SIZEOF_LONG == 4
   current_log_number = (int)client_data;
#else
   union intlong
         {
            int  ival[2];
            long lval;
         } il;
   int  byte_order = 1;

   il.lval = (long)client_data;
   if (*(char *)&byte_order == 1)
   {
      current_log_number = il.ival[0];
   }
   else
   {
      current_log_number = il.ival[1];
   }
#endif

   return;
}


#else
# ifdef _WITH_SCROLLBAR
/*############################## slider_moved() #########################*/
void
slider_moved(Widget    w,
             XtPointer client_data,
             XtPointer call_data)
{
   char                      str_line[MAX_INT_LENGTH];
   XmString                  text;
   XmScrollBarCallbackStruct *cbs = (XmScrollBarCallbackStruct *)call_data;

   (void)sprintf(str_line, "%d", cbs->value);
   text = XmStringCreateLocalized(str_line);
   XtVaSetValues(selectlog, XmNlabelString, text, NULL);
   XmStringFree(text);

   return;
}
# endif
#endif
