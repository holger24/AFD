/*
 *  create_msa.c - Part of AFD, an automatic file distribution program.
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

DESCR__S_M3
/*
 ** NAME
 **   create_msa - creates the MSA of the AFD_MON
 **
 ** SYNOPSIS
 **   void create_msa(void)
 **
 ** DESCRIPTION
 **   This function creates the MSA (Monitor Status Area), to
 **   which all monitor process will map. The MSA has the following
 **   structure:
 **
 **      <AFD_WORD_OFFSET><struct mon_status_area msa[no_of_afds]>
 **
 **   A detailed description of the structure mon_status_area can
 **   be found in mondefs.h. The signed integer variable no_of_afds
 **   contains the number of AFD's that are to be monitored. This
 **   variable can have the value STALE (-1), which will tell all other
 **   process to unmap from this area and map to the new area.
 **
 ** RETURN VALUES
 **   None. Will exit with incorrect if any of the system call will
 **   fail.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   29.08.1998 H.Kiehl Created
 **   13.09.2000 H.Kiehl Addition of top number of process.
 **   24.02.2003 H.Kiehl Added "Remote Command" to AFD_MON_CONFIG.
 **   10.07.2003 H.Kiehl Added options field.
 **   03.12.2003 H.Kiehl Added connection and disconnection time.
 **   27.02.2005 H.Kiehl Option to switch between two AFD's.
 **   23.11.2008 H.Kiehl Added danger_no_of_jobs.
 **   05.12.2009 H.Kiehl Added afd_id.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                 /* strlen(), strcpy(), strerror()    */
#include <stdlib.h>                 /* calloc(), free()                  */
#include <time.h>                   /* time()                            */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>              /* mmap(), munmap()                  */
#endif
#include <unistd.h>                 /* read(), write(), close(), lseek() */
#include <errno.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "mondefs.h"


/* External global variables. */
extern char                   *p_work_dir;
extern int                    msa_fd,
                              msa_id,
                              no_of_afds,  /* The number of remote/  */
                                           /* local AFD's to be      */
                                           /* monitored.             */
                              sys_log_fd;
extern off_t                  msa_size;
extern struct mon_status_area *msa;


/*############################ create_msa() #############################*/
void
create_msa(void)
{
   int                    i, k,
                          fd,
                          loops,
                          old_msa_fd = -1,
                          old_msa_id,
                          old_no_of_afds = -1,
                          rest;
   off_t                  old_msa_size = -1;
   char                   buffer[4096],
                          *ptr = NULL,
                          new_msa_stat[MAX_PATH_LENGTH],
                          old_msa_stat[MAX_PATH_LENGTH],
                          msa_id_file[MAX_PATH_LENGTH];
   struct mon_status_area *old_msa = NULL;
   struct mon_list        *ml = NULL;
   struct flock           wlock = {F_WRLCK, SEEK_SET, 0, 1},
                          ulock = {F_UNLCK, SEEK_SET, 0, 1};
#ifdef HAVE_STATX
   struct statx           stat_buf;
#else
   struct stat            stat_buf;
#endif

   msa_size = -1;

   /* Read AFD_MON_DB file. */
   eval_afd_mon_db(&ml);

   /* Initialise all pathnames and file descriptors */
   (void)strcpy(msa_id_file, p_work_dir);
   (void)strcat(msa_id_file, FIFO_DIR);
   (void)strcpy(old_msa_stat, msa_id_file);
   (void)strcat(old_msa_stat, MON_STATUS_FILE);
   (void)strcat(msa_id_file, MSA_ID_FILE);

   /*
    * First just try open the msa_id_file. If this fails create
    * the file and initialise old_msa_id with -1.
    */
   if ((fd = open(msa_id_file, O_RDWR)) > -1)
   {
      /*
       * Lock MSA ID file.
       */
      if (fcntl(fd, F_SETLKW, &wlock) < 0)
      {
         /* Is lock already set or are we setting it again? */
         if ((errno != EACCES) && (errno != EAGAIN) && (errno != EBUSY))
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Could not set write lock for %s : %s",
                       msa_id_file, strerror(errno));
            exit(INCORRECT);
         }
      }

      /* Read the MSA file ID */
      if (read(fd, &old_msa_id, sizeof(int)) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not read the value of the MSA file ID : %s",
                    strerror(errno));
         exit(INCORRECT);
      }
   }
   else
   {
      if ((fd = open(msa_id_file, (O_RDWR | O_CREAT | O_TRUNC),
#ifdef GROUP_CAN_WRITE
                     (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP))) == -1)
#else
                     (S_IRUSR | S_IWUSR))) == -1)
#endif
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not open %s : %s", msa_id_file, strerror(errno));
         exit(INCORRECT);
      }
      old_msa_id = -1;
   }

   /*
    * Mark memory mapped region as old, so no process puts
    * any new information into the region after we
    * have copied it into the new region.
    */
   if (old_msa_id > -1)
   {
      /* Attach to old region */
      ptr = old_msa_stat + strlen(old_msa_stat);
      (void)sprintf(ptr, ".%d", old_msa_id);

      /* Get the size of the old MSA file. */
#ifdef HAVE_STATX
      if (statx(0, old_msa_stat, AT_STATX_SYNC_AS_STAT,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (stat(old_msa_stat, &stat_buf) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to access %s : %s", old_msa_stat, strerror(errno));
         old_msa_id = -1;
      }
      else
      {
#ifdef HAVE_STATX
         if (stat_buf.stx_size > 0)
#else
         if (stat_buf.st_size > 0)
#endif
         {
            if ((old_msa_fd = open(old_msa_stat, O_RDWR)) < 0)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to open() %s : %s",
                          old_msa_stat, strerror(errno));
               old_msa_id = old_msa_fd = -1;
            }
            else
            {
#ifdef HAVE_MMAP
               if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                               stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                               stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                               MAP_SHARED, old_msa_fd, 0)) == (caddr_t) -1)
#else
               if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                                   stat_buf.stx_size,
# else
                                   stat_buf.st_size,
# endif
                                   (PROT_READ | PROT_WRITE),
                                   MAP_SHARED, old_msa_stat,
                                   0)) == (caddr_t) -1)
#endif
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "mmap() error : %s", strerror(errno));
                  old_msa_id = -1;
               }
               else
               {
                  if (*(int *)ptr == STALE)
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "MSA in %s is stale! Ignoring this MSA.",
                                old_msa_stat);
                     old_msa_id = -1;
                  }
                  else
                  {
#ifdef HAVE_STATX
                     old_msa_size = stat_buf.stx_size;
#else
                     old_msa_size = stat_buf.st_size;
#endif
                  }

                  /*
                   * We actually could remove the old file now. Better
                   * do it when we are done with it.
                   */
               }

               /*
                * Do NOT close the old file! Else some file system
                * optimisers (like fsr in Irix 5.x) move the contents
                * of the memory mapped file!
                */
            }
         }
         else
         {
            old_msa_id = -1;
         }
      }

      if (old_msa_id != -1)
      {
         old_no_of_afds = *(int *)ptr;

         /* Now mark it as stale */
         *(int *)ptr = STALE;

         /* Check if the version has changed. */
         if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_MSA_VERSION)
         {
            unsigned char old_version = *(ptr + SIZEOF_INT + 1 + 1 + 1);

            /* Unmap old FSA file. */
#ifdef HAVE_MMAP
            if (munmap(ptr, old_msa_size) == -1)
#else
            if (munmap_emu(ptr) == -1)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to munmap() %s : %s",
                          old_msa_stat, strerror(errno));
            }
            if ((ptr = convert_msa(old_msa_fd, old_msa_stat, &old_msa_size,
                                   old_no_of_afds,
                                   old_version, CURRENT_MSA_VERSION)) == NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                           "Failed to convert_msa() %s", old_msa_stat);
               old_msa_id = -1;
            }
         }

         if (old_msa_id != -1)
         {
            /* Move pointer to correct position so */
            /* we can extract the relevant data.   */
            ptr += AFD_WORD_OFFSET;

            old_msa = (struct mon_status_area *)ptr;
         }
      }
   }

   /*
    * Create the new mmap region.
    */
   /* First calculate the new size */
   msa_size = AFD_WORD_OFFSET +
              (no_of_afds * sizeof(struct mon_status_area));

   if ((old_msa_id + 1) > -1)
   {
      msa_id = old_msa_id + 1;
   }
   else
   {
      msa_id = 0;
   }
   (void)sprintf(new_msa_stat, "%s%s%s.%d",
                 p_work_dir, FIFO_DIR, MON_STATUS_FILE, msa_id);

   /* Now map the new MSA region to a file */
   if ((msa_fd = open(new_msa_stat, (O_RDWR | O_CREAT | O_TRUNC),
                      FILE_MODE)) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to open() %s : %s", new_msa_stat, strerror(errno));
      exit(INCORRECT);
   }

   /*
    * Write the complete file before we mmap() to it. If we just lseek()
    * to the end, write one byte and then mmap to it can cause a SIGBUS
    * on some systems when the disk is full and data is written to the
    * mapped area. So to detect that the disk is full always write the
    * complete file we want to map.
    */
   loops = msa_size / 4096;
   rest = msa_size % 4096;
   (void)memset(buffer, 0, 4096);
   for (i = 0; i < loops; i++)
   {
      if (write(msa_fd, buffer, 4096) != 4096)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "write() error : %s", strerror(errno));
         exit(INCORRECT);
      }
   }
   if (rest > 0)
   {
      if (write(msa_fd, buffer, rest) != rest)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "write() error : %s", strerror(errno));
         exit(INCORRECT);
      }
   }

#ifdef HAVE_MMAP
   if ((ptr = mmap(NULL, msa_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
                   msa_fd, 0)) == (caddr_t) -1)
#else
   if ((ptr = mmap_emu(0, msa_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
                       new_msa_stat, 0)) == (caddr_t) -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "mmap() error : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Write number of AFD's to new memory mapped region. */
   *(int*)ptr = no_of_afds;

   /* Reposition msa pointer after no_of_afds. */
   ptr += AFD_WORD_OFFSET;
   msa = (struct mon_status_area *)ptr;

   /*
    * Copy all the old and new data into the new mapped region.
    */
   if (old_msa_id < 0)
   {
      /*
       * There is NO old MSA.
       */
      for (i = 0; i < no_of_afds; i++)
      {
         (void)strcpy(msa[i].afd_alias, ml[i].afd_alias);
         (void)strcpy(msa[i].hostname[0], ml[i].hostname[0]);
         (void)strcpy(msa[i].hostname[1], ml[i].hostname[1]);
         msa[i].options = ml[i].options;
         (void)strcpy(msa[i].rcmd, ml[i].rcmd);
         for (k = 0; k < MAX_CONVERT_USERNAME; k++)
         {
            (void)strcpy(msa[i].convert_username[k][0], ml[i].convert_username[k][0]);
            (void)strcpy(msa[i].convert_username[k][1], ml[i].convert_username[k][1]);
         }
         (void)memset(msa[i].log_history, NO_INFORMATION,
                      (NO_OF_LOG_HISTORY * MAX_LOG_HISTORY));
         msa[i].r_work_dir[0]      = '\0';
         msa[i].afd_version[0]     = '\0';
         msa[i].poll_interval      = ml[i].poll_interval;
         msa[i].connect_time       = ml[i].connect_time;
         msa[i].disconnect_time    = ml[i].disconnect_time;
#ifdef NEW_MSA
         msa[i].afd_id             = get_str_checksum(ml[i].afd_alias);
#endif
         msa[i].port[0]            = ml[i].port[0];
         msa[i].port[1]            = ml[i].port[1];
         msa[i].afd_switching      = ml[i].afd_switching;
         msa[i].afd_toggle         = DEFAULT_TOGGLE_HOST - 1;
         msa[i].amg                = STOPPED;
         msa[i].fd                 = STOPPED;
         msa[i].archive_watch      = STOPPED;
         msa[i].jobs_in_queue      = 0;
         msa[i].danger_no_of_jobs  = 0L;
         msa[i].no_of_transfers    = 0;
         msa[i].top_not_time       = 0L;
         (void)memset(msa[i].top_no_of_transfers, 0, (STORAGE_TIME * sizeof(int)));
         msa[i].max_connections    = MAX_DEFAULT_CONNECTIONS;
         msa[i].sys_log_ec         = 0;
         (void)memset(msa[i].sys_log_fifo, NO_INFORMATION, LOG_FIFO_SIZE);
         msa[i].host_error_counter = 0;
         msa[i].no_of_hosts        = 0;
         msa[i].no_of_dirs         = 0;
         msa[i].no_of_jobs         = 0;
         msa[i].log_capabilities   = 0;
         msa[i].fc                 = 0;
         msa[i].fs                 = 0;
         msa[i].tr                 = 0;
         (void)memset(msa[i].top_tr, 0, (STORAGE_TIME * sizeof(off_t)));
         msa[i].top_tr_time        = 0L;
         msa[i].fr                 = 0;
         (void)memset(msa[i].top_fr, 0, (STORAGE_TIME * sizeof(unsigned int)));
         msa[i].top_fr_time        = 0L;
         msa[i].ec                 = 0;
         msa[i].last_data_time     = 0;
         msa[i].connect_status     = DISCONNECTED;
         msa[i].special_flag       = 0;
         for (k = 0; k < SUM_STORAGE; k++)
         {
            msa[i].bytes_send[k]         = 0;
            msa[i].bytes_received[k]     = 0;
            msa[i].files_send[k]         = 0;
            msa[i].files_received[k]     = 0;
            msa[i].connections[k]        = 0;
            msa[i].total_errors[k]       = 0;
            msa[i].log_bytes_received[k] = 0;
         }
      } /* for (i = 0; i < no_of_afds; i++) */
   }
   else /* There is an old database file. */
   {
      int  afd_pos,
           no_of_gotchas = 0;
      char *gotcha = NULL;

      /*
       * The gotcha array is used to find AFD's that are in the
       * old MSA but not in the AFD_MON_CONFIG file.
       */
      if ((gotcha = malloc(old_no_of_afds)) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "malloc() error : %s", strerror(errno));
         exit(INCORRECT);
      }
      (void)memset(gotcha, NO, old_no_of_afds);

      for (i = 0; i < no_of_afds; i++)
      {
         (void)strcpy(msa[i].afd_alias, ml[i].afd_alias);
         (void)strcpy(msa[i].hostname[0], ml[i].hostname[0]);
         (void)strcpy(msa[i].hostname[1], ml[i].hostname[1]);
         msa[i].options = ml[i].options;
         (void)strcpy(msa[i].rcmd, ml[i].rcmd);
         for (k = 0; k < MAX_CONVERT_USERNAME; k++)
         {
            (void)strcpy(msa[i].convert_username[k][0], ml[i].convert_username[k][0]);
            (void)strcpy(msa[i].convert_username[k][1], ml[i].convert_username[k][1]);
         }
         msa[i].r_work_dir[0]   = '\0';
         msa[i].afd_version[0]  = '\0';
         msa[i].poll_interval   = ml[i].poll_interval;
         msa[i].connect_time    = ml[i].connect_time;
         msa[i].disconnect_time = ml[i].disconnect_time;
         msa[i].port[0]         = ml[i].port[0];
         msa[i].port[1]         = ml[i].port[1];
         msa[i].afd_switching   = ml[i].afd_switching;

         /*
          * Search in the old MSA for this AFD. If it is there use
          * the values from the old MSA or else initialise them with
          * defaults. When we find an old entry, remember this so we
          * can later check if there are entries in the old MSA but
          * there are no corresponding entries in the AFD_MON_CONFIG.
          * This will then have to be updated in the AFD_MON_CONFIG file.
          */
         afd_pos = INCORRECT;
         for (k = 0; k < old_no_of_afds; k++)
         {
            if (gotcha[k] != YES)
            {
               if (my_strcmp(old_msa[k].afd_alias, ml[i].afd_alias) == 0)
               {
                  afd_pos = k;
                  break;
               }
            }
         }

         if (afd_pos != INCORRECT)
         {
            no_of_gotchas++;
            gotcha[afd_pos] = YES;

            (void)strcpy(msa[i].r_work_dir, old_msa[afd_pos].r_work_dir);
            (void)strcpy(msa[i].afd_version, old_msa[afd_pos].afd_version);
#ifdef NEW_MSA
            msa[i].afd_id             = old_msa[afd_pos].afd_id;
#endif
            msa[i].afd_toggle         = old_msa[afd_pos].afd_toggle;
            (void)memcpy(msa[i].log_history, old_msa[afd_pos].log_history,
                         (NO_OF_LOG_HISTORY * MAX_LOG_HISTORY));
            msa[i].amg                = old_msa[afd_pos].amg;
            msa[i].fd                 = old_msa[afd_pos].fd;
            msa[i].archive_watch      = old_msa[afd_pos].archive_watch;
            msa[i].jobs_in_queue      = old_msa[afd_pos].jobs_in_queue;
            msa[i].danger_no_of_jobs  = old_msa[afd_pos].danger_no_of_jobs;
            msa[i].no_of_transfers    = old_msa[afd_pos].no_of_transfers;
            msa[i].top_not_time       = old_msa[afd_pos].top_not_time;
            (void)memcpy(msa[i].top_no_of_transfers,
                         old_msa[afd_pos].top_no_of_transfers,
                         (STORAGE_TIME * sizeof(int)));
            msa[i].sys_log_ec         = old_msa[afd_pos].sys_log_ec;
            (void)memcpy(msa[i].sys_log_fifo, old_msa[afd_pos].sys_log_fifo,
                         LOG_FIFO_SIZE);
            msa[i].host_error_counter = old_msa[afd_pos].host_error_counter;
            msa[i].no_of_hosts        = old_msa[afd_pos].no_of_hosts;
            msa[i].no_of_dirs         = old_msa[afd_pos].no_of_dirs;
            msa[i].no_of_jobs         = old_msa[afd_pos].no_of_jobs;
            msa[i].max_connections    = old_msa[afd_pos].max_connections;
            msa[i].log_capabilities   = old_msa[afd_pos].log_capabilities;
            msa[i].fc                 = old_msa[afd_pos].fc;
            msa[i].fs                 = old_msa[afd_pos].fs;
            msa[i].tr                 = old_msa[afd_pos].tr;
            msa[i].top_tr_time        = old_msa[afd_pos].top_tr_time;
            (void)memcpy(msa[i].top_tr, old_msa[afd_pos].top_tr,
                         (STORAGE_TIME * sizeof(off_t)));
            msa[i].fr                 = old_msa[afd_pos].fr;
            msa[i].top_fr_time        = old_msa[afd_pos].top_fr_time;
            (void)memcpy(msa[i].top_fr, old_msa[afd_pos].top_fr,
                         (STORAGE_TIME * sizeof(unsigned int)));
            msa[i].ec                 = old_msa[afd_pos].ec;
            msa[i].last_data_time     = old_msa[afd_pos].last_data_time;
            msa[i].connect_status     = old_msa[afd_pos].connect_status;
            msa[i].special_flag       = old_msa[afd_pos].special_flag;
            for (k = 0; k < SUM_STORAGE; k++)
            {
               msa[i].bytes_send[k]         = old_msa[afd_pos].bytes_send[k];
               msa[i].bytes_received[k]     = old_msa[afd_pos].bytes_received[k];
               msa[i].files_send[k]         = old_msa[afd_pos].files_send[k];
               msa[i].files_received[k]     = old_msa[afd_pos].files_received[k];
               msa[i].connections[k]        = old_msa[afd_pos].connections[k];
               msa[i].total_errors[k]       = old_msa[afd_pos].total_errors[k];
               msa[i].log_bytes_received[k] = old_msa[afd_pos].log_bytes_received[k];
            }
         }
         else /* This AFD is not in the old MSA, therefor it is new. */
         {
            msa[i].afd_toggle         = DEFAULT_TOGGLE_HOST - 1;
#ifdef NEW_MSA
            msa[i].afd_id             = get_str_checksum(ml[i].afd_alias);
#endif
            (void)memset(msa[i].log_history, NO_INFORMATION,
                         (NO_OF_LOG_HISTORY * MAX_LOG_HISTORY));
            msa[i].amg                = STOPPED;
            msa[i].fd                 = STOPPED;
            msa[i].archive_watch      = STOPPED;
            msa[i].jobs_in_queue      = 0;
            msa[i].danger_no_of_jobs  = 0L;
            msa[i].no_of_transfers    = 0;
            msa[i].top_not_time       = 0L;
            (void)memset(msa[i].top_no_of_transfers, 0,
                         (STORAGE_TIME * sizeof(int)));
            msa[i].max_connections    = MAX_DEFAULT_CONNECTIONS;
            msa[i].sys_log_ec         = 0;
            (void)memset(msa[i].sys_log_fifo, NO_INFORMATION, LOG_FIFO_SIZE);
            msa[i].host_error_counter = 0;
            msa[i].no_of_hosts        = 0;
            msa[i].no_of_dirs         = 0;
            msa[i].no_of_jobs         = 0;
            msa[i].log_capabilities   = 0;
            msa[i].fc                 = 0;
            msa[i].fs                 = 0;
            msa[i].tr                 = 0;
            msa[i].top_tr_time        = 0L;
            (void)memset(msa[i].top_tr, 0,
                         (STORAGE_TIME * sizeof(off_t)));
            msa[i].fr                 = 0;
            msa[i].top_fr_time        = 0L;
            (void)memset(msa[i].top_fr, 0,
                         (STORAGE_TIME * sizeof(unsigned int)));
            msa[i].ec                 = 0;
            msa[i].last_data_time     = 0;
            msa[i].connect_status     = DISCONNECTED;
            msa[i].special_flag       = 0;
            for (k = 0; k < SUM_STORAGE; k++)
            {
               msa[i].bytes_send[k]         = 0;
               msa[i].bytes_received[k]     = 0;
               msa[i].files_send[k]         = 0;
               msa[i].files_received[k]     = 0;
               msa[i].connections[k]        = 0;
               msa[i].total_errors[k]       = 0;
               msa[i].log_bytes_received[k] = 0;
            }
         }
      } /* for (i = 0; i < no_of_afds; i++) */
      free(gotcha);
   }

   /* Reposition msa pointer after no_of_afds. */
   ptr = (char *)msa;
   ptr -= AFD_WORD_OFFSET;
   *(ptr + sizeof(int) + 1 + 1 + 1) = CURRENT_MSA_VERSION; /* MSA Version Number */
   if (msa_size > 0)
   {
#ifdef HAVE_MMAP
      if (munmap(ptr, msa_size) == -1)
#else
      if (msync_emu(ptr) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__, "msync_emu() error");
      }
      if (munmap_emu(ptr) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to munmap() %s : %s",
                    new_msa_stat, strerror(errno));
      }
   }

   /*
    * Unmap from old memory mapped region.
    */
   if (old_msa != NULL)
   {
      ptr = (char *)old_msa;
      ptr -= AFD_WORD_OFFSET;

      /* Don't forget to unmap old MSA file. */
      if (old_msa_size > 0)
      {
#ifdef HAVE_MMAP
         if (munmap(ptr, old_msa_size) == -1)
#else
         if (munmap_emu(ptr) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to munmap() %s : %s",
                       old_msa_stat, strerror(errno));
         }
         old_msa = NULL;
      }
   }

   /* Remove the old MSA file if there was one. */
   if (old_msa_size > -1)
   {
      if (unlink(old_msa_stat) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to unlink() %s : %s",
                    old_msa_stat, strerror(errno));
      }
   }

   /*
    * Copy the new msa_id into the locked MSA_ID_FILE file, unlock
    * and close the file.
    */
   /* Go to beginning in file. */
   if (lseek(fd, 0, SEEK_SET) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not seek() to beginning of %s : %s",
                 msa_id_file, strerror(errno));
   }

   /* Write new value into MSA_ID_FILE file. */
   if (write(fd, &msa_id, sizeof(int)) != sizeof(int))
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not write value to MSA ID file : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Unlock file which holds the msa_id. */
   if (fcntl(fd, F_SETLKW, &ulock) < 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not unset write lock : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Close the MSA ID file. */
   if (close(fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }

   /* Close file with new MSA. */
   if (close(msa_fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }
   msa_fd = -1;

   /* Close old MSA file. */
   if (old_msa_fd != -1)
   {
      if (close(old_msa_fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "close() error : %s", strerror(errno));
      }
   }

   /* Free structure mon_list, it's no longer needed. */
   free(ml);

   return;
}
