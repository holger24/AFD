/*
 *  create_fsa.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   create_fsa - creates the FSA of the AFD
 **
 ** SYNOPSIS
 **   void create_fsa(void)
 **
 ** DESCRIPTION
 **   This function creates the FSA (Filetransfer Status Area),
 **   to which most process of the AFD will map. The FSA has the
 **   following structure:
 **
 **      <AFD_WORD_OFFSET><struct filetransfer_status fsa[no_of_hosts]>
 **
 **   A detailed description of the structure filetransfer_status
 **   can be found in afddefs.h. The signed integer variable no_of_hosts
 **   in AFD_WORD_OFFSET, contains the number of hosts that the AFD has
 **   to serve. This variable can have the value STALE (-1), which will
 **   tell all other process to unmap from this area and map to the new
 **   area.
 **
 ** RETURN VALUES
 **   None. Will exit with incorrect if any of the system call will
 **   fail.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.12.1997 H.Kiehl Created
 **   05.04.2001 H.Kiehl Fill file with data before it is mapped.
 **   18.04.2001 H.Kiehl Add version number to structure.
 **   26.09.2002 H.Kiehl Added automatic conversion of FSA.
 **   26.06.2004 H.Kiehl Added error_history and ttl.
 **   16.02.2006 H.Kiehl Added socket send and receive buffer.
 **   08.03.2006 H.Kiehl Added duplicate check on a per host basis.
 **   13.02.2017 H.Kiehl Increase FSA by one host, so get_new_positions()
 **                      can write invisible data.
 **   19.07.2019 H.Kiehl Added simulate mode for HOST_CONFIG.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                 /* strlen(), strcmp(), strcpy(),     */
                                    /* strerror()                        */
#include <stdlib.h>                 /* calloc(), free()                  */
#include <time.h>                   /* time()                            */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>               /* struct timeval                    */
#ifdef HAVE_MMAP
# include <sys/mman.h>              /* mmap(), munmap()                  */
#endif
#include <unistd.h>                 /* read(), write(), close(), lseek() */
#include <errno.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "amgdefs.h"

/* #define DEBUG_WAIT_LOOP */

/* External global variables. */
extern unsigned char              ignore_first_errors;
extern char                       *p_work_dir;
extern int                        first_time,
                                  fsa_id,
                                  fsa_fd,
                                  no_of_hosts; /* The number of remote/  */
                                               /* local hosts to which   */
                                               /* files have to be       */
                                               /* transfered.            */
extern off_t                      fsa_size;
extern struct host_list           *hl;         /* Structure that holds   */
                                               /* all the hosts.         */
extern struct filetransfer_status *fsa;
extern struct afd_status          *p_afd_status;


/*############################ create_fsa() #############################*/
void
create_fsa(void)
{
   int                        fsa_id_fd,
                              i, k,
                              loops,
                              old_fsa_fd = -1,
                              old_fsa_id,
                              old_no_of_hosts = -1,
                              pagesize,
                              rest,
                              size;
   off_t                      old_fsa_size = -1;
   char                       buffer[4096],
                              fsa_id_file[MAX_PATH_LENGTH],
                              new_fsa_stat[MAX_PATH_LENGTH],
                              old_fsa_stat[MAX_PATH_LENGTH],
                              *ptr = NULL;
   struct filetransfer_status *old_fsa = NULL;
   struct flock               wlock = {F_WRLCK, SEEK_SET, 0, 1};
#ifdef HAVE_STATX
   struct statx               stat_buf;
#else
   struct stat                stat_buf;
#endif

   fsa_size = -1;

   /* Initialise all pathnames and file descriptors. */
   (void)strcpy(fsa_id_file, p_work_dir);
   (void)strcat(fsa_id_file, FIFO_DIR);
   (void)strcpy(old_fsa_stat, fsa_id_file);
   (void)strcat(old_fsa_stat, FSA_STAT_FILE);
   (void)strcat(fsa_id_file, FSA_ID_FILE);

   /*
    * First just try open the fsa_id_file. If this fails create
    * the file and initialise old_fsa_id with -1.
    */
   if ((fsa_id_fd = open(fsa_id_file, O_RDWR)) > -1)
   {
      /*
       * Lock FSA ID file. If it is already locked
       * (by edit_hc dialog) wait for it to clear the lock
       * again.
       */
      if (fcntl(fsa_id_fd, F_SETLKW, &wlock) < 0)
      {
         /* Is lock already set or are we setting it again? */
         if ((errno != EACCES) && (errno != EAGAIN) && (errno != EBUSY))
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Could not set write lock for %s : %s",
                       fsa_id_file, strerror(errno));
            exit(INCORRECT);
         }
      }

      /* Read the FSA file ID. */
      if (read(fsa_id_fd, &old_fsa_id, sizeof(int)) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not read the value of the FSA file ID : %s",
                    strerror(errno));
         exit(INCORRECT);
      }
   }
   else
   {
      if ((fsa_id_fd = open(fsa_id_file, (O_RDWR | O_CREAT), FILE_MODE)) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not open %s : %s", fsa_id_file, strerror(errno));
         exit(INCORRECT);
      }
      old_fsa_id = -1;
   }

   /*
    * We now have to determine if this is the first time that the
    * AMG is being run. The only way to find this out is to see
    * whether the startup time of the AFD has been set.  If it is
    * not set (i.e. 0), it is the first time.
    */
   if (first_time == YES)
   {
      if (p_afd_status->start_time > 0)
      {
         first_time = NO;
      }
      else
      {
         first_time = YES;
      }
   }

   /* Set flag to indicate that we are rereading the DIR_CONFIG. */
   p_afd_status->amg_jobs |= REREADING_DIR_CONFIG;

   /*
    * Mark memory mapped region as old, so no process puts
    * any new information into the region after we
    * have copied it into the new region.
    */
   if (old_fsa_id > -1)
   {
      /* Attach to old region. */
      ptr = old_fsa_stat + strlen(old_fsa_stat);
      (void)snprintf(ptr, MAX_PATH_LENGTH - (ptr - old_fsa_stat),
                     ".%d", old_fsa_id);

      /* Get the size of the old FSA file. */
#ifdef HAVE_STATX
      if (statx(0, old_fsa_stat, AT_STATX_SYNC_AS_STAT,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (stat(old_fsa_stat, &stat_buf) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                    "Failed to statx() %s : %s",
#else
                    "Failed to stat() %s : %s",
#endif
                    old_fsa_stat, strerror(errno));
         old_fsa_id = -1;
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
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to open() %s : %s",
                          old_fsa_stat, strerror(errno));
               old_fsa_id = old_fsa_fd = -1;
            }
            else
            {
               /*
                * Lock the whole region so all sf_xxx process stop
                * writting data to the old FSA.
                */
#ifdef HAVE_STATX
               wlock.l_len = stat_buf.stx_size;
#else
               wlock.l_len = stat_buf.st_size;
#endif
               if (fcntl(old_fsa_fd, F_SETLKW, &wlock) < 0)
               {
                  /* Is lock already set or are we setting it again? */
                  if ((errno != EACCES) && (errno != EAGAIN) && (errno != EBUSY))
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Could not set write lock for %s : %s",
                                old_fsa_stat, strerror(errno));
                  }
                  else
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "Could not set write lock for %s : %s",
                                old_fsa_stat, strerror(errno));
                  }
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
                                   stat_buf.stx_size,
# else
                                   stat_buf.st_size,
# endif
                                   (PROT_READ | PROT_WRITE),
                                   MAP_SHARED, old_fsa_stat, 0)) == (caddr_t) -1)
#endif
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "mmap() error : %s", strerror(errno));
                  old_fsa_id = -1;
               }
               else
               {
                  if (*(int *)ptr == STALE)
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "FSA in %s is stale! Ignoring this FSA.",
                                old_fsa_stat);
                     old_fsa_id = -1;
                  }
                  else
                  {
#ifdef HAVE_STATX
                     old_fsa_size = stat_buf.stx_size;
#else
                     old_fsa_size = stat_buf.st_size;
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
            old_fsa_id = -1;
         }
      }

      if (old_fsa_id != -1)
      {
         old_no_of_hosts = *(int *)ptr;

         /* Now mark it as stale. */
         *(int *)ptr = STALE;

         /* Check if the version has changed. */
         if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_FSA_VERSION)
         {
            unsigned char old_version = *(ptr + SIZEOF_INT + 1 + 1 + 1);

            /* Unmap old FSA file. */
#ifdef HAVE_MMAP
            if (munmap(ptr, old_fsa_size) == -1)
#else
            if (munmap_emu(ptr) == -1)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to munmap() %s : %s",
                          old_fsa_stat, strerror(errno));
            }
            if ((ptr = convert_fsa(old_fsa_fd, old_fsa_stat, &old_fsa_size,
                                   old_no_of_hosts,
                                   old_version, CURRENT_FSA_VERSION)) == NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to convert_fsa() %s", old_fsa_stat);
               old_fsa_id = -1;
            }
         } /* FSA version has changed. */

         if (old_fsa_id != -1)
         {
            /* Move pointer to correct position so */
            /* we can extract the relevant data.   */
            ptr += AFD_WORD_OFFSET;

            old_fsa = (struct filetransfer_status *)ptr;
         }
      }
   }

   /*
    * Create the new mmap region.
    */
   /* First calculate the new size. The + 1 after no_of_hosts is in case */
   /* the function get_new_positions() needs to write some data not      */
   /* visible to the user.                                               */
   fsa_size = AFD_WORD_OFFSET +
              ((no_of_hosts + 1) * sizeof(struct filetransfer_status));

   if ((old_fsa_id + 1) > -1)
   {
      fsa_id = old_fsa_id + 1;
   }
   else
   {
      fsa_id = 0;
   }
   (void)snprintf(new_fsa_stat, MAX_PATH_LENGTH, "%s%s%s.%d",
                  p_work_dir, FIFO_DIR, FSA_STAT_FILE, fsa_id);

   /* Now map the new FSA region to a file. */
   if ((fsa_fd = open(new_fsa_stat, (O_RDWR | O_CREAT | O_TRUNC),
                      FILE_MODE)) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                "Failed to open() %s : %s", new_fsa_stat, strerror(errno));
      exit(INCORRECT);
   }

   /*
    * Write the complete file before we mmap() to it. If we just lseek()
    * to the end, write one byte and then mmap to it can cause a SIGBUS
    * on some systems when the disk is full and data is written to the
    * mapped area. So to detect that the disk is full always write the
    * complete file we want to map.
    */
   loops = fsa_size / 4096;
   rest = fsa_size % 4096;
   (void)memset(buffer, 0, 4096);
   for (i = 0; i < loops; i++)
   {
      if (write(fsa_fd, buffer, 4096) != 4096)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "write() error : %s", strerror(errno));
         exit(INCORRECT);
      }
   }
   if (rest > 0)
   {
      if (write(fsa_fd, buffer, rest) != rest)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "write() error : %s", strerror(errno));
         exit(INCORRECT);
      }
   }

#ifdef HAVE_MMAP
   if ((ptr = mmap(NULL, fsa_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
                   fsa_fd, 0)) == (caddr_t) -1)
#else
   if ((ptr = mmap_emu(NULL, fsa_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
                       new_fsa_stat, 0)) == (caddr_t) -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "mmap() error : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Write number of hosts to new memory mapped region. */
   *(int*)ptr = no_of_hosts;

   /* Initialize HOST_CONFIG counter. */
   *(unsigned char *)(ptr + SIZEOF_INT) = 0;

   /* Reposition fsa pointer after no_of_hosts. */
   ptr += AFD_WORD_OFFSET;
   fsa = (struct filetransfer_status *)ptr;

   /*
    * Copy all the old and new data into the new mapped region.
    */
   size = MAX_NO_PARALLEL_JOBS * sizeof(struct status);
   if (old_fsa_id < 0)
   {
      struct system_data sd;

      /*
       * There is NO old FSA.
       */
      for (i = 0; i < no_of_hosts; i++)
      {
         (void)memcpy(fsa[i].host_alias, hl[i].host_alias, MAX_HOSTNAME_LENGTH + 1);
         (void)memcpy(fsa[i].real_hostname[0], hl[i].real_hostname[0], MAX_REAL_HOSTNAME_LENGTH);
         (void)memcpy(fsa[i].real_hostname[1], hl[i].real_hostname[1], MAX_REAL_HOSTNAME_LENGTH);
         (void)memcpy(fsa[i].proxy_name, hl[i].proxy_name, MAX_PROXY_NAME_LENGTH + 1);
         fsa[i].allowed_transfers      = hl[i].allowed_transfers;
         fsa[i].max_errors             = hl[i].max_errors;
         fsa[i].retry_interval         = hl[i].retry_interval;
         fsa[i].block_size             = hl[i].transfer_blksize;
         fsa[i].max_successful_retries = hl[i].successful_retries;
         fsa[i].file_size_offset       = hl[i].file_size_offset;
         fsa[i].transfer_timeout       = hl[i].transfer_timeout;
         fsa[i].protocol               = hl[i].protocol;
         fsa[i].transfer_rate_limit    = hl[i].transfer_rate_limit;
         fsa[i].trl_per_process        = hl[i].transfer_rate_limit;
         fsa[i].ttl                    = hl[i].ttl;
         fsa[i].socksnd_bufsize        = hl[i].socksnd_bufsize;
         fsa[i].sockrcv_bufsize        = hl[i].sockrcv_bufsize;
         fsa[i].keep_connected         = hl[i].keep_connected;
         fsa[i].warn_time              = hl[i].warn_time;
#ifdef WITH_DUP_CHECK
         fsa[i].dup_check_flag         = hl[i].dup_check_flag;
         fsa[i].dup_check_timeout      = hl[i].dup_check_timeout;
#endif
         fsa[i].host_id                = get_str_checksum(fsa[i].host_alias);
         fsa[i].protocol_options       = hl[i].protocol_options;
         fsa[i].protocol_options2      = hl[i].protocol_options2;
         fsa[i].special_flag           = 0;
         if (hl[i].in_dir_config == YES)
         {
            fsa[i].special_flag |= HOST_IN_DIR_CONFIG;
         }
         if (hl[i].host_status & HOST_CONFIG_HOST_DISABLED)
         {
            fsa[i].special_flag |= HOST_DISABLED;
         }
         if (hl[i].protocol_options & KEEP_CON_NO_SEND_2)
         {
            fsa[i].special_flag |= KEEP_CON_NO_SEND;
         }
         if (hl[i].protocol_options & KEEP_CON_NO_FETCH_2)
         {
            fsa[i].special_flag |= KEEP_CON_NO_FETCH;
         }
         if (hl[i].host_status & HOST_TWO_FLAG)
         {
            fsa[i].host_toggle = HOST_TWO;
         }
         else
         {
            fsa[i].host_toggle = DEFAULT_TOGGLE_HOST;
         }

         /* Determine the host name to display. */
         fsa[i].original_toggle_pos = NONE;
         (void)memcpy(fsa[i].host_dsp_name, fsa[i].host_alias, MAX_HOSTNAME_LENGTH + 1);
         fsa[i].toggle_pos = strlen(fsa[i].host_alias);
         if (hl[i].host_toggle_str[0] == '\0')
         {
            fsa[i].host_toggle_str[0]  = '\0';
            if (fsa[i].real_hostname[0][0] == '\0')
            {
               (void)memcpy(fsa[i].real_hostname[0], hl[i].fullname, MAX_REAL_HOSTNAME_LENGTH);
               (void)memcpy(hl[i].real_hostname[0], hl[i].fullname, MAX_REAL_HOSTNAME_LENGTH);
            }
         }
         else
         {
            (void)memcpy(fsa[i].host_toggle_str, hl[i].host_toggle_str, MAX_TOGGLE_STR_LENGTH);
            if (hl[i].host_toggle_str[0] == AUTO_TOGGLE_OPEN)
            {
               fsa[i].auto_toggle = ON;
            }
            else
            {
               fsa[i].auto_toggle = OFF;
            }
            fsa[i].host_dsp_name[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[(int)fsa[i].host_toggle];
            fsa[i].host_dsp_name[(int)(fsa[i].toggle_pos + 1)] = '\0';
            if (fsa[i].real_hostname[0][0] == '\0')
            {
               (void)strcpy(fsa[i].real_hostname[0], fsa[i].host_dsp_name);
               (void)memcpy(hl[i].real_hostname[0], fsa[i].real_hostname[0], MAX_REAL_HOSTNAME_LENGTH);
            }
            if (fsa[i].real_hostname[1][0] == '\0')
            {
               (void)strcpy(fsa[i].real_hostname[1], fsa[i].host_dsp_name);
               if (fsa[i].host_toggle == HOST_ONE)
               {
                  fsa[i].host_dsp_name[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[HOST_TWO];
               }
               else
               {
                  fsa[i].host_dsp_name[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[HOST_ONE];
               }
               (void)memcpy(hl[i].real_hostname[1], fsa[i].real_hostname[1], MAX_REAL_HOSTNAME_LENGTH);
            }
         }
         (void)memset(&hl[i].fullname[0], 0, MAX_FILENAME_LENGTH);

         fsa[i].host_status         = 0;
         if (hl[i].host_status & STOP_TRANSFER_STAT)
         {
            fsa[i].host_status |= STOP_TRANSFER_STAT;
         }
         if (hl[i].host_status & PAUSE_QUEUE_STAT)
         {
            fsa[i].host_status |= PAUSE_QUEUE_STAT;
         }
         if (hl[i].host_status & HOST_ERROR_OFFLINE_STATIC)
         {
            fsa[i].host_status |= HOST_ERROR_OFFLINE_STATIC;
         }
         if (hl[i].host_status & DO_NOT_DELETE_DATA)
         {
            fsa[i].host_status |= DO_NOT_DELETE_DATA;
         }
         if (hl[i].host_status & SIMULATE_SEND_MODE)
         {
            fsa[i].host_status |= SIMULATE_SEND_MODE;
         }
         fsa[i].error_counter       = 0;
         fsa[i].total_errors        = 0;
         for (k = 0; k < ERROR_HISTORY_LENGTH; k++)
         {
            fsa[i].error_history[k] = 0;
         }
         fsa[i].jobs_queued         = 0;
         fsa[i].file_counter_done   = 0;
         fsa[i].bytes_send          = 0;
         fsa[i].connections         = 0;
         fsa[i].active_transfers    = 0;
         fsa[i].successful_retries  = 0;
         fsa[i].debug               = NO;
         fsa[i].last_connection = fsa[i].last_retry_time = time(NULL);
         fsa[i].first_error_time    = 0L;
         fsa[i].start_event_handle  = 0L;
         fsa[i].end_event_handle    = 0L;
         (void)memset(&fsa[i].job_status, 0, size);
         for (k = 0; k < fsa[i].allowed_transfers; k++)
         {
            fsa[i].job_status[k].connect_status = DISCONNECT;
            fsa[i].job_status[k].proc_id = -1;
#ifdef _WITH_BURST_2
            fsa[i].job_status[k].job_id = NO_ID;
#endif
         }
         for (k = fsa[i].allowed_transfers; k < MAX_NO_PARALLEL_JOBS; k++)
         {
            fsa[i].job_status[k].no_of_files = -1;
            fsa[i].job_status[k].proc_id = -1;
         }
      } /* for (i = 0; i < no_of_hosts; i++) */

      /* Copy configuration information from the old FSA when this */
      /* is stored in system_data file.                            */
      if (get_system_data(&sd) == SUCCESS)
      {
         ptr = (char *)fsa - AFD_FEATURE_FLAG_OFFSET_END;
         *ptr = sd.fsa_feature_flag;
      }
   }
   else /* There is an old database file. */
   {
      int  host_pos,
           no_of_gotchas = 0;
      char *gotcha = NULL;

      /*
       * The gotcha array is used to find hosts that are in the
       * old FSA but not in the HOST_CONFIG file.
       */
      if ((gotcha = malloc(old_no_of_hosts)) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "malloc() error [%d bytes] : %s",
                    old_no_of_hosts, strerror(errno));
         exit(INCORRECT);
      }
      (void)memset(gotcha, NO, old_no_of_hosts);

      for (i = 0; i < no_of_hosts; i++)
      {
         (void)memcpy(fsa[i].host_alias, hl[i].host_alias, MAX_HOSTNAME_LENGTH + 1);
         (void)memcpy(fsa[i].host_dsp_name, fsa[i].host_alias, MAX_HOSTNAME_LENGTH + 1);
         (void)memcpy(fsa[i].real_hostname[0], hl[i].real_hostname[0], MAX_REAL_HOSTNAME_LENGTH);
         (void)memcpy(fsa[i].real_hostname[1], hl[i].real_hostname[1], MAX_REAL_HOSTNAME_LENGTH);
         (void)memcpy(fsa[i].proxy_name, hl[i].proxy_name, MAX_PROXY_NAME_LENGTH + 1);
         fsa[i].allowed_transfers      = hl[i].allowed_transfers;
         fsa[i].max_errors             = hl[i].max_errors;
         fsa[i].retry_interval         = hl[i].retry_interval;
         fsa[i].block_size             = hl[i].transfer_blksize;
         fsa[i].max_successful_retries = hl[i].successful_retries;
         fsa[i].file_size_offset       = hl[i].file_size_offset;
         fsa[i].transfer_timeout       = hl[i].transfer_timeout;
         fsa[i].protocol               = hl[i].protocol;
         fsa[i].protocol_options       = hl[i].protocol_options;
         fsa[i].protocol_options2      = hl[i].protocol_options2;
         fsa[i].transfer_rate_limit    = hl[i].transfer_rate_limit;
         fsa[i].ttl                    = hl[i].ttl;
         fsa[i].socksnd_bufsize        = hl[i].socksnd_bufsize;
         fsa[i].sockrcv_bufsize        = hl[i].sockrcv_bufsize;
         fsa[i].keep_connected         = hl[i].keep_connected;
         fsa[i].warn_time              = hl[i].warn_time;
#ifdef WITH_DUP_CHECK
         fsa[i].dup_check_flag         = hl[i].dup_check_flag;
         fsa[i].dup_check_timeout      = hl[i].dup_check_timeout;
#endif
         if (hl[i].host_status & HOST_TWO_FLAG)
         {
            fsa[i].host_toggle = HOST_TWO;
         }
         else
         {
            fsa[i].host_toggle = DEFAULT_TOGGLE_HOST;
         }

         /*
          * Search in the old FSA for this host. If it is there use
          * the values from the old FSA or else initialise them with
          * defaults. When we find an old entry, remember this so we
          * can later check if there are entries in the old FSA but
          * there are no corresponding entries in the HOST_CONFIG.
          * This will then have to be updated in the HOST_CONFIG file.
          */
         host_pos = INCORRECT;
         for (k = 0; k < old_no_of_hosts; k++)
         {
            if (gotcha[k] != YES)
            {
               if (CHECK_STRCMP(old_fsa[k].host_alias, hl[i].host_alias) == 0)
               {
                  host_pos = k;
                  break;
               }
            }
         }

         if (host_pos != INCORRECT)
         {
            no_of_gotchas++;
            gotcha[host_pos] = YES;

            /*
             * When restarting the AMG and we did change the switching
             * information we will not recognise this change. Thus we have
             * to always evaluate the host name :-(
             */
            fsa[i].toggle_pos = strlen(fsa[i].host_alias);
            if (hl[i].host_toggle_str[0] == '\0')
            {
               fsa[i].host_toggle_str[0]  = '\0';
               fsa[i].original_toggle_pos = NONE;
               fsa[i].host_toggle         = DEFAULT_TOGGLE_HOST;
               if (fsa[i].real_hostname[0][0] == '\0')
               {
                  (void)memcpy(fsa[i].real_hostname[0], hl[i].fullname, MAX_REAL_HOSTNAME_LENGTH);
                  (void)memcpy(hl[i].real_hostname[0], hl[i].fullname, MAX_REAL_HOSTNAME_LENGTH);
               }
            }
            else
            {
               (void)memcpy(fsa[i].host_toggle_str, hl[i].host_toggle_str, MAX_TOGGLE_STR_LENGTH);
               if (hl[i].host_toggle_str[0] == AUTO_TOGGLE_OPEN)
               {
                  fsa[i].auto_toggle = ON;
                  if (old_fsa[host_pos].original_toggle_pos == NONE)
                  {
                     fsa[i].successful_retries  = 0;
                  }
                  else
                  {
                     fsa[i].successful_retries  = old_fsa[host_pos].successful_retries;
                  }
               }
               else
               {
                  fsa[i].auto_toggle = OFF;
                  fsa[i].original_toggle_pos = NONE;
                  fsa[i].successful_retries  = 0;
               }
               fsa[i].host_dsp_name[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[(int)fsa[i].host_toggle];
               fsa[i].host_dsp_name[(int)(fsa[i].toggle_pos + 1)] = '\0';
               if (fsa[i].real_hostname[0][0] == '\0')
               {
                  (void)strcpy(fsa[i].real_hostname[0], fsa[i].host_dsp_name);
                  (void)memcpy(hl[i].real_hostname[0], fsa[i].real_hostname[0], MAX_REAL_HOSTNAME_LENGTH);
               }
               if (fsa[i].real_hostname[1][0] == '\0')
               {
                  (void)strcpy(fsa[i].real_hostname[1], fsa[i].host_dsp_name);
                  if (fsa[i].host_toggle == HOST_ONE)
                  {
                     fsa[i].host_dsp_name[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[HOST_TWO];
                  }
                  else
                  {
                     fsa[i].host_dsp_name[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[HOST_ONE];
                  }
                  (void)memcpy(hl[i].real_hostname[1], fsa[i].real_hostname[1], MAX_REAL_HOSTNAME_LENGTH);
               }
            }
            (void)memset(&hl[i].fullname[0], 0, MAX_FILENAME_LENGTH);

            if (fsa[i].real_hostname[0][0] == '\0')
            {
               (void)memcpy(fsa[i].real_hostname[0], old_fsa[host_pos].real_hostname[0], MAX_REAL_HOSTNAME_LENGTH);
               (void)memcpy(hl[i].real_hostname[0], old_fsa[host_pos].real_hostname[0], MAX_REAL_HOSTNAME_LENGTH);
            }
            if (fsa[i].real_hostname[1][0] == '\0')
            {
               (void)memcpy(fsa[i].real_hostname[1], old_fsa[host_pos].real_hostname[1], MAX_REAL_HOSTNAME_LENGTH);
               (void)memcpy(hl[i].real_hostname[1], old_fsa[host_pos].real_hostname[1], MAX_REAL_HOSTNAME_LENGTH);
            }
            fsa[i].host_status            = old_fsa[host_pos].host_status;
            fsa[i].error_counter          = old_fsa[host_pos].error_counter;
            fsa[i].total_errors           = old_fsa[host_pos].total_errors;
            for (k = 0; k < ERROR_HISTORY_LENGTH; k++)
            {
               fsa[i].error_history[k]    = old_fsa[host_pos].error_history[k];
            }
            fsa[i].jobs_queued            = old_fsa[host_pos].jobs_queued;
            fsa[i].file_counter_done      = old_fsa[host_pos].file_counter_done;
            fsa[i].bytes_send             = old_fsa[host_pos].bytes_send;
            fsa[i].connections            = old_fsa[host_pos].connections;
            fsa[i].active_transfers       = old_fsa[host_pos].active_transfers;
            fsa[i].last_connection        = old_fsa[host_pos].last_connection;
            fsa[i].last_retry_time        = old_fsa[host_pos].last_retry_time;
            fsa[i].first_error_time       = old_fsa[host_pos].first_error_time;
            fsa[i].start_event_handle     = old_fsa[host_pos].start_event_handle;
            fsa[i].end_event_handle       = old_fsa[host_pos].end_event_handle;
            fsa[i].total_file_counter     = old_fsa[host_pos].total_file_counter;
            fsa[i].total_file_size        = old_fsa[host_pos].total_file_size;
            fsa[i].debug                  = old_fsa[host_pos].debug;
            fsa[i].special_flag           = old_fsa[host_pos].special_flag;
            fsa[i].original_toggle_pos    = old_fsa[host_pos].original_toggle_pos;
            if (old_fsa[host_pos].host_id == 0)
            {
               fsa[i].host_id             = get_str_checksum(fsa[i].host_alias);
            }
            else
            {
               fsa[i].host_id             = old_fsa[host_pos].host_id;
            }
            if (fsa[i].active_transfers > 1)
            {
               fsa[i].trl_per_process     = fsa[i].transfer_rate_limit /
                                            fsa[i].active_transfers;
            }
            else
            {
               fsa[i].trl_per_process     = fsa[i].transfer_rate_limit;
            }

            /* Copy all job entries. */
            (void)memcpy(&fsa[i].job_status, &old_fsa[host_pos].job_status, size);
         }
         else /* This host is not in the old FSA, therefor it is new. */
         {
            fsa[i].original_toggle_pos = NONE;
            fsa[i].toggle_pos = strlen(fsa[i].host_alias);
            if (hl[i].host_toggle_str[0] == '\0')
            {
               fsa[i].host_toggle_str[0]  = '\0';
               if (fsa[i].real_hostname[0][0] == '\0')
               {
                  (void)memcpy(fsa[i].real_hostname[0], hl[i].fullname, MAX_REAL_HOSTNAME_LENGTH);
                  (void)memcpy(hl[i].real_hostname[0], hl[i].fullname, MAX_REAL_HOSTNAME_LENGTH);
               }
            }
            else
            {
               (void)memcpy(fsa[i].host_toggle_str, hl[i].host_toggle_str, MAX_TOGGLE_STR_LENGTH);
               if (hl[i].host_toggle_str[0] == AUTO_TOGGLE_OPEN)
               {
                  fsa[i].auto_toggle = ON;
               }
               else
               {
                  fsa[i].auto_toggle = OFF;
               }
               fsa[i].host_dsp_name[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[(int)fsa[i].host_toggle];
               fsa[i].host_dsp_name[(int)(fsa[i].toggle_pos + 1)] = '\0';
               if (fsa[i].real_hostname[0][0] == '\0')
               {
                  (void)strcpy(fsa[i].real_hostname[0], fsa[i].host_dsp_name);
                  (void)memcpy(hl[i].real_hostname[0], fsa[i].real_hostname[0], MAX_REAL_HOSTNAME_LENGTH);
               }
               if (fsa[i].real_hostname[1][0] == '\0')
               {
                  (void)strcpy(fsa[i].real_hostname[1], fsa[i].host_dsp_name);
                  if (fsa[i].host_toggle == HOST_ONE)
                  {
                     fsa[i].host_dsp_name[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[HOST_TWO];
                  }
                  else
                  {
                     fsa[i].host_dsp_name[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[HOST_ONE];
                  }
                  (void)memcpy(hl[i].real_hostname[1], fsa[i].real_hostname[1], MAX_REAL_HOSTNAME_LENGTH);
               }
            }
            (void)memset(&hl[i].fullname[0], 0, MAX_FILENAME_LENGTH);

            fsa[i].host_status         = 0;
            fsa[i].error_counter       = 0;
            fsa[i].total_errors        = 0;
            for (k = 0; k < ERROR_HISTORY_LENGTH; k++)
            {
               fsa[i].error_history[k] = 0;
            }
            fsa[i].jobs_queued         = 0;
            fsa[i].file_counter_done   = 0;
            fsa[i].bytes_send          = 0;
            fsa[i].connections         = 0;
            fsa[i].active_transfers    = 0;
            fsa[i].total_file_counter  = 0;
            fsa[i].total_file_size     = 0;
            fsa[i].special_flag        = 0;
            fsa[i].successful_retries  = 0;
            fsa[i].trl_per_process     = fsa[i].transfer_rate_limit;
            fsa[i].debug               = NO;
            fsa[i].host_id             = get_str_checksum(fsa[i].host_alias);
            fsa[i].last_connection = fsa[i].last_retry_time = time(NULL);
            fsa[i].first_error_time    = 0L;
            fsa[i].start_event_handle  = 0L;
            fsa[i].end_event_handle    = 0L;
            memset(&fsa[i].job_status, 0, size);
            for (k = 0; k < fsa[i].allowed_transfers; k++)
            {
               fsa[i].job_status[k].connect_status = DISCONNECT;
               fsa[i].job_status[k].proc_id = -1;
#ifdef _WITH_BURST_2
               fsa[i].job_status[k].job_id = NO_ID;
#endif
            }
            for (k = fsa[i].allowed_transfers; k < MAX_NO_PARALLEL_JOBS; k++)
            {
               fsa[i].job_status[k].no_of_files = -1;
               fsa[i].job_status[k].proc_id = -1;
            }
         }

         if (hl[i].in_dir_config == YES)
         {
            fsa[i].special_flag |= HOST_IN_DIR_CONFIG;
            hl[i].host_status &= ~HOST_NOT_IN_DIR_CONFIG;
         }
         else
         {
            fsa[i].special_flag &= ~HOST_IN_DIR_CONFIG;
            hl[i].host_status |= HOST_NOT_IN_DIR_CONFIG;
         }
         if (hl[i].host_status & HOST_CONFIG_HOST_DISABLED)
         {
            fsa[i].special_flag |= HOST_DISABLED;
         }
         else
         {
            fsa[i].special_flag &= ~HOST_DISABLED;
         }
         if (hl[i].protocol_options & KEEP_CON_NO_SEND_2)
         {
            fsa[i].special_flag |= KEEP_CON_NO_SEND;
         }
         else
         {
            fsa[i].special_flag &= ~KEEP_CON_NO_SEND;
         }
         if (hl[i].protocol_options & KEEP_CON_NO_FETCH_2)
         {
            fsa[i].special_flag |= KEEP_CON_NO_FETCH;
         }
         else
         {
            fsa[i].special_flag &= ~KEEP_CON_NO_FETCH;
         }
         if (hl[i].host_status & STOP_TRANSFER_STAT)
         {
            fsa[i].host_status |= STOP_TRANSFER_STAT;
         }
         else
         {
            fsa[i].host_status &= ~STOP_TRANSFER_STAT;
         }
         if (hl[i].host_status & PAUSE_QUEUE_STAT)
         {
            fsa[i].host_status |= PAUSE_QUEUE_STAT;
         }
         else
         {
            fsa[i].host_status &= ~PAUSE_QUEUE_STAT;
         }
         if (hl[i].host_status & HOST_ERROR_OFFLINE_STATIC)
         {
            fsa[i].host_status |= HOST_ERROR_OFFLINE_STATIC;
         }
         else
         {
            fsa[i].host_status &= ~HOST_ERROR_OFFLINE_STATIC;
         }
         if (hl[i].host_status & DO_NOT_DELETE_DATA)
         {
            fsa[i].host_status |= DO_NOT_DELETE_DATA;
         }
         else
         {
            fsa[i].host_status &= ~DO_NOT_DELETE_DATA;
         }
         if (hl[i].host_status & SIMULATE_SEND_MODE)
         {
            fsa[i].host_status |= SIMULATE_SEND_MODE;
         }
         else
         {
            fsa[i].host_status &= ~SIMULATE_SEND_MODE;
         }
      } /* for (i = 0; i < no_of_hosts; i++) */

      /*
       * Check if there is a host entry in the old FSA but NOT in
       * the HOST_CONFIG.
       */
      if (gotcha != NULL)
      {
         if (no_of_gotchas != old_no_of_hosts)
         {
            int no_of_new_old_hosts = old_no_of_hosts - no_of_gotchas,
                j;

            /*
             * It could be that some of the new old hosts should be
             * deleted. The only way to find this out is to see
             * if they still have files to be send.
             */
            for (j = 0; j < old_no_of_hosts; j++)
            {
               if ((gotcha[j] == NO) &&
                   (old_fsa[j].total_file_counter == 0))
               {
                  /* This host is to be removed! */
                  no_of_new_old_hosts--;
                  gotcha[j] = YES;
               }
            }

            if (no_of_new_old_hosts > 0)
            {
               fsa_size += (no_of_new_old_hosts * sizeof(struct filetransfer_status));
               no_of_hosts += no_of_new_old_hosts;

               /*
                * Resize the host_list structure if necessary.
                */
               if ((no_of_hosts % HOST_BUF_SIZE) == 0)
               {
                  size_t new_size,
                         offset;

                  /* Calculate new size for host list. */
                  new_size = ((no_of_hosts / HOST_BUF_SIZE) + 1) *
                             HOST_BUF_SIZE * sizeof(struct host_list);

                  /* Now increase the space. */
                  if ((hl = realloc(hl, new_size)) == NULL)
                  {
                     system_log(FATAL_SIGN, __FILE__, __LINE__,
                                "Could not reallocate memory [%d bytes] for host list : %s",
                                new_size, strerror(errno));
                     exit(INCORRECT);
                  }

                  /* Initialise the new memory area. */
                  new_size = HOST_BUF_SIZE * sizeof(struct host_list);
                  offset = (no_of_hosts / HOST_BUF_SIZE) * new_size;
                  memset((char *)hl + offset, 0, new_size);
               }

               /*
                * We now have to make the FSA and host_list structure
                * larger to store the 'new' hosts.
                */
               ptr = (char *)fsa;
               ptr -= AFD_WORD_OFFSET;
               if (fsa_size > 0)
               {
#ifdef HAVE_MMAP
                  if (munmap(ptr, fsa_size) == -1)
#else
                  if (msync_emu(ptr) == -1)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "msync_emu() error");
                  }
                  if (munmap_emu(ptr) == -1)
#endif
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Failed to munmap() %s : %s",
                                new_fsa_stat, strerror(errno));
                  }
               }
               if (lseek(fsa_fd, fsa_size - 1, SEEK_SET) == -1)
               {
                  system_log(FATAL_SIGN, __FILE__, __LINE__,
                             "Failed to lseek() in %s : %s",
                             new_fsa_stat, strerror(errno));
                  exit(INCORRECT);
               }
               if (write(fsa_fd, "", 1) != 1)
               {
                     system_log(FATAL_SIGN, __FILE__, __LINE__,
                                "write() error : %s", strerror(errno));
                  exit(INCORRECT);
               }
#ifdef HAVE_MMAP
               if ((ptr = mmap(NULL, fsa_size, (PROT_READ | PROT_WRITE),
                               MAP_SHARED, fsa_fd, 0)) == (caddr_t) -1)
#else
               if ((ptr = mmap_emu(NULL, fsa_size, (PROT_READ | PROT_WRITE),
                                   MAP_SHARED,
                                   new_fsa_stat, 0)) == (caddr_t) -1)
#endif
               {
                  system_log(FATAL_SIGN, __FILE__, __LINE__,
                             "mmap() error : %s", strerror(errno));
                  exit(INCORRECT);
               }

               /* Write new number of hosts to memory mapped region. */
               *(int *)ptr = no_of_hosts;

               /* Reposition fsa pointer after no_of_hosts. */
               ptr += AFD_WORD_OFFSET;
               fsa = (struct filetransfer_status *)ptr;

               /*
                * Insert the 'new' old hosts.
                */
               for (j = 0; j < old_no_of_hosts; j++)
               {
                  if (gotcha[j] == NO)
                  {
                     /* Position the new host there were it was in the old FSA. */
                     if (j < i)
                     {
                        size_t move_size = (i - j) * sizeof(struct filetransfer_status);

                        (void)memmove(&fsa[j + 1], &fsa[j], move_size);

                        move_size = (i - j) * sizeof(struct host_list);
                        (void)memmove(&hl[j + 1], &hl[j], move_size);
                     }

                     /* Insert 'new' old host in FSA. */
                     (void)memcpy(&fsa[j], &old_fsa[j], sizeof(struct filetransfer_status));
                     for (k = 0; k < fsa[j].allowed_transfers; k++)
                     {
                        if (fsa[j].job_status[k].no_of_files == -1)
                        {
                           fsa[j].job_status[k].no_of_files = 0;
                           fsa[j].job_status[k].proc_id = -1;
                           fsa[j].job_status[k].connect_status = DISCONNECT;
#ifdef _WITH_BURST_2
                           fsa[j].job_status[k].job_id = NO_ID;
#endif
                        }
                     }
                     for (k = fsa[j].allowed_transfers; k < MAX_NO_PARALLEL_JOBS; k++)
                     {
                        fsa[j].job_status[k].no_of_files = -1;
                        fsa[j].job_status[k].proc_id = -1;
                     }

                     /* Insert 'new' old host in host_list structure. */
                     (void)memcpy(hl[j].host_alias, fsa[j].host_alias, MAX_HOSTNAME_LENGTH + 1);
                     (void)memcpy(hl[j].real_hostname[0], fsa[j].real_hostname[0], MAX_REAL_HOSTNAME_LENGTH);
                     (void)memcpy(hl[j].real_hostname[1], fsa[j].real_hostname[1], MAX_REAL_HOSTNAME_LENGTH);
                     (void)memcpy(hl[j].proxy_name, fsa[j].proxy_name, MAX_PROXY_NAME_LENGTH + 1);
                     (void)memset(&hl[j].fullname[0], 0, MAX_FILENAME_LENGTH);
                     hl[j].allowed_transfers   = fsa[j].allowed_transfers;
                     hl[j].max_errors          = fsa[j].max_errors;
                     hl[j].retry_interval      = fsa[j].retry_interval;
                     hl[j].transfer_blksize    = fsa[j].block_size;
                     hl[j].successful_retries  = fsa[j].max_successful_retries;
                     hl[j].file_size_offset    = fsa[j].file_size_offset;
                     hl[j].transfer_timeout    = fsa[j].transfer_timeout;
                     hl[j].transfer_rate_limit = fsa[j].transfer_rate_limit;
                     hl[j].ttl                 = fsa[j].ttl;
                     hl[j].socksnd_bufsize     = fsa[j].socksnd_bufsize;
                     hl[j].sockrcv_bufsize     = fsa[j].sockrcv_bufsize;
                     hl[j].keep_connected      = fsa[j].keep_connected;
                     hl[j].warn_time           = fsa[j].warn_time;
#ifdef WITH_DUP_CHECK
                     hl[j].dup_check_flag      = fsa[j].dup_check_flag;
                     hl[j].dup_check_timeout   = fsa[j].dup_check_timeout;
#endif
                     hl[j].protocol            = fsa[j].protocol;
                     hl[j].protocol_options    = fsa[j].protocol_options;
                     hl[j].protocol_options2   = fsa[j].protocol_options2;
                     hl[j].in_dir_config       = NO;
                     fsa[j].special_flag &= ~HOST_IN_DIR_CONFIG;
                     hl[j].host_status = 0;
                     if (fsa[j].host_status & HOST_ERROR_OFFLINE_STATIC)
                     {
                        hl[j].host_status |= HOST_ERROR_OFFLINE_STATIC;
                     }
                     if (fsa[j].special_flag & HOST_DISABLED)
                     {
                        hl[j].host_status |= HOST_CONFIG_HOST_DISABLED;
                     }
                     if (fsa[j].special_flag & KEEP_CON_NO_SEND)
                     {
                        hl[j].protocol_options |= KEEP_CON_NO_SEND_2;
                     }
                     if (fsa[j].special_flag & KEEP_CON_NO_FETCH)
                     {
                        hl[j].protocol_options |= KEEP_CON_NO_FETCH_2;
                     }
                     if ((fsa[j].special_flag & HOST_IN_DIR_CONFIG) == 0)
                     {
                        hl[j].host_status |= HOST_NOT_IN_DIR_CONFIG;
                     }
                     if (fsa[j].host_status & STOP_TRANSFER_STAT)
                     {
                        hl[j].host_status |= STOP_TRANSFER_STAT;
                     }
                     if (fsa[j].host_status & PAUSE_QUEUE_STAT)
                     {
                        hl[j].host_status |= PAUSE_QUEUE_STAT;
                     }
                     if (fsa[j].host_toggle == HOST_TWO)
                     {
                        hl[j].host_status |= HOST_TWO_FLAG;
                     }
                     if (fsa[j].host_status & DO_NOT_DELETE_DATA)
                     {
                        hl[j].host_status |= DO_NOT_DELETE_DATA;
                     }
                     if (fsa[j].host_status & SIMULATE_SEND_MODE)
                     {
                        hl[j].host_status |= SIMULATE_SEND_MODE;
                     }

                     i++;
                  } /* if (gotcha[j] == NO) */
               } /* for (j = 0; j < old_no_of_hosts; j++) */
            } /* if (no_of_new_old_hosts > 0) */
         }

         free(gotcha);
      }

      /* Copy configuration information from the old FSA. */
      ptr = (char *)fsa - AFD_FEATURE_FLAG_OFFSET_END;
      *ptr = *((char *)old_fsa - AFD_FEATURE_FLAG_OFFSET_END);
   }

   /* Reposition fsa pointer after no_of_hosts. */
   ptr = (char *)fsa;
   ptr -= AFD_WORD_OFFSET;
   *(ptr + SIZEOF_INT + 1 + 1) = ignore_first_errors;
   *(ptr + SIZEOF_INT + 1 + 1 + 1) = CURRENT_FSA_VERSION; /* FSA version number. */
   if ((pagesize = (int)sysconf(_SC_PAGESIZE)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to determine the pagesize with sysconf() : %s",
                 strerror(errno));
   }
   *(int *)(ptr + SIZEOF_INT + 4) = pagesize;
   *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;        /* Not used. */
   *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;    /* Not used. */
   *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;    /* Not used. */
   *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;    /* Not used. */
   if (fsa_size > 0)
   {
#ifdef HAVE_MMAP
      if (munmap(ptr, fsa_size) == -1)
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
                    new_fsa_stat, strerror(errno));
      }
   }
   fsa = NULL;

   /*
    * Unmap from old memory mapped region.
    */
   if (first_time == NO)
   {
      ptr = (char *)old_fsa;
      ptr -= AFD_WORD_OFFSET;

      /* Don't forget to unmap old FSA file. */
      if (old_fsa_size > 0)
      {
#ifdef HAVE_MMAP
         if (munmap(ptr, old_fsa_size) == -1)
#else
         if (munmap_emu(ptr) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to munmap() %s : %s",
                       old_fsa_stat, strerror(errno));
         }
      }
   }

   /* Remove the old FSA file if there was one. */
   if (old_fsa_size > -1)
   {
      if (unlink(old_fsa_stat) < 0)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to unlink() %s : %s",
                    old_fsa_stat, strerror(errno));
      }
   }

   /*
    * Copy the new fsa_id into the locked FSA_ID_FILE file, unlock
    * and close the file.
    */
   /* Go to beginning in file. */
   if (lseek(fsa_id_fd, 0, SEEK_SET) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not seek() to beginning of %s : %s",
                 fsa_id_file, strerror(errno));
   }

   /* Write new value into FSA_ID_FILE file. */
   if (write(fsa_id_fd, &fsa_id, sizeof(int)) != sizeof(int))
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not write value to FSA ID file : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Close and unlock FSA_ID_FILE. */
   if (close(fsa_id_fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }

   /* Close file with new FSA. */
   if (close(fsa_fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }
   fsa_fd = -1;

   /* Close old FSA file. */
   if (old_fsa_fd != -1)
   {
      if (close(old_fsa_fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "close() error : %s", strerror(errno));
      }
   }

   (void)write_typesize_data();

   return;
}
