/*
 *  create_fra.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   create_fra - creates the FRA of the AFD
 **
 ** SYNOPSIS
 **   void create_fra(int no_of_dirs)
 **
 ** DESCRIPTION
 **   This function creates the FRA (File Retrieve Area), to which
 **   most process of the AFD will map. The FRA has the following
 **   structure:
 **
 **      <AFD_WORD_OFFSET><struct fileretrieve_status fra[no_of_dirs]>
 **
 **   A detailed description of the structures fileretrieve_status
 **   can be found in afddefs.h. The variable no_of_dirs in AFD_WORD_OFFSET
 **   is the number of directories from where the destinations get there
 **   data. This variable can have the value STALE (-1), which will tell
 **   all other process to unmap from this area and map to the new area.
 **
 ** RETURN VALUES
 **   None. Will exit with incorrect if any of the system call will
 **   fail.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.03.2000 H.Kiehl Created
 **   05.04.2001 H.Kiehl Fill file with data before it is mapped.
 **   18.04.2001 H.Kiehl Add version number to structure.
 **   03.05.2002 H.Kiehl Added new elements to FRA structure to show
 **                      total number of files in directory.
 **   17.08.2003 H.Kiehl Added new elements wait_for_filename, accumulate
 **                      and accumulate_size.
 **   09.02.2005 H.Kiehl Added additional time entry structure to enable
 **                      alarm times.
 **   24.02.2007 H.Kiehl Added inotify support.
 **   28.05.2012 H.Kiehl Added 'create sourde dir' support.
 **   26.01.2019 H.Kiehl Added dir_mtime support.
 **   01.08.2019 H.Kiehl Added pagesize.
 **   28.01.2023 H.Kiehl Always create FRA one entry larger then needed,
 **                      so that get_new_positions() can direct writes
 **                      of no longer existing entries to the end of FRA
 **                      where it is not visible to user.
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
#ifdef HAVE_MMAP
# include <sys/mman.h>              /* mmap(), munmap()                  */
#endif
#include <unistd.h>                 /* read(), write(), close(), lseek() */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "amgdefs.h"


/* External global variables. */
extern char                       *p_work_dir;
extern int                        fra_id,
                                  fra_fd;
extern off_t                      fra_size;
extern struct dir_data            *dd;
extern struct fileretrieve_status *fra;


/*############################ create_fra() #############################*/
void
create_fra(int no_of_dirs)
{
   int                        i,
                              fra_id_fd,
                              loops,
                              old_fra_fd = -1,
                              old_fra_id,
                              old_no_of_dirs = -1,
                              pagesize,
                              rest;
   off_t                      old_fra_size = -1;
   time_t                     current_time;
   char                       buffer[4096],
                              fra_id_file[MAX_PATH_LENGTH],
                              new_fra_stat[MAX_PATH_LENGTH],
                              old_fra_stat[MAX_PATH_LENGTH],
                              *ptr = NULL;
   struct fileretrieve_status *old_fra = NULL;
   struct flock               wlock = {F_WRLCK, SEEK_SET, 0, 1},
                              ulock = {F_UNLCK, SEEK_SET, 0, 1};
#ifdef HAVE_STATX
   struct statx               stat_buf;
#else
   struct stat                stat_buf;
#endif

   fra_size = -1;

   /* Initialise all pathnames and file descriptors. */
   (void)strcpy(fra_id_file, p_work_dir);
   (void)strcat(fra_id_file, FIFO_DIR);
   (void)strcpy(old_fra_stat, fra_id_file);
   (void)strcat(old_fra_stat, FRA_STAT_FILE);
   (void)strcat(fra_id_file, FRA_ID_FILE);

   /*
    * First just try open the fra_id_file. If this fails create
    * the file and initialise old_fra_id with -1.
    */
   if ((fra_id_fd = open(fra_id_file, O_RDWR)) > -1)
   {
      /*
       * Lock FRA ID file. If it is already locked
       * wait for it to clear the lock again.
       */
      if (fcntl(fra_id_fd, F_SETLKW, &wlock) < 0)
      {
         /* Is lock already set or are we setting it again? */
         if ((errno != EACCES) && (errno != EAGAIN) && (errno != EBUSY))
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Could not set write lock for %s : %s",
                       fra_id_file, strerror(errno));
            exit(INCORRECT);
         }
      }

      /* Read the FRA file ID. */
      if (read(fra_id_fd, &old_fra_id, sizeof(int)) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not read the value of the FRA file ID : %s",
                    strerror(errno));
         exit(INCORRECT);
      }
   }
   else
   {
      if ((fra_id_fd = open(fra_id_file, (O_RDWR | O_CREAT), FILE_MODE)) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not open %s : %s", fra_id_file, strerror(errno));
         exit(INCORRECT);
      }
      old_fra_id = -1;
   }

   /*
    * Mark memory mapped region as old, so no process puts
    * any new information into the region after we
    * have copied it into the new region.
    */
   if (old_fra_id > -1)
   {
      /* Attach to old region. */
      ptr = old_fra_stat + strlen(old_fra_stat);
      (void)snprintf(ptr, MAX_PATH_LENGTH - (ptr - old_fra_stat),
                     ".%d", old_fra_id);

      /* Get the size of the old FSA file. */
#ifdef HAVE_STATX
      if (statx(0, old_fra_stat, AT_STATX_SYNC_AS_STAT,
                STATX_SIZE, &stat_buf) < 0)
#else
      if (stat(old_fra_stat, &stat_buf) < 0)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                    "Failed to statx() %s : %s",
#else
                    "Failed to stat() %s : %s",
#endif
                    old_fra_stat, strerror(errno));
         old_fra_id = -1;
      }
      else
      {
#ifdef HAVE_STATX
         if (stat_buf.stx_size > 0)
#else
         if (stat_buf.st_size > 0)
#endif
         {
            if ((old_fra_fd = open(old_fra_stat, O_RDWR)) < 0)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to open() %s : %s",
                          old_fra_stat, strerror(errno));
               old_fra_id = old_fra_fd = -1;
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
                               MAP_SHARED, old_fra_fd, 0)) == (caddr_t) -1)
#else
               if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                                   stat_buf.stx_size,
# else
                                   stat_buf.st_size,
# endif
                                   (PROT_READ | PROT_WRITE),
                                   MAP_SHARED, old_fra_stat, 0)) == (caddr_t) -1)
#endif
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "mmap() error : %s", strerror(errno));
                  old_fra_id = -1;
               }
               else
               {
                  if (*(int *)ptr == STALE)
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "FRA in %s is stale! Ignoring this FRA.",
                                old_fra_stat);
                     old_fra_id = -1;
                  }
                  else
                  {
#ifdef HAVE_STATX
                     old_fra_size = stat_buf.stx_size;
#else
                     old_fra_size = stat_buf.st_size;
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
            old_fra_id = -1;
         }
      }

      if (old_fra_id != -1)
      {
         old_no_of_dirs = *(int *)ptr;

         /* Mark it as stale. */
         *(int *)ptr = STALE;

         /* Check if the version has changed. */
         if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_FRA_VERSION)
         {
            unsigned char old_version = *(ptr + SIZEOF_INT + 1 + 1 + 1);

            /* Unmap old FRA file. */
#ifdef HAVE_MMAP
            if (munmap(ptr, old_fra_size) == -1)
#else
            if (munmap_emu(ptr) == -1)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to munmap() %s : %s",
                          old_fra_stat, strerror(errno));
            }
            if ((ptr = convert_fra(old_fra_fd, old_fra_stat, &old_fra_size,
                                   old_no_of_dirs,
                                   old_version, CURRENT_FRA_VERSION)) == NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to convert_fra() %s", old_fra_stat);
               old_fra_id = -1;
            }
         } /* FRA version has changed. */

         if (old_fra_id != -1)
         {
            /* Move pointer to correct position so */
            /* we can extract the relevant data.   */
            ptr += AFD_WORD_OFFSET;

            old_fra = (struct fileretrieve_status *)ptr;
         }
      }
   }

   /*
    * Create the new mmap region.
    */
   /* First calculate the new size. The + 1 after no_of_dirs is in case */
   /* the function get_new_positions() needs to write some data not     */
   /* visible to the user.                                              */
   fra_size = AFD_WORD_OFFSET +
              ((no_of_dirs + 1) * sizeof(struct fileretrieve_status));

   if ((old_fra_id + 1) > -1)
   {
      fra_id = old_fra_id + 1;
   }
   else
   {
      fra_id = 0;
   }
   (void)snprintf(new_fra_stat, MAX_PATH_LENGTH, "%s%s%s.%d",
                  p_work_dir, FIFO_DIR, FRA_STAT_FILE, fra_id);

   /* Now map the new FRA region to a file. */
   if ((fra_fd = open(new_fra_stat, (O_RDWR | O_CREAT | O_TRUNC),
                      FILE_MODE)) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to open() %s : %s", new_fra_stat, strerror(errno));
      exit(INCORRECT);
   }

   /*
    * Write the complete file before we mmap() to it. If we just lseek()
    * to the end, write one byte and then mmap to it can cause a SIGBUS
    * on some systems when the disk is full and data is written to the
    * mapped area. So to detect that the disk is full always write the
    * complete file we want to map.
    */
   loops = fra_size / 4096;
   rest = fra_size % 4096;
   (void)memset(buffer, 0, 4096);
   for (i = 0; i < loops; i++)
   {
      if (write(fra_fd, buffer, 4096) != 4096)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "write() error : %s", strerror(errno));
         exit(INCORRECT);
      }
   }
   if (rest > 0)
   {
      if (write(fra_fd, buffer, rest) != rest)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "write() error : %s", strerror(errno));
         exit(INCORRECT);
      }
   }

#ifdef HAVE_MMAP
   if ((ptr = mmap(NULL, fra_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
                   fra_fd, 0)) == (caddr_t) -1)
#else
   if ((ptr = mmap_emu(NULL, fra_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
                       new_fra_stat, 0)) == (caddr_t) -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "mmap() error : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Write number of directories to new memory mapped region. */
   *(int *)ptr = no_of_dirs;

   /* Reposition fra pointer after no_of_dirs. */
   ptr += AFD_WORD_OFFSET;
   fra = (struct fileretrieve_status *)ptr;

   /*
    * Copy all the old and new data into the new mapped region.
    */
   current_time = time(NULL);
   if (old_fra_id < 0)
   {
      struct system_data sd;

      /*
       * There is NO old FRA.
       */
      for (i = 0; i < no_of_dirs; i++)
      {
         (void)strcpy(fra[i].dir_alias, dd[i].dir_alias);
         (void)strcpy(fra[i].host_alias, dd[i].host_alias);
         (void)strcpy(fra[i].url, dd[i].url);
         (void)strcpy(fra[i].ls_data_alias, dd[i].ls_data_alias);
         (void)strcpy(fra[i].retrieve_work_dir, dd[i].retrieve_work_dir);
         (void)strcpy(fra[i].wait_for_filename, dd[i].wait_for_filename);
         (void)strcpy(fra[i].timezone, dd[i].timezone);
         fra[i].fsa_pos                = dd[i].fsa_pos;
         fra[i].protocol               = dd[i].protocol;
         fra[i].priority               = dd[i].priority;
         fra[i].delete_files_flag      = dd[i].delete_files_flag;
         fra[i].unknown_file_time      = dd[i].unknown_file_time;
         fra[i].queued_file_time       = dd[i].queued_file_time;
         fra[i].locked_file_time       = dd[i].locked_file_time;
         fra[i].unreadable_file_time   = dd[i].unreadable_file_time;
         fra[i].report_unknown_files   = dd[i].report_unknown_files;
         fra[i].end_character          = dd[i].end_character;
         fra[i].important_dir          = dd[i].important_dir;
         fra[i].no_of_time_entries     = dd[i].no_of_time_entries;
         fra[i].remove                 = dd[i].remove;
         fra[i].stupid_mode            = dd[i].stupid_mode;
         fra[i].force_reread           = dd[i].force_reread;
         fra[i].max_process            = dd[i].max_process;
         fra[i].dir_id                 = dd[i].dir_id;
         fra[i].ignore_size            = dd[i].ignore_size;
         fra[i].ignore_file_time       = dd[i].ignore_file_time;
         fra[i].gt_lt_sign             = dd[i].gt_lt_sign;
         fra[i].keep_connected         = dd[i].keep_connected;
         fra[i].max_copied_files       = dd[i].max_copied_files;
         fra[i].max_copied_file_size   = dd[i].max_copied_file_size;
         fra[i].accumulate_size        = dd[i].accumulate_size;
         fra[i].accumulate             = dd[i].accumulate;
#ifdef WITH_DUP_CHECK
         fra[i].dup_check_timeout      = dd[i].dup_check_timeout;
         fra[i].dup_check_flag         = dd[i].dup_check_flag;
#endif
         fra[i].dir_mode               = dd[i].dir_mode;
         fra[i].info_time              = dd[i].info_time;
         fra[i].warn_time              = dd[i].warn_time;
         fra[i].max_errors             = dd[i].max_errors;
         fra[i].in_dc_flag             = dd[i].in_dc_flag;
         fra[i].last_retrieval         = 0L;
         fra[i].start_event_handle     = 0L;
         fra[i].end_event_handle       = 0L;
         fra[i].dir_mtime              = 0;
         fra[i].bytes_received         = 0;
         fra[i].files_in_dir           = 0;
         fra[i].files_queued           = 0;
         fra[i].bytes_in_dir           = 0;
         fra[i].bytes_in_queue         = 0;
         fra[i].files_received         = 0;
         fra[i].no_of_process          = 0;
         fra[i].dir_status             = NORMAL_STATUS;
         fra[i].queued                 = 0;
         fra[i].error_counter          = 0;

         fra[i].dir_flag               = 0;
         fra[i].dir_options            = 0;
         if (dd[i].accept_dot_files == YES)
         {
            fra[i].dir_options |= ACCEPT_DOT_FILES;
         }
         if (dd[i].do_not_parallelize == YES)
         {
            fra[i].dir_options |= DO_NOT_PARALLELIZE;
         }
         if (dd[i].do_not_move == YES)
         {
            fra[i].dir_options |= DO_NOT_MOVE;
         }
         if (dd[i].do_not_get_dir_list == YES)
         {
            fra[i].dir_options |= DONT_GET_DIR_LIST;
         }
         if (dd[i].url_creates_file_name == YES)
         {
            fra[i].dir_options |= URL_CREATES_FILE_NAME;
         }
         if (dd[i].url_with_index_file_name == YES)
         {
            fra[i].dir_options |= URL_WITH_INDEX_FILE_NAME;
         }
         if (dd[i].no_delimiter == YES)
         {
            fra[i].dir_options |= NO_DELIMITER;
         }
         if (dd[i].keep_path == YES)
         {
            fra[i].dir_options |= KEEP_PATH;
         }
         if (dd[i].one_process_just_scaning == YES)
         {
            fra[i].dir_options |= ONE_PROCESS_JUST_SCANNING;
         }
         if (dd[i].create_source_dir == YES)
         {
            if (fra[i].dir_mode == 0)
            {
               fra[i].dir_mode = DIR_MODE;
            }
         }
         else
         {
            fra[i].dir_mode = 0;
         }
#ifdef WITH_INOTIFY
         if (dd[i].inotify_flag & INOTIFY_RENAME_FLAG)
         {
            fra[i].dir_options |= INOTIFY_RENAME;
         }
         if (dd[i].inotify_flag & INOTIFY_CLOSE_FLAG)
         {
            fra[i].dir_options |= INOTIFY_CLOSE;
         }
         if (dd[i].inotify_flag & INOTIFY_CREATE_FLAG)
         {
            fra[i].dir_options |= INOTIFY_CREATE;
         }
         if (dd[i].inotify_flag & INOTIFY_DELETE_FLAG)
         {
            fra[i].dir_options |= INOTIFY_DELETE;
         }
         if (dd[i].inotify_flag & INOTIFY_ATTRIB_FLAG)
         {
            fra[i].dir_options |= INOTIFY_ATTRIB;
         }
#endif
         if (fra[i].no_of_time_entries > 0)
         {
            (void)memcpy(&fra[i].te[0], &dd[i].te[0],
                         (fra[i].no_of_time_entries * sizeof(struct bd_time_entry)));
            fra[i].next_check_time = calc_next_time_array(fra[i].no_of_time_entries,
                                                          &fra[i].te[0],
#ifdef WITH_TIMEZONE
                                                          fra[i].timezone,
#endif
                                                          current_time,
                                                          __FILE__, __LINE__);
         }
         (void)memset(&fra[i].ate, 0, sizeof(struct bd_time_entry));
      } /* for (i = 0; i < no_of_dirs; i++) */

      /* Copy configuration information from the old FRA when this */
      /* is stored in system_data file.                            */
      if (get_system_data(&sd) == SUCCESS)
      {
         ptr = (char *)fra - AFD_FEATURE_FLAG_OFFSET_END;
         *ptr = sd.fra_feature_flag;
      }
   }
   else /* There is an old database file. */
   {
      int gotcha,
          k;

      for (i = 0; i < no_of_dirs; i++)
      {
         (void)strcpy(fra[i].dir_alias, dd[i].dir_alias);
         (void)strcpy(fra[i].host_alias, dd[i].host_alias);
         (void)strcpy(fra[i].url, dd[i].url);
         (void)strcpy(fra[i].ls_data_alias, dd[i].ls_data_alias);
         (void)strcpy(fra[i].retrieve_work_dir, dd[i].retrieve_work_dir);
         (void)strcpy(fra[i].wait_for_filename, dd[i].wait_for_filename);
         (void)strcpy(fra[i].timezone, dd[i].timezone);
         fra[i].fsa_pos                = dd[i].fsa_pos;
         fra[i].protocol               = dd[i].protocol;
         fra[i].priority               = dd[i].priority;
         fra[i].delete_files_flag      = dd[i].delete_files_flag;
         fra[i].unknown_file_time      = dd[i].unknown_file_time;
         fra[i].queued_file_time       = dd[i].queued_file_time;
         fra[i].locked_file_time       = dd[i].locked_file_time;
         fra[i].unreadable_file_time   = dd[i].unreadable_file_time;
         fra[i].report_unknown_files   = dd[i].report_unknown_files;
         fra[i].end_character          = dd[i].end_character;
         fra[i].important_dir          = dd[i].important_dir;
         fra[i].no_of_time_entries     = dd[i].no_of_time_entries;
         fra[i].remove                 = dd[i].remove;
         fra[i].stupid_mode            = dd[i].stupid_mode;
         fra[i].force_reread           = dd[i].force_reread;
         fra[i].max_process            = dd[i].max_process;
         fra[i].dir_id                 = dd[i].dir_id;
         fra[i].ignore_size            = dd[i].ignore_size;
         fra[i].ignore_file_time       = dd[i].ignore_file_time;
         fra[i].gt_lt_sign             = dd[i].gt_lt_sign;
         fra[i].keep_connected         = dd[i].keep_connected;
         fra[i].max_copied_files       = dd[i].max_copied_files;
         fra[i].max_copied_file_size   = dd[i].max_copied_file_size;
         fra[i].accumulate_size        = dd[i].accumulate_size;
         fra[i].accumulate             = dd[i].accumulate;
#ifdef WITH_DUP_CHECK
         fra[i].dup_check_timeout      = dd[i].dup_check_timeout;
         fra[i].dup_check_flag         = dd[i].dup_check_flag;
#endif
         fra[i].dir_mode               = dd[i].dir_mode;
         fra[i].info_time              = dd[i].info_time;
         fra[i].warn_time              = dd[i].warn_time;
         fra[i].in_dc_flag             = dd[i].in_dc_flag;
         fra[i].max_errors             = dd[i].max_errors;
         fra[i].no_of_process          = 0;
         fra[i].dir_status             = NORMAL_STATUS;
         if (fra[i].no_of_time_entries > 0)
         {
            (void)memcpy(&fra[i].te[0], &dd[i].te[0],
                         (fra[i].no_of_time_entries * sizeof(struct bd_time_entry)));
            fra[i].next_check_time = calc_next_time_array(fra[i].no_of_time_entries,
                                                          &fra[i].te[0],
#ifdef WITH_TIMEZONE
                                                          fra[i].timezone,
#endif
                                                          current_time,
                                                          __FILE__, __LINE__);
         }

         /*
          * Search in the old FRA for this directory. If it is there use
          * the values from the old FRA or else initialise them with
          * defaults. When we find an old entry, remember this so we
          * can later check if there are entries in the old FRA but
          * there are no corresponding entries in the DIR_CONFIG.
          */
         gotcha = NO;
         for (k = 0; k < old_no_of_dirs; k++)
         {
            if (((old_fra[k].dir_id != 0) &&
                 (old_fra[k].dir_id == fra[i].dir_id)) ||
                ((old_fra[k].dir_id == 0) &&
                 (CHECK_STRCMP(old_fra[k].url, fra[i].url) == 0)))
            {
               gotcha = YES;
               break;
            }
         }

         if (gotcha == YES)
         {
            fra[i].last_retrieval         = old_fra[k].last_retrieval;
            fra[i].start_event_handle     = old_fra[k].start_event_handle;
            fra[i].end_event_handle       = old_fra[k].end_event_handle;
            fra[i].dir_mtime              = old_fra[k].dir_mtime;
            fra[i].bytes_received         = old_fra[k].bytes_received;
            fra[i].files_received         = old_fra[k].files_received;
            fra[i].files_in_dir           = old_fra[k].files_in_dir;
            fra[i].files_queued           = old_fra[k].files_queued;
            fra[i].bytes_in_dir           = old_fra[k].bytes_in_dir;
            fra[i].bytes_in_queue         = old_fra[k].bytes_in_queue;
            fra[i].dir_status             = old_fra[k].dir_status;
            fra[i].dir_flag               = old_fra[k].dir_flag;
            fra[i].error_counter          = old_fra[k].error_counter;
            fra[i].dir_options            = old_fra[k].dir_options;
            if (((fra[i].dir_options & ACCEPT_DOT_FILES) &&
                 (dd[i].accept_dot_files == NO)) ||
                (((fra[i].dir_options & ACCEPT_DOT_FILES) == 0) &&
                 (dd[i].accept_dot_files == YES)))
            {
               fra[i].dir_options ^= ACCEPT_DOT_FILES;
            }
            if (((fra[i].dir_options & DO_NOT_PARALLELIZE) &&
                 (dd[i].do_not_parallelize == NO)) ||
                (((fra[i].dir_options & DO_NOT_PARALLELIZE) == 0) &&
                 (dd[i].do_not_parallelize == YES)))
            {
               fra[i].dir_options ^= DO_NOT_PARALLELIZE;
            }
            if (((fra[i].dir_options & DO_NOT_MOVE) &&
                 (dd[i].do_not_move == NO)) ||
                (((fra[i].dir_options & DO_NOT_MOVE) == 0) &&
                 (dd[i].do_not_move == YES)))
            {
               fra[i].dir_options ^= DO_NOT_MOVE;
            }
            if (((fra[i].dir_options & DONT_GET_DIR_LIST) &&
                 (dd[i].do_not_get_dir_list == NO)) ||
                (((fra[i].dir_options & DONT_GET_DIR_LIST) == 0) &&
                 (dd[i].do_not_get_dir_list == YES)))
            {
               fra[i].dir_options ^= DONT_GET_DIR_LIST;
            }
            if (((fra[i].dir_options & URL_CREATES_FILE_NAME) &&
                 (dd[i].url_creates_file_name == NO)) ||
                (((fra[i].dir_options & URL_CREATES_FILE_NAME) == 0) &&
                 (dd[i].url_creates_file_name == YES)))
            {
               fra[i].dir_options ^= URL_CREATES_FILE_NAME;
            }
            if (((fra[i].dir_options & URL_WITH_INDEX_FILE_NAME) &&
                 (dd[i].url_with_index_file_name == NO)) ||
                (((fra[i].dir_options & URL_WITH_INDEX_FILE_NAME) == 0) &&
                 (dd[i].url_with_index_file_name == YES)))
            {
               fra[i].dir_options ^= URL_WITH_INDEX_FILE_NAME;
            }
            if (((fra[i].dir_options & NO_DELIMITER) &&
                 (dd[i].no_delimiter == NO)) ||
                (((fra[i].dir_options & NO_DELIMITER) == 0) &&
                 (dd[i].no_delimiter == YES)))
            {
               fra[i].dir_options ^= NO_DELIMITER;
            }
            if (((fra[i].dir_options & KEEP_PATH) &&
                 (dd[i].keep_path == NO)) ||
                (((fra[i].dir_options & KEEP_PATH) == 0) &&
                 (dd[i].keep_path == YES)))
            {
               fra[i].dir_options ^= KEEP_PATH;
            }
            if (((fra[i].dir_options & ONE_PROCESS_JUST_SCANNING) &&
                 (dd[i].one_process_just_scaning == NO)) ||
                (((fra[i].dir_options & ONE_PROCESS_JUST_SCANNING) == 0) &&
                 (dd[i].one_process_just_scaning == YES)))
            {
               fra[i].dir_options ^= ONE_PROCESS_JUST_SCANNING;
            }
            if ((dd[i].create_source_dir == NO) &&
                (fra[i].dir_mode != 0))
            {
               fra[i].dir_mode = 0;
            }
            else if ((dd[i].create_source_dir == YES) &&
                     (fra[i].dir_mode == 0))
                 {
                    fra[i].dir_mode = DIR_MODE;
                 }
            if ((fra[i].dir_flag & INFO_TIME_REACHED) &&
                ((fra[i].info_time < 1) ||
                 ((current_time - fra[i].last_retrieval) < fra[i].info_time)))
            {
               fra[i].dir_flag &= ~INFO_TIME_REACHED;
               SET_DIR_STATUS(fra[i].dir_flag,
                              current_time,
                              fra[i].start_event_handle,
                              fra[i].end_event_handle,
                              fra[i].dir_status);
               error_action(fra[i].dir_alias, "stop", DIR_INFO_ACTION, -1);
               event_log(0L, EC_DIR, ET_AUTO, EA_INFO_TIME_UNSET, "%s",
                         fra[i].dir_alias);
            }
            if ((fra[i].dir_flag & WARN_TIME_REACHED) &&
                ((fra[i].warn_time < 1) ||
                 ((current_time - fra[i].last_retrieval) < fra[i].warn_time)))
            {
               fra[i].dir_flag &= ~WARN_TIME_REACHED;
               SET_DIR_STATUS(fra[i].dir_flag,
                              current_time,
                              fra[i].start_event_handle,
                              fra[i].end_event_handle,
                              fra[i].dir_status);
               error_action(fra[i].dir_alias, "stop", DIR_WARN_ACTION, -1);
               event_log(0L, EC_DIR, ET_AUTO, EA_WARN_TIME_UNSET, "%s",
                         fra[i].dir_alias);
            }
#ifdef WITH_INOTIFY
            if ((fra[i].dir_options & INOTIFY_RENAME) &&
                ((dd[i].inotify_flag & INOTIFY_RENAME_FLAG) == 0))
            {
               fra[i].dir_options &= ~INOTIFY_RENAME;
            }
            else if (((fra[i].dir_options & INOTIFY_RENAME) == 0) &&
                     (dd[i].inotify_flag & INOTIFY_RENAME_FLAG))
                 {
                    fra[i].dir_options |= INOTIFY_RENAME;
                 }
            if ((fra[i].dir_options & INOTIFY_CLOSE) &&
                ((dd[i].inotify_flag & INOTIFY_CLOSE_FLAG) == 0))
            {
               fra[i].dir_options &= ~INOTIFY_CLOSE;
            }
            else if (((fra[i].dir_options & INOTIFY_CLOSE) == 0) &&
                     (dd[i].inotify_flag & INOTIFY_CLOSE_FLAG))
                 {
                    fra[i].dir_options |= INOTIFY_CLOSE;
                 }
            if ((fra[i].dir_options & INOTIFY_CREATE) &&
                ((dd[i].inotify_flag & INOTIFY_CREATE_FLAG) == 0))
            {
               fra[i].dir_options &= ~INOTIFY_CREATE;
            }
            else if (((fra[i].dir_options & INOTIFY_CREATE) == 0) &&
                     (dd[i].inotify_flag & INOTIFY_CREATE_FLAG))
                 {
                    fra[i].dir_options |= INOTIFY_CREATE;
                 }
            if ((fra[i].dir_options & INOTIFY_DELETE) &&
                ((dd[i].inotify_flag & INOTIFY_DELETE_FLAG) == 0))
            {
               fra[i].dir_options &= ~INOTIFY_DELETE;
            }
            else if (((fra[i].dir_options & INOTIFY_DELETE) == 0) &&
                     (dd[i].inotify_flag & INOTIFY_DELETE_FLAG))
                 {
                    fra[i].dir_options |= INOTIFY_DELETE;
                 }
            if ((fra[i].dir_options & INOTIFY_ATTRIB) &&
                ((dd[i].inotify_flag & INOTIFY_ATTRIB_FLAG) == 0))
            {
               fra[i].dir_options &= ~INOTIFY_ATTRIB;
            }
            else if (((fra[i].dir_options & INOTIFY_ATTRIB) == 0) &&
                     (dd[i].inotify_flag & INOTIFY_ATTRIB_FLAG))
                 {
                    fra[i].dir_options |= INOTIFY_ATTRIB;
                 }
#endif
            fra[i].queued                 = old_fra[k].queued;
            (void)memcpy(&fra[i].ate, &old_fra[k].ate,
                         sizeof(struct bd_time_entry));
         }
         else /* This directory is not in the old FRA, therefor it is new. */
         {
            fra[i].last_retrieval         = 0L;
            fra[i].start_event_handle     = 0L;
            fra[i].end_event_handle       = 0L;
            fra[i].dir_mtime              = 0;
            fra[i].bytes_received         = 0;
            fra[i].files_received         = 0;
            fra[i].files_in_dir           = 0;
            fra[i].files_queued           = 0;
            fra[i].bytes_in_dir           = 0;
            fra[i].bytes_in_queue         = 0;
            fra[i].dir_status             = NORMAL_STATUS;
            fra[i].error_counter          = 0;
            fra[i].dir_options            = 0;
            fra[i].dir_flag               = 0;
            if (dd[i].accept_dot_files == YES)
            {
               fra[i].dir_options |= ACCEPT_DOT_FILES;
            }
            if (dd[i].do_not_parallelize == YES)
            {
               fra[i].dir_options |= DO_NOT_PARALLELIZE;
            }
            if (dd[i].do_not_move == YES)
            {
               fra[i].dir_options |= DO_NOT_MOVE;
            }
            if (dd[i].do_not_get_dir_list == YES)
            {
               fra[i].dir_options |= DONT_GET_DIR_LIST;
            }
            if (dd[i].url_creates_file_name == YES)
            {
               fra[i].dir_options |= URL_CREATES_FILE_NAME;
            }
            if (dd[i].url_with_index_file_name == YES)
            {
               fra[i].dir_options |= URL_WITH_INDEX_FILE_NAME;
            }
            if (dd[i].no_delimiter == YES)
            {
               fra[i].dir_options |= NO_DELIMITER;
            }
            if (dd[i].keep_path == YES)
            {
               fra[i].dir_options |= KEEP_PATH;
            }
            if (dd[i].one_process_just_scaning == YES)
            {
               fra[i].dir_options |= ONE_PROCESS_JUST_SCANNING;
            }
            if (dd[i].create_source_dir == YES)
            {
               if (fra[i].dir_mode == 0)
               {
                  fra[i].dir_mode = DIR_MODE;
               }
            }
            else
            {
               fra[i].dir_mode = 0;
            }
#ifdef WITH_INOTIFY
            if (dd[i].inotify_flag & INOTIFY_RENAME_FLAG)
            {
               fra[i].dir_options |= INOTIFY_RENAME;
            }
            if (dd[i].inotify_flag & INOTIFY_CLOSE_FLAG)
            {
               fra[i].dir_options |= INOTIFY_CLOSE;
            }
            if (dd[i].inotify_flag & INOTIFY_CREATE_FLAG)
            {
               fra[i].dir_options |= INOTIFY_CREATE;
            }
            if (dd[i].inotify_flag & INOTIFY_DELETE_FLAG)
            {
               fra[i].dir_options |= INOTIFY_DELETE;
            }
            if (dd[i].inotify_flag & INOTIFY_ATTRIB_FLAG)
            {
               fra[i].dir_options |= INOTIFY_ATTRIB;
            }
#endif
            fra[i].queued                 = 0;
            (void)memset(&fra[i].ate, 0, sizeof(struct bd_time_entry));
         }
      } /* for (i = 0; i < no_of_dirs; i++) */

      /* Copy configuration information from the old FRA. */
      ptr = (char *)fra - AFD_FEATURE_FLAG_OFFSET_END;
      *ptr = *((char *)old_fra - AFD_FEATURE_FLAG_OFFSET_END);
   }

   /* Release memory of the structure dir_data. */
   free((void *)dd);
   dd = NULL;

   /* Reposition fra pointer after no_of_dirs. */
   ptr = (char *)fra;
   ptr -= AFD_WORD_OFFSET;
   *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
   *(ptr + SIZEOF_INT + 1 + 1 + 1) = CURRENT_FRA_VERSION; /* FRA version number. */
   if ((pagesize = (int)sysconf(_SC_PAGESIZE)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to determine the pagesize with sysconf() : %s",
                 strerror(errno));
   }
   *(int *)(ptr + SIZEOF_INT + 4) = pagesize;
   *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
   *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
   *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
   *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
   if (fra_size > 0)
   {
#ifdef HAVE_MMAP
      if (munmap(ptr, fra_size) == -1)
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
                    new_fra_stat, strerror(errno));
      }
   }

   /*
    * Unmap from old memory mapped region.
    */
   if (old_fra_size > -1)
   {
      ptr = (char *)old_fra;
      ptr -= AFD_WORD_OFFSET;

      /* Don't forget to unmap old FRA file. */
      if (old_fra_size > 0)
      {
#ifdef HAVE_MMAP
         if (munmap(ptr, old_fra_size) == -1)
#else
         if (munmap_emu(ptr) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to munmap() %s : %s",
                       old_fra_stat, strerror(errno));
         }
      }

      /* Remove the old FSA file if there was one. */
      if (unlink(old_fra_stat) < 0)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to unlink() %s : %s",
                    old_fra_stat, strerror(errno));
      }
   }

   /*
    * Copy the new fra_id into the locked FRA_ID_FILE file, unlock
    * and close the file.
    */
   /* Go to beginning in file. */
   if (lseek(fra_id_fd, 0, SEEK_SET) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not seek() to beginning of %s : %s",
                 fra_id_file, strerror(errno));
   }

   /* Write new value into FRA_ID_FILE file. */
   if (write(fra_id_fd, &fra_id, sizeof(int)) != sizeof(int))
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not write value to FRA ID file : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Unlock file which holds the fsa_id. */
   if (old_fra_fd != -1)
   {
      if (fcntl(fra_id_fd, F_SETLKW, &ulock) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not unset write lock : %s", strerror(errno));
      }
   }

   /* Close the FRA ID file. */
   if (close(fra_id_fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }

   /* Close file with new FRA. */
   if (close(fra_fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }
   fra_fd = -1;

   /* Close old FRA file. */
   if (old_fra_fd != -1)
   {
      if (close(old_fra_fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "close() error : %s", strerror(errno));
      }
   }

   return;
}
