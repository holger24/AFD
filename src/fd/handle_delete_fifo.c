/*
 *  handle_delete_fifo.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   handle_delete_fifo - handles the deleting of jobs/files
 **
 ** SYNOPSIS
 **   void handle_delete_fifo(int    delete_jobs_fd,
 **                           size_t fifo_size,
 **                           char   *file_dir)
 **
 ** DESCRIPTION
 **   The function handle_delete_fifo handles the deleting of single files,
 **   all files from a certain host and all files belonging to a message/
 **   job. Data comes from a fifo and has a the following format:
 **
 **     DELETE_ALL_JOBS_FROM_HOST: <type><hast alias>\0
 **     DELETE_MESSAGE           : <type><message name>\0
 **     DELETE_SINGLE_FILE       : <type><message name + file name>\0
 **     DELETE_RETRIEVE          : <type><message number> <retrieve pos>\0
 **     DELETE_RETRIEVES_FROM_DIR: <type><dir alias>\0
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   09.01.2005 H.Kiehl Created
 **   11.06.2006 H.Kiehl Ability to remove retrieve jobs.
 **   29.09.2006 H.Kiehl Ability to remove all retrieves from a directory.
 **
 */
DESCR__E_M3

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>          /* waitpid()                              */
#include <fcntl.h>             /* open()                                 */
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include "fddefs.h"

/* External global variables. */
extern int                        fsa_fd,
                                  fra_fd,
                                  *no_msg_queued,
                                  no_of_dirs,
                                  no_of_hosts,
                                  no_of_trl_groups;
extern char                       *p_work_dir;
extern struct filetransfer_status *fsa;
extern struct fileretrieve_status *fra;
extern struct connection          *connection;
extern struct queue_buf           *qb;
extern struct msg_cache_buf       *mdb;
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif

/* Local function prototypes. */
static void                       remove_retrieve_job(int, int, time_t);


/*######################### handle_delete_fifo() ########################*/
void
handle_delete_fifo(int delete_jobs_fd, size_t fifo_size, char *file_dir)
{
   static int  del_bytes_buffered,
               del_bytes_read = 0;
   static char *del_fifo_buffer = NULL,
               *del_read_ptr = NULL;

   if (del_fifo_buffer == NULL)
   {
      if ((del_fifo_buffer = malloc(fifo_size)) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "malloc() error [%d bytes] : %s",
                    fifo_size, strerror(errno));
         exit(INCORRECT);
      }
   }

   if (del_bytes_read <= 0)
   {
      del_bytes_buffered = 0;
   }
   else
   {
      if ((del_read_ptr != del_fifo_buffer) && (del_fifo_buffer != NULL))
      {
         (void)memmove(del_fifo_buffer, del_read_ptr, del_bytes_read);
      }
      del_bytes_buffered = del_bytes_read;
   }
   del_read_ptr = del_fifo_buffer;

   if ((del_bytes_read = read(delete_jobs_fd,
                              &del_fifo_buffer[del_bytes_buffered],
                              (fifo_size - del_bytes_buffered))) > 0)
   {
      int    length;
      time_t now;
      char   *p_str_end;

      now = time(NULL);
      del_bytes_read += del_bytes_buffered;
      do
      {
         if ((*del_read_ptr == DELETE_ALL_JOBS_FROM_HOST) ||
             (*del_read_ptr == DELETE_MESSAGE) ||
             (*del_read_ptr == DELETE_SINGLE_FILE) ||
             (*del_read_ptr == DELETE_RETRIEVE) ||
             (*del_read_ptr == DELETE_RETRIEVES_FROM_DIR))
         {
            p_str_end = del_read_ptr + 1;
            while ((*p_str_end != '\0') &&
                   ((p_str_end - del_read_ptr) < (del_bytes_read - 1)))
            {
               p_str_end++;
            }
            if (*p_str_end == '\0')
            {
               p_str_end++;
               length = p_str_end - (del_read_ptr + 1);
            }
            else
            {
               length = 0;
            }
         }
         else
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Unknown identifier %d, deleting everything from fifo buffer.",
                       (int)*del_read_ptr);
            del_bytes_read = 0;
            length = 0;
         }
         if (length > 0)
         {
            int i;

            if (*del_read_ptr == DELETE_ALL_JOBS_FROM_HOST)
            {
               int  fsa_pos,
                    j;
               char *p_host_stored;

               del_read_ptr++;
               del_bytes_read--;
               for (i = 0; i < *no_msg_queued; i++)
               {
                  if ((qb[i].special_flag & FETCH_JOB) == 0)
                  {
                     p_host_stored = mdb[qb[i].pos].host_name;
                  }
                  else
                  {
                     p_host_stored = fra[qb[i].pos].host_alias;
                  }
                  if (CHECK_STRCMP(p_host_stored, del_read_ptr) == 0)
                  {
                     /*
                      * Kill the job when it is currently
                      * distributing data.
                      */
                     if (qb[i].pid > 0)
                     {
                        if (kill(qb[i].pid, SIGINT) < 0)
                        {
                           if (errno != ESRCH)
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                                         "Failed to kill transfer job to %s (%d) : %s",
#else
                                         "Failed to kill transfer job to %s (%lld) : %s",
#endif
                                         p_host_stored,
                                         (pri_pid_t)qb[i].pid, strerror(errno));
                           }
                        }
                        else
                        {
                           if (qb[i].pid > 0)
                           {
                              if (waitpid(qb[i].pid, NULL, 0) == -1)
                              {
                                 system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                                            "waitpid() error for %d : %s",
#else
                                            "waitpid() error for %lld : %s",
#endif
                                            (pri_pid_t)qb[i].pid,
                                            strerror(errno));
                              }
                           }
                           remove_connection(&connection[qb[i].connect_pos],
                                             NO, now);
                        }
                     }

                     if ((qb[i].special_flag & FETCH_JOB) == 0)
                     {
                        char *ptr;

#ifdef WITH_ERROR_QUEUE
                        if ((mdb[qb[i].pos].fsa_pos > -1) &&
                            (fsa[mdb[qb[i].pos].fsa_pos].host_status & ERROR_QUEUE_SET))
                        {
                           (void)remove_from_error_queue(mdb[qb[i].pos].job_id,
                                                         &fsa[mdb[qb[i].pos].fsa_pos],
                                                         mdb[qb[i].pos].fsa_pos,
                                                         fsa_fd);
                        }
#endif

                        ptr = file_dir + strlen(file_dir);
                        (void)strcpy(ptr, qb[i].msg_name);
#ifdef _DELETE_LOG
                        extract_cus(qb[i].msg_name, dl.input_time,
                                    dl.split_job_counter, dl.unique_number);
                        remove_job_files(file_dir, mdb[qb[i].pos].fsa_pos,
                                         mdb[qb[i].pos].job_id, FD, USER_DEL,
                                         -1, __FILE__, __LINE__);
#else
                        remove_job_files(file_dir, -1, -1, __FILE__, __LINE__);
#endif
                        *ptr = '\0';
                        fsa_pos = mdb[qb[i].pos].fsa_pos;
                     }
                     else
                     {
                        fsa_pos = fra[qb[i].pos].fsa_pos;
#ifdef WITH_ERROR_QUEUE
                        if ((fsa_pos > -1) &&
                            (fsa[fsa_pos].host_status & ERROR_QUEUE_SET))
                        {
                           (void)remove_from_error_queue(fra[qb[i].pos].dir_id,
                                                         &fsa[fsa_pos],
                                                         fsa_pos,
                                                         fsa_fd);
                        }
#endif
                     }

                     if (qb[i].pid < 1)
                     {
                        ABS_REDUCE(fsa_pos);
                     }
#if defined (_RMQUEUE_) && defined (_MAINTAINER_LOG)
                     remove_msg(i, NO, "handle_delete_fifo.c", __LINE__);
#else
                     remove_msg(i, NO);
#endif
                     if (i < *no_msg_queued)
                     {
                        i--;
                     }
                  }
               } /* for (i = 0; i < *no_msg_queued; i++) */

               /*
                * Hmmm. Best is to reset ALL values, so we do not
                * need to start and stop the FD to sort out any
                * problems in the FSA.
                */
               if ((fsa_pos = get_host_position(fsa, del_read_ptr,
                                                no_of_hosts)) != INCORRECT)
               {
                  fsa[fsa_pos].total_file_counter = 0;
                  fsa[fsa_pos].total_file_size = 0;
                  fsa[fsa_pos].active_transfers = 0;
                  if ((fsa[fsa_pos].transfer_rate_limit > 0) ||
                      (no_of_trl_groups > 0))
                  {
                     calc_trl_per_process(fsa_pos);
                  }
                  fsa[fsa_pos].error_counter = 0;
                  fsa[fsa_pos].jobs_queued = 0;
                  for (j = 0; j < MAX_NO_PARALLEL_JOBS; j++)
                  {
                     fsa[fsa_pos].job_status[j].no_of_files = 0;
                     fsa[fsa_pos].job_status[j].proc_id = -1;
                     fsa[fsa_pos].job_status[j].connect_status = DISCONNECT;
                     fsa[fsa_pos].job_status[j].file_name_in_use[0] = '\0';
                     fsa[fsa_pos].job_status[j].file_name_in_use[1] = 0;
                  }
                  for (j = 0; j < ERROR_HISTORY_LENGTH; j++)
                  {
                     fsa[fsa_pos].error_history[j] = 0;
                  }
               }
            }
            else if (*del_read_ptr == DELETE_MESSAGE)
                 {
                    del_read_ptr++;
                    del_bytes_read--;
                    for (i = 0; i < *no_msg_queued; i++)
                    {
                       if (CHECK_STRCMP(qb[i].msg_name, del_read_ptr) == 0)
                       {
                          char *ptr;

                          /*
                           * Kill the job when it is currently
                           * distributing data.
                           */
                          if (qb[i].pid > 0)
                          {
                             if (kill(qb[i].pid, SIGINT) < 0)
                             {
                                if (errno != ESRCH)
                                {
                                   system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                                              "Failed to kill transfer job to %s (%d) : %s",
#else
                                              "Failed to kill transfer job to %s (%lld) : %s",
#endif
                                              mdb[qb[i].pos].host_name,
                                              (pri_pid_t)qb[i].pid,
                                              strerror(errno));
                                }
                             }
                             else
                             {
                                if (qb[i].pid > 0)
                                {
                                   if (waitpid(qb[i].pid, NULL, 0) == -1)
                                   {
                                      system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                                                 "waitpid() error for %d : %s",
#else
                                                 "waitpid() error for %lld : %s",
#endif
                                                 (pri_pid_t)qb[i].pid,
                                                 strerror(errno));
                                   }
                                }
                                remove_connection(&connection[qb[i].connect_pos],
                                                  NO, now);
                             }
                          }

#ifdef WITH_ERROR_QUEUE
                          if ((mdb[qb[i].pos].fsa_pos > -1) &&
                              (fsa[mdb[qb[i].pos].fsa_pos].host_status & ERROR_QUEUE_SET))
                          {
                             (void)remove_from_error_queue(mdb[qb[i].pos].job_id,
                                                           &fsa[mdb[qb[i].pos].fsa_pos],
                                                           mdb[qb[i].pos].fsa_pos,
                                                           fsa_fd);
                          }
#endif
                          ptr = file_dir + strlen(file_dir);
                          (void)strcpy(ptr, qb[i].msg_name);
#ifdef _DELETE_LOG
                          extract_cus(qb[i].msg_name, dl.input_time,
                                      dl.split_job_counter, dl.unique_number);
                          remove_job_files(file_dir, mdb[qb[i].pos].fsa_pos,
                                           mdb[qb[i].pos].job_id, FD, USER_DEL,
                                           -1, __FILE__, __LINE__);
#else
                          remove_job_files(file_dir, -1, -1, __FILE__, __LINE__);
#endif
                          *ptr = '\0';

                          if (qb[i].pid < 1)
                          {
                             ABS_REDUCE(mdb[qb[i].pos].fsa_pos);
                          }
#if defined (_RMQUEUE_) && defined (_MAINTAINER_LOG)
                          remove_msg(i, NO, "handle_delete_fifo.c", __LINE__);
#else
                          remove_msg(i, NO);
#endif
                          break;
                       }
                    } /* for (i = 0; i < *no_msg_queued; i++) */
                 }
            else if (*del_read_ptr == DELETE_SINGLE_FILE)
                 {
#ifdef _DELETE_LOG
                    unsigned int   split_job_counter,
                                   unique_number;
                    time_t         input_time;
                    char           *p_end,
                                   *p_start;
#else
                    char           *p_end;
#endif

                    del_read_ptr++;
                    del_bytes_read--;
                    p_end = del_read_ptr;
                    while ((*p_end != '/') && (*p_end != '\0'))
                    {
                       p_end++; /* Away with job ID. */
                    }
                    if (*p_end == '/')
                    {
                       p_end++;
                       while ((*p_end != '/') && (*p_end != '\0'))
                       {
                          p_end++; /* Away with the dir number. */
                       }
                       if (*p_end == '/')
                       {
                          p_end++;
#ifdef _DELETE_LOG
                          p_start = p_end;
#endif
                          while ((*p_end != '_') && (*p_end != '\0'))
                          {
                             p_end++; /* Away with date. */
                          }
                          if (*p_end == '_')
                          {
                             char tmp_char;

#ifdef _DELETE_LOG
                             tmp_char = *p_end;
                             *p_end = '\0';
                             input_time = (time_t)str2timet(p_start, NULL, 16);
                             *p_end = tmp_char;
                             p_start = p_end + 1;
#endif
                             p_end++;
                             while ((*p_end != '_') && (*p_end != '\0'))
                             {
                                p_end++; /* Away with unique number. */
                             }
                             if (*p_end == '_')
                             {
#ifdef _DELETE_LOG
                                tmp_char = *p_end;
                                *p_end = '\0';
                                unique_number = (unsigned int)strtoul(p_start, NULL, 16);
                                *p_end = tmp_char;
                                p_start = p_end + 1;
#endif
                                p_end++;
                                while ((*p_end != '/') && (*p_end != '\0'))
                                {
                                   p_end++; /* Away with split job counter. */
                                }
                                if (*p_end == '/')
                                {
                                   tmp_char = *p_end;
                                   *p_end = '\0';
#ifdef _DELETE_LOG
                                   split_job_counter = (unsigned int)strtoul(p_start, NULL, 16);
#endif
                                   for (i = 0; i < *no_msg_queued; i++)
                                   {
                                      if (CHECK_STRCMP(qb[i].msg_name, del_read_ptr) == 0)
                                      {
                                         /*
                                          * Do not delete when file
                                          * is currently being processed.
                                          */
                                         if (qb[i].pid == PENDING)
                                         {
                                            char         *ptr;
#ifdef HAVE_STATX
                                            struct statx stat_buf;
#else
                                            struct stat  stat_buf;
#endif

                                            *p_end = tmp_char;
                                            ptr = file_dir +
                                                  strlen(file_dir);
                                            (void)strcpy(ptr, del_read_ptr);
#ifdef HAVE_STATX
                                            if (statx(0, file_dir,
                                                      AT_STATX_SYNC_AS_STAT,
                                                      STATX_SIZE,
                                                      &stat_buf) != -1)
#else
                                            if (stat(file_dir, &stat_buf) != -1)
#endif
                                            {
                                               if (unlink(file_dir) == -1)
                                               {
                                                  if (errno != ENOENT)
                                                  {
                                                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                                "Failed to unlink() `%s' : %s",
                                                                file_dir, strerror(errno));
                                                  }
                                               }
                                               else
                                               {
#ifdef _DELETE_LOG
                                                  size_t dl_real_size;
                                                  off_t  lock_offset;
#endif

                                                  qb[i].files_to_send--;
#ifdef HAVE_STATX
                                                  qb[i].file_size_to_send -= stat_buf.stx_size;
#else
                                                  qb[i].file_size_to_send -= stat_buf.st_size;
#endif
#ifdef _DELETE_LOG
                                                  (void)strcpy(dl.file_name, p_end + 1);
                                                  if (mdb[qb[i].pos].fsa_pos > -1)
                                                  {
                                                     lock_offset = AFD_WORD_OFFSET +
                                                                   (mdb[qb[i].pos].fsa_pos * sizeof(struct filetransfer_status));
# ifdef LOCK_DEBUG
                                                     lock_region_w(fsa_fd, lock_offset + LOCK_TFC, __FILE__, __LINE__);
# else
                                                     lock_region_w(fsa_fd, lock_offset + LOCK_TFC);
# endif
                                                     fsa[mdb[qb[i].pos].fsa_pos].total_file_counter -= 1;
# ifdef _VERIFY_FSA
                                                     if (fsa[mdb[qb[i].pos].fsa_pos].total_file_counter < 0)
                                                     {
                                                        system_log(INFO_SIGN, __FILE__, __LINE__,
                                                                   "Total file counter for host `%s' less then zero. Correcting.",
                                                                   fsa[mdb[qb[i].pos].fsa_pos].host_dsp_name);
                                                        fsa[mdb[qb[i].pos].fsa_pos].total_file_counter = 0;
                                                     }
# endif
# ifdef HAVE_STATX
                                                     fsa[mdb[qb[i].pos].fsa_pos].total_file_size -= stat_buf.stx_size;
# else
                                                     fsa[mdb[qb[i].pos].fsa_pos].total_file_size -= stat_buf.st_size;
# endif
# ifdef _VERIFY_FSA
                                                     if (fsa[mdb[qb[i].pos].fsa_pos].total_file_size < 0)
                                                     {
                                                        system_log(INFO_SIGN, __FILE__, __LINE__,
                                                                   "Total file size for host `%s' overflowed. Correcting.",
                                                                   fsa[mdb[qb[i].pos].fsa_pos].host_dsp_name);
                                                        fsa[mdb[qb[i].pos].fsa_pos].total_file_size = 0;
                                                     }
                                                     else if ((fsa[mdb[qb[i].pos].fsa_pos].total_file_counter == 0) &&
                                                              (fsa[mdb[qb[i].pos].fsa_pos].total_file_size > 0))
                                                          {
                                                             system_log(INFO_SIGN, __FILE__, __LINE__,
                                                                        "fc for host `%s' is zero but fs is not zero. Correcting.",
                                                                        fsa[mdb[qb[i].pos].fsa_pos].host_dsp_name);
                                                             fsa[mdb[qb[i].pos].fsa_pos].total_file_size = 0;
                                                          }
# endif
# ifdef LOCK_DEBUG
                                                     unlock_region(fsa_fd, lock_offset + LOCK_TFC, __FILE__, __LINE__);
# else
                                                     unlock_region(fsa_fd, lock_offset + LOCK_TFC);
# endif

                                                     if ((fsa[mdb[qb[i].pos].fsa_pos].error_counter > 0) &&
                                                         (fsa[mdb[qb[i].pos].fsa_pos].total_file_counter == 0))
                                                     {
# ifdef LOCK_DEBUG
                                                        lock_region_w(fsa_fd, lock_offset + LOCK_EC, __FILE__, __LINE__);
# else
                                                        lock_region_w(fsa_fd, lock_offset + LOCK_EC);
# endif
                                                        fsa[mdb[qb[i].pos].fsa_pos].error_counter = 0;
                                                        fsa[mdb[qb[i].pos].fsa_pos].error_history[0] = 0;
                                                        fsa[mdb[qb[i].pos].fsa_pos].error_history[1] = 0;
# ifdef LOCK_DEBUG
                                                        unlock_region(fsa_fd, lock_offset + LOCK_EC, __FILE__, __LINE__);
# else
                                                        unlock_region(fsa_fd, lock_offset + LOCK_EC);
# endif
                                                     }

                                                     (void)snprintf(dl.host_name,
                                                                    MAX_HOSTNAME_LENGTH + 4 + 1,
                                                                    "%-*s %03x",
                                                                    MAX_HOSTNAME_LENGTH,
                                                                    fsa[mdb[qb[i].pos].fsa_pos].host_alias,
                                                                    USER_DEL);
                                                  }
                                                  else
                                                  {
                                                     (void)snprintf(dl.host_name,
                                                                    MAX_HOSTNAME_LENGTH + 4 + 1,
                                                                    "%-*s %03x",
                                                                    MAX_HOSTNAME_LENGTH,
                                                                    "-",
                                                                    USER_DEL);
                                                  }
# ifdef HAVE_STATX
                                                  *dl.file_size = stat_buf.stx_size;
# else
                                                  *dl.file_size = stat_buf.st_size;
# endif
                                                  *dl.job_id = mdb[qb[i].pos].job_id;
                                                  *dl.dir_id = 0;
                                                  *dl.input_time = input_time;
                                                  *dl.split_job_counter = split_job_counter;
                                                  *dl.unique_number = unique_number;
                                                  *dl.file_name_length = strlen(p_end + 1);
                                                  dl_real_size = *dl.file_name_length +
                                                                 dl.size +
                                                                 snprintf((dl.file_name + *dl.file_name_length + 1),
                                                                          MAX_FILENAME_LENGTH + 1,
                                                                          "%s%c(%s %d)",
                                                                          FD, SEPARATOR_CHAR,
                                                                          __FILE__, __LINE__);
                                                  if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                                                  {
                                                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                                "write() error : %s", strerror(errno));
                                                  }
#endif
                                                  if (qb[i].files_to_send == 0)
                                                  {
#ifdef WITH_ERROR_QUEUE
                                                     if ((mdb[qb[i].pos].fsa_pos > -1) &&
                                                         (fsa[mdb[qb[i].pos].fsa_pos].host_status & ERROR_QUEUE_SET))
                                                     {
                                                        (void)remove_from_error_queue(mdb[qb[i].pos].job_id,
                                                                                      &fsa[mdb[qb[i].pos].fsa_pos],
                                                                                      mdb[qb[i].pos].fsa_pos,
                                                                                      fsa_fd);
                                                     }
#endif
                                                     ABS_REDUCE(mdb[qb[i].pos].fsa_pos);
                                                     if (i != (*no_msg_queued - 1))
                                                     {
                                                        size_t move_size;

                                                        move_size = (*no_msg_queued - (i + 1)) * sizeof(struct queue_buf);
                                                        (void)memmove(&qb[i],
                                                                      &qb[i + 1],
                                                                      move_size);
                                                     }
                                                     (*no_msg_queued)--;
                                                  }
                                               }
                                            }
                                            else
                                            {
                                               if (errno != ENOENT)
                                               {
                                                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                             "Failed to stat() `%s' : %s",
                                                             file_dir, strerror(errno));
                                               }
                                            }
                                            *ptr = '\0';
                                         }
                                         break;
                                      }
                                   } /* for (i = 0; i < *no_msg_queued; i++) */
                                }
                                else
                                {
                                   system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                              "Reading garbage on FD delete fifo (%c).",
                                              *p_end);
                                }
                             }
                             else
                             {
                                system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                           "Reading garbage on FD delete fifo (%c).",
                                           *p_end);
                             }
                          }
                          else
                          {
                             system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                        "Reading garbage on FD delete fifo (%c).",
                                        *p_end);
                          }
                       }
                       else
                       {
                          system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                     "Reading garbage on FD delete fifo (%c).",
                                     *p_end);
                       }
                    }
                    else
                    {
                       system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                  "Reading garbage on FD delete fifo (%c).",
                                  *p_end);
                    }
                 }
            else if (*del_read_ptr == DELETE_RETRIEVE)
                 {
                    char *p_end;

                    del_read_ptr++;
                    del_bytes_read--;
                    p_end = del_read_ptr;
                    while ((*p_end != ' ') && (*p_end != '\0'))
                    {
                       p_end++;
                    }
                    if (*p_end == ' ')
                    {
                       double msg_number;

                       *p_end = '\0';
                       p_end++;
                       msg_number = strtod(del_read_ptr, NULL);
                       if (*p_end != '\0')
                       {
                          int fra_pos;

                          fra_pos = atoi(p_end);
                          for (i = 0; i < *no_msg_queued; i++)
                          {
                             if ((qb[i].msg_number == msg_number) &&
                                 (qb[i].pos == fra_pos) &&
                                 (qb[i].special_flag & FETCH_JOB))
                             {
                                remove_retrieve_job(i, fra_pos, now);
                                break;
                             }
                          }
                       }
                       else
                       {
                          system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                     "Reading garbage on FD delete fifo (unexpected 0).");
                       }
                       *(p_end - 1) = ' ';
                    }
                    else
                    {
                       system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                  "Reading garbage on FD delete fifo (%c).",
                                  *p_end);
                    }
                 }
            else if (*del_read_ptr == DELETE_RETRIEVES_FROM_DIR)
                 {
                    int fra_pos = INCORRECT;

                    del_read_ptr++;
                    del_bytes_read--;

                    for (i = 0; i < no_of_dirs; i++)
                    {
                       if (CHECK_STRCMP(fra[i].dir_alias, del_read_ptr) == 0)
                       {
                          fra_pos = i;
                          break;
                       }
                    }

                    if (fra_pos == INCORRECT)
                    {
                       system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                  "Failed to locate `%s' in FRA.",
                                  del_read_ptr);
                    }
                    else
                    {
                       for (i = 0; i < *no_msg_queued; i++)
                       {
                          if ((qb[i].special_flag & FETCH_JOB) &&
                              (qb[i].pos == fra_pos))
                          {
                             remove_retrieve_job(i, fra_pos, now);
                             i--;
                          }
                       }
                    }
                 }
                 else
                 {
                    system_log(DEBUG_SIGN, __FILE__, __LINE__,
                               "Unknown identifier %d, deleting everything from fifo buffer.",
                               (int)*del_read_ptr);
                    del_bytes_read = 0;
                 }
            del_bytes_read -= length;
            del_read_ptr = p_str_end;
         } /* if (length > 0) */
      } while ((del_bytes_read > 0) && (length != 0));
   }
   else if (del_bytes_read == -1)
        {
           system_log(ERROR_SIGN, __FILE__, __LINE__,
                      "read() error : %s", strerror(errno));
           del_bytes_read = 0;
        }

   return;
}


/*++++++++++++++++++++++++ remove_retrieve_job() ++++++++++++++++++++++++*/
static void
remove_retrieve_job(int pos, int fra_pos, time_t now)
{
   /*
    * Kill the job when it is currently
    * retrieving data.
    */
   if (qb[pos].pid > 0)
   {
      if (kill(qb[pos].pid, SIGINT) < 0)
      {
         if (errno != ESRCH)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                       "Failed to kill transfer job to %s (%d) : %s",
#else
                       "Failed to kill transfer job to %s (%lld) : %s",
#endif
                       mdb[qb[pos].pos].host_name,
                       (pri_pid_t)qb[pos].pid, strerror(errno));
         }
      }
      else
      {
         off_t lock_offset;

         lock_offset = AFD_WORD_OFFSET +
                       (connection[qb[pos].connect_pos].fsa_pos * sizeof(struct filetransfer_status));

#ifdef LOCK_DEBUG
         lock_region_w(fsa_fd, lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
         lock_region_w(fsa_fd, lock_offset + LOCK_TFC);
#endif
         fsa[connection[qb[pos].connect_pos].fsa_pos].total_file_counter -= fsa[connection[qb[pos].connect_pos].fsa_pos].job_status[connection[qb[pos].connect_pos].job_no].no_of_files - fsa[connection[qb[pos].connect_pos].fsa_pos].job_status[connection[qb[pos].connect_pos].job_no].no_of_files_done;
#ifdef _VERIFY_FSA
         if (fsa[connection[qb[pos].connect_pos].fsa_pos].total_file_counter < 0)
         {
            system_log(INFO_SIGN, __FILE__, __LINE__,
                       "Total file counter for host `%s' less then zero. Correcting.",
                       fsa[connection[qb[pos].connect_pos].fsa_pos].host_dsp_name);
            fsa[connection[qb[pos].connect_pos].fsa_pos].total_file_counter = 0;
         }
#endif

         fsa[connection[qb[pos].connect_pos].fsa_pos].total_file_size -= (fsa[connection[qb[pos].connect_pos].fsa_pos].job_status[connection[qb[pos].connect_pos].job_no].file_size - fsa[connection[qb[pos].connect_pos].fsa_pos].job_status[connection[qb[pos].connect_pos].job_no].file_size_done + fsa[connection[qb[pos].connect_pos].fsa_pos].job_status[connection[qb[pos].connect_pos].job_no].file_size_in_use_done);
#ifdef _VERIFY_FSA
         if (fsa[connection[qb[pos].connect_pos].fsa_pos].total_file_size < 0)
         {
            system_log(INFO_SIGN, __FILE__, __LINE__,
                       "Total file size for host `%s' overflowed. Correcting.",
                       fsa[connection[qb[pos].connect_pos].fsa_pos].host_dsp_name);
            fsa[connection[qb[pos].connect_pos].fsa_pos].total_file_size = 0;
         }
         else
         {
            if ((fsa[connection[qb[pos].connect_pos].fsa_pos].total_file_counter == 0) &&
                (fsa[connection[qb[pos].connect_pos].fsa_pos].total_file_size < 0))
            {
               system_log(INFO_SIGN, __FILE__, __LINE__,
                          "fc for host `%s' is zero but fs is not zero. Correcting.",
                          fsa[connection[qb[pos].connect_pos].fsa_pos].host_dsp_name);
               fsa[connection[qb[pos].connect_pos].fsa_pos].total_file_size = 0;
            }
         }
#endif

#ifdef LOCK_DEBUG
         unlock_region(fsa_fd, lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
         unlock_region(fsa_fd, lock_offset + LOCK_TFC);
#endif
         fsa[connection[qb[pos].connect_pos].fsa_pos].job_status[connection[qb[pos].connect_pos].job_no].connect_status = DISCONNECT;
         fsa[connection[qb[pos].connect_pos].fsa_pos].job_status[connection[qb[pos].connect_pos].job_no].no_of_files = 0;
         fsa[connection[qb[pos].connect_pos].fsa_pos].job_status[connection[qb[pos].connect_pos].job_no].no_of_files_done = 0;
         fsa[connection[qb[pos].connect_pos].fsa_pos].job_status[connection[qb[pos].connect_pos].job_no].file_size = 0;
         fsa[connection[qb[pos].connect_pos].fsa_pos].job_status[connection[qb[pos].connect_pos].job_no].file_size_done = 0;
         fsa[connection[qb[pos].connect_pos].fsa_pos].job_status[connection[qb[pos].connect_pos].job_no].file_size_in_use = 0;
         fsa[connection[qb[pos].connect_pos].fsa_pos].job_status[connection[qb[pos].connect_pos].job_no].file_size_in_use_done = 0;
         fsa[connection[qb[pos].connect_pos].fsa_pos].job_status[connection[qb[pos].connect_pos].job_no].file_name_in_use[0] = '\0';
         fsa[connection[qb[pos].connect_pos].fsa_pos].job_status[connection[qb[pos].connect_pos].job_no].file_name_in_use[1] = 0;
         if (qb[pos].pid > 0)
         {
            if (waitpid(qb[pos].pid, NULL, 0) == -1)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                          "waitpid() error for %d : %s",
#else
                          "waitpid() error for %lld : %s",
#endif
                          (pri_pid_t)qb[pos].pid, strerror(errno));
            }
         }
         remove_connection(&connection[qb[pos].connect_pos], NO, now);
      }
   }
   else
   {
      int fsa_pos;

      if ((fsa_pos = get_host_position(fsa, fra[fra_pos].host_alias,
                                       no_of_hosts)) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to locate `%s' in FSA.", fra[fra_pos].host_alias);
      }
      else
      {
         ABS_REDUCE(fsa_pos);
         if ((fsa[fsa_pos].jobs_queued == 0) &&
             (fsa[fsa_pos].error_counter > 0))
         {
            int   j;
            off_t lock_offset;

            lock_offset = AFD_WORD_OFFSET +
                          (fsa_pos * sizeof(struct filetransfer_status));
#ifdef LOCK_DEBUG
            lock_region_w(fsa_fd, lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
            lock_region_w(fsa_fd, lock_offset + LOCK_EC);
#endif
            fsa[fsa_pos].error_counter = 0;

            /*
             * Remove the error condition (NOT_WORKING) from
             * all jobs of this host.
             */
            for (j = 0; j < fsa[fsa_pos].allowed_transfers; j++)
            {
               if (fsa[fsa_pos].job_status[j].connect_status == NOT_WORKING)
               {
                  fsa[fsa_pos].job_status[j].connect_status = DISCONNECT;
               }
            }
            fsa[fsa_pos].error_history[0] = 0;
            fsa[fsa_pos].error_history[1] = 0;
#ifdef LOCK_DEBUG
            unlock_region(fsa_fd, lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
            unlock_region(fsa_fd, lock_offset + LOCK_EC);
#endif
         }
      }
   }

   lock_region_w(fra_fd,
#ifdef LOCK_DEBUG
                 (char *)&fra[fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                 (char *)&fra[fra_pos].error_counter - (char *)fra);
#endif
   fra[fra_pos].error_counter = 0;
   if (fra[fra_pos].dir_flag & DIR_ERROR_SET)
   {
      int  receive_log_fd = -1;
#ifdef WITHOUT_FIFO_RW_SUPPORT
      int  receive_log_readfd;
#endif
      char receive_log_fifo[MAX_PATH_LENGTH];

      (void)strcpy(receive_log_fifo, p_work_dir);
      (void)strcat(receive_log_fifo, FIFO_DIR);
      (void)strcat(receive_log_fifo, RECEIVE_LOG_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(receive_log_fifo, &receive_log_readfd,
                       &receive_log_fd) == -1)
#else
      if ((receive_log_fd = open(receive_log_fifo, O_RDWR)) == -1)
#endif
      {
         if (errno == ENOENT)
         {
            if ((make_fifo(receive_log_fifo) == SUCCESS) &&
#ifdef WITHOUT_FIFO_RW_SUPPORT
                (open_fifo_rw(receive_log_fifo, &receive_log_readfd,
                              &receive_log_fd) == -1))
#else
                ((receive_log_fd = open(receive_log_fifo, O_RDWR)) == -1))
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not open fifo <%s> : %s",
                          RECEIVE_LOG_FIFO, strerror(errno));
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not open fifo %s : %s",
                       RECEIVE_LOG_FIFO, strerror(errno));
         }
      }

      fra[fra_pos].dir_flag &= ~DIR_ERROR_SET;
      SET_DIR_STATUS(fra[fra_pos].dir_flag,
                     now,
                     fra[fra_pos].start_event_handle,
                     fra[fra_pos].end_event_handle,
                     fra[fra_pos].dir_status);
      error_action(fra[fra_pos].dir_alias, "stop", DIR_ERROR_ACTION,
                   receive_log_fd);
      event_log(now, EC_DIR, ET_EXT, EA_ERROR_END, "%s", fra[fra_pos].dir_alias);
      (void)close(receive_log_fd);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      (void)close(receive_log_readfd);
#endif
   }
   unlock_region(fra_fd,
#ifdef LOCK_DEBUG
                 (char *)&fra[fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                 (char *)&fra[fra_pos].error_counter - (char *)fra);
#endif

   /*
    * NOTE: We must calculate the next check
    *       time, otherwise the job will popup
    *       again, although it was deleted.
    */
   if ((fra[fra_pos].no_of_time_entries > 0) &&
       (fra[fra_pos].next_check_time <= now))
   {
      fra[fra_pos].next_check_time = calc_next_time_array(fra[fra_pos].no_of_time_entries,
                                                          &fra[fra_pos].te[0],
#ifdef WITH_TIMEZONE
                                                          NULL,
#endif
                                                          now, __FILE__,
                                                          __LINE__);
   }

   fra[fra_pos].queued -= 1;
   if (fra[fra_pos].queued < 0)
   {
      fra[fra_pos].queued = 0;
   }
   if (pos != (*no_msg_queued - 1))
   {
      size_t move_size;

      move_size = (*no_msg_queued - (pos + 1)) * sizeof(struct queue_buf);
      (void)memmove(&qb[pos], &qb[pos + 1], move_size);
   }
   (*no_msg_queued)--;

   return;
}
