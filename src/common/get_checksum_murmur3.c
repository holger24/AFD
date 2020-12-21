/*
 *  get_checksum_murmur3.c - Part of AFD, an automatic file distribution
 *                           program.
 *  Copyright (c) 2020 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_checksum - gets a checksum for the given data
 **
 ** SYNOPSIS
 **   unsigned int get_checksum_murmur3(const unsigned int  icrc,
 **                                     const unsigned char *mem,
 **                                     size_t              size)
 **   unsigned int get_str_checksum_murmur3(char *str)
 **   int          get_file_checksum_murmur3(int          fd,
 **                                          char         *buffer,
 **                                          size_t       buf_size,
 **                                          size_t       offset,
 **                                          unsigned int *crc)
 **
 ** DESCRIPTION
 **   Gets the murmurhash (https://en.wikipedia.org/wiki/MurmurHash)
 **   used by many utils. This is taken from
 **   https://enqueuezero.com/algorithms/murmur-hash.html
 **
 ** RETURN VALUES
 **   The function get_checksum() returns the CRC while
 **   get_file_checksum() returns SUCCESS and the CRC in 'crc'
 **   when successful otherwise it returns INCORRECT.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   15.12.2020 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>              /* NULL                                  */
#include <string.h>             /* strerror()                            */
#include <unistd.h>
#include <errno.h>


/*####################### get_checksum_murmur3() ########################*/
unsigned int
get_checksum_murmur3(const unsigned int  icrc,
                     const unsigned char *mem,
                     size_t              size)
{
   unsigned int crc = icrc;

   if (size > 3)
   {
      const unsigned int *mem_x4 = (const unsigned int *)mem;
      size_t             i = size >> 2;

      do
      {
         unsigned int k = *mem_x4++;

         k *= 0xcc9e2d51;
         k = (k << 15) | (k >> 17);
         k *= 0x1b873593;
         crc ^= k;
         crc = (crc << 13) | (crc >> 19);
         crc = (crc * 5) + 0xe6546b64;
      } while (--i);
      mem = (const unsigned char *)mem_x4;
   }

   if (size & 3)
   {
      size_t       i = size & 3;
      unsigned int k = 0;

      mem = &mem[i - 1];
      do
      {
         k <<= 8;
         k |= *mem--;
      } while (--i);
      k *= 0xcc9e2d51;
      k = (k << 15) | (k >> 17);
      k *= 0x1b873593;
      crc ^= k;
   }
   crc ^= size;
   crc ^= crc >> 16;
   crc *= 0x85ebca6b;
   crc ^= crc >> 13;
   crc *= 0xc2b2ae35;
   crc ^= crc >> 16;

   return(crc);
}


/*##################### get_str_checksum_murmur3() ######################*/
unsigned int
get_str_checksum_murmur3(char *str)
{
   return(get_checksum_murmur3(INITIAL_CRC, (unsigned char *)str, strlen(str)));
}


/*#################### get_file_checksum_murmur3() ######################*/
int
get_file_checksum_murmur3(int          fd,
                          char         *buffer,
                          size_t       buf_size,
                          size_t       offset,
                          unsigned int *crc)
{
   char   *ptr;
   size_t bytes_read;

   *crc = INITIAL_CRC;
   ptr = buffer;
   if ((bytes_read = read(fd, (ptr + offset), (buf_size - offset))) >= 0)
   {
      bytes_read += offset;
      *crc = get_checksum_murmur3(*crc, (unsigned char *)ptr, bytes_read);
   }
   else if (bytes_read == -1)
        {
           system_log(ERROR_SIGN, __FILE__, __LINE__,
                      _("read() error : %s"), strerror(errno));
           return(INCORRECT);
        }

   if (bytes_read == buf_size)
   {
      do
      {
         ptr = buffer;
         if ((bytes_read = read(fd, ptr, buf_size)) > 0)
         {
            *crc = get_checksum_murmur3(*crc, (unsigned char *)ptr, bytes_read);
         }
         else if (bytes_read == -1)
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            _("read() error : %s"), strerror(errno));
                 return(INCORRECT);
              }
      } while (bytes_read == buf_size);
   }

   return(SUCCESS);
}
