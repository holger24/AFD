/*
 *  rm_dupcheck_crc.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2022 - 2024 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   rm_dupcheck_crc - removes dupcheck CRC for given filename
 **
 ** SYNOPSIS
 **   void rm_dupcheck_crc(char *fullname, char *file_name, off_t file_size)
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
 **   23.11.2022 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "fddefs.h"

/* External global variables. */
#ifdef HAVE_HW_CRC32
extern int        have_hw_crc32;
#endif
extern struct job db;

/*########################## rm_dupcheck_crc() ##########################*/
void
rm_dupcheck_crc(char *fullname, char *file_name, off_t file_size)
{
#ifdef WITH_DUP_CHECK
   if (db.dup_check_timeout > 0)
   {
      /* Remove the dupcheck CRC value. */
      (void)isdup(fullname, file_name, file_size, db.crc_id,
                  db.dup_check_timeout, db.dup_check_flag, YES,
# ifdef HAVE_HW_CRC32
                  have_hw_crc32,
# endif
                  YES, NO);
   }
#endif

   return;
}
