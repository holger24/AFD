/*
 *  mafdcmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 - 2016 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   mafdcmd - send commands to the AFD monitor
 **
 ** SYNOPSIS
 **   mafdcmd [-w <working directory>] option AFD|position
 **                -e          enable AFD
 **                -E          disable AFD
 **                -r          retry
 **                -s          switch AFD
 **                -X          toggle enable/disable AFD
 *+                -p <role>   user role
 **                -u[ <user>] fake user
 **                -v          Just print the version number.
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.11.2002 H.Kiehl Created
 **   27.02.2005 H.Kiehl Option to switch between two AFD's.
 **
 */
DESCR__E_M1

#include <stdio.h>                    /* fprintf(), stderr               */
#include <string.h>                   /* strcpy(), strerror(), strlen()  */
#include <stdlib.h>                   /* atoi()                          */
#include <ctype.h>                    /* isdigit()                       */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>                    /* open()                          */
#endif
#include <unistd.h>                   /* STDERR_FILENO                   */
#include <errno.h>
#include "mondefs.h"
#include "permission.h"
#include "version.h"

/* Global variables. */
int                    sys_log_fd = STDERR_FILENO,
                       msa_fd = -1,
                       msa_id,
                       no_of_afd_names,
                       no_of_afds = 0,
                       options = 0;
#ifdef HAVE_MMAP
off_t                  msa_size;
#endif
char                   **afds = NULL,
                       *p_work_dir;
struct mon_status_area *msa;
const char             *sys_log_name = MON_SYS_LOG_FIFO;

/* Local function prototypes */
static int             get_afd_position(struct mon_status_area *, char *, int);
static void            eval_input(int, char **),
                       usage(char *);

#define ENABLE_AFD_OPTION    1
#define DISABLE_AFD_OPTION   2
#define TOGGLE_AFD_OPTION    4
#define RETRY_OPTION         8
#define SWITCH_AFD_OPTION    16


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ mafdcmd() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int  errors = 0,
        i,
        position,
        user_offset;
   char fake_user[MAX_FULL_USER_ID_LENGTH],
        *perm_buffer,
        profile[MAX_PROFILE_NAME_LENGTH + 1],
        *ptr,
        user[MAX_FULL_USER_ID_LENGTH],
        work_dir[MAX_PATH_LENGTH];

   CHECK_FOR_VERSION(argc, argv);

   if ((argc > 1) &&
       (argv[1][0] == '-') && (argv[1][1] == 'v') && (argv[1][2] == '\0'))
   {
      (void)fprintf(stdout, "%s\n", PACKAGE_VERSION);
      exit(SUCCESS);
   }

   if (get_mon_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;
   if (get_arg(&argc, argv, "-p", profile, MAX_PROFILE_NAME_LENGTH) == INCORRECT)
   {
      user_offset = 0;
      profile[0] = '\0';
   }
   else
   {
      (void)my_strncpy(user, profile, MAX_FULL_USER_ID_LENGTH);
      user_offset = strlen(profile);
   }
#ifdef WITH_SETUID_PROGS
   set_afd_euid(work_dir);
#endif

   if (argc < 2)
   {
      usage(argv[0]);
      exit(INCORRECT);
   }
   check_fake_user(&argc, argv, MON_CONFIG_FILE, fake_user);
   eval_input(argc, argv);
   get_user(user, fake_user, user_offset);

   /*
    * Ensure that the user may use this program.
    */
   switch (get_permissions(&perm_buffer, fake_user, profile))
   {
      case NO_ACCESS : /* Cannot access afd.users file. */
         {
            char afd_user_file[MAX_PATH_LENGTH];

            (void)strcpy(afd_user_file, p_work_dir);
            (void)strcat(afd_user_file, ETC_DIR);
            (void)strcat(afd_user_file, AFD_USER_FILE);

            (void)fprintf(stderr,
                          "Failed to access `%s', unable to determine users permissions.\n",
                          afd_user_file);
         }
         exit(INCORRECT);

      case NONE :
         (void)fprintf(stderr, "%s (%s %d)\n",
                       PERMISSION_DENIED_STR, __FILE__, __LINE__);
         exit(INCORRECT);

      case SUCCESS : /* Lets evaluate the permissions and see what */
                     /* the user may do.                           */
         {
            int permission = NO;

            if ((perm_buffer[0] == 'a') && (perm_buffer[1] == 'l') &&
                (perm_buffer[2] == 'l') &&
                ((perm_buffer[3] == '\0') || (perm_buffer[3] == ',') ||
                 (perm_buffer[3] == ' ') || (perm_buffer[3] == '\t')))
            {
               permission = YES;
            }
            else
            {
               if (lposi(perm_buffer, MAFD_CMD_PERM,
                         MAFD_CMD_PERM_LENGTH) != NULL)
               {
                  permission = YES;

                  /*
                   * Check if the user may do everything
                   * he has requested. If not remove it
                   * from the option list.
                   */
                  if ((options & ENABLE_AFD_OPTION) ||
                      (options & DISABLE_AFD_OPTION))
                  {
                     if (lposi(perm_buffer, DISABLE_AFD_PERM,
                               DISABLE_AFD_PERM_LENGTH) == NULL)
                     {
                        if (options & ENABLE_AFD_OPTION)
                        {
                           options ^= ENABLE_AFD_OPTION;
                        }
                        if (options & DISABLE_AFD_OPTION)
                        {
                           options ^= DISABLE_AFD_OPTION;
                        }
                        if (options & TOGGLE_AFD_OPTION)
                        {
                           options ^= TOGGLE_AFD_OPTION;
                        }
                        (void)fprintf(stderr,
                                      "User %s not permitted to enable/disable a AFD.\n",
                                      user);
                     }
                  }
                  if (options & RETRY_OPTION)
                  {
                     if (lposi(perm_buffer, RETRY_PERM,
                               RETRY_PERM_LENGTH) == NULL)
                     {
                        options ^= RETRY_OPTION;
                        (void)fprintf(stderr,
                                      "User %s not permitted to retry.\n",
                                      user);
                     }
                  }
                  if (options & SWITCH_AFD_OPTION)
                  {
                     if (lposi(perm_buffer, SWITCH_HOST_PERM,
                               SWITCH_HOST_PERM_LENGTH) == NULL)
                     {
                        options ^= SWITCH_AFD_OPTION;
                        (void)fprintf(stderr,
                                      "User %s not permitted to retry.\n",
                                      user);
                     }
                  }
               }
            }
            free(perm_buffer);
            if (permission != YES)
            {
               (void)fprintf(stderr, "%s (%s %d)\n",
                             PERMISSION_DENIED_STR, __FILE__, __LINE__);
               exit(INCORRECT);
            }
         }
         break;

      case INCORRECT: /* Hmm. Something did go wrong. Since we want to */
                      /* be able to disable permission checking let    */
                      /* the user have all permissions.                */
         break;

      default :
         (void)fprintf(stderr, "Impossible!! Remove the programmer!\n");
         exit(INCORRECT);
   }

   if (msa_attach() < 0)
   {
      (void)fprintf(stderr, "ERROR   : Failed to attach to MSA. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   for (i = 0; i < no_of_afd_names; i++)
   {
      position = -1;
      ptr = afds[i];
      while ((*ptr != '\0') && (isdigit((int)(*ptr))))
      {
         ptr++;
      }
      if ((*ptr == '\0') && (ptr != afds[i]))
      {
         position = atoi(afds[i]);
         if ((position < 0) || (position > (no_of_afds - 1)))
         {
            (void)fprintf(stderr,
                          "WARNING : Position %d out of range. Ignoring. (%s %d)\n",
                          position, __FILE__, __LINE__);
            errors++;
            continue;
         }
      }
      if (position < 0)
      {
         if ((position = get_afd_position(msa, afds[i], no_of_afds)) < 0)
         {
            (void)fprintf(stderr,
                          "WARNING : Could not find AFD %s in MSA. (%s %d)\n",
                          afds[i], __FILE__, __LINE__);
            errors++;
            continue;
         }
      }

      /*
       * ENABLE AFD
       */
      if (options & ENABLE_AFD_OPTION)
      {
         if (msa[position].connect_status == DISABLED)
         {
            int  fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
            int  readfd;
#endif
            char mon_cmd_fifo[MAX_PATH_LENGTH];

            (void)sprintf(mon_cmd_fifo, "%s%s%s",
                          p_work_dir, FIFO_DIR, MON_CMD_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
            if (open_fifo_rw(mon_cmd_fifo, &readfd, &fd) == -1)
#else
            if ((fd = open(mon_cmd_fifo, O_RDWR)) == -1)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to open() %s : %s",
                          mon_cmd_fifo, strerror(errno));
               errors++;
            }
            else
            {
               char cmd[1 + SIZEOF_INT];

               cmd[0] = ENABLE_MON;
               (void)memcpy(&cmd[1], &position, SIZEOF_INT);
               if (write(fd, cmd, (1 + SIZEOF_INT)) != (1 + SIZEOF_INT))
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to write() to %s : %s",
                             mon_cmd_fifo, strerror(errno));
                  errors++;
               }
               else
               {
                  system_log(DEBUG_SIGN, NULL, 0,
                             "%-*s: ENABLED (%s) [mafdcmd].",
                             MAX_AFD_NAME_LENGTH, msa[position].afd_alias, user);
               }
#ifdef WITHOUT_FIFO_RW_SUPPORT
               if (close(readfd) == -1)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Failed to close() FIFO %s : %s",
                             mon_cmd_fifo, strerror(errno));
               }
#endif
               if (close(fd) == -1)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Failed to close() FIFO %s : %s",
                             mon_cmd_fifo, strerror(errno));
               }
            }
         }
         else
         {
            (void)fprintf(stderr,
                          "INFO    : AFD %s is already enabled.\n",
                          msa[position].afd_alias);
         }
      }

      /*
       * DISABLE AFD
       */
      if (options & DISABLE_AFD_OPTION)
      {
         if (msa[position].connect_status == DISABLED)
         {
            (void)fprintf(stderr,
                          "INFO    : AFD %s is already disabled.\n",
                          msa[position].afd_alias);
         }
         else
         {
            int  fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
            int  readfd;
#endif
            char mon_cmd_fifo[MAX_PATH_LENGTH];

            (void)sprintf(mon_cmd_fifo, "%s%s%s",
                          p_work_dir, FIFO_DIR, MON_CMD_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
            if (open_fifo_rw(mon_cmd_fifo, &readfd, &fd) == -1)
#else
            if ((fd = open(mon_cmd_fifo, O_RDWR)) == -1)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to open() %s : %s",
                          mon_cmd_fifo, strerror(errno));
               errors++;
            }
            else
            {
               char cmd[1 + SIZEOF_INT];

               cmd[0] = DISABLE_MON;
               (void)memcpy(&cmd[1], &position, SIZEOF_INT);
               if (write(fd, cmd, (1 + SIZEOF_INT)) != (1 + SIZEOF_INT))
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to write() to %s : %s",
                             mon_cmd_fifo, strerror(errno));
                  errors++;
               }
               else
               {
                  system_log(DEBUG_SIGN, NULL, 0,
                             "%-*s: DISABLED (%s) [mafdcmd].",
                             MAX_AFD_NAME_LENGTH, msa[position].afd_alias, user);
               }
#ifdef WITHOUT_FIFO_RW_SUPPORT
               if (close(readfd) == -1)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Failed to close() FIFO %s : %s",
                             mon_cmd_fifo, strerror(errno));
               }
#endif
               if (close(fd) == -1)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Failed to close() FIFO %s : %s",
                             mon_cmd_fifo, strerror(errno));
               }
            }
         }
      }

      /*
       * ENABLE or DISABLE a AFD.
       */
      if (options & TOGGLE_AFD_OPTION)
      {
         int  fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
         int  readfd;
#endif
         char mon_cmd_fifo[MAX_PATH_LENGTH];

         (void)sprintf(mon_cmd_fifo, "%s%s%s",
                       p_work_dir, FIFO_DIR, MON_CMD_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
         if (open_fifo_rw(mon_cmd_fifo, &readfd, &fd) == -1)
#else
         if ((fd = open(mon_cmd_fifo, O_RDWR)) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to open() %s : %s",
                       mon_cmd_fifo, strerror(errno));
            errors++;
         }
         else
         {
            char cmd[1 + SIZEOF_INT];

            if (msa[position].connect_status == DISABLED)
            {
               cmd[0] = ENABLE_MON;
            }
            else /* DISABLE AFD */
            {
               cmd[0] = DISABLE_MON;
            }
            (void)memcpy(&cmd[1], &position, SIZEOF_INT);
            if (write(fd, cmd, (1 + SIZEOF_INT)) != (1 + SIZEOF_INT))
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to write() to %s : %s",
                          mon_cmd_fifo, strerror(errno));
               errors++;
            }
            else
            {
               system_log(DEBUG_SIGN, NULL, 0,
                          "%-*s: %s (%s) [mafdcmd].",
                          MAX_AFD_NAME_LENGTH, msa[position].afd_alias,
                          (cmd[0] == DISABLE_MON) ? "DISABLE" : "ENABLE", user);
            }
#ifdef WITHOUT_FIFO_RW_SUPPORT
            if (close(readfd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Failed to close() FIFO %s : %s",
                          mon_cmd_fifo, strerror(errno));
            }
#endif
            if (close(fd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Failed to close() FIFO %s : %s",
                          mon_cmd_fifo, strerror(errno));
            }
         }
      }

      if (options & RETRY_OPTION)
      {
         int  fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
         int  readfd;
#endif
         char retry_fifo[MAX_PATH_LENGTH];

         (void)sprintf(retry_fifo, "%s%s%s%d",
                       p_work_dir, FIFO_DIR, RETRY_MON_FIFO, position);
#ifdef WITHOUT_FIFO_RW_SUPPORT
         if (open_fifo_rw(retry_fifo, &readfd, &fd) == -1)
#else
         if ((fd = open(retry_fifo, O_RDWR)) == -1)
#endif
         {
            (void)fprintf(stderr,
                          "WARNING : Failed to open() %s : %s (%s %d)\n",
                          retry_fifo, strerror(errno),
                          __FILE__, __LINE__);
            errors++;
         }
         else
         {
            if (write(fd, &position, sizeof(int)) != sizeof(int))
            {
               (void)fprintf(stderr,
                             "WARNING : Failed to write() to %s : %s (%s %d)\n",
                             retry_fifo, strerror(errno),
                             __FILE__, __LINE__);
               errors++;
            }
#ifdef WITHOUT_FIFO_RW_SUPPORT
            if (close(readfd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Failed to close() FIFO %s : %s"
                          RETRY_FD_FIFO, strerror(errno));
            }
#endif
            if (close(fd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Failed to close() FIFO %s : %s"
                          RETRY_FD_FIFO, strerror(errno));
            }
         }
      }

      /*
       * Switch AFD
       */
      if (options & SWITCH_AFD_OPTION)
      {
         if (msa[position].afd_switching == NO_SWITCHING)
         {
            (void)fprintf(stderr,
                          "INFO    : AFD %s cannot be switched.\n",
                          msa[position].afd_alias);
            errors++;
         }
         else
         {
            if (msa[position].afd_toggle == (HOST_ONE - 1))
            {
               msa[position].afd_toggle = (HOST_TWO - 1);
            }
            else
            {
               msa[position].afd_toggle = (HOST_ONE - 1);
            }
            system_log(DEBUG_SIGN, NULL, 0,
                       "%-*s: SWITCHED (%s) [mafdcmd].",
                       MAX_AFD_NAME_LENGTH, msa[position].afd_alias, user);
         }
      }
   } /* for (i = 0; i < no_of_afd_names; i++) */

   (void)msa_detach();

   exit(errors);
}


/*++++++++++++++++++++++++++++ eval_input() +++++++++++++++++++++++++++++*/
static void
eval_input(int argc, char *argv[])
{
   int  correct = YES,       /* Was input/syntax correct?      */
        need_afdname = NO;
   char progname[128];

   (void)my_strncpy(progname, argv[0], 128);

   /**********************************************************/
   /* The following while loop checks for the parameters:    */
   /*                                                        */
   /*         -e : enable AFD                                */
   /*         -E : disable AFD                               */
   /*         -r : retry                                     */
   /*         -s : switch AFD                                */
   /*         -X : toggle enable/disable AFD                 */
   /*                                                        */
   /**********************************************************/
   while ((--argc > 0) && ((*++argv)[0] == '-'))
   {
      if (*(argv[0] + 2) == '\0')
      {
         switch (*(argv[0] + 1))
         {
            case 'e': /* enable AFD */
               options ^= ENABLE_AFD_OPTION;
               need_afdname = YES;
               break;

            case 'E': /* disable AFD */
               options ^= DISABLE_AFD_OPTION;
               need_afdname = YES;
               break;

            case 'r': /* Retry */
               options ^= RETRY_OPTION;
               need_afdname = YES;
               break;

            case 'X': /* Toggle enable/disable AFD */
               options ^= TOGGLE_AFD_OPTION;
               break;

            default : /* Unknown parameter */

               (void)fprintf(stderr, "ERROR  : Unknown parameter %c. (%s %d)\n",
                            *(argv[0] + 1), __FILE__, __LINE__);
               correct = NO;
               break;

         } /* switch (*(argv[0] + 1)) */
      }
      else
      {
         (void)fprintf(stderr, "ERROR  : Unknown option %s. (%s %d)\n",
                      argv[0], __FILE__, __LINE__);
         correct = NO;
      }
   }

   if (correct != NO)
   {
      no_of_afd_names = argc;
      if (no_of_afd_names > 0)
      {
         int i = 0;

         RT_ARRAY(afds, no_of_afd_names, (MAX_AFD_NAME_LENGTH + 1), char);
         while (argc > 0)
         {
            (void)my_strncpy(afds[i], argv[0], MAX_AFD_NAME_LENGTH + 1);
            argc--; argv++;
            i++;
         }
      }
      else
      {
         if (need_afdname == YES)
         {
            (void)fprintf(stderr, "ERROR   : No AFD names specified!\n");
            correct = NO;
         }
      }
   }

   /* If input is not correct show syntax */
   if (correct == NO)
   {
      usage(progname);
      exit(1);
   }

   return;
}


/*######################### get_afd_position() ##########################*/
static int
get_afd_position(struct mon_status_area *msa,
                 char                   *afd_alias,
                 int                    no_of_afds)
{
   int position;

   for (position = 0; position < no_of_afds; position++)
   {
      if (CHECK_STRCMP(msa[position].afd_alias, afd_alias) == 0)
      {
         return(position);
      }
   }

   /* AFD not found in struct. */
   return(INCORRECT);
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{                    
   (void)fprintf(stderr,
                 "SYNTAX  : %s [-w working directory] options AFD|position\n",
                 progname);
   (void)fprintf(stderr,
                 "                 -e          enable AFD\n");
   (void)fprintf(stderr,
                 "                 -E          disable AFD\n");
   (void)fprintf(stderr,
                 "                 -r          retry\n");
   (void)fprintf(stderr,
                 "                 -s          switch AFD\n");
   (void)fprintf(stderr,
                 "                 -X          toggle enable/disable AFD\n");
   (void)fprintf(stderr,
                 "                 -u[ <user>] fake user\n");
   (void)fprintf(stderr,
                 "                 -v          just print Version\n");
   return;
}
