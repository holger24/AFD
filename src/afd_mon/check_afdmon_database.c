/*
 *  check_afdmon_database.c - Part of AFD, an automatic file distribution
 *                            program.
 *  Copyright (c) 2021 Deutscher Wetterdienst (DWD),
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
 **   check_afdmon_database - checks if afdmon database is ok
 **
 ** SYNOPSIS
 **   int check_afdmon_database(void)
 **
 ** DESCRIPTION
 **   The function check_afdmon_database() checks if there is a readable
 **   AFD_MON_CONFIG file.
 **
 ** RETURN VALUES
 **   On success 0 is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.02.2021 H.Kiehl Created from mafd.c.
 **
 */
DESCR__E_M1

#include <stdio.h>               /* sprintf()                            */
#include <unistd.h>              /* eaccess()                            */
#include "mondefs.h"

/* External global variables. */
extern char *p_work_dir;


/*######################## check_afdmon_database() ######################*/
int
check_afdmon_database(void)
{
   char db_file[MAX_PATH_LENGTH];

   (void)sprintf(db_file, "%s%s%s",
                 p_work_dir, ETC_DIR, AFD_MON_CONFIG_FILE);

   return(eaccess(db_file, R_OK));
}
