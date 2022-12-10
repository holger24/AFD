/*
 *  get_permissions.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2022 Deutscher Wetterdienst (DWD),
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
 **   get_permissions - retrieves permissions from the AFD_USER_FILE
 **                     for the calling user
 **
 ** SYNOPSIS
 **   int get_permissions(char **perm_buffer, char *fake_user, char *profile)
 **
 ** DESCRIPTION
 **   This function returns a list of permissions in 'perm_buffer'.
 **   To store the permissions it needs to allocate memory for
 **   which the caller is responsible to free.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it either fails to stat()/open()/read()
 **   from the AFD_USER_FILE. SUCCESS is returned when the calling
 **   user is found in AFD_USER_FILE and a list of permissions is
 **   stored in 'perm_buffer'. When the calling user could not be found,
 **   NONE is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   06.05.1997 H.Kiehl Created
 **   05.11.2003 H.Kiehl Ensure that we do check the full username.
 **   06.03.2006 H.Kiehl Do not allow all permissions if permission
 **                      file exist, but is not readable to user.
 **   04.12.2008 H.Kiehl When reading afd.users file check if we need
 **                      to convert from a dos style file to unix.
 **   31.01.2014 H.Kiehl The fix from 05.11.2003 was not complete.
 **                      Now we really check for complete username!
 **   15.10.2014 H.Kiehl Added support for taking the profile into
 **                      account.
 **
 */
DESCR__E_M3

#include <stdio.h>              /* NULL                                  */
#include <string.h>             /* strcat(), strcpy(), strerror()        */
#include <stdlib.h>             /* getenv(), calloc(), free()            */
#include <unistd.h>             /* read(), close(), getuid()             */
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>                /* getpwuid()                            */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>

/* External global variables. */
extern char *p_work_dir;


/*########################## get_permissions() ##########################*/
int
get_permissions(char **perm_buffer, char *fake_user, char *profile)
{
   int          fd,
                ret = SUCCESS;
   char         *buffer = NULL,
                *user = NULL,
                *user_profile = NULL,
                afd_user_file[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   if (fake_user[0] == '\0')
   {
      struct passwd *pwd;

      if ((pwd = getpwuid(getuid())) != NULL)
      {
         fd = strlen(pwd->pw_name) + 2;
         if ((user = malloc(fd)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "malloc() error : %s", strerror(errno));
            exit(INCORRECT);
         }
         (void)strcpy(&user[1], pwd->pw_name);
      }
      else
      {
         fd = sizeof("unknown") + 2;
         if ((user = malloc(fd)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "malloc() error : %s", strerror(errno));
            exit(INCORRECT);
         }
         (void)strcpy(&user[1], "unknown");
      }
   }
   else
   {
      fd = strlen(fake_user) + 2;
      if ((user = malloc(fd)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "malloc() error : %s", strerror(errno));
         exit(INCORRECT);
      }
      (void)strcpy(&user[1], fake_user);
   }
   user[0] = '\n';
   if ((profile != NULL) && (profile[0] != '\0'))
   {
      int length;

      length = fd + 1 + strlen(profile);
      if ((user_profile = malloc(length)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "malloc() error : %s", strerror(errno));
         exit(INCORRECT);
      }
      (void)memcpy(user_profile, user, fd);
      user_profile[fd - 1] = ':';
      (void)strcpy(&user_profile[fd], profile);
   }

   if ((strlen(p_work_dir) + ETC_DIR_LENGTH + AFD_USER_FILE_LENGTH + 1) > MAX_PATH_LENGTH)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Buffer for storing full name of %s to short %d > %d",
                 AFD_USER_FILE,
                 (strlen(p_work_dir) + ETC_DIR_LENGTH + AFD_USER_FILE_LENGTH + 1),
                 MAX_PATH_LENGTH);
      exit(INCORRECT);
   }
   (void)strcpy(afd_user_file, p_work_dir);
   (void)strcat(afd_user_file, ETC_DIR);
   (void)strcat(afd_user_file, AFD_USER_FILE);
   if ((fd = open(afd_user_file, O_RDONLY)) != -1)
   {
#ifdef HAVE_STATX
      if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) != -1)
#else
      if (fstat(fd, &stat_buf) != -1)
#endif
      {
#ifdef HAVE_STATX
         if ((stat_buf.stx_size > 0) && (stat_buf.stx_size < 1048576))
#else
         if ((stat_buf.st_size > 0) && (stat_buf.st_size < 1048576))
#endif
         {
            /* Create two buffers, one to scribble around 'buffer' the other */
            /* is returned to the calling process.                           */
#ifdef HAVE_STATX
            if (((buffer = malloc(stat_buf.stx_size + 2)) != NULL) &&
                ((*perm_buffer = calloc(stat_buf.stx_size, sizeof(char))) != NULL))
#else
            if (((buffer = malloc(stat_buf.st_size + 2)) != NULL) &&
                ((*perm_buffer = calloc(stat_buf.st_size, sizeof(char))) != NULL))
#endif
            {
               off_t   bytes_buffered;
#ifdef HAVE_GETLINE
               ssize_t n;
               size_t  line_buffer_length;
#else
               size_t  n;
#endif
               char    *read_ptr;
               FILE    *fp;

               if ((fp = fdopen(fd, "r")) == NULL)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Failed to read() `%s'. Permission control deactivated!!! : %s"),
                             afd_user_file, strerror(errno));
                  free(buffer); free(*perm_buffer); free(user); free(user_profile);
                  return(INCORRECT);
               }
               bytes_buffered = 0;
               read_ptr = buffer + 1;
#ifdef HAVE_GETLINE
# ifdef HAVE_STATX
               line_buffer_length = stat_buf.stx_size + 1;
# else
               line_buffer_length = stat_buf.st_size + 1;
# endif
               while ((n = getline(&read_ptr, &line_buffer_length, fp)) != -1)
#else
               while (fgets(read_ptr, MAX_LINE_LENGTH, fp) != NULL)
#endif
               {
#ifndef HAVE_GETLINE
                  n = strlen(read_ptr);
#endif
                  if (n > 1)
                  {
                     if (*(read_ptr + n - 2) == 13)
                     {
                        *(read_ptr + n - 2) = 10;
                        n--;
                     }
                  }
                  read_ptr += n;
#ifdef HAVE_GETLINE
                  line_buffer_length -= n;
#endif
                  bytes_buffered += n;
               }
               buffer[bytes_buffered] = '\0';

               if (fclose(fp) == EOF)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             _("Failed to fclose() `%s' : %s"),
                             afd_user_file, strerror(errno));
               }
            }
            else
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to allocate memory. Permission control deactivated!!! : %s"),
                          strerror(errno));
               ret = INCORRECT;
               free(buffer);

               if (close(fd) == -1)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             _("close() error : %s"), strerror(errno));
               }
            }
         }
         else
         {
#ifdef HAVE_STATX
            if (stat_buf.stx_size != 0)
#else
            if (stat_buf.st_size != 0)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("The function get_permissions() was not made to handle large file."));
            }
            ret = NONE;

            if (close(fd) == -1)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          _("close() error : %s"), strerror(errno));
            }
         }
      }
      else
      {
         if (errno == ENOENT)
         {
            /*
             * If there is no AFD_USER_FILE, or we fail to open
             * it because it does not exist, lets allow everything.
             */
            ret = INCORRECT;
         }
         else
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("Failed to fstat() permission file `%s' : %s"),
                       afd_user_file, strerror(errno));
            if (errno == EACCES)
            {
               ret = NO_ACCESS;
            }
            else
            {
               ret = NONE;
            }
         }

         if (close(fd) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("close() error : %s"), strerror(errno));
         }
      }
   }
   else
   {
      if (errno == ENOENT)
      {
         /*
          * If there is no AFD_USER_FILE, or we fail to open
          * it because it does not exist, lets allow everything.
          */
         ret = INCORRECT;
      }
      else
      {
         if (errno == EACCES)
         {
            ret = NO_ACCESS;
         }
         else
         {
            ret = NONE;
         }
      }
   }

   if (ret == SUCCESS)
   {
      char *ptr;

      buffer[0] = '\n';

      if (user_profile != NULL)
      {
         if (((ptr = posi(buffer, user_profile)) != NULL) &&
             ((*(ptr - 1) == ' ') || (*(ptr - 1) == '\t')))
         {
            int  i = 0;

            do
            {
               while ((*ptr == ' ') || (*ptr == '\t'))
               {
                  ptr++;
               }
               while ((*ptr != '\n') && (*ptr != '\0'))
               {
                  *(*perm_buffer + i) = *ptr;
                  i++; ptr++;
               }
#ifdef HAVE_STATX
               if (((ptr + 1) - buffer) >= stat_buf.stx_size)
#else
               if (((ptr + 1) - buffer) >= stat_buf.st_size)
#endif
               {
                  break;
               }
               ptr++;
            } while ((*ptr == ' ') || (*ptr == '\t'));

            *(*perm_buffer + i) = '\0';
            ret = NEITHER;
         }
      }

      if (ret == NEITHER)
      {
         ret = SUCCESS;
      }
      else
      {
         if (((ptr = posi(buffer, user)) != NULL) &&
             ((*(ptr - 1) == ' ') || (*(ptr - 1) == '\t')))
         {
            int  i = 0;

            do
            {
               while ((*ptr == ' ') || (*ptr == '\t'))
               {
                  ptr++;
               }
               while ((*ptr != '\n') && (*ptr != '\0'))
               {
                  *(*perm_buffer + i) = *ptr;
                  i++; ptr++;
               }
#ifdef HAVE_STATX
               if (((ptr + 1) - buffer) >= stat_buf.stx_size)
#else
               if (((ptr + 1) - buffer) >= stat_buf.st_size)
#endif
               {
                  break;
               }
               ptr++;
            } while ((*ptr == ' ') || (*ptr == '\t'));

            *(*perm_buffer + i) = '\0';
         }
         else
         {
            free(*perm_buffer);
            ret = NONE;
         }
      }
      free(buffer);
   }
   free(user);
   free(user_profile);

   /*
    * The caller is responsible to free 'perm_buffer'!
    */
   return(ret);
}
