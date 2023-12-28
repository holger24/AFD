/*
 *  smtpcmd.c - Part of AFD, an automatic file distribution program.
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
 **   smtpcmd - commands to send data via SMTP
 **
 ** SYNOPSIS
 **   int smtp_connect(char *hostname, int port, int sockbuf_size)
 **   int smtp_auth(char *user, char *passwd)
 **   int smtp_helo(char *host_name)
 **   int smtp_ehlo(char *host_name)
 **   int smtp_smarttls(int strict, int legacy_renegotiation)
 **   int smtp_user(char *user)
 **   int smtp_rcpt(char *recipient)
 **   int smtp_open(void)
 **   int smtp_write(char *block, char *buffer, int size)
 **   int smtp_write_iso8859(char *block, char *buffer, int size)
 **   int smtp_noop(void)
 **   int smtp_close(void)
 **   int smtp_quit(void)
 **
 ** DESCRIPTION
 **   smtpcmd provides a set of commands to communicate with an SMTP
 **   server via BSD sockets.
 **   The procedure to send files to another SMTP server is as
 **   follows:
 **            smtp_connect()
 **               |
 **               V
 **            smtp_helo()
 **               |
 **               V
 **               +<----------------------------+
 **               |                             |
 **               V                             |
 **            smtp_user()                      |
 **               |                             |
 **               V                             |
 **            smtp_rcpt()<---------+           |
 **               |                 |           |
 **               V                 |           |
 **               +-----------------+           |
 **               |                             |
 **               V                             |
 **            smtp_open()                      |
 **               |                             |
 **               V                             |
 **   smtp_write()/smtp_write_iso8859()<----+   |
 **               |                         |   |
 **               V                         |   |
 **        +-------------+   NO             |   |
 **        | File done ? |------------------+   |
 **        +-------------+                      |
 **               |                             |
 **               V                             |
 **            smtp_close()                     |
 **               |                             |
 **               V                             |
 **        +--------------+      YES            |
 **        | More files ? |---------------------+
 **        +--------------+
 **               |
 **               V
 **            smtp_quit()
 **
 **
 ** RETURN VALUES
 **   Returns SUCCESS when successful. When an error has occurred
 **   it will either return INCORRECT or the three digit SMTP reply
 **   code when the reply of the server does not conform to the
 **   one expected. The complete reply string of the SMTP server
 **   is returned to 'msg_str'. 'timeout_flag' is just a flag to
 **   indicate that the 'transfer_timeout' time has been reached.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.12.1996 H.Kiehl Created
 **   23.07.1999 H.Kiehl Added smtp_write_iso8859()
 **   08.07.2000 H.Kiehl Cleaned up log output to reduce code size.
 **   05.08.2000 H.Kiehl Buffering read() in read_msg() to reduce number
 **                      of system calls.
 **   21.05.2001 H.Kiehl Failed to fclose() connection when an error
 **                      occured in smtp_quit().
 **   25.12.2003 H.Kiehl Added traceing.
 **   10.01.2004 H.Kiehl Remove function check_reply().
 **   27.09.2006 H.Kiehl Function smtp_write() now handles the case
 **                      correctly when a line starts with a leading
 **                      dot '.'.
 **   09.11.2008 H.Kiehl Implemented EHLO command.
 **   21.02.2009 H.Kiehl Added smtp_noop().
 **   18.08.2012 H.Kiehl Use getaddrinfo() instead of gethostname() to
 **                      support IPv6.
 **   11.09.2014 H.Kiehl Added simulation mode.
 **   10.10.2014 H.Kiehl Added SMARTTLS support.
 **   22.08.2017 H.Kiehl Added function smtp_write_subject().
 **
 */
DESCR__E_M3


#include <stdio.h>            /* fprintf(), fdopen(), fclose()           */
#include <string.h>           /* memset(), memcpy(), strcpy()            */
#include <stdlib.h>           /* atoi(), exit(), malloc(), free()        */
#include <ctype.h>            /* isdigit()                               */
#include <sys/types.h>        /* fd_set                                  */
#include <sys/time.h>         /* struct timeval                          */
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <sys/socket.h>       /* socket(), shutdown(), bind(),           */
                              /* setsockopt()                            */
#include <netinet/in.h>       /* struct in_addr, sockaddr_in, htons()    */
#include <netdb.h>            /* struct hostent, gethostbyname()         */
#include <arpa/inet.h>        /* inet_addr()                             */
#include <unistd.h>           /* select(), write(), read(), close()      */
#include <errno.h>
#include "fddefs.h"           /* trans_log(), C_TRACE, BIN_W_TRACE, etc  */
#include "smtpdefs.h"
#include "commondefs.h"

#ifdef WITH_SSL
SSL                                    *ssl_con = NULL;
#endif

/* External global variables. */
extern int                             simulation_mode,
                                       timeout_flag;
extern unsigned int                    special_flag;
extern char                            msg_str[],
                                       tr_hostname[];
#ifdef LINUX
extern char                            *h_errlist[]; /* for gethostbyname() */
extern int                             h_nerr;       /* for gethostbyname() */
#endif
extern long                            transfer_timeout;

/* Local global variables. */
static int                             smtp_fd;
#ifdef WITH_SSL
static char                            connected_hostname[MAX_REAL_HOSTNAME_LENGTH];
#endif
static struct timeval                  timeout;
static struct smtp_server_capabilities ssc;

/* Local function prototypes. */
static int                             get_ehlo_reply(int, int),
                                       get_reply(int, int),
                                       read_msg(int);


/*########################## smtp_connect() #############################*/
int
smtp_connect(char *hostname, int port, int sockbuf_size)
{
   if (simulation_mode == YES)
   {
      if ((smtp_fd = open("/dev/null", O_RDWR)) == -1)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "smtp_connect", "Simulated smtp_connect()",
                   _("Failed to open() /dev/null : %s"), strerror(errno));
         return(INCORRECT);
      }
      else
      {
#ifdef WITH_TRACE
         int length;

         length = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                           _("Simulated SMTP connect to %s (port=%d)"),
                           hostname, port);
         if (length > MAX_RET_MSG_LENGTH)
         {
            length = MAX_RET_MSG_LENGTH;
         }
         trace_log(NULL, 0, C_TRACE, msg_str, length, NULL);
#else
         (void)snprintf(msg_str, MAX_RET_MSG_LENGTH,
                        _("Simulated SMTP connect to %s (port=%d)"),
                        hostname, port);
#endif
      }
   }
   else
   {
      int             reply;
#ifdef WITH_TRACE
      int             length;
#endif
#if defined (HAVE_GETADDRINFO) && defined (HAVE_GAI_STRERROR)
      char            str_port[MAX_INT_LENGTH];
      struct addrinfo hints,
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
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "smtp_connect", NULL,
                   _("Failed to getaddrinfo() %s : %s"),
                   hostname, gai_strerror(reply));
         freeaddrinfo(result);
         return(INCORRECT);
      }

      /*
       * getaddrinfo() returns a list of address structures.
       * Try each address until we successfully connect(). If socket()
       * (or connect()) fails, we (close the socket and) try the next
       * address.
       */
      for (rp = result; rp != NULL; rp = rp->ai_next)
      {
         if ((smtp_fd = socket(rp->ai_family, rp->ai_socktype,
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

         if (sockbuf_size > 0)
         {
            if (setsockopt(smtp_fd, SOL_SOCKET, SO_SNDBUF,
                           (char *)&sockbuf_size, sizeof(sockbuf_size)) < 0)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, "smtp_connect", NULL,
                         _("setsockopt() error : %s"), strerror(errno));
            }
         }
# ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
         if (timeout_flag != OFF)
         {
            reply = 1;
            if (setsockopt(smtp_fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&reply,
                           sizeof(reply)) < 0)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, "smtp_connect", NULL,
                         _("setsockopt() SO_KEEPALIVE error : %s"),
                         strerror(errno));
            }
#  ifdef TCP_KEEPALIVE
            reply = timeout_flag;
            if (setsockopt(smtp_fd, IPPROTO_IP, TCP_KEEPALIVE, (char *)&reply,
                           sizeof(reply)) < 0)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, "smtp_connect", NULL,
                         _("setsockopt() TCP_KEEPALIVE error : %s"),
                         strerror(errno));
            }
#  endif
            timeout_flag = OFF;
         }
# endif

         reply = connect_with_timeout(smtp_fd, rp->ai_addr, rp->ai_addrlen);
         if (reply == INCORRECT)
         {
            if (errno)
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
            (void)close(smtp_fd);
            continue;
         }
         else if (reply == PERMANENT_INCORRECT)
              {
                 (void)close(smtp_fd);
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, "smtp_connect", NULL,
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
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "smtp_connect", NULL,
                      _("Failed to connect() to %s : %s"),
                      hostname, strerror(errno));
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "smtp_connect", NULL,
                      _("Failed to connect() to %s"), hostname);
         }
         (void)close(smtp_fd);
         freeaddrinfo(result);
         return(INCORRECT);
      }

      freeaddrinfo(result);
#else
      struct sockaddr_in      sin;
      register struct hostent *p_host = NULL;

      (void)memset((struct sockaddr *) &sin, 0, sizeof(sin));
      if ((sin.sin_addr.s_addr = inet_addr(hostname)) == -1)
      {
         if ((p_host = gethostbyname(hostname)) == NULL)
         {
# if !defined (_HPUX) && !defined (_SCO)
            if (h_errno != 0)
            {
#  ifdef LINUX
               if ((h_errno > 0) && (h_errno < h_nerr))
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "smtp_connect", NULL,
                            _("Failed to gethostbyname() %s : %s"),
                            hostname, h_errlist[h_errno]);
               }
               else
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "smtp_connect", NULL,
                            _("Failed to gethostbyname() %s (h_errno = %d) : %s"),
                            hostname, h_errno, strerror(errno));
               }
#  else
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "smtp_connect", NULL,
                         _("Failed to gethostbyname() %s (h_errno = %d) : %s"),
                         hostname, h_errno, strerror(errno));
#  endif
            }
            else
            {
# endif /* !_HPUX && !_SCO */
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "smtp_connect", NULL,
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

      if ((smtp_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "smtp_connect", NULL,
                   _("socket() error : %s"), strerror(errno));
         return(INCORRECT);
      }
      sin.sin_family = AF_INET;
      sin.sin_port = htons((u_short)port);

      if (sockbuf_size > 0)
      {
         if (setsockopt(smtp_fd, SOL_SOCKET, SO_SNDBUF,
                        (char *)&sockbuf_size, sizeof(sockbuf_size)) < 0)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, "smtp_connect", NULL,
                      _("setsockopt() error : %s"), strerror(errno));
         }
      }

# ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
      if (timeout_flag != OFF)
      {
         reply = 1;
         if (setsockopt(smtp_fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&reply,
                        sizeof(reply)) < 0)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, "smtp_connect", NULL,
                      _("setsockopt() SO_KEEPALIVE error : %s"), strerror(errno));
         }
#  ifdef TCP_KEEPALIVE
         reply = timeout_flag;
         if (setsockopt(smtp_fd, IPPROTO_IP, TCP_KEEPALIVE, (char *)&reply,
                        sizeof(reply)) < 0)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, "smtp_connect", NULL,
                      _("setsockopt() TCP_KEEPALIVE error : %s"), strerror(errno));
         }
#  endif
         timeout_flag = OFF;
      }
# endif

      if (connect_with_timeout(smtp_fd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
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
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "smtp_connect", NULL,
                      _("Failed to connect() to %s : %s"),
                      hostname, strerror(errno));
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "smtp_connect", NULL,
                      _("Failed to connect() to %s"), hostname);
         }
         (void)close(smtp_fd);
         return(INCORRECT);
      }
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

      if ((reply = get_reply(220, __LINE__)) < 0)
      {
         (void)close(smtp_fd);
         return(INCORRECT);
      }
      if (reply != 220)
      {
         (void)close(smtp_fd);
         return(reply);
      }
   }
#ifdef WITH_SSL
   (void)memcpy(connected_hostname, hostname, MAX_REAL_HOSTNAME_LENGTH);
#endif

   return(SUCCESS);
}


/*############################# smtp_helo() #############################*/
int
smtp_helo(char *host_name)
{
   int reply;

   if ((reply = command(smtp_fd, "HELO %s", host_name)) == SUCCESS)
   {
      if ((reply = get_reply(250, __LINE__)) != INCORRECT)
      {
         if (reply == 250)
         {
            reply = SUCCESS;
         }
      }
   }

   return(reply);
}


/*############################# smtp_ehlo() #############################*/
int
smtp_ehlo(char *host_name)
{
   int reply;

   if ((reply = command(smtp_fd, "EHLO %s", host_name)) == SUCCESS)
   {
      if ((reply = get_ehlo_reply(250, __LINE__)) != INCORRECT)
      {
         if (reply == 250)
         {
            reply = SUCCESS;
         }
      }
   }

   return(reply);
}


#ifdef WITH_SSL
/*########################### smtp_smarttls() ###########################*/
int
smtp_smarttls(int strict, int legacy_renegotiation)
{
   int reply;

   if (ssc.starttls == YES)
   {
      if ((reply = command(smtp_fd, "STARTTLS")) == SUCCESS)
      {
         if ((reply = get_reply(220, __LINE__)) != INCORRECT)
         {
            if (reply == 220)
            {
               if (simulation_mode == YES)
               {
                  reply = SUCCESS;
                  ssc.ssl_enabled = YES;
               }
               else
               {
                  if ((reply = ssl_connect(smtp_fd, connected_hostname,
                                           "smtp_smarttls",
                                           strict,
                                           legacy_renegotiation)) == SUCCESS)
                  {
                     ssc.ssl_enabled = YES;
                  }
               }
            }
         }
      }
   }
   else
   {
      reply = NEITHER;
   }

   return(reply);
}
#endif /* WITH_SSL */


/*############################# smtp_auth() #############################*/
int
smtp_auth(unsigned char auth_type, char *user, char *passwd)
{
   int  reply;
   char auth_type_str[6];

   if (auth_type == SMTP_AUTH_LOGIN)
   {
      if (ssc.auth_login == YES)
      {
         auth_type_str[0] = 'L';
         auth_type_str[1] = 'O';
         auth_type_str[2] = 'G';
         auth_type_str[3] = 'I';
         auth_type_str[4] = 'N';
      }
      else
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "smtp_auth", NULL,
                   _("SMTP server does NOT support AUTH LOGIN."));
         return(INCORRECT);
      }
   }
   else if (auth_type == SMTP_AUTH_PLAIN)
        {
           if (ssc.auth_plain == YES)
           {
              auth_type_str[0] = 'P';
              auth_type_str[1] = 'L';
              auth_type_str[2] = 'A';
              auth_type_str[3] = 'I';
              auth_type_str[4] = 'N';
           }
           else
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "smtp_auth", NULL,
                        _("SMTP server does NOT support AUTH PLAIN."));
              return(INCORRECT);
           }
        }
        else
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, "smtp_auth", NULL,
                     _("Unknown SMTP AUTH method not supported by AFD."));
           return(INCORRECT);
        }

   auth_type_str[5] = '\0';

   if ((reply = command(smtp_fd, "AUTH %s", auth_type_str)) == SUCCESS)
   {
      if ((reply = get_reply(334, __LINE__)) != INCORRECT)
      {
         if (reply == 334)
         {
            int  length;
            char base_64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/",
                 *dst_ptr,
                 *src_ptr,
                 userpasswd[MAX_USER_NAME_LENGTH + MAX_USER_NAME_LENGTH],
                 userpasswd_b64[MAX_USER_NAME_LENGTH + MAX_USER_NAME_LENGTH];

            dst_ptr = userpasswd_b64;
            if (ssc.auth_login == YES)
            {
               length = strlen(user);
               src_ptr = user;
            }
            else
            {
               length = snprintf(userpasswd,
                                 MAX_USER_NAME_LENGTH + MAX_USER_NAME_LENGTH,
                                 "%s:%s", user, passwd);
               if (length > (MAX_USER_NAME_LENGTH + MAX_USER_NAME_LENGTH))
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "smtp_auth", NULL,
                            _("Buffer length to store user+passwd not long enough, needs %d bytes"),
                            length);
                  return(INCORRECT);
               }
               src_ptr = userpasswd;
            }
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
            *dst_ptr = '\0';

            if ((reply = command(smtp_fd, "%s", userpasswd_b64)) == SUCCESS)
            {
               if ((reply = get_reply(334, __LINE__)) != INCORRECT)
               {
                  if (reply == 334)
                  {
                     if (ssc.auth_login == YES)
                     {
                        dst_ptr = userpasswd_b64;
                        length = strlen(passwd);
                        src_ptr = passwd;
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
                        *dst_ptr = '\0';

                        if ((reply = command(smtp_fd, "%s", userpasswd_b64)) == SUCCESS)
                        {
                           if ((reply = get_reply(235, __LINE__)) != INCORRECT)
                           {
                              if (reply == 235)
                              {
                                 reply = SUCCESS;
                              }
                           }
                        }
                     }
                     else
                     {
                        reply = SUCCESS;
                     }
                  }
               }
            }
         }
      }
   }

   return(reply);
}


/*############################# smtp_user() #############################*/
int
smtp_user(char *user)
{
   int reply;

   if ((reply = command(smtp_fd, "MAIL FROM:%s", user)) == SUCCESS)
   {
      if ((reply = get_reply(250, __LINE__)) != INCORRECT)
      {
         if (reply == 250)
         {
            reply = SUCCESS;
         }
      }
   }

   return(reply);
}


/*############################# smtp_rcpt() #############################*/
int
smtp_rcpt(char *recipient)
{
   int reply;

   if ((reply = command(smtp_fd, "RCPT TO:%s", recipient)) == SUCCESS)
   {
      if ((reply = get_reply(250, __LINE__)) != INCORRECT)
      {
         if ((reply == 250) || (reply == 251))
         {
            reply = SUCCESS;
         }
      }
   }

   return(reply);
}


/*############################# smtp_noop() #############################*/
int
smtp_noop(void)
{
   int reply;

   if ((reply = command(smtp_fd, "NOOP")) == SUCCESS)
   {
      if ((reply = get_reply(250, __LINE__)) != INCORRECT)
      {
         if (reply == 250)
         {
            reply = SUCCESS;
         }
      }
   }

   return(reply);
}


/*############################# smtp_open() #############################*/
int
smtp_open(void)
{
   int reply;

   if ((reply = command(smtp_fd, "DATA")) == SUCCESS)
   {
      if ((reply = get_reply(354, __LINE__)) != INCORRECT)
      {
         if (reply == 354)
         {
            reply = SUCCESS;
         }
      }
   }

   return(reply);
}


/*######################## smtp_write_subject() #########################*/
int
smtp_write_subject(char *subject, size_t *length, char *charset)
{
   int  ret = 0;
   char *buffer;
#ifndef WITH_ASCII_ONLY_SUBJECT
   char *ptr = subject;

   while ((ret < *length) && (isascii(*ptr)))
   {
      ptr++; ret++;
   }
   if (ret == *length)
   {
#endif
      if ((buffer = malloc((9 + *length + 2 + 1))) == NULL)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "smtp_write_subject", NULL,
                   _("malloc() error : %s"), strerror(errno));
         return(INCORRECT);
      }

      /* Subject: */
      buffer[0] = 'S'; buffer[1] = 'u'; buffer[2] = 'b'; buffer[3] = 'j';
      buffer[4] = 'e'; buffer[5] = 'c'; buffer[6] = 't'; buffer[7] = ':';
      buffer[8] = ' ';

      (void)memcpy(&buffer[9], subject, *length);
      buffer[9 + *length] = '\r';
      buffer[9 + *length + 1] = '\n';

      (*length) += 9 + 2;
#ifndef WITH_ASCII_ONLY_SUBJECT
   }
   else
   {
      size_t charset_length;

      /*
       * Subject is not plain ASCII. So lets encode the subject to
       * base64.
       */
      if (charset == NULL)
      {
         charset_length = 5;
      }
      else
      {
         charset_length = strlen(charset);
      }
      if ((buffer = malloc((11 + charset_length + 3 + (2 * *length) + 4 + 1))) == NULL)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "smtp_write_subject", NULL,
                   _("malloc() error : %s"), strerror(errno));
         return(INCORRECT);
      }

      /* Subject: =? */
      buffer[0] = 'S'; buffer[1] = 'u'; buffer[2] = 'b'; buffer[3] = 'j';
      buffer[4] = 'e'; buffer[5] = 'c'; buffer[6] = 't'; buffer[7] = ':';
      buffer[8] = ' '; buffer[9] = '='; buffer[10] = '?';

      if (charset == NULL)
      {
         /* utf-8 */
         buffer[11] = 'u'; buffer[12] = 't'; buffer[13] = 'f';
         buffer[14] = '-'; buffer[15] = '8';
      }
      else
      {
         (void)memcpy(&buffer[11], charset, charset_length);
      }
      buffer[11 + charset_length] = '?';
      buffer[11 + charset_length + 1] = 'B';
      buffer[11 + charset_length + 2] = '?';

      ret = encode_base64((unsigned char *)subject, *length,
                          (unsigned char *)&buffer[11 + charset_length + 3],
                          NO);
      buffer[11 + charset_length + 3 + ret] = '?';
      buffer[11 + charset_length + 3 + ret + 1] = '=';
      buffer[11 + charset_length + 3 + ret + 2] = '\r';
      buffer[11 + charset_length + 3 + ret + 3] = '\n';

      *length = 11 + charset_length + 3 + ret + 4;
   }
#endif

   if (smtp_write(buffer, NULL, *length) < 0)
   {
      ret = INCORRECT;
   }
   else
   {
      ret = SUCCESS;
   }

   free(buffer);

   return(ret);
}


/*############################# smtp_write() ############################*/
/*                              ------------                             */
/* If buffer is not NULL, this function converts a '\n' only to a '\r'   */
/* '\n' and adds another dot '.' if a dot is at the beginning of the     */
/* line. Note that this works correctly the caller needs to allocate at  */
/* least twice the amount of memory as that specified in size. This      */
/* function does NOT check this. Also note that the first byte in buffer */
/* will always contain the last character of the previous block. So when */
/* function smtp_write() is used to send the mail body, buffer[0] must   */
/* be initialized by the caller to '\n' only for the first block send.   */
/*#######################################################################*/
int
smtp_write(char *block, char *buffer, size_t size)
{
   int    status;
   char   *ptr = block;
   fd_set wset;

   /* Initialise descriptor set. */
   FD_ZERO(&wset);
   FD_SET(smtp_fd, &wset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = transfer_timeout;

   /* Wait for message x seconds and then continue. */
   status = select(smtp_fd + 1, NULL, &wset, NULL, &timeout);

   if (status == 0)
   {
      /* Timeout has arrived. */
      timeout_flag = ON;
      return(INCORRECT);
   }
   else if (FD_ISSET(smtp_fd, &wset))
        {
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
                 else if ((*ptr == '.') &&
                          (((i > 0) && (*(ptr - 1) == '\n')) ||
                           ((i == 0) && (buffer[0] == '\n'))))
                      {
                         buffer[count++] = '.';
                         buffer[count++] = '.';
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
           if (ssl_con == NULL)
           {
#endif
              if ((status = write(smtp_fd, ptr, size)) != size)
              {
                 if ((errno == ECONNRESET) || (errno == EBADF))
                 {
                    timeout_flag = CON_RESET;
                 }
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, "smtp_write", NULL,
                           _("write() error after writting %d bytes : %s"),
                           status, strerror(errno));
                 return(INCORRECT);
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
   else if (status < 0)
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, "smtp_write", NULL,
                     _("select() error : %s"), strerror(errno));
           exit(INCORRECT);
        }
        else
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, "smtp_write", NULL,
                     _("Unknown condition."));
           exit(INCORRECT);
        }
   
   return(SUCCESS);
}


/*######################## smtp_write_iso8859() #########################*/
int
smtp_write_iso8859(char *block, char *buffer, int size)
{
   int           status;
   unsigned char *ptr = (unsigned char *)block;
   fd_set        wset;

   /* Initialise descriptor set. */
   FD_ZERO(&wset);
   FD_SET(smtp_fd, &wset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = transfer_timeout;

   /* Wait for message x seconds and then continue. */
   status = select(smtp_fd + 1, NULL, &wset, NULL, &timeout);

   if (status == 0)
   {
      /* Timeout has arrived. */
      timeout_flag = ON;
      return(INCORRECT);
   }
   else if (FD_ISSET(smtp_fd, &wset))
        {
           register int count = 1,
                        i;

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
              else if ((*ptr == '.') &&
                       (((i > 0) && (*(ptr - 1) == '\n')) ||
                        ((i == 0) && (buffer[0] == '\n'))))
                   {
                      buffer[count++] = '.';
                      buffer[count++] = '.';
                   }
                   else
                   {
                      switch (*ptr)
                      {
                         case 21 :
                            buffer[count] = (char)(167);
                            break;
                         case 129 : /* ue */
                            buffer[count] = (char)(252);
                            break;
                         case 130 :
                            buffer[count] = (char)(233);
                            break;
                         case 131 :
                            buffer[count] = (char)(226);
                            break;
                         case 132 : /* ae */
                            buffer[count] = (char)(228);
                            break;
                         case 140 :
                            buffer[count] = (char)(238);
                            break;
                         case 142 : /* AE */
                            buffer[count] = (char)(196);
                            break;
                         case 147 :
                            buffer[count] = (char)(244);
                            break;
                         case 148 : /* oe */
                            buffer[count] = (char)(246);
                            break;
                         case 153 : /* OE */
                            buffer[count] = (char)(214);
                            break;
                         case 154 : /* UE */
                            buffer[count] = (char)(220);
                            break;
                         case 160 :
                            buffer[count] = (char)(225);
                            break;
                         case 161 :
                            buffer[count] = (char)(237);
                            break;
                         case 163 :
                            buffer[count] = (char)(250);
                            break;
                         case 225 : /* ss */
                            buffer[count] = (char)(223);
                            break;
                         case 246 :
                            buffer[count] = (char)(247);
                            break;
                         case 248 :
                            buffer[count] = (char)(176);
                            break;
                         default :
                            buffer[count] = *ptr;
                            break;
                      }
                      count++;
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
           ptr = (unsigned char *)buffer + 1;

#ifdef WITH_SSL
           if (ssl_con == NULL)
           {
#endif
              if ((status = write(smtp_fd, ptr, size)) != size)
              {
                 if ((errno == ECONNRESET) || (errno == EBADF))
                 {
                    timeout_flag = CON_RESET;
                 }
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, "smtp_write_iso8859", NULL,
                           _("write() error after writting %d bytes : %s"),
                           status, strerror(errno));
                 return(INCORRECT);
              }
#ifdef WITH_SSL
           }
           else
           {
              if (ssl_write(ssl_con, (char *)ptr, size) != size)
              {
                 return(INCORRECT);
              }
           }
#endif
#ifdef WITH_TRACE
           trace_log(NULL, 0, BIN_W_TRACE, (char *)ptr, size, NULL);
#endif
        }
        else if (status < 0)
             {
                trans_log(ERROR_SIGN, __FILE__, __LINE__, "smtp_write_iso8859", NULL,
                          _("select() error : %s"), strerror(errno));
                return(INCORRECT);
             }
             else
             {
                trans_log(ERROR_SIGN, __FILE__, __LINE__, "smtp_write_iso8859", NULL,
                          _("Unknown condition."));
                exit(INCORRECT);
             }
   
   return(SUCCESS);
}


/*############################ smtp_close() #############################*/
int
smtp_close(void)
{
   int reply;

   if ((reply = command(smtp_fd, "\r\n.")) == SUCCESS)
   {
      if (timeout_flag == OFF)
      {
         if ((reply = get_reply(250, __LINE__)) != INCORRECT)
         {
            if (reply == 250)
            {
               reply = SUCCESS;
            }
         }
      }
   }

   return(reply);
}


/*############################# smtp_quit() #############################*/
int
smtp_quit(void)
{
   (void)command(smtp_fd, "QUIT");

   if ((timeout_flag != ON) && (timeout_flag != CON_RESET))
   {
      int reply;

      if ((reply = get_reply(221, __LINE__)) < 0)
      {
         (void)close(smtp_fd);
         return(INCORRECT);
      }
      if (reply != 221)
      {
         (void)close(smtp_fd);
         return(reply);
      }

#ifdef _WITH_SHUTDOWN
      if (simulation_mode != YES)
      {
         if (shutdown(smtp_fd, 1) < 0)
         {
            trans_log(DEBUG_SIGN, __FILE__, __LINE__, "smtp_quit", NULL,
                      _("shutdown() error : %s"), strerror(errno));
         }
      }
#endif
   }
#ifdef WITH_SSL
   if ((simulation_mode != YES) && (ssl_con != NULL))
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

   if (close(smtp_fd) == -1)
   {
      trans_log(DEBUG_SIGN, __FILE__, __LINE__, "smtp_quit", NULL,
                _("close() error : %s"), strerror(errno));
   }

   return(SUCCESS);
}


/*++++++++++++++++++++++++++++++ get_reply() ++++++++++++++++++++++++++++*/
static int
get_reply(int reply, int line)
{
   if (simulation_mode == YES)
   {
      return(reply);
   }

   for (;;)
   {
      if (read_msg(line) == INCORRECT)
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


/*+++++++++++++++++++++++++++ get_ehlo_reply() ++++++++++++++++++++++++++*/
static int
get_ehlo_reply(int reply, int line)
{
   ssc.auth_login = NO;
   ssc.auth_plain = NO;
   ssc.starttls = NO;
   ssc.ssl_enabled = NO;
   if (simulation_mode == YES)
   {
      return(reply);
   }
   for (;;)
   {
      if (read_msg(line) == INCORRECT)
      {
         return(INCORRECT);
      }

      if ((isdigit((int)msg_str[0]) != 0) && (isdigit((int)msg_str[1]) != 0) &&
          (isdigit((int)msg_str[2]) != 0) && (msg_str[3] != '-'))
      {
         break;
      }
           /* 250- */
      else if ((msg_str[0] == '2') && (msg_str[1] == '5') &&
               (msg_str[2] == '0') && (msg_str[3] == '-'))
           {
              /* AUTH */
              if (((msg_str[4] == 'A') || (msg_str[4] == 'a')) &&
                  ((msg_str[5] == 'U') || (msg_str[5] == 'u')) &&
                  ((msg_str[6] == 'T') || (msg_str[6] == 't')) &&
                  ((msg_str[7] == 'H') || (msg_str[7] == 'h')) &&
                  (msg_str[8] == ' '))
              {
                 char *ptr = &msg_str[9];

                 do
                 {
                    /* LOGIN */
                    if (((*ptr == 'L') || (*ptr == 'l')) &&
                        ((*(ptr + 1) == 'O') || (*(ptr + 1) == 'o')) &&
                        ((*(ptr + 2) == 'G') || (*(ptr + 2) == 'g')) &&
                        ((*(ptr + 3) == 'I') || (*(ptr + 3) == 'i')) &&
                        ((*(ptr + 4) == 'N') || (*(ptr + 4) == 'n')) &&
                        ((*(ptr + 5) == ' ') || (*(ptr + 5) == '\0')))
                    {
                       ptr += 5;
                       ssc.auth_login = YES;
                    }
                         /* PLAIN */
                    else if (((*ptr == 'P') || (*ptr == 'p')) &&
                             ((*(ptr + 1) == 'L') || (*(ptr + 1) == 'l')) &&
                             ((*(ptr + 2) == 'A') || (*(ptr + 2) == 'a')) &&
                             ((*(ptr + 3) == 'I') || (*(ptr + 3) == 'i')) &&
                             ((*(ptr + 4) == 'N') || (*(ptr + 4) == 'n')) &&
                             ((*(ptr + 5) == ' ') || (*(ptr + 5) == '\0')))
                         {
                            ptr += 5;
                            ssc.auth_plain = YES;
                         }
                         else
                         {
                            while ((*ptr != ' ') && (*ptr != '\0'))
                            {
                               ptr++;
                            }
                         }
                    if (*ptr == ' ')
                    {
                       do
                       {
                          ptr++;
                       } while (*ptr == ' ');
                    }
                 } while (*ptr != '\0');
              }
                    /* STARTTLS */
              else if (((msg_str[4] == 'S') || (msg_str[4] == 'a')) &&
                       ((msg_str[5] == 'T') || (msg_str[5] == 'u')) &&
                       ((msg_str[6] == 'A') || (msg_str[6] == 't')) &&
                       ((msg_str[7] == 'R') || (msg_str[7] == 'h')) &&
                       ((msg_str[8] == 'T') || (msg_str[8] == 'h')) &&
                       ((msg_str[9] == 'T') || (msg_str[9] == 'h')) &&
                       ((msg_str[10] == 'L') || (msg_str[10] == 'h')) &&
                       ((msg_str[11] == 'S') || (msg_str[11] == 'h')) &&
                       (msg_str[12] == '\0'))
                   {
                      ssc.starttls = YES;
                   }
           }
   }

   return(((msg_str[0] - '0') * 100) + ((msg_str[1] - '0') * 10) +
          (msg_str[2] - '0'));
}


/*----------------------------- read_msg() ------------------------------*/
static int
read_msg(int line)
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
         FD_SET(smtp_fd, &rset);
         timeout.tv_usec = 0L;
         timeout.tv_sec = transfer_timeout;

         /* Wait for message x seconds and then continue. */
         status = select(smtp_fd + 1, &rset, NULL, NULL, &timeout);

         if (status == 0)
         {
            /* Timeout has arrived. */
            timeout_flag = ON;
            bytes_read = 0;
            return(INCORRECT);
         }
         else if (FD_ISSET(smtp_fd, &rset))
              {
#ifdef WITH_SSL
                 if (ssl_con == NULL)
                 {
#endif
                    if ((bytes_read = read(smtp_fd, &msg_str[bytes_buffered],
                                           (MAX_RET_MSG_LENGTH - bytes_buffered))) < 1)
                    {
                       if (bytes_read == 0)
                       {
                          trans_log(ERROR_SIGN,  __FILE__, __LINE__, "read_msg", NULL,
                                    _("Remote hang up. [%d]"), line);
                          timeout_flag = NEITHER;
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
                          trans_log(ERROR_SIGN,  __FILE__, __LINE__, "read_msg", NULL,
                                    _("Remote hang up. [%d]"), line);
                          timeout_flag = NEITHER;
                       }
                       else
                       {
                          int ssl_ret;

                          ssl_error_msg("SSL_read", ssl_con, &ssl_ret,
                                        bytes_read, msg_str);
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_msg", msg_str,
                                    _("SSL_read() error (after reading %d bytes) (%d) [%d]"),
                                    bytes_buffered, status, line);

                          /*
                           * In case SMTP server has dropped back to clear text.
                           * Check if that is the case and try read
                           * the clear text responce.
                           */
                          if (ssl_ret == SSL_ERROR_SSL)
                          {
                             SSL_free(ssl_con);
                             ssl_con = NULL;
                             ssc.ssl_enabled = NO;
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
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_msg", NULL,
                           _("select() error [%d] : %s"),
                           line, strerror(errno));
                 exit(INCORRECT);
              }
              else
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_msg", NULL,
                           _("Unknown condition. [%d]"), line);
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
