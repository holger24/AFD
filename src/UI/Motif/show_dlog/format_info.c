/*
 *  format_info.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   void format_output_info(char **text)
 **   void format_input_info(char **text)
 **
 ** DESCRIPTION
 **   The function format_output_info() formats data from the global
 **   structure info_data to the following form:
 **         File name  : xxxxxxx.xx
 **         File size  : 9833 Bytes
 **         Proc/User  : sf_ftp
 **         Add. reason: > 50 min
 **         Directory  : /aaa/bbb/ccc
 **         Dir-Alias  : ccc_dir
 **         Dir-ID     : 4b231e1
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
 **         Job-ID     : a323f2c
 **
 **    format_input_info() does it slightly differently:
 **         File name  : xxxxxxx.xx
 **         File size  : 78123 Bytes
 **         Proc/User  : sf_ftp
 **         Add. reason: > 50 min
 **         Directory  : /aaa/bbb/ccc
 **         Dir-Alias  : ccc_dir
 **         Dir-ID     : 4b231e1
 **         =====================================================
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
 **         -----------------------------------------------------
 **                                  .
 **                                  .
 **                                 etc.
 **
 **
 ** RETURN VALUES
 **   Returns the formated text.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.02.1998 H.Kiehl Created
 **   01.07.2001 H.Kiehl Show directory options.
 **   14.09.2008 H.Kiehl Added directory ID and alias.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* sprintf()                          */
#include <stdlib.h>                /* atoi()                             */
#include <errno.h>
#include "show_dlog.h"

/* External global variables. */
extern int              max_x,
                        max_y;
extern struct info_data id;
extern struct sol_perm  perm;


/*######################## format_output_info() #########################*/
void
format_output_info(char **text)
{
   int buffer_length = 8192,
       count,
       length = 0;

   if ((*text = malloc(buffer_length)) == NULL)
   {
      (void)xrec(FATAL_DIALOG, "Failed to malloc() %d bytes : %s (%s %d)",
                 buffer_length, strerror(errno), __FILE__, __LINE__);
      return;
   }
   max_x = sprintf(*text, "File name  : ");
   while (id.file_name[length] != '\0')
   {
      if (id.file_name[length] < ' ')
      {
         *(*text + max_x) = '?';
      }
      else
      {
         *(*text + max_x) = id.file_name[length];
      }
      length++; max_x++;
   }
   *(*text + max_x) = '\n';
   max_x++;
   length = max_x;
   count = sprintf(*text + length, "File size  : %s Bytes\n", id.file_size);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }

   /* Show process/user that deleted file. */
   count = sprintf(*text + length, "Proc/User  : %s\n", id.proc_user);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }

   /* Show reason. */
   count = sprintf(*text + length, "Reason     : %s\n", id.reason_str);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y = 4;

   /* Show additional reason if it is available. */
   if (id.extra_reason[0] != '\0')
   {
      if (id.extra_reason[0] == '>')
      {
         int days,
             diff_time = atoi((char *)(&id.extra_reason[1])),
             hours,
             min,
             sec;

         days = diff_time / 86400;
         hours = (diff_time % 86400) / 3600;
         min = ((diff_time % 86400) % 3600) / 60;
         sec = ((diff_time % 86400) % 3600) % 60;
         if (days > 0)
         {
            count = sprintf(*text + length,
                            "Add. reason: > %d days %d hours %d min %d sec\n",
                            days, hours, min, sec);
         }
         else if (hours > 0)
              {
                 count = sprintf(*text + length,
                                 "Add. reason: > %d hours %d min %d sec\n",
                                 hours, min, sec);
              }
         else if (min > 0)
              {
                 count = sprintf(*text + length,
                                 "Add. reason: > %d min %d sec\n",
                                 min, sec);
              }
              else
              {
                 count = sprintf(*text + length,
                                 "Add. reason: > %d sec\n",
                                 sec);
              }
      }
      else
      {
         count = sprintf(*text + length, "Add. reason: %s\n", id.extra_reason);
      }
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
   }

   if ((id.dbe != NULL) && (id.dbe[0].no_of_files > 0))
   {
      count = sprintf(*text + length, "Directory  : %s\n", id.dir);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
      if (id.d_o.dir_alias != '\0')
      {
         count = sprintf(*text + length, "Dir_Alias  : %s\n", id.d_o.dir_alias);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
      }
      count = sprintf(*text + length, "Dir_ID     : %x\n", id.dir_id);
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
         count = sprintf(*text + length, "DIR-URL    : %s\n", id.d_o.url);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
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
      if (id.dbe[0].files != NULL)
      {
         int  i;
         char *p_file;

         p_file = id.dbe[0].files;
         count = sprintf(*text + length, "Filter     : %s\n", p_file);
         NEXT(p_file);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y += 2;
         for (i = 1; i < id.dbe[0].no_of_files; i++)
         {
            if (length > (buffer_length - 1024))
            {
               buffer_length += 8192;
               if (buffer_length > (10 * MEGABYTE))
               {
                  (void)xrec(INFO_DIALOG,
                             "Buffer for writting DIR_CONFIG data is larger then 10 Megabyte. DIR_CONFIG data icomplete. (%s %d)",
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
         insert_passwd(id.dbe[0].recipient);
      }
      count = sprintf(*text + length, "Recipient  : %s\n", id.dbe[0].recipient);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
      if (id.dbe[0].no_of_loptions > 0)
      {
         int i;

         count = sprintf(*text + length, "AMG-options: %s\n", id.dbe[0].loptions[0]);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
         for (i = 1; i < id.dbe[0].no_of_loptions; i++)
         {
            if (length > (buffer_length - 1024))
            {
               buffer_length += 8192;
               if (buffer_length > (10 * MEGABYTE))
               {
                  (void)xrec(INFO_DIALOG,
                             "Buffer for writting DIR_CONFIG data is larger then 10 Megabyte. DIR_CONFIG data icomplete. (%s %d)",
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
            count = sprintf(*text + length, "             %s\n", id.dbe[0].loptions[i]);
            length += count;
            if (count > max_x)
            {
               max_x = count;
            }
            max_y++;
         }
      }
      if (id.dbe[0].no_of_soptions == 1)
      {
         count = sprintf(*text + length, "FD-options : %s\n", id.dbe[0].soptions);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
      }
      else if (id.dbe[0].no_of_soptions > 1)
           {
              int  first = YES;
              char *p_start,
                   *p_end;

              p_start = p_end = id.dbe[0].soptions;
              do
              {
                 while ((*p_end != '\n') && (*p_end != '\0'))
                 {
                    p_end++;
                 }
                 if (*p_end == '\n')
                 {
                    if (length > (buffer_length - 1024))
                    {
                       buffer_length += 8192;
                       if (buffer_length > (10 * MEGABYTE))
                       {
                          (void)xrec(INFO_DIALOG,
                                     "Buffer for writting DIR_CONFIG data is larger then 10 Megabyte. DIR_CONFIG data icomplete. (%s %d)",
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
                    *p_end = '\0';
                    if (first == YES)
                    {
                       first = NO;
                       count = sprintf(*text + length, "FD-options : %s\n", p_start);
                    }
                    else
                    {
                       count = sprintf(*text + length, "             %s\n", p_start);
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
      count = sprintf(*text + length, "Priority   : %c\n", id.dbe[0].priority);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
   } /* if (id.dbe[0].no_of_files > 0) */

   count = sprintf(*text + length, "Job-ID     : %x", id.job_id);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y++;

   return;
}


/*######################### format_input_info() #########################*/
void
format_input_info(char **text)
{
   int  begin_underline,
        buffer_length = 8192,
        count,
        *l_array = NULL,
        length;

   if ((*text = malloc(buffer_length)) == NULL)
   {
      (void)xrec(FATAL_DIALOG, "Failed to malloc() %d bytes : %s (%s %d)",
                 buffer_length, strerror(errno), __FILE__, __LINE__);
      return;
   }
   if (id.count > 0)
   {
      if ((l_array = malloc(id.count * sizeof(int))) == NULL)
      {
         (void)fprintf(stderr, "calloc() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   max_x = sprintf(*text, "File name  : %s\n", id.file_name);
   length = max_x;

   /* Show process/user that deleted file. */
   count = sprintf(*text + length, "Proc/User  : %s\n", id.proc_user);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }

   /* Show reason. */
   count = sprintf(*text + length, "Reason     : %s\n", id.reason_str);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y = 3;

   /* Show additional reason if it is available. */
   if (id.extra_reason[0] != '\0')
   {
      if (id.extra_reason[0] == '>')
      {
         int days,
             diff_time = atoi((char *)(&id.extra_reason[1])),
             hours,
             min,
             sec;

         days = diff_time / 86400;
         hours = (diff_time % 86400) / 3600;
         min = ((diff_time % 86400) % 3600) / 60;
         sec = ((diff_time % 86400) % 3600) % 60;
         if (days > 0)
         {
            count = sprintf(*text + length,
                            "Add. reason: > %d days %d hours %d min %d sec\n",
                            days, hours, min, sec);
         }
         else if (hours > 0)
              {
                 count = sprintf(*text + length,
                                 "Add. reason: > %d hours %d min %d sec\n",
                                 hours, min, sec);
              }
         else if (min > 0)
              {
                 count = sprintf(*text + length,
                                 "Add. reason: > %d min %d sec\n",
                                 min, sec);
              }
              else
              {
                 count = sprintf(*text + length,
                                 "Add. reason: > %d sec\n",
                                 sec);
              }
      }
      else
      {
         count = sprintf(*text + length, "Add. reason: %s\n", id.extra_reason);
      }
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

      count = sprintf(*text + length, "Directory  : %s\n", id.dir);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
      if (id.d_o.dir_alias != '\0')
      {
         count = sprintf(*text + length, "Dir_Alias  : %s\n", id.d_o.dir_alias);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
      }
      count = sprintf(*text + length, "Dir_ID     : %x\n", id.dir_id);
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
         count = sprintf(*text + length, "DIR-URL    : %s\n", id.d_o.url);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
      }
      if (id.d_o.no_of_dir_options > 0)
      {
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
            if (length > (buffer_length - 1024))
            {
               buffer_length += 8192;
               if (buffer_length > (10 * MEGABYTE))
               {
                  (void)xrec(INFO_DIALOG,
                             "Buffer for writting DIR_CONFIG data is larger then 10 Megabyte. DIR_CONFIG data icomplete. (%s %d)",
                             __FILE__, __LINE__);
                  free(l_array);
                  return;
               }
               if ((*text = realloc(*text, buffer_length)) == NULL)
               {
                  (void)xrec(FATAL_DIALOG,
                             "Failed to realloc() %d bytes : %s (%s %d)",
                             buffer_length, strerror(errno),
                             __FILE__, __LINE__);
                  free(l_array);
                  return;
               }
            }
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
      begin_underline = length;

      for (j = 0; j < id.count; j++)
      {
         if (length > (buffer_length - 1024))
         {
            buffer_length += 8192;
            if (buffer_length > (10 * MEGABYTE))
            {
               (void)xrec(INFO_DIALOG,
                          "Buffer for writting DIR_CONFIG data is larger then 10 Megabyte. DIR_CONFIG data icomplete. (%s %d)",
                          __FILE__, __LINE__);
               free(l_array);
               return;
            }
            if ((*text = realloc(*text, buffer_length)) == NULL)
            {
               (void)xrec(FATAL_DIALOG,
                          "Failed to realloc() %d bytes : %s (%s %d)",
                          buffer_length, strerror(errno),
                          __FILE__, __LINE__);
               free(l_array);
               return;
            }
         }
         if (id.dbe[j].files != NULL)
         {
            char *p_file;

            p_file = id.dbe[j].files;
            count = sprintf(*text + length, "Filter     : %s\n", p_file);
            NEXT(p_file);
            length += count;
            if (count > max_x)
            {
               max_x = count;
            }
            max_y++;
            for (i = 1; i < id.dbe[j].no_of_files; i++)
            {
               if (length > (buffer_length - 1024))
               {
                  buffer_length += 8192;
                  if (buffer_length > (10 * MEGABYTE))
                  {
                     (void)xrec(INFO_DIALOG,
                                "Buffer for writting DIR_CONFIG data is larger then 10 Megabyte. DIR_CONFIG data icomplete. (%s %d)",
                                __FILE__, __LINE__);
                     free(l_array);
                     return;
                  }
                  if ((*text = realloc(*text, buffer_length)) == NULL)
                  {
                     (void)xrec(FATAL_DIALOG,
                                "Failed to realloc() %d bytes : %s (%s %d)",
                                buffer_length, strerror(errno),
                                __FILE__, __LINE__);
                     free(l_array);
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
            insert_passwd(id.dbe[j].recipient);
         }
         count = sprintf(*text + length, "Recipient  : %s\n", id.dbe[j].recipient);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
         if (id.dbe[j].no_of_loptions > 0)
         {
            count = sprintf(*text + length, "AMG-options: %s\n", id.dbe[j].loptions[0]);
            length += count;
            if (count > max_x)
            {
               max_x = count;
            }
            max_y++;
            for (i = 1; i < id.dbe[j].no_of_loptions; i++)
            {
               if (length > (buffer_length - 1024))
               {
                  buffer_length += 8192;
                  if (buffer_length > (10 * MEGABYTE))
                  {
                     (void)xrec(INFO_DIALOG,
                                "Buffer for writting DIR_CONFIG data is larger then 10 Megabyte. DIR_CONFIG data icomplete. (%s %d)",
                                __FILE__, __LINE__);
                     free(l_array);
                     return;
                  }
                  if ((*text = realloc(*text, buffer_length)) == NULL)
                  {
                     (void)xrec(FATAL_DIALOG,
                                "Failed to realloc() %d bytes : %s (%s %d)",
                                buffer_length, strerror(errno),
                                __FILE__, __LINE__);
                     free(l_array);
                     return;
                  }
               }
               count = sprintf(*text + length, "             %s\n", id.dbe[j].loptions[i]);
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
            count = sprintf(*text + length, "FD-options : %s\n", id.dbe[j].soptions);
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
                       if (length > (buffer_length - 1024))
                       {
                          buffer_length += 8192;
                          if (buffer_length > (10 * MEGABYTE))
                          {
                             (void)xrec(INFO_DIALOG,
                                        "Buffer for writting DIR_CONFIG data is larger then 10 Megabyte. DIR_CONFIG data icomplete. (%s %d)",
                                        __FILE__, __LINE__);
                             free(l_array);
                             return;
                          }
                          if ((*text = realloc(*text, buffer_length)) == NULL)
                          {
                             (void)xrec(FATAL_DIALOG,
                                        "Failed to realloc() %d bytes : %s (%s %d)",
                                        buffer_length, strerror(errno),
                                        __FILE__, __LINE__);
                             free(l_array);
                             return;
                          }
                       }
                       *p_end = '\0';
                       if (first == YES)
                       {
                          first = NO;
                          count = sprintf(*text + length, "FD-options : %s\n", p_start);
                       }
                       else
                       {
                          count = sprintf(*text + length, "             %s\n", p_start);
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
         count = sprintf(*text + length, "Priority   : %c\n", id.dbe[j].priority);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;

         l_array[j] = length;
      } /* for (j = 0; j < id.count; j++) */
      *(*text + length - 1) = '\0';
   } /* if (id.dir[0] != '\0') */
   else
   {
      count = sprintf(*text + length, "Dir_ID     : %x\n", id.dir_id);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
   }

   if (id.count > 0)
   {
      int i,
          j;

      if ((length + (id.count * (max_x + 1))) > buffer_length)
      {
         buffer_length += ((length + (id.count * (max_x + 1))) - buffer_length) + 1;
         if (buffer_length > (10 * MEGABYTE))
         {
            (void)xrec(INFO_DIALOG,
                       "Buffer for writting DIR_CONFIG data is larger then 10 Megabyte. DIR_CONFIG data icomplete. (%s %d)",
                       __FILE__, __LINE__);
            free(l_array);
            return;
         }
         if ((*text = realloc(*text, buffer_length)) == NULL)
         {
            (void)xrec(FATAL_DIALOG,
                       "Failed to realloc() %d bytes : %s (%s %d)",
                       buffer_length, strerror(errno),
                       __FILE__, __LINE__);
            free(l_array);
            return;
         }
      }
      (void)memmove(*text + begin_underline + max_x + 1,
                    *text + begin_underline, (length - begin_underline + 1));
      (void)memset(*text + begin_underline, '=', max_x);
      *(*text + begin_underline + max_x) = '\n';
      max_y++;

      for (i = 0; i < (id.count - 1); i++)
      {
         for (j = i; j < (id.count - 1); j++)
         {
            l_array[j] += (max_x + 1);
         }
         length += (max_x + 1);
         (void)memmove(*text + l_array[i] + max_x + 1, *text + l_array[i],
                       (length - l_array[i] + 1));
         (void)memset(*text + l_array[i], '-', max_x);
         *(*text + l_array[i] + max_x) = '\n';
         max_y++;
      }
   }

   if (id.count > 0)
   {
      free(l_array);
   }

   return;
}
