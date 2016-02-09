/*
 *  get_last_char.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2013 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_last_char - gets the last character of a file
 **
 ** SYNOPSIS
 **   int get_last_char(char *file_name, off_t file_size)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   Returns the last character.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.05.2013 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                /* strerror()                         */
#include <stdlib.h>                /* exit()                             */
#include <unistd.h>                /* read(), lseek(), close()           */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "amgdefs.h"



/*########################### get_last_char() ###########################*/
int
get_last_char(char *file_name, off_t file_size)
{
   int last_char = -1;

   if (file_size > 0)
   {
      int fd;

#ifdef O_LARGEFILE
      if ((fd = open(file_name, O_RDONLY | O_LARGEFILE)) != -1)
#else
      if ((fd = open(file_name, O_RDONLY)) != -1)
#endif
      {
         if (lseek(fd, file_size - 1, SEEK_SET) != -1)
         {
            char tmp_char;

            if (read(fd, &tmp_char, 1) == 1)
            {
               last_char = (int)tmp_char;
            }
            else
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          _("Failed to read() last character from `%s' : %s"),
                          file_name, strerror(errno));
            }
         }
         else
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("Failed to lseek() in `%s' : %s"),
                       file_name, strerror(errno));
         }
         if (close(fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("Failed to close() `%s' : %s"),
                       file_name, strerror(errno));
         }
      }
   }

   return(last_char);
}
