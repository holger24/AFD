/*
 *  ahtml_list.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2024, 2025 Deutscher Wetterdienst (DWD),
 *                           Holger Kiehl <Holger.Kiehl@dwd.de>
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

#include "ahtml_listdefs.h"

DESCR__S_M1
/*
 ** NAME
 **   ahtml_list - show a HTML listing what AFD is able to see.
 **
 ** SYNOPSIS
 **   ahtml_list [options] <file or URL>
 **
 **   options
 **     --version                       - Show current version
 **
 ** DESCRIPTION
 **   ahtml_list list the links it finds in a given URL or file
 **   name, to make it easier to create a [files] filter of the
 **   correct files one wants to download.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   23.02.2024 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <stdlib.h>     /* exit(), atexit()                              */
#include <string.h>     /* strlen()                                      */
#include <unistd.h>     /* STDERR_FILENO                                 */
#include <signal.h>     /* signal()                                      */
#include <errno.h>
#include "httpdefs.h"
#include "version.h"

/* Global variables. */
int         sigpipe_flag,
            sys_log_fd = STDERR_FILENO,
            timeout_flag,
            transfer_log_fd = STDERR_FILENO,
            use_ip_db = NO;
long        transfer_timeout;
char        *html_list_filename = NULL,
            msg_str[MAX_RET_MSG_LENGTH],
            *p_work_dir = NULL;
const char  *sys_log_name = SYSTEM_LOG_FIFO;
struct data db;

/* Local global variables. */
static char name[30];

/* Local function prototypes. */
static void ahtml_list_exit(void),
            init_ahtml_list(int, char **, struct data *, char **),
            sig_bus(int),
            sig_segv(int),
            sig_pipe(int),
            sig_exit(int),
            usage(void);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   char *parg = NULL;

   CHECK_FOR_VERSION(argc, argv);

   /* Do some cleanups when we exit. */
   db.index_file = NULL;
   if (atexit(ahtml_list_exit) != 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                _("Could not register exit function : %s (%s %d)\n"),
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR) ||
       (signal(SIGPIPE, sig_pipe) == SIG_ERR))
   {
      (void)rec(sys_log_fd, FATAL_SIGN, _("signal() error : %s (%s %d)\n"),
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Initialise variables. */
   init_ahtml_list(argc, argv, &db, &parg);

   /* Check if it is a URL. */
   if (((parg[0] == 'h') && (parg[1] == 't') && (parg[2] == 't') &&
        (parg[3] == 'p') && (parg[4] == ':') && (parg[5] == '/') &&
        (parg[6] == '/')) ||
       ((parg[0] == 'h') && (parg[1] == 't') && (parg[2] == 't') &&
        (parg[3] == 'p') && (parg[4] == 's') && (parg[5] == ':') &&
        (parg[6] == '/') && (parg[7] == '/')))
   {
      unsigned int error_mask;
      time_t       now = time(NULL);

      if (db.remove == NEITHER)
      {
         db.remove = YES;
      }

      if ((error_mask = url_evaluate(parg, NULL, db.user, NULL, NULL,
#ifdef WITH_SSH_FINGERPRINT
                                     NULL, NULL,
#endif
                                     db.password, NO, db.hostname,
                                     &db.port, db.remote_dir, NULL,
                                     &now, NULL, NULL, NULL, NULL,
                                     NULL, NULL)) > 3)
      {
         char error_msg[MAX_URL_ERROR_MSG];

         url_get_error(error_mask, error_msg, MAX_URL_ERROR_MSG);
         (void)fprintf(stderr,
                       _("ERROR   : Incorrect url `%s'. Error is: %s.\n"),
                       argv[1], error_msg);
      }
      else /* Try to retrieve the HTML list. */
      {
         if ((html_list_filename = malloc(DEFAULT_HTML_LIST_FILENAME_LENGTH + 1)) == NULL)
         {
            (void)fprintf(stderr,
                          _("ERROR   : malloc() error : %s\n"),
                          strerror(errno));
            exit(INCORRECT);
         }
         (void)strcpy(html_list_filename, DEFAULT_HTML_LIST_FILENAME);

         /* Set HTTP timeout value. */
         transfer_timeout = db.transfer_timeout;
         if (parg[4] == 's')
         {
            /* Note, url_evaluate() will set port to -1 if no port is given. */
            if (db.port == -1)
            {
               db.port = DEFAULT_HTTPS_PORT;
            }
#ifdef WITH_SSL
            db.tls_auth = YES;
#endif
         }
         else
         {
            /* Note, url_evaluate() will set port to -1 if no port is given. */
            if (db.port == -1)
            {
               db.port = DEFAULT_HTTP_PORT;
            }
#ifdef WITH_SSL
            db.tls_auth = NO;
#endif
         }
         get_html_content(html_list_filename, &db);
      }
   }
   else /* Lets assume this is a file name which contains a HTML list. */
   {
      off_t list_size;
      char  *list_buffer = NULL;

      if (db.remove == NEITHER)
      {
         db.remove = NO;
      }
      if ((html_list_filename = malloc(strlen(parg) + 1)) == NULL)
      {
         (void)fprintf(stderr, _("ERROR   : malloc() error : %s\n"),
                       strerror(errno));
         exit(INCORRECT);
      }
      (void)strcpy(html_list_filename, parg);

      if ((list_size = read_file(html_list_filename, &list_buffer)) == INCORRECT)
      {
         (void)rec(sys_log_fd, ERROR_SIGN, "Failed to read_file() %s (%s %d)\n",
                   html_list_filename, __FILE__, __LINE__);
         exit(INCORRECT);
      }
      if (list_size > 0)
      {
         if (eval_html_dir_list(list_buffer, list_size, 0,
                                (db.special_flag & HREF_SEARCH_ONLY) ? YES : NO,
                                NULL, &db) != SUCCESS)
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "eval_html_dir_list() error. (%s %d)\n",
                      __FILE__, __LINE__);
         }
      }
      free(list_buffer);
   }
   if (db.remove == YES)
   {
      if (unlink(html_list_filename) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to unlink() %s : %s (%s %d)\n",
                   html_list_filename, strerror(errno), __FILE__, __LINE__);
      }
   }
   free(parg);

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++ init_ahtml_list() +++++++++++++++++++++++++*/
static void
init_ahtml_list(int argc, char *argv[], struct data *p_db, char **parg)
{
   int    correct = YES;              /* Was input/syntax correct?      */
   size_t length;
   char   *ptr;

   ptr = argv[0] + strlen(argv[0]) - 1;
   while ((*ptr != '/') && (ptr != &argv[0][0]))
   {
      ptr--;
   }
   if (*ptr == '/')
   {
      ptr++;
   }
   (void)my_strncpy(name, ptr, 30);

   /* First initialize all values with default values. */
   msg_str[0] = '\0';
   p_db->hostname[0]          = '\0';
   p_db->user[0]              = '\0';
   p_db->password[0]          = '\0';
   p_db->remote_dir[0]        = '\0';
   p_db->blocksize            = DEFAULT_TRANSFER_BLOCKSIZE;
   p_db->proxy_name[0]        = '\0';
   p_db->transfer_timeout     = DEFAULT_TRANSFER_TIMEOUT;
   p_db->verbose              = NO;
   p_db->remove               = NEITHER;
   p_db->sndbuf_size          = 0;
   p_db->rcvbuf_size          = 0;
   p_db->no_expect            = NO;
#ifdef WITH_SSL
   p_db->strict               = NO;
   p_db->legacy_renegotiation = NO;
#endif
   p_db->special_flag         = 0;

   /* Evaluate all arguments with '-'. */
   while ((--argc > 0) && ((*++argv)[0] == '-'))
   {
      switch (*(argv[0] + 1))
      {
         case 'b' : /* HTTP transfer block size. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr, _("ERROR   : No block size specified for option -b.\n"));
               correct = NO;
            }
            else
            {
               p_db->blocksize = atoi(*(argv + 1));
               argc--;
               argv++;
            }
            break;

         case 'c' : /* Remove content file. */
            p_db->remove = YES;
            break;

         case 'C' : /* Do not remove content file. */
            p_db->remove = NO;
            break;

         case 'E' : /* No expect. */
            p_db->no_expect = YES;
            break;

         case 'f' : /* Force href search only. */
            p_db->special_flag |= HREF_SEARCH_ONLY;
            break;

         case 'i' : /* Index file name. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr,
                             _("ERROR   : No index file name specified for option -i.\n"));
               correct = NO;
            }
            else
            {
               argv++;
               length = strlen(argv[0]);
               if ((p_db->index_file = malloc(length + 1)) == NULL)
               {
                  (void)fprintf(stderr,
                                _("ERROR   : malloc() error : %s\n"),
                                strerror(errno));
               }
               else
               {
                  (void)memcpy(p_db->index_file, argv[0], length);
                  p_db->index_file[length] = '\0';
               }
            }
            break;

         case 'P' : /* Proxy server */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr,
                             _("ERROR   : No proxy server specified for option -P.\n"));
               correct = NO;
            }
            else
            {
               argv++;
               (void)my_strncpy(p_db->proxy_name, argv[0],
                                MAX_PROXY_NAME_LENGTH);
            }
            break;

         case 'R' : /* Socket receive buffer. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr, _("ERROR   : No buffer size specified for option -R.\n"));
               correct = NO;
            }
            else
            {
               p_db->rcvbuf_size = atoi(*(argv + 1));
               argc--;
               argv++;
            }
            break;

         case 'S' : /* Socket send buffer. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr, _("ERROR   : No buffer size specified for option -S.\n"));
               correct = NO;
            }
            else
            {
               p_db->sndbuf_size = atoi(*(argv + 1));
               argc--;
               argv++;
            }
            break;

         case 't' : /* HTTP timeout. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr, _("ERROR   : No timeout specified for option -t.\n"));
               correct = NO;
            }
            else
            {
               p_db->transfer_timeout = atol(*(argv + 1));
               argc--;
               argv++;
            }
            break;

         case 'u':
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr,
                             _("ERROR   : No URL given for option -u.\n"));
               correct = NO;
            }
            else
            {
               unsigned int error_mask;
               time_t       now = time(NULL);

               argv++;

               if ((error_mask = url_evaluate(argv[0], NULL, p_db->user,
                                              NULL, NULL,
#ifdef WITH_SSH_FINGERPRINT
                                              NULL, NULL,
#endif
                                              p_db->password, NO,
                                              p_db->hostname, &p_db->port,
                                              p_db->remote_dir, NULL, &now,
                                              NULL, NULL, NULL, NULL,
                                              NULL, NULL)) > 3)
               {
                  char error_msg[MAX_URL_ERROR_MSG];

                  url_get_error(error_mask, error_msg, MAX_URL_ERROR_MSG);
                  (void)fprintf(stderr,
                                _("ERROR   : Incorrect url `%s'. Error is: %s.\n"),
                                argv[0], error_msg);
                  correct = NO;
               }
            }
            break;

         case 'v' : /* Verbose mode. */
            p_db->verbose = YES;
            break;

#ifdef WITH_SSL
         case 'x' : /* TLS legacy renegotiation. */
            p_db->legacy_renegotiation = YES;
            break;

         case 'Y' : /* Strict SSL/TLS verification. */
            p_db->strict = YES;
            break;
#endif

         case '?' : /* Help. */
            usage();
            exit(0);

         default : /* Unknown parameter. */

            (void)fprintf(stderr,
                          _("ERROR   : Unknown parameter <%c>. (%s %d)\n"),
                          *(argv[0] + 1), __FILE__, __LINE__);
            correct = NO;
            break;
      } /* switch (*(argv[0] + 1)) */
   }
   if ((*argv)[0] != '-')
   {
      argc++;
      argv--;
   }

   length = strlen(argv[1]);
   if ((*parg = malloc(length + 1)) == NULL)
   {
      (void)fprintf(stderr, _("ERROR   : malloc() error : %s\n"),
                    strerror(errno));
      exit(INCORRECT);
   }
   (void)memcpy(*parg, argv[1], length);
   (*parg)[length] = '\0';

   if ((argc < 2) || (correct == NO))
   {
      usage();
      exit(SYNTAX_ERROR);
   }

   return;
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(void)
{
   (void)fprintf(stderr, _("SYNTAX: %s [options] [URL|file]\n\n"), name);
   (void)fprintf(stderr, _("  OPTIONS                      DESCRIPTION\n"));
   (void)fprintf(stderr, _("  --version                  - Show current version\n"));
   (void)fprintf(stderr, _("  -b <block size>            - Transfer block size in bytes. Default %d\n"), DEFAULT_TRANSFER_BLOCKSIZE);
   (void)fprintf(stderr, _("                               bytes.\n"));
   (void)fprintf(stderr, _("  -c                         - Remove content file.\n"));
   (void)fprintf(stderr, _("  -C                         - Do not remove content file.\n"));
   (void)fprintf(stderr, _("  -E                         - Do not send expect.\n"));
   (void)fprintf(stderr, _("  -f                         - Force href search only.\n"));
   (void)fprintf(stderr, _("  -i <file name>             - Non standard index file name.\n"));
   (void)fprintf(stderr, _("  -P <Proxy server>          - Proxy server.\n"));
   (void)fprintf(stderr, _("  -p <port number>           - Remote port number of HTTP-server.\n"));
   (void)fprintf(stderr, _("                               Default %d or %d.\n"), DEFAULT_HTTP_PORT, DEFAULT_HTTPS_PORT);
   (void)fprintf(stderr, _("  -R <buffer size>           - Socket receive buffer size\n"));
   (void)fprintf(stderr, _("                               (in bytes).\n"));
   (void)fprintf(stderr, _("  -S <buffer size>           - Socket send buffer size\n"));
   (void)fprintf(stderr, _("                               (in bytes).\n"));
   (void)fprintf(stderr, _("  -t <timout>                - HTTP timeout in seconds. Default %lds.\n"), DEFAULT_TRANSFER_TIMEOUT);
   (void)fprintf(stderr, _("  -u <URL>                   - When just evaluating a local file. This\n"));
   (void)fprintf(stderr, _("                               allows adding a URL for testing.\n"));
   (void)fprintf(stderr, _("  -v                         - Verbose. Shows more information.\n"));
#ifdef WITH_SSL
   (void)fprintf(stderr, _("  -x                         - Use TLS legacy renegotiation.\n"));
   (void)fprintf(stderr, _("  -Y                         - Use strict SSL/TLS verification.\n"));
#endif
   (void)fprintf(stderr, _("  -?                         - Display this help and exit.\n"));
   (void)fprintf(stderr, _("  The following values are returned on exit:\n"));
   (void)fprintf(stderr, _("      %2d - File transmitted successfully.\n"), TRANSFER_SUCCESS);
   (void)fprintf(stderr, _("      %2d - Failed to connect.\n"), CONNECT_ERROR);
   (void)fprintf(stderr, _("      %2d - Failed to open remote file.\n"), OPEN_REMOTE_ERROR);
   (void)fprintf(stderr, _("      %2d - System error stat().\n"), STAT_ERROR);
   (void)fprintf(stderr, "      %2d - %s.\n", TIMEOUT_ERROR, TIMEOUT_ERROR_STR);
   (void)fprintf(stderr, "      %2d - %s.\n", CONNECTION_RESET_ERROR, CONNECTION_RESET_ERROR_STR);
   (void)fprintf(stderr, "      %2d - %s.\n", CONNECTION_REFUSED_ERROR, CONNECTION_REFUSED_ERROR_STR);
   (void)fprintf(stderr, _("      %2d - System error malloc().\n"), ALLOC_ERROR);
   (void)fprintf(stderr, _("      %2d - Syntax wrong.\n"), SYNTAX_ERROR);

   return;
}


/*+++++++++++++++++++++++++++ ahtml_list_exit() +++++++++++++++++++++++++*/
static void
ahtml_list_exit(void)
{
   if (db.index_file != NULL)
   {
      free(db.index_file);
   }

   return;
}


/*++++++++++++++++++++++++++++++ sig_pipe() +++++++++++++++++++++++++++++*/
static void
sig_pipe(int signo)
{
   /* Ignore any future signals of this kind. */
   if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, _("signal() error : %s (%s %d)\n"),
                strerror(errno), __FILE__, __LINE__);
   }
   sigpipe_flag = ON;

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   (void)rec(sys_log_fd, DEBUG_SIGN,
             _("Aaarrrggh! Received SIGSEGV. Remove the programmer who wrote this! (%s %d)\n"),
             __FILE__, __LINE__);
   exit(INCORRECT);
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   (void)rec(sys_log_fd, DEBUG_SIGN,
             _("Uuurrrggh! Received SIGBUS. (%s %d)\n"),
             __FILE__, __LINE__);
   exit(INCORRECT);
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   exit(INCORRECT);
}
