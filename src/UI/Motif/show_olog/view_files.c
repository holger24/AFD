/*
 *  view_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 - 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   view_files - views files from the AFD archive
 **
 ** SYNOPSIS
 **   void view_files(int no_selected, int *select_list)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   04.04.2007 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>         /* strerror(), strcpy()                      */
#include <stdlib.h>         /* calloc(), free()                          */
#include <errno.h>
#include <Xm/Xm.h>
#include <Xm/List.h>
#include "mafd_ctrl.h"
#include "show_olog.h"


/* External global variables. */
extern Display          *display;
extern Widget           appshell, /* CHECK_INTERRUPT() */
                        listbox_w,
                        scrollbar_w,
                        special_button_w,
                        statusbox_w;
extern int              log_date_length,
                        max_hostname_length,
                        no_of_log_files,
                        special_button_flag,
                        view_mode;
extern char             *p_work_dir;
extern struct item_list *il;
extern struct info_data id;
extern struct sol_perm  perm;

/* Local global variables. */
static char             archive_dir[MAX_PATH_LENGTH],
                        *p_file_name,
                        *p_archive_name,
                        dest_dir[MAX_PATH_LENGTH];

/* Local function prototypes. */
static int              get_archive_data(int, int);


/*############################## view_files() ###########################*/
void
view_files(int no_selected, int *select_list)
{
   int                i,
                      total_no_of_items,
                      length = 0,
                      to_do = 0,    /* Number still to be done. */
                      no_done = 0,  /* Number done.             */
                      not_found = 0,
                      not_archived = 0,
                      not_in_archive = 0,
                      select_done = 0,
                      *select_done_list;
   char               user_message[256];
   XmString           xstr;
   struct resend_list *vl;

   dest_dir[0] = '\0';
   if (((vl = calloc(no_selected, sizeof(struct resend_list))) == NULL) ||
       ((select_done_list = calloc(no_selected, sizeof(int))) == NULL))
   {
      (void)xrec(FATAL_DIALOG, "calloc() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }

   /* Prepare the archive directory name. */
   p_archive_name = archive_dir;
   p_archive_name += sprintf(archive_dir, "%s%s/",
                             p_work_dir, AFD_ARCHIVE_DIR);

   /* Block all input and change button name. */
   special_button_flag = STOP_BUTTON;
   xstr = XmStringCreateLtoR("Stop", XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);
   CHECK_INTERRUPT();

   /*
    * Initialize selected set.
    */
   for (i = 0; i < no_selected; i++)
   {
      /* Determine log file and position in this log file. */
      total_no_of_items = 0;
      for (vl[i].file_no = 0; vl[i].file_no < no_of_log_files; vl[i].file_no++)
      {
         total_no_of_items += il[vl[i].file_no].no_of_items;

         if (select_list[i] <= total_no_of_items)
         {
            vl[i].pos = select_list[i] - (total_no_of_items - il[vl[i].file_no].no_of_items) - 1;
            if (vl[i].pos > il[vl[i].file_no].no_of_items)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "pos (%d) is greater then no_of_items (%d), ignoring this.",
                          vl[i].pos, il[vl[i].file_no].no_of_items);
               vl[i].pos = -1;
            }
            break;
         }
      }

      /* Check if the file is in fact archived. */
      if (vl[i].pos > -1)
      {
         if (il[vl[i].file_no].archived[vl[i].pos] == 1)
         {
            vl[i].status = FILE_PENDING;
            to_do++;
         }
         else
         {
            vl[i].status = NOT_ARCHIVED;
            not_archived++;
         }
      }
      else
      {
         vl[i].status = NOT_FOUND;
         not_found++;
      }
   }

   /*
    * Start only MAX_VIEW_DATA_WINDOWS programs at one time.
    */
   if (to_do > 0)
   {
      for (i = 0; i < no_selected; i++)
      {
         if (vl[i].status == FILE_PENDING)
         {
            if (get_archive_data(vl[i].pos, vl[i].file_no) < 0)
            {
               vl[i].status = NOT_IN_ARCHIVE;
               not_in_archive++;
            }
            else
            {
               if (view_mode == 0) /* Auto */
               {
                  view_data(archive_dir, p_file_name);
               }
               else
               {
                  view_data_no_filter(archive_dir, p_file_name,
                                      view_mode - 1);
               }
               vl[i].status = DONE;
               no_done++;
               select_done_list[select_done] = select_list[i];
               select_done++;
               if (select_done >= MAX_VIEW_DATA_WINDOWS)
               {
                  i = no_selected;
               }
            }
         }

         CHECK_INTERRUPT();
         if (special_button_flag == STOP_BUTTON_PRESSED)
         {
            break;
         }
      }
   }

   if (select_done > 0)
   {
      for (i = 0; i < select_done; i++)
      {
         XmListDeselectPos(listbox_w, select_done_list[i]);
      }
   }

   /* Show user a summary of what was done. */
   if (no_done > 0)
   {
      if (no_done == 1)
      {
         length = sprintf(user_message, "1 file shown");
      }
      else
      {
         length = sprintf(user_message, "%d files shown", no_done);
      }
   }
   if (not_archived > 0)
   {
      if (length > 0)
      {
         length += sprintf(&user_message[length], ", %d not archived",
                           not_archived);
      }
      else
      {
         length = sprintf(user_message, "%d not archived", not_archived);
      }
   }
   if (not_in_archive > 0)
   {
      if (length > 0)
      {
         length += sprintf(&user_message[length], ", %d not in archive",
                           not_in_archive);
      }
      else
      {
         length = sprintf(user_message, "%d not in archive", not_in_archive);
      }
   }
   if (not_found > 0)
   {
      if (length > 0)
      {
         length += sprintf(&user_message[length], ", %d not found", not_found);
      }
      else
      {
         length = sprintf(user_message, "%d not found", not_found);
      }
   }
   show_message(statusbox_w, user_message);

   free((void *)vl);
   free((void *)select_done_list);

   /* Button back to normal. */
   special_button_flag = SEARCH_BUTTON;
   xstr = XmStringCreateLtoR("Search", XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);

   return;
}


/*+++++++++++++++++++++++++++ get_archive_data() ++++++++++++++++++++++++*/
/*                            ------------------                         */
/* Description: From the output log file, this function gets the file    */
/*              name and the name of the archive directory.              */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static int
get_archive_data(int pos, int file_no)
{
   int  i, j,
        type_offset;
   char buffer[MAX_FILENAME_LENGTH + MAX_PATH_LENGTH],
        *ptr,
        *p_unique_string;

   if (fseek(il[file_no].fp, (long)il[file_no].line_offset[pos], SEEK_SET) == -1)
   {
      (void)xrec(FATAL_DIALOG, "fseek() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }
   if (fgets(buffer, MAX_FILENAME_LENGTH + MAX_PATH_LENGTH, il[file_no].fp) == NULL)
   {
      (void)xrec(FATAL_DIALOG, "fgets() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   if (buffer[log_date_length + 1 + max_hostname_length + 2] == ' ')
   {
#ifdef ACTIVATE_THIS_AFTER_VERSION_14
      type_offset = 5;
#else
      if (buffer[log_date_length + 1 + max_hostname_length + 4] == ' ')
      {
         type_offset = 5;
      }
      else
      {
         type_offset = 3;
      }
#endif
   }
   else
   {
      type_offset = 1;
   }
   ptr = &buffer[log_date_length + 1 + max_hostname_length + type_offset + 2];

   /* Mark end of file name. */
   while (*ptr != SEPARATOR_CHAR)
   {
      ptr++;
   }
   *(ptr++) = '\0';
   if (*ptr != SEPARATOR_CHAR)
   {
      /* Ignore the remote file name. */
      while (*ptr != SEPARATOR_CHAR)
      {
         ptr++;
      }
   }
   ptr++;

   /* Away with the size. */
   while (*ptr != SEPARATOR_CHAR)
   {
      ptr++;
   }
   ptr++;

   /* Away with transfer duration. */
   while (*ptr != SEPARATOR_CHAR)
   {
      ptr++;
   }
   ptr++;

   /* Away with number of retries. */
   if (type_offset > 1)
   {
      while (*ptr != SEPARATOR_CHAR)
      {
         ptr++;
      }
      ptr++;
   }

   /* Away with the job ID. */
   while (*ptr != SEPARATOR_CHAR)
   {
      ptr++;
   }
   ptr++;

   /* Away with the unique string. */
   p_unique_string = ptr;
   while (*ptr != SEPARATOR_CHAR)
   {
      ptr++;
   }
   ptr++;

   /* Ahhh. Now here is the archive directory we are looking for. */
   i = 0;
   while (*ptr != '\n')
   {
      if (*ptr == '$')
      {
         *(p_archive_name + i) = '\\';
         i++;
      }
      *(p_archive_name + i) = *ptr;
      i++; ptr++;
   }
   *(p_archive_name + i++) = '/';
   while ((*p_unique_string != SEPARATOR_CHAR) && (*p_unique_string != ' ') &&
          (*p_unique_string != '\n'))
   {
      *(p_archive_name + i) = *p_unique_string;
      i++; p_unique_string++;
   }
   *(p_archive_name + i++) = '_';

   /* Copy the file name to the archive directory. */
   j = 0;
   ptr = &buffer[log_date_length + 1 + max_hostname_length + type_offset + 2];
   while (*ptr != '\0')
   {
      if (*ptr == ' ')
      {
         *(p_archive_name + i + j) = '\\';
         *(p_archive_name + i + j + 1) = ' ';
         j += 2;
      }
      else
      {
         *(p_archive_name + i + j) = *ptr;
         j++;
      }
      ptr++;
   }
   *(p_archive_name + i + j) = '\0';
   p_file_name = p_archive_name + i;

   return(SUCCESS);
}
