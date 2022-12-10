/*
 *  set_ls_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2021, 2022 Deutscher Wetterdienst (DWD),
 *                           Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   set_ls_data - set some value in ls data file
 **
 ** SYNOPSIS
 **   set_ls_data [--version] <ls data filename 1>[...<ls data filename n>]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.04.2021 H.Kiehl Created
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
#include "fddefs.h"
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
   int  fd,
        set_retrieved = NEITHER;
   char fullname[MAX_PATH_LENGTH],
        *p_dir_alias,
        work_dir[MAX_PATH_LENGTH];

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

   if ((argc < 3) || (argv[1][0] == '-'))
   {
      usage(argv[0]);
      exit(INCORRECT);
   }
   p_dir_alias = argv[1];
   if ((1 + 1) < argc)
   {
      register int j;

      for (j = 1; j < argc; j++)
      {
         argv[j] = argv[j + 1];
      }
      argv[j] = NULL;
   }
   else
   {
      argv[1] = NULL;
   }
   argc -= 1;

   if (get_arg(&argc, argv, "-r", NULL, 0) == SUCCESS)
   {
      set_retrieved = YES;
   }
   else if (get_arg(&argc, argv, "-R", NULL, 0) == SUCCESS)
        {
           set_retrieved = NO;
        }
        else
        {
           usage(argv[0]);
           exit(INCORRECT);
        }

   (void)snprintf(fullname, MAX_PATH_LENGTH, "%s%s%s%s/%s",
                  p_work_dir, AFD_FILE_DIR, INCOMING_DIR, LS_DATA_DIR,
                  p_dir_alias);

   if ((fd = open(fullname, O_RDWR)) == -1)
   {
      (void)fprintf(stderr, "Failed to open() %s : %s\n",
                    fullname, strerror(errno));
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
         char *ptr;

         if ((ptr = mmap(NULL,
#ifdef HAVE_STATX
                         stat_buf.stx_size, (PROT_READ | PROT_WRITE),
#else
                         stat_buf.st_size, (PROT_READ | PROT_WRITE),
#endif
                         MAP_SHARED, fd, 0)) == (caddr_t) -1)
         {
            (void)fprintf(stderr, "Failed to mmap() %s : %s\n",
                          fullname, strerror(errno));
         }
         else
         {
            int no_of_listed_files,
                version;

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
               struct retrieve_list *rl;

               ptr += AFD_WORD_OFFSET;
               rl = (struct retrieve_list *)ptr;

               /* Try get a lock. */
               if (rlock_region(fd, LOCK_RETR_PROC) == LOCK_IS_SET)
               {
                  (void)fprintf(stderr,
                                "ls data file %s is locked.", p_dir_alias);
               }
               else
               {
                  int i, j,
                      gotcha;

                  for (j = 0; j < no_of_listed_files; j++)
                  {
                     gotcha = NO;
                     if (argc > 1)
                     {
                        for (i = 1; i < argc; i++)
                        {
                           if (pmatch(argv[i], rl[j].file_name, NULL) == 0)
                           {
                              gotcha = YES;
                              break;
                           }
                        }
                     }
                     else
                     {
                        gotcha = YES;
                     }
                     if (gotcha == YES)
                     {
                        if (set_retrieved != NEITHER)
                        {
                           rl[j].retrieved = set_retrieved;
                        }
                     }
                  }
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

   exit(SUCCESS);
}


/*############################## usage() ################################*/
static void
usage(char *progname)
{
   (void)fprintf(stderr, "%s <dir-alias> <option> [<file name>]\n",
                 progname);
   (void)fprintf(stderr, "         -r     mark as retrieved.\n");
   (void)fprintf(stderr, "         -R     mark as Not retrieved.\n");
   return;
}
