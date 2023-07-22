/*
 *  common.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2004 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   common - functions that can be used for several protocols
 **
 ** SYNOPSIS
 **   int     command(int fd, char *fmt, ...)
 **   int     ssl_connect(int sock_fd, char *hostname,
 **                       char *func_name, int strict,
 **                       int legacy_renegotiation)
 **   ssize_t ssl_write(SSL *ssl, const char *buf, size_t count)
 **   char    *ssl_error_msg(char *function, SSL *ssl, int *ssl_ret,
 **                          int reply, char *msg_str)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.03.2004 H.Kiehl Created
 **   26.06.2005 H.Kiehl Don't show password during a trace.
 **   27.02.2013 H.Kiehl Added function connect_with_timeout().
 **   03.11.2018 H.Kiehl Implemented ServerNameIndication for TLS.
 **   17.10.2021 H.Kiehl Added test_command() which is very similar
 **                      to command() but does not treat a disconnect
 **                      by the remote server as an error in the log.
 **   19.03.2022 H.Kiehl Add strict TLS support.
 **   17.07.2022 H.Kiehl Add option to enable legacy renegotiation
 **                      for ssl_connect().
 */
DESCR__E_M3


#include <stdio.h>
#include <stdarg.h>       /* va_start(), va_end()                        */
#include <string.h>       /* memcpy(), strerror(), strdup()              */
#include <sys/types.h>    /* fd_set                                      */
#include <sys/time.h>     /* struct timeval                              */
#include <sys/stat.h>     /* S_ISUID, S_ISGID, etc                       */
#ifdef WITH_SSL
# include <stdlib.h>      /* malloc()                                    */
# include <setjmp.h>      /* sigsetjmp(), siglongjmp()                   */
# include <signal.h>      /* signal()                                    */
# include <openssl/crypto.h>
# include <openssl/x509.h>
# include <openssl/ssl.h>
# include <openssl/err.h>
#endif
#include <unistd.h>       /* write()                                     */
#include <errno.h>
#include "fddefs.h"
#include "commondefs.h"


/* External global variables. */
extern int        timeout_flag;
extern long       transfer_timeout;
#ifdef WITH_SSL
extern char       msg_str[];
extern SSL        *ssl_con;

/* Local global variables. */
static sigjmp_buf env_alrm;
static SSL_CTX    *ssl_ctx = NULL;

/* Local function prototypes. */
static void       sig_handler(int);
#endif /* WITH_SSL */



/*############################## command() ##############################*/
int
command(int fd, char *fmt, ...)
{
   int     length;
   char    buf[MAX_LINE_LENGTH + 1];
   va_list ap;
   char    *ptr,
           *ptr_start;

   va_start(ap, fmt);
   length = vsnprintf(buf, MAX_LINE_LENGTH, fmt, ap);
   va_end(ap);
   if (length > MAX_LINE_LENGTH)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "command", NULL,
                "Command to long (%d > %d)", length, MAX_LINE_LENGTH);
      return(INCORRECT);
   }
   buf[length] = '\r';
   buf[length + 1] = '\n';
   length += 2;
#ifdef WITH_SSL
   if (ssl_con == NULL)
   {
#endif
      if (write(fd, buf, length) != length)
      {
         char *sign;

         if ((errno == ECONNRESET) || (errno == EBADF) || (errno == EPIPE))
         {
            timeout_flag = CON_RESET;
            if (errno == EPIPE)
            {
               sign = INFO_SIGN;
            }
            else
            {
               sign = ERROR_SIGN;
            }
         }
         else
         {
            sign = ERROR_SIGN;
         }
         trans_log(sign, __FILE__, __LINE__, "command", NULL,
                   _("write() error : %s"), strerror(errno));
         ptr = buf;
         do
         {
            ptr_start = ptr;
            while ((*ptr != '\r') && (*ptr != '\n') && (ptr < &buf[length - 1]))
            {
               ptr++;
            }
            if ((*ptr == '\r') || (*ptr == '\n'))
            {
               *ptr = '\0';
               ptr++;
               while (((*ptr == '\n') || (*ptr == '\r')) && (ptr < &buf[length - 1]))
               {
                  ptr++;
               }
            }
            trans_log(DEBUG_SIGN, NULL, 0, "command", NULL, "%s", ptr_start);
         } while (ptr < &buf[length - 1]);

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
#ifdef WITH_TRACE
   ptr = buf;
   do
   {
      ptr_start = ptr;
      while ((*ptr != '\r') && (*(ptr + 1) != '\n') && (ptr < &buf[length - 1]))
      {
         ptr++;
      }
      if (*ptr == '\r')
      {
         *ptr = '\0';
      }
      if ((*(ptr + 2) == '\r') && (*(ptr + 3) == '\n'))
      {
         /* This is required by HTTP, meaning end of command. */
         if (((ptr_start + 5) < (buf + length)) &&
             (*ptr_start == 'P') && (*(ptr_start + 1) == 'A') &&
             (*(ptr_start + 2) == 'S') && (*(ptr_start + 3) == 'S') &&
             (*(ptr_start + 4) == ' '))
         {
            trace_log(NULL, 0, W_TRACE, NULL, 0, "PASS xxx<0D><0A><0D><0A>");
         }
         else
         {
            trace_log(NULL, 0, W_TRACE, NULL, 0, "%s<0D><0A><0D><0A>",
                      ptr_start);
         }
         ptr += 4;
      }
      else
      {
         if (((ptr_start + 5) < (buf + length)) &&
             (*ptr_start == 'P') && (*(ptr_start + 1) == 'A') &&
             (*(ptr_start + 2) == 'S') && (*(ptr_start + 3) == 'S') &&
             (*(ptr_start + 4) == ' '))
         {
            trace_log(NULL, 0, W_TRACE, NULL, 0, "PASS xxx<0D><0A>");
         }
         else
         {
            trace_log(NULL, 0, W_TRACE, NULL, 0, "%s<0D><0A>", ptr_start);
         }
         ptr += 2;
      }
   } while (ptr < &buf[length - 1]);
#endif
   return(SUCCESS);
}


/*############################ test_command() ###########################*/
int
test_command(int fd, char *fmt, ...)
{
   int     length;
   char    buf[MAX_LINE_LENGTH + 1];
   va_list ap;
   char    *ptr,
           *ptr_start;

   va_start(ap, fmt);
   length = vsnprintf(buf, MAX_LINE_LENGTH, fmt, ap);
   va_end(ap);
   if (length > MAX_LINE_LENGTH)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "command", NULL,
                "Command to long (%d > %d)", length, MAX_LINE_LENGTH);
      return(INCORRECT);
   }
   buf[length] = '\r';
   buf[length + 1] = '\n';
   length += 2;
#ifdef WITH_SSL
   if (ssl_con == NULL)
   {
#endif
      if (write(fd, buf, length) != length)
      {
         if ((errno == ECONNRESET) || (errno == EBADF) || (errno == EPIPE))
         {
            timeout_flag = CON_RESET;
         }
         trans_log(DEBUG_SIGN, __FILE__, __LINE__, "command", NULL,
                   _("write() error : %s"), strerror(errno));
         ptr = buf;
         do
         {
            ptr_start = ptr;
            while ((*ptr != '\r') && (*ptr != '\n') && (ptr < &buf[length - 1]))
            {
               ptr++;
            }
            if ((*ptr == '\r') || (*ptr == '\n'))
            {
               *ptr = '\0';
               ptr++;
               while (((*ptr == '\n') || (*ptr == '\r')) && (ptr < &buf[length - 1]))
               {
                  ptr++;
               }
            }
            trans_log(DEBUG_SIGN, NULL, 0, "command", NULL, "%s", ptr_start);
         } while (ptr < &buf[length - 1]);

         return(INCORRECT);
      }
#ifdef WITH_SSL
   }
   else
   {
      int     bytes_done;
      size_t  count = length;
      ssize_t bytes_total = 0;

      do
      {
         if ((bytes_done = SSL_write(ssl_con, buf + bytes_total, count)) <= 0)
         {
            int ret;

            ret = SSL_get_error(ssl_con, bytes_done);
            switch (ret)
            {
               case SSL_ERROR_WANT_READ : /* Renegotiation takes place. */
                  my_usleep(50000L);
                  break;

               case SSL_ERROR_SYSCALL :
                  if ((errno == ECONNRESET) || (errno == EBADF) ||
                      (errno == EPIPE))
                  {
                     timeout_flag = CON_RESET;
                  }
                  trans_log(DEBUG_SIGN, __FILE__, __LINE__, "ssl_write", NULL,
                            _("SSL_write() error (%d) : %s"),
                            ret, strerror(errno));
                  return(INCORRECT);

               default : /* Error */
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssl_write", NULL,
                            _("SSL_write() error (%d)"), ret);
                  return(INCORRECT);
            }
         }
         else
         {
            count -= bytes_done;
            bytes_total += bytes_done;
         }
      } while (count > 0);

      if (bytes_total != length)
      {
         return(INCORRECT);
      }
   }
#endif
#ifdef WITH_TRACE
   ptr = buf;
   do
   {
      ptr_start = ptr;
      while ((*ptr != '\r') && (*(ptr + 1) != '\n') && (ptr < &buf[length - 1]))
      {
         ptr++;
      }
      if (*ptr == '\r')
      {
         *ptr = '\0';
      }
      if ((*(ptr + 2) == '\r') && (*(ptr + 3) == '\n'))
      {
         /* This is required by HTTP, meaning end of command. */
         if (((ptr_start + 5) < (buf + length)) &&
             (*ptr_start == 'P') && (*(ptr_start + 1) == 'A') &&
             (*(ptr_start + 2) == 'S') && (*(ptr_start + 3) == 'S') &&
             (*(ptr_start + 4) == ' '))
         {
            trace_log(NULL, 0, W_TRACE, NULL, 0, "PASS xxx<0D><0A><0D><0A>");
         }
         else
         {
            trace_log(NULL, 0, W_TRACE, NULL, 0, "%s<0D><0A><0D><0A>",
                      ptr_start);
         }
         ptr += 4;
      }
      else
      {
         if (((ptr_start + 5) < (buf + length)) &&
             (*ptr_start == 'P') && (*(ptr_start + 1) == 'A') &&
             (*(ptr_start + 2) == 'S') && (*(ptr_start + 3) == 'S') &&
             (*(ptr_start + 4) == ' '))
         {
            trace_log(NULL, 0, W_TRACE, NULL, 0, "PASS xxx<0D><0A>");
         }
         else
         {
            trace_log(NULL, 0, W_TRACE, NULL, 0, "%s<0D><0A>", ptr_start);
         }
         ptr += 2;
      }
   } while (ptr < &buf[length - 1]);
#endif
   return(SUCCESS);
}


#ifdef WITH_SSL
/*############################ ssl_connect() ############################*/
int
ssl_connect(int  sock_fd,
            char *hostname,
            char *func_name,
            int  strict,
            int  legacy_renegotiation)
{
   int      reply;
   uint64_t ctx_options;
   char     *p_env,
            *p_env1;

   if (ssl_ctx != NULL)
   {
      SSL_CTX_free(ssl_ctx);
   }
   SSLeay_add_ssl_algorithms();
   if ((ssl_ctx = (SSL_CTX *)SSL_CTX_new(SSLv23_client_method())) == NULL)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, func_name, NULL,
                _("SSL_CTX_new() unable to create a new SSL context structure."));
      (void)close(sock_fd);
      return(INCORRECT);
   }
# ifdef NO_SSLv2
   ctx_options = (SSL_OP_ALL | SSL_OP_NO_SSLv2);
# else
#  ifdef NO_SSLv3
   ctx_options = (SSL_OP_ALL | SSL_OP_NO_SSLv3);
#  else
#   ifdef NO_SSLv23
   ctx_options = (SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
#   else
#    ifdef NO_SSLv23TLS1_0
   ctx_options = (SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
                  SSL_OP_NO_TLSv1);
#    else
#     ifdef NO_SSLv23TLS1_0TLS1_1
   ctx_options = (SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
                  SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);
#     else
   ctx_options = SSL_OP_ALL;
#     endif
#    endif
#   endif
#  endif
# endif
   if (legacy_renegotiation == YES)
   {
      ctx_options |= SSL_OP_LEGACY_SERVER_CONNECT;
   }
   SSL_CTX_set_options(ssl_ctx, ctx_options);
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
   else
   {
      if (SSL_CTX_set_default_verify_paths(ssl_ctx) != 1)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, func_name, NULL,
                   _("SSL_CTX_set_default_verify_paths() failed."));
         (void)close(sock_fd);
         return(INCORRECT);
      }
   }

   if ((ssl_con = (SSL *)SSL_new(ssl_ctx)) == NULL)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, func_name, NULL,
                _("SSL_new() cannot create SSL_CTX."));
      (void)close(sock_fd);
      return(INCORRECT);
   }
   SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);
   SSL_set_connect_state(ssl_con);
   SSL_set_fd(ssl_con, sock_fd);
   if (!SSL_set_tlsext_host_name(ssl_con, hostname))
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, func_name, NULL,
                _("SSL_set_tlsext_host_name() failed to enable ServerNameIndication for %s"),
                hostname);
      (void)close(sock_fd);
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
      trans_log(ERROR_SIGN, __FILE__, __LINE__, func_name, NULL,
                _("Failed to set signal handler : %s"), strerror(errno));
      (void)close(sock_fd);
      return(INCORRECT);
   }

   if (sigsetjmp(env_alrm, 1) != 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, func_name, NULL,
                _("SSL_connect() timeout (%ld)"), transfer_timeout);
      timeout_flag = ON;
      (void)close(sock_fd);
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
   }
   else
   {
      if (strict == YES)
      {
         X509 *cert;

         if ((cert = SSL_get_peer_certificate(ssl_con)) == NULL)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, func_name, NULL,
                      _("No certificate presented by %s. Strict TLS requested."),
                      hostname);
            (void)SSL_shutdown(ssl_con);
            SSL_free(ssl_con);
            ssl_con = NULL;
            (void)close(sock_fd);
            X509_free(cert);

            return(INCORRECT);
         }
         else
         {
            char *issuer = NULL;
# ifdef WITH_TRACE
            char *subject;

            issuer = rfc2253_formatted(X509_get_issuer_name(cert));
            subject = rfc2253_formatted(X509_get_subject_name(cert));
            trans_log(DEBUG_SIGN, __FILE__, __LINE__, "ssl_connect", NULL,
                      "<CERT subject: %s issuer: %s>", subject, issuer);
            free(subject);

# endif
            if ((reply = SSL_get_verify_result(ssl_con)) != X509_V_OK)
            {
               switch (reply)
               {
                  case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
                     if (issuer == NULL)
                     {
                        issuer = rfc2253_formatted(X509_get_issuer_name(cert));
                     }
                     (void)snprintf(msg_str, MAX_RET_MSG_LENGTH,
                                    "Unable to locally verify the issuer's (%s) authority.",
                                    issuer);
                     break;
                  case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
                  case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
                     (void)snprintf(msg_str, MAX_RET_MSG_LENGTH,
                                    "Self-signed certificate encountered.");
                  case X509_V_ERR_CERT_NOT_YET_VALID:
                     (void)snprintf(msg_str, MAX_RET_MSG_LENGTH,
                                    "Issued certificate not yet valid.");
                     break;
                  case X509_V_ERR_CERT_HAS_EXPIRED:
                     (void)snprintf(msg_str, MAX_RET_MSG_LENGTH,
                                    "Issued certificate has expired.");
                     break;
                  default:
                     (void)snprintf(msg_str, MAX_RET_MSG_LENGTH, "%s",
                                    X509_verify_cert_error_string(reply));
               }
               (void)SSL_shutdown(ssl_con);
               SSL_free(ssl_con);
               ssl_con = NULL;
               (void)close(sock_fd);
               sock_fd = -1;
               free(issuer);
               X509_free(cert);

               return(INCORRECT);
            }
            free(issuer);
         }
         X509_free(cert);
      }
      reply = SUCCESS;
   }
# ifdef WITH_SSL_READ_AHEAD
   /* This is not set because I could not detect any advantage using this. */
   SSL_set_read_ahead(ssl_con, 1);
# endif

   return(reply);
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
               {
                  char *sign;

                  if ((errno == ECONNRESET) || (errno == EBADF) ||
                      (errno == EPIPE))
                  {
                     timeout_flag = CON_RESET;
                     if (errno == EPIPE)
                     {
                        sign = INFO_SIGN;
                     }
                     else
                     {
                        sign = ERROR_SIGN;
                     }
                  }
                  else
                  {
                     sign = ERROR_SIGN;
                  }
                  trans_log(sign, __FILE__, __LINE__, "ssl_write", NULL,
                            _("SSL_write() error (%d) : %s"),
                            ret, strerror(errno));
               }
               return(INCORRECT);

            default : /* Error */
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssl_write", NULL,
                         _("SSL_write() error (%d)"), ret);
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


/*########################### ssl_error_msg() ###########################*/
char *
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


/*+++++++++++++++++++++++++++++ sig_handler() +++++++++++++++++++++++++++*/
static void
sig_handler(int signo)
{
   siglongjmp(env_alrm, 1);
}


/*######################## rfc2253_formatted() ##########################*/
/*                         -------------------                           */
/* This code is taken from wget https://www.gnu.org/software/wget/       */
/*#######################################################################*/
char *
rfc2253_formatted(X509_NAME *name)
{
   int  length;
   char *out = NULL;
   BIO*  b;

   if ((b = BIO_new(BIO_s_mem())))
   {
      if ((X509_NAME_print_ex(b, name, 0, XN_FLAG_RFC2253) >= 0) &&
          ((length = BIO_number_written(b)) > 0))
      {
         out = malloc(length + 1);
         BIO_read(b, out, length);
         out[length] = 0;
      }
      BIO_free(b);
   }

  return(out ? out : strdup(""));
}
#endif /* WITH_SSL */
