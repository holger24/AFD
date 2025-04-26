/*
 *  ssh_common.c - Part of AFD, an automatic file distribution program.
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
 **   ssh_common - functions that can be used for SSH protocol
 **
 ** SYNOPSIS
 **   int     ssh_exec(char *host, int port, unsigned char ssh_protocol,
 **                    int ssh_options, char *user, char *passwd,
 **                    char *cmd, char *subsystem, int *fd)
 **   int     ssh_login(int data_fd, char *passwd)
 **   size_t  pipe_write(int fd, char *buf, size_t count)
 **   int     get_ssh_reply(int fd, int check_reply)
 **
 ** DESCRIPTION
 **   The idea to split ssh_exec() and ssh_login() into two separate
 **   functions and using unix sockets to make the password handling
 **   better was taken from the GFTP package, see http://gftp.seul.org/.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.01.2006 H.Kiehl Created
 **   23.07.2006 H.Kiehl Added host fingerprint check.
 **   18.04.2009 H.Kiehl Added compression.
 **   03.08.2012 H.Kiehl In function ssh_login() use O_NONBLOCK for reading
 **                      initial string from ssh process instead of an
 **                      alarm().
 **   19.10.2018 H.Kiehl Change compression parameter to ssh_options to be
 **                      able to path more options.
 **   26.04.2025 H.Kiehl Set ServerAliveInterval when keep connected is set.
 */
DESCR__E_M3


#include <stdio.h>
#include <string.h>       /* memcpy(), strerror()                        */
#include <stdlib.h>       /* malloc(), free(), exit()                    */
#include <sys/types.h>    /* fd_set                                      */
#include <signal.h>       /* kill()                                      */
#include <sys/wait.h>     /* waitpid()                                   */
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#include <sys/time.h>     /* struct timeval                              */
#include <sys/stat.h>     /* S_ISUID, S_ISGID, etc                       */
#ifdef WITH_SSH_FINGERPRINT
# ifdef WITH_REMOVE_FROM_KNOWNHOSTS
#  include <sys/mman.h>   /* mmap(), msync(), munmap()                   */
#  include <pwd.h>        /* getpwuid()                                  */
# endif
#endif
#ifdef HAVE_PTY_H
# include <pty.h>
#endif
#ifdef HAVE_LIBUTIL_H
# include <libutil.h>
#endif
#ifdef HAVE_UTIL_H
# include <util.h>
#endif
#include <termios.h>
#include <unistd.h>       /* select(), write(), read(), close()          */
#include <fcntl.h>        /* open(), fcntl()                             */
#include <errno.h>
#include "fddefs.h"
#include "ssh_commondefs.h"

/* Global variables. */
pid_t                  data_pid;

/* External global variables. */
extern int             timeout_flag;
extern long            transfer_timeout;
extern char            msg_str[];

/* Local global variables. */
static int             fdm;
#ifdef WITH_SSH_FINGERPRINT
# ifdef WITH_REMOVE_FROM_KNOWNHOSTS
static struct ssh_data sd;
# endif
#endif

/* Local function prototypes. */
static int             get_passwd_reply(int),
                       ptym_open(char *),
                       ptys_open(char *),
#ifdef WITH_SSH_FINGERPRINT
# ifdef WITH_REMOVE_FROM_KNOWNHOSTS
                       remove_from_knownhosts(char *),
# endif
#endif
                       tty_raw(int);
#ifdef WITH_TRACE
static size_t          pipe_write_np(int, char *, size_t);
#endif

#define NO_PROMPT 0

/* #define _WITH_DEBUG */


/*############################## ssh_exec() #############################*/
int
ssh_exec(char          *host,
         int           port,
         unsigned char ssh_protocol,
         int           ssh_options,
#ifndef FORCE_SFTP_NOOP
         int           keep_connected_set,
#endif
         char          *user,
         char          *passwd,
         char          *cmd,
         char          *subsystem,
         int           *fd)
{
   int  status;
   char pts_name[MAX_PATH_LENGTH],
        *identityFilePath = NULL, /* To store the identity part. */
        *passwdBeg = NULL,
        *idBeg = NULL,
        *idEnd,
        *ptr;

   /* We want to be generic and allow a user to place the */
   /* tags in any order.                                  */

   if (passwd != NULL)
   {
      /* Do we have an identity tag "<i>" ? */
      if ((ptr = strstr(passwd, "<i>")))
      {
         idBeg = ptr + 3;
      }

      /* Do we have a passwd tag "<p>" ? */
      if ((ptr = strstr(passwd, "<p>")))
      {
         passwdBeg = ptr + 3;
      }

      /* Locate identity end tag. */
      if (passwdBeg < idBeg)  /* Id is after Pwd. */
      {
         idEnd = idBeg + (strlen(idBeg) - 1);
      }
      else  /* Id ends with Pwd. */
      {
         idEnd = passwdBeg - 4;
      }

      /* Last case, we have no tag. We should have a password alone. */
      if (!passwdBeg && !idBeg)
      {
         passwdBeg = passwd;
      }

      if (idBeg)
      {
         int len;

         len = idEnd - idBeg + 1;
         if ((identityFilePath = malloc(len + 1)) == NULL)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_exec", NULL,
                      _("malloc() error : %s"), strerror(errno));
            return(INCORRECT);
         }
         memcpy(identityFilePath, idBeg, len);
         identityFilePath[len] = 0;
      }
   }

   msg_str[0] = '\0';
   if ((fdm = ptym_open(pts_name)) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_exec", NULL,
                _("ptym_open() error"));
      status = INCORRECT;
      data_pid = -3;
   }
   else
   {
      int sock_fd[2];

      /* Prepare unix socket for parent child communication. */
#ifdef AF_LOCAL
      if (socketpair(AF_LOCAL, SOCK_STREAM, 0, sock_fd) == -1)
#else
      if (socketpair(AF_UNIX, SOCK_STREAM, 0, sock_fd) == -1)
#endif
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_exec", NULL,
                   _("socketpair() error : %s"), strerror(errno));
         status = INCORRECT;
         data_pid = -3;
      }
      else
      {
         int pipe_fds[2];

         if (pipe(pipe_fds) == -1)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_exec", NULL,
                      _("pipe() error : %s"), strerror(errno));
            status = INCORRECT;
            data_pid = -3;
         }
         else
         {
            if ((data_pid = fork()) == 0)  /* Child process. */
            {
               char *args[25],
                    dummy,
                    str_protocol[1 + 3 + 1],
                    str_port[MAX_INT_LENGTH],
                    str_server_alive_interval[22 + MAX_INT_LENGTH + 1],
                    str_timeout[17 + MAX_INT_LENGTH + 1];
               int  argcount,
                    fds;
#ifdef WITH_TRACE
               int  i,
                    length;
               char buffer[MAX_PATH_LENGTH];
#endif

               setsid();
               if ((fds = ptys_open(pts_name)) < 0)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_exec", NULL,
                            _("ptys_open() error"));
                  (void)close(fdm);
                  _exit(INCORRECT);
               }
               (void)close(fdm);
               if (tty_raw(fds) == -1)
               {
                  trans_log(DEBUG_SIGN, __FILE__, __LINE__, "ssh_exec", NULL,
                            _("tty_raw() failed in child!"));
               }

               (void)close(sock_fd[0]);

               dup2(sock_fd[1], STDIN_FILENO);
               dup2(sock_fd[1], STDOUT_FILENO);
               dup2(fds, STDERR_FILENO);

               if (fds > 2)
               {
                  (void)close(fds);
               }

               args[0] = SSH_COMMAND;
               argcount = 1;
               if (ssh_protocol != 0)
               {
                  args[argcount] = str_protocol;
                  argcount++;
                  (void)snprintf(str_protocol, 1 + 3 + 1,
                                 "-%d", (int)ssh_protocol);
               }
               if (ssh_options & ENABLE_COMPRESSION)
               {
                  args[argcount] = "-C";
                  argcount++;
               }
               if (ssh_options & DISABLE_STRICT_HOST_KEY)
               {
                  args[argcount] = "-oUserKnownHostsFile /dev/null";
                  argcount++;
                  args[argcount] = "-oStrictHostKeyChecking no";
                  argcount++;
               }
#ifdef WITH_TRACE
               args[argcount] = "-oLogLevel debug";
               argcount++;
#endif
               args[argcount] = "-oForwardX11 no";
               argcount++;
               args[argcount] = "-oForwardAgent no";
               argcount++;
               args[argcount] = "-oPermitLocalCommand no";
               argcount++;
               args[argcount] = "-oClearAllForwardings yes";
               argcount++;
               args[argcount] = str_timeout;
               argcount++;
               (void)snprintf(str_timeout, 17 + MAX_INT_LENGTH + 1,
                              "-oConnectTimeout %ld", transfer_timeout);
               if (keep_connected_set == YES)
               {
                  int alive_interval = transfer_timeout - 4;

                  if (alive_interval > 0)
                  {
                     (void)snprintf(str_server_alive_interval,
                                    22 + MAX_INT_LENGTH + 1,
                                    "-oServerAliveInterval %d",
                                    alive_interval);
                     args[argcount] = str_server_alive_interval;
                     argcount++;
                  }
               }
#ifdef _WITH_FALLBACK_TO_RSH_NO
               args[argcount] = "-oFallBackToRsh no";
               argcount++;
#endif
               if (port != SSH_PORT_UNSET)
               {
                  args[argcount] = "-p";
                  argcount++;
                  args[argcount] = str_port;
                  argcount++;
                  (void)snprintf(str_port, MAX_INT_LENGTH, "%d", port);
               }
               if (subsystem != NULL)
               {
                  args[argcount] = "-e";
                  argcount++;
                  args[argcount] = "none";
                  argcount++;
               }
               if (identityFilePath != NULL)
               {
                  args[argcount] = "-i";
                  argcount++;
                  args[argcount] = identityFilePath;
                  argcount++;
               }
               if (user != NULL)
               {
                  args[argcount] = "-l";
                  argcount++;
                  args[argcount] = user;
                  argcount++;
               }
               args[argcount] = host;
               argcount++;
               if (subsystem != NULL)
               {
                  args[argcount] = "-s";
                  argcount++;
                  args[argcount] = subsystem;
                  argcount++;
               }
               if (cmd != NULL)
               {
                  args[argcount] = cmd;
                  argcount++;
               }
               args[argcount] = NULL;
               argcount++;

#ifdef WITH_TRACE
               length = 0;
               for (i = 0; i < (argcount - 1); i++)
               {
                  length += snprintf(&buffer[length], MAX_PATH_LENGTH - length,
                                     "%s ", args[i]);
               }
               if (length > MAX_PATH_LENGTH)
               {
                  length = MAX_PATH_LENGTH;
               }
               trace_log(__FILE__, __LINE__, C_TRACE, buffer, length, NULL);
#endif
               /* Synchronize with parent. */
               (void)close(pipe_fds[1]);
               if (read(pipe_fds[0], &dummy, 1) != 1)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_exec", NULL,
                            _("read() error : %s"), strerror(errno));
               }
               (void)close(pipe_fds[0]);

               (void)execvp(SSH_COMMAND, args);
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_exec", NULL,
                         _("execvp() error : %s"), strerror(errno));
               _exit(INCORRECT);
            }
            else if (data_pid > 0) /* Parent process. */
                 {
                    (void)close(sock_fd[1]);

                    /* Synchronize with child. */
                    (void)close(pipe_fds[0]);
                    if (write(pipe_fds[1], "", 1) != 1)
                    {
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_exec", NULL,
                                 _("write() error : %s"), strerror(errno));
                    }
                    (void)close(pipe_fds[1]);

                    *fd = sock_fd[0];
                    if (tty_raw(fdm) == -1)
                    {
                       trans_log(DEBUG_SIGN, __FILE__, __LINE__, "ssh_exec", NULL,
                                 _("tty_raw() failed in parent!"));
                    }

                    status = SUCCESS;
                 }
                 else /* Failed to fork(). */
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_exec", NULL,
                              _("fork() error : %s"), strerror(errno));
                    data_pid = -3;
                    status = INCORRECT;
                 }
         }
      }
   }

   (void)free(identityFilePath);
#ifdef WITH_SSH_FINGERPRINT
# ifdef WITH_REMOVE_FROM_KNOWNHOSTS
   (void)strcpy(sd.hostname, host);
   (void)strcpy(sd.user, user);
   sd.port = port;
# endif
#endif
   return(status);
}


/*############################ ssh_login() ##############################*/
int
#ifdef WITH_SSH_FINGERPRINT
ssh_login(int data_fd, char *passwd, char debug, char *fingerprint)
#else
ssh_login(int data_fd, char *passwd, char debug)
#endif
{
   int            eio_loops = 0,
                  max_fd,
                  rr_loops = 0,
                  ssh_client_up = NO,
                  status;
   char           *password = NULL, /* To store the password part. */
                  *passwdBeg = NULL,
                  *passwdEnd,
                  *idBeg = NULL,
                  *ptr;
   fd_set         eset,
                  rset;
   struct timeval timeout;

   /* We want to be generic and allow a user to place the */
   /* tags in any order.                                  */

   if ((passwd != NULL) && (passwd[0] != '\0'))
   {
      /* Do we have an identity tag "<i>" ? */
      if ((ptr = strstr(passwd, "<i>")))
      {
         idBeg = ptr + 3;
      }

      /* Do we have a passwd tag "<p>" ? */
      if ((ptr = strstr(passwd, "<p>")))
      {
         passwdBeg = ptr + 3;
      }

      /* Locate password end tag. */
      if (idBeg < passwdBeg)  /* Pwd is after Id. */
      {
         passwdEnd = passwdBeg + (strlen(passwdBeg) - 1);
      }
      else  /* Pwd ends with Id. */
      {
         passwdEnd = idBeg - 4;
      }

      /* Last case, we have no tag. We should have a password alone. */
      if (!passwdBeg && !idBeg)
      {
         passwdBeg = passwd;
         passwdEnd = passwdBeg + (strlen(passwdBeg) - 1);
      }

      /* Copy what we found. If we don't have a password, nor an           */
      /* identity file, carry on anyways. In this case, ssh will use       */
      /* the ~/.ssh/id_dsa (or rsa) file. It should not have a passphrase! */

      if (passwdBeg)
      {
         int len;

         len = passwdEnd - passwdBeg + 1;
         if ((password = malloc(len + 2)) == NULL)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                      _("malloc() error : %s"), strerror(errno));
            return(INCORRECT);
         }
         memcpy(password, passwdBeg, len);
         password[len] = '\n';
         password[len + 1] = 0;
      }
   }

   /* Initialize select variables. */
   max_fd = fdm;
   if (data_fd > max_fd)
   {
      max_fd = data_fd;
   }
   max_fd++;
   FD_ZERO(&rset);
   FD_ZERO(&eset);

   for (;;)
   {
retry_read_with_stat:
      FD_SET(data_fd, &rset);
      FD_SET(data_fd, &eset);
      FD_SET(fdm, &rset);
      FD_SET(fdm, &eset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = transfer_timeout + 7;

      status = select(max_fd, &rset, NULL, &eset, &timeout);
      if (status > 0)
      {
retry_read:
          if (FD_ISSET(data_fd, &eset) || FD_ISSET(fdm, &eset))
          {
             trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                       _("Unix socket error."));
             status = INCORRECT;
             break;
          }
          else if (FD_ISSET(fdm, &rset))
               {
                  int flags,
                      tmp_errno;

                  /*
                   * Since it sometimes happens that also select() tells
                   * us that data is there ready to be read but there is
                   * no data available. Thus we have to set O_NONBLOCK
                   * otherwise we will block here.
                   * We also have tried this (up to 1.4.4) to set an alarm.
                   * This had the disadvantage that we lost a lot of time
                   * waiting for the alarm.
                   */
                  if ((flags = fcntl(fdm, F_GETFL, 0)) == -1)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                               "Failed to get flag via fcntl() : %s", strerror(errno));
                     status = INCORRECT;
                     break;
                  }
                  flags |= O_NONBLOCK;
                  if (fcntl(fdm, F_SETFL, flags) == -1)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                               "Failed to set O_NONBLOCK flag via fcntl() : %s",
                               strerror(errno));
                     status = INCORRECT;
                     break;
                  }
                  if (((status = read(fdm, msg_str, MAX_RET_MSG_LENGTH)) == -1) &&
                      (errno == EAGAIN))
                  {
                     if ((rr_loops > 5) || (debug > 0))
                     {
                        trans_log(DEBUG_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                                  _("Hit an Input/Output error, assuming child was not up. Retrying (%d)."),
                                  rr_loops);
                     }
                     rr_loops++;
                     if (rr_loops == 11)
                     {
                        break;
                     }
                     else
                     {
                        if ((flags = fcntl(fdm, F_GETFL, 0)) == -1)
                        {
                           trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                                     "Failed to get flag via fcntl() : %s", strerror(errno));
                           status = INCORRECT;
                           break;
                        }
                        flags &= ~O_NONBLOCK;
                        if (fcntl(fdm, F_SETFL, flags) == -1)
                        {
                           trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                                     "Failed to unset O_NONBLOCK flag via fcntl() : %s",
                                     strerror(errno));
                           status = INCORRECT;
                           break;
                        }
                        goto retry_read_with_stat;
                     }
                  }
                  tmp_errno = errno;
                  if ((flags = fcntl(fdm, F_GETFL, 0)) == -1)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                               "Failed to get flag via fcntl() : %s", strerror(errno));
                     status = INCORRECT;
                     break;
                  }
                  flags &= ~O_NONBLOCK;
                  if (fcntl(fdm, F_SETFL, flags) == -1)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                               "Failed to unset O_NONBLOCK flag via fcntl() : %s",
                               strerror(errno));
                     status = INCORRECT;
                     break;
                  }

                  if (status < 0)
                  {
                     if (ssh_client_up == YES)
                     {
                        /* SSH client has disconnected. */
                        trans_log(DEBUG_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                                  "SSH Client disconnected");
                        status = INCORRECT;
                        break;
                     }
                     else
                     {
                        if (ssh_child_up() == NO)
                        {
                           status = INCORRECT;
                           break;
                        }
                        if ((tmp_errno == EIO) && (eio_loops < 10))
                        {
                           if ((eio_loops > 5) || (debug > 0))
                           {
                              trans_log(DEBUG_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                                        _("Hit an Input/Output error, assuming child was not up. Retrying (%d)."),
                                        eio_loops);
                           }
                           (void)my_usleep(200000L);
                           eio_loops++;
                           continue;
                        }
                        else
                        {
                           if ((tmp_errno == EIO) && (eio_loops > 0))
                           {
                              trans_log(DEBUG_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                                        _("Hit an Input/Output error, even after retrying %d times."),
                                        eio_loops);
                           }
                           if (tmp_errno == ECONNRESET)
                           {
                              timeout_flag = CON_RESET;
                           }
                           trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                                     _("read() error (%d): %s"),
                                     status, strerror(tmp_errno));
                           status = INCORRECT;
                        }
                     }
                  }
                  else
                  {
#ifdef WITH_TRACE
                     trace_log(NULL, 0, CRLF_R_TRACE, msg_str, status, NULL);
                     msg_str[status] = '\0';
#else
                     msg_str[status] = '\0';
                     if (debug > 0)
                     {
                        int i = status - 1;

                        while ((i > 0) &&
                               ((msg_str[i] == '\r') || (msg_str[i] == '\n')))
                        {
                           msg_str[i] = '\0';
                           i--;
                        }
                        trans_log(DEBUG_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                                  "SSH client response = `%s'", msg_str);
                     }
#endif
                     ssh_client_up = YES;
                     if (status == 0)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                                  _("SSH program closed the socket unexpected."));
                        status = INCORRECT;
                     }
                     else
                     {
                        char *ptr = msg_str;

                        if ((lposi(ptr, "assword:", 8) != NULL) ||
                            (!strncmp(msg_str, "Enter passphrase", 16)))
                        {
                           if (debug > 0)
                           {
                              trans_log(DEBUG_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                                        "Identified password prompt");
                           }
                           if ((password != NULL) && (password[0] != '\0'))
                           {
                              size_t length = strlen(password);

#ifdef WITH_TRACE
                              if ((status = pipe_write_np(fdm, password, length)) != length)
#else
                              if ((status = pipe_write(fdm, password, length)) != length)
#endif
                              {
                                 if (errno != 0)
                                 {
                                    msg_str[0] = '\0';
                                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                                              _("write() error [%d] : %s"),
                                              status, strerror(errno));
                                 }
                              }
                              else
                              {
                                 /* Check if correct password was entered. */
                                 msg_str[0] = '\0';
                                 if ((status = get_passwd_reply(fdm)) != SUCCESS)
                                 {
                                    while (*ptr)
                                    {
                                       if ((*ptr == '\n') || (*ptr == '\r'))
                                       {
                                          *ptr = ' ';
                                       }
                                       ptr++;
                                    }
                                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", msg_str,
                                              _("Failed to enter password."));
                                    status = INCORRECT;
                                    msg_str[0] = '\0';
                                 }
                              }
                           }
                           else /* No password given. */
                           {
                              /* It's asking for a password or passphrase and */
                              /* we don't have one. Report error.             */
                              while (*ptr)
                              {
                                 if ((*ptr == '\n') || (*ptr == '\r'))
                                 {
                                    *ptr = ' ';
                                 }
                                 ptr++;
                              }
                              trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", msg_str,
                                        _("ssh is asking for password (or passphrase) and none is provided. Bailing out!"));
                              status = INCORRECT;
                              msg_str[0] = '\0';
                           }
                        }
                        else if ((status == 1) &&
                                 ((msg_str[0] == '\n') || (msg_str[0] == ' ')))
                             {
                                status = SUCCESS;
#ifdef WITH_EFENCE
                                sleep(1);
                                continue;
#endif
                             }
                        /*
                         * It's not asking for a password. Three cases :
                         * 1) We're using a private key (Identity file)
                         * 2) It's asking for something else (prior host key
                         *    exchange or host key mismatch).
                         * 3) It's an unknown failure. Go on, we'll catch by
                         *    later (with a timeout, and no good message. Bad).
                         */
                        else if (lposi(ptr, "(yes/no)", 8) != NULL)
                             {
#ifdef WITH_SSH_FINGERPRINT
                                if ((fingerprint[0] == '\0') ||
                                    (posi(ptr, fingerprint) == NULL))
                                {
#endif
                                   if ((status = pipe_write(fdm, "no\n", 3)) != 3)
                                   {
                                      if (errno != 0)
                                      {
                                         trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                                                   _("write() error [%d] : %s"),
                                                   status, strerror(errno));
                                      }
                                   }
                                   else
                                   {
                                      msg_str[0] = '\0';
                                      if ((status = get_ssh_reply(fdm, YES)) != SUCCESS)
                                      {
                                         trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                                                   _("Failed to send `no' to verify SSH connection. [%d]"),
                                                   status);
                                      }
                                   }
                                   trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
#ifdef WITH_SSH_FINGERPRINT
                                             _("Please connect to this host with the command line SSH utility and answer this question appropriately, or use fingerprints."));
#else
                                             _("Please connect to this host with the command line SSH utility and answer this question appropriately."));
#endif
                                   status = INCORRECT;
#ifdef WITH_SSH_FINGERPRINT
                                }
                                else
                                {
                                   if ((status = pipe_write(fdm, "yes\n", 4)) != 4)
                                   {
                                      if (errno != 0)
                                      {
                                         trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                                                   _("write() error [%d] : %s"),
                                                   status, strerror(errno));
                                      }
                                      status = INCORRECT;
                                   }
                                   else
                                   {
                                      msg_str[0] = '\0';
                                      if ((status = get_ssh_reply(fdm, YES)) != SUCCESS)
                                      {
                                         trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                                                   _("Failed to send `yes' to verify SSH connection. [%d]"),
                                                   status);
                                      }
                                      continue;
                                   }
                                }
#endif
                             }
#ifdef WITH_SSH_FINGERPRINT
# ifdef WITH_REMOVE_FROM_KNOWNHOSTS
                        else if ((lposi(ptr, "DOING SOMETHING NASTY!", 22) != NULL) ||
                                 (lposi(ptr, "man-in-the-middle attack", 24) != NULL) ||
                                 (lposi(ptr, "known_hosts", 11) != NULL))
                             {
                                if ((fingerprint[0] != '\0') &&
                                    (posi(ptr, fingerprint) != NULL))
                                {
                                   status = remove_from_knownhosts(sd.hostname);
                                }
                                else
                                {
                                   continue;
                                }
                             }
# endif
#endif
#ifdef WITH_EFENCE
                        else if (lposi(ptr, "Electric Fence", 14) != NULL)
                             {
                                status = SUCCESS;
                                continue;
                             }
#endif
#ifdef WITH_TRACE
                        else if (strncmp(msg_str, "debug1: ", 8) == 0)
                             {
                                status = SUCCESS;
                                continue;
                             }
#endif
                        else if (lposi(ptr, "Warning: Permanently added", 26) != NULL)
                             {
                                /* Just some info that key has been added. */
                                status = SUCCESS;
                                continue;
                             }
                             else
                             {
                                int tmp_status;

                                /*
                                 * If the ssh daemon sends a banner this is
                                 * send to us by the ssh client in another
                                 * buffer. So we need to ensure there is no
                                 * more data in the pipe otherwise we will not
                                 * see password prompt and or other messages
                                 * that are important.
                                 */
                                FD_SET(data_fd, &rset);
                                FD_SET(data_fd, &eset);
                                FD_SET(fdm, &rset);
                                FD_SET(fdm, &eset);
                                timeout.tv_usec = 0L;
                                timeout.tv_sec = 5L;
                                tmp_status = select(max_fd, &rset, NULL, &eset,
                                                    &timeout);
                                if (tmp_status > 0)
                                {
                                   status = tmp_status;
                                   goto retry_read;
                                }

                                /* Replace '\n's by spaces for logging. */
                                while (*ptr++)
                                {
                                   if ((*ptr == '\n') || (*ptr == '\r'))
                                   {
                                      *ptr = ' ';
                                   }
                                }
                                trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", msg_str,
                                          _("Protocol error. SSH is complaining, see next message (%d)."),
                                          status);
                                if (status == 1)
                                {
                                   trans_log(DEBUG_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                                             "msg_str[0] = %d", msg_str[0]);
                                }
                                msg_str[0] = '\0';
                                status = INCORRECT;
                             }
                     }
                  }
                  break;
               }
          else if (FD_ISSET(data_fd, &rset))
               {
                  /*
                   * We need to check if this is not a close event
                   * from the SSH client closing the connection for
                   * what ever reason. When one starts many openssh
                   * clients at the same time, this is what can happen
                   * regularly. I do not know why this is so.
                   */
                  if (ssh_child_up() == NO)
                  {
                     status = INCORRECT;
                  }
                  else
                  {
                     if (write(data_fd, "", 0) == -1)
                     {
                        /* SSH client has closed the pipe! */
                        if (ssh_child_up() == NO)
                        {
                           status = INCORRECT;
                        }
                        else
                        {
                           status = RETRY;
                        }
                     }
                     else
                     {
                        /* No password required to login. */
                        status = SUCCESS;
                     }
                  }
                  break;
               }
      }
      else if (status == 0) /* Timeout. */
           {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                         _("SSH program not responding."));
               status = INCORRECT;
               timeout_flag = ON;
               if (data_pid > 0)
               {
                  if (kill(data_pid, SIGKILL) == -1)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
#if SIZEOF_PID_T == 4
                               _("Failed to kill() data ssh process %d : %s"),
#else
                               _("Failed to kill() data ssh process %lld : %s"),
#endif
                               (pri_pid_t)data_pid, strerror(errno));
                  }
                  else
                  {
                     trans_log(WARN_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                               _("Killed ssh process."));
                     my_usleep(100000);
                     (void)waitpid(data_pid, NULL, WNOHANG);
                     data_pid = 0;
                  }
               }
               break;
           }
           else
           {
              if ((errno != EINTR) && (errno != EAGAIN))
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                           _("select() error : %s"), strerror(errno));
                 break;
              }
           }
   } /* for (;;) */
   (void)free(password);

   return(status);
}


/*########################### ssh_child_up() ############################*/
int
ssh_child_up(void)
{
   pid_t pid;

   /* For now report if ssh transport process terminated. */
   if ((pid = waitpid(data_pid, NULL, WNOHANG)) == data_pid)
   {
      trans_log(DEBUG_SIGN, __FILE__, __LINE__, "ssh_child_up", NULL,
                "SSH process terminated.");
      data_pid = -2;
      return(NO);
   }

   return(YES);
}


/*############################# pipe_write() ############################*/
size_t
pipe_write(int fd, char *buf, size_t count)
{
   if (fd != -1)
   {
      int            status;
      fd_set         wset;
      struct timeval timeout;

      /* Initialise descriptor set. */
      FD_ZERO(&wset);
      FD_SET(fd, &wset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = transfer_timeout;

      /* Wait for message x seconds and then continue. */
      status = select(fd + 1, NULL, &wset, NULL, &timeout);
      if (status == 0)
      {
         /* Timeout has arrived. */
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "pipe_write", NULL,
                   _("There is no reply from pipe, failed to send command %s."),
                   buf);
      }
      else if (FD_ISSET(fd, &wset))
           {
#ifdef _WITH_DEBUG
              char tmp_char;

              tmp_char = buf[count - 1];
              buf[count - 1] = '\0';
              trans_log(INFO_SIGN, NULL, 0, "pipe_write", NULL,
                        "== Pipe Writting (%d) ==", count);
              buf[count - 1] = tmp_char;
#endif
#ifdef WITH_TRACE
              trace_log(NULL, 0, W_TRACE, buf, count, NULL);
#endif
              return(write(fd, buf, count));
           }
           else
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "pipe_write", NULL,
                        _("select() error : %s"), strerror(errno));
           }
   }
   errno = 0;
   return(INCORRECT);
}


#ifdef WITH_TRACE
/*########################### pipe_write_np() ###########################*/
static size_t
pipe_write_np(int fd, char *buf, size_t count)
{
   if (fd != -1)
   {
      int            status;
      fd_set         wset;
      struct timeval timeout;

      /* Initialise descriptor set. */
      FD_ZERO(&wset);
      FD_SET(fd, &wset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = transfer_timeout;

      /* Wait for message x seconds and then continue. */
      status = select(fd + 1, NULL, &wset, NULL, &timeout);
      if (status == 0)
      {
         /* Timeout has arrived. */
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "pipe_write_np", NULL,
                   _("There is no reply from pipe, failed to send command %s."),
                   buf);
      }
      else if (FD_ISSET(fd, &wset))
           {
# ifdef _WITH_DEBUG
              char tmp_char;

              tmp_char = buf[count - 1];
              buf[count - 1] = '\0';
              trans_log(INFO_SIGN, NULL, 0, "pipe_write_np", NULL,
                        "== Pipe Writting (%d) ==", count);
              buf[count - 1] = tmp_char;
# endif
              trace_log(NULL, 0, W_TRACE, "XXXX", 4, NULL);
              return(write(fd, buf, count));
           }
           else
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "pipe_write_np", NULL,
                        _("select() error : %s"), strerror(errno));
           }
   }
   errno = 0;
   return(INCORRECT);
}
#endif /* WITH_TRACE */


/*########################### get_ssh_reply() ###########################*/
int
get_ssh_reply(int fd, int check_reply)
{
   int            status;
   fd_set         rset;
   struct timeval timeout;

   FD_ZERO(&rset);
   FD_SET(fd, &rset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = transfer_timeout;

   status = select(fd + 1, &rset, NULL, NULL, &timeout);

   if (status == 0)
   {
      msg_str[0] = '\0';
      timeout_flag = ON;
      status = INCORRECT;
   }
   else if (FD_ISSET(fd, &rset))
        {
           if ((status = read(fd, msg_str, MAX_RET_MSG_LENGTH)) < 0)
           {
              if (errno == ECONNRESET)
              {
                 timeout_flag = CON_RESET;
              }
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "get_ssh_reply", NULL,
                        _("read() error : %s"), strerror(errno));
              status = INCORRECT;
           }
           else
           {
#ifdef WITH_TRACE
              trace_log(NULL, 0, R_TRACE, msg_str, status, NULL);
#endif
              msg_str[status] = '\0';
              if (status == 0)
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, "get_ssh_reply", NULL,
                           _("Other side closed the pipe."));
                 status = INCORRECT;
              }
              else
              {
                 char *ptr = msg_str;

                 while (*ptr)
                 {
                    if (*ptr == '\n')
                    {
                       *ptr = ' ';
                    }
                    ptr++;
                 }
#ifdef _WITH_DEBUG
                 if (status == 1)
                 {
                    trans_log(INFO_SIGN, NULL, 0, "get_ssh_reply", NULL,
                              "== Reading ONE byte %d ==", (int)msg_str[0]);
                 }
                 else
                 {
                    trans_log(INFO_SIGN, NULL, 0, "get_ssh_reply", NULL,
                              "== Reading (%d) ==", status);
                 }
#endif
                 if (check_reply == YES)
                 {
                    if (msg_str[status - 1] == '\n')
                    {
                       msg_str[status - 1] = '\0';
                    }

                    if ((msg_str[0] == 1) || /* This is a ^A */
                        (msg_str[0] == 2))   /* Fatal error. */
                    {
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, "get_ssh_reply", NULL,
                                 "scp error : %s", msg_str + 1);
                       status = INCORRECT; /* This could be an exit !! */
                    }
                    else
                    {
                       status = SUCCESS;
                    }
                 }
              }
           }
        }
        else
        {
           msg_str[0] = '\0';
           trans_log(ERROR_SIGN, __FILE__, __LINE__, "get_ssh_reply", NULL,
                     _("select() error : %s"), strerror(errno));
           status = INCORRECT;
        }

   return(status);
}


#ifdef WITH_SSH_FINGERPRINT
# ifdef WITH_REMOVE_FROM_KNOWNHOSTS
/*###################### remove_from_knownhosts() #######################*/
static int
remove_from_knownhosts(char *hostname)
{
   int           ret;
   struct passwd *pwd;

   if ((pwd = getpwuid(getuid())) != NULL)
   {
      int  fd;
      char fullname[MAX_PATH_LENGTH];

      (void)snprintf(fullname, MAX_PATH_LENGTH,
                     "%s/.ssh/known_hosts", pwd->pw_dir);

      if ((fd = lock_file(fullname, ON)) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to open/lock file `%s' : %s"),
                    fullname, strerror(errno));
         ret = INCORRECT;
      }
      else
      {
#  ifdef HAVE_STATX
         struct statx stat_buf;
#  else
         struct stat stat_buf;
#  endif

#  ifdef HAVE_STATX
         if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                   STATX_SIZE, &stat_buf) == -1)
#  else
         if (fstat(fd, &stat_buf) == -1)
#  endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
#  ifdef HAVE_STATX
                       _("Failed to statx() `%s' : %s"),
#  else
                       _("Failed to fstat() `%s' : %s"),
#  endif
                       fullname, strerror(errno));
            ret = INCORRECT;
         }
         else
         {
#  ifdef HAVE_STATX
            if (stat_buf.stx_size == 0)
#  else
            if (stat_buf.st_size == 0)
#  endif
            {
               trans_log(DEBUG_SIGN, __FILE__, __LINE__, "remove_from_knownhosts", NULL,
                         _("File `%s' is empty."), fullname);
               ret = INCORRECT;
            }
            else
            {
               char *data;

               if ((data = mmap(NULL,
#  ifdef HAVE_STATX
                                stat_buf.stx_size, (PROT_READ | PROT_WRITE),
#  else
                                stat_buf.st_size, (PROT_READ | PROT_WRITE),
#  endif
                                MAP_SHARED, fd, 0)) == (caddr_t) -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("mmap() error : %s"), strerror(errno));
                  ret = INCORRECT;
               }
               else
               {
                  size_t remove_size;
                  char   *ptr;

                  remove_size = 0;
                  ptr = data;
                  ret = RETRY;
                  while ((remove_size == 0) && (ret != INCORRECT))
                  {
                     if ((ptr = posi(ptr, hostname)) == NULL)
                     {
                        trans_log(DEBUG_SIGN, __FILE__, __LINE__, "remove_from_knownhosts", NULL,
                                  _("Failed to locate `%s' in `%s'"),
                                  hostname, fullname);
                        ret = INCORRECT;
                     }
                     else
                     {
                        char *p_tmp = ptr;

                        ptr = ptr - strlen(hostname) - 1;
                        if ((ptr == data) || (*(ptr - 1) == '\n') ||
                            (*(ptr - 1) == ','))
                        {
                           char *p_start;

                           /* We might be looking at the IP number that is */
                           /* following the real hostname.                 */
                           if ((ptr != data) && (*(ptr - 1) == ','))
                           {
                              ptr -= 2;
                              while ((ptr > data) && (*ptr != '\n'))
                              {
                                 ptr--;
                              }
                           }
                           p_start = ptr;

#  ifdef HAVE_STATX
                           while ((ptr < (data + stat_buf.stx_size)) &&
                                  (*ptr != '\n'))
#  else
                           while ((ptr < (data + stat_buf.st_size)) &&
                                  (*ptr != '\n'))
#  endif
                           {
                              ptr++;
                           }
                           if (*ptr == '\n')
                           {
                              ptr++;
#  ifdef HAVE_STATX
                              if (ptr < (data + stat_buf.stx_size))
#  else
                              if (ptr < (data + stat_buf.st_size))
#  endif
                              {
#  ifdef HAVE_STATX
                                 (void)memmove(p_start, ptr,
                                               ((data + stat_buf.stx_size) - ptr));
                                 if (msync(data, stat_buf.stx_size, MS_SYNC) == -1)
#  else
                                 (void)memmove(p_start, ptr,
                                               ((data + stat_buf.st_size) - ptr));
                                 if (msync(data, stat_buf.st_size, MS_SYNC) == -1)
#  endif
                                 {
                                    system_log(WARN_SIGN, __FILE__, __LINE__,
                                               _("msync() error : %s"),
                                               strerror(errno));
                                 }
                              }
                           }
                           remove_size = ptr - p_start;
                        }
                        else
                        {
                           ptr = p_tmp;
                        }
                     }
                  }
#  ifdef HAVE_STATX
                  if (munmap(data, stat_buf.stx_size) == -1)
#  else
                  if (munmap(data, stat_buf.st_size) == -1)
#  endif
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                _("munmap() error : %s"), strerror(errno));
                  }
                  if (remove_size > 0)
                  {
#  ifdef HAVE_STATX
                     if (ftruncate(fd, (stat_buf.stx_size - remove_size)) == -1)
#  else
                     if (ftruncate(fd, (stat_buf.st_size - remove_size)) == -1)
#  endif
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   _("ftruncate() error : %s"), strerror(errno));
                        ret = INCORRECT;
                     }
                  }
                  else
                  {
                     ret = INCORRECT;
                  }
               }
            } /* (stat_buf.st_size > 0) */
         }
         if (close(fd) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("Failed to close() `%s' : %s"),
                       fullname, strerror(errno));
         }
      }
   }
   else
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("getpwuid() error : %s"), strerror(errno));
      ret = INCORRECT;
   }

   return(ret);
}
# endif
#endif


/*-------------------------- get_passwd_reply() -------------------------*/
static int
get_passwd_reply(int fd)
{
   int            status;
   fd_set         rset;
   struct timeval timeout;

#ifdef WITH_TRACE
read_passwd_reply:
#endif
   FD_ZERO(&rset);
   FD_SET(fd, &rset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = transfer_timeout;

   status = select(fd + 1, &rset, NULL, NULL, &timeout);
  
   if (status == 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "get_passwd_reply", NULL,
                _("Timeout while waiting for password responce."));
      timeout_flag = ON;
      status = INCORRECT;
      msg_str[0] = '\0';
   }
   else if (FD_ISSET(fd, &rset))
        {
           if ((status = read(fd, msg_str, MAX_RET_MSG_LENGTH)) < 0)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "get_passwd_reply", NULL,
                        _("read() error : %s"), strerror(errno));
              status = INCORRECT;
              msg_str[0] = '\0';
           }
           else
           {
              if ((status == 1) && (msg_str[0] == '\n'))
              {
#ifdef WITH_TRACE
                 trace_log(NULL, 0, BIN_CMD_R_TRACE, msg_str, status, NULL);
                 goto read_passwd_reply;
#else
                 int flags;

                 /*
                  * There migth be more information, like login failed.
                  * So let's try to continue reading, but without blocking.
                  */
                 if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                              "Failed to get flag via fcntl() : %s", strerror(errno));
                    return(INCORRECT);
                 }
                 flags |= O_NONBLOCK;
                 if (fcntl(fd, F_SETFL, flags) == -1)
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                              "Failed to set O_NONBLOCK flag via fcntl() : %s",
                              strerror(errno));
                    return(INCORRECT);
                 }
                 if ((status = read(fd, msg_str, MAX_RET_MSG_LENGTH)) == -1)
                 {
                    if (errno == EAGAIN)
                    {
                       /* OK. Nothing to read. So let's assume everything */
                       /* is good.                                        */
                       status = SUCCESS;
                       msg_str[0] = '\0';
                    }
                    if (ssh_child_up() == NO)
                    {
                       status = INCORRECT;
                    }
                 }
                 else
                 {
                    if (status > 0)
                    {
                       msg_str[status] = '\0';
                       if (lposi(msg_str, "Authenticated", 13) != NULL)
                       {
                          status = SUCCESS;
                       }
                       else
                       {
                          char *ptr = msg_str;

                          status = INCORRECT;
                          if ((ptr = lposi(ptr, "assword:", 8)) != NULL)
                          {
                             /* Let's remove the next line showing   */
                             /* the password prompt. It confuses the */
                             /* user since we show msg_str in the    */
                             /* error message later.                 */
                             ptr -= 9;
                             while ((ptr > msg_str) && (*ptr != '\n'))
                             {
                                ptr--;
                             }
                             if (*ptr == '\n')
                             {
                                if (*(ptr - 1) == '\r')
                                {
                                   *(ptr - 1) = '\0';
                                }
                                else
                                {
                                   *ptr = '\0';
                                }
                             }
                          }
                       }
                    }
                    else
                    {
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                                 "Hmmm, read() returned %d", status);
                       status = INCORRECT;
                       msg_str[0] = '\0';
                    }
                 }
                 if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                              "Failed to get flag via fcntl() : %s", strerror(errno));
                    return(INCORRECT);
                 }
                 flags &= ~O_NONBLOCK;
                 if (fcntl(fd, F_SETFL, flags) == -1)
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                              "Failed to unset O_NONBLOCK flag via fcntl() : %s",
                              strerror(errno));
                    return(INCORRECT);
                 }
#endif
              }
#ifdef WITH_TRACE
              else
              {
                 char *ptr;

                 trace_log(NULL, 0, CRLF_R_TRACE, msg_str, status, NULL);
                 while ((status > 8) && (strncmp(msg_str, "debug1: ", 8) == 0))
                 {
                    ptr = msg_str + 8;
                    while ((*ptr != '\r') && (*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while ((*ptr == '\r') || (*ptr == '\n'))
                    {
                       ptr++;
                    }
                    status = status - (ptr - msg_str);
                    if (status >= (MAX_RET_MSG_LENGTH - 1))
                    {
                       trans_log(WARN_SIGN, __FILE__, __LINE__, "ssh_login", NULL,
                                 "msg_str buffer to small. Wanted to copy %d bytes.",
                                 status);
                       status = MAX_RET_MSG_LENGTH - 1;
                    }
                    (void)memmove(msg_str, ptr, status);
                 }
                 if (status > 0)
                 {
                    msg_str[status] = '\0';
                 }
              }
#endif
              if (status > 0)
              {
                  if (lposi(msg_str, "Authenticated", 13) != NULL)
                  {
                     status = SUCCESS;
                  }
                  else
                  {
                     status = INCORRECT;
                  }
              }
           }
        }
        else
        {
           msg_str[0] = '\0';
           trans_log(ERROR_SIGN, __FILE__, __LINE__, "get_passwd_reply", NULL,
                     _("select() error : %s"), strerror(errno));
           status = INCORRECT;
        }

   return(status);
}


/*----------------------------- ptym_open() -----------------------------*/
/*                              -----------                              */
/* This code was taken from Advanced Programming in the Unix             */
/* environment by W.Richard Stevens.                                     */
/*-----------------------------------------------------------------------*/
static int
ptym_open(char *pts_name)
{
   int  fd;
#ifdef HAVE_OPENPTY
   int dummy_fd;

   fd = dummy_fd = 0;
   if (openpty(&fd, &dummy_fd, pts_name, NULL, NULL) == -1)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "ptym_open", NULL,
                _("openpty() error : %s"), strerror(errno));
      if (fd != 0)
      {
         (void)close(fd);
      }
      if (dummy_fd != 0)
      {
         (void)close(dummy_fd);
      }
   }
   else
   {
      (void)close(dummy_fd);
   }
   return(fd);
#else
# ifdef HAVE__GETPTY
   char *tempstr;

   if ((tempstr = _getpty(&fd, O_RDWR, 0600, 0)) == NULL)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "ptym_open", NULL,
                _("_getpty() error : %s"), strerror(errno));
      return(-1);
   }
   (void)strcpy(pts_name, tempstr);
   return(fd);
# else
#  ifdef HAVE_GETPT
   char *tempstr;

   if ((fd = getpt()) == -1)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "ptym_open", NULL,
                _("getpt() error : %s"), strerror(errno));
      return(-1);
   }
   if ((tempstr = ptsname(fd)) == NULL)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "ptym_open", NULL,
                _("ptsname() error : %s"), strerror(errno));
      return(-1);
   }
   (void)strcpy(pts_name, tempstr);
   return(fd);
#  else
#   ifdef SYSV
   char *tempstr;

   strcpy(pts_name, "/dev/ptmx");
   if ((fd = open(pts_name, O_RDWR)) == -1)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "ptym_open", NULL,
                _("Failed to open() `%s' error : %s"),
                pts_name, strerror(errno));
      return(-1);
   }

   if (grantpt(fd) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "ptym_open", NULL,
                _("grantpt() error : %s"), strerror(errno));
      (void)close(fd);
      return(-1);
   }

   if (unlockpt(fd) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "ptym_open", NULL,
                _("unlockpt() error : %s"), strerror(errno));
      (void)close(fd);
      return(-1);
   }

   if ((tempstr = ptsname(fd)) == NULL)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "ptym_open", NULL,
                _("tempstr() error : %s"), strerror(errno));
      (void)close(fd);
      return(-1);
   }

   (void)strcpy(pts_name, tempstr);
   return(fd);
#   else
   char *pos1, *pos2;

   (void)strcpy(pts_name, "/dev/ptyXY");
   for (pos1 = "pqrstuvwxyzPQRST"; *pos1 != '\0'; pos1++)
   {
      pts_name[8] = *pos1;
      for (pos2 = "0123456789abcdef"; *pos2 != '\0'; pos2++)
      {
         pts_name[9] = *pos2;
         if ((fd = open(pts_name, O_RDWR)) == -1)
         {
            if (errno == ENOENT)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "ptym_open", NULL,
                         _("Failed to open() `%s' error : %s"),
                         pts_name, strerror(errno));
               return(-1);
            }
            else
            {
               continue;
            }
         }
         pts_name[5] = 't';
         return(fd);
      }
   }
   return(-1);
#   endif
#  endif
# endif
#endif
}


/*----------------------------- ptys_open() -----------------------------*/
/*                              -----------                              */
/* This code was taken from Advanced Programming in the Unix             */
/* environment by W.Richard Stevens.                                     */
/*-----------------------------------------------------------------------*/
static int
ptys_open(char *pts_name)
{
   int fds;

   if ((fds = open(pts_name, O_RDWR)) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "ptys_open", NULL,
                _("Failed to open() `%s' error : %s"),
                pts_name, strerror(errno));
      return(-1);
   }

#if defined(SYSV)
   if (ioctl(fds, I_PUSH, "ptem") < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "ptys_open", NULL,
                _("ioctl() error : %s"), strerror(errno));
      (void)close(fds);
      return(-1);
   }

   if (ioctl(fds, I_PUSH, "ldterm") < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "ptys_open", NULL,
                _("ioctl() error : %s"), strerror(errno));
      (void)close(fds);
      return(-1);
   }

   if (ioctl(fds, I_PUSH, "ttcompat") < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "ptys_open", NULL,
                _("ioctl() error : %s"), strerror(errno));
      (void)close(fds);
      return(-1);
   }
#endif

#if !defined(SYSV) && defined(TIOCSCTTY) && !defined(CIBAUD) && !defined(HAVE__GETPTY)
   if (ioctl(fds, TIOCSCTTY, (char *) 0) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "ptys_open", NULL,
                _("ioctl() error : %s"), strerror(errno));
      (void)close(fds);
      return(-1);
   }
#endif

   return(fds);
}


/*------------------------------ tty_raw() ------------------------------*/
/*                              -----------                              */
/* This code was taken from Advanced Programming in the Unix             */
/* environment by W.Richard Stevens. The modification where taken from   */
/* gFTP software package by Brian Masney.                                */
/*-----------------------------------------------------------------------*/
static int
tty_raw(int fd)
{
   struct termios buf;

   if (tcgetattr(fd, &buf) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "tty_raw", NULL,
                _("tcgetattr() error : %s"), strerror(errno));
      return(-1);
   }
   buf.c_iflag |= IGNPAR;
   buf.c_iflag &= ~(ICRNL | INPCK | ISTRIP | IXON | IGNCR | IXANY | IXOFF | INLCR);
   buf.c_lflag &= ~(ECHO | ICANON | ISIG | ECHOE | ECHOK | ECHONL);
#ifdef IEXTEN
   buf.c_lflag &= ~(IEXTEN);
#endif
   buf.c_cflag &= ~(CSIZE | PARENB);
   buf.c_cflag |= CS8;
   buf.c_oflag &= ~(OPOST);  /* Output processing off. */
   buf.c_cc[VMIN] = 1;       /* Case B: 1 byte at a time, no timer. */
   buf.c_cc[VTIME] = 0;

   if (tcsetattr(fd, TCSANOW, &buf) < 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "tty_raw", NULL,
                _("tcsetattr() error : %s"), strerror(errno));
      return(-1);
   }
   return(0);
}
