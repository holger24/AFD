/*
 *  get_alias_name.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2017 Deutscher Wetterdienst (DWD),
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

DESCR__S_M3
/*
 ** NAME
 **   get_alias_name      - get alias name for a certain job ID.
 **
 ** SYNOPSIS
 **   void get_alias_name(unsigned int job_id, char *alias_name,
 **                       int *alias_name_length)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   09.11.2017 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>
#ifdef WITH_AFD_MON
# include "mondefs.h"
#endif
#include "aldadefs.h"

/* External global variables. */
extern int               max_hostname_length;
extern struct alda_odata olog;
extern struct jid_data   jidd;


/*########################### get_alias_name() ##########################*/
void
get_alias_name(unsigned int job_id, char *alias_name, int *alias_name_length)
{
#ifdef WITH_AFD_MON
   if (jidd.jd != NULL)
   {
#endif
      if ((jidd.prev_pos != -1) && (job_id == jidd.jd[jidd.prev_pos].job_id))
      {
         (void)strcpy(alias_name, jidd.jd[jidd.prev_pos].host_alias);
         *alias_name_length = (int)strlen(alias_name);
         return;
      }
      else
      {
         int i;

         for (i = 0; i < jidd.no_of_job_ids; i++)
         {
            if (job_id == jidd.jd[i].job_id)
            {
               (void)strcpy(alias_name, jidd.jd[i].host_alias);
               *alias_name_length = (int)strlen(alias_name);
               jidd.prev_pos = i;
               return;
            }
         }
      }
#ifdef WITH_AFD_MON
   }
   else if (jidd.ajl != NULL)
        {
           if ((jidd.prev_pos != -1) &&
               (job_id == jidd.ajl[jidd.prev_pos].job_id))
           {
              if (url_evaluate(jidd.ajl[jidd.prev_pos].recipient, NULL, NULL,
                               NULL, NULL,
# ifdef WITH_SSH_FINGERPRINT
                               NULL, NULL,
# endif
                               NULL, NO, alias_name, NULL, NULL, NULL,
                               NULL, NULL, NULL, NULL) < 4)
              {
                 register int j = 0;

                 while ((alias_name[j] != '\0') &&
                        (alias_name[j] != '\n') &&
                        (alias_name[j] != ':') &&
                        (alias_name[j] != '.') &&
                        (j != max_hostname_length))
                 {
                    j++;
                 }
                 alias_name[j] = '\0';
                 *alias_name_length = j;
              }

              return;
           }
           else
           {
              int i;

              for (i = 0; i < jidd.no_of_job_ids; i++)
              {
                 if (job_id == jidd.ajl[i].job_id)
                 {
                    if (url_evaluate(jidd.ajl[jidd.prev_pos].recipient, NULL,
                                     NULL, NULL, NULL,
# ifdef WITH_SSH_FINGERPRINT
                                     NULL, NULL,
# endif
                                     NULL, NO, alias_name, NULL, NULL,
                                     NULL, NULL, NULL, NULL, NULL) < 4)
                    {
                       register int j = 0;

                       while ((alias_name[j] != '\0') &&
                              (alias_name[j] != '\n') &&
                              (alias_name[j] != ':') &&
                              (alias_name[j] != '.') &&
                              (j != max_hostname_length))
                       {
                          j++;
                       }
                       alias_name[j] = '\0';
                       *alias_name_length = j;
                    }
                    jidd.prev_pos = i;

                    return;
                 }
              }
           }   
        }
#endif
   alias_name[0] = '\0';
   jidd.prev_pos = -1;

   return;
}
