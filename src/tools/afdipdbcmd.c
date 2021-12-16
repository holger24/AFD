/*
 *  afdipdbcmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   afdipdbcmd - allows to show or modify AFD IP database
 **
 ** SYNOPSIS
 **   afdipdbcmd [-w <AFD work dir>] [--version] [option] [<hostname>]
 **                -a <hostname> <ip>
 **                -l
 **                -r <hostname>
 **
 ** DESCRIPTION
 **   This program shows or modifies the AFD IP's that are stored in
 **   a database.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   28.02.2014 H.Kiehl Created
 **   05.04.2014 H.Kiehl Added optional parameters.
 **   16.12.2021 H.Kiehl Added -l to lookup all current IP's.
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <stdlib.h>                 /* exit()                            */
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>                  /* gethostbyname()                   */
#include <arpa/inet.h>
#include <unistd.h>                 /* STDERR_FILENO                     */
#include <errno.h>
#include "version.h"

/* Global variables. */
int                        fsa_id,
                           fsa_fd = -1,
                           no_of_hosts = 0,
                           sys_log_fd = STDERR_FILENO;
#ifdef HAVE_MMAP
off_t                      fsa_size;
#endif
char                       *p_work_dir = NULL;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;
struct filetransfer_status *fsa;

/* External global variables. */
#ifdef LINUX
extern char                *h_errlist[];  /* for gethostbyname()   */
#endif

/* Local function prototypes. */
static void                lookup_all_ips(void),
                           usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int  ret = SUCCESS;
   char work_dir[MAX_PATH_LENGTH];

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD. */
   if (get_afd_path(&argc, argv, work_dir) < 0) 
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if (argc == 1)
   {
      (void)print_ip_db(stdout, NULL);
   }
   else
   {
      if (get_arg(&argc, argv, "-l", NULL, 0) == SUCCESS)
      {
         (void)lookup_all_ips();
      }
      else
      {
         if (get_arg(&argc, argv, "-a", NULL, 0) != SUCCESS)
         {
            if (get_arg(&argc, argv, "-r", NULL, 0) != SUCCESS)
            {
               if (argv[1][0] == '-')
               {
                  usage(argv[0]);
                  ret = INCORRECT;
               }
               else
               {
                  (void)print_ip_db(stdout, argv[1]);
               }
            }
            else
            {
               if (argc == 2)
               {
                  if (remove_from_ip_db(argv[1]) == SUCCESS)
                  {
                     (void)fprintf(stdout, "Removed %s from database.\n",
                                   argv[1]);
                  }
                  else
                  {
                     (void)fprintf(stderr,
                                   "Could not remove %s from database.\n",
                                   argv[1]);
                     ret = INCORRECT;
                  }
               }
               else
               {
                  usage(argv[0]);
                  ret = INCORRECT;
               }
            }
         }
         else
         {
            if (argc == 3)
            {
               set_store_ip(YES);
               add_to_ip_db(argv[1], argv[2]);
               (void)fprintf(stdout, "Added/modified %s in database.\n",
                             argv[1]);
            }
            else
            {
               usage(argv[0]);
               ret = INCORRECT;
            }
         }
      }
   }

   exit(ret);
}


/*++++++++++++++++++++++++++ lookup_all_ips() ++++++++++++++++++++++++++*/
static void
lookup_all_ips(void)
{
   int            i,
                  ret;
   struct hostent *host_entry;

   if ((ret = fsa_attach_passive(NO, "afdipdbcmd")) != SUCCESS)
   {
      if (ret == INCORRECT_VERSION)
      {
         (void)fprintf(stderr,
                       _("ERROR   : This program is not able to attach to the FSA due to incorrect version. (%s %d)\n"),
                       __FILE__, __LINE__);
      }
      else
      {
         if (ret < 0)
         {
            (void)fprintf(stderr,
                             _("ERROR   : Failed to attach to FSA. (%s %d)\n"),
                             __FILE__, __LINE__);
         }
         else
         {
            (void)fprintf(stderr,
                             _("ERROR   : Failed to attach to FSA : %s (%s %d)\n"),
                             strerror(ret), __FILE__, __LINE__);
         }
      }
      exit(INCORRECT);
   }

   for (i = 0; i < no_of_hosts; i++)
   {
      if ((fsa[i].protocol & FTP_FLAG) || (fsa[i].protocol & SFTP_FLAG) ||
#ifdef _WITH_WMO_SUPPORT
          (fsa[i].protocol & WMO_FLAG) ||
#endif
#ifdef _WITH_SCP_SUPPORT
          (fsa[i].protocol & SCP_FLAG) ||
#endif
          (fsa[i].protocol & HTTP_FLAG))
      {
         if ((fsa[i].real_hostname[0][0] != GROUP_IDENTIFIER) &&
             (fsa[i].real_hostname[0][0] != '\0'))
         {
            if ((host_entry = gethostbyname(fsa[i].real_hostname[0])) == NULL)
            {
#ifdef LINUX
               (void)fprintf(stderr,
                             _("ERROR   : gethostbyname(%s) error : %s (%s %d)\n"),
                             fsa[i].real_hostname[0], h_errlist[h_errno],
                             __FILE__, __LINE__);
#else
               (void)fprintf(stderr,
                             _("ERROR   : gethostbyname(%s) error : %d (%s %d)\n"),
                             fsa[i].real_hostname[0], h_errno,
                             __FILE__, __LINE__);
#endif
            }
            else
            {
               (void)fprintf(stdout, "%s %s\n",
                             fsa[i].real_hostname[0],
                             inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0])));
            }
         }
         if (fsa[i].real_hostname[1][0] != '\0')
         {
            if ((host_entry = gethostbyname(fsa[i].real_hostname[1])) == NULL)
            {
#ifdef LINUX
               (void)fprintf(stderr,
                             _("ERROR   : gethostbyname(%s) error : %s (%s %d)\n"),
                             fsa[i].real_hostname[1], h_errlist[h_errno],
                             __FILE__, __LINE__);
#else
               (void)fprintf(stderr,
                             _("ERROR   : gethostbyname(%s) error : %d (%s %d)\n"),
                             fsa[i].real_hostname[1], h_errno,
                             __FILE__, __LINE__);
#endif
            }
            else
            {
               (void)fprintf(stdout, "%s %s\n",
                             fsa[i].real_hostname[1],
                             inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0])));
            }
         }
      }
   }

   (void)fsa_detach(NO);

   return;
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{
   (void)fprintf(stderr,
                 _("SYNTAX  : %s [-w working directory] [option] [<hostname>]\n"),
                 progname);
   (void)fprintf(stderr, _("    -a <hostname> <ip>     add or modify given hostname\n"));
   (void)fprintf(stderr, _("    -l                     get a current list of all IP's without looking into the IP database\n"));
   (void)fprintf(stderr, _("    -r <hostname>          remove hostname\n"));
   return;
}
