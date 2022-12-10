/*
 *  gen_file.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2004 - 2022 Deutscher Wetterdienst (DWD),
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
 **   gen_file - process that gnerates files at intervals
 **
 ** SYNOPSIS
 **   gen_file <no. of files> <size> <interval> <directory> <file name>
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   13.05.2004 H.Kiehl Created
 **   07.09.2008 H.Kiehl Write with blocksize suggested by filesystem.
 **
 */
DESCR__E_M1

#include <stdio.h>              /* fprintf(), printf(), stderr           */
#include <string.h>             /* strcpy(), strerror()                  */
#include <stdlib.h>             /* exit()                                */
#include <sys/types.h>
#include <sys/stat.h>           /* stat(), S_ISDIR()                     */
#include <unistd.h>             /* TDERR_FILENO                          */
#include <time.h>               /* time()                                */
#include <fcntl.h>
#include <errno.h>


/* Local function prototypes. */
static void usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ gen_file() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   unsigned int counter = 0;
   int          fd,
                i, j,
                loops,
                no_of_files,
                rest;
   long int     interval;
   off_t        filesize;
   time_t       current_time,
                *p_block;
   char         *block,
                *dot_ptr,
                *ptr,
                filename[MAX_FILENAME_LENGTH],
                dot_target_dir[MAX_PATH_LENGTH],
                str_number[MAX_INT_LENGTH],
                target_dir[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   if (argc == 6)
   {
      no_of_files = atoi(argv[1]);
      filesize = (off_t)str2offt(argv[2], NULL, 10);
      interval = atol(argv[3]);
      (void)strcpy(target_dir, argv[4]);
      (void)strcpy(dot_target_dir, argv[4]);
      (void)strcpy(filename, argv[5]);
   }
   else
   {
      usage(argv[0]);
      exit(0);
   }

   /*
    * Determine blocksize with which we should create the files.
    */
#ifdef HAVE_STATX
   if (statx(0, target_dir, AT_STATX_SYNC_AS_STAT, 0, &stat_buf) == -1)
#else
   if (stat(target_dir, &stat_buf) == -1)
#endif
   {
      (void)fprintf(stderr, "Failed to access target directory %s : %s\n",
                    target_dir, strerror(errno));
      exit(INCORRECT);
   }
#ifdef HAVE_STATX
   if ((block = malloc(stat_buf.stx_blksize)) == NULL)
#else
   if ((block = malloc(stat_buf.st_blksize)) == NULL)
#endif
   {
      (void)fprintf(stderr, "Failed to malloc() %d bytes of memory : %s\n",
#ifdef HAVE_STATX
                    (int)stat_buf.stx_blksize,
#else
                    (int)stat_buf.st_blksize,
#endif
                    strerror(errno));
      exit(INCORRECT);
   }

   ptr = target_dir + strlen(target_dir);
   *ptr++ = '/';
   (void)strcpy(ptr, filename);
   ptr += strlen(filename);
   *ptr++ = '-';
   dot_ptr = dot_target_dir + strlen(dot_target_dir);
   *dot_ptr++ = '/';
   *dot_ptr++ = '.';
   (void)strcpy(dot_ptr, filename);
   dot_ptr += strlen(filename);
   *dot_ptr++ = '-';
#ifdef HAVE_STATX
   loops = filesize / stat_buf.stx_blksize;
   rest = filesize % stat_buf.stx_blksize;
#else
   loops = filesize / stat_buf.st_blksize;
   rest = filesize % stat_buf.st_blksize;
#endif

   for (;;)
   {
      p_block = (time_t *)block;
      current_time = time(NULL);
#ifdef HAVE_STATX
      while (p_block < (time_t *)&block[stat_buf.stx_blksize - sizeof(time_t)])
#else
      while (p_block < (time_t *)&block[stat_buf.st_blksize - sizeof(time_t)])
#endif
      {
         *p_block = current_time;
         p_block++;
      }
      for (i = 0; i < no_of_files; i++)
      {
         (void)sprintf(str_number, "%d", counter);
         (void)strcpy(ptr, str_number);
         (void)strcpy(dot_ptr, str_number);
         if ((fd = open(dot_target_dir, (O_WRONLY | O_CREAT | O_TRUNC),
#ifdef GROUP_CAN_WRITE
                        (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP))) == -1)
#else
                        (S_IRUSR | S_IWUSR))) == -1)
#endif
         {
            (void)fprintf(stderr, "Failed top open() %s : %s\n",
                          dot_target_dir, strerror(errno));
            exit(INCORRECT);
         }
         for (j = 0; j < loops; j++)
         {
#ifdef HAVE_STATX
            if (write(fd, block, stat_buf.stx_blksize) != stat_buf.stx_blksize)
#else
            if (write(fd, block, stat_buf.st_blksize) != stat_buf.st_blksize)
#endif
            {
               (void)fprintf(stderr, "Failed to write() %d bytes : %s\n",
#ifdef HAVE_STATX
                             (int)stat_buf.stx_blksize,
#else
                             (int)stat_buf.st_blksize,
#endif
                             strerror(errno));
               exit(INCORRECT);
            }
         }
         if (rest)
         {
            if (write(fd, block, rest) != rest)
            {
               (void)fprintf(stderr, "Failed to write() %d bytes : %s\n",
                             rest, strerror(errno));
               exit(INCORRECT);
            }
         }
         if (close(fd) == -1)
         {
            (void)fprintf(stderr, "Failed to close() %s : %s\n",
                          dot_target_dir, strerror(errno));
         }
         if (rename(dot_target_dir, target_dir) == -1)
         {
            (void)fprintf(stderr, "Failed to rename() %s to %s : %s\n",
                          dot_target_dir, target_dir, strerror(errno));
         }
         counter++;
      }
      if (interval)
      {
         (void)sleep(interval);
      }
      else
      {
         break;
      }
   }

   exit(0);
}


/*+++++++++++++++++++++++++++++ usage() +++++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{
   (void)fprintf(stderr,
                 "Usage : %s <no. of files> <size> <interval> <directory> <file name>\n",
                 progname);
   return;
}
