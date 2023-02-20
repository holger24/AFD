/*
 *  mafd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   mafd - controls startup and shutdown of AFD_MON
 **
 ** SYNOPSIS
 **   mafd [options]
 **          -a            only start AFD_MON
 **          --all         in combination with -s or -S, stop all
 **                        process
 **          -c            only check if AFD_MON is active
 **          -C            check if AFD_MON is active, if not start it
 **          -d            only start mon_ctrl dialog
 **          -i            initialize AFD_MON, by deleting fifodir
 **          -I            initialize AFD_MON, by deleting everything except
 **                        for the etc directory
 **          -p <role>     role to use
 **          -s            shutdown AFD_MON
 **          -S            silent AFD_MON shutdown
 **          -u[ <user>]   fake user
 **          -w <work dir> AFD_MON working directory
 **          -v            Just print the version number.
 **          --version     Current version
 **
 ** DESCRIPTION
 **   This program controls the startup or shutdown procedure of
 **   the AFDMON. When starting the following process are being
 **   initiated in this order:
 **
 **          afd_mon         - Monitors all process of the AFD.
 **          mon_log         - Logs all system activities.
 **          
 **
 ** RETURN VALUES
 **   None when successful. Otherwise if no response is received
 **   from the AFD_MON, it will tell the user.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.08.1998 H.Kiehl Created
 **   27.02.2005 H.Kiehl Added options to initialize AFD_MON.
 **   07.04.2006 H.Kiehl When doing initialization only delete AFD_MON
 **                      files, not AFD files.
 **
 */
DESCR__E_M1

#include <stdio.h>            /* fprintf(), remove(), NULL               */
#include <string.h>           /* strcpy(), strcat(), strerror()          */
#include <stdlib.h>           /* exit()                                  */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>         /* struct timeval, FD_SET...               */
#include <signal.h>           /* kill()                                  */
#ifdef HAVE_FCNTL_H
#include <fcntl.h>            /* O_RDWR, O_CREAT, O_WRONLY, etc          */
#endif
#include <unistd.h>           /* execlp(), chdir(), R_OK, X_OK           */
#include <errno.h>
#include "mondefs.h"
#include "logdefs.h"
#include "version.h"
#include "permission.h"

/* Some local definitions. */
#define AFD_MON_ONLY             1
#define AFD_MON_CHECK_ONLY       2
#define AFD_MON_CHECK            3
#define MON_CTRL_ONLY            4
#define SHUTDOWN_ONLY            5
#define SILENT_SHUTDOWN_ONLY     6
#define START_BOTH               7
#define MAKE_BLOCK_FILE          8
#define REMOVE_BLOCK_FILE        9
#define AFD_MON_INITIALIZE      10
#define AFD_MON_FULL_INITIALIZE 11

/* External global variables. */
int         sys_log_fd = STDERR_FILENO;
char        *p_work_dir,
            mon_active_file[MAX_PATH_LENGTH],
            mon_cmd_fifo[MAX_PATH_LENGTH],
            probe_only_fifo[MAX_PATH_LENGTH];
const char  *sys_log_name = MON_SYS_LOG_FIFO;

/* Local function prototypes. */
static void delete_fifodir_files(char *, int),
            delete_log_files(char *, int),
            usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int            fd,
                  initialize_perm,
                  mon_ctrl_perm,
                  n,
                  readfd,
                  ret,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                  writefd,
#endif
                  shutdown_perm,
                  start_up,
                  startup_perm,
                  stop_all = NO,
                  user_offset;
   char           buffer[2],
                  *perm_buffer,
                  auto_block_file[MAX_PATH_LENGTH],
                  exec_cmd[MAX_PATH_LENGTH],
                  fake_user[MAX_FULL_USER_ID_LENGTH],
                  profile[MAX_PROFILE_NAME_LENGTH + 1],
                  sys_log_fifo[MAX_PATH_LENGTH],
                  user[MAX_FULL_USER_ID_LENGTH],
                  work_dir[MAX_PATH_LENGTH];
   fd_set         rset;
   struct timeval timeout;
#ifdef HAVE_STATX
   struct statx   stat_buf;
#else
   struct stat    stat_buf;
#endif

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
      profile[0] = '\0';
      user_offset = 0;
   }
   else
   {
      (void)my_strncpy(user, profile, MAX_FULL_USER_ID_LENGTH);
      user_offset = strlen(profile);
   }
#ifdef WITH_SETUID_PROGS
   set_afd_euid(work_dir);
#endif
   check_fake_user(&argc, argv, MON_CONFIG_FILE, fake_user);
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
         if ((perm_buffer[0] == 'a') && (perm_buffer[1] == 'l') &&
             (perm_buffer[2] == 'l') &&
             ((perm_buffer[3] == '\0') || (perm_buffer[3] == ',') ||
              (perm_buffer[3] == ' ') || (perm_buffer[3] == '\t')))
         {
            mon_ctrl_perm   = YES;
            shutdown_perm   = YES;
            startup_perm    = YES;
            initialize_perm = YES;
         }
         else
         {
            if (lposi(perm_buffer, MON_CTRL_PERM,
                      MON_CTRL_PERM_LENGTH) == NULL)
            {
               mon_ctrl_perm = NO_PERMISSION;
            }
            else
            {
               mon_ctrl_perm = YES;
            }
            if (lposi(perm_buffer, MON_SHUTDOWN_PERM,
                      MON_SHUTDOWN_PERM_LENGTH) == NULL)
            {
               shutdown_perm = NO_PERMISSION;
            }
            else
            {
               shutdown_perm = YES;
            }
            if (lposi(perm_buffer, MON_STARTUP_PERM,
                      MON_STARTUP_PERM_LENGTH) == NULL)
            {
               startup_perm = NO_PERMISSION;
            }
            else
            {
               startup_perm = YES;
            }
            if (lposi(perm_buffer, INITIALIZE_PERM,
                      INITIALIZE_PERM_LENGTH) == NULL)
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

      case INCORRECT : /* Hmm. Something did go wrong. Since we want to */
                       /* be able to disable permission checking let    */
                       /* the user have all permissions.                */
         mon_ctrl_perm   = YES;
         shutdown_perm   = YES;
         startup_perm    = YES;
         initialize_perm = YES;
         break;

      default :
         (void)fprintf(stderr,
                       "Impossible!! Remove the programmer!\n");
         exit(INCORRECT);
   }

   if (argc <= 3)
   {
      if ((argc == 2) || (argc == 3))
      {
         if (my_strcmp(argv[1], "-a") == 0) /* Start AFD_MON. */
         {
            if (startup_perm != YES)
            {
               (void)fprintf(stderr,
                             "You do not have the permission to start the AFD_MON.\n");
               exit(INCORRECT);
            }
            start_up = AFD_MON_ONLY;
         }
         else if (my_strcmp(argv[1], "-b") == 0) /* Block auto restart. */
              {
                 start_up = MAKE_BLOCK_FILE;
              }
         else if (my_strcmp(argv[1], "-c") == 0) /* Only check if AFD_MON is active. */
              {
                 start_up = AFD_MON_CHECK_ONLY;
              }
         else if (my_strcmp(argv[1], "-C") == 0) /* Only check if AFD_MON is active. */
              {
                 if (startup_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  "You do not have the permission to start the AFD_MON.\n");
                    exit(INCORRECT);
                 }
                 start_up = AFD_MON_CHECK;
              }
         else if (my_strcmp(argv[1], "-d") == 0) /* Start mon_ctrl. */
              {
                 if (mon_ctrl_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  "You do not have the permission to start the MON control dialog.\n");
                    exit(INCORRECT);
                 }
                 start_up = MON_CTRL_ONLY;
              }
         else if (my_strcmp(argv[1], "-i") == 0) /* Initialize AFD_MON. */
              {
                 if (initialize_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  "You do not have the permission to initialize AFD_MON.\n");
                    exit(INCORRECT);
                 }
                 start_up = AFD_MON_INITIALIZE;
              }
         else if (my_strcmp(argv[1], "-I") == 0) /* Full Initialization AFD_MON. */
              {
                 if (initialize_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  "You do not have the permission to do a full initialization of AFD_MON.\n");
                    exit(INCORRECT);
                 }
                 start_up = AFD_MON_FULL_INITIALIZE;
              }
         else if (my_strcmp(argv[1], "-s") == 0) /* Shutdown AFD_MON. */
              {
                 if (shutdown_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  "You do not have the permission to shutdown the AFD_MON.\n");
                    exit(INCORRECT);
                 }
                 if ((argc == 3) &&
                     (my_strcmp(argv[2], "--all") == 0))
                 {
                    stop_all = YES;
                 }
                 start_up = SHUTDOWN_ONLY;
              }
         else if (my_strcmp(argv[1], "-S") == 0) /* Silent AFD_MON shutdown. */
              {
                 if (shutdown_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  "You do not have the permission to shutdown the AFD_MON.\n");
                    exit(INCORRECT);
                 }
                 if ((argc == 3) &&
                     (my_strcmp(argv[2], "--all") == 0))
                 {
                    stop_all = YES;
                 }
                 start_up = SILENT_SHUTDOWN_ONLY;
              }
         else if (my_strcmp(argv[1], "-r") == 0) /* Removes blocking file. */
              {
                 start_up = REMOVE_BLOCK_FILE;
              }
         else if ((my_strcmp(argv[1], "-h") == 0) ||
                  (my_strcmp(argv[1], "-?") == 0) ||
                  (my_strcmp(argv[1], "--help") == 0)) /* Show usage. */
              {
                 usage(argv[0]);
                 exit(0);
              }
              else
              {
                 usage(argv[0]);
                 exit(1);
              }
      }
      else /* Start AFD_MON and mon_ctrl. */
      {
         if ((startup_perm == YES) && (mon_ctrl_perm == YES))
         {
            start_up = START_BOTH;
         }
         else if (startup_perm == YES)
              {
                 start_up = AFD_MON_ONLY;
              }
         else if (mon_ctrl_perm == YES)
              {
                 start_up = MON_CTRL_ONLY;
              }
              else
              {
                 (void)fprintf(stderr,
                               "You do not have enough permissions to use this program.\n");
                 exit(INCORRECT);
              }
      }
   }
   else
   {
      usage(argv[0]);
      exit(1);
   }

   if (chdir(work_dir) < 0)
   {
      (void)fprintf(stderr,
                    "ERROR   : Failed to change directory to %s : %s (%s %d)\n",
                    work_dir, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Initialise variables. */
   (void)strcpy(auto_block_file, work_dir);
   (void)strcat(auto_block_file, ETC_DIR);
   (void)strcat(auto_block_file, AFDMON_BLOCK_FILE);
   (void)strcpy(mon_active_file, work_dir);
   (void)strcat(mon_active_file, FIFO_DIR);
   if (check_dir(mon_active_file, R_OK | X_OK) < 0)
   {
      exit(INCORRECT);
   }
   (void)strcpy(sys_log_fifo, mon_active_file);
   (void)strcat(sys_log_fifo, MON_SYS_LOG_FIFO);
   (void)strcpy(mon_cmd_fifo, mon_active_file);
   (void)strcat(mon_cmd_fifo, MON_CMD_FIFO);
   (void)strcpy(probe_only_fifo, mon_active_file);
   (void)strcat(probe_only_fifo, MON_PROBE_ONLY_FIFO);
   (void)strcat(mon_active_file, MON_ACTIVE_FILE);

#ifdef HAVE_STATX
   if ((statx(0, sys_log_fifo, AT_STATX_SYNC_AS_STAT,
              STATX_MODE, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.stx_mode)))
#else
   if ((stat(sys_log_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
#endif
   {
      if (make_fifo(sys_log_fifo) < 0)
      {
         (void)fprintf(stderr, "ERROR   : Could not create fifo %s. (%s %d)\n",
                       sys_log_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   if ((start_up == SHUTDOWN_ONLY) || (start_up == SILENT_SHUTDOWN_ONLY))
   {
      int   n;
      pid_t ia_pid;

      /* First get the pid of init_afd before we send the */
      /* shutdown command.                                */
      if ((readfd = coe_open(mon_active_file, O_RDONLY)) == -1)
      {
         if (errno != ENOENT)
         {
            (void)fprintf(stderr, "Failed to open %s : %s (%s %d)\n",
                          mon_active_file, strerror(errno),
                          __FILE__, __LINE__);
         }
         else
         {
            if (start_up == SHUTDOWN_ONLY)
            {
               (void)fprintf(stderr, "There is no AFD_MON active.\n");
            }
         }
         exit(AFD_MON_IS_NOT_ACTIVE);
      }
      if ((n = read(readfd, &ia_pid, sizeof(pid_t))) != sizeof(pid_t))
      {
         if (n == 0)
         {
            (void)fprintf(stderr,
                          "File %s is empty. Unable to determine if AFD_MON is active.\n",
                          mon_active_file);
         }
         else
         {
            (void)fprintf(stderr, "read() error : %s (%s %d)\n",
                          strerror(errno),  __FILE__, __LINE__);
         }
         exit(INCORRECT);
      }
      (void)close(readfd);

      if (start_up == SHUTDOWN_ONLY)
      {
         (void)fprintf(stdout, "Starting %s shutdown ", AFD_MON);
         fflush(stdout);

#ifdef WITH_SYSTEMD
         shutdown_mon(NO, user, stop_all);
#else
         shutdown_mon(NO, user);
#endif

         (void)fprintf(stdout, "\nDone!\n");
      }
      else
      {
#ifdef WITH_SYSTEMD
         shutdown_mon(YES, user, stop_all);
#else
         shutdown_mon(YES, user);
#endif
      }

      exit(0);
   }
   else if (start_up == MON_CTRL_ONLY)
        {
           (void)strcpy(exec_cmd, MON_CTRL);
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
                            "ERROR   : Failed to execute %s : %s (%s %d)\n",
                            exec_cmd, strerror(errno), __FILE__, __LINE__);
              exit(1);
           }
           exit(0);
        }
   else if (start_up == AFD_MON_ONLY)
        {
           /* Check if starting of AFD_MON is currently disabled. */
           if (eaccess(auto_block_file, F_OK) == 0)
           {
              (void)fprintf(stderr,
                            _("AFD_MON is currently disabled by system manager.\n"));
              exit(AFD_DISABLED_BY_SYSADM);
           }

           if (check_afdmon_database() == -1)
           {
              (void)fprintf(stderr,
                            "Cannot read AFD_MON_CONFIG file : %s\nUnable to start AFD_MON.\n",
                            strerror(errno));
              exit(INCORRECT);
           }

           if ((ret = check_mon(5L)) == ACKN)
           {
              (void)fprintf(stdout, "AFD_MON is active in %s\n", p_work_dir);
              exit(5);
           }
           else if (ret == ACKN_STOPPED)
                {
                   if (send_afdmon_start() != 1)
                   {
                      exit(1);
                   }
                }
                else
                {
                   (void)strcpy(exec_cmd, AFD_MON);
                   system_log(INFO_SIGN, NULL, 0,
                              "AFD_MON startup initiated by %s", user);
                   switch (fork())
                   {
                      case -1 :

                         /* Could not generate process. */
                         (void)fprintf(stderr,
                                       "Could not create a new process : %s (%s %d)\n",
                                       strerror(errno),  __FILE__, __LINE__);
                         break;

                      case  0 :

                         /* Child process. */
                         if (execlp(exec_cmd, exec_cmd, WORK_DIR_ID, work_dir,
                                    (char *) 0) == -1)
                         {
                            (void)fprintf(stderr,
                                          "ERROR   : Failed to execute %s : %s (%s %d)\n",
                                          exec_cmd, strerror(errno),
                                          __FILE__, __LINE__);
                            exit(1);
                         }
                         exit(0);

                      default :

                         /* Parent process. */
                         break;
                   }
                }

           exit(0);
        }
   else if ((start_up == AFD_MON_CHECK) || (start_up == AFD_MON_CHECK_ONLY))
        {
           /* Check if starting of AFD_MON is currently disabled. */
           if (eaccess(auto_block_file, F_OK) == 0)
           {
              (void)fprintf(stderr,
                            _("AFD_MON is currently disabled by system manager.\n"));
              exit(AFD_DISABLED_BY_SYSADM);
           }

           if ((ret = check_mon(18L)) == ACKN)
           {
              (void)fprintf(stdout, "AFD_MON is active in %s\n", p_work_dir);
              exit(5);
           }
           else if (ret == ACKN_STOPPED)
                {
                   if (send_afdmon_start() != 1)
                   {
                      exit(1);
                   }
                }
           else if (start_up == AFD_MON_CHECK)
                {
                   if (check_afdmon_database() == -1)
                   {
                      (void)fprintf(stderr,
                                    "Cannot read AFD_MON_CONFIG file : %s\nUnable to start AFD_MON.\n",
                                    strerror(errno));
                      exit(INCORRECT);
                   }

                   (void)strcpy(exec_cmd, AFD_MON);
                   system_log(INFO_SIGN, NULL, 0,
                              "Hmm. AFD_MON is NOT running! Startup initiated by %s",
                              user);
                   switch (fork())
                   {
                      case -1 : /* Could not generate process. */
                         (void)fprintf(stderr,
                                       "Could not create a new process : %s (%s %d)\n",
                                       strerror(errno),  __FILE__, __LINE__);
                         break;

                      case  0 : /* Child process. */
                         if (execlp(exec_cmd, exec_cmd, WORK_DIR_ID, work_dir,
                                    (char *) 0) == -1)
                         {
                            (void)fprintf(stderr,
                                          "ERROR   : Failed to execute %s : %s (%s %d)\n",
                                          exec_cmd, strerror(errno),
                                          __FILE__, __LINE__);
                            exit(1);
                         }
                         exit(0);

                      default : /* Parent process. */
                         break;
                   }
                }
                else
                {
                   (void)fprintf(stderr, "No AFD_MON active in %s\n",
                                 p_work_dir);
                }

           exit(0);
        }
   else if ((start_up == AFD_MON_INITIALIZE) ||
            (start_up == AFD_MON_FULL_INITIALIZE))
        {
           if (((ret = check_mon(18L)) == ACKN) || (ret == ACKN_STOPPED))
           {
              (void)fprintf(stderr,
                            "ERROR   : AFD_MON is still active, unable to initialize.\n");
              exit(INCORRECT);
           }
           else
           {
              int  offset;
              char dirs[MAX_PATH_LENGTH];

              offset = sprintf(dirs, "%s%s", p_work_dir, FIFO_DIR);
              delete_fifodir_files(dirs, offset);
              if (start_up == AFD_MON_FULL_INITIALIZE)
              {
                 offset = sprintf(dirs, "%s%s", p_work_dir, LOG_DIR);
                 delete_log_files(dirs, offset);
                 (void)sprintf(dirs, "%s%s", p_work_dir, RLOG_DIR);
                 (void)rec_rmdir(dirs);
              }
              exit(SUCCESS);
           }
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


   /* Create a lock, to ensure that AFD_MON does not get started twice. */
   if ((fd = lock_file(sys_log_fifo, ON)) == INCORRECT)
   {
      (void)fprintf(stderr, "Failed to create lock! (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }
   else if (fd == LOCK_IS_SET)
        {
           (void)fprintf(stderr, "Someone else is trying to start the AFD_MON!\n");
           exit(INCORRECT);
        }
   else if (fd == LOCKFILE_NOT_THERE)
        {
           (void)fprintf(stderr, "Lock file `%s' not there.\n", sys_log_fifo);
           exit(INCORRECT);
        }

   /* Is another AFD_MON active in this directory? */
   if ((ret = check_mon(10L)) == ACKN)
   {
      /* Unlock, so other users don't get blocked. */
      (void)close(fd);

      /* Another AFD_MON is active. Only start mon_ctrl. */
      (void)strcpy(exec_cmd, MON_CTRL);
      if (profile[0] == '\0')
      {
         ret = execlp(exec_cmd, exec_cmd, WORK_DIR_ID, work_dir, (char *) 0);
      }
      else
      {
         ret = execlp(exec_cmd, exec_cmd, WORK_DIR_ID, work_dir,
                      "-p", profile, (char *) 0);
      }
      if (ret == -1)
      {
         (void)fprintf(stderr,
                       "ERROR   : Failed to execute %s : %s (%s %d)\n",
                       exec_cmd, strerror(errno), __FILE__, __LINE__);
         exit(1);
      }
   }
   else /* Start both. */
   {
      int status;

      if (check_afdmon_database() == -1)
      {
         (void)fprintf(stderr,
                       "Cannot read AFD_MON_CONFIG file : %s\nUnable to start AFD_MON.\n",
                       strerror(errno));
         exit(INCORRECT);
      }

#ifdef HAVE_STATX
      if ((statx(0, probe_only_fifo, AT_STATX_SYNC_AS_STAT,
                 STATX_MODE, &stat_buf) == -1) ||
          (!S_ISFIFO(stat_buf.stx_mode)))
#else
      if ((stat(probe_only_fifo, &stat_buf) == -1) ||
          (!S_ISFIFO(stat_buf.st_mode)))
#endif
      {
         if (make_fifo(probe_only_fifo) < 0)
         {
            (void)fprintf(stderr,
                          "Could not create fifo %s. (%s %d)\n",
                          probe_only_fifo, __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(probe_only_fifo, &readfd, &writefd) == -1)
#else
      if ((readfd = coe_open(probe_only_fifo, O_RDWR)) == -1)
#endif
      {
         (void)fprintf(stderr, "Could not open fifo %s : %s (%s %d)\n",
                       probe_only_fifo, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }

      if (ret == ACKN_STOPPED)
      {
         if (send_afdmon_start() != 1)
         {
            (void)close(readfd);
#ifdef WITHOUT_FIFO_RW_SUPPORT
            (void)close(writefd);
#endif
            exit(1);
         }
      }
      else
      {
         /* Start AFD_MON. */
         (void)strcpy(exec_cmd, AFD_MON);
         system_log(INFO_SIGN, NULL, 0,
                    "AFD_MON automatic startup initiated by %s", user);
         switch (fork())
         {
            case -1 : /* Could not generate process. */
               (void)fprintf(stderr,
                             "Could not create a new process : %s (%s %d)\n",
                             strerror(errno),  __FILE__, __LINE__);
               break;

            case  0 : /* Child process. */
               if (execlp(exec_cmd, exec_cmd, WORK_DIR_ID, work_dir,
                          (char *) 0) < 0)
               {
                  (void)fprintf(stderr,
                                "ERROR   : Failed to execute %s : %s (%s %d)\n",
                                exec_cmd, strerror(errno), __FILE__, __LINE__);
                  exit(1);
               }
               exit(0);

            default : /* Parent process. */
               break;
         }
      }

      /* Now lets wait for the AFD_MON to have finished */
      /* creating MSA (Monitor Status Area).            */
      FD_ZERO(&rset);
      FD_SET(readfd, &rset);
      timeout.tv_usec = 0;
      timeout.tv_sec = 20;

      /* Wait for message x seconds and then continue. */
      status = select(readfd + 1, &rset, NULL, NULL, &timeout);

      if (status == 0)
      {
         /* No answer from the other AFD_MON. Lets assume it */
         /* not able to startup properly.                    */
         (void)fprintf(stderr, "%s does not reply. (%s %d)\n",
                       AFD_MON, __FILE__, __LINE__);
         exit(INCORRECT);
      }
      else if (FD_ISSET(readfd, &rset))
           {
              /* Ahhh! Now we can start mon_ctrl. */
              if ((n = read(readfd, buffer, 1)) > 0)
              {
                 if (buffer[0] == ACKN)
                 {
                    /* Unlock, so other users don't get blocked. */
                    (void)close(fd);

                    (void)close(readfd);
#ifdef WITHOUT_FIFO_RW_SUPPORT
                    (void)close(writefd);
#endif

                    (void)strcpy(exec_cmd, MON_CTRL);
                    if (profile[0] == '\0')
                    {
                       ret = execlp(exec_cmd, exec_cmd, WORK_DIR_ID,
                                    work_dir, (char *) 0);
                    }
                    else
                    {
                       ret = execlp(exec_cmd, exec_cmd, WORK_DIR_ID,
                                    work_dir, "-p", profile, (char *) 0);
                    }
                    if (ret == -1)
                    {
                       (void)fprintf(stderr,
                                     "ERROR   : Failed to execute %s : %s (%s %d)\n",
                                     exec_cmd, strerror(errno),
                                     __FILE__, __LINE__);
                       exit(1);
                    }
                 }
                 else
                 {
                    (void)fprintf(stderr,
                                  "Reading garbage from fifo %s. (%s %d)\n",
                                  probe_only_fifo,  __FILE__, __LINE__);
                    exit(INCORRECT);
                 }
              }
              else if (n < 0)
                   {
                      (void)fprintf(stderr, "read() error : %s (%s %d)\n",
                                    strerror(errno),  __FILE__, __LINE__);
                      exit(INCORRECT);
                   }
           }
           else if (status < 0)
                {
                   (void)fprintf(stderr,
                                 "Select error : %s (%s %d)\n",
                                 strerror(errno),  __FILE__, __LINE__);
                   exit(INCORRECT);
                }
                else
                {
                   (void)fprintf(stderr,
                                 "Unknown condition. Maybe you can tell what's going on here. (%s %d)\n",
                                 __FILE__, __LINE__);
                   exit(INCORRECT);
                }

      (void)close(readfd);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      (void)close(writefd);
#endif
   }

   exit(0);
}


/*+++++++++++++++++++++++++ delete_fifodir_files() ++++++++++++++++++++++*/
static void
delete_fifodir_files(char *fifodir, int offset)
{
   int  i,
        tmp_sys_log_fd;
   char *file_ptr,
        *filelist[] =
        {
           MON_ACTIVE_FILE,
           AFD_MON_STATUS_FILE,
           MSA_ID_FILE,
           MON_CMD_FIFO,
           MON_RESP_FIFO,
           MON_PROBE_ONLY_FIFO,
           MON_LOG_FIFO,
           MON_SYS_LOG_FIFO
        },
        *mfilelist[] =
        {
           MON_STATUS_FILE_ALL,
           RETRY_MON_FIFO_ALL,
           ADL_FILE_NAME_ALL,
           AHL_FILE_NAME_ALL,
           AJL_FILE_NAME_ALL,
           OLD_ADL_FILE_NAME_ALL,
           OLD_AJL_FILE_NAME_ALL,
           TMP_AJL_FILE_NAME_ALL
        };

   file_ptr = fifodir + offset;

   /* Delete single files. */
   for (i = 0; i < (sizeof(filelist) / sizeof(*filelist)); i++)
   {
      (void)strcpy(file_ptr, filelist[i]);
      (void)unlink(fifodir);
   }
   *file_ptr = '\0';

   tmp_sys_log_fd = sys_log_fd;
   sys_log_fd = STDOUT_FILENO;

   /* Delete multiple files. */
   for (i = 0; i < (sizeof(mfilelist) / sizeof(*mfilelist)); i++)
   {
      (void)remove_files(fifodir, &mfilelist[i][1]);
   }

   sys_log_fd = tmp_sys_log_fd;

   return;
}


/*++++++++++++++++++++++++++++ delete_log_files() +++++++++++++++++++++++*/
static void
delete_log_files(char *logdir, int offset)
{
   int  i,
        tmp_sys_log_fd;
   char *log_ptr,
        *loglist[] =
        {
           "/DAEMON_LOG.afd_mon"
        },
        *mloglist[] =
        {
           MON_SYS_LOG_NAME_ALL,
           MON_LOG_NAME_ALL
        };

   log_ptr = logdir + offset;

   /* Delete single files. */
   for (i = 0; i < (sizeof(loglist) / sizeof(*loglist)); i++)
   {
      (void)strcpy(log_ptr, loglist[i]);
      (void)unlink(logdir);
   }
   *log_ptr = '\0';

   tmp_sys_log_fd = sys_log_fd;
   sys_log_fd = STDOUT_FILENO;

   /* Delete multiple files. */
   for (i = 0; i < (sizeof(mloglist)/sizeof(*mloglist)); i++)
   {
      (void)remove_files(logdir, mloglist[i]);
   }

   sys_log_fd = tmp_sys_log_fd;

   return;
}


/*+++++++++++++++++++++++++++++++ usage() +++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{
   (void)fprintf(stderr, "Usage: %s[ -w <AFD_MON working dir>][ -p <role>][ -u[ <user>]] [option]\n", progname);
   (void)fprintf(stderr, "              -a          only start AFD_MON\n");
#ifdef WITH_SYSTEMD
   (void)fprintf(stderr, "              --all       in combination with -s or -S, stop all process\n");
#endif
   (void)fprintf(stderr, "              -b          blocks starting of AFD_MON\n");
   (void)fprintf(stderr, "              -c          only check if AFD_MON is active\n");
   (void)fprintf(stderr, "              -C          check if AFD_MON is active, if not start it\n");
   (void)fprintf(stderr, "              -d          only start mon_ctrl dialog\n");
   (void)fprintf(stderr, "              -i          initialize AFD_MON, by deleting fifodir\n");
   (void)fprintf(stderr, "              -I          initialize AFD_MON, by deleting everything\n");
   (void)fprintf(stderr, "              -s          shutdown AFD_MON\n");
   (void)fprintf(stderr, "              -S          silent AFD_MON shutdown\n");
   (void)fprintf(stderr, "              -r          removes blocking startup of AFD_MON\n");
   (void)fprintf(stderr, "              --help      Prints out this syntax\n");
   (void)fprintf(stderr, "              -v          Just print version number\n");
   (void)fprintf(stderr, "              --version   Show current version\n");

   return;
}
