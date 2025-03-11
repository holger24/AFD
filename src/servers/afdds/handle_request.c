/*
 *  handle_request.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2025 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   handle_request - handles a TCP request
 **
 ** SYNOPSIS
 **   void handle_request(SSL  *ssl,
 **                       int  sock_fd,
 **                       int  pos,
 **                       int  trusted_ip_pos,
 **                       char *remote_ip_str)
 **
 ** DESCRIPTION
 **   Handles all request from remote user in a loop. If user is inactive
 **   for AFDD_CMD_TIMEOUT seconds, the connection will be closed.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.01.1999 H.Kiehl Created
 **   06.04.2005 H.Kiehl Open FSA here and not in afdd.c.
 **   23.11.2008 H.Kiehl Added danger_no_of_jobs.
 **   22.03.2014 H.Kiehl Added typesize data information (TD).
 **   31.03.2017 H.Kiehl Do not count the data from group identifiers.
 */
DESCR__E_M3

#include <stdio.h>            /* fprintf()                               */
#include <string.h>           /* memset()                                */
#include <stdlib.h>           /* atoi(), strtoul()                       */
#include <time.h>             /* time()                                  */
#include <ctype.h>            /* toupper()                               */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>           /* close()                                 */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <netdb.h>
#include <errno.h>
#include "afddsdefs.h"
#include "version.h"
#include "logdefs.h"

/* #define DEBUG_LOG_CMD */

/* Global variables. */
int                        cmd_sd,
                           fra_fd = -1,
                           fra_id,
                           fsa_fd = -1,
                           fsa_id,
                           host_config_counter,
                           in_log_child = NO,
                           no_of_dirs,
                           no_of_hosts;
#ifdef HAVE_MMAP
off_t                      fra_size,
                           fsa_size;
#endif
char                       *line_buffer = NULL,
                           log_dir[MAX_PATH_LENGTH],
                           *log_buffer = NULL,
                           *p_log_dir;
unsigned char              **old_error_history;
SSL                        *cmdssl;
struct filetransfer_status *fsa;
struct fileretrieve_status *fra;


/* External global variables. */
extern int                 *ip_log_defs,
                           log_defs;
extern long                danger_no_of_jobs;
extern char                afd_name[],
                           hostname[],
                           *p_work_dir,
                           *p_work_dir_end;
extern struct logdata      ld[];
extern struct afd_status   *p_afd_status;

/* Local global variables. */
static int                 report_changes = NO;

/*  Local function prototypes. */
static void                report_shutdown(void);


/*########################### handle_request() ##########################*/
void
handle_request(SSL  *ssl,
               int  sock_sd,
               int  pos,
               int  trusted_ip_pos,
               char *remote_ip_str)
{
   register int   i,
                  j;
   int            nbytes,
                  status;
   long           log_interval = 0;
   time_t         last,
                  last_time_read,
                  now,
                  report_changes_interval = DEFAULT_CHECK_INTERVAL;
   char           cmd[1024];
   fd_set         rset;
   struct timeval timeout;

   if (fsa_attach_passive(NO, AFDDS) != SUCCESS)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__, _("Failed to attach to FSA."));
      exit(INCORRECT);
   }
   host_config_counter = (int)*(unsigned char *)((char *)fsa - AFD_WORD_OFFSET +
                                                 SIZEOF_INT);
   RT_ARRAY(old_error_history, no_of_hosts, ERROR_HISTORY_LENGTH,
            unsigned char);
   for (i = 0; i < no_of_hosts; i++)
   {
      if (fsa[i].real_hostname[0][0] == GROUP_IDENTIFIER)
      {
         (void)memset(old_error_history[i], 0, ERROR_HISTORY_LENGTH);
      }
      else
      {
         (void)memcpy(old_error_history[i], fsa[i].error_history,
                      ERROR_HISTORY_LENGTH);
      }
   }
   if (fra_attach_passive() != SUCCESS)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__, _("Failed to attach to FRA."));
      exit(INCORRECT);
   }

   status = 0;
   while (p_afd_status->amg_jobs & WRITTING_JID_STRUCT)
   {
      (void)my_usleep(100000L);
      status++;
      if ((status > 1) && ((status % 100) == 0))
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Timeout arrived for waiting for AMG to finish writting to JID structure."));
      }
   }

   cmd_sd = sock_sd;
   cmdssl = ssl;
   if (atexit(report_shutdown) != 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not register exit handler : %s"), strerror(errno));
   }

   /* Give some information to the user where he is and what service */
   /* is offered here.                                               */
   (void)command(ssl, "220 %s AFD server %s (Version %s) ready.",
                 hostname, afd_name, PACKAGE_VERSION);

   init_get_display_data();

   /*
    * Handle all request until the remote user has entered QUIT_CMD or
    * the connection was idle for AFDD_CMD_TIMEOUT seconds.
    */
   now = last = last_time_read = time(NULL);
   FD_ZERO(&rset);
   for (;;)
   {
      now = time(NULL);
      nbytes = 0;
      if (report_changes == YES)
      {
         if ((now - last) >= report_changes_interval)
         {
            check_changes(ssl);
            timeout.tv_sec = report_changes_interval;
            last = now = time(NULL);
         }
         else
         {
            timeout.tv_sec = report_changes_interval - (now - last);
            last = now;
         }
      }
      else
      {
         if (in_log_child == YES)
         {
            timeout.tv_sec = log_interval;
         }
         else
         {
            timeout.tv_sec = AFDD_CMD_TIMEOUT;
         }
      }

      if ((in_log_child == NO) && ((now - last_time_read) > AFDD_CMD_TIMEOUT))
      {
         (void)command(ssl,
                       "421 Timeout (%d seconds): closing connection.",
                       AFDD_CMD_TIMEOUT);
         break;
      }
      FD_SET(cmd_sd, &rset);
      timeout.tv_usec = 0;

      /* Wait for message x seconds and then continue. */
      status = select(cmd_sd + 1, &rset, NULL, NULL, &timeout);

      if (FD_ISSET(cmd_sd, &rset))
      {
         if ((nbytes = read(cmd_sd, cmd, 1024)) <= 0)
         {
            if (nbytes == 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          _("Remote hangup by %s"), remote_ip_str);
            }
            else
            {
#ifdef ECONNRESET
               if (errno == ECONNRESET)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             _("%s: read() error : %s"),
                             remote_ip_str, strerror(errno));
               }
               else
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("%s: read() error : %s"),
                             remote_ip_str, strerror(errno));
               }
#else
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("%s: read() error : %s"),
                          remote_ip_str, strerror(errno));
#endif
            }
            break;
         }
         last_time_read = time(NULL);
      }
      else if (status == 0)
           {
              if (report_changes == YES)
              {
                 /*
                  * Check if there have been any changes. If no value has changed
                  * just be silent and do nothing.
                  */
                 check_changes(ssl);
              }
              else if (in_log_child == YES)
                   {
                      if (log_defs)
                      {
                         log_interval = check_logs(now + log_interval);
                      }
                   }
                   else
                   {
                      (void)command(ssl,
                                    "421 Timeout (%d seconds): closing connection.",
                                    AFDD_CMD_TIMEOUT);
                      break;
                   }
           }

      if (nbytes > 0)
      {
         i = 0;
         while ((i < nbytes) && (cmd[i] != ' ') && (cmd[i] != '\r') &&
                (cmd[i] != '\n'))
         {
            cmd[i] = toupper((int)cmd[i]);
            i++;
         }
         cmd[nbytes] = '\0';
         if (my_strcmp(cmd, QUIT_CMD) == 0)
         {
            (void)command(ssl, "221 Goodbye.");
            break;
         }
         else if (my_strcmp(cmd, HELP_CMD) == 0)
              {
                 (void)command(ssl,
                               "214- The following commands are recognized (* =>'s unimplemented).\r\n\
   *AFDSTAT *DISC    HELP    HSTAT    ILOG     *INFO    *LDB     LOG\r\n\
   LRF      NOP      OLOG    *PROC    QUIT     SLOG     STAT     TDLOG\r\n\
   TLOG     *TRACEF  *TRACEI *TRACEO  SSTAT\r\n\
214 Direct comments to %s",
                                  AFD_MAINTAINER);
              }
         else if ((cmd[0] == 'H') && (cmd[1] == 'E') && (cmd[2] == 'L') &&
                  (cmd[3] == 'P') && (cmd[4] == ' ') && (cmd[5] != '\r'))
              {
                 j = 5;
                 while ((cmd[j] != ' ') && (cmd[j] != '\r'))
                 {
                    cmd[j] = toupper((int)cmd[j]);
                    j++;
                 }

                 if (my_strcmp(&cmd[5], QUIT_CMD) == 0)
                 {
                    (void)command(ssl, "%s", QUIT_SYNTAX);
                 }
                 else if (my_strcmp(&cmd[5], HELP_CMD) == 0)
                      {
                         (void)command(ssl, "%s", HELP_SYNTAX);
                      }
                 else if (my_strcmp(&cmd[5], TRACEI_CMDL) == 0)
                      {
                         (void)command(ssl, "%s", TRACEI_SYNTAX);
                      }
                 else if (my_strcmp(&cmd[5], TRACEO_CMDL) == 0)
                      {
                         (void)command(ssl, "%s", TRACEO_SYNTAX);
                      }
                 else if (my_strcmp(&cmd[5], TRACEF_CMDL) == 0)
                      {
                         (void)command(ssl, "%s", TRACEF_SYNTAX);
                      }
                 else if (my_strcmp(&cmd[5], ILOG_CMDL) == 0)
                      {
                         (void)command(ssl, "%s", ILOG_SYNTAX);
                      }
                 else if (my_strcmp(&cmd[5], OLOG_CMDL) == 0)
                      {
                         (void)command(ssl, "%s", OLOG_SYNTAX);
                      }
                 else if (my_strcmp(&cmd[5], SLOG_CMDL) == 0)
                      {
                         (void)command(ssl, "%s", SLOG_SYNTAX);
                      }
                 else if (my_strcmp(&cmd[5], TLOG_CMDL) == 0)
                      {
                         (void)command(ssl, "%s", TLOG_SYNTAX);
                      }
                 else if (my_strcmp(&cmd[5], TDLOG_CMDL) == 0)
                      {
                         (void)command(ssl, "%s", TDLOG_SYNTAX);
                      }
                 else if (my_strcmp(&cmd[5], PROC_CMD) == 0)
                      {
                         (void)command(ssl, "%s", PROC_SYNTAX);
                      }
                 else if (my_strcmp(&cmd[5], DISC_CMD) == 0)
                      {
                         (void)command(ssl, "%s", DISC_SYNTAX);
                      }
                 else if (my_strcmp(&cmd[5], STAT_CMDL) == 0)
                      {
                         (void)command(ssl, "%s", STAT_SYNTAX);
                      }
                 else if (my_strcmp(&cmd[5], HSTAT_CMDL) == 0)
                      {
                         (void)command(ssl, "%s", HSTAT_SYNTAX);
                      }
                 else if (my_strcmp(&cmd[5], START_STAT_CMDL) == 0)
                      {
                         (void)command(ssl, "%s", START_STAT_SYNTAX);
                      }
                 else if (my_strcmp(&cmd[5], LDB_CMD) == 0)
                      {
                         (void)command(ssl, "%s", LDB_SYNTAX);
                      }
                 else if (my_strcmp(&cmd[5], LRF_CMD) == 0)
                      {
                         (void)command(ssl, "%s", LRF_SYNTAX);
                      }
                 else if (my_strcmp(&cmd[5], INFO_CMDL) == 0)
                      {
                         (void)command(ssl, "%s", INFO_SYNTAX);
                      }
                 else if (my_strcmp(&cmd[5], AFDSTAT_CMDL) == 0)
                      {
                         (void)command(ssl, "%s", AFDSTAT_SYNTAX);
                      }
                 else if (my_strcmp(&cmd[5], NOP_CMDL) == 0)
                      {
                         (void)command(ssl, "%s", NOP_SYNTAX);
                      }
                 else if (my_strcmp(&cmd[5], LOG_CMDL) == 0)
                      {
                         (void)command(ssl, "%s", LOG_SYNTAX);
                         (void)command(ssl, "%s", LOG_TYPES_SYNTAX);
                      }
                      else
                      {
                         *(cmd + nbytes - 2) = '\0';
                         (void)command(ssl, "502 Unknown command %s", &cmd[5]);
                      }
              }
              else if ((strncmp(cmd, ILOG_CMD, ILOG_CMD_LENGTH) == 0) ||
                       (strncmp(cmd, OLOG_CMD, OLOG_CMD_LENGTH) == 0) ||
                       (strncmp(cmd, SLOG_CMD, SLOG_CMD_LENGTH) == 0) ||
                       (strncmp(cmd, TLOG_CMD, TLOG_CMD_LENGTH) == 0) ||
                       (strncmp(cmd, TDLOG_CMD, TDLOG_CMD_LENGTH) == 0))
                   {
                      int  log_number;
                      char *p_search_file,
                           search_file[MAX_PATH_LENGTH];

                      /*
                       * For now lets just disable the following code.
                       */
                      (void)command(ssl, "503 Service disabled.");
                      break;

                      /*
                       * First lets determine what file we want and then
                       * create the full file name without the number.
                       */
                      p_search_file = search_file + snprintf(search_file,
                                                             MAX_PATH_LENGTH,
                                                             "%s%s/",
                                                             p_work_dir, LOG_DIR);
                      switch (cmd[0])
                      {
#ifdef _INPUT_LOG
                         case 'I' : /* Input log. */
                            (void)my_strncpy(p_search_file, INPUT_BUFFER_FILE,
                                             MAX_PATH_LENGTH - (p_search_file - search_file));
                            log_number = AFDD_ILOG_NO;
                            break;
#endif
#ifdef _OUTPUT_LOG
                         case 'O' : /* Output log. */
                            (void)my_strncpy(p_search_file, OUTPUT_BUFFER_FILE,
                                             MAX_PATH_LENGTH - (p_search_file - search_file));
                            log_number = AFDD_OLOG_NO;
                            break;
#endif
                         case 'S' : /* System log. */
                            (void)my_strncpy(p_search_file, SYSTEM_LOG_NAME,
                                             MAX_PATH_LENGTH - (p_search_file - search_file));
                            log_number = AFDD_SLOG_NO;
                            break;
                         case 'T' : /* Transfer or transfer debug log */
                            if (cmd[1] == 'D')
                            {
                               (void)my_strncpy(p_search_file, TRANS_DB_LOG_NAME,
                                                MAX_PATH_LENGTH - (p_search_file - search_file));
                               log_number = AFDD_TDLOG_NO;
                            }
                            else
                            {
                               (void)my_strncpy(p_search_file, TRANSFER_LOG_NAME,
                                                MAX_PATH_LENGTH - (p_search_file - search_file));
                               log_number = AFDD_TLOG_NO;
                            }
                            break;
                         default  : /* Impossible!!! */
                            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                       _("%s: Unknown error!"), remote_ip_str);
                            (void)command(ssl,
                                          "500 Unknown error. (%s %d)",
                                          __FILE__, __LINE__);
                            SSL_shutdown(ssl);
                            SSL_free(ssl);
                            exit(INCORRECT);
                      }

                      if (cmd[i] == ' ')
                      {
                         if ((cmd[i + 1] == '-') || (cmd[i + 1] == '+') ||
                             (cmd[i + 1] == '#') || (cmd[i + 1] == '%'))
                         {
                            int  k = 0,
                                 lines = EVERYTHING,
                                 show_time = EVERYTHING,
                                 start_line = NOT_SET,
                                 file_no = DEFAULT_FILE_NO,
                                 faulty = NO;
                            char numeric_str[MAX_INT_LENGTH];

                            do
                            {
                               i += k;
                               k = 0;
                               while ((cmd[i + 1 + k] != ' ') &&
                                      (cmd[i + 1 + k] != '\r') &&
                                      (k < MAX_INT_LENGTH))
                               {
                                  if (isdigit((int)cmd[i + 1 + k]))
                                  {
                                     numeric_str[k] = cmd[i + 1 + k];
                                  }
                                  else
                                  {
                                     faulty = YES;
                                     (void)command(ssl,
                                                   "500 Expecting numeric value after \'%c\'",
                                                   cmd[i + 1]);
                                     break;
                                  }
                                  k++;
                               }
                               if (k > 0)
                               {
                                  numeric_str[k] = '\0';
                                  switch (cmd[i + 1])
                                  {
                                     case '#' : /* File number. */
                                        file_no = atoi(numeric_str);
                                        break;
                                     case '-' : /* Number of lines. */
                                        lines = atoi(numeric_str);
                                        break;
                                     case '+' : /* Duration of displaying data. */
                                        show_time = atoi(numeric_str);
                                        break;
                                     case '%' : /* Start at line number. */
                                        start_line = atoi(numeric_str);
                                        break;
                                     default  : /* Impossible!!! */
                                        faulty = YES;
                                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                   _("%s: Unknown error!"),
                                                   remote_ip_str);
                                        (void)command(ssl,
                                                      "500 Unknown error. (%s %d)",
                                                      __FILE__, __LINE__);
                                        break;
                                  }
                               }
                               else
                               {
                                  faulty = YES;
                                  (void)command(ssl,
                                                "500 No numeric value supplied after \'%c\'",
                                                cmd[i + 1]);
                               }
                            } while ((cmd[i + 1 + k] != '\r') && (faulty == NO));

                            if (faulty == NO)
                            {
                               get_display_data(ssl, search_file, log_number,
                                                NULL, start_line, lines,
                                                show_time, file_no);
                            }
                         }
                         else if (isascii(cmd[i + 1]))
                              {
                                 int  k = 0;
                                 char *ptr,
			              search_string[256];

                                 /* User has supplied a search string. */
                                 ptr = &cmd[i + 1];
                                 while ((*ptr != ' ') && (*ptr != '\r') &&
                                        (*ptr != '\n'))
                                 {
                                    search_string[k] = *ptr;
                                    ptr++; k++;
                                 }
                                 if (*ptr == ' ')
                                 {
                                    search_string[k] = '\0';
                                    if ((cmd[i + k + 1] == '-') || (cmd[i + k + 1] == '+') ||
                                        (cmd[i + k + 1] == '#') || (cmd[i + k + 1] == '%'))
                                    {
                                       int  m = 0,
                                            lines = EVERYTHING,
                                            show_time = EVERYTHING,
                                            start_line = NOT_SET,
                                            file_no = DEFAULT_FILE_NO,
                                            faulty = NO;
                                       char numeric_str[MAX_INT_LENGTH];

                                       do
                                       {
                                          i += m;
                                          m = 0;
                                          if (cmd[i + k + 1 + m] == '*')
                                          {
                                             if (cmd[i + k + 1] == '#')
                                             {
                                                file_no = EVERYTHING;
                                             }
                                          }
                                          else
                                          {
                                             while ((cmd[i + k + 1 + m] != ' ') &&
                                                    (cmd[i + k + 1 + m] != '\r') &&
                                                    (m < MAX_INT_LENGTH))
                                             {
                                                if (isdigit((int)cmd[i + k + 1 + m]))
                                                {
                                                   numeric_str[m] = cmd[i + k + 1 + m];
                                                }
                                                else
                                                {
                                                   faulty = YES;
                                                   (void)command(ssl,
                                                                 "500 Expecting numeric value after \'%c\'",
                                                                 cmd[i + k + 1]);
                                                   break;
                                                }
                                                m++;
                                             }
                                             if (m > 0)
                                             {
                                                numeric_str[m] = '\0';
                                                switch (cmd[i + k + 1])
                                                {
                                                   case '#' : /* File number. */
                                                      file_no = atoi(numeric_str);
                                                      break;
                                                   case '-' : /* Number of lines. */
                                                      lines = atoi(numeric_str);
                                                      break;
                                                   case '+' : /* Duration of displaying data. */
                                                      show_time = atoi(numeric_str);
                                                      break;
                                                   case '%' : /* Start at line number. */
                                                      start_line = atoi(numeric_str);
                                                      break;
                                                   default  : /* Impossible!!! */
                                                      faulty = YES;
                                                      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                                 _("%s: Unknown error!"),
                                                                 remote_ip_str);
                                                      (void)command(ssl,
                                                                    "500 Unknown error. (%s %d)",
                                                                    __FILE__, __LINE__);
                                                      break;
                                                }
                                             }
                                             else
                                             {
                                                faulty = YES;
                                                (void)command(ssl,
                                                              "500 No numeric value supplied after \'%c\'",
                                                              cmd[i + k + 1]);
                                             }
                                          }
                                       } while ((cmd[i + k + 1 + m] != '\r') && (faulty == NO));

                                       if (faulty == NO)
                                       {
                                          get_display_data(ssl, search_file,
                                                           log_number,
                                                           search_string,
                                                           start_line,
                                                           lines,
                                                           show_time,
                                                           file_no);
                                       }
                                    }
                                    else
                                    {
                                       *(cmd + nbytes - 2) = '\0';
                                       (void)command(ssl,
                                                     "500 \'%s\': Syntax wrong (see HELP).",
                                                     cmd);
                                    }
                                 }
                              }
                              else
                              {
                                 *(cmd + nbytes - 2) = '\0';
                                 (void)command(ssl,
                                               "500 \'%s\': command not understood.",
                                               cmd);
                              }
                      } /* if (cmd[i] == ' ') */
                      else if (cmd[i] == '\r')
                           {
                              get_display_data(ssl, search_file, log_number,
                                               NULL, NOT_SET, EVERYTHING,
                                               EVERYTHING, DEFAULT_FILE_NO);
                           }
                           else
                           {
                               *(cmd + nbytes - 2) = '\0';
                               (void)command(ssl,
                                             "500 \'%s\': command not understood.",
                                             cmd);
                           }
                   }
              else if (strncmp(cmd, STAT_CMD, STAT_CMD_LENGTH) == 0)
                   {
                      show_summary_stat(ssl);
                   }
              else if (strncmp(cmd, HSTAT_CMD, HSTAT_CMD_LENGTH) == 0)
                   {
                      show_host_stat(ssl);
                   }
              else if (strncmp(cmd, START_STAT_CMD, START_STAT_CMD_LENGTH) == 0)
                   {
                      show_summary_stat(ssl);
                      show_host_list(ssl);
                      show_dir_list(ssl);
                      show_job_list(ssl);
                      (void)command(ssl,
                                    "LC %d\r\nWD %s\r\nAV %s\r\nDJ %ld\r\nTD %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
                                    ip_log_defs[trusted_ip_pos], p_work_dir,
                                    PACKAGE_VERSION, danger_no_of_jobs,
                                    MAX_MSG_NAME_LENGTH, MAX_FILENAME_LENGTH,
                                    MAX_HOSTNAME_LENGTH, MAX_REAL_HOSTNAME_LENGTH,
                                    MAX_PROXY_NAME_LENGTH, MAX_TOGGLE_STR_LENGTH,
                                    ERROR_HISTORY_LENGTH, MAX_NO_PARALLEL_JOBS,
                                    MAX_DIR_ALIAS_LENGTH, MAX_RECIPIENT_LENGTH,
                                    MAX_WAIT_FOR_LENGTH, MAX_FRA_TIME_ENTRIES,
                                    MAX_OPTION_LENGTH, MAX_PATH_LENGTH,
                                    MAX_USER_NAME_LENGTH, MAX_TIMEZONE_LENGTH);
                      report_changes = YES;
                   }
              else if (strncmp(cmd, NOP_CMD, NOP_CMD_LENGTH) == 0)
                   {
                      (void)command(ssl, "200 OK\r\n");
                   }
              else if (strncmp(cmd, LRF_CMD, LRF_CMD_LENGTH) == 0)
                   {
                      (void)snprintf(p_work_dir_end,
                                     MAX_PATH_LENGTH - (p_work_dir_end - p_work_dir),
                                     "%s%s", ETC_DIR, RENAME_RULE_FILE);
                      display_file(ssl);
                      *p_work_dir_end = '\0';
                   }
              else if (strncmp(cmd, LOG_CMD, LOG_CMD_LENGTH) == 0)
                   {
                      int  complete_failure = NO,
                           tmp_log_defs;
                      char *log_type_ptr,
                            *ptr;
#ifdef DEBUG_LOG_CMD
                      int  cmd_buffer_length;
                      char cmd_buffer[3 + 1 + 2 + 1 + (NO_OF_LOGS * (MAX_INT_LENGTH + 1 + MAX_LONG_LONG_LENGTH + 1 + MAX_LONG_LONG_LENGTH + 1))];
#endif

                      tmp_log_defs = log_defs;
                      log_defs = 0;
#ifdef DEBUG_LOG_CMD
                      (void)strcpy(cmd_buffer, LOG_CMD);
                      cmd_buffer_length = LOG_CMD_LENGTH;
#endif
                      ptr = cmd + LOG_CMD_LENGTH;
                      do
                      {
                         if ((*(ptr + 1) == 'L') && (*(ptr + 3) == ' '))
                         {
                            char *p_start;

                            log_type_ptr = ptr + 2;
                            ptr += 4;
                            p_start = ptr;
                            while (isdigit((int)(*ptr)))
                            {
                               ptr++;
                            }
                            if (*ptr == ' ')
                            {
                               *ptr = '\0';
                               ptr++;
                               ld[DUM_LOG_POS].options = (unsigned int)strtoul(p_start, NULL, 10);

                               p_start = ptr;
                               while (isdigit((int)(*ptr)))
                               {
                                  ptr++;
                               }
                               if (*ptr == ' ')
                               {
                                  *ptr = '\0';
                                  ptr++;
                                  ld[DUM_LOG_POS].current_log_inode = (ino_t)str2inot(p_start, NULL, 10);

                                  p_start = ptr;
                                  while (isdigit((int)(*ptr)))
                                  {
                                     ptr++;
                                  }
                                  if ((*ptr == ' ') ||
                                      ((*ptr == '\r') && (*(ptr + 1) == '\n')))
                                  {
                                     int end_reached;

                                     if ((*ptr == ' ') && (*(ptr + 1) == 'L'))
                                     {
                                        end_reached = NO;
                                     }
                                     else
                                     {
                                        end_reached = YES;
                                     }
                                     *ptr = '\0';
                                     ld[DUM_LOG_POS].offset = (off_t)str2offt(p_start, NULL, 10);
                                     ld[DUM_LOG_POS].flag = 0;
                                     if (end_reached == NO)
                                     {
                                        *ptr = ' ';
                                     }
                                  }
                               }
                            }
#ifdef DEBUG_LOG_CMD
                            cmd_buffer_length += snprintf(&cmd_buffer[cmd_buffer_length],
                                                          (3 + 1 + 2 + 1 + (NO_OF_LOGS * (MAX_INT_LENGTH + 1 + MAX_LONG_LONG_LENGTH + 1 + MAX_LONG_LONG_LENGTH + 1))) - cmd_buffer_length,
# if SIZEOF_INO_T == 4
#  if SIZEOF_OFF_T == 4
                                       " L%c %u %ld %ld",
#  else
                                       " L%c %u %ld %lld",
#  endif
# else
#  if SIZEOF_OFF_T == 4
                                       " L%c %u %lld %ld",
#  else
                                       " L%c %u %lld %lld",
#  endif
# endif
                                       *log_type_ptr, ld[DUM_LOG_POS].options,
                                       (pri_ino_t)ld[DUM_LOG_POS].current_log_inode,
                                       (pri_off_t)ld[DUM_LOG_POS].offset);
                            if (cmd_buffer_length > ((3 + 1 + 2 + 1 + (NO_OF_LOGS * (MAX_INT_LENGTH + 1 + MAX_LONG_LONG_LENGTH + 1 + MAX_LONG_LONG_LENGTH + 1))) - cmd_buffer_length))
                            {
                               system_log(WARN_SIGN, __FILE__, __LINE__,
                                          "%s: Buffer to small (%d > %d).",
                                          remote_ip_str, cmd_buffer_length,
                                          (3 + 1 + 2 + 1 + (NO_OF_LOGS * (MAX_INT_LENGTH + 1 + MAX_LONG_LONG_LENGTH + 1 + MAX_LONG_LONG_LENGTH + 1))) - cmd_buffer_length);
                               cmd_buffer_length = (3 + 1 + 2 + 1 + (NO_OF_LOGS * (MAX_INT_LENGTH + 1 + MAX_LONG_LONG_LENGTH + 1 + MAX_LONG_LONG_LENGTH + 1))) - cmd_buffer_length;
                            }
#endif
                            switch (*log_type_ptr)
                            {
                               case 'S' : /* System Log. */
                                  if (ip_log_defs[trusted_ip_pos] & AFDD_SYSTEM_LOG)
                                  {
                                     ld[SYS_LOG_POS].options = ld[DUM_LOG_POS].options;
                                     ld[SYS_LOG_POS].current_log_inode = ld[DUM_LOG_POS].current_log_inode;
                                     ld[SYS_LOG_POS].offset = ld[DUM_LOG_POS].offset;
                                     ld[SYS_LOG_POS].flag = ld[DUM_LOG_POS].flag;
                                     (void)strcpy(ld[SYS_LOG_POS].log_name, SYSTEM_LOG_NAME);
                                     ld[SYS_LOG_POS].log_name_length = SYSTEM_LOG_NAME_LENGTH;
                                     ld[SYS_LOG_POS].log_data_cmd[0] = 'L';
                                     ld[SYS_LOG_POS].log_data_cmd[1] = 'S';
                                     ld[SYS_LOG_POS].log_data_cmd[2] = '\0';
                                     ld[SYS_LOG_POS].log_inode_cmd[0] = 'O';
                                     ld[SYS_LOG_POS].log_inode_cmd[1] = 'S';
                                     ld[SYS_LOG_POS].log_inode_cmd[2] = '\0';
                                     ld[SYS_LOG_POS].log_flag = AFDD_SYSTEM_LOG;
                                     ld[SYS_LOG_POS].fp = NULL;
                                     ld[SYS_LOG_POS].current_log_no = 0;
                                     ld[SYS_LOG_POS].packet_no = 0;
                                     if ((log_defs & AFDD_SYSTEM_LOG) == 0)
                                     {
                                        log_defs |= AFDD_SYSTEM_LOG;
                                     }
                                  }
                                  else
                                  {
                                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                _("Host %s was denied access for %s"),
                                                remote_ip_str, SYSTEM_LOG_NAME);
                                  }
                                  break;
                               case 'E' : /* Event Log. */
                                  if (ip_log_defs[trusted_ip_pos] & AFDD_EVENT_LOG)
                                  {
                                     ld[EVE_LOG_POS].options = ld[DUM_LOG_POS].options;
                                     ld[EVE_LOG_POS].current_log_inode = ld[DUM_LOG_POS].current_log_inode;
                                     ld[EVE_LOG_POS].offset = ld[DUM_LOG_POS].offset;
                                     ld[EVE_LOG_POS].flag = ld[DUM_LOG_POS].flag;
                                     (void)strcpy(ld[EVE_LOG_POS].log_name, EVENT_LOG_NAME);
                                     ld[EVE_LOG_POS].log_name_length = EVENT_LOG_NAME_LENGTH;
                                     ld[EVE_LOG_POS].log_data_cmd[0] = 'L';
                                     ld[EVE_LOG_POS].log_data_cmd[1] = 'E';
                                     ld[EVE_LOG_POS].log_data_cmd[2] = '\0';
                                     ld[EVE_LOG_POS].log_inode_cmd[0] = 'O';
                                     ld[EVE_LOG_POS].log_inode_cmd[1] = 'E';
                                     ld[EVE_LOG_POS].log_inode_cmd[2] = '\0';
                                     ld[EVE_LOG_POS].log_flag = AFDD_EVENT_LOG;
                                     ld[EVE_LOG_POS].fp = NULL;
                                     ld[EVE_LOG_POS].current_log_no = 0;
                                     ld[EVE_LOG_POS].packet_no = 0;
                                     if ((log_defs & AFDD_EVENT_LOG) == 0)
                                     {
                                        log_defs |= AFDD_EVENT_LOG;
                                     }
                                  }
                                  else
                                  {
                                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                _("Host %s was denied access for %s"),
                                                remote_ip_str, EVENT_LOG_NAME);
                                  }
                                  break;
                               case 'R' : /* Retrieve Log. */
                                  if (ip_log_defs[trusted_ip_pos] & AFDD_RECEIVE_LOG)
                                  {
                                     ld[REC_LOG_POS].options = ld[DUM_LOG_POS].options;
                                     ld[REC_LOG_POS].current_log_inode = ld[DUM_LOG_POS].current_log_inode;
                                     ld[REC_LOG_POS].offset = ld[DUM_LOG_POS].offset;
                                     ld[REC_LOG_POS].flag = ld[DUM_LOG_POS].flag;
                                     (void)strcpy(ld[REC_LOG_POS].log_name, RECEIVE_LOG_NAME);
                                     ld[REC_LOG_POS].log_name_length = RECEIVE_LOG_NAME_LENGTH;
                                     ld[REC_LOG_POS].log_data_cmd[0] = 'L';
                                     ld[REC_LOG_POS].log_data_cmd[1] = 'R';
                                     ld[REC_LOG_POS].log_data_cmd[2] = '\0';
                                     ld[REC_LOG_POS].log_inode_cmd[0] = 'O';
                                     ld[REC_LOG_POS].log_inode_cmd[1] = 'R';
                                     ld[REC_LOG_POS].log_inode_cmd[2] = '\0';
                                     ld[REC_LOG_POS].log_flag = AFDD_RECEIVE_LOG;
                                     ld[REC_LOG_POS].fp = NULL;
                                     ld[REC_LOG_POS].current_log_no = 0;
                                     ld[REC_LOG_POS].packet_no = 0;
                                     if ((log_defs & AFDD_RECEIVE_LOG) == 0)
                                     {
                                        log_defs |= AFDD_RECEIVE_LOG;
                                     }
                                  }
                                  else
                                  {
                                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                _("Host %s was denied access for %s"),
                                                remote_ip_str, RECEIVE_LOG_NAME);
                                  }
                                  break;
                               case 'T' : /* Transfer Log. */
                                  if (ip_log_defs[trusted_ip_pos] & AFDD_TRANSFER_LOG)
                                  {
                                     ld[TRA_LOG_POS].options = ld[DUM_LOG_POS].options;
                                     ld[TRA_LOG_POS].current_log_inode = ld[DUM_LOG_POS].current_log_inode;
                                     ld[TRA_LOG_POS].offset = ld[DUM_LOG_POS].offset;
                                     ld[TRA_LOG_POS].flag = ld[DUM_LOG_POS].flag;
                                     (void)strcpy(ld[TRA_LOG_POS].log_name, TRANSFER_LOG_NAME);
                                     ld[TRA_LOG_POS].log_name_length = TRANSFER_LOG_NAME_LENGTH;
                                     ld[TRA_LOG_POS].log_data_cmd[0] = 'L';
                                     ld[TRA_LOG_POS].log_data_cmd[1] = 'T';
                                     ld[TRA_LOG_POS].log_data_cmd[2] = '\0';
                                     ld[TRA_LOG_POS].log_inode_cmd[0] = 'O';
                                     ld[TRA_LOG_POS].log_inode_cmd[1] = 'T';
                                     ld[TRA_LOG_POS].log_inode_cmd[2] = '\0';
                                     ld[TRA_LOG_POS].log_flag = AFDD_TRANSFER_LOG;
                                     ld[TRA_LOG_POS].fp = NULL;
                                     ld[TRA_LOG_POS].current_log_no = 0;
                                     ld[TRA_LOG_POS].packet_no = 0;
                                     if ((log_defs & AFDD_TRANSFER_LOG) == 0)
                                     {
                                        log_defs |= AFDD_TRANSFER_LOG;
                                     }
                                  }
                                  else
                                  {
                                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                _("Host %s was denied access for %s"),
                                                remote_ip_str, TRANSFER_LOG_NAME);
                                  }
                                  break;
                               case 'B' : /* Transfer Debug Log. */
                                  if (ip_log_defs[trusted_ip_pos] & AFDD_TRANSFER_DEBUG_LOG)
                                  {
                                     ld[TDB_LOG_POS].options = ld[DUM_LOG_POS].options;
                                     ld[TDB_LOG_POS].current_log_inode = ld[DUM_LOG_POS].current_log_inode;
                                     ld[TDB_LOG_POS].offset = ld[DUM_LOG_POS].offset;
                                     ld[TDB_LOG_POS].flag = ld[DUM_LOG_POS].flag;
                                     (void)strcpy(ld[TDB_LOG_POS].log_name, TRANS_DB_LOG_NAME);
                                     ld[TDB_LOG_POS].log_name_length = TRANS_DB_LOG_NAME_LENGTH;
                                     ld[TDB_LOG_POS].log_data_cmd[0] = 'L';
                                     ld[TDB_LOG_POS].log_data_cmd[1] = 'B';
                                     ld[TDB_LOG_POS].log_data_cmd[2] = '\0';
                                     ld[TDB_LOG_POS].log_inode_cmd[0] = 'O';
                                     ld[TDB_LOG_POS].log_inode_cmd[1] = 'B';
                                     ld[TDB_LOG_POS].log_inode_cmd[2] = '\0';
                                     ld[TDB_LOG_POS].log_flag = AFDD_TRANSFER_DEBUG_LOG;
                                     ld[TDB_LOG_POS].fp = NULL;
                                     ld[TDB_LOG_POS].current_log_no = 0;
                                     ld[TDB_LOG_POS].packet_no = 0;
                                     if ((log_defs & AFDD_TRANSFER_DEBUG_LOG) == 0)
                                     {
                                        log_defs |= AFDD_TRANSFER_DEBUG_LOG;
                                     }
                                  }
                                  else
                                  {
                                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                _("Host %s was denied access for %s"),
                                                remote_ip_str, TRANS_DB_LOG_NAME);
                                  }
                                  break;
#ifdef _INPUT_LOG
                               case 'I' : /* INPUT Log. */
                                  if (ip_log_defs[trusted_ip_pos] & AFDD_INPUT_LOG)
                                  {
                                     ld[INP_LOG_POS].options = ld[DUM_LOG_POS].options;
                                     ld[INP_LOG_POS].current_log_inode = ld[DUM_LOG_POS].current_log_inode;
                                     ld[INP_LOG_POS].offset = ld[DUM_LOG_POS].offset;
                                     ld[INP_LOG_POS].flag = ld[DUM_LOG_POS].flag;
                                     (void)strcpy(ld[INP_LOG_POS].log_name, INPUT_BUFFER_FILE);
                                     ld[INP_LOG_POS].log_name_length = INPUT_BUFFER_FILE_LENGTH;
                                     ld[INP_LOG_POS].log_data_cmd[0] = 'L';
                                     ld[INP_LOG_POS].log_data_cmd[1] = 'I';
                                     ld[INP_LOG_POS].log_data_cmd[2] = '\0';
                                     ld[INP_LOG_POS].log_inode_cmd[0] = 'O';
                                     ld[INP_LOG_POS].log_inode_cmd[1] = 'I';
                                     ld[INP_LOG_POS].log_inode_cmd[2] = '\0';
                                     ld[INP_LOG_POS].log_flag = AFDD_INPUT_LOG;
                                     ld[INP_LOG_POS].fp = NULL;
                                     ld[INP_LOG_POS].current_log_no = 0;
                                     ld[INP_LOG_POS].packet_no = 0;
                                     if ((log_defs & AFDD_INPUT_LOG) == 0)
                                     {
                                        log_defs |= AFDD_INPUT_LOG;
                                     }
                                  }
                                  else
                                  {
                                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                _("Host %s was denied access for %s"),
                                                remote_ip_str,
                                                INPUT_BUFFER_FILE);
                                  }
                                  break;
#endif
#ifdef _DISTRIBUTION_LOG
                               case 'U' : /* DISTRIBUTION Log. */
                                  if (ip_log_defs[trusted_ip_pos] & AFDD_DISTRIBUTION_LOG)
                                  {
                                     ld[DIS_LOG_POS].options = ld[DUM_LOG_POS].options;
                                     ld[DIS_LOG_POS].current_log_inode = ld[DUM_LOG_POS].current_log_inode;
                                     ld[DIS_LOG_POS].offset = ld[DUM_LOG_POS].offset;
                                     ld[DIS_LOG_POS].flag = ld[DUM_LOG_POS].flag;
                                     (void)strcpy(ld[DIS_LOG_POS].log_name, DISTRIBUTION_BUFFER_FILE);
                                     ld[DIS_LOG_POS].log_name_length = DISTRIBUTION_BUFFER_FILE_LENGTH;
                                     ld[DIS_LOG_POS].log_data_cmd[0] = 'L';
                                     ld[DIS_LOG_POS].log_data_cmd[1] = 'U';
                                     ld[DIS_LOG_POS].log_data_cmd[2] = '\0';
                                     ld[DIS_LOG_POS].log_inode_cmd[0] = 'O';
                                     ld[DIS_LOG_POS].log_inode_cmd[1] = 'U';
                                     ld[DIS_LOG_POS].log_inode_cmd[2] = '\0';
                                     ld[DIS_LOG_POS].log_flag = AFDD_DISTRIBUTION_LOG;
                                     ld[DIS_LOG_POS].fp = NULL;
                                     ld[DIS_LOG_POS].current_log_no = 0;
                                     ld[DIS_LOG_POS].packet_no = 0;
                                     if ((log_defs & AFDD_DISTRIBUTION_LOG) == 0)
                                     {
                                        log_defs |= AFDD_DISTRIBUTION_LOG;
                                     }
                                  }
                                  else
                                  {
                                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                _("Host %s was denied access for %s"),
                                                remote_ip_str,
                                                DISTRIBUTION_BUFFER_FILE);
                                  }
                                  break;
#endif
#ifdef _PRODUCTION_LOG
                               case 'P' : /* Production Log. */
                                  if (ip_log_defs[trusted_ip_pos] & AFDD_PRODUCTION_LOG)
                                  {
                                     ld[PRO_LOG_POS].options = ld[DUM_LOG_POS].options;
                                     ld[PRO_LOG_POS].current_log_inode = ld[DUM_LOG_POS].current_log_inode;
                                     ld[PRO_LOG_POS].offset = ld[DUM_LOG_POS].offset;
                                     ld[PRO_LOG_POS].flag = ld[DUM_LOG_POS].flag;
                                     (void)strcpy(ld[PRO_LOG_POS].log_name, PRODUCTION_BUFFER_FILE);
                                     ld[PRO_LOG_POS].log_name_length = PRODUCTION_BUFFER_FILE_LENGTH;
                                     ld[PRO_LOG_POS].log_data_cmd[0] = 'L';
                                     ld[PRO_LOG_POS].log_data_cmd[1] = 'P';
                                     ld[PRO_LOG_POS].log_data_cmd[2] = '\0';
                                     ld[PRO_LOG_POS].log_inode_cmd[0] = 'O';
                                     ld[PRO_LOG_POS].log_inode_cmd[1] = 'P';
                                     ld[PRO_LOG_POS].log_inode_cmd[2] = '\0';
                                     ld[PRO_LOG_POS].log_flag = AFDD_PRODUCTION_LOG;
                                     ld[PRO_LOG_POS].fp = NULL;
                                     ld[PRO_LOG_POS].current_log_no = 0;
                                     ld[PRO_LOG_POS].packet_no = 0;
                                     if ((log_defs & AFDD_PRODUCTION_LOG) == 0)
                                     {
                                        log_defs |= AFDD_PRODUCTION_LOG;
                                     }
                                  }
                                  else
                                  {
                                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                _("Host %s was denied access for %s"),
                                                remote_ip_str,
                                                PRODUCTION_BUFFER_FILE);
                                  }
                                  break;
#endif
#ifdef _OUTPUT_LOG
                               case 'O' : /* Output Log. */
                                  if (ip_log_defs[trusted_ip_pos] & AFDD_OUTPUT_LOG)
                                  {
                                     ld[OUT_LOG_POS].options = ld[DUM_LOG_POS].options;
                                     ld[OUT_LOG_POS].current_log_inode = ld[DUM_LOG_POS].current_log_inode;
                                     ld[OUT_LOG_POS].offset = ld[DUM_LOG_POS].offset;
                                     ld[OUT_LOG_POS].flag = ld[DUM_LOG_POS].flag;
                                     (void)strcpy(ld[OUT_LOG_POS].log_name, OUTPUT_BUFFER_FILE);
                                     ld[OUT_LOG_POS].log_name_length = OUTPUT_BUFFER_FILE_LENGTH;
                                     ld[OUT_LOG_POS].log_data_cmd[0] = 'L';
                                     ld[OUT_LOG_POS].log_data_cmd[1] = 'O';
                                     ld[OUT_LOG_POS].log_data_cmd[2] = '\0';
                                     ld[OUT_LOG_POS].log_inode_cmd[0] = 'O';
                                     ld[OUT_LOG_POS].log_inode_cmd[1] = 'O';
                                     ld[OUT_LOG_POS].log_inode_cmd[2] = '\0';
                                     ld[OUT_LOG_POS].log_flag = AFDD_OUTPUT_LOG;
                                     ld[OUT_LOG_POS].fp = NULL;
                                     ld[OUT_LOG_POS].current_log_no = 0;
                                     ld[OUT_LOG_POS].packet_no = 0;
                                     if ((log_defs & AFDD_OUTPUT_LOG) == 0)
                                     {
                                        log_defs |= AFDD_OUTPUT_LOG;
                                     }
                                  }
                                  else
                                  {
                                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                _("Host %s was denied access for %s"),
                                                remote_ip_str,
                                                OUTPUT_BUFFER_FILE);
                                  }
                                  break;
#endif
#ifdef _DELETE_LOG
                               case 'D' : /* Delete Log. */
                                  if (ip_log_defs[trusted_ip_pos] & AFDD_DELETE_LOG)
                                  {
                                     ld[DEL_LOG_POS].options = ld[DUM_LOG_POS].options;
                                     ld[DEL_LOG_POS].current_log_inode = ld[DUM_LOG_POS].current_log_inode;
                                     ld[DEL_LOG_POS].offset = ld[DUM_LOG_POS].offset;
                                     ld[DEL_LOG_POS].flag = ld[DUM_LOG_POS].flag;
                                     (void)strcpy(ld[DEL_LOG_POS].log_name, DELETE_BUFFER_FILE);
                                     ld[DEL_LOG_POS].log_name_length = DELETE_BUFFER_FILE_LENGTH;
                                     ld[DEL_LOG_POS].log_data_cmd[0] = 'L';
                                     ld[DEL_LOG_POS].log_data_cmd[1] = 'D';
                                     ld[DEL_LOG_POS].log_data_cmd[2] = '\0';
                                     ld[DEL_LOG_POS].log_inode_cmd[0] = 'O';
                                     ld[DEL_LOG_POS].log_inode_cmd[1] = 'D';
                                     ld[DEL_LOG_POS].log_inode_cmd[2] = '\0';
                                     ld[DEL_LOG_POS].log_flag = AFDD_DELETE_LOG;
                                     ld[DEL_LOG_POS].fp = NULL;
                                     ld[DEL_LOG_POS].current_log_no = 0;
                                     ld[DEL_LOG_POS].packet_no = 0;
                                     if ((log_defs & AFDD_DELETE_LOG) == 0)
                                     {
                                        log_defs |= AFDD_DELETE_LOG;
                                     }
                                  }
                                  else
                                  {
                                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                _("Host %s was denied access for %s"),
                                                remote_ip_str,
                                                DELETE_BUFFER_FILE);
                                  }
                                  break;
#endif
                               default  : /* Unknown, lets just ignore it. */
                                  (void)command(ssl, "501- Unknown log type");
                                  break;
                            }
                         }
                         else
                         {
                            /* Unknown message type. We are unable to */
                            /* determine the end, so lets discard the */
                            /* complete message.                      */
                            (void)command(ssl, "501- Unknown log type");
                            log_defs = 0;
                            complete_failure = YES;
                            break;
                         }
                      } while (*ptr == ' ');

                      if (complete_failure == YES)
                      {
                         log_defs = tmp_log_defs;
                      }
                      else
                      {
                         (void)command(ssl, "211- Command success (%u)",
                                       log_defs);
                         in_log_child = YES;
                         log_interval = 0;
                         if (line_buffer == NULL)
                         {
                            if ((line_buffer = malloc(MAX_LOG_DATA_BUFFER)) == NULL)
                            {
                               system_log(ERROR_SIGN, __FILE__, __LINE__,
                                          _("Failed to malloc() %d bytes : %s"),
                                          MAX_LOG_DATA_BUFFER, strerror(errno));
                               exit(INCORRECT);
                            }
                         }
                         if (log_buffer == NULL)
                         {
                            if ((log_buffer = malloc(MAX_LOG_DATA_BUFFER)) == NULL)
                            {
                               system_log(ERROR_SIGN, __FILE__, __LINE__,
                                          _("Failed to malloc() %d bytes : %s"),
                                          MAX_LOG_DATA_BUFFER, strerror(errno));
                               exit(INCORRECT);
                            }
                         }
                         p_log_dir = log_dir + snprintf(log_dir, MAX_PATH_LENGTH,
                                                        "%s%s/",
                                                        p_work_dir, LOG_DIR);
#ifdef DEBUG_LOG_CMD
                         if (cmd_buffer_length > LOG_CMD_LENGTH)
                         {
                            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                       "%s: R-> %s", remote_ip_str, cmd_buffer);
                         }
#endif
                         /*
                          * Since we are from now on only handling log
                          * data lets release those resources we no longer
                          * need.
                          */
                         (void)fsa_detach(NO);
                         (void)fra_detach();
                      }
                   }
              else if (strncmp(cmd, TRACEI_CMD, TRACEI_CMD_LENGTH) == 0)
                   {
                      (void)command(ssl,
                                    "502 Service not implemented. See help for commands.");
                   }
              else if (strncmp(cmd, TRACEO_CMD, TRACEO_CMD_LENGTH) == 0)
                   {
                      (void)command(ssl,
                                    "502 Service not implemented. See help for commands.");
                   }
              else if (strncmp(cmd, TRACEF_CMD, TRACEF_CMD_LENGTH) == 0)
                   {
                      (void)command(ssl,
                                    "502 Service not implemented. See help for commands.");
                   }
              else if (strncmp(cmd, PROC_CMD, PROC_CMD_LENGTH) == 0)
                   {
                      (void)command(ssl,
                                    "502 Service not implemented. See help for commands.");
                   }
              else if (strncmp(cmd, DISC_CMD, DISC_CMD_LENGTH) == 0)
                   {
                      (void)command(ssl,
                                    "502 Service not implemented. See help for commands.");
                   }
              else if (strncmp(cmd, LDB_CMD, LDB_CMD_LENGTH) == 0)
                   {
                      (void)command(ssl,
                                    "502 Service not implemented. See help for commands.");
                   }
              else if (strncmp(cmd, INFO_CMD, INFO_CMD_LENGTH) == 0)
                   {
                      (void)command(ssl,
                                    "502 Service not implemented. See help for commands.");
                   }
              else if (strncmp(cmd, AFDSTAT_CMD, AFDSTAT_CMD_LENGTH) == 0)
                   {
                      (void)command(ssl,
                                    "502 Service not implemented. See help for commands.");
                   }
                   else
                   {
                      *(cmd + nbytes - 2) = '\0';
                      (void)command(ssl,
                                    "500 \'%s\': command not understood.", cmd);
                   }
      } /* if (nbytes > 0) */
   } /* for (;;) */

   SSL_shutdown(ssl);
   SSL_free(ssl);
   ssl = NULL;
   close_get_display_data();

   exit(SUCCESS);
}


/*++++++++++++++++++++++++ report_shutdown() ++++++++++++++++++++++++++++*/
static void
report_shutdown(void)
{
   if (in_log_child == NO)
   {
      if (cmdssl != NULL)
      {
         if (report_changes == YES)
         {
            show_summary_stat(cmdssl);
            check_changes(cmdssl);
         }
         (void)command(cmdssl, "%s", AFDDS_SHUTDOWN_MESSAGE);
      }
   }

   return;
}
