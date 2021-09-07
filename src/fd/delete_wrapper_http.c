/*
 *  delete_wrapper_http.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2014 - 2021 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   delete_wrapper_http - wrapper function for HTTP delete operation
 **
 ** SYNOPSIS
 **   int delete_wrapper(char *file_name)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   20.07.2014 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "fddefs.h"
#include "httpdefs.h"

/* External global variabal. */
extern char       msg_str[];
extern struct job db;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$ delete_wrapper() $$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
delete_wrapper(char *file_name)
{
   int ret;

   ret = http_del(db.target_dir, file_name);
   if (ret != SUCCESS)
   {
      trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, msg_str,
                "Failed to delete remote file %s (%d).", file_name, ret);
   }
   
   return(ret);
}
