/*
 *  initialize_db.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2011 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   initialize_db - initializes the AFD database
 **
 ** SYNOPSIS
 **   void initialize_db(int init_level, int *old_value_list, int dry_run)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   20.10.2011 H.Kiehl Created
 **   04.11.2012 H.Kiehl When deleting data try only delete the data
 **                      that has changed, ie. that what is really
 **                      necessary.
 **   20.11.2018 H.Kiehl Since at startup function check_typesize_data()
 **                      converts the password database, we no longer
 **                      must delete it here.
 **   24.02.2023 H.Kiehl When initialize below level 7, try to store
 **                      afdcfg values.
 **
 */
DESCR__E_M3

#include <stdio.h>              /* sprintf(), fprintf()                  */
#include <string.h>             /* strcpy()                              */
#include <unistd.h>             /* unlink()                              */
#include <errno.h>
#include "amgdefs.h"
#include "logdefs.h"
#include "statdefs.h"

#define FSA_ID_FILE_NO              0
#define FRA_ID_FILE_NO              1
#define BLOCK_FILE_NO               2
#define AMG_COUNTER_FILE_NO         3
#define COUNTER_FILE_NO             4
#define MESSAGE_BUF_FILE_NO         5
#define MSG_CACHE_FILE_NO           6
#define MSG_QUEUE_FILE_NO           7
#ifdef SF_BURST_ACK
# define ACK_QUEUE_FILE_NO          8
#endif
#ifdef WITH_ERROR_QUEUE
# define ERROR_QUEUE_FILE_NO        9
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
# define DEMCD_QUEUE_FILE_NO        10
#endif
#define FILE_MASK_FILE_NO           11
#define DC_LIST_FILE_NO             12
#define DIR_NAME_FILE_NO            13
#define JOB_ID_DATA_FILE_NO         14
#define DCPL_FILE_NAME_NO           15
#define PWB_DATA_FILE_NO            16
#define CURRENT_MSG_LIST_FILE_NO    17
#define AMG_DATA_FILE_NO            18
#define AMG_DATA_FILE_TMP_NO        19
#define LOCK_PROC_FILE_NO           20
#define AFD_ACTIVE_FILE_NO          21
#define WINDOW_ID_FILE_NO           22
#define SYSTEM_LOG_FIFO_NO          23
#define EVENT_LOG_FIFO_NO           24
#define RECEIVE_LOG_FIFO_NO         25
#define TRANSFER_LOG_FIFO_NO        26
#define TRANS_DEBUG_LOG_FIFO_NO     27
#define AFD_CMD_FIFO_NO             28
#define AFD_RESP_FIFO_NO            29
#define AMG_CMD_FIFO_NO             30
#define DB_UPDATE_FIFO_NO           31
#define FD_CMD_FIFO_NO              32
#define AW_CMD_FIFO_NO              33
#define IP_FIN_FIFO_NO              34
#ifdef WITH_ONETIME
# define OT_FIN_FIFO_NO             35
#endif
#define SF_FIN_FIFO_NO              36
#define RETRY_FD_FIFO_NO            37
#define FD_DELETE_FIFO_NO           38
#define FD_WAKE_UP_FIFO_NO          39
#define TRL_CALC_FIFO_NO            40
#define QUEUE_LIST_READY_FIFO_NO    41
#define QUEUE_LIST_DONE_FIFO_NO     42
#define PROBE_ONLY_FIFO_NO          43
#ifdef _INPUT_LOG
# define INPUT_LOG_FIFO_NO          44
#endif
#ifdef _DISTRIBUTION_LOG
# define DISTRIBUTION_LOG_FIFO_NO   45
#endif
#ifdef _OUTPUT_LOG
# define OUTPUT_LOG_FIFO_NO         46
#endif
#ifdef _CONFIRMATION_LOG
# define CONFIRMATION_LOG_FIFO_NO   47
#endif
#ifdef _DELETE_LOG
# define DELETE_LOG_FIFO_NO         48
#endif
#ifdef _PRODUCTION_LOG
# define PRODUCTION_LOG_FIFO_NO     49
#endif
#define DEL_TIME_JOB_FIFO_NO        50
#define MSG_FIFO_NO                 51
#define DC_CMD_FIFO_NO              52
#define DC_RESP_FIFO_NO             53
#define AFDD_LOG_FIFO_NO            54
#define AFDDS_LOG_FIFO_NO           55
#define TYPESIZE_DATA_FILE_NO       56
#define SYSTEM_DATA_FILE_NO         57
#ifdef _MAINTAINER_LOG
# define MAINTAINER_LOG_FIFO_NO     58
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
# define DEMCD_FIFO_NO              59
#endif
#ifdef SF_BURST_ACK
# define SF_BURST_ACK_FIFO_NO       60
#endif
#define MAX_FILE_LIST_LENGTH        61

#define FSA_STAT_FILE_ALL_NO        0
#define FRA_STAT_FILE_ALL_NO        1
#define AFD_STATUS_FILE_ALL_NO      2
#define ALTERNATE_FILE_ALL_NO       3
#define DB_UPDATE_REPLY_FIFO_ALL_NO 4
#define NNN_FILE_ALL_NO             5
#define MAX_MFILE_LIST_LENGTH       6

#define AFD_MSG_DIR_FLAG            1
#ifdef WITH_DUP_CHECK
# define CRC_DIR_FLAG               2
#endif
#define FILE_MASK_DIR_FLAG          4
#define LS_DATA_DIR_FLAG            8

/* External global variables. */
extern int  sys_log_fd;
extern char *p_work_dir;

/* Local function prototypes. */
static void afdcfg_save_status(void),
            delete_fifodir_files(char *, int, char *, char *, int, int),
            delete_log_files(char *, int, int);


/*########################## initialize_db() ############################*/
void
initialize_db(int init_level, int *old_value_list, int dry_run)
{
   int  delete_dir,
        offset;
   char dirs[MAX_PATH_LENGTH],
        filelistflag[MAX_FILE_LIST_LENGTH],
        mfilelistflag[MAX_MFILE_LIST_LENGTH];

   (void)memset(filelistflag, NO, MAX_FILE_LIST_LENGTH);
   (void)memset(mfilelistflag, NO, MAX_MFILE_LIST_LENGTH);
   if (old_value_list != NULL)
   {
      delete_dir = 0;
      if (old_value_list[0] & MAX_MSG_NAME_LENGTH_NR)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
         filelistflag[MSG_QUEUE_FILE_NO] = YES;
#ifdef SF_BURST_ACK
         filelistflag[ACK_QUEUE_FILE_NO] = YES;
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
         filelistflag[DEMCD_QUEUE_FILE_NO] = YES;
#endif
      }
      if (old_value_list[0] & MAX_FILENAME_LENGTH_NR)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
#ifdef _WITH_DE_MAIL_SUPPORT
         filelistflag[DEMCD_QUEUE_FILE_NO] = YES;
#endif
         delete_dir |= LS_DATA_DIR_FLAG;
      }
      if (old_value_list[0] & MAX_HOSTNAME_LENGTH_NR)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
         filelistflag[FRA_ID_FILE_NO] = YES;
         mfilelistflag[FRA_STAT_FILE_ALL_NO] = YES;
         filelistflag[JOB_ID_DATA_FILE_NO] = YES;
#ifdef _WITH_DE_MAIL_SUPPORT
         filelistflag[DEMCD_QUEUE_FILE_NO] = YES;
#endif
      }
      if (old_value_list[0] & MAX_REAL_HOSTNAME_LENGTH_NR)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
         mfilelistflag[AFD_STATUS_FILE_ALL_NO] = YES;
      }
      if (old_value_list[0] & MAX_PROXY_NAME_LENGTH_NR)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
      }
      if (old_value_list[0] & MAX_TOGGLE_STR_LENGTH_NR)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
      }
      if (old_value_list[0] & ERROR_HISTORY_LENGTH_NR)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
      }
      if (old_value_list[0] & MAX_NO_PARALLEL_JOBS_NR)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
      }
      if (old_value_list[0] & MAX_DIR_ALIAS_LENGTH_NR)
      {
         filelistflag[FRA_ID_FILE_NO] = YES;
         mfilelistflag[FRA_STAT_FILE_ALL_NO] = YES;
      }
      if (old_value_list[0] & MAX_RECIPIENT_LENGTH_NR)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
         filelistflag[JOB_ID_DATA_FILE_NO] = YES;
      }
      if (old_value_list[0] & MAX_WAIT_FOR_LENGTH_NR)
      {
         filelistflag[FRA_ID_FILE_NO] = YES;
         mfilelistflag[FRA_STAT_FILE_ALL_NO] = YES;
      }
      if (old_value_list[0] & MAX_FRA_TIME_ENTRIES_NR)
      {
         filelistflag[FRA_ID_FILE_NO] = YES;
         mfilelistflag[FRA_STAT_FILE_ALL_NO] = YES;
      }
      if (old_value_list[0] & MAX_OPTION_LENGTH_NR)
      {
         filelistflag[JOB_ID_DATA_FILE_NO] = YES;
      }
      if (old_value_list[0] & MAX_PATH_LENGTH_NR)
      {
         filelistflag[DIR_NAME_FILE_NO] = YES;
         filelistflag[DC_LIST_FILE_NO] = YES;
      }
      if (old_value_list[0] & MAX_USER_NAME_LENGTH_NR)
      {
         /* Password are changed by check_typesize_data(). */;
      }
      if ((old_value_list[0] & CHAR_NR) ||
          (old_value_list[0] & INT_NR))
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         filelistflag[FRA_ID_FILE_NO] = YES;
         /* No need to delete BLOCK_FILE */
         filelistflag[AMG_COUNTER_FILE_NO] = YES;
         filelistflag[COUNTER_FILE_NO] = YES;
         filelistflag[MESSAGE_BUF_FILE_NO] = YES;
         filelistflag[MSG_CACHE_FILE_NO] = YES;
         filelistflag[MSG_QUEUE_FILE_NO] = YES;
#ifdef WITH_ERROR_QUEUE
         filelistflag[ERROR_QUEUE_FILE_NO] = YES;
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
         filelistflag[DEMCD_QUEUE_FILE_NO] = YES;
#endif
         filelistflag[FILE_MASK_FILE_NO] = YES;
         filelistflag[DC_LIST_FILE_NO] = YES;
         filelistflag[DIR_NAME_FILE_NO] = YES;
         filelistflag[JOB_ID_DATA_FILE_NO] = YES;
         filelistflag[DCPL_FILE_NAME_NO] = YES;
         filelistflag[PWB_DATA_FILE_NO] = YES;
         filelistflag[CURRENT_MSG_LIST_FILE_NO] = YES;
         filelistflag[AMG_DATA_FILE_NO] = YES;
         filelistflag[AMG_DATA_FILE_TMP_NO] = YES;
         filelistflag[LOCK_PROC_FILE_NO] = YES;
         filelistflag[AFD_ACTIVE_FILE_NO] = YES;
         filelistflag[TYPESIZE_DATA_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
         mfilelistflag[FRA_STAT_FILE_ALL_NO] = YES;
         mfilelistflag[AFD_STATUS_FILE_ALL_NO] = YES;
         mfilelistflag[ALTERNATE_FILE_ALL_NO] = YES;
         mfilelistflag[NNN_FILE_ALL_NO] = YES;
         delete_dir |= LS_DATA_DIR_FLAG;
      }
      if (old_value_list[0] & OFF_T_NR)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
         filelistflag[MSG_QUEUE_FILE_NO] = YES;
#ifdef _WITH_DE_MAIL_SUPPORT
         filelistflag[DEMCD_QUEUE_FILE_NO] = YES;
#endif
         delete_dir |= LS_DATA_DIR_FLAG;
      }
      if (old_value_list[0] & TIME_T_NR)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
         filelistflag[MSG_QUEUE_FILE_NO] = YES;
#ifdef SF_BURST_ACK
         filelistflag[ACK_QUEUE_FILE_NO] = YES;
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
         filelistflag[DEMCD_QUEUE_FILE_NO] = YES;
#endif
         delete_dir |= LS_DATA_DIR_FLAG;
      }
      if (old_value_list[0] & SHORT_NR)
      {
         filelistflag[FRA_ID_FILE_NO] = YES;
         mfilelistflag[FRA_STAT_FILE_ALL_NO] = YES;
      }
#ifdef HAVE_LONG_LONG
      if (old_value_list[0] & LONG_LONG_NR)
      {
         filelistflag[FRA_ID_FILE_NO] = YES;
         mfilelistflag[FRA_STAT_FILE_ALL_NO] = YES;
      }
#endif
      if (old_value_list[0] & PID_T_NR)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
         filelistflag[MSG_QUEUE_FILE_NO] = YES;
      }
   }
   else
   {
      delete_dir = 0;
      if (init_level > 0)
      {
         filelistflag[SYSTEM_LOG_FIFO_NO] = YES;
#ifdef _MAINTAINER_LOG
         filelistflag[MAINTAINER_LOG_FIFO_NO] = YES;
#endif
         filelistflag[EVENT_LOG_FIFO_NO] = YES;
         filelistflag[RECEIVE_LOG_FIFO_NO] = YES;
         filelistflag[TRANSFER_LOG_FIFO_NO] = YES;
         filelistflag[TRANS_DEBUG_LOG_FIFO_NO] = YES;
         filelistflag[AFD_CMD_FIFO_NO] = YES;
         filelistflag[AFD_RESP_FIFO_NO] = YES;
         filelistflag[AMG_CMD_FIFO_NO] = YES;
         filelistflag[DB_UPDATE_FIFO_NO] = YES;
         filelistflag[FD_CMD_FIFO_NO] = YES;
         filelistflag[AW_CMD_FIFO_NO] = YES;
         filelistflag[IP_FIN_FIFO_NO] = YES;
#ifdef WITH_ONETIME
         filelistflag[OT_FIN_FIFO_NO] = YES;
#endif
         filelistflag[SF_FIN_FIFO_NO] = YES;
#ifdef SF_BURST_ACK
         filelistflag[SF_BURST_ACK_FIFO_NO] = YES;
#endif
         filelistflag[RETRY_FD_FIFO_NO] = YES;
         filelistflag[FD_DELETE_FIFO_NO] = YES;
         filelistflag[FD_WAKE_UP_FIFO_NO] = YES;
         filelistflag[TRL_CALC_FIFO_NO] = YES;
         filelistflag[QUEUE_LIST_READY_FIFO_NO] = YES;
         filelistflag[QUEUE_LIST_DONE_FIFO_NO] = YES;
         filelistflag[PROBE_ONLY_FIFO_NO] = YES;
#ifdef _INPUT_LOG
         filelistflag[INPUT_LOG_FIFO_NO] = YES;
#endif
#ifdef _DISTRIBUTION_LOG
         filelistflag[DISTRIBUTION_LOG_FIFO_NO] = YES;
#endif
#ifdef _OUTPUT_LOG
         filelistflag[OUTPUT_LOG_FIFO_NO] = YES;
#endif
#ifdef _CONFIRMATION_LOG
         filelistflag[CONFIRMATION_LOG_FIFO_NO] = YES;
#endif
#ifdef _DELETE_LOG
         filelistflag[DELETE_LOG_FIFO_NO] = YES;
#endif
#ifdef _PRODUCTION_LOG
         filelistflag[PRODUCTION_LOG_FIFO_NO] = YES;
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
         filelistflag[DEMCD_FIFO_NO] = YES;
#endif
         filelistflag[DEL_TIME_JOB_FIFO_NO] = YES;
         filelistflag[MSG_FIFO_NO] = YES;
         filelistflag[DC_CMD_FIFO_NO] = YES;
         filelistflag[DC_RESP_FIFO_NO] = YES;
         filelistflag[AFDD_LOG_FIFO_NO] = YES;
         filelistflag[AFDDS_LOG_FIFO_NO] = YES;
         mfilelistflag[DB_UPDATE_REPLY_FIFO_ALL_NO] = YES;
      }
      if (init_level > 1)
      {
         filelistflag[AFD_ACTIVE_FILE_NO] = YES;
         filelistflag[WINDOW_ID_FILE_NO] = YES;
         filelistflag[LOCK_PROC_FILE_NO] = YES;
         filelistflag[DCPL_FILE_NAME_NO] = YES;
      }
      if (init_level > 2)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
         filelistflag[FRA_ID_FILE_NO] = YES;
         mfilelistflag[FRA_STAT_FILE_ALL_NO] = YES;
         filelistflag[AMG_DATA_FILE_NO] = YES;
         filelistflag[AMG_DATA_FILE_TMP_NO] = YES;
         mfilelistflag[ALTERNATE_FILE_ALL_NO] = YES;
      }
      if (init_level > 3)
      {
         delete_dir |= AFD_MSG_DIR_FLAG | FILE_MASK_DIR_FLAG;
         filelistflag[MESSAGE_BUF_FILE_NO] = YES;
         filelistflag[MSG_CACHE_FILE_NO] = YES;
         filelistflag[MSG_QUEUE_FILE_NO] = YES;
#ifdef SF_BURST_ACK
         filelistflag[ACK_QUEUE_FILE_NO] = YES;
#endif
#ifdef WITH_ERROR_QUEUE
         filelistflag[ERROR_QUEUE_FILE_NO] = YES;
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
         filelistflag[DEMCD_QUEUE_FILE_NO] = YES;
#endif
         filelistflag[CURRENT_MSG_LIST_FILE_NO] = YES;
      }
      if (init_level > 4)
      {
         filelistflag[FILE_MASK_FILE_NO] = YES;
         filelistflag[DC_LIST_FILE_NO] = YES;
         filelistflag[DIR_NAME_FILE_NO] = YES;
         filelistflag[JOB_ID_DATA_FILE_NO] = YES;
      }
      if (init_level > 5)
      {
         mfilelistflag[AFD_STATUS_FILE_ALL_NO] = YES;
      }
      if (init_level > 6)
      {
         filelistflag[BLOCK_FILE_NO] = YES;
         filelistflag[AMG_COUNTER_FILE_NO] = YES;
         filelistflag[COUNTER_FILE_NO] = YES;
         mfilelistflag[NNN_FILE_ALL_NO] = YES;
#ifdef WITH_DUP_CHECK
         delete_dir |= CRC_DIR_FLAG;
#endif
      }
      if (init_level > 7)
      {
         filelistflag[PWB_DATA_FILE_NO] = YES;
         filelistflag[TYPESIZE_DATA_FILE_NO] = YES;
         filelistflag[SYSTEM_DATA_FILE_NO] = YES;
         delete_dir |= LS_DATA_DIR_FLAG;
      }
   }

   offset = snprintf(dirs, MAX_PATH_LENGTH, "%s%s", p_work_dir, FIFO_DIR);
   delete_fifodir_files(dirs, offset, filelistflag, mfilelistflag,
                        init_level, dry_run);
   if (delete_dir & AFD_MSG_DIR_FLAG)
   {
      (void)snprintf(dirs, MAX_PATH_LENGTH, "%s%s", p_work_dir, AFD_MSG_DIR);
      if (dry_run == YES)
      {
         (void)fprintf(stdout, "rm -rf %s\n", dirs);
      }
      else
      {
         if (rec_rmdir(dirs) == INCORRECT)
         {
            (void)fprintf(stderr,
                           _("WARNING : Failed to delete everything in %s.\n"),
                           dirs);
         }
      }
   }
#ifdef WITH_DUP_CHECK
   if (delete_dir & CRC_DIR_FLAG)
   {
      (void)snprintf(dirs, MAX_PATH_LENGTH, "%s%s%s",
                     p_work_dir, AFD_FILE_DIR, CRC_DIR);
      if (dry_run == YES)
      {
         (void)fprintf(stdout, "rm -rf %s\n", dirs);
      }
      else
      {
         if (rec_rmdir(dirs) == INCORRECT)
         {
            (void)fprintf(stderr,
                          _("WARNING : Failed to delete everything in %s.\n"),
                          dirs);
         }
      }
   }
#endif
   if (delete_dir & FILE_MASK_DIR_FLAG)
   {
      (void)snprintf(dirs, MAX_PATH_LENGTH, "%s%s%s%s",
                     p_work_dir, AFD_FILE_DIR, INCOMING_DIR, FILE_MASK_DIR);
      if (dry_run == YES)
      {
         (void)fprintf(stdout, "rm -rf %s\n", dirs);
      }
      else
      {
         if (rec_rmdir(dirs) == INCORRECT)
         {
            (void)fprintf(stderr,
                          _("WARNING : Failed to delete everything in %s.\n"),
                          dirs);
         }
      }
   }
   if (delete_dir & LS_DATA_DIR_FLAG)
   {
      (void)snprintf(dirs, MAX_PATH_LENGTH, "%s%s%s%s",
                     p_work_dir, AFD_FILE_DIR, INCOMING_DIR, LS_DATA_DIR);
      if (dry_run == YES)
      {
         (void)fprintf(stdout, "rm -rf %s\n", dirs);
      }
      else
      {
         if (rec_rmdir(dirs) == INCORRECT)
         {
            (void)fprintf(stderr,
                          _("WARNING : Failed to delete everything in %s.\n"),
                          dirs);
         }
      }
   }
   if (init_level > 8)
   {
#ifdef MULTI_FS_SUPPORT
      int                    i,
                             no_of_extra_work_dirs;
      char                   archive_dir[MAX_PATH_LENGTH];
      struct extra_work_dirs *ewl;

      get_extra_work_dirs(NULL, &no_of_extra_work_dirs, &ewl, NO);
      for (i = 0; i < no_of_extra_work_dirs; i++)
      {
         if (dry_run == YES)
         {
            (void)fprintf(stdout, "rm -rf %s\n", ewl[i].afd_file_dir);
         }
         else
         {
            if (rec_rmdir(ewl[i].afd_file_dir) == INCORRECT)
            {
               (void)fprintf(stderr,
                             _("WARNING : Failed to delete everything in %s.\n"),
                             ewl[i].afd_file_dir);
            }
         }
         (void)memcpy(archive_dir, ewl[i].dir_name, ewl[i].dir_name_length);
         (void)memcpy(archive_dir + ewl[i].dir_name_length, AFD_ARCHIVE_DIR,
                      AFD_ARCHIVE_DIR_LENGTH);
         *(archive_dir + ewl[i].dir_name_length + AFD_ARCHIVE_DIR_LENGTH) = '\0';
         if (dry_run == YES)
         {
            (void)fprintf(stdout, "rm -rf %s\n", archive_dir);
         }
         else
         {
            if (rec_rmdir(archive_dir) == INCORRECT)
            {
               (void)fprintf(stderr,
                             _("WARNING : Failed to delete everything in %s.\n"),
                             archive_dir);
            }
         }
      }
      free_extra_work_dirs(no_of_extra_work_dirs, &ewl);
#else
      (void)snprintf(dirs, MAX_PATH_LENGTH, "%s%s", p_work_dir, AFD_FILE_DIR);
      if (dry_run == YES)
      {
         (void)fprintf(stdout, "rm -rf %s\n", dirs);
      }
      else
      {
         if (rec_rmdir(dirs) == INCORRECT)
         {
            (void)fprintf(stderr,
                          _("WARNING : Failed to delete everything in %s.\n"),
                          dirs);
         }
      }
      (void)snprintf(dirs, MAX_PATH_LENGTH, "%s%s", p_work_dir, AFD_ARCHIVE_DIR);
      if (dry_run == YES)
      {
         (void)fprintf(stdout, "rm -rf %s\n", dirs);
      }
      else
      {
         if (rec_rmdir(dirs) == INCORRECT)
         {
            (void)fprintf(stderr,
                          _("WARNING : Failed to delete everything in %s.\n"),
                          dirs);
         }
      }
#endif
      offset = snprintf(dirs, MAX_PATH_LENGTH, "%s%s", p_work_dir, LOG_DIR);
      delete_log_files(dirs, offset, dry_run);
   }

   return;
}


/*+++++++++++++++++++++++++ delete_fifodir_files() ++++++++++++++++++++++*/
static void
delete_fifodir_files(char *fifodir,
                     int  offset,
                     char *filelistflag,
                     char *mfilelistflag,
                     int  init_level,
                     int  dry_run)
{
   int  i,
        tmp_sys_log_fd;
   char *file_ptr,
        *filelist[] =
        {
           FSA_ID_FILE,
           FRA_ID_FILE,
           BLOCK_FILE,
           AMG_COUNTER_FILE,
           COUNTER_FILE,
           MESSAGE_BUF_FILE,
           MSG_CACHE_FILE,
           MSG_QUEUE_FILE,
#ifdef SF_BURST_ACK
           ACK_QUEUE_FILE,
#endif
#ifdef WITH_ERROR_QUEUE
           ERROR_QUEUE_FILE,
#else
           "",
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
           DEMCD_QUEUE_FILE,
#else
           "",
#endif
           FILE_MASK_FILE,
           DC_LIST_FILE,
           DIR_NAME_FILE,
           JOB_ID_DATA_FILE,
           DCPL_FILE_NAME,
           PWB_DATA_FILE,
           CURRENT_MSG_LIST_FILE,
           AMG_DATA_FILE,
           AMG_DATA_FILE_TMP,
           LOCK_PROC_FILE,
           AFD_ACTIVE_FILE,
           WINDOW_ID_FILE,
           SYSTEM_LOG_FIFO,
#ifdef _MAINTAINER_LOG
           MAINTAINER_LOG_FIFO,
#else
           "",
#endif
           EVENT_LOG_FIFO,
           RECEIVE_LOG_FIFO,
           TRANSFER_LOG_FIFO,
           TRANS_DEBUG_LOG_FIFO,
           AFD_CMD_FIFO,
           AFD_RESP_FIFO,
           AMG_CMD_FIFO,
           DB_UPDATE_FIFO,
           FD_CMD_FIFO,
           AW_CMD_FIFO,
           IP_FIN_FIFO,
#ifdef WITH_ONETIME
           OT_FIN_FIFO,
#else
           "",
#endif
           SF_FIN_FIFO,
#ifdef SF_BURST_ACK
           SF_BURST_ACK_FIFO,
#endif
           RETRY_FD_FIFO,
           FD_DELETE_FIFO,
           FD_WAKE_UP_FIFO,
           TRL_CALC_FIFO,
           QUEUE_LIST_READY_FIFO,
           QUEUE_LIST_DONE_FIFO,
           PROBE_ONLY_FIFO,
#ifdef _INPUT_LOG
           INPUT_LOG_FIFO,
#else
           "",
#endif
#ifdef _DISTRIBUTION_LOG
           DISTRIBUTION_LOG_FIFO,
#else
           "",
#endif
#ifdef _OUTPUT_LOG
           OUTPUT_LOG_FIFO,
#else
           "",
#endif
#ifdef _CONFIRMATION_LOG
           CONFIRMATION_LOG_FIFO,
#else
           "",
#endif
#ifdef _DELETE_LOG
           DELETE_LOG_FIFO,
#else
           "",
#endif
#ifdef _PRODUCTION_LOG
           PRODUCTION_LOG_FIFO,
#else
           "",
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
           DEMCD_FIFO,
#else
           "",
#endif
           DEL_TIME_JOB_FIFO,
           MSG_FIFO,
           DC_CMD_FIFO, /* from amgdefs.h */
           DC_RESP_FIFO,
           AFDD_LOG_FIFO,
           AFDDS_LOG_FIFO,
           TYPESIZE_DATA_FILE,
#ifdef STAT_IN_FIFODIR
           NEW_STATISTIC_FILE,
           NEW_ISTATISTIC_FILE,
#endif
           SYSTEM_DATA_FILE
        },
        *mfilelist[] =
        {
           FSA_STAT_FILE_ALL,
           FRA_STAT_FILE_ALL,
           AFD_STATUS_FILE_ALL,
           ALTERNATE_FILE_ALL,
           DB_UPDATE_REPLY_FIFO_ALL,
#ifdef STAT_IN_FIFODIR
           ISTATISTIC_FILE_ALL,
           STATISTIC_FILE_ALL,
#endif
           NNN_FILE_ALL
        };

   file_ptr = fifodir + offset;

   if ((init_level < 8) && (dry_run != YES) &&
       ((mfilelistflag[FSA_STAT_FILE_ALL_NO] == YES) ||
        (mfilelistflag[FRA_STAT_FILE_ALL_NO] == YES)))
   {
      afdcfg_save_status();
   }

   /* Delete single files. */
   for (i = 0; i < (sizeof(filelist) / sizeof(*filelist)); i++)
   {
      if (filelistflag[i] == YES)
      {
         (void)strcpy(file_ptr, filelist[i]);
         if (dry_run == YES)
         {
            (void)fprintf(stdout, "rm -f %s\n", fifodir);
         }
         else
         {
            (void)unlink(fifodir);
         }
      }
   }
   *file_ptr = '\0';

   tmp_sys_log_fd = sys_log_fd;
   sys_log_fd = STDOUT_FILENO;

   /* Delete multiple files. */
   for (i = 0; i < (sizeof(mfilelist) / sizeof(*mfilelist)); i++)
   {
      if (mfilelistflag[i] == YES)
      {
         if (dry_run == YES)
         {
            (void)fprintf(stdout, "rm -f %s/%s\n", fifodir, &mfilelist[i][1]);
         }
         else
         {
            (void)remove_files(fifodir, &mfilelist[i][1]);
         }
      }
   }

   sys_log_fd = tmp_sys_log_fd;

   return;
}


/*------------------------ afdcfg_save_status() -------------------------*/
static void
afdcfg_save_status(void)
{
   char exec_cmd[MAX_PATH_LENGTH];
   FILE *fp;

   /* Call 'afdcfg --save_status' */
   (void)snprintf(exec_cmd, MAX_PATH_LENGTH,
                  "%s -w %s --save_status %s%s%s 2>&1",
                  AFDCFG, p_work_dir, p_work_dir, FIFO_DIR, AFDCFG_RECOVER);
   if ((fp = popen(exec_cmd, "r")) == NULL)
   {
      (void)fprintf(stderr, "Failed to popen() `%s' : %s\n",
                    exec_cmd, strerror(errno));
   }
   else
   {
      int status = 0;

      exec_cmd[0] = '\0';
      while (fgets(exec_cmd, MAX_PATH_LENGTH, fp) != NULL)
      {
         ;
      }
      if (exec_cmd[0] != '\0')
      {
         (void)fprintf(stderr, "%s failed : `%s'\n", AFDCFG, exec_cmd);
         status = 1;
      }
      if (ferror(fp))
      {
         (void)fprintf(stderr, "ferror() error : %s\n", strerror(errno));
         status |= 2;
      }
      if (pclose(fp) == -1)
      {
         (void)fprintf(stderr, "Failed to pclose() : %s\n", strerror(errno));
      }
      if (status == 0)
      {
         (void)fprintf(stderr, "DEBUG: %s saved status\n", AFDCFG);
      }
   }

   return;
}


/*++++++++++++++++++++++++++ delete_log_files() +++++++++++++++++++++++++*/
static void
delete_log_files(char *logdir, int offset, int dry_run)
{
   int  i,
        tmp_sys_log_fd;
   char *log_ptr,
        *loglist[] =
        {
#ifdef STAT_IN_FIFODIR
           "/DAEMON_LOG.init_afd"
#else
           "/DAEMON_LOG.init_afd",
           NEW_STATISTIC_FILE,
           NEW_ISTATISTIC_FILE
#endif
        },
        *mloglist[] =
        {
           SYSTEM_LOG_NAME_ALL,
#ifdef _MAINTAINER_LOG
           MAINTAINER_LOG_NAME_ALL,
#endif
           EVENT_LOG_NAME_ALL,
           RECEIVE_LOG_NAME_ALL,
           TRANSFER_LOG_NAME_ALL,
#ifdef _TRANSFER_RATE_LOG
           TRANSFER_RATE_LOG_NAME_ALL,
#endif
#ifdef _INPUT_LOG
           INPUT_BUFFER_FILE_ALL,
#endif
#ifdef _DISTRIBUTION_LOG
           DISTRIBUTION_BUFFER_FILE_ALL,
#endif
#ifdef _OUTPUT_LOG
           OUTPUT_BUFFER_FILE_ALL,
#endif
#ifdef _CONFIRMATION_LOG
           CONFIRMATION_BUFFER_FILE_ALL,
#endif
#ifdef _DELETE_LOG
           DELETE_BUFFER_FILE_ALL,
#endif
#ifdef _PRODUCTION_LOG
           PRODUCTION_BUFFER_FILE_ALL,
#endif
#ifdef STAT_IN_FIFODIR
           TRANS_DB_LOG_NAME_ALL
#else
           TRANS_DB_LOG_NAME_ALL,
           ISTATISTIC_FILE_ALL,
           STATISTIC_FILE_ALL
#endif
        };

   log_ptr = logdir + offset;

   /* Delete single files. */
   for (i = 0; i < (sizeof(loglist) / sizeof(*loglist)); i++)
   {
      (void)strcpy(log_ptr, loglist[i]);
      if (dry_run == YES)
      {
         (void)fprintf(stdout, "rm -f %s\n", logdir);
      }
      else
      {
         (void)unlink(logdir);
      }
   }
   *log_ptr = '\0';

   tmp_sys_log_fd = sys_log_fd;
   sys_log_fd = STDOUT_FILENO;

   /* Delete multiple files. */
   for (i = 0; i < (sizeof(mloglist)/sizeof(*mloglist)); i++)
   {
      if (dry_run == YES)
      {
         (void)fprintf(stdout, "rm -f %s/%s\n", logdir, mloglist[i]);
      }
      else
      {
         (void)remove_files(logdir, mloglist[i]);
      }
   }

   sys_log_fd = tmp_sys_log_fd;

   return;
}
