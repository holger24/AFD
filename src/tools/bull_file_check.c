/*
 *  bull_file_check.c - Part of AFD, an automatic file distribution program.
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

#include "afddefs.h"

DESCR__S_M1
/*
 ** NAME
 **   bull_file_check - checks files with bulletins for correct
 **                     length and start/stop sequence
 **
 ** SYNOPSIS
 **   bull_file_check file-name
 **
 ** DESCRIPTION
 **   This program checks the four byte length indicator in files
 **   with more then one bulletin.
 **
 ** RETURN VALUES
 **   0
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.06.1996 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int           i,
                 fd,
                 size;
   unsigned char lbyte,
                 hbyte;
   char          *ptr,
                 *ptr_start,
                 *file_buf,
                 filename[256],
                 bullname[256];
#ifdef HAVE_STATX
   struct statx  stat_buf;
#else
   struct stat   stat_buf;
#endif

   if (argc != 2)
   {
      (void)fprintf(stderr, "usage : %s filename\n", argv[0]);
      exit(1);
   }
   else
   {
      (void)strcpy(filename, argv[1]);
   }

#ifdef HAVE_STATX
   if (statx(0, filename, AT_STATX_SYNC_AS_STAT, STATX_SIZE, &stat_buf) == -1)
#else
   if (stat(filename, &stat_buf) == -1)
#endif
   {
      (void)fprintf(stderr, "ERROR   : Failed to access %s : %s (%s %d)\n",
                    filename, strerror(errno), __FILE__, __LINE__);
      exit(1);
   }

#ifdef HAVE_STATX
   if ((file_buf = calloc(stat_buf.stx_size, sizeof(char))) == NULL)
#else
   if ((file_buf = calloc(stat_buf.st_size, sizeof(char))) == NULL)
#endif
   {
      (void)fprintf(stderr, "ERROR   : Failed to calloc() : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(1);
   }

   if ((fd = open(filename, O_RDONLY)) < 0)
   {
      (void)fprintf(stderr, "ERROR   : Failed to open() %s : %s (%s %d)\n",
                    filename, strerror(errno), __FILE__, __LINE__);
      exit(1);
   }

#ifdef HAVE_STATX
   if (read(fd, file_buf, stat_buf.stx_size) < stat_buf.stx_size)
#else
   if (read(fd, file_buf, stat_buf.st_size) < stat_buf.st_size)
#endif
   {
      (void)fprintf(stderr, "ERROR   : Failed to read() : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(1);
   }

   (void)close(fd);

   ptr = file_buf;

   while (((signed char)*ptr != EOF))
   {
      ptr_start = ptr + 2;
      lbyte = *ptr++;
      hbyte = *ptr++;
      size = (hbyte * 256) + lbyte;
      if (*ptr++ != 1)
      {
         (void)fprintf(stdout, "No SOH!\n");
         exit(0);
      }
      if (*ptr++ != 13)
      {
         (void)fprintf(stdout, "No first CR!\n");
         exit(0);
      }
      if (*ptr++ != 13)
      {
         (void)fprintf(stdout, "No second CR!\n");
         exit(0);
      }
      if (*ptr++ != 10)
      {
         (void)fprintf(stdout, "No LF!\n");
         exit(0);
      }

      ptr += 3;

      if (*ptr++ != 13)
      {
         (void)fprintf(stdout, "No first CR (before bulletin header)!\n");
         exit(0);
      }
      if (*ptr++ != 13)
      {
         (void)fprintf(stdout, "No second CR (before bulletin header)!\n");
         exit(0);
      }
      if (*ptr++ != 10)
      {
         (void)fprintf(stdout, "No LF (before bulletin header)!\n");
         exit(0);
      }

      /* Get bulletin header. */
      i = 0;
      while ((*ptr != 13) && (*ptr != '\0'))
      {
         bullname[i] = *ptr;
         i++; ptr++;
      }
      bullname[i] = '\0';
      (void)fprintf(stdout, "bullname = %s  size = %d\n", bullname, size);

      if (*ptr++ != 13)
      {
         (void)fprintf(stdout, "No first CR (after bulletin header)!\n");
         exit(0);
      }
      if (*ptr++ != 13)
      {
         (void)fprintf(stdout, "No second CR (after bulletin header)!\n");
         exit(0);
      }
      if (*ptr++ != 10)
      {
         (void)fprintf(stdout, "No LF (after bulletin header)!\n");
         exit(0);
      }

      if (size % 2)
      {
         ptr = ptr_start + size - 4;
      }
      else
      {
         ptr = ptr_start + size - 4;
      }

      if (*ptr++ != 13)
      {
         (void)fprintf(stdout, "No first CR (%d) (at end)!\n", *(ptr - 1));
         exit(0);
      }
      if (*ptr++ != 13)
      {
         (void)fprintf(stdout, "No second CR (at end)!\n");
         exit(0);
      }
      if (*ptr++ != 10)
      {
         (void)fprintf(stdout, "No LF (at end)!\n");
         exit(0);
      }
      if (*ptr++ != 3)
      {
         (void)fprintf(stdout, "No ETX (at end)!\n");
         exit(0);
      }
      if (size % 2)
      {
         if (*ptr++ != 0)
         {
            (void)fprintf(stdout, "No fill byte at end!\n");
            exit(0);
         }
      }
   }

   exit(0);
}
