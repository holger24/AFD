/*
 *  rdu.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
/************************************************************************/
/*                               rdu                                    */
/*                               ===                                    */
/*                                                                      */
/* Description : Calculates the size of a directory in kBytes.          */
/* Syntax      : rdu [directory 1 [directory 2] ...]                    */
/* Author      : H.Kiehl                                                */
/* Modules     : rdu.c; error.c                                         */
/* Changes     : NONE                                                   */
/* Version     : 1.0                                                    */
/************************************************************************/

#include <stdio.h>                /* printf()                           */
#include <string.h>               /* strerror()                         */
#include <stdlib.h>               /* exit()                             */
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>               /* Definition of AT_* constants       */
#endif
#include <sys/stat.h>             /* lstat(), S_ISDIR()                 */
#include <dirent.h>               /* opendir(), closedir(), readdir()   */
#include <errno.h>

static long rdu(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   if (argc == 1)
      rdu(".");
   else
      while (--argc)
         rdu(*++argv);

   exit(0);
}


/*++++++++++++++++++++++++++++++ rdu() ++++++++++++++++++++++++++++++++++*/
static long
rdu(char *dirname)
{
   DIR            *p_dir;
   struct dirent  *p_struct_dir;
#ifdef HAVE_STATX
   struct statx   stat_buf;
#else
   struct stat    stat_buf;
#endif
   long           byte_size = 0L;
   char           filename[1024];

   if ((p_dir = opendir(dirname)) == NULL)
   {
      (void)fprintf(stderr, "opendir error : %s", strerror(errno));
      return(0L);
   }

   while ((p_struct_dir = readdir(p_dir)) != NULL)
   {
      if (my_strcmp(p_struct_dir->d_name, "..") == 0)
      {
         continue;
      }

      (void)sprintf(filename, "%s/%s", dirname, p_struct_dir->d_name);

#ifdef HAVE_STATX
      if (statx(filename, AT_STATX_SYNC_AS_STAT | AT_SYMLINK_NOFOLLOW,
                STATX_SIZE | STATX_MODE, &stat_buf) != 0)
#else
      if (lstat(filename, &stat_buf) != 0)
#endif
      {
#ifdef HAVE_STATX
         (void)fprintf(stderr, "statx() error (%s) : %s",
                       filename, strerror(errno));
#else
         (void)fprintf(stderr, "lstat() error (%s) : %s",
                       filename, strerror(errno));
#endif
         continue;
      }

#ifdef HAVE_STATX
      if (S_ISDIR(stat_buf.stx_mode))
#else
      if (S_ISDIR(stat_buf.st_mode))
#endif
      {
         if (my_strcmp(p_struct_dir->d_name, ".") != 0)
         {
            byte_size += rdu(filename);
         }

         continue;
      }

#ifdef HAVE_STATX
      byte_size += stat_buf.stx_size;
#else
      byte_size += stat_buf.st_size;
#endif
   }

   closedir(p_dir);

   if (byte_size)
   {
      (void)printf("%ld\t%s\n", byte_size/1024, dirname);
   }

   return(byte_size);
}
