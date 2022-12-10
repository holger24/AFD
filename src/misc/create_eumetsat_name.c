/*
 *  create_eumetsat_name.c - Part of AFD, an automatic file distribution
 *                           program.
 *  Copyright (c) 1999 - 2022 Deutscher Wetterdienst (DWD),
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

DESCR__S_M3
/*
 ** NAME
 **   create_eumetsat_name - generates the file names for EUMETSAT
 **
 ** SYNOPSIS
 **
 ** DESCRIPTION
 **   This program puts in a WMO bulletin header into the given file
 **   and creates a file with the name as required by EUMETSAT. The
 **   bulletin header is created from the ECMWF filename.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.03.1999 H.Kiehl Created
 **   13.12.2001 H.Kiehl Adopted to the new file names from ECMWF
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>         /* struct tm                               */
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"

/* Global variables */
int sys_log_fd = STDERR_FILENO;

#define HUNK_MAX 20480
#define _WITH_FIXED_NNN
#ifdef _WITH_FIXED_NNN
#define WMO_HEADER_OFFSET 10
#else
#define WMO_HEADER_OFFSET 4
#endif


/*$$$$$$$$$$$$$$$$$$$$$$$ create_eumetsat_name() $$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int          diff_time,
                ver_time = 0;
   time_t       base_time;
   char         newname[70];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif
   struct tm    *p_tm;

   if ((argc != 2) && (argc != 3))
   {
      (void)fprintf(stderr,
                    _("Usage: %s <file name> [<rename rule>]\n"), argv[0]);
      exit(1);
   }
   if (strlen(argv[1]) < 19)
   {
      (void)fprintf(stderr, _("Filename to short.\n"));
      exit(1);
   }

#ifdef HAVE_STATX
   if (statx(0, argv[1], AT_STATX_SYNC_AS_STAT, STATX_MTIME, &stat_buf) == -1)
#else
   if (stat(argv[1], &stat_buf) == -1)
#endif
   {
      (void)fprintf(stderr,
#ifdef HAVE_STATX
                    _("Failed to statx() `%s' : %s\n"),
#else
                    _("Failed to stat() `%s' : %s\n"),
#endif
                    argv[1], strerror(errno));
      exit(1);
   }

   /* Get diff_time in hours from validate date and base date. */
#ifdef HAVE_STATX
   p_tm = gmtime(&stat_buf.stx_mtime.tv_sec);
#else
   p_tm = gmtime(&stat_buf.st_mtime);
#endif
   p_tm->tm_sec = 0;
   p_tm->tm_min = ((argv[1][9] - '0') * 10) + (argv[1][10] - '0');
   p_tm->tm_hour = ((argv[1][7] - '0') * 10) + (argv[1][8] - '0');
   p_tm->tm_mday = ((argv[1][5] - '0') * 10) + (argv[1][6] - '0');
   p_tm->tm_mon = ((argv[1][3] - '0') * 10) + (argv[1][4] - '0') - 1;
   p_tm->tm_wday = 0;
   base_time = mktime(p_tm);
#ifdef HAVE_STATX
   p_tm = gmtime(&stat_buf.stx_mtime.tv_sec);
#else
   p_tm = gmtime(&stat_buf.st_mtime);
#endif
   p_tm->tm_sec = 0;
   p_tm->tm_min = ((argv[1][17] - '0') * 10) + (argv[1][18] - '0');
   p_tm->tm_hour = ((argv[1][15] - '0') * 10) + (argv[1][16] - '0');
   p_tm->tm_mday = ((argv[1][13] - '0') * 10) + (argv[1][14] - '0');
   p_tm->tm_mon = ((argv[1][11] - '0') * 10) + (argv[1][12] - '0') - 1;
   p_tm->tm_wday = 0;
   p_tm->tm_yday = 0;
   diff_time = abs(mktime(p_tm) - base_time) / 3600;

   if (diff_time <= 0)
   {
      if ((argv[1][7] == '0') && (argv[1][8] == '0'))
      {
         if (argv[1][19] != '2')
         {
            ver_time = 2;
         }
         else
         {
            ver_time = 8;
         }
      }
      else if ((argv[1][7] == '0') && (argv[1][8] == '6'))
           {
              if (argv[1][19] != '2')
              {
                 ver_time = 3;
              }
              else
              {
                 ver_time = 2;
              }
           }
      else if ((argv[1][7] == '1') && (argv[1][8] == '2'))
           {
              ver_time = 4;
           }
      else if ((argv[1][7] == '1') && (argv[1][8] == '8'))
           {
              if (argv[1][19] != '2')
              {
                 ver_time = 1;
              }
              else
              {
                 ver_time = 6;
              }
           }
   }
   else
   {
      if ((argv[1][7] == '0') && (argv[1][8] == '0'))
      {
         ver_time = diff_time / 3 + 8;
      }
      else /* ((argv[1][7] == '1') && (argv[1][8] == '2')) */
      {
         ver_time = diff_time / 6 + 4;
      }
   }

   /*
    * Check if it is neccessary to insert a WMO-header.
    */
   if (argc == 3)
   {
      int    from_fd,
             to_fd;
      size_t hunk,
             left;
      char   *buffer,
             wmo_header[MAX_FILENAME_LENGTH];

      /* File time */
#ifdef HAVE_STATX
      p_tm = gmtime(&stat_buf.stx_mtime.tv_sec);
#else
      p_tm = gmtime(&stat_buf.st_mtime);
#endif
      (void)strftime(&newname[49], 15, "%Y%m%d%H%M%S", p_tm);

      /* RTH_DADF_MET_FOR_ */
      newname[0] = 'R';
      newname[1] = 'T';
      newname[2] = 'H';
      newname[3] = '_';
      newname[4] = 'D';
      newname[5] = 'A';
      newname[6] = 'D';
      newname[7] = 'F';
      newname[8] = '_';
      newname[9] = 'M';
      newname[10] = 'E';
      newname[11] = 'T';
      newname[12] = '_';
      newname[13] = 'F';
      newname[14] = 'O';
      newname[15] = 'R';
      newname[16] = '_';

      /* Base date */
      newname[17] = newname[49];  /* Year */
      newname[18] = newname[50];  /* Year */
      newname[19] = newname[51];  /* Year */
      newname[20] = newname[52];  /* Year */
      newname[21] = argv[1][3];   /* Month */
      newname[22] = argv[1][4];   /* Month */
      newname[23] = argv[1][5];   /* Day */
      newname[24] = argv[1][6];   /* Day */
      newname[25] = argv[1][7];   /* Hour */
      newname[26] = argv[1][8];   /* Hour */
      newname[27] = argv[1][9];  /* Minute */
      newname[28] = argv[1][10]; /* Minute */
      newname[29] = '0';
      newname[30] = '0';
      newname[31] = 'Z';
      newname[32] = '_';

      /* Validate date */
      newname[33] = newname[49]; /* Year */
      newname[34] = newname[50]; /* Year */
      newname[35] = newname[51]; /* Year */
      newname[36] = newname[52]; /* Year */
      newname[37] = argv[1][11]; /* Month */
      newname[38] = argv[1][12]; /* Month */
      newname[39] = argv[1][13]; /* Day */
      newname[40] = argv[1][14]; /* Day */
      newname[41] = argv[1][15]; /* Hour */
      newname[42] = argv[1][16]; /* Hour */
      newname[43] = argv[1][17]; /* Minute */
      newname[44] = argv[1][18]; /* Minute */
      newname[45] = '0';
      newname[46] = '0';
      newname[47] = 'Z';
      newname[48] = '_';
      newname[63] = 'Z';
      newname[64] = '.';
      newname[65] = 'T';
      newname[66] = 'E';
      newname[67] = 'M';
      newname[68] = 'P';
      newname[69] = '\0';

      wmo_header[0] = 1;
      wmo_header[1] = '\015'; /* CR */
      wmo_header[2] = '\015'; /* CR */
      wmo_header[3] = '\012'; /* LF */
#ifdef _WITH_FIXED_NNN
      wmo_header[4] = '5';
      wmo_header[5] = '5';
      wmo_header[6] = '5';
      wmo_header[7] = '\015'; /* CR */
      wmo_header[8] = '\015'; /* CR */
      wmo_header[9] = '\012'; /* LF */
#endif /* _WITH_FIXED_NNN */
      wmo_header[WMO_HEADER_OFFSET] = 'H';
      wmo_header[WMO_HEADER_OFFSET + 1] = 'X';
      wmo_header[WMO_HEADER_OFFSET + 2] = 'X';
      wmo_header[WMO_HEADER_OFFSET + 3] = 'X';

      if (ver_time < 10)
      {
         wmo_header[WMO_HEADER_OFFSET + 4] = '0';
         wmo_header[WMO_HEADER_OFFSET + 5] = ver_time + '0';
      }
      else
      {
         wmo_header[WMO_HEADER_OFFSET + 4] = (ver_time / 10) + '0';
         wmo_header[WMO_HEADER_OFFSET + 5] = (ver_time % 10) + '0';
      }
      wmo_header[WMO_HEADER_OFFSET + 6] = ' ';
      wmo_header[WMO_HEADER_OFFSET + 7] = 'E';
      wmo_header[WMO_HEADER_OFFSET + 8] = 'C';
      wmo_header[WMO_HEADER_OFFSET + 9] = 'M';
      wmo_header[WMO_HEADER_OFFSET + 10] = 'F';
      wmo_header[WMO_HEADER_OFFSET + 11] = ' ';
      wmo_header[WMO_HEADER_OFFSET + 12] = argv[1][5];
      wmo_header[WMO_HEADER_OFFSET + 13] = argv[1][6];
      wmo_header[WMO_HEADER_OFFSET + 14] = argv[1][7];
      wmo_header[WMO_HEADER_OFFSET + 15] = argv[1][8];
      wmo_header[WMO_HEADER_OFFSET + 16] = '0';
      wmo_header[WMO_HEADER_OFFSET + 17] = '0';

      wmo_header[WMO_HEADER_OFFSET + 18] = '\015';
      wmo_header[WMO_HEADER_OFFSET + 19] = '\015';
      wmo_header[WMO_HEADER_OFFSET + 20] = '\012';

      /* Open source file */
      if ((from_fd = open(argv[1], O_RDONLY)) == -1)
      {
         (void)fprintf(stderr, _("Failed to open() `%s' : %s (%s %d)\n"),
                       argv[1], strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }

#ifdef HAVE_STATX
      if (statx(from_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(from_fd, &stat_buf) == -1)
#endif
      {
         (void)fprintf(stderr,
#ifdef HAVE_STATX
                       _("Failed to statx() `%s' : %s (%s %d)\n"),
#else
                       _("Failed to fstat() `%s' : %s (%s %d)\n"),
#endif
                       argv[1], strerror(errno), __FILE__, __LINE__);
         (void)close(from_fd);
         exit(INCORRECT);
      }

#ifdef HAVE_STATX
      left = hunk = stat_buf.stx_size;
#else
      left = hunk = stat_buf.st_size;
#endif

      if (hunk > HUNK_MAX)
      {
         hunk = HUNK_MAX;
      }

      if ((buffer = malloc(hunk)) == NULL)
      {
         (void)fprintf(stderr, _("Failed to allocate memory : %s (%s %d)\n"),
                       strerror(errno), __FILE__, __LINE__);
         (void)close(from_fd);
         exit(INCORRECT);
      }

      /* Open destination file */
      if ((to_fd = open(newname,
                        O_WRONLY | O_CREAT | O_TRUNC,
                        S_IRUSR|S_IWUSR)) == -1)
      {
         (void)fprintf(stderr, _("Failed to open() `%s' : %s (%s %d)\n"),
                       newname, strerror(errno), __FILE__, __LINE__);
         free(buffer);
         (void)close(from_fd);
         exit(INCORRECT);
      }
      if (write(to_fd, wmo_header, (WMO_HEADER_OFFSET + 21)) != (WMO_HEADER_OFFSET + 21))
      {
         (void)fprintf(stderr, _("Failed to write() `%s' : %s (%s %d)\n"),
                       newname, strerror(errno), __FILE__, __LINE__);
         free(buffer);
         (void)close(from_fd);
         (void)close(to_fd);
         exit(INCORRECT);
      }

      while (left > 0)
      {
         /* Try read file in one hunk */
         if (read(from_fd, buffer, hunk) != hunk)
         {
            (void)fprintf(stderr, _("Failed to read() `%s' : %s (%s %d)\n"),
                          argv[1], strerror(errno), __FILE__, __LINE__);
            free(buffer);
            (void)close(from_fd);
            (void)close(to_fd);
            exit(INCORRECT);
         }

         /* Try write file in one hunk */
         if (write(to_fd, buffer, hunk) != hunk)
         {
            (void)fprintf(stderr, _("Failed to write() `%s' : %s (%s %d)\n"),
                          newname, strerror(errno), __FILE__, __LINE__);
            free(buffer);
            (void)close(from_fd);
            (void)close(to_fd);
            exit(INCORRECT);
         }
         left -= hunk;
         if (left < hunk)
         {
            hunk = left;
         }
      }
      free(buffer);
      if (close(from_fd) == -1)
      {
         (void)fprintf(stderr, _("Failed to close() `%s' : %s (%s %d)\n"),
                       argv[1], strerror(errno), __FILE__, __LINE__);
      }
      wmo_header[0] = '\015'; /* CR */
      wmo_header[1] = '\015'; /* CR */
      wmo_header[2] = '\012'; /* LF */
      wmo_header[3] = 3;
      if (write(to_fd, wmo_header, 4) != 4)
      {
         (void)fprintf(stderr, _("Failed to write() `%s' : %s (%s %d)\n"),
                       newname, strerror(errno), __FILE__, __LINE__);
         free(buffer);
         (void)close(from_fd);
         (void)close(to_fd);
         exit(INCORRECT);
      }
      if (close(to_fd) == -1)
      {
         (void)fprintf(stderr, _("Failed to close() `%s' : %s (%s %d)\n"),
                       newname, strerror(errno), __FILE__, __LINE__);
      }
      if (remove(argv[1]) == -1)
      {
         (void)fprintf(stderr, _("Failed to remove() `%s' : %s (%s %d)\n"),
                       argv[1], strerror(errno), __FILE__, __LINE__);
      }
   }
   else
   {
      /* SMA_HXXX */
      newname[0] = 'S';
      newname[1] = 'M';
      newname[2] = 'A';
      newname[3] = '_';
      newname[4] = 'H';
      newname[5] = 'X';
      newname[6] = 'X';
      newname[7] = 'X';
      if (ver_time < 10)
      {
         newname[8] = '0';
         newname[9] = ver_time + '0';
      }
      else
      {
         newname[8] = (ver_time / 10) + '0';
         newname[9] = (ver_time % 10) + '0';
      }
      newname[10] = '_';
      newname[11] = 'E';
      newname[12] = 'C';
      newname[13] = 'M';
      newname[14] = 'F';
      newname[15] = '_';
      newname[16] = argv[1][5];
      newname[17] = argv[1][6];
      newname[18] = argv[1][7];
      newname[19] = argv[1][8];
      newname[20] = '0';
      newname[21] = '0';
      newname[22] = '\0';

      if (rename(argv[1], newname) == -1)
      {
         (void)fprintf(stderr, _("Failed to rename() `%s' to `%s' : %s\n"),
                       argv[1], newname, strerror(errno));
         exit(1);
      }
   }

   exit(SUCCESS);
}
