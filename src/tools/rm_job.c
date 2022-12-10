/*
 *  rm_job.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   rm_job - removes job(s) from the internal AFD queue
 **
 ** SYNOPSIS
 **   rm_job [-w <AFD work dir>] [--version] <job 1> [... <job n>]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   Returns INCORRECT (-1) when it fails to send the remove command.
 **   Otherwise SUCCESS (0) is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.05.1998 H.Kiehl Created
 **   27.06.1998 H.Kiehl Remove directory information from job name.
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>                 /* exit()                            */
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>                 /* rmdir()                           */
#include <sys/mman.h>               /* mmap()                            */
#include <dirent.h>                 /* opendir(), closedir(), readdir(), */
                                    /* DIR, struct dirent                */
#include <fcntl.h>                  /* open()                            */
#include <unistd.h>                 /* write()                           */
#include <errno.h>
#include "fddefs.h"
#include "version.h"

/* Global variables. */
int                        event_log_fd = STDERR_FILENO,
                           fsa_fd = -1,
                           fsa_id,
                           no_of_hosts,
                           *no_msg_cached,
                           *no_msg_queued,
                           sys_log_fd = STDERR_FILENO;
#ifdef HAVE_MMAP
off_t                      fsa_size;
#endif
char                       *p_work_dir;
struct afd_status          *p_afd_status;
struct queue_buf           *qb = NULL;
struct msg_cache_buf       *mdb;
struct filetransfer_status *fsa;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void                attach_to_queue_buffer(void),
                           remove_job(char *, int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int    fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
   int    readfd;
#endif
   size_t length;
   char   *p_job_name,
          delete_fifo[MAX_PATH_LENGTH],
          file_dir[MAX_PATH_LENGTH],
          work_dir[MAX_PATH_LENGTH];

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD. */
   if (get_afd_path(&argc, argv, work_dir) < 0) 
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if (argc < 2)
   {
      (void)fprintf(stderr,
                    "Usage: %s [-w <AFD work dir>] [--version] <job 1> [... <job n>]\n",
                    argv[0]);
      exit(INCORRECT);
   }

   /* Attach to FSA and the AFD Status Area. */
   if ((fd = fsa_attach("rm_job")) != SUCCESS)
   {
      if (fd == INCORRECT_VERSION)
      {
         (void)fprintf(stderr, "This program is not able to attach to the FSA due to incorrect version. (%s %d)\n",
                       __FILE__, __LINE__);
      }
      else
      {
         if (fd < 0)
         {
            (void)fprintf(stderr, "Failed to attach to FSA. (%s %d)\n",
                          __FILE__, __LINE__);
         }
         else
         {
            (void)fprintf(stderr, "Failed to attach to FSA : %s (%s %d)\n",
                          strerror(fd), __FILE__, __LINE__);
         }
      }
      exit(INCORRECT);
   }
   if (attach_afd_status(NULL, WAIT_AFD_STATUS_ATTACH) < 0)
   {
      (void)fprintf(stderr,
                    "Failed to map to AFD status area. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   (void)sprintf(file_dir, "%s%s%s/", p_work_dir, AFD_FILE_DIR, OUTGOING_DIR);
   (void)strcpy(delete_fifo, work_dir);
   (void)strcat(delete_fifo, FIFO_DIR);
   (void)strcat(delete_fifo, FD_DELETE_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(delete_fifo, &readfd, &fd) == -1)
#else
   if ((fd = open(delete_fifo, O_RDWR)) == -1)
#endif
   {
      (void)fprintf(stderr, "Failed to open %s : %s (%s %d)\n",
                    delete_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   while (argc > 1)
   {
      p_job_name = argv[1];
      if (is_msgname(p_job_name) == SUCCESS)
      {
         length = strlen(p_job_name) + 1;

         if (p_afd_status->fd == ON)
         {
            char wbuf[MAX_MSG_NAME_LENGTH + 1];

            wbuf[0] = DELETE_MESSAGE;
            (void)memcpy(&wbuf[1], p_job_name, length);
            if (write(fd, wbuf, (length + 1)) != (length + 1))
            {
               (void)fprintf(stderr, "write() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
         }
         else /* FD is currently not active. */
         {
            int i;

            if (qb == NULL)
            {
               attach_to_queue_buffer();
            }

            for (i = 0; i < *no_msg_queued; i++)
            {
               if (my_strcmp(qb[i].msg_name, p_job_name) == 0)
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
                  remove_job(file_dir, mdb[qb[i].pos].fsa_pos);
                  *ptr = '\0';

                  if (i != (*no_msg_queued - 1))
                  {
                     size_t move_size;

                     move_size = (*no_msg_queued - 1 - i) * sizeof(struct queue_buf);
                     (void)memmove(&qb[i], &qb[i + 1], move_size);
                  }
                  (*no_msg_queued)--;
                  break;
               }
            }
         }
      }
      else
      {
         (void)fprintf(stderr, "%s is not an AFD job name!\n", p_job_name);
      }
      argv++;
      argc--;
   }
#ifdef WITHOUT_FIFO_RW_SUPPORT
   (void)close(readfd);
#endif
   (void)close(fd);

   exit(SUCCESS);
}


/*++++++++++++++++++++++++ attach_to_queue_buffer() +++++++++++++++++++++*/
static void
attach_to_queue_buffer(void)
{
   int          fd;
   char         file[MAX_PATH_LENGTH],
                *ptr;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   (void)sprintf(file, "%s%s%s", p_work_dir, FIFO_DIR, MSG_QUEUE_FILE);
   if ((fd = open(file, O_RDWR)) == -1)
   {
      (void)fprintf(stderr,
                    "Failed to open() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

#ifdef HAVE_STATX
   if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(fd, &stat_buf) == -1)
#endif
   {
      (void)fprintf(stderr,
                    "Failed to access %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if ((ptr = mmap(NULL,
#ifdef HAVE_STATX
                   stat_buf.stx_size, (PROT_READ | PROT_WRITE),
#else
                   stat_buf.st_size, (PROT_READ | PROT_WRITE),
#endif
                   MAP_SHARED, fd, 0)) == (caddr_t)-1)
   {
      (void)fprintf(stderr,
                    "Failed to mmap() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   no_msg_queued = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   qb = (struct queue_buf *)ptr;

   (void)sprintf(file, "%s%s%s", p_work_dir, FIFO_DIR, MSG_CACHE_FILE);
   if ((fd = open(file, O_RDWR)) == -1)
   {
      (void)fprintf(stderr,
                    "Failed to open() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

#ifdef HAVE_STATX
   if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(fd, &stat_buf) == -1)
#endif
   {
      (void)fprintf(stderr,
                    "Failed to access %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if ((ptr = mmap(NULL,
#ifdef HAVE_STATX
                   stat_buf.stx_size, (PROT_READ | PROT_WRITE),
#else
                   stat_buf.st_size, (PROT_READ | PROT_WRITE),
#endif
                   MAP_SHARED, fd, 0)) == (caddr_t)-1)
   {
      (void)fprintf(stderr,
                    "Failed to mmap() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   no_msg_cached = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   mdb = (struct msg_cache_buf *)ptr;

   return;
}


/*+++++++++++++++++++++++++++++ remove_job() ++++++++++++++++++++++++++++*/
static void
remove_job(char *del_dir, int fsa_pos)
{
   int           number_deleted = 0;
   off_t         file_size_deleted = 0;
   char          *ptr;
   DIR           *dp;
   struct dirent *p_dir;
#ifdef HAVE_STATX
   struct statx  stat_buf;
#else
   struct stat   stat_buf;
#endif

   if ((dp = opendir(del_dir)) == NULL)
   {
      (void)fprintf(stderr,
                    "Failed to opendir() %s : %s (%s %d)\n",
                    del_dir, strerror(errno), __FILE__, __LINE__);
      return;
   }
   ptr = del_dir + strlen(del_dir);
   *(ptr++) = '/';

   errno = 0;
   while ((p_dir = readdir(dp)) != NULL)
   {
      if (p_dir->d_name[0] == '.')
      {
         continue;
      }
      (void)strcpy(ptr, p_dir->d_name);

#ifdef HAVE_STATX
      if (statx(0, del_dir, AT_STATX_SYNC_AS_STAT,
                STATX_MODE | STATX_SIZE, &stat_buf) == -1)
#else
      if (stat(del_dir, &stat_buf) == -1)
#endif
      {
         (void)fprintf(stderr,
                       "Failed to access %s : %s (%s %d)\n",
                       del_dir, strerror(errno), __FILE__, __LINE__);
         if (unlink(del_dir) == -1)
         {
            (void)fprintf(stderr,
                          "Failed to unlink() file %s : %s (%s %d)\n",
                          del_dir, strerror(errno), __FILE__, __LINE__);
         }
      }
      else
      {
#ifdef HAVE_STATX
         if (!S_ISDIR(stat_buf.stx_mode))
#else
         if (!S_ISDIR(stat_buf.st_mode))
#endif
         {
            if (unlink(del_dir) == -1)
            {
               (void)fprintf(stderr,
                             "Failed to unlink() file %s : %s (%s %d)\n",
                             del_dir, strerror(errno), __FILE__, __LINE__);
            }
            else
            {
               number_deleted++;
#ifdef HAVE_STATX
               file_size_deleted += stat_buf.stx_size;
#else
               file_size_deleted += stat_buf.st_size;
#endif
            }
         }
      }
      errno = 0;
   }

   *(ptr - 1) = '\0';
   if (errno)
   {
      (void)fprintf(stderr,
                    "Could not readdir() `%s' : %s (%s %d)\n",
                    del_dir, strerror(errno), __FILE__, __LINE__);
   }
   if (closedir(dp) == -1)
   {
      (void)fprintf(stderr,
                    "Could not closedir() %s : %s (%s %d)\n",
                    del_dir, strerror(errno), __FILE__, __LINE__);
   }
   if (rmdir(del_dir) == -1)
   {
      (void)fprintf(stderr,
                    "Could not rmdir() %s : %s (%s %d)\n",
                    del_dir, strerror(errno), __FILE__, __LINE__);
   }

   if ((number_deleted > 0) && (fsa_pos != -1))
   {
#ifdef _VERIFY_FSA
      off_t off_t_variable;
#endif
      off_t        lock_offset;

      /* Total file counter. */
      lock_offset = AFD_WORD_OFFSET +
                    (fsa_pos * sizeof(struct filetransfer_status));
#ifdef LOCK_DEBUG
      lock_region_w(fsa_fd, lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
      lock_region_w(fsa_fd, lock_offset + LOCK_TFC);
#endif
      fsa[fsa_pos].total_file_counter -= number_deleted;
#ifdef _VERIFY_FSA
      if (fsa[fsa_pos].total_file_counter < 0)
      {
         (void)fprintf(stderr,
                       "Total file counter for host %s less then zero. Correcting. (%s %d)\n",
                       fsa[fsa_pos].host_dsp_name, __FILE__, __LINE__);
         fsa[fsa_pos].total_file_counter = 0;
      }
#endif

      /* Total file size. */
#ifdef _VERIFY_FSA
      off_t_variable = fsa[fsa_pos].total_file_size;
#endif
      fsa[fsa_pos].total_file_size -= file_size_deleted;
#ifdef _VERIFY_FSA
      if (fsa[fsa_pos].total_file_size > off_t_variable)
      {
         (void)fprintf(stderr,
                       "Total file size for host %s overflowed. Correcting. (%s %d)\n",
                       fsa[fsa_pos].host_dsp_name, __FILE__, __LINE__);
         fsa[fsa_pos].total_file_size = 0;
      }
      else if ((fsa[fsa_pos].total_file_counter == 0) &&
               (fsa[fsa_pos].total_file_size > 0))
           {
              (void)fprintf(stderr,
                            "fc for host %s is zero but fs is not zero. Correcting. (%s %d)\n",
                            fsa[fsa_pos].host_dsp_name, __FILE__, __LINE__);
              fsa[fsa_pos].total_file_size = 0;
           }
#endif
#ifdef LOCK_DEBUG
      unlock_region(fsa_fd, lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
      unlock_region(fsa_fd, lock_offset + LOCK_TFC);
#endif
   }

   return;
}
