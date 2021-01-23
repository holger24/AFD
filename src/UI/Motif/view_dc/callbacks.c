/*
 *  callbacks.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2021 Deutscher Wetterdienst (DWD),
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
 **   callbacks - all callback functions for module view_dc
 **
 ** SYNOPSIS
 **   void close_button(Widget w, XtPointer client_data, XtPointer call_data)
 **   void search_button(Widget w, XtPointer client_data, XtPointer call_data)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   23.02.1999 H.Kiehl Created
 **   26.09.2017 H.Kiehl Fix memory leak in search_button().
 **
 */
DESCR__E_M3

#include <stdlib.h>
#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <errno.h>
#include "view_dc.h"

/* External global variables. */
extern char   font_name[],
              *p_work_dir;
extern Widget searchbox_w,
              text_w;


/*########################### close_button() ############################*/
void
close_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   exit(0);
}


/*########################## search_button() ############################*/
void
search_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   char                  *ptr,
                         *search_str,
                         *text_str;
   static char           *last_search_str = NULL;
   static XmTextPosition last_pos = 0;

   if (last_pos != 0)
   {
      XmTextClearSelection(text_w, 0);
   }
   if (!(search_str = XmTextGetString(searchbox_w)) || (!*search_str))
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
   if (!(text_str = XmTextGetString(text_w)) || (!*text_str))
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
      XmTextShowPosition(text_w, pos);
      XmTextSetSelection(text_w, pos, pos + length, 0);
      last_pos = pos + length;
   }
   else
   {
      if (last_pos != 0)
      {
         XmTextClearSelection(text_w, 0);
         last_pos = 0;
      }
   }
   XtFree(text_str);
   XtFree(search_str);

   return;
}
