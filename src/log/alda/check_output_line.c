/*
 *  check_output_line.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_output_line - stores the log data in a structure if it matches
 **
 ** SYNOPSIS
 **   int  check_output_line(char         *line,
 **                          char         *prev_file_name,
 **                          off_t        prev_filename_length,
 **                          time_t       prev_log_time,
 **                          unsigned int prev_job_id,
 **                          unsigned int *prev_unique_number,
 **                          unsigned int *prev_split_job_counter)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.11.2008 H.Kiehl Created
 **   11.02.2010 H.Kiehl Check if we need to search for a certain directory.
 **   18.05.2018 H.Kiehl Added option to only search for retrieved data
 **                      in OUTPUT_LOG.
 **   30.08.2022 A.Maul  When there is a mail ID, do not forget to
 **                      continue reading for archive data.
 **
 */
DESCR__E_M1

#include <stdio.h>         /* fprintf()                                  */
#include <string.h>        /* strcpy()                                   */
#include <stdlib.h>
#ifdef WITH_AFD_MON
# include "mondefs.h"
#endif
#include "aldadefs.h"


/* External global variables. */
extern int                       gt_lt_sign,
                                 gt_lt_sign_duration,
                                 log_date_length,
                                 max_hostname_length,
                                 trace_mode,
                                 verbose;
extern unsigned int              file_pattern_counter,
                                 mode,
                                 search_dir_alias_counter,
                                 search_dir_id_counter,
                                 search_dir_name_counter,
                                 search_duration_flag,
                                 search_file_size_flag,
                                 search_job_id,
                                 search_unique_number,
                                 show_output_type;
extern time_t                    start,
                                 start_time_end,
                                 start_time_start;
extern off_t                     search_file_size;
extern double                    search_duration;
extern char                      **file_pattern;
extern struct alda_cache_data    *ocache;
extern struct alda_position_list **opl;
extern struct log_file_data      output;
extern struct alda_odata         olog;
extern struct jid_data           jidd;

/* Local function prototypes. */
static int                       get_dir_id(unsigned int);


/*######################### check_output_line() #########################*/
int
check_output_line(char         *line,
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
      if ((ocache[output.current_file_no].mpc != ocache[output.current_file_no].pc) &&
          (opl[output.current_file_no][ocache[output.current_file_no].pc].gotcha == YES))
      {
         ocache[output.current_file_no].pc++;
         return(DATA_ALREADY_SHOWN);
      }
   }
   olog.output_time = (time_t)str2timet(line, NULL, 16);
   if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE))
   {
      if (ocache[output.current_file_no].mpc == ocache[output.current_file_no].pc)
      {
         opl[output.current_file_no][ocache[output.current_file_no].pc].time = olog.output_time;
         opl[output.current_file_no][ocache[output.current_file_no].pc].gotcha = NO;
# ifdef CACHE_DEBUG
         opl[output.current_file_no][ocache[output.current_file_no].pc].filename[0] = '\0';
# endif
         ocache[output.current_file_no].mpc++;
      }
      ocache[output.current_file_no].pc++;
   }
   if ((olog.output_time >= start_time_start) &&
       (((prev_file_name != NULL) && (mode & ALDA_FORWARD_MODE)) ||
        (start_time_end == 0) || (olog.output_time < start_time_end)))
   {
      register int i = 0;

      while ((*(ptr + i) != ' ') && (i < MAX_REAL_HOSTNAME_LENGTH) &&
             (*(ptr + i) != '\0'))
      {
         olog.alias_name[i] = *(ptr + i);
         i++;
      }
      if (*(ptr + i) == ' ')
      {
         char tmp_char;

         olog.alias_name[i] = '\0';
         olog.alias_name_length = i;
         if (*(ptr + max_hostname_length + 2) == ' ')
         {
            if (*(ptr + max_hostname_length + 4) == ' ')
            {
               olog.output_type = (int)(*(ptr + max_hostname_length + 1) - '0');
               olog.current_toggle = (int)(*(ptr + max_hostname_length + 3) - '0');
               tmp_char = *(ptr + max_hostname_length + 5);
               ptr += max_hostname_length + 6;
            }
            else
            {
               olog.current_toggle = (int)(*(ptr + max_hostname_length + 1) - '0');
               tmp_char = *(ptr + max_hostname_length + 3);
               ptr += max_hostname_length + 4;
            }
         }
         else
         {
            olog.current_toggle = -1;
            tmp_char = *(ptr + max_hostname_length + 1);
            ptr += max_hostname_length + 2;
         }
         if ((tmp_char >= '0') && (tmp_char <= '9'))
         {
            tmp_char -= '0';
         }
         else
         {
            tmp_char = tmp_char - 'a' + 10;
         }
         if (*ptr == SEPARATOR_CHAR)
         {
            switch (tmp_char)
            {
               case ALDA_FTP    : olog.protocol = ALDA_FTP_FLAG;
                                  break;
               case ALDA_LOC    : olog.protocol = ALDA_LOC_FLAG;
                                  break;
               case ALDA_EXEC   : olog.protocol = ALDA_EXEC_FLAG;
                                  break;
               case ALDA_SMTP   : olog.protocol = ALDA_SMTP_FLAG;
                                  break;
               case ALDA_MAP    : olog.protocol = ALDA_MAP_FLAG;
                                  break;
               case ALDA_DFAX   : olog.protocol = ALDA_DFAX_FLAG;
                                  break;
               case ALDA_DE_MAIL: olog.protocol = ALDA_DE_MAIL_FLAG;
                                  break;
               case ALDA_SCP    : olog.protocol = ALDA_SCP_FLAG;
                                  break;
               case ALDA_WMO    : olog.protocol = ALDA_WMO_FLAG;
                                  break;
               case ALDA_HTTP   : olog.protocol = ALDA_HTTP_FLAG;
                                  break;
               case ALDA_FTPS   : olog.protocol = ALDA_FTPS_FLAG;
                                  break;
               case ALDA_HTTPS  : olog.protocol = ALDA_HTTPS_FLAG;
                                  break;
               case ALDA_SFTP   : olog.protocol = ALDA_SFTP_FLAG;
                                  break;
               default          : olog.protocol = ~0u;
                                  break;
            }
            ptr++;
            i = 0;
            while ((*(ptr + i) != SEPARATOR_CHAR) &&
                   (i < MAX_FILENAME_LENGTH) && (*(ptr + i) != '\0'))
            {
               olog.local_filename[i] = *(ptr + i);
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

               olog.local_filename[i] = '\0';
               olog.local_filename_length = i;
# ifdef CACHE_DEBUG
               (void)strcpy(opl[output.current_file_no][ocache[output.current_file_no].pc - 1].filename,
                            olog.local_filename);
# endif

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
                     if (prev_filename_length == olog.local_filename_length)
                     {
                        ret = my_strcmp(local_pattern[j], olog.local_filename);
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
                                  olog.local_filename, NULL);
                  }
                  if (ret == 0)
                  {
                     /*
                      * This file is wanted, so lets store the rest
                      * and/or do more checks.
                      */
                     ptr += i + 1;

                     /* Store remote rename part. */
                     i = 0;
                     while ((*(ptr + i) != SEPARATOR_CHAR) &&
                            (i < MAX_PATH_LENGTH) &&
                            (*(ptr + i) != '\0'))
                     {
                        olog.remote_name[i] = *(ptr + i);
                        i++;
                     }
                     if (*(ptr + i) == SEPARATOR_CHAR)
                     {
                        olog.remote_name[i] = '\0';
                        olog.remote_name_length = i;
                        ptr += i + 1;
                     }
                     else
                     {
                        olog.local_filename[0] = '\0';
                        olog.remote_name[0] = '\0';
                        olog.alias_name[0] = '\0';
                        olog.real_hostname[0] = '\0';
                        olog.local_filename_length = 0;
                        olog.alias_name_length = 0;
                        olog.current_toggle = 0;
                        olog.protocol = 0;
# ifndef HAVE_GETLINE
                        while (*(ptr + i) != '\0')
                        {
                           i++;
                        }
                        output.bytes_read += (ptr + i - line);
# endif

                        return(INCORRECT);
                     }

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
                        olog.file_size = (off_t)str2offt(ptr, NULL, 16);
                        if (((search_file_size_flag & SEARCH_OUTPUT_LOG) == 0) ||
                            ((search_file_size == -1) ||
                             ((gt_lt_sign == EQUAL_SIGN) &&
                              (olog.file_size == search_file_size)) ||
                             ((gt_lt_sign == LESS_THEN_SIGN) &&
                              (olog.file_size < search_file_size)) ||
                             ((gt_lt_sign == GREATER_THEN_SIGN) &&
                              (olog.file_size > search_file_size))))
                        {
                           ptr += i + 1;

                           /* Store transmission time. */
                           i = 0;
                           while ((*(ptr + i) != SEPARATOR_CHAR) &&
                                  (*(ptr + i) != '\0') &&
                                  (i < (MAX_TIME_T_HEX_LENGTH + 3)))
                           {
                              i++;
                           }
                           if (*(ptr + i) == SEPARATOR_CHAR)
                           {
                              *(ptr + i) = '\0';
                              olog.transmission_time = strtod(ptr, NULL);
                              if (((search_duration_flag & SEARCH_OUTPUT_LOG) == 0) ||
                                  (((gt_lt_sign_duration == EQUAL_SIGN) &&
                                    (olog.transmission_time == search_duration)) ||
                                   ((gt_lt_sign_duration == LESS_THEN_SIGN) &&
                                    (olog.transmission_time < search_duration)) ||
                                   ((gt_lt_sign_duration == GREATER_THEN_SIGN) &&
                                    (olog.transmission_time > search_duration)) ||
                                   ((gt_lt_sign_duration == NOT_SIGN) &&
                                    (olog.transmission_time != search_duration))))
                              {
                                 olog.send_start_time = olog.output_time - (time_t)olog.transmission_time;
                                 ptr += i + 1;

                                 if (olog.current_toggle != -1)
                                 {
                                    /* Store number of retries. */
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
                                       olog.retries = (unsigned int)strtoul(ptr, NULL, 16);
                                       ptr += i + 1;
                                    }
                                    else
                                    {
                                       olog.local_filename[0] = '\0';
                                       olog.remote_name[0] = '\0';
                                       olog.alias_name[0] = '\0';
                                       olog.real_hostname[0] = '\0';
                                       olog.transmission_time = 0.0;
                                       olog.file_size = -1;
                                       olog.send_start_time = -1L;
                                       olog.local_filename_length = 0;
                                       olog.alias_name_length = 0;
                                       olog.current_toggle = 0;
                                       olog.protocol = 0;
# ifndef HAVE_GETLINE
                                       while (*(ptr + i) != '\0')
                                       {
                                          i++;
                                       }
                                       output.bytes_read += (ptr + i - line);
# endif

                                       return(INCORRECT);
                                    }
                                 }

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
                                    if ((show_output_type & SHOW_NORMAL_RECEIVED) &&
                                        (olog.output_type == OT_NORMAL_RECEIVED))
                                    {
                                       olog.dir_id = (unsigned int)strtoul(ptr, NULL, 16);
                                       olog.job_id = 0;
# ifndef HAVE_GETLINE
                                       while (*(ptr + i) != '\0')
                                       {
                                          i++;
                                       }
                                       output.bytes_read += (ptr + i - line);
# endif

                                       return(SUCCESS);
                                    }
                                    if (((show_output_type & SHOW_NORMAL_DELIVERED) &&
                                         (olog.output_type == OT_NORMAL_DELIVERED))
# if defined(_WITH_DE_MAIL_SUPPORT) && !defined(_CONFIRMATION_LOG)
                                        || ((show_output_type & SHOW_CONF_OF_DISPATCH) &&
                                            (olog.output_type == OT_CONF_OF_DISPATCH))
                                        || ((show_output_type & SHOW_CONF_OF_RECEIPT) &&
                                            (olog.output_type == OT_CONF_OF_RECEIPT))
                                        || ((show_output_type & SHOW_CONF_OF_RETRIEVE) &&
                                            (olog.output_type == OT_CONF_OF_RETRIEVE))
                                        || ((show_output_type & SHOW_CONF_TIMEUP) &&
                                            (olog.output_type == OT_CONF_TIMEUP))
# endif
                                       )
                                    {
                                       olog.job_id = (unsigned int)strtoul(ptr, NULL, 16);
                                       (void)get_recipient(olog.job_id);
                                       if (((search_job_id == 0) ||
                                            (olog.job_id == search_job_id)) &&
                                           ((prev_job_id == 0) ||
                                            (olog.job_id == prev_job_id)) &&
                                           (((search_dir_alias_counter == 0) &&
                                             (search_dir_id_counter == 0) &&
                                             (search_dir_name_counter == 0)) ||
                                            (get_dir_id(olog.job_id) == INCORRECT) ||
                                            (check_did(olog.dir_id) == SUCCESS)))
                                       {
                                          ptr += i + 1;

                                          /* Get job creation time. */
                                          i = 0;
                                          while ((*(ptr + i) != '_') &&
                                                 (i < MAX_TIME_T_HEX_LENGTH) &&
                                                 (*(ptr + i) != '\0'))
                                          {
                                             i++;
                                          }
                                          if (*(ptr + i) == '_')
                                          {
                                             *(ptr + i) = '\0';
                                             olog.job_creation_time = (time_t)str2timet(ptr, NULL, 16);
                                             if (((prev_log_time == 0) || (olog.job_creation_time == prev_log_time)) &&
                                                 ((start_time_end == 0) || (olog.job_creation_time < start_time_end)))
                                             {
                                                ptr += i + 1;
                                                i = 0;
                                                while ((*(ptr + i) != '_') &&
                                                       (i < MAX_INT_HEX_LENGTH) &&
                                                       (*(ptr + i) != '\0'))
                                                {
                                                   i++;
                                                }
                                                if (*(ptr + i) == '_')
                                                {
                                                   *(ptr + i) = '\0';
                                                   olog.unique_number = (unsigned int)strtoul(ptr, NULL, 16);

                                                   if (((prev_unique_number == NULL) ||
                                                        (olog.unique_number == *prev_unique_number)) &&
                                                       ((search_unique_number == 0) ||
                                                        (search_unique_number == olog.unique_number)))
                                                   {
                                                      ptr += i + 1;
                                                      i = 0;
                                                      while ((*(ptr + i) != SEPARATOR_CHAR) &&
                                                             (*(ptr + i) != ' ') &&
                                                             (*(ptr + i) != '\0') &&
                                                             (*(ptr + i) != '\n') &&
                                                             (i < MAX_INT_HEX_LENGTH))
                                                      {
                                                         i++;
                                                      }
                                                      if ((*(ptr + i) == SEPARATOR_CHAR) ||
                                                          (*(ptr + i) == ' ') ||
                                                          (*(ptr + i) == '\0') ||
                                                          (*(ptr + i) == '\n'))
                                                      {
                                                         tmp_char = *(ptr + i);
                                                         *(ptr + i) = '\0';
                                                         olog.split_job_counter = (unsigned int)strtoul(ptr, NULL, 16);

                                                         if ((prev_split_job_counter == NULL) ||
                                                             (olog.split_job_counter == *prev_split_job_counter))
                                                         {
                                                            if (tmp_char == ' ')
                                                            {
                                                               ptr += i + 1;
                                                               i = 0;
                                                               while ((*(ptr + i) != SEPARATOR_CHAR) &&
                                                                      (*(ptr + i) != '\n') &&
                                                                      (*(ptr + i) != '\0') &&
                                                                      (i < MAX_MAIL_ID_LENGTH))
                                                               {
                                                                  olog.mail_id[i] = *(ptr + i);
                                                                  i++;
                                                               }
                                                               olog.mail_id[i] = '\0';
                                                               olog.mail_id_length = i;
                                                               if (i == MAX_MAIL_ID_LENGTH)
                                                               {
                                                                  while ((*(ptr + i) != SEPARATOR_CHAR) &&
                                                                         (*(ptr + i) != '\n') &&
                                                                         (*(ptr + i) != '\0'))
                                                                  {
                                                                     i++;
                                                                  }
                                                               }
                                                               if ((*(ptr + i)) == SEPARATOR_CHAR)
                                                               {
                                                                   /* To get the archive data. */
                                                                   tmp_char = SEPARATOR_CHAR;
                                                               }
                                                            }
                                                            if (tmp_char == SEPARATOR_CHAR)
                                                            {
                                                               ptr += i + 1;

                                                               /* Store remote rename part. */
                                                               i = 0;
                                                               while ((*(ptr + i) != '\n') &&
                                                                      (*(ptr + i) != '\0') &&
                                                                      (i < MAX_PATH_LENGTH))
                                                               {
                                                                  olog.archive_dir[i] = *(ptr + i);
                                                                  i++;
                                                               }
                                                               olog.archive_dir[i] = '\0';
                                                               olog.archive_dir_length = i;
                                                            }
                                                            else
                                                            {
                                                               olog.archive_dir[0] = '\0';
                                                               olog.archive_dir_length = 0;
                                                            }
# ifndef HAVE_GETLINE
                                                            while (*(ptr + i) != '\0')
                                                            {
                                                               i++;
                                                            }
                                                            output.bytes_read += (ptr + i - line);
# endif
                                                            if (verbose > 2)
                                                            {
                                                               if (olog.remote_name[0] == '\0')
                                                               {
# if SIZEOF_TIME_T == 4
                                                                  (void)printf("%06ld DEBUG 3: [OUTPUT] %s %s %x %x %x (%u)\n",
# else
                                                                  (void)printf("%06lld DEBUG 3: [OUTPUT] %s %s %x %x %x (%u)\n",
# endif
                                                                               (pri_time_t)(time(NULL) - start),
                                                                               olog.local_filename,
                                                                               olog.alias_name,
                                                                               olog.job_id,
                                                                               olog.unique_number,
                                                                               olog.split_job_counter,
                                                                               olog.retries);
                                                               }
                                                               else
                                                               {
# if SIZEOF_TIME_T == 4
                                                                  (void)printf("%06ld DEBUG 3: [OUTPUT] %s->%s %s %x %x %x (%u)\n",
# else
                                                                  (void)printf("%06lld DEBUG 3: [OUTPUT] %s->%s %s %x %x %x (%u)\n",
# endif
                                                                               (pri_time_t)(time(NULL) - start),
                                                                               olog.local_filename,
                                                                               olog.remote_name,
                                                                               olog.alias_name,
                                                                               olog.job_id,
                                                                               olog.unique_number,
                                                                               olog.split_job_counter,
                                                                               olog.retries);
                                                               }
                                                            }

                                                            return(SUCCESS);
                                                         }
                                                         else
                                                         {
                                                            olog.local_filename[0] = '\0';
                                                            olog.remote_name[0] = '\0';
                                                            olog.alias_name[0] = '\0';
                                                            olog.real_hostname[0] = '\0';
                                                            olog.transmission_time = 0.0;
                                                            olog.file_size = -1;
                                                            olog.job_creation_time = -1L;
                                                            olog.send_start_time = -1L;
                                                            olog.local_filename_length = 0;
                                                            olog.remote_name_length = 0;
                                                            olog.alias_name_length = 0;
                                                            olog.current_toggle = 0;
                                                            olog.job_id = 0;
                                                            olog.unique_number = 0;
                                                            olog.protocol = 0;
                                                            olog.retries = 0;
# ifndef HAVE_GETLINE
                                                            while (*(ptr + i) != '\0')
                                                            {
                                                               i++;
                                                            }
                                                            output.bytes_read += (ptr + i - line);
# endif

                                                            return(NOT_WANTED);
                                                         }
                                                      }
                                                      else
                                                      {
                                                         time_t now;

                                                         now = time(NULL);
                                                         (void)fprintf(stderr,
                                                                       "[%s] Unable to store the unique number since it is to large. (%s %d)\n",
                                                                       ctime(&now), __FILE__, __LINE__);
                                                         (void)fprintf(stderr, "line: %s", line);
                                                         olog.local_filename[0] = '\0';
                                                         olog.remote_name[0] = '\0';
                                                         olog.alias_name[0] = '\0';
                                                         olog.real_hostname[0] = '\0';
                                                         olog.transmission_time = 0.0;
                                                         olog.file_size = -1;
                                                         olog.job_creation_time = -1L;
                                                         olog.send_start_time = -1L;
                                                         olog.local_filename_length = 0;
                                                         olog.remote_name_length = 0;
                                                         olog.alias_name_length = 0;
                                                         olog.current_toggle = 0;
                                                         olog.job_id = 0;
                                                         olog.unique_number = 0;
                                                         olog.protocol = 0;
                                                         olog.retries = 0;
# ifndef HAVE_GETLINE
                                                         while (*(ptr + i) != '\0')
                                                         {
                                                            i++;
                                                         }
                                                         output.bytes_read += (ptr + i - line);
# endif

                                                         return(INCORRECT);
                                                      }
                                                   }
                                                   else
                                                   {
                                                      olog.local_filename[0] = '\0';
                                                      olog.remote_name[0] = '\0';
                                                      olog.alias_name[0] = '\0';
                                                      olog.real_hostname[0] = '\0';
                                                      olog.transmission_time = 0.0;
                                                      olog.file_size = -1;
                                                      olog.job_creation_time = -1L;
                                                      olog.send_start_time = -1L;
                                                      olog.local_filename_length = 0;
                                                      olog.remote_name_length = 0;
                                                      olog.alias_name_length = 0;
                                                      olog.current_toggle = 0;
                                                      olog.job_id = 0;
                                                      olog.unique_number = 0;
                                                      olog.protocol = 0;
                                                      olog.retries = 0;
# ifndef HAVE_GETLINE
                                                      while (*(ptr + i) != '\0')
                                                      {
                                                         i++;
                                                      }
                                                      output.bytes_read += (ptr + i - line);
# endif

                                                      return(NOT_WANTED);
                                                   }
                                                }
                                                else
                                                {
                                                   time_t now;

                                                   now = time(NULL);
                                                   if (i == MAX_INT_HEX_LENGTH)
                                                   {
                                                      (void)fprintf(stderr,
                                                                    "[%s] Unable to store the unique number since it is to large. (%s %d)\n",
                                                                    ctime(&now),
                                                                    __FILE__, __LINE__);
                                                      (void)fprintf(stderr, "line: %s", line);
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
                                                                    "[%s] Unable to store the unique number because end was not found. (%s %d)\n",
                                                                    ctime(&now),
                                                                    __FILE__, __LINE__);
                                                      (void)fprintf(stderr, "line: %s", line);
                                                   }
                                                   olog.local_filename[0] = '\0';
                                                   olog.remote_name[0] = '\0';
                                                   olog.alias_name[0] = '\0';
                                                   olog.real_hostname[0] = '\0';
                                                   olog.transmission_time = 0.0;
                                                   olog.file_size = -1;
                                                   olog.job_creation_time = -1L;
                                                   olog.send_start_time = -1L;
                                                   olog.local_filename_length = 0;
                                                   olog.remote_name_length = 0;
                                                   olog.alias_name_length = 0;
                                                   olog.current_toggle = 0;
                                                   olog.job_id = 0;
                                                   olog.protocol = 0;
                                                   olog.retries = 0;
# ifndef HAVE_GETLINE
                                                   output.bytes_read += (ptr + i - line);
# endif

                                                   return(INCORRECT);
                                                }
                                             }
                                             else
                                             {
                                                olog.local_filename[0] = '\0';
                                                olog.remote_name[0] = '\0';
                                                olog.alias_name[0] = '\0';
                                                olog.real_hostname[0] = '\0';
                                                olog.transmission_time = 0.0;
                                                olog.file_size = -1;
                                                olog.job_creation_time = -1L;
                                                olog.send_start_time = -1L;
                                                olog.local_filename_length = 0;
                                                olog.remote_name_length = 0;
                                                olog.alias_name_length = 0;
                                                olog.current_toggle = 0;
                                                olog.job_id = 0;
                                                olog.protocol = 0;
                                                olog.retries = 0;
# ifndef HAVE_GETLINE
                                                while (*(ptr + i) != '\0')
                                                {
                                                   i++;
                                                }
                                                output.bytes_read += (ptr + i - line);
# endif

                                                return(NOT_WANTED);
                                             }
                                          }
                                          else
                                          {
                                             time_t now;

                                             now = time(NULL);
                                             if (i == MAX_TIME_T_HEX_LENGTH)
                                             {
                                                (void)fprintf(stderr,
                                                              "[%s] Unable to store the input time since it is to large. (%s %d)\n",
                                                              ctime(&now), __FILE__, __LINE__);
                                                (void)fprintf(stderr, "line: %s", line);
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
                                                              "[%s] Unable to store the input time because end was not found. (%s %d)\n",
                                                              ctime(&now), __FILE__, __LINE__);
                                                (void)fprintf(stderr, "line: %s", line);
                                             }
                                             olog.local_filename[0] = '\0';
                                             olog.remote_name[0] = '\0';
                                             olog.alias_name[0] = '\0';
                                             olog.real_hostname[0] = '\0';
                                             olog.transmission_time = 0.0;
                                             olog.file_size = -1;
                                             olog.send_start_time = -1L;
                                             olog.local_filename_length = 0;
                                             olog.remote_name_length = 0;
                                             olog.alias_name_length = 0;
                                             olog.current_toggle = 0;
                                             olog.job_id = 0;
                                             olog.protocol = 0;
                                             olog.retries = 0;
# ifndef HAVE_GETLINE
                                             output.bytes_read += (ptr + i - line);
# endif

                                             return(INCORRECT);
                                          }
                                       }
                                       else
                                       {
                                          olog.local_filename[0] = '\0';
                                          olog.remote_name[0] = '\0';
                                          olog.alias_name[0] = '\0';
                                          olog.real_hostname[0] = '\0';
                                          olog.transmission_time = 0.0;
                                          olog.file_size = -1;
                                          olog.send_start_time = -1L;
                                          olog.local_filename_length = 0;
                                          olog.remote_name_length = 0;
                                          olog.alias_name_length = 0;
                                          olog.current_toggle = 0;
                                          olog.job_id = 0;
                                          olog.protocol = 0;
                                          olog.retries = 0;
# ifndef HAVE_GETLINE
                                          while (*(ptr + i) != '\0')
                                          {
                                             i++;
                                          }
                                          output.bytes_read += (ptr + i - line);
# endif

                                          return(NOT_WANTED);
                                       }
                                    }
                                 }
                                 else
                                 {
                                    time_t now;

                                    now = time(NULL);
                                    if (i == MAX_INT_HEX_LENGTH)
                                    {
                                       (void)fprintf(stderr,
                                                     "[%s] Unable to store the job ID since it is to large. (%s %d)\n",
                                                     ctime(&now), __FILE__, __LINE__);
                                       (void)fprintf(stderr, "line: %s", line);
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
                                                     "[%s] Unable to store the job ID because end was not found. (%s %d)\n",
                                                     ctime(&now), __FILE__, __LINE__);
                                       (void)fprintf(stderr, "line: %s", line);
                                    }
                                    olog.local_filename[0] = '\0';
                                    olog.remote_name[0] = '\0';
                                    olog.alias_name[0] = '\0';
                                    olog.real_hostname[0] = '\0';
                                    olog.transmission_time = 0.0;
                                    olog.file_size = -1;
                                    olog.send_start_time = -1L;
                                    olog.local_filename_length = 0;
                                    olog.remote_name_length = 0;
                                    olog.alias_name_length = 0;
                                    olog.current_toggle = 0;
                                    olog.protocol = 0;
# ifndef HAVE_GETLINE
                                    output.bytes_read += (ptr + i - line);
# endif

                                    return(INCORRECT);
                                 }
                              }
                              else
                              {
                                 olog.local_filename[0] = '\0';
                                 olog.remote_name[0] = '\0';
                                 olog.alias_name[0] = '\0';
                                 olog.real_hostname[0] = '\0';
                                 olog.transmission_time = 0.0;
                                 olog.file_size = -1;
                                 olog.send_start_time = -1L;
                                 olog.local_filename_length = 0;
                                 olog.remote_name_length = 0;
                                 olog.alias_name_length = 0;
                                 olog.current_toggle = 0;
                                 olog.protocol = 0;
# ifndef HAVE_GETLINE
                                 output.bytes_read += (ptr + i - line);
# endif

                                 return(NOT_WANTED);
                              }
                           }
                           else
                           {
                              time_t now;

                              now = time(NULL);
                              if (i == (MAX_TIME_T_HEX_LENGTH + 3))
                              {
                                 (void)fprintf(stderr,
                                               "[%s] Unable to store the transmission time since it is to large. (%s %d)\n",
                                               ctime(&now), __FILE__, __LINE__);
                                 (void)fprintf(stderr, "line: %s", line);
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
                                               "[%s] Unable to store the transmission time because end was not found. (%s %d)\n",
                                               ctime(&now), __FILE__, __LINE__);
                                 (void)fprintf(stderr, "line: %s", line);
                              }
                              olog.local_filename[0] = '\0';
                              olog.remote_name[0] = '\0';
                              olog.alias_name[0] = '\0';
                              olog.real_hostname[0] = '\0';
                              olog.file_size = -1;
                              olog.local_filename_length = 0;
                              olog.remote_name_length = 0;
                              olog.alias_name_length = 0;
                              olog.current_toggle = 0;
                              olog.protocol = 0;
# ifndef HAVE_GETLINE
                              output.bytes_read += (ptr + i - line);
# endif

                              return(INCORRECT);
                           }
                        }
                        else
                        {
                           olog.local_filename[0] = '\0';
                           olog.remote_name[0] = '\0';
                           olog.alias_name[0] = '\0';
                           olog.real_hostname[0] = '\0';
                           olog.file_size = -1;
                           olog.local_filename_length = 0;
                           olog.remote_name_length = 0;
                           olog.alias_name_length = 0;
                           olog.current_toggle = 0;
                           olog.protocol = 0;
# ifndef HAVE_GETLINE
                           while (*(ptr + i) != '\0')
                           {
                              i++;
                           }
                           output.bytes_read += (ptr + i - line);
# endif

                           return(NOT_WANTED);
                        }
                     }
                     else
                     {
                        time_t now;

                        now = time(NULL);
                        if (i == MAX_OFF_T_HEX_LENGTH)
                        {
                           (void)fprintf(stderr,
                                         "[%s] Unable to store the file size since it is to large. (%s %d)\n",
                                         ctime(&now), __FILE__, __LINE__);
                           (void)fprintf(stderr, "line: %s", line);
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
                                         "[%s] Unable to store the file size because end was not found. (%s %d)\n",
                                         ctime(&now), __FILE__, __LINE__);
                           (void)fprintf(stderr, "line: %s", line);
                        }
                        olog.local_filename[0] = '\0';
                        olog.remote_name[0] = '\0';
                        olog.alias_name[0] = '\0';
                        olog.real_hostname[0] = '\0';
                        olog.local_filename_length = 0;
                        olog.remote_name_length = 0;
                        olog.alias_name_length = 0;
                        olog.current_toggle = 0;
                        olog.protocol = 0;
# ifndef HAVE_GETLINE
                        output.bytes_read += (ptr + i - line);
# endif

                        return(INCORRECT);
                     }
                  }
                  else if (ret == 1)
                       {
                          /*
                           * This file is definitly not wanted,
                           * so let us just ignore it.
                           */
                          olog.local_filename[0] = '\0';
                          olog.alias_name[0] = '\0';
                          olog.real_hostname[0] = '\0';
                          olog.local_filename_length = 0;
                          olog.alias_name_length = 0;
                          olog.current_toggle = 0;
                          olog.protocol = 0;
# ifndef HAVE_GETLINE
                          while (*(ptr + i) != '\0')
                          {
                             i++;
                          }
                          output.bytes_read += (ptr + i - line);
# endif

                          return(NOT_WANTED);
                       }
               } /* for (j = 0; j < local_pattern_counter; j++) */
            }
            else
            {
               time_t now;

               now = time(NULL);
               if (i == MAX_FILENAME_LENGTH)
               {
                  (void)fprintf(stderr,
                                "[%s] Unable to store the local filename since it is to large. (%s %d)\n",
                                ctime(&now), __FILE__, __LINE__);
                  (void)fprintf(stderr, "line: %s", line);
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
                                "[%s] Unable to store the local filename because end was not found. (%s %d)\n",
                                ctime(&now), __FILE__, __LINE__);
                  (void)fprintf(stderr, "line: %s", line);
               }
               olog.local_filename[0] = '\0';
               olog.alias_name[0] = '\0';
               olog.real_hostname[0] = '\0';
               olog.local_filename_length = 0;
               olog.alias_name_length = 0;
               olog.current_toggle = 0;
               olog.protocol = 0;
# ifndef HAVE_GETLINE
               output.bytes_read += (ptr + i - line);
# endif

               return(INCORRECT);
            }
         }
         else
         {
            time_t now;

            now = time(NULL);
            (void)fprintf(stderr,
                          "[%s] Unable to locate the local filename. (%s %d)\n",
                          ctime(&now), __FILE__, __LINE__);
            (void)fprintf(stderr, "line: %s", line);
            olog.alias_name[0] = '\0';
            olog.alias_name_length = 0;
            olog.current_toggle = 0;
            olog.protocol = 0;
# ifndef HAVE_GETLINE
            while (*(ptr + i) != '\0')
            {
               i++;
            }
            output.bytes_read += (ptr + i - line);
# endif

            return(INCORRECT);
         }
      }
      else
      {
         time_t now;

         now = time(NULL);
         if (i == MAX_REAL_HOSTNAME_LENGTH)
         {
            (void)fprintf(stderr,
                          "[%s] Unable to store the alias name since it is to large. (%s %d)\n",
                          ctime(&now), __FILE__, __LINE__);
            (void)fprintf(stderr, "line: %s", line);
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
                          "[%s] Unable to store the alias name because end was not found. (%s %d)\n",
                          ctime(&now), __FILE__, __LINE__);
            (void)fprintf(stderr, "line: %s", line);
         }
         olog.alias_name[0] = '\0';
         olog.current_toggle = 0;
         olog.protocol = 0;
# ifndef HAVE_GETLINE
         output.bytes_read += (ptr + i - line);
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
          (start_time_end != 0) && (olog.output_time > start_time_end))
      {
         return(SEARCH_TIME_UP);
      }
# ifndef HAVE_GETLINE
      while (*(ptr + i) != '\0')
      {
         i++;
      }
      output.bytes_read += (ptr + i - line);
# endif
   }

   return(NOT_WANTED);
}


/*+++++++++++++++++++++++++++++ get_dir_id() ++++++++++++++++++++++++++++*/
static int
get_dir_id(unsigned int job_id)
{
#ifdef WITH_AFD_MON
   if (jidd.jd != NULL)
   {
#endif
      if ((jidd.prev_pos != -1) && (job_id == jidd.jd[jidd.prev_pos].job_id))
      {
         olog.dir_id = jidd.jd[jidd.prev_pos].dir_id;
         return(SUCCESS);
      }
      else
      {
         int i;

         for (i = 0; i < jidd.no_of_job_ids; i++)
         {
            if (job_id == jidd.jd[i].job_id)
            {
               olog.dir_id = jidd.jd[i].dir_id;
               jidd.prev_pos = i;
               return(SUCCESS);
            }
         }
      }
#ifdef WITH_AFD_MON
   }
   else if (jidd.ajl != NULL)
        {
           if ((jidd.prev_pos != -1) &&
               (job_id == jidd.ajl[jidd.prev_pos].job_id))
           {
              olog.dir_id = jidd.ajl[jidd.prev_pos].dir_id;
              return(SUCCESS);
           }
           else
           {
              int i;

              for (i = 0; i < jidd.no_of_job_ids; i++)
              {
                 if (job_id == jidd.ajl[i].job_id)
                 {
                    olog.dir_id = jidd.ajl[i].dir_id;
                    jidd.prev_pos = i;
                    return(SUCCESS);
                 }
              }
           }
        }
#endif
   olog.dir_id = -1;
   jidd.prev_pos = -1;

   return(INCORRECT);
}
