/*
 *  eval_alda_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2009 - 2016 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   eval_alda_data - converts ALDA text data to a structure
 **
 ** SYNOPSIS
 **   void eval_alda_data(char *text)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   The filled structure.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.08.2009 H.Kiehl Created
 **   16.08.2016 H.Kiehl Added production log filenames (input + output).
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <stdlib.h>                /* malloc(), free(), strtoul()        */
#include <errno.h>
#include "ui_common_defs.h"

/* External global variables. */
extern int                   acd_counter;
extern struct alda_call_data *acd;


/*########################### eval_alda_data() ##########################*/
void
eval_alda_data(char *text)
{
   int  length;
   char *ptr,
#if MAX_TIME_T_HEX_LENGTH > MAX_OFF_T_HEX_LENGTH
        str_val[2 + MAX_TIME_T_HEX_LENGTH + 1];
#else
        str_val[2 + MAX_OFF_T_HEX_LENGTH + 1];
#endif

   if (acd != NULL)
   {
      int i;

      for (i = 0; i < acd_counter; i++)
      {
         if (acd[i].job_id_list != NULL)
         {
            free(acd[i].job_id_list);
         }
      }
      free(acd);
      acd = NULL;
   }
   acd_counter = 0;
   if ((acd = malloc(sizeof(struct alda_call_data))) == NULL)
   {
      (void)fprintf(stderr, "malloc() error : %s\n", strerror(errno));
      exit(INCORRECT);
   }
   ptr = text;
   while (*ptr != '\0')
   {
      /* Alias name. */
      length = 0;
      while ((*ptr != '|') && (*ptr > '\n') &&
             (length < MAX_REAL_HOSTNAME_LENGTH))
      {
         acd[acd_counter].alias_name[length] = *ptr;
         length++; ptr++;
      }
      acd[acd_counter].alias_name[length] = '\0';
      if (*ptr != '|')
      {
         while ((*ptr != '|') && (*ptr > '\n'))
         {
            ptr++;
         }
      }
      else
      {
         ptr++;
      }

      /* Real hostname. */
      length = 0;
      while ((*ptr != '|') && (*ptr > '\n') &&
             (length < MAX_REAL_HOSTNAME_LENGTH))
      {
         acd[acd_counter].real_hostname[length] = *ptr;
         length++; ptr++;
      }
      acd[acd_counter].real_hostname[length] = '\0';
      if (*ptr != '|')
      {
         while ((*ptr != '|') && (*ptr > '\n'))
         {
            ptr++;
         }
      }
      else
      {
         ptr++;
      }

      /* Final name at destination. */
      length = 0;
      while ((*ptr != '|') && (*ptr > '\n') && (length < MAX_PATH_LENGTH))
      {
         acd[acd_counter].final_name[length] = *ptr;
         length++; ptr++;
      }
      acd[acd_counter].final_name[length] = '\0';
      if (*ptr != '|')
      {
         while ((*ptr != '|') && (*ptr > '\n'))
         {
            ptr++;
         }
      }
      else
      {
         ptr++;
      }

      /* Delivery size of data. */
      length = 0;
      str_val[0] = '0';
      str_val[1] = 'x';
      while ((*ptr != '|') && (*ptr > '\n') && (length < MAX_OFF_T_HEX_LENGTH))
      {
         str_val[2 + length] = *ptr;
         length++; ptr++;
      }
      str_val[2 + length] = '\0';
      if (*ptr != '|')
      {
         while ((*ptr != '|') && (*ptr > '\n'))
         {
            ptr++;
         }
      }
      else
      {
         ptr++;
      }
      acd[acd_counter].final_size = (pri_off_t)str2offt(str_val, NULL, 16);

      /* Final size in human readable form. */
      length = 0;
      while ((*ptr != '|') && (*ptr > '\n') && (length < 12))
      {
         acd[acd_counter].hr_final_size[length] = *ptr;
         length++; ptr++;
      }
      acd[acd_counter].hr_final_size[length] = '\0';
      if (*ptr != '|')
      {
         while ((*ptr != '|') && (*ptr > '\n'))
         {
            ptr++;
         }
      }
      else
      {
         ptr++;
      }

      /* Time when data was delivered. */
      length = 0;
      str_val[0] = '0';
      str_val[1] = 'x';
      while ((*ptr != '|') && (*ptr > '\n') && (length < MAX_TIME_T_HEX_LENGTH))
      {
         str_val[2 + length] = *ptr;
         length++; ptr++;
      }
      str_val[2 + length] = '\0';
      if (*ptr != '|')
      {
         while ((*ptr != '|') && (*ptr > '\n'))
         {
            ptr++;
         }
      }
      else
      {
         ptr++;
      }
      acd[acd_counter].delivery_time = (pri_time_t)str2timet(str_val, NULL, 16);

      /* Transmission time. */
      length = 0;
      while ((*ptr != '|') && (*ptr > '\n') &&
             (length < (MAX_INT_LENGTH + 1 + 2 + 1 + 2 + 1 + 1)))
      {
         acd[acd_counter].transmission_time[length] = *ptr;
         length++; ptr++;
      }
      acd[acd_counter].transmission_time[length] = '\0';
      if (*ptr != '|')
      {
         while ((*ptr != '|') && (*ptr > '\n'))
         {
            ptr++;
         }
      }
      else
      {
         ptr++;
      }

      /* Output Job ID. */
      length = 0;
      str_val[0] = '0';
      str_val[1] = 'x';
      while ((*ptr != '|') && (*ptr > '\n') && (length < MAX_INT_HEX_LENGTH))
      {
         str_val[2 + length] = *ptr;
         length++; ptr++;
      }
      str_val[2 + length] = '\0';
      if (*ptr != '|')
      {
         while ((*ptr != '|') && (*ptr > '\n'))
         {
            ptr++;
         }
      }
      else
      {
         ptr++;
      }
      acd[acd_counter].output_job_id = (unsigned int)strtoul(str_val, NULL, 16);

      /* Number of retries. */
      length = 0;
      str_val[0] = '0';
      str_val[1] = 'x';
      while ((*ptr != '|') && (*ptr > '\n') && (length < MAX_INT_HEX_LENGTH))
      {
         str_val[2 + length] = *ptr;
         length++; ptr++;
      }
      str_val[2 + length] = '\0';
      if (*ptr != '|')
      {
         while ((*ptr != '|') && (*ptr > '\n'))
         {
            ptr++;
         }
      }
      else
      {
         ptr++;
      }
      acd[acd_counter].retries = (unsigned int)strtoul(str_val, NULL, 16);

      /* Split Job Number. */
      length = 0;
      str_val[0] = '0';
      str_val[1] = 'x';
      while ((*ptr != '|') && (*ptr > '\n') && (length < MAX_INT_HEX_LENGTH))
      {
         str_val[2 + length] = *ptr;
         length++; ptr++;
      }
      str_val[2 + length] = '\0';
      if (*ptr != '|')
      {
         while ((*ptr != '|') && (*ptr > '\n'))
         {
            ptr++;
         }
      }
      else
      {
         ptr++;
      }
      acd[acd_counter].split_job_counter = (unsigned int)strtoul(str_val, NULL, 16);

      /* Archive Directory. */
      length = 0;
      while ((*ptr != '|') && (*ptr > '\n') && (length < MAX_PATH_LENGTH))
      {
         acd[acd_counter].archive_dir[length] = *ptr;
         length++; ptr++;
      }
      acd[acd_counter].archive_dir[length] = '\0';
      if (*ptr != '|')
      {
         while ((*ptr != '|') && (*ptr > '\n'))
         {
            ptr++;
         }
      }
      else
      {
         ptr++;
      }

      /* Time when data was deleted. */
      length = 0;
      str_val[0] = '0';
      str_val[1] = 'x';
      while ((*ptr != '|') && (*ptr > '\n') && (length < MAX_TIME_T_HEX_LENGTH))
      {
         str_val[2 + length] = *ptr;
         length++; ptr++;
      }
      str_val[2 + length] = '\0';
      if (*ptr != '|')
      {
         while ((*ptr != '|') && (*ptr > '\n'))
         {
            ptr++;
         }
      }
      else
      {
         ptr++;
      }
      acd[acd_counter].delete_time = (pri_time_t)str2timet(str_val, NULL, 16);

      /* Delete Job ID. */
      length = 0;
      str_val[0] = '0';
      str_val[1] = 'x';
      while ((*ptr != '|') && (*ptr > '\n') && (length < MAX_INT_HEX_LENGTH))
      {
         str_val[2 + length] = *ptr;
         length++; ptr++;
      }
      str_val[2 + length] = '\0';
      if (*ptr != '|')
      {
         while ((*ptr != '|') && (*ptr > '\n'))
         {
            ptr++;
         }
      }
      else
      {
         ptr++;
      }
      acd[acd_counter].delete_job_id = (unsigned int)strtoul(str_val, NULL, 16);

      /* Production log input file name. */
      length = 0;
      while ((*ptr != '|') && (*ptr > '\n') && (length < MAX_FILENAME_LENGTH))
      {
         acd[acd_counter].production_input_name[length] = *ptr;
         length++; ptr++;
      }
      acd[acd_counter].production_input_name[length] = '\0';
      if (*ptr != '|')
      {
         while ((*ptr != '|') && (*ptr > '\n'))
         {
            ptr++;
         }
      }
      else
      {
         ptr++;
      }

      /* Production log final file name. */
      length = 0;
      while ((*ptr != '|') && (*ptr > '\n') && (length < MAX_FILENAME_LENGTH))
      {
         acd[acd_counter].production_final_name[length] = *ptr;
         length++; ptr++;
      }
      acd[acd_counter].production_final_name[length] = '\0';
      if (*ptr != '|')
      {
         while ((*ptr != '|') && (*ptr > '\n'))
         {
            ptr++;
         }
      }
      else
      {
         ptr++;
      }

      /* Production Job ID. */
      length = 0;
      str_val[0] = '0';
      str_val[1] = 'x';
      while ((*ptr != '|') && (*ptr > '\n') && (length < MAX_INT_HEX_LENGTH))
      {
         str_val[2 + length] = *ptr;
         length++; ptr++;
      }
      str_val[2 + length] = '\0';
      if (*ptr != '|')
      {
         while ((*ptr != '|') && (*ptr > '\n'))
         {
            ptr++;
         }
      }
      else
      {
         ptr++;
      }
      acd[acd_counter].production_job_id = (unsigned int)strtoul(str_val, NULL, 16);

      /* Distribution type. */
      length = 0;
      str_val[0] = '0';
      str_val[1] = 'x';
      while ((*ptr != '|') && (*ptr > '\n') && (length < MAX_INT_HEX_LENGTH))
      {
         str_val[2 + length] = *ptr;
         length++; ptr++;
      }
      str_val[2 + length] = '\0';
      if (*ptr != '|')
      {
         while ((*ptr != '|') && (*ptr > '\n'))
         {
            ptr++;
         }
      }
      else
      {
         ptr++;
      }
      acd[acd_counter].distribution_type = (unsigned int)strtoul(str_val, NULL, 16);

      /* Number of distribution job ID's. */
      length = 0;
      str_val[0] = '0';
      str_val[1] = 'x';
      while ((*ptr != '|') && (*ptr > '\n') && (length < MAX_INT_HEX_LENGTH))
      {
         str_val[2 + length] = *ptr;
         length++; ptr++;
      }
      str_val[2 + length] = '\0';
      if (*ptr != '|')
      {
         while ((*ptr != '|') && (*ptr > '\n'))
         {
            ptr++;
         }
      }
      else
      {
         ptr++;
      }
      acd[acd_counter].no_of_distribution_types = (unsigned int)strtoul(str_val, NULL, 16);

#ifdef _DISTRIBUTION_LOG
      if (acd[acd_counter].distribution_type == DISABLED_DIS_TYPE)
      {
         int i;

         if ((acd[acd_counter].job_id_list = malloc(acd[acd_counter].no_of_distribution_types * sizeof(unsigned int))) == NULL)
         {
            (void)fprintf(stderr, "malloc() error : %s\n", strerror(errno));
            exit(INCORRECT);
         }
         for (i = 0; i < acd[acd_counter].no_of_distribution_types; i++)
         {
            length = 0;
            str_val[0] = '0';
            str_val[1] = 'x';
            while ((*ptr != ',') && (*ptr != '|') && (*ptr > '\n') &&
                   (length < MAX_INT_HEX_LENGTH))
            {
               str_val[2 + length] = *ptr;
               length++; ptr++;
            }
            str_val[2 + length] = '\0';
            if ((*ptr != ',') && (*ptr != '|'))
            {
               while ((*ptr != ',') && (*ptr != '|') && (*ptr > '\n'))
               {
                  ptr++;
               }
            }
            else
            {
               ptr++;
            }
            acd[acd_counter].job_id_list[i] = (unsigned int)strtoul(str_val, NULL, 16);
         }
      }
      else
      {
#endif
         acd[acd_counter].job_id_list = NULL;
         while ((*ptr != '|') && (*ptr > '\n'))
         {
            ptr++;
         }
         if (*ptr == '|')
         {
            ptr++;
         }
#ifdef _DISTRIBUTION_LOG
      }
#endif

      /* Delete reason ID. */
      length = 0;
      str_val[0] = '0';
      str_val[1] = 'x';
      while ((*ptr != '|') && (*ptr > '\n') && (length < MAX_INT_HEX_LENGTH))
      {
         str_val[2 + length] = *ptr;
         length++; ptr++;
      }
      str_val[2 + length] = '\0';
      if (*ptr != '|')
      {
         while ((*ptr != '|') && (*ptr > '\n'))
         {
            ptr++;
         }
      }
      else
      {
         ptr++;
      }
      acd[acd_counter].delete_type = (unsigned int)strtoul(str_val, NULL, 16);

      /* User/process that deleted the data. */
      length = 0;
      while ((*ptr != '|') && (*ptr > '\n') && (length < MAX_USER_NAME_LENGTH))
      {
         acd[acd_counter].user_process[length] = *ptr;
         length++; ptr++;
      }
      acd[acd_counter].user_process[length] = '\0';
      if (*ptr != '|')
      {
         while ((*ptr != '|') && (*ptr > '\n'))
         {
            ptr++;
         }
      }
      else
      {
         ptr++;
      }

      /* Additional reason why data was deleted. */
      length = 0;
      while ((*ptr != '|') && (*ptr > '\n') && (length < MAX_PATH_LENGTH))
      {
         acd[acd_counter].add_reason[length] = *ptr;
         length++; ptr++;
      }
      acd[acd_counter].add_reason[length] = '\0';
      if (*ptr != '|')
      {
         while ((*ptr != '|') && (*ptr > '\n'))
         {
            ptr++;
         }
      }
      else
      {
         ptr++;
      }

      acd_counter++;
      if (*ptr == '\n')
      {
         ptr++;
         if ((acd = realloc(acd,
                            ((acd_counter + 1) * sizeof(struct alda_call_data)))) == NULL)
         {
            (void)fprintf(stderr, "realloc() error : %s\n", strerror(errno));
            exit(INCORRECT);
         }
      }
   }

   return;
}
