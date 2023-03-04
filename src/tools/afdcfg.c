/*
 *  afdcfg.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2023 Deutscher Wetterdienst (DWD),
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

DESCR__S_M1
/*
 ** NAME
 **   afdcfg - configure AFD
 **
 ** SYNOPSIS
 **   afdcfg [-w <working directory>] [-u[ <fake user>]] option
 **           -a                      enable archive
 **           -A                      disable archive
 **           -b                      enable create source dir
 **           -B                      disable create source dir
 **           -c                      enable create target dir
 **           -C                      disable create target dir
 **           -d                      enable directory warn time
 **           -du                     enable + update directory warn time
 **           -D                      disable directory warn time
 **           -h                      enable host warn time
 **           -H                      disable host warn time
 **           -i                      enable simulate send mode
 **           -I                      disable simulate send mode
 **           -o <errors offline>     modify first errors offline\n"));
 **           -r                      enable retrieving of files
 **           -R                      disable retrieving of files
 **           -s                      status of the above flags
 **           --save_status <name>    store status in given file <name>
 **           --recover_status <name> recover status from file <name>
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
 **   31.10.2000 H.Kiehl Created
 **   04.03.2004 H.Kiehl Option to set flag to enable/disable creating
 **                      target directories.
 **   07.02.2007 H.Kiehl Added option to enable/disable directory warn
 **                      time.
 **   03.01.2008 H.Kiehl Added host warn time.
 **   08.05.2008 H.Kiehl Added enable + update directory warn time (-du).
 **   09.05.2011 H.Kiehl Added enable + disable source dir
 **   15.09.2014 H.Kiehl Added enable + disable simulate send mode.
 **   21.02.2023 H.Kiehl Added --save_status and --recover_status.
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr            */
#include <string.h>                      /* strcpy(), strerror()         */
#include <stdlib.h>                      /* atoi(), strtoul()            */
#include <time.h>                        /* time()                       */
#include <ctype.h>                       /* isdigit()                    */
#include <sys/types.h>
#include <fcntl.h>                       /* open()                       */
#include <unistd.h>                      /* STDERR_FILENO                */
#include <errno.h>
#include "permission.h"
#include "version.h"

/* Global variables. */
int                        event_log_fd = STDERR_FILENO,
                           sys_log_fd = STDERR_FILENO,   /* Not used!    */
                           fra_fd = -1,
                           fra_id,
                           fsa_fd = -1,
                           fsa_id,
                           no_of_dirs = 0,
                           no_of_hosts = 0;
#ifdef HAVE_MMAP
off_t                      fra_size,
                           fsa_size;
#endif
char                       *p_work_dir;
struct fileretrieve_status *fra;
struct filetransfer_status *fsa;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void                usage(char *);

#define ENABLE_ARCHIVE_SEL              1
#define DISABLE_ARCHIVE_SEL             2
#define ENABLE_RETRIEVE_SEL             3
#define DISABLE_RETRIEVE_SEL            4
#define ENABLE_DIR_WARN_TIME_SEL        5
#define ENABLE_UPDATE_DIR_WARN_TIME_SEL 6
#define DISABLE_DIR_WARN_TIME_SEL       7
#define ENABLE_HOST_WARN_TIME_SEL       8
#define DISABLE_HOST_WARN_TIME_SEL      9
#define ENABLE_CREATE_TARGET_DIR_SEL    10
#define DISABLE_CREATE_TARGET_DIR_SEL   11
#define ENABLE_CREATE_SOURCE_DIR_SEL    12
#define DISABLE_CREATE_SOURCE_DIR_SEL   13
#define ENABLE_SIMULATE_SEND_MODE_SEL   14
#define DISABLE_SIMULATE_SEND_MODE_SEL  15
#define MODIFY_ERRORS_OFFLINE_SEL       16
#define STATUS_SEL                      17
#define STORE_STATUS_SEL                18
#define RECOVER_STATUS_SEL              19

#define AFDCFG_OPEN_ERROR                    3
#define AFDCFG_STORE_FILENAME                "afdcfg_saved_values"
#define AFDCFG_ARCHIVE_STR                   "Archiving disabled"
#define AFDCFG_ARCHIVE_STR_LENGTH            (sizeof(AFDCFG_ARCHIVE_STR) - 1)
#define AFDCFG_RETRIEVE_STR                  "Retrieving disabled"
#define AFDCFG_RETRIEVE_STR_LENGTH           (sizeof(AFDCFG_RETRIEVE_STR) - 1)
#define AFDCFG_HOST_WARN_TIME_STR            "Host warn time disabled"
#define AFDCFG_HOST_WARN_TIME_STR_LENGTH     (sizeof(AFDCFG_HOST_WARN_TIME_STR) - 1)
#define AFDCFG_DIR_WARN_TIME_STR             "Dir warn time disabled"
#define AFDCFG_DIR_WARN_TIME_STR_LENGTH      (sizeof(AFDCFG_DIR_WARN_TIME_STR) - 1)
#define AFDCFG_CREATE_SOURCE_DIR_STR         "Create source dir disabled"
#define AFDCFG_CREATE_SOURCE_DIR_STR_LENGTH  (sizeof(AFDCFG_CREATE_SOURCE_DIR_STR) - 1)
#define AFDCFG_CREATE_TARGET_DIR_STR         "Create target dir enabled"
#define AFDCFG_CREATE_TARGET_DIR_STR_LENGTH  (sizeof(AFDCFG_CREATE_TARGET_DIR_STR) - 1)
#define AFDCFG_SIMULATE_SEND_MODE_STR        "Simulate mode enabled"
#define AFDCFG_SIMULATE_SEND_MODE_STR_LENGTH (sizeof(AFDCFG_SIMULATE_SEND_MODE_STR) - 1)
#define AFDCFG_ERRORS_OFFLINE_STR            "First errors offline"
#define AFDCFG_ERRORS_OFFLINE_STR_LENGTH     (sizeof(AFDCFG_ERRORS_OFFLINE_STR) - 1)


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ afdcfg() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int  action,
        ignore_first_errors = 0, /* Silence compiler. */
        ret,
        user_offset;
   char fake_user[MAX_FULL_USER_ID_LENGTH],
        name[MAX_PATH_LENGTH],
        *perm_buffer,
        profile[MAX_PROFILE_NAME_LENGTH + 1],
        *ptr_fra = NULL, /* Silence compiler. */
        *ptr_fsa = NULL, /* Silence compiler. */
        user[MAX_FULL_USER_ID_LENGTH],
        work_dir[MAX_PATH_LENGTH];

   if ((get_arg(&argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      usage(argv[0]);
      exit(SUCCESS);
   }

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
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

   if (argc == 1)
   {
      usage(argv[0]);
      exit(INCORRECT);
   }
   name[0] = '\0';
   if (get_arg(&argc, argv, "-a", NULL, 0) != SUCCESS)
   {
      if (get_arg(&argc, argv, "-A", NULL, 0) != SUCCESS)
      {
         if (get_arg(&argc, argv, "-b", NULL, 0) != SUCCESS)
         {
            if (get_arg(&argc, argv, "-B", NULL, 0) != SUCCESS)
            {
               if (get_arg(&argc, argv, "-c", NULL, 0) != SUCCESS)
               {
                  if (get_arg(&argc, argv, "-C", NULL, 0) != SUCCESS)
                  {
                     if (get_arg(&argc, argv, "-d", NULL, 0) != SUCCESS)
                     {
                        if (get_arg(&argc, argv, "-du", NULL, 0) != SUCCESS)
                        {
                           if (get_arg(&argc, argv, "-D", NULL, 0) != SUCCESS)
                           {
                              if (get_arg(&argc, argv, "-h", NULL, 0) != SUCCESS)
                              {
                                 if (get_arg(&argc, argv, "-H", NULL, 0) != SUCCESS)
                                 {
                                    if (get_arg(&argc, argv, "-i", NULL, 0) != SUCCESS)
                                    {
                                       if (get_arg(&argc, argv, "-I", NULL, 0) != SUCCESS)
                                       {
                                          char value[MAX_INT_LENGTH + 1];

                                          if (get_arg(&argc, argv, "-o", value, MAX_INT_LENGTH) != SUCCESS)
                                          {
                                             if (get_arg(&argc, argv, "-r", NULL, 0) != SUCCESS)
                                             {
                                                if (get_arg(&argc, argv, "-R", NULL, 0) != SUCCESS)
                                                {
                                                   if (get_arg(&argc, argv, "-s", NULL, 0) != SUCCESS)
                                                   {
                                                      if (get_arg(&argc, argv, "--save_status", name, MAX_PATH_LENGTH) != SUCCESS)
                                                      {
                                                         if (get_arg(&argc, argv, "--recover_status", name, MAX_PATH_LENGTH) != SUCCESS)
                                                         {
                                                            usage(argv[0]);
                                                            exit(INCORRECT);
                                                         }
                                                         else
                                                         {
                                                            action = RECOVER_STATUS_SEL;
                                                         }
                                                      }
                                                      else
                                                      {
                                                         action = STORE_STATUS_SEL;
                                                      }
                                                   }
                                                   else
                                                   {
                                                      action = STATUS_SEL;
                                                   }
                                                }
                                                else
                                                {
                                                   action = DISABLE_RETRIEVE_SEL;
                                                }
                                             }
                                             else
                                             {
                                                action = ENABLE_RETRIEVE_SEL;
                                             }
                                          }
                                          else
                                          {
                                             ignore_first_errors = atoi(value);
                                             if (ignore_first_errors > 255)
                                             {
                                                (void)fprintf(stderr,
                                                              "The number of errors to be ignored is to high (%d). It may not be larger then 255.\n",
                                                              ignore_first_errors);
                                                exit(INCORRECT);
                                             }
                                             action = MODIFY_ERRORS_OFFLINE_SEL;
                                          }
                                       }
                                       else
                                       {
                                          action = DISABLE_SIMULATE_SEND_MODE_SEL;
                                       }
                                    }
                                    else
                                    {
                                       action = ENABLE_SIMULATE_SEND_MODE_SEL;
                                    }
                                 }
                                 else
                                 {
                                    action = DISABLE_HOST_WARN_TIME_SEL;
                                 }
                              }
                              else
                              {
                                 action = ENABLE_HOST_WARN_TIME_SEL;
                              }
                           }
                           else
                           {
                              action = DISABLE_DIR_WARN_TIME_SEL;
                           }
                        }
                        else
                        {
                           action = ENABLE_UPDATE_DIR_WARN_TIME_SEL;
                        }
                     }
                     else
                     {
                        action = ENABLE_DIR_WARN_TIME_SEL;
                     }
                  }
                  else
                  {
                     action = DISABLE_CREATE_TARGET_DIR_SEL;
                  }
               }
               else
               {
                  action = ENABLE_CREATE_TARGET_DIR_SEL;
               }
            }
            else
            {
               action = DISABLE_CREATE_SOURCE_DIR_SEL;
            }
         }
         else
         {
            action = ENABLE_CREATE_SOURCE_DIR_SEL;
         }
      }
      else
      {
         action = DISABLE_ARCHIVE_SEL;
      }
   }
   else /* Enable archive. */
   {
      action = ENABLE_ARCHIVE_SEL;
   }
   check_fake_user(&argc, argv, AFD_CONFIG_FILE, fake_user);
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
                          _("Failed to access `%s', unable to determine users permissions.\n"),
                          afd_user_file);
         }
         exit(INCORRECT);

      case NONE :
         (void)fprintf(stderr, "%s (%s %d)\n",
                       PERMISSION_DENIED_STR, __FILE__, __LINE__);
         exit(INCORRECT);

      case SUCCESS  : /* Lets evaluate the permissions and see what */
                      /* the user may do.                           */
         {
            int permission;

            if ((perm_buffer[0] == 'a') && (perm_buffer[1] == 'l') &&
                (perm_buffer[2] == 'l') &&
                ((perm_buffer[3] == '\0') || (perm_buffer[3] == ',') ||
                 (perm_buffer[3] == ' ') || (perm_buffer[3] == '\t')))
            {
               permission = YES;
            }
            else
            {
               if (lposi(perm_buffer, AFD_CFG_PERM,
                         AFD_CFG_PERM_LENGTH) != NULL)
               {
                  permission = YES;
               }
               else
               {
                  permission = NO;
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

      default       :
         (void)fprintf(stderr, _("Impossible!! Remove the programmer!\n"));
         exit(INCORRECT);
   }

   if ((action == STATUS_SEL) || (action == STORE_STATUS_SEL))
   {
      if ((ret = fsa_attach_features_passive(NO, AFDCFG)) != SUCCESS)
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
         exit(INCORRECT);
      }
      ptr_fsa = (char *)fsa - AFD_FEATURE_FLAG_OFFSET_END;

      if ((ret = fra_attach_features_passive()) != SUCCESS)
      {
         if (ret < 0)
         {
            (void)fprintf(stderr,
                          _("ERROR   : Failed to attach to FRA. (%s %d)\n"),
                          __FILE__, __LINE__);
         }
         else
         {
            (void)fprintf(stderr,
                          _("ERROR   : Failed to attach to FRA : %s (%s %d)\n"),
                          strerror(ret), __FILE__, __LINE__);
         }
         exit(INCORRECT);
      }
      ptr_fra = (char *)fra - AFD_FEATURE_FLAG_OFFSET_END;
   }
   else
   {
      if ((action == ENABLE_ARCHIVE_SEL) || (action == DISABLE_ARCHIVE_SEL) ||
          (action == ENABLE_CREATE_SOURCE_DIR_SEL) ||
          (action == DISABLE_CREATE_SOURCE_DIR_SEL) ||
          (action == ENABLE_CREATE_TARGET_DIR_SEL) ||
          (action == DISABLE_CREATE_TARGET_DIR_SEL) ||
          (action == ENABLE_HOST_WARN_TIME_SEL) ||
          (action == DISABLE_HOST_WARN_TIME_SEL) ||
          (action == ENABLE_RETRIEVE_SEL) || (action == DISABLE_RETRIEVE_SEL) ||
          (action == ENABLE_SIMULATE_SEND_MODE_SEL) ||
          (action == DISABLE_SIMULATE_SEND_MODE_SEL) ||
          (action == MODIFY_ERRORS_OFFLINE_SEL) ||
          (action == RECOVER_STATUS_SEL))
      {
         if (action == DISABLE_HOST_WARN_TIME_SEL)
         {
            if ((ret = fsa_attach(AFDCFG)) != SUCCESS)
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
               exit(INCORRECT);
            }
         }
         else
         {
            if ((ret = fsa_attach_features(AFDCFG)) != SUCCESS)
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
               exit(INCORRECT);
            }
         }
         ptr_fsa = (char *)fsa - AFD_FEATURE_FLAG_OFFSET_END;
      }

      if ((action == ENABLE_DIR_WARN_TIME_SEL) ||
          (action == ENABLE_UPDATE_DIR_WARN_TIME_SEL) ||
          (action == DISABLE_DIR_WARN_TIME_SEL) ||
          (action == RECOVER_STATUS_SEL))
      {
         if ((action == ENABLE_UPDATE_DIR_WARN_TIME_SEL) ||
             (action == DISABLE_DIR_WARN_TIME_SEL))
         {
            if ((ret = fra_attach()) != SUCCESS)
            {
               if (ret < 0)
               {
                  (void)fprintf(stderr,
                                _("ERROR   : Failed to attach to FRA. (%s %d)\n"),
                                __FILE__, __LINE__);
               }
               else
               {
                  (void)fprintf(stderr,
                                _("ERROR   : Failed to attach to FRA : %s (%s %d)\n"),
                                strerror(ret), __FILE__, __LINE__);
               }
               exit(INCORRECT);
            }
         }
         else
         {
            if ((ret = fra_attach_features()) != SUCCESS)
            {
               if (ret < 0)
               {
                  (void)fprintf(stderr,
                                _("ERROR   : Failed to attach to FRA features. (%s %d)\n"),
                                __FILE__, __LINE__);
               }
               else
               {
                  (void)fprintf(stderr,
                                _("ERROR   : Failed to attach to FRA features : %s (%s %d)\n"),
                                strerror(ret), __FILE__, __LINE__);
               }
               exit(INCORRECT);
            }
         }
         ptr_fra = (char *)fra - AFD_FEATURE_FLAG_OFFSET_END;
      }
   }

   ret = SUCCESS;
   switch (action)
   {
      case ENABLE_ARCHIVE_SEL :
         if (*ptr_fsa & DISABLE_ARCHIVE)
         {
            *ptr_fsa ^= DISABLE_ARCHIVE;
            system_log(CONFIG_SIGN, __FILE__, __LINE__,
                       _("Archiving enabled by %s"), user);
            event_log(0L, EC_GLOB, ET_MAN, EA_ENABLE_ARCHIVE, "%s",
                      user);
         }
         else
         {
            (void)fprintf(stdout, _("Archiving is already enabled.\n"));
         }
         break;

      case DISABLE_ARCHIVE_SEL :
         if (*ptr_fsa & DISABLE_ARCHIVE)
         {
            (void)fprintf(stdout, _("Archiving is already disabled.\n"));
         }
         else
         {
            *ptr_fsa |= DISABLE_ARCHIVE;
            system_log(CONFIG_SIGN, __FILE__, __LINE__,
                       _("Archiving disabled by %s"), user);
            event_log(0L, EC_GLOB, ET_MAN, EA_DISABLE_ARCHIVE, "%s",
                      user);
         }
         break;

      case ENABLE_SIMULATE_SEND_MODE_SEL :
         if ((*ptr_fsa & ENABLE_SIMULATE_SEND_MODE) == 0)
         {
            *ptr_fsa |= ENABLE_SIMULATE_SEND_MODE;
            system_log(CONFIG_SIGN, __FILE__, __LINE__,
                       _("Simulate send enabled by %s"), user);
            event_log(0L, EC_GLOB, ET_MAN, EA_ENABLE_SIMULATE_SEND_MODE, "%s",
                      user);
         }
         else
         {
            (void)fprintf(stdout, _("Simulate send is already enabled.\n"));
         }
         break;

      case DISABLE_SIMULATE_SEND_MODE_SEL :
         if (*ptr_fsa & EA_ENABLE_SIMULATE_SEND_MODE)
         {
            (void)fprintf(stdout, _("Simulate send is already disabled.\n"));
         }
         else
         {
            *ptr_fsa &= ~ENABLE_SIMULATE_SEND_MODE;
            system_log(CONFIG_SIGN, __FILE__, __LINE__,
                       _("Simulate send disabled by %s"), user);
            event_log(0L, EC_GLOB, ET_MAN, EA_DISABLE_SIMULATE_SEND_MODE, "%s",
                      user);
         }
         break;

      case ENABLE_CREATE_SOURCE_DIR_SEL :
         if (*ptr_fsa & DISABLE_CREATE_SOURCE_DIR)
         {
            *ptr_fsa ^= DISABLE_CREATE_SOURCE_DIR;
            system_log(CONFIG_SIGN, __FILE__, __LINE__,
                       _("Create source dir enabled by %s"), user);
            event_log(0L, EC_GLOB, ET_MAN, EA_ENABLE_CREATE_SOURCE_DIR,
                      "%s", user);
         }
         else
         {
            (void)fprintf(stdout, _("Create source dir already enabled.\n"));
         }
         break;

      case DISABLE_CREATE_SOURCE_DIR_SEL :
         if (*ptr_fsa & DISABLE_CREATE_SOURCE_DIR)
         {
            (void)fprintf(stdout, _("Create source dir already disabled.\n"));
         }
         else
         {
            *ptr_fsa |= DISABLE_CREATE_SOURCE_DIR;
            system_log(CONFIG_SIGN, __FILE__, __LINE__,
                       _("Create source dir disabled by %s"), user);
            event_log(0L, EC_GLOB, ET_MAN, EA_DISABLE_CREATE_SOURCE_DIR,
                      "%s", user);
         }
         break;

      case ENABLE_CREATE_TARGET_DIR_SEL :
         if (*ptr_fsa & ENABLE_CREATE_TARGET_DIR)
         {
            (void)fprintf(stdout, _("Create target dir already enabled.\n"));
         }
         else
         {
            *ptr_fsa |= ENABLE_CREATE_TARGET_DIR;
            system_log(CONFIG_SIGN, __FILE__, __LINE__,
                       _("Create target dir by default enabled by %s"), user);
            event_log(0L, EC_GLOB, ET_MAN, EA_ENABLE_CREATE_TARGET_DIR,
                      "%s", user);
         }
         break;

      case DISABLE_CREATE_TARGET_DIR_SEL :
         if (*ptr_fsa & ENABLE_CREATE_TARGET_DIR)
         {
            *ptr_fsa ^= ENABLE_CREATE_TARGET_DIR;
            system_log(CONFIG_SIGN, __FILE__, __LINE__,
                       _("Create target dir by default disabled by %s"), user);
            event_log(0L, EC_GLOB, ET_MAN, EA_DISABLE_CREATE_TARGET_DIR,
                      "%s", user);
         }
         else
         {
            (void)fprintf(stdout, _("Create target dir already disabled.\n"));
         }
         break;

      case ENABLE_DIR_WARN_TIME_SEL :
         if (*ptr_fra & DISABLE_DIR_WARN_TIME)
         {
            *ptr_fra ^= DISABLE_DIR_WARN_TIME;
            system_log(CONFIG_SIGN, __FILE__, __LINE__,
                       _("Directory info+warn time enabled by %s"), user);
            event_log(0L, EC_GLOB, ET_MAN, EA_ENABLE_DIR_WARN_TIME,
                      "%s", user);
         }
         else
         {
            (void)fprintf(stdout, _("Directory info+warn time already enabled.\n"));
         }
         break;

      case ENABLE_UPDATE_DIR_WARN_TIME_SEL :
         if (*ptr_fra & DISABLE_DIR_WARN_TIME)
         {
            int    i;
            time_t now;

            now = time(NULL);
            for (i = 0; i < no_of_dirs; i++)
            {
               if ((fra[i].warn_time > 0) || (fra[i].info_time > 0))
               {
                  fra[i].last_retrieval = now;
               }
            }
            *ptr_fra ^= DISABLE_DIR_WARN_TIME;
            system_log(CONFIG_SIGN, __FILE__, __LINE__,
                       _("Directory info+warn time enabled and directory times updated by %s"),
                       user);
            event_log(0L, EC_GLOB, ET_MAN, EA_ENABLE_DIR_WARN_TIME,
                      "%s", user);
         }
         else
         {
            (void)fprintf(stdout, _("Directory info+warn time already enabled.\n"));
         }
         break;

      case DISABLE_DIR_WARN_TIME_SEL :
         if (*ptr_fra & DISABLE_DIR_WARN_TIME)
         {
            (void)fprintf(stdout,
                          _("Directory info+warn time is already disabled.\n"));
         }
         else
         {
            int    i;
            time_t now;

            now = time(NULL);
            *ptr_fra |= DISABLE_DIR_WARN_TIME;
            for (i = 0; i < no_of_dirs; i++)
            {
               if (fra[i].dir_flag & WARN_TIME_REACHED)
               {
                  fra[i].dir_flag &= ~WARN_TIME_REACHED;
                  SET_DIR_STATUS(fra[i].dir_flag,
                                 now,
                                 fra[i].start_event_handle,
                                 fra[i].end_event_handle,
                                 fra[i].dir_status);
               }
            }
            system_log(CONFIG_SIGN, __FILE__, __LINE__,
                       _("Directory info+warn time is disabled by %s"), user);
            event_log(0L, EC_GLOB, ET_MAN, EA_DISABLE_DIR_WARN_TIME,
                      "%s", user);
         }
         break;

      case ENABLE_HOST_WARN_TIME_SEL :
         if (*ptr_fsa & DISABLE_HOST_WARN_TIME)
         {
            *ptr_fsa ^= DISABLE_HOST_WARN_TIME;
            system_log(CONFIG_SIGN, __FILE__, __LINE__,
                       _("Host info+warn time enabled by %s"), user);
            event_log(0L, EC_GLOB, ET_MAN, EA_ENABLE_HOST_WARN_TIME,
                      "%s", user);
         }
         else
         {
            (void)fprintf(stdout, _("Host info+warn time already enabled.\n"));
         }
         break;

      case DISABLE_HOST_WARN_TIME_SEL :
         if (*ptr_fsa & DISABLE_HOST_WARN_TIME)
         {
            (void)fprintf(stdout, _("Host info+warn time is already disabled.\n"));
         }
         else
         {
            int i;

            *ptr_fsa |= DISABLE_HOST_WARN_TIME;
            for (i = 0; i < no_of_hosts; i++)
            {
               if (fsa[i].host_status & HOST_WARN_TIME_REACHED)
               {
#ifdef LOCK_DEBUG
                  lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (i * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
                  lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (i * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
                  fsa[i].host_status &= ~HOST_WARN_TIME_REACHED;
#ifdef LOCK_DEBUG
                  unlock_region(fsa_fd, (AFD_WORD_OFFSET + (i * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
                  unlock_region(fsa_fd, (AFD_WORD_OFFSET + (i * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
               }
            }
            system_log(CONFIG_SIGN, __FILE__, __LINE__,
                       _("Host info+warn time is disabled by %s"), user);
            event_log(0L, EC_GLOB, ET_MAN, EA_DISABLE_HOST_WARN_TIME,
                      "%s", user);
         }
         break;

      case ENABLE_RETRIEVE_SEL :
         if (*ptr_fsa & DISABLE_RETRIEVE)
         {
            *ptr_fsa ^= DISABLE_RETRIEVE;
            system_log(CONFIG_SIGN, __FILE__, __LINE__,
                       "Retrieving enabled by %s", user);
            event_log(0L, EC_GLOB, ET_MAN, EA_ENABLE_RETRIEVE,
                      "%s", user);
         }
         else
         {
            (void)fprintf(stdout, _("Retrieving is already enabled.\n"));
         }
         break;

      case DISABLE_RETRIEVE_SEL :
         if (*ptr_fsa & DISABLE_RETRIEVE)
         {
            (void)fprintf(stdout, _("Retrieving is already disabled.\n"));
         }
         else
         {
            *ptr_fsa |= DISABLE_RETRIEVE;
            system_log(CONFIG_SIGN, __FILE__, __LINE__,
                       _("Retrieving disabled by %s"), user);
            event_log(0L, EC_GLOB, ET_MAN, EA_DISABLE_RETRIEVE,
                      "%s", user);
         }
         break;

      case MODIFY_ERRORS_OFFLINE_SEL :
         {
            int original_value = *(unsigned char *)((char *)fsa - AFD_WORD_OFFSET  + SIZEOF_INT + 1 + 1);

            if (original_value == ignore_first_errors)
            {
               (void)fprintf(stdout, _("Ignore first errors is already %d.\n"),
                             ignore_first_errors);
            }
            else
            {
               *(unsigned char *)((char *)fsa - AFD_WORD_OFFSET  + SIZEOF_INT + 1 + 1) = ignore_first_errors;
               system_log(CONFIG_SIGN, __FILE__, __LINE__,
                          _("Ignore first errors is set to %d by %s"),
                          ignore_first_errors, user);
               event_log(0L, EC_GLOB, ET_MAN, EA_MODIFY_ERRORS_OFFLINE,
                         "%s %d->%d",
                         user, original_value, ignore_first_errors);
            }
         }
         break;

      case STATUS_SEL :
         if (*ptr_fsa & DISABLE_ARCHIVE)
         {
            (void)fprintf(stdout, _("Archiving           : Disabled\n"));
         }
         else
         {
            (void)fprintf(stdout, _("Archiving           : Enabled\n"));
         }
         if (*ptr_fsa & DISABLE_RETRIEVE)
         {
            (void)fprintf(stdout, _("Retrieving          : Disabled\n"));
         }
         else
         {
            (void)fprintf(stdout, _("Retrieving          : Enabled\n"));
         }
         if (*ptr_fsa & DISABLE_HOST_WARN_TIME)
         {
            (void)fprintf(stdout, _("Host warn time      : Disabled\n"));
         }
         else
         {
            (void)fprintf(stdout, _("Host warn time      : Enabled\n"));
         }
         if (*ptr_fra & DISABLE_DIR_WARN_TIME)
         {
            (void)fprintf(stdout, _("Dir warn time       : Disabled\n"));
         }
         else
         {
            (void)fprintf(stdout, _("Dir warn time       : Enabled\n"));
         }
         if (*ptr_fsa & DISABLE_CREATE_SOURCE_DIR)
         {
            (void)fprintf(stdout, _("Create source dir   : Disabled\n"));
         }
         else
         {
            (void)fprintf(stdout, _("Create source dir   : Enabled\n"));
         }
         if (*ptr_fsa & ENABLE_CREATE_TARGET_DIR)
         {
            (void)fprintf(stdout, _("Create target dir   : Enabled\n"));
         }
         else
         {
            (void)fprintf(stdout, _("Create target dir   : Disabled\n"));
         }
         if (*ptr_fsa & ENABLE_SIMULATE_SEND_MODE)
         {
            (void)fprintf(stdout, _("Simulate mode       : Enabled\n"));
         }
         else
         {
            (void)fprintf(stdout, _("Simulate mode       : Disabled\n"));
         }
         (void)fprintf(stdout, _("First errors offline: %d\n"),
                       *(unsigned char *)((char *)fsa - AFD_WORD_OFFSET + SIZEOF_INT + 1 + 1));
         break;

      case STORE_STATUS_SEL :
         {
            FILE *fp;

            if ((fp = fopen(name, "w+")) == NULL)
            {
               (void)fprintf(stderr, "Failed to fopen() %s : %s (%s %d)\n",
                             name, strerror(errno), __FILE__, __LINE__);
               ret = AFDCFG_OPEN_ERROR;
            }
            else
            {
               (void)fprintf(fp, "%s : %d\n", AFDCFG_ARCHIVE_STR,
                             (*ptr_fsa & DISABLE_ARCHIVE) ? 1 : 0);
               (void)fprintf(fp, "%s : %d\n", AFDCFG_RETRIEVE_STR,
                             (*ptr_fsa & DISABLE_RETRIEVE) ? 1 : 0);
               (void)fprintf(fp, "%s : %d\n", AFDCFG_HOST_WARN_TIME_STR,
                             (*ptr_fsa & DISABLE_HOST_WARN_TIME) ? 1 : 0);
               (void)fprintf(fp, "%s : %d\n", AFDCFG_DIR_WARN_TIME_STR,
                             (*ptr_fra & DISABLE_DIR_WARN_TIME) ? 1 : 0);
               (void)fprintf(fp, "%s : %d\n", AFDCFG_CREATE_SOURCE_DIR_STR,
                             (*ptr_fsa & DISABLE_CREATE_SOURCE_DIR) ? 1 : 0);
               (void)fprintf(fp, "%s : %d\n", AFDCFG_CREATE_TARGET_DIR_STR,
                             (*ptr_fsa & ENABLE_CREATE_TARGET_DIR) ? 1 : 0);
               (void)fprintf(fp, "%s : %d\n", AFDCFG_SIMULATE_SEND_MODE_STR,
                             (*ptr_fsa & ENABLE_SIMULATE_SEND_MODE) ? 1 : 0);
               (void)fprintf(fp, "%s : %d\n", AFDCFG_ERRORS_OFFLINE_STR,
                             *(unsigned char *)((char *)fsa - AFD_WORD_OFFSET + SIZEOF_INT + 1 + 1));
               if (fclose(fp) == EOF)
               {
                  (void)fprintf(stderr, "Failed to fclose() %s : %s (%s %d)\n",
                                name, strerror(errno),
                                __FILE__, __LINE__);
               }
            }
         }
         break;

      case RECOVER_STATUS_SEL :
         {
            FILE *fp;

            if ((fp = fopen(name, "r")) == NULL)
            {
               (void)fprintf(stderr, "Failed to fopen() %s : %s (%s %d)\n",
                             name, strerror(errno), __FILE__, __LINE__);
               ret = AFDCFG_OPEN_ERROR;
            }
            else
            {
#ifdef HAVE_GETLINE
               size_t line_length = 0;
               char   *line = NULL;
#else
               char   line[MAX_LINE_LENGTH];
#endif

#ifdef HAVE_GETLINE
               while (getline(&line, &line_length, fp) != -1)
#else
               while (fgets(line, MAX_LINE_LENGTH, fp) != NULL)
#endif
               {
                  if (line[0] != '#')
                  {
                     if (strncmp(line, AFDCFG_ARCHIVE_STR,
                                 AFDCFG_ARCHIVE_STR_LENGTH) == 0)
                     {
                        if ((line[AFDCFG_ARCHIVE_STR_LENGTH] == ' ') &&
                            (line[AFDCFG_ARCHIVE_STR_LENGTH + 1] == ':') &&
                            (line[AFDCFG_ARCHIVE_STR_LENGTH + 2] == ' ') &&
                            ((line[AFDCFG_ARCHIVE_STR_LENGTH + 3] == '0') ||
                             (line[AFDCFG_ARCHIVE_STR_LENGTH + 3] == '1')))
                        {
                           if (line[AFDCFG_ARCHIVE_STR_LENGTH + 3] == '0')
                           {
                              if (*ptr_fsa & DISABLE_ARCHIVE)
                              {
                                 *ptr_fsa &= ~DISABLE_ARCHIVE; /* unset */
                              }
                           }
                           else
                           {
                              if ((*ptr_fsa & DISABLE_ARCHIVE) == 0)
                              {
                                 *ptr_fsa |= DISABLE_ARCHIVE; /* set */
                              }
                           }
                        }
                     }
                     else if (strncmp(line, AFDCFG_RETRIEVE_STR,
                                      AFDCFG_RETRIEVE_STR_LENGTH) == 0)
                          {
                             if ((line[AFDCFG_RETRIEVE_STR_LENGTH] == ' ') &&
                                 (line[AFDCFG_RETRIEVE_STR_LENGTH + 1] == ':') &&
                                 (line[AFDCFG_RETRIEVE_STR_LENGTH + 2] == ' ') &&
                                 ((line[AFDCFG_RETRIEVE_STR_LENGTH + 3] == '0') ||
                                  (line[AFDCFG_RETRIEVE_STR_LENGTH + 3] == '1')))
                             {
                                if (line[AFDCFG_RETRIEVE_STR_LENGTH + 3] == '0')
                                {
                                   if (*ptr_fsa & DISABLE_RETRIEVE)
                                   {
                                      *ptr_fsa &= ~DISABLE_RETRIEVE; /* unset */
                                   }
                                }
                                else
                                {
                                   if ((*ptr_fsa & DISABLE_RETRIEVE) == 0)
                                   {
                                      *ptr_fsa |= DISABLE_RETRIEVE; /* set */
                                   }
                                }
                             }
                          }
                     else if (strncmp(line, AFDCFG_HOST_WARN_TIME_STR,
                                      AFDCFG_HOST_WARN_TIME_STR_LENGTH) == 0)
                          {
                             if ((line[AFDCFG_HOST_WARN_TIME_STR_LENGTH] == ' ') &&
                                 (line[AFDCFG_HOST_WARN_TIME_STR_LENGTH + 1] == ':') &&
                                 (line[AFDCFG_HOST_WARN_TIME_STR_LENGTH + 2] == ' ') &&
                                 ((line[AFDCFG_HOST_WARN_TIME_STR_LENGTH + 3] == '0') ||
                                  (line[AFDCFG_HOST_WARN_TIME_STR_LENGTH + 3] == '1')))
                             {
                                if (line[AFDCFG_HOST_WARN_TIME_STR_LENGTH + 3] == '0')
                                {
                                   if (*ptr_fsa & DISABLE_HOST_WARN_TIME)
                                   {
                                      *ptr_fsa &= ~DISABLE_HOST_WARN_TIME; /* unset */
                                   }
                                }
                                else
                                {
                                   if ((*ptr_fsa & DISABLE_HOST_WARN_TIME) == 0)
                                   {
                                      *ptr_fsa |= DISABLE_HOST_WARN_TIME; /* set */
                                   }
                                }
                             }
                          }
                     else if (strncmp(line, AFDCFG_DIR_WARN_TIME_STR,
                                      AFDCFG_DIR_WARN_TIME_STR_LENGTH) == 0)
                          {
                             if ((line[AFDCFG_DIR_WARN_TIME_STR_LENGTH] == ' ') &&
                                 (line[AFDCFG_DIR_WARN_TIME_STR_LENGTH + 1] == ':') &&
                                 (line[AFDCFG_DIR_WARN_TIME_STR_LENGTH + 2] == ' ') &&
                                 ((line[AFDCFG_DIR_WARN_TIME_STR_LENGTH + 3] == '0') ||
                                  (line[AFDCFG_DIR_WARN_TIME_STR_LENGTH + 3] == '1')))
                             {
                                if (line[AFDCFG_DIR_WARN_TIME_STR_LENGTH + 3] == '0')
                                {
                                   if (*ptr_fsa & DISABLE_DIR_WARN_TIME)
                                   {
                                      *ptr_fsa &= ~DISABLE_DIR_WARN_TIME; /* unset */
                                   }
                                }
                                else
                                {
                                   if ((*ptr_fsa & DISABLE_DIR_WARN_TIME) == 0)
                                   {
                                      *ptr_fsa |= DISABLE_DIR_WARN_TIME; /* set */
                                   }
                                }
                             }
                          }
                     else if (strncmp(line, AFDCFG_CREATE_SOURCE_DIR_STR,
                                      AFDCFG_CREATE_SOURCE_DIR_STR_LENGTH) == 0)
                          {
                             if ((line[AFDCFG_CREATE_SOURCE_DIR_STR_LENGTH] == ' ') &&
                                 (line[AFDCFG_CREATE_SOURCE_DIR_STR_LENGTH + 1] == ':') &&
                                 (line[AFDCFG_CREATE_SOURCE_DIR_STR_LENGTH + 2] == ' ') &&
                                 ((line[AFDCFG_CREATE_SOURCE_DIR_STR_LENGTH + 3] == '0') ||
                                  (line[AFDCFG_CREATE_SOURCE_DIR_STR_LENGTH + 3] == '1')))
                             {
                                if (line[AFDCFG_CREATE_SOURCE_DIR_STR_LENGTH + 3] == '0')
                                {
                                   if (*ptr_fsa & DISABLE_CREATE_SOURCE_DIR)
                                   {
                                      *ptr_fsa &= ~DISABLE_CREATE_SOURCE_DIR; /* unset */
                                   }
                                }
                                else
                                {
                                   if ((*ptr_fsa & DISABLE_CREATE_SOURCE_DIR) == 0)
                                   {
                                      *ptr_fsa |= DISABLE_CREATE_SOURCE_DIR; /* set */
                                   }
                                }
                             }
                          }
                     else if (strncmp(line, AFDCFG_CREATE_TARGET_DIR_STR,
                                      AFDCFG_CREATE_TARGET_DIR_STR_LENGTH) == 0)
                          {
                             if ((line[AFDCFG_CREATE_TARGET_DIR_STR_LENGTH] == ' ') &&
                                 (line[AFDCFG_CREATE_TARGET_DIR_STR_LENGTH + 1] == ':') &&
                                 (line[AFDCFG_CREATE_TARGET_DIR_STR_LENGTH + 2] == ' ') &&
                                 ((line[AFDCFG_CREATE_TARGET_DIR_STR_LENGTH + 3] == '0') ||
                                  (line[AFDCFG_CREATE_TARGET_DIR_STR_LENGTH + 3] == '1')))
                             {
                                if (line[AFDCFG_CREATE_TARGET_DIR_STR_LENGTH + 3] == '0')
                                {
                                   if (*ptr_fsa & ENABLE_CREATE_TARGET_DIR)
                                   {
                                      *ptr_fsa &= ~ENABLE_CREATE_TARGET_DIR; /* unset */
                                   }
                                }
                                else
                                {
                                   if ((*ptr_fsa & ENABLE_CREATE_TARGET_DIR) == 0)
                                   {
                                      *ptr_fsa |= ENABLE_CREATE_TARGET_DIR; /* set */
                                   }
                                }
                             }
                          }
                     else if (strncmp(line, AFDCFG_SIMULATE_SEND_MODE_STR,
                                      AFDCFG_SIMULATE_SEND_MODE_STR_LENGTH) == 0)
                          {
                             if ((line[AFDCFG_SIMULATE_SEND_MODE_STR_LENGTH] == ' ') &&
                                 (line[AFDCFG_SIMULATE_SEND_MODE_STR_LENGTH + 1] == ':') &&
                                 (line[AFDCFG_SIMULATE_SEND_MODE_STR_LENGTH + 2] == ' ') &&
                                 ((line[AFDCFG_SIMULATE_SEND_MODE_STR_LENGTH + 3] == '0') ||
                                  (line[AFDCFG_SIMULATE_SEND_MODE_STR_LENGTH + 3] == '1')))
                             {
                                if (line[AFDCFG_SIMULATE_SEND_MODE_STR_LENGTH + 3] == '0')
                                {
                                   if (*ptr_fsa & ENABLE_SIMULATE_SEND_MODE)
                                   {
                                      *ptr_fsa &= ~ENABLE_SIMULATE_SEND_MODE; /* unset */
                                   }
                                }
                                else
                                {
                                   if ((*ptr_fsa & ENABLE_SIMULATE_SEND_MODE) == 0)
                                   {
                                      *ptr_fsa |= ENABLE_SIMULATE_SEND_MODE; /* set */
                                   }
                                }
                             }
                          }
                     else if (strncmp(line, AFDCFG_ERRORS_OFFLINE_STR,
                                      AFDCFG_ERRORS_OFFLINE_STR_LENGTH) == 0)
                          {
                             if ((line[AFDCFG_ERRORS_OFFLINE_STR_LENGTH] == ' ') &&
                                 (line[AFDCFG_ERRORS_OFFLINE_STR_LENGTH + 1] == ':') &&
                                 (line[AFDCFG_ERRORS_OFFLINE_STR_LENGTH + 2] == ' '))
                             {
                                int  i, j;
                                char string[MAX_INT_LENGTH];

                                i = 3;
                                j = 0;
                                while ((j < MAX_INT_LENGTH) &&
                                       (isdigit((int)line[AFDCFG_ERRORS_OFFLINE_STR_LENGTH + i])))
                                {
                                   string[j] = line[AFDCFG_ERRORS_OFFLINE_STR_LENGTH + i];
                                   i++; j++;
                                }
                                if (j > 0)
                                {
                                   string[j] = '\0';
                                   *(unsigned char *)((char *)fsa - AFD_WORD_OFFSET  + SIZEOF_INT + 1 + 1) = (unsigned char)strtoul(string, NULL, 10);
                                }
                             }
                          }
                  }
               }
#ifdef HAVE_GETLINE
               free(line);
#endif
               clearerr(fp);
               if (fclose(fp) == EOF)
               {
                  (void)fprintf(stderr, "Failed to fclose() %s : %s (%s %d)\n",
                                name, strerror(errno), __FILE__, __LINE__);
               }
               if (unlink(name) != 0)
               {
                  (void)fprintf(stderr, "Failed to unlink() %s : %s (%s %d)\n",
                                name, strerror(errno), __FILE__, __LINE__);
               }
            }
         }
         break;

      default :
         (void)fprintf(stderr, _("Impossible! (%s %d)\n"), __FILE__, __LINE__);
         ret = INCORRECT;
         break;
   }

   if ((action == ENABLE_ARCHIVE_SEL) || (action == DISABLE_ARCHIVE_SEL) ||
       (action == ENABLE_CREATE_SOURCE_DIR_SEL) ||
       (action == DISABLE_CREATE_SOURCE_DIR_SEL) ||
       (action == ENABLE_CREATE_TARGET_DIR_SEL) ||
       (action == DISABLE_CREATE_TARGET_DIR_SEL) ||
       (action == ENABLE_RETRIEVE_SEL) || (action == DISABLE_RETRIEVE_SEL) ||
       (action == MODIFY_ERRORS_OFFLINE_SEL) || (action == STATUS_SEL))
   {
      (void)fsa_detach(YES);
   }
   if ((action == ENABLE_DIR_WARN_TIME_SEL) ||
       (action == DISABLE_DIR_WARN_TIME_SEL) ||
       (action == STATUS_SEL))
   {
      (void)fra_detach();
   }

   exit(ret);
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{
   (void)fprintf(stderr,
                 _("SYNTAX  : %s [-w working directory] [-p <user profile>] [-u [<fake user>]] options\n"),
                 progname);
   (void)fprintf(stderr, _("          -a                      enable archive\n"));
   (void)fprintf(stderr, _("          -A                      disable archive\n"));
   (void)fprintf(stderr, _("          -b                      enable create source dir\n"));
   (void)fprintf(stderr, _("          -B                      disable create source dir\n"));
   (void)fprintf(stderr, _("          -c                      enable create target dir\n"));
   (void)fprintf(stderr, _("          -C                      disable create target dir\n"));
   (void)fprintf(stderr, _("          -d                      enable directory warn time\n"));
   (void)fprintf(stderr, _("          -du                     enable + update directory warn time\n"));
   (void)fprintf(stderr, _("          -D                      disable directory warn time\n"));
   (void)fprintf(stderr, _("          -h                      enable host warn time\n"));
   (void)fprintf(stderr, _("          -H                      disable host warn time\n"));
   (void)fprintf(stderr, _("          -i                      enable simulate send mode\n"));
   (void)fprintf(stderr, _("          -I                      disable simulate send mode\n"));
   (void)fprintf(stderr, _("          -o <errors offline>     modify first errors offline\n"));
   (void)fprintf(stderr, _("          -r                      enable retrieving of files\n"));
   (void)fprintf(stderr, _("          -R                      disable retrieving of files\n"));
   (void)fprintf(stderr, _("          -s                      status of the above flags\n"));
   (void)fprintf(stderr, _("          --save_status <name>    store status of the above flags to file <name>\n"));
   (void)fprintf(stderr, _("          --recover_status <name> recover status from file <name>\n"));
   return;
}
