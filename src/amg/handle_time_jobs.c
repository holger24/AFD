/*
 *  handle_time_jobs.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   handle_time_jobs - process all time jobs of AMG
 **
 ** SYNOPSIS
 **   void handle_time_jobs(time_t now);
 **
 ** DESCRIPTION
 **   The function handle_time_jobs() searches the time directory for
 **   jobs that have to be distributed. After handling them it also
 **   calculates the next time for each job.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   26.04.1999 H.Kiehl Created
 **   24.09.2004 H.Kiehl Added split job counter.
 **
 */
DESCR__E_M3

#include <string.h>           /* strerror(), strcpy()                    */
#include <stdlib.h>           /* strtoul()                               */
#include <time.h>             /* time()                                  */
#include <ctype.h>            /* isdigit()                               */
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>           /* Definition of AT_* constants            */
#endif
#include <sys/stat.h>         /* S_ISDIR()                               */
#include <dirent.h>           /* opendir(), readdir(), closedir()        */
#include <unistd.h>           /* sleep(), pipe(), close()                */
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int                        *amg_counter,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                                  fin_writefd,
#else
                                  fin_fd,
#endif
                                  max_process,
                                  *no_of_process,
                                  no_of_time_jobs,
#ifndef MULTI_FS_SUPPORT
                                  outgoing_file_dir_length,
#endif
                                  *time_job_list;
#ifndef MULTI_FS_SUPPORT
extern char                       outgoing_file_dir[],
                                  *p_time_dir_id,
                                  time_dir[];
#endif
#ifndef _WITH_PTHREAD
extern char                       *file_name_buffer;
extern off_t                      *file_size_buffer;
#endif
#ifdef MULTI_FS_SUPPORT
extern struct extra_work_dirs     *ewl;
#endif
extern struct dc_proc_list        *dcpl;      /* Dir Check Process List. */
extern struct instant_db          *db;
extern struct directory_entry     *de;
extern struct filetransfer_status *fsa;
extern struct fileretrieve_status *fra,
                                  *p_fra;
extern struct afd_status          *p_afd_status;

/* Local variables. */
static unsigned int               files_handled;

/* Local function prototypes. */
#ifdef _WITH_PTHREAD
static void                       handle_time_dir(int, off_t *, time_t *,
                                                  char **, unsigned char *);
#else
static void                       handle_time_dir(int);
#endif

#define MAX_FILES_FOR_TIME_JOBS   800


/*######################### handle_time_jobs() ##########################*/
void
handle_time_jobs(time_t now)
{
   register int i;

   files_handled = 0;
   /*
    * First rescan structure to update all next_start_time values.
    */
   for (i = 0; i < no_of_time_jobs; i++)
   {
      if (db[time_job_list[i]].next_start_time <= now)
      {
         handle_time_dir(i);
         if (files_handled > MAX_FILES_FOR_TIME_JOBS)
         {
            break;
         }
         db[time_job_list[i]].next_start_time = calc_next_time_array(db[time_job_list[i]].no_of_time_entries,
                                                                     db[time_job_list[i]].te,
#ifdef WITH_TIMEZONE
                                                                     db[time_job_list[i]].timezone,
#endif
                                                                     now,
                                                                     __FILE__, __LINE__);
      }
   }

   return;
}


/*++++++++++++++++++++++++++ handle_time_dir() ++++++++++++++++++++++++++*/
static void
handle_time_dir(int time_job_no)
{
#ifdef MULTI_FS_SUPPORT
   int  outgoing_file_dir_length;
   char *outgoing_file_dir,
        *time_dir;
#endif
   char *time_dir_ptr;
   DIR  *dp;

#ifdef MULTI_FS_SUPPORT
   outgoing_file_dir_length = ewl[db[time_job_list[time_job_no]].ewl_pos].outgoing_file_dir_length;
   outgoing_file_dir = ewl[db[time_job_list[time_job_no]].ewl_pos].outgoing_file_dir;
   time_dir = ewl[db[time_job_list[time_job_no]].ewl_pos].time_dir;
   time_dir_ptr = ewl[db[time_job_list[time_job_no]].ewl_pos].p_time_dir_id;
#else
   time_dir_ptr = p_time_dir_id;
#endif
   /*
    * Now search time job directory and see if there are any files to
    * be processed.
    */
   (void)strcpy(time_dir_ptr, db[time_job_list[time_job_no]].str_job_id);

   if ((dp = opendir(time_dir)) == NULL)
   {
      if (errno != ENOENT)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Can't access directory %s : %s",
                    time_dir, strerror(errno));
      }
   }
   else
   {
      int            files_moved,
                     ret;
      off_t          file_size_moved;
      time_t         creation_time = 0; /* Silence compiler. */
      int            unique_number;
      unsigned int   split_job_counter;
      char           dest_file_path[MAX_PATH_LENGTH],
                     *p_dest,
                     *p_dest_end = NULL,
                     *p_src,
                     unique_name[MAX_PATH_LENGTH];
      struct dirent  *p_dir = NULL;
#ifdef HAVE_STATX
      struct statx   stat_buf;
#else
      struct stat    stat_buf;
#endif

      unique_name[0] = '/';
      p_src = time_dir + strlen(time_dir);
      *p_src++ = '/';
      do
      {
         files_moved = 0;
         file_size_moved = 0;
         p_dest = NULL;

         while ((files_moved < fra[db[time_job_list[time_job_no]].fra_pos].max_copied_files) &&
                ((p_dir = readdir(dp)) != NULL))
         {
            if (p_dir->d_name[0] == '.')
            {
               continue;
            }

            (void)strcpy(p_src, p_dir->d_name);
#ifdef HAVE_STATX
            if (statx(0, time_dir, AT_STATX_SYNC_AS_STAT,
                      STATX_SIZE, &stat_buf) == -1)
#else
            if (stat(time_dir, &stat_buf) == -1)
#endif
            {
               if (errno != ENOENT)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Failed to stat() %s : %s", strerror(errno));
               }
            }
            else
            {
               if (p_dest == NULL)
               {
                  if (db[time_job_list[time_job_no]].loptions != NULL)
                  {
                     /* Create a new message name and directory. */
                     creation_time = time(NULL);
                     (void)strcpy(dest_file_path, outgoing_file_dir);
                     split_job_counter = 0;
                     next_counter_no_lock(amg_counter, MAX_MSG_PER_SEC);
                     unique_number = *amg_counter;
                     if (create_name(dest_file_path,
                                     outgoing_file_dir_length,
                                     db[time_job_list[time_job_no]].priority,
                                     creation_time,
                                     db[time_job_list[time_job_no]].job_id,
                                     &split_job_counter, &unique_number,
                                     &unique_name[1], MAX_PATH_LENGTH - 1,
                                     -1) < 0)
                     {
                        if (errno == ENOSPC)
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      "DISK FULL!!! Will retry in %d second interval.",
                                      DISK_FULL_RESCAN_TIME);

                           while (errno == ENOSPC)
                           {
                              (void)sleep(DISK_FULL_RESCAN_TIME);
                              creation_time = time(NULL);
                              errno = 0;
                              split_job_counter = 0;
                              next_counter_no_lock(amg_counter, MAX_MSG_PER_SEC);
                              unique_number = *amg_counter;
                              if (create_name(dest_file_path,
                                              outgoing_file_dir_length,
                                              db[time_job_list[time_job_no]].priority,
                                              creation_time,
                                              db[time_job_list[time_job_no]].job_id,
                                              &split_job_counter,
                                              &unique_number,
                                              &unique_name[1],
                                              MAX_PATH_LENGTH - 1,
                                              -1) < 0)
                              {
                                 if (errno != ENOSPC)
                                 {
                                    system_log(FATAL_SIGN, __FILE__, __LINE__,
                                               "Failed to create a unique name : %s",
                                               strerror(errno));
                                    exit(INCORRECT);
                                 }
                              }
                           }

                           system_log(INFO_SIGN, __FILE__, __LINE__,
                                      "Continuing after disk was full.");
                        }
                        else
                        {
                           system_log(FATAL_SIGN, __FILE__, __LINE__,
                                      "Failed to create a unique name : %s",
                                      strerror(errno));
                           exit(INCORRECT);
                        }
                     }
                     p_dest_end = dest_file_path + strlen(dest_file_path);
                     (void)strcpy(p_dest_end, unique_name);
                     p_dest = p_dest_end + strlen(unique_name);
                     *(p_dest++) = '/';
                     *p_dest = '\0';
                  }
                  else
                  {
                     int dir_no;

                     if ((dir_no = get_dir_number(dest_file_path,
                                                  db[time_job_list[time_job_no]].job_id,
                                                  NULL)) == INCORRECT)
                     {
                        if (p_dest_end != NULL)
                        {
                           *p_dest_end = '\0';
                        }
                        return;
                     }
                     creation_time = time(NULL);
                     p_dest_end = dest_file_path + strlen(dest_file_path);
                     if (*(p_dest_end - 1) == '/')
                     {
                        p_dest_end--;
                     }
                     (void)snprintf(unique_name, MAX_PATH_LENGTH,
#if SIZEOF_TIME_T == 4
                                    "%x/%x/%lx_%x_%x",
#else
                                    "%x/%x/%llx_%x_%x",
#endif
                                    db[time_job_list[time_job_no]].job_id,
                                    dir_no, (pri_time_t)creation_time,
                                    unique_number, split_job_counter);
                     p_dest = p_dest_end +
                              snprintf(p_dest_end,
                                       MAX_PATH_LENGTH - (p_dest_end - dest_file_path),
                                       "/%s/", unique_name);
                     if (mkdir(dest_file_path, DIR_MODE) == -1)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "Failed to create directory %s : %s",
                                   dest_file_path, strerror(errno));
                        if (p_dest_end != NULL)
                        {
                           *p_dest_end = '\0';
                        }
                        return;
                     }
                  }
               }

               (void)strcpy(p_dest, p_dir->d_name);
               if (((ret = move_file(time_dir, dest_file_path)) < 0) ||
                   (ret == 2))
               {
                  char reason_str[23];

                  if (errno == ENOENT)
                  {
                     int          tmp_errno = errno;
                     char         tmp_char = *p_dest;
#ifdef HAVE_STATX
                     struct statx tmp_stat_buf;
#else
                     struct stat  tmp_stat_buf;
#endif

                     *p_dest = '\0';
#ifdef HAVE_STATX
                     if ((statx(0, time_dir, AT_STATX_SYNC_AS_STAT,
                                0, &tmp_stat_buf) == -1) &&
                         (errno == ENOENT))
#else
                     if ((stat(time_dir, &tmp_stat_buf) == -1) &&
                         (errno == ENOENT))
#endif
                     {
                        (void)strcpy(reason_str, "(source missing) ");
                     }
#ifdef HAVE_STATX
                     else if ((statx(0, dest_file_path,
                                     AT_STATX_SYNC_AS_STAT,
                                     0, &tmp_stat_buf) == -1) &&
                              (errno == ENOENT))
#else
                     else if ((stat(dest_file_path, &tmp_stat_buf) == -1) &&
                              (errno == ENOENT))
#endif
                          {
                             (void)strcpy(reason_str, "(destination missing) ");
                          }
                          else
                          {
                             reason_str[0] = '\0';
                          }

                     *p_dest = tmp_char;
                     errno = tmp_errno;
                  }
                  else
                  {
                     reason_str[0] = '\0';
                  }
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Failed to move file %s to %s %s: %s",
                             time_dir, dest_file_path, reason_str,
                             strerror(errno));
               }
               else
               {
#ifndef _WITH_PTHREAD
                  if ((files_moved % FILE_NAME_STEP_SIZE) == 0)
                  {
                     size_t new_size;

                     /* Calculate new size of file name buffer. */
                     new_size = ((files_moved / FILE_NAME_STEP_SIZE) + 1) * FILE_NAME_STEP_SIZE * MAX_FILENAME_LENGTH;

                     /* Increase the space for the file name buffer. */
                     if ((file_name_buffer = realloc(file_name_buffer, new_size)) == NULL)
                     {
                        system_log(FATAL_SIGN, __FILE__, __LINE__,
                                   "Could not realloc() memory [%d bytes] : %s",
                                   new_size, strerror(errno));
                        exit(INCORRECT);
                     }

                     /* Calculate new size of file size buffer. */
                     new_size = ((files_moved / FILE_NAME_STEP_SIZE) + 1) * FILE_NAME_STEP_SIZE * sizeof(off_t);

                     /* Increase the space for the file size buffer. */
                     if ((file_size_buffer = realloc(file_size_buffer, new_size)) == NULL)
                     {
                        system_log(FATAL_SIGN, __FILE__, __LINE__,
                                   "Could not realloc() memory [%d bytes] : %s",
                                   new_size, strerror(errno));
                        exit(INCORRECT);
                     }
                  }
                  (void)strcpy((file_name_buffer + (files_moved * MAX_FILENAME_LENGTH)), p_dir->d_name);
# ifdef HAVE_STATX
                  file_size_buffer[files_moved] = stat_buf.stx_size;
# else
                  file_size_buffer[files_moved] = stat_buf.st_size;
# endif
#endif
                  files_handled++;
                  files_moved++;
#ifdef HAVE_STATX
                  file_size_moved += stat_buf.stx_size;
#else
                  file_size_moved += stat_buf.st_size;
#endif
               }
            }
         } /* while ((p_dir = readdir(dp)) != NULL) */

         if (files_moved > 0)
         {
            p_fra = &fra[db[time_job_list[time_job_no]].fra_pos];
            if ((db[time_job_list[time_job_no]].lfs & GO_PARALLEL) &&
                (*no_of_process < max_process) &&
                (fra[db[time_job_list[time_job_no]].fra_pos].no_of_process < fra[db[time_job_list[time_job_no]].fra_pos].max_process) &&
                (fsa[db[time_job_list[time_job_no]].position].host_status < 2) &&
                ((fsa[db[time_job_list[time_job_no]].position].special_flag & HOST_DISABLED) == 0))
            {
               int   pfd1[2],
                     pfd2[2];
               pid_t pid;

               if ((pipe(pfd1) < 0) || (pipe(pfd2) < 0))
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "pipe() error : %s", strerror(errno));
               }

               if ((pid = fork()) == -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to fork() : %s", strerror(errno));
                  if ((close(pfd1[0]) == -1) || (close(pfd1[1]) == -1) ||
                      (close(pfd2[0]) == -1) || (close(pfd2[1]) == -1))
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "close() error : %s", strerror(errno));
                  }
                  send_message(outgoing_file_dir,
#ifdef MULTI_FS_SUPPORT
                               ewl[db[time_job_list[time_job_no]].ewl_pos].dev,
#endif
                               unique_name, split_job_counter,
                               unique_number, creation_time,
                               time_job_list[time_job_no], 0,
                               files_moved, file_size_moved, YES);
               }
               else if (pid == 0) /* Child process. */
                    {
                       pid_t pid;

                       if (write(pfd2[1], "c", 1) != 1)
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "write() error : %s", strerror(errno));
                       }
                       if (read(pfd1[0], &pid, 1) != 1)
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "read() error : %s", strerror(errno));
                       }
                       if ((close(pfd1[0]) == -1) || (close(pfd1[1]) == -1) ||
                           (close(pfd2[0]) == -1) || (close(pfd2[1]) == -1))
                       {
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     "close() error : %s", strerror(errno));
                       }

                       send_message(outgoing_file_dir,
#ifdef MULTI_FS_SUPPORT
                                    ewl[db[time_job_list[time_job_no]].ewl_pos].dev,
#endif
                                    unique_name,
                                    split_job_counter, unique_number,
                                    creation_time, time_job_list[time_job_no],
                                    0, files_moved, file_size_moved, YES);

                       /* Tell parent we completed our task. */
                       pid = getpid();
#ifdef WITHOUT_FIFO_RW_SUPPORT
                       if (write(fin_writefd, &pid, sizeof(pid_t)) != sizeof(pid_t))
#else
                       if (write(fin_fd, &pid, sizeof(pid_t)) != sizeof(pid_t))
#endif
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "Could not write() to fifo %s : %s",
                                     IP_FIN_FIFO, strerror(errno));
                       }
                       exit(SUCCESS);
                    }
                    else /* Parent process. */
                    {
                       char c;

                       if (write(pfd1[1], "p", 1) != 1)
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "write() error : %s", strerror(errno));
                       }
                       if (read(pfd2[0], &c, 1) != 1)
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "read() error : %s", strerror(errno));
                       }
                       if ((close(pfd1[0]) == -1) || (close(pfd1[1]) == -1) ||
                           (close(pfd2[0]) == -1) || (close(pfd2[1]) == -1))
                       {
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     "close() error : %s", strerror(errno));
                       }

                       dcpl[*no_of_process].pid = pid;
                       dcpl[*no_of_process].fra_pos = db[time_job_list[time_job_no]].fra_pos;
                       dcpl[*no_of_process].job_id = db[time_job_list[time_job_no]].job_id;
                       fra[db[time_job_list[time_job_no]].fra_pos].no_of_process++;
                       (*no_of_process)++;
                       p_afd_status->amg_fork_counter++;
                    }
            }
            else
            {
               send_message(outgoing_file_dir,
#ifdef MULTI_FS_SUPPORT
                            ewl[db[time_job_list[time_job_no]].ewl_pos].dev,
#endif
                            unique_name, split_job_counter,
                            unique_number, creation_time,
                            time_job_list[time_job_no], 0,
                            files_moved, file_size_moved, YES);
            }
         } /* if (files_moved > 0) */
      } while ((p_dir != NULL) && (files_handled < MAX_FILES_FOR_TIME_JOBS));

      if (closedir(dp) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to closedir() %s : %s", time_dir, strerror(errno));
      }
      if (p_dest_end != NULL)
      {
         *p_dest_end = '\0';
      }
   }
   *time_dir_ptr = '\0';

   return;
}
