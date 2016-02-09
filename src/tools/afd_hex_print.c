/*
 *  afd_hex_print.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 - 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   afd_hex_prints - writes the content of a file in hex to stdout
 **
 ** SYNOPSIS
 **   afd_hex_print <file_name>
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   On error -1 (255) is returned, otherwise 0.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   07.04.2007 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                    /* fprintf()                       */
#include <string.h>                   /* strerror()                      */
#include <stdlib.h>                   /* malloc()                        */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>                   /* read(), write()                 */
#include <errno.h>

#define MAX_HUNK       4096
#define ASCII_OFFSET   54
#define HEADER_LENGTH  9
#define CHARS_PER_LINE 16

/* Local function prototypes. */
static void hex_print(char *, char *, int, int *),
            usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$ afd_hex_print() $$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int  bytes_read,
        fd,
        line_counter;
   char *buffer,
        *wbuf;

   if (argc < 2)
   {
      (void)usage(argv[0]);
      exit(INCORRECT);
   }

   if ((fd = open(argv[1], O_RDONLY)) == -1)
   {
      (void)fprintf(stderr,
                    _("Failed to open() `%s' for reading : %s (%s %d)\n"),
                    argv[1], strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (((buffer = malloc(MAX_HUNK + 1)) == NULL) ||
       ((wbuf = malloc(HEADER_LENGTH + ASCII_OFFSET + CHARS_PER_LINE + 1)) == NULL))
   {
      (void)fprintf(stderr, _("malloc() error : %s (%s %d)\n"),
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   line_counter = 0;
   do
   {
      if ((bytes_read = read(fd, buffer, MAX_HUNK)) == -1)
      {
         (void)fprintf(stderr,
                       _("Failed to read() %d bytes from `%s' : %s (%s %d)\n"),
                       MAX_HUNK, argv[1], strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      if (bytes_read > 0)
      {
         hex_print(wbuf, buffer, bytes_read, &line_counter);
      }
   } while (bytes_read == MAX_HUNK);

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++ hex_print() ++++++++++++++++++++++++++++++*/
static void
hex_print(char *wbuf, char *buffer, int length, int *line_counter)
{
   int  i,
        line_length = 0,
        offset,
        wpos = HEADER_LENGTH;
   char *hex = "0123456789ABCDEF";

   for (i = 0; i < length; i++)
   {
      if ((i % CHARS_PER_LINE) == 0)
      {
         if (line_length > 0)
         {
            (void)sprintf(wbuf, "%0*x", HEADER_LENGTH - 1, *line_counter * CHARS_PER_LINE);
            wbuf[HEADER_LENGTH - 1] = ' ';
            offset = HEADER_LENGTH + ASCII_OFFSET + line_length;
            wbuf[HEADER_LENGTH + ASCII_OFFSET - 1] = ' ';
            wbuf[offset] = '\n';
            if (write(STDOUT_FILENO, wbuf, (offset + 1)) != (offset + 1))
            {
               (void)fprintf(stderr,
                             _("Failed to write() %d bytes : %s (%s %d)\n"),
                             offset + 1, strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
            (*line_counter)++;
            wpos = HEADER_LENGTH;
            line_length = 0;
         }
      }
      else
      {
         if ((i % 4) == 0)
         {
            wbuf[wpos] = '|'; wbuf[wpos + 1] = ' ';
            wpos += 2;
         }
      }
      if ((unsigned char)buffer[i] > 15)
      {
         wbuf[wpos] = hex[((unsigned char)buffer[i]) >> 4];
         wbuf[wpos + 1] = hex[((unsigned char)buffer[i]) & 0x0F];
      }
      else
      {
         wbuf[wpos] = '0';
         wbuf[wpos + 1] = hex[(unsigned char)buffer[i]];
      }
      wbuf[wpos + 2] = ' ';
      wpos += 3;
      if (((unsigned char)buffer[i] < 32) || ((unsigned char)buffer[i] > 126))
      {
         wbuf[HEADER_LENGTH + ASCII_OFFSET + line_length] = '.';
      }
      else
      {
         wbuf[HEADER_LENGTH + ASCII_OFFSET + line_length] = buffer[i];
      }
      line_length++;
   }
   if (line_length > 0)
   {
      (void)sprintf(wbuf, "%0*x", HEADER_LENGTH - 1, *line_counter * CHARS_PER_LINE);
      wbuf[HEADER_LENGTH - 1] = ' ';
      for (i = line_length; i < CHARS_PER_LINE; i++)
      {
         if ((i % 4) == 0)
         {
            wbuf[wpos] = '|'; wbuf[wpos + 1] = ' ';
            wpos += 2;
         }
         wbuf[wpos] = ' '; wbuf[wpos + 1] = ' '; wbuf[wpos + 2] = ' ';
         wpos += 3;
      }
      offset = HEADER_LENGTH + ASCII_OFFSET + line_length;
      wbuf[HEADER_LENGTH + ASCII_OFFSET - 1] = ' ';
      wbuf[offset] = '\n';
      if (write(STDOUT_FILENO, wbuf, (offset + 1)) != (offset + 1))
      {
         (void)fprintf(stderr,
                       _("Failed to write() %d bytes : %s (%s %d)\n"),
                       offset + 1, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      (*line_counter)++;
   }
   return;
}


/*+++++++++++++++++++++++++++++++ usage() +++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{
   (void)fprintf(stderr, _("Usage: %s <file name>\n"), progname);
   return;
}
