/*
 *  noop_wrapper_http.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2021 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   noop_wrapper_http - wrapper function for HTTP NOOP operation
 **
 ** SYNOPSIS
 **   int noop_wrapper(void)
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
 **   12.04.2021 H.Kiehl Created
 **
 */
DESCR__E_M3

/* #define DO_REAL_NOOP_HTTP 1 */

#ifdef DO_REAL_NOOP_HTTP
# include <stdlib.h>            /* exit()                                */
# include "httpdefs.h"
#endif
#include "fddefs.h"

/* External global variabal. */
#ifdef DO_REAL_NOOP_HTTP
extern int        exitflag,
                  timeout_flag;
extern char       msg_str[];
extern struct job db;
#endif


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$ noop_wrapper() $$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
noop_wrapper(void)
{
#ifndef DO_REAL_NOOP_HTTP
   return(SUCCESS);
#else
   int ret;

   ret = http_noop(db.target_dir);
   if (ret != SUCCESS)
   {
      if (ret == 301) /* Moved Permanently */
      {
         ret = SUCCESS;
      }
      else
      {
         if (timeout_flag == CON_RESET)
         {
            trans_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                      (ret == INCORRECT) ? NULL : msg_str,
                      "Connection closed by remote server.");
         }
         else
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                      (ret == INCORRECT) ? NULL : msg_str,
                      "Failed to send NOOP command.");
         }
         exitflag = 0;
         http_quit();
         exit(NOOP_ERROR);
      }
   }
   
   return(ret);
#endif
}
