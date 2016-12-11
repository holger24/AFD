/*
 *  format_info.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2016 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   void format_info(char **text)
 **
 ** DESCRIPTION
 **   format_info() shows data in the following format:
 **         Orig File name      : xxxxxxx.xx
 **         Orig File size      : 2376 Bytes
 **         Receive time        : Mon Sep 27 12:45:39 2004
 **         Command             : exec check.sh %s
 **         Return code         : 0
 **         Ratio               : 1:1
 **         New File name       : xxxxxxx.xx
 **         New File size       : 2376 Bytes
 **         CPU time used       : 0.000341
 **         Production time     : 2.259
 **         Production finished : Mon Sep 27 12:45:41 2004
 **         Directory           : /aaa/bbb/ccc
 **         Dir-Alias           : ccc_dir
 **         Dir-ID              : 4a231f1
 **         =====================================================
 **         Filter              : filter_1
 **                               filter_2
 **                               filter_n
 **         Recipient           : ftp://donald:secret@hollywood//home/user
 **         AMG-options         : option_1
 **                               option_2
 **                               option_n
 **         FD-options          : option_1
 **                               option_2
 **                               option_n
 **         Priority            : 5
 **         Job-ID              : d88f540e
 **         DIR_CONFIG          : /home/afd/etc/DIR_CONFIG
 **
 ** RETURN VALUES
 **   Returns the formated text.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.09.2016 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* sprintf()                          */
#include <stdlib.h>                /* malloc(), realloc()                */
#include <time.h>                  /* ctime()                            */
#include <unistd.h>                /* F_OK                               */
#include <errno.h>
#include "show_plog.h"

/* External global variables. */
extern int              max_x,
                        max_y;
extern char             *p_work_dir;
extern struct info_data id;
extern struct sol_perm  perm;


/*############################ format_info() ############################*/
void
format_info(char **text)
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
   max_x = sizeof("Orig File name      : ") - 1 + strlen((char *)id.original_filename) + 1;
   length = max_x;
   max_y = 1;
   if (id.orig_file_size != -1)
   {
#if SIZEOF_OFF_T == 4
      count = sprintf(*text + length, "Orig File size      : %ld bytes\n",
#else
      count = sprintf(*text + length, "Orig File size      : %lld bytes\n",
#endif
                      (pri_off_t)id.orig_file_size);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
   }
   count = sizeof("Receive time        : ") - 1 + strlen(ctime(&id.input_time));
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y++;
   count = sizeof("Command             : ") - 1 + strlen((char *)id.command) + 1;
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y++;
   count = sprintf(*text, "Return code         : %d\n", id.return_code);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y++;
   count = sprintf(*text, "Ratio               : %d:%d\n", id.ratio_1, id.ratio_2);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y++;
   count = sizeof("New File name       : ") - 1 + strlen((char *)id.new_filename) + 1;
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y++;
   if (id.new_file_size == -1)
   {
      count = sizeof("New File size       : ");
   }
   else
   {
#if SIZEOF_OFF_T == 4
      count = sprintf(*text + length, "New File size       : %ld bytes\n",
#else
      count = sprintf(*text + length, "New File size       : %lld bytes\n",
#endif
                      (pri_off_t)id.new_file_size);
   }
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y++;
   if (id.cpu_time == -1.0)
   {
      count = sprintf(*text, "CPU time used       :\n");
   }
   else
   {
      count = sprintf(*text, "CPU time used       : %.6f sec\n", id.cpu_time);
   }
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y++;
   count = sprintf(*text, "Production time     : %.3f sec\n", id.production_time);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y++;
   count = sizeof("Production finished : ") - 1 + strlen(ctime(&id.time_when_produced));
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y++;
   if (id.dir[0] != '\0')
   {
      int i;

      count = sizeof("Directory           : ") - 1 + strlen((char *)id.dir) + 1;
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
      if (id.d_o.dir_alias[0] != '\0')
      {
         count = sizeof("Dir-Alias           : ") - 1 + strlen(id.d_o.dir_alias) + 1;
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
      }
      count = sprintf(*text, "Dir-ID              : %x\n", id.dir_id);
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
         count = sizeof("DIR-URL             : ") - 1 + strlen(id.d_o.url) + 1;
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
      }
      if (id.d_o.no_of_dir_options > 0)
      {
         count = sizeof("DIR-options         : ") - 1 + strlen(id.d_o.aoptions[0]) + 1;
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
         for (i = 1; i < id.d_o.no_of_dir_options; i++)
         {
            count = sizeof("                      ") - 1 + strlen(id.d_o.aoptions[i]) + 1;
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
         char *p_file;

         p_file = id.files;
         count = sizeof("Filter              : ") - 1 + strlen(p_file) + 1;
         NEXT(p_file);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
         for (i = 1; i < id.no_of_files; i++)
         {
            count = sizeof("                      ") - 1 + strlen(p_file) + 1;
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
      count = sizeof("Recipient           : ") - 1 + strlen(id.recipient) + 1;
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
      if (id.no_of_loptions > 0)
      {
         count = sizeof("AMG-options         : ") - 1 + strlen(id.loptions[0]) + 1;
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
         for (i = 1; i < id.no_of_loptions; i++)
         {
            count = sizeof("                      ") - 1 + strlen(id.loptions[i]) + 1;
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
         count = sizeof("FD-options          : ") - 1 + strlen(id.soptions) + 1;
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
                       count = sizeof("FD-options          : ") - 1 + strlen(p_start) + 1;
                    }
                    else
                    {
                       count = sizeof("                      ") - 1 + strlen(p_start) + 1;
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
              count = sizeof("                      ") - 1 + strlen(p_start) + 1;
              length += count;
              if (count > max_x)
              {
                 max_x = count;
              }
              max_y++;
           }
      count = sizeof("Priority            : 0") - 1 + 1;
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
      count = sprintf(*text, "Job-ID              : %x\n", id.job_id);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
      count = sprintf(*text, "DIR_CONFIG          : %s", id.dir_config_file);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
   } /* if (id.dir[0] != '\0') */
   else
   {
      count = sprintf(*text, "Dir-ID              : %x", id.dir_id);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
   }

   new_size = length + (2 * (max_x + 1));
   if (MAX_PATH_LENGTH < new_size)
   {
      if ((*text = realloc(*text, new_size)) == NULL)
      {
         (void)fprintf(stderr, "realloc() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   length = sprintf(*text, "Orig File name      : ");
   count = 0;
   while (id.original_filename[count] != '\0')
   {
      if (id.original_filename[count] < ' ')
      {
         *(*text + length) = '?';
      }
      else
      {
         *(*text + length) = id.original_filename[count];
      }
      count++; length++;
   }
   *(*text + length) = '\n';
   length++;
   if (id.orig_file_size != -1)
   {
#if SIZEOF_OFF_T == 4
      length += sprintf(*text + length, "Orig File size      : %ld bytes\n",
#else
      length += sprintf(*text + length, "Orig File size      : %lld bytes\n",
#endif
                        (pri_off_t)id.orig_file_size);
   }
   length += sprintf(*text + length, "Receive time        : %s", ctime(&id.input_time));
   length += sprintf(*text + length, "Command             : ");
   count = 0;
   while (id.command[count] != '\0')
   {
      if (id.command[count] < ' ')
      {
         *(*text + length) = '?';
      }
      else
      {
         *(*text + length) = id.command[count];
      }
      count++; length++;
   }
   *(*text + length) = '\n';
   length++;
   length += sprintf(*text + length, "Return code         : %d\n", id.return_code);
   length += sprintf(*text + length, "Ratio               : %d:%d\n", id.ratio_1, id.ratio_2);
   length += sprintf(*text + length, "New File name       : ");
   count = 0;
   while (id.new_filename[count] != '\0')
   {
      if (id.new_filename[count] < ' ')
      {
         *(*text + length) = '?';
      }
      else
      {
         *(*text + length) = id.new_filename[count];
      }
      count++; length++;
   }
   *(*text + length) = '\n';
   length++;
   if (id.new_file_size == -1)
   {
      length += sprintf(*text + length, "New File size       : \n");
   }
   else
   {
#if SIZEOF_OFF_T == 4
      length += sprintf(*text + length, "New File size       : %ld bytes\n",
#else
      length += sprintf(*text + length, "New File size       : %lld bytes\n",
#endif
                        (pri_off_t)id.new_file_size);
   }
   if (id.cpu_time == -1)
   {
      length += sprintf(*text + length, "CPU time used       :\n");
   }
   else
   {
      length += sprintf(*text + length, "CPU time used       : %g sec\n", id.cpu_time);
   }
   length += sprintf(*text + length, "Production time     : %g sec\n", id.production_time);
   length += sprintf(*text + length, "Production finished : %s", ctime(&id.time_when_produced));
   if (id.dir[0] != '\0')
   {
      int i;

      length += sprintf(*text + length, "Directory           : %s\n", id.dir);
      if (id.d_o.dir_alias[0] != '\0')
      {
         length += sprintf(*text + length, "Dir-Alias           : %s\n", id.d_o.dir_alias);
      }
      length += sprintf(*text + length, "Dir-ID              : %x\n", id.dir_id);
      if (id.d_o.url[0] != '\0')
      {
         length += sprintf(*text + length, "DIR-URL             : %s\n", id.d_o.url);
      }
      if (id.d_o.no_of_dir_options > 0)
      {
         length += sprintf(*text + length, "DIR-options         : %s\n",
                           id.d_o.aoptions[0]);
         for (i = 1; i < id.d_o.no_of_dir_options; i++)
         {
            length += sprintf(*text + length, "                      %s\n",
                              id.d_o.aoptions[i]);
         }
      }
      (void)memset(*text + length, '#', max_x);
      length += max_x;
      *(*text + length) = '\n';
      length++;
      max_y++;

      if (id.files != NULL)
      {
         char *p_file;

         p_file = id.files;
         length += sprintf(*text + length, "Filter              : %s\n", p_file);
         NEXT(p_file);
         for (i = 1; i < id.no_of_files; i++)
         {
            length += sprintf(*text + length, "                      %s\n", p_file);
            NEXT(p_file);
         }
      }

      /* Print recipient. */
      length += sprintf(*text + length,
                        "Recipient           : %s\n", id.recipient);
      if (id.no_of_loptions > 0)
      {
         length += sprintf(*text + length,
                           "AMG-options         : %s\n", id.loptions[0]);
         for (i = 1; i < id.no_of_loptions; i++)
         {
            length += sprintf(*text + length,
                              "                      %s\n", id.loptions[i]);
         }
      }
      if (id.no_of_soptions == 1)
      {
         length += sprintf(*text + length,
                           "FD-options          : %s\n", id.soptions);
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
                       length += sprintf(*text + length,
                                         "FD-options          : %s\n", p_start);
                    }
                    else
                    {
                       length += sprintf(*text + length,
                                         "                      %s\n", p_start);
                    }
                    p_end++;
                    p_start = p_end;
                 }
              } while (*p_end != '\0');
              length += sprintf(*text + length,
                                "                      %s\n", p_start);
           }
      length += sprintf(*text + length,
                        "Priority            : %c\n", id.priority);
      length += sprintf(*text + length,
                        "Job-ID              : %x\n", id.job_id);
      length += sprintf(*text + length,
                        "DIR_CONFIG          : %s", id.dir_config_file);
   } /* if (id.dir[0] != '\0') */
   else
   {
      length += sprintf(*text + length, "Dir-ID              : %x", id.dir_id);
   }

   return;
}
