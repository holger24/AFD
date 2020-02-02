/*
 *  error_action.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 - 2020 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   error_action - call a script and execute it if it exists
 **
 ** SYNOPSIS
 **   void error_action(char *alias_name, char *action, int type)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None. When it fails it will exit with INCORRECT.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.02.2005 H.Kiehl Created
 **   23.11.2007 H.Kiehl Added warn and error action for directories.
 **   06.01.2008 H.Kiehl Added warn action for host and a default action.
 **   04.05.2009 H.Kiehl Added success actions.
 **   24.06.2013 H.Kiehl Added info actions.
 **   22.03.2017 H.Kiehl Handle the case when alias_name is empty.
 **   31.01.2020 H.Kiehl Added optional log_fd parameter.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* strerror()                      */
#include <stdlib.h>                   /* malloc(), free()                */
#include <sys/types.h>
#include <sys/wait.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <unistd.h>
#include <errno.h>
#include "version.h"

/* External global variables. */
extern char *p_work_dir;


/*########################### error_action() ############################*/
void
error_action(char *alias_name, char *action, int type, int log_fd)
{
   if (alias_name[0] == '\0')
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 _("No alias_name set. [action=`%s' type=%d]"), action, type);
   }
   else
   {
      int  alias_name_length,
           default_action,
           event_action,
           event_class,
           status;
      char fullname[MAX_PATH_LENGTH],
           *p_alias_name;

      if (type == HOST_ERROR_ACTION)
      {
         p_alias_name = fullname + snprintf(fullname, MAX_PATH_LENGTH,
                                            "%s%s%s%s%s/",
                                            p_work_dir, ETC_DIR, ACTION_DIR,
                                            ACTION_TARGET_DIR,
                                            ACTION_ERROR_DIR);
         event_class = EC_HOST;
         alias_name_length = MAX_HOSTNAME_LENGTH;
         if ((action[0] == 's') && (action[1] == 't') && (action[2] == 'a') &&
             (action[3] == 'r') && (action[4] == 't') && (action[5] == '\0'))
         {
            event_action = EA_EXEC_ERROR_ACTION_START;
         }
         else if ((action[0] == 's') && (action[1] == 't') &&
                  (action[2] == 'o') && (action[3] == 'p') &&
                  (action[4] == '\0'))
              {
                 event_action = EA_EXEC_ERROR_ACTION_STOP;
              }
              else
              {
                 event_action = 0;
              }
      }
      else if (type == DIR_ERROR_ACTION)
           {
              p_alias_name = fullname + snprintf(fullname, MAX_PATH_LENGTH,
                                                 "%s%s%s%s%s/",
                                                 p_work_dir, ETC_DIR,
                                                 ACTION_DIR, ACTION_SOURCE_DIR,
                                                 ACTION_ERROR_DIR);
              event_class = EC_DIR;
              alias_name_length = MAX_DIR_ALIAS_LENGTH;
              if ((action[0] == 's') && (action[1] == 't') &&
                  (action[2] == 'a') && (action[3] == 'r') &&
                  (action[4] == 't') && (action[5] == '\0'))
              {
                 event_action = EA_EXEC_ERROR_ACTION_START;
              }
              else if ((action[0] == 's') && (action[1] == 't') &&
                       (action[2] == 'o') && (action[3] == 'p') &&
                       (action[4] == '\0'))
                   {
                      event_action = EA_EXEC_ERROR_ACTION_STOP;
                   }
                   else
                   {
                      event_action = 0;
                   }
           }
      else if (type == HOST_WARN_ACTION)
           {
              p_alias_name = fullname + snprintf(fullname, MAX_PATH_LENGTH,
                                                 "%s%s%s%s%s/",
                                                 p_work_dir, ETC_DIR,
                                                 ACTION_DIR, ACTION_TARGET_DIR,
                                                 ACTION_WARN_DIR);
              event_class = EC_HOST;
              alias_name_length = MAX_HOSTNAME_LENGTH;
              if ((action[0] == 's') && (action[1] == 't') &&
                  (action[2] == 'a') && (action[3] == 'r') &&
                  (action[4] == 't') && (action[5] == '\0'))
              {
                 event_action = EA_EXEC_WARN_ACTION_START;
              }
              else if ((action[0] == 's') && (action[1] == 't') &&
                       (action[2] == 'o') && (action[3] == 'p') &&
                       (action[4] == '\0'))
                   {
                      event_action = EA_EXEC_WARN_ACTION_STOP;
                   }
                   else
                   {
                      event_action = 0;
                   }
           }
      else if (type == DIR_INFO_ACTION)
           {
              p_alias_name = fullname + snprintf(fullname, MAX_PATH_LENGTH,
                                                 "%s%s%s%s%s/",
                                                 p_work_dir, ETC_DIR,
                                                 ACTION_DIR, ACTION_SOURCE_DIR,
                                                 ACTION_INFO_DIR);
              event_class = EC_DIR;
              alias_name_length = MAX_DIR_ALIAS_LENGTH;
              if ((action[0] == 's') && (action[1] == 't') &&
                  (action[2] == 'a') && (action[3] == 'r') &&
                  (action[4] == 't') && (action[5] == '\0'))
              {
                 event_action = EA_EXEC_INFO_ACTION_START;
              }
              else if ((action[0] == 's') && (action[1] == 't') &&
                       (action[2] == 'o') && (action[3] == 'p') &&
                       (action[4] == '\0'))
                   {
                      event_action = EA_EXEC_INFO_ACTION_STOP;
                   }
                   else
                   {
                      event_action = 0;
                   }
           }
      else if (type == DIR_WARN_ACTION)
           {
              p_alias_name = fullname + snprintf(fullname, MAX_PATH_LENGTH,
                                                 "%s%s%s%s%s/",
                                                 p_work_dir, ETC_DIR,
                                                 ACTION_DIR, ACTION_SOURCE_DIR,
                                                 ACTION_WARN_DIR);
              event_class = EC_DIR;
              alias_name_length = MAX_DIR_ALIAS_LENGTH;
              if ((action[0] == 's') && (action[1] == 't') &&
                  (action[2] == 'a') && (action[3] == 'r') &&
                  (action[4] == 't') && (action[5] == '\0'))
              {
                 event_action = EA_EXEC_WARN_ACTION_START;
              }
              else if ((action[0] == 's') && (action[1] == 't') &&
                       (action[2] == 'o') && (action[3] == 'p') &&
                       (action[4] == '\0'))
                   {
                      event_action = EA_EXEC_WARN_ACTION_STOP;
                   }
                   else
                   {
                      event_action = 0;
                   }
           }
      else if (type == HOST_SUCCESS_ACTION)
           {
              p_alias_name = fullname + snprintf(fullname, MAX_PATH_LENGTH,
                                                 "%s%s%s%s%s/",
                                                 p_work_dir, ETC_DIR,
                                                 ACTION_DIR, ACTION_TARGET_DIR,
                                                 ACTION_SUCCESS_DIR);
              event_class = EC_HOST;
              alias_name_length = MAX_HOSTNAME_LENGTH;
              if ((action[0] == 's') && (action[1] == 't') &&
                  (action[2] == 'a') && (action[3] == 'r') &&
                  (action[4] == 't') && (action[5] == '\0'))
              {
                 event_action = EA_EXEC_SUCCESS_ACTION_START;
              }
              else if ((action[0] == 's') && (action[1] == 't') &&
                       (action[2] == 'o') && (action[3] == 'p') &&
                       (action[4] == '\0'))
                   {
                      event_action = EA_EXEC_SUCCESS_ACTION_STOP;
                   }
                   else
                   {
                      event_action = 0;
                   }
           }
      else if (type == DIR_SUCCESS_ACTION)
           {
              p_alias_name = fullname + snprintf(fullname, MAX_PATH_LENGTH,
                                                 "%s%s%s%s%s/",
                                                 p_work_dir, ETC_DIR,
                                                 ACTION_DIR, ACTION_SOURCE_DIR,
                                                 ACTION_SUCCESS_DIR);
              event_class = EC_DIR;
              alias_name_length = MAX_DIR_ALIAS_LENGTH;
              if ((action[0] == 's') && (action[1] == 't') &&
                  (action[2] == 'a') && (action[3] == 'r') &&
                  (action[4] == 't') && (action[5] == '\0'))
              {
                 event_action = EA_EXEC_SUCCESS_ACTION_START;
              }
              else if ((action[0] == 's') && (action[1] == 't') &&
                       (action[2] == 'o') && (action[3] == 'p') &&
                       (action[4] == '\0'))
                   {
                      event_action = EA_EXEC_SUCCESS_ACTION_STOP;
                   }
                   else
                   {
                      event_action = 0;
                   }
           }
           else
           {
              system_log(WARN_SIGN, __FILE__, __LINE__,
                         _("Unknown action type %d, please contact maintainer %s."),
                         type, AFD_MAINTAINER);
              return;
           }
      (void)strcpy(p_alias_name, alias_name);

      if ((status = eaccess(fullname, (R_OK | X_OK))) != 0)
      {
         default_action = YES;
         (void)strcpy(p_alias_name, DEFAULT_ACTION_FILE);
         status = eaccess(fullname, (R_OK | X_OK));
      }
      else
      {
         default_action = NO;
      }

      if (status == 0)
      {
         pid_t pid;
         char  reason_str[38 + MAX_INT_LENGTH + 1];

         if (log_fd > -1)
         {
            int  i = 0;
            char *tr_alias_name;

            if ((tr_alias_name = malloc(alias_name_length + 1)) == NULL)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to malloc() %d bytes : %s",
                          alias_name_length + 1, strerror(errno));
               return;
            }
            while ((alias_name[i] != '\0') && (i < alias_name_length))
            {
               tr_alias_name[i] = alias_name[i];
               i++;
            }
            tr_alias_name[i] = '\0';
            (void)rec(log_fd, DEBUG_SIGN,
                      "%-*s[X]: Calling action: %s %s\n",
                      alias_name_length, tr_alias_name, fullname, action);
            free(tr_alias_name);
         }

         if ((pid = fork()) < 0)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("Could not create a new process : %s"),
                       strerror(errno));
            return;
         }
         else if (pid == 0) /* Child process. */
              {
                 int ret;

                 if ((pid = fork()) < 0)
                 {
                    system_log(WARN_SIGN, __FILE__, __LINE__,
                               _("Could not create a new process : %s"),
                               strerror(errno));
                    _exit(INCORRECT);
                 }
                 else if (pid > 0)
                      {
                         _exit(SUCCESS);
                      }

                 if (default_action == NO)
                 {
                    ret = execlp(fullname, fullname, action, (char *)0);
                 }
                 else
                 {
                    ret = execlp(fullname, fullname, action, alias_name,
                                 (char *)0);
                 }
                 if (ret < 0)
                 {
                    system_log(WARN_SIGN, __FILE__, __LINE__,
                               _("Failed to start process %s (%d) : %s [type=%d alias_name=`%s' action=`%s']"),
                               fullname, ret, strerror(errno), type, alias_name,
                               action);
                    _exit(INCORRECT);
                 }
                 _exit(SUCCESS);
              }

         if (waitpid(pid, &status, 0) != pid)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                       "Failed to wait for pid %ld : %s",
#else
                       "Failed to wait for pid %lld : %s",
#endif
                       (pri_pid_t)pid, strerror(errno));
         }
         if (WIFEXITED(status))
         {
            (void)snprintf(reason_str, 38 + MAX_INT_LENGTH + 1, "%d",
                           WEXITSTATUS(status));
         }
         else if (WIFSIGNALED(status))
              {
                 (void)snprintf(reason_str, 38 + MAX_INT_LENGTH + 1,
                                _("Abnormal termination caused by signal %d"),
                                WTERMSIG(status));
              }
              else
              {
                 (void)my_strncpy(reason_str,
                                  _("Unable to determine return code"),
                                  38 + MAX_INT_LENGTH + 1);
              }

         if (event_action)
         {
            event_log(0L, event_class, ET_AUTO, event_action,
                      "%s%c%s", alias_name, SEPARATOR_CHAR, reason_str);
         }
      }
      else
      {
         if (log_fd > -1)
         {
            int  i = 0;
            char *tr_alias_name;

            if ((tr_alias_name = malloc(alias_name_length + 1)) == NULL)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to malloc() %d bytes : %s",
                          alias_name_length + 1, strerror(errno));
               return;
            }
            while ((alias_name[i] != '\0') && (i < alias_name_length))
            {
               tr_alias_name[i] = alias_name[i];
               i++;
            }
            tr_alias_name[i] = '\0';
            (void)rec(log_fd, DEBUG_SIGN,
                      "%-*s[X]: No action script to %s.\n",
                      alias_name_length, tr_alias_name, action);
            free(tr_alias_name);
         }
      }
   }

   return;
}
