/*
 *  afdcmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2025 Deutscher Wetterdienst (DWD),
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
 **   afdcmd - send commands to the AFD
 **
 ** SYNOPSIS
 **   afdcmd [-w <working directory>] [-p <role>] [-u[ <fake user>]] option hostalias|diralias|position [... hostalias|diralias|position n]
 **                -q              start queue
 **                -Q              stop queue
 **                -t              start transfer
 **                -T              stop transfer
 **                -b              enable directory
 **                -B              disable directory
 **                -j              start directory
 **                -J              stop directory
 **                -e              enable host
 **                -E              disable host
 **                -g              enable delete data for host
 **                -G              disable delete data for host
 **                -h <pos> <name> change real hostname to <name>
 **                -s              switch host
 **                -r              retry
 **                -R              rescan directory
 **                -d              enable/disable debug
 **                -c              enable/disable trace
 **                -C              enable/disable full trace
 **                -I              enable/disable simulate send mode
 **                -f              start FD
 **                -F              stop FD
 **                -a              start AMG
 **                -A              stop AMG
 **                -U              toggle start/stop directory
 **                -W              toggle enable/disable directory
 **                -X              toggle enable/disable host
 **                -Y              start/stop AMG
 **                -Z              start/stop FD
 **                -k              force file dir check
 **                -i              reread local interface file
 **                -o              show exec statistics
 **                -O              force search for old files
 **                -P              force archive check
 **                -S              enable scanning of directories [afdbench]
 **                -v              Just print the version number.
 **     [aAbBcCdeEfFgGhiIjJkoOpPqQrRsStTuUWXYZ]
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
 **   09.09.1998 H.Kiehl Created
 **   27.02.2003 H.Kiehl When queue has been stopped automatically and we
 **                      wish to start it again error counter is set to zero.
 **   25.01.2005 H.Kiehl Don't start AUTO_PAUSE_QUEUE_STAT when starting
 **                      queue.
 **   08.11.2007 H.Kiehl Added -i option to reread local interface file.
 **                      Integrated programs force_archive_check,
 **                      file_dir_check and show_exec_stat.
 **   06.08.2009 H.Kiehl Added options to start, stop and toggle starting/
 **                      stopping of a directory.
 **   15.09.2014 H.Kiehl Added -I option to simulate sending files.
 **   31.03.2017 H.Kiehl Do not allow to set things on group identifier.
 **   19.07.2019 H.Kiehl Write simulate mode to HOST_CONFIG.
 **   23.02.2020 H.Kiehl Added -h to change the real_hostname.
 **   12.10.2020 H.Kiehl Added -O to force a search for old files.
 **   06.01.2025 H.Kiehl Send 'force archive check' to the correct fifo.
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr            */
#include <string.h>                      /* strcpy(), strerror()         */
#include <stdlib.h>                      /* atoi()                       */
#include <ctype.h>                       /* isdigit()                    */
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>                      /* kill()                       */
#include <fcntl.h>                       /* open()                       */
#include <time.h>                        /* time()                       */
#include <unistd.h>                      /* STDERR_FILENO                */
#include <errno.h>
#include "amgdefs.h"
#include "permission.h"
#include "version.h"

/* Global variables. */
int                        event_log_fd = STDERR_FILENO,
                           sys_log_fd = STDERR_FILENO,
                           fra_fd = -1,
                           fra_id,
                           fsa_fd = -1,
                           fsa_id,
                           jid_fd = -1,
                           no_of_dirs = 0,
                           no_of_host_names,
                           no_of_hosts = 0,
                           no_of_job_ids = 0,
                           real_hostname_pos;
unsigned int               options = 0,
                           options2 = 0;
#ifdef HAVE_MMAP
off_t                      fra_size,
                           fsa_size,
                           jid_size;
#endif
char                       **hosts = NULL,
                           *p_work_dir,
                           real_hostname[MAX_REAL_HOSTNAME_LENGTH + 1];
struct filetransfer_status *fsa;
struct fileretrieve_status *fra;
struct job_id_data         *jid;
struct afd_status          *p_afd_status;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void                eval_input(int, char **),
                           usage(char *);

#define START_QUEUE_OPTION                 1
#define STOP_QUEUE_OPTION                  2
#define START_TRANSFER_OPTION              4
#define STOP_TRANSFER_OPTION               8
#define ENABLE_DIRECTORY_OPTION            16
#define DISABLE_DIRECTORY_OPTION           32
#define START_DIRECTORY_OPTION             64
#define STOP_DIRECTORY_OPTION              128
#define ENABLE_HOST_OPTION                 256
#define DISABLE_HOST_OPTION                512
#define SWITCH_OPTION                      1024
#define RETRY_OPTION                       2048
#define RESCAN_OPTION                      4096
#define DEBUG_OPTION                       8192
#define TRACE_OPTION                       16384
#define FULL_TRACE_OPTION                  32768
#define START_FD_OPTION                    65536
#define STOP_FD_OPTION                     131072
#define START_AMG_OPTION                   262144
#define STOP_AMG_OPTION                    524288
#define START_STOP_AMG_OPTION              1048576
#define START_STOP_FD_OPTION               2097152
#define TOGGLE_DISABLE_DIRECTORY_OPTION    4194304
#define TOGGLE_STOP_DIRECTORY_OPTION       8388608
#define TOGGLE_HOST_OPTION                 16777216
#define FORCE_FILE_DIR_CHECK_OPTION        33554432
#define REREAD_LOCAL_INTERFACE_FILE_OPTION 67108864
#define SHOW_EXEC_STAT_OPTION              134217728
#define FORCE_ARCHIVE_CHECK_OPTION         268435456
#define ENABLE_DELETE_DATA                 536870912
#define DISABLE_DELETE_DATA                1073741824
#ifdef AFDBENCH_CONFIG
# define ENABLE_DIRECTORY_SCAN_OPTION      2147483648U
#endif
#define SIMULATE_SEND_MODE_OPTION          1
#define CHANGE_REAL_HOSTNAME               2
#define FORCE_SEARCH_OLD_FILES_OPTION      4


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ afdcmd() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int              change_host_config = NO,
                    ehc = YES, /* Error flag for eval_host_config(). */
                    errors = 0,
                    hosts_found,
                    i,
                    position,
                    user_offset;
   char             fake_user[MAX_FULL_USER_ID_LENGTH],
                    host_config_file[MAX_PATH_LENGTH],
                    *perm_buffer,
                    profile[MAX_PROFILE_NAME_LENGTH + 1],
                    *ptr,
                    user[41 + MAX_FULL_USER_ID_LENGTH],
                    work_dir[MAX_PATH_LENGTH];
   struct host_list *hl = NULL;

   if ((get_arg(&argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      usage(argv[0]);
      exit(SUCCESS);
   }

   CHECK_FOR_VERSION(argc, argv);

   if ((argc > 1) &&
       (argv[1][0] == '-') && (argv[1][1] == 'v') && (argv[1][2] == '\0'))
   {
      (void)fprintf(stdout, "%s\n", PACKAGE_VERSION);
      exit(SUCCESS);
   }

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;
   if (get_arg(&argc, argv, "-p", profile, MAX_PROFILE_NAME_LENGTH) == SUCCESS)
   {
      user_offset = strlen(profile);
      (void)my_strncpy(user, profile, MAX_FULL_USER_ID_LENGTH);
   }
   else
   {
      profile[0] = '\0';
      user_offset = 0;
   }
#ifdef WITH_SETUID_PROGS
   set_afd_euid(work_dir);
#endif

   if (argc < 2)
   {
      usage(argv[0]);
      exit(INCORRECT);
   }
   check_fake_user(&argc, argv, AFD_CONFIG_FILE, fake_user);
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
                          _("Failed to access `%s', unable to determine users permissions.\n"),
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
               if (lposi(perm_buffer, AFD_CMD_PERM,
                         AFD_CMD_PERM_LENGTH) != NULL)
               {
                  permission = YES;

                  /*
                   * Check if the user may do everything
                   * he has requested. If not remove it
                   * from the option list.
                   */
                  if ((options & START_QUEUE_OPTION) ||
                      (options & STOP_QUEUE_OPTION))
                  {
                     if (lposi(perm_buffer, CTRL_QUEUE_PERM,
                               CTRL_QUEUE_PERM_LENGTH) == NULL)
                     {
                        options &= ~START_QUEUE_OPTION;
                        options &= ~STOP_QUEUE_OPTION;
                        (void)fprintf(stderr,
                                      _("User %s not permitted to start/stop the queue.\n"),
                                      user);
                     }
                  }
                  if ((options & START_TRANSFER_OPTION) ||
                      (options & STOP_TRANSFER_OPTION))
                  {
                     if (lposi(perm_buffer, CTRL_TRANSFER_PERM,
                               CTRL_TRANSFER_PERM_LENGTH) == NULL)
                     {
                        options &= ~START_TRANSFER_OPTION;
                        options &= ~STOP_TRANSFER_OPTION;
                        (void)fprintf(stderr,
                                      _("User %s not permitted to start/stop the transfer.\n"),
                                      user);
                     }
                  }
                  if ((options & ENABLE_DIRECTORY_OPTION) ||
                      (options & DISABLE_DIRECTORY_OPTION) ||
                      (options & TOGGLE_DISABLE_DIRECTORY_OPTION))
                  {
                     if (lposi(perm_buffer, DISABLE_DIR_PERM,
                               DISABLE_DIR_PERM_LENGTH) == NULL)
                     {
                        options &= ~ENABLE_DIRECTORY_OPTION;
                        options &= ~DISABLE_DIRECTORY_OPTION;
                        options &= ~TOGGLE_DISABLE_DIRECTORY_OPTION;
                        (void)fprintf(stderr,
                                      _("User %s not permitted to enable/disable a directory.\n"),
                                      user);
                     }
                  }
                  if ((options & START_DIRECTORY_OPTION) ||
                      (options & STOP_DIRECTORY_OPTION) ||
                      (options & TOGGLE_STOP_DIRECTORY_OPTION))
                  {
                     if (lposi(perm_buffer, STOP_DIR_PERM,
                               STOP_DIR_PERM_LENGTH) == NULL)
                     {
                        options &= ~START_DIRECTORY_OPTION;
                        options &= ~STOP_DIRECTORY_OPTION;
                        options &= ~TOGGLE_STOP_DIRECTORY_OPTION;
                        (void)fprintf(stderr,
                                      _("User %s not permitted to start/stop a directory.\n"),
                                      user);
                     }
                  }
                  if ((options & ENABLE_HOST_OPTION) ||
                      (options & DISABLE_HOST_OPTION) ||
                      (options & TOGGLE_HOST_OPTION))
                  {
                     if (lposi(perm_buffer, DISABLE_HOST_PERM,
                               DISABLE_HOST_PERM_LENGTH) == NULL)
                     {
                        options &= ~ENABLE_HOST_OPTION;
                        options &= ~DISABLE_HOST_OPTION;
                        options &= ~TOGGLE_HOST_OPTION;
                        (void)fprintf(stderr,
                                      _("User %s not permitted to enable/disable a host.\n"),
                                      user);
                     }
                  }
                  if ((options & ENABLE_DELETE_DATA) ||
                      (options & DISABLE_DELETE_DATA))
                  {
                     if (lposi(perm_buffer, DO_NOT_DELETE_DATA_PERM,
                               DO_NOT_DELETE_DATA_PERM_LENGTH) == NULL)
                     {
                        options &= ~ENABLE_DELETE_DATA;
                        options &= ~DISABLE_DELETE_DATA;
                        (void)fprintf(stderr,
                                      _("User %s not permitted to enable/disable deletion of data for host.\n"),
                                      user);
                     }
                  }
                  if (options & SWITCH_OPTION)
                  {
                     if (lposi(perm_buffer, SWITCH_HOST_PERM,
                               SWITCH_HOST_PERM_LENGTH) == NULL)
                     {
                        options &= ~SWITCH_OPTION;
                        (void)fprintf(stderr,
                                      _("User %s not permitted to switch hosts.\n"),
                                      user);
                     }
                  }
                  if (options & RETRY_OPTION)
                  {
                     if (lposi(perm_buffer, RETRY_PERM,
                               RETRY_PERM_LENGTH) == NULL)
                     {
                        options &= ~RETRY_OPTION;
                        (void)fprintf(stderr,
                                      _("User %s not permitted to retry.\n"),
                                      user);
                     }
                  }
                  if (options & RESCAN_OPTION)
                  {
                     if (lposi(perm_buffer, RESCAN_PERM,
                               RESCAN_PERM_LENGTH) == NULL)
                     {
                        options &= ~RESCAN_OPTION;
                        (void)fprintf(stderr,
                                      _("User %s not permitted to rerscan a directory.\n"),
                                      user);
                     }
                  }
                  if (options & DEBUG_OPTION)
                  {
                     if (lposi(perm_buffer, DEBUG_PERM,
                               DEBUG_PERM_LENGTH) == NULL)
                     {
                        options &= ~DEBUG_OPTION;
                        (void)fprintf(stderr,
                                      _("User %s not permitted to enable/disable debugging for a host.\n"),
                                      user);
                     }
                  }
                  if (options & TRACE_OPTION)
                  {
                     if (lposi(perm_buffer, TRACE_PERM,
                               TRACE_PERM_LENGTH) == NULL)
                     {
                        options &= ~TRACE_OPTION;
                        (void)fprintf(stderr,
                                      _("User %s not permitted to enable/disable tracing for a host.\n"),
                                      user);
                     }
                  }
                  if (options & FULL_TRACE_OPTION)
                  {
                     if (lposi(perm_buffer, FULL_TRACE_PERM,
                               FULL_TRACE_PERM_LENGTH) == NULL)
                     {
                        options &= ~FULL_TRACE_OPTION;
                        (void)fprintf(stderr,
                                      _("User %s not permitted to enable/disable full tracing for a host.\n"),
                                      user);
                     }
                  }
                  if (options2 & SIMULATE_SEND_MODE_OPTION)
                  {
                     if (lposi(perm_buffer, SIMULATE_MODE_PERM,
                               SIMULATE_MODE_PERM_LENGTH) == NULL)
                     {
                        options2 &= ~SIMULATE_SEND_MODE_OPTION;
                        (void)fprintf(stderr,
                                      _("User %s not permitted to enable/disable simulate mode for a host.\n"),
                                      user);
                     }
                  }
                  if ((options & START_FD_OPTION) ||
                      (options & STOP_FD_OPTION))
                  {
                     if (lposi(perm_buffer, FD_CTRL_PERM,
                               FD_CTRL_PERM_LENGTH) == NULL)
                     {
                        options &= ~START_FD_OPTION;
                        options &= ~STOP_FD_OPTION;
                        (void)fprintf(stderr,
                                      _("User %s not permitted to start/stop the FD.\n"),
                                      user);
                     }
                  }
                  if ((options & START_AMG_OPTION) ||
                      (options & STOP_AMG_OPTION))
                  {
                     if (lposi(perm_buffer, AMG_CTRL_PERM,
                               AMG_CTRL_PERM_LENGTH) == NULL)
                     {
                        options &= ~START_AMG_OPTION;
                        options &= ~STOP_AMG_OPTION;
                        (void)fprintf(stderr,
                                      _("User %s not permitted to start/stop the AMG.\n"),
                                      user);
                     }
                  }
                  if (options & START_STOP_AMG_OPTION)
                  {
                     if (lposi(perm_buffer, AMG_CTRL_PERM,
                               AMG_CTRL_PERM_LENGTH) == NULL)
                     {
                        options &= ~START_STOP_AMG_OPTION;
                        (void)fprintf(stderr,
                                      _("User %s not permitted to start/stop the AMG.\n"),
                                      user);
                     }
                  }
                  if (options & START_STOP_FD_OPTION)
                  {
                     if (lposi(perm_buffer, FD_CTRL_PERM,
                               FD_CTRL_PERM_LENGTH) == NULL)
                     {
                        options &= ~START_STOP_FD_OPTION;
                        (void)fprintf(stderr,
                                      _("User %s not permitted to start/stop the FD.\n"),
                                      user);
                     }
                  }
                  if (options & FORCE_ARCHIVE_CHECK_OPTION)
                  {
                     if (lposi(perm_buffer, FORCE_AC_PERM,
                               FORCE_AC_PERM_LENGTH) == NULL)
                     {
                        options &= ~FORCE_ARCHIVE_CHECK_OPTION;
                        (void)fprintf(stderr,
                                      _("User %s is not allowed to force a file dir check.\n"),
                                      user);
                     }
                  }
                  if (options & FORCE_FILE_DIR_CHECK_OPTION)
                  {
                     if (lposi(perm_buffer, FILE_DIR_CHECK_PERM,
                               FILE_DIR_CHECK_PERM_LENGTH) == NULL)
                     {
                        options &= ~FORCE_FILE_DIR_CHECK_OPTION;
                        (void)fprintf(stderr,
                                      _("User %s is not allowed to force a file dir check.\n"),
                                      user);
                     }
                  }
                  if (options & REREAD_LOCAL_INTERFACE_FILE_OPTION)
                  {
                     if (lposi(perm_buffer, RR_LC_FILE_PERM,
                               RR_LC_FILE_PERM_LENGTH) == NULL)
                     {
                        options &= ~REREAD_LOCAL_INTERFACE_FILE_OPTION;
                        (void)fprintf(stderr,
                                      _("User %s not allowed to tell FD to reread the local interface file.\n"),
                                      user);
                     }
                  }
                  if (options & SHOW_EXEC_STAT_OPTION)
                  {
                     if (lposi(perm_buffer, SHOW_EXEC_STAT_PERM,
                               SHOW_EXEC_STAT_PERM_LENGTH) == NULL)
                     {
                        options &= ~SHOW_EXEC_STAT_OPTION;
                        (void)fprintf(stderr,
                                      _("User %s not allowed to show exec statistics.\n"),
                                      user);
                     }
                  }
                  if (options2 & FORCE_SEARCH_OLD_FILES_OPTION)
                  {
                     if (lposi(perm_buffer, SEARCH_OLD_FILES_PERM,
                               SEARCH_OLD_FILES_PERM_LENGTH) == NULL)
                     {
                        options2 &= ~FORCE_SEARCH_OLD_FILES_OPTION;
                        (void)fprintf(stderr,
                                      _("User %s not allowed to force a search for old files.\n"),
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
         (void)fprintf(stderr, _("Impossible!! Remove the programmer!\n"));
         exit(INCORRECT);
   }

   if ((options & RESCAN_OPTION) ||
       (options & ENABLE_DIRECTORY_OPTION) ||
       (options & DISABLE_DIRECTORY_OPTION) ||
       (options & TOGGLE_DISABLE_DIRECTORY_OPTION) ||
       (options & START_DIRECTORY_OPTION) ||
       (options & STOP_DIRECTORY_OPTION) ||
       (options & TOGGLE_STOP_DIRECTORY_OPTION))
   {
      int    send_msg = NO;
      time_t current_time;

      if ((i = fra_attach()) != SUCCESS)
      {
         if (i == INCORRECT_VERSION)
         {
            (void)fprintf(stderr,
                          _("ERROR   : This program is not able to attach to the FRA due to incorrect version. (%s %d)\n"),
                          __FILE__, __LINE__);
         }
         else
         {
            if (i < 0)
            {
               (void)fprintf(stderr,
                             _("ERROR   : Failed to attach to FRA. (%s %d)\n"),
                             __FILE__, __LINE__);
            }
            else
            {
               (void)fprintf(stderr,
                             _("ERROR   : Failed to attach to FRA : %s (%s %d)\n"),
                             strerror(i), __FILE__, __LINE__);
            }
         }
         exit(INCORRECT);
      }
      current_time = time(NULL);

      for (i = 0; i < no_of_host_names; i++)
      {
         position = -1;
         ptr = hosts[i];
         while ((*ptr != '\0') && (isdigit((int)(*ptr))))
         {
            ptr++;
         }
         if ((*ptr == '\0') && (ptr != hosts[i]))
         {
            position = atoi(hosts[i]);
            if ((position < 0) || (position > (no_of_dirs - 1)))
            {
               /*
                * Lets assume that this is not the position but the
                * directory alias.
                */
               position = -1;
            }
         }
         if (position < 0)
         {
            if ((position = get_dir_position(fra, hosts[i], no_of_dirs)) < 0)
            {
               (void)fprintf(stderr,
                             _("WARNING : Could not find directory %s in FRA. (%s %d)\n"),
                             hosts[i], __FILE__, __LINE__);
               errors++;
               continue;
            }
         }

         /*
          * RESCAN DIRECTORY
          */
         if (options & RESCAN_OPTION)
         {
            if ((fra[position].no_of_time_entries > 0) &&
                (fra[position].next_check_time > current_time))
            {
               if (fra[position].host_alias[0] != '\0')
               {
                  send_msg = YES;
               }
               fra[position].next_check_time = current_time;
               system_log(DEBUG_SIGN, NULL, 0,
                          _("%-*s: FORCED rescan (%s) [afdcmd]."),
                          MAX_DIR_ALIAS_LENGTH, fra[position].dir_alias, user);
               event_log(0L, EC_DIR, ET_MAN, EA_RESCAN_DIRECTORY, "%s%c%s",
                         fra[position].dir_alias, SEPARATOR_CHAR, user);
            }
         }

         /*
          * ENABLE DIRECTORY
          */
         if (options & ENABLE_DIRECTORY_OPTION)
         {
            if (fra[position].dir_flag & DIR_DISABLED)
            {
               system_log(DEBUG_SIGN, NULL, 0,
                          _("%-*s: ENABLED directory (%s) [afdcmd]."),
                          MAX_DIR_ALIAS_LENGTH, fra[position].dir_alias,
                          user);
               event_log(0L, EC_DIR, ET_MAN, EA_ENABLE_DIRECTORY, "%s%c%s",
                         fra[position].dir_alias, SEPARATOR_CHAR, user);
               fra[position].dir_flag &= ~DIR_DISABLED;
               SET_DIR_STATUS(fra[position].dir_flag,
                              current_time,
                              fra[position].start_event_handle,
                              fra[position].end_event_handle,
                              fra[position].dir_status);
            }
            else
            {
               (void)fprintf(stdout,
                             _("INFO    : Directory %s is already enabled.\n"),
                             fra[position].dir_alias);
            }
         }

         /*
          * DISABLE DIRECTORY
          */
         if (options & DISABLE_DIRECTORY_OPTION)
         {
            if (fra[position].dir_flag & DIR_DISABLED)
            {
               (void)fprintf(stdout,
                             _("INFO    : Directory %s is already disabled.\n"),
                             fra[position].dir_alias);
            }
            else
            {
               system_log(DEBUG_SIGN, NULL, 0,
                          _("%-*s: DISABLED directory (%s) [afdcmd]."),
                          MAX_DIR_ALIAS_LENGTH, fra[position].dir_alias, user);
               event_log(0L, EC_DIR, ET_MAN, EA_DISABLE_DIRECTORY, "%s%c%s",
                         fra[position].dir_alias, SEPARATOR_CHAR, user);
               fra[position].dir_flag |= DIR_DISABLED;
               SET_DIR_STATUS(fra[position].dir_flag,
                              current_time,
                              fra[position].start_event_handle,
                              fra[position].end_event_handle,
                              fra[position].dir_status);

               if (fra[position].host_alias[0] != '\0')
               {
                  int  fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
                  int  readfd;
#endif
                  char fd_delete_fifo[MAX_PATH_LENGTH];

                  (void)sprintf(fd_delete_fifo, "%s%s%s",
                                p_work_dir, FIFO_DIR, FD_DELETE_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
                  if (open_fifo_rw(fd_delete_fifo, &readfd, &fd) == -1)
#else
                  if ((fd = open(fd_delete_fifo, O_RDWR)) == -1)
#endif
                  {
                     (void)fprintf(stderr,
                                   _("Failed to open() %s : %s (%s %d)\n"),
                                   FD_DELETE_FIFO, strerror(errno),
                                   __FILE__, __LINE__);
                     errors++;
                  }
                  else
                  {
                     size_t length;
                     char   wbuf[MAX_DIR_ALIAS_LENGTH + 2];

                     wbuf[0] = DELETE_RETRIEVES_FROM_DIR;
                     (void)strcpy(&wbuf[1], fra[position].dir_alias);
                     length = 1 + strlen(fra[position].dir_alias) + 1;
                     if (write(fd, wbuf, length) != length)
                     {
                        (void)fprintf(stderr,
                                      _("Failed to write() to %s : %s (%s %d)\n"),
                                      FD_DELETE_FIFO,
                                      strerror(errno), __FILE__, __LINE__);
                         errors++;
                     }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                     if (close(readfd) == -1)
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   _("Failed to close() `%s' : %s"),
                                   FD_DELETE_FIFO, strerror(errno));
                     }
#endif
                     if (close(fd) == -1)
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   _("Failed to close() `%s' : %s"),
                                   FD_DELETE_FIFO, strerror(errno));
                     }
                  }
               }
            }
         }

         /*
          * ENABLE or DISABLE a directory.
          */
         if (options & TOGGLE_DISABLE_DIRECTORY_OPTION)
         {
            if (fra[position].dir_flag & DIR_DISABLED)
            {
               /*
                * ENABLE DIRECTORY
                */
               system_log(DEBUG_SIGN, NULL, 0,
                          _("%-*s: ENABLED directory (%s) [afdcmd]."),
                          MAX_DIR_ALIAS_LENGTH, fra[position].dir_alias, user);
               event_log(0L, EC_DIR, ET_MAN, EA_ENABLE_DIRECTORY, "%s%c%s",
                         fra[position].dir_alias, SEPARATOR_CHAR, user);
               fra[position].dir_flag ^= DIR_DISABLED;
               SET_DIR_STATUS(fra[position].dir_flag,
                              current_time,
                              fra[position].start_event_handle,
                              fra[position].end_event_handle,
                              fra[position].dir_status);
            }
            else /* DISABLE DIRECTORY */
            {
               system_log(DEBUG_SIGN, NULL, 0,
                          _("%-*s: DISABLED directory (%s) [afdcmd]."),
                          MAX_DIR_ALIAS_LENGTH, fra[position].dir_alias,
                          user);
               event_log(0L, EC_DIR, ET_MAN, EA_DISABLE_DIRECTORY, "%s%c%s",
                         fra[position].dir_alias, SEPARATOR_CHAR, user);
               fra[position].dir_flag ^= DIR_DISABLED;
               SET_DIR_STATUS(fra[position].dir_flag,
                              current_time,
                              fra[position].start_event_handle,
                              fra[position].end_event_handle,
                              fra[position].dir_status);

               if (fra[position].host_alias[0] != '\0')
               {
                  int  fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
                  int  readfd;
#endif
                  char fd_delete_fifo[MAX_PATH_LENGTH];

                  (void)sprintf(fd_delete_fifo, "%s%s%s",
                                p_work_dir, FIFO_DIR, FD_DELETE_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
                  if (open_fifo_rw(fd_delete_fifo, &readfd, &fd) == -1)
#else
                  if ((fd = open(fd_delete_fifo, O_RDWR)) == -1)
#endif
                  {
                     (void)fprintf(stderr,
                                   _("Failed to open() %s : %s (%s %d)\n"),
                                   FD_DELETE_FIFO, strerror(errno),
                                   __FILE__, __LINE__);
                     errors++;
                  }
                  else
                  {
                     size_t length;
                     char   wbuf[MAX_DIR_ALIAS_LENGTH + 2];

                     wbuf[0] = DELETE_RETRIEVES_FROM_DIR;
                     (void)strcpy(&wbuf[1], fra[position].dir_alias);
                     length = 1 + strlen(fra[position].dir_alias) + 1;
                     if (write(fd, wbuf, length) != length)
                     {
                        (void)fprintf(stderr,
                                      _("Failed to write() to %s : %s (%s %d)\n"),
                                      FD_DELETE_FIFO,
                                      strerror(errno), __FILE__, __LINE__);
                         errors++;
                     }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                     if (close(readfd) == -1)
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   _("Failed to close() `%s' : %s"),
                                   FD_DELETE_FIFO, strerror(errno));
                     }
#endif
                     if (close(fd) == -1)
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   _("Failed to close() `%s' : %s"),
                                   FD_DELETE_FIFO, strerror(errno));
                     }
                  }
               }
            }
         }

         /*
          * START DIRECTORY
          */
         if (options & START_DIRECTORY_OPTION)
         {
            if (fra[position].dir_flag & DIR_STOPPED)
            {
               system_log(DEBUG_SIGN, NULL, 0,
                          _("%-*s: Started directory (%s) [afdcmd]."),
                          MAX_DIR_ALIAS_LENGTH, fra[position].dir_alias,
                          user);
               event_log(0L, EC_DIR, ET_MAN, EA_START_DIRECTORY, "%s%c%s",
                         fra[position].dir_alias, SEPARATOR_CHAR, user);
               fra[position].dir_flag &= ~DIR_STOPPED;
               SET_DIR_STATUS(fra[position].dir_flag,
                              current_time,
                              fra[position].start_event_handle,
                              fra[position].end_event_handle,
                              fra[position].dir_status);
            }
            else
            {
               (void)fprintf(stdout,
                             _("INFO    : Directory %s is already started.\n"),
                             fra[position].dir_alias);
            }
         }

         /*
          * STOP DIRECTORY
          */
         if (options & STOP_DIRECTORY_OPTION)
         {
            if (fra[position].dir_flag & DIR_STOPPED)
            {
               (void)fprintf(stdout,
                             _("INFO    : Directory %s is already stopped.\n"),
                             fra[position].dir_alias);
            }
            else
            {
               system_log(DEBUG_SIGN, NULL, 0,
                          _("%-*s: STOPPED directory (%s) [afdcmd]."),
                          MAX_DIR_ALIAS_LENGTH, fra[position].dir_alias, user);
               event_log(0L, EC_DIR, ET_MAN, EA_STOP_DIRECTORY, "%s%c%s",
                         fra[position].dir_alias, SEPARATOR_CHAR, user);
               fra[position].dir_flag |= DIR_STOPPED;
               SET_DIR_STATUS(fra[position].dir_flag,
                              current_time,
                              fra[position].start_event_handle,
                              fra[position].end_event_handle,
                              fra[position].dir_status);

               if (fra[position].host_alias[0] != '\0')
               {
                  int  fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
                  int  readfd;
#endif
                  char fd_delete_fifo[MAX_PATH_LENGTH];

                  (void)sprintf(fd_delete_fifo, "%s%s%s",
                                p_work_dir, FIFO_DIR, FD_DELETE_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
                  if (open_fifo_rw(fd_delete_fifo, &readfd, &fd) == -1)
#else
                  if ((fd = open(fd_delete_fifo, O_RDWR)) == -1)
#endif
                  {
                     (void)fprintf(stderr,
                                   _("Failed to open() %s : %s (%s %d)\n"),
                                   FD_DELETE_FIFO, strerror(errno),
                                   __FILE__, __LINE__);
                     errors++;
                  }
                  else
                  {
                     size_t length;
                     char   wbuf[MAX_DIR_ALIAS_LENGTH + 2];

                     wbuf[0] = DELETE_RETRIEVES_FROM_DIR;
                     (void)strcpy(&wbuf[1], fra[position].dir_alias);
                     length = 1 + strlen(fra[position].dir_alias) + 1;
                     if (write(fd, wbuf, length) != length)
                     {
                        (void)fprintf(stderr,
                                      _("Failed to write() to %s : %s (%s %d)\n"),
                                      FD_DELETE_FIFO,
                                      strerror(errno), __FILE__, __LINE__);
                         errors++;
                     }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                     if (close(readfd) == -1)
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   _("Failed to close() `%s' : %s"),
                                   FD_DELETE_FIFO, strerror(errno));
                     }
#endif
                     if (close(fd) == -1)
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   _("Failed to close() `%s' : %s"),
                                   FD_DELETE_FIFO, strerror(errno));
                     }
                  }
               }
            }
         }

         /*
          * START or STOP a directory.
          */
         if (options & TOGGLE_STOP_DIRECTORY_OPTION)
         {
            if (fra[position].dir_flag & DIR_STOPPED)
            {
               /*
                * START DIRECTORY
                */
               system_log(DEBUG_SIGN, NULL, 0,
                          _("%-*s: STARTED directory (%s) [afdcmd]."),
                          MAX_DIR_ALIAS_LENGTH, fra[position].dir_alias, user);
               event_log(0L, EC_DIR, ET_MAN, EA_START_DIRECTORY, "%s%c%s",
                         fra[position].dir_alias, SEPARATOR_CHAR, user);
               fra[position].dir_flag &= ~DIR_STOPPED;
               SET_DIR_STATUS(fra[position].dir_flag,
                              current_time,
                              fra[position].start_event_handle,
                              fra[position].end_event_handle,
                              fra[position].dir_status);
            }
            else /* STOP DIRECTORY */
            {
               system_log(DEBUG_SIGN, NULL, 0,
                          _("%-*s: STOPPED directory (%s) [afdcmd]."),
                          MAX_DIR_ALIAS_LENGTH, fra[position].dir_alias,
                          user);
               event_log(0L, EC_DIR, ET_MAN, EA_STOP_DIRECTORY, "%s%c%s",
                         fra[position].dir_alias, SEPARATOR_CHAR, user);
               fra[position].dir_flag |= DIR_STOPPED;
               SET_DIR_STATUS(fra[position].dir_flag,
                              current_time,
                              fra[position].start_event_handle,
                              fra[position].end_event_handle,
                              fra[position].dir_status);

               if (fra[position].host_alias[0] != '\0')
               {
                  int  fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
                  int  readfd;
#endif
                  char fd_delete_fifo[MAX_PATH_LENGTH];

                  (void)sprintf(fd_delete_fifo, "%s%s%s",
                                p_work_dir, FIFO_DIR, FD_DELETE_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
                  if (open_fifo_rw(fd_delete_fifo, &readfd, &fd) == -1)
#else
                  if ((fd = open(fd_delete_fifo, O_RDWR)) == -1)
#endif
                  {
                     (void)fprintf(stderr,
                                   _("Failed to open() %s : %s (%s %d)\n"),
                                   FD_DELETE_FIFO, strerror(errno),
                                   __FILE__, __LINE__);
                     errors++;
                  }
                  else
                  {
                     size_t length;
                     char   wbuf[MAX_DIR_ALIAS_LENGTH + 2];

                     wbuf[0] = DELETE_RETRIEVES_FROM_DIR;
                     (void)strcpy(&wbuf[1], fra[position].dir_alias);
                     length = 1 + strlen(fra[position].dir_alias) + 1;
                     if (write(fd, wbuf, length) != length)
                     {
                        (void)fprintf(stderr,
                                      _("Failed to write() to %s : %s (%s %d)\n"),
                                      FD_DELETE_FIFO,
                                      strerror(errno), __FILE__, __LINE__);
                         errors++;
                     }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                     if (close(readfd) == -1)
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   _("Failed to close() `%s' : %s"),
                                   FD_DELETE_FIFO, strerror(errno));
                     }
#endif
                     if (close(fd) == -1)
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   _("Failed to close() `%s' : %s"),
                                   FD_DELETE_FIFO, strerror(errno));
                     }
                  }
               }
            }
         }
      }

      if (send_msg == YES)
      {
         int  fd_cmd_fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
         int  fd_cmd_readfd;
#endif
         char fd_cmd_fifo[MAX_PATH_LENGTH];

         (void)sprintf(fd_cmd_fifo, "%s%s%s",
                       p_work_dir, FIFO_DIR, FD_CMD_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
         if (open_fifo_rw(fd_cmd_fifo, &fd_cmd_readfd, &fd_cmd_fd) == -1)
#else
         if ((fd_cmd_fd = open(fd_cmd_fifo, O_RDWR)) == -1)
#endif
         {
            (void)fprintf(stderr, _("Could not open() `%s' : %s (%s %d)\n"),
                          fd_cmd_fifo, strerror(errno), __FILE__, __LINE__);
         }
         else
         {
            if (send_cmd(FORCE_REMOTE_DIR_CHECK, fd_cmd_fd) != SUCCESS)
            {
               (void)fprintf(stderr, _("write() error : %s\n"), strerror(errno));
            }
#ifdef WITHOUT_FIFO_RW_SUPPORT
            if (close(fd_cmd_readfd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          _("close() error : %s"), strerror(errno));
            }
#endif
            if (close(fd_cmd_fd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          _("close() error : %s"), strerror(errno));
            }
         }
      }

      (void)fra_detach();
   }

   if ((options & START_QUEUE_OPTION) || (options & STOP_QUEUE_OPTION) ||
       (options & START_TRANSFER_OPTION) || (options & STOP_TRANSFER_OPTION) ||
       (options & DISABLE_HOST_OPTION) || (options & ENABLE_HOST_OPTION) ||
       (options & TOGGLE_HOST_OPTION) || (options & SWITCH_OPTION) ||
       (options & RETRY_OPTION) || (options & DEBUG_OPTION) ||
       (options & TRACE_OPTION) || (options & FULL_TRACE_OPTION) ||
       (options & ENABLE_DELETE_DATA) || (options & DISABLE_DELETE_DATA) ||
       (options2 & SIMULATE_SEND_MODE_OPTION) ||
       (options2 & CHANGE_REAL_HOSTNAME))
   {
      if ((i = fsa_attach(AFD_CMD)) != SUCCESS)
      {
         if (i == INCORRECT_VERSION)
         {
            (void)fprintf(stderr,
                          _("ERROR   : This program is not able to attach to the FSA due to incorrect version. (%s %d)\n"),
                          __FILE__, __LINE__);
         }
         else
         {
            if (i < 0)
            {
               (void)fprintf(stderr,
                             _("ERROR   : Failed to attach to FSA. (%s %d)\n"),
                             __FILE__, __LINE__);
            }
            else
            {
               (void)fprintf(stderr,
                             _("ERROR   : Failed to attach to FSA : %s (%s %d)\n"),
                             strerror(i), __FILE__, __LINE__);
            }
         }
         exit(INCORRECT);
      }

      if ((options & START_QUEUE_OPTION) || (options & STOP_QUEUE_OPTION) ||
          (options & START_TRANSFER_OPTION) ||
          (options & STOP_TRANSFER_OPTION) ||
          (options & DISABLE_HOST_OPTION) || (options & ENABLE_HOST_OPTION) ||
          (options & TOGGLE_HOST_OPTION) || (options & SWITCH_OPTION) ||
          (options & ENABLE_DELETE_DATA) || (options & DISABLE_DELETE_DATA) ||
          (options2 & SIMULATE_SEND_MODE_OPTION) ||
          (options2 & CHANGE_REAL_HOSTNAME))
      {
         (void)sprintf(host_config_file, "%s%s%s",
                       p_work_dir, ETC_DIR, DEFAULT_HOST_CONFIG_FILE);
         if (eaccess(host_config_file, (R_OK | W_OK)) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("Unable to read/write from/to HOST_CONFIG, therefore no values changed in it!"));
            ehc = YES;
         }
         else
         {
            ehc = eval_host_config(&hosts_found, host_config_file, &hl, NULL,
                                  NULL, NO);
            if ((ehc == NO) && (no_of_hosts != hosts_found))
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          _("Hosts found in HOST_CONFIG (%d) and those currently storred (%d) are not the same. Unable to do any changes."),
                          no_of_hosts, hosts_found);
               ehc = YES;
            }
            else if (ehc == YES)
                 {
                    system_log(WARN_SIGN, __FILE__, __LINE__,
                               _("Unable to retrieve data from HOST_CONFIG, therefore no values changed in it!"));
                 }
         }
      }
      else
      {
         ehc = NO;
      }

      for (i = 0; i < no_of_host_names; i++)
      {
         position = -1;
         ptr = hosts[i];
         while ((*ptr != '\0') && (isdigit((int)(*ptr))))
         {
            ptr++;
         }
         if ((*ptr == '\0') && (ptr != hosts[i]))
         {
            position = atoi(hosts[i]);
            if ((position < 0) || (position > (no_of_hosts - 1)))
            {
               (void)fprintf(stderr,
                             _("WARNING : Position %d out of range. Ignoring. (%s %d)\n"),
                             position, __FILE__, __LINE__);
               errors++;
               continue;
            }
         }
         if (position < 0)
         {
            if ((position = get_host_position(fsa, hosts[i], no_of_hosts)) < 0)
            {
               (void)fprintf(stderr,
                             _("WARNING : Could not find host %s in FSA. (%s %d)\n"),
                             hosts[i], __FILE__, __LINE__);
               errors++;
               continue;
            }
         }

         if (fsa[position].real_hostname[0][0] == GROUP_IDENTIFIER)
         {
            (void)fprintf(stderr,
                          _("WARNING : Action not possible on group identifier %s (%s %d)\n"),
                          fsa[position].host_alias, __FILE__, __LINE__);
            errors++;
         }
         else
         {
            /*
             * START OUEUE
             */
            if (ehc == NO)
            {
               if (options & START_QUEUE_OPTION)
               {
                  if (fsa[position].host_status & PAUSE_QUEUE_STAT)
                  {
                     system_log(DEBUG_SIGN, NULL, 0,
                                _("%-*s: STARTED queue (%s) [afdcmd]."),
                                MAX_HOSTNAME_LENGTH,
                                fsa[position].host_dsp_name, user);
                     event_log(0L, EC_HOST, ET_MAN, EA_START_QUEUE, "%s%c%s",
                               fsa[position].host_alias, SEPARATOR_CHAR, user);
#ifdef LOCK_DEBUG
                     lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
                     lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
                     fsa[position].host_status ^= PAUSE_QUEUE_STAT;
#ifdef LOCK_DEBUG
                     unlock_region(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
                     unlock_region(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
                     hl[position].host_status &= ~PAUSE_QUEUE_STAT;
                     change_host_config = YES;
                  }
                  else
                  {
                     (void)fprintf(stdout,
                                   _("INFO    : Queue for host %s is already started.\n"),
                                   fsa[position].host_dsp_name);
                  }
               }

               /*
                * STOP OUEUE
                */
               if (options & STOP_QUEUE_OPTION)
               {
                  if (fsa[position].host_status & PAUSE_QUEUE_STAT)
                  {
                     (void)fprintf(stdout,
                                   _("INFO    : Queue for host %s is already stopped.\n"),
                                   fsa[position].host_dsp_name);
                  }
                  else
                  {
                     system_log(DEBUG_SIGN, NULL, 0,
                                _("%-*s: STOPPED queue (%s) [afdcmd]."),
                                MAX_HOSTNAME_LENGTH,
                                fsa[position].host_dsp_name, user);
                     event_log(0L, EC_HOST, ET_MAN, EA_STOP_QUEUE, "%s%c%s",
                               fsa[position].host_alias, SEPARATOR_CHAR, user);
#ifdef LOCK_DEBUG
                     lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
                     lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
                     fsa[position].host_status ^= PAUSE_QUEUE_STAT;
#ifdef LOCK_DEBUG
                     unlock_region(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
                     unlock_region(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
                     hl[position].host_status |= PAUSE_QUEUE_STAT;
                     change_host_config = YES;
                  }
               }

               /*
                * START TRANSFER
                */
               if (options & START_TRANSFER_OPTION)
               {
                  if (fsa[position].host_status & STOP_TRANSFER_STAT)
                  {
                     int  fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
                     int  readfd;
#endif
                     char wake_up_fifo[MAX_PATH_LENGTH];

                     (void)sprintf(wake_up_fifo, "%s%s%s",
                                   p_work_dir, FIFO_DIR, FD_WAKE_UP_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
                     if (open_fifo_rw(wake_up_fifo, &readfd, &fd) == -1)
#else
                     if ((fd = open(wake_up_fifo, O_RDWR)) == -1)
#endif
                     {
                        (void)fprintf(stderr,
                                      _("WARNING : Failed to open() `%s' : %s (%s %d)\n"),
                                      FD_WAKE_UP_FIFO, strerror(errno),
                                      __FILE__, __LINE__);
                        errors++;
                     }
                     else
                     {
                        if (write(fd, "", 1) != 1)
                        {
                           (void)fprintf(stderr,
                                         _("WARNING : Failed to write() to `%s' : %s (%s %d)\n"),
                                         FD_WAKE_UP_FIFO, strerror(errno),
                                         __FILE__, __LINE__);
                           errors++;
                        }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                        if (close(readfd) == -1)
                        {
                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                      _("Failed to close() `%s' : %s"),
                                      FD_WAKE_UP_FIFO, strerror(errno));
                        }
#endif
                        if (close(fd) == -1)
                        {
                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                      _("Failed to close() `%s' : %s"),
                                      FD_WAKE_UP_FIFO, strerror(errno));
                        }
                     }
                     system_log(DEBUG_SIGN, NULL, 0,
                                _("%-*s: STARTED transfer (%s) [afdcmd]."),
                                MAX_HOSTNAME_LENGTH,
                                fsa[position].host_dsp_name, user);
                     event_log(0L, EC_HOST, ET_MAN, EA_START_TRANSFER, "%s%c%s",
                               fsa[position].host_alias, SEPARATOR_CHAR, user);
#ifdef LOCK_DEBUG
                     lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
                     lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
                     fsa[position].host_status ^= STOP_TRANSFER_STAT;
#ifdef LOCK_DEBUG
                     unlock_region(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
                     unlock_region(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
                     hl[position].host_status &= ~STOP_TRANSFER_STAT;
                     change_host_config = YES;
                  }
                  else
                  {
                     (void)fprintf(stdout,
                                   _("INFO    : Transfer for host %s is already started.\n"),
                                   fsa[position].host_dsp_name);
                  }
               }

               /*
                * STOP TRANSFER
                */
               if (options & STOP_TRANSFER_OPTION)
               {
                  if (fsa[position].host_status & STOP_TRANSFER_STAT)
                  {
                     (void)fprintf(stdout,
                                   _("INFO    : Transfer for host %s is already stopped.\n"),
                                   fsa[position].host_dsp_name);
                  }
                  else
                  {
                     system_log(DEBUG_SIGN, NULL, 0,
                                _("%-*s: STOPPED transfer (%s) [afdcmd]."),
                                MAX_HOSTNAME_LENGTH,
                                fsa[position].host_dsp_name, user);
                     event_log(0L, EC_HOST, ET_MAN, EA_STOP_TRANSFER, "%s%c%s",
                               fsa[position].host_alias, SEPARATOR_CHAR, user);
#ifdef LOCK_DEBUG
                     lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
                     lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
                     fsa[position].host_status ^= STOP_TRANSFER_STAT;
#ifdef LOCK_DEBUG
                     unlock_region(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
                     unlock_region(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
                     if (fsa[position].active_transfers > 0)
                     {
                        int m;

                        for (m = 0; m < fsa[position].allowed_transfers; m++)
                        {
                           if (fsa[position].job_status[m].proc_id > 0)
                           {
                              if ((kill(fsa[position].job_status[m].proc_id,
                                        SIGINT) == -1) && (errno != ESRCH))
                              {
                                 system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                            "Failed to kill process %d : %s",
                                            fsa[position].job_status[m].proc_id,
                                            strerror(errno));
                              }
                           }
                        }
                     }
                     hl[position].host_status |= STOP_TRANSFER_STAT;
                     change_host_config = YES;
                  }
               }

               /*
                * ENABLE HOST
                */
               if (options & ENABLE_HOST_OPTION)
               {
                  if (fsa[position].special_flag & HOST_DISABLED)
                  {
                     system_log(DEBUG_SIGN, NULL, 0,
                                _("%-*s: ENABLED (%s) [afdcmd]."),
                                MAX_HOSTNAME_LENGTH,
                                fsa[position].host_dsp_name, user);
                     event_log(0L, EC_HOST, ET_MAN, EA_ENABLE_HOST, "%s%c%s",
                               fsa[position].host_alias, SEPARATOR_CHAR, user);
                     fsa[position].special_flag ^= HOST_DISABLED;
                     hl[position].host_status &= ~HOST_CONFIG_HOST_DISABLED;
                     check_fra_disable_all_flag(fsa[position].host_id,
                                                (fsa[position].special_flag & HOST_DISABLED));
                     change_host_config = YES;
                  }
                  else
                  {
                     (void)fprintf(stdout,
                                   _("INFO    : Host %s is already enabled.\n"),
                                   fsa[position].host_dsp_name);
                  }
               }

               /*
                * DISABLE HOST
                */
               if (options & DISABLE_HOST_OPTION)
               {
                  if (fsa[position].special_flag & HOST_DISABLED)
                  {
                     (void)fprintf(stdout,
                                   _("INFO    : Host %s is already disabled.\n"),
                                   fsa[position].host_dsp_name);
                  }
                  else
                  {
                     int    fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
                     int    readfd;
#endif
                     size_t length;
                     char   delete_jobs_host_fifo[MAX_PATH_LENGTH];

                     system_log(DEBUG_SIGN, NULL, 0,
                                _("%-*s: DISABLED (%s) [afdcmd]."),
                                MAX_HOSTNAME_LENGTH,
                                fsa[position].host_dsp_name, user);
                     event_log(0L, EC_HOST, ET_MAN, EA_DISABLE_HOST, "%s%c%s",
                               fsa[position].host_alias, SEPARATOR_CHAR, user);
#ifdef LOCK_DEBUG
                     lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
                     lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
                     if (time(NULL) > fsa[position].end_event_handle)
                     {
                        fsa[position].host_status &= ~(EVENT_STATUS_FLAGS |
                                                       AUTO_PAUSE_QUEUE_STAT);
                        if (fsa[position].end_event_handle > 0L)
                        {
                           fsa[position].end_event_handle = 0L;
                        }
                        if (fsa[position].start_event_handle > 0L)
                        {
                           fsa[position].start_event_handle = 0L;
                        }
                     }
                     else
                     {
                        fsa[position].host_status &= ~(EVENT_STATUS_STATIC_FLAGS |
                                                       AUTO_PAUSE_QUEUE_STAT);
                     }
#ifdef LOCK_DEBUG
                     unlock_region(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
                     unlock_region(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
                     fsa[position].special_flag ^= HOST_DISABLED;
                     hl[position].host_status |= HOST_CONFIG_HOST_DISABLED;
                     change_host_config = YES;
                     length = strlen(fsa[position].host_alias) + 1;

                     (void)sprintf(delete_jobs_host_fifo, "%s%s%s",
                                   p_work_dir, FIFO_DIR, FD_DELETE_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
                     if (open_fifo_rw(delete_jobs_host_fifo, &readfd, &fd) == -1)
#else
                     if ((fd = open(delete_jobs_host_fifo, O_RDWR)) == -1)
#endif
                     {
                        (void)fprintf(stderr,
                                      _("Failed to open() `%s' : %s (%s %d)\n"),
                                      FD_DELETE_FIFO, strerror(errno),
                                      __FILE__, __LINE__);
                        errors++;
                     }
                     else
                     {
                        char wbuf[MAX_HOSTNAME_LENGTH + 2];

                        wbuf[0] = DELETE_ALL_JOBS_FROM_HOST;
                        (void)strcpy(&wbuf[1], fsa[position].host_alias);
                        if (write(fd, wbuf, (length + 1)) != (length + 1))
                        {
                           (void)fprintf(stderr,
                                         _("Failed to write() to `%s' : %s (%s %d)\n"),
                                         FD_DELETE_FIFO,
                                         strerror(errno), __FILE__, __LINE__);
                            errors++;
                        }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                        if (close(readfd) == -1)
                        {
                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                      _("Failed to close() `%s' : %s"),
                                      FD_DELETE_FIFO, strerror(errno));
                        }
#endif
                        if (close(fd) == -1)
                        {
                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                      _("Failed to close() `%s' : %s"),
                                      FD_DELETE_FIFO, strerror(errno));
                        }
                     }
                     (void)sprintf(delete_jobs_host_fifo, "%s%s%s",
                                   p_work_dir, FIFO_DIR, DEL_TIME_JOB_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
                     if (open_fifo_rw(delete_jobs_host_fifo, &readfd, &fd) == -1)
#else
                     if ((fd = open(delete_jobs_host_fifo, O_RDWR)) == -1)
#endif
                     {
                        (void)fprintf(stderr,
                                      _("Failed to open() `%s' : %s (%s %d)\n"),
                                      DEL_TIME_JOB_FIFO, strerror(errno),
                                      __FILE__, __LINE__);
                        errors++;
                     }
                     else
                     {
                        if (write(fd, fsa[position].host_alias, length) != length)
                        {
                           (void)fprintf(stderr,
                                         _("Failed to write() to `%s' : %s (%s %d)\n"),
                                         DEL_TIME_JOB_FIFO, strerror(errno),
                                         __FILE__, __LINE__);
                           errors++;
                        }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                        if (close(readfd) == -1)
                        {
                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                      _("Failed to close() `%s' : %s"),
                                      DEL_TIME_JOB_FIFO, strerror(errno));
                        }
#endif
                        if (close(fd) == -1)
                        {
                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                      _("Failed to close() `%s' : %s"),
                                      DEL_TIME_JOB_FIFO, strerror(errno));
                        }
                     }

                     check_fra_disable_all_flag(fsa[position].host_id,
                                                (fsa[position].special_flag & HOST_DISABLED));
                  }
               }

               /*
                * ENABLE or DISABLE a host.
                */
               if (options & TOGGLE_HOST_OPTION)
               {
                  if (fsa[position].special_flag & HOST_DISABLED)
                  {
                     /*
                      * ENABLE HOST
                      */
                     system_log(DEBUG_SIGN, NULL, 0,
                                _("%-*s: ENABLED (%s) [afdcmd]."),
                                MAX_HOSTNAME_LENGTH,
                                fsa[position].host_dsp_name, user);
                     event_log(0L, EC_HOST, ET_MAN, EA_ENABLE_HOST, "%s%c%s",
                               fsa[position].host_alias, SEPARATOR_CHAR, user);
                     fsa[position].special_flag ^= HOST_DISABLED;
                     hl[position].host_status &= ~HOST_CONFIG_HOST_DISABLED;
                  }
                  else /* DISABLE HOST */
                  {
                     int    fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
                     int    readfd;
#endif
                     size_t length;
                     char   delete_jobs_host_fifo[MAX_PATH_LENGTH];

                     system_log(DEBUG_SIGN, NULL, 0,
                                _("%-*s: DISABLED (%s) [afdcmd]."),
                                MAX_HOSTNAME_LENGTH,
                                fsa[position].host_dsp_name, user);
                     event_log(0L, EC_HOST, ET_MAN, EA_DISABLE_HOST, "%s%c%s",
                               fsa[position].host_alias, SEPARATOR_CHAR, user);
#ifdef LOCK_DEBUG
                     lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
                     lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
                     if (time(NULL) > fsa[position].end_event_handle)
                     {
                        fsa[position].host_status &= ~(EVENT_STATUS_FLAGS |
                                                       AUTO_PAUSE_QUEUE_STAT);
                        if (fsa[position].end_event_handle > 0L)
                        {
                           fsa[position].end_event_handle = 0L;
                        }
                        if (fsa[position].start_event_handle > 0L)
                        {
                           fsa[position].start_event_handle = 0L;
                        }
                     }
                     else
                     {
                        fsa[position].host_status &= ~(EVENT_STATUS_STATIC_FLAGS |
                                                       AUTO_PAUSE_QUEUE_STAT);
                     }
#ifdef LOCK_DEBUG
                     unlock_region(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
                     unlock_region(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
                     fsa[position].special_flag ^= HOST_DISABLED;
                     hl[position].host_status |= HOST_CONFIG_HOST_DISABLED;
                     length = strlen(fsa[position].host_alias) + 1;

                     (void)sprintf(delete_jobs_host_fifo, "%s%s%s",
                                   p_work_dir, FIFO_DIR, FD_DELETE_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
                     if (open_fifo_rw(delete_jobs_host_fifo, &readfd, &fd) == -1)
#else
                     if ((fd = open(delete_jobs_host_fifo, O_RDWR)) == -1)
#endif
                     {
                        (void)fprintf(stderr,
                                      _("Failed to open() `%s' : %s (%s %d)\n"),
                                      FD_DELETE_FIFO, strerror(errno),
                                      __FILE__, __LINE__);
                        errors++;
                     }
                     else
                     {
                        char wbuf[MAX_HOSTNAME_LENGTH + 2];

                        wbuf[0] = DELETE_ALL_JOBS_FROM_HOST;
                        (void)strcpy(&wbuf[1], fsa[position].host_alias);
                        if (write(fd, wbuf, (length + 1)) != (length + 1))
                        {
                           (void)fprintf(stderr,
                                         _("Failed to write() to `%s' : %s (%s %d)\n"),
                                         FD_DELETE_FIFO,
                                         strerror(errno), __FILE__, __LINE__);
                            errors++;
                        }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                        if (close(readfd) == -1)
                        {
                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                      _("Failed to close() `%s' : %s"),
                                      FD_DELETE_FIFO, strerror(errno));
                        }
#endif
                        if (close(fd) == -1)
                        {
                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                      _("Failed to close() `%s' : %s"),
                                      FD_DELETE_FIFO, strerror(errno));
                        }
                     }
                     (void)sprintf(delete_jobs_host_fifo, "%s%s%s",
                                   p_work_dir, FIFO_DIR, DEL_TIME_JOB_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
                     if (open_fifo_rw(delete_jobs_host_fifo, &readfd, &fd) == -1)
#else
                     if ((fd = open(delete_jobs_host_fifo, O_RDWR)) == -1)
#endif
                     {
                        (void)fprintf(stderr,
                                      _("Failed to open() `%s' : %s (%s %d)\n"),
                                      DEL_TIME_JOB_FIFO, strerror(errno),
                                      __FILE__, __LINE__);
                        errors++;
                     }
                     else
                     {
                        if (write(fd, fsa[position].host_alias, length) != length)
                        {
                           (void)fprintf(stderr,
                                         _("Failed to write() to `%s' : %s (%s %d)\n"),
                                         DEL_TIME_JOB_FIFO, strerror(errno),
                                         __FILE__, __LINE__);
                           errors++;
                        }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                        if (close(readfd) == -1)
                        {
                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                      _("Failed to close() `%s' : %s"),
                                      DEL_TIME_JOB_FIFO, strerror(errno));
                        }
#endif
                        if (close(fd) == -1)
                        {
                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                      _("Failed to close() `%s' : %s"),
                                      DEL_TIME_JOB_FIFO, strerror(errno));
                        }
                     }
                  }
                  check_fra_disable_all_flag(fsa[position].host_id,
                                             (fsa[position].special_flag & HOST_DISABLED));
                  change_host_config = YES;
               }

               /*
                * ENABLE DELETE DATA
                */
               if (options & ENABLE_DELETE_DATA)
               {
                  if (fsa[position].host_status & DO_NOT_DELETE_DATA)
                  {
                     system_log(DEBUG_SIGN, NULL, 0,
                                _("%-*s: ENABLED delete data (%s) [afdcmd]."),
                                MAX_HOSTNAME_LENGTH,
                                fsa[position].host_dsp_name, user);
                     event_log(0L, EC_HOST, ET_MAN, EA_ENABLE_DELETE_DATA,
                               "%s%c%s",
                               fsa[position].host_alias, SEPARATOR_CHAR, user);
#ifdef LOCK_DEBUG
                     lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
                     lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
                     fsa[position].host_status &= ~DO_NOT_DELETE_DATA;
#ifdef LOCK_DEBUG
                     unlock_region(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
                     unlock_region(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
                     hl[position].host_status &= ~DO_NOT_DELETE_DATA;
                     change_host_config = YES;
                  }
                  else
                  {
                     (void)fprintf(stdout,
                                   _("INFO    : Data deletion for host %s is already enabled.\n"),
                                   fsa[position].host_dsp_name);
                  }
               }

               /*
                * DISABLE DELETE DATA
                */
               if (options & DISABLE_DELETE_DATA)
               {
                  if ((fsa[position].host_status & DO_NOT_DELETE_DATA) == 0)
                  {
                     system_log(DEBUG_SIGN, NULL, 0,
                                _("%-*s: DISABLED delete data (%s) [afdcmd]."),
                                MAX_HOSTNAME_LENGTH,
                                fsa[position].host_dsp_name, user);
                     event_log(0L, EC_HOST, ET_MAN, EA_DISABLE_DELETE_DATA,
                               "%s%c%s",
                               fsa[position].host_alias, SEPARATOR_CHAR, user);
#ifdef LOCK_DEBUG
                     lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
                     lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
                     fsa[position].host_status |= DO_NOT_DELETE_DATA;
#ifdef LOCK_DEBUG
                     unlock_region(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
                     unlock_region(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
                     hl[position].host_status |= DO_NOT_DELETE_DATA;
                     change_host_config = YES;
                  }
                  else
                  {
                     (void)fprintf(stdout,
                                   _("INFO    : Data deletion for host %s is already disabled.\n"),
                                   fsa[position].host_dsp_name);
                  }
               }

               /*
                * Change real hostname.
                */
               if ((options2 & CHANGE_REAL_HOSTNAME) &&
                   (real_hostname_pos != -1) && (real_hostname[0] != '\0'))
               {
                  if ((real_hostname_pos != 0) &&
                      (fsa[position].host_toggle_str[0] == '\0'))
                  {
                     (void)fprintf(stdout,
                                   _("WARNING : Host %s has just one real hostname!\n"),
                                   fsa[position].host_dsp_name);
                     errors++;
                  }
                  else
                  {
                     if (strcmp(fsa[position].real_hostname[real_hostname_pos],
                                real_hostname) != 0)
                     {
                        (void)strcpy(fsa[position].real_hostname[real_hostname_pos],
                                     real_hostname);
                        (void)strcpy(hl[position].real_hostname[real_hostname_pos],
                                     real_hostname);
                        event_log(0L, EC_HOST, ET_MAN, EA_CHANGE_REAL_HOSTNAME,
                                  "%s%c%s%c%d %s",
                                  fsa[position].host_alias, SEPARATOR_CHAR,
                                  user, SEPARATOR_CHAR, real_hostname_pos,
                                  real_hostname);
                        change_host_config = YES;
                     }
                  }
               }

               if (options & SWITCH_OPTION)
               {
                  if ((fsa[position].toggle_pos > 0) &&
                      (fsa[position].host_toggle_str[0] != '\0'))
                  {
                     char tmp_host_alias[MAX_HOSTNAME_LENGTH + 2];

                     system_log(DEBUG_SIGN, NULL, 0,
                                _("Host Switch initiated for host %s (%s) [afdcmd]"),
                                fsa[position].host_dsp_name, user);
                     if (fsa[position].host_toggle == HOST_ONE)
                     {
                        fsa[position].host_toggle = HOST_TWO;
                        hl[position].host_status |= HOST_TWO_FLAG;
                     }
                     else
                     {
                        fsa[position].host_toggle = HOST_ONE;
                        hl[position].host_status &= ~HOST_TWO_FLAG;
                     }
                     change_host_config = YES;
                     (void)strcpy(tmp_host_alias, fsa[position].host_dsp_name);
                     fsa[position].host_dsp_name[(int)fsa[position].toggle_pos] = fsa[position].host_toggle_str[(int)fsa[position].host_toggle];
                     event_log(0L, EC_HOST, ET_MAN, EA_SWITCH_HOST,
                               "%s%c%s%c%s -> %s",
                               fsa[position].host_alias, SEPARATOR_CHAR,
                               user, SEPARATOR_CHAR, tmp_host_alias,
                               fsa[position].host_dsp_name);
                  }
                  else
                  {
                     (void)fprintf(stderr,
                                   _("WARNING : Host %s cannot be switched!\n"),
                                   fsa[position].host_dsp_name);
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

               (void)sprintf(retry_fifo, "%s%s%s",
                             p_work_dir, FIFO_DIR, RETRY_FD_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
               if (open_fifo_rw(retry_fifo, &readfd, &fd) == -1)
#else
               if ((fd = open(retry_fifo, O_RDWR)) == -1)
#endif
               {
                  (void)fprintf(stderr,
                                _("WARNING : Failed to open() `%s' : %s (%s %d)\n"),
                                RETRY_FD_FIFO, strerror(errno),
                                __FILE__, __LINE__);
                  errors++;
               }
               else
               {
                  event_log(0L, EC_HOST, ET_MAN, EA_RETRY_HOST, "%s%c%s",
                            fsa[position].host_alias, SEPARATOR_CHAR, user);
                  if (write(fd, &position, sizeof(int)) != sizeof(int))
                  {
                     (void)fprintf(stderr,
                                   _("WARNING : Failed to write() to `%s' : %s (%s %d)\n"),
                                   RETRY_FD_FIFO, strerror(errno),
                                   __FILE__, __LINE__);
                     errors++;
                  }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                  if (close(readfd) == -1)
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                _("Failed to close() `%s' : %s"),
                                RETRY_FD_FIFO, strerror(errno));
                  }
#endif
                  if (close(fd) == -1)
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                _("Failed to close() `%s' : %s"),
                                RETRY_FD_FIFO, strerror(errno));
                  }
               }
            }

            if (options & DEBUG_OPTION)
            {
               if (fsa[position].debug == NORMAL_MODE)
               {
                  system_log(DEBUG_SIGN, NULL, 0,
                             _("%-*s: Enabled DEBUG mode by user %s [afdcmd]."),
                             MAX_HOSTNAME_LENGTH, fsa[position].host_dsp_name,
                             user);
                  event_log(0L, EC_HOST, ET_MAN, EA_ENABLE_DEBUG_HOST, "%s%c%s",
                            fsa[position].host_alias, SEPARATOR_CHAR, user);
                  fsa[position].debug = DEBUG_MODE;
               }
               else
               {
                  system_log(DEBUG_SIGN, NULL, 0,
                             _("%-*s: Disabled DEBUG mode by user %s [afdcmd]."),
                             MAX_HOSTNAME_LENGTH, fsa[position].host_dsp_name,
                             user);
                  event_log(0L, EC_HOST, ET_MAN, EA_DISABLE_DEBUG_HOST, "%s%c%s",
                            fsa[position].host_alias, SEPARATOR_CHAR, user);
                  fsa[position].debug = NORMAL_MODE;
               }
            }

            if (options & TRACE_OPTION)
            {
               if (fsa[position].debug == NORMAL_MODE)
               {
                  system_log(DEBUG_SIGN, NULL, 0,
                             _("%-*s: Enabled TRACE mode by user %s [afdcmd]."),
                             MAX_HOSTNAME_LENGTH, fsa[position].host_dsp_name,
                             user);
                  event_log(0L, EC_HOST, ET_MAN, EA_ENABLE_TRACE_HOST, "%s%c%s",
                            fsa[position].host_alias, SEPARATOR_CHAR, user);
                  fsa[position].debug = TRACE_MODE;
               }
               else
               {
                  system_log(DEBUG_SIGN, NULL, 0,
                             _("%-*s: Disabled TRACE mode by user %s [afdcmd]."),
                             MAX_HOSTNAME_LENGTH, fsa[position].host_dsp_name,
                             user);
                  event_log(0L, EC_HOST, ET_MAN, EA_DISABLE_TRACE_HOST, "%s%c%s",
                            fsa[position].host_alias, SEPARATOR_CHAR, user);
                  fsa[position].debug = NORMAL_MODE;
               }
            }

            if (options & FULL_TRACE_OPTION)
            {
               if (fsa[position].debug == NORMAL_MODE)
               {
                  system_log(DEBUG_SIGN, NULL, 0,
                             _("%-*s: Enabled FULL TRACE MODE by user %s [afdcmd]."),
                             MAX_HOSTNAME_LENGTH, fsa[position].host_dsp_name,
                             user);
                  event_log(0L, EC_HOST, ET_MAN, EA_ENABLE_FULL_TRACE_HOST,
                            "%s%c%s",
                            fsa[position].host_alias, SEPARATOR_CHAR, user);
                  fsa[position].debug = FULL_TRACE_MODE;
               }
               else
               {
                  system_log(DEBUG_SIGN, NULL, 0,
                             _("%-*s: Disabled FULL TRACE mode by user %s [afdcmd]."),
                             MAX_HOSTNAME_LENGTH, fsa[position].host_dsp_name,
                             user);
                  event_log(0L, EC_HOST, ET_MAN, EA_DISABLE_FULL_TRACE_HOST,
                            "%s%c%s",
                            fsa[position].host_alias, SEPARATOR_CHAR, user);
                  fsa[position].debug = NORMAL_MODE;
               }
            }

            if (options2 & SIMULATE_SEND_MODE_OPTION)
            {
               if ((fsa[position].host_status & SIMULATE_SEND_MODE) == 0)
               {
                  system_log(DEBUG_SIGN, NULL, 0,
                             _("%-*s: Enabled SIMULATE SEND MODE by user %s [afdcmd]."),
                             MAX_HOSTNAME_LENGTH, fsa[position].host_dsp_name,
                             user);
                  event_log(0L, EC_HOST, ET_MAN, EA_ENABLE_SIMULATE_SEND_HOST,
                            "%s%c%s",
                            fsa[position].host_alias, SEPARATOR_CHAR, user);
                  fsa[position].host_status |= SIMULATE_SEND_MODE;
                  hl[position].host_status |= SIMULATE_SEND_MODE;
               }
               else
               {
                  system_log(DEBUG_SIGN, NULL, 0,
                             _("%-*s: Disabled SIMULATE SEND mode by user %s [afdcmd]."),
                             MAX_HOSTNAME_LENGTH, fsa[position].host_dsp_name,
                             user);
                  event_log(0L, EC_HOST, ET_MAN, EA_DISABLE_SIMULATE_SEND_HOST,
                            "%s%c%s",
                            fsa[position].host_alias, SEPARATOR_CHAR, user);
                  fsa[position].host_status &= ~SIMULATE_SEND_MODE;
                  hl[position].host_status &= ~SIMULATE_SEND_MODE;
               }
               change_host_config = YES;
            }
         }
      } /* for (i = 0; i < no_of_host_names; i++) */

      (void)fsa_detach(YES);

      /*
       * NOTE: for now lets not store SIMULATE_SEND_MODE for a host
       *       in HOST_CONFIG.
       */
      if (((options & START_QUEUE_OPTION) || (options & STOP_QUEUE_OPTION) ||
           (options & START_TRANSFER_OPTION) ||
           (options & STOP_TRANSFER_OPTION) ||
           (options & DISABLE_HOST_OPTION) || (options & ENABLE_HOST_OPTION) ||
           (options & TOGGLE_HOST_OPTION) || (options & SWITCH_OPTION) ||
           (options & ENABLE_DELETE_DATA) || (options & DISABLE_DELETE_DATA) ||
           (options2 & SIMULATE_SEND_MODE_OPTION) ||
           (options2 & CHANGE_REAL_HOSTNAME)) &&
          (ehc == NO) && (change_host_config == YES))
      {
         (void)write_host_config(no_of_hosts, host_config_file, hl);
         free(hl);
      }
   }

   if ((options & START_FD_OPTION) || (options & STOP_FD_OPTION) ||
       (options & START_AMG_OPTION) || (options & STOP_AMG_OPTION) ||
       (options & START_STOP_AMG_OPTION) || (options & START_STOP_FD_OPTION))
   {
      int  afd_cmd_fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
      int  afd_cmd_readfd;
#endif
      char afd_cmd_fifo[MAX_PATH_LENGTH];

      (void)sprintf(afd_cmd_fifo, "%s%s%s",
                    p_work_dir, FIFO_DIR, AFD_CMD_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(afd_cmd_fifo, &afd_cmd_readfd, &afd_cmd_fd) == -1)
#else
      if ((afd_cmd_fd = open(afd_cmd_fifo, O_RDWR)) == -1)
#endif
      {
         (void)fprintf(stderr, _("Could not open() `%s' : %s (%s %d)\n"),
                       afd_cmd_fifo, strerror(errno), __FILE__, __LINE__);
      }
      else
      {
         if (attach_afd_status(NULL, WAIT_AFD_STATUS_ATTACH) < 0)
         {
            (void)fprintf(stderr,
                          _("ERROR   : Failed to attach to AFD status area. (%s %d)\n"),
                          __FILE__, __LINE__);
            exit(INCORRECT);
         }
         if (options & START_FD_OPTION)
         {
            if (p_afd_status->fd == ON)
            {
               (void)fprintf(stderr, _("%s is running. (%s %d)\n"),
                             FD, __FILE__, __LINE__);
            }
            else
            {
               system_log(CONFIG_SIGN, NULL, 0,
                          _("Sending START to %s by %s [afdcmd]"), FD, user);
               event_log(0L, EC_GLOB, ET_MAN, EA_FD_START, "%s", user);
               if (send_cmd(START_FD, afd_cmd_fd) != SUCCESS)
               {
                  (void)fprintf(stderr,
                                _("Was not able to start %s : %s (%s %d)\n"),
                                FD, strerror(errno), __FILE__, __LINE__);
               }
            }
         }

         if (options & STOP_FD_OPTION)
         {
            if (p_afd_status->fd == ON)
            {
               system_log(CONFIG_SIGN, NULL, 0,
                          _("Sending STOP to %s by %s [afdcmd]"), FD, user);
               event_log(0L, EC_GLOB, ET_MAN, EA_FD_STOP, "%s", user);
               if (send_cmd(STOP_FD, afd_cmd_fd) != SUCCESS)
               {
                  (void)fprintf(stderr,
                                _("Was not able to stop %s : %s (%s %d)\n"),
                                FD, strerror(errno), __FILE__, __LINE__);
               }
            }
            else
            {
               (void)fprintf(stderr, _("%s is already stopped. (%s %d)\n"),
                             FD, __FILE__, __LINE__);
            }
         }

         if (options & START_AMG_OPTION)
         {
            if (p_afd_status->amg == ON)
            {
               (void)fprintf(stderr, _("%s is already running. (%s %d)\n"),
                             AMG, __FILE__, __LINE__);
            }
            else
            {
               system_log(CONFIG_SIGN, NULL, 0,
                          _("Sending START to %s by %s [afdcmd]"), AMG, user);
               event_log(0L, EC_GLOB, ET_MAN, EA_AMG_START, "%s", user);
               if (send_cmd(START_AMG, afd_cmd_fd) != SUCCESS)
               {
                  (void)fprintf(stderr,
                                _("Was not able to start %s : %s (%s %d)\n"),
                                AMG, strerror(errno), __FILE__, __LINE__);
               }
            }
         }

         if (options & STOP_AMG_OPTION)
         {
            if (p_afd_status->amg == ON)
            {
               system_log(CONFIG_SIGN, NULL, 0,
                          _("Sending STOP to %s by %s [afdcmd]"), AMG, user);
               event_log(0L, EC_GLOB, ET_MAN, EA_AMG_STOP, "%s", user);
               if (send_cmd(STOP_AMG, afd_cmd_fd) != SUCCESS)
               {
                  (void)fprintf(stderr,
                                _("Was not able to stop %s : %s (%s %d)\n"),
                                AMG, strerror(errno), __FILE__, __LINE__);
               }
            }
            else
            {
               (void)fprintf(stderr, _("%s is already stopped. (%s %d)\n"),
                             AMG, __FILE__, __LINE__);
            }
         }

         if (options & START_STOP_AMG_OPTION)
         {
            if (p_afd_status->amg == ON)
            {
               system_log(CONFIG_SIGN, NULL, 0,
                          _("Sending STOP to %s by %s [afdcmd]"), AMG, user);
               event_log(0L, EC_GLOB, ET_MAN, EA_AMG_STOP, "%s", user);
               if (send_cmd(STOP_AMG, afd_cmd_fd) != SUCCESS)
               {
                  (void)fprintf(stderr,
                                _("Was not able to stop %s : %s (%s %d)\n"),
                                AMG, strerror(errno), __FILE__, __LINE__);
               }
            }
            else
            {
               system_log(CONFIG_SIGN, NULL, 0,
                          _("Sending START to %s by %s [afdcmd]"), AMG, user);
               event_log(0L, EC_GLOB, ET_MAN, EA_AMG_START, "%s", user);
               if (send_cmd(START_AMG, afd_cmd_fd) != SUCCESS)
               {
                  (void)fprintf(stderr,
                                _("Was not able to start %s : %s (%s %d)\n"),
                                AMG, strerror(errno), __FILE__, __LINE__);
               }
            }
         }

         if (options & START_STOP_FD_OPTION)
         {
            if (p_afd_status->fd == ON)
            {
               system_log(CONFIG_SIGN, NULL, 0,
                          _("Sending STOP to %s by %s [afdcmd]"), FD, user);
               event_log(0L, EC_GLOB, ET_MAN, EA_FD_STOP, "%s", user);
               if (send_cmd(STOP_FD, afd_cmd_fd) != SUCCESS)
               {
                  (void)fprintf(stderr,
                                _("Was not able to stop %s : %s (%s %d)\n"),
                                FD, strerror(errno), __FILE__, __LINE__);
               }
            }
            else
            {
               system_log(CONFIG_SIGN, NULL, 0,
                          _("Sending START to %s by %s [afdcmd]"), FD, user);
               event_log(0L, EC_GLOB, ET_MAN, EA_AMG_START, "%s", user);
               if (send_cmd(START_FD, afd_cmd_fd) != SUCCESS)
               {
                  (void)fprintf(stderr,
                                _("Was not able to start %s : %s (%s %d)\n"),
                                FD, strerror(errno), __FILE__, __LINE__);
               }
            }
         }
#ifdef WITHOUT_FIFO_RW_SUPPORT
         if (close(afd_cmd_readfd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("close() error : %s"), strerror(errno));
         }
#endif
         if (close(afd_cmd_fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("close() error : %s"), strerror(errno));
         }
         detach_afd_status();
      }
   }

   if ((options & FORCE_FILE_DIR_CHECK_OPTION) ||
       (options & REREAD_LOCAL_INTERFACE_FILE_OPTION))
   {
      int  fd_cmd_fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
      int  fd_cmd_readfd;
#endif
      char fd_cmd_fifo[MAX_PATH_LENGTH];

      (void)sprintf(fd_cmd_fifo, "%s%s%s",
                    p_work_dir, FIFO_DIR, DC_CMD_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(fd_cmd_fifo, &fd_cmd_readfd, &fd_cmd_fd) == -1)
#else
      if ((fd_cmd_fd = open(fd_cmd_fifo, O_RDWR)) == -1)
#endif
      {
         (void)fprintf(stderr, _("Could not open() `%s' : %s (%s %d)\n"),
                       fd_cmd_fifo, strerror(errno), __FILE__, __LINE__);
      }
      else
      {
         if (options & FORCE_FILE_DIR_CHECK_OPTION)
         {
            if (send_cmd(CHECK_FILE_DIR, fd_cmd_fd) != SUCCESS)
            {
               (void)fprintf(stderr,
                             _("Was not able to send command CHECK_FILE_DIR to %s : %s (%s %d)\n"),
                             FD, strerror(errno), __FILE__, __LINE__);
            }
         }

         if (options & REREAD_LOCAL_INTERFACE_FILE_OPTION)
         {
            if (send_cmd(REREAD_LOC_INTERFACE_FILE, fd_cmd_fd) != SUCCESS)
            {
               (void)fprintf(stderr,
                             _("Was not able to send command REREAD_LOC_INTERFACE_FILE to %s : %s (%s %d)\n"),
                             FD, strerror(errno), __FILE__, __LINE__);
            }
         }

#ifdef WITHOUT_FIFO_RW_SUPPORT
         if (close(fd_cmd_readfd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("close() error : %s"), strerror(errno));
         }
#endif
         if (close(fd_cmd_fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("close() error : %s"), strerror(errno));
         }
      }
   }

   if ((options & SHOW_EXEC_STAT_OPTION) ||
       (options2 & FORCE_SEARCH_OLD_FILES_OPTION))
   {
      int  dc_cmd_fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
      int  dc_cmd_readfd;
#endif
      char dc_cmd_fifo[MAX_PATH_LENGTH];

      (void)sprintf(dc_cmd_fifo, "%s%s%s",
                    p_work_dir, FIFO_DIR, DC_CMD_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(dc_cmd_fifo, &dc_cmd_readfd, &dc_cmd_fd) == -1)
#else
      if ((dc_cmd_fd = open(dc_cmd_fifo, O_RDWR)) == -1)
#endif
      {
         (void)fprintf(stderr, _("Could not open() `%s' : %s (%s %d)\n"),
                       dc_cmd_fifo, strerror(errno), __FILE__, __LINE__);
      }
      else
      {
         if (options & SHOW_EXEC_STAT_OPTION)
         {
            if (send_cmd(SR_EXEC_STAT, dc_cmd_fd) != SUCCESS)
            {
               (void)fprintf(stderr,
                             _("Was not able to send command SR_EXEC_STAT to %s : %s (%s %d)\n"),
                             DIR_CHECK, strerror(errno), __FILE__, __LINE__);
            }
         }
         if (options2 & FORCE_SEARCH_OLD_FILES_OPTION)
         {
            if (send_cmd(SEARCH_OLD_FILES, dc_cmd_fd) != SUCCESS)
            {
               (void)fprintf(stderr,
                             _("Was not able to send command SEARCH_OLD_FILES to %s : %s (%s %d)\n"),
                             DIR_CHECK, strerror(errno), __FILE__, __LINE__);
            }
         }

#ifdef WITHOUT_FIFO_RW_SUPPORT
         if (close(dc_cmd_readfd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("close() error : %s"), strerror(errno));
         }
#endif
         if (close(dc_cmd_fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("close() error : %s"), strerror(errno));
         }
      }
   }

   if (options & FORCE_ARCHIVE_CHECK_OPTION)
   {
      int  ac_cmd_fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
      int  ac_cmd_readfd;
#endif
      char ac_cmd_fifo[MAX_PATH_LENGTH];

      (void)sprintf(ac_cmd_fifo, "%s%s%s",
                    p_work_dir, FIFO_DIR, AW_CMD_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(ac_cmd_fifo, &ac_cmd_readfd, &ac_cmd_fd) == -1)
#else
      if ((ac_cmd_fd = open(ac_cmd_fifo, O_RDWR)) == -1)
#endif
      {
         (void)fprintf(stderr, _("Could not open() `%s' : %s (%s %d)\n"),
                       ac_cmd_fifo, strerror(errno), __FILE__, __LINE__);
      }
      else
      {
         if (send_cmd(RETRY, ac_cmd_fd) != SUCCESS)
         {
            (void)fprintf(stderr,
                          _("Was not able to send command RETRY to archive_check : %s (%s %d)\n"),
                          strerror(errno), __FILE__, __LINE__);
         }

#ifdef WITHOUT_FIFO_RW_SUPPORT
         if (close(ac_cmd_readfd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("close() error : %s"), strerror(errno));
         }
#endif
         if (close(ac_cmd_fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("close() error : %s"), strerror(errno));
         }
      }
   }

#ifdef AFDBENCH_CONFIG
   if (options & ENABLE_DIRECTORY_SCAN_OPTION)
   {
      if (attach_afd_status(NULL, WAIT_AFD_STATUS_ATTACH) < 0)
      {
         (void)fprintf(stderr,
                       _("ERROR   : Failed to attach to AFD status area. (%s %d)\n"),
                       __FILE__, __LINE__);
         exit(INCORRECT);
      }
      if (p_afd_status->amg_jobs & PAUSE_DISTRIBUTION)
      {
         p_afd_status->amg_jobs &= ~PAUSE_DISTRIBUTION;
      }
      detach_afd_status();
   }
#endif

   exit(errors);
}


/*++++++++++++++++++++++++++++ eval_input() +++++++++++++++++++++++++++++*/
static void
eval_input(int argc, char *argv[])
{
   int  correct = YES,       /* Was input/syntax correct?      */
        need_hostname = NO;
   char progname[128];

   (void)strcpy(progname, argv[0]);
   real_hostname[0] = '\0';
   real_hostname_pos = -1;

   /*****************************************************************/
   /* The following while loop checks for the parameters:           */
   /*                                                               */
   /*   -q              : start queue                               */
   /*   -Q              : stop queue                                */
   /*   -t              : start transfer                            */
   /*   -T              : stop transfer                             */
   /*   -b              : enable directory                          */
   /*   -B              : disable directory                         */
   /*   -j              : start directory                           */
   /*   -J              : stop directory                            */
   /*   -e              : enable host                               */
   /*   -E              : disable host                              */
   /*   -g              : enable delete data for host               */
   /*   -G              : disable delete data for host              */
   /*   -h <pos> <name> : change real_hostname to <name>            */
   /*   -s              : switch host                               */
   /*   -r              : retry                                     */
   /*   -R              : rescan directory                          */
   /*   -d              : enable/disable debug                      */
   /*   -c              : enable/disable trace                      */
   /*   -C              : enable/disable fulle trace                */
   /*   -I              : enable/disable simulate send mode         */
   /*   -f              : start FD                                  */
   /*   -F              : stop FD                                   */
   /*   -a              : start AMG                                 */
   /*   -A              : stop AMG                                  */
   /*   -U              : toggle start/stop directory               */
   /*   -W              : toggle enable/disable directory           */
   /*   -X              : toggle enable/disable host                */
   /*   -Y              : start/stop AMG                            */
   /*   -Z              : start/stop FD                             */
   /*   -k              : force file dir check                      */
   /*   -i              : reread local interface file               */
   /*   -o              : show exec statistics                      */
   /*   -P              : force archive check                       */
   /*   -S              : enable scanning of directories [afdbench] */
   /*                                                               */
   /*****************************************************************/
   while ((--argc > 0) && ((*++argv)[0] == '-'))
   {
      if (*(argv[0] + 2) == '\0')
      {
         switch (*(argv[0] + 1))
         {
            case 'q': /* Start queue. */
               options ^= START_QUEUE_OPTION;
               need_hostname = YES;
               break;

            case 'Q': /* Stop queue. */
               options ^= STOP_QUEUE_OPTION;
               need_hostname = YES;
               break;

            case 't': /* Start transfer. */
               options ^= START_TRANSFER_OPTION;
               need_hostname = YES;
               break;

            case 'T': /* Stop transfer. */
               options ^= STOP_TRANSFER_OPTION;
               need_hostname = YES;
               break;

            case 'b': /* Enable directory. */
               options ^= ENABLE_DIRECTORY_OPTION;
               need_hostname = YES;
               break;

            case 'B': /* Disable directory. */
               options ^= DISABLE_DIRECTORY_OPTION;
               need_hostname = YES;
               break;

            case 'j': /* Start directory. */
               options ^= START_DIRECTORY_OPTION;
               need_hostname = YES;
               break;

            case 'J': /* Stop directory. */
               options ^= STOP_DIRECTORY_OPTION;
               need_hostname = YES;
               break;

            case 'g': /* Enable delete data. */
               options ^= ENABLE_DELETE_DATA;
               need_hostname = YES;
               break;

            case 'G': /* Disable delete data. */
               options ^= DISABLE_DELETE_DATA;
               need_hostname = YES;
               break;

            case 'h': /* Modify real_hostname. */
               if ((argc > 2) && (argv[1][0] != '-') && (argv[2][0] != '-'))
               {
                  if ((argv[1][1] != '\0') && (argv[1][0] != '0') &&
                      (argv[1][0] != '1'))
                  {
                     (void)fprintf(stderr,
                                   _("ERROR  : Position can only be 0 and 1. (%s %d)\n"),
                                   __FILE__, __LINE__);
                     correct = NO;
                  }
                  else
                  {
                     if (argv[1][0] == '0')
                     {
                        real_hostname_pos = 0;
                     }
                     else
                     {
                        real_hostname_pos = 1;
                     }
                     if (strlen(argv[2]) > MAX_REAL_HOSTNAME_LENGTH)
                     {
                        (void)fprintf(stderr,
                                      _("ERROR  : real hostname to long, may only be %d characters long. (%s %d)\n"),
                                      MAX_REAL_HOSTNAME_LENGTH,
                                      __FILE__, __LINE__);
                        correct = NO;
                     }
                     else
                     {
                        (void)strcpy(real_hostname, argv[2]);
                        options2 ^= CHANGE_REAL_HOSTNAME;
                        need_hostname = YES;
                     }
                  }
                  argc -= 2;
                  argv += 2;
               }
               else
               {
                  (void)fprintf(stderr,
                                _("ERROR  : No position and/or real hostname provided for option -h. (%s %d)\n"),
                                __FILE__, __LINE__);
                  correct = NO;
               }
               break;

            case 'e': /* Enable host. */
               options ^= ENABLE_HOST_OPTION;
               need_hostname = YES;
               break;

            case 'E': /* Disable host. */
               options ^= DISABLE_HOST_OPTION;
               need_hostname = YES;
               break;

            case 's': /* Switch host. */
               options ^= SWITCH_OPTION;
               need_hostname = YES;
               break;

            case 'r': /* Retry. */
               options ^= RETRY_OPTION;
               need_hostname = YES;
               break;

            case 'R': /* Reread. */
               options ^= RESCAN_OPTION;
               need_hostname = YES;
               break;

            case 'd': /* Debug. */
               options ^= DEBUG_OPTION;
               need_hostname = YES;
               break;

            case 'c': /* Trace. */
               options ^= TRACE_OPTION;
               need_hostname = YES;
               break;

            case 'C': /* Full Trace. */
               options ^= FULL_TRACE_OPTION;
               need_hostname = YES;
               break;

            case 'I': /* Simulate Send Mode. */
               options2 ^= SIMULATE_SEND_MODE_OPTION;
               need_hostname = YES;
               break;

            case 'f': /* Start FD. */
               options ^= START_FD_OPTION;
               break;

            case 'F': /* Stop FD. */
               options ^= STOP_FD_OPTION;
               break;

            case 'a': /* Start AMG. */
               options ^= START_AMG_OPTION;
               break;

            case 'A': /* Stop AMG. */
               options ^= STOP_AMG_OPTION;
               break;

            case 'U': /* Toggle start/stop directory. */
               options ^= TOGGLE_STOP_DIRECTORY_OPTION;
               need_hostname = YES;
               break;

            case 'W': /* Toggle enable/disable directory. */
               options ^= TOGGLE_DISABLE_DIRECTORY_OPTION;
               need_hostname = YES;
               break;

            case 'X': /* Toggle enable/disable host. */
               options ^= TOGGLE_HOST_OPTION;
               need_hostname = YES;
               break;

            case 'Y': /* Start/Stop AMG. */
               options ^= START_STOP_AMG_OPTION;
               break;

            case 'Z': /* Start/Stop FD. */
               options ^= START_STOP_FD_OPTION;
               break;

            case 'k': /* Force file dir check. */
               options ^= FORCE_FILE_DIR_CHECK_OPTION;
               break;

            case 'i': /* Reread local interface file. */
               options ^= REREAD_LOCAL_INTERFACE_FILE_OPTION;
               break;

            case 'o': /* Show exec statistics. */
               options ^= SHOW_EXEC_STAT_OPTION;
               break;

            case 'O': /* Force a search for old files. */
               options2 ^= FORCE_SEARCH_OLD_FILES_OPTION;
               break;

            case 'P': /* Force archive check. */
               options ^= FORCE_ARCHIVE_CHECK_OPTION;
               break;

#ifdef AFDBENCH_CONFIG
            case 'S': /* Enable directory scanning. */
               options ^= ENABLE_DIRECTORY_SCAN_OPTION;
               break;
#endif

            default : /* Unknown parameter. */

               (void)fprintf(stderr,
                             _("ERROR  : Unknown parameter %c. (%s %d)\n"),
                            *(argv[0] + 1), __FILE__, __LINE__);
               correct = NO;
               break;

         } /* switch (*(argv[0] + 1)) */
      }
      else
      {
         (void)fprintf(stderr, _("ERROR  : Unknown option %s. (%s %d)\n"),
                      argv[0], __FILE__, __LINE__);
         correct = NO;
      }
   }

   if (correct != NO)
   {
      no_of_host_names = argc;
      if (no_of_host_names > 0)
      {
         int i = 0;

#if MAX_DIR_ALIAS_LENGTH > MAX_HOSTNAME_LENGTH
         RT_ARRAY(hosts, no_of_host_names, (MAX_DIR_ALIAS_LENGTH + 1), char);
#else
         RT_ARRAY(hosts, no_of_host_names, (MAX_HOSTNAME_LENGTH + 1), char);
#endif
         while (argc > 0)
         {
            (void)strcpy(hosts[i], argv[0]);
            argc--; argv++;
            i++;
         }
      }
      else
      {
         if (need_hostname == YES)
         {
            (void)fprintf(stderr, _("ERROR   : No host names specified!\n"));
            correct = NO;
         }
      }
   }

   /* If input is not correct show syntax. */
   if (correct == NO)
   {
      usage(progname);
      exit(1);
   }

   return;
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{                    
   (void)fprintf(stderr,
                 _("SYNTAX  : %s[ -w working directory][ -p <role>][ -u[ <fake user>]] options hostalias|diralias|position [... hostalias|diralias|position n]\n"),
                 progname);
   (void)fprintf(stderr,
                 _("    FSA options:\n"));
   (void)fprintf(stderr,
                 _("               -q              start queue\n"));
   (void)fprintf(stderr,
                 _("               -Q              stop queue\n"));
   (void)fprintf(stderr,
                 _("               -t              start transfer\n"));
   (void)fprintf(stderr,
                 _("               -T              stop transfer\n"));
   (void)fprintf(stderr,
                 _("               -g              enable delete data for host\n"));
   (void)fprintf(stderr,
                 _("               -G              disable delete data for host\n"));
   (void)fprintf(stderr,
                 _("               -h <pos> <name> change real hostname to <name>\n"));
   (void)fprintf(stderr,
                 _("               -e              enable host\n"));
   (void)fprintf(stderr,
                 _("               -E              disable host\n"));
   (void)fprintf(stderr,
                 _("               -s              switch host\n"));
   (void)fprintf(stderr,
                 _("               -r              retry\n"));
   (void)fprintf(stderr,
                 _("               -d              enable/disable debug\n"));
   (void)fprintf(stderr,
                 _("               -c              enable/disable trace\n"));
   (void)fprintf(stderr,
                 _("               -C              enable/disable full trace\n"));
   (void)fprintf(stderr,
                 _("               -I              enable/disable simulate send mode\n"));
   (void)fprintf(stderr,
                 _("               -X              toggle enable/disable host\n"));
   (void)fprintf(stderr,
                 _("    FRA options:\n"));
   (void)fprintf(stderr,
                 _("               -b              enable directory\n"));
   (void)fprintf(stderr,
                 _("               -B              disable directory\n"));
   (void)fprintf(stderr,
                 _("               -j              start directory\n"));
   (void)fprintf(stderr,
                 _("               -J              stop directory\n"));
   (void)fprintf(stderr,
                 _("               -R              rescan directory\n"));
   (void)fprintf(stderr,
                 _("               -U              toggle start/stop directory\n"));
   (void)fprintf(stderr,
                 _("               -W              toggle enable/disable directory\n"));
   (void)fprintf(stderr,
                 _("General options:\n"));
   (void)fprintf(stderr,
                 _("               -f              start FD\n"));
   (void)fprintf(stderr,
                 _("               -F              stop FD\n"));
   (void)fprintf(stderr,
                 _("               -a              start AMG\n"));
   (void)fprintf(stderr,
                 _("               -A              stop AMG\n"));
   (void)fprintf(stderr,
                 _("               -Y              start/stop AMG\n"));
   (void)fprintf(stderr,
                 _("               -Z              start/stop FD\n"));
#ifdef AFDBENCH_CONFIG
   (void)fprintf(stderr,
                 _("               -S              enable scanning of directories\n"));
#endif
   (void)fprintf(stderr,
                 _("               -k              force file dir check\n"));
   (void)fprintf(stderr,
                 _("               -i              reread local interface file\n"));
   (void)fprintf(stderr,
                 _("               -o              show exec statistics\n"));
   (void)fprintf(stderr,
                 _("               -O              force search for old files\n"));
   (void)fprintf(stderr,
                 _("               -P              force archive check\n"));
   (void)fprintf(stderr,
                 _("               -v              just print Version\n"));
   return;
}
