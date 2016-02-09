/*
 *  open_counter_file.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2010 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   open_counter_file - opens the AFD counter file
 **
 ** SYNOPSIS
 **   int  open_counter_file(char *file_name, int **counter)
 **   int  next_counter(int counter_fd, int *counter, int max_counter)
 **   void next_counter_no_lock(int *counter, int max_counter)
 **   void close_counter_file(int counter_fd, int **counter)
 **
 ** DESCRIPTION
 **   This function open_counter_file() opens the given counter file
 **   and maps to it. This counter can be used as a unique counter.
 **
 **   The function next_counter() reads and returns the current counter.
 **   It then increments the counter by one and writes this value back
 **   to the counter file. When the value is larger then max_counter
 **   it resets the counter to zero.
 **
 ** RETURN VALUES
 **   open_counter_file() returns the file descriptor of the open file
 **   or INCORRECT when it fails to do so.
 **
 **   Function next_counter() returns the current value 'counter' in the
 **   AFD counter file and SUCCESS. Else INCORRECT will be returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.08.1996 H.Kiehl Created
 **   21.08.1998 H.Kiehl Allow to open different counter files and
 **                      if not available create and initialize it.
 **   28.03.2009 H.Kiehl Instead of always doing a read(), write() and
 **                      lseek(), lets just map to the counter file.
 **   30.03.2009 H.Kiehl Added function close_counter_file().
 **   02.12.2010 H.Kiehl Function next_counter() now has the additional
 **                      parameter max_counter. So we can remove
 **                      function next_wmo_counter().
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>           /* strcpy(), strcat(), strerror()          */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>        /* mmap(), munmap()                        */
#endif
#include <unistd.h>           /* lseek(), write()                        */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>

/* External global variables. */
extern char *p_work_dir;


/*########################### open_counter_file() #######################*/
int
open_counter_file(char *file_name, int **counter)
{
   int  fd;
   char counter_file[MAX_PATH_LENGTH];

   (void)strcpy(counter_file, p_work_dir);
   (void)strcat(counter_file, FIFO_DIR);
   (void)strcat(counter_file, file_name);
   if ((fd = coe_open(counter_file, O_RDWR)) == -1)
   {
      if (errno == ENOENT)
      {
         if ((fd = coe_open(counter_file, O_RDWR | O_CREAT,
#ifdef GROUP_CAN_WRITE
                            S_IRUSR | S_IWUSR| S_IRGRP | S_IWGRP)) == -1)
#else
                            S_IRUSR | S_IWUSR)) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Could not open() `%s' : %s"),
                       counter_file, strerror(errno));
         }
         else
         {
            if (write(fd, &fd, sizeof(int)) != sizeof(int))
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to write() to `%s' : %s"),
                          counter_file, strerror(errno));
            }
            else
            {
               if (lseek(fd, 0, SEEK_SET) == -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Could not lseek() to start in `%s' : %s"),
                             counter_file, strerror(errno));
               }
               else
               {
                  char *ptr;

                  /* Map to counter file. */
#ifdef HAVE_MMAP
                  if ((ptr = mmap(NULL, sizeof(int), (PROT_READ | PROT_WRITE),
                                  MAP_SHARED, fd, 0)) == (caddr_t) -1)
#else
                  if ((ptr = mmap_emu(NULL, sizeof(int), (PROT_READ | PROT_WRITE),
                                      MAP_SHARED, counter_file, 0)) == (caddr_t) -1)
#endif
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                _("mmap() error : %s"), strerror(errno));
                     (void)close(fd);
                  }
                  else
                  {
                     *counter = (int *)ptr;
                     **counter = 0;

                     return(fd);
                  }
               }
            }
         }
      }
      else
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not open `%s' : %s"),
                    counter_file, strerror(errno));
      }
   }
   else
   {
      char *ptr;

      /* Map to counter file. */
#ifdef HAVE_MMAP
      if ((ptr = mmap(NULL, sizeof(int), (PROT_READ | PROT_WRITE),
                      MAP_SHARED, fd, 0)) == (caddr_t) -1)
#else
      if ((ptr = mmap_emu(NULL, sizeof(int), (PROT_READ | PROT_WRITE),
                          MAP_SHARED, counter_file, 0)) == (caddr_t) -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("mmap() error : %s"), strerror(errno));
         (void)close(fd);
      }
      else
      {
         *counter = (int *)ptr;

         return(fd);
      }
   }

   return(INCORRECT);
}


/*############################# next_counter() ##########################*/
int
next_counter(int counter_fd, int *counter, int max_counter)
{
   struct flock wulock = {F_WRLCK, SEEK_CUR, 0, 1};

   /* Try to lock file which holds counter. */
   if (fcntl(counter_fd, F_SETLKW, &wulock) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not set write lock : %s"), strerror(errno));
      return(INCORRECT);
   }

   /* Ensure that counter does not become larger then max_counter. */
   if (((*counter)++ >= max_counter) || (*counter < 0))
   {
      *counter = 0;
   }

   /* Unlock file which holds the counter. */
   wulock.l_type = F_UNLCK;
   if (fcntl(counter_fd, F_SETLKW, &wulock) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not unset write lock : %s"), strerror(errno));
      return(INCORRECT);
   }

   return(SUCCESS);
}


/*######################### next_counter_no_lock() ######################*/
void
next_counter_no_lock(int *counter, int max_counter)
{
   /* Ensure that counter does not become larger then max_counter. */
   if (((*counter)++ >= max_counter) || (*counter < 0))
   {
      *counter = 0;
   }

   return;
}


/*########################## close_counter_file() #######################*/
void
close_counter_file(int counter_fd, int **counter)
{
   char *ptr;

   ptr = (char *)*counter;
#ifdef HAVE_MMAP
   if (munmap(ptr, sizeof(int)) == -1)
#else
   if (munmap_emu(ptr) == -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to munmap() from counter file : %s"),
                 strerror(errno));
   }
   else
   {
      *counter = NULL;
   }

   if (close(counter_fd) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 _("Failed to close() counter file : %s"), strerror(errno));
   }

   return;
}
