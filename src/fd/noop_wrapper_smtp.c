/*
 *  noop_wrapper_smtp.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2009 - 2021 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   noop_wrapper_smtp - wrapper function for SMTP NOOP operation
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
 **   21.02.2009 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdlib.h>             /* exit()                                */
#include "fddefs.h"
#include "smtpdefs.h"

/* External global variabal. */
extern int  exitflag,
            timeout_flag;
extern char msg_str[];


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$ noop_wrapper() $$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
noop_wrapper(void)
{
   int ret;

   ret = smtp_noop();
   if (ret != SUCCESS)
   {
      if (timeout_flag == CON_RESET)
      {
         trans_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                   (ret == INCORRECT) ? NULL : msg_str,
                   "Connection closed by remote server.");
         exitflag = 0;
      }
      else
      {
         trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                   (ret == INCORRECT) ? NULL : msg_str,
                   "Failed to send NOOP command.");
      }
      (void)smtp_quit();
      exit(NOOP_ERROR);
   }
   
   return(ret);
}
