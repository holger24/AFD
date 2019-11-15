/*
 *  update_db.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2019 Deutscher Wetterdienst (DWD),
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
 **   uhc [<options>]
 **          -w <AFD work dir>
 **          -p <role>\
 **          -u[ <fake user>]
 **          -v   More verbose output.
 **   udc [<options>]
 **          -w <AFD work dir>
 **          -p <role>\
 **          -u[ <fake user>]
 **          -v   More verbose output.
 **          -j   Show job ID's of changed configs.
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
 **   20.04.2017 H.Kiehl Added parameter -j to list the job ID's of
 **                      changed configs.
 **   14.11.2019 H.Kiehl When calling get_dc_data we must pass $AFD_WORK_DIR.
 **                      Increase time we wait for a command reply.
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), fgets(), stderr   */
#include <string.h>                      /* strerror()                   */
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

#define DC_ID_STEP_SIZE 1


/* Global variables. */
int               cmd_length = 0,
                  cmd_pos = 0,
                  event_log_fd = STDERR_FILENO,
                  sys_log_fd = STDERR_FILENO;
char              db_update_reply_fifo[MAX_PATH_LENGTH],
                  *get_dc_data_cmd = NULL,
                  *p_work_dir;
struct afd_status *p_afd_status;
const char        *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void       update_db_exit(void),
                  usage(char *),
                  sig_exit(int),
                  show_debug_data(int, int, FILE **, char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ update_db() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int            db_update_fd,
                  db_update_reply_fd,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                  db_update_readfd,
                  db_update_reply_writefd,
#endif
                  verbose_level = 1,
                  read_reply_length,
                  ret,
                  show_job_ids = NO,
                  status,
                  udc,
                  user_offset;
   pid_t          my_pid;
   fd_set         rset;
   struct timeval timeout;
   char           buffer[1 + SIZEOF_PID_T],
                  db_update_fifo[MAX_PATH_LENGTH],
                  fake_user[MAX_FULL_USER_ID_LENGTH],
                  *perm_buffer,
                  profile[MAX_PROFILE_NAME_LENGTH + 1],
                  uc_reply_name[MAX_PATH_LENGTH],
                  user[MAX_FULL_USER_ID_LENGTH],
                  work_dir[MAX_PATH_LENGTH];
   FILE           *debug_fp = NULL;

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
   if (get_arg(&argc, argv, "-v", NULL, 0) == SUCCESS)
   {
      verbose_level = 2;
   }
   if (get_arg(&argc, argv, "-j", NULL, 0) == SUCCESS)
   {
      verbose_level = 2;
      show_job_ids = YES;
      cmd_length = GET_DC_DATA_LENGTH +
                   1 + 2 + 1 + strlen(work_dir) + /* -w work_dir */
                   1 + 2 + 1 + /* -C */ 
                   (DC_ID_STEP_SIZE * (MAX_INT_HEX_LENGTH + 1));
      if ((get_dc_data_cmd = malloc(cmd_length)) == NULL)
      {
         (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      cmd_pos = snprintf(get_dc_data_cmd, cmd_length, "%s -w %s -C ",
                         GET_DC_DATA, work_dir);
   }
/*   if (get_arg(&argc, argv, "-vv", NULL, 0) == SUCCESS)
   {
      verbose_level = 3;
   } */
#ifdef WITH_SETUID_PROGS
   set_afd_euid(work_dir);
#endif
   if (my_strcmp((argv[0] + strlen(argv[0]) - 3), "udc") == 0)
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

   (void)snprintf(db_update_fifo, MAX_PATH_LENGTH, "%s%s%s",
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
   (void)snprintf(db_update_reply_fifo, MAX_PATH_LENGTH, "%s%s%s%d",
#else
   (void)snprintf(db_update_reply_fifo, MAX_PATH_LENGTH, "%s%s%s%lld",
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
      if (verbose_level == 1)
      {
         buffer[0] = REREAD_DIR_CONFIG_VERBOSE1;
      }
      else if (verbose_level == 2)
           {
              buffer[0] = REREAD_DIR_CONFIG_VERBOSE2;
           }
           else
           {
              buffer[0] = REREAD_DIR_CONFIG;
           }
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
      if (verbose_level == 1)
      {
         buffer[0] = REREAD_HOST_CONFIG_VERBOSE1;
      }
      else if (verbose_level == 2)
           {
              buffer[0] = REREAD_HOST_CONFIG_VERBOSE2;
           }
           else
           {
              buffer[0] = REREAD_HOST_CONFIG;
           }
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

   if (verbose_level > 0)
   {
#if SIZEOF_PID_T == 4
      (void)snprintf(uc_reply_name, MAX_PATH_LENGTH, "%s%s%s.%d",
#else
      (void)snprintf(uc_reply_name, MAX_PATH_LENGTH, "%s%s%s.%lld",
#endif
                     p_work_dir, FIFO_DIR, DB_UPDATE_REPLY_DEBUG_FILE,
                     (pri_pid_t)my_pid);
   }
   else
   {
      uc_reply_name[0] = '\0';
   }

   /* Wait for the responce from AMG and get the result code. */
   FD_ZERO(&rset);
   for (;;)
   {
      FD_SET(db_update_reply_fd, &rset);
      timeout.tv_usec = 500000;
      timeout.tv_sec = 1;
      status = select(db_update_reply_fd + 1, &rset, NULL, NULL, &timeout);

      if ((status > 0) && (FD_ISSET(db_update_reply_fd, &rset)))
      {
         int  n;
         char rbuffer[MAX_UDC_RESPONCE_LENGTH];

         /* Check if we have any information we can show to the user. */
         show_debug_data(verbose_level, show_job_ids, &debug_fp, uc_reply_name);

         if ((n = read(db_update_reply_fd, rbuffer, read_reply_length)) >= MAX_UHC_RESPONCE_LENGTH)
         {
            int          hc_result;
            unsigned int hc_warn_counter;
            char         hc_result_str[MAX_UPDATE_REPLY_STR_LENGTH];

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
                                       hc_warn_counter, NULL, &n);
                     (void)fprintf(stdout, "%s\n", hc_result_str);
                  }
                  get_dc_result_str(dc_result_str, dc_result, dc_warn_counter,
                                    NULL, &ret);
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
                                 NULL, &ret);
               (void)fprintf(stdout, "%s\n", hc_result_str);
               ret -= 1;
            }
         }
         else
         {
            (void)fprintf(stderr, "Failed to read() responce : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         break;
      }
      else if (status == 0)
           {
              /* Check if we have any information we can show to the user. */
              show_debug_data(verbose_level, show_job_ids,
                              &debug_fp, uc_reply_name);
           }
           else
           {
              (void)fprintf(stderr, "select() error : %s (%s %d)\n",
                            strerror(errno), __FILE__, __LINE__);
              exit(INCORRECT);
           }
   }

   show_debug_data(verbose_level, show_job_ids, &debug_fp, uc_reply_name);
   if ((verbose_level > 0) && (uc_reply_name[0] != '\0'))
   {
      (void)unlink(uc_reply_name);
   }

   if ((show_job_ids == YES) && (get_dc_data_cmd != NULL))
   {
      char *text = NULL;

      get_dc_data_cmd[cmd_pos] = '\0';
      if ((exec_cmd(get_dc_data_cmd, &text, -1, NULL, 0,
#ifdef HAVE_SETPRIORITY
                    NO_PRIORITY,
#endif
                    "", NULL, NULL, 0, 0L, NO, NO) != 0) ||
          (text == NULL))
      {
         (void)fprintf(stderr, "Failed to execute command: %s\n",
                       get_dc_data_cmd);
      }
      else
      {
         (void)fprintf(stdout, "%s", text);
      }
      free(text);
      free(get_dc_data_cmd);
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


/*++++++++++++++++++++++++++ show_debug_data() ++++++++++++++++++++++++++*/
static void
show_debug_data(int  verbose_level,
                int  show_job_ids,
                FILE **debug_fp,
                char *uc_reply_name)
{
   if (verbose_level > 0)
   {
      if ((*debug_fp == NULL) && (uc_reply_name[0] != '\0'))
      {
         if ((*debug_fp = fopen(uc_reply_name, "r")) == NULL)
         {
            if (errno != ENOENT)
            {
               (void)fprintf(stderr,
                             "Failed to fopen() `%s' : %s (%s %d)\n",
                             uc_reply_name, strerror(errno),
                             __FILE__, __LINE__);
            }
         }
      }
      if (*debug_fp != NULL)
      {
         int  i;
         char line[MAX_LINE_LENGTH];

         while (fgets(line, MAX_LINE_LENGTH, *debug_fp) != NULL)
         {
            if (show_job_ids == YES)
            {
               if ((line[0] == '<') && (line[1] == 'D') && (line[2] == '>') &&
                   (line[3] == ' ') && (line[4] == '[') && (line[5] == '!'))
               {
                  i = 0;
                  while ((i < MAX_INT_HEX_LENGTH) && (line[6 + i] != ']') &&
                         (line[6 + i] != '\n') && (line[6 + i] != '\0'))
                  {
                     get_dc_data_cmd[cmd_pos + i] = line[6 + i];
                     i++;
                  }
                  if (line[6 + i] == ']')
                  {
                     get_dc_data_cmd[cmd_pos + i] = ' ';
                     cmd_pos += i + 1;
                     if ((cmd_length - cmd_pos) < (DC_ID_STEP_SIZE * (MAX_INT_HEX_LENGTH + 1)))
                     {
                        cmd_length += (DC_ID_STEP_SIZE * (MAX_INT_HEX_LENGTH + 1));
                        if ((get_dc_data_cmd = realloc(get_dc_data_cmd,
                                                       cmd_length)) == NULL)
                        {
                           (void)fprintf(stderr,
                                         "Failed to realloc() memory : %s (%s %d)\n",
                                         strerror(errno), __FILE__, __LINE__);
                           exit(INCORRECT);
                        }
                     }
                  }
               }
               else
               {
                  (void)fprintf(stdout, "%s", line);
               }
            }
            else
            {
               (void)fprintf(stdout, "%s", line);
            }
         }
         fflush(stdout);
      }
   }

   return;
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
   int length = strlen(progname);

   (void)fprintf(stderr, "Usage : %s [<options>]\n", progname);
   (void)fprintf(stderr, "        %*s   -w <AFD work dir>\n", length, "");
   (void)fprintf(stderr, "        %*s   -p <role>\n", length, "");
   (void)fprintf(stderr, "        %*s   -u[ <fake user>]\n", length, "");
   (void)fprintf(stderr, "        %*s   -v   More verbose output.\n",
                 length, "");
   (void)fprintf(stderr, "        %*s   -j   Show job ID's of changed configs.\n",
                 length, "");
   (void)fprintf(stderr, "        The following values are returned on exit:\n");
   (void)fprintf(stderr, "                0 - Config file updated or no changes found.\n");
   (void)fprintf(stderr, "                2 - Config file updated with warnings.\n");
   (void)fprintf(stderr, "                3 - Config file (possibly) updated with errors.\n");
   (void)fprintf(stderr, "              255 - Internal errors, no update.\n");

   return;
}
