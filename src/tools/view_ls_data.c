/*
 *  view_ls_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 - 2009 Deutscher Wetterdienst (DWD),
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
 **   view_ls_data - Show contents of the AFD ls data file
 **
 ** SYNOPSIS
 **   view_ls_data [--version] <ls data filename 1>[...<ls data filename n>]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   08.04.2005 H.Kiehl Created
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

/* Local function prototypes. */
static void usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int                  fd,
                        i,
                        j,
                        no_of_listed_files;
   char                 *ptr,
                        time_str[25];
   struct stat          stat_buf;
   struct retrieve_list *rl;

   CHECK_FOR_VERSION(argc, argv);

   if (argc < 2)
   {
      usage(argv[0]);
      exit(INCORRECT);
   }
   for (i = 1; i < argc; i++)
   {
      if ((fd = open(argv[i], O_RDONLY)) == -1)
      {
         (void)fprintf(stderr, "Failed to open() %s : %s\n",
                       argv[i], strerror(errno));
      }
      else
      {
         if (fstat(fd, &stat_buf) == -1)
         {
            (void)fprintf(stderr, "Failed to fstat() %s : %s\n",
                          argv[i], strerror(errno));
         }
         else
         {
            if ((ptr = mmap(NULL, stat_buf.st_size, PROT_READ, MAP_SHARED,
                            fd, 0)) == (caddr_t) -1)
            {
               (void)fprintf(stderr, "Failed to mmap() %s : %s\n",
                             argv[i], strerror(errno));
            }
            else
            {
               int version;

               no_of_listed_files = *(int *)ptr;
               version = (int)(*(ptr + SIZEOF_INT + 1 + 1 + 1));
               if (version != CURRENT_RL_VERSION)
               {
                  (void)fprintf(stderr,
                                "Incorrect structure version, can only display version %d. This version is %d.\n",
                                CURRENT_RL_VERSION, version);
               }
               else
               {
                  ptr += AFD_WORD_OFFSET;
                  rl = (struct retrieve_list *)ptr;

                  (void)fprintf(stdout, "\n        %s (%d entries  Struct Version: %d)\n\n",
                                argv[i], no_of_listed_files, version);
                  (void)fprintf(stdout, "                        |            |Got |    | In |Assi|\n");
                  (void)fprintf(stdout, "          Date          |    Size    |date|Retr|list|nged|Flag|   File name\n");
                  (void)fprintf(stdout, "------------------------+------------+----+----+----+----+----+-------------------------------\n");
                  for (j = 0; j < no_of_listed_files; j++)
                  {
                     (void)strftime(time_str, 25, "%c", localtime(&rl[j].file_mtime));
#if SIZEOF_OFF_T == 4
                     (void)fprintf(stdout, "%s|%12ld| %s| %s| %s| %3d| %3d|%s\n",
#else
                     (void)fprintf(stdout, "%s|%12lld| %s| %s| %s| %3d| %3d|%s\n",
#endif
                                   time_str, (pri_off_t)rl[j].size,
                                   (rl[j].got_date == YES) ? "YES" : "NO ",
                                   (rl[j].retrieved == YES) ? "YES" : "NO ",
                                   (rl[j].in_list == YES) ? "YES" : "NO ",
                                   rl[j].assigned,
                                   rl[j].special_flag, rl[j].file_name);
                  }
                  ptr -= AFD_WORD_OFFSET;
               }
               if (munmap(ptr, stat_buf.st_size) == -1)
               {
                  (void)fprintf(stderr, "Failed to munmap() from %s : %s\n",
                                argv[i], strerror(errno));
               }
            }
         }
         (void)close(fd);
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
