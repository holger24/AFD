/*
 *  get_rule.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2009 Deutscher Wetterdienst (DWD),
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
 **   get_rule - locates a given rule and returns its position
 **
 ** SYNOPSIS
 **   int get_rule(int fd, off_t offset)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   08.10.1997 H.Kiehl Created
 **   10.12.2002 H.Kiehl Catch the case that someone has empty characters
 **                      behind the rule identifier. This is required for
 **                      the new overwrite option.
 **   17.05.2007 H.Kiehl 128 byte should be enough and no need to malloc().
 **
 */
DESCR__E_M3

#include <string.h>                     /* strcmp()                      */

/* External global variables. */
extern struct rule *rule;


/*############################ get_rule() ###############################*/
int
get_rule(char *wanted_rule, int no_of_rule_headers)
{
   int  i;
   char rule_buffer[MAX_RULE_HEADER_LENGTH + 1];

   i = 0;
   while ((i < MAX_RULE_HEADER_LENGTH) && (wanted_rule[i] != '\0') &&
          (wanted_rule[i] != ' ') && (wanted_rule[i] != '\t'))
   {
      rule_buffer[i] = wanted_rule[i];
      i++;
   }
   if (i == MAX_RULE_HEADER_LENGTH)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 _("Rule identifier is to long, limit is %d."),
                 MAX_RULE_HEADER_LENGTH);
   }
   else
   {
      rule_buffer[i] = '\0';
      for (i = 0; i < no_of_rule_headers; i++)
      {
         if (CHECK_STRCMP(rule[i].header, rule_buffer) == 0)
         {
            return(i);
         }
      }
   }

   return(INCORRECT);
}
