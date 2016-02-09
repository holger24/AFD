/*
 *  read_job_ids.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2010 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   read_job_ids - Reads the job ID data file and stores values in
 **                  structure
 **
 ** SYNOPSIS
 **   int read_job_ids(char               *jid_file_name,
 **                    int                *no_of_job_ids,
 **                    struct job_id_data **jd)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   On success returns SUCCESS, otherwise INCORECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.02.2010 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* strcpy(), strcat()              */
#include <stdlib.h>                   /* malloc(), free()                */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>                    /* open()                          */
#include <unistd.h>                   /* read()                          */
#include <errno.h>

/* External global variables. */
extern char *p_work_dir;


/*############################ read_job_ids() ###########################*/
int
read_job_ids(char               *jid_file_name,
             int                *no_of_job_ids,
             struct job_id_data **jd)
{
   int  fd,
        ret = SUCCESS;
   char job_id_data_file[MAX_PATH_LENGTH],
        *p_job_id_data_file;

   if (jid_file_name == NULL)
   {
      (void)strcpy(job_id_data_file, p_work_dir);
      (void)strcat(job_id_data_file, FIFO_DIR);
      (void)strcat(job_id_data_file, JOB_ID_DATA_FILE);
      p_job_id_data_file = job_id_data_file;
   }
   else
   {
      p_job_id_data_file = jid_file_name;
   }
   if ((fd = open(p_job_id_data_file, O_RDONLY)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to open() `%s' : %s"),
                 p_job_id_data_file, strerror(errno));
      ret = INCORRECT;
   }
   else
   {
      if (read(fd, no_of_job_ids, sizeof(int)) != sizeof(int))
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to read() `%s' : %s"),
                    p_job_id_data_file, strerror(errno));
         ret = INCORRECT;
      }
      else
      {
         if (lseek(fd, AFD_WORD_OFFSET, SEEK_SET) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to lseek() `%s' : %s"),
                       p_job_id_data_file, strerror(errno));
            ret = INCORRECT;
         }
         else
         {
            size_t size;

            size = *no_of_job_ids * sizeof(struct job_id_data);
            if ((*jd = malloc(size)) == NULL)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          _("Failed to malloc() %d bytes : %s"),
                          size, strerror(errno));
               ret = INCORRECT;
            }
            else
            {
               if (read(fd, *jd, size) != size)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Failed to read() from `%s' : %s"),
                             p_job_id_data_file, strerror(errno));
                  free(*jd);
                  *jd = NULL;
                  ret = INCORRECT;
               }
            }
         }
      }
      if (close(fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("Failed to close() `%s' : %s"),
                    p_job_id_data_file, strerror(errno));
      }
   }

   return(ret);
}
