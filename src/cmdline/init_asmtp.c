/*
 *  init_asmtp.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2014 Deutscher Wetterdienst (DWD),
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

#include "asmtpdefs.h"

DESCR__S_M3
/*
 ** NAME
 **   init_asmtp - checks syntax of input for process asmtp
 **
 ** SYNOPSIS
 **   int init_asmtp(int         argc,
 **                  char        *argv[],
 **                  struct data *p_db)
 **
 ** DESCRIPTION
 **   This module checks whether the syntax of aftp is correct and
 **   stores these values into the structure job.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   20.11.2000 H.Kiehl Created
 **   04.08.2002 H.Kiehl Added To:, From: and Reply-To headers.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* stderr, fprintf()                  */
#include <stdlib.h>                /* exit(), atoi()                     */
#include <string.h>                /* strcpy(), strerror(), strcmp()     */
#include <ctype.h>                 /* isdigit()                          */
#include <errno.h>
#include "cmdline.h"
#include "smtpdefs.h"

/* Global variables. */
extern int  sys_log_fd;
extern char *p_work_dir;

/* Local global variables. */
static char name[30];

/* Local function prototypes. */
static void usage(void);


/*############################ init_asmtp() #############################*/
int
init_asmtp(int argc, char *argv[], struct data *p_db)
{
   int  correct = YES;                /* Was input/syntax correct?      */

   (void)my_strncpy(name, argv[0], 30);

   /* First initialize all values with default values. */
   p_db->blocksize        = DEFAULT_TRANSFER_BLOCKSIZE;
   p_db->smtp_server[0]   = '\0';
   p_db->user[0]          = '\0';
   p_db->password[0]      = '\0';
   p_db->hostname[0]      = '\0';
   p_db->port             = DEFAULT_SMTP_PORT;
   p_db->remove           = NO;
   p_db->transfer_timeout = DEFAULT_TRANSFER_TIMEOUT;
   p_db->verbose          = NO;
   p_db->no_of_files      = 0;
   p_db->subject          = NULL;
   p_db->from             = NULL;
   p_db->reply_to         = NULL;
   p_db->special_flag     = 0;
   p_db->filename         = NULL;
   p_db->realname         = NULL;

   /* Evaluate all arguments with '-'. */
   while ((--argc > 0) && ((*++argv)[0] == '-'))
   {
      switch (*(argv[0] + 1))
      {
         case 'a' : /* Adress, where to send the mail, that is user@host. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr,
                             _("ERROR   : No address specified for option -a.\n"));
               correct = NO;
            }
            else
            {
               int  length = 0;
               char *ptr;

               ptr = argv[1];
               while ((*ptr != '@') && (*ptr != '\0') &&
                      (length < MAX_USER_NAME_LENGTH))
               {
                  p_db->user[length] = *ptr;
                  ptr++; length++;
               }
               if (*ptr == '@')
               {
                  ptr++; length++;
                  if (length > 0)
                  {
                     p_db->user[length] = '\0';
                     length = 0;
                     while ((*ptr != '\0') && (length < MAX_FILENAME_LENGTH))
                     {
                        p_db->hostname[length] = *ptr;
                        ptr++; length++;
                     }
                     if (length > 0)
                     {
                        if (length < MAX_FILENAME_LENGTH)
                        {
                           p_db->hostname[length] = '\0';
                        }
                        else
                        {
                           (void)fprintf(stderr,
                                         _("ERROR   : The hostname is to long, it may only be %d characters long.\n"),
                                         MAX_FILENAME_LENGTH);
                           correct = NO;
                        }
                     }
                     else
                     {
                        (void)fprintf(stderr,
                                      _("ERROR   : No hostname specified.\n"));
                        correct = NO;
                     }
                  }
                  else /* No user name specified. */
                  {
                     (void)fprintf(stderr, _("ERROR   : No user specified.\n"));
                     correct = NO;
                  }
               }
               else if (length < MAX_USER_NAME_LENGTH)
                    {
                       (void)fprintf(stderr,
                                     _("ERROR   : No remote host specified. (%s)\n"),
                                     argv[1]);
                       correct = NO;
                    }
                    else
                    {
                       (void)fprintf(stderr,
                                     _("ERROR   : The user name is to long, it may only be %d characters long.\n"),
                                     MAX_USER_NAME_LENGTH);
                       correct = NO;
                    }
               argc--;
               argv++;
            }
            break;

         case 'b' : /* Transfer block size. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr,
                             _("ERROR   : No block size specified for option -b.\n"));
               correct = NO;
            }
            else
            {
               p_db->blocksize = atoi(*(argv + 1));
               argc--;
               argv++;
            }
            break;

         case 'c' : /* Configuration file for user, passwd, etc */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr,
                             _("ERROR   : No config file specified for option -c.\n"));
               correct = NO;
            }
            else
            {
               char config_file[MAX_PATH_LENGTH];

               argv++;
               (void)my_strncpy(config_file, argv[0], MAX_PATH_LENGTH);
               argc--;

               eval_config_file(config_file, p_db);
            }
            break;

         case 'e' : /* Encode file using BASE64. */

            p_db->special_flag |= ATTACH_FILE;
            break;

         case 'f' : /* Configuration file for filenames. */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr,
                             _("ERROR   : No filename file specified for option -f.\n"));
               correct = NO;
            }
            else
            {
               char filename_file[MAX_PATH_LENGTH];

               argv++;
               (void)my_strncpy(filename_file, argv[0], MAX_PATH_LENGTH);
               argc--;

               if (eval_filename_file(filename_file, p_db) == INCORRECT)
               {
                  exit(FILE_NAME_FILE_ERROR);
               }
            }
            break;

         case 'h' : /* Remote host name. */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr,
                             _("ERROR   : No host name or IP number specified for option -h.\n"));
               correct = NO;
            }
            else
            {
               argv++;
               (void)my_strncpy(p_db->hostname, argv[0], MAX_FILENAME_LENGTH);
               argc--;
            }
            break;

         case 'i' : /* From header. */

            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr,
                             _("ERROR   : No from address specified for option -i.\n"));
               correct = NO;
            }
            else
            {
               size_t length;

               argv++;
               length = strlen(argv[0]) + 1;
               if ((p_db->from = malloc(length)) == NULL)
               {
                  (void)fprintf(stderr,
                                _("ERROR   : malloc() error : %s (%s %d)\n"),
                                strerror(errno), __FILE__, __LINE__);
                  correct = NO;
               }
               else
               {
                  (void)strcpy(p_db->from, argv[0]);
               }
               argc--;
            }
            break;

         case 'm' : /* Mail server that will send this mail. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr,
                             _("ERROR   : No mail server name or IP number specified for option -m.\n"));
               correct = NO;
            }
            else
            {
               argv++;
               (void)my_strncpy(p_db->smtp_server, argv[0],
                                MAX_USER_NAME_LENGTH);
               argc--;
            }
            break;

         case 'n' : /* Filename is subject. */
            p_db->special_flag |= FILE_NAME_IS_SUBJECT;
            break;

         case 'o' : /* Reply-To. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr,
                             _("ERROR   : No reply-to address specified for option -o.\n"));
               correct = NO;
            }
            else
            {
               size_t length;

               argv++;
               length = strlen(argv[0]) + 1;
               if ((p_db->reply_to = malloc(length)) == NULL)
               {
                  (void)fprintf(stderr,
                                _("ERROR   : malloc() error : %s (%s %d)\n"),
                                strerror(errno), __FILE__, __LINE__);
                  correct = NO;
               }
               else
               {
                  (void)strcpy(p_db->reply_to, argv[0]);
               }
               argc--;
            }
            break;

         case 'p' : /* Remote TCP port number. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr,
                             _("ERROR   : No port number specified for option -p.\n"));
               correct = NO;
            }
            else
            {
               p_db->port = atoi(*(argv + 1));
               argc--;
               argv++;
            }
            break;

         case 'r' : /* Remove file that was transmitted. */
            p_db->remove = YES;
            break;

         case 's' : /* Subject. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr,
                             _("ERROR   : No subject specified for option -s.\n"));
               correct = NO;
            }
            else
            {
               size_t length;

               argv++;
               length = strlen(argv[0]) + 1;
               if ((p_db->subject = malloc(length)) == NULL)
               {
                  (void)fprintf(stderr,
                                _("ERROR   : malloc() error : %s (%s %d)\n"),
                                strerror(errno), __FILE__, __LINE__);
                  correct = NO;
               }
               else
               {
                  (void)strcpy(p_db->subject, argv[0]);
               }
               argc--;
            }
            break;


         case 't' : /* Transfer timeout. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr,
                             _("ERROR   : No timeout specified for option -t.\n"));
               correct = NO;
            }
            else
            {
               p_db->transfer_timeout = atol(*(argv + 1));
               argc--;
               argv++;
            }
            break;

         case 'u' : /* User name. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr,
                             _("ERROR   : No user specified for option -u.\n"));
               correct = NO;
            }
            else
            {
               argv++;
               (void)my_strncpy(p_db->user, argv[0], MAX_USER_NAME_LENGTH);
               argc--;
            }
            break;

         case 'v' : /* Verbose mode. */
            p_db->verbose = YES;
            break;

         case 'y' : /* Filename is user. */
            p_db->special_flag |= FILE_NAME_IS_USER;
            break;

         case '?' : /* Help */
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

   if (p_db->hostname[0] == '\0')
   {
      (void)fprintf(stderr,
                    _("ERROR   : No host name or IP number specified.\n"));
      correct = NO;
   }

   if ((p_db->no_of_files == 0) && (argc == 0))
   {
      (void)fprintf(stderr,
                    _("ERROR   : No files to be send specified.\n"));
      correct = NO;
   }
   else if ((correct == YES) && (argc > 0) && (p_db->no_of_files == 0))
        {
           int i = p_db->no_of_files;

           if (i == 0)
           {
              RT_ARRAY(p_db->filename, argc, MAX_PATH_LENGTH, char);
           }
           else
           {
              REALLOC_RT_ARRAY(p_db->filename, (i + argc), MAX_PATH_LENGTH, char);
           }
           p_db->no_of_files += argc - 1;
           while ((--argc > 0) && ((*++argv)[0] != '-'))
           {
              (void)my_strncpy(p_db->filename[i], argv[0], MAX_PATH_LENGTH);
              i++;
           }
        }

   /* If input is not correct show syntax. */
   if (correct == NO)
   {
      usage();
      exit(SYNTAX_ERROR);
   }

   return(SUCCESS);
} /* eval_input() */


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(void)
{
   (void)fprintf(stderr, _("SYNTAX: %s [options] [file(s)]\n\n"), name);
   (void)fprintf(stderr, _("  OPTIONS                      DESCRIPTION\n"));
   (void)fprintf(stderr, _("  --version                  - Show current version\n"));
   (void)fprintf(stderr, _("  -a <user@host>             - The address where the mail is sent to.\n"));
   (void)fprintf(stderr, _("  -b <block size>            - Transfer block size in bytes. Default %d\n"), DEFAULT_TRANSFER_BLOCKSIZE);
   (void)fprintf(stderr, _("                               bytes.\n"));
   (void)fprintf(stderr, _("  -c <config file>           - Configuration file holding user name,\n"));
   (void)fprintf(stderr, _("                               domain and SMTP server in URL format.\n"));
   (void)fprintf(stderr, _("  -e                         - Encode files in BASE64.\n"));
   (void)fprintf(stderr, _("  -f <filename>              - File containing a list of filenames\n"));
   (void)fprintf(stderr, _("                               that are to be send.\n"));
   (void)fprintf(stderr, _("  -h <hostname | IP number>  - Recipient hostname of this mail.\n"));
   (void)fprintf(stderr, _("  -i <From-address>          - Address of who send the mail.\n"));
   (void)fprintf(stderr, _("  -m <mailserver-address>    - Mailserver that will send this mail.\n"));
   (void)fprintf(stderr, _("                               Default is %s.\n"), SMTP_HOST_NAME);
   (void)fprintf(stderr, _("  -n                         - File name is subject.\n"));
   (void)fprintf(stderr, _("  -o <reply-to address>      - Where the receiver should send is reply.\n"));
   (void)fprintf(stderr, _("  -p <port number>           - Remote port number of SMTP-server.\n"));
   (void)fprintf(stderr, _("                               Default %d.\n"), DEFAULT_SMTP_PORT);
   (void)fprintf(stderr, _("  -r                         - Remove transmitted file.\n"));
   (void)fprintf(stderr, _("  -s <subject>               - Subject of this mail.\n"));
   (void)fprintf(stderr, _("  -t <timout>                - SMTP timeout in seconds. Default %lds.\n"), DEFAULT_TRANSFER_TIMEOUT);
   (void)fprintf(stderr, _("  -u <user>                  - The user who should get the mail.\n"));
   (void)fprintf(stderr, _("  -v                         - Verbose. Shows all SMTP commands and\n"));
   (void)fprintf(stderr, _("                               the reply from the SMTP server.\n"));
   (void)fprintf(stderr, _("  -y                         - File name is user.\n"));
   (void)fprintf(stderr, _("  -?                         - Display this help and exit.\n"));
   (void)fprintf(stderr, _("  The following values are returned on exit:\n"));
   (void)fprintf(stderr, _("      %2d - File transmitted successfully.\n"), TRANSFER_SUCCESS);
   (void)fprintf(stderr, _("      %2d - Failed to connect.\n"), CONNECT_ERROR);
   (void)fprintf(stderr, _("      %2d - User name wrong.\n"), USER_ERROR);
   (void)fprintf(stderr, _("      %2d - Failed to open remote file.\n"), OPEN_REMOTE_ERROR);
   (void)fprintf(stderr, _("      %2d - Error when writing into remote file.\n"), WRITE_REMOTE_ERROR);
   (void)fprintf(stderr, _("      %2d - Failed to close remote file.\n"), CLOSE_REMOTE_ERROR);
   (void)fprintf(stderr, _("      %2d - Failed to rename remote file.\n"), MOVE_REMOTE_ERROR);
   (void)fprintf(stderr, _("      %2d - Remote directory could not be set.\n"), CHDIR_ERROR);
   (void)fprintf(stderr, "      %2d - %s.\n", TIMEOUT_ERROR, TIMEOUT_ERROR_STR);
   (void)fprintf(stderr, "      %2d - %s.\n", CONNECTION_RESET_ERROR, CONNECTION_RESET_ERROR_STR);
   (void)fprintf(stderr, "      %2d - %s.\n", CONNECTION_REFUSED_ERROR, CONNECTION_REFUSED_ERROR_STR);
   (void)fprintf(stderr, _("      %2d - Could not open source file.\n"), OPEN_LOCAL_ERROR);
   (void)fprintf(stderr, _("      %2d - Failed to read source file.\n"), READ_LOCAL_ERROR);
   (void)fprintf(stderr, _("      %2d - System error stat().\n"), STAT_ERROR);
   (void)fprintf(stderr, _("      %2d - System error malloc().\n"), ALLOC_ERROR);
   (void)fprintf(stderr, _("      %2d - Failed to read file name file.\n"), FILE_NAME_FILE_ERROR);
   (void)fprintf(stderr, _("      %2d - Failed to send remote mail address.\n"), REMOTE_USER_ERROR);
   (void)fprintf(stderr, _("      %2d - Failed to send SMTP DATA command.\n"), DATA_ERROR);
   (void)fprintf(stderr, _("      %2d - Syntax wrong.\n"), SYNTAX_ERROR);

   return;
}
