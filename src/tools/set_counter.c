/*
 *  set_counter.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2015 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   set_counter - sets the AFD counter
 **
 ** SYNOPSIS
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.08.1996 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                    /* fprintf()                       */
#include <string.h>                   /* strcpy(), strcat(), strerror()  */
#include <unistd.h>                   /* read(), write(), close(),       */
                                      /* STDERR_FILENO                   */
#include <stdlib.h>                   /* atoi()                          */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"
#include "version.h"

/* Global variables. */
int        sys_log_fd = STDERR_FILENO; /* Needed by get_afd_path(). */
char       *p_work_dir = NULL;
const char *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int          counter,
                new_counter,
                fd;
   char         counter_file[MAX_PATH_LENGTH],
                work_dir[MAX_PATH_LENGTH];
   struct flock wlock = {F_WRLCK, SEEK_SET, 0, 1},
                ulock = {F_UNLCK, SEEK_SET, 0, 1};

   CHECK_FOR_VERSION(argc, argv);

   if ((argc < 2) || (my_strcmp(argv[1], "-w") == 0))
   {
      (void)fprintf(stderr, "%s: <new value> [-w <workdir>]\n", argv[0]);
      exit(INCORRECT);
   }
   new_counter = atoi(argv[1]);
   argc--; argv++;

   /* Open file that holds the counter. */
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   (void)strcpy(counter_file, work_dir);
   (void)strcat(counter_file, FIFO_DIR);
   (void)strcat(counter_file, COUNTER_FILE);
#ifdef GROUP_CAN_WRITE
   if ((fd = open(counter_file, O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) < 0)
#else
   if ((fd = open(counter_file, O_RDWR, S_IRUSR | S_IWUSR)) < 0)
#endif
   {
      (void)fprintf(stderr, "Could not open %s : %s (%s %d)\n",
                    counter_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Try to lock file which holds counter. */
   if (fcntl(fd, F_SETLKW, &wlock) < 0)
   {
      (void)fprintf(stderr, "Could not set write lock. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Read the value of counter. */
   if (read(fd, &counter, sizeof(int)) < 0)
   {
      (void)fprintf(stderr, "Could not read value of counter : %s (%s %d)\n",
                    strerror(errno),  __FILE__, __LINE__);
      exit(INCORRECT);
   }

   (void)fprintf(stdout, "Befor changing AFD counter = %d\n", counter);

   /* Go to beginning in file. */
   if (lseek(fd, 0, SEEK_SET) < 0)
   {
      (void)fprintf(stderr, "Could not seek() : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
   }

   /* Write new value into counter file. */
   if (write(fd, &new_counter, sizeof(int)) != sizeof(int))
   {
      (void)fprintf(stderr, "Could not write value to counter file : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   (void)fprintf(stdout, "Changed AFD counter to %d\n", new_counter);

   /* Unlock file which holds counter. */
   if (fcntl(fd, F_SETLKW, &ulock) < 0)
   {
      (void)fprintf(stderr, "Could not unset write lock : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Close the counter file. */
   (void)close(fd);

   exit(SUCCESS);
}
