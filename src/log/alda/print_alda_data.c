/*
 *  print_alda_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 - 2025 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   print_alda_data - prints alda output data
 **
 ** SYNOPSIS
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.02.2008 H.Kiehl Created
 **   09.03.2010 H.Kiehl Handle case when user wants to print a % sign.
 **   14.09.2017 H.Kiehl Added format parameter %A to print information
 **                      about remote AFD.
 **   11.02.2019 H.Kiehl Function pri_string() can now print only certain
 **                      characters from the given string.
 **   18.03.2025 H.Kiehl Add header line support with additional
 **                      information about the node.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>      /* strlen()                                    */
#include <stdlib.h>      /* atoi()                                      */
#include <ctype.h>       /* isdigit()                                   */
#include <time.h>        /* strftime()                                  */
#include <unistd.h>      /* STDERR_FILENO                               */
#include <errno.h>
#include "aldadefs.h"
#ifdef _DELETE_LOG
# include "dr_str.h"
#endif
#ifdef _OUTPUT_LOG
# include "ot_str.h"
#endif

/* External global variables. */
extern unsigned int      mode;
extern int               data_printed,
                         header_line_shown,
                         no_of_header_lines;
extern off_t             log_data_written;
extern char              *format_str,
                         header_filename[],
                         **header_line,
                         output_filename[];
extern FILE              *output_fp;
extern struct afd_info   afd;
#ifdef _INPUT_LOG
extern struct alda_idata ilog;
#endif
#ifdef _DISTRIBUTION_LOG
extern struct alda_udata ulog;
#endif
#ifdef _PRODUCTION_LOG
extern struct alda_pdata plog;
#endif
#ifdef _OUTPUT_LOG
extern struct alda_odata olog;
#endif
#ifdef _DELETE_LOG
extern struct alda_ddata dlog;
#endif

/* Local global variables. */
static char              output_line[MAX_OUTPUT_LINE_LENGTH + 1];

/* Local function prototypes. */
#if defined (_PRODUCTION_LOG) || defined (_OUTPUT_LOG)
static int               pri_duration(char *, int, int, int, char, double, int);
#endif
static int               pri_id(char *, int, int, int, unsigned int, int),
                         pri_int(char *, int, int, int, int, int),
#ifdef _DISTRIBUTION_LOG
                         pri_int_array(char *, int, int, int, int,
                                       unsigned int *, char, int),
                         pri_int_char_array(char *, int, int, int, int,
                                            unsigned char *, char, int),
#endif
                         pri_size(char *, int, int, char, char, off_t, int),
                         pri_string(int, char *, int, const char *, int, int),
                         pri_time(char *, int, int, char, char, time_t,
                                  struct tm *, int);


/*########################## print_alda_data() ##########################*/
void
print_alda_data(void)
{
   int  fo_pos,
        i = 0,
        j;
   char format_orientation[MAX_FORMAT_ORIENTATION_LENGTH],
        *p_char_selection,
        *p_start,
        *ptr = format_str,
        str_number[MAX_INT_LENGTH + 1];

   if ((header_line != NULL) && (header_line_shown == NO))
   {
      char *p_percent;

      for (j = 0; j < no_of_header_lines; j++)
      {
         p_percent = header_line[j];
         while (*p_percent != '\0')
         {
            if (*p_percent == '\\')
            {
               p_percent++;
            }
            else
            {
               if (*p_percent == '%')
               {
                  if (*(p_percent + 1) == 'I')
                  {
                     int fd;

                     if ((fd = fileno(output_fp)) == -1)
                     {
                        (void)fprintf(stderr, "fileno() error: %s (%s %d)\n",
                                      strerror(errno), __FILE__, __LINE__);
                        if (fputc((int)*p_percent, output_fp) == EOF)
                        {
                           (void)fprintf(stderr,
                                         "fputc()  error: %s (%s %d)\n",
                                         strerror(errno), __FILE__, __LINE__);
                           return;
                        }
                        p_percent++;
                        if (fputc((int)*p_percent, output_fp) == EOF)
                        {
                           (void)fprintf(stderr,
                                         "fputc()  error: %s (%s %d)\n",
                                         strerror(errno), __FILE__, __LINE__);
                           return;
                        }
                        p_percent++;
                     }
                     else
                     {
                        struct stat stat_buf;

                        if (fstat(fd, &stat_buf) == -1)
                        {
                           (void)fprintf(stderr, "fstat() error: %s (%s %d)\n",
                                         strerror(errno), __FILE__, __LINE__);
                           if (fputc((int)*p_percent, output_fp) == EOF)
                           {
                              (void)fprintf(stderr,
                                            "fputc()  error: %s (%s %d)\n",
                                            strerror(errno),
                                            __FILE__, __LINE__);
                              return;
                           }
                           p_percent++;
                           if (fputc((int)*p_percent, output_fp) == EOF)
                           {
                              (void)fprintf(stderr,
                                            "fputc()  error: %s (%s %d)\n",
                                            strerror(errno),
                                            __FILE__, __LINE__);
                              return;
                           }
                           p_percent++;
                        }
                        else
                        {
                           log_data_written += (off_t)fprintf(output_fp,
#if SIZEOF_INO_T == 4
                                                              "%ld",
#else
                                                              "%lld",
#endif
                                                              (pri_ino_t)stat_buf.st_ino);
                           p_percent += 2;
                        }
                     }
                  }
#ifdef HAVE_GETHOSTID
                  else if (*(p_percent + 1) == 'H')
                       {
                          long hostid;

                          if ((hostid = gethostid()) == -1)
                          {
                             (void)fprintf(stderr,
                                           "gethostid() error: %s (%s %d)\n",
                                           strerror(errno),
                                           __FILE__, __LINE__);
                             if (fputc((int)*p_percent, output_fp) == EOF)
                             {
                                (void)fprintf(stderr,
                                              "fputc()  error: %s (%s %d)\n",
                                              strerror(errno),
                                              __FILE__, __LINE__);
                                return;
                             }
                             p_percent++;
                             if (fputc((int)*p_percent, output_fp) == EOF)
                             {
                                (void)fprintf(stderr,
                                              "fputc()  error: %s (%s %d)\n",
                                              strerror(errno),
                                              __FILE__, __LINE__);
                                return;
                             }
                             p_percent++;
                          }
                          else
                          {
                             log_data_written += (off_t)fprintf(output_fp,
                                                                "%x",
                                                                (unsigned int)hostid);
                             p_percent += 2;
                          }
                       }
#endif
                       else
                       {
                           if (fputc((int)*p_percent, output_fp) == EOF)
                           {
                              (void)fprintf(stderr,
                                            "fputc()  error: %s (%s %d)\n",
                                            strerror(errno),
                                            __FILE__, __LINE__);
                              return;
                           }
                           p_percent++;
                       }
               }
               else
               {
                  if (fputc((int)*p_percent, output_fp) == EOF)
                  {
                     (void)fprintf(stderr, "fputc()  error: %s (%s %d)\n",
                                   strerror(errno), __FILE__, __LINE__);
                     return;
                  }
                  p_percent++;
               }
            }
         }
         log_data_written += (off_t)fprintf(output_fp, "\n");
      }
      header_line_shown = YES;
   }

   if (header_filename[0] != '\0')
   {
      show_file_content(output_fp, header_filename);
      header_filename[0] = '\0';
   }

   format_orientation[0] = '%';
   do
   {
      if ((*ptr == '\\') && (*(ptr + 1) == '%'))
      {
         ptr += 2;
         output_line[i] = '%';
         i++;
      }
      else
      {
         if ((*ptr == '\\') && (*(ptr + 1) != 'n') && (*(ptr + 1) != 't'))
         {
            ptr++;
         }
         if (*ptr == '%')
         {
            int  max_length,
                 right_side;
            char base_char;

            p_start = ptr;
            fo_pos = 1;
            ptr++;
            if (*ptr == '-')
            {
               format_orientation[fo_pos] = *ptr;
               ptr++; fo_pos++;
               right_side = NO;
            }
            else
            {
               right_side = YES;
            }
            if (*ptr == '[')
            {
               ptr++;
               p_char_selection = ptr;
               while ((*ptr != ']') &&
                      ((isdigit((int)(*ptr))) ||
                       (*ptr == ',') || (*ptr == '-') || (*ptr == '$')))
               {
                  ptr++;
               }
               if (*ptr != ']')
               {
                  p_char_selection = NULL;
               }
               else
               {
                  ptr++;
               }
            }
            else
            {
               p_char_selection = NULL;
            }
            j = 0;
            while ((isdigit((int)(*(ptr + j)))) && (j < MAX_INT_LENGTH))
            {
               format_orientation[fo_pos + j] = *(ptr + j);
               str_number[j] = *(ptr + j);
               j++;
            }
            if (j > 0)
            {
               str_number[j] = '\0';
               max_length = atoi(str_number);
               ptr += j;
               fo_pos += j;
            }
            else
            {
               max_length = 0;
            }
            if (j == MAX_INT_LENGTH)
            {
               while (isdigit((int)(*ptr)))
               {
                  ptr++;
               }
               (void)fprintf(stderr, "Length indicator to long. (%s %d)\n",
                             __FILE__, __LINE__);
            }
            if (*ptr == '.')
            {
               format_orientation[fo_pos] = '.';
               ptr++;
               fo_pos++;
               j = 0;
               while ((isdigit((int)(*(ptr + j)))) && (j < MAX_INT_LENGTH))
               {
                  format_orientation[fo_pos + j] = *(ptr + j);
                  j++;
               }
               ptr += j;
               fo_pos += j;
               if (j == MAX_INT_LENGTH)
               {
                  while (isdigit((int)(*ptr)))
                  {
                     ptr++;
                  }
                  (void)fprintf(stderr, "Length indicator to long. (%s %d)\n",
                                __FILE__, __LINE__);
               }
               if ((*ptr == 'd') || (*ptr == 'x') || (*ptr == 'o'))
               {
                  ptr++;
               }
               base_char = 'f';
            }
            else
            {
               if ((*ptr == 'd') || (*ptr == 'x') || (*ptr == 'o'))
               {
                  base_char = *ptr;
                  ptr++;
               }
               else
               {
                  base_char = DEFAULT_BASE_CHAR;
               }
            }
            switch (*ptr)
            {
#ifdef _INPUT_LOG
               case 'I' : /* Input data. */
                  switch (*(ptr + 1))
                  {
                     case 'T' : /* Input time. */
                        if ((j = pri_time(format_orientation, fo_pos,
                                          max_length, base_char, *(ptr + 2),
                                          ilog.input_time,
                                          &ilog.bd_input_time, i)) == -1)
                        {
                           j = ptr + 2 - p_start;
                           (void)memcpy(&output_line[i], p_start, j);
                        }
                        ptr += 2;
                        i += j;
                        break;

                     case 'F' : /* Input file name. */
                        i += pri_string(right_side, p_char_selection,
                                        max_length, ilog.filename,
                                        ilog.filename_length, i);
                        ptr++;
                        break;

                     case 'S' : /* Input file size. */
                        if ((j = pri_size(format_orientation, fo_pos,
                                          max_length, base_char, *(ptr + 2),
                                          ilog.file_size, i)) == -1)
                        {
                           j = ptr + 2 - p_start;
                           (void)memcpy(&output_line[i], p_start, j);
                        }
                        ptr += 2;
                        i += j;
                        break;

                     case 'I' : /* Input source ID. */
                        i += pri_id(format_orientation, fo_pos, max_length,
                                    base_char, ilog.dir_id, i);
                        ptr++;
                        break;

# if defined (_INPUT_LOG) || defined (_DISTRIBUTION_LOG)
                     case 'N' : /* Full source name. */
#  ifdef _INPUT_LOG
                        if (ilog.full_source[0] == '\0')
                        {
                           get_full_source(ilog.dir_id, ilog.full_source,
                                           &ilog.full_source_length);
                        }
                        i += pri_string(right_side, p_char_selection,
                                        max_length, ilog.full_source,
                                        ilog.full_source_length, i);
#  else
                        if (ulog.full_source[0] == '\0')
                        {
                           get_full_source(ulog.dir_id, ulog.full_source,
                                           &ulog.full_source_length);
                        }
                        i += pri_string(right_side, p_char_selection,
                                        max_length, ilog.full_source,
                                        ulog.full_source_length, i);
#  endif
                        ptr++;
                        break;
# endif

                     case 'U' : /* Unique number. */
                        i += pri_id(format_orientation, fo_pos, max_length,
                                    base_char, ilog.unique_number, i);
                        ptr++;
                        break;

                     default  : /* Unknown, lets print this as supplied. */
                        ptr++;
                        j = ptr - p_start;
                        (void)memcpy(&output_line[i], p_start, j);
                        i += j;
                        break;
                  }
                  break;
#endif /* _INPUT_LOG */
#ifdef _DISTRIBUTION_LOG
               case 'U' : /* Distribution data. */
                  switch (*(ptr + 1))
                  {
                     case 'T' : /* Input time. */
                        if ((j = pri_time(format_orientation, fo_pos,
                                          max_length, base_char, *(ptr + 2),
                                          ulog.input_time,
                                          &ulog.bd_input_time, i)) == -1)
                        {
                           j = ptr + 2 - p_start;
                           (void)memcpy(&output_line[i], p_start, j);
                        }
                        ptr += 2;
                        i += j;
                        break;

                     case 't' : /* Distribution time. */
                        if ((j = pri_time(format_orientation, fo_pos,
                                          max_length, base_char, *(ptr + 2),
                                          ulog.distribution_time,
                                          &ulog.bd_distribution_time, i)) == -1)
                        {
                           j = ptr + 2 - p_start;
                           (void)memcpy(&output_line[i], p_start, j);
                        }
                        ptr += 2;
                        i += j;
                        break;

                     case 'F' : /* Input file name. */
                        i += pri_string(right_side, p_char_selection,
                                        max_length, ulog.filename,
                                        ulog.filename_length, i);
                        ptr++;
                        break;

                     case 'S' : /* Input file size. */
                        if ((j = pri_size(format_orientation, fo_pos,
                                          max_length, base_char, *(ptr + 2),
                                          ulog.file_size, i)) == -1)
                        {
                           j = ptr + 2 - p_start;
                           (void)memcpy(&output_line[i], p_start, j);
                        }
                        ptr += 2;
                        i += j;
                        break;

                     case 'I' : /* Input source ID. */
                        i += pri_id(format_orientation, fo_pos, max_length,
                                    base_char, ulog.dir_id, i);
                        ptr++;
                        break;

                     case 'U' : /* Unique number. */
                        i += pri_id(format_orientation, fo_pos, max_length,
                                    base_char, ulog.unique_number, i);
                        ptr++;
                        break;

                     case 'n' : /* Number of jobs distributed. */
                        i += pri_int(format_orientation, fo_pos, max_length,
                                     base_char, ulog.no_of_dist_jobs, i);
                        ptr++;
                        break;

                     case 'j' : /* List of job ID's distributed. */
                        i += pri_int_array(format_orientation, fo_pos,
                                           max_length, base_char,
                                           ulog.no_of_dist_jobs,
                                           ulog.job_id_list, *(ptr + 2), i);
                        ptr += 2;
                        break;

                     case 'c' : /* List of number of pre-processing. */
                        i += pri_int_char_array(format_orientation, fo_pos,
                                                max_length, base_char,
                                                ulog.no_of_dist_jobs,
                                                ulog.proc_cycles, *(ptr + 2), i);
                        ptr += 2;
                        break;

                     case 'Y' : /* Distribution type. */
                        i += pri_int(format_orientation, fo_pos, max_length,
                                     base_char, (int)ulog.distribution_type, i);
                        ptr++;
                        break;

                     default  : /* Unknown, lets print this as supplied. */
                        ptr++;
                        j = ptr - p_start;
                        (void)memcpy(&output_line[i], p_start, j);
                        i += j;
                        break;
                  }
                  break;
#endif /* _DISTRIBUTION_LOG */
#ifdef _PRODUCTION_LOG
               case 'P' : /* Production data. */
                  switch (*(ptr + 1))
                  {
                     case 't' : /* Time when production starts. */
                        if ((j = pri_time(format_orientation, fo_pos,
                                          max_length, base_char, *(ptr + 2),
                                          plog.input_time,
                                          &plog.bd_input_time, i)) == -1)
                        {
                           j = ptr + 2 - p_start;
                           (void)memcpy(&output_line[i], p_start, j);
                        }
                        ptr += 2;
                        i += j;
                        break;

                     case 'T' : /* Time when production finished. */
                        if ((j = pri_time(format_orientation, fo_pos,
                                          max_length, base_char, *(ptr + 2),
                                          plog.output_time,
                                          &plog.bd_output_time, i)) == -1)
                        {
                           j = ptr + 2 - p_start;
                           (void)memcpy(&output_line[i], p_start, j);
                        }
                        ptr += 2;
                        i += j;
                        break;

                     case 'D' : /* Production time. */
                        if ((j = pri_duration(format_orientation, fo_pos,
                                              max_length, base_char, *(ptr + 2),
                                              plog.production_time, i)) == -1)
                        {
                           j = ptr + 2 - p_start;
                           (void)memcpy(&output_line[i], p_start, j);
                        }
                        ptr += 2;
                        i += j;
                        break;

                     case 'u' : /* CPU usage. */
                        if ((j = pri_duration(format_orientation, fo_pos,
                                              max_length, base_char, *(ptr + 2),
                                              plog.cpu_time, i)) == -1)
                        {
                           j = ptr + 2 - p_start;
                           (void)memcpy(&output_line[i], p_start, j);
                        }
                        ptr += 2;
                        i += j;
                        break;

                     case 'b' : /* Ratio relationship 1. */
                        i += pri_id(format_orientation, fo_pos, max_length,
                                    base_char, plog.ratio_1, i);
                        ptr++;
                        break;

                     case 'B' : /* Ratio relationship 2. */
                        i += pri_id(format_orientation, fo_pos, max_length,
                                    base_char, plog.ratio_2, i);
                        ptr++;
                        break;

                     case 'J' : /* Job ID. */
                        i += pri_id(format_orientation, fo_pos, max_length,
                                    base_char, plog.job_id, i);
                        ptr++;
                        break;

                     case 'Z' : /* Job creation time. */
                        if ((j = pri_time(format_orientation, fo_pos,
                                          max_length, base_char, *(ptr + 2),
                                          plog.input_time,
                                          &plog.bd_input_time, i)) == -1)
                        {
                           j = ptr + 2 - p_start;
                           (void)memcpy(&output_line[i], p_start, j);
                        }
                        ptr += 2;
                        i += j;
                        break;

                     case 'U' : /* Unique number. */
                        i += pri_id(format_orientation, fo_pos, max_length,
                                    base_char, plog.unique_number, i);
                        ptr++;
                        break;

                     case 'L' : /* Split job number. */
                        i += pri_id(format_orientation, fo_pos, max_length,
                                    base_char, plog.split_job_counter, i);
                        ptr++;
                        break;

                     case 'f' : /* Input file name. */
                        i += pri_string(right_side, p_char_selection,
                                        max_length, plog.original_filename,
                                        plog.original_filename_length, i);
                        ptr++;
                        break;

                     case 'F' : /* Produced file name. */
                        i += pri_string(right_side, p_char_selection,
                                        max_length, plog.new_filename,
                                        plog.new_filename_length, i);
                        ptr++;
                        break;

                     case 's' : /* Original file size. */
                        if ((j = pri_size(format_orientation, fo_pos,
                                          max_length, base_char, *(ptr + 2),
                                          plog.original_file_size, i)) == -1)
                        {
                           j = ptr + 2 - p_start;
                           (void)memcpy(&output_line[i], p_start, j);
                        }
                        ptr += 2;
                        i += j;
                        break;

                     case 'S' : /* Produced file size. */
                        if ((j = pri_size(format_orientation, fo_pos,
                                          max_length, base_char, *(ptr + 2),
                                          plog.new_file_size, i)) == -1)
                        {
                           j = ptr + 2 - p_start;
                           (void)memcpy(&output_line[i], p_start, j);
                        }
                        ptr += 2;
                        i += j;
                        break;

                     case 'C' : /* Command execueted. */
                        i += pri_string(right_side, p_char_selection,
                                        max_length, plog.what_done,
                                        plog.what_done_length, i);
                        ptr++;
                        break;

                     case 'R' : /* Return code of command executed. */
                        i += pri_int(format_orientation, fo_pos, max_length,
                                     base_char, plog.return_code, i);
                        ptr++;
                        break;

                     default  : /* Unknown, lets print this as supplied. */
                        ptr++;
                        j = ptr - p_start;
                        (void)memcpy(&output_line[i], p_start, j);
                        i += j;
                        break;
                  }
                  break;
#endif /* _PRODUCTION_LOG */
#ifdef _OUTPUT_LOG
               case 'O' : /* Output data. */
                  switch (*(ptr + 1))
                  {
                     case 't' : /* Time when sending starts. */
                        if ((j = pri_time(format_orientation, fo_pos,
                                          max_length, base_char, *(ptr + 2),
                                          olog.send_start_time,
                                          &olog.bd_send_start_time, i)) == -1)
                        {
                           j = ptr + 2 - p_start;
                           (void)memcpy(&output_line[i], p_start, j);
                        }
                        ptr += 2;
                        i += j;
                        break;

                     case 'T' : /* Time when file is transmitted. */
                        if ((j = pri_time(format_orientation, fo_pos,
                                          max_length, base_char, *(ptr + 2),
                                          olog.output_time,
                                          &olog.bd_output_time, i)) == -1)
                        {
                           j = ptr + 2 - p_start;
                           (void)memcpy(&output_line[i], p_start, j);
                        }
                        ptr += 2;
                        i += j;
                        break;

                     case 'D' : /* Time taken to transmit file. */
                        if ((j = pri_duration(format_orientation, fo_pos,
                                              max_length, base_char, *(ptr + 2),
                                              olog.transmission_time, i)) == -1)
                        {
                           j = ptr + 2 - p_start;
                           (void)memcpy(&output_line[i], p_start, j);
                        }
                        ptr += 2;
                        i += j;
                        break;

                     case 'f' : /* Local output file name. */
                        i += pri_string(right_side, p_char_selection,
                                        max_length, olog.local_filename,
                                        olog.local_filename_length, i);
                        ptr++;
                        break;

                     case 'F' : /* Remote output file name/directory. */
                        i += pri_string(right_side, p_char_selection,
                                        max_length, olog.remote_name,
                                        olog.remote_name_length, i);
                        ptr++;
                        break;

                     case 'M' : /* Mail Queue ID. */
                        i += pri_string(right_side, p_char_selection,
                                        max_length, olog.mail_id,
                                        olog.mail_id_length, i);
                        ptr++;
                        break;

                     case 'E' : /* Final output file name/directory. */
                        if (olog.remote_name[0] == '\0')
                        {
                           i += pri_string(right_side, p_char_selection,
                                           max_length, olog.local_filename,
                                           olog.local_filename_length, i);
                        }
                        else
                        {
                           i += pri_string(right_side, p_char_selection,
                                           max_length, olog.remote_name,
                                           olog.remote_name_length, i);
                        }
                        ptr++;
                        break;

                     case 'P' : /* Protocol used for transmission. */
                        if (olog.output_time == -1)
                        {
                           i += pri_string(right_side, p_char_selection,
                                           max_length, "", 0, i);
                        }
                        else
                        {
                           switch (olog.protocol)
                           {
                              case ALDA_FTP_FLAG :
                                 i += pri_string(right_side, p_char_selection,
                                                 max_length, ALDA_FTP_SHEME,
                                                 ALDA_FTP_SHEME_LENGTH, i);
                                 break;

                              case ALDA_LOC_FLAG :
                                 i += pri_string(right_side, p_char_selection,
                                                 max_length, ALDA_LOC_SHEME,
                                                 ALDA_LOC_SHEME_LENGTH, i);
                                 break;

                              case ALDA_EXEC_FLAG :
                                 i += pri_string(right_side, p_char_selection,
                                                 max_length, ALDA_EXEC_SHEME,
                                                 ALDA_EXEC_SHEME_LENGTH, i);
                                 break;

                              case ALDA_SMTP_FLAG :
                                 i += pri_string(right_side, p_char_selection,
                                                 max_length, ALDA_SMTP_SHEME,
                                                 ALDA_SMTP_SHEME_LENGTH, i);
                                 break;

                              case ALDA_DE_MAIL_FLAG :
                                 i += pri_string(right_side, p_char_selection,
                                                 max_length, ALDA_DEMAIL_SHEME,
                                                 ALDA_DEMAIL_SHEME_LENGTH, i);
                                 break;

                              case ALDA_SFTP_FLAG :
                                 i += pri_string(right_side, p_char_selection,
                                                 max_length, ALDA_SFTP_SHEME,
                                                 ALDA_SFTP_SHEME_LENGTH, i);
                                 break;

                              case ALDA_SCP_FLAG :
                                 i += pri_string(right_side, p_char_selection,
                                                 max_length, ALDA_SCP_SHEME,
                                                 ALDA_SCP_SHEME_LENGTH, i);
                                 break;

                              case ALDA_HTTP_FLAG :
                                 i += pri_string(right_side, p_char_selection,
                                                 max_length, ALDA_HTTP_SHEME,
                                                 ALDA_HTTP_SHEME_LENGTH, i);
                                 break;

                              case ALDA_HTTPS_FLAG :
                                 i += pri_string(right_side, p_char_selection,
                                                 max_length, ALDA_HTTPS_SHEME,
                                                 ALDA_HTTPS_SHEME_LENGTH, i);
                                 break;

                              case ALDA_FTPS_FLAG :
                                 i += pri_string(right_side, p_char_selection,
                                                 max_length, ALDA_FTPS_SHEME,
                                                 ALDA_FTPS_SHEME_LENGTH, i);
                                 break;

                              case ALDA_WMO_FLAG :
                                 i += pri_string(right_side, p_char_selection,
                                                 max_length, ALDA_WMO_SHEME,
                                                 ALDA_WMO_SHEME_LENGTH, i);
                                 break;

                              case ALDA_MAP_FLAG :
                                 i += pri_string(right_side, p_char_selection,
                                                 max_length, ALDA_MAP_SHEME,
                                                 ALDA_MAP_SHEME_LENGTH, i);
                                 break;

                              case ALDA_DFAX_FLAG :
                                 i += pri_string(right_side, p_char_selection,
                                                 max_length, ALDA_DFAX_SHEME,
                                                 ALDA_DFAX_SHEME_LENGTH, i);
                                 break;

                              default :
                                 i += pri_string(right_side, p_char_selection,
                                                 max_length, ALDA_UNKNOWN_SHEME,
                                                 ALDA_UNKNOWN_SHEME_LENGTH, i);
                                 break;
                           }
                        }
                        ptr++;
                        break;

                     case 'p' : /* Protocol ID used for transmission. */
                        i += pri_int(format_orientation, fo_pos, max_length,
                                     base_char, (int)olog.protocol, i);
                        ptr++;
                        break;

                     case 'S' : /* Output file size. */
                        if ((j = pri_size(format_orientation, fo_pos,
                                          max_length, base_char, *(ptr + 2),
                                          olog.file_size, i)) == -1)
                        {
                           j = ptr + 2 - p_start;
                           (void)memcpy(&output_line[i], p_start, j);
                        }
                        ptr += 2;
                        i += j;
                        break;

                     case 'J' : /* Job ID. */
                        i += pri_id(format_orientation, fo_pos, max_length,
                                    base_char, olog.job_id, i);
                        ptr++;
                        break;

                     case 'e' : /* Number of retries. */
                        i += pri_int(format_orientation, fo_pos, max_length,
                                     base_char, (int)olog.retries, i);
                        ptr++;
                        break;

                     case 'A' : /* Archive directory. */
                        i += pri_string(right_side, p_char_selection,
                                        max_length, olog.archive_dir,
                                        olog.archive_dir_length, i);
                        ptr++;
                        break;

                     case 'Z' : /* Job creation time. */
                        if ((j = pri_time(format_orientation, fo_pos,
                                          max_length, base_char, *(ptr + 2),
                                          olog.job_creation_time,
                                          &olog.bd_job_creation_time, i)) == -1)
                        {
                           j = ptr + 2 - p_start;
                           (void)memcpy(&output_line[i], p_start, j);
                        }
                        ptr += 2;
                        i += j;
                        break;

                     case 'U' : /* Unique number. */
                        i += pri_id(format_orientation, fo_pos, max_length,
                                    base_char, olog.unique_number, i);
                        ptr++;
                        break;

                     case 'L' : /* Split job number. */
                        i += pri_id(format_orientation, fo_pos, max_length,
                                    base_char, olog.split_job_counter, i);
                        ptr++;
                        break;

                     case 'h' : /* Target real hostname/IP. */
                        if (olog.real_hostname[0] == '\0')
                        {
                           (void)get_real_hostname(olog.alias_name,
                                                   olog.current_toggle,
                                                   olog.real_hostname);
                        }
                        i += pri_string(right_side, p_char_selection,
                                        max_length, olog.real_hostname,
                                        strlen(olog.real_hostname), i);
                        ptr++;
                        break;

                     case 'H' : /* Target alias name. */
                        i += pri_string(right_side, p_char_selection,
                                        max_length, olog.alias_name,
                                        olog.alias_name_length, i);
                        ptr++;
                        break;

                     case 'o' : /* Output type ID. */
                        i += pri_int(format_orientation, fo_pos, max_length,
                                     base_char, (int)olog.output_type, i);
                        ptr++;
                        break;

                     case 'O' : /* Output type string. */
                        if (olog.output_type <= MAX_OUTPUT_TYPES)
                        {
                           i += pri_string(right_side, p_char_selection,
                                           max_length, otstr[olog.output_type],
                                           strlen(otstr[olog.output_type]),
                                           i);
                        }
                        else
                        {
                           i += pri_string(right_side, p_char_selection,
                                           max_length, otstr[OT_UNKNOWN],
                                           strlen(otstr[OT_UNKNOWN]),
                                           i);
                        }
                        ptr++;
                        break;

                     case 'R' : /* Recipient of job. */
                        i += pri_string(right_side, p_char_selection,
                                        max_length, olog.recipient,
                                        strlen(olog.recipient), i);
                        ptr++;
                        break;

                     default  : /* Unknown, lets print this as supplied. */
                        ptr++;
                        j = ptr - p_start;
                        (void)memcpy(&output_line[i], p_start, j);
                        i += j;
                        break;
                  }
                  break;
#endif /* _OUTPUT_LOG */
#ifdef _DELETE_LOG
               case 'D' : /* Delete data. */
                  switch (*(ptr + 1))
                  {
                     case 't' : /* Job creation time. */
                        if ((j = pri_time(format_orientation, fo_pos,
                                          max_length, base_char, *(ptr + 2),
                                          dlog.job_creation_time,
                                          &dlog.bd_job_creation_time, i)) == -1)
                        {
                           j = ptr + 2 - p_start;
                           (void)memcpy(&output_line[i], p_start, j);
                        }
                        ptr += 2;
                        i += j;
                        break;

                     case 'T' : /* Time when file was deleted. */
                        if ((j = pri_time(format_orientation, fo_pos,
                                          max_length, base_char, *(ptr + 2),
                                          dlog.delete_time,
                                          &dlog.bd_delete_time, i)) == -1)
                        {
                           j = ptr + 2 - p_start;
                           (void)memcpy(&output_line[i], p_start, j);
                        }
                        ptr += 2;
                        i += j;
                        break;

                     case 'r' : /* Delete reason ID. */
                        i += pri_int(format_orientation, fo_pos, max_length,
                                     base_char, (int)dlog.deletion_type, i);
                        ptr++;
                        break;

                     case 'R' : /* Delete reason string. */
                        if (dlog.delete_time == -1)
                        {
                           i += pri_string(right_side, p_char_selection,
                                           max_length, "", 0, i);
                        }
                        else
                        {
                           if (dlog.deletion_type <= MAX_DELETE_REASONS)
                           {
                              i += pri_string(right_side, p_char_selection,
                                              max_length,
                                              drstr[dlog.deletion_type],
                                              strlen(drstr[dlog.deletion_type]),
                                              i);
                           }
                           else
                           {
                              i += pri_string(right_side, p_char_selection,
                                              max_length, UKN_DEL_REASON_STR,
                                              UKN_DEL_REASON_STR_LENGTH, i);
                           }
                        }
                        ptr++;
                        break;

                     case 'W' : /* User/program causing deletion. */
                        i += pri_string(right_side, p_char_selection,
                                        max_length, dlog.user_process,
                                        dlog.user_process_length, i);
                        ptr++;
                        break;

                     case 'A' : /* Additional reason. */
                        i += pri_string(right_side, p_char_selection,
                                        max_length, dlog.add_reason,
                                        dlog.add_reason_length, i);
                        ptr++;
                        break;

                     case 'Z' : /* Job creation time. */
                        if ((j = pri_time(format_orientation, fo_pos,
                                          max_length, base_char, *(ptr + 2),
                                          dlog.job_creation_time,
                                          &dlog.bd_job_creation_time, i)) == -1)
                        {
                           j = ptr + 2 - p_start;
                           (void)memcpy(&output_line[i], p_start, j);
                        }
                        ptr += 2;
                        i += j;
                        break;

                     case 'U' : /* Unique number. */
                        i += pri_id(format_orientation, fo_pos, max_length,
                                    base_char, dlog.unique_number, i);
                        ptr++;
                        break;

                     case 'L' : /* Split job number. */
                        i += pri_id(format_orientation, fo_pos, max_length,
                                    base_char, dlog.split_job_counter, i);
                        ptr++;
                        break;

                     case 'F' : /* File name of deleted file. */
                        i += pri_string(right_side, p_char_selection,
                                        max_length, dlog.filename,
                                        dlog.filename_length, i);
                        ptr++;
                        break;

                     case 'S' : /* File size of deleted file. */
                        if ((j = pri_size(format_orientation, fo_pos,
                                          max_length, base_char, *(ptr + 2),
                                          dlog.file_size, i)) == -1)
                        {
                           j = ptr + 2 - p_start;
                           (void)memcpy(&output_line[i], p_start, j);
                        }
                        ptr += 2;
                        i += j;
                        break;

                     case 'J' : /* Job ID of deleted file. */
                        i += pri_id(format_orientation, fo_pos, max_length,
                                    base_char, dlog.job_id, i);
                        ptr++;
                        break;

                     case 'I' : /* Input source ID. */
                        i += pri_id(format_orientation, fo_pos, max_length,
                                    base_char, dlog.dir_id, i);
                        ptr++;
                        break;

                     case 'N' : /* Full source name. */
                        if (dlog.dir_id != 0)
                        {
                           int  full_source_length = 0;
                           char full_source[MAX_PATH_LENGTH];

                           get_full_source(dlog.dir_id, full_source,
                                           &full_source_length);
                           i += pri_string(right_side, p_char_selection,
                                           max_length, full_source,
                                           full_source_length, i);
                        }
                        ptr++;
                        break;

                     case 'H' : /* Target alias name. */
                        if ((dlog.alias_name[0] == '\0') && (dlog.job_id != 0))
                        {
                           get_alias_name(dlog.job_id, dlog.alias_name,
                                          &dlog.alias_name_length);
                        }
                        i += pri_string(right_side, p_char_selection,
                                        max_length, dlog.alias_name,
                                        dlog.alias_name_length, i);
                        ptr++;
                        break;

                     default  : /* Unknown, lets print this as supplied. */
                        ptr++;
                        j = ptr - p_start;
                        (void)memcpy(&output_line[i], p_start, j);
                        i += j;
                        break;
                  }
                  break;
#endif /* _DELETE_LOG */
               case 'A' : /* Data about remote AFD. */
                  switch (*(ptr + 1))
                  {
                     case 'h' : /* AFD real hostname/IP. */
                        i += pri_string(right_side, p_char_selection,
                                        max_length, afd.hostname,
                                        afd.hostname_length, i);
                        ptr++;
                        break;

                     case 'H' : /* AFD alias name. */
                        i += pri_string(right_side, p_char_selection,
                                        max_length, afd.aliasname,
                                        afd.aliasname_length, i);
                        ptr++;
                        break;

                     case 'V' : /* AFD Version. */
                        i += pri_string(right_side, p_char_selection,
                                        max_length, afd.version,
                                        afd.version_length, i);
                        ptr++;
                        break;

                     default  : /* Unknown, lets print this as supplied. */
                        ptr++;
                        j = ptr - p_start;
                        (void)memcpy(&output_line[i], p_start, j);
                        i += j;
                        break;
                  }
                  break;

               default  : /* Unknown, lets print this as supplied. */
                  j = ptr - p_start;
                  (void)memcpy(&output_line[i], p_start, j);
                  i += j;
                  break;
            }
         }
         else if ((*ptr == '\\') &&
                  ((*(ptr + 1) == 'n') || (*(ptr + 1) == 't')))
              {
                 ptr++;
                 if (*ptr == 'n')
                 {
                    output_line[i] = '\n';
                 }
                 else
                 {
                    output_line[i] = '\t';
                 }
                 i++;
              }
              else
              {
                 output_line[i] = *ptr;
                 i++;
              }
         if (*ptr != '\0')
         {
            ptr++;
         }
      }
   } while (*ptr != '\0');

   output_line[i] = '\0';
   log_data_written += (off_t)fprintf(output_fp, "%s\n", output_line);
   if ((output_filename[0] != '\0') &&
       ((mode & ALDA_CONTINUOUS_MODE) || (mode & ALDA_CONTINUOUS_DAEMON_MODE)))
   {
      (void)fflush(output_fp);
   }
   data_printed = YES;

   return;
}


#ifdef TO_EVALUATE
/*+++++++++++++++++++++++++++++++ pri_string() ++++++++++++++++++++++++++*/
static int
pri_string(char       *format_orientation,
           int        fo_pos,
           int        max_length,
           const char *string,
           int        pos)
{
   format_orientation[fo_pos] = 's';
   format_orientation[fo_pos + 1] = '"';
   format_orientation[fo_pos + 2] = '\0';
   fo_pos = sprintf(&output_line[pos], format_orientation, string);
   if ((max_length > 0) && (fo_pos > max_length))
   {
      output_line[pos + max_length] = '\0';
      output_line[pos + max_length - 1] = '>';
      fo_pos = max_length;
   }

   return(fo_pos);
}
#endif


/*+++++++++++++++++++++++++++++++ pri_string() ++++++++++++++++++++++++++*/
static int
pri_string(int        right_side,
           char       *p_char_selection,
           int        max_length,
           const char *string,
           int        str_length,
           int        pos)
{
   if (max_length == 0)
   {
      if (p_char_selection == NULL)
      {
         (void)memcpy(&output_line[pos], string, str_length);
         output_line[pos + str_length] = '\0';
      }
      else
      {
         int  bytes_written = 0,
              i,
              number1,
              number2;
         char *ptr = p_char_selection,
              str_number[MAX_INT_LENGTH];

         do
         {
            if (*ptr == '$')
            {
               number1 = str_length - 1;
               ptr++;
            }
            else
            {
               i = 0;
               while ((isdigit((int)(*ptr))) && (i < MAX_INT_LENGTH))
               {
                  str_number[i] = *ptr;
                  ptr++; i++;
               }
               if (i >= MAX_INT_LENGTH)
               {
                  while (isdigit((int)(*ptr)))
                  {
                     ptr++;
                  }
               }
               str_number[i] = '\0';
               number1 = atoi(str_number);
               if (number1 > str_length)
               {
                  number1 = str_length - 1;
               }
            }
            if ((*ptr == ',') || (*ptr == ']'))
            {
               output_line[pos] = string[number1];
               pos++;
               bytes_written++;
               if (*ptr == ',')
               {
                  ptr++;
               }
            }
            else if (*ptr == '-')
                 {
                    ptr++;
                    if (*ptr == '$')
                    {
                       number2 = str_length - 1;
                       ptr++;
                    }
                    else
                    {
                       i = 0;
                       while ((isdigit((int)(*ptr))) && (i < MAX_INT_LENGTH))
                       {
                          str_number[i] = *ptr;
                          ptr++; i++;
                       }
                       if (i >= MAX_INT_LENGTH)
                       {
                          while (isdigit((int)(*ptr)))
                          {
                             ptr++;
                          }
                       }
                       str_number[i] = '\0';
                       number2 = atoi(str_number);
                       if (number2 > str_length)
                       {
                          number2 = str_length - 1;
                       }
                    }
                    (void)memcpy(&output_line[pos], &string[number1],
                                 (number2 - number1) + 1);
                    pos += ((number2 - number1) + 1);
                    bytes_written += ((number2 - number1) + 1);
                    if (*ptr == ',')
                    {
                       ptr++;
                    }
                 }
                 else /* Ignore character. */
                 {
                    ptr++;
                 }
         } while (*ptr != ']');
         output_line[pos] = '\0';
         str_length = bytes_written;
      }

      return(str_length);
   }
   else
   {
      if (str_length >= max_length)
      {
         (void)memcpy(&output_line[pos], string, max_length);
         if (str_length > max_length)
         {
            output_line[pos + max_length - 1] = '>';
         }
      }
      else
      {
         int fill_length;

         fill_length = max_length - str_length;
         if (right_side == YES)
         {
            (void)memset(&output_line[pos], ' ', fill_length);
            (void)memcpy(&output_line[pos + fill_length], string, str_length);
         }
         else
         {
            (void)memcpy(&output_line[pos], string, str_length);
            (void)memset(&output_line[pos + str_length], ' ', fill_length);
         }
      }
      output_line[pos + max_length] = '\0';

      return(max_length);
   }

   return(INCORRECT);
}


/*++++++++++++++++++++++++++++++++ pri_size() +++++++++++++++++++++++++++*/
static int
pri_size(char  *format_orientation,
         int   fo_pos,
         int   max_length,
         char  base_char,
         char  type,
         off_t size,
         int   pos)
{
   char unit_str[5];

   if (size == -1)
   {
      size = 0;
   }

   if (base_char == 'f')
   {
      double divisor;

      format_orientation[fo_pos] = base_char;
      format_orientation[fo_pos + 1] = '%';
      format_orientation[fo_pos + 2] = 's';
      format_orientation[fo_pos + 3] = '\0';

      switch (type)
      {
         case 'a' : /* Automatic shortes format. */
            unit_str[0] = ' ';
            if (size >= EXAFILE)
            {
               divisor = F_EXAFILE;
               unit_str[1] = 'E';
               unit_str[2] = 'B';
               unit_str[3] = '\0';
            }
            else if (size >= PETAFILE)
                 {
                    divisor = F_PETAFILE;
                    unit_str[1] = 'P';
                    unit_str[2] = 'B';
                    unit_str[3] = '\0';
                 }
            else if (size >= TERAFILE)
                 {
                    divisor = F_TERAFILE;
                    unit_str[1] = 'T';
                    unit_str[2] = 'B';
                    unit_str[3] = '\0';
                 }
            else if (size >= GIGAFILE)
                 {
                    divisor = F_GIGAFILE;
                    unit_str[1] = 'G';
                    unit_str[2] = 'B';
                    unit_str[3] = '\0';
                 }
            else if (size >= MEGAFILE)
                 {
                    divisor = F_MEGAFILE;
                    unit_str[1] = 'M';
                    unit_str[2] = 'B';
                    unit_str[3] = '\0';
                 }
            else if (size >= KILOFILE)
                 {
                    divisor = F_KILOFILE;
                    unit_str[1] = 'K';
                    unit_str[2] = 'B';
                    unit_str[3] = '\0';
                 }
                 else
                 {
                    divisor = 1.0;
                    unit_str[1] = 'B';
                    unit_str[2] = '\0';
                 }
            break;

         case 'A' : /* Automatic shortes format. */
            unit_str[0] = ' ';
            if (size >= EXABYTE)
            {
               divisor = F_EXABYTE;
               unit_str[1] = 'E';
               unit_str[2] = 'i';
               unit_str[3] = 'B';
               unit_str[4] = '\0';
            }
            else if (size >= PETABYTE)
                 {
                    divisor = F_PETABYTE;
                    unit_str[1] = 'P';
                    unit_str[2] = 'i';
                    unit_str[3] = 'B';
                    unit_str[4] = '\0';
                 }
            else if (size >= TERABYTE)
                 {
                    divisor = F_TERABYTE;
                    unit_str[1] = 'T';
                    unit_str[2] = 'i';
                    unit_str[3] = 'B';
                    unit_str[4] = '\0';
                 }
            else if (size >= GIGABYTE)
                 {
                    divisor = F_GIGABYTE;
                    unit_str[1] = 'G';
                    unit_str[2] = 'i';
                    unit_str[3] = 'B';
                    unit_str[4] = '\0';
                 }
            else if (size >= MEGABYTE)
                 {
                    divisor = F_MEGABYTE;
                    unit_str[1] = 'M';
                    unit_str[2] = 'i';
                    unit_str[3] = 'B';
                    unit_str[4] = '\0';
                 }
            else if (size >= KILOBYTE)
                 {
                    divisor = F_KILOBYTE;
                    unit_str[1] = 'K';
                    unit_str[2] = 'i';
                    unit_str[3] = 'B';
                    unit_str[4] = '\0';
                 }
                 else
                 {
                    divisor = 1.0;
                    unit_str[1] = 'B';
                    unit_str[2] = '\0';
                 }
            break;

         case 'b' :
         case 'B' : /* Bytes only. */
            unit_str[0] = '\0';
            divisor = 1.0;
            break;

         case 'e' : /* Exabyte only. */
            unit_str[0] = '\0';
            divisor = F_EXAFILE;
            break;

         case 'E' : /* Exbibyte only. */
            unit_str[0] = '\0';
            divisor = F_EXABYTE;
            break;

         case 'g' : /* Gigabyte only. */
            unit_str[0] = '\0';
            divisor = F_GIGAFILE;
            break;

         case 'G' : /* Gibibyte only. */
            unit_str[0] = '\0';
            divisor = F_GIGABYTE;
            break;

         case 'k' : /* Kilobyte only. */
            unit_str[0] = '\0';
            divisor = F_KILOFILE;
            break;

         case 'K' : /* Kibibyte only. */
            unit_str[0] = '\0';
            divisor = F_KILOBYTE;
            break;

         case 'm' : /* Megabyte only. */
            unit_str[0] = '\0';
            divisor = F_MEGAFILE;
            break;

         case 'M' : /* Mebibyte only. */
            unit_str[0] = '\0';
            divisor = F_MEGABYTE;
            break;

         case 'p' : /* Petabyte only. */
            unit_str[0] = '\0';
            divisor = F_PETAFILE;
            break;

         case 'P' : /* Pebibyte only. */
            unit_str[0] = '\0';
            divisor = F_PETABYTE;
            break;

         case 't' : /* Terabyte only. */
            unit_str[0] = '\0';
            divisor = F_TERAFILE;
            break;

         case 'T' : /* Tebibyte only. */
            unit_str[0] = '\0';
            divisor = F_TERABYTE;
            break;

         default  : /* Unknown, lets print this as supplied. */
            return(-1);
      }
      fo_pos = sprintf(&output_line[pos], format_orientation,
                       (double)((double)size / divisor), unit_str);
   }
   else /* 'd', 'o' or 'x'. */
   {
      off_t divisor;

      format_orientation[fo_pos++] = 'l';
#if SIZEOF_OFF_T > 4
      format_orientation[fo_pos++] = 'l';
#endif
      format_orientation[fo_pos] = base_char;
      format_orientation[fo_pos + 1] = '%';
      format_orientation[fo_pos + 2] = 's';
      format_orientation[fo_pos + 3] = '\0';

      switch (type)
      {
         case 'a' : /* Automatic shortes format. */
            unit_str[0] = ' ';
            if (size >= EXAFILE)
            {
               divisor = EXAFILE;
               unit_str[1] = 'E';
               unit_str[2] = 'B';
               unit_str[3] = '\0';
            }
            else if (size >= PETAFILE)
                 {
                    divisor = PETAFILE;
                    unit_str[1] = 'P';
                    unit_str[2] = 'B';
                    unit_str[3] = '\0';
                 }
            else if (size >= TERAFILE)
                 {
                    divisor = TERAFILE;
                    unit_str[1] = 'T';
                    unit_str[2] = 'B';
                    unit_str[3] = '\0';
                 }
            else if (size >= GIGAFILE)
                 {
                    divisor = GIGAFILE;
                    unit_str[1] = 'G';
                    unit_str[2] = 'B';
                    unit_str[3] = '\0';
                 }
            else if (size >= MEGAFILE)
                 {
                    divisor = MEGAFILE;
                    unit_str[1] = 'M';
                    unit_str[2] = 'B';
                    unit_str[3] = '\0';
                 }
            else if (size >= KILOFILE)
                 {
                    divisor = KILOFILE;
                    unit_str[1] = 'K';
                    unit_str[2] = 'B';
                    unit_str[3] = '\0';
                 }
                 else
                 {
                    divisor = 1;
                    unit_str[1] = 'B';
                    unit_str[2] = '\0';
                 }
            break;

         case 'A' : /* Automatic shortes format. */
            unit_str[0] = ' ';
            if (size >= EXABYTE)
            {
               divisor = EXABYTE;
               unit_str[1] = 'E';
               unit_str[2] = 'i';
               unit_str[3] = 'B';
               unit_str[4] = '\0';
            }
            else if (size >= PETABYTE)
                 {
                    divisor = PETABYTE;
                    unit_str[1] = 'P';
                    unit_str[2] = 'i';
                    unit_str[3] = 'B';
                    unit_str[4] = '\0';
                 }
            else if (size >= TERABYTE)
                 {
                    divisor = TERABYTE;
                    unit_str[1] = 'T';
                    unit_str[2] = 'i';
                    unit_str[3] = 'B';
                    unit_str[4] = '\0';
                 }
            else if (size >= GIGABYTE)
                 {
                    divisor = GIGABYTE;
                    unit_str[1] = 'G';
                    unit_str[2] = 'i';
                    unit_str[3] = 'B';
                    unit_str[4] = '\0';
                 }
            else if (size >= MEGABYTE)
                 {
                    divisor = MEGABYTE;
                    unit_str[1] = 'M';
                    unit_str[2] = 'i';
                    unit_str[3] = 'B';
                    unit_str[4] = '\0';
                 }
            else if (size >= KILOBYTE)
                 {
                    divisor = KILOBYTE;
                    unit_str[1] = 'K';
                    unit_str[2] = 'i';
                    unit_str[3] = 'B';
                    unit_str[4] = '\0';
                 }
                 else
                 {
                    divisor = 1;
                    unit_str[1] = 'B';
                    unit_str[2] = '\0';
                 }
            break;

         case 'b' :
         case 'B' : /* Bytes only. */
            unit_str[0] = '\0';
            divisor = 1;
            break;

         case 'e' : /* Exabyte only. */
            unit_str[0] = '\0';
            divisor = EXAFILE;
            break;

         case 'E' : /* Exbibyte only. */
            unit_str[0] = '\0';
            divisor = EXABYTE;
            break;

         case 'g' : /* Gigabyte only. */
            unit_str[0] = '\0';
            divisor = GIGAFILE;
            break;

         case 'G' : /* Gibibyte only. */
            unit_str[0] = '\0';
            divisor = GIGABYTE;
            break;

         case 'k' : /* Kilobyte only. */
            unit_str[0] = '\0';
            divisor = KILOFILE;
            break;

         case 'K' : /* Kibibyte only. */
            unit_str[0] = '\0';
            divisor = KILOBYTE;
            break;

         case 'm' : /* Megabyte only. */
            unit_str[0] = '\0';
            divisor = MEGAFILE;
            break;

         case 'M' : /* Mebibyte only. */
            unit_str[0] = '\0';
            divisor = MEGABYTE;
            break;

         case 'p' : /* Petabyte only. */
            unit_str[0] = '\0';
            divisor = PETAFILE;
            break;

         case 'P' : /* Pebibyte only. */
            unit_str[0] = '\0';
            divisor = PETABYTE;
            break;

         case 't' : /* Terabyte only. */
            unit_str[0] = '\0';
            divisor = TERAFILE;
            break;

         case 'T' : /* Tebibyte only. */
            unit_str[0] = '\0';
            divisor = TERABYTE;
            break;

         default  : /* Unknown, lets print this as supplied. */
            return(-1);
      }
      fo_pos = sprintf(&output_line[pos], format_orientation,
                       (pri_off_t)(size / divisor), unit_str);
   }

   if ((max_length > 0) && (fo_pos > max_length))
   {
      output_line[pos + max_length] = '\0';
      output_line[pos + max_length - 1] = '>';
      fo_pos = max_length;
   }

   return(fo_pos);
}


/*+++++++++++++++++++++++++++++++++ pri_id() ++++++++++++++++++++++++++++*/
static int
pri_id(char         *format_orientation,
       int          fo_pos,
       int          max_length,
       int          base_char,
       unsigned int id,
       int          pos)
{
   if (base_char == 'd')
   {
      format_orientation[fo_pos] = 'u';
   }
   else
   {
      format_orientation[fo_pos] = base_char;
   }
   format_orientation[fo_pos + 1] = '\0';
   fo_pos = sprintf(&output_line[pos], format_orientation, id);

   if ((max_length > 0) && (fo_pos > max_length))
   {
      output_line[pos + max_length] = '\0';
      output_line[pos + max_length - 1] = '>';
      fo_pos = max_length;
   }

   return(fo_pos);
}


/*++++++++++++++++++++++++++++++++ pri_int() ++++++++++++++++++++++++++++*/
static int
pri_int(char *format_orientation,
        int  fo_pos,
        int  max_length,
        int  base_char,
        int  value,
        int  pos)
{
   format_orientation[fo_pos] = base_char;
   format_orientation[fo_pos + 1] = '\0';
   fo_pos = sprintf(&output_line[pos], format_orientation, value);

   if ((max_length > 0) && (fo_pos > max_length))
   {
      output_line[pos + max_length] = '\0';
      output_line[pos + max_length - 1] = '>';
      fo_pos = max_length;
   }

   return(fo_pos);
}


#ifdef _DISTRIBUTION_LOG
/*+++++++++++++++++++++++++++++ pri_int_array() +++++++++++++++++++++++++*/
static int
pri_int_array(char         *format_orientation,
              int          fo_pos,
              int          max_length,
              int          base_char,
              int          no_of_elements,
              unsigned int *value,
              char         separator_char,
              int          pos)
{
   if (base_char == 'd')
   {
      format_orientation[fo_pos] = 'u';
   }
   else
   {
      format_orientation[fo_pos] = base_char;
   }
   format_orientation[fo_pos + 1] = '\0';

   if (value == NULL)
   {
      output_line[pos] = '?';
      output_line[pos + 1] = '\0';
      fo_pos = 1;
   }
   else
   {
      int  i;
      char new_format_orientation[MAX_FORMAT_ORIENTATION_LENGTH];

      i = sprintf(&output_line[pos], format_orientation, value[0]);
      if ((max_length > 0) && (i > max_length))
      {
         output_line[pos + max_length] = '\0';
         output_line[pos + max_length - 1] = '>';

         return(max_length);
      }
      new_format_orientation[0] = '%';
      new_format_orientation[1] = 'c';
      (void)strcpy(&new_format_orientation[2], format_orientation);
      fo_pos = i;
      for (i = 1; i < no_of_elements; i++)
      {
         fo_pos += sprintf(&output_line[pos + fo_pos], new_format_orientation,
                           separator_char, value[i]);

         if ((max_length > 0) && (fo_pos > max_length))
         {
            output_line[pos + max_length] = '\0';
            output_line[pos + max_length - 1] = '>';
            fo_pos = max_length;
            break;
         }
      }
   }

   return(fo_pos);
}


/*++++++++++++++++++++++++++ pri_int_char_array() +++++++++++++++++++++++*/
static int
pri_int_char_array(char          *format_orientation,
                   int           fo_pos,
                   int           max_length,
                   int           base_char,
                   int           no_of_elements,
                   unsigned char *value,
                   char          separator_char,
                   int           pos)
{
   int  i;
   char new_format_orientation[MAX_FORMAT_ORIENTATION_LENGTH];

   format_orientation[fo_pos] = base_char;
   format_orientation[fo_pos + 1] = '\0';

   i = sprintf(&output_line[pos], format_orientation, (int)value[0]);
   if ((max_length > 0) && (i > max_length))
   {
      output_line[pos + max_length] = '\0';
      output_line[pos + max_length - 1] = '>';

      return(max_length);
   }
   new_format_orientation[0] = '%';
   new_format_orientation[1] = 'c';
   (void)strcpy(&new_format_orientation[2], format_orientation);
   fo_pos = i;
   for (i = 1; i < no_of_elements; i++)
   {
      fo_pos += sprintf(&output_line[pos + fo_pos], new_format_orientation,
                        separator_char, (int)value[i]);

      if ((max_length > 0) && (fo_pos > max_length))
      {
         output_line[pos + max_length] = '\0';
         output_line[pos + max_length - 1] = '>';
         fo_pos = max_length;
         break;
      }
   }

   return(fo_pos);
}
#endif /* _DISTRIBUTION_LOG */


#if defined (_PRODUCTION_LOG) || defined (_OUTPUT_LOG)
/*+++++++++++++++++++++++++++++ pri_duration() ++++++++++++++++++++++++++*/
static int
pri_duration(char   *format_orientation,
             int    fo_pos,
             int    max_length,
             int    base_char,
             char   type,
             double duration,
             int    pos)
{
   int  divisor;
   char unit_str[2];

   switch (type)
   {
      case 'A' : /* Automatic shortes format. */
         unit_str[1] = '\0';
         if (duration < 60)
         {
            divisor = 1;
            unit_str[0] = 's';
         }
         else if (duration < 3600)
              {
                 divisor = 60;
                 unit_str[0] = 'm';
              }
         else if (duration < 86400)
              {
                 divisor = 3600;
                 unit_str[0] = 'h';
              }
              else
              {
                 divisor = 86400;
                 unit_str[0] = 'd';
              }
         break;

      case 'D' : /* Days only. */
         divisor = 86400;
         unit_str[0] = '\0';
         break;

      case 'H' : /* Hours only. */
         divisor = 3600;
         unit_str[0] = '\0';
         break;

      case 'M' : /* Minutes only. */
         divisor = 60;
         unit_str[0] = '\0';
         break;

      case 'S' : /* Seconds only. */
         divisor = 1;
         unit_str[0] = '\0';
         break;

      case 'X' : /* Time (h:mm:ss). */
         {
            int hours, left, min;

            hours = duration / 3600;
            left  = (unsigned long)duration % 3600;
            min   = left / 60;
            left  = left % 60;
            fo_pos = sprintf(&output_line[pos], "%d:%02d:%02d", 
                             hours, min, left);
            divisor = 0;
         }
         break;

      case 'Y' : /* Time (d:hh:mm). */
         {
            int days, left, hours;

            days  = duration / 86400;
            left  = (unsigned long)duration % 86400;
            hours = left / 3600;
            left  = (left % 3600) / 60;
            fo_pos = sprintf(&output_line[pos], "%d:%02d:%02d", 
                             days, hours, left);
            divisor = 0;
         }
         break;

      default  : /* Unknown, lets print this as supplied. */
         return(-1);
   }

   if (divisor != 0)
   {
      format_orientation[fo_pos] = base_char;
      format_orientation[fo_pos + 1] = '%';
      format_orientation[fo_pos + 2] = 's';
      format_orientation[fo_pos + 3] = '\0';
      if (base_char == 'f')
      {
         fo_pos = sprintf(&output_line[pos], format_orientation, 
                          duration / (double)divisor, unit_str);
      }
      else /* 'd', 'o' or 'x'. */
      {
         fo_pos = sprintf(&output_line[pos], format_orientation, 
                          (int)duration / divisor, unit_str);
      }

      if ((max_length > 0) && (fo_pos > max_length))
      {
         output_line[pos + max_length] = '\0';
         output_line[pos + max_length - 1] = '>';
         fo_pos = max_length;
      }
   }

   return(fo_pos);
}
#endif /* defined (_PRODUCTION_LOG) || defined (_OUTPUT_LOG) */


/*++++++++++++++++++++++++++++++++ pri_time() +++++++++++++++++++++++++++*/
static int
pri_time(char      *format_orientation,
         int       fo_pos,
         int       max_length,
         char      base_char,
         char      time_modifier,
         time_t    time_val,
         struct tm *bd_time_val,
         int       pos)
{
   switch (time_modifier)
   {
      case 'a' : /* Abbreviated weekday name: Tue */
      case 'A' : /* Full weekday name: Tuesday */
      case 'b' : /* Abbreviated month name: Jan */
      case 'B' : /* Full month name: January */
      case 'c' : /* Date and time: Tue Jan 19 16:24:50 1999 */
      case 'd' : /* Day of the month [01 - 31]: 19 */
      case 'H' : /* Hour of the 24-hour day [00 - 23]: 16 */
      case 'I' : /* Hour of the 24-hour day [00 - 12]: 04 */
      case 'j' : /* Day of the year [001 - 366]: 19 */
      case 'm' : /* Month [01 - 12]: 01 */
      case 'M' : /* Minute [00 - 59]: 24 */
      case 'p' : /* AM/PM: PM */
      case 'S' : /* Second [00 - 61]: 50 */
      case 'U' : /* Sunday week number [00 - 53]: 02 */
      case 'w' : /* Weekday [0 - 6] (0=Sunday): 2 */
      case 'W' : /* Monday week number [00 - 53]: 02 */
      case 'X' : /* Time: 16:24:50 */
      case 'y' : /* Year without centry [00 - 99]: 99 */
      case 'Y' : /* Year with centry: 1999 */
      case 'Z' : /* Time zone name: CET */
         {
            int  tmp_pos;
            char time_format[3];

            time_format[0] = '%';
            time_format[1] = time_modifier;
            time_format[2] = '\0';
            if (time_val == -1)
            {
               bd_time_val->tm_sec = 0;
               bd_time_val->tm_min = 0;
               bd_time_val->tm_hour = 0;
               bd_time_val->tm_mday = 0;
               bd_time_val->tm_mon = 0;
               bd_time_val->tm_year = 0;
               bd_time_val->tm_wday = 0;
               bd_time_val->tm_yday = 0;

               tmp_pos = pos;
            }
            else
            {
               if (bd_time_val->tm_mday == 0)
               {
                  struct tm *p_bd;

                  p_bd = localtime(&time_val);
                  (void)memcpy(bd_time_val, p_bd, sizeof(struct tm));
               }
               tmp_pos = 0; /* Silence compiler. */
            }
            pos = strftime(&output_line[pos], MAX_OUTPUT_LINE_LENGTH - pos,
                           time_format, bd_time_val);
            if (time_val == -1)
            {
               (void)memset(&output_line[tmp_pos], ' ', pos);
            }
         }
         break;

      case 'u' : /* Unix time: 916759490 */
         {
            format_orientation[fo_pos++] = 'l';
#if SIZEOF_TIME_T > 4
            format_orientation[fo_pos++] = 'l';
#endif
            format_orientation[fo_pos] = base_char;
            format_orientation[fo_pos + 1] = '\0';
            if (time_val == -1)
            {
               time_val = 0;
            }
            pos = sprintf(&output_line[pos], format_orientation, time_val);
         }
         break;

      default  : /* Unknown, lets print this as supplied. */
         pos = -1;
         break;
   }

   return(pos);
}
