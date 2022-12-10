/*
 *  tiff_sniffer.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2022 Deutscher Wetterdienst (DWD),
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

DESCR__S_M1
/*
 ** NAME
 **   tiff_sniffer - program to show the TIFF header
 **
 ** SYNOPSIS
 **   tiff_sniffer <file name>
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   21.10.1996 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>            /* fprintf()                               */
#include <string.h>           /* memcpy()                                */
#include <stdlib.h>           /* malloc(), free(), exit()                */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>
#endif
#include <fcntl.h>
#include <errno.h>
#include "version.h"

#ifndef MAP_FILE    /* Required for BSD          */
#define MAP_FILE 0  /* All others do not need it */
#endif

static void byte_swap_short(char *),
            byte_swap_word(char *);
#ifndef HAVE_MMAP
int        sys_log_fd = STDERR_FILENO;
char       *p_work_dir = NULL;
#endif
const char *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int          i = 0,
                j,
                fd,
                offset,
                ifd_offset,
                byte_order = 1,
                swap_bytes_flag,
                tmp_int,
                count,
                length;
   short        no_of_dirs,
                tag_id,
                tmp_short;
   char         *src,
                *buf;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   CHECK_FOR_VERSION(argc, argv);

   if (argc != 2)
   {
      (void)fprintf(stderr, "Usage : %s <TIFF-file>\n", argv[0]);
      exit(INCORRECT);
   }

   if ((fd = open(argv[1], O_RDONLY)) < 0)
   {
      (void)fprintf(stderr, "ERROR   : Failed to open %s : %s\n",
                    argv[1], strerror(errno));
      exit(INCORRECT);
   }

   /* Need size of input file. */
#ifdef HAVE_STATX
   if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(fd, &stat_buf) == -1)
#endif
   {
      (void)fprintf(stderr, "ERROR   : Could not access on %s : %s\n",
                    argv[1], strerror(errno));
      (void)close(fd);
      exit(INCORRECT);
   }

#ifdef HAVE_STATX
   if (stat_buf.stx_size > 0)
#else
   if (stat_buf.st_size > 0)
#endif
   {
#ifdef HAVE_MMAP
      if ((src = mmap(0,
# ifdef HAVE_STATX
                      stat_buf.stx_size, PROT_READ,
# else
                      stat_buf.st_size, PROT_READ,
# endif
                      (MAP_FILE | MAP_SHARED), fd, 0)) == (caddr_t) -1)
#else
      if ((src = mmap_emu(0,
# ifdef HAVE_STATX
                          stat_buf.stx_size, PROT_READ, (MAP_FILE | MAP_SHARED),
# else
                          stat_buf.st_size, PROT_READ, (MAP_FILE | MAP_SHARED),
# endif
                          argv[1], 0)) == (caddr_t) -1)
#endif
      {
         (void)fprintf(stderr, "ERROR   : Could not mmap() file %s : %s\n",
                       argv[1], strerror(errno));
         (void)close(fd);
         exit(INCORRECT);
      }
#ifdef HAVE_STATX
      if ((buf = malloc(stat_buf.stx_size)) == NULL)
#else
      if ((buf = malloc(stat_buf.st_size)) == NULL)
#endif
      {
         (void)fprintf(stderr, "ERROR   : Failed to allocate memory : %s\n",
                       strerror(errno));
         (void)close(fd);
         exit(INCORRECT);
      }
#ifdef HAVE_STATX
      (void)memcpy(buf, src, stat_buf.stx_size);
#else
      (void)memcpy(buf, src, stat_buf.st_size);
#endif
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
      if (munmap(src, stat_buf.stx_size) < 0)
# else
      if (munmap(src, stat_buf.st_size) < 0)
# endif
#else
      if (munmap_emu(src) < 0)
#endif
      {
         (void)fprintf(stderr, "ERROR   : munmap() error : %s\n",
                       strerror(errno));
         exit(INCORRECT);
      }
   }
   else
   {
      (void)fprintf(stderr, "WARNING : File %s is empty!\n", argv[1]);
      exit(SUCCESS);
   }

   /* See what byte-order we have here. To */
   /* see whether we have to swap bytes.   */
   if (*(char *)&byte_order == 1)
   {
      /* little-endian */
      if (buf[0] == 'M')
      {
         swap_bytes_flag = YES;
      }
      else
      {
         swap_bytes_flag = NO;
      }
   }
   else
   {
      /* big-endian */
      if (buf[0] == 'I')
      {
         swap_bytes_flag = YES;
      }
      else
      {
         swap_bytes_flag = NO;
      }
   }

   if (swap_bytes_flag == YES)
   {
      byte_swap_word(&buf[4]);
   }
   (void)fprintf(stdout, "##############################> TIFF Header <##############################\n\n");
   (void)fprintf(stdout, "            byte order   TIFF ID   offset to first IFD\n");
   (void)memcpy(&ifd_offset, &buf[4], 4);
   (void)fprintf(stdout, "               <%c%c>      <%2d %02d>         <%d>\n\n",
                 buf[0], buf[1], buf[2], buf[3], ifd_offset);
   (void)fprintf(stdout, "###########################################################################\n\n");

   while (ifd_offset > 0)
   {
      (void)fprintf(stdout, "================================> IFD %2d <=================================\n\n", i);
      if (swap_bytes_flag == YES)
      {
         byte_swap_short(&buf[ifd_offset]);
      }
      (void)memcpy(&no_of_dirs, &buf[ifd_offset], 2);
      offset = 2;

      (void)fprintf(stdout, "IFH | Tag ID |   Type    | Count |   Offset   | Data\n");
      (void)fprintf(stdout, "----+--------+-----------+-------+------------+----------------------------\n");
      /* Show all directory entries. */
      for (j = 0; j < no_of_dirs; j++)
      {
         if (swap_bytes_flag == YES)
         {
            byte_swap_short(&buf[ifd_offset + offset]);
         }
         (void)memcpy(&tag_id, &buf[ifd_offset + offset], 2);
         (void)fprintf(stdout, "%3d |  %4d  |", j, tag_id);
         offset += 2;

         if (swap_bytes_flag == YES)
         {
            byte_swap_short(&buf[ifd_offset + offset]);
         }
         (void)memcpy(&tmp_short, &buf[ifd_offset + offset], 2);
         switch (tmp_short)
         {
            case  1: (void)fprintf(stdout, "   BYTE    |");
                     length = 1;
                     break;
            case  2: (void)fprintf(stdout, "   ASCII   |");
                     length = 1;
                     break;
            case  3: (void)fprintf(stdout, "   SHORT   |");
                     length = 2;
                     break;
            case  4: (void)fprintf(stdout, "   LONG    |");
                     length = 4;
                     break;
            case  5: (void)fprintf(stdout, " RATIONAL  |");
                     length = 8;
                     break;
            case  6: (void)fprintf(stdout, "   SBYTE   |");
                     length = 1;
                     break;
            case  7: (void)fprintf(stdout, " UNDEFINED |");
                     length = 1;
                     break;
            case  8: (void)fprintf(stdout, "  SSHORT   |");
                     length = 2;
                     break;
            case  9: (void)fprintf(stdout, "   SLONG   |");
                     length = 4;
                     break;
            case 10: (void)fprintf(stdout, " SRATIONAL |");
                     length = 8;
                     break;
            case 11: (void)fprintf(stdout, "   FLOAT   |");
                     length = 4;
                     break;
            case 12: (void)fprintf(stdout, "  DOUBLE   |");
                     length = 8;
                     break;
            default: (void)fprintf(stdout, " <Unknown> |");
                     continue;
         }
         offset += 2;

         if (swap_bytes_flag == YES)
         {
            byte_swap_word(&buf[ifd_offset + offset]);
         }
         (void)memcpy(&count, &buf[ifd_offset + offset], 4);
         (void)fprintf(stdout, " %5d |", count);
         offset += 4;

         if ((count * length) > 4)
         {
            if (swap_bytes_flag == YES)
            {
               byte_swap_word(&buf[ifd_offset + offset]);
            }
            (void)memcpy(&tmp_int, &buf[ifd_offset + offset], 4);
            (void)fprintf(stdout, " %10d |", tmp_int);

            if ((tag_id == 282) || (tag_id == 283))
            {
               if (swap_bytes_flag == YES)
               {
                  byte_swap_word(&buf[*(int *)&buf[ifd_offset + offset]]);
                  byte_swap_word(&buf[*(int *)&buf[ifd_offset + offset]] + 4);
               }
               (void)memcpy(&tmp_int, &buf[ifd_offset + offset], 4);
               (void)fprintf(stdout, " %d", tmp_int);
               (void)memcpy(&tmp_int, &buf[ifd_offset + offset] + 4, 4);
               (void)fprintf(stdout, " %d\n", tmp_int);
            }
            else
            {
               (void)fprintf(stdout, "\n");
            }
         }
         else
         {
            (void)fprintf(stdout, "            | %02x%02x%02x%02x\n",
                          (unsigned char)buf[ifd_offset + offset],
                          (unsigned char)(buf[ifd_offset + offset + 1]),
                          (unsigned char)(buf[ifd_offset + offset + 2]),
                          (unsigned char)(buf[ifd_offset + offset + 3]));
         }
         offset += 4;
      }
      (void)fprintf(stdout, "----+--------+-----------+-------+------------+----------------------------\n\n");

      /* Get offset to the next IFD. */
      if (swap_bytes_flag == YES)
      {
         byte_swap_word(&buf[ifd_offset + offset]);
      }
      (void)memcpy(&ifd_offset, &buf[ifd_offset + offset], 4);
      (void)fprintf(stdout, "===========================================================================\n\n");
   }

   free(buf);
   (void)close(fd);

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++ byte_swap_short() +++++++++++++++++++++++++*/
static void
byte_swap_short(char *ptr)
{
  char  tmp_byte;

  tmp_byte = ptr[0];
  ptr[0] = ptr[1];
  ptr[1] = tmp_byte;

  return;
}


/*+++++++++++++++++++++++++++ byte_swap_word() ++++++++++++++++++++++++++*/
static void
byte_swap_word(char *ptr)
{
  char  tmp_byte;

  tmp_byte = ptr[0];
  ptr[0] = ptr[3];
  ptr[3] = tmp_byte;
  tmp_byte = ptr[1];
  ptr[1] = ptr[2];
  ptr[2] = tmp_byte;

  return;
}
