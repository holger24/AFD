/*
 *  handle_recipient_group_name.c - Part of AFD, an automatic file
 *                                  distribution program.
 *  Copyright (c) 2017 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   init_recipient_group_name - initialize recipient group names
 **   next_recipient_group_name - get next recipient entry
 **   free_recipient_group_name - free all resources
 **
 ** SYNOPSIS
 **   void init_recipient_group_name(char *recipient, char *group_name,
 **                                  int dir_group_type)
 **   int  next_recipient_group_name(char *recipient)
 **   void free_recipient_group_name(void)
 **
 ** DESCRIPTION
 **   This set of functions handles group names for recipients
 **   in DIR_CONFIG file. The group name is resolved via the file
 **   $AFD_WORK_DIR/etc/group.list.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.09.2017 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* fprintf(), sprintf()               */
#include <string.h>                /* strcmp(), strcpy(), strerror()     */
#include <stdlib.h>                /* exit()                             */
#include <sys/types.h>
#include <sys/stat.h>              /* stat(), S_ISREG()                  */
#include <dirent.h>                /* opendir(), closedir(), readdir(),  */
                                   /* DIR, struct dirent                 */
#include <errno.h>
#include "amgdefs.h"


/* External global variables. */
extern char *p_work_dir;

/* Local global variables. */
static int  next_group_pos,
            no_listed,
            start_group_pos;
static char **group_list = NULL,
            last_part[MAX_PATH_LENGTH],
            orig_recipient[MAX_PATH_LENGTH];


/*#################### init_recipient_group_name() ######################*/
void
init_recipient_group_name(char *location,
                          char *group_name,
                          int  dir_group_type)
{
   off_t file_size;
   char  *buffer = NULL,
         group_file[MAX_PATH_LENGTH];

   /* First, lets store all group elements in an array. */
   no_listed = 0;
   if (dir_group_type == YES)
   {
      (void)snprintf(group_file, MAX_PATH_LENGTH, "%s%s%s%s/%s",
                     p_work_dir, ETC_DIR, GROUP_NAME_DIR,
                     RECIPIENT_GROUP_NAME, group_name);
   }
   else
   {
      (void)snprintf(group_file, MAX_PATH_LENGTH, "%s%s%s",
                     p_work_dir, ETC_DIR, GROUP_FILE);
   }
   if (((file_size = read_file_no_cr(group_file, &buffer, YES,
                                     __FILE__, __LINE__)) != INCORRECT) &&
       (file_size > 0))
   {
      int  length;
      char *ptr;

      if (dir_group_type == YES)
      {
         ptr = buffer;
      }
      else
      {
         char group_id[MAX_PATH_LENGTH + 2 + 1];

         length = snprintf(group_id, MAX_PATH_LENGTH + 2 + 1,
                           "[%s]", group_name);
         ptr = lposi(buffer, group_id, length);
      }

      if (ptr == NULL)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to locate group [%s] in group file %s",
                    group_name, group_file);
         group_list = NULL;
         free(buffer);

         return;
      }
      else
      {
         ptr--;
         while ((*ptr != '\n') && (*ptr != '\0'))
         {
            ptr++;
         }
         if (*ptr == '\n')
         {
            int  max_length = 0;
            char *ptr_start = ptr; /* NOTE: NOT + 1 */

            /*
             * First count the number of groups.
             */
            length = 0;
            no_listed = 0;
            do
            {
               ptr++;
               if (*ptr == '#')
               {
                  while ((*ptr != '\n') && (*ptr != '\0'))
                  {
                     ptr++;
                  }
                  if (length > 0)
                  {
                     if (length > max_length)
                     {
                        max_length = length;
                     }
                     length = 0;
                     no_listed++;
                  }
               }
               else if ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       /* Ignore spaces! */;
                    }
                    else
                    {
                       length++;
                       if ((*ptr == '\n') || (*ptr == '\0'))
                       {
                          if (length > max_length)
                          {
                             max_length = length;
                          }
                          length = 0;
                          no_listed++;
                       }
                    }

               if ((*ptr == '\n') &&
                   ((*(ptr + 1) == '\n') || (*(ptr + 1) == '\0')))
               {
                  break;
               }
            } while ((*ptr != '[') && (*ptr != '\0'));

            if ((no_listed > 0) && (max_length > 0))
            {
               int counter = 0;

               RT_ARRAY(group_list, no_listed, max_length, char);
               ptr = ptr_start;
               length = 0;
               do
               {
                  ptr++;
                  if (*ptr == '#')
                  {
                     while ((*ptr != '\n') && (*ptr != '\0'))
                     {
                        ptr++;
                     }
                     if (length > 0)
                     {
                        group_list[counter][length] = '\0';
                        length = 0;
                        counter++;
                     }
                  }
                  else if ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          /* Ignore spaces! */;
                       }
                       else
                       {
                          if ((*ptr == '\n') || (*ptr == '\0'))
                          {
                             group_list[counter][length] = '\0';
                             length = 0;
                             counter++;
                          }
                          else
                          {
                             group_list[counter][length] = *ptr;
                             length++;
                          }
                       }
                  if ((*ptr == '\n') &&
                      ((*(ptr + 1) == '\n') || (*(ptr + 1) == '\0')))
                  {
                     break;
                  }
               } while ((*ptr != '[') && (*ptr != '\0'));
            }
            else
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "No group elements found for group %s.", group_name);
               group_list = NULL;
            }
         }
         else
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "No group elements found for group %s.", group_name);
            group_list = NULL;
         }
      }

      free(buffer);

      /* Now lets prepare the first element. */
      ptr = location;
      length = 0;
continue_search:
      while ((*ptr != GROUP_SIGN) && (*ptr != '\0'))
      {
         orig_recipient[length] = *ptr;
         ptr++; length++;
      }
      orig_recipient[length] = '\0';
      start_group_pos = length;
      if (*ptr == '\0')
      {
         /* Oops, no group sign? */
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "No group sign in original string %s", location);
         no_listed = 0;
         if (group_list != NULL)
         {
            FREE_RT_ARRAY(group_list);
            group_list = NULL;
         }
      }
      else
      {
         if ((*(ptr + 1) == CURLY_BRACKET_OPEN) ||
             (*(ptr + 1) == SQUARE_BRACKET_OPEN))
         {
            char closing_bracket;

            if (*(ptr + 1) == SQUARE_BRACKET_OPEN)
            {
               closing_bracket = SQUARE_BRACKET_CLOSE;
            }
            else
            {
               closing_bracket = CURLY_BRACKET_CLOSE;
            }
            ptr += 2;
            while ((*ptr != closing_bracket) && (*ptr != '\0'))
            {
               ptr++;
            }
            if (*ptr == '\0')
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "No closing bracket '%c' in string %s",
                          closing_bracket, location);
               no_listed = 0;
               FREE_RT_ARRAY(group_list);
               group_list = NULL;
            }
            else
            {
               ptr++;
               if (*ptr == '\0')
               {
                  last_part[0] = '\0';
               }
               else
               {
                  (void)my_strncpy(last_part, ptr, MAX_PATH_LENGTH);
               }
            }
         }
         else
         {
            ptr++;
            goto continue_search;
         }
         (void)strcpy(location + start_group_pos, group_list[0]);
         (void)strcat(location + start_group_pos, last_part);
         next_group_pos = 1;
      }
   }

   return;
}


/*#################### next_recipient_group_name() ######################*/
int
next_recipient_group_name(char *recipient)
{
   if ((no_listed > 0) && (next_group_pos < no_listed))
   {
      (void)strcpy(recipient, orig_recipient);
      (void)strcpy(recipient + start_group_pos, group_list[next_group_pos]);
      (void)strcat(recipient + start_group_pos, last_part);
      next_group_pos++;

      return(1);
   }

   return(0);
}


/*#################### free_recipient_group_name() ######################*/
void
free_recipient_group_name(void)
{
   no_listed = 0;
   if (group_list != NULL)
   {
      FREE_RT_ARRAY(group_list);
      group_list = NULL;
   }

   return;
}
