/*
 *  xrec.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 - 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   xrec - pops up a message dialog
 **
 ** SYNOPSIS
 **   int xrec(char type, char *fmt, ...)
 **
 ** DESCRIPTION
 **   This function pops up a message dialog with 'fmt' as contents.
 **   The following types of message dialogs have been implemented:
 **
 **    +----------------+-----------------------+-------+---------+--------+
 **    |                |                       | Block |         |        |
 **    |     Type       |      Description      | Input | Buttons | Action |
 **    +----------------+-----------------------+-------+---------+--------+
 **    |INFO_DIALOG     | Information.          |  Yes  | OK      | None   |
 **    |WARN_DIALOG     | Warning.              |  Yes  | OK      | None   |
 **    |ERROR_DIALOG    | Error.                |  Yes  | OK      | None   |
 **    |FATAL_DIALOG    | Fatal error.          |  Yes  | OK      | exit() |
 **    |ABORT_DIALOG    | Fatal error.          |  Yes  | OK      | abort()|
 **    |QUESTION_DIALOG | Question.             |  Yes  | YES, NO | None   |
 **    +----------------+-----------------------+-------+---------+--------+
 **
 ** RETURN VALUES
 **   When using QUESTION_DIALOG either YES or no will be returned. All
 **   other dialogs return NEITHER.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   26.12.2008 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <stdlib.h>            /* abort(), exit()                        */
#include <stdarg.h>
#include <gtk/gtk.h>
#include "gtk_common_defs.h"

/* External global variables. */
extern GtkWidget *appshell;

/*############################### xrec() ################################*/
int
xrec(char type, char *fmt, ...)
{
   int       answer;
   char      buf[MAX_LINE_LENGTH + 1];
   va_list   ap;
   GtkWidget *dialog;
   GtkMessageType msg_type;
   GtkButtonsType button_type;

   va_start(ap, fmt);
   (void)vsnprintf(buf, MAX_LINE_LENGTH, fmt, ap);

   switch (type)
   {
      case INFO_DIALOG    : /* Information and OK button. */
         msg_type = GTK_MESSAGE_INFO;
         button_type = GTK_BUTTONS_OK;
         break;

      case WARN_DIALOG    : /* Warning message and OK button. */
         msg_type = GTK_MESSAGE_WARNING;
         button_type = GTK_BUTTONS_OK;
         break;

      case ERROR_DIALOG   : /* Error message and OK button. */
         msg_type = GTK_MESSAGE_ERROR;
         button_type = GTK_BUTTONS_OK;
         break;

      case FATAL_DIALOG   :
      case ABORT_DIALOG   : /* Fatal error message and OK button. */
         msg_type = GTK_MESSAGE_ERROR;
         button_type = GTK_BUTTONS_OK;
         break;

      case QUESTION_DIALOG: /* Message and YES+NO button. */
         msg_type = GTK_MESSAGE_QUESTION;
         button_type = GTK_BUTTONS_YES_NO;
         break;

      default             : /* Undefined. Should not happen! */
         break;
   }
   dialog = gtk_message_dialog_new(GTK_WINDOW(appshell),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   msg_type, button_type, buf);
   answer = gtk_dialog_run(GTK_DIALOG(dialog));
   gtk_widget_destroy(dialog);
   va_end(ap);

   switch (type)
   {
      case ABORT_DIALOG:
         abort();

      case FATAL_DIALOG:
         exit(INCORRECT);

      default :
         break;
   }

   switch (answer)
   {
      case GTK_RESPONSE_YES :
         answer = YES;
         break;

      case GTK_RESPONSE_NO :
         answer = NO;
         break;

      default :
         answer = NEITHER;
         break;
   }

   return(answer);
}
