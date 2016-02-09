/*
 *  rdu.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 Holger Kiehl <Holger.Kiehl@dwd.de>
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
#include <string.h>               /* strcmp(), strerror()               */
#include <stdlib.h>               /* exit()                             */
#include <sys/types.h>
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
   struct stat    statbuf;
   long           byte_size = 0L;
   char           filename[1024];

   if ((p_dir = opendir(dirname)) == NULL)
   {
      (void)fprintf(stderr, "opendir error : %s", strerror(errno));
      return(0L);
   }

   while ((p_struct_dir = readdir(p_dir)) != NULL)
   {
      if (strcmp(p_struct_dir->d_name, "..") == 0)
         continue;

      (void)sprintf(filename, "%s/%s", dirname, p_struct_dir->d_name);

      if (lstat(filename, &statbuf) != 0)
      {
         (void)fprintf(stderr, "lstat error (%s) : %s", filename, strerror(errno));
         continue;
      }

      if (S_ISDIR(statbuf.st_mode))
      {
         if (strcmp(p_struct_dir->d_name, ".") != 0)
            byte_size += rdu(filename);

         continue;
      }

      byte_size += statbuf.st_size;
   }

   closedir(p_dir);

   if (byte_size)
      (void)printf("%ld\t%s\n", byte_size/1024, dirname);

   return(byte_size);
}
