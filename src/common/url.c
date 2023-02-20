/*
 *  url.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 - 2022 Deutscher Wetterdienst (DWD),
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
 **   url - set of functions to work with URL's
 **
 ** SYNOPSIS
 **   unsigned int url_evaluate(char          *url,
 **                             unsigned int  *scheme,
 **                             char          *user,
 **                             unsigned char *smtp_auth,
 **                             char          *smtp_user,
 **                             char          *fingerprint,
 **                             char          *key_type,
 **                             char          *password,
 **                             int           remove_passwd,
 **                             char          *hostname,
 **                             int           *port,
 **                             char          *path,
 **                             char          **p_path_start,
 **                             time_t        *time_val,
 **                             char          *transfer_type,
 **                             unsigned char *protocol_version,
 **                             unsigned char *auth,
 **                             char          *region,
 **                             unsigned char *service,
 **                             char          *server)
 **   void url_insert_password(char *url, char *password)
 **   int url_compare(char *url1, char *url2)
 **   void url_get_error(int error_mask, char *error_str, int error_str_length)
 **   void url_encode(char *src, char *dst)
 **   void url_path_encode(char *src, char *dst)
 **   void url_decode(char *src, char *dst)
 **
 ** DESCRIPTION
 **   The function url_evaluate() extracts individual elements of a
 **   URL and stores them in the given buffer if supplied. The url must
 **   have the following format:
 **
 **   <scheme>://[[<user>][;fingerprint=<SSH fingerprint>][;auth=<login|plain>;user=<user name>;][:<password>]@]<host>[:<port>][/<url-path>][;type=<i|a|d|n>][;server=<server name>][;protocol=<protocol number>][;auth=<basic|digest|aws4-hmac-sha256|aws-no-sign-request>][;region=<region name>][;service=s3]
 **
 **   Special characters may be masked with a \ or with a % sign plus two
 **   hexa digits representing the ASCII character. A plus behind the @
 **   part of the URL will be replaced by a space.
 **
 ** RETURN VALUES
 **   Function url_evaluate() will return an error mask which can
 **   be evaluated with url_get_error(). When successful this will
 **   be less then 4 and it will return the specified parameters.
 **   A return of 1 just indicates that the path may change.
 **
 **   url_insert_password() will insert the password and have no
 **   return value.
 **
 **   url_compare() returns a mask with values that are different.
 **
 **   url_get_error() returns the error message in error_str.
 **
 **   url_encode() returns encoded string in dst. Note the size
 **   of dst should always be 3 times + 1 the size of src.
 **
 **   url_path_encode() is similar to url_encode() and differs only
 **   in allowing the path sign /.
 **
 **   url_decode() returns decoded string in dst.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.04.2008 H.Kiehl Created
 **   17.11.2008 H.Kiehl Added SMTP AUTH.
 **   08.07.2009 H.Kiehl Made functions more RFC-conform by allowing
 **                      to specify special characters with % character
 **                      and two hexa digits.
 **   17.01.2010 H.Kiehl Handle case when no port number is specified.
 **   04.11.2011 H.Kiehl Handle case when the server= part is to long.
 **   27.11.2011 H.Kiehl Added scheme exec.
 **   07.02.2013 H.Kiehl Added %e configuration parameter to allow one
 **                      to execute a command to determine part or the
 **                      full directory name.
 **   06.08.2021 H.Kiehl Added parameter auth=, region= and service=.
 **   24.08.2021 H.Kiehl Added functions url_encode() and url_decode.
 **                      These where taken from Fred Bulback at:
 **                      http://geekhideout.com/urlcode.shtml and
 **                      modified.
 **   13.10.2021 H.Kiehl Added function url_path_encode(). This is
 **                      similar to url_encode() and differs only in
 **                      allowing the path sign /.
 **   23.01.2022 H.Kiehl Do not allow mailto://;server=somename
 **
 */
DESCR__E_M3

#include <stdio.h>      /* snprintf()                                    */
#include <string.h>     /* strcmp(), memmove(), strcpy()                 */
#include <stdlib.h>     /* atoi(), getenv()                              */
#include <ctype.h>      /* tolower(), isxdigit(), isdigit()              */
#include <time.h>       /* strftime(), localtime(), time()               */
#include <unistd.h>     /* gethostname()                                 */

#define EXEC_PATH_TIMEOUT        10 /* in seconds */

#define URL_GET_SCHEME           1
#define URL_GET_USER             2
#define URL_GET_SMTP_AUTH        4
#define URL_GET_SMTP_USER        8
#ifdef WITH_SSH_FINGERPRINT
# define URL_GET_FINGERPRINT     16
# define URL_GET_KEY_TYPE        32
#endif
#define URL_GET_PASSWORD         64
#define URL_GET_HOSTNAME         128
#define URL_GET_PORT             256
#define URL_GET_PATH             512
#define URL_GET_POINTER_PATH     1024
#define URL_GET_TRANSFER_TYPE    2048
#define URL_GET_PROTOCOL_VERSION 4096
#define URL_GET_AUTH             8192
#define URL_GET_REGION           16384
#define URL_GET_SERVICE          32768
#define URL_GET_SERVER           65536

/* Local function prototypes. */
static int insert_alias_name(char *, int *, char *, int *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$ url_evaluate() $$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
unsigned int
url_evaluate(char          *url,
             unsigned int  *scheme,
             char          *user,
             unsigned char *smtp_auth,
             char          *smtp_user,
#ifdef WITH_SSH_FINGERPRINT
             char          *fingerprint,
             char          *key_type,
#endif
             char          *password,
             int           remove_passwd,
             char          *hostname,
             int           *port,
             char          *path,
             char          **p_path_start,
             time_t        *time_val,
             char          *transfer_type,
             unsigned char *protocol_version,
             unsigned char *auth,
             char          *region,
             unsigned char *service,
             char          *server)
{
   unsigned int todo;
   int          url_error = 0;
   char         *ptr;

   /* Determine what needs to be done. */
   todo = 0;
   if (scheme != NULL)
   {
      todo |= URL_GET_SCHEME;
      *scheme = 0;
   }
   if (user != NULL)
   {
      todo |= URL_GET_USER;
      user[0] = '\0';
   }
   if (smtp_auth != NULL)
   {
      todo |= URL_GET_SMTP_AUTH;
      *smtp_auth = SMTP_AUTH_NONE;
   }
   if (smtp_user != NULL)
   {
      todo |= URL_GET_SMTP_USER;
      smtp_user[0] = '\0';
   }
#ifdef WITH_SSH_FINGERPRINT
   if (fingerprint != NULL)
   {
      todo |= URL_GET_FINGERPRINT;
      fingerprint[0] = '\0';
   }
   if (key_type != NULL)
   {
      todo |= URL_GET_KEY_TYPE;
      *key_type = 0;
   }
#endif
   if (password != NULL)
   {
      todo |= URL_GET_PASSWORD;
      password[0] = '\0';
   }
   if (hostname != NULL)
   {
      todo |= URL_GET_HOSTNAME;
      hostname[0] = '\0';
   }
   if (port != NULL)
   {
      todo |= URL_GET_PORT;
      *port = -1;
   }
   if (path != NULL)
   {
      todo |= URL_GET_PATH;
      path[0] = '\0';
   }
   if (p_path_start != NULL)
   {
      todo |= URL_GET_POINTER_PATH;
   }
   if (transfer_type != NULL)
   {
      todo |= URL_GET_TRANSFER_TYPE;
      *transfer_type = DEFAULT_TRANSFER_MODE;
   }
   if (protocol_version != NULL)
   {
      todo |= URL_GET_PROTOCOL_VERSION;
      *protocol_version = 0;
   }
   if (auth != NULL)
   {
      todo |= URL_GET_AUTH;
      *auth = AUTH_NONE;
   }
   if (region != NULL)
   {
      todo |= URL_GET_REGION;
      region[0] = '\0';
   }
   if (service != NULL)
   {
      todo |= URL_GET_SERVICE;
      *service = SERVICE_NONE;
   }
   if (server != NULL)
   {
      todo |= URL_GET_SERVER;
      server[0] = '\0';
   }

   ptr = url;
   if (scheme == NULL)
   {
      /* Ignore scheme. */
      while ((*ptr != '\0') && (*ptr != ':'))
      {
         ptr++;
      }
   }
   else
   {
      /* FTP_SHEME : ftp */
      if ((*ptr == 'f') && (*(ptr + 1) == 't') && (*(ptr + 2) == 'p') &&
          (*(ptr + 3) == ':'))
      {
         *scheme = FTP_FLAG;
         ptr += 3;
      }
           /* LOC_SHEME : file */
      else if ((*ptr == 'f') && (*(ptr + 1) == 'i') && (*(ptr + 2) == 'l') &&
               (*(ptr + 3) == 'e') && (*(ptr + 4) == ':'))
           {
              *scheme = LOC_FLAG;
              ptr += 4;
           }
           /* SMTP_SHEME : mailto */
      else if ((*ptr == 'm') && (*(ptr + 1) == 'a') && (*(ptr + 2) == 'i') &&
               (*(ptr + 3) == 'l') && (*(ptr + 4) == 't') &&
               (*(ptr + 5) == 'o') && (*(ptr + 6) == ':'))
           {
              *scheme = SMTP_FLAG;
              ptr += 6;
           }
           /* SFTP_SHEME : sftp */
      else if ((*ptr == 's') && (*(ptr + 1) == 'f') && (*(ptr + 2) == 't') &&
               (*(ptr + 3) == 'p') && (*(ptr + 4) == ':'))
           {
              *scheme = SFTP_FLAG;
              ptr += 4;
           }
           /* HTTP_SHEME : http */
      else if ((*ptr == 'h') && (*(ptr + 1) == 't') && (*(ptr + 2) == 't') &&
               (*(ptr + 3) == 'p') && (*(ptr + 4) == ':'))
           {
              *scheme = HTTP_FLAG;
              ptr += 4;
           }
#ifdef WITH_SSL
           /* HTTPS_SHEME : https */
      else if ((*ptr == 'h') && (*(ptr + 1) == 't') && (*(ptr + 2) == 't') &&
               (*(ptr + 3) == 'p') && (*(ptr + 4) == 's') &&
               (*(ptr + 5) == ':'))
           {
              *scheme = HTTP_FLAG | SSL_FLAG;
              ptr += 5;
           }
           /* FTPS_SHEME : ftps | ftpS */
      else if ((*ptr == 'f') && (*(ptr + 1) == 't') && (*(ptr + 2) == 'p') &&
               ((*(ptr + 3) == 's') || (*(ptr + 3) == 'S')) &&
               (*(ptr + 4) == ':'))
           {
              *scheme = FTP_FLAG | SSL_FLAG;
              ptr += 4;
           }
           /* SMTPS_SHEME : mailtos */
      else if ((*ptr == 'm') && (*(ptr + 1) == 'a') && (*(ptr + 2) == 'i') &&
               (*(ptr + 3) == 'l') && (*(ptr + 4) == 't') &&
               (*(ptr + 5) == 'o') && (*(ptr + 6) == 's') &&
               (*(ptr + 7) == ':'))
           {
              *scheme = SMTP_FLAG | SSL_FLAG;
              ptr += 7;
           }
#endif
           /* EXEC_SHEME : exec */
      else if ((*ptr == 'e') && (*(ptr + 1) == 'x') && (*(ptr + 2) == 'e') &&
               (*(ptr + 3) == 'c') && (*(ptr + 4) == ':'))
           {
              *scheme = EXEC_FLAG;
              ptr += 4;
           }
#ifdef _WITH_SCP_SUPPORT
           /* SCP_SHEME : scp */
      else if ((*ptr == 's') && (*(ptr + 1) == 'c') && (*(ptr + 2) == 'p') &&
               (*(ptr + 3) == ':'))
           {
              *scheme = SCP_FLAG;
              ptr += 3;
           }
#endif
#ifdef _WITH_WMO_SUPPORT
           /* WMO_SHEME : wmo */
      else if ((*ptr == 'w') && (*(ptr + 1) == 'm') && (*(ptr + 2) == 'o') &&
               (*(ptr + 3) == ':'))
           {
              *scheme = WMO_FLAG;
              ptr += 3;
           }
#endif
#ifdef _WITH_MAP_SUPPORT
           /* MAP_SHEME : map */
      else if ((*ptr == 'm') && (*(ptr + 1) == 'a') && (*(ptr + 2) == 'p') &&
               (*(ptr + 3) == ':'))
           {
              *scheme = MAP_FLAG;
              ptr += 3;
           }
#endif
#ifdef _WITH_DFAX_SUPPORT
           /* DFAX_SHEME : dfax */
      else if ((*ptr == 'd') && (*(ptr + 1) == 'f') && (*(ptr + 2) == 'a') &&
               (*(ptr + 3) == 'x') && (*(ptr + 4) == ':'))
           {
              *scheme = DFAX_FLAG;
              ptr += 4;
           }
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
           /* DEMAIL_SHEME : demail */
      else if ((*ptr == 'd') && (*(ptr + 1) == 'e') && (*(ptr + 2) == 'm') &&
               (*(ptr + 3) == 'a') && (*(ptr + 4) == 'i') &&
               (*(ptr + 5) == 'l') && (*(ptr + 6) == ':'))
           {
              *scheme = DE_MAIL_FLAG;
              ptr += 6;
           }
#endif
           else
           {
              *scheme = UNKNOWN_FLAG;
              url_error |= UNKNOWN_SCHEME;
              while ((*ptr != '\0') && (*ptr != ':'))
              {
                 ptr++;
              }
           }
      todo &= ~URL_GET_SCHEME;
   }
   if (todo)
   {
      if ((*ptr == ':') && (*(ptr + 1) == '/') && (*(ptr + 2) == '/'))
      {
         int  i;
         char *p_start;

         ptr += 3; /* Away with :// */

         /* In case we do not have a @, ie. no user, key and password. */
         p_start = ptr;

         if (user == NULL)
         {
            while ((*ptr != ':') && (*ptr != ';') && (*ptr != '@') &&
                   (*ptr != '/') && (*ptr != '\0'))
            {
               if (*ptr == '\\')
               {
                  ptr++;
               }
               ptr++;
            }
         }
         else
         {
            i = 0;
            while ((*ptr != ':') && (*ptr != ';') && (*ptr != '@') &&
                   (*ptr != '/') && (*ptr != '\0') &&
                   (i < MAX_USER_NAME_LENGTH))
            {
               if (*ptr == '\\')
               {
                  ptr++;
               }
               else if (*ptr == '%')
                    {
                       int value = -1;

                       ptr++;
                       if ((*ptr >= '0') && (*ptr <= '9'))
                       {
                          value = (*ptr - '0') * 16;
                       }
                       else if ((*ptr >= 'a') && (*ptr <= 'f'))
                            {
                               value = ((*ptr - 'a') + 10) * 16;
                            }
                       else if ((*ptr >= 'A') && (*ptr <= 'F'))
                            {
                               value = ((*ptr - 'A') + 10) * 16;
                            }
                       if (value != -1)
                       {
                          ptr++;
                          if ((*ptr >= '0') && (*ptr <= '9'))
                          {
                             value += (*ptr - '0');
                             user[i] = value;
                             ptr++; i++;
                             continue;
                          }
                          else if ((*ptr >= 'a') && (*ptr <= 'f'))
                               {
                                  value += ((*ptr - 'a') + 10);
                                  user[i] = value;
                                  ptr++; i++;
                                  continue;
                               }
                          else if ((*ptr >= 'A') && (*ptr <= 'F'))
                               {
                                  value += ((*ptr - 'A') + 10);
                                  user[i] = value;
                                  ptr++; i++;
                                  continue;
                               }
                               else
                               {
                                  user[i] = '%';
                                  user[i + 1] = *(ptr - 1);
                                  i += 2;
                               }
                       }
                       else
                       {
                          user[i] = '%';
                          i++;
                       }
                    }
               user[i] = *ptr;
               ptr++; i++;
            }
            if (i >= MAX_USER_NAME_LENGTH)
            {
               while ((*ptr != ':') && (*ptr != ';') && (*ptr != '@') &&
                      (*ptr != '/') && (*ptr != '\0'))
               {
                  if (*ptr == '\\')
                  {
                     ptr++;
                  }
                  ptr++;
               }
               url_error |= USER_NAME_TO_LONG;
               user[0] = '\0';
            }
            else
            {
               user[i] = '\0';
               if ((i == 0) && (scheme != NULL) &&
                   (*scheme & SMTP_FLAG) && (*ptr == ';'))
               {
                  return(PARAMETER_MISSING);
               }
            }
            todo &= ~URL_GET_USER;
         }

         /* SSH host key fingerprint or SMTP AUTH. */
         while (*ptr == ';')
         {
            ptr++;
            if (((*ptr == 'a') || (*ptr == 'A')) &&
                ((*(ptr + 1) == 'u') || (*(ptr + 1) == 'U')) &&
                ((*(ptr + 2) == 't') || (*(ptr + 2) == 'T')) &&
                ((*(ptr + 3) == 'h') || (*(ptr + 3) == 'H')) &&
                (*(ptr + 4) == '='))
            {
               ptr += 5;
               if (smtp_auth == NULL)
               {
                  while ((*ptr != ':') && (*ptr != ';') && (*ptr != '@') &&
                         (*ptr != '\0'))
                  {
                     if (*ptr == '\\')
                     {
                        ptr++;
                     }
                     ptr++;
                  }
                  if (*ptr != ';')
                  {
                     url_error |= UNKNOWN_SMTP_AUTH;
                     while ((*ptr != ':') && (*ptr != '@') &&
                            (*ptr != ';') && (*ptr != '\0'))
                     {
                        if (*ptr == '\\')
                        {
                           ptr++;
                        }
                        ptr++;
                     }
                  }
                  else
                  {
                     ptr++;
                  }
               }
               else
               {
                  if (((*ptr == 'l') || (*ptr == 'L')) &&
                      ((*(ptr + 1) == 'o') || (*(ptr + 1) == 'O')) &&
                      ((*(ptr + 2) == 'g') || (*(ptr + 2) == 'G')) &&
                      ((*(ptr + 3) == 'i') || (*(ptr + 3) == 'I')) &&
                      ((*(ptr + 4) == 'n') || (*(ptr + 4) == 'N')) &&
                      (*(ptr + 5) == ';'))
                  {
                     ptr += 6;
                     *smtp_auth = SMTP_AUTH_LOGIN;
                     todo &= ~URL_GET_SMTP_AUTH;
                  }
                  else if (((*ptr == 'p') || (*ptr == 'P')) &&
                           ((*(ptr + 1) == 'l') || (*(ptr + 1) == 'L')) &&
                           ((*(ptr + 2) == 'a') || (*(ptr + 2) == 'A')) &&
                           ((*(ptr + 3) == 'i') || (*(ptr + 3) == 'I')) &&
                           ((*(ptr + 4) == 'n') || (*(ptr + 4) == 'N')) &&
                           (*(ptr + 5) == ';'))
                       {
                          ptr += 6;
                          *smtp_auth = SMTP_AUTH_PLAIN;
                          todo &= ~URL_GET_SMTP_AUTH;
                       }
                       else
                       {
                          url_error |= UNKNOWN_SMTP_AUTH;
                          while ((*ptr != ':') && (*ptr != '@') &&
                                 (*ptr != ';') && (*ptr != '\0'))
                          {
                             if (*ptr == '\\')
                             {
                                ptr++;
                             }
                             ptr++;
                          }
                       }
               }

               if ((url_error & UNKNOWN_SMTP_AUTH) == 0)
               {
                  if (smtp_user == NULL)
                  {
                     while ((*ptr != ':') && (*ptr != ';') && (*ptr != '@') &&
                            (*ptr != '/') && (*ptr != '\0'))
                     {
                        if (*ptr == '\\')
                        {
                           ptr++;
                        }
                        ptr++;
                     }
                  }
                  else
                  {
                     /* Store SMTP user name. */
                     if (((*ptr == 'u') || (*ptr == 'U')) &&
                         ((*(ptr + 1) == 's') || (*(ptr + 1) == 'S')) &&
                         ((*(ptr + 2) == 'e') || (*(ptr + 2) == 'E')) &&
                         ((*(ptr + 3) == 'r') || (*(ptr + 3) == 'R')) &&
                         (*(ptr + 4) == '='))
                     {
                        ptr += 5;
                        i = 0;
                        while ((*ptr != ':') && (*ptr != ';') &&
                               (*ptr != '@') && (*ptr != '/') &&
                               (*ptr != '\0') &&
                               (i < MAX_USER_NAME_LENGTH))
                        {
                           if (*ptr == '\\')
                           {
                              ptr++;
                           }
                           else if (*ptr == '%')
                                {
                                   int value = -1;

                                   ptr++;
                                   if ((*ptr >= '0') && (*ptr <= '9'))
                                   {
                                      value = (*ptr - '0') * 16;
                                   }
                                   else if ((*ptr >= 'a') && (*ptr <= 'f'))
                                        {
                                           value = ((*ptr - 'a') + 10) * 16;
                                        }
                                   else if ((*ptr >= 'A') && (*ptr <= 'F'))
                                        {
                                           value = ((*ptr - 'A') + 10) * 16;
                                        }
                                   if (value != -1)
                                   {
                                      ptr++;
                                      if ((*ptr >= '0') && (*ptr <= '9'))
                                      {
                                         value += (*ptr - '0');
                                         smtp_user[i] = value;
                                         ptr++; i++;
                                         continue;
                                      }
                                      else if ((*ptr >= 'a') && (*ptr <= 'f'))
                                           {
                                              value += ((*ptr - 'a') + 10);
                                              smtp_user[i] = value;
                                              ptr++; i++;
                                              continue;
                                           }
                                      else if ((*ptr >= 'A') && (*ptr <= 'F'))
                                           {
                                              value += ((*ptr - 'A') + 10);
                                              smtp_user[i] = value;
                                              ptr++; i++;
                                              continue;
                                           }
                                           else
                                           {
                                              smtp_user[i] = '%';
                                              smtp_user[i + 1] = *(ptr - 1);
                                              i += 2;
                                           }
                                   }
                                   else
                                   {
                                      smtp_user[i] = '%';
                                      i++;
                                   }
                                }
                           smtp_user[i] = *ptr;
                           ptr++; i++;
                        }
                        if (i >= MAX_USER_NAME_LENGTH)
                        {
                           while ((*ptr != ':') && (*ptr != ';') &&
                                  (*ptr != '@') && (*ptr != '/') &&
                                  (*ptr != '\0'))
                           {
                              if (*ptr == '\\')
                              {
                                 ptr++;
                              }
                              ptr++;
                           }
                           url_error |= USER_NAME_TO_LONG;
                           smtp_user[0] = '\0';
                        }
                        else
                        {
                           smtp_user[i] = '\0';
                        }
                     }
                     else
                     {
                        /*
                         * Hmmm, we could take the current user name as
                         * smtp_user, but not sure if this makes sence?
                         */
                        if (user == NULL)
                        {
                           smtp_user[0] = '\0';
                        }
                        else
                        {
                           (void)strcpy(smtp_user, user);
                        }
                     }
                     todo &= ~URL_GET_SMTP_USER;
                  }
               }
            }
#ifdef WITH_SSH_FINGERPRINT
            else if ((*ptr == 'f') && (*(ptr + 1) == 'i') &&
                     (*(ptr + 2) == 'n') && (*(ptr + 3) == 'g') &&
                     (*(ptr + 4) == 'e') && (*(ptr + 5) == 'r') &&
                     (*(ptr + 6) == 'p') && (*(ptr + 7) == 'r') &&
                     (*(ptr + 8) == 'i') && (*(ptr + 9) == 'n') &&
                     (*(ptr + 10) == 't') && (*(ptr + 11) == '='))
                 {
                    ptr += 12;
                    if (fingerprint == NULL)
                    {
                       while ((*ptr != ':') && (*ptr != '@') && (*ptr != '\0'))
                       {
                          if (*ptr == '\\')
                          {
                             ptr++;
                          }
                          ptr++;
                       }
                    }
                    else
                    {
                       char local_key_type,
                            *p_key_type;

                       if (key_type == NULL)
                       {
                          p_key_type = &local_key_type;
                       }
                       else
                       {
                          p_key_type = key_type;
                       }
                       *p_key_type = SSH_RSA_KEY;

                       /*
                        * Check if public key and/or certificate formats are
                        * defined. We know of the following:
                        *     ssh-dss       Raw DSS Key
                        *     ssh-rsa       Raw RSA Key
                        *     pgp-sign-rsa  OpenPGP certificates (RSA key)
                        *     pgp-sign-dss  OpenPGP certificates (DSS key)
                        *
                        * If none are given, lets just assume ssh-dss.
                        */
                       if ((*ptr == 's') && (*(ptr + 1) == 's') &&
                           (*(ptr + 2) == 'h') && (*(ptr + 3) == '-'))
                       {
                          if ((*(ptr + 4) == 'd') && (*(ptr + 5) == 's') &&
                              (*(ptr + 6) == 's') && (*(ptr + 7) == '-'))
                          {
                             *p_key_type = SSH_DSS_KEY;
                             ptr += 8;
                          }
                          else if ((*(ptr + 4) == 'r') && (*(ptr + 5) == 's') &&
                                   (*(ptr + 6) == 'a') && (*(ptr + 7) == '-'))
                               {
                                  *p_key_type = SSH_RSA_KEY;
                                  ptr += 8;
                               }
                               else
                               {
                                  *p_key_type = 0;
                               }
                       }
                       else if ((*ptr == 'p') && (*(ptr + 1) == 'g') &&
                                (*(ptr + 2) == 'p') && (*(ptr + 3) == '-') &&
                                (*(ptr + 4) == 's') && (*(ptr + 5) == 'i') &&
                                (*(ptr + 6) == 'g') && (*(ptr + 7) == 'n') &&
                                (*(ptr + 8) == '-'))
                            {
                               if ((*(ptr + 9) == 'd') &&
                                   (*(ptr + 10) == 's') &&
                                   (*(ptr + 11) == 's') &&
                                   (*(ptr + 12) == '-'))
                               {
                                  *p_key_type = SSH_PGP_DSS_KEY;
                                  ptr += 13;
                               }
                               else if ((*(ptr + 9) == 'r') &&
                                        (*(ptr + 10) == 's') &&
                                        (*(ptr + 11) == 'a') &&
                                        (*(ptr + 12) == '-'))
                                    {
                                       *p_key_type = SSH_PGP_RSA_KEY;
                                       ptr += 13;
                                    }
                                    else
                                    {
                                       *p_key_type = 0;
                                    }
                            }

                       if (*p_key_type == 0)
                       {
                          url_error |= UNKNOWN_KEY_TYPE;
                          while ((*ptr != ':') && (*ptr != '@') &&
                                 (*ptr != '\0'))
                          {
                             if (*ptr == '\\')
                             {
                                ptr++;
                             }
                             ptr++;
                          }
                       }
                       else
                       {
                          /* Get fingerprint. */
                          if ((isxdigit((int)(*ptr))) &&
                              (isxdigit((int)(*(ptr + 1)))) &&
                              (*(ptr + 2) == '-') &&
                              (isxdigit((int)(*(ptr + 3)))) &&
                              (isxdigit((int)(*(ptr + 4)))) &&
                              (*(ptr + 5) == '-') &&
                              (isxdigit((int)(*(ptr + 6)))) &&
                              (isxdigit((int)(*(ptr + 7)))) &&
                              (*(ptr + 8) == '-') &&
                              (isxdigit((int)(*(ptr + 9)))) &&
                              (isxdigit((int)(*(ptr + 10)))) &&
                              (*(ptr + 11) == '-') &&
                              (isxdigit((int)(*(ptr + 12)))) &&
                              (isxdigit((int)(*(ptr + 13)))) &&
                              (*(ptr + 14) == '-') &&
                              (isxdigit((int)(*(ptr + 15)))) &&
                              (isxdigit((int)(*(ptr + 16)))) &&
                              (*(ptr + 17) == '-') &&
                              (isxdigit((int)(*(ptr + 18)))) &&
                              (isxdigit((int)(*(ptr + 19)))) &&
                              (*(ptr + 20) == '-') &&
                              (isxdigit((int)(*(ptr + 21)))) &&
                              (isxdigit((int)(*(ptr + 22)))) &&
                              (*(ptr + 23) == '-') &&
                              (isxdigit((int)(*(ptr + 24)))) &&
                              (isxdigit((int)(*(ptr + 25)))) &&
                              (*(ptr + 26) == '-') &&
                              (isxdigit((int)(*(ptr + 27)))) &&
                              (isxdigit((int)(*(ptr + 28)))) &&
                              (*(ptr + 29) == '-') &&
                              (isxdigit((int)(*(ptr + 30)))) &&
                              (isxdigit((int)(*(ptr + 31)))) &&
                              (*(ptr + 32) == '-') &&
                              (isxdigit((int)(*(ptr + 33)))) &&
                              (isxdigit((int)(*(ptr + 34)))) &&
                              (*(ptr + 35) == '-') &&
                              (isxdigit((int)(*(ptr + 36)))) &&
                              (isxdigit((int)(*(ptr + 37)))) &&
                              (*(ptr + 38) == '-') &&
                              (isxdigit((int)(*(ptr + 39)))) &&
                              (isxdigit((int)(*(ptr + 40)))) &&
                              (*(ptr + 41) == '-') &&
                              (isxdigit((int)(*(ptr + 42)))) &&
                              (isxdigit((int)(*(ptr + 43)))) &&
                              (*(ptr + 44) == '-') &&
                              (isxdigit((int)(*(ptr + 45)))) &&
                              (isxdigit((int)(*(ptr + 46)))))
                          {
                             fingerprint[0] = tolower((int)(*ptr));
                             fingerprint[1] = tolower((int)(*(ptr + 1)));
                             fingerprint[2] = ':';
                             fingerprint[3] = tolower((int)(*(ptr + 3)));
                             fingerprint[4] = tolower((int)(*(ptr + 4)));
                             fingerprint[5] = ':';
                             fingerprint[6] = tolower((int)(*(ptr + 6)));
                             fingerprint[7] = tolower((int)(*(ptr + 7)));
                             fingerprint[8] = ':';
                             fingerprint[9] = tolower((int)(*(ptr + 9)));
                             fingerprint[10] = tolower((int)(*(ptr + 10)));
                             fingerprint[11] = ':';
                             fingerprint[12] = tolower((int)(*(ptr + 12)));
                             fingerprint[13] = tolower((int)(*(ptr + 13)));
                             fingerprint[14] = ':';
                             fingerprint[15] = tolower((int)(*(ptr + 15)));
                             fingerprint[16] = tolower((int)(*(ptr + 16)));
                             fingerprint[17] = ':';
                             fingerprint[18] = tolower((int)(*(ptr + 18)));
                             fingerprint[19] = tolower((int)(*(ptr + 19)));
                             fingerprint[20] = ':';
                             fingerprint[21] = tolower((int)(*(ptr + 21)));
                             fingerprint[22] = tolower((int)(*(ptr + 22)));
                             fingerprint[23] = ':';
                             fingerprint[24] = tolower((int)(*(ptr + 24)));
                             fingerprint[25] = tolower((int)(*(ptr + 25)));
                             fingerprint[26] = ':';
                             fingerprint[27] = tolower((int)(*(ptr + 27)));
                             fingerprint[28] = tolower((int)(*(ptr + 28)));
                             fingerprint[29] = ':';
                             fingerprint[30] = tolower((int)(*(ptr + 30)));
                             fingerprint[31] = tolower((int)(*(ptr + 31)));
                             fingerprint[32] = ':';
                             fingerprint[33] = tolower((int)(*(ptr + 33)));
                             fingerprint[34] = tolower((int)(*(ptr + 34)));
                             fingerprint[35] = ':';
                             fingerprint[36] = tolower((int)(*(ptr + 36)));
                             fingerprint[37] = tolower((int)(*(ptr + 37)));
                             fingerprint[38] = ':';
                             fingerprint[39] = tolower((int)(*(ptr + 39)));
                             fingerprint[40] = tolower((int)(*(ptr + 40)));
                             fingerprint[41] = ':';
                             fingerprint[42] = tolower((int)(*(ptr + 42)));
                             fingerprint[43] = tolower((int)(*(ptr + 43)));
                             fingerprint[44] = ':';
                             fingerprint[45] = tolower((int)(*(ptr + 45)));
                             fingerprint[46] = tolower((int)(*(ptr + 46)));
                             fingerprint[47] = '\0';
                             ptr += 47;
                          }
                          else
                          {
                             url_error |= NOT_A_FINGERPRINT;
                             while ((*ptr != ':') && (*ptr != '@') &&
                                    (*ptr != '\0'))
                             {
                                if (*ptr == '\\')
                                {
                                   ptr++;
                                }
                                ptr++;
                             }
                          }
                       }
                    }
                    todo &= ~URL_GET_KEY_TYPE;
                    todo &= ~URL_GET_FINGERPRINT;
                 }
#endif /* WITH_SSH_FINGERPRINT */
                 else
                 {
                    url_error |= ONLY_FINGERPRINT_KNOWN;
                    while ((*ptr != ':') && (*ptr != '@') && (*ptr != '\0'))
                    {
                       if (*ptr == '\\')
                       {
                          ptr++;
                       }
                       ptr++;
                    }
                 }
         }

         /* Store password. */
         if (*ptr == ':')
         {
            char *p_start_pwd;

            p_start_pwd = ptr;
            ptr++; /* Away with : */

            if (password == NULL)
            {
               while ((*ptr != '@') && (*ptr != '/') && (*ptr != '\0'))
               {
                  if (*ptr == '\\')
                  {
                     ptr++;
                  }
                  ptr++;
               }
            }
            else
            {
               i = 0;
               while ((*ptr != '@') && (*ptr != '/') && (*ptr != '\0') &&
                      (i < MAX_USER_NAME_LENGTH))
               {
                  if (*ptr == '\\')
                  {
                     ptr++;
                  }
                  password[i] = *ptr;
                  ptr++; i++;
               }
               todo &= ~URL_GET_PASSWORD;
               if (i >= MAX_USER_NAME_LENGTH)
               {
                  url_error |= PASSWORD_TO_LONG;
                  while ((*ptr != '@') && (*ptr != '/') && (*ptr != '\0'))
                  {
                     ptr++;
                  }
                  password[0] = '\0';
               }
               else
               {
                  password[i] = '\0';
               }
            }
            if ((remove_passwd == YES) && (*ptr == '@') &&
                ((p_start_pwd + 1) != ptr))
            {
               /* Remove the password from url. */
               (void)memmove(p_start_pwd, ptr, strlen(ptr) + 1);
               ptr = p_start_pwd;
            }
         }
         else
         {
            todo &= ~URL_GET_PASSWORD;
         }

         /*
          * Only when we find the @ sign can we for certain say that
          * the stored values for user, fingerprint and password are
          * really representing the respected values. We could for
          * example have a URL without the @ sign such as:
          *      http//ducktown:2424
          */
         if (*ptr == '@')
         {
            ptr++;
         }
         else
         {
            if (url_error & UNKNOWN_SCHEME)
            {
               url_error = UNKNOWN_SCHEME;
            }
            else
            {
               url_error = 0;
            }
            if (user != NULL)
            {
               user[0] = '\0';
            }
            if (smtp_user != NULL)
            {
               smtp_user[0] = '\0';
            }
#ifdef WITH_SSH_FINGERPRINT
            if (key_type != NULL)
            {
               *key_type = 0;
            }
            if (fingerprint != NULL)
            {
               fingerprint[0] = '\0';
            }
#endif
            if (password != NULL)
            {
               password[0] = '\0';
            }
            ptr = p_start;
         }

         if (todo)
         {
            if (hostname == NULL)
            {
               while ((*ptr != '\0') && (*ptr != '/') && (*ptr != ':') &&
                      (*ptr != ';'))
               {
                  if (*ptr == '\\')
                  {
                     ptr++;
                  }
                  ptr++;
               }
            }
            else
            {
               int offset;

               i = 0;
               while ((*ptr != '\0') && (*ptr != '/') && (*ptr != ':') &&
                      (*ptr != ';') && (i < MAX_REAL_HOSTNAME_LENGTH))
               {
                  if (*ptr == '\\')
                  {
                     ptr++;
                  }
                  else if (*ptr == '+')
                       {
                          hostname[i] = ' ';
                          ptr++; i++;
                          continue;
                       }
                  else if (*ptr == '%')
                       {
                          int value = -1;

                          ptr++;
                          if ((*ptr >= '0') && (*ptr <= '9'))
                          {
                             value = (*ptr - '0') * 16;
                          }
                          else if ((*ptr >= 'a') && (*ptr <= 'f'))
                               {
                                  value = ((*ptr - 'a') + 10) * 16;
                               }
                          else if ((*ptr >= 'A') && (*ptr <= 'F'))
                               {
                                  value = ((*ptr - 'A') + 10) * 16;
                               }
                          if (value != -1)
                          {
                             ptr++;
                             if ((*ptr >= '0') && (*ptr <= '9'))
                             {
                                value += (*ptr - '0');
                                hostname[i] = value;
                                ptr++; i++;
                                continue;
                             }
                             else if ((*ptr >= 'a') && (*ptr <= 'f'))
                                  {
                                     value += ((*ptr - 'a') + 10);
                                     if ((value == '<') &&
                                         (insert_alias_name(ptr + 1, &offset,
                                                            hostname, &i)))
                                     {
                                        ptr += offset + 1;
                                     }
                                     else
                                     {
                                        hostname[i] = value;
                                        ptr++; i++;
                                     }
                                     continue;
                                  }
                             else if ((*ptr >= 'A') && (*ptr <= 'F'))
                                  {
                                     value += ((*ptr - 'A') + 10);
                                     if ((value == '<') &&
                                         (insert_alias_name(ptr + 1, &offset,
                                                            hostname, &i)))
                                     {
                                        ptr += offset + 1;
                                     }
                                     else
                                     {
                                        hostname[i] = value;
                                        ptr++; i++;
                                     }
                                     continue;
                                  }
                                  else
                                  {
                                     hostname[i] = '%';
                                     hostname[i + 1] = *(ptr - 1);
                                     i += 2;
                                  }
                          }
                          else
                          {
                             hostname[i] = '%';
                             i++;
                          }
                       }
                  else if ((*ptr == '<') &&
                           (insert_alias_name(ptr + 1, &offset, hostname, &i)))
                       {
                          ptr += offset + 1;
                          continue;
                       }
                  hostname[i] = *ptr;
                  i++; ptr++;
               }
               if (i >= MAX_REAL_HOSTNAME_LENGTH)
               {
                  url_error |= HOSTNAME_TO_LONG;
                  while ((*ptr != '\0') && (*ptr != '/') &&  (*ptr != ':') &&
                         (*ptr != ';'))
                  {
                     ptr++;
                  }
               }
               else
               {
                  hostname[i] = '\0';
               }
               todo &= ~URL_GET_HOSTNAME;
            }

            if (todo)
            {
               if (*ptr == ':')
               {
                  ptr++;
                  if (port == NULL)
                  {
                     while ((*ptr != '/') && (*ptr != '\0') && (*ptr != ';'))
                     {
                        ptr++;
                     }
                  }
                  else
                  {
                     char str_number[MAX_INT_LENGTH];

                     i = 0;
                     while ((*ptr != '/') && (*ptr != '\0') && (*ptr != ';') &&
                            (i < MAX_INT_LENGTH))
                     {
                        if (*ptr == '\\')
                        {
                           ptr++;
                        }
                        str_number[i] = *ptr;
                        i++; ptr++;
                     }
                     if (i >= MAX_INT_LENGTH)
                     {
                        url_error |= PORT_TO_LONG;
                        while ((*ptr != '/') && (*ptr != '\0') && (*ptr != ';'))
                        {
                           ptr++;
                        }
                     }
                     else if (i == 0)
                          {
                             url_error |= NO_PORT_SPECIFIED;
                          }
                          else
                          {
                             str_number[i] = '\0';
                             *port = atoi(str_number);
                          }
                     todo &= ~URL_GET_PORT;
                  }
               }
               else
               {
                  todo &= ~URL_GET_PORT;
               }

               if (todo)
               {
                  char *ptr_tmp;

                  if (*ptr == '/')
                  {
                     ptr++;
                     if (p_path_start != NULL)
                     {
                        *p_path_start = ptr;
                        todo &= ~URL_GET_POINTER_PATH;
                     }
                     if (path == NULL)
                     {
                        while ((*ptr != '\0') && (*ptr != ';'))
                        {
                           if (*ptr == '\\')
                           {
                              ptr++;
                           }
                           ptr++;
                        }
                     }
                     else
                     {
                        i = 0;
                        if (time_val == NULL)
                        {
                           while ((*ptr != '\0') && (*ptr != ';') &&
                                  (i < MAX_RECIPIENT_LENGTH))
                           {
                              if (*ptr == '\\')
                              {
                                 ptr++;
                              }
                              else if (*ptr == '+')
                                   {
                                      path[i] = ' ';
                                      ptr++; i++;
                                      continue;
                                   }
                              else if (*ptr == '%')
                                   {
                                      int value = -1;

                                      ptr++;
                                      if ((*ptr >= '0') && (*ptr <= '9'))
                                      {
                                         value = (*ptr - '0') * 16;
                                      }
                                      else if ((*ptr >= 'a') && (*ptr <= 'f'))
                                           {
                                              value = ((*ptr - 'a') + 10) * 16;
                                           }
                                      else if ((*ptr >= 'A') && (*ptr <= 'F'))
                                           {
                                              value = ((*ptr - 'A') + 10) * 16;
                                           }
                                      if (value != -1)
                                      {
                                         ptr++;
                                         if ((*ptr >= '0') && (*ptr <= '9'))
                                         {
                                            value += (*ptr - '0');
                                            path[i] = value;
                                            ptr++; i++;
                                            continue;
                                         }
                                         else if ((*ptr >= 'a') && (*ptr <= 'f'))
                                              {
                                                 value += ((*ptr - 'a') + 10);
                                                 path[i] = value;
                                                 ptr++; i++;
                                                 continue;
                                              }
                                         else if ((*ptr >= 'A') && (*ptr <= 'F'))
                                              {
                                                 value += ((*ptr - 'A') + 10);
                                                 path[i] = value;
                                                 ptr++; i++;
                                                 continue;
                                              }
                                              else
                                              {
                                                 path[i] = '%';
                                                 path[i + 1] = *(ptr - 1);
                                                 i += 2;
                                              }
                                      }
                                      else
                                      {
                                         path[i] = '%';
                                         i++;
                                      }
                                   }
                              path[i] = *ptr;
                              i++; ptr++;
                           }
                        }
                        else
                        {
                           time_t time_modifier = 0;
                           char   time_mod_sign = '+';

                           while ((*ptr != '\0') && (*ptr != ';') &&
                                  (i < MAX_RECIPIENT_LENGTH))
                           {
                              if (*ptr == '\\')
                              {
                                 ptr++;
                                 path[i] = *ptr;
                                 i++; ptr++;
                              }
                              else
                              {
                                 /* %t - insert date/time. */
                                 if ((*ptr == '%') && (*(ptr + 1) == 't'))
                                 {
                                    int    number;
                                    time_t time_buf = *time_val;

                                    if (time_buf == 0L)
                                    {
                                       time_buf = time(NULL);
                                    }
                                    if (time_modifier > 0)
                                    {
                                       switch (time_mod_sign)
                                       {
                                          case '-' :
                                             time_buf = time_buf - time_modifier;
                                             break;
                                          case '*' :
                                             time_buf = time_buf * time_modifier;
                                             break;
                                          case '/' :
                                             time_buf = time_buf / time_modifier;
                                             break;
                                          case '%' :
                                             time_buf = time_buf % time_modifier;
                                             break;
                                          case '+' :
                                          default  :
                                             time_buf = time_buf + time_modifier;
                                             break;
                                       }
                                    }
                                    switch (*(ptr + 2))
                                    {
                                       case 'a': /* short day of the week 'Tue' */
                                          number = strftime(&path[i],
                                                            MAX_RECIPIENT_LENGTH - i,
                                                            "%a", localtime(&time_buf));
                                          url_error |= TARGET_DIR_CAN_CHANGE;
                                          break;
                                       case 'b': /* short month 'Jan' */
                                          number = strftime(&path[i],
                                                            MAX_RECIPIENT_LENGTH - i,
                                                            "%b", localtime(&time_buf));
                                          url_error |= TARGET_DIR_CAN_CHANGE;
                                          break;
                                       case 'j': /* day of year [001,366] */
                                          number = strftime(&path[i],
                                                            MAX_RECIPIENT_LENGTH - i,
                                                            "%j", localtime(&time_buf));
                                          url_error |= TARGET_DIR_CAN_CHANGE;
                                          break;
                                       case 'i': /* day of month [1,31] */
                                          number = strftime(&path[i],
                                                            MAX_RECIPIENT_LENGTH - i,
                                                            "%d", localtime(&time_buf));
                                          if (path[i] == '0')
                                          {
                                             path[i] = path[i + 1];
                                             path[i + 1] = '\0';
                                             number = 1;
                                          }
                                          url_error |= TARGET_DIR_CAN_CHANGE;
                                          break;
                                       case 'd': /* day of month [01,31] */
                                          number = strftime(&path[i],
                                                            MAX_RECIPIENT_LENGTH - i,
                                                            "%d", localtime(&time_buf));
                                          url_error |= TARGET_DIR_CAN_CHANGE;
                                          break;
                                       case 'M': /* minute [00,59] */
                                          number = strftime(&path[i],
                                                            MAX_RECIPIENT_LENGTH - i,
                                                            "%M", localtime(&time_buf));
                                          url_error |= TARGET_DIR_CAN_CHANGE;
                                          break;
                                       case 'J': /* month [1,12] */
                                          number = strftime(&path[i],
                                                            MAX_RECIPIENT_LENGTH - i,
                                                            "%m", localtime(&time_buf));
                                          if (path[i] == '0')
                                          {
                                             path[i] = path[i + 1];
                                             path[i + 1] = '\0';
                                             number = 1;
                                          }
                                          url_error |= TARGET_DIR_CAN_CHANGE;
                                          break;
                                       case 'm': /* month [01,12] */
                                          number = strftime(&path[i],
                                                            MAX_RECIPIENT_LENGTH - i,
                                                            "%m", localtime(&time_buf));
                                          url_error |= TARGET_DIR_CAN_CHANGE;
                                          break;
                                       case 'R': /* Sunday week number [00,53]. */
                                          number = strftime(&path[i],
                                                            MAX_RECIPIENT_LENGTH - i,
                                                            "%U", localtime(&time_buf));
                                          url_error |= TARGET_DIR_CAN_CHANGE;
                                          break;
                                       case 'w': /* Weekday [0=Sunday,6]. */
                                          number = strftime(&path[i],
                                                            MAX_RECIPIENT_LENGTH - i,
                                                            "%w", localtime(&time_buf));
                                          url_error |= TARGET_DIR_CAN_CHANGE;
                                          break;
                                       case 'W': /* Monday week number [00,53]. */
                                          number = strftime(&path[i],
                                                            MAX_RECIPIENT_LENGTH - i,
                                                            "%W", localtime(&time_buf));
                                          url_error |= TARGET_DIR_CAN_CHANGE;
                                          break;
                                       case 'y': /* year 2 chars [01,99] */
                                          number = strftime(&path[i],
                                                            MAX_RECIPIENT_LENGTH - i,
                                                            "%y", localtime(&time_buf));
                                          url_error |= TARGET_DIR_CAN_CHANGE;
                                          break;
                                       case 'o': /* hour [0,23] */
                                          number = strftime(&path[i],
                                                            MAX_RECIPIENT_LENGTH - i,
                                                            "%H", localtime(&time_buf));
                                          if (path[i] == '0')
                                          {
                                             path[i] = path[i + 1];
                                             path[i + 1] = '\0';
                                             number = 1;
                                          }
                                          url_error |= TARGET_DIR_CAN_CHANGE;
                                          break;
                                       case 'H': /* hour [00,23] */
                                          number = strftime(&path[i],
                                                            MAX_RECIPIENT_LENGTH - i,
                                                            "%H", localtime(&time_buf));
                                          url_error |= TARGET_DIR_CAN_CHANGE;
                                          break;
                                       case 'S': /* second [00,59] */
                                          number = strftime(&path[i],
                                                            MAX_RECIPIENT_LENGTH - i,
                                                            "%S", localtime(&time_buf));
                                          url_error |= TARGET_DIR_CAN_CHANGE;
                                          break;
                                       case 'Y': /* year 4 chars 2002 */
                                          number = strftime(&path[i],
                                                            MAX_RECIPIENT_LENGTH - i,
                                                            "%Y", localtime(&time_buf));
                                          url_error |= TARGET_DIR_CAN_CHANGE;
                                          break;
                                       case 'A': /* long day of the week 'Tuesday' */
                                          number = strftime(&path[i],
                                                            MAX_RECIPIENT_LENGTH - i,
                                                            "%A", localtime(&time_buf));
                                          url_error |= TARGET_DIR_CAN_CHANGE;
                                          break;
                                       case 'B': /* month 'January' */
                                          number = strftime(&path[i],
                                                            MAX_RECIPIENT_LENGTH - i,
                                                            "%B", localtime(&time_buf));
                                          url_error |= TARGET_DIR_CAN_CHANGE;
                                          break;
                                       case 'U': /* Unix time. */
                                          number = snprintf(&path[i],
                                                             MAX_RECIPIENT_LENGTH - i,
#if SIZEOF_TIME_T == 4
                                                            "%ld",
#else
                                                            "%lld",
#endif
                                                            (pri_time_t)time_buf);
                                          if (number > (MAX_RECIPIENT_LENGTH - i))
                                          {
                                             url_error |= BUFFER_TO_SHORT;
                                             number = MAX_RECIPIENT_LENGTH - i;
                                          }
                                          url_error |= TARGET_DIR_CAN_CHANGE;
                                          break;
                                       default :
                                          number = 3;
                                          path[i] = '%';
                                          path[i + 1] = 't';
                                          path[i + 2] = *(ptr + 2);
                                          break;
                                    }
                                    i += number;
                                    ptr += 3;
                                 }
                                      /* %T - time modifier. */
                                 else if ((*ptr == '%') && (*(ptr + 1) == 'T'))
                                      {
                                         int  j,
                                              time_unit;
                                         char string[MAX_INT_LENGTH];

                                         ptr += 2;
                                         switch (*ptr)
                                         {
                                            case '+' :
                                            case '-' :
                                            case '*' :
                                            case '/' :
                                            case '%' :
                                               time_mod_sign = *ptr;
                                               ptr++;
                                               break;
                                            default  :
                                               time_mod_sign = '+';
                                               break;
                                         }
                                         j = 0;
                                         while ((isdigit((int)(*ptr))) && (j < MAX_INT_LENGTH))
                                         {
                                            string[j++] = *ptr++;
                                         }
                                         if ((j > 0) && (j < MAX_INT_LENGTH))
                                         {
                                            string[j] = '\0';
                                            time_modifier = atoi(string);
                                         }
                                         else
                                         {
                                            if (j >= MAX_INT_LENGTH)
                                            {
                                               url_error |= TIME_MODIFIER_TO_LONG;
                                               while (isdigit((int)(*ptr)))
                                               {
                                                  ptr++;
                                               }
                                            }
                                            else
                                            {
                                               url_error |= NO_TIME_MODIFIER_SPECIFIED;
                                            }
                                            time_modifier = 0;
                                         }
                                         switch (*ptr)
                                         {
                                            case 'S' : /* Second. */
                                               time_unit = 1;
                                               ptr++;
                                               break;
                                            case 'M' : /* Minute. */
                                               time_unit = 60;
                                               ptr++;
                                               break;
                                            case 'H' : /* Hour. */
                                               time_unit = 3600;
                                               ptr++;
                                               break;
                                            case 'd' : /* Day. */
                                               time_unit = 86400;
                                               ptr++;
                                               break;
                                            default :
                                               time_unit = 1;
                                               break;
                                         }
                                         if (time_modifier > 0)
                                         {
                                            time_modifier = time_modifier * time_unit;
                                         }
                                      }
                                      /* %h - add current hostname. */
                                 else if ((*ptr == '%') && (*(ptr + 1) == 'h'))
                                      {
                                         int length;
                                         char hostname[60];

                                         if (gethostname(hostname, 60) == -1)
                                         {
                                            char *p_hostname;

                                            if ((p_hostname = getenv("HOSTNAME")) != NULL)
                                            {
                                               length = snprintf(&path[i],
                                                                 MAX_RECIPIENT_LENGTH - i,
                                                                 "%s", p_hostname);
                                               if (length > (MAX_RECIPIENT_LENGTH - i))
                                               {
                                                  url_error |= BUFFER_TO_SHORT;
                                                  length = MAX_RECIPIENT_LENGTH - i;
                                               }
                                               i += length;
                                            }
                                            else
                                            {
                                               path[i] = *ptr;
                                               path[i + 1] = *(ptr + 1);
                                               i += 2;
                                            }
                                         }
                                         else
                                         {
                                            length = snprintf(&path[i],
                                                              MAX_RECIPIENT_LENGTH - i,
                                                              "%s", hostname);
                                            if (length > (MAX_RECIPIENT_LENGTH - i))
                                            {
                                               url_error |= BUFFER_TO_SHORT;
                                               length = MAX_RECIPIENT_LENGTH - i;
                                            }
                                            i += length;
                                         }
                                         ptr += 2;
                                      }
                                      /* %e - execute command to get path. */
                                 else if ((*ptr == '%') && (*(ptr + 1) == 'e') &&
                                          (*(ptr + 2) == '`'))
                                      {
                                         int  ii = 0;
                                         char cmd[MAX_RECIPIENT_LENGTH + 1];

                                         while ((ii < MAX_RECIPIENT_LENGTH) &&
                                                (*(ptr + 3 + ii) != '`') &&
                                                (*(ptr + 3 + ii) != '\0'))
                                         {
                                            cmd[ii] = *(ptr + 3 + ii);
                                            ii++;
                                         }
                                         if (*(ptr + 3 + ii) == '`')
                                         {
                                            int  ret;
                                            char *return_str = NULL;

                                            cmd[ii] = '\0';

                                            /* do it */
                                            if ((ret = exec_cmd(cmd,
                                                                &return_str,
                                                                -1, NULL, 0,
#ifdef HAVE_SETPRIORITY
                                                                NO_PRIORITY,
#endif
                                                                NULL, NULL,
                                                                NULL, 0,
                                                                EXEC_PATH_TIMEOUT,
                                                                YES, YES)) != 0)
                                            {
                                               url_error |= EXEC_CMD_ERROR;
                                               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                          "exec command `%s' failed and returned %d",
                                                          cmd, ret);
                                            }
                                            else
                                            {
                                               if (return_str == NULL)
                                               {
                                                  url_error |= EXEC_NO_RETURN;
                                               }
                                               else
                                               {
                                                  int length;

                                                  if ((length = strlen(return_str)) > 0)
                                                  {
                                                     if (return_str[length - 1] == '\n')
                                                     {
                                                        return_str[length - 1] = '\0';
                                                     }
                                                     (void)my_strncpy(&path[i],
                                                                      return_str,
                                                                      MAX_RECIPIENT_LENGTH - i);
                                                     i += (length - 1);
                                                  }

                                                  /* Away with the terminating ` */
                                                  ii++;
                                                  url_error |= TARGET_DIR_CAN_CHANGE;
                                               }
                                            }
                                            free(return_str);
                                         }
                                         else if (*(ptr + 3 + ii) == '\0')
                                              {
                                                 url_error |= EXEC_NOT_TERMINATED;
                                              }
                                              else
                                              {
                                                 url_error |= EXEC_CMD_TO_LONG;
                                              }

                                         ptr += (3 + ii);
                                      }
                                 else if (*ptr == '+')
                                      {
                                         path[i] = ' ';
                                         ptr++; i++;
                                      }
                                 else if (*ptr == '%')
                                      {
                                         int value = -1;

                                         ptr++;
                                         if ((*ptr >= '0') && (*ptr <= '9'))
                                         {
                                            value = (*ptr - '0') * 16;
                                         }
                                         else if ((*ptr >= 'a') && (*ptr <= 'f'))
                                              {
                                                 value = ((*ptr - 'a') + 10) * 16;
                                              }
                                         else if ((*ptr >= 'A') && (*ptr <= 'F'))
                                              {
                                                 value = ((*ptr - 'A') + 10) * 16;
                                              }
                                         if (value != -1)
                                         {
                                            ptr++;
                                            if ((*ptr >= '0') && (*ptr <= '9'))
                                            {
                                               value += (*ptr - '0');
                                               path[i] = value;
                                               ptr++; i++;
                                            }
                                            else if ((*ptr >= 'a') && (*ptr <= 'f'))
                                                 {
                                                    value += ((*ptr - 'a') + 10);
                                                    path[i] = value;
                                                    ptr++; i++;
                                                 }
                                            else if ((*ptr >= 'A') && (*ptr <= 'F'))
                                                 {
                                                    value += ((*ptr - 'A') + 10);
                                                    path[i] = value;
                                                    ptr++; i++;
                                                 }
                                                 else
                                                 {
                                                    path[i] = '%';
                                                    path[i + 1] = *(ptr - 1);
                                                    path[i + 2] = *ptr;
                                                    i += 3;
                                                    ptr++;
                                                 }
                                         }
                                         else
                                         {
                                            path[i] = '%';
                                            i++;
                                         }
                                      }
                                      else
                                      {
                                         path[i] = *ptr;
                                         i++; ptr++;
                                      }
                              }
                           }
                        }
                        if (i >= MAX_RECIPIENT_LENGTH)
                        {
                           path[0] = '\0';
                           url_error |= RECIPIENT_TO_LONG;
                           while ((*ptr != '\0') && (*ptr != ';'))
                           {
                              if (*ptr == '\\')
                              {
                                 ptr++;
                              }
                              ptr++;
                           }
                        }
                        else
                        {
                           path[i] = '\0';
                        }
                        todo &= ~URL_GET_PATH;
                     }
                  }
                  else
                  {
                     todo &= ~URL_GET_PATH;
                     if (p_path_start != NULL)
                     {
                        *p_path_start = ptr;
                        todo &= ~URL_GET_POINTER_PATH;
                     }
                  }

                  while ((todo) && (*ptr == ';') && (*ptr != '\0'))
                  {
                     ptr++;
                     ptr_tmp = ptr;
                     while ((*ptr != '\0') && (*ptr != '='))
                     {
                        ptr++;
                     }
                     if (*ptr == '=')
                     {
                        i = ptr - ptr_tmp;
                        if ((transfer_type != NULL) && (i == 4) &&
                            (*(ptr - 1) == 'e') && (*(ptr - 2) == 'p') &&
                            (*(ptr - 3) == 'y') && (*(ptr - 4) == 't'))
                        {
                           ptr++;
                           switch (*ptr)
                           {
                              case 'a': /* ASCII. */
                              case 'A': *transfer_type = 'A';
                                        break;
                              case 'd': /* DOS binary. */
                              case 'D': *transfer_type = 'D';
                                        break;
                              case 'i': /* Image/binary. */
                              case 'I': *transfer_type = 'I';
                                        break;
                              case 'n': /* None, for sending no type. */
                              case 'N': *transfer_type = 'N';
                                        break;
#ifdef _WITH_WMO_SUPPORT
                              case 'f': /* Indicates FAX message in scheme wmo. */
                              case 'F': *transfer_type = 'F';
                                        break;
#endif
                              default : url_error |= UNKNOWN_TRANSFER_TYPE;
                                        *transfer_type = 'I';
                                        break;
                           }
                           todo &= ~URL_GET_TRANSFER_TYPE;
                        }
                             /* server= */
                        else if ((server != NULL) && (i == 6) &&
                                 (*(ptr - 1) == 'r') &&
                                 (*(ptr - 2) == 'e') &&
                                 (*(ptr - 3) == 'v') &&
                                 (*(ptr - 4) == 'r') &&
                                 (*(ptr - 5) == 'e') && (*(ptr - 6) == 's'))
                             {
                                ptr++;
                                i = 0;
                                while ((*ptr != '\0') && (*ptr != ' ') &&
                                       (*ptr != '\t') && (*ptr != ';') &&
                                       (i < MAX_REAL_HOSTNAME_LENGTH))
                                {
                                   server[i] = *ptr;
                                   i++; ptr++;
                                }
                                if (i >= MAX_REAL_HOSTNAME_LENGTH)
                                {
                                   url_error |= SERVER_NAME_TO_LONG;
                                   server[0] = '\0';
                                   while ((*ptr != '\0') && (*ptr != ' ') &&
                                          (*ptr != '\t') && (*ptr != ';'))
                                   {
                                      if (*ptr == '\\')
                                      {
                                         ptr++;
                                      }
                                      ptr++;
                                   }
                                }
                                else
                                {
                                   server[i] = '\0';
                                }

                                todo &= ~URL_GET_SERVER;
                             }
                             /* auth= */
                        else if ((auth != NULL) && (i == 4) &&
                                 (*(ptr - 1) == 'h') &&
                                 (*(ptr - 2) == 't') &&
                                 (*(ptr - 3) == 'u') &&
                                 (*(ptr - 4) == 'a'))
                             {
                                ptr++;
                                /* basic */
                                if ((*ptr == 'b') && (*(ptr + 1) == 'a') &&
                                    (*(ptr + 2) == 's') &&
                                    (*(ptr + 3) == 'i') &&
                                    (*(ptr + 4) == 'c') &&
                                    ((*(ptr + 5) == '\0') ||
                                     (*(ptr + 5) == ';') ||
                                     (*(ptr + 5) == ' ') ||
                                     (*(ptr + 5) == '\0')))
                                {
                                   *auth = AUTH_BASIC;
                                   ptr += 5;
                                }
                                /* digest */
                                else if ((*ptr == 'd') && (*(ptr + 1) == 'i') &&
                                         (*(ptr + 2) == 'g') &&
                                         (*(ptr + 3) == 'e') &&
                                         (*(ptr + 4) == 's') &&
                                         (*(ptr + 5) == 't') &&
                                         ((*(ptr + 6) == '\0') ||
                                          (*(ptr + 6) == ';') ||
                                          (*(ptr + 6) == ' ') ||
                                          (*(ptr + 6) == '\0')))
                                     {
                                        *auth = AUTH_DIGEST;
                                        ptr += 6;
                                     }
                                     /* aws4-hmac-sha256 */
                                else if ((*ptr == 'a') &&
                                         (*(ptr + 1) == 'w') &&
                                         (*(ptr + 2) == 's') &&
                                         (*(ptr + 3) == '4') &&
                                         (*(ptr + 4) == '-') &&
                                         (*(ptr + 5) == 'h') &&
                                         (*(ptr + 6) == 'm') &&
                                         (*(ptr + 7) == 'a') &&
                                         (*(ptr + 8) == 'c') &&
                                         (*(ptr + 9) == '-') &&
                                         (*(ptr + 10) == 's') &&
                                         (*(ptr + 11) == 'h') &&
                                         (*(ptr + 12) == 'a') &&
                                         (*(ptr + 13) == '2') &&
                                         (*(ptr + 14) == '5') &&
                                         (*(ptr + 15) == '6') &&
                                         ((*(ptr + 16) == ';') ||
                                          (*(ptr + 16) == ' ') ||
                                          (*(ptr + 16) == '\0')))
                                     {
                                        *auth = AUTH_AWS4_HMAC_SHA256;
                                        ptr += 16;
                                     }
                                     /* aws-no-sign-request */
                                else if ((*ptr == 'a') &&
                                         (*(ptr + 1) == 'w') &&
                                         (*(ptr + 2) == 's') &&
                                         (*(ptr + 3) == '-') &&
                                         (*(ptr + 4) == 'n') &&
                                         (*(ptr + 5) == 'o') &&
                                         (*(ptr + 6) == '-') &&
                                         (*(ptr + 7) == 's') &&
                                         (*(ptr + 8) == 'i') &&
                                         (*(ptr + 9) == 'g') &&
                                         (*(ptr + 10) == 'n') &&
                                         (*(ptr + 11) == '-') &&
                                         (*(ptr + 12) == 'r') &&
                                         (*(ptr + 13) == 'e') &&
                                         (*(ptr + 14) == 'q') &&
                                         (*(ptr + 15) == 'u') &&
                                         (*(ptr + 16) == 'e') &&
                                         (*(ptr + 17) == 's') &&
                                         (*(ptr + 18) == 't') &&
                                         ((*(ptr + 19) == ';') ||
                                          (*(ptr + 19) == ' ') ||
                                          (*(ptr + 19) == '\0')))
                                     {
                                        *auth = AUTH_AWS_NO_SIGN_REQUEST;
                                        ptr += 19;
                                     }
                                     else
                                     {
                                        url_error |= URL_UNKNOWN_VALUE;
                                        *auth = AUTH_BASIC;
                                        while ((*ptr != '\0') && (*ptr != ' ') &&
                                               (*ptr != '\t') && (*ptr != ';'))
                                        {
                                           if (*ptr == '\\')
                                           {
                                              ptr++;
                                           }
                                           ptr++;
                                        }
                                     }

                                todo &= ~URL_GET_AUTH;
                             }
                             /* region= */
                        else if ((region != NULL) && (i == 6) &&
                                 (*(ptr - 1) == 'n') &&
                                 (*(ptr - 2) == 'o') &&
                                 (*(ptr - 3) == 'i') &&
                                 (*(ptr - 4) == 'g') &&
                                 (*(ptr - 5) == 'e') &&
                                 (*(ptr - 6) == 'r'))
                             {
                                ptr++;
                                i = 0;
                                while ((*ptr != '\0') && (*ptr != ' ') &&
                                       (*ptr != '\t') && (*ptr != ';') &&
                                       (i < MAX_REAL_HOSTNAME_LENGTH))
                                {
                                   region[i] = *ptr;
                                   i++; ptr++;
                                }
                                if (i >= MAX_REAL_HOSTNAME_LENGTH)
                                {
                                   url_error |= REGION_NAME_TO_LONG;
                                   region[0] = '\0';
                                   while ((*ptr != '\0') && (*ptr != ' ') &&
                                          (*ptr != '\t') && (*ptr != ';'))
                                   {
                                      if (*ptr == '\\')
                                      {
                                         ptr++;
                                      }
                                      ptr++;
                                   }
                                }
                                else
                                {
                                   region[i] = '\0';
                                   if (i == 0)
                                   {
                                      url_error |= PARAMETER_MISSING;
                                   }
                                }

                                todo &= ~URL_GET_REGION;
                             }
                             /* service= */
                        else if ((service != NULL) && (i == 7) &&
                                 (*(ptr - 1) == 'e') &&
                                 (*(ptr - 2) == 'c') &&
                                 (*(ptr - 3) == 'i') &&
                                 (*(ptr - 4) == 'v') &&
                                 (*(ptr - 5) == 'r') &&
                                 (*(ptr - 6) == 'e') &&
                                 (*(ptr - 7) == 's'))
                             {
                                ptr++;
                                /* s3 */
                                if ((*ptr == 's') && (*(ptr + 1) == '3') &&
                                    ((*(ptr + 2) == ';') ||
                                     (*(ptr + 2) == ' ') ||
                                     (*(ptr + 2) == '\0')))
                                {
                                   *service = SERVICE_S3;
                                   ptr += 2;
                                }
                                else
                                {
                                   url_error |= URL_UNKNOWN_VALUE;
                                   *service = SERVICE_NONE;
                                   while ((*ptr != '\0') && (*ptr != ' ') &&
                                          (*ptr != '\t') && (*ptr != ';'))
                                   {
                                      if (*ptr == '\\')
                                      {
                                         ptr++;
                                      }
                                      ptr++;
                                   }
                                }

                                todo &= ~URL_GET_SERVICE;
                             }
                             /* protocol= */
                        else if ((protocol_version != NULL) && (i == 8) &&
                                 (*(ptr - 1) == 'l') &&
                                 (*(ptr - 2) == 'o') &&
                                 (*(ptr - 3) == 'c') &&
                                 (*(ptr - 4) == 'o') &&
                                 (*(ptr - 5) == 't') &&
                                 (*(ptr - 6) == 'o') &&
                                 (*(ptr - 7) == 'r') &&
                                 (*(ptr - 8) == 'p'))
                             {
                                char str_number[MAX_INT_LENGTH];

                                ptr++;
                                i = 0;
                                while ((*ptr != '\0') && (*ptr != ' ') &&
                                       (*ptr != '\t') &&
                                       (i < MAX_INT_LENGTH))
                                {
                                   str_number[i] = *ptr;
                                   i++; ptr++;
                                }
                                if (i >= MAX_INT_LENGTH)
                                {
                                   url_error |= PROTOCOL_VERSION_TO_LONG;
                                   *protocol_version = 0;
                                }
                                else if (i == 0)
                                     {
                                        url_error |= NO_PROTOCOL_VERSION;
                                     }
                                     else
                                     {
                                        str_number[i] = '\0';
                                        *protocol_version = (unsigned char)atoi(str_number);
                                     }
                                todo &= ~URL_GET_PROTOCOL_VERSION;
                             }
                     }
                  }
               }
            }
         }
      }
      else
      {
         url_error = NOT_A_URL;
      }
   }

   return(url_error);
}


/*$$$$$$$$$$$$$$$$$$$$$$$$ url_insert_password() $$$$$$$$$$$$$$$$$$$$$$$$*/
void
url_insert_password(char *url, char *password)
{
   char *ptr;

   ptr = url;

   /*
    * We only need to insert password for the following schemes:
    *   ftp ftps ftpS sftp scp http https
    */
       /* FTP_SHEME : ftp */
   if (((*ptr == 'f') && (*(ptr + 1) == 't') && (*(ptr + 2) == 'p') &&
#ifdef WITH_SSL
        ((*(ptr + 3) == ':') ||
         (((*(ptr + 3) == 's') || (*(ptr + 3) == 'S')) &&
         (*(ptr + 4) == ':')))) ||
#else
        (*(ptr + 3) == ':')) ||
#endif
       /* SMTP_SHEME : mailto */
       ((*ptr == 'm') && (*(ptr + 1) == 'a') && (*(ptr + 2) == 'i') &&
        (*(ptr + 3) == 'l') && (*(ptr + 4) == 't') && (*(ptr + 5) == 'o') &&
        (*(ptr + 6) == ':')) ||
#ifdef _WITH_DE_MAIL_SUPPORT
       /* DEMAIL_SHEME : demail */
       ((*ptr == 'd') && (*(ptr + 1) == 'e') && (*(ptr + 2) == 'm') &&
        (*(ptr + 3) == 'a') && (*(ptr + 4) == 'i') && (*(ptr + 5) == 'l') &&
        (*(ptr + 6) == ':')) ||
#endif
       /* SFTP_SHEME : sftp */
       ((*ptr == 's') && (*(ptr + 1) == 'f') && (*(ptr + 2) == 't') &&
        (*(ptr + 3) == 'p') && (*(ptr + 4) == ':')) ||
#ifdef _WITH_SCP_SUPPORT
       /* SCP_SHEME : scp */
       ((*ptr == 's') && (*(ptr + 1) == 'c') && (*(ptr + 2) == 'p') &&
        (*(ptr + 3) == ':')) ||
#endif
       /* HTTP_SHEME : http */
       ((*ptr == 'h') && (*(ptr + 1) == 't') && (*(ptr + 2) == 't') &&
        (*(ptr + 3) == 'p') &&
#ifdef WITH_SSL
        ((*(ptr + 4) == ':') || ((*(ptr + 4) == 's') && (*(ptr + 5) == ':')))))
#else
        (*(ptr + 4) == ':')))
#endif
   {
      ptr += 3;
      while ((*ptr != ':') && (*ptr != '\0'))
      {
         ptr++;
      }
      if ((*ptr == ':') && (*(ptr + 1) == '/') && (*(ptr + 2) == '/'))
      {
         int  uh_name_length = 0;
         char *p_start_pwd,
              uh_name[MAX_USER_NAME_LENGTH + MAX_REAL_HOSTNAME_LENGTH + 1];

         ptr += 3; /* Away with '://'. */

         if (password != NULL)
         {
            while ((*ptr != ':') && (*ptr != ';') && (*ptr != '@') &&
                   (*ptr != '/') && (*ptr != '\0'))
            {
               if (*ptr == '\\')
               {
                  ptr++;
               }
               ptr++;
            }
         }
         else
         {
            while ((*ptr != ':') && (*ptr != ';') && (*ptr != '@') &&
                   (*ptr != '/') && (*ptr != '\0') &&
                   (uh_name_length < (MAX_USER_NAME_LENGTH - 1)))
            {
               if (*ptr == '\\')
               {
                  ptr++;
               }
               else if (*ptr == '%')
                    {
                       int value = -1;

                       ptr++;
                       if ((*ptr >= '0') && (*ptr <= '9'))
                       {
                          value = (*ptr - '0') * 16;
                       }
                       else if ((*ptr >= 'a') && (*ptr <= 'f'))
                            {
                               value = ((*ptr - 'a') + 10) * 16;
                            }
                       else if ((*ptr >= 'A') && (*ptr <= 'F'))
                            {
                               value = ((*ptr - 'A') + 10) * 16;
                            }
                       if (value != -1)
                       {
                          ptr++;
                          if ((*ptr >= '0') && (*ptr <= '9'))
                          {
                             value += (*ptr - '0');
                             uh_name[uh_name_length] = value;
                             ptr++; uh_name_length++;
                             continue;
                          }
                          else if ((*ptr >= 'a') && (*ptr <= 'f'))
                               {
                                  value += ((*ptr - 'a') + 10);
                                  uh_name[uh_name_length] = value;
                                  ptr++; uh_name_length++;
                                  continue;
                               }
                          else if ((*ptr >= 'A') && (*ptr <= 'F'))
                               {
                                  value += ((*ptr - 'A') + 10);
                                  uh_name[uh_name_length] = value;
                                  ptr++; uh_name_length++;
                                  continue;
                               }
                               else
                               {
                                  uh_name[uh_name_length] = '%';
                                  uh_name[uh_name_length + 1] = *(ptr - 1);
                                  uh_name_length += 2;
                               }
                       }
                       else
                       {
                          uh_name[uh_name_length] = '%';
                          uh_name_length++;
                       }
                    }
               uh_name[uh_name_length] = *ptr;
               ptr++; uh_name_length++;
            }
         }

         /* Either SSH host key fingerprint or SMTP AUTH. */
         if (*ptr == ';')
         {
            ptr++;
            if (password != NULL)
            {
               while ((*ptr != ':') && (*ptr != '@') && (*ptr != '\0'))
               {
                  if (*ptr == '\\')
                  {
                     ptr++;
                  }
                  ptr++;
               }
            }
            else
            {
               if (((*ptr == 'a') || (*ptr == 'A')) &&
                   ((*(ptr + 1) == 'u') || (*(ptr + 1) == 'U')) &&
                   ((*(ptr + 2) == 't') || (*(ptr + 2) == 'T')) &&
                   ((*(ptr + 3) == 'h') || (*(ptr + 3) == 'H')) &&
                   (*(ptr + 4) == '='))
               {
                  ptr += 5;

                  /* Either login or plain. */
                  if ((((*ptr == 'l') || (*ptr == 'L')) &&
                       ((*(ptr + 1) == 'o') || (*(ptr + 1) == 'O')) &&
                       ((*(ptr + 2) == 'g') || (*(ptr + 2) == 'G')) &&
                       ((*(ptr + 3) == 'i') || (*(ptr + 3) == 'I')) &&
                       ((*(ptr + 4) == 'n') || (*(ptr + 4) == 'N')) &&
                       (*(ptr + 5) == ';')) ||
                      (((*ptr == 'p') || (*ptr == 'P')) &&
                       ((*(ptr + 1) == 'l') || (*(ptr + 1) == 'L')) &&
                       ((*(ptr + 2) == 'a') || (*(ptr + 2) == 'A')) &&
                       ((*(ptr + 3) == 'i') || (*(ptr + 3) == 'I')) &&
                       ((*(ptr + 4) == 'n') || (*(ptr + 4) == 'N')) &&
                       (*(ptr + 5) == ';')))
                  {
                     ptr += 6;

                     if (((*ptr == 'u') || (*ptr == 'U')) &&
                         ((*(ptr + 1) == 's') || (*(ptr + 1) == 'S')) &&
                         ((*(ptr + 2) == 'e') || (*(ptr + 2) == 'E')) &&
                         ((*(ptr + 3) == 'r') || (*(ptr + 3) == 'R')) &&
                         (*(ptr + 4) == '='))
                     {
                        ptr += 5;

                        uh_name_length = 0;
                        while ((*ptr != ':') && (*ptr != ';') &&
                               (*ptr != '@') && (*ptr != '/') &&
                               (*ptr != '\0') &&
                               (uh_name_length < (MAX_USER_NAME_LENGTH - 1)))
                        {
                           if (*ptr == '\\')
                           {
                              ptr++;
                           }
                           else if (*ptr == '%')
                                {
                                   int value = -1;

                                   ptr++;
                                   if ((*ptr >= '0') && (*ptr <= '9'))
                                   {
                                      value = (*ptr - '0') * 16;
                                   }
                                   else if ((*ptr >= 'a') && (*ptr <= 'f'))
                                        {
                                           value = ((*ptr - 'a') + 10) * 16;
                                        }
                                   else if ((*ptr >= 'A') && (*ptr <= 'F'))
                                        {
                                           value = ((*ptr - 'A') + 10) * 16;
                                        }
                                   if (value != -1)
                                   {
                                      ptr++;
                                      if ((*ptr >= '0') && (*ptr <= '9'))
                                      {
                                         value += (*ptr - '0');
                                         uh_name[uh_name_length] = value;
                                         ptr++; uh_name_length++;
                                         continue;
                                      }
                                      else if ((*ptr >= 'a') && (*ptr <= 'f'))
                                           {
                                              value += ((*ptr - 'a') + 10);
                                              uh_name[uh_name_length] = value;
                                              ptr++; uh_name_length++;
                                              continue;
                                           }
                                      else if ((*ptr >= 'A') && (*ptr <= 'F'))
                                           {
                                              value += ((*ptr - 'A') + 10);
                                              uh_name[uh_name_length] = value;
                                              ptr++; uh_name_length++;
                                              continue;
                                           }
                                           else
                                           {
                                              uh_name[uh_name_length] = '%';
                                              uh_name[uh_name_length + 1] = *(ptr - 1);
                                              uh_name_length += 2;
                                           }
                                   }
                                   else
                                   {
                                      uh_name[uh_name_length] = '%';
                                      uh_name_length++;
                                   }
                                }
                           uh_name[uh_name_length] = *ptr;
                           ptr++; uh_name_length++;
                        }
                     }
                     else
                     {
                        while ((*ptr != ':') && (*ptr != '@') &&
                               (*ptr != ';') && (*ptr != '\0'))
                        {
                           if (*ptr == '\\')
                           {
                              ptr++;
                           }
                           ptr++;
                        }
                     }
                  }
                  else
                  {
                     while ((*ptr != ':') && (*ptr != '@') && (*ptr != ';') &&
                            (*ptr != '\0'))
                     {
                        if (*ptr == '\\')
                        {
                           ptr++;
                        }
                        ptr++;
                     }
                  }
               }
               else
               {
                  while ((*ptr != ':') && (*ptr != '@') && (*ptr != ';') &&
                         (*ptr != '\0'))
                  {
                     if (*ptr == '\\')
                     {
                        ptr++;
                     }
                     ptr++;
                  }
               }
            }
         }

         /* Remove any existing password. */
         p_start_pwd = ptr;
         if (*ptr == ':')
         {
            ptr++; /* Away with : */

            while ((*ptr != '@') && (*ptr != '/') && (*ptr != '\0'))
            {
               if (*ptr == '\\')
               {
                  ptr++;
               }
               ptr++;
            }

            if ((*ptr == '@') && ((p_start_pwd + 1) != ptr))
            {
               /* Remove the password from url. */
               (void)memmove(p_start_pwd, ptr, strlen(ptr) + 1);
               ptr = p_start_pwd;
            }
         }

         if (*ptr == '@')
         {
            char local_password[MAX_USER_NAME_LENGTH + 1],
                 *p_password;

            if (password == NULL)
            {
               int tmp_uh_name_length = uh_name_length;

               ptr++;
               while ((*ptr != '\0') && (*ptr != '/') && (*ptr != ':') &&
                      (*ptr != ';') &&
                      (uh_name_length < (MAX_USER_NAME_LENGTH + MAX_REAL_HOSTNAME_LENGTH - 1)))
               {
                  if (*ptr == '\\')
                  {
                     ptr++;
                  }
                  else if (*ptr == '+')
                       {
                          uh_name[uh_name_length] = ' ';
                          ptr++; uh_name_length++;
                          continue;
                       }
                  else if (*ptr == '%')
                       {
                          int value = -1;

                          ptr++;
                          if ((*ptr >= '0') && (*ptr <= '9'))
                          {
                             value = (*ptr - '0') * 16;
                          }
                          else if ((*ptr >= 'a') && (*ptr <= 'f'))
                               {
                                  value = ((*ptr - 'a') + 10) * 16;
                               }
                          else if ((*ptr >= 'A') && (*ptr <= 'F'))
                               {
                                  value = ((*ptr - 'A') + 10) * 16;
                               }
                          if (value != -1)
                          {
                             ptr++;
                             if ((*ptr >= '0') && (*ptr <= '9'))
                             {
                                value += (*ptr - '0');
                                uh_name[uh_name_length] = value;
                                ptr++; uh_name_length++;
                                continue;
                             }
                             else if ((*ptr >= 'a') && (*ptr <= 'f'))
                                  {
                                     value += ((*ptr - 'a') + 10);
                                     uh_name[uh_name_length] = value;
                                     ptr++; uh_name_length++;
                                     continue;
                                  }
                             else if ((*ptr >= 'A') && (*ptr <= 'F'))
                                  {
                                     value += ((*ptr - 'A') + 10);
                                     uh_name[uh_name_length] = value;
                                     ptr++; uh_name_length++;
                                     continue;
                                  }
                                  else
                                  {
                                     uh_name[uh_name_length] = '%';
                                     uh_name[uh_name_length + 1] = *(ptr - 1);
                                     uh_name_length += 2;
                                  }
                          }
                          else
                          {
                             uh_name[uh_name_length] = '%';
                             uh_name_length++;
                          }
                       }
                  uh_name[uh_name_length] = *ptr;
                  uh_name_length++; ptr++;
               }
               while ((*ptr != '\0') && (*ptr != ';'))
               {
                  ptr++;
               }
               if ((*ptr == ';') && (*(ptr + 1) == 's') &&
                   (*(ptr + 2) == 'e') && (*(ptr + 3) == 'r') &&
                   (*(ptr + 4) == 'v') && (*(ptr + 5) == 'e') &&
                   (*(ptr + 6) == 'r') && (*(ptr + 7) == '='))
               {
                  ptr += 8;
                  uh_name_length = tmp_uh_name_length;
                  while ((*ptr != '\0') &&
                         (uh_name_length < (MAX_USER_NAME_LENGTH + MAX_REAL_HOSTNAME_LENGTH - 1)))
                  {
                     if (*ptr == '\\')
                     {
                        ptr++;
                     }
                     uh_name[uh_name_length] = *ptr;
                     uh_name_length++; ptr++;
                  }
               }
               uh_name[uh_name_length] = '\0';
               (void)get_pw(uh_name, local_password, YES);
               p_password = local_password;
            }
            else
            {
               p_password = password;
            }
            if (*p_password != '\0')
            {
               char tmp_buffer[MAX_RECIPIENT_LENGTH + 1];

               (void)strcpy(tmp_buffer, p_start_pwd);
               *p_start_pwd = ':';
               *(p_start_pwd + 1) = *p_password;
               p_start_pwd += 2;
               p_password++;
               while (*p_password != '\0')
               {
                  if ((*p_password == '@') || (*p_password == ':') ||
                      (*p_password == ';'))
                  {
                     *p_start_pwd = '\\';
                     p_start_pwd++;
                  }
                  *p_start_pwd = *p_password;
                  p_start_pwd++; p_password++;
               }
               (void)strcpy(p_start_pwd, tmp_buffer);
            }
         }
      }
   }

   return;
}


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ url_compare() $$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
url_compare(char *url1, char *url2)
{
   unsigned int  scheme1,
                 scheme2;
   int           port1,
                 port2,
                 ret;
   char          user1[MAX_USER_NAME_LENGTH + 1],
                 user2[MAX_USER_NAME_LENGTH + 1],
                 smtp_user1[MAX_USER_NAME_LENGTH + 1],
                 smtp_user2[MAX_USER_NAME_LENGTH + 1],
#ifdef WITH_SSH_FINGERPRINT
                 fingerprint1[MAX_FINGERPRINT_LENGTH + 1],
                 fingerprint2[MAX_FINGERPRINT_LENGTH + 1],
                 key_type1,
                 key_type2,
#endif
                 password1[MAX_USER_NAME_LENGTH + 1],
                 password2[MAX_USER_NAME_LENGTH + 1],
                 hostname1[MAX_REAL_HOSTNAME_LENGTH + 1],
                 hostname2[MAX_REAL_HOSTNAME_LENGTH + 1],
                 path1[MAX_RECIPIENT_LENGTH + 1],
                 path2[MAX_RECIPIENT_LENGTH + 1],
                 transfer_type1,
                 transfer_type2,
                 region1[MAX_REAL_HOSTNAME_LENGTH + 1],
                 region2[MAX_REAL_HOSTNAME_LENGTH + 1],
                 server1[MAX_REAL_HOSTNAME_LENGTH + 1],
                 server2[MAX_REAL_HOSTNAME_LENGTH + 1];
   unsigned char protocol_version1,
                 protocol_version2,
                 smtp_auth1,
                 smtp_auth2,
                 auth1,
                 auth2,
                 service1,
                 service2;

   if ((url_evaluate(url1, &scheme1, user1,
                     &smtp_auth1, smtp_user1,
#ifdef WITH_SSH_FINGERPRINT
                     fingerprint1, &key_type1,
#endif
                     password1, NO, hostname1, &port1, path1, NULL, NULL,
                     &transfer_type1, &protocol_version1, &auth1,
                     region1, &service1, server1) == 0) &&
       (url_evaluate(url2, &scheme2, user2,
                     &smtp_auth2, smtp_user2,
#ifdef WITH_SSH_FINGERPRINT
                     fingerprint2, &key_type2,
#endif
                     password2, NO, hostname2, &port2, path2, NULL, NULL,
                     &transfer_type2, &protocol_version2, &auth2,
                     region2, &service2, server2) < 4))
   {
      ret = 0;

      if (scheme1 != scheme2)
      {
         ret |= URL_SCHEME_DIFS;
      }
      if (port1 != port2)
      {
         ret |= URL_PORT_DIFS;
      }
      if (transfer_type1 != transfer_type2)
      {
         ret |= URL_TRANSFER_TYPE_DIFS;
      }
      if (protocol_version1 != protocol_version2)
      {
         ret |= URL_PROTOCOL_VERSION_DIFS;
      }
      if (smtp_auth1 != smtp_auth2)
      {
         ret |= URL_SMTP_AUTH_DIFS;
      }
      if (auth1 != auth2)
      {
         ret |= URL_AUTH_DIFS;
      }
      if (service1 != service2)
      {
         ret |= URL_SERVICE_DIFS;
      }
      if (CHECK_STRCMP(user1, user2))
      {
         ret |= URL_USER_DIFS;
      }
      if (CHECK_STRCMP(smtp_user1, smtp_user2))
      {
         ret |= URL_SMTP_USER_DIFS;
      }
      if (CHECK_STRCMP(password1, password2))
      {
         ret |= URL_PASSWORD_DIFS;
      }
      if (CHECK_STRCMP(hostname1, hostname2))
      {
         ret |= URL_HOSTNAME_DIFS;
      }
      if (CHECK_STRCMP(path1, path2))
      {
         ret |= URL_PATH_DIFS;
      }
      if (CHECK_STRCMP(region1, region2))
      {
         ret |= URL_REGION_DIFS;
      }
      if (CHECK_STRCMP(server1, server2))
      {
         ret |= URL_SERVER_DIFS;
      }
#ifdef WITH_SSH_FINGERPRINT
      if (key_type1 != key_type2)
      {
         ret |= URL_KEYTYPE_DIFS;
      }
      if (CHECK_STRCMP(fingerprint1, fingerprint2))
      {
         ret |= URL_FINGERPRINT_DIFS;
      }
#endif
   }
   else
   {
      ret = -1;
   }

   return(ret);
}


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$ url_get_error() $$$$$$$$$$$$$$$$$$$$$$$$$$$*/
void
url_get_error(int error_mask, char *error_str, int error_str_length)
{
   if (error_str_length)
   {
      if ((error_str_length > 18) && (error_mask & NOT_A_URL))
      {
         (void)strcpy(error_str, "could not find ://");
      }
      else
      {
         int length = 0,
             tlen;

         error_str[0] = '\0';
         if ((error_str_length > 14) && (error_mask & UNKNOWN_SCHEME))
         {
            (void)strcpy(error_str, "unknown scheme");
            length = 14 + 1;
            error_str_length -= length;
         }
         if ((error_str_length > (35 + MAX_INT_LENGTH)) &&
             (error_mask & USER_NAME_TO_LONG))
         {
            if (length)
            {
               tlen = snprintf(&error_str[length], error_str_length,
                               ", user name may only be %d bytes long",
                               MAX_USER_NAME_LENGTH);
               if (tlen > error_str_length)
               {
                  length = error_str_length;
               }
               else
               {
                  length += tlen;
               }
            }
            else
            {
               length = snprintf(error_str, error_str_length,
                                 "user name may only be %d bytes long",
                                 MAX_USER_NAME_LENGTH);
               if (length > error_str_length)
               {
                  length = error_str_length;
               }
            }
            error_str_length -= length;
         }
         if ((error_str_length > 29) && (error_mask & UNKNOWN_SMTP_AUTH))
         {
            if (length)
            {
               (void)strcpy(&error_str[length], ", unknown SMTP authentication");
               length += (29 + 1);
            }
            else
            {
               (void)strcpy(error_str, "unknown SMTP authentication");
               length = 27 + 1;
            }
            error_str_length -= length;
         }
#ifdef WITH_SSH_FINGERPRINT
         if ((error_str_length > 18) && (error_mask & UNKNOWN_KEY_TYPE))
         {
            if (length)
            {
               (void)strcpy(&error_str[length], ", unknown key type");
               length += (18 + 1);
            }
            else
            {
               (void)strcpy(error_str, "unknown key type");
               length = 16 + 1;
            }
            error_str_length -= length;
         }
         if ((error_str_length > 21) && (error_mask & NOT_A_FINGERPRINT))
         {
            if (length)
            {
               (void)strcpy(&error_str[length], ", invalid fingerprint");
               length += (21 + 1);
            }
            else
            {
               (void)strcpy(error_str, "invalid fingerprint");
               length = 19 + 1;
            }
            error_str_length -= length;
         }
         if ((error_str_length > 48) && (error_mask & ONLY_FINGERPRINT_KNOWN))
         {
            if (length)
            {
               (void)strcpy(&error_str[length], ", only known parameter after user is fingerprint");
               length += (48 + 1);
            }
            else
            {
               (void)strcpy(error_str, "only known parameter after user is fingerprint");
               length = 46 + 1;
            }
            error_str_length -= length;
         }
#endif
         if ((error_str_length > (34 + MAX_INT_LENGTH)) &&
             (error_mask & PASSWORD_TO_LONG))
         {
            if (length)
            {
               tlen = snprintf(&error_str[length], error_str_length,
                               ", password may only be %d bytes long",
                               MAX_USER_NAME_LENGTH);
               if (tlen > error_str_length)
               {
                  length = error_str_length;
               }
               else
               {
                  length += tlen;
               }
            }
            else
            {
               length = snprintf(error_str, error_str_length,
                                 "password may only be %d bytes long",
                                 MAX_USER_NAME_LENGTH);
               if (length > error_str_length)
               {
                  length = error_str_length;
               }
            }
            error_str_length -= length;
         }
         if ((error_str_length > (34 + MAX_INT_LENGTH)) &&
             (error_mask & HOSTNAME_TO_LONG))
         {
            if (length)
            {
               tlen = snprintf(&error_str[length], error_str_length,
                               ", hostname may only be %d bytes long",
                               MAX_REAL_HOSTNAME_LENGTH);
               if (tlen > error_str_length)
               {
                  length = error_str_length;
               }
               else
               {
                  length += tlen;
               }
            }
            else
            {
               length = snprintf(error_str, error_str_length,
                                 "hostname may only be %d bytes long",
                                 MAX_REAL_HOSTNAME_LENGTH);
               if (length > error_str_length)
               {
                  length = error_str_length;
               }
            }
            error_str_length -= length;
         }
         if ((error_str_length > (37 + MAX_INT_LENGTH)) &&
             (error_mask & PORT_TO_LONG))
         {
            if (length)
            {
               tlen = snprintf(&error_str[length], error_str_length,
                               ", port number may only be %d bytes long",
                               MAX_INT_LENGTH);
               if (tlen > error_str_length)
               {
                  length = error_str_length;
               }
               else
               {
                  length += tlen;
               }
            }
            else
            {
               length = snprintf(error_str, error_str_length,
                                 "port number may only be %d bytes long",
                                 MAX_INT_LENGTH);
               if (length > error_str_length)
               {
                  length = error_str_length;
               }
            }
            error_str_length -= length;
         }
         if ((error_str_length > (47 + MAX_INT_LENGTH)) &&
             (error_mask & TIME_MODIFIER_TO_LONG))
         {
            if (length)
            {
               tlen = snprintf(&error_str[length], error_str_length,
                               ", time modifier in path may only be %d bytes long",
                               MAX_INT_LENGTH);
               if (tlen > error_str_length)
               {
                  length = error_str_length;
               }
               else
               {
                  length += tlen;
               }
            }
            else
            {
               length = snprintf(error_str, error_str_length,
                                 "time modifier in path may only be %d bytes long",
                                 MAX_INT_LENGTH);
               if (length > error_str_length)
               {
                  length = error_str_length;
               }
            }
            error_str_length -= length;
         }
         if ((error_str_length > 34) &&
             (error_mask & NO_TIME_MODIFIER_SPECIFIED))
         {
            if (length)
            {
               (void)strcpy(&error_str[length], ", time modifier in path is missing");
               length += (34 + 1);
            }
            else
            {
               (void)strcpy(error_str, "time modifier in path is missing");
               length = 31 + 1;
            }
            error_str_length -= length;
         }
         if ((error_str_length > (35 + MAX_INT_LENGTH)) &&
             (error_mask & RECIPIENT_TO_LONG))
         {
            if (length)
            {
               tlen = snprintf(&error_str[length], error_str_length,
                               ", recipient may only be %d bytes long",
                               MAX_RECIPIENT_LENGTH);
               if (tlen > error_str_length)
               {
                  length = error_str_length;
               }
               else
               {
                  length += tlen;
               }
            }
            else
            {
               length = snprintf(error_str, error_str_length,
                                 "path may only be %d bytes long",
                                 MAX_RECIPIENT_LENGTH);
               if (length > error_str_length)
               {
                  length = error_str_length;
               }
            }
            error_str_length -= length;
         }
         if ((error_str_length > 23) && (error_mask & UNKNOWN_TRANSFER_TYPE))
         {
            if (length)
            {
               (void)strcpy(&error_str[length], ", unknown transfer type");
               length += (23 + 1);
            }
            else
            {
               (void)strcpy(error_str, "unknown transfer type");
               length = 21 + 1;
            }
            error_str_length -= length;
         }
         if ((error_str_length > 28) && (error_mask & TARGET_DIR_CAN_CHANGE))
         {
            if (length)
            {
               (void)strcpy(&error_str[length], ", no error - path can change");
               length += (28 + 1);
            }
            else
            {
               (void)strcpy(error_str, "no error - path can change");
               length = 26 + 1;
            }
            error_str_length -= length;
         }
         if ((error_str_length > (42 + MAX_INT_LENGTH)) &&
             (error_mask & PROTOCOL_VERSION_TO_LONG))
         {
            if (length)
            {
               tlen = snprintf(&error_str[length], error_str_length,
                               ", protocol version may only be %d bytes long",
                               MAX_INT_LENGTH);
               if (tlen > error_str_length)
               {
                  length = error_str_length;
               }
               else
               {
                  length += tlen;
               }
            }
            else
            {
               length = snprintf(error_str, error_str_length,
                                 "protocol version may only be %d bytes long",
                                 MAX_INT_LENGTH);
               if (length > error_str_length)
               {
                  length = error_str_length;
               }
            }
            error_str_length -= length;
         }
         if ((error_str_length > 30) && (error_mask & NO_PROTOCOL_VERSION))
         {
            if (length)
            {
               (void)strcpy(&error_str[length], ", no protocol version supplied");
               length += (30 + 1);
            }
            else
            {
               (void)strcpy(error_str, "no protocol version supplied");
               length = 28 + 1;
            }
            error_str_length -= length;
         }
         if ((error_str_length > 25) && (error_mask & NO_PORT_SPECIFIED))
         {
            if (length)
            {
               (void)strcpy(&error_str[length], ", no port number supplied");
               length += (25 + 1);
            }
            else
            {
               (void)strcpy(error_str, "no port number supplied");
               length = 23 + 1;
            }
            error_str_length -= length;
         }
         if ((error_str_length > (37 + MAX_INT_LENGTH)) &&
             (error_mask & SERVER_NAME_TO_LONG))
         {
            if (length)
            {
               tlen = snprintf(&error_str[length], error_str_length,
                               ", server name may only be %d bytes long",
                               MAX_REAL_HOSTNAME_LENGTH);
               if (tlen > error_str_length)
               {
                  length = error_str_length;
               }
               else
               {
                  length += tlen;
               }
            }
            else
            {
               length = snprintf(error_str, error_str_length,
                                 "server name may only be %d bytes long",
                                 MAX_REAL_HOSTNAME_LENGTH);
               if (length > error_str_length)
               {
                  length = error_str_length;
               }
            }
            error_str_length -= length;
         }
         if ((error_str_length > 29) && (error_mask & EXEC_CMD_ERROR))
         {
            if (length)
            {
               (void)strcpy(&error_str[length], ", execution of command failed");
               length += (29 + 1);
            }
            else
            {
               (void)strcpy(error_str, "execution of command failed");
               length = 27 + 1;
            }
            error_str_length -= length;
         }
         if ((error_str_length > 25) && (error_mask & EXEC_NO_RETURN))
         {
            if (length)
            {
               (void)strcpy(&error_str[length], ", execution returned NULL");
               length += (25 + 1);
            }
            else
            {
               (void)strcpy(error_str, "execution returned NULL");
               length = 23 + 1;
            }
            error_str_length -= length;
         }
         if ((error_str_length > 43) && (error_mask & EXEC_NOT_TERMINATED))
         {
            if (length)
            {
               (void)strcpy(&error_str[length], ", exec command (%%e) not terminated with a `");
               length += (43 + 1);
            }
            else
            {
               (void)strcpy(error_str, "exec command (%%e) not terminated with a `");
               length = 41 + 1;
            }
            error_str_length -= length;
         }
         if ((error_str_length > (52 + MAX_INT_LENGTH)) &&
             (error_mask & EXEC_CMD_TO_LONG))
         {
            if (length)
            {
               tlen = snprintf(&error_str[length], error_str_length,
                               ", exec command (%%e) to long, may only be %d bytes long",
                               MAX_RECIPIENT_LENGTH);
               if (tlen > error_str_length)
               {
                  length = error_str_length;
               }
            }
            else
            {
               length = snprintf(error_str, error_str_length,
                                 "exec command (%%e) to long, may only be %d bytes long",
                                 MAX_RECIPIENT_LENGTH);
               if (length > error_str_length)
               {
                  length = error_str_length;
               }
            }
            error_str_length -= length;
         }
         if ((error_str_length > (37 + MAX_INT_LENGTH)) &&
             (error_mask & REGION_NAME_TO_LONG))
         {
            if (length)
            {
               tlen = snprintf(&error_str[length], error_str_length,
                               ", region name may only be %d bytes long",
                               MAX_REAL_HOSTNAME_LENGTH);
               if (tlen > error_str_length)
               {
                  length = error_str_length;
               }
               else
               {
                  length += tlen;
               }
            }
            else
            {
               length = snprintf(error_str, error_str_length,
                                 "region name may only be %d bytes long",
                                 MAX_REAL_HOSTNAME_LENGTH);
               if (length > error_str_length)
               {
                  length = error_str_length;
               }
            }
            error_str_length -= length;
         }
         if ((error_str_length > 19) && (error_mask & PARAMETER_MISSING))
         {
            if (length)
            {
               tlen = snprintf(&error_str[length], error_str_length,
                               ", parameter missing");
               if (tlen > error_str_length)
               {
                  length = error_str_length;
               }
               else
               {
                  length += tlen;
               }
            }
            else
            {
               length = snprintf(error_str, error_str_length,
                                 "parameter missing");
               if (length > error_str_length)
               {
                  length = error_str_length;
               }
            }
            error_str_length -= length;
         }
         if ((error_str_length > 15) && (error_mask & URL_UNKNOWN_VALUE))
         {
            if (length)
            {
               tlen = snprintf(&error_str[length], error_str_length,
                               ", unknown value");
               if (tlen > error_str_length)
               {
                  length = error_str_length;
               }
               else
               {
                  length += tlen;
               }
            }
            else
            {
               length = snprintf(error_str, error_str_length,
                                 "unknown value");
               if (length > error_str_length)
               {
                  length = error_str_length;
               }
            }
            error_str_length -= length;
         }
         if ((error_str_length > 17) && (error_mask & BUFFER_TO_SHORT))
         {
            if (length)
            {
               (void)strcpy(&error_str[length], ", buffer to short");
               error_str_length -= (17 + 1);
            }
            else
            {
               (void)strcpy(error_str, "buffer to short");
               error_str_length -= (15 + 1);
            }
         }
      }
   }

   return;
}


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ url_encode() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
void
url_encode(char *src, char *dst)
{
   char *hex = "0123456789ABCDEF",
        *p_dst = dst,
        *p_src = src;

   while (*p_src != '\0')
   {
      if ((isalnum(*p_src)) || (*p_src == '-') || (*p_src == '_') ||
          (*p_src == '.') || (*p_src == '~'))
      {
         *p_dst++ = *p_src;
      }
      else if (*p_src == ' ')
           {
              *p_dst++ = '+';
           }
           else
           {
              *p_dst = '%';
              if ((unsigned char)*p_src > 15)
              {
                 *(p_dst + 1) = hex[((unsigned char)*p_src) >> 4];
                 *(p_dst + 2) = hex[((unsigned char)*p_src) & 0x0F];
              }
              else
              {
                 *(p_dst + 1) = '0';
                 *(p_dst + 2) = hex[(unsigned char)*p_src];
              }
              p_dst += 3;
           }
      p_src++;
   }
   *p_dst = '\0';

   return;
}


/*$$$$$$$$$$$$$$$$$$$$$$$$$$ url_path_encode() $$$$$$$$$$$$$$$$$$$$$$$$$$*/
void
url_path_encode(char *src, char *dst)
{
   char *hex = "0123456789ABCDEF",
        *p_dst = dst,
        *p_src = src;

   while (*p_src != '\0')
   {
      if ((isalnum(*p_src)) || (*p_src == '-') || (*p_src == '_') ||
          (*p_src == '/') || (*p_src == '.') || (*p_src == '~'))
      {
         *p_dst++ = *p_src;
      }
      else if (*p_src == ' ')
           {
              *p_dst++ = '+';
           }
           else
           {
              *p_dst = '%';
              if ((unsigned char)*p_src > 15)
              {
                 *(p_dst + 1) = hex[((unsigned char)*p_src) >> 4];
                 *(p_dst + 2) = hex[((unsigned char)*p_src) & 0x0F];
              }
              else
              {
                 *(p_dst + 1) = '0';
                 *(p_dst + 2) = hex[(unsigned char)*p_src];
              }
              p_dst += 3;
           }
      p_src++;
   }
   *p_dst = '\0';

   return;
}


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ url_decode() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
void
url_decode(char *src, char *dst)
{
   char *p_src = src,
        *p_dst = dst;

   while (*p_src != '\0')
   {
      if (*p_src == '%')
      {
         if ((p_src[1] != '\0') && (p_src[2] != '\0'))
         {
            *p_dst++ = (isdigit(p_src[1]) ? (p_src[1] - '0') : (tolower(p_src[1]) - 'a' + 10)) << 4 | (isdigit(p_src[2]) ? (p_src[2] - '0') : (tolower(p_src[2]) - 'a' + 10));
            p_src += 2;
         }
      }
      else if (*p_src == '+')
           {
              *p_dst++ = ' ';
           }
           else
           {
              *p_dst++ = *p_src;
           }
      p_src++;
   }
   *p_dst = '\0';

   return;
}


/*+++++++++++++++++++++++++ insert_alias_name() +++++++++++++++++++++++++*/
static int
insert_alias_name(char *p_alias_id, int *offset, char *hostname, int *i)
{
   int  gotcha = NO;
   char *ptr = p_alias_id;

   *offset = 0;
   while ((*ptr != '>') && (*ptr != '\0'))
   {
      if ((*ptr == '%') && (*(ptr + 1) == '3') &&
          ((*(ptr + 2) == 'E') || (*(ptr + 2) == 'e')))
      {
         (*offset) += 2;
         gotcha = YES;
         break;
      }
      ptr++; (*offset)++;
   }
   if ((*ptr == '>') || (gotcha == YES))
   {
      int length = ptr - p_alias_id;

      if (length < MAX_ALIAS_NAME_LENGTH)
      {
         char alias_id_str[MAX_ALIAS_NAME_LENGTH + 1];

         (void)memcpy(alias_id_str, p_alias_id, length);
         alias_id_str[length] = '\0';
         get_alias_names();
         length = search_insert_alias_name(alias_id_str, &hostname[*i],
                                           MAX_REAL_HOSTNAME_LENGTH - *i);
         if (length > 0)
         {
            (*i) += length;
            (*offset)++;
            return(1);
         }
         else
         {
            return(0);
         }
      }
      else
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Alias name (%s) length for may not be longer then %d bytes.",
                    p_alias_id, MAX_ALIAS_NAME_LENGTH);
         return(0);
      }
   }

   return(0);
}
