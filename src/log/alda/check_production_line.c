/*
 *  check_production_line.c - Part of AFD, an automatic file distribution
 *                            program.
 *  Copyright (c) 2008 - 2016 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_production_line - stores the log data in a structure if it matches
 **
 ** SYNOPSIS
 **   int  check_production_line(char *line, char *prev_file_name,
 **                              off_t prev_filename_length,
 **                              time_t prev_log_time,
 **                              unsigned int prev_job_id,
 **                              unsigned int *prev_unique_number,
 **                              unsigned int *split_job_counter)
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
#include <string.h>        /* strcpy()                                   */
#include <stdlib.h>
#include "aldadefs.h"


/* External global variables. */
extern int                       gt_lt_sign,
                                 gt_lt_sign_duration,
                                 gt_lt_sign_orig,
                                 log_date_length,
                                 trace_mode,
                                 verbose;
extern unsigned int              file_pattern_counter,
                                 mode,
                                 search_duration_flag,
                                 search_file_size_flag,
                                 search_orig_file_size_flag,
                                 search_job_id,
                                 search_unique_number;
extern time_t                    start,
                                 start_time_end,
                                 start_time_start;
extern off_t                     search_file_size,
                                 search_orig_file_size;
extern double                    search_duration;
extern char                      **file_pattern;
extern struct alda_cache_data    *pcache;
extern struct alda_position_list **ppl;
extern struct log_file_data      production;
extern struct alda_pdata         plog;


/*###################### check_production_line() ########################*/
int
check_production_line(char         *line,
                      char         *prev_file_name,
                      off_t        prev_filename_length,
                      time_t       prev_log_time,
                      unsigned int prev_dir_id,
                      unsigned int prev_job_id,
                      unsigned int *prev_unique_number,
                      unsigned int *prev_split_job_counter)
{
   register int  i;
   register char *ptr = line + log_date_length;
   int           onefoureigth_or_greater;

   if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE))
   {
      if ((pcache[production.current_file_no].mpc != pcache[production.current_file_no].pc) &&
          (ppl[production.current_file_no][pcache[production.current_file_no].pc].gotcha == YES))
      {
         pcache[production.current_file_no].pc++;
         return(DATA_ALREADY_SHOWN);
      }
   }
   plog.output_time = (time_t)str2timet(line, NULL, 16);
   if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE))
   {
      if (pcache[production.current_file_no].mpc == pcache[production.current_file_no].pc)
      {
         ppl[production.current_file_no][pcache[production.current_file_no].pc].time = plog.output_time;
         ppl[production.current_file_no][pcache[production.current_file_no].pc].gotcha = NO;
#ifdef CACHE_DEBUG
         ppl[production.current_file_no][pcache[production.current_file_no].pc].filename[0] = '\0';
#endif
         pcache[production.current_file_no].mpc++;
      }
      pcache[production.current_file_no].pc++;
   }
   i = 0;
   while ((*(ptr + i) != ':') && (*(ptr + i) != '_') &&
          (i < MAX_INT_HEX_LENGTH) && (*(ptr + i) != '\0'))
   {
      i++;
   }
   if (*(ptr + i) == ':')
   {
      /* This is the 1.4.x version PRODUCTION_LOG. */
      *(ptr + i) = '\0';
      plog.ratio_1 = (unsigned int)strtoul(ptr, NULL, 16);
      ptr += i + 1;
      i = 0;
      while ((*(ptr + i) != SEPARATOR_CHAR) &&
             (i < MAX_INT_HEX_LENGTH) &&
             (*(ptr + i) != '\0'))
      {
         i++;
      }
      if (*(ptr + i) == SEPARATOR_CHAR)
      {
         *(ptr + i) = '\0';
         plog.ratio_2 = (unsigned int)strtoul(ptr, NULL, 16);
         ptr += i + 1;
         i = 0;
         while ((*(ptr + i) != '_') && (*(ptr + i) != '.') &&
                (*(ptr + i) != SEPARATOR_CHAR) && (i < MAX_DOUBLE_LENGTH) &&
                (*(ptr + i) != '\0'))
         {
            i++;
         }
      }
      else
      {
         if (i == MAX_INT_HEX_LENGTH)
         {
            (void)fprintf(stderr,
                          "Unable to store the unique number since it is to large. (%s %d)\n",
                          __FILE__, __LINE__);
#ifndef HAVE_GETLINE
            while (*(ptr + i) != '\0')
            {
               i++;
            }
#endif
         }
         else
         {
            (void)fprintf(stderr,
                          "Unable to store the unique number because end was not found. (%s %d)\n",
                          __FILE__, __LINE__);
         }
         plog.input_time = -1;
         plog.unique_number = 0;
#ifndef HAVE_GETLINE
         production.bytes_read += (ptr + i - line);
#endif

         return(INCORRECT);
      }
   }
   else if (*(ptr + i) == '_')
        {
           plog.ratio_1 = 0;
           plog.ratio_2 = 0;
           plog.production_time = 0.0;
           plog.cpu_time = 0.0;
        }

   /*
    * As of version 1.4.8 we added two more fields: production time + CPU
    * usage and the original file size.
    */
   if ((*(ptr + i) == '.') || (*(ptr + i) == SEPARATOR_CHAR))
   {
      if (*(ptr + i) == SEPARATOR_CHAR)
      {
         plog.production_time = 0.0;
         plog.cpu_time = 0.0;
         i++;
      }
      else
      {
         i++;
         while ((*(ptr + i) != '.') && (*(ptr + i) != SEPARATOR_CHAR) &&
                (i < MAX_DOUBLE_LENGTH) && (*(ptr + i) != '\0'))
         {
            i++;
         }
         if ((*(ptr + i) == '.') || (*(ptr + i) == SEPARATOR_CHAR))
         {
            char tmp_char = *(ptr + i);

            *(ptr + i) = '\0';
            plog.production_time = strtod(ptr, NULL);
            ptr += i + 1;
            i = 0;

            if (tmp_char == '.')
            {
               long int cpu_usec;
               time_t   cpu_sec;

               /* CPU usage. */
               while ((*(ptr + i) != '.') && (*(ptr + i) != SEPARATOR_CHAR) &&
                      (i < MAX_INT_HEX_LENGTH) && (*(ptr + i) != '\0'))
               {
                  i++;
               }
               if ((*(ptr + i) == '.') || (*(ptr + i) == SEPARATOR_CHAR))
               {
                  tmp_char = *(ptr + i);
                  *(ptr + i) = '\0';
                  cpu_sec = (time_t)str2timet(ptr, NULL, 16);
                  ptr += i + 1;
                  i = 0;

                  if (tmp_char == '.')
                  {
                     while ((*(ptr + i) != SEPARATOR_CHAR) &&
                            (i < MAX_INT_HEX_LENGTH) && (*(ptr + i) != '\0'))
                     {
                        i++;
                     }
                     if (*(ptr + i) == SEPARATOR_CHAR)
                     {
                        cpu_usec = strtol(ptr, NULL, 16);
                        ptr += i + 1;
                     }
                     else
                     {
                        cpu_usec = 0L;
                        while ((*(ptr + i) != SEPARATOR_CHAR) &&
                               (*(ptr + i) != '\0'))
                        {
                           i++;
                        }
                        if (*(ptr + i) == SEPARATOR_CHAR)
                        {
                           ptr += i + 1;
                        }
                        else
                        {
                           ptr += i;
                        }
                     }
                     i = 0;
                     plog.cpu_time = (double)(cpu_sec + (cpu_usec / (double)1000000));
                  }
                  else
                  {
                     plog.cpu_time = cpu_sec;
                  }
               }
               else
               {
                  plog.cpu_time = 0.0;
                  while ((*(ptr + i) != SEPARATOR_CHAR) && (*(ptr + i) != '\0'))
                  {
                     i++;
                  }
                  if (*(ptr + i) == SEPARATOR_CHAR)
                  {
                     ptr += i + 1;
                  }
                  else
                  {
                     ptr += i;
                  }
                  i = 0;
               }
            }
         }
         else
         {
            if (i == MAX_DOUBLE_LENGTH)
            {
               (void)fprintf(stderr,
                             "Unable to store the production time since it is to large. (%s %d)\n",
                             __FILE__, __LINE__);
#ifndef HAVE_GETLINE
               while (*(ptr + i) != '\0')
               {
                  i++;
               }
#endif
            }
            else
            {
               (void)fprintf(stderr,
                             "Unable to store the production time because end was not found. (%s %d)\n",
                             __FILE__, __LINE__);
            }
            plog.input_time = -1;
            plog.unique_number = 0;
            plog.ratio_1 = 0;
            plog.ratio_2 = 0;
            plog.production_time = 0.0;
#ifndef HAVE_GETLINE
            production.bytes_read += (ptr + i - line);
#endif

            return(INCORRECT);
         }
      }
      if (((search_duration_flag & SEARCH_PRODUCTION_LOG) == 0) ||
          (((gt_lt_sign_duration == EQUAL_SIGN) &&
            (plog.production_time == search_duration)) ||
           ((gt_lt_sign_duration == LESS_THEN_SIGN) &&
            (plog.production_time < search_duration)) ||
           ((gt_lt_sign_duration == GREATER_THEN_SIGN) &&
            (plog.production_time > search_duration)) ||
           ((gt_lt_sign_duration == NOT_SIGN) &&
            (plog.production_time != search_duration))))
      {
         while ((*(ptr + i) != '_') && (i < MAX_INT_HEX_LENGTH) &&
                (*(ptr + i) != '\0'))
         {
            i++;
         }
         onefoureigth_or_greater = YES;
      }
      else
      {
         plog.input_time = -1;
         plog.unique_number = 0;
         plog.ratio_1 = 0;
         plog.ratio_2 = 0;
         plog.production_time = 0.0;
#ifndef HAVE_GETLINE
         production.bytes_read += (ptr + i - line);
#endif

         return(NOT_WANTED);
      }
   }
   else
   {
      plog.cpu_time = 0.0;
      onefoureigth_or_greater = NO;
   }

   if (*(ptr + i) == '_')
   {
      *(ptr + i) = '\0';
      plog.input_time = (time_t)str2timet(ptr, NULL, 16);
      if ((plog.input_time >= start_time_start) &&
          ((prev_log_time == 0) || (plog.input_time == prev_log_time)) &&
          ((start_time_end == 0) || (plog.input_time < start_time_end)))
      {
         ptr += i + 1;
         i = 0;
         while ((*(ptr + i) != '_') &&
                (i < MAX_INT_HEX_LENGTH) && (*(ptr + i) != '\0'))
         {
            i++;
         }

         if (*(ptr + i) == '_')
         {
            *(ptr + i) = '\0';
            plog.unique_number = (unsigned int)strtoul(ptr, NULL, 16);

            if (((prev_unique_number == NULL) ||
                 (plog.unique_number == *prev_unique_number)) &&
                ((search_unique_number == 0) ||
                 (search_unique_number == plog.unique_number)))
            {
               ptr += i + 1;
               i = 0;
               while ((*(ptr + i) != SEPARATOR_CHAR) &&
                      (i < MAX_INT_HEX_LENGTH) &&
                      (*(ptr + i) != '\0'))
               {
                  i++;
               }
               if (*(ptr + i) == SEPARATOR_CHAR)
               {
                  *(ptr + i) = '\0';
                  plog.split_job_counter = (unsigned int)strtoul(ptr, NULL, 16);
                  if ((prev_split_job_counter == NULL) ||
                      (plog.split_job_counter == *prev_split_job_counter))
                  {
                     ptr += i + 1;

                     /* Store directory ID. */
                     i = 0;
                     while ((*(ptr + i) != SEPARATOR_CHAR) && 
                            (*(ptr + i) != '\0') && (i < MAX_INT_HEX_LENGTH))
                     {
                        i++;
                     }
                     if (*(ptr + i) == SEPARATOR_CHAR)
                     {
                        *(ptr + i) = '\0';
                        plog.dir_id = (unsigned int)strtoul(ptr, NULL, 16);
                        if (((prev_dir_id != 0) &&
                             (plog.dir_id == prev_dir_id)) ||
                            (check_did(plog.dir_id) == SUCCESS))
                        {
                           ptr += i + 1;

                           /* Store job ID. */
                           i = 0;
                           while ((*(ptr + i) != SEPARATOR_CHAR) &&
                                  (*(ptr + i) != '\0') && (i < MAX_INT_HEX_LENGTH))
                           {
                              i++;
                           }
                           if (*(ptr + i) == SEPARATOR_CHAR)
                           {
                              *(ptr + i) = '\0';
                              plog.job_id = (unsigned int)strtoul(ptr, NULL, 16);
                              if (((search_job_id == 0) || (plog.job_id == search_job_id)) &&
                                  ((prev_job_id == 0) || (plog.job_id == prev_job_id)))
                              {
                                 ptr += i + 1;

                                 /* Store the original filename. */
                                 i = 0;
                                 while ((*(ptr + i) != SEPARATOR_CHAR) &&
                                        (i < MAX_FILENAME_LENGTH) &&
                                        (*(ptr + i) != '\0'))
                                 {
                                    plog.original_filename[i] = *(ptr + i);
                                    i++;
                                 }
                                 if (*(ptr + i) == SEPARATOR_CHAR)
                                 {
                                    int  do_pmatch,
                                         j,
                                         local_pattern_counter,
                                         ret;
                                    char **local_pattern,
                                         *tmp_pattern[1];

                                    plog.original_filename[i] = '\0';
                                    plog.original_filename_length = i;
#ifdef CACHE_DEBUG
                                    (void)strcpy(ppl[production.current_file_no][pcache[production.current_file_no].pc - 1].filename,
                                                 plog.original_filename);
#endif

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
                                          if (prev_filename_length == plog.original_filename_length)
                                          {
                                             ret = my_strcmp(local_pattern[j],
                                                             plog.original_filename);
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
                                          ret = pmatch(local_pattern[j],
                                                       plog.original_filename,
                                                       NULL);
                                       }
                                       if (ret == 0)
                                       {
                                          /*
                                           * This file is wanted, so lets store the rest
                                           * and/or do more checks.
                                           */
                                          ptr += i + 1;

                                          if (onefoureigth_or_greater == YES)
                                          {
                                             /* Store original file size. */
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
                                                plog.original_file_size = (off_t)str2offt(ptr, NULL, 16);

                                                if (((search_orig_file_size_flag & SEARCH_PRODUCTION_LOG) == 0) ||
                                                    ((search_orig_file_size == -1) || (i == 0) ||
                                                     ((gt_lt_sign_orig == EQUAL_SIGN) &&
                                                      (plog.original_file_size == search_orig_file_size)) ||
                                                     ((gt_lt_sign_orig == LESS_THEN_SIGN) &&
                                                      (plog.original_file_size < search_orig_file_size)) ||
                                                     ((gt_lt_sign_orig == GREATER_THEN_SIGN) &&
                                                      (plog.original_file_size > search_orig_file_size)) ||
                                                     ((gt_lt_sign_orig == NOT_SIGN) &&
                                                      (plog.original_file_size != search_orig_file_size))))
                                                {
                                                   /* Lets continue. */
                                                   ptr += i + 1;
                                                }
                                                else
                                                {
                                                   /* Size does not match, so this is */
                                                   /* NOT wanted.                     */
                                                   plog.original_filename[0] = '\0';
                                                   plog.original_file_size = -1;
                                                   plog.new_filename[0] = '\0';
                                                   plog.new_file_size = -1;
                                                   plog.input_time = -1;
                                                   plog.original_filename_length = 0;
                                                   plog.new_filename_length = 0;
                                                   plog.dir_id = 0;
                                                   plog.job_id = 0;
                                                   plog.unique_number = 0;
                                                   plog.split_job_counter = 0;
#ifndef HAVE_GETLINE
                                                   while (*(ptr + i) != '\0')
                                                   {
                                                      i++;
                                                   }
                                                   production.bytes_read += (ptr + i - line);
#endif

                                                   return(NOT_WANTED);
                                                }
                                             }
                                             else
                                             {
                                                if (i == MAX_OFF_T_HEX_LENGTH)
                                                {
                                                   (void)fprintf(stderr,
                                                                 "Unable to store the size for file %s since it is to large. (%s %d)\n",
                                                                 plog.original_filename, __FILE__, __LINE__);
#ifndef HAVE_GETLINE
                                                   while (*(ptr + i) != '\0')
                                                   {
                                                      i++;
                                                   }
#endif
                                                }
                                                else
                                                {
                                                   (void)fprintf(stderr,
                                                                 "Unable to store the size for file %s because end was not found. (%s %d)\n",
                                                                 plog.original_filename, __FILE__, __LINE__);
                                                }
                                                plog.original_filename[0] = '\0';
                                                plog.original_file_size = -1;
                                                plog.new_filename[0] = '\0';
                                                plog.new_file_size = -1;
                                                plog.input_time = -1;
                                                plog.original_filename_length = 0;
                                                plog.new_filename_length = 0;
                                                plog.dir_id = 0;
                                                plog.job_id = 0;
                                                plog.unique_number = 0;
                                                plog.split_job_counter = 0;
#ifndef HAVE_GETLINE
                                                production.bytes_read += (ptr + i - line);
#endif

                                                return(INCORRECT);
                                             }
                                          }

                                          /* Store new filename. */
                                          i = 0;
                                          while ((*(ptr + i) != SEPARATOR_CHAR) &&
                                                 (i < MAX_FILENAME_LENGTH) &&
                                                 (*(ptr + i) != '\0'))
                                          {
                                             plog.new_filename[i] = *(ptr + i);
                                             i++;
                                          }
                                          if (*(ptr + i) == SEPARATOR_CHAR)
                                          {
                                             plog.new_filename[i] = '\0';
                                             plog.new_filename_length = i;
                                             ptr += i + 1;

                                             /* Store produced file size. */
                                             i = 0;
                                             while ((*(ptr + i) != SEPARATOR_CHAR) &&
                                                    (*(ptr + i) != '\0') &&
                                                    (i < MAX_OFF_T_HEX_LENGTH))
                                             {
                                                i++;
                                             }
                                             if (*(ptr + i) == SEPARATOR_CHAR)
                                             {
                                                if (i == 0)
                                                {
                                                   plog.new_file_size = 0;
                                                }
                                                else
                                                {
                                                   *(ptr + i) = '\0';
                                                   plog.new_file_size = (off_t)str2offt(ptr, NULL, 16);
                                                }
                                                if (((search_file_size_flag & SEARCH_PRODUCTION_LOG) == 0) ||
                                                    ((search_file_size == -1) || (i == 0) ||
                                                     ((gt_lt_sign == EQUAL_SIGN) &&
                                                      (plog.new_file_size == search_file_size)) ||
                                                     ((gt_lt_sign == LESS_THEN_SIGN) &&
                                                      (plog.new_file_size < search_file_size)) ||
                                                     ((gt_lt_sign == GREATER_THEN_SIGN) &&
                                                      (plog.new_file_size > search_file_size))))
                                                {
                                                   ptr += i + 1;

                                                   /* Store return code. */
                                                   i = 0;
                                                   while ((*(ptr + i) != SEPARATOR_CHAR) &&
                                                          (*(ptr + i) != '\0') &&
                                                          (i < MAX_INT_LENGTH))
                                                   {
                                                      i++;
                                                   }
                                                   if (*(ptr + i) == SEPARATOR_CHAR)
                                                   {
                                                      *(ptr + i) = '\0';
                                                      plog.return_code = atoi(ptr);
                                                      ptr += i + 1;

                                                      /* Store command executed. */
                                                      i = 0;
                                                      while ((*(ptr + i) != '\n') &&
                                                             (*(ptr + i) != '\0') &&
                                                             (i < MAX_OPTION_LENGTH))
                                                      {
                                                         plog.what_done[i] = *(ptr + i);
                                                         i++;
                                                      }
                                                      if (i < MAX_OPTION_LENGTH)
                                                      {
                                                         plog.what_done[i] = '\0';
                                                         plog.what_done_length = i;
#ifndef HAVE_GETLINE
                                                         if (*(ptr + i) == '\n')
                                                         {
                                                            i++;
                                                         }
#endif
                                                      }
                                                      else
                                                      {
                                                         (void)fprintf(stderr,
                                                                       "Unable to store the command executed since command is to long.\n");
                                                         plog.what_done[0] = '\0';
                                                         plog.what_done_length = 0;
#ifndef HAVE_GETLINE
                                                         while (*(ptr + i) != '\0')
                                                         {
                                                            i++;
                                                         }
#endif
                                                      }
#ifndef HAVE_GETLINE
                                                      production.bytes_read += (ptr + i - line);
#endif
                                                      if (verbose > 2)
                                                      {
#if SIZEOF_TIME_T == 4
                                                         (void)printf("%06ld DEBUG 3: [PRODUCTION] %s->%s %x %x %x %x\n",
#else
                                                         (void)printf("%06lld DEBUG 3: [PRODUCTION] %s->%s %x %x %x %x\n",
#endif
                                                                      (pri_time_t)(time(NULL) - start),
                                                                      plog.original_filename,
                                                                      plog.new_filename,
                                                                      plog.dir_id,
                                                                      plog.job_id,
                                                                      plog.unique_number,
                                                                      plog.split_job_counter);
                                                      }

                                                      return(SUCCESS);
                                                   }
                                                   else
                                                   {
                                                      if (i == MAX_INT_LENGTH)
                                                      {
                                                         (void)fprintf(stderr,
                                                                       "Unable to store return code for file %s since it is to large. (%s %d)\n",
                                                                       plog.original_filename, __FILE__, __LINE__);
#ifndef HAVE_GETLINE
                                                         while (*(ptr + i) != '\0')
                                                         {
                                                            i++;
                                                         }
#endif
                                                      }
                                                      else
                                                      {
                                                         (void)fprintf(stderr,
                                                                       "Unable to store return code for file %s because end was not found. (%s %d)\n",
                                                                       plog.original_filename, __FILE__, __LINE__);
                                                      }
                                                      plog.original_filename[0] = '\0';
                                                      plog.new_filename[0] = '\0';
                                                      plog.new_file_size = -1;
                                                      plog.input_time = -1;
                                                      plog.original_filename_length = 0;
                                                      plog.new_filename_length = 0;
                                                      plog.return_code = 0;
                                                      plog.dir_id = 0;
                                                      plog.job_id = 0;
                                                      plog.unique_number = 0;
                                                      plog.split_job_counter = 0;
#ifndef HAVE_GETLINE
                                                      production.bytes_read += (ptr + i - line);
#endif

                                                      return(INCORRECT);
                                                   }
                                                }
                                                else
                                                {
                                                   /* Size does not match, so this is */
                                                   /* NOT wanted.                     */
                                                   plog.original_filename[0] = '\0';
                                                   plog.new_filename[0] = '\0';
                                                   plog.new_file_size = -1;
                                                   plog.input_time = -1;
                                                   plog.original_filename_length = 0;
                                                   plog.new_filename_length = 0;
                                                   plog.dir_id = 0;
                                                   plog.job_id = 0;
                                                   plog.unique_number = 0;
                                                   plog.split_job_counter = 0;
#ifndef HAVE_GETLINE
                                                   while (*(ptr + i) != '\0')
                                                   {
                                                      i++;
                                                   }
                                                   production.bytes_read += (ptr + i - line);
#endif

                                                   return(NOT_WANTED);
                                                }
                                             }
                                             else
                                             {
                                                if (i == MAX_OFF_T_HEX_LENGTH)
                                                {
                                                   (void)fprintf(stderr,
                                                                 "Unable to store the size for file %s since it is to large. (%s %d)\n",
                                                                 plog.original_filename, __FILE__, __LINE__);
#ifndef HAVE_GETLINE
                                                   while (*(ptr + i) != '\0')
                                                   {
                                                      i++;
                                                   }
#endif
                                                }
                                                else
                                                {
                                                   (void)fprintf(stderr,
                                                                 "Unable to store the size for new file %s because end was not found. (%s %d)\n",
                                                                 plog.new_filename, __FILE__, __LINE__);
                                                }
                                                plog.original_filename[0] = '\0';
                                                plog.new_filename[0] = '\0';
                                                plog.new_file_size = -1;
                                                plog.input_time = -1;
                                                plog.original_filename_length = 0;
                                                plog.new_filename_length = 0;
                                                plog.dir_id = 0;
                                                plog.job_id = 0;
                                                plog.unique_number = 0;
                                                plog.split_job_counter = 0;
#ifndef HAVE_GETLINE
                                                production.bytes_read += (ptr + i - line);
#endif

                                                return(INCORRECT);
                                             }
                                          }
                                          else
                                          {
                                             if (i == MAX_FILENAME_LENGTH)
                                             {
                                                (void)fprintf(stderr,
                                                              "Unable to store the new filename for file %s since it is to large. (%s %d)\n",
                                                              plog.original_filename, __FILE__, __LINE__);
#ifndef HAVE_GETLINE
                                                while (*(ptr + i) != '\0')
                                                {
                                                   i++;
                                                }
#endif
                                             }
                                             else
                                             {
                                                (void)fprintf(stderr,
                                                              "Unable to store the new filename for file %s because end was not found. (%s %d)\n",
                                                              plog.original_filename, __FILE__, __LINE__);
                                             }
                                             plog.original_filename[0] = '\0';
                                             plog.new_filename[0] = '\0';
                                             plog.input_time = -1;
                                             plog.original_filename_length = 0;
                                             plog.new_filename_length = 0;
                                             plog.dir_id = 0;
                                             plog.job_id = 0;
                                             plog.unique_number = 0;
                                             plog.split_job_counter = 0;
#ifndef HAVE_GETLINE
                                             production.bytes_read += (ptr + i - line);
#endif

                                             return(INCORRECT);
                                          }
                                       }
                                       else if (ret == 1)
                                            {
                                               /*
                                                * This file is definitly not wanted,
                                                * so let us just ignore it.
                                                */
                                               plog.original_filename[0] = '\0';
                                               plog.input_time = -1;
                                               plog.original_filename_length = 0;
                                               plog.dir_id = 0;
                                               plog.job_id = 0;
                                               plog.unique_number = 0;
                                               plog.split_job_counter = 0;
#ifndef HAVE_GETLINE
                                               while (*(ptr + i) != '\0')
                                               {
                                                  i++;
                                               }
                                               production.bytes_read += (ptr + i - line);
#endif

                                               return(NOT_WANTED);
                                            }
                                    } /* for (j = 0; j < local_pattern_counter; j++) */
                                 }
                                 else
                                 {
                                    if (i == MAX_FILENAME_LENGTH)
                                    {
                                       (void)fprintf(stderr,
                                                     "Unable to store the original filename since it is to large. (%s %d)\n",
                                                     __FILE__, __LINE__);
#ifndef HAVE_GETLINE
                                       while (*(ptr + i) != '\0')
                                       {
                                          i++;
                                       }
#endif
                                    }
                                    else
                                    {
                                       (void)fprintf(stderr,
                                                     "Unable to store the original filename because end was not found. (%s %d)\n",
                                                     __FILE__, __LINE__);
                                    }
                                    plog.original_filename[0] = '\0';
                                    plog.input_time = -1;
                                    plog.original_filename_length = 0;
                                    plog.dir_id = 0;
                                    plog.job_id = 0;
                                    plog.unique_number = 0;
                                    plog.split_job_counter = 0;
#ifndef HAVE_GETLINE
                                    production.bytes_read += (ptr + i - line);
#endif

                                    return(INCORRECT);
                                 }
                              }
                              else
                              {
                                 /* This directory ID is not wanted, so ignore. */
                                 plog.input_time = -1;
                                 plog.dir_id = 0;
                                 plog.job_id = 0;
                                 plog.unique_number = 0;
                                 plog.split_job_counter = 0;
#ifndef HAVE_GETLINE
                                 while (*(ptr + i) != '\0')
                                 {
                                    i++;
                                 }
                                 production.bytes_read += (ptr + i - line);
#endif
                              }
                           }
                           else
                           {
                              if (i == MAX_INT_HEX_LENGTH)
                              {
                                 (void)fprintf(stderr,
                                               "Unable to store the job ID since it is to large. (%s %d)\n",
                                               __FILE__, __LINE__);
#ifndef HAVE_GETLINE
                                 while (*(ptr + i) != '\0')
                                 {
                                    i++;
                                 }
#endif
                              }
                              else
                              {
                                 (void)fprintf(stderr,
                                               "Unable to store the job ID because end was not found. (%s %d)\n",
                                               __FILE__, __LINE__);
                              }
                              plog.input_time = -1;
                              plog.dir_id = 0;
                              plog.unique_number = 0;
                              plog.split_job_counter = 0;
#ifndef HAVE_GETLINE
                              production.bytes_read += (ptr + i - line);
#endif

                              return(INCORRECT);
                           }
                        }
                        else
                        {
                           /* This directory ID is not wanted, so ignore. */
                           plog.input_time = -1;
                           plog.dir_id = 0;
                           plog.unique_number = 0;
                           plog.split_job_counter = 0;
#ifndef HAVE_GETLINE
                           while (*(ptr + i) != '\0')
                           {
                              i++;
                           }
                           production.bytes_read += (ptr + i - line);
#endif
                        }
                     }
                     else
                     {
                        if (i == MAX_INT_HEX_LENGTH)
                        {
                           (void)fprintf(stderr,
                                         "Unable to store the directory ID since it is to large. (%s %d)\n",
                                         __FILE__, __LINE__);
#ifndef HAVE_GETLINE
                           while (*(ptr + i) != '\0')
                           {
                              i++;
                           }
#endif
                        }
                        else
                        {
                           (void)fprintf(stderr,
                                         "Unable to store the directory ID because end was not found. (%s %d)\n",
                                         __FILE__, __LINE__);
                        }
                        plog.input_time = -1;
                        plog.unique_number = 0;
                        plog.split_job_counter = 0;
#ifndef HAVE_GETLINE
                        production.bytes_read += (ptr + i - line);
#endif

                        return(INCORRECT);
                     }
                  }
                  else
                  {
                     /* This split jon counter is not wanted, so ignore. */
                     plog.input_time = -1;
                     plog.unique_number = 0;
                     plog.split_job_counter = 0;
#ifndef HAVE_GETLINE
                     while (*(ptr + i) != '\0')
                     {
                        i++;
                     }
                     production.bytes_read += (ptr + i - line);
#endif
                  }
               }
               else
               {
                  if (i == MAX_INT_HEX_LENGTH)
                  {
                     (void)fprintf(stderr,
                                   "Unable to store the split job counter since it is to large. (%s %d)\n",
                                   __FILE__, __LINE__);
#ifndef HAVE_GETLINE
                     while (*(ptr + i) != '\0')
                     {
                        i++;
                     }
#endif
                  }
                  else
                  {
                     (void)fprintf(stderr,
                                   "Unable to store the split job counter because end was not found. (%s %d)\n",
                                   __FILE__, __LINE__);
                  }
                  plog.input_time = -1;
                  plog.unique_number = 0;
                  plog.split_job_counter = 0;
#ifndef HAVE_GETLINE
                  production.bytes_read += (ptr + i - line);
#endif

                  return(INCORRECT);
               }
            }
            else
            {
               plog.input_time = -1;
               plog.unique_number = 0;
               plog.split_job_counter = 0;
#ifndef HAVE_GETLINE
               while (*(ptr + i) != '\0')
               {
                  i++;
               }
               production.bytes_read += (ptr + i - line);
#endif
            }
         }
         else
         {
            if (i == MAX_INT_HEX_LENGTH)
            {
               (void)fprintf(stderr,
                             "Unable to store the unique number since it is to large. (%s %d)\n",
                             __FILE__, __LINE__);
#ifndef HAVE_GETLINE
               while (*(ptr + i) != '\0')
               {
                  i++;
               }
#endif
            }
            else
            {
               (void)fprintf(stderr,
                             "Unable to store the unique number because end was not found. (%s %d)\n",
                             __FILE__, __LINE__);
            }
            plog.input_time = -1;
            plog.unique_number = 0;
#ifndef HAVE_GETLINE
            production.bytes_read += (ptr + i - line);
#endif

            return(INCORRECT);
         }
      }
      else
      {
         plog.input_time = -1;
#ifndef HAVE_GETLINE
         while (*(ptr + i) != '\0')
         {
            i++;
         }
         production.bytes_read += (ptr + i - line);
#endif

         return(NOT_WANTED);
      }
   }
   else
   {
      if (i == MAX_INT_HEX_LENGTH)
      {
         (void)fprintf(stderr,
                       "Unable to store the input time since it is to large. (%s %d)\n",
                       __FILE__, __LINE__);
#ifndef HAVE_GETLINE
         while (*(ptr + i) != '\0')
         {
            i++;
         }
#endif
      }
      else
      {
         (void)fprintf(stderr,
                       "Unable to store the input time because end was not found. [%s] (%s %d)\n",
                       line, __FILE__, __LINE__);
      }
#ifndef HAVE_GETLINE
      production.bytes_read += (ptr + i - line);
#endif

      return(INCORRECT);
   }

   return(NOT_WANTED);
}
