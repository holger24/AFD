/*
 *  get_real_hostname.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2018 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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

DESCR__S_M1
/*
 ** NAME
 **   get_real_hostname - determines the two real hostnames for the given
 **                       alias
 **
 ** SYNOPSIS
 **   get_real_hostname(char *alias,
 **                     char *real_hostname_1,
 **                     char *real_hostname_2)
 **
 ** DESCRIPTION
 **   The function get_real_hostname() determines the two real hostnames for
 **   the given alias name.
 **
 ** RETURN VALUES
 **   On success it returns two real hostnames in real_hostname_1 and
 **   real_hostname_2. Note, on a critical error exit() is called.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.05.2018 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr, stdout    */
#include <string.h>                      /* strcpy(), strerror()         */
#include <stdlib.h>                      /* exit()                       */
#include <errno.h>


/* External global variables. */
extern int                        fsa_id,
                                  fsa_fd,
                                  no_of_hosts;
#ifdef HAVE_MMAP
extern off_t                      fsa_size;
#endif
extern char                       *p_work_dir;
extern struct filetransfer_status *fsa;


/*######################### get_real_hostname() #########################*/
void
get_real_hostname(char *alias, char *real_hostname_1, char *real_hostname_2)
{
   int position;

   if (fsa == NULL)
   {
      int ret;

      if ((ret = fsa_attach_passive(NO, "show_stat")) != SUCCESS)
      {
         if (ret == INCORRECT_VERSION)
         {
            (void)fprintf(stderr,
                          _("ERROR   : This program is not able to attach to the FSA due to incorrect version. (%s %d)\n"),
                          __FILE__, __LINE__);
         }
         else
         {
            if (ret < 0)
            {
               (void)fprintf(stderr,
                             _("ERROR   : Failed to attach to FSA. (%s %d)\n"),
                             __FILE__, __LINE__);
            }
            else
            {
               (void)fprintf(stderr,
                             _("ERROR   : Failed to attach to FSA : %s (%s %d)\n"),
                             strerror(ret), __FILE__, __LINE__);
            }
         }
         exit(INCORRECT);
      }
   }

   if ((position = get_host_position(fsa, alias, no_of_hosts)) < 0)
   {
      real_hostname_1[0] = '\0';
      real_hostname_2[0] = '\0';
   }
   else
   {
      (void)strcpy(real_hostname_1, fsa[position].real_hostname[0]);
      (void)strcpy(real_hostname_2, fsa[position].real_hostname[1]);
   }

   return;
}
