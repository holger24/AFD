/*
 *  format_info.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2017 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   format_info - puts data from a structure into a human readable
 **                 form
 **
 ** SYNOPSIS
 **   void format_send_info(char **text)
 **   void format_receive_info(char **text, int with_alda_data)
 **
 ** DESCRIPTION
 **   This function format_send_info() formats data from the global
 **   structure info_data to the following form:
 **         DIR_CONFIG : /home/afd/etc/DIR_CONFIG
 **         Local name : xxxxxxx.xx
 **         Remote name: XXXyyyy.ZZ
 **         File size  : 34234 Bytes
 **         Output time: Sun Sep 14 07:54:10 2008
 **         Trans time : 12.05
 **         Directory  : /aaa/bbb/ccc
 **         Dir-Alias  : ccc_dir
 **         Dir-ID     : 4a231f1
 **         Filter     : filter_1
 **                      filter_2
 **                      filter_n
 **         Recipient  : ftp://donald:secret@hollywood//home/user
 **         AMG-options: option_1
 **                      option_2
 **                      option_n
 **         FD-options : option_1
 **                      option_2
 **                      option_n
 **         Priority   : 5
 **         Job-ID     : 4f2ac21
 **         Retries    : 2
 **         Archive dir: hollywood/donald/0/862868443_491
 **         Unique name: 4249397a_4_0
 **
 **   format_receive_info() shows data in the following format:
 **         File name    : xxxxxxx.xx
 **         File size    : 2376 Bytes
 **         Receive time : Mon Sep 27 12:45:39 2004
 **         Trans time   : 2.25
 **         Directory    : /aaa/bbb/ccc
 **         Dir-Alias    : ccc_dir
 **         Dir-ID       : 4a231f1
 **         =====================================================
 **         Filter       : filter_1
 **                        filter_2
 **                        filter_n
 **         Recipient    : ftp://donald:secret@hollywood//home/user
 **         AMG-options  : option_1
 **                        option_2
 **                        option_n
 **         FD-options   : option_1
 **                        option_2
 **                        option_n
 **         Priority     : 5
 **         Job-ID       : d88f540e
 **         DIR_CONFIG   : /home/afd/etc/DIR_CONFIG
 **         -----------------------------------------------------
 **                                  .
 **                                  .
 **                                 etc.
 **
 ** RETURN VALUES
 **   Returns the formated text.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   04.05.1997 H.Kiehl Created
 **   15.01.1998 H.Kiehl Support for remote file name.
 **   01.07.2001 H.Kiehl Check if archive is still there.
 **                      Added directory options.
 **   14.09.2008 H.Kiehl Added output time, directory ID and alias.
 **   29.01.2009 H.Kiehl Resize text buffer when it gets to small.
 **   23.12.2013 H.Kiehl Added retries.
 **   13.10.2014 H.Kiehl Added support when we fetch files.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* sprintf()                          */
#include <stdlib.h>                /* malloc(), realloc()                */
#include <time.h>                  /* ctime()                            */
#include <unistd.h>                /* F_OK                               */
#include <errno.h>
#include "show_olog.h"
#include "dr_str.h"

/* External global variables. */
extern int                   acd_counter,
                             max_x,
                             max_y;
extern char                  *p_work_dir;
extern struct info_data      id;
extern struct alda_call_data *acd;
extern struct sol_perm       perm;


/*######################### format_send_info() ##########################*/
void
format_send_info(char **text)
{
   int buffer_length = 8192,
       count,
       i = 0,
       length;

   if ((*text = malloc(buffer_length)) == NULL)
   {
      (void)xrec(FATAL_DIALOG, "Failed to malloc() %d bytes : %s (%s %d)",
                 buffer_length, strerror(errno), __FILE__, __LINE__);
      return;
   }
   max_x = sprintf(*text, "DIR_CONFIG : %s\n", id.dir_config_file);
   length = max_x;
   max_y = 1;
   count = sprintf(*text + length, "Local name : ");
   while (id.local_file_name[i] != '\0')
   {
      if (id.local_file_name[i] < ' ')
      {
         *(*text + length + count) = '?';
      }
      else
      {
         *(*text + length + count) = id.local_file_name[i];
      }
      i++; count++;
   }
   *(*text + length + count) = '\n';
   count++;
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y++;
   if (id.remote_file_name[0] != '\0')
   {
      count = sprintf(*text + length, "Remote name: ");
      i = 0;
      while (id.remote_file_name[i] != '\0')
      {
         if (id.remote_file_name[i] < ' ')
         {
            *(*text + length + count) = '?';
         }
         else
         {
            *(*text + length + count) = id.remote_file_name[i];
         }
         i++; count++;
      }
      *(*text + length + count) = '\n';
      count++;
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
   }
   if (id.file_size[0] != '\0')
   {
      count = sprintf(*text + length, "File size  : %s Bytes\n", id.file_size);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
   }
   count = sprintf(*text + length, "Output time: %s", ctime(&id.date_send));
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y++;
   if (id.trans_time[0] != '\0')
   {
      count = sprintf(*text + length, "Trans time : %s sec\n", id.trans_time);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
   }

   if (id.no_of_files > 0)
   {
      char *p_file;

      count = sprintf(*text + length, "Directory  : %s\n", id.dir);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      count = sprintf(*text + length, "Dir-Alias  : %s\n", id.d_o.dir_alias);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      count = sprintf(*text + length, "Dir-ID     : %x\n", id.dir_id);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y += 3;
      if (id.d_o.url[0] != '\0')
      {
         if (perm.view_passwd == YES)
         {
            insert_passwd(id.d_o.url);
         }
         count = sprintf(*text + length, "DIR-URL    : %s\n", id.d_o.url);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
      }
      if (id.d_o.no_of_dir_options > 0)
      {
         int i;

         count = sprintf(*text + length, "DIR-options: %s\n",
                         id.d_o.aoptions[0]);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
         for (i = 1; i < id.d_o.no_of_dir_options; i++)
         {
            count = sprintf(*text + length, "             %s\n",
                            id.d_o.aoptions[i]);
            length += count;
            if (count > max_x)
            {
               max_x = count;
            }
            max_y++;
         }
      }
      if (id.files != NULL)
      {
         int i;

         count = sprintf(*text + length, "Filter     : %s\n", id.files);
         length += count;
         p_file = id.files;
         NEXT(p_file);
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
         for (i = 1; i < id.no_of_files; i++)
         {
            if (length > (buffer_length - 1024))
            {
               buffer_length += 8192;
               if (buffer_length > (10 * MEGABYTE))
               {
                  (void)xrec(INFO_DIALOG,
                             "Buffer for writting DIR_CONFIG data is larger then 10 Megabyte. DIR_CONFIG data incomplete. (%s %d)",
                             __FILE__, __LINE__);
                  return;
               }
               if ((*text = realloc(*text, buffer_length)) == NULL)
               {
                  (void)xrec(FATAL_DIALOG,
                             "Failed to realloc() %d bytes : %s (%s %d)",
                             buffer_length, strerror(errno),
                             __FILE__, __LINE__);
                  return;
               }
            }
            count = sprintf(*text + length, "             %s\n", p_file);
            length += count;
            if (count > max_x)
            {
               max_x = count;
            }
            max_y++;
            NEXT(p_file);
         }
      }

      /* Print recipient. */
      if (perm.view_passwd == YES)
      {
         insert_passwd(id.recipient);
      }
      count = sprintf(*text + length, "Recipient  : %s\n", id.recipient);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
      if (id.no_of_loptions > 0)
      {
         int i;

         count = sprintf(*text + length, "AMG-options: %s\n", id.loptions[0]);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
         for (i = 1; i < id.no_of_loptions; i++)
         {
            count = sprintf(*text + length, "             %s\n",
                            id.loptions[i]);
            length += count;
            if (count > max_x)
            {
               max_x = count;
            }
            max_y++;
         }
      }
      if (id.no_of_soptions == 1)
      {
         count = sprintf(*text + length, "FD-options : %s\n", id.soptions);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
      }
      else if (id.no_of_soptions > 1)
           {
              int  first = YES;
              char *p_start,
                   *p_end;

              p_start = p_end = id.soptions;
              do
              {
                 while ((*p_end != '\n') && (*p_end != '\0'))
                 {
                    p_end++;
                 }
                 if (*p_end == '\n')
                 {
                    *p_end = '\0';
                    if (first == YES)
                    {
                       first = NO;
                       count = sprintf(*text + length, "FD-options : %s\n",
                                       p_start);
                    }
                    else
                    {
                       count = sprintf(*text + length, "             %s\n",
                                       p_start);
                    }
                    length += count;
                    if (count > max_x)
                    {
                       max_x = count;
                    }
                    max_y++;
                    p_end++;
                    p_start = p_end;
                 }
              } while (*p_end != '\0');
              count = sprintf(*text + length, "             %s\n", p_start);
              length += count;
              if (count > max_x)
              {
                 max_x = count;
              }
              max_y++;
           }
      count = sprintf(*text + length, "Priority   : %c\n", id.priority);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
   } /* if (id.no_of_files > 0) */

   count = sprintf(*text + length, "Job-ID     : %x", id.job_no);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y++;

   if (id.retries > 0)
   {
      count = sprintf(*text + length, "\nRetries    : %u", id.retries);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
   }

   if (id.mail_id[0] != '\0')
   {
      count = sprintf(*text + length, "\nMailqueueno: %s", id.mail_id);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
   }

   /* Show archive directory if it is available. */
   if (id.archive_dir[0] != '\0')
   {
      char archive_dir[MAX_PATH_LENGTH + AFD_ARCHIVE_DIR_LENGTH + 1 + MAX_PATH_LENGTH + 1];

      (void)sprintf(archive_dir, "%s%s/%s",
                    p_work_dir, AFD_ARCHIVE_DIR, id.archive_dir);
      if (eaccess(archive_dir, F_OK) == 0)
      {
         count = sprintf(*text + length, "\nArchive dir: %s", id.archive_dir);
      }
      else
      {
         count = sprintf(*text + length, "\nArchive dir: %s [DELETED]",
                         id.archive_dir);
      }
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;

      if (id.unique_name[0] != '\0')
      {
         count = sprintf(*text + length, "\nUnique name: %s", id.unique_name);
      }
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
   }

   return;
}


/*######################## format_receive_info() ########################*/
void
format_receive_info(char **text, int with_alda_data)
{
   int    count,
          length;
   size_t new_size;

   if ((*text = malloc(MAX_PATH_LENGTH)) == NULL)
   {
      (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   max_x = sizeof("File name   : ") - 1 + strlen((char *)id.local_file_name) + 1;
   length = max_x;
   count = sizeof("File size   : ") - 1 + strlen(id.file_size) + sizeof(" bytes") - 1 + 1;
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   count = sizeof("Receive time: ") - 1 + strlen(ctime(&id.date_send));
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   if (id.trans_time[0] != '\0')
   {
      count = sizeof("Trans time  : ") - 1 + sizeof("Trans time : ") + strlen((char *)id.trans_time) + sizeof(" sec") + 1;
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
   }
   if (id.dir[0] != '\0')
   {
      int i,
          j;

      count = sizeof("Directory   : ") - 1 + strlen((char *)id.dir) + 1;
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y = 5;
      if (id.d_o.dir_alias[0] != '\0')
      {
         count = sizeof("Dir-Alias   : ") - 1 + strlen(id.d_o.dir_alias) + 1;
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
      }
      count = sprintf(*text, "Dir-ID      : %x\n", id.dir_id);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
      if (id.d_o.url[0] != '\0')
      {
         if (perm.view_passwd == YES)
         {
            insert_passwd(id.d_o.url);
         }
         count = sizeof("DIR-URL     : ") - 1 + strlen(id.d_o.url) + 1;
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
      }
      if (id.d_o.no_of_dir_options > 0)
      {
         count = sizeof("DIR-options : ") - 1 + strlen(id.d_o.aoptions[0]) + 1;
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
         for (i = 1; i < id.d_o.no_of_dir_options; i++)
         {
            count = sizeof("              ") - 1 + strlen(id.d_o.aoptions[i]) + 1;
            length += count;
            if (count > max_x)
            {
               max_x = count;
            }
            max_y++;
         }
      }

      for (j = 0; j < id.count; j++)
      {
         if (id.dbe[j].files != NULL)
         {
            char *p_file;

            p_file = id.dbe[j].files;
            count = sizeof("Filter      : ") - 1 + strlen(p_file) + 1;
            NEXT(p_file);
            length += count;
            if (count > max_x)
            {
               max_x = count;
            }
            max_y++;
            for (i = 1; i < id.dbe[j].no_of_files; i++)
            {
               count = sizeof("              ") - 1 + strlen(p_file) + 1;
               length += count;
               if (count > max_x)
               {
                  max_x = count;
               }
               max_y++;
               NEXT(p_file);
            }
         }

         /* Print recipient. */
         if (perm.view_passwd == YES)
         {
            insert_passwd(id.dbe[j].recipient);
         }
         count = sizeof("Recipient   : ") - 1 + strlen(id.dbe[j].recipient) + 1;
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
         if (id.dbe[j].no_of_loptions > 0)
         {
            count = sizeof("AMG-options : ") - 1 + strlen(id.dbe[j].loptions[0]) + 1;
            length += count;
            if (count > max_x)
            {
               max_x = count;
            }
            max_y++;
            for (i = 1; i < id.dbe[j].no_of_loptions; i++)
            {
               count = sizeof("              ") - 1 + strlen(id.dbe[j].loptions[i]) + 1;
               length += count;
               if (count > max_x)
               {
                  max_x = count;
               }
               max_y++;
            }
         }
         if (id.dbe[j].no_of_soptions == 1)
         {
            count = sizeof("FD-options  : ") - 1 + strlen(id.dbe[j].soptions) + 1;
            length += count;
            if (count > max_x)
            {
               max_x = count;
            }
            max_y++;
         }
         else if (id.dbe[j].no_of_soptions > 1)
              {
                 int  first = YES;
                 char *p_start,
                      *p_end;

                 p_start = p_end = id.dbe[j].soptions;
                 do
                 {
                    while ((*p_end != '\n') && (*p_end != '\0'))
                    {
                       p_end++;
                    }
                    if (*p_end == '\n')
                    {
                       *p_end = '\0';
                       if (first == YES)
                       {
                          first = NO;
                          count = sizeof("FD-options  : ") - 1 + strlen(p_start) + 1;
                       }
                       else
                       {
                          count = sizeof("              ") - 1 + strlen(p_start) + 1;
                       }
                       length += count;
                       if (count > max_x)
                       {
                          max_x = count;
                       }
                       *p_end = '\n';
                       max_y++;
                       p_end++;
                       p_start = p_end;
                    }
                 } while (*p_end != '\0');
                 count = sizeof("              ") - 1 + strlen(p_start) + 1;
                 length += count;
                 if (count > max_x)
                 {
                    max_x = count;
                 }
                 max_y++;
              }
         count = sizeof("Priority    : 0") - 1 + 1;
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
         count = sprintf(*text, "Job-ID      : %x\n", id.dbe[j].job_id);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
         count = sprintf(*text, "DIR_CONFIG  : %s\n", id.dbe[j].dir_config_file);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;

         if (with_alda_data == YES)
         {
            int gotcha = NO,
                k;

            for (k = 0; k < acd_counter; k++)
            {
               if (id.dbe[j].job_id == acd[k].output_job_id)
               {
                  count = sizeof("Dest name   : ") - 1 + strlen(acd[k].final_name) + 1;
                  length += count;
                  if (count > max_x)
                  {
                     max_x = count;
                  }
                  max_y++;
                  if (acd[k].final_size > MEGABYTE)
                  {
                     count = sprintf(*text,
#if SIZEOF_OFF_T == 4
                                     "Dest size   : %ld bytes (%s)\n",
#else
                                     "Dest size   : %lld bytes (%s)\n",
#endif
                                     (pri_off_t)acd[k].final_size,
                                     acd[k].hr_final_size);
                  }
                  else
                  {
                     count = sprintf(*text,
#if SIZEOF_OFF_T == 4
                                     "Dest size   : %ld bytes\n",
#else
                                     "Dest size   : %lld bytes\n",
#endif
                                     (pri_off_t)acd[k].final_size);
                  }
                  length += count;
                  if (count > max_x)
                  {
                     max_x = count;
                  }
                  max_y++;
                  count = sizeof("Arrival time: ") - 1 + strlen(ctime(&acd[k].delivery_time));
                  length += count;
                  if (count > max_x)
                  {
                     max_x = count;
                  }
                  max_y++;
                  count = sizeof("Transp. time: ") - 1 + strlen(acd[k].transmission_time) + 1;
                  length += count;
                  if (count > max_x)
                  {
                     max_x = count;
                  }
                  max_y++;
                  if (acd[k].retries > 0)
                  {
                     count = sprintf(*text, "Retries     : %u\n",
                                     acd[k].retries);
                     length += count;
                     if (count > max_x)
                     {
                        max_x = count;
                     }
                     max_y++;
                  }
                  if (acd[k].archive_dir[0] != '\0')
                  {
#if SIZEOF_TIME_T == 4
                     count = sprintf(*text, "Archive Dir : %s/%lx_%x_%x_\n",
#else
                     count = sprintf(*text, "Archive Dir : %s/%llx_%x_%x_\n",
#endif
                                     acd[k].archive_dir,
                                     (pri_time_t)id.arrival_time,
                                     id.unique_number,
                                     acd[k].split_job_counter);
                     length += count;
                     if (count > max_x)
                     {
                        max_x = count;
                     }
                     max_y++;
                  }
                  gotcha = YES;
               }
               else if (id.dbe[j].job_id == acd[k].delete_job_id)
                    {
                       count = sizeof("Delete time : ") - 1 + strlen(ctime(&acd[k].delete_time));
                       length += count;
                       if (count > max_x)
                       {
                          max_x = count;
                       }
                       max_y++;
                       count = sizeof("Del. reason : ") - 1 + strlen(drstr[acd[k].delete_type]) + 1;
                       length += count;
                       if (count > max_x)
                       {
                          max_x = count;
                       }
                       max_y++;
                       if (acd[k].add_reason[0] != '\0')
                       {
                          count = sizeof("Add. reason : ") - 1 + strlen(acd[k].add_reason) + 1;
                          length += count;
                          if (count > max_x)
                          {
                             max_x = count;
                          }
                          max_y++;
                       }
                       if (acd[k].user_process[0] != '\0')
                       {
                          count = sizeof("User/process: ") - 1 + strlen(acd[k].user_process) + 1;
                          length += count;
                          if (count > max_x)
                          {
                             max_x = count;
                          }
                          max_y++;
                       }
                       gotcha = YES;
                    }
#ifdef _DISTRIBUTION_LOG
               else if ((acd[k].distribution_type == DISABLED_DIS_TYPE) &&
                        (acd[k].delete_time != 0))
                    {
                       int m;

                       for (m = 0; m < acd[k].no_of_distribution_types; m++)
                       {
                          if (id.dbe[j].job_id == acd[k].job_id_list[m])
                          {
                             count = sizeof("Delete time : ") - 1 + strlen(ctime(&acd[k].delete_time));
                             length += count;
                             if (count > max_x)
                             {
                                max_x = count;
                             }
                             max_y++;
                             count = sizeof("Del. reason : ") - 1 + strlen(drstr[acd[k].delete_type]) + 1;
                             length += count;
                             if (count > max_x)
                             {
                                max_x = count;
                             }
                             max_y++;
                             if (acd[k].user_process[0] != '\0')
                             {
                                count = sizeof("User/process: ") - 1 + strlen(acd[k].user_process) + 1;
                                length += count;
                                if (count > max_x)
                                {
                                   max_x = count;
                                }
                                max_y++;
                             }
                             gotcha = YES;
                             break;
                          }
                       }
                    }
#endif

            }
            if (gotcha == NO)
            {
               if ((acd_counter == 1) && (acd[0].delete_time != 0))
               {
                  count = sizeof("Delete time : ") - 1 + strlen(ctime(&acd[k].delete_time));
                  length += count;
                  if (count > max_x)
                  {
                     max_x = count;
                  }
                  max_y++;
                  count = sizeof("Del. reason : ") - 1 + strlen(drstr[acd[k].delete_type]) + 1;
                  length += count;
                  if (count > max_x)
                  {
                     max_x = count;
                  }
                  max_y++;
                  if (acd[k].add_reason[0] != '\0')
                  {
                     count = sizeof("Add. reason : ") - 1 + strlen(acd[k].add_reason) + 1;
                     length += count;
                     if (count > max_x)
                     {
                        max_x = count;
                     }
                     max_y++;
                  }
                  if (acd[k].user_process[0] != '\0')
                  {
                     count = sizeof("User/process: ") - 1 + strlen(acd[k].user_process) + 1;
                     length += count;
                     if (count > max_x)
                     {
                        max_x = count;
                     }
                     max_y++;
                  }
               }
               else
               {
                  count = sizeof("No output/delete data found. See show_queue if it is still queued.") - 1 + 1;
                  length += count;
                  if (count > max_x)
                  {
                     max_x = count;
                  }
                  max_y++;
               }
            }
         }
      } /* for (j = 0; j < id.count; j++) */
   } /* if (id.dir[0] != '\0') */
   else
   {
      count = sprintf(*text, "Dir-ID      : %x\n", id.dir_id);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y = 5;
   }

   new_size = length + ((id.count + id.count + 1) * (max_x + 1));
   if (MAX_PATH_LENGTH < new_size)
   {
      if ((*text = realloc(*text, new_size)) == NULL)
      {
         (void)fprintf(stderr, "realloc() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   length = sprintf(*text, "File name   : ");
   count = 0;
   while (id.local_file_name[count] != '\0')
   {
      if (id.local_file_name[count] < ' ')
      {
         *(*text + length) = '?';
      }
      else
      {
         *(*text + length) = id.local_file_name[count];
      }
      count++; length++;
   }
   *(*text + length) = '\n';
   length++;
   length += sprintf(*text + length, "File size   : %s bytes\n", id.file_size);
   length += sprintf(*text + length, "Receive time: %s", ctime(&id.date_send));
   if (id.trans_time[0] != '\0')
   {
      length += sprintf(*text + length, "Trans time : %s sec\n", id.trans_time);
   }
   if (id.dir[0] != '\0')
   {
      int i,
          j;

      length += sprintf(*text + length, "Directory   : %s\n", id.dir);
      if (id.d_o.dir_alias[0] != '\0')
      {
         length += sprintf(*text + length, "Dir-Alias   : %s\n", id.d_o.dir_alias);
      }
      length += sprintf(*text + length, "Dir-ID      : %x\n", id.dir_id);
      if (id.d_o.url[0] != '\0')
      {
         length += sprintf(*text + length, "DIR-URL     : %s\n", id.d_o.url);
      }
      if (id.d_o.no_of_dir_options > 0)
      {
         length += sprintf(*text + length, "DIR-options : %s\n",
                           id.d_o.aoptions[0]);
         for (i = 1; i < id.d_o.no_of_dir_options; i++)
         {
            length += sprintf(*text + length, "              %s\n",
                              id.d_o.aoptions[i]);
         }
      }
      (void)memset(*text + length, '#', max_x);
      length += max_x;
      *(*text + length) = '\n';
      length++;
      max_y++;

      for (j = 0; j < id.count; j++)
      {
         if (id.dbe[j].files != NULL)
         {
            char *p_file;

            p_file = id.dbe[j].files;
            length += sprintf(*text + length, "Filter      : %s\n", p_file);
            NEXT(p_file);
            for (i = 1; i < id.dbe[j].no_of_files; i++)
            {
               length += sprintf(*text + length, "              %s\n", p_file);
               NEXT(p_file);
            }
         }

         /* Print recipient. */
         length += sprintf(*text + length,
                           "Recipient   : %s\n", id.dbe[j].recipient);
         if (id.dbe[j].no_of_loptions > 0)
         {
            length += sprintf(*text + length,
                              "AMG-options : %s\n", id.dbe[j].loptions[0]);
            for (i = 1; i < id.dbe[j].no_of_loptions; i++)
            {
               length += sprintf(*text + length,
                                 "              %s\n", id.dbe[j].loptions[i]);
            }
         }
         if (id.dbe[j].no_of_soptions == 1)
         {
            length += sprintf(*text + length,
                              "FD-options  : %s\n", id.dbe[j].soptions);
         }
         else if (id.dbe[j].no_of_soptions > 1)
              {
                 int  first = YES;
                 char *p_start,
                      *p_end;

                 p_start = p_end = id.dbe[j].soptions;
                 do
                 {
                    while ((*p_end != '\n') && (*p_end != '\0'))
                    {
                       p_end++;
                    }
                    if (*p_end == '\n')
                    {
                       *p_end = '\0';
                       if (first == YES)
                       {
                          first = NO;
                          length += sprintf(*text + length,
                                            "FD-options  : %s\n", p_start);
                       }
                       else
                       {
                          length += sprintf(*text + length,
                                            "              %s\n", p_start);
                       }
                       p_end++;
                       p_start = p_end;
                    }
                 } while (*p_end != '\0');
                 length += sprintf(*text + length,
                                   "              %s\n", p_start);
              }
         length += sprintf(*text + length,
                           "Priority    : %c\n", id.dbe[j].priority);
         length += sprintf(*text + length,
                           "Job-ID      : %x\n", id.dbe[j].job_id);
         length += sprintf(*text + length,
                           "DIR_CONFIG  : %s\n", id.dbe[j].dir_config_file);

         if (with_alda_data == YES)
         {
            int gotcha = NO,
                k;

            (void)memset(*text + length, '-', max_x);
            length += max_x;
            *(*text + length) = '\n';
            length++;
            max_y++;

            for (k = 0; k < acd_counter; k++)
            {
               if (id.dbe[j].job_id == acd[k].output_job_id)
               {
                  length += sprintf(*text + length,
                                    "Dest name   : %s\n", acd[k].final_name);
                  if (acd[k].final_size > MEGABYTE)
                  {
                     length += sprintf(*text + length,
#if SIZEOF_OFF_T == 4
                                       "Dest size   : %ld bytes (%s)\n",
#else
                                       "Dest size   : %lld bytes (%s)\n",
#endif
                                       (pri_off_t)acd[k].final_size,
                                       acd[k].hr_final_size);
                  }
                  else
                  {
                     length += sprintf(*text + length,
#if SIZEOF_OFF_T == 4
                                       "Dest size   : %ld bytes\n",
#else
                                       "Dest size   : %lld bytes\n",
#endif
                                       (pri_off_t)acd[k].final_size);
                  }
                  length += sprintf(*text + length, "Arrival time: %s",
                                    ctime(&acd[k].delivery_time));
                  length += sprintf(*text + length, "Transp. time: %s\n",
                                    acd[k].transmission_time);
                  if (acd[k].retries > 0)
                  {
                     length += sprintf(*text + length,
                                       "Retries     : %u\n", acd[k].retries);
                  }
                  if (acd[k].archive_dir[0] != '\0')
                  {
                     length += sprintf(*text + length,
#if SIZEOF_TIME_T == 4
                                       "Archive Dir : %s/%lx_%x_%x_\n",
#else
                                       "Archive Dir : %s/%llx_%x_%x_\n",
#endif
                                       acd[k].archive_dir,
                                       (pri_time_t)id.arrival_time,
                                       id.unique_number,
                                       acd[k].split_job_counter);
                  }
                  gotcha = YES;
               }
               else if (id.dbe[j].job_id == acd[k].delete_job_id)
                    {
                       length += sprintf(*text + length,
                                         "Delete time : %s",
                                         ctime(&acd[k].delete_time));
                       length += sprintf(*text + length,
                                         "Del. reason : %s\n",
                                         drstr[acd[k].delete_type]);
                       if (acd[k].add_reason[0] != '\0')
                       {
                          length += sprintf(*text + length,
                                            "Add. reason : %s\n",
                                            acd[k].add_reason);
                       }
                       if (acd[k].user_process[0] != '\0')
                       {
                          length += sprintf(*text + length,
                                            "User/process: %s\n",
                                            acd[k].user_process);
                       }
                       gotcha = YES;
                    }
#ifdef _DISTRIBUTION_LOG
               else if ((acd[k].distribution_type == DISABLED_DIS_TYPE) &&
                        (acd[k].delete_time != 0))
                    {
                       int m;

                       for (m = 0; m < acd[k].no_of_distribution_types; m++)
                       {
                          if (id.dbe[j].job_id == acd[k].job_id_list[m])
                          {
                             length += sprintf(*text + length,
                                               "Delete time : %s",
                                               ctime(&acd[k].delete_time));
                             length += sprintf(*text + length,
                                               "Del. reason : %s\n",
                                               drstr[acd[k].delete_type]);
                             if (acd[k].user_process[0] != '\0')
                             {
                                length += sprintf(*text + length,
                                                  "User/process: %s\n",
                                                  acd[k].user_process);
                             }
                             gotcha = YES;
                             break;
                          }
                       }
                    }
#endif
            }
            if (gotcha == NO)
            {
               if ((acd_counter == 1) && (acd[0].delete_time != 0))
               {
                  length += sprintf(*text + length, "Delete time : %s",
                                    ctime(&acd[k].delete_time));
                  length += sprintf(*text + length, "Del. reason : %s\n",
                                    drstr[acd[k].delete_type]);
                  if (acd[k].add_reason[0] != '\0')
                  {
                     length += sprintf(*text + length, "Add. reason : %s\n",
                                       acd[k].add_reason);
                  }
                  if (acd[k].user_process[0] != '\0')
                  {
                     length += sprintf(*text + length, "User/process: %s\n",
                                       acd[k].user_process);
                  }
               }
               else
               {
                  length += sprintf(*text + length,
                                    "No output/delete data found. See show_queue if it is still queued.\n");
               }
            }
         }
         if (j < (id.count - 1))
         {
            (void)memset(*text + length, '=', max_x);
            length += max_x;
            *(*text + length) = '\n';
            length++;
            max_y++;
         }
      } /* for (j = 0; j < id.count; j++) */
      *(*text + length - 1) = '\0';
   } /* if (id.dir[0] != '\0') */
   else
   {
      length += sprintf(*text + length, "Dir-ID      : %x\n", id.dir_id);
   }

   return;
}
