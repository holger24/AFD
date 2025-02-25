/*
 *  sf_exec.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2011 - 2025 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   sf_exec - executes a command with the data
 **
 ** SYNOPSIS
 **   sf_exec <work dir> <job no.> <FSA id> <FSA pos> <msg name> [options]
 **
 **   options
 **       --version        Version Number
 **       -a <age limit>   The age limit for the files being send.
 **       -A               Disable archiving of files.
 **       -o <retries>     Old/Error message and number of retries.
 **       -r               Resend from archive (job from show_olog).
 **       -t               Temp toggle.
 **
 ** DESCRIPTION
 **   sf_exec is very similar to sf_loc only that it executes
 **   a command with the file(s).
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has
 **   occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.11.2011 H.Kiehl Created
 **   15.09.2014 H.Kiehl Added simulation mode.
 **   14.01.2024 H.Kiehl Add more debug log information.
 **   25.02.2025 H.Kiehl To help the external program, export
 **                      the following environment variables:
 **                         AFD_HC_TIMEOUT
 **                         AFD_HC_BLOCKSIZE
 **                         AFD_CURRENT_HOSTNAME
 **
 */
DESCR__E_M1

#include <stdio.h>                     /* fprintf(), snprintf()          */
#include <string.h>                    /* strcpy(), strcat(), strerror() */
#include <stdlib.h>                    /* getenv(), abort()              */
#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>                     /* utime()                        */
#include <sys/time.h>                  /* struct timeval                 */
#ifdef _OUTPUT_LOG
# include <sys/times.h>                /* times(), struct tms            */
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <signal.h>                    /* signal()                       */
#include <unistd.h>                    /* unlink(), alarm()              */
#include <errno.h>
#include "fddefs.h"
#include "version.h"

/* Global variables. */
int                        counter_fd = -1,
                           *current_no_of_listed_files,
                           event_log_fd = STDERR_FILENO,
                           exitflag = IS_FAULTY_VAR,
                           files_to_delete,
#ifdef HAVE_HW_CRC32
                           have_hw_crc32 = NO,
#endif
#ifdef _MAINTAINER_LOG
                           maintainer_log_fd = STDERR_FILENO,
#endif
                           no_of_dirs = 0,
                           no_of_hosts,    /* This variable is not used */
                                           /* in this module.           */
                           no_of_listed_files,
                           *p_no_of_dirs = NULL,
                           *p_no_of_hosts = NULL,
                           fra_fd = -1,
                           fra_id,
                           fsa_fd = -1,
                           fsa_id,
                           fsa_pos_save = NO,
                           prev_no_of_files_done = 0,
                           rl_fd = -1,
                           simulation_mode = NO,
                           sys_log_fd = STDERR_FILENO,
                           timeout_flag = OFF,
                           transfer_log_fd = STDERR_FILENO,
                           trans_db_log_fd = STDERR_FILENO,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                           trans_db_log_readfd,
                           transfer_log_readfd,
#endif
                           trans_rename_blocked = NO,
                           *unique_counter;
#ifdef _OUTPUT_LOG
int                        ol_fd = -2;
# ifdef WITHOUT_FIFO_RW_SUPPORT
int                        ol_readfd = -2;
# endif
unsigned int               *ol_job_number,
                           *ol_retries;
char                       *ol_data = NULL,
                           *ol_file_name,
                           *ol_output_type;
unsigned short             *ol_archive_name_length,
                           *ol_file_name_length,
                           *ol_unl;
off_t                      *ol_file_size;
size_t                     ol_size,
                           ol_real_size;
clock_t                    *ol_transfer_time;
#endif
#ifdef _WITH_BURST_2
unsigned int               burst_2_counter = 0;
#endif
long                       transfer_timeout; /* Not used [init_sf()]    */
#ifdef HAVE_MMAP
off_t                      fra_size,
                           fsa_size;
#endif
off_t                      *file_size_buffer = NULL,
                           rl_size = 0;
time_t                     *file_mtime_buffer = NULL;
u_off_t                    prev_file_size_done = 0;
char                       *p_work_dir = NULL,
                           tr_hostname[MAX_HOSTNAME_LENGTH + 2],
                           *del_file_name_buffer = NULL,
                           *file_name_buffer = NULL;
struct fileretrieve_status *fra = NULL;
struct filetransfer_status *fsa = NULL;
struct retrieve_list       *rl;
struct job                 db;
struct rule                *rule;
#ifdef _DELETE_LOG
struct delete_log          dl;
#endif
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local global variables. */
static int                 files_send,
                           files_to_send,
                           local_file_counter;
static off_t               local_file_size,
                           *p_file_size_buffer;

/* Local function prototypes. */
static void                sf_exec_exit(void),
                           sig_bus(int),
                           sig_segv(int),
                           sig_kill(int),
                           sig_exit(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
#ifdef _WITH_BURST_2
   int              cb2_ret = NO;
#endif
   int              add_env_var_length,
                    current_toggle,
                    exec_done,
                    exit_status = TRANSFER_SUCCESS,
                    file_path_length,
                    ii,
                    k,
                    last_k,
                    length,
                    length_start,
                    mask_file_name,
#ifdef HAVE_SETPRIORITY
                    sched_priority,
#endif
                    ret;
#ifdef WITH_ARCHIVE_COPY_INFO
   unsigned int     archived_copied = 0;
#endif
   time_t           connected,
#ifdef _WITH_BURST_2
                    diff_time,
#endif
                    last_update_time,
                    now,
                    *p_file_mtime_buffer;
   char             add_env_var[15 + MAX_LONG_LENGTH + 1 + 17 + MAX_INT_LENGTH + 1 + 21 + MAX_REAL_HOSTNAME_LENGTH + 2 + 59 + 2],
                    *command_str,
                    file_name[MAX_FILENAME_LENGTH],
                    file_path[MAX_PATH_LENGTH],
                    *fnp,
                    *insert_list[MAX_EXEC_FILE_SUBSTITUTION],
                    job_str[4],
                    *p_command,
                    *p_source_file,
                    *p_file_name_buffer,
                    *return_str = NULL,
                    source_file[MAX_PATH_LENGTH],
                    tmp_char,
                    tmp_option[1024 + 1];
   clock_t          clktck;
   struct job       *p_db;
#ifdef SA_FULLDUMP
   struct sigaction sact;
#endif
#ifdef WITH_FAST_MOVE
   nlink_t          nlink;
#endif
#ifdef _OUTPUT_LOG
   clock_t          end_time = 0,
                    start_time = 0;
   struct tms       tmsdummy;
#endif

   CHECK_FOR_VERSION(argc, argv);

#ifdef SA_FULLDUMP
   /*
    * When dumping core sure we do a FULL core dump!
    */
   sact.sa_handler = SIG_DFL;
   sact.sa_flags = SA_FULLDUMP;
   sigemptyset(&sact.sa_mask);
   if (sigaction(SIGSEGV, &sact, NULL) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "sigaction() error : %s", strerror(errno));
      exit(INCORRECT);
   }
#endif

   /* Do some cleanups when we exit. */
   if (atexit(sf_exec_exit) != 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not register exit function : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Initialise variables. */
   local_file_counter = 0;
   files_to_send = init_sf(argc, argv, file_path, EXEC_FLAG);
   p_db = &db;
   if ((clktck = sysconf(_SC_CLK_TCK)) <= 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not get clock ticks per second : %s", strerror(errno));
      exit(INCORRECT);
   }

   if ((signal(SIGINT, sig_kill) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_kill) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to set signal handlers : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Now determine the real hostname. */
   if (fsa->real_hostname[1][0] == '\0')
   {
      (void)strcpy(db.hostname, fsa->real_hostname[0]);
      current_toggle = HOST_ONE;
   }
   else
   {
      if (db.toggle_host == YES)
      {
         if (fsa->host_toggle == HOST_ONE)
         {
            (void)strcpy(db.hostname, fsa->real_hostname[HOST_TWO - 1]);
            current_toggle = HOST_TWO;
         }
         else
         {
            (void)strcpy(db.hostname, fsa->real_hostname[HOST_ONE - 1]);
            current_toggle = HOST_ONE;
         }
      }
      else
      {
         current_toggle = (int)fsa->host_toggle;
         (void)strcpy(db.hostname, fsa->real_hostname[(current_toggle - 1)]);
      }
   }

   /* Add additional environment variables. */
   add_env_var_length = snprintf(add_env_var,
                                 15 + MAX_LONG_LENGTH + 1 + 17 + MAX_INT_LENGTH + 1 + 21 + MAX_REAL_HOSTNAME_LENGTH + 2 + 59 + 2,
                                 "AFD_HC_TIMEOUT=%ld;AFD_HC_BLOCKSIZE=%d;AFD_CURRENT_HOSTNAME=%s;export AFD_HC_TIMEOUT AFD_HC_BLOCKSIZE AFD_CURRENT_HOSTNAME",
                                 transfer_timeout, fsa->block_size,
                                 db.hostname);

   /* Allocate buffer for command string. */
   file_path_length = (int)strlen(file_path);
   if ((command_str = malloc(add_env_var_length + 3 + file_path_length + 4 + 1024 + 2)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      exit(ALLOC_ERROR);
   }

   /* Inform FSA that we have are ready to copy the files. */
   if (gsf_check_fsa(p_db) != NEITHER)
   {
      fsa->job_status[(int)db.job_no].connect_status = EXEC_ACTIVE;
      fsa->job_status[(int)db.job_no].no_of_files = files_to_send;
   }

   /* Init job_str for exec_cmd(). */
   job_str[0] = '[';
   job_str[1] = db.job_no + '0';
   job_str[2] = ']';
   job_str[3] = '\0';

   connected = time(NULL);

#ifdef _WITH_BURST_2
   do
   {
      if (burst_2_counter > 0)
      {
         if (fsa->debug > NORMAL_MODE)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL, "Bursting.");
         }
      }
#endif /* _WITH_BURST_2 */
      /* Prepare pointers and directory name. */
      (void)strcpy(source_file, file_path);
      p_source_file = source_file + strlen(source_file);
      *p_source_file++ = '/';

#ifdef HAVE_SETPRIORITY
      if (db.exec_base_priority != NO_PRIORITY)
      {
         sched_priority = db.exec_base_priority;
         if (db.add_afd_priority == YES)
         {
            sched_priority += (int)(fsa->job_status[(int)db.job_no].unique_name[MAX_MSG_NAME_LENGTH - 1]);
            if (sched_priority > db.min_sched_priority)
            {
               sched_priority = db.min_sched_priority;
            }
            else if (sched_priority < db.max_sched_priority)
                 {
                    sched_priority = db.max_sched_priority;
                 }
         }
         if ((sched_priority == db.current_priority) ||
             ((db.current_priority > sched_priority) && (geteuid() != 0)))
         {
            sched_priority = NO_PRIORITY;
         }
      }
      else
      {
         sched_priority = NO_PRIORITY;
      }
#endif

      p_command = db.exec_cmd;
      while ((*p_command == ' ') || (*p_command == '\t'))
      {
         p_command++;
      }
      ii = 0;
      if ((db.special_flag & EXEC_ONCE_ONLY) == 0)
      {
         char *p_end;

         /* Prepare insert list. */
         p_end = p_command;
         k = 0;
         while ((*p_end != '\0') && (ii < MAX_EXEC_FILE_SUBSTITUTION) &&
                (k < 1024))
         {
            if ((*p_end == '%') && (*(p_end + 1) == 's'))
            {
               tmp_option[k] = *p_end;
               tmp_option[k + 1] = *(p_end + 1);
               insert_list[ii] = &tmp_option[k];
               ii++;
               p_end += 2;
               k += 2;
            }
            else
            {
               tmp_option[k] = *p_end;
               p_end++; k++;
            }
         }
         if (ii >= MAX_EXEC_FILE_SUBSTITUTION)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                      "To many %%s in pexec option. Can oly handle %d.",
                      MAX_EXEC_FILE_SUBSTITUTION);
         }
         tmp_option[k] = '\0';
         p_command = tmp_option;
         last_k = k;
      }
      else
      {
         last_k = 0; /* Silence compiler. */
      }

      p_file_name_buffer = file_name_buffer;
      p_file_size_buffer = file_size_buffer;
      p_file_mtime_buffer = file_mtime_buffer;
      last_update_time = time(NULL);
      local_file_size = 0;
      exec_done = NO;
      for (files_send = 0; files_send < files_to_send; files_send++)
      {
         (void)strcpy(p_source_file, p_file_name_buffer);

#ifdef WITH_DUP_CHECK
# ifndef FAST_SF_DUPCHECK
         if ((db.dup_check_timeout > 0) &&
             (isdup(source_file, p_file_name_buffer, *p_file_size_buffer,
                    db.crc_id, db.dup_check_timeout, db.dup_check_flag, NO,
#  ifdef HAVE_HW_CRC32
                    have_hw_crc32,
#  endif
                    YES, YES) == YES))
         {
            time_t       file_mtime;
#  ifdef HAVE_STATX
            struct statx stat_buf;
#  else
            struct stat  stat_buf;
#  endif

            now = time(NULL);
            if (file_mtime_buffer == NULL)
            {
#  ifdef HAVE_STATX
               if (statx(0, source_file, AT_STATX_SYNC_AS_STAT,
                         STATX_MTIME, &stat_buf) == -1)
#  else
               if (stat(source_file, &stat_buf) == -1)
#  endif
               {
                  file_mtime = now;
               }
               else
               {
#  ifdef HAVE_STATX
                  file_mtime = stat_buf.stx_mtime.tv_sec;
#  else
                  file_mtime = stat_buf.st_mtime;
#  endif
               }
            }
            else
            {
               file_mtime = *p_file_mtime_buffer;
            }
            handle_dupcheck_delete(SEND_FILE_EXEC, fsa->host_alias, source_file,
                                   p_file_name_buffer, *p_file_size_buffer,
                                   file_mtime, now);
            if (db.dup_check_flag & DC_DELETE)
            {
               local_file_size += *p_file_size_buffer;
               local_file_counter += 1;
               if (now >= (last_update_time + LOCK_INTERVAL_TIME))
               {
                  last_update_time = now;
                  update_tfc(local_file_counter, local_file_size,
                             p_file_size_buffer, files_to_send,
                             files_send, now);
                  local_file_size = 0;
                  local_file_counter = 0;
               }
            }
         }
         else
         {
# endif
#endif

         if (db.special_flag & EXEC_ONCE_ONLY)
         {
            if (exec_done == NO)
            {
#ifdef _OUTPUT_LOG
               if (db.output_log == YES)
               {
                  start_time = times(&tmsdummy);
               }
#endif
               (void)snprintf(command_str,
                              add_env_var_length + 1 + 3 + file_path_length + 4 + 1024 + 1,
                              "%s;cd %s && %s",
                              add_env_var, file_path, p_command);
               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                               "Executing command `%s' to sent file `%s'",
                               command_str, p_file_name_buffer);
               }
               if (simulation_mode != YES)
               {
                  if ((ret = exec_cmd(command_str, &return_str, transfer_log_fd,
                                      fsa->host_dsp_name, MAX_HOSTNAME_LENGTH,
#ifdef HAVE_SETPRIORITY
                                      sched_priority,
#endif
                                      job_str, NULL, NULL, clktck,
                                      (fsa->protocol_options & TIMEOUT_TRANSFER) ? (time_t)transfer_timeout : 0L,
                                      YES, YES)) != 0)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Failed to execute command %s [Return code = %d]",
                               command_str, ret);
                     if ((return_str != NULL) && (return_str[0] != '\0'))
                     {
                        char *end_ptr = return_str,
                             *start_ptr;

                        do
                        {
                           start_ptr = end_ptr;
                           while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                           {
                              end_ptr++;
                           }
                           if (*end_ptr == '\n')
                           {
                              *end_ptr = '\0';
                              end_ptr++;
                           }
                           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                     "%s", start_ptr);
                        } while (*end_ptr != '\0');
                     }
                     rm_dupcheck_crc(source_file, p_file_name_buffer,
                                     *p_file_size_buffer);
                     exit(EXEC_ERROR);
                  }
                  else
                  {
                     if (fsa->debug > NORMAL_MODE)
                     {
                        trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                                     "Executed command `%s' [Return code = %d]",
                                     command_str, ret);
                        if ((return_str != NULL) && (return_str[0] != '\0'))
                        {
                           char *end_ptr = return_str,
                                *start_ptr;

                           do
                           {
                              start_ptr = end_ptr;
                              while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                              {
                                 end_ptr++;
                              }
                              if (*end_ptr == '\n')
                              {
                                 *end_ptr = '\0';
                                 end_ptr++;
                              }
                              trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                                           "%s", start_ptr);
                           } while (*end_ptr != '\0');
                        }
                     }
                  }
               }
#ifdef _OUTPUT_LOG
               if (db.output_log == YES)
               {
                  end_time = times(&tmsdummy);
               }
#endif
               free(return_str);
               return_str = NULL;
               exec_done = YES;
            }
         }
         else
         {
            /* Write status to FSA? */
            if (gsf_check_fsa(p_db) != NEITHER)
            {
               fsa->job_status[(int)db.job_no].file_size_in_use = *p_file_size_buffer;
               (void)strcpy(fsa->job_status[(int)db.job_no].file_name_in_use,
                            p_file_name_buffer);
            }

#ifdef _OUTPUT_LOG
            if (db.output_log == YES)
            {
               start_time = times(&tmsdummy);
            }
#endif

            insert_list[ii] = &tmp_option[last_k];
            tmp_char = *insert_list[0];
            *insert_list[0] = '\0';
            length_start = snprintf(command_str,
                                    add_env_var_length + 1 + 3 + file_path_length + 4 + 1024 + 1,
                                    "%s;cd %s && %s",
                                    add_env_var, file_path, p_command);
            *insert_list[0] = tmp_char;

            fnp = p_file_name_buffer;
            mask_file_name = NO;
            do
            {
               if ((*fnp == ';') || (*fnp == ' '))
               {
                  mask_file_name = YES;
                  break;
               }
               fnp++;
            } while (*fnp != '\0');

            /* Generate command string with file name(s). */
            length = 0;
            for (k = 1; k < (ii + 1); k++)
            {
               tmp_char = *insert_list[k];
               *insert_list[k] = '\0';
               if (mask_file_name == YES)
               {
                  length += snprintf(command_str + length_start + length,
                                     3 + MAX_PATH_LENGTH + 4 + 1024 + 1 - (length_start + length),
                                     "\"%s\"%s", p_file_name_buffer,
                                     insert_list[k - 1] + 2);
               }
               else
               {
                  length += snprintf(command_str + length_start + length,
                                     3 + MAX_PATH_LENGTH + 4 + 1024 + 1 - (length_start + length),
                                     "%s%s", p_file_name_buffer,
                                     insert_list[k - 1] + 2);
               }
               *insert_list[k] = tmp_char;
            }

            if (simulation_mode != YES)
            {
               if ((ret = exec_cmd(command_str, &return_str, transfer_log_fd,
                                   fsa->host_dsp_name, MAX_HOSTNAME_LENGTH,
#ifdef HAVE_SETPRIORITY
                                   sched_priority,
#endif
                                   job_str, NULL, NULL, clktck,
                                   (fsa->protocol_options & TIMEOUT_TRANSFER) ? (time_t)transfer_timeout : 0L,
                                   YES, YES)) != 0) /* ie != SUCCESS */
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Failed to execute command %s [Return code = %d]",
                            command_str, ret);
                  if ((return_str != NULL) && (return_str[0] != '\0'))
                  {
                     char *end_ptr = return_str,
                          *start_ptr;

                     do
                     {
                        start_ptr = end_ptr;
                        while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                        {
                           end_ptr++;
                        }
                        if (*end_ptr == '\n')
                        {
                           *end_ptr = '\0';
                              end_ptr++;
                        }
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  "%s", start_ptr);
                     } while (*end_ptr != '\0');
                  }
                  rm_dupcheck_crc(source_file, p_file_name_buffer,
                                  *p_file_size_buffer);
                  exit(EXEC_ERROR);
               }
               else
               {
                  if (fsa->debug > NORMAL_MODE)
                  {
                     trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                                  "Executed command `%s' [Return code = %d]",
                                  command_str, ret);
                     if ((return_str != NULL) && (return_str[0] != '\0'))
                     {
                        char *end_ptr = return_str,
                             *start_ptr;

                        do
                        {
                           start_ptr = end_ptr;
                           while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                           {
                              end_ptr++;
                           }
                           if (*end_ptr == '\n')
                           {
                              *end_ptr = '\0';
                              end_ptr++;
                           }
                           trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                                        "%s", start_ptr);
                        } while (*end_ptr != '\0');
                     }
                  }
               }
               free(return_str);
               return_str = NULL;
            }

#ifdef _OUTPUT_LOG
            if (db.output_log == YES)
            {
               end_time = times(&tmsdummy);
            }
#endif
         }

         /* Tell FSA we have done a file. */
         if (gsf_check_fsa(p_db) != NEITHER)
         {
            fsa->job_status[(int)db.job_no].file_name_in_use[0] = '\0';
            fsa->job_status[(int)db.job_no].no_of_files_done++;
            fsa->job_status[(int)db.job_no].file_size_in_use = 0;
            fsa->job_status[(int)db.job_no].file_size_in_use_done = 0;
            fsa->job_status[(int)db.job_no].file_size_done += *p_file_size_buffer;
            fsa->job_status[(int)db.job_no].bytes_send += *p_file_size_buffer;
            local_file_size += *p_file_size_buffer;
            local_file_counter += 1;

            now = time(NULL);
            if (now >= (last_update_time + LOCK_INTERVAL_TIME))
            {
               last_update_time = now;
               update_tfc(local_file_counter, local_file_size,
                          p_file_size_buffer, files_to_send,
                          files_send, now);
               local_file_size = 0;
               local_file_counter = 0;
            }
         }

#ifdef _WITH_TRANS_EXEC
         if (db.special_flag & TRANS_EXEC)
         {
            trans_exec(file_path, source_file, p_file_name_buffer, clktck);
         }
#endif

#ifdef _OUTPUT_LOG
         if (db.output_log == YES)
         {
            if (ol_fd == -2)
            {
# ifdef WITHOUT_FIFO_RW_SUPPORT
               output_log_fd(&ol_fd, &ol_readfd, &db.output_log);
# else
               output_log_fd(&ol_fd, &db.output_log);
# endif
            }
            if ((ol_fd > -1) && (ol_data == NULL))
            {
               output_log_ptrs(&ol_retries, &ol_job_number, &ol_data,
                               &ol_file_name, &ol_file_name_length,
                               &ol_archive_name_length, &ol_file_size,
                               &ol_unl, &ol_size, &ol_transfer_time,
                               &ol_output_type, db.host_alias,
                               (current_toggle - 1), EXEC, &db.output_log);
            }
         }
#endif

         /* Now archive file if necessary. */
         if ((db.archive_time > 0) &&
             (p_db->archive_dir[0] != FAILED_TO_CREATE_ARCHIVE_DIR))
         {
#ifdef WITH_ARCHIVE_COPY_INFO
            int ret;
#endif

            /*
             * By telling the function archive_file() that this
             * is the first time to archive a file for this job
             * (in struct p_db) it does not always have to check
             * whether the directory has been created or not. And
             * we ensure that we do not create duplicate names
             * when adding db.archive_time to msg_name.
             */
#ifdef WITH_ARCHIVE_COPY_INFO
            if ((ret = archive_file(file_path, p_file_name_buffer, p_db)) < 0)
#else
            if (archive_file(file_path, p_file_name_buffer, p_db) < 0)
#endif
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Failed to archive file `%s'", file_name);

               /*
                * NOTE: We _MUST_ delete the file we just send,
                *       else the file directory will run full!
                */
               if ((unlink(source_file) == -1) && (errno != ENOENT))
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Failed to unlink() local file `%s' : %s",
                            source_file, strerror(errno));
               }

#ifdef _OUTPUT_LOG
               if (db.output_log == YES)
               {
                  (void)memcpy(ol_file_name, db.p_unique_name, db.unl);
                  (void)strcpy(ol_file_name + db.unl, p_file_name_buffer);
                  *ol_file_name_length = (unsigned short)strlen(ol_file_name);
                  ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
                  ol_file_name[*ol_file_name_length + 1] = '\0';
                  (*ol_file_name_length)++;
                  *ol_file_size = *p_file_size_buffer;
                  *ol_job_number = fsa->job_status[(int)db.job_no].job_id;
                  *ol_retries = db.retries;
                  *ol_unl = db.unl;
                  *ol_transfer_time = end_time - start_time;
                  *ol_archive_name_length = 0;
                  *ol_output_type = OT_NORMAL_DELIVERED + '0';
                  ol_real_size = *ol_file_name_length + ol_size;
                  if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "write() error : %s", strerror(errno));
                  }
               }
#endif
            }
            else
            {
               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                               "Archived file `%s'.", file_name);
               }
#ifdef WITH_ARCHIVE_COPY_INFO
               if (ret == DATA_COPIED)
               {
                  archived_copied++;
               }
#endif

#ifdef _OUTPUT_LOG
               if (db.output_log == YES)
               {
                  (void)memcpy(ol_file_name, db.p_unique_name, db.unl);
                  (void)strcpy(ol_file_name + db.unl, p_file_name_buffer);
                  *ol_file_name_length = (unsigned short)strlen(ol_file_name);
                  ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
                  ol_file_name[*ol_file_name_length + 1] = '\0';
                  (*ol_file_name_length)++;
                  (void)strcpy(&ol_file_name[*ol_file_name_length + 1], &db.archive_dir[db.archive_offset]);
                  *ol_file_size = *p_file_size_buffer;
                  *ol_job_number = fsa->job_status[(int)db.job_no].job_id;
                  *ol_retries = db.retries;
                  *ol_unl = db.unl;
                  *ol_transfer_time = end_time - start_time;
                  *ol_archive_name_length = (unsigned short)strlen(&ol_file_name[*ol_file_name_length + 1]);
                  *ol_output_type = OT_NORMAL_DELIVERED + '0';
                  ol_real_size = *ol_file_name_length +
                                 *ol_archive_name_length + 1 + ol_size;
                  if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "write() error : %s", strerror(errno));
                  }
               }
#endif
            }
         }
         else
         {
#ifdef WITH_UNLINK_DELAY
            int unlink_loops = 0;

try_again_unlink:
#endif
            /* Delete the file we just have executed. */
            if ((unlink(source_file) == -1) && (errno != ENOENT))
            {
#ifdef WITH_UNLINK_DELAY
               if ((errno == EBUSY) && (unlink_loops < 20))
               {
                  (void)my_usleep(100000L);
                  unlink_loops++;
                  goto try_again_unlink;
               }
#endif
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Could not unlink() local file %s after transmitting it successfully : %s",
                         source_file, strerror(errno));
            }

#ifdef _OUTPUT_LOG
            if (db.output_log == YES)
            {
               (void)memcpy(ol_file_name, db.p_unique_name, db.unl);
               (void)strcpy(ol_file_name + db.unl, p_file_name_buffer);
               *ol_file_name_length = (unsigned short)strlen(ol_file_name);
               ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
               ol_file_name[*ol_file_name_length + 1] = '\0';
               (*ol_file_name_length)++;
               *ol_file_size = *p_file_size_buffer;
               *ol_job_number = fsa->job_status[(int)db.job_no].job_id;
               *ol_retries = db.retries;
               *ol_unl = db.unl;
               *ol_transfer_time = end_time - start_time;
               *ol_archive_name_length = 0;
               *ol_output_type = OT_NORMAL_DELIVERED + '0';
               ol_real_size = *ol_file_name_length + ol_size;
               if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "write() error : %s", strerror(errno));
               }
            }
#endif
         }

         /*
          * After each successful transfer set error counter to zero,
          * so that other jobs can be started.
          */
         if (gsf_check_fsa(p_db) != NEITHER)
         {
            unset_error_counter_fsa(fsa_fd, transfer_log_fd, p_work_dir,
                                    fsa, (struct job *)&db);
#ifdef WITH_ERROR_QUEUE
            if (fsa->host_status & ERROR_QUEUE_SET)
            {
               remove_from_error_queue(db.id.job, fsa, db.fsa_pos, fsa_fd);
            }
#endif
            if (fsa->host_status & HOST_ACTION_SUCCESS)
            {
               error_action(fsa->host_alias, "start",
                            HOST_SUCCESS_ACTION, transfer_log_fd);
            }
         }
#ifdef WITH_DUP_CHECK
# ifndef FAST_SF_DUPCHECK
         }
# endif
#endif
         p_file_name_buffer += MAX_FILENAME_LENGTH;
         p_file_size_buffer++;
         if (file_mtime_buffer != NULL)
         {
            p_file_mtime_buffer++;
         }
      } /* for (files_send = 0; files_send < files_to_send; files_send++) */

#ifdef WITH_ARCHIVE_COPY_INFO
      if (archived_copied > 0)
      {
         trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
                   "Copied %u files to archive.", archived_copied);
         archived_copied = 0;
      }
#endif

      if (local_file_counter)
      {
         if (gsf_check_fsa(p_db) != NEITHER)
         {
            update_tfc(local_file_counter, local_file_size,
                       p_file_size_buffer, files_to_send, files_send,
                       time(NULL));
            local_file_size = 0;
            local_file_counter = 0;
         }
      }

      /* Do not forget to remove lock file if we have created one. */
      if ((db.lock == LOCKFILE) && (fsa->active_transfers == 1))
      {
         if ((unlink(db.lock_file_name) == -1) && (errno != ENOENT))
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                      "Failed to unlink() lock file `%s' : %s",
                      db.lock_file_name, strerror(errno));
            exit(REMOVE_LOCKFILE_ERROR);
         }
         else
         {
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                            "Removed lock file `%s'.", db.lock_file_name);
            }
         }
      }

      /*
       * Remove file directory with everything in it.
       */
      if (rec_rmdir(file_path) == INCORRECT)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to rec_rmdir() `%s' : %s",
                    file_path, strerror(errno));
         exit_status = STILL_FILES_TO_SEND;
      }

#ifdef _WITH_BURST_2
      burst_2_counter++;
      diff_time = time(NULL) - connected;
      if (((fsa->protocol_options & KEEP_CONNECTED_DISCONNECT) &&
           (db.keep_connected > 0) && (diff_time > db.keep_connected)) ||
          ((db.disconnect > 0) && (diff_time > db.disconnect)))
      {
         cb2_ret = NO;
         break;
      }
   } while ((cb2_ret = check_burst_sf(file_path, &files_to_send, 0,
# ifdef _WITH_INTERRUPT_JOB
                                      0,
# endif
# ifdef _OUTPUT_LOG
                                      &ol_fd,
# endif
# ifndef AFDBENCH_CONFIG
                                      NULL,
# endif
                                      NULL)) == YES);
   burst_2_counter--;
   free(command_str);

   if (cb2_ret == NEITHER)
   {
      exit_status = STILL_FILES_TO_SEND;
   }
#endif /* _WITH_BURST_2 */

   if ((exit_status != STILL_FILES_TO_SEND) &&
       (fsa->job_status[(int)db.job_no].unique_name[1] != '\0') &&
       (fsa->job_status[(int)db.job_no].unique_name[0] != '\0') &&
       (fsa->job_status[(int)db.job_no].unique_name[2] > 7) &&
       (strncmp(fsa->job_status[(int)db.job_no].unique_name,
                db.msg_name, MAX_MSG_NAME_LENGTH) != 0))
   {
      /* Check for a burst miss. */
      if (check_job_dir_empty(fsa->job_status[(int)db.job_no].unique_name,
                              file_path) == NO)
      {
         exit_status = STILL_FILES_TO_SEND;
      }
   }

   exitflag = 0;
   exit(exit_status);
}


/*++++++++++++++++++++++++++++ sf_exec_exit() +++++++++++++++++++++++++++*/
static void
sf_exec_exit(void)
{
   if ((fsa != NULL) && (db.fsa_pos >= 0) && (fsa_pos_save == YES))
   {
      int     diff_no_of_files_done;
      u_off_t diff_file_size_done;

      if (local_file_counter)
      {
         if (gsf_check_fsa((struct job *)&db) != NEITHER)
         {
            update_tfc(local_file_counter, local_file_size,
                       p_file_size_buffer, files_to_send, files_send,
                       time(NULL));
         }
      }

      diff_no_of_files_done = fsa->job_status[(int)db.job_no].no_of_files_done -
                              prev_no_of_files_done;
      diff_file_size_done = fsa->job_status[(int)db.job_no].file_size_done -
                            prev_file_size_done;
      if ((diff_file_size_done > 0) || (diff_no_of_files_done > 0))
      {
         int  length;
#ifdef _WITH_BURST_2
         char buffer[MAX_INT_LENGTH + 10 + MAX_OFF_T_LENGTH + 24 + MAX_INT_LENGTH + 11 + MAX_INT_LENGTH + 1];

         length = MAX_INT_LENGTH + 10 + MAX_OFF_T_LENGTH + 24 + MAX_INT_LENGTH + 11 + MAX_INT_LENGTH + 1;
#else
         char buffer[MAX_INT_LENGTH + 10 + MAX_OFF_T_LENGTH + 24 + MAX_INT_LENGTH + 1];

         length = MAX_INT_LENGTH + 10 + MAX_OFF_T_LENGTH + 24 + MAX_INT_LENGTH + 1;
#endif

         WHAT_DONE_BUFFER(length, buffer, "exec sent",
                           diff_file_size_done, diff_no_of_files_done);
#ifdef _WITH_BURST_2
         if (burst_2_counter == 1)
         {
#ifdef _WITH_BURST_2
            if ((length + 9) < (MAX_INT_LENGTH + 5 + MAX_OFF_T_LENGTH + 24 + MAX_INT_LENGTH + 11 + MAX_INT_LENGTH + 1))
#else
            if ((length + 9) < (MAX_INT_LENGTH + 5 + MAX_OFF_T_LENGTH + 24 + MAX_INT_LENGTH + 1))
#endif
            {
               /* [BURST] */
               buffer[length] = ' ';
               buffer[length + 1] = '[';
               buffer[length + 2] = 'B';
               buffer[length + 3] = 'U';
               buffer[length + 4] = 'R';
               buffer[length + 5] = 'S';
               buffer[length + 6] = 'T';
               buffer[length + 7] = ']';
               buffer[length + 8] = '\0';
            }
         }
         else if (burst_2_counter > 1)
              {
                 (void)snprintf(buffer + length,
#ifdef _WITH_BURST_2
                                MAX_INT_LENGTH + 5 + MAX_OFF_T_LENGTH + 24 + MAX_INT_LENGTH + 11 + MAX_INT_LENGTH + 1 - length,
#else
                                MAX_INT_LENGTH + 5 + MAX_OFF_T_LENGTH + 24 + MAX_INT_LENGTH + 1 - length,
#endif
                                " [BURST * %u]", burst_2_counter);
              }
#endif /* _WITH_BURST_2 */
         trans_log(INFO_SIGN, NULL, 0, NULL, NULL, "%s #%x", buffer, db.id.job);
      }
      reset_fsa((struct job *)&db, exitflag, 0, 0);
      fsa_detach_pos(db.fsa_pos);
   }

   free(file_name_buffer);
   free(file_size_buffer);

   send_proc_fin(NO);
   if (sys_log_fd != STDERR_FILENO)
   {
      (void)close(sys_log_fd);
   }

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   reset_fsa((struct job *)&db, IS_FAULTY_VAR, 0, 0);
   system_log(DEBUG_SIGN, __FILE__, __LINE__,
              "Aaarrrggh! Received SIGSEGV. Remove the programmer who wrote this!");
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   reset_fsa((struct job *)&db, IS_FAULTY_VAR, 0, 0);
   system_log(DEBUG_SIGN, __FILE__, __LINE__, "Uuurrrggh! Received SIGBUS.");
   abort();
}


/*++++++++++++++++++++++++++++++ sig_kill() +++++++++++++++++++++++++++++*/
static void
sig_kill(int signo)
{
   exitflag = 0;
   if ((fsa != NULL) && (fsa_pos_save == YES) &&
       (fsa->job_status[(int)db.job_no].unique_name[2] == 5))
   {
      exit(SUCCESS);
   }
   else
   {
      exit(GOT_KILLED);
   }
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   exit(INCORRECT);
}
