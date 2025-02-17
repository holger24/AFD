/*
 *  evaluate_message.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2006 - 2025 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   evaluate_message - evaluates a message send from afdd
 **
 ** SYNOPSIS
 **   int evaluate_message(int *bytes_done)
 **
 ** DESCRIPTION
 **   Evaluates what is returned in msg_str. The first two characters
 **   always describe the content of the message:
 **               IS - Interval summary
 **               NH - Number of hosts
 **               ND - Number of directories
 **               NJ - Number of jobs configured
 **               MC - Maximum number of connections
 **               SR - System Log Radar Information
 **               AM - Status of AMG
 **               FD - Status of FD
 **               AW - Status of archive_watch
 **               WD - remote AFD working directory
 **               AV - AFD version
 **               DJ - Danger number of jobs
 **               TD - Typesize data
 **               HL - List of current host alias names and real
 **                    hostnames
 **               DL - List of current directories
 **               JL - List of current jobs
 **               EL - Error list for a given host
 **               RH - Receive log history
 **               SH - System log history
 **               TH - Transfer log history
 **             Log message types:
 **               LC - Log capabilities
 **
 ** RETURN VALUES
 **   Returns the following values:
 **     - SUCCESS
 **     - AFDD_SHUTTING_DOWN
 **     - any value above 99 which will be the return code of a command
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.10.2006 H.Kiehl Created
 **   21.04.2007 H.Kiehl Handle job ID data.
 **   23.11.2008 H.Kiehl Added DJ (Danger number of jobs).
 **   22.03.2014 H.Kiehl Added TD (Typesize data).
 **   17.02.2025 H.Kiehl For HL try handle group identifier.
 **
 */
DESCR__E_M1

#include <stdio.h>           /* fprintf()                                */
#include <string.h>          /* strcpy(), strcat(), strerror()           */
#include <stdlib.h>          /* strtoul(), atoi(), atol()                */
#include <ctype.h>           /* isdigit()                                */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>           /* O_RDONLY                                 */
#ifdef HAVE_MMAP
# include <sys/mman.h>
#endif
#include <unistd.h>
#include <errno.h>
#include "mondefs.h"
#include "logdefs.h"
#include "afdddefs.h"

/* Global variables. */
extern int                      afd_no,
                                got_log_capabilities,
                                shift_log_his[],
                                timeout_flag;
extern time_t                   new_hour_time;
extern size_t                   adl_size,
                                ahl_size,
                                ajl_size,
                                atd_size;
extern char                     msg_str[],
                                *p_work_dir;
extern struct mon_status_area   *msa;
extern struct afd_dir_list      *adl;
extern struct afd_host_list     *ahl;
extern struct afd_job_list      *ajl;
extern struct afd_typesize_data *atd;

/* Local function prototypes. */
static void                     reshuffel_dir_data(int),
                                reshuffel_job_data(int);


/*######################### evaluate_message() ##########################*/
int
evaluate_message(int *bytes_done)
{
   int  ret;
   char *ptr,
        *ptr_start;

   /* Evaluate what is returned and store in MSA. */
   ptr = msg_str;
   for (;;)
   {
      if (*ptr == '\0')
      {
         break;
      }
      ptr++;
   }
   *bytes_done = ptr - msg_str + 2;
   ptr = msg_str;
   if ((*ptr == 'I') && (*(ptr + 1) == 'S'))
   {
      ptr += 3;
      ptr_start = ptr;
      while ((*ptr != ' ') && (*ptr != '\0'))
      {
         ptr++;
      }
      if (*ptr == ' ')
      {
         /* Store the number of files still to be send. */
         *ptr = '\0';
         msa[afd_no].fc = (unsigned int)strtoul(ptr_start, NULL, 10);
         ptr++;
         ptr_start = ptr;
         while ((*ptr != ' ') && (*ptr != '\0'))
         {
            ptr++;
         }
         if (*ptr == ' ')
         {
            /* Store the number of bytes still to be send. */
            *ptr = '\0';
#ifdef HAVE_STRTOULL
# if SIZEOF_OFF_T == 4
            msa[afd_no].fs = (u_off_t)strtoul(ptr_start, NULL, 10);
# else
            msa[afd_no].fs = (u_off_t)strtoull(ptr_start, NULL, 10);
# endif
#else
            msa[afd_no].fs = (u_off_t)strtoul(ptr_start, NULL, 10);
#endif
            ptr++;
            ptr_start = ptr;
            while ((*ptr != ' ') && (*ptr != '\0'))
            {
               ptr++;
            }
            if (*ptr == ' ')
            {
               /* Store the transfer rate. */
               *ptr = '\0';
#ifdef HAVE_STRTOULL
# if SIZEOF_OFF_T == 4
               msa[afd_no].tr = (u_off_t)strtoul(ptr_start, NULL, 10);
# else
               msa[afd_no].tr = (u_off_t)strtoull(ptr_start, NULL, 10);
# endif
#else
               msa[afd_no].tr = (u_off_t)strtoul(ptr_start, NULL, 10);
#endif
               if (msa[afd_no].tr > msa[afd_no].top_tr[0])
               {
                  msa[afd_no].top_tr[0] = msa[afd_no].tr;
                  msa[afd_no].top_tr_time = msa[afd_no].last_data_time;
               }
               ptr++;
               ptr_start = ptr;
               while ((*ptr != ' ') && (*ptr != '\0'))
               {
                  ptr++;
               }
               if (*ptr == ' ')
               {
                  /* Store file rate. */
                  *ptr = '\0';
                  msa[afd_no].fr = (unsigned int)strtoul(ptr_start, NULL, 10);
                  if (msa[afd_no].fr > msa[afd_no].top_fr[0])
                  {
                     msa[afd_no].top_fr[0] = msa[afd_no].fr;
                     msa[afd_no].top_fr_time = msa[afd_no].last_data_time;
                  }
                  ptr++;
                  ptr_start = ptr;
                  while ((*ptr != ' ') && (*ptr != '\0'))
                  {
                     ptr++;
                  }
                  if (*ptr == ' ')
                  {
                     /* Store error counter. */
                     *ptr = '\0';
                     msa[afd_no].ec = (unsigned int)strtoul(ptr_start, NULL, 10);
                     ptr++;
                     ptr_start = ptr;
                     while ((*ptr != ' ') && (*ptr != '\0'))
                     {
                        ptr++;
                     }
                     if (*ptr == ' ')
                     {
                        /* Store error hosts. */
                        *ptr = '\0';
                        msa[afd_no].host_error_counter = atoi(ptr_start);
                        ptr++;
                        ptr_start = ptr;
                        while ((*ptr != ' ') && (*ptr != '\0'))
                        {
                           ptr++;
                        }
                        if (*ptr == ' ')
                        {
                           /* Store number of transfers. */
                           *ptr = '\0';
                           msa[afd_no].no_of_transfers = atoi(ptr_start);
                           if (msa[afd_no].no_of_transfers > msa[afd_no].top_no_of_transfers[0])
                           {
                              msa[afd_no].top_no_of_transfers[0] = msa[afd_no].no_of_transfers;
                              msa[afd_no].top_not_time = msa[afd_no].last_data_time;
                           }
                           ptr++;
                           ptr_start = ptr;
                           while ((*ptr != ' ') && (*ptr != '\0'))
                           {
                              ptr++;
                           }
                           if (*ptr == '\0')
                           {
                              /* Store number of jobs in queue. */
                              msa[afd_no].jobs_in_queue = atoi(ptr_start);
                           }
                           else if (*ptr == ' ')
                                {
                                   /* Store number of jobs in queue. */
                                   *ptr = '\0';
                                   msa[afd_no].jobs_in_queue = atoi(ptr_start);
                                   ptr++;
                                   ptr_start = ptr;
                                   while ((*ptr != ' ') && (*ptr != '\0'))
                                   {
                                      ptr++;
                                   }
                                   if (*ptr == ' ')
                                   {
                                      /* Store files send. */
                                      *ptr = '\0';
                                      msa[afd_no].files_send[CURRENT_SUM] = (unsigned int)strtoul(ptr_start, NULL, 10);
                                      ptr++;
                                      ptr_start = ptr;
                                      while ((*ptr != ' ') && (*ptr != '\0'))
                                      {
                                         ptr++;
                                      }
                                      if (*ptr == ' ')
                                      {
                                         /* Store bytes send. */
                                         *ptr = '\0';
#ifdef NEW_MSA
                                         msa[afd_no].bytes_send[CURRENT_SUM] = strtod(ptr_start, NULL);
#else
# ifdef HAVE_STRTOULL
#  if SIZEOF_OFF_T == 4
                                         msa[afd_no].bytes_send[CURRENT_SUM] = (u_off_t)strtoul(ptr_start, NULL, 10);
#  else
                                         msa[afd_no].bytes_send[CURRENT_SUM] = (u_off_t)strtoull(ptr_start, NULL, 10);
#  endif
# else
                                         msa[afd_no].bytes_send[CURRENT_SUM] = (u_off_t)strtoul(ptr_start, NULL, 10);
# endif
#endif
                                         ptr++;
                                         ptr_start = ptr;
                                         while ((*ptr != ' ') && (*ptr != '\0'))
                                         {
                                            ptr++;
                                         }
                                         if (*ptr == ' ')
                                         {
                                            /* Store number of connections. */
                                            *ptr = '\0';
                                            msa[afd_no].connections[CURRENT_SUM] = (unsigned int)strtoul(ptr_start, NULL, 10);
                                            ptr++;
                                            ptr_start = ptr;
                                            while ((*ptr != ' ') && (*ptr != '\0'))
                                            {
                                               ptr++;
                                            }
                                            if (*ptr == ' ')
                                            {
                                               /* Store number of total errors. */
                                               *ptr = '\0';
                                               msa[afd_no].total_errors[CURRENT_SUM] = (unsigned int)strtoul(ptr_start, NULL, 10);
                                               ptr++;
                                               ptr_start = ptr;
                                               while ((*ptr != ' ') && (*ptr != '\0'))
                                               {
                                                  ptr++;
                                               }
                                               if (*ptr == ' ')
                                               {
                                                  /* Store files received. */
                                                  *ptr = '\0';
                                                  msa[afd_no].files_received[CURRENT_SUM] = (unsigned int)strtoul(ptr_start, NULL, 10);
                                                  ptr++;
                                                  ptr_start = ptr;
                                                  while ((*ptr != ' ') && (*ptr != '\0'))
                                                  {
                                                     ptr++;
                                                  }
                                                  if ((*ptr == ' ') || (*ptr == '\0'))
                                                  {
                                                     /* Store bytes received. */
                                                     *ptr = '\0';
#ifdef NEW_MSA
                                                     msa[afd_no].bytes_received[CURRENT_SUM] = strtod(ptr_start, NULL);
#else
# ifdef HAVE_STRTOULL
#  if SIZEOF_OFF_T == 4
                                                     msa[afd_no].bytes_received[CURRENT_SUM] = (u_off_t)strtoul(ptr_start, NULL, 10);
#  else
                                                     msa[afd_no].bytes_received[CURRENT_SUM] = (u_off_t)strtoull(ptr_start, NULL, 10);
#  endif
# else
                                                     msa[afd_no].bytes_received[CURRENT_SUM] = (u_off_t)strtoul(ptr_start, NULL, 10);
# endif
#endif
                                                  }
                                                  else
                                                  {
                                                     mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, msg_str,
                                                             "Missed bytes_received.");
                                                  }
                                               }
                                               else
                                               {
                                                  mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, msg_str,
                                                          "Missed files_received.");
                                               }
                                            }
                                            else
                                            {
                                               mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, msg_str,
                                                       "Missed total_errors.");
                                            }
                                         }
                                         else
                                         {
                                            mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, msg_str,
                                                    "Missed connections.");
                                         }
                                      }
                                      else
                                      {
                                         mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, msg_str,
                                                 "Missed bytes_send.");
                                      }
                                   }
                                   else
                                   {
                                      mon_log(DEBUG_SIGN, __FILE__, __LINE__, 0L, msg_str,
                                              "Missed files_send.");
                                   }
                                }
                        }
                     }
                  }
               }
            }
         }
         if ((msa[afd_no].special_flag & SUM_VAL_INITIALIZED) == 0)
         {
            int i;

            for (i = 1; i < SUM_STORAGE; i++)
            {
               msa[afd_no].bytes_send[i]         = msa[afd_no].bytes_send[CURRENT_SUM];
               msa[afd_no].bytes_received[i]     = msa[afd_no].bytes_received[CURRENT_SUM];
               msa[afd_no].files_send[i]         = msa[afd_no].files_send[CURRENT_SUM];
               msa[afd_no].files_received[i]     = msa[afd_no].files_received[CURRENT_SUM];
               msa[afd_no].connections[i]        = msa[afd_no].connections[CURRENT_SUM];
               msa[afd_no].total_errors[i]       = msa[afd_no].total_errors[CURRENT_SUM];
               msa[afd_no].log_bytes_received[i] = msa[afd_no].log_bytes_received[CURRENT_SUM];
            }
            msa[afd_no].special_flag ^= SUM_VAL_INITIALIZED;
         }
         ret = SUCCESS;
#ifdef _DEBUG_PRINT
         (void)fprintf(stderr, "IS %d %d %d %d %d %d %d\n",
                       msa[afd_no].fc, msa[afd_no].fs, msa[afd_no].tr,
                       msa[afd_no].fr, msa[afd_no].ec,
                       msa[afd_no].host_error_counter,
                       msa[afd_no].jobs_in_queue);
#endif
      }
      else
      {
         mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, msg_str,
                 "Failed to evaluate IS message.");
         ret = UNKNOWN_MESSAGE;
      }
   }
   /*
    * Status of AMG (AM)
    */
   else if ((*ptr == 'A') && (*(ptr + 1) == 'M'))
        {
           ptr += 3;
           ptr_start = ptr;
           while (*ptr != '\0')
           {
              ptr++;
           }
           msa[afd_no].amg = (char)atoi(ptr_start);
           ret = SUCCESS;
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "AM %d\n", msa[afd_no].amg);
#endif
        }
   /*
    * Status of FD (FD)
    */
   else if ((*ptr == 'F') && (*(ptr + 1) == 'D'))
        {
           ptr += 3;
           ptr_start = ptr;
           while (*ptr != '\0')
           {
              ptr++;
           }
           msa[afd_no].fd = (char)atoi(ptr_start);
           ret = SUCCESS;
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "FD %d\n", msa[afd_no].fd);
#endif
        }
   /*
    * Status of archive watch (AW)
    */
   else if ((*ptr == 'A') && (*(ptr + 1) == 'W'))
        {
           ptr += 3;
           ptr_start = ptr;
           while (*ptr != '\0')
           {
              ptr++;
           }
           msa[afd_no].archive_watch = (char)atoi(ptr_start);
           ret = SUCCESS;
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "AW %d\n", msa[afd_no].archive_watch);
#endif
        }
   /*
    * Number of hosts (NH)
    */
   else if ((*ptr == 'N') && (*(ptr + 1) == 'H'))
        {
           int  new_no_of_hosts;
           char ahl_file_name[MAX_PATH_LENGTH];

           ptr += 3;
           ptr_start = ptr;
           while (*ptr != '\0')
           {
              ptr++;
           }
           new_no_of_hosts = atoi(ptr_start);
           if ((new_no_of_hosts != msa[afd_no].no_of_hosts) || (ahl == NULL))
           {
              int  fd;
              char *ahl_ptr;

              if (ahl != NULL)
              {
#ifdef HAVE_MMAP
                 if (munmap((void *)ahl, ahl_size) == -1)
#else
                 if (munmap_emu((void *)ahl) == -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "munmap() error : %s", strerror(errno));
                 }
              }
              ahl_size = new_no_of_hosts * sizeof(struct afd_host_list);
              msa[afd_no].no_of_hosts = new_no_of_hosts;
              (void)sprintf(ahl_file_name, "%s%s%s%s",
                            p_work_dir, FIFO_DIR, AHL_FILE_NAME,
                            msa[afd_no].afd_alias);
              if ((ahl_ptr = attach_buf(ahl_file_name, &fd, &ahl_size,
                                        NULL, FILE_MODE, NO)) == (caddr_t) -1)
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to mmap() %s : %s",
                            ahl_file_name, strerror(errno));
                 (void)close(fd);
                 exit(INCORRECT);
              }
              else
              {
                 if (close(fd) == -1)
                 {
                    system_log(DEBUG_SIGN, __FILE__, __LINE__,
                               "close() error : %s", strerror(errno));
                 }
                 ahl = (struct afd_host_list *)ahl_ptr;
              }
           }
           ret = SUCCESS;
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "NH %d\n", msa[afd_no].no_of_hosts);
#endif
        }
   /*
    * Number of directories (ND)
    */
   else if ((*ptr == 'N') && (*(ptr + 1) == 'D'))
        {
           int         new_no_of_dirs;
           char        adl_file_name[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
           struct statx stat_buf;
#else
           struct stat stat_buf;
#endif

           ptr += 3;
           ptr_start = ptr;
           while (*ptr != '\0')
           {
              ptr++;
           }
           new_no_of_dirs = atoi(ptr_start);

           /* Save old adl file, if it exist. */
           (void)sprintf(adl_file_name, "%s%s%s%s",
                         p_work_dir, FIFO_DIR, ADL_FILE_NAME,
                         msa[afd_no].afd_alias);
#ifdef HAVE_STATX
           if (statx(0, adl_file_name, AT_STATX_SYNC_AS_STAT,
                     STATX_MODE | STATX_SIZE | STATX_ATIME | STATX_MTIME,
                     &stat_buf) == 0)
#else
           if (stat(adl_file_name, &stat_buf) == 0)
#endif
           {
              char tmp_adl_file_name[MAX_PATH_LENGTH];

              (void)sprintf(tmp_adl_file_name, "%s%s%s%s",
                            p_work_dir, FIFO_DIR, TMP_ADL_FILE_NAME,
                            msa[afd_no].afd_alias);
              if (copy_file(adl_file_name, tmp_adl_file_name,
                            &stat_buf) == INCORRECT)
              {
                 (void)unlink(tmp_adl_file_name);
              }
           }
           if ((new_no_of_dirs != msa[afd_no].no_of_dirs) || (adl == NULL))
           {
              int  fd;
              char *adl_ptr;

              if (adl != NULL)
              {
#ifdef HAVE_MMAP
                 if (munmap((void *)adl, adl_size) == -1)
#else
                 if (munmap_emu((void *)adl) == -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "munmap() error : %s", strerror(errno));
                 }
              }
              adl_size = new_no_of_dirs * sizeof(struct afd_dir_list);
              msa[afd_no].no_of_dirs = new_no_of_dirs;
              if ((adl_ptr = attach_buf(adl_file_name, &fd, &adl_size,
                                        NULL, FILE_MODE, NO)) == (caddr_t) -1)
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to mmap() %s : %s",
                            adl_file_name, strerror(errno));
                 (void)close(fd);
                 adl = NULL;
              }
              else
              {
                 if (close(fd) == -1)
                 {
                    system_log(DEBUG_SIGN, __FILE__, __LINE__,
                               "close() error : %s", strerror(errno));
                 }
                 adl = (struct afd_dir_list *)adl_ptr;
              }
           }
           ret = SUCCESS;
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "ND %d\n", msa[afd_no].no_of_dirs);
#endif
        }
   /*
    * Number of current job ID's (NJ)
    */
   else if ((*ptr == 'N') && (*(ptr + 1) == 'J'))
        {
           int         new_no_of_job_ids;
           char        ajl_file_name[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
           struct statx stat_buf;
#else
           struct stat stat_buf;
#endif

           ptr += 3;
           ptr_start = ptr;
           while (*ptr != '\0')
           {
              ptr++;
           }
           new_no_of_job_ids = atoi(ptr_start);

           /* Save old adl file, if it exist. */
           (void)sprintf(ajl_file_name, "%s%s%s%s",
                         p_work_dir, FIFO_DIR, AJL_FILE_NAME,
                         msa[afd_no].afd_alias);
#ifdef HAVE_STATX
           if (statx(0, ajl_file_name, AT_STATX_SYNC_AS_STAT,
                     STATX_MODE | STATX_SIZE | STATX_ATIME | STATX_MTIME,
                     &stat_buf) == 0)
#else
           if (stat(ajl_file_name, &stat_buf) == 0)
#endif
           {
              char tmp_ajl_file_name[MAX_PATH_LENGTH];

              (void)sprintf(tmp_ajl_file_name, "%s%s%s%s",
                            p_work_dir, FIFO_DIR, TMP_AJL_FILE_NAME,
                            msa[afd_no].afd_alias);
              if (copy_file(ajl_file_name, tmp_ajl_file_name,
                            &stat_buf) == INCORRECT)
              {
                 (void)unlink(tmp_ajl_file_name);
              }
           }

           if ((new_no_of_job_ids != msa[afd_no].no_of_jobs) || (ajl == NULL))
           {
              int  fd;
              char *ajl_ptr;

              if (ajl != NULL)
              {
#ifdef HAVE_MMAP
                 if (munmap((void *)ajl, ajl_size) == -1)
#else
                 if (munmap_emu((void *)ajl) == -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "munmap() error : %s", strerror(errno));
                 }
              }
              ajl_size = new_no_of_job_ids * sizeof(struct afd_job_list);
              msa[afd_no].no_of_jobs = new_no_of_job_ids;
              if ((ajl_ptr = attach_buf(ajl_file_name, &fd, &ajl_size,
                                        NULL, FILE_MODE, NO)) == (caddr_t) -1)
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to mmap() %s : %s",
                            ajl_file_name, strerror(errno));
                 (void)close(fd);
                 ajl = NULL;
              }
              else
              {
                 if (close(fd) == -1)
                 {
                    system_log(DEBUG_SIGN, __FILE__, __LINE__,
                               "close() error : %s", strerror(errno));
                 }
                 ajl = (struct afd_job_list *)ajl_ptr;
              }
           }
           ret = SUCCESS;
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "NJ %d\n", msa[afd_no].no_of_jobs);
#endif
        }
   /*
    * Maximum number of connections. (MC)
    */
   else if ((*ptr == 'M') && (*(ptr + 1) == 'C'))
        {
           ptr += 3;
           ptr_start = ptr;
           while (*ptr != '\0')
           {
              ptr++;
           }
           msa[afd_no].max_connections = atoi(ptr_start);
           ret = SUCCESS;
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "MC %d\n", msa[afd_no].max_connections);
#endif
        }
   /*
    * System log radar (SR)
    */
   else if ((*ptr == 'S') && (*(ptr + 1) == 'R'))
        {
           ptr += 3;
           ptr_start = ptr;
           while ((*ptr != ' ') && (*ptr != '\0'))
           {
              ptr++;
           }
           if (*ptr == ' ')
           {
              register int i = 0;
#ifdef _DEBUG_PRINT
              int  length = 2;
              char fifo_buf[(LOG_FIFO_SIZE * 4) + 4];

              fifo_buf[0] = 'S';
              fifo_buf[1] = 'R';
#endif

              /* Store system log entry counter. */
              *ptr = '\0';
              msa[afd_no].sys_log_ec = (unsigned int)strtoul(ptr_start, NULL, 10);
              ptr++;
              while ((*ptr != '\0') && (i < LOG_FIFO_SIZE))
              {
                 msa[afd_no].sys_log_fifo[i] = *ptr - ' ';
                 if (msa[afd_no].sys_log_fifo[i] > COLOR_POOL_SIZE)
                 {
                    mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, msg_str,
                            "Reading garbage for Transfer Log Radar entry <%d>",
                            (int)msa[afd_no].sys_log_fifo[i]);
                    msa[afd_no].sys_log_fifo[i] = NO_INFORMATION;
                 }
#ifdef _DEBUG_PRINT
                 length += sprintf(&fifo_buf[length], " %d",
                                   (int)msa[afd_no].sys_log_fifo[i]);
#endif
                 ptr++; i++;
              }
#ifdef _DEBUG_PRINT
              (void)fprintf(stderr, "%s\n", fifo_buf);
#endif
           }
           ret = SUCCESS;
        }
   /*
    * Error list (EL)
    */
   else if ((*ptr == 'E') && (*(ptr + 1) == 'L'))
        {
           /* Syntax: EL <host_number> <error code 1> ... <error code n> */
           ptr += 3;
           ptr_start = ptr;
           while ((*ptr != ' ') && (*ptr != '\0'))
           {
              ptr++;
           }
           if (*ptr == ' ')
           {
              int pos;

              *ptr = '\0';
              pos = atoi(ptr_start);
              if (pos < msa[afd_no].no_of_hosts)
              {
                 int i, k = 0;

                 ptr = ptr + 1;
                 ptr_start = ptr;
                 while ((*ptr != ' ') && (*ptr != '\0'))
                 {
                    ptr++;
                 }
                 if (*ptr == ' ')
                 {
                    k = 1;
                    *ptr = '\0';
                    ahl[pos].error_history[0] = (unsigned char)atoi(ptr_start);

                    do
                    {
                       ptr = ptr + 1;
                       ptr_start = ptr;
                       while ((*ptr != ' ') && (*ptr != '\0'))
                       {
                          ptr++;
                       }
                       if ((*ptr == ' ') && (k < ERROR_HISTORY_LENGTH))
                       {
                          *ptr = '\0';
                          ahl[pos].error_history[k] = (unsigned char)atoi(ptr_start);
                          k++;
                       }
                    } while (*ptr != '\0');
                 }
                 for (i = k; i < ERROR_HISTORY_LENGTH; i++)
                 {
                    ahl[pos].error_history[k] = 0;
                 }
#ifdef _DEBUG_PRINT
                 (void)fprintf(stderr, "EL %d %d",
                               pos, ahl[pos].error_history[0]);
                 for (i = 0; i < k; i++)
                 {
                    (void)fprintf(stderr, " %d",
                                  (int)ahl[pos].error_history[i]);
                 }
                 (void)fprintf(stderr, "\n");
#endif
              }
              else
              {
                 mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, msg_str,
                         "Hmmm. Trying to insert at position %d, but there are only %d hosts.",
                         pos, msa[afd_no].no_of_hosts);
              }
           }
           ret = SUCCESS;
        }
   /*
    * Host list (HL)
    */
   else if ((*ptr == 'H') && (*(ptr + 1) == 'L'))
        {
           /* Syntax: HL <host_number> <host alias> <real hostname 1> [<real hostname 2>] */
           ptr += 3;
           ptr_start = ptr;
           while ((*ptr != ' ') && (*ptr != '\0'))
           {
              ptr++;
           }
           if (*ptr == ' ')
           {
              int pos;

              *ptr = '\0';
              pos = atoi(ptr_start);
              if (pos < msa[afd_no].no_of_hosts)
              {
                 int i = 0;

                 ptr = ptr + 1;
                 while ((*ptr != ' ') && (*ptr != '\0') &&
                        (i < MAX_HOSTNAME_LENGTH))
                 {
                    ahl[pos].host_alias[i] = *ptr;
                    ptr++; i++;
                 }
                 ahl[pos].host_alias[i] = '\0';
                 if (*ptr == ' ')
                 {
                    ahl[pos].host_id = get_str_checksum(ahl[pos].host_alias);
                    i = 0;
                    ptr = ptr + 1;
                    while ((*ptr != ' ') && (*ptr != '\0') &&
                           (i < (MAX_REAL_HOSTNAME_LENGTH - 1)))
                    {
                       ahl[pos].real_hostname[0][i] = *ptr;
                       ptr++; i++;
                    }
                    ahl[pos].real_hostname[0][i] = '\0';
                    if (*ptr == ' ')
                    {
                       i = 0;
                       ptr = ptr + 1;
                       while ((*ptr != ' ') && (*ptr != '\0') &&
                              (i < (MAX_REAL_HOSTNAME_LENGTH - 1)))
                       {
                          ahl[pos].real_hostname[1][i] = *ptr;
                          ptr++; i++;
                       }
                       ahl[pos].real_hostname[1][i] = '\0';
                    }
                    else
                    {
                       ahl[pos].real_hostname[1][0] = '\0';
                    }
                 }
                 else
                 {
                    /* Assume this is a group identifier. */
                    ahl[pos].real_hostname[0][0] = GROUP_IDENTIFIER;
                 }
#ifdef _DEBUG_PRINT
                 (void)fprintf(stderr, "HL %d %s %s %s\n",
                               pos, ahl[pos].host_alias,
                               (ahl[pos].real_hostname[0][0] == GROUP_IDENTIFIER) ? "" : ahl[pos].real_hostname[0],
                               ahl[pos].real_hostname[1]);
#endif
              }
              else
              {
                 mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, msg_str,
                         "Hmmm. Trying to insert host at position %d, but there are only %d hosts.",
                         pos, msa[afd_no].no_of_hosts);
              }
           }
           ret = SUCCESS;
        }
   /*
    * Directory list (DL)
    */
   else if ((*ptr == 'D') && (*(ptr + 1) == 'L'))
        {
           /* Syntax: DL <dir_number> <dir ID> <dir alias> <dir name> [<original dir name>] */

           if (adl != NULL)
           {
              ptr += 3;
              ptr_start = ptr;
              while ((*ptr != ' ') && (*ptr != '\0'))
              {
                 ptr++;
              }
              if (*ptr == ' ')
              {
                 int pos;

                 *ptr = '\0';
                 pos = atoi(ptr_start);
                 if (pos < msa[afd_no].no_of_dirs)
                 {
                    int i = 0;

                    ptr = ptr + 1;
                    ptr_start = ptr;
                    while ((*ptr != ' ') && (*ptr != '\0') &&
                           (i < MAX_INT_LENGTH))
                    {
                       ptr++; i++;
                    }
                    if (*ptr == ' ')
                    {
                       *ptr = '\0';
                       adl[pos].dir_id = (unsigned int)strtoul(ptr_start, NULL, 16);

                       i = 0;
                       ptr = ptr + 1;
                       while ((*ptr != ' ') && (*ptr != '\0') &&
                              (i < MAX_DIR_ALIAS_LENGTH))
                       {
                          adl[pos].dir_alias[i] = *ptr;
                          ptr++; i++;
                       }
                       if (*ptr == ' ')
                       {
                          adl[pos].dir_alias[i] = '\0';
                          i = 0;
                          ptr = ptr + 1;
                          while ((*ptr != ' ') && (*ptr != '\0') &&
                                 (i < (MAX_PATH_LENGTH - 1)))
                          {
                             adl[pos].dir_name[i] = *ptr;
                             ptr++; i++;
                          }
                          adl[pos].dir_name[i] = '\0';
                          if (*ptr == ' ')
                          {
                             i = 0;
                             ptr = ptr + 1;
                             while ((*ptr != ' ') && (*ptr != '\0') &&
                                    (i < (MAX_PATH_LENGTH - 1)))
                             {
                                adl[pos].orig_dir_name[i] = *ptr;
                                ptr++; i++;
                             }
                             adl[pos].orig_dir_name[i] = '\0';
                             if (*ptr == ' ')
                             {
                                i = 0;
                                ptr = ptr + 1;
                                while ((*ptr != ' ') && (*ptr != '\0') &&
                                       (i < (MAX_USER_NAME_LENGTH - 1)))
                                {
                                   adl[pos].home_dir_user[i] = *ptr;
                                   ptr++; i++;
                                }
                                adl[pos].home_dir_user[i] = '\0';
                                if (*ptr == ' ')
                                {
                                   i = 0;
                                   ptr = ptr + 1;
                                   ptr_start = ptr;
                                   while ((*ptr != ' ') && (*ptr != '\0') &&
                                          (i < MAX_INT_LENGTH))
                                   {
                                      ptr++; i++;
                                   }
                                   if (*ptr == ' ')
                                   {
                                      *ptr = '\0';
                                      adl[pos].home_dir_length = (unsigned int)strtoul(ptr_start, NULL, 16);
                                   }
                                }
                                else
                                {
                                   adl[pos].home_dir_length = 0;
                                }
                             }
                             else
                             {
                                adl[pos].home_dir_user[0] = '\0';
                                adl[pos].home_dir_length = 0;
                             }
                          }
                          else
                          {
                             adl[pos].home_dir_user[0] = '\0';
                             adl[pos].home_dir_length = 0;
                             adl[pos].orig_dir_name[0] = '\0';
                          }
                       }
                       else
                       {
                          adl[pos].dir_alias[0] = '\0';
                          adl[pos].home_dir_user[0] = '\0';
                          adl[pos].home_dir_length = 0;
                          adl[pos].orig_dir_name[0] = '\0';
                       }
                    }
                    else
                    {
                       adl[pos].dir_id = 0;
                       adl[pos].dir_alias[0] = '\0';
                       adl[pos].home_dir_user[0] = '\0';
                       adl[pos].home_dir_length = 0;
                       adl[pos].orig_dir_name[0] = '\0';
                    }
                    adl[pos].entry_time = msa[afd_no].last_data_time;
                    if ((pos + 1) == msa[afd_no].no_of_dirs)
                    {
                       reshuffel_dir_data(msa[afd_no].no_of_dirs);
                    }
#ifdef _DEBUG_PRINT
                    if (adl[pos].home_dir_user[0] == '\0')
                    {
                       (void)fprintf(stderr, "DL %d %s %x %s %s\n",
                                     pos, adl[pos].dir_id, adl[pos].dir_alias,
                                     adl[pos].dir_name, adl[pos].orig_dir_name);
                    }
                    else
                    {
                       (void)fprintf(stderr, "DL %d %s %x %s %s %s %x\n",
                                     pos, adl[pos].dir_id, adl[pos].dir_alias,
                                     adl[pos].dir_name, adl[pos].orig_dir_name,
                                     adl[pos].home_dir_user,
                                     adl[pos].home_dir_length);
                    }
#endif
                 }
                 else
                 {
                    mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, msg_str,
                            "Hmmm. Trying to insert directory at position %d, but there are only %d directories.",
                            pos, msa[afd_no].no_of_dirs);
                 }
              }
           }
           ret = SUCCESS;
        }
   /*
    * Job ID list (JL)
    */
#ifdef WITHOUT_BLUR_DATA
   else if ((*ptr == 'J') && (*(ptr + 1) == 'L'))
#else
   else if ((*ptr == 'J') && (*(ptr + 1) == 'l'))
#endif
        {
           /* Syntax: Jl <job_id_number> <job ID> <dir ID>            */
           /*            <no of local options> <priority> <recipient> */
           if (ajl != NULL)
           {
              ptr += 3;
              ptr_start = ptr;
              while ((*ptr != ' ') && (*ptr != '\0'))
              {
                 ptr++;
              }
              if (*ptr == ' ')
              {
                 int pos;

                 *ptr = '\0';
                 pos = atoi(ptr_start);
                 if (pos < msa[afd_no].no_of_jobs)
                 {
                    int i = 0;

                    ptr = ptr + 1;
                    ptr_start = ptr;
                    while ((*ptr != ' ') && (*ptr != '\0') &&
                           (i < MAX_INT_LENGTH))
                    {
                       ptr++; i++;
                    }
                    if (*ptr == ' ')
                    {
                       *ptr = '\0';
                       ajl[pos].job_id = (unsigned int)strtoul(ptr_start, NULL, 16);

                       i = 0;
                       ptr = ptr + 1;
                       ptr_start = ptr;
                       while ((*ptr != ' ') && (*ptr != '\0') &&
                              (i < MAX_INT_LENGTH))
                       {
                          ptr++; i++;
                       }
                       if (*ptr == ' ')
                       {
                          *ptr = '\0';
                          ajl[pos].dir_id = (unsigned int)strtoul(ptr_start, NULL, 16);

                          i = 0;
                          ptr = ptr + 1;
                          ptr_start = ptr;
                          while ((*ptr != ' ') && (*ptr != '\0') &&
                                 (i < MAX_INT_LENGTH))
                          {
                             ptr++; i++;
                          }
                          if (*ptr == ' ')
                          {
                             *ptr = '\0';
                             ajl[pos].no_of_loptions = (int)strtol(ptr_start, NULL, 16);

                             i = 0;
                             ptr = ptr + 1;
                             if ((isxdigit((int)(*ptr))) &&
                                 (*(ptr + 1) == ' '))
                             {
#ifndef WITHOUT_BLUR_DATA
                                int offset = 0;
#endif

                                ajl[pos].priority = *ptr;

                                i = 0;
                                ptr = ptr + 2;
                                while ((*ptr != '\0') &&
                                       (i < (MAX_RECIPIENT_LENGTH - 1)))
                                {
#ifdef WITHOUT_BLUR_DATA
                                   ajl[pos].recipient[i] = *ptr;
#else
                                   if ((i - offset) > 28)
                                   {
                                      offset += 28;
                                   }
                                   if (((i - offset) % 3) == 0)
                                   {
                                      ajl[pos].recipient[i] = (unsigned char)*ptr + 9 - (i - offset);
                                   }
                                   else
                                   {
                                      ajl[pos].recipient[i] = (unsigned char)*ptr + 17 - (i - offset);
                                   }
#endif
                                   ptr++; i++;
                                }
                                ajl[pos].recipient[i] = '\0';
                                if (i == MAX_RECIPIENT_LENGTH)
                                {
                                   system_log(WARN_SIGN, __FILE__, __LINE__,
                                              "Recipient overflow!!!");
                                }
                             }
                             else
                             {
                                ajl[pos].recipient[0] = '\0';
                                ajl[pos].priority = '9';
                             }
                          }
                          else
                          {
                             ajl[pos].recipient[0] = '\0';
                             ajl[pos].priority = '9';
                             ajl[pos].no_of_loptions = -1;
                          }
                       }
                       else
                       {
                          ajl[pos].recipient[0] = '\0';
                          ajl[pos].priority = '9';
                          ajl[pos].no_of_loptions = -1;
                          ajl[pos].dir_id = 0;
                       }
                    }
                    else
                    {
                       ajl[pos].recipient[0] = '\0';
                       ajl[pos].priority = '9';
                       ajl[pos].no_of_loptions = -1;
                       ajl[pos].dir_id = 0;
                       ajl[pos].job_id = 0;
                    }
                    ajl[pos].entry_time = msa[afd_no].last_data_time;
                    if ((pos + 1) == msa[afd_no].no_of_jobs)
                    {
                       reshuffel_job_data(msa[afd_no].no_of_jobs);
                    }
#ifdef _DEBUG_PRINT
# ifdef WITHOUT_BLUR_DATA
                    (void)fprintf(stderr, "JL %d %x %x %x %c %s\n",
# else
                    (void)fprintf(stderr, "Jl %d %x %x %x %c %s\n",
# endif
                                  pos, ajl[pos].job_id, ajl[pos].dir_id,
                                  ajl[pos].no_of_loptions, ajl[pos].priority,
                                  ajl[pos].recipient);
#endif
                 }
                 else
                 {
                    mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, msg_str,
                            "Hmmm. Trying to insert job ID at position %d, but there are only %d jobs.",
                            pos, msa[afd_no].no_of_jobs);
                 }
              }
           }
           ret = SUCCESS;
        }
   /*
    * Receive Log History (RH)
    */
   else if ((*ptr == 'R') && (*(ptr + 1) == 'H'))
        {
           register int i,
                        his_length_received;
#ifdef _DEBUG_PRINT
           int  length = 2;
           char fifo_buf[(MAX_LOG_HISTORY * 4) + 4];

           fifo_buf[0] = 'R';
           fifo_buf[1] = 'H';
#endif
           his_length_received = *bytes_done - 2 - 3;
           if (his_length_received > MAX_LOG_HISTORY)
           {
              his_length_received = MAX_LOG_HISTORY;
           }
           else if ((his_length_received < MAX_LOG_HISTORY) &&
                    (shift_log_his[RECEIVE_HISTORY] == NO) &&
                    (msa[afd_no].last_data_time >= new_hour_time))
                {
                   (void)memmove(msa[afd_no].log_history[RECEIVE_HISTORY],
                                 &msa[afd_no].log_history[RECEIVE_HISTORY][1],
                                 (MAX_LOG_HISTORY - his_length_received));
                   shift_log_his[RECEIVE_HISTORY] = DONE;
                }

           /* Syntax: RH <history data> */;
           ptr += 3;
           for (i = (MAX_LOG_HISTORY - his_length_received); i < MAX_LOG_HISTORY; i++)
           {
              msa[afd_no].log_history[RECEIVE_HISTORY][i] = *ptr - ' ';
              if (msa[afd_no].log_history[RECEIVE_HISTORY][i] > COLOR_POOL_SIZE)
              {
                 mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, msg_str,
                         "Reading garbage for Receive Log History <%d>",
                         (int)msa[afd_no].log_history[RECEIVE_HISTORY][i]);
                 msa[afd_no].log_history[RECEIVE_HISTORY][i] = NO_INFORMATION;
              }
              ptr++;
#ifdef _DEBUG_PRINT
              length += sprintf(&fifo_buf[length], " %d",
                                (int)msa[afd_no].log_history[RECEIVE_HISTORY][i]);
#endif
           }
           ret = SUCCESS;
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "%s\n", fifo_buf);
#endif
        }
   /*
    * Transfer Log History (TH)
    */
   else if ((*ptr == 'T') && (*(ptr + 1) == 'H'))
        {
           register int i,
                        his_length_received;
#ifdef _DEBUG_PRINT
           int  length = 2;
           char fifo_buf[(MAX_LOG_HISTORY * 4) + 4];

           fifo_buf[0] = 'T';
           fifo_buf[1] = 'H';
#endif
           his_length_received = *bytes_done - 2 - 3;
           if (his_length_received > MAX_LOG_HISTORY)
           {
              his_length_received = MAX_LOG_HISTORY;
           }
           else if ((his_length_received < MAX_LOG_HISTORY) &&
                    (shift_log_his[TRANSFER_HISTORY] == NO) &&
                    (msa[afd_no].last_data_time >= new_hour_time))
                {
                   (void)memmove(msa[afd_no].log_history[TRANSFER_HISTORY],
                                 &msa[afd_no].log_history[TRANSFER_HISTORY][1],
                                 (MAX_LOG_HISTORY - his_length_received));
                   shift_log_his[TRANSFER_HISTORY] = DONE;
                }

           /* Syntax: TH <history data> */;
           ptr += 3;
           for (i = (MAX_LOG_HISTORY - his_length_received); i < MAX_LOG_HISTORY; i++)
           {
              msa[afd_no].log_history[TRANSFER_HISTORY][i] = *ptr - ' ';
              if (msa[afd_no].log_history[TRANSFER_HISTORY][i] > COLOR_POOL_SIZE)
              {
                 mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, msg_str,
                         "Reading garbage for Transfer Log History <%d>",
                         (int)msa[afd_no].log_history[TRANSFER_HISTORY][i]);
                 msa[afd_no].log_history[TRANSFER_HISTORY][i] = NO_INFORMATION;
              }
              ptr++;
#ifdef _DEBUG_PRINT
              length += sprintf(&fifo_buf[length], " %d",
                                (int)msa[afd_no].log_history[TRANSFER_HISTORY][i]);
#endif
           }
           ret = SUCCESS;
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "%s\n", fifo_buf);
#endif
        }
   /*
    * System Log History (SH)
    */
   else if ((*ptr == 'S') && (*(ptr + 1) == 'H'))
        {
           register int i,
                        his_length_received;
#ifdef _DEBUG_PRINT
           int  length = 2;
           char fifo_buf[(MAX_LOG_HISTORY * 4) + 4];

           fifo_buf[0] = 'S';
           fifo_buf[1] = 'H';
#endif
           his_length_received = *bytes_done - 2 - 3;
           if (his_length_received > MAX_LOG_HISTORY)
           {
              his_length_received = MAX_LOG_HISTORY;
           }
           else if ((his_length_received < MAX_LOG_HISTORY) &&
                    (shift_log_his[SYSTEM_HISTORY] == NO) &&
                    (msa[afd_no].last_data_time >= new_hour_time))
                {
                   (void)memmove(msa[afd_no].log_history[SYSTEM_HISTORY],
                                 &msa[afd_no].log_history[SYSTEM_HISTORY][1],
                                 (MAX_LOG_HISTORY - his_length_received));
                   shift_log_his[SYSTEM_HISTORY] = DONE;
                }

           /* Syntax: SH <history data> */;
           ptr += 3;
           for (i = (MAX_LOG_HISTORY - his_length_received); i < MAX_LOG_HISTORY; i++)
           {
              msa[afd_no].log_history[SYSTEM_HISTORY][i] = *ptr - ' ';
              if (msa[afd_no].log_history[SYSTEM_HISTORY][i] > COLOR_POOL_SIZE)
              {
                 mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, msg_str,
                         "Reading garbage for System Log History <%d>",
                         (int)msa[afd_no].log_history[SYSTEM_HISTORY][i]);
                 msa[afd_no].log_history[SYSTEM_HISTORY][i] = NO_INFORMATION;
              }
              ptr++;
#ifdef _DEBUG_PRINT
              length += sprintf(&fifo_buf[length], " %d",
                                (int)msa[afd_no].log_history[SYSTEM_HISTORY][i]);
#endif
           }
           ret = SUCCESS;
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "%s\n", fifo_buf);
#endif
        }
   /*
    * Log capabilities of remote AFD. (LC)
    */
   else if ((*ptr == 'L') && (*(ptr + 1) == 'C'))
        {
           if ((*bytes_done - 3) < MAX_INT_LENGTH)
           {
              msa[afd_no].log_capabilities = atoi(ptr + 3);
              got_log_capabilities = YES;
#ifdef _DEBUG_PRINT
              (void)fprintf(stderr, "LC %d\n", msa[afd_no].log_capabilities);
#endif
           }
           else
           {
              mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, msg_str,
                      "Log capabilities is %d bytes long, but can handle only %d bytes.",
                      *bytes_done - 3, MAX_INT_LENGTH);
           }
           ret = SUCCESS;
        }
   /*
    * AFD Version Number (AV)
    */
   else if ((*ptr == 'A') && (*(ptr + 1) == 'V'))
        {
           if ((*bytes_done - 3) < MAX_VERSION_LENGTH)
           {
              (void)strcpy(msa[afd_no].afd_version, ptr + 3);
#ifdef _DEBUG_PRINT
              (void)fprintf(stderr, "AV %s\n", msa[afd_no].afd_version);
#endif
           }
           else
           {
              mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, msg_str,
                      "Version is %d Bytes long, but can handle only %d Bytes.",
                      *bytes_done - 3, MAX_VERSION_LENGTH);
           }
           ret = SUCCESS;
        }
   /*
    * Danger number of Jobs (DJ)
    */
   else if ((*ptr == 'D') && (*(ptr + 1) == 'J'))
        {
           if ((*bytes_done - 3) < MAX_INT_LENGTH)
           {
              msa[afd_no].danger_no_of_jobs = atol(ptr + 3);
#ifdef _DEBUG_PRINT
              (void)fprintf(stderr, "DJ %ld\n", msa[afd_no].danger_no_of_jobs);
#endif
           }
           else
           {
              mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, msg_str,
                      "Danger number of Jobs is %d Bytes long, but can handle only %d Bytes.",
                      *bytes_done - 3, MAX_INT_LENGTH);
           }
           ret = SUCCESS;
        }
   /*
    * Typesize data (TD)
    */
   else if ((*ptr == 'T') && (*(ptr + 1) == 'D'))
        {
           int  fd;
           char atd_file_name[MAX_PATH_LENGTH],
                *atd_ptr;

           (void)sprintf(atd_file_name, "%s%s%s%s",
                         p_work_dir, FIFO_DIR, ATD_FILE_NAME,
                         msa[afd_no].afd_alias);
           if (atd != NULL)
           {
#ifdef HAVE_MMAP
              if (munmap((void *)atd, atd_size) == -1)
#else
              if (munmap_emu((void *)atd) == -1)
#endif
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "munmap() error : %s", strerror(errno));
              }
           }
           atd_size = AFD_TYPESIZE_ELEMENTS * sizeof(int);
           if ((atd_ptr = attach_buf(atd_file_name, &fd, &atd_size,
                                     NULL, FILE_MODE, NO)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap() %s : %s",
                         atd_file_name, strerror(errno));
              (void)close(fd);
              atd = NULL;
           }
           else
           {
              int i,
                  no_of_elements = 0;

              if (close(fd) == -1)
              {
                 system_log(DEBUG_SIGN, __FILE__, __LINE__,
                            "close() error : %s", strerror(errno));
              }
              atd = (struct afd_typesize_data *)atd_ptr;

              ptr += 3;
              do
              {
                 i = 0;
                 ptr_start = ptr;
                 while ((*ptr != ' ') && (*ptr != '\0') && (i < MAX_INT_LENGTH))
                 {
                    ptr++; i++;
                 }
                 if ((*ptr == ' ') || (*ptr == '\0'))
                 {
                    if (*ptr == ' ')
                    {
                       *ptr = '\0';
                       ptr++;
                    }
                    atd->val[no_of_elements] = atoi(ptr_start);
                 }
                 else
                 {
                    while ((*ptr != ' ') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    atd->val[no_of_elements] = -1;

                    /*
                     * Lets silently ignore this error case.
                     */
                 }
                 no_of_elements++;
              } while ((*ptr != '\0') &&
                       (no_of_elements < AFD_TYPESIZE_ELEMENTS));
           }
           ret = SUCCESS;
        }
   /*
    * Remote AFD working directory. (WD)
    */
   else if ((*ptr == 'W') && (*(ptr + 1) == 'D'))
        {
           if ((*bytes_done - 3) < MAX_PATH_LENGTH)
           {
              (void)strcpy(msa[afd_no].r_work_dir, ptr + 3);
#ifdef _DEBUG_PRINT
              (void)fprintf(stderr, "WD %s\n", msa[afd_no].r_work_dir);
#endif
           }
           else
           {
              mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, msg_str,
                      "Path is %d Bytes long, but can handle only %d Bytes.",
                      *bytes_done - 3, MAX_PATH_LENGTH);
           }
           ret = SUCCESS;
        }
   else if ((isdigit((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))) &&
            (isdigit((int)(*(ptr + 2)))) && (*(ptr + 3) == '-'))
        {
           ret = ((*ptr - '0') * 100) + ((*(ptr + 1) - '0') * 10) +
                 (*(ptr + 2) - '0');
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "%s\n", ptr);
#endif
        }
   else if (my_strcmp(ptr, AFDD_SHUTDOWN_MESSAGE) == 0)
        {
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "%s\n", ptr);
#endif
           mon_log(WARN_SIGN, NULL, 0, 0L, NULL,
                   "========> AFDD SHUTDOWN <========");
           ret = AFDD_SHUTTING_DOWN;
           timeout_flag = ON;
           (void)tcp_quit();
           timeout_flag = OFF;
           msa[afd_no].connect_status = DISCONNECTED;
        }
        else
        {
           mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, msg_str,
                   "Failed to evaluate message.");
           ret = UNKNOWN_MESSAGE;
        }

   return(ret);
}


/*++++++++++++++++++++++++ reshuffel_dir_data() +++++++++++++++++++++++++*/
static void
reshuffel_dir_data(int no_of_dirs)
{
   int    oadl_fd;
   size_t oadl_size;
   char   *ptr,
          tmp_adl_file_name[MAX_PATH_LENGTH];

   (void)sprintf(tmp_adl_file_name, "%s%s%s%s",
                 p_work_dir, FIFO_DIR, OLD_ADL_FILE_NAME,
                 msa[afd_no].afd_alias);
   oadl_size = AFD_WORD_OFFSET + (DATA_STEP_SIZE * sizeof(struct afd_dir_list));
   if ((ptr = attach_buf(tmp_adl_file_name, &oadl_fd, &oadl_size, NULL,
                         FILE_MODE, NO)) == (caddr_t) -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to mmap() %s : %s",
                 tmp_adl_file_name, strerror(errno));
      (void)close(oadl_fd);
      (void)sprintf(tmp_adl_file_name, "%s%s%s%s",
                    p_work_dir, FIFO_DIR, TMP_ADL_FILE_NAME,
                    msa[afd_no].afd_alias);
   }
   else
   {
#ifdef HAVE_STATX
      struct statx        stat_buf;
#else
      struct stat         stat_buf;
#endif
      struct afd_dir_list *oadl;

      (void)sprintf(tmp_adl_file_name, "%s%s%s%s",
                    p_work_dir, FIFO_DIR, TMP_ADL_FILE_NAME,
                    msa[afd_no].afd_alias);
#ifdef HAVE_STATX
      if ((statx(0, tmp_adl_file_name, AT_STATX_SYNC_AS_STAT,
                 STATX_SIZE, &stat_buf) == 0) &&
          (stat_buf.stx_size > 0))
#else
      if ((stat(tmp_adl_file_name, &stat_buf) == 0) &&
          (stat_buf.st_size > 0))
#endif
      {
         int  fd,
              tmp_no_of_dirs;
         char *ptr2;

#ifdef HAVE_STATX
         tmp_no_of_dirs = stat_buf.stx_size / sizeof(struct afd_dir_list);
#else
         tmp_no_of_dirs = stat_buf.st_size / sizeof(struct afd_dir_list);
#endif
         if ((ptr2 = map_file(tmp_adl_file_name, &fd, NULL, &stat_buf,
                              O_RDONLY)) == (caddr_t)-1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to map_file() `%s' : %s",
                       tmp_adl_file_name, strerror(errno));
            (void)close(fd);
            oadl = NULL;
         }
         else
         {
            int                 end_pos,
                                gotcha,
                                i, j,
                                no_added,
                                no_deleted,
                                old_no_of_dirs;
            time_t              offset_time;
            size_t              work_size;
            struct afd_dir_list *tadl;

            if (close(fd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "close() error : %s", strerror(errno));
            }
            tadl = (struct afd_dir_list *)ptr2;
            old_no_of_dirs = *(int *)ptr;
            ptr += AFD_WORD_OFFSET;
            oadl = (struct afd_dir_list *)ptr;
            get_max_log_values(&gotcha, MAX_ADL_FILES_DEF, MAX_ADL_FILES,
                               NULL, NULL, 0, MON_CONFIG_FILE);
            offset_time = gotcha * SWITCH_FILE_TIME;
            no_added = no_deleted = 0;
            for (i = 0; i < old_no_of_dirs; i++)
            {
               if ((oadl[i].entry_time + offset_time) < msa[afd_no].last_data_time)
               {
                  end_pos = i;
                  while ((++end_pos <= old_no_of_dirs) &&
                         ((oadl[i].entry_time + offset_time) < msa[afd_no].last_data_time))
                  {
                     /* Nothing to be done. */;
                  }
                  if (end_pos <= old_no_of_dirs)
                  {
                     work_size = (old_no_of_dirs - (end_pos - 1)) *
                                 sizeof(struct afd_dir_list);
                     (void)memmove(&oadl[i], &oadl[end_pos - 1], work_size);
                  }
                  gotcha = end_pos - 1 - i;
                  old_no_of_dirs -= gotcha;
                  no_deleted += gotcha;
               }
            }
            for (i = 0; i < tmp_no_of_dirs; i++)
            {
               gotcha = NO;
               for (j = 0; j < no_of_dirs; j++)
               {
                  if (tadl[i].dir_id == adl[j].dir_id)
                  {
                     gotcha = YES;
                     j = no_of_dirs;
                  }
               }
               if (gotcha == NO)
               {
                  if ((no_added > no_deleted) &&
                      ((old_no_of_dirs % DATA_STEP_SIZE) == 0))
                  {
                     work_size = (((old_no_of_dirs / DATA_STEP_SIZE) + 1) *
                                  DATA_STEP_SIZE * sizeof(struct afd_dir_list)) +
                                 AFD_WORD_OFFSET;
                     ptr = (char *)oadl - AFD_WORD_OFFSET;
                     if ((ptr = mmap_resize(oadl_fd, ptr,
                                            work_size)) == (caddr_t) -1)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "mmap_resize() error : %s", strerror(errno));
                        (void)close(oadl_fd);
                        oadl = NULL;
                        break;
                     }
                     else
                     {
                        old_no_of_dirs = *(int *)ptr;
                        ptr += AFD_WORD_OFFSET;
                        oadl = (struct afd_dir_list *)ptr;
                     }
                  }
                  (void)memcpy(&oadl[old_no_of_dirs], &tadl[i],
                               sizeof(struct afd_dir_list));
                  old_no_of_dirs++;
                  no_added++;
               }
            } /* for (i = 0; i < tmp_no_of_dirs; i++) */

#ifdef HAVE_MMAP
# ifdef HAVE_STATX
            if (munmap((void *)tadl, stat_buf.stx_size) == -1)
# else
            if (munmap((void *)tadl, stat_buf.st_size) == -1)
# endif
#else
            if (munmap_emu((void *)tadl) == -1)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "munmap() error : %s", strerror(errno));
            }

            /* Check if we need to resize file. */
            if (no_deleted > no_added)
            {
               work_size = (((old_no_of_dirs / DATA_STEP_SIZE) + 1) *
                            DATA_STEP_SIZE * sizeof(struct afd_dir_list)) +
                           AFD_WORD_OFFSET;
               ptr = (char *)oadl - AFD_WORD_OFFSET;
               if ((ptr = mmap_resize(oadl_fd, ptr,
                                      work_size)) == (caddr_t) -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "mmap_resize() error : %s", strerror(errno));
                  (void)close(oadl_fd);
                  oadl = NULL;
               }
               else
               {
                  old_no_of_dirs = *(int *)ptr;
                  ptr += AFD_WORD_OFFSET;
                  oadl = (struct afd_dir_list *)ptr;
               }
            }
         }
      }
      else
      {
         oadl = NULL;
      }

      if ((unlink(tmp_adl_file_name) == -1) && (errno != ENOENT))
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to unlink() `%s' : %s",
                    tmp_adl_file_name, strerror(errno));
      }

      if (oadl != NULL)
      {
         ptr = (char *)oadl - AFD_WORD_OFFSET;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
         if (statx(oadl_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                   STATX_SIZE, &stat_buf) == -1)
# else
         if (fstat(oadl_fd, &stat_buf) == -1)
# endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
# ifdef HAVE_STATX
                       "statx() error : %s",
# else
                       "fstat() error : %s",
# endif
                       strerror(errno));
         }
         else
         {
# ifdef HAVE_STATX
            if (munmap((void *)ptr, stat_buf.stx_size) == -1)
# else
            if (munmap((void *)ptr, stat_buf.st_size) == -1)
# endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "munmap() error : %s", strerror(errno));
            }
         }
#else
         if (munmap_emu((void *)ptr) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "munmap_emu() error : %s", strerror(errno));
         }
#endif
         if (close(oadl_fd) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "close() error : %s", strerror(errno));
         }
      }
   }

#ifdef HAVE_MMAP
   if (munmap((void *)adl, adl_size) == -1)
#else
   if (munmap_emu((void *)adl) == -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "munmap() error : %s", strerror(errno));
   }
   adl = NULL;

   return;
}


/*++++++++++++++++++++++++ reshuffel_job_data() +++++++++++++++++++++++++*/
static void
reshuffel_job_data(int no_of_job_ids)
{
   int    oajl_fd;
   size_t oajl_size;
   char   *ptr,
          tmp_ajl_file_name[MAX_PATH_LENGTH];

   (void)sprintf(tmp_ajl_file_name, "%s%s%s%s",
                 p_work_dir, FIFO_DIR, OLD_AJL_FILE_NAME,
                 msa[afd_no].afd_alias);
   oajl_size = AFD_WORD_OFFSET + (DATA_STEP_SIZE * sizeof(struct afd_job_list));
   if ((ptr = attach_buf(tmp_ajl_file_name, &oajl_fd, &oajl_size, NULL,
                         FILE_MODE, NO)) == (caddr_t) -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to mmap() %s : %s",
                 tmp_ajl_file_name, strerror(errno));
      (void)close(oajl_fd);
      (void)sprintf(tmp_ajl_file_name, "%s%s%s%s",
                    p_work_dir, FIFO_DIR, TMP_AJL_FILE_NAME,
                    msa[afd_no].afd_alias);
   }
   else
   {
#ifdef HAVE_STATX
      struct statx        stat_buf;
#else
      struct stat         stat_buf;
#endif
      struct afd_job_list *oajl;

      (void)sprintf(tmp_ajl_file_name, "%s%s%s%s",
                    p_work_dir, FIFO_DIR, TMP_AJL_FILE_NAME,
                    msa[afd_no].afd_alias);
#ifdef HAVE_STATX
      if ((statx(0, tmp_ajl_file_name, AT_STATX_SYNC_AS_STAT,
                 STATX_SIZE, &stat_buf) == 0) &&
          (stat_buf.stx_size > 0))
#else
      if ((stat(tmp_ajl_file_name, &stat_buf) == 0) &&
          (stat_buf.st_size > 0))
#endif
      {
         int  fd,
              tmp_no_of_job_ids;
         char *ptr2;

#ifdef HAVE_STATX
         tmp_no_of_job_ids = stat_buf.stx_size / sizeof(struct afd_job_list);
#else
         tmp_no_of_job_ids = stat_buf.st_size / sizeof(struct afd_job_list);
#endif
         if ((ptr2 = map_file(tmp_ajl_file_name, &fd, NULL, &stat_buf,
                              O_RDONLY)) == (caddr_t)-1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to map_file() `%s' : %s",
                       tmp_ajl_file_name, strerror(errno));
            (void)close(fd);
            oajl = NULL;
         }
         else
         {
            int                 end_pos,
                                gotcha,
                                i, j,
                                no_added,
                                no_deleted,
                                old_no_of_job_ids;
            time_t              offset_time;
            size_t              work_size;
            struct afd_job_list *tajl;

            if (close(fd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "close() error : %s", strerror(errno));
            }
            tajl = (struct afd_job_list *)ptr2;
            old_no_of_job_ids = *(int *)ptr;
            ptr += AFD_WORD_OFFSET;
            oajl = (struct afd_job_list *)ptr;
            get_max_log_values(&gotcha, MAX_AJL_FILES_DEF, MAX_AJL_FILES,
                               NULL, NULL, 0, MON_CONFIG_FILE);
            offset_time = gotcha * SWITCH_FILE_TIME;
            no_added = no_deleted = 0;
            for (i = 0; i < old_no_of_job_ids; i++)
            {
               if ((oajl[i].entry_time + offset_time) < msa[afd_no].last_data_time)
               {
                  end_pos = i;
                  while ((++end_pos <= old_no_of_job_ids) &&
                         ((oajl[i].entry_time + offset_time) < msa[afd_no].last_data_time))
                  {
                     /* Nothing to be done. */;
                  }
                  if (end_pos <= old_no_of_job_ids)
                  {
                     work_size = (old_no_of_job_ids - (end_pos - 1)) *
                                 sizeof(struct afd_job_list);
                     (void)memmove(&oajl[i], &oajl[end_pos - 1], work_size);
                  }
                  gotcha = end_pos - 1 - i;
                  old_no_of_job_ids -= gotcha;
                  no_deleted += gotcha;
               }
            }
            for (i = 0; i < tmp_no_of_job_ids; i++)
            {
               gotcha = NO;
               for (j = 0; j < no_of_job_ids; j++)
               {
                  if (tajl[i].job_id == ajl[j].job_id)
                  {
                     gotcha = YES;
                     j = no_of_job_ids;
                  }
               }
               if (gotcha == NO)
               {
                  if ((no_added > no_deleted) &&
                      ((old_no_of_job_ids % DATA_STEP_SIZE) == 0))
                  {
                     work_size = (((old_no_of_job_ids / DATA_STEP_SIZE) + 1) *
                                  DATA_STEP_SIZE * sizeof(struct afd_job_list)) +
                                 AFD_WORD_OFFSET;
                     ptr = (char *)oajl - AFD_WORD_OFFSET;
                     if ((ptr = mmap_resize(oajl_fd, ptr,
                                            work_size)) == (caddr_t) -1)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "mmap_resize() error : %s", strerror(errno));
                        (void)close(oajl_fd);
                        oajl = NULL;
                        break;
                     }
                     else
                     {
                        old_no_of_job_ids = *(int *)ptr;
                        ptr += AFD_WORD_OFFSET;
                        oajl = (struct afd_job_list *)ptr;
                     }
                  }
                  (void)memcpy(&oajl[old_no_of_job_ids], &tajl[i],
                               sizeof(struct afd_job_list));
                  old_no_of_job_ids++;
                  no_added++;
               }
            } /* for (i = 0; i < tmp_no_of_job_ids; i++) */

#ifdef HAVE_MMAP
# ifdef HAVE_STATX
            if (munmap((void *)tajl, stat_buf.stx_size) == -1)
# else
            if (munmap((void *)tajl, stat_buf.st_size) == -1)
# endif
#else
            if (munmap_emu((void *)tajl) == -1)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "munmap() error : %s", strerror(errno));
            }

            /* Check if we need to resize file. */
            if (no_deleted > no_added)
            {
               work_size = (((old_no_of_job_ids / DATA_STEP_SIZE) + 1) *
                            DATA_STEP_SIZE * sizeof(struct afd_job_list)) +
                           AFD_WORD_OFFSET;
               ptr = (char *)oajl - AFD_WORD_OFFSET;
               if ((ptr = mmap_resize(oajl_fd, ptr,
                                      work_size)) == (caddr_t) -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "mmap_resize() error : %s", strerror(errno));
                  (void)close(oajl_fd);
                  oajl = NULL;
               }
               else
               {
                  old_no_of_job_ids = *(int *)ptr;
                  ptr += AFD_WORD_OFFSET;
                  oajl = (struct afd_job_list *)ptr;
               }
            }
         }
      }
      else
      {
         oajl = NULL;
      }

      if ((unlink(tmp_ajl_file_name) == -1) && (errno != ENOENT))
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to unlink() `%s' : %s",
                    tmp_ajl_file_name, strerror(errno));
      }

      if (oajl != NULL)
      {
         ptr = (char *)oajl - AFD_WORD_OFFSET;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
         if (statx(oajl_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                   STATX_SIZE, &stat_buf) == -1)
# else
         if (fstat(oajl_fd, &stat_buf) == -1)
# endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
# ifdef HAVE_STATX
                       "statx() error : %s",
# else
                       "fstat() error : %s",
# endif
                       strerror(errno));
         }
         else
         {
# ifdef HAVE_STATX
            if (munmap((void *)ptr, stat_buf.stx_size) == -1)
# else
            if (munmap((void *)ptr, stat_buf.st_size) == -1)
# endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "munmap() error : %s", strerror(errno));
            }
         }
#else
         if (munmap_emu((void *)ptr) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "munmap_emu() error : %s", strerror(errno));
         }
#endif
         if (close(oajl_fd) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "close() error : %s", strerror(errno));
         }
      }
   }

#ifdef HAVE_MMAP
   if (munmap((void *)ajl, ajl_size) == -1)
#else
   if (munmap_emu((void *)ajl) == -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "munmap() error : %s", strerror(errno));
   }
   ajl = NULL;

   return;
}
