/*
 *  handle_request.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2015 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   handle_request - handles a TCP request
 **
 ** SYNOPSIS
 **   void handle_request(int  cmd_sd,
 **                       int  pos,
 **                       int  trusted_ip_pos,
 **                       char *remote_ip_str)
 **
 ** DESCRIPTION
 **   Handles all request from remote user in a loop. If user is inactive
 **   for ATPD_CMD_TIMEOUT seconds, the connection will be closed.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.10.2015 H.Kiehl Created
 */
DESCR__E_M3

#include <stdio.h>            /* fprintf()                               */
#include <string.h>           /* memset()                                */
#include <stdlib.h>           /* atoi(), strtoul()                       */
#include <time.h>             /* time()                                  */
#include <ctype.h>            /* toupper()                               */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>           /* close()                                 */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <netdb.h>
#include <errno.h>
#include "atpddefs.h"
#include "version.h"

/* Global variables. */
int                        cmd_sd,
                           fra_fd = -1,
                           fra_id,
                           fsa_fd = -1,
                           fsa_id,
                           no_of_dirs,
                           no_of_hosts;
#ifdef HAVE_MMAP
off_t                      fra_size,
                           fsa_size;
#endif
char                       *line_buffer = NULL;
FILE                       *p_data = NULL;
struct filetransfer_status *fsa;
struct fileretrieve_status *fra;


/* External global variables. */
extern char                afd_name[],
                           hostname[],
                           *p_work_dir,
                           *p_work_dir_end;
extern struct afd_status   *p_afd_status;

/*  Local function prototypes. */
static void                report_shutdown(void);


/*########################### handle_request() ##########################*/
void
handle_request(int  sock_sd,
               int  pos,
               int  trusted_ip_pos,
               char *remote_ip_str)
{
   register int   i,
                  j;
   int            nbytes,
                  status;
   time_t         last,
                  last_time_read,
                  now;
   char           cmd[1024];
   fd_set         rset;
   struct timeval timeout;

   cmd_sd = sock_sd;
   if ((p_data = fdopen(cmd_sd, "r+")) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("fdopen() control error : %s"), strerror(errno));
      exit(INCORRECT);
   }

   if (fsa_attach_passive(NO, ATPD) != SUCCESS)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__, _("Failed to attach to FSA."));
      exit(INCORRECT);
   }
   if (fra_attach_passive() != SUCCESS)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__, _("Failed to attach to FRA."));
      exit(INCORRECT);
   }

   status = 0;
   while (p_afd_status->amg_jobs & WRITTING_JID_STRUCT)
   {
      (void)my_usleep(100000L);
      status++;
      if ((status > 1) && ((status % 100) == 0))
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Timeout arrived for waiting for AMG to finish writting to JID structure."));
      }
   }

   if (atexit(report_shutdown) != 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not register exit handler : %s"), strerror(errno));
   }

   /* Give some information to the user where he is and what service */
   /* is offered here.                                               */
   (void)fprintf(p_data, "220 %s AFD server %s (Version %s) ready.\r\n",
                 hostname, afd_name, PACKAGE_VERSION);
   (void)fflush(p_data);

   /*
    * Handle all request until the remote user has entered QUIT_CMD or
    * the connection was idle for ATPD_CMD_TIMEOUT seconds.
    */
   now = last = last_time_read = time(NULL);
   FD_ZERO(&rset);
   for (;;)
   {
      now = time(NULL);
      nbytes = 0;
      timeout.tv_sec = ATPD_CMD_TIMEOUT;

      if ((now - last_time_read) > ATPD_CMD_TIMEOUT)
      {
         (void)fprintf(p_data,
                       "421 Timeout (%d seconds): closing connection.\r\n",
                       ATPD_CMD_TIMEOUT);
         break;
      }
      FD_SET(cmd_sd, &rset);
      timeout.tv_usec = 0;

      /* Wait for message x seconds and then continue. */
      status = select(cmd_sd + 1, &rset, NULL, NULL, &timeout);

      if (FD_ISSET(cmd_sd, &rset))
      {
         if ((nbytes = read(cmd_sd, cmd, 1024)) <= 0)
         {
            if (nbytes == 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          _("Remote hangup by %s"), remote_ip_str);
            }
            else
            {
#ifdef ECONNRESET
               if (errno == ECONNRESET)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             _("%s: read() error : %s"),
                             remote_ip_str, strerror(errno));
               }
               else
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("%s: read() error : %s"),
                             remote_ip_str, strerror(errno));
               }
#else
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("%s: read() error : %s"),
                          remote_ip_str, strerror(errno));
#endif
            }
            break;
         }
         last_time_read = time(NULL);
      }
      else if (status == 0)
           {
              (void)fprintf(p_data,
                            "421 Timeout (%d seconds): closing connection.\r\n",
                            ATPD_CMD_TIMEOUT);
              break;
           }

      if (nbytes > 0)
      {
         i = 0;
         while ((cmd[i] != ' ') && (cmd[i] != '\r'))
         {
            cmd[i] = toupper((int)cmd[i]);
            i++;
         }
         cmd[nbytes] = '\0';
         if (my_strcmp(cmd, QUIT_CMD) == 0)
         {
            (void)fprintf(p_data, "221 Goodbye.\r\n");
            break;
         }
         else if (my_strcmp(cmd, HELP_CMD) == 0)
              {
                 (void)fprintf(p_data,
                               "214- The following commands are recognized (* =>'s unimplemented).\r\n\
   *AFDSTAT *DISC    HELP    HSTAT    ILOG     *INFO    *LDB     LOG\r\n\
   LRF      NOP      OLOG    *PROC    QUIT     SLOG     STAT     TDLOG\r\n\
   TLOG     *TRACEF  *TRACEI *TRACEO  SSTAT\r\n\
214 Direct comments to %s\r\n",
                                  AFD_MAINTAINER);
              }
         else if ((cmd[0] == 'H') && (cmd[1] == 'E') && (cmd[2] == 'L') &&
                  (cmd[3] == 'P') && (cmd[4] == ' ') && (cmd[5] != '\r'))
              {
                 j = 5;
                 while ((cmd[j] != ' ') && (cmd[j] != '\r'))
                 {
                    cmd[j] = toupper((int)cmd[j]);
                    j++;
                 }

                 if (my_strcmp(&cmd[5], QUIT_CMD) == 0)
                 {
                    (void)fprintf(p_data, "%s\r\n", QUIT_SYNTAX);
                 }
                 else if (my_strcmp(&cmd[5], HELP_CMD) == 0)
                      {
                         (void)fprintf(p_data, "%s\r\n", HELP_SYNTAX);
                      }
                 else if (my_strcmp(&cmd[5], NOP_CMDL) == 0)
                      {
                         (void)fprintf(p_data, "%s\r\n", NOP_SYNTAX);
                      }
                      else
                      {
                         *(cmd + nbytes - 2) = '\0';
                         (void)fprintf(p_data,
                                       "502 Unknown command %s\r\n",
                                       &cmd[5]);
                      }
              }
              else if (strncmp(cmd, NOP_CMD, NOP_CMD_LENGTH) == 0)
                   {
                      (void)fprintf(p_data, "200 OK\r\n");
                   }
                   else
                   {
                      *(cmd + nbytes - 2) = '\0';
                      (void)fprintf(p_data,
                                    "500 \'%s\': command not understood.\r\n",
                                    cmd);
                   }
         (void)fflush(p_data);
      } /* if (nbytes > 0) */
   } /* for (;;) */

   (void)fflush(p_data);
   if (fclose(p_data) == EOF)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 _("fclose() error : %s"), strerror(errno));
   }
   p_data = NULL;

   exit(SUCCESS);
}


/*++++++++++++++++++++++++ report_shutdown() ++++++++++++++++++++++++++++*/
static void
report_shutdown(void)
{
   if (p_data != NULL)
   {
      (void)fprintf(p_data, "%s\r\n", ATPD_SHUTDOWN_MESSAGE);
      (void)fflush(p_data);

      if (fclose(p_data) == EOF)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("fclose() error : %s"), strerror(errno));
      }
      p_data = NULL;
   }

   return;
}
