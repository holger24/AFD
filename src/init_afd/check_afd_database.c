/*
 *  check_afd_database.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2022 Deutscher Wetterdienst (DWD),
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
 **   check_afd_database - controls startup and shutdown of AFD
 **
 ** SYNOPSIS
 **   int check_afd_database(void)
 **
 ** DESCRIPTION
 **   The function check_afd_database() checks if there is a readable
 **   DIR_CONFIG file. If not and AFD was compiled with WITH_AUTO_CONFIG
 **   a default DIR_CONFIG is created.
 **
 ** RETURN VALUES
 **   On success 0 is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   05.08.2013 H.Kiehl Check correct DIR_CONFIG in case when we specify
 **                      another one in AFD_CONFIG in function
 **                      check_afd_database().
 **   26.11.2018 H.Kiehl Created from afd.c.
 **
 */
DESCR__E_M1

#include <stdio.h>            /* fprintf(), NULL                         */
#include <string.h>           /* strcpy(), strerror()                    */
#include <stdlib.h>           /* free()                                  */
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>           /* Definition of AT_* constants            */
#endif
#include <sys/stat.h>         /* stat()                                  */
#include <unistd.h>           /* R_OK                                    */
#include <errno.h>

/* External global variables. */
extern char *p_work_dir;


/*######################### check_afd_database() ########################*/
int
check_afd_database(void)
{
   int  length,
        ret;
   char db_file[MAX_PATH_LENGTH];

   length = snprintf(db_file, MAX_PATH_LENGTH, "%s%s", p_work_dir, ETC_DIR);
   (void)strcpy(&db_file[length], AFD_CONFIG_FILE);
   if (eaccess(db_file, R_OK) == -1)
   {
      (void)strcpy(&db_file[length], DEFAULT_DIR_CONFIG_FILE);
      ret = eaccess(db_file, R_OK);
   }
   else
   {
      char *buffer,
           config_file[MAX_PATH_LENGTH + 5];

      ret = -1;
      if (read_file_no_cr(db_file, &buffer, YES, __FILE__, __LINE__) != INCORRECT)
      {
         char *ptr;

         ptr = buffer;
         while ((ptr = get_definition(ptr, DIR_CONFIG_NAME_DEF,
                                      config_file, MAX_PATH_LENGTH)) != NULL)
         {
            if (config_file[0] != '/')
            {
               if (config_file[0] == '~')
               {
                  size_t config_length;
                  char   *p_path,
                         user[MAX_USER_NAME_LENGTH + 1];

                  if (config_file[1] == '/')
                  {
                     p_path = &config_file[2];
                     user[0] = '\0';
                  }
                  else
                  {
                     int j = 0;

                     p_path = &config_file[1];
                     while ((*(p_path + j) != '/') && (*(p_path + j) != '\0') &&
                            (j < MAX_USER_NAME_LENGTH))
                     {
                        user[j] = *(p_path + j);
                        j++;
                     }
                     user[j] = '\0';
                  }
                  (void)expand_path(user, p_path);
                  config_length = strlen(p_path) + 1;
                  (void)memmove(config_file, p_path, config_length);
               }
               else
               {
                  char tmp_config_file[MAX_PATH_LENGTH];

                  (void)strcpy(tmp_config_file, config_file);
                  (void)snprintf(config_file, MAX_PATH_LENGTH + 5, "%s%s/%s",
                                 p_work_dir, ETC_DIR, tmp_config_file);
               }
            }

            if ((ret = eaccess(config_file, R_OK)) == 0)
            {
               break;
            }
         }
         free(buffer);
      }

      if (ret == -1)
      {
         (void)strcpy(&db_file[length], DEFAULT_DIR_CONFIG_FILE);
         ret = eaccess(db_file, R_OK);
      }
   }

#ifdef WITH_AUTO_CONFIG
   if (ret == -1)
   {
      FILE         *fp;
# ifdef HAVE_STATX
      struct statx stat_buf;
# else
      struct stat  stat_buf;
# endif

      db_file[length] = '\0';
# ifdef HAVE_STATX
      if (statx(0, db_file, AT_STATX_SYNC_AS_STAT, 0, &stat_buf) == -1)
# else
      if (stat(db_file, &stat_buf) == -1)
# endif
      {
         if (errno == ENOENT)
         {
            if (mkdir(db_file, DIR_MODE) == -1)
            {
               (void)fprintf(stderr, _("Failed to mkdir() `%s' : %s\n"),
                             db_file, strerror(errno));
               return(-1);
            }
         }
         else
         {
            return(-1);
         }
      }
      (void)snprintf(db_file, MAX_PATH_LENGTH, "%s %s 2>&1",
                     AFD_AUTO_CONFIG, p_work_dir);
      if ((fp = popen(db_file, "r")) == NULL)
      {
         (void)fprintf(stderr, _("Failed to popen() `%s' : %s\n"),
                       db_file, strerror(errno));
         return(-1);
      }
      db_file[0] = '\0';
      while (fgets(db_file, MAX_PATH_LENGTH, fp) != NULL)
      {
         ;
      }
      if (db_file[0] != '\0')
      {
         (void)fprintf(stderr, _("%s failed : `%s'\n"),
                       AFD_AUTO_CONFIG, db_file);
      }
      if (ferror(fp))
      {
         (void)fprintf(stderr, _("ferror() error : %s\n"), strerror(errno));
      }
      if (pclose(fp) == -1)
      {
         (void)fprintf(stderr, _("Failed to pclose() : %s\n"), strerror(errno));
      }
      (void)snprintf(db_file, MAX_PATH_LENGTH, "%s%s%s",
                     p_work_dir, ETC_DIR, DEFAULT_DIR_CONFIG_FILE);
      return(eaccess(db_file, R_OK));
   }
   else
   {
      return(0);
   }
#else
   return(ret);
#endif
}
