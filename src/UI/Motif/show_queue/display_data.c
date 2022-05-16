/*
 *  display_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   display_data - displays the data found by get_data()
 **
 ** SYNOPSIS
 **   void display_data(void)
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
 **   28.07.2001 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>        /* sprintf()                                   */
#include <string.h>       /* strerror()                                  */
#include <time.h>         /* localtime()                                 */
#include <sys/time.h>     /* struct tm                                   */
#include <errno.h>

#include <Xm/Xm.h>
#include <Xm/List.h>
#include <Xm/Text.h>
#include "motif_common_defs.h"
#include "show_queue.h"

/* External global variables. */
extern Widget                  listbox_w,
                               scrollbar_w,
                               statusbox_w,
                               special_button_w,
                               summarybox_w;
extern int                     file_name_toggle_set,
                               radio_set,
                               no_of_search_hosts,
                               special_button_flag,
                               file_name_length;
extern unsigned int            total_no_files,
                               unprintable_chars;
extern XT_PTR_TYPE             toggles_set;
extern char                    summary_str[],
                               total_summary_str[];
extern struct queued_file_list *qfl;


/*############################ display_data() ###########################*/
void
display_data(void)
{
   unsigned int  i,
                 lines_displayed;
   register int  j;
   char          line[MAX_OUTPUT_LINE_LENGTH + SHOW_LONG_FORMAT + 1],
                 *p_file_name,
                 *p_hostname,
                 *p_type;
   struct tm     *p_ts;
   XmStringTable str_list;

   if ((str_list = (XmStringTable)XtMalloc((LINES_BUFFERED + 1) * sizeof(XmString))) == NULL)
   {
      (void)xrec(FATAL_DIALOG, "XtMalloc() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }

   /* Initialise all pointer in line. */
   p_file_name    = line + 20;
   p_type         = p_file_name + file_name_length + 1;
   p_hostname     = p_type + 5;

   lines_displayed = i = 0;
   do
   {
      (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);

      /* Insert date and time. */
      p_ts = localtime(&qfl[lines_displayed].mtime);
      CONVERT_TIME_YEAR();
      line[19] = ' ';

      /* Insert the file name. */
      j = 0;
      while ((j < file_name_length) && (qfl[lines_displayed].file_name[j] != '\0'))
      {
         if (qfl[lines_displayed].file_name[j] < ' ')
         {
            *(p_file_name + j) = '?';
            unprintable_chars++;
         }
         else
         {
            *(p_file_name + j) = qfl[lines_displayed].file_name[j];
         }
         j++;
      }
      while (j < file_name_length)
      {
         *(p_file_name + j) = ' ';
         j++;
      }

      /* Insert queue type. */
      if (qfl[lines_displayed].queue_type == SHOW_INPUT)
      {
         *p_type = 'I';
      }
      else if (qfl[lines_displayed].queue_type == SHOW_OUTPUT)
           {
              *p_type = 'O';
              if (qfl[lines_displayed].msg_name[0] != '\0')
              {
                 *(p_type + 1) = qfl[lines_displayed].priority;
              }
           }
      else if (qfl[lines_displayed].queue_type == SHOW_UNSENT_OUTPUT)
           {
              *p_type = 'O';
              *(p_type + 1) = 'U';
              if (qfl[lines_displayed].msg_name[0] != '\0')
              {
                 *(p_type + 2) = qfl[lines_displayed].priority;
              }
           }
      else if (qfl[lines_displayed].queue_type == SHOW_UNSENT_INPUT)
           {
              *p_type = 'I';
              *(p_type + 1) = 'U';
           }
      else if (qfl[lines_displayed].queue_type == SHOW_RETRIEVES)
           {
              *p_type = 'R';
           }
      else if (qfl[lines_displayed].queue_type == SHOW_PENDING_RETRIEVES)
           {
              *p_type = 'R';
              *(p_type + 1) = 'P';
           }
      else if (qfl[lines_displayed].queue_type == SHOW_TIME_JOBS)
           {
              *p_type = 'T';
           }
           else
           {
              *p_type = '?';
           }

      /* Insert hostname and file size. */
#if SIZEOF_OFF_T == 4
      (void)sprintf(p_hostname, "%-*s %*ld",
#else
      (void)sprintf(p_hostname, "%-*s %*lld",
#endif
                    MAX_HOSTNAME_LENGTH, qfl[lines_displayed].hostname,
                    MAX_DISPLAYED_FILE_SIZE,
                    (pri_off_t)qfl[lines_displayed].size);

      str_list[i] = XmStringCreateLocalized(line);
      if (i == LINES_BUFFERED)
      {
         XmListAddItemsUnselected(listbox_w, str_list, LINES_BUFFERED + 1, 0);
         for (i = 0; i < LINES_BUFFERED; i++)
         {
            XmStringFree(str_list[i]);
         }
         i = 0;
      }
      else
      {
         i++;
      }
      lines_displayed++;
   } while (lines_displayed < total_no_files);

   /* Free all memory. */
   lines_displayed = i % LINES_BUFFERED;
   if ((lines_displayed != 0) && (i != 0))
   {
      XmListAddItemsUnselected(listbox_w, str_list, lines_displayed, 0);
      for (j = 0; j < lines_displayed; j++)
      {
         XmStringFree(str_list[j]);
      }
   }
   XtFree((char *)str_list);

   return;
}


/*############################## show_summary() #########################*/
void
show_summary(unsigned int total_no_files, double file_size)
{
   char *ptr;

   (void)memset(summary_str, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);
   ptr = summary_str + 20 +
         sprintf(&summary_str[20], "%u Files", total_no_files);
   *ptr = ' ';
   ptr = summary_str + 20 + file_name_length + 1 + MAX_HOSTNAME_LENGTH + 1 + 4 + 2;

   if (file_size < F_KILOBYTE)
   {
      ptr += sprintf(ptr, "%4.0f Bytes ", file_size);
   }
   else if (file_size < F_MEGABYTE)
        {
           ptr += sprintf(ptr, "%7.2f KB ", file_size / F_KILOBYTE);
        }
   else if (file_size < F_GIGABYTE)
        {
           ptr += sprintf(ptr, "%7.2f MB ", file_size / F_MEGABYTE);
        }
   else if (file_size < F_TERABYTE)
        {
           ptr += sprintf(ptr, "%7.2f GB ", file_size / F_GIGABYTE);
        }
   else if (file_size < F_PETABYTE)
        {
           ptr += sprintf(ptr, "%7.2f TB ", file_size / F_TERABYTE);
        }
   else if (file_size < F_EXABYTE)
        {
           ptr += sprintf(ptr, "%7.2f PB ", file_size / F_PETABYTE);
        }
        else
        {
           ptr += sprintf(ptr, "%7.2f EB ", file_size / F_EXABYTE);
        }
   *ptr = ' ';
   XmTextSetString(summarybox_w, summary_str);

   return;
}
