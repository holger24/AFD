/*
 *  show_istat.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2015 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   show_istat - shows all input statistic information of the AFD
 **
 ** SYNOPSIS
 **   show_istat [options] [DIR_1 DIR_2 .... DIR_n]
 **               -w <work dir>   Working directory of the AFD.
 **               -f <name>       Path and name of the statistics file.
 **               -d [<x>]        Show information of all days [or day
 **                               minus x].
 **               -D              Show total summary on a per day basis.
 **               -h [<x>]        Show information of all hours [or hour
 **                               minus x].
 **               -H              Show total summary of last 24 hours.
 **               -m [<x>]        Show information of all minutes [or
 **                               minute minus x].
 **               -mr <x>         Show the last x minutes.
 **               -M [<x>]        Show total summary of last hour.
 **               -t[u]           Put in a timestamp when the output is valid.
 **               -y [<x>]        Show information of all years [or year
 **                               minus x].
 **               -T              Numeric total only.
 **               --version       Show version.
 **
 ** DESCRIPTION
 **   This program shows all input statistic information of the number
 **   of files and bytes received, the number of files and bytes queued
 **   and the number of files and bytes in the directory for each
 **   directory and a total for all directories, depending on the options
 **   that where used when calling 'show_istat'.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.03.2003 H.Kiehl Created
 **   23.07.2006 H.Kiehl Added -T option.
 **
 */
DESCR__E_M1

#include <stdio.h>                  /* fprintf(), stderr                 */
#include <string.h>                 /* strcpy(), strerror()              */
#include <time.h>                   /* time()                            */
#include <ctype.h>                  /* isdigit()                         */
#ifdef TM_IN_SYS_TIME
# include <sys/time.h>              /* struct tm                         */
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>                 /* getcwd(), close(), STDERR_FILENO  */
#include <stdlib.h>                 /* getenv(), free(), malloc(), exit()*/
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
int         sys_log_fd = STDERR_FILENO; /* Used by get_afd_path() */
char        *p_work_dir,
            **arglist; /* Holds list of dirs from command line when given. */
const char  *sys_log_name = SYSTEM_LOG_FIFO;

/* Local global variables. */
static int  show_numeric_total_only = NO;

/* Function prototypes. */
static void display_data(char *, int, char, int, double, double);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   register int i;
   int          current_year,
                show_min_range = 0,
                show_min = -1,
                show_min_summary = -1,
                show_hour = -1,
                show_hour_summary = -1,
                show_day = -1,
                show_day_summary = -1,
                show_year = -1,
                show_time_stamp = 0,
                show_old_year = NO,
                dir_counter = -1,
                year;
   time_t       now;
   char         work_dir[MAX_PATH_LENGTH],
                statistic_file_name[MAX_FILENAME_LENGTH],
                statistic_file[MAX_PATH_LENGTH];
   struct tm    *p_ts;
   struct stat  stat_buf;

   CHECK_FOR_VERSION(argc, argv);

   /* Evaluate arguments */
   (void)strcpy(statistic_file_name, ISTATISTIC_FILE);
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   eval_input_ss(argc, argv, statistic_file_name, &show_day, &show_day_summary,
                 &show_hour, &show_hour_summary, &show_min_range, &show_min,
                 &show_min_summary, &show_year, &dir_counter, &show_time_stamp,
                 &show_numeric_total_only, YES);

   /* Initialize variables */
   p_work_dir = work_dir;
   now = time(NULL);
   p_ts = localtime(&now);
   current_year = p_ts->tm_year + 1900;
   if (my_strcmp(statistic_file_name, ISTATISTIC_FILE) == 0)
   {
      if (show_day > 0)
      {
         now -= (86400 * show_day);
      }
      else if (show_hour > 0)
           {
              now -= (3600 * show_hour);
           }
      else if (show_min > 0)
           {
              now -= (60 * show_min);
           }
      else if (show_year > 0)
           {
              now -= (31536000 * show_year);
           }
      p_ts = gmtime(&now);
      year = p_ts->tm_year + 1900;
      if (year < current_year)
      {
         show_old_year = YES;
         if (show_day > 0)
         {
            show_day = p_ts->tm_yday;
         }
      }
      (void)sprintf(statistic_file, "%s%s%s.%d",
#ifdef STAT_IN_FIFODIR
                    work_dir, FIFO_DIR, statistic_file_name, year);
#else
                    work_dir, LOG_DIR, statistic_file_name, year);
#endif
   }
   else
   {
      char *ptr;

      (void)strcpy(statistic_file, statistic_file_name);

      ptr = statistic_file_name + strlen(statistic_file_name) - 1;
      i = 0;
      while ((isdigit((int)(*ptr))) && (i < MAX_INT_LENGTH))
      {
         ptr--; i++;
      }
      if (*ptr == '.')
      {
         ptr++;
         year = atoi(ptr);
         if (year < current_year)
         {
            show_old_year = YES;
            if (show_day > 0)
            {
               show_day = p_ts->tm_yday;
            }
         }
      }
      else
      {
         /* We cannot know from what year this file is, so set to zero. */
         year = 0;
      }
   }

   if (stat(statistic_file, &stat_buf) != 0)
   {
      (void)fprintf(stderr, "ERROR   : Failed to stat() %s : %s (%s %d)\n",
                    statistic_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (stat_buf.st_size > 0)
   {
      register int j;
      int          no_of_dirs,
                   position,
                   stat_fd;
      double       nfr = 0.0, nbr = 0.0,
                   tmp_nfr = 0.0, tmp_nbr = 0.0,
                   total_nfr = 0.0, total_nbr = 0.0;

      /* Open file */
      if ((stat_fd = open(statistic_file, O_RDONLY)) < 0)
      {
         (void)fprintf(stderr, "ERROR   : Failed to open() %s : %s (%s %d)\n",
                       statistic_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }

      if (show_old_year == YES)
      {
         char                  *ptr;
         struct afd_year_istat *afd_istat;
         
#ifdef HAVE_MMAP
         if ((ptr = mmap(NULL, stat_buf.st_size, PROT_READ,
                         (MAP_FILE | MAP_SHARED),
                         stat_fd, 0)) == (caddr_t) -1)
         {
            (void)fprintf(stderr,
                          "ERROR   : Could not mmap() file %s : %s (%s %d)\n",
                          statistic_file, strerror(errno), __FILE__, __LINE__);
            (void)close(stat_fd);
            exit(INCORRECT);
         }
#else
         if ((ptr = malloc(stat_buf.st_size)) == NULL)
         {
            (void)fprintf(stderr,
                          "ERROR   : Failed to allocate memory : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            (void)close(stat_fd);
            exit(INCORRECT);
         }

         if (read(stat_fd, ptr, stat_buf.st_size) != stat_buf.st_size)
         {
            (void)fprintf(stderr,
                          "ERROR   : Failed to read() %s : %s (%s %d)\n",
                          statistic_file, strerror(errno), __FILE__, __LINE__);
            free(ptr);
            (void)close(stat_fd);
            exit(INCORRECT);
         }
#endif /* HAVE_MMAP */
         afd_istat = (struct afd_year_istat *)(ptr + AFD_WORD_OFFSET);

         no_of_dirs = (stat_buf.st_size - AFD_WORD_OFFSET) /
                      sizeof(struct afd_year_istat);
         if (show_year != -1)
         {
            /*
             * Show total for all directories.
             */
            tmp_nfr = tmp_nbr = 0.0;

            if (show_time_stamp > 0)
            {
               time_t    first_time,
                         last_time;
               struct tm *p_ts;

               p_ts = localtime(&now);
               p_ts->tm_year = year - 1900;
               p_ts->tm_mon = 0;
               p_ts->tm_mday = 1;
               p_ts->tm_hour = 0;
               p_ts->tm_min = 0;
               p_ts->tm_sec = 0;
               first_time = mktime(p_ts);
               p_ts->tm_year = year + 1 - 1900;
               last_time = mktime(p_ts);
               if (show_time_stamp == 1)
               {
                  char first_time_str[25],
                       last_time_str[25];

                  (void)strftime(first_time_str, 25, "%c",
                                 localtime(&first_time));
                  (void)strftime(last_time_str, 25, "%c",
                                 localtime(&last_time));
                  (void)fprintf(stdout, "          [time span %s -> %s]\n",
                                first_time_str, last_time_str);
               }
               else
               {
                  (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                                "                   [time span %ld -> %ld]\n",
#else
                                "                   [time span %lld -> %lld]\n",
#endif
                                (pri_time_t)first_time, (pri_time_t)last_time);
               }
            }

            if (show_numeric_total_only == NO)
            {
               (void)fprintf(stdout, "                   ===================================\n");
               (void)fprintf(stdout, "==================> AFD INPUT STATISTICS SUMMARY %d <==================\n", year);
               (void)fprintf(stdout, "                   ===================================\n");
            }

            if (dir_counter > 0)
            {
               for (i = 0; i < dir_counter; i++)
               {
                  if ((position = locate_dir_year(afd_istat, arglist[i],
                                                  no_of_dirs)) < 0)
                  {
                     (void)fprintf(stdout,
                                   "No directory %s found in statistic database.\n",
                                   arglist[i]);
                  }
                  else
                  {
                     nfr = nbr = 0.0;
                     for (j = 0; j < DAYS_PER_YEAR; j++)
                     {
                        nfr += (double)afd_istat[position].year[j].nfr;
                        nbr +=         afd_istat[position].year[j].nbr;
                     }
                     display_data(afd_istat[position].dir_alias,
                                  -1, ' ', -1, nfr, nbr);
                     tmp_nfr += nfr; tmp_nbr += nbr;
                  }
               }
            }
            else
            {
               if (show_day_summary == 0)
               {
                  for (j = 0; j < DAYS_PER_YEAR; j++)
                  {
                     nfr = nbr = 0.0;
                     for (i = 0; i < no_of_dirs; i++)
                     {
                        nfr += (double)afd_istat[i].year[j].nfr;
                        nbr +=         afd_istat[i].year[j].nbr;
                     }
                     display_data(" ", -1, ' ', j, nfr, nbr);
                     tmp_nfr += nfr;
                     tmp_nbr += nbr;
                  }
               }
               else
               {
                  /* Make a summary of everything */
                  for (i = 0; i < no_of_dirs; i++)
                  {
                     nfr = nbr = 0.0;
                     for (j = 0; j < DAYS_PER_YEAR; j++)
                     {
                        nfr += (double)afd_istat[i].year[j].nfr;
                        nbr +=         afd_istat[i].year[j].nbr;
                     }
                     display_data(afd_istat[i].dir_alias,
                                  -1, ' ', -1, nfr, nbr);
                     tmp_nfr += nfr; tmp_nbr += nbr;
                  }
               }
            }

            if (show_numeric_total_only == NO)
            {
               (void)fprintf(stdout, "----------------------------------------------------------------------\n");
               display_data("Total", -1, ' ', -1, tmp_nfr, tmp_nbr);
               (void)fprintf(stdout, "======================================================================\n");
            }
            else
            {
               (void)fprintf(stdout, "%.0f %.0f\n", tmp_nfr, tmp_nbr);
            }
         }
         else
         {
            /*
             * Show data for one or all days for this year.
             */
            if (show_day > -1)
            {
               tmp_nfr = tmp_nbr = 0.0;
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "                       ========================\n");
                  (void)fprintf(stdout, "=====================> AFD INPUT STATISTICS DAY <=====================\n");
                  (void)fprintf(stdout, "                       ========================\n");
               }
               if (dir_counter < 0)
               {
                  for (i = 0; i < no_of_dirs; i++)
                  {
                     nfr = nbr = 0.0;
                     if (show_day == 0) /* Show all days */
                     {
                        display_data(afd_istat[i].dir_alias, -1, ' ', 0,
                                     afd_istat[i].year[0].nfr,
                                     afd_istat[i].year[0].nbr);
                        for (j = 1; j < DAYS_PER_YEAR; j++)
                        {
                           display_data(" ", -1, ' ', j,
                                        afd_istat[i].year[j].nfr,
                                        afd_istat[i].year[j].nbr);
                           nfr += (double)afd_istat[i].year[j].nfr;
                           nbr +=         afd_istat[i].year[j].nbr;
                        }
                     }
                     else /* Show a specific day */
                     {
                        nfr += (double)afd_istat[i].year[show_day].nfr;
                        nbr +=         afd_istat[i].year[show_day].nbr;
                        display_data(afd_istat[i].dir_alias, -1, ' ', -1,
                                     afd_istat[i].year[show_day].nfr,
                                     afd_istat[i].year[show_day].nbr);
                     }
                     tmp_nfr += nfr; tmp_nbr += nbr;
                  }
               } /* if (dir_counter < 0) */
               else /* Show some specific directory */
               {
                  for (i = 0; i < dir_counter; i++)
                  {
                     if ((position = locate_dir_year(afd_istat, arglist[i],
                                                     no_of_dirs)) < 0)
                     {
                        (void)fprintf(stdout,
                                      "No directory %s found in statistic database.\n",
                                      arglist[i]);
                     }
                     else
                     {
                        nfr = nbr = 0.0;
                        if (show_day == 0) /* Show all days */
                        {
                           display_data(afd_istat[position].dir_alias,
                                        -1, ' ', 0,
                                        afd_istat[position].year[0].nfr,
                                        afd_istat[position].year[0].nbr);
                           for (j = 1; j < DAYS_PER_YEAR; j++)
                           {
                              display_data(" ", -1, ' ', j,
                                           afd_istat[position].year[j].nfr,
                                           afd_istat[position].year[j].nbr);
                              nfr += (double)afd_istat[position].year[j].nfr;
                              nbr +=         afd_istat[position].year[j].nbr;
                           }
                        }
                        else /* Show a specific interval */
                        {
                           nfr += (double)afd_istat[position].year[show_day].nfr;
                           nbr +=         afd_istat[position].year[show_day].nbr;
                           display_data(afd_istat[position].dir_alias,
                                        -1, ' ', -1,
                                        afd_istat[position].year[show_day].nfr,
                                        afd_istat[position].year[show_day].nbr);
                        }
                        tmp_nfr += nfr; tmp_nbr += nbr;
                     }
                  }
               }

               if ((show_year > -1) || (show_day_summary > -1))
               {
                  if (show_numeric_total_only == NO)
                  {
                     display_data("Total", -1, ' ', -1, tmp_nfr, tmp_nbr);
                  }
                  else
                  {
                     (void)fprintf(stdout, "%.0f %.0f\n", tmp_nfr, tmp_nbr);
                  }
               }
               else
               {
                  total_nfr += tmp_nfr; total_nbr += tmp_nbr;
               }
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "======================================================================\n");
               }
            } /* if (show_day > -1) */

            /*
             * Show total summary on a per day basis for this year.
             */
            if (show_day_summary > -1)
            {
               struct tm *p_ts;

               if (show_time_stamp > 0)
               {
                  time_t first_time, last_time;

                  p_ts = localtime(&now);
                  p_ts->tm_year = year - 1900;
                  p_ts->tm_mon = 0;
                  p_ts->tm_mday = 1;
                  p_ts->tm_hour = 0;
                  p_ts->tm_min = 0;
                  p_ts->tm_sec = 0;
                  first_time = mktime(p_ts);
                  p_ts->tm_year = year + 1 - 1900;
                  last_time = mktime(p_ts);
                  if (show_time_stamp == 1)
                  {
                     char first_time_str[25],
                          last_time_str[25];

                     (void)strftime(first_time_str, 25, "%c",
                                    localtime(&first_time));
                     (void)strftime(last_time_str, 25, "%c",
                                    localtime(&last_time));
                     (void)fprintf(stdout, "        [time span %s -> %s]\n",
                                   first_time_str, last_time_str);
                  }
                  else
                  {
                     (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                                   "                 [time span %ld -> %ld]\n",
#else
                                   "                 [time span %lld -> %lld]\n",
#endif
                                   (pri_time_t)first_time,
                                   (pri_time_t)last_time);
                  }
               }

               tmp_nfr = tmp_nbr = 0.0;
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "                   ================================\n");
                  (void)fprintf(stdout, "=================> AFD INPUT STATISTICS DAY SUMMARY <=================\n");
                  (void)fprintf(stdout, "                   ================================\n");
               }
               for (j = 0; j < DAYS_PER_YEAR; j++)
               {
                  nfr = nbr = 0.0;
                  for (i = 0; i < no_of_dirs; i++)
                  {
                     nfr += (double)afd_istat[i].year[j].nfr;
                     nbr +=         afd_istat[i].year[j].nbr;
                  }
                  display_data(" ", -1, ' ', j, nfr, nbr);
                  tmp_nfr += nfr;
                  tmp_nbr += nbr;
               }

               if ((show_year > -1) || (show_day > -1))
               {
                  if (show_numeric_total_only == NO)
                  {
                     display_data("Total", -1, ' ', -1, tmp_nfr, tmp_nbr);
                  }
                  else
                  {
                     (void)fprintf(stdout, "%.0f %.0f\n", tmp_nfr, tmp_nbr);
                  }
               }
               else
               {
                  total_nfr += tmp_nfr;
                  total_nbr += tmp_nbr;
               }
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "======================================================================\n");
               }
            } /* if (show_day_summary > -1) */

            if (show_numeric_total_only == NO)
            {
               display_data("Total", -1, ' ', -1, total_nfr, total_nbr);
            }
            else
            {
               (void)fprintf(stdout, "%.0f %.0f\n", total_nfr, total_nbr);
            }
         }

#ifdef HAVE_MMAP
         if (munmap(ptr, stat_buf.st_size) < 0)
         {
            (void)fprintf(stderr,
                          "ERROR   : Could not munmap() file %s : %s (%s %d)\n",
                          statistic_file, strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
#else
         free(ptr);
#endif /* HAVE_MMAP */
      }
      else /* Show data of current year */
      {
         char            *ptr;
         struct afdistat *afd_istat;

#ifdef HAVE_MMAP
         if ((ptr = mmap(NULL, stat_buf.st_size, PROT_READ,
                         (MAP_FILE | MAP_SHARED), stat_fd, 0)) == (caddr_t) -1)
         {
            (void)fprintf(stderr,
                          "ERROR   : Could not mmap() file %s : %s (%s %d)\n",
                          statistic_file, strerror(errno), __FILE__, __LINE__);
            (void)close(stat_fd);
            exit(INCORRECT);
         }
#else
         if ((ptr = malloc(stat_buf.st_size)) == NULL)
         {
            (void)fprintf(stderr,
                          "ERROR   : Failed to allocate memory : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            (void)close(stat_fd);
            exit(INCORRECT);
         }

         if (read(stat_fd, ptr, stat_buf.st_size) != stat_buf.st_size)
         {
            (void)fprintf(stderr,
                          "ERROR   : Failed to read() %s : %s (%s %d)\n",
                          statistic_file, strerror(errno), __FILE__, __LINE__);
            free(ptr);
            (void)close(stat_fd);
            exit(INCORRECT);
         }
#endif /* HAVE_MMAP */
         afd_istat = (struct afdistat *)(ptr + AFD_WORD_OFFSET);
         no_of_dirs = (stat_buf.st_size - AFD_WORD_OFFSET) /
                      sizeof(struct afdistat);

         if (show_min_range)
         {
            int left,
                sec_ints = (show_min_range * 60) / STAT_RESCAN_TIME;

            if (show_time_stamp > 0)
            {
               time_t    first_time,
                         last_time;
               struct tm *p_ts;

               p_ts = localtime(&now);
               p_ts->tm_year = year - 1900;
               p_ts->tm_mon = 0;
               p_ts->tm_mday = 1;
               p_ts->tm_hour = afd_istat[0].hour_counter;
               p_ts->tm_min = (afd_istat[0].sec_counter * STAT_RESCAN_TIME) / 60;
               p_ts->tm_sec = (afd_istat[0].sec_counter * STAT_RESCAN_TIME) % 60;
               last_time = mktime(p_ts) + (86400 * afd_istat[0].day_counter);
               first_time = last_time - (sec_ints * STAT_RESCAN_TIME);
               if (show_time_stamp == 1)
               {
                  char first_time_str[25],
                       last_time_str[25];

                  (void)strftime(first_time_str, 25, "%c",
                                 localtime(&first_time));
                  (void)strftime(last_time_str, 25, "%c",
                                 localtime(&last_time));
                  (void)fprintf(stdout, "        [time span %s -> %s]\n",
                                first_time_str, last_time_str);
               }
               else
               {
                  (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                                "                 [time span %ld -> %ld]\n",
#else
                                "                 [time span %lld -> %lld]\n",
#endif
                                (pri_time_t)first_time, (pri_time_t)last_time);
               }
            }
            tmp_nfr = tmp_nbr = 0.0;
            if (show_numeric_total_only == NO)
            {
               (void)fprintf(stdout, "                ========================================\n");
               (void)fprintf(stdout, "==============> AFD INPUT STATISTICS LAST %2d MINUTE(S) <==============\n", show_min_range);
               (void)fprintf(stdout, "                ========================================\n");
            }
            if (dir_counter < 0)
            {
               for (i = 0; i < no_of_dirs; i++)
               {
                  nfr = nbr = 0.0;
                  left = afd_istat[i].sec_counter - sec_ints;
                  if (left < 0)
                  {
                     for (j = (SECS_PER_HOUR + left); j < SECS_PER_HOUR; j++)
                     {
                        nfr += (double)afd_istat[i].hour[j].nfr;
                        nbr +=         afd_istat[i].hour[j].nbr;
                     }
                     for (j = 0; j < (sec_ints + left); j++)
                     {
                        nfr += (double)afd_istat[i].hour[j].nfr;
                        nbr +=         afd_istat[i].hour[j].nbr;
                     }
                  }
                  else
                  {
                     for (j = left; j < afd_istat[i].sec_counter; j++)
                     {
                        nfr += (double)afd_istat[i].hour[j].nfr;
                        nbr +=         afd_istat[i].hour[j].nbr;
                     }
                  }
                  display_data(afd_istat[i].dir_alias, -1, ' ', -1, nfr, nbr);
                  tmp_nfr += nfr; tmp_nbr += nbr;
               }
            }
            else
            {
               for (i = 0; i < dir_counter; i++)
               {
                  if ((position = locate_dir(afd_istat, arglist[i],
                                             no_of_dirs)) < 0)
                  {
                     (void)fprintf(stdout,
                                   "No host %s found in statistic database.\n",
                                   arglist[i]);
                  }
                  else
                  {
                     nfr = nbr = 0.0;
                     left = afd_istat[position].sec_counter - sec_ints;
                     if (left < 0)
                     {
                        for (j = (SECS_PER_HOUR + left); j < SECS_PER_HOUR; j++)
                        {
                           nfr += (double)afd_istat[position].hour[j].nfr;
                           nbr +=         afd_istat[position].hour[j].nbr;
                        }
                        for (j = 0; j < (sec_ints + left); j++)
                        {
                           nfr += (double)afd_istat[position].hour[j].nfr;
                           nbr +=         afd_istat[position].hour[j].nbr;
                        }
                     }
                     else
                     {
                        for (j = left; j < afd_istat[position].sec_counter; j++)
                        {
                           nfr += (double)afd_istat[position].hour[j].nfr;
                           nbr +=         afd_istat[position].hour[j].nbr;
                        }
                     }
                     display_data(afd_istat[position].dir_alias, -1, ' ', -1,
                                  nfr, nbr);
                     tmp_nfr += nfr; tmp_nbr += nbr;
                  }
               }
            }
            if (show_numeric_total_only == NO)
            {
               (void)fprintf(stdout, "----------------------------------------------------------------------\n");
               display_data("Total", -1, ' ', -1, tmp_nfr, tmp_nbr);
               (void)fprintf(stdout, "======================================================================\n");
            }
            else
            {
               (void)fprintf(stdout, "%.0f %.0f\n", tmp_nfr, tmp_nbr);
            }

#ifdef HAVE_MMAP
            if (munmap(ptr, stat_buf.st_size) < 0)
            {
               (void)fprintf(stderr,
                             "ERROR   : Could not munmap() file %s : %s (%s %d)\n",
                             statistic_file, strerror(errno),
                             __FILE__, __LINE__);
               exit(INCORRECT);
            }
#else
            free(ptr);
#endif /* HAVE_MMAP */
            if (close(stat_fd) == -1)
            {
               (void)fprintf(stderr,
                             "WARNING : Could not close() file %s : %s (%s %d)\n",
                             statistic_file, strerror(errno),
                             __FILE__, __LINE__);
            }
            exit(SUCCESS);
         }

         if ((show_day == -1) && (show_year == -1) && (show_hour == -1) &&
             (show_min == -1) && (show_hour_summary == -1) &&
             (show_day_summary == -1) && (show_min_summary == -1))
         {
            /*
             * Show total for all directories.
             */
            tmp_nfr = tmp_nbr = 0.0;

            if (show_numeric_total_only == NO)
            {
               (void)fprintf(stdout, "                     ============================\n");
               (void)fprintf(stdout, "===================> AFD INPUT STATISTICS SUMMARY <===================\n");
               (void)fprintf(stdout, "                     ============================\n");
            }

            if (dir_counter > 0)
            {
               for (i = 0; i < dir_counter; i++)
               {
                  if ((position = locate_dir(afd_istat, arglist[i],
                                             no_of_dirs)) < 0)
                  {
                     (void)fprintf(stdout,
                                   "No directory %s found in statistic database.\n",
                                   arglist[i]);
                  }
                  else
                  {
                     nfr = nbr = 0.0;
                     for (j = 0; j < afd_istat[position].sec_counter; j++)
                     {
                        nfr += (double)afd_istat[position].hour[j].nfr;
                        nbr +=         afd_istat[position].hour[j].nbr;
                     }
                     for (j = 0; j < afd_istat[position].hour_counter; j++)
                     {
                        nfr += (double)afd_istat[position].day[j].nfr;
                        nbr +=         afd_istat[position].day[j].nbr;
                     }
                     for (j = 0; j < afd_istat[position].day_counter; j++)
                     {
                        nfr += (double)afd_istat[position].year[j].nfr;
                        nbr +=         afd_istat[position].year[j].nbr;
                     }
                     tmp_nfr += nfr; tmp_nbr += nbr;
                     display_data(afd_istat[position].dir_alias, -1, ' ', -1,
                                  nfr, nbr);
                  }
               }
            }
            else
            {
               /* Make a summary of everything */
               for (i = 0; i < no_of_dirs; i++)
               {
                  nfr = nbr = 0.0;
                  for (j = 0; j < afd_istat[i].sec_counter; j++)
                  {
                     nfr += (double)afd_istat[i].hour[j].nfr;
                     nbr +=         afd_istat[i].hour[j].nbr;
                  }
                  for (j = 0; j < afd_istat[i].hour_counter; j++)
                  {
                     nfr += (double)afd_istat[i].day[j].nfr;
                     nbr +=         afd_istat[i].day[j].nbr;
                  }
                  for (j = 0; j < afd_istat[i].day_counter; j++)
                  {
                     nfr += (double)afd_istat[i].year[j].nfr;
                     nbr +=         afd_istat[i].year[j].nbr;
                  }
                  tmp_nfr += nfr; tmp_nbr += nbr;
                  display_data(afd_istat[i].dir_alias, -1, ' ', -1, nfr, nbr);
               }
            }
            if (show_numeric_total_only == NO)
            {
               (void)fprintf(stdout, "----------------------------------------------------------------------\n");
               display_data("Total", -1, ' ', -1, tmp_nfr, tmp_nbr);
               (void)fprintf(stdout, "======================================================================\n");
            }
            else
            {
               (void)fprintf(stdout, "%.0f %.0f\n", tmp_nfr, tmp_nbr);
            }
         }
         else
         {
            /*
             * Show data for one or all days for this year.
             */
            if (show_day > -1)
            {
               tmp_nfr = tmp_nbr = 0.0;
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "                       ========================\n");
                  (void)fprintf(stdout, "=====================> AFD INPUT STATISTICS DAY <=====================\n");
                  (void)fprintf(stdout, "                       ========================\n");
               }
               if (dir_counter < 0)
               {
                  for (i = 0; i < no_of_dirs; i++)
                  {
                     nfr = nbr = 0.0;
                     if (show_day == 0) /* Show all days */
                     {
                        if (afd_istat[i].day_counter == 0)
                        {
                           display_data(afd_istat[i].dir_alias, -1,
                                        ' ', 0, 0.0, 0.0);
                        }
                        else
                        {
                           display_data(afd_istat[i].dir_alias, -1, ' ', 0,
                                        afd_istat[i].year[0].nfr,
                                        afd_istat[i].year[0].nbr);
                           for (j = 1; j < afd_istat[i].day_counter; j++)
                           {
                              display_data(" ", -1, ' ', j,
                                           afd_istat[i].year[j].nfr,
                                           afd_istat[i].year[j].nbr);
                              nfr += (double)afd_istat[i].year[j].nfr;
                              nbr +=         afd_istat[i].year[j].nbr;
                           }
                        }
                     }
                     else /* Show a specific day */
                     {
                        if (show_day < DAYS_PER_YEAR)
                        {
                           if (afd_istat[i].day_counter < show_day)
                           {
                              j = DAYS_PER_YEAR -
                                  (show_day - afd_istat[i].day_counter);
                           }
                           else
                           {
                              j = afd_istat[i].day_counter - show_day;
                           }
                           nfr += (double)afd_istat[i].year[j].nfr;
                           nbr +=         afd_istat[i].year[j].nbr;
                           display_data(afd_istat[i].dir_alias, -1, ' ', -1,
                                        afd_istat[i].year[j].nfr,
                                        afd_istat[i].year[j].nbr);
                        }
                        else
                        {
                           display_data(afd_istat[i].dir_alias, -1,
                                        ' ', -1, 0.0, 0.0);
                        }
                     }
                     tmp_nfr += nfr; tmp_nbr += nbr;
                  }
               } /* if (dir_counter < 0) */
               else /* Show some specific directory */
               {
                  for (i = 0; i < dir_counter; i++)
                  {
                     if ((position = locate_dir(afd_istat, arglist[i],
                                                no_of_dirs)) < 0)
                     {
                        (void)fprintf(stdout,
                                      "No direcotry %s found in statistic database.\n",
                                      arglist[i]);
                     }
                     else
                     {
                        nfr = nbr = 0.0;
                        if (show_day == 0) /* Show all days */
                        {
                           display_data(afd_istat[position].dir_alias,
                                        -1, ' ', 0,
                                        afd_istat[position].year[0].nfr,
                                        afd_istat[position].year[0].nbr);
                           for (j = 1; j < afd_istat[position].day_counter; j++)
                           {
                              display_data(" ", -1, ' ', j,
                                           afd_istat[position].year[j].nfr,
                                           afd_istat[position].year[j].nbr);
                              nfr += (double)afd_istat[position].year[j].nfr;
                              nbr +=         afd_istat[position].year[j].nbr;
                           }
                        }
                        else /* Show a specific interval */
                        {
                           if (show_day < DAYS_PER_YEAR)
                           {
                              if (afd_istat[position].day_counter < show_day)
                              {
                                 j = DAYS_PER_YEAR -
                                     (show_day - afd_istat[position].day_counter);
                              }
                              else
                              {
                                 j = afd_istat[position].day_counter - show_day;
                              }
                              nfr += (double)afd_istat[position].year[j].nfr;
                              nbr +=         afd_istat[position].year[j].nbr;
                              display_data(afd_istat[position].dir_alias,
                                           -1, ' ', -1,
                                           afd_istat[position].year[j].nfr,
                                           afd_istat[position].year[j].nbr);
                           }
                           else
                           {
                              display_data(afd_istat[position].dir_alias,
                                           -1, ' ', -1, 0.0, 0.0);
                           }
                        }
                        tmp_nfr += nfr; tmp_nbr += nbr;
                     }
                  }
               }

               if ((show_year > -1) || (show_hour > -1) ||
                   (show_day_summary > -1) || (show_hour_summary > -1))
               {
                  if (show_numeric_total_only == NO)
                  {
                     display_data("Total", -1, ' ', -1, tmp_nfr, tmp_nbr);
                  }
                  else
                  {
                     (void)fprintf(stdout, "%.0f %.0f\n", tmp_nfr, tmp_nbr);
                  }
               }
               else
               {
                  total_nfr += tmp_nfr; total_nbr += tmp_nbr;
               }
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "======================================================================\n");
               }
            } /* if (show_day > -1) */

            /*
             * Show total summary on a per day basis for this year.
             */
            if (show_day_summary > -1)
            {
               struct tm *p_ts;

               p_ts = localtime(&now);
               tmp_nfr = tmp_nbr = 0.0;
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "                   ================================\n");
                  (void)fprintf(stdout, "=================> AFD INPUT STATISTICS DAY SUMMARY <=================\n");
                  (void)fprintf(stdout, "                   ================================\n");
               }
               for (j = 0; j < p_ts->tm_yday; j++)
               {
                  nfr = nbr = 0.0;
                  for (i = 0; i < no_of_dirs; i++)
                  {
                     nfr += (double)afd_istat[i].year[j].nfr;
                     nbr +=         afd_istat[i].year[j].nbr;
                  }
                  display_data(" ", -1, ' ', j, nfr, nbr);
                  tmp_nfr += nfr; tmp_nbr += nbr;
               }

               if ((show_year > -1) || (show_day > -1) ||
                   (show_hour > -1) || (show_hour_summary > -1))
               {
                  if (show_numeric_total_only == NO)
                  {
                     display_data("Total", -1, ' ', -1, tmp_nfr, tmp_nbr);
                  }
                  else
                  {
                     (void)fprintf(stdout, "%.0f %.0f\n", tmp_nfr, tmp_nbr);
                  }
               }
               else
               {
                  total_nfr += tmp_nfr; total_nbr += tmp_nbr;
               }
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "======================================================================\n");
               }
            } /* if (show_day_summary > -1) */

            /*
             * Show data for one or all hours for this day.
             */
            if (show_hour > -1)
            {
               double sec_nfr, sec_nbr;

               tmp_nfr = tmp_nbr = 0.0;
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "                       =========================\n");
                  (void)fprintf(stdout, "=====================> AFD INPUT STATISTICS HOUR <====================\n");
                  (void)fprintf(stdout, "                       =========================\n");
               }
               if (dir_counter < 0)
               {
                  for (i = 0; i < no_of_dirs; i++)
                  {
                     nfr = nbr = 0.0;
                     if (show_hour == 0) /* Show all hours of the day */
                     {
                        sec_nfr = sec_nbr = 0.0;
                        for (j = 0; j < afd_istat[i].sec_counter; j++)
                        {
                           sec_nfr += (double)afd_istat[i].hour[j].nfr;
                           sec_nbr +=         afd_istat[i].hour[j].nbr;
                        }
                        if (afd_istat[i].hour_counter == 0)
                        {
                           display_data(afd_istat[i].dir_alias,
                                        -1, '*', 0, sec_nfr, sec_nbr);
                        }
                        else
                        {
                           display_data(afd_istat[i].dir_alias, -1, ' ', 0,
                                        afd_istat[i].day[0].nfr,
                                        afd_istat[i].day[0].nbr);
                           for (j = 1; j < afd_istat[i].hour_counter; j++)
                           {
                              display_data(" ", -1, ' ', j,
                                           afd_istat[i].day[j].nfr,
                                           afd_istat[i].day[j].nbr);
                              nfr += (double)afd_istat[i].day[j].nfr;
                              nbr +=         afd_istat[i].day[j].nbr;
                           }
                           display_data(" ", -1, '*', j, sec_nfr, sec_nbr);
                        }
                        nfr += sec_nfr; nbr += sec_nbr;
                        for (j = (afd_istat[i].hour_counter + 1);
                             j < HOURS_PER_DAY; j++)
                        {
                           display_data(" ", -1, ' ', j,
                                        afd_istat[i].day[j].nfr,
                                        afd_istat[i].day[j].nbr);
                           nfr += (double)afd_istat[i].day[j].nfr;
                           nbr +=         afd_istat[i].day[j].nbr;
                        }
                     }
                     else /* Show a specific hour */
                     {
                        if (show_hour < HOURS_PER_DAY)
                        {
                           if (afd_istat[i].hour_counter < show_hour)
                           {
                              j = HOURS_PER_DAY -
                                  (show_hour - afd_istat[i].hour_counter);
                           }
                           else
                           {
                              j = afd_istat[i].hour_counter - show_hour;
                           }
                           nfr += (double)afd_istat[i].day[j].nfr;
                           nbr +=         afd_istat[i].day[j].nbr;
                           display_data(afd_istat[i].dir_alias, -1, ' ', -1,
                                        afd_istat[i].day[j].nfr,
                                        afd_istat[i].day[j].nbr);
                        }
                        else
                        {
                           display_data(afd_istat[i].dir_alias, -1, ' ',
                                        -1, 0.0, 0.0);
                        }
                     }
                     tmp_nfr += nfr; tmp_nbr += nbr;
                  }
               } /* if (dir_counter < 0) */
               else /* Show some specific directory */
               {
                  for (i = 0; i < dir_counter; i++)
                  {
                     if ((position = locate_dir(afd_istat, arglist[i],
                                                no_of_dirs)) < 0)
                     {
                        (void)fprintf(stdout,
                                     "No directory %s found in statistic database.\n",
                                     arglist[i]);
                     }
                     else
                     {
                        nfr = nbr = 0.0;
                        if (show_hour == 0) /* Show all hours of the day */
                        {
                           sec_nfr = sec_nbr = 0.0;
                           for (j = 0; j < afd_istat[position].sec_counter; j++)
                           {
                              sec_nfr += (double)afd_istat[position].hour[j].nfr;
                              sec_nbr +=         afd_istat[position].hour[j].nbr;
                           }
                           if (afd_istat[i].hour_counter == 0)
                           {
                              display_data(afd_istat[position].dir_alias,
                                           -1, '*', 0, sec_nfr, sec_nbr);
                           }
                           else
                           {
                              display_data(afd_istat[position].dir_alias,
                                           -1, ' ', 0,
                                           afd_istat[position].day[0].nfr,
                                           afd_istat[position].day[0].nbr);
                              for (j = 1; j < afd_istat[position].hour_counter; j++)
                              {
                                 display_data(" ", -1, ' ', j,
                                              afd_istat[position].day[j].nfr,
                                              afd_istat[position].day[j].nbr);
                                 nfr += (double)afd_istat[position].day[j].nfr;
                                 nbr +=         afd_istat[position].day[j].nbr;
                              }
                              display_data(" ", -1, '*', j, sec_nfr, sec_nbr);
                           }
                           nfr += sec_nfr; nbr += sec_nbr;
                           for (j = (afd_istat[position].hour_counter + 1);
                                j < HOURS_PER_DAY; j++)
                           {
                              display_data(" ", -1, ' ', j,
                                           afd_istat[position].day[j].nfr,
                                           afd_istat[position].day[j].nbr);
                              nfr += (double)afd_istat[position].day[j].nfr;
                              nbr +=         afd_istat[position].day[j].nbr;
                           }
                        }
                        else /* Show a specific interval */
                        {
                           if (show_hour < HOURS_PER_DAY)
                           {
                              if (afd_istat[position].hour_counter < show_hour)
                              {
                                 j = HOURS_PER_DAY -
                                     (show_hour - afd_istat[position].hour_counter);
                              }
                              else
                              {
                                 j = afd_istat[position].hour_counter -
                                     show_hour;
                              }
                              nfr += (double)afd_istat[position].day[j].nfr;
                              nbr +=         afd_istat[position].day[j].nbr;
                              display_data(afd_istat[position].dir_alias,
                                           -1, ' ', j,
                                           afd_istat[position].day[j].nfr,
                                           afd_istat[position].day[j].nbr);
                           }
                           else
                           {
                              display_data(afd_istat[position].dir_alias,
                                           -1, ' ', -1, 0.0, 0.0);
                           }
                        }
                        tmp_nfr += nfr; tmp_nbr += nbr;
                     }
                  }
               }

               if ((show_year > -1) || (show_day > -1) ||
                   (show_day_summary > -1) || (show_hour_summary > -1))
               {
                  if (show_numeric_total_only == NO)
                  {
                     display_data("Total", -1, ' ', -1, tmp_nfr, tmp_nbr);
                  }
                  else
                  {
                     (void)fprintf(stdout, "%.0f %.0f\n", tmp_nfr, tmp_nbr);
                  }
               }
               else
               {
                  total_nfr += tmp_nfr; total_nbr += tmp_nbr;
               }
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "======================================================================\n");
               }
            } /* if (show_hour > -1) */

            /*
             * Show total summary on a per hour basis for the last 24 hours.
             */
            if (show_hour_summary > -1)
            {
               tmp_nfr = tmp_nbr = 0.0;
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "                   =================================\n");
                  (void)fprintf(stdout, "=================> AFD INPUT STATISTICS HOUR SUMMARY <================\n");
                  (void)fprintf(stdout, "                   =================================\n");
               }
               for (j = 0; j < afd_istat[0].hour_counter; j++)
               {
                  nfr = nbr = 0.0;
                  for (i = 0; i < no_of_dirs; i++)
                  {
                     nfr += (double)afd_istat[i].day[j].nfr;
                     nbr +=         afd_istat[i].day[j].nbr;
                  }
                  display_data(" ", -1, ' ', j, nfr, nbr);
                  tmp_nfr += nfr; tmp_nbr += nbr;
               }
               nfr = nbr = 0.0;
               for (i = 0; i < no_of_dirs; i++)
               {
                  for (j = 0; j < afd_istat[i].sec_counter; j++)
                  {
                     nfr += (double)afd_istat[i].hour[j].nfr;
                     nbr +=         afd_istat[i].hour[j].nbr;
                  }
               }
               display_data(" ", -1, '*', afd_istat[0].hour_counter, nfr, nbr);
               tmp_nfr += nfr; tmp_nbr += nbr;
               for (j = (afd_istat[0].hour_counter + 1); j < HOURS_PER_DAY; j++)
               {
                  nfr = nbr = 0.0;
                  for (i = 0; i < no_of_dirs; i++)
                  {
                     nfr += (double)afd_istat[i].day[j].nfr;
                     nbr +=         afd_istat[i].day[j].nbr;
                  }
                  display_data(" ", -1, ' ', j, nfr, nbr);
                  tmp_nfr += nfr; tmp_nbr += nbr;
               }

               if ((show_year > -1) || (show_day > -1) ||
                   (show_day_summary > -1) || (show_hour > -1))
               {
                  if (show_numeric_total_only == NO)
                  {
                     display_data("Total", -1, ' ', -1, tmp_nfr, tmp_nbr);
                  }
                  else
                  {
                     (void)fprintf(stdout, "%.0f %.0f\n", tmp_nfr, tmp_nbr);
                  }
               }
               else
               {
                  total_nfr += tmp_nfr; total_nbr += tmp_nbr;
               }
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "======================================================================\n");
               }
            } /* if (show_hour_summary > -1) */

            /*
             * Show data for one or all minutes for this hour.
             */
            if (show_min > -1)
            {
               int tmp;

               tmp_nfr = tmp_nbr = 0.0;
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "                      ===========================\n");
                  (void)fprintf(stdout, "====================> AFD INPUT STATISTICS MINUTE <===================\n");
                  (void)fprintf(stdout, "                      ===========================\n");
               }
               if (dir_counter < 0)
               {
                  for (i = 0; i < no_of_dirs; i++)
                  {
                     nfr = nbr = 0.0;
                     if (show_min == 0) /* Show all minutes of the hour */
                     {
                        nfr += (double)afd_istat[i].hour[0].nfr;
                        nbr +=         afd_istat[i].hour[0].nbr;
                        display_data(afd_istat[i].dir_alias, 0, ' ', 0,
                                     afd_istat[i].hour[0].nfr,
                                     afd_istat[i].hour[0].nbr);
                        for (j = 1; j < afd_istat[i].sec_counter; j++)
                        {
                           tmp = j * STAT_RESCAN_TIME;
                           if ((tmp % 60) == 0)
                           {
                              tmp = tmp / 60;
                           }
                           else
                           {
                              tmp = -1;
                           }
                           display_data(" ", tmp, ' ', j,
                                        afd_istat[i].hour[j].nfr,
                                        afd_istat[i].hour[j].nbr);
                           nfr += (double)afd_istat[i].hour[j].nfr;
                           nbr +=         afd_istat[i].hour[j].nbr;
                        }
                        tmp = afd_istat[0].sec_counter * STAT_RESCAN_TIME;
                        if ((tmp % 60) == 0)
                        {
                           tmp = tmp / 60;
                        }
                        else
                        {
                           tmp = -1;
                        }
                        display_data(" ", tmp, '*', afd_istat[i].sec_counter,
                                     0.0, 0.0);
                        for (j = (afd_istat[i].sec_counter + 1);
                             j < SECS_PER_HOUR; j++)
                        {
                           tmp = j * STAT_RESCAN_TIME;
                           if ((tmp % 60) == 0)
                           {
                              tmp = tmp / 60;
                           }
                           else
                           {
                              tmp = -1;
                           }
                           nfr += (double)afd_istat[i].hour[j].nfr;
                           nbr +=         afd_istat[i].hour[j].nbr;
                           display_data(" ", tmp, ' ', j,
                                        afd_istat[i].hour[j].nfr,
                                        afd_istat[i].hour[j].nbr);
                        }
                     }
                     else /* Show a specific minute */
                     {
                        int sec = (show_min * 60) / STAT_RESCAN_TIME;

                        if (afd_istat[i].sec_counter < sec)
                        {
                           j = SECS_PER_HOUR - (sec - afd_istat[i].sec_counter);
                        }
                        else
                        {
                           j = afd_istat[i].sec_counter - sec;
                        }
                        nfr += (double)afd_istat[i].hour[j].nfr;
                        nbr +=         afd_istat[i].hour[j].nbr;
                        display_data(afd_istat[i].dir_alias, -1, ' ', -1,
                                     afd_istat[i].hour[j].nfr,
                                     afd_istat[i].hour[j].nbr);
                     }
                     tmp_nfr += nfr; tmp_nbr += nbr;
                  }
               } /* if (host_counter < 0) */
               else /* Show some specific directory */
               {
                  for (i = 0; i < dir_counter; i++)
                  {
                     if ((position = locate_dir(afd_istat, arglist[i],
                                                no_of_dirs)) < 0)
                     {
                        (void)fprintf(stdout,
                                      "No directory %s found in statistic database.\n",
                                      arglist[i]);
                     }
                     else
                     {
                        nfr = nbr = 0.0;
                        if (show_min == 0) /* Show all minutes of the hour */
                        {
                           if (afd_istat[position].sec_counter == 0)
                           {
                              display_data(afd_istat[position].dir_alias,
                                           0, '*', 0, 0.0, 0.0);
                           }
                           else
                           {
                              display_data(afd_istat[position].dir_alias,
                                           0, ' ', 0,
                                           afd_istat[position].hour[0].nfr,
                                           afd_istat[position].hour[0].nbr);
                              for (j = 1; j < afd_istat[position].sec_counter; j++)
                              {
                                 tmp = j * STAT_RESCAN_TIME;
                                 if ((tmp % 60) == 0)
                                 {
                                    tmp = tmp / 60;
                                 }
                                 else
                                 {
                                    tmp = -1;
                                 }
                                 nfr += (double)afd_istat[position].hour[j].nfr;
                                 nbr +=         afd_istat[position].hour[j].nbr;
                                 display_data(" ", tmp, ' ', j,
                                              afd_istat[position].hour[j].nfr,
                                              afd_istat[position].hour[j].nbr);
                              }
                              tmp = afd_istat[position].sec_counter * STAT_RESCAN_TIME;
                              if ((tmp % 60) == 0)
                              {
                                 tmp = tmp / 60;
                              }
                              else
                              {
                                 tmp = -1;
                              }
                              display_data(" ", tmp, '*',
                                           afd_istat[position].sec_counter,
                                           0.0, 0.0);
                           }
                           for (j = (afd_istat[position].sec_counter + 1);
                                j < SECS_PER_HOUR; j++)
                           {
                              tmp = j * STAT_RESCAN_TIME;
                              if ((tmp % 60) == 0)
                              {
                                 tmp = tmp / 60;
                              }
                              else
                              {
                                 tmp = -1;
                              }
                              nfr += (double)afd_istat[position].hour[j].nfr;
                              nbr +=         afd_istat[position].hour[j].nbr;
                              display_data(" ", tmp, ' ', j,
                                           afd_istat[position].hour[j].nfr,
                                           afd_istat[position].hour[j].nbr);
                           }
                        }
                        else /* Show a specific interval */
                        {
                           if (show_min < 60)
                           {
                              int sec = (show_min * 60) / STAT_RESCAN_TIME;

                              if (afd_istat[position].sec_counter < sec)
                              {
                                 j = SECS_PER_HOUR -
                                     (sec - afd_istat[position].sec_counter);
                              }
                              else
                              {
                                 j = afd_istat[position].sec_counter - sec;
                              }
                              nfr += (double)afd_istat[position].hour[j].nfr;
                              nbr +=         afd_istat[position].hour[j].nbr;
                              display_data(afd_istat[position].dir_alias,
                                           -1, ' ', -1,
                                           afd_istat[position].hour[j].nfr,
                                           afd_istat[position].hour[j].nbr);
                           }
                           else
                           {
                              display_data(afd_istat[position].dir_alias,
                                           -1, ' ', -1, 0.0, 0.0);
                           }
                        }
                        tmp_nfr += nfr; tmp_nbr += nbr;
                     }
                  }
               }

               if ((show_year > -1) || (show_day > -1) || (show_hour > -1) ||
                   (show_day_summary > -1) || (show_hour_summary > -1))
               {
                  if (show_numeric_total_only == NO)
                  {
                     display_data("Total", -1, ' ', -1, tmp_nfr, tmp_nbr);
                  }
                  else
                  {
                     (void)fprintf(stdout, "%.0f %.0f\n", tmp_nfr, tmp_nbr);
                  }
               }
               else
               {
                  total_nfr += tmp_nfr; total_nbr += tmp_nbr;
               }
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "======================================================================\n");
               }
            } /* if (show_min > -1) */

            /*
             * Show summary on a per minute basis for the last hour.
             */
            if (show_min_summary > -1)
            {
               tmp_nfr = tmp_nbr = 0.0;
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "                  ===================================\n");
                  (void)fprintf(stdout, "================> AFD INPUT STATISTICS MINUTE SUMMARY <===============\n");
                  (void)fprintf(stdout, "                  ===================================\n");
               }
            }
            if (show_min_summary == 0)
            {
               int tmp;

               for (j = 0; j < afd_istat[0].sec_counter; j++)
               {
                  tmp = j * STAT_RESCAN_TIME;
                  if ((tmp % 60) == 0)
                  {
                     tmp = tmp / 60;
                  }
                  else
                  {
                     tmp = -1;
                  }
                  nfr = nbr = 0.0;
                  for (i = 0; i < no_of_dirs; i++)
                  {
                     nfr += (double)afd_istat[i].hour[j].nfr;
                     nbr +=         afd_istat[i].hour[j].nbr;
                  }
                  display_data(" ", tmp, ' ', j, nfr, nbr);
                  tmp_nfr += nfr; tmp_nbr += nbr;
               }
               tmp = afd_istat[0].sec_counter * STAT_RESCAN_TIME;
               if ((tmp % 60) == 0)
               {
                  tmp = tmp / 60;
               }
               else
               {
                  tmp = -1;
               }
               display_data(" ", tmp, '*', afd_istat[0].sec_counter, 0.0, 0.0);
               for (j = (afd_istat[0].sec_counter + 1); j < SECS_PER_HOUR; j++)
               {
                  tmp = j * STAT_RESCAN_TIME;
                  if ((tmp % 60) == 0)
                  {
                     tmp = tmp / 60;
                  }
                  else
                  {
                     tmp = -1;
                  }
                  nfr = nbr = 0.0;
                  for (i = 0; i < no_of_dirs; i++)
                  {
                     nfr += (double)afd_istat[i].hour[j].nfr;
                     nbr +=         afd_istat[i].hour[j].nbr;
                  }
                  display_data(" ", tmp, ' ', j, nfr, nbr);
                  tmp_nfr += nfr; tmp_nbr += nbr;
               }
            }
            else if (show_min_summary > 0)
                 {
                    int left,
                        sec_ints = (show_min_summary * 60) / STAT_RESCAN_TIME,
                        tmp;

                    left = afd_istat[0].sec_counter - sec_ints;
                    if (left < 0)
                    {
                       for (j = (SECS_PER_HOUR + left); j < SECS_PER_HOUR; j++)
                       {
                          tmp = j * STAT_RESCAN_TIME;
                          if ((tmp % 60) == 0)
                          {
                             tmp = tmp / 60;
                          }
                          else
                          {
                             tmp = -1;
                          }
                          nfr = nbr = 0.0;
                          for (i = 0; i < no_of_dirs; i++)
                          {
                             nfr += (double)afd_istat[i].hour[j].nfr;
                             nbr +=         afd_istat[i].hour[j].nbr;
                          }
                          display_data(" ", tmp, ' ', j, nfr, nbr);
                          tmp_nfr += nfr; tmp_nbr += nbr;
                       }
                       for (j = 0; j < (sec_ints + left); j++)
                       {
                          tmp = j * STAT_RESCAN_TIME;
                          if ((tmp % 60) == 0)
                          {
                             tmp = tmp / 60;
                          }
                          else
                          {
                             tmp = -1;
                          }
                          nfr = nbr = 0.0;
                          for (i = 0; i < no_of_dirs; i++)
                          {
                             nfr += (double)afd_istat[i].hour[j].nfr;
                             nbr +=         afd_istat[i].hour[j].nbr;
                          }
                          display_data(" ", tmp, ' ', j, nfr, nbr);
                          tmp_nfr += nfr; tmp_nbr += nbr;
                       }
                    }
                    else
                    {
                       for (j = left; j < afd_istat[0].sec_counter; j++)
                       {
                          tmp = j * STAT_RESCAN_TIME;
                          if ((tmp % 60) == 0)
                          {
                             tmp = tmp / 60;
                          }
                          else
                          {
                             tmp = -1;
                          }
                          nfr = nbr = 0.0;
                          for (i = 0; i < no_of_dirs; i++)
                          {
                             nfr += (double)afd_istat[i].hour[j].nfr;
                             nbr +=         afd_istat[i].hour[j].nbr;
                          }
                          display_data(" ", tmp, ' ', j, nfr, nbr);
                          tmp_nfr += nfr; tmp_nbr += nbr;
                       }
                    }
                 }

            if (show_min_summary > -1)
            {
               if ((show_year > -1) || (show_day > -1) ||
                   (show_day_summary > -1) || (show_hour > -1))
               {
                  if (show_numeric_total_only == NO)
                  {
                     display_data("Total", -1, ' ', -1, tmp_nfr, tmp_nbr);
                  }
                  else
                  {
                     (void)fprintf(stdout, "%.0f %.0f\n", tmp_nfr, tmp_nbr);
                  }
               }
               else
               {
                  total_nfr += tmp_nfr; total_nbr += tmp_nbr;
               }
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "======================================================================\n");
               }
            }

            if (show_numeric_total_only == NO)
            {
               display_data("Total", -1, ' ', -1, total_nfr, total_nbr);
            }
            else
            {
               (void)fprintf(stdout, "%.0f %.0f\n", total_nfr, total_nbr);
            }
         }

#ifdef HAVE_MMAP
         if (munmap(ptr, stat_buf.st_size) < 0)
         {
            (void)fprintf(stderr,
                          "ERROR   : Could not munmap() file %s : %s (%s %d)\n",
                          statistic_file, strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
#else
         free(ptr);
#endif /* HAVE_MMAP */
      }

      if (close(stat_fd) == -1)
      {
         (void)fprintf(stderr,
                       "WARNING : Could not close() file %s : %s (%s %d)\n",
                       statistic_file, strerror(errno), __FILE__, __LINE__);
      }
   }
   else
   {
      (void)fprintf(stderr, "ERROR   : No data in %s (%s %d)\n",
                    statistic_file, __FILE__, __LINE__);
      exit(INCORRECT);
   }

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++ display_data() +++++++++++++++++++++++++++*/
static void
display_data(char   *name,
             int    val1,
             char   current,
             int    val2,
             double nfr,
             double nbr)
{
   if (show_numeric_total_only == NO)
   {
      char str1[3],
           str2[5];

      if (val1 == -1)
      {
         str1[0] = ' ';
         str1[1] = '\0';
      }
      else
      {
         str1[2] = '\0';
         if (val1 < 10)
         {
            str1[0] = ' ';
            str1[1] = val1 + '0';
         }
         else
         {
            str1[0] = (val1 / 10) + '0';
            str1[1] = (val1 % 10) + '0';
         }
      }
      if (val2 == -1)
      {
         str2[0] = ' ';
         str2[1] = '\0';
      }
      else
      {
         str2[3] = ':';
         str2[4] = '\0';
         if (val2 < 10)
         {
            str2[0] = ' ';
            str2[1] = ' ';
            str2[2] = val2 + '0';
         }
         else if (val2 < 100)
              {
                 str2[0] = ' ';
                 str2[1] = (val2 / 10) + '0';
                 str2[2] = (val2 % 10) + '0';
              }
              else
              {
                 str2[0] = (val2 / 100) + '0';
                 str2[1] = ((val2 / 10) % 10) + '0';
                 str2[2] = (val2 % 10) + '0';
              }
      }
      if (nbr >= F_EXABYTE)
      {
         (void)fprintf(stdout, "%-*s %2s %c%4s%12.0f %8.3f EB\n",
                       MAX_DIR_ALIAS_LENGTH, name, str1, current, str2,
                       nfr, nbr / F_EXABYTE);
      }
      else if (nbr >= F_PETABYTE)
           {
              (void)fprintf(stdout, "%-*s %2s %c%4s%12.0f %8.3f PB\n",
                            MAX_DIR_ALIAS_LENGTH, name, str1, current, str2,
                            nfr, nbr / F_PETABYTE);
           }
      else if (nbr >= F_TERABYTE)
           {
              (void)fprintf(stdout, "%-*s %2s %c%4s%12.0f %8.3f TB\n",
                            MAX_DIR_ALIAS_LENGTH, name, str1, current, str2,
                            nfr, nbr / F_TERABYTE);
           }
      else if (nbr >= F_GIGABYTE)
           {
              (void)fprintf(stdout, "%-*s %2s %c%4s%12.0f %8.3f GB\n",
                            MAX_DIR_ALIAS_LENGTH, name, str1, current, str2,
                            nfr, nbr / F_GIGABYTE);
           }
      else if (nbr >= F_MEGABYTE)
           {
              (void)fprintf(stdout, "%-*s %2s %c%4s%12.0f %8.3f MB\n",
                            MAX_DIR_ALIAS_LENGTH, name, str1, current, str2,
                            nfr,  nbr / F_MEGABYTE);
           }
      else if (nbr >= F_KILOBYTE)
           {
              (void)fprintf(stdout, "%-*s %2s %c%4s%12.0f %8.3f KB\n",
                            MAX_DIR_ALIAS_LENGTH, name, str1, current, str2,
                            nfr,  nbr / F_KILOBYTE);
           }
           else
           {
              (void)fprintf(stdout, "%-*s %2s %c%4s%12.0f %8.0f B\n",
                            MAX_DIR_ALIAS_LENGTH, name, str1, current, str2,
                            nfr,  nbr);
           }
   }

   return;
}
