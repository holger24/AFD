/*
 *  convert_msa.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2006 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   convert_msa - converts the MSA from an old format to new one
 **
 ** SYNOPSIS
 **   char *convert_msa(int           old_msa_fd,
 **                     char          *old_msa_stat,
 **                     off_t         *old_msa_size,
 **                     int           old_no_of_afds,
 **                     unsigned char old_version,
 **                     unsigned char new_version)
 **
 ** DESCRIPTION
 **   When there is a change in the structure mon_status_area (MSA)
 **   This function converts an old MSA to the new one. This one
 **   is currently for converting Version 0, 1 and 2.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   24.07.2006 H.Kiehl Created
 **   23.11.2008 H.Kiehl Added version 2.
 **   05.12.2009 H.Kiehl Added version 3.
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
#include <unistd.h>                   /* sysconf()                       */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>                   /* O_RDWR, O_CREAT, O_WRONLY, etc  */
#endif
#include <errno.h>
#include "mondefs.h"

/* Version 0 */
#define AFD_WORD_OFFSET_0          (SIZEOF_INT + 4 + SIZEOF_INT + 4)
#define MAX_PATH_LENGTH_0          1024
#define MAX_CONVERT_USERNAME_0     5
#define MAX_USER_NAME_LENGTH_0     80
#define MAX_AFDNAME_LENGTH_0       12
#define MAX_REAL_HOSTNAME_LENGTH_0 40
#define MAX_REMOTE_CMD_LENGTH_0    10
#define MAX_VERSION_LENGTH_0       40
#define STORAGE_TIME_0             7
#define LOG_FIFO_SIZE_0            5
#define NO_OF_LOG_HISTORY_0        3
#define MAX_LOG_HISTORY_0          48
struct mon_status_area_0
       {
          char          r_work_dir[MAX_PATH_LENGTH_0];
          char          convert_username[MAX_CONVERT_USERNAME_0][2][MAX_USER_NAME_LENGTH_0];
          char          afd_alias[MAX_AFDNAME_LENGTH_0 + 1];
          char          hostname[2][MAX_REAL_HOSTNAME_LENGTH_0];
          char          rcmd[MAX_REMOTE_CMD_LENGTH_0];
          char          afd_version[MAX_VERSION_LENGTH_0];
          int           port[2];
          int           poll_interval;
          unsigned int  connect_time;
          unsigned int  disconnect_time;
          char          amg;
          char          fd;
          char          archive_watch;
          int           jobs_in_queue;
          int           no_of_transfers;
          int           top_no_of_transfers[STORAGE_TIME_0];
          time_t        top_not_time;
          int           max_connections;
          unsigned int  sys_log_ec;
          char          sys_log_fifo[LOG_FIFO_SIZE_0 + 1];
          char          log_history[NO_OF_LOG_HISTORY_0][MAX_LOG_HISTORY_0];
          int           host_error_counter;
          int           no_of_hosts;
          unsigned int  no_of_jobs;
          unsigned int  options;
          unsigned int  fc;
          unsigned int  fs;
          unsigned int  tr;
          unsigned int  top_tr[STORAGE_TIME_0];
          time_t        top_tr_time;
          unsigned int  fr;
          unsigned int  top_fr[STORAGE_TIME_0];
          time_t        top_fr_time;
          unsigned int  ec;
          time_t        last_data_time;
          char          connect_status;
          unsigned char afd_switching;
          char          afd_toggle;
       };

/* Version 1 */
#define AFD_WORD_OFFSET_1          (SIZEOF_INT + 4 + SIZEOF_INT + 4)
#define MAX_PATH_LENGTH_1          1024
#define MAX_CONVERT_USERNAME_1     5
#define MAX_USER_NAME_LENGTH_1     80
#define MAX_AFDNAME_LENGTH_1       12
#define MAX_REAL_HOSTNAME_LENGTH_1 40
#define MAX_REMOTE_CMD_LENGTH_1    10
#define MAX_VERSION_LENGTH_1       40
#define STORAGE_TIME_1             7
#define LOG_FIFO_SIZE_1            5
#define NO_OF_LOG_HISTORY_1        3
#define MAX_LOG_HISTORY_1          48
#define SUM_STORAGE_1              6
struct mon_status_area_1
       {
          char          r_work_dir[MAX_PATH_LENGTH_1];
          char          convert_username[MAX_CONVERT_USERNAME_1][2][MAX_USER_NAME_LENGTH_1];
          char          afd_alias[MAX_AFDNAME_LENGTH_1 + 1];
          char          hostname[2][MAX_REAL_HOSTNAME_LENGTH_1];
          char          rcmd[MAX_REMOTE_CMD_LENGTH_1];
          char          afd_version[MAX_VERSION_LENGTH_1];
          int           port[2];
          int           poll_interval;
          unsigned int  connect_time;
          unsigned int  disconnect_time;
          char          amg;
          char          fd;
          char          archive_watch;
          int           jobs_in_queue;
          int           no_of_transfers;
          int           top_no_of_transfers[STORAGE_TIME_1];
          time_t        top_not_time;
          int           max_connections;
          unsigned int  sys_log_ec;
          char          sys_log_fifo[LOG_FIFO_SIZE_1 + 1];
          char          log_history[NO_OF_LOG_HISTORY_1][MAX_LOG_HISTORY_1];
          int           host_error_counter;
          int           no_of_hosts;
          int           no_of_dirs;                        /* New */
          unsigned int  no_of_jobs;
          unsigned int  options;
          unsigned int  log_capabilities;                  /* New */
          unsigned int  fc;
          u_off_t       fs;                                /* Type changed */
          u_off_t       tr;                                /* Type changed */
          u_off_t       top_tr[STORAGE_TIME_1];            /* Type changed */
          time_t        top_tr_time;
          unsigned int  fr;
          unsigned int  top_fr[STORAGE_TIME_1];
          time_t        top_fr_time;
          unsigned int  ec;
          time_t        last_data_time;
          u_off_t       bytes_send[SUM_STORAGE_1];         /* New */
          u_off_t       bytes_received[SUM_STORAGE_1];     /* New */
          u_off_t       log_bytes_received[SUM_STORAGE_1]; /* New */
          unsigned int  files_send[SUM_STORAGE_1];         /* New */
          unsigned int  files_received[SUM_STORAGE_1];     /* New */
          unsigned int  connections[SUM_STORAGE_1];        /* New */
          unsigned int  total_errors[SUM_STORAGE_1];       /* New */
          char          connect_status;
          unsigned char special_flag;                      /* New */
          unsigned char afd_switching;
          char          afd_toggle;
       };

/* Version 2 */
#define AFD_WORD_OFFSET_2          (SIZEOF_INT + 4 + SIZEOF_INT + 4)
#define MAX_PATH_LENGTH_2          1024
#define MAX_CONVERT_USERNAME_2     5
#define MAX_USER_NAME_LENGTH_2     80
#define MAX_AFDNAME_LENGTH_2       12
#define MAX_REAL_HOSTNAME_LENGTH_2 40
#define MAX_REMOTE_CMD_LENGTH_2    10
#define MAX_VERSION_LENGTH_2       40
#define STORAGE_TIME_2             7
#define LOG_FIFO_SIZE_2            5
#define NO_OF_LOG_HISTORY_2        3
#define MAX_LOG_HISTORY_2          48
#define SUM_STORAGE_2              6
struct mon_status_area_2
       {
          char          r_work_dir[MAX_PATH_LENGTH_2];
          char          convert_username[MAX_CONVERT_USERNAME_2][2][MAX_USER_NAME_LENGTH_2];
          char          afd_alias[MAX_AFDNAME_LENGTH_2 + 1];
          char          hostname[2][MAX_REAL_HOSTNAME_LENGTH_2];
          char          rcmd[MAX_REMOTE_CMD_LENGTH_2];
          char          afd_version[MAX_VERSION_LENGTH_2];
          int           port[2];
          int           poll_interval;
          unsigned int  connect_time;
          unsigned int  disconnect_time;
          char          amg;
          char          fd;
          char          archive_watch;
          int           jobs_in_queue;
          long          danger_no_of_jobs;  /* New */
          int           no_of_transfers;
          int           top_no_of_transfers[STORAGE_TIME_2];
          time_t        top_not_time;
          int           max_connections;
          unsigned int  sys_log_ec;
          char          sys_log_fifo[LOG_FIFO_SIZE_2 + 1];
          char          log_history[NO_OF_LOG_HISTORY_2][MAX_LOG_HISTORY_2];
          int           host_error_counter;
          int           no_of_hosts;
          int           no_of_dirs;
          unsigned int  no_of_jobs;
          unsigned int  options;
          unsigned int  log_capabilities;
          unsigned int  fc;
          u_off_t       fs;
          u_off_t       tr;
          u_off_t       top_tr[STORAGE_TIME_2];
          time_t        top_tr_time;
          unsigned int  fr;
          unsigned int  top_fr[STORAGE_TIME_2];
          time_t        top_fr_time;
          unsigned int  ec;
          time_t        last_data_time;
          u_off_t       bytes_send[SUM_STORAGE_2];
          u_off_t       bytes_received[SUM_STORAGE_2];
          u_off_t       log_bytes_received[SUM_STORAGE_2];
          unsigned int  files_send[SUM_STORAGE_2];
          unsigned int  files_received[SUM_STORAGE_2];
          unsigned int  connections[SUM_STORAGE_2];
          unsigned int  total_errors[SUM_STORAGE_2];
          char          connect_status;
          unsigned char special_flag;
          unsigned char afd_switching;
          char          afd_toggle;
       };

/* Version 3 */
#define AFD_WORD_OFFSET_3          (SIZEOF_INT + 4 + SIZEOF_INT + 4)
#define MAX_PATH_LENGTH_3          1024
#define MAX_CONVERT_USERNAME_3     5
#define MAX_USER_NAME_LENGTH_3     80
#define MAX_AFDNAME_LENGTH_3       12
#define MAX_REAL_HOSTNAME_LENGTH_3 40
#define MAX_REMOTE_CMD_LENGTH_3    10
#define MAX_VERSION_LENGTH_3       40
#define STORAGE_TIME_3             7
#define LOG_FIFO_SIZE_3            5
#define NO_OF_LOG_HISTORY_3        3
#define MAX_LOG_HISTORY_3          48
#define SUM_STORAGE_3              6
struct mon_status_area_3
       {
          char          r_work_dir[MAX_PATH_LENGTH_3];
          char          convert_username[MAX_CONVERT_USERNAME_3][2][MAX_USER_NAME_LENGTH_3];
          char          afd_alias[MAX_AFDNAME_LENGTH_3 + 1];
          char          hostname[2][MAX_REAL_HOSTNAME_LENGTH_3];
          char          rcmd[MAX_REMOTE_CMD_LENGTH_3];
          char          afd_version[MAX_VERSION_LENGTH_3];
          int           port[2];
          int           poll_interval;
          unsigned int  connect_time;
          unsigned int  disconnect_time;
          unsigned int  afd_id;            /* New */
          char          amg;
          char          fd;
          char          archive_watch;
          int           jobs_in_queue;
          long          danger_no_of_jobs;
          int           no_of_transfers;
          int           top_no_of_transfers[STORAGE_TIME_3];
          time_t        top_not_time;
          int           max_connections;
          unsigned int  sys_log_ec;
          char          sys_log_fifo[LOG_FIFO_SIZE_3 + 1];
          char          log_history[NO_OF_LOG_HISTORY_3][MAX_LOG_HISTORY_3];
          int           host_error_counter;
          int           no_of_hosts;
          int           no_of_dirs;
          unsigned int  no_of_jobs;
          unsigned int  options;
          unsigned int  log_capabilities;
          unsigned int  fc;
          u_off_t       fs;
          u_off_t       tr;
          u_off_t       top_tr[STORAGE_TIME_3];
          time_t        top_tr_time;
          unsigned int  fr;
          unsigned int  top_fr[STORAGE_TIME_3];
          time_t        top_fr_time;
          unsigned int  ec;
          time_t        last_data_time;
          double        bytes_send[SUM_STORAGE_3];         /* Changed */
          double        bytes_received[SUM_STORAGE_3];     /* Changed */
          double        log_bytes_received[SUM_STORAGE_3]; /* Changed */
          unsigned int  files_send[SUM_STORAGE_3];
          unsigned int  files_received[SUM_STORAGE_3];
          unsigned int  connections[SUM_STORAGE_3];
          unsigned int  total_errors[SUM_STORAGE_3];
          char          connect_status;
          unsigned char special_flag;
          unsigned char afd_switching;
          char          afd_toggle;
       };



/*############################ convert_msa() ############################*/
char *
convert_msa(int           old_msa_fd,
            char          *old_msa_stat,
            off_t         *old_msa_size,
            int           old_no_of_afds,
            unsigned char old_version,
            unsigned char new_version)
{
   int          i, j;
   size_t       new_size;
   char         *ptr;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   if ((old_version == 0) && (new_version == 1))
   {
      int                      pagesize;
      struct mon_status_area_0 *old_msa;
      struct mon_status_area_1 *new_msa;

      /* Get the size of the old MSA file. */
#ifdef HAVE_STATX
      if (statx(old_msa_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(old_msa_fd, &stat_buf) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to access %s : %s",
                    old_msa_stat, strerror(errno));
         *old_msa_size = -1;
         return(NULL);
      }
      else
      {
#ifdef HAVE_STATX
         if (stat_buf.stx_size > 0)
#else
         if (stat_buf.st_size > 0)
#endif
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
                                stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                                stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                                MAP_SHARED, old_msa_stat, 0)) == (caddr_t) -1)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to mmap() to %s : %s",
                          old_msa_stat, strerror(errno));
               *old_msa_size = -1;
               return(NULL);
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "MSA file %s is empty.", old_msa_stat);
            *old_msa_size = -1;
            return(NULL);
         }
      }

      ptr += AFD_WORD_OFFSET_0;
      old_msa = (struct mon_status_area_0 *)ptr;

      new_size = old_no_of_afds * sizeof(struct mon_status_area_1);
      if ((ptr = malloc(new_size)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "malloc() error [%d %d] : %s",
                    old_no_of_afds, new_size, strerror(errno));
         ptr = (char *)old_msa;
         ptr -= AFD_WORD_OFFSET_0;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
         if (munmap(ptr, stat_buf.stx_size) == -1)
# else
         if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
         if (munmap_emu(ptr) == -1)
#endif
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to munmap() %s : %s",
                       old_msa_stat, strerror(errno));
         }
         *old_msa_size = -1;
         return(NULL);
      }
      (void)memset(ptr, 0, new_size);
      new_msa = (struct mon_status_area_1 *)ptr;

      /*
       * Copy all the old data into the new region.
       */
      for (i = 0; i < old_no_of_afds; i++)
      {
         (void)strcpy(new_msa[i].r_work_dir, old_msa[i].r_work_dir);
         for (j = 0; j < MAX_CONVERT_USERNAME_0; j++)
         {
            (void)strcpy(new_msa[i].convert_username[j][0],
                         old_msa[i].convert_username[j][0]);
            (void)strcpy(new_msa[i].convert_username[j][1],
                         old_msa[i].convert_username[j][1]);
         }
         (void)memcpy(new_msa[i].afd_alias, old_msa[i].afd_alias,
                      (MAX_AFDNAME_LENGTH_0 + 1));
         (void)strcpy(new_msa[i].hostname[0], old_msa[i].hostname[0]);
         (void)strcpy(new_msa[i].hostname[1], old_msa[i].hostname[1]);
         (void)strcpy(new_msa[i].rcmd, old_msa[i].rcmd);
         (void)strcpy(new_msa[i].afd_version, old_msa[i].afd_version);
         new_msa[i].port[0] = old_msa[i].port[0];
         new_msa[i].port[1] = old_msa[i].port[1];
         new_msa[i].poll_interval = old_msa[i].poll_interval;
         new_msa[i].connect_time = old_msa[i].connect_time;
         new_msa[i].disconnect_time = old_msa[i].disconnect_time;
         new_msa[i].amg = old_msa[i].amg;
         new_msa[i].fd = old_msa[i].fd;
         new_msa[i].archive_watch = old_msa[i].archive_watch;
         new_msa[i].jobs_in_queue = old_msa[i].jobs_in_queue;
         new_msa[i].no_of_transfers = old_msa[i].no_of_transfers;
         for (j = 0; j < STORAGE_TIME_0; j++)
         {
            new_msa[i].top_no_of_transfers[j] = old_msa[i].top_no_of_transfers[j];
            new_msa[i].top_tr[j] = (u_off_t)old_msa[i].top_tr[j];
            new_msa[i].top_fr[j] = old_msa[i].top_fr[j];
         }
         new_msa[i].top_not_time = old_msa[i].top_not_time;
         new_msa[i].max_connections = old_msa[i].max_connections;
         new_msa[i].sys_log_ec = old_msa[i].sys_log_ec;
         (void)memcpy(new_msa[i].sys_log_fifo, old_msa[i].sys_log_fifo,
                      (LOG_FIFO_SIZE_0 + 1));
         for (j = 0; j < NO_OF_LOG_HISTORY_0; j++)
         {
            (void)memcpy(new_msa[i].log_history[j], old_msa[i].log_history[j],
                         MAX_LOG_HISTORY_0);
         }
         new_msa[i].host_error_counter = old_msa[i].host_error_counter;
         new_msa[i].no_of_hosts = old_msa[i].no_of_hosts;
         new_msa[i].no_of_dirs = 0;
         new_msa[i].no_of_jobs = old_msa[i].no_of_jobs;
         new_msa[i].options = old_msa[i].options;
         new_msa[i].log_capabilities = 0;
         new_msa[i].fc = old_msa[i].fc;
         new_msa[i].fs = (u_off_t)old_msa[i].fs;
         new_msa[i].tr = (u_off_t)old_msa[i].tr;
         new_msa[i].top_tr_time = old_msa[i].top_tr_time;
         new_msa[i].fr = old_msa[i].fr;
         new_msa[i].top_fr_time = old_msa[i].top_fr_time;
         new_msa[i].ec = old_msa[i].ec;
         new_msa[i].last_data_time = old_msa[i].last_data_time;
         for (j = 0; j < SUM_STORAGE_1; j++)
         {
            new_msa[i].bytes_send[j] = 0;
            new_msa[i].bytes_received[j] = 0;
            new_msa[i].files_send[j] = 0;
            new_msa[i].files_received[j] = 0;
            new_msa[i].connections[j] = 0;
            new_msa[i].total_errors[j] = 0;
            new_msa[i].log_bytes_received[j] = 0;
         }
         new_msa[i].connect_status = old_msa[i].connect_status;
         new_msa[i].special_flag = 0;
         new_msa[i].afd_switching = old_msa[i].afd_switching;
         new_msa[i].afd_toggle = old_msa[i].afd_toggle;
      }

      ptr = (char *)old_msa;
      ptr -= AFD_WORD_OFFSET_0;

      /*
       * Resize the old MSA to the size of new one and then copy
       * the new structure into it. Then update the MSA version
       * number.
       */
      if ((ptr = mmap_resize(old_msa_fd, ptr, new_size + AFD_WORD_OFFSET_1)) == (caddr_t) -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to mmap_resize() %s : %s",
                    old_msa_stat, strerror(errno));
         free((void *)new_msa);
         return(NULL);
      }
      ptr += AFD_WORD_OFFSET_1;
      (void)memcpy(ptr, new_msa, new_size);
      free((void *)new_msa);
      ptr -= AFD_WORD_OFFSET_1;
      *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
      *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
      if ((pagesize = (int)sysconf(_SC_PAGESIZE)) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to determine the pagesize with sysconf() : %s",
                    strerror(errno));
      }
      *(int *)(ptr + SIZEOF_INT + 4) = pagesize;
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
      *old_msa_size = new_size + AFD_WORD_OFFSET_1;

      system_log(INFO_SIGN, NULL, 0,
                 "Converted MSA from verion %d to %d.",
                 (int)old_version, (int)new_version);
   }
   else if ((old_version == 0) && (new_version == 2))
        {
           int                      pagesize;
           struct mon_status_area_0 *old_msa;
           struct mon_status_area_2 *new_msa;

           /* Get the size of the old MSA file. */
#ifdef HAVE_STATX
           if (statx(old_msa_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                     STATX_SIZE, &stat_buf) == -1)
#else
           if (fstat(old_msa_fd, &stat_buf) == -1)
#endif
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to access %s : %s",
                         old_msa_stat, strerror(errno));
              *old_msa_size = -1;
              return(NULL);
           }
           else
           {
#ifdef HAVE_STATX
              if (stat_buf.stx_size > 0)
#else
              if (stat_buf.st_size > 0)
#endif
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
                                     MAP_SHARED, old_msa_stat, 0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_msa_stat, strerror(errno));
                    *old_msa_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "MSA file %s is empty.", old_msa_stat);
                 *old_msa_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_0;
           old_msa = (struct mon_status_area_0 *)ptr;

           new_size = old_no_of_afds * sizeof(struct mon_status_area_2);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_afds, new_size, strerror(errno));
              ptr = (char *)old_msa;
              ptr -= AFD_WORD_OFFSET_0;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
              if (munmap(ptr, stat_buf.stx_size) == -1)
# else
              if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_msa_stat, strerror(errno));
              }
              *old_msa_size = -1;
              return(NULL);
           }
           (void)memset(ptr, 0, new_size);
           new_msa = (struct mon_status_area_2 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_afds; i++)
           {
              (void)strcpy(new_msa[i].r_work_dir, old_msa[i].r_work_dir);
              for (j = 0; j < MAX_CONVERT_USERNAME_0; j++)
              {
                 (void)strcpy(new_msa[i].convert_username[j][0],
                              old_msa[i].convert_username[j][0]);
                 (void)strcpy(new_msa[i].convert_username[j][1],
                              old_msa[i].convert_username[j][1]);
              }
              (void)memcpy(new_msa[i].afd_alias, old_msa[i].afd_alias,
                           (MAX_AFDNAME_LENGTH_0 + 1));
              (void)strcpy(new_msa[i].hostname[0], old_msa[i].hostname[0]);
              (void)strcpy(new_msa[i].hostname[1], old_msa[i].hostname[1]);
              (void)strcpy(new_msa[i].rcmd, old_msa[i].rcmd);
              (void)strcpy(new_msa[i].afd_version, old_msa[i].afd_version);
              new_msa[i].port[0] = old_msa[i].port[0];
              new_msa[i].port[1] = old_msa[i].port[1];
              new_msa[i].poll_interval = old_msa[i].poll_interval;
              new_msa[i].connect_time = old_msa[i].connect_time;
              new_msa[i].disconnect_time = old_msa[i].disconnect_time;
              new_msa[i].amg = old_msa[i].amg;
              new_msa[i].fd = old_msa[i].fd;
              new_msa[i].archive_watch = old_msa[i].archive_watch;
              new_msa[i].jobs_in_queue = old_msa[i].jobs_in_queue;
              new_msa[i].danger_no_of_jobs = 0L;
              new_msa[i].no_of_transfers = old_msa[i].no_of_transfers;
              for (j = 0; j < STORAGE_TIME_0; j++)
              {
                 new_msa[i].top_no_of_transfers[j] = old_msa[i].top_no_of_transfers[j];
                 new_msa[i].top_tr[j] = (u_off_t)old_msa[i].top_tr[j];
                 new_msa[i].top_fr[j] = old_msa[i].top_fr[j];
              }
              new_msa[i].top_not_time = old_msa[i].top_not_time;
              new_msa[i].max_connections = old_msa[i].max_connections;
              new_msa[i].sys_log_ec = old_msa[i].sys_log_ec;
              (void)memcpy(new_msa[i].sys_log_fifo, old_msa[i].sys_log_fifo,
                           (LOG_FIFO_SIZE_0 + 1));
              for (j = 0; j < NO_OF_LOG_HISTORY_0; j++)
              {
                 (void)memcpy(new_msa[i].log_history[j], old_msa[i].log_history[j],
                              MAX_LOG_HISTORY_0);
              }
              new_msa[i].host_error_counter = old_msa[i].host_error_counter;
              new_msa[i].no_of_hosts = old_msa[i].no_of_hosts;
              new_msa[i].no_of_dirs = 0;
              new_msa[i].no_of_jobs = old_msa[i].no_of_jobs;
              new_msa[i].options = old_msa[i].options;
              new_msa[i].log_capabilities = 0;
              new_msa[i].fc = old_msa[i].fc;
              new_msa[i].fs = (u_off_t)old_msa[i].fs;
              new_msa[i].tr = (u_off_t)old_msa[i].tr;
              new_msa[i].top_tr_time = old_msa[i].top_tr_time;
              new_msa[i].fr = old_msa[i].fr;
              new_msa[i].top_fr_time = old_msa[i].top_fr_time;
              new_msa[i].ec = old_msa[i].ec;
              new_msa[i].last_data_time = old_msa[i].last_data_time;
              for (j = 0; j < SUM_STORAGE_2; j++)
              {
                 new_msa[i].bytes_send[j] = 0;
                 new_msa[i].bytes_received[j] = 0;
                 new_msa[i].files_send[j] = 0;
                 new_msa[i].files_received[j] = 0;
                 new_msa[i].connections[j] = 0;
                 new_msa[i].total_errors[j] = 0;
                 new_msa[i].log_bytes_received[j] = 0;
              }
              new_msa[i].connect_status = old_msa[i].connect_status;
              new_msa[i].special_flag = 0;
              new_msa[i].afd_switching = old_msa[i].afd_switching;
              new_msa[i].afd_toggle = old_msa[i].afd_toggle;
           }

           ptr = (char *)old_msa;
           ptr -= AFD_WORD_OFFSET_0;

           /*
            * Resize the old MSA to the size of new one and then copy
            * the new structure into it. Then update the MSA version
            * number.
            */
           if ((ptr = mmap_resize(old_msa_fd, ptr, new_size + AFD_WORD_OFFSET_2)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_msa_stat, strerror(errno));
              free((void *)new_msa);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_2;
           (void)memcpy(ptr, new_msa, new_size);
           free((void *)new_msa);
           ptr -= AFD_WORD_OFFSET_2;
           *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           if ((pagesize = (int)sysconf(_SC_PAGESIZE)) == -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to determine the pagesize with sysconf() : %s",
                         strerror(errno));
           }
           *(int *)(ptr + SIZEOF_INT + 4) = pagesize;
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
           *old_msa_size = new_size + AFD_WORD_OFFSET_2;

           system_log(INFO_SIGN, NULL, 0,
                      "Converted MSA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 0) && (new_version == 3))
        {
           int                      pagesize;
           struct mon_status_area_0 *old_msa;
           struct mon_status_area_3 *new_msa;

           /* Get the size of the old MSA file. */
#ifdef HAVE_STATX
           if (statx(old_msa_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                     STATX_SIZE, &stat_buf) == -1)
#else
           if (fstat(old_msa_fd, &stat_buf) == -1)
#endif
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to access %s : %s",
                         old_msa_stat, strerror(errno));
              *old_msa_size = -1;
              return(NULL);
           }
           else
           {
#ifdef HAVE_STATX
              if (stat_buf.stx_size > 0)
#else
              if (stat_buf.st_size > 0)
#endif
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
                               "Failed to mmap() to %s : %s",
                               old_msa_stat, strerror(errno));
                    *old_msa_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "MSA file %s is empty.", old_msa_stat);
                 *old_msa_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_0;
           old_msa = (struct mon_status_area_0 *)ptr;

           new_size = old_no_of_afds * sizeof(struct mon_status_area_3);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_afds, new_size, strerror(errno));
              ptr = (char *)old_msa;
              ptr -= AFD_WORD_OFFSET_0;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
              if (munmap(ptr, stat_buf.stx_size) == -1)
# else
              if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_msa_stat, strerror(errno));
              }
              *old_msa_size = -1;
              return(NULL);
           }
           (void)memset(ptr, 0, new_size);
           new_msa = (struct mon_status_area_3 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_afds; i++)
           {
              (void)strcpy(new_msa[i].r_work_dir, old_msa[i].r_work_dir);
              for (j = 0; j < MAX_CONVERT_USERNAME_0; j++)
              {
                 (void)strcpy(new_msa[i].convert_username[j][0],
                              old_msa[i].convert_username[j][0]);
                 (void)strcpy(new_msa[i].convert_username[j][1],
                              old_msa[i].convert_username[j][1]);
              }
              (void)memcpy(new_msa[i].afd_alias, old_msa[i].afd_alias,
                           (MAX_AFDNAME_LENGTH_0 + 1));
              (void)strcpy(new_msa[i].hostname[0], old_msa[i].hostname[0]);
              (void)strcpy(new_msa[i].hostname[1], old_msa[i].hostname[1]);
              (void)strcpy(new_msa[i].rcmd, old_msa[i].rcmd);
              (void)strcpy(new_msa[i].afd_version, old_msa[i].afd_version);
              new_msa[i].port[0] = old_msa[i].port[0];
              new_msa[i].port[1] = old_msa[i].port[1];
              new_msa[i].poll_interval = old_msa[i].poll_interval;
              new_msa[i].connect_time = old_msa[i].connect_time;
              new_msa[i].disconnect_time = old_msa[i].disconnect_time;
              new_msa[i].afd_id = get_str_checksum(new_msa[i].afd_alias);
              new_msa[i].amg = old_msa[i].amg;
              new_msa[i].fd = old_msa[i].fd;
              new_msa[i].archive_watch = old_msa[i].archive_watch;
              new_msa[i].jobs_in_queue = old_msa[i].jobs_in_queue;
              new_msa[i].danger_no_of_jobs = 0L;
              new_msa[i].no_of_transfers = old_msa[i].no_of_transfers;
              for (j = 0; j < STORAGE_TIME_0; j++)
              {
                 new_msa[i].top_no_of_transfers[j] = old_msa[i].top_no_of_transfers[j];
                 new_msa[i].top_tr[j] = (u_off_t)old_msa[i].top_tr[j];
                 new_msa[i].top_fr[j] = old_msa[i].top_fr[j];
              }
              new_msa[i].top_not_time = old_msa[i].top_not_time;
              new_msa[i].max_connections = old_msa[i].max_connections;
              new_msa[i].sys_log_ec = old_msa[i].sys_log_ec;
              (void)memcpy(new_msa[i].sys_log_fifo, old_msa[i].sys_log_fifo,
                           (LOG_FIFO_SIZE_0 + 1));
              for (j = 0; j < NO_OF_LOG_HISTORY_0; j++)
              {
                 (void)memcpy(new_msa[i].log_history[j], old_msa[i].log_history[j],
                              MAX_LOG_HISTORY_0);
              }
              new_msa[i].host_error_counter = old_msa[i].host_error_counter;
              new_msa[i].no_of_hosts = old_msa[i].no_of_hosts;
              new_msa[i].no_of_dirs = 0;
              new_msa[i].no_of_jobs = old_msa[i].no_of_jobs;
              new_msa[i].options = old_msa[i].options;
              new_msa[i].log_capabilities = 0;
              new_msa[i].fc = old_msa[i].fc;
              new_msa[i].fs = (u_off_t)old_msa[i].fs;
              new_msa[i].tr = (u_off_t)old_msa[i].tr;
              new_msa[i].top_tr_time = old_msa[i].top_tr_time;
              new_msa[i].fr = old_msa[i].fr;
              new_msa[i].top_fr_time = old_msa[i].top_fr_time;
              new_msa[i].ec = old_msa[i].ec;
              new_msa[i].last_data_time = old_msa[i].last_data_time;
              for (j = 0; j < SUM_STORAGE_3; j++)
              {
                 new_msa[i].bytes_send[j] = 0;
                 new_msa[i].bytes_received[j] = 0;
                 new_msa[i].files_send[j] = 0;
                 new_msa[i].files_received[j] = 0;
                 new_msa[i].connections[j] = 0;
                 new_msa[i].total_errors[j] = 0;
                 new_msa[i].log_bytes_received[j] = 0;
              }
              new_msa[i].connect_status = old_msa[i].connect_status;
              new_msa[i].special_flag = 0;
              new_msa[i].afd_switching = old_msa[i].afd_switching;
              new_msa[i].afd_toggle = old_msa[i].afd_toggle;
           }

           ptr = (char *)old_msa;
           ptr -= AFD_WORD_OFFSET_0;

           /*
            * Resize the old MSA to the size of new one and then copy
            * the new structure into it. Then update the MSA version
            * number.
            */
           if ((ptr = mmap_resize(old_msa_fd, ptr, new_size + AFD_WORD_OFFSET_3)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_msa_stat, strerror(errno));
              free((void *)new_msa);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_3;
           (void)memcpy(ptr, new_msa, new_size);
           free((void *)new_msa);
           ptr -= AFD_WORD_OFFSET_3;
           *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           if ((pagesize = (int)sysconf(_SC_PAGESIZE)) == -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to determine the pagesize with sysconf() : %s",
                         strerror(errno));
           }
           *(int *)(ptr + SIZEOF_INT + 4) = pagesize;
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
           *old_msa_size = new_size + AFD_WORD_OFFSET_3;

           system_log(INFO_SIGN, NULL, 0,
                      "Converted MSA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 1) && (new_version == 2))
        {
           int                      pagesize;
           struct mon_status_area_1 *old_msa;
           struct mon_status_area_2 *new_msa;

           /* Get the size of the old MSA file. */
#ifdef HAVE_STATX
           if (statx(old_msa_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                     STATX_SIZE, &stat_buf) == -1)
#else
           if (fstat(old_msa_fd, &stat_buf) == -1)
#endif
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to access %s : %s",
                         old_msa_stat, strerror(errno));
              *old_msa_size = -1;
              return(NULL);
           }
           else
           {
#ifdef HAVE_STATX
              if (stat_buf.stx_size > 0)
#else
              if (stat_buf.st_size > 0)
#endif
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                                 stat_buf.stx_size,
# else
                                 stat_buf.st_size,
# endif
                                 (PROT_READ | PROT_WRITE),
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
                               "Failed to mmap() to %s : %s",
                               old_msa_stat, strerror(errno));
                    *old_msa_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "MSA file %s is empty.", old_msa_stat);
                 *old_msa_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_1;
           old_msa = (struct mon_status_area_1 *)ptr;

           new_size = old_no_of_afds * sizeof(struct mon_status_area_2);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_afds, new_size, strerror(errno));
              ptr = (char *)old_msa;
              ptr -= AFD_WORD_OFFSET_1;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
              if (munmap(ptr, stat_buf.stx_size) == -1)
# else
              if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_msa_stat, strerror(errno));
              }
              *old_msa_size = -1;
              return(NULL);
           }
           (void)memset(ptr, 0, new_size);
           new_msa = (struct mon_status_area_2 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_afds; i++)
           {
              (void)strcpy(new_msa[i].r_work_dir, old_msa[i].r_work_dir);
              for (j = 0; j < MAX_CONVERT_USERNAME_1; j++)
              {
                 (void)strcpy(new_msa[i].convert_username[j][0],
                              old_msa[i].convert_username[j][0]);
                 (void)strcpy(new_msa[i].convert_username[j][1],
                              old_msa[i].convert_username[j][1]);
              }
              (void)memcpy(new_msa[i].afd_alias, old_msa[i].afd_alias,
                           (MAX_AFDNAME_LENGTH_1 + 1));
              (void)strcpy(new_msa[i].hostname[0], old_msa[i].hostname[0]);
              (void)strcpy(new_msa[i].hostname[1], old_msa[i].hostname[1]);
              (void)strcpy(new_msa[i].rcmd, old_msa[i].rcmd);
              (void)strcpy(new_msa[i].afd_version, old_msa[i].afd_version);
              new_msa[i].port[0] = old_msa[i].port[0];
              new_msa[i].port[1] = old_msa[i].port[1];
              new_msa[i].poll_interval = old_msa[i].poll_interval;
              new_msa[i].connect_time = old_msa[i].connect_time;
              new_msa[i].disconnect_time = old_msa[i].disconnect_time;
              new_msa[i].amg = old_msa[i].amg;
              new_msa[i].fd = old_msa[i].fd;
              new_msa[i].archive_watch = old_msa[i].archive_watch;
              new_msa[i].jobs_in_queue = old_msa[i].jobs_in_queue;
              new_msa[i].danger_no_of_jobs = 0L;
              new_msa[i].no_of_transfers = old_msa[i].no_of_transfers;
              for (j = 0; j < STORAGE_TIME_1; j++)
              {
                 new_msa[i].top_no_of_transfers[j] = old_msa[i].top_no_of_transfers[j];
                 new_msa[i].top_tr[j] = (u_off_t)old_msa[i].top_tr[j];
                 new_msa[i].top_fr[j] = old_msa[i].top_fr[j];
              }
              new_msa[i].top_not_time = old_msa[i].top_not_time;
              new_msa[i].max_connections = old_msa[i].max_connections;
              new_msa[i].sys_log_ec = old_msa[i].sys_log_ec;
              (void)memcpy(new_msa[i].sys_log_fifo, old_msa[i].sys_log_fifo,
                           (LOG_FIFO_SIZE_1 + 1));
              for (j = 0; j < NO_OF_LOG_HISTORY_1; j++)
              {
                 (void)memcpy(new_msa[i].log_history[j], old_msa[i].log_history[j],
                              MAX_LOG_HISTORY_1);
              }
              new_msa[i].host_error_counter = old_msa[i].host_error_counter;
              new_msa[i].no_of_hosts = old_msa[i].no_of_hosts;
              new_msa[i].no_of_dirs = old_msa[i].no_of_dirs;
              new_msa[i].no_of_jobs = old_msa[i].no_of_jobs;
              new_msa[i].options = old_msa[i].options;
              new_msa[i].log_capabilities = old_msa[i].log_capabilities;
              new_msa[i].fc = old_msa[i].fc;
              new_msa[i].fs = old_msa[i].fs;
              new_msa[i].tr = old_msa[i].tr;
              new_msa[i].top_tr_time = old_msa[i].top_tr_time;
              new_msa[i].fr = old_msa[i].fr;
              new_msa[i].top_fr_time = old_msa[i].top_fr_time;
              new_msa[i].ec = old_msa[i].ec;
              new_msa[i].last_data_time = old_msa[i].last_data_time;
              for (j = 0; j < SUM_STORAGE_1; j++)
              {
                 new_msa[i].bytes_send[j] = old_msa[i].bytes_send[j];
                 new_msa[i].bytes_received[j] = old_msa[i].bytes_received[j];
                 new_msa[i].files_send[j] = old_msa[i].files_send[j];
                 new_msa[i].files_received[j] = old_msa[i].files_received[j];
                 new_msa[i].connections[j] = old_msa[i].connections[j];
                 new_msa[i].total_errors[j] = old_msa[i].total_errors[j];
                 new_msa[i].log_bytes_received[j] = old_msa[i].log_bytes_received[j];
              }
              new_msa[i].connect_status = old_msa[i].connect_status;
              new_msa[i].special_flag = old_msa[i].special_flag;
              new_msa[i].afd_switching = old_msa[i].afd_switching;
              new_msa[i].afd_toggle = old_msa[i].afd_toggle;
           }

           ptr = (char *)old_msa;
           ptr -= AFD_WORD_OFFSET_1;

           /*
            * Resize the old MSA to the size of new one and then copy
            * the new structure into it. Then update the MSA version
            * number.
            */
           if ((ptr = mmap_resize(old_msa_fd, ptr, new_size + AFD_WORD_OFFSET_2)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_msa_stat, strerror(errno));
              free((void *)new_msa);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_2;
           (void)memcpy(ptr, new_msa, new_size);
           free((void *)new_msa);
           ptr -= AFD_WORD_OFFSET_2;
           *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           if ((pagesize = (int)sysconf(_SC_PAGESIZE)) == -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to determine the pagesize with sysconf() : %s",
                         strerror(errno));
           }
           *(int *)(ptr + SIZEOF_INT + 4) = pagesize;
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
           *old_msa_size = new_size + AFD_WORD_OFFSET_2;

           system_log(INFO_SIGN, NULL, 0,
                      "Converted MSA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 1) && (new_version == 3))
        {
           int                      pagesize;
           struct mon_status_area_1 *old_msa;
           struct mon_status_area_3 *new_msa;

           /* Get the size of the old MSA file. */
#ifdef HAVE_STATX
           if (statx(old_msa_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                     STATX_SIZE, &stat_buf) == -1)
#else
           if (fstat(old_msa_fd, &stat_buf) == -1)
#endif
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to access %s : %s",
                         old_msa_stat, strerror(errno));
              *old_msa_size = -1;
              return(NULL);
           }
           else
           {
#ifdef HAVE_STATX
              if (stat_buf.stx_size > 0)
#else
              if (stat_buf.st_size > 0)
#endif
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                                 stat_buf.stx_size,
# else
                                 stat_buf.st_size,
# endif
                                 (PROT_READ | PROT_WRITE),
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
                               "Failed to mmap() to %s : %s",
                               old_msa_stat, strerror(errno));
                    *old_msa_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "MSA file %s is empty.", old_msa_stat);
                 *old_msa_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_1;
           old_msa = (struct mon_status_area_1 *)ptr;

           new_size = old_no_of_afds * sizeof(struct mon_status_area_3);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_afds, new_size, strerror(errno));
              ptr = (char *)old_msa;
              ptr -= AFD_WORD_OFFSET_1;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
              if (munmap(ptr, stat_buf.stx_size) == -1)
# else
              if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_msa_stat, strerror(errno));
              }
              *old_msa_size = -1;
              return(NULL);
           }
           (void)memset(ptr, 0, new_size);
           new_msa = (struct mon_status_area_3 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_afds; i++)
           {
              (void)strcpy(new_msa[i].r_work_dir, old_msa[i].r_work_dir);
              for (j = 0; j < MAX_CONVERT_USERNAME_1; j++)
              {
                 (void)strcpy(new_msa[i].convert_username[j][0],
                              old_msa[i].convert_username[j][0]);
                 (void)strcpy(new_msa[i].convert_username[j][1],
                              old_msa[i].convert_username[j][1]);
              }
              (void)memcpy(new_msa[i].afd_alias, old_msa[i].afd_alias,
                           (MAX_AFDNAME_LENGTH_1 + 1));
              (void)strcpy(new_msa[i].hostname[0], old_msa[i].hostname[0]);
              (void)strcpy(new_msa[i].hostname[1], old_msa[i].hostname[1]);
              (void)strcpy(new_msa[i].rcmd, old_msa[i].rcmd);
              (void)strcpy(new_msa[i].afd_version, old_msa[i].afd_version);
              new_msa[i].port[0] = old_msa[i].port[0];
              new_msa[i].port[1] = old_msa[i].port[1];
              new_msa[i].poll_interval = old_msa[i].poll_interval;
              new_msa[i].connect_time = old_msa[i].connect_time;
              new_msa[i].disconnect_time = old_msa[i].disconnect_time;
              new_msa[i].afd_id = get_str_checksum(new_msa[i].afd_alias);
              new_msa[i].amg = old_msa[i].amg;
              new_msa[i].fd = old_msa[i].fd;
              new_msa[i].archive_watch = old_msa[i].archive_watch;
              new_msa[i].jobs_in_queue = old_msa[i].jobs_in_queue;
              new_msa[i].danger_no_of_jobs = 0L;
              new_msa[i].no_of_transfers = old_msa[i].no_of_transfers;
              for (j = 0; j < STORAGE_TIME_1; j++)
              {
                 new_msa[i].top_no_of_transfers[j] = old_msa[i].top_no_of_transfers[j];
                 new_msa[i].top_tr[j] = (u_off_t)old_msa[i].top_tr[j];
                 new_msa[i].top_fr[j] = old_msa[i].top_fr[j];
              }
              new_msa[i].top_not_time = old_msa[i].top_not_time;
              new_msa[i].max_connections = old_msa[i].max_connections;
              new_msa[i].sys_log_ec = old_msa[i].sys_log_ec;
              (void)memcpy(new_msa[i].sys_log_fifo, old_msa[i].sys_log_fifo,
                           (LOG_FIFO_SIZE_1 + 1));
              for (j = 0; j < NO_OF_LOG_HISTORY_1; j++)
              {
                 (void)memcpy(new_msa[i].log_history[j], old_msa[i].log_history[j],
                              MAX_LOG_HISTORY_1);
              }
              new_msa[i].host_error_counter = old_msa[i].host_error_counter;
              new_msa[i].no_of_hosts = old_msa[i].no_of_hosts;
              new_msa[i].no_of_dirs = old_msa[i].no_of_dirs;
              new_msa[i].no_of_jobs = old_msa[i].no_of_jobs;
              new_msa[i].options = old_msa[i].options;
              new_msa[i].log_capabilities = old_msa[i].log_capabilities;
              new_msa[i].fc = old_msa[i].fc;
              new_msa[i].fs = old_msa[i].fs;
              new_msa[i].tr = old_msa[i].tr;
              new_msa[i].top_tr_time = old_msa[i].top_tr_time;
              new_msa[i].fr = old_msa[i].fr;
              new_msa[i].top_fr_time = old_msa[i].top_fr_time;
              new_msa[i].ec = old_msa[i].ec;
              new_msa[i].last_data_time = old_msa[i].last_data_time;
              for (j = 0; j < SUM_STORAGE_1; j++)
              {
                 new_msa[i].bytes_send[j] = (double)old_msa[i].bytes_send[j];
                 new_msa[i].bytes_received[j] = (double)old_msa[i].bytes_received[j];
                 new_msa[i].files_send[j] = old_msa[i].files_send[j];
                 new_msa[i].files_received[j] = old_msa[i].files_received[j];
                 new_msa[i].connections[j] = old_msa[i].connections[j];
                 new_msa[i].total_errors[j] = old_msa[i].total_errors[j];
                 new_msa[i].log_bytes_received[j] = (double)old_msa[i].log_bytes_received[j];
              }
              new_msa[i].connect_status = old_msa[i].connect_status;
              new_msa[i].special_flag = old_msa[i].special_flag;
              new_msa[i].afd_switching = old_msa[i].afd_switching;
              new_msa[i].afd_toggle = old_msa[i].afd_toggle;
           }

           ptr = (char *)old_msa;
           ptr -= AFD_WORD_OFFSET_1;

           /*
            * Resize the old MSA to the size of new one and then copy
            * the new structure into it. Then update the MSA version
            * number.
            */
           if ((ptr = mmap_resize(old_msa_fd, ptr, new_size + AFD_WORD_OFFSET_3)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_msa_stat, strerror(errno));
              free((void *)new_msa);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_3;
           (void)memcpy(ptr, new_msa, new_size);
           free((void *)new_msa);
           ptr -= AFD_WORD_OFFSET_3;
           *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           if ((pagesize = (int)sysconf(_SC_PAGESIZE)) == -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to determine the pagesize with sysconf() : %s",
                         strerror(errno));
           }
           *(int *)(ptr + SIZEOF_INT + 4) = pagesize;
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
           *old_msa_size = new_size + AFD_WORD_OFFSET_3;

           system_log(INFO_SIGN, NULL, 0,
                      "Converted MSA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 2) && (new_version == 3))
        {
           int                      pagesize;
           struct mon_status_area_2 *old_msa;
           struct mon_status_area_3 *new_msa;

           /* Get the size of the old MSA file. */
#ifdef HAVE_STATX
           if (statx(old_msa_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                     STATX_SIZE, &stat_buf) == -1)
#else
           if (fstat(old_msa_fd, &stat_buf) == -1)
#endif
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to access %s : %s",
                         old_msa_stat, strerror(errno));
              *old_msa_size = -1;
              return(NULL);
           }
           else
           {
#ifdef HAVE_STATX
              if (stat_buf.stx_size > 0)
#else
              if (stat_buf.st_size > 0)
#endif
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                                 stat_buf.stx_size,
# else
                                 stat_buf.st_size,
# endif
                                 (PROT_READ | PROT_WRITE),
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
                               "Failed to mmap() to %s : %s",
                               old_msa_stat, strerror(errno));
                    *old_msa_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "MSA file %s is empty.", old_msa_stat);
                 *old_msa_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_2;
           old_msa = (struct mon_status_area_2 *)ptr;

           new_size = old_no_of_afds * sizeof(struct mon_status_area_3);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_afds, new_size, strerror(errno));
              ptr = (char *)old_msa;
              ptr -= AFD_WORD_OFFSET_2;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
              if (munmap(ptr, stat_buf.stx_size) == -1)
# else
              if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_msa_stat, strerror(errno));
              }
              *old_msa_size = -1;
              return(NULL);
           }
           (void)memset(ptr, 0, new_size);
           new_msa = (struct mon_status_area_3 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_afds; i++)
           {
              (void)strcpy(new_msa[i].r_work_dir, old_msa[i].r_work_dir);
              for (j = 0; j < MAX_CONVERT_USERNAME_2; j++)
              {
                 (void)strcpy(new_msa[i].convert_username[j][0],
                              old_msa[i].convert_username[j][0]);
                 (void)strcpy(new_msa[i].convert_username[j][1],
                              old_msa[i].convert_username[j][1]);
              }
              (void)memcpy(new_msa[i].afd_alias, old_msa[i].afd_alias,
                           (MAX_AFDNAME_LENGTH_2 + 1));
              (void)strcpy(new_msa[i].hostname[0], old_msa[i].hostname[0]);
              (void)strcpy(new_msa[i].hostname[1], old_msa[i].hostname[1]);
              (void)strcpy(new_msa[i].rcmd, old_msa[i].rcmd);
              (void)strcpy(new_msa[i].afd_version, old_msa[i].afd_version);
              new_msa[i].port[0] = old_msa[i].port[0];
              new_msa[i].port[1] = old_msa[i].port[1];
              new_msa[i].poll_interval = old_msa[i].poll_interval;
              new_msa[i].connect_time = old_msa[i].connect_time;
              new_msa[i].disconnect_time = old_msa[i].disconnect_time;
              new_msa[i].afd_id = get_str_checksum(new_msa[i].afd_alias);
              new_msa[i].amg = old_msa[i].amg;
              new_msa[i].fd = old_msa[i].fd;
              new_msa[i].archive_watch = old_msa[i].archive_watch;
              new_msa[i].jobs_in_queue = old_msa[i].jobs_in_queue;
              new_msa[i].danger_no_of_jobs = old_msa[i].danger_no_of_jobs;
              new_msa[i].no_of_transfers = old_msa[i].no_of_transfers;
              for (j = 0; j < STORAGE_TIME_2; j++)
              {
                 new_msa[i].top_no_of_transfers[j] = old_msa[i].top_no_of_transfers[j];
                 new_msa[i].top_tr[j] = (u_off_t)old_msa[i].top_tr[j];
                 new_msa[i].top_fr[j] = old_msa[i].top_fr[j];
              }
              new_msa[i].top_not_time = old_msa[i].top_not_time;
              new_msa[i].max_connections = old_msa[i].max_connections;
              new_msa[i].sys_log_ec = old_msa[i].sys_log_ec;
              (void)memcpy(new_msa[i].sys_log_fifo, old_msa[i].sys_log_fifo,
                           (LOG_FIFO_SIZE_2 + 1));
              for (j = 0; j < NO_OF_LOG_HISTORY_2; j++)
              {
                 (void)memcpy(new_msa[i].log_history[j], old_msa[i].log_history[j],
                              MAX_LOG_HISTORY_2);
              }
              new_msa[i].host_error_counter = old_msa[i].host_error_counter;
              new_msa[i].no_of_hosts = old_msa[i].no_of_hosts;
              new_msa[i].no_of_dirs = old_msa[i].no_of_dirs;
              new_msa[i].no_of_jobs = old_msa[i].no_of_jobs;
              new_msa[i].options = old_msa[i].options;
              new_msa[i].log_capabilities = old_msa[i].log_capabilities;
              new_msa[i].fc = old_msa[i].fc;
              new_msa[i].fs = old_msa[i].fs;
              new_msa[i].tr = old_msa[i].tr;
              new_msa[i].top_tr_time = old_msa[i].top_tr_time;
              new_msa[i].fr = old_msa[i].fr;
              new_msa[i].top_fr_time = old_msa[i].top_fr_time;
              new_msa[i].ec = old_msa[i].ec;
              new_msa[i].last_data_time = old_msa[i].last_data_time;
              for (j = 0; j < SUM_STORAGE_2; j++)
              {
                 new_msa[i].bytes_send[j] = (double)old_msa[i].bytes_send[j];
                 new_msa[i].bytes_received[j] = (double)old_msa[i].bytes_received[j];
                 new_msa[i].files_send[j] = old_msa[i].files_send[j];
                 new_msa[i].files_received[j] = old_msa[i].files_received[j];
                 new_msa[i].connections[j] = old_msa[i].connections[j];
                 new_msa[i].total_errors[j] = old_msa[i].total_errors[j];
                 new_msa[i].log_bytes_received[j] = (double)old_msa[i].log_bytes_received[j];
              }
              new_msa[i].connect_status = old_msa[i].connect_status;
              new_msa[i].special_flag = old_msa[i].special_flag;
              new_msa[i].afd_switching = old_msa[i].afd_switching;
              new_msa[i].afd_toggle = old_msa[i].afd_toggle;
           }

           ptr = (char *)old_msa;
           ptr -= AFD_WORD_OFFSET_2;

           /*
            * Resize the old MSA to the size of new one and then copy
            * the new structure into it. Then update the MSA version
            * number.
            */
           if ((ptr = mmap_resize(old_msa_fd, ptr, new_size + AFD_WORD_OFFSET_3)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_msa_stat, strerror(errno));
              free((void *)new_msa);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_3;
           (void)memcpy(ptr, new_msa, new_size);
           free((void *)new_msa);
           ptr -= AFD_WORD_OFFSET_3;
           *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           if ((pagesize = (int)sysconf(_SC_PAGESIZE)) == -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to determine the pagesize with sysconf() : %s",
                         strerror(errno));
           }
           *(int *)(ptr + SIZEOF_INT + 4) = pagesize;
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
           *old_msa_size = new_size + AFD_WORD_OFFSET_3;

           system_log(INFO_SIGN, NULL, 0,
                      "Converted MSA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
        else
        {
           system_log(ERROR_SIGN, NULL, 0,
                      "Don't know how to convert a version %d MSA to version %d.",
                      old_version, new_version);
           ptr = NULL;
        }

   return(ptr);
}
