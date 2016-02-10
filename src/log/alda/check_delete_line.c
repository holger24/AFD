/*
 *  check_delete_line.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 - 2015 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_delete_line - stores the log data in a structure if it matches
 **
 ** SYNOPSIS
 **   int  check_delete_line(char *line, char *prev_file_name,
 **                          off_t prev_filename_length,
 **                          time_t prev_log_time, unsigned int prev_job_id,
 **                          unsigned int *prev_unique_number,
 **                          unsigned int *prev_split_job_counter)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS when entry is matching. If the entry is not wanted 
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
                                 log_date_length,
                                 max_hostname_length,
                                 trace_mode,
                                 verbose;
extern unsigned int              file_pattern_counter,
                                 mode,
                                 search_file_size_flag,
                                 search_job_id;
extern time_t                    start_time_end,
                                 start_time_start;
extern off_t                     search_file_size;
extern char                      **file_pattern;
extern struct alda_cache_data    *dcache;
extern struct alda_position_list **dpl;
extern struct log_file_data      delete;
extern struct alda_ddata         dlog;
extern struct jid_data           jidd;


/*######################### check_delete_line() #########################*/
int
check_delete_line(char         *line,
                  char         *prev_file_name,
                  off_t        prev_filename_length,
                  time_t       prev_log_time,
                  unsigned int prev_job_id,
                  unsigned int *prev_unique_number,
                  unsigned int *prev_split_job_counter)
{
   register char *ptr = line + log_date_length + 1;

   if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE))
   {
      if ((dcache[delete.current_file_no].mpc != dcache[delete.current_file_no].pc) &&
          (dpl[delete.current_file_no][dcache[delete.current_file_no].pc].gotcha == YES))
      {
         dcache[delete.current_file_no].pc++;
         return(DATA_ALREADY_SHOWN);
      }
   }
   dlog.delete_time = (time_t)str2timet(line, NULL, 16);
   if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE))
   {
      if (dcache[delete.current_file_no].mpc == dcache[delete.current_file_no].pc)
      {
         dpl[delete.current_file_no][dcache[delete.current_file_no].pc].time = dlog.delete_time;
         dpl[delete.current_file_no][dcache[delete.current_file_no].pc].gotcha = NO;
# ifdef CACHE_DEBUG
         dpl[delete.current_file_no][dcache[delete.current_file_no].pc].filename[0] = '\0';
# endif
         dcache[delete.current_file_no].mpc++;
      }
      dcache[delete.current_file_no].pc++;
   }
   if ((dlog.delete_time >= start_time_start) &&
       ((start_time_end == 0) || (dlog.delete_time < start_time_end)))
   {
      register int i = 0;

      while (*ptr == ' ')
      {
         ptr++;
      }
      while ((*(ptr + i) != ' ') && (i < MAX_REAL_HOSTNAME_LENGTH) &&
             (*(ptr + i) != '\0'))
      {
         dlog.alias_name[i] = *(ptr + i);
         i++;
      }
      if (*(ptr + i) == ' ')
      {
         int old_type;

         if ((i == 1) && (dlog.alias_name[0] == '-'))
         {
            dlog.alias_name_length = 0;
         }
         else
         {
            dlog.alias_name_length = i;
         }
         dlog.alias_name[dlog.alias_name_length] = '\0';
         if (*(ptr + max_hostname_length + 2) == SEPARATOR_CHAR)
         {
            dlog.deletion_type = (unsigned int)(*(ptr + max_hostname_length + 1) - '0');
            ptr += max_hostname_length + 2;
            old_type = YES;
         }
         else
         {
            char hex_dr_str[4];

            hex_dr_str[0] = *(ptr + max_hostname_length + 1);
            hex_dr_str[1] = *(ptr + max_hostname_length + 2);
            hex_dr_str[2] = *(ptr + max_hostname_length + 3);
            hex_dr_str[3] = '\0';
            if (hex_dr_str[0] == '0')
            {
               hex_dr_str[0] = ' ';
               if (hex_dr_str[1] == '0')
               {
                  hex_dr_str[1] = ' ';
               }
            }
            dlog.deletion_type = (unsigned int)strtol(hex_dr_str, NULL, 16);
            ptr += max_hostname_length + 4;
            old_type = NO;
         }
         if (*ptr == SEPARATOR_CHAR)
         {
            ptr++;
            i = 0;
            while ((*(ptr + i) != SEPARATOR_CHAR) &&
                   (i < MAX_FILENAME_LENGTH) && (*(ptr + i) != '\0'))
            {
               dlog.filename[i] = *(ptr + i);
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

               dlog.filename[i] = '\0';
               dlog.filename_length = i;

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
                     if (prev_filename_length == dlog.filename_length)
                     {
                        ret = my_strcmp(local_pattern[j], dlog.filename);
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
                     ret = pmatch(local_pattern[j], dlog.filename, NULL);
                  }
                  if (ret == 0)
                  {
                     /*
                      * This file is wanted, so lets store the rest
                      * and/or do more checks.
                      */
                     ptr += i + 1;

                     /* Store file size. */
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
                        dlog.file_size = (off_t)str2offt(ptr, NULL, 16);
                        if (((search_file_size_flag & SEARCH_DELETE_LOG) == 0) ||
                            ((search_file_size == -1) ||
                             ((gt_lt_sign == EQUAL_SIGN) &&
                              (dlog.file_size == search_file_size)) ||
                             ((gt_lt_sign == LESS_THEN_SIGN) &&
                              (dlog.file_size < search_file_size)) ||
                             ((gt_lt_sign == GREATER_THEN_SIGN) &&
                              (dlog.file_size > search_file_size))))
                        {
                           ptr += i + 1;

                           /* Store job ID. */
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
                              dlog.job_id = (unsigned int)strtoul(ptr, NULL, 16);
                              if (old_type == NO)
                              {
                                 ptr += i + 1;

                                 /* Store directory ID. */
                                 i = 0;
                                 while ((*(ptr + i) != SEPARATOR_CHAR) &&
                                        (*(ptr + i) != '\0') &&
                                        (i < MAX_INT_HEX_LENGTH))
                                 {
                                    i++;
                                 }
                                 if (*(ptr + i) == SEPARATOR_CHAR)
                                 {
                                    if (i > 0)
                                    {
                                       *(ptr + i) = '\0';
                                       dlog.dir_id = (unsigned int)strtoul(ptr, NULL, 16);
                                       if ((dlog.dir_id != 0) &&
                                           (check_did(dlog.dir_id) != SUCCESS))
                                       {
                                          /*
                                           * This file is definitly not wanted,
                                           * so let us just ignore it.
                                           */
                                          dlog.filename[0] = '\0';
                                          dlog.alias_name[0] = '\0';
                                          dlog.file_size = -1;
                                          dlog.filename_length = 0;
                                          dlog.alias_name_length = 0;
                                          dlog.job_id = 0;
                                          dlog.dir_id = 0;
                                          dlog.deletion_type = 0;
# ifndef HAVE_GETLINE
                                          while (*(ptr + i) != '\0')
                                          {
                                             i++;
                                          }
                                          delete.bytes_read += (ptr + i - line);
# endif

                                          return(NOT_WANTED);
                                       }
                                    }
                                    else
                                    {
                                       dlog.dir_id = 0;
                                    }

                                    ptr += i + 1;

                                    /* Store creation time. */
                                    i = 0;
                                    while ((*(ptr + i) != '_') &&
                                           (*(ptr + i) != SEPARATOR_CHAR) &&
                                           (*(ptr + i) != '\0') &&
                                           (i < MAX_INT_HEX_LENGTH))
                                    {
                                       i++;
                                    }
                                    if (*(ptr + i) == '_')
                                    {
                                       *(ptr + i) = '\0';
                                       dlog.job_creation_time = (time_t)str2timet(ptr, NULL, 16);
                                       if ((dlog.job_creation_time >= start_time_start) &&
                                           ((prev_log_time == 0) || (dlog.job_creation_time == prev_log_time)) &&
                                           ((start_time_end == 0) || (dlog.job_creation_time < start_time_end)))
                                       {
                                          ptr += i + 1;

                                          /* Store unique number. */
                                          i = 0;
                                          while ((*(ptr + i) != '_') &&
                                                 (*(ptr + i) != SEPARATOR_CHAR) &&
                                                 (*(ptr + i) != '\0') &&
                                                 (i < MAX_INT_HEX_LENGTH))
                                          {
                                             i++;
                                          }
                                          if (*(ptr + i) == '_')
                                          {
                                             *(ptr + i) = '\0';
                                             dlog.unique_number = (unsigned int)strtoul(ptr, NULL, 16);

                                             if ((prev_unique_number == NULL) ||
                                                 (dlog.unique_number == *prev_unique_number))
                                             {
                                                ptr += i + 1;

                                                /* Store split job counter. */
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
                                                   dlog.split_job_counter = (unsigned int)strtoul(ptr, NULL, 16);
                                                   if ((prev_split_job_counter != NULL) &&
                                                       (dlog.split_job_counter != *prev_split_job_counter))
                                                   {
                                                      dlog.filename[0] = '\0';
                                                      dlog.alias_name[0] = '\0';
                                                      dlog.file_size = -1;
                                                      dlog.job_creation_time = -1L;
                                                      dlog.filename_length = 0;
                                                      dlog.alias_name_length = 0;
                                                      dlog.job_id = 0;
                                                      dlog.dir_id = 0;
                                                      dlog.deletion_type = 0;
                                                      dlog.unique_number = 0;
                                                      dlog.split_job_counter = 0;
# ifndef HAVE_GETLINE
                                                      while (*(ptr + i) != '\0')
                                                      {
                                                         i++;
                                                      }
                                                      delete.bytes_read += (ptr + i - line);
# endif

                                                      return(NOT_WANTED);
                                                   }
                                                }
                                                else
                                                {
                                                   if (i == MAX_INT_HEX_LENGTH)
                                                   {
                                                      (void)fprintf(stderr,
                                                                    "Unable to store the split job counter since it is to large. (%s %d)\n",
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
                                                                    "Unable to store the split job counter because end was not found. (%s %d)\n",
                                                                    __FILE__, __LINE__);
                                                   }
                                                   dlog.filename[0] = '\0';
                                                   dlog.alias_name[0] = '\0';
                                                   dlog.file_size = -1;
                                                   dlog.job_creation_time = -1L;
                                                   dlog.filename_length = 0;
                                                   dlog.alias_name_length = 0;
                                                   dlog.job_id = 0;
                                                   dlog.dir_id = 0;
                                                   dlog.deletion_type = 0;
                                                   dlog.unique_number = 0;
# ifndef HAVE_GETLINE
                                                   delete.bytes_read += (ptr + i - line);
# endif

                                                   return(INCORRECT);
                                                }
                                             }
                                             else
                                             {
                                                dlog.filename[0] = '\0';
                                                dlog.alias_name[0] = '\0';
                                                dlog.file_size = -1;
                                                dlog.job_creation_time = -1L;
                                                dlog.filename_length = 0;
                                                dlog.alias_name_length = 0;
                                                dlog.job_id = 0;
                                                dlog.dir_id = 0;
                                                dlog.deletion_type = 0;
                                                dlog.unique_number = 0;
# ifndef HAVE_GETLINE
                                                while (*(ptr + i) != '\0')
                                                {
                                                   i++;
                                                }
                                                delete.bytes_read += (ptr + i - line);
# endif

                                                return(NOT_WANTED);
                                             }
                                          }
                                          else
                                          {
                                             if (i == MAX_INT_HEX_LENGTH)
                                             {
                                                (void)fprintf(stderr,
                                                              "Unable to store the unique number since it is to large. (%s %d)\n",
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
                                                              "Unable to store the unique number because end was not found. (%s %d)\n",
                                                              __FILE__, __LINE__);
                                             }
                                             dlog.filename[0] = '\0';
                                             dlog.alias_name[0] = '\0';
                                             dlog.file_size = -1;
                                             dlog.job_creation_time = -1L;
                                             dlog.filename_length = 0;
                                             dlog.alias_name_length = 0;
                                             dlog.job_id = 0;
                                             dlog.dir_id = 0;
                                             dlog.deletion_type = 0;
# ifndef HAVE_GETLINE
                                             delete.bytes_read += (ptr + i - line);
# endif

                                             return(INCORRECT);
                                          }
                                       }
                                       else
                                       {
                                          /*
                                           * This file is definitly not wanted,
                                           * so let us just ignore it.
                                           */
                                          dlog.filename[0] = '\0';
                                          dlog.alias_name[0] = '\0';
                                          dlog.file_size = -1;
                                          dlog.job_creation_time = -1L;
                                          dlog.filename_length = 0;
                                          dlog.alias_name_length = 0;
                                          dlog.job_id = 0;
                                          dlog.dir_id = 0;
                                          dlog.deletion_type = 0;
# ifndef HAVE_GETLINE
                                          while (*(ptr + i) != '\0')
                                          {
                                             i++;
                                          }
                                          delete.bytes_read += (ptr + i - line);
# endif

                                          return(NOT_WANTED);
                                       }
                                    }
                                    else if (*(ptr + i) == SEPARATOR_CHAR)
                                         {
                                            dlog.job_creation_time = 0L;
                                            dlog.unique_number = 0;
                                            dlog.split_job_counter = 0;
# ifdef WHEN_WE_KNOW
                                            /*
                                             * Hmm, sometimes we do not have
                                             * the unique ID since the file
                                             * is deleted before it enters
                                             * AFD or it is a deleted time
                                             * job. Not sure if is correct
                                             * to not count this data as a
                                             * hit.
                                             */
                                            if ((prev_unique_name != 0) ||
                                                (prev_split_job_counter != 0))
                                            {
                                               dlog.filename[0] = '\0';
                                               dlog.alias_name[0] = '\0';
                                               dlog.file_size = -1;
                                               dlog.job_creation_time = -1L;
                                               dlog.filename_length = 0;
                                               dlog.alias_name_length = 0;
                                               dlog.job_id = 0;
                                               dlog.dir_id = 0;
                                               dlog.deletion_type = 0;
#  ifndef HAVE_GETLINE
                                               while (*(ptr + i) != '\0')
                                               {
                                                  i++;
                                               }
                                               delete.bytes_read += (ptr + i - line);
#  endif

                                               return(NOT_WANTED);
                                            }
# endif
                                         }
                                         else
                                         {
                                            if (i == MAX_INT_HEX_LENGTH)
                                            {
                                               (void)fprintf(stderr,
                                                             "Unable to store the job creation time since it is to large. (%s %d)\n",
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
                                                             "Unable to store the job creation time because end was not found. (%s %d)\n",
                                                             __FILE__, __LINE__);
                                            }
                                            dlog.filename[0] = '\0';
                                            dlog.alias_name[0] = '\0';
                                            dlog.file_size = -1;
                                            dlog.filename_length = 0;
                                            dlog.alias_name_length = 0;
                                            dlog.job_id = 0;
                                            dlog.dir_id = 0;
                                            dlog.deletion_type = 0;
# ifndef HAVE_GETLINE
                                            delete.bytes_read += (ptr + i - line);
# endif

                                            return(INCORRECT);
                                         }
                                 }
                                 else
                                 {
                                    if (i == MAX_INT_HEX_LENGTH)
                                    {
                                       (void)fprintf(stderr,
                                                     "Unable to store the directory ID since it is to large. (%s %d)\n",
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
                                                     "Unable to store the directory ID because end was not found. (%s %d)\n",
                                                     __FILE__, __LINE__);
                                    }
                                    dlog.filename[0] = '\0';
                                    dlog.alias_name[0] = '\0';
                                    dlog.file_size = -1;
                                    dlog.filename_length = 0;
                                    dlog.alias_name_length = 0;
                                    dlog.job_id = 0;
                                    dlog.deletion_type = 0;
# ifndef HAVE_GETLINE
                                    delete.bytes_read += (ptr + i - line);
# endif

                                    return(INCORRECT);
                                 }
                              }
                              else
                              {
                                 if ((dlog.deletion_type == AGE_OUTPUT) ||
                                     (dlog.deletion_type == NO_MESSAGE_FILE_DEL) ||
                                     (dlog.deletion_type == DUP_OUTPUT))
                                 {
                                    dlog.dir_id = 0;
                                 }
                                 else
                                 {
                                    dlog.dir_id = dlog.job_id;
                                    dlog.job_id = 0;
                                 }
                              }
                              if ((search_job_id == 0) || (dlog.job_id == 0) ||
                                  (dlog.job_id == search_job_id))
                              {
                                 ptr += i + 1;

                                 /* Store user/proccess deleting file. */
                                 i = 0;
                                 while ((*(ptr + i) != SEPARATOR_CHAR) &&
                                        (i < MAX_USER_NAME_LENGTH) &&
                                        (*(ptr + i) != '\n') &&
                                        (*(ptr + i) != '\0'))
                                 {
                                    dlog.user_process[i] = *(ptr + i);
                                    i++;
                                 }
                                 if ((*(ptr + i) == SEPARATOR_CHAR) ||
                                     (*(ptr + i) == '\n') ||
                                     (*(ptr + i) == '\0'))
                                 {
                                    dlog.user_process[i] = '\0';
                                    dlog.user_process_length = i;
                                    if (*(ptr + i) == SEPARATOR_CHAR)
                                    {
                                       ptr += i + 1;

                                       /* Store additional reason. */
                                       i = 0;
                                       while ((*(ptr + i) != '\n') &&
                                              (*(ptr + i) != '\0') &&
                                              (i < MAX_PATH_LENGTH))
                                       {
                                          dlog.add_reason[i] = *(ptr + i);
                                          i++;
                                       }
                                       if ((*(ptr + i) == '\n') ||
                                           (*(ptr + i) == '\0'))
                                       {
                                          dlog.add_reason[i] = '\0';
                                          dlog.add_reason_length = i;
# ifndef HAVE_GETLINE
                                          while (*(ptr + i) != '\0')
                                          {
                                             i++;
                                          }
                                          delete.bytes_read += (ptr + i - line);
# endif
                                          if (verbose > 2)
                                          {
                                             if (dlog.alias_name[0] == '\0')
                                             {
                                                (void)printf("DEBUG 3: [DELETE] %s %x %x %x %x (%u)\n",
                                                             dlog.filename,
                                                             dlog.dir_id,
                                                             dlog.job_id,
                                                             dlog.unique_number,
                                                             dlog.split_job_counter,
                                                             dlog.deletion_type);
                                             }
                                             else
                                             {
                                                (void)printf("DEBUG 3: [DELETE] %s %s %x %x %x %x (%u)\n",
                                                             dlog.filename,
                                                             dlog.alias_name,
                                                             dlog.dir_id,
                                                             dlog.job_id,
                                                             dlog.unique_number,
                                                             dlog.split_job_counter,
                                                             dlog.deletion_type);
                                             }
                                          }

                                          return(SUCCESS);
                                       }
                                       else
                                       {
                                          (void)fprintf(stderr,
                                                        "Unable to store the additional reason since it is to large. (%s %d)\n",
                                                        __FILE__, __LINE__);
                                          dlog.filename[0] = '\0';
                                          dlog.alias_name[0] = '\0';
                                          dlog.user_process[0] = '\0';
                                          dlog.add_reason[0] = '\0';
                                          dlog.file_size = -1;
                                          dlog.filename_length = 0;
                                          dlog.alias_name_length = 0;
                                          dlog.user_process_length = 0;
                                          dlog.job_id = 0;
                                          dlog.dir_id = 0;
                                          dlog.deletion_type = 0;
# ifndef HAVE_GETLINE
                                          while (*(ptr + i) != '\0')
                                          {
                                             i++;
                                          }
                                          delete.bytes_read += (ptr + i - line);
# endif

                                          return(INCORRECT);
                                       }
                                    }
                                    else
                                    {
# ifndef HAVE_GETLINE
                                       delete.bytes_read += (ptr + i - line);
# endif

                                       return(SUCCESS);
                                    }
                                 }
                                 else
                                 {
                                    (void)fprintf(stderr,
                                                  "Unable to store the user/process since it is to large. (%s %d)\n",
                                                  __FILE__, __LINE__);
                                    dlog.filename[0] = '\0';
                                    dlog.alias_name[0] = '\0';
                                    dlog.user_process[0] = '\0';
                                    dlog.file_size = -1;
                                    dlog.filename_length = 0;
                                    dlog.alias_name_length = 0;
                                    dlog.job_id = 0;
                                    dlog.dir_id = 0;
                                    dlog.deletion_type = 0;
# ifndef HAVE_GETLINE
                                    while (*(ptr + i) != '\0')
                                    {
                                       i++;
                                    }
                                    delete.bytes_read += (ptr + i - line);
# endif

                                    return(INCORRECT);
                                 }
                              }
                              else
                              {
                                 dlog.filename[0] = '\0';
                                 dlog.alias_name[0] = '\0';
                                 dlog.file_size = -1;
                                 dlog.filename_length = 0;
                                 dlog.alias_name_length = 0;
                                 dlog.job_id = 0;
                                 dlog.dir_id = 0;
                                 dlog.deletion_type = 0;
# ifndef HAVE_GETLINE
                                 while (*(ptr + i) != '\0')
                                 {
                                    i++;
                                 }
                                 delete.bytes_read += (ptr + i - line);
# endif

                                 return(NOT_WANTED);
                              }
                           }
                           else
                           {
                              if (i == MAX_INT_HEX_LENGTH)
                              {
                                 (void)fprintf(stderr,
                                               "Unable to store the job ID since it is to large. (%s %d)\n",
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
                                                     "Unable to store the job ID because end was not found. (%s %d)\n",
                                               __FILE__, __LINE__);
                              }
                              dlog.filename[0] = '\0';
                              dlog.alias_name[0] = '\0';
                              dlog.file_size = -1;
                              dlog.filename_length = 0;
                              dlog.alias_name_length = 0;
                              dlog.deletion_type = 0;
# ifndef HAVE_GETLINE
                              delete.bytes_read += (ptr + i - line);
# endif

                              return(INCORRECT);
                           }
                        }
                        else
                        {
                           dlog.filename[0] = '\0';
                           dlog.alias_name[0] = '\0';
                           dlog.file_size = -1;
                           dlog.filename_length = 0;
                           dlog.alias_name_length = 0;
                           dlog.deletion_type = 0;
# ifndef HAVE_GETLINE
                           while (*(ptr + i) != '\0')
                           {
                              i++;
                           }
                           delete.bytes_read += (ptr + i - line);
# endif

                           return(NOT_WANTED);
                        }
                     }
                     else
                     {
                        if (i == MAX_OFF_T_HEX_LENGTH)
                        {
                           (void)fprintf(stderr,
                                         "Unable to store the filename since it is to large. (%s %d)\n",
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
                                               "Unable to store the filename because end was not found. (%s %d)\n",
                                         __FILE__, __LINE__);
                        }
                        dlog.filename[0] = '\0';
                        dlog.alias_name[0] = '\0';
                        dlog.filename_length = 0;
                        dlog.alias_name_length = 0;
                        dlog.deletion_type = 0;

                        return(INCORRECT);
                     }
                  }
                  else if (ret == 1)
                       {
                          /*
                           * This file is definitly not wanted,
                           * so let us just ignore it.
                           */
                          dlog.filename[0] = '\0';
                          dlog.alias_name[0] = '\0';
                          dlog.filename_length = 0;
                          dlog.alias_name_length = 0;
                          dlog.deletion_type = 0;
# ifndef HAVE_GETLINE
                          while (*(ptr + i) != '\0')
                          {
                             i++;
                          }
                          delete.bytes_read += (ptr + i - line);
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
                                "Unable to store the filename since it is to large. (%s %d)\n",
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
                                      "Unable to store the filename because end was not found. (%s %d)\n",
                                __FILE__, __LINE__);
               }
               dlog.filename[0] = '\0';
               dlog.alias_name[0] = '\0';
               dlog.filename_length = 0;
               dlog.alias_name_length = 0;
               dlog.deletion_type = 0;
# ifndef HAVE_GETLINE
               delete.bytes_read += (ptr + i - line);
# endif

               return(INCORRECT);
            }
         }
         else
         {
            (void)fprintf(stderr,
                          "Unable to locate the filename that was deleted. (%s %d)\n",
                          __FILE__, __LINE__);
            dlog.alias_name[0] = '\0';
            dlog.alias_name_length = 0;
            dlog.deletion_type = 0;
# ifndef HAVE_GETLINE
            while (*(ptr + i) != '\0')
            {
               i++;
            }
            delete.bytes_read += (ptr + i - line);
# endif

            return(INCORRECT);
         }
      }
      else
      {
         if (i == MAX_REAL_HOSTNAME_LENGTH)
         {
            (void)fprintf(stderr,
                          "Unable to store the alias name since it is to large. (%s %d)\n",
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
                          "Unable to store the alias name because end was not found. (%s %d)\n",
                          __FILE__, __LINE__);
         }
         dlog.alias_name[0] = '\0';
         dlog.deletion_type = 0;
# ifndef HAVE_GETLINE
         delete.bytes_read += (ptr + i - line);
# endif

         return(INCORRECT);
      }
   }
   else
   {
# ifndef HAVE_GETLINE
      register int i = 0;

      while (*(ptr + i) != '\0')
      {
         i++;
      }
      delete.bytes_read += (ptr + i - line);
# endif
   }

   return(NOT_WANTED);
}
