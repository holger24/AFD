/*
 *  sf_map.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2023 Deutscher Wetterdienst (DWD),
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
 **   sf_map - sends files to MAP
 **
 ** SYNOPSIS
 **   sf_map [--version] [-w <directory>] -m <message-file>
 **
 ** DESCRIPTION
 **   sf_map is very similar to sf_ftp only that it sends files
 **   to the MAP system with special functions from that system.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has
 **   occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.05.1997 H.Kiehl Created
 **   08.05.1997 H.Kiehl Logging archive directory.
 **   25.08.1997 H.Kiehl Integrated map function store_blob().
 **   29.08.1997 H.Kiehl Support for 'real' host names.
 **   10.09.1997 H.Kiehl Add FAX_E print service.
 **
 */
DESCR__E_M1

#include <stdio.h>                     /* fprintf(), snprintf()          */
#include <string.h>                    /* strcpy(), strcat(), strcmp(),  */
                                       /* strerror()                     */
#include <stdlib.h>                    /* getenv(), strtoul(), abort()   */
#include <ctype.h>                     /* isdigit()                      */
#include <setjmp.h>                    /* setjmp(), longjmp()            */
#include <signal.h>                    /* signal()                       */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>                  /* struct timeval                 */
#ifdef _OUTPUT_LOG
# include <sys/times.h>                /* times(), struct tms            */
#endif
#include <fcntl.h>
#include <unistd.h>                    /* unlink(), alarm()              */
#include <errno.h>
#include "fddefs.h"
#include "version.h"

/* Global variables. */
int                        counter_fd = -1,    /* NOT USED */
                           event_log_fd = STDERR_FILENO,
                           exitflag = IS_FAULTY_VAR,
                           files_to_delete,    /* NOT USED */
#ifdef HAVE_HW_CRC32
                           have_hw_crc32 = NO,
#endif
#ifdef _MAINTAINER_LOG
                           maintainer_log_fd = STDERR_FILENO,
#endif
                           no_of_dirs,
                           no_of_hosts,   /* This variable is not used   */
                                          /* in this module.             */
                           *p_no_of_hosts,
                           timeout_flag = OFF,
                           fra_fd = -1,
                           fra_id,
                           fsa_fd = -1,
                           fsa_id,
                           fsa_pos_save = NO,
                           simulation_mode = NO,
                           sys_log_fd = STDERR_FILENO,
                           transfer_log_fd = STDERR_FILENO,
                           trans_db_log_fd = STDERR_FILENO,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                           trans_db_log_readfd,
                           transfer_log_readfd,
#endif
                           trans_rename_blocked = NO,
                           *unique_counter;
long                       transfer_timeout; /* Not used [init_sf()]    */
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
#ifdef HAVE_MMAP
off_t                      fra_size,
                           fsa_size;
#endif
off_t                      *file_size_buffer = NULL;
time_t                     *file_mtime_buffer = NULL;
char                       msg_str[MAX_RET_MSG_LENGTH],
                           *p_work_dir = NULL,
                           tr_hostname[MAX_HOSTNAME_LENGTH + 2],
                           *del_file_name_buffer = NULL, /* NOT USED */
                           *file_name_buffer = NULL;
struct fileretrieve_status *fra = NULL;
struct filetransfer_status *fsa = NULL;
struct job                 db;
struct rule                *rule;      /* NOT USED */
#ifdef _DELETE_LOG
struct delete_log          dl;
#endif
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

#ifdef _WITH_MAP_SUPPORT
/* Local global variables. */
static int                 files_send,
                           files_to_send,
                           local_file_counter;
static off_t               local_file_size,
                           *p_file_size_buffer;
static jmp_buf             env_alrm;

/* Local function prototypes. */
static void                sf_map_exit(void),
                           sig_bus(int),
                           sig_segv(int),
                           sig_kill(int),
                           sig_exit(int),
                           sig_handler(int);

/* Remote function prototypes. */
extern int                 store_blob(unsigned long, char *, char *, char *,
                                      char *, long, signed long *);
extern char                *map_db_errafd(void);

#define DB_OKAY 100000L
#endif
#define MAP_TIMEOUT 1200    /* 20 Minutes */


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
#ifdef _WITH_MAP_SUPPORT
   int              current_toggle;
#ifdef WITH_ARCHIVE_COPY_INFO
   unsigned int     archived_copied = 0;
#endif
   signed long      map_errno;
   time_t           last_update_time,
                    now;
   char             *p_source_file,
                    *p_file_name_buffer,
                    file_path[MAX_PATH_LENGTH],
                    source_file[MAX_PATH_LENGTH];
   struct job       *p_db;
#ifdef SA_FULLDUMP
   struct sigaction sact;
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
   if (atexit(sf_map_exit) != 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not register exit function : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Initialise variables. */
   local_file_counter = 0;
   msg_str[0] = '\0';
   files_to_send = init_sf(argc, argv, file_path, MAP_FLAG);
   p_db = &db;

   if ((signal(SIGINT, sig_kill) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_kill) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR) ||
       (signal(SIGALRM, sig_handler) == SIG_ERR))
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not set signal handler to catch SIGINT : %s",
                 strerror(errno));
      exit(INCORRECT);
   }

   /* Inform FSA that we have are ready to copy the files. */
   if (gsf_check_fsa(p_db) != NEITHER)
   {
      fsa->job_status[(int)db.job_no].connect_status = MAP_ACTIVE;
      fsa->job_status[(int)db.job_no].no_of_files = files_to_send;
   }

   /* Prepare pointers and directory name. */
   (void)strcpy(source_file, file_path);
   p_source_file = source_file + strlen(source_file);
   *p_source_file++ = '/';

   /* Now determine the real hostname. */
   if (db.toggle_host == YES)
   {
      if (fsa->host_toggle == HOST_ONE)
      {
         (void)strcpy(db.hostname,
                      fsa->real_hostname[HOST_TWO - 1]);
         current_toggle = HOST_TWO;
      }
      else
      {
         (void)strcpy(db.hostname,
                      fsa->real_hostname[HOST_ONE - 1]);
         current_toggle = HOST_ONE;
      }
   }
   else
   {
      (void)strcpy(db.hostname,
                   fsa->real_hostname[(int)(fsa->host_toggle - 1)]);
      current_toggle = (int)(fsa->host_toggle);
   }

#ifdef _OUTPUT_LOG
   if (db.output_log == YES)
   {
# ifdef WITHOUT_FIFO_RW_SUPPORT
      output_log_fd(&ol_fd, &ol_readfd, &db.output_log);
# else
      output_log_fd(&ol_fd, &db.output_log);
# endif
      output_log_ptrs(&ol_retries,
                      &ol_job_number,
                      &ol_data,
                      &ol_file_name,
                      &ol_file_name_length,
                      &ol_archive_name_length,
                      &ol_file_size,
                      &ol_unl,
                      &ol_size,
                      &ol_transfer_time,
                      &ol_output_type,
                      db.host_alias,
                      (current_toggle - 1),
                      MAP,
                      &db.output_log);
   }
#endif

   /* Send all files. */
   p_file_name_buffer = file_name_buffer;
   p_file_size_buffer = file_size_buffer;
   last_update_time = time(NULL);
   local_file_size = 0;
   for (files_send = 0; files_send < files_to_send; files_send++)
   {
      /* Get the the name of the file we want to send next. */
      (void)strcpy(p_source_file, p_file_name_buffer);

#ifdef WITH_DUP_CHECK
# ifndef FAST_SF_DUPCHECK
      if ((db.dup_check_timeout > 0) &&
          (isdup(fullname, p_file_name_buffer, *p_file_size_buffer,
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
            if (statx(0, fullname, AT_STATX_SYNC_AS_STAT,
                      STATX_MTIME, &stat_buf) == -1)
#  else
            if (stat(fullname, &stat_buf) == -1)
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
         handle_dupcheck_delete(SEND_FILE_MAP, fsa->host_alias, fullname,
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

      if (*p_file_size_buffer > 0)
      {
         /*
          * Due to constant hang ups of the map functions its necessary
          * to timeout these functions. :-((((
          */
         if (setjmp(env_alrm) != 0)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                      "Map function timed out!");
            rm_dupcheck_crc(fullname, p_file_name_buffer, *p_file_size_buffer);
            exit(MAP_FUNCTION_ERROR);
         }
         (void)alarm(MAP_TIMEOUT);

         /*
          * Determine if we want to print a job or send it to the
          * MAP database. We determine this by checking if db.user
          * is numeric or not. If it is numeric send it to the
          * MAP database, otherwise it is a print job.
          */
         if (isdigit(db.user[0]) == 0)
         {
            int  fax_error;
            char fax_print_error_str[80];

            if ((fax_error = faxe_print(source_file,       /* FSS name 8-O */
                                        p_file_name_buffer,/* FAX_E name   */
                                        db.hostname,       /* Remote host  */
                                        db.user,           /* Print format */
                                        fax_print_error_str)) < 0)
            {
               (void)alarm(0);
               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               "Failed to print file `%s' to %s [FAX PRINT ERROR %d].",
                               p_file_name_buffer, db.hostname, fax_error);
                  trans_db_log(ERROR_SIGN, NULL, 0, NULL, "%s",
                               fax_print_error_str);
               }
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Failed to print file `%s' to %s [FAX PRINT ERROR %d].",
                         p_file_name_buffer, db.hostname, fax_error);
               trans_log(ERROR_SIGN, NULL, 0, NULL, NULL,
                         "%s", fax_print_error_str);
               trans_log(INFO_SIGN, NULL, 0, NULL, NULL,
#if SIZEOF_OFF_T == 4
                         "%lu bytes printed in %d file(s).",
#else
                         "%llu bytes printed in %d file(s).",
#endif
                         fsa->job_status[(int)db.job_no].file_size_done,
                         fsa->job_status[(int)db.job_no].no_of_files_done);
               rm_dupcheck_crc(fullname, p_file_name_buffer,
                               *p_file_size_buffer);
               exit(MAP_FUNCTION_ERROR);
            }
            else
            {
               (void)alarm(0);
               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                               "Printed file `%s' to %s.",
                               p_file_name_buffer, db.hostname);
               }
            }
         }
         else /* Send to MAP database. */
         {
            store_blob(strtoul(db.user, NULL, 10),  /* Datentyp des MFS-Objekts */
                       file_path,                   /* source directory         */
                       p_file_name_buffer,
                       p_file_name_buffer,          /* Objektname das gespeichert werden soll */
                       db.hostname,                 /* Remote hostname          */
                       db.port,                     /* Schluessel der Empfangskette des MFS   */
                       &map_errno);
            (void)alarm(0);
            if (map_errno != DB_OKAY)
            {
               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               "Failed to send file `%s' to %s:%d [MAP ERROR %ld].",
                               p_file_name_buffer, db.hostname, db.port,
                               map_errno);
                  trans_db_log(ERROR_SIGN, NULL, 0, NULL, "%s",
                               map_db_errafd());
               }
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Failed to send file `%s' to %s:%d [MAP ERROR %ld].",
                         p_file_name_buffer, db.hostname, db.port, map_errno);
               trans_log(ERROR_SIGN, NULL, 0, NULL, NULL,
                         "%s", map_db_errafd());
               trans_log(INFO_SIGN, NULL, 0, NULL, NULL,
#if SIZEOF_OFF_T == 4
                         "%lu bytes sent in %d file(s).",
#else
                         "%llu bytes sent in %d file(s).",
#endif
                         fsa->job_status[(int)db.job_no].file_size_done,
                         fsa->job_status[(int)db.job_no].no_of_files_done);
               rm_dupcheck_crc(fullname, p_file_name_buffer,
                               *p_file_size_buffer);
               exit(MAP_FUNCTION_ERROR);
            }
            else
            {
               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                               "Send file `%s' to %s:%d [%ld].",
                               p_file_name_buffer, db.hostname,
                               db.port, map_errno);
               }
            }
         }
      }
      else
      {
         trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                   "Ignoring file `%s', since MAP can't handle files with %d bytes length.",
                   p_file_name_buffer, *p_file_size_buffer);
      }

#ifdef _OUTPUT_LOG
      if (db.output_log == YES)
      {
         end_time = times(&tmsdummy);
      }
#endif

      /* Tell FSA we have send a file !!!! */
      if (gsf_check_fsa(p_db) != NEITHER)
      {
         fsa->job_status[(int)db.job_no].file_name_in_use[0] = '\0';
         fsa->job_status[(int)db.job_no].no_of_files_done = files_send + 1;
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
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                            "Failed to archive file `%s'", p_file_name_buffer);
            }

            /*
             * NOTE: We _MUST_ delete the file we just send,
             *       else the file directory will run full!
             */
            if (unlink(source_file) < 0)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not unlink() local file `%s' after copying it successfully : %s",
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
                            "Archived file `%s'.", p_file_name_buffer);
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
         /* Delete the file we just have copied. */
         if (unlink(source_file) < 0)
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
                       "Could not unlink() local file `%s' after copying it successfully : %s",
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
       * After each successful transfer set error
       * counter to zero, so that other jobs can be
       * started.
       */
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
         error_action(fsa->host_alias, "start", HOST_SUCCESS_ACTION,
                      transfer_log_fd);
      }
#ifdef WITH_DUP_CHECK
# ifndef FAST_SF_DUPCHECK
      }
# endif
#endif

      p_file_name_buffer += MAX_FILENAME_LENGTH;
      p_file_size_buffer++;
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

   WHAT_DONE("send", fsa->job_status[(int)db.job_no].file_size_done, files_send);

   /*
    * Remove file directory.
    */
   if (rmdir(file_path) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to remove directory `%s' : %s",
                 file_path, strerror(errno));
   }
#endif /* _WITH_MAP_SUPPORT */

   exitflag = 0;
   exit(TRANSFER_SUCCESS);
}


#ifdef _WITH_MAP_SUPPORT
/*+++++++++++++++++++++++++++++ sf_map_exit() +++++++++++++++++++++++++++*/
static void
sf_map_exit(void)
{
   reset_fsa((struct job *)&db, exitflag, 0, 0);

   if ((fsa != NULL) && (db.fsa_pos != INCORRECT) && (fsa_pos_save == YES))
   {
      fsa_detach_pos(db.fsa_pos);
   }
   if (file_name_buffer != NULL)
   {
      free(file_name_buffer);
   }
   if (file_size_buffer != NULL)
   {
      free(file_size_buffer);
   }

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


/*++++++++++++++++++++++++++++++ sig_handler() ++++++++++++++++++++++++++*/
static void
sig_handler(int signo)
{
   longjmp(env_alrm, 1);
}


/*++++++++++++++++++++++++++++++ sig_kill() +++++++++++++++++++++++++++++*/
static void
sig_kill(int signo)
{
   exitflag = 0;
   if ((fsa != NULL) && (fsa_pos_save == YES) &&
       (fsa->job_status[(int)db.job_no].unique_name[2] == 5)
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
#endif /* _WITH_MAP_SUPPORT */
