/*
 *  msa_view.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2020 Deutscher Wetterdienst (DWD),
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
 **   msa_view - shows all information in the MSA about a specific
 **              AFD
 **
 ** SYNOPSIS
 **   msa_view [-w <working directory>] afdname|position
 **
 ** DESCRIPTION
 **   This program shows all information about a specific AFD in the
 **   MSA.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.01.1999 H.Kiehl Created
 **   13.09.2000 H.Kiehl Addition of top number of transfers.
 **   03.12.2003 H.Kiehl Added connection and disconnection time.
 **   27.02.2005 H.Kiehl Option to switch between two AFD's.
 **   29.09.2015 H.Kiehl If error_host_counter is greater 0 show which
 **                      host have what errors.
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr            */
#include <string.h>                      /* strerror()                   */
#include <stdlib.h>                      /* atoi()                       */
#include <ctype.h>                       /* isdigit()                    */
#include <time.h>                        /* ctime()                      */
#include <sys/types.h>
#include <unistd.h>                      /* STDERR_FILENO                */
#include <errno.h>
#include "mondefs.h"
#include "fddefs.h"
#include "sumdefs.h"
#include "version.h"

/* Global variables. */
int                    sys_log_fd = STDERR_FILENO,   /* Not used!    */
                       msa_fd = -1,
                       msa_id,
                       no_of_afds = 0;
off_t                  msa_size;
char                   *p_work_dir;
struct mon_status_area *msa;
const char             *sys_log_name = MON_SYS_LOG_FIFO;

/* Local function prototype. */
static void usage(void);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ msa_view() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int                  i, j, k,
                        last = 0,
                        position = -1;
   char                 afdname[MAX_AFDNAME_LENGTH + 1],
                        ahl_file[MAX_PATH_LENGTH],
                        *ptr,
                        work_dir[MAX_PATH_LENGTH];
   struct afd_host_list *ahl;

   CHECK_FOR_VERSION(argc, argv);

   if (get_mon_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if (argc == 2)
   {
      if (isdigit((int)(argv[1][0])) != 0)
      {
         position = atoi(argv[1]);
         last = position + 1;
      }
      else
      {
         (void)my_strncpy(afdname, argv[1], MAX_AFDNAME_LENGTH + 1);
      }
   }
   else if (argc == 1)
        {
           position = -2;
        }
        else
        {
           usage();
           exit(INCORRECT);
        }

   if ((i = msa_attach_passive()) < 0)
   {
      if (i == INCORRECT_VERSION)
      {
         (void)fprintf(stderr,
                       "ERROR   : This program is not able to attach to the MSA due to incorrect version. (%s %d)\n",
                       __FILE__, __LINE__);
      }
      else
      {
         (void)fprintf(stderr, "ERROR   : Failed to attach to MSA. (%s %d)\n",
                       __FILE__, __LINE__);
      }
      exit(INCORRECT);
   }

   if (position == -1)
   {
      for (i = 0; i < no_of_afds; i++)
      {
         if (my_strcmp(msa[i].afd_alias, afdname) == 0)
         {
            position = i;
            break;
         }
      }
      if (position < 0)
      {
         (void)fprintf(stderr,
                       "WARNING : Could not find AFD `%s' in MSA. (%s %d)\n",
                       afdname, __FILE__, __LINE__);
         exit(INCORRECT);
      }
      last = position + 1;
   }
   else if (position == -2)
        {
           last = no_of_afds;
           position = 0;
        }
   else if (position >= no_of_afds)
        {
           (void)fprintf(stderr,
                         "WARNING : There are only %d AFD's in the MSA. (%s %d)\n",
                         no_of_afds, __FILE__, __LINE__);
           exit(INCORRECT);
        }

   ptr = (char *)msa;
   ptr -= AFD_WORD_OFFSET;
   (void)fprintf(stdout, " Number of hosts: %d  MSA ID: %d  Struct Version: %d\n\n",
                 no_of_afds, msa_id, (int)(*(ptr + SIZEOF_INT + 1 + 1 + 1)));

   for (j = position; j < last; j++)
   {
      (void)fprintf(stdout, "=============================> %s (%d) <=============================\n",
                    msa[j].afd_alias, j);
#ifdef NEW_MSA
      (void)fprintf(stdout, "AFD alias CRC      : %x\n", msa[j].afd_id);
#endif
      (void)fprintf(stdout, "Remote work dir    : %s\n", msa[j].r_work_dir);
      (void)fprintf(stdout, "Remote AFD version : %s\n", msa[j].afd_version);
      (void)fprintf(stdout, "Remote command     : %s\n", msa[j].rcmd);
      (void)fprintf(stdout, "Remote options     : %d =>", msa[j].options);
      if (msa[j].options == 0)
      {
         (void)fprintf(stdout, " None");
      }
      else
      {
         if (msa[j].options & COMPRESS_FLAG)
         {
            (void)fprintf(stdout, " COMPRESS");
         }
         if (msa[j].options & MINUS_Y_FLAG)
         {
            (void)fprintf(stdout, " MINUS_Y");
         }
         if (msa[j].options & DONT_USE_FULL_PATH_FLAG)
         {
            (void)fprintf(stdout, " DONT_USE_FULL_PATH");
         }
         if (msa[j].options & ENABLE_SSL_ENCRYPTION)
         {
            (void)fprintf(stdout, " ENABLE_SSL_ENCRYPTION");
         }
         if (msa[j].options & AFDD_SYSTEM_LOG)
         {
            (void)fprintf(stdout, " System");
         }
         if (msa[j].options & AFDD_RECEIVE_LOG)
         {
            (void)fprintf(stdout, " Receive");
         }
         if (msa[j].options & AFDD_TRANSFER_LOG)
         {
            (void)fprintf(stdout, " Transfer");
         }
         if (msa[j].options & AFDD_TRANSFER_DEBUG_LOG)
         {
            (void)fprintf(stdout, " Trans_db");
         }
#ifdef _INPUT_LOG
         if (msa[j].options & AFDD_INPUT_LOG)
         {
            (void)fprintf(stdout, " Input");
         }
#endif
#ifdef _DISTRIBUTION_LOG
         if (msa[j].options & AFDD_DISTRIBUTION_LOG)
         {
            (void)fprintf(stdout, " Distribution");
         }
#endif
#ifdef _PRODUCTION_LOG
         if (msa[j].options & AFDD_PRODUCTION_LOG)
         {
            (void)fprintf(stdout, " Production");
         }
#endif
#ifdef _OUTPUT_LOG
         if (msa[j].options & AFDD_OUTPUT_LOG)
         {
            (void)fprintf(stdout, " Output");
         }
#endif
#ifdef _DELETE_LOG
         if (msa[j].options & AFDD_DELETE_LOG)
         {
            (void)fprintf(stdout, " Delete");
         }
#endif
         if (msa[j].options & AFDD_JOB_DATA)
         {
            (void)fprintf(stdout, " Job_data");
         }
         if (msa[j].options & AFDD_COMPRESSION_1)
         {
            (void)fprintf(stdout, " Compression1");
         }
      }
      (void)fprintf(stdout, "\n");
      (void)fprintf(stdout, "Log capabilities   : %d =>", msa[j].log_capabilities);
      if (msa[j].log_capabilities == 0)
      {
         (void)fprintf(stdout, " None");
      }
      else
      {
         if (msa[j].log_capabilities & AFDD_SYSTEM_LOG)
         {
            (void)fprintf(stdout, " System");
         }
         if (msa[j].log_capabilities & AFDD_EVENT_LOG)
         {
            (void)fprintf(stdout, " Event");
         }
         if (msa[j].log_capabilities & AFDD_RECEIVE_LOG)
         {
            (void)fprintf(stdout, " Receive");
         }
         if (msa[j].log_capabilities & AFDD_TRANSFER_LOG)
         {
            (void)fprintf(stdout, " Transfer");
         }
         if (msa[j].log_capabilities & AFDD_TRANSFER_DEBUG_LOG)
         {
            (void)fprintf(stdout, " Trans_db");
         }
#ifdef _INPUT_LOG
         if (msa[j].log_capabilities & AFDD_INPUT_LOG)
         {
            (void)fprintf(stdout, " Input");
         }
#endif
#ifdef _DISTRIBUTION_LOG
         if (msa[j].log_capabilities & AFDD_DISTRIBUTION_LOG)
         {
            (void)fprintf(stdout, " Distribution");
         }
#endif
#ifdef _PRODUCTION_LOG
         if (msa[j].log_capabilities & AFDD_PRODUCTION_LOG)
         {
            (void)fprintf(stdout, " Production");
         }
#endif
#ifdef _OUTPUT_LOG
         if (msa[j].log_capabilities & AFDD_OUTPUT_LOG)
         {
            (void)fprintf(stdout, " Output");
         }
#endif
#ifdef _DELETE_LOG
         if (msa[j].log_capabilities & AFDD_DELETE_LOG)
         {
            (void)fprintf(stdout, " Delete");
         }
#endif
         if (msa[j].log_capabilities & AFDD_JOB_DATA)
         {
            (void)fprintf(stdout, " Job_data");
         }
         if (msa[j].log_capabilities & AFDD_COMPRESSION_1)
         {
            (void)fprintf(stdout, " Compression1");
         }
      }
      (void)fprintf(stdout, "\n");
      if (msa[j].afd_switching != NO_SWITCHING)
      {
         (void)fprintf(stdout, "Real hostname 0    : %s\n", msa[j].hostname[0]);
         (void)fprintf(stdout, "TCP port 0         : %d\n", msa[j].port[0]);
         (void)fprintf(stdout, "Real hostname 1    : %s\n", msa[j].hostname[1]);
         (void)fprintf(stdout, "TCP port 1         : %d\n", msa[j].port[1]);
         (void)fprintf(stdout, "Current host       : AFD %d\n", msa[j].afd_toggle);
         (void)fprintf(stdout, "Switch type        : %s\n", (msa[j].afd_switching == AUTO_SWITCHING) ? "Auto" : "User");
      }
      else
      {
         (void)fprintf(stdout, "Real hostname      : %s\n", msa[j].hostname[0]);
         (void)fprintf(stdout, "TCP port           : %d\n", msa[j].port[0]);
         (void)fprintf(stdout, "Switch type        : No switching.\n");
      }
      (void)fprintf(stdout, "Poll interval      : %d\n", msa[j].poll_interval);
      (void)fprintf(stdout, "Connect time       : %d\n", msa[j].connect_time);
      (void)fprintf(stdout, "Disconnect time    : %d\n", msa[j].disconnect_time);
      (void)fprintf(stdout, "Status of AMG      : %d\n", (int)msa[j].amg);
      (void)fprintf(stdout, "Status of FD       : %d\n", (int)msa[j].fd);
      (void)fprintf(stdout, "Status of AW       : %d\n", (int)msa[j].archive_watch);
      (void)fprintf(stdout, "Jobs in queue      : %d\n", msa[j].jobs_in_queue);
      (void)fprintf(stdout, "Active transfers   : %d\n", msa[j].no_of_transfers);
      (void)fprintf(stdout, "TOP no. process    : %d", msa[j].top_no_of_transfers[0]);
      for (i = 1; i < STORAGE_TIME; i++)
      {
         (void)fprintf(stdout, " %d", msa[j].top_no_of_transfers[i]);
      }
      (void)fprintf(stdout, "\n");
      (void)fprintf(stdout, "Last TOP no process: %s", ctime(&msa[j].top_not_time));
      (void)fprintf(stdout, "Maximum connections: %d\n", msa[j].max_connections);
      (void)fprintf(stdout, "Sys log EC         : %u  |", msa[j].sys_log_ec);
      for (i = 0; i < LOG_FIFO_SIZE; i++)
      {
         switch (msa[j].sys_log_fifo[i])
         {
            case INFO_ID :
               (void)fprintf(stdout, " I");
               break;

            case ERROR_ID :
               (void)fprintf(stdout, " E");
               break;

            case WARNING_ID :
               (void)fprintf(stdout, " W");
               break;

            case CONFIG_ID :
               (void)fprintf(stdout, " C");
               break;

            case FAULTY_ID :
               (void)fprintf(stdout, " F");
               break;

            default :
               (void)fprintf(stdout, " ?");
               break;
         }
      }
      (void)fprintf(stdout, " |\n");
      (void)fprintf(stdout, "Receive History    :");
      for (i = 0; i < MAX_LOG_HISTORY; i++)
      {
         switch (msa[j].log_history[RECEIVE_HISTORY][i])
         {
            case INFO_ID :
               (void)fprintf(stdout, " I");
               break;

            case ERROR_ID :
               (void)fprintf(stdout, " E");
               break;

            case WARNING_ID :
               (void)fprintf(stdout, " W");
               break;

            case FAULTY_ID :
               (void)fprintf(stdout, " F");
               break;

            default :
               (void)fprintf(stdout, " ?");
               break;
         }
      }
      (void)fprintf(stdout, "\n");
      (void)fprintf(stdout, "System History     :");
      for (i = 0; i < MAX_LOG_HISTORY; i++)
      {
         switch (msa[j].log_history[SYSTEM_HISTORY][i])
         {
            case INFO_ID :
               (void)fprintf(stdout, " I");
               break;

            case ERROR_ID :
               (void)fprintf(stdout, " E");
               break;

            case WARNING_ID :
               (void)fprintf(stdout, " W");
               break;

            case CONFIG_ID :
               (void)fprintf(stdout, " C");
               break;

            case FAULTY_ID :
               (void)fprintf(stdout, " F");
               break;

            default :
               (void)fprintf(stdout, " ?");
               break;
         }
      }
      (void)fprintf(stdout, "\n");
      (void)fprintf(stdout, "Transfer History   :");
      for (i = 0; i < MAX_LOG_HISTORY; i++)
      {
         switch (msa[j].log_history[TRANSFER_HISTORY][i])
         {
            case INFO_ID :
               (void)fprintf(stdout, " I");
               break;

            case ERROR_ID :
               (void)fprintf(stdout, " E");
               break;

            case WARNING_ID :
               (void)fprintf(stdout, " W");
               break;

            case ERROR_OFFLINE_ID :
               (void)fprintf(stdout, " O");
               break;

            case FAULTY_ID :
               (void)fprintf(stdout, " F");
               break;

            default :
               (void)fprintf(stdout, " ?");
               break;
         }
      }
      (void)fprintf(stdout, "\n");
      (void)fprintf(stdout, "Host error counter : %d\n", msa[j].host_error_counter);
      if ((msa[j].host_error_counter > 0) && (msa[j].rcmd[0] != '\0'))
      {
         (void)sprintf(ahl_file, "%s%s%s%s",
                       p_work_dir, FIFO_DIR, AHL_FILE_NAME, msa[j].afd_alias);
         (void)read_file(ahl_file, (char **)&ahl);
         for (i = 0; i < msa[j].no_of_hosts; i++)
         {
            if ((ahl[i].error_history[0] != TRANSFER_SUCCESS) &&
                (ahl[i].error_history[0] != OPEN_FILE_DIR_ERROR) &&
                ((msa[j].ec > 0) || (msa[j].host_error_counter > 0)))
            {
               (void)fprintf(stdout, "Error host(s)      : %-*s [%d] %s\n",
                             MAX_HOSTNAME_LENGTH, ahl[i].host_alias,
                             (int)ahl[i].error_history[0],
                             get_error_str(ahl[i].error_history[0]));
               for (k = 1; k < ERROR_HISTORY_LENGTH && ahl[i].error_history[k] != TRANSFER_SUCCESS; k++)
               {
                  (void)fprintf(stdout, "                     %-*s [%d] %s\n",
                                MAX_HOSTNAME_LENGTH, "",
                                (int)ahl[i].error_history[k],
                                get_error_str(ahl[i].error_history[k]));
               }
            }
         }
         free((char *)ahl);
      }
      (void)fprintf(stdout, "Number of hosts    : %d\n", msa[j].no_of_hosts);
      (void)fprintf(stdout, "Number of dirs     : %d\n", msa[j].no_of_dirs);
      (void)fprintf(stdout, "Number of jobs     : %d\n", msa[j].no_of_jobs);
      (void)fprintf(stdout, "fc                 : %u\n", msa[j].fc);
#if SIZEOF_OFF_T == 4
      (void)fprintf(stdout, "fs                 : %lu\n", msa[j].fs);
      (void)fprintf(stdout, "tr                 : %lu\n", msa[j].tr);
      (void)fprintf(stdout, "TOP tr             : %lu", msa[j].top_tr[0]);
#else
      (void)fprintf(stdout, "fs                 : %llu\n", msa[j].fs);
      (void)fprintf(stdout, "tr                 : %llu\n", msa[j].tr);
      (void)fprintf(stdout, "TOP tr             : %llu", msa[j].top_tr[0]);
#endif
      for (i = 1; i < STORAGE_TIME; i++)
      {
#if SIZEOF_OFF_T == 4
         (void)fprintf(stdout, " %lu", msa[j].top_tr[i]);
#else
         (void)fprintf(stdout, " %llu", msa[j].top_tr[i]);
#endif
      }
      (void)fprintf(stdout, "\n");
      (void)fprintf(stdout, "Last TOP tr time   : %s", ctime(&msa[j].top_tr_time));
      (void)fprintf(stdout, "fr                 : %u\n", msa[j].fr);
      (void)fprintf(stdout, "TOP fr             : %u", msa[j].top_fr[0]);
      for (i = 1; i < STORAGE_TIME; i++)
      {
         (void)fprintf(stdout, " %u", msa[j].top_fr[i]);
      }
      (void)fprintf(stdout, "\n");
      (void)fprintf(stdout, "Last TOP fr time   : %s", ctime(&msa[j].top_fr_time));
      (void)fprintf(stdout, "ec                 : %u\n", msa[j].ec);
      (void)fprintf(stdout, "Last data time     : %s", ctime(&msa[j].last_data_time));
      for (i = 0; i < SUM_STORAGE; i++)
      {
         (void)fprintf(stdout, "                   : --- %s sum values ---\n", sum_stat_type[i]);
         (void)fprintf(stdout, "files_received     : %u\n", msa[j].files_received[i]);
#ifdef NEW_MSA
         (void)fprintf(stdout, "bytes_received     : %.0lf\n", msa[j].bytes_received[i]);
#else
# if SIZEOF_OFF_T == 4
         (void)fprintf(stdout, "bytes_received     : %lu\n", msa[j].bytes_received[i]);
# else
         (void)fprintf(stdout, "bytes_received     : %llu\n", msa[j].bytes_received[i]);
# endif
#endif
         (void)fprintf(stdout, "files_send         : %u\n", msa[j].files_send[i]);
#ifdef NEW_MSA
         (void)fprintf(stdout, "bytes_send         : %.0lf\n", msa[j].bytes_send[i]);
#else
# if SIZEOF_OFF_T == 4
         (void)fprintf(stdout, "bytes_send         : %lu\n", msa[j].bytes_send[i]);
# else
         (void)fprintf(stdout, "bytes_send         : %llu\n", msa[j].bytes_send[i]);
# endif
#endif
         (void)fprintf(stdout, "connections        : %u\n", msa[j].connections[i]);
         (void)fprintf(stdout, "total_errors       : %u\n", msa[j].total_errors[i]);
#ifdef NEW_MSA
         (void)fprintf(stdout, "log_bytes_received : %.0lf\n", msa[j].log_bytes_received[i]);
#else
# if SIZEOF_OFF_T == 4
         (void)fprintf(stdout, "log_bytes_received : %lu\n", msa[j].log_bytes_received[i]);
# else
         (void)fprintf(stdout, "log_bytes_received : %llu\n", msa[j].log_bytes_received[i]);
# endif
#endif
      }
      (void)fprintf(stdout, "                   : ---------------------\n");
      switch (msa[j].connect_status)
      {
         case CONNECTION_ESTABLISHED :
            (void)fprintf(stdout, "Connect status     : CONNECTION_ESTABLISHED\n");
            break;
         case CONNECTION_DEFUNCT :
            (void)fprintf(stdout, "Connect status     : CONNECTION_DEFUNCT\n");
            break;
         case DISCONNECTED :
            (void)fprintf(stdout, "Connect status     : DISCONNECTED\n");
            break;
         case DISABLED : /* This AFD is disabled, ie should not be monitored. */
            (void)fprintf(stdout, "Connect status     : DISABLED\n");
            break;
         default : /* Should not get here. */
            (void)fprintf(stdout, "Connect status     : Unknown\n");
      }
      (void)fprintf(stdout, "Special flag (%3d) :", msa[j].special_flag);
      if (msa[j].special_flag & SUM_VAL_INITIALIZED)
      {
         (void)fprintf(stdout, " SUM_VAL_INITIALIZED");
      }
      (void)fprintf(stdout, "\n");
      if (msa[j].convert_username[0][0][0] != '\0')
      {
         (void)fprintf(stdout, "Convert user name  : %s -> %s\n",
                       msa[j].convert_username[0][0],
                       msa[j].convert_username[0][1]);
         for (i = 1; i < MAX_CONVERT_USERNAME; i++)
         {
            (void)fprintf(stdout, "                   : %s -> %s\n",
                          msa[j].convert_username[i][0],
                          msa[j].convert_username[i][1]);
         }
      }
   }

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/ 
static void
usage(void)
{
   (void)fprintf(stderr,
                 "SYNTAX  : msa_view [--version][-w <working directory>] afdname|position\n");
   return;
}
