/*
 *  convert_stat.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   convert_stat - converts the AFD statistics from an old format to
 **                  a new one
 **
 ** SYNOPSIS
 **   convert_stat [-w <working directory>]
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
 **   23.05.1998 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                    /* fprintf(), sprintf()            */
#include <string.h>                   /* strcpy(), strcat(), strerror()  */
#include <time.h>                     /* time()                          */
#include <sys/time.h>                 /* struct tm                       */
#include <stdlib.h>                   /* malooc(), free()                */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>                    /* O_RDWR, O_CREAT, O_TRUNC,  etc  */
#include <errno.h>
#include "../statistics/statdefs.h"
#include "version.h"


/* Global varaibles. */
int        sys_log_fd = STDERR_FILENO;
char       *p_work_dir;
const char *sys_log_name = SYSTEM_LOG_FIFO;

struct old_afdstat
       {
          char              hostname[MAX_HOSTNAME_LENGTH + 1];
          time_t            start_time;                 /* Time when acc. for */
                                                        /* this host starts.  */
          int               year_counter;               /* Position in        */
                                                        /* century.           */
          struct statistics century[100];               /* Per year.          */
          int               day_counter;                /* Position in year.  */
          struct statistics year[365];                  /* Per day.           */
          int               hour_counter;               /* Position in day.   */
          struct statistics day[HOURS_PER_DAY];         /* Per hour.          */
          int               sec_counter;                /* Position in hour.  */
          struct statistics hour[SECS_PER_HOUR];        /* Per                */
                                                        /* STAT_RESCAN_TIME   */
                                                        /* seconds.           */
          unsigned int      prev_nfs;
          double            prev_nbs;
          unsigned int      prev_ne;
          unsigned int      prev_nc;
       };


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int                fd,
                      i,
                      j,
                      no_of_hosts;
   size_t             new_size;
   time_t             now;
   char               new_statistic_file_name[MAX_PATH_LENGTH],
                      old_statistic_file_name[MAX_PATH_LENGTH],
                      work_dir[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx       stat_buf;
#else
   struct stat        stat_buf;
#endif
   struct tm          *p_ts;
   struct old_afdstat *old_stat_db = NULL;
   struct afdstat     *stat_db = NULL;

   /* Evaluate arguments. */
   CHECK_FOR_VERSION(argc, argv);
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   (void)time(&now);
   p_ts = gmtime(&now);
   if (argc == 3)
   {
      (void)strcpy(new_statistic_file_name, argv[2]);
   }
   else
   {
      (void)sprintf(new_statistic_file_name, "%s%s%s.%d",
                    p_work_dir, LOG_DIR, STATISTIC_FILE,
                    p_ts->tm_year + 1900);
   }
   if ((argc == 2) || (argc == 3))
   {
      (void)strcpy(old_statistic_file_name, argv[1]);
   }
   else
   {
      (void)sprintf(old_statistic_file_name, "%s%s/afd_status_file",
                    p_work_dir, FIFO_DIR);
   }

   /*
    * Read old statistic file.
    */
   if ((fd = open(old_statistic_file_name, O_RDONLY)) == -1)
   {
      (void)fprintf(stderr, "ERROR   : Failed to open() %s : %s (%s %d)\n",
                    old_statistic_file_name, strerror(errno),
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef HAVE_STATX
   if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(fd, &stat_buf) == -1)
#endif
   {
      (void)fprintf(stderr, "ERROR   : Failed to access %s : %s (%s %d)\n",
                    old_statistic_file_name, strerror(errno),
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if ((fd = open(old_statistic_file_name, O_RDONLY)) == -1)
   {
      (void)fprintf(stderr, "ERROR   : Failed to open() %s : %s (%s %d)\n",
                    old_statistic_file_name, strerror(errno),
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef HAVE_STATX
   if ((old_stat_db = malloc(stat_buf.stx_size + 1)) == NULL)
#else
   if ((old_stat_db = malloc(stat_buf.st_size + 1)) == NULL)
#endif
   {
      (void)fprintf(stderr, "ERROR   : malloc() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef HAVE_STATX
   if (read(fd, old_stat_db, stat_buf.stx_size) != stat_buf.stx_size)
#else
   if (read(fd, old_stat_db, stat_buf.st_size) != stat_buf.st_size)
#endif
   {
      (void)fprintf(stderr, "ERROR   : Failed to read() %s : %s (%s %d)\n",
                    old_statistic_file_name, strerror(errno),
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef HAVE_STATX
   no_of_hosts = stat_buf.stx_size / sizeof(struct old_afdstat);
#else
   no_of_hosts = stat_buf.st_size / sizeof(struct old_afdstat);
#endif
   if (close(fd) == -1)
   {
      (void)fprintf(stderr, "WARNING : close() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
   }

   /*
    * Create new statistic file.
    */
   if ((fd = open(new_statistic_file_name, (O_RDWR | O_CREAT | O_TRUNC),
                  FILE_MODE)) == -1)
   {
      (void)fprintf(stderr, "ERROR   : Failed to open() %s : %s (%s %d)\n",
                    new_statistic_file_name, strerror(errno),
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }
   new_size = no_of_hosts * sizeof(struct afdstat);
   if ((stat_db = malloc(new_size)) == NULL)
   {
      (void)fprintf(stderr, "ERROR   : malloc() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /*
    * Copy data from old structure to the new one.
    */
   for (i = 0; i < no_of_hosts; i++)
   {
      (void)strcpy(stat_db[i].hostname, old_stat_db[i].hostname);
      stat_db[i].start_time = old_stat_db[i].start_time;
      stat_db[i].day_counter = old_stat_db[i].day_counter;
      for (j = 0; j < 365; j++)
      {
         stat_db[i].year[j].nfs = old_stat_db[i].year[j].nfs;
         stat_db[i].year[j].nbs = old_stat_db[i].year[j].nbs;
         stat_db[i].year[j].ne = old_stat_db[i].year[j].ne;
         stat_db[i].year[j].nc = old_stat_db[i].year[j].nc;
      }
      stat_db[i].year[j].nfs = 0;
      stat_db[i].year[j].nbs = 0.0;
      stat_db[i].year[j].ne = 0;
      stat_db[i].year[j].nc = 0;
      stat_db[i].hour_counter = old_stat_db[i].hour_counter;
      (void)memcpy(&stat_db[i].day, &old_stat_db[i].day,
                   (HOURS_PER_DAY * sizeof(struct statistics)));
      stat_db[i].sec_counter = old_stat_db[i].sec_counter;
      (void)memcpy(&stat_db[i].hour, &old_stat_db[i].hour,
                   (SECS_PER_HOUR * sizeof(struct statistics)));
      stat_db[i].prev_nfs = old_stat_db[i].prev_nfs;
      stat_db[i].prev_nbs[0] = old_stat_db[i].prev_nbs;
      stat_db[i].prev_ne = old_stat_db[i].prev_ne;
      stat_db[i].prev_nc = old_stat_db[i].prev_nc;
   }

   if (write(fd, (char *)stat_db, new_size) != new_size)
   {
      (void)fprintf(stderr, "ERROR   : Failed to write() %s : %s (%s %d)\n",
                    new_statistic_file_name, strerror(errno),
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (close(fd) == -1)
   {
      (void)fprintf(stderr, "WARNING : close() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
   }
   free(stat_db);
   free(old_stat_db);

   if (remove(old_statistic_file_name) == -1)
   {
      (void)fprintf(stderr, "WARNING : Failed to remove %s : %s (%s %d)\n",
                    old_statistic_file_name, strerror(errno),
                    __FILE__, __LINE__);
   }

   (void)fprintf(stdout,
                 "Successfully converted AFD statistics 0.9.x -> 1.0.x\n");

   exit(SUCCESS);
}
