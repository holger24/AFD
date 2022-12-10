/*
 *  delete_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   delete_files - deletes selected files from the AFD queue
 **
 ** SYNOPSIS
 **   void delete_files(int no_selected, int *select_list)
 **
 ** DESCRIPTION
 **   delete_files() will delete the given files from the AFD queue
 **   and remove them from the display list and qfl structure.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   29.07.2001 H.Kiehl Created
 **   06.05.2002 H.Kiehl Remove files and bytes from FRA queue data.
 **   03.01.2005 H.Kiehl Jobs are no longer stored in separete directories.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>         /* strerror(), strcpy(), strcat()            */
#include <stdlib.h>         /* calloc(), free()                          */
#include <unistd.h>         /* rmdir()                                   */
#include <time.h>           /* time()                                    */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>       /* mmap(), munmap()                          */
#include <dirent.h>         /* opendir(), readdir(), closedir()          */
#include <fcntl.h>
#include <errno.h>
#include <Xm/Xm.h>
#include <Xm/List.h>
#include "motif_common_defs.h"
#include "show_queue.h"
#include "fddefs.h"

/* External global variables. */
extern Display                    *display;
extern Widget                     listbox_w,
                                  statusbox_w;
extern int                        fra_fd,
#ifdef WITH_ERROR_QUEUE
                                  fsa_fd,
#endif
                                  no_of_dirs,
                                  queue_tmp_buf_entries;
extern XT_PTR_TYPE                toggles_set;
extern unsigned int               total_no_files;
extern double                     total_file_size;
extern char                       *p_work_dir,
                                  user[];
extern struct queued_file_list    *qfl;
extern struct queue_tmp_buf       *qtb;
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif
extern struct fileretrieve_status *fra;

/* Global variables. */
int                               fsa_fd = -1,
                                  fsa_id,
                                  no_of_hosts,
                                  counter_fd;
#ifdef HAVE_MMAP
off_t                             fsa_size;
#endif
struct filetransfer_status        *fsa;
struct afd_status                 *p_afd_status;

/* Local function prototypes. */
#ifdef WITHOUT_FIFO_RW_SUPPORT
static void                       check_fds(int *, int *);
#else
static void                       check_fds(int *);
#endif


/*############################ delete_files() ###########################*/
void
delete_files(int no_selected, int *select_list)
{
   int                 deleted,
                       fd,
                       fd_delete_fd = -1,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                       fd_delete_readfd = -1,
#endif
                       files_deleted = 0,
                       files_not_deleted = 0,
                       gotcha,
                       i,
                       j,
                       k,
                       *no_msg_queued = NULL,
                       skipped;
   size_t              length;
   off_t               dnb_size,
                       qb_size = 0;
   char                fullname[MAX_PATH_LENGTH + 1 + MAX_HOSTNAME_LENGTH + 1 + MAX_FILENAME_LENGTH + 1],
                       message[MAX_MESSAGE_LENGTH],
                       wbuf[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx        stat_buf;
#else
   struct stat         stat_buf;
#endif
   struct queue_buf    *qb = NULL;
   struct dir_name_buf *dnb;

   /* Map to directory name buffer. */
   (void)sprintf(fullname, "%s%s%s", p_work_dir, FIFO_DIR, DIR_NAME_FILE);
   if ((fd = open(fullname, O_RDONLY)) == -1)
   {
      (void)xrec(ERROR_DIALOG, "Failed to open() <%s> : %s (%s %d)",
                 fullname, strerror(errno), __FILE__, __LINE__);
      return;
   }
#ifdef HAVE_STATX
   if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(fd, &stat_buf) == -1)
#endif
   {
      (void)xrec(ERROR_DIALOG, "Failed to access <%s> : %s (%s %d)",
                 fullname, strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      return;
   }
#ifdef HAVE_STATX
   if (stat_buf.stx_size > 0)
#else
   if (stat_buf.st_size > 0)
#endif
   {
      char *ptr;

      if ((ptr = mmap(NULL,
#ifdef HAVE_STATX
                      stat_buf.stx_size, PROT_READ,
#else
                      stat_buf.st_size, PROT_READ,
#endif
                      MAP_SHARED, fd, 0)) == (caddr_t) -1)
      {
         (void)xrec(ERROR_DIALOG, "Failed to mmap() to <%s> : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return;
      }
#ifdef HAVE_STATX
      dnb_size = stat_buf.stx_size;
#else
      dnb_size = stat_buf.st_size;
#endif
      ptr += AFD_WORD_OFFSET;
      dnb = (struct dir_name_buf *)ptr;
      (void)close(fd);
   }
   else
   {
      (void)xrec(ERROR_DIALOG, "Dirname database file is empty. (%s %d)",
                 __FILE__, __LINE__);
      (void)close(fd);
      return;
   }

   if ((toggles_set & SHOW_OUTPUT) || (toggles_set & SHOW_RETRIEVES) ||
       (toggles_set & SHOW_PENDING_RETRIEVES))
   {
      /* Map to FD queue. */
      (void)sprintf(fullname, "%s%s%s", p_work_dir, FIFO_DIR,
                    MSG_QUEUE_FILE);
      if ((fd = open(fullname, O_RDWR)) == -1)
      {
         (void)xrec(ERROR_DIALOG, "Failed to open() <%s> : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
         return;
      }
#ifdef HAVE_STATX
      if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(fd, &stat_buf) == -1)
#endif
      {
         (void)xrec(ERROR_DIALOG, "Failed to access <%s> : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return;
      }
#ifdef HAVE_STATX
      if (stat_buf.stx_size > 0)
#else
      if (stat_buf.st_size > 0)
#endif
      {
         char *ptr;

         if ((ptr = mmap(NULL,
#ifdef HAVE_STATX
                         stat_buf.stx_size, PROT_READ | PROT_WRITE,
#else
                         stat_buf.st_size, PROT_READ | PROT_WRITE,
#endif
                         MAP_SHARED, fd, 0)) == (caddr_t) -1)
         {
            (void)xrec(ERROR_DIALOG, "Failed to mmap() to <%s> : %s (%s %d)",
                       fullname, strerror(errno), __FILE__, __LINE__);
            (void)close(fd);
            return;
         }
#ifdef HAVE_STATX
         qb_size = stat_buf.stx_size;
#else
         qb_size = stat_buf.st_size;
#endif
         no_msg_queued = (int *)ptr;
         ptr += AFD_WORD_OFFSET;
         qb = (struct queue_buf *)ptr;
         (void)close(fd);
      }
      else
      {
         (void)xrec(ERROR_DIALOG,
                    "Queue file is empty. (%s %d)", __FILE__, __LINE__);
         (void)close(fd);
         return;
      }

      /* Map to the FSA. */
      if ((i = fsa_attach(SHOW_QUEUE)) != SUCCESS)
      {
         if (i == INCORRECT_VERSION)
         {
            (void)fprintf(stderr, "This program is not able to attach to the FSA due to incorrect version.\n");
         }
         else
         {
            if (i < 0)
            {
               (void)fprintf(stderr, "Failed to attach to FSA.\n");
            }
            else
            {
               (void)fprintf(stderr, "Failed to attach to FSA : %s\n",
                             strerror(i));
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
   }

   if (toggles_set & SHOW_INPUT)
   {
      /* Map to the FRA. */
      if ((i = fra_attach()) != SUCCESS)
      {
         if (i == INCORRECT_VERSION)
         {
            (void)fprintf(stderr, "This program is not able to attach to the FRA due to incorrect version.\n");
         }
         else
         {
            if (i < 0)
            {
               (void)fprintf(stderr, "Failed to attach to FRA.\n");
            }
            else
            {
               (void)fprintf(stderr, "Failed to attach to FRA : %s\n",
                             strerror(i));
            }
         }
         exit(INCORRECT);
      }
   }

   for (i = 0; i < no_selected; i++)
   {
      if ((qfl[select_list[i] - 1].queue_type == SHOW_OUTPUT) &&
          (toggles_set & SHOW_OUTPUT))
      {
         if (qfl[select_list[i] - 1].queue_tmp_buf_pos != -1)
         {
            if ((qtb[qfl[select_list[i] - 1].queue_tmp_buf_pos].files_to_delete % 100) == 0)
            {
               size_t new_size = (((qtb[qfl[select_list[i] - 1].queue_tmp_buf_pos].files_to_delete / 100) + 1) *
                                  100) * sizeof(struct queue_tmp_buf);

               if (qtb[qfl[select_list[i] - 1].queue_tmp_buf_pos].files_to_delete == 0)
               {
                  if ((qtb[qfl[select_list[i] - 1].queue_tmp_buf_pos].qfl_pos = malloc(new_size)) == NULL)
                  {
                     (void)xrec(FATAL_DIALOG, "malloc() error : %s (%s %d)",
                                strerror(errno), __FILE__, __LINE__);
                     return;
                  }
               }
               else
               {
                  if ((qtb[qfl[select_list[i] - 1].queue_tmp_buf_pos].qfl_pos = realloc((char *)qtb[qfl[select_list[i] - 1].queue_tmp_buf_pos].qfl_pos, new_size)) == NULL)
                  {
                     (void)xrec(FATAL_DIALOG, "realloc() error : %s (%s %d)",
                                strerror(errno), __FILE__, __LINE__);
                     return;
                  }
               }
            }
            qtb[qfl[select_list[i] - 1].queue_tmp_buf_pos].qfl_pos[qtb[qfl[select_list[i] - 1].queue_tmp_buf_pos].files_to_delete] = select_list[i] - 1;
            qtb[qfl[select_list[i] - 1].queue_tmp_buf_pos].files_to_delete++;
         }
         deleted = NEITHER;
      }
      else if (qfl[select_list[i] - 1].queue_type == SHOW_UNSENT_OUTPUT)
           {
              /* Don't allow user to delete unsent files. */
              deleted = NO;
           }
      else if ((qfl[select_list[i] - 1].queue_type == SHOW_RETRIEVES) ||
               (qfl[select_list[i] - 1].queue_type == SHOW_PENDING_RETRIEVES))
           {
#ifdef WITHOUT_FIFO_RW_SUPPORT
              check_fds(&fd_delete_readfd, &fd_delete_fd);
#else
              check_fds(&fd_delete_fd);
#endif
              if (p_afd_status->fd == ON)
              {
                 wbuf[0] = DELETE_RETRIEVE;
                 length = 1 + sprintf(&wbuf[1], "%.0f %d",
                                      qfl[select_list[i] - 1].msg_number,
                                      qfl[select_list[i] - 1].pos) + 1;
                 if (write(fd_delete_fd, wbuf, length) != length)
                 {
                    (void)xrec(FATAL_DIALOG,
                               "Failed to write() to <%s> : %s (%s %d)",
                               FD_DELETE_FIFO, strerror(errno),
                               __FILE__, __LINE__);
                    return;
                 }
              }
              else
              {
                 int pos;

                 if ((pos = get_host_position(fsa,
                                              qfl[select_list[i] - 1].hostname,
                                              no_of_hosts)) != INCORRECT)
                 {
                    int gotcha,
                        qb_pos;

                    gotcha = NO;
                    for (qb_pos = (*no_msg_queued - 1); qb_pos > -1; qb_pos--)
                    {
                       if ((qb[qb_pos].msg_number == qfl[select_list[i] - 1].msg_number) &&
                           (qb[qb_pos].pos == qfl[select_list[i] - 1].pos) &&
                           (qb[qb_pos].msg_name[0] == '\0'))
                       {
                          if (qb[qb_pos].pid == PENDING)
                          {
                             gotcha = YES;
                          }
                          break;
                       }
                    }
                    if (gotcha == YES)
                    {
                       off_t lock_offset;

                       lock_offset = AFD_WORD_OFFSET +
                                     (pos * sizeof(struct filetransfer_status));
#ifdef LOCK_DEBUG
                       lock_region_w(fsa_fd, lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
                       lock_region_w(fsa_fd, lock_offset + LOCK_TFC);
#endif
                       fsa[pos].jobs_queued--;
                       if (qb_pos != (*no_msg_queued - 1))
                       {
                          size_t move_size;

                          move_size = (*no_msg_queued - 1 - qb_pos) * sizeof(struct queue_buf);
                          (void)memmove(&qb[qb_pos], &qb[qb_pos + 1], move_size);
                       }
                       (*no_msg_queued)--;
#ifdef LOCK_DEBUG
                       unlock_region(fsa_fd, lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
                       unlock_region(fsa_fd, lock_offset + LOCK_TFC);
#endif
                    }
                 }
              }
              deleted = NEITHER;
           }
      else if (qfl[select_list[i] - 1].queue_type == SHOW_TIME_JOBS)
           {
              (void)sprintf(fullname, "%s%s%s/%x/%s",
                            p_work_dir, AFD_FILE_DIR, AFD_TIME_DIR,
                            qfl[select_list[i] - 1].job_id,
                            qfl[select_list[i] - 1].file_name);
              if (unlink(fullname) == -1)
              {
                 deleted = NO;
              }
              else
              {
                 deleted = YES;
              }
           }
           else /* It's in one of the input queue's. */
           {
              if (qfl[select_list[i] - 1].hostname[0] == '\0')
              {
                 (void)sprintf(fullname, "%s/%s",
                               dnb[qfl[select_list[i] - 1].dir_id_pos].dir_name,
                               qfl[select_list[i] - 1].file_name);
              }
              else
              {
                 (void)sprintf(fullname, "%s/.%s/%s",
                               dnb[qfl[select_list[i] - 1].dir_id_pos].dir_name,
                               qfl[select_list[i] - 1].hostname,
                               qfl[select_list[i] - 1].file_name);
              }
              if (unlink(fullname) == -1)
              {
                 deleted = NO;
              }
              else
              {
                 if (qfl[select_list[i] - 1].hostname[0] != '\0')
                 {
                    int k;

                    for (k = 0; k < no_of_dirs; k++)
                    {
                       if (qfl[select_list[i] - 1].dir_id == fra[k].dir_id)
                       {
                          ABS_REDUCE_QUEUE(k, 1, qfl[select_list[i] - 1].size);
                          break;
	               }
	            }
                 }
                 deleted = YES;
              }
           }

      if (deleted == YES)
      {
#ifdef _DELETE_LOG
         int    prog_name_length;
         size_t dl_real_size;

         (void)strcpy(dl.file_name, (char *)qfl[select_list[i] - 1].file_name);
         if (qfl[select_list[i] - 1].hostname[0] == '\0')
         {
            (void)sprintf(dl.host_name, "%-*s %03x",
                          MAX_HOSTNAME_LENGTH, "-", USER_DEL);
         }
         else
         {
            (void)sprintf(dl.host_name, "%-*s %03x",
                          MAX_HOSTNAME_LENGTH, qfl[select_list[i] - 1].hostname,
                          USER_DEL);
         }
         *dl.file_size = qfl[select_list[i] - 1].size;
         *dl.dir_id = dnb[qfl[select_list[i] - 1].dir_id_pos].dir_id;
         *dl.job_id = qfl[select_list[i] - 1].job_id;
         *dl.input_time = 0L;
         *dl.split_job_counter = 0;
         *dl.unique_number = 0;
         *dl.file_name_length = strlen((char *)qfl[select_list[i] - 1].file_name);
         prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                    "%s %s", SHOW_QUEUE, user);
         dl_real_size = *dl.file_name_length + dl.size + prog_name_length;
         if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
         {
            (void)fprintf(stderr, "write() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
#endif
         files_deleted++;
      }
      else if (deleted == NO)
           {
              files_not_deleted++;
           }
   } /* for (i = 0; i < no_selected; i++) */

   for (i = 0; i < queue_tmp_buf_entries; i++)
   {
      if (qtb[i].files_to_delete > 0)
      {
         gotcha = NO;
         for (k = (*no_msg_queued - 1); k > -1; k--)
         {
            if (my_strcmp(qb[k].msg_name, qtb[i].msg_name) == 0)
            {
               if (qb[k].pid == PENDING)
               {
                  gotcha = YES;
               }
               break;
            }
         }
         if (gotcha == YES)
         {
            if (p_afd_status->fd == ON)
            {
#ifdef WITHOUT_FIFO_RW_SUPPORT
               check_fds(&fd_delete_readfd, &fd_delete_fd);
#else
               check_fds(&fd_delete_fd);
#endif
               if (qtb[i].files_to_send == qtb[i].files_to_delete)
               {
                  length = strlen(qtb[i].msg_name) + 1;
                  wbuf[0] = DELETE_MESSAGE;
                  (void)memcpy(&wbuf[1], qtb[i].msg_name, length);
                  if (write(fd_delete_fd, wbuf, (length + 1)) != (length + 1))
                  {
                     (void)xrec(FATAL_DIALOG,
                                "Failed to write() to <%s> : %s (%s %d)",
                                FD_DELETE_FIFO, strerror(errno),
                                __FILE__, __LINE__);
                     return;
                  }
               }
               else
               {
                  for (j = 0; j < qtb[i].files_to_delete; j++)
                  {
                     length = sprintf(&wbuf[1], "%s/%s", qtb[i].msg_name,
                                      qfl[qtb[i].qfl_pos[j]].file_name) + 2;
                     wbuf[0] = DELETE_SINGLE_FILE;
                     if (write(fd_delete_fd, wbuf, length) != length)
                     {
                        (void)xrec(FATAL_DIALOG,
                                   "Failed to write() to <%s> : %s (%s %d)",
                                   FD_DELETE_FIFO, strerror(errno),
                                   __FILE__, __LINE__);
                        return;
                     }
                  }
               }
               files_deleted += qtb[i].files_to_delete;
            }
            else
            {
               for (j = 0; j < qtb[i].files_to_delete; j++)
               {
                  length = sprintf(fullname, "%s%s%s/%s/%s",
                                   p_work_dir, AFD_FILE_DIR, OUTGOING_DIR,
                                   qtb[i].msg_name,
                                   qfl[qtb[i].qfl_pos[j]].file_name);
                  if (unlink(fullname) != -1)
                  {
                     if (qb[k].files_to_send > 0)
                     {
                        int    pos;
#ifdef _DELETE_LOG
                        int    prog_name_length;
                        size_t dl_real_size;
#endif

                        qb[k].files_to_send -= 1;
                        qb[k].file_size_to_send -= qfl[qtb[i].qfl_pos[j]].size;
                        if ((pos = get_host_position(fsa,
                                                     qfl[qtb[i].qfl_pos[j]].hostname,
                                                     no_of_hosts)) != INCORRECT)
                        {
                           off_t lock_offset;

                           lock_offset = AFD_WORD_OFFSET +
                                         (pos * sizeof(struct filetransfer_status));
#ifdef LOCK_DEBUG
                           lock_region_w(fsa_fd, lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
                           lock_region_w(fsa_fd, lock_offset + LOCK_TFC);
#endif
                           fsa[pos].total_file_counter--;
                           fsa[pos].total_file_size -= qfl[qtb[i].qfl_pos[j]].size;
#ifdef WITH_ERROR_QUEUE
                           if ((qfl[qtb[i].qfl_pos[j]].job_id != 0) &&
                               (fsa[pos].host_status & ERROR_QUEUE_SET))
                           {
                              (void)remove_from_error_queue(qfl[qtb[i].qfl_pos[j]].job_id,
                                                            &fsa[pos], pos,
                                                            fsa_fd);
                           }
#endif
                           if (qb[k].files_to_send == 0)
                           {
                              (void)sprintf(fullname, "%s%s%s/%s",
                                            p_work_dir, AFD_FILE_DIR, OUTGOING_DIR,
                                            qtb[i].msg_name);
                              if (rmdir(fullname) == -1)
                              {
                                 (void)fprintf(stderr,
                                               "Failed to rmdir() %s : %s (%s %d)\n",
                                               fullname, strerror(errno),
                                               __FILE__, __LINE__);
                              }
                              fsa[pos].jobs_queued--;
                              if (k != (*no_msg_queued - 1))
                              {
                                 size_t move_size;

                                 move_size = (*no_msg_queued - 1 - k) * sizeof(struct queue_buf);
                                 (void)memmove(&qb[k], &qb[k + 1], move_size);
                              }
                              (*no_msg_queued)--;
                           }
#ifdef LOCK_DEBUG
                           unlock_region(fsa_fd, lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
                           unlock_region(fsa_fd, lock_offset + LOCK_TFC);
#endif
                        }
#ifdef _DELETE_LOG
                        (void)strcpy(dl.file_name, (char *)qfl[qtb[i].qfl_pos[j]].file_name);
                        if (qfl[qtb[i].qfl_pos[j]].hostname[0] == '\0')
                        {
                           (void)sprintf(dl.host_name, "%-*s %03x",
                                         MAX_HOSTNAME_LENGTH, "-", USER_DEL);
                        }
                        else
                        {
                           (void)sprintf(dl.host_name, "%-*s %03x",
                                         MAX_HOSTNAME_LENGTH, qfl[qtb[i].qfl_pos[j]].hostname,
                                         USER_DEL);
                        }
                        *dl.file_size = qfl[qtb[i].qfl_pos[j]].size;
                        *dl.dir_id = dnb[qfl[qtb[i].qfl_pos[j]].dir_id_pos].dir_id;
                        *dl.job_id = qfl[qtb[i].qfl_pos[j]].job_id;
                        extract_cus(qtb[i].msg_name, dl.input_time,
                                    dl.split_job_counter, dl.unique_number);
                        *dl.file_name_length = strlen((char *)qfl[qtb[i].qfl_pos[j]].file_name);
                        prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                                   "%s %s", SHOW_QUEUE, user);
                        dl_real_size = *dl.file_name_length + dl.size + prog_name_length;
                        if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                        {
                           (void)fprintf(stderr, "write() error : %s (%s %d)\n",
                                         strerror(errno), __FILE__, __LINE__);
                           exit(INCORRECT);
                        }
#endif
                     }
                     files_deleted++;
                  }
                  else
                  {
                     files_not_deleted++;
                  }
               }
            }
         }
      }
   }

#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (fd_delete_readfd != -1)
   {
      (void)close(fd_delete_readfd);
   }
#endif
   if (fd_delete_fd != -1)
   {
      (void)close(fd_delete_fd);
   }

   /*
    * Remove all selected files from the structure queued_file_list.
    */
   skipped = 0;
   for (i = (no_selected - 1); i > -1; i--)
   {
      total_file_size -= qfl[select_list[i] - 1].size;
      if ((i == 0) ||
          (((i - 1) > -1) && (select_list[i - 1] != (select_list[i] - 1))))
      {
         if ((select_list[i] - 1) < total_no_files)
         {
            size_t move_size;

            move_size = (total_no_files - (select_list[i + skipped])) *
                        sizeof(struct queued_file_list);
            (void)memmove(&qfl[select_list[i] - 1],
                          &qfl[select_list[i + skipped]],
                          move_size);
         }
         skipped = 0;
      }
      else
      {
         skipped++;
      }
   }
   total_no_files -= no_selected;

   /*
    * Now remove all selected files list widget.
    */
   XmListDeletePositions(listbox_w, select_list, no_selected);

   if (munmap(((char *)dnb - AFD_WORD_OFFSET), dnb_size) == -1)
   {
      (void)xrec(INFO_DIALOG, "munmap() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
   }

   if (toggles_set & SHOW_OUTPUT)
   {
      if (munmap(((char *)qb - AFD_WORD_OFFSET), qb_size) == -1)
      {
         (void)xrec(INFO_DIALOG, "munmap() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
      }
      (void)fsa_detach(NO);
      (void)detach_afd_status();
   }
   if (toggles_set & SHOW_INPUT)
   {
      (void)fra_detach();
   }

   /* Tell user what we have done. */
   show_summary(total_no_files, total_file_size);
   if ((files_deleted > 0) && (files_not_deleted == 0))
   {
      (void)sprintf(message, "Deleted %d files.", files_deleted);
   }
   else if ((files_deleted > 0) && (files_not_deleted > 0))
        {
           (void)sprintf(message, "Deleted %d files (%d gone).",
                         files_deleted, files_not_deleted);
        }
   else if ((files_deleted == 0) && (files_not_deleted == 0))
        {
           (void)sprintf(message, "Request to delete data send.");
        }
        else
        {
           (void)sprintf(message, "All %d files already gone.",
                         files_not_deleted);
        }
   show_message(statusbox_w, message);

   return;
}


/*+++++++++++++++++++++++++++++ check_fds() +++++++++++++++++++++++++++++*/
static void
#ifdef WITHOUT_FIFO_RW_SUPPORT
check_fds(int *fd_delete_readfd, int *fd_delete_fd)
#else
check_fds(int *fd_delete_fd)
#endif
{
   if (*fd_delete_fd == -1)
   {
      char delete_fifo[MAX_PATH_LENGTH];

      (void)sprintf(delete_fifo, "%s%s%s",
                    p_work_dir, FIFO_DIR, FD_DELETE_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(delete_fifo, fd_delete_readfd, fd_delete_fd) == -1)
#else
      if ((*fd_delete_fd = open(delete_fifo, O_RDWR)) == -1)
#endif
      {
         (void)xrec(FATAL_DIALOG, "Failed to open() <%s> : %s (%s %d)",
                    delete_fifo, strerror(errno), __FILE__, __LINE__);
      }
   }
   return;
}
