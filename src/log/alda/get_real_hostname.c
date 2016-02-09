/*
 *  get_real_hostname.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 - 2013 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_real_hostname - get the real hostname
 **
 ** SYNOPSIS
 **   int  get_real_hostname(char *host_alias, int current_toggle,
 **                          char *real_hostname)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   23.01.2008 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>         /* fprintf()                                  */
#include <string.h>        /* strcmp(), strcpy()                         */
#include "aldadefs.h"
#ifdef WITH_AFD_MON
# include "mondefs.h"
#endif


/* External global variables. */
extern int                        fsa_fd,
                                  mode,
                                  no_of_hosts;
extern struct filetransfer_status *fsa;
#ifdef WITH_AFD_MON
extern unsigned int               ahl_entries;
extern struct afd_host_list       *ahl;
#endif


/*######################### get_real_hostname() #########################*/
int
get_real_hostname(char *host_alias, int current_toggle, char *real_hostname)
{
   int ret = INCORRECT;

   real_hostname[0] = '\0';
   if (mode & ALDA_LOCAL_MODE)
   {
      int we_attached;

      if (fsa_fd == -1)
      {
         if (fsa_attach_passive(NO, ALDA_CMD) != SUCCESS)
         {
            (void)fprintf(stderr, "Failed to attach to FSA. (%s %d)\n",
                          __FILE__, __LINE__);
            we_attached = NO;
         }
         else
         {
            we_attached = YES;
         }
      }
      else
      {
         we_attached = NO;
      }
      if (fsa_fd != -1)
      {
         int i;

         for (i = 0; i < no_of_hosts; i++)
         {
            if (strcmp(host_alias, fsa[i].host_alias) == 0)
            {
               if (current_toggle == -1)
               {
                  (void)strcpy(real_hostname, fsa[i].real_hostname[0]);
               }
               else
               {
                  (void)strcpy(real_hostname,
                               fsa[i].real_hostname[current_toggle]);
               }
               ret = i;

               break;
            }
         }
      }
      if (we_attached == YES)
      {
         (void)fsa_detach(NO);
      }
   }
#ifdef WITH_AFD_MON
   else
   {
      int i;

      for (i = 0; i < ahl_entries; i++)
      {
         if (strcmp(host_alias, ahl[i].host_alias) == 0)
         {
            if (current_toggle == -1)
            {
               (void)strcpy(real_hostname, ahl[i].real_hostname[0]);
            }
            else
            {
               (void)strcpy(real_hostname,
                            ahl[i].real_hostname[current_toggle]);
            }
            ret = i;

            break;
         }
      }
   }
#endif

   return(ret);
}
