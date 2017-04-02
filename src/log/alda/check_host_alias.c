/*
 *  check_host_alias.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 - 2017 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_host_alias - checks if given host_alias matches
 **
 ** SYNOPSIS
 **   int check_host_alias(char *host_alias,
 **                        char *real_hostname,
 **                        int  current_toggle)
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
#include "aldadefs.h"
#ifdef WITH_AFD_MON
# include "mondefs.h"
#endif


/* External global variables. */
extern int                        fsa_fd,
                                  no_of_hosts;
extern unsigned int               mode,
                                  search_host_alias_counter,
                                  *search_host_id,
                                  search_host_id_counter,
                                  search_host_name_counter;
extern char                       **search_host_alias,
                                  **search_host_name;
extern struct filetransfer_status *fsa;
#ifdef WITH_AFD_MON
extern unsigned int               start_alias_counter,
                                  start_id_counter,
                                  *start_id,
                                  start_name_counter;
extern char                       **start_alias,
                                  **start_name;
extern struct afd_host_list       *ahl;
#endif


/*########################## check_host_alias() #########################*/
int
check_host_alias(char *host_alias, char *real_hostname, int current_toggle)
{
   if ((search_host_alias_counter != 0) || (search_host_id_counter != 0) ||
       (search_host_name_counter != 0))
   {
      int i,
          ret;

      if (search_host_alias_counter != 0)
      {
         for (i = 0; i < search_host_alias_counter; i++)
         {
            if ((ret = pmatch(search_host_alias[i], host_alias, NULL)) == 0)
            {
               return(SUCCESS);
            }
            else if (ret == 1)
                 {
                    /*
                     * This alias is definitly not wanted,
                     * so no need to check the other filters.
                     */
                    i = search_host_alias_counter;
                 }
         }
      }

      if (mode & ALDA_LOCAL_MODE)
      {
         if (search_host_id_counter != 0)
         {
            if (fsa_fd == -1)
            {
               if (fsa_attach_passive(NO, ALDA_CMD) != SUCCESS)
               {
                  (void)fprintf(stderr, "Failed to attach to FSA. (%s %d)\n",
                                __FILE__, __LINE__);
               }
            }
            if (fsa_fd != -1)
            {
               int j;

               for (i = 0; i < no_of_hosts; i++)
               {
                  if (my_strcmp(host_alias, fsa[i].host_alias) == 0)
                  {
                     for (j = 0; j < search_host_id_counter; j++)
                     {
                        if (fsa[i].host_id == search_host_id[j])
                        {
                           return(SUCCESS);
                        }
                     }
                  }
               }
            }
         }

         if (search_host_name_counter != 0)
         {
            if (fsa_fd == -1)
            {
               if (fsa_attach_passive(NO, ALDA_CMD) != SUCCESS)
               {
                  (void)fprintf(stderr, "Failed to attach to FSA. (%s %d)\n",
                                __FILE__, __LINE__);
               }
            }
            if (fsa_fd != -1)
            {
               if ((i = get_real_hostname(host_alias, current_toggle,
                                          real_hostname)) != INCORRECT)
               {
                  int j;

                  for (j = 0; j < search_host_name_counter; j++)
                  {
                     if (fsa[i].real_hostname[0][0] != GROUP_IDENTIFIER)
                     {
                        if (current_toggle == -1)
                        {
                           if ((ret = pmatch(search_host_name[j],
                                             fsa[i].real_hostname[0],
                                             NULL)) == 0)
                           {
                              return(SUCCESS);
                           }
                           else if (ret == 1)
                                {
                                   /*
                                    * This alias is definitly not wanted,
                                    * so no need to check the other filters.
                                    */
                                   j = search_host_alias_counter;
                                }
                           else if (fsa[i].host_toggle_str[0] != '\0')
                                {
                                   if ((ret = pmatch(search_host_name[j],
                                                     fsa[i].real_hostname[1],
                                                     NULL)) == 0)
                                   {
                                      return(SUCCESS);
                                   }
                                   else if (ret == 1)
                                        {
                                           /*
                                            * This alias is definitly not wanted,
                                            * so no need to check the other filters.
                                            */
                                           j = search_host_alias_counter;
                                        }
                                }
                        }
                        else
                        {
                           if ((ret = pmatch(search_host_name[j],
                                             fsa[i].real_hostname[current_toggle],
                                             NULL)) == 0)
                           {
                              return(SUCCESS);
                           }
                           else if (ret == 1)
                                {
                                   /*
                                    * This alias is definitly not wanted,
                                    * so no need to check the other filters.
                                    */
                                   j = search_host_alias_counter;
                                }
                        }
                     }
                  }
               }
            }
         }
      }
#ifdef WITH_AFD_MON
      else
      {
         if (start_alias_counter != 0)
         {
            for (i = 0; i < start_alias_counter; i++)
            {
               if ((ret = pmatch(start_alias[i], host_alias, NULL)) == 0)
               {
                  return(SUCCESS);
               }
               else if (ret == 1)
                    {
                       /*
                        * This alias is definitly not wanted,
                        * so no need to check the other filters.
                        */
                       i = start_alias_counter;
                    }
            }
         }

         if (start_id_counter != 0)
         {
            int j;

            for (i = 0; i < no_of_hosts; i++)
            {
               if (my_strcmp(host_alias, ahl[i].host_alias) == 0)
               {
                  for (j = 0; j < start_id_counter; j++)
                  {
                     if (ahl[i].host_id == start_id[j])
                     {
                        return(SUCCESS);
                     }
                  }
               }
            }
         }

         if (start_name_counter != 0)
         {
            if ((i = get_real_hostname(host_alias, current_toggle,
                                       real_hostname)) != INCORRECT)
            {
               int j;

               for (j = 0; j < start_name_counter; j++)
               {
                  if (ahl[i].real_hostname[0][0] != GROUP_IDENTIFIER)
                  {
                     if (current_toggle == -1)
                     {
                        if ((ret = pmatch(start_name[j],
                                          ahl[i].real_hostname[0],
                                          NULL)) == 0)
                        {
                           return(SUCCESS);
                        }
                        else if (ret == 1)
                             {
                                /*
                                 * This alias is definitly not wanted,
                                 * so no need to check the other filters.
                                 */
                                j = search_host_alias_counter;
                             }
                        else if (fsa[i].host_toggle_str[0] != '\0')
                             {
                                if ((ret = pmatch(start_name[j],
                                                  ahl[i].real_hostname[1],
                                                  NULL)) == 0)
                                {
                                   return(SUCCESS);
                                }
                                else if (ret == 1)
                                     {
                                        /*
                                         * This alias is definitly not wanted,
                                         * so no need to check the other filters.
                                         */
                                        j = search_host_alias_counter;
                                     }
                             }
                     }
                     else
                     {
                        if ((ret = pmatch(start_name[j],
                                          ahl[i].real_hostname[current_toggle],
                                          NULL)) == 0)
                        {
                           return(SUCCESS);
                        }
                        else if (ret == 1)
                             {
                                /*
                                 * This alias is definitly not wanted,
                                 * so no need to check the other filters.
                                 */
                                j = start_name_counter;
                             }
                     }
                  }
               }
            }
         }
      }
#endif

      return(INCORRECT);
   }

   return(SUCCESS);
}
