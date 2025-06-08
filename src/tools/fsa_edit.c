/*
 *  fsa_edit.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2025 Deutscher Wetterdienst (DWD),
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
 **   fsa_edit - changes certain values int the FSA
 **
 ** SYNOPSIS
 **   fsa_edit [working directory] hostname|position
 **
 ** DESCRIPTION
 **   So far this program can change the following values:
 **   total_file_counter (fc), total_file_size (fs) and error_counter
 **   (ec).
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   15.04.1996 H.Kiehl Created
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
                           fsa_fd = -1,
                           fsa_id,
                           no_of_hosts = 0;
#ifdef HAVE_MMAP
off_t                      fsa_size;
#endif
char                       *p_work_dir;
struct termios             buf,
                           set;
struct filetransfer_status *fsa;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ fsa_edit() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int          position = -1,
                leave_flag = NO,
                ret;
   unsigned int value;
   char         hostname[MAX_HOSTNAME_LENGTH + 1],
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
         t_hostname(argv[1], hostname);
      }
   }
   else
   {
      usage(argv[0]);
      exit(INCORRECT);
   }

   if ((ret = fsa_attach("fsa_edit")) != SUCCESS)
   {
      if (ret == INCORRECT_VERSION)
      {
         (void)fprintf(stderr,
                       _("ERROR   : This program is not able to attach to the FSA due to incorrect version. (%s %d)\n"),
                       __FILE__, __LINE__);
      }
      else
      {
         if (ret < 0)
         {
            (void)fprintf(stderr,
                          _("ERROR   : Failed to attach to FSA. (%s %d)\n"),
                          __FILE__, __LINE__);
         }
         else
         {
            (void)fprintf(stderr,
                          _("ERROR   : Failed to attach to FSA : %s (%s %d)\n"),
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
      if ((position = get_host_position(fsa, hostname, no_of_hosts)) < 0)
      {
         (void)fprintf(stderr,
                       _("ERROR   : Could not find host %s in FSA. (%s %d)\n"),
                       hostname, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   for (;;)
   {
      menu(position);

      switch (get_key())
      {
         
         case 0 : /* So we do not get wrong choice in first loop. */
            break;

         case '1' : /* total_file_counter */
            (void)fprintf(stderr, _("\n\n     Enter value [1] : "));
            if (scanf("%11u", &value) == EOF)
            {
               (void)fprintf(stderr,
                             _("ERROR   : scanf() error, failed to read input : %s (%s %d)\n"),
                             strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
            fsa[position].total_file_counter = (int)value;
            break;

         case '2' : /* total_file_size */
            (void)fprintf(stderr, _("\n\n     Enter value [2] : "));
            if (scanf("%11u", &value) == EOF)
            {
               (void)fprintf(stderr,
                             _("ERROR   : scanf() error, failed to read input : %s (%s %d)\n"),
                             strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
            fsa[position].total_file_size = value;
            break;

         case '3' : /* error counter */
            (void)fprintf(stderr, _("\n\n     Enter value [3] : "));
            if (scanf("%11u", &value) == EOF)
            {
               (void)fprintf(stderr,
                             _("ERROR   : scanf() error, failed to read input : %s (%s %d)\n"),
                             strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
            fsa[position].error_counter = value;
            break;

         case '4' : /* No. of connections */
            (void)fprintf(stderr, _("\n\n     Enter value [4] (0 - %d): "),
                          fsa[position].allowed_transfers);
            if (scanf("%11u", &value) == EOF)
            {
               (void)fprintf(stderr,
                             _("ERROR   : scanf() error, failed to read input : %s (%s %d)\n"),
                             strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
            if (value <= fsa[position].allowed_transfers)
            {
               fsa[position].connections = value;
            }
            else
            {
               (void)printf(_("Wrong choice!\n"));
               break;
            }
            break;

         case '5' : /* host status */
            (void)fprintf(stdout, "\033[2J\033[3;1H");
            (void)fprintf(stdout, "\n\n\n");
            (void)fprintf(stdout, "     Start/Stop queue [%d]..........(1)\n",
                          (fsa[position].host_status & PAUSE_QUEUE_STAT) ? 1 : 0);
            (void)fprintf(stdout, "     Start/Stop transfer [%d].......(2)\n",
                          (fsa[position].host_status & STOP_TRANSFER_STAT) ? 1 : 0);
            (void)fprintf(stdout, "     Start/Stop auto queue [%d].....(3)\n",
                          (fsa[position].host_status & AUTO_PAUSE_QUEUE_STAT) ? 1 : 0);
            (void)fprintf(stdout, "     Start/Stop danger queue [%d]...(4)\n",
                          (fsa[position].host_status & DANGER_PAUSE_QUEUE_STAT) ? 1 : 0);
#ifdef WITH_ERROR_QUEUE
            (void)fprintf(stdout, "     Set/Unset error queue flag [%d](5)\n",
                          (fsa[position].host_status & ERROR_QUEUE_SET) ? 1 : 0);
#endif
            (void)fprintf(stdout, "     HOST_CONFIG host disabled [%d].(6)\n",
                          (fsa[position].host_status & HOST_CONFIG_HOST_DISABLED) ? 1 : 0);
            (void)fprintf(stdout, "     Pending errors [%d]............(7)\n",
                          (fsa[position].host_status & PENDING_ERRORS) ? 1 : 0);
            (void)fprintf(stdout, "     Host errors ackn [%d]..........(8)\n",
                          (fsa[position].host_status & HOST_ERROR_ACKNOWLEDGED) ? 1 : 0);
            (void)fprintf(stdout, "     Host errors offline [%d].......(9)\n",
                          (fsa[position].host_status & HOST_ERROR_OFFLINE) ? 1 : 0);
            (void)fprintf(stdout, "     Host errors ackn time [%d].....(a)\n",
                          (fsa[position].host_status & HOST_ERROR_ACKNOWLEDGED_T) ? 1 : 0);
            (void)fprintf(stdout, "     Host errors offline time [%d]..(b)\n",
                          (fsa[position].host_status & HOST_ERROR_OFFLINE_T) ? 1 : 0);
            (void)fprintf(stdout, "     Reset integer value to 0 [%d]..(c)\n",
                          fsa[position].host_status);
            (void)fprintf(stderr, "     None..........................(d) ");
#ifdef LOCK_DEBUG
            lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
            lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
            switch (get_key())
            {
               case '1' : fsa[position].host_status ^= PAUSE_QUEUE_STAT;
                          break;
               case '2' : fsa[position].host_status ^= STOP_TRANSFER_STAT;
                          break;
               case '3' : fsa[position].host_status ^= AUTO_PAUSE_QUEUE_STAT;
                          break;
               case '4' : fsa[position].host_status ^= DANGER_PAUSE_QUEUE_STAT;
                          break;
#ifdef WITH_ERROR_QUEUE
               case '5' : fsa[position].host_status ^= ERROR_QUEUE_SET;
                                  break;
#endif
               case '6' : fsa[position].host_status ^= HOST_CONFIG_HOST_DISABLED;
                          break;
               case '7' : fsa[position].host_status ^= PENDING_ERRORS;
                          break;
               case '8' : fsa[position].host_status ^= HOST_ERROR_ACKNOWLEDGED;
                          break;
               case '9' : fsa[position].host_status ^= HOST_ERROR_OFFLINE;
                          break;
               case 'a' : fsa[position].host_status ^= HOST_ERROR_ACKNOWLEDGED_T;
                          break;
               case 'b' : fsa[position].host_status ^= HOST_ERROR_OFFLINE_T;
                          break;
               case 'c' : fsa[position].host_status = 0;
                          break;
               case 'd' : break;
               default  : (void)printf(_("Wrong choice!\n"));
                          (void)sleep(1);
                          break;
            }
#ifdef LOCK_DEBUG
            unlock_region(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
            unlock_region(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
            break;

         case '6' : /* Max. errors */
            (void)fprintf(stderr, _("\n\n     Enter value [6] : "));
            if (scanf("%11u", &value) == EOF)
            {
               (void)fprintf(stderr,
                             _("ERROR   : scanf() error, failed to read input : %s (%s %d)\n"),
                             strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
            fsa[position].max_errors = value;
            break;

         case '7' : /* Block size */
            (void)fprintf(stderr, _("\n\n     Enter value [7] : "));
            if (scanf("%11u", &value) == EOF)
            {
               (void)fprintf(stderr,
                             _("ERROR   : scanf() error, failed to read input : %s (%s %d)\n"),
                             strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
            fsa[position].block_size = value;
            break;

         case '8' : /* Allowed transfers */
            (void)fprintf(stderr, _("\n\n     Enter value [8] (1 - %d): "), MAX_NO_PARALLEL_JOBS);
            if (scanf("%11u", &value) == EOF)
            {
               (void)fprintf(stderr,
                             _("ERROR   : scanf() error, failed to read input : %s (%s %d)\n"),
                             strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
            if ((value > 0) && (value <= MAX_NO_PARALLEL_JOBS))
            {
               fsa[position].allowed_transfers = value;
            }
            else
            {
               (void)printf(_("Wrong choice!\n"));
               (void)sleep(1);
            }
            break;

         case '9' : /* Transfer timeout */
            (void)fprintf(stderr, _("\n\n     Enter value [9] : "));
            if (scanf("%11u", &value) == EOF)
            {
               (void)fprintf(stderr,
                             _("ERROR   : scanf() error, failed to read input : %s (%s %d)\n"),
                             strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
            fsa[position].transfer_timeout = value;
            break;

         case 'a' : /* Real hostname */
            if (fsa[position].real_hostname[0][0] != GROUP_IDENTIFIER)
            {
               char buffer[256];

               (void)fprintf(stderr, _("\n\n     Enter hostname  : "));
               buffer[0] = '\0';
               if (scanf("%255s", buffer) == EOF)
               {
                  (void)fprintf(stderr,
                                _("ERROR   : scanf() error, failed to read input : %s (%s %d)\n"),
                                strerror(errno), __FILE__, __LINE__);
                  exit(INCORRECT);
               }
               (void)memcpy(fsa[position].real_hostname[0], buffer,
                            MAX_REAL_HOSTNAME_LENGTH);
               fsa[position].host_dsp_name[MAX_HOSTNAME_LENGTH - 1] = '\0';
            }
            break;

         case 'b' : /* Host display name */
            {
               char buffer[256];

               (void)fprintf(stderr, _("\n\nEnter hostdisplayname: "));
               buffer[0] = '\0';
               if (scanf("%255s", buffer) == EOF)
               {
                  (void)fprintf(stderr,
                                _("ERROR   : scanf() error, failed to read input : %s (%s %d)\n"),
                                strerror(errno), __FILE__, __LINE__);
                  exit(INCORRECT);
               }
               (void)memcpy(fsa[position].host_dsp_name, buffer,
                            MAX_HOSTNAME_LENGTH + 2);
               fsa[position].host_dsp_name[MAX_HOSTNAME_LENGTH + 1] = '\0';
            }
            break;

         case 'c' :  /* Error offline stat */
#ifdef LOCK_DEBUG
            lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
            lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
            fsa[position].host_status ^= HOST_ERROR_OFFLINE_STATIC;
#ifdef LOCK_DEBUG
            unlock_region(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
            unlock_region(fsa_fd, (AFD_WORD_OFFSET + (position * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
            break;

         case 'd' : /* Active transfers */
            (void)fprintf(stderr, _("\n\n     Enter value [d] : "));
            if (scanf("%11u", &value) == EOF)
            {
               (void)fprintf(stderr,
                             _("ERROR   : scanf() error, failed to read input : %s (%s %d)\n"),
                             strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
            if (value > MAX_NO_PARALLEL_JOBS)
            {
               (void)printf(_("The value must be between 0 and %d!\n"), MAX_NO_PARALLEL_JOBS);
               (void)sleep(1);
            }
            else
            {
               fsa[position].active_transfers = value;
            }
            break;

         case 'e' : /* File name */
            {
               char buffer[MAX_FILENAME_LENGTH];

               (void)fprintf(stderr, _("\n\n     Enter value [e] : "));
               buffer[0] = '\0';
               if (scanf("%255s", buffer) == EOF)
               {
                  (void)fprintf(stderr,
                                _("ERROR   : scanf() error, failed to read input : %s (%s %d)\n"),
                                strerror(errno), __FILE__, __LINE__);
                  exit(INCORRECT);
               }
               (void)memcpy(fsa[position].job_status[0].file_name_in_use,
                            buffer, MAX_FILENAME_LENGTH);
               fsa[position].job_status[0].file_name_in_use[MAX_FILENAME_LENGTH - 1] = '\0';
            }
            break;

         case 'f' : /* Jobs queued */
            (void)fprintf(stderr, _("\n\n     Enter value [f] : "));
            if (scanf("%11u", &value) == EOF)
            {
               (void)fprintf(stderr,
                             _("ERROR   : scanf() error, failed to read input : %s (%s %d)\n"),
                             strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
            fsa[position].jobs_queued = value;
            break;

         case 'g' : /* Transferrate limit */
            (void)fprintf(stderr, _("\n\n     Enter value [g] : "));
            if (scanf("%11u", &value) == EOF)
            {
               (void)fprintf(stderr,
                             _("ERROR   : scanf() error, failed to read input : %s (%s %d)\n"),
                             strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
            fsa[position].transfer_rate_limit = value;
            break;

         case 'h' : /* Original toggle */
            if ((fsa[position].auto_toggle == ON) &&
                (fsa[position].original_toggle_pos != NONE))
            {
               if (fsa[position].original_toggle_pos == HOST_ONE)
               {
                  fsa[position].original_toggle_pos = NONE;
               }
               else if (fsa[position].original_toggle_pos == HOST_TWO)
                    {
                       fsa[position].original_toggle_pos = NONE;
                    }
            }
            break;

         case 'x' :
         case 'Q' :
         case 'q' :
            leave_flag = YES;
            break;

         default  :
            (void)printf(_("Wrong choice!\n"));
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
   (void)fprintf(stdout, "\n\n                     FSA Editor (%s)\n\n", fsa[position].host_dsp_name);
   (void)fprintf(stdout, "        +-----+------------------+----------------+\n");
   (void)fprintf(stdout, "        | Key | Description      | current value  |\n");
   (void)fprintf(stdout, "        +-----+------------------+----------------+\n");
   (void)fprintf(stdout, "        |  1  |total_file_counter| %14u |\n", fsa[position].total_file_counter);
#if SIZEOF_OFF_T == 4
   (void)fprintf(stdout, "        |  2  |total_file_size   | %14ld |\n", (pri_off_t)fsa[position].total_file_size);
#else
   (void)fprintf(stdout, "        |  2  |total_file_size   | %14lld |\n", (pri_off_t)fsa[position].total_file_size);
#endif
   (void)fprintf(stdout, "        |  3  |error counter     | %14d |\n", fsa[position].error_counter);
   (void)fprintf(stdout, "        |  4  |No. of connections| %14d |\n", fsa[position].connections);
   (void)fprintf(stdout, "        |  5  |host status       | %14d |\n", fsa[position].host_status);
   (void)fprintf(stdout, "        |  6  |Max. errors       | %14d |\n", fsa[position].max_errors);
   (void)fprintf(stdout, "        |  7  |Block size        | %14d |\n", fsa[position].block_size);
   (void)fprintf(stdout, "        |  8  |Allowed transfers | %14d |\n", fsa[position].allowed_transfers);
   (void)fprintf(stdout, "        |  9  |Transfer timeout  | %14ld |\n", fsa[position].transfer_timeout);
   if (fsa[position].real_hostname[0][0] != GROUP_IDENTIFIER)
   {
      (void)fprintf(stdout, "        |  a  |Real hostname     | %14s |\n", fsa[position].real_hostname[0]);
   }
   (void)fprintf(stdout, "        |  b  |Host display name | %14s |\n", fsa[position].host_dsp_name);
   (void)fprintf(stdout, "        |  c  |Error offline stat| %14s |\n", (fsa[position].host_status & HOST_ERROR_OFFLINE_STATIC) ? "Yes" : "No");
   (void)fprintf(stdout, "        |  d  |Active transfers  | %14d |\n", fsa[position].active_transfers);
   (void)fprintf(stdout, "        |  e  |File name         | %14s |\n", fsa[position].job_status[0].file_name_in_use);
   (void)fprintf(stdout, "        |  f  |Jobs queued       | %14u |\n", fsa[position].jobs_queued);
#if SIZEOF_OFF_T == 4
   (void)fprintf(stdout, "        |  g  |Transferrate limit| %14ld |\n", (pri_off_t)fsa[position].transfer_rate_limit);
#else
   (void)fprintf(stdout, "        |  g  |Transferrate limit| %14lld |\n", (pri_off_t)fsa[position].transfer_rate_limit);
#endif
   if ((fsa[position].auto_toggle == ON) &&
       (fsa[position].original_toggle_pos != NONE))
   {
      (void)fprintf(stdout, "        |  h  |Original toggle   | %14s |\n", (fsa[position].original_toggle_pos == HOST_ONE) ? "HOST_ONE" : "HOST_TWO");
   }
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
                 _("SYNTAX  : %s [-w working directory] hostname|position\n"),
                 progname);
   return;
}
