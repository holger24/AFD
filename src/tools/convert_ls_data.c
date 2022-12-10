/*
 *  convert_ls_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 - 2022 Deutscher Wetterdienst (DWD),
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
 **   convert_ls_data - Converts the ls data file from 32bit to 64bit
 **
 ** SYNOPSIS
 **   convert_ls_data [--version] <ls data filename 1>[...<ls data filename n>]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.12.2008 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>                 /* exit()                            */
#include <time.h>                   /* strftime()                        */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>                  /* open()                            */
#include <unistd.h>                 /* fstat()                           */
#include <sys/mman.h>               /* mmap()                            */
#include <errno.h>
#include "version.h"

struct retrieve_list32
       {
          char   file_name[MAX_FILENAME_LENGTH];
          char   got_date;
          char   retrieved;              /* Has the file already been      */
                                         /* retrieved?                     */
          char   in_list;                /* Used to remove any files from  */
                                         /* the list that are no longer at */
                                         /* the remote host.               */
          int    size;                   /* Size of the file.              */
          char   fill_bytes[4];
          int    file_mtime;             /* Modification time of file.     */
       };

/* Local function prototypes. */
static void usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int                    from_fd,
                          i,
                          j,
                          no_of_listed_files,
                          to_fd;
   char                   *ptr,
                          tmp_name[MAX_FILENAME_LENGTH];
#ifdef HAVE_STATX
   struct statx           stat_buf;
#else
   struct stat            stat_buf;
#endif
   struct retrieve_list   *rl;
   struct retrieve_list32 *rl32;

   CHECK_FOR_VERSION(argc, argv);

   if (argc < 2)
   {
      usage(argv[0]);
      exit(INCORRECT);
   }
   for (i = 1; i < argc; i++)
   {
      if ((from_fd = open(argv[i], O_RDONLY)) == -1)
      {
         (void)fprintf(stderr, "Failed to open() %s : %s\n",
                       argv[i], strerror(errno));
      }
      else
      {
#ifdef HAVE_STATX
         if (statx(from_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                   STATX_SIZE | STATX_MODE, &stat_buf) == -1)
#else
         if (fstat(from_fd, &stat_buf) == -1)
#endif
         {
            (void)fprintf(stderr, "Failed to access %s : %s\n",
                          argv[i], strerror(errno));
         }
         else
         {
            if ((ptr = mmap(NULL,
#ifdef HAVE_STATX
                            stat_buf.stx_size, PROT_READ, MAP_SHARED,
#else
                            stat_buf.st_size, PROT_READ, MAP_SHARED,
#endif
                            from_fd, 0)) == (caddr_t) -1)
            {
               (void)fprintf(stderr, "Failed to mmap() %s : %s\n",
                             argv[i], strerror(errno));
            }
            else
            {
               (void)strcpy(tmp_name, argv[i]);
               (void)strcat(tmp_name, ".converted");
#ifdef HAVE_STATX
               if ((to_fd = open(tmp_name, (O_WRONLY | O_CREAT | O_TRUNC),
                                 stat_buf.stx_mode)) == -1)
#else
               if ((to_fd = open(tmp_name, (O_WRONLY | O_CREAT | O_TRUNC),
                                 stat_buf.st_mode)) == -1)
#endif
               {
                  (void)fprintf(stderr, "Failed to open() %s : %s\n",
                                tmp_name, strerror(errno));
               }
               else
               {
                  int   rest;
                  off_t new_size;
                  char  *buffer,
                        *ptr2;

                  no_of_listed_files = *(int *)ptr;
                  rest = RETRIEVE_LIST_STEP_SIZE - (no_of_listed_files % RETRIEVE_LIST_STEP_SIZE);
                  new_size = AFD_WORD_OFFSET +
                             ((no_of_listed_files + rest) * sizeof(struct retrieve_list));
                  if ((buffer = malloc(new_size)) == NULL)
                  {
                     (void)fprintf(stderr, "malloc() error : %s\n",
                                   strerror(errno));
                  }
                  else
                  {
                     int version;

                     ptr2 = buffer;
                     (void)memset(buffer, 0, new_size);
                     *(int *)ptr2 = no_of_listed_files;
                     version = (int)(*(ptr + SIZEOF_INT + 1 + 1 + 1));
                     *(ptr2 + SIZEOF_INT + 1 + 1 + 1) = version;
                     ptr += AFD_WORD_OFFSET;
                     ptr2 += AFD_WORD_OFFSET;
                     rl32 = (struct retrieve_list32 *)ptr;
                     rl = (struct retrieve_list *)ptr2;

                     for (j = 0; j < no_of_listed_files; j++)
                     {
                        (void)memcpy(rl[j].file_name, rl32[j].file_name,
                                     MAX_FILENAME_LENGTH);
                        rl[j].got_date = rl32[j].got_date;
                        rl[j].retrieved = rl32[j].retrieved;
                        rl[j].in_list = rl32[j].in_list;
                        rl[j].size = (off_t)rl32[j].size;
                        rl[j].file_mtime = (time_t)rl32[j].file_mtime;
                     }

                     if (write(to_fd, buffer, new_size) != new_size)
                     {
                        (void)fprintf(stderr, "write() error : %s\n",
                                      strerror(errno));
                        exit(INCORRECT);
                     }
                     if (close(to_fd) == -1)
                     {
                        (void)fprintf(stderr, "close() error : %s\n",
                                      strerror(errno));
                     }
                     if (rename(tmp_name, argv[i]) == -1)
                     {
                        (void)fprintf(stderr, "rename() error : %s\n",
                                      strerror(errno));
                     }
                     ptr -= AFD_WORD_OFFSET;
                     free(buffer);
                  }
               }
#ifdef HAVE_STATX
               if (munmap(ptr, stat_buf.stx_size) == -1)
#else
               if (munmap(ptr, stat_buf.st_size) == -1)
#endif
               {
                  (void)fprintf(stderr, "Failed to munmap() from %s : %s\n",
                                argv[i], strerror(errno));
               }
            }
         }
         (void)close(from_fd);
      }
   }
   exit(SUCCESS);
}


/*############################## usage() ################################*/
static void
usage(char *progname)
{
   (void)fprintf(stderr, "%s <ls data filename 1>[... <ls data file name n>]\n",
                 progname);
   return;
}
