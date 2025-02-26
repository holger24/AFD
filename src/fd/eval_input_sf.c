/*
 *  eval_input_sf.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2025 Deutscher Wetterdienst (DWD),
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

#include "afddefs.h"

DESCR__S_M3
/*
 ** NAME
 **   eval_input_sf - checks syntax of input for process sf_xxx
 **
 ** SYNOPSIS
 **   int eval_input_sf(int argc, char *argv[], struct job *p_db)
 **
 ** DESCRIPTION
 **   This function evaluates the parameters given to the process
 **   sf_xxx which may have the following format:
 **
 **    sf_xxx <work dir> <job no.> <FSA id> <FSA pos> <msg name> [options]
 **
 **          -a <age limit>            The age limit for the files being send.
 **          -A                        Disable archiving of files.
 **          -c                        Enable support for hardware CRC-32.
 **          -C <charset>              Default charset to use.
 **          -D <DE-mail sender>       The sender DE-mail address.
 **          -e <seconds>              Disconnect after given time.
 **          -f <SMTP from>            Default from identifier to send.
 **          -g <group mail domain>    Group mail domain.
 **          -h <HTTP proxy>[:<port>]  Proxy where to send the HTTP requests.
 **          -m <mode>                 Create target dir mode.
 **          -o <retries>              Old/Error message and number of retries.
 **          -r                        Resend from archive (job from show_olog).
 **          -R <SMTP reply-to>        Default reply-to identifier to send.
 **          -s <SMTP server>[:<port>] Server where to send the mails.
 **          -S                        Simulation mode.
 **          -t                        Temp toggle.
 **
 ** RETURN VALUES
 **   Returns SUCCESS when it successfully decoded the parameters.
 **   SYNTAX_ERROR is returned when it thinks one of the parameter
 **   is wrong or it did not manage to attach to the FSA. JID_NUMBER_ERROR
 **   is returned when it failed to determine the job id number.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.11.1995 H.Kiehl Created
 **   30.10.2002 H.Kiehl Added -r option.
 **   12.11.2002 H.Kiehl Option -a to supply the default age-limit.
 **   19.03.2003 H.Kiehl Added -A to disable archiving.
 **   21.03.2003 H.Kiehl Rewrite to accomodate to new syntax.
 **   30.08.2005 H.Kiehl Added -s to specify mail server name.
 **   22.01.2008 H.Kiehl Added -R to specify a default reply-to identifier.
 **   28.12.2011 H.Kiehl Added -h to specify HTTP proxy.
 **   23.11.2012 H.Kiehl Added -c for hardware CRC-32 support.
 **   15.09.2014 H.Kiehl Added -S for simulation mode.
 **   01.07.2015 H.Kiehl Added -e for disconnect time.
 **   16.07.2017 H.Kiehl Added -m for create target dir mode.
 **   24.08.2017 H.Kiehl Added -C to specify the default charset.
 **   22.06.2020 H.Kiehl Added -g to set group mail domain.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* stderr, fprintf()                  */
#include <stdlib.h>                /* exit(), strtoul(), malloc()        */
#include <string.h>                /* strerror()                         */
#include <ctype.h>                 /* isdigit()                          */
#include <errno.h>
#include "fddefs.h"

/* Global variables. */
extern int                        fsa_id,
#ifdef HAVE_HW_CRC32
                                  have_hw_crc32,
#endif
                                  *p_no_of_hosts,
                                  simulation_mode;
extern char                       *p_work_dir;
extern struct filetransfer_status *fsa;

/* Local function prototypes. */
static void                       usage(char *, unsigned int);


/*########################### eval_input_sf() ###########################*/
int
eval_input_sf(int argc, char *argv[], struct job *p_db)
{
   int ret = SUCCESS;

   if (argc < 6)
   {
      usage(argv[0], p_db->protocol);
      ret = SYNTAX_ERROR;
   }
   else
   {
      if (p_work_dir == NULL)
      {
         size_t length;

         length = strlen(argv[1]) + 1;
         if ((p_work_dir = malloc(length)) == NULL)
         {
            (void)fprintf(stderr,
#if SIZEOF_SIZE_T == 4
                          "ERROR   : Failed to malloc() %d bytes : %s",
#else
                          "ERROR   : Failed to malloc() %lld bytes : %s",
#endif
                          (pri_size_t)length, strerror(errno));
            ret = ALLOC_ERROR;
            return(-ret);
         }
         (void)memcpy(p_work_dir, argv[1], length);
      }
      if (isdigit((int)(argv[2][0])) == 0)
      {
         (void)fprintf(stderr,
                       "ERROR   : None nummeric value for job number : %s.\n",
                       argv[2]);
         usage(argv[0], p_db->protocol);
         ret = SYNTAX_ERROR;
      }
      else
      {
         int i;

         p_db->job_no = (char)strtol(argv[2], (char **)NULL, 10);

         /* Check if FSA ID is correct. */
         i = 0;
         do
         {
            if (isdigit((int)(argv[3][i])) == 0)
            {
               i = MAX_INT_LENGTH;
            }
            i++;
         } while ((argv[3][i] != '\0') && (i < MAX_INT_LENGTH));
         if ((i > 0) && (i < MAX_INT_LENGTH))
         {
            fsa_id = (int)strtol(argv[3], (char **)NULL, 10);

            /* Check if FSA position is correct. */
            i = 0;
            do
            {
               if (isdigit((int)(argv[4][i])) == 0)
               {
                  i = MAX_INT_LENGTH;
               }
               i++;
            } while ((argv[4][i] != '\0') && (i < MAX_INT_LENGTH));
            if ((i > 0) && (i < MAX_INT_LENGTH))
            {
               int msg_length;

               p_db->fsa_pos = (int)strtol(argv[4], (char **)NULL, 10);

               /* Check if the supplied message name is correct. */
               i = 0;
               do
               {
                  if ((isxdigit((int)(argv[5][i]))) || (argv[5][i] == '_') ||
                      (argv[5][i] == '/'))
                  {
                     p_db->msg_name[i] = argv[5][i];
                  }
                  else
                  {
                     i = MAX_MSG_NAME_LENGTH;
                  }
                  i++;
               } while ((argv[5][i] != '\0') && (i < MAX_MSG_NAME_LENGTH));
               msg_length = i;
               if ((i > 0) && (i < MAX_MSG_NAME_LENGTH))
               {
                  char *p_job_id,
                       *ptr;

                  p_db->msg_name[msg_length] = '\0';
                  ptr = p_db->msg_name;
                  while ((*ptr != '/') && (*ptr != '\0'))
                  {
                     ptr++;
                  }
#ifdef MULTI_FS_SUPPORT
                  if (*ptr != '/')
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Failed to filesystem ID in message name %s",
                                argv[5]);
                     ret = JID_NUMBER_ERROR;
                     return(-ret);
                  }
                  ptr++;
                  p_job_id = ptr;
                  while ((*ptr != '/') && (*ptr != '\0'))
                  {
                     ptr++;
                  }
#else
                  p_job_id = p_db->msg_name;
#endif
                  if (*ptr != '/')
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Failed to locate job ID in message name %s",
                                argv[5]);
                     ret = JID_NUMBER_ERROR;
                  }
                  else
                  {
                     *ptr = '\0';
                     p_db->id.job = (unsigned int)strtoul(p_job_id, (char **)NULL, 16);
                     if (fsa_attach_pos(p_db->fsa_pos) == SUCCESS)
                     {
                        /*
                         * Now lets evaluate the options.
                         */
                        for (i = 6; i < argc; i++)
                        {
                           if (argv[i][0] == '-')
                           {
                              switch (argv[i][1])
                              {
                                 case 'a' : /* Default age-limit. */
                                    if (((i + 1) < argc) &&
                                        (argv[i + 1][0] != '-'))
                                    {
                                       int k = 0;

                                       i++;
                                       do
                                       {
                                          if ((isdigit((int)(argv[i][k]))) == 0)
                                          {
                                             k = MAX_INT_LENGTH;
                                          }
                                          k++;
                                       } while ((argv[i][k] != '\0') &&
                                                (k < MAX_INT_LENGTH));
                                       if ((k > 0) && (k < MAX_INT_LENGTH))
                                       {
                                          p_db->age_limit = (unsigned int)strtoul(argv[i], (char **)NULL, 10);
                                       }
                                       else
                                       {
                                          (void)fprintf(stderr,
                                                        "ERROR   : Hmm, could not find the age limit for -a option.\n");
                                       }
                                    }
                                    else
                                    {
                                       (void)fprintf(stderr,
                                                     "ERROR   : No age limit specified for -a option.\n");
                                       usage(argv[0], p_db->protocol);
                                       ret = SYNTAX_ERROR;
                                    }
                                    break;
                                 case 'A' : /* Archiving is disabled. */
                                    p_db->archive_time = -1;
                                    break;
#ifdef HAVE_HW_CRC32
                                 case 'c' : /* CPU supports CRC32 in HW. */
                                    have_hw_crc32 = YES;
                                    break;
#endif
                                 case 'C' : /* Charset. */
                                    if (((i + 1) < argc) &&
                                        (argv[i + 1][0] != '-'))
                                    {
                                       size_t length;

                                       i++;
                                       length = strlen(argv[i]) + 1;
                                       if ((p_db->default_charset = malloc(length)) == NULL)
                                       {
                                          (void)fprintf(stderr,
# if SIZEOF_SIZE_T == 4
                                                        "ERROR   : Failed to malloc() %d bytes : %s",
# else
                                                        "ERROR   : Failed to malloc() %lld bytes : %s",
# endif
                                                        (pri_size_t)length,
                                                        strerror(errno));
                                          ret = ALLOC_ERROR;
                                       }
                                       else
                                       {
                                          (void)strcpy(p_db->default_charset, argv[i]);
                                       }
                                    }
                                    else
                                    {
                                       (void)fprintf(stderr,
                                                     "ERROR   : No default charset specified for -C option.\n");
                                       usage(argv[0], p_db->protocol);
                                       ret = SYNTAX_ERROR;
                                    }
                                    break;
#ifdef _WITH_DE_MAIL_SUPPORT
                                 case 'D' : /* DE-Mail sender address. */
                                    if (((i + 1) < argc) &&
                                        (argv[i + 1][0] != '-'))
                                    {
                                       size_t length;

                                       i++;
                                       length = strlen(argv[i]) + 1;
                                       if ((p_db->de_mail_sender = malloc(length)) == NULL)
                                       {
                                          (void)fprintf(stderr,
# if SIZEOF_SIZE_T == 4
                                                        "ERROR   : Failed to malloc() %d bytes : %s",
# else
                                                        "ERROR   : Failed to malloc() %lld bytes : %s",
# endif
                                                        (pri_size_t)length,
                                                        strerror(errno));
                                          ret = ALLOC_ERROR;
                                       }
                                       else
                                       {
                                          (void)strcpy(p_db->de_mail_sender, argv[i]);
                                       }
                                    }
                                    else
                                    {
                                       (void)fprintf(stderr,
                                                     "ERROR   : No DE-Mail sender address specified for -D option.\n");
                                       usage(argv[0], p_db->protocol);
                                       ret = SYNTAX_ERROR;
                                    }
                                    break;
#endif /* _WITH_DE_MAIL_SUPPORT */
                                 case 'e' : /* Disconnect after given time. */
                                    if (((i + 1) < argc) &&
                                        (argv[i + 1][0] != '-'))
                                    {
                                       p_db->disconnect = (unsigned int)strtoul(argv[i + 1], (char **)NULL, 10);
                                    }
                                    else
                                    {
                                       (void)fprintf(stderr,
                                                     "ERROR   : No disconnect time specified for -d option.\n");
                                       usage(argv[0], p_db->protocol);
                                       ret = SYNTAX_ERROR;
                                    }
                                    break;
                                 case 'f' : /* Default SMTP from. */
                                    if (((i + 1) < argc) &&
                                        (argv[i + 1][0] != '-'))
                                    {
                                       size_t length;

                                       i++;
                                       length = strlen(argv[i]) + 1;
                                       if ((p_db->default_from = malloc(length)) == NULL)
                                       {
                                          (void)fprintf(stderr,
#if SIZEOF_SIZE_T == 4
                                                        "ERROR   : Failed to malloc() %d bytes : %s",
#else
                                                        "ERROR   : Failed to malloc() %lld bytes : %s",
#endif
                                                        (pri_size_t)length,
                                                        strerror(errno));
                                          ret = ALLOC_ERROR;
                                       }
                                       else
                                       {
                                          (void)strcpy(p_db->default_from, argv[i]);
                                       }
                                    }
                                    else
                                    {
                                       (void)fprintf(stderr,
                                                     "ERROR   : No default SMTP from specified for -f option.\n");
                                       usage(argv[0], p_db->protocol);
                                       ret = SYNTAX_ERROR;
                                    }
                                    break;
                                 case 'g' : /* Group mail domain. */
                                    if (((i + 1) < argc) &&
                                        (argv[i + 1][0] != '-'))
                                    {
                                       size_t length;

                                       i++;
                                       length = strlen(argv[i]) + 1;
                                       if ((p_db->group_mail_domain = malloc(length)) == NULL)
                                       {
                                          (void)fprintf(stderr,
# if SIZEOF_SIZE_T == 4
                                                        "ERROR   : Failed to malloc() %d bytes : %s",
# else
                                                        "ERROR   : Failed to malloc() %lld bytes : %s",
# endif
                                                        (pri_size_t)length,
                                                        strerror(errno));
                                          ret = ALLOC_ERROR;
                                       }
                                       else
                                       {
                                          (void)strcpy(p_db->group_mail_domain, argv[i]);
                                       }
                                    }
                                    else
                                    {
                                       (void)fprintf(stderr,
                                                     "ERROR   : No mail group domain specified for -g option.\n");
                                       usage(argv[0], p_db->protocol);
                                       ret = SYNTAX_ERROR;
                                    }
                                    break;
                                 case 'h' : /* Default HTTP proxy. */
                                    if (((i + 1) < argc) &&
                                        (argv[i + 1][0] != '-'))
                                    {
                                       int k = 0;

                                       i++;
                                       while ((argv[i][k] != '\0') &&
                                              (argv[i][k] != ':') &&
                                              (k < MAX_REAL_HOSTNAME_LENGTH))
                                       {
                                          p_db->http_proxy[k] = argv[i][k];
                                          k++;
                                       }
                                       if ((k > 0) &&
                                           (k < MAX_REAL_HOSTNAME_LENGTH))
                                       {
                                          p_db->http_proxy[k] = '\0';
                                          if (argv[i][k] == ':')
                                          {
                                             p_db->port = atoi(&argv[i][k + 1]);
                                          }
                                       }
                                       else
                                       {
                                          if (k == 0)
                                          {
                                             (void)fprintf(stderr,
                                                           "ERROR   : No default HTTP proxy specified for -h option.\n");
                                          }
                                          else
                                          {
                                             (void)fprintf(stderr,
                                                           "ERROR   : Default HTTP proxy specified for -h option is to long, may only be %d bytes long.\n",
                                                           MAX_REAL_HOSTNAME_LENGTH);
                                          }
                                          usage(argv[0], p_db->protocol);
                                          ret = SYNTAX_ERROR;
                                       }
                                    }
                                    else
                                    {
                                       (void)fprintf(stderr,
                                                     "ERROR   : No default HTTP proxy specified for -h option.\n");
                                       usage(argv[0], p_db->protocol);
                                       ret = SYNTAX_ERROR;
                                    }
                                    break;
                                 case 'm' : /* The mode with which remote dirs should be created. */
                                    if (((i + 1) < argc) &&
                                        (argv[i + 1][0] != '-'))
                                    {
                                       int k = 0;

                                       i++;
                                       do
                                       {
                                          if ((argv[i][k] >= '0') &&
                                              (argv[i][k] <= '7'))
                                          {
                                             p_db->dir_mode_str[k] = argv[i][k];
                                             k++;
                                          }
                                          else
                                          {
                                             k = 5;
                                          }
                                       } while ((argv[i][k] != '\0') &&
                                                (k < 5));
                                       if ((k > 0) && (k < 5))
                                       {
                                          p_db->dir_mode = (unsigned int)strtoul(argv[i], (char **)NULL, 8);
                                          p_db->dir_mode_str[k] = '\0';
                                          p_db->special_flag |= CREATE_TARGET_DIR;
                                       }
                                       else
                                       {
                                          (void)fprintf(stderr,
                                                        "ERROR   : Hmm, could not find or evaluate the mode (%s) for -m option.\n",
                                                        argv[i]);
                                          p_db->dir_mode_str[0] = '0';
                                          p_db->dir_mode_str[1] = '\0';
                                       }
                                    }
                                    else
                                    {
                                       (void)fprintf(stderr,
                                                     "ERROR   : No mode specified for -m option.\n");
                                       usage(argv[0], p_db->protocol);
                                       ret = SYNTAX_ERROR;
                                    }
                                    break;
                                 case 'R' : /* Default SMTP reply-to. */
                                    if (((i + 1) < argc) &&
                                        (argv[i + 1][0] != '-'))
                                    {
                                       size_t length;

                                       i++;
                                       length = strlen(argv[i]) + 1;
                                       if ((p_db->reply_to = malloc(length)) == NULL)
                                       {
                                          (void)fprintf(stderr,
#if SIZEOF_SIZE_T == 4
                                                        "ERROR   : Failed to malloc() %d bytes : %s",
#else
                                                        "ERROR   : Failed to malloc() %lld bytes : %s",
#endif
                                                        (pri_size_t)length,
                                                        strerror(errno));
                                          ret = ALLOC_ERROR;
                                       }
                                       else
                                       {
                                          (void)strcpy(p_db->reply_to, argv[i]);
                                       }
                                    }
                                    else
                                    {
                                       (void)fprintf(stderr,
                                                     "ERROR   : No default SMTP reply-to specified for -R option.\n");
                                       usage(argv[0], p_db->protocol);
                                       ret = SYNTAX_ERROR;
                                    }
                                    break;
                                 case 'r' : /* This is a resend from archive. */
                                    p_db->resend = YES;
                                    break;
                                 case 'o' : /* This is an old/error job. */
                                    p_db->special_flag |= OLD_ERROR_JOB;
                                    if (((i + 1) < argc) &&
                                        (argv[i + 1][0] != '-'))
                                    {
                                       int k = 0;

                                       i++;
                                       do
                                       {
                                          if (isdigit((int)(argv[i][k])) == 0)
                                          {
                                             k = MAX_INT_LENGTH;
                                          }
                                          k++;
                                       } while ((argv[i][k] != '\0') &&
                                                (k < MAX_INT_LENGTH));
                                       if ((k > 0) && (k < MAX_INT_LENGTH))
                                       {
                                          p_db->retries = (unsigned int)strtoul(argv[i], (char **)NULL, 10);
                                       }
                                       else
                                       {
                                          (void)fprintf(stderr,
                                                        "ERROR   : Hmm, could not find the retries for -o option.\n");
                                       }
                                    }
                                    else
                                    {
                                       (void)fprintf(stderr,
                                                     "ERROR   : No retries specified for -o option.\n");
                                       usage(argv[0], p_db->protocol);
                                       ret = SYNTAX_ERROR;
                                    }
                                    break;
                                 case 's' : /* Default SMTP server. */
                                    if (((i + 1) < argc) &&
                                        (argv[i + 1][0] != '-'))
                                    {
                                       int k = 0;

                                       i++;
                                       while ((argv[i][k] != '\0') &&
                                              (argv[i][k] != ':') &&
                                              (k < MAX_REAL_HOSTNAME_LENGTH))
                                       {
                                          p_db->smtp_server[k] = argv[i][k];
                                          k++;
                                       }
                                       if ((k > 0) &&
                                           (k < MAX_REAL_HOSTNAME_LENGTH))
                                       {
                                          p_db->smtp_server[k] = '\0';
                                          p_db->special_flag |= SMTP_SERVER_NAME_IN_AFD_CONFIG;
                                          if (argv[i][k] == ':')
                                          {
                                             p_db->port = atoi(&argv[i][k + 1]);
                                          }
                                       }
                                       else
                                       {
                                          if (k == 0)
                                          {
                                             (void)fprintf(stderr,
                                                           "ERROR   : No default SMTP server specified for -s option.\n");
                                          }
                                          else
                                          {
                                             (void)fprintf(stderr,
                                                           "ERROR   : Default SMTP server specified for -s option is to long, may only be %d bytes long.\n",
                                                           MAX_REAL_HOSTNAME_LENGTH);
                                          }
                                          usage(argv[0], p_db->protocol);
                                          ret = SYNTAX_ERROR;
                                       }
                                    }
                                    else
                                    {
                                       (void)fprintf(stderr,
                                                     "ERROR   : No default SMTP server specified for -s option.\n");
                                       usage(argv[0], p_db->protocol);
                                       ret = SYNTAX_ERROR;
                                    }
                                    break;
                                 case 'S' : /* Simulate sending data. */
                                    simulation_mode = YES;
                                    break;
                                 case 't' : /* Toggle host. */
                                    p_db->toggle_host = YES;
                                    break;
                                 default  : /* Unknown parameter. */
                                    (void)fprintf(stderr,
                                                  "ERROR  : Unknown parameter %c. (%s %d)\n",
                                                  argv[i][1], __FILE__, __LINE__);
                                    break;
                              }
                           }
                        }
                        if (*(unsigned char *)((char *)p_no_of_hosts + AFD_FEATURE_FLAG_OFFSET_START) & ENABLE_CREATE_TARGET_DIR)
                        {
                           p_db->special_flag |= CREATE_TARGET_DIR;
                        }
#ifdef WITH_DUP_CHECK
                        p_db->dup_check_flag = fsa->dup_check_flag;
                        p_db->dup_check_timeout = fsa->dup_check_timeout;
#endif
                        if (ret == SUCCESS)
                        {
                           char fullname[MAX_PATH_LENGTH];

                           (void)snprintf(fullname, MAX_PATH_LENGTH, "%s%s/%s",
                                          p_work_dir, AFD_MSG_DIR, p_job_id);
                           if (eval_message(fullname, p_db) < 0)
                           {
                              ret = SYNTAX_ERROR;
                           }
                           else
                           {
                              *ptr = '/';
                           }
                        }
                     }
                     else
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Failed to attach to FSA.");
                        ret = SYNTAX_ERROR;
                     }
                  }
               }
               else
               {
                  (void)fprintf(stderr,
                                "ERROR   : Wrong message name : %s.\n",
                                argv[5]);
                  usage(argv[0], p_db->protocol);
                  ret = SYNTAX_ERROR;
               }
            }
            else
            {
               (void)fprintf(stderr,
                             "ERROR   : Wrong value for FSA position : %s.\n",
                             argv[4]);
               usage(argv[0], p_db->protocol);
               ret = SYNTAX_ERROR;
            }
         }
         else
         {
            (void)fprintf(stderr,
                          "ERROR   : Wrong value for FSA ID : %s.\n", argv[3]);
            usage(argv[0], p_db->protocol);
            ret = SYNTAX_ERROR;
         }
      }
   }
   if (ret != SUCCESS)
   {
      ret = -ret;
   }
   return(ret);
} /* eval_input_sf() */


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(char *name, unsigned int protocol)
{
   (void)fprintf(stderr,
                 "SYNTAX: %s <work dir> <job no.> <FSA id> <FSA pos> <msg name> [options]\n\n",
                 name);
   (void)fprintf(stderr, "OPTIONS                       DESCRIPTION\n");
   (void)fprintf(stderr, "  --version                 - Show current version.\n");
   (void)fprintf(stderr, "  -a <age limit>            - Set the default age limit in seconds.\n");
   (void)fprintf(stderr, "  -A                        - Archiving is disabled.\n");
#ifdef HAVE_HW_CRC32
   (void)fprintf(stderr, "  -c                        - Enable support for hardware CRC-32.\n");
#endif
   if (protocol == SMTP_FLAG)
   {
      (void)fprintf(stderr, "  -C <charset>              - Set the default charset.\n");
   }
#ifdef _WITH_DE_MAIL_SUPPORT
   if (protocol == DE_MAIL_FLAG)
   {
      (void)fprintf(stderr, "  -D <DE-Mail sender>       - DE-Mail sender address.\n");
   }
#endif
   (void)fprintf(stderr, "  -e <seconds>              - Disconnect after the given amount of time.\n");
   if (protocol == SMTP_FLAG)
   {
      (void)fprintf(stderr, "  -f <SMTP from>            - Default from identifier to send.\n");
      (void)fprintf(stderr, "  -g <group mail domain>    - Group mail domain.\n");
   }
   if (protocol == HTTP_FLAG)
   {
      (void)fprintf(stderr, "  -h <HTTP proxy>[:<port>]  - Proxy where to send the HTTP request.\n");
   }
   (void)fprintf(stderr, "  -m <mode>                 - Mode of the created target dir.\n");
   (void)fprintf(stderr, "  -o <retries>              - Old/error message and number of retries.\n");
   (void)fprintf(stderr, "  -r                        - Resend from archive.\n");
   if (protocol == SMTP_FLAG)
   {
      (void)fprintf(stderr, "  -R <SMTP reply-to>        - Default reply-to identifier to send.\n");
      (void)fprintf(stderr, "  -s <SMTP server>[:<port>] - Server where to send the mails.\n");
   }
   (void)fprintf(stderr, "  -S                        - Simulation mode.\n");
   (void)fprintf(stderr, "  -t                        - Use other host (toggle).\n");

   return;
}
