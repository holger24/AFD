/*
 *  fra_edit.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   fra_edit - changes certain values int the FRA
 **
 ** SYNOPSIS
 **   fra_edit [working directory] diralias|position
 **
 ** DESCRIPTION
 **   So far this program can change the following values:
 **   total_file_counter (fd), total_file_size (bd) and error_counter
 **   (ec).
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.09.2007 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr            */
#include <string.h>                      /* strcpy(), strerror()         */
#include <stdlib.h>                      /* atoi()                       */
#include <ctype.h>                       /* isdigit()                    */
#include <time.h>                        /* ctime()                      */
#include <termios.h>
#include <unistd.h>                      /* sleep(), STDERR_FILENO       */
#include <setjmp.h>
#include <signal.h>
#include <sys/types.h>
#include <errno.h>
#include "version.h"

/* Local functions. */
static unsigned char       get_key(void);
static jmp_buf             env_alarm;
static void                menu(int),
                           usage(char *),
                           sig_handler(int),
                           sig_alarm(int);

/* Global variables. */
int                        sys_log_fd = STDERR_FILENO,   /* Not used!    */
                           fra_fd = -1,
                           fra_id,
                           no_of_dirs = 0;
#ifdef HAVE_MMAP
off_t                      fra_size;
#endif
char                       *p_work_dir;
struct termios             buf,
                           set;
struct fileretrieve_status *fra;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ fra_edit() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int          position = -1,
                leave_flag = NO,
                ret;
   unsigned int value;
   char         dir_alias[MAX_DIR_ALIAS_LENGTH + 1],
                work_dir[MAX_PATH_LENGTH];

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if (argc == 2)
   {
      if (isdigit((int)(argv[1][0])) != 0)
      {
         position = atoi(argv[1]);
      }
      else
      {
         (void)strcpy(dir_alias, argv[1]);
      }
   }
   else
   {
      usage(argv[0]);
      exit(INCORRECT);
   }

   if ((ret = fra_attach()) != SUCCESS)
   {
      if (ret == INCORRECT_VERSION)
      {
         (void)fprintf(stderr,
                       _("ERROR   : This program is not able to attach to the FRA due to incorrect version. (%s %d)\n"),
                       __FILE__, __LINE__);
      }
      else
      {
         if (ret < 0)
         {
            (void)fprintf(stderr,
                          _("ERROR   : Failed to attach to FRA. (%s %d)\n"),
                          __FILE__, __LINE__);
         }
         else
         {
            (void)fprintf(stderr,
                          _("ERROR   : Failed to attach to FRA : %s (%s %d)\n"),
                          strerror(ret), __FILE__, __LINE__);
         }
      }
      exit(INCORRECT);
   }

   if (tcgetattr(STDIN_FILENO, &buf) < 0)
   {
      (void)fprintf(stderr, _("ERROR   : tcgetattr() error : %s (%s %d)\n"),
                    strerror(errno), __FILE__, __LINE__);
      exit(0);
   }

   if (position < 0)
   {
      if ((position = get_dir_position(fra, dir_alias, no_of_dirs)) < 0)
      {
         (void)fprintf(stderr,
                       _("ERROR   : Could not find directory %s in FRA. (%s %d)\n"),
                       dir_alias, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   for (;;)
   {
      menu(position);

      switch (get_key())
      {
         case 0   : break;
         case '1' : (void)fprintf(stderr, _("\n\n     Enter value [1] : "));
                    if (scanf("%11u", &value) == EOF)
                    {
                       (void)fprintf(stderr,
                                     _("ERROR   : scanf() error, failed to read input : %s (%s %d)\n"),
                                     strerror(errno), __FILE__, __LINE__);
                       exit(INCORRECT);
                    }
                    fra[position].files_in_dir = (int)value;
                    break;
         case '2' : (void)fprintf(stderr, _("\n\n     Enter value [2] : "));
                    if (scanf("%11u", &value) == EOF)
                    {
                       (void)fprintf(stderr,
                                     _("ERROR   : scanf() error, failed to read input : %s (%s %d)\n"),
                                     strerror(errno), __FILE__, __LINE__);
                       exit(INCORRECT);
                    }
                    fra[position].bytes_in_dir = value;
                    break;
         case '3' : (void)fprintf(stderr, _("\n\n     Enter value [3] : "));
                    if (scanf("%11u", &value) == EOF)
                    {
                       (void)fprintf(stderr,
                                     _("ERROR   : scanf() error, failed to read input : %s (%s %d)\n"),
                                     strerror(errno), __FILE__, __LINE__);
                       exit(INCORRECT);
                    }
                    fra[position].files_queued = value;
                    break;
         case '4' : (void)fprintf(stderr, _("\n\n     Enter value [4] : "));
                    if (scanf("%11u", &value) == EOF)
                    {
                       (void)fprintf(stderr,
                                     _("ERROR   : scanf() error, failed to read input : %s (%s %d)\n"),
                                     strerror(errno), __FILE__, __LINE__);
                       exit(INCORRECT);
                    }
                    fra[position].bytes_in_queue = value;
                    break;
         case '5' : (void)fprintf(stderr, _("\n\n     Enter value [5] : "));
                    if (scanf("%11u", &value) == EOF)
                    {
                       (void)fprintf(stderr,
                                     _("ERROR   : scanf() error, failed to read input : %s (%s %d)\n"),
                                     strerror(errno), __FILE__, __LINE__);
                       exit(INCORRECT);
                    }
                    fra[position].error_counter = value;
                    break;
         case '6' : (void)fprintf(stdout, "\033[2J\033[3;1H");
                    (void)fprintf(stdout, "\n\n\n");
                    (void)fprintf(stdout, "     Reset to zero.................(0)\n");
                    (void)fprintf(stdout, "     MAX_COPIED [%d]...............(1)\n",
                                  (fra[position].dir_flag & MAX_COPIED) ? 1 : 0);
                    (void)fprintf(stdout, "     FILES_IN_QUEUE [%d]...........(2)\n",
                                  (fra[position].dir_flag & FILES_IN_QUEUE) ? 1 : 0);
                    (void)fprintf(stdout, "     LINK_NO_EXEC [%d].............(3)\n",
                                  (fra[position].dir_flag & LINK_NO_EXEC) ? 1 : 0);
                    (void)fprintf(stdout, "     DIR_DISABLED [%d].............(4)\n",
                                  (fra[position].dir_flag & DIR_DISABLED) ? 1 : 0);
                    (void)fprintf(stdout, "     ACCEPT_DOT_FILES [%d].........(5)\n",
                                  (fra[position].dir_options & ACCEPT_DOT_FILES) ? 1 : 0);
                    (void)fprintf(stdout, "     DONT_GET_DIR_LIST [%d]........(6)\n",
                                  (fra[position].dir_options & DONT_GET_DIR_LIST) ? 1 : 0);
                    (void)fprintf(stdout, "     DIR_ERROR_SET [%d]............(7)\n",
                                  (fra[position].dir_flag & DIR_ERROR_SET) ? 1 : 0);
                    (void)fprintf(stdout, "     WARN_TIME_REACHED [%d]........(8)\n",
                                  (fra[position].dir_flag & WARN_TIME_REACHED) ? 1 : 0);
                    (void)fprintf(stdout, "     DIR_ERROR_ACKN [%d]...........(9)\n",
                                  (fra[position].dir_flag & DIR_ERROR_ACKN) ? 1 : 0);
                    (void)fprintf(stdout, "     DIR_ERROR_OFFLINE [%d]........(a)\n",
                                  (fra[position].dir_flag & DIR_ERROR_OFFLINE) ? 1 : 0);
                    (void)fprintf(stdout, "     DIR_ERROR_ACKN_T [%d].........(b)\n",
                                  (fra[position].dir_flag & DIR_ERROR_ACKN_T) ? 1 : 0);
                    (void)fprintf(stdout, "     DIR_ERROR_OFFL_T [%d].........(c)\n",
                                  (fra[position].dir_flag & DIR_ERROR_OFFL_T) ? 1 : 0);
                    (void)fprintf(stdout, "     DIR_STOPPED [%d]..............(d)\n",
                                  (fra[position].dir_flag & DIR_ERROR_OFFL_T) ? 1 : 0);
#ifdef WITH_INOTIFY
                    (void)fprintf(stdout, "     INOTIFY_RENAME [%d]...........(e)\n",
                                  (fra[position].dir_options & INOTIFY_RENAME) ? 1 : 0);
                    (void)fprintf(stdout, "     INOTIFY_CLOSE [%d]............(f)\n",
                                  (fra[position].dir_options & INOTIFY_CLOSE) ? 1 : 0);
                    (void)fprintf(stdout, "     INOTIFY_CREATE [%d]...........(g)\n",
                                  (fra[position].dir_options & INOTIFY_CREATE) ? 1 : 0);
                    (void)fprintf(stdout, "     INOTIFY_DELETE [%d]...........(h)\n",
                                  (fra[position].dir_options & INOTIFY_DELETE) ? 1 : 0);
                    (void)fprintf(stdout, "     INOTIFY_ATTRIB [%d]...........(i)\n",
                                  (fra[position].dir_options & INOTIFY_ATTRIB) ? 1 : 0);
#endif
                    (void)fprintf(stdout, "     ALL_DISABLED [%d].............(j)\n",
                                  (fra[position].dir_flag & ALL_DISABLED) ? 1 : 0);
                    (void)fprintf(stderr, "     None..........................(Z) ");

                    switch (get_key())
                    {
                       case '0' : fra[position].dir_flag = 0;
                                  break;
                       case '1' : fra[position].dir_flag ^= MAX_COPIED;
                                  break;
                       case '2' : fra[position].dir_flag ^= FILES_IN_QUEUE;
                                  break;
                       case '3' : fra[position].dir_flag ^= LINK_NO_EXEC;
                                  break;
                       case '4' : fra[position].dir_flag ^= DIR_DISABLED;
                                  break;
                       case '5' : fra[position].dir_options ^= ACCEPT_DOT_FILES;
                                  break;
                       case '6' : fra[position].dir_options ^= DONT_GET_DIR_LIST;
                                  break;
                       case '7' : fra[position].dir_flag ^= DIR_ERROR_SET;
                                  break;
                       case '8' : fra[position].dir_flag ^= WARN_TIME_REACHED;
                                  break;
                       case '9' : fra[position].dir_flag ^= DIR_ERROR_ACKN;
                                  break;
                       case 'a' : fra[position].dir_flag ^= DIR_ERROR_OFFLINE;
                                  break;
                       case 'b' : fra[position].dir_flag ^= DIR_ERROR_ACKN_T;
                                  break;
                       case 'c' : fra[position].dir_flag ^= DIR_ERROR_OFFL_T;
                                  break;
                       case 'd' : fra[position].dir_flag ^= DIR_STOPPED;
                                  break;
#ifdef WITH_INOTIFY
                       case 'e' : fra[position].dir_options ^= INOTIFY_RENAME;
                                  break;
                       case 'f' : fra[position].dir_options ^= INOTIFY_CLOSE;
                                  break;
                       case 'g' : fra[position].dir_options ^= INOTIFY_CREATE;
                                  break;
                       case 'h' : fra[position].dir_options ^= INOTIFY_DELETE;
                                  break;
                       case 'i' : fra[position].dir_options ^= INOTIFY_ATTRIB;
                                  break;
#endif
                       case 'j' : fra[position].dir_flag ^= ALL_DISABLED;
                                  break;
                       case 'Z' : break;
                       default  : (void)printf(_("Wrong choice!\n"));
                                  (void)sleep(1);
                                  break;
                    }
                    break;
         case '7' : (void)fprintf(stderr, _("\n\n     Enter value [7] : "));
                    if (scanf("%11u", &value) == EOF)
                    {
                       (void)fprintf(stderr,
                                     _("ERROR   : scanf() error, failed to read input : %s (%s %d)\n"),
                                     strerror(errno), __FILE__, __LINE__);
                       exit(INCORRECT);
                    }
                    fra[position].queued = (char)value;
                    break;
         case 'x' :
         case 'Q' :
         case 'q' : leave_flag = YES;
                    break;
         default  : (void)printf(_("Wrong choice!\n"));
                    (void)sleep(1);
                    break;
      }

      if (leave_flag == YES)
      {
         (void)fprintf(stdout, "\n\n");
         break;
      }
      else
      {
         (void)my_usleep(100000L);
      }
   } /* for (;;) */

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ menu() +++++++++++++++++++++++++++++++*/
static void
menu(int position)
{
   (void)fprintf(stdout, "\033[2J\033[3;1H"); /* Clear the screen (CLRSCR). */
   (void)fprintf(stdout, "\n\n                     FRA Editor (%s)\n\n", fra[position].dir_alias);
   (void)fprintf(stdout, "        +-----+------------------+----------------+\n");
   (void)fprintf(stdout, "        | Key | Description      | current value  |\n");
   (void)fprintf(stdout, "        +-----+------------------+----------------+\n");
   (void)fprintf(stdout, "        |  1  |files_in_dir      | %14u |\n", fra[position].files_in_dir);
#if SIZEOF_OFF_T == 4
   (void)fprintf(stdout, "        |  2  |bytes_in_dir      | %14ld |\n", (pri_off_t)fra[position].bytes_in_dir);
#else
   (void)fprintf(stdout, "        |  2  |bytes_in_dir      | %14lld |\n", (pri_off_t)fra[position].bytes_in_dir);
#endif
   (void)fprintf(stdout, "        |  3  |files_queued      | %14u |\n", fra[position].files_queued);
#if SIZEOF_OFF_T == 4
   (void)fprintf(stdout, "        |  4  |bytes_in_queue    | %14ld |\n", (pri_off_t)fra[position].bytes_in_queue);
#else
   (void)fprintf(stdout, "        |  4  |bytes_in_queue    | %14lld |\n", (pri_off_t)fra[position].bytes_in_queue);
#endif
   (void)fprintf(stdout, "        |  5  |error counter     | %14d |\n", fra[position].error_counter);
   (void)fprintf(stdout, "        |  6  |dir_flag          | %14u |\n", fra[position].dir_flag);
   (void)fprintf(stdout, "        |  7  |queued            | %14d |\n", (int)fra[position].queued);
   (void)fprintf(stdout, "        +-----+------------------+----------------+\n");

   return;
}


/*+++++++++++++++++++++++++++++++ get_key() +++++++++++++++++++++++++++++*/
static unsigned char
get_key(void)
{
   static unsigned char byte;

   if ((signal(SIGQUIT, sig_handler) == SIG_ERR) ||
       (signal(SIGINT, sig_handler) == SIG_ERR) ||
       (signal(SIGTSTP, sig_handler) == SIG_ERR) ||
       (signal(SIGALRM, sig_alarm) == SIG_ERR))
   {
      (void)fprintf(stderr, _("ERROR   : signal() error : %s (%s %d)\n"),
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (tcgetattr(STDIN_FILENO, &buf) < 0)
   {
      (void)fprintf(stderr, _("ERROR   : tcgetattr() error : %s (%s %d)\n"),
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   set = buf;

   set.c_lflag &= ~ICANON;
   set.c_lflag &= ~ECHO;
   set.c_cc[VMIN] = 1;
   set.c_cc[VTIME] = 0;

   if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &set) < 0)
   {
      (void)fprintf(stderr, _("ERROR   : tcsetattr() error : %s (%s %d)\n"),
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (setjmp(env_alarm) != 0)
   {
      if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &buf) < 0)
      {
         (void)fprintf(stderr, _("ERROR   : tcsetattr() error : %s (%s %d)\n"),
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      return(0);
   }
   alarm(5);
   if (read(STDIN_FILENO, &byte, 1) < 0)
   {
      (void)fprintf(stderr, _("ERROR   : read() error : %s (%s %d)\n"),
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   alarm(0);

   if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &buf) < 0)
   {
      (void)fprintf(stderr, _("ERROR   : tcsetattr() error : %s (%s %d)\n"),
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   return(byte);
}


/*------------------------------ sig_alarm() ----------------------------*/
static void
sig_alarm(int signo)
{
   longjmp(env_alarm, 1);
}


/*---------------------------- sig_handler() ----------------------------*/
static void
sig_handler(int signo)
{
   if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &buf) < 0)
   {
      (void)fprintf(stderr, _("ERROR   : tcsetattr() error : %s (%s %d)\n"),
                    strerror(errno), __FILE__, __LINE__);
   }
   exit(0);
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/ 
static void
usage(char *progname)
{
   (void)fprintf(stderr,
                 _("SYNTAX  : %s [-w working directory] dir_alias|position\n"),
                 progname);
   return;
}
