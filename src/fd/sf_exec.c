/*
 *  sf_exec.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2011 - 2015 Holger Kiehl <Holger.Kiehl@dwd.de>
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
int                        amg_flag = NO,
                           counter_fd = -1,
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
                           *no_of_listed_files,
                           *p_no_of_hosts = NULL,
                           fra_fd = -1,
                           fra_id,
                           fsa_fd = -1,
                           fsa_id,
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
off_t                      *file_size_buffer = NULL;
time_t                     *file_mtime_buffer = NULL;
u_off_t                    prev_file_size_done = 0;
char                       *p_work_dir = NULL,
                           tr_hostname[MAX_HOSTNAME_LENGTH + 1],
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
   int              cb2_ret;
#endif
   int              exec_done,
                    exit_status = TRANSFER_SUCCESS,
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
   time_t           connected,
                    last_update_time,
                    now,
                    *p_file_mtime_buffer;
   char             command_str[MAX_PATH_LENGTH + MAX_RECIPIENT_LENGTH],
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
                    tmp_option[MAX_PATH_LENGTH + MAX_RECIPIENT_LENGTH];
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

   if ((signal(SIGINT, sig_kill) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to set signal handlers : %s", strerror(errno));
      exit(INCORRECT);
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
      if ((db.special_flag & EXEC_ONCE_ONLY) == 0)
      {
         char *p_end;

         /* Prepare insert list. */
         p_end = p_command;
         k = 0;
         ii = 0;
         while ((*p_end != '\0') && (ii < MAX_EXEC_FILE_SUBSTITUTION))
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

      p_file_name_buffer = file_name_buffer;
      p_file_size_buffer = file_size_buffer;
      p_file_mtime_buffer = file_mtime_buffer;
      last_update_time = time(NULL);
      local_file_size = 0;
      exec_done = NO;
      for (files_send = 0; files_send < files_to_send; files_send++)
      {
         (void)strcpy(p_source_file, p_file_name_buffer);

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
               (void)snprintf(command_str, MAX_PATH_LENGTH + MAX_RECIPIENT_LENGTH,
                              "cd %s && %s", file_path, p_command);
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
                                      job_str,
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
                     exit(EXEC_ERROR);
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
            length_start = snprintf(command_str, MAX_PATH_LENGTH + MAX_RECIPIENT_LENGTH,
                                    "cd %s && %s", file_path, p_command);
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
                                     MAX_PATH_LENGTH + MAX_RECIPIENT_LENGTH - (length_start + length),
                                     "\"%s\"%s", p_file_name_buffer,
                                     insert_list[k - 1] + 2);
               }
               else
               {
                  length += snprintf(command_str + length_start + length,
                                     MAX_PATH_LENGTH + MAX_RECIPIENT_LENGTH - (length_start + length),
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
                                   job_str,
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
                  exit(EXEC_ERROR);
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
            trans_exec(file_path, source_file, p_file_name_buffer);
         }
#endif

#ifdef _OUTPUT_LOG
         if (db.output_log == YES)
         {
            if (ol_fd == -2)
            {
# ifdef WITHOUT_FIFO_RW_SUPPORT
               output_log_fd(&ol_fd, &ol_readfd);
# else
               output_log_fd(&ol_fd);
# endif
            }
            if ((ol_fd > -1) && (ol_data == NULL))
            {
               output_log_ptrs(&ol_retries, &ol_job_number, &ol_data,
                               &ol_file_name, &ol_file_name_length,
                               &ol_archive_name_length, &ol_file_size,
                               &ol_unl, &ol_size, &ol_transfer_time,
                               &ol_output_type, db.host_alias, 0, EXEC);
            }
         }
#endif

         /* Now archive file if necessary. */
         if ((db.archive_time > 0) &&
             (p_db->archive_dir[0] != FAILED_TO_CREATE_ARCHIVE_DIR))
         {
            /*
             * By telling the function archive_file() that this
             * is the first time to archive a file for this job
             * (in struct p_db) it does not always have to check
             * whether the directory has been created or not. And
             * we ensure that we do not create duplicate names
             * when adding db.archive_time to msg_name.
             */
            if (archive_file(file_path, p_file_name_buffer, p_db) < 0)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Failed to archive file `%s'", file_name);

               /*
                * NOTE: We _MUST_ delete the file we just send,
                *       else the file directory will run full!
                */
               if (unlink(source_file) == -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Could not unlink() local file `%s' after copying it successfully : %s",
                             source_file, strerror(errno));
               }

#ifdef _OUTPUT_LOG
               if (db.output_log == YES)
               {
                  (void)memcpy(ol_file_name, db.p_unique_name, db.unl);
                  if (db.trans_rename_rule[0] != '\0')
                  {
                     *ol_file_name_length = (unsigned short)snprintf(ol_file_name + db.unl,
                                                                     MAX_FILENAME_LENGTH,
                                                                     "%s%c/%s",
                                                                     p_file_name_buffer,
                                                                     SEPARATOR_CHAR,
                                                                     p_file_name_buffer) +
                                                                     db.unl;
                  }
                  else
                  {
                     (void)strcpy(ol_file_name + db.unl, p_file_name_buffer);
                     *ol_file_name_length = (unsigned short)strlen(ol_file_name);
                     ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
                     ol_file_name[*ol_file_name_length + 1] = '\0';
                     (*ol_file_name_length)++;
                  }
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

#ifdef _OUTPUT_LOG
               if (db.output_log == YES)
               {
                  (void)memcpy(ol_file_name, db.p_unique_name, db.unl);
                  if (db.trans_rename_rule[0] != '\0')
                  {
                     *ol_file_name_length = (unsigned short)snprintf(ol_file_name + db.unl,
                                                                     MAX_FILENAME_LENGTH,
                                                                     "%s%c/%s",
                                                                     p_file_name_buffer,
                                                                     SEPARATOR_CHAR,
                                                                     p_file_name_buffer) +
                                                                     db.unl;
                  }
                  else
                  {
                     (void)strcpy(ol_file_name + db.unl, p_file_name_buffer);
                     *ol_file_name_length = (unsigned short)strlen(ol_file_name);
                     ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
                     ol_file_name[*ol_file_name_length + 1] = '\0';
                     (*ol_file_name_length)++;
                  }
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
            if (unlink(source_file) == -1)
            {
#ifdef WITH_UNLINK_DELAY
               if ((errno == EBUSY) && (unlink_loops < 20))
               {
                  (void)my_usleep(100000L);
                  unlink_loops++;
                  goto try_again_unlink;
               }
#endif
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not unlink() local file %s after copying it successfully : %s",
                          source_file, strerror(errno));
            }

#ifdef _OUTPUT_LOG
            if (db.output_log == YES)
            {
               (void)memcpy(ol_file_name, db.p_unique_name, db.unl);
               if (db.trans_rename_rule[0] != '\0')
               {
                  *ol_file_name_length = (unsigned short)snprintf(ol_file_name + db.unl,
                                                                  MAX_FILENAME_LENGTH,
                                                                  "%s%c/%s",
                                                                  p_file_name_buffer,
                                                                  SEPARATOR_CHAR,
                                                                  p_file_name_buffer) +
                                                                  db.unl;
               }
               else
               {
                  (void)strcpy(ol_file_name + db.unl, p_file_name_buffer);
                  *ol_file_name_length = (unsigned short)strlen(ol_file_name);
                  ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
                  ol_file_name[*ol_file_name_length + 1] = '\0';
                  (*ol_file_name_length)++;
               }
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
            if (fsa->error_counter > 0)
            {
               int  fd,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                    readfd,
#endif
                    j;
               char fd_wake_up_fifo[MAX_PATH_LENGTH];

#ifdef LOCK_DEBUG
               lock_region_w(fsa_fd, db.lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
               lock_region_w(fsa_fd, db.lock_offset + LOCK_EC);
#endif
               fsa->error_counter = 0;

               /*
                * Wake up FD!
                */
               (void)snprintf(fd_wake_up_fifo, MAX_PATH_LENGTH, "%s%s%s",
                              p_work_dir, FIFO_DIR, FD_WAKE_UP_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
               if (open_fifo_rw(fd_wake_up_fifo, &readfd, &fd) == -1)
#else
               if ((fd = open(fd_wake_up_fifo, O_RDWR)) == -1)
#endif
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Failed to open() FIFO %s : %s",
                             fd_wake_up_fifo, strerror(errno));
               }
               else
               {
                  if (write(fd, "", 1) != 1)
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "Failed to write() to FIFO %s : %s",
                                fd_wake_up_fifo, strerror(errno));
                  }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                  if (close(readfd) == -1)
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "Failed to close() FIFO %s : %s",
                                fd_wake_up_fifo, strerror(errno));
                  }
#endif
                  if (close(fd) == -1)
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "Failed to close() FIFO %s : %s",
                                fd_wake_up_fifo, strerror(errno));
                  }
               }

               /*
                * Remove the error condition (NOT_WORKING) from all jobs
                * of this host.
                */
               for (j = 0; j < fsa->allowed_transfers; j++)
               {
                  if ((j != db.job_no) &&
                      (fsa->job_status[j].connect_status == NOT_WORKING))
                  {
                     fsa->job_status[j].connect_status = DISCONNECT;
                  }
               }
               fsa->error_history[0] = 0;
               fsa->error_history[1] = 0;
#ifdef LOCK_DEBUG
               unlock_region(fsa_fd, db.lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
               unlock_region(fsa_fd, db.lock_offset + LOCK_EC);
#endif

               /*
                * Since we have successfully transmitted a file, no need to
                * have the queue stopped anymore.
                */
               if (fsa->host_status & AUTO_PAUSE_QUEUE_STAT)
               {
                  char *sign;

#ifdef LOCK_DEBUG
                  lock_region_w(fsa_fd, db.lock_offset + LOCK_HS, __FILE__, __LINE__);
#else
                  lock_region_w(fsa_fd, db.lock_offset + LOCK_HS);
#endif
                  fsa->host_status &= ~AUTO_PAUSE_QUEUE_STAT;
                  if (fsa->host_status & HOST_ERROR_EA_STATIC)
                  {
                     fsa->host_status &= ~EVENT_STATUS_STATIC_FLAGS;
                  }
                  else
                  {
                     fsa->host_status &= ~EVENT_STATUS_FLAGS;
                  }
                  fsa->host_status &= ~PENDING_ERRORS;
#ifdef LOCK_DEBUG
                  unlock_region(fsa_fd, db.lock_offset + LOCK_HS, __FILE__, __LINE__);
#else
                  unlock_region(fsa_fd, db.lock_offset + LOCK_HS);
#endif
                  error_action(fsa->host_alias, "stop", HOST_ERROR_ACTION);
                  event_log(0L, EC_HOST, ET_EXT, EA_ERROR_END, "%s",
                            fsa->host_alias);
                  if ((fsa->host_status & HOST_ERROR_OFFLINE_STATIC) ||
                      (fsa->host_status & HOST_ERROR_OFFLINE) ||
                      (fsa->host_status & HOST_ERROR_OFFLINE_T))
                  {
                     sign = OFFLINE_SIGN;
                  }
                  else
                  {
                     sign = INFO_SIGN;
                  }
                  trans_log(sign, __FILE__, __LINE__, NULL, NULL,
                            "Starting input queue that was stopped by init_afd.");
                  event_log(0L, EC_HOST, ET_AUTO, EA_START_QUEUE, "%s",
                            fsa->host_alias);
               }
            } /* if (fsa->error_counter > 0) */
#ifdef WITH_ERROR_QUEUE
            if (fsa->host_status & ERROR_QUEUE_SET)
            {
               remove_from_error_queue(db.id.job, fsa, db.fsa_pos, fsa_fd);
            }
#endif
            if (fsa->host_status & HOST_ACTION_SUCCESS)
            {
               error_action(fsa->host_alias, "start", HOST_SUCCESS_ACTION);
            }
         }

         p_file_name_buffer += MAX_FILENAME_LENGTH;
         p_file_size_buffer++;
         if (file_mtime_buffer != NULL)
         {
            p_file_mtime_buffer++;
         }
      } /* for (files_send = 0; files_send < files_to_send; files_send++) */

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
         if (unlink(db.lock_file_name) == -1)
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
      if ((fsa->protocol_options & KEEP_CONNECTED_DISCONNECT) &&
          (db.keep_connected > 0) &&
          ((time(NULL) - connected) > db.keep_connected))
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

   if (cb2_ret == NEITHER)
   {
      exit_status = STILL_FILES_TO_SEND;
   }
#endif /* _WITH_BURST_2 */

   exitflag = 0;
   exit(exit_status);
}


/*++++++++++++++++++++++++++++ sf_exec_exit() +++++++++++++++++++++++++++*/
static void
sf_exec_exit(void)
{
   if ((fsa != NULL) && (db.fsa_pos >= 0))
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
   if (fsa->job_status[(int)db.job_no].unique_name[2] == 5)
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
