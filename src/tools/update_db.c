/*
 *  update_db.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2015 Deutscher Wetterdienst (DWD),
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
 **   uhc - update HOST_CONFIG
 **   udc - update DIR_CONFIG
 **
 ** SYNOPSIS
 **   uhc [-w working directory][ -p <profile>][ -u[ <fake user>]]
 **   udc [-w working directory][ -p <profile>][ -u[ <fake user>]]
 **
 ** DESCRIPTION
 **   Program to update the DIR_CONFIG or HOST_CONFIG after they have
 **   been changed. Only possible if AMG is up running.
 **
 ** RETURN VALUES
 **   Returns 0 when update was performed without warnings or errors.
 **   2 is is returned when update was done but there are warnings
 **   and 3 or -1 is returned when the update could not be done.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.08.1998 H.Kiehl Created
 **   01.05.2007 H.Kiehl Wait and return responce when we exit.
 **   26.06.2007 H.Kiehl Do permission check and added parameters
 **                      -u and -p.
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr            */
#include <string.h>                      /* strcmp(), strerror()         */
#include <stdlib.h>                      /* exit(), atexit()             */
#include <sys/types.h>
#include <sys/stat.h>                    /* umask()                      */
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>                      /* getpid()                     */
#include <errno.h>
#include "logdefs.h"
#include "permission.h"
#include "version.h"

/* Global variables. */
int               event_log_fd = STDERR_FILENO,
                  sys_log_fd = STDERR_FILENO;
char              db_update_reply_fifo[MAX_PATH_LENGTH],
                  *p_work_dir;
struct afd_status *p_afd_status;
const char        *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void       update_db_exit(void),
                  usage(char *),
                  sig_exit(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ update_db() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int    db_update_fd,
          db_update_reply_fd,
#ifdef WITHOUT_FIFO_RW_SUPPORT
          db_update_readfd,
          db_update_reply_writefd,
#endif
          read_reply_length,
          ret,
          status,
          udc,
          user_offset;
   pid_t  my_pid;
   fd_set rset;
   char   buffer[1 + SIZEOF_PID_T],
          db_update_fifo[MAX_PATH_LENGTH],
          fake_user[MAX_FULL_USER_ID_LENGTH],
          *perm_buffer,
          profile[MAX_PROFILE_NAME_LENGTH + 1],
          user[MAX_FULL_USER_ID_LENGTH],
          work_dir[MAX_PATH_LENGTH];

   CHECK_FOR_VERSION(argc, argv);

   if ((get_arg(&argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      usage(argv[0]);
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
   if (strcmp((argv[0] + strlen(argv[0]) - 3), "udc") == 0)
   {
      udc = YES;
   }
   else
   {
      udc = NO;
   }

   check_fake_user(&argc, argv, AFD_CONFIG_FILE, fake_user);

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
         (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
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
               if (udc == YES)
               {
                  if (lposi(perm_buffer, RR_DC_PERM, RR_DC_PERM_LENGTH) != NULL)
                  {
                     permission = YES;
                  }
                  else
                  {
                     permission = NO;
                  }
               }
               else
               {
                  if (lposi(perm_buffer, RR_HC_PERM, RR_HC_PERM_LENGTH) != NULL)
                  {
                     permission = YES;
                  }
                  else
                  {
                     permission = NO;
                  }
               }
            }
            free(perm_buffer);
            if (permission != YES)
            {
               (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
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

   /* Attach to the AFD Status Area. */
   if (attach_afd_status(NULL, WAIT_AFD_STATUS_ATTACH) < 0)
   {
      (void)fprintf(stderr,
                    "ERROR   : Failed to map to AFD status area. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (p_afd_status->amg != 1)
   {
      (void)fprintf(stderr,
                    "Database can only be updated if AMG is running.\n");
      exit(INCORRECT);
   }

   (void)sprintf(db_update_fifo, "%s%s%s",
                 p_work_dir, FIFO_DIR, DB_UPDATE_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(db_update_fifo, &db_update_readfd, &db_update_fd) == -1)
#else
   if ((db_update_fd = open(db_update_fifo, O_RDWR)) == -1)
#endif
   {
      (void)fprintf(stderr, "ERROR   : Could not open fifo %s : %s (%s %d)",
                    db_update_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Setup some exit handler that removes the reply fifo */
   /* case we get terminated.                             */
   if (atexit(update_db_exit) != 0)
   {
      (void)fprintf(stderr, "Could not register exit handler : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_exit) == SIG_ERR) ||
       (signal(SIGBUS, sig_exit) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      (void)fprintf(stderr, "Could not set signal handler : %s\n",
                    strerror(errno));
      exit(INCORRECT);
   }
   my_pid = getpid();
#if SIZEOF_PID_T == 4
   (void)sprintf(db_update_reply_fifo, "%s%s%s%d",
#else
   (void)sprintf(db_update_reply_fifo, "%s%s%s%lld",
#endif
                 p_work_dir, FIFO_DIR, DB_UPDATE_REPLY_FIFO, (pri_pid_t)my_pid);
   umask(0);
   if ((mkfifo(db_update_reply_fifo,
               S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) == -1) &&
       (errno != EEXIST))
   {
      (void)fprintf(stderr,
                    "ERROR   : Could not create fifo `%s' : %s (%s %d)\n",
                    db_update_reply_fifo, strerror(errno),
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(db_update_reply_fifo, &db_update_reply_fd,
                    &db_update_reply_writefd) == -1)
#else
   if ((db_update_reply_fd = open(db_update_reply_fifo, O_RDWR)) == -1)
#endif
   {
      (void)fprintf(stderr, "ERROR   : Could not open fifo %s : %s (%s %d)",
                    db_update_reply_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   get_user(user, fake_user, user_offset);
   if (udc == YES)
   {
      buffer[0] = REREAD_DIR_CONFIG;
      (void)memcpy(&buffer[1], &my_pid, SIZEOF_PID_T);
      if (write(db_update_fd, buffer, (1 + SIZEOF_PID_T)) != (1 + SIZEOF_PID_T))
      {
         (void)fprintf(stderr,
                       "ERROR   : Unable to send reread DIR_CONFIG command to %s : %s (%s %d)\n",
                       AMG, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      system_log(CONFIG_SIGN, NULL, 0,
                 "Reread DIR_CONFIG initiated by %s [%s]", user, argv[0]);
      event_log(0L, EC_GLOB, ET_MAN, EA_REREAD_DIR_CONFIG,
                "initiated by %s [%s]", user, argv[0]);
      read_reply_length = MAX_UDC_RESPONCE_LENGTH;
   }
   else
   {
      buffer[0] = REREAD_HOST_CONFIG;
      (void)memcpy(&buffer[1], &my_pid, SIZEOF_PID_T);
      if (write(db_update_fd, buffer, (1 + SIZEOF_PID_T)) != (1 + SIZEOF_PID_T))
      {
         (void)fprintf(stderr,
                       "ERROR   : Unable to send reread HOST_CONFIG command to %s : %s (%s %d)\n",
                       AMG, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      system_log(CONFIG_SIGN, NULL, 0,
                 "Reread HOST_CONFIG initiated by %s [%s]", user, argv[0]);
      event_log(0L, EC_GLOB, ET_MAN, EA_REREAD_HOST_CONFIG,
                "initiated by %s [%s]", user, argv[0]);
      read_reply_length = MAX_UHC_RESPONCE_LENGTH;
   }

   /* Wait for the responce from AMG and get the result code. */
   FD_ZERO(&rset);
   FD_SET(db_update_reply_fd, &rset);
   status = select(db_update_reply_fd + 1, &rset, NULL, NULL, NULL);

   if ((status > 0) && (FD_ISSET(db_update_reply_fd, &rset)))
   {
      int  n;
      char rbuffer[MAX_UDC_RESPONCE_LENGTH];

      if ((n = read(db_update_reply_fd, rbuffer, read_reply_length)) >= MAX_UHC_RESPONCE_LENGTH)
      {
         int          hc_result,
                      see_sys_log;
         unsigned int hc_warn_counter;
         char         hc_result_str[MAX_UPDATE_REPLY_STR_LENGTH];

         see_sys_log = NO;
         (void)memcpy(&hc_result, rbuffer, SIZEOF_INT);
         (void)memcpy(&hc_warn_counter, &rbuffer[SIZEOF_INT], SIZEOF_INT);
         if (read_reply_length == MAX_UDC_RESPONCE_LENGTH)
         {
            if (n == MAX_UDC_RESPONCE_LENGTH)
            {
               int          dc_result;
               unsigned int dc_warn_counter;
               char         dc_result_str[MAX_UPDATE_REPLY_STR_LENGTH];

               n = 0;
               (void)memcpy(&dc_result, &rbuffer[SIZEOF_INT + SIZEOF_INT],
                            SIZEOF_INT);
               (void)memcpy(&dc_warn_counter,
                            &rbuffer[SIZEOF_INT + SIZEOF_INT + SIZEOF_INT],
                            SIZEOF_INT);
               if (hc_result != NO_CHANGE_IN_HOST_CONFIG)
               {
                  get_hc_result_str(hc_result_str, hc_result,
                                    hc_warn_counter, &see_sys_log, &n);
                  (void)fprintf(stdout, "%s\n", hc_result_str);
               }
               get_dc_result_str(dc_result_str, dc_result, dc_warn_counter,
                                 &see_sys_log, &ret);
               (void)fprintf(stdout, "%s\n", dc_result_str);
               if (n > ret)
               {
                  ret = n;
               }
               ret -= 1;
            }
            else
            {
               (void)fprintf(stderr,
                             "ERROR   : Unable to evaluate reply since it is to short (%d, should be %d).",
                             n, MAX_UDC_RESPONCE_LENGTH);
               ret = -1;
            }
         }
         else
         {
            get_hc_result_str(hc_result_str, hc_result, hc_warn_counter,
                              &see_sys_log, &ret);
            (void)fprintf(stdout, "%s\n", hc_result_str);
            ret -= 1;
         }
         if (see_sys_log == YES)
         {
            (void)fprintf(stdout,
                          "--> See %s0 for more details about warnings and/or errors. <--\n",
                          SYSTEM_LOG_NAME);
         }
      }
      else
      {
         (void)fprintf(stderr, "Failed to read() responce : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   else
   {
      (void)fprintf(stderr, "select() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

#ifdef WITHOUT_FIFO_RW_SUPPORT
   if ((close(db_update_readfd) == -1) ||
       (close(db_update_reply_writefd) == -1))
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }
#endif
   if ((close(db_update_fd) == -1) || (close(db_update_reply_fd) == -1))
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }

   exit(ret);
}


/*++++++++++++++++++++++++++ update_db_exit() +++++++++++++++++++++++++++*/
static void
update_db_exit(void)
{
   (void)unlink(db_update_reply_fifo);
   return;
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   exit(INCORRECT);
}


/*+++++++++++++++++++++++++++++++ usage() +++++++++++++++++++++++++++++++*/
static void                                                                
usage(char *progname)
{
   (void)fprintf(stderr,
                  "Usage : %s[ -w <AFD work dir>][ -p <role>][ -u[ <fake user>]]\n",
                  progname);
   (void)fprintf(stderr, "        The following values are returned on exit:\n");
   (void)fprintf(stderr, "                0 - Config file updated or no changes found.\n");
   (void)fprintf(stderr, "                2 - Config file updated with warnings.\n");
   (void)fprintf(stderr, "                3 - Config file (possibly) updated with errors.\n");
   (void)fprintf(stderr, "              255 - Internal errors, no update.\n");

   return;
}
