/*
 *  show_fifo_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   show_fifo_data - shows all data send/received via a fifo
 **
 ** SYNOPSIS
 **   void show_fifo_data(char typ,          R - read, W - write
 **                       char *fifo,        name of fifo
 **                       char *data,        fifo data buffer
 **                       int  data_length,  fifo data size
 **                       char *filename,    source file
 **                       int  position)     position in source file
 **
 ** DESCRIPTION
 **   The function show_fifo_data() prints all caracters in 'data'
 **   to stdout. If there are non printable characters these will
 **   be displayed by <decimal value>. It will always start a new
 **   line when reaching the right hand border, see the example
 **   below:
 **     W  write_fsa >    sf_tcp.c  565< : automati0<17>  <12><2>
 **                                        <5>1635017059<12><6>3
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   26.12.1995 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                       /* fprintf()                    */
#include <ctype.h>                       /* iscntrl()                    */


#ifdef _FIFO_DEBUG
/*########################## show_fifo_data() ###########################*/
void
show_fifo_data(char typ,           /* R - read, W - write     */
               char *fifo,         /* name of fifo            */
               char *data,         /* fifo data buffer        */
               int  data_length,   /* fifo data size          */
               char *filename,     /* source file             */
               int  position)      /* position in source file */
{
   int i,
       printed_chars = 0;

   (void)fprintf(stdout, "%c %10.10s >%12.12s %4d< : ",
                 typ, fifo, filename, position);

   /* Now show contents of fifo. */
   for (i = 0; i < data_length; i++)
   {
      /* Check if we reached EOL. */
      if (((printed_chars / 43) > 0) && (printed_chars != 0))
      {
         (void)fprintf(stdout, "\n%35s", " ");
         printed_chars = 0;
      }

      /* Show character after character. */
      if (iscntrl(data[i]) == 0)
      {
         printed_chars += fprintf(stdout, "%c", data[i]);
      }
      else
      {
         printed_chars += fprintf(stdout, "<%d>", data[i]);
      }
   }
   (void)fprintf(stdout, "\n");

   return;
}
#endif
