/*
 *  fax2gts.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2006 - 2022 Deutscher Wetterdienst (DWD),
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
 **   fax2gts - changes FAX files to GTS T4 files
 **
 ** SYNOPSIS
 **   off_t fax2gts(char *path, char *filename, int fax_format)
 **            path       - path where the FAX file can be found
 **            filename   - file name of the FAX file
 **            fax_format - allows to specify different DWD specific
 **                         fax formats
 **
 ** DESCRIPTION
 **   Inserts a WMO-bulletin header. The following fax formats can
 **   be set with fax_format:
 **        1 - DFAX1062
 **        2 - DFAX1064
 **        3 - DFAX1074
 **        4 - DFAX1084
 **        5 - DFAX1099
 **   Any other number will always result in DFAX1064.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to convert the file. Otherwise
 **   the size of the converted file is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   15.02.2006 H.Kiehl Created
 **   18.08.2008 H.Kiehl Added option to specify fax format.
 **   19.01.2010 H.Kiehl Made it a standalone program.
 **
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>                /* malloc(), free(), atoi(), exit()   */
#include <sys/stat.h>              /* stat()                             */
#include <sys/types.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define SUCCESS         0
#define INCORRECT       -1
#define MAX_PATH_LENGTH 1024
#define FILE_MODE       (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int ret = SUCCESS;

   if ((argc == 3) || (argc == 4))
   {
      int  fax_format,
           to_fd;
      char dest_file_name[MAX_PATH_LENGTH],
           *filename,
           *ttaaii;

      filename = argv[1];
      ttaaii = argv[2];
      if ((to_fd = strlen(ttaaii)) != 18)
      {
         (void)fprintf(stderr,
                       "WMO header must be at least 18 bytes long and must be a WMO header. (%s %d)\n",
                       __FILE__, __LINE__);
         exit(INCORRECT);
      }
      if (argc == 4)
      {
         fax_format = atoi(argv[3]);
         switch (fax_format)
         {
            case 1 :
            case 2 :
            case 3 :
            case 4 :
            case 5 :
               /* Correct. */
               break;

            default:
               (void)fprintf(stderr, "Wrong FAX format number %d (%s %d)\n",
                             fax_format, __FILE__, __LINE__);
               exit(INCORRECT);
         }
      }
      else
      {
         fax_format = 2;
      }

      /* Open destination file. */
      (void)sprintf(dest_file_name, ".%s", filename);
      if ((to_fd = open(dest_file_name,
                        (O_WRONLY | O_CREAT | O_TRUNC), FILE_MODE)) < 0)
      {
         (void)fprintf(stderr, "Failed to open() `%s' : %s (%s %d)\n",
                       dest_file_name, strerror(errno), __FILE__, __LINE__);
         (void)unlink(dest_file_name);
      }
      else
      {
         char fullname[MAX_PATH_LENGTH],
              wmo_header[33];

         /* Write header. */
         wmo_header[0] = 1;
         wmo_header[1] = '\015'; /* CR */
         wmo_header[2] = '\015'; /* CR */
         wmo_header[3] = '\012'; /* LF */
         (void)memcpy(&wmo_header[4], ttaaii, 18);
         wmo_header[10] = wmo_header[15] = ' ';
         wmo_header[22] = '\015'; /* CR */
         wmo_header[23] = '\015'; /* CR */
         wmo_header[24] = '\012'; /* LF */
         wmo_header[25] = 'D';
         wmo_header[26] = 'F';
         wmo_header[27] = 'A';
         wmo_header[28] = 'X';
         wmo_header[29] = '1';
         wmo_header[30] = '0';
         switch (fax_format)
         {
            case 1 : /* DFAX1062 */
               wmo_header[31] = '6';
               wmo_header[32] = '2';
               break;

            case 3 : /* DFAX1074 */
               wmo_header[31] = '7';
               wmo_header[32] = '4';
               break;

            case 4 : /* DFAX1084 */
               wmo_header[31] = '8';
               wmo_header[32] = '4';
               break;

            case 5 : /* DFAX1099 */
               wmo_header[31] = '9';
               wmo_header[32] = '9';
               break;

            default: /* DFAX1064 */
               wmo_header[31] = '6';
               wmo_header[32] = '4';
               break;
         }
         if (write(to_fd, wmo_header, 33) != 33)
         {
            (void)fprintf(stderr,
                          "Failed to write() WMO header to `%s' : %s (%s %d)\n",
                          dest_file_name, strerror(errno), __FILE__, __LINE__);
            (void)unlink(dest_file_name);
            ret = INCORRECT;
         }
         else
         {
            int from_fd;

            /* Open source file. */
            (void)sprintf(fullname, "%s", filename);
            if ((from_fd = open(fullname, O_RDONLY, 0)) < 0)
            {
               (void)fprintf(stderr, "Failed to open() `%s' : %s (%s %d)\n",
                             fullname, strerror(errno), __FILE__, __LINE__);
               ret = INCORRECT;
            }
            else
            {
#ifdef HAVE_STATX
               struct statx stat_buf;
#else
               struct stat stat_buf;
#endif

#ifdef HAVE_STATX
               if (statx(from_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                         STATX_SIZE, &stat_buf) == -1)
#else
               if (fstat(from_fd, &stat_buf) == -1)
#endif
               {
                  (void)fprintf(stderr, "Failed to access `%s' : %s (%s %d)\n",
                                fullname, strerror(errno), __FILE__, __LINE__);
                  ret = INCORRECT;
               }
               else
               {
#ifdef HAVE_STATX
                  if (stat_buf.stx_size > 0)
#else
                  if (stat_buf.st_size > 0)
#endif
                  {
                     char *buffer;

#ifdef HAVE_STATX
                     if ((buffer = malloc(stat_buf.stx_blksize)) == NULL)
#else
                     if ((buffer = malloc(stat_buf.st_blksize)) == NULL)
#endif
                     {
                        (void)fprintf(stderr,
                                      "Failed to malloc() %d bytes : %s (%s %d)\n",
#ifdef HAVE_STATX
                                      stat_buf.stx_blksize, strerror(errno),
#else
                                      stat_buf.st_blksize, strerror(errno),
#endif
                                      __FILE__, __LINE__);
                        ret = INCORRECT;
                     }
                     else
                     {
                        int bytes_buffered;

                        do
                        {
#ifdef HAVE_STATX
                           if ((bytes_buffered = read(from_fd, buffer,
                                                      stat_buf.stx_blksize)) == -1)
#else
                           if ((bytes_buffered = read(from_fd, buffer,
                                                      stat_buf.st_blksize)) == -1)
#endif
                           {
                              (void)fprintf(stderr,
                                            "Failed to read() %d bytes from `%s' : %s (%s %d)\n",
#ifdef HAVE_STATX
                                            stat_buf.stx_blksize, fullname,
#else
                                            stat_buf.st_blksize, fullname,
#endif
                                            strerror(errno),
                                            __FILE__, __LINE__);
                              ret = INCORRECT;
                              break;
                           }
                           if (bytes_buffered > 0)
                           {
                              if (write(to_fd, buffer, bytes_buffered) != bytes_buffered)
                              {
                                 (void)fprintf(stderr,
                                               "Failed to write() %d bytes to `%s' : %s (%s %d)\n",
                                               bytes_buffered, dest_file_name,
                                               strerror(errno), __FILE__, __LINE__);
                                 ret = INCORRECT;
                                 break;
                              }
                           }
#ifdef HAVE_STATX
                        } while (bytes_buffered == stat_buf.stx_blksize);
#else
                        } while (bytes_buffered == stat_buf.st_blksize);
#endif
                        free(buffer);

                        if (ret != INCORRECT)
                        {
                           wmo_header[0] = '\015'; /* CR */
                           wmo_header[1] = '\015'; /* CR */
                           wmo_header[2] = '\012'; /* LF */
                           wmo_header[3] = 3;
                           if (write(to_fd, wmo_header, 4) != 4)
                           {
                              (void)fprintf(stderr,
                                            "Failed to write() WMO end to `%s' : %s (%s %d)",
                                            dest_file_name, strerror(errno),
                                            __FILE__, __LINE__);
                           }
                        }
                     }
                  }
                  else
                  {
                     (void)fprintf(stderr,
                                   "Source FAX file `%s' is empty. (%s %d)\n",
                                   fullname, __FILE__, __LINE__);
                     ret = INCORRECT;
                  }
               }
               if (close(from_fd) == -1)
               {
                  (void)fprintf(stderr,
                                "Failed to close() `%s' : %s (%s %d)\n",
                                fullname, strerror(errno), __FILE__, __LINE__);
               }
            }
         }
         if (close(to_fd) == -1)
         {
            (void)fprintf(stderr, "Failed to close() `%s' : %s (%s %d)\n",
                          dest_file_name, strerror(errno), __FILE__, __LINE__);
         }
         if (ret == SUCCESS)
         {
            /* Remove the original file */
            if (unlink(fullname) < 0)
            {
               (void)fprintf(stderr,
                             "Failed to unlink() original TIFF file `%s' : %s (%s %d)\n",
                             fullname, strerror(errno), __FILE__, __LINE__);
            }
            if (rename(dest_file_name, fullname) < 0)
            {
               (void)fprintf(stderr,
                             "Failed to rename() file `%s' to `%s' : %s (%s %d)\n",
                             dest_file_name, fullname, strerror(errno),
                             __FILE__, __LINE__);
            }
         }
      }
   }
   else
   {
      (void)fprintf(stderr,
                    "Usage: %s <filename> <WMO Header>[ <FAX format>]\n\n",
                    argv[0]);
      (void)fprintf(stderr, "    The following Fax formats are possible:\n");
      (void)fprintf(stderr, "       1 - DFAX1062\n");
      (void)fprintf(stderr, "       2 - DFAX1064\n");
      (void)fprintf(stderr, "       3 - DFAX1074\n");
      (void)fprintf(stderr, "       4 - DFAX1084\n");
      (void)fprintf(stderr, "       5 - DFAX1099\n");
      (void)fprintf(stderr, "    Default is 2 when none is supplied.\n\n");
      (void)fprintf(stderr, "    WMO Header must be of the following format:\n");
      (void)fprintf(stderr, "       TTAAii_CCCC_YYGGgg\n");
   }

   return(ret);
}
