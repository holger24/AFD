/*
 *  count_burst.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 - 2022 Deutscher Wetterdienst (DWD),
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


/*
 ** NAME
 **   count_burst - counts the number of bursts in the given log file
 **
 ** SYNOPSIS
 **   count_burst <log file name>
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   0 on normal exit and 1 when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   09.01.2002 H.Kiehl Created
 **
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

/* Local function prototype. */
static char *posi(char *, char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int          fd;
   unsigned int burst_counter = 0;
   char         *file_buf,
                *ptr,
                str_num[25];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   if (argc != 2)
   {
      (void)fprintf(stderr, "Usage: %s <log file name>\n", argv[0]);
      exit(1);
   }
   if ((fd = open(argv[1], O_RDWR)) == -1)
   {
      (void)fprintf(stderr, "Failed to open() %s : %s\n",
                    argv[1], strerror(errno));
      exit(1);
   }
#ifdef HAVE_STATX
   if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(fd, &stat_buf) == -1)
#endif
   {
      (void)fprintf(stderr, "Failed to access %s : %s\n",
                    argv[1], strerror(errno));
      exit(1);
   }
#ifdef HAVE_STATX
   if ((file_buf = malloc(stat_buf.stx_size + 1)) == NULL)
#else
   if ((file_buf = malloc(stat_buf.st_size + 1)) == NULL)
#endif
   {
      (void)fprintf(stderr, "malloc() error : %s\n", strerror(errno));
      exit(1);
   }
#ifdef HAVE_STATX
   if (read(fd, file_buf, stat_buf.stx_size) != stat_buf.stx_size)
#else
   if (read(fd, file_buf, stat_buf.st_size) != stat_buf.st_size)
#endif
   {
      (void)fprintf(stderr, "Failed to read() %s : %s\n",
                    argv[1], strerror(errno));
      exit(1);
   }
   if (close(fd) == -1)
   {
      (void)fprintf(stderr, "Failed to close() %s : %s\n",
                    argv[1], strerror(errno));
   }
#ifdef HAVE_STATX
   file_buf[stat_buf.stx_size] = '\0';
#else
   file_buf[stat_buf.st_size] = '\0';
#endif
   ptr = file_buf;
   while ((ptr = posi(ptr, "[BURST")) != NULL)
   {
      if (*ptr == '\n')
      {
         burst_counter++;
      }
      else if ((*ptr == '*') && (*(ptr + 1) == ' '))
           {
              int i = 0;

              ptr += 2;
              while ((isdigit(*ptr)) && (i < 24))
              {
                 str_num[i] = *ptr;
                 ptr++; i++;
              }
              str_num[i] = '\0';
              burst_counter += atoi(str_num);
           }
           else
           {
              (void)fprintf(stderr, "Whats this!?\n");
           }
   }
   (void)fprintf(stdout, "Number of bursts = %u\n", burst_counter);

   return(0);
}


/*################################ posi() ###############################*/
static char *
posi(char *search_text, char *search_string)
{
   int    hit = 0;
   size_t string_length;

   string_length = strlen(search_string);

   while (*search_text != '\0')
   {
      if (*(search_text++) == *(search_string++))
      {
         if (++hit == string_length)
         {
            return(++search_text);
         }
      }
      else
      {
         if ((hit == 1) &&
             (*(search_string - 2) == *(search_text - 1)))
         {
            search_string--;
         }
         else
         {
            search_string -= hit + 1;
            hit = 0;
         }
      }
   }

   return(NULL); /* Found nothing. */
}
