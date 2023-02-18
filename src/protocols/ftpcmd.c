/*
 *  ftpcmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   ftpcmd - commands to send files via FTP
 **
 ** SYNOPSIS
 **   int ftp_account(char *user)
 **   int ftp_block_write(char *block, unsigned short size, char descriptor)
 **   int ftp_mode(char mode)
 **   int ftp_open(char *filename, int seek)
 **   int ftp_cd(char *directory, int create_dir, char *dir_mode,
 **              char *created_path)
 **   int ftp_chmod(char *filename, char *mode)
 **   int ftp_close_data(void)
 **   int ftp_connect(char *hostname, int port, int ssl, int strict,
 **                   int legacy_renegotiation)
 **   int ftp_auth_data(void)
 **   int ftp_ssl_auth(int strict, int legacy_renegotiation)
 **   int ftp_ssl_init(char type)
 **   int ftp_ssl_disable_ctrl_encrytion(void)
 **   int ftp_feat(unsigned int *)
 **   int ftp_data(char *filename, off_t seek, int mode, int type)
 **   int ftp_date(char *filename, time_t *file_mtime)
 **   int ftp_mlst(char *filename, time_t *file_mtime)
 **   int ftp_dele(char *filename)
 **   int ftp_exec(char *cmd, char *filename)
 **   int ftp_get_reply(void)
 **   int ftp_idle(int timeout)
 **   int ftp_keepalive(void)
 **   int ftp_list(int mode, int type, ...)
 **   int ftp_move(char *from, char *to, int fast_move, int create_dir,
 **                char *dir_mode, char *created_path)
 **   int ftp_noop(void)
 **   int ftp_pass(char *password)
 **   int ftp_pwd(void)
 **   int ftp_quit(void)
 **   int ftp_read(char *block, int blocksize)
 **   int ftp_sendfile(int source_fd, off_t *offset, int size)
 **   int ftp_set_date(char *filename, time_t file_mtime)
 **   int ftp_set_utf8_on(void)
 **   int ftp_size(char *filename, off_t *remote_size)
 **   int ftp_type(char type)
 **   int ftp_user(char *user)
 **   int ftp_write(char *block, char *buffer, int size)
 **
 ** DESCRIPTION
 **   ftpcmd provides a set of commands to communicate with an FTP
 **   server via BSD sockets.
 **   The procedure to send files to another FTP server is as
 **   follows:
 **          ftp_connect()
 **             |
 **             V
 **     +---------------+ YES
 **     | reply = 230 ? |-----+
 **     +---------------+     |
 **             |             |
 **             V             |
 **          ftp_user()       |
 **             |             |
 **             V             |
 **     +---------------+ YES V
 **     | reply = 230 ? |-----+
 **     +---------------+     |
 **             |             |
 **             V             |
 **          ftp_pass()       |
 **             |             |
 **             +<------------+
 **             |
 **             +-----------> ftp_idle()
 **             |                 |
 **             +<----------------+
 **             |
 **             V
 **          ftp_type() -----> ftp_cd()
 **             |                 |
 **             +<----------------+
 **             |
 **             +------------> ftp_size()
 **             |                 |
 **             +<----------------+
 **             |
 **             V
 **          ftp_data()<----------------------+
 **             |                             |
 **             V                             |
 **          ftp_write()<---------------+     |
 **             |                       |     |
 **             +----> ftp_keepalive()  |     |
 **             |             |         |     |
 **             +<------------+         |     |
 **             |                       |     |
 **             V                       |     |
 **      +-------------+       NO       |     |
 **      | File done ? |----------------+     |
 **      +-------------+                      |
 **             |                             |
 **             V                             |
 **          ftp_close_data() ---> ftp_move() |
 **             |                     |       |
 **             +<--------------------+       |
 **             |                             |
 **             V                             |
 **      +-------------+           YES        |
 **      | Next file ? |----------------------+
 **      +-------------+
 **             |
 **             +-------> ftp_dele()
 **             |            |
 **             +<-----------+
 **             |
 **             V
 **          ftp_quit()
 **
 **   The second argument buffer to ftp_write() can be NULL, if you do
 **   not want it to add a carriage return (CR) to a line feed
 **   (LF) when transmitting in ASCII mode. However when used the first
 **   byte of this buffer has special meaning in that it always contains
 **   the last byte of the previous block written. Since ftp_write() does
 **   not know when a new file starts, ie. the first block of a file, the
 **   caller is responsible in setting this to zero for the first block
 **   of a file.
 **
 ** RETURN VALUES
 **   Returns SUCCESS when successful. When an error has occurred
 **   it will either return INCORRECT or the three digit FTP reply
 **   code when the reply of the server does not conform to the
 **   one expected. The complete reply string of the FTP server
 **   is returned to 'msg_str'. 'timeout_flag' is just a flag to
 **   indicate that the 'transfer_timeout' time has been reached.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   24.02.1996 H.Kiehl Created
 **   02.02.1997 H.Kiehl Appending files when already partly transmitted.
 **   20.04.1997 H.Kiehl Fixed ftp_connect() so it does take an IP number.
 **   22.04.1997 H.Kiehl When in ftp_data() the accept times out, set
 **                      timeout_flag!
 **   05.05.1997 H.Kiehl Added function ftp_get_reply().
 **   08.09.1998 H.Kiehl ftp_user() returns 230, so application can handle
 **                      the case where no password is required.
 **   22.08.1999 H.Kiehl Use type-of-service field to minimize delay for
 **                      control connection and maximize throughput for
 **                      data connection.
 **   29.01.2000 H.Kiehl Added function ftp_size().
 **   20.02.2000 H.Kiehl Added passive mode.
 **   02.03.2000 H.Kiehl Use setvbuf() once instead of always fflushing
 **                      each fprintf(). And double the timeout when
 **                      closing the data connection.
 **   01.05.2000 H.Kiehl Buffering read() in read_msg() to reduce number
 **                      of system calls.
 **   08.07.2000 H.Kiehl Cleaned up log output to reduce code size.
 **   06.08.2000 H.Kiehl Expanded ftp_list() to support the NLST command
 **                      and create a buffer for the output from that
 **                      command.
 **   20.08.2000 H.Kiehl Implemented ftp_read() function.
 **                      Implemented ftp_date() function.
 **   03.11.2000 H.Kiehl Added ftp_chmod() function.
 **   12.11.2000 H.Kiehl Added ftp_exec() function.
 **   21.05.2001 H.Kiehl Failed to fclose() control connection when an error
 **                      occured in ftp_quit().
 **   22.07.2001 H.Kiehl Reduce transfer_timeout to 20 second in function
 **                      ftp_get_reply().
 **   26.07.2001 H.Kiehl Added ftp_account() function.
 **   05.08.2001 H.Kiehl Added ftp_idle() function.
 **   02.09.2001 H.Kiehl Added ftp_keepalive() function.
 **   29.10.2002 H.Kiehl Remove remote file if RNTO command files in
 **                      function ftp_move().
 **   06.11.2002 H.Kiehl Try to remove remote file if we fail to open
 **                      a remote file and the return message says this
 **                      is a overwrite error with ftp_data().
 **   24.12.2003 H.Kiehl Added tracing.
 **   09.01.2004 H.Kiehl Added fast_move parameter to function ftp_move().
 **                      Remove function check_reply().
 **   01.02.2004 H.Kiehl Added TLS/SSL support.
 **   11.03.2004 H.Kiehl Introduced command() to make code more readable
 **                      and removed all stdio buffering code.
 **   02.09.2004 H.Kiehl ftp_date() function returns unix time.
 **   07.10.2004 H.Kiehl Added ftp_pwd().
 **                      ftp_move() creates the missing directory on demand.
 **   12.08.2006 H.Kiehl Added extended mode (EPRT, EPSV).
 **   29.09.2009 H.Kiehl Added function ftp_set_date() to set date of
 **                      a file.
 **   17.08.2011 H.Kiehl Did not insert the remote IP in EPSV mode,
 **                      in function ftp_list() and ftp_data().
 **   08.08.2012 H.Kiehl Use getaddrinfo() instead of gethostname() to
 **                      support IPv6.
 **   06.02.2014 H.Kiehl Added CCC comand to end encryption.
 **   10.09.2014 H.Kiehl Added simulation mode.
 **   03.11.2018 H.Kiehl Implemented ServerNameIndication for TLS.
 **   27.06.2019 H.Kiehl Extended list command to show hidden files.
 **   05.11.2019 H.Kiehl Add support for STAT list command.
 **   06.03.2020 H.Kiehl Implement implicit FTPS.
 **   19.03.2022 H.Kiehl Add strict TLS support.
 **   17.07.2022 H.Kiehl Add option to enable TLS legacy renegotiation.
 **   15.02.2023 H.Kiehl Added ftp_set_utf8_on().
 */
DESCR__E_M3


#include <stdio.h>        /* fprintf(), fdopen(), fclose()               */
#include <stdarg.h>       /* va_start(), va_arg(), va_end()              */
#include <string.h>       /* memset(), memcpy(), strcpy()                */
#include <stdlib.h>       /* strtoul(), free()                           */
#include <ctype.h>        /* isdigit()                                   */
#include <time.h>         /* mktime(), strftime(), gmtime()              */
#include <setjmp.h>       /* sigsetjmp(), siglongjmp()                   */
#include <signal.h>       /* signal()                                    */
#include <sys/types.h>    /* fd_set                                      */
#include <sys/time.h>     /* struct timeval                              */
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef WITH_SENDFILE
# include <sys/sendfile.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>  /* socket(), shutdown(), bind(), accept(),     */
                          /* setsockopt()                                */
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
# include <netdb.h>       /* struct hostent, gethostbyname(),            */
                          /* getaddrinfo()                               */
#endif
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>   /* inet_addr()                                 */
#endif
#ifdef WITH_SSL
# include <openssl/crypto.h>
# include <openssl/x509.h>
# include <openssl/ssl.h>
#endif
#include <unistd.h>       /* select(), write(), read(), close(), alarm() */
#include <errno.h>
#include "fddefs.h"
#include "ftpdefs.h"
#include "commondefs.h"

#ifdef WITH_SSL
SSL                            *ssl_con = NULL;
#endif

/* External global variables. */
extern int                     simulation_mode,
                               timeout_flag;
#ifdef WITH_IP_DB
extern int                     use_ip_db;
#endif
extern unsigned int            special_flag;
extern char                    msg_str[];
#ifdef LINUX
extern char                    *h_errlist[];  /* for gethostbyname() */
extern int                     h_nerr;        /* for gethostbyname() */
#endif
extern long                    transfer_timeout;

/* Local global variables. */
#if defined (HAVE_GETADDRINFO) && defined (HAVE_GAI_STRERROR)
static int                     ai_family;
static size_t                  ai_addrlen;
static struct sockaddr         *ai_addr = NULL;
#endif
static int                     control_fd,
                               data_fd = -1;
#ifdef WITH_SSL
static SSL                     *ssl_data = NULL;
static SSL_CTX                 *ssl_ctx = NULL;
static char                    connected_hostname[MAX_REAL_HOSTNAME_LENGTH];
#endif
static sigjmp_buf              env_alrm;
static struct sockaddr_in      ctrl,
                               sin;
static struct timeval          timeout;
static struct ftp_connect_data fcd;

/* Local function prototypes. */
static int                     check_data_socket(int, int, int *, char *, int,
                                                 char *, char *),
                               get_extended_number(char *),
                               get_mlst_reply(char *, int, time_t *, int),
                               get_number(char **, char),
                               get_reply(char *, int, int),
#ifdef WITH_SSL
                               encrypt_data_connection(int),
                               get_stat_reply(int, SSL *, int, ...),
                               read_data_line(int, SSL *, char *),
                               read_data_to_buffer(int, SSL *, char ***),
#else
                               get_stat_reply(int, int, ...),
                               read_data_line(int, char *),
                               read_data_to_buffer(int, char ***),
#endif
                               read_msg(char *, int);
static void                    sig_handler(int);


/*########################## ftp_connect() ##############################*/
int
#ifdef WITH_SSL
ftp_connect(char *hostname, int port, int ssl, int strict, int legacy_renegotiation)
#else
ftp_connect(char *hostname, int port)
#endif
{
   if (simulation_mode == YES)
   {
      if ((control_fd = open("/dev/null", O_RDWR)) == -1)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_connect", "Simulated ftp_connect()",
                   _("Failed to open() /dev/null : %s"), strerror(errno));
         return(INCORRECT);
      }
      else
      {
#ifdef WITH_TRACE
         int length;

         length = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                           _("Simulated FTP connect to %s (port=%d)"),
                           hostname, port);
         if (length > MAX_RET_MSG_LENGTH)
         {
            length = MAX_RET_MSG_LENGTH;
         }
         trace_log(NULL, 0, C_TRACE, msg_str, length, NULL);
#else
         (void)snprintf(msg_str, MAX_RET_MSG_LENGTH,
                        _("Simulated FTP connect to %s (port=%d)"),
                        hostname, port);
#endif
      }
   }
   else
   {
      int                     reply;
      my_socklen_t            length;
#ifdef WITH_IP_DB
      int                     ip_from_db = NO;
#endif
#if defined (HAVE_GETADDRINFO) && defined (HAVE_GAI_STRERROR)
      char                    str_port[MAX_INT_LENGTH];
      struct addrinfo         hints,
                              *result = NULL,
                              *rp;

      (void)memset((struct addrinfo *) &hints, 0, sizeof(struct addrinfo));
      hints.ai_family = (special_flag & DISABLE_IPV6_FLAG) ? AF_INET : AF_UNSPEC;
      hints.ai_socktype = SOCK_STREAM;
/* ???   hints.ai_flags = AI_CANONNAME; */

      (void)snprintf(str_port, MAX_INT_LENGTH, "%d", port);
      reply = getaddrinfo(hostname, str_port, &hints, &result);
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
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
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
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
                      _("Failed to getaddrinfo() %s : %s"),
                      hostname, gai_strerror(reply));
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
         if ((control_fd = socket(rp->ai_family, rp->ai_socktype,
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

# ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
         if (timeout_flag != OFF)
         {
            reply = 1;
            if (setsockopt(control_fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&reply,
                           sizeof(reply)) < 0)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
                         _("setsockopt() SO_KEEPALIVE error : %s"),
                         strerror(errno));
            }
#  ifdef TCP_KEEPALIVE
            reply = timeout_flag;
            if (setsockopt(control_fd, IPPROTO_IP, TCP_KEEPALIVE,
                           (char *)&reply, sizeof(reply)) < 0)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
                         _("setsockopt() TCP_KEEPALIVE error : %s"),
                         strerror(errno));
            }
#  endif
            timeout_flag = OFF;
         }
# endif

         reply = connect_with_timeout(control_fd, rp->ai_addr, rp->ai_addrlen);
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
            (void)close(control_fd);
            continue;
         }
         else if (reply == PERMANENT_INCORRECT)
              {
                 (void)close(control_fd);
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
                           _("Failed to connect() to %s"), hostname);
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
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
                      _("Failed to connect() to %s : %s"),
                      hostname, strerror(errno));
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
                      _("Failed to connect() to %s"), hostname);
         }
         (void)close(control_fd);
         freeaddrinfo(result);
         return(INCORRECT);
      }

      if (ai_addr != NULL)
      {
         free(ai_addr);
      }
      if ((ai_addr = malloc(rp->ai_addrlen)) == NULL)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
                   _("malloc() error : %s"), strerror(errno));
         (void)close(control_fd);
         freeaddrinfo(result);
         return(INCORRECT);
      }
      (void)memcpy(ai_addr, rp->ai_addr, rp->ai_addrlen);
      ai_addrlen = rp->ai_addrlen;
      ai_family = rp->ai_family;
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

      (void)memset((struct sockaddr_in *) &sin, 0, sizeof(sin));
      if (ai_family == AF_INET)
      {
         (void)memcpy((struct sockaddr *) &sin, (struct sockaddr_in *)ai_addr,
                      ai_addrlen);
      }
#else
      register struct hostent *p_host = NULL;

      (void)memset((struct sockaddr *) &sin, 0, sizeof(sin));
      if ((sin.sin_addr.s_addr = inet_addr(hostname)) == -1)
      {
         if ((p_host = gethostbyname(hostname)) == NULL)
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
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
                                  _("Failed to inet_addr() %s : %s"),
                                  ip_str, h_errlist[h_errno]);
                     }
                     else
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
                                  _("Failed to inet_addr() %s (h_errno = %d) : %s"),
                                  ip_str, h_errno, strerror(errno));
                     }
#  else
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
                               _("Failed to inet_addr() %s (h_errno = %d) : %s"),
                               ip_str, h_errno, strerror(errno));
#  endif
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
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
                               _("Failed to gethostbyname() %s : %s"),
                               hostname, h_errlist[h_errno]);
                  }
                  else
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
                               _("Failed to gethostbyname() %s (h_errno = %d) : %s"),
                               hostname, h_errno, strerror(errno));
                  }
#  else
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
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
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
                         _("Failed to gethostbyname() %s : %s"),
                         hostname, strerror(errno));
# if !defined (_HPUX) && !defined (_SCO)
            }
# endif
            return(INCORRECT);
         }

         /* Copy IP number to socket structure. */
         memcpy((char *)&sin.sin_addr, p_host->h_addr, p_host->h_length);
      }

      if ((control_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
                   _("socket() error : %s"), strerror(errno));
         return(INCORRECT);
      }
      sin.sin_family = AF_INET;
      sin.sin_port = htons((u_short)port);

# ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
      if (timeout_flag != OFF)
      {
         reply = 1;
         if (setsockopt(control_fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&reply,
                        sizeof(reply)) < 0)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
                      _("setsockopt() SO_KEEPALIVE error : %s"), strerror(errno));
         }
#  ifdef TCP_KEEPALIVE
         reply = timeout_flag;
         if (setsockopt(control_fd, IPPROTO_IP, TCP_KEEPALIVE, (char *)&reply,
                        sizeof(reply)) < 0)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
                      _("setsockopt() TCP_KEEPALIVE error : %s"),
                      strerror(errno));
         }
#  endif
         timeout_flag = OFF;
      }
# endif

      if (connect_with_timeout(control_fd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
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
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
                      _("Failed to connect() to %s : %s"),
                      hostname, strerror(errno));
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
                      _("Failed to connect() to %s"), hostname);
         }
         (void)close(control_fd);
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
      length = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                        _("Connected to %s at port %d"), hostname, port);
      if (length > MAX_RET_MSG_LENGTH)
      {
         length = MAX_RET_MSG_LENGTH;
      }
      trace_log(NULL, 0, C_TRACE, msg_str, length, NULL);
#endif

      length = sizeof(ctrl);
      if (getsockname(control_fd, (struct sockaddr *)&ctrl, &length) < 0)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
                   _("getsockname() error : %s"), strerror(errno));
         (void)close(control_fd);
         return(INCORRECT);
      }

#if defined (_WITH_TOS) && defined (IP_TOS) && defined (IPTOS_LOWDELAY)
      reply = IPTOS_LOWDELAY;
      if (setsockopt(control_fd, IPPROTO_IP, IP_TOS, (char *)&reply,
                     sizeof(reply)) < 0)
      {
         trans_log(WARN_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
                   _("setsockopt() IP_TOS error : %s"), strerror(errno));
      }
#endif

#ifdef WITH_SSL
      if (ssl == YES)
      {
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
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
                      _("SSL_CTX_new() unable to create a new SSL context structure."));
            (void)close(control_fd);
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
         SSL_CTX_set_verify(ssl_ctx,
                            (strict == YES) ? SSL_VERIFY_PEER : SSL_VERIFY_NONE,
                            NULL);

         ssl_con = (SSL *)SSL_new(ssl_ctx);
         SSL_set_connect_state(ssl_con);
         SSL_set_fd(ssl_con, control_fd);
         if (!SSL_set_tlsext_host_name(ssl_con, hostname))
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
                      _("SSL_set_tlsext_host_name() failed to enable ServerNameIndication for %s"),
                      hostname);
            (void)close(control_fd);
            return(INCORRECT);
         }

         if (signal(SIGALRM, sig_handler) == SIG_ERR)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
                      _("Failed to set signal handler : %s"), strerror(errno));
            (void)close(control_fd);
            return(INCORRECT);
         }
         if (sigsetjmp(env_alrm, 1) != 0)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
                      _("accept() timeout (%lds)"), transfer_timeout);
            timeout_flag = ON;
            (void)close(control_fd);
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
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_connect", msg_str,
                      "SSL/TSL connection to server `%s' at port %d failed.",
                      hostname, port);
            SSL_free(ssl_con);
            ssl_con = NULL;
            (void)close(control_fd);
            control_fd = -1;
            return(INCORRECT);
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
            if (strict == YES)
            {
               X509 *cert;

               if ((cert = SSL_get_peer_certificate(ssl_con)) == NULL)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
                           _("No certificate presented by %s. Strict TLS requested."),
                           hostname);
                  (void)SSL_shutdown(ssl_con);
                  SSL_free(ssl_con);
                  ssl_con = NULL;
                  (void)close(control_fd);
                  control_fd = -1;
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
                  trans_log(DEBUG_SIGN, __FILE__, __LINE__, "ftp_connect", NULL,
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
                     (void)close(control_fd);
                     control_fd = -1;
                     free(issuer);
                     X509_free(cert);

                     return(INCORRECT);
                  }
                  free(issuer);
               }
               X509_free(cert);
            }
         }
      }
#endif /* WITH_SSL */

      if ((reply = get_reply(ERROR_SIGN, 0, __LINE__)) < 0)
      {
         (void)close(control_fd);
         return(INCORRECT);
      }
      if ((reply != 220) && (reply != 120))
      {
         if (reply != 230)
         {
            (void)close(control_fd);
         }
         return(reply);
      }
   }
   fcd.ftp_options = 0;
   fcd.data_port = 0;
#ifdef WITH_SSL
   (void)memcpy(connected_hostname, hostname, MAX_REAL_HOSTNAME_LENGTH);
#endif

   return(SUCCESS);
}


#ifdef WITH_SSL
/*########################## ftp_ssl_auth() #############################*/
int
ftp_ssl_auth(int strict, int legacy_renegotiation)
{
   int reply = SUCCESS;

   if (ssl_con == NULL)
   {
      reply = command(control_fd, "AUTH TLS");
      if ((reply == SUCCESS) &&
          ((reply = get_reply(ERROR_SIGN, 999, __LINE__)) != INCORRECT))
      {
         if ((reply == 234) || (reply == 334))
         {
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
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_ssl_auth", NULL,
                         _("SSL_CTX_new() unable to create a new SSL context structure."));
               (void)close(control_fd);
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
            SSL_set_fd(ssl_con, control_fd);
            if (!SSL_set_tlsext_host_name(ssl_con, connected_hostname))
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_ssl_auth", NULL,
                         _("SSL_set_tlsext_host_name() failed to enable ServerNameIndication for %s"),
                         connected_hostname);
               (void)close(control_fd);
               control_fd = -1;
               return(INCORRECT);
            }

            if (signal(SIGALRM, sig_handler) == SIG_ERR)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_ssl_auth", NULL,
                         _("Failed to set signal handler : %s"), strerror(errno));
               (void)close(control_fd);
               return(INCORRECT);
            }
            if (sigsetjmp(env_alrm, 1) != 0)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_ssl_auth", NULL,
                         _("accept() timeout (%lds)"), transfer_timeout);
               timeout_flag = ON;
               (void)close(control_fd);
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
               reply = SUCCESS;
            }
         }
         else if (reply == 999)
              {
                 ssl_con = NULL;
                 reply = SUCCESS;
              }
              else
              {
                 reply = INCORRECT;
              }
      }
   }

   return(reply);
}
#endif /* WITH_SSL */


/*############################## ftp_user() #############################*/
int
ftp_user(char *user)
{
   int reply,
       count = 0;

   do
   {
      reply = command(control_fd, "USER %s", user);
      if ((reply != SUCCESS) ||
          ((reply = get_reply(ERROR_SIGN, 230, __LINE__)) < 0))
      {
         return(INCORRECT);
      }

      /*
       * Some brain-damaged implementations think we are still
       * logged on, when we try to log in too quickly after we
       * just did a log-off.
       */
      if (reply == 430)
      {
         trans_log(DEBUG_SIGN, __FILE__, __LINE__, "ftp_user", NULL,
                   _("Hmmm. Still thinks I am logged on. Lets wait for a while."));
         (void)my_usleep(700000L);
      }
   } while ((reply == 430) && (count++ < 10));

   /*
    * NOTE: We delibaretly ignore 230 here, since this means that no
    *       password is required. Thus we do have to return the 230
    *       so the apllication knows what to do with it.
    */
   if (reply == 331)
   {
      /*
       * Some broken FTP servers return 331 instead of 4xx or 5xx
       * when logging in to another user is not permitted. This is
       * really broken because a 3xx reply suggest one must continue
       * with a PASS instruction. So check if this is the case by
       * looking for the following string:
       *    331 Can't change to another user.
       */
      if (!((msg_str[4] == 'C') && (msg_str[5] == 'a') &&
            (msg_str[6] == 'n') && (msg_str[7] == '\'') &&
            (msg_str[8] == 't') && (msg_str[9] == ' ') &&
            (msg_str[10] == 'c') && (msg_str[11] == 'h') &&
            (msg_str[12] == 'a') && (msg_str[13] == 'n') &&
            (msg_str[14] == 'g') && (msg_str[15] == 'e') &&
            (msg_str[16] == ' ') && (msg_str[17] == 't') &&
            (msg_str[18] == 'o') && (msg_str[19] == ' ') &&
            (msg_str[20] == 'a') && (msg_str[21] == 'n') &&
            (msg_str[22] == 'o') && (msg_str[23] == 't') &&
            (msg_str[24] == 'h') && (msg_str[25] == 'e') &&
            (msg_str[26] == 'r') && (msg_str[27] == ' ') &&
            (msg_str[28] == 'u') && (msg_str[29] == 's') &&
            (msg_str[30] == 'e') && (msg_str[31] == 'r')))
      {
         reply = SUCCESS;
      }
   }
   else if (reply == 332)
        {
           reply = SUCCESS;
        }

   return(reply);
}


/*############################ ftp_account() ############################*/
int
ftp_account(char *user)
{
   int reply;

   if ((reply = command(control_fd, "ACCT %s", user)) == SUCCESS)
   {
      if ((reply = get_reply(ERROR_SIGN, 202, __LINE__)) != INCORRECT)
      {
         /*
          * NOTE: We delibaretly ignore 230 here, since this means that no
          *       password is required. Thus we do have to return the 230
          *       so the apllication knows what to do with it.
          */
         if (reply == 202)
         {
            reply = SUCCESS;
         }
      }
   }

   return(reply);
}


/*############################## ftp_pass() #############################*/
int
ftp_pass(char *password)
{
   int reply;

   if ((reply = command(control_fd, "PASS %s", password)) == SUCCESS)
   {
      if ((reply = get_reply(ERROR_SIGN, 230, __LINE__)) != INCORRECT)
      {
          /*
           * If the remote server returns 332, it want's as the next step
           * the ACCT command. We do not handle this automatically since in
           * case of a proxy there can be more then one login name and we
           * don't know which one to take. Thus, the user must use the
           * proxy syntax of AFD, ie do it by hand :-(
           */
         if ((reply == 230) || (reply == 202) || (reply == 331) ||
             (reply == 332))
         {
            reply = SUCCESS;
         }
      }
   }

   return(reply);
}


#ifdef WITH_SSL
/*########################## ftp_ssl_init() #############################*/
int
ftp_ssl_init(char type)
{
   int reply = SUCCESS;

   if ((type == YES) || (type == BOTH))
   {
      reply = command(control_fd, "PBSZ 0");
      if ((reply == SUCCESS) &&
          ((reply = get_reply(ERROR_SIGN, 200, __LINE__)) != INCORRECT))
      {
         if (reply == 200)
         {
            if (type == BOTH)
            {
               reply = command(control_fd, "PROT P");
            }
            else
            {
               reply = command(control_fd, "PROT C");
            }
            if ((reply == SUCCESS) &&
                ((reply = get_reply(ERROR_SIGN, 200, __LINE__)) != INCORRECT))
            {
               if (reply == 200)
               {
                  reply = SUCCESS;
               }
            }
         }
         else
         {
            reply = INCORRECT;
         }
      }
   }

   return(reply);
}


/*################# ftp_ssl_disable_ctrl_encrytion() ####################*/
int
ftp_ssl_disable_ctrl_encrytion(void)
{
   int reply;

   reply = command(control_fd, "CCC");
   if ((reply == SUCCESS) &&
       ((reply = get_reply(ERROR_SIGN, 999, __LINE__)) != INCORRECT))
   {
      if (reply == 200)
      {
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
         reply = SUCCESS;
      }
      else if (reply == 999)
           {
              ssl_con = NULL;
              reply = SUCCESS;
           }
           else
           {
              reply = INCORRECT;
           }
   }

   return(reply);
}
#endif /* WITH_SSL */


/*############################## ftp_feat() #############################*/
int
ftp_feat(unsigned int *ftp_options)
{
   int reply;

   *ftp_options = 0;
   if ((reply = command(control_fd, "FEAT")) == SUCCESS)
   {
      if (simulation_mode == YES)
      {
         *ftp_options |= (FTP_OPTION_FEAT | FTP_OPTION_MDTM |
                          FTP_OPTION_SIZE | FTP_OPTION_UTF8);
         fcd.ftp_options = *ftp_options;

         return(SUCCESS);
      }

      if (read_msg(ERROR_SIGN, __LINE__) == INCORRECT)
      {
         return(INCORRECT);
      }
      if ((msg_str[0] == '2') && (msg_str[1] == '1') &&
          (msg_str[2] == '1') && (msg_str[3] == '-'))
      {
         for (;;)
         {
            if (read_msg(ERROR_SIGN, __LINE__) == INCORRECT)
            {
               return(INCORRECT);
            }

            if ((msg_str[0] == '2') && (msg_str[1] == '1') &&
                (msg_str[2] == '1') && (msg_str[3] != '-'))
            {
               *ftp_options |= FTP_OPTION_FEAT;
               break;
            }
                 /* MDTM - File Modification Time */
            else if ((msg_str[0] == ' ') &&
                     ((msg_str[1] == 'M') || (msg_str[1] == 'm')) &&
                     ((msg_str[2] == 'D') || (msg_str[2] == 'd')) &&
                     ((msg_str[3] == 'T') || (msg_str[3] == 't')) &&
                     ((msg_str[4] == 'M') || (msg_str[4] == 'm')))
                 {
                    *ftp_options |= FTP_OPTION_MDTM;
                 }
                 /* SIZE - File Size */
            else if ((msg_str[0] == ' ') &&
                     ((msg_str[1] == 'S') || (msg_str[1] == 's')) &&
                     ((msg_str[2] == 'I') || (msg_str[2] == 'i')) &&
                     ((msg_str[3] == 'Z') || (msg_str[3] == 'z')) &&
                     ((msg_str[4] == 'E') || (msg_str[4] == 'e')))
                 {
                    *ftp_options |= FTP_OPTION_SIZE;
                 }
                 /* UTF8 - Filename encoding */
            else if ((msg_str[0] == ' ') &&
                     ((msg_str[1] == 'U') || (msg_str[1] == 'u')) &&
                     ((msg_str[2] == 'T') || (msg_str[2] == 't')) &&
                     ((msg_str[3] == 'F') || (msg_str[3] == 'f')) &&
                     (msg_str[4] == '8'))
                 {
                    *ftp_options |= FTP_OPTION_UTF8;
                 }
                 /* MLST - List Single Object + List Directory */
            else if ((msg_str[0] == ' ') &&
                     ((msg_str[1] == 'M') || (msg_str[1] == 'm')) &&
                     ((msg_str[2] == 'L') || (msg_str[2] == 'l')) &&
                     ((msg_str[3] == 'S') || (msg_str[3] == 's')) &&
                     ((msg_str[4] == 'T') || (msg_str[4] == 't')))
                 {
                    *ftp_options |= FTP_OPTION_MLST;
                    if (msg_str[5] == ' ')
                    {
                       int i = 6;

                       do
                       {
                          /* modify*; */
                          if (((msg_str[i] == 'M') || (msg_str[i] == 'm')) &&
                              ((msg_str[i + 1] == 'O') || (msg_str[i + 1] == 'o')) &&
                              ((msg_str[i + 2] == 'D') || (msg_str[i + 2] == 'd')) &&
                              ((msg_str[i + 3] == 'I') || (msg_str[i + 3] == 'i')) &&
                              ((msg_str[i + 4] == 'F') || (msg_str[i + 4] == 'f')) &&
                              ((msg_str[i + 5] == 'Y') || (msg_str[i + 5] == 'y')) &&
                              (msg_str[i + 6] == '*') && (msg_str[i + 7] == ';'))
                          {
                             *ftp_options |= FTP_OPTION_MLST_MODIFY;
                             i += 8;
                          }
                               /* perm*; */
                          else if (((msg_str[i] == 'P') || (msg_str[i] == 'p')) &&
                                   ((msg_str[i + 1] == 'E') || (msg_str[i + 1] == 'e')) &&
                                   ((msg_str[i + 2] == 'R') || (msg_str[i + 2] == 'r')) &&
                                   ((msg_str[i + 3] == 'M') || (msg_str[i + 3] == 'm')) &&
                                   (msg_str[i + 4] == '*') && (msg_str[i + 5] == ';'))
                               {
                                  *ftp_options |= FTP_OPTION_MLST_PERM;
                                  i += 6;
                               }
                               /* size*; */
                          else if (((msg_str[i] == 'S') || (msg_str[i] == 's')) &&
                                   ((msg_str[i + 1] == 'I') || (msg_str[i + 1] == 'i')) &&
                                   ((msg_str[i + 2] == 'Z') || (msg_str[i + 2] == 'z')) &&
                                   ((msg_str[i + 3] == 'E') || (msg_str[i + 3] == 'e')) &&
                                   (msg_str[i + 4] == '*') && (msg_str[i + 5] == ';'))
                               {
                                  *ftp_options |= FTP_OPTION_MLST_SIZE;
                                  i += 6;
                               }
                               /* type*; */
                          else if (((msg_str[i] == 'T') || (msg_str[i] == 't')) &&
                                   ((msg_str[i + 1] == 'Y') || (msg_str[i + 1] == 'y')) &&
                                   ((msg_str[i + 2] == 'P') || (msg_str[i + 2] == 'p')) &&
                                   ((msg_str[i + 3] == 'E') || (msg_str[i + 3] == 'e')) &&
                                   (msg_str[i + 4] == '*') && (msg_str[i + 5] == ';'))
                               {
                                  *ftp_options |= FTP_OPTION_MLST_TYPE;
                                  i += 6;
                               }
                               else /* Ignore other features. */
                               {
                                  while ((msg_str[i] != ';') &&
                                         (msg_str[i] != '\r') &&
                                         (msg_str[i] != '\n') &&
                                         (msg_str[i] != '\0'))
                                  {
                                     i++;
                                  }
                                  if (msg_str[i] == ';')
                                  {
                                     i++;
                                  }
                               }
                       } while ((msg_str[i] != '\0') && (msg_str[i] != '\r') &&
                                (msg_str[i] != '\n'));
                    }
                 }
         } /* for (;;) */

         reply = SUCCESS;
      }
   }
   fcd.ftp_options = *ftp_options;

   return(reply);
}


/*############################## ftp_idle() #############################*/
int
ftp_idle(int timeout)
{
   int reply;

   if ((reply = command(control_fd, "SITE IDLE %d", timeout)) == SUCCESS)
   {
      if ((reply = get_reply(ERROR_SIGN, 200, __LINE__)) != INCORRECT)
      {
         if (reply == 200)
         {
            reply = SUCCESS;
         }
      }
   }

   return(reply);
}


/*########################## ftp_set_utf8_on() ##########################*/
int
ftp_set_utf8_on(void)
{
   int reply;

   if ((reply = command(control_fd, "OPTS UTF8 ON")) == SUCCESS)
   {
      if ((reply = get_reply(ERROR_SIGN, 200, __LINE__)) != INCORRECT)
      {
         if (reply == 200)
         {
            reply = SUCCESS;
         }
      }
   }

   return(reply);
}


/*############################### ftp_pwd() #############################*/
int
ftp_pwd(void)
{
   int reply;

   if ((reply = command(control_fd, "PWD")) == SUCCESS)
   {
      if ((reply = get_reply(ERROR_SIGN, 999, __LINE__)) != INCORRECT)
      {
         if (reply == 257)
         {
            char *ptr;

            ptr = msg_str + 4;
            if (*ptr == '"')
            {
               char *end_ptr;

               ptr++;
               end_ptr = ptr;
               while ((*end_ptr != '"') && (*end_ptr != '\0'))
               {
                  end_ptr++;
               }
               if (*end_ptr == '"')
               {
                  (void)memmove(msg_str, ptr, end_ptr - ptr);
                  msg_str[end_ptr - ptr] = '\0';
                  reply = SUCCESS;
               }
            }
         }
         else if (reply == 999)
              {
                 (void)strcpy(msg_str, "/simulated/pwd");
                 reply = SUCCESS;
              }
      }
   }

   return(reply);
}


/*############################## ftp_type() #############################*/
int
ftp_type(char type)
{
   int reply;

   if ((reply = command(control_fd, "TYPE %c", type)) == SUCCESS)
   {
      if ((reply = get_reply(ERROR_SIGN, 200, __LINE__)) != INCORRECT)
      {
         if (reply == 200)
         {
            reply = SUCCESS;
         }
      }
   }

   return(reply);
}


/*############################### ftp_cd() ##############################*/
int
ftp_cd(char *directory, int create_dir, char *dir_mode, char *created_path)
{
   int reply;

   if (directory[0] == '\0')
   {
      reply = command(control_fd, "CWD ~");
   }
   else
   {
      reply = command(control_fd, "CWD %s", directory);
   }

   if ((reply == SUCCESS) &&
       ((reply = get_reply(ERROR_SIGN, 250, __LINE__)) != INCORRECT))
   {
      if ((reply == 250) || (reply == 200))
      {
         reply = SUCCESS;
      }
      else if ((create_dir == YES) && (directory[0] != '\0') && (reply == 550))
           {
              int  offset = 0;
              char *p_start,
                   *ptr,
                   tmp_char;

              ptr = directory;

              /*
               * We need to check if this is an absolute path and set
               * offset, otherwise we will create the remote path
               * as relative.
               */
              if (*ptr == '/')
              {
                 ptr++;
                 offset = 1;
              }

              do
              {
                 while (*ptr == '/')
                 {
                    ptr++;
                 }
                 if (offset)
                 {
                    p_start = ptr - 1;
                    offset = 0;
                 }
                 else
                 {
                    p_start = ptr;
                 }
                 while ((*ptr != '/') && (*ptr != '\0'))
                 {
                    ptr++;
                 }
                 if ((*ptr == '/') || ((*ptr == '\0') && (p_start != ptr)))
                 {
                    tmp_char = *ptr;
                    *ptr = '\0';
                    reply = command(control_fd, "CWD %s", p_start);
                    if ((reply == SUCCESS) &&
                        ((reply = get_reply(ERROR_SIGN, 0, __LINE__)) != INCORRECT))
                    {
                       if ((reply != 250) && (reply != 200))
                       {
                          reply = command(control_fd, "MKD %s", p_start);
                          if ((reply == SUCCESS) &&
                              ((reply = get_reply(ERROR_SIGN, 0, __LINE__)) != INCORRECT))
                          {
                             if (reply == 257) /* Directory created. */
                             {
                                if (created_path != NULL)
                                {
                                   if (created_path[0] != '\0')
                                   {
                                      (void)strcat(created_path, "/");
                                   }
                                   (void)strcat(created_path, p_start);
                                }
                                if ((dir_mode[0] != '\0') && (p_start != NULL))
                                {
                                   int tmp_reply;

                                   if ((tmp_reply = ftp_chmod(p_start,
                                                              dir_mode)) != SUCCESS)
                                   {
                                      trans_log(WARN_SIGN, __FILE__, __LINE__, "ftp_cd", msg_str,
                                                "Failed to chmod remote directory `%s' to %s (%d).",
                                                p_start, dir_mode, tmp_reply);
                                   }
                                }

                                reply = command(control_fd, "CWD %s", p_start);
                                if ((reply == SUCCESS) &&
                                    ((reply = get_reply(ERROR_SIGN, 0, __LINE__)) != INCORRECT))
                                {
                                   if ((reply != 250) && (reply != 200))
                                   {
                                      p_start = NULL;
                                   }
                                   else
                                   {
                                      reply = SUCCESS;
                                   }
                                }
                             }
                             else
                             {
                                p_start = NULL;
                             }
                          }
                       }
                    }
                    else
                    {
                       p_start = NULL;
                    }
                    *ptr = tmp_char;
                 }
              } while ((*ptr != '\0') && (p_start != NULL));
           }
   }

   return(reply);
}


/*############################# ftp_chmod() #############################*/
int
ftp_chmod(char *filename, char *mode)
{
   int reply;

   if ((reply = command(control_fd, "SITE CHMOD %s %s", mode, filename)) == SUCCESS)
   {
      if ((reply = get_reply(ERROR_SIGN, 250, __LINE__)) != INCORRECT)
      {
         if ((reply == 250) || (reply == 200))
         {
            reply = SUCCESS;
         }
      }
   }

   return(reply);
}


/*############################## ftp_move() #############################*/
int
ftp_move(char *from,
         char *to,
         int  fast_move,
         int  create_dir,
         char *dir_mode,
         char *created_path)
{
   int reply;
#ifdef WITH_MS_ERROR_WORKAROUND
   int retries = 0;
#endif

#ifdef WITH_MS_ERROR_WORKAROUND
retry_command:
#endif
   if (fast_move)
   {
      reply = command(control_fd, "RNFR %s\r\nRNTO %s", from, to);
   }
   else
   {
      reply = command(control_fd, "RNFR %s", from);
   }

   if ((reply != SUCCESS) || ((reply = get_reply(ERROR_SIGN, 350, __LINE__)) < 0))
   {
      return(INCORRECT);
   }

#ifdef WITH_MS_ERROR_WORKAROUND
   if ((reply == 550) && (retries == 0))
   {
      if (fast_move)
      {
         (void)get_reply(ERROR_SIGN, 0, __LINE__);
      }
      retries++;
      my_usleep(50000L);
      goto retry_command;
   }
#endif

   if ((reply != 350) && (reply != 200))
   {
      if (fast_move)
      {
         char tmp_msg_str[MAX_RET_MSG_LENGTH];

         /*
          * We already have send the second command and it is very likely
          * that there will be a reply for this one too. So lets read
          * that reply as well, but remember to store the original
          * messages otherwise we will show the user the reply
          * of the second command, which will presumably always be
          * wrong and thus the user will be unable to see what went
          * wrong.
          */
         (void)memcpy(tmp_msg_str, msg_str, MAX_RET_MSG_LENGTH);
         (void)get_reply(ERROR_SIGN, 0, __LINE__);
         (void)memcpy(msg_str, tmp_msg_str, MAX_RET_MSG_LENGTH);
      }
      return(reply);
   }

   if (!fast_move)
   {
      if (command(control_fd, "RNTO %s", to) != SUCCESS)
      {
         return(INCORRECT);
      }
   }

   /* Get reply from RNTO part. */
   if ((reply = get_reply(ERROR_SIGN, 250, __LINE__)) < 0)
   {
      return(INCORRECT);
   }

   if ((reply != 250) && (reply != 200))
   {
      /*
       * We would overwrite the original file in any case, so if
       * the RNTO part fails lets try to delete it and try again.
       */
      if (ftp_dele(to) == SUCCESS)
      {
         if (!fast_move)
         {
            /*
             * YUCK! Window system require that we send the RNFR again!
             * This is unnecessary, we only need to send the RNTO
             * again.
             */
            reply = command(control_fd, "RNFR %s", from);
            if ((reply != SUCCESS) ||
                ((reply = get_reply(ERROR_SIGN, 0, __LINE__)) < 0))
            {
               return(INCORRECT);
            }

            if ((reply != 350) && (reply != 200))
            {
               return(reply);
            }
         }

         reply = command(control_fd, "RNTO %s", to);
         if ((reply != SUCCESS) ||
             ((reply = get_reply(ERROR_SIGN, 0, __LINE__)) < 0))
         {
            return(INCORRECT);
         }

         if ((reply != 250) && (reply != 200))
         {
            return(reply);
         }
      }
      else if ((reply == 550) && (create_dir == YES))
           {
              char *ptr,
                   to_dir[MAX_PATH_LENGTH];

              (void)strcpy(to_dir, to);
              ptr = to_dir + strlen(to_dir) - 1;
              while ((*ptr == '/') && (ptr > to_dir))
              {
                 ptr--;
              }
              while ((*ptr != '/') && (ptr > to_dir))
              {
                 ptr--;
              }
              if ((*ptr == '/') && (to_dir != ptr))
              {
                 *ptr = '\0';
                 *(ptr + 1) = '\0';
                 if (ftp_pwd() == SUCCESS)
                 {
                    char current_dir[MAX_PATH_LENGTH];

                    (void)strcpy(current_dir, msg_str);

                    /* The following will actually create the directory. */
                    if (ftp_cd(to_dir, YES, dir_mode, created_path) == SUCCESS)
                    {
                       if (ftp_cd(current_dir, NO, "", NULL) == SUCCESS)
                       {
                          reply = command(control_fd, "RNFR %s", from);
                          if ((reply != SUCCESS) ||
                              ((reply = get_reply(ERROR_SIGN, 0, __LINE__)) < 0))
                          {
                             return(INCORRECT);
                          }
                          reply = command(control_fd, "RNTO %s", to);
                          if ((reply != SUCCESS) ||
                              ((reply = get_reply(ERROR_SIGN, 0, __LINE__)) < 0))
                          {
                             return(INCORRECT);
                          }
                          if ((reply != 250) && (reply != 200))
                          {
                             return(reply);
                          }
                       }
                       else
                       {
                          return(reply);
                       }
                    }
                    else
                    {
                       return(reply);
                    }
                 }
                 else
                 {
                    return(reply);
                 }
              }
              else
              {
                 return(reply);
              }
           }
           else
           {
              return(reply);
           }
   }

   return(SUCCESS);
}


/*############################## ftp_dele() #############################*/
int
ftp_dele(char *filename)
{
   int reply;

   if ((reply = command(control_fd, "DELE %s", filename)) == SUCCESS)
   {
      if ((reply = get_reply(ERROR_SIGN, 250, __LINE__)) != INCORRECT)
      {
         if ((reply == 250) || (reply == 200))
         {
            reply = SUCCESS;
         }
      }
   }

   return(reply);
}


/*############################# ftp_noop() ##############################*/
int
ftp_noop(void)
{
   int reply;

   if ((reply = command(control_fd, "NOOP")) == SUCCESS)
   {
      if ((reply = get_reply(ERROR_SIGN, 200, __LINE__)) != INCORRECT)
      {
         if (reply == 200)
         {
            reply = SUCCESS;
         }
      }
   }

   return(reply);
}


/*########################### ftp_keepalive() ###########################*/
int
ftp_keepalive(void)
{
   if (simulation_mode == YES)
   {
      (void)command(control_fd, "STAT");
   }
   else
   {
      int           reply;
      long          tmp_transfer_timeout;
      unsigned char telnet_cmd[2];
      fd_set        wset;

      tmp_transfer_timeout = transfer_timeout;
      transfer_timeout = 0L;
      while ((reply = read_msg(ERROR_SIGN, __LINE__)) > 0)
      {
         trans_log(INFO_SIGN, __FILE__, __LINE__, "ftp_keepalive", msg_str,
                   _("Hmmm, read %d bytes."), reply);
      }
      timeout_flag = OFF;
      transfer_timeout = tmp_transfer_timeout;
      telnet_cmd[0] = IAC;
      telnet_cmd[1] = IP;

      /* Initialise descriptor set. */
      FD_ZERO(&wset);
      FD_SET(control_fd, &wset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = transfer_timeout;

      /* Wait for message x seconds and then continue. */
      reply = select(control_fd + 1, NULL, &wset, NULL, &timeout);

      if (reply == 0)
      {
         /* Timeout has arrived. */
         timeout_flag = ON;
         return(INCORRECT);
      }
      else if (FD_ISSET(control_fd, &wset))
           {
#ifdef WITH_SSL
              if (ssl_con == NULL)
              {
#endif
                 if ((reply = write(control_fd, telnet_cmd, 2)) != 2)
                 {
                    if ((errno == ECONNRESET) || (errno == EBADF))
                    {
                       timeout_flag = CON_RESET;
                    }
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_keepalive", NULL,
                              _("write() error (%d) : %s"),
                              reply, strerror(errno));
                    return(reply);
                 }
#ifdef WITH_SSL
              }
              else
              {
                 if ((reply = ssl_write(ssl_con, (char *)telnet_cmd, 2)) != 2)
                 {
                    return(reply);
                 }
              }
#endif
#ifdef WITH_TRACE
              trace_log(NULL, 0, W_TRACE, NULL, 0, "Telnet Interrupt IAC,IP");
#endif
           }
      else if (reply < 0)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_keepalive", NULL,
                        _("select() error : %s"), strerror(errno));
              return(INCORRECT);
           }
           else
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_keepalive", NULL,
                        _("Unknown condition."));
              return(INCORRECT);
           }

      /* Initialise descriptor set. */
      FD_SET(control_fd, &wset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = transfer_timeout;

      /* Wait for message x seconds and then continue. */
      reply = select(control_fd + 1, NULL, &wset, NULL, &timeout);

      if (reply == 0)
      {
         /* Timeout has arrived. */
         timeout_flag = ON;
         return(INCORRECT);
      }
      else if (FD_ISSET(control_fd, &wset))
           {
              telnet_cmd[1] = DM;
              if ((reply = send(control_fd, telnet_cmd, 2, MSG_OOB)) != 2)
              {
                 if ((errno == ECONNRESET) || (errno == EBADF))
                 {
                    timeout_flag = CON_RESET;
                 }
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_keepalive", NULL,
                           _("send() error (%d) : %s"), reply, strerror(errno));
                 return(errno);
              }
#ifdef WITH_TRACE
              trace_log(NULL, 0, W_TRACE, NULL, 0, "send MSG_OOB IAC, DM");
#endif
           }
      else if (reply < 0)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_keepalive", NULL,
                        _("select() error : %s"), strerror(errno));
              return(INCORRECT);
           }
           else
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_keepalive", NULL,
                        _("Unknown condition."));
              return(INCORRECT);
           }
      if (command(control_fd, "STAT") != SUCCESS)
      {
         return(INCORRECT);
      }

      reply = ftp_get_reply();

      /*
       * RFC 959 recomments 211, 212 or 213 as a responce to STAT. However,
       * there is no FTP Server that I know of that handles this correct
       * and since we do not evaluate the reply lets just accept anything
       * even a 500 as responce.
       */
      if (reply < 0)
      {
         return(reply);
      }
   }

   return(SUCCESS);
}


/*############################## ftp_size() #############################*/
int
ftp_size(char *filename, off_t *remote_size)
{
   int reply;

   if ((reply = command(control_fd, "SIZE %s", filename)) == SUCCESS)
   {
      if ((reply = get_reply(ERROR_SIGN, 999, __LINE__)) != INCORRECT)
      {
         if (reply == 213)
         {
            char *ptr = &msg_str[3];

            while ((*ptr == ' ') || (*ptr == '\t'))
            {
               ptr++;
            }
#ifdef HAVE_STRTOULL
# if SIZEOF_OFF_T == 4
            if ((*remote_size = (off_t)strtoul(ptr, NULL, 10)) != ULONG_MAX)
# else
            if ((*remote_size = (off_t)strtoull(ptr, NULL, 10)) != ULLONG_MAX)
# endif                                                             
#else
            if ((*remote_size = (off_t)strtoul(ptr, NULL, 10)) != ULONG_MAX)
#endif
            {
               reply = SUCCESS;
            }
            else
            {
               *remote_size = 0;
               reply = INCORRECT;
            }
         }
         else if (reply == 999)
              {
                 reply = SUCCESS;
              }
      }
      else
      {
         *remote_size = 0;
      }
   }
   else
   {
      *remote_size = 0;
   }

   return(reply);
}


/*############################## ftp_date() #############################*/
int
ftp_date(char *filename, time_t *file_mtime)
{
   int reply;

   if ((reply = command(control_fd, "MDTM %s", filename)) == SUCCESS)
   {
      if ((reply = get_reply(ERROR_SIGN, 999, __LINE__)) != INCORRECT)
      {
         if (reply == 213)
         {
            int  i = 0;
            char date[MAX_FTP_DATE_LENGTH],
                 *ptr = &msg_str[3];

            while ((*ptr == ' ') || (*ptr == '\t'))
            {
               ptr++;
            }

            /* Store date YYYYMMDDhhmmss. */
            while ((*ptr != '\0') && (i < MAX_FTP_DATE_LENGTH))
            {
               date[i] = *ptr;
               ptr++; i++;
            }
            if (i == (MAX_FTP_DATE_LENGTH - 1))
            {
               struct tm bd_time;

               date[i] = '\0';
               bd_time.tm_sec  = atoi(&date[i - 2]);
               date[i - 2] = '\0';
               bd_time.tm_min  = atoi(&date[i - 4]);
               date[i - 4] = '\0';
               bd_time.tm_hour = atoi(&date[i - 6]);
               date[i - 6] = '\0';
               bd_time.tm_mday = atoi(&date[i - 8]);
               date[i - 8] = '\0';
               bd_time.tm_mon = atoi(&date[i - 10]) - 1;
               date[i - 10] = '\0';
               bd_time.tm_year = atoi(date) - 1900;
               bd_time.tm_isdst = 0;
               *file_mtime = mktime(&bd_time);
            }
            else
            {
               *file_mtime = 0L;
            }
            reply = SUCCESS;
         }
         else if (reply == 999)
              {
                 reply = SUCCESS;
              }
      }
   }

   return(reply);
}


/*############################ ftp_set_date() ###########################*/
int
ftp_set_date(char *filename, time_t file_mtime)
{
   int reply;
   char date[MAX_FTP_DATE_LENGTH];

   (void)strftime(date, MAX_FTP_DATE_LENGTH, "%Y%m%d%H%M%S",
                  gmtime(&file_mtime));

   if ((reply = command(control_fd, "MDTM %s %s", date, filename)) == SUCCESS)
   {
      if ((reply = get_reply(ERROR_SIGN, 213, __LINE__)) != INCORRECT)
      {
         if (reply == 213)
         {
            reply = SUCCESS;
         }
      }
   }

   return(reply);
}


/*############################## ftp_mlst() #############################*/
int
ftp_mlst(char *filename, time_t *file_mtime)
{
   int reply;

   *file_mtime = 0L;
   if ((reply = command(control_fd, "MLST %s", filename)) == SUCCESS)
   {
      if ((reply = get_mlst_reply(ERROR_SIGN, 999,
                                  file_mtime, __LINE__)) != INCORRECT)
      {
         if ((reply == 250) && (*file_mtime != 0))
         {
            return(SUCCESS);
         }
      }
   }

   return(reply);
}


/*############################# ftp_exec() ##############################*/
int
ftp_exec(char *cmd, char *filename)
{
   int reply;

   if (filename == NULL)
   {
      reply = command(control_fd, "SITE %s", cmd);
   }
   else
   {
      reply = command(control_fd, "SITE %s %s", cmd, filename);
   }

   if ((reply == SUCCESS) &&
       ((reply = get_reply(ERROR_SIGN, 250, __LINE__)) != INCORRECT))
   {
      if ((reply == 250) || (reply == 200))
      {
         reply = SUCCESS;
      }
   }

   return(reply);
}


/*############################## ftp_list() #############################*/
int
ftp_list(int mode, int type, ...)
{
   char    **buffer,
           *filename,
           *msg;
   va_list ap;

   va_start(ap, type);
   if (type & BUFFERED_LIST)
   {
      buffer = va_arg(ap, char **);
      filename = NULL;
      msg = NULL;
   }
   else
   {
      filename = va_arg(ap, char *);
      msg = va_arg(ap, char *);
   }
   va_end(ap);
   msg_str[0] = '\0';

   if (simulation_mode == YES)
   {
      if (mode & PASSIVE_MODE)
      {
         if ((mode & EXTENDED_MODE) == 0)
         {
            (void)command(control_fd, "PASV");
         }
         else
         {
            (void)command(control_fd, "EPSV");
         }
      }
      else
      {
         if ((mode & EXTENDED_MODE) == 0)
         {
            (void)command(control_fd, "PORT xx,xx,xx,xx,xx,xx");
         }
         else
         {
            (void)command(control_fd, "EPRT |1|simulated ip|port|");
         }
      }

      if (type & MLSD_CMD)
      {
         (void)command(control_fd, "simulated MLSD");
      }
      else if (type & NLIST_CMD)
           {
              (void)command(control_fd, "simulated NLST");
           }
      else if (type & LIST_CMD)
           {
              if (filename == NULL)
              {
                 (void)command(control_fd, "simulated LIST");
              }
              else
              {
                 (void)command(control_fd, "simulated LIST %s", filename);
              }
           }
      else if (type & SLIST_CMD)
           {
              if (filename == NULL)
              {
                 (void)command(control_fd, "simulated STAT .");
              }
              else
              {
                 (void)command(control_fd, "simulated STAT %s", filename);
              }
           }
      else if (type & FNLIST_CMD)
           {
              (void)command(control_fd, "simulated NLST -a");
           }
           else /* FLIST_CMD */
           {
              if (filename == NULL)
              {
                 (void)command(control_fd, "simulated LIST -al");
              }
              else
              {
                 (void)command(control_fd, "simulated LIST -al %s", filename);
              }
           }

      return(SUCCESS);
   }
   else
   {
      if (type & SLIST_CMD)
      {
         int reply;

         /* With STATUS we can get a directory listing without */
         /* opening an extra socket to get a list.             */
         if (filename == NULL)
         {
            reply = command(control_fd, "STAT .");
         }
         else
         {
            reply = command(control_fd, "STAT %s", filename);
         }
         if (reply != SUCCESS)
         {
            return(INCORRECT);
         }

         if (type & BUFFERED_LIST)
         {
#ifdef WITH_SSL
            if (get_stat_reply(213, ssl_con, type, &buffer) < 0)
#else
            if (get_stat_reply(213, type, &buffer) < 0)
#endif
            {
               return(INCORRECT);
            }
            else
            {
               return(SUCCESS);
            }
         }
         else
         {
#ifdef WITH_SSL
            if (get_stat_reply(213, ssl_con, type, msg) < 0)
#else
            if (get_stat_reply(213, type, msg) < 0)
#endif
            {
               return(INCORRECT);
            }
            else
            {
               return(SUCCESS);
            }
         }
      }
      else
      {
         int new_sock_fd,
             on = 1,
             reply;

         if (mode & PASSIVE_MODE)
         {
            char *ptr;

#if defined (HAVE_GETADDRINFO) && defined (HAVE_GAI_STRERROR)
            if (ai_family == AF_INET6)
            {
               reply = command(control_fd, "EPSV");
               if ((reply != SUCCESS) ||
                   ((reply = get_reply(ERROR_SIGN, 0, __LINE__)) < 0))
               {
                  return(INCORRECT);
               }

               if (reply != 229)
               {
                  return(reply);
               }
            }
            else
            {
#endif
               if ((mode & EXTENDED_MODE) == 0)
               {
                  reply = command(control_fd, "PASV");
               }
               else
               {
                  reply = command(control_fd, "EPSV");
               }
               if ((reply != SUCCESS) ||
                   ((reply = get_reply(ERROR_SIGN, 0, __LINE__)) < 0))
               {
                  return(INCORRECT);
               }

               if ((mode & EXTENDED_MODE) == 0)
               {
                  if (reply != 227)
                  {
                     return(reply);
                  }
               }
               else
               {
                  if (reply != 229)
                  {
                     return(reply);
                  }
               }
#if defined (HAVE_GETADDRINFO) && defined (HAVE_GAI_STRERROR)
            }
#endif
            ptr = &msg_str[3];
            do
            {
               ptr++;
            } while ((*ptr != '(') && (*ptr != '\0'));
            if (*ptr == '(')
            {
               int number;

#if defined (HAVE_GETADDRINFO) && defined (HAVE_GAI_STRERROR)
               if (ai_family == AF_INET6)
               {
                  struct sockaddr_in6 data;

                  (void)memset(&data, 0, sizeof(data));
                  (void)memcpy(&data, ai_addr, ai_addrlen);
                  data.sin6_family = AF_INET6;

                  /* Note, only extended mode is supported in IPv6. */
                  if ((number = get_extended_number(ptr)) == INCORRECT)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                               _("Failed to retrieve remote address %s"), msg_str);
                     return(INCORRECT);
                  }
                  data.sin6_port = htons((u_short)number);

                  if ((new_sock_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP)) < 0)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                               _("socket() error : %s"), strerror(errno));
                     return(INCORRECT);
                  }
                  if (setsockopt(new_sock_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
                                 sizeof(on)) < 0)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                               _("setsockopt() error : %s"), strerror(errno));
                     (void)close(new_sock_fd);
                     return(INCORRECT);
                  }
                  fcd.data_port = data.sin6_port;
                  if (connect_with_timeout(new_sock_fd, (struct sockaddr *) &data,
                                           sizeof(data)) < 0)
                  {
                      if (errno)
                      {
#ifdef ETIMEDOUT
                         if (errno == ETIMEDOUT)
                         {
                            timeout_flag = ON;
                         }
# ifdef ECONNREFUSED
                         else if (errno == ECONNREFUSED)
                              {
                                 timeout_flag = CON_REFUSED;
                              }
# endif
#else
# ifdef ECONNREFUSED
                         if (errno == ECONNREFUSED)
                         {
                            timeout_flag = CON_REFUSED;
                         }
# endif
#endif
                         trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                                   _("connect() error : %s"), strerror(errno));
                      }
                      else
                      {
                         trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                                   _("connect() error"));
                      }
                      (void)close(new_sock_fd);
                      return(INCORRECT);
                  }
               }
               else
               {
#endif
                  struct sockaddr_in data;

                  data = ctrl;
                  data.sin_family = AF_INET;
                  data.sin_port = htons((u_short)0);
                  if ((mode & EXTENDED_MODE) == 0)
                  {
                     if ((number = get_number(&ptr, ',')) != INCORRECT)
                     {
                        if (mode & ALLOW_DATA_REDIRECT)
                        {
                           *((char *)&data.sin_addr) = number;
                        }
                        else
                        {
                           *((char *)&data.sin_addr) = *((char *)&sin.sin_addr);
                        }
                        if ((number = get_number(&ptr, ',')) != INCORRECT)
                        {
                           if (mode & ALLOW_DATA_REDIRECT)
                           {
                              *((char *)&data.sin_addr + 1) = number;
                           }
                           else
                           {
                              *((char *)&data.sin_addr + 1) = *((char *)&sin.sin_addr + 1);
                           }
                           if ((number = get_number(&ptr, ',')) != INCORRECT)
                           {
                              if (mode & ALLOW_DATA_REDIRECT)
                              {
                                 *((char *)&data.sin_addr + 2) = number;
                              }
                              else
                              {
                                 *((char *)&data.sin_addr + 2) = *((char *)&sin.sin_addr + 2);
                              }
                              if ((number = get_number(&ptr, ',')) != INCORRECT)
                              {
                                 if (mode & ALLOW_DATA_REDIRECT)
                                 {
                                    *((char *)&data.sin_addr + 3) = number;
                                 }
                                 else
                                 {
                                    *((char *)&data.sin_addr + 3) = *((char *)&sin.sin_addr + 3);
                                 }
                                 if ((number = get_number(&ptr, ',')) != INCORRECT)
                                 {
                                    *((char *)&data.sin_port) = number;
                                    if ((number = get_number(&ptr, ')')) != INCORRECT)
                                    {
                                       *((char *)&data.sin_port + 1) = number;
                                    }
                                 }
                              }
                           }
                        }
                     }
                     if (number == INCORRECT)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                                  _("Failed to retrieve remote address %s"), msg_str);
                        return(INCORRECT);
                     }
                  }
                  else
                  {
                     if ((number = get_extended_number(ptr)) == INCORRECT)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                                  _("Failed to retrieve remote address %s"), msg_str);
                        return(INCORRECT);
                     }
                     data.sin_port = htons((u_short)number);
                     *((char *)&data.sin_addr) = *((char *)&sin.sin_addr);
                     *((char *)&data.sin_addr + 1) = *((char *)&sin.sin_addr + 1);
                     *((char *)&data.sin_addr + 2) = *((char *)&sin.sin_addr + 2);
                     *((char *)&data.sin_addr + 3) = *((char *)&sin.sin_addr + 3);
                  }

                  if ((new_sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                               _("socket() error : %s"), strerror(errno));
                     return(INCORRECT);
                  }

                  if (setsockopt(new_sock_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
                                 sizeof(on)) < 0)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                               _("setsockopt() error : %s"), strerror(errno));
                     (void)close(new_sock_fd);
                     return(INCORRECT);
                  }
                  fcd.data_port = data.sin_port;
                  if (connect_with_timeout(new_sock_fd, (struct sockaddr *) &data, sizeof(data)) < 0)
                  {
                     if (errno)
                     {
#ifdef ETIMEDOUT
                        if (errno == ETIMEDOUT)
                        {
                           timeout_flag = ON;
                        }
# ifdef ECONNREFUSED
                        else if (errno == ECONNREFUSED)
                             {
                                timeout_flag = CON_REFUSED;
                             }
# endif
#else
# ifdef ECONNREFUSED
                        if (errno == ECONNREFUSED)
                        {
                           timeout_flag = CON_REFUSED;
                        }
# endif
#endif
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                                  _("connect() error : %s"), strerror(errno));
                     }
                     else
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                                  _("connect() error"));
                     }
                     (void)close(new_sock_fd);
                     return(INCORRECT);
                  }
#if defined (HAVE_GETADDRINFO) && defined (HAVE_GAI_STRERROR)
               }
#endif

               if (type & MLSD_CMD)
               {
                  reply = command(control_fd, "MLSD");
               }
               else if (type & NLIST_CMD)
                    {
                       reply = command(control_fd, "NLST");
                    }
               else if (type & LIST_CMD)
                    {
                       if (filename == NULL)
                       {
                          reply = command(control_fd, "LIST");
                       }
                       else
                       {
                          reply = command(control_fd, "LIST %s", filename);
                       }
                    }
               else if (type & FNLIST_CMD)
                    {
                       reply = command(control_fd, "NLST -a");
                    }
                    else /* FLIST_CMD */
                    {
                       if (filename == NULL)
                       {
                          reply = command(control_fd, "LIST -al");
                       }
                       else
                       {
                          reply = command(control_fd, "LIST -al %s", filename);
                       }
                    }

               if ((reply != SUCCESS) ||
                   ((reply = get_reply(ERROR_SIGN, 0, __LINE__)) < 0))
               {
                  (void)close(new_sock_fd);
                  return(INCORRECT);
               }

               if ((reply != 150) && (reply != 125))
               {
                  (void)close(new_sock_fd);
                  return(reply);
               }
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", msg_str,
                         _("Failed to locate an open bracket <(> in reply from PASV command."));
               return(INCORRECT);
            }
         }
         else /* mode & ACTIVE_MODE */
         {
            int          sock_fd,
                         tmp_errno;
            my_socklen_t length;

#if defined (HAVE_GETADDRINFO) && defined (HAVE_GAI_STRERROR)
            if (ai_family == AF_INET6)
            {
               char                buf[64];
               struct sockaddr_in6 data;

               (void)memset(&data, 0, sizeof(data));
               (void)memcpy(&data, ai_addr, ai_addrlen);
               data.sin6_family = AF_INET6;
               data.sin6_port = htons((u_short)0);

               if ((sock_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP)) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                            _("socket() error : %s"), strerror(errno));
                  return(INCORRECT);
               }

               if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
                              sizeof(on)) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                            _("setsockopt() error : %s"), strerror(errno));
                  (void)close(sock_fd);
                  return(INCORRECT);
               }

               length = sizeof(data);
               if (bind(sock_fd, (struct sockaddr *)&data, length) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                            _("bind() error : %s"), strerror(errno));
                  (void)close(sock_fd);
                  return(INCORRECT);
               }

               if (getsockname(sock_fd, (struct sockaddr *)&data, &length) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                            _("getsockname() error : %s"), strerror(errno));
                  (void)close(sock_fd);
                  return(INCORRECT);
               }

               if (listen(sock_fd, 1) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                            _("listen() error : %s"), strerror(errno));
                  (void)close(sock_fd);
                  return(INCORRECT);
               }

               if (inet_ntop(AF_INET6, &data.sin6_addr, buf, sizeof(buf)) == NULL)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                            "Cannot get address of local socket : %s",
                            strerror(errno));
                  (void)close(sock_fd);
                  return(INCORRECT);
               }
               fcd.data_port = data.sin6_port;

               /* Note, only extended mode is supported in IPv6. */
               reply = command(control_fd, "EPRT |2|%s|%d|",
                               buf, (int)ntohs(data.sin6_port));
            }
            else
            {
#endif
               register char      *h,
                                  *p;
               struct sockaddr_in data;

               data = ctrl;
               data.sin_family = AF_INET;
               data.sin_port = htons((u_short)0);

               if ((sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                            _("socket() error : %s"), strerror(errno));
                  return(INCORRECT);
               }

               if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
                              sizeof(on)) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                            _("setsockopt() error : %s"), strerror(errno));
                  (void)close(sock_fd);
                  return(INCORRECT);
               }

               length = sizeof(data);
               if (bind(sock_fd, (struct sockaddr *)&data, length) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                            _("bind() error : %s"), strerror(errno));
                  (void)close(sock_fd);
                  return(INCORRECT);
               }

               if (getsockname(sock_fd, (struct sockaddr *)&data, &length) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                            _("getsockname() error : %s"), strerror(errno));
                  (void)close(sock_fd);
                  return(INCORRECT);
               }

               if (listen(sock_fd, 1) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                            _("listen() error : %s"), strerror(errno));
                  (void)close(sock_fd);
                  return(INCORRECT);
               }

               h = (char *)&data.sin_addr;
               p = (char *)&data.sin_port;
               fcd.data_port = data.sin_port;
               if ((mode & EXTENDED_MODE) == 0)
               {
                  reply = command(control_fd, "PORT %d,%d,%d,%d,%d,%d",
                                  (((int)h[0]) & 0xff), (((int)h[1]) & 0xff),
                                  (((int)h[2]) & 0xff), (((int)h[3]) & 0xff),
                                  (((int)p[0]) & 0xff), (((int)p[1]) & 0xff));
               }
               else
               {
                  reply = command(control_fd, "EPRT |1|%s|%d|",
                                  inet_ntoa(data.sin_addr),
                                  (int)ntohs(data.sin_port));
               }
#if defined (HAVE_GETADDRINFO) && defined (HAVE_GAI_STRERROR)
            }
#endif

            if ((reply != SUCCESS) ||
                ((reply = get_reply(ERROR_SIGN, 0, __LINE__)) < 0))
            {
               (void)close(sock_fd);
               return(INCORRECT);
            }

            if (reply != 200)
            {
               (void)close(sock_fd);
               return(reply);
            }

            if (type & MLSD_CMD)
            {
               reply = command(control_fd, "MLSD");
            }
            else if (type & NLIST_CMD)
                 {
                    reply = command(control_fd, "NLST");
                 }
            else if (type & LIST_CMD)
                 {
                    if (filename == NULL)
                    {
                       reply = command(control_fd, "LIST");
                    }
                    else
                    {
                       reply = command(control_fd, "LIST %s", filename);
                    }
                 }
            else if (type & FNLIST_CMD)
                 {
                    reply = command(control_fd, "NLST -a");
                 }
                 else /* FLIST_CMD */
                 {
                    if (filename == NULL)
                    {
                       reply = command(control_fd, "LIST -al");
                    }
                    else
                    {
                       reply = command(control_fd, "LIST -al %s", filename);
                    }
                 }

            if ((reply != SUCCESS) ||
                ((reply = get_reply(ERROR_SIGN, 0, __LINE__)) < 0))
            {
               return(INCORRECT);
            }

            if ((reply != 150) && (reply != 125))
            {
               return(reply);
            }

            /*
             * Juck! Here follows some ugly code.
             * Experience has shown that on very rare occasions the
             * accept() call blocks. This is normal behaviour of the accept()
             * system call. Could make sock_fd non-blocking, but then
             * the next time we use the socket we will get an error. This is
             * not we want. When we have a bad connection it can take quit
             * some time before we get a respond.
             */
            if (signal(SIGALRM, sig_handler) == SIG_ERR)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                         _("Failed to set signal handler : %s"), strerror(errno));
               (void)close(sock_fd);
               return(INCORRECT);
            }
            if (sigsetjmp(env_alrm, 1) != 0)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                         _("accept() timeout (%lds)"), transfer_timeout);
               timeout_flag = ON;
               (void)close(sock_fd);
               return(INCORRECT);
            }
            (void)alarm(transfer_timeout);
            new_sock_fd = accept(sock_fd, NULL, NULL);
            tmp_errno = errno;
            (void)alarm(0);

            if (new_sock_fd < 0)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                         _("accept() error : %s"), strerror(tmp_errno));
               (void)close(sock_fd);
               return(INCORRECT);
            }
            if (close(sock_fd) == -1)
            {
               trans_log(DEBUG_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                         _("close() error : %s"), strerror(errno));
            }
         } /* mode == ACTIVE_MODE */

#ifdef WITH_SSL
         if ((type & ENCRYPT_DATA) && (ssl_con != NULL))
         {
            encrypt_data_connection(new_sock_fd);
         }
#endif

         if (type & BUFFERED_LIST)
         {
#ifdef WITH_SSL
            if (read_data_to_buffer(new_sock_fd, ssl_data, &buffer) < 0)
#else
            if (read_data_to_buffer(new_sock_fd, &buffer) < 0)
#endif
            {
               return(INCORRECT);
            }
         }
         else
         {
#ifdef WITH_SSL
            if (read_data_line(new_sock_fd, ssl_data, msg) < 0)
#else
            if (read_data_line(new_sock_fd, msg) < 0)
#endif
            {
               return(INCORRECT);
            }
         }

#ifdef WITH_SSL
         if ((type & ENCRYPT_DATA) && (ssl_data != NULL))
         {
            if (timeout_flag != CON_RESET)
            {
               if (SSL_shutdown(ssl_data) == 0)
               {
                  (void)SSL_shutdown(ssl_data);
               }
            }
            SSL_free(ssl_data);
            ssl_data = NULL;
         }
#endif

#ifdef _WITH_SHUTDOWN
         if (shutdown(new_sock_fd, 1) < 0)
         {
            trans_log(DEBUG_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                      _("shutdown() error : %s"), strerror(errno));
         }
#endif

         if (close(new_sock_fd) == -1)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_list", NULL,
                      _("close() error : %s"), strerror(errno));
            return(INCORRECT);
         }

         /* Read last message: 'Binary Transfer complete'. */
         if ((reply = get_reply(ERROR_SIGN, 0, __LINE__)) != INCORRECT)
         {
            if ((reply == 226) || (reply == 250))
            {
               reply = SUCCESS;
            }
         }

         return(reply);
      }
   }
}


/*############################## ftp_data() #############################*/
int
ftp_data(char  *filename,
         off_t seek,
         int   mode,
         int   type,
         int   sockbuf_size,
         int   create_dir,
         char  *dir_mode,
         char  *created_path)
{
   char cmd[5];

   if (simulation_mode == YES)
   {
      if (type == DATA_WRITE)
      {
         if (seek == 0)
         {
            cmd[0] = 'S'; cmd[1] = 'T'; cmd[2] = 'O'; cmd[3] = 'R';
         }
         else
         {
            cmd[0] = 'A'; cmd[1] = 'P'; cmd[2] = 'P'; cmd[3] = 'E';
         }
      }
      else
      {
         cmd[0] = 'R'; cmd[1] = 'E'; cmd[2] = 'T'; cmd[3] = 'R';
      }
      cmd[4] = '\0';

      if (mode & PASSIVE_MODE)
      {
         if ((mode & EXTENDED_MODE) == 0)
         {
            (void)command(control_fd, "PASV");
         }
         else
         {
            (void)command(control_fd, "EPSV");
         }
      }
      else
      {
         if ((mode & EXTENDED_MODE) == 0)
         {
            (void)command(control_fd, "PORT xx,xx,xx,xx,xx,xx");
         }
         else
         {
            (void)command(control_fd, "EPRT |1|simulated ip|port|");
         }
      }

      if ((seek > 0) && (type == DATA_READ))
      {
#if SIZEOF_OFF_T == 4
         (void)command(control_fd, "REST %ld", (pri_off_t)seek);
#else
         (void)command(control_fd, "REST %lld", (pri_off_t)seek);
#endif
      }

      (void)command(control_fd, "%s %s", cmd, filename);

      if ((data_fd = open("/dev/null", O_RDWR)) == -1)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                   _("Failed to open() /dev/null : %s"), strerror(errno));
         return(INCORRECT);
      }
   }
   else
   {
      int           new_sock_fd,
                    on = 1,
                    reply;
#if defined (_WITH_TOS) && defined (IP_TOS) && defined (IPTOS_THROUGHPUT)
      int           tos;
#endif
#ifdef FTX
      struct linger l;
#endif

      if (type == DATA_WRITE)
      {
         if (seek == 0)
         {
            cmd[0] = 'S'; cmd[1] = 'T'; cmd[2] = 'O'; cmd[3] = 'R';
         }
         else
         {
            cmd[0] = 'A'; cmd[1] = 'P'; cmd[2] = 'P'; cmd[3] = 'E';
         }
      }
      else
      {
         cmd[0] = 'R'; cmd[1] = 'E'; cmd[2] = 'T'; cmd[3] = 'R';
      }
      cmd[4] = '\0';

      if (mode & PASSIVE_MODE)
      {
         char *ptr;

#if defined (HAVE_GETADDRINFO) && defined (HAVE_GAI_STRERROR)
         if (ai_family == AF_INET6)
         {
            if (command(control_fd, "EPSV") != SUCCESS)
            {
               return(INCORRECT);
            }
            if ((reply = get_reply(ERROR_SIGN, 0, __LINE__)) < 0)
            {
               if (timeout_flag == OFF)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                            _("Failed to get reply after sending EPSV command (%d)."),
                            reply);
               }
               return(INCORRECT);
            }

            if (reply != 229)
            {
               return(reply);
            }
         }
         else
         {
#endif
            if ((mode & EXTENDED_MODE) == 0)
            {
               if (command(control_fd, "PASV") != SUCCESS)
               {
                  return(INCORRECT);
               }
            }
            else
            {
               if (command(control_fd, "EPSV") != SUCCESS)
               {
                  return(INCORRECT);
               }
            }
            if ((reply = get_reply(ERROR_SIGN, 0, __LINE__)) < 0)
            {
               if (timeout_flag == OFF)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                            _("Failed to get reply after sending %s command (%d)."),
                            ((mode & EXTENDED_MODE) == 0) ? "PASV" : "EPSV",
                            reply);
               }
               return(INCORRECT);
            }

            if ((mode & EXTENDED_MODE) == 0)
            {
               if (reply != 227)
               {
                  return(reply);
               }
            }
            else
            {
               if (reply != 229)
               {
                  return(reply);
               }
            }
#if defined (HAVE_GETADDRINFO) && defined (HAVE_GAI_STRERROR)
         }
#endif
         ptr = &msg_str[3];
         do
         {
            ptr++;
         } while ((*ptr != '(') && (*ptr != '\0'));
         if (*ptr == '(')
         {
            int number;

#if defined (HAVE_GETADDRINFO) && defined (HAVE_GAI_STRERROR)
            if (ai_family == AF_INET6)
            {
               struct sockaddr_in6 data;

               (void)memset(&data, 0, sizeof(data));
               (void)memcpy(&data, ai_addr, ai_addrlen);
               data.sin6_family = AF_INET6;

               /* Note, only extended mode is supported in IPv6. */
               if ((number = get_extended_number(ptr)) == INCORRECT)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                            _("Failed to retrieve remote address %s"), msg_str);
                  return(INCORRECT);
               }
               data.sin6_port = htons((u_short)number);

               if ((new_sock_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP)) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                            _("socket() error : %s"), strerror(errno));
                  return(INCORRECT);
               }

               if (setsockopt(new_sock_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
                              sizeof(on)) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                            _("setsockopt() error : %s"), strerror(errno));
                  (void)close(new_sock_fd);
                  return(INCORRECT);
               }
               if (sockbuf_size > 0)
               {
                  int optname;

                  if (type == DATA_WRITE)
                  {
                     optname = SO_SNDBUF;
                  }
                  else
                  {
                     optname = SO_RCVBUF;
                  }
                  if (setsockopt(new_sock_fd, SOL_SOCKET, optname,
                                 (char *)&sockbuf_size, sizeof(sockbuf_size)) < 0)
                  {
                     trans_log(WARN_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                               _("setsockopt() error : %s"), strerror(errno));
                  }
               }
               fcd.data_port = data.sin6_port;
               if (connect_with_timeout(new_sock_fd, (struct sockaddr *) &data, sizeof(data)) < 0)
               {
                  if (errno)
                  {
#ifdef ETIMEDOUT
                     if (errno == ETIMEDOUT)
                     {
                        timeout_flag = ON;
                     }
# ifdef ECONNREFUSED
                     else if (errno == ECONNREFUSED)
                          {
                             timeout_flag = CON_REFUSED;
                          }
# endif
#else
# ifdef ECONNREFUSED
                     if (errno == ECONNREFUSED)
                     {
                        timeout_flag = CON_REFUSED;
                     }
# endif
#endif
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                               _("connect() error : %s"), strerror(errno));
                  }
                  else
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                               _("connect() error"));
                  }
                  (void)close(new_sock_fd);
                  return(INCORRECT);
               }
            }
            else
            {
#endif
               struct sockaddr_in data;

               data = ctrl;
               data.sin_family = AF_INET;
               if ((mode & EXTENDED_MODE) == 0)
               {
                  if ((number = get_number(&ptr, ',')) != INCORRECT)
                  {
                     if (mode & ALLOW_DATA_REDIRECT)
                     {
                        *((char *)&data.sin_addr) = number;
                     }
                     else
                     {
                        *((char *)&data.sin_addr) = *((char *)&sin.sin_addr);
                     }
                     if ((number = get_number(&ptr, ',')) != INCORRECT)
                     {
                        if (mode & ALLOW_DATA_REDIRECT)
                        {
                           *((char *)&data.sin_addr + 1) = number;
                        }
                        else
                        {
                           *((char *)&data.sin_addr + 1) = *((char *)&sin.sin_addr + 1);
                        }
                        if ((number = get_number(&ptr, ',')) != INCORRECT)
                        {
                           if (mode & ALLOW_DATA_REDIRECT)
                           {
                              *((char *)&data.sin_addr + 2) = number;
                           }
                           else
                           {
                              *((char *)&data.sin_addr + 2) = *((char *)&sin.sin_addr + 2);
                           }
                           if ((number = get_number(&ptr, ',')) != INCORRECT)
                           {
                              if (mode & ALLOW_DATA_REDIRECT)
                              {
                                 *((char *)&data.sin_addr + 3) = number;
                              }
                              else
                              {
                                 *((char *)&data.sin_addr + 3) = *((char *)&sin.sin_addr + 3);
                              }
                              if ((number = get_number(&ptr, ',')) != INCORRECT)
                              {
                                 *((char *)&data.sin_port) = number;
                                 if ((number = get_number(&ptr, ')')) != INCORRECT)
                                 {
                                    *((char *)&data.sin_port + 1) = number;
                                 }
                              }
                           }
                        }
                     }
                  }
                  if (number == INCORRECT)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                               _("Failed to retrieve remote address %s"), msg_str);
                     return(INCORRECT);
                  }
               }
               else
               {
                  if ((number = get_extended_number(ptr)) == INCORRECT)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                               _("Failed to retrieve remote address %s"), msg_str);
                     return(INCORRECT);
                  }
                  data.sin_port = htons((u_short)number);
                  *((char *)&data.sin_addr) = *((char *)&sin.sin_addr);
                  *((char *)&data.sin_addr + 1) = *((char *)&sin.sin_addr + 1);
                  *((char *)&data.sin_addr + 2) = *((char *)&sin.sin_addr + 2);
                  *((char *)&data.sin_addr + 3) = *((char *)&sin.sin_addr + 3);
               }

               msg_str[0] = '\0';
               if ((new_sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                            _("socket() error : %s"), strerror(errno));
                  return(INCORRECT);
               }

               if (setsockopt(new_sock_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
                              sizeof(on)) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                            _("setsockopt() error : %s"), strerror(errno));
                  (void)close(new_sock_fd);
                  return(INCORRECT);
               }
               if (sockbuf_size > 0)
               {
                  int optname;

                  if (type == DATA_WRITE)
                  {
                     optname = SO_SNDBUF;
                  }
                  else
                  {
                     optname = SO_RCVBUF;
                  }
                  if (setsockopt(new_sock_fd, SOL_SOCKET, optname,
                                 (char *)&sockbuf_size, sizeof(sockbuf_size)) < 0)
                  {
                     trans_log(WARN_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                               _("setsockopt() error : %s"), strerror(errno));
                  }
               }
               fcd.data_port = data.sin_port;
               if (connect_with_timeout(new_sock_fd, (struct sockaddr *) &data, sizeof(data)) < 0)
               {
                  char *h,
                       *p;

                  h = (char *)&data.sin_addr;
                  p = (char *)&data.sin_port;
                  if (errno)
                  {
#ifdef ETIMEDOUT
                     if (errno == ETIMEDOUT)
                     {
                        timeout_flag = ON;
                     }
# ifdef ECONNREFUSED
                     else if (errno == ECONNREFUSED)
                          {
                             timeout_flag = CON_REFUSED;
                          }
# endif
#else
# ifdef ECONNREFUSED
                     if (errno == ECONNREFUSED)
                     {
                        timeout_flag = CON_REFUSED;
                     }
# endif
#endif
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                               _("connect() error (%d,%d,%d,%d,%d,%d) : %s"),
                               (((int)h[0]) & 0xff), (((int)h[1]) & 0xff),
                               (((int)h[2]) & 0xff), (((int)h[3]) & 0xff),
                               (((int)p[0]) & 0xff), (((int)p[1]) & 0xff),
                               strerror(errno));
                  }
                  else
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                               _("connect() error (%d,%d,%d,%d,%d,%d)"),
                               (((int)h[0]) & 0xff), (((int)h[1]) & 0xff),
                               (((int)h[2]) & 0xff), (((int)h[3]) & 0xff),
                               (((int)p[0]) & 0xff), (((int)p[1]) & 0xff));
                  }
                  (void)close(new_sock_fd);
                  return(INCORRECT);
               }
#if defined (HAVE_GETADDRINFO) && defined (HAVE_GAI_STRERROR)
            }
#endif

            /*
             * When we retrieve a file and part of it has already been
             * retrieved, tell the remote ftp server where to put the server
             * marker. No need to resend the data we already have.
             */
            if ((seek > 0) && (type == DATA_READ))
            {
#if SIZEOF_OFF_T == 4
               if (command(control_fd, "REST %ld", (pri_off_t)seek) != SUCCESS)
#else
               if (command(control_fd, "REST %lld", (pri_off_t)seek) != SUCCESS)
#endif
               {
                  (void)close(new_sock_fd);
                  return(INCORRECT);
               }
               if ((reply = get_reply(ERROR_SIGN, 0, __LINE__)) < 0)
               {
                  if (timeout_flag == OFF)
                  {
                     trans_log(WARN_SIGN, __FILE__, __LINE__, "ftp_data", msg_str,
                               _("Failed to get proper reply for REST command (%d)."),
                               reply);
                  }
                  else
                  {
                     (void)close(new_sock_fd);
                     return(INCORRECT);
                  }
               }
               else
               {
                  if (reply != 350)
                  {
                     trans_log(WARN_SIGN, __FILE__, __LINE__, "ftp_data", msg_str,
                               _("Failed to get proper reply for REST command (%d)."),
                               reply);
                  }
               }
            }

            if (command(control_fd, "%s %s", cmd, filename) != SUCCESS)
            {
               (void)close(new_sock_fd);
               return(INCORRECT);
            }
            if ((reply = get_reply(ERROR_SIGN, 0, __LINE__)) < 0)
            {
               if (timeout_flag == OFF)
               {
                  trans_log(WARN_SIGN, __FILE__, __LINE__, "ftp_data", msg_str,
                            _("Failed to get proper reply (%d)."), reply);
               }
               (void)close(new_sock_fd);
               return(INCORRECT);
            }

            if ((reply != 150) && (reply != 125))
            {
               /*
                * Assume that we may not overwrite the remote file so let's
                * just delete it and try again.
                * This is not 100% correct since there is no sure way to
                * tell that this is an overwrite error, since there are
                * just to many different return codes possible:
                *    553 (Upload)
                *    550 (Filename (deny))
                *    553 (Overwrite)
                * So lets just do it for those cases we know and where
                * a delete makes sence.
                */
               if ((((reply == 553) &&
                     (lposi(&msg_str[3], "(Overwrite)", 11) != NULL)) ||
                    ((reply == 550) &&
                     (lposi(&msg_str[3], "Overwrite permission denied",
                            27) != NULL))) &&
                    (ftp_dele(filename) == SUCCESS))
               {
                  if (command(control_fd, "%s %s", cmd, filename) != SUCCESS)
                  {
                     (void)close(new_sock_fd);
                     return(INCORRECT);
                  }
                  if ((reply = get_reply(ERROR_SIGN, 0, __LINE__)) < 0)
                  {
                     if (timeout_flag == OFF)
                     {
                        trans_log(WARN_SIGN, __FILE__, __LINE__, "ftp_data", msg_str,
                                  _("Failed to get proper reply (%d) for command: %s %s"),
                                  reply, cmd, filename);         
                     }
                     (void)close(new_sock_fd);
                     return(INCORRECT);
                  }
                  if ((reply != 150) && (reply != 125))
                  {
                     (void)close(new_sock_fd);
                     return(INCORRECT);
                  }
               }
          else if (((reply == 550) || (reply == 553)) && (create_dir == YES))
               {
                  char *ptr,
                       to_dir[MAX_PATH_LENGTH];

                  (void)strcpy(to_dir, filename);
                  ptr = to_dir + strlen(to_dir) - 1;
                  while ((*ptr == '/') && (ptr > to_dir))
                  {
                     ptr--;
                  }
                  while ((*ptr != '/') && (ptr > to_dir))
                  {
                     ptr--;
                  }
                  if ((*ptr == '/') && (to_dir != ptr))
                  {
                     *ptr = '\0';
                     *(ptr + 1) = '\0';
                     if (ftp_pwd() == SUCCESS)
                     {
                        char current_dir[MAX_PATH_LENGTH];

                        (void)strcpy(current_dir, msg_str);

                        /* The following will actually create the directory. */
                        if (ftp_cd(to_dir, YES, dir_mode, created_path) == SUCCESS)
                        {
                           if (ftp_cd(current_dir, NO, "", NULL) == SUCCESS)
                           {
                              if (command(control_fd, "%s %s", cmd, filename) != SUCCESS)
                              {
                                 (void)close(new_sock_fd);
                                 return(INCORRECT);
                              }
                              if ((reply = get_reply(ERROR_SIGN, 0, __LINE__)) < 0)
                              {
                                 if (timeout_flag == OFF)
                                 {
                                    trans_log(WARN_SIGN, __FILE__, __LINE__, "ftp_data", msg_str,
                                              _("Failed to get proper reply (%d)."), reply);
                                 }
                                 (void)close(new_sock_fd);
                                 return(INCORRECT);
                              }

                              if ((reply != 150) && (reply != 125))
                              {
                                 (void)close(new_sock_fd);
                                 return(-reply);
                              }
                           }
                           else
                           {
                              (void)close(new_sock_fd);
                              return(-reply);
                           }
                        }
                        else
                        {
                           (void)close(new_sock_fd);
                           return(-reply);
                        }
                     }
                     else
                     {
                        (void)close(new_sock_fd);
                        return(-reply);
                     }
                  }
                  else
                  {
                     (void)close(new_sock_fd);
                     return(-reply);
                  }
               }
               else
               {
                  (void)close(new_sock_fd);
                  return(-reply);
               }
            }
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", msg_str,
                      _("Failed to locate an open bracket <(> in reply from PASV command."));
            return(INCORRECT);
         }
      }
      else /* ACTIVE_MODE */
      {
         int            retries = 0,
                        ret,
                        sock_fd,
                        tmp_errno;
         my_socklen_t   length;
#ifdef FTP_REUSE_DATA_PORT
         unsigned int   loop_counter = 0;
         static u_short data_port = 0;
#endif
         register char  *h, *p;

         do
         {
            msg_str[0] = '\0';
#if defined (HAVE_GETADDRINFO) && defined (HAVE_GAI_STRERROR)
            if (ai_family == AF_INET6)
            {
               char                buf[64];
               struct sockaddr_in6 data;

               (void)memset(&data, 0, sizeof(data));
               (void)memcpy(&data, ai_addr, ai_addrlen);
               data.sin6_family = AF_INET6;
# ifdef FTP_REUSE_DATA_PORT
try_again_ipv6:
               if (type != DATA_READ)
               {
                  data.sin6_port = htons(data_port);
               }
               else
               {
                  data.sin6_port = htons((u_short)0);
               }
# else
               data.sin6_port = htons((u_short)0);
# endif
               if ((sock_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP)) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                            _("socket() error : %s"), strerror(errno));
                  return(INCORRECT);
               }

               if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
                              sizeof(on)) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                            _("setsockopt() error : %s"), strerror(errno));
                  (void)close(sock_fd);
                  return(INCORRECT);
               }

               length = sizeof(data);
               if (bind(sock_fd, (struct sockaddr *)&data, length) < 0)
               {
# ifdef FTP_REUSE_DATA_PORT
                  if ((type != DATA_READ) &&
                      ((errno == EADDRINUSE) || (errno == EACCES)))
                  {
                     data_port = 0;
                     loop_counter++;
                     if (loop_counter < 100)
                     {
                        goto try_again_ipv6;
                     }
                  }
# endif
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                            _("bind() error : %s"), strerror(errno));
                  (void)close(sock_fd);
                  return(INCORRECT);
               }

# ifdef FTP_REUSE_DATA_PORT
               if ((type == DATA_READ) || (data_port == (u_short)0))
               {
# endif
                  if (getsockname(sock_fd, (struct sockaddr *)&data, &length) < 0)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                               _("getsockname() error : %s"), strerror(errno));
                     (void)close(sock_fd);
                     return(INCORRECT);
                  }
# ifdef FTP_REUSE_DATA_PORT
               }
# endif
               if (sockbuf_size > 0)
               {
                  int optname;

                  if (type == DATA_WRITE)
                  {
                     optname = SO_SNDBUF;
                  }
                  else
                  {
                     optname = SO_RCVBUF;
                  }
                  if (setsockopt(sock_fd, SOL_SOCKET, optname,
                                 (char *)&sockbuf_size, sizeof(sockbuf_size)) < 0)
                  {
                     trans_log(WARN_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                               _("setsockopt() error : %s"), strerror(errno));
                  }
               }

               if (listen(sock_fd, 1) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                            _("listen() error : %s"), strerror(errno));
                  (void)close(sock_fd);
                  return(INCORRECT);
               }

# ifdef FTP_REUSE_DATA_PORT
               data_port = data.sin6_port;
# endif
               fcd.data_port = data.sin6_port;
               if (inet_ntop(AF_INET6, &data.sin6_addr, buf, sizeof(buf)) == NULL)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                            "Cannot get address of local socket : %s",
                            strerror(errno));
                  (void)close(sock_fd);
                  return(INCORRECT);
               }

               /* Note, only extended mode is supported in IPv6. */
               if (command(control_fd, "EPRT |2|%s|%d|",
                           buf, (int)ntohs(data.sin6_port)) != SUCCESS)
               {
                  (void)close(sock_fd);
                  return(INCORRECT);
               }
            }
            else
            {
#endif
               struct sockaddr_in data;

               data = ctrl;
               data.sin_family = AF_INET;
#ifdef FTP_REUSE_DATA_PORT
try_again:
               if (type != DATA_READ)
               {
                  data.sin_port = htons(data_port);
               }
               else
               {
                  data.sin_port = htons((u_short)0);
               }
#else
               data.sin_port = htons((u_short)0);
#endif

               if ((sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                            _("socket() error : %s"), strerror(errno));
                  return(INCORRECT);
               }

               if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
                              sizeof(on)) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                            _("setsockopt() error : %s"), strerror(errno));
                  (void)close(sock_fd);
                  return(INCORRECT);
               }

               length = sizeof(data);
               if (bind(sock_fd, (struct sockaddr *)&data, length) < 0)
               {
#ifdef FTP_REUSE_DATA_PORT
                  if ((type != DATA_READ) &&
                      ((errno == EADDRINUSE) || (errno == EACCES)))
                  {
                     data_port = 0;
                     loop_counter++;
                     if (loop_counter < 100)
                     {
                        goto try_again;
                     }
                  }
#endif
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                            _("bind() error : %s"), strerror(errno));
                  (void)close(sock_fd);
                  return(INCORRECT);
               }

#ifdef FTP_REUSE_DATA_PORT
               if ((type == DATA_READ) || (data_port == (u_short)0))
               {
#endif
                  if (getsockname(sock_fd, (struct sockaddr *)&data, &length) < 0)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                               _("getsockname() error : %s"), strerror(errno));
                     (void)close(sock_fd);
                     return(INCORRECT);
                  }
#ifdef FTP_REUSE_DATA_PORT
               }
#endif
               if (sockbuf_size > 0)
               {
                  int optname;

                  if (type == DATA_WRITE)
                  {
                     optname = SO_SNDBUF;
                  }
                  else
                  {
                     optname = SO_RCVBUF;
                  }
                  if (setsockopt(sock_fd, SOL_SOCKET, optname,
                                 (char *)&sockbuf_size, sizeof(sockbuf_size)) < 0)
                  {
                     trans_log(WARN_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                               _("setsockopt() error : %s"), strerror(errno));
                  }
               }

               if (listen(sock_fd, 1) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                            _("listen() error : %s"), strerror(errno));
                  (void)close(sock_fd);
                  return(INCORRECT);
               }

               h = (char *)&data.sin_addr;
               p = (char *)&data.sin_port;
#ifdef FTP_REUSE_DATA_PORT
               data_port = data.sin_port;
#endif
               fcd.data_port = data.sin_port;

               if ((mode & EXTENDED_MODE) == 0)
               {
                  if (command(control_fd, "PORT %d,%d,%d,%d,%d,%d",
                              (((int)h[0]) & 0xff), (((int)h[1]) & 0xff),
                              (((int)h[2]) & 0xff), (((int)h[3]) & 0xff),
                              (((int)p[0]) & 0xff), (((int)p[1]) & 0xff)) != SUCCESS)
                  {
                     (void)close(sock_fd);
                     return(INCORRECT);
                  }
               }
               else
               {
                  if (command(control_fd, "EPRT |1|%s|%d|",
                              inet_ntoa(data.sin_addr),
                              (int)ntohs(data.sin_port)) != SUCCESS)
                  {
                     (void)close(sock_fd);
                     return(INCORRECT);
                  }
               }
#if defined (HAVE_GETADDRINFO) && defined (HAVE_GAI_STRERROR)
            }
#endif
            if ((reply = get_reply(ERROR_SIGN, 0, __LINE__)) < 0)
            {
               if (timeout_flag == OFF)
               {
                  trans_log(WARN_SIGN, __FILE__, __LINE__, "ftp_data", msg_str,
                            _("Failed to get proper reply (%d)."), reply);
               }
               (void)close(sock_fd);
               return(INCORRECT);
            }

            if (reply != 200)
            {
               (void)close(sock_fd);
               return(reply);
            }

            /*
             * When we retrieve a file and part of it has already been
             * retrieved, tell the remote ftp server where to put the server
             * marker. No need to resend the data we already have.
             */
            if ((seek > 0) && (type == DATA_READ))
            {
#if SIZEOF_OFF_T == 4
               if (command(control_fd, "REST %ld", (pri_off_t)seek) != SUCCESS)
#else
               if (command(control_fd, "REST %lld", (pri_off_t)seek) != SUCCESS)
#endif
               {
                  (void)close(sock_fd);
                  return(INCORRECT);
               }
               if ((reply = get_reply(ERROR_SIGN, 0, __LINE__)) < 0)
               {
                  if (timeout_flag == OFF)
                  {
                     trans_log(WARN_SIGN, __FILE__, __LINE__, "ftp_data", msg_str,
                               _("Failed to get proper reply for REST command (%d)."),
                               reply);
                  }
                  else
                  {
                     (void)close(sock_fd);
                     return(INCORRECT);
                  }
               }
               else
               {
                  if (reply != 350)
                  {
                     trans_log(WARN_SIGN, __FILE__, __LINE__, "ftp_data", msg_str,
                               _("Failed to get proper reply for REST command (%d)."),
                               reply);
                  }
               }
            }

            if (command(control_fd, "%s %s", cmd, filename) != SUCCESS)
            {
               (void)close(sock_fd);
               return(INCORRECT);
            }
            if ((reply = get_reply(ERROR_SIGN, 0, __LINE__)) < 0)
            {
               if (timeout_flag == OFF)
               {
                  trans_log(WARN_SIGN, __FILE__, __LINE__, "ftp_data", msg_str,
                            _("Failed to get proper reply (%d) for command: %s %s"),
                            reply, cmd, filename);
               }
               (void)close(sock_fd);
               return(INCORRECT);
            }
         } while ((ret = check_data_socket(reply, sock_fd, &retries,
                                           filename, create_dir,
                                           dir_mode, created_path)) == 1);

         if (ret < 0)
         {
            if (reply < 0)
            {
               return(reply);
            }
            else
            {
               return(-reply);
            }
         }

         /*
          * Juck! Here follows some ugly code.
          * Experience has shown that on very rare occasions the
          * accept() call blocks. This is normal behaviour of the accept()
          * system call. Could make sock_fd non-blocking, but then
          * the next time we use the socket we will get an error. This is
          * not what we want. When we have a bad connection it can take quit
          * some time before we get a respond.
          */
         if (signal(SIGALRM, sig_handler) == SIG_ERR)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                      _("Failed to set signal handler : %s"), strerror(errno));
            (void)close(sock_fd);
            return(INCORRECT);
         }
         if (sigsetjmp(env_alrm, 1) != 0)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                      _("accept() timeout (%lds)"), transfer_timeout);
            timeout_flag = ON;
            (void)close(sock_fd);
            return(INCORRECT);
         }
         (void)alarm(transfer_timeout);
         new_sock_fd = accept(sock_fd, NULL, NULL);
         tmp_errno = errno;
         (void)alarm(0);

         if (new_sock_fd < 0)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                      _("accept() error : %s"), strerror(tmp_errno));
            (void)close(sock_fd);
            return(INCORRECT);
         }
         if (close(sock_fd) == -1)
         {
            trans_log(DEBUG_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                      _("close() error : %s"), strerror(errno));
         }
      } /* ACTIVE_MODE */

#ifdef FTX
      l.l_onoff = 1; l.l_linger = 240;
      if (setsockopt(new_sock_fd, SOL_SOCKET, SO_LINGER,
                     (char *)&l, sizeof(struct linger)) < 0)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                   _("setsockopt() error : %s"), strerror(errno));
         (void)close(new_sock_fd);
         return(INCORRECT);
      }
#endif

#if defined (_WITH_TOS) && defined (IP_TOS) && defined (IPTOS_THROUGHPUT)
      tos = IPTOS_THROUGHPUT;
      if (setsockopt(new_sock_fd, IPPROTO_IP, IP_TOS, (char *)&tos,
                     sizeof(tos)) < 0)
      {
         trans_log(WARN_SIGN, __FILE__, __LINE__, "ftp_data", NULL,
                   _("setsockopt() IP_TOS error : %s"), strerror(errno));
      }
#endif

      data_fd = new_sock_fd;
   }

   return(SUCCESS);
}


#ifdef WITH_SSL
/*########################### ftp_auth_data() ###########################*/
int
ftp_auth_data(void)
{
   if (ssl_con != NULL)
   {
      return(encrypt_data_connection(data_fd));
   }
   return(SUCCESS);
}
#endif


/*+++++++++++++++++++++++++ check_data_socket() +++++++++++++++++++++++++*/
static int
check_data_socket(int  reply,
                  int  sock_fd,
                  int  *retries,
                  char *filename,
                  int  create_dir,
                  char *dir_mode,
                  char *created_path)
{
   /*
    * The replies for STOR and APPE should be the same.
    */
   if ((reply != 150) && (reply != 125) && (reply != 120) &&
       (reply != 250) && (reply != 200))
   {
      if (close(sock_fd) == -1)
      {
         trans_log(DEBUG_SIGN, __FILE__, __LINE__, "check_data_socket", NULL,
                   _("close() error : %s"), strerror(errno));
      }

      if (((reply == 553) && (lposi(&msg_str[3], "(Overwrite)", 11) != NULL)) ||
          ((reply == 550) &&
           (lposi(&msg_str[3], "Overwrite permission denied", 27) != NULL)))
      {
         if (*retries < MAX_DATA_CONNECT_RETRIES)
         {
            if (ftp_dele(filename) == SUCCESS)
            {  
               (*retries)++;
               return(1);
            }
         }
         return(-2);
      }

      /*
       * If we do not get a data connection lets try it again.
       * It could be that the address is already in use.
       */
      if ((reply == 425) && (*retries < MAX_DATA_CONNECT_RETRIES))
      {
         (*retries)++;
         (void)my_usleep(10000L);

         return(1);
      }
      else if (((reply == 550) || (reply == 553)) && (create_dir == YES) &&
               (*retries < MAX_DATA_CONNECT_RETRIES))
           {
              char *ptr,
                   to_dir[MAX_PATH_LENGTH];

              (void)strcpy(to_dir, filename);
              ptr = to_dir + strlen(to_dir) - 1;
              while ((*ptr == '/') && (ptr > to_dir))
              {
                 ptr--;
              }
              while ((*ptr != '/') && (ptr > to_dir))
              {
                 ptr--;
              }
              if ((*ptr == '/') && (to_dir != ptr))
              {
                 *ptr = '\0';
                 *(ptr + 1) = '\0';
                 if (ftp_pwd() == SUCCESS)
                 {
                    char current_dir[MAX_PATH_LENGTH];

                    (void)strcpy(current_dir, msg_str);

                    /* The following will actually create the directory. */
                    if (ftp_cd(to_dir, YES, dir_mode, created_path) == SUCCESS)
                    {
                       if (ftp_cd(current_dir, NO, "", NULL) == SUCCESS)
                       {
                          (*retries)++;
                          return(1);
                       }
                       else
                       {
                          return(-3);
                       }
                    }
                    else
                    {
                       return(-3);
                    }
                 }
                 else
                 {
                    return(-3);
                 }
              }
              else
              {
                 return(-3);
              }
           }

      return(-1);
   }

   return(0);
}


#ifdef WITH_SSL
/*++++++++++++++++++++++ encrypt_data_connection() ++++++++++++++++++++++*/
static int
encrypt_data_connection(int fd)
{
   int reply;

   ssl_data = (SSL *)SSL_new(ssl_ctx);
   SSL_set_connect_state(ssl_data);
   SSL_set_fd(ssl_data, fd);
   SSL_copy_session_id(ssl_data, ssl_con);
   if ((reply = SSL_connect(ssl_data)) <= 0)
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
      X509 *x509_ssl_con,
           *x509_ssl_data;

      /* Get server certifcates for ctrl and data connection. */
      x509_ssl_con = SSL_get_peer_certificate(ssl_con);
      x509_ssl_data = SSL_get_peer_certificate(ssl_data);

      /* Now check if we did get a server certificate and */
      /* if it matches with that from ctrl connection.    */
      if ((x509_ssl_con != NULL) && (x509_ssl_data == NULL))
      {
         (void)my_strncpy(msg_str,
                          _("Server did not present a certificate for data connection."),
                          MAX_RET_MSG_LENGTH);
         SSL_free(ssl_data);
         ssl_data = NULL;
         (void)close(fd);
         reply = INCORRECT;
      }
      else if ((x509_ssl_con == NULL) &&
               ((x509_ssl_data == NULL) || (x509_ssl_data != NULL)))
           {
              (void)snprintf(msg_str, MAX_RET_MSG_LENGTH,
                             _("Failed to compare server certificates for control and data connection (%d)."),
                             reply);
              SSL_free(ssl_data);
              ssl_data = NULL;
              (void)close(fd);
              reply = INCORRECT;
           }
           else
           {
              if (X509_cmp(x509_ssl_con, x509_ssl_data))
              {
                 SSL_free(ssl_data);
                 ssl_data = NULL;
                 (void)close(fd);
                 reply = INCORRECT;
              }
              else
              {
                 reply = SUCCESS;
              }
           }
      X509_free(x509_ssl_con);
      X509_free(x509_ssl_data);
   }

   /* The error or encryption info is in msg_str. */
   return(reply);
}
#endif /* WITH_SSL */


#ifdef _BLOCK_MODE
/*############################## ftp_open() #############################*/
int
ftp_open(char *filename, int seek)
{
   int reply;

   if (seek != 0)
   {
      reply = command(control_fd, "REST %d", seek);
      if ((reply != SUCCESS) ||
          ((reply = get_reply(ERROR_SIGN, 350, __LINE__)) < 0))
      {
         return(INCORRECT);
      }

      if (reply != 350)
      {
         return(reply);
      }
   }

   reply = command(control_fd, "STOR %s", filename);
   if ((reply != SUCCESS) ||
       ((reply = get_reply(ERROR_SIGN, 125, __LINE__)) < 0))
   {
      return(INCORRECT);
   }

   if ((reply != 125) && (reply != 150) && (reply != 120) &&
       (reply != 250) && (reply != 200))
   {
      return(INCORRECT);
   }

   return(SUCCESS);
}
#endif /* _BLOCK_MODE */


/*########################### ftp_close_data() ##########################*/
int
ftp_close_data(void)
{
   fcd.data_port = 0;
   if (simulation_mode == YES)
   {
      if (close(data_fd) == -1)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_close_data", NULL,
                   _("close() error : %s"), strerror(errno));
         return(INCORRECT);
      }
      data_fd = -1;
   }
   else
   {
#ifdef WITH_SSL
      if (ssl_data != NULL)
      {
         if (timeout_flag != CON_RESET)
         {
            if (SSL_shutdown(ssl_data) == 0)
            {
               (void)SSL_shutdown(ssl_data);
            }
         }
         SSL_free(ssl_data);
         ssl_data = NULL;
      }
#endif
#ifdef _WITH_SHUTDOWN
      if (shutdown(data_fd, 1) < 0)
      {
         trans_log(DEBUG_SIGN, __FILE__, __LINE__, "ftp_close_data", NULL,
                   _("shutdown() error : %s"), strerror(errno));
      }
#endif

      if (close(data_fd) == -1)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_close_data", NULL,
                   _("close() error : %s"), strerror(errno));
         return(INCORRECT);
      }
      data_fd = -1;

      if (timeout_flag == OFF)
      {
         int reply;

         long tmp_ftp_timeout = transfer_timeout;

         /*
          * Since there are so many timeouts on slow lines when closing
          * the data connection lets double the timeout and see if this
          * helps.
          */
         transfer_timeout += transfer_timeout;
         if ((reply = get_reply(ERROR_SIGN, 0, __LINE__)) < 0)
         {
            transfer_timeout = tmp_ftp_timeout;
            return(INCORRECT);
         }
         transfer_timeout = tmp_ftp_timeout;
         if ((reply != 226) && (reply != 250))
         {
            return(reply);
         }
      }
   }

   return(SUCCESS);
}


/*########################### ftp_data_port() ###########################*/
int
ftp_data_port(void)
{
   return((int)fcd.data_port);
}


/*############################## ftp_write() ############################*/
int
ftp_write(char *block, char *buffer, int size)
{
   char   *ptr = block;
   int    status;
   fd_set wset;

   /* Initialise descriptor set. */
   FD_ZERO(&wset);
   FD_SET(data_fd, &wset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = transfer_timeout;

   /* Wait for message x seconds and then continue. */
   status = select(data_fd + 1, NULL, &wset, NULL, &timeout);

   if (status == 0)
   {
      /* Timeout has arrived. */
      timeout_flag = ON;
      return(INCORRECT);
   }
   else if (FD_ISSET(data_fd, &wset))
        {
           /*
            * When buffer is not NULL we want to sent the
            * data in ASCII-mode.
            */
           if (buffer != NULL)
           {
              register int i,
                           count = 1;

              for (i = 0; i < size; i++)
              {
                 if (*ptr == '\n')
                 {
                    if (((i > 0) && (*(ptr - 1) == '\r')) ||
                        ((i == 0) && (buffer[0] == '\r')))
                    {
                       buffer[count++] = *ptr;
                    }
                    else
                    {
                       buffer[count++] = '\r';
                       buffer[count++] = '\n';
                    }
                 }
                 else
                 {
                    buffer[count++] = *ptr;
                 }
                 ptr++;
              }
              if (i > 0)
              {
                 buffer[0] = *(ptr - 1);
                 size = count - 1;
              }
              else
              {
                 size = count;
              }
              ptr = buffer + 1;
           }

#ifdef WITH_SSL
           if (ssl_data == NULL)
           {
#endif
              if ((status = write(data_fd, ptr, size)) != size)
              {
                 if ((errno == ECONNRESET) || (errno == EBADF))
                 {
                    timeout_flag = CON_RESET;
                 }
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_write", NULL,
                           _("write() error (%d) : %s"),
                           status, strerror(errno));
                 return(errno);
              }
#ifdef WITH_SSL
           }
           else
           {
              if (ssl_write(ssl_data, ptr, size) != size)
              {
                 return(INCORRECT);
              }
           }
#endif
#ifdef WITH_TRACE
           trace_log(NULL, 0, BIN_W_TRACE, ptr, size, NULL);
#endif
        }
   else if (status < 0)
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_write", NULL,
                     _("select() error : %s"), strerror(errno));
           return(INCORRECT);
        }
        else
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_write", NULL,
                     _("Unknown condition."));
           return(INCORRECT);
        }
   
   return(SUCCESS);
}


#ifdef WITH_SENDFILE
/*############################ ftp_sendfile() ###########################*/
int
ftp_sendfile(int source_fd, off_t *offset, int size)
{
   int    nleft,
          nwritten;
   fd_set wset;

   nleft = size;
   size = 0;
   FD_ZERO(&wset);
   while (nleft > 0)
   {
      FD_SET(data_fd, &wset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = transfer_timeout;

      nwritten = select(data_fd + 1, NULL, &wset, NULL, &timeout);

      if (FD_ISSET(data_fd, &wset))
      {
         if ((nwritten = sendfile(data_fd, source_fd, offset, nleft)) > 0)
         {
            nleft -= nwritten;
            size += nwritten;
         }
         else if (nwritten == 0)
              {
                 nleft = 0;
              }
              else
              {
                 if (errno == ECONNRESET)
                 {
                    timeout_flag = CON_RESET;
                 }
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_sendfile", NULL,
                           _("sendfile() error (%d) : %s"),
                           nwritten, strerror(errno));
                 return(-errno);
              }
      }
      else if (nwritten == 0)
           {
              /* Timeout has arrived. */
              timeout_flag = ON;
              return(INCORRECT);
           }
           else
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_sendfile", NULL,
                        _("select() error : %s"), strerror(errno));
              return(INCORRECT);
           }
   }
   
   return(size);
}
#endif /* WITH_SENDFILE */


#ifdef _BLOCK_MODE
/*########################### ftp_block_write() #########################*/
int
ftp_block_write(char *block, unsigned short size, char descriptor)
{
   int    status;
   fd_set wset;

   /* Initialise descriptor set. */
   FD_ZERO(&wset);
   FD_SET(data_fd, &wset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = transfer_timeout;

   /* Wait for message x seconds and then continue. */
   status = select(data_fd + 1, NULL, &wset, NULL, &timeout);

   if (status == 0)
   {
      /* Timeout has arrived. */
      timeout_flag = ON;
      return(INCORRECT);
   }
   else if (FD_ISSET(data_fd, &wset))
        {
           /* Write descriptor of block header. */
           if (write(data_fd, &descriptor, 1) != 1)
           {
              if ((errno == ECONNRESET) || (errno == EBADF))
              {
                 timeout_flag = CON_RESET;
              }
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_block_write", NULL,
                        _("Failed to write() descriptor of block header : %s"),
                        strerror(errno));
              return(INCORRECT);
           }

           /* Write byte counter of block header. */
           if (write(data_fd, &size, 2) != 2)
           {
              if ((errno == ECONNRESET) || (errno == EBADF))
              {
                 timeout_flag = CON_RESET;
              }
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_block_write", NULL,
                        _("Failed to write() byte counter of block header : %s"),
                        strerror(errno));
              return(INCORRECT);
           }

           /* Now write the data. */
           if (write(data_fd, block, size) != size)
           {
              if ((errno == ECONNRESET) || (errno == EBADF))
              {
                 timeout_flag = CON_RESET;
              }
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_block_write", NULL,
                        _("write() error : %s"), strerror(errno));
              return(INCORRECT);
           }
        }
        else if (status < 0)
             {
                trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_block_write", NULL,
                          _("select() error : %s"), strerror(errno));
                return(INCORRECT);
             }
             else
             {
                trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_block_write", NULL,
                          _("Unknown condition."));
                return(INCORRECT);
             }
   
   return(SUCCESS);
}


/*############################## ftp_mode() #############################*/
int
ftp_mode(char mode)
{
   int reply;

   if ((reply = command(control_fd, "MODE %c", mode)) == SUCCESS)
   {
      if ((reply = get_reply(ERROR_SIGN, 200, __LINE__)) != INCORRECT)
      {
         if (reply == 200)
         {
            reply = SUCCESS;
         }
      }
   }

   return(reply);
}
#endif /* _BLOCK_MODE */


/*############################## ftp_read() #############################*/
int
ftp_read(char *block, int blocksize)
{
   int bytes_read;

#ifdef WITH_SSL
   if ((ssl_data != NULL) && (SSL_pending(ssl_data)))
   {
      if ((bytes_read = SSL_read(ssl_data, block, blocksize)) == INCORRECT)
      {
         ssl_error_msg("SSL_read", ssl_con, NULL, bytes_read, msg_str);
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_read", msg_str,
                   _("SSL_read() error"));

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
      FD_SET(data_fd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = transfer_timeout;

      /* Wait for message x seconds and then continue. */
      status = select(data_fd + 1, &rset, NULL, NULL, &timeout);

      if (status == 0)
      {
         /* Timeout has arrived. */
         timeout_flag = ON;
         return(INCORRECT);
      }
      else if (FD_ISSET(data_fd, &rset))
           {
#ifdef WITH_SSL
              if (ssl_data == NULL)
              {
#endif
                 if ((bytes_read = read(data_fd, block, blocksize)) == -1)
                 {
                    if (errno == ECONNRESET)
                    {
                       timeout_flag = CON_RESET;
                    }
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_read", NULL,
                              _("read() error : %s"), strerror(errno));
                    return(INCORRECT);
                 }
#ifdef WITH_SSL
              }
              else
              {
                 if ((bytes_read = SSL_read(ssl_data, block,
                                            blocksize)) == INCORRECT)
                 {
                    ssl_error_msg("SSL_read", ssl_con, NULL,
                                  bytes_read, msg_str);
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_read", msg_str,
                              _("SSL_read() error %d"), status);
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
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_read", NULL,
                        _("select() error : %s"), strerror(errno));
              return(INCORRECT);
           }
           else
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "ftp_read", NULL,
                        _("Unknown condition."));
              return(INCORRECT);
           }
   }

   return(bytes_read);
}


/*############################## ftp_quit() #############################*/
int
ftp_quit(void)
{
   if (timeout_flag != CON_RESET)
   {
      (void)command(control_fd, "QUIT");
   }
   if (simulation_mode == YES)
   {
      if (data_fd != -1)
      {
         if (close(data_fd) == -1)
         {
            trans_log(DEBUG_SIGN, __FILE__, __LINE__, "ftp_quit", NULL,
                      _("close() error : %s"), strerror(errno));
         }
      }
   }
   else
   {
      if (data_fd != -1)
      {
#ifdef WITH_SSL
         if (ssl_data != NULL)
         {
            if (timeout_flag != CON_RESET)
            {
               if (SSL_shutdown(ssl_data) == 0)
               {
                  (void)SSL_shutdown(ssl_data);
               }
            }
            SSL_free(ssl_data);
            ssl_data = NULL;
         }
#endif
#ifdef _WITH_SHUTDOWN
         if (timeout_flag == OFF)
         {
            if (shutdown(data_fd, 1) < 0)
            {
               trans_log(DEBUG_SIGN, __FILE__, __LINE__, "ftp_quit", NULL,
                         _("shutdown() error : %s"), strerror(errno));
            }
         }
#endif /* _WITH_SHUTDOWN */
         if (close(data_fd) == -1)
         {
            trans_log(DEBUG_SIGN, __FILE__, __LINE__, "ftp_quit", NULL,
                      _("close() error : %s"), strerror(errno));
         }
      }

      /*
       * If timeout_flag is ON, lets NOT check the reply from
       * the QUIT command. Else we are again waiting 'transfer_timeout'
       * seconds for the reply!
       */
      if (timeout_flag == OFF)
      {
         int reply;

         if ((reply = get_reply(INFO_SIGN, 0, __LINE__)) < 0)
         {
            (void)close(control_fd);
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
         if ((reply != 221) && (reply != 421))
         {
            (void)close(control_fd);
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
            return(reply);
         }
#ifdef _WITH_SHUTDOWN
         if (shutdown(control_fd, 1) < 0)
         {
            trans_log(DEBUG_SIGN, __FILE__, __LINE__, "ftp_quit", NULL,
                      _("shutdown() error : %s"), strerror(errno));
         }
#endif /* _WITH_SHUTDOWN */
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

   if (close(control_fd) == -1)
   {
      trans_log(DEBUG_SIGN, __FILE__, __LINE__, "ftp_quit", NULL,
                _("close() error : %s"), strerror(errno));
   }

   return(SUCCESS);
}


/*############################ ftp_get_reply() ##########################*/
int
ftp_get_reply(void)
{
   int  tmp_timeout_flag,
        reply;
   long tmp_transfer_timeout = 0L;

   if (transfer_timeout > 20L)
   {
      tmp_transfer_timeout = transfer_timeout;
      transfer_timeout = 20L;
   }
   if (timeout_flag == ON)
   {
      tmp_timeout_flag = ON;
   }
   else
   {
      tmp_timeout_flag = OFF;
   }
   reply = get_reply(ERROR_SIGN, 0, __LINE__);
   if ((timeout_flag == ON) && (tmp_timeout_flag == OFF))
   {
      timeout_flag = ON;
   }
   if (tmp_transfer_timeout > 0L)
   {
      transfer_timeout = tmp_transfer_timeout;
   }

   return(reply);
}


/*++++++++++++++++++++++++++++++ get_reply() ++++++++++++++++++++++++++++*/
static int
get_reply(char *sign, int reply, int line)
{
   if (simulation_mode == YES)
   {
      return(reply);
   }

   for (;;)
   {
      if (read_msg(sign, line) == INCORRECT)
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


/*++++++++++++++++++++++++++++ get_stat_reply() +++++++++++++++++++++++++*/
static int
#ifdef WITH_SSL
get_stat_reply(int reply, SSL *ssl, int type, ...)
#else
get_stat_reply(int reply, int type, ...)
#endif
{
   int     bytes_buffered = 0,
           bytes_read = 0,
           i,
           status;
   char    ***buffer,
           *ptr,
           *line,
           tmp_buffer[MAX_RET_MSG_LENGTH];
   fd_set  rset;
   va_list ap;

   va_start(ap, type);
   if (type & BUFFERED_LIST)
   {
      buffer = va_arg(ap, char ***);
      line = NULL;
   }
   else
   {
      buffer = NULL;
      line = va_arg(ap, char *);
   }
   va_end(ap);

   if (simulation_mode == YES)
   {
      return(reply);
   }
   msg_str[0] = '\0';

   FD_ZERO(&rset);
   for (;;)
   {
      /* Initialise descriptor set. */
      FD_SET(control_fd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = transfer_timeout;

      /* Wait for message x seconds and then continue. */
      status = select(control_fd + 1, &rset, NULL, NULL, &timeout);

      if (status == 0)
      {
         /* Timeout has arrived. */
         timeout_flag = ON;
         return(INCORRECT);
      }
      else if (FD_ISSET(control_fd, &rset))
           {
#ifdef WITH_SSL
              if (ssl == NULL)
              {
#endif
                 if ((bytes_read = read(control_fd, tmp_buffer,
                                        MAX_RET_MSG_LENGTH)) < 1)
                 {
                    if (bytes_read == -1)
                    {
                       if (errno == ECONNRESET)
                       {
                          timeout_flag = CON_RESET;
                       }
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, "get_stat_reply", NULL,
                                 _("read() error (after reading %d bytes) : %s"),
                                 bytes_buffered, strerror(errno));
                    }
                    else
                    {
                       if (type & BUFFERED_LIST)
                       {
                          if (bytes_buffered > 0)
                          {
                             *(**buffer + bytes_buffered) = '\0';
                          }
                          bytes_read = bytes_buffered;
                       }
                    }
                    return(bytes_read);
                 }
#ifdef WITH_SSL
              }
              else
              {
                 if ((bytes_read = SSL_read(ssl, tmp_buffer,
                                            MAX_RET_MSG_LENGTH)) < 1)
                 {
                    if (bytes_read == -1)
                    {
                       ssl_error_msg("SSL_read", ssl_con, NULL,
                                     bytes_read, msg_str);
                       if (errno == ECONNRESET)
                       {
                          timeout_flag = CON_RESET;
                       }
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, "get_stat_reply", msg_str,
                                 _("SSL_read() error (after reading %d bytes) (%d)"),
                                 bytes_buffered, status);
                       bytes_buffered = INCORRECT;
                    }
                    else
                    {
                       if (type & BUFFERED_LIST)
                       {
                          if (bytes_buffered > 0)
                          {
                             *(**buffer + bytes_buffered) = '\0';
                          }
                       }
                    }
                    return(bytes_buffered);
                 }
              }
#endif
#ifdef WITH_TRACE
              trace_log(NULL, 0, LIST_R_TRACE, tmp_buffer, bytes_read, NULL);
#endif
              if ((msg_str[0] == '\0') &&
                  (isdigit((int)tmp_buffer[0]) != 0) &&
                  (isdigit((int)tmp_buffer[1]) != 0) &&
                  (isdigit((int)tmp_buffer[2]) != 0) &&
                  (tmp_buffer[3] == '-'))
              {
                 msg_str[0] = tmp_buffer[0];
                 msg_str[1] = tmp_buffer[1];
                 msg_str[2] = tmp_buffer[2];
                 msg_str[3] = tmp_buffer[3];
                 ptr = &tmp_buffer[4];
                 i = 4;

                 do
                 {
                    if ((tmp_buffer[i] == '\n') &&
                        (tmp_buffer[i - 1] == '\r'))
                    {
                       msg_str[i - 1] = '\0';
                       bytes_read -= (i + 1);
                       if (bytes_read > 0)
                       {
                          (void)memmove(tmp_buffer, &tmp_buffer[i + 1],
                                        bytes_read);
                       }
                       break;
                    }
                    msg_str[i] = *ptr;
                    ptr++; i++;
                 } while (i < bytes_read);
              }

              if (bytes_read > 0)
              {
                 if (type & BUFFERED_LIST)
                 {
                    if (**buffer == NULL)
                    {
                       if ((**buffer = malloc(bytes_read + 1)) == NULL)
                       {
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, "get_stat_reply", NULL,
                                    _("malloc() error : %s"), strerror(errno));
                          return(INCORRECT);
                       }
                    }
                    else
                    {
                       char *tmp_buffer;

                       if ((tmp_buffer = realloc(**buffer,
                                                 bytes_buffered + bytes_read + 1)) == NULL)
                       {
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, "get_stat_reply", NULL,
                                    _("realloc() error (after reading %d bytes) : %s"),
                                    bytes_buffered, strerror(errno));
                          free(**buffer);
                          **buffer = NULL;
                          return(INCORRECT);
                       }
                       **buffer = tmp_buffer;
                    }
                    (void)memcpy(**buffer + bytes_buffered, tmp_buffer, bytes_read);
                 }
                 else
                 {
                    (void)memcpy(line + bytes_buffered, tmp_buffer, bytes_read);
                 }
                 bytes_buffered += bytes_read;

                 /* See if we have reached end of reply. */
                 i = 3;
                 if (type & BUFFERED_LIST)
                 {
                    ptr = **buffer + bytes_buffered - 3;
                 }
                 else
                 {
                    ptr = line + bytes_buffered - 3;
                 }
                 while ((i < bytes_buffered) && (*ptr != '\n'))
                 {
                    ptr--; i++;
                 }
                 if ((*ptr == '\n') && (*(ptr - 1) == '\r') &&
                     (isdigit((int)(*(ptr + 1))) != 0) &&
                     (isdigit((int)(*(ptr + 2))) != 0) &&
                     (isdigit((int)(*(ptr + 3))) != 0) &&
                     ((*(ptr + 4)) != '-'))
                 {
                    /* Cut away data we do not need. */
                    *(ptr - 1) = '\0';

                    return(bytes_buffered - (i + 1));
                 }
              }
           }
      else if (status < 0)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "get_stat_reply", NULL,
                        _("select() error : %s"), strerror(errno));
              return(INCORRECT);
           }
           else
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "get_stat_reply", NULL,
                        _("Unknown condition."));
              return(INCORRECT);
           }
   } /* for (;;) */

   return(bytes_buffered);
}


/*+++++++++++++++++++++++++++ get_mlst_reply() ++++++++++++++++++++++++++*/
static int
get_mlst_reply(char *sign, int reply, time_t *file_mtime, int line)
{
   char *ptr;

   if (simulation_mode == YES)
   {
      return(reply);
   }

   for (;;)
   {
      if (read_msg(sign, line) == INCORRECT)
      {
         return(INCORRECT);
      }

      ptr = msg_str;
      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }

      /* modify=YYYYMMDDHHMMSS[.sss]; */
      if (((*ptr == 'M') || (*ptr == 'm')) &&
          ((*(ptr + 1) == 'O')  || (*(ptr + 1) == 'o')) &&
          ((*(ptr + 2) == 'D')  || (*(ptr + 2) == 'd')) &&
          ((*(ptr + 3) == 'I')  || (*(ptr + 3) == 'i')) &&
          ((*(ptr + 4) == 'F')  || (*(ptr + 4) == 'f')) &&
          ((*(ptr + 5) == 'Y')  || (*(ptr + 5) == 'y')) &&
          (*(ptr + 6) == '=') &&
          (isdigit((int)(*(ptr + 7)))) &&
          (isdigit((int)(*(ptr + 8)))) &&
          (isdigit((int)(*(ptr + 9)))) &&
          (isdigit((int)(*(ptr + 10)))) &&
          (isdigit((int)(*(ptr + 11)))) &&
          (isdigit((int)(*(ptr + 12)))) &&
          (isdigit((int)(*(ptr + 13)))) &&
          (isdigit((int)(*(ptr + 14)))) &&
          (isdigit((int)(*(ptr + 15)))) &&
          (isdigit((int)(*(ptr + 16)))) &&
          (isdigit((int)(*(ptr + 17)))) &&
          (isdigit((int)(*(ptr + 18)))) &&
          (isdigit((int)(*(ptr + 19)))) &&
          (isdigit((int)(*(ptr + 20)))))
      {
         struct tm bd_time;

         bd_time.tm_isdst = 0;
         bd_time.tm_year  = (((*(ptr + 7) - '0') * 1000) +
                            ((*(ptr + 8) - '0') * 100) +
                            ((*(ptr + 9) - '0') * 10) +
                            (*(ptr + 10) - '0')) - 1900;
         bd_time.tm_mon   = ((*(ptr + 11) - '0') * 10) +
                            *(ptr + 12) - '0' - 1;
         bd_time.tm_mday  = ((*(ptr + 13) - '0') * 10) +
                            *(ptr + 14) - '0';
         bd_time.tm_hour  = ((*(ptr + 15) - '0') * 10) +
                            *(ptr + 16) - '0';
         bd_time.tm_min   = ((*(ptr + 17) - '0') * 10) +
                            *(ptr + 18) - '0';
         bd_time.tm_sec   = ((*(ptr + 19) - '0') * 10) +
                            *(ptr + 20) - '0';
         *file_mtime = mktime(&bd_time);
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


/*+++++++++++++++++++++++++++ read_data_line() ++++++++++++++++++++++++++*/
static int
#ifdef WITH_SSL
read_data_line(int read_fd, SSL *ssl, char *line)
#else
read_data_line(int read_fd, char *line)
#endif
{
   int    bytes_buffered = 0,
          bytes_read = 0,
          status;
   char   *read_ptr = line;
   fd_set rset;

   FD_ZERO(&rset);
   for (;;)
   {
      if (bytes_read <= 0)
      {
         /* Initialise descriptor set. */
         FD_SET(read_fd, &rset);
         timeout.tv_usec = 0L;
         timeout.tv_sec = transfer_timeout;

         /* Wait for message x seconds and then continue. */
         status = select(read_fd + 1, &rset, NULL, NULL, &timeout);

         if (status == 0)
         {
            /* Timeout has arrived. */
            timeout_flag = ON;
            return(INCORRECT);
         }
         else if (FD_ISSET(read_fd, &rset))
              {
#ifdef WITH_SSL
                 if (ssl == NULL)
                 {
#endif
                    if ((bytes_read = read(read_fd, &line[bytes_buffered],
                                           (MAX_RET_MSG_LENGTH - bytes_buffered))) < 1)
                    {
                       if (bytes_read == 0)
                       {
                          /*
                           * Due to security reasons some systems do not
                           * return any data here. So lets not count this
                           * as a remote hangup.
                           */
                          line[bytes_buffered] = '\0';
                       }
                       else
                       {
                          if (errno == ECONNRESET)
                          {
                             timeout_flag = CON_RESET;
                          }
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_data_line", NULL,
                                    _("read() error (after reading %d bytes) : %s"),
                                    bytes_buffered, strerror(errno));
                       }
                       return(bytes_read);
                    }
#ifdef WITH_SSL
                 }
                 else
                 {
                    if ((bytes_read = SSL_read(ssl, &line[bytes_buffered],
                                               (MAX_RET_MSG_LENGTH - bytes_buffered))) < 1)
                    {
                       if (bytes_read == 0)
                       {
                          /*
                           * Due to security reasons some systems do not
                           * return any data here. So lets not count this
                           * as a remote hangup.
                           */
                          line[bytes_buffered] = '\0';
                       }
                       else
                       {
                          ssl_error_msg("SSL_read", ssl_con, NULL,
                                        bytes_read, msg_str);
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_data_line", msg_str,
                                    _("SSL_read() error (after reading %d bytes) (%d)"),
                                    bytes_buffered, status);
                       }
                       return(bytes_read);
                    }
                 }
#endif
#ifdef WITH_TRACE
                 trace_log(NULL, 0, BIN_R_TRACE,
                           &line[bytes_buffered], bytes_read, NULL);
#endif
                 read_ptr = &line[bytes_buffered];
                 bytes_buffered += bytes_read;
              }
         else if (status < 0)
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_data_line", NULL,
                           _("select() error : %s"), strerror(errno));
                 return(INCORRECT);
              }
              else
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_data_line", NULL,
                           _("Unknown condition."));
                 return(INCORRECT);
              }
      }

      /* Evaluate what we have read. */
      do
      {
         if ((*read_ptr == '\n') && (*(read_ptr - 1) == '\r'))
         {
            *(read_ptr - 1) = '\0';
            return(bytes_buffered);
         }
         read_ptr++;
         bytes_read--;
      } while (bytes_read > 0);
   } /* for (;;) */
}


/*+++++++++++++++++++++++++ read_data_to_buffer() +++++++++++++++++++++++*/
static int
#ifdef WITH_SSL
read_data_to_buffer(int read_fd, SSL *ssl, char ***buffer)
#else
read_data_to_buffer(int read_fd, char ***buffer)
#endif
{
   int    bytes_buffered = 0,
          bytes_read = 0,
          status;
   fd_set rset;
   char   tmp_buffer[MAX_RET_MSG_LENGTH];

   FD_ZERO(&rset);
   for (;;)
   {
      /* Initialise descriptor set. */
      FD_SET(read_fd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = transfer_timeout;

      /* Wait for message x seconds and then continue. */
      status = select(read_fd + 1, &rset, NULL, NULL, &timeout);

      if (status == 0)
      {
         /* Timeout has arrived. */
         timeout_flag = ON;
         return(INCORRECT);
      }
      else if (FD_ISSET(read_fd, &rset))
           {
#ifdef WITH_SSL
              if (ssl == NULL)
              {
#endif
                 if ((bytes_read = read(read_fd, tmp_buffer,
                                        MAX_RET_MSG_LENGTH)) < 1)
                 {
                    if (bytes_read == -1)
                    {
                       if (errno == ECONNRESET)
                       {
                          timeout_flag = CON_RESET;
                       }
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_data_to_buffer", NULL,
                                 _("read() error (after reading %d bytes) : %s"),
                                 bytes_buffered, strerror(errno));
                    }
                    else
                    {
                       if (bytes_buffered > 0)
                       {
                          *(**buffer + bytes_buffered) = '\0';
                       }
                       bytes_read = bytes_buffered;
                    }
                    return(bytes_read);
                 }
#ifdef WITH_SSL
              }
              else
              {
                 if ((bytes_read = SSL_read(ssl, tmp_buffer,
                                            MAX_RET_MSG_LENGTH)) < 1)
                 {
                    if (bytes_read == -1)
                    {
                       ssl_error_msg("SSL_read", ssl_con, NULL,
                                     bytes_read, msg_str);
                       if (errno == ECONNRESET)
                       {
                          timeout_flag = CON_RESET;
                       }
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_data_to_buffer", msg_str,
                                 _("SSL_read() error (after reading %d bytes) (%d)"),
                                 bytes_buffered, status);
                       bytes_buffered = INCORRECT;
                    }
                    else
                    {
                       if (bytes_buffered > 0)
                       {
                          *(**buffer + bytes_buffered) = '\0';
                       }
                    }
                    return(bytes_buffered);
                 }
              }
#endif
#ifdef WITH_TRACE
              trace_log(NULL, 0, LIST_R_TRACE, tmp_buffer, bytes_read, NULL);
#endif
              if (**buffer == NULL)
              {
                 if ((**buffer = malloc(bytes_read + 1)) == NULL)
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_data_to_buffer", NULL,
                              _("malloc() error : %s"), strerror(errno));
                    return(INCORRECT);
                 }
              }
              else
              {
                 char *tmp_buffer;

                 if ((tmp_buffer = realloc(**buffer,
                                           bytes_buffered + bytes_read + 1)) == NULL)
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_data_to_buffer", NULL,
                              _("realloc() error (after reading %d bytes) : %s"),
                              bytes_buffered, strerror(errno));
                    free(**buffer);
                    **buffer = NULL;
                    return(INCORRECT);
                 }
                 **buffer = tmp_buffer;
              }
              (void)memcpy(**buffer + bytes_buffered, tmp_buffer, bytes_read);
              bytes_buffered += bytes_read;
           }
      else if (status < 0)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_data_to_buffer", NULL,
                        _("select() error : %s"), strerror(errno));
              return(INCORRECT);
           }
           else
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_data_to_buffer", NULL,
                        _("Unknown condition."));
              return(INCORRECT);
           }
   } /* for (;;) */
}


/*----------------------------- read_msg() ------------------------------*/
static int
read_msg(char *sign, int line)
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

         /* Initialise descriptor set. */
         FD_SET(control_fd, &rset);
         timeout.tv_usec = 0L;
         timeout.tv_sec = transfer_timeout;

         /* Wait for message x seconds and then continue. */
         status = select(control_fd + 1, &rset, NULL, NULL, &timeout);

         if (status == 0)
         {
            /* Timeout has arrived. */
            timeout_flag = ON;
            bytes_read = 0;
            return(INCORRECT);
         }
         else if (FD_ISSET(control_fd, &rset))
              {
#ifdef WITH_SSL
                 if (ssl_con == NULL)
                 {
#endif
                    if ((bytes_read = read(control_fd, &msg_str[bytes_buffered],
                                           (MAX_RET_MSG_LENGTH - bytes_buffered))) < 1)
                    {
                       if (bytes_read == 0)
                       {
                          trans_log(sign,  __FILE__, __LINE__, "read_msg", NULL,
                                    _("Remote hang up. [%d]"), line);
                          timeout_flag = NEITHER;
                       }
                       else
                       {
                          if (errno == ECONNRESET)
                          {
                             timeout_flag = CON_RESET;
                          }
                          trans_log(sign, __FILE__, __LINE__, "read_msg", NULL,
                                    _("read() error (after reading %d bytes) [%d] : %s"),
                                    bytes_buffered, line, strerror(errno));
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
                          trans_log(sign, __FILE__, __LINE__, "read_msg", NULL,
                                    _("Remote hang up. [%d]"), line);
                          timeout_flag = NEITHER;
                       }
                       else
                       {
                          int ssl_ret;

                          ssl_error_msg("SSL_read", ssl_con, &ssl_ret,
                                        bytes_read, msg_str);
                          trans_log(sign, __FILE__, __LINE__, "read_msg", msg_str,
                                    _("SSL_read() error (after reading %d bytes) (%d) [%d]"),
                                    bytes_buffered, status, line);

                          /*
                           * Some FTP servers drop back to clear text.
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
#ifdef WITH_TRACE
                 trace_log(NULL, 0, R_TRACE,
                           &msg_str[bytes_buffered], bytes_read, NULL);
#endif
                 read_ptr = &msg_str[bytes_buffered];
                 bytes_buffered += bytes_read;
              }
         else if (status < 0)
              {
                 trans_log(sign, __FILE__, __LINE__, "read_msg", NULL,
                           _("select() error [%d] : %s"),
                           line, strerror(errno));
                 return(INCORRECT);
              }
              else
              {
                 trans_log(sign, __FILE__, __LINE__, "read_msg", NULL,
                           _("Unknown condition. [%d]"), line);
                 return(INCORRECT);
              }
      }

      /* Evaluate what we have read. */
      do
      {
         if (*read_ptr == '\n')
         {
            if (*(read_ptr - 1) == '\r')
            {
               *(read_ptr - 1) = '\0';
            }
            else
            {
               *read_ptr = '\0';
            }
            bytes_read--;
            return(bytes_buffered);
         }
         read_ptr++;
         bytes_read--;
      } while (bytes_read > 0);
   } /* for (;;) */
}


/*+++++++++++++++++++++++ get_extended_number() +++++++++++++++++++++++++*/
static int
get_extended_number(char *ptr)
{
   if (*ptr == '(')
   {
      char delimiter;

      ptr++;
      delimiter = *ptr;

      /* Protocol Version. */
      if (*(ptr + 1) != delimiter)
      {
         ptr++;
#if defined (HAVE_GETADDRINFO) && defined (HAVE_GAI_STRERROR)
         if ((*ptr != '1') && (*ptr != '2') && (*(ptr + 1) != delimiter))
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "get_extended_number", NULL,
                      _("Can only handle IPv4 or IPv6."));
            return(INCORRECT);
         }
#else
         if ((*ptr != '1') && (*(ptr + 1) != delimiter))
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "get_extended_number", NULL,
                      _("Can only handle IPv4."));
            return(INCORRECT);
         }
#endif
         else
         {
            ptr++;
         }
      }
      else
      {
         ptr++;
      }

      /* Address. */
      if (*ptr != delimiter)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "get_extended_number", NULL,
                   _("Remote host reuturns a network address, which is not allowed according to RFC 2428."));
         return(INCORRECT);
      }
      else
      {
         ptr++;
      }

      /* Port. */
      if (*ptr == delimiter)
      {
         char *ptr_start;

         ptr++;
         ptr_start = ptr;
         while ((*ptr != delimiter) && (*ptr != '\0'))
         {
            ptr++;
         }
         if ((*ptr == delimiter) && (ptr != ptr_start))
         {
            int number;

            errno = 0;
            number = (int)strtol(ptr_start, (char **)NULL, 10);
            if (errno == 0)
            {
               return(number);
            }
         }
      }
      else
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "get_extended_number", NULL,
                   _("Could not locate a port number."));
         return(INCORRECT);
      }
   }
   return(INCORRECT);
}


/*++++++++++++++++++++++++++++ get_number() +++++++++++++++++++++++++++++*/
static int
get_number(char **ptr, char end_char)
{
   int length = 0,
       number = INCORRECT;

   (*ptr)++;
   do
   {
      if (isdigit((int)(**ptr)))
      {
         length++; (*ptr)++;
      }
      else
      {
         return(INCORRECT);
      }
   } while ((**ptr != end_char) && (length < 4));
   if ((length > 0) && (length < 4) && (**ptr == end_char))
   {
      number = *(*ptr - 1) - '0';
      if (length == 2)
      {
         number += (*(*ptr - 2) - '0') * 10;
      }
      else if (length == 3)
           {
              number += (*(*ptr - 3) - '0') * 100 + (*(*ptr - 2) - '0') * 10;
           }
   }

   return(number);
}


/*++++++++++++++++++++++++++++++ sig_handler() ++++++++++++++++++++++++++*/
static void
sig_handler(int signo)
{
   siglongjmp(env_alrm, 1);
}
