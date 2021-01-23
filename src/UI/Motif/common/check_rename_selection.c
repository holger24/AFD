/*
 *  check_rename_selection.c - Part of AFD, an automatic file distribution
 *                             program.
 *  Copyright (c) 2021 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_rename_selection - checks if the selection is a rename rule
 **                            alias
 **
 ** SYNOPSIS
 **   void check_rename_selection(Widget    w,
 **                               XtPointer client_data,
 **                               XtPointer call_data)
 **
 ** DESCRIPTION
 **   This function checks if the selected text is part of the rename
 **   rule. If that is the case it calls show_cmd with the command
 **   'get_rr_data <selection>'. In client_data must be the complete
 **   context of of the text shown in widget w.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.01.2021 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>            /* memcpy()                               */
#include <stdlib.h>            /* malloc(), free()                       */
#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <errno.h>
#include "motif_common_defs.h"

/* External global variables. */
extern char font_name[],
            *p_work_dir;


/*####################### check_rename_selection() ######################*/
void
check_rename_selection(Widget w, XtPointer client_data, XtPointer call_data)
{
   XmTextPosition left,
                  right;

   if (XmTextGetSelectionPosition(w, &left, &right) == True)
   {
      char *view_buffer;

      if ((char *)client_data == NULL)
      {
         view_buffer = XmTextGetString(w);
      }
      else
      {
         view_buffer = (char *)client_data;
      }

      /* Check if this is a rename rule. */
      if ((left > 6) && (view_buffer[left - 1] == ' ') &&
          (view_buffer[left - 2] == 'e') &&
          (view_buffer[left - 3] == 'm') &&
          (view_buffer[left - 4] == 'a') &&
          (view_buffer[left - 5] == 'n') &&
          (view_buffer[left - 6] == 'e') &&
          (view_buffer[left - 7] == 'r'))
      {
         /* If longer then 'rename' ensure it is not one of the */
         /* srename option.                                     */
         if ((left == 7) ||
             ((left > 7) &&
              ((view_buffer[left - 8] == ' ') ||
               (view_buffer[left - 8] == '_'))))
         {
            char *args[8],
                 cmd[GET_RR_DATA_LENGTH + 1 + MAX_RULE_HEADER_LENGTH + 1],
                 *selected_str = NULL;

            if ((selected_str = malloc((right - left + 1))) == NULL)
            {
               (void)xrec(FATAL_DIALOG,
                          "Could not malloc() %d Bytes : %s (%s %d)",
                          right - left + 1, strerror(errno),
                          __FILE__, __LINE__);
               return;
            }
            (void)memcpy(selected_str, &view_buffer[left], right - left);
            selected_str[right - left] = '\0';
            args[0] = SHOW_CMD;
            args[1] = "-f";
            args[2] = font_name;
            args[3] = WORK_DIR_ID;
            args[4] = p_work_dir;
            args[5] = "-nrb";
            args[6] = cmd;
            args[7] = NULL;
            (void)snprintf(cmd,
                           GET_RR_DATA_LENGTH + 1 + MAX_RULE_HEADER_LENGTH + 1,
                           "\"%s %s %s\"",
                           GET_RR_DATA, selected_str, selected_str);
            make_xprocess(SHOW_CMD, SHOW_CMD, args, -1);
            free(selected_str);
         }
      }
      if ((char *)client_data == NULL)
      {
         XtFree(view_buffer);
      }
   }

   return;
}
