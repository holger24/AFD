/*
 *  encode_base64.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   encode_base64 - 
 **
 ** SYNOPSIS
 **   int encode_base64(unsigned char *src,
 **                     int           src_length,
 **                     unsigned char *dst,
 **                     int           limit_line_length)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.10.1998 H.Kiehl Created
 **   28.12.2023 H.Kiehl Added parameter limit_line_length.
 **
 */
DESCR__E_M3

#include "smtpdefs.h"

extern int        line_length;
static const char base_64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


/*########################### encode_base64() ###########################*/
int
encode_base64(unsigned char *src,
              int           src_length,
              unsigned char *dst,
              int           limit_line_length)
{
   unsigned char *dst_ptr = dst,
                 *src_ptr = src;

   while (src_length > 2)
   {
      *dst_ptr = base_64[(int)(*src_ptr) >> 2];
      *(dst_ptr + 1) = base_64[((((int)(*src_ptr) & 0x3)) << 4) | (((int)(*(src_ptr + 1)) & 0xF0) >> 4)];
      *(dst_ptr + 2) = base_64[((((int)(*(src_ptr + 1))) & 0xF) << 2) | ((((int)(*(src_ptr + 2))) & 0xC0) >> 6)];
      *(dst_ptr + 3) = base_64[((int)(*(src_ptr + 2))) & 0x3F];
      src_ptr += 3;
      src_length -= 3;
      line_length += 4;
      if ((limit_line_length == YES) && (line_length > 71))
      {
         line_length = 0;
         *(dst_ptr + 4) = '\r';
         *(dst_ptr + 5) = '\n';
         dst_ptr += 6;
      }
      else
      {
         dst_ptr += 4;
      }
   }
   if (src_length == 2)
   {
      *dst_ptr = base_64[(int)(*src_ptr) >> 2];
      *(dst_ptr + 1) = base_64[((((int)(*src_ptr) & 0x3)) << 4) | (((int)(*(src_ptr + 1)) & 0xF0) >> 4)];
      *(dst_ptr + 2) = base_64[(((int)(*(src_ptr + 1))) & 0xF) << 2];
      *(dst_ptr + 3) = '=';
      dst_ptr += 4;
      line_length = 0;
   }
   else if (src_length == 1)
        {
           *dst_ptr = base_64[(int)(*src_ptr) >> 2];
           *(dst_ptr + 1) = base_64[(((int)(*src_ptr) & 0x3)) << 4];
           *(dst_ptr + 2) = '=';
           *(dst_ptr + 3) = '=';
           dst_ptr += 4;
           line_length = 0;
        }

   return(dst_ptr - dst);
}
