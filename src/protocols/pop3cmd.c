/*
 *  pop3cmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2006 - 2025 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   pop3cmd - commands to send data via POP3
 **
 ** SYNOPSIS
 **   int  pop3_connect(char *hostname, int port)
 **   int  pop3_user(char *user)
 **   int  pop3_quit(void)
 **
 ** DESCRIPTION
 **   pop3cmd provides a set of commands to communicate with a POP3
 **   server via BSD sockets.
 **   The following functions are necessary to do the communication:
 **      pop3_connect()  - build a connection to a POP3 server
 **      pop3_user()     - sends the user name
 **      pop3_pass()     - sends the user password
 **      pop3_stat()     - ask for the number of messages and their size
 **      pop3_retrieve() - retrieve a message
 **      pop3_read()     - read message
 **      pop3_dele()     - delete a message
 **      pop3_quit()     - disconnect from the POP3 server
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
 **   15.06.2006 H.Kiehl Created
 **
 */
DESCR__E_M3


#include <stdio.h>
#include <string.h>           /* memset(), memcpy()                      */
#include <stdlib.h>           /* malloc(), free()                        */
#include <ctype.h>            /* isdigit()                               */
#include <sys/types.h>        /* fd_set                                  */
#include <sys/time.h>         /* struct timeval                          */
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>      /* socket(), shutdown(), bind(),           */
                              /* setsockopt()                            */
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>      /* struct in_addr, sockaddr_in, htons()    */
#endif
#ifdef HAVE_NETDB_H
# include <netdb.h>           /* struct hostent, gethostbyname()         */
#endif
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>       /* inet_addr()                             */
#endif
#ifdef WITH_SSL
# include <openssl/crypto.h>
# include <openssl/x509.h>
# include <openssl/ssl.h>
#endif
#include <unistd.h>           /* select(), write(), read(), close()      */
#include <fcntl.h>            /* O_NONBLOCK                              */
#include <errno.h>
#include "pop3defs.h"
#include "fddefs.h"           /* trans_log()                             */
#include "commondefs.h"

#ifdef WITH_SSL
SSL                   *ssl_con = NULL;
#endif

/* External global variables. */
extern int            timeout_flag;
#ifdef LINUX
extern char           *h_errlist[];  /* for gethostbyname()          */
extern int            h_nerr;        /* for gethostbyname()          */
#endif
extern long           transfer_timeout;
extern char           msg_str[],
                      tr_hostname[];

/* Local global variables. */
static int            pop3_fd,
                      rb_offset;
static char           read_buffer[4];
static struct timeval timeout;

/* Local function prototypes. */
static int            get_reply(void),
                      read_msg(void);



/*########################## pop3_connect() #############################*/
int
pop3_connect(char *hostname, int port)
{
   int                     loop_counter = 0;
   struct sockaddr_in      sin;
   register struct hostent *p_host = NULL;
#ifdef FTX
   struct linger           l;
#endif

   (void)memset((struct sockaddr *) &sin, 0, sizeof(sin));
   if ((sin.sin_addr.s_addr = inet_addr(hostname)) == -1)
   {
      if ((p_host = gethostbyname(hostname)) == NULL)
      {
#if !defined (_HPUX) && !defined (_SCO)
         if (h_errno != 0)
         {
#ifdef LINUX
            if ((h_errno > 0) && (h_errno < h_nerr))
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "pop3_connect", NULL,
                         _("Failed to gethostbyname() %s : %s"),
                         hostname, h_errlist[h_errno]);
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "pop3_connect", NULL,
                         _("Failed to gethostbyname() %s (h_errno = %d) : %s"),
                         hostname, h_errno, strerror(errno));
            }
#else
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "pop3_connect", NULL,
                      _("Failed to gethostbyname() %s (h_errno = %d) : %s"),
                      hostname, h_errno, strerror(errno));
#endif
         }
         else
         {
#endif /* !_HPUX && !_SCO */
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "pop3_connect", NULL,
                      _("Failed to gethostbyname() %s : %s"),
                      hostname, strerror(errno));
#if !defined (_HPUX) && !defined (_SCO)
         }
#endif
         return(INCORRECT);
      }

      /* Copy IP number to socket structure. */
      memcpy((char *)&sin.sin_addr, p_host->h_addr, p_host->h_length);
   }

   if ((pop3_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "pop3_connect", NULL,
                _("socket() error : %s"), strerror(errno));
      return(INCORRECT);
   }
   sin.sin_family = AF_INET;
   sin.sin_port = htons((u_short)port);

#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
   if (timeout_flag != OFF)
   {
      int reply = 1;

      if (setsockopt(pop3_fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&reply,
                     sizeof(reply)) < 0)
      {
         trans_log(WARN_SIGN, __FILE__, __LINE__, "pop3_connect", NULL,
                   _("setsockopt() SO_KEEPALIVE error : %s"), strerror(errno));
      }
# ifdef TCP_KEEPALIVE
      reply = timeout_flag;
      if (setsockopt(pop3_fd, IPPROTO_IP, TCP_KEEPALIVE, (char *)&reply,
                     sizeof(reply)) < 0)
      {
         trans_log(WARN_SIGN, __FILE__, __LINE__, "pop3_connect", NULL,
                   _("setsockopt() TCP_KEEPALIVE error : %s"), strerror(errno));
      }
# endif
      timeout_flag = OFF;
   }
#endif

   while (connect(pop3_fd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
   {
      loop_counter++;

      if (loop_counter <= 8)
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
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "pop3_connect", NULL,
                   _("Failed to connect() to %s, have tried %d times : %s"),
                   hostname, loop_counter, strerror(errno));
         (void)close(pop3_fd);
         pop3_fd = -1;
         return(INCORRECT);
      }
      if (close(pop3_fd) == -1)
      {
         trans_log(DEBUG_SIGN, __FILE__, __LINE__, "pop3_connect", NULL,
                   _("close() error : %s"), strerror(errno));
      }
      if ((pop3_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "pop3_connect", NULL,
                   _("socket() error : %s"), strerror(errno));
         (void)close(pop3_fd);
         return(INCORRECT);
      }
   }

#ifdef FTX
   l.l_onoff = 1; l.l_linger = 240;
   if (setsockopt(pop3_fd, SOL_SOCKET, SO_LINGER, (char *)&l,
                  sizeof(struct linger)) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "pop3_connect", NULL,
                _("setsockopt() error : %s"), strerror(errno));
      return(INCORRECT);
   }
#endif

   if (get_reply() != POP3_OK)
   {
      (void)close(pop3_fd);
      return(INCORRECT);
   }

   return(SUCCESS);
}


/*############################# pop3_user() #############################*/
int
pop3_user(char *user)
{
   int reply;

   if ((reply = command(pop3_fd, "USER %s", user)) == SUCCESS)
   {
      reply = get_reply();
   }

   return(reply);
}


/*############################# pop3_pass() #############################*/
int
pop3_pass(char *password)
{
   int reply;

   if ((reply = command(pop3_fd, "PASS %s", password)) == SUCCESS)
   {
      reply = get_reply();
   }

   return(reply);
}


/*############################# pop3_stat() #############################*/
int
pop3_stat(int *no_of_messages, off_t *msg_size)
{
   int reply;

   if ((reply = command(pop3_fd, "STAT")) == SUCCESS)
   {
      if ((reply = get_reply()) == POP3_OK)
      {
         int  i;
         char *ptr,
              str_number[MAX_LONG_LENGTH + 1];

         ptr = msg_str + 4;
         i = 0;
         while (isdigit((int)(*(ptr + i))) && (i < MAX_INT_LENGTH))
         {
            str_number[i] = *(ptr + i);
            i++;
         }
         if (i <= MAX_INT_LENGTH)
         {
            if (i > 0)
            {
               str_number[i] = '\0';
               *no_of_messages = atoi(str_number);
            }
            else
            {
               *no_of_messages = 0;
            }
         }
         else
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, "pop3_stat", msg_str,
                      _("Number of messages in reply to large to store."));
            while ((*(ptr + i) != ' ') && (*(ptr + i) != '\r') &&
                   (*(ptr + i) != '\n') && (*(ptr + i) != '\0'))
            {
               i++;
            }
            *no_of_messages = 0;
         }
         ptr += i;
         i = 0;
         while (isdigit((int)(*(ptr + i))) && (i < MAX_LONG_LENGTH))
         {
            str_number[i] = *(ptr + i);
            i++;
         }
         if (i <= MAX_LONG_LENGTH)
         {
            if (i > 0)
            {
               str_number[i] = '\0';
               *msg_size = (off_t)str2offt(str_number, NULL, 8);
            }
            else
            {
               *msg_size = 0;
            }
         }
         else
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, "pop3_stat", msg_str,
                      _("Size in reply to large to store."));
            *msg_size = 0;
         }
         read_buffer[0] = '\0';
         rb_offset = 0;
      }
   }

   return(reply);
}


/*########################### pop3_retrieve() ###########################*/
int
pop3_retrieve(unsigned int msg_number, off_t *msg_size)
{
   int reply;

   if ((reply = command(pop3_fd, "RETR %u", msg_number)) == SUCCESS)
   {
      if ((reply = get_reply()) == POP3_OK)
      {
         char *ptr;

         ptr = msg_str + 3;
         if (*ptr == ' ')
         {
            int  i;
            char str_number[MAX_LONG_LENGTH + 1];

            i = 0;
            ptr++;
            while (isdigit((int)(*(ptr + i))) && (i < MAX_LONG_LENGTH))
            {
               str_number[i] = *(ptr + i);
               i++;
            }
            if (i <= MAX_LONG_LENGTH)
            {
               if (i > 0)
               {
                  str_number[i] = '\0';
                  *msg_size = (off_t)str2offt(str_number, NULL, 8);
               }
               else
               {
                  trans_log(WARN_SIGN, __FILE__, __LINE__, "pop3_retrieve", msg_str,
                            _("Failed to get size from reply."));
                  *msg_size = 0;
               }
            }
            else
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, "pop3_retrieve", msg_str,
                         _("Size in reply to large to store."));
               *msg_size = 0;
            }
         }
         else
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, "pop3_retrieve", msg_str,
                      _("Failed to get size from reply."));
            *msg_size = 0;
         }
      }
      else
      {
         *msg_size = 0;
      }
   }

   return(reply);
}


/*############################# pop3_read() #############################*/
int
pop3_read(char *block, int blocksize)
{
   int    bytes_read,
          status;
   fd_set rset;

   /* Initialise descriptor set. */
   FD_ZERO(&rset);
   FD_SET(pop3_fd, &rset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = transfer_timeout;

   /* Wait for message x seconds and then continue. */
   status = select(pop3_fd + 1, &rset, NULL, NULL, &timeout);

   if ((status > 0) && (FD_ISSET(pop3_fd, &rset)))
   {
#ifdef WITH_SSL
           if (ssl_con == NULL)
           {
#endif
              if ((bytes_read = read(pop3_fd, &block[rb_offset],
                                     blocksize - rb_offset)) == -1)
              {
                 if (errno == ECONNRESET)
                 {
                    timeout_flag = CON_RESET;
                 }
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, "pop3_read", NULL,
                           _("read() error : %s"), strerror(errno));
                 return(INCORRECT);
              }
#ifdef WITH_SSL
           }
           else
           {
              if ((bytes_read = SSL_read(ssl_con, &block[rb_offset],
                                         blocksize - rb_offset)) <= 0)
              {
                 (void)ssl_error_msg("SSL_read", ssl_con, &status, bytes_read,
                                     msg_str);
                 if (status == SSL_ERROR_SYSCALL)
                 {
                    if (errno == ECONNRESET)
                    {
                       timeout_flag = CON_RESET;
                    }
                 }
                 else if (status == SSL_ERROR_SSL)
                      {
                         timeout_flag = CON_RESET;
                      }
                 trans_log(DEBUG_SIGN, __FILE__, __LINE__, "pop3_read", NULL,
                           _("SSL_read() error %d"), status);
                 return(INCORRECT);
              }
           }
#endif
#ifdef WITH_TRACE
           trace_log(NULL, 0, BIN_R_TRACE, block, bytes_read, NULL);
#endif
           /*
            * Whatch out for byte stuffing and maybe end of file
            * marker.
            */
           if (bytes_read > 0)
           {
              char *p_end,
                   *ptr;

              if (rb_offset > 0)
              {
                 (void)memcpy(block, read_buffer, rb_offset);
              }
              ptr = block;
              p_end = block + rb_offset + bytes_read;
              while (ptr < p_end)
              {
                 if ((*ptr == '\r') && ((p_end - ptr) > 3) &&
                     (*(ptr + 1) == '\n') && (*(ptr + 2) == '.'))
                 {
                    if (*(ptr + 3) == '.')
                    {
                       (void)memcpy((ptr + 3), (ptr + 4), (p_end - (ptr + 4)));
                       p_end--;
                       bytes_read--;
                       ptr += 3;
                    }
                    else if ((*(ptr + 3) == '\r') && (*(ptr + 4) == '\n'))
                         {
                         }
                         else
                         {
                            ptr += 2;
                         }
                 }
                 else
                 {
                    ptr++;
                 }
              }


              if (read_buffer[0] == '\0')
              {
                 int i = 0;

                 status = bytes_read - 1;
                 while ((status >= 0) && (i < 3))
                 {
                    read_buffer[i] = block[status];
                    i++; status--;
                 }
                 read_buffer[i] = '\0';
              }
              else
              {
              }
           }
   }
   else if (status == 0)
        {
           /* Timeout has arrived. */
           timeout_flag = ON;
           return(INCORRECT);
        }
        else
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, "pop3_read", NULL,
                     _("select() error : %s"), strerror(errno));
           return(INCORRECT);
        }

   return(bytes_read);
}


/*############################# pop3_dele() #############################*/
int
pop3_dele(unsigned int msg_number)
{
   int reply;

   if ((reply = command(pop3_fd, "DELE %u", msg_number)) == SUCCESS)
   {
      reply = get_reply();
   }

   return(reply);
}


/*############################# pop3_quit() #############################*/
int
pop3_quit(void)
{
   int reply;

   if (pop3_fd != -1)
   {
      (void)command(pop3_fd, "QUIT");
      if (timeout_flag == OFF)
      {
         if ((reply = get_reply()) == INCORRECT)
         {
            (void)close(pop3_fd);
            return(INCORRECT);
         }

#ifdef _WITH_SHUTDOWN
         if (shutdown(pop3_fd, 1) < 0)
         {
            trans_log(DEBUG_SIGN, __FILE__, __LINE__, "pop3_quit", NULL,
                      _("shutdown() error : %s"), strerror(errno));
         }
         else
         {
            int    status;
            char   buffer[32];
            fd_set rset;

            /* Initialise descriptor set */
            FD_ZERO(&rset);
            FD_SET(pop3_fd, &rset);
            timeout.tv_usec = 0L;
            timeout.tv_sec = transfer_timeout;

            /* Wait for message x seconds and then continue. */
            status = select(pop3_fd + 1, &rset, NULL, NULL, &timeout);

            if (status > 0)
            {
               if (FD_ISSET(pop3_fd, &rset))
               {
                  if ((status = read(pop3_fd, buffer, 32)) < 0)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "pop3_quit", NULL,
                               _("read() error (%d) : %s"),
                               status, strerror(errno));
                     reply = INCORRECT;
                  }
               }
            }
            else if (status == 0)
                 {
                    /* Timeout has arrived. */
                    timeout_flag = ON;
                    reply = INCORRECT;
                 }
                 else
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "pop3_quit", NULL,
                              _("select() error : %s"), strerror(errno));
                    reply = INCORRECT;
                 }
         }
#endif
      }
      else
      {
         reply = SUCCESS;
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
      if (close(pop3_fd) == -1)
      {
         trans_log(DEBUG_SIGN, __FILE__, __LINE__, "pop3_quit", NULL,
                   _("close() error : %s"), strerror(errno));
      }
      pop3_fd = -1;
   }
   else
   {
      reply = SUCCESS;
   }

   return(reply);
}


/*++++++++++++++++++++++++++++++ get_reply() ++++++++++++++++++++++++++++*/
static int
get_reply(void)
{
   int ret;

   for (;;)
   {
      if (read_msg() == INCORRECT)
      {
         return(INCORRECT);
      }

      if ((msg_str[0] == '+') &&
          ((msg_str[1] == 'O') || (msg_str[1] == 'o')) &&
          ((msg_str[2] == 'K') || (msg_str[2] == 'k')))
      {
         ret = POP3_OK;
         break;
      }
      else if ((msg_str[0] == '-') &&
               ((msg_str[1] == 'E') || (msg_str[1] == 'e')) &&
               ((msg_str[2] == 'R') || (msg_str[2] == 'r')) &&
               ((msg_str[3] == 'R') || (msg_str[3] == 'r')))
           {
              ret = POP3_ERROR;
              break;
           }
   }

   return(ret);
}


/*----------------------------- read_msg() ------------------------------*/
static int
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
         /* Initialise descriptor set. */
         FD_SET(pop3_fd, &rset);
         timeout.tv_usec = 0L;
         timeout.tv_sec = transfer_timeout;

         /* Wait for message x seconds and then continue. */
         status = select(pop3_fd + 1, &rset, NULL, NULL, &timeout);

         if (status == 0)
         {
            /* Timeout has arrived. */
            timeout_flag = ON;
            bytes_read = 0;
            return(INCORRECT);
         }
         else if (FD_ISSET(pop3_fd, &rset))
              {
#ifdef WITH_SSL
                 if (ssl_con == NULL)
                 {
#endif
                    if ((bytes_read = read(pop3_fd, &msg_str[bytes_buffered],
                                           (MAX_RET_MSG_LENGTH - bytes_buffered))) < 1)
                    {
                       if (bytes_read == 0)
                       {
                          trans_log(ERROR_SIGN,  __FILE__, __LINE__, "read_msg", NULL,
                                    _("Remote hang up."));
                          timeout_flag = NEITHER;
                       }
                       else
                       {
                          if (errno == ECONNRESET)
                          {
                             timeout_flag = CON_RESET;
                          }
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_msg", NULL,
                                    _("read() error (after reading %d bytes) : %s"),
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
                          trans_log(ERROR_SIGN,  __FILE__, __LINE__,
                                    "read_msg", NULL,
                                    _("Remote hang up."));
                          timeout_flag = NEITHER;
                       }
                       else
                       {
                          (void)ssl_error_msg("SSL_read", ssl_con, &status,
                                              bytes_read, msg_str);
                          if (status == SSL_ERROR_SYSCALL)
                          {
                             if (errno == ECONNRESET)
                             {
                                timeout_flag = CON_RESET;
                             }
                          }
                          else if (status == SSL_ERROR_SSL)
                               {
                                  timeout_flag = CON_RESET;
                               }
                          trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                    "read_msg", msg_str,
                                    _("SSL_read() error (after reading %d bytes) (%d)"),
                                    bytes_buffered, status);
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
                           _("select() error : %s"), strerror(errno));
                 return(INCORRECT);
              }
              else
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, "read_msg", NULL,
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
            bytes_read--;
            return(bytes_buffered);
         }
         read_ptr++;
         bytes_read--;
      } while(bytes_read > 0);
   } /* for (;;) */
}
