/*
 *  convert_jid.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   convert_jid - converts the JID from an old format to new one
 **
 ** SYNOPSIS
 **   convert_jid [-w <working directory>]
 **
 ** DESCRIPTION
 **   Helps to convert the JID (Job ID Data) structure when there is
 **   a change in the structure itself. This program must be run before
 **   the change and before the AFD is started, otherwise the whole
 **   internal data structure can be destroyed.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.08.2000 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                    /* fprintf(), rename(), remove()   */
#include <string.h>                   /* strcpy(), strcat(), strerror()  */
#include <stdlib.h>                   /* exit()                          */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>                 /* mmap()                          */
#include <unistd.h>
#include <fcntl.h>                    /* O_RDWR, O_CREAT, O_WRONLY, etc  */
#include <errno.h>
#include "amgdefs.h"
#include "version.h"


/* Global varaibles. */
int        sys_log_fd = STDERR_FILENO;
char       *p_work_dir;
const char *sys_log_name = SYSTEM_LOG_FIFO;

/* Put the old defines that have changed here. */
#define OLD_MAX_NO_FILES 20

struct old_job_id_data
       {
          int  job_id;
          int  dir_id_pos;
          char priority;
          int  no_of_files;
          char file_list[OLD_MAX_NO_FILES][MAX_FILENAME_LENGTH];
          int  no_of_loptions;
          char loptions[MAX_OPTION_LENGTH];
          int  no_of_soptions;
          char soptions[MAX_OPTION_LENGTH];
          char recipient[MAX_RECIPIENT_LENGTH];
          char host_alias[MAX_HOSTNAME_LENGTH + 1];
       };


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int                    i, j,
                          length,
                          new_jd_fd,
                          new_size,
                          no_of_job_ids,
                          old_jd_fd;
   char                   old_job_id_data_file[MAX_PATH_LENGTH],
                          *p_file,
                          *ptr,
                          new_job_id_data_file[MAX_PATH_LENGTH],
                          work_dir[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx           stat_buf;
#else
   struct stat            stat_buf;
#endif
   struct job_id_data     *new_jd;
   struct old_job_id_data *old_jd;

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD. */
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   /* Attach to the old JID. */
   (void)sprintf(old_job_id_data_file, "%s%s%s", p_work_dir, FIFO_DIR,
                 JOB_ID_DATA_FILE);
   if ((old_jd_fd = open(old_job_id_data_file, O_RDWR)) == -1)
   {
      (void)fprintf(stderr, "Failed to open() %s : %s (%s %d)\n",
                    old_job_id_data_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef HAVE_STATX
   if (statx(old_jd_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(old_jd_fd, &stat_buf) == -1)
#endif
   {
      (void)fprintf(stderr, "Failed to access %s : %s (%s %d)\n",
                    old_job_id_data_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef HAVE_STATX
   if (stat_buf.stx_size > 0)
#else
   if (stat_buf.st_size > 0)
#endif
   {
      if ((ptr = mmap(0,
#ifdef HAVE_STATX
                      stat_buf.stx_size, (PROT_READ | PROT_WRITE),
#else
                      stat_buf.st_size, (PROT_READ | PROT_WRITE),
#endif
                      MAP_SHARED, old_jd_fd, 0)) == (caddr_t) -1)
      {
         (void)fprintf(stderr, "Failed to mmap() to %s : %s (%s %d)\n",
                       old_job_id_data_file, strerror(errno),
                       __FILE__, __LINE__);
         exit(INCORRECT);
      }
      no_of_job_ids = *(int *)ptr;
      ptr += AFD_WORD_OFFSET;
      old_jd = (struct old_job_id_data *)ptr;
#ifdef LOCK_DEBUG
      lock_region_w(old_jd_fd, 1, __FILE__, __LINE__);
#else
      lock_region_w(old_jd_fd, 1);
#endif
   }
   else
   {
      (void)fprintf(stderr,
                   "File %s is empty! Terminating, don't know what to do :-( (%s %d)\n",
                   old_job_id_data_file, __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Create and attach to the new JID. */
   (void)strcpy(new_job_id_data_file, old_job_id_data_file);
   (void)strcat(new_job_id_data_file, "_new");
   new_size = (((no_of_job_ids / JOB_ID_DATA_STEP_SIZE) + 1) *
               JOB_ID_DATA_STEP_SIZE * sizeof(struct job_id_data)) +
              AFD_WORD_OFFSET;
   if ((new_jd_fd = open(new_job_id_data_file, (O_RDWR | O_CREAT | O_TRUNC),
                         (S_IRUSR | S_IWUSR))) == -1)
   {
      (void)fprintf(stderr, "Failed to open() %s : %s (%s %d)\n",
                    new_job_id_data_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (lseek(new_jd_fd, new_size - 1, SEEK_SET) == -1)
   {
      (void)fprintf(stderr, "Failed to lseek() %s : %s (%s %d)\n",
                    new_job_id_data_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (write(new_jd_fd, "", 1) != 1)
   {
      (void)fprintf(stderr, "write() error in %s : %s (%s %d)\n",
                    new_job_id_data_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if ((ptr = mmap(0, new_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
                   new_jd_fd, 0)) == (caddr_t) -1)
   {
      (void)fprintf(stderr, "Failed to mmap() to %s : %s (%s %d)\n",
                    new_job_id_data_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   *(int*)ptr = no_of_job_ids;
   ptr += AFD_WORD_OFFSET;
   new_jd = (struct job_id_data *)ptr;

   /* Now copy the old data into the new structure. */
   for (i = 0; i < no_of_job_ids; i++)
   {
      new_jd[i].job_id = old_jd[i].job_id;
      new_jd[i].dir_id_pos = old_jd[i].dir_id_pos;
      new_jd[i].priority = old_jd[i].priority;
      new_jd[i].no_of_files = old_jd[i].no_of_files;
      (void)memset(new_jd[i].file_list, 0, MAX_FILE_MASK_BUFFER);
      p_file = new_jd[i].file_list;
      for (j = 0; j < new_jd[i].no_of_files; j++)
      {
         length = strlen(old_jd[i].file_list[j]);
         if ((length + (p_file - new_jd[i].file_list)) >= MAX_FILE_MASK_BUFFER)
         {
            (void)fprintf(stderr,
                          "WARNING: Could NOT store all file filters for JID %d. (%s %d)\n",
                          new_jd[i].job_id, __FILE__, __LINE__);
            break;
         }
         (void)strcpy(p_file, old_jd[i].file_list[j]);
         p_file += length + 1;
      }
      new_jd[i].no_of_loptions = old_jd[i].no_of_loptions;
      (void)memcpy(new_jd[i].loptions, old_jd[i].loptions, MAX_OPTION_LENGTH);
      new_jd[i].no_of_soptions = old_jd[i].no_of_soptions;
      (void)memcpy(new_jd[i].soptions, old_jd[i].soptions, MAX_OPTION_LENGTH);
      (void)memcpy(new_jd[i].recipient, old_jd[i].recipient, MAX_RECIPIENT_LENGTH);
      (void)memcpy(new_jd[i].host_alias, old_jd[i].host_alias, MAX_HOSTNAME_LENGTH + 1);
   }

   /* Unmap the memory mmaped areas. */
   ptr -= AFD_WORD_OFFSET;
   if (munmap(ptr, new_size) == -1)
   {
      (void)fprintf(stderr, "Failed to munmap() to %s : %s (%s %d)\n",
                    new_job_id_data_file, strerror(errno), __FILE__, __LINE__);
   }
   ptr = (char *)old_jd;
   ptr -= AFD_WORD_OFFSET;
#ifdef HAVE_STATX
   if (munmap(ptr, stat_buf.stx_size) == -1)
#else
   if (munmap(ptr, stat_buf.st_size) == -1)
#endif
   {
      (void)fprintf(stderr, "Failed to munmap() to %s : %s (%s %d)\n",
                    old_job_id_data_file, strerror(errno), __FILE__, __LINE__);
   }

   /* Move the old JID to the new one. */
   if (remove(old_job_id_data_file) == -1)
   {
      (void)fprintf(stderr, "Failed to remove() %s : %s (%s %d)\n",
                    old_job_id_data_file, strerror(errno), __FILE__, __LINE__);
   }
   if (rename(new_job_id_data_file, old_job_id_data_file) == -1)
   {
      (void)fprintf(stderr, "Failed to rename() %s to %s : %s (%s %d)\n",
                    new_job_id_data_file, old_job_id_data_file,
                    strerror(errno), __FILE__, __LINE__);
   }

   /* Close and thus release the lock of the old JID. */
   if (close(old_jd_fd) == -1)
   {
      (void)fprintf(stderr, "Failed to close() %s : %s (%s %d)\n",
                    old_job_id_data_file, strerror(errno), __FILE__, __LINE__);
   }
   if (close(new_jd_fd) == -1)
   {
      (void)fprintf(stderr, "Failed to close() %s : %s (%s %d)\n",
                    new_job_id_data_file, strerror(errno), __FILE__, __LINE__);
   }
   (void)fprintf(stdout, " Successfully converted JID!\n");

   exit(SUCCESS);
}
