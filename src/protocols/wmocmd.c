/*
 *  wmocmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2018 Deutscher Wetterdienst (DWD),
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
 **   wmocmd - commands to send files via TCP according WMO regulations
 **
 ** SYNOPSIS
 **   int wmo_connect(char *hostname, int port, int sndbuf_size)
 **   int wmo_write(char *block, char *buffer, int size)
 **   int wmo_check_reply(char *expect_string)
 **   int wmo_quit(void)
 **
 ** DESCRIPTION
 **   wmocmd provides a set of commands to communicate with a TCP
 **   server via BSD sockets according WMO regulations.
 **   Only three functions are necessary to do the communication:
 **      wmo_connect() - build a TCP connection to the WMO server
 **      wmo_write()   - write data to the socket
 **      wmo_quit()    - disconnect from the WMO server
 **   The function wmo_check_reply() is optional, it checks if the
 **   remote site has received the reply.
 **
 ** RETURN VALUES
 **   Returns SUCCESS when successful. When an error has occurred
 **   it will return INCORRECT. 'timeout_flag' is just a flag to
 **   indicate that the 'transfer_timeout' time has been reached.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   28.05.1998 H.Kiehl Created
 **   10.11.1998 H.Kiehl Added function wmp_check_reply().
 **   08.07.2000 H.Kiehl Cleaned up log output to reduce code size.
 **   18.08.2012 H.Kiehl Use getaddrinfo() instead of gethostname() to
 **                      support IPv6.
 **   11.09.2014 H.Kiehl Added simulation mode.
 **
 */
DESCR__E_M3


#include <stdio.h>
#include <string.h>           /* memset(), memcpy()                      */
#include <stdlib.h>           /* malloc(), free()                        */
#include <sys/types.h>        /* fd_set                                  */
#include <sys/time.h>         /* struct timeval                          */
#include <sys/socket.h>       /* socket(), shutdown(), bind(),           */
                              /* setsockopt()                            */
#include <netinet/in.h>       /* struct in_addr, sockaddr_in, htons()    */
#include <netdb.h>            /* struct hostent, gethostbyname()         */
#include <arpa/inet.h>        /* inet_addr()                             */
#include <unistd.h>           /* select(), write(), read(), close()      */
#include <fcntl.h>            /* O_NONBLOCK                              */
#include <errno.h>
#include "wmodefs.h"
#include "fddefs.h"           /* trans_log()                             */
#include "commondefs.h"


/* External global variables. */
extern int            simulation_mode,
                      timeout_flag;
extern unsigned int   special_flag;
#ifdef LINUX
extern char           *h_errlist[];  /* for gethostbyname()          */
extern int            h_nerr;        /* for gethostbyname()          */
#endif
extern long           transfer_timeout;
extern char           tr_hostname[];

/* Local global variables. */
static int            wmo_fd;
static struct timeval timeout;

#define MAX_CHARS_IN_LINE 45


/*########################## wmo_connect() ##############################*/
int
wmo_connect(char *hostname, int port, int sndbuf_size)
{
   if (simulation_mode == YES)
   {
      if ((wmo_fd = open("/dev/null", O_RDWR)) == -1)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_connect", "Simulated wmo_connect()",
                   _("Failed to open() /dev/null : %s"), strerror(errno));
         return(INCORRECT);
      }
#ifdef WITH_TRACE
      else
      {
         int  length;
         char line[MAX_RET_MSG_LENGTH];

         length = snprintf(line, MAX_RET_MSG_LENGTH, _("Simulated WMO connect to %s (port=%d)"), hostname, port);
         trace_log(NULL, 0, C_TRACE, line, length, NULL);
      }
#endif
   }
   else
   {
#ifdef WITH_TRACE
      int             length;
      char            line[MAX_RET_MSG_LENGTH];
#endif
#ifdef FTX
      struct linger   l;
#endif
      int             reply;
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
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_connect", NULL,
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
         if ((wmo_fd = socket(rp->ai_family, rp->ai_socktype,
                              rp->ai_protocol)) == -1)
         {
# ifdef WITH_TRACE
            length = snprintf(line, MAX_RET_MSG_LENGTH,
                              _("socket() error : %s"), strerror(errno));
            trace_log(NULL, 0, C_TRACE, line, length, NULL);
# endif
            continue;
         }

         if (sndbuf_size > 0)
         {
            if (setsockopt(wmo_fd, SOL_SOCKET, SO_SNDBUF,
                           (char *)&sndbuf_size, sizeof(sndbuf_size)) < 0)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, "wmo_connect", NULL,
                         _("setsockopt() error : %s"), strerror(errno));
            }
         }
# ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
         if (timeout_flag != OFF)
         {
            reply = 1;
            if (setsockopt(wmo_fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&reply,
                           sizeof(reply)) < 0)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, "wmo_connect", NULL,
                         _("setsockopt() SO_KEEPALIVE error : %s"),
                         strerror(errno));
            }
#  ifdef TCP_KEEPALIVE
            reply = timeout_flag;
            if (setsockopt(wmo_fd, IPPROTO_IP, TCP_KEEPALIVE, (char *)&reply,
                           sizeof(reply)) < 0)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, "wmo_connect", NULL,
                         _("setsockopt() TCP_KEEPALIVE error : %s"),
                         strerror(errno));
            }
#  endif
            timeout_flag = OFF;
      }
# endif

         reply = connect_with_timeout(wmo_fd, rp->ai_addr, rp->ai_addrlen);
         if (reply == INCORRECT)
         {
            if (errno != 0)
            {
# ifdef WITH_TRACE
               length = snprintf(line, MAX_RET_MSG_LENGTH,
                                 _("connect() error : %s"), strerror(errno));
               trace_log(NULL, 0, C_TRACE, line, length, NULL);
# endif
            }
            (void)close(wmo_fd);
            continue;
         }
         else if (reply == PERMANENT_INCORRECT)
              {
                 (void)close(wmo_fd);
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_connect", NULL,
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
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_connect", NULL,
                      _("Failed to connect() to %s : %s"),
                      hostname, strerror(errno));
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_connect", NULL,
                      _("Failed to connect() to %s"), hostname);
         }
         freeaddrinfo(result);
         return(INCORRECT);
      }

      freeaddrinfo(result);
#else
      int                     loop_counter = 0;
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
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_connect", NULL,
                            _("Failed to gethostbyname() %s : %s"),
                            hostname, h_errlist[h_errno]);
               }
               else
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_connect", NULL,
                            _("Failed to gethostbyname() %s (h_errno = %d) : %s"),
                            hostname, h_errno, strerror(errno));
               }
#  else
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_connect", NULL,
                         _("Failed to gethostbyname() %s (h_errno = %d) : %s"),
                         hostname, h_errno, strerror(errno));
#  endif
            }
            else
            {
# endif /* !_HPUX && !_SCO */
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_connect", NULL,
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

      if ((wmo_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_connect", NULL,
                   _("socket() error : %s"), strerror(errno));
         return(INCORRECT);
      }
      sin.sin_family = AF_INET;
      sin.sin_port = htons((u_short)port);

# ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
      if (timeout_flag != OFF)
      {
         int reply = 1;

         if (setsockopt(wmo_fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&reply,
                        sizeof(reply)) < 0)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, "wmo_connect", NULL,
                      _("setsockopt() SO_KEEPALIVE error : %s"), strerror(errno));
         }
#  ifdef TCP_KEEPALIVE
         reply = timeout_flag;
         if (setsockopt(wmo_fd, IPPROTO_IP, TCP_KEEPALIVE, (char *)&reply,
                        sizeof(reply)) < 0)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, "wmo_connect", NULL,
                      _("setsockopt() TCP_KEEPALIVE error : %s"), strerror(errno));
         }
#  endif
         timeout_flag = OFF;
      }
# endif

      while ((reply = connect_with_timeout(wmo_fd, (struct sockaddr *) &sin,
                                           sizeof(sin))) == INCORRECT)
      {
         loop_counter++;

# ifdef ETIMEDOUT
#  ifdef ECONNREFUSED
         if ((loop_counter <= 8) && (errno != ETIMEDOUT) && (errno != ECONNREFUSED))
#  else
         if ((loop_counter <= 8) && (errno != ETIMEDOUT))
#  endif
# else
#  ifdef ECONNREFUSED
         if ((loop_counter <= 8) && (errno != ECONNREFUSED))
#  else
         if (loop_counter <= 8)
#  endif
# endif
         {
            /*
             * Lets not give up to early. When we just have closed
             * the connection and immediatly retry, the other side
             * might not be as quick and still have the socket open.
             */
            (void)sleep(1);
         }
         else
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
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_connect", NULL,
                         _("Failed to connect() to %s, have tried %d times : %s"),
                         hostname, loop_counter, strerror(errno));
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_connect", NULL,
                         _("Failed to connect() to %s, have tried %d times"),
                         hostname, loop_counter);
            }
            (void)close(wmo_fd);
            wmo_fd = -1;
            return(INCORRECT);
         }
         if (close(wmo_fd) == -1)
         {
            trans_log(DEBUG_SIGN, __FILE__, __LINE__, "wmo_connect", NULL,
                      _("close() error : %s"), strerror(errno));
         }
         if ((wmo_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_connect", NULL,
                      _("socket() error : %s"), strerror(errno));
            (void)close(wmo_fd);
            return(INCORRECT);
         }
      }
      if (reply == PERMANENT_INCORRECT)
      {
         (void)close(wmo_fd);
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_connect", NULL,
                   _("Failed to connect() to %s"), hostname);
         wmo_fd = -1;
         return(INCORRECT);
      }
# ifdef WITH_TRACE
      if (loop_counter)
      {
         length = snprintf(line, MAX_RET_MSG_LENGTH
                           _("Connected to %s after %d tries"),
                           hostname, loop_counter);
      }
      else
      {
         length = snprintf(line, MAX_RET_MSG_LENGTH,
                           _("Connected to %s"), hostname);
      }
      trace_log(NULL, 0, C_TRACE, line, length, NULL);
# endif
#endif

#ifdef FTX
      l.l_onoff = 1; l.l_linger = 240;
      if (setsockopt(wmo_fd, SOL_SOCKET, SO_LINGER, (char *)&l,
                  sizeof(struct linger)) < 0)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_connect", NULL,
                   _("setsockopt() error : %s"), strerror(errno));
         return(INCORRECT);
      }
#endif
   }

   return(SUCCESS);
}


/*############################## wmo_write() ############################*/
int
wmo_write(char *block, int size)
{
   int    status;
   fd_set wset;

   /* Initialise descriptor set. */
   FD_ZERO(&wset);
   FD_SET(wmo_fd, &wset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = transfer_timeout;

   /* Wait for message x seconds and then continue. */
   status = select(wmo_fd + 1, NULL, &wset, NULL, &timeout);

   if (status == 0)
   {
      /* Timeout has arrived. */
      timeout_flag = ON;
      return(INCORRECT);
   }
   else if (FD_ISSET(wmo_fd, &wset))
        {
#ifdef _WITH_SEND
           if ((status = send(wmo_fd, block, size, 0)) != size)
           {
              if ((errno == ECONNRESET) || (errno == EBADF))
              {
                 timeout_flag = CON_RESET;
              }
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_write", NULL,
                        _("send() error (%d) : %s"), status, strerror(errno));
              return(errno);
           }
#else
           if ((status = write(wmo_fd, block, size)) != size)
           {
              if ((errno == ECONNRESET) || (errno == EBADF))
              {
                 timeout_flag = CON_RESET;
              }
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_write", NULL,
                        _("write() error (%d) : %s"), status, strerror(errno));
              return(errno);
           }
#endif
        }
        else if (status < 0)
             {
                trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_write", NULL,
                          _("select() error : %s"), strerror(errno));
                return(INCORRECT);
             }
             else
             {
                trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_write", NULL,
                          _("Unknown condition."));
                return(INCORRECT);
             }
   
   return(SUCCESS);
}


/*########################### wmo_check_reply() #########################*/
int
wmo_check_reply(void)
{
   int  n = 0;
   char buffer[10];

   if (simulation_mode == YES)
   {
      return(SUCCESS);
   }

   if ((n = readn(wmo_fd, buffer, 10, transfer_timeout)) == 10)
   {
      if ((buffer[0] == '0') && (buffer[1] == '0') &&
          (buffer[2] == '0') && (buffer[3] == '0') &&
          (buffer[4] == '0') && (buffer[5] == '0') &&
          (buffer[6] == '0') && (buffer[7] == '0'))
      {
         if ((buffer[8] == 'A') && (buffer[9] == 'K'))
         {
            return(SUCCESS);
         }
         else if ((buffer[8] == 'N') && (buffer[9] == 'A'))
              {
                 return(NEGATIV_ACKNOWLEDGE);
              }
      }

      trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_check_reply", NULL,
                _("Incorrect reply from remote site."));

      /* Show context of what has been returned. */
      {
         int  line_length = 0,
              no_read = 10;
         char *ptr = buffer,
              line[10 + 40 + 1];

         do
         {
            if (*ptr < ' ')
            {
               /* Yuck! Not printable. */
               line_length += snprintf(&line[line_length],
                                       10 + 40 - line_length,
#ifdef _SHOW_HEX
                                       "<%x>", (int)*ptr);
#else
                                       "<%d>", (int)*ptr);
#endif
            }
            else
            {
               line[line_length] = *ptr;
               line_length++;
            }

            ptr++; no_read--;
         } while ((no_read > 0) && (line_length < (10 + 40)));

         line[line_length] = '\0';
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_check_reply", NULL,
                   "%s", line);
      }
   }
   else if (n == -1) /* Read error. */
        {
           if (errno == ECONNRESET)
           {
              timeout_flag = CON_RESET;
           }
           trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_check_reply", NULL,
                     _("read() error : %s"), strerror(errno));
        }
   else if (n == -2) /* Timeout. */
        {
           timeout_flag = ON;
        }
   else if (n == -3) /* Select error. */
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_check_reply", NULL,
                     _("select() error : %s"), strerror(errno));
        }
        else
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_check_reply", NULL,
                     _("Remote site closed connection."));
        }

   return(INCORRECT);
}


/*############################## wmo_quit() #############################*/
void
wmo_quit(void)
{
   if (wmo_fd != -1)
   {
      if ((timeout_flag != ON) && (timeout_flag != CON_RESET) &&
          (simulation_mode != YES))
      {
         if (shutdown(wmo_fd, 1) < 0)
         {
            trans_log(DEBUG_SIGN, __FILE__, __LINE__, "wmo_quit", NULL,
                      _("shutdown() error : %s"), strerror(errno));
         }
         else
         {
            int    status;
            char   buffer[32];
            fd_set rset;

            /* Initialise descriptor set. */
            FD_ZERO(&rset);
            FD_SET(wmo_fd, &rset);
            timeout.tv_usec = 0L;
            timeout.tv_sec = transfer_timeout;

            /* Wait for message x seconds and then continue. */
            status = select(wmo_fd + 1, &rset, NULL, NULL, &timeout);

            if (status == 0)
            {
               /* Timeout has arrived. */
               timeout_flag = ON;
            }
            else if (FD_ISSET(wmo_fd, &rset))
                 {
                    if ((status = read(wmo_fd, buffer, 32)) < 0)
                    {
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_quit", NULL,
                                 _("read() error (%d) : %s"),
                                 status, strerror(errno));
                    }
                 }
            else if (status < 0)
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_quit", NULL,
                              _("select() error : %s"), strerror(errno));
                 }
                 else
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "wmo_quit", NULL,
                              _("Unknown condition."));
                 }
         }
      }
      if (close(wmo_fd) == -1)
      {
         trans_log(DEBUG_SIGN, __FILE__, __LINE__, "wmo_quit", NULL,
                   _("close() error : %s"), strerror(errno));
      }
      wmo_fd = -1;
   }

   return;
}
