/*
 *  httpcmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2021 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   httpcmd - commands to send and retrieve files via HTTP
 **
 ** SYNOPSIS
 **   int  http_connect(char *hostname, int port, int sndbuf_size, int rcvbuf_size, int strict)
 **   int  http_ssl_auth(void)
 **   int  http_options(char *host, char *path)
 **   int  http_del(char *host, char *path, char *filename)
 **   int  http_get(char *host, char *path, char *filename, off_t *content_length, off_t offset)
 **   int  http_head(char *host, char *path, char *filename, off_t *content_length, time_t date)
 **   int  http_noop(void)
 **   int  http_put(char *host, char *path, char *filename, off_t length)
 **   int  http_read(char *block, int blocksize)
 **   int  http_chunk_read(char **chunk, int *chunksize)
 **   int  http_version(void)
 **   int  http_write(char *block, char *buffer, int size)
 **   void http_quit(void)
 **
 ** DESCRIPTION
 **   httpcmd provides a set of commands to communicate with an HTTP
 **   server via BSD sockets.
 **
 **
 ** RETURN VALUES
 **   Returns SUCCESS when successful. When an error has occurred
 **   it will either return INCORRECT or the three digit HTTP reply
 **   code when the reply of the server does not conform to the
 **   one expected. The complete reply string of the HTTP server
 **   is returned to 'msg_str'. 'timeout_flag' is just a flag to
 **   indicate that the 'transfer_timeout' time has been reached.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   19.04.2003 H.Kiehl Created
 **   25.12.2003 H.Kiehl Added traceing.
 **   28.04.2004 H.Kiehl Handle chunked reading.
 **   16.08.2006 H.Kiehl Added http_head() function.
 **   17.08.2006 H.Kiehl Added appending to http_get().
 **   12.08.2012 H.Kiehl Use getaddrinfo() instead of gethostname() to
 **                      support IPv6.
 **   11.09.2014 H.Kiehl Added simulation mode.
 **   03.11.2018 H.Kiehl Implemented ServerNameIndication for TLS.
 **   23.03.2021 H.Kiehl Added etag support for http_get().
 */
DESCR__E_M3


#include <stdio.h>        /* fprintf(), fdopen(), fclose()               */
#include <stdarg.h>       /* va_start(), va_arg(), va_end()              */
#include <string.h>       /* memset(), memcpy(), strcpy()                */
#include <stdlib.h>       /* strtol()                                    */
#include <ctype.h>        /* isdigit()                                   */
#include <sys/types.h>    /* fd_set                                      */
#include <sys/time.h>     /* struct timeval                              */
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>  /* socket(), shutdown(), bind(), setsockopt()  */
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>  /* struct in_addr, sockaddr_in, htons()        */
#endif
#if defined (_WITH_TOS) && defined (IP_TOS)
# ifdef HAVE_NETINET_IN_SYSTM_H
#  include <netinet/in_systm.h>
# endif
# ifdef HAVE_NETINET_IP_H
#  include <netinet/ip.h> /* IPTOS_LOWDELAY, IPTOS_THROUGHPUT            */
# endif
#endif
#ifdef HAVE_ARPA_TELNET_H
# include <arpa/telnet.h> /* IAC, SYNCH, IP                              */
#endif
#ifdef HAVE_NETDB_H
# include <netdb.h>       /* struct hostent, gethostbyname()             */
#endif
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>   /* inet_addr()                                 */
#endif
#ifdef WITH_SSL
# include <setjmp.h>      /* sigsetjmp(), siglongjmp()                   */
# include <signal.h>      /* signal()                                    */
# include <openssl/crypto.h>
# include <openssl/x509.h>  
# include <openssl/ssl.h>
#endif
#include <unistd.h>       /* select(), write(), read(), close(), alarm() */
#include <errno.h>
#include "fddefs.h"
#include "httpdefs.h"
#include "commondefs.h"

#ifdef WITH_SSL
SSL                              *ssl_con = NULL;
#endif

/* External global variables. */
extern int                       simulation_mode,
                                 timeout_flag;
#ifdef WITH_IP_DB
extern int                       use_ip_db;
#endif
extern unsigned int              special_flag;
extern char                      msg_str[];
#ifdef LINUX
extern char                      *h_errlist[];  /* for gethostbyname()   */
extern int                       h_nerr;        /* for gethostbyname()   */
#endif
extern long                      transfer_timeout;

/* Local global variables. */
static int                       http_fd;
#ifdef WITH_SSL
static sigjmp_buf                env_alrm;
static SSL_CTX                   *ssl_ctx = NULL;
#endif
static struct timeval            timeout;
static struct http_message_reply hmr;

/* Local function prototypes. */
static int                       basic_authentication(void),
                                 check_connection(void),
                                 flush_read(void),
                                 get_http_reply(int *, int, int),
                                 read_msg(int *, int, int);
static void                      read_last_chunk(void),
#ifdef WITH_SSL
                                 sig_handler(int),
#endif
#ifdef _WITH_EXTRA_CHECK
                                 store_http_etag(int, int),
#endif
                                 store_http_options(int, int);


/*########################## http_connect() #############################*/
int
http_connect(char *hostname,
             char *http_proxy,
             int  port,
             char *user,
             char *passwd,
#ifdef WITH_SSL
             int  ssl,
             int  strict,
#endif
             int  sndbuf_size,
             int  rcvbuf_size)
{
   if (simulation_mode == YES)
   {
      if ((http_fd = open("/dev/null", O_RDWR)) == -1)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", "Simulated http_connect()",
                   _("Failed to open() /dev/null : %s"), strerror(errno));
         return(INCORRECT);
      }
      else
      {
#ifdef WITH_TRACE
         int length;

         length = snprintf(msg_str, MAX_RET_MSG_LENGTH,
#else
         (void)snprintf(msg_str, MAX_RET_MSG_LENGTH,
#endif
                           _("Simulated HTTP connect to %s%s (port=%d)"),
                           (http_proxy[0] == '\0') ? "" : "proxy ",
                           (http_proxy[0] == '\0') ? hostname : http_proxy,
                           port);
#ifdef WITH_TRACE
         if (length > MAX_RET_MSG_LENGTH)
         {
            length = MAX_RET_MSG_LENGTH;
         }
         trace_log(NULL, 0, C_TRACE, msg_str, length, NULL);
#endif
         (void)my_strncpy(hmr.hostname, hostname, MAX_REAL_HOSTNAME_LENGTH + 1);
         (void)my_strncpy(hmr.http_proxy, http_proxy, MAX_REAL_HOSTNAME_LENGTH + 1);
         (void)my_strncpy(hmr.user, user, MAX_USER_NAME_LENGTH + 1);
         (void)my_strncpy(hmr.passwd, passwd, MAX_USER_NAME_LENGTH + 1);
         if ((user[0] != '\0') || (passwd[0] != '\0'))
         {
            if (basic_authentication() != SUCCESS)
            {
               (void)close(http_fd);
               http_fd = -1;
               return(INCORRECT);
            }
         }
         hmr.port = port;
         hmr.free = YES;
#ifdef WITH_SSL
         hmr.strict = strict;
#endif
         hmr.http_version = 0;
         hmr.http_options = 0;
         hmr.http_options_not_working = 0;
         hmr.bytes_buffered = 0;
         hmr.bytes_read = 0;
#ifdef _WITH_EXTRA_CHECK
         hmr.http_etag[0] = '\0';
         hmr.http_weak_etag = YES;
#endif
      }
   }
   else
   {
#ifdef WITH_TRACE
      int                length;
#endif
#ifdef WITH_IP_DB
      int                ip_from_db = NO;
#endif
      char               *p_hostname;
#if defined (HAVE_GETADDRINFO) && defined (HAVE_GAI_STRERROR)
      int                reply;
      char               str_port[MAX_INT_LENGTH];
      struct addrinfo    hints,
                         *result = NULL,
                         *rp;

      if (http_proxy[0] == '\0')
      {
         p_hostname = hostname;
      }
      else
      {
         p_hostname = http_proxy;
      }
      (void)memset((struct addrinfo *) &hints, 0, sizeof(struct addrinfo));
      hints.ai_family = (special_flag & DISABLE_IPV6_FLAG) ? AF_INET : AF_UNSPEC;
      hints.ai_socktype = SOCK_STREAM;
/* ???   hints.ai_flags = AI_CANONNAME; */

      (void)snprintf(str_port, MAX_INT_LENGTH, "%d", port);
      reply = getaddrinfo(p_hostname, str_port, &hints, &result);
      if (reply != 0)
      {
# ifdef WITH_IP_DB
         char ip_str[MAX_REAL_HOSTNAME_LENGTH + 10];

         if (((reply == EAI_NONAME) || (reply == EAI_SYSTEM) || (reply == EAI_AGAIN)) &&
             (use_ip_db == YES) &&
             (lookup_ip_from_ip_db(hostname, ip_str, MAX_REAL_HOSTNAME_LENGTH + 10) == SUCCESS))
         {
            reply = getaddrinfo(ip_str, str_port, &hints, &result);
            if (reply != 0)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                         _("Failed to getaddrinfo() %s : %s"),
                         ip_str, gai_strerror(reply));
               freeaddrinfo(result);
               return(INCORRECT);
            }

            ip_from_db = YES;
         }
         else
         {
# endif
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                      _("Failed to getaddrinfo() %s : %s"),
                      p_hostname, gai_strerror(reply));
            freeaddrinfo(result);
            return(INCORRECT);
# ifdef WITH_IP_DB
         }
# endif
      }

      /*
       * getaddrinfo() returns a list of address structures.
       * Try each address until we successfully connect(). If socket()
       * (or connect()) fails, we (close the socket and) try the next
       * address.
       */
      for (rp = result; rp != NULL; rp = rp->ai_next)
      {
         if ((http_fd = socket(rp->ai_family, rp->ai_socktype,
                                  rp->ai_protocol)) == -1)
         {
# ifdef WITH_TRACE
            length = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                              _("socket() error : %s"), strerror(errno));
             if (length > MAX_RET_MSG_LENGTH)
             {
                length = MAX_RET_MSG_LENGTH;
             }
            trace_log(NULL, 0, C_TRACE, msg_str, length, NULL);
# endif
            continue;
         }

         if (sndbuf_size > 0)
         {
            if (setsockopt(http_fd, SOL_SOCKET, SO_SNDBUF,
                           (char *)&sndbuf_size, sizeof(sndbuf_size)) < 0)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                         _("setsockopt() error : %s"), strerror(errno));
            }
            hmr.sndbuf_size = sndbuf_size;
         }
         else
         {
            hmr.sndbuf_size = 0;
         }
         if (rcvbuf_size > 0)
         {
            if (setsockopt(http_fd, SOL_SOCKET, SO_RCVBUF,
                           (char *)&rcvbuf_size, sizeof(rcvbuf_size)) < 0)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                         _("setsockopt() error : %s"), strerror(errno));
            }
            hmr.rcvbuf_size = rcvbuf_size;
         }
         else
         {
            hmr.rcvbuf_size = 0;
         }
# ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
         if (timeout_flag != OFF)
         {
            reply = 1;
            if (setsockopt(http_fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&reply,
                           sizeof(reply)) < 0)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                         _("setsockopt() SO_KEEPALIVE error : %s"), strerror(errno));
            }
#  ifdef TCP_KEEPALIVE
            reply = timeout_flag;
            if (setsockopt(http_fd, IPPROTO_IP, TCP_KEEPALIVE, (char *)&reply,
                           sizeof(reply)) < 0)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                         _("setsockopt() TCP_KEEPALIVE error : %s"),
                         strerror(errno));
            }
#  endif
            timeout_flag = OFF;
         }
# endif

         reply = connect_with_timeout(http_fd, rp->ai_addr, rp->ai_addrlen);
         if (reply == INCORRECT)
         {
            if (errno != 0)
            {
# ifdef WITH_TRACE
               length = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                                 _("connect() error : %s"), strerror(errno));
               if (length > MAX_RET_MSG_LENGTH)
               {
                  length = MAX_RET_MSG_LENGTH;
               }
               trace_log(NULL, 0, C_TRACE, msg_str, length, NULL);
# endif
            }
            (void)close(http_fd);
            continue;
         }
         else if (reply == PERMANENT_INCORRECT)
              {
                 (void)close(http_fd);
                 http_fd = -1;
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                           _("Failed to connect() to %s at port %d"),
                           hostname, port);
                 freeaddrinfo(result);
                 return(INCORRECT);
              }

         break; /* Success */
      }

      /* Ensure that we succeeded in finding an address. */
      if (rp == NULL)
      {
         if (errno)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                      _("Failed to connect() to %s at port %d : %s"),
                      hostname, port, strerror(errno));
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                      _("Failed to connect() to %s at port %d"),
                      hostname, port);
         }
         (void)close(http_fd);
         http_fd = -1;
         freeaddrinfo(result);

         return(INCORRECT);
      }

# ifdef WITH_IP_DB
      if ((ip_from_db == NO) && (use_ip_db == YES) && (get_store_ip() == YES))
      {
         char ip_str[MAX_AFD_INET_ADDRSTRLEN];

         if (rp->ai_family == AF_INET)
         {
            struct sockaddr_in *sa = (struct sockaddr_in *)rp->ai_addr;

            (void)my_strncpy(ip_str, inet_ntoa(sa->sin_addr),
                             MAX_AFD_INET_ADDRSTRLEN);
         }
         else if (rp->ai_family == AF_INET6)
              {
                 struct sockaddr_in6 *sa = (struct sockaddr_in6 *)rp->ai_addr;

                 if (inet_ntop(rp->ai_family, &(sa->sin6_addr),
                               ip_str, MAX_AFD_INET_ADDRSTRLEN) == NULL)
                 {
                    ip_str[0] = '\0';
                 }
              }
              else
              {
                 /* Unknown family type. */
                 ip_str[0] = '\0';
              }

         if (ip_str[0] != '\0')
         {
            add_to_ip_db(hostname, ip_str);
         }
      }
# endif /* WITH_IP_DB */

      freeaddrinfo(result);
#else
      struct sockaddr_in sin;

      if (http_proxy[0] == '\0')
      {
         p_hostname = hostname;
      }
      else
      {
         p_hostname = http_proxy;
      }
      (void)memset((struct sockaddr *) &sin, 0, sizeof(sin));
      if ((sin.sin_addr.s_addr = inet_addr(p_hostname)) == -1)
      {
         register struct hostent *p_host = NULL;

         if ((p_host = gethostbyname(p_hostname)) == NULL)
         {
# if !defined (_HPUX) && !defined (_SCO)
            if (h_errno != 0)
            {
#  ifdef WITH_IP_DB
               char ip_str[MAX_REAL_HOSTNAME_LENGTH + 10];

               if (((h_errno == HOST_NOT_FOUND) ||
#   ifdef NO_ADDRESS
                    (h_errno == NO_ADDRESS) ||
#   endif
                    (h_errno == NO_DATA)) && (use_ip_db == YES) &&
                   (lookup_ip_from_ip_db(hostname, ip_str, MAX_REAL_HOSTNAME_LENGTH + 10) == SUCCESS))
               {
                  if ((sin.sin_addr.s_addr = inet_addr(ip_str)) == -1)
                  {
#  ifdef LINUX
                     if ((h_errno > 0) && (h_errno < h_nerr))
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                                  _("Failed to gethostbyname() %s : %s"),
                                  ip_str, h_errlist[h_errno]);
                     }
                     else
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                                  _("Failed to gethostbyname() %s (h_errno = %d) : %s"),
                                  ip_str, h_errno, strerror(errno));
                     }
#  else
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                               _("Failed to gethostbyname() %s (h_errno = %d) : %s"),
                               ip_str, h_errno, strerror(errno));
#  endif
                     http_fd = -1;
                     return(INCORRECT);
                  }
                  ip_from_db = YES;
               }
               else
               {
#  endif /* WITH_IP_DB */
#  ifdef LINUX
                  if ((h_errno > 0) && (h_errno < h_nerr))
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                               _("Failed to gethostbyname() %s : %s"),
                               hostname, h_errlist[h_errno]);
                  }
                  else
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                               _("Failed to gethostbyname() %s (h_errno = %d) : %s"),
                               hostname, h_errno, strerror(errno));
                  }
#  else
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                            _("Failed to gethostbyname() %s (h_errno = %d) : %s"),
                            hostname, h_errno, strerror(errno));
#  endif
#  ifdef WITH_IP_DB
               }
#  endif
            }
            else
            {
# endif /* !_HPUX && !_SCO */
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                         _("Failed to gethostbyname() %s : %s"),
                         p_hostname, strerror(errno));
# if !defined (_HPUX) && !defined (_SCO)
            }
# endif
            http_fd = -1;
            return(INCORRECT);
         }

         /* Copy IP number to socket structure. */
         memcpy((char *)&sin.sin_addr, p_host->h_addr, p_host->h_length);
      }

      if ((http_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                   _("socket() error : %s"), strerror(errno));
         return(INCORRECT);
      }
      sin.sin_family = AF_INET;
      sin.sin_port = htons((u_short)port);

      if (sndbuf_size > 0)
      {
         if (setsockopt(http_fd, SOL_SOCKET, SO_SNDBUF,
                        (char *)&sndbuf_size, sizeof(sndbuf_size)) < 0)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                      _("setsockopt() error : %s"), strerror(errno));
         }
         hmr.sndbuf_size = sndbuf_size;
      }
      else
      {
         hmr.sndbuf_size = 0;
      }
      if (rcvbuf_size > 0)
      {
         if (setsockopt(http_fd, SOL_SOCKET, SO_RCVBUF,
                        (char *)&rcvbuf_size, sizeof(rcvbuf_size)) < 0)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                      _("setsockopt() error : %s"), strerror(errno));
         }
         hmr.rcvbuf_size = rcvbuf_size;
      }
      else
      {
         hmr.rcvbuf_size = 0;
      }
# ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
      if (timeout_flag != OFF)
      {
         reply = 1;
         if (setsockopt(http_fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&reply,
                        sizeof(reply)) < 0)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                      _("setsockopt() SO_KEEPALIVE error : %s"), strerror(errno));
         }
#  ifdef TCP_KEEPALIVE
         reply = timeout_flag;
         if (setsockopt(http_fd, IPPROTO_IP, TCP_KEEPALIVE, (char *)&reply,
                        sizeof(reply)) < 0)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                      _("setsockopt() TCP_KEEPALIVE error : %s"), strerror(errno));
         }
#  endif
         timeout_flag = OFF;
      }
# endif

      if (connect_with_timeout(http_fd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
      {
         if (errno)
         {
# ifdef ETIMEDOUT
            if (errno == ETIMEDOUT)
            {
               timeout_flag = ON;
            }
#  ifdef ECONNREFUSED
            else if (errno == ECONNREFUSED)
                 {
                    timeout_flag = CON_REFUSED;
                 }
#  endif
# else
#  ifdef ECONNREFUSED
            if (errno == ECONNREFUSED)
            {
               timeout_flag = CON_REFUSED;
            }
#  endif
# endif
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                      _("Failed to connect() to %s at port %d : %s"),
                      p_hostname, port, strerror(errno));
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                      _("Failed to connect() to %s at port %d"),
                      p_hostname, port);
         }
         (void)close(http_fd);
         http_fd = -1;
         return(INCORRECT);
      }

# ifdef WITH_IP_DB
      if ((ip_from_db == NO) && (use_ip_db == YES) && (get_store_ip() == YES))
      {
         char ip_str[MAX_AFD_INET_ADDRSTRLEN];

         (void)my_strncpy(ip_str, inet_ntoa(sin.sin_addr), MAX_AFD_INET_ADDRSTRLEN);

         if (ip_str[0] != '\0')
         {
            add_to_ip_db(hostname, ip_str);
         }
      }
# endif
#endif
#ifdef WITH_TRACE
      if (http_proxy[0] == '\0')
      {
         length = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                           "Connected to %s at port %d", p_hostname, port);
      }
      else
      {
         length = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                           "Connected to HTTP proxy %s at port %d",
                           p_hostname, port);
      }
      if (length > MAX_RET_MSG_LENGTH)
      {
         length = MAX_RET_MSG_LENGTH;
      }
      trace_log(NULL, 0, C_TRACE, msg_str, length, NULL);
#endif

      (void)my_strncpy(hmr.hostname, hostname, MAX_REAL_HOSTNAME_LENGTH + 1);
      (void)my_strncpy(hmr.http_proxy, http_proxy, MAX_REAL_HOSTNAME_LENGTH + 1);
      (void)my_strncpy(hmr.user, user, MAX_USER_NAME_LENGTH + 1);
      (void)my_strncpy(hmr.passwd, passwd, MAX_USER_NAME_LENGTH + 1);
      if ((user[0] != '\0') || (passwd[0] != '\0'))
      {
         if (basic_authentication() != SUCCESS)
         {
            (void)close(http_fd);
            http_fd = -1;
            return(INCORRECT);
         }
      }
      hmr.port = port;
      hmr.free = YES;
      hmr.http_version = 0;
      hmr.http_options = 0;
      hmr.http_options_not_working = 0;
      hmr.bytes_buffered = 0;
      hmr.bytes_read = 0;
#ifdef _WITH_EXTRA_CHECK
      hmr.http_etag[0] = '\0';
      hmr.http_weak_etag = YES;
#endif

#ifdef WITH_SSL
      hmr.strict = strict;
      if ((ssl == YES) || (ssl == BOTH))
      {
         char *p_env,
              *p_env1;

         if (ssl_ctx != NULL)
         {
            SSL_CTX_free(ssl_ctx);
         }
         hmr.ssl = YES;
         SSLeay_add_ssl_algorithms();
         if ((ssl_ctx = (SSL_CTX *)SSL_CTX_new(SSLv23_client_method())) == NULL)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                      _("SSL_CTX_new() unable to create a new SSL context structure."));
            (void)close(http_fd);
            http_fd = -1;
            return(INCORRECT);
         }
# ifdef NO_SSLv2
         SSL_CTX_set_options(ssl_ctx, SSL_OP_ALL | SSL_OP_NO_SSLv2);
# else 
#  ifdef NO_SSLv3
         SSL_CTX_set_options(ssl_ctx, SSL_OP_ALL | SSL_OP_NO_SSLv3);
#  else
#   ifdef NO_SSLv23
         SSL_CTX_set_options(ssl_ctx, SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
#   else
#    ifdef NO_SSLv23TLS1_0
         SSL_CTX_set_options(ssl_ctx, SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1);
#    else
#     ifdef NO_SSLv23TLS1_0TLS1_1
         SSL_CTX_set_options(ssl_ctx, SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);
#     else
         SSL_CTX_set_options(ssl_ctx, SSL_OP_ALL);
#     endif
#    endif
#   endif
#  endif
# endif
         SSL_CTX_set_mode(ssl_ctx, SSL_MODE_AUTO_RETRY);
         if ((p_env = getenv("SSL_CIPHER")) != NULL)
         {
            SSL_CTX_set_cipher_list(ssl_ctx, p_env);
         }
         else
         {
            SSL_CTX_set_cipher_list(ssl_ctx, NULL);
         }
         if (((p_env = getenv(X509_get_default_cert_file_env())) != NULL) &&
             ((p_env1 = getenv(X509_get_default_cert_dir_env())) != NULL))
         {
            SSL_CTX_load_verify_locations(ssl_ctx, p_env, p_env1);
         }
# ifdef WHEN_WE_KNOW
         if (((p_env = getenv("SSL_CRL_FILE")) != NULL) &&
             ((p_env1 = getenv("SSL_CRL_DIR")) != NULL))
         {
         }
         else
         {
         }
# endif
         SSL_CTX_set_verify(ssl_ctx,
                            (strict == YES) ? SSL_VERIFY_PEER : SSL_VERIFY_NONE,
                            NULL);

         ssl_con = (SSL *)SSL_new(ssl_ctx);
         SSL_set_connect_state(ssl_con);
         SSL_set_fd(ssl_con, http_fd);
         if (!SSL_set_tlsext_host_name(ssl_con, hostname))
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                      _("SSL_set_tlsext_host_name() failed to enable ServerNameIndication for %s"),
                      hostname);
            (void)close(http_fd);
            http_fd = -1;
            return(INCORRECT);
         }

         /*
          * NOTE: Because we have set SSL_MODE_AUTO_RETRY, a SSL_read() can
          *       block even when we use select(). The same thing might be true
          *       for SSL_write() but have so far not encountered this case.
          *       It might be cleaner not to set SSL_MODE_AUTO_RETRY and handle
          *       SSL_ERROR_WANT_READ error case.
          */
         if (signal(SIGALRM, sig_handler) == SIG_ERR)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                      _("Failed to set signal handler : %s"), strerror(errno));
            (void)close(http_fd);
            http_fd = -1;
            return(INCORRECT);
         }

         if (sigsetjmp(env_alrm, 1) != 0)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_connect", NULL,
                      _("SSL_connect() timeout (%ld)"), transfer_timeout);
            timeout_flag = ON;
            (void)close(http_fd);
            http_fd = -1;
            return(INCORRECT);
         }
         (void)alarm(transfer_timeout);
         reply = SSL_connect(ssl_con);
         (void)alarm(0);
         if (reply <= 0)
         {
            char *ptr;

            ptr = ssl_error_msg("SSL_connect", ssl_con, NULL, reply, msg_str);
            reply = SSL_get_verify_result(ssl_con);
            if (reply == X509_V_ERR_CRL_SIGNATURE_FAILURE)
            {
               (void)my_strncpy(ptr,
                                _(" | Verify result: The signature of the certificate is invalid!"),
                                MAX_RET_MSG_LENGTH - (ptr - msg_str));
            }
            else if (reply == X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD)
                 {
                     (void)my_strncpy(ptr,
                                      _(" | Verify result: The CRL nextUpdate field contains an invalid time."),
                                      MAX_RET_MSG_LENGTH - (ptr - msg_str));
                 }
            else if (reply == X509_V_ERR_CRL_HAS_EXPIRED)
                 {
                     (void)my_strncpy(ptr,
                                      _(" | Verify result: The CRL has expired."),
                                      MAX_RET_MSG_LENGTH - (ptr - msg_str));
                 }
            else if (reply == X509_V_ERR_CERT_REVOKED)
                 {
                    (void)my_strncpy(ptr,
                                     _(" | Verify result: Certificate revoked."),
                                      MAX_RET_MSG_LENGTH - (ptr - msg_str));
                 }
            else if (reply > X509_V_OK)
                 {
                    (void)snprintf(ptr, MAX_RET_MSG_LENGTH - (ptr - msg_str),
                                   _(" | Verify result: %d"), reply);
                 }
            reply = INCORRECT;

            SSL_free(ssl_con);
            ssl_con = NULL;
            (void)close(http_fd);
            http_fd = -1;
         }
         else
         {
# ifdef WITH_TRACE
            const char       *ssl_version;
            int              length;
            const SSL_CIPHER *ssl_cipher;

            ssl_version = SSL_get_cipher_version(ssl_con);
            length = strlen(msg_str);
            if ((ssl_cipher = SSL_get_current_cipher(ssl_con)) != NULL)
            {
               int ssl_bits;

               SSL_CIPHER_get_bits(ssl_cipher, &ssl_bits);
               (void)snprintf(&msg_str[length], MAX_RET_MSG_LENGTH - length,
                              "  <%s, cipher %s, %d bits>",
                              ssl_version, SSL_CIPHER_get_name(ssl_cipher),
                              ssl_bits);
            }
            else
            {
               (void)snprintf(&msg_str[length], MAX_RET_MSG_LENGTH - length,
                              "  <%s, cipher ?, ? bits>", ssl_version);
            }
# endif
            reply = SUCCESS;
         }
# ifdef WITH_SSL_READ_AHEAD
         /* This is not set because I could not detect any advantage using this. */
         SSL_set_read_ahead(ssl_con, 1);
# endif

         return(reply);
      }
      else
      {
         hmr.ssl = NO;
      }
#endif /* WITH_SSL */
   }

   return(SUCCESS);
}


/*########################### http_version() ############################*/
int
http_version(void)
{
   return(hmr.http_version);
}


/*############################## http_get() #############################*/
int
http_get(char  *host,
         char  *path,
         char  *filename,
#ifdef _WITH_EXTRA_CHECK
         char  *etag,
#endif
         off_t *content_length,
         off_t offset)
{
   int reply;

   if (http_fd == -1)
   {
      reply = PERMANENT_DISCONNECT;
   }
   else
   {
      char range[13 + MAX_OFF_T_LENGTH + 1 + MAX_OFF_T_LENGTH + 3],
#ifdef _WITH_EXTRA_CHECK
           none_match[16 + MAX_EXTRA_LS_DATA_LENGTH + 5],
#endif
           resource[MAX_RECIPIENT_LENGTH];

      hmr.bytes_read = 0;
      hmr.retries = 0;
      hmr.date = -1;
      if ((*content_length == 0) && (filename[0] != '\0') &&
          ((hmr.http_options_not_working & HTTP_OPTION_HEAD) == 0))
      {
         off_t end;

         if ((reply = http_head(host, path, filename, &end, NULL)) == SUCCESS)
         {
            *content_length = end;
            hmr.retries = 0;
         }
         else
         {
            if ((reply == 400) || /* Bad Request */
                (reply == 405) || /* Method Not Allowed */
                (reply == 403) || /* Forbidden */
                (reply == 501))   /* Not Implemented */
            {
               *content_length = end;
               hmr.retries = 0;
               hmr.http_options_not_working |= HTTP_OPTION_HEAD;
            }
            else
            {
               return(reply);
            }
         }
      }

      /* If we have send a HEAD command the remote server indicates a */
      /* close connection we need to reopen the connection.           */
      if ((reply = check_connection()) == CONNECTION_REOPENED)
      {
         hmr.retries = 1;
      }
      else if (reply == INCORRECT)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_get", NULL,
                        _("Failed to reconnect."));
              return(INCORRECT);
           }

      if ((offset) && (*content_length == offset))
      {
         return(NOTHING_TO_FETCH);
      }
      if (hmr.http_proxy[0] == '\0')
      {
         if (*path == '/')
         {
            (void)snprintf(resource, MAX_RECIPIENT_LENGTH,
                           "%s%s", path, filename);
         }
         else
         {
            (void)snprintf(resource, MAX_RECIPIENT_LENGTH,
                           "/%s%s", path, filename);
         }
      }
      else
      {
#ifdef WITH_SSL
         if (hmr.ssl == YES)
         {
            if (*path == '/')
            {
               (void)snprintf(resource, MAX_RECIPIENT_LENGTH,
                              "https://%s%s%s", host, path, filename);
            }
            else
            {
               (void)snprintf(resource, MAX_RECIPIENT_LENGTH,
                              "https://%s/%s%s", host, path, filename);
            }
         }
         else
         {
#endif
            if (*path == '/')
            {
               (void)snprintf(resource, MAX_RECIPIENT_LENGTH,
                              "http://%s%s%s", host, path, filename);
            }
            else
            {
               (void)snprintf(resource, MAX_RECIPIENT_LENGTH,
                              "http://%s/%s%s", host, path, filename);
            }
#ifdef WITH_SSL
         }
#endif
      }
retry_get_range:
      if ((offset == 0) || (offset < 0))
      {
         range[0] = '\0';
      }
      else
      {
         if (*content_length == 0)
         {
            (void)snprintf(range,
                           13 + MAX_OFF_T_LENGTH + 1 + MAX_OFF_T_LENGTH + 3,
#if SIZEOF_OFF_T == 4
                           "Range: bytes=%ld-\r\n",
#else
                           "Range: bytes=%lld-\r\n",
#endif
                           (pri_off_t)offset);
         }
         else
         {
            (void)snprintf(range,
                           13 + MAX_OFF_T_LENGTH + 1 + MAX_OFF_T_LENGTH + 3,
#if SIZEOF_OFF_T == 4
                           "Range: bytes=%ld-%ld\r\n",
#else
                           "Range: bytes=%lld-%lld\r\n",
#endif
                           (pri_off_t)offset, (pri_off_t)*content_length);
         }
      }
#ifdef _WITH_EXTRA_CHECK
      if (etag[0] == '\0')
      {
         none_match[0] = '\0';
      }
      else
      {
         size_t bytes_copied;

         /* If-None-Match */
         none_match[0] = 'I';
         none_match[1] = 'f';
         none_match[2] = '-';
         none_match[3] = 'N';
         none_match[4] = 'o';
         none_match[5] = 'n';
         none_match[6] = 'e';
         none_match[7] = '-';
         none_match[8] = 'M';
         none_match[9] = 'a';
         none_match[10] = 't';
         none_match[11] = 'c';
         none_match[12] = 'h';
         none_match[13] = ':';
         none_match[14] = ' ';
         none_match[15] = '"';
         bytes_copied = my_strlcpy(&none_match[16], etag, MAX_EXTRA_LS_DATA_LENGTH);
         none_match[16 + bytes_copied] = '"';
         none_match[16 + bytes_copied + 1] = '\r';
         none_match[16 + bytes_copied + 2] = '\n';
         none_match[16 + bytes_copied + 3] = '\0';
      }
#endif
retry_get:
#ifdef _WITH_EXTRA_CHECK
      if ((reply = command(http_fd,
                           "GET %s HTTP/1.1\r\n%sUser-Agent: AFD/%s\r\n%s%sHost: %s\r\nContent-length: 0\r\nAccept: */*\r\n",
                           resource, range, PACKAGE_VERSION,
                           (hmr.authorization == NULL) ? "" : hmr.authorization,
                           none_match, host)) == SUCCESS)
#else
      if ((reply = command(http_fd,
                           "GET %s HTTP/1.1\r\n%sUser-Agent: AFD/%s\r\n%sHost: %s\r\nContent-length: 0\r\nAccept: */*\r\n",
                           resource, range, PACKAGE_VERSION,
                           (hmr.authorization == NULL) ? "" : hmr.authorization,
                           host)) == SUCCESS)
#endif
      {
         hmr.content_length = -1;
         if (((reply = get_http_reply(&hmr.bytes_buffered, 200, __LINE__)) == 200) ||
             (reply == 204) || /* No content. */
             (reply == 206))   /* Partial Content. */
         {
            if ((filename[0] != '\0') && (hmr.content_length == 0))
            {
               reply = NOTHING_TO_FETCH;
            }
            else
            {
               if (hmr.chunked == YES)
               {
                  reply = CHUNKED;
               }
               else
               {
                  reply = SUCCESS;
               }
            }
            if ((*content_length != hmr.content_length) &&
                (hmr.content_length > 0))
            {
               *content_length = hmr.content_length;
            }
#ifdef _WITH_EXTRA_CHECK
            (void)strcpy(etag, hmr.http_etag);
#endif
         }
         else if (reply == 304) /* Not Modified */
              {
                 reply = NOTHING_TO_FETCH;
              }
         else if ((reply == 403) || /* Forbidden */
                  (reply == 404))   /* Not Found */
              {
                 int try_restore_msg_buffer;

                 if (hmr.bytes_buffered > 0)
                 {
                    try_restore_msg_buffer = YES;
                 }
                 else
                 {
                    try_restore_msg_buffer = NO;
                 }

                 if ((flush_read() == NO) && (hmr.chunked == YES))
                 {
                    read_last_chunk();
                 }
                 if (try_restore_msg_buffer == YES)
                 {
                    /* get_http_reply() has already overwritten the */
                    /* error message since read_msg() has read more */
                    /* then required. So in msg_str there is now    */
                    /* garbage. Let's try to restore at least the   */
                    /* first line.                                  */
                    if (hmr.header_length > 0)
                    {
                       (void)memcpy(msg_str, hmr.msg_header, hmr.header_length);
                    }
                    else
                    {
                       /* So we do not show any garbage. */
                       msg_str[0] = '\0';
                    }
                 }
                 hmr.bytes_buffered = 0;
                 hmr.bytes_read = 0;
              }
         else if (reply == 401) /* Unauthorized */
              {
                 if (hmr.www_authenticate == WWW_AUTHENTICATE_BASIC)
                 {
                    if (basic_authentication() == SUCCESS)
                    {
                       if (check_connection() > INCORRECT)
                       {
                          goto retry_get;
                       }
                    }
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_get", NULL,
                              _("Failed to create basic authentication."));
                 }
                 else if (hmr.www_authenticate == WWW_AUTHENTICATE_DIGEST)
                      {
                         trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_get", NULL,
                                   _("Digest authentication not yet implemented."));
                      }

                 hmr.bytes_buffered = 0;
                 hmr.bytes_read = 0;
              }
         else if (reply == 416) /* Requested Range Not Satisfiable */
              {
                 offset = 0;
                 goto retry_get_range;
              }
         else if (reply == CONNECTION_REOPENED)
              {
                 goto retry_get;
              }
              else
              {
                 hmr.bytes_buffered = 0;
                 hmr.bytes_read = 0;
              }
      }
   }

   return(reply);
}


/*############################## http_put() #############################*/
int
http_put(char *host, char *path, char *filename, off_t length, int first_file)
{
   if (http_fd == -1)
   {
      return(PERMANENT_DISCONNECT);
   }
   hmr.retries = 0;
   hmr.date = -1;
   if (first_file == NO)
   {
      if (check_connection() == CONNECTION_REOPENED)
      {
         trans_log(DEBUG_SIGN, __FILE__, __LINE__, "http_put", NULL,
                   _("Reconnected."));
      }
   }
   return(command(http_fd,
#if SIZEOF_OFF_T == 4
                  "PUT %s%s%s HTTP/1.1\r\nUser-Agent: AFD/%s\r\nContent-length: %ld\r\n%sHost: %s\r\nControl: overwrite=1\r\n",
#else
                  "PUT %s%s%s HTTP/1.1\r\nUser-Agent: AFD/%s\r\nContent-length: %lld\r\n%sHost: %s\r\nControl: overwrite=1\r\n",
#endif
                  (*path != '/') ? "/" : "", path, filename,
                  PACKAGE_VERSION,
                  (pri_off_t)length,
                  (hmr.authorization == NULL) ? "" : hmr.authorization,
                  host));
}


/*######################### http_put_response() #########################*/
int
http_put_response(void)
{
   int reply;

   hmr.retries = -1; /* -1 so we do not do a reconnect in get_http_reply()! */
   hmr.date = -1;
   hmr.content_length = 0;
retry_put_response:
   if (((reply = get_http_reply(NULL, 201, __LINE__)) == 201) ||
       (reply == 204) || (reply == 200))
   {
      reply = SUCCESS;
   }
   else if (reply == 401) /* Unauthorized */
        {
           if (hmr.www_authenticate == WWW_AUTHENTICATE_BASIC)
           {
              if (basic_authentication() == SUCCESS)
              {
                 if (check_connection() > INCORRECT)
                 {
                    goto retry_put_response;
                 }
              }
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_put_response", NULL,
                        _("Failed to create basic authentication."));
           }
           else if (hmr.www_authenticate == WWW_AUTHENTICATE_DIGEST)
                {
                   trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_put_response", NULL,
                             _("Digest authentication not yet implemented."));
                }

           hmr.bytes_buffered = 0;
           hmr.bytes_read = 0;
        }
   else if (reply == CONNECTION_REOPENED)
        {
           goto retry_put_response;
        }

   hmr.bytes_buffered = 0;
   hmr.bytes_read = 0;

   return(reply);
}


/*############################## http_del() #############################*/
int
http_del(char *host, char *path, char *filename)
{
   int reply;

   if (http_fd == -1)
   {
      reply = PERMANENT_DISCONNECT;
   }
   else
   {
      char resource[MAX_RECIPIENT_LENGTH];

      hmr.retries = 0;
      hmr.date = -1;
      if (hmr.http_proxy[0] == '\0')
      {
         if (*path == '/')
         {
            (void)snprintf(resource, MAX_RECIPIENT_LENGTH,
                           "%s%s", path, filename);
         }
         else
         {
            (void)snprintf(resource, MAX_RECIPIENT_LENGTH,
                           "/%s%s", path, filename);
         }
      }
      else
      {
#ifdef WITH_SSL
         if (hmr.ssl == YES)
         {
            if (*path == '/')
            {
               (void)snprintf(resource, MAX_RECIPIENT_LENGTH,
                              "https://%s%s%s", host, path, filename);
            }
            else
            {
               (void)snprintf(resource, MAX_RECIPIENT_LENGTH,
                              "https://%s/%s%s", host, path, filename);
            }
         }
         else
         {
#endif
            if (*path == '/')
            {
               (void)snprintf(resource, MAX_RECIPIENT_LENGTH,
                              "http://%s%s%s", host, path, filename);
            }
            else
            {
               (void)snprintf(resource, MAX_RECIPIENT_LENGTH,
                              "http://%s/%s%s", host, path, filename);
            }
#ifdef WITH_SSL
         }
#endif
      }
retry_del:
      if ((reply = command(http_fd,
                           "DELETE %s HTTP/1.1\r\nUser-Agent: AFD/%s\r\n%sHost: %s\r\nContent-length: 0\r\n",
                           resource, PACKAGE_VERSION,
                           (hmr.authorization == NULL) ? "" : hmr.authorization,
                           host)) == SUCCESS)
      {
         if ((reply = get_http_reply(NULL, 200, __LINE__)) == 200)
         {
            reply = SUCCESS;
         }
         else if (reply == 401) /* Unauthorized */
              {
                 if (hmr.www_authenticate == WWW_AUTHENTICATE_BASIC)
                 {
                    if (basic_authentication() == SUCCESS)
                    {
                       if (check_connection() > INCORRECT)
                       {
                          goto retry_del;
                       }
                    }
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_del", NULL,
                              _("Failed to create basic authentication."));
                 }
                 else if (hmr.www_authenticate == WWW_AUTHENTICATE_DIGEST)
                      {
                         trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_del", NULL,
                                   _("Digest authentication not yet implemented."));
                      }

                 hmr.bytes_buffered = 0;
                 hmr.bytes_read = 0;
              }
         else if (reply == CONNECTION_REOPENED)
              {
                 goto retry_del;
              }
              else
              {
                 hmr.bytes_buffered = 0;
                 hmr.bytes_read = 0;
              }
      }
   }

   return(reply);
}


/*############################ http_options() ###########################*/
int
http_options(char *host, char *path)
{
   int reply;

   if (http_fd == -1)
   {
      reply = PERMANENT_DISCONNECT;
   }
   else
   {
      char resource[MAX_RECIPIENT_LENGTH];

      if (hmr.http_options_not_working & HTTP_OPTION_OPTIONS)
      {
         return(SUCCESS);
      }

      hmr.retries = 0;
      hmr.date = -1;
      if (*path == '\0')
      {
         resource[0] = '*';
         resource[1] = '\0';
      }
      else
      {
#ifdef WITH_SSL
         if (hmr.ssl == YES)
         {
            if (*path == '/')
            {
               (void)snprintf(resource, MAX_RECIPIENT_LENGTH,
                              "https://%s%s", host, path);
            }
            else
            {
               (void)snprintf(resource, MAX_RECIPIENT_LENGTH,
                              "https://%s/%s", host, path);
            }
         }
         else
         {
#endif
            if (*path == '/')
            {
               (void)snprintf(resource, MAX_RECIPIENT_LENGTH,
                              "http://%s%s", host, path);
            }
            else
            {
               (void)snprintf(resource, MAX_RECIPIENT_LENGTH,
                              "http://%s/%s", host, path);
            }
#ifdef WITH_SSL
         }
#endif
      }
retry_options:
      if ((reply = command(http_fd,
                           "OPTIONS %s HTTP/1.1\r\nUser-Agent: AFD/%s\r\n%sHost: %s\r\nContent-length: 0\r\nAccept: */*\r\n",
                           resource,
                           PACKAGE_VERSION,
                           (hmr.authorization == NULL) ? "" : hmr.authorization,
                           host)) == SUCCESS)
      {
         if ((reply = get_http_reply(&hmr.bytes_buffered, 200, __LINE__)) == 200)
         {
            reply = SUCCESS;
            if (hmr.chunked == YES)
            {
               read_last_chunk();
            }
         }
         else if (reply == 401) /* Unauthorized */
              {
                 if (hmr.www_authenticate == WWW_AUTHENTICATE_BASIC)
                 {
                    if (basic_authentication() == SUCCESS)
                    {
                       if (check_connection() > INCORRECT)
                       {
                          goto retry_options;
                       }
                    }
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_options", NULL,
                              _("Failed to create basic authentication."));
                 }
                 else if (hmr.www_authenticate == WWW_AUTHENTICATE_DIGEST)
                      {
                         trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_options", NULL,
                                   _("Digest authentication not yet implemented."));
                      }

                 hmr.bytes_buffered = 0;
                 hmr.bytes_read = 0;
              }
         else if ((reply == 403) || /* Forbidden */
                  (reply == 405) || /* Not Allowed */
                  (reply == 500))   /* Internal Server Error */
              {
                 int try_restore_msg_buffer;

                 if (hmr.bytes_buffered > 0)
                 {
                    try_restore_msg_buffer = YES;
                 }
                 else
                 {
                    try_restore_msg_buffer = NO;
                 }

                 if ((flush_read() == NO) && (hmr.chunked == YES))
                 {
                    read_last_chunk();
                 }
                 if (try_restore_msg_buffer == YES)
                 {
                    /* get_http_reply() has already overwritten the */
                    /* error message since read_msg() has read more */
                    /* then required. So in msg_str there is now    */
                    /* garbage. Let's try to restore at least the   */
                    /* first line.                                  */
                    if (hmr.header_length > 0)
                    {
                       (void)memcpy(msg_str, hmr.msg_header, hmr.header_length);
                    }
                    else
                    {
                       /* So we do not show any garbage. */
                       msg_str[0] = '\0';
                    }
                 }
                 hmr.bytes_buffered = 0;
                 hmr.bytes_read = 0;
                 reply = SUCCESS;
                 hmr.http_options_not_working |= HTTP_OPTION_OPTIONS;
              }
         else if (reply == CONNECTION_REOPENED)
              {
                 goto retry_options;
              }
              else
              {
                 int try_restore_msg_buffer;

                 if (hmr.bytes_buffered > 0)
                 {
                    try_restore_msg_buffer = YES;
                 }
                 else
                 {
                    try_restore_msg_buffer = NO;
                 }

                 if ((flush_read() == NO) && (hmr.chunked == YES))
                 {
                    read_last_chunk();
                 }
                 if (try_restore_msg_buffer == YES)
                 {
                    /* get_http_reply() has already overwritten the */
                    /* error message since read_msg() has read more */
                    /* then required. So in msg_str there is now    */
                    /* garbage. Let's try to restore at least the   */
                    /* first line.                                  */
                    if (hmr.header_length > 0)
                    {
                       (void)memcpy(msg_str, hmr.msg_header, hmr.header_length);
                    }
                    else
                    {
                       /* So we do not show any garbage. */
                       msg_str[0] = '\0';
                    }
                 }
                 hmr.bytes_buffered = 0;
                 hmr.bytes_read = 0;
              }
      }
   }

   return(reply);
}


/*############################# http_head() #############################*/
int
http_head(char   *host,
          char   *path,
          char   *filename,
          off_t  *content_length,
          time_t *date)
{
   int reply;

   if (http_fd == -1)
   {
      reply = PERMANENT_DISCONNECT;
   }
   else
   {
      char resource[MAX_RECIPIENT_LENGTH];

      if (hmr.http_options_not_working & HTTP_OPTION_HEAD)
      {
         hmr.date = 0;
         hmr.content_length = 0;
         *content_length = -1;
         if (date != NULL)
         {
            *date = 0;
         }
         return(SUCCESS);
      }
      hmr.retries = 0;
      hmr.date = 0;
      if (hmr.http_proxy[0] == '\0')
      {
         if (*path == '/')
         {
            (void)snprintf(resource, MAX_RECIPIENT_LENGTH,
                           "%s%s", path, filename);
         }
         else
         {
            (void)snprintf(resource, MAX_RECIPIENT_LENGTH,
                           "/%s%s", path, filename);
         }
      }
      else
      {
#ifdef WITH_SSL
         if (hmr.ssl == YES)
         {
            if (*path == '/')
            {
               (void)snprintf(resource, MAX_RECIPIENT_LENGTH,
                              "https://%s%s%s", host, path, filename);
            }
            else
            {
               (void)snprintf(resource, MAX_RECIPIENT_LENGTH,
                              "https://%s/%s%s", host, path, filename);
            }
         }
         else
         {
#endif
            if (*path == '/')
            {
               (void)snprintf(resource, MAX_RECIPIENT_LENGTH,
                              "http://%s%s%s", host, path, filename);
            }
            else
            {
               (void)snprintf(resource, MAX_RECIPIENT_LENGTH, 
                              "http://%s/%s%s", host, path, filename);
            }
#ifdef WITH_SSL
         }
#endif
      }
retry_head:
      if ((reply = command(http_fd,
                           "HEAD %s HTTP/1.1\r\nUser-Agent: AFD/%s\r\n%sHost: %s\r\nContent-length: 0\r\nAccept: */*\r\n",
                           resource, PACKAGE_VERSION,
                           (hmr.authorization == NULL) ? "" : hmr.authorization,
                           host)) == SUCCESS)
      {
         if ((reply = get_http_reply(NULL, 999, __LINE__)) == 200)
         {
            reply = SUCCESS;
            *content_length = hmr.content_length;
            if (date != NULL)
            {
               *date = hmr.date;
            }
         }
         else if (reply == 401) /* Unauthorized */
              {
                 if (hmr.www_authenticate == WWW_AUTHENTICATE_BASIC)
                 {
                    if (basic_authentication() == SUCCESS)
                    {
                       if (check_connection() > INCORRECT)
                       {
                          goto retry_head;
                       }
                    }
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_head", NULL,
                              _("Failed to create basic authentication."));
                 }
                 else if (hmr.www_authenticate == WWW_AUTHENTICATE_DIGEST)
                      {
                         trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_head", NULL,
                                   _("Digest authentication not yet implemented."));
                      }

                 hmr.bytes_buffered = 0;
                 hmr.bytes_read = 0;
              }
         else if ((reply == 400) || /* Bad Request */
                  (reply == 405) || /* Method Not Allowed */
                  (reply == 403) || /* Forbidden */
                  (reply == 501))   /* Not Implemented */
              {
                 hmr.http_options_not_working |= HTTP_OPTION_HEAD;
                 *content_length = -1;
                 if (date != NULL)
                 {
                    *date = 0;
                 }
                 hmr.bytes_buffered = 0;
                 hmr.bytes_read = 0;
              }
         else if (reply == CONNECTION_REOPENED)
              {
                 goto retry_head;
              }
              else
              {
                 hmr.bytes_buffered = 0;
                 hmr.bytes_read = 0;
              }
      }
   }

   return(reply);
}


/*++++++++++++++++++++++++ basic_authentication() +++++++++++++++++++++++*/
static int
basic_authentication(void)
{
   size_t length;
   char   *dst_ptr,
          *src_ptr,
          base_64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/",
          userpasswd[MAX_USER_NAME_LENGTH + MAX_USER_NAME_LENGTH];

   /*
    * Let us first construct the authorization string from user name
    * and passwd:  <user>:<passwd>
    * And then encode this to using base-64 encoding.
    */
   length = snprintf(userpasswd, MAX_USER_NAME_LENGTH + MAX_USER_NAME_LENGTH,
                     "%s:%s", hmr.user, hmr.passwd);
   if (length > (MAX_USER_NAME_LENGTH + MAX_USER_NAME_LENGTH))
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "basic_authentication", NULL,
#if SIZEOF_SIZE_T == 4
                _("Buffer length to store user+passwd not long enough, needs %d bytes"),
#else
                _("Buffer length to store user+passwd not long enough, needs %lld bytes"),
#endif
               (pri_size_t)length);
      return(INCORRECT);
   }
   free(hmr.authorization);
   if ((hmr.authorization = malloc(21 + length + (length / 3) + 4 + 2 + 1)) == NULL)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "basic_authentication", NULL,
                _("malloc() error : %s"), strerror(errno));
      return(INCORRECT);
   }
   dst_ptr = hmr.authorization;
   *dst_ptr = 'A'; *(dst_ptr + 1) = 'u'; *(dst_ptr + 2) = 't';
   *(dst_ptr + 3) = 'h'; *(dst_ptr + 4) = 'o'; *(dst_ptr + 5) = 'r';
   *(dst_ptr + 6) = 'i'; *(dst_ptr + 7) = 'z'; *(dst_ptr + 8) = 'a';
   *(dst_ptr + 9) = 't'; *(dst_ptr + 10) = 'i'; *(dst_ptr + 11) = 'o';
   *(dst_ptr + 12) = 'n'; *(dst_ptr + 13) = ':'; *(dst_ptr + 14) = ' ';
   *(dst_ptr + 15) = 'B'; *(dst_ptr + 16) = 'a'; *(dst_ptr + 17) = 's';
   *(dst_ptr + 18) = 'i'; *(dst_ptr + 19) = 'c'; *(dst_ptr + 20) = ' ';
   dst_ptr += 21;
   src_ptr = userpasswd;
   while (length > 2)
   {
      *dst_ptr = base_64[(int)(*src_ptr) >> 2];
      *(dst_ptr + 1) = base_64[((((int)(*src_ptr) & 0x3)) << 4) | (((int)(*(src_ptr + 1)) & 0xF0) >> 4)];
      *(dst_ptr + 2) = base_64[((((int)(*(src_ptr + 1))) & 0xF) << 2) | ((((int)(*(src_ptr + 2))) & 0xC0) >> 6)];
      *(dst_ptr + 3) = base_64[((int)(*(src_ptr + 2))) & 0x3F];
      src_ptr += 3;
      length -= 3;
      dst_ptr += 4;
   }
   if (length == 2)
   {
      *dst_ptr = base_64[(int)(*src_ptr) >> 2];
      *(dst_ptr + 1) = base_64[((((int)(*src_ptr) & 0x3)) << 4) | (((int)(*(src_ptr + 1)) & 0xF0) >> 4)];
      *(dst_ptr + 2) = base_64[(((int)(*(src_ptr + 1))) & 0xF) << 2];
      *(dst_ptr + 3) = '=';
      dst_ptr += 4;
   }
   else if (length == 1)
        {
           *dst_ptr = base_64[(int)(*src_ptr) >> 2];
           *(dst_ptr + 1) = base_64[(((int)(*src_ptr) & 0x3)) << 4];
           *(dst_ptr + 2) = '=';
           *(dst_ptr + 3) = '=';
           dst_ptr += 4;
        }
   *dst_ptr = '\r';
   *(dst_ptr + 1) = '\n';
   *(dst_ptr + 2) = '\0';

   return(SUCCESS);
}


/*############################# http_write() ############################*/
int
http_write(char *block, char *buffer, int size)
{
   char   *ptr = block;
   int    status;
   fd_set wset;

   /* Initialise descriptor set. */
   FD_ZERO(&wset);
   FD_SET(http_fd, &wset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = transfer_timeout;

   /* Wait for message x seconds and then continue. */
   status = select(http_fd + 1, NULL, &wset, NULL, &timeout);

   if (status == 0)
   {
      /* Timeout has arrived. */
      timeout_flag = ON;
      return(INCORRECT);
   }
   else if (status < 0)
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_write", NULL,
                     _("select() error : %s"), strerror(errno));
           return(INCORRECT);
        }
   else if (FD_ISSET(http_fd, &wset))
        {
           /*
            * When buffer is not NULL we want to sent the
            * data in ASCII-mode.
            */
           if (buffer != NULL)
           {
              register int i,
                           count = 0;

              for (i = 0; i < size; i++)
              {
                 if (*ptr == '\n')
                 {
                    buffer[count++] = '\r';
                    buffer[count++] = '\n';
                 }
                 else
                 {
                    buffer[count++] = *ptr;
                 }
                 ptr++;
              }
              size = count;
              ptr = buffer;
           }

#ifdef WITH_SSL
           if (ssl_con == NULL)
           {
#endif
              if ((status = write(http_fd, ptr, size)) != size)
              {
                 if ((errno == ECONNRESET) || (errno == EBADF))
                 {
                    timeout_flag = CON_RESET;
                 }
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_write", NULL,
                           _("write() error (%d) : %s"),
                           status, strerror(errno));
                 return(errno);
              }
#ifdef WITH_SSL
           }
           else
           {
              if (ssl_write(ssl_con, ptr, size) != size)
              {
                 return(INCORRECT);
              }
           }
#endif
#ifdef WITH_TRACE
           trace_log(NULL, 0, BIN_W_TRACE, ptr, size, NULL);
#endif
        }
        else
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_write", NULL,
                     _("Unknown condition."));
           return(INCORRECT);
        }
   
   return(SUCCESS);
}


/*############################# http_read() #############################*/
int
http_read(char *block, int blocksize)
{
   int bytes_read;

   if (hmr.bytes_buffered > 0)
   {
      if (hmr.bytes_buffered >= blocksize)
      {
         memcpy(block, msg_str, blocksize);
         if (hmr.bytes_buffered > blocksize)
         {
            hmr.bytes_buffered -= blocksize;
            (void)memmove(msg_str, msg_str + blocksize, hmr.bytes_buffered);
         }
         else
         {
            hmr.bytes_buffered = 0;
         }
         hmr.bytes_read = 0;
         bytes_read = blocksize;
      }
      else
      {
         memcpy(block, msg_str, hmr.bytes_buffered);
         bytes_read = hmr.bytes_buffered;
         hmr.bytes_buffered = 0;
         hmr.bytes_read = 0;
      }
      return(bytes_read);
   }

#ifdef WITH_SSL
   if ((ssl_con != NULL) && (SSL_pending(ssl_con)))
   {
      if ((bytes_read = SSL_read(ssl_con, block, blocksize)) == INCORRECT)
      {
         int status;

         if ((status = SSL_get_error(ssl_con,
                                     bytes_read)) == SSL_ERROR_SYSCALL)
         {
            if (errno == ECONNRESET)
            {
               timeout_flag = CON_RESET;
            }
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_read", NULL,
                      _("SSL_read() error : %s"), strerror(errno));
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_read", NULL,
                      _("SSL_read() error %d"), status);
         }
         return(INCORRECT);
      }
# ifdef WITH_TRACE
      trace_log(NULL, 0, BIN_R_TRACE, block, bytes_read, NULL);
# endif
   }
   else
#endif
   {
      int    status;
      fd_set rset;

      /* Initialise descriptor set. */
      FD_ZERO(&rset);
      FD_SET(http_fd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = transfer_timeout;

      /* Wait for message x seconds and then continue. */
      status = select(http_fd + 1, &rset, NULL, NULL, &timeout);

      if (status == 0)
      {
         /* Timeout has arrived. */
         timeout_flag = ON;
         return(INCORRECT);
      }
      else if (FD_ISSET(http_fd, &rset))
           {
#ifdef WITH_SSL
              if (ssl_con == NULL)
              {
#endif
                 if ((bytes_read = read(http_fd, block, blocksize)) == -1)
                 {
                    if (errno == ECONNRESET)
                    {
                       timeout_flag = CON_RESET;
                    }
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_read", NULL,
                              _("read() error : %s"), strerror(errno));
                    return(INCORRECT);
                 }
#ifdef WITH_SSL
              }
              else
              {
                 int tmp_errno;

                 /*
                  * Remember we have set SSL_MODE_AUTO_RETRY. This
                  * means the SSL lib may do several read() calls. We
                  * just assured one read() with select(). So lets
                  * set an an alarm since we might block on subsequent
                  * calls to read(). It might be better when we reimplement
                  * this without SSL_MODE_AUTO_RETRY and handle
                  * SSL_ERROR_WANT_READ ourself.
                  */
                 if (sigsetjmp(env_alrm, 1) != 0)
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_read", NULL,
                              _("SSL_read() timeout (%ld)"), transfer_timeout);
                    timeout_flag = ON;
                    return(INCORRECT);
                 }
                 (void)alarm(transfer_timeout);
                 bytes_read = SSL_read(ssl_con, block, blocksize);
                 tmp_errno = errno;
                 (void)alarm(0);

                 if (bytes_read == INCORRECT)
                 {
                    if ((status = SSL_get_error(ssl_con,
                                                bytes_read)) == SSL_ERROR_SYSCALL)
                    {
                       if (tmp_errno == ECONNRESET)
                       {
                          timeout_flag = CON_RESET;
                       }
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_read", NULL,
                                 _("SSL_read() error : %s"),
                                 strerror(tmp_errno));
                    }
                    else
                    {
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_read", NULL,
                                 _("SSL_read() error %d"), status);
                    }
                    return(INCORRECT);
                 }
              }
#endif
#ifdef WITH_TRACE
              trace_log(NULL, 0, BIN_R_TRACE, block, bytes_read, NULL);
#endif
           }
      else if (status < 0)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_read", NULL,
                        _("select() error : %s"), strerror(errno));
              return(INCORRECT);
           }
           else
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_read", NULL,
                        _("Unknown condition."));
              return(INCORRECT);
           }
   }

   return(bytes_read);
}


/*########################## http_chunk_read() ##########################*/
int
http_chunk_read(char **chunk, int *chunksize)
{
   int bytes_buffered,
       read_length;

   /* First, try read the chunk size. */
   if ((bytes_buffered = read_msg(&read_length, 0, __LINE__)) > 0)
   {
      int    bytes_read,
             status,
             tmp_chunksize;
      fd_set rset;

      tmp_chunksize = (int)strtol(msg_str, NULL, 16);
      if (errno == ERANGE)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_chunk_read", msg_str,
                   _("Failed to determine the chunk size with strtol() : %s"),
                   strerror(errno));
         return(INCORRECT);
      }
      else
      {
         if (tmp_chunksize == 0)
         {
            hmr.bytes_read = 0;
            hmr.bytes_buffered = 0;
            return(HTTP_LAST_CHUNK);
         }
         tmp_chunksize += 2;
         if (tmp_chunksize > *chunksize)
         {
            char *tmp_chunk;

            if ((tmp_chunk = realloc(*chunk, tmp_chunksize)) == NULL)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_chunk_read", NULL,
                         _("Failed to realloc() %d bytes : %s"),
                         tmp_chunksize, strerror(errno));
               free(*chunk);
               *chunk = NULL;
               return(INCORRECT);
            }
            *chunk = tmp_chunk;
            *chunksize = tmp_chunksize;
         }
         bytes_buffered -= (read_length + 1);
         if (tmp_chunksize > bytes_buffered)
         {
            (void)memcpy(*chunk, msg_str + read_length + 1, bytes_buffered);
            hmr.bytes_read = 0;
         }
         else
         {
            (void)memcpy(*chunk, msg_str + read_length + 1, tmp_chunksize - 2);
            hmr.bytes_read = bytes_buffered - tmp_chunksize;
            (void)read_msg(NULL, tmp_chunksize, __LINE__);
            return(tmp_chunksize - 2);
         }
      }

      FD_ZERO(&rset);
      do
      {
#ifdef WITH_SSL
         if ((ssl_con != NULL) && (SSL_pending(ssl_con)))
         {
            if ((bytes_read = SSL_read(ssl_con, (*chunk + bytes_buffered),
                                       tmp_chunksize - bytes_buffered)) == INCORRECT)
            {
               if ((status = SSL_get_error(ssl_con,
                                           bytes_read)) == SSL_ERROR_SYSCALL)
               {
                  if (errno == ECONNRESET)
                  {
                     timeout_flag = CON_RESET;
                  }
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_chunk_read", NULL,
                            _("SSL_read() error : %s"), strerror(errno));
               }
               else
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_chunk_read", NULL,
                            _("SSL_read() error %d"), status);
               }
               return(INCORRECT);
            }
            if (bytes_read == 0)
            {
               /* Premature end, remote side has closed connection. */
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_chunk_read", NULL,
                         _("Remote side closed connection (expected: %d read: %d)"),
                         tmp_chunksize, bytes_buffered);
               return(INCORRECT);
            }
# ifdef WITH_TRACE
            trace_log(NULL, 0, BIN_CMD_R_TRACE, (*chunk + bytes_buffered),
                      bytes_read, NULL);
# endif
            bytes_buffered += bytes_read;
         }
         else
         {
#endif
            FD_SET(http_fd, &rset);
            timeout.tv_usec = 0L;
            timeout.tv_sec = transfer_timeout;
   
            /* Wait for message x seconds and then continue. */
            status = select(http_fd + 1, &rset, NULL, NULL, &timeout);

            if (status == 0)
            {
               /* Timeout has arrived. */
               timeout_flag = ON;
               return(INCORRECT);
            }
            else if (FD_ISSET(http_fd, &rset))
                 {
#ifdef WITH_SSL
                    if (ssl_con == NULL)
                    {
#endif
                       if ((bytes_read = read(http_fd, (*chunk + bytes_buffered),
                                              tmp_chunksize - bytes_buffered)) == -1)
                       {
                          if (errno == ECONNRESET)
                          {
                             timeout_flag = CON_RESET;
                          }
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_chunk_read", NULL,
                                    _("read() error : %s"), strerror(errno));
                          return(INCORRECT);
                       }
#ifdef WITH_SSL
                    }
                    else
                    {
                       int tmp_errno;

                       /*
                        * Remember we have set SSL_MODE_AUTO_RETRY. This
                        * means the SSL lib may do several read() calls. We
                        * just assured one read() with select(). So lets
                        * set an an alarm since we might block on subsequent
                        * calls to read(). It might be better when we
                        * reimplement this without SSL_MODE_AUTO_RETRY
                        * and handle SSL_ERROR_WANT_READ ourself.
                        */
                       if (sigsetjmp(env_alrm, 1) != 0)
                       {
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_chunk_read", NULL,
                                    _("SSL_read() timeout (%ld)"),
                                    transfer_timeout);
                          timeout_flag = ON;
                          return(INCORRECT);
                       }
                       (void)alarm(transfer_timeout);
                       bytes_read = SSL_read(ssl_con, (*chunk + bytes_buffered),
                                             tmp_chunksize - bytes_buffered);
                       tmp_errno = errno;
                       (void)alarm(0);

                       if (bytes_read == INCORRECT)
                       {
                          if ((status = SSL_get_error(ssl_con,
                                                      bytes_read)) == SSL_ERROR_SYSCALL)
                          {
                             if (tmp_errno == ECONNRESET)
                             {
                                timeout_flag = CON_RESET;
                             }
                             trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_chunk_read", NULL,
                                       _("SSL_read() error : %s"),
                                       strerror(tmp_errno));
                          }
                          else
                          {
                             trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_chunk_read", NULL,
                                       _("SSL_read() error %d"), status);
                          }
                          return(INCORRECT);
                       }
                    }
#endif
                    if (bytes_read == 0)
                    {
                       /* Premature end, remote side has closed connection. */
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_chunk_read", NULL,
                                 _("Remote side closed connection (expected: %d read: %d)"),
                                 tmp_chunksize, bytes_buffered);
                       return(INCORRECT);
                    }
#ifdef WITH_TRACE
                    trace_log(NULL, 0, BIN_CMD_R_TRACE,
                              (*chunk + bytes_buffered), bytes_read, NULL);
#endif
                    bytes_buffered += bytes_read;
                 }
            else if (status < 0)
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_chunk_read", NULL,
                              _("select() error : %s"), strerror(errno));
                    return(INCORRECT);
                 }
                 else
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "http_chunk_read", NULL,
                              _("Unknown condition."));
                    return(INCORRECT);
                 }
#ifdef WITH_SSL
         }
#endif
      } while (bytes_buffered < tmp_chunksize);

      if (bytes_buffered == tmp_chunksize)
      {
         bytes_buffered -= 2;
      }
   }
   else if (bytes_buffered == 0)
        {
           timeout_flag = NEITHER;
           trans_log(ERROR_SIGN,  __FILE__, __LINE__, "http_chunk_read", NULL,
                     _("Remote hang up."));
           bytes_buffered = INCORRECT;
        }

   return(bytes_buffered);
}


/*############################ http_noop() ##############################*/
int
http_noop(void)
{
   char   *p_hostname;
   off_t  file_size;
   time_t file_mtime;

   /* I do not know of a better way. HTTP does not support  */
   /* a NOOP command, so lets just do a HEAD on the current */
   /* current server.                                       */
   if (hmr.http_proxy[0] == '\0')
   {
      p_hostname = hmr.hostname;
   }
   else
   {
      p_hostname = hmr.http_proxy;
   }

#ifdef WITH_TRACE
   trace_log(__FILE__, __LINE__, C_TRACE, NULL, 0,
             "http_noop(): Calling http_head(\"%s\", \"\", \"\")",
             p_hostname);
#endif
   return(http_head(p_hostname, "", "", &file_size, &file_mtime));
}


/*############################ http_quit() ##############################*/
void
http_quit(void)
{
   if ((hmr.authorization != NULL) && (hmr.free != NO))
   {
      free(hmr.authorization);
      hmr.authorization = NULL;
   }
   if (http_fd != -1)
   {
      if ((timeout_flag != ON) && (timeout_flag != CON_RESET) &&
          (simulation_mode != YES))
      {
         if (shutdown(http_fd, 1) < 0)
         {
            trans_log(DEBUG_SIGN, __FILE__, __LINE__, "http_quit", NULL,
                      _("shutdown() error : %s"), strerror(errno));
         }
      }
#ifdef WITH_SSL
      if (ssl_con != NULL)
      {
         if (timeout_flag != CON_RESET)
         {
            if (SSL_shutdown(ssl_con) == 0)
            {
               (void)SSL_shutdown(ssl_con);
            }
         }
         SSL_free(ssl_con);
         ssl_con = NULL;
      }
#endif
      if (close(http_fd) == -1)
      {
         trans_log(DEBUG_SIGN, __FILE__, __LINE__, "http_quit", NULL,
                   _("close() error : %s"), strerror(errno));
      }
      http_fd = -1;
   }
   return;
}


/*+++++++++++++++++++++++++ check_connection() ++++++++++++++++++++++++++*/
static int
check_connection(void)
{
   int connection_closed;

   if (hmr.close == YES)
   {
      connection_closed = YES;
      hmr.free = NO;
      http_quit();
      hmr.free = YES;
   }
   else
   {
      /*
       * TODO: We still need the code to check if connection is still up!
       */
      connection_closed = NO;
   }
   if (connection_closed == YES)
   {
      int status;

      if ((status = http_connect(hmr.hostname, hmr.http_proxy, hmr.port,
                                 hmr.user, hmr.passwd,
#ifdef WITH_SSL
                                 hmr.ssl, hmr.strict,
#endif
                                 hmr.sndbuf_size, hmr.rcvbuf_size)) != SUCCESS)
      {
         if (hmr.http_proxy[0] == '\0')
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "check_connection", msg_str,
                      _("HTTP reconnect to %s at port %d failed (%d)."),
                      hmr.hostname, hmr.port, status);
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "check_connection", msg_str,
                      _("HTTP reconnect to HTTP proxy %s at port %d failed (%d)."),
                      hmr.http_proxy, hmr.port, status);
         }
         return(INCORRECT);
      }
      else
      {
         return(CONNECTION_REOPENED);
      }
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++ get_http_reply() ++++++++++++++++++++++++++*/
static int
get_http_reply(int *ret_bytes_buffered, int reply, int line)
{
   int bytes_buffered,
       read_length,
       status_code = INCORRECT;

   if (simulation_mode == YES)
   {
      if (reply == 999)
      {
         hmr.content_length = 0;
         hmr.date = time(NULL);
         reply = 200;
      }
      hmr.chunked = NO;
      hmr.http_options = HTTP_OPTION_DELETE | HTTP_OPTION_HEAD | HTTP_OPTION_GET | HTTP_OPTION_PUT | HTTP_OPTION_MOVE | HTTP_OPTION_POST | HTTP_OPTION_OPTIONS;

      return(reply);
   }

   /* First read start line. */
   hmr.bytes_buffered = 0;
   if (ret_bytes_buffered != NULL)
   {
      *ret_bytes_buffered = 0;
   }
   if ((bytes_buffered = read_msg(&read_length, 0, line)) > 0)
   {
      hmr.close = NO;
      hmr.chunked = NO;
      if ((read_length > 12) &&
          (((msg_str[0] == 'H') || (msg_str[0] == 'h')) &&
           ((msg_str[1] == 'T') || (msg_str[1] == 't')) &&
           ((msg_str[2] == 'T') || (msg_str[2] == 't')) &&
           ((msg_str[3] == 'P') || (msg_str[3] == 'p')) &&
           (msg_str[4] == '/') &&
           (isdigit((int)(msg_str[5]))) && (msg_str[6] == '.') &&
           (isdigit((int)(msg_str[7]))) && (msg_str[8] == ' ') &&
           (isdigit((int)(msg_str[9]))) && (isdigit((int)(msg_str[10]))) &&
           (isdigit((int)(msg_str[11])))))
      {
         hmr.http_version = ((msg_str[5] - '0') * 10) + (msg_str[7] - '0');
         status_code = ((msg_str[9] - '0') * 100) +
                       ((msg_str[10] - '0') * 10) +
                       (msg_str[11] - '0');
         if (read_length <= MAX_HTTP_HEADER_BUFFER)
         {
            (void)memcpy(hmr.msg_header, msg_str, read_length);
            hmr.header_length = read_length;
         }
         else
         {
            (void)memcpy(hmr.msg_header, msg_str, MAX_HTTP_HEADER_BUFFER - 1);
            hmr.msg_header[MAX_HTTP_HEADER_BUFFER - 1] = '\0';
            hmr.header_length = MAX_HTTP_HEADER_BUFFER;
         }

         /*
          * Now lets read the headers and lets store that what we
          * think might be useful later.
          */
         for (;;)
         {
            if ((bytes_buffered = read_msg(&read_length, 0, line)) <= 0)
            {
               if (bytes_buffered == 0)
               {
                  trans_log(ERROR_SIGN,  __FILE__, __LINE__, "get_http_reply", NULL,
                            _("Remote hang up. [%d]"), line);
                  timeout_flag = NEITHER;
               }
               return(INCORRECT);
            }

            /* Check if we have reached header end. */
            if  ((read_length == 1) && (msg_str[0] == '\0'))
            {
               if (status_code >= 300)
               {
                  (void)strcpy(msg_str, hmr.msg_header);
               }
               break;
            }

            /* Content-Length: */
            if ((read_length > 15) && (msg_str[14] == ':') &&
                ((msg_str[8] == 'l') || (msg_str[8] == 'L')) &&
                ((msg_str[9] == 'e') || (msg_str[9] == 'E')) &&
                ((msg_str[10] == 'n') || (msg_str[10] == 'N')) &&
                ((msg_str[11] == 'g') || (msg_str[11] == 'G')) &&
                ((msg_str[12] == 't') || (msg_str[12] == 'T')) &&
                ((msg_str[13] == 'h') || (msg_str[13] == 'H')) &&
                ((msg_str[0] == 'C') || (msg_str[0] == 'c')) &&
                ((msg_str[1] == 'o') || (msg_str[1] == 'O')) &&
                ((msg_str[2] == 'n') || (msg_str[2] == 'N')) &&
                ((msg_str[3] == 't') || (msg_str[3] == 'T')) &&
                ((msg_str[4] == 'e') || (msg_str[4] == 'E')) &&
                ((msg_str[5] == 'n') || (msg_str[5] == 'N')) &&
                ((msg_str[6] == 't') || (msg_str[6] == 'T')) &&
                (msg_str[7] == '-'))
            {
               int i = 15;

               while ((i < read_length) &&
                      ((msg_str[i] == ' ') || (msg_str[i] == '\t')))
               {
                  i++;
               }
               if (i < read_length)
               {
                  int k = i;

                  while ((k < read_length) && (isdigit((int)(msg_str[k]))))
                  {
                     k++;
                  }
                  if (k > 0)
                  {
                     if (msg_str[k] != '\0')
                     {
                        msg_str[k] = '\0';
                     }
                     errno = 0;
                     hmr.content_length = (off_t)str2offt(&msg_str[i], NULL, 10);
                     if (errno != 0)
                     {
                        hmr.content_length = 0;
                     }
                  }
               }
            }
                 /* Connection: */
            else if ((read_length > 11) && (msg_str[10] == ':') &&
                     ((msg_str[0] == 'C') || (msg_str[0] == 'c')) &&
                     ((msg_str[1] == 'o') || (msg_str[1] == 'O')) &&
                     ((msg_str[2] == 'n') || (msg_str[2] == 'N')) &&
                     ((msg_str[3] == 'n') || (msg_str[3] == 'N')) &&
                     ((msg_str[4] == 'e') || (msg_str[4] == 'E')) &&
                     ((msg_str[5] == 'c') || (msg_str[5] == 'C')) &&
                     ((msg_str[6] == 't') || (msg_str[6] == 'T')) &&
                     ((msg_str[7] == 'i') || (msg_str[7] == 'I')) &&
                     ((msg_str[8] == 'o') || (msg_str[8] == 'O')) &&
                     ((msg_str[9] == 'n') || (msg_str[9] == 'N')))
                 {
                    int i = 11;

                    while ((i < read_length) &&
                           ((msg_str[i] == ' ') || (msg_str[i] == '\t')))
                    {
                       i++;
                    }
                    if (((i + 4) < read_length) &&
                        ((msg_str[i] == 'c') || (msg_str[i] == 'C')) &&
                        ((msg_str[i + 1] == 'l') || (msg_str[i + 1] == 'L')) &&
                        ((msg_str[i + 2] == 'o') || (msg_str[i + 2] == 'O')) &&
                        ((msg_str[i + 3] == 's') || (msg_str[i + 3] == 'S')) &&
                        ((msg_str[i + 4] == 'e') || (msg_str[i + 4] == 'E')))
                    {
                       hmr.close = YES;
                    }
                 }
                 /* WWW-Authenticate: */
            else if ((read_length > 17) && (msg_str[16] == ':') &&
                     (msg_str[3] == '-') &&
                     ((msg_str[0] == 'W') || (msg_str[0] == 'w')) &&
                     ((msg_str[1] == 'W') || (msg_str[1] == 'w')) &&
                     ((msg_str[2] == 'W') || (msg_str[2] == 'w')) &&
                     ((msg_str[4] == 'A') || (msg_str[4] == 'a')) &&
                     ((msg_str[5] == 'U') || (msg_str[5] == 'u')) &&
                     ((msg_str[6] == 'T') || (msg_str[6] == 't')) &&
                     ((msg_str[7] == 'H') || (msg_str[7] == 'h')) &&
                     ((msg_str[8] == 'E') || (msg_str[8] == 'e')) &&
                     ((msg_str[9] == 'N') || (msg_str[9] == 'n')) &&
                     ((msg_str[10] == 'T') || (msg_str[10] == 't')) &&
                     ((msg_str[11] == 'I') || (msg_str[11] == 'i')) &&
                     ((msg_str[12] == 'C') || (msg_str[12] == 'c')) &&
                     ((msg_str[13] == 'A') || (msg_str[13] == 'a')) &&
                     ((msg_str[14] == 'T') || (msg_str[14] == 't')) &&
                     ((msg_str[15] == 'E') || (msg_str[15] == 'e')))
                 {
                    int i = 17;

                    while ((i < read_length) &&
                           ((msg_str[i] == ' ') || (msg_str[i] == '\t')))
                    {
                       i++;
                    }
                    if ((i + 4) < read_length)
                    {
                       /* Basic */
                       if (((msg_str[i] == 'B') || (msg_str[i] == 'b')) &&
                           ((msg_str[i + 1] == 'A') || (msg_str[i + 1] == 'a')) &&
                           ((msg_str[i + 2] == 'S') || (msg_str[i + 2] == 's')) &&
                           ((msg_str[i + 3] == 'I') || (msg_str[i + 3] == 'i')) &&
                           ((msg_str[i + 4] == 'C') || (msg_str[i + 4] == 'c')))
                       {
                          hmr.www_authenticate = WWW_AUTHENTICATE_BASIC;
                       }
                            /* Digest */
                       else if (((msg_str[i] == 'D') || (msg_str[i] == 'd')) &&
                                ((msg_str[i + 1] == 'I') || (msg_str[i + 1] == 'i')) &&
                                ((msg_str[i + 2] == 'G') || (msg_str[i + 2] == 'g')) &&
                                ((msg_str[i + 3] == 'E') || (msg_str[i + 3] == 'e')) &&
                                ((msg_str[i + 4] == 'S') || (msg_str[i + 4] == 's')) &&
                                ((msg_str[i + 5] == 'T') || (msg_str[i + 5] == 't')))
                       {
                          hmr.www_authenticate = WWW_AUTHENTICATE_DIGEST;
                       }
                    }
                 }
                 /* Transfer-encoding: */
            else if ((read_length > 18) && (msg_str[17] == ':') &&
                     (msg_str[8] == '-') &&
                     ((msg_str[0] == 'T') || (msg_str[0] == 't')) &&
                     ((msg_str[1] == 'R') || (msg_str[1] == 'r')) &&
                     ((msg_str[2] == 'A') || (msg_str[2] == 'a')) &&
                     ((msg_str[3] == 'N') || (msg_str[3] == 'n')) &&
                     ((msg_str[4] == 'S') || (msg_str[4] == 's')) &&
                     ((msg_str[5] == 'F') || (msg_str[5] == 'f')) &&
                     ((msg_str[6] == 'E') || (msg_str[6] == 'e')) &&
                     ((msg_str[7] == 'R') || (msg_str[7] == 'r')) &&
                     ((msg_str[9] == 'E') || (msg_str[9] == 'e')) &&
                     ((msg_str[10] == 'N') || (msg_str[10] == 'n')) &&
                     ((msg_str[11] == 'C') || (msg_str[11] == 'c')) &&
                     ((msg_str[12] == 'O') || (msg_str[12] == 'o')) &&
                     ((msg_str[13] == 'D') || (msg_str[13] == 'd')) &&
                     ((msg_str[14] == 'I') || (msg_str[14] == 'i')) &&
                     ((msg_str[15] == 'N') || (msg_str[15] == 'n')) &&
                     ((msg_str[16] == 'G') || (msg_str[16] == 'g')))
                 {
                    int i = 18;

                    while ((i < read_length) &&
                           ((msg_str[i] == ' ') || (msg_str[i] == '\t')))
                    {
                       i++;
                    }
                    if ((i + 7) < read_length)
                    {
                       /* Chunked */
                       if (((msg_str[i] == 'C') || (msg_str[i] == 'c')) &&
                           ((msg_str[i + 1] == 'H') || (msg_str[i + 1] == 'h')) &&
                           ((msg_str[i + 2] == 'U') || (msg_str[i + 2] == 'u')) &&
                           ((msg_str[i + 3] == 'N') || (msg_str[i + 3] == 'n')) &&
                           ((msg_str[i + 4] == 'K') || (msg_str[i + 4] == 'k')) &&
                           ((msg_str[i + 5] == 'E') || (msg_str[i + 5] == 'e')) &&
                           ((msg_str[i + 6] == 'D') || (msg_str[i + 6] == 'd')))
                       {
                          hmr.chunked = YES;
                       }
                    }
                 }
                 /* Last-modified: */
            else if ((hmr.date != -1) &&
                     (read_length > 14) && (msg_str[13] == ':') &&
                     (msg_str[4] == '-') &&
                     ((msg_str[0] == 'L') || (msg_str[0] == 'l')) &&
                     ((msg_str[1] == 'a') || (msg_str[1] == 'A')) &&
                     ((msg_str[2] == 's') || (msg_str[2] == 'S')) &&
                     ((msg_str[3] == 't') || (msg_str[3] == 'T')) &&
                     ((msg_str[5] == 'M') || (msg_str[5] == 'm')) &&
                     ((msg_str[6] == 'o') || (msg_str[6] == 'O')) &&
                     ((msg_str[7] == 'd') || (msg_str[7] == 'D')) &&
                     ((msg_str[8] == 'i') || (msg_str[8] == 'I')) &&
                     ((msg_str[9] == 'f') || (msg_str[9] == 'F')) &&
                     ((msg_str[10] == 'i') || (msg_str[10] == 'I')) &&
                     ((msg_str[11] == 'e') || (msg_str[11] == 'E')) &&
                     ((msg_str[12] == 'd') || (msg_str[12] == 'D')))
                 {
                    int i = 14;

                    while ((i < read_length) &&
                           ((msg_str[i] == ' ') || (msg_str[i] == '\t')))
                    {
                       i++;
                    }
                    if (i < read_length)
                    {
                       hmr.date = datestr2unixtime(&msg_str[i]);
                    }
                 }
#ifdef _WITH_EXTRA_CHECK
                 /* ETag: */
            else if ((read_length > 5) && (msg_str[4] == ':') &&
                     ((msg_str[0] == 'E') || (msg_str[0] == 'e')) &&
                     ((msg_str[1] == 'T') || (msg_str[1] == 't')) &&
                     ((msg_str[2] == 'a') || (msg_str[2] == 'A')) &&
                     ((msg_str[3] == 'g') || (msg_str[3] == 'G')))
                 {
                    int i = 5;

                    while ((i < read_length) &&
                           ((msg_str[i] == ' ') || (msg_str[i] == '\t')))
                    {
                       i++;
                    }
                    store_http_etag(i, read_length);
                 }
#endif
                 /* Allow: */
            else if ((read_length > 6) && (msg_str[5] == ':') &&
                     ((msg_str[0] == 'A') || (msg_str[0] == 'a')) &&
                     ((msg_str[1] == 'l') || (msg_str[1] == 'L')) &&
                     ((msg_str[2] == 'l') || (msg_str[2] == 'L')) &&
                     ((msg_str[3] == 'o') || (msg_str[3] == 'O')) &&
                     ((msg_str[4] == 'w') || (msg_str[4] == 'W')))
                 {
                    int i = 6;

                    while ((i < read_length) &&
                           ((msg_str[i] == ' ') || (msg_str[i] == '\t')))
                    {
                       i++;
                    }
                    store_http_options(i, read_length);
                 }
         } /* for (;;) */

         if ((ret_bytes_buffered != NULL) && (bytes_buffered > read_length))
         {
            *ret_bytes_buffered = bytes_buffered - read_length - 1;
            if (msg_str[0] != '\0')
            {
               (void)memmove(msg_str, msg_str + read_length + 1,
                             *ret_bytes_buffered);
            }
         }
         if ((read_length == 1) && (msg_str[0] == '\0') && (msg_str[1] == '\n'))
         {
            (void)memmove(msg_str, msg_str + 1 + 1, (bytes_buffered - 1 - 1));
            (void)read_msg(NULL, -2, line);
         }
      }
   }
   else if (bytes_buffered == 0)
        {
           if (hmr.retries == 0)
           {
              hmr.close = YES;
              if ((status_code = check_connection()) == CONNECTION_REOPENED)
              {
                 trans_log(DEBUG_SIGN, __FILE__, __LINE__, "get_http_reply", NULL,
                           _("Reconnected. [%d]"), line);
                 hmr.retries = 1;
              }
           }
           else
           {
              timeout_flag = NEITHER;
              trans_log(ERROR_SIGN,  __FILE__, __LINE__, "get_http_reply", NULL,
                        _("Remote hang up. [%d]"), line);
              status_code = INCORRECT;
           }
        }

#ifdef DEBUG
   if (status_code == INCORRECT)
   {
      trans_log(DEBUG_SIGN, __FILE__, __LINE__, "get_http_reply", msg_str,
                _("Returning INCORRECT (bytes_buffered = %d) [%d]"),
                bytes_buffered, line);
   }
#endif
   return(status_code);
}


/*----------------------------- read_msg() ------------------------------*/
/*                             ------------                              */
/* Reads blockwise from http_fd until it has read one complete line.     */
/* From this line it removes the \r\n and inserts a \0 and returns the   */
/* the number of bytes it buffered. The number of bytes in the line are  */
/* returned by read_length.                                              */
/*-----------------------------------------------------------------------*/
static int
read_msg(int *read_length, int offset, int line)
{
   static int  bytes_buffered;
   static char *read_ptr = NULL;
   int         status;
   fd_set      rset;

   if (read_length == NULL)
   {
      if (read_ptr != NULL)
      {
         read_ptr += offset;
      }
      return(0);
   }

   *read_length = 0;
   if (hmr.bytes_read == 0)
   {
      bytes_buffered = 0;
   }
   else
   {
      (void)memmove(msg_str, read_ptr, hmr.bytes_read);
      bytes_buffered = hmr.bytes_read;
      read_ptr = msg_str;
   }

   FD_ZERO(&rset);
   for (;;)
   {
      if (hmr.bytes_read <= 0)
      {
#ifdef WITH_SSL
         if ((ssl_con != NULL) && (SSL_pending(ssl_con)))
         {
            if ((hmr.bytes_read = SSL_read(ssl_con,
                                           &msg_str[bytes_buffered],
                                           (MAX_RET_MSG_LENGTH - bytes_buffered))) < 1)
            {
               if (hmr.bytes_read == 0)
               {
                  return(0);
               }
               else
               {
                  if ((status = SSL_get_error(ssl_con,
                                              hmr.bytes_read)) == SSL_ERROR_SYSCALL)
                  {
                     if (errno == ECONNRESET)
                     {
                        timeout_flag = CON_RESET;
                     }
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_msg", NULL,
                               _("SSL_read() error (after reading %d bytes) [%d] : %s"),
                               bytes_buffered, line, strerror(errno));
                  }
                  else
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_msg", NULL,
                               _("SSL_read() error (after reading %d bytes) (%d) [%d]"),
                               bytes_buffered, status, line);
                  }
                  hmr.bytes_read = 0;
                  return(INCORRECT);
               }
            }
# ifdef WITH_TRACE
            trace_log(NULL, 0, BIN_CMD_R_TRACE,
                      &msg_str[bytes_buffered], hmr.bytes_read, NULL);
# endif
            read_ptr = &msg_str[bytes_buffered];
            bytes_buffered += hmr.bytes_read;
         }
         else
         {
#endif
            /* Initialise descriptor set. */
            FD_SET(http_fd, &rset);
            timeout.tv_usec = 0L;
            timeout.tv_sec = transfer_timeout;

            /* Wait for message x seconds and then continue. */
            status = select(http_fd + 1, &rset, NULL, NULL, &timeout);

            if (status == 0)
            {
               /* Timeout has arrived. */
               timeout_flag = ON;
               hmr.bytes_read = 0;
               return(INCORRECT);
            }
            else if (FD_ISSET(http_fd, &rset))
                 {
#ifdef WITH_SSL
                    if (ssl_con == NULL)
                    {
#endif
                       if ((hmr.bytes_read = read(http_fd, &msg_str[bytes_buffered],
                                                  (MAX_RET_MSG_LENGTH - bytes_buffered))) < 1)
                       {
                          if (hmr.bytes_read == 0)
                          {
#ifdef WITH_TRACE
                             trace_log(__FILE__, __LINE__, R_TRACE, NULL, 0,
                                       "read_msg(): 0 bytes read");
#endif
                             return(0);
                          }
                          else
                          {
                             if (errno == ECONNRESET)
                             {
                                timeout_flag = CON_RESET;
                             }
                             trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_msg", NULL,
                                       _("read() error (after reading %d bytes) [%d] : %s"),
                                       bytes_buffered, line, strerror(errno));
                             hmr.bytes_read = 0;
                             return(INCORRECT);
                          }
                       }
#ifdef WITH_SSL
                    }
                    else
                    {
                       int tmp_errno;

                       /*
                        * Remember we have set SSL_MODE_AUTO_RETRY. This
                        * means the SSL lib may do several read() calls. We
                        * just assured one read() with select(). So lets
                        * set an an alarm since we might block on subsequent
                        * calls to read(). It might be better when we
                        * reimplement this without SSL_MODE_AUTO_RETRY
                        * and handle SSL_ERROR_WANT_READ ourself.
                        */
                       if (sigsetjmp(env_alrm, 1) != 0)
                       {
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_msg", NULL,
                                    _("SSL_read() timeout (%ld)"),
                                    transfer_timeout);
                          timeout_flag = ON;
                          return(INCORRECT);
                       }
                       (void)alarm(transfer_timeout);
                       hmr.bytes_read = SSL_read(ssl_con,
                                                 &msg_str[bytes_buffered],
                                                 (MAX_RET_MSG_LENGTH - bytes_buffered));
                       tmp_errno = errno;
                       (void)alarm(0);
                       if (hmr.bytes_read < 1)
                       {
                          if (hmr.bytes_read == 0)
                          {
# ifdef WITH_TRACE
                             trace_log(__FILE__, __LINE__, R_TRACE, NULL, 0,
                                       "read_msg(): 0 bytes read");
# endif
                             return(0);
                          }
                          else
                          {
                             if ((status = SSL_get_error(ssl_con,
                                                         hmr.bytes_read)) == SSL_ERROR_SYSCALL)
                             {
                                if (tmp_errno == ECONNRESET)
                                {
                                   timeout_flag = CON_RESET;
                                }
                                trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_msg", NULL,
                                          _("SSL_read() error (after reading %d bytes) [%d] : %s"),
                                          bytes_buffered, line,
                                          strerror(tmp_errno));
                             }
                             else
                             {
                                trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_msg", NULL,
                                          _("SSL_read() error (after reading %d bytes) (%d) [%d]"),
                                          bytes_buffered, status, line);
                             }
                             hmr.bytes_read = 0;
                             return(INCORRECT);
                          }
                       }
                    }
#endif
#ifdef WITH_TRACE
                    trace_log(NULL, 0, BIN_CMD_R_TRACE,
                              &msg_str[bytes_buffered], hmr.bytes_read, NULL);
                    trace_log(__FILE__, __LINE__, R_TRACE, NULL, 0,
                              "read_msg(): %d bytes read", hmr.bytes_read);
#endif
                    read_ptr = &msg_str[bytes_buffered];
                    bytes_buffered += hmr.bytes_read;
                 }
            else if (status < 0)
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_msg", NULL,
                              _("select() error [%d] : %s"),
                              line, strerror(errno));
                    return(INCORRECT);
                 }
                 else
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_msg", NULL,
                              _("Unknown condition. [%d]"), line);
                    return(INCORRECT);
                 }
#ifdef WITH_SSL
         }
#endif
      }

      /* Evaluate what we have read. */
      do
      {
         if (*read_ptr == '\n')
         {
            if (*(read_ptr - 1) == '\r')
            {
               *(read_ptr - 1) = '\0';
               hmr.bytes_read--;
            }
            else
            {
               *read_ptr = '\0';
            }
            *read_length = read_ptr - msg_str;
            read_ptr++;
#ifdef WITH_TRACE
            trace_log(__FILE__, __LINE__, R_TRACE, msg_str, *read_length, NULL);
#endif
            return(bytes_buffered);
         }
         read_ptr++;
         hmr.bytes_read--;
      } while (hmr.bytes_read > 0);
   } /* for (;;) */
}


/*----------------------------- flush_read() ----------------------------*/
/*                              ------------                             */
/* Some HTTP servers return warn/error information in human readable     */
/* form, that we do not need. We must however read the complete message, */
/* otherwise the command/reponce sequence will get mixed up.             */
/*-----------------------------------------------------------------------*/
static int
flush_read(void)
{
   off_t content_length;

   if (hmr.content_length == -1)
   {
      content_length = hmr.bytes_buffered;
   }
   else
   {
      content_length = hmr.content_length;
   }

   if (content_length != 0)
   {
      int   bytes_buffered,
            hunk_size;
      off_t total_read = 0;
      char  buffer[2048];

#ifdef WITH_TRACE
      trace_log(__FILE__, __LINE__, R_TRACE, NULL, 0,
# if SIZEOF_OFF_T == 4
                "Flush reading %ld bytes (bufferd bytes = %d).",
# else
                "Flush reading %lld bytes (bufferd bytes = %d).",
# endif
                (pri_off_t)content_length, hmr.bytes_buffered);
#endif

      while (total_read != content_length)
      {
         hunk_size = content_length - total_read;
         if (hunk_size > 2048)
         {
            hunk_size = 2048;
         }
#ifdef WITH_TRACE
         trace_log(__FILE__, __LINE__, R_TRACE, NULL, 0,
                   "Reading hunk size = %d bytes.", hunk_size);
#endif
         if ((bytes_buffered = http_read(buffer, hunk_size)) <= 0)
         {
#ifdef WITH_TRACE
            trace_log(__FILE__, __LINE__, R_TRACE, NULL, 0,
# if SIZEOF_OFF_T == 4
                      "No good read %d, flushed %ld bytes.",
# else
                      "No good read %d, flushed %lld bytes.",
# endif
                      bytes_buffered, (pri_off_t)total_read);
#endif
            return(NO);
         }
         total_read += bytes_buffered;
      }
#ifdef WITH_TRACE
      trace_log(__FILE__, __LINE__, R_TRACE, NULL, 0,
# if SIZEOF_OFF_T == 4
                "Flushed %ld bytes.", (pri_off_t)total_read);
# else
                "Flushed %lld bytes.", (pri_off_t)total_read);
# endif
#endif

      if ((bytes_buffered > 4) && (buffer[bytes_buffered - 1] == 10) &&
          (buffer[bytes_buffered - 2] == 13) &&
          (buffer[bytes_buffered - 3] == 10) &&
          (buffer[bytes_buffered - 4] == 13) &&
          (buffer[bytes_buffered - 5] == 48))
      {
         return(YES);
      }
   }
   return(NO);
}


/*-------------------------- read_last_chunk() --------------------------*/
static void
read_last_chunk(void)
{
   int  bytes_read;
   char buffer[5];

   if ((bytes_read = http_read(buffer, 5)) <= 0)
   {
#ifdef WITH_TRACE
      trace_log(__FILE__, __LINE__, R_TRACE, NULL, 0,
                "read_last_chunk(): No good read %d (%d).",
                bytes_read, hmr.bytes_buffered);
#endif
   }
   else
   {
#ifdef WITH_TRACE
      trace_log(__FILE__, __LINE__, R_TRACE, NULL, 0,
                "read_last_chunk(): Flushed %d bytes (%d).",
                bytes_read, hmr.bytes_buffered);
#endif
   }

   return;
}


#ifdef _WITH_EXTRA_CHECK
/*-------------------------- store_http_etag() --------------------------*/
static void
store_http_etag(int i, int read_length)
{
   if ((read_length > 2) && (msg_str[i] == 'W') && (msg_str[i + 1] == '/'))
   {
      hmr.http_weak_etag = YES;
      i += 2;
   }
   else
   {
      hmr.http_weak_etag = NO;
   }
   if (msg_str[i] == '"')
   {
      int j = 0;

      i++;
      while ((i < read_length) && (j < MAX_EXTRA_LS_DATA_LENGTH) &&
             (msg_str[i] != '\0') && (msg_str[i] != '"'))
      {
         hmr.http_etag[j] = msg_str[i];
         i++; j++;
      }
      if (msg_str[i] != '"')
      {
         if (j >= MAX_EXTRA_LS_DATA_LENGTH)
         {
            trans_log(DEBUG_SIGN, __FILE__, __LINE__, "store_http_etag", NULL,
                      _("Buffer for storing ETAG not long enough, may only be %d bytes long."),
                      MAX_EXTRA_LS_DATA_LENGTH);
         }
         else if (msg_str[i] == '\0')
              {
                 trans_log(DEBUG_SIGN, __FILE__, __LINE__, "store_http_etag", msg_str,
                           _("ETAG not terminated properly."));
              }
              else
              {
                 trans_log(DEBUG_SIGN, __FILE__, __LINE__, "store_http_etag", msg_str,
                           _("Unable to store ETAG. Premature end of buffer [i=%d read_length=%d]."),
                           i, read_length);
              }
         j = 0; /* Clear storage for ETAG. */
      }
      hmr.http_etag[j] = '\0';
   }
   else
   {
      hmr.http_etag[0] = '\0';
   }

   return;
}
#endif


/*------------------------- store_http_options() ------------------------*/
static void
store_http_options(int i, int read_length)
{
   while ((i < read_length) && (msg_str[i] != '\0'))
   {
      /* HEAD */
      if (((i + 4) < read_length) &&
          ((msg_str[i] == 'H') || (msg_str[i] == 'h')) &&
          ((msg_str[i + 1] == 'E') || (msg_str[i + 1] == 'e')) &&
          ((msg_str[i + 2] == 'A') || (msg_str[i + 2] == 'a')) &&
          ((msg_str[i + 3] == 'D') || (msg_str[i + 3] == 'd')) &&
          ((msg_str[i + 4] == ',') || (msg_str[i + 4] == '\0')))
      {
         hmr.http_options |= HTTP_OPTION_HEAD;
         i += 4;
      }
           /* GET */
      else if (((i + 3) < read_length) &&
               ((msg_str[i] == 'G') || (msg_str[i] == 'g')) &&
               ((msg_str[i + 1] == 'E') || (msg_str[i + 1] == 'e')) &&
               ((msg_str[i + 2] == 'T') || (msg_str[i + 2] == 't')) &&
               ((msg_str[i + 3] == ',') || (msg_str[i + 3] == '\0')))
           {
              hmr.http_options |= HTTP_OPTION_GET;
              i += 3;
           }
           /* PUT */
      else if (((i + 3) < read_length) &&
               ((msg_str[i] == 'P') || (msg_str[i] == 'p')) &&
               ((msg_str[i + 1] == 'U') || (msg_str[i + 1] == 'u')) &&
               ((msg_str[i + 2] == 'T') || (msg_str[i + 2] == 't')) &&
               ((msg_str[i + 3] == ',') || (msg_str[i + 3] == '\0')))
           {
              hmr.http_options |= HTTP_OPTION_PUT;
              i += 3;
           }
           /* MOVE */
      else if (((i + 4) < read_length) &&
               ((msg_str[i] == 'M') || (msg_str[i] == 'm')) &&
               ((msg_str[i + 1] == 'O') || (msg_str[i + 1] == 'o')) &&
               ((msg_str[i + 2] == 'V') || (msg_str[i + 2] == 'v')) &&
               ((msg_str[i + 3] == 'E') || (msg_str[i + 3] == 'e')) &&
               ((msg_str[i + 4] == ',') || (msg_str[i + 4] == '\0')))
           {
              hmr.http_options |= HTTP_OPTION_MOVE;
              i += 4;
           }
           /* POST */
      else if (((i + 4) < read_length) &&
               ((msg_str[i] == 'P') || (msg_str[i] == 'p')) &&
               ((msg_str[i + 1] == 'O') || (msg_str[i + 1] == 'o')) &&
               ((msg_str[i + 2] == 'S') || (msg_str[i + 2] == 's')) &&
               ((msg_str[i + 3] == 'T') || (msg_str[i + 3] == 't')) &&
               ((msg_str[i + 4] == ',') || (msg_str[i + 4] == '\0')))
           {
              hmr.http_options |= HTTP_OPTION_POST;
              i += 4;
           }
           /* DELETE */
      else if (((i + 6) < read_length) &&
               ((msg_str[i] == 'D') || (msg_str[i] == 'd')) &&
               ((msg_str[i + 1] == 'E') || (msg_str[i + 1] == 'e')) &&
               ((msg_str[i + 2] == 'L') || (msg_str[i + 2] == 'l')) &&
               ((msg_str[i + 3] == 'E') || (msg_str[i + 3] == 'e')) &&
               ((msg_str[i + 4] == 'T') || (msg_str[i + 4] == 't')) &&
               ((msg_str[i + 5] == 'E') || (msg_str[i + 5] == 'e')) &&
               ((msg_str[i + 6] == ',') || (msg_str[i + 6] == '\0')))
           {
              hmr.http_options |= HTTP_OPTION_DELETE;
              i += 6;
           }
           /* OPTIONS */
      else if (((i + 7) < read_length) &&
               ((msg_str[i] == 'O') || (msg_str[i] == 'o')) &&
               ((msg_str[i + 1] == 'P') || (msg_str[i + 1] == 'p')) &&
               ((msg_str[i + 2] == 'T') || (msg_str[i + 2] == 't')) &&
               ((msg_str[i + 3] == 'I') || (msg_str[i + 3] == 'i')) &&
               ((msg_str[i + 4] == 'O') || (msg_str[i + 4] == 'o')) &&
               ((msg_str[i + 5] == 'N') || (msg_str[i + 5] == 'n')) &&
               ((msg_str[i + 6] == 'S') || (msg_str[i + 6] == 's')) &&
               ((msg_str[i + 7] == ',') || (msg_str[i + 7] == '\0')))
           {
              hmr.http_options |= HTTP_OPTION_OPTIONS;
              i += 7;
           }
           else /* Ignore any other options. */
           {
              while ((i < read_length) &&
                     (msg_str[i] != ',') && (msg_str[i] != '\0'))
              {
                 i++;
              }
           }

      if (msg_str[i] == ',')
      {
         i++;
         while ((i < read_length) &&
                ((msg_str[i] == ' ') || (msg_str[i] == '\t')))
         {
            i++;
         }
      }
   }

   return;
}


#ifdef WITH_SSL
/*+++++++++++++++++++++++++++++ sig_handler() +++++++++++++++++++++++++++*/
static void
sig_handler(int signo)
{
   siglongjmp(env_alrm, 1);
}
#endif
