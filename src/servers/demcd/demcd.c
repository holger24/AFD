/*
 *  demcd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2015 - 2020 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   demcd - De Mail Confirmation Daemon
 **
 ** SYNOPSIS
 **   demcd [--version][-w <AFD working directory>]
 **
 ** DESCRIPTION
 **   demcd is a daemon that reads the mail file and searches for De Mail
 **   comfirmation messages.
 **
 ** RETURN VALUES
 **   Will exit with INCORRECT when some system call failed.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.09.2015 H.Kiehl Created
 */
DESCR__E_M1

#include <stdio.h>            /* fprintf()                               */
#include <string.h>           /* memset()                                */
#include <stdlib.h>           /* atoi(), getenv(), malloc()              */
#include <ctype.h>            /* isdigit()                               */
#include <time.h>             /* clock_t                                 */
#include <signal.h>           /* signal()                                */
#include <sys/time.h>
#ifdef HAVE_SETPRIORITY
# include <sys/resource.h>    /* setpriority()                           */
#endif
#include <sys/types.h>
#include <sys/wait.h>         /* waitpid()                               */
#include <unistd.h>           /* close(), select(), sysconf(), pathconf()*/
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "demcddefs.h"
#include "version.h"

/* Global variables. */
int                    dqb_fd = -1,
                       *no_demcd_queued,
                       sys_log_fd = STDERR_FILENO;
char                   afd_config_file[MAX_PATH_LENGTH + ETC_DIR_LENGTH + AFD_CONFIG_FILE_LENGTH],
                       *p_work_dir,
                       *p_work_dir_end;
struct afd_status      *p_afd_status;
struct demcd_queue_buf *dqb = NULL;
const char             *sys_log_name = SYSTEM_LOG_FIFO;
#ifdef _OUTPUT_LOG
int                    ol_fd = -2;
# ifdef WITHOUT_FIFO_RW_SUPPORT
int                    ol_readfd = -2;
# endif
unsigned int           *ol_job_number,
                       *ol_retries;
char                   *ol_data = NULL,
                       *ol_file_name,
                       *ol_output_type;
unsigned short         *ol_archive_name_length,
                       *ol_file_name_length,
                       *ol_unl;
off_t                  *ol_file_size;
size_t                 ol_size,
                       ol_real_size;
clock_t                *ol_transfer_time;
#endif /* _OUTPUT_LOG */

/* Local function prototypes. */
static void            demcd_exit(void),
                       get_demcd_config_value(char *, long *),
                       sig_bus(int),
                       sig_exit(int),
                       sig_segv(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int            bytes_buffered = 0,
                  check_size,
                  demcd_fd,
                  i,
                  mail_fd,
                  length,
                  n,
                  no_of_buffered_writes = 0,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                  read_demcd_writefd,
#endif
                  status;
   unsigned int   *job_number;
   long           fifo_size,
                  time_up;
   off_t          *file_size;
   time_t         now;
   dev_t          inode_number;
   char           demcd_fifo[MAX_PATH_LENGTH + FIFO_DIR_LENGTH + DEMCD_FIFO_LENGTH],
                  *fifo_buffer,
                  line[MAX_LINE_LENGTH],
                  mail_file[MAX_PATH_LENGTH],
                  *p_host_name,
                  *p_file_name,
                  *ptr,
                  work_dir[MAX_PATH_LENGTH];
   unsigned char  *confirmation_type;
   unsigned short *file_name_length,
                  *unl;
   fd_set         rset;
   struct timeval timeout;
   struct stat    stat_buf;
   FILE           *mail_fp;

   CHECK_FOR_VERSION(argc, argv);

   /* Initialize variables. */
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;
   p_work_dir_end = work_dir + strlen(work_dir);
   get_demcd_config_value(mail_file, &time_up);

   /* Open (create) fifo where we get our jobs. */
   (void)sprintf(demcd_fifo, "%s%s%s", work_dir, FIFO_DIR, DEMCD_FIFO);
   if ((stat(demcd_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(demcd_fifo) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not create fifo %s.", demcd_fifo);
         exit(INCORRECT);
      }
   }
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(demcd_fifo, &demcd_fd, &read_demcd_writefd) == -1)
#else 
   if ((demcd_fd = coe_open(demcd_fifo, O_RDWR)) == -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s", demcd_fifo, strerror(errno));
      exit(INCORRECT);
   }

   /*
    * Lets determine the largest offset so the 'structure'
    * is aligned correctly.
    */
   n = sizeof(off_t);
   if (sizeof(unsigned int) > n)
   {
      n = sizeof(unsigned int);
   }

   /*
    * Determine the size of the fifo buffer. Then create a buffer
    * large enough to hold the data from a fifo.
    */
   if ((fifo_size = fpathconf(demcd_fd, _PC_PIPE_BUF)) < 0)
   {
      /* If we cannot determine the size of the fifo set default value. */
      fifo_size = DEFAULT_FIFO_SIZE;
   }
   if (fifo_size < (n + n + MAX_HOSTNAME_LENGTH +
                    1 + sizeof(unsigned short) +
                    sizeof(unsigned short) + MAX_FILENAME_LENGTH + 1))
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Fifo is NOT large enough to ensure atomic writes!");
      fifo_size = n + n + MAX_HOSTNAME_LENGTH +
                  1 + sizeof(unsigned short) +
                  sizeof(unsigned short) + MAX_FILENAME_LENGTH + 1;
   }

   /* Now lets allocate memory for the fifo buffer. */
   if ((fifo_buffer = malloc((size_t)fifo_size)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not allocate memory for the fifo buffer : %s",
                 strerror(errno));
      exit(INCORRECT);
   }

   /* Open mail file where we read the confirmations. */
   if (mail_file[0] == '\0')
   {
      mail_fp = NULL;
      mail_fd = -1;
      inode_number = 0;
   }
   else
   {
      if ((mail_fp = fopen(mail_file, "r")) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not fopen() mail file %s : %s",
                    mail_file, strerror(errno));
         exit(INCORRECT);
      }
      mail_fd = fileno(mail_fp);
      if (fstat(mail_fd, &stat_buf) == -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not fstat() mail file %s : %s",
                    mail_file, strerror(errno));
         exit(INCORRECT);
      }
      inode_number = stat_buf.st_ino;
   }

   /* Do some cleanups when we exit. */
   if (atexit(demcd_exit) != 0)          
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("Could not register exit handler : %s"), strerror(errno));
      exit(INCORRECT);
   }
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGPIPE, SIG_IGN) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("Could not set signal handlers : %s"), strerror(errno));
      exit(INCORRECT);
   }

   if ((ptr = lock_proc(DEMCD_LOCK_ID, NO)) != NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Process DEMCD already started by %s"), ptr);
      (void)fprintf(stderr,
                    _("Process DEMCD already started by %s : (%s %d)\n"),
                    ptr, __FILE__, __LINE__);
      _exit(INCORRECT);
   }

   /* Position pointers in fifo so that we only need to read */
   /* the data as they are in the fifo.                      */
   file_size = (off_t *)fifo_buffer;
   job_number = (unsigned int *)(fifo_buffer + n);
   unl = (unsigned short *)(fifo_buffer + n + n);
   file_name_length = (unsigned short *)(fifo_buffer + n + n +
                                         sizeof(unsigned short));
   confirmation_type = (unsigned char *)(fifo_buffer + n + n +
                                         sizeof(unsigned short) +
                                         sizeof(unsigned short));
   p_host_name = (char *)(fifo_buffer + n + n +
                          sizeof(unsigned short) + sizeof(unsigned short) +
                          sizeof(unsigned char));
   p_file_name = (char *)(fifo_buffer + n + n +
                          sizeof(unsigned short) + sizeof(unsigned short) +
                          sizeof(unsigned char) + MAX_HOSTNAME_LENGTH + 1);
   check_size = n + n +
                sizeof(unsigned short) + sizeof(unsigned short) +
                sizeof(unsigned char) + MAX_HOSTNAME_LENGTH + 1 + 1;

   if (dqb_fd == -1)
   {
      size_t new_size = (DEMCD_QUE_BUF_SIZE * sizeof(struct demcd_queue_buf)) +
                        AFD_WORD_OFFSET;
      char   fullname[MAX_PATH_LENGTH];

      (void)snprintf(fullname, MAX_PATH_LENGTH, "%s%s%s",
                     p_work_dir, FIFO_DIR, DEMCD_QUEUE_FILE);
      if ((ptr = attach_buf(fullname, &dqb_fd, &new_size, "DEMCD",
                            FILE_MODE, NO)) == (caddr_t) -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to mmap() `%s' : %s", fullname, strerror(errno));
         exit(INCORRECT);
      }
      no_demcd_queued = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      dqb = (struct demcd_queue_buf *)ptr;
   }

   /* Attach to the AFD Status Area. */
   if (attach_afd_status(NULL, WAIT_AFD_STATUS_ATTACH) < 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("Failed to map to AFD status area."));
      exit(INCORRECT);
   }

   system_log(INFO_SIGN, NULL, 0, _("Starting %s monitoring %s (%s)"),
              DEMCD, mail_file, PACKAGE_VERSION);
   FD_ZERO(&rset);

   for (;;)
   {
      FD_SET(demcd_fd, &rset);
      timeout.tv_usec = 100000L;
      timeout.tv_sec = 0L;

      /* Wait for message x seconds and then see if new confirmation arrived. */
      status = select(demcd_fd + 1, &rset, NULL, NULL, &timeout);

      /* Timeout, lets check if a new confirmation message arrived. */
      if (status == 0)
      {
         if ((*no_demcd_queued > 0) && (mail_file[0] != '\0'))
         {
            if (fstat(mail_fd, &stat_buf) == -1)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "Could not fstat() mail file %s : %s",
                          mail_file, strerror(errno));
               exit(INCORRECT);
            }
            else
            {
               if (stat_buf.st_ino != inode_number)
               {
                  if (fclose(mail_fp) == EOF)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Could not fclose() mail file %s after inode change : %s",
                                mail_file, strerror(errno));
                  }
                  if ((mail_fp = fopen(mail_file, "r")) == NULL)
                  {
                     system_log(FATAL_SIGN, __FILE__, __LINE__,
                                "Could not fopen() mail file %s after inode change : %s",
                                mail_file, strerror(errno));
                     exit(INCORRECT);
                  }
                  mail_fd = fileno(mail_fp);
                  inode_number = stat_buf.st_ino;
               }
               while (fgets(line, MAX_LINE_LENGTH, mail_fp) != NULL)
               {
                  check_line(line);
               }
            }
         }

         /* We cannot keep the jobs queued forever. So lets remove */
         /* them once they have reached a certain age.             */
         now = time(NULL);
         for (i = 0; i < *no_demcd_queued; i++)
         {
            if ((now - dqb[i].log_time) >= time_up)
            {
               log_confirmation(i, CL_TIMEUP);
               if (i <= (*no_demcd_queued - 1))
               {
                  (void)memmove(&dqb[i], &dqb[i + 1],
                                ((*no_demcd_queued - 1 - i) * sizeof(struct demcd_queue_buf)));
               }
               (*no_demcd_queued)--;
               if (i < *no_demcd_queued)
               {
                  i--;
               }
            }
         }
      }
           /* A new job arrived. */
      else if (FD_ISSET(demcd_fd, &rset))
           {
              if ((n = read(demcd_fd, &fifo_buffer[bytes_buffered],
                            fifo_size - bytes_buffered)) > 0)
              {
                 n += bytes_buffered;
                 bytes_buffered = 0;
                 now = time(NULL);
                 do
                 {
                    if ((n < (check_size - 1)) ||
                        (n < (check_size + *file_name_length)))
                    {
                       length = n;
                       bytes_buffered = n;
                    }
                    else
                    {
                       check_demcd_queue_space();
                       (void)memcpy(dqb[*no_demcd_queued].de_mail_privat_id,
                                    p_file_name, *unl);
                       dqb[*no_demcd_queued].de_mail_privat_id[*unl] = '\0';
                       (void)strcpy(dqb[*no_demcd_queued].file_name,
                                    p_file_name + *unl);
                       (void)strcpy(dqb[*no_demcd_queued].alias_name,
                                    p_host_name);
                       dqb[*no_demcd_queued].log_time = now;
                       dqb[*no_demcd_queued].file_size = *file_size;
                       dqb[*no_demcd_queued].jid = *job_number;
                       dqb[*no_demcd_queued].confirmation_type = *confirmation_type;
                       (*no_demcd_queued)++;
                    }
                    length = check_size + *file_name_length;
                    n -= length;
                    if (n > 0)
                    {
                       (void)memmove(fifo_buffer, &fifo_buffer[length], n);
                    }
                    no_of_buffered_writes++;
                 } while (n > 0);
              }
              else if (n < 0)
                   {
                      system_log(FATAL_SIGN, __FILE__, __LINE__,
                                 "read() error (%d) : %s", n, strerror(errno));
                      exit(INCORRECT);
                   }
           }
      else if (status == -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Select error : %s", strerror(errno));
              exit(INCORRECT);
           }
   } /* for (;;) */

   /* Should never come to this point. */
   exit(SUCCESS);
}


/*+++++++++++++++++++++++ get_demcd_config_value() ++++++++++++++++++++++*/
static void
get_demcd_config_value(char *mail_file, long *time_up)
{
   char *buffer = NULL;

   (void)snprintf(afd_config_file,
                  MAX_PATH_LENGTH + ETC_DIR_LENGTH + AFD_CONFIG_FILE_LENGTH,
                  "%s%s%s", p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   if ((eaccess(afd_config_file, F_OK) == 0) &&
       (read_file_no_cr(afd_config_file, &buffer, YES,
                        __FILE__, __LINE__) != INCORRECT))
   {
      char value[MAX_LONG_LENGTH];

#ifdef HAVE_SETPRIORITY
      if (get_definition(buffer, DEMCD_PRIORITY_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         if (setpriority(PRIO_PROCESS, 0, atoi(value)) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to set priority to %d : %s",
                       atoi(value), strerror(errno));
         }
      }
#endif
      if (get_definition(buffer, DEFAULT_DE_MAIL_CONF_TIMEUP_DEF,
                         value, MAX_LONG_LENGTH) != NULL)
      {
         *time_up = atol(value);
      }
      else
      {
         *time_up = DEFAULT_DE_MAIL_CONF_TIMEUP;
      }
      if (get_definition(buffer, DE_MAIL_RESPONSE_FILE_DEF,
                         mail_file, MAX_PATH_LENGTH) == NULL)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("No %s defined in AFD_CONFIG."),
                    DE_MAIL_RESPONSE_FILE_DEF);
         mail_file[0] = '\0';
      }
   }
   else
   {
      *time_up = DEFAULT_DE_MAIL_CONF_TIMEUP;
   }
   free(buffer);

   return;
}


/*++++++++++++++++++++++++++++++ demcd_exit() +++++++++++++++++++++++++++*/
static void
demcd_exit(void)
{
   system_log(INFO_SIGN, __FILE__, __LINE__, "%s terminating.", DEMCD);
   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   system_log(FATAL_SIGN, __FILE__, __LINE__,
              _("Aaarrrggh! Received SIGSEGV."));
   demcd_exit();

   /* Dump core so we know what happened. */
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   system_log(FATAL_SIGN, __FILE__, __LINE__, _("Uuurrrggh! Received SIGBUS."));
   demcd_exit();

   /* Dump core so we know what happened. */
   abort();
}


/*+++++++++++++++++++++++++++++++ sig_exit() ++++++++++++++++++++++++++++*/
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
                 DEMCD, signo, (pri_pid_t)getpid());
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
