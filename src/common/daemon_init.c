/*
 *  daemon_init.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   daemon_init - initialises the calling process as daemon
 **
 ** SYNOPSIS
 **   void daemon_init(char *process)
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
 **   07.06.1997 H.Kiehl Created
 **   22.05.1999 H.Kiehl Log everything from stdout and stderr.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* strerror(), strlen(), memset()  */
#include <stdlib.h>                   /* malloc(), free()                */
#include <time.h>                     /* time()                          */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <unistd.h>                   /* STDIN_FILENO, dup2()            */
#include <errno.h>

/* External global variables. */
extern char *p_work_dir;

/*
 * NOTE: Only define _CLOSE_STDIN if you are absolutly sure you do
 *       not need stdin. There are always some application (eg. IDL)
 *       that need stdin when called with the exec option.
 */

/*############################ daemon_init() ############################*/
void
daemon_init(char *process)
{
   pid_t pid;

   if ((pid = fork()) < 0)
   {
      (void)fprintf(stderr, _("fork() error : %s (%s %d)\n"),
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   else if (pid != 0)
        {
           _exit(0); /* Parent process exits. */
        }

   if (setsid() == (pid_t)-1)
   {
      (void)fprintf(stderr, _("setsid() error : %s (%s %d)\n"),
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if ((pid = fork()) < 0)
   {
      (void)fprintf(stderr, _("fork() error : %s (%s %d)\n"),
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   else if (pid != 0)
        {
           _exit(0); /* Parent process exits. */
        }

   /*
    * Redirect stdout and stderr to DAEMON_LOG.process. And redirect
    * stdin to /dev/null. That way we are able to see if someone writes
    * to stdout or stderr and will not hang if some process wants to read
    * from stdin.
    */
   if (process != NULL)
   {
      int          fd,
                   mode;
      time_t       current_time;
      size_t       length;
      char         *buffer,
                   daemon_log[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat  stat_buf;
#endif

      (void)snprintf(daemon_log, MAX_PATH_LENGTH, "%s%s", p_work_dir, LOG_DIR);
      if (check_dir(daemon_log, R_OK | W_OK | X_OK) < 0)
      {
         (void)fprintf(stderr, _("Failed to create directory `%s' (%s %d)\n"),
                       daemon_log, __FILE__, __LINE__);
         exit(INCORRECT);
      }
      (void)snprintf(daemon_log, MAX_PATH_LENGTH, "%s%s/DAEMON_LOG.%s",
                     p_work_dir, LOG_DIR, process);
#ifdef HAVE_STATX
      if (statx(0, daemon_log, AT_STATX_SYNC_AS_STAT,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (stat(daemon_log, &stat_buf) == -1)
#endif
      {
         mode = O_CREAT | O_TRUNC | O_WRONLY;
      }
      else
      {
#ifdef HAVE_STATX
         if (stat_buf.stx_size > 102400)
#else
         if (stat_buf.st_size > 102400)
#endif
         {
            mode = O_CREAT | O_TRUNC | O_WRONLY;
         }
         else
         {
            mode = O_CREAT | O_APPEND | O_WRONLY;
         }
      }
 /*     if ((fd = coe_open(daemon_log, mode, FILE_MODE)) == -1) */
      if ((fd = coe_open(daemon_log, mode, (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH))) == -1)
      {
         (void)fprintf(stderr, _("Failed to coe_open() `%s' : %s (%s %d)\n"),
                       daemon_log, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      dup2(fd, STDERR_FILENO);
      dup2(fd, STDOUT_FILENO);
      if (close(fd) == -1)
      {
         (void)fprintf(stderr, _("close() error : %s (%s %d)\n"),
                       strerror(errno), __FILE__, __LINE__);
      }
#ifdef _CLOSE_STDIN
      (void)fclose(stdin);
#else
      if ((fd = coe_open("/dev/null", O_RDONLY)) == -1)
      {
         (void)fprintf(stderr,
                       _("Failed to coe_open() /dev/null : %s (%s %d)\n"),
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      dup2(fd, STDIN_FILENO);
      if (close(fd) == -1)
      {
         (void)fprintf(stderr, _("close() error : %s (%s %d)\n"),
                       strerror(errno), __FILE__, __LINE__);
      }
#endif
      length = 35 + strlen(process);
      if ((buffer = malloc(length + 1)) != NULL)
      {
         (void)memset(buffer, '=', length);
         buffer[length] = '\0';
         current_time = time(NULL);
         (void)fprintf(stderr, _("%s\n%.24s : Started %s\n"),
                       buffer, ctime(&current_time), process);
         (void)memset(buffer, '-', length);
         (void)fprintf(stderr, "%s\n", buffer);
         free(buffer);
      }
   }
   else
   {
      (void)fclose(stdin);
      (void)fclose(stdout);
      (void)fclose(stderr);
   }

   /*
    * Hmm. For the daemon process to dump a core it needs to write
    * access in the directory where the core gets dumped.
    */
   if (p_work_dir != NULL)
   {
      if (chdir(p_work_dir) == -1)
      {
         (void)fprintf(stderr,
                       "Failed to change directory to `%s' : %s (%s %d)\n",
                       p_work_dir, strerror(errno), __FILE__, __LINE__);
      }
   }
   else
   {
      if (chdir("/") == -1)
      {
         (void)fprintf(stderr,
                       "Failed to change directory to `/' : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
      }
   }
   umask(0);

   return;
}
