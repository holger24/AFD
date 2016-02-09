/*
 *  als.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   als - AFD log seach program
 **
 ** SYNOPSIS
 **   als [--version]
 **                 OR
 **   als [-w <AFD working directory>] [-f <search file pattern>] -t hostname 1..n
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   05.10.2007 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define _XOPEN_SOURCE
#include <time.h>
#include <locale.h>
#include <unistd.h>
#include <errno.h>
#include "mondefs.h"
#include "logdefs.h"
#include "version.h"

#ifndef VIEW_HOSTS
# define VIEW_HOSTS "view_hosts"
#endif

#define DEFAULT_SEARCH_FILE  "nin01-lm_1h[ABCD]_lm2_000_000-"
#define DEFAULT_SEARCH_START "Cos3"
#define DEFAULT_END_TARGET   "Ninjo"
#define DEFAULT_AFD_LOG_NO   1
#define NINJO_START_ID       "INFO: preparing file"
#define NINJO_END_ID         "INFO: finished file"

/* Global variables. */
int           afd_log_number,
              no_of_search_targets,
              sys_log_fd = STDERR_FILENO;
char          *p_work_dir,
              search_end_target[MAX_HOSTNAME_LENGTH + 1],
              **search_target,
              **search_alias_target;
FILE          **afd_fp,
              **ninjo_fp;
const char    *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void   get_data_afd(int, char *, time_t *),
              get_data_ninjo(int, char *, time_t *, time_t *),
              usage(char *);
static time_t get_ninjo_time(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ als $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   unsigned int *afd_ninjo_max_time,
                *afd_total_time,
                *afd_time_entries,
                *afd_max_time,
                *ninjo_total_time,
                *ninjo_time_entries,
                *ninjo_max_time,
                *ninjo_import_total_time,
                *ninjo_import_time_entries,
                *ninjo_import_max_time;
   int          alternate_only = YES,
                i;
   time_t       afd_output_time,
                input_time,
                ninjo_input_time,
                ninjo_output_time;
   off_t        filesize;
   char         filename[MAX_FILENAME_LENGTH + 1],
                flatfile[MAX_PATH_LENGTH],
                fullname[MAX_PATH_LENGTH],
                input_start[MAX_HOSTNAME_LENGTH + 1],
                line[MAX_LINE_LENGTH + 1],
                *ptr,
                search_file[MAX_FILENAME_LENGTH + 1],
                tmp_search_file[MAX_FILENAME_LENGTH + 1],
                time_str[11],
                tmp_time_str[11],
                work_dir[MAX_PATH_LENGTH];
   FILE         *fp,
                *fp_flat;

   CHECK_FOR_VERSION(argc, argv);

   if (argc > 1)
   {
      char str_log_number[MAX_INT_LENGTH];

      if (get_arg_array(&argc, argv, "-t", &search_target,
                        &no_of_search_targets) == INCORRECT)
      {
         no_of_search_targets = 0;
      }
      else
      {
         char *buffer = NULL,
              cmd[MAX_PATH_LENGTH];

         if (((afd_fp = malloc((no_of_search_targets + 1) * sizeof(FILE *))) == NULL) ||
             ((ninjo_fp = malloc(no_of_search_targets * sizeof(FILE *))) == NULL) ||
             ((afd_ninjo_max_time = malloc(no_of_search_targets * sizeof(unsigned int))) == NULL) ||
             ((afd_total_time = malloc(no_of_search_targets * sizeof(unsigned int))) == NULL) ||
             ((afd_time_entries = malloc(no_of_search_targets * sizeof(unsigned int))) == NULL) ||
             ((afd_max_time = malloc(no_of_search_targets * sizeof(unsigned int))) == NULL) ||
             ((ninjo_total_time = malloc(no_of_search_targets * sizeof(unsigned int))) == NULL) ||
             ((ninjo_time_entries = malloc(no_of_search_targets * sizeof(unsigned int))) == NULL) ||
             ((ninjo_max_time = malloc(no_of_search_targets * sizeof(unsigned int))) == NULL) ||
             ((ninjo_import_total_time = malloc(no_of_search_targets * sizeof(unsigned int))) == NULL) ||
             ((ninjo_import_time_entries = malloc(no_of_search_targets * sizeof(unsigned int))) == NULL) ||
             ((ninjo_import_max_time = malloc(no_of_search_targets * sizeof(unsigned int))) == NULL))
         {
            (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         RT_ARRAY(search_alias_target, no_of_search_targets,
                  (MAX_AFDNAME_LENGTH + 1), char);
         for (i = 0; i < no_of_search_targets; i++)
         {
            (void)sprintf(cmd, "%s -A %s", VIEW_HOSTS, search_target[i]);
            if (exec_cmd(cmd, &buffer, -1, NULL, 0, "", 0L, YES) == 0)
            {
               (void)strcpy(search_alias_target[i], buffer);
               ptr = search_alias_target[i];
               while ((*ptr != ' ') && (*ptr != '\n') && (*ptr != '\0'))
               {
                  ptr++;
               }
               *ptr = '\0';
            }
            else
            {
               search_alias_target[i][0] = '\0';
            }
         }
      }
      if (get_arg(&argc, argv, "-f", tmp_search_file,
                  MAX_FILENAME_LENGTH) == INCORRECT)
      {
         (void)sprintf(search_file, "* %s*", DEFAULT_SEARCH_FILE);
      }
      else
      {
         (void)sprintf(search_file, "* %s*", tmp_search_file);
      }
      if (get_arg(&argc, argv, "-i", input_start,
                  MAX_HOSTNAME_LENGTH) == INCORRECT)
      {
         (void)strcpy(input_start, DEFAULT_SEARCH_START);
      }
      if (get_arg(&argc, argv, "-e", search_end_target,
                  MAX_HOSTNAME_LENGTH) == INCORRECT)
      {
         (void)strcpy(search_end_target, DEFAULT_END_TARGET);
      }
      if (get_arg(&argc, argv, "-a", str_log_number,
                  MAX_INT_LENGTH) == INCORRECT)
      {
         afd_log_number = DEFAULT_AFD_LOG_NO;
      }
      else
      {
         afd_log_number = atoi(str_log_number);
      }
      if (get_arg(&argc, argv, "-nl", NULL, 0) == SUCCESS)
      {
         alternate_only = NO;
      }
      if (get_arg(&argc, argv, "-F", flatfile,
                  MAX_PATH_LENGTH) == INCORRECT)
      {
         flatfile[0] = '\0';
      }
   }
   else
   {
      usage(argv[0]);
      exit(INCORRECT);
   }

   for (i = 0; i < no_of_search_targets; i++)
   {
      afd_ninjo_max_time[i] = 0;
      afd_total_time[i] = 0;
      afd_time_entries[i] = 0;
      afd_max_time[i] = 0;
      ninjo_total_time[i] = 0;
      ninjo_time_entries[i] = 0;
      ninjo_max_time[i] = 0;
      ninjo_import_total_time[i] = 0;
      ninjo_import_time_entries[i] = 0;
      ninjo_import_max_time[i] = 0;
   }

   /* Needed since ninjo dates are given in german format. */
   setlocale(LC_TIME, "de_DE");

   p_work_dir = work_dir;
   if (get_afd_path(&argc, argv, p_work_dir) < 0)
   {
      (void)fprintf(stderr,
                    "Failed to get working directory of AFD. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (flatfile[0] != '\0')
   {
      if ((fp_flat = fopen(flatfile, "w")) == NULL)
      {
         (void)fprintf(stderr, "Failed to fopen() %s : %s (%s %d)\n",
                       flatfile, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   (void)sprintf(fullname, "%s%s/%s/%s%d",
                 p_work_dir, RLOG_DIR, input_start,
                 INPUT_BUFFER_FILE, afd_log_number);
   if ((fp = fopen(fullname, "r")) == NULL)
   {
      (void)fprintf(stderr, "Failed to fopen() %s : %s (%s %d)\n",
                    fullname, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   while (fgets(line, MAX_LINE_LENGTH, fp) != NULL)
   {
      if (pmatch(search_file, line, NULL) == 0)
      {
         input_time = (time_t)strtoull(line, NULL, 16);
         i = 0;
         while (line[i] != ' ')
         {
            i++;
         }
         while (line[i] == ' ')
         {
            i++;
         }
         ptr = &line[i];

         /* Store full file name. */
         i = 0;
         while (ptr[i] != SEPARATOR_CHAR)
         {
            filename[i] = ptr[i];
            i++;
         }
         filename[i] = '\0';
         if ((i > 2) && ((filename[i - 1] == '0') ||
             (filename[i - 1] == '1')) && (filename[i - 2] == '_'))
         {
            filename[i - 2] = '\0';
         }
         else
         {
            if (alternate_only == YES)
            {
               continue;
            }
         }

         /* Store file size. */
         ptr = &ptr[i + 1];
         i = 0;
         while (ptr[i] != SEPARATOR_CHAR)
         {
            i++;
         }
         ptr[i] = '\0';
         filesize = (off_t)strtoull(ptr, NULL, 16);

         (void)strftime(time_str, 10, "%H:%M:%S", gmtime(&input_time));
         if (fp_flat != NULL)
         {
            (void)strcpy(tmp_time_str, time_str);
         }
         (void)fprintf(stdout, "\n%s %s %lld Bytes\n",
                       time_str, filename, (pri_off_t)filesize);

         /* Search each output target for the data. */
         for (i = 0; i < no_of_search_targets; i++)
         {
            if (fp_flat != NULL)
            {
               (void)fprintf(fp_flat, "%s %s %lld ",
                             tmp_time_str, filename, (pri_off_t)filesize);
            }
            get_data_afd(i, filename, &afd_output_time);
            if (afd_output_time == 0L)
            {
               (void)fprintf(stdout, "%10s No AFD output data found\n",
                             search_target[i]);
               if (fp_flat != NULL)
               {
                  (void)fprintf(fp_flat, "%10s No AFD output data found\n",
                                search_target[i]);
               }
            }
            else
            {
               afd_total_time[i] += (unsigned int)(afd_output_time - input_time);
               if ((unsigned int)(afd_output_time - input_time) > afd_max_time[i])
               {
                  afd_max_time[i] = (unsigned int)(afd_output_time - input_time);
               }
               afd_time_entries[i] += 1;
               (void)strftime(time_str, 10, "%H:%M:%S", gmtime(&afd_output_time));
               (void)fprintf(stdout, "%10s %s ", search_target[i], time_str);
               if (fp_flat != NULL)
               {
                  (void)fprintf(fp_flat, "%10s %s ",
                                search_target[i], time_str);
               }
               get_data_ninjo(i, filename, &ninjo_input_time,
                              &ninjo_output_time);
               if (ninjo_input_time == 0L)
               {
                  (void)fprintf(stdout, "No NINJO input data found\n");
                  if (fp_flat != NULL)
                  {
                     (void)fprintf(fp_flat, "No NINJO input data found\n");
                  }
               }
               else
               {
                  if ((unsigned int)(ninjo_input_time - input_time) > afd_ninjo_max_time[i])
                  {
                     afd_ninjo_max_time[i] = (unsigned int)(ninjo_input_time - input_time);
                  }
                  (void)strftime(time_str, 10, "%H:%M:%S",
                                 gmtime(&ninjo_input_time));
                  (void)fprintf(stdout, "%s ", time_str);
                  if (fp_flat != NULL)
                  {
                     (void)fprintf(fp_flat, "%s ", time_str);
                  }
                  if (ninjo_output_time > 0L)
                  {
                     ninjo_total_time[i] += (unsigned int)(ninjo_output_time - afd_output_time);
                     if ((unsigned int)(ninjo_output_time - afd_output_time) > ninjo_max_time[i])
                     {
                        ninjo_max_time[i] = (unsigned int)(ninjo_output_time - afd_output_time);
                     }
                     ninjo_time_entries[i] += 1;
                     ninjo_import_total_time[i] += (unsigned int)(ninjo_output_time - ninjo_input_time);
                     if ((unsigned int)(ninjo_output_time - ninjo_input_time) > ninjo_import_max_time[i])
                     {
                        ninjo_import_max_time[i] = (unsigned int)(ninjo_output_time - ninjo_input_time);
                     }
                     ninjo_import_time_entries[i] += 1;
                     (void)strftime(time_str, 10, "%H:%M:%S",
                                    gmtime(&ninjo_output_time));
                     (void)fprintf(stdout, "%s %6d %10d %6d\n",
                                   time_str, (int)(afd_output_time - input_time),
                                   (int)(ninjo_output_time - afd_output_time),
                                   (int)(ninjo_output_time - ninjo_input_time));
                     if (fp_flat != NULL)
                     {
                        (void)fprintf(fp_flat, "%s %6d %10d %6d\n",
                                      time_str, (int)(afd_output_time - input_time),
                                      (int)(ninjo_output_time - afd_output_time),
                                      (int)(ninjo_output_time - ninjo_input_time));
                     }
                  }
                  else
                  {
                     ninjo_total_time[i] += (unsigned int)(ninjo_input_time - afd_output_time);
                     if ((unsigned int)(ninjo_input_time - afd_output_time) > ninjo_max_time[i])
                     {
                        ninjo_max_time[i] = (unsigned int)(ninjo_input_time - afd_output_time);
                     }
                     ninjo_time_entries[i] += 1;
                     (void)fprintf(stdout, "??:??:?? %6d %10d*\n",
                                   (int)(afd_output_time - input_time),
                                   (int)(ninjo_input_time - afd_output_time));
                     if (fp_flat != NULL)
                     {
                        (void)fprintf(fp_flat, "??:??:?? %6d %10d*\n",
                                      (int)(afd_output_time - input_time),
                                      (int)(ninjo_input_time - afd_output_time));
                     }
                  }
               }
            }
         }
      }
   }

#ifdef OLD_STYLE
   (void)fprintf(stdout, "\n           AFD                    |     NINJO\n");
   (void)fprintf(stdout, "              avr    max  entries |       avr        max  entries    avr    max  entries\n");
   for (i = 0; i < no_of_search_targets; i++)
   {
      if (ninjo_import_time_entries[i] == 0)
      {
         ninjo_import_time_entries[i] = 1;
      }
      (void)fprintf(stdout,
                    "%10s %6d %6d (%6d) |%10d %10d (%6d) %6d %6d (%6d)\n",
                    search_target[i], afd_total_time[i] / afd_time_entries[i],
                    afd_max_time[i], afd_time_entries[i],
                    ninjo_total_time[i] / ninjo_time_entries[i],
                    ninjo_max_time[i], ninjo_time_entries[i],
                    ninjo_import_total_time[i] / ninjo_import_time_entries[i],
                    ninjo_import_max_time[i], ninjo_import_time_entries[i]);
   }
#else
   (void)fprintf(stdout, "\n           +---------------------+--------------------+----------------------------+--------------------+\n");
   (void)fprintf(stdout, "           |     Total time      |         AFD        |      Ninjo wait import     |    Ninjo import    |\n");
   (void)fprintf(stdout, "+----------+----------+----------+------+------+------+----------+----------+------+------+------+------+\n");
   (void)fprintf(stdout, "|    Server|       avr|       max|   avr|   max| files|       avr|       max| files|   avr|   max| files|\n");
   (void)fprintf(stdout, "+----------+----------+----------+------+------+------+----------+----------+------+------+------+------+\n");
   for (i = 0; i < no_of_search_targets; i++)
   {
      if (ninjo_import_time_entries[i] == 0)
      {
         ninjo_import_time_entries[i] = 1;
      }
      (void)fprintf(stdout,
                    "|%10s|%10d|%10d|%6d|%6d|%6d|%10d|%10d|%6d|%6d|%6d|%6d|\n",
                    search_target[i],
                    (afd_total_time[i] / afd_time_entries[i]) +
                    (ninjo_total_time[i] / ninjo_time_entries[i]),
                    afd_ninjo_max_time[i],
                    afd_total_time[i] / afd_time_entries[i],
                    afd_max_time[i], afd_time_entries[i],
                    (ninjo_total_time[i] - ninjo_import_total_time[i]) / ninjo_time_entries[i],
                    ninjo_max_time[i], ninjo_time_entries[i],
                    ninjo_import_total_time[i] / ninjo_import_time_entries[i],
                    ninjo_import_max_time[i], ninjo_import_time_entries[i]);
   }
   (void)fprintf(stdout, "+----------+----------+----------+------+------+------+----------+----------+------+------+------+------+\n");
#endif

   if (fp_flat != NULL)
   {
      (void)fclose(fp_flat);
   }

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++ get_data_afd() ++++++++++++++++++++++++++++*/
static void
get_data_afd(int spos, char *filename, time_t *afd_output_time)
{
   int  special_case_helena;
   char line[MAX_LINE_LENGTH + 1],
        search_str[MAX_HOSTNAME_LENGTH + 1 + 1 + 1 + MAX_FILENAME_LENGTH + 1 + 1];

   *afd_output_time = 0L;
   if ((search_target[spos][0] == 'h') && (search_target[spos][1] == 'e') &&
       (search_target[spos][2] == 'l') && (search_target[spos][3] == 'e') &&
       (search_target[spos][4] == 'n') && (search_target[spos][5] == 'a') &&
       (search_target[spos][6] == '\0'))
   {
      (void)sprintf(search_str, "* %-*s ?%c%s%c*",
                    MAX_HOSTNAME_LENGTH, "Ni?-hele",
                    SEPARATOR_CHAR, filename, SEPARATOR_CHAR);
      special_case_helena = YES;
   }
   else
   {
      (void)sprintf(search_str, "* %-*s ?%c%s%c*",
                    MAX_HOSTNAME_LENGTH, search_end_target,
                    SEPARATOR_CHAR, filename, SEPARATOR_CHAR);
      special_case_helena = NO;
   }
   if (afd_fp[spos] == NULL)
   {
      char fullname[MAX_PATH_LENGTH];

      (void)sprintf(fullname, "%s%s/%s/%s%d.als",
                    p_work_dir, RLOG_DIR, search_alias_target[spos],
                    OUTPUT_BUFFER_FILE, afd_log_number);
      if ((afd_fp[spos] = fopen(fullname, "r")) == NULL)
      {
         (void)fprintf(stderr, "Failed to fopen() %s : %s (%s %d)\n",
                       fullname, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   else
   {
      while (fgets(line, MAX_LINE_LENGTH, afd_fp[spos]) != NULL)
      {
         if (pmatch(search_str, line, NULL) == 0)
         {
            *afd_output_time = (time_t)strtoull(line, NULL, 16);
            return;
         }
      }
      rewind(afd_fp[spos]);
   }

   while (fgets(line, MAX_LINE_LENGTH, afd_fp[spos]) != NULL)
   {
      if (pmatch(search_str, line, NULL) == 0)
      {
         *afd_output_time = (time_t)strtoull(line, NULL, 16);
         break;
      }
   }

   if (special_case_helena == YES)
   {
      if (afd_fp[no_of_search_targets] == NULL)
      {
         char fullname[MAX_PATH_LENGTH];

         (void)sprintf(fullname, "%s%s/%s/%s%d.als",
                       p_work_dir, RLOG_DIR, "AFDZ-athena",
                       OUTPUT_BUFFER_FILE, afd_log_number);
         if ((afd_fp[no_of_search_targets] = fopen(fullname, "r")) == NULL)
         {
            (void)fprintf(stderr, "Failed to fopen() %s : %s (%s %d)\n",
                          fullname, strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }
      else
      {
         while (fgets(line, MAX_LINE_LENGTH, afd_fp[no_of_search_targets]) != NULL)
         {
            if (pmatch(search_str, line, NULL) == 0)
            {
               *afd_output_time = (time_t)strtoull(line, NULL, 16);
               return;
            }
         }
         rewind(afd_fp[no_of_search_targets]);
      }

      while (fgets(line, MAX_LINE_LENGTH, afd_fp[no_of_search_targets]) != NULL)
      {
         if (pmatch(search_str, line, NULL) == 0)
         {
            *afd_output_time = (time_t)strtoull(line, NULL, 16);
            break;
         }
      }
   }

   return;
}


/*++++++++++++++++++++++++++ get_data_ninjo() +++++++++++++++++++++++++++*/
static void
get_data_ninjo(int spos,
               char *filename,
               time_t *ninjo_input_time,
               time_t *ninjo_output_time)
{
   char line[MAX_LINE_LENGTH + 1];

   *ninjo_input_time = *ninjo_output_time = 0L;
   if (ninjo_fp[spos] == NULL)
   {
      char fullname[MAX_PATH_LENGTH];

      (void)sprintf(fullname, "%s/ninjolog/%s/log",
                    p_work_dir, search_target[spos]);
      if ((ninjo_fp[spos] = fopen(fullname, "r")) == NULL)
      {
         (void)fprintf(stderr, "Failed to fopen() %s : %s (%s %d)\n",
                       fullname, strerror(errno), __FILE__, __LINE__);
         return;
      }
   }
   else
   {
      while (fgets(line, MAX_LINE_LENGTH, ninjo_fp[spos]) != NULL)
      {
         if (posi(line, filename) != NULL)
         {
            if (*ninjo_input_time == 0L)
            {
               if (posi(line, NINJO_START_ID) != NULL)
               {
                  *ninjo_input_time = get_ninjo_time(line);
               }
            }
            else
            {
               if (posi(line, NINJO_END_ID) != NULL)
               {
                  *ninjo_output_time = get_ninjo_time(line);
                  return;
               }
            }
         }
      }
      rewind(ninjo_fp[spos]);
   }

   while (fgets(line, MAX_LINE_LENGTH, ninjo_fp[spos]) != NULL)
   {
      if (posi(line, filename) != NULL)
      {
         if (*ninjo_input_time == 0L)
         {
            if (posi(line, NINJO_START_ID) != NULL)
            {
               *ninjo_input_time = get_ninjo_time(line);
            }
         }
         else
         {
            if (posi(line, NINJO_END_ID) != NULL)
            {
               *ninjo_output_time = get_ninjo_time(line);
               break;
            }
         }
      }
   }

   return;
}


/*--------------------------- get_ninjo_time() --------------------------*/
static time_t
get_ninjo_time(char *line)
{
   struct tm tm;

   (void)strptime(line, "%d %b %Y %H:%M:%S", &tm);

   return(mktime(&tm));
}


/*++++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{
   (void)fprintf(stdout,
                 "Usage: %s [-w <AFD working directory>] [-f <search file pattern>] -t hostname 1..n\n",
                 progname);
   return;
}
