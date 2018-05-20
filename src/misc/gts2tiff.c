/*
 *  gts2tiff.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2018 Deutscher Wetterdienst (DWD),
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
 **   gts2tiff - changes GTS (T4) files to TIFF files
 **
 ** SYNOPSIS
 **   off_t gts2tiff(char *path, char *filename)
 **            path     - path where the TIFF file can be found
 **            filename - file name of the TIFF file
 **
 ** DESCRIPTION
 **   Converts CCITT Group 3 or Group 4 encoded data to TIFF data.
 **   If the T4 file has a WMO vertical resolution factor of 2,
 **   all lines will be duplicated.
 **   
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to convert the file. Otherwise
 **   the size of the converted file is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.10.1996 H.Kiehl    Created
 **   13.11.1996 T.Freyberg Debugged function dup_count_eols().
 **   02.09.2002 H.Kiehl    Return file name with TIFF_END.
 **   20.11.2002 H.Kiehl    Don't mark T4 code wrong if it does not have
 **                         the 6 EOL's at end.
 **   05.02.2003 H.Kiehl    Remove the strlen() to get header length and
 **                         put in more size checks so we don't go beyond
 **                         our memory.
 **
 */
DESCR__E_M3

#include <stdio.h>             /* sprintf()                              */
#include <string.h>            /* strcat(), memcpy()                     */
#include <stdlib.h>            /* exit()                                 */
#include <unistd.h>            /* lseek(), unlink()                      */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>         /* mmap(), munmap()                       */
#endif
#if defined (_TIFF_CONVERSION_TIME_TEST) || defined (_EOL_TIME_TEST)
# include <sys/times.h>
#endif
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"

#define TIFF_END      "-tiff"
#define NO_OF_TAGS    14
#define TIFF_SHORT    3
#define TIFF_LONG     4
#define TIFF_RATIONAL 5

#ifndef MAP_FILE    /* Required for BSD.          */
# define MAP_FILE 0 /* All others do not need it. */
#endif

/* Global local variables. */
static int  hr,                    /* Horizontal resolution  */
            vr,                    /* Vertical resolution    */
            bhl;                   /* Bulletin header length */
#ifdef _WITH_FILE_INFO
char        bh[100];
#endif

/* Local function prototypes. */
static int evaluate_wmo_stuff(char *, size_t),
           dup_count_eols(char *, size_t, char *),
#ifdef _EOL_TIME_TEST
           count_eols1(char *, size_t),
           count_eols2(char *, size_t),
#endif
           count_eols(char *, size_t);


/*############################## gts2tiff() #############################*/
off_t
gts2tiff(char *path, char *filename)
{
#ifdef _EOL_TIME_TEST
   register    i;
   clock_t     start_time,
               end_time;
#endif
#ifdef _TIFF_CONVERSION_TIME_TEST
   clock_t     start_total_time,
               end_total_time;
#endif
#if defined (_TIFF_CONVERSION_TIME_TEST) || defined (_EOL_TIME_TEST)
   clock_t     clktck;
   struct tms  tmsdummy;
#endif
   int         byte_order = 1,
               word_offset,
               total_size,
               fdin,
               fdout,
               ifd_offset,
               offset,
               no_of_eols;
   size_t      tiff_file_size;
   char        *src,
               *dst,
               from[MAX_PATH_LENGTH],
               to[MAX_PATH_LENGTH + 5];
   struct stat stat_buf;

#if defined (_TIFF_CONVERSION_TIME_TEST) || defined (_EOL_TIME_TEST)
   if ((clktck = sysconf(_SC_CLK_TCK)) <= 0)
   {
      (void)fprintf(stderr,
                    _("ERROR   : Could not get clock ticks per second : %s (%s %d)\n"),
                    strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }
#endif

#ifdef _TIFF_CONVERSION_TIME_TEST
   start_total_time = times(&tmsdummy);
#endif

   /* Open source file. */
   if (path[0] == '\0')
   {
      (void)strcpy(from, filename);
   }
   else
   {
      (void)snprintf(from, MAX_PATH_LENGTH, "%s/%s", path, filename);
   }
   if ((fdin = open(from, O_RDONLY)) < 0)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Could not open() `%s' for copying : %s"),
                  from, strerror(errno));
      return(INCORRECT);
   }

   if (fstat(fdin, &stat_buf) < 0)   /* need size of input file */
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Could not fstat() `%s' : %s"), from, strerror(errno));
      (void)close(fdin);
      return(INCORRECT);
   }

   /* Open and create destination file. */
   (void)snprintf(to, MAX_PATH_LENGTH + 5, "%s%s", from, TIFF_END);
   if ((fdout = open(to, (O_RDWR | O_CREAT | O_TRUNC), stat_buf.st_mode)) < 0)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Could not open() `%s' for copying : %s"),
                  to, strerror(errno));
      (void)close(fdin);
      return(INCORRECT);
   }

   if (stat_buf.st_size > 0)
   {
#ifdef HAVE_MMAP
      if ((src = mmap(NULL, stat_buf.st_size, PROT_READ,
                      (MAP_FILE | MAP_SHARED), fdin, 0)) == (caddr_t) -1)
#else
      if ((src = mmap_emu(NULL, stat_buf.st_size, PROT_READ,
                          (MAP_FILE | MAP_SHARED), from, 0)) == (caddr_t) -1)
#endif
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Could not mmap() file `%s' : %s"),
                     from, strerror(errno));
         (void)close(fdin);
         (void)close(fdout);
         if (unlink(to) < 0)
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                        _("Failed to unlink() `%s' : %s"), to, strerror(errno));
         }
         return(INCORRECT);
      }
      /*
       * WARNING: Do NOT close the memory mapped file yet!
       */

      if (evaluate_wmo_stuff(src, stat_buf.st_size) < 0)
      {
#ifdef HAVE_MMAP
         if (munmap(src, stat_buf.st_size) < 0)
#else
         if (munmap_emu(src) < 0)
#endif
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                        _("munmap() error : %s"), strerror(errno));
         }
         (void)close(fdin);
         (void)close(fdout);
         if (unlink(to) < 0)
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                        _("Failed to unlink() `%s' : %s"), to, strerror(errno));
         }
         return(INCORRECT);
      }

      if (vr == 2)
      {
         total_size = (2 * stat_buf.st_size) - bhl - 4;
      }
      else
      {
         total_size = stat_buf.st_size;
      }
      switch (total_size % 4)
      {
         case 0: word_offset = 2;
                 break;
         case 1: word_offset = 1;
                 break;
         case 2: word_offset = 0;
                 break;
         case 3: word_offset = 3;
                 break;
         default: receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                              _("Impossible!!!!"));
                  (void)close(fdin);
                  (void)close(fdout);
                  if (unlink(to) < 0)
                  {
                     receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                 _("Failed to unlink() `%s' : %s"),
                                 to, strerror(errno));
                  }
                  return(INCORRECT);
      }
      tiff_file_size = 8 +                      /* TIFF Header         */
                       total_size +             /* T4 code + WMO stuff */
                       word_offset +            /* word offset for IFD */
                       (2 + (NO_OF_TAGS * 12) + 4) +
                                                /* IFD                 */
                       8 +                      /* X-Resolution        */
                       8;                       /* Y-Resolution        */

      /* Set size of output file. */
      if (lseek(fdout, tiff_file_size - 1, SEEK_SET) == -1)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Could not lseek() on `%s' : %s"), to, strerror(errno));
         (void)close(fdin);
         (void)close(fdout);
         if (unlink(to) < 0)
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                        _("Failed to unlink() `%s' : %s"), to, strerror(errno));
         }
         return(INCORRECT);
      }
      if (write(fdout, "", 1) != 1)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Could not write() to `%s' : %s"), to, strerror(errno));
         (void)close(fdin);
         (void)close(fdout);
         if (unlink(to) < 0)
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                        _("Failed to unlink() `%s' : %s"), to, strerror(errno));
         }
         return(INCORRECT);
      }

#ifdef HAVE_MMAP
      if ((dst = mmap(NULL, tiff_file_size, (PROT_READ | PROT_WRITE),
                      (MAP_FILE |  MAP_SHARED), fdout, 0)) == (caddr_t) -1)
#else
      if ((dst = mmap_emu(NULL, tiff_file_size, (PROT_READ | PROT_WRITE),
                          (MAP_FILE |  MAP_SHARED), to, 0)) == (caddr_t) -1)
#endif
      {
#ifdef HAVE_MMAP
         if (munmap(src, stat_buf.st_size) < 0)
#else
         if (munmap_emu(src) < 0)
#endif
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                        _("munmap() error : %s"), strerror(errno));
         }
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Could not mmap() file `%s' : %s"), to, strerror(errno));
         (void)close(fdin);
         (void)close(fdout);
         if (unlink(to) < 0)
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                        _("Failed to unlink() `%s' : %s"), to, strerror(errno));
         }
         return(INCORRECT);
      }

#ifdef _EOL_TIME_TEST
      /* Lets take a look how long it takes to find the EOL's. */
      no_of_eols = 0;
      start_time = times(&tmsdummy);
      for (i = 0; i < 100; i++)
      {
         no_of_eols += count_eols(&src[bhl], (stat_buf.st_size - bhl - 4));
      }
      end_time = times(&tmsdummy);
      (void)fprintf(stderr, "Ver 0       : %7.2f for %d EOL's\n", (end_time - start_time) / (double)clktck, no_of_eols);


      if (*(char *)&byte_order == 1)
      {
         no_of_eols = 0;
         start_time = times(&tmsdummy);
         for (i = 0; i < 100; i++)
         {
            no_of_eols += count_eols2(&src[bhl], (stat_buf.st_size - bhl - 4));
         }
         end_time = times(&tmsdummy);
      }
      else
      {
         no_of_eols = 0;
         start_time = times(&tmsdummy);
         for (i = 0; i < 100; i++)
         {
            no_of_eols += count_eols1(&src[bhl], (stat_buf.st_size - bhl - 4));
         }
         end_time = times(&tmsdummy);
      }
      (void)fprintf(stderr, "Ver 1       : %7.2f for %d EOL's\n", (end_time - start_time) / (double)clktck, no_of_eols);

      /* See how long it takes to duplicate all lines. */
      no_of_eols = 0;
      start_time = times(&tmsdummy);
      for (i = 0; i < 100; i++)
      {
         no_of_eols += dup_count_eols(src, stat_buf.st_size, dst);
      }
      end_time = times(&tmsdummy);
      (void)fprintf(stderr, "duplication : %7.2f for %d EOL's\n", (end_time - start_time) / (double)clktck, no_of_eols);
#endif /* _EOL_TIME_TEST */

      if (vr == 2)
      {
         if ((no_of_eols = dup_count_eols(src, stat_buf.st_size, dst)) < 1)
         {
            if (no_of_eols < 0)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           _("T4-code is corrupt since there were no 6 EOL's in a row."));
            }
            else
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           _("There are no EOL's."));
            }
#ifdef HAVE_MMAP
            if (munmap(src, stat_buf.st_size) < 0)
#else
            if (munmap_emu(src) < 0)
#endif
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           _("munmap() error : %s"), strerror(errno));
            }
            (void)close(fdin);
            (void)close(fdout);
            if (unlink(to) < 0)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           _("Failed to unlink() `%s` : %s"), to, strerror(errno));
            }
            return(INCORRECT);
         }
      }
      else
      {
         if ((no_of_eols = count_eols(&src[bhl], (stat_buf.st_size - bhl - 4))) < 1)
         {
            if (no_of_eols < 0)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           _("T4-code is corrupt since there were no 6 EOL's in a row."));
            }
            else
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           _("There are no EOL's"));
            }
#ifdef HAVE_MMAP
            if (munmap(src, stat_buf.st_size) < 0)
#else
            if (munmap_emu(src) < 0)
#endif
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           _("munmap() error : %s"), strerror(errno));
            }
            (void)close(fdin);
            (void)close(fdout);
            if (unlink(to) < 0)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           _("Failed to unlink() `%s' : %s"),
                           to, strerror(errno));
            }
            return(INCORRECT);
         }
      }
   }
   else
   {
      if (close(fdin) == -1)
      {
         receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                     _("close() error : %s"), strerror(errno));
      }
      if (close(fdout) == -1)
      {
         receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                     _("close() error : %s"), strerror(errno));
      }
      if (unlink(to) < 0)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to unlink() `%s' : %s"), to, strerror(errno));
      }
      receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                  _("Unusable data size (%d) for file `%s'"),
                  stat_buf.st_size, from);
      return(stat_buf.st_size);
   }

   /*
    * Create TIFF-header
    */
   if (*(char *)&byte_order == 1)
   {
      /* little-endian */
      dst[0] = dst[1] = 'I';

      /* TIFF identifier */
      dst[2] = 42;
      dst[3] = 0;
   }
   else
   {
      /* big-endian */
      dst[0] = dst[1] = 'M';

      /* TIFF identifier */
      dst[2] = 0;
      dst[3] = 42;
   }

   /* Insert offset to the first IFD as follows:              */
   /* 8 Byte TIFF header + T4 contents + offset to next word */
   ifd_offset = 4 + 4 + total_size + word_offset;
   *(int *)&dst[4] = ifd_offset;

   if (vr != 2)
   {
      /*
       * Now lets copy the T4 plus WMO data to the TIFF file.
       */
      (void)memcpy(&dst[8], src, stat_buf.st_size);
   }

   /*
    * Create the IFD of the TIFF-file.
    */
   *(short *)&dst[ifd_offset] = NO_OF_TAGS;    /* write no. of tags */

   /* New subfile type */
   *(short *)&dst[ifd_offset + 2] = 254;       /* Tag ID            */
   *(short *)&dst[ifd_offset + 4] = TIFF_LONG; /* Type              */
   *(int *)&dst[ifd_offset + 6]   = 1;         /* Count             */
   *(int *)&dst[ifd_offset + 10]  = 0;         /* Data              */
   offset = ifd_offset + 10 + 4;

   /* Image width */
   *(short *)&dst[offset]      = 256;          /* Tag ID            */
   *(short *)&dst[offset + 2]  = TIFF_SHORT;   /* Type              */
   *(int *)  &dst[offset + 4]  = 1;            /* Count             */
   *(short *)&dst[offset + 8]  = hr;           /* Data              */
   *(short *)&dst[offset + 10] = 0;            /* Data              */
   offset += 12;

   /* Image length */
   *(short *)&dst[offset]      = 257;          /* Tag ID            */
   *(short *)&dst[offset + 2]  = TIFF_SHORT;   /* Type              */
   *(int *)  &dst[offset + 4]  = 1;            /* Count             */
   *(short *)&dst[offset + 8]  = no_of_eols;   /* Data              */
   *(short *)&dst[offset + 10] = 0;            /* Data              */
   offset += 12;

   /* Compression */
   *(short *)&dst[offset]      = 259;          /* Tag ID            */
   *(short *)&dst[offset + 2]  = TIFF_SHORT;   /* Type              */
   *(int *)  &dst[offset + 4]  = 1;            /* Count             */
   *(short *)&dst[offset + 8]  = 3;            /* Data              */
   *(short *)&dst[offset + 10] = 0;            /* Data              */
   offset += 12;

   /* Photometric interpretation */
   *(short *)&dst[offset]      = 262;          /* Tag ID            */
   *(short *)&dst[offset + 2]  = TIFF_SHORT;   /* Type              */
   *(int *)  &dst[offset + 4]  = 1;            /* Count             */
   *(int *)  &dst[offset + 8]  = 0;            /* Data              */
   offset += 12;

   /* Fill order (Bits in a byte) */
   *(short *)&dst[offset]      = 266;          /* Tag ID            */
   *(short *)&dst[offset + 2]  = TIFF_SHORT;   /* Type              */
   *(int *)  &dst[offset + 4]  = 1;            /* Count             */
   *(short *)&dst[offset + 8]  = 1;            /* Data              */
   *(short *)&dst[offset + 10] = 0;            /* Data              */
   offset += 12;

   /* Strip offset */
   *(short *)&dst[offset]      = 273;          /* Tag ID            */
   *(short *)&dst[offset + 2]  = TIFF_LONG;    /* Type              */
   *(int *)  &dst[offset + 4]  = 1;            /* Count             */
   *(int *)  &dst[offset + 8]  = 8 + bhl;      /* Data              */
   offset += 12;

   /* Orientation */
   *(short *)&dst[offset]      = 274;          /* Tag ID            */
   *(short *)&dst[offset + 2]  = TIFF_SHORT;   /* Type              */
   *(int *)  &dst[offset + 4]  = 1;            /* Count             */
   *(short *)&dst[offset + 8]  = 1;            /* Data              */
   *(short *)&dst[offset + 10] = 0;            /* Data              */
   offset += 12;

   /* Samples/pixels */
   *(short *)&dst[offset]      = 277;          /* Tag ID            */
   *(short *)&dst[offset + 2]  = TIFF_SHORT;   /* Type              */
   *(int *)  &dst[offset + 4]  = 1;            /* Count             */
   *(short *)&dst[offset + 8]  = 1;            /* Data              */
   *(short *)&dst[offset + 10] = 0;            /* Data              */
   offset += 12;

   /* Rows/strip */
   *(short *)&dst[offset]      = 278;          /* Tag ID            */
   *(short *)&dst[offset + 2]  = TIFF_SHORT;   /* Type              */
   *(int *)  &dst[offset + 4]  = 1;            /* Count             */
   *(short *)&dst[offset + 8]  = no_of_eols;   /* Data              */
   *(short *)&dst[offset + 10] = 0;            /* Data              */
   offset += 12;

   /* Strip byte count */
   *(short *)&dst[offset]      = 279;          /* Tag ID            */
   *(short *)&dst[offset + 2]  = TIFF_LONG;    /* Type              */
   *(int *)  &dst[offset + 4]  = 1;            /* Count             */
   *(int *)  &dst[offset + 8]  = total_size;   /* Data              */
   offset += 12;

   /* X-Resolution */
   *(short *)&dst[offset]      = 282;          /* Tag ID            */
   *(short *)&dst[offset + 2]  = TIFF_RATIONAL;/* Type              */
   *(int *)  &dst[offset + 4]  = 1;            /* Count             */
   *(int *)  &dst[offset + 8]  = tiff_file_size - 16; /* Offset     */
   *(int *)  &dst[tiff_file_size - 16] = 803721;      /* 1728 / 21,5*/
   *(int *)  &dst[tiff_file_size - 12] = 10000;
   offset += 12;

   /* Y-Resolution */
   *(short *)&dst[offset]      = 283;          /* Tag ID            */
   *(short *)&dst[offset + 2]  = TIFF_RATIONAL;/* Type              */
   *(int *)  &dst[offset + 4]  = 1;            /* Count             */
   *(int *)  &dst[offset + 8]  = tiff_file_size - 8;  /* Offset     */
   *(int *)&dst[tiff_file_size - 8] = 770000;
   *(int *)&dst[tiff_file_size - 4] = 10000;
   offset += 12;

   /* Resolution unit */
   *(short *)&dst[offset]      = 296;          /* Tag ID            */
   *(short *)&dst[offset + 2]  = TIFF_SHORT;   /* Type              */
   *(int *)  &dst[offset + 4]  = 1;            /* Count             */
   *(short *)&dst[offset + 8]  = 3;            /* Data              */
   *(short *)&dst[offset + 10] = 0;            /* Data              */
   offset += 12;

#ifdef _WITH_FILE_INFO
   receive_log(INFO_SIGN, __FILE__, __LINE__, 0L,
               _("T4 to TIFF conversion : %s %s"), filename, bh);
   receive_log(INFO_SIGN, __FILE__, __LINE__, 0L,
               "                      : hr = %d  vr = %d  eol's = %d",
               hr, vr, no_of_eols);
#endif

#ifdef HAVE_MMAP
   if (munmap(src, stat_buf.st_size) < 0)
#else
   if (munmap_emu(src) < 0)
#endif
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("munmap() error : %s"), strerror(errno));
      (void)close(fdin);
      (void)close(fdout);
      return(INCORRECT);
   }
#ifdef HAVE_MMAP
   if (munmap(dst, tiff_file_size) < 0)
#else
   if (munmap_emu(dst) < 0)
#endif
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("munmap() error : %s"), strerror(errno));
      (void)close(fdin);
      (void)close(fdout);
      return(INCORRECT);
   }

   /* Time to remove the file with T4 code only */
   if (unlink(from) < 0)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to unlink() `%s' : %s"), from, strerror(errno));
   }

#ifdef _TIFF_CONVERSION_TIME_TEST
   end_total_time = times(&tmsdummy);
   (void)fprintf(stderr, "Total time to convert file: %7.2f s\n", (end_total_time - start_total_time) / (double)clktck);
#endif

   if (close(fdin) == -1)
   {
      receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                  _("close() error : %s"), strerror(errno));
   }
   if (close(fdout) == -1)
   {
      receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                  _("close() error : %s"), strerror(errno));
   }

   /* Append TIFF ending to the original filename. */
   (void)strcat(filename, TIFF_END);

   return(tiff_file_size);
}


/*++++++++++++++++++++++++ evaluate_wmo_stuff() +++++++++++++++++++++++++*/
static int
evaluate_wmo_stuff(char *buf, size_t size)
{
   char *ptr,
        search_str[3];
#ifdef _WITH_FILE_INFO
   char *bh_start,
        *bh_end;
#endif

   search_str[0] = search_str[1] = 13;
   search_str[2] = 10;

   if ((ptr = lposi(buf, search_str, 3)) != NULL)
   {
      if ((ptr - buf) > size)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("This file does not seem to have a valid WMO header."));
         return(INCORRECT);
      }
      if ((ptr = lposi(ptr, search_str, 3)) != NULL)
      {
         char *tmp_ptr;

#ifdef _WITH_FILE_INFO
         bh_start = ptr - 1;
#endif
         if ((ptr - buf) > size)
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                        _("This file does not seem to have a valid WMO header."));
            return(INCORRECT);
         }
         if ((tmp_ptr = lposi(ptr, search_str, 3)) != NULL)
         {
            ptr = tmp_ptr;

            /* In case it is NULL, just assume counter is missing */
            /* and continue.                                      */
         }
         if ((ptr + 8 - buf) > size)
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                        _("This file does not seem to have a valid WMO header."));
            return(INCORRECT);
         }
#ifdef _WITH_FILE_INFO
         bh_end = ptr;
         (void)strncpy(bh, bh_start, (bh_end - bh_start - 4));
#endif
      }
      else
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to find second <CR><CR><LF>."));
         return(INCORRECT);
      }
   }
   else
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to find first <CR><CR><LF>."));
      return(INCORRECT);
   }

   /* Get the vertical and horizontal resolution. */
   vr = 0;
   ptr--;
   if (ptr[0] == 'D')
   {
      if (ptr[1] == 'F')
      {
         if (ptr[2] == 'A')
         {
            if (ptr[3] == 'X')
            {
               vr = ptr[7] - 48;
               if (ptr[6] == '6')
               {
                  hr = 1728;
               }
               else if (ptr[6] == '7')
                    {
                       hr = 3456;
                    }
               else if (ptr[6] == '8')
                    {
                       hr = 2432;
                    }
                    else
                    {
                       receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                   _("Unknown horizontal resolution of %c."),
                                   ptr[6]);
                       return(INCORRECT);
                    }
            }
         }
      }
   }
   if ((vr != 2) && (vr != 4))
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to get the vertical resolution (%d)."), vr);
      return(INCORRECT);
   }

   /* Determine the bulletin header length. */
   bhl = ptr + 8 - buf;

   /* Check end for <CR><CR><LF><ETX> */
   if ((buf[(size - 1)] != 3) || (buf[(size - 2)] != 10) ||
       (buf[(size - 3)] != 13) || (buf[(size - 4)] != 13))
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to locate bulletin end."));
      return(INCORRECT);
   }

   return(SUCCESS);
}


/*++++++++++++++++++++++++++++ count_eols() +++++++++++++++++++++++++++++*/
static int
count_eols(char *buf, size_t size)
{
   static int    no_of_eols;
   unsigned char shifter;
   unsigned int  count = 0;
   register int  i,
                 zero_hit = 0,
                 last_was_eol = 0;

   no_of_eols = 0;
   while (count < size)
   {
      shifter = buf[count];

      for (i = 0; i < 8; i++)
      {
         if ((shifter & 0x80) == 0)
         {
            if (zero_hit++ == 11)
            {
               last_was_eol = 0;
            }
         }
         else
         {
            if (zero_hit >= 11)
            {
               if (++last_was_eol == 6)
               {
                  return((no_of_eols - 6)); /* 1 at start and 6 at end */
               }
               no_of_eols++;
               zero_hit = 0;
            }
            else
            {
               zero_hit = last_was_eol = 0;
            }
         }
         shifter = shifter << 1;
      }
      count++;
   }

   if (no_of_eols > 0)
   {
      receive_log(INFO_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to read 6 EOL's in a row, after reading %d EOL's [last_was_eol = %d]."),
                  no_of_eols, last_was_eol);
      return((no_of_eols - 7));
   }

   /* Since we did not read 6 EOL's in a row, lets assume */
   /* the T4-code is corrupt.                             */
   return(INCORRECT);
}


/*++++++++++++++++++++++++++ dup_count_eols() +++++++++++++++++++++++++++*/
static int
dup_count_eols(char *buf, size_t size, char *dst)
{
   static int    no_of_eols;
   int           bit_offset_start = -1,
                 byte_offset_start = -1;
   unsigned char d_shifter = 0,      /* destination shifter        */
                 s_shifter,          /* source shifter             */
                 s_shifter_copy;     /* source shifter when coping */
   unsigned int  d_count = 8 + bhl,
                 s_count = bhl;
   register int  i,
                 j,
                 k,
                 bit_count = 0,
                 zero_hit = 0,
                 last_was_eol = 0;

   /*
    * Copy bulletin header.
    */
   (void)memcpy(&dst[8], buf, bhl);

   no_of_eols = 0;
   while (s_count < size)
   {
      s_shifter = buf[s_count];

      for (i = 0; i < 8; i++)
      {
         if ((s_shifter & 0x80) == 0)
         {
            if (zero_hit++ == 11)
            {
               last_was_eol = 0;
            }
            s_shifter = s_shifter << 1;

            if (++bit_count == 8)
            {
               dst[d_count++] = d_shifter;
               bit_count = d_shifter = 0;
            }
            else
            {
               d_shifter = d_shifter << 1;
            }
         }
         else
         {
            /* Copy bit */
            d_shifter |= 0x01;

            s_shifter = s_shifter << 1;

            if (++bit_count == 8)
            {
               dst[d_count++] = d_shifter;
               bit_count = d_shifter = 0;
            }
            else
            {
               d_shifter = d_shifter << 1;
            }


            if (zero_hit >= 11)
            {
               if (bit_offset_start == -1)
               {
                  bit_offset_start = i + 1;
                  byte_offset_start = s_count;
               }
               else
               {
                  /* Time to duplicate last line? */
                  if (last_was_eol == 0)
                  {
                     /*
                      * Duplicate last line
                      */
                     /* Copy bits in first byte we have to copy. */
                     s_shifter_copy = buf[byte_offset_start];
                     s_shifter_copy = s_shifter_copy << bit_offset_start;
                     for (j = bit_offset_start; j < 8; j++)
                     {
                        if ((s_shifter_copy & 0x80) != 0)
                        {
                           d_shifter |= 0x01;
                        }
                        s_shifter_copy = s_shifter_copy << 1;
                        if (++bit_count == 8)
                        {
                           dst[d_count++] = d_shifter;
                           bit_count = d_shifter = 0;
                        }
                        else
                        {
                           d_shifter = d_shifter << 1;
                        }
                     }

                     /* Copy all bytes between start and end. */
                     for (j = (byte_offset_start + 1); j < s_count; j++)
                     {
                        s_shifter_copy = buf[j];

                        for (k = 0; k < 8; k++)
                        {
                           if ((s_shifter_copy & 0x80) != 0)
                           {
                              d_shifter |= 0x01;
                           }
                           s_shifter_copy = s_shifter_copy << 1;
                           if (++bit_count == 8)
                           {
                              dst[d_count++] = d_shifter;
                              bit_count = d_shifter = 0;
                           }
                           else
                           {
                              d_shifter = d_shifter << 1;
                           }
                        }
                     }

                     /* Copy bits in last byte. */
                     s_shifter_copy = buf[s_count];
                     for (j = 0; j < (i + 1); j++)
                     {
                        if ((s_shifter_copy & 0x80) != 0)
                        {
                           d_shifter |= 0x01;
                        }
                        s_shifter_copy = s_shifter_copy << 1;
                        if (++bit_count == 8)
                        {
                           dst[d_count++] = d_shifter;
                           bit_count = d_shifter = 0;
                        }
                        else
                        {
                           d_shifter = d_shifter << 1;
                        }
                     }

                     /* Remember the start offset for the next line. */
                     if (i == 7)
                     {
                        bit_offset_start = 0;
                        byte_offset_start = s_count + 1;
                     }
                     else
                     {
                        bit_offset_start = i + 1;
                        byte_offset_start = s_count;
                     }

                     no_of_eols++;
                  }

                  if (++last_was_eol == 6)
                  {
                     /* Don't forget to shift in destination buffer. */
                     if (bit_count != 0)
                     {
                        d_shifter = d_shifter << (8 - bit_count);
                        dst[d_count++] = d_shifter;
                     }

                     /* Write bulletin end */
                     dst[d_count] = dst[d_count + 1] = 13;
                     dst[d_count + 2] = 10;
                     dst[d_count + 3] = '\0';

                     return((no_of_eols - 6)); /* 1 at start and 5 at end */
                  }
               }
               no_of_eols++;
               zero_hit = 0;
            }
            else
            {
               zero_hit = last_was_eol = 0;
            }
         }
      }
      s_count++;
   }

   if (no_of_eols > 0)
   {
      receive_log(INFO_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to read 6 EOL's in a row, after reading %d EOL's [last_was_eol = %d]."),
                  no_of_eols, last_was_eol);

      /* Don't forget to shift in destination buffer. */
      if (bit_count != 0)
      {
         d_shifter = d_shifter << (8 - bit_count);
         dst[d_count++] = d_shifter;
      }

      /* Write bulletin end */
      dst[d_count] = dst[d_count + 1] = 13;
      dst[d_count + 2] = 10;
      dst[d_count + 3] = '\0';

      return((no_of_eols - 7)); /* 1 at start and 5 at end */
   }

   /* Since we did not read 6 EOL's in a row, lets assume */
   /* the T4-code is corrupt.                             */
   return(INCORRECT);
}


#ifdef _EOL_TIME_TEST
/*+++++++++++++++++++++++++++ count_eols1() +++++++++++++++++++++++++++++*/
/*
 * NOTE : only for MM
 */
static int
count_eols1(char *buf, size_t size)
{
   static int   no_of_eols;
   unsigned int shifter,
                count = 0;
   register     i,
                zero_hit = 0,
                last_was_eol = 0;

   no_of_eols = 0;
   while (count < size)
   {
      (void)memcpy(&shifter, &buf[count], 4);

      for (i = 0; i < 32; i++)
      {
         if ((shifter & 0x80000000) == 0)
         {
            if (zero_hit++ == 11)
            {
               last_was_eol = 0;
            }
         }
         else
         {
            if (zero_hit >= 11)
            {
               if (++last_was_eol == 6)
               {
                  return((no_of_eols - 6)); /* 1 at start and 5 at end */
               }
               no_of_eols++;
               zero_hit = 0;
            }
            else
            {
               zero_hit = last_was_eol = 0;
            }
         }
         shifter = shifter << 1;
      }

      count += 4;
   }

   /* Since we did not read 6 EOL's in a row, lets assume */
   /* the T4-code is corrupt.                             */
   return(INCORRECT);
}


/*+++++++++++++++++++++++++++ count_eols2() +++++++++++++++++++++++++++++*/
/*
 * NOTE : only for II
 */
static int
count_eols2(char *buf, size_t size)
{
   unsigned char tmp_byte;
   static int    no_of_eols;
   unsigned int  shifter,
                 count = 0;
   register      i,
                 zero_hit = 0,
                 last_was_eol = 0;

   no_of_eols = 0;
   while (count < size)
   {
      (void)memcpy(&shifter, &buf[count], 4);

      /* Swap bytes */
      tmp_byte = ((char *)&shifter)[0];
      ((char *)&shifter)[0] = ((char *)&shifter)[3];
      ((char *)&shifter)[3] = tmp_byte;
      tmp_byte = ((char *)&shifter)[1];
      ((char *)&shifter)[1] = ((char *)&shifter)[2];
      ((char *)&shifter)[2] = tmp_byte;

      for (i = 0; i < 32; i++)
      {
         if ((shifter & 0x80000000) == 0)
         {
            if (zero_hit++ == 11)
            {
               last_was_eol = 0;
            }
         }
         else
         {
            if (zero_hit >= 11)
            {
               if (++last_was_eol == 6)
               {
                  return((no_of_eols - 6)); /* 1 at start and 5 at end */
               }
               no_of_eols++;
               zero_hit = 0;
            }
            else
            {
               zero_hit = last_was_eol = 0;
            }
         }
         shifter = shifter << 1;
      }

      count += 4;
   }

   /* Since we did not read 6 EOL's in a row, lets assume */
   /* the T4-code is corrupt.                             */
   return(INCORRECT);
}
#endif /* _EOL_TIME_TEST */
