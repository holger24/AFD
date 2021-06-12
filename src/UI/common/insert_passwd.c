/*
 *  insert_passwd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2021 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   insert_passwd - inserts the passwd for the given url
 **
 ** SYNOPSIS
 **   void insert_passwd(char *url)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   If the passwd is found the url is returned with passwd.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   23.12.2003 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>     /* memmove(), memcpy(), strlen()                 */
#include "ui_common_defs.h"


/*############################ insert_passwd() ##########################*/
void
insert_passwd(char *url)
{
   int  i;
   char *ptr = url,
        uh_name[MAX_USER_NAME_LENGTH + MAX_REAL_HOSTNAME_LENGTH + 1];

   while ((*ptr != ':') && (*ptr != '\0'))
   {
      ptr++;
   }
   if (*ptr == '\0')
   {
      /* This is not in URL format, just return. */
      return;
   }
   ptr += 3; /* Away with '://'. */
   i = 0;
   while ((i < (MAX_USER_NAME_LENGTH + MAX_REAL_HOSTNAME_LENGTH + 1)) &&
          (*ptr != ':') && (*ptr != '@') && (*ptr != '\0'))
   {
      if (*ptr == '\\')
      {
         ptr++;
      }
      uh_name[i] = *ptr;
      i++; ptr++;
   }
   if (*ptr == ':')
   {
      /* Oops, passwd is already there, nothing for us to do. */ ;
   }
   else if (*ptr == '@')
        {
           if (i < MAX_USER_NAME_LENGTH)
           {
              int  uh_name_length = i;
              char password[MAX_USER_NAME_LENGTH + 1],
                   *p_host = ptr;

              i = 0;
              ptr++;
              while ((*ptr != '\0') && (*ptr != '/') &&
                     (*ptr != ':') && (*ptr != ';') &&
                     (i < MAX_REAL_HOSTNAME_LENGTH))
              {
                 if (*ptr == '\\')
                 {
                    ptr++;
                 }
                 uh_name[uh_name_length + i] = *ptr;
                 i++; ptr++;
              }
              uh_name[uh_name_length + i] = '\0';
              if (get_pw(uh_name, password, YES) == SUCCESS)
              {
                 size_t pwd_length;

                 pwd_length = strlen(password);
                 if (pwd_length > 0)
                 {
                    (void)memmove(p_host + pwd_length + 1, p_host,
                                  strlen(p_host) + 1);
                    *p_host = ':';
                    (void)memcpy(p_host + 1, password, pwd_length);
                 }
              }
           }
        }

   return;
}
