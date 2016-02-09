/*
 *  update_time.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2007 Deutscher Wetterdienst (DWD),
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
 **   update_time - updates the clock every minute
 **
 ** SYNOPSIS
 **   void update_time(XtPointer clientdata, XtIntervalId id)
 **
 ** DESCRIPTION
 **   Writes the current time every minute to the widget 'clientdata'.
 **   The time format is as follows: DD.MM.YYYY hh:mm
 **         DD   - Day of the month       [1 - 31]
 **         MM   - Month of the year      [1 - 12]
 **         YYYY - Year
 **         hh   - Hours after midnight   [0 - 23]
 **         mm   - Minutes after the hour [0 - 59]
 **
 **   This function uses XtAppAddTimeOut() to call itself every
 **   minute. The next minute is calculated as follows:
 **              (60 - current_time % 60) * 1000
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.04.1997 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <time.h>             /* strftime(), localtime(), time()         */
#include <sys/types.h>

#include <Xm/Xm.h>
#include <Xm/Label.h>
#include "motif_common_defs.h"

#define MAX_TMP_STRING_LENGTH 18


/*############################ update_time() ############################*/
void
update_time(XtPointer clientdata, XtIntervalId id)
{
   Widget   w = (Widget)clientdata;
   time_t   current_time,
            next_minute;
   char     tmp_str_line[MAX_TMP_STRING_LENGTH];
   XmString xstr;

   current_time = time(NULL);
   (void)strftime(tmp_str_line, MAX_TMP_STRING_LENGTH, "%d.%m.%Y  %H:%M",
                  localtime(&current_time));
   xstr = XmStringCreateLtoR(tmp_str_line, XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);

   next_minute = (60 - current_time % 60) * 1000;

   (void)XtAppAddTimeOut(XtWidgetToApplicationContext(w), next_minute,
                         (XtTimerCallbackProc)update_time, (XtPointer)w);

   return;
}
