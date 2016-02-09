/*
 *  todos.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2014 Deutscher Wetterdienst (DWD),
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
 **   todos - converts carriage return to carriage return line feed
 **
 ** SYNOPSIS
 **   todos <file name>
 **
 ** DESCRIPTION
 **   Converts all carriage return in <file name> to carriage return
 **   line feed in <file name>.tmp.
 **
 ** RETURN VALUES
 **   On success it will exit with 0 otherwise 1 will be returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   24.02.1998 H.Kiehl Created
 **
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>

/* Local function prototypes */
static void convert(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   if (argc != 2)
   {
      (void)fprintf(stderr, _("Usage: %s <file name>\n"), argv[0]);
      exit(1);
   }
   convert(argv[1]);

   exit(0);
}


/*++++++++++++++++++++++++++++++ convert() ++++++++++++++++++++++++++++++*/
static void
convert(char *source_file)
{
   int  tmp_char;
   char dest_file[MAX_PATH_LENGTH];
   FILE *rfp,
        *wfp;

   if ((rfp = fopen(source_file, "r")) == NULL)
   {
      (void)fprintf(stderr, _("Failed to fopen() `%s' : %s\n"),
                    source_file, strerror(errno));
      exit(1);
   }
   (void)snprintf(dest_file, MAX_PATH_LENGTH, "%s.tmp", source_file);
   if ((wfp = fopen(dest_file, "w+")) == NULL)
   {
      (void)fprintf(stderr, _("Failed to fopen() `%s' : %s\n"),
                    dest_file, strerror(errno));
      exit(1);
   }

   while ((tmp_char = fgetc(rfp)) != EOF)
   {
      if (tmp_char == 10)
      {
         tmp_char = 13;
         if (fputc(tmp_char, wfp) == EOF)
         {
            (void)fprintf(stderr, _("fputc() error : %s\n"), strerror(errno));
            exit(1);
         }
         tmp_char = 10;
         if (fputc(tmp_char, wfp) == EOF)
         {
            (void)fprintf(stderr, _("fputc() error : %s\n"), strerror(errno));
            exit(1);
         }
      }
      else
      {
         if (fputc(tmp_char, wfp) == EOF)
         {
            (void)fprintf(stderr, _("fputc() error : %s\n"), strerror(errno));
            exit(1);
         }
      }
   }

   if ((fclose(rfp) == EOF) || (fclose(wfp) == EOF))
   {
      (void)fprintf(stderr, _("fclose() error : %s\n"), strerror(errno));
      exit(1);
   }

   return;
}
