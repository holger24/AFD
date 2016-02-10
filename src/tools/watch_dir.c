/*
 *  watch_dir.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2015 Deutscher Wetterdienst (DWD),
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

DESCR__S_M1
/*
 ** NAME
 **   watch_dir - prints out all file names that enter a directory
 **
 ** SYNOPSIS
 **   watch_dir
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.04.1996 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>              /* fprintf(), printf(), stderr           */
#include <string.h>             /* strcpy(), strerror()                  */
#include <stdlib.h>             /* exit()                                */
#include <sys/types.h>
#include <sys/stat.h>           /* stat(), S_ISDIR()                     */
#include <dirent.h>             /* opendir(), readdir(), closedir()      */
#include <unistd.h>             /* STDERR_FILENO                         */
#include <time.h>               /* ctime()                               */
#include <errno.h>

int        sys_log_fd = STDERR_FILENO;
char       *p_work_dir = NULL;
const char *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ watch_dir() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int           gotcha,
                 ret;
   off_t         filesize = 0;
   char          *ptr,
                 filename[MAX_FILENAME_LENGTH],
                 watch_dir[MAX_PATH_LENGTH];
   struct stat   stat_buf;
   struct dirent *dirp;
   DIR           *dp;

   if (argc == 2)
   {
      (void)my_strncpy(watch_dir, argv[1], MAX_PATH_LENGTH);
   }
   else
   {
      usage(argv[0]);
      exit(0);
   }

   if ((dp = opendir(watch_dir)) == NULL)
   {
      (void)fprintf(stderr, "ERROR   : Failed to opendir() %s : %s (%s %d)\n",
                    watch_dir, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   ptr = watch_dir + strlen(watch_dir);
   *ptr++ = '/';

   (void)printf("               File name                | File size |        File date\n");
   (void)printf("----------------------------------------+-----------+-------------------------\n");
   for (;;)
   {
      gotcha = 0;
      while ((dirp = readdir(dp)) != NULL)
      {
         if ((my_strcmp(dirp->d_name, ".") == 0) ||
             (my_strcmp(dirp->d_name, "..") == 0))
         {
            continue;
         }

         (void)strcpy(ptr, dirp->d_name);
         if (stat(watch_dir, &stat_buf) < 0)
         {
            (void)fprintf(stderr, "WARNING : Failed to stat() %s : %s (%s %d)\n",
                          watch_dir, strerror(errno), __FILE__, __LINE__);
            continue;
         }

         /* Make sure it's NOT a directory. */
         if (S_ISDIR(stat_buf.st_mode) == 0)
         {
            if (((ret = my_strcmp(dirp->d_name, filename)) != 0) ||
                ((ret == 0) && (filesize != stat_buf.st_size)))
            {
               (void)printf("%-39s |%10d | %s",
                            dirp->d_name, (int)stat_buf.st_size,
                            ctime(&stat_buf.st_mtime));
               (void)strcpy(filename, dirp->d_name);
               filesize = stat_buf.st_size;
               gotcha = 1;
            }
         }
      }

      rewinddir(dp);

      if (gotcha)
      {
         (void)printf("----------------------------------------+-----------+-------------------------\n");
      }

      (void)my_usleep(10000L);
   }

   exit(0);
}


/*+++++++++++++++++++++++++++++ usage() +++++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{
   (void)fprintf(stderr, "USAGE : %s directory\n", progname);
   return;
}
