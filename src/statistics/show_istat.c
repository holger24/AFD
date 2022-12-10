/*
 *  show_istat.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **               -t[u]           put in a time stamp for time the output
 **                               is valid. With the u the time is shown
 **                               in unix time.
 **               -C              Output in CSV format.
 **               -N              Show directory name not alias.
 **               -n              Show alias and directory name.
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
 **   01.03.2003 H.Kiehl   Created
 **   23.07.2006 H.Kiehl   Added -T option.
 **   27.04.2018 S.Schuetz Added option to format output in CSV format.
 **   05.11.2018 H.Kiehl   Added -n option.
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
int                        fra_fd,
                           fra_id,
                           max_alias_name_length = MAX_DIR_ALIAS_LENGTH,
                           no_of_dirs,
                           sys_log_fd = STDERR_FILENO; /* Used by get_afd_path() */
#ifdef HAVE_MMAP
off_t                      fra_size;
#endif
char                       *p_work_dir,
                           **arglist; /* Holds list of dirs from command line when given. */
const char                 *sys_log_name = SYSTEM_LOG_FIFO;
struct fileretrieve_status *fra = NULL;

/* Local global variables. */
static int                 display_format = NORMAL_OUTPUT,
                           show_alias = YES;
static char                prev_name[MAX_PATH_LENGTH];
static FILE                *output_fp = NULL;
static struct afdistat     *afd_istat;

/* Function prototypes. */
static void                display_data(int, char *, int, char, int,
                                        double, double);


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
                dir_counter = -1,
                year;
   time_t       now;
   char         *double_single_line = NULL,
                output_file_name[MAX_PATH_LENGTH],
                *space_line = NULL,
                statistic_file_name[MAX_FILENAME_LENGTH],
                statistic_file[MAX_PATH_LENGTH  + FIFO_DIR_LENGTH + MAX_FILENAME_LENGTH + 1 + MAX_INT_LENGTH],
                work_dir[MAX_PATH_LENGTH];
   struct tm    *p_ts;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   CHECK_FOR_VERSION(argc, argv);

   /* Evaluate arguments */
   (void)strcpy(statistic_file_name, ISTATISTIC_FILE);
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   eval_input_ss(argc, argv, statistic_file_name, output_file_name, &show_day,
                 &show_day_summary, &show_hour, &show_hour_summary,
                 &show_min_range, &show_min, &show_min_summary, &show_year,
                 &dir_counter, &show_time_stamp, &display_format, &show_alias,
                 YES, &options);

   /* Initialize variables */
   prev_name[0] = '\0';
   p_work_dir = work_dir;

   if ((show_alias == NO) || (show_alias == BOTH))
   {
      get_max_name_length();
   }

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
      int          no_of_stat_entries,
                   position,
                   *show_index = NULL,
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
         afd_istat = (struct afd_year_istat *)(ptr + AFD_WORD_OFFSET);

#ifdef HAVE_STATX
         no_of_stat_entries = (stat_buf.stx_size - AFD_WORD_OFFSET) /
                              sizeof(struct afd_year_istat);
#else
         no_of_stat_entries = (stat_buf.st_size - AFD_WORD_OFFSET) /
                              sizeof(struct afd_year_istat);
#endif
         if ((show_index = malloc(no_of_stat_entries * sizeof(int))) == NULL)
         {
            (void)fprintf(stderr,
                          "ERROR   : Failed to allocate memory : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         if (options & ONLY_SHOW_REMOTE_DIRS)
         {
            if (fra == NULL)
            {
               (void)fra_attach_passive();
            }
            for (i = 0; i < no_of_stat_entries; i++)
            {
               show_index[i] = YES;
               for (j = 0; j < no_of_dirs; j++)
               {
                  if (strcmp(afd_istat[i].dir_alias, fra[j].dir_alias) == 0)
                  {
                     if (fra[j].host_alias[0] == '\0')
                     {
                        show_index[i] = NO;
                     }
                     break;
                  }
               }
            }
         }
         else
         {
            for (i = 0; i < no_of_stat_entries; i++)
            {
               show_index[i] = YES;
            }
         }
         if (show_year != -1)
         {
            /*
             * Show total for all directories.
             */
            tmp_nfr = tmp_nbr = 0.0;

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
                  (void)fprintf(output_fp,
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
               (void)fprintf(output_fp, "%s ===================================\n", space_line);
               (void)fprintf(output_fp, "%s> AFD INPUT STATISTICS SUMMARY %d <%s\n", double_single_line, year, double_single_line);
               (void)fprintf(output_fp, "%s ===================================\n", space_line);
            }
            else if (display_format == CSV_FORMAT)
                 {
                    if (show_alias == BOTH)
                    {
                       (void)fprintf(output_fp,
                                     "alias;name;val1;current;val2;files;size\n");
                    }
                    else
                    {
                       (void)fprintf(output_fp,
                                     "%s;val1;current;val2;files;size\n",
                                     (show_alias == YES) ? "alias" : "name");
                    }
                 }

            if (dir_counter > 0)
            {
               for (i = 0; i < dir_counter; i++)
               {
                  if ((position = locate_dir_year(afd_istat, arglist[i],
                                                  no_of_stat_entries)) < 0)
                  {
                     (void)fprintf(output_fp,
                                   "No directory %s found in statistic database.\n",
                                   arglist[i]);
                  }
                  else
                  {
                     if (show_index[position] == YES)
                     {
                        nfr = nbr = 0.0;
                        for (j = 0; j < DAYS_PER_YEAR; j++)
                        {
                           nfr += (double)afd_istat[position].year[j].nfr;
                           nbr +=         afd_istat[position].year[j].nbr;
                        }
                        display_data(position, afd_istat[position].dir_alias,
                                     -1, ' ', -1, nfr, nbr);
                        tmp_nfr += nfr; tmp_nbr += nbr;
                     }
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
                     for (i = 0; i < no_of_stat_entries; i++)
                     {
                        if (show_index[i] == YES)
                        {
                           nfr += (double)afd_istat[i].year[j].nfr;
                           nbr +=         afd_istat[i].year[j].nbr;
                        }
                     }
                     display_data(SHOW_SPACE, NULL, -1, ' ', j, nfr, nbr);
                     tmp_nfr += nfr;
                     tmp_nbr += nbr;
                  }
               }
               else
               {
                  /* Make a summary of everything */
                  for (i = 0; i < no_of_stat_entries; i++)
                  {
                     if (show_index[i] == YES)
                     {
                        nfr = nbr = 0.0;
                        for (j = 0; j < DAYS_PER_YEAR; j++)
                        {
                           nfr += (double)afd_istat[i].year[j].nfr;
                           nbr +=         afd_istat[i].year[j].nbr;
                        }
                        display_data(i, afd_istat[i].dir_alias,
                                     -1, ' ', -1, nfr, nbr);
                        tmp_nfr += nfr; tmp_nbr += nbr;
                     }
                  }
               }
            }

            if (display_format == CSV_FORMAT)
            {
               display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfr, tmp_nbr);
            }
            else if (display_format == NUMERIC_TOTAL_ONLY)
                 {
                    (void)fprintf(output_fp, "%.0f %.0f\n", tmp_nfr, tmp_nbr);
                 }
                 else
                 {
                    (void)memset(double_single_line, '-', max_alias_name_length);
                    double_single_line[max_alias_name_length] = '\0';
                    (void)fprintf(output_fp, "%s---------------------------------\n", double_single_line);
                    display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfr, tmp_nbr);
                    (void)memset(double_single_line, '=', max_alias_name_length);
                    (void)fprintf(output_fp, "%s=================================\n", double_single_line);
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
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(space_line, ' ', max_alias_name_length / 2);
                  space_line[max_alias_name_length / 2] = '\0';
                  (void)memset(double_single_line, '=', max_alias_name_length / 2);
                  double_single_line[max_alias_name_length / 2] = '\0';
                  (void)fprintf(output_fp, "%s     ========================\n", space_line);
                  (void)fprintf(output_fp, "%s===> AFD INPUT STATISTICS DAY <%s===\n", double_single_line, double_single_line);
                  (void)fprintf(output_fp, "%s     ========================\n", space_line);
               }
               else if (display_format == CSV_FORMAT)
                    {
                       if (show_alias == BOTH)
                       {
                          (void)fprintf(output_fp,
                                        "alias;name;val1;current;val2;files;size\n");
                       }
                       else
                       {
                          (void)fprintf(output_fp,
                                        "%s;val1;current;val2;files;size\n",
                                        (show_alias == YES) ? "alias" : "name");
                       }
                    }
               if (dir_counter < 0)
               {
                  for (i = 0; i < no_of_stat_entries; i++)
                  {
                     if (show_index[i] == YES)
                     {
                        nfr = nbr = 0.0;
                        if (show_day == 0) /* Show all days */
                        {
                           display_data(i, afd_istat[i].dir_alias,
                                        -1, ' ', 0, afd_istat[i].year[0].nfr,
                                        afd_istat[i].year[0].nbr);
                           for (j = 1; j < DAYS_PER_YEAR; j++)
                           {
                              display_data(SHOW_SPACE, NULL, -1, ' ', j,
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
                           display_data(i, afd_istat[i].dir_alias,
                                        -1, ' ', -1,
                                        afd_istat[i].year[show_day].nfr,
                                        afd_istat[i].year[show_day].nbr);
                        }
                        tmp_nfr += nfr; tmp_nbr += nbr;
                     }
                  }
               } /* if (dir_counter < 0) */
               else /* Show some specific directory */
               {
                  for (i = 0; i < dir_counter; i++)
                  {
                     if ((position = locate_dir_year(afd_istat, arglist[i],
                                                     no_of_stat_entries)) < 0)
                     {
                        (void)fprintf(stdout,
                                      "No directory %s found in statistic database.\n",
                                      arglist[i]);
                     }
                     else
                     {
                        if (show_index[position] == YES)
                        {
                           nfr = nbr = 0.0;
                           if (show_day == 0) /* Show all days */
                           {
                              display_data(position,
                                           afd_istat[position].dir_alias,
                                           -1, ' ', 0,
                                           afd_istat[position].year[0].nfr,
                                           afd_istat[position].year[0].nbr);
                              for (j = 1; j < DAYS_PER_YEAR; j++)
                              {
                                 display_data(SHOW_SPACE, NULL, -1, ' ', j,
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
                              display_data(position,
                                           afd_istat[position].dir_alias,
                                           -1, ' ', -1,
                                           afd_istat[position].year[show_day].nfr,
                                           afd_istat[position].year[show_day].nbr);
                           }
                           tmp_nfr += nfr; tmp_nbr += nbr;
                        }
                     }
                  }
               }

               if ((show_year > -1) || (show_day_summary > -1))
               {
                  if (display_format == CSV_FORMAT)
                  {
                     display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfr, tmp_nbr);
                  }
                  else if (display_format == NUMERIC_TOTAL_ONLY)
                       {
                          (void)fprintf(output_fp, "%.0f %.0f\n", tmp_nfr, tmp_nbr);
                       }
                       else
                       {
                          display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfr, tmp_nbr);
                       }
               }
               else
               {
                  total_nfr += tmp_nfr; total_nbr += tmp_nbr;
               }
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(double_single_line, '=', max_alias_name_length);
                  double_single_line[max_alias_name_length] = '\0';
                  (void)fprintf(output_fp, "%s=================================\n", double_single_line);
               }
            } /* if (show_day > -1) */

            /*
             * Show total summary on a per day basis for this year.
             */
            if (show_day_summary > -1)
            {
               if ((display_format == NORMAL_OUTPUT) && (show_time_stamp > 0))
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

               tmp_nfr = tmp_nbr = 0.0;
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(space_line, ' ', max_alias_name_length / 2);
                  space_line[max_alias_name_length / 2] = '\0';
                  (void)memset(double_single_line, '=', max_alias_name_length / 2);
                  double_single_line[max_alias_name_length / 2] = '\0';
                  (void)fprintf(output_fp, "%s  ================================\n", space_line);
                  (void)fprintf(output_fp, "%s> AFD INPUT STATISTICS DAY SUMMARY <%s\n", double_single_line, double_single_line);
                  (void)fprintf(output_fp, "%s  ================================\n", space_line);
               }
               else if (display_format == CSV_FORMAT)
                    {
                       if (show_alias == BOTH)
                       {
                          (void)fprintf(output_fp,
                                        "alias;name;val1;current;val2;files;size\n");
                       }
                       else
                       {
                          (void)fprintf(output_fp,
                                        "%s;val1;current;val2;files;size\n",
                                        (show_alias == YES) ? "alias" : "name");
                       }
                    }
               for (j = 0; j < DAYS_PER_YEAR; j++)
               {
                  nfr = nbr = 0.0;
                  for (i = 0; i < no_of_stat_entries; i++)
                  {
                     if (show_index[i] == YES)
                     {
                        nfr += (double)afd_istat[i].year[j].nfr;
                        nbr +=         afd_istat[i].year[j].nbr;
                     }
                  }
                  display_data(SHOW_SPACE, NULL, -1, ' ', j, nfr, nbr);
                  tmp_nfr += nfr;
                  tmp_nbr += nbr;
               }

               if ((show_year > -1) || (show_day > -1))
               {
                  if (display_format == CSV_FORMAT)
                  {
                     display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfr, tmp_nbr);
                  }
                  else if (display_format == NUMERIC_TOTAL_ONLY)
                       {
                          (void)fprintf(output_fp, "%.0f %.0f\n", tmp_nfr, tmp_nbr);
                       }
                       else
                       {
                          display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfr, tmp_nbr);
                       }
               }
               else
               {
                  total_nfr += tmp_nfr;
                  total_nbr += tmp_nbr;
               }
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(double_single_line, '=', max_alias_name_length);
                  double_single_line[max_alias_name_length] = '\0';
                  (void)fprintf(output_fp, "%s=================================\n", double_single_line);
               }
            } /* if (show_day_summary > -1) */

            if (display_format == CSV_FORMAT)
            {
               display_data(SHOW_BIG_TOTAL, NULL, -1, ' ', -1, total_nfr, total_nbr);
            }
            else if (display_format == NUMERIC_TOTAL_ONLY)
                 {
                    (void)fprintf(output_fp, "%.0f %.0f\n", total_nfr, total_nbr);
                 }
                 else
                 {
                    display_data(SHOW_BIG_TOTAL, NULL, -1, ' ', -1, total_nfr, total_nbr);
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
         char            *ptr;

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
         afd_istat = (struct afdistat *)(ptr + AFD_WORD_OFFSET);
#ifdef HAVE_STATX
         no_of_stat_entries = (stat_buf.stx_size - AFD_WORD_OFFSET) /
                              sizeof(struct afdistat);
#else
         no_of_stat_entries = (stat_buf.st_size - AFD_WORD_OFFSET) /
                              sizeof(struct afdistat);
#endif
         if ((show_index = malloc(no_of_stat_entries * sizeof(int))) == NULL)
         {
            (void)fprintf(stderr,
                          "ERROR   : Failed to allocate memory : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         if (options & ONLY_SHOW_REMOTE_DIRS)
         {
            if (fra == NULL)
            {
               (void)fra_attach_passive();
            }
            for (i = 0; i < no_of_stat_entries; i++)
            {
               show_index[i] = YES;
               for (j = 0; j < no_of_dirs; j++)
               {
                  if (strcmp(afd_istat[i].dir_alias, fra[j].dir_alias) == 0)
                  {
                     if (fra[j].host_alias[0] == '\0')
                     {
                        show_index[i] = NO;
                     }
                     break;
                  }
               }
            }
         }
         else
         {
            for (i = 0; i < no_of_stat_entries; i++)
            {
               show_index[i] = YES;
            }
         }

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
            tmp_nfr = tmp_nbr = 0.0;
            if (display_format == NORMAL_OUTPUT)
            {
               (void)memset(space_line, ' ', max_alias_name_length / 2);
               space_line[max_alias_name_length / 2] = '\0';
               (void)memset(double_single_line, '=', max_alias_name_length / 2);
               double_single_line[max_alias_name_length / 2] = '\0';
               (void)fprintf(output_fp, "%s  ========================================\n", space_line);
               (void)fprintf(output_fp, "%s> AFD INPUT STATISTICS LAST %2d MINUTE(S) <%s\n", double_single_line, show_min_range, double_single_line);
               (void)fprintf(output_fp, "%s  ========================================\n", space_line);
            }
            else if (display_format == CSV_FORMAT)
                 {
                    if (show_alias == BOTH)
                    {
                       (void)fprintf(output_fp,
                                     "alias;name;val1;current;val2;files;size\n");
                    }
                    else
                    {
                       (void)fprintf(output_fp,
                                     "%s;val1;current;val2;files;size\n",
                                     (show_alias == YES) ? "alias" : "name");
                    }
                 }
            if (dir_counter < 0)
            {
               for (i = 0; i < no_of_stat_entries; i++)
               {
                  if (show_index[i] == YES)
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
                     display_data(i, afd_istat[i].dir_alias,
                                  -1, ' ', -1, nfr, nbr);
                     tmp_nfr += nfr; tmp_nbr += nbr;
                  }
               }
            }
            else
            {
               for (i = 0; i < dir_counter; i++)
               {
                  if ((position = locate_dir(afd_istat, arglist[i],
                                             no_of_stat_entries)) < 0)
                  {
                     (void)fprintf(stdout,
                                   "No dir alias %s found in statistic database.\n",
                                   arglist[i]);
                  }
                  else
                  {
                     if (show_index[position] == YES)
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
                        display_data(position, afd_istat[position].dir_alias,
                                     -1, ' ', -1, nfr, nbr);
                        tmp_nfr += nfr; tmp_nbr += nbr;
                     }
                  }
               }
            }
            if (display_format == CSV_FORMAT)
            {
               display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfr, tmp_nbr);
            }
            else if (display_format == NUMERIC_TOTAL_ONLY)
                 {
                    (void)fprintf(output_fp, "%.0f %.0f\n", tmp_nfr, tmp_nbr);
                 }
                 else
                 {
                    (void)memset(double_single_line, '-', max_alias_name_length);
                    double_single_line[max_alias_name_length] = '\0';
                    (void)fprintf(output_fp, "%s---------------------------------\n", double_single_line);
                    display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfr, tmp_nbr);
                    (void)memset(double_single_line, '=', max_alias_name_length);
                    (void)fprintf(output_fp, "%s=================================\n", double_single_line);
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
             * Show total for all directories.
             */
            tmp_nfr = tmp_nbr = 0.0;

            if (display_format == NORMAL_OUTPUT)
            {
               (void)memset(space_line, ' ', max_alias_name_length / 2);
               space_line[max_alias_name_length / 2] = '\0';
               (void)memset(double_single_line, '=', max_alias_name_length / 2);
               double_single_line[max_alias_name_length / 2] = '\0';
               (void)fprintf(output_fp, "%s   ============================\n", space_line);
               (void)fprintf(output_fp, "%s=> AFD INPUT STATISTICS SUMMARY <%s=\n", double_single_line, double_single_line);
               (void)fprintf(output_fp, "%s   ============================\n", space_line);
            }
            else if (display_format == CSV_FORMAT)
                 {
                    if (show_alias == BOTH)
                    {
                       (void)fprintf(output_fp,
                                     "alias;name;val1;current;val2;files;size\n");
                    }
                    else
                    {
                       (void)fprintf(output_fp,
                                     "%s;val1;current;val2;files;size\n",
                                     (show_alias == YES) ? "alias" : "name");
                    }
                 }

            if (dir_counter > 0)
            {
               for (i = 0; i < dir_counter; i++)
               {
                  if ((position = locate_dir(afd_istat, arglist[i],
                                             no_of_stat_entries)) < 0)
                  {
                     (void)fprintf(stdout,
                                   "No dir alias %s found in statistic database.\n",
                                   arglist[i]);
                  }
                  else
                  {
                     if (show_index[position] == YES)
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
                        display_data(position, afd_istat[position].dir_alias,
                                     -1, ' ', -1, nfr, nbr);
                     }
                  }
               }
            }
            else
            {
               /* Make a summary of everything */
               for (i = 0; i < no_of_stat_entries; i++)
               {
                  if (show_index[i] == YES)
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
                     display_data(i, afd_istat[i].dir_alias,
                                  -1, ' ', -1, nfr, nbr);
                  }
               }
            }
            if (display_format == CSV_FORMAT)
            {
               display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfr, tmp_nbr);
            }
            else if (display_format == NUMERIC_TOTAL_ONLY)
                 {
                    (void)fprintf(output_fp, "%.0f %.0f\n", tmp_nfr, tmp_nbr);
                 }
                 else
                 {
                    (void)memset(double_single_line, '-', max_alias_name_length);
                    double_single_line[max_alias_name_length] = '\0';
                    (void)fprintf(output_fp, "%s---------------------------------\n", double_single_line);
                    display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfr, tmp_nbr);
                    (void)memset(double_single_line, '=', max_alias_name_length);
                    (void)fprintf(output_fp, "%s=================================\n", double_single_line);
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
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(space_line, ' ', max_alias_name_length / 2);
                  space_line[max_alias_name_length / 2] = '\0';
                  (void)memset(double_single_line, '=', max_alias_name_length / 2);
                  double_single_line[max_alias_name_length / 2] = '\0';
                  (void)fprintf(output_fp, "%s     ========================\n", space_line);
                  (void)fprintf(output_fp, "%s===> AFD INPUT STATISTICS DAY <%s==\n", double_single_line, double_single_line);
                  (void)fprintf(output_fp, "%s     ========================\n", space_line);
               }
               else if (display_format == CSV_FORMAT)
                    {
                       if (show_alias == BOTH)
                       {
                          (void)fprintf(output_fp,
                                        "alias;name;val1;current;val2;files;size\n");
                       }
                       else
                       {
                          (void)fprintf(output_fp,
                                        "%s;val1;current;val2;files;size\n",
                                        (show_alias == YES) ? "alias" : "name");
                       }
                    }
               if (dir_counter < 0)
               {
                  for (i = 0; i < no_of_stat_entries; i++)
                  {
                     if (show_index[i] == YES)
                     {
                        nfr = nbr = 0.0;
                        if (show_day == 0) /* Show all days */
                        {
                           if (afd_istat[i].day_counter == 0)
                           {
                              display_data(i, afd_istat[i].dir_alias,
                                           -1, ' ', 0, 0.0, 0.0);
                           }
                           else
                           {
                              display_data(i, afd_istat[i].dir_alias,
                                           -1, ' ', 0,
                                           afd_istat[i].year[0].nfr,
                                           afd_istat[i].year[0].nbr);
                              for (j = 1; j < afd_istat[i].day_counter; j++)
                              {
                                 display_data(SHOW_SPACE, NULL, -1, ' ', j,
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
                              display_data(i, afd_istat[i].dir_alias,
                                           -1, ' ', -1,
                                           afd_istat[i].year[j].nfr,
                                           afd_istat[i].year[j].nbr);
                           }
                           else
                           {
                              display_data(i, afd_istat[i].dir_alias,
                                           -1, ' ', -1, 0.0, 0.0);
                           }
                        }
                        tmp_nfr += nfr; tmp_nbr += nbr;
                     }
                  }
               } /* if (dir_counter < 0) */
               else /* Show some specific directory */
               {
                  for (i = 0; i < dir_counter; i++)
                  {
                     if ((position = locate_dir(afd_istat, arglist[i],
                                                no_of_stat_entries)) < 0)
                     {
                        (void)fprintf(stdout,
                                      "No dir alias %s found in statistic database.\n",
                                      arglist[i]);
                     }
                     else
                     {
                        if (show_index[position] == YES)
                        {
                           nfr = nbr = 0.0;
                           if (show_day == 0) /* Show all days */
                           {
                              display_data(position,
                                           afd_istat[position].dir_alias,
                                           -1, ' ', 0,
                                           afd_istat[position].year[0].nfr,
                                           afd_istat[position].year[0].nbr);
                              for (j = 1; j < afd_istat[position].day_counter; j++)
                              {
                                 display_data(SHOW_SPACE, NULL, -1, ' ', j,
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
                                 display_data(position,
                                              afd_istat[position].dir_alias,
                                              -1, ' ', -1,
                                              afd_istat[position].year[j].nfr,
                                              afd_istat[position].year[j].nbr);
                              }
                              else
                              {
                                 display_data(position,
                                              afd_istat[position].dir_alias,
                                              -1, ' ', -1, 0.0, 0.0);
                              }
                           }
                           tmp_nfr += nfr; tmp_nbr += nbr;
                        }
                     }
                  }
               }

               if ((show_year > -1) || (show_hour > -1) ||
                   (show_day_summary > -1) || (show_hour_summary > -1))
               {
                  if (display_format == CSV_FORMAT)
                  {
                     display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfr, tmp_nbr);
                  }
                  else if (display_format == NUMERIC_TOTAL_ONLY)
                       {
                          (void)fprintf(output_fp, "%.0f %.0f\n", tmp_nfr, tmp_nbr);
                       }
                       else
                       {
                          display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfr, tmp_nbr);
                       }
               }
               else
               {
                  total_nfr += tmp_nfr; total_nbr += tmp_nbr;
               }
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(double_single_line, '=', max_alias_name_length);
                  double_single_line[max_alias_name_length] = '\0';
                  (void)fprintf(output_fp, "%s=================================\n", double_single_line);
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
               tmp_nfr = tmp_nbr = 0.0;
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(space_line, ' ', max_alias_name_length / 2);
                  space_line[max_alias_name_length / 2] = '\0';
                  (void)memset(double_single_line, '=', max_alias_name_length / 2);
                  double_single_line[max_alias_name_length / 2] = '\0';
                  (void)fprintf(output_fp, "%s  ================================\n", space_line);
                  (void)fprintf(output_fp, "%s> AFD INPUT STATISTICS DAY SUMMARY <%s\n", double_single_line, double_single_line);
                  (void)fprintf(output_fp, "%s  ================================\n", space_line);
               }
               else if (display_format == CSV_FORMAT)
                    {
                       if (show_alias == BOTH)
                       {
                          (void)fprintf(output_fp,
                                        "alias;name;val1;current;val2;files;size\n");
                       }
                       else
                       {
                          (void)fprintf(output_fp,
                                        "%s;val1;current;val2;files;size\n",
                                        (show_alias == YES) ? "alias" : "name");
                       }
                    }
               for (j = 0; j < p_ts->tm_yday; j++)
               {
                  nfr = nbr = 0.0;
                  for (i = 0; i < no_of_stat_entries; i++)
                  {
                     if (show_index[i] == YES)
                     {
                        nfr += (double)afd_istat[i].year[j].nfr;
                        nbr +=         afd_istat[i].year[j].nbr;
                     }
                  }
                  display_data(SHOW_SPACE, NULL, -1, ' ', j, nfr, nbr);
                  tmp_nfr += nfr; tmp_nbr += nbr;
               }

               if ((show_year > -1) || (show_day > -1) ||
                   (show_hour > -1) || (show_hour_summary > -1))
               {
                  if (display_format == CSV_FORMAT)
                  {
                     display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfr, tmp_nbr);
                  }
                  else if (display_format == NUMERIC_TOTAL_ONLY)
                       {
                          (void)fprintf(output_fp, "%.0f %.0f\n", tmp_nfr, tmp_nbr);
                       }
                       else
                       {
                          display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfr, tmp_nbr);
                       }
               }
               else
               {
                  total_nfr += tmp_nfr; total_nbr += tmp_nbr;
               }
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(double_single_line, '=', max_alias_name_length);
                  double_single_line[max_alias_name_length] = '\0';
                  (void)fprintf(output_fp, "%s=================================\n", double_single_line);
               }
            } /* if (show_day_summary > -1) */

            /*
             * Show data for one or all hours for this day.
             */
            if (show_hour > -1)
            {
               double sec_nfr, sec_nbr;

               tmp_nfr = tmp_nbr = 0.0;
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(space_line, ' ', max_alias_name_length / 2);
                  space_line[max_alias_name_length / 2] = '\0';
                  (void)memset(double_single_line, '=', max_alias_name_length / 2);
                  double_single_line[max_alias_name_length / 2] = '\0';
                  (void)fprintf(output_fp, "%s     =========================\n", space_line);
                  (void)fprintf(output_fp, "%s===> AFD INPUT STATISTICS HOUR <%s==\n", double_single_line, double_single_line);
                  (void)fprintf(output_fp, "%s     =========================\n", space_line);
               }
               else if (display_format == CSV_FORMAT)
                    {
                       if (show_alias == BOTH)
                       {
                          (void)fprintf(output_fp,
                                        "alias;name;val1;current;val2;files;size\n");
                       }
                       else
                       {
                          (void)fprintf(output_fp,
                                        "%s;val1;current;val2;files;size\n",
                                        (show_alias == YES) ? "alias" : "name");
                       }
                    }
               if (dir_counter < 0)
               {
                  for (i = 0; i < no_of_stat_entries; i++)
                  {
                     if (show_index[i] == YES)
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
                              display_data(i, afd_istat[i].dir_alias,
                                           -1, '*', 0, sec_nfr, sec_nbr);
                           }
                           else
                           {
                              display_data(i, afd_istat[i].dir_alias,
                                           -1, ' ', 0,
                                           afd_istat[i].day[0].nfr,
                                           afd_istat[i].day[0].nbr);
                              for (j = 1; j < afd_istat[i].hour_counter; j++)
                              {
                                 display_data(SHOW_SPACE, NULL, -1, ' ', j,
                                              afd_istat[i].day[j].nfr,
                                              afd_istat[i].day[j].nbr);
                                 nfr += (double)afd_istat[i].day[j].nfr;
                                 nbr +=         afd_istat[i].day[j].nbr;
                              }
                              display_data(SHOW_SPACE, NULL, -1, '*', j,
                                           sec_nfr, sec_nbr);
                           }
                           nfr += sec_nfr; nbr += sec_nbr;
                           for (j = (afd_istat[i].hour_counter + 1);
                                j < HOURS_PER_DAY; j++)
                           {
                              display_data(SHOW_SPACE, NULL, -1, ' ', j,
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
                              display_data(i, afd_istat[i].dir_alias,
                                           -1, ' ', -1,
                                           afd_istat[i].day[j].nfr,
                                           afd_istat[i].day[j].nbr);
                           }
                           else
                           {
                              display_data(i, afd_istat[i].dir_alias,
                                           -1, ' ', -1, 0.0, 0.0);
                           }
                        }
                        tmp_nfr += nfr; tmp_nbr += nbr;
                     }
                  }
               } /* if (dir_counter < 0) */
               else /* Show some specific directory */
               {
                  for (i = 0; i < dir_counter; i++)
                  {
                     if ((position = locate_dir(afd_istat, arglist[i],
                                                no_of_stat_entries)) < 0)
                     {
                        (void)fprintf(stdout,
                                      "No dir alias %s found in statistic database.\n",
                                      arglist[i]);
                     }
                     else
                     {
                        if (show_index[position] == YES)
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
                                 display_data(position,
                                              afd_istat[position].dir_alias,
                                              -1, '*', 0, sec_nfr, sec_nbr);
                              }
                              else
                              {
                                 display_data(position,
                                              afd_istat[position].dir_alias,
                                              -1, ' ', 0,
                                              afd_istat[position].day[0].nfr,
                                              afd_istat[position].day[0].nbr);
                                 for (j = 1; j < afd_istat[position].hour_counter; j++)
                                 {
                                    display_data(SHOW_SPACE, NULL, -1, ' ', j,
                                                 afd_istat[position].day[j].nfr,
                                                 afd_istat[position].day[j].nbr);
                                    nfr += (double)afd_istat[position].day[j].nfr;
                                    nbr +=         afd_istat[position].day[j].nbr;
                                 }
                                 display_data(SHOW_SPACE, NULL, -1, '*', j,
                                              sec_nfr, sec_nbr);
                              }
                              nfr += sec_nfr; nbr += sec_nbr;
                              for (j = (afd_istat[position].hour_counter + 1);
                                   j < HOURS_PER_DAY; j++)
                              {
                                 display_data(SHOW_SPACE, NULL, -1, ' ', j,
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
                                 display_data(position,
                                              afd_istat[position].dir_alias,
                                              -1, ' ', j,
                                              afd_istat[position].day[j].nfr,
                                              afd_istat[position].day[j].nbr);
                              }
                              else
                              {
                                 display_data(position,
                                              afd_istat[position].dir_alias,
                                              -1, ' ', -1, 0.0, 0.0);
                              }
                           }
                           tmp_nfr += nfr; tmp_nbr += nbr;
                        }
                     }
                  }
               }

               if ((show_year > -1) || (show_day > -1) ||
                   (show_day_summary > -1) || (show_hour_summary > -1))
               {
                  if (display_format == CSV_FORMAT)
                  {
                     display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfr, tmp_nbr);
                  }
                  else if (display_format == NUMERIC_TOTAL_ONLY)
                       {
                          (void)fprintf(output_fp, "%.0f %.0f\n", tmp_nfr, tmp_nbr);
                       }
                       else
                       {
                          display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfr, tmp_nbr);
                       }
               }
               else
               {
                  total_nfr += tmp_nfr; total_nbr += tmp_nbr;
               }
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(double_single_line, '=', max_alias_name_length);
                  double_single_line[max_alias_name_length] = '\0';
                  (void)fprintf(output_fp, "%s=================================\n", double_single_line);
               }
            } /* if (show_hour > -1) */

            /*
             * Show total summary on a per hour basis for the last 24 hours.
             */
            if (show_hour_summary > -1)
            {
               tmp_nfr = tmp_nbr = 0.0;
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(space_line, ' ', max_alias_name_length / 2);
                  space_line[max_alias_name_length / 2] = '\0';
                  (void)memset(double_single_line, '=', max_alias_name_length / 2);
                  double_single_line[max_alias_name_length / 2] = '\0';
                  (void)fprintf(output_fp, "%s  =================================\n", space_line);
                  (void)fprintf(output_fp, "%s> AFD INPUT STATISTICS HOUR SUMMARY <%s\n", double_single_line, double_single_line);
                  (void)fprintf(output_fp, "%s  =================================\n", space_line);
               }
               else if (display_format == CSV_FORMAT)
                    {
                       if (show_alias == BOTH)
                       {
                          (void)fprintf(output_fp,
                                        "alias;name;val1;current;val2;files;size\n");
                       }
                       else
                       {
                          (void)fprintf(output_fp,
                                        "%s;val1;current;val2;files;size\n",
                                        (show_alias == YES) ? "alias" : "name");
                       }
                    }
               for (j = 0; j < afd_istat[0].hour_counter; j++)
               {
                  nfr = nbr = 0.0;
                  for (i = 0; i < no_of_stat_entries; i++)
                  {
                     if (show_index[i] == YES)
                     {
                        nfr += (double)afd_istat[i].day[j].nfr;
                        nbr +=         afd_istat[i].day[j].nbr;
                     }
                  }
                  display_data(SHOW_SPACE, NULL, -1, ' ', j, nfr, nbr);
                  tmp_nfr += nfr; tmp_nbr += nbr;
               }
               nfr = nbr = 0.0;
               for (i = 0; i < no_of_stat_entries; i++)
               {
                  for (j = 0; j < afd_istat[i].sec_counter; j++)
                  {
                     if (show_index[i] == YES)
                     {
                        nfr += (double)afd_istat[i].hour[j].nfr;
                        nbr +=         afd_istat[i].hour[j].nbr;
                     }
                  }
               }
               display_data(SHOW_SPACE, NULL, -1, '*', afd_istat[0].hour_counter, nfr, nbr);
               tmp_nfr += nfr; tmp_nbr += nbr;
               for (j = (afd_istat[0].hour_counter + 1); j < HOURS_PER_DAY; j++)
               {
                  nfr = nbr = 0.0;
                  for (i = 0; i < no_of_stat_entries; i++)
                  {
                     if(show_index[i] == YES)
                     {
                        nfr += (double)afd_istat[i].day[j].nfr;
                        nbr +=         afd_istat[i].day[j].nbr;
                     }
                  }
                  display_data(SHOW_SPACE, NULL, -1, ' ', j, nfr, nbr);
                  tmp_nfr += nfr; tmp_nbr += nbr;
               }

               if ((show_year > -1) || (show_day > -1) ||
                   (show_day_summary > -1) || (show_hour > -1))
               {
                  if (display_format == CSV_FORMAT)
                  {
                     display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfr, tmp_nbr);
                  }
                  else if (display_format == NUMERIC_TOTAL_ONLY)
                       {
                          (void)fprintf(output_fp, "%.0f %.0f\n", tmp_nfr, tmp_nbr);
                       }
                       else
                       {
                          display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfr, tmp_nbr);
                       }
               }
               else
               {
                  total_nfr += tmp_nfr; total_nbr += tmp_nbr;
               }
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(double_single_line, '=', max_alias_name_length);
                  double_single_line[max_alias_name_length] = '\0';
                  (void)fprintf(output_fp, "%s=================================\n", double_single_line);
               }
            } /* if (show_hour_summary > -1) */

            /*
             * Show data for one or all minutes for this hour.
             */
            if (show_min > -1)
            {
               int tmp;

               tmp_nfr = tmp_nbr = 0.0;
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(space_line, ' ', max_alias_name_length / 2);
                  space_line[max_alias_name_length / 2] = '\0';
                  (void)memset(double_single_line, '=', max_alias_name_length / 2);
                  double_single_line[max_alias_name_length / 2] = '\0';
                  (void)fprintf(output_fp, "%s   ===========================\n", space_line);
                  (void)fprintf(output_fp, "%s=> AFD INPUT STATISTICS MINUTE <%s\n", double_single_line, double_single_line);
                  (void)fprintf(output_fp, "%s   ===========================\n", space_line);
               }
               else if (display_format == CSV_FORMAT)
                    {
                       if (show_alias == BOTH)
                       {
                          (void)fprintf(output_fp,
                                        "alias;name;val1;current;val2;files;size\n");
                       }
                       else
                       {
                          (void)fprintf(output_fp,
                                        "%s;val1;current;val2;files;size\n",
                                        (show_alias == YES) ? "alias" : "name");
                       }
                    }
               if (dir_counter < 0)
               {
                  for (i = 0; i < no_of_stat_entries; i++)
                  {
                     if (show_index[i] == YES)
                     {
                        nfr = nbr = 0.0;
                        if (show_min == 0) /* Show all minutes of the hour */
                        {
                           nfr += (double)afd_istat[i].hour[0].nfr;
                           nbr +=         afd_istat[i].hour[0].nbr;
                           display_data(i, afd_istat[i].dir_alias,
                                        0, ' ', 0, afd_istat[i].hour[0].nfr,
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
                              display_data(SHOW_SPACE, NULL, tmp, ' ', j,
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
                           display_data(SHOW_SPACE, NULL, tmp, '*',
                                        afd_istat[i].sec_counter, 0.0, 0.0);
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
                              display_data(SHOW_SPACE, NULL, tmp, ' ', j,
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
                           display_data(i, afd_istat[i].dir_alias,
                                        -1, ' ', -1,
                                        afd_istat[i].hour[j].nfr,
                                        afd_istat[i].hour[j].nbr);
                        }
                        tmp_nfr += nfr; tmp_nbr += nbr;
                     }
                  }
               } /* if (dir_counter < 0) */
               else /* Show some specific directory */
               {
                  for (i = 0; i < dir_counter; i++)
                  {
                     if ((position = locate_dir(afd_istat, arglist[i],
                                                no_of_stat_entries)) < 0)
                     {
                        (void)fprintf(stdout,
                                      "No dir alias %s found in statistic database.\n",
                                      arglist[i]);
                     }
                     else
                     {
                        if (show_index[position] == YES)
                        {
                           nfr = nbr = 0.0;
                           if (show_min == 0) /* Show all minutes of the hour */
                           {
                              if (afd_istat[position].sec_counter == 0)
                              {
                                 display_data(position,
                                              afd_istat[position].dir_alias,
                                              0, '*', 0, 0.0, 0.0);
                              }
                              else
                              {
                                 display_data(position,
                                              afd_istat[position].dir_alias,
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
                                    display_data(SHOW_SPACE, NULL, tmp, ' ', j,
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
                                 display_data(SHOW_SPACE, NULL, tmp, '*',
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
                                 display_data(SHOW_SPACE, NULL, tmp, ' ', j,
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
                                 display_data(position,
                                              afd_istat[position].dir_alias,
                                              -1, ' ', -1,
                                              afd_istat[position].hour[j].nfr,
                                              afd_istat[position].hour[j].nbr);
                              }
                              else
                              {
                                 display_data(position,
                                              afd_istat[position].dir_alias,
                                              -1, ' ', -1, 0.0, 0.0);
                              }
                           }
                           tmp_nfr += nfr; tmp_nbr += nbr;
                        }
                     }
                  }
               }

               if ((show_year > -1) || (show_day > -1) || (show_hour > -1) ||
                   (show_day_summary > -1) || (show_hour_summary > -1))
               {
                  if (display_format == CSV_FORMAT)
                  {
                     display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfr, tmp_nbr);
                  }
                  else if (display_format == NUMERIC_TOTAL_ONLY)
                       {
                          (void)fprintf(output_fp, "%.0f %.0f\n", tmp_nfr, tmp_nbr);
                       }
                       else
                       {
                          display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfr, tmp_nbr);
                       }
               }
               else
               {
                  total_nfr += tmp_nfr; total_nbr += tmp_nbr;
               }
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(double_single_line, '=', max_alias_name_length);
                  double_single_line[max_alias_name_length] = '\0';
                  (void)fprintf(output_fp, "%s=================================\n", double_single_line);
               }
            } /* if (show_min > -1) */

            /*
             * Show summary on a per minute basis for the last hour.
             */
            if (show_min_summary > -1)
            {
               tmp_nfr = tmp_nbr = 0.0;
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(space_line, ' ', max_alias_name_length / 2);
                  space_line[max_alias_name_length / 2] = '\0';
                  (void)memset(double_single_line, '=', max_alias_name_length / 2);
                  double_single_line[max_alias_name_length / 2] = '\0';
                  (void)fprintf(output_fp, "%s  ===================================\n", space_line);
                  (void)fprintf(output_fp, "%s> AFD INPUT STATISTICS MINUTE SUMMARY <%s\n", double_single_line, double_single_line);
                  (void)fprintf(output_fp, "%s  ===================================\n", space_line);
               }
               else if (display_format == CSV_FORMAT)
                    {
                       if (show_alias == BOTH)
                       {
                          (void)fprintf(output_fp,
                                        "alias;name;val1;current;val2;files;size\n");
                       }
                       else
                       {
                          (void)fprintf(output_fp,
                                        "%s;val1;current;val2;files;size\n",
                                        (show_alias == YES) ? "alias" : "name");
                       }
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
                  for (i = 0; i < no_of_stat_entries; i++)
                  {
                     if (show_index[i] == YES)
                     {
                        nfr += (double)afd_istat[i].hour[j].nfr;
                        nbr +=         afd_istat[i].hour[j].nbr;
                     }
                  }
                  display_data(SHOW_SPACE, NULL, tmp, ' ', j, nfr, nbr);
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
               display_data(SHOW_SPACE, NULL, tmp, '*', afd_istat[0].sec_counter, 0.0, 0.0);
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
                  for (i = 0; i < no_of_stat_entries; i++)
                  {
                     if (show_index[i] == YES)
                     {
                        nfr += (double)afd_istat[i].hour[j].nfr;
                        nbr +=         afd_istat[i].hour[j].nbr;
                     }
                  }
                  display_data(SHOW_SPACE, NULL, tmp, ' ', j, nfr, nbr);
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
                          for (i = 0; i < no_of_stat_entries; i++)
                          {
                             if (show_index[i] == YES)
                             {
                                nfr += (double)afd_istat[i].hour[j].nfr;
                                nbr +=         afd_istat[i].hour[j].nbr;
                             }
                          }
                          display_data(SHOW_SPACE, NULL, tmp, ' ', j, nfr, nbr);
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
                          for (i = 0; i < no_of_stat_entries; i++)
                          {
                             if (show_index[i] == YES)
                             {
                                nfr += (double)afd_istat[i].hour[j].nfr;
                                nbr +=         afd_istat[i].hour[j].nbr;
                             }
                          }
                          display_data(SHOW_SPACE, NULL, tmp, ' ', j, nfr, nbr);
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
                          for (i = 0; i < no_of_stat_entries; i++)
                          {
                             if (show_index[i] == YES)
                             {
                                nfr += (double)afd_istat[i].hour[j].nfr;
                                nbr +=         afd_istat[i].hour[j].nbr;
                             }
                          }
                          display_data(SHOW_SPACE, NULL, tmp, ' ', j, nfr, nbr);
                          tmp_nfr += nfr; tmp_nbr += nbr;
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
                     display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfr, tmp_nbr);
                  }
                  else if (display_format == NUMERIC_TOTAL_ONLY)
                       {
                          (void)fprintf(output_fp, "%.0f %.0f\n", tmp_nfr, tmp_nbr);
                       }
                       else
                       {
                          display_data(SHOW_SMALL_TOTAL, NULL, -1, ' ', -1, tmp_nfr, tmp_nbr);
                       }
               }
               else
               {
                  total_nfr += tmp_nfr; total_nbr += tmp_nbr;
               }
               if (display_format == NORMAL_OUTPUT)
               {
                  (void)memset(double_single_line, '=', max_alias_name_length);
                  double_single_line[max_alias_name_length] = '\0';
                  (void)fprintf(output_fp, "%s=================================\n", double_single_line);
               }
            }

            if (display_format == NUMERIC_TOTAL_ONLY)
            {
               (void)fprintf(output_fp, "%.0f %.0f\n", total_nfr, total_nbr);
            }
            else
            {
               display_data(SHOW_BIG_TOTAL, NULL, -1, ' ', -1, total_nfr, total_nbr);
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
   free_get_dir_name();

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++ display_data() +++++++++++++++++++++++++++*/
static void
display_data(int    position,
             char   *dir_alias,
             int    val1,
             char   current,
             int    val2,
             double nfr,
             double nbr)
{
   char name[MAX_PATH_LENGTH];

   if (position == SHOW_SMALL_TOTAL)
   {
      /* Total */
      name[0] = 'T'; name[1] = 'o'; name[2] = 't'; name[3] = 'a';
      name[4] = 'l';
      if ((show_alias == BOTH) && (display_format == CSV_FORMAT))
      {
         name[5] = ';';
         name[6] = '\0';
      }
      else
      {
         name[5] = '\0';
      }
   }
   else if (position == SHOW_BIG_TOTAL)
        {
           /* TOTAL */
           name[0] = 'T'; name[1] = 'O'; name[2] = 'T'; name[3] = 'A';
           name[4] = 'L';
           if ((show_alias == BOTH) && (display_format == CSV_FORMAT))
           {
              name[5] = ';';
              name[6] = '\0';
           }
           else
           {
              name[5] = '\0';
           }
        }
   else if (position == SHOW_SPACE)
        {
           /* Space */
           name[0] = ' '; name[1] = '\0';
        }
        else
        {
           if (show_alias == YES)
           {
              (void)strcpy(name, dir_alias);
           }
           else
           {
              size_t offset;

              if (show_alias == BOTH)
              {
                 (void)strcpy(name, dir_alias);
                 offset = strlen(name);
                 name[offset] = ';';
                 offset++;
              }
              else
              {
                 offset = 0;
              }
              get_dir_name(dir_alias, &name[offset]);
           }
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
      if (nbr >= F_EXABYTE)
      {
         (void)fprintf(output_fp, "%-*s %2s %c%4s%12.0f %8.3f EB\n",
                       max_alias_name_length, name, str1, current, str2,
                       nfr, nbr / F_EXABYTE);
      }
      else if (nbr >= F_PETABYTE)
           {
              (void)fprintf(output_fp, "%-*s %2s %c%4s%12.0f %8.3f PB\n",
                            max_alias_name_length, name, str1, current, str2,
                            nfr, nbr / F_PETABYTE);
           }
      else if (nbr >= F_TERABYTE)
           {
              (void)fprintf(output_fp, "%-*s %2s %c%4s%12.0f %8.3f TB\n",
                            max_alias_name_length, name, str1, current, str2,
                            nfr, nbr / F_TERABYTE);
           }
      else if (nbr >= F_GIGABYTE)
           {
              (void)fprintf(output_fp, "%-*s %2s %c%4s%12.0f %8.3f GB\n",
                            max_alias_name_length, name, str1, current, str2,
                            nfr, nbr / F_GIGABYTE);
           }
      else if (nbr >= F_MEGABYTE)
           {
              (void)fprintf(output_fp, "%-*s %2s %c%4s%12.0f %8.3f MB\n",
                            max_alias_name_length, name, str1, current, str2,
                            nfr,  nbr / F_MEGABYTE);
           }
      else if (nbr >= F_KILOBYTE)
           {
              (void)fprintf(output_fp, "%-*s %2s %c%4s%12.0f %8.3f KB\n",
                            max_alias_name_length, name, str1, current, str2,
                            nfr,  nbr / F_KILOBYTE);
           }
           else
           {
              (void)fprintf(output_fp, "%-*s %2s %c%4s%12.0f %8.0f B\n",
                            max_alias_name_length, name, str1, current, str2,
                            nfr,  nbr);
           }
   }
   else if (display_format == CSV_FORMAT)
        {
           if ((name[1] != '\0') && (name[0] != ' '))
           {
              (void)strcpy(prev_name, name);
           }
           (void)fprintf(output_fp, "%s;%d;%d;%d;%.0f;%.0f\n",
                         prev_name, val1, (current == '*') ? 1 : -1,
                         val2, nfr, nbr);
        }

   return;
}
