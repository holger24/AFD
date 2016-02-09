/*
 *  view_data_no_filter.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2013 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   view_data_no_filter - calls a configured program to view data
 **
 ** SYNOPSIS
 **   void view_data_no_filter(char *fullname, char *file_name, int view_mode)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   29.10.2013 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>            /* strcpy(), strlen()                     */
#include "ui_common_defs.h"

/* External global variables. */
extern struct view_modes *vm;


/*######################## view_data_no_filter() ########################*/
void
view_data_no_filter(char *fullname, char *file_name, int view_mode)
{
   if (vm[view_mode].p_cmd != NULL)
   {
      int  extra_length = 0,
           length;
      char *p_cmd_orig,
           *ptr;

      length = strlen(fullname);
      p_cmd_orig = vm[view_mode].p_cmd;
      ptr = vm[view_mode].cmd;
      while (*ptr != '\0')
      {
         if ((*ptr == '%') && (*(ptr + 1) == 's') &&
             ((extra_length + length) < MAX_PATH_LENGTH))
         {
            (void)strcpy(vm[view_mode].p_cmd, fullname);
            ptr += 2;
            vm[view_mode].p_cmd += length;
            extra_length += length;
         }
         else
         {
            *vm[view_mode].p_cmd = *ptr;
            vm[view_mode].p_cmd++; ptr++;
         }
      }

      /* If no %s is found, add it to the end. */
      if (extra_length == 0)
      {
         *vm[view_mode].p_cmd = ' ';
         vm[view_mode].p_cmd++;
         (void)strcpy(vm[view_mode].p_cmd, fullname);
         vm[view_mode].p_cmd += length;
      }

      if (vm[view_mode].with_show_cmd == YES)
      {
         *vm[view_mode].p_cmd = ' ';
         vm[view_mode].p_cmd++;
         (void)strcpy(vm[view_mode].p_cmd, file_name);
         vm[view_mode].p_cmd += strlen(file_name);
      }
      *vm[view_mode].p_cmd = '"';
      vm[view_mode].p_cmd++;
      *vm[view_mode].p_cmd = '\0';
      vm[view_mode].p_cmd = p_cmd_orig;
   }
   else
   {
      int i;

      for (i = 0; i < vm[view_mode].argcounter; i++)
      {
         if ((vm[view_mode].args[i] == NULL) ||
             ((vm[view_mode].args[i][0] == '%') &&
              (vm[view_mode].args[i][1] == 's')))
         {
            vm[view_mode].args[i] = fullname;
         }
      }
   }
   make_xprocess(vm[view_mode].progname, vm[view_mode].progname,
                 vm[view_mode].args, -1);
   if (vm[view_mode].p_cmd != NULL)
   {
      *vm[view_mode].p_cmd = '\0';
   }

   return;
}
