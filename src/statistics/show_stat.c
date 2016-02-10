/*
 *  show_stat.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2015 Deutscher Wetterdienst (DWD),
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
 **   show_stat - shows all output statistic information of the AFD
 **
 ** SYNOPSIS
 **   show_stat [options] [hostname_1 hostname_2 .... hostname_n]
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
 **   This program shows all output statistic information of the number
 **   of files transfered, the number of bytes transfered, the number
 **   of connections and the number of errors that occured for each
 **   host and a total for all hosts, depending on the options
 **   that where used when calling 'show_stat'.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   18.09.1996 H.Kiehl Created
 **   19.04.1998 H.Kiehl Added -D option.
 **   26.05.1998 H.Kiehl Added -H option.
 **   09.08.2002 H.Kiehl Added -M option.
 **   16.02.2003 H.Kiehl Added -m and -mr options.
 **   19.02.2003 H.Kiehl Added -t[u] option.
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
            **arglist; /* Holds list of hostnames from command line when given. */
const char  *sys_log_name = SYSTEM_LOG_FIFO;

/* Local global variables. */
static int  show_numeric_total_only = NO;

/* Function prototypes. */
static void display_data(double, double, double, double);


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
                host_counter = -1,
                year;
   time_t       now;
   char         work_dir[MAX_PATH_LENGTH],
                statistic_file_name[MAX_FILENAME_LENGTH],
                statistic_file[MAX_PATH_LENGTH];
   struct tm    *p_ts;
   struct stat  stat_buf;

   CHECK_FOR_VERSION(argc, argv);

   /* Evaluate arguments */
   (void)strcpy(statistic_file_name, STATISTIC_FILE);
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   eval_input_ss(argc, argv, statistic_file_name, &show_day,
                 &show_day_summary, &show_hour, &show_hour_summary,
                 &show_min_range, &show_min, &show_min_summary, &show_year,
                 &host_counter, &show_time_stamp, &show_numeric_total_only, NO);

   /* Initialize variables */
   p_work_dir = work_dir;
   now = time(NULL);
   p_ts = localtime(&now);
   current_year = p_ts->tm_year + 1900;
   if (my_strcmp(statistic_file_name, STATISTIC_FILE) == 0)
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
      int          no_of_hosts,
                   position,
                   stat_fd;
      double       nfs = 0.0, nbs = 0.0, nc = 0.0, ne = 0.0,
                   tmp_nfs = 0.0, tmp_nbs = 0.0, tmp_nc = 0.0, tmp_ne = 0.0,
                   total_nfs = 0.0, total_nbs = 0.0, total_nc = 0.0,
                   total_ne = 0.0;

      /* Open file */
      if ((stat_fd = open(statistic_file, O_RDONLY)) < 0)
      {
         (void)fprintf(stderr, "ERROR   : Failed to open() %s : %s (%s %d)\n",
                       statistic_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }

      if (show_old_year == YES)
      {
         char                 *ptr;
         struct afd_year_stat *afd_stat;
         
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
         afd_stat = (struct afd_year_stat *)(ptr + AFD_WORD_OFFSET);

         no_of_hosts = (stat_buf.st_size - AFD_WORD_OFFSET) /
                       sizeof(struct afd_year_stat);
         if (show_year != -1)
         {
            /*
             * Show total for all host.
             */
            tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;

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
               (void)fprintf(stdout, "                     =============================\n");
               (void)fprintf(stdout, "====================> AFD STATISTICS SUMMARY %d <===================\n", year);
               (void)fprintf(stdout, "                     =============================\n");
            }

            if (host_counter > 0)
            {
               for (i = 0; i < host_counter; i++)
               {
                  if ((position = locate_host_year(afd_stat, arglist[i],
                                                   no_of_hosts)) < 0)
                  {
                     (void)fprintf(stdout,
                                   "No host %s found in statistic database.\n",
                                   arglist[i]);
                  }
                  else
                  {
                     nfs = nbs = nc = ne = 0.0;
                     if (show_numeric_total_only == NO)
                     {
                        (void)fprintf(stdout, "%-*s", MAX_HOSTNAME_LENGTH + 4,
                                      afd_stat[position].hostname);
                     }
                     for (j = 0; j < DAYS_PER_YEAR; j++)
                     {
                        nfs += (double)afd_stat[position].year[j].nfs;
                        nbs +=         afd_stat[position].year[j].nbs;
                        nc  += (double)afd_stat[position].year[j].nc;
                        ne  += (double)afd_stat[position].year[j].ne;
                     }
                     if (show_numeric_total_only == NO)
                     {
                        display_data(nfs, nbs, nc, ne);
                     }
                     tmp_nfs += nfs; tmp_nbs += nbs;
                     tmp_nc += nc; tmp_ne += ne;
                  }
               }
            }
            else
            {
               if (show_day_summary == 0)
               {
                  for (j = 0; j < DAYS_PER_YEAR; j++)
                  {
                     if (show_numeric_total_only == NO)
                     {
                        (void)fprintf(stdout, "%*d:", MAX_HOSTNAME_LENGTH + 4, j);
                     }
                     nfs = nbs = nc = ne = 0.0;
                     for (i = 0; i < no_of_hosts; i++)
                     {
                        nfs += (double)afd_stat[i].year[j].nfs;
                        nbs +=         afd_stat[i].year[j].nbs;
                        nc  += (double)afd_stat[i].year[j].nc;
                        ne  += (double)afd_stat[i].year[j].ne;
                     }
                     if (show_numeric_total_only == NO)
                     {
                        display_data(nfs, nbs, nc, ne);
                     }
                     tmp_nfs += nfs; tmp_nbs += nbs;
                     tmp_nc  += nc; tmp_ne  += ne;
                  }
               }
               else
               {
                  /* Make a summary of everything. */
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs = nbs = nc = ne = 0.0;
                     if (show_numeric_total_only == NO)
                     {
                        (void)fprintf(stdout, "%-*s", MAX_HOSTNAME_LENGTH + 4,
                                      afd_stat[i].hostname);
                     }
                     for (j = 0; j < DAYS_PER_YEAR; j++)
                     {
                        nfs += (double)afd_stat[i].year[j].nfs;
                        nbs +=         afd_stat[i].year[j].nbs;
                        nc  += (double)afd_stat[i].year[j].nc;
                        ne  += (double)afd_stat[i].year[j].ne;
                     }
                     if (show_numeric_total_only == NO)
                     {
                        display_data(nfs, nbs, nc, ne);
                     }
                     tmp_nfs += nfs; tmp_nbs += nbs;
                     tmp_nc += nc; tmp_ne += ne;
                  }
               }
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "----------------------------------------------------------------------\n");
                  (void)fprintf(stdout, "Total       ");
                  display_data(tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                  (void)fprintf(stdout, "======================================================================\n");
               }
               else
               {
                  display_data(tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
               }
            }
         }
         else
         {
            /*
             * Show data for one or all days for this year.
             */
            if (show_day > -1)
            {
               tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "                        ====================\n");
                  (void)fprintf(stdout, "=======================> AFD STATISTICS DAY <==========================\n");
                  (void)fprintf(stdout, "                        ====================\n");
               }
               if (host_counter < 0)
               {
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs = nbs = nc = ne = 0.0;
                     if (show_numeric_total_only == NO)
                     {
                        (void)fprintf(stdout, "%-*s",
                                      MAX_HOSTNAME_LENGTH, afd_stat[i].hostname);
                     }
                     if (show_day == 0) /* Show all days */
                     {
                        for (j = 0; j < DAYS_PER_YEAR; j++)
                        {
                           if (show_numeric_total_only == NO)
                           {
                              if (j == 0)
                              {
                                 (void)fprintf(stdout, "%4d:", j);
                              }
                              else
                              {
                                 (void)fprintf(stdout, "%*d:",
                                               MAX_HOSTNAME_LENGTH + 4, j);
                              }
                           }
                           nfs += (double)afd_stat[i].year[j].nfs;
                           nbs +=         afd_stat[i].year[j].nbs;
                           nc  += (double)afd_stat[i].year[j].nc;
                           ne  += (double)afd_stat[i].year[j].ne;
                           if (show_numeric_total_only == NO)
                           {
                              display_data(afd_stat[i].year[j].nfs,
                                           afd_stat[i].year[j].nbs,
                                           afd_stat[i].year[j].nc,
                                           afd_stat[i].year[j].ne);
                           }
                        }
                     }
                     else /* Show a specific day */
                     {
                        if (show_numeric_total_only == NO)
                        {
                           (void)fprintf(stdout, "%*s",
                                         MAX_HOSTNAME_LENGTH - 3, " ");
                        }
                        nfs += (double)afd_stat[i].year[show_day].nfs;
                        nbs +=         afd_stat[i].year[show_day].nbs;
                        nc  += (double)afd_stat[i].year[show_day].nc;
                        ne  += (double)afd_stat[i].year[show_day].ne;
                        if (show_numeric_total_only == NO)
                        {
                           display_data(afd_stat[i].year[show_day].nfs,
                                        afd_stat[i].year[show_day].nbs,
                                        afd_stat[i].year[show_day].nc,
                                        afd_stat[i].year[show_day].ne);
                        }
                     }
                     tmp_nfs += nfs; tmp_nbs += nbs;
                     tmp_nc  += nc; tmp_ne  += ne;
                  }
               } /* if (host_counter < 0) */
               else /* Show some specific hosts */
               {
                  for (i = 0; i < host_counter; i++)
                  {
                     if ((position = locate_host_year(afd_stat, arglist[i],
                                                      no_of_hosts)) < 0)
                     {
                        (void)fprintf(stdout,
                                      "No host %s found in statistic database.\n",
                                      arglist[i]);
                     }
                     else
                     {
                        nfs = nbs = nc = ne = 0.0;
                        if (show_numeric_total_only == NO)
                        {
                           (void)fprintf(stdout, "%-*s", MAX_HOSTNAME_LENGTH,
                                         afd_stat[position].hostname);
                        }
                        if (show_day == 0) /* Show all days */
                        {
                           for (j = 0; j < DAYS_PER_YEAR; j++)
                           {
                              if (show_numeric_total_only == NO)
                              {
                                 if (j == 0)
                                 {
                                    (void)fprintf(stdout, "%4d:", j);
                                 }
                                 else
                                 {
                                    (void)fprintf(stdout, "%*d:",
                                                  MAX_HOSTNAME_LENGTH + 4, j);
                                 }
                              }
                              nfs += (double)afd_stat[position].year[j].nfs;
                              nbs +=         afd_stat[position].year[j].nbs;
                              nc  += (double)afd_stat[position].year[j].nc;
                              ne  += (double)afd_stat[position].year[j].ne;
                              if (show_numeric_total_only == NO)
                              {
                                 display_data(afd_stat[position].year[j].nfs,
                                              afd_stat[position].year[j].nbs,
                                              afd_stat[position].year[j].nc,
                                              afd_stat[position].year[j].ne);
                              }
                           }
                        }
                        else /* Show a specific interval */
                        {
                           if (show_numeric_total_only == NO)
                           {
                              (void)fprintf(stdout, "%*s",
                                            MAX_HOSTNAME_LENGTH - 3, " ");
                           }
                           nfs += (double)afd_stat[position].year[show_day].nfs;
                           nbs +=         afd_stat[position].year[show_day].nbs;
                           nc  += (double)afd_stat[position].year[show_day].nc;
                           ne  += (double)afd_stat[position].year[show_day].ne;
                           if (show_numeric_total_only == NO)
                           {
                              display_data(afd_stat[position].year[show_day].nfs,
                                           afd_stat[position].year[show_day].nbs,
                                           afd_stat[position].year[show_day].nc,
                                           afd_stat[position].year[show_day].ne);
                           }
                        }
                        tmp_nfs += nfs; tmp_nbs += nbs;
                        tmp_nc  += nc; tmp_ne  += ne;
                     }
                  }
               }

               if ((show_year > -1) || (show_day_summary > -1))
               {
                  if (show_numeric_total_only == NO)
                  {
                     (void)fprintf(stdout, "Total        ");
                  }
                  display_data(tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
               }
               else
               {
                  total_nfs += tmp_nfs; total_nbs += tmp_nbs;
                  total_nc  += tmp_nc; total_ne  += tmp_ne;
               }
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "=======================================================================\n");
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

               tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "                     ==========================\n");
                  (void)fprintf(stdout, "===================> AFD STATISTICS DAY SUMMARY <======================\n");
                  (void)fprintf(stdout, "                     ==========================\n");
               }
               for (j = 0; j < DAYS_PER_YEAR; j++)
               {
                  if (show_numeric_total_only == NO)
                  {
                     (void)fprintf(stdout, "%*d:", MAX_HOSTNAME_LENGTH + 4, j);
                  }
                  nfs = nbs = nc = ne = 0.0;
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs += (double)afd_stat[i].year[j].nfs;
                     nbs +=         afd_stat[i].year[j].nbs;
                     nc  += (double)afd_stat[i].year[j].nc;
                     ne  += (double)afd_stat[i].year[j].ne;
                  }
                  if (show_numeric_total_only == NO)
                  {
                     display_data(nfs, nbs, nc, ne);
                  }
                  tmp_nfs += nfs; tmp_nbs += nbs;
                  tmp_nc  += nc; tmp_ne  += ne;
               }

               if ((show_year > -1) || (show_day > -1))
               {
                  if (show_numeric_total_only == NO)
                  {
                     (void)fprintf(stdout, "Total        ");
                  }
                  display_data(tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
               }
               else
               {
                  total_nfs += tmp_nfs; total_nbs += tmp_nbs;
                  total_nc  += tmp_nc; total_ne  += tmp_ne;
               }
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "=======================================================================\n");
               }
            } /* if (show_day_summary > -1) */

            if (show_numeric_total_only == NO)
            {
               (void)fprintf(stdout, "Total        ");
            }
            display_data(total_nfs, total_nbs, total_nc, total_ne);
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
         char           *ptr;
         struct afdstat *afd_stat;

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
         afd_stat = (struct afdstat *)(ptr + AFD_WORD_OFFSET);
         no_of_hosts = (stat_buf.st_size - AFD_WORD_OFFSET) /
                       sizeof(struct afdstat);

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
               p_ts->tm_hour = afd_stat[0].hour_counter;
               p_ts->tm_min = (afd_stat[0].sec_counter * STAT_RESCAN_TIME) / 60;
               p_ts->tm_sec = (afd_stat[0].sec_counter * STAT_RESCAN_TIME) % 60;
               last_time = mktime(p_ts) + (86400 * afd_stat[0].day_counter);
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
            tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;
            if (show_numeric_total_only == NO)
            {
               (void)fprintf(stdout, "                  ==================================\n");
               (void)fprintf(stdout, "=================> AFD STATISTICS LAST %2d MINUTE(S) <=================\n", show_min_range);
               (void)fprintf(stdout, "                  ==================================\n");
            }
            if (host_counter < 0)
            {
               for (i = 0; i < no_of_hosts; i++)
               {
                  nfs = nbs = nc = ne = 0.0;
                  left = afd_stat[i].sec_counter - sec_ints;
                  if (left < 0)
                  {
                     for (j = (SECS_PER_HOUR + left); j < SECS_PER_HOUR; j++)
                     {
                        nfs += (double)afd_stat[i].hour[j].nfs;
                        nbs +=         afd_stat[i].hour[j].nbs;
                        nc  += (double)afd_stat[i].hour[j].nc;
                        ne  += (double)afd_stat[i].hour[j].ne;
                     }
                     for (j = 0; j < (sec_ints + left); j++)
                     {
                        nfs += (double)afd_stat[i].hour[j].nfs;
                        nbs +=         afd_stat[i].hour[j].nbs;
                        nc  += (double)afd_stat[i].hour[j].nc;
                        ne  += (double)afd_stat[i].hour[j].ne;
                     }
                  }
                  else
                  {
                     for (j = left; j < afd_stat[i].sec_counter; j++)
                     {
                        nfs += (double)afd_stat[i].hour[j].nfs;
                        nbs +=         afd_stat[i].hour[j].nbs;
                        nc  += (double)afd_stat[i].hour[j].nc;
                        ne  += (double)afd_stat[i].hour[j].ne;
                     }
                  }
                  if (show_numeric_total_only == NO)
                  {
                     (void)fprintf(stdout, "%-*s",
                                   MAX_HOSTNAME_LENGTH + 4,
                                   afd_stat[i].hostname);
                     display_data(nfs, nbs, nc, ne);
                  }
                  tmp_nfs += nfs; tmp_nbs += nbs;
                  tmp_nc  += nc; tmp_ne  += ne;
               }
            }
            else
            {
               for (i = 0; i < host_counter; i++)
               {
                  if ((position = locate_host(afd_stat, arglist[i],
                                              no_of_hosts)) < 0)
                  {
                     (void)fprintf(stdout,
                                   "No host %s found in statistic database.\n",
                                   arglist[i]);
                  }
                  else
                  {
                     nfs = nbs = nc = ne = 0.0;
                     left = afd_stat[position].sec_counter - sec_ints;
                     if (left < 0)
                     {
                        for (j = (SECS_PER_HOUR + left); j < SECS_PER_HOUR; j++)
                        {
                           nfs += (double)afd_stat[position].hour[j].nfs;
                           nbs +=         afd_stat[position].hour[j].nbs;
                           nc  += (double)afd_stat[position].hour[j].nc;
                           ne  += (double)afd_stat[position].hour[j].ne;
                        }
                        for (j = 0; j < (sec_ints + left); j++)
                        {
                           nfs += (double)afd_stat[position].hour[j].nfs;
                           nbs +=         afd_stat[position].hour[j].nbs;
                           nc  += (double)afd_stat[position].hour[j].nc;
                           ne  += (double)afd_stat[position].hour[j].ne;
                        }
                     }
                     else
                     {
                        for (j = left; j < afd_stat[position].sec_counter; j++)
                        {
                           nfs += (double)afd_stat[position].hour[j].nfs;
                           nbs +=         afd_stat[position].hour[j].nbs;
                           nc  += (double)afd_stat[position].hour[j].nc;
                           ne  += (double)afd_stat[position].hour[j].ne;
                        }
                     }
                     if (show_numeric_total_only == NO)
                     {
                        (void)fprintf(stdout, "%-*s", MAX_HOSTNAME_LENGTH + 4,
                                      afd_stat[position].hostname);
                        display_data(nfs, nbs, nc, ne);
                     }
                     tmp_nfs += nfs; tmp_nbs += nbs;
                     tmp_nc  += nc; tmp_ne  += ne;
                  }
               }
            }
            if (show_numeric_total_only == NO)
            {
               (void)fprintf(stdout, "----------------------------------------------------------------------\n");
               (void)fprintf(stdout, "Total       ");
               display_data(tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
               (void)fprintf(stdout, "======================================================================\n");
            }
            else
            {
               display_data(tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
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
             * Show total for all host.
             */
            tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;

            if (show_numeric_total_only == NO)
            {
               (void)fprintf(stdout, "                       ========================\n");
               (void)fprintf(stdout, "======================> AFD STATISTICS SUMMARY <======================\n");
               (void)fprintf(stdout, "                       ========================\n");
            }

            if (host_counter > 0)
            {
               for (i = 0; i < host_counter; i++)
               {
                  if ((position = locate_host(afd_stat, arglist[i],
                                              no_of_hosts)) < 0)
                  {
                     (void)fprintf(stdout,
                                   "No host %s found in statistic database.\n",
                                   arglist[i]);
                  }
                  else
                  {
                     nfs = nbs = nc = ne = 0.0;
                     if (show_numeric_total_only == NO)
                     {
                        (void)fprintf(stdout, "%-*s", MAX_HOSTNAME_LENGTH + 4,
                                      afd_stat[position].hostname);
                     }
                     for (j = 0; j < afd_stat[position].sec_counter; j++)
                     {
                        nfs += (double)afd_stat[position].hour[j].nfs;
                        nbs +=         afd_stat[position].hour[j].nbs;
                        nc  += (double)afd_stat[position].hour[j].nc;
                        ne  += (double)afd_stat[position].hour[j].ne;
                     }
                     for (j = 0; j < afd_stat[position].hour_counter; j++)
                     {
                        nfs += (double)afd_stat[position].day[j].nfs;
                        nbs +=         afd_stat[position].day[j].nbs;
                        nc  += (double)afd_stat[position].day[j].nc;
                        ne  += (double)afd_stat[position].day[j].ne;
                     }
                     for (j = 0; j < afd_stat[position].day_counter; j++)
                     {
                        nfs += (double)afd_stat[position].year[j].nfs;
                        nbs +=         afd_stat[position].year[j].nbs;
                        nc  += (double)afd_stat[position].year[j].nc;
                        ne  += (double)afd_stat[position].year[j].ne;
                     }
                     if (show_numeric_total_only == NO)
                     {
                        display_data(nfs, nbs, nc, ne);
                     }
                     tmp_nfs += nfs; tmp_nbs += nbs;
                     tmp_nc += nc; tmp_ne += ne;
                  }
               }
            }
            else
            {
               /* Make a summary of everything */
               for (i = 0; i < no_of_hosts; i++)
               {
                  nfs = nbs = nc = ne = 0.0;
                  if (show_numeric_total_only == NO)
                  {
                     (void)fprintf(stdout, "%-*s", MAX_HOSTNAME_LENGTH + 4,
                                   afd_stat[i].hostname);
                  }
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
                  if (show_numeric_total_only == NO)
                  {
                     display_data(nfs, nbs, nc, ne);
                  }
                  tmp_nfs += nfs; tmp_nbs += nbs;
                  tmp_nc += nc; tmp_ne += ne;
               }
            }

            if (show_numeric_total_only == NO)
            {
               (void)fprintf(stdout, "----------------------------------------------------------------------\n");
               (void)fprintf(stdout, "Total       ");
               display_data(tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
               (void)fprintf(stdout, "======================================================================\n");
            }
            else
            {
               display_data(tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
            }
         }
         else
         {
            /*
             * Show data for one or all days for this year.
             */
            if (show_day > -1)
            {
               tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "                        ====================\n");
                  (void)fprintf(stdout, "=======================> AFD STATISTICS DAY <==========================\n");
                  (void)fprintf(stdout, "                        ====================\n");
               }
               if (host_counter < 0)
               {
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs = nbs = nc = ne = 0.0;
                     if (show_numeric_total_only == NO)
                     {
                        (void)fprintf(stdout, "%-*s", MAX_HOSTNAME_LENGTH,
                                      afd_stat[i].hostname);
                     }
                     if (show_day == 0) /* Show all days */
                     {
                        for (j = 0; j < afd_stat[i].day_counter; j++)
                        {
                           if (show_numeric_total_only == NO)
                           {
                              if (j == 0)
                              {
                                 (void)fprintf(stdout, "%4d:", j);
                              }
                              else
                              {
                                 (void)fprintf(stdout, "%*d:",
                                               MAX_HOSTNAME_LENGTH + 4, j);
                              }
                           }
                           nfs += (double)afd_stat[i].year[j].nfs;
                           nbs +=         afd_stat[i].year[j].nbs;
                           nc  += (double)afd_stat[i].year[j].nc;
                           ne  += (double)afd_stat[i].year[j].ne;
                           if (show_numeric_total_only == NO)
                           {
                              display_data(afd_stat[i].year[j].nfs,
                                           afd_stat[i].year[j].nbs,
                                           afd_stat[i].year[j].nc,
                                           afd_stat[i].year[j].ne);
                           }
                        }
                        if ((afd_stat[i].day_counter == 0) &&
                            (show_numeric_total_only == NO))
                        {
                           (void)fprintf(stdout, "%4d:", 0);
                           display_data(0, 0.0, 0, 0);
                        }
                     }
                     else /* Show a specific day */
                     {
                        if (show_numeric_total_only == NO)
                        {
                           (void)fprintf(stdout, "%*s", MAX_HOSTNAME_LENGTH - 3,
                                         " ");
                        }
                        if (show_day < DAYS_PER_YEAR)
                        {
                           if (afd_stat[i].day_counter < show_day)
                           {
                              j = DAYS_PER_YEAR -
                                  (show_day - afd_stat[i].day_counter);
                           }
                           else
                           {
                              j = afd_stat[i].day_counter - show_day;
                           }
                           nfs += (double)afd_stat[i].year[j].nfs;
                           nbs +=         afd_stat[i].year[j].nbs;
                           nc  += (double)afd_stat[i].year[j].nc;
                           ne  += (double)afd_stat[i].year[j].ne;
                           if (show_numeric_total_only == NO)
                           {
                              display_data(afd_stat[i].year[j].nfs,
                                           afd_stat[i].year[j].nbs,
                                           afd_stat[i].year[j].nc,
                                           afd_stat[i].year[j].ne);
                           }
                        }
                        else
                        {
                           if (show_numeric_total_only == NO)
                           {
                              display_data(0, 0.0, 0, 0);
                           }
                        }
                     }
                     tmp_nfs += nfs; tmp_nbs += nbs;
                     tmp_nc  += nc; tmp_ne  += ne;
                  }
               } /* if (host_counter < 0) */
               else /* Show some specific hosts */
               {
                  for (i = 0; i < host_counter; i++)
                  {
                     if ((position = locate_host(afd_stat, arglist[i],
                                                 no_of_hosts)) < 0)
                     {
                        (void)fprintf(stdout,
                                      "No host %s found in statistic database.\n",
                                      arglist[i]);
                     }
                     else
                     {
                        nfs = nbs = nc = ne = 0.0;
                        if (show_numeric_total_only == NO)
                        {
                           (void)fprintf(stdout, "%-*s", MAX_HOSTNAME_LENGTH,
                                         afd_stat[position].hostname);
                        }
                        if (show_day == 0) /* Show all days */
                        {
                           for (j = 0; j < afd_stat[position].day_counter; j++)
                           {
                              if (show_numeric_total_only == NO)
                              {
                                 if (j == 0)
                                 {
                                    (void)fprintf(stdout, "%4d:", j);
                                 }
                                 else
                                 {
                                    (void)fprintf(stdout, "%*d:",
                                                  MAX_HOSTNAME_LENGTH + 4, j);
                                 }
                              }
                              nfs += (double)afd_stat[position].year[j].nfs;
                              nbs +=         afd_stat[position].year[j].nbs;
                              nc  += (double)afd_stat[position].year[j].nc;
                              ne  += (double)afd_stat[position].year[j].ne;
                              if (show_numeric_total_only == NO)
                              {
                                 display_data(afd_stat[position].year[j].nfs,
                                              afd_stat[position].year[j].nbs,
                                              afd_stat[position].year[j].nc,
                                              afd_stat[position].year[j].ne);
                              }
                           }
                        }
                        else /* Show a specific interval */
                        {
                           if (show_numeric_total_only == NO)
                           {
                              (void)fprintf(stdout, "%*s", MAX_HOSTNAME_LENGTH - 3,
                                            " ");
                           }
                           if (show_day < DAYS_PER_YEAR)
                           {
                              if (afd_stat[position].day_counter < show_day)
                              {
                                 j = DAYS_PER_YEAR -
                                     (show_day - afd_stat[position].day_counter);
                              }
                              else
                              {
                                 j = afd_stat[position].day_counter - show_day;
                              }
                              nfs += (double)afd_stat[position].year[j].nfs;
                              nbs +=         afd_stat[position].year[j].nbs;
                              nc  += (double)afd_stat[position].year[j].nc;
                              ne  += (double)afd_stat[position].year[j].ne;
                              if (show_numeric_total_only == NO)
                              {
                                 display_data(afd_stat[position].year[j].nfs,
                                              afd_stat[position].year[j].nbs,
                                              afd_stat[position].year[j].nc,
                                              afd_stat[position].year[j].ne);
                              }
                           }
                           else
                           {
                              if (show_numeric_total_only == NO)
                              {
                                 display_data(0, 0.0, 0, 0);
                              }
                           }
                        }
                        tmp_nfs += nfs; tmp_nbs += nbs;
                        tmp_nc  += nc; tmp_ne  += ne;
                     }
                  }
               }

               if ((show_year > -1) || (show_hour > -1) ||
                   (show_day_summary > -1) || (show_hour_summary > -1))
               {
                  if (show_numeric_total_only == NO)
                  {
                     (void)fprintf(stdout, "Total        ");
                  }
                  display_data(tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
               }
               else
               {
                  total_nfs += tmp_nfs; total_nbs += tmp_nbs;
                  total_nc  += tmp_nc; total_ne  += tmp_ne;
               }
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "=======================================================================\n");
               }
            } /* if (show_day > -1) */

            /*
             * Show total summary on a per day basis for this year.
             */
            if (show_day_summary > -1)
            {
               struct tm *p_ts;

               p_ts = localtime(&now);
               tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "                     ==========================\n");
                  (void)fprintf(stdout, "===================> AFD STATISTICS DAY SUMMARY <======================\n");
                  (void)fprintf(stdout, "                     ==========================\n");
               }
               for (j = 0; j < p_ts->tm_yday; j++)
               {
                  if (show_numeric_total_only == NO)
                  {
                     (void)fprintf(stdout, "%*d:", MAX_HOSTNAME_LENGTH + 4, j);
                  }
                  nfs = nbs = nc = ne = 0.0;
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs += (double)afd_stat[i].year[j].nfs;
                     nbs +=         afd_stat[i].year[j].nbs;
                     nc  += (double)afd_stat[i].year[j].nc;
                     ne  += (double)afd_stat[i].year[j].ne;
                  }
                  if (show_numeric_total_only == NO)
                  {
                     display_data(nfs, nbs, nc, ne);
                  }

                  tmp_nfs += nfs;
                  tmp_nbs += nbs;
                  tmp_nc  += nc;
                  tmp_ne  += ne;
               }

               if ((show_year > -1) || (show_day > -1) ||
                   (show_hour > -1) || (show_hour_summary > -1))
               {
                  if (show_numeric_total_only == NO)
                  {
                     (void)fprintf(stdout, "Total        ");
                  }
                  display_data(tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
               }
               else
               {
                  total_nfs += tmp_nfs; total_nbs += tmp_nbs;
                  total_nc  += tmp_nc; total_ne  += tmp_ne;
               }
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "=======================================================================\n");
               }
            } /* if (show_day_summary > -1) */

            /*
             * Show data for one or all hours for this day.
             */
            if (show_hour > -1)
            {
               double sec_nfs, sec_nbs, sec_nc, sec_ne;

               tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "                        =====================\n");
                  (void)fprintf(stdout, "=======================> AFD STATISTICS HOUR <=========================\n");
                  (void)fprintf(stdout, "                        =====================\n");
               }
               if (host_counter < 0)
               {
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs = nbs = nc = ne = 0.0;
                     if (show_numeric_total_only == NO)
                     {
                        (void)fprintf(stdout, "%-*s", MAX_HOSTNAME_LENGTH,
                                      afd_stat[i].hostname);
                     }
                     if (show_hour == 0) /* Show all hours of the day */
                     {
                        for (j = 0; j < afd_stat[i].hour_counter; j++)
                        {
                           if (show_numeric_total_only == NO)
                           {
                              if (j == 0)
                              {
                                 (void)fprintf(stdout, "%4d:", j);
                              }
                              else
                              {
                                 (void)fprintf(stdout, "%*d:",
                                               MAX_HOSTNAME_LENGTH + 4, j);
                              }
                           }
                           nfs += (double)afd_stat[i].day[j].nfs;
                           nbs +=         afd_stat[i].day[j].nbs;
                           nc  += (double)afd_stat[i].day[j].nc;
                           ne  += (double)afd_stat[i].day[j].ne;
                           if (show_numeric_total_only == NO)
                           {
                              display_data(afd_stat[i].day[j].nfs,
                                           afd_stat[i].day[j].nbs,
                                           afd_stat[i].day[j].nc,
                                           afd_stat[i].day[j].ne);
                           }
                        }
                        if (show_numeric_total_only == NO)
                        {
                           if (afd_stat[i].hour_counter == 0)
                           {
                              (void)fprintf(stdout, "* %2d:",
                                            afd_stat[i].hour_counter);
                           }
                           else
                           {
                              (void)fprintf(stdout, "%*s* %2d:",
                                            MAX_HOSTNAME_LENGTH, " ",
                                            afd_stat[i].hour_counter);
                           }
                        }
                        sec_nfs = sec_nbs = sec_nc = sec_ne = 0.0;
                        for (j = 0; j < afd_stat[i].sec_counter; j++)
                        {
                           sec_nfs += (double)afd_stat[i].hour[j].nfs;
                           sec_nbs +=         afd_stat[i].hour[j].nbs;
                           sec_nc  += (double)afd_stat[i].hour[j].nc;
                           sec_ne  += (double)afd_stat[i].hour[j].ne;
                        }
                        if (show_numeric_total_only == NO)
                        {
                           display_data(sec_nfs, sec_nbs, sec_nc, sec_ne);
                        }
                        nfs += sec_nfs; nbs += sec_nbs;
                        nc  += sec_nc; ne  += sec_ne;
                        for (j = (afd_stat[i].hour_counter + 1);
                             j < HOURS_PER_DAY; j++)
                        {
                           if (show_numeric_total_only == NO)
                           {
                              (void)fprintf(stdout, "%*d:",
                                            MAX_HOSTNAME_LENGTH + 4, j);
                           }
                           nfs += (double)afd_stat[i].day[j].nfs;
                           nbs +=         afd_stat[i].day[j].nbs;
                           nc  += (double)afd_stat[i].day[j].nc;
                           ne  += (double)afd_stat[i].day[j].ne;
                           if (show_numeric_total_only == NO)
                           {
                              display_data(afd_stat[i].day[j].nfs,
                                           afd_stat[i].day[j].nbs,
                                           afd_stat[i].day[j].nc,
                                           afd_stat[i].day[j].ne);
                           }
                        }
                     }
                     else /* Show a specific hour */
                     {
                        if (show_numeric_total_only == NO)
                        {
                           (void)fprintf(stdout, "%*s", MAX_HOSTNAME_LENGTH - 3,
                                         " ");
                        }
                        if (show_hour < HOURS_PER_DAY)
                        {
                           if (afd_stat[i].hour_counter < show_hour)
                           {
                              j = HOURS_PER_DAY -
                                  (show_hour - afd_stat[i].hour_counter);
                           }
                           else
                           {
                              j = afd_stat[i].hour_counter - show_hour;
                           }
                           nfs += (double)afd_stat[i].day[j].nfs;
                           nbs +=         afd_stat[i].day[j].nbs;
                           nc  += (double)afd_stat[i].day[j].nc;
                           ne  += (double)afd_stat[i].day[j].ne;
                           if (show_numeric_total_only == NO)
                           {
                              display_data(afd_stat[i].day[j].nfs,
                                           afd_stat[i].day[j].nbs,
                                           afd_stat[i].day[j].nc,
                                           afd_stat[i].day[j].ne);
                           }
                        }
                        else
                        {
                           if (show_numeric_total_only == NO)
                           {
                              display_data(0, 0.0, 0, 0);
                           }
                        }
                     }
                     tmp_nfs += nfs; tmp_nbs += nbs;
                     tmp_nc  += nc; tmp_ne  += ne;
                  }
               } /* if (host_counter < 0) */
               else /* Show some specific hosts */
               {
                  for (i = 0; i < host_counter; i++)
                  {
                     if ((position = locate_host(afd_stat, arglist[i],
                                                 no_of_hosts)) < 0)
                     {
                        (void)fprintf(stdout,
                                      "No host %s found in statistic database.\n",
                                      arglist[i]);
                     }
                     else
                     {
                        nfs = nbs = nc = ne = 0.0;
                        if (show_numeric_total_only == NO)
                        {
                           (void)fprintf(stdout, "%-*s", MAX_HOSTNAME_LENGTH,
                                         afd_stat[position].hostname);
                        }
                        if (show_hour == 0) /* Show all hours of the day */
                        {
                           for (j = 0; j < afd_stat[position].hour_counter; j++)
                           {
                              if (show_numeric_total_only == NO)
                              {
                                 if (j == 0)
                                 {
                                    (void)fprintf(stdout, "%4d:", j);
                                 }
                                 else
                                 {
                                    (void)fprintf(stdout, "%*d:",
                                                  MAX_HOSTNAME_LENGTH + 4, j);
                                 }
                              }
                              nfs += (double)afd_stat[position].day[j].nfs;
                              nbs +=         afd_stat[position].day[j].nbs;
                              nc  += (double)afd_stat[position].day[j].nc;
                              ne  += (double)afd_stat[position].day[j].ne;
                              if (show_numeric_total_only == NO)
                              {
                                 display_data(afd_stat[position].day[j].nfs,
                                              afd_stat[position].day[j].nbs,
                                              afd_stat[position].day[j].nc,
                                              afd_stat[position].day[j].ne);
                              }
                           }
                           if (show_numeric_total_only == NO)
                           {
                              if (afd_stat[position].hour_counter == 0)
                              {
                                 (void)fprintf(stdout, "* %2d:",
                                               afd_stat[position].hour_counter);
                              }
                              else
                              {
                                 (void)fprintf(stdout, "        * %2d:",
                                               afd_stat[position].hour_counter);
                              }
                           }
                           sec_nfs = sec_nbs = sec_nc = sec_ne = 0.0;
                           for (j = 0; j < afd_stat[position].sec_counter; j++)
                           {
                              sec_nfs += (double)afd_stat[position].hour[j].nfs;
                              sec_nbs +=         afd_stat[position].hour[j].nbs;
                              sec_nc  += (double)afd_stat[position].hour[j].nc;
                              sec_ne  += (double)afd_stat[position].hour[j].ne;
                           }
                           if (show_numeric_total_only == NO)
                           {
                              display_data(sec_nfs, sec_nbs, sec_nc, sec_ne);
                           }
                           nfs += sec_nfs; nbs += sec_nbs;
                           nc  += sec_nc; ne  += sec_ne;
                           for (j = (afd_stat[position].hour_counter + 1);
                                j < HOURS_PER_DAY; j++)
                           {
                              if (show_numeric_total_only == NO)
                              {
                                 (void)fprintf(stdout, "%*d:",
                                               MAX_HOSTNAME_LENGTH + 4, j);
                              }
                              nfs += (double)afd_stat[position].day[j].nfs;
                              nbs +=         afd_stat[position].day[j].nbs;
                              nc  += (double)afd_stat[position].day[j].nc;
                              ne  += (double)afd_stat[position].day[j].ne;
                              if (show_numeric_total_only == NO)
                              {
                                 display_data(afd_stat[position].day[j].nfs,
                                              afd_stat[position].day[j].nbs,
                                              afd_stat[position].day[j].nc,
                                              afd_stat[position].day[j].ne);
                              }
                           }
                        }
                        else /* Show a specific interval */
                        {
                           if (show_numeric_total_only == NO)
                           {
                              (void)fprintf(stdout, "%*s",
                                            MAX_HOSTNAME_LENGTH - 3, " ");
                           }
                           if (show_hour < HOURS_PER_DAY)
                           {
                              if (afd_stat[position].hour_counter < show_hour)
                              {
                                 j = HOURS_PER_DAY -
                                     (show_hour - afd_stat[position].hour_counter);
                              }
                              else
                              {
                                 j = afd_stat[position].hour_counter - show_hour;
                              }
                              nfs += (double)afd_stat[position].day[j].nfs;
                              nbs +=         afd_stat[position].day[j].nbs;
                              nc  += (double)afd_stat[position].day[j].nc;
                              ne  += (double)afd_stat[position].day[j].ne;
                              if (show_numeric_total_only == NO)
                              {
                                 display_data(afd_stat[position].day[j].nfs,
                                              afd_stat[position].day[j].nbs,
                                              afd_stat[position].day[j].nc,
                                              afd_stat[position].day[j].ne);
                              }
                           }
                           else
                           {
                              if (show_numeric_total_only == NO)
                              {
                                 display_data(0, 0.0, 0, 0);
                              }
                           }
                        }
                        tmp_nfs += nfs; tmp_nbs += nbs;
                        tmp_nc  += nc; tmp_ne  += ne;
                     }
                  }
               }

               if ((show_year > -1) || (show_day > -1) ||
                   (show_day_summary > -1) || (show_hour_summary > -1))
               {
                  if (show_numeric_total_only == NO)
                  {
                     (void)fprintf(stdout, "Total        ");
                  }
                  display_data(tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
               }
               else
               {
                  total_nfs += tmp_nfs; total_nbs += tmp_nbs;
                  total_nc  += tmp_nc; total_ne  += tmp_ne;
               }
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "=======================================================================\n");
               }
            } /* if (show_hour > -1) */

            /*
             * Show total summary on a per hour basis for the last 24 hours.
             */
            if (show_hour_summary > -1)
            {
               tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "                     ===========================\n");
                  (void)fprintf(stdout, "===================> AFD STATISTICS HOUR SUMMARY <=====================\n");
                  (void)fprintf(stdout, "                     ===========================\n");
               }
               for (j = 0; j < afd_stat[0].hour_counter; j++)
               {
                  if (show_numeric_total_only == NO)
                  {
                     (void)fprintf(stdout, "%*d:", MAX_HOSTNAME_LENGTH + 4, j);
                  }
                  nfs = nbs = nc = ne = 0.0;
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs += (double)afd_stat[i].day[j].nfs;
                     nbs +=         afd_stat[i].day[j].nbs;
                     nc  += (double)afd_stat[i].day[j].nc;
                     ne  += (double)afd_stat[i].day[j].ne;
                  }
                  if (show_numeric_total_only == NO)
                  {
                     display_data(nfs, nbs, nc, ne);
                  }
                  tmp_nfs += nfs; tmp_nbs += nbs;
                  tmp_nc  += nc; tmp_ne  += ne;
               }
               if (show_numeric_total_only == NO)
               {
                  if (afd_stat[0].hour_counter == 0)
                  {
                     (void)fprintf(stdout, "* %2d:", afd_stat[0].hour_counter);
                  }
                  else
                  {
                     (void)fprintf(stdout, "%*s* %2d:",
                                   MAX_HOSTNAME_LENGTH, " ",
                                   afd_stat[0].hour_counter);
                  }
               }
               nfs = nbs = nc = ne = 0.0;
               for (i = 0; i < no_of_hosts; i++)
               {
                  for (j = 0; j < afd_stat[i].sec_counter; j++)
                  {
                     nfs += (double)afd_stat[i].hour[j].nfs;
                     nbs +=         afd_stat[i].hour[j].nbs;
                     nc  += (double)afd_stat[i].hour[j].nc;
                     ne  += (double)afd_stat[i].hour[j].ne;
                  }
               }
               if (show_numeric_total_only == NO)
               {
                  display_data(nfs, nbs, nc, ne);
               }
               tmp_nfs += nfs; tmp_nbs += nbs;
               tmp_nc  += nc; tmp_ne  += ne;
               for (j = (afd_stat[0].hour_counter + 1); j < HOURS_PER_DAY; j++)
               {
                  if (show_numeric_total_only == NO)
                  {
                     (void)fprintf(stdout, "%*d:", MAX_HOSTNAME_LENGTH + 4, j);
                  }
                  nfs = nbs = nc = ne = 0.0;
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs += (double)afd_stat[i].day[j].nfs;
                     nbs +=         afd_stat[i].day[j].nbs;
                     nc  += (double)afd_stat[i].day[j].nc;
                     ne  += (double)afd_stat[i].day[j].ne;
                  }
                  if (show_numeric_total_only == NO)
                  {
                     display_data(nfs, nbs, nc, ne);
                  }
                  tmp_nfs += nfs; tmp_nbs += nbs;
                  tmp_nc  += nc; tmp_ne  += ne;
               }

               if ((show_year > -1) || (show_day > -1) ||
                   (show_day_summary > -1) || (show_hour > -1))
               {
                  if (show_numeric_total_only == NO)
                  {
                     (void)fprintf(stdout, "Total        ");
                  }
                  display_data(tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
               }
               else
               {
                  total_nfs += tmp_nfs; total_nbs += tmp_nbs;
                  total_nc  += tmp_nc; total_ne  += tmp_ne;
               }
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "=======================================================================\n");
               }
            } /* if (show_hour_summary > -1) */

            /*
             * Show data for one or all minutes for this hour.
             */
            if (show_min > -1)
            {
               int tmp;

               tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "                        =====================\n");
                  (void)fprintf(stdout, "======================> AFD STATISTICS MINUTE <=========================\n");
                  (void)fprintf(stdout, "                        =====================\n");
               }
               if (host_counter < 0)
               {
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs = nbs = nc = ne = 0.0;
                     if (show_numeric_total_only == NO)
                     {
                        (void)fprintf(stdout, "%-*s", MAX_HOSTNAME_LENGTH,
                                      afd_stat[i].hostname);
                     }
                     if (show_min == 0) /* Show all minutes of the hour */
                     {
                        for (j = 0; j < afd_stat[i].sec_counter; j++)
                        {
                           if (show_numeric_total_only == NO)
                           {
                              tmp = j * STAT_RESCAN_TIME;
                              if ((tmp % 60) == 0)
                              {
                                 (void)fprintf(stdout, "%*d %4d:",
                                               MAX_HOSTNAME_LENGTH - 1,
                                               tmp / 60, j);
                              }
                              else
                              {
                                 (void)fprintf(stdout, "%*d:",
                                               MAX_HOSTNAME_LENGTH + 4, j);
                              }
                           }
                           nfs += (double)afd_stat[i].hour[j].nfs;
                           nbs +=         afd_stat[i].hour[j].nbs;
                           nc  += (double)afd_stat[i].hour[j].nc;
                           ne  += (double)afd_stat[i].hour[j].ne;
                           if (show_numeric_total_only == NO)
                           {
                              display_data(afd_stat[i].hour[j].nfs,
                                           afd_stat[i].hour[j].nbs,
                                           afd_stat[i].hour[j].nc,
                                           afd_stat[i].hour[j].ne);
                           }
                        }
                        if (show_numeric_total_only == NO)
                        {
                           tmp = afd_stat[0].sec_counter * STAT_RESCAN_TIME;
                           if ((tmp % 60) == 0)
                           {
                              (void)fprintf(stdout, "%*d*%4d:",
                                            MAX_HOSTNAME_LENGTH - 1,
                                            tmp / 60, afd_stat[i].sec_counter);
                           }
                           else
                           {
                              (void)fprintf(stdout, "%*s*%3d:",
                                            MAX_HOSTNAME_LENGTH, " ",
                                            afd_stat[i].sec_counter);
                           }
                           display_data(0, 0.0, 0, 0);
                        }
                        for (j = (afd_stat[i].sec_counter + 1); j < SECS_PER_HOUR; j++)
                        {
                           if (show_numeric_total_only == NO)
                           {
                              tmp = j * STAT_RESCAN_TIME;
                              if ((tmp % 60) == 0)
                              {
                                 (void)fprintf(stdout, "%*d %4d:",
                                               MAX_HOSTNAME_LENGTH - 1,
                                               tmp / 60, j);
                              }
                              else
                              {
                                 (void)fprintf(stdout, "%*d:",
                                               MAX_HOSTNAME_LENGTH + 4, j);
                              }
                           }
                           nfs += (double)afd_stat[i].hour[j].nfs;
                           nbs +=         afd_stat[i].hour[j].nbs;
                           nc  += (double)afd_stat[i].hour[j].nc;
                           ne  += (double)afd_stat[i].hour[j].ne;
                           if (show_numeric_total_only == NO)
                           {
                              display_data(afd_stat[i].hour[j].nfs,
                                           afd_stat[i].hour[j].nbs,
                                           afd_stat[i].hour[j].nc,
                                           afd_stat[i].hour[j].ne);
                           }
                        }
                     }
                     else /* Show a specific minute */
                     {
                        int sec = (show_min * 60) / STAT_RESCAN_TIME;

                        if (afd_stat[i].sec_counter < sec)
                        {
                           j = SECS_PER_HOUR - (sec - afd_stat[i].sec_counter);
                        }
                        else
                        {
                           j = afd_stat[i].sec_counter - sec;
                        }
                        if (show_numeric_total_only == NO)
                        {
                           (void)fprintf(stdout, "%*s",
                                         MAX_HOSTNAME_LENGTH - 3, " ");
                        }
                        nfs += (double)afd_stat[i].hour[j].nfs;
                        nbs +=         afd_stat[i].hour[j].nbs;
                        nc  += (double)afd_stat[i].hour[j].nc;
                        ne  += (double)afd_stat[i].hour[j].ne;
                        if (show_numeric_total_only == NO)
                        {
                           display_data(afd_stat[i].hour[j].nfs,
                                        afd_stat[i].hour[j].nbs,
                                        afd_stat[i].hour[j].nc,
                                        afd_stat[i].hour[j].ne);
                        }
                     }
                     tmp_nfs += nfs; tmp_nbs += nbs;
                     tmp_nc  += nc; tmp_ne  += ne;
                  }
               } /* if (host_counter < 0) */
               else /* Show some specific hosts */
               {
                  for (i = 0; i < host_counter; i++)
                  {
                     if ((position = locate_host(afd_stat, arglist[i],
                                                 no_of_hosts)) < 0)
                     {
                        (void)fprintf(stdout,
                                      "No host %s found in statistic database.\n",
                                      arglist[i]);
                     }
                     else
                     {
                        nfs = nbs = nc = ne = 0.0;
                        if (show_numeric_total_only == NO)
                        {
                           (void)fprintf(stdout, "%-*s", MAX_HOSTNAME_LENGTH,
                                         afd_stat[position].hostname);
                        }
                        if (show_min == 0) /* Show all minutes of the hour */
                        {
                           for (j = 0; j < afd_stat[position].sec_counter; j++)
                           {
                              if (show_numeric_total_only == NO)
                              {
                                 tmp = j * STAT_RESCAN_TIME;
                                 if ((tmp % 60) == 0)
                                 {
                                    (void)fprintf(stdout, "%*d %4d:",
                                                  MAX_HOSTNAME_LENGTH - 1,
                                                  tmp / 60, j);
                                 }
                                 else
                                 {
                                    (void)fprintf(stdout, "%*d:",
                                                  MAX_HOSTNAME_LENGTH + 4, j);
                                 }
                              }
                              nfs += (double)afd_stat[position].hour[j].nfs;
                              nbs +=         afd_stat[position].hour[j].nbs;
                              nc  += (double)afd_stat[position].hour[j].nc;
                              ne  += (double)afd_stat[position].hour[j].ne;
                              if (show_numeric_total_only == NO)
                              {
                                 display_data(afd_stat[position].hour[j].nfs,
                                              afd_stat[position].hour[j].nbs,
                                              afd_stat[position].hour[j].nc,
                                              afd_stat[position].hour[j].ne);
                              }
                           }
                           if (show_numeric_total_only == NO)
                           {
                              tmp = afd_stat[position].sec_counter * STAT_RESCAN_TIME;
                              if ((tmp % 60) == 0)
                              {
                                 (void)fprintf(stdout, "%*d*%4d:",
                                               MAX_HOSTNAME_LENGTH - 1, tmp / 60,
                                               afd_stat[position].sec_counter);
                              }
                              else
                              {
                                 (void)fprintf(stdout, "%*s*%3d:",
                                               MAX_HOSTNAME_LENGTH, " ",
                                               afd_stat[position].sec_counter);
                              }
                              display_data(0, 0.0, 0, 0);
                           }
                           for (j = (afd_stat[position].sec_counter + 1);
                                j < SECS_PER_HOUR; j++)
                           {
                              if (show_numeric_total_only == NO)
                              {
                                 tmp = j * STAT_RESCAN_TIME;
                                 if ((tmp % 60) == 0)
                                 {
                                    (void)fprintf(stdout, "%*d %4d:",
                                                  MAX_HOSTNAME_LENGTH - 1,
                                                  tmp / 60, j);
                                 }
                                 else
                                 {
                                    (void)fprintf(stdout, "%*d:",
                                                  MAX_HOSTNAME_LENGTH + 4, j);
                                 }
                              }
                              nfs += (double)afd_stat[position].hour[j].nfs;
                              nbs +=         afd_stat[position].hour[j].nbs;
                              nc  += (double)afd_stat[position].hour[j].nc;
                              ne  += (double)afd_stat[position].hour[j].ne;
                              if (show_numeric_total_only == NO)
                              {
                                 display_data(afd_stat[position].hour[j].nfs,
                                              afd_stat[position].hour[j].nbs,
                                              afd_stat[position].hour[j].nc,
                                              afd_stat[position].hour[j].ne);
                              }
                           }
                        }
                        else /* Show a specific interval */
                        {
                           if (show_numeric_total_only == NO)
                           {
                              (void)fprintf(stdout, "%*s",
                                            MAX_HOSTNAME_LENGTH - 3, " ");
                           }
                           if (show_min < 60)
                           {
                              int sec = (show_min * 60) / STAT_RESCAN_TIME;

                              if (afd_stat[position].sec_counter < sec)
                              {
                                 j = SECS_PER_HOUR -
                                     (sec - afd_stat[position].sec_counter);
                              }
                              else
                              {
                                 j = afd_stat[position].sec_counter - sec;
                              }
                              nfs += (double)afd_stat[position].hour[j].nfs;
                              nbs +=         afd_stat[position].hour[j].nbs;
                              nc  += (double)afd_stat[position].hour[j].nc;
                              ne  += (double)afd_stat[position].hour[j].ne;
                              if (show_numeric_total_only == NO)
                              {
                                 display_data(afd_stat[position].hour[j].nfs,
                                              afd_stat[position].hour[j].nbs,
                                              afd_stat[position].hour[j].nc,
                                              afd_stat[position].hour[j].ne);
                              }
                           }
                           else
                           {
                              if (show_numeric_total_only == NO)
                              {
                                 display_data(0, 0.0, 0, 0);
                              }
                           }
                        }
                        tmp_nfs += nfs; tmp_nbs += nbs;
                        tmp_nc  += nc; tmp_ne  += ne;
                     }
                  }
               }

               if ((show_year > -1) || (show_day > -1) || (show_hour > -1) ||
                   (show_day_summary > -1) || (show_hour_summary > -1))
               {
                  if (show_numeric_total_only == NO)
                  {
                     (void)fprintf(stdout, "Total        ");
                  }
                  display_data(tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
               }
               else
               {
                  total_nfs += tmp_nfs; total_nbs += tmp_nbs;
                  total_nc  += tmp_nc; total_ne  += tmp_ne;
               }
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "=======================================================================\n");
               }
            } /* if (show_min > -1) */

            /*
             * Show summary on a per minute basis for the last hour.
             */
            if (show_min_summary > -1)
            {
               tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "                    =============================\n");
                  (void)fprintf(stdout, "==================> AFD STATISTICS MINUTE SUMMARY <====================\n");
                  (void)fprintf(stdout, "                    =============================\n");
               }
            }
            if (show_min_summary == 0)
            {
               int tmp;

               for (j = 0; j < afd_stat[0].sec_counter; j++)
               {
                  if (show_numeric_total_only == NO)
                  {
                     tmp = j * STAT_RESCAN_TIME;
                     if ((tmp % 60) == 0)
                     {
                        (void)fprintf(stdout, "%*d %3d:",
                                      MAX_HOSTNAME_LENGTH, tmp / 60, j);
                     }
                     else
                     {
                        (void)fprintf(stdout, "%*d:",
                                      MAX_HOSTNAME_LENGTH + 4, j);
                     }
                  }
                  nfs = nbs = nc = ne = 0.0;
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs += (double)afd_stat[i].hour[j].nfs;
                     nbs +=         afd_stat[i].hour[j].nbs;
                     nc  += (double)afd_stat[i].hour[j].nc;
                     ne  += (double)afd_stat[i].hour[j].ne;
                  }
                  if (show_numeric_total_only == NO)
                  {
                     display_data(nfs, nbs, nc, ne);
                  }

                  tmp_nfs += nfs;
                  tmp_nbs += nbs;
                  tmp_nc  += nc;
                  tmp_ne  += ne;
               }
               if (show_numeric_total_only == NO)
               {
                  tmp = afd_stat[0].sec_counter * STAT_RESCAN_TIME;
                  if ((tmp % 60) == 0)
                  {
                     (void)fprintf(stdout, "%*d*%3d:", MAX_HOSTNAME_LENGTH,
                                   tmp / 60, afd_stat[0].sec_counter);
                  }
                  else
                  {
                     (void)fprintf(stdout, "%*s*%3d:", MAX_HOSTNAME_LENGTH, " ",
                                   afd_stat[0].sec_counter);
                  }
                  display_data(0, 0.0, 0, 0);
               }
               for (j = (afd_stat[0].sec_counter + 1); j < SECS_PER_HOUR; j++)
               {
                  if (show_numeric_total_only == NO)
                  {
                     tmp = j * STAT_RESCAN_TIME;
                     if ((tmp % 60) == 0)
                     {
                        (void)fprintf(stdout, "%*d %3d:",
                                      MAX_HOSTNAME_LENGTH, tmp / 60, j);
                     }
                     else
                     {
                        (void)fprintf(stdout, "%*d:",
                                      MAX_HOSTNAME_LENGTH + 4, j);
                     }
                  }
                  nfs = nbs = nc = ne = 0.0;
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs += (double)afd_stat[i].hour[j].nfs;
                     nbs +=         afd_stat[i].hour[j].nbs;
                     nc  += (double)afd_stat[i].hour[j].nc;
                     ne  += (double)afd_stat[i].hour[j].ne;
                  }
                  if (show_numeric_total_only == NO)
                  {
                     display_data(nfs, nbs, nc, ne);
                  }
                  tmp_nfs += nfs; tmp_nbs += nbs;
                  tmp_nc  += nc; tmp_ne  += ne;
               }
            }
            else if (show_min_summary > 0)
                 {
                    int left,
                        sec_ints = (show_min_summary * 60) / STAT_RESCAN_TIME,
                        tmp;

                    left = afd_stat[0].sec_counter - sec_ints;
                    if (left < 0)
                    {
                       for (j = (SECS_PER_HOUR + left); j < SECS_PER_HOUR; j++)
                       {
                          if (show_numeric_total_only == NO)
                          {
                             tmp = j * STAT_RESCAN_TIME;
                             if ((tmp % 60) == 0)
                             {
                                (void)fprintf(stdout, "%*d %3d:",
                                              MAX_HOSTNAME_LENGTH, tmp / 60, j);
                             }
                             else
                             {
                                (void)fprintf(stdout, "%*d:",
                                              MAX_HOSTNAME_LENGTH + 4, j);
                             }
                          }
                          nfs = nbs = nc = ne = 0.0;
                          for (i = 0; i < no_of_hosts; i++)
                          {
                             nfs += (double)afd_stat[i].hour[j].nfs;
                             nbs +=         afd_stat[i].hour[j].nbs;
                             nc  += (double)afd_stat[i].hour[j].nc;
                             ne  += (double)afd_stat[i].hour[j].ne;
                          }
                          if (show_numeric_total_only == NO)
                          {
                             display_data(nfs, nbs, nc, ne);
                          }
                          tmp_nfs += nfs; tmp_nbs += nbs;
                          tmp_nc  += nc; tmp_ne  += ne;
                       }
                       for (j = 0; j < (sec_ints + left); j++)
                       {
                          if (show_numeric_total_only == NO)
                          {
                             tmp = j * STAT_RESCAN_TIME;
                             if ((tmp % 60) == 0)
                             {
                                (void)fprintf(stdout, "%*d %3d:",
                                              MAX_HOSTNAME_LENGTH, tmp / 60, j);
                             }
                             else
                             {
                                (void)fprintf(stdout, "%*d:",
                                              MAX_HOSTNAME_LENGTH + 4, j);
                             }
                          }
                          nfs = nbs = nc = ne = 0.0;
                          for (i = 0; i < no_of_hosts; i++)
                          {
                             nfs += (double)afd_stat[i].hour[j].nfs;
                             nbs +=         afd_stat[i].hour[j].nbs;
                             nc  += (double)afd_stat[i].hour[j].nc;
                             ne  += (double)afd_stat[i].hour[j].ne;
                          }
                          if (show_numeric_total_only == NO)
                          {
                             display_data(nfs, nbs, nc, ne);
                          }
                          tmp_nfs += nfs; tmp_nbs += nbs;
                          tmp_nc  += nc; tmp_ne  += ne;
                       }
                    }
                    else
                    {
                       for (j = left; j < afd_stat[0].sec_counter; j++)
                       {
                          if (show_numeric_total_only == NO)
                          {
                             tmp = j * STAT_RESCAN_TIME;
                             if ((tmp % 60) == 0)
                             {
                                (void)fprintf(stdout, "%*d %3d:",
                                              MAX_HOSTNAME_LENGTH, tmp / 60, j);
                             }
                             else
                             {
                                (void)fprintf(stdout, "%*d:",
                                              MAX_HOSTNAME_LENGTH + 4, j);
                             }
                          }
                          nfs = nbs = nc = ne = 0.0;
                          for (i = 0; i < no_of_hosts; i++)
                          {
                             nfs += (double)afd_stat[i].hour[j].nfs;
                             nbs +=         afd_stat[i].hour[j].nbs;
                             nc  += (double)afd_stat[i].hour[j].nc;
                             ne  += (double)afd_stat[i].hour[j].ne;
                          }
                          if (show_numeric_total_only == NO)
                          {
                             display_data(nfs, nbs, nc, ne);
                          }
                          tmp_nfs += nfs; tmp_nbs += nbs;
                          tmp_nc  += nc; tmp_ne  += ne;
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
                     (void)fprintf(stdout, "Total        ");
                  }
                  display_data(tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
               }
               else
               {
                  total_nfs += tmp_nfs; total_nbs += tmp_nbs;
                  total_nc  += tmp_nc; total_ne  += tmp_ne;
               }
               if (show_numeric_total_only == NO)
               {
                  (void)fprintf(stdout, "=======================================================================\n");
               }
            }

            if (show_numeric_total_only == NO)
            {
               (void)fprintf(stdout, "Total        ");
            }
            display_data(total_nfs, total_nbs, total_nc, total_ne);
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
display_data(double nfs, double nbs, double nc, double ne)
{
   if (show_numeric_total_only == NO)
   {
      (void)fprintf(stdout, "%14.0f   ", nfs);
      if (nbs >= F_EXABYTE)
      {
         (void)fprintf(stdout, "%12.3f EB", nbs / F_EXABYTE);
      }
      else if (nbs >= F_PETABYTE)
           {
              (void)fprintf(stdout, "%12.3f PB", nbs / F_PETABYTE);
           }
      else if (nbs >= F_TERABYTE)
           {
              (void)fprintf(stdout, "%12.3f TB", nbs / F_TERABYTE);
           }
      else if (nbs >= F_GIGABYTE)
           {
              (void)fprintf(stdout, "%12.3f GB", nbs / F_GIGABYTE);
           }
      else if (nbs >= F_MEGABYTE)
           {
              (void)fprintf(stdout, "%12.3f MB", nbs / F_MEGABYTE);
           }
      else if (nbs >= F_KILOBYTE)
           {
              (void)fprintf(stdout, "%12.3f KB", nbs / F_KILOBYTE);
           }
           else
           {
              (void)fprintf(stdout, "%12.0f B ", nbs);
           }
      (void)fprintf(stdout, "%14.0f", nc);
      (void)fprintf(stdout, "%12.0f\n", ne);
   }
   else
   {
      (void)fprintf(stdout, "%.0f %.0f %.0f %.0f\n", nfs, nbs, nc, ne);
   }

   return;
}
