/*
 *  convert_fsa.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   convert_fsa - converts the FSA from an old format to new one
 **
 ** SYNOPSIS
 **   char *convert_fsa(int           old_fsa_fd,
 **                     char          *old_fsa_stat,
 **                     off_t         *old_fsa_size,
 **                     int           old_no_of_hosts,
 **                     unsigned char old_version,
 **                     unsigned char new_version)
 **
 ** DESCRIPTION
 **   When there is a change in the structure filetransfer_status (FSA)
 **   This function converts an old FSA to the new one. This one
 **   is currently for converting Version 0, 1, 2 and 3.
 **
 ** RETURN VALUES
 **   When successful it returns a pointer to the converted structure,
 **   otherwise NULL is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   26.09.2002 H.Kiehl Created
 **   16.02.2006 H.Kiehl Added Version 2.
 **   31.12.2007 H.Kiehl Added Version 3.
 **   08.08.2021 H.Kiehl Added Version 4.
 **   25.02.2023 H.Kiehl Restore feature flag.
 **
 */
DESCR__E_M1

#include <stdio.h>                    /* fprintf()                       */
#include <string.h>                   /* strcpy(), strcat(), strerror()  */
#include <stdlib.h>                   /* exit()                          */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>                /* mmap()                          */
#endif
#include <unistd.h>                   /* sysconf()                       */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>                   /* O_RDWR, O_CREAT, O_WRONLY, etc  */
#endif
#include <errno.h>
#include "amgdefs.h"

/* Version 0 */
#define MAX_REAL_HOSTNAME_LENGTH_0 40
#define MAX_PROXY_NAME_LENGTH_0    80
#define MAX_TOGGLE_STR_LENGTH_0    5
#define MAX_HOSTNAME_LENGTH_0      8
#define MAX_NO_PARALLEL_JOBS_0     5
#define MAX_MSG_NAME_LENGTH_0      30
#define MAX_FILENAME_LENGTH_0      256
#define AFD_WORD_OFFSET_0          8

#define GET_FTP_FLAG_0             16777216
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
# define STAT_KEEPALIVE_0          4096
#endif
#define SET_IDLE_TIME_0            2048
#define FTP_PASSIVE_MODE_0         1024
#define RETRIEVE_FLAG_0            512
#define SEND_FLAG_0                256
struct status_0
       {
          pid_t           proc_id;
#ifdef _WITH_BURST_2
          char            error_file;
          char            unique_name[MAX_MSG_NAME_LENGTH_0];
          unsigned char   burst_counter;
          unsigned int    job_id;
#endif
          char            connect_status;
          int             no_of_files;
          int             no_of_files_done;
          unsigned long   file_size;
          unsigned long   file_size_done;
          unsigned long   bytes_send;
          char            file_name_in_use[MAX_FILENAME_LENGTH_0];
          unsigned long   file_size_in_use;
          unsigned long   file_size_in_use_done;
       };
struct filetransfer_status_0
       {
          char            host_alias[MAX_HOSTNAME_LENGTH_0 + 1];
          char            real_hostname[2][MAX_REAL_HOSTNAME_LENGTH_0];
          char            host_dsp_name[MAX_HOSTNAME_LENGTH_0 + 1];
          char            proxy_name[MAX_PROXY_NAME_LENGTH_0 + 1];
          char            host_toggle_str[MAX_TOGGLE_STR_LENGTH_0];
          char            toggle_pos;
          char            original_toggle_pos;
          char            auto_toggle;
          signed char     file_size_offset;
          int             successful_retries;
          int             max_successful_retries;
          unsigned char   special_flag;
          unsigned int    protocol;
          char            debug;
          char            host_toggle;
          int             host_status;
          int             error_counter;
          unsigned int    total_errors;
          int             max_errors;
          int             retry_interval;
          int             block_size;
          time_t          last_retry_time;
          time_t          last_connection;
          int             total_file_counter;
          unsigned long   total_file_size;
          unsigned int    jobs_queued;
          unsigned int    file_counter_done;
          unsigned long   bytes_send;
          unsigned int    connections;
          int             active_transfers;
          int             allowed_transfers;
          long            transfer_timeout;
          struct status_0 job_status[MAX_NO_PARALLEL_JOBS_0];
       };

/* Version 1 */
#define MAX_REAL_HOSTNAME_LENGTH_1 40
#define MAX_PROXY_NAME_LENGTH_1    80
#define MAX_TOGGLE_STR_LENGTH_1    5
#define MAX_HOSTNAME_LENGTH_1      8
#define MAX_NO_PARALLEL_JOBS_1     5
#define MAX_ADD_FNL_1              30
#define MAX_MSG_NAME_LENGTH_1      (MAX_ADD_FNL_1 + 11)
#define MAX_FILENAME_LENGTH_1      256
#define AFD_WORD_OFFSET_1          (SIZEOF_INT + 4 + SIZEOF_INT + 4)
#define ERROR_HISTORY_LENGTH_1     5

#define GET_FTP_FLAG_1             32768
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
# define STAT_KEEPALIVE_1          4
#endif
#define SET_IDLE_TIME_1            2
#define FTP_PASSIVE_MODE_1         1
#define RETRIEVE_FLAG_1            2147483648U
#define SEND_FLAG_1                1073741824
struct status_1
       {
          pid_t           proc_id;
#ifdef _WITH_BURST_2
          char            unique_name[MAX_MSG_NAME_LENGTH_1];
          unsigned int    job_id;
#endif
          char            connect_status;
          int             no_of_files;
          int             no_of_files_done;
          off_t           file_size;
          u_off_t         file_size_done;
          u_off_t         bytes_send;
          char            file_name_in_use[MAX_FILENAME_LENGTH_1];
          off_t           file_size_in_use;
          off_t           file_size_in_use_done;
       };
struct filetransfer_status_1
       {
          char            host_alias[MAX_HOSTNAME_LENGTH_1 + 1];
          char            real_hostname[2][MAX_REAL_HOSTNAME_LENGTH_1];
          char            host_dsp_name[MAX_HOSTNAME_LENGTH_1 + 1];
          char            proxy_name[MAX_PROXY_NAME_LENGTH_1 + 1];
          char            host_toggle_str[MAX_TOGGLE_STR_LENGTH_1];
          char            toggle_pos;
          char            original_toggle_pos;
          char            auto_toggle;
          signed char     file_size_offset;
          int             successful_retries;
          int             max_successful_retries;
          unsigned char   special_flag;
          unsigned int    protocol;
          unsigned int    protocol_options;
          char            debug;
          char            host_toggle;
          int             host_status;
          int             error_counter;
          unsigned int    total_errors;
          int             max_errors;
          unsigned char   error_history[ERROR_HISTORY_LENGTH_1];
          int             retry_interval;
          int             block_size;
          int             ttl;
          time_t          last_retry_time;
          time_t          last_connection;
          time_t          first_error_time;
          int             total_file_counter;
          off_t           total_file_size;
          unsigned int    jobs_queued;
          unsigned int    file_counter_done;
          u_off_t         bytes_send;
          unsigned int    connections;
          unsigned int    mc_nack_counter;
          int             active_transfers;
          int             allowed_transfers;
          long            transfer_timeout;
          off_t           transfer_rate_limit;
          off_t           trl_per_process;
          off_t           mc_ct_rate_limit;
          off_t           mc_ctrl_per_process;
          struct status_1 job_status[MAX_NO_PARALLEL_JOBS_1];
       };

/* Version 2 */
#define MAX_REAL_HOSTNAME_LENGTH_2 MAX_REAL_HOSTNAME_LENGTH
#define MAX_PROXY_NAME_LENGTH_2    MAX_PROXY_NAME_LENGTH
#define MAX_TOGGLE_STR_LENGTH_2    MAX_TOGGLE_STR_LENGTH
#define MAX_HOSTNAME_LENGTH_2      MAX_HOSTNAME_LENGTH
#define MAX_NO_PARALLEL_JOBS_2     MAX_NO_PARALLEL_JOBS
#define MAX_MSG_NAME_LENGTH_2      MAX_MSG_NAME_LENGTH /* Changed. */
#define MAX_FILENAME_LENGTH_2      MAX_FILENAME_LENGTH
#define AFD_WORD_OFFSET_2          (SIZEOF_INT + 4 + SIZEOF_INT + 4)
#define ERROR_HISTORY_LENGTH_2     ERROR_HISTORY_LENGTH

#define GET_FTP_FLAG_2             32768
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
# define STAT_KEEPALIVE_2          4
#endif
#define SET_IDLE_TIME_2            2
#define FTP_PASSIVE_MODE_2         1
#define RETRIEVE_FLAG_2            2147483648U
#define SEND_FLAG_2                1073741824
struct status_2
       {
          pid_t           proc_id;
#ifdef _WITH_BURST_2
          char            unique_name[MAX_MSG_NAME_LENGTH_2];
          unsigned int    job_id;
#endif
          char            connect_status;
          int             no_of_files;
          int             no_of_files_done;
          off_t           file_size;
          u_off_t         file_size_done;
          u_off_t         bytes_send;
          char            file_name_in_use[MAX_FILENAME_LENGTH_2];
          off_t           file_size_in_use;
          off_t           file_size_in_use_done;
       };
struct filetransfer_status_2
       {
          char            host_alias[MAX_HOSTNAME_LENGTH_2 + 1];
          char            real_hostname[2][MAX_REAL_HOSTNAME_LENGTH_2];
          char            host_dsp_name[MAX_HOSTNAME_LENGTH_2 + 1];
          char            proxy_name[MAX_PROXY_NAME_LENGTH_2 + 1];
          char            host_toggle_str[MAX_TOGGLE_STR_LENGTH_2];
          char            toggle_pos;
          char            original_toggle_pos;
          char            auto_toggle;
          signed char     file_size_offset;
          int             successful_retries;
          int             max_successful_retries;
          unsigned char   special_flag;
          unsigned int    protocol;
          unsigned int    protocol_options;
          unsigned int    socksnd_bufsize;   /* New */
          unsigned int    sockrcv_bufsize;   /* New */
          unsigned int    keep_connected;    /* New */
#ifdef WITH_DUP_CHECK
          unsigned int    dup_check_flag;    /* New */
#endif
          unsigned int    host_id;           /* New */
          char            debug;
          char            host_toggle;
          unsigned int    host_status;       /* Modified */
          int             error_counter;
          unsigned int    total_errors;
          int             max_errors;
          unsigned char   error_history[ERROR_HISTORY_LENGTH_2];
          int             retry_interval;
          int             block_size;
          int             ttl;
#ifdef WITH_DUP_CHECK
          time_t          dup_check_timeout; /* New */
#endif
          time_t          last_retry_time;
          time_t          last_connection;
          time_t          first_error_time;
          int             total_file_counter;
          off_t           total_file_size;
          unsigned int    jobs_queued;
          unsigned int    file_counter_done;
          u_off_t         bytes_send;
          unsigned int    connections;
          unsigned int    mc_nack_counter;
          int             active_transfers;
          int             allowed_transfers;
          long            transfer_timeout;
          off_t           transfer_rate_limit;
          off_t           trl_per_process;
          off_t           mc_ct_rate_limit;
          off_t           mc_ctrl_per_process;
          struct status_2 job_status[MAX_NO_PARALLEL_JOBS_2];
       };

/* Version 3 */
#define MAX_REAL_HOSTNAME_LENGTH_3 MAX_REAL_HOSTNAME_LENGTH
#define MAX_PROXY_NAME_LENGTH_3    MAX_PROXY_NAME_LENGTH
#define MAX_TOGGLE_STR_LENGTH_3    MAX_TOGGLE_STR_LENGTH
#define MAX_HOSTNAME_LENGTH_3      MAX_HOSTNAME_LENGTH
#define MAX_NO_PARALLEL_JOBS_3     MAX_NO_PARALLEL_JOBS
#define MAX_MSG_NAME_LENGTH_3      MAX_MSG_NAME_LENGTH /* Changed. */
#define MAX_FILENAME_LENGTH_3      MAX_FILENAME_LENGTH
#define AFD_WORD_OFFSET_3          (SIZEOF_INT + 4 + SIZEOF_INT + 4)
#define ERROR_HISTORY_LENGTH_3     ERROR_HISTORY_LENGTH

#define GET_FTP_FLAG_3             32768
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
# define STAT_KEEPALIVE_3          4
#endif
#define SET_IDLE_TIME_3            2
#define FTP_PASSIVE_MODE_3         1
#define RETRIEVE_FLAG_3            2147483648U
#define SEND_FLAG_3                1073741824
struct status_3
       {
          pid_t           proc_id;
#ifdef _WITH_BURST_2
          char            unique_name[MAX_MSG_NAME_LENGTH_3];
          unsigned int    job_id;
#endif
          char            connect_status;
          int             no_of_files;
          int             no_of_files_done;
          off_t           file_size;
          u_off_t         file_size_done;
          u_off_t         bytes_send;
          char            file_name_in_use[MAX_FILENAME_LENGTH_3];
          off_t           file_size_in_use;
          off_t           file_size_in_use_done;
       };
struct filetransfer_status_3
       {
          char            host_alias[MAX_HOSTNAME_LENGTH_3 + 1];
          char            real_hostname[2][MAX_REAL_HOSTNAME_LENGTH_3];
          char            host_dsp_name[MAX_HOSTNAME_LENGTH_3 + 2]; /* Changed */
          char            proxy_name[MAX_PROXY_NAME_LENGTH_3 + 1];
          char            host_toggle_str[MAX_TOGGLE_STR_LENGTH_3];
          char            toggle_pos;
          char            original_toggle_pos;
          char            auto_toggle;
          signed char     file_size_offset;
          int             successful_retries;
          int             max_successful_retries;
          unsigned char   special_flag;
          unsigned int    protocol;
          unsigned int    protocol_options;
          unsigned int    socksnd_bufsize;
          unsigned int    sockrcv_bufsize;
          unsigned int    keep_connected;
#ifdef WITH_DUP_CHECK
          unsigned int    dup_check_flag;
#endif
          unsigned int    host_id;
          char            debug;
          char            host_toggle;
          unsigned int    host_status;
          int             error_counter;
          unsigned int    total_errors;
          int             max_errors;
          unsigned char   error_history[ERROR_HISTORY_LENGTH_3];
          int             retry_interval;
          int             block_size;
          int             ttl;
#ifdef WITH_DUP_CHECK
          time_t          dup_check_timeout;
#endif
          time_t          last_retry_time;
          time_t          last_connection;
          time_t          first_error_time;
          time_t          start_event_handle; /* New */
          time_t          end_event_handle;   /* New */
          time_t          warn_time;          /* New */
          int             total_file_counter;
          off_t           total_file_size;
          unsigned int    jobs_queued;
          unsigned int    file_counter_done;
          u_off_t         bytes_send;
          unsigned int    connections;
          unsigned int    mc_nack_counter;
          int             active_transfers;
          int             allowed_transfers;
          long            transfer_timeout;
          off_t           transfer_rate_limit;
          off_t           trl_per_process;
          off_t           mc_ct_rate_limit;
          off_t           mc_ctrl_per_process;
          struct status_3 job_status[MAX_NO_PARALLEL_JOBS_3];
       };

/* Version 4 */
#define MAX_REAL_HOSTNAME_LENGTH_4 MAX_REAL_HOSTNAME_LENGTH /* Changed. */
#define MAX_PROXY_NAME_LENGTH_4    MAX_PROXY_NAME_LENGTH
#define MAX_TOGGLE_STR_LENGTH_4    MAX_TOGGLE_STR_LENGTH
#define MAX_HOSTNAME_LENGTH_4      MAX_HOSTNAME_LENGTH
#define MAX_NO_PARALLEL_JOBS_4     MAX_NO_PARALLEL_JOBS
#define MAX_MSG_NAME_LENGTH_4      MAX_MSG_NAME_LENGTH /* Changed. */
#define MAX_FILENAME_LENGTH_4      MAX_FILENAME_LENGTH
#define AFD_WORD_OFFSET_4          (SIZEOF_INT + 4 + SIZEOF_INT + 4)
#define ERROR_HISTORY_LENGTH_4     ERROR_HISTORY_LENGTH

#define GET_FTP_FLAG_4             32768
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
# define STAT_KEEPALIVE_4          4
#endif
#define SET_IDLE_TIME_4            2
#define FTP_PASSIVE_MODE_4         1
#define RETRIEVE_FLAG_4            2147483648U
#define SEND_FLAG_4                1073741824
struct status_4
       {
          pid_t           proc_id;
#ifdef _WITH_BURST_2
          char            unique_name[MAX_MSG_NAME_LENGTH_4];
          unsigned int    job_id;
#endif
          unsigned char   special_flag;           /* New. */
          char            connect_status;
          int             no_of_files;
          int             no_of_files_done;
          off_t           file_size;
          u_off_t         file_size_done;
          u_off_t         bytes_send;
          char            file_name_in_use[MAX_FILENAME_LENGTH_4];
          off_t           file_size_in_use;
          off_t           file_size_in_use_done;
       };
struct filetransfer_status_4
       {
          char            host_alias[MAX_HOSTNAME_LENGTH_4 + 1];
          char            real_hostname[2][MAX_REAL_HOSTNAME_LENGTH_4];
          char            host_dsp_name[MAX_HOSTNAME_LENGTH_4 + 2];
          char            proxy_name[MAX_PROXY_NAME_LENGTH_4 + 1];
          char            host_toggle_str[MAX_TOGGLE_STR_LENGTH_4];
          char            toggle_pos;
          char            original_toggle_pos;
          char            auto_toggle;
          signed char     file_size_offset;
          int             successful_retries;
          int             max_successful_retries;
          unsigned char   special_flag;
          unsigned int    protocol;
          unsigned int    protocol_options;
          unsigned int    protocol_options2;      /* New. */
          unsigned int    socksnd_bufsize;
          unsigned int    sockrcv_bufsize;
          unsigned int    keep_connected;
#ifdef WITH_DUP_CHECK
          unsigned int    dup_check_flag;
#endif
          unsigned int    host_id;
          char            debug;
          char            host_toggle;
          unsigned int    host_status;
          int             error_counter;
          unsigned int    total_errors;
          int             max_errors;
          unsigned char   error_history[ERROR_HISTORY_LENGTH_4];
          int             retry_interval;
          int             block_size;
          int             ttl;
#ifdef WITH_DUP_CHECK
          time_t          dup_check_timeout;
#endif
          time_t          last_retry_time;
          time_t          last_connection;
          time_t          first_error_time;
          time_t          start_event_handle;
          time_t          end_event_handle;
          time_t          warn_time;
          int             total_file_counter;
          off_t           total_file_size;
          unsigned int    jobs_queued;
          unsigned int    file_counter_done;
          u_off_t         bytes_send;
          unsigned int    connections;
/*        unsigned int    mc_nack_counter;     */ /* Removed. */
          int             active_transfers;
          int             allowed_transfers;
          long            transfer_timeout;
          off_t           transfer_rate_limit;
          off_t           trl_per_process;
/*        off_t           mc_ct_rate_limit;    */ /* Removed. */
/*        off_t           mc_ctrl_per_process; */ /* Removed. */
          struct status_4 job_status[MAX_NO_PARALLEL_JOBS_4];
       };


/*############################ convert_fsa() ############################*/
char *
convert_fsa(int           old_fsa_fd,
            char          *old_fsa_stat,
            off_t         *old_fsa_size,
            int           old_no_of_hosts,
            unsigned char old_version,
            unsigned char new_version)
{
   int          i, j,
                pagesize;
   size_t       new_size;
   char         *ptr,
                old_features;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   if ((pagesize = (int)sysconf(_SC_PAGESIZE)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to determine the pagesize with sysconf() : %s",
                 strerror(errno));
   }

   if ((old_version == 0) && (new_version == 1))
   {
      struct filetransfer_status_0 *old_fsa;
      struct filetransfer_status_1 *new_fsa;

      /* Get the size of the old FSA file. */
#ifdef HAVE_STATX
      if (statx(old_fsa_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(old_fsa_fd, &stat_buf) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                    "Failed to statx() %s : %s",
#else
                    "Failed to fstat() %s : %s",
#endif
                    old_fsa_stat, strerror(errno));
         *old_fsa_size = -1;
         return(NULL);
      }
      else
      {
#ifdef HAVE_STATX
         if (stat_buf.stx_size > 0)
#else
         if (stat_buf.st_size > 0)
#endif
         {
#ifdef HAVE_MMAP
            if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                            stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                            stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                            MAP_SHARED, old_fsa_fd, 0)) == (caddr_t) -1)
#else
            if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                                stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                                stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                                MAP_SHARED, old_fsa_stat, 0)) == (caddr_t) -1)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to mmap() to %s : %s",
                          old_fsa_stat, strerror(errno));
               *old_fsa_size = -1;
               return(NULL);
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "FSA file %s is empty.", old_fsa_stat);
            *old_fsa_size = -1;
            return(NULL);
         }
      }

      old_features = *(ptr + SIZEOF_INT + 1);
      ptr += AFD_WORD_OFFSET_0;
      old_fsa = (struct filetransfer_status_0 *)ptr;

      new_size = old_no_of_hosts * sizeof(struct filetransfer_status_1);
      if ((ptr = malloc(new_size)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "malloc() error [%d %d] : %s",
                    old_no_of_hosts, new_size, strerror(errno));
         ptr = (char *)old_fsa;
         ptr -= AFD_WORD_OFFSET_0;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
         if (munmap(ptr, stat_buf.stx_size) == -1)
# else
         if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
         if (munmap_emu(ptr) == -1)
#endif
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to munmap() %s : %s",
                       old_fsa_stat, strerror(errno));
         }
         *old_fsa_size = -1;
         return(NULL);
      }
      (void)memset(ptr, 0, new_size);
      new_fsa = (struct filetransfer_status_1 *)ptr;

      /*
       * Copy all the old data into the new region.
       */
      for (i = 0; i < old_no_of_hosts; i++)
      {
         (void)strcpy(new_fsa[i].host_alias, old_fsa[i].host_alias);
         (void)strcpy(new_fsa[i].real_hostname[0], old_fsa[i].real_hostname[0]);
         (void)strcpy(new_fsa[i].real_hostname[1], old_fsa[i].real_hostname[1]);
         (void)strcpy(new_fsa[i].host_dsp_name, old_fsa[i].host_dsp_name);
         (void)strcpy(new_fsa[i].proxy_name, old_fsa[i].proxy_name);
         (void)strcpy(new_fsa[i].host_toggle_str, old_fsa[i].host_toggle_str);
         new_fsa[i].toggle_pos             = old_fsa[i].toggle_pos;
         new_fsa[i].original_toggle_pos    = old_fsa[i].original_toggle_pos;
         new_fsa[i].auto_toggle            = old_fsa[i].auto_toggle;
         new_fsa[i].file_size_offset       = old_fsa[i].file_size_offset;
         new_fsa[i].successful_retries     = old_fsa[i].successful_retries;
         new_fsa[i].max_successful_retries = old_fsa[i].max_successful_retries;
         new_fsa[i].special_flag           = old_fsa[i].special_flag;
         new_fsa[i].protocol               = 0;
         if (old_fsa[i].protocol & FTP_FLAG)
         {
            new_fsa[i].protocol |= FTP_FLAG;
         }
         if (old_fsa[i].protocol & LOC_FLAG)
         {
            new_fsa[i].protocol |= LOC_FLAG;
         }
         if (old_fsa[i].protocol & SMTP_FLAG)
         {
            new_fsa[i].protocol |= SMTP_FLAG;
         }
#ifdef _WITH_MAP_SUPPORT
         if (old_fsa[i].protocol & MAP_FLAG)
         {
            new_fsa[i].protocol |= MAP_FLAG;
         }
#endif
#ifdef _WITH_SCP_SUPPORT
         if (old_fsa[i].protocol & SCP_FLAG)
         {
            new_fsa[i].protocol |= SCP_FLAG;
         }
#endif
#ifdef _WITH_WMO_SUPPORT
         if (old_fsa[i].protocol & WMO_FLAG)
         {
            new_fsa[i].protocol |= WMO_FLAG;
         }
#endif
         if (old_fsa[i].protocol & GET_FTP_FLAG_0)
         {
            new_fsa[i].protocol |= GET_FTP_FLAG_1;
         }
         if (old_fsa[i].protocol & SEND_FLAG_0)
         {
            new_fsa[i].protocol |= SEND_FLAG_1;
         }
         if (old_fsa[i].protocol & RETRIEVE_FLAG_0)
         {
            new_fsa[i].protocol |= RETRIEVE_FLAG_1;
         }
         new_fsa[i].protocol_options = 0;
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
         if (old_fsa[i].protocol & STAT_KEEPALIVE_0)
         {
            new_fsa[i].protocol_options |= STAT_KEEPALIVE_1;
         }
#endif
         if (old_fsa[i].protocol & SET_IDLE_TIME_0)
         {
            new_fsa[i].protocol_options |= SET_IDLE_TIME_1;
         }
         if (old_fsa[i].protocol & FTP_PASSIVE_MODE_0)
         {
            new_fsa[i].protocol_options |= FTP_PASSIVE_MODE_1;
         }
         if (old_fsa[i].debug == NO)
         {
            new_fsa[i].debug               = NORMAL_MODE;
         }
         else
         {
            new_fsa[i].debug               = DEBUG_MODE;
         }
         new_fsa[i].host_toggle            = old_fsa[i].host_toggle;
         new_fsa[i].host_status            = old_fsa[i].host_status;
         new_fsa[i].error_counter          = old_fsa[i].error_counter;
         new_fsa[i].total_errors           = old_fsa[i].total_errors;
         new_fsa[i].max_errors             = old_fsa[i].max_errors;
         for (j = 0; j < ERROR_HISTORY_LENGTH; j++)
         {
            new_fsa[i].error_history[j] = 0;
         }
         new_fsa[i].retry_interval         = old_fsa[i].retry_interval;
         new_fsa[i].block_size             = old_fsa[i].block_size;
         new_fsa[i].ttl                    = 0;
         new_fsa[i].last_retry_time        = old_fsa[i].last_retry_time;
         new_fsa[i].last_connection        = old_fsa[i].last_connection;
         new_fsa[i].first_error_time       = 0L;
         new_fsa[i].total_file_counter     = old_fsa[i].total_file_counter;
         new_fsa[i].total_file_size        = (off_t)(old_fsa[i].total_file_size);
         new_fsa[i].jobs_queued            = old_fsa[i].jobs_queued;
         new_fsa[i].file_counter_done      = old_fsa[i].file_counter_done;
         new_fsa[i].bytes_send             = (u_off_t)(old_fsa[i].bytes_send);
         new_fsa[i].connections            = old_fsa[i].connections;
         new_fsa[i].mc_nack_counter        = 0;
         new_fsa[i].active_transfers       = old_fsa[i].active_transfers;
         new_fsa[i].allowed_transfers      = old_fsa[i].allowed_transfers;
         new_fsa[i].transfer_rate_limit    = 0;
         new_fsa[i].trl_per_process        = 0;
         new_fsa[i].mc_ct_rate_limit       = 0;
         new_fsa[i].mc_ctrl_per_process    = 0;
         new_fsa[i].transfer_timeout       = old_fsa[i].transfer_timeout;
         for (j = 0; j < MAX_NO_PARALLEL_JOBS_0; j++)
         {
            new_fsa[i].job_status[j].proc_id = old_fsa[i].job_status[j].proc_id;
#ifdef _WITH_BURST_2
            (void)strcpy(new_fsa[i].job_status[j].unique_name, old_fsa[i].job_status[j].unique_name);
            new_fsa[i].job_status[j].job_id = old_fsa[i].job_status[j].job_id;
#endif
            new_fsa[i].job_status[j].connect_status = old_fsa[i].job_status[j].connect_status;
            new_fsa[i].job_status[j].no_of_files = old_fsa[i].job_status[j].no_of_files;
            new_fsa[i].job_status[j].no_of_files_done = old_fsa[i].job_status[j].no_of_files_done;
            new_fsa[i].job_status[j].file_size = (off_t)old_fsa[i].job_status[j].file_size;
            new_fsa[i].job_status[j].file_size_done = (u_off_t)old_fsa[i].job_status[j].file_size_done;
            new_fsa[i].job_status[j].bytes_send = (u_off_t)old_fsa[i].job_status[j].bytes_send;
            (void)strcpy(new_fsa[i].job_status[j].file_name_in_use, old_fsa[i].job_status[j].file_name_in_use);
            new_fsa[i].job_status[j].file_size_in_use = (off_t)old_fsa[i].job_status[j].file_size_in_use;
            new_fsa[i].job_status[j].file_size_in_use_done = (off_t)old_fsa[i].job_status[j].file_size_in_use_done;
         }
      }

      ptr = (char *)old_fsa;
      ptr -= AFD_WORD_OFFSET_0;

      /*
       * Resize the old FSA to the size of new one and then copy
       * the new structure into it. Then update the FSA version
       * number.
       */
      if ((ptr = mmap_resize(old_fsa_fd, ptr,
                             new_size + AFD_WORD_OFFSET_1)) == (caddr_t) -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to mmap_resize() %s : %s",
                    old_fsa_stat, strerror(errno));
         free((void *)new_fsa);
         return(NULL);
      }
      ptr += AFD_WORD_OFFSET_1;
      (void)memcpy(ptr, new_fsa, new_size);
      free((void *)new_fsa);
      ptr -= AFD_WORD_OFFSET_1;
      *(ptr + SIZEOF_INT + 1) = old_features;
      *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
      *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
      *(int *)(ptr + SIZEOF_INT + 4) = pagesize;
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
      *old_fsa_size = new_size + AFD_WORD_OFFSET_1;

      system_log(INFO_SIGN, NULL, 0,
                 "Converted FSA from verion %d to %d.",
                 (int)old_version, (int)new_version);
   }
   else if ((old_version == 0) && (new_version == 2))
        {
           struct filetransfer_status_0 *old_fsa;
           struct filetransfer_status_2 *new_fsa;

           /* Get the size of the old FSA file. */
#ifdef HAVE_STATX
           if (statx(old_fsa_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                     STATX_SIZE, &stat_buf) == -1)
#else
           if (fstat(old_fsa_fd, &stat_buf) == -1)
#endif
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                         "Failed to statx() %s : %s",
#else
                         "Failed to fstat() %s : %s",
#endif
                         old_fsa_stat, strerror(errno));
              *old_fsa_size = -1;
              return(NULL);
           }
           else
           {
#ifdef HAVE_STATX
              if (stat_buf.stx_size > 0)
#else
              if (stat_buf.st_size > 0)
#endif
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                                 stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                                 stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                                 MAP_SHARED, old_fsa_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                                     stat_buf.stx_size,
# else
                                     stat_buf.st_size,
# endif
                                     (PROT_READ | PROT_WRITE),
                                     MAP_SHARED, old_fsa_stat, 0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fsa_stat, strerror(errno));
                    *old_fsa_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FSA file %s is empty.", old_fsa_stat);
                 *old_fsa_size = -1;
                 return(NULL);
              }
           }

           old_features = *(ptr + SIZEOF_INT + 1);
           ptr += AFD_WORD_OFFSET_0;
           old_fsa = (struct filetransfer_status_0 *)ptr;

           new_size = old_no_of_hosts * sizeof(struct filetransfer_status_2);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_hosts, new_size, strerror(errno));
              ptr = (char *)old_fsa;
              ptr -= AFD_WORD_OFFSET_0;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
              if (munmap(ptr, stat_buf.stx_size) == -1)
# else
              if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fsa_stat, strerror(errno));
              }
              *old_fsa_size = -1;
              return(NULL);
           }
           (void)memset(ptr, 0, new_size);
           new_fsa = (struct filetransfer_status_2 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_hosts; i++)
           {
              (void)strcpy(new_fsa[i].host_alias, old_fsa[i].host_alias);
              (void)strcpy(new_fsa[i].real_hostname[0], old_fsa[i].real_hostname[0]);
              (void)strcpy(new_fsa[i].real_hostname[1], old_fsa[i].real_hostname[1]);
              (void)strcpy(new_fsa[i].host_dsp_name, old_fsa[i].host_dsp_name);
              (void)strcpy(new_fsa[i].proxy_name, old_fsa[i].proxy_name);
              (void)strcpy(new_fsa[i].host_toggle_str, old_fsa[i].host_toggle_str);
              new_fsa[i].toggle_pos             = old_fsa[i].toggle_pos;
              new_fsa[i].original_toggle_pos    = old_fsa[i].original_toggle_pos;
              new_fsa[i].auto_toggle            = old_fsa[i].auto_toggle;
              new_fsa[i].file_size_offset       = old_fsa[i].file_size_offset;
              new_fsa[i].successful_retries     = old_fsa[i].successful_retries;
              new_fsa[i].max_successful_retries = old_fsa[i].max_successful_retries;
              new_fsa[i].special_flag           = old_fsa[i].special_flag;
              new_fsa[i].protocol               = 0;
              if (old_fsa[i].protocol & FTP_FLAG)
              {
                 new_fsa[i].protocol |= FTP_FLAG;
              }
              if (old_fsa[i].protocol & LOC_FLAG)
              {
                 new_fsa[i].protocol |= LOC_FLAG;
              }
              if (old_fsa[i].protocol & SMTP_FLAG)
              {
                 new_fsa[i].protocol |= SMTP_FLAG;
              }
#ifdef _WITH_MAP_SUPPORT
              if (old_fsa[i].protocol & MAP_FLAG)
              {
                 new_fsa[i].protocol |= MAP_FLAG;
              }
#endif
#ifdef _WITH_SCP_SUPPORT
              if (old_fsa[i].protocol & SCP_FLAG)
              {
                 new_fsa[i].protocol |= SCP_FLAG;
              }
#endif
#ifdef _WITH_WMO_SUPPORT
              if (old_fsa[i].protocol & WMO_FLAG)
              {
                 new_fsa[i].protocol |= WMO_FLAG;
              }
#endif
              if (old_fsa[i].protocol & GET_FTP_FLAG_0)
              {
                 new_fsa[i].protocol |= GET_FTP_FLAG_2;
              }
              if (old_fsa[i].protocol & SEND_FLAG_0)
              {
                 new_fsa[i].protocol |= SEND_FLAG_2;
              }
              if (old_fsa[i].protocol & RETRIEVE_FLAG_0)
              {
                 new_fsa[i].protocol |= RETRIEVE_FLAG_2;
              }
              new_fsa[i].protocol_options = 0;
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
              if (old_fsa[i].protocol & STAT_KEEPALIVE_0)
              {
                 new_fsa[i].protocol_options |= STAT_KEEPALIVE_2;
              }
#endif
              if (old_fsa[i].protocol & SET_IDLE_TIME_0)
              {
                 new_fsa[i].protocol_options |= SET_IDLE_TIME_2;
              }
              if (old_fsa[i].protocol & FTP_PASSIVE_MODE_0)
              {
                 new_fsa[i].protocol_options |= FTP_PASSIVE_MODE_2;
              }
              new_fsa[i].socksnd_bufsize        = 0U;
              new_fsa[i].sockrcv_bufsize        = 0U;
              new_fsa[i].keep_connected         = 0U;
#ifdef WITH_DUP_CHECK
              new_fsa[i].dup_check_flag         = 0U;
#endif
              new_fsa[i].host_id                = get_str_checksum(new_fsa[i].host_alias);
              if (old_fsa[i].debug == NO)
              {
                 new_fsa[i].debug               = NORMAL_MODE;
              }
              else
              {
                 new_fsa[i].debug               = DEBUG_MODE;
              }
              new_fsa[i].host_toggle            = old_fsa[i].host_toggle;
              new_fsa[i].host_status            = old_fsa[i].host_status;
              new_fsa[i].error_counter          = old_fsa[i].error_counter;
              new_fsa[i].total_errors           = old_fsa[i].total_errors;
              new_fsa[i].max_errors             = old_fsa[i].max_errors;
              for (j = 0; j < ERROR_HISTORY_LENGTH; j++)
              {
                 new_fsa[i].error_history[j] = 0;
              }
              new_fsa[i].retry_interval         = old_fsa[i].retry_interval;
              new_fsa[i].block_size             = old_fsa[i].block_size;
              new_fsa[i].ttl                    = 0;
#ifdef WITH_DUP_CHECK
              new_fsa[i].dup_check_timeout      = 0L;
#endif
              new_fsa[i].last_retry_time        = old_fsa[i].last_retry_time;
              new_fsa[i].last_connection        = old_fsa[i].last_connection;
              new_fsa[i].first_error_time       = 0L;
              new_fsa[i].total_file_counter     = old_fsa[i].total_file_counter;
              new_fsa[i].total_file_size        = (off_t)(old_fsa[i].total_file_size);
              new_fsa[i].jobs_queued            = old_fsa[i].jobs_queued;
              new_fsa[i].file_counter_done      = old_fsa[i].file_counter_done;
              new_fsa[i].bytes_send             = (u_off_t)(old_fsa[i].bytes_send);
              new_fsa[i].connections            = old_fsa[i].connections;
              new_fsa[i].mc_nack_counter        = 0U;
              new_fsa[i].active_transfers       = old_fsa[i].active_transfers;
              new_fsa[i].allowed_transfers      = old_fsa[i].allowed_transfers;
              new_fsa[i].transfer_rate_limit    = 0;
              new_fsa[i].trl_per_process        = 0;
              new_fsa[i].mc_ct_rate_limit       = 0;
              new_fsa[i].mc_ctrl_per_process    = 0;
              new_fsa[i].transfer_timeout       = old_fsa[i].transfer_timeout;
              for (j = 0; j < MAX_NO_PARALLEL_JOBS_0; j++)
              {
                 new_fsa[i].job_status[j].proc_id = old_fsa[i].job_status[j].proc_id;
#ifdef _WITH_BURST_2
                 (void)strcpy(new_fsa[i].job_status[j].unique_name, old_fsa[i].job_status[j].unique_name);
                 new_fsa[i].job_status[j].job_id = old_fsa[i].job_status[j].job_id;
#endif
                 new_fsa[i].job_status[j].connect_status = old_fsa[i].job_status[j].connect_status;
                 new_fsa[i].job_status[j].no_of_files = old_fsa[i].job_status[j].no_of_files;
                 new_fsa[i].job_status[j].no_of_files_done = old_fsa[i].job_status[j].no_of_files_done;
                 new_fsa[i].job_status[j].file_size = (off_t)old_fsa[i].job_status[j].file_size;
                 new_fsa[i].job_status[j].file_size_done = (u_off_t)old_fsa[i].job_status[j].file_size_done;
                 new_fsa[i].job_status[j].bytes_send = (u_off_t)old_fsa[i].job_status[j].bytes_send;
                 (void)strcpy(new_fsa[i].job_status[j].file_name_in_use, old_fsa[i].job_status[j].file_name_in_use);
                 new_fsa[i].job_status[j].file_size_in_use = (off_t)old_fsa[i].job_status[j].file_size_in_use;
                 new_fsa[i].job_status[j].file_size_in_use_done = (off_t)old_fsa[i].job_status[j].file_size_in_use_done;
              }
           }

           ptr = (char *)old_fsa;
           ptr -= AFD_WORD_OFFSET_0;

           /*
            * Resize the old FSA to the size of new one and then copy
            * the new structure into it. Then update the FSA version
            * number.
            */
           if ((ptr = mmap_resize(old_fsa_fd, ptr,
                                  new_size + AFD_WORD_OFFSET_2)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fsa_stat, strerror(errno));
              free((void *)new_fsa);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_2;
           (void)memcpy(ptr, new_fsa, new_size);
           free((void *)new_fsa);
           ptr -= AFD_WORD_OFFSET_2;
           *(ptr + SIZEOF_INT + 1) = old_features;
           *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           *(int *)(ptr + SIZEOF_INT + 4) = pagesize;
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
           *old_fsa_size = new_size + AFD_WORD_OFFSET_2;

           system_log(INFO_SIGN, NULL, 0,
                      "Converted FSA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 1) && (new_version == 2))
        {
           struct filetransfer_status_1 *old_fsa;
           struct filetransfer_status_2 *new_fsa;

           /* Get the size of the old FSA file. */
#ifdef HAVE_STATX
           if (statx(old_fsa_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                     STATX_SIZE, &stat_buf) == -1)
#else
           if (fstat(old_fsa_fd, &stat_buf) == -1)
#endif
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                         "Failed to statx() %s : %s",
#else
                         "Failed to fstat() %s : %s",
#endif
                         old_fsa_stat, strerror(errno));
              *old_fsa_size = -1;
              return(NULL);
           }
           else
           {
#ifdef HAVE_STATX
              if (stat_buf.stx_size > 0)
#else
              if (stat_buf.st_size > 0)
#endif
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                                 stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                                 stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                                 MAP_SHARED, old_fsa_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                                     stat_buf.stx_size,
# else
                                     stat_buf.st_size,
# endif
                                     (PROT_READ | PROT_WRITE),
                                     MAP_SHARED, old_fsa_stat,
                                     0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fsa_stat, strerror(errno));
                    *old_fsa_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FSA file %s is empty.", old_fsa_stat);
                 *old_fsa_size = -1;
                 return(NULL);
              }
           }

           old_features = *(ptr + SIZEOF_INT + 1);
           ptr += AFD_WORD_OFFSET_1;
           old_fsa = (struct filetransfer_status_1 *)ptr;

           new_size = old_no_of_hosts * sizeof(struct filetransfer_status_2);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_hosts, new_size, strerror(errno));
              ptr = (char *)old_fsa;
              ptr -= AFD_WORD_OFFSET_1;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
              if (munmap(ptr, stat_buf.stx_size) == -1)
# else
              if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fsa_stat, strerror(errno));
              }
              *old_fsa_size = -1;
              return(NULL);
           }
           (void)memset(ptr, 0, new_size);
           new_fsa = (struct filetransfer_status_2 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_hosts; i++)
           {
              (void)my_strncpy(new_fsa[i].host_alias, old_fsa[i].host_alias, MAX_HOSTNAME_LENGTH_2 + 1);
              (void)strcpy(new_fsa[i].real_hostname[0], old_fsa[i].real_hostname[0]);
              (void)strcpy(new_fsa[i].real_hostname[1], old_fsa[i].real_hostname[1]);
              (void)my_strncpy(new_fsa[i].host_dsp_name, old_fsa[i].host_dsp_name, MAX_HOSTNAME_LENGTH_2 + 1);
              (void)strcpy(new_fsa[i].proxy_name, old_fsa[i].proxy_name);
              (void)strcpy(new_fsa[i].host_toggle_str, old_fsa[i].host_toggle_str);
              new_fsa[i].toggle_pos             = old_fsa[i].toggle_pos;
              new_fsa[i].original_toggle_pos    = old_fsa[i].original_toggle_pos;
              new_fsa[i].auto_toggle            = old_fsa[i].auto_toggle;
              new_fsa[i].file_size_offset       = old_fsa[i].file_size_offset;
              new_fsa[i].successful_retries     = old_fsa[i].successful_retries;
              new_fsa[i].max_successful_retries = old_fsa[i].max_successful_retries;
              new_fsa[i].special_flag           = old_fsa[i].special_flag;
              new_fsa[i].protocol               = old_fsa[i].protocol;
              new_fsa[i].protocol_options       = old_fsa[i].protocol_options;
              new_fsa[i].socksnd_bufsize        = 0U;
              new_fsa[i].sockrcv_bufsize        = 0U;
              new_fsa[i].keep_connected         = 0U;
#ifdef WITH_DUP_CHECK
              new_fsa[i].dup_check_flag         = 0U;
#endif
              new_fsa[i].host_id                = get_str_checksum(new_fsa[i].host_alias);
              new_fsa[i].debug                  = old_fsa[i].debug;
              new_fsa[i].host_toggle            = old_fsa[i].host_toggle;
              new_fsa[i].host_status            = old_fsa[i].host_status;
              new_fsa[i].error_counter          = old_fsa[i].error_counter;
              new_fsa[i].total_errors           = old_fsa[i].total_errors;
              new_fsa[i].max_errors             = old_fsa[i].max_errors;
              (void)memcpy(new_fsa[i].error_history, old_fsa[i].error_history, ERROR_HISTORY_LENGTH);
              new_fsa[i].retry_interval         = old_fsa[i].retry_interval;
              new_fsa[i].block_size             = old_fsa[i].block_size;
              new_fsa[i].ttl                    = old_fsa[i].ttl;
#ifdef WITH_DUP_CHECK
              new_fsa[i].dup_check_timeout      = 0L;
#endif
              new_fsa[i].last_retry_time        = old_fsa[i].last_retry_time;
              new_fsa[i].last_connection        = old_fsa[i].last_connection;
              new_fsa[i].first_error_time       = old_fsa[i].first_error_time;
              new_fsa[i].total_file_counter     = old_fsa[i].total_file_counter;
              new_fsa[i].total_file_size        = old_fsa[i].total_file_size;
              new_fsa[i].jobs_queued            = old_fsa[i].jobs_queued;
              new_fsa[i].file_counter_done      = old_fsa[i].file_counter_done;
              new_fsa[i].bytes_send             = old_fsa[i].bytes_send;
              new_fsa[i].connections            = old_fsa[i].connections;
              new_fsa[i].mc_nack_counter        = old_fsa[i].mc_nack_counter;
              new_fsa[i].active_transfers       = old_fsa[i].active_transfers;
              new_fsa[i].allowed_transfers      = old_fsa[i].allowed_transfers;
              new_fsa[i].transfer_rate_limit    = old_fsa[i].transfer_rate_limit;
              new_fsa[i].trl_per_process        = old_fsa[i].trl_per_process;
              new_fsa[i].mc_ct_rate_limit       = old_fsa[i].mc_ct_rate_limit;
              new_fsa[i].mc_ctrl_per_process    = old_fsa[i].mc_ctrl_per_process;
              new_fsa[i].transfer_timeout       = old_fsa[i].transfer_timeout;
              for (j = 0; j < MAX_NO_PARALLEL_JOBS_1; j++)
              {
                 new_fsa[i].job_status[j].proc_id = old_fsa[i].job_status[j].proc_id;
#ifdef _WITH_BURST_2
                 (void)memcpy(new_fsa[i].job_status[j].unique_name, old_fsa[i].job_status[j].unique_name, MAX_MSG_NAME_LENGTH_1);
                 new_fsa[i].job_status[j].job_id = old_fsa[i].job_status[j].job_id;
#endif
                 new_fsa[i].job_status[j].connect_status = old_fsa[i].job_status[j].connect_status;
                 new_fsa[i].job_status[j].no_of_files = old_fsa[i].job_status[j].no_of_files;
                 new_fsa[i].job_status[j].no_of_files_done = old_fsa[i].job_status[j].no_of_files_done;
                 new_fsa[i].job_status[j].file_size = old_fsa[i].job_status[j].file_size;
                 new_fsa[i].job_status[j].file_size_done = old_fsa[i].job_status[j].file_size_done;
                 new_fsa[i].job_status[j].bytes_send = old_fsa[i].job_status[j].bytes_send;
                 (void)strcpy(new_fsa[i].job_status[j].file_name_in_use, old_fsa[i].job_status[j].file_name_in_use);
                 new_fsa[i].job_status[j].file_size_in_use = old_fsa[i].job_status[j].file_size_in_use;
                 new_fsa[i].job_status[j].file_size_in_use_done = old_fsa[i].job_status[j].file_size_in_use_done;
              }
           }

           ptr = (char *)old_fsa;
           ptr -= AFD_WORD_OFFSET_1;

           /*
            * Resize the old FSA to the size of new one and then copy
            * the new structure into it. Then update the FSA version
            * number.
            */
           if ((ptr = mmap_resize(old_fsa_fd, ptr,
                                  new_size + AFD_WORD_OFFSET_2)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fsa_stat, strerror(errno));
              free((void *)new_fsa);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_2;
           (void)memcpy(ptr, new_fsa, new_size);
           free((void *)new_fsa);
           ptr -= AFD_WORD_OFFSET_2;
           *(ptr + SIZEOF_INT + 1) = old_features;
           *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           *(int *)(ptr + SIZEOF_INT + 4) = pagesize;
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
           *old_fsa_size = new_size + AFD_WORD_OFFSET_2;

           system_log(INFO_SIGN, NULL, 0,
                      "Converted FSA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 0) && (new_version == 3))
        {
           struct filetransfer_status_0 *old_fsa;
           struct filetransfer_status_3 *new_fsa;

           /* Get the size of the old FSA file. */
#ifdef HAVE_STATX
           if (statx(old_fsa_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                     STATX_SIZE, &stat_buf) == -1)
#else
           if (fstat(old_fsa_fd, &stat_buf) == -1)
#endif
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                         "Failed to statx() %s : %s",
#else
                         "Failed to fstat() %s : %s",
#endif
                         old_fsa_stat, strerror(errno));
              *old_fsa_size = -1;
              return(NULL);
           }
           else
           {
#ifdef HAVE_STATX
              if (stat_buf.stx_size > 0)
#else
              if (stat_buf.st_size > 0)
#endif
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                                 stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                                 stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                                 MAP_SHARED, old_fsa_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                                     stat_buf.stx_size,
# else
                                     stat_buf.st_size,
# endif
                                     (PROT_READ | PROT_WRITE),
                                     MAP_SHARED, old_fsa_stat,
                                     0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fsa_stat, strerror(errno));
                    *old_fsa_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FSA file %s is empty.", old_fsa_stat);
                 *old_fsa_size = -1;
                 return(NULL);
              }
           }

           old_features = *(ptr + SIZEOF_INT + 1);
           ptr += AFD_WORD_OFFSET_0;
           old_fsa = (struct filetransfer_status_0 *)ptr;

           new_size = old_no_of_hosts * sizeof(struct filetransfer_status_3);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_hosts, new_size, strerror(errno));
              ptr = (char *)old_fsa;
              ptr -= AFD_WORD_OFFSET_0;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
              if (munmap(ptr, stat_buf.stx_size) == -1)
# else
              if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fsa_stat, strerror(errno));
              }
              *old_fsa_size = -1;
              return(NULL);
           }
           (void)memset(ptr, 0, new_size);
           new_fsa = (struct filetransfer_status_3 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_hosts; i++)
           {
              (void)strcpy(new_fsa[i].host_alias, old_fsa[i].host_alias);
              (void)strcpy(new_fsa[i].real_hostname[0], old_fsa[i].real_hostname[0]);
              (void)strcpy(new_fsa[i].real_hostname[1], old_fsa[i].real_hostname[1]);
              (void)strcpy(new_fsa[i].host_dsp_name, old_fsa[i].host_dsp_name);
              new_fsa[i].host_dsp_name[MAX_HOSTNAME_LENGTH_3 + 1] = '\0';
              (void)strcpy(new_fsa[i].proxy_name, old_fsa[i].proxy_name);
              (void)strcpy(new_fsa[i].host_toggle_str, old_fsa[i].host_toggle_str);
              new_fsa[i].toggle_pos             = old_fsa[i].toggle_pos;
              new_fsa[i].original_toggle_pos    = old_fsa[i].original_toggle_pos;
              new_fsa[i].auto_toggle            = old_fsa[i].auto_toggle;
              new_fsa[i].file_size_offset       = old_fsa[i].file_size_offset;
              new_fsa[i].successful_retries     = old_fsa[i].successful_retries;
              new_fsa[i].max_successful_retries = old_fsa[i].max_successful_retries;
              new_fsa[i].special_flag           = old_fsa[i].special_flag;
              new_fsa[i].protocol               = 0;
              if (old_fsa[i].protocol & FTP_FLAG)
              {
                 new_fsa[i].protocol |= FTP_FLAG;
              }
              if (old_fsa[i].protocol & LOC_FLAG)
              {
                 new_fsa[i].protocol |= LOC_FLAG;
              }
              if (old_fsa[i].protocol & SMTP_FLAG)
              {
                 new_fsa[i].protocol |= SMTP_FLAG;
              }
#ifdef _WITH_MAP_SUPPORT
              if (old_fsa[i].protocol & MAP_FLAG)
              {
                 new_fsa[i].protocol |= MAP_FLAG;
              }
#endif
#ifdef _WITH_SCP_SUPPORT
              if (old_fsa[i].protocol & SCP_FLAG)
              {
                 new_fsa[i].protocol |= SCP_FLAG;
              }
#endif
#ifdef _WITH_WMO_SUPPORT
              if (old_fsa[i].protocol & WMO_FLAG)
              {
                 new_fsa[i].protocol |= WMO_FLAG;
              }
#endif
              if (old_fsa[i].protocol & GET_FTP_FLAG_0)
              {
                 new_fsa[i].protocol |= GET_FTP_FLAG_3;
              }
              if (old_fsa[i].protocol & SEND_FLAG_0)
              {
                 new_fsa[i].protocol |= SEND_FLAG_3;
              }
              if (old_fsa[i].protocol & RETRIEVE_FLAG_0)
              {
                 new_fsa[i].protocol |= RETRIEVE_FLAG_3;
              }
              new_fsa[i].protocol_options = 0;
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
              if (old_fsa[i].protocol & STAT_KEEPALIVE_0)
              {
                 new_fsa[i].protocol_options |= STAT_KEEPALIVE_3;
              }
#endif
              if (old_fsa[i].protocol & SET_IDLE_TIME_0)
              {
                 new_fsa[i].protocol_options |= SET_IDLE_TIME_3;
              }
              if (old_fsa[i].protocol & FTP_PASSIVE_MODE_0)
              {
                 new_fsa[i].protocol_options |= FTP_PASSIVE_MODE_3;
              }
              new_fsa[i].socksnd_bufsize        = 0U;
              new_fsa[i].sockrcv_bufsize        = 0U;
              new_fsa[i].keep_connected         = 0U;
#ifdef WITH_DUP_CHECK
              new_fsa[i].dup_check_flag         = 0U;
#endif
              new_fsa[i].host_id                = get_str_checksum(new_fsa[i].host_alias);
              if (old_fsa[i].debug == NO)
              {
                 new_fsa[i].debug               = NORMAL_MODE;
              }
              else
              {
                 new_fsa[i].debug               = DEBUG_MODE;
              }
              new_fsa[i].host_toggle            = old_fsa[i].host_toggle;
              new_fsa[i].host_status            = old_fsa[i].host_status;
              new_fsa[i].error_counter          = old_fsa[i].error_counter;
              new_fsa[i].total_errors           = old_fsa[i].total_errors;
              new_fsa[i].max_errors             = old_fsa[i].max_errors;
              for (j = 0; j < ERROR_HISTORY_LENGTH; j++)
              {
                 new_fsa[i].error_history[j] = 0;
              }
              new_fsa[i].retry_interval         = old_fsa[i].retry_interval;
              new_fsa[i].block_size             = old_fsa[i].block_size;
              new_fsa[i].ttl                    = 0;
#ifdef WITH_DUP_CHECK
              new_fsa[i].dup_check_timeout      = 0L;
#endif
              new_fsa[i].last_retry_time        = old_fsa[i].last_retry_time;
              new_fsa[i].last_connection        = old_fsa[i].last_connection;
              new_fsa[i].first_error_time       = 0L;
              new_fsa[i].start_event_handle     = 0L;
              new_fsa[i].end_event_handle       = 0L;
              new_fsa[i].warn_time              = 0L;
              new_fsa[i].total_file_counter     = old_fsa[i].total_file_counter;
              new_fsa[i].total_file_size        = (off_t)(old_fsa[i].total_file_size);
              new_fsa[i].jobs_queued            = old_fsa[i].jobs_queued;
              new_fsa[i].file_counter_done      = old_fsa[i].file_counter_done;
              new_fsa[i].bytes_send             = (u_off_t)(old_fsa[i].bytes_send);
              new_fsa[i].connections            = old_fsa[i].connections;
              new_fsa[i].mc_nack_counter        = 0U;
              new_fsa[i].active_transfers       = old_fsa[i].active_transfers;
              new_fsa[i].allowed_transfers      = old_fsa[i].allowed_transfers;
              new_fsa[i].transfer_rate_limit    = 0;
              new_fsa[i].trl_per_process        = 0;
              new_fsa[i].mc_ct_rate_limit       = 0;
              new_fsa[i].mc_ctrl_per_process    = 0;
              new_fsa[i].transfer_timeout       = old_fsa[i].transfer_timeout;
              for (j = 0; j < MAX_NO_PARALLEL_JOBS_0; j++)
              {
                 new_fsa[i].job_status[j].proc_id = old_fsa[i].job_status[j].proc_id;
#ifdef _WITH_BURST_2
                 (void)strcpy(new_fsa[i].job_status[j].unique_name, old_fsa[i].job_status[j].unique_name);
                 new_fsa[i].job_status[j].job_id = old_fsa[i].job_status[j].job_id;
#endif
                 new_fsa[i].job_status[j].connect_status = old_fsa[i].job_status[j].connect_status;
                 new_fsa[i].job_status[j].no_of_files = old_fsa[i].job_status[j].no_of_files;
                 new_fsa[i].job_status[j].no_of_files_done = old_fsa[i].job_status[j].no_of_files_done;
                 new_fsa[i].job_status[j].file_size = (off_t)old_fsa[i].job_status[j].file_size;
                 new_fsa[i].job_status[j].file_size_done = (u_off_t)old_fsa[i].job_status[j].file_size_done;
                 new_fsa[i].job_status[j].bytes_send = (u_off_t)old_fsa[i].job_status[j].bytes_send;
                 (void)strcpy(new_fsa[i].job_status[j].file_name_in_use, old_fsa[i].job_status[j].file_name_in_use);
                 new_fsa[i].job_status[j].file_size_in_use = (off_t)old_fsa[i].job_status[j].file_size_in_use;
                 new_fsa[i].job_status[j].file_size_in_use_done = (off_t)old_fsa[i].job_status[j].file_size_in_use_done;
              }
           }

           ptr = (char *)old_fsa;
           ptr -= AFD_WORD_OFFSET_0;

           /*
            * Resize the old FSA to the size of new one and then copy
            * the new structure into it. Then update the FSA version
            * number.
            */
           if ((ptr = mmap_resize(old_fsa_fd, ptr,
                                  new_size + AFD_WORD_OFFSET_3)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fsa_stat, strerror(errno));
              free((void *)new_fsa);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_3;
           (void)memcpy(ptr, new_fsa, new_size);
           free((void *)new_fsa);
           ptr -= AFD_WORD_OFFSET_3;
           *(ptr + SIZEOF_INT + 1) = old_features;
           *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           *(int *)(ptr + SIZEOF_INT + 4) = pagesize;
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
           *old_fsa_size = new_size + AFD_WORD_OFFSET_3;

           system_log(INFO_SIGN, NULL, 0,
                      "Converted FSA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 1) && (new_version == 3))
        {
           struct filetransfer_status_1 *old_fsa;
           struct filetransfer_status_3 *new_fsa;

           /* Get the size of the old FSA file. */
#ifdef HAVE_STATX
           if (statx(old_fsa_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                     STATX_SIZE, &stat_buf) == -1)
#else
           if (fstat(old_fsa_fd, &stat_buf) == -1)
#endif
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                         "Failed to statx() %s : %s",
#else
                         "Failed to fstat() %s : %s",
#endif
                         old_fsa_stat, strerror(errno));
              *old_fsa_size = -1;
              return(NULL);
           }
           else
           {
#ifdef HAVE_STATX
              if (stat_buf.stx_size > 0)
#else
              if (stat_buf.st_size > 0)
#endif
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                                 stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                                 stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                                 MAP_SHARED, old_fsa_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                                     stat_buf.stx_size,
# else
                                     stat_buf.st_size,
# endif
                                     (PROT_READ | PROT_WRITE),
                                     MAP_SHARED, old_fsa_stat,
                                     0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fsa_stat, strerror(errno));
                    *old_fsa_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FSA file %s is empty.", old_fsa_stat);
                 *old_fsa_size = -1;
                 return(NULL);
              }
           }

           old_features = *(ptr + SIZEOF_INT + 1);
           ptr += AFD_WORD_OFFSET_1;
           old_fsa = (struct filetransfer_status_1 *)ptr;

           new_size = old_no_of_hosts * sizeof(struct filetransfer_status_3);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_hosts, new_size, strerror(errno));
              ptr = (char *)old_fsa;
              ptr -= AFD_WORD_OFFSET_1;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
              if (munmap(ptr, stat_buf.stx_size) == -1)
# else
              if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fsa_stat, strerror(errno));
              }
              *old_fsa_size = -1;
              return(NULL);
           }
           (void)memset(ptr, 0, new_size);
           new_fsa = (struct filetransfer_status_3 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_hosts; i++)
           {
              (void)my_strncpy(new_fsa[i].host_alias, old_fsa[i].host_alias, MAX_HOSTNAME_LENGTH_3 + 1);
              (void)strcpy(new_fsa[i].real_hostname[0], old_fsa[i].real_hostname[0]);
              (void)strcpy(new_fsa[i].real_hostname[1], old_fsa[i].real_hostname[1]);
              (void)my_strncpy(new_fsa[i].host_dsp_name, old_fsa[i].host_dsp_name, MAX_HOSTNAME_LENGTH_3 + 1);
              new_fsa[i].host_dsp_name[MAX_HOSTNAME_LENGTH_3 + 1] = '\0';
              (void)strcpy(new_fsa[i].proxy_name, old_fsa[i].proxy_name);
              (void)strcpy(new_fsa[i].host_toggle_str, old_fsa[i].host_toggle_str);
              new_fsa[i].toggle_pos             = old_fsa[i].toggle_pos;
              new_fsa[i].original_toggle_pos    = old_fsa[i].original_toggle_pos;
              new_fsa[i].auto_toggle            = old_fsa[i].auto_toggle;
              new_fsa[i].file_size_offset       = old_fsa[i].file_size_offset;
              new_fsa[i].successful_retries     = old_fsa[i].successful_retries;
              new_fsa[i].max_successful_retries = old_fsa[i].max_successful_retries;
              new_fsa[i].special_flag           = old_fsa[i].special_flag;
              new_fsa[i].protocol               = old_fsa[i].protocol;
              new_fsa[i].protocol_options       = old_fsa[i].protocol_options;
              new_fsa[i].socksnd_bufsize        = 0U;
              new_fsa[i].sockrcv_bufsize        = 0U;
              new_fsa[i].keep_connected         = 0U;
#ifdef WITH_DUP_CHECK
              new_fsa[i].dup_check_flag         = 0U;
#endif
              new_fsa[i].host_id                = get_str_checksum(new_fsa[i].host_alias);
              new_fsa[i].debug                  = old_fsa[i].debug;
              new_fsa[i].host_toggle            = old_fsa[i].host_toggle;
              new_fsa[i].host_status            = old_fsa[i].host_status;
              new_fsa[i].error_counter          = old_fsa[i].error_counter;
              new_fsa[i].total_errors           = old_fsa[i].total_errors;
              new_fsa[i].max_errors             = old_fsa[i].max_errors;
              (void)memcpy(new_fsa[i].error_history, old_fsa[i].error_history, ERROR_HISTORY_LENGTH);
              new_fsa[i].retry_interval         = old_fsa[i].retry_interval;
              new_fsa[i].block_size             = old_fsa[i].block_size;
              new_fsa[i].ttl                    = old_fsa[i].ttl;
#ifdef WITH_DUP_CHECK
              new_fsa[i].dup_check_timeout      = 0L;
#endif
              new_fsa[i].last_retry_time        = old_fsa[i].last_retry_time;
              new_fsa[i].last_connection        = old_fsa[i].last_connection;
              new_fsa[i].first_error_time       = old_fsa[i].first_error_time;
              new_fsa[i].start_event_handle     = 0L;
              new_fsa[i].end_event_handle       = 0L;
              new_fsa[i].warn_time              = 0L;
              new_fsa[i].total_file_counter     = old_fsa[i].total_file_counter;
              new_fsa[i].total_file_size        = old_fsa[i].total_file_size;
              new_fsa[i].jobs_queued            = old_fsa[i].jobs_queued;
              new_fsa[i].file_counter_done      = old_fsa[i].file_counter_done;
              new_fsa[i].bytes_send             = old_fsa[i].bytes_send;
              new_fsa[i].connections            = old_fsa[i].connections;
              new_fsa[i].mc_nack_counter        = old_fsa[i].mc_nack_counter;
              new_fsa[i].active_transfers       = old_fsa[i].active_transfers;
              new_fsa[i].allowed_transfers      = old_fsa[i].allowed_transfers;
              new_fsa[i].transfer_rate_limit    = old_fsa[i].transfer_rate_limit;
              new_fsa[i].trl_per_process        = old_fsa[i].trl_per_process;
              new_fsa[i].mc_ct_rate_limit       = old_fsa[i].mc_ct_rate_limit;
              new_fsa[i].mc_ctrl_per_process    = old_fsa[i].mc_ctrl_per_process;
              new_fsa[i].transfer_timeout       = old_fsa[i].transfer_timeout;
              for (j = 0; j < MAX_NO_PARALLEL_JOBS_1; j++)
              {
                 new_fsa[i].job_status[j].proc_id = old_fsa[i].job_status[j].proc_id;
#ifdef _WITH_BURST_2
                 (void)memcpy(new_fsa[i].job_status[j].unique_name, old_fsa[i].job_status[j].unique_name, MAX_MSG_NAME_LENGTH_1);
                 new_fsa[i].job_status[j].job_id = old_fsa[i].job_status[j].job_id;
#endif
                 new_fsa[i].job_status[j].connect_status = old_fsa[i].job_status[j].connect_status;
                 new_fsa[i].job_status[j].no_of_files = old_fsa[i].job_status[j].no_of_files;
                 new_fsa[i].job_status[j].no_of_files_done = old_fsa[i].job_status[j].no_of_files_done;
                 new_fsa[i].job_status[j].file_size = old_fsa[i].job_status[j].file_size;
                 new_fsa[i].job_status[j].file_size_done = old_fsa[i].job_status[j].file_size_done;
                 new_fsa[i].job_status[j].bytes_send = old_fsa[i].job_status[j].bytes_send;
                 (void)strcpy(new_fsa[i].job_status[j].file_name_in_use, old_fsa[i].job_status[j].file_name_in_use);
                 new_fsa[i].job_status[j].file_size_in_use = old_fsa[i].job_status[j].file_size_in_use;
                 new_fsa[i].job_status[j].file_size_in_use_done = old_fsa[i].job_status[j].file_size_in_use_done;
              }
           }

           ptr = (char *)old_fsa;
           ptr -= AFD_WORD_OFFSET_1;

           /*
            * Resize the old FSA to the size of new one and then copy
            * the new structure into it. Then update the FSA version
            * number.
            */
           if ((ptr = mmap_resize(old_fsa_fd, ptr,
                                  new_size + AFD_WORD_OFFSET_3)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fsa_stat, strerror(errno));
              free((void *)new_fsa);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_3;
           (void)memcpy(ptr, new_fsa, new_size);
           free((void *)new_fsa);
           ptr -= AFD_WORD_OFFSET_3;
           *(ptr + SIZEOF_INT + 1) = old_features;
           *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           *(int *)(ptr + SIZEOF_INT + 4) = pagesize;
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
           *old_fsa_size = new_size + AFD_WORD_OFFSET_3;

           system_log(INFO_SIGN, NULL, 0,
                      "Converted FSA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 2) && (new_version == 3))
        {
           struct filetransfer_status_2 *old_fsa;
           struct filetransfer_status_3 *new_fsa;

           /* Get the size of the old FSA file. */
#ifdef HAVE_STATX
           if (statx(old_fsa_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                     STATX_SIZE, &stat_buf) == -1)
#else
           if (fstat(old_fsa_fd, &stat_buf) == -1)
#endif
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                         "Failed to statx() %s : %s",
#else
                         "Failed to fstat() %s : %s",
#endif
                         old_fsa_stat, strerror(errno));
              *old_fsa_size = -1;
              return(NULL);
           }
           else
           {
#ifdef HAVE_STATX
              if (stat_buf.stx_size > 0)
#else
              if (stat_buf.st_size > 0)
#endif
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                                 stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                                 stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                                 MAP_SHARED, old_fsa_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                                     stat_buf.stx_size,
# else
                                     stat_buf.st_size,
# endif
                                     (PROT_READ | PROT_WRITE),
                                     MAP_SHARED, old_fsa_stat,
                                     0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fsa_stat, strerror(errno));
                    *old_fsa_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FSA file %s is empty.", old_fsa_stat);
                 *old_fsa_size = -1;
                 return(NULL);
              }
           }

           old_features = *(ptr + SIZEOF_INT + 1);
           ptr += AFD_WORD_OFFSET_2;
           old_fsa = (struct filetransfer_status_2 *)ptr;

           new_size = old_no_of_hosts * sizeof(struct filetransfer_status_3);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_hosts, new_size, strerror(errno));
              ptr = (char *)old_fsa;
              ptr -= AFD_WORD_OFFSET_2;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
              if (munmap(ptr, stat_buf.stx_size) == -1)
# else
              if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fsa_stat, strerror(errno));
              }
              *old_fsa_size = -1;
              return(NULL);
           }
           (void)memset(ptr, 0, new_size);
           new_fsa = (struct filetransfer_status_3 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_hosts; i++)
           {
              (void)my_strncpy(new_fsa[i].host_alias, old_fsa[i].host_alias, MAX_HOSTNAME_LENGTH_3 + 1);
              (void)strcpy(new_fsa[i].real_hostname[0], old_fsa[i].real_hostname[0]);
              (void)strcpy(new_fsa[i].real_hostname[1], old_fsa[i].real_hostname[1]);
              (void)my_strncpy(new_fsa[i].host_dsp_name, old_fsa[i].host_dsp_name, MAX_HOSTNAME_LENGTH_3 + 1);
              new_fsa[i].host_dsp_name[MAX_HOSTNAME_LENGTH_3 + 1] = '\0';
              (void)strcpy(new_fsa[i].proxy_name, old_fsa[i].proxy_name);
              (void)strcpy(new_fsa[i].host_toggle_str, old_fsa[i].host_toggle_str);
              new_fsa[i].toggle_pos             = old_fsa[i].toggle_pos;
              new_fsa[i].original_toggle_pos    = old_fsa[i].original_toggle_pos;
              new_fsa[i].auto_toggle            = old_fsa[i].auto_toggle;
              new_fsa[i].file_size_offset       = old_fsa[i].file_size_offset;
              new_fsa[i].successful_retries     = old_fsa[i].successful_retries;
              new_fsa[i].max_successful_retries = old_fsa[i].max_successful_retries;
              new_fsa[i].special_flag           = old_fsa[i].special_flag;
              new_fsa[i].protocol               = old_fsa[i].protocol;
              new_fsa[i].protocol_options       = old_fsa[i].protocol_options;
              new_fsa[i].socksnd_bufsize        = old_fsa[i].socksnd_bufsize;
              new_fsa[i].sockrcv_bufsize        = old_fsa[i].sockrcv_bufsize;
              new_fsa[i].keep_connected         = old_fsa[i].keep_connected;
#ifdef WITH_DUP_CHECK
              new_fsa[i].dup_check_flag         = old_fsa[i].dup_check_flag;
#endif
              new_fsa[i].host_id                = old_fsa[i].host_id;
              new_fsa[i].debug                  = old_fsa[i].debug;
              new_fsa[i].host_toggle            = old_fsa[i].host_toggle;
              new_fsa[i].host_status            = old_fsa[i].host_status;
              new_fsa[i].error_counter          = old_fsa[i].error_counter;
              new_fsa[i].total_errors           = old_fsa[i].total_errors;
              new_fsa[i].max_errors             = old_fsa[i].max_errors;
              (void)memcpy(new_fsa[i].error_history, old_fsa[i].error_history, ERROR_HISTORY_LENGTH);
              new_fsa[i].retry_interval         = old_fsa[i].retry_interval;
              new_fsa[i].block_size             = old_fsa[i].block_size;
              new_fsa[i].ttl                    = old_fsa[i].ttl;
#ifdef WITH_DUP_CHECK
              new_fsa[i].dup_check_timeout      = old_fsa[i].dup_check_timeout;
#endif
              new_fsa[i].last_retry_time        = old_fsa[i].last_retry_time;
              new_fsa[i].last_connection        = old_fsa[i].last_connection;
              new_fsa[i].first_error_time       = old_fsa[i].first_error_time;
              new_fsa[i].start_event_handle     = 0L;
              new_fsa[i].end_event_handle       = 0L;
              new_fsa[i].warn_time              = 0L;
              new_fsa[i].total_file_counter     = old_fsa[i].total_file_counter;
              new_fsa[i].total_file_size        = old_fsa[i].total_file_size;
              new_fsa[i].jobs_queued            = old_fsa[i].jobs_queued;
              new_fsa[i].file_counter_done      = old_fsa[i].file_counter_done;
              new_fsa[i].bytes_send             = old_fsa[i].bytes_send;
              new_fsa[i].connections            = old_fsa[i].connections;
              new_fsa[i].mc_nack_counter        = old_fsa[i].mc_nack_counter;
              new_fsa[i].active_transfers       = old_fsa[i].active_transfers;
              new_fsa[i].allowed_transfers      = old_fsa[i].allowed_transfers;
              new_fsa[i].transfer_rate_limit    = old_fsa[i].transfer_rate_limit;
              new_fsa[i].trl_per_process        = old_fsa[i].trl_per_process;
              new_fsa[i].mc_ct_rate_limit       = old_fsa[i].mc_ct_rate_limit;
              new_fsa[i].mc_ctrl_per_process    = old_fsa[i].mc_ctrl_per_process;
              new_fsa[i].transfer_timeout       = old_fsa[i].transfer_timeout;
              for (j = 0; j < MAX_NO_PARALLEL_JOBS_2; j++)
              {
                 new_fsa[i].job_status[j].proc_id = old_fsa[i].job_status[j].proc_id;
#ifdef _WITH_BURST_2
                 (void)memcpy(new_fsa[i].job_status[j].unique_name, old_fsa[i].job_status[j].unique_name, MAX_MSG_NAME_LENGTH_2);
                 new_fsa[i].job_status[j].job_id = old_fsa[i].job_status[j].job_id;
#endif
                 new_fsa[i].job_status[j].connect_status = old_fsa[i].job_status[j].connect_status;
                 new_fsa[i].job_status[j].no_of_files = old_fsa[i].job_status[j].no_of_files;
                 new_fsa[i].job_status[j].no_of_files_done = old_fsa[i].job_status[j].no_of_files_done;
                 new_fsa[i].job_status[j].file_size = old_fsa[i].job_status[j].file_size;
                 new_fsa[i].job_status[j].file_size_done = old_fsa[i].job_status[j].file_size_done;
                 new_fsa[i].job_status[j].bytes_send = old_fsa[i].job_status[j].bytes_send;
                 (void)strcpy(new_fsa[i].job_status[j].file_name_in_use, old_fsa[i].job_status[j].file_name_in_use);
                 new_fsa[i].job_status[j].file_size_in_use = old_fsa[i].job_status[j].file_size_in_use;
                 new_fsa[i].job_status[j].file_size_in_use_done = old_fsa[i].job_status[j].file_size_in_use_done;
              }
           }

           ptr = (char *)old_fsa;
           ptr -= AFD_WORD_OFFSET_2;

           /*
            * Resize the old FSA to the size of new one and then copy
            * the new structure into it. Then update the FSA version
            * number.
            */
           if ((ptr = mmap_resize(old_fsa_fd, ptr,
                                  new_size + AFD_WORD_OFFSET_3)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fsa_stat, strerror(errno));
              free((void *)new_fsa);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_3;
           (void)memcpy(ptr, new_fsa, new_size);
           free((void *)new_fsa);
           ptr -= AFD_WORD_OFFSET_3;
           *(ptr + SIZEOF_INT + 1) = old_features;
           *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           *(int *)(ptr + SIZEOF_INT + 4) = pagesize;
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
           *old_fsa_size = new_size + AFD_WORD_OFFSET_3;

           system_log(INFO_SIGN, NULL, 0,
                      "Converted FSA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 0) && (new_version == 4))
        {
           struct filetransfer_status_0 *old_fsa;
           struct filetransfer_status_4 *new_fsa;

           /* Get the size of the old FSA file. */
#ifdef HAVE_STATX
           if (statx(old_fsa_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                     STATX_SIZE, &stat_buf) == -1)
#else
           if (fstat(old_fsa_fd, &stat_buf) == -1)
#endif
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                         "Failed to statx() %s : %s",
#else
                         "Failed to fstat() %s : %s",
#endif
                         old_fsa_stat, strerror(errno));
              *old_fsa_size = -1;
              return(NULL);
           }
           else
           {
#ifdef HAVE_STATX
              if (stat_buf.stx_size > 0)
#else
              if (stat_buf.st_size > 0)
#endif
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                                 stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                                 stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                                 MAP_SHARED, old_fsa_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                                     stat_buf.stx_size,
# else
                                     stat_buf.st_size,
# endif
                                     (PROT_READ | PROT_WRITE),
                                     MAP_SHARED, old_fsa_stat,
                                     0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fsa_stat, strerror(errno));
                    *old_fsa_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FSA file %s is empty.", old_fsa_stat);
                 *old_fsa_size = -1;
                 return(NULL);
              }
           }

           old_features = *(ptr + SIZEOF_INT + 1);
           ptr += AFD_WORD_OFFSET_0;
           old_fsa = (struct filetransfer_status_0 *)ptr;

           new_size = old_no_of_hosts * sizeof(struct filetransfer_status_4);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_hosts, new_size, strerror(errno));
              ptr = (char *)old_fsa;
              ptr -= AFD_WORD_OFFSET_0;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
              if (munmap(ptr, stat_buf.stx_size) == -1)
# else
              if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fsa_stat, strerror(errno));
              }
              *old_fsa_size = -1;
              return(NULL);
           }
           (void)memset(ptr, 0, new_size);
           new_fsa = (struct filetransfer_status_4 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_hosts; i++)
           {
              (void)strcpy(new_fsa[i].host_alias, old_fsa[i].host_alias);
              (void)strcpy(new_fsa[i].real_hostname[0], old_fsa[i].real_hostname[0]);
              (void)strcpy(new_fsa[i].real_hostname[1], old_fsa[i].real_hostname[1]);
              (void)strcpy(new_fsa[i].host_dsp_name, old_fsa[i].host_dsp_name);
              new_fsa[i].host_dsp_name[MAX_HOSTNAME_LENGTH_4 + 1] = '\0';
              (void)strcpy(new_fsa[i].proxy_name, old_fsa[i].proxy_name);
              (void)strcpy(new_fsa[i].host_toggle_str, old_fsa[i].host_toggle_str);
              new_fsa[i].toggle_pos             = old_fsa[i].toggle_pos;
              new_fsa[i].original_toggle_pos    = old_fsa[i].original_toggle_pos;
              new_fsa[i].auto_toggle            = old_fsa[i].auto_toggle;
              new_fsa[i].file_size_offset       = old_fsa[i].file_size_offset;
              new_fsa[i].successful_retries     = old_fsa[i].successful_retries;
              new_fsa[i].max_successful_retries = old_fsa[i].max_successful_retries;
              new_fsa[i].special_flag           = old_fsa[i].special_flag;
              new_fsa[i].protocol               = 0;
              if (old_fsa[i].protocol & FTP_FLAG)
              {
                 new_fsa[i].protocol |= FTP_FLAG;
              }
              if (old_fsa[i].protocol & LOC_FLAG)
              {
                 new_fsa[i].protocol |= LOC_FLAG;
              }
              if (old_fsa[i].protocol & SMTP_FLAG)
              {
                 new_fsa[i].protocol |= SMTP_FLAG;
              }
#ifdef _WITH_MAP_SUPPORT
              if (old_fsa[i].protocol & MAP_FLAG)
              {
                 new_fsa[i].protocol |= MAP_FLAG;
              }
#endif
#ifdef _WITH_SCP_SUPPORT
              if (old_fsa[i].protocol & SCP_FLAG)
              {
                 new_fsa[i].protocol |= SCP_FLAG;
              }
#endif
#ifdef _WITH_WMO_SUPPORT
              if (old_fsa[i].protocol & WMO_FLAG)
              {
                 new_fsa[i].protocol |= WMO_FLAG;
              }
#endif
              if (old_fsa[i].protocol & GET_FTP_FLAG_0)
              {
                 new_fsa[i].protocol |= GET_FTP_FLAG_4;
              }
              if (old_fsa[i].protocol & SEND_FLAG_0)
              {
                 new_fsa[i].protocol |= SEND_FLAG_4;
              }
              if (old_fsa[i].protocol & RETRIEVE_FLAG_0)
              {
                 new_fsa[i].protocol |= RETRIEVE_FLAG_4;
              }
              new_fsa[i].protocol_options = 0;
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
              if (old_fsa[i].protocol & STAT_KEEPALIVE_0)
              {
                 new_fsa[i].protocol_options |= STAT_KEEPALIVE_4;
              }
#endif
              if (old_fsa[i].protocol & SET_IDLE_TIME_0)
              {
                 new_fsa[i].protocol_options |= SET_IDLE_TIME_4;
              }
              if (old_fsa[i].protocol & FTP_PASSIVE_MODE_0)
              {
                 new_fsa[i].protocol_options |= FTP_PASSIVE_MODE_4;
              }
              new_fsa[i].socksnd_bufsize        = 0U;
              new_fsa[i].sockrcv_bufsize        = 0U;
              new_fsa[i].keep_connected         = 0U;
#ifdef WITH_DUP_CHECK
              new_fsa[i].dup_check_flag         = 0U;
#endif
              new_fsa[i].host_id                = get_str_checksum(new_fsa[i].host_alias);
              if (old_fsa[i].debug == NO)
              {
                 new_fsa[i].debug               = NORMAL_MODE;
              }
              else
              {
                 new_fsa[i].debug               = DEBUG_MODE;
              }
              new_fsa[i].host_toggle            = old_fsa[i].host_toggle;
              new_fsa[i].host_status            = old_fsa[i].host_status;
              new_fsa[i].error_counter          = old_fsa[i].error_counter;
              new_fsa[i].total_errors           = old_fsa[i].total_errors;
              new_fsa[i].max_errors             = old_fsa[i].max_errors;
              for (j = 0; j < ERROR_HISTORY_LENGTH; j++)
              {
                 new_fsa[i].error_history[j] = 0;
              }
              new_fsa[i].retry_interval         = old_fsa[i].retry_interval;
              new_fsa[i].block_size             = old_fsa[i].block_size;
              new_fsa[i].ttl                    = 0;
#ifdef WITH_DUP_CHECK
              new_fsa[i].dup_check_timeout      = 0L;
#endif
              new_fsa[i].last_retry_time        = old_fsa[i].last_retry_time;
              new_fsa[i].last_connection        = old_fsa[i].last_connection;
              new_fsa[i].first_error_time       = 0L;
              new_fsa[i].start_event_handle     = 0L;
              new_fsa[i].end_event_handle       = 0L;
              new_fsa[i].warn_time              = 0L;
              new_fsa[i].total_file_counter     = old_fsa[i].total_file_counter;
              new_fsa[i].total_file_size        = (off_t)(old_fsa[i].total_file_size);
              new_fsa[i].jobs_queued            = old_fsa[i].jobs_queued;
              new_fsa[i].file_counter_done      = old_fsa[i].file_counter_done;
              new_fsa[i].bytes_send             = (u_off_t)(old_fsa[i].bytes_send);
              new_fsa[i].connections            = old_fsa[i].connections;
              new_fsa[i].active_transfers       = old_fsa[i].active_transfers;
              new_fsa[i].allowed_transfers      = old_fsa[i].allowed_transfers;
              new_fsa[i].transfer_rate_limit    = 0;
              new_fsa[i].trl_per_process        = 0;
              new_fsa[i].transfer_timeout       = old_fsa[i].transfer_timeout;
              for (j = 0; j < MAX_NO_PARALLEL_JOBS_0; j++)
              {
                 new_fsa[i].job_status[j].proc_id = old_fsa[i].job_status[j].proc_id;
#ifdef _WITH_BURST_2
                 (void)strcpy(new_fsa[i].job_status[j].unique_name, old_fsa[i].job_status[j].unique_name);
                 new_fsa[i].job_status[j].job_id = old_fsa[i].job_status[j].job_id;
#endif
                 new_fsa[i].job_status[j].connect_status = old_fsa[i].job_status[j].connect_status;
                 new_fsa[i].job_status[j].no_of_files = old_fsa[i].job_status[j].no_of_files;
                 new_fsa[i].job_status[j].no_of_files_done = old_fsa[i].job_status[j].no_of_files_done;
                 new_fsa[i].job_status[j].file_size = (off_t)old_fsa[i].job_status[j].file_size;
                 new_fsa[i].job_status[j].file_size_done = (u_off_t)old_fsa[i].job_status[j].file_size_done;
                 new_fsa[i].job_status[j].bytes_send = (u_off_t)old_fsa[i].job_status[j].bytes_send;
                 (void)strcpy(new_fsa[i].job_status[j].file_name_in_use, old_fsa[i].job_status[j].file_name_in_use);
                 new_fsa[i].job_status[j].file_size_in_use = (off_t)old_fsa[i].job_status[j].file_size_in_use;
                 new_fsa[i].job_status[j].file_size_in_use_done = (off_t)old_fsa[i].job_status[j].file_size_in_use_done;
              }
           }

           ptr = (char *)old_fsa;
           ptr -= AFD_WORD_OFFSET_0;

           /*
            * Resize the old FSA to the size of new one and then copy
            * the new structure into it. Then update the FSA version
            * number.
            */
           if ((ptr = mmap_resize(old_fsa_fd, ptr,
                                  new_size + AFD_WORD_OFFSET_4)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fsa_stat, strerror(errno));
              free((void *)new_fsa);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_4;
           (void)memcpy(ptr, new_fsa, new_size);
           free((void *)new_fsa);
           ptr -= AFD_WORD_OFFSET_4;
           *(ptr + SIZEOF_INT + 1) = old_features;
           *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           *(int *)(ptr + SIZEOF_INT + 4) = pagesize;
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
           *old_fsa_size = new_size + AFD_WORD_OFFSET_4;

           system_log(INFO_SIGN, NULL, 0,
                      "Converted FSA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 1) && (new_version == 4))
        {
           struct filetransfer_status_1 *old_fsa;
           struct filetransfer_status_4 *new_fsa;

           /* Get the size of the old FSA file. */
#ifdef HAVE_STATX
           if (statx(old_fsa_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                     STATX_SIZE, &stat_buf) == -1)
#else
           if (fstat(old_fsa_fd, &stat_buf) == -1)
#endif
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                         "Failed to statx() %s : %s",
#else
                         "Failed to fstat() %s : %s",
#endif
                         old_fsa_stat, strerror(errno));
              *old_fsa_size = -1;
              return(NULL);
           }
           else
           {
#ifdef HAVE_STATX
              if (stat_buf.stx_size > 0)
#else
              if (stat_buf.st_size > 0)
#endif
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                                 stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                                 stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                                 MAP_SHARED, old_fsa_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                                     stat_buf.stx_size,
# else
                                     stat_buf.st_size,
# endif
                                     (PROT_READ | PROT_WRITE),
                                     MAP_SHARED, old_fsa_stat,
                                     0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fsa_stat, strerror(errno));
                    *old_fsa_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FSA file %s is empty.", old_fsa_stat);
                 *old_fsa_size = -1;
                 return(NULL);
              }
           }

           old_features = *(ptr + SIZEOF_INT + 1);
           ptr += AFD_WORD_OFFSET_1;
           old_fsa = (struct filetransfer_status_1 *)ptr;

           new_size = old_no_of_hosts * sizeof(struct filetransfer_status_4);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_hosts, new_size, strerror(errno));
              ptr = (char *)old_fsa;
              ptr -= AFD_WORD_OFFSET_1;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
              if (munmap(ptr, stat_buf.stx_size) == -1)
# else
              if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fsa_stat, strerror(errno));
              }
              *old_fsa_size = -1;
              return(NULL);
           }
           (void)memset(ptr, 0, new_size);
           new_fsa = (struct filetransfer_status_4 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_hosts; i++)
           {
              (void)my_strncpy(new_fsa[i].host_alias, old_fsa[i].host_alias, MAX_HOSTNAME_LENGTH_4 + 1);
              (void)strcpy(new_fsa[i].real_hostname[0], old_fsa[i].real_hostname[0]);
              (void)strcpy(new_fsa[i].real_hostname[1], old_fsa[i].real_hostname[1]);
              (void)my_strncpy(new_fsa[i].host_dsp_name, old_fsa[i].host_dsp_name, MAX_HOSTNAME_LENGTH_4 + 1);
              new_fsa[i].host_dsp_name[MAX_HOSTNAME_LENGTH_4 + 1] = '\0';
              (void)strcpy(new_fsa[i].proxy_name, old_fsa[i].proxy_name);
              (void)strcpy(new_fsa[i].host_toggle_str, old_fsa[i].host_toggle_str);
              new_fsa[i].toggle_pos             = old_fsa[i].toggle_pos;
              new_fsa[i].original_toggle_pos    = old_fsa[i].original_toggle_pos;
              new_fsa[i].auto_toggle            = old_fsa[i].auto_toggle;
              new_fsa[i].file_size_offset       = old_fsa[i].file_size_offset;
              new_fsa[i].successful_retries     = old_fsa[i].successful_retries;
              new_fsa[i].max_successful_retries = old_fsa[i].max_successful_retries;
              new_fsa[i].special_flag           = old_fsa[i].special_flag;
              new_fsa[i].protocol               = old_fsa[i].protocol;
              new_fsa[i].protocol_options       = old_fsa[i].protocol_options;
              new_fsa[i].protocol_options2      = 0;
              new_fsa[i].socksnd_bufsize        = 0U;
              new_fsa[i].sockrcv_bufsize        = 0U;
              new_fsa[i].keep_connected         = 0U;
#ifdef WITH_DUP_CHECK
              new_fsa[i].dup_check_flag         = 0U;
#endif
              new_fsa[i].host_id                = get_str_checksum(new_fsa[i].host_alias);
              new_fsa[i].debug                  = old_fsa[i].debug;
              new_fsa[i].host_toggle            = old_fsa[i].host_toggle;
              new_fsa[i].host_status            = old_fsa[i].host_status;
              new_fsa[i].error_counter          = old_fsa[i].error_counter;
              new_fsa[i].total_errors           = old_fsa[i].total_errors;
              new_fsa[i].max_errors             = old_fsa[i].max_errors;
              (void)memcpy(new_fsa[i].error_history, old_fsa[i].error_history, ERROR_HISTORY_LENGTH);
              new_fsa[i].retry_interval         = old_fsa[i].retry_interval;
              new_fsa[i].block_size             = old_fsa[i].block_size;
              new_fsa[i].ttl                    = old_fsa[i].ttl;
#ifdef WITH_DUP_CHECK
              new_fsa[i].dup_check_timeout      = 0L;
#endif
              new_fsa[i].last_retry_time        = old_fsa[i].last_retry_time;
              new_fsa[i].last_connection        = old_fsa[i].last_connection;
              new_fsa[i].first_error_time       = old_fsa[i].first_error_time;
              new_fsa[i].start_event_handle     = 0L;
              new_fsa[i].end_event_handle       = 0L;
              new_fsa[i].warn_time              = 0L;
              new_fsa[i].total_file_counter     = old_fsa[i].total_file_counter;
              new_fsa[i].total_file_size        = old_fsa[i].total_file_size;
              new_fsa[i].jobs_queued            = old_fsa[i].jobs_queued;
              new_fsa[i].file_counter_done      = old_fsa[i].file_counter_done;
              new_fsa[i].bytes_send             = old_fsa[i].bytes_send;
              new_fsa[i].connections            = old_fsa[i].connections;
              new_fsa[i].active_transfers       = old_fsa[i].active_transfers;
              new_fsa[i].allowed_transfers      = old_fsa[i].allowed_transfers;
              new_fsa[i].transfer_rate_limit    = old_fsa[i].transfer_rate_limit;
              new_fsa[i].trl_per_process        = old_fsa[i].trl_per_process;
              new_fsa[i].transfer_timeout       = old_fsa[i].transfer_timeout;
              for (j = 0; j < MAX_NO_PARALLEL_JOBS_1; j++)
              {
                 new_fsa[i].job_status[j].proc_id = old_fsa[i].job_status[j].proc_id;
#ifdef _WITH_BURST_2
                 (void)memcpy(new_fsa[i].job_status[j].unique_name, old_fsa[i].job_status[j].unique_name, MAX_MSG_NAME_LENGTH_1);
                 new_fsa[i].job_status[j].job_id = old_fsa[i].job_status[j].job_id;
#endif
                 new_fsa[i].job_status[j].connect_status = old_fsa[i].job_status[j].connect_status;
                 new_fsa[i].job_status[j].no_of_files = old_fsa[i].job_status[j].no_of_files;
                 new_fsa[i].job_status[j].no_of_files_done = old_fsa[i].job_status[j].no_of_files_done;
                 new_fsa[i].job_status[j].file_size = old_fsa[i].job_status[j].file_size;
                 new_fsa[i].job_status[j].file_size_done = old_fsa[i].job_status[j].file_size_done;
                 new_fsa[i].job_status[j].bytes_send = old_fsa[i].job_status[j].bytes_send;
                 (void)strcpy(new_fsa[i].job_status[j].file_name_in_use, old_fsa[i].job_status[j].file_name_in_use);
                 new_fsa[i].job_status[j].file_size_in_use = old_fsa[i].job_status[j].file_size_in_use;
                 new_fsa[i].job_status[j].file_size_in_use_done = old_fsa[i].job_status[j].file_size_in_use_done;
              }
           }

           ptr = (char *)old_fsa;
           ptr -= AFD_WORD_OFFSET_1;

           /*
            * Resize the old FSA to the size of new one and then copy
            * the new structure into it. Then update the FSA version
            * number.
            */
           if ((ptr = mmap_resize(old_fsa_fd, ptr,
                                  new_size + AFD_WORD_OFFSET_4)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fsa_stat, strerror(errno));
              free((void *)new_fsa);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_4;
           (void)memcpy(ptr, new_fsa, new_size);
           free((void *)new_fsa);
           ptr -= AFD_WORD_OFFSET_4;
           *(ptr + SIZEOF_INT + 1) = old_features;
           *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           *(int *)(ptr + SIZEOF_INT + 4) = pagesize;
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
           *old_fsa_size = new_size + AFD_WORD_OFFSET_4;

           system_log(INFO_SIGN, NULL, 0,
                      "Converted FSA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 2) && (new_version == 4))
        {
           struct filetransfer_status_2 *old_fsa;
           struct filetransfer_status_4 *new_fsa;

           /* Get the size of the old FSA file. */
#ifdef HAVE_STATX
           if (statx(old_fsa_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                     STATX_SIZE, &stat_buf) == -1)
#else
           if (fstat(old_fsa_fd, &stat_buf) == -1)
#endif
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                         "Failed to statx() %s : %s",
#else
                         "Failed to fstat() %s : %s",
#endif
                         old_fsa_stat, strerror(errno));
              *old_fsa_size = -1;
              return(NULL);
           }
           else
           {
#ifdef HAVE_STATX
              if (stat_buf.stx_size > 0)
#else
              if (stat_buf.st_size > 0)
#endif
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                                 stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                                 stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                                 MAP_SHARED, old_fsa_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                                     stat_buf.stx_size,
# else
                                     stat_buf.st_size,
# endif
                                     (PROT_READ | PROT_WRITE),
                                     MAP_SHARED, old_fsa_stat,
                                     0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fsa_stat, strerror(errno));
                    *old_fsa_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FSA file %s is empty.", old_fsa_stat);
                 *old_fsa_size = -1;
                 return(NULL);
              }
           }

           old_features = *(ptr + SIZEOF_INT + 1);
           ptr += AFD_WORD_OFFSET_2;
           old_fsa = (struct filetransfer_status_2 *)ptr;

           new_size = old_no_of_hosts * sizeof(struct filetransfer_status_4);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_hosts, new_size, strerror(errno));
              ptr = (char *)old_fsa;
              ptr -= AFD_WORD_OFFSET_2;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
              if (munmap(ptr, stat_buf.stx_size) == -1)
# else
              if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fsa_stat, strerror(errno));
              }
              *old_fsa_size = -1;
              return(NULL);
           }
           (void)memset(ptr, 0, new_size);
           new_fsa = (struct filetransfer_status_4 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_hosts; i++)
           {
              (void)my_strncpy(new_fsa[i].host_alias, old_fsa[i].host_alias, MAX_HOSTNAME_LENGTH_4 + 1);
              (void)strcpy(new_fsa[i].real_hostname[0], old_fsa[i].real_hostname[0]);
              (void)strcpy(new_fsa[i].real_hostname[1], old_fsa[i].real_hostname[1]);
              (void)my_strncpy(new_fsa[i].host_dsp_name, old_fsa[i].host_dsp_name, MAX_HOSTNAME_LENGTH_4 + 1);
              new_fsa[i].host_dsp_name[MAX_HOSTNAME_LENGTH_4 + 1] = '\0';
              (void)strcpy(new_fsa[i].proxy_name, old_fsa[i].proxy_name);
              (void)strcpy(new_fsa[i].host_toggle_str, old_fsa[i].host_toggle_str);
              new_fsa[i].toggle_pos             = old_fsa[i].toggle_pos;
              new_fsa[i].original_toggle_pos    = old_fsa[i].original_toggle_pos;
              new_fsa[i].auto_toggle            = old_fsa[i].auto_toggle;
              new_fsa[i].file_size_offset       = old_fsa[i].file_size_offset;
              new_fsa[i].successful_retries     = old_fsa[i].successful_retries;
              new_fsa[i].max_successful_retries = old_fsa[i].max_successful_retries;
              new_fsa[i].special_flag           = old_fsa[i].special_flag;
              new_fsa[i].protocol               = old_fsa[i].protocol;
              new_fsa[i].protocol_options       = old_fsa[i].protocol_options;
              new_fsa[i].protocol_options2      = 0;
              new_fsa[i].socksnd_bufsize        = old_fsa[i].socksnd_bufsize;
              new_fsa[i].sockrcv_bufsize        = old_fsa[i].sockrcv_bufsize;
              new_fsa[i].keep_connected         = old_fsa[i].keep_connected;
#ifdef WITH_DUP_CHECK
              new_fsa[i].dup_check_flag         = old_fsa[i].dup_check_flag;
#endif
              new_fsa[i].host_id                = old_fsa[i].host_id;
              new_fsa[i].debug                  = old_fsa[i].debug;
              new_fsa[i].host_toggle            = old_fsa[i].host_toggle;
              new_fsa[i].host_status            = old_fsa[i].host_status;
              new_fsa[i].error_counter          = old_fsa[i].error_counter;
              new_fsa[i].total_errors           = old_fsa[i].total_errors;
              new_fsa[i].max_errors             = old_fsa[i].max_errors;
              (void)memcpy(new_fsa[i].error_history, old_fsa[i].error_history, ERROR_HISTORY_LENGTH);
              new_fsa[i].retry_interval         = old_fsa[i].retry_interval;
              new_fsa[i].block_size             = old_fsa[i].block_size;
              new_fsa[i].ttl                    = old_fsa[i].ttl;
#ifdef WITH_DUP_CHECK
              new_fsa[i].dup_check_timeout      = old_fsa[i].dup_check_timeout;
#endif
              new_fsa[i].last_retry_time        = old_fsa[i].last_retry_time;
              new_fsa[i].last_connection        = old_fsa[i].last_connection;
              new_fsa[i].first_error_time       = old_fsa[i].first_error_time;
              new_fsa[i].start_event_handle     = 0L;
              new_fsa[i].end_event_handle       = 0L;
              new_fsa[i].warn_time              = 0L;
              new_fsa[i].total_file_counter     = old_fsa[i].total_file_counter;
              new_fsa[i].total_file_size        = old_fsa[i].total_file_size;
              new_fsa[i].jobs_queued            = old_fsa[i].jobs_queued;
              new_fsa[i].file_counter_done      = old_fsa[i].file_counter_done;
              new_fsa[i].bytes_send             = old_fsa[i].bytes_send;
              new_fsa[i].connections            = old_fsa[i].connections;
              new_fsa[i].active_transfers       = old_fsa[i].active_transfers;
              new_fsa[i].allowed_transfers      = old_fsa[i].allowed_transfers;
              new_fsa[i].transfer_rate_limit    = old_fsa[i].transfer_rate_limit;
              new_fsa[i].trl_per_process        = old_fsa[i].trl_per_process;
              new_fsa[i].transfer_timeout       = old_fsa[i].transfer_timeout;
              for (j = 0; j < MAX_NO_PARALLEL_JOBS_2; j++)
              {
                 new_fsa[i].job_status[j].proc_id = old_fsa[i].job_status[j].proc_id;
#ifdef _WITH_BURST_2
                 (void)memcpy(new_fsa[i].job_status[j].unique_name, old_fsa[i].job_status[j].unique_name, MAX_MSG_NAME_LENGTH_2);
                 new_fsa[i].job_status[j].job_id = old_fsa[i].job_status[j].job_id;
#endif
                 new_fsa[i].job_status[j].connect_status = old_fsa[i].job_status[j].connect_status;
                 new_fsa[i].job_status[j].no_of_files = old_fsa[i].job_status[j].no_of_files;
                 new_fsa[i].job_status[j].no_of_files_done = old_fsa[i].job_status[j].no_of_files_done;
                 new_fsa[i].job_status[j].file_size = old_fsa[i].job_status[j].file_size;
                 new_fsa[i].job_status[j].file_size_done = old_fsa[i].job_status[j].file_size_done;
                 new_fsa[i].job_status[j].bytes_send = old_fsa[i].job_status[j].bytes_send;
                 (void)strcpy(new_fsa[i].job_status[j].file_name_in_use, old_fsa[i].job_status[j].file_name_in_use);
                 new_fsa[i].job_status[j].file_size_in_use = old_fsa[i].job_status[j].file_size_in_use;
                 new_fsa[i].job_status[j].file_size_in_use_done = old_fsa[i].job_status[j].file_size_in_use_done;
              }
           }

           ptr = (char *)old_fsa;
           ptr -= AFD_WORD_OFFSET_2;

           /*
            * Resize the old FSA to the size of new one and then copy
            * the new structure into it. Then update the FSA version
            * number.
            */
           if ((ptr = mmap_resize(old_fsa_fd, ptr,
                                  new_size + AFD_WORD_OFFSET_4)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fsa_stat, strerror(errno));
              free((void *)new_fsa);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_4;
           (void)memcpy(ptr, new_fsa, new_size);
           free((void *)new_fsa);
           ptr -= AFD_WORD_OFFSET_4;
           *(ptr + SIZEOF_INT + 1) = old_features;
           *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           *(int *)(ptr + SIZEOF_INT + 4) = pagesize;
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
           *old_fsa_size = new_size + AFD_WORD_OFFSET_4;

           system_log(INFO_SIGN, NULL, 0,
                      "Converted FSA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 3) && (new_version == 4))
        {
           int                          ignore_first_errors;
           struct filetransfer_status_3 *old_fsa;
           struct filetransfer_status_4 *new_fsa;

           /* Get the size of the old FSA file. */
#ifdef HAVE_STATX
           if (statx(old_fsa_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                     STATX_SIZE, &stat_buf) == -1)
#else
           if (fstat(old_fsa_fd, &stat_buf) == -1)
#endif
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                         "Failed to statx() %s : %s",
#else
                         "Failed to fstat() %s : %s",
#endif
                         old_fsa_stat, strerror(errno));
              *old_fsa_size = -1;
              return(NULL);
           }
           else
           {
#ifdef HAVE_STATX
              if (stat_buf.stx_size > 0)
#else
              if (stat_buf.st_size > 0)
#endif
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                                 stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                                 stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                                 MAP_SHARED, old_fsa_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                                     stat_buf.stx_size,
# else
                                     stat_buf.st_size,
# endif
                                     (PROT_READ | PROT_WRITE),
                                     MAP_SHARED, old_fsa_stat,
                                     0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fsa_stat, strerror(errno));
                    *old_fsa_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FSA file %s is empty.", old_fsa_stat);
                 *old_fsa_size = -1;
                 return(NULL);
              }
           }

           old_features = *(ptr + SIZEOF_INT + 1);
           ignore_first_errors = *(ptr + SIZEOF_INT + 1 + 1);
           ptr += AFD_WORD_OFFSET_3;
           old_fsa = (struct filetransfer_status_3 *)ptr;

           new_size = old_no_of_hosts * sizeof(struct filetransfer_status_4);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_hosts, new_size, strerror(errno));
              ptr = (char *)old_fsa;
              ptr -= AFD_WORD_OFFSET_3;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
              if (munmap(ptr, stat_buf.stx_size) == -1)
# else
              if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fsa_stat, strerror(errno));
              }
              *old_fsa_size = -1;
              return(NULL);
           }
           (void)memset(ptr, 0, new_size);
           new_fsa = (struct filetransfer_status_4 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_hosts; i++)
           {
              (void)my_strncpy(new_fsa[i].host_alias, old_fsa[i].host_alias, MAX_HOSTNAME_LENGTH_4 + 1);
              if (old_fsa[i].real_hostname[0][0] == GROUP_IDENTIFIER)
              {
                 new_fsa[i].real_hostname[0][0] = 1;
                 new_fsa[i].real_hostname[1][0] = '\0';
              }
              else
              {
                 (void)strcpy(new_fsa[i].real_hostname[0], old_fsa[i].real_hostname[0]);
                 (void)strcpy(new_fsa[i].real_hostname[1], old_fsa[i].real_hostname[1]);
              }
              (void)my_strncpy(new_fsa[i].host_dsp_name, old_fsa[i].host_dsp_name, MAX_HOSTNAME_LENGTH_4 + 1);
              new_fsa[i].host_dsp_name[MAX_HOSTNAME_LENGTH_4 + 1] = '\0';
              (void)strcpy(new_fsa[i].proxy_name, old_fsa[i].proxy_name);
              (void)strcpy(new_fsa[i].host_toggle_str, old_fsa[i].host_toggle_str);
              new_fsa[i].toggle_pos             = old_fsa[i].toggle_pos;
              new_fsa[i].original_toggle_pos    = old_fsa[i].original_toggle_pos;
              new_fsa[i].auto_toggle            = old_fsa[i].auto_toggle;
              new_fsa[i].file_size_offset       = old_fsa[i].file_size_offset;
              new_fsa[i].successful_retries     = old_fsa[i].successful_retries;
              new_fsa[i].max_successful_retries = old_fsa[i].max_successful_retries;
              new_fsa[i].special_flag           = old_fsa[i].special_flag;
              new_fsa[i].protocol               = old_fsa[i].protocol;
              new_fsa[i].protocol_options       = old_fsa[i].protocol_options;
              new_fsa[i].protocol_options2      = 0;
              new_fsa[i].socksnd_bufsize        = old_fsa[i].socksnd_bufsize;
              new_fsa[i].sockrcv_bufsize        = old_fsa[i].sockrcv_bufsize;
              new_fsa[i].keep_connected         = old_fsa[i].keep_connected;
#ifdef WITH_DUP_CHECK
              new_fsa[i].dup_check_flag         = old_fsa[i].dup_check_flag;
#endif
              new_fsa[i].host_id                = old_fsa[i].host_id;
              new_fsa[i].debug                  = old_fsa[i].debug;
              new_fsa[i].host_toggle            = old_fsa[i].host_toggle;
              new_fsa[i].host_status            = old_fsa[i].host_status;
              new_fsa[i].error_counter          = old_fsa[i].error_counter;
              new_fsa[i].total_errors           = old_fsa[i].total_errors;
              new_fsa[i].max_errors             = old_fsa[i].max_errors;
              (void)memcpy(new_fsa[i].error_history, old_fsa[i].error_history, ERROR_HISTORY_LENGTH);
              new_fsa[i].retry_interval         = old_fsa[i].retry_interval;
              new_fsa[i].block_size             = old_fsa[i].block_size;
              new_fsa[i].ttl                    = old_fsa[i].ttl;
#ifdef WITH_DUP_CHECK
              new_fsa[i].dup_check_timeout      = old_fsa[i].dup_check_timeout;
#endif
              new_fsa[i].last_retry_time        = old_fsa[i].last_retry_time;
              new_fsa[i].last_connection        = old_fsa[i].last_connection;
              new_fsa[i].first_error_time       = old_fsa[i].first_error_time;
              new_fsa[i].start_event_handle     = old_fsa[i].start_event_handle;
              new_fsa[i].end_event_handle       = old_fsa[i].end_event_handle;
              new_fsa[i].warn_time              = old_fsa[i].warn_time;
              new_fsa[i].total_file_counter     = old_fsa[i].total_file_counter;
              new_fsa[i].total_file_size        = old_fsa[i].total_file_size;
              new_fsa[i].jobs_queued            = old_fsa[i].jobs_queued;
              new_fsa[i].file_counter_done      = old_fsa[i].file_counter_done;
              new_fsa[i].bytes_send             = old_fsa[i].bytes_send;
              new_fsa[i].connections            = old_fsa[i].connections;
              new_fsa[i].active_transfers       = old_fsa[i].active_transfers;
              new_fsa[i].allowed_transfers      = old_fsa[i].allowed_transfers;
              new_fsa[i].transfer_rate_limit    = old_fsa[i].transfer_rate_limit;
              new_fsa[i].trl_per_process        = old_fsa[i].trl_per_process;
              new_fsa[i].transfer_timeout       = old_fsa[i].transfer_timeout;
              for (j = 0; j < MAX_NO_PARALLEL_JOBS_3; j++)
              {
                 new_fsa[i].job_status[j].proc_id = old_fsa[i].job_status[j].proc_id;
#ifdef _WITH_BURST_2
                 (void)memcpy(new_fsa[i].job_status[j].unique_name, old_fsa[i].job_status[j].unique_name, MAX_MSG_NAME_LENGTH_3);
                 new_fsa[i].job_status[j].job_id = old_fsa[i].job_status[j].job_id;
#endif
                 new_fsa[i].job_status[j].connect_status = old_fsa[i].job_status[j].connect_status;
                 new_fsa[i].job_status[j].no_of_files = old_fsa[i].job_status[j].no_of_files;
                 new_fsa[i].job_status[j].no_of_files_done = old_fsa[i].job_status[j].no_of_files_done;
                 new_fsa[i].job_status[j].file_size = old_fsa[i].job_status[j].file_size;
                 new_fsa[i].job_status[j].file_size_done = old_fsa[i].job_status[j].file_size_done;
                 new_fsa[i].job_status[j].bytes_send = old_fsa[i].job_status[j].bytes_send;
                 (void)strcpy(new_fsa[i].job_status[j].file_name_in_use, old_fsa[i].job_status[j].file_name_in_use);
                 new_fsa[i].job_status[j].file_size_in_use = old_fsa[i].job_status[j].file_size_in_use;
                 new_fsa[i].job_status[j].file_size_in_use_done = old_fsa[i].job_status[j].file_size_in_use_done;
              }
           }

           ptr = (char *)old_fsa;
           ptr -= AFD_WORD_OFFSET_3;

           /*
            * Resize the old FSA to the size of new one and then copy
            * the new structure into it. Then update the FSA version
            * number.
            */
           if ((ptr = mmap_resize(old_fsa_fd, ptr,
                                  new_size + AFD_WORD_OFFSET_4)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fsa_stat, strerror(errno));
              free((void *)new_fsa);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_4;
           (void)memcpy(ptr, new_fsa, new_size);
           free((void *)new_fsa);
           ptr -= AFD_WORD_OFFSET_4;
           *(ptr + SIZEOF_INT + 1) = old_features;
           *(ptr + SIZEOF_INT + 1 + 1) = ignore_first_errors;
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           *(int *)(ptr + SIZEOF_INT + 4) = pagesize;
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
           *old_fsa_size = new_size + AFD_WORD_OFFSET_4;

           system_log(INFO_SIGN, NULL, 0,
                      "Converted FSA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
        else
        {
           system_log(ERROR_SIGN, NULL, 0,
                      "Don't know how to convert a version %d FSA to version %d.",
                      old_version, new_version);
           ptr = NULL;
        }

   return(ptr);
}
