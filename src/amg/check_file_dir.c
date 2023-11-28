/*
 *  check_file_dir.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2023 Deutscher Wetterdienst (DWD),
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

DESCR__S_M3
/*
 ** NAME
 **   check_file_dir - checks AFD file directory for jobs without
 **                    messages
 **
 ** SYNOPSIS
 **   void check_file_dir(time_t now,
 **                       dev_t  dev,
 **                       char   *outgoing_file_dir,
 **                       int    outgoing_file_dir_length)
 **
 ** DESCRIPTION
 **   The function check_file_dir() checks the AFD file directory for
 **   jobs without messages.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   08.04.1998 H.Kiehl Created
 **   16.04.2002 H.Kiehl Improve speed when inserting new jobs in queue.
 **   01.03.2008 H.Kiehl Moved from fd to dir_check.
 **   18.09.2021 H.Kiehl Tell user when we omit directories due to too many
 **                      links.
 **                      Make use of linux readdir() d_type to avoid stat().
 **
 */
DESCR__E_M3

#include <string.h>          /* strerror(), strcpy(), strlen()           */
#include <stdlib.h>          /* strtoul(), strtol()                      */
#include <time.h>            /* time()                                   */
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>        /* S_ISDIR(), stat()                        */
#include <sys/mman.h>        /* munmap()                                 */
#include <dirent.h>          /* opendir(), readdir()                     */
#include <unistd.h>          /* rmdir(), read()                          */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "amgdefs.h"

/* #define WITH_VERBOSE_LOG */
#define MAX_FILE_DIR_CHECK_TIME 30L

/* External global variables. */
extern int                        no_of_jobs,
                                  *no_of_process;
extern char                       *p_work_dir;
extern struct instant_db          *db;
extern struct filetransfer_status *fsa;
extern struct fileretrieve_status *fra,
                                  *p_fra;
extern struct dc_proc_list        *dcpl;
extern struct afd_status          *p_afd_status;
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif
#ifdef MULTI_FS_SUPPORT
extern int                        no_of_extra_work_dirs;
extern struct extra_work_dirs     *ewl;
#endif

/* Local global variables. */
static int                        no_of_fd_msgs = 0,
                                  no_of_job_ids;
static char                       **fd_msg_list = NULL,
                                  file_dir[MAX_PATH_LENGTH],
                                  *p_file_dir;
static struct job_id_data         *jd = NULL;

/* Local function prototypes. */
#ifdef MULTI_FS_SUPPORT
static int                        not_a_multi_fs_link(unsigned int),
                                  message_in_queue(char *, int, char *);
static void                       add_message_to_queue(char *, dev_t, int, off_t,
                                                       unsigned int, int),
                                  check_jobs(dev_t);
#else
static int                        message_in_queue(char *);
static void                       add_message_to_queue(char *, int, off_t,
                                                       unsigned int, int),
                                  check_jobs(void);
#endif
static int                        lookup_db_pos(unsigned int);


/*########################## check_file_dir() ###########################*/
void
check_file_dir(time_t now,
#ifdef MULTI_FS_SUPPORT
               dev_t  dev,
#endif
               char   *outgoing_file_dir,
               int    outgoing_file_dir_length)
{
   time_t       diff_time;
#ifdef HAVE_STATX
   struct statx  stat_buf;
#else
   struct stat  stat_buf;
#endif

   (void)strcpy(file_dir, outgoing_file_dir);
   p_file_dir = file_dir + outgoing_file_dir_length;
   if (*(p_file_dir - 1) != '/')
   {
      *p_file_dir = '/';
      p_file_dir++;
      *p_file_dir = '\0';
   }
#ifdef HAVE_STATX
   if (statx(0, file_dir, AT_STATX_SYNC_AS_STAT, 0, &stat_buf) == -1)
#else
   if (stat(file_dir, &stat_buf) == -1)
#endif
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                 _("Failed to statx() `%s' [%d] : %s"),
#else
                 _("Failed to stat() `%s' [%d] : %s"),
#endif
                 file_dir, outgoing_file_dir_length, strerror(errno));
      return;
   }
   p_afd_status->amg_jobs |= CHECK_FILE_DIR_ACTIVE;

#ifdef _MAINTAINER_LOG
   maintainer_log(DEBUG_SIGN, NULL, 0,
                  _("%s starting file dir check in %s . . ."),
                  DIR_CHECK, file_dir);
#endif

   if (read_job_ids(NULL, &no_of_job_ids, &jd) == INCORRECT)
   {
      no_of_job_ids = 0;
   }

#ifdef MULTI_FS_SUPPORT
   check_jobs(dev);
#else
   check_jobs();
#endif

   p_afd_status->amg_jobs &= ~CHECK_FILE_DIR_ACTIVE;

   diff_time = time(NULL) - now;
   if (diff_time > MAX_FILE_DIR_CHECK_TIME)
   {
      system_log(DEBUG_SIGN, NULL, 0,
#if SIZEOF_TIME_T == 4
                 _("Checking file directory for jobs without messages took %ld seconds!"),
#else
                 _("Checking file directory for jobs without messages took %lld seconds!"),
#endif
                 (pri_time_t)diff_time);
   }

#ifdef _MAINTAINER_LOG
   maintainer_log(DEBUG_SIGN, NULL, 0,
# if SIZEOF_TIME_T == 4
                  _("%s file dir check done, time %lds."),
# else
                  _("%s file dir check done, time %llds."),
# endif
                  DIR_CHECK, (pri_time_t)diff_time);
#endif

   if (jd != NULL)
   {
      free(jd);
      jd = NULL;
   }
   if (fd_msg_list != NULL)
   {
      FREE_RT_ARRAY(fd_msg_list);
      fd_msg_list = NULL;
      no_of_fd_msgs = 0;
   }
   else
   {
      int  fd_cmd_fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
      int  fd_cmd_readfd;
#endif
      char fd_cmd_fifo[MAX_PATH_LENGTH];

      /* Tell FD to perform a check of its FSA entries. */
      (void)snprintf(fd_cmd_fifo, MAX_PATH_LENGTH, "%s%s%s",
                     p_work_dir, FIFO_DIR, FD_CMD_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(fd_cmd_fifo, &fd_cmd_readfd, &fd_cmd_fd) == -1)
#else
      if ((fd_cmd_fd = open(fd_cmd_fifo, O_RDWR)) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to open() `%s' : %s"),
                    fd_cmd_fifo, strerror(errno));
      }
      else
      {
         if (send_cmd(CHECK_FSA_ENTRIES, fd_cmd_fd) != SUCCESS)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to write() to `%s' : %s"),
                       fd_cmd_fifo, strerror(errno));
         }
#ifdef WITHOUT_FIFO_RW_SUPPORT
         if ((close(fd_cmd_fd) == -1) || (close(fd_cmd_readfd) == -1))
#else
         if (close(fd_cmd_fd) == -1)
#endif
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("close() error : %s"), strerror(errno));
         }
      }
   }

   return;
}


/*++++++++++++++++++++++++++++++ check_jobs() +++++++++++++++++++++++++++*/
static void
#ifdef MULTI_FS_SUPPORT
check_jobs(dev_t dev)
#else
check_jobs(void)
#endif
{
   int           i,
                 jd_pos;
   unsigned int  job_id,
                 max_nlinks_job_id,
                 max_nlinks_reached = 0;
   nlink_t       max_nlinks = 0;
   char          *p_dir_no,
                 *p_job_id,
                 *ptr;
#ifdef MULTI_FS_SUPPORT
   int           str_dev_length;
   char          str_dev[MAX_INT_HEX_LENGTH + 1];
#endif
   DIR           *dir_no_dp,
                 *dp,
                 *id_dp;
   struct dirent *dir_no_dirp,
                 *dirp,
                 *id_dirp;
#ifdef HAVE_STATX
   struct statx  stat_buf;
#else
   struct stat   stat_buf;
#endif

   /*
    * Check file directory for files that do not have a message.
    */
   if ((dp = opendir(file_dir)) == NULL)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 _("Failed to opendir() `%s' : %s"), file_dir, strerror(errno));
      return;
   }
   ptr = p_file_dir;
   if (*(ptr - 1) != '/')
   {
      *(ptr++) = '/';
   }
#ifdef MULTI_FS_SUPPORT
   str_dev_length = snprintf(str_dev, MAX_INT_HEX_LENGTH,
                             "%x", (unsigned int)dev);
#endif

   /* Check each job if it has a corresponding message. */
   errno = 0;
   while ((dirp = readdir(dp)) != NULL)
   {
#ifdef LINUX
      if ((dirp->d_name[0] != '.') && (dirp->d_type == DT_DIR))
#else
      if (dirp->d_name[0] != '.')
#endif
      {
         (void)strcpy(ptr, dirp->d_name);
         job_id = (unsigned int)strtoul(ptr, (char **)NULL, 16);
         jd_pos = -1;
         for (i = 0; i < no_of_job_ids; i++)
         {
            if (jd[i].job_id == job_id)
            {
               jd_pos = i;
               break;
            }
         }
         if (jd_pos == -1)
         {
            /*
             * This is a old directory no longer in
             * our job list. So lets remove the
             * entire directory.
             */
#ifdef MULTI_FS_SUPPORT
            if (not_a_multi_fs_link(job_id) == 0)
            {
#endif
               if (rec_rmdir(file_dir) < 0)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Failed to rec_rmdir() `%s' #%x"),
                             file_dir, job_id);
               }
               else
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             _("Removed directory `%s' since it is no longer in database. #%x"),
                             file_dir, job_id);
               }
#ifdef MULTI_FS_SUPPORT
            }
#endif
         }
         else
         {
            int proc_pos;

            proc_pos = -1;
            for (i = 0; i < *no_of_process; i++)
            {
               if (job_id == dcpl[i].job_id)
               {
                  proc_pos = i;
                  break;
               }
            }
            if (proc_pos == -1)
            {
#ifdef HAVE_STATX
               if (statx(0, file_dir, AT_STATX_SYNC_AS_STAT,
                         STATX_MODE, &stat_buf) == -1)
#else
               if (stat(file_dir, &stat_buf) == -1)
#endif
               {
                  /*
                   * Be silent when there is no such file or directory, since
                   * it could be that it has been removed by some other process.
                   */
                  if (errno != ENOENT)
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                                _("Failed to statx() `%s' : %s"),
#else
                                _("Failed to stat() `%s' : %s"),
#endif
                                file_dir, strerror(errno));
                  }
               }
               else
               {
                  /* Test if it is a directory. */
#ifdef HAVE_STATX
                  if (S_ISDIR(stat_buf.stx_mode))
#else
                  if (S_ISDIR(stat_buf.st_mode))
#endif
                  {
                     if ((id_dp = opendir(file_dir)) == NULL)
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   _("Failed to opendir() `%s' : %s #%x"),
                                   file_dir, strerror(errno), job_id);
                     }
                     else
                     {
                        p_job_id = ptr + strlen(dirp->d_name);
                        *p_job_id = '/';
                        p_job_id++;
                        errno = 0;
                        while ((id_dirp = readdir(id_dp)) != NULL)
                        {
                           if (id_dirp->d_name[0] != '.')
                           {
                              (void)strcpy(p_job_id, id_dirp->d_name);
#ifdef HAVE_STATX
                              if (statx(0, file_dir, AT_STATX_SYNC_AS_STAT,
                                        STATX_MODE | STATX_NLINK,
                                        &stat_buf) == -1)
#else
                              if (stat(file_dir, &stat_buf) == -1)
#endif
                              {
                                 if (errno != ENOENT)
                                 {
                                    system_log(WARN_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                                               _("Failed to statx() `%s' : %s #%x"),
#else
                                               _("Failed to stat() `%s' : %s #%x"),
#endif
                                               file_dir, strerror(errno),
                                               job_id);
                                 }
                              }
                              else
                              {
#ifdef HAVE_STATX
                                 if (S_ISDIR(stat_buf.stx_mode))
#else
                                 if (S_ISDIR(stat_buf.st_mode))
#endif
                                 {
#ifdef HAVE_STATX
                                    if (stat_buf.stx_nlink < MAX_CHECK_FILE_DIRS)
#else
                                    if (stat_buf.st_nlink < MAX_CHECK_FILE_DIRS)
#endif
                                    {
                                       if ((dir_no_dp = opendir(file_dir)) == NULL)
                                       {
                                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                                     _("Failed to opendir() `%s' : %s #%x"),
                                                     file_dir, strerror(errno),
                                                     job_id);
                                       }
                                       else
                                       {
                                          p_dir_no = p_job_id + strlen(id_dirp->d_name);
                                          *p_dir_no = '/';
                                          p_dir_no++;
                                          errno = 0;
                                          while ((dir_no_dirp = readdir(dir_no_dp)) != NULL)
                                          {
#ifdef LINUX
                                             if ((dir_no_dirp->d_name[0] != '.') &&
                                                 (dir_no_dirp->d_type == DT_DIR))
#else
                                             if (dir_no_dirp->d_name[0] != '.')
#endif
                                             {
                                                (void)strcpy(p_dir_no, dir_no_dirp->d_name);
#ifndef LINUX
# ifdef HAVE_STATX
                                                if (statx(0, file_dir,
                                                          AT_STATX_SYNC_AS_STAT,
                                                          STATX_MODE,
                                                          &stat_buf) == -1)
# else
                                                if (stat(file_dir, &stat_buf) == -1)
# endif
                                                {
                                                   if (errno != ENOENT)
                                                   {
                                                      system_log(WARN_SIGN, __FILE__, __LINE__,
# ifdef HAVE_STATX
                                                                 _("Failed to statx() `%s' : %s #%x"),
# else
                                                                 _("Failed to stat() `%s' : %s #%x"),
# endif
                                                                 file_dir,
                                                                 strerror(errno),
                                                                 job_id);
                                                   }
                                                }
                                                else
                                                {
# ifdef HAVE_STATX
                                                   if (S_ISDIR(stat_buf.stx_mode))
# else
                                                   if (S_ISDIR(stat_buf.st_mode))
# endif
                                                   {
#endif
#ifdef MULTI_FS_SUPPORT
                                                      if (message_in_queue(str_dev, str_dev_length, ptr) == NO)
#else
                                                      if (message_in_queue(ptr) == NO)
#endif
                                                      {
                                                         char          *p_file;
                                                         DIR           *file_dp;
                                                         struct dirent *file_dirp;

                                                         if ((file_dp = opendir(file_dir)) == NULL)
                                                         {
                                                            if (errno != ENOENT)
                                                            {
                                                               system_log(WARN_SIGN, __FILE__, __LINE__,
                                                                          _("Failed to opendir() `%s' : %s #%x"),
                                                                          file_dir, strerror(errno),
                                                                          job_id);
                                                            }
                                                         }
                                                         else
                                                         {
                                                            int   file_counter = 0;
                                                            off_t size_counter = 0;

                                                            p_file = file_dir + strlen(file_dir);
                                                            *p_file = '/';
                                                            p_file++;
                                                            errno = 0;
                                                            while ((file_dirp = readdir(file_dp)) != NULL)
                                                            {
                                                               if (((file_dirp->d_name[0] == '.') &&
                                                                   (file_dirp->d_name[1] == '\0')) ||
                                                                   ((file_dirp->d_name[0] == '.') &&
                                                                   (file_dirp->d_name[1] == '.') &&
                                                                   (file_dirp->d_name[2] == '\0')))
                                                               {
                                                                  continue;
                                                               }
                                                               (void)strcpy(p_file, file_dirp->d_name);
#ifdef HAVE_STATX
                                                               if (statx(0, file_dir,
                                                                         AT_STATX_SYNC_AS_STAT,
                                                                         STATX_SIZE,
                                                                         &stat_buf) != -1)
#else
                                                               if (stat(file_dir, &stat_buf) != -1)
#endif
                                                               {
                                                                  file_counter++;
#ifdef HAVE_STATX
                                                                  size_counter += stat_buf.stx_size;
#else
                                                                  size_counter += stat_buf.st_size;
#endif
                                                               }
                                                               errno = 0;
                                                            }
                                                            *(p_file - 1) = '\0';
                                                            if ((errno) && (errno != ENOENT))
                                                            {
                                                               system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                                          _("Failed to readdir() `%s' : %s #%x"),
                                                                          file_dir, strerror(errno),
                                                                          job_id);
                                                            }
                                                            if (closedir(file_dp) == -1)
                                                            {
                                                               system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                                          _("Failed to closedir() `%s' : %s #%x"),
                                                                          file_dir, strerror(errno),
                                                                          job_id);
                                                            }

                                                            if (file_counter > 0)
                                                            {
                                                               /*
                                                                * Message is NOT in queue. Add message to queue.
                                                                */
                                                               system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                                                                          _("Message `%s' not in queue, adding message (%d files %ld bytes). #%x"),
#else
                                                                          _("Message `%s' not in queue, adding message (%d files %lld bytes). #%x"),
#endif
                                                                          ptr, file_counter,
                                                                          (pri_off_t)size_counter,
                                                                          job_id);
                                                               add_message_to_queue(ptr,
#ifdef MULTI_FS_SUPPORT
                                                                                    dev,
#endif
                                                                                    file_counter, size_counter, job_id, jd_pos);
                                                            }
                                                            else
                                                            {
                                                               /*
                                                                * This is just an empty directory, delete it.
                                                                */
                                                               if (rmdir(file_dir) == -1)
                                                               {
                                                                  if ((errno == ENOTEMPTY) ||
                                                                      (errno == EEXIST))
                                                                  {
                                                                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                                                _("Failed to rmdir() `%s' because there is still data in it, deleting everything in this directory. #%x"),
                                                                                file_dir,
                                                                                job_id);
                                                                     (void)rec_rmdir(file_dir);
                                                                  }
                                                                  else
                                                                  {
                                                                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                                                                _("Failed to rmdir() `%s' : %s #%x"),
                                                                                file_dir,
                                                                                strerror(errno),
                                                                                job_id);
                                                                  }
                                                               }
                                                               else
                                                               {
                                                                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                                             _("Deleted empty directory `%s'. #%x"),
                                                                             file_dir, job_id);
                                                               }
                                                            }
                                                         }
                                                      }
#ifndef LINUX
                                                   }
                                                }
#endif
                                             }
                                             errno = 0;
                                          }

                                          if ((errno) && (errno != ENOENT))
                                          {
                                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                        _("Failed to readdir() `%s' : %s #%x"),
                                                        file_dir, strerror(errno),
                                                        job_id);
                                          }
                                          if (closedir(dir_no_dp) == -1)
                                          {
                                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                        _("Failed to closedir() `%s' : %s #%x"),
                                                        file_dir, strerror(errno),
                                                        job_id);
                                          }
                                       }
                                    }
                                    else
                                    {
#ifdef HAVE_STATX
                                       if (stat_buf.stx_nlink > max_nlinks)
#else
                                       if (stat_buf.st_nlink > max_nlinks)
#endif
                                       {
#ifdef HAVE_STATX
                                          max_nlinks = stat_buf.stx_nlink;
#else
                                          max_nlinks = stat_buf.st_nlink;
#endif
                                          max_nlinks_job_id = job_id;
                                       }
                                       max_nlinks_reached++;
                                    }
                                 }
                              }
                           }
                           errno = 0;
                        }

                        if ((errno) && (errno != ENOENT))
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      _("Failed to readdir() `%s' : %s #%x"),
                                      file_dir, strerror(errno), job_id);
                        }
                        if (closedir(id_dp) == -1)
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      _("Failed to closedir() `%s' : %s #%x"),
                                      file_dir, strerror(errno), job_id);
                        }
                     }
                  }
               }
            }
         }
      }
      errno = 0;
   } /* while ((dirp = readdir(dp)) != NULL) */

   *ptr = '\0';
   if ((errno) && (errno != ENOENT))
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to readdir() `%s' : %s"),
                 file_dir, strerror(errno));
   }
   if (closedir(dp) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to closedir() `%s' : %s"),
                 file_dir, strerror(errno));
   }

   if (max_nlinks_reached > 0)
   {
      (void)sprintf(ptr, "%x", max_nlinks_job_id);
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_NLINK_T > 4
                 _("Did not check %u job directories because of more then %d links in %s (max=%lld #%x )"),
#else
                 _("Did not check %u job directories because of more then %d links in %s (max=%d #%x )"),
#endif
                 max_nlinks_reached, MAX_CHECK_FILE_DIRS, file_dir,
                 (pri_nlink_t)max_nlinks, max_nlinks_job_id);
      *ptr = '\0';
   }

   return;
}


#ifdef MULTI_FS_SUPPORT
/*------------------------- not_a_multi_fs_link() -----------------------*/
static int
not_a_multi_fs_link(unsigned int job_id)
{
   register int i;

   for (i = 0; i < no_of_extra_work_dirs; i++)
   {
      if (job_id == (unsigned int)ewl[i].dev)
      {
         return(1);
      }
   }

   return(0);
}
#endif


/*-------------------------- message_in_queue() -------------------------*/
static int
#ifdef MULTI_FS_SUPPORT
message_in_queue(char *str_dev, int str_dev_length, char *msg_name)
#else
message_in_queue(char *msg_name)
#endif
{
   int i;
#ifdef MULTI_FS_SUPPORT
   char full_msg_name[MAX_MSG_NAME_LENGTH + 1];

   (void)memcpy(full_msg_name, str_dev, str_dev_length);
   full_msg_name[str_dev_length] = '/';
   (void)strcpy(full_msg_name + str_dev_length + 1, msg_name);
#endif

   /*
    * If we do not already have it, ask FD to send us a current
    * message list.
    */
   if (fd_msg_list == NULL)
   {
      int  fd_cmd_fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
      int  fd_cmd_readfd;
#endif
      char fd_cmd_fifo[MAX_PATH_LENGTH];

      (void)snprintf(fd_cmd_fifo, MAX_PATH_LENGTH, "%s%s%s",
                     p_work_dir, FIFO_DIR, FD_CMD_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(fd_cmd_fifo, &fd_cmd_readfd, &fd_cmd_fd) == -1)
#else
      if ((fd_cmd_fd = open(fd_cmd_fifo, O_RDWR)) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to open() `%s' : %s"),
                    fd_cmd_fifo, strerror(errno));
      }
      else
      {
         int  qlr_fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
         int  qlr_write_fd;
#endif
         char queue_list_ready_fifo[MAX_PATH_LENGTH];

         if (send_cmd(FLUSH_MSG_FIFO_DUMP_QUEUE, fd_cmd_fd) != SUCCESS)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to write() to `%s' : %s"),
                       fd_cmd_fifo, strerror(errno));
         }
#ifdef WITHOUT_FIFO_RW_SUPPORT
         if ((close(fd_cmd_fd) == -1) || (close(fd_cmd_readfd) == -1))
#else
         if (close(fd_cmd_fd) == -1)
#endif
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("close() error : %s"), strerror(errno));
         }

         (void)snprintf(queue_list_ready_fifo, MAX_PATH_LENGTH, "%s%s%s",
                        p_work_dir, FIFO_DIR, QUEUE_LIST_READY_FIFO);

#ifdef WITHOUT_FIFO_RW_SUPPORT
         if (open_fifo_rw(queue_list_ready_fifo, &qlr_fd, &qlr_write_fd) == -1)
#else
         if ((qlr_fd = open(queue_list_ready_fifo, O_RDWR)) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to open() `%s' : %s"),
                       queue_list_ready_fifo, strerror(errno));
         }
         else
         {
            int            status;
            fd_set         rset;
            struct timeval timeout;

            FD_ZERO(&rset);
            FD_SET(qlr_fd, &rset);
            timeout.tv_usec = 0;
            timeout.tv_sec = QUEUE_LIST_READY_TIMEOUT;

            status = select((qlr_fd + 1), &rset, NULL, NULL, &timeout);

            if (((status > 0) && (FD_ISSET(qlr_fd, &rset))) ||
                (p_afd_status->fd != ON))
            {
               if (p_afd_status->fd == ON)
               {
                  int  qld_fd,
                       ret;
                  char buffer[32];

                  if ((ret = read(qlr_fd, buffer, 32)) > 0)
                  {
                     if (buffer[0] == QUEUE_LIST_READY)
                     {
                        int   msg_queue_fd;
                        off_t msg_queue_size;
                        char  msg_queue_file[MAX_PATH_LENGTH],
                              *ptr;

                        (void)snprintf(msg_queue_file, MAX_PATH_LENGTH, "%s%s%s",
                                       p_work_dir, FIFO_DIR, MSG_QUEUE_FILE);
                        if ((ptr = map_file(msg_queue_file, &msg_queue_fd,
                                            &msg_queue_size, NULL,
                                            O_RDONLY)) == (caddr_t) -1)
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      _("Failed to map_file() `%s' : %s"),
                                      msg_queue_file, strerror(errno));
                        }
                        else
                        {
                           struct queue_buf *qb;

                           no_of_fd_msgs = *(int *)ptr;
                           ptr += AFD_WORD_OFFSET;
                           qb = (struct queue_buf *)ptr;

                           if (no_of_fd_msgs > 0)
                           {
                              RT_ARRAY(fd_msg_list, no_of_fd_msgs,
                                       MAX_MSG_NAME_LENGTH, char);
                              for (i = 0; i < no_of_fd_msgs; i++)
                              {
                                 (void)strcpy(fd_msg_list[i], qb[i].msg_name);
                              }
                           }

                           if (close(msg_queue_fd) == -1)
                           {
                              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                         _("Failed to close() `%s' : %s"),
                                         msg_queue_file, strerror(errno));
                           }
                           ptr -= AFD_WORD_OFFSET;
                           if (munmap(ptr, msg_queue_size) == -1)
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                         _("Failed to munmap() from `%s' : %s"),
                                         msg_queue_file, strerror(errno));
                           }
                        }
                     }
                     else if (buffer[0] == QUEUE_LIST_EMPTY)
                          {
                             no_of_fd_msgs = 0;
                          }
                          else
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        _("Reading garbage (%d) from `%s'."),
                                        (int)buffer[0], queue_list_ready_fifo);
                          }
                  }
                  else
                  {
                     if (ret == 0)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   _("Reading zero!"));
                     }
                     else
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   _("read() error : %s"), strerror(errno));
                     }
                  }

                  /* Respond to FD so it can continue normal operations. */
                  (void)snprintf(queue_list_ready_fifo, MAX_PATH_LENGTH, "%s%s%s",
                                 p_work_dir, FIFO_DIR, QUEUE_LIST_DONE_FIFO);

                  if ((qld_fd = open(queue_list_ready_fifo, O_WRONLY)) == -1)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                queue_list_ready_fifo, strerror(errno));
                  }
                  else
                  {
                     FD_ZERO(&rset);
                     FD_SET(qld_fd, &rset);
                     timeout.tv_usec = 0;
                     timeout.tv_sec = QUEUE_LIST_READY_TIMEOUT;

                     status = select((qld_fd + 1), NULL, &rset, NULL, &timeout);

                     if ((status > 0) && (FD_ISSET(qld_fd, &rset)))
                     {
                        char buf = QUEUE_LIST_DONE;

                        if (write(qld_fd, &buf, 1) != 1)
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      _("Failed to write() to `%s' : %s"),
                                      queue_list_ready_fifo, strerror(errno));
                        }
                     }
                     else if (status == 0)
                          {
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        _("%s failed to respond."), FD);
                          }
                          else
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        _("select() error (%d) : %s"),
                                        status, strerror(errno));
                          }

                     if (close(qld_fd) == -1)
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   _("Failed to close() `%s' : %s"),
                                   queue_list_ready_fifo, strerror(errno));
                     }
                  }
               }
               else
               {
                  int   msg_queue_fd;
                  off_t msg_queue_size;
                  char  msg_queue_file[MAX_PATH_LENGTH],
                        *ptr;

                  (void)snprintf(msg_queue_file, MAX_PATH_LENGTH, "%s%s%s",
                                 p_work_dir, FIFO_DIR, MSG_QUEUE_FILE);
                  if ((ptr = map_file(msg_queue_file, &msg_queue_fd,
                                      &msg_queue_size, NULL,
                                      O_RDONLY)) == (caddr_t) -1)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                _("Failed to map_file() `%s' : %s"),
                                msg_queue_file, strerror(errno));
                  }
                  else
                  {
                     struct queue_buf *qb;

                     no_of_fd_msgs = *(int *)ptr;
                     ptr += AFD_WORD_OFFSET;
                     qb = (struct queue_buf *)ptr;

                     if (no_of_fd_msgs > 0)
                     {
                        RT_ARRAY(fd_msg_list, no_of_fd_msgs,
                                 MAX_MSG_NAME_LENGTH, char);
                        for (i = 0; i < no_of_fd_msgs; i++)
                        {
                           (void)strcpy(fd_msg_list[i], qb[i].msg_name);
                        }
                     }

                     if (close(msg_queue_fd) == -1)
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   _("Failed to close() `%s' : %s"),
                                   msg_queue_file, strerror(errno));
                     }
                     ptr += AFD_WORD_OFFSET;
                     if (munmap(ptr, msg_queue_size) == -1)
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   _("Failed to munmap() from `%s' : %s"),
                                   msg_queue_file, strerror(errno));
                     }
                  }
               }
            }
            else if (status == 0)
                 {
                    system_log(WARN_SIGN, __FILE__, __LINE__,
                               _("%s failed to respond."), FD);
                 }
                 else
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               _("select() error (%d) : %s"),
                               status, strerror(errno));
                 }

#ifdef WITHOUT_FIFO_RW_SUPPORT
            if ((close(qlr_fd) == -1) || (close(qlr_write_fd) == -1))
#else
            if (close(qlr_fd) == -1)
#endif
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          _("close() error : %s"), strerror(errno));
            }
         }
      }
   }

   /* See if any of the messages in list match, ie. FD has currently */
   /* in its queue or is under proccessing.                          */
   for (i = 0; i < no_of_fd_msgs; i++)
   {
#ifdef MULTI_FS_SUPPORT
      if (my_strcmp(full_msg_name, fd_msg_list[i]) == 0)
#else
      if (my_strcmp(msg_name, fd_msg_list[i]) == 0)
#endif
      {
         return(YES);
      }
   }

   return(NO);
}


/*------------------------ add_message_to_queue() -----------------------*/
static void
add_message_to_queue(char         *dir_name,
#ifdef MULTI_FS_SUPPORT
                     dev_t        dev,
#endif
                     int          file_counter,
                     off_t        size_counter,
                     unsigned int job_id,
                     int          jd_pos)
{
   unsigned int pos,
                split_job_counter,
                unique_number;
   time_t       creation_time;
   char         missing_file_dir[MAX_PATH_LENGTH],
                msg_name[MAX_MSG_NAME_LENGTH],
                *p_start,
                *ptr;

   /*
    * Retrieve priority, creation time, unique number and job ID
    * from the message name. This looks for example as follows:
    *
    *      ae891320/0/56a1bc00_a9f3_0
    *         |     |    |      |   |
    *         |     |    |      |   +-> split job counter
    *         |     |    |      +-----> unique number
    *         |     |    +------------> creation time
    *         |     +-----------------> directory number
    *         +-----------------------> job ID
    */
   (void)strcpy(msg_name, dir_name);
   ptr = msg_name;
   while ((*ptr != '/') && (*ptr != '\0'))
   {
      ptr++; /* Ignore job ID */
   }
   if (*ptr != '/')
   {
      return;
   }
   ptr++;
   while ((*ptr != '/') && (*ptr != '\0'))
   {
      ptr++; /* Ignore directory number. */
   }
   if (*ptr != '/')
   {
      return;
   }
   ptr++;
   p_start = ptr;
   while ((*ptr != '_') && (*ptr != '\0'))
   {
      ptr++;
   }
   if (*ptr != '_')
   {
      return;
   }
   *ptr = '\0';
   creation_time = (time_t)str2timet(p_start, (char **)NULL, 16);
   ptr++;
   p_start = ptr;
   while ((*ptr != '_') && (*ptr != '\0'))
   {
      ptr++;
   }
   if (*ptr != '_')
   {
      return;
   }
   *ptr = '\0';
   unique_number = (unsigned int)strtoul(p_start, (char **)NULL, 16);
   ptr++;
   p_start = ptr;
   while ((*ptr != '_') && (*ptr != '\0'))
   {
      ptr++;
   }
   if (*ptr != '\0')
   {
      return;
   }
   *ptr = '\0';
   split_job_counter = (unsigned int)strtoul(p_start, (char **)NULL, 16);

   if ((pos = lookup_db_pos(job_id)) == INCORRECT)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not locate job %x"), job_id);
#ifdef MULTI_FS_SUPPORT
      (void)snprintf(missing_file_dir, MAX_PATH_LENGTH, "%s%s%s/%x/%s",
                     p_work_dir, AFD_FILE_DIR, OUTGOING_DIR, (unsigned int)dev,
                     dir_name);
#else
      (void)snprintf(missing_file_dir, MAX_PATH_LENGTH, "%s%s%s/%s", p_work_dir,
                     AFD_FILE_DIR, OUTGOING_DIR, dir_name);
#endif
#ifdef _DELETE_LOG
      *dl.input_time = creation_time;
      *dl.unique_number = unique_number;
      *dl.split_job_counter = split_job_counter;
      remove_job_files(missing_file_dir, -1, job_id, DIR_CHECK,
                       JID_LOOKUP_FAILURE_DEL, -1, __FILE__, __LINE__);
#else
      remove_job_files(missing_file_dir, -1, -1, __FILE__, __LINE__);
#endif
   }
   else
   {
      char *p_unique_name;

#ifdef MULTI_FS_SUPPORT
      p_unique_name = missing_file_dir + snprintf(missing_file_dir, MAX_PATH_LENGTH,
                                                  "%s%s%s/%x",
                                                  p_work_dir, AFD_FILE_DIR,
                                                  OUTGOING_DIR,
                                                  (unsigned int)dev);
#else
      p_unique_name = missing_file_dir + snprintf(missing_file_dir, MAX_PATH_LENGTH,
                                                  "%s%s%s",
                                                  p_work_dir, AFD_FILE_DIR,
                                                  OUTGOING_DIR);
#endif
      p_unique_name++;
      (void)snprintf(p_unique_name,
                     MAX_PATH_LENGTH - (p_unique_name - missing_file_dir),
                     "/%s", dir_name);
      if (jd_pos != -1)
      {
         p_fra = &fra[db[pos].fra_pos];
         send_message(missing_file_dir,
#ifdef MULTI_FS_SUPPORT
                      dev,
#endif
                      p_unique_name, split_job_counter,
                      unique_number, creation_time, pos, 0, file_counter,
                      size_counter, NO);
      }
   }

   return;
}


/*-------------------------- lookup_db_pos() ---------------------------*/
static int
lookup_db_pos(unsigned int job_id)
{
   register int i;

   for (i = 0; i < no_of_jobs; i++)
   {
      if (db[i].job_id == job_id)
      {
         return(i);
      }
   }

   return(INCORRECT);
}
