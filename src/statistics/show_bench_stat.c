/*
 *  show_bench_stat.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   show_bench_stat - shows statistic information of one or more AFD's
 **
 ** SYNOPSIS
 **   show_bench_stat <common dir> <interval> <loops> <sub dir 1>...<sub dir n>
 **               --version       Show version.
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
 **   03.10.1999 H.Kiehl Created
 **   14.01.2001 H.Kiehl Show loop counter.
 **
 */
DESCR__E_M1

#include <stdio.h>                  /* fprintf(), stderr                 */
#include <string.h>                 /* strcpy(), strerror(), strcmp()    */
#include <time.h>                   /* time()                            */
#include <sys/time.h>               /* struct tm                         */
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>                 /* sleep(), close(), STDERR_FILENO   */
#include <stdlib.h>                 /* free(), malloc(), exit()          */
#ifdef HAVE_MMAP
# include <sys/mman.h>              /* mmap(), munmap()                  */
#endif
#include <fcntl.h>
#include <errno.h>
#include "statdefs.h"
#include "version.h"

#ifdef HAVE_MMAP
# ifndef MAP_FILE    /* Required for BSD          */
#  define MAP_FILE 0 /* All others do not need it */
# endif
#endif

/* Global variables. */
int            loops,
               loops_to_do,
               no_of_afds,
               *no_of_hosts,
               sys_log_fd = STDERR_FILENO;
unsigned int   interval_time;
double         nfs = 0.0, nbs = 0.0, nc = 0.0, ne = 0.0, fps, bps,
               tmp_nfs = 0.0, tmp_nbs = 0.0, tmp_nc = 0.0, tmp_ne = 0.0;
char           **p_afd_stat,
               *p_work_dir = NULL;
struct afdstat *afd_stat;
const char     *sys_log_name = SYSTEM_LOG_FIFO;

/* Function prototypes. */
static void    display_data(double, double, double, double, double, double),
               summary(int),
               timeout(int, void (*func)(int)),
               usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   register int i;
   int          stat_fd;
   time_t       now;
#ifdef HAVE_MMAP
   off_t        *afdstat_size;
#endif
   char         **afd_dir,
                **statistic_file;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif
   struct tm    *p_ts;

   CHECK_FOR_VERSION(argc, argv);

   /* Evaluate arguments. */
   if (argc < 5)
   {
      usage(argv[0]);
      exit(INCORRECT);
   }
   interval_time = atoi(argv[2]);
   loops_to_do = atoi(argv[3]);
   no_of_afds = argc - 4;
   RT_ARRAY(afd_dir, no_of_afds, MAX_PATH_LENGTH, char);
   RT_ARRAY(statistic_file, no_of_afds, MAX_PATH_LENGTH, char);
#ifdef HAVE_MMAP
   if ((afdstat_size = malloc((no_of_afds * sizeof(off_t)))) == NULL)
   {
      (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#endif
   if ((p_afd_stat = malloc((no_of_afds * sizeof(char *)))) == NULL)
   {
      (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if ((no_of_hosts = malloc((no_of_afds * sizeof(int)))) == NULL)
   {
      (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   i = 4;
   do
   {
      (void)sprintf(afd_dir[i - 4], "%s/%s", argv[1], argv[i]);
      i++;
   } while (i != argc);

   /* Initialize variables. */
   (void)time(&now);
   p_ts = gmtime(&now);
   for (i = 0; i < no_of_afds; i++)
   {
      (void)sprintf(statistic_file[i], "%s%s%s.%d",
#ifdef STAT_IN_FIFODIR
                    afd_dir[i], FIFO_DIR, STATISTIC_FILE,
#else
                    afd_dir[i], LOG_DIR, STATISTIC_FILE,
#endif
                    p_ts->tm_year + 1900);

      do
      {
         errno = 0;
#ifdef HAVE_STATX
         while ((statx(0, statistic_file[i], AT_STATX_SYNC_AS_STAT,
                       0, &stat_buf) != 0) && (errno == ENOENT))
#else
         while ((stat(statistic_file[i], &stat_buf) != 0) &&
                (errno == ENOENT))
#endif
         {
            my_usleep(100000L);
         }
         if ((errno != 0) && (errno != ENOENT))
         {
            (void)fprintf(stderr,
                          "ERROR   : Failed to stat() %s : %s (%s %d)\n",
                          statistic_file[i], strerror(errno),
                          __FILE__, __LINE__);
            exit(INCORRECT);
         }
#ifdef HAVE_STATX
         if (stat_buf.stx_size == 0)
#else
         if (stat_buf.st_size == 0)
#endif
         {
            my_usleep(100000L);
         }
#ifdef HAVE_STATX
      } while (stat_buf.stx_size == 0);
#else
      } while (stat_buf.st_size == 0);
#endif

      /* Open file */
      if ((stat_fd = open(statistic_file[i], O_RDONLY)) < 0)
      {
         (void)fprintf(stderr,
                       "ERROR   : Failed to open() %s : %s (%s %d)\n",
                       statistic_file[i], strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }

#ifdef HAVE_MMAP
# ifdef HAVE_STATX
      afdstat_size[i] = stat_buf.stx_size;
# else
      afdstat_size[i] = stat_buf.st_size;
# endif
      if ((p_afd_stat[i] = mmap(NULL, afdstat_size[i],
                                PROT_READ, (MAP_FILE | MAP_SHARED),
                                stat_fd, 0)) == (caddr_t) -1)
#else
      if ((p_afd_stat[i] = mmap_emu(NULL,
# ifdef HAVE_STATX
                                    stat_buf.stx_size,
# else
                                    stat_buf.st_size,
# endif
                                    PROT_READ, (MAP_FILE | MAP_SHARED),
                                    statistic_file[i], 0)) == (caddr_t) -1)
#endif
      {
         (void)fprintf(stderr,
                       "ERROR   : Could not mmap() file %s : %s (%s %d)\n",
                       statistic_file[i], strerror(errno), __FILE__, __LINE__);
         (void)close(stat_fd);
         exit(INCORRECT);
      }
      p_afd_stat[i] = p_afd_stat[i] + AFD_WORD_OFFSET;
# ifdef HAVE_STATX
      no_of_hosts[i] = (stat_buf.stx_size - AFD_WORD_OFFSET) /
                       sizeof(struct afdstat);
# else
      no_of_hosts[i] = (stat_buf.st_size - AFD_WORD_OFFSET) /
                       sizeof(struct afdstat);
# endif
      (void)close(stat_fd);
   }

   tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;
   loops = 0;
   timeout(interval_time, summary);

   do
   {
      (void)sleep(1);
   } while (loops < loops_to_do);

   (void)fprintf(stdout, "---------------------------------------------------------------------------\n");
   (void)fflush(stdout);
   fps = nfs / (double)(interval_time * loops);
   bps = nbs / (double)(interval_time * loops);
   (void)fprintf(stdout, "Total:");
   display_data(nfs, nbs, nc, ne, fps, bps);
   (void)fprintf(stdout, "===========================================================================\n");

   for (i = 0; i < no_of_afds; i++)
   {
      p_afd_stat[i] = p_afd_stat[i] - AFD_WORD_OFFSET;
#ifdef HAVE_MMAP
      if (munmap(p_afd_stat[i], afdstat_size[i]) == -1)
#else
      if (munmap_emu((void *)p_afd_stat[i]) == -1)
#endif
      {
         (void)fprintf(stderr,
                       "ERROR   : Could not munmap() file %s : %s (%s %d)\n",
                       statistic_file[i], strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
#ifdef HAVE_MMAP
   free(afdstat_size);
#endif
   free(p_afd_stat);
   FREE_RT_ARRAY(statistic_file);
   FREE_RT_ARRAY(afd_dir);

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++++ summary() ++++++++++++++++++++++++++++++*/
static void
summary(int dummy)
{
   register int i, j, k;

   /* Make a summary of everything. */
   nfs = nbs = nc = ne = 0.0;
   for (k = 0; k < no_of_afds; k++)
   {
      afd_stat = (struct afdstat *)p_afd_stat[k];
      for (i = 0; i < no_of_hosts[k]; i++)
      {
         for (j = 0; j < afd_stat[i].sec_counter; j++)
         {
            nfs += (double)afd_stat[i].hour[j].nfs;
            nbs +=         afd_stat[i].hour[j].nbs;
            nc  += (double)afd_stat[i].hour[j].nc;
            ne  += (double)afd_stat[i].hour[j].ne;
         }
         for (j = 0; j < afd_stat[i].hour_counter; j++)
         {
            nfs += (double)afd_stat[i].day[j].nfs;
            nbs +=         afd_stat[i].day[j].nbs;
            nc  += (double)afd_stat[i].day[j].nc;
            ne  += (double)afd_stat[i].day[j].ne;
         }
         for (j = 0; j < afd_stat[i].day_counter; j++)
         {
            nfs += (double)afd_stat[i].year[j].nfs;
            nbs +=         afd_stat[i].year[j].nbs;
            nc  += (double)afd_stat[i].year[j].nc;
            ne  += (double)afd_stat[i].year[j].ne;
         }
      }
   }
   fps = (nfs - tmp_nfs) / (double)interval_time;
   bps = (nbs - tmp_nbs) / (double)interval_time;
   (void)fprintf(stdout, "%5d:", loops + 1);
   display_data(nfs - tmp_nfs, nbs - tmp_nbs, nc - tmp_nc, ne - tmp_ne,
                fps, bps);
   tmp_nfs = nfs;
   tmp_nbs = nbs;
   tmp_nc = nc;
   tmp_ne = ne;

   loops++;
   if (loops < loops_to_do)
   {
      timeout(interval_time, summary);
   }

   return;
}


/*+++++++++++++++++++++++++++++ timeout() +++++++++++++++++++++++++++++++*/
static void
timeout(int sec, void (*func)(int))
{
   (void)signal((char)SIGALRM, func);
   (void)alarm(sec);

   return;
}


/*++++++++++++++++++++++++++++ display_data() +++++++++++++++++++++++++++*/
static void
display_data(double nfs, double nbs, double nc, double ne,
             double fps, double bps)
{
   (void)fprintf(stdout, "%11.0f   ", nfs);
   if (nbs >= F_EXABYTE)
   {
      (void)fprintf(stdout, "%7.2f EB", nbs / F_EXABYTE);
   }
   else if (nbs >= F_PETABYTE)
        {
           (void)fprintf(stdout, "%7.2f PB", nbs / F_PETABYTE);
        }
   else if (nbs >= F_TERABYTE)
        {
           (void)fprintf(stdout, "%7.2f TB", nbs / F_TERABYTE);
        }
   else if (nbs >= F_GIGABYTE)
        {
           (void)fprintf(stdout, "%7.2f GB", nbs / F_GIGABYTE);
        }
   else if (nbs >= F_MEGABYTE)
        {
           (void)fprintf(stdout, "%7.2f MB", nbs / F_MEGABYTE);
        }
   else if (nbs >= F_KILOBYTE)
        {
           (void)fprintf(stdout, "%7.2f KB", nbs / F_KILOBYTE);
        }
        else
        {
           (void)fprintf(stdout, "%7.0f B ", nbs);
        }
   (void)fprintf(stdout, "%8.0f", nc);
   (void)fprintf(stdout, "%6.0f", ne);
   if (bps >= F_EXABYTE)
   {
      (void)fprintf(stdout, "  => %8.2f fps %8.2f EB/s\n",
                    fps, bps / F_EXABYTE);
   }
   else if (bps >= F_PETABYTE)
        {
           (void)fprintf(stdout, "  => %8.2f fps %8.2f PB/s\n",
                         fps, bps / F_PETABYTE);
        }
   else if (bps >= F_TERABYTE)
        {
           (void)fprintf(stdout, "  => %8.2f fps %8.2f TB/s\n",
                         fps, bps / F_TERABYTE);
        }
   else if (bps >= F_GIGABYTE)
        {
           (void)fprintf(stdout, "  => %8.2f fps %8.2f GB/s\n",
                         fps, bps / F_GIGABYTE);
        }
   else if (bps >= F_MEGABYTE)
        {
           (void)fprintf(stdout, "  => %8.2f fps %8.2f MB/s\n",
                         fps, bps / F_MEGABYTE);
        }
   else if (bps >= F_KILOBYTE)
        {
           (void)fprintf(stdout, "  => %8.2f fps %8.2f KB/s\n",
                         fps, bps / F_KILOBYTE);
        }
        else
        {
           (void)fprintf(stdout, "  => %8.2f fps %8.2f  B/s\n", fps, bps);
        }
   (void)fflush(stdout);

   return;
}


/*++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{
   (void)fprintf(stderr,
                 "Usage: %s <common dir> <interval> <loops> <sub dir 1>...<sub dir n>\n",
                 progname);
   return;
}
