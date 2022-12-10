/*
 *  show_stat.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2022 Deutscher Wetterdienst (DWD),
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
 **               -o <name>       Output file name.
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
 **               -C              Output in CSV format.
 **               -N              Show directory name not alias.
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
 **   14.05.2018 H.Kiehl Added -o, -C and -N options.
 **
 */
DESCR__E_M1

#include <stdio.h>                  /* fprintf(), stderr                 */
#include <string.h>                 /* strcpy(), strerror()              */
#include <time.h>                   /* time()                            */
#include <ctype.h>                  /* isdigit()                         */
#include <sys/time.h>               /* struct tm                         */
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
int                        fsa_id,
                           fsa_fd = -1,
                           max_alias_name_length = MAX_HOSTNAME_LENGTH,
                           no_of_hosts = 0,
                           sys_log_fd = STDERR_FILENO; /* Used by get_afd_path() */
#ifdef HAVE_MMAP
off_t                      fsa_size;
#endif
char                       *p_work_dir,
                           **arglist; /* Holds list of hostnames from command line when given. */
const char                 *sys_log_name = SYSTEM_LOG_FIFO;
struct filetransfer_status *fsa = NULL;

/* Local global variables. */
static int                 display_format = NORMAL_OUTPUT,
                           show_alias = YES;
static char                prev_name[MAX_PATH_LENGTH],
                           real_hostname_1[MAX_REAL_HOSTNAME_LENGTH + 1],
                           real_hostname_2[MAX_REAL_HOSTNAME_LENGTH + 1];
static FILE                *output_fp = NULL;
static struct afdstat      *afd_stat;

/* Function prototypes. */
static void                display_data(int, char *, int, char, int,
                                        double, double, double, double);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   register int i;
   int          current_year,
                options = 0,
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
   char         *double_single_line = NULL,
                output_file_name[MAX_PATH_LENGTH],
                *space_line = NULL,
                statistic_file_name[MAX_FILENAME_LENGTH],
                statistic_file[MAX_PATH_LENGTH + FIFO_DIR_LENGTH + MAX_FILENAME_LENGTH + 1 + MAX_INT_LENGTH],
                work_dir[MAX_PATH_LENGTH];
   struct tm    *p_ts;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   CHECK_FOR_VERSION(argc, argv);

   /* Evaluate arguments */
   (void)strcpy(statistic_file_name, STATISTIC_FILE);
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   eval_input_ss(argc, argv, statistic_file_name, output_file_name, &show_day,
                 &show_day_summary, &show_hour, &show_hour_summary,
                 &show_min_range, &show_min, &show_min_summary, &show_year,
                 &host_counter, &show_time_stamp, &display_format,
                 &show_alias, NO, &options);

   /* Initialize variables */
   prev_name[0] = '\0';
   real_hostname_1[0] = '\0';
   real_hostname_2[0] = '\0';
   p_work_dir = work_dir;
   if (((space_line = malloc(max_alias_name_length + 1)) == NULL) ||
       ((double_single_line = malloc(max_alias_name_length + 1)) == NULL))
   {
      (void)fprintf(stderr, "Failed malloc() %d bytes : %s (%s %d)\n",
                    max_alias_name_length + 1, strerror(errno),
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (output_file_name[0] == '\0')
   {
      output_fp = stdout;
   }
   else
   {
      if ((output_fp = fopen(output_file_name, "w")) == NULL)
      {
         (void)fprintf(stderr, "Failed to fopen() `%s' : %s (%s %d)\n",
                       output_file_name, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   now = time(NULL);
   if ((p_ts = localtime(&now)) == NULL)
   {
      (void)fprintf(stderr, "ERROR   : Failed to get localtime() : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
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

#ifdef HAVE_STATX
   if (statx(0, statistic_file, AT_STATX_SYNC_AS_STAT,
             STATX_SIZE, &stat_buf) != 0)
#else
   if (stat(statistic_file, &stat_buf) != 0)
#endif
   {
      (void)fprintf(stderr, "ERROR   : Failed to access %s : %s (%s %d)\n",
                    statistic_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef HAVE_STATX
   if (stat_buf.stx_size > 0)
#else
   if (stat_buf.st_size > 0)
#endif
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
         if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                         stat_buf.stx_size, PROT_READ,
# else
                         stat_buf.st_size, PROT_READ,
# endif
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
# ifdef HAVE_STATX
         if ((ptr = malloc(stat_buf.stx_size)) == NULL)
# else
         if ((ptr = malloc(stat_buf.st_size)) == NULL)
# endif
         {
            (void)fprintf(stderr,
                          "ERROR   : Failed to allocate memory : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            (void)close(stat_fd);
            exit(INCORRECT);
         }

# ifdef HAVE_STATX
         if (read(stat_fd, ptr, stat_buf.stx_size) != stat_buf.stx_size)
# else
         if (read(stat_fd, ptr, stat_buf.st_size) != stat_buf.st_size)
# endif
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

#ifdef HAVE_STATX
         no_of_hosts = (stat_buf.stx_size - AFD_WORD_OFFSET) /
                       sizeof(struct afd_year_stat);
#else
         no_of_hosts = (stat_buf.st_size - AFD_WORD_OFFSET) /
                       sizeof(struct afd_year_stat);
#endif
         if (show_year != -1)
         {
            /*
             * Show total for all host.
             */
            tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;

            if ((display_format == NORMAL_OUTPUT) && (show_time_stamp > 0))
            {
               time_t    first_time,
                         last_time;
               struct tm *p_ts;

               if ((p_ts = localtime(&now)) == NULL)
               {
                  (void)fprintf(stderr, "ERROR   : Failed to get localtime() : %s (%s %d)\n",
                                strerror(errno), __FILE__, __LINE__);
                  exit(INCORRECT);
               }
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
                  (void)fprintf(output_fp, "          [time span %s -> %s]\n",
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

            if (display_format == NORMAL_OUTPUT)
            {
               (void)memset(space_line, ' ', max_alias_name_length / 2);
               space_line[max_alias_name_length / 2] = '\0';
               (void)memset(double_single_line, '=', max_alias_name_length / 2);
               double_single_line[max_alias_name_length / 2] = '\0';
               (void)fprintf(output_fp, "%s                  =============================\n", space_line);
               (void)fprintf(output_fp, "%s=================> AFD STATISTICS SUMMARY %d <%s================\n", double_single_line, year, double_single_line);
               (void)fprintf(output_fp, "%s                  =============================\n", space_line);
            }
            else if (display_format == CSV_FORMAT)
                 {
                    if (show_alias == YES)
                    {
                       (void)fprintf(output_fp,
                                     "alias;val1;current;val2;files;size;connects;errors\n");
                    }
                    else
                    {
                       (void)fprintf(output_fp,
                                     "alias;name1;name2;val1;current;val2;files;size;connects;errors\n");
                    }
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
                     for (j = 0; j < DAYS_PER_YEAR; j++)
                     {
                        nfs += (double)afd_stat[position].year[j].nfs;
                        nbs +=         afd_stat[position].year[j].nbs;
                        nc  += (double)afd_stat[position].year[j].nc;
                        ne  += (double)afd_stat[position].year[j].ne;
                     }
                     display_data(position, afd_stat[position].hostname,
                                  -1, ' ', -1, nfs, nbs, nc, ne);
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
                     nfs = nbs = nc = ne = 0.0;
                     for (i = 0; i < no_of_hosts; i++)
                     {
                        nfs += (double)afd_stat[i].year[j].nfs;
                        nbs +=         afd_stat[i].year[j].nbs;
                        nc  += (double)afd_stat[i].year[j].nc;
                        ne  += (double)afd_stat[i].year[j].ne;
                     }
                     display_data(SHOW_SPACE, NULL, -1, ' ', j,
                                  nfs, nbs, nc, ne);
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
                     for (j = 0; j < DAYS_PER_YEAR; j++)
                     {
                        nfs += (double)afd_stat[i].year[j].nfs;
                        nbs +=         afd_stat[i].year[j].nbs;
                        nc  += (double)afd_stat[i].year[j].nc;
                        ne  += (double)afd_stat[i].year[j].ne;
                     }
                     display_data(i, afd_stat[i].hostname, -1, ' ',
                                  -1, nfs, nbs, nc, ne);
                     tmp_nfs += nfs; tmp_nbs += nbs;
                     tmp_nc += nc; tmp_ne += ne;
                  }
               }
               if (display_format == CSV_FORMAT)
               {
                  display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1,
                               tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
               }
               else if (display_format == NUMERIC_TOTAL_ONLY)
                    {
                       (void)fprintf(output_fp, "%.0f %.0f %.0f %.0f\n", tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                    }
                    else
                    {
                       (void)memset(double_single_line, '-', max_alias_name_length);
                       double_single_line[max_alias_name_length] = '\0';
                       (void)fprintf(output_fp, "%s----------------------------------------------------------------\n", double_single_line);
                       display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                       (void)memset(double_single_line, '=', max_alias_name_length);
                       (void)fprintf(output_fp, "%s================================================================\n", double_single_line);
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
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(space_line, ' ', max_alias_name_length / 2);
                  space_line[max_alias_name_length / 2] = '\0';
                  (void)memset(double_single_line, '=', max_alias_name_length / 2);
                  double_single_line[max_alias_name_length / 2] = '\0';
                  (void)fprintf(output_fp, "%s                     ====================\n", space_line);
                  (void)fprintf(output_fp, "%s====================> AFD STATISTICS DAY <%s=======================\n", double_single_line, double_single_line);
                  (void)fprintf(output_fp, "%s                     ====================\n", space_line);
               }
               else if (display_format == CSV_FORMAT)
                    {
                       if (show_alias == YES)
                       {
                          (void)fprintf(output_fp,
                                        "alias;val1;current;val2;files;size;connects;errors\n");
                       }
                       else
                       {
                          (void)fprintf(output_fp,
                                        "alias;name1;name2;val1;current;val2;files;size;connects;errors\n");
                       }
                    }
               if (host_counter < 0)
               {
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs = nbs = nc = ne = 0.0;
                     if (show_day == 0) /* Show all days */
                     {
                        display_data(i, afd_stat[i].hostname,
                                     -1, ' ', 0, afd_stat[i].year[0].nfs,
                                     afd_stat[i].year[0].nbs,
                                     afd_stat[i].year[0].nc,
                                     afd_stat[i].year[0].ne);
                        for (j = 0; j < DAYS_PER_YEAR; j++)
                        {
                           nfs += (double)afd_stat[i].year[j].nfs;
                           nbs +=         afd_stat[i].year[j].nbs;
                           nc  += (double)afd_stat[i].year[j].nc;
                           ne  += (double)afd_stat[i].year[j].ne;
                           display_data(SHOW_SPACE, NULL, -1, ' ', j,
                                        afd_stat[i].year[j].nfs,
                                        afd_stat[i].year[j].nbs,
                                        afd_stat[i].year[j].nc,
                                        afd_stat[i].year[j].ne);
                        }
                     }
                     else /* Show a specific day */
                     {
                        nfs += (double)afd_stat[i].year[show_day].nfs;
                        nbs +=         afd_stat[i].year[show_day].nbs;
                        nc  += (double)afd_stat[i].year[show_day].nc;
                        ne  += (double)afd_stat[i].year[show_day].ne;
                        display_data(i, afd_stat[i].hostname, -1, ' ', -1,
                                     afd_stat[i].year[show_day].nfs,
                                     afd_stat[i].year[show_day].nbs,
                                     afd_stat[i].year[show_day].nc,
                                     afd_stat[i].year[show_day].ne);
                     }
                     tmp_nfs += nfs; tmp_nbs += nbs;
                     tmp_nc += nc; tmp_ne += ne;
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
                        if (show_day == 0) /* Show all days */
                        {
                           display_data(position, afd_stat[position].hostname,
                                        -1, ' ', 0,
                                        afd_stat[position].year[0].nfs,
                                        afd_stat[position].year[0].nbs,
                                        afd_stat[position].year[0].nc,
                                        afd_stat[position].year[0].ne);
                           for (j = 0; j < DAYS_PER_YEAR; j++)
                           {
                              nfs += (double)afd_stat[position].year[j].nfs;
                              nbs +=         afd_stat[position].year[j].nbs;
                              nc  += (double)afd_stat[position].year[j].nc;
                              ne  += (double)afd_stat[position].year[j].ne;
                              display_data(SHOW_SPACE, NULL, -1, ' ', j,
                                           afd_stat[position].year[j].nfs,
                                           afd_stat[position].year[j].nbs,
                                           afd_stat[position].year[j].nc,
                                           afd_stat[position].year[j].ne);
                           }
                        }
                        else /* Show a specific interval */
                        {
                           nfs += (double)afd_stat[position].year[show_day].nfs;
                           nbs +=         afd_stat[position].year[show_day].nbs;
                           nc  += (double)afd_stat[position].year[show_day].nc;
                           ne  += (double)afd_stat[position].year[show_day].ne;
                           display_data(position, afd_stat[position].hostname,
                                        -1, ' ', -1,
                                        afd_stat[position].year[show_day].nfs,
                                        afd_stat[position].year[show_day].nbs,
                                        afd_stat[position].year[show_day].nc,
                                        afd_stat[position].year[show_day].ne);
                        }
                        tmp_nfs += nfs; tmp_nbs += nbs;
                        tmp_nc += nc; tmp_ne += ne;
                     }
                  }
               }

               if ((show_year > -1) || (show_day_summary > -1))
               {
                  if (display_format == CSV_FORMAT)
                  {
                     display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                  }
                  else if (display_format == NUMERIC_TOTAL_ONLY)
                       {
                          (void)fprintf(output_fp, "%.0f %.0f %.0f %.0f\n", tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                       }
                       else
                       {
                          display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                       }
               }
               else
               {
                  total_nfs += tmp_nfs; total_nbs += tmp_nbs;
                  total_nc  += tmp_nc; total_ne  += tmp_ne;
               }
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(double_single_line, '=', max_alias_name_length);
                  double_single_line[max_alias_name_length] = '\0';
                  (void)fprintf(output_fp, "%s=================================================================\n", double_single_line);
               }
            } /* if (show_day > -1) */

            /*
             * Show total summary on a per day basis for this year.
             */
            if (show_day_summary > -1)
            {
               if ((display_format == NORMAL_OUTPUT) &&  (show_time_stamp > 0))
               {
                  struct tm *p_ts;
                  time_t    first_time, last_time;

                  if ((p_ts = localtime(&now)) == NULL)
                  {
                     (void)fprintf(stderr, "ERROR   : Failed to get localtime() : %s (%s %d)\n",
                                   strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
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
                     (void)fprintf(output_fp, "        [time span %s -> %s]\n",
                                   first_time_str, last_time_str);
                  }
                  else
                  {
                     (void)fprintf(output_fp,
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
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(space_line, ' ', max_alias_name_length / 2);
                  space_line[max_alias_name_length / 2] = '\0';
                  (void)memset(double_single_line, '=', max_alias_name_length / 2);
                  double_single_line[max_alias_name_length / 2] = '\0';
                  (void)fprintf(output_fp, "%s                  ==========================\n", space_line);
                  (void)fprintf(output_fp, "%s================> AFD STATISTICS DAY SUMMARY <%s===================\n", double_single_line, double_single_line);
                  (void)fprintf(output_fp, "%s                  ==========================\n", space_line);
               }
               else if (display_format == CSV_FORMAT)
                    {
                       if (show_alias == YES)
                       {
                          (void)fprintf(output_fp,
                                        "alias;val1;current;val2;files;size;connects;errors\n");
                       }
                       else
                       {
                          (void)fprintf(output_fp,
                                        "alias;name1;name2;val1;current;val2;files;size;connects;errors\n");
                       }
                    }
               for (j = 0; j < DAYS_PER_YEAR; j++)
               {
                  nfs = nbs = nc = ne = 0.0;
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs += (double)afd_stat[i].year[j].nfs;
                     nbs +=         afd_stat[i].year[j].nbs;
                     nc  += (double)afd_stat[i].year[j].nc;
                     ne  += (double)afd_stat[i].year[j].ne;
                  }
                  display_data(SHOW_SPACE, NULL, -1, ' ', j, nfs, nbs, nc, ne);
                  tmp_nfs += nfs; tmp_nbs += nbs;
                  tmp_nc  += nc; tmp_ne  += ne;
               }

               if ((show_year > -1) || (show_day > -1))
               {
                  if (display_format == CSV_FORMAT)
                  {
                     display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                  }
                  else if (display_format == NUMERIC_TOTAL_ONLY)
                       {
                          (void)fprintf(output_fp, "%.0f %.0f %.0f %.0f\n", tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                       }
                       else
                       {
                          display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                       }
               }
               else
               {
                  total_nfs += tmp_nfs; total_nbs += tmp_nbs;
                  total_nc  += tmp_nc; total_ne  += tmp_ne;
               }
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(double_single_line, '=', max_alias_name_length);
                  double_single_line[max_alias_name_length] = '\0';
                  (void)fprintf(output_fp, "%s=================================================================\n", double_single_line);
               }
            } /* if (show_day_summary > -1) */

            if (display_format == CSV_FORMAT)
            {
               display_data(SHOW_BIG_TOTAL, NULL, -1, ' ', -1, total_nfs, total_nbs, total_nc, total_ne);
            }
            else if (display_format == NUMERIC_TOTAL_ONLY)
                 {
                    (void)fprintf(output_fp, "%.0f %.0f %.0f %.0f\n", total_nfs, total_nbs, total_nc, total_ne);
                 }
                 else
                 {
                    display_data(SHOW_BIG_TOTAL, NULL, -1, ' ', -1, total_nfs, total_nbs, total_nc, total_ne);
                 }
         }

#ifdef HAVE_MMAP
# ifdef HAVE_STATX
         if (munmap(ptr, stat_buf.stx_size) < 0)
# else
         if (munmap(ptr, stat_buf.st_size) < 0)
# endif
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
         char *ptr;

#ifdef HAVE_MMAP
         if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                         stat_buf.stx_size, PROT_READ,
# else
                         stat_buf.st_size, PROT_READ,
# endif
                         (MAP_FILE | MAP_SHARED), stat_fd, 0)) == (caddr_t) -1)
         {
            (void)fprintf(stderr,
                          "ERROR   : Could not mmap() file %s : %s (%s %d)\n",
                          statistic_file, strerror(errno), __FILE__, __LINE__);
            (void)close(stat_fd);
            exit(INCORRECT);
         }
#else
# ifdef HAVE_STATX
         if ((ptr = malloc(stat_buf.stx_size)) == NULL)
# else
         if ((ptr = malloc(stat_buf.st_size)) == NULL)
# endif
         {
            (void)fprintf(stderr,
                          "ERROR   : Failed to allocate memory : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            (void)close(stat_fd);
            exit(INCORRECT);
         }

# ifdef HAVE_STATX
         if (read(stat_fd, ptr, stat_buf.stx_size) != stat_buf.stx_size)
# else
         if (read(stat_fd, ptr, stat_buf.st_size) != stat_buf.st_size)
# endif
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
#ifdef HAVE_STATX
         no_of_hosts = (stat_buf.stx_size - AFD_WORD_OFFSET) /
                       sizeof(struct afdstat);
#else
         no_of_hosts = (stat_buf.st_size - AFD_WORD_OFFSET) /
                       sizeof(struct afdstat);
#endif

         if (show_min_range)
         {
            int left,
                sec_ints = (show_min_range * 60) / STAT_RESCAN_TIME;

            if ((display_format == NORMAL_OUTPUT) && (show_time_stamp > 0))
            {
               time_t    first_time,
                         last_time;
               struct tm *p_ts;

               if ((p_ts = localtime(&now)) == NULL)
               {
                  (void)fprintf(stderr, "ERROR   : Failed to get localtime() : %s (%s %d)\n",
                                strerror(errno), __FILE__, __LINE__);
                  exit(INCORRECT);
               }
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
                  (void)fprintf(output_fp, "        [time span %s -> %s]\n",
                                first_time_str, last_time_str);
               }
               else
               {
                  (void)fprintf(output_fp,
#if SIZEOF_TIME_T == 4
                                "                 [time span %ld -> %ld]\n",
#else
                                "                 [time span %lld -> %lld]\n",
#endif
                                (pri_time_t)first_time, (pri_time_t)last_time);
               }
            }
            tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;
            if (display_format == NORMAL_OUTPUT)
            {
               (void)memset(space_line, ' ', max_alias_name_length / 2);
               space_line[max_alias_name_length / 2] = '\0';
               (void)memset(double_single_line, '=', max_alias_name_length / 2);
               double_single_line[max_alias_name_length / 2] = '\0';
               (void)fprintf(output_fp, "%s               ==================================\n", space_line);
               (void)fprintf(output_fp, "%s==============> AFD STATISTICS LAST %2d MINUTE(S) <%s==============\n", double_single_line, show_min_range, double_single_line);
               (void)fprintf(output_fp, "%s               ==================================\n", space_line);
            }
            else if (display_format == CSV_FORMAT)
                 {
                    if (show_alias == YES)
                    {
                       (void)fprintf(output_fp,
                                     "alias;val1;current;val2;files;size;connects;errors\n");
                    }
                    else
                    {
                       (void)fprintf(output_fp,
                                     "alias;name1;name2;val1;current;val2;files;size;connects;errors\n");
                    }
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
                  display_data(i, afd_stat[i].hostname, -1, ' ', -1,
                               nfs, nbs, nc, ne);
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
                     display_data(position, afd_stat[position].hostname,
                                  -1, ' ', -1, nfs, nbs, nc, ne);
                     tmp_nfs += nfs; tmp_nbs += nbs;
                     tmp_nc  += nc; tmp_ne  += ne;
                  }
               }
            }
            if (display_format == CSV_FORMAT)
            {
               display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
            }
            else if (display_format == NUMERIC_TOTAL_ONLY)
                 {
                    (void)fprintf(output_fp, "%.0f %.0f %.0f %.0f\n", tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                 }
                 else
                 {
                    (void)memset(double_single_line, '-', max_alias_name_length);
                    double_single_line[max_alias_name_length] = '\0';
                    (void)fprintf(output_fp, "%s----------------------------------------------------------------\n", double_single_line);
                    display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                    (void)memset(double_single_line, '=', max_alias_name_length);
                    (void)fprintf(output_fp, "%s================================================================\n", double_single_line);
                 }

#ifdef HAVE_MMAP
# ifdef HAVE_STATX
            if (munmap(ptr, stat_buf.stx_size) < 0)
# else
            if (munmap(ptr, stat_buf.st_size) < 0)
# endif
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

            if (display_format == NORMAL_OUTPUT)
            {
               (void)memset(space_line, ' ', max_alias_name_length / 2);
               space_line[max_alias_name_length / 2] = '\0';
               (void)memset(double_single_line, '=', max_alias_name_length / 2);
               double_single_line[max_alias_name_length / 2] = '\0';
               (void)fprintf(output_fp, "%s                    ========================\n", space_line);
               (void)fprintf(output_fp, "%s===================> AFD STATISTICS SUMMARY <%s===================\n", double_single_line, double_single_line);
               (void)fprintf(output_fp, "%s                    ========================\n", space_line);
            }
            else if (display_format == CSV_FORMAT)
                 {
                    if (show_alias == YES)
                    {
                       (void)fprintf(output_fp,
                                     "alias;val1;current;val2;files;size;connects;errors\n");
                    }
                    else
                    {
                       (void)fprintf(output_fp,
                                     "alias;name1;name2;val1;current;val2;files;size;connects;errors\n");
                    }
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
                     display_data(position, afd_stat[position].hostname,
                                  -1, ' ', -1, nfs, nbs, nc, ne);
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
                  display_data(i, afd_stat[i].hostname, -1, ' ', -1,
                               nfs, nbs, nc, ne);
                  tmp_nfs += nfs; tmp_nbs += nbs;
                  tmp_nc += nc; tmp_ne += ne;
               }
            }

            if (display_format == CSV_FORMAT)
            {
               display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
            }
            else if (display_format == NUMERIC_TOTAL_ONLY)
                 {
                    (void)fprintf(output_fp, "%.0f %.0f %.0f %.0f\n", tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                 }
                 else
                 {
                    (void)memset(double_single_line, '-', max_alias_name_length);
                    double_single_line[max_alias_name_length] = '\0';
                    (void)fprintf(output_fp, "%s----------------------------------------------------------------\n", double_single_line);
                    display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                    (void)memset(double_single_line, '=', max_alias_name_length);
                    (void)fprintf(output_fp, "%s================================================================\n", double_single_line);
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
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(space_line, ' ', max_alias_name_length / 2);
                  space_line[max_alias_name_length / 2] = '\0';
                  (void)memset(double_single_line, '=', max_alias_name_length / 2);
                  double_single_line[max_alias_name_length / 2] = '\0';
                  (void)fprintf(output_fp, "%s                     ====================\n", space_line);
                  (void)fprintf(output_fp, "%s====================> AFD STATISTICS DAY <%s=======================\n", double_single_line, double_single_line);
                  (void)fprintf(output_fp, "%s                     ====================\n", space_line);
               }
               else if (display_format == CSV_FORMAT)
                    {
                       if (show_alias == YES)
                       {
                          (void)fprintf(output_fp,
                                        "alias;val1;current;val2;files;size;connects;errors\n");
                       }
                       else
                       {
                          (void)fprintf(output_fp,
                                        "alias;name1;name2;val1;current;val2;files;size;connects;errors\n");
                       }
                    }
               if (host_counter < 0)
               {
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs = nbs = nc = ne = 0.0;
                     if (show_day == 0) /* Show all days */
                     {
                        if (afd_stat[i].day_counter == 0)
                        {
                           display_data(i, afd_stat[i].hostname, -1, ' ',
                                        0, 0.0, 0.0, 0.0, 0.0);
                        }
                        else
                        {
                           display_data(i, afd_stat[i].hostname,
                                        -1, ' ', 0, afd_stat[i].year[0].nfs,
                                        afd_stat[i].year[0].nbs,
                                        afd_stat[i].year[0].nc,
                                        afd_stat[i].year[0].ne);
                           for (j = 1; j < afd_stat[i].day_counter; j++)
                           {
                              nfs += (double)afd_stat[i].year[j].nfs;
                              nbs +=         afd_stat[i].year[j].nbs;
                              nc  += (double)afd_stat[i].year[j].nc;
                              ne  += (double)afd_stat[i].year[j].ne;
                              display_data(SHOW_SPACE, NULL, -1, ' ', j,
                                           afd_stat[i].year[j].nfs,
                                           afd_stat[i].year[j].nbs,
                                           afd_stat[i].year[j].nc,
                                           afd_stat[i].year[j].ne);
                           }
                        }
                     }
                     else /* Show a specific day */
                     {
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
                           display_data(i, afd_stat[i].hostname, -1, ' ', -1,
                                        afd_stat[i].year[j].nfs,
                                        afd_stat[i].year[j].nbs,
                                        afd_stat[i].year[j].nc,
                                        afd_stat[i].year[j].ne);
                        }
                        else
                        {
                           display_data(i, afd_stat[i].hostname, -1, ' ', -1, 0.0, 0.0, 0.0, 0.0);
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
                        if (show_day == 0) /* Show all days */
                        {
                           display_data(position, afd_stat[position].hostname,
                                        -1, ' ', 0,
                                        afd_stat[position].year[0].nfs,
                                        afd_stat[position].year[0].nbs,
                                        afd_stat[position].year[0].nc,
                                        afd_stat[position].year[0].ne);
                           for (j = 1; j < afd_stat[position].day_counter; j++)
                           {
                              nfs += (double)afd_stat[position].year[j].nfs;
                              nbs +=         afd_stat[position].year[j].nbs;
                              nc  += (double)afd_stat[position].year[j].nc;
                              ne  += (double)afd_stat[position].year[j].ne;
                              display_data(SHOW_SPACE, NULL, -1, ' ', j,
                                           afd_stat[position].year[j].nfs,
                                           afd_stat[position].year[j].nbs,
                                           afd_stat[position].year[j].nc,
                                           afd_stat[position].year[j].ne);
                           }
                        }
                        else /* Show a specific interval */
                        {
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
                              display_data(position,
                                           afd_stat[position].hostname,
                                           -1, ' ', -1,
                                           afd_stat[position].year[j].nfs,
                                           afd_stat[position].year[j].nbs,
                                           afd_stat[position].year[j].nc,
                                           afd_stat[position].year[j].ne);
                           }
                           else
                           {
                              display_data(position, afd_stat[position].hostname, -1, ' ', -1, 0.0, 0.0, 0.0, 0.0);
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
                  if (display_format == CSV_FORMAT)
                  {
                     display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                  }
                  else if (display_format == NUMERIC_TOTAL_ONLY)
                       {
                          (void)fprintf(output_fp, "%.0f %.0f %.0f %.0f\n", tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                       }
                       else
                       {
                          display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                       }
               }
               else
               {
                  total_nfs += tmp_nfs; total_nbs += tmp_nbs;
                  total_nc  += tmp_nc; total_ne  += tmp_ne;
               }
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(double_single_line, '=', max_alias_name_length);
                  double_single_line[max_alias_name_length] = '\0';
                  (void)fprintf(output_fp, "%s=================================================================\n", double_single_line);
               }
            } /* if (show_day > -1) */

            /*
             * Show total summary on a per day basis for this year.
             */
            if (show_day_summary > -1)
            {
               struct tm *p_ts;

               if ((p_ts = localtime(&now)) == NULL)
               {
                  (void)fprintf(stderr, "ERROR   : Failed to get localtime() : %s (%s %d)\n",
                                strerror(errno), __FILE__, __LINE__);
                  exit(INCORRECT);
               }
               tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(space_line, ' ', max_alias_name_length / 2);
                  space_line[max_alias_name_length / 2] = '\0';
                  (void)memset(double_single_line, '=', max_alias_name_length / 2);
                  double_single_line[max_alias_name_length / 2] = '\0';
                  (void)fprintf(output_fp, "%s                  ==========================\n", space_line);
                  (void)fprintf(output_fp, "%s================> AFD STATISTICS DAY SUMMARY <%s===================\n", double_single_line, double_single_line);
                  (void)fprintf(output_fp, "%s                  ==========================\n", space_line);
               }
               else if (display_format == CSV_FORMAT)
                    {
                       if (show_alias == YES)
                       {
                          (void)fprintf(output_fp,
                                        "alias;val1;current;val2;files;size;connects;errors\n");
                       }
                       else
                       {
                          (void)fprintf(output_fp,
                                        "alias;name1;name2;val1;current;val2;files;size;connects;errors\n");
                       }
                    }
               for (j = 0; j < p_ts->tm_yday; j++)
               {
                  nfs = nbs = nc = ne = 0.0;
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs += (double)afd_stat[i].year[j].nfs;
                     nbs +=         afd_stat[i].year[j].nbs;
                     nc  += (double)afd_stat[i].year[j].nc;
                     ne  += (double)afd_stat[i].year[j].ne;
                  }
                  display_data(SHOW_SPACE, NULL, -1, ' ', j, nfs, nbs, nc, ne);
                  tmp_nfs += nfs; tmp_nbs += nbs;
                  tmp_nc += nc; tmp_ne += ne;
               }

               if ((show_year > -1) || (show_day > -1) ||
                   (show_hour > -1) || (show_hour_summary > -1))
               {
                  if (display_format == CSV_FORMAT)
                  {
                     display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                  }
                  else if (display_format == NUMERIC_TOTAL_ONLY)
                       {
                          (void)fprintf(output_fp, "%.0f %.0f %.0f %.0f\n", tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                       }
                       else
                       {
                          display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                       }
               }
               else
               {
                  total_nfs += tmp_nfs; total_nbs += tmp_nbs;
                  total_nc  += tmp_nc; total_ne  += tmp_ne;
               }
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(double_single_line, '=', max_alias_name_length);
                  double_single_line[max_alias_name_length] = '\0';
                  (void)fprintf(output_fp, "%s=================================================================\n", double_single_line);
               }
            } /* if (show_day_summary > -1) */

            /*
             * Show data for one or all hours for this day.
             */
            if (show_hour > -1)
            {
               double sec_nfs, sec_nbs, sec_nc, sec_ne;

               tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(space_line, ' ', max_alias_name_length / 2);
                  space_line[max_alias_name_length / 2] = '\0';
                  (void)memset(double_single_line, '=', max_alias_name_length / 2);
                  double_single_line[max_alias_name_length / 2] = '\0';
                  (void)fprintf(output_fp, "%s                     =====================\n", space_line);
                  (void)fprintf(output_fp, "%s====================> AFD STATISTICS HOUR <%s======================\n", double_single_line, double_single_line);
                  (void)fprintf(output_fp, "%s                     =====================\n", space_line);
               }
               else if (display_format == CSV_FORMAT)
                    {
                       if (show_alias == YES)
                       {
                          (void)fprintf(output_fp,
                                        "alias;val1;current;val2;files;size;connects;errors\n");
                       }
                       else
                       {
                          (void)fprintf(output_fp,
                                        "alias;name1;name2;val1;current;val2;files;size;connects;errors\n");
                       }
                    }
               if (host_counter < 0)
               {
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs = nbs = nc = ne = 0.0;
                     if (show_hour == 0) /* Show all hours of the day */
                     {
                        sec_nfs = sec_nbs = sec_nc = sec_ne = 0.0;
                        for (j = 0; j < afd_stat[i].sec_counter; j++)
                        {
                           sec_nfs += (double)afd_stat[i].hour[j].nfs;
                           sec_nbs +=         afd_stat[i].hour[j].nbs;
                           sec_nc  += (double)afd_stat[i].hour[j].nc;
                           sec_ne  += (double)afd_stat[i].hour[j].ne;
                        }
                        if (afd_stat[i].hour_counter == 0)
                        {
                           display_data(i, afd_stat[i].hostname, -1, '*', 0, sec_nfs, sec_nbs, sec_nc, sec_ne);
                        }
                        else
                        {
                           display_data(i, afd_stat[i].hostname, -1, ' ', 0, afd_stat[i].day[0].nfs, afd_stat[i].day[0].nbs, afd_stat[i].day[0].nc, afd_stat[i].day[0].ne);
                           for (j = 1; j < afd_stat[i].hour_counter; j++)
                           {
                              display_data(SHOW_SPACE, NULL, -1, ' ', j,
                                           afd_stat[i].day[j].nfs,
                                           afd_stat[i].day[j].nbs,
                                           afd_stat[i].day[j].nc,
                                           afd_stat[i].day[j].ne);
                              nfs += (double)afd_stat[i].day[j].nfs;
                              nbs +=         afd_stat[i].day[j].nbs;
                              nc  += (double)afd_stat[i].day[j].nc;
                              ne  += (double)afd_stat[i].day[j].ne;
                           }
                           display_data(SHOW_SPACE, NULL, -1, '*', j, sec_nfs, sec_nbs, sec_nc, sec_ne);
                        }
                        nfs += sec_nfs; nbs += sec_nbs;
                        nc  += sec_nc; ne  += sec_ne;
                        for (j = (afd_stat[i].hour_counter + 1);
                             j < HOURS_PER_DAY; j++)
                        {
                           nfs += (double)afd_stat[i].day[j].nfs;
                           nbs +=         afd_stat[i].day[j].nbs;
                           nc  += (double)afd_stat[i].day[j].nc;
                           ne  += (double)afd_stat[i].day[j].ne;
                           display_data(SHOW_SPACE, NULL, -1, ' ', j,
                                        afd_stat[i].day[j].nfs,
                                        afd_stat[i].day[j].nbs,
                                        afd_stat[i].day[j].nc,
                                        afd_stat[i].day[j].ne);
                        }
                     }
                     else /* Show a specific hour */
                     {
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
                           display_data(i, afd_stat[i].hostname, -1, ' ', -1,
                                        afd_stat[i].day[j].nfs,
                                        afd_stat[i].day[j].nbs,
                                        afd_stat[i].day[j].nc,
                                        afd_stat[i].day[j].ne);
                        }
                        else
                        {
                           display_data(i, afd_stat[i].hostname, -1, ' ', -1, 0.0, 0.0, 0.0, 0.0);
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
                        if (show_hour == 0) /* Show all hours of the day */
                        {
                           sec_nfs = sec_nbs = sec_nc = sec_ne = 0.0;
                           for (j = 0; j < afd_stat[position].sec_counter; j++)
                           {
                              sec_nfs += (double)afd_stat[position].hour[j].nfs;
                              sec_nbs +=         afd_stat[position].hour[j].nbs;
                              sec_nc  += (double)afd_stat[position].hour[j].nc;
                              sec_ne  += (double)afd_stat[position].hour[j].ne;
                           }
                           if (afd_stat[i].hour_counter == 0)
                           {
                              display_data(position,
                                           afd_stat[position].hostname,
                                           -1, '*', 0,
                                           sec_nfs, sec_nbs, sec_nc, sec_ne);
                           }
                           else
                           {
                              display_data(position,
                                           afd_stat[position].hostname,
                                           -1, ' ', 0,
                                           afd_stat[position].day[0].nfs,
                                           afd_stat[position].day[0].nbs,
                                           afd_stat[position].day[0].nc,
                                           afd_stat[position].day[0].ne);
                              for (j = 1; j < afd_stat[position].hour_counter; j++)
                              {
                                 nfs += (double)afd_stat[position].day[j].nfs;
                                 nbs +=         afd_stat[position].day[j].nbs;
                                 nc  += (double)afd_stat[position].day[j].nc;
                                 ne  += (double)afd_stat[position].day[j].ne;
                                 display_data(SHOW_SPACE, NULL, -1, ' ', j,
                                              afd_stat[position].day[j].nfs,
                                              afd_stat[position].day[j].nbs,
                                              afd_stat[position].day[j].nc,
                                              afd_stat[position].day[j].ne);
                              }
                              display_data(SHOW_SPACE, NULL, -1, '*', j, sec_nfs, sec_nbs, sec_nc, sec_ne);
                           }
                           nfs += sec_nfs; nbs += sec_nbs;
                           nc  += sec_nc; ne  += sec_ne;
                           for (j = (afd_stat[position].hour_counter + 1);
                                j < HOURS_PER_DAY; j++)
                           {
                              nfs += (double)afd_stat[position].day[j].nfs;
                              nbs +=         afd_stat[position].day[j].nbs;
                              nc  += (double)afd_stat[position].day[j].nc;
                              ne  += (double)afd_stat[position].day[j].ne;
                              display_data(SHOW_SPACE, NULL, -1, ' ', j,
                                           afd_stat[position].day[j].nfs,
                                           afd_stat[position].day[j].nbs,
                                           afd_stat[position].day[j].nc,
                                           afd_stat[position].day[j].ne);
                           }
                        }
                        else /* Show a specific interval */
                        {
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
                              display_data(position,
                                           afd_stat[position].hostname,
                                           -1, ' ', j,
                                           afd_stat[position].day[j].nfs,
                                           afd_stat[position].day[j].nbs,
                                           afd_stat[position].day[j].nc,
                                           afd_stat[position].day[j].ne);
                           }
                           else
                           {
                              display_data(position,
                                           afd_stat[position].hostname,
                                           -1, ' ', -1, 0.0, 0.0, 0.0, 0.0);
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
                  if (display_format == CSV_FORMAT)
                  {
                     display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                  }
                  else if (display_format == NUMERIC_TOTAL_ONLY)
                       {
                          (void)fprintf(output_fp, "%.0f %.0f %.0f %.0f\n", tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                       }
                       else
                       {
                          display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                       }
               }
               else
               {
                  total_nfs += tmp_nfs; total_nbs += tmp_nbs;
                  total_nc  += tmp_nc; total_ne  += tmp_ne;
               }
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(double_single_line, '=', max_alias_name_length);
                  double_single_line[max_alias_name_length] = '\0';
                  (void)fprintf(output_fp, "%s=================================================================\n", double_single_line);
               }
            } /* if (show_hour > -1) */

            /*
             * Show total summary on a per hour basis for the last 24 hours.
             */
            if (show_hour_summary > -1)
            {
               tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(space_line, ' ', max_alias_name_length / 2);
                  space_line[max_alias_name_length / 2] = '\0'; 
                  (void)memset(double_single_line, '=', max_alias_name_length / 2);
                  double_single_line[max_alias_name_length / 2] = '\0';
                  (void)fprintf(output_fp, "%s                  ===========================\n", space_line);
                  (void)fprintf(output_fp, "%s================> AFD STATISTICS HOUR SUMMARY <%s==================\n", double_single_line, double_single_line);
                  (void)fprintf(output_fp, "%s                  ===========================\n", space_line);
               }
               else if (display_format == CSV_FORMAT)
                    {
                       if (show_alias == YES)
                       {
                          (void)fprintf(output_fp,
                                        "alias;val1;current;val2;files;size;connects;errors\n");
                       }
                       else
                       {
                          (void)fprintf(output_fp,
                                        "alias;name1;name2;val1;current;val2;files;size;connects;errors\n");
                       }
                    }
               for (j = 0; j < afd_stat[0].hour_counter; j++)
               {
                  nfs = nbs = nc = ne = 0.0;
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs += (double)afd_stat[i].day[j].nfs;
                     nbs +=         afd_stat[i].day[j].nbs;
                     nc  += (double)afd_stat[i].day[j].nc;
                     ne  += (double)afd_stat[i].day[j].ne;
                  }
                  display_data(SHOW_SPACE, NULL, -1, ' ', j, nfs, nbs, nc, ne);
                  tmp_nfs += nfs; tmp_nbs += nbs;
                  tmp_nc  += nc; tmp_ne  += ne;
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
               display_data(SHOW_SPACE, NULL, -1, '*',
                            afd_stat[0].hour_counter, nfs, nbs, nc, ne);
               tmp_nfs += nfs; tmp_nbs += nbs;
               tmp_nc  += nc; tmp_ne  += ne;
               for (j = (afd_stat[0].hour_counter + 1); j < HOURS_PER_DAY; j++)
               {
                  nfs = nbs = nc = ne = 0.0;
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs += (double)afd_stat[i].day[j].nfs;
                     nbs +=         afd_stat[i].day[j].nbs;
                     nc  += (double)afd_stat[i].day[j].nc;
                     ne  += (double)afd_stat[i].day[j].ne;
                  }
                  display_data(SHOW_SPACE, NULL, -1, ' ', j, nfs, nbs, nc, ne);
                  tmp_nfs += nfs; tmp_nbs += nbs;
                  tmp_nc  += nc; tmp_ne  += ne;
               }

               if ((show_year > -1) || (show_day > -1) ||
                   (show_day_summary > -1) || (show_hour > -1))
               {
                  if (display_format == CSV_FORMAT)
                  {
                     display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ',
                                  -1, tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                  }
                  else if (display_format == NUMERIC_TOTAL_ONLY)
                       {
                          (void)fprintf(output_fp, "%.0f %.0f %.0f %.0f\n", tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                       }
                       else
                       {
                          display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ',
                                       -1, tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                       }
               }
               else
               {
                  total_nfs += tmp_nfs; total_nbs += tmp_nbs;
                  total_nc  += tmp_nc; total_ne  += tmp_ne;
               }
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(double_single_line, '=', max_alias_name_length);
                  double_single_line[max_alias_name_length] = '\0';
                  (void)fprintf(output_fp, "%s=================================================================\n", double_single_line);
               }
            } /* if (show_hour_summary > -1) */

            /*
             * Show data for one or all minutes for this hour.
             */
            if (show_min > -1)
            {
               int tmp;

               tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(space_line, ' ', max_alias_name_length / 2);
                  space_line[max_alias_name_length / 2] = '\0';
                  (void)memset(double_single_line, '=', max_alias_name_length / 2);
                  double_single_line[max_alias_name_length / 2] = '\0';
                  (void)fprintf(output_fp, "%s                     =====================\n", space_line);
                  (void)fprintf(output_fp, "%s===================> AFD STATISTICS MINUTE <%s======================\n", double_single_line, double_single_line);
                  (void)fprintf(output_fp, "%s                     =====================\n", space_line);
               }
               else if (display_format == CSV_FORMAT)
                    {
                       if (show_alias == YES)
                       {
                          (void)fprintf(output_fp,
                                        "alias;val1;current;val2;files;size;connects;errors\n");
                       }
                       else
                       {
                          (void)fprintf(output_fp,
                                        "alias;name1;name2;val1;current;val2;files;size;connects;errors\n");
                       }
                    }
               if (host_counter < 0)
               {
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs = nbs = nc = ne = 0.0;
                     if (show_min == 0) /* Show all minutes of the hour */
                     {
                        nfs += (double)afd_stat[i].hour[0].nfs;
                        nbs +=         afd_stat[i].hour[0].nbs;
                        nc  += (double)afd_stat[i].hour[0].nc;
                        ne  += (double)afd_stat[i].hour[0].ne;
                        display_data(i, afd_stat[i].hostname,
                                     0, ' ', 0, afd_stat[i].hour[0].nfs,
                                     afd_stat[i].hour[0].nbs,
                                     afd_stat[i].hour[0].nc,
                                     afd_stat[i].hour[0].ne);
                        for (j = 1; j < afd_stat[i].sec_counter; j++)
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
                           nfs += (double)afd_stat[i].hour[j].nfs;
                           nbs +=         afd_stat[i].hour[j].nbs;
                           nc  += (double)afd_stat[i].hour[j].nc;
                           ne  += (double)afd_stat[i].hour[j].ne;
                           display_data(SHOW_SPACE, NULL, tmp, ' ', j,
                                        afd_stat[i].hour[j].nfs,
                                        afd_stat[i].hour[j].nbs,
                                        afd_stat[i].hour[j].nc,
                                        afd_stat[i].hour[j].ne);
                        }
                        tmp = afd_stat[0].sec_counter * STAT_RESCAN_TIME;
                        if ((tmp % 60) == 0)
                        {
                           tmp = tmp / 60;
                        }
                        else
                        {
                           tmp = -1;
                        }
                        display_data(SHOW_SPACE, NULL, tmp, '*',
                                     afd_stat[i].sec_counter, 0.0, 0.0,
                                     0.0, 0.0);
                        for (j = (afd_stat[i].sec_counter + 1);
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
                           nfs += (double)afd_stat[i].hour[j].nfs;
                           nbs +=         afd_stat[i].hour[j].nbs;
                           nc  += (double)afd_stat[i].hour[j].nc;
                           ne  += (double)afd_stat[i].hour[j].ne;
                           display_data(SHOW_SPACE, NULL, tmp, ' ', j,
                                        afd_stat[i].hour[j].nfs,
                                        afd_stat[i].hour[j].nbs,
                                        afd_stat[i].hour[j].nc,
                                        afd_stat[i].hour[j].ne);
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
                        nfs += (double)afd_stat[i].hour[j].nfs;
                        nbs +=         afd_stat[i].hour[j].nbs;
                        nc  += (double)afd_stat[i].hour[j].nc;
                        ne  += (double)afd_stat[i].hour[j].ne;
                        display_data(i, afd_stat[i].hostname, -1, ' ', -1,
                                     afd_stat[i].hour[j].nfs,
                                     afd_stat[i].hour[j].nbs,
                                     afd_stat[i].hour[j].nc,
                                     afd_stat[i].hour[j].ne);
                     }
                     tmp_nfs += nfs; tmp_nbs += nbs;
                     tmp_nc += nc; tmp_ne += ne;
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
                        if (show_min == 0) /* Show all minutes of the hour */
                        {
                           if (afd_stat[position].sec_counter == 0)
                           {
                              display_data(position,
                                           afd_stat[position].hostname,
                                           0, '*', 0, 0.0, 0.0, 0.0, 0.0);
                           }
                           else
                           {
                              display_data(position,
                                           afd_stat[position].hostname,
                                           0, ' ', 0,
                                           afd_stat[position].hour[0].nfs,
                                           afd_stat[position].hour[0].nbs,
                                           afd_stat[position].hour[0].nc,
                                           afd_stat[position].hour[0].ne);
                              for (j = 1; j < afd_stat[position].sec_counter; j++)
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
                                 nfs += (double)afd_stat[position].hour[j].nfs;
                                 nbs +=         afd_stat[position].hour[j].nbs;
                                 nc  += (double)afd_stat[position].hour[j].nc;
                                 ne  += (double)afd_stat[position].hour[j].ne;
                                 display_data(SHOW_SPACE, NULL, tmp, ' ', j,
                                              afd_stat[position].hour[j].nfs,
                                              afd_stat[position].hour[j].nbs,
                                              afd_stat[position].hour[j].nc,
                                              afd_stat[position].hour[j].ne);
                              }
                              tmp = afd_stat[position].sec_counter * STAT_RESCAN_TIME;
                              if ((tmp % 60) == 0)
                              {
                                 tmp = tmp / 60;
                              }
                              else
                              {
                                 tmp = -1;
                              }
                              display_data(SHOW_SPACE, NULL, tmp, '*',
                                           afd_stat[position].sec_counter,
                                           0.0, 0.0, 0.0, 0.0);
                           }
                           for (j = (afd_stat[position].sec_counter + 1);
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
                              nfs += (double)afd_stat[position].hour[j].nfs;
                              nbs +=         afd_stat[position].hour[j].nbs;
                              nc  += (double)afd_stat[position].hour[j].nc;
                              ne  += (double)afd_stat[position].hour[j].ne;
                              display_data(SHOW_SPACE, NULL, tmp, ' ', j,
                                           afd_stat[position].hour[j].nfs,
                                           afd_stat[position].hour[j].nbs,
                                           afd_stat[position].hour[j].nc,
                                           afd_stat[position].hour[j].ne);
                           }
                        }
                        else /* Show a specific interval */
                        {
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
                              display_data(position,
                                           afd_stat[position].hostname,
                                           -1, ' ', -1,
                                           afd_stat[position].hour[j].nfs,
                                           afd_stat[position].hour[j].nbs,
                                           afd_stat[position].hour[j].nc,
                                           afd_stat[position].hour[j].ne);
                           }
                           else
                           {
                              display_data(position,
                                           afd_stat[position].hostname,
                                           -1, ' ', -1, 0.0, 0.0, 0.0, 0.0);
                           }
                        }
                        tmp_nfs += nfs; tmp_nbs += nbs;
                        tmp_nc += nc; tmp_ne += ne;
                     }
                  }
               }

               if ((show_year > -1) || (show_day > -1) || (show_hour > -1) ||
                   (show_day_summary > -1) || (show_hour_summary > -1))
               {
                  if (display_format == CSV_FORMAT)
                  {
                     display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                  }
                  else if (display_format == NUMERIC_TOTAL_ONLY)
                       {
                          (void)fprintf(output_fp, "%.0f %.0f %.0f %.0f\n", tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                       }
                       else
                       {
                          display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                       }
               }
               else
               {
                  total_nfs += tmp_nfs; total_nbs += tmp_nbs;
                  total_nc += tmp_nc; total_ne += tmp_ne;
               }
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(double_single_line, '=', max_alias_name_length);
                  double_single_line[max_alias_name_length] = '\0';
                  (void)fprintf(output_fp, "%s=================================================================\n", double_single_line);
               }
            } /* if (show_min > -1) */

            /*
             * Show summary on a per minute basis for the last hour.
             */
            if (show_min_summary > -1)
            {
               tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(space_line, ' ', max_alias_name_length / 2);
                  space_line[max_alias_name_length / 2] = '\0';
                  (void)memset(double_single_line, '=', max_alias_name_length / 2);
                  double_single_line[max_alias_name_length / 2] = '\0';
                  (void)fprintf(output_fp, "%s                 =============================\n", space_line);
                  (void)fprintf(output_fp, "%s===============> AFD STATISTICS MINUTE SUMMARY <%s=================\n", double_single_line, double_single_line);
                  (void)fprintf(output_fp, "%s                 =============================\n", space_line);
               }
               else if (display_format == CSV_FORMAT)
                    {
                       if (show_alias == YES)
                       {
                          (void)fprintf(output_fp,
                                        "alias;val1;current;val2;files;size;connects;errors\n");
                       }
                       else
                       {
                          (void)fprintf(output_fp,
                                        "alias;name1;name2;val1;current;val2;files;size;connects;errors\n");
                       }
                    }
            }
            if (show_min_summary == 0)
            {
               int tmp;

               for (j = 0; j < afd_stat[0].sec_counter; j++)
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
                  nfs = nbs = nc = ne = 0.0;
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs += (double)afd_stat[i].hour[j].nfs;
                     nbs +=         afd_stat[i].hour[j].nbs;
                     nc  += (double)afd_stat[i].hour[j].nc;
                     ne  += (double)afd_stat[i].hour[j].ne;
                  }
                  display_data(SHOW_SPACE, NULL, tmp, ' ', j, nfs, nbs, nc, ne);

                  tmp_nfs += nfs; tmp_nbs += nbs;
                  tmp_nc += nc; tmp_ne += ne;
               }
               tmp = afd_stat[0].sec_counter * STAT_RESCAN_TIME;
               if ((tmp % 60) == 0)
               {
                  tmp = tmp / 60;
               }
               else
               {
                  tmp = -1;
               }
               display_data(SHOW_SPACE, NULL, tmp, '*', afd_stat[0].sec_counter,
                            0.0, 0.0, 0.0, 0.0);
               for (j = (afd_stat[0].sec_counter + 1); j < SECS_PER_HOUR; j++)
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
                  nfs = nbs = nc = ne = 0.0;
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs += (double)afd_stat[i].hour[j].nfs;
                     nbs +=         afd_stat[i].hour[j].nbs;
                     nc  += (double)afd_stat[i].hour[j].nc;
                     ne  += (double)afd_stat[i].hour[j].ne;
                  }
                  display_data(SHOW_SPACE, NULL, tmp, ' ', j, nfs, nbs, nc, ne);
                  tmp_nfs += nfs; tmp_nbs += nbs;
                  tmp_nc += nc; tmp_ne += ne;
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
                          tmp = j * STAT_RESCAN_TIME;
                          if ((tmp % 60) == 0)
                          {
                             tmp = tmp / 60;
                          }
                          else
                          {
                             tmp = -1;
                          }
                          nfs = nbs = nc = ne = 0.0;
                          for (i = 0; i < no_of_hosts; i++)
                          {
                             nfs += (double)afd_stat[i].hour[j].nfs;
                             nbs +=         afd_stat[i].hour[j].nbs;
                             nc  += (double)afd_stat[i].hour[j].nc;
                             ne  += (double)afd_stat[i].hour[j].ne;
                          }
                          display_data(SHOW_SPACE, NULL, tmp, ' ', j,
                                       nfs, nbs, nc, ne);
                          tmp_nfs += nfs; tmp_nbs += nbs;
                          tmp_nc += nc; tmp_ne += ne;
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
                          nfs = nbs = nc = ne = 0.0;
                          for (i = 0; i < no_of_hosts; i++)
                          {
                             nfs += (double)afd_stat[i].hour[j].nfs;
                             nbs +=         afd_stat[i].hour[j].nbs;
                             nc  += (double)afd_stat[i].hour[j].nc;
                             ne  += (double)afd_stat[i].hour[j].ne;
                          }
                          display_data(SHOW_SPACE, NULL, tmp, ' ', j,
                                       nfs, nbs, nc, ne);
                          tmp_nfs += nfs; tmp_nbs += nbs;
                          tmp_nc += nc; tmp_ne += ne;
                       }
                    }
                    else
                    {
                       for (j = left; j < afd_stat[0].sec_counter; j++)
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
                          nfs = nbs = nc = ne = 0.0;
                          for (i = 0; i < no_of_hosts; i++)
                          {
                             nfs += (double)afd_stat[i].hour[j].nfs;
                             nbs +=         afd_stat[i].hour[j].nbs;
                             nc  += (double)afd_stat[i].hour[j].nc;
                             ne  += (double)afd_stat[i].hour[j].ne;
                          }
                          display_data(SHOW_SPACE, NULL, tmp, ' ', j,
                                       nfs, nbs, nc, ne);
                          tmp_nfs += nfs; tmp_nbs += nbs;
                          tmp_nc += nc; tmp_ne += ne;
                       }
                    }
                 }

            if (show_min_summary > -1)
            {
               if ((show_year > -1) || (show_day > -1) ||
                   (show_day_summary > -1) || (show_hour > -1))
               {
                  if (display_format == CSV_FORMAT)
                  {
                     display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                  }
                  else if (display_format == NUMERIC_TOTAL_ONLY)
                       {
                          (void)fprintf(output_fp, "%.0f %.0f %.0f %.0f\n", tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                       }
                       else
                       {
                          display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
                       }
               }
               else
               {
                  total_nfs += tmp_nfs; total_nbs += tmp_nbs;
                  total_nc += tmp_nc; total_ne += tmp_ne;
               }
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(double_single_line, '=', max_alias_name_length);
                  double_single_line[max_alias_name_length] = '\0';
                  (void)fprintf(output_fp, "%s=================================================================\n", double_single_line);
               }
            }

            if (display_format == NUMERIC_TOTAL_ONLY)
            {
               (void)fprintf(output_fp, "%.0f %.0f %.0f %.0f\n",
                             total_nfs, total_nbs, total_nc, total_ne);
            }
            else
            {
               display_data(SHOW_BIG_TOTAL, NULL, -1, ' ', -1, total_nfs,
                            total_nbs, total_nc, total_ne);
            }
         }

#ifdef HAVE_MMAP
# ifdef HAVE_STATX
         if (munmap(ptr, stat_buf.stx_size) < 0)
# else
         if (munmap(ptr, stat_buf.st_size) < 0)
# endif
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

   if (output_file_name[0] != '\0')
   {
      (void)fclose(output_fp);
   }
   fsa_detach(NO);

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++ display_data() +++++++++++++++++++++++++++*/
static void
display_data(int    position,
             char   *hostname,
             int    val1,
             char   current,
             int    val2,
             double nfs,
             double nbs,
             double nc,
             double ne)
{
   char name[MAX_PATH_LENGTH];

   if (position == SHOW_SMALL_TOTAL)
   {
      /* Total */
      name[0] = 'T'; name[1] = 'o'; name[2] = 't'; name[3] = 'a';
      name[4] = 'l'; name[5] = '\0';
   }
   else if (position == SHOW_BIG_TOTAL)
        {
           /* TOTAL */
           name[0] = 'T'; name[1] = 'O'; name[2] = 'T'; name[3] = 'A';
           name[4] = 'L'; name[5] = '\0';
        }
   else if (position == SHOW_SPACE)
        {
           /* Space */
           name[0] = ' '; name[1] = '\0';
        }
        else
        {
           (void)strcpy(name, hostname);
        }

   if (display_format == NORMAL_OUTPUT)
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
      if (nbs >= F_EXABYTE)
      {
         (void)fprintf(output_fp, "%-*s %2s %c%4s%14.0f%12.3f EB%14.0f%12.0f\n",
                       max_alias_name_length, name, str1, current, str2,
                       nfs, nbs / F_EXABYTE, nc, ne);
      }
      else if (nbs >= F_PETABYTE)
           {
              (void)fprintf(output_fp, "%-*s %2s %c%4s%14.0f%12.3f PB%14.0f%12.0f\n",
                            max_alias_name_length, name, str1, current, str2,
                            nfs, nbs / F_PETABYTE, nc, ne);
           }
      else if (nbs >= F_TERABYTE)
           {
              (void)fprintf(output_fp, "%-*s %2s %c%4s%14.0f%12.3f TB%14.0f%12.0f\n",
                            max_alias_name_length, name, str1, current, str2,
                            nfs, nbs / F_TERABYTE, nc, ne);
           }
      else if (nbs >= F_GIGABYTE)
           {
              (void)fprintf(output_fp, "%-*s %2s %c%4s%14.0f%12.3f GB%14.0f%12.0f\n",
                            max_alias_name_length, name, str1, current, str2,
                            nfs, nbs / F_GIGABYTE, nc, ne);
           }
      else if (nbs >= F_MEGABYTE)
           {
              (void)fprintf(output_fp, "%-*s %2s %c%4s%14.0f%12.3f MB%14.0f%12.0f\n",
                            max_alias_name_length, name, str1, current, str2,
                            nfs,  nbs / F_MEGABYTE, nc, ne);
           }
      else if (nbs >= F_KILOBYTE)
           {
              (void)fprintf(output_fp, "%-*s %2s %c%4s%14.0f%12.3f KB%14.0f%12.0f\n",
                            max_alias_name_length, name, str1, current, str2,
                            nfs,  nbs / F_KILOBYTE, nc, ne);
           }
           else
           {
              (void)fprintf(output_fp, "%-*s %2s %c%4s%14.0f%12.0f B %14.0f%12.0f\n",
                            max_alias_name_length, name, str1, current, str2,
                            nfs,  nbs, nc, ne);
           }
   }
   else if (display_format == CSV_FORMAT)
        {
           if ((name[1] != '\0') && (name[0] != ' '))
           {
              (void)strcpy(prev_name, name);
              if (show_alias == NO)
              {
                 get_real_hostname(name, real_hostname_1, real_hostname_2);
              }
           }
           if (show_alias == YES)
           {
              (void)fprintf(output_fp, "%s;%d;%d;%d;%.0f;%.0f;%.0f;%.0f\n",
                            prev_name, val1, (current == '*') ? 1 : -1,
                            val2, nfs, nbs, nc, ne);
           }
           else
           {
              (void)fprintf(output_fp, "%s;%s;%s;%d;%d;%d;%.0f;%.0f;%.0f;%.0f\n",
                            prev_name, real_hostname_1, real_hostname_2,
                            val1, (current == '*') ? 1 : -1,
                            val2, nfs, nbs, nc, ne);
           }
        }

   return;
}
