/*
 *  get_position.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2010 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_position - gets the position of the host or directory
 **                  in the FSA or FRA
 **
 ** SYNOPSIS
 **   int get_dir_position(struct fileretrieve_status *fra,
 **                        char                       *dir_alias,
 **                        int                        no_of_dirs)
 **   int get_host_position(struct filetransfer_status *fsa,
 **                         char                       *host_alias,
 **                         int                        no_of_hosts)
 **
 ** DESCRIPTION
 **   This function get_host_position() gets the position of host
 **   'host_alias' in the FSA (File Transfer Status Area) 'fsa'.
 **   The FSA consists of 'no_of_hosts' structures, i.e. for each
 **   host a structure of filetransfer_status exists.
 **
 **   Function get_dir_position() is very similar to the above
 **   function, only that it gets the 'dir_alias' from the FRA
 **   (File Retrieve Area).
 **
 ** RETURN VALUES
 **   Returns the position of the host 'host' in the FSA pointed
 **   to by 'fsa'. If the host is not found in the FSA, INCORRECT
 **   is returned instead.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   03.11.1995 H.Kiehl Created
 **   31.03.2000 H.Kiehl Added function get_dir_position().
 **   26.06.2008 H.Kiehl Added function get_host_id_position().
 **   28.01.2010 H.Kiehl Added function get_dir_id_position().
 **
 */
DESCR__E_M3

#include <string.h>                 /* strcmp()                          */


/*######################## get_host_position() ##########################*/
int
get_host_position(struct filetransfer_status *fsa,
                  char                       *host_alias,
                  int                        no_of_hosts)
{
   int position;

   for (position = 0; position < no_of_hosts; position++)
   {
      if (CHECK_STRCMP(fsa[position].host_alias, host_alias) == 0)
      {
         return(position);
      }
   }

   /* Host not found in struct. */
   return(INCORRECT);
}


/*###################### get_host_id_position() #########################*/
int
get_host_id_position(struct filetransfer_status *fsa,
                     unsigned int               host_id,
                     int                        no_of_hosts)
{
   int position;

   for (position = 0; position < no_of_hosts; position++)
   {
      if (fsa[position].host_id == host_id)
      {
         return(position);
      }
   }

   /* Host not found in struct. */
   return(INCORRECT);
}


/*######################### get_dir_position() ##########################*/
int
get_dir_position(struct fileretrieve_status *fra,
                 char                       *dir_alias,
                 int                        no_of_dirs)
{
   int position;

   for (position = 0; position < no_of_dirs; position++)
   {
      if (CHECK_STRCMP(fra[position].dir_alias, dir_alias) == 0)
      {
         return(position);
      }
   }

   /* Directory not found in struct. */
   return(INCORRECT);
}


/*####################### get_dir_id_position() #########################*/
int
get_dir_id_position(struct fileretrieve_status *fra,
                     unsigned int               dir_id,
                     int                        no_of_dirs)
{
   int position;

   for (position = 0; position < no_of_dirs; position++)
   {
      if (fra[position].dir_id == dir_id)
      {
         return(position);
      }
   }

   /* Directory ID not found in struct. */
   return(INCORRECT);
}
