/*
 *  check_fake_user.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2004 - 2015 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_fake_user - checks for fake user and if this is allowed
 **
 ** SYNOPSIS
 **   void check_fake_user(int  *argc,
 **                        char *argv[],
 **                        char *config_file,
 **                        char *fake_user)
 **
 ** DESCRIPTION
 **   The function checks if the arguments contain a "-u" to set
 **   a fake user, if the `config_file' allows this. If no user
 **   is passed with the option it tries to get the user from the
 **   `config_file', or else checks in it if the user may set to
 **   this user.
 **
 ** RETURN VALUES
 **   Will return the fake username in 'fake_user'.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.05.2004 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>              /* NULL                                  */
#include <string.h>             /* strcat(), strlen(), memcpy()          */
#include <stdlib.h>             /* free(), exit()                        */
#include <sys/types.h>
#include <pwd.h>                /* getpwuid()                            */
#include <unistd.h>             /* getuid()                              */
#include "permission.h"

/* External global variables. */
extern char *p_work_dir;


/*######################### check_fake_user() ###########################*/
void
check_fake_user(int *argc, char *argv[], char *config_file, char *fake_user)
{
   register int i;
   char         wanted_user[MAX_FULL_USER_ID_LENGTH];

   wanted_user[0] = 1;
   for (i = 1; i < *argc; i++)
   {
      if (CHECK_STRCMP(argv[i], "-u") == 0)
      {
         if (((i + 1) < *argc) && (argv[i + 1][0] != '-'))
         {
            /* Check if the buffer is long enough! */
            if (MAX_FULL_USER_ID_LENGTH < strlen(argv[i + 1]))
            {
               (void)fprintf(stderr,
                             _("Buffer for storing fake user to short. (%s %d)\n"),
                             __FILE__, __LINE__);
               fake_user[0] = '\0';
               return;
            }
            (void)strcpy(wanted_user, argv[i + 1]);
            if ((i + 2) < *argc)
            {
               register int j;

               for (j = i; j < *argc; j++)
               {
                  argv[j] = argv[j + 2];
               }
               argv[j] = NULL;
            }
            else
            {
               argv[i] = NULL;
            }
            *argc -= 2;
            i = *argc;
         }
         else
         {
            /* No fake user supplied, so lets take it from */
            /* config file.                                */
            wanted_user[0] = '\0';
            if ((i + 1) < *argc)
            {
               register int j;

               for (j = i; j < *argc; j++)
               {
                  argv[j] = argv[j + 1];
               }
               argv[j] = NULL;
            }
            else
            {
               argv[i] = NULL;
            }
            *argc -= 1;
            i = *argc;
         }
         break;
      }
   }

   if (wanted_user[0] != 1)
   {
      struct passwd *pwd;

      if ((pwd = getpwuid(getuid())) != NULL)
      {
         char *buffer = NULL,
              full_config_name[MAX_PATH_LENGTH];

         (void)snprintf(full_config_name, MAX_PATH_LENGTH, "%s%s%s",
                        p_work_dir, ETC_DIR, config_file);
         if ((eaccess(full_config_name, F_OK) == 0) &&
             (read_file_no_cr(full_config_name, &buffer, YES, __FILE__, __LINE__) != INCORRECT))
         {
            char fake_user_list[MAX_PATH_LENGTH];

            if (get_definition(buffer, FAKE_USER_DEF,
                               fake_user_list, MAX_PATH_LENGTH) != NULL)
            {
               size_t length;

               length = strlen(pwd->pw_name);
               if (length > 0)
               {
                  int  change_char;
                  char real_user[MAX_FULL_USER_ID_LENGTH + 2],
                       *ptr,
                       *tmp_ptr;

                  if ((length + 1) < MAX_FULL_USER_ID_LENGTH)
                  {
                     (void)memcpy(real_user, pwd->pw_name, length);
                  }
                  else
                  {
                     (void)memcpy(real_user, pwd->pw_name,
                                  MAX_FULL_USER_ID_LENGTH - 1);
                     length = MAX_FULL_USER_ID_LENGTH - 1;
                  }
                  real_user[length] = '-';
                  real_user[length + 1] = '>';
                  real_user[length + 2] = '\0';

                  ptr = fake_user_list;
                  while ((ptr = lposi(ptr, real_user, length + 2)) != NULL)
                  {
                     ptr--;
                     tmp_ptr = ptr;
                     while ((*tmp_ptr != ',') && (*tmp_ptr != '\0'))
                     {
                        tmp_ptr++;
                     }
                     if (*tmp_ptr == ',')
                     {
                        change_char = YES;
                        *tmp_ptr = '\0';
                     }
                     else
                     {
                        change_char = NO;
                     }
                     if ((wanted_user[0] == '\0') ||
                         (CHECK_STRCMP(ptr, wanted_user) == 0))
                     {
                        i = 0;
                        while ((*(ptr + i) != ' ') && (*(ptr + i) != '\0') &&
                               (*(ptr + i) != '\t') &&
                               (i < MAX_FULL_USER_ID_LENGTH))
                        {
                           fake_user[i] = *(ptr + i);
                           i++;
                        }
                        fake_user[i] = '\0';
                        free(buffer);
                        return;
                     }
                     if (change_char == YES)
                     {
                        *tmp_ptr = ',';
                     }
                     ptr = tmp_ptr;
                  }
               }
            }
         }
         free(buffer);
         (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
         exit(INCORRECT);
      }
   }
   fake_user[0] = '\0';

   return;
}
