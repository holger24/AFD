/*
 *  mon.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2022 Deutscher Wetterdienst (DWD),
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
 **   mon - monitor process that watches one AFD
 **
 ** SYNOPSIS
 **   mon [--version] [-w <working directory>] AFD-number
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   31.08.1998 H.Kiehl Created
 **   05.06.1999 H.Kiehl Added remote host list.
 **   03.09.2000 H.Kiehl Addition of log history.
 **   13.09.2000 H.Kiehl Addition of top number of process.
 **   30.07.2003 H.Kiehl Added number of jobs configured.
 **   03.12.2003 H.Kiehl Added connection and disconnection time.
 **   26.06.2004 H.Kiehl Store error host list.
 **   27.02.2005 H.Kiehl Option to switch between two AFD's.
 **   30.07.2006 H.Kiehl Handle total values for input and output.
 **
 */
DESCR__E_M1

#include <stdio.h>           /* fprintf()                                */
#include <stdarg.h>          /* va_start(), va_end()                     */
#include <string.h>          /* strcpy(), strcat(), strerror()           */
#include <stdlib.h>          /* atexit(), exit()                         */
#include <signal.h>          /* signal()                                 */
#include <ctype.h>           /* isdigit()                                */
#include <time.h>            /* time()                                   */
#ifdef WITH_SSL
# include <openssl/ssl.h>
#endif
#include <sys/types.h>
#include <sys/time.h>        /* struct timeval                           */
#include <sys/stat.h>
#include <sys/mman.h>        /* munmap()                                 */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <unistd.h>
#include <errno.h>
#include "mondefs.h"
#include "logdefs.h"
#include "afdd_common_defs.h"
#include "version.h"


/* Global variables. */
int                      afd_no,
                         got_log_capabilities,
                         mon_log_fd = STDERR_FILENO,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                         mon_log_readfd,
#endif
                         msa_fd = -1,
                         msa_id,
                         no_of_afds,
                         shift_log_his[NO_OF_LOG_HISTORY],
                         sock_fd,
                         sys_log_fd = STDERR_FILENO,
                         timeout_flag;
#ifdef WITH_SSL
SSL                      *ssl_con = NULL;
#endif
time_t                   new_hour_time;
size_t                   adl_size,
                         ahl_size,
                         ajl_size,
                         atd_size;
off_t                    msa_size;
long                     tcp_timeout = 120L;
char                     msg_str[MAX_RET_MSG_LENGTH],
                         *p_mon_alias,
                         *p_work_dir;
struct mon_status_area   *msa;
struct afd_dir_list      *adl = NULL;
struct afd_host_list     *ahl = NULL;
struct afd_job_list      *ajl = NULL;
struct afd_typesize_data *atd = NULL;
const char               *sys_log_name = MON_SYS_LOG_FIFO;

/* Local global variables. */
static int               send_log_request;

/* Local functions prototypes */
static int               send_got_log_capabilities(int),
                         tcp_cmd(char *, ...);
static void              mon_exit(void),
                         sig_bus(int),
                         sig_exit(int),
                         sig_segv(int);

/* #define _DEBUG_PRINT 1 */


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ mon() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int            bytes_done = 0,
                  bytes_buffered = 0,
                  i,
                  retry_fd,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                  retry_writefd,
#endif
                  status;
   time_t         new_day_time,
                  now,
                  retry_interval = RETRY_INTERVAL,
                  start_time;
   char           mon_log_fifo[MAX_PATH_LENGTH],
                  retry_fifo[MAX_PATH_LENGTH],
                  work_dir[MAX_PATH_LENGTH];
   fd_set         rset;
#ifdef HAVE_STATX
   struct statx   stat_buf;
#else
   struct stat    stat_buf;
#endif
   struct timeval timeout;

   CHECK_FOR_VERSION(argc, argv);

   if (get_mon_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if (argc != 2)
   {
      (void)fprintf(stderr,
                    "Usage: %s [-w working directory] AFD-number\n", argv[0]);
      exit(MON_SYNTAX_ERROR);
   }
   else
   {
      char *ptr = argv[1];

      while (*ptr != '\0')
      {
         if (isdigit((int)(*ptr)) == 0)
         {
            (void)fprintf(stderr,
                          "Usage: %s [-w working directory] AFD-number\n",
                          argv[0]);
            exit(MON_SYNTAX_ERROR);
         }
         ptr++;
      }
      afd_no = atoi(argv[1]);
   }

   /* Initialize variables. */
   (void)strcpy(mon_log_fifo, p_work_dir);
   (void)strcat(mon_log_fifo, FIFO_DIR);
   (void)strcpy(retry_fifo, mon_log_fifo);
   (void)strcat(retry_fifo, RETRY_MON_FIFO);
   (void)strcat(retry_fifo, argv[1]);
   (void)strcat(mon_log_fifo, MON_LOG_FIFO);

   /* Open (create) monitor log fifo. */
#ifdef HAVE_STATX
   if ((statx(0, mon_log_fifo, AT_STATX_SYNC_AS_STAT,
              STATX_MODE, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.stx_mode)))
#else
   if ((stat(mon_log_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
#endif
   {
      if (make_fifo(mon_log_fifo) < 0)
      {
         (void)fprintf(stderr,
                       "ERROR   : Could not create fifo %s. (%s %d)\n",
                       mon_log_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(mon_log_fifo, &mon_log_readfd, &mon_log_fd) == -1)
#else
   if ((mon_log_fd = open(mon_log_fifo, O_RDWR)) == -1)
#endif
   {
      (void)fprintf(stderr, "ERROR   : Could not open() fifo %s : %s (%s %d)\n",
                    mon_log_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Open (create) retry fifo. */
#ifdef HAVE_STATX
   if ((statx(0, retry_fifo, AT_STATX_SYNC_AS_STAT,
              STATX_MODE, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.stx_mode)))
#else
   if ((stat(retry_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
#endif
   {
      if (make_fifo(retry_fifo) < 0)
      {
         (void)fprintf(stderr,
                       "ERROR   : Could not create fifo %s. (%s %d)\n",
                       retry_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(retry_fifo, &retry_fd, &retry_writefd) == -1)
#else
   if ((retry_fd = open(retry_fifo, O_RDWR)) == -1)
#endif
   {
      (void)fprintf(stderr,
                    "ERROR   : Could not open() fifo %s : %s (%s %d)\n",
                    retry_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Do some cleanups when we exit. */
   if (atexit(mon_exit) != 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not register exit handler : %s", strerror(errno));
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
                 "Could not set signal handlers : %s", strerror(errno));
      exit(INCORRECT);
   }

   if (msa_attach() != SUCCESS)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__, "Failed to attach to MSA.");
      exit(INCORRECT);
   }
   msa[afd_no].tr = 0;
   p_mon_alias = msa[afd_no].afd_alias;
   now = time(NULL);
   new_day_time = ((now / 86400) * 86400) + 86400;
   new_hour_time = ((now / 3600) * 3600) + 3600;
   for (i = 0; i < NO_OF_LOG_HISTORY; i++)
   {
      shift_log_his[i] = DONE;
   }

   for (;;)
   {
      msa[afd_no].connect_status = CONNECTING;
      timeout_flag = OFF;
      if ((status = tcp_connect(msa[afd_no].hostname[(int)msa[afd_no].afd_toggle],
                                msa[afd_no].port[(int)msa[afd_no].afd_toggle],
                                NO
#ifdef WITH_SSL
                                , msa[afd_no].options & ENABLE_TLS_ENCRYPTION
#endif
                                )) != SUCCESS)
      {
         if (timeout_flag == OFF)
         {
            if (status != INCORRECT)
            {
               mon_log(WARN_SIGN, NULL, 0, 0L, msg_str, "Failed to connect.");
            }

            /*
             * The tcp_connect() function wrote to the log file if status
             * is INCORRECT. Thus it is not necessary to do it here again.
             */
         }
         else
         {
            mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                    "Failed to connect due to timeout.");
         }
         msa[afd_no].connect_status = CONNECTION_DEFUNCT;
         retry_interval = RETRY_INTERVAL;

         if (msa[afd_no].afd_switching == AUTO_SWITCHING)
         {
            if (msa[afd_no].afd_toggle == (HOST_ONE - 1))
            {
               msa[afd_no].afd_toggle = (HOST_TWO - 1);
            }
            else
            {
               msa[afd_no].afd_toggle = (HOST_ONE - 1);
            }
            mon_log(WARN_SIGN, NULL, 0, 0L, NULL,
                    "Automatic switching to %s%d (%s at port %d).",
                    msa[afd_no].afd_alias, (int)msa[afd_no].afd_toggle + 1,
                    msa[afd_no].hostname[(int)msa[afd_no].afd_toggle],
                    msa[afd_no].port[(int)msa[afd_no].afd_toggle]);
         }
      }
      else
      {
         char current_afd_toggle;

         got_log_capabilities = NO;
         send_log_request = NO;
         if (msa[afd_no].afd_switching != NO_SWITCHING)
         {
            current_afd_toggle = msa[afd_no].afd_toggle;
         }
         else
         {
            current_afd_toggle = 0; /* Silence compiler. */
         }
         msa[afd_no].connect_status = CONNECTION_ESTABLISHED;
         mon_log(INFO_SIGN, NULL, 0, 0L, NULL,
                 "========> AFDD Connected <========");
         if ((bytes_buffered = tcp_cmd("%s", START_STAT_CMD)) < 0)
         {
            if (bytes_buffered == -AFDD_SHUTTING_DOWN)
            {
               timeout_flag = ON;
               (void)tcp_quit();
               timeout_flag = OFF;
               msa[afd_no].connect_status = DISCONNECTED;
            }
            else
            {
               mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, msg_str,
                       "Failed to send %s command.", START_STAT_CMD);
               (void)tcp_quit();
               msa[afd_no].connect_status = CONNECTION_DEFUNCT;
            }
            retry_interval = RETRY_INTERVAL;
         }
         else
         {
            if ((msa[afd_no].connect_time != 0) &&
                (msa[afd_no].disconnect_time != 0))
            {
               start_time = time(NULL);
            }
            else
            {
               start_time = 0; /* Silence compiler. */
            }
            FD_ZERO(&rset);
            for (;;)
            {
               /*
                * Check if it is midnight so we can reset the top
                * rate counters.
                */
               if (time(&now) > new_day_time)
               {
                  for (i = (STORAGE_TIME - 1); i > 0; i--)
                  {
                     msa[afd_no].top_no_of_transfers[i] = msa[afd_no].top_no_of_transfers[i - 1];
                     msa[afd_no].top_tr[i] = msa[afd_no].top_tr[i - 1];
                     msa[afd_no].top_fr[i] = msa[afd_no].top_fr[i - 1];
                  }
                  msa[afd_no].top_no_of_transfers[0] = 0;
                  msa[afd_no].top_tr[0] = msa[afd_no].top_fr[0] = 0;
                  msa[afd_no].top_not_time = msa[afd_no].top_tr_time = msa[afd_no].top_fr_time = 0L;
                  new_day_time = ((now / 86400) * 86400) + 86400;
               }
               if (now > (new_hour_time + 120))
               {
                  for (i = 0; i < NO_OF_LOG_HISTORY; i++)
                  {
                     shift_log_his[i] = NO;
                  }
                  new_hour_time = ((now / 3600) * 3600) + 3600;
               }

               while (bytes_buffered > 0)
               {
                  if ((bytes_buffered = read_msg()) == INCORRECT)
                  {
                     bytes_buffered -= bytes_done;
                     msa[afd_no].connect_status = CONNECTION_DEFUNCT;
                     retry_interval = RETRY_INTERVAL;
                     goto done;
                  }
                  else
                  {
                     if (evaluate_message(&bytes_done) == AFDD_SHUTTING_DOWN)
                     {
                        bytes_buffered -= bytes_done;
                        retry_interval = RETRY_INTERVAL;
                        goto done;
                     }
                     bytes_buffered -= bytes_done;
                  }
               }

               if ((msa[afd_no].log_capabilities > 0) &&
                   (got_log_capabilities == YES) && (send_log_request == NO))
               {
                  if (send_got_log_capabilities(afd_no) == SUCCESS)
                  {
                     send_log_request = YES;
                  }
               }

               /* Initialise descriptor set and timeout. */
               FD_SET(sock_fd, &rset);
               timeout.tv_usec = 0;
               timeout.tv_sec = msa[afd_no].poll_interval;

               /* Wait for message x seconds and then continue. */
               status = select(sock_fd + 1, &rset, NULL, NULL, &timeout);

               if (FD_ISSET(sock_fd, &rset))
               {
                  msa[afd_no].last_data_time = time(NULL);
                  do
                  {
                     if ((bytes_buffered = read_msg()) == INCORRECT)
                     {
                        msa[afd_no].connect_status = CONNECTION_DEFUNCT;
                        retry_interval = RETRY_INTERVAL;
                        goto done;
                     }
                     else
                     {
                        if (evaluate_message(&bytes_done) == AFDD_SHUTTING_DOWN)
                        {
                           bytes_buffered -= bytes_done;
                           retry_interval = RETRY_INTERVAL;
                           goto done;
                        }
                        bytes_buffered -= bytes_done;
                     }
                  } while (bytes_buffered > 0);
               }
               else if (status == 0)
                    {
                       if ((bytes_buffered = tcp_cmd("%s", STAT_CMD)) < 0)
                       {
                          if (bytes_buffered != -AFDD_SHUTTING_DOWN)
                          {
                             mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, msg_str,
                                     "Failed to send %s command.", STAT_CMD);
                             (void)tcp_quit();
                             msa[afd_no].connect_status = CONNECTION_DEFUNCT;
                          }
                          retry_interval = RETRY_INTERVAL;
                          break;
                       }
                       else
                       {
                          msa[afd_no].last_data_time = now + msa[afd_no].poll_interval;
                          if (evaluate_message(&bytes_done) == AFDD_SHUTTING_DOWN)
                          {
                             bytes_buffered -= bytes_done;
                             retry_interval = RETRY_INTERVAL;
                             break;
                          }
                          bytes_buffered -= bytes_done;
                       }
                    }
                    else
                    {
                       system_log(FATAL_SIGN, __FILE__, __LINE__,
                                  "select() error : %s", strerror(errno));
                       exit(MON_SELECT_ERROR);
                    }

               if ((msa[afd_no].connect_time != 0) &&
                   (msa[afd_no].disconnect_time != 0))
               {
                  if ((time(NULL) - start_time) >= msa[afd_no].connect_time)
                  {
                     if (tcp_quit() != SUCCESS)
                     {
                        mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, msg_str,
                                "Failed to send %s command.", QUIT_CMD);
                     }
                     retry_interval = msa[afd_no].disconnect_time;
                     msa[afd_no].connect_status = DISCONNECTED;
                     mon_log(INFO_SIGN, NULL, 0, 0L, NULL,
                             "========> Disconnect (due to connect interval %ds) <========",
                             msa[afd_no].connect_time);
                     break;
                  }
               }
               if ((msa[afd_no].afd_switching != NO_SWITCHING) &&
                   (current_afd_toggle != msa[afd_no].afd_toggle))
               {
                  if (tcp_quit() != SUCCESS)
                  {
                     mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, msg_str,
                             "Failed to send %s command.", QUIT_CMD);
                  }
                  msa[afd_no].connect_status = DISCONNECTED;
                  retry_interval = 0L;
                  mon_log(WARN_SIGN, NULL, 0, 0L, NULL,
                          "Switching to %s%d (%s at port %d).",
                          msa[afd_no].afd_alias, (int)msa[afd_no].afd_toggle,
                          msa[afd_no].hostname[(int)msa[afd_no].afd_toggle],
                          msa[afd_no].port[(int)msa[afd_no].afd_toggle]);
                  break;
               }
            } /* for (;;) */
         }
      }

done:
      if (adl != NULL)
      {
#ifdef HAVE_MMAP
         if (munmap((void *)adl, adl_size) == -1)
#else
         if (munmap_emu((void *)adl) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "munmap() error : %s", strerror(errno));
         }
         adl = NULL;
      }
      if (ajl != NULL)
      {
#ifdef HAVE_MMAP
         if (munmap((void *)ajl, ajl_size) == -1)
#else
         if (munmap_emu((void *)ajl) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "munmap() error : %s", strerror(errno));
         }
         ajl = NULL;
      }
      if (atd != NULL)
      {
#ifdef HAVE_MMAP
         if (munmap((void *)atd, atd_size) == -1)
#else
         if (munmap_emu((void *)atd) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "munmap() error : %s", strerror(errno));
         }
         atd = NULL;
      }
      msa[afd_no].tr = 0;

      /* Initialise descriptor set and timeout. */
      FD_ZERO(&rset);
      FD_SET(retry_fd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = retry_interval;

      /* Wait for message x seconds and then continue. */
      status = select(retry_fd + 1, &rset, NULL, NULL, &timeout);

      if (FD_ISSET(retry_fd, &rset))
      {
         /*
          * This fifo is just here to wake up, so disregard whats
          * in it.
          */
         if (read(retry_fd, msg_str, MAX_RET_MSG_LENGTH) < 0)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "read() error on %s : %s", retry_fifo, strerror(errno));
         }
      }
      else if (status == -1)
           {
              system_log(FATAL_SIGN, __FILE__, __LINE__,
                         "select() error : %s", strerror(errno));
              exit(INCORRECT);
           }

      if (retry_interval == 0L)
      {
         if (msa[afd_no].disconnect_time != 0L)
         {
            retry_interval = msa[afd_no].disconnect_time;
         }
         else
         {
            retry_interval = RETRY_INTERVAL;
         }
      }
   } /* for (;;) */
}


/*++++++++++++++++++++++++++++++ tcp_cmd() ++++++++++++++++++++++++++++++*/
static int
tcp_cmd(char *fmt, ...)
{
   int     bytes_buffered,
           bytes_done,
           length;
   char    buf[MAX_LINE_LENGTH + 1];
   va_list ap;

   va_start(ap, fmt);
   length = vsnprintf(buf, MAX_LINE_LENGTH, fmt, ap);
   va_end(ap);
   if (length > MAX_LINE_LENGTH)
   {
      mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, msg_str,
              "tcp_cmd(): Command to long (%d > %d)", length, MAX_LINE_LENGTH);
      return(INCORRECT);
   }
   buf[length] = '\r';
   buf[length + 1] = '\n';
   length += 2;
#ifdef WITH_SSL
   if (ssl_con == NULL)
   {
#endif
      if (write(sock_fd, buf, length) != length)
      {
         mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, msg_str,
                 "tcp_cmd(): write() error : %s", strerror(errno));
         return(INCORRECT);
      }
#ifdef WITH_SSL
   }
   else
   {
      if (ssl_write(ssl_con, buf, length) != length)
      {
         return(INCORRECT);
      }
   }
#endif

   while ((bytes_buffered = read_msg()) != INCORRECT)
   {
      if (bytes_buffered > 3)
      {
         if ((msg_str[0] == '2') && (msg_str[1] == '1') &&
             (msg_str[2] == '1') && (msg_str[3] == '-'))
         {
            bytes_buffered = read_msg();
            break;
         }
         if ((isdigit((int)msg_str[0])) && (isdigit((int)msg_str[1])) &&
             (isdigit((int)msg_str[2])) && (msg_str[3] == '-'))
         {
            return(-(((msg_str[0] - '0') * 100) + ((msg_str[1] - '0') * 10) +
                   (msg_str[2] - '0')));
         }
         if ((isupper((int)msg_str[0])) && (isupper((int)msg_str[1])) &&
             (msg_str[2] == ' '))
         {
            if (evaluate_message(&bytes_done) == AFDD_SHUTTING_DOWN)
            {
               return(-AFDD_SHUTTING_DOWN);
            }
            bytes_buffered -= bytes_done;
         }
         else
         {
            mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, msg_str,
                    "Reading garbage, don't know what to do?");
            return(INCORRECT);
         }
      }
      else
      {
         mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, msg_str,
                 "Reading garbage, don't know what to do?");
         return(INCORRECT);
      }
   }

   return(bytes_buffered);
}


/*+++++++++++++++++++++ send_got_log_capabilities() +++++++++++++++++++++*/
static int
send_got_log_capabilities(int afd_no)
{
   int  mon_cmd_fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
   int  mon_cmd_readfd;
#endif
   char mon_cmd_fifo[MAX_PATH_LENGTH];

   (void)snprintf(mon_cmd_fifo, MAX_PATH_LENGTH, "%s%s%s", p_work_dir, FIFO_DIR, MON_CMD_FIFO);

   /* Open fifo to AFD to receive commands. */
#ifdef WITHOUT_FIFO_RW_SUPPORT                
   if (open_fifo_rw(mon_cmd_fifo, &mon_cmd_readfd, &mon_cmd_fd) == -1)
#else                                                                  
   if ((mon_cmd_fd = open(mon_cmd_fifo, O_RDWR)) == -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s", mon_cmd_fifo, strerror(errno));
   }
   else
   {
      int  length;
      char cmd[1 + SIZEOF_INT];

      cmd[0] = GOT_LC;
      (void)memcpy(&cmd[1], &afd_no, SIZEOF_INT);
      length = 1 + SIZEOF_INT;
      if (write(mon_cmd_fd, cmd, length) != length)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to write() %d bytes to `%s' : %s",
                    length, mon_cmd_fifo, strerror(errno));
         length = INCORRECT;
      }
      else
      {
         length = SUCCESS;
      }
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (close(mon_cmd_readfd) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to close() `%s' (read) : %s",
                    mon_cmd_fifo, strerror(errno));
      }
#endif
      if (close(mon_cmd_fd) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to close() `%s' : %s",
                    mon_cmd_fifo, strerror(errno));
      }

      return(length);
   }

   return(INCORRECT);                                                       
}


/*++++++++++++++++++++++++++++++ mon_exit() +++++++++++++++++++++++++++++*/
static void
mon_exit(void)
{
   if (tcp_quit() < 0)
   {
      mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, NULL,
              "Failed to close TCP connection.");
   }
   if (msa[afd_no].connect_status == DISABLED)
   {
      msa[afd_no].fr = 0;
      msa[afd_no].fc = 0;
      msa[afd_no].fs = 0;
      msa[afd_no].ec = 0;
      msa[afd_no].no_of_transfers = 0;
      msa[afd_no].host_error_counter = 0;
   }
   else
   {
      msa[afd_no].connect_status = DISCONNECTED;
   }
   msa[afd_no].tr = 0;
   mon_log(INFO_SIGN, NULL, 0, 0L, NULL, "========> Disconnect <========");

   if (msa_detach() != SUCCESS)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__, "Failed to detach from MSA.");
   }

#ifdef WITHOUT_FIFO_RW_SUPPORT
   (void)close(mon_log_readfd);
#endif
   (void)close(mon_log_fd);
   (void)close(sys_log_fd);

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   system_log(FATAL_SIGN, __FILE__, __LINE__, "Aaarrrggh! Received SIGSEGV.");

   /* Dump core so we know what happened! */
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   system_log(FATAL_SIGN, __FILE__, __LINE__, "Uuurrrggh! Received SIGBUS.");

   /* Dump core so we know what happened! */
   abort();
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   exit(INCORRECT);
}
