/*
 *  convert_grib2wmo.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   convert_grib2wmo - converts the given file from GRIB to WMO
 **
 ** SYNOPSIS
 **   int convert_grib2wmo(char *file, off_t *file_size, char *default_CCCC)
 **
 ** DESCRIPTION
 **   This function converts a given file from GRIB to WMO.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to read any valid data from the
 **   file or cannot do the conversion for whatever reason. On success
 **   SUCCESS will be returned and the size of the converted file.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.02.2003 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>              /* strlen(), strcpy()                   */
#include <time.h>                /* time(), strftime(), gmtime()         */
#include <stdlib.h>              /* malloc(), free()                     */
#include <unistd.h>              /* close(), read()                      */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>            /* mmap(), munmap()                     */
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"

#define DATA_TYPES 3

/* Global local variables. */
static int  id_length[DATA_TYPES] = { 4, 4, 4 },
            end_id_length[DATA_TYPES] = { 4, 4, 4 };
static char bul_format[DATA_TYPES][5] =
            {
               "GRIB",
               "BUFR",
               "BLOK"
            },
            end_id[DATA_TYPES][5] =
            {
               "7777",
               "7777",
               "7777"
            };

/* Local function prototypes. */
static char *bin_search_start(char *, off_t, int *, off_t *);
static int  bin_search_end(char *, char *, size_t);


/*######################### convert_grib2wmo() ##########################*/
int
convert_grib2wmo(char *file, off_t *file_size, char *default_CCCC)
{
   int fd, ret = SUCCESS;

   if ((fd = open(file, O_RDONLY)) == -1)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to open() `%s' : %s"), file, strerror(errno));
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
      if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE | STATX_MODE, &stat_buf) == -1)
#else
      if (fstat(fd, &stat_buf) == -1)
#endif
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
#ifdef HAVE_STATX
                     _("Failed to statx() `%s' : %s"),
#else
                     _("Failed to fstat() `%s' : %s"),
#endif
                     file, strerror(errno));
         ret = INCORRECT;
      }
      else
      {
#ifdef HAVE_STATX
         if (stat_buf.stx_size < 20)
#else
         if (stat_buf.st_size < 20)
#endif
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                        _("File is to short to convert."));
            ret = INCORRECT;
         }
         else
         {
            char *buffer;

            if ((buffer = mmap(NULL,
#ifdef HAVE_STATX
                               stat_buf.stx_size, PROT_READ,
#else
                               stat_buf.st_size, PROT_READ,
#endif
                               MAP_SHARED, fd, 0)) == (caddr_t) -1)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           _("Failed to mmap() `%s' : %s"),
                           file, strerror(errno));
               ret = INCORRECT;
            }
            else
            {
               int  to_fd;
               char tmp_file[13];

               (void)strcpy(tmp_file, ".convert.tmp");
#ifdef HAVE_STATX
               if ((to_fd = open(tmp_file, O_WRONLY | O_CREAT | O_TRUNC,
                                 stat_buf.stx_mode)) == -1)
#else
               if ((to_fd = open(tmp_file, O_WRONLY | O_CREAT | O_TRUNC,
                                 stat_buf.st_mode)) == -1)
#endif
               {
                  receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                              _("Failed to open() `%s' : %s"),
                              tmp_file, strerror(errno));
#ifdef HAVE_STATX
                  if (munmap(buffer, stat_buf.stx_size) == -1)
#else
                  if (munmap(buffer, stat_buf.st_size) == -1)
#endif
                  {
                     receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                 _("Failed to munmap() from `%s' : %s"),
                                 file, strerror(errno));
                  }
                  ret = INCORRECT;
               }
               else
               {
                  int   i;
#ifdef HAVE_STATX
                  off_t total_length = stat_buf.stx_size;
#else
                  off_t total_length = stat_buf.st_size;
#endif
                  char  header[36 + 6], /* +6 for wmoheader_from_grib() */
                        *ptr,
                        *ptr_end;

                  ptr = buffer;
#ifdef HAVE_STATX
                  ptr_end = buffer + stat_buf.stx_size;
#else
                  ptr_end = buffer + stat_buf.st_size;
#endif
                  while (total_length > 9)
                  {
                     unsigned int data_length = 0,
                                  message_length = 0;

                     if ((ptr = bin_search_start(ptr, total_length, &i,
                                                 &total_length)) == NULL)
                     {
                        receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                    _("Failed to locate a valid start identifier in %s"),
                                    file);
                        ret = INCORRECT;
                        break;
                     }
                     wmoheader_from_grib(ptr - 4, &header[14], default_CCCC);
                     if ((i == 0) && (*(ptr + 3) == 0))
                     {
                        if ((data_length = bin_search_end(end_id[i],
                                                          ptr,
                                                          total_length)) == 0)
                        {
                           receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                       _("Failed to locate end in %s."),
                                       file);
                           ret = INCORRECT;
                           break;
                        }
                     }
                     else
                     {
                        message_length = 0;
                        message_length |= (unsigned char)*ptr;
                        message_length <<= 8;
                        message_length |= (unsigned char)*(ptr + 1);
                        message_length <<= 8;
                        message_length |= (unsigned char)*(ptr + 2);
                     }
                     if (message_length > (total_length + id_length[i]))
                     {
                        receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                    _("message length %lu greater then total length %d."),
                                    message_length, total_length + id_length[i]);
                        break;
                     }
                     if (message_length == 0)
                     {
                        data_length = data_length + id_length[i] + end_id_length[i];
                     }
                     else
                     {
                        data_length = message_length;
                     }
                     (void)snprintf(header, 14, "%08u00\01\015\015",
                                    data_length + 18 + 3 + 8);
                     header[13] = '\012';
                     header[20] = ' ';
                     header[25] = ' ';
                     header[32] = '\015';
                     header[33] = '\015';
                     header[34] = '\012';
                     if (write(to_fd, header, 35) != 35)
                     {
                        receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                    _("Failed to write() to `%s' : %s"),
                                    tmp_file, strerror(errno));
                        ret = INCORRECT;
                        break;
                     }
                     if (write(to_fd, ptr - 4, data_length) != data_length)
                     {
                        receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                    _("Failed to write() to `%s' : %s"),
                                    tmp_file, strerror(errno));
                        ret = INCORRECT;
                        break;
                     }
                     if (write(to_fd, "\015\015\012\03", 4) != 4)
                     {
                        receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                    _("Failed to write() to `%s' : %s"),
                                    tmp_file, strerror(errno));
                        ret = INCORRECT;
                        break;
                     }
                     *file_size += 35 + data_length + 4;
                     if (data_length > (total_length + 4))
                     {
                        receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                                    _("Hmmm, data_length (%d) > total_length (%u)?"),
                                    data_length, (total_length + 4));
                     }
                     ptr = ptr - 4 + data_length;
                     total_length = ptr_end - ptr;
                  } /* while (total_length > 9) */

                  if (close(to_fd) == -1)
                  {
                     receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                                 _("Failed to close() `%s' : %s"),
                                 tmp_file, strerror(errno));
                  }
               }
#ifdef HAVE_STATX
               if (munmap(buffer, stat_buf.stx_size) == -1)
#else
               if (munmap(buffer, stat_buf.st_size) == -1)
#endif
               {
                  receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                              _("Failed to munmap() from `%s' : %s"),
                              file, strerror(errno));
               }

               /* Remove the original file. */
               if (unlink(file) < 0)
               {
                  receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                              _("Failed to unlink() original file `%s' : %s"),
                              file, strerror(errno));
               }

               if ((ret == SUCCESS) || (*file_size > 0))
               {
                  if (rename(tmp_file, file) == -1)
                  {
                     receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                 _("Failed to rename() `%s' to `%s' : %s"),
                                 tmp_file, file, strerror(errno));
                  }
               }
            }
         }
      }
      if (close(fd) == -1)
      {
         receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to close() `%s' : %s"), file, strerror(errno));
      }
   }
   return(ret);
}


/*+++++++++++++++++++++++++ bin_search_start() ++++++++++++++++++++++++++*/
static char *
bin_search_start(char  *search_text,
                 off_t search_length,
                 int   *i,
                 off_t *total_length)
{
   int    hit[DATA_TYPES] = { 0, 0, 0 },
          count[DATA_TYPES] = { 0, 0, 0 },
          counter = 0;
   size_t tmp_length = *total_length;

   while (counter != search_length)
   {
      for (*i = 0; *i < DATA_TYPES; (*i)++)
      {
         if (*search_text == bul_format[*i][count[*i]])
         {
            if (++hit[*i] == 4)
            {
               (*total_length)--;
               return(++search_text);
            }
            (count[*i])++;
         }
         else
         {
            count[*i] = hit[*i] = 0;
         }
      }

      search_text++; counter++;
      (*total_length)--;
   }
   *total_length = tmp_length;

   return(NULL); /* Found nothing */
}


/*++++++++++++++++++++++++++ bin_search_end() +++++++++++++++++++++++++++*/
static int
bin_search_end(char   *search_string,
               char   *search_text,
               size_t total_length)
{
   int        hit = 0;
   static int counter;
   size_t     string_length = strlen(search_string);

   counter = 0;
   while (counter != total_length)
   {
      if (*(search_text++) == *(search_string++))
      {
         if (++hit == string_length)
         {
            counter -= (string_length - 1);
            return(counter);
         }
      }
      else
      {
         search_string -= hit + 1;
         hit = 0;
      }

      counter++;
   }

   return(0); /* Found nothing */
}
