/*
 *  fra_disable_all_flag.c - Part of AFD, an automatic file distribution
 *                           program.
 *  Copyright (c) 2019, 2020 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_every_fra_disable_all_flag - goes through FRA and checks if
 **                                      the DISABLE_ALL is set correctly
 **   check_fra_disable_all_flag - checks if DISABLE_ALL needs to be set
 **                                for given FSA position
 **
 ** SYNOPSIS
 **   void check_every_fra_disable_all_flag(void)
 **   void check_fra_disable_all_flag(unsigned int host_id, int host_disabled)
 **
 ** DESCRIPTION
 **   These two function ensure that in FRA that dir_flag has DISABLE_ALL
 **   set or not.
 **
 ** RETURN VALUES
 **   Both just do the test and just return nothing even when they fail.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.10.2019 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                    /* snprintf()                      */
#include <string.h>                   /* strcpy(), strcat(), strerror()  */
#include <sys/types.h>
#include <sys/stat.h>                 /* fstat()                         */
#include <fcntl.h>                    /* open(), close()                 */
#include <unistd.h>
#include <sys/mman.h>                 /* mmap(), munmap()                */
#include <errno.h>

/* External global variables. */
extern int                        fra_fd,
                                  fsa_fd,
                                  jid_fd,
                                  no_of_hosts,
                                  no_of_dirs,
                                  no_of_job_ids;
extern char                       *p_work_dir;
extern struct fileretrieve_status *fra;
extern struct filetransfer_status *fsa;
extern struct job_id_data         *jid;


/*################# check_every_fra_disable_all_flag() ##################*/
void
check_every_fra_disable_all_flag(void)
{
   int fsa_attached = NO,
       i,
       jid_attached = NO;

   if (fsa_fd == -1)
   {
      if (fsa_attach_passive(NO, AMG) != SUCCESS)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to attach to FSA.");
         return;
      }
      fsa_attached = YES;
   }
   if (jid_fd == -1)
   {
      if (jid_attach(NO, "check_every_fra_disable_all_flag()") != SUCCESS)
      {
         if (fsa_attached == YES)
         {
            fsa_detach(NO);
         }
         return;
      }
      jid_attached = YES;
   }

   for (i = 0; i < no_of_hosts; i++)
   {
      check_fra_disable_all_flag(fsa[i].host_id,
                                 (fsa[i].special_flag & HOST_DISABLED));
   }

   if (fsa_attached == YES)
   {
      fsa_detach(NO);
   }
   if (jid_attached == YES)
   {
      jid_detach(NO);
   }

   return;
}


/*#################### check_fra_disable_all_flag() #####################*/
void
check_fra_disable_all_flag(unsigned int host_id, int host_disabled)
{
   int fra_attached = NO,
       fsa_attached = NO,
       jid_attached = NO,
       i, j, k,
       with_enabled_host;

   if (fra_fd == -1)
   {
      if (fra_attach() != SUCCESS)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to attach to FRA.");
         return;
      }
      fra_attached = YES;
   }
   if (fsa_fd == -1)
   {
      if (fsa_attach_passive(NO, AMG) != SUCCESS)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to attach to FSA.");
         if (fra_attached == YES)
         {
            fra_detach();
         }
         return;
      }
      fsa_attached = YES;
   }
   if (jid_fd == -1)
   {
      if (jid_attach(NO, "check_fra_disable_all_flag()") != SUCCESS)
      {
         if (fra_attached == YES)
         {
            fra_detach();
         }
         if (fsa_attached == YES)
         {
            fsa_detach(NO);
         }
         return;
      }
      jid_attached = YES;
   }

   for (i = 0; i < no_of_job_ids; i++)
   {
      if (jid[i].host_id == host_id)
      {
         with_enabled_host = NO;
         for (j = 0; j < no_of_job_ids; j++)
         {
            if (j != i)
            {
               if (jid[j].dir_id == jid[i].dir_id)
               {
                  if (jid[j].host_id != jid[i].host_id)
                  {
                     for (k = 0; k < no_of_hosts; k++)
                     {
                        if (fsa[k].host_id == jid[j].host_id)
                        {
                           if ((fsa[k].special_flag & HOST_DISABLED) == 0)
                           {
                              /* There is a host that is enabled, no */
                              /* need to to further checks for given */
                              /* dir_id.                             */
                              with_enabled_host = YES;
                              j = no_of_job_ids;
                              break;
                           }
                        }
                     }
                  }
               }
            }
         }

         for (k = 0; k < no_of_dirs; k++)
         {
            if (fra[k].dir_id == jid[i].dir_id)
            {
               if (host_disabled)
               {
                  if (with_enabled_host == NO)
                  {
                     fra[k].dir_flag |= ALL_DISABLED;
                  }
                  else
                  {
                     fra[k].dir_flag &= ~ALL_DISABLED;
                  }
               }
               else
               {
                  fra[k].dir_flag &= ~ALL_DISABLED;
               }
               break;
            }
         }
      }
   }

   if (fra_attached == YES)
   {
      fra_detach();
   }
   if (fsa_attached == YES)
   {
      fsa_detach(NO);
   }
   if (jid_attached == YES)
   {
      jid_detach(NO);
   }

   return;
}
