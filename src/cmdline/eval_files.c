/*
 *  eval_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2015 Deutscher Wetterdienst (DWD),
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

#include "aftpdefs.h"

DESCR__S_M3
/*
 ** NAME
 **   eval_config_file - evaluates configuration file
 **   eval_filename_file - evaluates file with filenames
 **
 ** SYNOPSIS
 **   void eval_config_file(char *file_name, struct data *p_db)
 **   void eval_filename_file(char *file_name, struct data *p_db)
 **
 ** DESCRIPTION
 **   These functions read the configuration files and store the
 **   data into struct data.
 **
 ** RETURN VALUES
 **   None, will just exit when it encounters any difficulties.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.07.2000 H.Kiehl Created
 **   29.01.2005 H.Kiehl Determine max file name length and only
 **                      allocate this.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* stderr, fprintf()                  */
#include <stdlib.h>                /* atoi()                             */
#include <string.h>                /* strcpy(), strerror(), strcmp()     */
#include <errno.h>

/* External global definitions */
extern int sys_log_fd;


/*######################### eval_config_file() ##########################*/
void
eval_config_file(char *file_name, struct data *p_db)
{
   off_t length;
   char  *buffer;

   if ((length = read_file_no_cr(file_name, &buffer, NO, __FILE__, __LINE__)) != INCORRECT)
   {
      char *ptr = buffer;

      if (buffer[length - 1] == '\n')
      {
         buffer[length - 1] = '\0';
      }
      while ((*ptr != ':') && (*ptr != '\0'))
      {
         ptr++;
      }
      if (*ptr == ':')
      {
         register int i;

         /* Get user name */
         if ((*(ptr + 1) == '/') && (*(ptr + 2) == '/'))
         {
            ptr += 3; /* Away with '://' */
            if (*ptr == '\0')
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         _("Just telling me the sheme and nothing else is not of much use! (%s %d)\n"),
                         __FILE__, __LINE__);
               exit(INCORRECT);
            }
            i = 0;
            while ((*ptr != ':') && (*ptr != '@') && (*ptr != '\0') &&
                   (i < MAX_USER_NAME_LENGTH))
            {
               if (*ptr == '\\')
               {
                  ptr++;
               }
               p_db->user[i] = *ptr;
               ptr++; i++;
            }
            if (*ptr == '\0')
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         _("Hmm. This does NOT look like URL for me!? (%s %d)\n"),
                         __FILE__, __LINE__);
               exit(INCORRECT);  
            }
            if (i == MAX_USER_NAME_LENGTH)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         _("Unable to store user name. It is longer than %d bytes! (%s %d)\n"),
                         MAX_USER_NAME_LENGTH, __FILE__, __LINE__);
               exit(INCORRECT);  
            }
            p_db->user[i] = '\0';
         }
         else
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      _("This definitely is GARBAGE! Get a new administrator!!! (%s %d)\n"),
                      __FILE__, __LINE__);
            exit(INCORRECT);
         }
         if (*ptr == ':')
         {
            ptr++;

            /* Get password */
            i = 0;
            while ((*ptr != '@') && (*ptr != '\0') &&
                   (i < MAX_USER_NAME_LENGTH))
            {
               if (*ptr == '\\')
               {
                  ptr++;
               }
               p_db->password[i] = *ptr;
               ptr++; i++;
            }
            if ((i == 0) && (*ptr != '@'))
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         _("Hmmm. How am I suppose to find the hostname? (%s %d)\n"),
                         __FILE__, __LINE__);
               exit(INCORRECT);
            }
            if (i == MAX_USER_NAME_LENGTH)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         _("Unable to store password. It is longer than %d bytes! (%s %d)\n"),
                         MAX_USER_NAME_LENGTH, __FILE__, __LINE__);
               exit(INCORRECT);
            }
            p_db->password[i] = '\0';
            ptr++;
         }
         else if (*ptr == '@')
              {
                 ptr++;

                 /* No password given. This could be a mail or  */
                 /* we want to do anonymous ftp. Since we       */
                 /* currently do not know what protocol we have */
                 /* here lets set the password to 'anonymous'.  */
                 (void)strcpy(p_db->password, "anonymous");
              }
              else
              {
                 (void)rec(sys_log_fd, ERROR_SIGN,
                           _("Hmmm. How am I suppose to find the hostname? (%s %d)\n"),
                           __FILE__, __LINE__);
                 exit(INCORRECT);
              }

         /* Now lets get the host alias name. */
         i = 0;
         while ((*ptr != '\0') && (*ptr != '/') &&
                (*ptr != ':') && (*ptr != ';') && (i < MAX_FILENAME_LENGTH))
         {
            if (*ptr == '\\')
            {
               ptr++;
            }
            p_db->hostname[i] = *ptr;
            i++; ptr++;
         }
         if (i == MAX_FILENAME_LENGTH)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      _("Unable to store hostname. It is longer than %d bytes! (%s %d)\n"),
                      MAX_FILENAME_LENGTH, __FILE__, __LINE__);
            exit(INCORRECT);
         }
         p_db->hostname[i] = '\0';

         /* Save TCP port number. */
         if (*ptr == ':')
         {
            char *ptr_tmp;

            ptr++;
            ptr_tmp = ptr;
            while ((*ptr != '/') && (*ptr != '\0') && (*ptr != ';'))
            {
               ptr++;
            }
            if (*ptr == '/')
            {
               *ptr = '\0';
               p_db->port = atoi(ptr_tmp);
               *ptr = '/';
            }
            else if (*ptr == '\0')
                 {
                    p_db->port = atoi(ptr_tmp);
                 }
         }

         /* Save the remote directory. */
         if (*ptr == '/')
         {
            ptr++;
            i = 0;
            while ((*ptr != '\0') && (*ptr != ';') && (i < MAX_PATH_LENGTH))
            {
               p_db->remote_dir[i] = *ptr;
               i++; ptr++;
            }
            if (i == MAX_PATH_LENGTH)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         _("Unable to store remote dir. It is longer than %d bytes! (%s %d)\n"),
                         MAX_PATH_LENGTH, __FILE__, __LINE__);
               exit(INCORRECT);
            }
            p_db->remote_dir[i] = '\0';
         }

         /* Save the type code (FTP) or the server name (SMTP). */
         if (*ptr == ';')
         {
            int  count = 0;
            char *ptr_tmp;

            ptr++;
            while ((*ptr == ' ') || (*ptr == '\t'))
            {
               ptr++;
            }
            ptr_tmp = ptr;
            while ((*ptr != '\0') && (*ptr != '='))
            {
               ptr++; count++;
            }
            if (*ptr == '=')
            {
               if (count == 4)
               {
                  if ((*(ptr - 1) != 'e') || (*(ptr - 2) != 'p') ||
                      (*(ptr - 3) != 'y') || (*(ptr - 4) != 't'))
                  {
                     *ptr = '\0';
                     (void)rec(sys_log_fd, ERROR_SIGN,
                               _("Actually I was expecting <type=> and not <%s=> (%s %d)\n"),
                               ptr_tmp, __FILE__, __LINE__);
                     *ptr = '=';
                     exit(INCORRECT);
                  }
                  ptr++;
                  switch (*ptr)
                  {
                     case 'a': /* ASCII */
                     case 'A': p_db->transfer_mode = 'A';
                               break;
                     case 'd': /* DOS binary */
                     case 'D': p_db->transfer_mode = 'D';
                               break;
                     case 'i': /* Image/binary */
                     case 'I': p_db->transfer_mode = 'I';
                               break;
                     default : (void)rec(sys_log_fd, ERROR_SIGN,
                                         _("Unknown ftp type (%d). Changing to I. (%s %d)\n"),
                                         *ptr, __FILE__, __LINE__);
                               p_db->transfer_mode = 'I';
                               break;
                  }
               }
               else if (count == 6)
                    {
                       if ((*(ptr - 1) != 'r') || (*(ptr - 2) != 'e') ||
                           (*(ptr - 3) != 'v') || (*(ptr - 4) != 'r') ||
                           (*(ptr - 5) != 'e') || (*(ptr - 6) != 's'))
                       {
                          *ptr = '\0';
                          (void)rec(sys_log_fd, ERROR_SIGN,
                                    _("Actually I was expecting <server=> and not <%s=> (%s %d)\n"),
                                    ptr_tmp, __FILE__, __LINE__);
                          *ptr = '=';
                          exit(INCORRECT);
                       }
                       ptr++;
                       i = 0;
                       while ((*ptr != '\0') && (*ptr != ' ') &&
                              (*ptr != '\t') && (i < MAX_USER_NAME_LENGTH))
                       {
                          p_db->smtp_server[i] = *ptr;
                          i++; ptr++;
                       }
                       if (i == MAX_USER_NAME_LENGTH)
                       {
                          (void)rec(sys_log_fd, ERROR_SIGN,
                                    _("Unable to store server name. It is longer than %d bytes! (%s %d)\n"),
                                    MAX_USER_NAME_LENGTH, __FILE__, __LINE__);
                          exit(INCORRECT);
                       }
                       p_db->smtp_server[i] = '\0';
                    }
            }
            /*
             * Ignore anything behind the ftp type.
             */
         }
      }
      free(buffer);
   }

   return;
}


/*####################### eval_filename_file() ##########################*/
int
eval_filename_file(char *file_name, struct data *p_db)
{
   int  ret;
   char *buffer;

   if (read_file_no_cr(file_name, &buffer, NO, __FILE__, __LINE__) == INCORRECT)
   {
      ret = INCORRECT;
   }
   else
   {
      size_t length,
             max_length,
             max_rename_length;
      char   *p_start,
             *ptr = buffer;

      /* First lets count the number of entries so we can later */
      /* allocate the memory we need.                           */
      max_length = 0;
      max_rename_length = 0;
      p_db->no_of_files = 0;
      do
      {
         p_start = ptr;
         while ((*ptr != '\n') && (*ptr != '|') && (*ptr != '\0'))
         {
            ptr++;
         }
         if ((*ptr == '\n') || (*ptr == '|'))
         {
            length = ptr - p_start;
            if (length > max_length)
            {
               max_length = length;
            }
            p_db->no_of_files++;
            if (*ptr == '|')
            {
               ptr++;
               p_start = ptr;
               while ((*ptr != '\n') && (*ptr != '\0'))
               {
                  ptr++;
               }
               if (*ptr == '\n')
               {
                  length = ptr - p_start;
                  if (length > max_rename_length)
                  {
                     max_rename_length = length;
                  }
               }
            }
            ptr++;
         }
      } while (*ptr != '\0');

      if (max_length > 0)
      {
         register int i, j;

         max_length++;
         RT_ARRAY(p_db->filename, p_db->no_of_files, max_length, char);
         if (max_rename_length > 0)
         {
            max_rename_length++;
            RT_ARRAY(p_db->realname, p_db->no_of_files,
                     max_rename_length, char);
         }

         ptr = buffer;
         j = 0;
         do
         {
            i = 0;
            while ((*ptr != '\n') && (*ptr != '|') && (*ptr != '\0'))
            {
               p_db->filename[j][i] = *ptr;
               ptr++; i++;
            }
            if (*ptr == '\n')
            {
               ptr++;
            }
            p_db->filename[j][i] = '\0';
            if (*ptr == '|')
            {
               ptr++;
               i = 0;
               while ((*ptr != '\n') && (*ptr != '\0'))
               {
                  p_db->realname[j][i] = *ptr;
                  ptr++; i++;
               }
               if (*ptr == '\n')
               {
                  ptr++;
               }
               p_db->realname[j][i] = '\0';
            }
            else
            {
               if (max_rename_length > 0)
               {
                  p_db->realname[j][0] = '\0';
               }
            }
            j++;
         } while (*ptr != '\0');
         ret = SUCCESS;
      }
      else
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   _("No files in %s (%s %d)\n"),
                   file_name, __FILE__, __LINE__);
         ret = INCORRECT;
      }

      free(buffer);
   }
#ifdef _DEBUG
   (void)rec(sys_log_fd, DEBUG_SIGN,
             "Found %d files in %s\n", p_db->no_of_files, file_name);
#endif

   return(ret);
}
