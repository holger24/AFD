/*
 *  print_file_size.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2004 - 2016 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   print_file_size - prints the file size to a given memory location
 **
 ** SYNOPSIS
 **   void print_file_size(char *buf, off_t file_size)
 **
 ** DESCRIPTION
 **   This function prints the given file_size in ascii decimal to buf.
 **   It assumes that buf is of at least 10 bytes long and will not
 **   use sprintf() for values less then 10.000.000.000. It will print
 **   from right to left and leading values will be initialized with
 **   a space sign.
 **
 ** RETURN VALUES
 **   Will print the given file_size to buf.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   06.06.2004 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include "ui_common_defs.h"


/*########################## print_file_size() ##########################*/
void
print_file_size(char *buf, off_t file_size)
{
   if (file_size < 10)
   {
      buf[0] = ' ';
      buf[1] = ' ';
      buf[2] = ' ';
      buf[3] = ' ';
      buf[4] = ' ';
      buf[5] = ' ';
      buf[6] = ' ';
      buf[7] = ' ';
      buf[8] = ' ';
      buf[9] = file_size + '0';
   }
   else if (file_size < 100)
        {
           buf[0] = ' ';
           buf[1] = ' ';
           buf[2] = ' ';
           buf[3] = ' ';
           buf[4] = ' ';
           buf[5] = ' ';
           buf[6] = ' ';
           buf[7] = ' ';
           buf[8] = (file_size / 10) + '0';
           buf[9] = (file_size % 10) + '0';
        }
   else if (file_size < 1000)
        {
           buf[0] = ' ';
           buf[1] = ' ';
           buf[2] = ' ';
           buf[3] = ' ';
           buf[4] = ' ';
           buf[5] = ' ';
           buf[6] = ' ';
           buf[7] = (file_size / 100) + '0';
           buf[8] = ((file_size / 10) % 10) + '0';
           buf[9] = (file_size % 10) + '0';
        }
   else if (file_size < 10000)
        {
           buf[0] = ' ';
           buf[1] = ' ';
           buf[2] = ' ';
           buf[3] = ' ';
           buf[4] = ' ';
           buf[5] = ' ';
           buf[6] = (file_size / 1000) + '0';
           buf[7] = ((file_size / 100) % 10) + '0';
           buf[8] = ((file_size / 10) % 10) + '0';
           buf[9] = (file_size % 10) + '0';
        }
   else if (file_size < 100000)
        {
           buf[0] = ' ';
           buf[1] = ' ';
           buf[2] = ' ';
           buf[3] = ' ';
           buf[4] = ' ';
           buf[5] = (file_size / 10000) + '0';
           buf[6] = ((file_size / 1000) % 10) + '0';
           buf[7] = ((file_size / 100) % 10) + '0';
           buf[8] = ((file_size / 10) % 10) + '0';
           buf[9] = (file_size % 10) + '0';
        }
   else if (file_size < 1000000)
        {
           buf[0] = ' ';
           buf[1] = ' ';
           buf[2] = ' ';
           buf[3] = ' ';
           buf[4] = (file_size / 100000) + '0';
           buf[5] = ((file_size / 10000) % 10) + '0';
           buf[6] = ((file_size / 1000) % 10) + '0';
           buf[7] = ((file_size / 100) % 10) + '0';
           buf[8] = ((file_size / 10) % 10) + '0';
           buf[9] = (file_size % 10) + '0';
        }
   else if (file_size < 10000000)
        {
           buf[0] = ' ';
           buf[1] = ' ';
           buf[2] = ' ';
           buf[3] = (file_size / 1000000) + '0';
           buf[4] = ((file_size / 100000) % 10) + '0';
           buf[5] = ((file_size / 10000) % 10) + '0';
           buf[6] = ((file_size / 1000) % 10) + '0';
           buf[7] = ((file_size / 100) % 10) + '0';
           buf[8] = ((file_size / 10) % 10) + '0';
           buf[9] = (file_size % 10) + '0';
        }
   else if (file_size < 100000000)
        {
           buf[0] = ' ';
           buf[1] = ' ';
           buf[2] = (file_size / 10000000) + '0';
           buf[3] = ((file_size / 1000000) % 10) + '0';
           buf[4] = ((file_size / 100000) % 10) + '0';
           buf[5] = ((file_size / 10000) % 10) + '0';
           buf[6] = ((file_size / 1000) % 10) + '0';
           buf[7] = ((file_size / 100) % 10) + '0';
           buf[8] = ((file_size / 10) % 10) + '0';
           buf[9] = (file_size % 10) + '0';
        }
   else if (file_size < 1000000000)
        {
           buf[0] = ' ';
           buf[1] = (file_size / 100000000) + '0';
           buf[2] = ((file_size / 10000000) % 10) + '0';
           buf[3] = ((file_size / 1000000) % 10) + '0';
           buf[4] = ((file_size / 100000) % 10) + '0';
           buf[5] = ((file_size / 10000) % 10) + '0';
           buf[6] = ((file_size / 1000) % 10) + '0';
           buf[7] = ((file_size / 100) % 10) + '0';
           buf[8] = ((file_size / 10) % 10) + '0';
           buf[9] = (file_size % 10) + '0';
        }
   else if (file_size < 10000000000LL)
        {
           buf[0] = (file_size / 1000000000) + '0';
           buf[1] = ((file_size / 100000000) % 10) + '0';
           buf[2] = ((file_size / 10000000) % 10) + '0';
           buf[3] = ((file_size / 1000000) % 10) + '0';
           buf[4] = ((file_size / 100000) % 10) + '0';
           buf[5] = ((file_size / 10000) % 10) + '0';
           buf[6] = ((file_size / 1000) % 10) + '0';
           buf[7] = ((file_size / 100) % 10) + '0';
           buf[8] = ((file_size / 10) % 10) + '0';
           buf[9] = (file_size % 10) + '0';
        }
        else
        {
           char tmp_buf[11];

           if (file_size < (TERABYTE - 1))
           {
              (void)snprintf(tmp_buf, 11, "%7.2f GB", (double)file_size / GIGABYTE);
           }
           else if (file_size < (PETABYTE - 1))
                {
                   (void)snprintf(tmp_buf, 11, "%7.2f TB", (double)file_size / TERABYTE);
                }
           else if (file_size < (EXABYTE - 1))
                {
                   (void)snprintf(tmp_buf, 11, "%7.2f PB", (double)file_size / PETABYTE);
                }
                else
                {
                   (void)snprintf(tmp_buf, 11, "%7.2f EB", (double)file_size / EXABYTE);
                }
           (void)memcpy(buf, tmp_buf, 10);
        }
   return;
}
