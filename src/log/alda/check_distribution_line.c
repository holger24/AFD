/*
 *  check_distribution_line.c - Part of AFD, an automatic file distribution
 *                              program.
 *  Copyright (c) 2008 - 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_distributon_line - stores the log data in a structure if it matches
 **
 ** SYNOPSIS
 **   int  check_distribution_line(char *line, char *prev_file_name,
 **                                off_t prev_filename_length,
 **                                time_t prev_log_time,
 **                                unsigned int prev_job_id,
 **                                unsigned int *prev_unique_number)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS when entry is matching. If the entry is not wanted NOT_WANTED
 **   is returned, otherwise INCORRECT will be returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.11.2008 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>         /* fprintf()                                  */
#include <string.h>        /* strerror()                                 */
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "aldadefs.h"


/* External global variables. */
extern int                       gt_lt_sign,
                                 log_date_length,
                                 trace_mode,
                                 verbose;
extern unsigned int              file_pattern_counter,
                                 mode,
                                 search_file_size_flag;
extern time_t                    start_time_end,
                                 start_time_start;
extern off_t                     search_file_size;
extern char                      **file_pattern;
extern struct alda_cache_data    *ucache;
extern struct alda_position_list **upl;
extern struct log_file_data      distribution;
extern struct alda_udata         ulog;


/*##################### check_distribution_line() #######################*/
int
check_distribution_line(char         *line,
                        char         *prev_file_name,
                        off_t        prev_filename_length,
                        time_t       prev_log_time,
                        unsigned int prev_dir_id,
                        unsigned int *prev_unique_number)
{
   register char *ptr = line + log_date_length + 1;

   if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE))
   {
      if ((ucache[distribution.current_file_no].mpc != ucache[distribution.current_file_no].pc) &&
          (upl[distribution.current_file_no][ucache[distribution.current_file_no].pc].gotcha == YES))
      {
         ucache[distribution.current_file_no].pc++;
         return(DATA_ALREADY_SHOWN);
      }
   }
   ulog.distribution_time = (time_t)str2timet(line, NULL, 16);
   if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE))
   {
      if (ucache[distribution.current_file_no].mpc == ucache[distribution.current_file_no].pc)
      {
         upl[distribution.current_file_no][ucache[distribution.current_file_no].pc].time = ulog.distribution_time;
         upl[distribution.current_file_no][ucache[distribution.current_file_no].pc].gotcha = NO;
# ifdef CACHE_DEBUG
         upl[distribution.current_file_no][ucache[distribution.current_file_no].pc].filename[0] = '\0';
# endif
         ucache[distribution.current_file_no].mpc++;
      }
      ucache[distribution.current_file_no].pc++;
   }
   if ((ulog.distribution_time >= start_time_start) &&
       (((prev_file_name != NULL) && (mode & ALDA_FORWARD_MODE)) ||
        (start_time_end == 0) || (ulog.distribution_time < start_time_end)))
   {
      register int i = 0;

      /* Store distribution type. */
      if ((*ptr >= '0') && (*ptr <= '9'))
      {
         ulog.distribution_type = *ptr - '0';
      }
      else if ((*ptr >= 'a') && (*ptr < 'g'))
           {
              ulog.distribution_type = *ptr - 'a';
           }
           else
           {
              (void)fprintf(stderr,
                            "Unknown character (%d) for distribution type. (%s %d)\n",
                            (int)*ptr, __FILE__, __LINE__);
              ulog.distribution_time = -1;

              return(INCORRECT);
           }

      if (*(ptr + 1) == '-')
      {
         ptr += 2;

         /* Store number of distribution types. */
         while ((*(ptr + i) != SEPARATOR_CHAR) && (i < MAX_INT_HEX_LENGTH) &&
                (*(ptr + i) != '\0'))
         {
            i++;
         }
         if (*(ptr + i) == SEPARATOR_CHAR)
         {
            *(ptr + i) = '\0';
            ulog.no_of_distribution_types = (unsigned int)strtoul(ptr, NULL, 16);
            *(ptr + i) = SEPARATOR_CHAR;
            if (i > 0)
            {
               ptr += (i - 1);
            }
            else
            {
               ptr += i;
            }
            i = 0;
         }
         else
         {
            if (i == MAX_INT_HEX_LENGTH)
            {
               (void)fprintf(stderr,
                             "Unable to store number of distribution types since it is to large. (%s %d)\n",
                             __FILE__, __LINE__);
# ifndef HAVE_GETLINE
               while (*(ptr + i) != '\0')
               {
                  i++;
               }
# endif
            }
            else
            {
               (void)fprintf(stderr,
                             "Unable to store number of distribution types because end was not found. (%s %d)\n",
                             __FILE__, __LINE__);
            }
            ulog.distribution_time = -1;
            ulog.distribution_type = (unsigned char)-1;
# ifndef HAVE_GETLINE
            distribution.bytes_read += (ptr + i - line);
# endif

            return(INCORRECT);
         }
      }

      if (*(ptr + 1) == SEPARATOR_CHAR)
      {
         ptr += 2;

         /* Store file name distributed. */
         while ((*(ptr + i) != SEPARATOR_CHAR) && (i < MAX_FILENAME_LENGTH) &&
                (*(ptr + i) != '\0'))
         {
            ulog.filename[i] = *(ptr + i);
            i++;
         }
         if (*(ptr + i) == SEPARATOR_CHAR)
         {
            int  do_pmatch,
                 j,
                 local_pattern_counter,
                 ret;
            char **local_pattern,
                 *p_start,
                 *tmp_pattern[1];

            ulog.filename[i] = '\0';
            ulog.filename_length = i;

            if (prev_file_name == NULL)
            {
               local_pattern_counter = file_pattern_counter;
               local_pattern = file_pattern;
               do_pmatch = YES;
            }
            else
            {
               local_pattern_counter = 1;
               tmp_pattern[0] = prev_file_name;
               local_pattern = tmp_pattern;
               do_pmatch = NO;
            }

            for (j = 0; j < local_pattern_counter; j++)
            {
               if (do_pmatch == NO)
               {
                  if (prev_filename_length == ulog.filename_length)
                  {
                     ret = strcmp(local_pattern[j], ulog.filename);
                     if (ret == 1)
                     {
                        ret = 2;
                     }
                  }
                  else
                  {
                     ret = 2;
                  }
               }
               else
               {
                  ret = pmatch(local_pattern[j], ulog.filename, NULL);
               }
               if (ret == 0)
               {
                  /*
                   * This file is wanted, so lets store the rest and/or do
                   * more checks.
                   */
                  ptr += i + 1;

                  /* Store input time. */
                  i = 0;
                  while ((*(ptr + i) != SEPARATOR_CHAR) &&
                         (*(ptr + i) != '\0') && (i < MAX_TIME_T_HEX_LENGTH))
                  {
                     i++;
                  }
                  if (*(ptr + i) == SEPARATOR_CHAR)
                  {
                     *(ptr + i) = '\0';
                     ulog.input_time = (time_t)str2timet(ptr, NULL, 16);
                     if ((ulog.input_time >= start_time_start) &&
                         ((prev_log_time == 0) ||
                          (ulog.input_time == prev_log_time)) &&
                         ((start_time_end == 0) ||
                          (ulog.input_time < start_time_end)))
                     {
                        ptr += i + 1;

                        /* Store directory identifier. */
                        i = 0;
                        while ((*(ptr + i) != SEPARATOR_CHAR) &&
                               (i < MAX_INT_HEX_LENGTH) && (*(ptr + i) != '\0'))
                        {
                           i++;
                        }
                        if (*(ptr + i) == SEPARATOR_CHAR)
                        {
                           *(ptr + i) = '\0';
                           ulog.dir_id = (unsigned int)strtoul(ptr, NULL, 16);
                           if (((prev_dir_id != 0) &&
                                (prev_dir_id == ulog.dir_id)) ||
                               (check_did(ulog.dir_id) == SUCCESS))
                           {
                              ptr += i + 1;

                              /* Store unique number. */
                              i = 0;
                              while ((*(ptr + i) != SEPARATOR_CHAR) &&
                                     (*(ptr + i) != '\0') && 
                                     (i < MAX_INT_HEX_LENGTH))
                              {
                                 i++;
                              }
                              if (*(ptr + i) == SEPARATOR_CHAR)
                              {
                                 *(ptr + i) = '\0';
                                 ulog.unique_number = (unsigned int)strtoul(ptr, NULL, 16);

                                 if ((prev_unique_number == NULL) ||
                                     (ulog.unique_number == *prev_unique_number))
                                 {
                                    ptr += i + 1;

                                    /* Store input file size. */
                                    i = 0;
                                    while ((*(ptr + i) != SEPARATOR_CHAR) &&
                                           (*(ptr + i) != '\0') &&
                                           (i < MAX_OFF_T_HEX_LENGTH))
                                    {
                                       i++;
                                    }
                                    if (*(ptr + i) == SEPARATOR_CHAR)
                                    {
                                       *(ptr + i) = '\0';
                                       ulog.file_size = (off_t)str2offt(ptr, NULL, 16);
                                       if (((search_file_size_flag & SEARCH_DISTRIBUTION_LOG) == 0)  ||
                                           ((search_file_size == -1) ||
                                            ((gt_lt_sign == EQUAL_SIGN) &&
                                             (ulog.file_size == search_file_size)) ||
                                            ((gt_lt_sign == LESS_THEN_SIGN) &&
                                             (ulog.file_size < search_file_size)) ||
                                            ((gt_lt_sign == GREATER_THEN_SIGN) &&
                                             (ulog.file_size > search_file_size))))
                                       {
                                          ptr += i + 1;
                                          ulog.no_of_dist_jobs = 0;

                                          /*
                                           * Collect the job ID's and the number
                                           * of times they are going to be processed
                                           * in a dynamic array.
                                           */
                                          while (*ptr != '\0')
                                          {
                                             if (ulog.djid_buffer_length <= ulog.no_of_dist_jobs)
                                             {
                                                size_t size;

                                                size = ((ulog.no_of_dist_jobs / DIS_JOB_LIST_STEP_SIZE) + 1) *
                                                       DIS_JOB_LIST_STEP_SIZE;
                                                if (((ulog.job_id_list = realloc(ulog.job_id_list,
                                                                                 (size * sizeof(unsigned int)))) == NULL) ||
                                                    ((ulog.proc_cycles = realloc(ulog.proc_cycles,
                                                                                 (size * sizeof(unsigned char)))) == NULL))
                                                {
                                                   (void)fprintf(stderr,
                                                                 "realloc() error : %s (%s %d)\n",
                                                                 strerror(errno), __FILE__, __LINE__);
                                                   exit(INCORRECT);
                                                }
                                                ulog.djid_buffer_length += DIS_JOB_LIST_STEP_SIZE;
                                             }
                                             i = 0;
                                             p_start = ptr;
                                             while ((*(ptr + i) != '_') &&
                                                    (*(ptr + i) != '\0') &&
                                                    (i < MAX_INT_HEX_LENGTH))
                                             {
                                                i++;
                                             }
                                             if (*(ptr + i) == '_')
                                             {
                                                ptr += i;
                                                *ptr = '\0';
                                                ulog.job_id_list[ulog.no_of_dist_jobs] = (unsigned int)strtoul(p_start, NULL, 16);
                                                if (verbose > 2)
                                                {
                                                   (void)printf("DEBUG 3: [DISTRIBUTION] %s[%d]  %x\n",
                                                                ulog.filename,
                                                                ulog.no_of_dist_jobs,
                                                                ulog.job_id_list[ulog.no_of_dist_jobs]);
                                                }
                                                ptr++;
                                                p_start = ptr;
                                                i = 0;
                                                while ((*(ptr + i) != ',') &&
                                                       (*(ptr + i) != '\n') &&
                                                       (i < MAX_CHAR_HEX_LENGTH))
                                                {
                                                   i++;
                                                }
                                                if ((*(ptr + i) == ',') ||
                                                    (*(ptr + i) == '\n'))
                                                {
                                                   ptr += i;
                                                   *ptr = '\0';
                                                   ptr++;
                                                   ulog.proc_cycles[ulog.no_of_dist_jobs] = (unsigned char)strtoul(p_start, NULL, 16);
                                                   ulog.no_of_dist_jobs++;
                                                }
                                                else
                                                {
                                                   (void)fprintf(stderr,
                                                                 "Unable to store number of processing cycles for file %s since it is to large. (%s %d)\n",
                                                                 ulog.filename,
                                                                 __FILE__, __LINE__);
                                                   ulog.filename[0] = '\0';
                                                   ulog.file_size = -1;
                                                   ulog.distribution_time = -1;
                                                   ulog.input_time = -1;
                                                   ulog.no_of_dist_jobs = 0;
                                                   ulog.dir_id = 0;
                                                   ulog.unique_number = 0;
                                                   ulog.distribution_type = (unsigned char)-1;
# ifndef HAVE_GETLINE
                                                   while (*(ptr + i) != '\0')
                                                   {
                                                      i++;
                                                   }
                                                   distribution.bytes_read += (ptr + i - line);
# endif
                                                   return(INCORRECT);
                                                }
                                             }
                                             else
                                             {
                                                (void)fprintf(stderr,
                                                              "Unable to store job ID for file %s since it is to large. (%s %d)\n",
                                                              ulog.filename,
                                                              __FILE__, __LINE__);
                                                ulog.filename[0] = '\0';
                                                ulog.file_size = -1;
                                                ulog.distribution_time = -1;
                                                ulog.input_time = -1;
                                                ulog.no_of_dist_jobs = 0;
                                                ulog.dir_id = 0;
                                                ulog.unique_number = 0;
                                                ulog.distribution_type = (unsigned char)-1;
# ifndef HAVE_GETLINE
                                                while (*(ptr + i) != '\0')
                                                {
                                                   i++;
                                                }
                                                distribution.bytes_read += (ptr + i - line);
# endif
                                                return(INCORRECT);
                                             }
                                          }
                                          return(SUCCESS);
                                       }
                                       else
                                       {
                                          ulog.filename[0] = '\0';
                                          ulog.file_size = -1;
                                          ulog.distribution_time = -1;
                                          ulog.input_time = -1;
                                          ulog.no_of_dist_jobs = 0;
                                          ulog.dir_id = 0;
                                          ulog.unique_number = 0;
                                          ulog.distribution_type = (unsigned char)-1;
# ifndef HAVE_GETLINE
                                          while (*(ptr + i) != '\0')
                                          {
                                             i++;
                                          }
                                          distribution.bytes_read += (ptr + i - line);
# endif
                                          return(NOT_WANTED);
                                       }
                                    }
                                    else
                                    {
                                       if (i == MAX_OFF_T_HEX_LENGTH)
                                       {
                                          (void)fprintf(stderr,
                                                        "Unable to store file size for file %s since it is to large. (%s %d)\n",
                                                        ulog.filename,
                                                        __FILE__, __LINE__);
# ifndef HAVE_GETLINE
                                          while (*(ptr + i) != '\0')
                                          {
                                             i++;
                                          }
# endif
                                       }
                                       else
                                       {
                                          (void)fprintf(stderr,
                                                        "Unable to store file size for file %s because end was not found. (%s %d)\n",
                                                        ulog.filename,
                                                        __FILE__, __LINE__);
                                       }
                                       ulog.filename[0] = '\0';
                                       ulog.distribution_time = -1;
                                       ulog.input_time = -1;
                                       ulog.no_of_dist_jobs = 0;
                                       ulog.dir_id = 0;
                                       ulog.unique_number = 0;
                                       ulog.distribution_type = (unsigned char)-1;
# ifndef HAVE_GETLINE
                                       distribution.bytes_read += (ptr + i - line);
# endif

                                       return(INCORRECT);
                                    }
                                 }
                                 else
                                 {
                                    ulog.filename[0] = '\0';
                                    ulog.distribution_time = -1;
                                    ulog.input_time = -1;
                                    ulog.dir_id = 0;
                                    ulog.unique_number = 0;
                                    ulog.distribution_type = (unsigned char)-1;
# ifndef HAVE_GETLINE
                                    while (*(ptr + i) != '\0')
                                    {
                                       i++;
                                    }
                                    distribution.bytes_read += (ptr + i - line);
# endif

                                    return(NOT_WANTED);
                                 }
                              }
                              else
                              {
                                 if (i == MAX_INT_HEX_LENGTH)
                                 {
                                    (void)fprintf(stderr,
                                                  "Unable to store unique number for file %s since it is to large. (%s %d)\n",
                                                  ulog.filename,
                                                  __FILE__, __LINE__);
# ifndef HAVE_GETLINE
                                    while (*(ptr + i) != '\0')
                                    {
                                       i++;
                                    }
# endif
                                 }
                                 else
                                 {
                                    (void)fprintf(stderr,
                                                  "Unable to store unique number for file %s because end was not found. (%s %d)\n",
                                                  ulog.filename,
                                                  __FILE__, __LINE__);
                                 }
                                 ulog.filename[0] = '\0';
                                 ulog.distribution_time = -1;
                                 ulog.input_time = -1;
                                 ulog.distribution_type = (unsigned char)-1;
# ifndef HAVE_GETLINE
                                 distribution.bytes_read += (ptr + i - line);
# endif

                                 return(INCORRECT);
                              }
                           }
                           else
                           {
                              ulog.filename[0] = '\0';
                              ulog.distribution_time = -1;
                              ulog.input_time = -1;
                              ulog.dir_id = 0;
                              ulog.distribution_type = (unsigned char)-1;
# ifndef HAVE_GETLINE
                              while (*(ptr + i) != '\0')
                              {
                                 i++;
                              }
                              distribution.bytes_read += (ptr + i - line);
# endif

                              return(NOT_WANTED);
                           }
                        }
                        else
                        {
                           if (i == MAX_INT_HEX_LENGTH)
                           {
                              (void)fprintf(stderr,
                                            "Unable to store directory identifier for file %s since it is to large. (%s %d)\n",
                                            ulog.filename, __FILE__, __LINE__);
# ifndef HAVE_GETLINE
                              while (*(ptr + i) != '\0')
                              {
                                 i++;
                              }
# endif
                           }
                           else
                           {
                              (void)fprintf(stderr,
                                            "Unable to store directory identifier for file %s because end was not found. (%s %d)\n",
                                            ulog.filename, __FILE__, __LINE__);
                           }
                           ulog.filename[0] = '\0';
                           ulog.distribution_time = -1;
                           ulog.input_time = -1;
                           ulog.distribution_type = (unsigned char)-1;
# ifndef HAVE_GETLINE
                           distribution.bytes_read += (ptr + i - line);
# endif

                           return(INCORRECT);
                        }
                     }
                     else
                     {
                        ulog.filename[0] = '\0';
                        ulog.distribution_time = -1;
                        ulog.input_time = -1;
                        ulog.distribution_type = (unsigned char)-1;
# ifndef HAVE_GETLINE
                        while (*(ptr + i) != '\0')
                        {
                           i++;
                        }
                        distribution.bytes_read += (ptr + i - line);
# endif

                        return(NOT_WANTED);
                     }
                  }
                  else
                  {
                     if (i == MAX_TIME_T_HEX_LENGTH)
                     {
                        (void)fprintf(stderr,
                                      "Unable to store input time for file %s since it is to large. (%s %d)\n",
                                      ulog.filename, __FILE__, __LINE__);
# ifndef HAVE_GETLINE
                        while (*(ptr + i) != '\0')
                        {
                           i++;
                        }
# endif
                     }
                     else
                     {
                        (void)fprintf(stderr,
                                      "Unable to store input time for file %s because end was not found. (%s %d)\n",
                                      ulog.filename, __FILE__, __LINE__);
                     }
                     ulog.filename[0] = '\0';
                     ulog.distribution_time = -1;
                     ulog.distribution_type = (unsigned char)-1;
# ifndef HAVE_GETLINE
                     distribution.bytes_read += (ptr + i - line);
# endif

                     return(INCORRECT);
                  }
               }
               else if (ret == 1)
                    {
                       /*
                        * This file is definitly not wanted, so let us just
                        * ignore it.
                        */
                       ulog.distribution_time = -1;
                       ulog.filename[0] = '\0';
                       ulog.distribution_type = (unsigned char)-1;
# ifndef HAVE_GETLINE
                       while (*(ptr + i) != '\0')
                       {
                          i++;
                       }
                       distribution.bytes_read += (ptr + i - line);
# endif

                       return(NOT_WANTED);
                    }
            } /* for (j = 0; j < local_pattern_counter; j++) */
         }
         else
         {
            if (i == MAX_FILENAME_LENGTH)
            {
               (void)fprintf(stderr,
                             "Unable to store input file name since it is to long. (%s %d)\n",
                             __FILE__, __LINE__);
# ifndef HAVE_GETLINE
                while (*(ptr + i) != '\0')
                {
                   i++;
                }
# endif
            }
            else
            {
               (void)fprintf(stderr,
                             "Unable to read input file name due to premature end of line. (%s %d)\n",
                             __FILE__, __LINE__);
            }
            ulog.distribution_time = -1;
# ifndef HAVE_GETLINE
            distribution.bytes_read += (ptr + i - line);
# endif

            return(INCORRECT);;
         }
      }
      else
      {
         (void)fprintf(stderr,
                       "Unable to locate end of distribution type. (%s %d)\n",
                       __FILE__, __LINE__);
         ulog.distribution_time = -1;
# ifndef HAVE_GETLINE
         while (*(ptr + i) != '\0')
         {
            i++;
         }
         distribution.bytes_read += (ptr + i - line);
# endif

         return(INCORRECT);
      }
   }
   else
   {
# ifndef HAVE_GETLINE
      register int i = 0;
# endif

      if ((prev_file_name == NULL) && (mode & ALDA_FORWARD_MODE) &&
          (start_time_end != 0) && (ulog.distribution_time > start_time_end))
      {
         return(SEARCH_TIME_UP);
      }
# ifndef HAVE_GETLINE
      while (*(ptr + i) != '\0')
      {
         i++;
      }
      distribution.bytes_read += (ptr + i - line);
# endif
   }

   return(NOT_WANTED);
}
