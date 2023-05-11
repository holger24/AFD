/*
 *  aldad.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2009 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   aldad - AFD log data analyser daemon
 **
 ** SYNOPSIS
 **   aldad [options]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.01.2009 H.Kiehl Created
 **   22.04.2023 H.Kiehl Added option --afdmon to read configuration
 **                      from MON_CONFIG_FILE.
 **   09.05.2023 H.Kiehl If all entries are removed and realloc() returns
 **                      NULL, evaluate errno to check if this is the
 **                      case.
 **
 */
DESCR__E_M3


#include <stdio.h>
#include <stdlib.h>               /* realloc(), free(), atexit()         */
#include <time.h>                 /* time()                              */
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>               /* Definition of AT_* constants        */
#endif
#include <sys/stat.h>             /* stat()                              */
#include <sys/wait.h>             /* waitpid()                           */
#include <signal.h>               /* signal(), kill()                    */
#include <unistd.h>               /* fork()                              */
#include <errno.h>
#include "aldadefs.h"
#include "version.h"

/* Global variables. */
int                    no_of_process,
                       sys_log_fd = STDERR_FILENO;
char                   config_file[MAX_PATH_LENGTH + ETC_DIR_LENGTH + AFD_CONFIG_FILE_LENGTH + 1],
                       *p_work_dir;
struct aldad_proc_list *apl = NULL;
const char             *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
#ifdef WITH_AFD_MON
static pid_t           make_process(char *, int);
#else
static pid_t           make_process(char *);
#endif
static void            aldad_exit(void),
                       sig_segv(int),
                       sig_bus(int),
                       sig_exit(int),
                       zombie_check(void);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ aldad $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
#ifdef WITH_AFD_MON
   int    remote_log_data;
#endif
   time_t next_stat_time,
          now,
          old_st_mtime;
   char   work_dir[MAX_PATH_LENGTH];

   /* Evaluate input arguments. */
   CHECK_FOR_VERSION(argc, argv);
#ifdef WITH_AFD_MON
   if (get_arg(&argc, argv, "--afdmon", NULL, 0) == SUCCESS)
   {
      if (get_mon_path(&argc, argv, work_dir) < 0)
      {
         exit(INCORRECT);
      }
      (void)sprintf(config_file, "%s%s%s",
                    work_dir, ETC_DIR, MON_CONFIG_FILE);
      remote_log_data = YES;
   }
   else
   {
#endif
      if (get_afd_path(&argc, argv, work_dir) < 0)
      {
         exit(INCORRECT);
      }
      (void)sprintf(config_file, "%s%s%s",
                    work_dir, ETC_DIR, AFD_CONFIG_FILE);
#ifdef WITH_AFD_MON
      remote_log_data = NO;
   }
#endif

   /* Initialize variables. */
   p_work_dir = work_dir;
   next_stat_time = 0L;
   old_st_mtime = 0L;
   no_of_process = 0;

   /* Do some cleanups when we exit. */
   if (atexit(aldad_exit) != 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not register exit function : %s", strerror(errno));
      exit(INCORRECT);
   }
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("Could not set signal handler : %s"), strerror(errno));
      exit(INCORRECT);
   }
   system_log(INFO_SIGN, NULL, 0, "Started %s.", ALDAD);

   /* Lets determine what log files we need to search. */
   for (;;)
   {
      now = time(NULL);
      if (next_stat_time < now)
      {
#ifdef HAVE_STATX
         struct statx stat_buf;
#else
         struct stat stat_buf;
#endif

         next_stat_time = now + STAT_INTERVAL;
#ifdef HAVE_STATX
         if (statx(0, config_file, AT_STATX_SYNC_AS_STAT,
                   STATX_MTIME, &stat_buf) == 0)
#else
         if (stat(config_file, &stat_buf) == 0)
#endif
         {
#ifdef HAVE_STATX
            if (stat_buf.stx_mtime.tv_sec != old_st_mtime)
#else
            if (stat_buf.st_mtime != old_st_mtime)
#endif
            {
               int  i;
               char *buffer,
                    tmp_aldad[MAX_PATH_LENGTH + 1];

               for (i = 0; i < no_of_process; i++)
               {
                  apl[i].in_list = NO;
               }

#ifdef HAVE_STATX
               old_st_mtime = stat_buf.stx_mtime.tv_sec;
#else
               old_st_mtime = stat_buf.st_mtime;
#endif
               if ((eaccess(config_file, F_OK) == 0) &&
                   (read_file_no_cr(config_file, &buffer, YES,
                                    __FILE__, __LINE__) != INCORRECT))
               {
                  int  gotcha;
                  char *ptr = buffer;

                  system_log(DEBUG_SIGN, NULL, 0,
                             "ALDAD read %s", config_file);

                  /*
                   * Read all alda daemons entries.
                   */
                  do
                  {
                     if ((ptr = get_definition(ptr, ALDA_DAEMON_DEF, tmp_aldad,
                                               MAX_PATH_LENGTH)) != NULL)
                     {
                        /* First check if we already have this in our list. */
                        gotcha = NO;
                        for (i = 0; i < no_of_process; i++)
                        {
                           if (my_strcmp(apl[i].parameters, tmp_aldad) == 0)
                           {
                              apl[i].in_list = YES;
                              gotcha = YES;
                              break;
                           }
                        }
                        if (gotcha == NO)
                        {
                           no_of_process++;
                           if ((apl = realloc(apl,
                                              (no_of_process * sizeof(struct aldad_proc_list)))) == NULL)
                           {
                              system_log(FATAL_SIGN, __FILE__, __LINE__,
                                         "Failed to realloc() memory : %s",
                                         strerror(errno));
                              exit(INCORRECT);
                           }
                           if ((apl[no_of_process - 1].parameters = malloc((strlen(tmp_aldad) + 1))) == NULL)
                           {
                              system_log(FATAL_SIGN, __FILE__, __LINE__,
                                         "Failed to malloc() memory : %s",
                                         strerror(errno));
                              exit(INCORRECT);
                           }
                           (void)strcpy(apl[no_of_process - 1].parameters,
                                        tmp_aldad);
                           apl[no_of_process - 1].in_list = YES;
#ifdef WITH_AFD_MON
                           if ((apl[no_of_process - 1].pid = make_process(apl[no_of_process - 1].parameters, remote_log_data)) == 0)
#else
                           if ((apl[no_of_process - 1].pid = make_process(apl[no_of_process - 1].parameters)) == 0)
#endif
                           {
                              system_log(ERROR_SIGN, __FILE__, __LINE__,
                                         "Failed to start aldad process with the following parameters : %s",
                                         apl[no_of_process - 1].parameters);
                              free(apl[no_of_process - 1].parameters);
                              no_of_process--;
                              if ((apl = realloc(apl,
                                                 (no_of_process * sizeof(struct aldad_proc_list)))) == NULL)
                              {
                                 if (errno != 0)
                                 {
                                    system_log(FATAL_SIGN, __FILE__, __LINE__,
                                               "Failed to realloc() memory : %s",
                                               strerror(errno));
                                    exit(INCORRECT);
                                 }
                              }
                           }
                        }
                     }
                  } while (ptr != NULL);

                  for (i = 0; i < no_of_process; i++)
                  {
                     if (apl[i].in_list == NO)
                     {
                        if (kill(apl[i].pid, SIGINT) == -1)
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                                      "Failed to kill() process %d with parameters %s",
#else
                                      "Failed to kill() process %lld with parameters %s",
#endif
                                      (pri_pid_t)apl[i].pid, apl[i].parameters);
                        }
                        else
                        {
                           free(apl[i].parameters);
                           if (i < no_of_process)
                           {
                              (void)memmove(&apl[i], &apl[i + 1],
                                            ((no_of_process - (i + 1)) * sizeof(struct aldad_proc_list)));
                           }
                           no_of_process--;
                           i--;
                           if ((apl = realloc(apl,
                                              (no_of_process * sizeof(struct aldad_proc_list)))) == NULL)
                           {
                              if (errno != 0)
                              {
                                 system_log(FATAL_SIGN, __FILE__, __LINE__,
                                            "Failed to realloc() memory : %s",
                                            strerror(errno));
                                 exit(INCORRECT);
                              }
                           }
                        }
                     }
                  }

                  free(buffer);

                  if (no_of_process > 0)
                  {
                     system_log(INFO_SIGN, NULL, 0,
                                "ALDAD %d process running.", no_of_process);
                  }
                  else
                  {
                     system_log(DEBUG_SIGN, NULL, 0,
                                "ALDAD no definitions found.");
                  }
               }
            }
         }
      }
      zombie_check();

      sleep(5L);
   }

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++ make_process() +++++++++++++++++++++++++++*/
static pid_t
#ifdef WITH_AFD_MON
make_process(char *parameters, int remote_log_data)
#else
make_process(char *parameters)
#endif
{
   pid_t proc_id;

   switch (proc_id = fork())
   {
      case -1: /* Could not generate process. */
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not create a new process : %s", strerror(errno));
         exit(INCORRECT);

      case  0: /* Child process. */
         {
            int  length,
                 work_dir_length;
            char *cmd;

            work_dir_length = strlen(p_work_dir);
            length = 5 + 3 + work_dir_length + 1 + 6 + strlen(parameters) + 1;
            if ((cmd = malloc(length)) == NULL)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "Failed to malloc() %d bytes : %s",
                          length, strerror(errno));
               exit(INCORRECT);
            }
            cmd[0] = 'a';
            cmd[1] = 'l';
            cmd[2] = 'd';
            cmd[3] = 'a';
            cmd[4] = ' ';
            cmd[5] = '-';
            cmd[6] = 'w';
            cmd[7] = ' ';
            (void)strcpy(&cmd[8], p_work_dir);
            cmd[8 + work_dir_length] = ' ';
            cmd[9 + work_dir_length] = '-';
            cmd[10 + work_dir_length] = 'C';
            cmd[11 + work_dir_length] = ' ';
#ifdef WITH_AFD_MON
            if (remote_log_data == YES)
            {
               cmd[12 + work_dir_length] = '-';
               cmd[13 + work_dir_length] = 'r';
               cmd[14 + work_dir_length] = ' ';
            }
            else
            {
#endif
               cmd[12 + work_dir_length] = '-';
               cmd[13 + work_dir_length] = 'l';
               cmd[14 + work_dir_length] = ' ';
#ifdef WITH_AFD_MON
            }
#endif
            (void)strcpy(&cmd[15 + work_dir_length], parameters);
            system_log(DEBUG_SIGN, NULL, 0, "aldad: %s", cmd);
            (void)execl("/bin/sh", "sh", "-c", cmd, (char *)0);
         }
         exit(SUCCESS);

      default: /* Parent process. */
         return(proc_id);
   }

   return(INCORRECT);
}


/*++++++++++++++++++++++++++++ zombie_check() +++++++++++++++++++++++++++*/
static void
zombie_check(void)
{
   int exit_status,
       i,
       status;

   for (i = 0; i < no_of_process; i++)
   {
      if (waitpid(apl[i].pid, &status, WNOHANG) > 0)
      {
         if (WIFEXITED(status))
         {
            exit_status = WEXITSTATUS(status);
            if (exit_status != 0)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Alda log process (%s) died, return code is %d",
                          apl[i].parameters, exit_status);
            }
            free(apl[i].parameters);
            if (i < (no_of_process - 1))
            {
               (void)memmove(&apl[i], &apl[i + 1],
                             ((no_of_process - i) * sizeof(struct aldad_proc_list)));
            }
            no_of_process--;
            i--;
            if (no_of_process == 0)
            {
               free(apl);
               apl = NULL;
            }
            else
            {
               if ((apl = realloc(apl,
                                  (no_of_process * sizeof(struct aldad_proc_list)))) == NULL)
               {
                  system_log(FATAL_SIGN, __FILE__, __LINE__,
                             "Failed to realloc() memory : %s",
                             strerror(errno));
                  exit(INCORRECT);
               }
            }
         }
         else if (WIFSIGNALED(status))
              {
                 /* Termination caused by signal. */
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Alda log process (%s) terminated by signal %d.",
                            apl[i].parameters, WTERMSIG(status));
                 free(apl[i].parameters);
                 if (i < no_of_process)
                 {
                    (void)memmove(&apl[i], &apl[i + 1],
                                  ((no_of_process - i) * sizeof(struct aldad_proc_list)));
                 }
                 no_of_process--;
                 i--;
                 if (no_of_process == 0)
                 {
                    free(apl);
                    apl = NULL;
                 }
                 else
                 {
                    if ((apl = realloc(apl,
                                       (no_of_process * sizeof(struct aldad_proc_list)))) == NULL)
                    {
                       system_log(FATAL_SIGN, __FILE__, __LINE__,
                                  "Failed to realloc() memory : %s",
                                  strerror(errno));
                       exit(INCORRECT);
                    }
                 }
              }
         else if (WIFSTOPPED(status))
              {
                 /* Child has been stopped. */
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Alda log process (%s) received STOP signal.",
                            apl[i].parameters);
              }
      }
   }

   return;
}


/*+++++++++++++++++++++++++++++ aldad_exit() ++++++++++++++++++++++++++++*/
static void                                                                
aldad_exit(void)
{
   int i;

   system_log(INFO_SIGN, NULL, 0, "Stopped %s.", ALDAD);

   /* Kill all jobs that where started. */
   for (i = 0; i < no_of_process; i++)
   {
      if (kill(apl[i].pid, SIGINT) < 0)
      {
         if (errno != ESRCH)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                       "Failed to kill process alda with pid %d : %s",
#else
                       "Failed to kill process alda with pid %lld : %s",
#endif
                       (pri_pid_t)apl[i].pid, strerror(errno));
         }
      }
   }

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   system_log(FATAL_SIGN, __FILE__, __LINE__,
              "Aaarrrggh! Received SIGSEGV.");
   aldad_exit();
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   system_log(FATAL_SIGN, __FILE__, __LINE__, "Uuurrrggh! Received SIGBUS.");
   aldad_exit();
   abort();
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   int ret;

   (void)fprintf(stderr,
#if SIZEOF_PID_T == 4
                 "%s terminated by signal %d (%d)\n",
#else
                 "%s terminated by signal %d (%lld)\n",
#endif
                 ALDAD, signo, (pri_pid_t)getpid());
   if ((signo == SIGINT) || (signo == SIGTERM))
   {
      ret = SUCCESS;
   }
   else
   {
      ret = INCORRECT;
   }

   exit(ret);
}
