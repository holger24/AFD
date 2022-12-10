/*
 *  convert.c - Part of AFD, an automatic file distribution program.
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
 **   convert - converts a file from one format to another
 **
 ** SYNOPSIS
 **   int convert(char         *file_path,
 **               char         *file_name,
 **               int          type,
 **               int          nnn_length,
 **               unsigned int host_id,
 **               unsigned int job_id,
 **               off_t        *file_size)
 **
 ** DESCRIPTION
 **   The function converts the file from one format to another. The
 **   following conversions are currently implemented:
 **
 **     sohetx      - Adds <SOH><CR><CR><LF> to the beginning of the
 **                   file and <CR><CR><LF><ETX> to the end of the file.
 **     wmo         - Just add WMO 8 byte ascii length and 2 bytes type
 **                   indicator. If the message contains <SOH><CR><CR><LF>
 **                   at start and <CR><CR><LF><ETX> at the end, these
 **                   will be removed.
 **     sohetxwmo   - Adds WMO 8 byte ascii length and 2 bytes type
 **                   indicator plus <SOH><CR><CR><LF> at start and
 **                   <CR><CR><LF><ETX> to end if they are not there.
 **     sohetx2wmo1 - converts a file that contains many ascii bulletins
 **                   starting with SOH and ending with ETX to the WMO
 **                   standart (8 byte ascii length and 2 bytes type
 **                   indicators). The SOH and ETX will NOT be copied
 **                   to the new file.
 **     sohetx2wmo0 - same as above only that here the SOH and ETX will be
 **                   copied to the new file.
 **     mrz2wmo     - Converts GRIB, BUFR and BLOK files to files with
 **                   8 byte ascii length and 2 bytes type indicator plus
 **                   <SOH><CR><CR><LF> at start and <CR><CR><LF><ETX> to
 **                   the end, for the individual fields.
 **     unix2dos    - Converts all <LF> to <CR><LF>.
 **     dos2unix    - Converts all <CR><LF> to <LF>.
 **     lf2crcrlf   - Converts all <LF> to <CR><CR><LF>.
 **     crcrlf2lf   - Converts all <CR><CR><LF> to <LF>.
 **
 **   The original file will be overwritten with the new format.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to read any valid data from the
 **   file. On success SUCCESS will be returned and the size of the
 **   newly created file.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.09.2003 H.Kiehl Created
 **   10.05.2005 H.Kiehl Don't just check for SOH and ETX, check for
 **                      <SOH><CR><CR><LF> and <CR><CR><LF><ETX>.
 **   20.07.2006 H.Kiehl Added mrz2wmo.
 **   19.11.2008 H.Kiehl Added unix2dos, dos2unix, lf2crcrlf and crcrlf2lf.
 **   29.06.2011 H.Kiehl For sohetx, if it already contains SOH and ETX,
 **                      do not create a new file of zero length.
 **   06.08.2013 H.Kiehl Added option to add WMO nnn counter to SOHETX2WMO0,
 **                      SOHETX2WMO1, SOHETX, SOHETXWMO and ONLY_WMO.
 **
 */
DESCR__E_M3

#include <stdio.h>                       /* snprintf(), rename()         */
#include <stdlib.h>                      /* strtoul(), malloc(), free()  */
#include <string.h>                      /* strerror()                   */
#include <ctype.h>                       /* isdigit()                    */
#include <unistd.h>                      /* close(), unlink()            */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>                   /* mmap(), munmap()             */
#endif
#include <fcntl.h>                       /* open()                       */
#include <errno.h>
#include "amgdefs.h"

#ifndef MAP_FILE     /* Required for BSD          */
# define MAP_FILE 0  /* All others do not need it */
#endif

/* Macro definition that clears things when we call return(). */
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
#define CONVERT_CLEAN_UP()                                  \
        {                                                   \
           (void)close(from_fd);                            \
           (void)munmap((void *)src_ptr, stat_buf.stx_size);\
           if (nnn_length > 0)                              \
           {                                                \
              close_counter_file(counter_fd, &counter);     \
           }                                                \
        }
# else
#define CONVERT_CLEAN_UP()                                  \
        {                                                   \
           (void)close(from_fd);                            \
           (void)munmap((void *)src_ptr, stat_buf.st_size); \
           if (nnn_length > 0)                              \
           {                                                \
              close_counter_file(counter_fd, &counter);     \
           }                                                \
        }
# endif
#else
#define CONVERT_CLEAN_UP()                              \
        {                                               \
           (void)close(from_fd);                        \
           (void)munmap_emu((void *)src_ptr);           \
           if (nnn_length > 0)                          \
           {                                            \
              close_counter_file(counter_fd, &counter); \
           }                                            \
        }
#endif

/* External global variables. */
extern char *p_work_dir;

/* Local function prototypes. */
static int  crcrlf2lf(char *, char *, off_t *),
            dos2unix(char *, char *, off_t *),
            lf2crcrlf(char *, char *, off_t *),
            unix2dos(char *, char *, off_t *);


/*############################### convert() #############################*/
int
convert(char         *file_path,
        char         *file_name,
        int          type,
        int          nnn_length,
        unsigned int host_id,
        unsigned int job_id,
        off_t        *file_size)
{
   int   *counter,
         counter_fd,
         no_change = NO;
   char  fullname[MAX_PATH_LENGTH],
         new_name[MAX_PATH_LENGTH + 12];

   *file_size = 0;
   (void)snprintf(fullname, MAX_PATH_LENGTH, "%s/%s", file_path, file_name);
   (void)snprintf(new_name, MAX_PATH_LENGTH + 12, "%s.tmpnewname", fullname);
   if (type == UNIX2DOS)
   {
      if (unix2dos(fullname, new_name, file_size) == INCORRECT)
      {
         return(INCORRECT);
      }
   }
   else if (type == DOS2UNIX)
        {
           if (dos2unix(fullname, new_name, file_size) == INCORRECT)
           {
              return(INCORRECT);
           }
        }
   else if (type == LF2CRCRLF)
        {
           if (lf2crcrlf(fullname, new_name, file_size) == INCORRECT)
           {
              return(INCORRECT);
           }
        }
   else if (type == CRCRLF2LF)
        {
           if (crcrlf2lf(fullname, new_name, file_size) == INCORRECT)
           {
              return(INCORRECT);
           }
        }
        else
        {
           int          add_nnn_length,
                        from_fd,
                        to_fd;
           char         *src_ptr;
#ifdef HAVE_STATX
           struct statx stat_buf;
#else
           struct stat  stat_buf;
#endif

           if ((from_fd = open(fullname, O_RDONLY)) < 0)
           {
              receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                          _("Could not open() `%s' for extracting : %s"),
                          fullname, strerror(errno));
              return(INCORRECT);
           }

           /* Need size of input file. */
#ifdef HAVE_STATX
           if (statx(from_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                     STATX_SIZE | STATX_MODE , &stat_buf) == -1)
#else
           if (fstat(from_fd, &stat_buf) == -1)
#endif
           {
              receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
#ifdef HAVE_STATX
                          _("statx() error : %s"),
#else
                          _("fstat() error : %s"),
#endif
                          strerror(errno));
              (void)close(from_fd);
              return(INCORRECT);
           }

           /*
            * If the size of the file is less then 10 forget it. There can't
            * be a WMO bulletin in it.
            */
#ifdef HAVE_STATX
           if (stat_buf.stx_size < 10)
#else
           if (stat_buf.st_size < 10)
#endif
           {
              receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                          _("Got a file for converting that is less then 10 bytes long!"));
              (void)close(from_fd);
              return(INCORRECT);
           }

#ifdef HAVE_MMAP
           if ((src_ptr = mmap(NULL,
# ifdef HAVE_STATX
                               stat_buf.stx_size, PROT_READ,
# else
                               stat_buf.st_size, PROT_READ,
# endif
                               (MAP_FILE | MAP_SHARED),
                               from_fd, 0)) == (caddr_t) -1)
#else
           if ((src_ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                                   stat_buf.stx_size, PROT_READ,
# else
                                   stat_buf.st_size, PROT_READ,
# endif
                                   (MAP_FILE | MAP_SHARED),
                                   fullname, 0)) == (caddr_t) -1)
#endif
           {
              receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                          _("mmap() error : %s"), strerror(errno));
              (void)close(from_fd);
              return(INCORRECT);
           }

           if (nnn_length > 0)
           {
              char counter_file[MAX_FILENAME_LENGTH];

              (void)snprintf(counter_file, MAX_FILENAME_LENGTH, "%s.%x",
                             NNN_FILE, host_id);

              /* Open counter file. */
              if ((counter_fd = open_counter_file(counter_file, &counter)) == -1)
              {
                 receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                             _("Failed to open counter file %s"),
                             counter_file);
                 (void)close(from_fd);
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
                 (void)munmap((void *)src_ptr, stat_buf.stx_size);
# else
                 (void)munmap((void *)src_ptr, stat_buf.st_size);
# endif
#else
                 (void)munmap_emu((void *)src_ptr);
#endif
                 return(INCORRECT);
              }
              add_nnn_length = nnn_length + 3;
           }
           else
           {
              add_nnn_length = 0;
              counter_fd = -1; /* Silence compiler. */
           }

           switch (type)
           {
              case SOHETX :
#ifdef HAVE_STATX
                 if ((*src_ptr != 1) && (*(src_ptr + stat_buf.stx_size - 1) != 3))
#else
                 if ((*src_ptr != 1) && (*(src_ptr + stat_buf.st_size - 1) != 3))
#endif
                 {
                    char buffer[4];

#ifdef HAVE_STATX
                    if ((to_fd = open(new_name, (O_RDWR | O_CREAT | O_TRUNC),
                                      stat_buf.stx_mode)) == -1)
#else
                    if ((to_fd = open(new_name, (O_RDWR | O_CREAT | O_TRUNC),
                                      stat_buf.st_mode)) == -1)
#endif
                    {
                       receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                   _("Failed to open() %s : %s"),
                                   new_name, strerror(errno));
                       CONVERT_CLEAN_UP();
                       return(INCORRECT);
                    }
                    buffer[0] = 1;
                    buffer[1] = 13;
                    buffer[2] = 13;
                    buffer[3] = 10;
                    if (write(to_fd, buffer, 4) != 4)
                    {
                       receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                   _("Failed to write() to `%s' : %s"),
                                   new_name, strerror(errno));
                       CONVERT_CLEAN_UP();
                       (void)close(to_fd);
                       return(INCORRECT);
                    }
                    *file_size += 4;

                    if (nnn_length > 0)
                    {
                       int  add_length;
                       char nnn[MAX_INT_LENGTH + 3 + 1];

                       (void)next_counter(counter_fd, counter,
                                          my_power(10, nnn_length) - 1);
                       add_length = sprintf(nnn, "%0*d\r\r\n",
                                            nnn_length, *counter);
                       if (write(to_fd, nnn, add_length) != add_length)
                       {
                          receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                      _("Failed to write() to `%s' : %s"),
                                      new_name, strerror(errno));
                          CONVERT_CLEAN_UP();
                          (void)close(to_fd);
                          return(INCORRECT);
                       }
                       *file_size += add_length;
                    }

#ifdef HAVE_STATX
                    if (writen(to_fd, src_ptr, stat_buf.stx_size,
                               stat_buf.stx_blksize) != stat_buf.stx_size)
#else
                    if (writen(to_fd, src_ptr, stat_buf.st_size,
                               stat_buf.st_blksize) != stat_buf.st_size)
#endif
                    {
                       receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                   _("Failed to writen() to `%s' : %s"),
                                   new_name, strerror(errno));
                       CONVERT_CLEAN_UP();
                       (void)close(to_fd);
                       return(INCORRECT);
                    }
#ifdef HAVE_STATX
                    *file_size += stat_buf.stx_size;
#else
                    *file_size += stat_buf.st_size;
#endif

                    buffer[0] = 13;
                    buffer[2] = 10;
                    buffer[3] = 3;
                    if (write(to_fd, buffer, 4) != 4)
                    {
                       receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                   _("Failed to write() to `%s' : %s"),
                                   new_name, strerror(errno));
                       CONVERT_CLEAN_UP();
                       (void)close(to_fd);
                       return(INCORRECT);
                    }
                    *file_size += 4;
                 }
                 else
                 {
#ifdef HAVE_STATX
                    *file_size += stat_buf.stx_size;
#else
                    *file_size += stat_buf.st_size;
#endif
                    no_change = YES;
                 }
                 break;

              case ONLY_WMO :
                 {
                    int   offset;
                    off_t size;
                    char  length_indicator[10];

#ifdef HAVE_STATX
                    if ((to_fd = open(new_name, (O_RDWR | O_CREAT | O_TRUNC),
                                      stat_buf.stx_mode)) == -1)
#else
                    if ((to_fd = open(new_name, (O_RDWR | O_CREAT | O_TRUNC),
                                      stat_buf.st_mode)) == -1)
#endif
                    {
                       receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                   _("Failed to open() %s : %s"),
                                   new_name, strerror(errno));
                       CONVERT_CLEAN_UP();
                       return(INCORRECT);
                    }

                    if ((*src_ptr == 1) &&
                        (*(src_ptr + 1) == 13) &&
                        (*(src_ptr + 2) == 13) &&
                        (*(src_ptr + 3) == 10) &&
#ifdef HAVE_STATX
                        (*(src_ptr + stat_buf.stx_size - 4) == 13) &&
                        (*(src_ptr + stat_buf.stx_size - 3) == 13) &&
                        (*(src_ptr + stat_buf.stx_size - 2) == 10) &&
                        (*(src_ptr + stat_buf.stx_size - 1) == 3)
#else
                        (*(src_ptr + stat_buf.st_size - 4) == 13) &&
                        (*(src_ptr + stat_buf.st_size - 3) == 13) &&
                        (*(src_ptr + stat_buf.st_size - 2) == 10) &&
                        (*(src_ptr + stat_buf.st_size - 1) == 3)
#endif
                       )
                    {
                       offset = 4;
#ifdef HAVE_STATX
                       size = stat_buf.stx_size - 8 + add_nnn_length;
#else
                       size = stat_buf.st_size - 8 + add_nnn_length;
#endif
                    }
                    else
                    {
#ifdef HAVE_STATX
                       size = stat_buf.stx_size + add_nnn_length;
#else
                       size = stat_buf.st_size + add_nnn_length;
#endif
                       offset = 0;
                    }
                    if (size > 99999999)
                    {
                       (void)strcpy(length_indicator, "99999999");
                       receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
#if SIZEOF_OFF_T == 4
                                   "Data length (%ld) greater then what is possible in WMO header size, inserting maximum possible 99999999.",
#else
                                   "Data length (%lld) greater then what is possible in WMO header size, inserting maximum possible 99999999.",
#endif
                                   (pri_off_t)size);
                    }
                    else
                    {
                       (void)snprintf(length_indicator, 10, "%08lu",
                                      (unsigned long)size);
                    }
                    length_indicator[8] = '0';
                    length_indicator[9] = '1';
                    if (write(to_fd, length_indicator, 10) != 10)
                    {
                       receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                   _("Failed to write() to `%s' : %s"),
                                   new_name, strerror(errno));
                       CONVERT_CLEAN_UP();
                       (void)close(to_fd);
                       return(INCORRECT);
                    }
                    *file_size += 10;

                    if (nnn_length > 0)
                    {
                       int  add_length;
                       char nnn[3 + MAX_INT_LENGTH + 1];

                       (void)next_counter(counter_fd, counter,
                                          my_power(10, nnn_length) - 1);
                       nnn[0] = 13;
                       nnn[1] = 13;
                       nnn[2] = 10;
                       add_length = sprintf(&nnn[3], "%0*d",
                                            nnn_length, *counter);
                       if (write(to_fd, nnn, add_length) != add_length)
                       {
                          receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                      _("Failed to write() to `%s' : %s"),
                          new_name, strerror(errno));
                          CONVERT_CLEAN_UP();
                          (void)close(to_fd);
                          return(INCORRECT);
                       }
                       *file_size += add_length;
                    }

#ifdef HAVE_STATX
                    if (writen(to_fd, (src_ptr + offset), size,
                               stat_buf.stx_blksize) != size)
#else
                    if (writen(to_fd, (src_ptr + offset), size,
                               stat_buf.st_blksize) != size)
#endif
                    {
                       receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                   _("Failed to writen() to `%s' : %s"),
                                   new_name, strerror(errno));
                       CONVERT_CLEAN_UP();
                       (void)close(to_fd);
                       return(INCORRECT);
                    }
                    *file_size += size;
                 }
                 break;

              case SOHETXWMO :
                 {
                    int    additional_length,
                           end_offset,
                           front_offset;
                    size_t length,
                           write_length;
                    char   length_indicator[14];

#ifdef HAVE_STATX
                    if ((to_fd = open(new_name, (O_RDWR | O_CREAT | O_TRUNC),
                                      stat_buf.stx_mode)) == -1)
#else
                    if ((to_fd = open(new_name, (O_RDWR | O_CREAT | O_TRUNC),
                                      stat_buf.st_mode)) == -1)
#endif
                    {
                       receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                   _("Failed to open() %s : %s"),
                                   new_name, strerror(errno));
                       CONVERT_CLEAN_UP();
                       return(INCORRECT);
                    }

                    if (*src_ptr != 1)
                    {
                       if (
#ifdef HAVE_STATX
                           (stat_buf.stx_size > 10) &&
#else
                           (stat_buf.st_size > 10) &&
#endif
                           (isdigit((int)*src_ptr)) &&
                           (isdigit((int)*(src_ptr + 1))) &&
                           (isdigit((int)*(src_ptr + 2))) &&
                           (isdigit((int)*(src_ptr + 3))) &&
                           (isdigit((int)*(src_ptr + 4))) &&
                           (isdigit((int)*(src_ptr + 5))) &&
                           (isdigit((int)*(src_ptr + 6))) &&
                           (isdigit((int)*(src_ptr + 7))) &&
                           (isdigit((int)*(src_ptr + 8))) &&
                           (isdigit((int)*(src_ptr + 9))))
                       {
                          length_indicator[0] = *src_ptr;
                          length_indicator[1] = *(src_ptr + 1);
                          length_indicator[2] = *(src_ptr + 2);
                          length_indicator[3] = *(src_ptr + 3);
                          length_indicator[4] = *(src_ptr + 4);
                          length_indicator[5] = *(src_ptr + 5);
                          length_indicator[6] = *(src_ptr + 6);
                          length_indicator[7] = *(src_ptr + 7);
                          length_indicator[8] = '\0';
                          length = (size_t)strtoul(length_indicator, NULL, 10);
#ifdef HAVE_STATX
                          if (stat_buf.stx_size == (length + 10))
#else
                          if (stat_buf.st_size == (length + 10))
#endif
                          {
                             if (*(src_ptr + 10) == 1)
                             {
                                if (*(src_ptr + 11) == 10)
                                {
                                   length_indicator[10] = 1;
                                   length_indicator[11] = 13;
                                   length_indicator[12] = 13;
                                   length_indicator[13] = 10;
                                   length = 14;
                                   front_offset = 12;
                                   additional_length = 4;
                                }
                                else if (((*(src_ptr + 11) == 13) ||
                                          (*(src_ptr + 11) == ' ')) &&
                                         (*(src_ptr + 12) == 10))
                                     {
                                        length_indicator[10] = 1;
                                        length_indicator[11] = 13;
                                        length_indicator[12] = 13;
                                        length_indicator[13] = 10;
                                        length = 14;
                                        front_offset = 13;
                                        additional_length = 4;
                                     }
                                     else
                                     {
                                        length = 10;
                                        front_offset = 10;
                                        additional_length = 0;
                                     }
                             }
                             else
                             {
                                length_indicator[10] = 1;
                                length_indicator[11] = 13;
                                length_indicator[12] = 13;
                                length_indicator[13] = 10;
                                length = 14;
                                front_offset = 10;
                                additional_length = 4;
                             }
                          }
                          else
                          {
                             length_indicator[10] = 1;
                             length_indicator[11] = 13;
                             length_indicator[12] = 13;
                             length_indicator[13] = 10;
                             length = 14;
                             front_offset = 0;
                             additional_length = 4;
                          }
                       }
                       else
                       {
                          length_indicator[10] = 1;
                          length_indicator[11] = 13;
                          length_indicator[12] = 13;
                          length_indicator[13] = 10;
                          length = 14;
                          front_offset = 0;
                          additional_length = 4;
                       }
                    }
                    else
                    {
                       if (*(src_ptr + 1) == 10)
                       {
                          length_indicator[10] = 1;
                          length_indicator[11] = 13;
                          length_indicator[12] = 13;
                          length_indicator[13] = 10;
                          length = 14;
                          front_offset = 2;
                          additional_length = 4;
                       }
                       else if (((*(src_ptr + 1) == 13) ||
                                 (*(src_ptr + 1) == ' ')) &&
                                (*(src_ptr + 2) == 10))
                            {
                               length_indicator[10] = 1;
                               length_indicator[11] = 13;
                               length_indicator[12] = 13;
                               length_indicator[13] = 10;
                               length = 14;
                               front_offset = 3;
                               additional_length = 4;
                            }
                            else if ((*(src_ptr + 1) == 13) &&
                                     (*(src_ptr + 2) == 13) &&
                                     (*(src_ptr + 3) == 10))
                                 {
                                    length = 10;
                                    front_offset = 0;
                                    additional_length = 0;
                                 }
                                 else
                                 {
                                    length_indicator[10] = 1;
                                    length_indicator[11] = 13;
                                    length_indicator[12] = 13;
                                    length_indicator[13] = 10;
                                    length = 14;
                                    front_offset = 1;
                                    additional_length = 4;
                                 }
                    }
#ifdef HAVE_STATX
                    if (*(src_ptr + stat_buf.stx_size - 1) != 3)
#else
                    if (*(src_ptr + stat_buf.st_size - 1) != 3)
#endif
                    {
                       end_offset = 0;
                       additional_length += 4;
                    }
                    else
                    {
#ifdef HAVE_STATX
                       if (*(src_ptr + stat_buf.stx_size - 2) != 10)
#else
                       if (*(src_ptr + stat_buf.st_size - 2) != 10)
#endif
                       {
                          end_offset = 1;
                          additional_length += 4;
                       }
#ifdef HAVE_STATX
                       else if (*(src_ptr + stat_buf.stx_size - 3) != 13)
#else
                       else if (*(src_ptr + stat_buf.st_size - 3) != 13)
#endif
                            {
                               end_offset = 2;
                               additional_length += 4;
                            }
                            else
                            {
                               end_offset = 0;
                            }
                    }
#ifdef HAVE_STATX
                    if ((stat_buf.stx_size - front_offset - end_offset + additional_length + add_nnn_length) > 99999999)
#else
                    if ((stat_buf.st_size - front_offset - end_offset + additional_length + add_nnn_length) > 99999999)
#endif
                    {
                       (void)strcpy(length_indicator, "99999999");
                       receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
#if SIZEOF_OFF_T == 4
                                   "Data length (%ld) greater then what is possible in WMO header size, inserting maximum possible 99999999.",
#else
                                   "Data length (%lld) greater then what is possible in WMO header size, inserting maximum possible 99999999.",
#endif
#ifdef HAVE_STATX
                                   (pri_off_t)(stat_buf.stx_size - front_offset - end_offset + additional_length)
#else
                                   (pri_off_t)(stat_buf.st_size - front_offset - end_offset + additional_length)
#endif
                                  );
                    }
                    else
                    {
#ifdef HAVE_STATX
                       (void)snprintf(length_indicator, 14, "%08lu",
                                      (unsigned long)stat_buf.stx_size - front_offset - end_offset + additional_length + add_nnn_length);
#else
                       (void)snprintf(length_indicator, 14, "%08lu",
                                      (unsigned long)stat_buf.st_size - front_offset - end_offset + additional_length + add_nnn_length);
#endif
                    }
                    length_indicator[8] = '0';
                    length_indicator[9] = '0';
#ifdef HAVE_STATX
                    if (writen(to_fd, length_indicator, length,
                               stat_buf.stx_blksize) != length)
#else
                    if (writen(to_fd, length_indicator, length,
                               stat_buf.st_blksize) != length)
#endif
                    {
                       receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                   _("Failed to writen() to `%s' : %s"),
                                   new_name, strerror(errno));
                       CONVERT_CLEAN_UP();
                       (void)close(to_fd);
                       return(INCORRECT);
                    }
                    *file_size += length;

                    if (nnn_length > 0)
                    {
                       int  add_length;
                       char nnn[MAX_INT_LENGTH + 3 + 1];

                       (void)next_counter(counter_fd, counter,
                                          my_power(10, nnn_length) - 1);
                       add_length = sprintf(nnn, "%0*d\r\r\n",
                                            nnn_length, *counter);
                       if (write(to_fd, nnn, add_length) != add_length)
                       {
                          receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                      _("Failed to write() to `%s' : %s"),
                                      new_name, strerror(errno));
                          CONVERT_CLEAN_UP();
                          (void)close(to_fd);
                          return(INCORRECT);
                       }
                       *file_size += add_length;
                    }

#ifdef HAVE_STATX
                    write_length = stat_buf.stx_size - (front_offset + end_offset);
                    if (writen(to_fd, src_ptr + front_offset, write_length,
                               stat_buf.stx_blksize) != write_length)
#else
                    write_length = stat_buf.st_size - (front_offset + end_offset);
                    if (writen(to_fd, src_ptr + front_offset, write_length,
                               stat_buf.st_blksize) != write_length)
#endif
                    {
                       receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                   _("Failed to writen() to `%s' : %s"),
                                   new_name, strerror(errno));
                       CONVERT_CLEAN_UP();
                       (void)close(to_fd);
                       return(INCORRECT);
                    }
                    *file_size += write_length;

#ifdef HAVE_STATX
                    if ((*(src_ptr + stat_buf.stx_size - 1) != 3) ||
                        (*(src_ptr + stat_buf.stx_size - 2) != 10) ||
                        (*(src_ptr + stat_buf.stx_size - 3) != 13))
#else
                    if ((*(src_ptr + stat_buf.st_size - 1) != 3) ||
                        (*(src_ptr + stat_buf.st_size - 2) != 10) ||
                        (*(src_ptr + stat_buf.st_size - 3) != 13))
#endif
                    {
                       length_indicator[10] = 13;
                       length_indicator[11] = 13;
                       length_indicator[12] = 10;
                       length_indicator[13] = 3;
                       if (write(to_fd, &length_indicator[10], 4) != 4)
                       {
                          receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                      _("Failed to write() to `%s' : %s"),
                                      new_name, strerror(errno));
                          CONVERT_CLEAN_UP();
                          (void)close(to_fd);
                          return(INCORRECT);
                       }
                       *file_size += 4;
                    }
                 }
                 break;

              case SOHETX2WMO0 :
              case SOHETX2WMO1 :
                 {
                    int    add_sohcrcrlf;
                    size_t length;
                    char   length_indicator[14 + MAX_INT_LENGTH + 3],
                           *ptr,
                           *ptr_start;

#ifdef HAVE_STATX
                    if ((to_fd = open(new_name, (O_RDWR | O_CREAT | O_TRUNC),
                                      stat_buf.stx_mode)) == -1)
#else
                    if ((to_fd = open(new_name, (O_RDWR | O_CREAT | O_TRUNC),
                                      stat_buf.st_mode)) == -1)
#endif
                    {
                       receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                   _("Failed to open() %s : %s"),
                                   new_name, strerror(errno));
                       CONVERT_CLEAN_UP();
                       return(INCORRECT);
                    }

                    ptr = src_ptr;
                    do
                    {
                       /* Always try finding the beginning of a message */
                       /* first.                                        */
                       do
                       {
#ifdef HAVE_STATX
                          while ((*ptr != 1) &&
                                 ((ptr - src_ptr + 1) < stat_buf.stx_size))
#else
                          while ((*ptr != 1) &&
                                 ((ptr - src_ptr + 1) < stat_buf.st_size))
#endif
                          {
                             ptr++;
                          }
                          if (*ptr == 1)
                          {
                             if (
#ifdef HAVE_STATX
                                 ((ptr - src_ptr + 3) < stat_buf.stx_size) &&
#else
                                 ((ptr - src_ptr + 3) < stat_buf.st_size) &&
#endif
                                 ((*(ptr + 1) == 10) || (*(ptr + 3) == 10) ||
                                  (*(ptr + 2) == 10)))
                             {
                                break;
                             }
                             else
                             {
                                ptr++;
                             }
                          }
#ifdef HAVE_STATX
                       } while ((ptr - src_ptr + 1) < stat_buf.stx_size);
#else
                       } while ((ptr - src_ptr + 1) < stat_buf.st_size);
#endif

                       /* Now lets cut out the message. */
                       if (*ptr == 1)
                       {
                          if (type == SOHETX2WMO1)
                          {
                             ptr++; /* Away with SOH */
                             add_sohcrcrlf = NO;
                          }
                          else
                          {
#ifdef HAVE_STATX
                             if ((ptr + 1 - src_ptr + 3) < stat_buf.stx_size)
#else
                             if ((ptr + 1 - src_ptr + 3) < stat_buf.st_size)
#endif
                             {
                                if (*(ptr + 1) == 10)
                                {
                                   ptr += 2;
                                   add_sohcrcrlf = YES;
                                }
                                else if (((*(ptr + 1) == 13) ||
                                          (*(ptr + 1) == ' ')) &&
                                         (*(ptr + 2) == 10))
                                     {
                                        ptr += 3;
                                        add_sohcrcrlf = YES;
                                     }
                                else if ((*(ptr + 1) == 13) &&
                                         (*(ptr + 2) == 13) &&
                                         (*(ptr + 3) == 10))
                                     {
                                        add_sohcrcrlf = NO;
                                     }
                                     else
                                     {
                                        ptr++;
                                        add_sohcrcrlf = YES;
                                     }
                             }
                             else
                             {
                                add_sohcrcrlf = NO;
                             }
                          }

                          /* Lets find end of message. */
                          ptr_start = ptr;
                          do
                          {
#ifdef HAVE_STATX
                             while ((*ptr != 3) &&
                                    ((ptr - src_ptr + 1) < stat_buf.stx_size))
#else
                             while ((*ptr != 3) &&
                                    ((ptr - src_ptr + 1) < stat_buf.st_size))
#endif
                             {
                                ptr++;
                             }
                             if (*ptr == 3)
                             {
                                if (((ptr - ptr_start) >= 1) &&
                                    (*(ptr - 1) == 10))
                                {
                                   break;
                                }
                                else
                                {
                                   ptr++;
                                }
                             }
#ifdef HAVE_STATX
                          } while ((ptr - src_ptr + 1) < stat_buf.stx_size);
#else
                          } while ((ptr - src_ptr + 1) < stat_buf.st_size);
#endif

                          if (*ptr == 3)
                          {
                             int add_offset = 0,
                                 end_length,
                                 start_length;

                             if (type == SOHETX2WMO1)
                             {
                                length = ptr - ptr_start;
                                length_indicator[9] = '1';
                                start_length = 10;
                                end_length = 0;
                             }
                             else
                             {
                                if (add_sohcrcrlf == YES)
                                {
                                   length = ptr - ptr_start + 1;
                                   length_indicator[10] = 1;
                                   length_indicator[11] = 13;
                                   length_indicator[12] = 13;
                                   length_indicator[13] = 10;
                                   start_length = 14;
                                }
                                else
                                {
                                   length = ptr - ptr_start + 1;
                                   start_length = 10;
                                }
                                length_indicator[9] = '0';

                                if ((*(ptr - 1) == 10) && (*(ptr - 2) == 13) &&
                                    (*(ptr - 3) == 13))
                                {
                                   end_length = 0;
                                }
                                else if ((*(ptr - 1) == 10) &&
                                         ((*(ptr - 2) == 13) ||
                                          (*(ptr - 2) == ' ')))
                                     {
                                        end_length = 4;
                                        length -= 3;
                                     }
                                else if (*(ptr - 1) == 10)
                                     {
                                        end_length = 4;
                                        length -= 2;
                                     }
                                     else
                                     {
                                        end_length = 4;
                                        length -= 1;
                                     }
                             }
                             if ((length + end_length + add_nnn_length) > 99999999)
                             {
                                (void)strcpy(length_indicator, "99999999");
                                receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
#if SIZEOF_OFF_T == 4
                                            "Data length (%ld) greater then what is possible in WMO header size, inserting maximum possible 99999999.",
#else
                                            "Data length (%lld) greater then what is possible in WMO header size, inserting maximum possible 99999999.",
#endif
                                             (pri_off_t)(length + end_length));
                             }
                             else
                             {
                                 (void)snprintf(length_indicator, 14,
                                                "%08lu",
                                                (unsigned long)(length + end_length + add_nnn_length));
                             }
                             length_indicator[8] = '0';
                             if (write(to_fd, length_indicator,
                                       start_length) != start_length)
                             {
                                receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                            _("Failed to write() to `%s' : %s"),
                                            new_name, strerror(errno));
                                CONVERT_CLEAN_UP();
                                (void)close(to_fd);
                                return(INCORRECT);
                             }
                             if (nnn_length > 0)
                             {
                                int  add_length;
                                char nnn[MAX_INT_LENGTH + 3 + 1];

                                (void)next_counter(counter_fd, counter,
                                                   my_power(10, nnn_length) - 1);
                                if (add_sohcrcrlf == YES)
                                {
                                   add_length = sprintf(nnn, "%0*d\r\r\n",
                                                        nnn_length, *counter);
                                }
                                else
                                {
                                   if (*ptr_start == 1)
                                   {
                                      nnn[0] = 1;
                                      add_offset = 1;
                                   }
                                   add_length = sprintf(&nnn[add_offset],
                                                        "\r\r\n%0*d",
                                                        nnn_length, *counter) +
                                                        add_offset;
                                }
                                if (write(to_fd, nnn, add_length) != add_length)
                                {
                                   receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                               _("Failed to write() to `%s' : %s"),
                                               new_name, strerror(errno));
                                   CONVERT_CLEAN_UP();
                                   (void)close(to_fd);
                                   return(INCORRECT);
                                }
                                *file_size += add_length;
                             }
#ifdef HAVE_STATX
                             if (writen(to_fd, ptr_start + add_offset,
                                       length - add_offset,
                                        stat_buf.stx_blksize) != (length - add_offset))
#else
                             if (writen(to_fd, ptr_start + add_offset,
                                       length - add_offset,
                                        stat_buf.st_blksize) != (length - add_offset))
#endif
                             {
                                receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                            _("Failed to writen() to `%s' : %s"),
                                            new_name, strerror(errno));
                                CONVERT_CLEAN_UP();
                                (void)close(to_fd);
                                return(INCORRECT);
                             }
                             if (end_length > 0)
                             {
                                length_indicator[0] = 13;
                                length_indicator[1] = 13;
                                length_indicator[2] = 10;
                                length_indicator[3] = 3;
                                if (write(to_fd, length_indicator,
                                          end_length) != end_length)
                                {
                                   receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                               _("Failed to write() to `%s' : %s"),
                                               new_name, strerror(errno));
                                   CONVERT_CLEAN_UP();
                                   (void)close(to_fd);
                                   return(INCORRECT);
                                }
                             }
                             *file_size += (start_length + length + end_length);
                          }
                       }
#ifdef HAVE_STATX
                    } while ((ptr - src_ptr + 1) < stat_buf.stx_size);
#else
                    } while ((ptr - src_ptr + 1) < stat_buf.st_size);
#endif
                 }
                 break;

              case MRZ2WMO :
#ifdef HAVE_STATX
                 if ((to_fd = open(new_name, (O_RDWR | O_CREAT | O_TRUNC),
                                   stat_buf.stx_mode)) == -1)
#else
                 if ((to_fd = open(new_name, (O_RDWR | O_CREAT | O_TRUNC),
                                   stat_buf.st_mode)) == -1)
#endif
                 {
                    receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                _("Failed to open() %s : %s"),
                                new_name, strerror(errno));
                    (void)close(from_fd);
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
                    (void)munmap((void *)src_ptr, stat_buf.stx_size);
# else
                    (void)munmap((void *)src_ptr, stat_buf.st_size);
# endif
#else
                    (void)munmap_emu((void *)src_ptr);
#endif
                    return(INCORRECT);
                 }

                 if ((*file_size = bin_file_convert(src_ptr,
#ifdef HAVE_STATX
                                                    stat_buf.stx_size,
#else
                                                    stat_buf.st_size,
#endif
                                                    to_fd, file_name,
                                                    job_id)) < 0)
                 {
                    receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                _("Failed to convert MRZ file `%s' to WMO-format."),
                                file_name);
                    *file_size = 0;
                 }
                 break;

              case ISO8859_2ASCII :
                 {
                    char *dst;

#ifdef HAVE_STATX
                    if ((to_fd = open(new_name, (O_RDWR | O_CREAT | O_TRUNC),
                                      stat_buf.stx_mode)) == -1)
#else
                    if ((to_fd = open(new_name, (O_RDWR | O_CREAT | O_TRUNC),
                                      stat_buf.st_mode)) == -1)
#endif
                    {
                       receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                   _("Failed to open() %s : %s"),
                                   new_name, strerror(errno));
                       CONVERT_CLEAN_UP();
                       return(INCORRECT);
                    }

#ifdef HAVE_STATX
                    if ((dst = malloc((stat_buf.stx_size * 3))) == NULL)
#else
                    if ((dst = malloc((stat_buf.st_size * 3))) == NULL)
#endif
                    {
                       receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                   _("malloc() error : %s"),
                                   strerror(errno));
                       CONVERT_CLEAN_UP();
                       (void)close(to_fd);
                       return(INCORRECT);
                    }
#ifdef HAVE_STATX
                    if ((*file_size = iso8859_2ascii(src_ptr, dst,
                                                     stat_buf.stx_size)) < 0)
#else
                    if ((*file_size = iso8859_2ascii(src_ptr, dst,
                                                     stat_buf.st_size)) < 0)
#endif
                    {
                       receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                   _("Failed to convert ISO8859 file `%s' to ASCII."),
                                   file_name);
                       *file_size = 0;
                    }
                    else
                    {
#ifdef HAVE_STATX
                       if (writen(to_fd, dst, *file_size,
                                  stat_buf.stx_blksize) != *file_size)
#else
                       if (writen(to_fd, dst, *file_size,
                                  stat_buf.st_blksize) != *file_size)
#endif
                       {
                          receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                      _("Failed to writen() to `%s' : %s"),
                                      new_name, strerror(errno));
                          CONVERT_CLEAN_UP();
                          (void)close(to_fd);
                          (void)free(dst);
                          return(INCORRECT);
                       }
                    }
                    (void)free(dst);
                 }
                 break;

              default            : /* Impossible! */
                 receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                             _("Unknown convert type (%d)."), type);
                 CONVERT_CLEAN_UP();
                 return(INCORRECT);
           }

#ifdef HAVE_MMAP
# ifdef HAVE_STATX
           if (munmap((void *)src_ptr, stat_buf.stx_size) == -1)
# else
           if (munmap((void *)src_ptr, stat_buf.st_size) == -1)
# endif
#else
           if (munmap_emu((void *)src_ptr) == -1)
#endif
           {
              receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                          _("Failed to munmap() `%s' : %s"), fullname, strerror(errno));
           }

           if (close(from_fd) == -1)
           {
              receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                          _("close() error : `%s'"), strerror(errno));
           }
           if (no_change == NO)
           {
              if (close(to_fd) == -1)
              {
                 receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                             _("close() error : `%s'"), strerror(errno));
              }
           }

           if (nnn_length > 0)
           {
              close_counter_file(counter_fd, &counter);
           }
        }

   /* Remove the file that has just been extracted. */
   if (no_change == NO)
   {
      if (unlink(fullname) < 0)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to unlink() `%s' : %s"), fullname, strerror(errno));
      }
      else
      {
         if (rename(new_name, fullname) == -1)
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                        _("Failed to rename() `%s' to `%s' : %s"),
                        new_name, fullname, strerror(errno));
         }
      }
      if (*file_size == 0)
      {
         receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                     _("No data converted in %s #%x"), file_name, job_id);
      }
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++++ unix2dos() ++++++++++++++++++++++++++++++*/
static int
unix2dos(char *source_file, char *dest_file, off_t *new_length)
{
   FILE *rfp;

   if ((rfp = fopen(source_file, "r")) == NULL)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to fopen() `%s' : %s"),
                  source_file, strerror(errno));
      return(INCORRECT);
   }
   else
   {
      FILE *wfp;

      if ((wfp = fopen(dest_file, "w+")) == NULL)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to fopen() `%s' : %s"),
                     dest_file, strerror(errno));
         (void)fclose(rfp);

         return(INCORRECT);
      }
      else
      {
         int prev_char = 0,
             tmp_char;

         while ((tmp_char = fgetc(rfp)) != EOF)
         {
            if ((tmp_char == 10) && (prev_char != 13))
            {
               prev_char = 13;
               if (fputc(prev_char, wfp) == EOF)
               {
                  receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                              _("fputc() error for file `%s' : %s"),
                              dest_file, strerror(errno));
                  (void)fclose(rfp);
                  (void)fclose(wfp);
                  if (unlink(dest_file) == 0)
                  {
                     *new_length = 0;
                  }

                  return(INCORRECT);
               }
               else
               {
                  (*new_length)++;
               }
            }
            if (fputc(tmp_char, wfp) == EOF)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           _("fputc() error for file `%s' : %s"),
                           dest_file, strerror(errno));
               (void)fclose(rfp);
               (void)fclose(wfp);
               if (unlink(dest_file) == 0)
               {
                  *new_length = 0;
               }

               return(INCORRECT);
            }
            else
            {
               (*new_length)++;
            }
            prev_char = tmp_char;
         }

         if (fclose(wfp) == EOF)
         {
            receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                        _("fclose() error for file `%s' : %s"),
                        dest_file, strerror(errno));
         }
      }

      if (fclose(rfp) == EOF)
      {
         receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                     _("fclose() error for file `%s' : %s"),
                     source_file, strerror(errno));
      }
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++++ dos2unix() ++++++++++++++++++++++++++++++*/
static int
dos2unix(char *source_file, char *dest_file, off_t *new_length)
{
   FILE *rfp;

   if ((rfp = fopen(source_file, "r")) == NULL)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to fopen() `%s' : %s"),
                  source_file, strerror(errno));
      return(INCORRECT);
   }
   else
   {
      FILE *wfp;

      if ((wfp = fopen(dest_file, "w+")) == NULL)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to fopen() `%s' : %s"),
                     dest_file, strerror(errno));
         (void)fclose(rfp);

         return(INCORRECT);
      }
      else
      {
         int prev_char = 0,
             tmp_char;

         while ((tmp_char = fgetc(rfp)) != EOF)
         {
            if ((tmp_char == 10) && (prev_char == 13))
            {
               if (fputc(tmp_char, wfp) == EOF)
               {
                  receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                              _("fputc() error for file `%s' : %s"),
                              dest_file, strerror(errno));
                  (void)fclose(rfp);
                  (void)fclose(wfp);
                  if (unlink(dest_file) == 0)
                  {
                     *new_length = 0;
                  }

                  return(INCORRECT);
               }
               else
               {
                  (*new_length)++;
               }
            }
            else
            {
               if (prev_char == 13)
               {
                  if (fputc(prev_char, wfp) == EOF)
                  {
                     receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                 _("fputc() error for file `%s' : %s"),
                                 dest_file, strerror(errno));
                     (void)fclose(rfp);
                     (void)fclose(wfp);
                     if (unlink(dest_file) == 0)
                     {
                        *new_length = 0;
                     }

                     return(INCORRECT);
                  }
                  else
                  {
                     (*new_length)++;
                  }
               }
               if (tmp_char != 13)
               {
                  if (fputc(tmp_char, wfp) == EOF)
                  {
                     receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                 _("fputc() error for file `%s' : %s"),
                                 dest_file, strerror(errno));
                     (void)fclose(rfp);
                     (void)fclose(wfp);
                     if (unlink(dest_file) == 0)
                     {
                        *new_length = 0;
                     }

                     return(INCORRECT);
                  }
                  else
                  {
                     (*new_length)++;
                  }
               }
            }
            prev_char = tmp_char;
         }

         if (fclose(wfp) == EOF)
         {
            receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                        _("fclose() error for file `%s' : %s"),
                        dest_file, strerror(errno));
         }
      }

      if (fclose(rfp) == EOF)
      {
         receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                     _("fclose() error for file `%s' : %s"),
                     source_file, strerror(errno));
      }
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++++ lf2crcrlf() +++++++++++++++++++++++++++++*/
static int
lf2crcrlf(char *source_file, char *dest_file, off_t *new_length)
{
   FILE *rfp;

   if ((rfp = fopen(source_file, "r")) == NULL)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to fopen() `%s' : %s"),
                  source_file, strerror(errno));
      return(INCORRECT);
   }
   else
   {
      FILE *wfp;

      if ((wfp = fopen(dest_file, "w+")) == NULL)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to fopen() `%s' : %s"),
                     dest_file, strerror(errno));
         (void)fclose(rfp);

         return(INCORRECT);
      }
      else
      {
         int prev_char = 0,
             prev_prev_char = 0,
             tmp_char,
             write_char = 13;

         while ((tmp_char = fgetc(rfp)) != EOF)
         {
            if ((tmp_char == 10) && (prev_char != 13) && (prev_prev_char != 13))
            {
               if (fputc(write_char, wfp) == EOF)
               {
                  receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                              _("fputc() error for file `%s' : %s"),
                              dest_file, strerror(errno));
                  (void)fclose(rfp);
                  (void)fclose(wfp);
                  if (unlink(dest_file) == 0)
                  {
                     *new_length = 0;
                  }

                  return(INCORRECT);
               }
               else
               {
                  (*new_length)++;
               }
               if (fputc(write_char, wfp) == EOF)
               {
                  receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                              _("fputc() error for file `%s' : %s"),
                              dest_file, strerror(errno));
                  (void)fclose(rfp);
                  (void)fclose(wfp);
                  if (unlink(dest_file) == 0)
                  {
                     *new_length = 0;
                  }

                  return(INCORRECT);
               }
               else
               {
                  (*new_length)++;
               }
            }
            if ((tmp_char == 10) && (prev_char == 13) && (prev_prev_char != 13))
            {
               if (fputc(write_char, wfp) == EOF)
               {
                  receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                              _("fputc() error for file `%s' : %s"),
                              dest_file, strerror(errno));
                  (void)fclose(rfp);
                  (void)fclose(wfp);
                  if (unlink(dest_file) == 0)
                  {
                     *new_length = 0;
                  }

                  return(INCORRECT);
               }
               else
               {
                  (*new_length)++;
               }
            }
            if (fputc(tmp_char, wfp) == EOF)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           _("fputc() error for file `%s' : %s"),
                           dest_file, strerror(errno));
               (void)fclose(rfp);
               (void)fclose(wfp);
               if (unlink(dest_file) == 0)
               {
                  *new_length = 0;
               }

               return(INCORRECT);
            }
            else
            {
               (*new_length)++;
            }
            prev_prev_char = prev_char;
            prev_char = tmp_char;
         }

         if (fclose(wfp) == EOF)
         {
            receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                        _("fclose() error for file `%s' : %s"),
                        dest_file, strerror(errno));
         }
      }

      if (fclose(rfp) == EOF)
      {
         receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                     _("fclose() error for file `%s' : %s"),
                     source_file, strerror(errno));
      }
   }

   return(SUCCESS);
}


/*++++++++++++++++++++++++++++ crcrlf2lf() ++++++++++++++++++++++++++++++*/
static int
crcrlf2lf(char *source_file, char *dest_file, off_t *new_length)
{
   FILE *rfp;

   if ((rfp = fopen(source_file, "r")) == NULL)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to fopen() `%s' : %s"),
                  source_file, strerror(errno));
      return(INCORRECT);
   }
   else
   {
      FILE *wfp;

      if ((wfp = fopen(dest_file, "w+")) == NULL)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to fopen() `%s' : %s"),
                     dest_file, strerror(errno));
         (void)fclose(rfp);

         return(INCORRECT);
      }
      else
      {
         int prev_char = 0,
             prev_prev_char = 0,
             tmp_char;

         while ((tmp_char = fgetc(rfp)) != EOF)
         {
            if ((tmp_char == 10) && (prev_char == 13) && (prev_prev_char == 13))
            {
               if (fputc(tmp_char, wfp) == EOF)
               {
                  receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                              _("fputc() error for file `%s' : %s"),
                              dest_file, strerror(errno));
                  (void)fclose(rfp);
                  (void)fclose(wfp);
                  if (unlink(dest_file) == 0)
                  {
                     *new_length = 0;
                  }

                  return(INCORRECT);
               }
               else
               {
                  (*new_length)++;
               }
            }
            else
            {
               if ((prev_char == 13) && (tmp_char != 13))
               {
                  if (fputc(prev_char, wfp) == EOF)
                  {
                     receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                 _("fputc() error for file `%s' : %s"),
                                 dest_file, strerror(errno));
                     (void)fclose(rfp);
                     (void)fclose(wfp);
                     if (unlink(dest_file) == 0)
                     {
                        *new_length = 0;
                     }

                     return(INCORRECT);
                  }
                  else
                  {
                     (*new_length)++;
                  }
               }
               if ((prev_char == 13) && (prev_prev_char == 13))
               {
                  if (fputc(prev_char, wfp) == EOF)
                  {
                     receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                 _("fputc() error for file `%s' : %s"),
                                 dest_file, strerror(errno));
                     (void)fclose(rfp);
                     (void)fclose(wfp);
                     if (unlink(dest_file) == 0)
                     {
                        *new_length = 0;
                     }

                     return(INCORRECT);
                  }
                  else
                  {
                     (*new_length)++;
                  }
               }
               if (tmp_char != 13)
               {
                  if (fputc(tmp_char, wfp) == EOF)
                  {
                     receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                 _("fputc() error for file `%s' : %s"),
                                 dest_file, strerror(errno));
                     (void)fclose(rfp);
                     (void)fclose(wfp);
                     if (unlink(dest_file) == 0)
                     {
                        *new_length = 0;
                     }

                     return(INCORRECT);
                  }
                  else
                  {
                     (*new_length)++;
                  }
               }
            }
            prev_prev_char = prev_char;
            prev_char = tmp_char;
         }

         if (fclose(wfp) == EOF)
         {
            receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                        _("fclose() error for file `%s' : %s"),
                        dest_file, strerror(errno));
         }
      }

      if (fclose(rfp) == EOF)
      {
         receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                     _("fclose() error for file `%s' : %s"),
                     source_file, strerror(errno));
      }
   }

   return(SUCCESS);
}
