/*
 *  make_fifo.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2009 Deutscher Wetterdienst (DWD),
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
 **   make_fifo - creates a new fifo
 **
 ** SYNOPSIS
 **   int make_fifo(char *fifoname)
 **
 ** DESCRIPTION
 **   The function make_fifo() creates the fifo 'fifoname' with read
 **   and write permissions of the owner.
 **
 ** RETURN VALUES
 **   Returns SUCCESS when the fifo was created. Otherwise it returns
 **   INCORRECT.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.12.1995 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>                      /* strerror()                   */
#include <sys/types.h>
#include <sys/stat.h>                    /* mkfifo()                     */
#include <errno.h>


/*############################ make_fifo() ##############################*/
int
make_fifo(char *fifoname)
{
   /* Create new fifo. */
#ifdef GROUP_CAN_WRITE
   if (mkfifo(fifoname, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) == -1)
#else
   if (mkfifo(fifoname, S_IRUSR | S_IWUSR) == -1)
#endif
   {
      if (errno != EEXIST)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not create fifo `%s' : %s"),
                    fifoname, strerror(errno));
         return(INCORRECT);
      }
   }

   return(SUCCESS);
}
