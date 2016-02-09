/*
 *  check_did.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 - 2011 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_did - checks if directory name, alias and/or id matches
 **
 ** SYNOPSIS
 **   int check_did(unsigned int)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS when directory matches, otherwise INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.11.2008 H.Kiehl Created
 **   06.12.2009 H.Kiehl Added support for remote AFD's.
 **
 */
DESCR__E_M1

#include <stdio.h>         /* fprintf()                                  */
#include "aldadefs.h"
#ifdef WITH_AFD_MON
# include "mondefs.h"
#endif


/* External global variables. */
extern int                        fra_fd,
                                  no_of_dirs;
extern unsigned int               
                                  mode,
                                  search_dir_alias_counter,
                                  *search_dir_id,
                                  search_dir_id_counter,
                                  search_dir_name_counter;
extern char                       **search_dir_alias,
                                  **search_dir_name;
extern struct dir_name_area       dna;
extern struct fileretrieve_status *fra;
#ifdef _INPUT_LOG
extern struct alda_idata          ilog;
#endif
#ifdef _DISTRIBUTION_LOG
extern struct alda_udata          ulog;
#endif
#ifdef WITH_AFD_MON
extern unsigned int               adl_entries;
extern struct afd_dir_list        *adl;
#endif


/*############################# check_did() #############################*/
int
check_did(unsigned int did)
{
   if ((search_dir_alias_counter != 0) || (search_dir_id_counter != 0) ||
       (search_dir_name_counter != 0))
   {
      int i,
          ret;

      for (i = 0; i < search_dir_id_counter; i++)
      {
         if (did == search_dir_id[i])
         {
            return(SUCCESS);
         }
      }

      if (mode & ALDA_LOCAL_MODE)
      {
         if (search_dir_alias_counter != 0)
         {
            if (fra_fd == -1)
            {
               if (fra_attach_passive() != SUCCESS)
               {
                  (void)fprintf(stderr, "Failed to attach to FRA. (%s %d)\n",
                                __FILE__, __LINE__);
               }
            }
            if (fra_fd != -1)
            {
               int j;

               for (i = 0; i < no_of_dirs; i++)
               {
                  if (fra[i].dir_id == did)
                  {
                     for (j = 0; j < search_dir_alias_counter; j++)
                     {
                        if ((ret = pmatch(search_dir_alias[j],
                                          fra[i].dir_alias, NULL)) == 0)
                        {
                           return(SUCCESS);
                        }
                        else if (ret == 1)
                             {
                                /*
                                 * This alias is definitly not wanted,
                                 * so no need to check the other filters.
                                 */
                                j = search_dir_alias_counter;
                             }
                     }
                  }
               }
            }
         }

#if defined (_INPUT_LOG) || defined (_DISTRIBUTION_LOG)
         if (search_dir_name_counter != 0)
         {
            int j, k;

            check_dna();
            for (i = 0; i < *dna.no_of_dir_names; i++)
            {
               if (dna.dnb[i].dir_id == did)
               {
                  for (j = 0; j < search_dir_name_counter; j++)
                  {
                     if ((ret = pmatch(search_dir_name[j],
                                       dna.dnb[i].dir_name, NULL)) == 0)
                     {
                        k = 0;
                        while (dna.dnb[i].dir_name[k] != '\0')
                        {
# ifdef _INPUT_LOG
                           ilog.full_source[k] = dna.dnb[i].dir_name[k];
# else
                           ulog.full_source[k] = dna.dnb[i].dir_name[k];
# endif
                           k++;
                        }
# ifdef _INPUT_LOG
                        ilog.full_source[k] = '\0';
                        ilog.full_source_length = k;
# else
                        ulog.full_source[k] = '\0';
                        ulog.full_source_length = k;
# endif

                        return(SUCCESS);
                     }
                     else if (ret == 1)
                          {
                             /*
                              * This name is definitly not wanted,
                              * so no need to check the other filters.
                              */
                             j = search_dir_name_counter;
                          }
                  }
               }
            }
         }
#endif
      }
#ifdef WITH_AFD_MON
      else
      {
         if ((search_dir_alias_counter != 0) && (adl != NULL))
         {
            int j;

            for (i = 0; i < adl_entries; i++)
            {
               if (adl[i].dir_id == did)
               {
                  for (j = 0; j < search_dir_alias_counter; j++)
                  {
                     if ((ret = pmatch(search_dir_alias[j],
                                       adl[i].dir_alias, NULL)) == 0)
                     {
                        return(SUCCESS);
                     }
                     else if (ret == 1)
                          {
                             /*
                              * This alias is definitly not wanted,
                              * so no need to check the other filters.
                              */
                             j = search_dir_alias_counter;
                          }
                  }
               }
            }
         }

# if defined (_INPUT_LOG) || defined (_DISTRIBUTION_LOG)
         if ((search_dir_name_counter != 0) && (adl != NULL))
         {
            int j, k;

            for (i = 0; i < adl_entries; i++)
            {
               if (adl[i].dir_id == did)
               {
                  for (j = 0; j < search_dir_name_counter; j++)
                  {
                     if ((ret = pmatch(search_dir_name[j],
                                       adl[i].dir_name, NULL)) == 0)
                     {
                        k = 0;
                        while (adl[i].dir_name[k] != '\0')
                        {
#  ifdef _INPUT_LOG
                           ilog.full_source[k] = adl[i].dir_name[k];
#  else
                           ulog.full_source[k] = adl[i].dir_name[k];
#  endif
                           k++;
                        }
#  ifdef _INPUT_LOG
                        ilog.full_source[k] = '\0';
                        ilog.full_source_length = k;
#  else
                        ulog.full_source[k] = '\0';
                        ulog.full_source_length = k;
#  endif

                        return(SUCCESS);
                     }
                     else if (ret == 1)
                          {
                             /*
                              * This name is definitly not wanted,
                              * so no need to check the other filters.
                              */
                             j = search_dir_name_counter;
                          }
                  }
               }
            }
         }
# endif
      }
#endif

      return(INCORRECT);
   }

   return(SUCCESS);
}
