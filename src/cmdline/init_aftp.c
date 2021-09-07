/*
 *  init_aftp.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2021 Deutscher Wetterdienst (DWD),
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

#include "aftpdefs.h"

DESCR__S_M3
/*
 ** NAME
 **   init_aftp - checks syntax of input for process aftp
 **
 ** SYNOPSIS
 **   int init_aftp(int         argc,
 **                 char        *argv[],
 **                 struct data *p_db)
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
 **   24.09.1997 H.Kiehl    Created
 **   31.10.1997 T.Freyberg Error messages in german.
 **   17.07.2000 H.Kiehl    Added options -c and -f.
 **   30.07.2000 H.Kiehl    Added option -x.
 **   17.10.2000 H.Kiehl    Added support to retrieve files from a
 **                         remote host.
 **   14.09.2001 H.Kiehl    Added option -k.
 **   20.09.2001 H.Kiehl    Added option -A.
 **   17.02.2004 H.Kiehl    Added option -z.
 **   11.04.2004 H.Kiehl    Added option -Z.
 **   28.09.2004 H.Kiehl    Added option -C (create target dir).
 **   07.02.2005 H.Kiehl    Added DOT_VMS and DOS mode.
 **   12.08.2006 H.Kiehl    Added option -X.
 **   05.03.2008 H.Kiehl    Added option -o.
 **   06.03.2020 H.Kiehl    Added option -I.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* stderr, fprintf()                  */
#include <stdlib.h>                /* exit(), atoi()                     */
#include <string.h>                /* strcpy(), strerror(), strcmp()     */
#include <ctype.h>                 /* isdigit()                          */
#include <errno.h>
#include "cmdline.h"
#include "ftpdefs.h"

/* Global variables. */
extern int  no_of_host,
            sys_log_fd,
            fsa_id;
extern char *p_work_dir;

/* Local global variables. */
static char name[30];

/* Local function prototypes. */
static void usage(void);


/*############################ init_aftp() ##############################*/
int
init_aftp(int argc, char *argv[], struct data *p_db)
{
   int  correct = YES,                /* Was input/syntax correct?      */
        set_extended_mode = NO;
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
   p_db->file_size_offset  = -1;       /* No appending. */
   p_db->blocksize         = DEFAULT_TRANSFER_BLOCKSIZE;
   p_db->remote_dir[0]     = '\0';
   p_db->hostname[0]       = '\0';
   p_db->lock              = DOT;
   p_db->lock_notation[0]  = '.';
   p_db->lock_notation[1]  = '\0';
   p_db->transfer_mode     = 'I';
   p_db->ftp_mode          = ACTIVE_MODE;
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
   p_db->keepalive         = NO;
#endif
   p_db->port              = DEFAULT_FTP_PORT;
   (void)strcpy(p_db->user, DEFAULT_AFD_USER);
   (void)strcpy(p_db->password, DEFAULT_AFD_PASSWORD);
   p_db->remove            = NO;
   p_db->transfer_timeout  = DEFAULT_TRANSFER_TIMEOUT;
   p_db->verbose           = NO;
   p_db->append            = NO;
#ifdef WITH_SSL
   p_db->implicit_ftps     = NO;
   p_db->tls_auth          = NO;
   p_db->strict            = NO;
#endif
   p_db->create_target_dir = NO;
   p_db->dir_mode_str[0]   = '\0';
   if (name[0] == 't')
   {
      p_db->no_of_files    = 1;
      p_db->dummy_size     = DEFAULT_TRANSFER_BLOCKSIZE;
   }
   else
   {
      p_db->no_of_files    = 0;
   }
   p_db->filename          = NULL;
   p_db->realname          = NULL;
   p_db->sndbuf_size       = 0;
   p_db->rcvbuf_size       = 0;
   p_db->proxy_name[0]     = '\0';

   /* Evaluate all arguments with '-'. */
   while ((--argc > 0) && ((*++argv)[0] == '-'))
   {
      switch (*(argv[0] + 1))
      {
         case 'A' : /* Search for file localy for appending. */
            p_db->append = YES;
            break;

         case 'a' : /* Remote file size offset for appending. */
            if ((argc == 1) ||
                ((*(argv + 1)[0] == '-') && ((argv + 1)[0][1] != '2')))
            {
               (void)fprintf(stderr, _("ERROR   : No file size offset specified for option -a.\n"));
               correct = NO;
            }
            else
            {
               if ((name[0] == 'r') || (name[0] == 't'))
               {
                  (void)fprintf(stderr, _("ERROR   : This option is only for %s.\n"), &name[1]);
                  correct = NO;
               }
               else
               {
                  p_db->file_size_offset = (signed char)atoi(*(argv + 1));
               }
               argc--;
               argv++;
            }
            break;

         case 'b' : /* FTP transfer block size. */
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

#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
         case 'k' : /* Keep control connection alive. */
            p_db->keepalive = YES;
            break;
#endif

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

#ifdef WITH_SSL
         case 'I' : /* Implicit FTPS. */
            p_db->implicit_ftps = YES;
            break;
#endif

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
#ifdef WITH_READY_FILES
                  else if (my_strcmp(argv[0], READY_FILE_ASCII) == 0)
                       {
                          p_db->lock = READY_A_FILE;
                       }
                  else if (my_strcmp(argv[0], READY_FILE_BINARY) == 0)
                       {
                          p_db->lock = READY_B_FILE;
                       }
#endif /* WITH_READY_FILES */
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

         case 'm' : /* FTP transfer mode. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr, _("ERROR   : No transfer mode specified for option -m.\n"));
               correct = NO;
            }
            else
            {
               switch (*(argv + 1)[0])
               {
                  case 'a':
                  case 'A': /* ASCII mode. */
                     p_db->transfer_mode = 'A';
                     argv++;
                     argc--;
                     break;

                  case 'i':
                  case 'I':
                  case 'b':
                  case 'B': /* Bianary mode. */
                     p_db->transfer_mode = 'I';
                     argv++;
                     argc--;
                     break;

                  case 'd':
                  case 'D': /* DOS mode. */
                     p_db->transfer_mode = 'D';
                     argv++;
                     argc--;
                     break;

                  default : /* Wrong/unknown mode! */

                     (void)fprintf(stderr,
                                   _("ERROR   : Unknown FTP transfer mode <%c> specified for option -m.\n"),
                                   *(argv + 1)[0]);
                     correct = NO;
                     break;
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

         case 'P' : /* Use the given proxy procedure to login. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr, _("ERROR   : No proxy procedure for option -P.\n"));
               correct = NO;
            }
            else
            {
               argv++;
               (void)my_strncpy(p_db->proxy_name, argv[0],
                                MAX_PROXY_NAME_LENGTH + 1);
               argc--;
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

         case 't' : /* FTP timeout. */
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

         case 'x' : /* Use passive mode instead of active mode. */
            p_db->ftp_mode = PASSIVE_MODE;
            break;

         case 'X' : /* Set extended mode. */
            set_extended_mode = YES;
            break;

#ifdef WITH_SSL
         case 'Y' : /* TLS/SSL strict authentification. */
            p_db->strict = YES;
            break;

         case 'z' : /* SSL/TLS authentification for control connection only. */
            p_db->tls_auth = YES;
            break;

         case 'Z' : /* SSL/TLS authentification for control and data connection. */
            p_db->tls_auth = BOTH;
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

   if (set_extended_mode == YES)
   {
      p_db->ftp_mode |= EXTENDED_MODE;
   }

   if (p_db->ftp_mode & PASSIVE_MODE)
   {
      if (p_db->ftp_mode & EXTENDED_MODE)
      {
         /* Write "extended passive" */
         p_db->mode_str[0] = 'e'; p_db->mode_str[1] = 'x'; p_db->mode_str[2] = 't';
         p_db->mode_str[3] = 'e'; p_db->mode_str[4] = 'n'; p_db->mode_str[5] = 'd';
         p_db->mode_str[6] = 'e'; p_db->mode_str[7] = 'd'; p_db->mode_str[8] = ' ';
         p_db->mode_str[9] = 'p'; p_db->mode_str[10] = 'a';
         p_db->mode_str[11] = 's'; p_db->mode_str[12] = 's';
         p_db->mode_str[13] = 'i'; p_db->mode_str[14] = 'v';
         p_db->mode_str[15] = 'e'; p_db->mode_str[16] = '\0';
      }
      else
      {
         /* Write "passive" */
         p_db->mode_str[0] = 'p'; p_db->mode_str[1] = 'a'; p_db->mode_str[2] = 's';
         p_db->mode_str[3] = 's'; p_db->mode_str[4] = 'i'; p_db->mode_str[5] = 'v';
         p_db->mode_str[6] = 'e'; p_db->mode_str[7] = '\0';
      }
   }
   else
   {
      if (p_db->ftp_mode & EXTENDED_MODE)
      {
         /* Write "extended active" */
         p_db->mode_str[0] = 'e'; p_db->mode_str[1] = 'x'; p_db->mode_str[2] = 't';
         p_db->mode_str[3] = 'e'; p_db->mode_str[4] = 'n'; p_db->mode_str[5] = 'd';
         p_db->mode_str[6] = 'e'; p_db->mode_str[7] = 'd'; p_db->mode_str[8] = ' ';
         p_db->mode_str[9] = 'a'; p_db->mode_str[10] = 'c'; p_db->mode_str[11] = 't';
         p_db->mode_str[12] = 'i'; p_db->mode_str[13] = 'v'; p_db->mode_str[14] = 'e';
         p_db->mode_str[15] = '\0';
      }
      else
      {
         /* Write "active" */
         p_db->mode_str[0] = 'a'; p_db->mode_str[1] = 'c'; p_db->mode_str[2] = 't';
         p_db->mode_str[3] = 'i'; p_db->mode_str[4] = 'v'; p_db->mode_str[5] = 'e';
         p_db->mode_str[6] = '\0';
      }
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
                     _("  -a <file size offset>              - Offset of file name when doing a LIST\n\
                                       command on the remote side. If you\n\
                                       specify -2 it will try to determine\n\
                                       the size with the SIZE command.\n"));
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
#ifdef WITH_SSL
   (void)fprintf(stderr, _("  -I                                 - Enable implicit FTPS. Works only with -z or -Z.\n"));
#endif
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
   (void)fprintf(stderr, _("  -k                                 - Keep FTP control connection with STAT\n\
                                       calls alive/fresh.\n"));
#endif
   if (name[0] != 'r')
   {
#ifdef WITH_READY_FILES
      (void)fprintf(stderr, _("  -l <DOT | DOT_VMS | OFF | RDYA | RDYB | xyz> - How to lock the file on the remote site.\n"));
#else
      (void)fprintf(stderr, _("  -l <DOT | DOT_VMS | OFF | xyz.>    - How to lock the file on the remote site.\n"));
#endif
   }
   (void)fprintf(stderr, _("  -m <A | I | D>                     - FTP transfer mode, ASCII, binary or DOS.\n\
                                       Default is binary.\n"));
   if (name[0] == 't')
   {
      (void)fprintf(stderr, _("  -n <number of files>               - Number of files to be transfered.\n"));
   }
   if (name[0] != 'r')
   {
      (void)fprintf(stderr, _("  -o <mode>                          - Changes the permission of each file\n\
                                       distributed.\n"));
   }
   (void)fprintf(stderr, _("  -p <port number>                   - Remote port number of FTP-server.\n"));
   (void)fprintf(stderr, _("  -P <proxy procedure>               - Use the given proxy procedure to\n\
                                       login. See documentation for more\n\
                                       details on syntax.\n"));
   (void)fprintf(stderr, _("  -u <user> <password>               - Remote user name and password. If not\n\
                                       supplied, it will login as %s.\n"), DEFAULT_AFD_USER);
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
   (void)fprintf(stderr, _("  -t <timout>                        - FTP timeout in seconds. Default %lds.\n"), DEFAULT_TRANSFER_TIMEOUT);
   (void)fprintf(stderr, _("  -v                                 - Verbose. Shows all FTP commands and\n\
                                       the reply from the remote server.\n"));
   (void)fprintf(stderr, _("  -x                                 - Use passive mode instead of active\n\
                                       mode when doing the data connection.\n"));
   (void)fprintf(stderr, _("  -X                                 - Use extended mode active or passive\n\
                                       (-x) mode.\n"));
#ifdef WITH_SSL
   (void)fprintf(stderr, _("  -Y                                 - Use strict SSL/TLS checks.\n"));
   (void)fprintf(stderr, _("  -z                                 - Use SSL/TLS for control connection.\n"));
   (void)fprintf(stderr, _("  -Z                                 - Use SSL/TLS for control and data\n\
                                       connection.\n"));
#endif
   (void)fprintf(stderr, _("  -?                                 - Display this help and exit.\n"));
   (void)fprintf(stderr, _("  The following values are returned on exit:\n"));
   (void)fprintf(stderr, _("      %2d - File transmitted successfully.\n"), TRANSFER_SUCCESS);
   (void)fprintf(stderr, _("      %2d - Failed to connect.\n"), CONNECT_ERROR);
#ifdef WITH_SSL
   (void)fprintf(stderr, _("      %2d - SSL/TLS authentification error.\n"), AUTH_ERROR);
#endif
   (void)fprintf(stderr, _("      %2d - User name wrong.\n"), USER_ERROR);
   (void)fprintf(stderr, _("      %2d - Wrong password.\n"), PASSWORD_ERROR);
   (void)fprintf(stderr, _("      %2d - Failed to set ascii/binary mode.\n"), TYPE_ERROR);
   (void)fprintf(stderr, _("      %2d - Failed to send NLST command.\n"), LIST_ERROR);
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

   return;
}
