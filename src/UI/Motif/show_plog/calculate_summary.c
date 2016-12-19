/*
 *  calculate_summary.c - Part of AFD, an automatic file distribution
 *                        program.
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
 **   calculate_summary - calculates and creates a summary string
 **
 ** SYNOPSIS
 **   void calculate_summary(char         *summary_str,
 **                          time_t       first_date_found,
 **                          time_t       last_date_found,
 **                          unsigned int total_no_files,
 **                          double       orig_file_size,
 **                          double       new_file_size,
 **                          double       prod_time,
 **                          double       cpu_time)
 **
 ** DESCRIPTION
 **   This function calculates the summary for the production log and
 **   writes the summary to the string summary_str, which will
 **   look as follows:
 **    1 13:16:46  7906 Files    940.12 MB     810.92 MB 1h 25m  2m 24s
 **    ----+-----  ----+-----    ---+-----     ----+---- ---+--  ---+--
 **        |           |            |              |        |       |
 **        V           V            V              V        V       V
 **        1           2            3              4        5       6
 **
 **    1 - Time that has passed starting from the first file found to
 **        the last file in the following format: ddd hh:mm:ss
 **    2 - Total number of files found/selected.
 **    3 - Summary of the size of all original files found/selected.
 **    4 - Summary of the size of all new files found/selected.
 **    5 - Summary of the total production time.
 **    6 - Summary of the total cpu time used.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.09.2016 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>               /* sprintf()                            */
#include <string.h>              /* memset()                             */
#include "show_plog.h"

/* External global variables. */
extern int file_name_length;


/*####################### calculate_summary() ###########################*/
void
calculate_summary(char         *summary_str,
                  time_t       first_date_found,
                  time_t       last_date_found,
                  unsigned int total_no_files,
                  double       orig_file_size,
                  double       new_file_size,
                  double       prod_time,
                  double       cpu_time)
{
   int          hours,
                min,
                length;
   time_t       total_time;
   double       average;
   char         *p_summary_str;

   (void)memset(summary_str, ' ', MAX_PRODUCTION_LINE_LENGTH + file_name_length + file_name_length + 5);
   *(summary_str + MAX_PRODUCTION_LINE_LENGTH + file_name_length + file_name_length + 5) = '\0';

   if ((first_date_found != -1) &&
       ((total_time = last_date_found - first_date_found) > 0L))
   {
      int days;

      days = total_time / 86400;
      total_time = total_time - (days * 86400);
      hours = total_time / 3600;
      total_time -= (hours * 3600);
      min = total_time / 60;
      total_time -= (min * 60);
      length = sprintf(summary_str, "%5d  %02d:%02d:%02d",
                       days, hours, min, (int)total_time);
      summary_str[length] = ' ';
   }
   else
   {
      total_time = 0L;
      summary_str[0] = ' ';
      summary_str[1] = ' ';
      summary_str[2] = ' ';
      summary_str[3] = ' ';
      summary_str[4] = '0';
      summary_str[5] = ' ';
      summary_str[6] = ' ';
      summary_str[7] = '0';
      summary_str[8] = '0';
      summary_str[9] = ':';
      summary_str[10] = '0';
      summary_str[11] = '0';
      summary_str[12] = ':';
      summary_str[13] = '0';
      summary_str[14] = '0';
      length = 15;
   }
   p_summary_str = summary_str + length + 1;
   length = sprintf(p_summary_str, "%u Files", total_no_files);
   *(p_summary_str + length) = ' ';
   p_summary_str += file_name_length + 1;
   if (orig_file_size < F_KILOBYTE)
   {
      length = sprintf(p_summary_str, "%*.0f Bytes",
                       MAX_DISPLAYED_FILE_SIZE, orig_file_size);
   }
   else if (orig_file_size < F_MEGABYTE)
        {
           length = sprintf(p_summary_str, "%*.2f KB",
                            MAX_DISPLAYED_FILE_SIZE,
                            orig_file_size / F_KILOBYTE);
        }
   else if (orig_file_size < F_GIGABYTE)
        {
           length = sprintf(p_summary_str, "%*.2f MB",
                            MAX_DISPLAYED_FILE_SIZE,
                            orig_file_size / F_MEGABYTE);
        }
   else if (orig_file_size < F_TERABYTE)
        {
           length = sprintf(p_summary_str, "%*.2f GB",
                            MAX_DISPLAYED_FILE_SIZE,
                            orig_file_size / F_GIGABYTE);
        }
   else if (orig_file_size < F_PETABYTE)
        {
           length = sprintf(p_summary_str, "%*.2f TB",
                            MAX_DISPLAYED_FILE_SIZE,
                            orig_file_size / F_TERABYTE);
        }
   else if (orig_file_size < F_EXABYTE)
        {
           length = sprintf(p_summary_str, "%*.2f PB",
                            MAX_DISPLAYED_FILE_SIZE,
                            orig_file_size / F_PETABYTE);
        }
        else
        {
           length = sprintf(p_summary_str, "%*.2f EB",
                            MAX_DISPLAYED_FILE_SIZE,
                            orig_file_size / F_EXABYTE);
        }
   *(p_summary_str + length) = ' ';
   p_summary_str += MAX_DISPLAYED_FILE_SIZE + 1 + file_name_length + 1;
   if (new_file_size < F_KILOBYTE)
   {
      length = sprintf(p_summary_str, "%*.0f Bytes",
                       MAX_DISPLAYED_FILE_SIZE, new_file_size);
   }
   else if (new_file_size < F_MEGABYTE)
        {
           length = sprintf(p_summary_str, "%*.2f KB",
                            MAX_DISPLAYED_FILE_SIZE,
                            new_file_size / F_KILOBYTE);
        }
   else if (new_file_size < F_GIGABYTE)
        {
           length = sprintf(p_summary_str, "%*.2f MB",
                            MAX_DISPLAYED_FILE_SIZE,
                            new_file_size / F_MEGABYTE);
        }
   else if (new_file_size < F_TERABYTE)
        {
           length = sprintf(p_summary_str, "%*.2f GB",
                            MAX_DISPLAYED_FILE_SIZE,
                            new_file_size / F_GIGABYTE);
        }
   else if (new_file_size < F_PETABYTE)
        {
           length = sprintf(p_summary_str, "%*.2f TB",
                            MAX_DISPLAYED_FILE_SIZE,
                            new_file_size / F_TERABYTE);
        }
   else if (new_file_size < F_EXABYTE)
        {
           length = sprintf(p_summary_str, "%*.2f PB",
                            MAX_DISPLAYED_FILE_SIZE,
                            new_file_size / F_PETABYTE);
        }
        else
        {
           length = sprintf(p_summary_str, "%*.2f EB",
                            MAX_DISPLAYED_FILE_SIZE,
                            new_file_size / F_EXABYTE);
        }
   *(p_summary_str + length) = ' ';
   p_summary_str += MAX_DISPLAYED_FILE_SIZE + 1 + MAX_DISPLAYED_RATIO + 1 + MAX_DISPLAYED_COMMAND + 1 + MAX_DISPLAYED_RC + 1;
   average = prod_time;
   hours = average / 3600;
   average -= (hours * 3600);
   if (hours > 0)
   {
      length = sprintf(p_summary_str, "%3dh %02dm", hours, (int)average / 60);
   }
   else
   {
      min = average / 60;
      average -= (min * 60);
      if (min > 0)
      {
         length = sprintf(p_summary_str, "%3dm %02ds", min, (int)average);
      }
      else
      {
         length = sprintf(p_summary_str, "%7.3fs", average);
      }
   }
   p_summary_str += MAX_DISPLAYED_PROD_TIME + 1;
   average = cpu_time;
   hours = average / 3600;
   average -= (hours * 3600);
   if (hours > 0)
   {
      length = sprintf(p_summary_str, "%3dh %02dm", hours, (int)average / 60);
   }
   else
   {
      min = average / 60;
      average -= (min * 60);
      if (min > 0)
      {
         length = sprintf(p_summary_str, "%3dm %02ds", min, (int)average);
      }
      else
      {
         length = sprintf(p_summary_str, "%7.3fs", average);
      }
   }
   *(p_summary_str + length) = ' ';

   return;
}
