/*
 *  file_mask_list_spy.c - Part of AFD, an automatic file distribution
 *                         program.
 *  Copyright (c) 2003 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   file_mask_list_spy - shows all file mask entries held by the AFD
 **
 ** SYNOPSIS
 **   file_mask_list_spy [-w <AFD work dir>] [--version] [<file mask id>]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   29.12.2003 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>                 /* atoi()                            */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>                  /* open()                            */
#include <unistd.h>                 /* fstat(), lseek(), write()         */
#include <sys/mman.h>               /* mmap()                            */
#include <errno.h>
#include "version.h"

/* Global variables. */
int        sys_log_fd = STDERR_FILENO;
char       *p_work_dir = NULL;
const char *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int          fd,
                fml_offset,
                mask_offset,
                no_of_file_masks_id,
                search_id;
   unsigned int file_mask_id;
   char         file[MAX_PATH_LENGTH],
                *fmd,
                *fmd_end,
                *ptr,
                work_dir[MAX_PATH_LENGTH];
   struct stat  stat_buf;

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD. */
   if (get_afd_path(&argc, argv, work_dir) < 0) 
   {
      exit(INCORRECT);
   }
   if (argc == 2)
   {
      file_mask_id = (unsigned int)strtoul(argv[1], (char **)NULL, 16);
      search_id = YES;
   }
   else
   {
      search_id = NO;
   }

   (void)sprintf(file, "%s%s%s", work_dir, FIFO_DIR, FILE_MASK_FILE);
   if ((fd = open(file, O_RDONLY)) == -1)
   {
      (void)fprintf(stderr, _("Failed to open() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (fstat(fd, &stat_buf) == -1)
   {
      (void)fprintf(stderr, _("Failed to fstat() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if ((ptr = mmap(NULL, stat_buf.st_size, PROT_READ,
                   MAP_SHARED, fd, 0)) == (caddr_t)-1)
   {
      (void)fprintf(stderr, _("Failed to mmap() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   no_of_file_masks_id = *(int *)ptr;
   fmd_end = ptr + stat_buf.st_size;
   ptr += AFD_WORD_OFFSET;
   fmd = ptr;
   fml_offset = sizeof(int) + sizeof(int);
   mask_offset = fml_offset + sizeof(int) + sizeof(unsigned int) +
                 sizeof(unsigned char);

   if (no_of_file_masks_id > 0)
   {
      int i, j;

      for (i = 0; i < no_of_file_masks_id; i++)
      {
         if ((search_id == NO) ||
             (file_mask_id == *(unsigned int *)(ptr + fml_offset + sizeof(int))))
         {
            (void)fprintf(stdout, "File Mask ID        : %x\n",
                          *(unsigned int *)(ptr + fml_offset + sizeof(int)));
            (void)fprintf(stdout, "No of File Mask     : %d\n", *(int *)ptr);
            (void)fprintf(stdout, "Max File Mask length: %d\n",
                          *(int *)(ptr + sizeof(int)));
            (void)fprintf(stdout, "Total length        : %d\n",
                          *(int *)(ptr + fml_offset));
            (void)fprintf(stdout, "No of fill bytes    : %d\n",
                          (int)*(ptr + mask_offset - 1));
            (void)fprintf(stdout, "CRC redundant value : %d\n",
                          (int)*(ptr + mask_offset + *(int *)(ptr + fml_offset)));
            if (*(int *)ptr == 1)
            {
               (void)fprintf(stdout, "File mask           : %s\n",
                             (ptr + mask_offset));
            }
            else
            {
               char *p_file = ptr + mask_offset;

               (void)fprintf(stdout, "File filters        : %s\n", p_file);
               NEXT(p_file);
               for (j = 1; j < *(int *)ptr; j++)
               {
                  (void)fprintf(stdout, "                    : %s\n", p_file);
                  NEXT(p_file);
               }
            }
            if ((search_id == NO) && ((i + 1) < no_of_file_masks_id))
            {
               (void)fprintf(stdout, "--------------------------------------------------------------------------------\n");
            }
         }
         ptr += (mask_offset + *(int *)(ptr + fml_offset) + sizeof(char) +
                 *(ptr + mask_offset - 1));
         if (ptr > fmd_end)
         {
            break;
         }
      }
   }
   else
   {
      (void)fprintf(stdout, _("File mask list is empty.\n"));
   }

   fmd -= AFD_WORD_OFFSET;
   if (munmap(fmd, stat_buf.st_size) == -1)
   {
      (void)fprintf(stderr, _("Failed to munmap() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
   }
   (void)close(fd);

   exit(INCORRECT);
}
