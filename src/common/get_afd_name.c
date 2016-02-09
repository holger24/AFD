/*
 *  get_afd_name.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2014 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_afd_name - reads the name stored in afd.name file
 **
 ** SYNOPSIS
 **   int get_afd_name(char *afd_name)
 **
 ** DESCRIPTION
 **   This function stores the AFD name from the file afd.name
 **   into the buffer 'afd_name'. The calling function must provide
 **   the buffer which must be at least of MAX_AFD_NAME_LENGTH length.
 **
 ** RETURN VALUES
 **   When there is no file afd.name or it fails to read the name
 **   INCORRECT is returned. Otherwise SUCCESS and the name is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   04.07.1997 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>             /* snprintf()                             */
#include <string.h>            /* strerror()                             */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <unistd.h>            /* close()                                */
#include <errno.h>

extern char *p_work_dir;


/*############################# get_afd_name() ##########################*/
int
get_afd_name(char *afd_name)
{
   int  fd,
        n;
   char afd_file_name[MAX_PATH_LENGTH];

   if (snprintf(afd_file_name, MAX_PATH_LENGTH, "%s%s/%s", p_work_dir, ETC_DIR, AFD_NAME) >= MAX_PATH_LENGTH)
   {
      return(INCORRECT);
   }

   if ((fd = open(afd_file_name, O_RDONLY)) < 0)
   {
      return(INCORRECT);
   }

   if ((n = read(fd, afd_name, MAX_AFD_NAME_LENGTH)) < 0)
   {
      (void)close(fd);
      return(INCORRECT);
   }

   if (close(fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }

   if (n > 0)
   {
      if (afd_name[n - 1] == '\n')
      {
         afd_name[n - 1] = '\0';
      }
      else
      {
         if (n < MAX_AFD_NAME_LENGTH)
         {
            afd_name[n] = '\0';
         }
         else
         {
            afd_name[MAX_AFD_NAME_LENGTH - 1] = '\0';
         }
      }
   }
   else
   {
      return(INCORRECT);
   }

   return(SUCCESS);
}
