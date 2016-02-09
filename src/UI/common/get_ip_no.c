/*
 *  get_ip_no.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_ip_no - gets the internet address of a given host
 **
 ** SYNOPSIS
 **   void get_ip_no(char *host_name, char *p_dest)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   When an error is encountered in gethostbyname() this function
 **   will returns 'Unknown'. Otherwise it will write the IP-address
 *    to 'p_dest'.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.11.1996 H.Kiehl Created
 **   05.12.2000 H.Kiehl Check if hostname does contain a value.
 **   19.08.2012 H.Kiehl Handle IPv6.
 **
 */
DESCR__E_M3

#include <stdio.h>      /* NULL                                          */
#include <string.h>     /* strcpy(), strerror()                          */
#include <ctype.h>      /* isdigit()                                     */
#include <sys/types.h>
#include <netdb.h>      /* struct hostent                                */
#include <sys/socket.h> /* AF_INET                                       */
#include <netinet/in.h> /* struct in_addr                                */
#include <arpa/inet.h>  /* inet_ntoa()                                   */
#include <errno.h>
#include "ui_common_defs.h"


/*############################## get_ip_no() ############################*/
void
get_ip_no(char *host_name, char *p_dest)
{
   if (host_name[0] == '\0')
   {
      *p_dest = '\0';
   }
   else
   {
#if defined (HAVE_GETADDRINFO) && defined (HAVE_GAI_STRERROR)
      int             reply;
      struct addrinfo hints,
                      *result;

      (void)memset(&hints, 0, sizeof(hints));
      hints.ai_family = AF_UNSPEC;
      hints.ai_socktype = SOCK_STREAM;

      reply = getaddrinfo(host_name, NULL, &hints, &result);
      if (reply != 0)
      {
         char *ptr = host_name;

         do
         {
            if (!((isxdigit((int)(*ptr))) || (*ptr == '.') || (*ptr == ':')))
            {
               (void)strcpy(p_dest, "Unknown");
               return;
            }
            ptr++;
         } while (*ptr != '\0');
         (void)strcpy(p_dest, host_name);
      }
      else
      {
         if (result->ai_family == AF_INET)
         {
            struct sockaddr_in *sa = (struct sockaddr_in *)result->ai_addr;

            (void)strcpy(p_dest, inet_ntoa(sa->sin_addr));
         }
         else if (result->ai_family == AF_INET6)
              {
                 struct sockaddr_in6 *sa = (struct sockaddr_in6 *)result->ai_addr;

                 if (inet_ntop(result->ai_family, &(sa->sin6_addr),
                               p_dest, MAX_INFO_STRING_LENGTH) == NULL)
                 {
                    (void)fprintf(stderr, "Cannot get address of %s : %s\n",
                                  host_name, strerror(errno));
                 }
              }
              else
              {
                 (void)strcpy(p_dest, "Unknown address type");
              }
      }
      freeaddrinfo(result);
#else
      struct hostent *p_host;

      if ((p_host = gethostbyname(host_name)) == NULL)
      {
         char *ptr = host_name;

         do
         {
            if (!((isdigit((int)(*ptr))) || (*ptr == '.')))
            {
               (void)strcpy(p_dest, "Unknown");
               return;
            }
            ptr++;
         } while (*ptr != '\0');
         (void)strcpy(p_dest, host_name);
      }
      else
      {
         if (p_host->h_addrtype == AF_INET)
         {
            (void)strcpy(p_dest, inet_ntoa(*(struct in_addr *)p_host->h_addr));
         }
         else
         {
            (void)strcpy(p_dest, "Unknown address type");
         }
      }
#endif
   }

   return;
}
