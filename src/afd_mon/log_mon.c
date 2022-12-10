/*
 *  log_mon.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 - 2022 Deutscher Wetterdienst (DWD),
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
 **   log_mon - process that handles the log data of the remote AFD
 **
 ** SYNOPSIS
 **   log_mon [--version] [-w <working directory>] AFD-number Log-capabilities
 **
 ** DESCRIPTION
 **   Checks if any log data is received from the given remote AFD.
 **   This data is evaluated and then written to the appropriate log
 **   file. The messages received via socket have the following
 **   format:
 **
 **      <log type> <options> <packet number> <packet length> <packet data>
 **
 **   The following <log types> are known:
 **
 **      LS - System Log data
 **      LE - Event Log data
 **      LR - Receive Log data
 **      LT - Transfer Log data
 **      LB - Transfer Debug Log data
 **      LI - Input Log data
 **      LU - Distribution Log data
 **      LP - Production Log data
 **      LO - Output Log data
 **      LD - Delete Log data
 **      LN - NOP message
 **
 **   The <options> field holds information if this packet is compressed
 **   and if yes what compression type is used.
 **
 **   Additionally this also nneds to handle the inode data of the remote
 **   log files. These messages have the following format:
 **
 **      <inode type> <inode number>
 **
 **   The following <inode types> are known:
 **
 **      OS - System Log data
 **      OE - Event Log data
 **      OR - Receive Log data
 **      OT - Transfer Log data
 **      OB - Transfer Debug Log data
 **      OI - Input Log data
 **      OU - Distribution Log data
 **      OP - Production Log data
 **      OO - Output Log data
 **      OD - Delete Log data
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   08.01.2007 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>           /* fprintf()                                */
#include <string.h>          /* strcpy(), strcat(), strerror(), memmove()*/
#include <stdlib.h>          /* strtoul(), atexit()                      */
#include <ctype.h>           /* isdigit()                                */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>        /* struct timeval                           */
#ifdef WITH_SSL
# include <openssl/ssl.h>
#endif
#include <signal.h>          /* signal()                                 */
#ifdef HAVE_MMAP
# include <sys/mman.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "mondefs.h"
#include "logdefs.h"
#include "afdd_common_defs.h"
#include "version.h"

/* #define DEBUG_LOG_CMD */
/* #define _LOG_DEBUG */

/* Global variables. */
int                    log_fd[NO_OF_LOGS],
                       mon_log_fd = STDERR_FILENO,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                       mon_log_readfd,
#endif
                       msa_fd = -1,
                       msa_id,
                       no_of_afds,
                       sock_fd,
                       sys_log_fd = STDERR_FILENO,
                       timeout_flag;
#ifdef WITH_SSL
SSL                    *ssl_con = NULL;
#endif
unsigned int           log_flags[NO_OF_LOGS];
off_t                  msa_size;
long                   tcp_timeout = 120L;
char                   log_dir[MAX_PATH_LENGTH],
                       msg_str[MAX_RET_MSG_LENGTH],
                       *p_log_dir,
                       *p_mon_alias,
                       *p_work_dir;
struct mon_status_area *msa;
const char             *sys_log_name = MON_SYS_LOG_FIFO;

/* Local global variables. */
static int             last_packet_number[NO_OF_LOGS];
static char            cur_ino_log_no_str[NO_OF_LOGS][MAX_INODE_LOG_NO_LENGTH];

/* Local function prototypes. */
static int             check_inode(char *, char *, int, int *, int *);
static void            check_create_log_file(char *, int),
                       eval_log_buffer(char *, int, int *, int),
                       get_cur_ino_log_no(char *, int),
                       log_mon_exit(void),
                       sig_bus(int),
                       sig_exit(int),
                       sig_segv(int),
                       write_remote_log_inode(char *, int, int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ log_mon() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int            afd_no,
                  bytes_buffered,
                  status;
   unsigned int   log_capabilities;
   long           log_data_interval;
   char           *log_data_buffer,
                  mon_log_fifo[MAX_PATH_LENGTH],
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

   if (argc != 3)
   {
      (void)fprintf(stderr,
                    "Usage: %s [-w working directory] AFD-number Log-capabilities\n",
                    argv[0]);
      exit(INCORRECT);
   }
   else
   {
      char *ptr = argv[1];

      while (*ptr != '\0')
      {
         if (isdigit((int)(*ptr)) == 0)
         {
            (void)fprintf(stderr,
                          "Usage: %s [-w working directory] AFD-number Log-capabilities\n",
                          argv[0]);
            exit(MON_SYNTAX_ERROR);
         }
         ptr++;
      }
      afd_no = atoi(argv[1]);

      ptr = argv[2];
      while (*ptr != '\0')
      {
         if (isdigit((int)(*ptr)) == 0)
         {
            (void)fprintf(stderr,
                          "Usage: %s [-w working directory] AFD-number Log-capabilities\n",
                          argv[0]);
            exit(MON_SYNTAX_ERROR);
         }
         ptr++;
      }
      log_capabilities = (unsigned int)atoi(argv[2]);
   }

   /* Initialize variables. */
   (void)strcpy(mon_log_fifo, p_work_dir);
   (void)strcat(mon_log_fifo, FIFO_DIR);
   (void)strcat(mon_log_fifo, MON_LOG_FIFO);
   if ((log_data_buffer = malloc(MAX_LOG_DATA_BUFFER)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() %d bytes : %s",
                 MAX_LOG_DATA_BUFFER, strerror(errno));
      exit(INCORRECT);
   }
   for (status = 0; status < NO_OF_LOGS; status++)
   {
      log_fd[status] = -1;
      cur_ino_log_no_str[status][0] = '\0';
   }
   log_flags[SYS_LOG_POS] = AFDD_SYSTEM_LOG;
   log_flags[EVE_LOG_POS] = AFDD_EVENT_LOG;
   log_flags[REC_LOG_POS] = AFDD_RECEIVE_LOG;
   log_flags[TRA_LOG_POS] = AFDD_TRANSFER_LOG;
   log_flags[TDB_LOG_POS] = AFDD_TRANSFER_DEBUG_LOG;
#ifdef _INPUT_LOG
   log_flags[INP_LOG_POS] = AFDD_INPUT_LOG;
#endif
#ifdef _DISTRIBUTION_LOG
   log_flags[DIS_LOG_POS] = AFDD_DISTRIBUTION_LOG;
#endif
#ifdef _PRODUCTION_LOG
   log_flags[PRO_LOG_POS] = AFDD_PRODUCTION_LOG;
#endif
#ifdef _OUTPUT_LOG
   log_flags[OUT_LOG_POS] = AFDD_OUTPUT_LOG;
#endif
#ifdef _DELETE_LOG
   log_flags[DEL_LOG_POS] = AFDD_DELETE_LOG;
#endif

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

   /* Do some cleanups when we exit. */
   if (atexit(log_mon_exit) != 0)
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

   /* Attach to MSA. */
   if (msa_attach() != SUCCESS)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__, "Failed to attach to MSA.");
      exit(INCORRECT);
   }
   p_mon_alias = msa[afd_no].afd_alias;

   /* Check log_capabilities. */
   if (log_capabilities != msa[afd_no].log_capabilities)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Log capabilities have changed from %u to %u!",
                 log_capabilities, msa[afd_no].log_capabilities);
      log_capabilities = msa[afd_no].log_capabilities;
   }

   /* Check if log directory exists. */
   p_log_dir = log_dir + sprintf(log_dir, "%s%s/%s/",
                                 p_work_dir, RLOG_DIR, p_mon_alias);
   if (check_dir(log_dir, R_OK | W_OK | X_OK) != SUCCESS)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Unable to get directory `%s', terminating.", log_dir);
      exit(INCORRECT);
   }

   /* Connect to remote AFDD. */
   timeout_flag = OFF;
   if ((status = tcp_connect(msa[afd_no].hostname[(int)msa[afd_no].afd_toggle],
                             msa[afd_no].port[(int)msa[afd_no].afd_toggle],
                             YES
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
      exit(LOG_CONNECT_ERROR);
   }
   mon_log(INFO_SIGN, NULL, 0, 0L, NULL,
           "========> AFDD Log Connected (%u) <========", log_capabilities);

   bytes_buffered = 0;
   if (send_log_cmd(afd_no, log_data_buffer, &bytes_buffered) != SUCCESS)
   {
      exit(FAILED_LOG_CMD);
   }
   log_data_interval = AFDD_CMD_TIMEOUT;
   if (log_data_interval < (10 * LOG_WRITE_INTERVAL))
   {
      log_data_interval = 10 * LOG_WRITE_INTERVAL;
   }

   FD_ZERO(&rset);
   for (;;)
   {
      /* Initialise descriptor set. */
      FD_SET(sock_fd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = log_data_interval;

      status = select(sock_fd + 1, &rset, NULL, NULL, &timeout);

      if ((status > 0) && (FD_ISSET(sock_fd, &rset)))
      {
         if ((status = read(sock_fd, &log_data_buffer[bytes_buffered],
                            MAX_LOG_DATA_BUFFER - bytes_buffered)) > 0)
         {
#ifdef _LOG_DEBUG
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Have received %d bytes.", status);
#endif
            eval_log_buffer(log_data_buffer, status, &bytes_buffered, afd_no);
            msa[afd_no].log_bytes_received[CURRENT_SUM] += status;
         }
         else if (status == 0)
              {
                 mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, NULL,
                         "Remote hang up.");
                 timeout_flag = NEITHER;
                 exit(REMOTE_HANGUP);
              }
              else
              {
                 mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                         "read() error (after reading %d Bytes) : %s",
                         bytes_buffered, strerror(errno));
                 exit(INCORRECT);
              }
      }
      else if (status == 0)
           {
              mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, NULL,
                      "Not receiving any data for more then %ld seconds, hanging up.",
                      log_data_interval);
              exit(LOG_DATA_TIMEOUT);
           }
           else
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "select() error : %s", strerror(errno));
              exit(INCORRECT);
           }
   } /* for (;;) */

   return(INCORRECT);
}


/*++++++++++++++++++++++++++ eval_log_buffer() ++++++++++++++++++++++++++*/
static void
eval_log_buffer(char *log_data_buffer,
                int  bytes_read,
                int  *bytes_buffered,
                int  afd_no)
{
   int i,
       start_i;

   if (*bytes_buffered > 0)
   {
      bytes_read += *bytes_buffered;
      *bytes_buffered = 0;
   }

   /*
    * We must always mark the end of what we read with a special character
    * otherwise it can happen that we think part of the old buffer is
    * part of the new message. This special character must be one that
    * is not part of the header message.
    */
   i = MAX_LOG_DATA_BUFFER - bytes_read;
   if (i > 0)
   {
      log_data_buffer[bytes_read] = 1;
   }
   i = 0;
   do
   {
      if ((i + 2) < bytes_read)
      {
         start_i = i;
      }
      else
      {
         if (i != 0)
         {
            *bytes_buffered += (bytes_read - i);
            (void)memmove(log_data_buffer, &log_data_buffer[i],
                          (bytes_read - i));
         }
         else
         {
            *bytes_buffered += bytes_read;
         }
         return;
      }
      switch (log_data_buffer[i])
      {
         case 'L' : /* Log data */
            {
               int log_type;

               /*
                * Lets also accept log types we do not know, just discard
                * the data we receive for it. This way older versions of
                * AFD_MON will not complain if they receive a new log data
                * type from a newer AFD version.
                */
               switch (log_data_buffer[i + 1])
               {
#ifdef _OUTPUT_LOG
                  case 'O' : /* Output log */
                     log_type = OUT_LOG_POS;
                     break;
#endif
#ifdef _INPUT_LOG
                  case 'I' : /* Input log */
                     log_type = INP_LOG_POS;
                     break;
#endif
                  case 'T' : /* Transfer log */
                     log_type = TRA_LOG_POS;
                     break;
                  case 'R' : /* Receive log */
                     log_type = REC_LOG_POS;
                     break;
#ifdef _DISTRIBUTION_LOG
                  case 'U' : /* Distribution log */
                     log_type = DIS_LOG_POS;
                     break;
#endif
#ifdef _PRODUCTION_LOG
                  case 'P' : /* Production log */
                     log_type = PRO_LOG_POS;
                     break;
#endif
#ifdef _DELETE_LOG
                  case 'D' : /* Delete log */
                     log_type = DEL_LOG_POS;
                     break;
#endif
                  case 'S' : /* System log */
                     log_type = SYS_LOG_POS;
                     break;
                  case 'E' : /* Event log */
                     log_type = EVE_LOG_POS;
                     break;
                  case 'N' : /* No log data message */
                     if ((i + 4) <= bytes_read)
                     {
                        if ((log_data_buffer[i + 2] == '\r') &&
                            (log_data_buffer[i + 3] == '\n'))
                        {
                           i += 4;
                           continue;
                        }
                        else
                        {
                           mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, NULL,
                                   "Reading garbage! Discarding data!");
                           *bytes_buffered = 0;
                           return;
                        }
                     }
                     else
                     {
                        if (i != 0)
                        {
                           *bytes_buffered += (bytes_read - i);
                           (void)memmove(log_data_buffer, &log_data_buffer[i],
                                         (bytes_read - i));
                        }
                        else
                        {
                           *bytes_buffered += bytes_read;
                        }
                        return;
                     }
                     break;
                  case 'B' : /* Transfer debug log */
                     log_type = TDB_LOG_POS;
                     break;
                  default : /* Unknown log type */
                     log_type = DUM_LOG_POS;
                     break;
               }
               if (log_data_buffer[i + 2] == ' ')
               {
                  int  j;
                  char str_number[MAX_INT_LENGTH + 1];

                  i += 3;

                  /* Read options. */
                  j = 0;
                  while ((i < bytes_read) && (j < MAX_INT_LENGTH) &&
                         (isdigit((int)(log_data_buffer[i]))))
                  {
                     str_number[j] = log_data_buffer[i];
                     i++; j++;
                  }
                  if ((j > 0) && (i < bytes_read) &&
                      (log_data_buffer[i] == ' '))
                  {
                     unsigned int options;

                     str_number[j] = '\0';
                     options = (unsigned int)atoi(str_number);
                     i++;

                     /* Read packet number. */
                     j = 0;
                     while ((i < bytes_read) && (j < MAX_INT_LENGTH) &&
                            (isdigit((int)(log_data_buffer[i]))))
                     {
                        str_number[j] = log_data_buffer[i];
                        i++; j++;
                     }
                     if ((j > 0) && (i < bytes_read) &&
                         (log_data_buffer[i] == ' '))
                     {
                        unsigned int packet_number;

                        str_number[j] = '\0';
                        packet_number = (unsigned int)atoi(str_number);
                        if ((packet_number != (last_packet_number[log_type] + 1)) &&
                            (packet_number != 0) && (log_type != DUM_LOG_POS))
                        {
                           char pri_log_name[13];

                           switch (log_type)
                           {
#ifdef _OUTPUT_LOG
                              case OUT_LOG_POS :
                                 (void)strcpy(pri_log_name, "output");
                                 break;
#endif
#ifdef _INPUT_LOG
                              case INP_LOG_POS :
                                 (void)strcpy(pri_log_name, "input");
                                 break;
#endif
                              case TRA_LOG_POS :
                                 (void)strcpy(pri_log_name, "transfer");
                                 break;
                              case REC_LOG_POS :
                                 (void)strcpy(pri_log_name, "receive");
                                 break;
#ifdef _DISTRIBUTION_LOG
                              case DIS_LOG_POS :
                                 (void)strcpy(pri_log_name, "distribution");
                                 break;
#endif
#ifdef _PRODUCTION_LOG
                              case PRO_LOG_POS :
                                 (void)strcpy(pri_log_name, "production");
                                 break;
#endif
#ifdef _DELETE_LOG
                              case DEL_LOG_POS :
                                 (void)strcpy(pri_log_name, "delete");
                                 break;
#endif
                              case SYS_LOG_POS :
                                 (void)strcpy(pri_log_name, "system");
                                 break;
                              case EVE_LOG_POS :
                                 (void)strcpy(pri_log_name, "event");
                                 break;
                              case TDB_LOG_POS :
                                 (void)strcpy(pri_log_name, "trans debug");
                                 break;
                              default :
                                 (void)strcpy(pri_log_name, "unknown");
                                 break;
                           }

                           /* We missed one or more packets! */
                           mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, NULL,
                                   "We missed %u packet(s) from %s log (%u %u)",
                                   (unsigned int)(packet_number - (last_packet_number[log_type] + 1)),
                                   pri_log_name, packet_number,
                                   last_packet_number[log_type]);

                           exit(MISSED_PACKET);
                        }
                        i++;

                        /* Read packet length. */
                        j = i;
                        while ((i < bytes_read) && ((i - j) < MAX_INT_LENGTH) &&
                               (isdigit((int)(log_data_buffer[i]))))
                        {
                           i++;
                        }
                        if ((i > j) && (i < bytes_read) &&
                            (log_data_buffer[i] == '\0'))
                        {
                           unsigned int packet_length;

                           packet_length = (unsigned int)atoi(&log_data_buffer[j]);
                           i++;
                           if ((*bytes_buffered + bytes_read - i) >= packet_length)
                           {
                              if (log_type != DUM_LOG_POS)
                              {
                                 write_afd_log(afd_no, log_type, options,
                                               packet_length,
                                               &log_data_buffer[i]);
                              }
                              i += packet_length;
                              last_packet_number[log_type] = packet_number;
#ifdef DEBUG_LOG_CMD
                              mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, NULL,
                                      "R-> %c%c %u %u %u",
                                      log_data_buffer[start_i],
                                      log_data_buffer[start_i + 1],
                                      options, packet_number, packet_length);
#endif
                           }
                           else
                           {
                              if (start_i != 0)
                              {
                                 *bytes_buffered += (bytes_read - start_i);
                                 (void)memmove(log_data_buffer,
                                               &log_data_buffer[start_i],
                                               (bytes_read - start_i));
                              }
                              else
                              {
                                 *bytes_buffered += bytes_read;
                              }
#ifdef _LOG_DEBUG
                              mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, NULL,
                                      "Need to read more data! (packet_length=%u rest=%d bytes_read=%d start_i=%d)",
                                      packet_length, (bytes_read - i),
                                      bytes_read, start_i);
#endif
                              return;
                           }
                        }
                        else
                        {
#ifdef _LOG_DEBUG
                           mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, NULL,
                                   "Unable to locate end of packet length for %c%c. Buffering %d bytes.",
                                   log_data_buffer[start_i],
                                   log_data_buffer[start_i + 1],
                                   bytes_read - start_i);
#endif
                           if (start_i != 0)
                           {
                              *bytes_buffered += (bytes_read - start_i);
                              (void)memmove(log_data_buffer,
                                            &log_data_buffer[start_i],
                                            (bytes_read - start_i));
                           }
                           else
                           {
                              *bytes_buffered += bytes_read;
                           }
                           return;
                        }
                     }
                     else
                     {
#ifdef _LOG_DEBUG
                        mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, NULL,
                                "Unable to locate end of packet number for %c%c. Buffering %d bytes.",
                                log_data_buffer[start_i],
                                log_data_buffer[start_i + 1],
                                bytes_read - start_i);
#endif
                        if (start_i != 0)
                        {
                           *bytes_buffered += (bytes_read - start_i);
                           (void)memmove(log_data_buffer,
                                         &log_data_buffer[start_i],
                                         (bytes_read - start_i));
                        }
                        else
                        {
                           *bytes_buffered += bytes_read;
                        }
                        return;
                     }
                  }
                  else
                  {
#ifdef _LOG_DEBUG
                     mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, NULL,
                             "Unable to locate end of options for %c%c. Buffering %d bytes.",
                             log_data_buffer[start_i],
                             log_data_buffer[start_i + 1],
                             bytes_read - start_i);
#endif
                     if (start_i != 0)
                     {
                        *bytes_buffered += (bytes_read - start_i);
                        (void)memmove(log_data_buffer,
                                      &log_data_buffer[start_i],
                                      (bytes_read - start_i));
                     }
                     else
                     {
                        *bytes_buffered += bytes_read;
                     }
                     return;
                  }
               }
               else
               {
#ifdef _LOG_DEBUG
                  mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, NULL,
                          "Unable to locate start of L%c message type. Buffering %d bytes.",
                          log_data_buffer[i + 1], bytes_read - start_i);
#endif
                  if (start_i != 0)
                  {
                     *bytes_buffered += (bytes_read - start_i);
                     (void)memmove(log_data_buffer,
                                   &log_data_buffer[start_i],
                                   (bytes_read - start_i));
                  }
                  else
                  {
                     *bytes_buffered += bytes_read;
                  }
                  return;
               }
            }
            break;

         case 'O' : /* Log meta data */
            {
               int  log_pos,
                    ret;
               char log_name[MAX_LOG_NAME_LENGTH + 1];

               switch (log_data_buffer[i + 1])
               {
#ifdef _OUTPUT_LOG
                  case 'O' : /* Output log */
                     (void)strcpy(log_name, OUTPUT_BUFFER_FILE);
                     log_pos = OUT_LOG_POS;
                     break;
#endif
#ifdef _INPUT_LOG
                  case 'I' : /* Input log */
                     (void)strcpy(log_name, INPUT_BUFFER_FILE);
                     log_pos = INP_LOG_POS;
                     break;
#endif
                  case 'T' : /* Transfer log */
                     (void)strcpy(log_name, TRANSFER_LOG_NAME);
                     log_pos = TRA_LOG_POS;
                     break;
                  case 'R' : /* Receive log */
                     (void)strcpy(log_name, RECEIVE_LOG_NAME);
                     log_pos = REC_LOG_POS;
                     break;
#ifdef _DISTRIBUTION_LOG
                  case 'U' : /* Distribution log */
                     (void)strcpy(log_name, DISTRIBUTION_BUFFER_FILE);
                     log_pos = DIS_LOG_POS;
                     break;
#endif
#ifdef _PRODUCTION_LOG
                  case 'P' : /* Production log */
                     (void)strcpy(log_name, PRODUCTION_BUFFER_FILE);
                     log_pos = PRO_LOG_POS;
                     break;
#endif
#ifdef _DELETE_LOG
                  case 'D' : /* Delete log */
                     (void)strcpy(log_name, DELETE_BUFFER_FILE);
                     log_pos = DEL_LOG_POS;
                     break;
#endif
                  case 'S' : /* System log */
                     (void)strcpy(log_name, SYSTEM_LOG_NAME);
                     log_pos = SYS_LOG_POS;
                     break;
                  case 'E' : /* Event log */
                     (void)strcpy(log_name, EVENT_LOG_NAME);
                     log_pos = EVE_LOG_POS;
                     break;
                  case 'B' : /* Transfer debug log */
                     (void)strcpy(log_name, TRANS_DB_LOG_NAME);
                     log_pos = TDB_LOG_POS;
                     break;
                  default : /* Unknown log type */
                     (void)strcpy(log_name, "UNKNOWN.");
                     log_pos = DUM_LOG_POS;
                     break;
               }
               if (log_data_buffer[i + 2] == ' ')
               {
                  int shift,
                      shift_offset,
                      tmp_i;

                  i += 3;

                  /*
                   * Before we evaluate, lets see if we have the complete
                   * message. If not lets read again.
                   */
                  tmp_i = i;
                  while ((i < bytes_read) && (log_data_buffer[i] != '\r'))
                  {
                     i++;
                  }
                  if ((i < bytes_read) && (log_data_buffer[i] == '\r') &&
                      (log_data_buffer[i + 1] == '\n'))
                  {
                     i += 2;
                  }
                  else
                  {
#ifdef _LOG_DEBUG
                     mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, NULL,
                             "Hmm, not reading end of O%c message! Buffering %d bytes.",
                             log_data_buffer[start_i + 1],
                             bytes_read - start_i);
#endif
                     if (start_i != 0)
                     {
                        *bytes_buffered += (bytes_read - start_i);
                        (void)memmove(log_data_buffer,
                                      &log_data_buffer[start_i],
                                      (bytes_read - start_i));
                     }
                     else
                     {
                        *bytes_buffered += bytes_read;
                     }
                     return;
                  }

                  /* Now evaluate what we have. */
                  if ((log_pos != DUM_LOG_POS) &&
                      ((ret = check_inode(&log_data_buffer[tmp_i],
                                          log_name, log_pos,
                                          &shift, &shift_offset)) != SUCCESS))
                  {
                     if (log_fd[log_pos] != -1)
                     {
                        if (close(log_fd[log_pos]) == -1)
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      "Failed to close() log file : %s",
                                      strerror(errno));
                        }
                        log_fd[log_pos] = -1;
                     }
                     if (ret == LOG_RESHUFFEL)
                     {
                        int  default_log_files,
                             log_name_length,
                             log_number = 0,
                             max_log_files;
                        char max_log_def[MAX_LOG_DEF_NAME_LENGTH + 1],
                             *p_end;

                        switch (log_pos)
                        {
#ifdef _OUTPUT_LOG
                           case OUT_LOG_POS : /* Output log */
                              (void)strcpy(max_log_def, MAX_OUTPUT_LOG_FILES_DEF);
                              max_log_files = default_log_files = MAX_OUTPUT_LOG_FILES;
                              log_name_length = OUTPUT_BUFFER_FILE_LENGTH;
                              break;
#endif
#ifdef _INPUT_LOG
                           case INP_LOG_POS : /* Input log */
                              (void)strcpy(max_log_def, MAX_INPUT_LOG_FILES_DEF);
                              max_log_files = default_log_files = MAX_INPUT_LOG_FILES;
                              log_name_length = INPUT_BUFFER_FILE_LENGTH;
                              break;
#endif
                           case TRA_LOG_POS : /* Transfer log */
                              (void)strcpy(max_log_def, MAX_TRANSFER_LOG_FILES_DEF);
                              max_log_files = default_log_files = MAX_TRANSFER_LOG_FILES;
                              log_name_length = TRANSFER_LOG_NAME_LENGTH;
                              break;
                           case REC_LOG_POS : /* Receive log */
                              (void)strcpy(max_log_def, MAX_RECEIVE_LOG_FILES_DEF);
                              max_log_files = default_log_files = MAX_RECEIVE_LOG_FILES;
                              log_name_length = RECEIVE_LOG_NAME_LENGTH;
                              break;
#ifdef _DISTRIBUTION_LOG
                           case DIS_LOG_POS : /* Distribution log */
                              (void)strcpy(max_log_def, MAX_DISTRIBUTION_LOG_FILES_DEF);
                              max_log_files = default_log_files = MAX_DISTRIBUTION_LOG_FILES;
                              log_name_length = DISTRIBUTION_BUFFER_FILE_LENGTH;
                              break;
#endif
#ifdef _PRODUCTION_LOG
                           case PRO_LOG_POS : /* Production log */
                              (void)strcpy(max_log_def, MAX_PRODUCTION_LOG_FILES_DEF);
                              max_log_files = default_log_files = MAX_PRODUCTION_LOG_FILES;
                              log_name_length = PRODUCTION_BUFFER_FILE_LENGTH;
                              break;
#endif
#ifdef _DELETE_LOG
                           case DEL_LOG_POS : /* Delete log */
                              (void)strcpy(max_log_def, MAX_DELETE_LOG_FILES_DEF);
                              max_log_files = default_log_files = MAX_DELETE_LOG_FILES;
                              log_name_length = DELETE_BUFFER_FILE_LENGTH;
                              break;
#endif
                           case SYS_LOG_POS : /* System log */
                              (void)strcpy(max_log_def, MAX_SYSTEM_LOG_FILES_DEF);
                              max_log_files = default_log_files = MAX_SYSTEM_LOG_FILES;
                              log_name_length = SYSTEM_LOG_NAME_LENGTH;
                              break;
                           case EVE_LOG_POS : /* Event log */
                              (void)strcpy(max_log_def, MAX_EVENT_LOG_FILES_DEF);
                              max_log_files = default_log_files = MAX_EVENT_LOG_FILES;
                              log_name_length = EVENT_LOG_NAME_LENGTH;
                              break;
                           case TDB_LOG_POS : /* Transfer debug log */
                              (void)strcpy(max_log_def, MAX_TRANS_DB_LOG_FILES_DEF);
                              max_log_files = default_log_files = MAX_TRANS_DB_LOG_FILES;
                              log_name_length = TRANS_DB_LOG_NAME_LENGTH;
                              break;
                           default : /* Unknown log type */
                              system_log(ERROR_SIGN, __FILE__, __LINE__,
                                         "This cannot happen! Get a new computer!");
                              *bytes_buffered = 0;
                              return;
                        }
                        get_max_log_values(&max_log_files, max_log_def,
                                           default_log_files, NULL, NULL, 0,
                                           MON_CONFIG_FILE);
                        get_log_number(&log_number, (max_log_files - 1),
                                       log_name, log_name_length, p_mon_alias);
                        if (log_number < (max_log_files - 1))
                        {
                           log_number++;
                        }
                        p_end = p_log_dir + log_name_length;
                        (void)strcpy(p_log_dir, log_name);
#ifdef _LOG_DEBUG
                        mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, NULL,
                                "Reshuffel log file for %s (log_number=%d max_log_files=%d log_pos=%d shift=%d shift_offset=%d)",
                                log_dir, log_number, max_log_files,
                                log_pos, shift, shift_offset);
#endif
                        reshuffel_log_files(log_number, log_dir, p_end,
                                            shift, shift_offset);
                     }
                     check_create_log_file(log_name, log_pos);
                  }
               }
               else
               {
#ifdef _LOG_DEBUG
                  mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, NULL,
                          "Unable to locate start of O%c message type. Buffering %d bytes.",
                          log_data_buffer[i + 1], bytes_read - start_i);
#endif
                  if (start_i != 0)
                  {
                     *bytes_buffered += (bytes_read - start_i);
                     (void)memmove(log_data_buffer,
                                   &log_data_buffer[start_i],
                                   (bytes_read - start_i));
                  }
                  else
                  {
                     *bytes_buffered += bytes_read;
                  }
                  return;
               }
            }
            break;

         default  : /* Unknown, discard buffer. */
            mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, NULL,
                    "Reading garbage! Discarding %d bytes.", bytes_read - i);
            *bytes_buffered = 0;
            return;
      }
   } while (i < bytes_read);

   *bytes_buffered = 0;

   return;
}


/*---------------------------- check_inode() ----------------------------*/
static int
check_inode(char *p_inode_str,
            char *log_name,
            int  log_pos,
            int  *shift,
            int  *shift_offset)
{
   int  log_no_pos,
        i;
#ifdef DEBUG_LOG_CMD
   char debug_buffer[MAX_PATH_LENGTH],
        *ptr_read,
        *ptr_write;
#endif

   *shift = 0;
   *shift_offset = 0;
   if (cur_ino_log_no_str[log_pos][0] == '\0')
   {
      /* Read inode and log number from local file. */
      get_cur_ino_log_no(log_name, log_pos);
   }
   i = 0;
   log_no_pos = 0;
   while ((*(p_inode_str + i) == cur_ino_log_no_str[log_pos][i]) &&
          (i < MAX_INODE_LOG_NO_LENGTH))
   {
      if (*(p_inode_str + i) == ' ')
      {
         log_no_pos = i;
      }
      i++;
   }
   if (i == MAX_INODE_LOG_NO_LENGTH)
   {
      mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
              "Remode inode and log number string to long!");
      return(INCORRECT);
   }
   if ((*(p_inode_str + i) == '\r') && (cur_ino_log_no_str[log_pos][i] == '\0'))
   {
      i = SUCCESS;
   }
   else
   {
      int fd,
          ret;

      if (cur_ino_log_no_str[log_pos][0] != '\0')
      {
         int j;

         j = strlen(cur_ino_log_no_str[log_pos]) - 1 - 1;
         if (log_no_pos == 0)
         {

            /*
             * See if we need to reshuffle the log files. This is only the
             * the case when the inode has changed and our local log number
             * is 0.
             */
            if ((cur_ino_log_no_str[log_pos][j] == ' ') &&
                (cur_ino_log_no_str[log_pos][j + 1] == '0'))
            {
               ret = LOG_RESHUFFEL;
            }
            else
            {
               ret = LOG_STALE;
            }
         }
         else
         {
            int gotcha = NO,
                tmp_j;

            while ((cur_ino_log_no_str[log_pos][j] != ' ') && (j > 0))
            {
               j--;
            }
            if (cur_ino_log_no_str[log_pos][j] != ' ')
            {
               mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                       "Failed to locate inode and log number separator!");
               return(INCORRECT);
            }
            tmp_j = j;
            i = log_no_pos;
            while ((*(p_inode_str + i) != '\r') &&
                   (i < MAX_INODE_LOG_NO_LENGTH))
            {
               if (*(p_inode_str + i) != cur_ino_log_no_str[log_pos][j])
               {
                  gotcha = YES;
                  break;
               }
               i++; j++;
            }
            if (gotcha == YES)
            {
               char str_number[MAX_INT_LENGTH];

               *shift_offset = atoi(&cur_ino_log_no_str[log_pos][tmp_j + 1]);
               i = log_no_pos + 1;
               j = 0;
               while (*(p_inode_str + i) != '\r')
               {
                  str_number[j] = *(p_inode_str + i);
                  i++; j++;
               }
               if (j > 0)
               {
                  str_number[j] = '\0';
                  *shift = atoi(str_number);
               }
               ret = LOG_RESHUFFEL;
            }
            else
            {
               ret = LOG_STALE;
            }
         }
      }
      else
      {
         ret = LOG_STALE;
      }
      i = 0;
      while ((*(p_inode_str + i) != '\r') && (i < MAX_INODE_LOG_NO_LENGTH))
      {
         cur_ino_log_no_str[log_pos][i] = *(p_inode_str + i);
         i++;
      }
      if (i == MAX_INODE_LOG_NO_LENGTH)
      {
         mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                 "Remode inode and log number string to long!");
         return(INCORRECT);
      }
      cur_ino_log_no_str[log_pos][i] = '\0';
      write_remote_log_inode(log_name, i, log_pos);
      (void)sprintf(p_log_dir, "%s%s", log_name, REMOTE_INODE_EXTENSION);
      if ((fd = open(log_dir, O_WRONLY | O_CREAT | O_TRUNC, FILE_MODE)) == -1)
      {
         mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                 "Failed to open() `%s' : %s", log_dir, strerror(errno));
      }
      else
      {
         cur_ino_log_no_str[log_pos][i] = '\n';
         if (write(fd, cur_ino_log_no_str[log_pos], (i + 1)) != (i + 1))
         {
            mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                    "Failed to write() %d bytes to `%s' : %s",
                    i + 1, log_dir, strerror(errno));
         }
         cur_ino_log_no_str[log_pos][i] = '\0';
         if (close(fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Failed to close() `%s' : %s", log_dir, strerror(errno));
         }
      }
      i = ret;
   }
#ifdef DEBUG_LOG_CMD
   ptr_read = p_inode_str - 3;
   ptr_write = debug_buffer;
   while (*ptr_read != '\r')
   {
      *ptr_write = *ptr_read;
      ptr_read++; ptr_write++;
   }
   if (*ptr_read == '\r')
   {
      *ptr_write = '\0';
      mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, NULL,
              "R-> %s (returning %d)", debug_buffer, i);
   }
   else
   {
      mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, NULL,
              "X-> Reading garbage! (returning %d)", i);
   }
#endif

   return(i);
}


/*----------------------- check_create_log_file() -----------------------*/
static void
check_create_log_file(char *log_name, int log_pos)
{
   if (cur_ino_log_no_str[log_pos][0] != '\0')
   {
      int i,
          length;

      length = sprintf(p_log_dir, "%s", log_name);
      i = 0;
      while ((i < (MAX_INODE_LOG_NO_LENGTH - 1)) &&
             (cur_ino_log_no_str[log_pos][i] != ' '))
      {
         i++;
      }
      if ((i > 0) && (i < MAX_INODE_LOG_NO_LENGTH))
      {
         i++;
         while ((i < (MAX_INODE_LOG_NO_LENGTH - 1)) &&
               (cur_ino_log_no_str[log_pos][i] != '\0'))
         {
            *(p_log_dir + length) = cur_ino_log_no_str[log_pos][i];
            i++; length++;
         }
         if (cur_ino_log_no_str[log_pos][i] == '\0')
         {
            int fd;

            *(p_log_dir + length) = '\0';
            if ((fd = open(log_dir, O_WRONLY | O_CREAT, FILE_MODE)) == -1)
            {
               mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                       "Failed to open() `%s' : %s",
                       log_dir, strerror(errno));
            }
            else
            {
               (void)close(fd);
            }
         }
      }
   }

   return;
}


/*---------------------- write_remote_log_inode() -----------------------*/
static void
write_remote_log_inode(char *log_name, int length, int log_pos)
{
   int fd;

   (void)sprintf(p_log_dir, "%s%s", log_name, REMOTE_INODE_EXTENSION);
   if ((fd = open(log_dir, O_WRONLY | O_CREAT | O_TRUNC, FILE_MODE)) == -1)
   {
      mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
              "Failed to open() `%s' : %s", log_dir, strerror(errno));
   }
   else
   {
      cur_ino_log_no_str[log_pos][length] = '\n';
      if (write(fd, cur_ino_log_no_str[log_pos], (length + 1)) != (length + 1))
      {
         mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                 "Failed to write() %d bytes to `%s' : %s",
                 length + 1, log_dir, strerror(errno));
      }
      cur_ino_log_no_str[log_pos][length] = '\0';
      if (close(fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Failed to close() `%s' : %s", log_dir, strerror(errno));
      }
   }
   return;
}


/*----------------------- get_cur_ino_log_no() --------------------------*/
static void
get_cur_ino_log_no(char *log_name, int log_pos)
{
   int fd;

   (void)sprintf(p_log_dir, "%s%s", log_name, REMOTE_INODE_EXTENSION);
   if ((fd = open(log_dir, O_RDONLY)) == -1)
   {
      if (errno != ENOENT)
      {
         mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                 "Failed to open() `%s' : %s", log_dir, strerror(errno));
      }
   }
   else
   {
      char buffer[MAX_INODE_LOG_NO_LENGTH];

      if (read(fd, buffer, MAX_INODE_LOG_NO_LENGTH) < 0)
      {
         mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                 "Failed to read() from `%s' : %s", log_dir, strerror(errno));
      }
      else
      {
         int i = 0;

         while ((i < MAX_LONG_LONG_LENGTH) && (buffer[i] != ' '))
         {
            cur_ino_log_no_str[log_pos][i] = buffer[i];
            i++;
         }
         if ((i > 0) && (i < MAX_LONG_LONG_LENGTH))
         {
            while ((i < (MAX_INODE_LOG_NO_LENGTH - 1)) && (buffer[i] != '\n'))
            {
               cur_ino_log_no_str[log_pos][i] = buffer[i];
               i++;
            }
            if (buffer[i] == '\n')
            {
               cur_ino_log_no_str[log_pos][i] = '\0';
               if (close(fd) == -1)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Failed to close() `%s' : %s",
                             log_dir, strerror(errno));
               }
               return;
            }
            else
            {
               mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                       "Failed to find the log number!");
            }
         }
         else
         {
            mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, NULL,
                    "Failed to locate the log number! (%d)", i);
         }
      }
      if (close(fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Failed to close() `%s' : %s", log_dir, strerror(errno));
      }
   }
   cur_ino_log_no_str[log_pos][0] = '\0';

   return;
}


/*++++++++++++++++++++++++++++ log_mon_exit() +++++++++++++++++++++++++++*/
static void
log_mon_exit(void)
{
   int i;

   if (tcp_quit() < 0)
   {
      mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, NULL,
              "Failed to close TCP connection.");
   }
   mon_log(INFO_SIGN, NULL, 0, 0L, NULL, "========> Log Disconnect <========");

   for (i = 0; i < NO_OF_LOGS; i++)
   {
      if (log_fd[i] > -1)
      {
         (void)close(log_fd[i]);
      }
   }

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
