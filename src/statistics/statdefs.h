/*
 *  statdefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2020 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __statdefs_h
#define __statdefs_h

#define SHOW_SMALL_TOTAL      -1
#define SHOW_BIG_TOTAL        -2
#define SHOW_SPACE            -3
#define NORMAL_OUTPUT         0
#define NUMERIC_TOTAL_ONLY    1
#define CSV_FORMAT            2
#define ONLY_SHOW_REMOTE_DIRS 3

#define SPACE_LINE  "                                                                                                                                                                                                                                                       "
#define DOUBLE_LINE "======================================================================================================================================================================================================================================================="
#define SINGLE_LINE "-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------"

#define STAT_RESCAN_TIME                              5
#define DAYS_PER_YEAR                               366
#define HOURS_PER_DAY                                24
#define SECS_PER_HOUR         (3600 / STAT_RESCAN_TIME)
#define MAX_FILES_PER_SCAN      (STAT_RESCAN_TIME * 10) /* just assuming */
/**************************************************************************/
/* Assuming 8 Byte for double and 4 Bytes for integer we get the          */
/* following size for one statistic entry per host :                      */
/*                            output                   input              */
/* STAT_RESCAN_TIME               size required in Bytes                  */
/* ---------------------------------------------------------------------- */
/*        1         =>         79877                  143691              */
/*        5         =>         22277                   40011              */
/*       10         =>         15077                   27051              */
/*       20         =>         11477                   20571              */
/*       30         =>         10277                   18411              */
/*       40         =>          9677                   17331              */
/*       50         =>          9317                   16683              */
/*       60         =>          9077                   16251              */
/**************************************************************************/

#define STATISTIC_FILE         "/afd_statistic_file"
#define STATISTIC_FILE_NAME    "afd_statistic_file"
#define NEW_STATISTIC_FILE     "/.afd_statistic_file.NEW"
#define STATISTIC_FILE_ALL     "afd_statistic_file.*"
#define ISTATISTIC_FILE        "/afd_istatistic_file"
#define ISTATISTIC_FILE_NAME   "afd_istatistic_file"
#define NEW_ISTATISTIC_FILE    "/.afd_istatistic_file.NEW"
#define ISTATISTIC_FILE_ALL    "afd_istatistic_file.*"

#define CURRENT_STAT_VERSION 0
struct statistics
       {
          unsigned int nfs;      /* Number of files send  */
          double       nbs;      /* Number of bytes send  */
          unsigned int ne;       /* Number of errors      */
          unsigned int nc;       /* Number of connections */
       };

struct afdstat
       {
          char              hostname[MAX_HOSTNAME_LENGTH + 1];
          time_t            start_time;                 /* Time when acc. for */
                                                        /* this host starts.  */
          int               day_counter;                /* Position in year.  */
          struct statistics year[DAYS_PER_YEAR];        /* Per day.           */
          int               hour_counter;               /* Position in day.   */
          struct statistics day[HOURS_PER_DAY];         /* Per hour.          */
          int               sec_counter;                /* Position in hour.  */
          struct statistics hour[SECS_PER_HOUR];        /* Per                */
                                                        /* STAT_RESCAN_TIME   */
                                                        /* seconds.           */
          unsigned int      prev_nfs;
          double            prev_nbs[MAX_NO_PARALLEL_JOBS];
          unsigned int      prev_ne;
          unsigned int      prev_nc;
       };
#define CURRENT_YEAR_STAT_VERSION 0
struct afd_year_stat
       {
          char              hostname[MAX_HOSTNAME_LENGTH + 1];
          time_t            start_time;                 /* Time when acc. for */
                                                        /* this host starts.  */
          struct statistics year[DAYS_PER_YEAR];
       };

#define CURRENT_ISTAT_VERSION 0
struct istatistics
       {
          unsigned int nfr;      /* Number of files received     */
          double       nbr;      /* Number of bytes received     */
       };

struct afdistat
       {
          char               dir_alias[MAX_DIR_ALIAS_LENGTH + 1];
          time_t             start_time;                /* Time when acc. for */
                                                        /* this dir starts.   */
          int                day_counter;               /* Position in year.  */
          struct istatistics year[DAYS_PER_YEAR];       /* Per day.           */
          int                hour_counter;              /* Position in day.   */
          struct istatistics day[HOURS_PER_DAY];        /* Per hour.          */
          int                sec_counter;               /* Position in hour.  */
          struct istatistics hour[SECS_PER_HOUR];       /* Per                */
                                                        /* STAT_RESCAN_TIME   */
                                                        /* seconds.           */
          unsigned int       prev_nfr;
          double             prev_nbr;
       };
#define CURRENT_YEAR_ISTAT_VERSION 0
struct afd_year_istat
       {
          char               dir_alias[MAX_DIR_ALIAS_LENGTH + 1];
          time_t             start_time;                /* Time when acc. for */
                                                        /* this host starts.  */
          struct istatistics year[DAYS_PER_YEAR];
       };

extern void eval_input_as(int, char **, char *, char *, char *),
            eval_input_ss(int, char **, char *, char *, int *, int *, int *,
                          int *, int *, int *, int *, int *, int *, int *,
                          int *, int *, int, int *),
            free_get_dir_name(void),
            get_dir_name(char *, char *),
            get_max_name_length(void),
            get_real_hostname(char *, char *, char *),
            read_afd_istat_db(int),
            read_afd_stat_db(int),
            save_old_input_year(int),
            save_old_output_year(int);
extern int  locate_dir(struct afdistat *, char *, int),
            locate_dir_year(struct afd_year_istat *, char *, int),
            locate_host(struct afdstat *, char *, int),
            locate_host_year(struct afd_year_stat *, char *, int);

#endif /* __statdefs_h */
