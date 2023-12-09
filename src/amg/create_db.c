/*
 *  create_db.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   create_db - creates structure instant_db and initialises it
 **               for dir_check
 **
 ** SYNOPSIS
 **   int create_db(FILE *udc_reply_fp)
 **
 ** DESCRIPTION
 **   This function creates structure instant_db and initialises it
 **   with data from the shared memory area created by the AMG. See
 **   amgdefs.h for a more detailed description of structure instant_db.
 **
 ** RETURN VALUES
 **   Will exit with INCORRECT when itt encounters an error. On success
 **   it will return the number of jobs it has found in the shared
 **   memory area.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.10.1995 H.Kiehl Created
 **   02.01.1997 H.Kiehl Added logging of file names that are found by
 **                      by the AFD.
 **   01.02.1997 H.Kiehl Read renaming rule file.
 **   19.04.1997 H.Kiehl Add support for writing messages in one memory
 **                      mapped file.
 **   26.10.1997 H.Kiehl If disk is full do not give up.
 **   21.01.1998 H.Kiehl Rewrite to accommodate new messages and job ID's.
 **   14.08.2000 H.Kiehl File mask no longer a fixed array.
 **   04.01.2001 H.Kiehl Check for duplicate paused directories.
 **   05.04.2001 H.Kiehl Remove O_TRUNC.
 **   25.04.2001 H.Kiehl Rename files from pool directory if there is only
 **                      one job for this directory.
 **   16.05.2002 H.Kiehl Removed shared memory stuff.
 **   30.10.2002 H.Kiehl Take afd_file_dir as the directory to determine
 **                      if we can just move a file.
 **   28.02.2003 H.Kiehl Added option grib2wmo.
 **   13.07.2003 H.Kiehl Option "age-limit" is now used by AMG as well.
 **   24.06.2006 H.Kiehl Allow for duplicate directories when the
 **                      directories are in different DIR_CONFIGS.
 **   11.05.2017 H.Kiehl Use update_db_log() when printing errrors so
 **                      one sees them with udc.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* fprintf(), snprintf()              */
#include <string.h>                /* strcpy(), strcat(), strcmp(),      */
                                   /* memcpy(), strerror(), strlen()     */
#include <stdlib.h>                /* atoi(), calloc(), malloc(), free(),*/
                                   /* exit()                             */
#include <ctype.h>                 /* isdigit()                          */
#include <time.h>                  /* time()                             */
#include <sys/types.h>
#ifdef HAVE_STATX
# include <sys/sysmacros.h>        /* makedev()                          */
#endif
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>             /* msync(), munmap()                  */
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <unistd.h>                /* fork(), rmdir(), fstat()           */
#include <errno.h>
#include "amgdefs.h"
#include "version.h"

/* External global variables. */
extern int                        fsa_fd,
                                  no_fork_jobs,
                                  no_of_hosts,
                                  no_of_local_dirs,
                                  no_of_time_jobs,
#ifdef MULTI_FS_SUPPORT
                                  no_of_extra_work_dirs,
#else
                                  outgoing_file_dir_length,
#endif
                                  *time_job_list;
extern unsigned int               default_age_limit;
#ifdef _DISTRIBUTION_LOG
extern unsigned int               max_jobs_per_file;
#endif
extern off_t                      amg_data_size;
extern char                       *afd_file_dir,
#ifndef MULTI_FS_SUPPORT
                                  outgoing_file_dir[],
                                  time_dir[],
#endif
                                  *p_mmap,
                                  *p_work_dir;
#ifdef _WITH_PTHREAD
extern struct data_t              *p_data;
#else
extern unsigned int               max_file_buffer;
#endif
#ifdef MULTI_FS_SUPPORT
extern struct extra_work_dirs     *ewl;
#endif
extern struct fork_job_data       *fjd;
extern struct directory_entry     *de;
extern struct instant_db          *db;
extern struct afd_status          *p_afd_status;
extern struct filetransfer_status *fsa;
extern struct fileretrieve_status *fra;
#ifdef _DISTRIBUTION_LOG
extern struct file_dist_list      **file_dist_pool;
#endif

/* Global variables. */
int                               dnb_fd,
                                  fmd_fd = -1, /* File Mask Database fd. */
                                  jd_fd,
                                  *no_of_passwd,
                                  pwb_fd,
                                  *no_of_dir_names,
                                  *no_of_job_ids;
off_t                             fmd_size = 0;
char                              *fmd,
                                  *fmd_end,
#ifdef WITH_GOTCHA_LIST
                                  *gotcha = NULL,
#endif
                                  msg_dir[MAX_PATH_LENGTH],
                                  *p_msg_dir;
struct job_id_data                *jd;
struct dir_name_buf               *dnb;
struct passwd_buf                 *pwb;

/* Local function prototypes. */
#ifdef MULTI_FS_SUPPORT
static int                        check_extra_filesystem(dev_t, int);
#endif
static void                       send_busy_working(int),
                                  write_current_job_list(int);

/* #define _WITH_JOB_LIST_TEST 1 */
#define POS_STEP_SIZE      20
#define FORK_JOB_STEP_SIZE 20


/*+++++++++++++++++++++++++++++ create_db() +++++++++++++++++++++++++++++*/
int
create_db(FILE *udc_reply_fp, int write_fd)
{
   int            amg_data_fd,
                  exec_flag,
                  exec_flag_dir,
                  i,
                  j,
#if defined (_TEST_FILE_TABLE) || defined (_DISTRIBUTION_LOG)
                  k,
#endif
                  not_in_same_file_system = 0,
                  one_job_only_dir = 0,
                  show_one_job_no_link,
                  size,
                  dir_counter = 0,
                  no_of_jobs;
   unsigned int   jid_number,
                  scheme;
#ifdef WITH_ERROR_QUEUE
   int            no_of_cids;
   unsigned int   *cml;
#endif
#ifdef _DISTRIBUTION_LOG
   unsigned int   max_jobs_per_dir;
#endif
   dev_t          current_dir_dev,
                  ldv;               /* Local device number (filesystem). */
   time_t         start_time;
   char           amg_data_file[MAX_PATH_LENGTH],
                  *ptr,
                  *tmp_ptr,
                  *p_offset,
                  *p_loptions,
                  *p_file;
   struct p_array *p_ptr;
#ifdef HAVE_STATX
   struct statx   stat_buf;
#else
   struct stat    stat_buf;
#endif

   /* Check if we need to free some data. */
   if (fjd != NULL)
   {
      free(fjd);
      fjd = NULL;
      no_fork_jobs = 0;
   }
   if (db != NULL)
   {
      if (p_mmap != NULL)
      {
         ptr = p_mmap;
         no_of_jobs = *(int *)ptr;
      }
      else
      {
         no_of_jobs = 0;
      }
      for (i = 0; i < no_of_jobs; i++)
      {
         if (db[i].te != NULL)
         {
            free(db[i].te);
            db[i].te = NULL;
         }
      }
      free(db);
      db = NULL;

      /* Lets just assume that when db was allocated that the data */
      /* in struct directory_entry is also still allocated.        */
#ifdef WITH_ONETIME
      for (i = 0; i < (no_of_local_dirs + MAX_NO_OF_ONETIME_DIRS); i++)
#else
      for (i = 0; i < no_of_local_dirs; i++)
#endif
      {
         for (j = 0; j < de[i].nfg; j++)
         {
            free(de[i].fme[j].pos);
            de[i].fme[j].pos = NULL;
            free(de[i].fme[j].file_mask);
            de[i].fme[j].file_mask = NULL;
         }
         free(de[i].fme);
         de[i].fme = NULL;
         de[i].nfg = 0;
         if (de[i].paused_dir != NULL)
         {
            free(de[i].paused_dir);
            de[i].paused_dir = NULL;
         }
         if (de[i].rl_fd != -1)
         {
            if (close(de[i].rl_fd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Failed to close() retrieve list file for directory ID %x: %s",
                          de[i].dir_id, strerror(errno));
            }
            de[i].rl_fd = -1;
         }
         if (de[i].rl != NULL)
         {
            ptr = (char *)de[i].rl - AFD_WORD_OFFSET;
            if (munmap(ptr, de[i].rl_size) == -1)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to munmap() from retrieve list file for directory ID %x: %s",
                          de[i].dir_id, strerror(errno));
            }
            de[i].rl = NULL;
         }
      }
   }
   if (p_mmap != NULL)
   {
#ifdef HAVE_MMAP
      if (munmap(p_mmap, amg_data_size) == -1)
#else
      if (munmap_emu((void *)p_mmap) == -1)
#endif
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to munmap() from %s : %s",
                    AMG_DATA_FILE, strerror(errno));
      }
      p_mmap = NULL;

      show_one_job_no_link = NO;
   }
   else
   {
      show_one_job_no_link = YES;
   }

   /* Set flag to indicate that we are writting in JID structure. */
   if ((p_afd_status->amg_jobs & WRITTING_JID_STRUCT) == 0)
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
   }

   /* Get device number for working directory. */
#ifdef HAVE_STATX
   if (statx(0, afd_file_dir, AT_STATX_SYNC_AS_STAT, 0, &stat_buf) == -1)
#else
   if (stat(afd_file_dir, &stat_buf) == -1)
#endif
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
      p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
      system_log(FATAL_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                 "Failed to statx() `%s' : %s",
#else
                 "Failed to stat() `%s' : %s",
#endif
                 afd_file_dir, strerror(errno));
      exit(INCORRECT);
   }
#ifdef HAVE_STATX
   ldv = makedev(stat_buf.stx_dev_major, stat_buf.stx_dev_minor);
#else
   ldv = stat_buf.st_dev;
#endif

   (void)snprintf(amg_data_file, MAX_PATH_LENGTH,
                  "%s%s%s", p_work_dir, FIFO_DIR, AMG_DATA_FILE);
   if ((amg_data_fd = open(amg_data_file, O_RDWR)) == -1)
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
      p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to open() `%s' : %s", amg_data_file, strerror(errno));
      exit(INCORRECT);
   }
#ifdef HAVE_STATX
   if (statx(amg_data_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(amg_data_fd, &stat_buf) == -1)
#endif
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
      p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
      system_log(FATAL_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                 "Failed to statx() `%s' : %s",
#else
                 "Failed to fstat() `%s' : %s",
#endif
                 amg_data_file, strerror(errno));
      exit(INCORRECT);
   }
#ifdef HAVE_STATX
   amg_data_size = stat_buf.stx_size;
#else
   amg_data_size = stat_buf.st_size;
#endif
#ifdef HAVE_MMAP
   if ((p_mmap = mmap(NULL, amg_data_size, (PROT_READ | PROT_WRITE),
                      MAP_SHARED, amg_data_fd, 0)) == (caddr_t) -1)
#else
   if ((p_mmap = mmap_emu(NULL, amg_data_size, (PROT_READ | PROT_WRITE),
                          MAP_SHARED, amg_data_file, 0)) == (caddr_t) -1)
#endif
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
      p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to mmap() `%s' : %s", amg_data_file, strerror(errno));
      exit(INCORRECT);
   }
   if (close(amg_data_fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }
   ptr = p_mmap;

   /* First get the no of jobs. */
   no_of_jobs = *(int *)ptr;
   ptr += sizeof(int);

   /* Allocate some memory to store instant database. */
   if ((db = malloc(no_of_jobs * sizeof(struct instant_db))) == NULL)
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
      p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      exit(INCORRECT);
   }

   init_job_data();

#ifdef WITH_GOTCHA_LIST
   /* Allocate space for the gotchas. */
   size = ((*no_of_job_ids / JOB_ID_DATA_STEP_SIZE) + 1) *
           JOB_ID_DATA_STEP_SIZE * sizeof(char);
   if ((gotcha = malloc(size)) == NULL)
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
      p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() memory : %s", strerror(errno));
      exit(INCORRECT);
   }
# ifdef _WITH_JOB_LIST_TEST
   {
      int changed = 0;

      for (i = 0; i < size; i++)
      {
         if (changed < 7)
         {
            gotcha[i] = YES;
            changed = 0;
         }
         else
         {
            gotcha[i] = NO;
         }
         changed++;
      }
   }
# else
   (void)memset(gotcha, NO, size);
# endif
#endif

   if (no_of_time_jobs > 0)
   {
      no_of_time_jobs = 0;
      free(time_job_list);
      time_job_list = NULL;
   }
#ifdef _DISTRIBUTION_LOG
   max_jobs_per_file = 0;
   max_jobs_per_dir = 0;
#endif

   /* Create and copy pointer array. */
   size = no_of_jobs * sizeof(struct p_array);
   if ((tmp_ptr = calloc(no_of_jobs, sizeof(struct p_array))) == NULL)
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
      p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not allocate memory : %s", strerror(errno));
      exit(INCORRECT);
   }
   p_ptr = (struct p_array *)tmp_ptr;
   memcpy(p_ptr, ptr, size);
   p_offset = ptr + size;

   de[0].nfg         = 0;
   de[0].fme         = NULL;
   de[0].flag        = 0;
   de[0].dir         = p_ptr[0].ptr[DIRECTORY_PTR_POS] + p_offset;
   de[0].alias       = p_ptr[0].ptr[ALIAS_NAME_PTR_POS] + p_offset;
   if ((de[0].fra_pos = lookup_fra_pos(de[0].alias)) == INCORRECT)
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
      p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to locate dir alias `%s' for directory `%s'",
                 de[0].alias, de[0].dir);
      exit(INCORRECT);
   }
   de[0].dir_id      = fra[de[0].fra_pos].dir_id;
   de[0].search_time = 0;
   if ((fra[de[0].fra_pos].fsa_pos != -1) &&
       (fra[de[0].fra_pos].fsa_pos < no_of_hosts))
   {
      size_t length;

      length = strlen(de[0].dir) + 1 + 1 +
               strlen(fsa[fra[de[0].fra_pos].fsa_pos].host_alias) + 1;
      if ((de[0].paused_dir = malloc(length)) == NULL)
      {
         p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
         p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to malloc() %d bytes : %s",
                    length, strerror(errno));
         exit(INCORRECT);
      }
      (void)snprintf(de[0].paused_dir, length, "%s/.%s", de[0].dir,
                     fsa[fra[de[0].fra_pos].fsa_pos].host_alias);
   }
   else
   {
      de[0].paused_dir = NULL;
   }
#ifdef HAVE_STATX
   if (statx(0, de[0].dir, AT_STATX_SYNC_AS_STAT, 0, &stat_buf) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__, "Failed to statx() `%s' : %s",
                 de[0].dir, strerror(errno));
      current_dir_dev = ldv + 1;
   }
   else
   {
      current_dir_dev = makedev(stat_buf.stx_dev_major, stat_buf.stx_dev_minor);
   }
#else
   if (stat(de[0].dir, &stat_buf) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__, "Failed to stat() `%s' : %s",
                 de[0].dir, strerror(errno));
      current_dir_dev = ldv + 1;
   }
   else
   {
      current_dir_dev = stat_buf.st_dev;
   }
#endif
#ifdef MULTI_FS_SUPPORT
   de[0].dev = current_dir_dev;
#endif
   if (current_dir_dev != ldv)
   {
#ifdef MULTI_FS_SUPPORT
      if (check_extra_filesystem(de[0].dev, 0) == YES)
      {
         de[0].flag |= IN_SAME_FILESYSTEM;
      }
      else
      {
#endif
#ifdef _MAINTAINER_LOG
         maintainer_log(INFO_SIGN, NULL, 0,
                        "%s not in same filesystem", db[0].dir);
#endif
         not_in_same_file_system++;
#ifdef MULTI_FS_SUPPORT
      }
#endif
   }
   else
   {
      de[0].flag |= IN_SAME_FILESYSTEM;
#ifdef MULTI_FS_SUPPORT
      de[0].ewl_pos = 0;
#endif
   }
   if (((no_of_jobs - 1) == 0) ||
       (de[0].dir != (p_ptr[1].ptr[DIRECTORY_PTR_POS] + p_offset)))
   {
      de[0].flag |= RENAME_ONE_JOB_ONLY;
      one_job_only_dir++;
   }

   start_time = time(NULL);
   exec_flag_dir = 0;
   for (i = 0; i < no_of_jobs; i++)
   {
#ifdef _DISTRIBUTION_LOG
      max_jobs_per_dir += 1;
#endif
      exec_flag = 0;

      /* Store DIR_CONFIG ID. */
      db[i].dir_config_id = (unsigned int)strtoul(p_ptr[i].ptr[DIR_CONFIG_ID_PTR_POS] + p_offset, NULL, 16);

      /* Store directory pointer. */
      db[i].dir = p_ptr[i].ptr[DIRECTORY_PTR_POS] + p_offset;

      /* Store priority. */
      db[i].priority = *(p_ptr[i].ptr[PRIORITY_PTR_POS] + p_offset);

      /* Store number of files to be send. */
      db[i].no_of_files = atoi(p_ptr[i].ptr[NO_OF_FILES_PTR_POS] + p_offset);

      /* Store pointer to first file (filter). */
      db[i].files = p_ptr[i].ptr[FILE_PTR_POS] + p_offset;

      /*
       * Store all file names of one directory into one array. This
       * is necessary so we can specify overlapping wild cards in
       * different file sections for one directory section.
       */
      if ((i > 0) && (db[i].dir != db[i - 1].dir))
      {
         if ((de[dir_counter].flag & DELETE_ALL_FILES) ||
             ((de[dir_counter].flag & IN_SAME_FILESYSTEM) &&
              (exec_flag_dir == 0)))
         {
            if ((fra[de[dir_counter].fra_pos].dir_flag & LINK_NO_EXEC) == 0)
            {
               fra[de[dir_counter].fra_pos].dir_flag |= LINK_NO_EXEC;
            }
         }
         else
         {
            if (fra[de[dir_counter].fra_pos].dir_flag & LINK_NO_EXEC)
            {
               fra[de[dir_counter].fra_pos].dir_flag ^= LINK_NO_EXEC;
            }
         }
#ifdef _DISTRIBUTION_LOG
         if (max_jobs_per_dir > max_jobs_per_file)
         {
            max_jobs_per_file = max_jobs_per_dir;
         }
         max_jobs_per_dir = 0;
#endif
         exec_flag_dir = 0;
         dir_counter++;
         if (dir_counter >= no_of_local_dirs)
         {
            p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
            p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Aaarghhh, dir_counter (%d) >= no_of_local_dirs (%d)!?",
                       dir_counter, no_of_local_dirs);
            exit(INCORRECT);
         }
         de[dir_counter].dir           = db[i].dir;
         de[dir_counter].alias         = p_ptr[i].ptr[ALIAS_NAME_PTR_POS] + p_offset;
         if ((de[dir_counter].fra_pos = lookup_fra_pos(de[dir_counter].alias)) == INCORRECT)
         {
            p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
            p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Failed to locate dir alias `%s' for directory `%s'",
                       de[dir_counter].alias, de[dir_counter].dir);
            exit(INCORRECT);
         }
         de[dir_counter].dir_id        = fra[de[dir_counter].fra_pos].dir_id;
         de[dir_counter].nfg           = 0;
         de[dir_counter].fme           = NULL;
         de[dir_counter].flag          = 0;
         de[dir_counter].search_time   = 0;
         if ((fra[de[dir_counter].fra_pos].fsa_pos != -1) &&
             (fra[de[dir_counter].fra_pos].fsa_pos < no_of_hosts))
         {
            size_t length;

            length = strlen(de[dir_counter].dir) + 1 + 1 +
                     strlen(fsa[fra[de[dir_counter].fra_pos].fsa_pos].host_alias) + 1;
            if ((de[dir_counter].paused_dir = malloc(length)) == NULL)
            {
               p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
               p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "Failed to malloc() %d bytes : %s",
                          length, strerror(errno));
               exit(INCORRECT);
            }
            (void)snprintf(de[dir_counter].paused_dir, length, "%s/.%s",
                           de[dir_counter].dir,
                           fsa[fra[de[dir_counter].fra_pos].fsa_pos].host_alias);
         }
         else
         {
            de[dir_counter].paused_dir = NULL;
         }
#ifdef HAVE_STATX
         if (statx(0, db[i].dir, AT_STATX_SYNC_AS_STAT, 0, &stat_buf) < 0)
#else
         if (stat(db[i].dir, &stat_buf) < 0)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                       "Failed to statx() `%s' (i=%d) : %s",
#else
                       "Failed to stat() `%s' (i=%d) : %s",
#endif
                       db[i].dir, i, strerror(errno));
            current_dir_dev = ldv + 1;
         }
         else
         {
#ifdef HAVE_STATX
            current_dir_dev = makedev(stat_buf.stx_dev_major, stat_buf.stx_dev_minor);
#else
            current_dir_dev = stat_buf.st_dev;
#endif
         }
#ifdef MULTI_FS_SUPPORT
         de[dir_counter].dev = current_dir_dev;
#endif
         if (current_dir_dev != ldv)
         {
#ifdef MULTI_FS_SUPPORT
            if (check_extra_filesystem(current_dir_dev, dir_counter) == YES)
            {
               de[dir_counter].flag |= IN_SAME_FILESYSTEM;
            }
            else
            {
#endif
#ifdef _MAINTAINER_LOG
               maintainer_log(INFO_SIGN, NULL, 0,
                              "%s not in same filesystem", de[dir_counter].dir);
#endif
               not_in_same_file_system++;
#ifdef MULTI_FS_SUPPORT
            }
#endif
         }
         else
         {
            de[dir_counter].flag |= IN_SAME_FILESYSTEM;
#ifdef MULTI_FS_SUPPORT
            de[dir_counter].ewl_pos = 0;
#endif
         }
         if ((i == (no_of_jobs - 1)) ||
             (db[i].dir != (p_ptr[i + 1].ptr[DIRECTORY_PTR_POS] + p_offset)))
         {
            de[dir_counter].flag |= RENAME_ONE_JOB_ONLY;
            one_job_only_dir++;
         }
      }
      db[i].fra_pos = de[dir_counter].fra_pos;
      db[i].dir_id = de[dir_counter].dir_id;
#ifdef MULTI_FS_SUPPORT
      db[i].ewl_pos = de[dir_counter].ewl_pos;
#endif

      /*
       * Check if this directory is in the same file system
       * as file directory of the AFD. If this is not the case
       * lets fork when we copy.
       */
      db[i].lfs = 0;
#ifdef MULTI_FS_SUPPORT
      if (de[dir_counter].flag & IN_SAME_FILESYSTEM)
      {
         db[i].lfs |= IN_SAME_FILESYSTEM;
      }
#else
      if (current_dir_dev == ldv)
      {
         db[i].lfs |= IN_SAME_FILESYSTEM;
      }
#endif

      if ((i == 0) || ((i > 0) && (db[i].files != db[i - 1].files)))
      {
         if ((de[dir_counter].nfg % FG_BUFFER_STEP_SIZE) == 0)
         {
            char   *ptr_start;
            size_t new_size;

            /* Calculate new size of file group buffer. */
             new_size = ((de[dir_counter].nfg / FG_BUFFER_STEP_SIZE) + 1) *
                        FG_BUFFER_STEP_SIZE * sizeof(struct file_mask_entry);

            if ((de[dir_counter].fme = realloc(de[dir_counter].fme, new_size)) == NULL)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "Failed to realloc() %d bytes : %s",
                          new_size, strerror(errno));
               p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
               p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
               unmap_data(jd_fd, (void *)&jd);
               exit(INCORRECT);
            }

            /* Initialise new memory area. */
            if (de[dir_counter].nfg > (FG_BUFFER_STEP_SIZE - 1))
            {
               ptr_start = (char *)(de[dir_counter].fme) +
                           (de[dir_counter].nfg * sizeof(struct file_mask_entry));
            }
            else
            {
               ptr_start = (char *)de[dir_counter].fme;
            }
            (void)memset(ptr_start, 0,
                         (FG_BUFFER_STEP_SIZE * sizeof(struct file_mask_entry)));
         }
         p_file = db[i].files;
         de[dir_counter].fme[de[dir_counter].nfg].nfm = db[i].no_of_files;
         if ((de[dir_counter].fme[de[dir_counter].nfg].file_mask = malloc((db[i].no_of_files * sizeof(char *)))) == NULL)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Failed to malloc() %d bytes : %s",
                       db[i].no_of_files * sizeof(char *), strerror(errno));
            p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
            p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
            unmap_data(jd_fd, (void *)&jd);
            exit(INCORRECT);
         }
         de[dir_counter].fme[de[dir_counter].nfg].dest_count = 0;
         for (j = 0; j < db[i].no_of_files; j++)
         {
            de[dir_counter].fme[de[dir_counter].nfg].file_mask[j] = p_file;
            if ((p_file[0] == '*') && (p_file[1] == '\0'))
            {
               de[dir_counter].flag |= ALL_FILES;
            }
            NEXT(p_file);
         }
         db[i].fbl = p_file - db[i].files;
         lookup_file_mask_id(&db[i], db[i].fbl);
         if ((de[dir_counter].flag & ALL_FILES) && (db[i].no_of_files > 1))
         {
            p_file = db[i].files;
            for (j = 0; j < db[i].no_of_files; j++)
            {
               if (p_file[0] == '!')
               {
                  de[dir_counter].flag ^= ALL_FILES;
                  break;
               }
               NEXT(p_file);
            }
         }
         if ((de[dir_counter].fme[de[dir_counter].nfg].dest_count % POS_STEP_SIZE) == 0)
         {
            size_t new_size;

            /* Calculate new size of file group buffer. */
             new_size = ((de[dir_counter].fme[de[dir_counter].nfg].dest_count / POS_STEP_SIZE) + 1) *
                        POS_STEP_SIZE * sizeof(int);

            if ((de[dir_counter].fme[de[dir_counter].nfg].pos = realloc(de[dir_counter].fme[de[dir_counter].nfg].pos, new_size)) == NULL)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "Failed to realloc() %d bytes : %s",
                          new_size, strerror(errno));
               p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
               p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
               unmap_data(jd_fd, (void *)&jd);
               exit(INCORRECT);
            }
         }
         de[dir_counter].fme[de[dir_counter].nfg].pos[de[dir_counter].fme[de[dir_counter].nfg].dest_count] = i;
         de[dir_counter].fme[de[dir_counter].nfg].dest_count++;
         de[dir_counter].nfg++;
#ifdef _DISTRIBUTION_LOG
         if (de[dir_counter].nfg > max_jobs_per_file)
         {
            max_jobs_per_file = de[dir_counter].nfg;
         }
#endif
      }
      else if ((i > 0) && (db[i].files == db[i - 1].files))
           {
              if ((de[dir_counter].fme[de[dir_counter].nfg - 1].dest_count % POS_STEP_SIZE) == 0)
              {
                 size_t new_size;

                 /* Calculate new size of file group buffer. */
                  new_size = ((de[dir_counter].fme[de[dir_counter].nfg - 1].dest_count / POS_STEP_SIZE) + 1) *
                             POS_STEP_SIZE * sizeof(int);

                 if ((de[dir_counter].fme[de[dir_counter].nfg - 1].pos = realloc(de[dir_counter].fme[de[dir_counter].nfg - 1].pos, new_size)) == NULL)
                 {
                    system_log(FATAL_SIGN, __FILE__, __LINE__,
                               "Failed to realloc() %d bytes : %s",
                               new_size, strerror(errno));
                    p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
                    p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
                    unmap_data(jd_fd, (void *)&jd);
                    exit(INCORRECT);
                 }
              }
              de[dir_counter].fme[de[dir_counter].nfg - 1].pos[de[dir_counter].fme[de[dir_counter].nfg - 1].dest_count] = i;
              de[dir_counter].fme[de[dir_counter].nfg - 1].dest_count++;
              db[i].file_mask_id = db[i - 1].file_mask_id;
              db[i].fbl = db[i - 1].fbl;
           }

      /* Store number of local options. */
      db[i].no_of_loptions = atoi(p_ptr[i].ptr[NO_LOCAL_OPTIONS_PTR_POS] + p_offset);
      db[i].next_start_time = 0;
      db[i].time_option_type = NO_TIME;
      db[i].no_of_time_entries = 0;
      db[i].te = NULL;

      /* Store pointer to first local option. */
      if (db[i].no_of_loptions > 0)
      {
         db[i].loptions = p_ptr[i].ptr[LOCAL_OPTIONS_PTR_POS] + p_offset;
         db[i].loptions_flag = (unsigned int)strtoul(p_ptr[i].ptr[LOCAL_OPTIONS_FLAG_PTR_POS] + p_offset, NULL, 16);

         /*
          * Because some options (such as exec, extracting bulletins, etc.)
          * can take a while, it is better to fork such jobs. We can do this
          * by setting the lfs flag to GO_PARALLEL.
          */
         p_loptions = db[i].loptions;
         db[i].timezone[0] = '\0';
         for (j = 0; j < db[i].no_of_loptions; j++)
         {
            if (db[i].loptions_flag & DELETE_ID_FLAG)
            {
               db[i].lfs = DELETE_ALL_FILES;
               break;
            }
            if ((db[i].loptions_flag & EXEC_ID_FLAG) &&
                (CHECK_STRNCMP(p_loptions, EXEC_ID, EXEC_ID_LENGTH) == 0))
            {
               db[i].lfs |= GO_PARALLEL;
               db[i].lfs |= DO_NOT_LINK_FILES;
               exec_flag = 1;
               exec_flag_dir = 1;
            }
#ifdef WITH_TIMEZONE
            else if ((db[i].loptions_flag & TIMEZONE_ID_FLAG) &&
                     (CHECK_STRNCMP(p_loptions, TIMEZONE_ID, TIMEZONE_ID_LENGTH) == 0))
                 {
                    int  length = 0;
                    char *ptr = p_loptions + TIMEZONE_ID_LENGTH;

                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }
                    while ((length < MAX_TIMEZONE_LENGTH) && (*ptr != '\n') &&
                           (*ptr != '\0'))
                    {
                       db[i].timezone[length] = *ptr;
                       ptr++; length++;
                    }
                    if ((length > 0) && (length != MAX_TIMEZONE_LENGTH))
                    {
                       db[i].timezone[length] = '\0';
                    }
                    else
                    {
                       db[i].timezone[0] = '\0';
                       update_db_log(WARN_SIGN, __FILE__, __LINE__, udc_reply_fp, NULL,
                                     "Unable to store the timezone `%s', since we can only store %d bytes. Please contact maintainer (%s) if this is a valid timezone.",
                                     p_loptions + TIMEZONE_ID_LENGTH + 1,
                                     MAX_TIMEZONE_LENGTH, AFD_MAINTAINER);
                    }
                 }
#endif
            else if ((db[i].loptions_flag & TIME_NO_COLLECT_ID_FLAG) &&
                     (CHECK_STRNCMP(p_loptions, TIME_NO_COLLECT_ID,
                                    TIME_NO_COLLECT_ID_LENGTH) == 0))
                 {
                    char                 *ptr = p_loptions + TIME_NO_COLLECT_ID_LENGTH;
                    struct bd_time_entry te;

                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }
                    if (eval_time_str(ptr, &te, udc_reply_fp) == SUCCESS)
                    {
                       int                  new_size;
                       struct bd_time_entry *tmp_te;

                       new_size = (db[i].no_of_time_entries + 1) *
                                  sizeof(struct bd_time_entry);
                       if ((tmp_te = realloc(db[i].te, new_size)) == NULL)
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "Failed to realloc() %d bytes : %s",
                                     new_size, strerror(errno));
                       }
                       else
                       {
                          db[i].te = tmp_te;
                          db[i].time_option_type = SEND_NO_COLLECT_TIME;
                          (void)memcpy(&db[i].te[db[i].no_of_time_entries],
                                       &te, sizeof(struct bd_time_entry));
                          db[i].no_of_time_entries++;
                       }
                    }
                    else
                    {
                       update_db_log(ERROR_SIGN, __FILE__, __LINE__,
                                     udc_reply_fp, NULL, "%s", ptr);
                    }
                 }
            else if ((db[i].loptions_flag & TIME_ID_FLAG) &&
                     (CHECK_STRNCMP(p_loptions, TIME_ID, TIME_ID_LENGTH) == 0))
                 {
                    char                 *ptr = p_loptions + TIME_ID_LENGTH;
                    struct bd_time_entry te;

                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }
                    if (eval_time_str(ptr, &te, udc_reply_fp) == SUCCESS)
                    {
                       int                  new_size;
                       struct bd_time_entry *tmp_te;

                       new_size = (db[i].no_of_time_entries + 1) *
                                  sizeof(struct bd_time_entry);
                       if ((tmp_te = realloc(db[i].te, new_size)) == NULL)
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "Failed to realloc() %d bytes : %s",
                                     new_size, strerror(errno));
                       }
                       else
                       {
                          db[i].te = tmp_te;
                          (void)memcpy(&db[i].te[db[i].no_of_time_entries],
                                       &te, sizeof(struct bd_time_entry));
                          db[i].no_of_time_entries++;
                          db[i].time_option_type = SEND_COLLECT_TIME;
                       }
                    }
                    else
                    {
                       update_db_log(ERROR_SIGN, __FILE__, __LINE__,
                                     udc_reply_fp, NULL, "%s", ptr);
                    }
                 }
            else if ((db[i].loptions_flag & CONVERT_ID_FLAG) &&
                     (CHECK_STRNCMP(p_loptions, CONVERT_ID,
                                    CONVERT_ID_LENGTH) == 0))
                 {
                    db[i].lfs |= GO_PARALLEL;
                    db[i].lfs |= DO_NOT_LINK_FILES;
                 }
            else if ((db[i].loptions_flag & GTS2TIFF_ID_FLAG) &&
                     (CHECK_STRNCMP(p_loptions, GTS2TIFF_ID,
                                    GTS2TIFF_ID_LENGTH) == 0))
                 {
                    db[i].lfs |= GO_PARALLEL;
                 }
            else if ((db[i].loptions_flag & GRIB2WMO_ID_FLAG) &&
                     (CHECK_STRNCMP(p_loptions, GRIB2WMO_ID,
                                    GRIB2WMO_ID_LENGTH) == 0))
                 {
                    db[i].lfs |= GO_PARALLEL;
                 }
#ifdef _WITH_AFW2WMO
            else if ((db[i].loptions_flag & AFW2WMO_ID_FLAG) &&
                     (CHECK_STRNCMP(p_loptions, AFW2WMO_ID,
                                    AFW2WMO_ID_LENGTH) == 0))
                 {
                    db[i].lfs |= DO_NOT_LINK_FILES;
                 }
#endif
            else if ((db[i].loptions_flag & EXTRACT_ID_FLAG) &&
                     (CHECK_STRNCMP(p_loptions, EXTRACT_ID,
                                    EXTRACT_ID_LENGTH) == 0))
                 {
                    db[i].lfs |= GO_PARALLEL;
                    db[i].lfs |= SPLIT_FILE_LIST;
                 }
            NEXT(p_loptions);
         }
         if ((db[i].no_of_time_entries > 0) &&
             (db[i].time_option_type == SEND_COLLECT_TIME))
         {
            db[i].next_start_time = calc_next_time_array(db[i].no_of_time_entries,
                                                         db[i].te,
#ifdef WITH_TIMEZONE
                                                         db[i].timezone,
#endif
                                                         time(NULL),
                                                         __FILE__, __LINE__);
         }
      }
      else
      {
         db[i].loptions_flag = 0;
         db[i].loptions = NULL;
      }

      /*
       * If we have RENAME_ONE_JOB_ONLY and there are options that force
       * us to link the file, we cannot just rename the files! We must
       * copy them. Thus we must remove the flag when this is the case.
       */
      if ((i == 0) || ((i > 0) && (db[i].files != db[i - 1].files)))
      {
         if ((de[dir_counter].flag & RENAME_ONE_JOB_ONLY) &&
             (db[i].lfs & DO_NOT_LINK_FILES))
         {
            one_job_only_dir--;
            de[dir_counter].flag &= ~RENAME_ONE_JOB_ONLY;
         }
      }

      /* Store number of standard options. */
      db[i].no_of_soptions = atoi(p_ptr[i].ptr[NO_STD_OPTIONS_PTR_POS] + p_offset);

      /* Store pointer to first local option and age limit. */
      if (db[i].no_of_soptions > 0)
      {
         char *sptr;

         db[i].soptions = p_ptr[i].ptr[STD_OPTIONS_PTR_POS] + p_offset;
         if ((sptr = lposi(db[i].soptions, AGE_LIMIT_ID,
                           AGE_LIMIT_ID_LENGTH)) != NULL)
         {
            int  count = 0;
            char str_number[MAX_INT_LENGTH];

            while ((*sptr == ' ') || (*sptr == '\t'))
            {
               sptr++;
            }
            while ((*sptr != '\n') && (*sptr != '\0'))
            {
               str_number[count] = *sptr;
               count++; sptr++;
            }
            str_number[count] = '\0';
            errno = 0;
            db[i].age_limit = (unsigned int)strtoul(str_number,
                                                    (char **)NULL, 10);
            if (errno == ERANGE)
            {
               update_db_log(WARN_SIGN, __FILE__, __LINE__, udc_reply_fp, NULL,
                             "Option %s for directory `%s' out of range, resetting to default %u.",
                             AGE_LIMIT_ID, db[i].dir, default_age_limit);
               db[i].age_limit = default_age_limit;
            }
         }
         else
         {
            db[i].age_limit = default_age_limit;
         }
      }
      else
      {
         db[i].age_limit = default_age_limit;
         db[i].soptions = NULL;
      }

      /* Store recipient part. */
      db[i].recipient = p_ptr[i].ptr[RECIPIENT_PTR_POS] + p_offset;
      db[i].recipient_id = get_str_checksum(db[i].recipient);
      scheme = (unsigned int)strtoul((p_ptr[i].ptr[SCHEME_PTR_POS] + p_offset),
                                     NULL, 10);
      db[i].host_alias = p_ptr[i].ptr[HOST_ALIAS_PTR_POS] + p_offset;

      if ((db[i].position = get_host_position(fsa, db[i].host_alias,
                                              no_of_hosts)) < 0)
      {
         /* This should be impossible !(?) */
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Could not locate host `%s' in FSA.", db[i].host_alias);
         db[i].host_id = get_str_checksum(db[i].host_alias);
      }
      else
      {
         db[i].host_id = fsa[db[i].position].host_id;
      }

      /*
       * Always check if this directory is not already specified.
       * This might help to reduce the number of directories that the
       * function check_paused_dir() has to check.
       */
      db[i].dup_paused_dir = NO;
      for (j = 0; j < i; j++)
      {
         if ((db[j].dir == db[i].dir) && (db[j].host_id == db[i].host_id))
         {
            db[i].dup_paused_dir = YES;
            break;
         }
      }
      (void)strcpy(db[i].paused_dir, db[i].dir);
      (void)strcat(db[i].paused_dir, "/.");
      (void)strcat(db[i].paused_dir, db[i].host_alias);

      /* Now lets determine what kind of protocol we have here. */
      if (scheme & FTP_FLAG)
      {
         db[i].protocol = FTP;
      }
      else if (scheme & LOC_FLAG)
           {
              db[i].protocol = LOC;
           }
      else if (scheme & SMTP_FLAG)
           {
              db[i].protocol = SMTP;
           }
      else if (scheme & SFTP_FLAG)
           {
              db[i].protocol = SFTP;
           }
      else if (scheme & HTTP_FLAG)
           {
              db[i].protocol = HTTP;
           }
      else if (scheme & EXEC_FLAG)
           {
              db[i].protocol = EXEC;
           }
#ifdef _WITH_SCP_SUPPORT
      else if (scheme & SCP_FLAG)
           {
              db[i].protocol = SCP;
           }
#endif
#ifdef _WITH_WMO_SUPPORT
      else if (scheme & WMO_FLAG)
           {
              db[i].protocol = WMO;
           }
#endif
#ifdef _WITH_MAP_SUPPORT
      else if (scheme & MAP_FLAG)
           {
              db[i].protocol = MAP;
           }
#endif
#ifdef _WITH_DFAX_SUPPORT
      else if (scheme & DFAX_FLAG)
           {
              db[i].protocol = DFAX;
           }
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
      else if (scheme & DE_MAIL_FLAG)
           {
              db[i].protocol = DE_MAIL;
           }
#endif
           else
           {
              update_db_log(FATAL_SIGN, __FILE__, __LINE__, udc_reply_fp, NULL,
                            "Unknown sheme in url: `%s'.", db[i].recipient);
              p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
              p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
              free(db); free(de);
              free(tmp_ptr);
              unmap_data(jd_fd, (void *)&jd);
              exit(INCORRECT);
           }

#ifdef MULTI_FS_SUPPORT
      lookup_job_id(&db[i], &jid_number,
                    ewl[de[dir_counter].ewl_pos].outgoing_file_dir,
                    ewl[de[dir_counter].ewl_pos].outgoing_file_dir_length);
#else
      lookup_job_id(&db[i], &jid_number, outgoing_file_dir,
                    outgoing_file_dir_length);
#endif
      if (db[i].time_option_type == SEND_COLLECT_TIME)
      {
         enter_time_job(i);
      }
      if (exec_flag == 1)
      {
         if ((no_fork_jobs % FORK_JOB_STEP_SIZE) == 0)
         {
            size_t new_size;

            new_size = ((no_fork_jobs / FORK_JOB_STEP_SIZE) + 1) *
                       FORK_JOB_STEP_SIZE * sizeof(struct fork_job_data);
            if ((fjd = realloc(fjd, new_size)) == NULL)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "Could not realloc() %d bytes : %s",
                          new_size, strerror(errno));
               p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
               p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
               exit(INCORRECT);
            }
         }
         fjd[no_fork_jobs].forks = 0;
         fjd[no_fork_jobs].job_id = db[i].job_id;
         fjd[no_fork_jobs].user_time = 0;
         fjd[no_fork_jobs].system_time = 0;
         no_fork_jobs++;
      }
      if ((i % 20) == 0)
      {
         time_t now = time(NULL);

         if ((now - start_time) > (JOB_TIMEOUT / 2))
         {
            send_busy_working(write_fd);
         }
      }
   } /* for (i = 0; i < no_of_jobs; i++)  */

   if ((de[dir_counter].flag & DELETE_ALL_FILES) ||
       ((de[dir_counter].flag & IN_SAME_FILESYSTEM) && (exec_flag_dir == 0)))
   {
      if ((fra[de[dir_counter].fra_pos].dir_flag & LINK_NO_EXEC) == 0)
      {
         fra[de[dir_counter].fra_pos].dir_flag |= LINK_NO_EXEC;
      }
   }
   else
   {
      if (fra[de[dir_counter].fra_pos].dir_flag & LINK_NO_EXEC)
      {
         fra[de[dir_counter].fra_pos].dir_flag ^= LINK_NO_EXEC;
      }
   }

   if (no_of_time_jobs > 1)
   {
      sort_time_job();
   }

#ifdef WITH_ERROR_QUEUE
   if ((cml = malloc(((no_of_jobs + dir_counter) * sizeof(int)))) == NULL)
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
      p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() memory : %s", strerror(errno));
      exit(INCORRECT);
   }
#endif

   /* Check for duplicate job entries. */
   for (i = 0; i < no_of_jobs; i++)
   {
#ifdef IGNORE_DUPLICATE_JOB_IDS
      for (j = (i + 1); j < no_of_jobs; j++)
      {
         if ((db[i].job_id != 0) && (db[i].job_id == db[j].job_id))
         {
            update_db_log(WARN_SIGN, __FILE__, __LINE__, udc_reply_fp, NULL,
                          "Duplicate job entries for job #%x with directory %s and recipient %s! Will ignore the duplicate entry.",
                          db[i].job_id, db[i].dir, db[i].recipient);
            db[j].job_id = 0;
         }
      }
# ifdef WITH_ERROR_QUEUE
      if (db[i].job_id != 0)
      {
         cml[i] = db[i].job_id;
      }
# endif
#else
      for (j = (i + 1); j < no_of_jobs; j++)
      {
         if (db[i].job_id == db[j].job_id)
         {
            update_db_log(WARN_SIGN, __FILE__, __LINE__, udc_reply_fp, NULL,
                          "Duplicate job entries for job #%x with directory %s and recipient %s!",
                          db[i].job_id, db[i].dir, db[i].recipient);
         }
      }
# ifdef WITH_ERROR_QUEUE
      cml[i] = db[i].job_id;
# endif
#endif
   }

#ifdef WITH_ERROR_QUEUE
   no_of_cids = i;
   for (j = 0; j < dir_counter; j++)
   {
      if (de[j].paused_dir != NULL)
      {
         cml[no_of_cids] = de[j].dir_id;
         no_of_cids++;
      }
   }
#endif

#ifdef _DISTRIBUTION_LOG
   if (max_jobs_per_file == 0)
   {
      max_jobs_per_file = 1;
   }
   j = max_jobs_per_file * sizeof(unsigned int);
   system_log(DEBUG_SIGN, NULL, 0,
              "max_jobs_per_file = %u max_file_buffer = %u",
              max_jobs_per_file, max_file_buffer);
# ifdef _WITH_PTHREAD
   for (m = 0; m < no_of_local_dirs; m++)
   {
      for (i = 0; i < fra[m].max_copied_files; i++)
      {
         for (k = 0; k < NO_OF_DISTRIBUTION_TYPES; k++)
         {
            if (((p_data[m].file_dist_pool[i][k].jid_list = malloc(j)) == NULL) ||
                ((p_data[m].file_dist_pool[i][k].proc_cycles = malloc(max_jobs_per_file)) == NULL))
            {
               p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
               p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "malloc() error : %s",
                          strerror(errno));
               exit(INCORRECT);
            }
            p_data[m].file_dist_pool[i][k].no_of_dist = 0;
         }
      }
   }
# else
   for (i = 0; i < max_file_buffer; i++)
   {
      for (k = 0; k < NO_OF_DISTRIBUTION_TYPES; k++)
      {
         if (((file_dist_pool[i][k].jid_list = malloc(j)) == NULL) ||
             ((file_dist_pool[i][k].proc_cycles = malloc(max_jobs_per_file)) == NULL))
         {
            p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
            p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "malloc() error : %s",
                       strerror(errno));
            exit(INCORRECT);
         }
         file_dist_pool[i][k].no_of_dist = 0;
      }
   }
# endif
#endif

   /* Write job list file. */
   write_current_job_list(no_of_jobs);

   /* Remove old time job directories. */
#ifdef MULTI_FS_SUPPORT
   for (i = 0; i < no_of_extra_work_dirs; i++)
   {
      if (ewl[i].time_dir != NULL)
      {
         check_old_time_jobs(no_of_jobs, ewl[i].time_dir);
      }
   }
#else
   check_old_time_jobs(no_of_jobs, time_dir);
#endif

#ifdef WITH_ERROR_QUEUE
   /* Validate error queue. */
   validate_error_queue(no_of_cids, cml, no_of_hosts, fsa, fsa_fd);

   free(cml);
#endif

   /* Free all memory. */
   free(tmp_ptr);
#ifdef WITH_GOTCHA_LIST
   free(gotcha);
   gotcha = NULL;
#endif
   unmap_data(dnb_fd, (void *)&dnb);
   unmap_data(jd_fd, (void *)&jd);
   unmap_data(fmd_fd, (void *)&fmd);
   if (pwb != NULL)
   {
      unmap_data(pwb_fd, (void *)&pwb);
   }
   p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
   p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;

   if (p_afd_status->start_time == 0)
   {
      p_afd_status->start_time = time(NULL);
   }

   if (show_one_job_no_link == YES)
   {
      if (one_job_only_dir > 1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "%d directories with only one job and no need for linking.",
                    one_job_only_dir);
      }
      else if (one_job_only_dir == 1)
           {
              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                         "One directory with only one job.");
           }

      if (not_in_same_file_system > 1)
      {
         update_db_log(INFO_SIGN, __FILE__, __LINE__, udc_reply_fp, NULL,
                       "%d directories not in the same filesystem as AFD.",
                       not_in_same_file_system);
      }
      else if (not_in_same_file_system == 1)
           {
              update_db_log(INFO_SIGN, __FILE__, __LINE__, udc_reply_fp, NULL,
                            "One directory not in the same filesystem as AFD.");
           }
   }

#ifdef WITH_ONETIME
   for (i = (dir_counter + 1); i < (no_of_local_dirs + MAX_NO_OF_ONETIME_DIRS); i++)
   {
      de[i].nfg   = 0;
      de[i].fme   = NULL;
      de[i].flag  = 0;
      de[i].dir   = NULL;
      de[i].alias = ;
      de[i].fra_pos = ;
      de[i].dir_id = ;
      de[i].search_time = 0;
      de[i].paused_dir = NULL;
   }
#endif

#ifdef _TEST_FILE_TABLE
   for (i = 0; i < no_of_local_dirs; i++)
   {
      (void)fprintf(stdout, "Directory entry %d : %s\n", i, de[i].dir);
      for (j = 0; j < de[i].nfg; j++)
      {
         (void)fprintf(stdout, "\t%d:\t", j);
         for (k = 0; k < de[i].fme[j].nfm; k++)
         {
            (void)fprintf(stdout, "%s ", de[i].fme[j].file_mask[k]);
         }
         (void)fprintf(stdout, "(%d)\n", de[i].fme[j].nfm);
         (void)fprintf(stdout, "\t\tNumber of destinations = %d\n",
                       de[i].fme[j].dest_count);
      }
      (void)fprintf(stdout, "\tNumber of file groups  = %d\n", de[i].nfg);
      if (de[i].flag & ALL_FILES)
      {
         (void)fprintf(stdout, "\tAll files selected    = YES\n");
      }
      else
      {
         (void)fprintf(stdout, "\tAll files selected    = NO\n");
      }
   }
#endif

   return(no_of_jobs);
}


/*+++++++++++++++++++++++++ send_busy_working() +++++++++++++++++++++++++*/
static void
send_busy_working(int write_fd)
{
   int action = BUSY_WORKING;

   if (write(write_fd, &action, 1) != 1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Could not write to fifo %s : %s",
                 DC_RESP_FIFO, strerror(errno));
   }

   return;
}


/*++++++++++++++++++++++++ write_current_job_list() +++++++++++++++++++++*/
static void
write_current_job_list(int no_of_jobs)
{
   int          i,
                fd;
   unsigned int *int_buf;
   size_t       buf_size;
   char         current_msg_list_file[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   (void)strcpy(current_msg_list_file, p_work_dir);
   (void)strcat(current_msg_list_file, FIFO_DIR);
   (void)strcat(current_msg_list_file, CURRENT_MSG_LIST_FILE);

   /* Overwrite current message list file. */
   if ((fd = open(current_msg_list_file, (O_WRONLY | O_CREAT | O_TRUNC),
                  FILE_MODE)) == -1)
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
      p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to open() `%s' : %s",
                 current_msg_list_file, strerror(errno));
      exit(INCORRECT);
   }
#ifdef LOCK_DEBUG
   lock_region_w(fd, (off_t)0, __FILE__, __LINE__);
#else
   lock_region_w(fd, (off_t)0);
#endif

   /* Create buffer to write ID's in one hunk. */
   buf_size = (no_of_jobs + 1) * sizeof(unsigned int);
   if ((int_buf = malloc(buf_size)) == NULL)
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
      p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() %d bytes : %s", buf_size, strerror(errno));
      exit(INCORRECT);
   }

   int_buf[0] = no_of_jobs;
   for (i = 0; i < no_of_jobs; i++)
   {
      int_buf[i + 1] = db[i].job_id;
   }

   if (write(fd, int_buf, buf_size) != buf_size)
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
      p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to write() to `%s' : %s",
                 current_msg_list_file, strerror(errno));
      exit(INCORRECT);
   }
#ifdef HAVE_STATX
   if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(fd, &stat_buf) == -1)
#endif
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                 "Failed to statx() `%s' : %s",
#else
                 "Failed to fstat() `%s' : %s",
#endif
                 current_msg_list_file, strerror(errno));
   }
   else
   {
#ifdef HAVE_STATX
      if (stat_buf.stx_size > buf_size)
#else
      if (stat_buf.st_size > buf_size)
#endif
      {
         if (ftruncate(fd, buf_size) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to ftruncate() `%s' to %d bytes : %s",
                       current_msg_list_file, buf_size, strerror(errno));
         }
      }
   }
   if (close(fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }
   free(int_buf);

   return;
}


#ifdef MULTI_FS_SUPPORT
/*++++++++++++++++++++++++ check_extra_filesystem() +++++++++++++++++++++*/
static int
check_extra_filesystem(dev_t dev, int de_no)
{
   int i;

   for (i = 0; i < no_of_extra_work_dirs; i++)
   {
      if (ewl[i].dev == dev)
      {
         if (ewl[i].dir_name == NULL)
         {
            de[de_no].ewl_pos = 0;

            return(NO);
         }
         else
         {
            de[de_no].ewl_pos = i;

            return(YES);
         }
      }
   }
   de[de_no].ewl_pos = 0;

   return(NO);
}
#endif /* MULTI_FS_SUPPORT */
