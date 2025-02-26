/*
 *  create_remote_dir.c - Part of AFD, an automatic file distribution
 *                        program.
 *  Copyright (c) 2000 - 2025 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   create_remote_dir - creates a directory name from a url
 **
 ** SYNOPSIS
 **   int create_remote_dir(char *url,
 **                         char *local_work_dir,
 **                         char *user,
 **                         char *host_alias,
 **                         char *directory,
 **                         char *remote_dir,
 **                         int  *remote_dir_length)
 **
 ** DESCRIPTION
 **   This function creates a directory name from a URL (url) or if this
 **   is NULL creates it from the given user, host_alias and directory.
 **   The resulting directory is stored in remote_dir and has the
 **   following format:
 **
 **   $AFD_WORK_DIR/files/incoming/<user>@<hostname>/[<user>/]<remote dir>
 **
 **   When the remote directory is an absolute path the second <user>
 **   will NOT be inserted.
 **
 ** RETURN VALUES
 **   When remote_dir has the correct format it will return SUCCESS
 **   and the new directory name will be returned by overwritting
 **   the variable remote_dir. The length of the string stored in
 **   remote_dir will be returned by remote_dir_length and will include
 **   the terminating '\0'. On error INCORRECT will be returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   13.08.2000 H.Kiehl Created
 **   15.04.2008 H.Kiehl Accept url's without @ sign such as http://idefix.
 **   20.04.2008 H.Kiehl Let function url_evaluate() handle the URL.
 **   24.10.2008 H.Kiehl Additional parameter to return the length of
 **                      remote_dir.
 **   22.04.2016 H.Kiehl Added parameter local_work_dir.
 **   26.02.2025 H.Kiehl When url is not NULL, allow overriding values
 **                      user, host_alias and directory by passing a none
 **                      NULL argument.
 **
 */
DESCR__E_M3

#include <stdio.h>           /* snprintf()                               */


/*######################### create_remote_dir() #########################*/
int
create_remote_dir(char *url,
                  char *local_work_dir,
                  char *user,
                  char *host_alias,
                  char *directory,
                  char *remote_dir,
                  int  *remote_dir_length)
{
   int ret;

   if (url == NULL)
   {
      if (directory[0] == '/')
      {
         *remote_dir_length = snprintf(remote_dir, MAX_PATH_LENGTH,
                                       "%s%s%s/%s@%s%s", local_work_dir,
                                       AFD_FILE_DIR, INCOMING_DIR, user,
                                       host_alias, directory) + 1;
      }
      else if (directory[0] == '\0')
           {
              if (user[0] == '\0')
              {
                 *remote_dir_length = snprintf(remote_dir, MAX_PATH_LENGTH,
                                               "%s%s%s/@%s",
                                               local_work_dir, AFD_FILE_DIR,
                                               INCOMING_DIR, host_alias) + 1;
              }
              else
              {
                 *remote_dir_length = snprintf(remote_dir, MAX_PATH_LENGTH,
                                               "%s%s%s/%s@%s/%s",
                                               local_work_dir, AFD_FILE_DIR,
                                               INCOMING_DIR, user, host_alias,
                                               user) + 1;
              }
           }
           else
           {
              if (user[0] == '\0')
              {
                 *remote_dir_length = snprintf(remote_dir, MAX_PATH_LENGTH,
                                               "%s%s%s/@%s/%s",
                                               local_work_dir, AFD_FILE_DIR,
                                               INCOMING_DIR, host_alias,
                                               directory) + 1;
              }
              else
              {
                 *remote_dir_length = snprintf(remote_dir, MAX_PATH_LENGTH,
                                               "%s%s%s/%s@%s/%s/%s",
                                               local_work_dir, AFD_FILE_DIR,
                                               INCOMING_DIR, user, host_alias,
                                               user, directory) + 1;
              }
           }
      ret = SUCCESS;
   }
   else
   {
      unsigned int error_mask;
      char         *p_directory,
                   *p_host_alias,
                   *p_user,
                   url_directory[MAX_RECIPIENT_LENGTH + 1],
                   url_host_alias[MAX_REAL_HOSTNAME_LENGTH + 1],
                   url_user[MAX_USER_NAME_LENGTH + 1];

      if (user == NULL)
      {
         p_user = url_user;
      }
      else
      {
         p_user = user;
      }
      if (host_alias == NULL)
      {
         p_host_alias = url_host_alias;
      }
      else
      {
         p_host_alias = host_alias;
      }
      if (directory == NULL)
      {
         p_directory = url_directory;
      }
      else
      {
         p_directory = directory;
      }
      if ((error_mask = url_evaluate(url, NULL,
                                     (user == NULL) ? url_user : NULL, NULL,
                                     NULL,
#ifdef WITH_SSH_FINGERPRINT
                                     NULL, NULL,
#endif
                                     NULL, NO,
                                     (host_alias == NULL) ? url_host_alias : NULL,
                                     NULL,
                                     (directory == NULL) ? url_directory : NULL,
                                     NULL, NULL, NULL, NULL,
                                     NULL, NULL, NULL, NULL)) > 3)
      {
         char error_msg[MAX_URL_ERROR_MSG];

         url_get_error(error_mask, error_msg, MAX_URL_ERROR_MSG);
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Incorrect url `%s'. Error is: %s."), url, remote_dir);
         remote_dir[0] = '\0';
         ret = INCORRECT;
      }
      else
      {
         if (p_directory[0] == '/')
         {
            *remote_dir_length = snprintf(remote_dir, MAX_PATH_LENGTH,
                                          "%s%s%s/%s@%s%s",
                                          local_work_dir, AFD_FILE_DIR,
                                          INCOMING_DIR, p_user,
                                          p_host_alias, p_directory) + 1;
         }
         else if (p_directory[0] == '\0')
              {
                 if (p_user[0] == '\0')
                 {
                    *remote_dir_length = snprintf(remote_dir, MAX_PATH_LENGTH,
                                                  "%s%s%s/@%s",
                                                  local_work_dir, AFD_FILE_DIR,
                                                  INCOMING_DIR,
                                                  p_host_alias) + 1;
                 }
                 else
                 {
                    *remote_dir_length = snprintf(remote_dir, MAX_PATH_LENGTH,
                                                  "%s%s%s/%s@%s/%s",
                                                  local_work_dir, AFD_FILE_DIR,
                                                  INCOMING_DIR, p_user,
                                                  p_host_alias, p_user) + 1;
                 }
              }
              else
              {
                 if (p_user[0] == '\0')
                 {
                    *remote_dir_length = snprintf(remote_dir, MAX_PATH_LENGTH,
                                                  "%s%s%s/@%s/%s",
                                                  local_work_dir, AFD_FILE_DIR,
                                                  INCOMING_DIR, p_host_alias,
                                                  p_directory) + 1;
                 }
                 else
                 {
                    *remote_dir_length = snprintf(remote_dir, MAX_PATH_LENGTH,
                                                  "%s%s%s/%s@%s/%s/%s",
                                                  local_work_dir, AFD_FILE_DIR,
                                                  INCOMING_DIR, p_user,
                                                  p_host_alias, p_user,
                                                  p_directory) + 1;
                 }
              }
         ret = SUCCESS;
      }
   }

   return(ret);
}
