/*
 *  init_awmo.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2010 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   init_awmo - checks syntax of input for process awmo
 **
 ** SYNOPSIS
 **   int init_awmo(int         argc,
 **                 char        *argv[],
 **                 struct data *p_db)
 **
 ** DESCRIPTION
 **   This module checks whether the syntax of awmo is correct and
 **   stores these values into the structure job.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   13.11.2010 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* stderr, fprintf()                  */
#include <stdlib.h>                /* exit(), atoi()                     */
#include <string.h>                /* strerror(), strcmp()               */
#include <ctype.h>                 /* isdigit()                          */
#include <errno.h>
#include "cmdline.h"
#include "wmodefs.h"

/* Global variables. */
extern int  no_of_host,
            sys_log_fd,
            fsa_id;
extern char *p_work_dir;

/* Local global variables. */
static char name[30];

/* Local function prototypes. */
static void usage(void);


/*############################ init_awmo() ##############################*/
int
init_awmo(int argc, char *argv[], struct data *p_db)
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
   if (name[0] == 't')
   {
      p_db->exec_mode = TEST_MODE;
   }
   else
   {
      p_db->exec_mode = TRANSFER_MODE;
   }

   /* First initialize all values with default values. */
   p_db->acknowledge       = NO;
   p_db->blocksize         = DEFAULT_TRANSFER_BLOCKSIZE;
   p_db->hostname[0]       = '\0';
   p_db->transfer_mode     = 'I';
   p_db->port              = DEFAULT_WMO_PORT;
   p_db->remove            = NO;
   p_db->transfer_timeout  = DEFAULT_TRANSFER_TIMEOUT;
   p_db->verbose           = NO;
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
   p_db->special_flag      = 0;

   /* Evaluate all arguments with '-'. */
   while ((--argc > 0) && ((*++argv)[0] == '-'))
   {
      switch (*(argv[0] + 1))
      {
         case 'A' : /* . */
            p_db->acknowledge = YES;
            break;

         case 'b' : /* WMO transfer block size. */
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

         case 'H' : /* File name is header. */
            p_db->special_flag |= FILE_NAME_IS_HEADER;
            break;

         case 'm' : /* WMO transfer type. */
            if ((argc == 1) || (*(argv + 1)[0] == '-'))
            {
               (void)fprintf(stderr, _("ERROR   : No transfer type specified for option -m.\n"));
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

                  case 'f':
                  case 'F': /* FAX mode. */
                     p_db->transfer_mode = 'F';
                     argv++;
                     argc--;
                     break;

                  default : /* Wrong/unknown mode! */

                     (void)fprintf(stderr,
                                   _("ERROR   : Unknown WMO transfer type <%c> specified for option -m.\n"),
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

         case 'r' : /* Remove file that was transmitted. */
            p_db->remove = YES;
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

         case 't' : /* WMO timeout. */
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
   (void)fprintf(stderr, _("SYNTAX: [t]%s [options] [file 1 ... file n]\n\n"), p_name);
   (void)fprintf(stderr, _("  OPTIONS                              DESCRIPTION\n"));
   (void)fprintf(stderr, _("  --version                          - Show current version\n"));
   (void)fprintf(stderr, _("  -a                                 - Wait for an acknowledge from server.\n"));
   (void)fprintf(stderr, _("  -b <block size>                    - Transfer block size in bytes. Default %d\n\
                                       bytes.\n"), DEFAULT_TRANSFER_BLOCKSIZE);
   (void)fprintf(stderr, _("  -f <filename>                      - File containing a list of filenames\n\
                                       that are to be send.\n"));
   (void)fprintf(stderr, _("  -h <host name | IP number>         - Hostname or IP number to which to\n\
                                       send the file(s).\n"));
   (void)fprintf(stderr, _("  -H                                 - File name is header\n"));
   (void)fprintf(stderr, _("  -m <A | I | F>                     - WMO transfer type, ASCII, binary or Fax.\n\
                                       Default is binary.\n"));
   if (name[0] == 't')
   {
      (void)fprintf(stderr, _("  -n <number of files>               - Number of files to be transfered.\n"));
   }
   (void)fprintf(stderr, _("  -p <port number>                   - Remote port number of WMO-server.\n"));
   (void)fprintf(stderr, _("  -r                                 - Remove transmitted file.\n"));
   (void)fprintf(stderr, _("  -S <buffer size>                   - Socket send buffer size\n\
                                    (in bytes).\n"));
   if (name[0] == 't')
   {
      (void)fprintf(stderr, _("  -s <file size>                     - File size of file to be transfered.\n"));
   }
   (void)fprintf(stderr, _("  -t <timout>                        - WMO timeout in seconds. Default %lds.\n"), DEFAULT_TRANSFER_TIMEOUT);
   (void)fprintf(stderr, _("  -v                                 - Verbose. Shows all WMO commands and\n\
                                       the reply from the remote server.\n"));
   (void)fprintf(stderr, _("  -?                                 - Display this help and exit.\n"));
   (void)fprintf(stderr, _("  The following values are returned on exit:\n"));
   (void)fprintf(stderr, _("      %2d - File transmitted successfully.\n"), TRANSFER_SUCCESS);
   (void)fprintf(stderr, _("      %2d - Failed to connect.\n"), CONNECT_ERROR);
   (void)fprintf(stderr, _("      %2d - Failed to open remote file.\n"), OPEN_REMOTE_ERROR);
   (void)fprintf(stderr, _("      %2d - Error when writing into remote file.\n"), WRITE_REMOTE_ERROR);
   (void)fprintf(stderr, _("      %2d - Failed to close remote file.\n"), CLOSE_REMOTE_ERROR);
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
