/*
 *  handle_xor_crypt.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2014 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   handle_xor_crypt - handles all XOR encryption and decryption
 **
 ** SYNOPSIS
 **   int  open_counter_file(char *file_name, int **counter)
 **   int  next_counter(int counter_fd, int *counter, int max_counter)
 **   void next_counter_no_lock(int *counter, int max_counter)
 **   void close_counter_file(int counter_fd, int **counter)
 **
 ** DESCRIPTION
 **   This function open_counter_file() opens the given counter file
 **   and maps to it. This counter can be used as a unique counter.
 **
 **   The function next_counter() reads and returns the current counter.
 **   It then increments the counter by one and writes this value back
 **   to the counter file. When the value is larger then max_counter
 **   it resets the counter to zero.
 **
 ** RETURN VALUES
 **   open_counter_file() returns the file descriptor of the open file
 **   or INCORRECT when it fails to do so.
 **
 **   Function next_counter() returns the current value 'counter' in the
 **   AFD counter file and SUCCESS. Else INCORRECT will be returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   24.10.2014 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>           /* strerror()                              */
#include <stdlib.h>           /* malloc()                                */
#include <unistd.h>           /* read()                                  */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>


/* Local global variables. */
#ifdef XOR_KEY
static int                 xor_key_length = sizeof(XOR_KEY) - 1;
static char                xor_key[] = XOR_KEY;
#else
static int                 xor_key_length;
static char                *xor_key = NULL;
#endif
static const char          base_64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const unsigned char dtable[] = {
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
      0x80, 0x80, 0x80, 0x3e, 0x80, 0x80, 0x80, 0x3f, 0x34, 0x35,
      0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x80, 0x80,
      0x80, 0x00, 0x80, 0x80, 0x80, 0x00, 0x01, 0x02, 0x03, 0x04,
      0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
      0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
      0x19, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x1a, 0x1b, 0x1c,
      0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
      0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
      0x31, 0x32, 0x33, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80
   };

/* External global variables. */
extern char *p_work_dir;

/* Local function prototypes. */
static int  decode_base64(const unsigned char *, const int, unsigned char *),
            encode_base64(const unsigned char *, const int, unsigned char *);
#ifndef XOR_KEY
static int  init_xor_key(void);
#endif
static void xor_encrypt_decrypt(char *, int);


/*############################ xor_encrypt() ############################*/
int
xor_encrypt(unsigned char *string, int length, unsigned char *dst)
{
   int           ret_length;
   unsigned char *buffer;

#ifndef XOR_KEY
   if (xor_key == NULL)
   {
      if (init_xor_key() == INCORRECT)
      {
         *dst = '\0';
         return(INCORRECT);
      }
   }
#endif
   if ((buffer = malloc(length + 1)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() %d bytes : %s",
                 length + 1, strerror(errno));
      ret_length = INCORRECT;
      *dst = '\0';
   }
   else
   {
      (void)my_strncpy((char *)buffer, (char *)string, length + 1);
      xor_encrypt_decrypt((char *)buffer, length);
      ret_length = encode_base64(buffer, length, dst);
      free(buffer);
   }

   return(ret_length);
}


/*############################ xor_decrypt() ############################*/
int
xor_decrypt(unsigned char *string, int length, unsigned char *dst)
{
   int           ret_length;
   unsigned char *buffer;

#ifndef XOR_KEY
   if (xor_key == NULL)
   {
      if (init_xor_key() == INCORRECT)
      {
         *dst = '\0';
         return(INCORRECT);
      }
   }
#endif
   if ((buffer = malloc(length + 1)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() %d bytes : %s",
                 length + 1, strerror(errno));
      ret_length = INCORRECT;
      *dst = '\0';
   }
   else
   {
      (void)my_strncpy((char *)buffer, (char *)string, length + 1);
      if ((ret_length = decode_base64(buffer, length, dst)) == INCORRECT)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to decode BASE64 string.");
         *dst = '\0';
      }
      else
      {
         xor_encrypt_decrypt((char *)dst, ret_length);
      }
      free(buffer);
   }

   return(SUCCESS);
}


#ifndef XOR_KEY
/*+++++++++++++++++++++++++++ init_xor_key() ++++++++++++++++++++++++++++*/
static int
init_xor_key(void)
{
   int  ret;
   char key_file[MAX_PATH_LENGTH];

   if (snprintf(key_file, MAX_PATH_LENGTH, "%s%s%s",
                p_work_dir, ETC_DIR, XOR_KEY_FILENAME) > MAX_PATH_LENGTH)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Buffer to short for storing key name file.");
      ret = INCORRECT;
   }
   else
   {
      int fd;

      if ((fd = coe_open(key_file, O_RDONLY)) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to open() %s : %s", key_file, strerror(errno));
         ret = INCORRECT;
      }
      else
      {
# ifdef HAVE_STATX
         struct statx stat_buf;
# else
         struct stat stat_buf;
# endif

# ifdef HAVE_STATX
         if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                   STATX_SIZE, &stat_buf) == -1)
# else
         if (fstat(fd, &stat_buf) == -1)
# endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
# ifdef HAVE_STATX
                       "Failed to statx() %s : %s",
# else
                       "Failed to fstat() %s : %s",
# endif
                       key_file, strerror(errno));
            ret = INCORRECT;
         }
         else
         {
# ifdef HAVE_STATX
            if (stat_buf.stx_size == 0)
# else
            if (stat_buf.st_size == 0)
# endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "File %s is empty", key_file);
               ret = INCORRECT;
            }
            else
            {
# ifdef HAVE_STATX
               if ((xor_key = malloc(stat_buf.stx_size + 1)) == NULL)
# else
               if ((xor_key = malloc(stat_buf.st_size + 1)) == NULL)
# endif
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "malloc() error : %s", strerror(errno));
                  ret = INCORRECT;
               }
               else
               {
# ifdef HAVE_STATX
                  if (read(fd, xor_key, stat_buf.stx_size) != stat_buf.stx_size)
# else
                  if (read(fd, xor_key, stat_buf.st_size) != stat_buf.st_size)
# endif
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "read() error : %s", strerror(errno));
                     ret = INCORRECT;
                  }
                  else
                  {
# ifdef HAVE_STATX
                     if (xor_key[stat_buf.stx_size - 1] == 10)
                     {
                        if (xor_key[stat_buf.stx_size - 2] == 13)
                        {
                           xor_key[stat_buf.stx_size - 2] = '\0';
                           xor_key_length = stat_buf.stx_size - 3;
                        }
                        else
                        {
                           xor_key[stat_buf.stx_size - 1] = '\0';
                           xor_key_length = stat_buf.stx_size - 2;
                        }
                     }
                     else
                     {
                        xor_key[stat_buf.stx_size] = '\0';
                        xor_key_length = stat_buf.stx_size - 1;
                     }
# else
                     if (xor_key[stat_buf.st_size - 1] == 10)
                     {
                        if (xor_key[stat_buf.st_size - 2] == 13)
                        {
                           xor_key[stat_buf.st_size - 2] = '\0';
                           xor_key_length = stat_buf.st_size - 3;
                        }
                        else
                        {
                           xor_key[stat_buf.st_size - 1] = '\0';
                           xor_key_length = stat_buf.st_size - 2;
                        }
                     }
                     else
                     {
                        xor_key[stat_buf.st_size] = '\0';
                        xor_key_length = stat_buf.st_size - 1;
                     }
# endif
                     ret = SUCCESS;
                  }
               }
            }
         }

         if (close(fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Failed to close() %s : %s",
                       key_file, strerror(errno));
         }
      }
   }

   return(ret);
}
#endif


/*++++++++++++++++++++++++ xor_encrypt_decrypt() ++++++++++++++++++++++++*/
static void
xor_encrypt_decrypt(char *string, int length)
{
   int i;

   for (i = 0; i < length; i++)
   {
      string[i] = string[i] ^ xor_key[i % xor_key_length];
   }

   return;
}


/*++++++++++++++++++++++++++++ encode_base64 ++++++++++++++++++++++++++++*/
static int
encode_base64(const unsigned char *src,
              const int           src_length,
              unsigned char       *dst)
{
   int           i = 0;
   unsigned char *dst_ptr = dst;

   while ((src_length - i) > 2)
   {
      *dst_ptr = base_64[(int)src[i] >> 2];
      *(dst_ptr + 1) = base_64[(((int)src[i] & 0x3) << 4) | (((int)src[i + 1] & 0xF0) >> 4)];
      *(dst_ptr + 2) = base_64[(((int)src[i + 1] & 0xF) << 2) | (((int)src[i + 2] & 0xC0) >> 6)];
      *(dst_ptr + 3) = base_64[(int)src[i + 2] & 0x3F];
      i += 3;
      dst_ptr += 4;
   }
   if ((src_length - i) == 2)
   {
      *dst_ptr = base_64[(int)src[i] >> 2];
      *(dst_ptr + 1) = base_64[(((int)src[i] & 0x3) << 4) | (((int)src[i + 1] & 0xF0) >> 4)];
      *(dst_ptr + 2) = base_64[((int)src[i + 1] & 0xF) << 2];
      *(dst_ptr + 3) = '=';
      dst_ptr += 4;
   }
   else if ((src_length - i) == 1)
        {
           *dst_ptr = base_64[(int)src[i] >> 2];
           *(dst_ptr + 1) = base_64[((int)src[i] & 0x3) << 4];
           *(dst_ptr + 2) = '=';
           *(dst_ptr + 3) = '=';
           dst_ptr += 4;
        }

   return(dst_ptr - dst);
}


/*++++++++++++++++++++++++++++ decode_base64 ++++++++++++++++++++++++++++*/
static int
decode_base64(const unsigned char *src,
              const int           src_length,
              unsigned char       *dst)
{
   unsigned char block[4],
                 *dst_ptr = dst;
   int           count = 0,
                 i,
                 pad = 0;

   for (i = 0; i < src_length; i++)
   {
      if (dtable[src[i]] != 0x80)
      {
         count++;
      }
   }

   if ((count == 0) || (count % 4))
   {
      *dst = '\0';
      return(INCORRECT);
   }

   count = 0;
   for (i = 0; i < src_length; i++)
   {
      if (dtable[src[i]] != 0x80)
      {
         if (src[i] == '=')
         {
            pad++;
         }
         block[count] = dtable[src[i]];
         count++;
         if (count == 4)
         {
            *dst_ptr = (block[0] << 2) | (block[1] >> 4);
            *(dst_ptr + 1) = (block[1] << 4) | (block[2] >> 2);
            *(dst_ptr + 2) = (block[2] << 6) | block[3];
            dst_ptr += 3;
            count = 0;
            if (pad > 0)
            {
               if (pad == 1)
               {
                  dst_ptr--;
               }
               else if (pad == 2)
                    {
                       dst_ptr -= 2;
                    }
                    else
                    {
                       /* Invalid padding. */
                       return(INCORRECT);
                    }
               break;
            }
         }
      }
   }
   *dst_ptr = '\0';

   return(dst_ptr - dst);
}
