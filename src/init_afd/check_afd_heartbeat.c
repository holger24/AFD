/*
 *  check_afd_heartbeat.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_afd_heartbeat - Checks if the heartbeat of process init_afd
 **                         is still working.
 **
 ** SYNOPSIS
 **   int check_afd_heartbeat(long wait_time, int remove_process)
 **
 ** DESCRIPTION
 **   This function checks if the heartbeat of init_afd is still
 **   going.
 **
 ** RETURN VALUES
 **   Returns 1 if another AFD is active. 2 is returned when
 **   we got a timeout. 3 is returned when init_afd is active
 **   but all process are stopped. Otherwise it returns 0.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.06.2002 H.Kiehl Created
 **   27.04.2009 H.Kiehl Read from AFD_ACTIVE file so NFS updates the
 **                      content.
 **   11.10.2011 H.Kiehl Return 2 to indicate that we did not get any
 **                      responce.
 **   26.11.2018 H.Kiehl Try shorten the time when init_afd is dead by
 **                      checking if process is active.
 **   09.07.2020 H.Kiehl With systemd init_afd stays alive when
 **                      AFD is shutdown. So add additional checks
 **                      if other (system_log, archive_watch) are
 **                      active.
 **
 */
DESCR__E_M3

#include <stdio.h>               /* NULL                                 */
#include <string.h>              /* strerror(), memcpy()                 */
#include <stdlib.h>              /* malloc(), free()                     */
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>              /* kill()                               */
#include <unistd.h>              /* unlink(), lseek(), read()            */
#include <fcntl.h>               /* O_RDWR                               */
#include <errno.h>

/* External global variables. */
extern char afd_active_file[];

/* Local function prototype. */
static void kill_jobs(off_t);


/*######################## check_afd_heartbeat() ########################*/
int
check_afd_heartbeat(long wait_time, int remove_process)
{
   int          afd_active = 0;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

#ifdef HAVE_STATX
   if ((statx(0, afd_active_file, AT_STATX_SYNC_AS_STAT,
              STATX_SIZE, &stat_buf) == 0) &&
       (stat_buf.stx_size > (((NO_OF_PROCESS + 1) * sizeof(pid_t)) + sizeof(unsigned int))))
#else
   if ((stat(afd_active_file, &stat_buf) == 0) &&
       (stat_buf.st_size > (((NO_OF_PROCESS + 1) * sizeof(pid_t)) + sizeof(unsigned int))))
#endif
   {
      int afd_active_fd;

      if ((afd_active_fd = open(afd_active_file, O_RDWR)) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to open() `%s' : %s"),
                    afd_active_file, strerror(errno));
      }
      else
      {
         unsigned int current_value,
                      heartbeat;
         long         elapsed_time = 0L;
         pid_t        ia_pid;
         ssize_t      n;
         off_t        offset;
#ifdef WITH_SYSTEMD
         char         *buffer,
                      *pid_list;

#ifdef HAVE_STATX
         if ((buffer = malloc(stat_buf.stx_size)) == NULL)
#else
         if ((buffer = malloc(stat_buf.st_size)) == NULL)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("malloc() error : %s"), strerror(errno));
            (void)close(afd_active_fd);
            return(0);
         }

         /* First read  pid's to check if these process are still alive. */
#ifdef HAVE_STATX
         if ((n = read(afd_active_fd, buffer, stat_buf.stx_size)) != stat_buf.stx_size)
#else
         if ((n = read(afd_active_fd, buffer, stat_buf.st_size)) != stat_buf.st_size)
#endif
         {
            if (n != 0)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("read() error : %s"), strerror(errno));
            }
            (void)close(afd_active_fd);
            free(buffer);
            return(0);
         }
         (void)memcpy(&ia_pid, buffer, sizeof(pid_t));
         pid_list = buffer;
#else
         /* First read init_afd pid to check if this process is still alive. */
         if ((n = read(afd_active_fd, &ia_pid, sizeof(pid_t))) != sizeof(pid_t))
         {
            if (n != 0)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("read() error : %s"), strerror(errno));
            }
            (void)close(afd_active_fd);
            return(0);
         }
#endif
         if (ia_pid > 0)
         {
            if ((kill(ia_pid, 0) == -1) && (errno == ESRCH))
            {
               /* init_afd is no longer alive, so lets assume AFD is */
               /* no longer active. We will still check for a        */
               /* heartbeat, but let's wait only for a short time.   */
               if (wait_time > 2)
               {
                  wait_time = 2;
               }
            }
#ifdef LINUX
            else /* We find a process alive with this pid. This does */
                 /* not mean that this is really init_afd. So, let's */
                 /* do another check if the process name really is   */
                 /* init_afd.                                        */
            {
               char proc_name[96];

               get_proc_name_from_pid(ia_pid, proc_name);
               if (proc_name[0] != '\0')
               {
                  if (posi(proc_name, AFD) == NULL)
                  {
                     if (wait_time > 2)
                     {
                        wait_time = 2;
                     }
                  }
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
# if SIZEOF_PID_T == 4
                             "Found %s for pid %ld [wait_time=%ld]",
# else
                             "Found %s for pid %lld [wait_time=%ld]",
# endif
                             proc_name, (pri_pid_t)ia_pid, wait_time);
               }
            }
#endif
         }

         /* NOTE: Since this is a memory mapped file, NFS needs a read() */
         /*       event so it does update the content of the file. So    */
         /*       do NOT use mmap()!!!                                   */
         offset = (NO_OF_PROCESS + 1) * sizeof(pid_t);
         if (lseek(afd_active_fd, offset, SEEK_SET) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("lseek() error : %s"), strerror(errno));
            (void)close(afd_active_fd);
#ifdef WITH_SYSTEMD
            free(buffer);
#endif
            return(0);
         }
         if (read(afd_active_fd, &current_value, sizeof(unsigned int)) != sizeof(unsigned int))
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("read() error : %s"), strerror(errno));
            (void)close(afd_active_fd);
#ifdef WITH_SYSTEMD
            free(buffer);
#endif
            return(0);
         }

         wait_time = wait_time * 1000000L;
         while (elapsed_time < wait_time)
         {
            if (lseek(afd_active_fd, offset, SEEK_SET) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("lseek() error : %s"), strerror(errno));
               (void)close(afd_active_fd);
#ifdef WITH_SYSTEMD
               free(buffer);
#endif
               return(0);
            }
            if (read(afd_active_fd, &heartbeat, sizeof(unsigned int)) != sizeof(unsigned int))
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("read() error : %s"), strerror(errno));
               (void)close(afd_active_fd);
#ifdef WITH_SYSTEMD
               free(buffer);
#endif
               return(0);
            }
            if (current_value != heartbeat)
            {
               afd_active = 1;
               break;
            }
            my_usleep(100000L);
            elapsed_time += 100000L;
         }
         if ((afd_active == 0) && (remove_process == YES))
         {
#ifdef HAVE_STATX
            kill_jobs(stat_buf.stx_size);
#else
            kill_jobs(stat_buf.st_size);
#endif
         }
         if (elapsed_time > wait_time)
         {
            afd_active = 2; /* To signal a timeout. */
         }
         if (close(afd_active_fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("Failed to close() `%s' : %s"),
                       afd_active_file, strerror(errno));
         }
#ifdef WITH_SYSTEMD
         if (afd_active == 1)
         {
            int   i;
            char  name_list[2][96];
            pid_t check_list[2];

            check_list[0] = *(pid_t *)(pid_list + ((SLOG_NO + 1) * sizeof(pid_t)));
            (void)strcpy(name_list[0], SLOG);
            check_list[1] = *(pid_t *)(pid_list + ((AW_NO + 1) * sizeof(pid_t)));
            (void)strcpy(name_list[1], ARCHIVE_WATCH);
            for (i = 0; i < 2; i++)
            {
               if (check_list[i] > 0)
               {
                  if ((kill(check_list[i], 0) == -1) && (errno == ESRCH))
                  {
                     afd_active = 3;
                     break;
                  }
# ifdef LINUX
                  else /* We find a process alive with this pid. This does */
                       /* not mean that this is really is our process. So, */
                       /* let's do another check if the process name really*/
                       /* is the one we are lokking for.                   */
                  {
                     char proc_name[96];

                     get_proc_name_from_pid(check_list[i], proc_name);
                     if (proc_name[0] != '\0')
                     {
                        if (posi(proc_name, name_list[i]) == NULL)
                        {
                           afd_active = 3;
                           break;
                        }
                     }
                  }
# endif
               }
            }
         }
         free(buffer);
#endif /* WITH_SYSTEMD */
      }
   }

   return(afd_active);
}


/*++++++++++++++++++++++++++++ kill_jobs() ++++++++++++++++++++++++++++++*/
static void
kill_jobs(off_t st_size)
{
   char *ptr,
        *buffer;
   int  i,
        loops = 0,
        process_left,
        read_fd;

   if ((read_fd = open(afd_active_file, O_RDWR)) < 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("Failed to open `%s' : %s"),
                 afd_active_file, strerror(errno));
      exit(-10);
   }

   if ((buffer = malloc(st_size)) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("malloc() error : %s"), strerror(errno));
      exit(-11);
   }

   if (read(read_fd, buffer, st_size) != st_size)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("read() error : %s"), strerror(errno));
      exit(-12);
   }

   /* Try to send kill signal to all running process. */
   ptr = buffer;
   for (i = 0; i <= NO_OF_PROCESS; i++)
   {
      if (*(pid_t *)ptr > 0)
      {
         (void)kill(*(pid_t *)ptr, SIGINT);
      }
      ptr += sizeof(pid_t);
   }

   /* Check for 10 seconds if they are all gone. */
   do
   {
      process_left = 0;
      ptr = buffer;
      for (i = 0; i <= NO_OF_PROCESS; i++)
      {
         if (*(pid_t *)ptr > 0)
         {
            if (kill(*(pid_t *)ptr, 0) == 0)
            {
               process_left++;
            }
            else
            {
               *(pid_t *)ptr = 0;
            }
         }
         ptr += sizeof(pid_t);
      }
      if (process_left > 0)
      {
         my_usleep(10000L);
      }
      loops++;
   } while ((process_left > 0) && (loops < 1000));

   if (process_left > 0)
   {
      /* Now kill am the hard way. */
      ptr = buffer;
      for (i = 0; i <= NO_OF_PROCESS; i++)
      {
         if (*(pid_t *)ptr > 0)
         {
            (void)kill(*(pid_t *)ptr, SIGKILL);
            *(pid_t *)ptr = 0;
         }
         ptr += sizeof(pid_t);
      }
   }

   if (close(read_fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 _("close() error : %s"), strerror(errno));
   }
   free(buffer);

   (void)unlink(afd_active_file);

   return;
}
