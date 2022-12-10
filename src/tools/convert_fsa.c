/*
 *  convert_fsa.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2022 Deutscher Wetterdienst (DWD),
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

DESCR__S_M1
/*
 ** NAME
 **   convert_fsa - converts the FSA from an old format to new one
 **
 ** SYNOPSIS
 **   convert_fsa [-w <working directory>]
 **
 ** DESCRIPTION
 **   When there is a change in the structure filetransfer_status (FSA)
 **   use this program to convert an old FSA to the new one. This one
 **   is currently for converting Version 0.9.x to 1.0.x.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   08.09.1997 H.Kiehl Created
 **   03.05.1998 H.Kiehl Updated to convert 0.9.x -> 1.0.x
 **   14.07.2000 H.Kiehl Updated to convert 1.[01].x -> 1.2.x
 **
 */
DESCR__E_M1

#include <stdio.h>                    /* fprintf(), sprintf()            */
#include <string.h>                   /* strcpy(), strcat(), strerror()  */
#include <stdlib.h>                   /* exit()                          */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>                /* mmap()                          */
#endif
#include <unistd.h>
#include <fcntl.h>                    /* O_RDWR, O_CREAT, O_WRONLY, etc  */
#include <errno.h>
#include "version.h"


/* Global varaibles. */
int        sys_log_fd = STDERR_FILENO;
char       *p_work_dir;
const char *sys_log_name = SYSTEM_LOG_FIFO;

/* Put the old defines that have changed here. */
#define OLD_MAX_PROXY_NAME_LENGTH 25

struct old_filetransfer_status
       {
          char          host_alias[MAX_HOSTNAME_LENGTH + 1];
          char          real_hostname[2][MAX_REAL_HOSTNAME_LENGTH];
          char          host_dsp_name[MAX_HOSTNAME_LENGTH + 1];
          char          proxy_name[OLD_MAX_PROXY_NAME_LENGTH];
          char          host_toggle_str[MAX_TOGGLE_STR_LENGTH];
          char          toggle_pos;
          char          original_toggle_pos;
          char          auto_toggle;
          signed char   file_size_offset;
          int           successful_retries;
          int           max_successful_retries;
          unsigned char special_flag;
          unsigned char protocol;
          char          debug;
          char          host_toggle;
          int           host_status;
          int           error_counter;
          unsigned int  total_errors;
          int           max_errors;
          int           retry_interval;
          int           block_size;
          time_t        last_retry_time;
          time_t        last_connection;
          int           total_file_counter;
          unsigned long total_file_size;
          int           total_connect_time;
          unsigned int  file_counter_done;
          unsigned long bytes_send;
          unsigned int  connections;
          int           active_transfers;
          int           allowed_transfers;
          int           transfer_rate;
          long          transfer_timeout;
          struct status job_status[MAX_NO_PARALLEL_JOBS];
       };


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int                            i,
                                  j,
                                  fd,
                                  old_fsa_fd,
                                  old_fsa_id,
                                  old_no_of_hosts,
                                  fsa_id,
                                  fsa_fd;
   off_t                          old_fsa_size,
                                  fsa_size;
   char                           *ptr,
                                  work_dir[MAX_PATH_LENGTH],
                                  fsa_id_file[MAX_PATH_LENGTH],
                                  old_fsa_stat[MAX_PATH_LENGTH],
                                  new_fsa_stat[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx                   stat_buf;
#else
   struct stat                    stat_buf;
#endif
   struct flock                   wlock = {F_WRLCK, SEEK_SET, 0, 1};
   struct filetransfer_status     *fsa;
   struct old_filetransfer_status *old_fsa;

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD. */
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   (void)strcpy(fsa_id_file, p_work_dir);
   (void)strcat(fsa_id_file, FIFO_DIR);
   (void)strcpy(old_fsa_stat, fsa_id_file);
   (void)strcat(old_fsa_stat, FSA_STAT_FILE);
   (void)strcat(fsa_id_file, FSA_ID_FILE);

   if ((fd = open(fsa_id_file, O_RDWR)) > -1)
   {
      if (fcntl(fd, F_SETLKW, &wlock) < 0)
      {
         /* Is lock already set or are we setting it again? */
         if ((errno != EACCES) && (errno != EAGAIN) && (errno != EBUSY))
         {
            (void)fprintf(stderr,
                          "Could not set write lock for %s : %s (%s %d)\n",
                          fsa_id_file, strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }

      /* Read the FSA file ID. */
      if (read(fd, &old_fsa_id, sizeof(int)) < 0)
      {
         (void)fprintf(stderr,
                       "Could not read the value of the FSA file ID : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   else
   {
      (void)fprintf(stderr, "Failed to open() %s : %s (%s %d)\n",
                    fsa_id_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Attach to old region. */
   ptr = old_fsa_stat + strlen(old_fsa_stat);
   (void)sprintf(ptr, ".%d", old_fsa_id);

   /* Get the size of the old FSA file. */
#ifdef HAVE_STATX
   if (statx(0, old_fsa_stat, AT_STATX_SYNC_AS_STAT,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (stat(old_fsa_stat, &stat_buf) == -1)
#endif
   {
      (void)fprintf(stderr, "Failed to access %s : %s (%s %d)\n",
                    old_fsa_stat, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   else
   {
#ifdef HAVE_STATX
      if (stat_buf.stx_size > 0)
#else
      if (stat_buf.st_size > 0)
#endif
      {
         if ((old_fsa_fd = open(old_fsa_stat, O_RDWR)) < 0)
         {
            (void)fprintf(stderr, "Failed to open() %s : %s (%s %d)\n",
                          old_fsa_stat, strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }

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
            (void)fprintf(stderr, "Failed to mmap() to %s : %s (%s %d)\n",
                          old_fsa_stat, strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
#ifdef HAVE_STATX
         old_fsa_size = stat_buf.stx_size;
#else
         old_fsa_size = stat_buf.st_size;
#endif
      }
      else
      {
         (void)fprintf(stderr, "FSA file %s is empty. (%s %d)\n",
                       old_fsa_stat, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   old_no_of_hosts = *(int *)ptr;
   ptr += AFD_WORD_OFFSET;
   old_fsa = (struct old_filetransfer_status *)ptr;

   /*
    * Create the new mmap FSA region. First calculate the new size.
    */
   fsa_size = AFD_WORD_OFFSET +
              (old_no_of_hosts * sizeof(struct filetransfer_status));
   fsa_id = old_fsa_id + 1;
   (void)sprintf(new_fsa_stat, "%s%s%s.%d",
                 p_work_dir, FIFO_DIR, FSA_STAT_FILE, fsa_id);

   /* Now map the new FSA region to a file. */
   if ((fsa_fd = open(new_fsa_stat, (O_RDWR | O_CREAT | O_TRUNC), FILE_MODE)) < 0)
   {
      (void)fprintf(stderr, "Failed to open() %s : %s (%s %d)\n",
                    new_fsa_stat, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (lseek(fsa_fd, fsa_size - 1, SEEK_SET) == -1)
   {
      (void)fprintf(stderr, "Failed to lseek() in %s : %s (%s %d)\n",
                    new_fsa_stat, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (write(fsa_fd, "", 1) != 1)
   {
      (void)fprintf(stderr, "Failed to write() to %s : %s (%s %d)\n",
                    new_fsa_stat, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef HAVE_MMAP
   if ((ptr = mmap(NULL, fsa_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
                   fsa_fd, 0)) == (caddr_t) -1)
#else
   if ((ptr = mmap_emu(NULL, fsa_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
                       new_fsa_stat, 0)) == (caddr_t) -1)
#endif
   {
      (void)fprintf(stderr, "Failed to mmap() to %s : %s (%s %d)\n",
                    new_fsa_stat, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Write number of hosts to new shm region. */
   *(int*)ptr = old_no_of_hosts;

   /* Reposition fsa pointer after old_no_of_hosts. */
   ptr += AFD_WORD_OFFSET;
   fsa = (struct filetransfer_status *)ptr;

   /*
    * Copy all the old and new data into the new shm region.
    */
   for (i = 0; i < old_no_of_hosts; i++)
   {
      (void)strcpy(fsa[i].host_alias, old_fsa[i].host_alias);
      (void)strcpy(fsa[i].real_hostname[0], old_fsa[i].real_hostname[0]);
      (void)strcpy(fsa[i].real_hostname[1], old_fsa[i].real_hostname[1]);
      (void)strcpy(fsa[i].host_dsp_name, old_fsa[i].host_dsp_name);
      (void)strcpy(fsa[i].proxy_name, old_fsa[i].proxy_name);
      (void)strcpy(fsa[i].host_toggle_str, old_fsa[i].host_toggle_str);
      fsa[i].toggle_pos = old_fsa[i].toggle_pos;
      fsa[i].original_toggle_pos = old_fsa[i].original_toggle_pos;
      fsa[i].auto_toggle = old_fsa[i].auto_toggle;
      fsa[i].file_size_offset = old_fsa[i].file_size_offset;
      fsa[i].successful_retries = old_fsa[i].successful_retries;
      fsa[i].max_successful_retries = old_fsa[i].max_successful_retries;
      fsa[i].special_flag = old_fsa[i].special_flag;
      fsa[i].protocol = (int)old_fsa[i].protocol;
      fsa[i].debug = old_fsa[i].debug;
      fsa[i].host_toggle = old_fsa[i].host_toggle;
      fsa[i].host_status = old_fsa[i].host_status;
      fsa[i].error_counter = old_fsa[i].error_counter;
      fsa[i].total_errors = old_fsa[i].total_errors;
      fsa[i].max_errors = old_fsa[i].max_errors;
      fsa[i].retry_interval = old_fsa[i].retry_interval;
      fsa[i].block_size = old_fsa[i].block_size;
      fsa[i].last_retry_time = old_fsa[i].last_retry_time;
      fsa[i].last_connection = old_fsa[i].last_connection;
      fsa[i].total_file_counter = old_fsa[i].total_file_counter;
      fsa[i].total_file_size = old_fsa[i].total_file_size;
      fsa[i].jobs_queued = (unsigned long)old_fsa[i].total_connect_time;
      fsa[i].file_counter_done = old_fsa[i].file_counter_done;
      fsa[i].bytes_send = old_fsa[i].bytes_send;
      fsa[i].connections = old_fsa[i].connections;
      fsa[i].active_transfers = old_fsa[i].active_transfers;
      fsa[i].allowed_transfers = old_fsa[i].allowed_transfers;
      fsa[i].transfer_timeout = old_fsa[i].transfer_timeout;

      for (j = 0; j < MAX_NO_PARALLEL_JOBS; j++)
      {
         fsa[i].job_status[j].proc_id = old_fsa[i].job_status[j].proc_id;
#ifdef _WITH_BURST_2
         (void)strcpy(fsa[i].job_status[j].unique_name, old_fsa[i].job_status[j].unique_name);
         fsa[i].job_status[j].job_id = old_fsa[i].job_status[j].job_id;
#endif
         fsa[i].job_status[j].connect_status = old_fsa[i].job_status[j].connect_status;
         fsa[i].job_status[j].no_of_files = old_fsa[i].job_status[j].no_of_files;
         fsa[i].job_status[j].no_of_files_done = old_fsa[i].job_status[j].no_of_files_done;
         fsa[i].job_status[j].file_size = old_fsa[i].job_status[j].file_size;
         fsa[i].job_status[j].file_size_done = old_fsa[i].job_status[j].file_size_done;
         fsa[i].job_status[j].bytes_send = old_fsa[i].job_status[j].bytes_send;
         (void)strcpy(fsa[i].job_status[j].file_name_in_use, old_fsa[i].job_status[j].file_name_in_use);
         fsa[i].job_status[j].file_size_in_use = old_fsa[i].job_status[j].file_size_in_use;
         fsa[i].job_status[j].file_size_in_use_done = old_fsa[i].job_status[j].file_size_in_use_done;
      }
   } /* for (i = 0; i < old_no_of_hosts; i++) */

   ptr = (char *)fsa;
   ptr -= AFD_WORD_OFFSET;
#ifdef HAVE_MMAP
   if (munmap(ptr, fsa_size) == -1)
#else
   if (msync_emu(ptr) == -1)
   {
      (void)fprintf(stderr, "msync_emu() error (%s %d)\n",
                    __FILE__, __LINE__);
   }
   if (munmap_emu(ptr) == -1)
#endif
   {
      (void)fprintf(stderr, "Failed to munmap() %s : %s (%s %d)\n",
                    new_fsa_stat, strerror(errno), __FILE__, __LINE__);
   }

   /*
    * Unmap from old memory mapped region.
    */
   ptr = (char *)old_fsa;
   ptr -= AFD_WORD_OFFSET;

#ifdef HAVE_MMAP
   if (munmap(ptr, old_fsa_size) == -1)
#else
   if (munmap_emu(ptr) == -1)
#endif
   {
      (void)fprintf(stderr, "Failed to munmap() %s : %s (%s %d)\n",
                    old_fsa_stat, strerror(errno), __FILE__, __LINE__);
   }

   if (close(fd) == -1)
   {
      (void)fprintf(stderr, "close() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
   }
   if (close(fsa_fd) == -1)
   {
      (void)fprintf(stderr, "close() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
   }
   if (close(old_fsa_fd) == -1)
   {
      (void)fprintf(stderr, "close() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
   }

   /* Now overwrite old FSA file with the new one. */
   if (rename(new_fsa_stat, old_fsa_stat) == -1)
   {
      (void)fprintf(stderr, "Failed to rename() %s to %s : %s (%s %d)\n",
                    old_fsa_stat, new_fsa_stat, strerror(errno),
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }
   else
   {
      (void)fprintf(stdout, "Successfully converted FSA Version 1.[01].x -> 1.2.x\n");
   }

   exit(SUCCESS);
}
