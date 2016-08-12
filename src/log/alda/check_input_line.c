/*
 *  check_input_line.c - Part of AFD, an automatic file distribution program.
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
 **   check_log_line - stores the log data in a structure if it matches
 **
 ** SYNOPSIS
 **   int check_input_line(char *line, char *prev_file_name,
 **                        off_t prev_filename_length, time_t prev_log_time)
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
#include <stdlib.h>        /* str2timet(), strtoul()                     */
#include "aldadefs.h"


/* External global variables. */
extern int                  gt_lt_sign,
                            log_date_length,
                            verbose;
extern unsigned int         file_pattern_counter,
                            search_file_size_flag;
extern time_t               start,
                            start_time_end,
                            start_time_start;
extern off_t                search_file_size;
extern char                 **file_pattern;
#ifndef HAVE_GETLINE
extern struct log_file_data input;
#endif
extern struct alda_idata    ilog;


/*######################### check_input_line() ##########################*/
int
check_input_line(char         *line,
                 char         *prev_file_name,
                 off_t        prev_filename_length,
                 time_t       prev_log_time,
                 unsigned int prev_dir_id)
{
   register char *ptr = line + log_date_length + 1;

   ilog.input_time = (time_t)str2timet(line, NULL, 16);
   if ((ilog.input_time >= start_time_start) &&
       ((start_time_end == 0) || (ilog.input_time < start_time_end)))
   {
      register int i = 0;

      while ((*(ptr + i) != SEPARATOR_CHAR) && (i < MAX_FILENAME_LENGTH) &&
             (*(ptr + i) != '\0'))
      {
         ilog.filename[i] = *(ptr + i);
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

         ilog.filename[i] = '\0';
         ilog.filename_length = i;

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
               if (prev_filename_length == ilog.filename_length)
               {
                  ret = my_strcmp(local_pattern[j], ilog.filename);
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
               ret = pmatch(local_pattern[j], ilog.filename, NULL);
            }
            if (ret == 0)
            {
               /*
                * This file is wanted, so lets store the rest and/or do
                * more checks.
                */
               ptr += i + 1;

               /* Store input file size. */
               i = 0;
               while ((*(ptr + i) != SEPARATOR_CHAR) && (*(ptr + i) != '\0') &&
                      (i < MAX_OFF_T_HEX_LENGTH))
               {
                  i++;
               }
               if (*(ptr + i) == SEPARATOR_CHAR)
               {
                  *(ptr + i) = '\0';
                  ilog.file_size = (off_t)str2offt(ptr, NULL, 16);
                  if (((search_file_size_flag & SEARCH_INPUT_LOG) == 0) ||
                      (((search_file_size == -1) ||
                       ((gt_lt_sign == EQUAL_SIGN) &&
                        (ilog.file_size == search_file_size)) ||
                       ((gt_lt_sign == LESS_THEN_SIGN) &&
                        (ilog.file_size < search_file_size)) ||
                       ((gt_lt_sign == GREATER_THEN_SIGN) &&
                        (ilog.file_size > search_file_size)))))
                  {
                     ptr += i + 1;

                     /* Store directory identifier. */
                     i = 0;
                     while ((*(ptr + i) != SEPARATOR_CHAR) && 
                            (*(ptr + i) != '\0') && (i < MAX_INT_HEX_LENGTH))
                     {
                        i++;
                     }
                     if (*(ptr + i) == SEPARATOR_CHAR)
                     {
                        *(ptr + i) = '\0';
                        ilog.dir_id = (unsigned int)strtoul(ptr, NULL, 16);
                        if (((prev_dir_id != 0) &&
                             (prev_dir_id == ilog.dir_id)) ||
                            (check_did(ilog.dir_id) == SUCCESS))
                        {
                           ptr += i + 1;

                           /* Store unique number. */
                           i = 0;
                           while ((*(ptr + i) != '\0') && 
                                  (i < MAX_INT_HEX_LENGTH))
                           {
                              i++;
                           }
                           if (*(ptr + i) == '\0')
                           {
                              *(ptr + i) = '\0';
                              ilog.unique_number = (unsigned int)strtoul(ptr, NULL, 16);
# ifndef HAVE_GETLINE
                              input.bytes_read += (ptr + i - line);
# endif
                              if (verbose > 2)
                              {
# if SIZEOF_TIME_T == 4
                                 (void)printf("%06ld DEBUG 3: [INPUT] %s %x %x\n",
# else
                                 (void)printf("%06lld DEBUG 3: [INPUT] %s %x %x\n",
# endif
                                              (pri_time_t)(time(NULL) - start),
                                              ilog.filename, ilog.dir_id,
                                              ilog.unique_number);
                              }

                              return(SUCCESS);
                           }
                           else
                           {
                              (void)fprintf(stderr,
                                            "Unable to store unique number since it is to long.\n");
                              ilog.dir_id = 0;
                              ilog.input_time = -1;
                              ilog.filename[0] = '\0';
                              ilog.file_size = -1;
# ifndef HAVE_GETLINE
                              while (*(ptr + i) != '\0')
                              {
                                 i++;
                              }
                              input.bytes_read += (ptr + i - line);
# endif

                              return(INCORRECT);
                           }
                        }
                        else
                        {
                           ilog.dir_id = 0;
                           ilog.input_time = -1;
                           ilog.filename[0] = '\0';
                           ilog.file_size = -1;
# ifndef HAVE_GETLINE
                           while (*(ptr + i) != '\0')
                           {
                              i++;
                           }
                           input.bytes_read += (ptr + i - line);
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
                                         ilog.filename, __FILE__, __LINE__);
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
                                         ilog.filename, __FILE__, __LINE__);
                        }
                        ilog.dir_id = 0;
                        ilog.input_time = -1;
                        ilog.filename[0] = '\0';
                        ilog.file_size = -1;
# ifndef HAVE_GETLINE
                        input.bytes_read += (ptr + i - line);
# endif

                        return(INCORRECT);
                     }
                  }
                  else
                  {
                     /* Size does not match, so this is NOT wanted. */
                     ilog.input_time = -1;
                     ilog.filename[0] = '\0';
                     ilog.file_size = -1;
# ifndef HAVE_GETLINE
                     while (*(ptr + i) != '\0')
                     {
                        i++;
                     }
                     input.bytes_read += (ptr + i - line);
# endif

                     return(NOT_WANTED);
                  }
               }
               else
               {
                  if (i == MAX_OFF_T_HEX_LENGTH)
                  {
                     (void)fprintf(stderr,
                                   "Unable to store size for file %s since it is to large. (%s %d)\n",
                                   ilog.filename, __FILE__, __LINE__);
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
                                   "Unable to store size for file %s because end was not found. (%s %d)\n",
                                   ilog.filename, __FILE__, __LINE__);
                  }
                  ilog.input_time = -1;
                  ilog.filename[0] = '\0';
# ifndef HAVE_GETLINE
                  input.bytes_read += (ptr + i - line);
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
                    ilog.input_time = -1;
                    ilog.filename[0] = '\0';
# ifndef HAVE_GETLINE
                    while (*(ptr + i) != '\0')
                    {
                       i++;
                    }
                    input.bytes_read += (ptr + i - line);
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
# ifndef HAVE_GETLINE
         input.bytes_read += (ptr + i - line);
# endif

         return(INCORRECT);
      }
   }
   else
   {
# ifndef HAVE_GETLINE
      register int i = 0;
# endif

      if ((start_time_end != 0) && (ilog.input_time > start_time_end))
      {
         return(SEARCH_TIME_UP);
      }
# ifndef HAVE_GETLINE
      while (*(ptr + i) != '\0')
      {
         i++;
      }
      input.bytes_read += (ptr + i - line);
# endif
   }

   return(NOT_WANTED);
}
