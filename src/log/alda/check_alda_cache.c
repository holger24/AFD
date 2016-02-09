/*
 *  check_alda_cache.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2010 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_alda_cache - prints content of given file
 **
 ** SYNOPSIS
 **   check_alda_cache <log type> <log number>
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   18.03.2010 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>            /* exit(), malloc()                       */
#include <unistd.h>            /* read()                                 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>             /* open()                                 */
#include <errno.h>
#include "aldadefs.h"
#include "logdefs.h"
#include "version.h"

/* Global variables. */
int         sys_log_fd = STDOUT_FILENO;
char        *p_work_dir;
const char  *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void usage(char *);


/*######################### check_alda_cache() ##########################*/
int
main(int argc, char *argv[])
{
   int  fd;
   char cache_file[MAX_PATH_LENGTH],
        log_file[MAX_PATH_LENGTH],
        work_dir[MAX_PATH_LENGTH];

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if (argc != 3)
   {
      usage(argv[0]);
      exit(INCORRECT);
   }
   if (argv[1][1] == '\0')
   {
      switch (argv[1][0])
      {
#ifdef WHEN_ITS_IMPLEMENTED
#ifdef _INPUT_LOG
         case 'I' : /* Input */
            (void)sprintf(log_file, "%s%s/%s%s",
                          p_work_dir, LOG_DIR, INPUT_BUFFER_FILE, argv[2]);
            (void)sprintf(cache_file, "%s%s/%s%s",
                          p_work_dir, LOG_DIR, INPUT_BUFFER_CACHE_FILE,
                          argv[2]);
            break;
#endif
#endif

#ifdef _OUTPUT_LOG
         case 'O' : /* Output */
            (void)sprintf(log_file, "%s%s/%s%s",
                          p_work_dir, LOG_DIR, OUTPUT_BUFFER_FILE, argv[2]);
            (void)sprintf(cache_file, "%s%s/%s%s",
                          p_work_dir, LOG_DIR, OUTPUT_BUFFER_CACHE_FILE,
                          argv[2]);
            break;
#endif

         default : /* Error/Unknown. */
            (void)fprintf(stderr, "Unknown log type %s.\n", argv[1]);
            exit(INCORRECT);
      }
   }
   else
   {
      (void)fprintf(stderr, "Unknown log type %s.\n", argv[1]);
      exit(INCORRECT);
   }

   if ((fd = open(cache_file, O_RDONLY)) == -1)
   {
      (void)fprintf(stderr, "Failed to open() `%s` : %s (%s %d)\n",
                    cache_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   else
   {
      unsigned int line = 0;
      int          log_cache_buf_size,
                   n;
      off_t        *log_pos;
      char         *log_cache_buffer;
      FILE         *fp;

      n = sizeof(time_t);
      if (sizeof(off_t) > n)
      {
         n = sizeof(off_t);
      }
      log_cache_buf_size = n + n;
      if ((log_cache_buffer = malloc((size_t)log_cache_buf_size)) == NULL)
      {
         (void)fprintf(stderr,
                       "Failed to malloc() %d bytes : %s (%s %d)\n",
                       log_cache_buf_size, strerror(errno),
                       __FILE__, __LINE__);
         exit(INCORRECT);
      }
      log_pos = (off_t *)(log_cache_buffer + n);

      if ((fp = fopen(log_file, "r")) == NULL)
      {
         (void)fprintf(stderr, _("Failed to fopen() `%s' : %s\n"),
                       log_file, strerror(errno));
         exit(INCORRECT);
      }

      while ((n = read(fd, log_cache_buffer,
                       log_cache_buf_size)) == log_cache_buf_size)
      {
         if (*log_pos != 0)
         {
            if (fseeko(fp, (*log_pos) - 1, SEEK_SET) == -1)
            {
               (void)fprintf(stderr,
                             "Failed to fseeko() in `%s' : %s (%s %d)\n",
                             cache_file, strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
            else
            {
               if (fgetc(fp) != '\n')
               {
                  (void)fprintf(stdout,
#if SIZEOF_OFF_T == 4
                                "Cache broken at line %u (position=%ld).\n",
#else
                                "Cache broken at line %u (position=%lld).\n",
#endif
                                line, (pri_off_t)(*log_pos));
                  exit(INCORRECT);
               }
            }
         }
         line++;
      }
      if (n == -1)
      {
         (void)fprintf(stderr,
                       "Read error while reading from `%s' : %s (%s %d)\n",
                       cache_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      (void)fprintf(stdout, "Alda cache file `%s' is good!\n", cache_file);

      if (close(fd) == -1)
      {
         (void)fprintf(stderr, "Failed to close() `%s` : %s (%s %d)\n",
                       cache_file, strerror(errno), __FILE__, __LINE__);
      }
      if (fclose(fp) == EOF)
      {
         (void)fprintf(stderr, _("Failed to fclose() `%s' : %s\n"),
                       log_file, strerror(errno));
      }
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{
   (void)fprintf(stderr,
                 _("SYNTAX  : %s <log type> <log number>\n"), progname);
   (void)fprintf(stderr, _("         log types : O\n"));

   return;
}
