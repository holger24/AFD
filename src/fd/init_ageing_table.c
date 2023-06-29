/*
 *  init_ageing_table.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   init_ageing_table - initializes ageing table structure with some
 **                       default values.
 **
 ** SYNOPSIS
 **   void init_ageing_table(void)
 **
 ** DESCRIPTION
 **   Files ageing table structure with some default values.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.06.2023 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "fddefs.h"

/* External global variables. */
extern struct ageing_table at[];


/*$$$$$$$$$$$$$$$$$$$$$$$$$ init_ageing_table() $$$$$$$$$$$$$$$$$$$$$$$$$*/
void
init_ageing_table(void)
{
   /* Ageing Level 0 (no ageing). */
   at[0].before_threshold = 0.0;
   at[0].retry_threshold  = 0;
   at[0].after_threshold  = 0.0;

   /* Ageing Level 1. */
   at[1].before_threshold = 6000000.0;
   at[1].retry_threshold  = 24;
   at[1].after_threshold  = 313.0;

   /* Ageing Level 2. */
   at[2].before_threshold = 9000000.0;
   at[2].retry_threshold  = 16;
   at[2].after_threshold  = 625.0;

   /* Ageing Level 3. */
   at[3].before_threshold = 12000000.0;
   at[3].retry_threshold  = 12;
   at[3].after_threshold  = 1250.0;

   /* Ageing Level 4. */
   at[4].before_threshold = 15000000.0;
   at[4].retry_threshold  = 9;
   at[4].after_threshold  = 2500.0;

   /* Ageing Level 5. */
   at[5].before_threshold = 30000000.0;
   at[5].retry_threshold  = 6;
   at[5].after_threshold  = 5000.0;

   /* Ageing Level 6 (default). */
   at[6].before_threshold = 60000000.0;
   at[6].retry_threshold  = 4;
   at[6].after_threshold  = 10000.0;

   /* Ageing Level 7. */
   at[7].before_threshold = 120000000.0;
   at[7].retry_threshold  = 3;
   at[7].after_threshold  = 20000.0;

   /* Ageing Level 8. */
   at[8].before_threshold = 240000000.0;
   at[8].retry_threshold  = 2;
   at[8].after_threshold  = 40000.0;

   /* Ageing Level 9 (maximum). */
   at[9].before_threshold = 480000000.0; /* Will never be used. */
   at[9].retry_threshold  = 1;
   at[9].after_threshold  = 80000.0;

   return;
}
