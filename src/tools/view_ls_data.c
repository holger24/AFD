/*
 *  view_ls_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 - 2022 Deutscher Wetterdienst (DWD),
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
 **   15.10.2017 H.Kiehl Add the full path name for the ls_data directory.
 **   06.01.2019 H.Kiehl Show last directory modification time.
 **   26.01.2019 H.Kiehl Hmmm, storing last directory modification time
 **                      here is wrong. It must be in the FRA. Let's
 **                      just keep it and store the time when this file
 **                      was created.
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

/* Global variables. */
int         sys_log_fd = STDERR_FILENO;
char        *p_work_dir;
const char  *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int                  fd,
                        i,
                        j;
   char                 fullname[MAX_PATH_LENGTH],
                        *ptr,
                        time_str[25],
                        work_dir[MAX_PATH_LENGTH],
                        *work_ptr;
#ifdef HAVE_STATX
   struct statx         stat_buf;
#else
   struct stat          stat_buf;
#endif
   struct retrieve_list *rl;

   if ((get_arg(&argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      usage(argv[0]);
      exit(SUCCESS);
   }

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if (argc < 2)
   {
      usage(argv[0]);
      exit(INCORRECT);
   }
   work_ptr = fullname + snprintf(fullname, MAX_PATH_LENGTH, "%s%s%s%s/",
                                  p_work_dir, AFD_FILE_DIR, INCOMING_DIR,
                                  LS_DATA_DIR);
   for (i = 1; i < argc; i++)
   {
      (void)strcpy(work_ptr, argv[i]);
      if ((fd = open(fullname, O_RDONLY)) == -1)
      {
         (void)fprintf(stderr, "Failed to open() %s : %s\n",
                       fullname, strerror(errno));
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
            (void)fprintf(stderr, "Failed to access %s : %s\n",
                          fullname, strerror(errno));
         }
         else
         {
            if ((ptr = mmap(NULL,
#ifdef HAVE_STATX
                            stat_buf.stx_size, PROT_READ, MAP_SHARED,
#else
                            stat_buf.st_size, PROT_READ, MAP_SHARED,
#endif
                            fd, 0)) == (caddr_t) -1)
            {
               (void)fprintf(stderr, "Failed to mmap() %s : %s\n",
                             fullname, strerror(errno));
            }
            else
            {
               int    no_of_listed_files,
                      version;
               time_t create_time;

               no_of_listed_files = *(int *)ptr;
               version = (int)(*(ptr + SIZEOF_INT + 1 + 1 + 1));
               create_time = *(time_t *)(ptr + SIZEOF_INT + 4);
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

                  (void)strftime(time_str, 25, "%c", localtime(&create_time));
                  (void)fprintf(stdout, "\n        %s (%d entries  Struct Version: %d  Create time: %s)\n\n",
                                argv[i], no_of_listed_files, version, time_str);
                  (void)fprintf(stdout, "                        |            |  Previous  |Got |    | In |Assi|\n");
#ifdef _WITH_EXTRA_CHECK
                  (void)fprintf(stdout, "          Date          |    Size    |    Size    |date|Retr|list|nged|Flag|   File name + possible extra data\n");
#else
                  (void)fprintf(stdout, "          Date          |    Size    |    Size    |date|Retr|list|nged|Flag|   File name\n");
#endif
                  (void)fprintf(stdout, "------------------------+------------+------------+----+----+----+----+----+----------------------------------\n");
                  for (j = 0; j < no_of_listed_files; j++)
                  {
                     (void)strftime(time_str, 25, "%c", localtime(&rl[j].file_mtime));
#ifdef _WITH_EXTRA_CHECK
                     if (rl[j].extra_data[0] != '\0')
                     {
# if SIZEOF_OFF_T == 4
                        (void)fprintf(stdout, "%s|%12ld|%12ld| %s| %s| %s| %3d| %3d|%s|%s\n",
# else
                        (void)fprintf(stdout, "%s|%12lld|%12lld| %s| %s| %s| %3d| %3d|%s|%s\n",
# endif
                                      time_str, (pri_off_t)rl[j].size,
                                      (pri_off_t)rl[j].prev_size,
                                      (rl[j].got_date == YES) ? "YES" : "NO ",
                                      (rl[j].retrieved == YES) ? "YES" : "NO ",
                                      (rl[j].in_list == YES) ? "YES" : "NO ",
                                      rl[j].assigned,
                                      rl[j].special_flag, rl[j].file_name,
                                      rl[j].extra_data);
                     }
                     else
                     {
#endif
#if SIZEOF_OFF_T == 4
                        (void)fprintf(stdout, "%s|%12ld|%12ld| %s| %s| %s| %3d| %3d|%s\n",
#else
                        (void)fprintf(stdout, "%s|%12lld|%12lld| %s| %s| %s| %3d| %3d|%s\n",
#endif
                                      time_str, (pri_off_t)rl[j].size,
                                      (pri_off_t)rl[j].prev_size,
                                      (rl[j].got_date == YES) ? "YES" : "NO ",
                                      (rl[j].retrieved == YES) ? "YES" : "NO ",
                                      (rl[j].in_list == YES) ? "YES" : "NO ",
                                      rl[j].assigned,
                                      rl[j].special_flag, rl[j].file_name);
#ifdef _WITH_EXTRA_CHECK
                     }
#endif
                  }
                  ptr -= AFD_WORD_OFFSET;
               }
#ifdef HAVE_STATX
               if (munmap(ptr, stat_buf.stx_size) == -1)
#else
               if (munmap(ptr, stat_buf.st_size) == -1)
#endif
               {
                  (void)fprintf(stderr, "Failed to munmap() from %s : %s\n",
                                fullname, strerror(errno));
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
