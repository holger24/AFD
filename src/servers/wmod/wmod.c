/*
 *  wmod.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 - 2021 Holger Kiehl <Holger.Kiehl@dwd.de>
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

DESCR__S_M1
/*
 ** NAME
 **   wmod - WMO daemon for receiving files via WMO socket procedure
 **
 ** SYNOPSIS
 **   wmod [--version][-w <AFD working directory>] [Options] <alias name> <port>
 **
 **             -A                    Acknowledge each message received.
 **             -a <hostname|IP>      Hostnames|IP's that may connect.
 **             -c                    Check sequence number.
 **             -d <time in seconds>  Disconnect after given time.
 **             -r <filename>         How to request missing messages.
 **
 ** DESCRIPTION
 **   This is a small tcp server that listens at the specified port
 **   to receive data via the WMO socket procedure.
 **
 ** RETURN VALUES
 **   Will exit with INCORRECT when some system call failed.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   26.08.2005 H.Kiehl Created
 */
DESCR__E_M1

#include <stdio.h>            /* fprintf()                               */
#include <string.h>           /* memset()                                */
#include <stdlib.h>           /* atoi(), getenv()                        */
#include <ctype.h>            /* isdigit()                               */
#include <time.h>             /* clock_t                                 */
#include <signal.h>           /* signal()                                */
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>         /* waitpid()                               */
#include <netinet/in.h>
#include <arpa/inet.h>        /* inet_ntoa()                             */
#include <unistd.h>           /* close(), select(), sysconf()            */
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <netdb.h>
#include <errno.h>
#include "commondefs.h"
#include "version.h"

/* Global variables. */
int                        fsa_fd = -1,
                           fsa_id,
                           fsa_pos,
                           no_of_hosts = 0,
                           sys_log_fd = STDERR_FILENO,
                           timeout_flag = OFF,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                           transfer_log_readfd,
                           trans_db_log_readfd,
#endif
                           transfer_log_fd = STDERR_FILENO,
                           trans_db_log_fd = STDERR_FILENO;
char                       alias_name[MAX_HOSTNAME_LENGTH + 1],
                           *p_work_dir;
off_t                      fsa_size;
struct proclist            pl[MAX_NO_PARALLEL_JOBS];
struct filetransfer_status *fsa = NULL;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local global variables. */
static int                 in_child = NO,
                           new_sockfd,
                           sockfd;

/* Local function prototypes. */
static void                sig_bus(int),
                           sig_exit(int),
                           sig_segv(int),
                           usage(char *),
                           wmod_exit(void),
                           zombie_check(void);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int                acknowledge,
                      chs,
                      number_of_trusted_hosts,
                      on = 1,
                      port,
                      status;
   pid_t              current_pid;
   time_t             disconnect;
   my_socklen_t       peer_addrlen;
   char               **trusted_host,
                      *ptr,
                      reply[256],
                      *reqfile,
                      *request,
                      work_dir[MAX_PATH_LENGTH];
   fd_set             rset;
   struct servent     *p_service;
   struct protoent    *p_protocol;
   struct sockaddr_in data,
                      peer_address;
   struct timeval     timeout;

   CHECK_FOR_VERSION(argc, argv);

   if (get_arg(&argc, argv, "-A", NULL, 0) == SUCCESS)
   {
      acknowledge = YES;
   }
   else
   {
      acknowledge = NO;
   }
   if (get_arg(&argc, argv, "-a", work_dir, MAX_PATH_LENGTH) != SUCCESS)
   {
      int length = 0,
          max_length = 0;

      number_of_trusted_hosts = 0;
      ptr = work_dir;
      while (*ptr != '\0')
      {
         if (*ptr == ',')
         {
            if (max_length < length)
            {
               max_length = length;
            }
            length = 0;
            number_of_trusted_hosts++;
         }
         else
         {
            length++;
         }
         ptr++;
      }
      if (length > 0)
      {
         if (max_length < length)
         {
            max_length = length;
         }
         number_of_trusted_hosts++;
      }
      if (max_length > 0)
      {
         int i, j;

         max_length++;
         RT_ARRAY(trusted_host, number_of_trusted_hosts, max_length, char);
         ptr = work_dir;
         i = j = 0;
         while (*ptr != '\0')
         {
            if (*ptr == ',')
            {
               trusted_host[i][j] = '\0';
               i++;
               j = 0;
            }
            else
            {
               trusted_host[i][j] = *ptr;
               j++;
            }
            ptr++;
         }
      }
   }
   else
   {
      trusted_host = NULL;
      number_of_trusted_hosts = 0;
   }
   if (get_arg(&argc, argv, "-c", NULL, 0) == SUCCESS)
   {
      chs = YES;
   }
   else
   {
      chs = NO;
   }
   if (get_arg(&argc, argv, "-d", work_dir, MAX_PATH_LENGTH) == SUCCESS)
   {
      disconnect = atoi(work_dir);
   }
   else
   {
      disconnect = -1;
   }
   if (get_arg(&argc, argv, "-r", work_dir, MAX_PATH_LENGTH) == SUCCESS)
   {
      size_t length;

      length = strlen(work_dir) + 1;
      if ((reqfile = malloc(length)) == NULL)
      {
         (void)fprintf(stderr, "Failed to malloc() %d bytes : %s\n",
                       length, strerror(errno));
         exit(ALLOC_ERROR);
      }
      (void)memcpy(reqfile, work_dir, length);
   }
   else
   {
      reqfile = NULL;
      request = NULL;
   }

   /* Initialize variables */
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   else
   {
      if (reqfile != NULL)
      {
         if ((request = read_request(work_dir, reqfile)) == NULL)
         {
            exit(1);
         }
         free(reqfile);
         reqfile = NULL;
      }
   }
   p_work_dir = work_dir;

   if (argc != 3)
   {
      usage(argv[0]);
      exit(INCORRECT);
   }

   if (my_strncpy(alias_name, argv[1], MAX_HOSTNAME_LENGTH + 1) != SUCCESS)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Alias name name `%s' to long, truncated to %d bytes.",
                 argv[1], MAX_HOSTNAME_LENGTH);
   }
   port = atoi(argv[2]);

   /* Do some cleanups when we exit */
   if (atexit(wmod_exit) != 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not register exit handler : %s", strerror(errno));
      exit(INCORRECT);
   }
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGPIPE, SIG_IGN) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not set signal handlers : %s", strerror(errno));
      exit(INCORRECT);
   }

   if (fsa_attach(WMOD) != SUCCESS)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__, "Could not attach to FSA!");
      exit(INCORRECT);
   }
   if ((fsa_pos = get_host_position(fsa, alias_name, no_of_hosts)) == INCORRECT)
   {
      (void)fprintf(stderr, "Failed to locate %s in FSA.\n", alias_name);
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to locate %s in FSA.", alias_name);
      exit(INCORRECT);
   }
   else
   {
      char buffer[MAX_PATH_LENGTH];

      (void)strcpy(buffer, p_work_dir);
      (void)strcat(buffer, FIFO_DIR);
      (void)strcat(buffer, TRANSFER_LOG_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(buffer, &transfer_log_readfd,
                       &transfer_log_fd) == -1)
#else
      if ((transfer_log_fd = open(buffer, O_RDWR)) == -1)
#endif
      {
         if (errno == ENOENT)
         {
            if ((make_fifo(buffer) == SUCCESS) &&
#ifdef WITHOUT_FIFO_RW_SUPPORT
                (open_fifo_rw(buffer, &transfer_log_readfd,
                              &transfer_log_fd) == -1))
#else
                ((transfer_log_fd = open(buffer, O_RDWR)) == -1))
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not open fifo %s : %s",
                          TRANSFER_LOG_FIFO, strerror(errno));
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not open fifo %s : %s",
                       TRANSFER_LOG_FIFO, strerror(errno));
         }
      }
   }

   (void)memset((struct sockaddr *) &data, 0, sizeof(data));
   data.sin_family = AF_INET;
   data.sin_addr.s_addr = INADDR_ANY;
   peer_addrlen = sizeof(peer_address);

   do
   {
      if ((p_service = getservbyport(port, "tcp")))
      {
         data.sin_port =  htons(ntohs((u_short)p_service->s_port));
      }
      else if ((data.sin_port = htons((u_short)port)) == 0)
           {
              system_log(FATAL_SIGN, __FILE__, __LINE__,
                         "Failed to copy service to structure.");
              exit(INCORRECT);
           }

      if ((p_protocol = getprotobyname("tcp")) == 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to get protocol tcp : %s", strerror(errno));
         exit(INCORRECT);
      }

      if ((sockfd = socket(AF_INET, SOCK_STREAM, p_protocol->p_proto)) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not create socket : %s", strerror(errno));
         exit(INCORRECT);
      }

      if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "setsockopt() error : %s", strerror(errno));
         (void)close(sockfd);
         exit(INCORRECT);
      }

      if ((status = bind(sockfd, (struct sockaddr *) &data, sizeof(data))) == -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "bind() error : %s", strerror(errno));
         (void)close(sockfd);
         exit(INCORRECT);
      }
   } while (status == -1);

   current_pid = getpid();
   system_log(INFO_SIGN, NULL, 0, "Starting %s at port %d (%s)",
              WMOD, port, PACKAGE_VERSION);

   if (listen(sockfd, 5) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "listen() error : %s", strerror(errno));
      (void)close(sockfd);
      exit(INCORRECT);
   }
   FD_ZERO(&rset);

   for (;;)
   {
      /* Initialise descriptor set */
      FD_SET(sockfd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = 5L;

      if (select(sockfd + 1, &rset, NULL, NULL, &timeout) < 0)
      {
         (void)close(sockfd);
         if (errno != EBADF)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "select() error : %s", strerror(errno));
         }
         exit(INCORRECT);
      }
      check_fsa_pos();

      if (FD_ISSET(sockfd, &rset))
      {
         if ((new_sockfd = accept(sockfd, (struct sockaddr *)&peer_address, &peer_addrlen)) < 0)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "accept() error : %s", strerror(errno));
            (void)close(sockfd);
            exit(INCORRECT);
         }
         if (number_of_trusted_hosts > 0)
         {
            int  gotcha = NO,
                 i;
            char remote_ip_str[16];

            (void)strcpy(remote_ip_str, inet_ntoa(peer_address.sin_addr));
            for (i = 0; i < number_of_trusted_hosts; i++)
            {
               if (pmatch(trusted_host[i], remote_ip_str, NULL) == 0)
               {
                  gotcha = YES;
                  break;
               }
            }
            if (gotcha == NO)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "WMOD: Illegal access from %s", remote_ip_str);
               (void)close(new_sockfd);
               continue;
            }
         }

         if (fsa[fsa_pos].active_transfers >= fsa[fsa_pos].allowed_transfers)
         {
            int length;

            length = sprintf(reply,
                             "421 Service not available. There are currently to many users (%d) connected.\r\n",
                             fsa[fsa_pos].active_transfers);
            (void)write(new_sockfd, reply, length);
            (void)close(new_sockfd);
         }
         else
         {
            int pos;

            check_fsa_pos();
            if ((pos = get_free_connection(fsa_pos, current_pid)) < 0)
            {
               int length;

               length = sprintf(reply, "421 Service not available.\r\n");
               (void)write(new_sockfd, reply, length);
               (void)close(new_sockfd);
            }
            else
            {
               trans_log(INFO_SIGN, NULL, 0, pl[pos].job_pos,
                         "WMOD: Connection from %s",
                         inet_ntoa(peer_address.sin_addr));
               switch (pl[pos].pid = fork())
               {
                  case -1 : /* Could not generate process */
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "fork() error : %s", strerror(errno));
                     break;

                  case 0  : /* Child process to serve user. */
                     in_child = YES;
                     (void)close(sockfd);
                     handle_wmo_request(new_sockfd, pos, acknowledge,
                                        chs, disconnect, request);
                     exit(0);

                  default : /* Parent process */
                     (void)close(new_sockfd);
                     break;
               }
            }
         }
      } /* if (FD_ISSET(sockfd, &rset)) */

      zombie_check();
   } /* for (;;) */

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{
   (void)fprintf(stderr,
                 "Usage: %s [--version][-w <AFD working directory>] [Options] <alias name> <port>\n",
                 progname);
   (void)fprintf(stderr, "OPTIONS                  DESCRIPTION\n");
   (void)fprintf(stderr, "  --version            - Show current version\n");
   (void)fprintf(stderr, "  -A                   - Acknowledge each message received.\n");
   (void)fprintf(stderr, "  -a <hostname|IP>     - Hostnames|IP's that may connect.\n");
   (void)fprintf(stderr, "  -c                   - Check sequence number.\n");
   (void)fprintf(stderr, "  -d <time in seconds> - Disconnect after given time.\n");
   (void)fprintf(stderr, "  -r <filename>        - How to request missing messages.\n");

   return;
}


/*++++++++++++++++++++++++++++++ wmod_exit() ++++++++++++++++++++++++++++*/
static void
wmod_exit(void)
{
   if (in_child == NO)
   {
      int i;

      /* Kill all child process */
      for (i = 0; i < MAX_NO_PARALLEL_JOBS; i++)
      {
         if (pl[i].pid > 0)
         {
            if (kill(pl[i].pid, SIGINT) == -1)
            {
               if (errno != ESRCH)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                             "Failed to kill() %d : %s",
#else
                             "Failed to kill() %lld : %s",
#endif
                             (pri_pid_t)pl[i].pid, strerror(errno));
               }
            }
            else
            {
               fsa[fsa_pos].active_transfers--;
               if (fsa[fsa_pos].active_transfers < 0)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Active transfers for FSA position %d < 0!? [%d]",
                             fsa_pos, fsa[fsa_pos].active_transfers);
                  fsa[fsa_pos].active_transfers = 0;
                  fsa[fsa_pos].trl_per_process = fsa[fsa_pos].transfer_rate_limit;
               }
               else
               {
                  if (fsa[fsa_pos].active_transfers > 1)
                  {
                     fsa[fsa_pos].trl_per_process = fsa[fsa_pos].transfer_rate_limit /
                                                    fsa[fsa_pos].active_transfers;
                  }
                  else
                  {
                     fsa[fsa_pos].trl_per_process = fsa[fsa_pos].transfer_rate_limit;
                  }
               }
               fsa[fsa_pos].job_status[pl[i].job_pos].connect_status = DISCONNECT;
            }
         }
      }
      fsa_detach(YES);

      system_log(INFO_SIGN, NULL, 0, "Stopped %s.", WMOD);
   }

   (void)close(sockfd);
   (void)close(new_sockfd);
   (void)close(sys_log_fd);

   return;
}


/*+++++++++++++++++++++++++++++ zombie_check() +++++++++++++++++++++++++*/
/*                              --------------                          */
/* Description : Checks if any process is finished (zombie), if this    */
/*               is the case it is killed with waitpid().               */
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void
zombie_check(void)
{
   int i,
       status;

   /* Did any of the jobs become zombies? */
   for (i = 0; i < MAX_NO_PARALLEL_JOBS; i++)
   {
      if ((pl[i].pid > 0) &&
          (waitpid(pl[i].pid, &status, WNOHANG) > 0))
      {
         if (WIFEXITED(status))
         {
            pl[i].pid = 0;
            fsa[fsa_pos].job_status[pl[i].job_pos].proc_id = -1;
            fsa[fsa_pos].active_transfers--;
            if (fsa[fsa_pos].active_transfers < 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Active transfers for FSA position %d < 0!? [%d]",
                          fsa_pos, fsa[fsa_pos].active_transfers);
               fsa[fsa_pos].active_transfers = 0;
               fsa[fsa_pos].trl_per_process = fsa[fsa_pos].transfer_rate_limit;
            }
            else
            {
               if (fsa[fsa_pos].active_transfers > 1)
               {
                  fsa[fsa_pos].trl_per_process = fsa[fsa_pos].transfer_rate_limit /
                                                 fsa[fsa_pos].active_transfers;
               }
               else
               {
                  fsa[fsa_pos].trl_per_process = fsa[fsa_pos].transfer_rate_limit;
               }
            }
         }
         else  if (WIFSIGNALED(status))
               {
                  /* abnormal termination */
                  pl[i].pid = 0;
                  fsa[fsa_pos].job_status[pl[i].job_pos].proc_id = -1;
                  fsa[fsa_pos].active_transfers--;
                  if (fsa[fsa_pos].active_transfers < 0)
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "Active transfers for FSA position %d < 0!? [%d]",
                                fsa_pos, fsa[fsa_pos].active_transfers);
                     fsa[fsa_pos].active_transfers = 0;
                     fsa[fsa_pos].trl_per_process = fsa[fsa_pos].transfer_rate_limit;
                  }
                  else
                  {
                     if (fsa[fsa_pos].active_transfers > 1)
                     {
                        fsa[fsa_pos].trl_per_process = fsa[fsa_pos].transfer_rate_limit /
                                                       fsa[fsa_pos].active_transfers;
                     }
                     else
                     {
                        fsa[fsa_pos].trl_per_process = fsa[fsa_pos].transfer_rate_limit;
                     }
                  }
               }
          else if (WIFSTOPPED(status))
               {
                  /* Child stopped */;
               }
      }
   }

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   system_log(FATAL_SIGN, __FILE__, __LINE__, "Aaarrrggh! Received SIGSEGV.");
   wmod_exit();

   /* Dump core so we know what happened. */
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   system_log(FATAL_SIGN, __FILE__, __LINE__, "Uuurrrggh! Received SIGBUS.");
   wmod_exit();

   /* Dump core so we know what happened. */
   abort();
}


/*+++++++++++++++++++++++++++++++ sig_exit() ++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   int ret;

   (void)fprintf(stderr,
#if SIZEOF_PID_T == 4
                 "%s terminated by signal %d (%d)\n",
#else
                 "%s terminated by signal %d (%lld)\n",
#endif
                 WMOD, signo, (pri_pid_t)getpid());
   if ((signo == SIGINT) || (signo == SIGTERM))
   {
      ret = SUCCESS;
   }
   else
   {
      ret = INCORRECT;
   }

   exit(ret);
}
