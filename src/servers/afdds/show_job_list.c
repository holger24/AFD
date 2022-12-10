/*
 *  show_job_list.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   show_job_list - shows short summary of all jobs of this AFD
 **
 ** SYNOPSIS
 **   void show_job_list(SSL *ssl)
 **
 ** DESCRIPTION
 **   This function prints a list of all jobs of this AFD
 **   in the following format:
 **     JL <job_number> <job ID> <dir ID> <recipient string> <AMG options>
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   19.04.2007 H.Kiehl Created
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>
#endif
#include <unistd.h>
#include <errno.h>
#include "afddsdefs.h"
#include "server_common_defs.h"

/* External global variables. */
extern char *p_work_dir;


/*########################### show_job_list() ###########################*/
void
show_job_list(SSL *ssl)
{
   int  fd;
   char fullname[MAX_PATH_LENGTH];

   (void)snprintf(fullname, MAX_PATH_LENGTH, "%s%s%s",
                  p_work_dir, FIFO_DIR, JOB_ID_DATA_FILE);
   if ((fd = open(fullname, O_RDONLY)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to open() `%s' : %s"), fullname, strerror(errno));
   }
   else
   {
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat stat_buf;
#endif

#ifdef HAVE_STATX
      if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(fd, &stat_buf) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to access `%s' : %s"), fullname, strerror(errno));
      }
      else
      {
#ifdef HAVE_STATX
         if (stat_buf.stx_size > AFD_WORD_OFFSET)
#else
         if (stat_buf.st_size > AFD_WORD_OFFSET)
#endif
         {
            off_t job_id_db_size;
            char  *ptr;

#ifdef HAVE_STATX
            job_id_db_size = stat_buf.stx_size;
#else
            job_id_db_size = stat_buf.st_size;
#endif
#ifdef HAVE_MMAP
            if ((ptr = mmap(NULL, job_id_db_size, PROT_READ,
                            MAP_SHARED, fd, 0)) == (caddr_t) -1)
#else
            if ((ptr = mmap_emu(NULL, job_id_db_size, PROT_READ,
                                MAP_SHARED, fullname, 0)) == (caddr_t) -1)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to mmap() to `%s' : %s"),
                          fullname, strerror(errno));
            }
            else
            {
               int                cml_fd,
                                  no_of_job_ids;
               struct job_id_data *jd;

               no_of_job_ids = *(int *)ptr;
               ptr += AFD_WORD_OFFSET;
               jd = (struct job_id_data *)ptr;

               (void)snprintf(fullname, MAX_PATH_LENGTH, "%s%s%s",
                              p_work_dir, FIFO_DIR, CURRENT_MSG_LIST_FILE);
               if ((cml_fd = open(fullname, O_RDONLY)) == -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Failed to open() `%s' : %s"),
                             fullname, strerror(errno));
               }
               else
               {
#ifdef HAVE_STATX
                  if (statx(cml_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                            STATX_SIZE, &stat_buf) == -1)
#else
                  if (fstat(cml_fd, &stat_buf) == -1)
#endif
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                _("Failed to access `%s' : %s"),
                                fullname, strerror(errno));
                  }
                  else
                  {
#ifdef HAVE_STATX
                     if (stat_buf.stx_size > sizeof(int))
#else
                     if (stat_buf.st_size > sizeof(int))
#endif
                     {
#ifdef HAVE_MMAP
                        if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                                        stat_buf.stx_size, PROT_READ,
# else
                                        stat_buf.st_size, PROT_READ,
# endif
                                        MAP_SHARED, cml_fd, 0)) == (caddr_t) -1)
#else
                        if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                                            stat_buf.stx_size, PROT_READ,
# else
                                            stat_buf.st_size, PROT_READ,
# endif
                                            MAP_SHARED, fullname,
                                            0)) == (caddr_t) -1)
#endif
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      _("Failed to mmap() to `%s' : %s"),
                                      fullname, strerror(errno));
                        }
                        else
                        {
                           int no_of_current_jobs;

                           no_of_current_jobs = *(int *)ptr;
                           if ((no_of_current_jobs > 0) &&
                               (no_of_job_ids > 0))
                           {
                              int           gotcha,
                                            i, j;
                              char          *cjn; /* Current Job Number. */

                              cjn = ptr + sizeof(int);

                              (void)command(ssl, "211- AFD current job list:");
                              (void)command(ssl, "NJ %d", no_of_current_jobs);

                              for (i = 0; i < no_of_current_jobs; i++)
                              {
                                 gotcha = NO;
                                 for (j = 0; j < no_of_job_ids; j++)
                                 {
                                    if (*(int *)cjn == jd[j].job_id)
                                    {
                                       (void)command(ssl,
                                                     "JL %d %x %x %x %c %s",
                                                     i, jd[j].job_id,
                                                     jd[j].dir_id,
                                                     jd[j].no_of_loptions,
                                                     jd[j].priority,
                                                     jd[j].recipient);
                                       j = no_of_job_ids;
                                       gotcha = YES;
                                    }
                                 }
                                 if (gotcha == NO)
                                 {
                                    (void)command(ssl, "JL %d 0 0 none 0 0", i);
                                 }
                                 cjn += sizeof(int);
                              }
                           }
                           else
                           {
                              (void)command(ssl, "211- AFD current job list:");
                              (void)command(ssl, "NJ 0");
                           }
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
                                         _("Failed to munmap() `%s' : %s"),
                                         fullname, strerror(errno));
                           }
                        }
                     }
                  }
                  if (close(cml_fd) == -1)
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                _("close() error : %s"), strerror(errno));
                  }
               }

#ifdef HAVE_MMAP
               if (munmap(((char *)jd - AFD_WORD_OFFSET), job_id_db_size) == -1)
#else
               if (munmap_emu((void *)((char *)jd - AFD_WORD_OFFSET)) == -1)
#endif
               {
                  (void)snprintf(fullname, MAX_PATH_LENGTH, "%s%s%s",
                                 p_work_dir, FIFO_DIR, JOB_ID_DATA_FILE);
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             _("Failed to munmap() `%s' : %s"),
                             fullname, strerror(errno));
               }
            }
         }
         else
         {
            (void)snprintf(fullname, MAX_PATH_LENGTH, "%s%s%s",
                           p_work_dir, FIFO_DIR, JOB_ID_DATA_FILE);
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("Hmmm, `%s' is less then %d bytes long."),
                       fullname, AFD_WORD_OFFSET);
         }
      }
      if (close(fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("close() error : %s"), strerror(errno));
      }
   }

   return;
}
