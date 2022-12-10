/*
 *  afdconvert.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2009 - 2022 Deutscher Wetterdienst (DWD),
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

/*
 ** NAME
 **   afdconvert - converts a file from one format to another
 **
 ** SYNOPSIS
 **   afdconvert <format> <file name to convert>
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
 **   21.07.2009 H.Kiehl Created
 **
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>                      /* Definition of AT_* constants */
#endif
#include <sys/stat.h>
#include <unistd.h>                      /* STDERR_FILENO                */
#include <errno.h>
#include "amgdefs.h"

/* Global variables. */
int        production_log_fd = STDERR_FILENO,
           receive_log_fd = STDERR_FILENO,
           sys_log_fd = STDERR_FILENO;
char       *p_work_dir;
const char *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int          nnn_length = 0,
                type;
   off_t        file_size;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   if (argc != 3)
   {
      (void)fprintf(stderr,
                    "Usage: %s <format> <file name to convert>\n", argv[0]);
      exit(1);
   }
   /* sohetx */
   if ((argv[1][0] == 's') && (argv[1][1] == 'o') &&
       (argv[1][2] == 'h') && (argv[1][3] == 'e') &&
       (argv[1][3] == 't') && (argv[1][4] == 'x'))
   {
      type = SOHETX;
      if (argv[1][5] == '+')
      {
         int length = 6;

         while (argv[1][length] == 'n')
         {
            length++; nnn_length++;
         }
      }
   }
        /* wmo */
   else if ((argv[1][0] == 'w') && (argv[1][1] == 'm') &&
            (argv[1][2] == 'o'))
        {
           type = ONLY_WMO;
           if (argv[1][3] == '+')
           {
              int length = 4;

              while (argv[1][length] == 'n')
              {
                 length++; nnn_length++;
              }
           }
        }
        /* sohetxwmo */
   else if ((argv[1][0] == 's') && (argv[1][1] == 'o') &&
            (argv[1][2] == 'h') && (argv[1][3] == 'e') &&
            (argv[1][3] == 't') && (argv[1][4] == 'x') &&
            (argv[1][5] == 'w') && (argv[1][6] == 'm') &&
            (argv[1][7] == 'o'))
        {
           type = SOHETXWMO;
           if (argv[1][8] == '+')
           {
              int length = 9;

              while (argv[1][length] == 'n')
              {
                 length++; nnn_length++;
              }
           }
        }
        /* sohetx2wmo0 */
   else if ((argv[1][0] == 's') && (argv[1][1] == 'o') &&
            (argv[1][2] == 'h') && (argv[1][3] == 'e') &&
            (argv[1][3] == 't') && (argv[1][4] == 'x') &&
            (argv[1][5] == '2') && (argv[1][6] == 'w') &&
            (argv[1][7] == 'm') && (argv[1][8] == 'o') &&
            (argv[1][9] == '0'))
        {
           type = SOHETX2WMO0;
           if (argv[1][10] == '+')
           {
              int length = 11;

              while (argv[1][length] == 'n')
              {
                 length++; nnn_length++;
              }
           }
        }
        /* sohetx2wmo1 */
   else if ((argv[1][0] == 's') && (argv[1][1] == 'o') &&
            (argv[1][2] == 'h') && (argv[1][3] == 'e') &&
            (argv[1][3] == 't') && (argv[1][4] == 'x') &&
            (argv[1][5] == '2') && (argv[1][6] == 'w') &&
            (argv[1][7] == 'm') && (argv[1][8] == 'o') &&
            (argv[1][9] == '1'))
        {
           type = SOHETX2WMO1;
           if (argv[1][10] == '+')
           {
              int length = 11;

              while (argv[1][length] == 'n')
              {
                 length++; nnn_length++;
              }
           }
        }
        /* mrz2wmo */
   else if ((argv[1][0] == 'm') && (argv[1][1] == 'r') &&
            (argv[1][2] == 'z') && (argv[1][3] == '2') &&
            (argv[1][3] == 'w') && (argv[1][4] == 'm') &&
            (argv[1][5] == 'o') && (argv[1][6] == '\0'))
        {
           type = MRZ2WMO;
        }
        /* unix2dos */
   else if ((argv[1][0] == 'u') && (argv[1][1] == 'n') &&
            (argv[1][2] == 'i') && (argv[1][3] == 'x') &&
            (argv[1][3] == '2') && (argv[1][4] == 'd') &&
            (argv[1][5] == 'o') && (argv[1][6] == 's') &&
            (argv[1][7] == '\0'))
        {
           type = UNIX2DOS;
        }
        /* dos2unix */
   else if ((argv[1][0] == 'd') && (argv[1][1] == 'o') &&
            (argv[1][2] == 's') && (argv[1][3] == '2') &&
            (argv[1][3] == 'u') && (argv[1][4] == 'n') &&
            (argv[1][5] == 'i') && (argv[1][6] == 'x') &&
            (argv[1][7] == '\0'))
        {
           type = DOS2UNIX;
        }
        /* lf2crcrlf */
   else if ((argv[1][0] == 'l') && (argv[1][1] == 'f') &&
            (argv[1][2] == '2') && (argv[1][3] == 'c') &&
            (argv[1][3] == 'r') && (argv[1][4] == 'c') &&
            (argv[1][5] == 'r') && (argv[1][6] == 'l') &&
            (argv[1][7] == 'f') && (argv[1][8] == '\0'))
        {
           type = LF2CRCRLF;
        }
        /* crcrlf2lf */
   else if ((argv[1][0] == 'c') && (argv[1][1] == 'r') &&
            (argv[1][2] == 'c') && (argv[1][3] == 'r') &&
            (argv[1][3] == 'l') && (argv[1][4] == 'f') &&
            (argv[1][5] == '2') && (argv[1][6] == 'l') &&
            (argv[1][7] == 'f') && (argv[1][8] == '\0'))
        {
           type = CRCRLF2LF;
        }
        else
        {
           (void)fprintf(stderr, "Unknown convert format %s\n", argv[1]);
           (void)fprintf(stderr, "Known formats are: sohetx, wmo, sohetxwmo, sohetx2wmo1, sohetx2wmo0\n");
           (void)fprintf(stderr, "                   mrz2wmo, unix2dos, dos2unix, lf2crcrlf and crcrlf2lf\n");
           return(1);
        }

#ifdef HAVE_STATX
   if (statx(0, argv[2], AT_STATX_SYNC_AS_STAT, STATX_SIZE, &stat_buf) == -1)
#else
   if (stat(argv[2], &stat_buf) == -1)
#endif
   {
      (void)fprintf(stderr,
                    "Failed to access %s : %s\n", argv[2], strerror(errno));
      return(1);
   }
#ifdef HAVE_STATX
   file_size = stat_buf.stx_size;
#else
   file_size = stat_buf.st_size;
#endif
   if (convert(".", argv[2], type, nnn_length, 0U, 0U, &file_size) != SUCCESS)
   {
      (void)fprintf(stderr, "Failed to convert %s\n", argv[2]);
      return(1);
   }

   return(0);
}
