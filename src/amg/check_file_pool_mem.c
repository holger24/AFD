/*
 *  check_file_pool_mem.c - Part of AFD, an automatic file distribution program.
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
 **   check_file_pool_mem - check if memory for all file pool data is still
 **                         large enough.
 **
 ** SYNOPSIS
 **   void check_file_pool_mem(int current_file_buffer)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None. Increases the size of file pool values.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.12.2016 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>          /* strerror()                               */
#include <stdlib.h>          /* exit()                                   */
#include <unistd.h>          /* write()                                  */
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern unsigned int               max_file_buffer;
extern char                       *file_name_buffer;
extern off_t                      *file_size_pool;
extern time_t                     *file_mtime_pool;
extern char                       **file_name_pool;
extern unsigned char              *file_length_pool;
#ifdef _DISTRIBUTION_LOG
extern unsigned int               max_jobs_per_file;
extern struct file_dist_list      **file_dist_pool;
#endif


/*######################## check_file_pool_mem() ########################*/
void
check_file_pool_mem(int current_file_buffer)
{
   if (current_file_buffer > max_file_buffer)
   {
#ifdef _DISTRIBUTION_LOG
      int          k, m;
      size_t       tmp_val;
      unsigned int prev_max_file_buffer = max_file_buffer;
#endif

      do
      {
         max_file_buffer += FILE_BUFFER_STEP_SIZE;
      } while (current_file_buffer > max_file_buffer);

      REALLOC_RT_ARRAY(file_name_pool, max_file_buffer,
                       MAX_FILENAME_LENGTH, char);
      if ((file_length_pool = realloc(file_length_pool,
                                      max_file_buffer * sizeof(unsigned char))) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("realloc() error : %s"), strerror(errno));
         exit(INCORRECT);
      }
      if ((file_mtime_pool = realloc(file_mtime_pool,
                                     max_file_buffer * sizeof(time_t))) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("realloc() error : %s"), strerror(errno));
         exit(INCORRECT);
      }
      if ((file_size_pool = realloc(file_size_pool,
                                    max_file_buffer * sizeof(off_t))) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("realloc() error : %s"), strerror(errno));
         exit(INCORRECT);
      }
#ifdef _DISTRIBUTION_LOG
# ifdef RT_ARRAY_STRUCT_WORKING
      REALLOC_RT_ARRAY(file_dist_pool, max_file_buffer,
                       NO_OF_DISTRIBUTION_TYPES, struct file_dist_list);
# else
      if ((file_dist_pool = (struct file_dist_list **)realloc(file_dist_pool,
                                                              max_file_buffer * NO_OF_DISTRIBUTION_TYPES * sizeof(struct file_dist_list *))) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("realloc() error : %s"), strerror(errno));
         exit(INCORRECT);
      }
      if ((file_dist_pool[0] = (struct file_dist_list *)realloc(file_dist_pool[0],
                                                                max_file_buffer * NO_OF_DISTRIBUTION_TYPES * sizeof(struct file_dist_list))) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("realloc() error : %s"), strerror(errno));
         exit(INCORRECT);
      }
      for (k = 0; k < max_file_buffer; k++)
      {
         file_dist_pool[k] = file_dist_pool[0] + (k * NO_OF_DISTRIBUTION_TYPES);
      }
# endif
      tmp_val = max_jobs_per_file * sizeof(unsigned int);
      for (k = prev_max_file_buffer; k < max_file_buffer; k++)
      {
         for (m = 0; m < NO_OF_DISTRIBUTION_TYPES; m++)
         {
            if (((file_dist_pool[k][m].jid_list = malloc(tmp_val)) == NULL) ||
                ((file_dist_pool[k][m].proc_cycles = malloc(max_jobs_per_file)) == NULL))
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          _("malloc() error : %s"),
                          strerror(errno));
               exit(INCORRECT);
            }
            file_dist_pool[k][m].no_of_dist = 0;
         }
      }
#endif              
   }

   return;
}
