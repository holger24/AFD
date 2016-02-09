/*
 *  show_file_content.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   show_file_content - prints content of given file
 **
 ** SYNOPSIS
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   03.08.2008 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "aldadefs.h"


/*######################### show_file_content() #########################*/
void
show_file_content(FILE *output_fp, char *filename)
{
   FILE *input_fp;

   if ((input_fp = fopen(filename, "r")) != NULL)
   {
      char line[MAX_LINE_LENGTH + 1];

      while (fgets(line, MAX_LINE_LENGTH, input_fp) != NULL)
      {
         if (fputs(line, output_fp) == EOF)
         {
            (void)fprintf(stderr, "fputs() error: %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            break;
         }
      }
      if (fclose(input_fp) == EOF)
      {
         (void)fprintf(stderr, "fclose() error: %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
      }
   }
   else
   {
      (void)fprintf(stderr, "Failed to fopen() %s for reading : %s (%s %d)\n",
                    filename, strerror(errno), __FILE__, __LINE__);
   }

   return;
}
