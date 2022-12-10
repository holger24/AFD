/*
 *  tiff2gts.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2014 Deutscher Wetterdienst (DWD),
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
 **   tiff2gts - changes TIFF files to GTS files
 **
 ** SYNOPSIS
 **   int tiff2gts(char *path, char *filename)
 **         path     - path where the TIFF file can be found
 **         filename - file name of the TIFF file
 **
 ** DESCRIPTION
 **   Removes the TIFF-header and inserts a WMO-bulletin header.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to convert the file. Otherwise
 **   the size of the converted file is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   28.05.1996 H.Kiehl    Created
 **   18.04.2006 L.Brojboiu Handle not only DWD special TIFF files.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>                /* malloc(), free()                   */
#include <sys/stat.h>              /* stat()                             */
#include <sys/types.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "amgdefs.h"

#define OFFSET_START    8
#define OFFSET_END      12

/* Function prototypes. */
static void byte_swap(char *);


/*############################# tiff2gts() ##############################*/
off_t
tiff2gts(char *path, char* filename)
{
   int          fd,
                data_start,
                data_end,
                byte_order = 1;
   static off_t data_size;
   char         *buf,
                dest_file_name[MAX_PATH_LENGTH],
                fullname[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   (void)snprintf(fullname, MAX_PATH_LENGTH, "%s/%s", path, filename);

#ifdef HAVE_STATX
   if (statx(0, fullname, AT_STATX_SYNC_AS_STAT, STATX_SIZE, &stat_buf) == -1)
#else
   if (stat(fullname, &stat_buf) == -1)
#endif
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to stat() file `%s' : %s"),
                  fullname, strerror(errno));
      return(INCORRECT);
   }

#ifdef HAVE_STATX
   if (stat_buf.stx_size <= (OFFSET_END + 4))
#else
   if (stat_buf.st_size <= (OFFSET_END + 4))
#endif
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Could not convert file `%s'. File does not have the correct length."),
                  filename);
      return(INCORRECT);
   }

#ifdef HAVE_STATX
   if ((buf = malloc(stat_buf.stx_size)) == NULL)
#else
   if ((buf = malloc(stat_buf.st_size)) == NULL)
#endif
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("malloc() error : %s"), strerror(errno));
      return(INCORRECT);
   }

   if ((fd = open(fullname, O_RDONLY, 0)) < 0)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to open() `%s' : %s"), fullname, strerror(errno));
      free(buf);
      return(INCORRECT);
   }

#ifdef HAVE_STATX
   if (read(fd, buf, stat_buf.stx_size) != stat_buf.stx_size)
#else
   if (read(fd, buf, stat_buf.st_size) != stat_buf.st_size)
#endif
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("read() error : %s"), fullname, strerror(errno));
      free(buf);
      (void)close(fd);
      return(INCORRECT);
   }

   (void)close(fd);

   /*
    * Check if we have a DWD special scanner TIFF file or
    * some standart TIFF. The DWD special TIFF has at OFFSET_START
    * the offset to the start of the data, while others have
    * here <SOH><CR><CR><LF>.
    */
   if ((buf[OFFSET_START] == '\001') && (buf[OFFSET_START + 1] == '\015') &&
       (buf[OFFSET_START + 2] == '\015') && (buf[OFFSET_START + 3] == '\012'))
   {
      int ifd_offset;

      memcpy(&ifd_offset, &buf[4], 4);
      if (((buf[0] == 'I') && (buf[1] == 'I') && (*(char *)&byte_order != 1)) ||
          ((buf[0] == 'M') && (buf[1] == 'M') && (*(char *)&byte_order == 1)))

      {
         byte_swap((char *)&ifd_offset);
      }
      data_start = OFFSET_START;
      data_end = ifd_offset - 1;
   }
   else
   {
      memcpy(&data_start, &buf[OFFSET_START], 4);
      memcpy(&data_end, &buf[OFFSET_END], 4);
      if (((buf[0] == 'I') && (buf[1] == 'I') && (*(char *)&byte_order != 1)) ||
          ((buf[0] == 'M') && (buf[1] == 'M') && (*(char *)&byte_order == 1)))
      {
         byte_swap((char *)&data_start);
         byte_swap((char *)&data_end);
      }
   }

   data_size = data_end - data_start + 1;
#ifdef HAVE_STATX
   if (data_size > stat_buf.stx_size)
#else
   if (data_size > stat_buf.st_size)
#endif
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("File %s is corrupt. Data size (%d) larger then file size (%d)."),
#ifdef HAVE_STATX
                  filename, data_size, stat_buf.stx_size
#else
                  filename, data_size, stat_buf.st_size
#endif
                 );
      free(buf);
      return(INCORRECT);
   }
   else if (data_size <= 1)
        {
           receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                       _("File %s is corrupt. Data size (%d) less then or equal to file size (%d)."),
#ifdef HAVE_STATX
                       filename, data_size, stat_buf.stx_size
#else
                       filename, data_size, stat_buf.st_size
#endif
                      );
           free(buf);
           return(INCORRECT);
        }

   /*
    * Open destination file and copy data.
    */
   (void)snprintf(dest_file_name, MAX_PATH_LENGTH, "%s/.%s", path, filename);
   if ((fd = open(dest_file_name, (O_WRONLY | O_CREAT | O_TRUNC), FILE_MODE)) < 0)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to open() %s : %s"),
                  dest_file_name, strerror(errno));
      free(buf);
      return(INCORRECT);
   }

   if (write(fd, &buf[data_start], data_size) != data_size)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to write() to `%s' : %s"),
                  dest_file_name, strerror(errno));
      free(buf);
      (void)close(fd);
      return(INCORRECT);
   }

   free(buf);
   if (close(fd) == -1)
   {
      receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                  _("close() error : %s"), strerror(errno));
   }

   /* Remove the original file. */
   if (unlink(fullname) < 0)
   {
      receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to unlink() original TIFF file `%s' : %s"),
                  fullname, strerror(errno));
   }

   /* Rename file to its new name. */
   (void)snprintf(fullname, MAX_PATH_LENGTH, "%s/%s", path, filename);
   if (rename(dest_file_name, fullname) < 0)
   {
      receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to rename() file `%s' to `%s' : %s"),
                  dest_file_name, fullname, strerror(errno));
   }

   return(data_size);
}


/*+++++++++++++++++++++++++++++ byte_swap() +++++++++++++++++++++++++++++*/
static void
byte_swap(char *ptr)
{
   char tmp_byte;

   tmp_byte = ptr[0];
   ptr[0] = ptr[3];
   ptr[3] = tmp_byte;
   tmp_byte = ptr[1];
   ptr[1] = ptr[2];
   ptr[2] = tmp_byte;

   return;
}
