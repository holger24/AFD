/*
 *  assemble.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   assemble - files stored as one file per bulletin are assembled
 **              into a single file
 **
 ** SYNOPSIS
 **   int assemble(char         *source_dir,
 **                char         *p_file_name,
 **                int          file_counter,
 **                char         *dest_file,
 **                int          type,
 **                unsigned int unique_number,
 **                int          nnn_length,
 **                unsigned int job_id,
 **                int          *files_to_send,
 **                off_t        *file_size)
 **
 ** DESCRIPTION
 **   The function assembles all WMO bulletin files found in the source_dir
 **   into one file. Before each bulletin a length indicator is written,
 **   to simplify later extraction.
 **
 **       <length indicator><SOH><CR><CR><LF>nnn<CR><CR><LF>
 **       WMO header<CR><CR><LF>WMO message<CR><CR><LF><ETX>
 **
 **   Five length indicators with the following length are currently
 **   created by assemble():
 **
 **                   2 Byte - Vax standard
 **                   4 Byte - Low byte first
 **                   4 Byte - High byte first
 **                   4 Byte - MSS standard
 **                   8 Byte - WMO standard (plus 2 Bytes type indicator)
 **                   4 Byte - DWD
 **
 **   In addition it is possible to generate the file without a length
 **   indicator with the help of the ASCII standard method.
 **
 **   The file name of the new file will be dest_file.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to read any valid data from the
 **   file. On success SUCCESS will be returned and the file size of
 **   the new assembled file will be returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.05.1999 H.Kiehl Created
 **   14.06.2002 H.Kiehl Fixed assembling MSS files.
 **   08.08.2002 H.Kiehl Added new type DWD.
 **   18.05.2005 H.Kiehl Forgot to write type indicator for WMO standard.
 **   16.11.2009 H.Kiehl Fix error if dest_file has same name as one of
 **                      the source files.
 **   06.09.2012 H.Kiehl Added WMO standard with dummy message at end.
 **   07.08.2013 H.Kiehl Added option to add WMO nnn counter.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>           /* strerror()                              */
#include <stdlib.h>           /* realloc(), free()                       */
#include <unistd.h>           /* read(), write(), close()                */
#include <sys/types.h>
#include <sys/stat.h>         /* stat(), S_ISDIR()                       */
#include <dirent.h>           /* opendir(), readdir(), closedir()        */
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"

/* Local gloabal variables. */
static int *counter;

/* Local function prototypes. */
static int write_length_indicator(int, int, int, int, int, int);


/*############################## assemble() #############################*/
int
assemble(char         *source_dir,
         char         *p_file_name,
         int          file_counter,
         char         *dest_file,
         int          type,
         unsigned int unique_number,
         int          nnn_length,
         unsigned int job_id,
         int          *files_to_send,
         off_t        *file_size)
{
   int          buffer_size = 0,
                counter_fd,
                fd,
                have_sohetx = YES,
                i,
                length,
                source_length,
                to_fd = -1;
   char         *buffer = NULL,
                *p_dest,
                *p_src,
                temp_dest_file[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   source_length = strlen(source_dir);
   if (source_length > (MAX_PATH_LENGTH - 1))
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Buffer to store destination file name to small (%d < %d).",
                 MAX_PATH_LENGTH, source_length);
      return(INCORRECT);
   }
   if (nnn_length > 0)
   {
      char counter_file[MAX_FILENAME_LENGTH];

      (void)snprintf(counter_file, MAX_FILENAME_LENGTH, "%s.%x",
                     NNN_FILE, job_id);

      /* Open counter file. */
      if ((counter_fd = open_counter_file(counter_file, &counter)) == -1)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to open counter file %s"),
                     counter_file);
         return(INCORRECT);
      }
   }
   else
   {
      counter_fd = -1; /* Silence compiler. */
   }
   (void)strcpy(temp_dest_file, source_dir);
   p_src = source_dir + source_length;
   p_dest = temp_dest_file + source_length;
   *p_src++ = '/'; *p_dest++ = '/';
   *p_dest = '\0';
   *file_size = 0;

   for (i = 0; i < file_counter; i++)
   {
      (void)my_strncpy(p_src, p_file_name, MAX_PATH_LENGTH - (source_length + 1));
      if ((fd = open(source_dir, O_RDONLY)) == -1)
      {
         receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to open() `%s' : %s #%x"),
                     source_dir, strerror(errno), job_id);
      }
      else
      {
#ifdef HAVE_STATX
         if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                   STATX_SIZE, &stat_buf) == -1)
#else
         if (fstat(fd, &stat_buf) == -1)
#endif
         {
            receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
#ifdef HAVE_STATX
#else
#endif
                        _("Failed to fstat() `%s' : %s"),
                        source_dir, strerror(errno));
            (void)close(fd);
         }
         else
         {
#ifdef HAVE_STATX
            if (stat_buf.stx_size > 0)
#else
            if (stat_buf.st_size > 0)
#endif
            {
#ifdef HAVE_STATX
               if (buffer_size < stat_buf.stx_size)
#else
               if (buffer_size < stat_buf.st_size)
#endif
               {
#ifdef HAVE_STATX
                  if ((buffer = realloc(buffer, stat_buf.stx_size)) == NULL)
#else
                  if ((buffer = realloc(buffer, stat_buf.st_size)) == NULL)
#endif
                  {
                     receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                 _("realloc() error : %s"), strerror(errno));
                     (void)close(fd);
                     exit(INCORRECT);
                  }
                  else
                  {
#ifdef HAVE_STATX
                     buffer_size = stat_buf.stx_size;
#else
                     buffer_size = stat_buf.st_size;
#endif
                  }
               }

#ifdef HAVE_STATX
               if (read(fd, buffer, stat_buf.stx_size) != stat_buf.stx_size)
#else
               if (read(fd, buffer, stat_buf.st_size) != stat_buf.st_size)
#endif
               {
                  receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                              _("Failed to read() `%s' : %s"),
                              source_dir, strerror(errno));
                  (void)close(fd);
               }
               else
               {
                  int offset = 0;

                  if (close(fd) == -1)
                  {
                     receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                                 _("close() error : %s"), strerror(errno));
                  }
                  if (to_fd == -1)
                  {
                     if (*p_dest == '\0')
                     {
                        (void)snprintf(p_dest,
                                       MAX_PATH_LENGTH - (source_length + 1),
                                       ".%x", unique_number);
                     }
                     if ((to_fd = open(temp_dest_file, O_CREAT | O_RDWR,
                                       FILE_MODE)) == -1)
                     {
                        receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                    _("Failed to open() `%s' : %s"),
                                    temp_dest_file, strerror(errno));
                        free(buffer);
                        return(INCORRECT);
                     }
                     if (type == FOUR_BYTE_DWD)
                     {
                        if (write(to_fd, "\000\000\000\000", 4) != 4)
                        {
                           receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                       _("Failed to write() first four zeros : %s"),
                                       strerror(errno));
                        }
                        else
                        {
                           *file_size += 4;
                        }
                     }
                  }

#ifdef HAVE_STATX
                  if ((buffer[0] == 1) && (buffer[stat_buf.stx_size - 1] == 3))
#else
                  if ((buffer[0] == 1) && (buffer[stat_buf.st_size - 1] == 3))
#endif
                  {
                     have_sohetx = YES;
                     if (nnn_length > 0)
                     {
                        offset = 1;
                     }
                  }
                  else
                  {
                     have_sohetx = NO;
                  }

                  if (type != ASCII_STANDARD)
                  {
                     if ((length = write_length_indicator(to_fd,
                                                          type, have_sohetx,
#ifdef HAVE_STATX
                                                          stat_buf.stx_size,
#else
                                                          stat_buf.st_size,
#endif
                                                          nnn_length,
                                                          counter_fd)) < 0)
                     {
                        if (length == -1)
                        {
                           receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                       _("write() error : %s"), strerror(errno));
                        }
                        continue;
                     }
                     *file_size += length;
                  }
                  else
                  {
                     if (nnn_length > 0)
                     {
                        char nnn[4 + MAX_INT_LENGTH + 1];

                        (void)next_counter(counter_fd, counter,
                                           my_power(10, nnn_length) - 1);
                        if (offset == 1)
                        {
                           nnn[0] = 1;
                        }
                        length = offset + sprintf(&nnn[offset], "\r\r\n%0*d",
                                                  nnn_length, *counter);
                        if (write(to_fd, nnn, length) != length)
                        {
                            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                        _("write() error : %s"), strerror(errno));
                        }
                        else
                        {
                           *file_size += length;
                        }
                     }
                  }

                  /* Write data */
#ifdef HAVE_STATX
                  if (writen(to_fd, &buffer[offset], stat_buf.stx_size - offset,
                             stat_buf.stx_blksize) != (stat_buf.stx_size - offset))
#else
                  if (writen(to_fd, &buffer[offset], stat_buf.st_size - offset,
                             stat_buf.st_blksize) != (stat_buf.st_size - offset))
#endif
                  {
                      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                  _("Failed to writen() data part : %s"),
                                  strerror(errno));
                  }
                  else
                  {
#ifdef HAVE_STATX
                     *file_size += stat_buf.stx_size;
#else
                     *file_size += stat_buf.st_size;
#endif
                  }
                  if (type == FOUR_BYTE_DWD)
                  {
                     if ((length = write_length_indicator(to_fd,
                                                          type, NO,
#ifdef HAVE_STATX
                                                          stat_buf.stx_size,
#else
                                                          stat_buf.st_size,
#endif
                                                          0, -1)) < 0)
                     {
                        if (length == -1)
                        {
                           receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                       _("write() error : %s"),
                                       strerror(errno));
                        }
                     }
                     else
                     {
                        *file_size += length;
                     }
                  }
               } /* read() == successful */
            }
            else
            {
               if (close(fd) == -1)
               {
                  receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                              _("close() error : %s"), strerror(errno));
               }
            }
         }
         if (unlink(source_dir) == -1)
         {
            receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                        _("Failed to unlink() `%s' : %s"),
                        source_dir, strerror(errno));
         }
      }
      p_file_name += MAX_FILENAME_LENGTH;
   } /* for (i = 0; i < file_counter; i++) */

   if (nnn_length > 0)
   {
      close_counter_file(counter_fd, &counter);
   }

   if (to_fd != -1)
   {
      if (type == FOUR_BYTE_DWD)
      {
         if (write(to_fd, "\000\000\000\000", 4) != 4)
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                        _("Failed to write() last four zeros : %s #%x"),
                        strerror(errno), job_id);
         }
         else
         {
            *file_size += 4;
         }
      }
      else if (type == WMO_WITH_DUMMY_MESSAGE)
           {
              if ((length = write_length_indicator(to_fd, type, have_sohetx,
                                                   0, 0, -1)) < 0)
              {
                 if (length == -1)
                 {
                    receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                _("write() error : %s #%x"),
                                strerror(errno), job_id);
                 }
              }
              else
              {
                 *file_size += length;
              }
           }
      if (close(to_fd) == -1)
      {
         receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                     _("close() error : %s"), strerror(errno));
      }
      if (rename(temp_dest_file, dest_file) == -1)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to rename() `%s' to `%s' : %s #%x"),
                     temp_dest_file, dest_file, strerror(errno), job_id);
      }
   }

   *(p_src - 1) = '\0';
   free(buffer);
   *files_to_send = 1;

   return(SUCCESS);
}


/*+++++++++++++++++++++++ write_length_indicator() ++++++++++++++++++++++*/
static int
write_length_indicator(int fd,
                       int type,
                       int have_sohetx,
                       int length,
                       int nnn_length,
                       int counter_fd)
{
   int           byte_order = 1,
                 write_length;
   unsigned char buffer[10 + 4 + MAX_INT_LENGTH];

   if (nnn_length > 0)
   {
      length += (nnn_length + 3);
   }

   switch (type)
   {
      case TWO_BYTE      : /* Vax Standard */
         {
            unsigned short short_length = (unsigned short)length;

            if (*(char *)&byte_order == 1)
            {
               /* little-endian */
               buffer[0] = ((char *)&short_length)[0];
               buffer[1] = ((char *)&short_length)[1];
            }
            else
            {
               /* big-endian */
               buffer[0] = ((char *)&short_length)[1];
               buffer[1] = ((char *)&short_length)[0];
            }
            write_length = 2;
         }
         break;

      case FOUR_BYTE_LBF : /* Low byte first */
         if (*(char *)&byte_order == 1)
         {
            /* little-endian */
            buffer[0] = ((char *)&length)[0];
            buffer[1] = ((char *)&length)[1];
            buffer[2] = ((char *)&length)[2];
            buffer[3] = ((char *)&length)[3];
         }
         else
         {
            /* big-endian */
            buffer[0] = ((char *)&length)[3];
            buffer[1] = ((char *)&length)[2];
            buffer[2] = ((char *)&length)[1];
            buffer[3] = ((char *)&length)[0];
         }
         write_length = 4;
         break;

      case FOUR_BYTE_DWD :
      case FOUR_BYTE_HBF : /* High byte first */
         if (*(char *)&byte_order == 1)
         {
            /* little-endian */
            buffer[0] = ((char *)&length)[3];
            buffer[1] = ((char *)&length)[2];
            buffer[2] = ((char *)&length)[1];
            buffer[3] = ((char *)&length)[0];
         }
         else
         {
            /* big-endian */
            buffer[0] = ((char *)&length)[0];
            buffer[1] = ((char *)&length)[1];
            buffer[2] = ((char *)&length)[2];
            buffer[3] = ((char *)&length)[3];
         }
         write_length = 4;
         break;

      case FOUR_BYTE_MSS : /* MSS Standard */
         if (*(char *)&byte_order == 1)
         {
            /* little-endian */
            buffer[0] = 250;
            buffer[1] = ((char *)&length)[2];
            buffer[2] = ((char *)&length)[1];
            buffer[3] = ((char *)&length)[0];
         }
         else
         {
            /* big-endian */
            buffer[0] = 250;
            buffer[1] = ((char *)&length)[1];
            buffer[2] = ((char *)&length)[2];
            buffer[3] = ((char *)&length)[3];
         }
         write_length = 4;
         break;

      case WMO_STANDARD  : /* WMO Standard */
      case WMO_WITH_DUMMY_MESSAGE : /* WMO Standard with dummy message at end. */
         if (length > 99999999)
         {
            buffer[0] = buffer[1] = buffer[2] = buffer[3] = buffer[4] = '9';
            buffer[5] = buffer[6] = buffer[7] = '9';
            buffer[8] = '\0';
            receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                        "Data length (%d) greater then what is possible in WMO header size, inserting maximum possible 99999999.",
                        length);
         }
         else if (length < 0)
              {
                 buffer[0] = buffer[1] = buffer[2] = buffer[3] = '0';
                 buffer[4] = buffer[5] = buffer[6] = buffer[7] = '0';
                 buffer[8] = '\0';
                 receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                             "Data length is less then 0 (%d), inserting 00000000.",
                             length);
              }
              else
              {
                 (void)snprintf((char *)buffer, 10, "%08d", length);
              }
         write_length = 10;
         buffer[8] = '0';
         if (have_sohetx == YES)
         {
            buffer[9] = '0';
         }
         else
         {
            buffer[9] = '1';
         }
         break;

      default            : /* Impossible! */
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Unknown length type (%d) for assembling bulletins."),
                     type);
         return(-2);

   }

   if (nnn_length > 0)
   {
      (void)next_counter(counter_fd, counter,
                         my_power(10, nnn_length) - 1);
      if (have_sohetx == YES)
      {
         buffer[write_length] = 1;
         write_length++;
      }
      write_length += sprintf((char *)(&buffer[write_length]), "\r\r\n%0*d",
                              nnn_length, *counter);
   }

   return(write(fd, buffer, write_length));
}
