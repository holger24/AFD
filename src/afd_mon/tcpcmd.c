/*
 *  tcpcmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   tcpcmd - commands to send files via TCP/IP
 **
 ** SYNOPSIS
 **   int tcp_connect(char *hostname,
 **                   int  port,
 **                   int  sending_logdata,
 **                   int  encrypt)
 **   int tcp_cmd(SSL *ssl, char *fmt, ...)
 **   int tcp_quit(void)
 **
 ** DESCRIPTION
 **   tcpcmd provides a set of commands to communicate with an TCP
 **   server via BSD sockets.
 **
 ** RETURN VALUES
 **   Returns SUCCESS when successful. When an error has occurred
 **   it will either return INCORRECT or the three digit TCP reply
 **   code when the reply of the server does not conform to the
 **   one expected. The complete reply string of the TCP server
 **   is returned to 'msg_str'. 'timeout_flag' is just a flag to
 **   indicate that the 'tcp_timeout' time has been reached.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.08.1998 H.Kiehl Created
 **   12.04.2022 H.Kiehl Add TLS support.
 **
 */
DESCR__E_M3


#include <stdio.h>            /* fprintf(), fdopen(), fclose()           */
#include <stdarg.h>           /* va_start(), va_arg(), va_end()          */
#include <string.h>           /* memset(), memcpy(), strcpy()            */
#include <stdlib.h>           /* exit()                                  */
#include <ctype.h>            /* isdigit()                               */
#include <sys/types.h>        /* fd_set                                  */
#include <sys/time.h>         /* struct timeval                          */
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>      /* socket(), shutdown(), bind(), accept(), */
                              /* setsockopt()                            */
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>      /* struct in_addr, sockaddr_in, htons()    */
#endif
#if defined (_WITH_TOS) && defined (IP_TOS)
# ifdef HAVE_NETINET_IN_SYSTM_H
#  include <netinet/in_systm.h>
# endif
# ifdef HAVE_NETINET_IP_H
#  include <netinet/ip.h>     /* IPTOS_LOWDELAY, IPTOS_THROUGHPUT        */
# endif
#endif
#ifdef HAVE_NETDB_H
# include <netdb.h>           /* struct hostent, gethostbyname()         */
#endif
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>       /* inet_addr()                             */
#endif
#ifdef WITH_SSL
# include <openssl/ssl.h>
# include <openssl/err.h>
# include <signal.h>          /* signal()                                */
# include <setjmp.h>          /* sigsetjmp(), siglongjmp()               */
#endif
#include <unistd.h>           /* select(), write(), read(), close(),     */
                              /* alarm()                                 */
#include <errno.h>
#include "mondefs.h"


/* External global variables. */
extern int                timeout_flag;
#ifdef WITH_SSL
extern SSL                *ssl_con;
#endif
extern char               msg_str[];
#ifdef LINUX
extern char               *h_errlist[];  /* for gethostbyname()      */
extern int                h_nerr;        /* for gethostbyname()      */
#endif
extern int                sock_fd;
extern long               tcp_timeout;

/* Local global variables */
#ifdef WITH_SSL
static SSL_CTX            *ssl_ctx = NULL;
#endif
static sigjmp_buf         env_alrm;
static struct sockaddr_in ctrl;
static struct timeval     timeout;

/* Local functions */
static int                check_reply(int, ...),
                          get_reply(void),
                          tcpcmd(char *, ...);
#ifdef WITH_SSL
static char               *ssl_error_msg(char *, SSL *, int *, int, char *);
static void               sig_handler(int);
#endif


/*########################## tcp_connect() ##############################*/
int
tcp_connect(char *hostname, int port, int sending_logdata, int encrypt)
{
   int                     reply;
   my_socklen_t            length;
   struct sockaddr_in      sin;
   register struct hostent *p_host = NULL;

   (void)memset((struct sockaddr *) &sin, 0, sizeof(sin));
   if ((sin.sin_addr.s_addr = inet_addr(hostname)) != -1)
   {
      sin.sin_family = AF_INET;
   }
   else
   {
      if ((p_host = gethostbyname(hostname)) == NULL)
      {
#if !defined (_HPUX) && !defined (_SCO)
         if (h_errno != 0)
	 {
# ifdef LINUX
	    if ((h_errno > 0) && (h_errno < h_nerr))
	    {
               mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                       "Failed to gethostbyname() %s : %s",
                       hostname, h_errlist[h_errno]);
	    }
	    else
	    {
               mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                       "Failed to gethostbyname() %s (h_errno = %d) : %s",
                       hostname, h_errno, strerror(errno));
	    }
# else
          mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                  "Failed to gethostbyname() %s (h_errno = %d) : %s",
                  hostname, h_errno, strerror(errno));
# endif
	 }
	 else
	 {
#endif /* !_HPUX && !_SCO */
          mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                  "Failed to gethostbyname() %s : %s",
                  hostname, strerror(errno));
#if !defined (_HPUX) && !defined (_SCO)
	 }
#endif
         return(INCORRECT);
      }
      sin.sin_family = p_host->h_addrtype;

      /* Copy IP number to socket structure */
      memcpy((char *)&sin.sin_addr, p_host->h_addr, p_host->h_length);
   }

   if ((sock_fd = socket(sin.sin_family, SOCK_STREAM, IPPROTO_TCP)) < 0)
   {
      mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
              "socket() error : %s", strerror(errno));
      return(INCORRECT);
   }

   sin.sin_port = htons((u_short)port);

#ifdef _TRY_ALL_HOSTS
   while (connect(sock_fd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
   {
      if (p_host == NULL)
      {
         mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                 "Failed to connect() to %s : %s", hostname, strerror(errno));
         (void)close(sock_fd);
         return(INCORRECT);
      }
      p_host->h_addr_list++;
      if (p_host->h_addr_list[0] == NULL)
      {
         mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                 "Failed to connect() to %s. Have tried all hosts in h_addr_list.",
                 hostname);
         (void)close(sock_fd);
         return(INCORRECT);
      }
      memcpy((char *)&sin.sin_addr, p_host->h_addr_list[0], p_host->h_length);
      if (close(sock_fd) == -1)
      {
         mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, NULL,
                 "close() error : %s", strerror(errno));
      }
      if ((sock_fd = socket(sin.sin_family, SOCK_STREAM, IPPROTO_TCP)) < 0)
      {
         mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                 "socket() error : %s", strerror(errno));
         (void)close(sock_fd);
         return(INCORRECT);
      }
   }
#else
   if (connect(sock_fd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
   {
#ifdef ETIMEDOUT
      if (errno == ETIMEDOUT)
      {
         timeout_flag = ON;
      }
#endif
      mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
              "Failed to connect() to %s : %s", hostname, strerror(errno));
      (void)close(sock_fd);
      return(INCORRECT);
   }
#endif

   length = sizeof(ctrl);
   if (getsockname(sock_fd, (struct sockaddr *)&ctrl, &length) < 0)
   {
      mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
              "getsockname() error : %s", strerror(errno));
      (void)close(sock_fd);
      return(INCORRECT);
   }

   if (sending_logdata == YES)
   {
      reply = 1;
      if (setsockopt(sock_fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&reply,
                     sizeof(reply)) < 0)
      {
         mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, NULL,
                 "tcp_connect(): setsockopt() SO_KEEPALIVE error : %s",
                 strerror(errno));
      }
   }
#if defined (_WITH_TOS) && defined (IP_TOS) && defined (IPTOS_LOWDELAY) && defined (IPTOS_THROUGHPUT)
   if (sending_logdata == YES)
   {
      reply = IPTOS_THROUGHPUT;
   }
   else
   {
      reply = IPTOS_LOWDELAY;
   }
   if (setsockopt(sock_fd, IPPROTO_IP, IP_TOS, (char *)&reply,
                  sizeof(reply)) < 0)
   {
      mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, NULL,
              "tcp_connect(): setsockopt() IP_TOS error : %s",
              strerror(errno));
   }
#endif

#ifdef WITH_SSL
   if (encrypt)
   {
      if (ssl_ctx != NULL)
      {
         SSL_CTX_free(ssl_ctx);
      }
      SSLeay_add_ssl_algorithms();
      if ((ssl_ctx = (SSL_CTX *)SSL_CTX_new(afd_encrypt_client_method())) == NULL)
      {
         mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                 _("SSL_CTX_new() unable to create a new SSL context structure."));
         (void)close(sock_fd);
         return(INCORRECT);
      }
      SSL_CTX_set_mode(ssl_ctx, SSL_MODE_AUTO_RETRY);
      SSL_CTX_set_cipher_list(ssl_ctx, NULL);
      SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);

      ssl_con = (SSL *)SSL_new(ssl_ctx);
      SSL_set_connect_state(ssl_con);
      SSL_set_fd(ssl_con, sock_fd);

      if (signal(SIGALRM, sig_handler) == SIG_ERR)
      {
         mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                 _("Failed to set signal handler : %s"), strerror(errno));
         (void)close(sock_fd);
         return(INCORRECT);
      }
      if (sigsetjmp(env_alrm, 1) != 0)
      {
         mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                 _("SSL_connect() timeout (%lds)"), tcp_timeout);
         timeout_flag = ON;
         (void)close(sock_fd);
         return(INCORRECT);
      }
      (void)alarm(tcp_timeout);
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
            mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, msg_str,
                    "SSL/TSL connection to server `%s' at port %d failed.",
                    hostname, port);
            SSL_free(ssl_con);
            ssl_con = NULL;
            (void)close(sock_fd);
            sock_fd = -1;
            return(INCORRECT);
      }
   }
#endif

   if ((reply = get_reply()) < 0)
   {
#ifdef WITH_SSL
      SSL_free(ssl_con);
      ssl_con = NULL;
#endif
      return(INCORRECT);
   }
   if (check_reply(2, reply, 220) < 0)
   {
#ifdef WITH_SSL
      SSL_free(ssl_con);
      ssl_con = NULL;
#endif
      return(reply);
   }

   return(SUCCESS);
}


/*############################## tcp_quit() #############################*/
int
tcp_quit(void)
{
   if (sock_fd != -1)
   {
      /*
       * If timeout_flag is ON, lets NOT check the reply from
       * the QUIT command. Else we are again waiting 'tcp_timeout'
       * seconds for the reply!
       */
      if (timeout_flag == OFF)
      {
         int reply;

         (void)tcpcmd("QUIT");
         if ((reply = get_reply()) < 0)
         {
#ifdef WITH_SSL
            if (ssl_con != NULL)
            {
               SSL_free(ssl_con);
               ssl_con = NULL;
            }
#endif
            return(INCORRECT);
         }

         /*
          * NOTE: Lets not count the 421 warning as an error.
          */
         if (check_reply(3, reply, 221, 421) < 0)
         {
#ifdef WITH_SSL
            if (ssl_con != NULL)
            {
               SSL_free(ssl_con);
               ssl_con = NULL;
            }
#endif
            return(reply);
         }

         if (shutdown(sock_fd, 1) < 0)
         {
            mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, NULL,
                    "shutdown() error : %s", strerror(errno));
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
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ tcpcmd() ++++++++++++++++++++++++++++++*/
static int
tcpcmd(char *fmt, ...)
{
   int     length;
   char    buf[MAX_LINE_LENGTH + 1];
   va_list ap;

   va_start(ap, fmt);
   length = vsnprintf(buf, MAX_LINE_LENGTH, fmt, ap);
   va_end(ap);
   if (length > MAX_LINE_LENGTH)
   {
      mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, msg_str,
              "tcp_cmd(): Command to long (%d > %d)", length, MAX_LINE_LENGTH);
      return(INCORRECT);
   }
   buf[length] = '\r';
   buf[length + 1] = '\n';
   length += 2;
#ifdef WITH_SSL
   if (ssl_con == NULL)
   {
#endif
      if (write(sock_fd, buf, length) != length)
      {
         mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, msg_str,
                 "tcp_cmd(): write() error : %s", strerror(errno));
         return(INCORRECT);
      }
#ifdef WITH_SSL
   }
   else
   {
      if (ssl_write(ssl_con, buf, length) != length)
      {
         return(INCORRECT);
      }
   }
#endif
   return(length);
}


/*++++++++++++++++++++++++++++++ get_reply() ++++++++++++++++++++++++++++*/
static int
get_reply(void)
{
   for (;;)
   {
      if (read_msg() == INCORRECT)
      {
         return(INCORRECT);
      }

      /*
       * Lets ignore anything we get from the remote site
       * not starting with three digits and a dash.
       */
      if ((isdigit((int)msg_str[0]) != 0) && (isdigit((int)msg_str[1]) != 0) &&
          (isdigit((int)msg_str[2]) != 0) && (msg_str[3] != '-'))
      {
         break;
      }
   }

   return(((msg_str[0] - '0') * 100) + ((msg_str[1] - '0') * 10) +
          (msg_str[2] - '0'));
}


/*############################# read_msg() ##############################*/
int
read_msg(void)
{
   static int  bytes_buffered,
               bytes_read = 0;
   static char *read_ptr = NULL;
   int         status;
   fd_set      rset;

   if (bytes_read == 0)
   {
      bytes_buffered = 0;
   }
   else
   {
      (void)memmove(msg_str, read_ptr + 1, bytes_read);
      bytes_buffered = bytes_read;
      read_ptr = msg_str;
   }
   FD_ZERO(&rset);

   for (;;)
   {
      if (bytes_read <= 0)
      {
#ifdef WITH_SSL
try_again_read_msg:
#endif

         /* Initialise descriptor set */
         FD_SET(sock_fd, &rset);
         timeout.tv_usec = 0L;
         timeout.tv_sec = tcp_timeout;

         /* Wait for message x seconds and then continue. */
         status = select(sock_fd + 1, &rset, NULL, NULL, &timeout);

         if (status == 0)
         {
            /* timeout has arrived */
            timeout_flag = ON;
            bytes_read = 0;
            return(INCORRECT);
         }
         else if (FD_ISSET(sock_fd, &rset))
              {
#ifdef WITH_SSL
                 if (ssl_con == NULL)
                 {
#endif
                    if ((bytes_read = read(sock_fd, &msg_str[bytes_buffered],
                                           (MAX_RET_MSG_LENGTH - bytes_buffered))) < 1)
                    {
                       if (bytes_read == 0)
                       {
                          mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                                  "Remote hang up.");
                          timeout_flag = NEITHER;
                       }
                       else
                       {
                          if (errno == ECONNRESET)
                          {
                             timeout_flag = CON_RESET;
                          }
                          mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                                  "read() error (after reading %d Bytes) : %s",
                                  bytes_buffered, strerror(errno));
                          bytes_read = 0;
                       }
                       return(INCORRECT);
                    }
#ifdef WITH_SSL
                 }
                 else
                 {
                    if ((bytes_read = SSL_read(ssl_con,
                                               &msg_str[bytes_buffered],
                                               (MAX_RET_MSG_LENGTH - bytes_buffered))) < 1)
                    {
                       if (bytes_read == 0)
                       {
                          mon_log(INFO_SIGN, __FILE__, __LINE__, 0L, NULL,
                                  _("Remote hang up."));
                          timeout_flag = NEITHER;
                       }
                       else
                       {
                          int ssl_ret;

                          ssl_error_msg("SSL_read", ssl_con, &ssl_ret,
                                        bytes_read, msg_str);
                          mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, msg_str,
                                  _("SSL_read() error (after reading %d bytes) (%d)"),
                                  bytes_buffered, status);

                          /*
                           * Some servers drop back to clear text.
                           * Check if that is the case and try read
                           * the clear text responce.
                           */
                          if (ssl_ret == SSL_ERROR_SSL)
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
                             goto try_again_read_msg;
                          }
                          bytes_read = 0;
                       }
                       return(INCORRECT);
                    }
                 }
#endif
                 read_ptr = &msg_str[bytes_buffered];
                 bytes_buffered += bytes_read;
              }
         else if (status < 0)
              {
                 mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                         "select() error : %s", strerror(errno));
                 exit(INCORRECT);
              }
              else
              {
                 mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                         "Unknown condition.");
                 exit(INCORRECT);
              }
      }

      /* Evaluate what we have read. */
      do
      {
         if ((*read_ptr == '\n') && (*(read_ptr - 1) == '\r'))
         {                                                                
            *(read_ptr - 1) = '\0';
            bytes_read--;
            return(bytes_buffered);
         }
         read_ptr++;
         bytes_read--;
      } while(bytes_read > 0);
   } /* for (;;) */
}


/*+++++++++++++++++++++++++++++ check_reply() +++++++++++++++++++++++++++*/
static int
check_reply(int count, ...)
{
   int     i,
           reply;
   va_list ap;

   va_start(ap, count);
   reply = va_arg(ap, int);
   for (i = 1; i < count; i++)
   {
      if (reply == va_arg(ap, int))
      {
         va_end(ap);
         return(SUCCESS);
      }
   }
   va_end(ap);

   return(INCORRECT);
}


#ifdef WITH_SSL
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


/*+++++++++++++++++++++++++++ ssl_error_msg() +++++++++++++++++++++++++++*/
static char *
ssl_error_msg(char *function, SSL *ssl, int *ssl_ret, int reply, char *msg_str)
{
   int len,
       *p_ret,
       ret;

   if (ssl_ret == NULL)
   {
      p_ret = &ret;
   }
   else
   {
      p_ret = ssl_ret;
   }
   *p_ret = SSL_get_error(ssl, reply);
   switch (*p_ret)
   {
      case SSL_ERROR_NONE :
         len = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                        _("%s error SSL_ERROR_NONE : The TLS/SSL I/O operation completed."),
                        function);
         break;

      case SSL_ERROR_ZERO_RETURN :
         len = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                        _("%s error SSL_ERROR_ZERO_RETURN : The TLS/SSL connection has been closed."),
                        function);
         break;

      case SSL_ERROR_WANT_WRITE :
         len = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                        _("%s error SSL_ERROR_WANT_WRITE : Operation not complete, try again later."),
                        function);
         break;

      case SSL_ERROR_WANT_READ :
         len = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                        _("%s error SSL_ERROR_WANT_READ : Operation not complete, try again later."),
                        function);
         break;

#ifdef SSL_ERROR_WANT_ACCEPT
      case SSL_ERROR_WANT_ACCEPT :
         len = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                        _("%s error SSL_ERROR_WANT_ACCEPT : Operation not complete, try again later."),
                        function);
         break;
#endif
      case SSL_ERROR_WANT_CONNECT :
         len = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                        _("%s error SSL_ERROR_WANT_CONNECT : Operation not complete, try again later."),
                        function);
         break;

      case SSL_ERROR_WANT_X509_LOOKUP :
         len = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                        _("%s error SSL_ERROR_WANT_X509_LOOKUP : Operation not complete, tray again."),
                        function);
         break;

      case SSL_ERROR_SYSCALL :
         {
            unsigned long queued;

            queued = ERR_get_error();
            if (queued == 0)
            {
               if (reply == 0)
               {
                  len = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                                 _("%s error SSL_ERROR_SYSCALL : Observed EOF which violates the protocol."),
                                  function);
               }
               else if (reply == -1)
                    {
                       len = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                                      _("%s error SSL_ERROR_SYSCALL : %s"),
                                      function, strerror(errno));
                    }
                    else
                    {
                        len = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                                       _("%s error SSL_ERROR_SYSCALL : No error queued."),
                                       function);
                    }
            }
            else
            {
               SSL_load_error_strings();
               len = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                              _("%s error SSL_ERROR_SYSCALL : %s"),
                              function, ERR_error_string(queued, NULL));
               ERR_free_strings();
            }
         }
         break;

      case SSL_ERROR_SSL :
         SSL_load_error_strings();
         len = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                        _("%s error SSL_ERROR_SSL : %s"),
                        function, ERR_error_string(ERR_get_error(), NULL));
         ERR_free_strings();
         break;

      default :
         len = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                        _("%s error unknown (%d)."), function, *p_ret);
   }
   if (len > MAX_RET_MSG_LENGTH)
   {
      len = MAX_RET_MSG_LENGTH;
   }
   return(msg_str + len);
}


/*++++++++++++++++++++++++++++++ sig_handler() ++++++++++++++++++++++++++*/
static void
sig_handler(int signo)
{
   siglongjmp(env_alrm, 1);
}
#endif
