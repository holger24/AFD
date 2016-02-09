/*
 *  get_user.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2011 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_user - gets user name and DISPLAY
 **
 ** SYNOPSIS
 **   void get_user(char *user, char *fake_user, int user_offset)
 **
 ** DESCRIPTION
 **   Gets the username and display of the current process. This
 **   is done by getting the user ID (UID) with getuid() and
 **   getpwuid() and the DISPLAY environment variable.
 **   If not known 'unknown' will be used for either user or 'DISPLAY'.
 **   When 'DISPLAY' is set to 'localhost' it will try to get the
 **   remote host by looking at the output of 'who am i'.
 **
 ** RETURN VALUES
 **   Will return the username and display in 'user'.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.04.1996 H.Kiehl Created
 **   18.08.2003 H.Kiehl Build in length checks.
 **   20.10.2003 H.Kiehl If DISPLAY is localhost (ssh) get real hostname
 **                      with 'who am i'.
 **   29.10.2003 H.Kiehl Try to evaluate SSH_CLIENT as well when
 **                      DISPLAY is localhost.
 **   02.05.2004 H.Kiehl Take user from command line option, if allowed.
 **   26.06.2007 H.Kiehl Added parameter user_offset to indicate we
 **                      have already added the profile name in the
 **                      user string.
 **
 */
DESCR__E_M3

#include <stdio.h>              /* NULL                                  */
#include <string.h>             /* strcat(), strlen(), memcpy()          */
#include <stdlib.h>             /* getenv(), malloc(), free()            */
#include <sys/types.h>
#include <pwd.h>                /* getpwuid()                            */
#include <unistd.h>             /* getuid()                              */


/*############################ get_user() ###############################*/
void
get_user(char *user, char *fake_user, int user_offset)
{
   size_t        length;
   char          *ptr;
   struct passwd *pwd;

   if (user_offset > 0)
   {
      user[user_offset] = ' ';
      user[user_offset + 1] = '<';
      user_offset += 2;
   }
   if ((pwd = getpwuid(getuid())) != NULL)
   {
      length = strlen(pwd->pw_name);
      if (length > 0)
      {
         if ((user_offset + length + 1) < MAX_FULL_USER_ID_LENGTH)
         {
            (void)memcpy(&user[user_offset], pwd->pw_name, length);
            length += user_offset;
            if (fake_user[0] != '\0')
            {
               size_t tmp_length;

               tmp_length = strlen(fake_user);
               if ((length + 2 + tmp_length + 1) < MAX_FULL_USER_ID_LENGTH)
               {
                  user[length] = '-';
                  user[length + 1] = '>';
                  (void)memcpy(&user[length + 2], fake_user, tmp_length);
                  length += (2 + tmp_length);
               }
            }
            user[length] = '@';
            length++;
         }
         else
         {
            (void)memcpy(&user[user_offset], pwd->pw_name,
                         MAX_FULL_USER_ID_LENGTH - user_offset - 1);
            user[MAX_FULL_USER_ID_LENGTH - user_offset - 1] = '\0';
            length = MAX_FULL_USER_ID_LENGTH;
         }
      }
      else
      {
         if (8 < (MAX_FULL_USER_ID_LENGTH - user_offset))
         {
            length = user_offset + 7;
            (void)memcpy(&user[user_offset], "unknown", 7);
            if (fake_user[0] != '\0')
            {
               size_t tmp_length;

               tmp_length = strlen(fake_user);
               if ((length + 2 + tmp_length + 1) < MAX_FULL_USER_ID_LENGTH)
               {
                  user[length] = '-';
                  user[length + 1] = '>';
                  (void)memcpy(&user[length + 2], fake_user, tmp_length);
                  length += (2 + tmp_length);
               }
            }
            user[length] = '@';
            length++;
         }
         else
         {
            user[user_offset] = '\0';
            length = user_offset;
         }
      }
   }
   else
   {
      if (8 < (MAX_FULL_USER_ID_LENGTH - user_offset))
      {
         length = user_offset + 7;
         (void)memcpy(&user[user_offset], "unknown", 7);
         if (fake_user[0] != '\0')
         {
            size_t tmp_length;

            tmp_length = strlen(fake_user);
            if ((length + 2 + tmp_length + 1) < MAX_FULL_USER_ID_LENGTH)
            {
               user[length] = '-';
               user[length + 1] = '>';
               (void)memcpy(&user[length + 2], fake_user, tmp_length);
               length += (2 + tmp_length);
            }
         }
         user[length] = '@';
         length++;
      }
      else
      {
         user[user_offset] = '\0';
         length = user_offset;
      }
   }

   if (length < MAX_FULL_USER_ID_LENGTH)
   {
      if ((ptr = getenv("DISPLAY")) != NULL)
      {
         size_t display_length;

         display_length = strlen(ptr);
         if ((display_length >= 9) && (*ptr == 'l') && (*(ptr + 1) == 'o') &&
             (*(ptr + 2) == 'c') && (*(ptr + 3) == 'a') &&
             (*(ptr + 4) == 'l') && (*(ptr + 5) == 'h') &&
             (*(ptr + 6) == 'o') && (*(ptr + 7) == 's') &&
             (*(ptr + 8) == 't'))
         {
            if ((ptr = getenv("SSH_CLIENT")) != NULL)
            {
               char *search_ptr = ptr;

               while ((*search_ptr != ' ') && (*search_ptr != '\0') &&
                      (*search_ptr != '\t'))
               {
                  search_ptr++;
               }
               display_length = search_ptr - ptr;
               if ((length + display_length) >= MAX_FULL_USER_ID_LENGTH)
               {
                  if (user_offset > 0)
                  {
                     display_length = MAX_FULL_USER_ID_LENGTH - length - 1;
                  }
                  else
                  {
                     display_length = MAX_FULL_USER_ID_LENGTH - length;
                  }
               }
               (void)memcpy(&user[length], ptr, display_length);
               if (user_offset > 0)
               {
                  user[length + display_length] = '>';
                  user[length + display_length + 1] = '\0';
               }
               else
               {
                  user[length + display_length] = '\0';
               }
               return;
            }
            else
            {
               char *buffer = NULL;

               if ((exec_cmd("who am i", &buffer, -1, NULL, 0,
#ifdef HAVE_SETPRIORITY
                             NO_PRIORITY,
#endif
                             "", 0L, YES, YES) != INCORRECT) &&
                   (buffer != NULL))
               {
                  char *search_ptr = buffer;

                  while ((*search_ptr != '(') && (*search_ptr != '\0') &&
                         ((search_ptr - buffer) < (MAX_PATH_LENGTH + MAX_PATH_LENGTH)))
                  {
                     search_ptr++;
                  }
                  if (*search_ptr == '(')
                  {
                     char *start_ptr = search_ptr + 1;

                     search_ptr++;
                     while ((*search_ptr != ')') && (*search_ptr != '\0') &&
                            ((search_ptr - buffer) < (MAX_PATH_LENGTH + MAX_PATH_LENGTH)))
                     {
                        search_ptr++;
                     }
                     if (*search_ptr == ')')
                     {
                        int offset,
                            real_display_length = search_ptr - start_ptr;

                        if (user_offset > 0)
                        {
                           offset = 1;
                        }
                        else
                        {
                           offset = 0;
                        }
                        if ((length + real_display_length + offset) < MAX_FULL_USER_ID_LENGTH)
                        {
                           (void)memcpy(&user[length], start_ptr,
                                        real_display_length);
                           if (offset)
                           {
                              user[length + real_display_length] = '>';
                           }
                           user[length + real_display_length + offset] = '\0';
                           free(buffer);
                           return;
                        }
                     }
                  }
               }
               free(buffer);
               ptr = getenv("DISPLAY");
            }
         }
         if ((length + display_length) < MAX_FULL_USER_ID_LENGTH)
         {
            (void)strcpy(&user[length], ptr);
            if ((user_offset > 0) &&
                ((length + display_length + 1) < MAX_FULL_USER_ID_LENGTH))
            {
               (void)strcat(&user[length], ">");
            }
         }
      }
      else
      {
         if ((length + 7) < MAX_FULL_USER_ID_LENGTH)
         {
            (void)strcpy(&user[length], "unknown");
            if ((user_offset > 0) &&
                ((length + 7 + 1) < MAX_FULL_USER_ID_LENGTH))
            {
               (void)strcat(&user[length], ">");
            }
         }
      }
   }

   return;
}
