/*
 *  sf_dfax.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2015 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   sf_dfax - sends FAX via the Diva API
 **
 ** SYNOPSIS
 **   sf_dfax [--version] [-w <directory>] -m <message-file>
 **
 ** DESCRIPTION
 **   sf_dfax sends files to a FAX card via the Diva API.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has
 **   occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   20.06.2015 H.Kiehl Created
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
#include <dssdk.h>
#include <errno.h>
#include "fddefs.h"
#include "version.h"

#define DFAX_CONNECTED    1
#define DFAX_DISCONNECTED 2
#define DFAX_SENT         3
#define DFAX_TIMEOUT      4

typedef struct
{
   DivaCallHandle hCall;
   DWORD          Id;
   BOOL           bIncoming;
} SingleCall;

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
DivaAppHandle              hApp = NULL;
SingleCall                 Call;

/* Local global variables. */
static int                 diva_status,
                           files_send,
                           files_to_send,
                           local_file_counter;
static off_t               local_file_size,
                           *p_file_size_buffer;
static char                *p_fax_file;

/* Local function prototypes. */
static void                CallbackHandler(DivaAppHandle, DivaEvent, PVOID, PVOID),
                           sf_dfax_exit(void),
                           sig_bus(int),
                           sig_segv(int),
                           sig_kill(int),
                           sig_exit(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int              current_toggle;
#ifdef WITH_ARCHIVE_COPY_INFO
   unsigned int     archived_copied = 0;
#endif
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
   DWORD            CallType = DivaCallTypeFax,
                    Device = LINEDEV_ALL,
                    MaxRate = 0;
   BOOL             bDisableECM = FALSE,
                    bSet = TRUE;
   SingleCall       *pCall;

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
   if (atexit(sf_dfax_exit) != 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not register exit function : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Initialise variables. */
   local_file_counter = 0;
   msg_str[0] = '\0';
   files_to_send = init_sf(argc, argv, file_path, DFAX_FLAG);
   p_db = &db;

   if ((signal(SIGINT, sig_kill) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_kill) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not set signal handler to catch SIGINT : %s",
                 strerror(errno));
      exit(INCORRECT);
   }

   /* Inform FSA that we have are ready to copy the files. */
   if (gsf_check_fsa(p_db) != NEITHER)
   {
      fsa->job_status[(int)db.job_no].connect_status = DFAX_ACTIVE;
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
                      DFAX,
                      &db.output_log);
   }
#endif

   if (DivaInitialize() != DivaSuccess)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                "DivaInitialize() failed.");
      exit(DFAX_FUNCTION_ERROR);
   }
   else
   {
      if (fsa->debug > NORMAL_MODE)
      {
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                      "DivaInitialize() success : Version %u.%u",
                       (DivaGetVersion() >> 16) & 0xffff,
                       DivaGetVersion() & 0xffff);
      }
   }

   if (DivaRegister(&hApp, DivaEventModeCallback,
                    (void *)CallbackHandler, 0, 0, 5, 512) != DivaSuccess)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                "DivaRegister() failed to register callback function.");
      exit(DFAX_FUNCTION_ERROR);
   }
   else
   {
      if (fsa->debug > NORMAL_MODE)
      {
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                      "DivaRegister() successfully registered callback function.");
      }
   }

   if (DivaListen(hApp, DivaListenAll, Device, "") != DivaSuccess)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                "DivaListen() failed.");
      exit(DFAX_FUNCTION_ERROR);
   }
   else
   {
      if (fsa->debug > NORMAL_MODE)
      {
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                      "DivaListen() success.");
      }
   }

   Call.Id = 1;
   Call.hCall = NULL;
   pCall = &Call;

   if (DivaCreateCall(hApp, pCall, &(pCall->hCall)) != DivaSuccess)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                "DivaCreateCall() failed.");
      exit(DFAX_FUNCTION_ERROR);
   }
   else
   {
      if (fsa->debug > NORMAL_MODE)
      {
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                      "DivaCreateCall() success.");
      }
   }

   /* Set some call properties. */
   if (DivaSetCallProperties(pCall->hCall, DivaCPT_LineDevice,
                             (DivaCallPropertyValue *)&Device,
                             sizeof(DWORD)) != DivaSuccess)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                "DivaSetCallProperties() Line Device failed.");
      exit(DFAX_FUNCTION_ERROR);
   }
   else
   {
      if (fsa->debug > NORMAL_MODE)
      {
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                      "DivaSetCallProperties() Line Device success.");
      }
   }
   if (DivaSetCallProperties(pCall->hCall, DivaCPT_CallType,
                             (DivaCallPropertyValue *)&CallType,
                             sizeof(DWORD)) != DivaSuccess)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                "DivaSetCallProperties() Call Type failed.");
      exit(DFAX_FUNCTION_ERROR);
   }
   else
   {
      if (fsa->debug > NORMAL_MODE)
      {
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                      "DivaSetCallProperties() Call Type success.");
      }
   }
   if (DivaSetCallProperties(pCall->hCall, DivaCPT_EnableFaxStatusReporting,
                             (DivaCallPropertyValue *)&bSet,
                             sizeof(BOOL)) != DivaSuccess)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                "DivaSetCallProperties() Enable Fax Status Reporting failed.");
      exit(DFAX_FUNCTION_ERROR);
   }
   else
   {
      if (fsa->debug > NORMAL_MODE)
      {
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                      "DivaSetCallProperties() Enable Fax Status Reporting success.");
      }
   }
#ifdef WITH_POLLING
   if (DivaSetCallProperties(pCall->hCall, DivaCPT_FaxEnablePolling,
                             (DivaCallPropertyValue *)&bSet,
                             sizeof(BOOL)) != DivaSuccess)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                "DivaSetCallProperties() to enable polling failed.");
      exit(DFAX_FUNCTION_ERROR);
   }
   else
   {
      if (fsa->debug > NORMAL_MODE)
      {
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                      "DivaSetCallProperties() to enable polling success.");
      }
   }
#endif
   if (bDisableECM)
   {
      if (DivaSetCallProperties(pCall->hCall, DivaCPT_FaxDisableECM,
                                (DivaCallPropertyValue *)&bSet,
                                sizeof(BOOL)) != DivaSuccess)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                   "DivaSetCallProperties() Fax Disable ECM failed.");
         exit(DFAX_FUNCTION_ERROR);
      }
      else
      {
         if (fsa->debug > NORMAL_MODE)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                         "DivaSetCallProperties() Fax Disable ECM success.");
         }
      }
   }
   if (MaxRate)
   {
      if (DivaSetCallProperties(pCall->hCall, DivaCPT_FaxMaxSpeed,
                                (DivaCallPropertyValue *)&MaxRate,
                                sizeof(DWORD)) != DivaSuccess)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                   "DivaSetCallProperties() Fax Max Speed failed.");
         exit(DFAX_FUNCTION_ERROR);
      }
      else
      {
         if (fsa->debug > NORMAL_MODE)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                         "DivaSetCallProperties() Fax Max Speed success.");
         }
      }
   }

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
         handle_dupcheck_delete(SEND_FILE_DFAX, fsa->host_alias, fullname,
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

      if (DivaDial(pCall->hCall, db.hostname) != DivaSuccess)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                   "DivaDial() failed.");
         rm_dupcheck_crc(fullname, p_file_name_buffer, *p_file_size_buffer);
         exit(DFAX_FUNCTION_ERROR);
      }
      else
      {
         if (fsa->debug > NORMAL_MODE)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                         "DivaDial() success.");
         }
      }

      diva_status = 0;
      p_fax_file = p_file_name_buffer;
      for (;;)
      {
         if (diva_status != 0)
         {
            if (diva_status == DFAX_CONNECTED)
            {
               if (gsf_check_fsa(p_db) != NEITHER)
               {
                  fsa->job_status[(int)db.job_no].connect_status = DFAX_ACTIVE;
               }
               diva_status = 0;
            }
            else if (diva_status == DFAX_DISCONNECTED)
                 {
                    if (gsf_check_fsa(p_db) != NEITHER)
                    {
                       fsa->job_status[(int)db.job_no].connect_status = DISCONNECT;
                    }
                    break; /* or exit() ??? */
                 }
            else if (diva_status == DFAX_SENT)
                 {
                    if (gsf_check_fsa(p_db) != NEITHER)
                    {
                       fsa->job_status[(int)db.job_no].connect_status = DISCONNECT;
                    }
                    break;
                 }
            else if (diva_status == DFAX_TIMEOUT)
                 {
                    if (gsf_check_fsa(p_db) != NEITHER)
                    {
                       fsa->job_status[(int)db.job_no].connect_status = DISCONNECT;
                    }
                    rm_dupcheck_crc(fullname, p_file_name_buffer,
                                    *p_file_size_buffer);
                    exit(TIMEOUT_ERROR);
                 }
         }
         (void)my_usleep(100000L);
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

   WHAT_DONE("faxed", fsa->job_status[(int)db.job_no].file_size_done, files_send);

   DivaUnregister(hApp);
   DivaTerminate();

   /*
    * Remove file directory.
    */
   if (rmdir(file_path) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to remove directory `%s' : %s",
                 file_path, strerror(errno));
   }

   exitflag = 0;
   exit(TRANSFER_SUCCESS);
}


/*----------------------------------------------------------------------*/
/* Callback handler for events from the Diva SDK.                       */
/*----------------------------------------------------------------------*/
static void
CallbackHandler(DivaAppHandle hApp,
                DivaEvent     Event,
                PVOID         Param1,
                PVOID         Param2)
{
   SingleCall                  *pCall;
   DWORD                       EventMask;
   BOOL                        bValue;
   DivaFaxTrainingStats        TrainingStats;
   DWORD                       dwValue;
   BYTE                        Buffer[2048];
   DivaFaxPageQualityDetails   QualityDetails;
   DivaFaxPartialPageDetails   PageReport;

   if (Event == DivaEventIncomingCall)
   {
      /* Ignore any incoming call. */
      if (fsa->debug > NORMAL_MODE)
      {
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                      "Incoming call, ignoring.");
      }
      return;
   }

   /*
    * If the call has not been answered there is no application handle.
    * If this is the disconnect event, close the call.
    */
   if (Param1 == INVALID_APP_CALL_HANDLE)
   {
      if (Event == DivaEventCallDisconnected)
      {
         DivaCloseCall(Param2);
      }

      return;
   }

   /*
    * All following event handling is based on the single call instance.
    */
   pCall = (SingleCall *)Param1;

   switch (Event)
   {
      case DivaEventCallProgress:
         if (((DivaCallState)(long)Param2) == DivaCallStateConnected)
         {
            DivaReportDTMF(pCall->hCall, TRUE);
         }
         break;

      case DivaEventCallConnected:
         diva_status = DFAX_CONNECTED;
         if (fsa->debug > NORMAL_MODE)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                         "Outgoing call, connected to %d.", pCall->Id);
         }
         if (DivaSendFax(pCall->hCall, p_fax_file, DivaFaxFormatAutodetect) != DivaSuccess)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                      "Failed to initiate fax to %d.", pCall->Id);
            if (DivaDisconnect(pCall->hCall) != DivaSuccess)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "DivaDisconnect() failed.");
            }
         }
         break;

      case DivaEventCallDisconnected:
         diva_status = DFAX_DISCONNECTED;
         if (fsa->debug > NORMAL_MODE)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                         "Disconnected %d.", pCall->Id);
         }
         pCall->hCall = NULL;
         DivaCloseCall(Param2);
         break;

      case DivaEventDTMFReceived:
         if (((char)(long)Param2) == 'Y')
         {
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                            "Fax answer tone detected for %d.", pCall->Id);
            }
         }
         break;

      case DivaEventFaxSent:
         diva_status = DFAX_SENT;
         if (fsa->debug > NORMAL_MODE)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                         "Fax for %d sent successfully.", pCall->Id);
         }
         if (DivaDisconnect(pCall->hCall) != DivaSuccess)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                      "DivaDisconnect() failed.");
         }
         break;

      case DivaEventFaxPageSent:
         if (fsa->debug > NORMAL_MODE)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                         "Fax page sent for %d.", pCall->Id);
         }
         break;

      case DivaEventDetailedFaxStatus:
         EventMask = (DWORD)(long)Param2;
         if (EventMask & DivaFaxStatusTrainingResult)
         {
            DivaGetCallProperties(pCall->hCall, DivaCPT_FaxReportTrainingResult,
                                  (DivaCallPropertyValue *)&bValue,
                                  sizeof(BOOL));
            DivaGetCallProperties(pCall->hCall, DivaCPT_TxSpeed,
                                  (DivaCallPropertyValue *)&dwValue,
                                  sizeof(DWORD));
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                            "Training for %d %s [%d].",
                            dwValue, bValue ? "succeeded" : "failed",
                            pCall->Id);
            }
         }
    
         if (EventMask & DivaFaxStatusTrainingStatistics)
         {
            DivaGetCallProperties(pCall->hCall, DivaCPT_FaxReportTrainingStats,
                                  (DivaCallPropertyValue *)&TrainingStats,
                                  sizeof(TrainingStats));
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                            "Training results: Good %d  Error %d  Noise %d [%d].",
                            TrainingStats.GoodBytes, TrainingStats.ErrorBytes,
                            TrainingStats.Noise, pCall->Id);
            }
         }

         if (EventMask & DivaFaxStatusPhaseReport)
         {
            DivaGetCallProperties(pCall->hCall, DivaCPT_FaxT30Phase,
                                  (DivaCallPropertyValue *)&dwValue,
                                  sizeof(DWORD));
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                            "Change to phase %c [%d].",
                            0x40 + (char)dwValue, pCall->Id);
            }
         }

         if (EventMask & DivaFaxStatusDCSReport)
         {
            Buffer[0] = 0;
            DivaGetCallProperties(pCall->hCall, DivaCPT_FaxReportDCS,
                                  (DivaCallPropertyValue *)&Buffer,
                                  sizeof(Buffer));
#ifdef WITH_TRACE
            trace_log(NULL, 0, R_TRACE, NULL, 0, "DCS Report [%d]:", pCall->Id);
            trace_log(NULL, 0, R_TRACE, Buffer + 1, (int)Buffer[0]);
#endif
         }

         if (EventMask & DivaFaxStatusDISReport)
         {
            Buffer[0] = 0;
            DivaGetCallProperties(pCall->hCall, DivaCPT_FaxRemoteFeatures,
                                  (DivaCallPropertyValue *)&Buffer,
                                  sizeof(Buffer));
#ifdef WITH_TRACE
            trace_log(NULL, 0, R_TRACE, NULL, 0, "DIS Report [%d]:", pCall->Id);
            trace_log(NULL, 0, R_TRACE, Buffer + 1, (int)Buffer[0]);
#endif
         }

         if (EventMask & DivaFaxStatusQualityReport)
         {
            DivaGetCallProperties(pCall->hCall, DivaCPT_FaxReportPageQuality,
                                  (DivaCallPropertyValue *)&QualityDetails,
                                  sizeof(QualityDetails));
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                            "Page Quality %d/%d/%d,  %d/%d/%d [%d].",
                            QualityDetails.TotalScanLines,
                            QualityDetails.ErrorScanLines,
                            QualityDetails.ConsecutiveErrors,
                            QualityDetails.TotalBytes,
                            QualityDetails.ErrorBytes,
                            QualityDetails.ConsecutiveErrorBytes, pCall->Id);
            }
         }

         if (EventMask & DivaFaxStatusPartialPageReport)
         {
            DivaGetCallProperties(pCall->hCall, DivaCPT_FaxReportPartialPage,
                                  (DivaCallPropertyValue *)&PageReport,
                                  sizeof(PageReport));
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                            "ECM Frame Length %d, PPS Length %d [%d].",
                            PageReport.ECMFrameLength,
                            PageReport.PPSFrameLength, pCall->Id);
            }
#ifdef WITH_TRACE
            trace_log(NULL, 0, R_TRACE, NULL, 0, "ECM State [%d]:", pCall->Id);
            trace_log(NULL, 0, R_TRACE, PageReport.ECMState,
                      sizeof(PageReport.ECMState));
            trace_log(NULL, 0, R_TRACE, NULL, 0, "PPS Frame [%d]:", pCall->Id);
            trace_log(NULL, 0, R_TRACE, PageReport.PPSFrame,
                      PageReport.PPSFrameLength);
#endif
         }

         if (EventMask & DivaFaxStatusTimeoutReport)
         {
            DivaGetCallProperties(pCall->hCall, DivaCPT_FaxReportT30Timeout,
                                  (DivaCallPropertyValue *)&dwValue,
                                  sizeof(DWORD));
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                            "Fax Timeout %d [%d].", dwValue, pCall->Id);
            }
            diva_status = DFAX_TIMEOUT;
         }

         if (EventMask & DivaFaxStatusResultReport)
         {
            DivaGetCallProperties(pCall->hCall, DivaCPT_FaxResultReport,
                                  (DivaCallPropertyValue *)&dwValue,
                                  sizeof(DWORD));
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                            "Fax result code %d [%d].",
                            dwValue, pCall->Id);
            }
         }
         break;

      default:
         break;
   }

   return;
}


/*++++++++++++++++++++++++++++ sf_dfax_exit() +++++++++++++++++++++++++++*/
static void
sf_dfax_exit(void)
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
