/*
 *  init_msg_buffer.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2024 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   init_msg_buffer - initialises the structures queue_buf and
 **                     msg_cache_buf
 **
 ** SYNOPSIS
 **   void init_msg_buffer(void)
 **
 ** DESCRIPTION
 **   The function init_msg_buffer() initialises the structures
 **   queue_buf and msg_cache_buf, and removes any old messages
 **   from them. In addition any old job ID's, password and file mask
 **   entries will be removed. Also any old message not in both buffers
 **   and older then the oldest output log file are removed.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   20.01.1998 H.Kiehl Created
 **   06.05.2003 H.Kiehl Also remove old passwords from password database.
 **   30.12.2003 H.Kiehl Remove old file mask entries.
 **   17.10.2004 H.Kiehl Remove outgoing job directory.
 **   10.04.2005 H.Kiehl Remove alternate file.
 **   22.06.2006 H.Kiehl Catch the case when *no_of_file_mask_ids is larger
 **                      then the actual number stored.
 **   22.04.2008 H.Kiehl Let function url_evaluate() handle the url.
 **   25.11.2010 H.Kiehl If fd starts structure connection does not yet
 **                      exists, handle this case.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <unistd.h>     /* lseek(), write(), unlink()                    */
#include <string.h>     /* strcpy(), strcat(), strerror()                */
#include <time.h>       /* time()                                        */
#include <stdlib.h>     /* malloc(), free()                              */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>  /* mmap(), msync(), munmap()                     */
#endif
#include <sys/wait.h>   /* waitpid()                                     */
#include <signal.h>     /* kill(), SIGKILL                               */
#include <fcntl.h>
#include <dirent.h>     /* opendir(), closedir(), readdir(), DIR         */
#include <errno.h>
#include "fddefs.h"

#define REMOVE_STEP_SIZE 50
/* #define SHOW_MSG_CACHE */


/* External global variables. */
extern int                        mdb_fd,
                                  qb_fd,
#ifdef _OUTPUT_LOG
                                  max_output_log_files,
#endif
                                  *no_msg_cached,
                                  *no_msg_queued,
                                  no_of_hosts;
extern char                       file_dir[],
                                  msg_dir[],
                                  *p_file_dir,
                                  *p_msg_dir,
                                  *p_work_dir;
#ifdef SF_BURST_ACK
extern int                        ab_fd,
                                  *no_of_acks_queued;
extern struct ack_queue_buf       *ab;
#endif
extern struct queue_buf           *qb;
extern struct msg_cache_buf       *mdb;
extern struct afd_status          *p_afd_status;
extern struct connection          *connection;
extern struct filetransfer_status *fsa;
extern struct fileretrieve_status *fra;
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif

/* Local global variables. */
static int                        file_mask_to_remove,
                                  *no_of_job_ids,
                                  removed_jobs,
                                  removed_messages;
static unsigned int               *fmtrl, /* File mask to remove list. */
                                  *rjl,   /* Removed job list. */
                                  *rml;   /* Removed message list. */
static struct job_id_data         *jd;

/* Local function prototypes. */
static void                       remove_jobs(int, off_t *, char *),
                                  sort_array(int);
static int                        list_job_to_remove(int, int,
                                                     struct job_id_data *,
                                                     unsigned int);


/*########################## init_msg_buffer() ##########################*/
void
init_msg_buffer(void)
{
   register int i;
   unsigned int *cml;            /* Current message list array. */
   int          jd_fd,
                sleep_counter = 0,
                no_of_current_msg;
#ifdef _OUTPUT_LOG
   time_t       current_time;
#endif
   off_t        jid_struct_size;
   char         *ptr,
                job_id_data_file[MAX_PATH_LENGTH];
#if defined (_MAINTAINER_LOG) && defined (SHOW_MSG_CACHE)
   char         tbuf1[20],
                tbuf2[20];
#endif
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif
   DIR          *dp;

   removed_jobs = removed_messages = file_mask_to_remove = 0;
   rml = NULL;
   rjl = NULL;
   fmtrl = NULL;

   /* If necessary attach to the buffers. */
   if (mdb_fd == -1)
   {
      if (mdb_attach() != SUCCESS)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to attach to MDB.");
         exit(INCORRECT);
      }
   }

   if (qb_fd == -1)
   {
      size_t new_size = (MSG_QUE_BUF_SIZE * sizeof(struct queue_buf)) +
                        AFD_WORD_OFFSET;
      char   fullname[MAX_PATH_LENGTH];

      (void)snprintf(fullname, MAX_PATH_LENGTH, "%s%s%s",
                     p_work_dir, FIFO_DIR, MSG_QUEUE_FILE);
      if ((ptr = attach_buf(fullname, &qb_fd, &new_size, "FD",
                            FILE_MODE, NO)) == (caddr_t) -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to mmap() `%s' : %s", fullname, strerror(errno));
         exit(INCORRECT);
      }
      no_msg_queued = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      qb = (struct queue_buf *)ptr;
   }

#ifdef SF_BURST_ACK
   if (ab_fd == -1)
   {
      size_t new_size = (ACK_QUE_BUF_SIZE * sizeof(struct ack_queue_buf)) +
                        AFD_WORD_OFFSET;
      char   fullname[MAX_PATH_LENGTH];

      (void)snprintf(fullname, MAX_PATH_LENGTH, "%s%s%s",
                     p_work_dir, FIFO_DIR, ACK_QUEUE_FILE);
      if ((ptr = attach_buf(fullname, &ab_fd, &new_size, "FD",
                            FILE_MODE, NO)) == (caddr_t) -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to mmap() `%s' : %s", fullname, strerror(errno));
         exit(INCORRECT);
      }
      no_of_acks_queued = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      ab = (struct ack_queue_buf *)ptr;
   }
#endif

#if defined (_MAINTAINER_LOG) && defined (SHOW_MSG_CACHE)
   maintainer_log(DEBUG_SIGN, __FILE__, __LINE__,
                  "%s with %d elements before any modifications.",
                  MSG_CACHE_FILE, *no_msg_cached);
   for (i = 0; i < *no_msg_cached; i++)
   {
      (void)strftime(tbuf1, 20, "%d.%m.%Y %H:%M:%S", localtime(&mdb[i].msg_time));
      (void)strftime(tbuf2, 20, "%d.%m.%Y %H:%M:%S", localtime(&mdb[i].last_transfer_time));
      maintainer_log(INFO_SIGN, __FILE__, __LINE__,
                     "%5d: %-*s %s %s %d %d %x %u %d %s",
                     i, MAX_HOSTNAME_LENGTH, mdb[i].host_name, tbuf1, tbuf2,
                     mdb[i].fsa_pos, mdb[i].port, mdb[i].job_id,
                     mdb[i].age_limit, (int)mdb[i].type,
                     (mdb[i].in_current_fsa == YES) ? "Yes" : "No");
   }
#endif

   /* Attach to job_id_data structure, so we can remove any old data. */
   (void)snprintf(job_id_data_file, MAX_PATH_LENGTH, "%s%s%s",
                  p_work_dir, FIFO_DIR, JOB_ID_DATA_FILE);
stat_again:
   if ((jd_fd = coe_open(job_id_data_file, O_RDWR)) == -1)
   {
      if (errno == ENOENT)
      {
         do
         {
            (void)my_usleep(100000L);
            errno = 0;
            sleep_counter++;
            if (((jd_fd = coe_open(job_id_data_file, O_RDWR)) == -1) &&
                ((errno != ENOENT) || (sleep_counter > 100)))
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "Failed to open() `%s' : %s",
                          job_id_data_file, strerror(errno));
               exit(INCORRECT);
            }
         } while (errno == ENOENT);
      }
      else
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to open() `%s' : %s",
                    job_id_data_file, strerror(errno));
         exit(INCORRECT);
      }
   }

   sleep_counter = 0;
   while ((p_afd_status->start_time == 0) && (sleep_counter < 1800))
   {
      (void)my_usleep(100000L);
      sleep_counter++;
      if ((sleep_counter % 300) == 0)
      {
         system_log(INFO_SIGN, __FILE__, __LINE__,
                    "Hmm, still waiting for AMG to finish writting to JID structure (wait time %d).",
                    (sleep_counter / 10));
      }
   }
   if (sleep_counter >= 1800)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Timeout arrived for waiting (180 s) for AMG to finish writting to JID structure.");
   }

#ifdef HAVE_STATX
   if (statx(jd_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(jd_fd, &stat_buf) == -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                 "Failed to statx() `%s' : %s",
#else
                 "Failed to fstat() `%s' : %s",
#endif
                 job_id_data_file, strerror(errno));
      exit(INCORRECT);
   }
   sleep_counter = 0;
#ifdef HAVE_STATX
   while (stat_buf.stx_size == 0)
#else
   while (stat_buf.st_size == 0)
#endif
   {
      (void)my_usleep(100000L);
#ifdef HAVE_STATX
      if (statx(jd_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(jd_fd, &stat_buf) == -1)
#endif
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                    "Failed to statx() `%s' : %s",
#else
                    "Failed to fstat() `%s' : %s",
#endif
                    job_id_data_file, strerror(errno));
         exit(INCORRECT);
      }
      sleep_counter++;
      if (sleep_counter > 100)
      {
         break;
      }
   }

   /*
    * If we lock the file to early init_job_data() of the AMG does not
    * get the time to fill all data into the JID structure.
    */
   sleep_counter = 0;
   while (p_afd_status->amg_jobs & WRITTING_JID_STRUCT)
   {
      (void)my_usleep(100000L);
      sleep_counter++;
      if (sleep_counter > 110)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Timeout arrived for waiting for AMG to finish writing to JID structure.");
         exit(INCORRECT);
      }
   }
#ifdef LOCK_DEBUG
   lock_region_w(jd_fd, 1, __FILE__, __LINE__);
#else
   lock_region_w(jd_fd, 1);
#endif
#ifdef HAVE_STATX
   if (statx(jd_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(jd_fd, &stat_buf) == -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                 "Failed to statx() `%s' : %s",
#else
                 "Failed to fstat() `%s' : %s",
#endif
                 job_id_data_file, strerror(errno));
      exit(INCORRECT);
   }
#ifdef LOCK_DEBUG
   unlock_region(jd_fd, 1, __FILE__, __LINE__);
#else
   unlock_region(jd_fd, 1);
#endif

#ifdef HAVE_STATX
   if (stat_buf.stx_size > 0)
#else
   if (stat_buf.st_size > 0)
#endif
   {
#ifdef HAVE_MMAP
      if ((ptr = mmap(0,
# ifdef HAVE_STATX
                      stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                      stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                      MAP_SHARED, jd_fd, 0)) == (caddr_t) -1)
#else
      if ((ptr = mmap_emu(0,
# ifdef HAVE_STATX
                          stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                          stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                          MAP_SHARED, job_id_data_file, 0)) == (caddr_t) -1)
#endif
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to mmap() `%s' : %s",
                    job_id_data_file, strerror(errno));
         exit(INCORRECT);
      }
      if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_JID_VERSION)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Incorrect JID version (data=%d current=%d)!",
                    *(ptr + SIZEOF_INT + 1 + 1 + 1), CURRENT_JID_VERSION);
         exit(INCORRECT);
      }
      no_of_job_ids = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      jd = (struct job_id_data *)ptr;
   }
   else
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "File `%s' is empty! Terminating, don't know what to do :-(",
                 job_id_data_file);
      exit(INCORRECT);
   }

#ifdef HAVE_STATX
   if (((*no_of_job_ids * sizeof(struct job_id_data)) + AFD_WORD_OFFSET) > stat_buf.stx_size)
#else
   if (((*no_of_job_ids * sizeof(struct job_id_data)) + AFD_WORD_OFFSET) > stat_buf.st_size)
#endif
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                 "Hmmmm. Size of `%s' is %ld bytes, but calculation says it should be %d bytes (%d jobs)!",
#else
                 "Hmmmm. Size of `%s' is %lld bytes, but calculation says it should be %d bytes (%d jobs)!",
#endif
#ifdef HAVE_STATX
                 JOB_ID_DATA_FILE, (pri_off_t)stat_buf.stx_size,
#else
                 JOB_ID_DATA_FILE, (pri_off_t)stat_buf.st_size,
#endif
                 (*no_of_job_ids * sizeof(struct job_id_data)) + AFD_WORD_OFFSET,
                 *no_of_job_ids);
      ptr = (char *)jd - AFD_WORD_OFFSET;
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
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "munmap() error : %s", strerror(errno));
      }
      (void)close(jd_fd);
      (void)sleep(1);
      if (sleep_counter > 20)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Something is really wrong here! Size of structure is not what it should be!");
         exit(INCORRECT);
      }
      sleep_counter++;
      goto stat_again;
   }
#ifdef _DEBUG
   system_log(DEBUG_SIGN, __FILE__, __LINE__,
# ifdef HAVE_STATX
              "stat_buf.stx_size = %d|no_of_job_ids = %d|struct size = %d|total plus word offset = %d",
              stat_buf.stx_size, *no_of_job_ids, sizeof(struct job_id_data),
# else
              "stat_buf.st_size = %d|no_of_job_ids = %d|struct size = %d|total plus word offset = %d",
              stat_buf.st_size, *no_of_job_ids, sizeof(struct job_id_data),
# endif
              (*no_of_job_ids * sizeof(struct job_id_data)) + AFD_WORD_OFFSET);
#endif
#ifdef HAVE_STATX
   jid_struct_size = stat_buf.stx_size;
#else
   jid_struct_size = stat_buf.st_size;
#endif

   /* Read and store current message list. */
   if (read_current_msg_list(&cml, &no_of_current_msg) == INCORRECT)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Unable to read current message list, no point to continue.");
      exit(INCORRECT);
   }

   /*
    * Compare the current message list with those in the cache. Each
    * message not found is read from the message file and appended to
    * the cache. All messages that are in the current message list
    * are marked.
    */
   for (i = 0; i < *no_msg_cached; i++)
   {
      mdb[i].in_current_fsa = NO;
   }
   if ((mdb != NULL) && (*no_msg_cached > 0))
   {
      register int gotcha,
                   j;

      for (i = 0; i < no_of_current_msg; i++)
      {
         gotcha = NO;
         for (j = 0; j < *no_msg_cached; j++)
         {
            if (cml[i] == mdb[j].job_id)
            {
               /*
                * The position of this job might have changed
                * within the FSA, but the contents of the job is
                * still the same.
                */
               if ((no_of_hosts > mdb[j].fsa_pos) &&
                   (CHECK_STRCMP(mdb[j].host_name,
                                 fsa[mdb[j].fsa_pos].host_alias) == 0))
               {
                  mdb[j].in_current_fsa = YES;
                  gotcha = YES;
               }
               else
               {
                  int pos;

                  if ((pos = get_host_position(fsa, mdb[j].host_name,
                                               no_of_hosts)) != -1)
                  {
                     mdb[j].in_current_fsa = YES;
                     mdb[j].fsa_pos = pos;
                     gotcha = YES;
                  }
                  else
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "Hmmm. Host `%s' no longer in FSA [job = %x]",
                                mdb[j].host_name, mdb[j].job_id);
                  }
               }

               /*
                * NOTE: Due to duplicate job entries we may not bail
                *       out of this loop, otherwise BOTH of them will
                *       be removed.
                */
            }
         }
         if (gotcha == NO)
         {
            if (get_job_data(cml[i], -1, 0L, 0) == SUCCESS)
            {
               mdb[*no_msg_cached - 1].in_current_fsa = YES;
            }
            else
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Unable to add job `%x' to cache.", cml[i]);
            }
         }
      }
   }
   else /* No messages cached. */
   {
      for (i = 0; i < no_of_current_msg; i++)
      {
         if (get_job_data(cml[i], -1, 0L, 0) == SUCCESS)
         {
            mdb[*no_msg_cached - 1].in_current_fsa = YES;
         }
         else
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Added job `%x' to cache.",
                       mdb[*no_msg_cached - 1].job_id);
         }
      }
   }
   free(cml);

   /*
    * Go through the message directory and check if any unmarked message
    * is NOT in the queue buffer and the age of the message file is not
    * older then the oldest output log file [They are kept for show_olog
    * which can resend files]. Remove such a message from the msg_cache_buf
    * structure AND from the message directory AND job_id_data structure.
    */
#ifdef _OUTPUT_LOG
   current_time = time(NULL);
#endif
   *p_msg_dir = '\0';
   if ((dp = opendir(msg_dir)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to opendir() `%s'. Thus unable to delete any old messages. : %s",
                 msg_dir, strerror(errno));
   }
   else
   {
      char          *ck_ml;             /* Checked message list. */
      struct dirent *p_dir;

      if ((ck_ml = malloc(*no_msg_cached)) == NULL)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "malloc() error, cannot perform full message cache check : %s",
                    strerror(errno));
      }
      else
      {
         (void)memset(ck_ml, NO, *no_msg_cached);
      }

      errno = 0;
      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] == '.')
         {
            continue;
         }

         (void)strcpy(p_msg_dir, p_dir->d_name);
#ifdef HAVE_STATX
         if (statx(0, msg_dir, AT_STATX_SYNC_AS_STAT,
                   STATX_MTIME, &stat_buf) == -1)
#else
         if (stat(msg_dir, &stat_buf) == -1)
#endif
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                       "Failed to statx() `%s' : %s",
#else
                       "Failed to stat() `%s' : %s",
#endif
                       msg_dir, strerror(errno));
         }
         else
         {
#ifdef _OUTPUT_LOG
# ifdef HAVE_STATX
            if (current_time >
                (stat_buf.stx_mtime.tv_sec + (SWITCH_FILE_TIME * max_output_log_files)))
# else
            if (current_time >
                (stat_buf.st_mtime + (SWITCH_FILE_TIME * max_output_log_files)))
# endif
            {
#endif
               unsigned int job_id = (unsigned int)strtoul(p_dir->d_name, (char **)NULL, 16);
               int          gotcha = NO;

               for (i = 0; i < *no_msg_cached; i++)
               {
                  if (mdb[i].job_id == job_id)
                  {
                     if (ck_ml != NULL)
                     {
                        ck_ml[i] = YES;
                     }
                     if (mdb[i].in_current_fsa == YES)
                     {
                        gotcha = YES;
                     }
#ifdef _OUTPUT_LOG
                     else if (current_time <
                              (mdb[i].last_transfer_time + (SWITCH_FILE_TIME * max_output_log_files)))
                          {
                             /*
                              * Hmmm. Files have been transferred. Lets
                              * set gotcha so it does not delete this
                              * job, so show_olog can still resend files.
                              */
                             gotcha = NEITHER;
                          }
#endif
                     break;
                  }
               } /* for (i = 0; i < *no_msg_cached; i++) */

               /*
                * Yup, we can remove this job.
                */
               if (gotcha == NO)
               {
                  if (i == *no_msg_cached)
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "Hmm, i == *no_msg_cached (%d) for job %x",
                                i, *no_msg_cached, job_id);
                  }
                  (void)list_job_to_remove(i, jd_fd, jd, job_id);
               }
#ifdef _OUTPUT_LOG
            }
#endif
         }
         errno = 0;
      } /* while ((p_dir = readdir(dp)) != NULL) */

      if (errno)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "readdir() error : %s", strerror(errno));
      }
      if (closedir(dp) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "closedir() error : %s", strerror(errno));
      }

      remove_jobs(jd_fd, &jid_struct_size, job_id_data_file);

      /*
       * Lets go through the message cache again and locate any jobs
       * no longer of interest. This is necessary when messages
       * in the message directory are deleted by hand.
       * At the same time lets check if the host for the cached
       * message is still in the FSA.
       */
      if (ck_ml != NULL)
      {
         int remove_flag;

         for (i = 0; i < *no_msg_cached; i++)
         {
            remove_flag = NO;

            if (ck_ml[i] == NO)
            {
#ifdef _OUTPUT_LOG
               if ((mdb[i].in_current_fsa != YES) &&
                   (current_time > (mdb[i].last_transfer_time + (SWITCH_FILE_TIME * max_output_log_files))))
#else
               if (mdb[i].in_current_fsa != YES)
#endif
               {
                  remove_flag = YES;
               }
            }

            if ((mdb[i].fsa_pos < no_of_hosts) && (mdb[i].fsa_pos > -1))
            {
               if (CHECK_STRCMP(mdb[i].host_name, fsa[mdb[i].fsa_pos].host_alias) != 0)
               {
                  int new_pos;

                  if ((new_pos = get_host_position(fsa, mdb[i].host_name,
                                                   no_of_hosts)) == INCORRECT)
                  {
                     if (remove_flag == NO)
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   "Hmm. Host `%s' is no longer in the FSA. Removed it from cache.",
                                   mdb[i].host_name);
                     }
                     mdb[i].fsa_pos = -1; /* So remove_job_files() does NOT write to FSA! */
                     remove_flag = YES;
                  }
                  else
                  {
#ifdef _DEBUG
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "Hmm. Host position for `%s' is incorrect!? Correcting %d->%d",
                                mdb[i].host_name, mdb[i].fsa_pos, new_pos);
#endif
                     mdb[i].fsa_pos = new_pos;
                  }
               }
            }
            else
            {
               int new_pos;

               if ((new_pos = get_host_position(fsa, mdb[i].host_name,
                                                no_of_hosts)) == INCORRECT)
               {
                  if (remove_flag == NO)
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "Hmm. Host `%s' is no longer in the FSA. Removed it from cache.",
                                mdb[i].host_name);
                  }
                  mdb[i].fsa_pos = -1; /* So remove_job_files() does NOT write to FSA! */
                  remove_flag = YES;
               }
               else
               {
#ifdef _DEBUG
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Hmm. Host position for `%s' is incorrect!? Correcting %d->%d",
                             mdb[i].host_name, mdb[i].fsa_pos, new_pos);
#endif
                  mdb[i].fsa_pos = new_pos;
               }
            }

            if (remove_flag == YES)
            {
               (void)snprintf(p_msg_dir,
                              MAX_PATH_LENGTH - (p_msg_dir - msg_dir),
                              "%x", mdb[i].job_id);
               if (list_job_to_remove(i, jd_fd, jd, mdb[i].job_id) == SUCCESS)
               {
                  i--;
               }
               *p_msg_dir = '\0';
            }
         } /* for (i = 0; i < *no_msg_cached; i++) */

         free(ck_ml);
      }
      remove_jobs(jd_fd, &jid_struct_size, job_id_data_file);
   }

   /* Don't forget to unmap from job_id_data structure. */
   ptr = (char *)jd - AFD_WORD_OFFSET;
   if (msync(ptr, jid_struct_size, MS_SYNC) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "msync() error : %s", strerror(errno));
   }
#ifdef HAVE_MMAP
   if (munmap(ptr, jid_struct_size) == -1)
#else
   if (munmap_emu(ptr) == -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "munmap() error : %s", strerror(errno));
   }
   if (close(jd_fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }

   if (removed_messages > 0)
   {
      size_t length = 0;
      char   alternate_file_name[MAX_PATH_LENGTH],
             buffer[80],
             *p_alternate_file_name;

      p_alternate_file_name = alternate_file_name +
                              snprintf(alternate_file_name, MAX_PATH_LENGTH,
                                       "%s%s%s",
                                       p_work_dir, FIFO_DIR, ALTERNATE_FILE);
      system_log(INFO_SIGN, NULL, 0,
                 "Removed %d old message(s).", removed_messages);

      /* Show which messages have been removed. */
      for (i = 0; i < removed_messages; i++)
      {
         (void)snprintf(p_alternate_file_name,
                        MAX_PATH_LENGTH - (p_alternate_file_name - alternate_file_name),
                        "%x", rml[i]);
         (void)unlink(alternate_file_name);
         length += snprintf(&buffer[length], 80 - length, "#%x ", rml[i]);
         if (length > 51)
         {
            system_log(DEBUG_SIGN, NULL, 0, "%s", buffer);
            length = 0;
         }
      }
      if (length != 0)
      {
         system_log(DEBUG_SIGN, NULL, 0, "%s", buffer);
      }
      free(rml);
   }

#if defined (_MAINTAINER_LOG) && defined (SHOW_MSG_CACHE)
   maintainer_log(INFO_SIGN, __FILE__, __LINE__,
                  "%s with %d elements after modifying it.",
                  MSG_CACHE_FILE, *no_msg_cached);
   for (i = 0; i < *no_msg_cached; i++)
   {
      (void)strftime(tbuf1, 20, "%d.%m.%Y %H:%M:%S", localtime(&mdb[i].msg_time));
      (void)strftime(tbuf2, 20, "%d.%m.%Y %H:%M:%S", localtime(&mdb[i].last_transfer_time));
      maintainer_log(DEBUG_SIGN, __FILE__, __LINE__,
                     "%5d: %-*s %s %s %d %d %x %u %d %s",
                     i, MAX_HOSTNAME_LENGTH, mdb[i].host_name, tbuf1, tbuf2,
                     mdb[i].fsa_pos, mdb[i].port, mdb[i].job_id,
                     mdb[i].age_limit, (int)mdb[i].type,
                     (mdb[i].in_current_fsa == YES) ? "Yes" : "No");
   }
#endif

   return;
}


/*++++++++++++++++++++++++ list_job_to_remove() +++++++++++++++++++++++++*/
static int
list_job_to_remove(int                cache_pos,
                   int                jd_fd,
                   struct job_id_data *jd,
                   unsigned int       job_id)
{
   int          dir_id_pos = -1,
                gotcha,
                j,
                k,
                remove_file_mask = NO,
                removed_job_pos = -1;
   unsigned int file_mask_id = 0;

   /*
    * Before we remove anything, make sure that this job is NOT
    * in the queue and sending data.
    */
   for (j = 0; j < *no_msg_queued; j++)
   {
      if (((qb[j].special_flag & FETCH_JOB) == 0) && (qb[j].pos == cache_pos))
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Job `%x' is currently in the queue!", job_id);

         if (qb[j].pid > 0)
         {
            if (connection != NULL)
            {
               if (qb[j].connect_pos >= 0)
               {
                  system_log(DEBUG_SIGN, NULL, 0,
#if SIZEOF_PID_T == 4
                             "AND process %d is currently distributing files for host %s! Will terminate this process.",
#else
                             "AND process %lld is currently distributing files for host %s! Will terminate this process.",
#endif
                             (pri_pid_t)qb[j].pid, connection[qb[j].connect_pos].hostname);
               }
               else
               {
                  system_log(DEBUG_SIGN, NULL, 0,
#if SIZEOF_PID_T == 4
                             "AND process %d is currently distributing files! Will terminate this process.",
#else
                             "AND process %lld is currently distributing files! Will terminate this process.",
#endif
                             (pri_pid_t)qb[j].pid);
               }
            }
            if (kill(qb[j].pid, SIGKILL) < 0)
            {
               if (errno != ESRCH)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                             "Failed to kill transfer job to `%s' (%d) : %s",
#else
                             "Failed to kill transfer job to `%s' (%lld) : %s",
#endif
                             mdb[qb[j].pos].host_name, (pri_pid_t)qb[j].pid,
                             strerror(errno));
               }
            }
            else
            {
               int status;

               /* Remove the zombie. */
               if (waitpid(qb[j].pid, &status, 0) == qb[j].pid)
               {
                  qb[j].pid = PENDING;
                  if (qb[j].connect_pos != -1)
                  {
                     /* Decrease the number of active transfers. */
                     if (p_afd_status->no_of_transfers > 0)
                     {
                        p_afd_status->no_of_transfers--;
                     }
                     else
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   "Huh?! Whats this trying to reduce number of transfers although its zero???");
                     }

                     if (connection != NULL)
                     {
                        if ((connection[qb[j].connect_pos].fsa_pos != -1) &&
                            (mdb[qb[j].pos].fsa_pos != -1))
                        {
                           if (fsa[connection[qb[j].connect_pos].fsa_pos].active_transfers > fsa[connection[qb[j].connect_pos].fsa_pos].allowed_transfers)
                           {
                              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                         "Active transfers > allowed transfers %d!? [%d]",
                                         fsa[connection[qb[j].connect_pos].fsa_pos].allowed_transfers,
                                         fsa[connection[qb[j].connect_pos].fsa_pos].active_transfers);
                              fsa[connection[qb[j].connect_pos].fsa_pos].active_transfers = fsa[connection[qb[j].connect_pos].fsa_pos].allowed_transfers;
                           }
                           fsa[connection[qb[j].connect_pos].fsa_pos].active_transfers -= 1;
                           if (fsa[connection[qb[j].connect_pos].fsa_pos].active_transfers < 0)
                           {
                              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                         "Active transfers for FSA position %d < 0!? [%d]",
                                         connection[qb[j].connect_pos].fsa_pos,
                                         fsa[connection[qb[j].connect_pos].fsa_pos].active_transfers);
                              fsa[connection[qb[j].connect_pos].fsa_pos].active_transfers = 0;
                           }
                           calc_trl_per_process(connection[qb[j].connect_pos].fsa_pos);
                           fsa[connection[qb[j].connect_pos].fsa_pos].job_status[connection[qb[j].connect_pos].job_no].proc_id = -1;
                           fsa[connection[qb[j].connect_pos].fsa_pos].job_status[connection[qb[j].connect_pos].job_no].connect_status = DISCONNECT;
                           fsa[connection[qb[j].connect_pos].fsa_pos].job_status[connection[qb[j].connect_pos].job_no].no_of_files_done = 0;
                           fsa[connection[qb[j].connect_pos].fsa_pos].job_status[connection[qb[j].connect_pos].job_no].file_size_done = 0;
                           fsa[connection[qb[j].connect_pos].fsa_pos].job_status[connection[qb[j].connect_pos].job_no].no_of_files = 0;
                           fsa[connection[qb[j].connect_pos].fsa_pos].job_status[connection[qb[j].connect_pos].job_no].file_size = 0;
                           fsa[connection[qb[j].connect_pos].fsa_pos].job_status[connection[qb[j].connect_pos].job_no].file_size_in_use = 0;
                           fsa[connection[qb[j].connect_pos].fsa_pos].job_status[connection[qb[j].connect_pos].job_no].file_size_in_use_done = 0;
                           fsa[connection[qb[j].connect_pos].fsa_pos].job_status[connection[qb[j].connect_pos].job_no].file_name_in_use[0] = '\0';
                           fsa[connection[qb[j].connect_pos].fsa_pos].job_status[connection[qb[j].connect_pos].job_no].file_name_in_use[1] = 0;
#ifdef _WITH_BURST_2
                           fsa[connection[qb[j].connect_pos].fsa_pos].job_status[connection[qb[j].connect_pos].job_no].unique_name[0] = '\0';
                           fsa[connection[qb[j].connect_pos].fsa_pos].job_status[connection[qb[j].connect_pos].job_no].job_id = NO_ID;
#endif
                        }
                        connection[qb[j].connect_pos].hostname[0] = '\0';
                        connection[qb[j].connect_pos].host_id = 0;
                        connection[qb[j].connect_pos].job_no = -1;
                        connection[qb[j].connect_pos].fra_pos = -1;
                        connection[qb[j].connect_pos].msg_name[0] = '\0';
                        connection[qb[j].connect_pos].pid = 0;
                        connection[qb[j].connect_pos].fsa_pos = -1;
                     }
                  }
               }
            }
         }

#ifdef SF_BURST_ACK
         /* There may not be any pending acks. */
         for (k = 0; k < *no_of_acks_queued; k++)
         {
            if (strncmp(qb[j].msg_name, ab[k].msg_name,
                        MAX_MSG_NAME_LENGTH) == 0)
            {
               if (k <= (*no_of_acks_queued - 1))
               {
                  (void)memmove(&ab[k], &ab[k + 1],
                                ((*no_of_acks_queued - 1 - k) *
                                 sizeof(struct ack_queue_buf)));
               }
               (*no_of_acks_queued)--;

               break;
            }
         }
#endif

         /*
          * NOOOO. There may not be any message in the queue.
          * Remove it if there is one.
          */
         (void)strcpy(p_file_dir, qb[j].msg_name);
#ifdef _DELETE_LOG
         extract_cus(qb[j].msg_name, dl.input_time, dl.split_job_counter,
                     dl.unique_number);
         remove_job_files(file_dir, mdb[qb[j].pos].fsa_pos,
                          mdb[qb[j].pos].job_id, FD, CLEAR_STALE_MESSAGES, -1,
                          __FILE__, __LINE__);
#else
         remove_job_files(file_dir, -1, -1, __FILE__, __LINE__);
#endif
         *p_file_dir = '\0';
#if defined (_RMQUEUE_) && defined (_MAINTAINER_LOG)
         remove_msg(j, NO, "init_msg_buffer.c", __LINE__);
#else
         remove_msg(j, NO);
#endif
         if (j < *no_msg_queued)
         {
            j--;
         }
      }
   }

   /* Remember the position in jd structure where we */
   /* have to remove the job.                        */
   for (j = 0; j < *no_of_job_ids; j++)
   {
      if (jd[j].job_id == job_id)
      {
         /*
          * Before we remove it, lets remember the
          * directory and file mask ID's from where it came from.
          * So we can remove any stale directory or file mask
          * entries from their relevant structures.
          */
         dir_id_pos = jd[j].dir_id_pos;
         remove_file_mask = YES;
         file_mask_id = jd[j].file_mask_id;

         if ((removed_jobs % REMOVE_STEP_SIZE) == 0)
         {
            size_t new_size = ((removed_jobs / REMOVE_STEP_SIZE) + 1) *
                              REMOVE_STEP_SIZE * sizeof(unsigned int);

            if (removed_jobs == 0)
            {
               if ((rjl = malloc(new_size)) == NULL)
               {
                  system_log(FATAL_SIGN, __FILE__, __LINE__,
#if SIZEOF_SIZE_T == 4
                             "Could not malloc() %d bytes : %s",
#else
                             "Could not malloc() %lld bytes : %s",
#endif
                             (pri_size_t)new_size, strerror(errno));
                  exit(INCORRECT);
               }
            }
            else
            {
               if ((rjl = realloc(rjl, new_size)) == NULL)
               {
                  system_log(FATAL_SIGN, __FILE__, __LINE__,
#if SIZEOF_SIZE_T == 4
                             "Could not realloc() %d bytes : %s",
#else
                             "Could not realloc() %lld bytes : %s",
#endif
                             (pri_size_t)new_size, strerror(errno));
                  exit(INCORRECT);
               }
            }
         }
         removed_job_pos = rjl[removed_jobs] = j;
         removed_jobs++;
         break;
      }
   }
   if ((dir_id_pos != -1) || (remove_file_mask == YES))
   {
      /*
       * Go through job list and make sure no other job
       * has the same dir_id_pos, ie is using this directory
       * as its source directory. Same goes for the file masks.
       */
      for (j = 0; j < *no_of_job_ids; j++)
      {
         gotcha = NO;
         for (k = 0; k < removed_jobs; k++)
         {
            if (rjl[k] == j)
            {
               gotcha = YES;
               break;
            }
         }
         if (gotcha == NO)
         {
            if (dir_id_pos == jd[j].dir_id_pos)
            {
               dir_id_pos = -1;
               if (remove_file_mask == NO)
               {
                  break;
               }
            }
            if (file_mask_id == jd[j].file_mask_id)
            {
               remove_file_mask = NO;
               if (dir_id_pos == -1)
               {
                  break;
               }
            }
         }
      }
   }

   /* Remove outgoing job directory. */
   if (*(p_file_dir - 1) != '/')
   {
      *p_file_dir = '/';
      (void)strcpy(p_file_dir + 1, p_msg_dir);
   }
   else
   {
      (void)strcpy(p_file_dir, p_msg_dir);
   }
   if (rec_rmdir(file_dir) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to rec_rmdir() %s", file_dir);
   }
   *p_file_dir = '\0';

   /* Remove message from message directory. */
   if (unlink(msg_dir) == -1)
   {
      if (errno != ENOENT)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to unlink() `%s' : %s", msg_dir, strerror(errno));
      }
   }
   else
   {
      if ((removed_messages % REMOVE_STEP_SIZE) == 0)
      {
         size_t new_size = ((removed_messages / REMOVE_STEP_SIZE) + 1) *
                           REMOVE_STEP_SIZE * sizeof(unsigned int);

         if (removed_messages == 0)
         {
            if ((rml = malloc(new_size)) == NULL)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
#if SIZEOF_SIZE_T == 4
                          "Could not malloc() %d bytes : %s",
#else
                          "Could not malloc() %lld bytes : %s",
#endif
                          (pri_size_t)new_size, strerror(errno));
               exit(INCORRECT);
            }
         }
         else
         {
            if ((rml = realloc(rml, new_size)) == NULL)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
#if SIZEOF_SIZE_T == 4
                          "Could not realloc() %d bytes : %s",
#else
                          "Could not realloc() %lld bytes : %s",
#endif
                          (pri_size_t)new_size, strerror(errno));
               exit(INCORRECT);
            }
         }
      }
      rml[removed_messages] = job_id;
      removed_messages++;
   }

   /*
    * Only remove from cache if it has the same job_id.
    * When there is no data in the fifodir but the
    * messages are still there, we would delete the
    * current message cache.
    */
   if (cache_pos < *no_msg_cached)
   {
      if ((cache_pos != (*no_msg_cached - 1)) &&
          (mdb[cache_pos].job_id == job_id) &&
          (mdb[cache_pos].in_current_fsa != YES))
      {
         register int j;
         size_t       move_size = (*no_msg_cached - 1 - cache_pos) *
                                  sizeof(struct msg_cache_buf);

         /*
          * The position in struct queue_buf is no longer
          * correct. Update all positions that are larger
          * then cache_pos.
          */
         for (j = 0; j < *no_msg_queued; j++)
         {
            if ((qb[j].pos > cache_pos) && ((qb[j].special_flag & FETCH_JOB) == 0))
            {
               qb[j].pos--;
            }
         } /* for (j = 0; j < *no_msg_queued; j++) */

         /*
          * Remove message data from cache.
          */
         (void)memmove(&mdb[cache_pos], &mdb[cache_pos + 1], move_size);
      }
      (*no_msg_cached)--;
   }
   else
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Hmmm, whats this!? cache_pos (%d) >= *no_msg_cached (%d), for job %x!",
                 cache_pos, *no_msg_cached, job_id);
   }

   /*
    * If the directory is not used anymore, remove it
    * from the DIR_NAME_FILE database.
    */
   if (dir_id_pos != -1)
   {
      int          fd;
      char         file[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat  stat_buf;
#endif

      (void)strcpy(file, p_work_dir);
      (void)strcat(file, FIFO_DIR);
      (void)strcat(file, DIR_NAME_FILE);
      if ((fd = open(file, O_RDWR)) == -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to open() `%s' : %s", file, strerror(errno));
         exit(INCORRECT);
      }

#ifdef HAVE_STATX
      if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(fd, &stat_buf) == -1)
#endif
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                    "Failed to statx() `%s' : %s",
#else
                    "Failed to fstat() `%s' : %s",
#endif
                    file, strerror(errno));
         (void)close(fd);
         exit(INCORRECT);
      }

#ifdef HAVE_STATX
      if (stat_buf.stx_size != 0)
#else
      if (stat_buf.st_size != 0)
#endif
      {
         int                 *no_of_dir_names;
         char                *ptr;
         struct dir_name_buf *dnb;

#ifdef HAVE_MMAP
         if ((ptr = mmap(0,
# ifdef HAVE_STATX
                         stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                         stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                         MAP_SHARED, fd, 0)) == (caddr_t) -1)
#else
         if ((ptr = mmap_emu(0,
# ifdef HAVE_STATX
                             stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                             stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                             MAP_SHARED, file, 0)) == (caddr_t) -1)
#endif
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Failed to mmap() `%s' : %s", file, strerror(errno));
            (void)close(fd);
            exit(INCORRECT);
         }
         no_of_dir_names = (int *)ptr;
         ptr += AFD_WORD_OFFSET;
         dnb = (struct dir_name_buf *)ptr;

         if (dir_id_pos < *no_of_dir_names)
         {
#ifdef WITH_DUP_CHECK
            char dup_dir[MAX_PATH_LENGTH];

            (void)snprintf(dup_dir, MAX_PATH_LENGTH, "%s%s%s/%u",
                           p_work_dir, AFD_FILE_DIR, STORE_DIR,
                           dnb[dir_id_pos].dir_id);
            (void)rmdir(dup_dir);
            (void)snprintf(dup_dir, MAX_PATH_LENGTH, "%s%s%s/%u",
                           p_work_dir, AFD_FILE_DIR, CRC_DIR,
                           dnb[dir_id_pos].dir_id);
            (void)unlink(dup_dir);
#endif
            system_log(DEBUG_SIGN, NULL, 0,
                       "Removing `%s' [%x] from dir_name_buf structure.",
                       dnb[dir_id_pos].dir_name, dnb[dir_id_pos].dir_id);
            if (dir_id_pos != (*no_of_dir_names - 1))
            {
               int    k;
               size_t move_size = (*no_of_dir_names - 1 - dir_id_pos) *
                                  sizeof(struct dir_name_buf);

               (void)memmove(&dnb[dir_id_pos], &dnb[dir_id_pos + 1], move_size);

               /*
                * Correct position dir_id_pos in struct job_id_data.
                */
#ifdef LOCK_DEBUG
               lock_region_w(jd_fd, 1, __FILE__, __LINE__);
#else
               lock_region_w(jd_fd, 1);
#endif
               for (k = 0; k < *no_of_job_ids; k++)
               {
                  if ((removed_job_pos != k) &&
                      (jd[k].dir_id_pos > dir_id_pos))
                  {
                     jd[k].dir_id_pos--;
                  }
               }
#ifdef LOCK_DEBUG
               unlock_region(jd_fd, 1, __FILE__, __LINE__);
#else
               unlock_region(jd_fd, 1);
#endif
            }
            (*no_of_dir_names)--;
         }
         else
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Hmmm, whats this? dir_id_pos (%d) >= *no_of_dir_names (%d)!?",
                       dir_id_pos, *no_of_dir_names);
         }

         ptr -= AFD_WORD_OFFSET;
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
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to munmap() `%s' : %s", file, strerror(errno));
         }
      } /* if (stat_buf.st_size != 0) */
      
      if (close(fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Failed to close() `%s' : %s", file, strerror(errno));
      }
   } /* if (dir_id_pos != -1) */

   /*
    * Store the file mask ID we might want to remove.
    */
   if (remove_file_mask == YES)
   {
      if ((file_mask_to_remove % REMOVE_STEP_SIZE) == 0)
      {
         size_t new_size = ((file_mask_to_remove / REMOVE_STEP_SIZE) + 1) *
                           REMOVE_STEP_SIZE * sizeof(unsigned int);

         if (file_mask_to_remove == 0)
         {
            if ((fmtrl = malloc(new_size)) == NULL)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
#if SIZEOF_SIZE_T == 4
                          "Could not malloc() %d bytes memory : %s",
#else
                          "Could not malloc() %lld bytes memory : %s",
#endif
                          (pri_size_t)new_size, strerror(errno));
               exit(INCORRECT);
            }
         }
         else
         {
            if ((fmtrl = realloc(fmtrl, new_size)) == NULL)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
#if SIZEOF_SIZE_T == 4
                          "Could not realloc() %d bytes memory : %s",
#else
                          "Could not realloc() %lld bytes memory : %s",
#endif
                          (pri_size_t)new_size, strerror(errno));
               exit(INCORRECT);
            }
         }
      }
      fmtrl[file_mask_to_remove] = file_mask_id;
      file_mask_to_remove++;
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++ remove_jobs() +++++++++++++++++++++++++++++*/
static void
remove_jobs(int jd_fd, off_t *jid_struct_size, char *job_id_data_file)
{
   if (removed_jobs > 0)
   {
      int           dcid_still_in_jid,
                    dc_id_to_remove,
                    i, k,
                    gotcha,
                    pwb_still_in_jid,
                    pwb_to_remove;
      unsigned int  *dcidr,
                    error_mask,
                    scheme;
      char          *ptr,
                    real_hostname[MAX_REAL_HOSTNAME_LENGTH + 1],
                    **rpl,
                    uh_name[MAX_USER_NAME_LENGTH + MAX_REAL_HOSTNAME_LENGTH + 1],
                    user[MAX_USER_NAME_LENGTH + 1],
                    smtp_user[MAX_USER_NAME_LENGTH + 1];
      unsigned char smtp_auth;
#ifdef WITH_DUP_CHECK
      char          dup_dir[MAX_PATH_LENGTH],
                    *p_dup_dir;

      p_dup_dir = dup_dir + snprintf(dup_dir, MAX_PATH_LENGTH, "%s%s",
                                     p_work_dir, AFD_FILE_DIR);
#endif

#ifdef LOCK_DEBUG
      lock_region_w(jd_fd, 1, __FILE__, __LINE__);
#else
      lock_region_w(jd_fd, 1);
#endif
      sort_array(removed_jobs);

      /*
       * Always ensure that the JID structure size did NOT
       * change, else we might be moving things that do not
       * belong to us!
       */
      if (((*no_of_job_ids * sizeof(struct job_id_data)) + AFD_WORD_OFFSET) > *jid_struct_size)
      {
#ifdef HAVE_STATX
         struct statx stat_buf;
#else
         struct stat stat_buf;
#endif

         system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                    "Hmmmm. Size of `%s' is %ld bytes, but calculation says it should be %d bytes!",
#else
                    "Hmmmm. Size of `%s' is %lld bytes, but calculation says it should be %d bytes!",
#endif
                    JOB_ID_DATA_FILE, (pri_off_t)*jid_struct_size,
                    (*no_of_job_ids * sizeof(struct job_id_data)) + AFD_WORD_OFFSET);
         ptr = (char *)jd - AFD_WORD_OFFSET;
#ifdef HAVE_MMAP
         if (munmap(ptr, *jid_struct_size) == -1)
#else
         if (munmap_emu(ptr) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to munmap() `%s' : %s",
                       job_id_data_file, strerror(errno));
         }
#ifdef HAVE_STATX
         if (statx(jd_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                   STATX_SIZE, &stat_buf) == -1)
#else
         if (fstat(jd_fd, &stat_buf) == -1)
#endif
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                       "Failed to statx() `%s' : %s",
#else
                       "Failed to fstat() `%s' : %s",
#endif
                       job_id_data_file, strerror(errno));
            exit(INCORRECT);
         }
#ifdef HAVE_STATX
         if (stat_buf.stx_size > 0)
#else
         if (stat_buf.st_size > 0)
#endif
         {
#ifdef HAVE_MMAP
            if ((ptr = mmap(0,
# ifdef HAVE_STATX
                            stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                            stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                            MAP_SHARED, jd_fd, 0)) == (caddr_t) -1)
#else
            if ((ptr = mmap_emu(0,
# ifdef HAVE_STATX
                                stat_buf.stx_size,
# else
                                stat_buf.st_size,
# endif
                                (PROT_READ | PROT_WRITE), MAP_SHARED,
                                job_id_data_file, 0)) == (caddr_t) -1)
#endif
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "Failed to mmap() to `%s' : %s",
                          job_id_data_file, strerror(errno));
               exit(INCORRECT);
            }
            if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_JID_VERSION)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "Incorrect JID version (data=%d current=%d)!",
                          *(ptr + SIZEOF_INT + 1 + 1 + 1), CURRENT_JID_VERSION);
               exit(INCORRECT);
            }
            no_of_job_ids = (int *)ptr;
            ptr += AFD_WORD_OFFSET;
            jd = (struct job_id_data *)ptr;
#ifdef HAVE_STATX
            *jid_struct_size = stat_buf.stx_size;
#else
            *jid_struct_size = stat_buf.st_size;
#endif
         }
         else
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                      "File `%s' is empty! Terminating, don't know what to do :-( (%s %d)\n",
                      job_id_data_file, __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }

      /*
       * Store a list of user and hostnames and the DIR_CONFIG ID that we
       * might remove. Later after the jobs have been remove, go through
       * the job list again and see if these are still in the list,
       * because then we may not remove them.
       */
      pwb_to_remove = 0;
      RT_ARRAY(rpl, removed_jobs,
               (MAX_USER_NAME_LENGTH + MAX_HOSTNAME_LENGTH + 1), char);
      dc_id_to_remove = 0;
      if ((dcidr = malloc(removed_jobs * sizeof(unsigned int))) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to malloc() %d bytes : %s",
                    (removed_jobs * sizeof(unsigned int)), strerror(errno));
         exit(INCORRECT);
      }
      for (i = 0; i < removed_jobs; i++)
      {
#ifdef WITH_DUP_CHECK
         (void)snprintf(p_dup_dir, MAX_PATH_LENGTH - (p_dup_dir - dup_dir),
                        "%s/%u", STORE_DIR, jd[rjl[i]].job_id);
         (void)rmdir(dup_dir);
         (void)snprintf(p_dup_dir, MAX_PATH_LENGTH - (p_dup_dir - dup_dir),
                        "%s/%u", CRC_DIR, jd[rjl[i]].job_id);
         (void)unlink(dup_dir);
#endif
         rpl[pwb_to_remove][0] = '\0';
         if ((error_mask = url_evaluate(jd[rjl[i]].recipient, &scheme, user,
                                        &smtp_auth, smtp_user,
#ifdef WITH_SSH_FINGERPRINT
                                        NULL, NULL,
#endif
                                        NULL, NO, real_hostname, NULL, NULL,
                                        NULL, NULL, NULL, NULL, NULL, NULL,
                                        NULL, NULL)) < 4)
         {
            if ((scheme & LOC_FLAG) ||
#ifdef _WITH_DE_MAIL_SUPPORT
                (((scheme & SMTP_FLAG) || (scheme & DE_MAIL_FLAG)) &&
                  (smtp_auth == SMTP_AUTH_NONE)) ||
#else
                ((scheme & SMTP_FLAG) && (smtp_auth == SMTP_AUTH_NONE)) ||
#endif
#ifdef _WITH_WMO_SUPPORT
                (scheme & WMO_FLAG) ||
#endif
#ifdef _WITH_MAP_SUPPORT
                (scheme & MAP_FLAG) ||
#endif
#ifdef _WITH_DFAX_SUPPORT
                (scheme & DFAX_FLAG) ||
#endif
                (scheme & EXEC_FLAG))
            {
               /* These do not have passwords. */;
            }
            else
            {
#ifdef _WITH_DE_MAIL_SUPPORT
               if (((scheme & SMTP_FLAG) || (scheme & DE_MAIL_FLAG)) &&
                    (smtp_auth != SMTP_AUTH_NONE))
#else
               if ((scheme & SMTP_FLAG) && (smtp_auth != SMTP_AUTH_NONE))
#endif
               {
                  (void)strcpy(rpl[pwb_to_remove], smtp_user);
                  t_hostname(real_hostname,
                             &rpl[pwb_to_remove][strlen(rpl[pwb_to_remove])]);
               }
               else
               {
                  if (user[0] != '\0')
                  {
                     (void)strcpy(rpl[pwb_to_remove], user);
                     t_hostname(real_hostname,
                                &rpl[pwb_to_remove][strlen(rpl[pwb_to_remove])]);
                  }
                  else
                  {
                     t_hostname(real_hostname, rpl[pwb_to_remove]);
                  }
               }

               /* Check that we do not already stored this */
               /* user hostname pair.                      */
               gotcha = NO;
               for (k = 0; k < pwb_to_remove; k++)
               {
                  if (CHECK_STRCMP(rpl[k], rpl[pwb_to_remove]) == 0)
                  {
                     gotcha = YES;
                     break;
                  }
               }
               if (gotcha == NO)
               {
                  pwb_to_remove++;
               }
            }
         }
         else
         {
            char error_msg[MAX_URL_ERROR_MSG];

            url_get_error(error_mask, error_msg, MAX_URL_ERROR_MSG);
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Incorrect url `%s'. Error is: %s.",
                       jd[rjl[i]].recipient, error_msg);
         }

         /* Store the DIR_CONFIG ID. */
         gotcha = NO;
         for (k = 0; k < dc_id_to_remove; k++)
         {
            if (dcidr[k] == jd[rjl[i]].dir_config_id)
            {
               gotcha = YES;
               break;
            }
         }
         if (gotcha == NO)
         {
            dcidr[dc_id_to_remove] = jd[rjl[i]].dir_config_id;
            dc_id_to_remove++;
         }
      } /* for (i = 0; i < removed_jobs; i++) */

      /* Delete the jobs. */
      for (i = 0; i < removed_jobs; i++)
      {
         int end_pos = i,
             j,
             jobs_deleted,
             start_pos = i;

         while ((++end_pos < removed_jobs) &&
                ((rjl[end_pos - 1] + 1) == rjl[end_pos]))
         {
            /* Nothing to be done. */;
         }
         if (rjl[end_pos - 1] != (*no_of_job_ids - 1))
         {
            size_t move_size = (*no_of_job_ids - (rjl[end_pos - 1] + 1)) *
                               sizeof(struct job_id_data);

#ifdef _DEBUG
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "no_of_job_ids = %d|struct size = %d|move_size = %d|&jd[%d] = %d|&jd[%d] = %d",
                       *no_of_job_ids, sizeof(struct job_id_data),
                       move_size, rjl[i], &jd[rjl[i]],
                       rjl[end_pos - 1] + 1, &jd[rjl[end_pos - 1] + 1]);
#endif
            (void)memmove(&jd[rjl[i]], &jd[rjl[end_pos - 1] + 1], move_size);
         }
         jobs_deleted = end_pos - i;
         (*no_of_job_ids) -= jobs_deleted;
         i += (end_pos - i - 1);
         for (j = (i + 1); j < removed_jobs; j++)
         {
            if (rjl[j] > rjl[start_pos])
            {
               rjl[j] -= jobs_deleted;
            }
         }
      } /* for (i = 0; i < removed_jobs; i++) */

      /* Check if any of the passwd we want to delete are still in the JID. */
      pwb_still_in_jid = 0;
      dcid_still_in_jid = 0;
      for (i = 0; i < *no_of_job_ids; i++)
      {
         if ((error_mask = url_evaluate(jd[i].recipient, &scheme, user,
                                        &smtp_auth, smtp_user,
#ifdef WITH_SSH_FINGERPRINT
                                        NULL, NULL,
#endif
                                        NULL, NO, real_hostname, NULL, NULL,
                                        NULL, NULL, NULL, NULL, NULL, NULL,
                                        NULL, NULL)) < 4)
         {
            if ((scheme & LOC_FLAG) ||
#ifdef _WITH_DE_MAIL_SUPPORT
                (((scheme & SMTP_FLAG) || (scheme & DE_MAIL_FLAG)) &&
                  (smtp_auth == SMTP_AUTH_NONE)) ||
#else
                ((scheme & SMTP_FLAG) && (smtp_auth == SMTP_AUTH_NONE)) ||
#endif
#ifdef _WITH_WMO_SUPPORT
                (scheme & WMO_FLAG) ||
#endif
#ifdef _WITH_MAP_SUPPORT
                (scheme & MAP_FLAG) ||
#endif
#ifdef _WITH_DFAX_SUPPORT
                (scheme & DFAX_FLAG) ||
#endif
                (scheme & EXEC_FLAG))
            {
               /* These do not have passwords. */;
            }
            else
            {
#ifdef _WITH_DE_MAIL_SUPPORT
               if (((scheme & SMTP_FLAG) || (scheme & DE_MAIL_FLAG)) &&
                    (smtp_auth != SMTP_AUTH_NONE))
#else
               if ((scheme & SMTP_FLAG) && (smtp_auth != SMTP_AUTH_NONE))
#endif
               {
                  (void)strcpy(uh_name, smtp_user);
                  t_hostname(real_hostname, &uh_name[strlen(uh_name)]);
               }
               else
               {
                  if (user[0] != '\0')
                  {
                     (void)strcpy(uh_name, user);
                     t_hostname(real_hostname, &uh_name[strlen(uh_name)]);
                  }
                  else
                  {
                     t_hostname(real_hostname, uh_name);
                  }
               }

               for (k = 0; k < pwb_to_remove; k++)
               {
                  if ((rpl[k][0] != '\0') &&
                      (CHECK_STRCMP(uh_name, rpl[k]) == 0))
                  {
                     rpl[k][0] = '\0';
                     pwb_still_in_jid++;
                     break;
                  }
               }
            }
         }
         else
         {
            char error_msg[MAX_URL_ERROR_MSG];

            url_get_error(error_mask, error_msg, MAX_URL_ERROR_MSG);
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Incorrect url `%s' (JID position %d). Error is: %s.",
                       jd[i].recipient, i, error_msg);
         }
         for (k = 0; k < dc_id_to_remove; k++)
         {
            if (jd[i].dir_config_id == dcidr[k])
            {
               dcidr[k] = 0;
               dcid_still_in_jid++;
               break;
            }
         }
      }

      /* Remove any unused passwords from the password database. */
      if ((pwb_to_remove - pwb_still_in_jid) > 0)
      {
         int  pwb_fd;
         char pwb_file_name[MAX_PATH_LENGTH];

         (void)strcpy(pwb_file_name, p_work_dir);
         (void)strcat(pwb_file_name, FIFO_DIR);
         (void)strcat(pwb_file_name, PWB_DATA_FILE);
         if ((pwb_fd = open(pwb_file_name, O_RDWR)) == -1)
         {
            if (errno != ENOENT)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to open() `%s' : %s",
                          pwb_file_name, strerror(errno));
            }
         }
         else
         {
#ifdef HAVE_STATX
            struct statx stat_buf;
#else
            struct stat stat_buf;
#endif

#ifdef LOCK_DEBUG
            lock_region_w(pwb_fd, 1, __FILE__, __LINE__);
#else
            lock_region_w(pwb_fd, 1);
#endif
#ifdef HAVE_STATX
            if (statx(pwb_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                      STATX_SIZE, &stat_buf) == -1)
#else
            if (fstat(pwb_fd, &stat_buf) == -1)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                          "Failed to statx() `%s' : %s",
#else
                          "Failed to fstat() `%s' : %s",
#endif
                          pwb_file_name, strerror(errno));
            }
            else
            {
#ifdef HAVE_STATX
               if (stat_buf.stx_size > AFD_WORD_OFFSET)
#else
               if (stat_buf.st_size > AFD_WORD_OFFSET)
#endif
               {
#ifdef HAVE_MMAP
                  if ((ptr = mmap(0,
# ifdef HAVE_STATX
                                  stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                                  stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                                  MAP_SHARED, pwb_fd, 0)) != (caddr_t) -1)
#else
                  if ((ptr = mmap_emu(0,
# ifdef HAVE_STATX
                                      stat_buf.stx_size,
# else
                                      stat_buf.st_size,
# endif
                                      (PROT_READ | PROT_WRITE), MAP_SHARED,
                                      pwb_file_name, 0)) != (caddr_t) -1)
#endif
                  {
                     int               j,
                                       *no_of_passwd,
                                       pw_removed = 0;
#ifdef HAVE_STATX
                     size_t            pwb_size = stat_buf.stx_size;
#else
                     size_t            pwb_size = stat_buf.st_size;
#endif
                     struct passwd_buf *pwb;

                     no_of_passwd = (int *)ptr;
                     ptr += AFD_WORD_OFFSET;
                     pwb = (struct passwd_buf *)ptr;

                     for (j = 0; j < pwb_to_remove; j++)
                     {
                        for (k = 0; k < *no_of_passwd; k++)
                        {
                           if (CHECK_STRCMP(pwb[k].uh_name, rpl[j]) == 0)
                           {
                              if ((*no_of_passwd > 1) &&
                                  ((k + 1) < *no_of_passwd))
                              {
                                 size_t move_size = (*no_of_passwd - (k + 1)) * sizeof(struct passwd_buf);

                                 (void)memmove(&pwb[k], &pwb[k + 1], move_size);
                              }
                              pw_removed++;
                              (*no_of_passwd)--;
                              break;
                           }
                        }
                     }
                     ptr = (char *)pwb - AFD_WORD_OFFSET;
                     if (pw_removed > 0)
                     {
                        /* If necessary resize the password buffer file. */
                        pwb_size = (((*no_of_passwd / PWB_STEP_SIZE) + 1) *
                                   PWB_STEP_SIZE * sizeof(struct passwd_buf)) +
                                   AFD_WORD_OFFSET;
#ifdef HAVE_STATX
                        if (pwb_size != stat_buf.stx_size)
#else
                        if (pwb_size != stat_buf.st_size)
#endif
                        {
                           if ((ptr = mmap_resize(pwb_fd, ptr,
                                                  pwb_size)) == (caddr_t) -1)
                           {
                              system_log(FATAL_SIGN, __FILE__, __LINE__,
                                         "Failed to mmap_resize() `%s' : %s",
                                         pwb_file_name, strerror(errno));
                              exit(INCORRECT);
                           }
                        }
                        system_log(DEBUG_SIGN, NULL, 0,
                                   "Removed %d password(s).", pw_removed);
                     }
#ifdef HAVE_MMAP
                     if (munmap(ptr, pwb_size) == -1)
#else
                     if (munmap_emu((void *)ptr) == -1)
#endif
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   "Failed to munmap() `%s' : %s",
                                   pwb_file_name, strerror(errno));
                     }
                  }
                  else
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Failed to mmap() `%s' : %s",
                                pwb_file_name, strerror(errno));
                  }
               }
               else
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "File `%s' is not large enough (%d bytes) to contain any valid data.",
#ifdef HAVE_STATX
                             pwb_file_name, stat_buf.stx_size
#else
                             pwb_file_name, stat_buf.st_size
#endif
                            );
               }
            }
            if (close(pwb_fd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Failed to close() `%s' : %s",
                          pwb_file_name, strerror(errno));
            }
         }
      }
      FREE_RT_ARRAY(rpl);

      /* Remove any unused DIR_CONFIG ID's. */
      if ((dc_id_to_remove - dcid_still_in_jid) > 0)
      {
         int  dc_id_fd;
         char dc_id_name[MAX_PATH_LENGTH];

         (void)strcpy(dc_id_name, p_work_dir);
         (void)strcat(dc_id_name, FIFO_DIR);
         (void)strcat(dc_id_name, DC_LIST_FILE);
         if ((dc_id_fd = open(dc_id_name, O_RDWR)) == -1)
         {
            if (errno != ENOENT)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to open() `%s' : %s",
                          dc_id_name, strerror(errno));
            }
         }
         else
         {
#ifdef HAVE_STATX
            struct statx stat_buf;
#else
            struct stat stat_buf;
#endif

#ifdef LOCK_DEBUG
            lock_region_w(dc_id_fd, 0, __FILE__, __LINE__);
#else
            lock_region_w(dc_id_fd, 0);
#endif
#ifdef HAVE_STATX
            if (statx(dc_id_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                      STATX_SIZE, &stat_buf) == -1)
#else
            if (fstat(dc_id_fd, &stat_buf) == -1)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                          "Failed to statx() `%s' : %s",
#else
                          "Failed to fstat() `%s' : %s",
#endif
                          dc_id_name, strerror(errno));
            }
            else
            {
#ifdef HAVE_STATX
               if (stat_buf.stx_size > AFD_WORD_OFFSET)
#else
               if (stat_buf.st_size > AFD_WORD_OFFSET)
#endif
               {
#ifdef HAVE_MMAP
                  if ((ptr = mmap(0,
# ifdef HAVE_STATX
                                  stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                                  stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                                  MAP_SHARED, dc_id_fd, 0)) != (caddr_t) -1)
#else
                  if ((ptr = mmap_emu(0,
# ifdef HAVE_STATX
                                      stat_buf.stx_size,
# else
                                      stat_buf.st_size,
# endif
                                      (PROT_READ | PROT_WRITE), MAP_SHARED,
                                      dc_id_name, 0)) != (caddr_t) -1)
#endif
                  {
                     int                    j,
                                            *no_of_dc_ids,
                                            dc_removed = 0;
#ifdef HAVE_STATX
                     size_t                 dcid_size = stat_buf.stx_size;
#else
                     size_t                 dcid_size = stat_buf.st_size;
#endif
                     struct dir_config_list *dcl;

                     no_of_dc_ids = (int *)ptr;
                     ptr += AFD_WORD_OFFSET;
                     dcl = (struct dir_config_list *)ptr;

                     for (j = 0; j < dc_id_to_remove; j++)
                     {
                        for (k = 0; k < *no_of_dc_ids; k++)
                        {
                           if (dcidr[j] == dcl[k].dc_id)
                           {
                              if ((*no_of_dc_ids > 1) &&
                                  ((k + 1) < *no_of_dc_ids))
                              {
                                 size_t move_size = (*no_of_dc_ids - (k + 1)) * sizeof(struct dir_config_list);

                                 (void)memmove(&dcl[k], &dcl[k + 1], move_size);
                              }
                              dc_removed++;
                              (*no_of_dc_ids)--;
                              break;
                           }
                        }
                     }
                     ptr = (char *)dcl - AFD_WORD_OFFSET;
                     if (dc_removed > 0)
                     {
                        /* If necessary resize the password buffer file. */
                        dcid_size = (*no_of_dc_ids * sizeof(struct dir_config_list)) +
                                    AFD_WORD_OFFSET;
#ifdef HAVE_STATX
                        if (dcid_size != stat_buf.stx_size)
#else
                        if (dcid_size != stat_buf.st_size)
#endif
                        {
                           if ((ptr = mmap_resize(dc_id_fd, ptr,
                                                  dcid_size)) == (caddr_t) -1)
                           {
                              system_log(FATAL_SIGN, __FILE__, __LINE__,
                                         "Failed to mmap_resize() `%s' : %s",
                                         dc_id_name, strerror(errno));
                              exit(INCORRECT);
                           }
                        }
                        system_log(DEBUG_SIGN, NULL, 0,
                                   "Removed %d DIR_CONFIG ID's.", dc_removed);
                     }
#ifdef HAVE_MMAP
                     if (munmap(ptr, dcid_size) == -1)
#else
                     if (munmap_emu((void *)ptr) == -1)
#endif
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   "Failed to munmap() `%s' : %s",
                                   dc_id_name, strerror(errno));
                     }
                  }
                  else
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Failed to mmap() `%s' : %s",
                                dc_id_name, strerror(errno));
                  }
               }
               else
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "File `%s' is not large enough (%d bytes) to contain any valid data.",
#ifdef HAVE_STATX
                             dc_id_name, stat_buf.stx_size
#else
                             dc_id_name, stat_buf.st_size
#endif
                            );
               }
            }
            if (close(dc_id_fd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Failed to close() `%s' : %s",
                          dc_id_name, strerror(errno));
            }
         }
      }
      free(dcidr);

      /* Remove any unused file masks. */
      if (file_mask_to_remove > 0)
      {
         int  fmd_fd;
         char fmd_file_name[MAX_PATH_LENGTH];

         (void)strcpy(fmd_file_name, p_work_dir);
         (void)strcat(fmd_file_name, FIFO_DIR);
         (void)strcat(fmd_file_name, FILE_MASK_FILE);
         if ((fmd_fd = open(fmd_file_name, O_RDWR)) == -1)
         {
            if (errno != ENOENT)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to open() `%s' : %s",
                          fmd_file_name, strerror(errno));
            }
         }
         else
         {
#ifdef HAVE_STATX
            struct statx stat_buf;
#else
            struct stat stat_buf;
#endif

#ifdef LOCK_DEBUG
            lock_region_w(fmd_fd, 0, __FILE__, __LINE__);
#else
            lock_region_w(fmd_fd, 0);
#endif
#ifdef HAVE_STATX
            if (statx(fmd_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                      STATX_SIZE, &stat_buf) == -1)
#else
            if (fstat(fmd_fd, &stat_buf) == -1)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                          "Failed to statx() `%s' : %s",
#else
                          "Failed to fstat() `%s' : %s",
#endif
                          fmd_file_name, strerror(errno));
            }
            else
            {
#ifdef HAVE_STATX
               if (stat_buf.stx_size > AFD_WORD_OFFSET)
#else
               if (stat_buf.st_size > AFD_WORD_OFFSET)
#endif
               {
#ifdef HAVE_MMAP
                  if ((ptr = mmap(0,
# ifdef HAVE_STATX
                                  stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                                  stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                                  MAP_SHARED, fmd_fd, 0)) != (caddr_t) -1)
#else
                  if ((ptr = mmap_emu(0,
# ifdef HAVE_STATX
                                      stat_buf.stx_size,
# else
                                      stat_buf.st_size,
# endif
                                      (PROT_READ | PROT_WRITE), MAP_SHARED,
                                      fmd_file_name, 0)) != (caddr_t) -1)
#endif
                  {
                     int    file_mask_removed = 0,
                            fml_offset,
                            j,
                            mask_offset,
                            *no_of_file_mask_ids;
#ifdef HAVE_STATX
                     size_t original_size = stat_buf.stx_size - AFD_WORD_OFFSET,
#else
                     size_t original_size = stat_buf.st_size - AFD_WORD_OFFSET,
#endif
                            remove_size,
                            size_removed = 0;
                     char   *fmd;

                     no_of_file_mask_ids = (int *)ptr;
                     ptr += AFD_WORD_OFFSET;
                     fmd = ptr;
                     fml_offset = sizeof(int) + sizeof(int);
                     mask_offset = fml_offset + sizeof(int) +
                                   sizeof(unsigned int) + sizeof(unsigned char);

                     for (j = 0; j < file_mask_to_remove; j++)
                     {
                        ptr = fmd;
                        for (k = 0; k < *no_of_file_mask_ids; k++)
                        {
                           if (*(unsigned int *)(ptr + fml_offset +
                                                 sizeof(int)) == fmtrl[j])
                           {
                              remove_size = (mask_offset +
                                             *(int *)(ptr + fml_offset) +
                                             sizeof(char) +
                                             *(ptr + mask_offset - 1));
                              if ((*no_of_file_mask_ids > 1) &&
                                  ((k + 1) < *no_of_file_mask_ids))
                              {
                                 size_t move_size;
                                 char   *next;

                                 next = ptr + remove_size;
                                 move_size = (original_size - size_removed) -
                                             (next - fmd);
                                 (void)memmove(ptr, next, move_size);
                              }
                              size_removed += remove_size;
                              file_mask_removed++;
                              (*no_of_file_mask_ids)--;
                              break;
                           }
                           ptr += (mask_offset + *(int *)(ptr + fml_offset) +
                                   sizeof(char) + *(ptr + mask_offset - 1));
                           if ((ptr - fmd) >= (original_size - size_removed))
                           {
                              if ((k + 1) != *no_of_file_mask_ids)
                              {
                                 system_log(WARN_SIGN, __FILE__, __LINE__,
                                            "The number of file mask is to large %d, changing to %d.",
                                            *no_of_file_mask_ids, k);
                                 *no_of_file_mask_ids = k;
                              }
                              else if ((ptr - fmd) > (original_size - size_removed))
                                   {
                                      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                 "Hmmm, something is wrong here (k=%d *no_of_file_mask_ids=%d  diff1=%d diff2=%d).",
                                                 k, *no_of_file_mask_ids,
                                                 (ptr - fmd),
                                                 (original_size - size_removed));
                                      break;
                                   }
                           }
                        }
                     }
                     ptr = fmd - AFD_WORD_OFFSET;
                     if (size_removed > 0)
                     {
                        if ((ptr = mmap_resize(fmd_fd, ptr,
                                               original_size + AFD_WORD_OFFSET - size_removed)) == (caddr_t) -1)
                        {
                           system_log(FATAL_SIGN, __FILE__, __LINE__,
                                      "Failed to mmap_resize() `%s' : %s",
                                      fmd_file_name, strerror(errno));
                           exit(INCORRECT);
                        }
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   "Removed %d file masks.", file_mask_removed);
                     }
#ifdef HAVE_MMAP
                     if (msync(ptr, (original_size + AFD_WORD_OFFSET - size_removed), MS_SYNC) == -1)
#else
                     if (msync_emu(ptr) == -1)
#endif
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   "Failed to msync() `%s' : %s",
                                   fmd_file_name, strerror(errno));
                     }
#ifdef HAVE_MMAP
                     if (munmap(ptr, (original_size + AFD_WORD_OFFSET - size_removed)) == -1)
#else
                     if (munmap_emu((void *)ptr) == -1)
#endif
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   "Failed to munmap() `%s' : %s",
                                   fmd_file_name, strerror(errno));
                     }
                  }
                  else
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Failed to mmap() `%s' : %s",
                                fmd_file_name, strerror(errno));
                  }
               }
               else
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "File `%s' is not large enough (%d bytes) to contain any valid data.",
#ifdef HAVE_STATX
                             fmd_file_name, stat_buf.stx_size
#else
                             fmd_file_name, stat_buf.st_size
#endif
                            );
               }
            }
            if (close(fmd_fd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Failed to close() `%s' : %s",
                          fmd_file_name, strerror(errno));
            }
         }
         free(fmtrl);
         fmtrl = NULL;
         file_mask_to_remove = 0;
      }

#ifdef LOCK_DEBUG
      unlock_region(jd_fd, 1, __FILE__, __LINE__);
#else
      unlock_region(jd_fd, 1);
#endif
#ifdef _DEBUG
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "no_of_job_ids = %d", *no_of_job_ids);
#endif
      free(rjl);
      rjl = NULL;
      removed_jobs = 0;
   }
   return;
}


/*+++++++++++++++++++++++++++++ sort_array() ++++++++++++++++++++++++++++*/
/*                              ------------                             */
/* Description: Heapsort found in linux kernel mailing list from         */
/*              Jamie Lokier.                                            */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void
sort_array(int count)
{
   int          i, j, k;
   unsigned int tmp;

   for (i = 1; i < count; i++)
   {
      j = i;
      tmp = rjl[j];
      while ((j > 0) && (tmp > rjl[(j - 1) / 2]))
      {
         rjl[j] = rjl[(j - 1) / 2];
         j = (j - 1) / 2;
      }
      rjl[j] = tmp;
   }
   for (i = (count - 1); i > 0; i--)
   {
      j = 0;
      k = 1;
      tmp = rjl[i];
      rjl[i] = rjl[0];
      while ((k < i) &&
             ((tmp < rjl[k]) || (((k + 1) < i) && (tmp < rjl[k + 1]))))
      {
         k += (((k + 1) < i) && (rjl[k + 1] > rjl[k]));
         rjl[j] = rjl[k];
         j = k;
         k = 2 * j + 1;
      }
      rjl[j] = tmp;
   }

   return;
}
