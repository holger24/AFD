/*
 *  calculate_summary.c - Part of AFD, an automatic file distribution
 *                        program.
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
 **   calculate_summary - calculates and creates a summary string
 **
 ** SYNOPSIS
 **   void calculate_summary(char         *summary_str,
 **                          time_t       first_date_found,
 **                          time_t       last_date_found,
 **                          unsigned int total_no_files,
 **                          double       file_size)
 **
 ** DESCRIPTION
 **   This function calculates the summary for the input log and
 **   writes the summary to the string summary_str, which will
 **   look as follows:
 **    7 20:41:14 4956 Files (0.44 Files/m)       866.11 MB
 **    ----+----- -----+----  -----+-------       ----+----
 **        |           |           |                  |
 **        V           V           V                  V
 **        1           2           3                  4
 **
 **    1 - Time that has passed starting from the first file found to
 **        the last file in the following format: ddd hh:mm:ss
 **    2 - Total number of files found/selected.
 **    3 - Average file rate for files found/selected.
 **    4 - Summary of the size of all files found/selected.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.03.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>               /* sprintf()                            */
#include <string.h>              /* memset()                             */
#include "show_ilog.h"

/* External global variables. */
extern int file_name_length;


/*####################### calculate_summary() ###########################*/
void
calculate_summary(char         *summary_str,
                  time_t       first_date_found,
                  time_t       last_date_found,
                  unsigned int total_no_files,
                  double       file_size)
{
   int    length;
   time_t total_time;
   double file_rate;
   char   file_rate_unit,
          *p_summary_str;

   (void)memset(summary_str, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);
   if ((first_date_found != -1) &&
       ((total_time = last_date_found - first_date_found) > 0L))
   {
      int days,
          hours,
          min;

      file_rate = (double)(total_no_files / (double)total_time);
      if (file_rate < 1.0)
      {
          file_rate *= 60.0;
          if (file_rate < 1.0)
          {
             file_rate *= 60.0;
             if (file_rate < 1.0)
             {
                file_rate *= 24.0;
                if (file_rate < 1.0)
                {
                   file_rate *= 365.0;
                   file_rate_unit = 'y';
                }
                else
                {
                   file_rate_unit = 'd';
                }
             }
             else
             {
                file_rate_unit = 'h';
             }
          }
          else
          {
             file_rate_unit = 'm';
          }
      }
      else
      {
         file_rate_unit = 's';
      }
      days = total_time / 86400;
      total_time = total_time - (days * 86400);
      hours = total_time / 3600;
      total_time -= (hours * 3600);
      min = total_time / 60;
      total_time -= (min * 60);
      length = sprintf(summary_str, "%5d  %02d:%02d:%02d %d Files (",
                       days, hours, min, (int)total_time,
                       total_no_files);
   }
   else
   {
      total_time = 0L;
      length = sprintf(summary_str, "    0  00:00:00 %d Files (",
                       total_no_files);
      file_rate = (double)total_no_files;
      file_rate_unit = 's';
   }
   p_summary_str = summary_str;
   length += sprintf((p_summary_str + length), "%.2f Files/%c)",
                     file_rate, file_rate_unit);
   *(p_summary_str + length) = ' ';
   p_summary_str += 16 + file_name_length + 1;
   if (file_size < F_KILOBYTE)
   {
      (void)sprintf(p_summary_str, "%4.0f Bytes ", file_size);
   }
   else if (file_size < F_MEGABYTE)
        {
           (void)sprintf(p_summary_str, "%7.2f KB ", file_size / F_KILOBYTE);
        }
   else if (file_size < F_GIGABYTE)
        {
           (void)sprintf(p_summary_str, "%7.2f MB ", file_size / F_MEGABYTE);
        }
   else if (file_size < F_TERABYTE)
        {
           (void)sprintf(p_summary_str, "%7.2f GB ", file_size / F_GIGABYTE);
        }
   else if (file_size < F_PETABYTE)
        {
           (void)sprintf(p_summary_str, "%7.2f TB ", file_size / F_TERABYTE);
        }
   else if (file_size < F_EXABYTE)
        {
           (void)sprintf(p_summary_str, "%7.2f PB ", file_size / F_PETABYTE);
        }
        else
        {
           (void)sprintf(p_summary_str, "%7.2f EB ", file_size / F_EXABYTE);
        }

   return;
}
