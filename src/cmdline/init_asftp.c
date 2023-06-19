/*
 *  init_asftp.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2015-2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#include "asftpdefs.h"

DESCR__S_M3
/*
 ** NAME
 **   init_asftp - checks syntax of input for process asftp
 **
 ** SYNOPSIS
 **   int init_asftp(int         argc,
 **                  char        *argv[],
 **                  struct data *p_db)
 **
 ** DESCRIPTION
 **   This module checks whether the syntax of asftp is correct and
 **   stores these values into the structure job.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   04.05.2015 H.Kiehl    Created
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* stderr, fprintf()                  */
#include <stdlib.h>                /* exit(), atoi()                     */
#include <string.h>                /* strcpy(), strerror(), strcmp()     */
#include <ctype.h>                 /* isdigit()                          */
#include <errno.h>
#include "cmdline.h"
#include "sftpdefs.h"

/* Global variables. */
extern int  no_of_host,
            sys_log_fd,
            fsa_id;
extern char *p_work_dir;

/* Local global variables. */
static char name[30];

/* Local function prototypes. */
static void usage(void);


/*########################### init_asftp() ##############################*/
int
init_asftp(int argc, char *argv[], struct data *p_db)
{
   int  correct = YES;                /* Was input/syntax correct?      */
   char *ptr;

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
   if (name[0] == 'r')
   {
      p_db->exec_mode = RETRIEVE_MODE;
   }
   else if (name[0] == 't')
        {
           p_db->exec_mode = TEST_MODE;
        }
        else
        {
           p_db->exec_mode = TRANSFER_MODE;
        }

   /* First initialize all values with default values. */
   p_db->file_size_offset   = -1;       /* No appending. */
   p_db->blocksize          = DEFAULT_TRANSFER_BLOCKSIZE;
   p_db->remote_dir[0]      = '\0';
   p_db->hostname[0]        = '\0';
   p_db->lock               = DOT;
   p_db->lock_notation[0]   = '.';
   p_db->lock_notation[1]   = '\0';
   p_db->transfer_mode      = 'I';
   p_db->port               = SSH_PORT_UNSET;
   p_db->user[0]            = '\0';
   p_db->password[0]        = '\0';
   p_db->remove             = NO;
   p_db->transfer_timeout   = DEFAULT_TRANSFER_TIMEOUT;
   p_db->verbose            = NO;
   p_db->append             = NO;
   p_db->create_target_dir  = NO;
   p_db->dir_mode           = 0;
   p_db->dir_mode_str[0]    = '\0';
   if (name[0] == 't')
   {
      p_db->no_of_files     = 1;
      p_db->dummy_size      = DEFAULT_TRANSFER_BLOCKSIZE;
   }
   else
   {
      p_db->no_of_files     = 0;
   }
   p_db->filename           = NULL;
   p_db->realname           = NULL;
   p_db->sndbuf_size        = 0;
   p_db->rcvbuf_size        = 0;
   p_db->proxy_name[0]      = '\0';
   p_db->ssh_protocol       = 0;
#ifdef WITH_SSH_FINGERPRINT
   p_db->ssh_fingerprint[0] = '\0';
   p_db->key_type           = 0;
#endif

   /* Evaluate all arguments with '-'. */
   while ((--argc > 0) && ((*++argv)[0] == '-'))
   {
      switch (*(argv[0] + 1))
      {
         case 'A' : /* Search for file localy for appending. */
            p_db->append = YES;
            break;

         case 'a' : /* Remote file size offset for appending. */
            if ((name[0] == 'r') || (name[0] == 't'))
            {
               (void)fprintf(stderr, _("ERROR   : This option is only for %s.\n"), &name[1]);
               correct = NO;
            }
            else
            {
               p_db->append = YES;
            }
            argc--;
            argv++;
            break;

         case 'b' : /* Transfer block size. */
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
               if (p_db->blocksize > MAX_SFTP_BLOCKSIZE)
               {
                  (void)fprintf(stdout,
                                "Decreasing block size to %d because it is the maximum SFTP can handle.\n",
                                MAX_SFTP_BLOCKSIZE);
                  p_db->blocksize = MAX_SFTP_BLOCKSIZE;
               }
            }
            break;

         case 'c' : /* Configuration file for user, passwd, etc. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr, _("ERROR   : No config file specified for option -c.\n"));
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

         case 'C' : /* Create target dir. */
            p_db->create_target_dir = YES;
            if ((argc > 1) && (*(argv + 1)[0] != '-'))
            {
               int  i = 0;
               char *aptr;

               aptr = argv[1];
               do
               {
                  if ((isdigit((int)*aptr)) && (i < 4))
                  {
                     p_db->dir_mode_str[i] = *aptr;
                     i++; aptr++;
                  }
                  else
                  {
                     i = 0;
                     p_db->dir_mode_str[0] = '\0';
                     break;
                  }
               } while (*aptr != '\0');
               if (i > 0)
               {
                  argv++;
                  argc--;
                  p_db->dir_mode_str[i] = '\0';
               }
               p_db->dir_mode = str2mode_t(p_db->dir_mode_str);
            }
            break;

         case 'd' : /* Target directory on remote host. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr, _("ERROR   : No target directory for option -d.\n"));
               correct = NO;
            }
            else
            {
               argv++;
               (void)my_strncpy(p_db->remote_dir, argv[0], MAX_PATH_LENGTH);
               argc--;
            }
            break;

         case 'f' : /* Configuration file for filenames. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr, _("ERROR   : No filename file specified for option -f.\n"));
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
               (void)fprintf(stderr, _("ERROR   : No host name or IP number specified for option -h.\n"));
               correct = NO;
            }
            else
            {
               argv++;
               (void)my_strncpy(p_db->hostname, argv[0], MAX_FILENAME_LENGTH);
               argc--;
            }
            break;

         case 'l' : /* Lock type. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr, _("ERROR   : No lock type specified for option -l.\n"));
               correct = NO;
            }
            else
            {
               argv++;
               argc--;

               if (name[0] == 'r')
               {
                  (void)fprintf(stderr, _("ERROR   : This option is only for %s.\n"), &name[1]);
                  correct = NO;
               }
               else
               {
                  /* Check which lock type is specified. */
                  if (my_strcmp(argv[0], LOCK_DOT) == 0)
                  {
                     p_db->lock = DOT;
                  }
                  else if (CHECK_STRCMP(ptr, LOCK_DOT_VMS) == 0)
                       {
                          p_db->lock = DOT_VMS;
                       }
                  else if (my_strcmp(argv[0], LOCK_OFF) == 0)
                       {
                          p_db->lock = OFF;
                       }
                       else
                       {
                          (void)my_strncpy(p_db->lock_notation, argv[0],
                                           MAX_FILENAME_LENGTH);
                       }
               }
            }
            break;

         case 'n' : /* Specify the number of files. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr, _("ERROR   : No number of files specified for option -n.\n"));
               correct = NO;
            }
            else
            {
               if (name[0] != 't')
               {
                  char *p_name;

                  if (name[0] == 'r')
                  {
                     p_name = &name[1];
                  }
                  else
                  {
                     p_name = name;
                  }
                  (void)fprintf(stderr, _("ERROR   : This option is only for t%s.\n"), p_name);
                  correct = NO;
               }
               else
               {
                  p_db->no_of_files = atoi(*(argv + 1));
               }
               argc--;
               argv++;
            }
            break;

         case 'o' : /* Change mode of file. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr, _("ERROR   : No mode number specified for option -o.\n"));
               correct = NO;
            }
            else
            {
               int n;

               ptr = *(argv + 1);
               n = 0;
               while ((*ptr != '\n') && (*ptr != '\0') && (n < 4) &&
                      (isdigit((int)(*ptr))))
               {
                  p_db->chmod_str[n] = *ptr;
                  ptr++; n++;
               }
               if (n > 1)
               {
                  p_db->chmod_str[n] = '\0';
                  p_db->chmod = str2mode_t(p_db->chmod_str);
               }
               else
               {
                  (void)fprintf(stderr, _("ERROR   : Not a correct mode number for option -o.\n"));
                  correct = NO;
               }
               argc--;
               argv++;
            }
            break;

         case 'p' : /* Remote TCP port number. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr, _("ERROR   : No port number specified for option -p.\n"));
               correct = NO;
            }
            else
            {
               p_db->port = atoi(*(argv + 1));
               argc--;
               argv++;
            }
            break;

         case 'u' : /* User name and password for remote login. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr,
                             _("ERROR   : No user and password specified for option -u.\n"));
               correct = NO;
            }
            else
            {
               argv++;
               (void)my_strncpy(p_db->user, argv[0], MAX_USER_NAME_LENGTH);
               argc--;

               /* If user is specified a password must be there as well! */
               if ((argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  (void)fprintf(stderr,
                                _("ERROR   : No password specified for option -u.\n"));
                  correct = NO;
               }
               else
               {
                  argv++;
                  (void)my_strncpy(p_db->password, argv[0],
                                   MAX_USER_NAME_LENGTH);
                  argc--;
               }
            }
            break;

         case 'r' : /* Remove file that was transmitted. */
            p_db->remove = YES;
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

         case 's' : /* Dummy file size. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr, _("ERROR   : No file size specified for option -s.\n"));
               correct = NO;
            }
            else
            {
               if (name[0] != 't')
               {
                  char *p_name;

                  if (name[0] == 'r')
                  {
                     p_name = &name[1];
                  }
                  else
                  {
                     p_name = name;
                  }
                  (void)fprintf(stderr, _("ERROR   : This option is only for t%s.\n"), p_name);
                  correct = NO;
               }
               else
               {
                  p_db->dummy_size = atoi(*(argv + 1));
               }
               argc--;
               argv++;
            }
            break;

         case 't' : /* SFTP timeout. */
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

         case 'v' : /* Verbose mode. */
            p_db->verbose = YES;
            break;

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
           if (name[0] == 't')
           {
              if (p_db->filename == NULL)
              {
                 RT_ARRAY(p_db->filename, 1, MAX_PATH_LENGTH, char);
                 (void)my_strncpy(p_db->filename[0], argv[1], MAX_PATH_LENGTH);
              }
              else
              {
                 /* Ignore what ever the dummy user has written. */;
              }
           }
           else
           {
              int i = p_db->no_of_files;

              if (i == 0)
              {
                 RT_ARRAY(p_db->filename, argc, MAX_PATH_LENGTH, char);
              }
              else
              {
                 REALLOC_RT_ARRAY(p_db->filename, (i + argc),
                                  MAX_PATH_LENGTH, char);
              }
              p_db->no_of_files += argc - 1;
              while ((--argc > 0) && ((*++argv)[0] != '-'))
              {
                 (void)my_strncpy(p_db->filename[i], argv[0], MAX_PATH_LENGTH);
                 i++;
              }
           }
        }

   /* If input is not correct show syntax. */
   if (correct == NO)
   {
      usage();
      exit(SYNTAX_ERROR);
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(void)
{
   char *p_name;

   if ((name[0] == 'r') || (name[0] == 't'))
   {
      p_name = &name[1];
   }
   else
   {
      p_name = name;
   }
   (void)fprintf(stderr, _("SYNTAX: [t|r]%s [options] [file 1 ... file n]\n\n"), p_name);
   (void)fprintf(stderr, _("   When calling it with r%s files will be retrieved from the\n\
   given host, otherwise (when using %s) files will be send to that host.\n\n"), p_name, p_name);
   (void)fprintf(stderr, _("  OPTIONS                              DESCRIPTION\n"));
   (void)fprintf(stderr, _("  --version                          - Show current version\n"));
   if (name[0] == 'r')
   {
      (void)fprintf(stderr, _("  -A                                 - If only part of a file was retrieved, you\n\
                                       can retrieve the rest with this option.\n"));
   }
   if ((name[0] != 'r') && (name[0] != 't'))
   {
      (void)fprintf(stderr,
                     _("  -a                                 - Append file.\n"));
   }
   (void)fprintf(stderr, _("  -b <block size>                    - Transfer block size in byte. Default %d\n\
                                       byte.\n"), DEFAULT_TRANSFER_BLOCKSIZE);
   (void)fprintf(stderr, _("  -c <config file>                   - Configuration file holding user name,\n\
                                       password and target directory in URL\n\
                                       format.\n"));
   (void)fprintf(stderr, _("  -C[ <mode>]                        - If target directory does not exist create\n\
                                       it. The optional mode can be used to\n\
                                       set the permission of this directory.\n"));
   (void)fprintf(stderr, _("  -d <remote directory>              - Directory where file(s) are to be stored.\n"));
   (void)fprintf(stderr, _("  -f <filename>                      - File containing a list of filenames\n\
                                       that are to be send.\n"));
   (void)fprintf(stderr, _("  -h <host name | IP number>         - Hostname or IP number to which to\n\
                                       send the file(s).\n"));
   if (name[0] != 'r')
   {
      (void)fprintf(stderr, _("  -l <DOT | DOT_VMS | OFF | xyz.>    - How to lock the file on the remote site.\n"));
   }
   if (name[0] == 't')
   {
      (void)fprintf(stderr, _("  -n <number of files>               - Number of files to be transfered.\n"));
   }
   if (name[0] != 'r')
   {
      (void)fprintf(stderr, _("  -o <mode>                          - Changes the permission of each file\n\
                                       distributed.\n"));
   }
   (void)fprintf(stderr, _("  -p <port number>                   - Remote port number of SFTP-server.\n"));
   (void)fprintf(stderr, _("  -u <user> <password>               - Remote user name and password.\n"));
   if (name[0] == 'r')
   {
      (void)fprintf(stderr, _("  -R <buffer size>                   - Socket receive buffer size\n\
                                       (in bytes).\n"));
      (void)fprintf(stderr, _("  -r                                 - Remove remote file after it was\n\
                                       retrieved.\n"));
   }
   else
   {
      (void)fprintf(stderr, _("  -r                                 - Remove transmitted file.\n"));
      (void)fprintf(stderr, _("  -S <buffer size>                   - Socket send buffer size\n\
                                       (in bytes).\n"));
   }
   if (name[0] == 't')
   {
      (void)fprintf(stderr, _("  -s <file size>                     - File size of file to be transfered.\n"));
   }
   (void)fprintf(stderr, _("  -t <timout>                        - SFTP timeout in seconds. Default %lds.\n"), DEFAULT_TRANSFER_TIMEOUT);
   (void)fprintf(stderr, _("  -v                                 - Verbose. Shows all SFTP commands and\n\
                                       the reply from the remote server.\n"));
   (void)fprintf(stderr, _("  -?                                 - Display this help and exit.\n"));
   (void)fprintf(stderr, _("  The following values are returned on exit:\n"));
   (void)fprintf(stderr, _("      %2d - File transmitted successfully.\n"), TRANSFER_SUCCESS);
   (void)fprintf(stderr, _("      %2d - Failed to connect.\n"), CONNECT_ERROR);
   (void)fprintf(stderr, _("      %2d - User name wrong.\n"), USER_ERROR);
   (void)fprintf(stderr, _("      %2d - Wrong password.\n"), PASSWORD_ERROR);
   (void)fprintf(stderr, _("      %2d - Failed to set ascii/binary mode.\n"), TYPE_ERROR);
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
   (void)fprintf(stderr, _("      %2d - Syntax wrong.\n"), SYNTAX_ERROR);
   (void)fprintf(stderr, _("      %2d - Set blocksize error.\n"), SET_BLOCKSIZE_ERROR);

   return;
}
