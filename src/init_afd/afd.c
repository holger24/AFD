/*
 *  afd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2023 Deutscher Wetterdienst (DWD),
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
 **   afd - controls startup and shutdown of AFD
 **
 ** SYNOPSIS
 **   afd [options]
 **    -a                        only start AFD
 **    --all                     in combination with -s or -S, stop all
 **                              process
 **    -A                        only start AFD, but do not scan directories
 **    -b                        Blocks starting of AFD
 **    -c[ <timeout in seconds>] only check if AFD is active
 **    -C[ <timeout in seconds>] check if AFD is active, if not start it
 **    -d                        only start afd_ctrl dialog
 **    -h[ <timeout in seconds>] only check for heartbeat
 **    -H[ <timeout in seconds>] check if heartbeat is active, if not start AFD
 **    -i[1-9]                   initialize AFD, the optional number is the
 **                              level of initialization. The higher the number
 **                              the more data will be deleted. If no number is
 **                              specified the level will be 5. 9 is the same
 **                              as -I below.
 **    -I                        initialize AFD, by deleting everything except
 **                              for the etc directory
 **    -n                        in combination with -i or -I just print
 **                              and do not execute.
 **    -p <role>                 use the given user role
 **    -r                        remove blocking startup of AFD
 **    -T                        check if data types match current binary
 **    -s                        shutdown AFD
 **    -S                        silent AFD shutdown
 **    -sn <name>                Provide a service name.
 **    -u[ <user>]               different user
 **    -w <work dir>             AFD working directory
 **    -v                        Just print the version number.
 **    --version                 Current version
 **    -z                        Set shared shutdown bit.
 **
 ** DESCRIPTION
 **   This program controls the startup or shutdown procedure of
 **   the AFD. When starting the following process are being
 **   initiated in this order:
 **
 **          init_afd         - Monitors all process of the AFD.
 **          system_log       - Logs all system activities.
 **          transfer_log     - Logs all transfer activities.
 **          trans_db_log     - Logs all debug transfer activities.
 **          receive_log      - Logs all receive activities.
 **          archive_watch    - Searches archive for old files and
 **                             removes them.
 **          input_log        - Logs all activities on input.
 **          distribution_log - Logs how data is distributed.
 **          production_log   - Logs all production activity such as
 **                             exec, rename, assemble, etc.
 **          output_log       - Logs activities on output (can be turned
 **                             on/off on a per job basis).
 **          delete_log       - Logs all files that are being removed
 **                             by the AFD.
 **          afd_stat         - Collects statistic information.
 **          amg              - Searches user directories and generates
 **                             messages for the FD.
 **          fd               - Reads messages from the AMG and distributes
 **                             the corresponding files.
 **          
 **
 ** RETURN VALUES
 **   None when successful. Otherwise if no response is received
 **   from the AFD, it will tell the user.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.04.1996 H.Kiehl Created
 **   30.05.1997 H.Kiehl Added permission check.
 **   13.08.1997 H.Kiehl When doing a shutdown, waiting for init_afd to
 **                      terminate.
 **   10.05.1998 H.Kiehl Added -c and -C options.
 **   16.06.2002 H.Kiehl Added -h and -H options.
 **   01.05.2004 H.Kiehl Added options to initialize AFD.
 **   23.04.2005 H.Kiehl Added -z option.
 **   07.04.2006 H.Kiehl When doing initialization only delete AFD files,
 **                      not AFD_MON files.
 **   10.01.2007 H.Kiehl Do not return -1 when doing a shutdown and
 **                      AFD is not active.
 **   03.02.2009 H.Kiehl Try handle the case better when the AFD_ACTIVE
 **                      file gets deleted while AFD is still active.
 **   04.03.2010 H.Kiehl When initializing with -i delete everything in
 **                      crc, incoming/file_mask and incoming/ls_data
 **                      directory.
 **   20.10.2011 H.Kiehl Added -T option.
 **   28.11.2012 H.Kiehl Let user choose the level of initialization.
 **   05.08.2013 H.Kiehl Check correct DIR_CONFIG in case when we specify
 **                      another one in AFD_CONFIG in function
 **                      check_afd_database().
 **   20.03.2014 H.Kiehl When AFD is on a global filesystem and we check
 **                      if AFD is active (-c), also check that we are on
 **                      the correct node.
 **   16.06.2020 H.Kiehl Add support for systemd.
 **
 */
DESCR__E_M1

#include <stdio.h>            /* fprintf(), remove(), NULL               */
#include <string.h>           /* strcpy(), strcat(), strerror()          */
#include <stdlib.h>           /* exit()                                  */
#include <ctype.h>            /* isdigit()                               */
#include <sys/types.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>        /* mmap(), munmap()                        */
#endif
#include <sys/stat.h>
#include <sys/time.h>         /* struct timeval, FD_SET...               */
#include <signal.h>           /* kill()                                  */
#include <fcntl.h>            /* O_RDWR, O_CREAT, O_WRONLY, etc          */
#include <unistd.h>           /* execlp(), chdir(), unlink(), R_OK, X_OK */
#include <errno.h>
#include "version.h"
#include "permission.h"
#include "logdefs.h"

/* Some local definitions. */
#define AFD_ONLY                  1
#define AFD_CHECK_ONLY            2
#define AFD_CHECK                 3
#define AFD_CTRL_ONLY             4
#define SHUTDOWN_ONLY             5
#define SILENT_SHUTDOWN_ONLY      6
#define START_BOTH                7
#define MAKE_BLOCK_FILE           8
#define REMOVE_BLOCK_FILE         9
#define AFD_HEARTBEAT_CHECK_ONLY 10
#define AFD_HEARTBEAT_CHECK      11
#define AFD_INITIALIZE           12
#define SET_SHUTDOWN_BIT         13

/* External global variables. */
int               pause_dir_check = NO;
int               sys_log_fd = STDERR_FILENO;
char              *p_work_dir,
                  afd_active_file[MAX_PATH_LENGTH],
                  afd_cmd_fifo[MAX_PATH_LENGTH],
                  *service_name;
const char        *sys_log_name = SYSTEM_LOG_FIFO;
struct afd_status *p_afd_status;

/* Local functions. */
static void       usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int          afd_ctrl_perm,
                dry_run = NO,
                initialize_perm,
                init_level = 0,
                ret,
                startup_perm,
                start_up,
#ifdef WITH_SYSTEMD
                stop_all = NO,
#endif
                shutdown_perm,
                user_offset;
   long         default_heartbeat_timeout = DEFAULT_HEARTBEAT_TIMEOUT;
   char         auto_block_file[MAX_PATH_LENGTH],
                exec_cmd[AFD_CTRL_LENGTH + 1],
                fake_user[MAX_FULL_USER_ID_LENGTH],
                *perm_buffer,
                profile[MAX_PROFILE_NAME_LENGTH + 1],
                sys_log_fifo[MAX_PATH_LENGTH],
                user[MAX_FULL_USER_ID_LENGTH],
                work_dir[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

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
   if (get_arg(&argc, argv, "-p", profile, MAX_PROFILE_NAME_LENGTH) == INCORRECT)
   {
      profile[0] = '\0';
      user_offset = 0;
   }
   else
   {
      (void)my_strncpy(user, profile, MAX_FULL_USER_ID_LENGTH);
      user_offset = strlen(profile);
   }
   if (get_argb(&argc, argv, "-sn", &service_name) == INCORRECT)
   {
      service_name = NULL;
   }
#ifdef WITH_SETUID_PROGS
   set_afd_euid(work_dir);
#endif

   check_fake_user(&argc, argv, AFD_CONFIG_FILE, fake_user);
   get_user(user, fake_user, user_offset);

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
         (void)fprintf(stderr, "%s [%s] (%s %d)\n",
                       PERMISSION_DENIED_STR, user, __FILE__, __LINE__);
         exit(INCORRECT);

      case SUCCESS  : /* Lets evaluate the permissions and see what */
                      /* the user may do.                           */
         if ((perm_buffer[0] == 'a') && (perm_buffer[1] == 'l') &&
             (perm_buffer[2] == 'l') &&
             ((perm_buffer[3] == '\0') || (perm_buffer[3] == ',') ||
              (perm_buffer[3] == ' ') || (perm_buffer[3] == '\t')))
         {
            afd_ctrl_perm = YES;
            shutdown_perm = YES;
            startup_perm  = YES;
            initialize_perm = YES;
         }
         else
         {
            if (lposi(perm_buffer, AFD_CTRL_PERM, AFD_CTRL_PERM_LENGTH) == NULL)
            {
               afd_ctrl_perm = NO_PERMISSION;
            }
            else
            {
               afd_ctrl_perm = YES;
            }
            if (lposi(perm_buffer, SHUTDOWN_PERM, SHUTDOWN_PERM_LENGTH) == NULL)
            {
               shutdown_perm = NO_PERMISSION;
            }
            else
            {
               shutdown_perm = YES;
            }
            if (lposi(perm_buffer, STARTUP_PERM, STARTUP_PERM_LENGTH) == NULL)
            {
               startup_perm = NO_PERMISSION;
            }
            else
            {
               startup_perm = YES;
            }
            if (lposi(perm_buffer, INITIALIZE_PERM, INITIALIZE_PERM_LENGTH) == NULL)
            {
               initialize_perm = NO_PERMISSION;
            }
            else
            {
               initialize_perm = YES;
            }
            free(perm_buffer);
         }
         break;

      case INCORRECT: /* Hmm. Something did go wrong. Since we want to */
                      /* be able to disable permission checking let    */
                      /* the user have all permissions.                */
         afd_ctrl_perm   = YES;
         shutdown_perm   = YES;
         startup_perm    = YES;
         initialize_perm = YES;
         break;

      default :
         (void)fprintf(stderr, _("Impossible!! Remove the programmer!\n"));
         exit(INCORRECT);
   }

   if (argc <= 3)
   {
      if ((argc == 2) ||
          ((argc == 3) && (argv[1][0] == '-') &&
           (((((argv[1][1] == 'C') || (argv[1][1] == 'c') ||
               (argv[1][1] == 'I') || (argv[1][1] == 'i')) &&
              (argv[1][2] == '\0')) ||
             ((argv[1][1] == 'i') && (isdigit((int)argv[1][2])))) ||
            (((argv[1][1] == 'S') || (argv[1][1] == 's')) &&
             (argv[2][0] == '-') && (argv[2][1] == '-') &&
             (argv[2][2] == 'a') && (argv[2][3] == 'l') &&
             (argv[2][4] == 'l') && (argv[2][5] == '\0')))))
      {
         if (my_strcmp(argv[1], "-a") == 0) /* Start AFD. */
         {
            if (startup_perm != YES)
            {
               (void)fprintf(stderr,
                             _("You do not have the permission to start the AFD.\n"));
               exit(INCORRECT);
            }
            start_up = AFD_ONLY;
         }
         else if (my_strcmp(argv[1], "-A") == 0) /* Start AFD but no dir scans. */
              {
                 if (startup_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  _("You do not have the permission to start the AFD.\n"));
                    exit(INCORRECT);
                 }
                 pause_dir_check = YES;
                 start_up = AFD_ONLY;
              }
         else if (my_strcmp(argv[1], "-b") == 0) /* Block auto restart. */
              {
                 start_up = MAKE_BLOCK_FILE;
              }
         else if (my_strcmp(argv[1], "-c") == 0) /* Only check if AFD is active. */
              {
                 start_up = AFD_CHECK_ONLY;
                 if (argc == 3)
                 {
                    default_heartbeat_timeout = atol(argv[2]);
                 }
              }
         else if (my_strcmp(argv[1], "-C") == 0) /* Only check if AFD is active. */
              {
                 if (startup_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  _("You do not have the permission to start the AFD.\n"));
                    exit(INCORRECT);
                 }
                 start_up = AFD_CHECK;
                 if (argc == 3)
                 {
                    default_heartbeat_timeout = atol(argv[2]);
                 }
              }
         else if (my_strcmp(argv[1], "-d") == 0) /* Start afd_ctrl. */
              {
                 if (afd_ctrl_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  _("You do not have the permission to start the AFD control dialog.\n"));
                    exit(INCORRECT);
                 }
                 start_up = AFD_CTRL_ONLY;
              }
         else if (my_strcmp(argv[1], "-h") == 0) /* Only check for heartbeat. */
              {
                 start_up = AFD_HEARTBEAT_CHECK_ONLY;
                 if (argc == 3)
                 {
                    default_heartbeat_timeout = atol(argv[2]);
                 }
              }
         else if (my_strcmp(argv[1], "-H") == 0) /* If there is no heartbeat start AFD. */
              {
                 if (startup_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  _("You do not have the permission to start the AFD.\n"));
                    exit(INCORRECT);
                 }
                 start_up = AFD_HEARTBEAT_CHECK;
                 if (argc == 3)
                 {
                    default_heartbeat_timeout = atol(argv[2]);
                 }
              }
         else if (my_strcmp(argv[1], "-i") == 0) /* Initialize AFD. */
              {
                 if (initialize_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  _("You do not have the permission to initialize the AFD.\n"));
                    exit(INCORRECT);
                 }
                 init_level = 5;
                 start_up = AFD_INITIALIZE;
                 if (argc == 3)
                 {
                    if (my_strcmp(argv[2], "-n") == 0) /* Just print, do not execute. */
                    {
                       dry_run = YES;
                    }
                    else
                    {
                       usage(argv[0]);
                       exit(1);
                    }
                 }
              }
         else if ((argv[1][0] == '-') && (argv[1][1] == 'i') && /* Initialize AFD with init level. */
                  (isdigit((int)argv[1][2])) && (argv[1][3] == '\0'))
              {
                 if (initialize_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  _("You do not have the permission to initialize the AFD.\n"));
                    exit(INCORRECT);
                 }
                 init_level = (int)argv[1][2] - 48;
                 start_up = AFD_INITIALIZE;
                 if (argc == 3)
                 {
                    if (my_strcmp(argv[2], "-n") == 0) /* Just print, do not execute. */
                    {
                       dry_run = YES;
                    }
                    else
                    {
                       usage(argv[0]);
                       exit(1);
                    }
                 }
              }
         else if (my_strcmp(argv[1], "-I") == 0) /* Full Initialization of AFD. */
              {
                 if (initialize_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  _("You do not have the permission to do a full initialization of AFD.\n"));
                    exit(INCORRECT);
                 }
                 init_level = 9;
                 start_up = AFD_INITIALIZE;
                 if (argc == 3)
                 {
                    if (my_strcmp(argv[2], "-n") == 0) /* Just print, do not execute. */
                    {
                       dry_run = YES;
                    }
                    else
                    {
                       usage(argv[0]);
                       exit(1);
                    }
                 }
              }
         else if (my_strcmp(argv[1], "-s") == 0) /* Shutdown AFD. */
              {
                 if (shutdown_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  _("You do not have the permission to shutdown the AFD. [%s]\n"),
                                  user);
                    exit(INCORRECT);
                 }
#ifdef WITH_SYSTEMD
                 if ((argc == 3) &&
                     (my_strcmp(argv[2], "--all") == 0))
                 {
                    stop_all = YES;
                 }
#endif
                 start_up = SHUTDOWN_ONLY;
              }
         else if (my_strcmp(argv[1], "-S") == 0) /* Silent AFD shutdown. */
              {
                 if (shutdown_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  _("You do not have the permission to shutdown the AFD. [%s]\n"),
                                  user);
                    exit(INCORRECT);
                 }
#ifdef WITH_SYSTEMD
                 if ((argc == 3) &&
                     (my_strcmp(argv[2], "--all") == 0))
                 {
                    stop_all = YES;
                 }
#endif
                 start_up = SILENT_SHUTDOWN_ONLY;
              }
         else if (my_strcmp(argv[1], "-r") == 0) /* Removes blocking file. */
              {
                 start_up = REMOVE_BLOCK_FILE;
              }
         else if (my_strcmp(argv[1], "-T") == 0) /* Check if data types match */
                                              /* current binary.           */
              {
                 int changes;

                 changes = check_typesize_data(NULL, stdout, NO);
                 if (changes > 0)
                 {
                    (void)fprintf(stdout,
                                  _("There are %d changes. Database needs to be reinitialized with 'afd -i'\n"),
                                  changes);
                    (void)fprintf(stdout,
                                  _("To see exactly what has changed, see %s%s/%s0 for more details.\n"),
                                  p_work_dir, LOG_DIR, SYSTEM_LOG_NAME);
                 }
                 else
                 {
                    if (changes == 0)
                    {
                       (void)fprintf(stdout,
                                     _("Database matches compiled version.\n"));
                    }
                    else
                    {
                       (void)fprintf(stdout,
                                     _("Failed to check if there are changes. See %s%s/%s0 for more details.\n"),
                                     p_work_dir, LOG_DIR, SYSTEM_LOG_NAME);
                    }
                 }

                 exit(changes);
              }
         else if (my_strcmp(argv[1], "-z") == 0) /* Set shutdown bit. */
              {
                 if (shutdown_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  _("You do not have the permission to set the shutdown bit. [%s]\n"),
                                  user);
                    exit(INCORRECT);
                 }
                 start_up = SET_SHUTDOWN_BIT;
              }
         else if ((my_strcmp(argv[1], "--help") == 0) ||
                  (my_strcmp(argv[1], "-?") == 0)) /* Show usage. */
              {
                 usage(argv[0]);
                 exit(SUCCESS);
              }
              else
              {
                 usage(argv[0]);
                 exit(1);
              }
      }
      else /* Start AFD and afd_ctrl. */
      {
         if ((startup_perm == YES) && (afd_ctrl_perm == YES))
         {
            start_up = START_BOTH;
         }
         else if (startup_perm == YES)
              {
                 start_up = AFD_ONLY;
              }
         else if (afd_ctrl_perm == YES)
              {
                 start_up = AFD_CTRL_ONLY;
              }
              else
              {
                 (void)fprintf(stderr,
                               _("You do not have enough permissions to use this program.\n"));
                 exit(INCORRECT);
              }
      }
   }
   else
   {
      usage(argv[0]);
      exit(1);
   }

   (void)umask(0);
   if (chdir(work_dir) < 0)
   {
      (void)fprintf(stderr,
                    _("ERROR   : Failed to change directory to `%s' : %s (%s %d)\n"),
                    work_dir, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Initialise variables. */
   (void)strcpy(auto_block_file, work_dir);
   (void)strcat(auto_block_file, ETC_DIR);
   (void)strcat(auto_block_file, BLOCK_FILE);
   (void)strcpy(afd_active_file, work_dir);
   (void)strcat(afd_active_file, FIFO_DIR);
   if (check_dir(afd_active_file, R_OK | X_OK) < 0)
   {
      exit(INCORRECT);
   }
   (void)strcpy(sys_log_fifo, afd_active_file);
   (void)strcat(sys_log_fifo, SYSTEM_LOG_FIFO);
   (void)strcpy(afd_cmd_fifo, afd_active_file);
   (void)strcat(afd_cmd_fifo, AFD_CMD_FIFO);
   (void)strcat(afd_active_file, AFD_ACTIVE_FILE);

#ifdef HAVE_STATX
   if ((statx(0, sys_log_fifo, AT_STATX_SYNC_AS_STAT,
              STATX_MODE, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.stx_mode)))
#else
   if ((stat(sys_log_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
#endif
   {
      if (make_fifo(sys_log_fifo) < 0)
      {
         (void)fprintf(stderr,
                       _("ERROR   : Could not create fifo `%s'. (%s %d)\n"),
                       sys_log_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   if ((start_up == SHUTDOWN_ONLY) || (start_up == SILENT_SHUTDOWN_ONLY))
   {
      int   loops = 0,
            n,
            readfd;
      pid_t ia_pid;

      p_afd_status = NULL;
      if (attach_afd_status(NULL, 5) == SUCCESS)
      {
         if (p_afd_status->hostname[0] != '\0')
         {
            char hostname[MAX_REAL_HOSTNAME_LENGTH];

            if (gethostname(hostname, MAX_REAL_HOSTNAME_LENGTH) == 0)
            {
               if (my_strcmp(hostname, p_afd_status->hostname) != 0)
               {
                  if (start_up == SHUTDOWN_ONLY)
                  {
                     (void)fprintf(stderr,
                                   _("Shutdown can only be done on %s or use -z.\n"),
                                   p_afd_status->hostname);
                  }
                  exit(NOT_ON_CORRECT_HOST);
               }
            }
         }
         (void)detach_afd_status();
      }

      /* First get the pid of init_afd before we send the */
      /* shutdown command.                                */
      if ((readfd = open(afd_active_file, O_RDONLY)) == -1)
      {
         if (errno != ENOENT)
         {
            (void)fprintf(stderr, _("Failed to open `%s' : %s (%s %d)\n"),
                          afd_active_file, strerror(errno),
                          __FILE__, __LINE__);
            exit(INCORRECT);
         }
#ifdef WITH_SYSTEMD
         if ((n = shutdown_afd(user, 1L, YES, stop_all)) == 2)
#else
         if ((n = shutdown_afd(user, 1L, YES)) == 2)
#endif
         {
            if (start_up == SHUTDOWN_ONLY)
            {
               (void)fprintf(stderr, _("There is no AFD active.\n"));
            }
            n = AFD_IS_NOT_ACTIVE;
         }
         else if (n != 0)
              {
                 if (start_up == SHUTDOWN_ONLY)
                 {
                    (void)fprintf(stderr,
                                  _("ERROR   : An error (%d) occured when shutting down, see SYSTEM_LOG for more information.\n"),
                                  n);
                 }
                 n = INCORRECT;
              }

         exit(n);
      }
      if ((n = read(readfd, &ia_pid, sizeof(pid_t))) != sizeof(pid_t))
      {
         if (n == 0)
         {
            (void)fprintf(stderr,
                          _("File `%s' is empty. Unable to determine if AFD is active.\n"),
                          afd_active_file);
         }
         else
         {
            (void)fprintf(stderr, _("read() error : %s (%s %d)\n"),
                          strerror(errno),  __FILE__, __LINE__);
         }
         exit(INCORRECT);
      }
      (void)close(readfd);

      /* Check that we have a valid pid! */
      if (ia_pid < 1)
      {
         (void)fprintf(stderr,
#if SIZEOF_PID_T == 4
                       _("File %s contains an invalid pid (%d). Please try and terminate it by hand.\n"),
#else
                       _("File %s contains an invalid pid (%lld). Please try and terminate it by hand.\n"),
#endif
                       afd_active_file, (pri_pid_t)ia_pid);
         exit(INCORRECT);
      }

      if (start_up == SHUTDOWN_ONLY)
      {
         (void)fprintf(stdout, _("Starting AFD shutdown "));
         fflush(stdout);
      }

      /*
       * First check if init_afd process is there.
       */
      if ((kill(ia_pid, 0) == -1) && (errno == ESRCH))
      {
         if (start_up == SHUTDOWN_ONLY)
         {
            (void)fprintf(stdout, ".");
            fflush(stdout);
         }

         /*
          * No init_afd seems active. Lets still send a stop and
          * try listen to heartbeat. But regardless if we succeed,
          * lets return 0 (success) when we exit. Assume that no
          * AFD was running.
          */
#ifdef WITH_SYSTEMD
         (void)shutdown_afd(user, 3L, NO, stop_all);
#else
         (void)shutdown_afd(user, 3L, NO);
#endif
         (void)unlink(afd_active_file);
         if (start_up == SHUTDOWN_ONLY)
         {
            (void)fprintf(stdout, "\nDone! Note, no %s process!!!\n", AFD);
         }
         exit(0);
      }

#ifdef WITH_SYSTEMD
      (void)shutdown_afd(user, 10L, NO, stop_all);
#else
      (void)shutdown_afd(user, 10L, NO);
#endif

#ifdef WITH_SYSTEMD
      if (stop_all == YES)
      {
#endif
         /*
          * Wait for init_afd to terminate. But lets not wait forever.
          */
         for (;;)
         {
            if ((access(afd_active_file, F_OK) == -1) && (errno == ENOENT))
            {
               if (start_up == SHUTDOWN_ONLY)
               {
                  (void)fprintf(stdout, "\nDone!\n");
               }
               exit(0);
            }

            if ((start_up == SHUTDOWN_ONLY) && ((loops % 10) == 0))
            {
               (void)fprintf(stdout, ".");
               fflush(stdout);
            }
            my_usleep(100000L);

            if (loops++ >= 1200)
            {
               (void)fprintf(stdout, _("\nTimeout reached, killing %s.\n"), AFD);
               if (kill(ia_pid, SIGINT) == -1)
               {
                  if (errno == ESRCH)
                  {
                     (void)fprintf(stderr, _("init_afd already gone (%s %d)\n"),
                                   __FILE__, __LINE__);
                     exit(0);
                  }
                  (void)fprintf(stderr,
#if SIZEOF_PID_T == 4
                                _("Failed to kill init_afd (%d) : %s (%s %d)\n"),
#else
                                _("Failed to kill init_afd (%lld) : %s (%s %d)\n"),
#endif
                                (pri_pid_t)ia_pid, strerror(errno),
                                __FILE__, __LINE__);
               }
               else
               {
                  if (start_up == SHUTDOWN_ONLY)
                  {
                     (void)fprintf(stdout, _("\nDone!\n"));
                  }
               }
               break;
            }
         }

         /*
          * Before we exit lets check if init_afd is really gone.
          */
         loops = 0;
         for (;;)
         {
            if ((access(afd_active_file, F_OK) == -1) && (errno == ENOENT))
            {
               break;
            }
            my_usleep(100000L);

            if (loops++ >= 400)
            {
               (void)fprintf(stdout,
                             _("\nSecond timeout reached, killing init_afd the hard way.\n"));
               if (kill(ia_pid, SIGKILL) == -1)
               {
                  (void)fprintf(stderr,
#if SIZEOF_PID_T == 4
                                _("Failed to kill init_afd (%d) : %s (%s %d)\n"),
#else
                                _("Failed to kill init_afd (%lld) : %s (%s %d)\n"),
#endif
                                (pri_pid_t)ia_pid, strerror(errno),
                                __FILE__, __LINE__);
               }
               break;
            }
         }
#ifdef WITH_SYSTEMD
      }
      else
      {
         if (start_up == SHUTDOWN_ONLY)
         {
            (void)fprintf(stdout, _("\nDone!\n"));
         }
      }
#endif

      exit(0);
   }
   else if (start_up == AFD_CTRL_ONLY)
        {
           (void)strcpy(exec_cmd, AFD_CTRL);
           if (profile[0] == '\0')
           {
              ret = execlp(exec_cmd, exec_cmd, WORK_DIR_ID, work_dir,
                           (char *) 0);
           }
           else
           {
              ret = execlp(exec_cmd, exec_cmd, WORK_DIR_ID, work_dir,
                           "-p", profile, (char *) 0);
           }
           if (ret == -1)
           {
              (void)fprintf(stderr,
                            _("ERROR   : Failed to execute %s : %s (%s %d)\n"),
                            exec_cmd, strerror(errno), __FILE__, __LINE__);
              exit(1);
           }
           exit(0);
        }
   else if (start_up == AFD_ONLY)
        {
           /* Check if starting of AFD is currently disabled.  */
           if (eaccess(auto_block_file, F_OK) == 0)
           {
              (void)fprintf(stderr,
                            _("AFD is currently disabled by system manager.\n"));
              exit(AFD_DISABLED_BY_SYSADM);
           }

           if (check_afd_database() == -1)
           {
              (void)fprintf(stderr,
                            _("ERROR   : Cannot read database file (DIR_CONFIG) : %s\n          Unable to start AFD.\n"),
                            strerror(errno));
              exit(INCORRECT);
           }

           if ((ret = check_afd_heartbeat(DEFAULT_HEARTBEAT_TIMEOUT, NO)) == 1)
           {
              if (attach_afd_status(NULL, 5) == SUCCESS)
              {
                 if (p_afd_status->hostname[0] != '\0')
                 {
                    char hostname[MAX_REAL_HOSTNAME_LENGTH];

                    if (gethostname(hostname, MAX_REAL_HOSTNAME_LENGTH) == 0)
                    {
                       if (my_strcmp(p_afd_status->hostname, hostname) == 0)
                       {
                          (void)fprintf(stdout,
                                        _("AFD is active on %s in %s\n"),
                                        hostname, p_work_dir);
                          ret = AFD_IS_ACTIVE;
                       }
                       else
                       {
                          (void)fprintf(stdout,
                                        _("No AFD is active on %s in %s, but is active on %s\n"),
                                        hostname, p_work_dir,
                                        p_afd_status->hostname);
                          ret = NOT_ON_CORRECT_HOST;
                       }
                    }
                    else
                    {
                       (void)fprintf(stdout,
                                     _("AFD is active on %s in %s\n"),
                                     p_afd_status->hostname, p_work_dir);
                       ret = AFD_IS_ACTIVE;
                    }
                 }
                 else
                 {
                    (void)fprintf(stdout,
                                  _("AFD is active in %s\n"), p_work_dir);
                    ret = AFD_IS_ACTIVE;
                 }
                 (void)detach_afd_status();
              }
              else
              {
                 (void)fprintf(stdout,
                               _("AFD is active in %s\n"), p_work_dir);
                 ret = AFD_IS_ACTIVE;
              }
              exit(ret);
           }
#ifdef WITH_SYSTEMD
           else if (ret == 3)
                {
                   ret = send_start_afd(user, 15);
                   exit(ret);
                }
#endif

           if (startup_afd() != YES)
           {
              exit(INCORRECT);
           }

           exit(0);
        }
   else if ((start_up == AFD_CHECK) || (start_up == AFD_CHECK_ONLY) ||
            (start_up == AFD_HEARTBEAT_CHECK) ||
            (start_up == AFD_HEARTBEAT_CHECK_ONLY))
        {
           int remove_process;

           if ((start_up == AFD_CHECK_ONLY) ||
               (start_up == AFD_HEARTBEAT_CHECK_ONLY))
           {
              remove_process = NO;
           }
           else
           {
              remove_process = YES;
           }
           if ((ret = check_afd_heartbeat(default_heartbeat_timeout,
                                          remove_process)) == 1)
           {
              if (attach_afd_status(NULL, 5) == SUCCESS)
              {
                 if (p_afd_status->hostname[0] != '\0')
                 {
                    char hostname[MAX_REAL_HOSTNAME_LENGTH];

                    if (gethostname(hostname, MAX_REAL_HOSTNAME_LENGTH) == 0)
                    {
                       if (my_strcmp(p_afd_status->hostname, hostname) == 0)
                       {
                          (void)fprintf(stdout,
                                        _("AFD is active on %s in %s\n"),
                                        hostname, p_work_dir);
                          ret = AFD_IS_ACTIVE;
                       }
                       else
                       {
                          (void)fprintf(stdout,
                                        _("No AFD is active on %s in %s, but is active on %s\n"),
                                        hostname, p_work_dir,
                                        p_afd_status->hostname);
                          ret = NOT_ON_CORRECT_HOST;
                       }
                    }
                    else
                    {
                       (void)fprintf(stdout,
                                     _("AFD is active on %s in %s\n"),
                                     p_afd_status->hostname, p_work_dir);
                       ret = AFD_IS_ACTIVE;
                    }
                 }
                 else
                 {
                    (void)fprintf(stdout,
                                  _("AFD is active in %s\n"), p_work_dir);
                    ret = AFD_IS_ACTIVE;
                 }
                 (void)detach_afd_status();
              }
              else
              {
                 (void)fprintf(stdout,
                               _("AFD is active in %s\n"), p_work_dir);
                 ret = AFD_IS_ACTIVE;
              }
              exit(ret);
           }
           else if (ret == 2)
                {
                   (void)fprintf(stdout,
                                 _("AFD NOT responding within %ld seconds!\n"),
                                 default_heartbeat_timeout);
                   exit(AFD_NOT_RESPONDING);
                }

           if ((start_up == AFD_CHECK) ||
               (start_up == AFD_HEARTBEAT_CHECK))
           {
              /* Check if starting of AFD is currently disabled.  */
              if (eaccess(auto_block_file, F_OK) == 0)
              {
                 (void)fprintf(stderr,
                               _("AFD is currently disabled by system manager.\n"));
                 exit(AFD_DISABLED_BY_SYSADM);
              }

              if (check_afd_database() == -1)
              {
                 (void)fprintf(stderr,
                               _("Cannot read database file (DIR_CONFIG) : %s\nUnable to start AFD.\n"),
                               strerror(errno));
                 exit(NO_DIR_CONFIG);
              }

#ifdef WITH_SYSTEMD
              if (ret == 3)
              {
                 ret = send_start_afd(user, 15);
              }
              else
              {
#endif
                 ret = startup_afd();
#ifdef WITH_SYSTEMD
              }
#endif
              if (ret != YES)
              {
                 exit(INCORRECT);
              }
           }
           else
           {
              (void)fprintf(stderr, _("No AFD active in %s\n"), p_work_dir);
           }

           exit(0);
        }
   else if (start_up == MAKE_BLOCK_FILE)
        {
           int fd;

           if ((fd = open(auto_block_file, O_WRONLY | O_CREAT | O_TRUNC,
#ifdef GROUP_CAN_WRITE
                          S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) == -1)
#else
                          S_IRUSR | S_IWUSR)) == -1)
#endif
           {
              (void)fprintf(stderr,
                            _("ERROR   : Failed to create block file `%s' : %s (%s %d)\n"),
                            auto_block_file, strerror(errno),
                            __FILE__, __LINE__);
              exit(INCORRECT);
           }
           if (close(fd) == -1)
           {
              (void)fprintf(stderr,
                            _("WARNING : Failed to close() block file `%s' : %s (%s %d)\n"),
                            auto_block_file, strerror(errno),
                            __FILE__, __LINE__);
           }
           exit(SUCCESS);
        }
   else if (start_up == REMOVE_BLOCK_FILE)
        {
           if (remove(auto_block_file) < 0)
           {
              (void)fprintf(stderr,
                            _("ERROR   : Failed to remove block file `%s' : %s (%s %d)\n"),
                            auto_block_file, strerror(errno),
                            __FILE__, __LINE__);
              exit(INCORRECT);
           }
           exit(SUCCESS);
        }
   else if (start_up == AFD_INITIALIZE)
        {
#ifdef WITH_SYSTEMD
           if (((ret = check_afd_heartbeat(DEFAULT_HEARTBEAT_TIMEOUT, NO)) == 1) ||
               (ret == 3))
#else
           if (check_afd_heartbeat(DEFAULT_HEARTBEAT_TIMEOUT, NO) == 1)
#endif
           {
              (void)fprintf(stderr,
                            _("ERROR   : AFD is still active, unable to initialize.\n"));
              exit(INCORRECT);
           }
           else
           {
              initialize_db(init_level, NULL, dry_run);
              exit(SUCCESS);
           }
        }
   else if (start_up == SET_SHUTDOWN_BIT)
        {
           int   fd;
           off_t offset;
           char  *ptr,
                 *shared_shutdown;

           if ((fd = open(afd_active_file, O_RDWR)) == -1)
           {
              (void)fprintf(stderr,
                            _("ERROR   : Failed to open() `%s' : %s (%s %d)\n"),
                            afd_active_file, strerror(errno),
                            __FILE__, __LINE__);
              exit(INCORRECT);
           }
#ifdef HAVE_STATX
           if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                     STATX_SIZE, &stat_buf) == -1)
#else
           if (fstat(fd, &stat_buf) == -1)
#endif
           {
              (void)fprintf(stderr,
#ifdef HAVE_STATX
                            _("ERROR   : Failed to statx() `%s' : %s (%s %d)\n"),
#else
                            _("ERROR   : Failed to fstat() `%s' : %s (%s %d)\n"),
#endif
                            afd_active_file, strerror(errno),
                            __FILE__, __LINE__);
              exit(INCORRECT);
           }
           offset = ((NO_OF_PROCESS + 1) * sizeof(pid_t)) + sizeof(unsigned int) + 1 + 1;
#ifdef HAVE_STATX
           if ((offset + 1) != stat_buf.stx_size)
#else
           if ((offset + 1) != stat_buf.st_size)
#endif
           {
              (void)fprintf(stderr,
#if SIZEOF_OFF_T == 4
                            _("ERROR   : Unable to set shutdown bit due to incorrect size (%ld != %ld) of %s.\n"),
#else
                            _("ERROR   : Unable to set shutdown bit due to incorrect size (%lld != %lld) of %s.\n"),
#endif
                            (pri_off_t)(offset + 1),
#ifdef HAVE_STATX
                            (pri_off_t)stat_buf.stx_size,
#else
                            (pri_off_t)stat_buf.st_size,
#endif
                            afd_active_file);
              exit(INCORRECT);
           }
#ifdef HAVE_MMAP
           if ((ptr = mmap(NULL, offset, (PROT_READ | PROT_WRITE),
                           MAP_SHARED, fd, 0)) == (caddr_t) -1)
#else
           if ((ptr = mmap_emu(NULL, offset, (PROT_READ | PROT_WRITE),
                               MAP_SHARED, afd_active_file, 0)) == (caddr_t) -1)
#endif
           {
              (void)fprintf(stderr, _("ERROR   : mmap() error : %s (%s %d)\n"),
                            strerror(errno), __FILE__, __LINE__);
              exit(INCORRECT);
           }
           offset -= (sizeof(unsigned int) + 1 + 1);
           shared_shutdown = ptr + offset + sizeof(unsigned int);
           *shared_shutdown = SHUTDOWN;
           (void)fprintf(stdout, "Shutdown bit set.\n");
           exit(SUCCESS);
        }

   /* Check if starting of AFD is currently disabled.  */
   if (eaccess(auto_block_file, F_OK) == 0)
   {
      (void)fprintf(stderr, _("AFD is currently disabled by system manager.\n"));
      exit(AFD_DISABLED_BY_SYSADM);
   }

   /* Is another AFD active in this directory? */
   if (check_afd_heartbeat(DEFAULT_HEARTBEAT_TIMEOUT, YES) == 1)
   {
      /* Another AFD is active. Only start afd_ctrl. */
      (void)strcpy(exec_cmd, AFD_CTRL);
      if (profile[0] == '\0')
      {
         ret = execlp(exec_cmd, exec_cmd, WORK_DIR_ID, work_dir,
                      (char *) 0);
      }
      else
      {
         ret = execlp(exec_cmd, exec_cmd, WORK_DIR_ID, work_dir,
                      "-p", profile, (char *) 0);
      }
      if (ret == -1)
      {
         (void)fprintf(stderr,
                       _("ERROR   : Failed to execute %s : %s (%s %d)\n"),
                       exec_cmd, strerror(errno), __FILE__, __LINE__);
         exit(1);
      }
   }
   else /* Start both. */
   {
      if (check_afd_database() == -1)
      {
         (void)fprintf(stderr,
                       _("Cannot read database file (DIR_CONFIG) : %s\nUnable to start AFD.\n"),
                       strerror(errno));
         exit(INCORRECT);
      }

      if ((ret = check_afd_heartbeat(DEFAULT_HEARTBEAT_TIMEOUT, NO)) == 1)
      {
         /* AFD is already up and running. */
         (void)strcpy(exec_cmd, AFD_CTRL);
         if (profile[0] == '\0')
         {
            ret = execlp(exec_cmd, exec_cmd, WORK_DIR_ID, work_dir,
                         (char *) 0);
         }
         else
         {
            ret = execlp(exec_cmd, exec_cmd, WORK_DIR_ID, work_dir,
                         "-p", profile, (char *) 0);
         }
         if (ret == -1)
         {
            (void)fprintf(stderr,
                          _("ERROR   : Failed to execute %s : %s (%s %d)\n"),
                          exec_cmd, strerror(errno),
                          __FILE__, __LINE__);
            exit(1);
         }
      }
      else
      {
#ifdef WITH_SYSTEMD
         if (ret == 3)
         {
            ret = send_start_afd(user, 15);
         }
         else
         {
#endif
            ret = startup_afd();
#ifdef WITH_SYSTEMD
         }
#endif
         if (ret == YES)
         {
            (void)strcpy(exec_cmd, AFD_CTRL);
            if (profile[0] == '\0')
            {
               ret = execlp(exec_cmd, exec_cmd, WORK_DIR_ID, work_dir,
                            (char *) 0);
            }
            else
            {
               ret = execlp(exec_cmd, exec_cmd, WORK_DIR_ID, work_dir,
                            "-p", profile, (char *) 0);
            }
            if (ret == -1)
            {
               (void)fprintf(stderr,
                             _("ERROR   : Failed to execute %s : %s (%s %d)\n"),
                             exec_cmd, strerror(errno),
                             __FILE__, __LINE__);
               exit(1);
            }
         }
         else
         {
            exit(INCORRECT);
         }
      }
   }

   exit(0);
}


/*+++++++++++++++++++++++++++++++ usage() +++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{
   (void)fprintf(stderr, _("Usage: %s[ -w <AFD working dir>][ -p <role>][ -u[ <user>]] [option]\n"), progname);
   (void)fprintf(stderr, _("\n   Other possible options:\n"));
   (void)fprintf(stderr, _("    -a                        only start AFD\n"));
#ifdef WITH_SYSTEMD
   (void)fprintf(stderr, _("    --all                     in combination with -s or -S, stop all process\n"));
#endif
   (void)fprintf(stderr, _("    -A                        only start AFD, but do not scan directories\n"));
   (void)fprintf(stderr, _("    -b                        blocks starting of AFD\n"));
   (void)fprintf(stderr, _("    -c[ <timeout in seconds>] only check if AFD is active\n"));
   (void)fprintf(stderr, _("    -C[ <timeout in seconds>] check if AFD is active, if not start it\n"));
   (void)fprintf(stderr, _("    -d                        only start afd_ctrl dialog\n"));
   (void)fprintf(stderr, _("    -h[ <timeout in seconds>] only check for heartbeat\n"));
   (void)fprintf(stderr, _("    -H[ <timeout in seconds>] check if heartbeat is active, if not start AFD\n"));
   (void)fprintf(stderr, _("    -i[1-9]                   initialize AFD, the optional number is the\n"));
   (void)fprintf(stderr, _("                              level of initialization. The higher the number\n"));
   (void)fprintf(stderr, _("                              the more data will be deleted. If no number is\n"));
   (void)fprintf(stderr, _("                              specified the level will be 5. 9 is the same\n"));
   (void)fprintf(stderr, _("                              as -I below. As of a level 7 it will not try\n"));
   (void)fprintf(stderr, _("                              to restore any values set via afdcfg.\n"));
   (void)fprintf(stderr, _("    -I                        initialize AFD, by deleting everything\n"));
   (void)fprintf(stderr, _("                              except for etc directory\n"));
   (void)fprintf(stderr, _("    -n                        in combination with -i or -I just print\n"));
   (void)fprintf(stderr, _("                              and do not execute.\n"));
   (void)fprintf(stderr, _("    -r                        removes blocking startup of AFD\n"));
   (void)fprintf(stderr, _("    -s                        shutdown AFD\n"));
   (void)fprintf(stderr, _("    -S                        silent AFD shutdown\n"));
   (void)fprintf(stderr, _("    -sn <name>                Provide a service name.\n"));
   (void)fprintf(stderr, _("    -T                        check if data types match current binary\n"));
   (void)fprintf(stderr, _("    -z                        set shutdown bit\n"));
   (void)fprintf(stderr, _("    --help                    prints out this syntax\n"));
   (void)fprintf(stderr, _("    -v                        just print version number\n"));
   (void)fprintf(stderr, _("    --version                 show current version\n"));
   (void)fprintf(stderr, _("\n   Possible return values:\n"));
   (void)fprintf(stderr, _("    %d                       No DIR_CONFIG.\n"), NO_DIR_CONFIG);
   (void)fprintf(stderr, _("    %d                        Success.\n"), SUCCESS);
   (void)fprintf(stderr, _("    %d                        AFD is active.\n"), AFD_IS_ACTIVE);
   (void)fprintf(stderr, _("    %d                        AFD is disabled by sysadm.\n"), AFD_DISABLED_BY_SYSADM);
   (void)fprintf(stderr, _("    %d                        AFD not responding.\n"), AFD_NOT_RESPONDING);
   (void)fprintf(stderr, _("    %d                       AFD not active.\n"), AFD_IS_NOT_ACTIVE);
   (void)fprintf(stderr, _("    %d                       Not on correct host.\n"), NOT_ON_CORRECT_HOST);

   return;
}
