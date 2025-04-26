/*
 *  scpcmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2025 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   scpcmd - commands to send files via the SCP protocol
 **
 ** SYNOPSIS
 **   int  scp_connect(char *hostname, int port, char *user,
 **                    unsigned char ssh_protocol, int ssh_options,
 **                    char *fingerprint, char *passwd, char *dir)
 **   int  scp_open_file(char *filename, off_t size, mode_t mode)
 **   int  scp_close_file(void)
 **   int  scp_write(char *block, int size)
 **   void scp_quit(void)
 **
 ** DESCRIPTION
 **   scpcmd provides a set of commands to communicate with a SSH
 **   (Secure Shell) server via pipes. The following functions are
 **   required to send a file:
 **      scp_connect()    - build a connection to the SSH server
 **      scp_open_file()  - open a file
 **      scp_close_file() - close a file
 **      scp_write()      - write data to the pipe
 **      scp_quit()       - disconnect from the SSH server
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
 **   22.04.2001 H.Kiehl        Created
 **   30.09.2001 H.Kiehl        Added functions scp1_cmd_connect(),
 **                             scp1_chmod() and scp1_move().
 **   10.03.2003 F.Olivie (Alf) Support for SCP2.
 **                             Added timeouts and identity file handling.
 **   10.12.2003 H.Kiehl        Remove everything with ctrl connection,
 **                             since it does not work on all systems.
 **                             The SCP protocol was not designed for this.
 **   25.12.2003 H.Kiehl        Added tracing.
 **   01.01.2006 H.Kiehl        Move ssh_cmd() and pty code to common.c.
 **   13.09.2014 H.Kiehl        Added simulation mode.
 **
 */
DESCR__E_M3


#include <stdio.h>
#include <string.h>           /* strerror()                              */
#include <sys/types.h>        /* fd_set                                  */
#include <sys/time.h>         /* struct timeval                          */
#include <sys/wait.h>         /* waitpid()                               */
#include <sys/stat.h>         /* S_ISUID, S_ISGID, etc                   */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <setjmp.h>           /* sigsetjmp(), siglongjmp()               */
#include <signal.h>           /* signal(), kill()                        */
#include <unistd.h>           /* select(), write(), close()              */
#include <errno.h>
#include "ssh_commondefs.h"
#include "scpdefs.h"
#include "fddefs.h"           /* trans_log()                             */


/* External global variables. */
extern int        simulation_mode,
                  timeout_flag;
extern char       msg_str[];
extern long       transfer_timeout;
extern pid_t      data_pid;
extern char       tr_hostname[];

/* Local global variables. */
static int        data_fd = -1;
static sigjmp_buf env_alrm;

/* Local function prototypes. */
static void       sig_handler(int);


/*########################### scp_connect() #############################*/
int
scp_connect(char          *hostname,
            int           port,
            unsigned char ssh_protocol,
            int           ssh_options,
            char          *user,
#ifdef WITH_SSH_FINGERPRINT
            char          *fingerprint,
#endif
            char          *passwd,
            char          *dir)
{
#ifdef WITH_SSH_FINGERPRINT
# ifdef WITH_REMOVE_FROM_KNOWNHOSTS
   int  retries = 0;
# endif
#endif
   int  status;
   char cmd[MAX_PATH_LENGTH];

   (void)snprintf(cmd, MAX_PATH_LENGTH,
                  "scp -t %s", (dir[0] == '\0') ? "." : dir);

   if (simulation_mode == YES)
   {
      if ((data_fd = open("/dev/null", O_RDWR)) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "scp_connect", "Simulated scp_connect()",
                    _("Failed to open() /dev/null : %s"), strerror(errno));
         return(INCORRECT);
      }
      else
      {
#ifdef WITH_TRACE
         int length;

         length = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                           _("Simulated SCP connect to %s (port=%d)"),
                           hostname, port);
         trace_log(NULL, 0, C_TRACE, msg_str, length, NULL);
#else
         (void)snprintf(msg_str, MAX_RET_MSG_LENGTH,
                        _("Simulated SCP connect to %s (port=%d)"),
                        hostname, port);
#endif
         return(SUCCESS);
      }
   }

#ifdef WITH_SSH_FINGERPRINT
# ifdef WITH_REMOVE_FROM_KNOWNHOSTS
   do
   {
# endif
#endif
      if ((status = ssh_exec(hostname, port, ssh_protocol, ssh_options,
#ifndef FORCE_SFTP_NOOP
                             NO,
#endif
                             user, passwd, cmd, NULL, &data_fd)) == SUCCESS)
      {
#ifdef WITH_SSH_FINGERPRINT
         status = ssh_login(data_fd, passwd, 0, fingerprint);
#else
         status = ssh_login(data_fd, passwd, 0);
#endif
      }
#ifdef WITH_SSH_FINGERPRINT
# ifdef WITH_REMOVE_FROM_KNOWNHOSTS
      retries++;
      if (status == RETRY)
      {
         scp_quit();
      }
   } while ((status == RETRY) && (retries < 5));
# endif
#endif

   return(status);
}


/*########################## scp_open_file() ############################*/
int
scp_open_file(char *filename, off_t size, mode_t mode)
{
   int    status;
   size_t length;
   char   cmd[MAX_PATH_LENGTH];

   length = snprintf(cmd, MAX_PATH_LENGTH,
#if SIZEOF_OFF_T == 4
                     "C%04o %ld %s\n",
#else
                     "C%04o %lld %s\n",
#endif
                     (mode & (S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO)),
                     (pri_off_t)size, filename);
   if ((status = pipe_write(data_fd, cmd, length)) != length)
   {
      if (errno != 0)
      {
         cmd[length - 1] = '\0';
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "scp_open_file", NULL,
                   _("Failed to pipe_write() `%s' to pipe [%d] : %s"),
                   cmd, status, strerror(errno));
      }
      status = INCORRECT;
   }
   else
   {
      if (simulation_mode == YES)
      {
         status = SUCCESS;
      }
      else
      {
         status = get_ssh_reply(data_fd, YES);
      }
   }
   return(status);
}


/*########################### scp_close_file() ##########################*/
int
scp_close_file(void)
{
   int status;

   if ((status = pipe_write(data_fd, "\0", 1)) != 1)
   {
      if (errno != 0)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "scp_close_file", NULL,
                   _("Failed to pipe_write() [close file] to pipe [%d] : %s"),
                   status, strerror(errno));
      }
      status = INCORRECT;
   }
   else
   {
      if (simulation_mode == YES)
      {
         status = SUCCESS;
      }
      else
      {
         status = get_ssh_reply(data_fd, YES);
      }
   }
   return(status);
}


/*############################## scp_write() ############################*/
int
scp_write(char *block, int size)
{
   int            status;
   fd_set         wset;
   struct timeval timeout;

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
           int tmp_errno;

           /* In some cases, the write system call hangs. */
           if (signal(SIGALRM, sig_handler) == SIG_ERR)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "scp_write", NULL,
                        _("Failed to set signal handler : %s"),
                        strerror(errno));
              return(INCORRECT);
           }
           if (sigsetjmp(env_alrm, 1) != 0)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "scp_write", NULL,
                        _("write() timeout (%ld)"), transfer_timeout);
              timeout_flag = ON;
              return(INCORRECT);
           }
           (void)alarm(transfer_timeout);
           status = write(data_fd, block, size);
           tmp_errno = errno;
           (void)alarm(0);

           if (status != size)
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__, "scp_write", NULL,
                        _("write() error (%d) : %s"),
                        status, strerror(tmp_errno));
              return(tmp_errno);
           }
#ifdef WITH_TRACE
           trace_log(NULL, 0, BIN_W_TRACE, block, size, NULL);
#endif
        }
   else if (status < 0)
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, "scp_write", NULL,
                     _("select() error : %s"), strerror(errno));
           return(INCORRECT);
        }
        else
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, "scp_write", NULL,
                     _("Unknown condition."));
           return(INCORRECT);
        }
   
   return(SUCCESS);
}


/*############################## scp_quit() #############################*/
void
scp_quit(void)
{
   /* Close pipe for read/write data connection. */
   if (data_fd != -1)
   {
      if (close(data_fd) == -1)
      {
         trans_log(WARN_SIGN, __FILE__, __LINE__, "scp_quit", NULL,
                   _("Failed to close() write pipe to ssh process : %s"),
                   strerror(errno));
      }
      data_fd = -1;
   }

   /* Remove ssh process for writing data. */
   if (data_pid > 0)
   {
      int   loop_counter,
            max_waitpid_loops;
      pid_t return_pid;

      loop_counter = 0;
      max_waitpid_loops = (transfer_timeout / 2) * 10;
      while (((return_pid = waitpid(data_pid, NULL, WNOHANG)) != data_pid) &&
             (return_pid != -1) &&
             (loop_counter < max_waitpid_loops))
      {
         my_usleep(100000L);
         loop_counter++;
      }
      if ((return_pid == -1) || (loop_counter >= max_waitpid_loops))
      {
         msg_str[0] = '\0';
         if (return_pid == -1)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, "scp_quit", NULL,
                      _("Failed to catch zombie of data ssh process : %s"),
                      strerror(errno));
         }
         if (data_pid > 0)
         {
            if (kill(data_pid, SIGKILL) == -1)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "scp_quit", NULL,
#if SIZEOF_PID_T == 4
                         _("Failed to kill() data ssh process %d : %s"),
#else
                         _("Failed to kill() data ssh process %lld : %s"),
#endif
                         (pri_pid_t)data_pid, strerror(errno));
            }
            else
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, "scp_quit", NULL,
#if SIZEOF_PID_T == 4
                         _("Killing hanging data ssh process %d."),
#else
                         _("Killing hanging data ssh process %lld."),
#endif
                         (pri_pid_t)data_pid);

               my_usleep(100000L);
               (void)waitpid(data_pid, NULL, WNOHANG);
            }
         }
         else
         {
            trans_log(DEBUG_SIGN, __FILE__, __LINE__, "scp_quit", NULL,
#if SIZEOF_PID_T == 4
                      _("Hmm, pid is %d!!!"), (pri_pid_t)data_pid);
#else
                      _("Hmm, pid is %lld!!!"), (pri_pid_t)data_pid);
#endif
         }
      }
      data_pid = -1;
   }
   else if (simulation_mode == YES)
        {
           /* Close pipe for read/write data connection. */
           if (data_fd != -1)
           {
              if (close(data_fd) == -1)
              {
                 trans_log(WARN_SIGN, __FILE__, __LINE__, "scp_quit", NULL,
                           _("Failed to close() write pipe to ssh process : %s"),
                           strerror(errno));
              }
              data_fd = -1;
           }
        }

   return;
}


/*++++++++++++++++++++++++++++ sig_handler() ++++++++++++++++++++++++++++*/
static void
sig_handler(int signo)
{
   siglongjmp(env_alrm, 1);
}
