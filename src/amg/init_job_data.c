/*
 *  init_job_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   init_job_data - 
 **
 ** SYNOPSIS
 **   void init_job_data(void)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   21.01.1998 H.Kiehl Created
 **   29.12.2003 H.Kiehl Added file mask database.
 **
 */
DESCR__E_M3

#include <string.h>       /* strcpy(), strcat(), strlen(), strerror()    */
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>       /* exit()                                      */
#include <unistd.h>       /* read()                                      */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef HAVE_MMAP
# include <sys/mman.h>    /* munmap()                                    */
#endif
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int                 dnb_fd,
                           fmd_fd,
                           jd_fd,
                           *no_of_dir_names,
                           *no_of_file_masks,
                           *no_of_job_ids;
extern off_t               fmd_size;
extern char                *fmd,
                           *fmd_end,
                           msg_dir[],
                           *p_msg_dir,
                           *p_work_dir;
extern struct job_id_data  *jd;
extern struct dir_name_buf *dnb;


/*########################### init_job_data() ###########################*/
void
init_job_data(void)
{
   int          new_job_id_data_file;
   char         *ptr,
                job_id_data_file[MAX_PATH_LENGTH],
                dir_name_file[MAX_PATH_LENGTH],
                file_mask_file[MAX_PATH_LENGTH];
   size_t       new_size;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   (void)strcpy(job_id_data_file, p_work_dir);
   (void)strcat(job_id_data_file, FIFO_DIR);
   (void)strcpy(dir_name_file, job_id_data_file);
   (void)strcat(dir_name_file, DIR_NAME_FILE);
   (void)strcpy(file_mask_file, job_id_data_file);
   (void)strcat(file_mask_file, FILE_MASK_FILE);
   (void)strcat(job_id_data_file, JOB_ID_DATA_FILE);
   (void)strcpy(msg_dir, p_work_dir);
   (void)strcat(msg_dir, AFD_MSG_DIR);
   (void)strcat(msg_dir, "/");
   p_msg_dir = msg_dir + strlen(msg_dir);

   /* Check if job ID data file exists. */
#ifdef HAVE_STATX
   if (statx(0, job_id_data_file, AT_STATX_SYNC_AS_STAT, 0, &stat_buf) == -1)
#else
   if (stat(job_id_data_file, &stat_buf) == -1)
#endif
   {
      new_job_id_data_file = YES;
   }
   else
   {
      new_job_id_data_file = NO;
   }

   /* Attach job ID data. */
   new_size = (JOB_ID_DATA_STEP_SIZE * sizeof(struct job_id_data)) +
              AFD_WORD_OFFSET;
   if ((ptr = attach_buf(job_id_data_file, &jd_fd, &new_size,
                         "AMG1", FILE_MODE, NO)) == (caddr_t) -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to mmap() to `%s' : %s",
                 job_id_data_file, strerror(errno));
      exit(INCORRECT);
   }
   no_of_job_ids = (int *)ptr;
   if ((*no_of_job_ids == 0) ||
       (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_JID_VERSION))
   {
      if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_JID_VERSION)
      {
         if (*no_of_job_ids == 0)
         {
            if (new_job_id_data_file == NO)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Removing old JID database with version %d, creating new version %d.",
                          (int)*(ptr + SIZEOF_INT + 1 + 1 + 1),
                          CURRENT_JID_VERSION);
            }
         }
         else
         {
#ifdef HAVE_MMAP
            off_t old_jid_size = new_size;
#endif
            char  *tmp_ptr = ptr;

            if ((ptr = convert_jid(jd_fd, job_id_data_file, &new_size,
                                   *no_of_job_ids,
                                   *(ptr + SIZEOF_INT + 1 + 1 + 1),
                                   CURRENT_JID_VERSION)) == NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to convert_jid() %s", job_id_data_file);
               ptr = tmp_ptr;
               no_of_job_ids = (int *)ptr;
               *no_of_job_ids = 0;
            }
            else
            {
#ifdef HAVE_MMAP
               if (munmap(tmp_ptr, old_jid_size) == -1)
#else
               if (munmap_emu(tmp_ptr) == -1)
#endif
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Failed to munmap() JID : %s"),
                             strerror(errno));
               }
               no_of_job_ids = (int *)ptr;
            }
         }
      }
      *(ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
      *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
      *(ptr + SIZEOF_INT + 1 + 1 + 1) = CURRENT_JID_VERSION;
      (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
   }
   ptr += AFD_WORD_OFFSET;
   jd = (struct job_id_data *)ptr;
#ifdef LOCK_DEBUG
   lock_region_w(jd_fd, (off_t)1, __FILE__, __LINE__);
#else
   lock_region_w(jd_fd, (off_t)1);
#endif

   /* Attach directory names. */
   new_size = (DIR_NAME_BUF_SIZE * sizeof(struct dir_name_buf)) +
              AFD_WORD_OFFSET;
   if ((ptr = attach_buf(dir_name_file, &dnb_fd, &new_size,
                         "AMG2", FILE_MODE, YES)) == (caddr_t) -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to mmap() to `%s' : %s",
                 dir_name_file, strerror(errno));
      exit(INCORRECT);
   }
   no_of_dir_names = (int *)ptr;
   if (*no_of_dir_names == 0)
   {                       
      *(ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
      *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
      *(ptr + SIZEOF_INT + 1 + 1 + 1) = CURRENT_DNB_VERSION;
      (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
   }
   ptr += AFD_WORD_OFFSET;
   dnb = (struct dir_name_buf *)ptr;

   /* Attach file mask. */
   new_size = AFD_WORD_OFFSET;
   if ((ptr = attach_buf(file_mask_file, &fmd_fd, &new_size,
                         "AMG3", FILE_MODE, YES)) == (caddr_t) -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to mmap() to `%s' : %s",
                 file_mask_file, strerror(errno));
      exit(INCORRECT);
   }
   fmd_end = ptr + new_size;
   fmd_size = new_size;
   no_of_file_masks = (int *)ptr;
   if (*no_of_file_masks == 0)
   {                       
      *(ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
      *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
      *(ptr + SIZEOF_INT + 1 + 1 + 1) = CURRENT_FMD_VERSION;
      (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
   }
   ptr += AFD_WORD_OFFSET;
   fmd = ptr;

   return;
}
