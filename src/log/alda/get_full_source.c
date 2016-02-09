/*
 *  get_full_source.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_full_source - gets full source directory
 **
 ** SYNOPSIS
 **   void get_full_source(unsigned int dir_id,
 **                        char         *full_source,
 **                        int          *full_source_length)
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
 **   22.11.2008 H.Kiehl Created
 **   06.12.2009 H.Kiehl Added support for remote AFD's.
 **
 */
DESCR__E_M1

#include "aldadefs.h"
#ifdef WITH_AFD_MON
# include "mondefs.h"
#endif


/* External global variables. */
extern int                  mode;
extern struct dir_name_area dna;
#ifdef WITH_AFD_MON
extern unsigned int         adl_entries;
extern struct afd_dir_list  *adl;
#endif


/*++++++++++++++++++++++++++ get_full_source() ++++++++++++++++++++++++++*/
void                                                                     
get_full_source(unsigned int dir_id, char *full_source, int *full_source_length)
{
   if (mode & ALDA_LOCAL_MODE)
   {
      int i, k;

      check_dna();

      for (i = 0; i < *dna.no_of_dir_names; i++)
      {
         if (dna.dnb[i].dir_id == dir_id)
         {
            k = 0;
            while (dna.dnb[i].orig_dir_name[k] != '\0')
            {
               full_source[k] = dna.dnb[i].orig_dir_name[k];
               k++;
            }
            full_source[k] = '\0';
            *full_source_length = k;

            return;
         }
      }
   }
#ifdef WITH_AFD_MON
   else
   {
      if (adl != NULL)
      {
         int i, k;

         for (i = 0; i < adl_entries; i++)
         {
            if (adl[i].dir_id == dir_id)
            {
               k = 0;
               while (adl[i].orig_dir_name[k] != '\0')
               {
                  full_source[k] = adl[i].orig_dir_name[k];
                  k++;
               }
               full_source[k] = '\0';
               *full_source_length = k;

               return;
            }
         }
      }
   }
#endif

   return;
}
