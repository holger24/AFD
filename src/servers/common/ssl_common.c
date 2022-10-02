/*
 *  ssl_common.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   ssl_common - TLS functions that can be used for programs
 **
 ** SYNOPSIS
 **   ssize_t ssl_write(SSL *ssl, const char *buf, size_t count)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.04.2022 H.Kiehl Created
 */
DESCR__E_M3

#include <string.h>       /* strerror()                                 */
#include <openssl/ssl.h>
#include <errno.h>
#include "server_common_defs.h"


/*############################## command() ##############################*/
int
command(SSL *ssl, char *fmt, ...)
{
   int     length;
   char    buf[MAX_LINE_LENGTH + 1];
   va_list ap;

   va_start(ap, fmt);
   length = vsnprintf(buf, MAX_LINE_LENGTH, fmt, ap);
   va_end(ap);
   if (length > MAX_LINE_LENGTH)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "command(): Command to long (%d > %d)",
                 length, MAX_LINE_LENGTH);
      return(INCORRECT);
   }
   buf[length] = '\r';
   buf[length + 1] = '\n';
   length += 2;

   if (ssl_write(ssl, buf, length) != length)
   {
      return(INCORRECT);
   }

   return(SUCCESS);
}


/*############################# ssl_write() #############################*/
ssize_t
ssl_write(SSL *ssl, const char *buf, size_t count)
{
   int     bytes_done;
   ssize_t bytes_total = 0;

   do
   {
      if ((bytes_done = SSL_write(ssl, buf + bytes_total, count)) <= 0)
      {
         int ret;

         ret = SSL_get_error(ssl, bytes_done);
         switch (ret)
         {
            case SSL_ERROR_WANT_READ : /* Renegotiation takes place. */
               my_usleep(50000L);
               break;

            case SSL_ERROR_SYSCALL :
               system_log(((errno == ECONNRESET) || (errno == EPIPE)) ? INFO_SIGN : WARN_SIGN, __FILE__, __LINE__,
                         _("ssl_write(): SSL_write() error (%d) : %s"),
                         ret, strerror(errno));
               return(INCORRECT);

            default : /* Error */
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                         _("ssl_write(): SSL_write() error (%d)"), ret);
               return(INCORRECT);
         }
      }
      else
      {
         count -= bytes_done;
         bytes_total += bytes_done;
      }
   } while (count > 0);

   return(bytes_total);
}
