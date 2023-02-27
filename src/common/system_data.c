/*
 *  system_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2013 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_system_data   - read all system data values
 **   write_system_data - stores some important values of AFD into
 **                       a machine independent format
 **
 ** SYNOPSIS
 **   void write_system_data(struct afd_status *p_afd_status,
 **                          int               fsa_feature_flag,
 **                          int               fra_feature_flag)
 **
 ** DESCRIPTION
 **   The function write_system_data stores some important values
 **   of AFD into a machine independent format (text file). This can
 **   be useful when the AFD starts, but was compiled with different
 **   values making the internal format incompatible.
 **
 ** RETURN VALUES
 **   None for get_system_data(). write_system_data() returns SUCCESS
 **   when data was written, otherwise INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   07.09.2013 H.Kiehl Created
 **   12.01.2017 H.Kiehl Don't just check if the *_log_fifo values are
 **                      correct in get_system_data(), do the same check
 **                      for *_log_history values.
 **   26.02.2023 H.Kiehl Added a return value for write_system_data().
 **
 */
DESCR__E_M3

#include <stdio.h>               /* stderr, NULL, fclose()               */
#include <string.h>              /* strerror()                           */
#include <stdlib.h>              /* exit(), atoi()                       */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>               /* open()                               */
#ifdef HAVE_MMAP
# include <sys/mman.h>           /* munmap()                             */
#endif
#include <unistd.h>              /* close()                              */
#include <errno.h>
#include "version.h"

#define FSA_FEATURE_FLAG_NAME       "FSA_FEATURE_FLAG"
#define FRA_FEATURE_FLAG_NAME       "FRA_FEATURE_FLAG"
#define AMG_FORK_COUNTER_NAME       "AMG_FORK_COUNTER"
#define FD_FORK_COUNTER_NAME        "FD_FORK_COUNTER"
#define BURST2_COUNTER_NAME         "BURST2_COUNTER"
#define AMG_CHILD_USER_TIME_NAME    "AMG_CHILD_USER_TIME"
#define AMG_CHILD_SYSTEM_TIME_NAME  "AMG_CHILD_SYSTEM_TIME"
#define FD_CHILD_USER_TIME_NAME     "FD_CHILD_USER_TIME"
#define FD_CHILD_SYSTEM_TIME_NAME   "FD_CHILD_SYSTEM_TIME"
#define MAX_FD_QUEUE_LENGTH_NAME    "MAX_FD_QUEUE_LENGTH"
#define DIRS_SCANNED_NAME           "DIRS_SCANNED"
#define INOTIFY_EVENTS_NAME         "INOTIFY_EVENTS"
#define RECEIVE_LOG_INDICATOR_NAME  "RECEIVE_LOG_INDICATOR"
#define RECEIVE_LOG_HISTORY_NAME    "RECEIVE_LOG_HISTORY"
#define SYSTEM_LOG_INDICATOR_NAME   "SYSTEM_LOG_INDICATOR"
#define SYSTEM_LOG_HISTORY_NAME     "SYSTEM_LOG_HISTORY"
#define TRANSFER_LOG_INDICATOR_NAME "TRANSFER_LOG_INDICATOR"
#define TRANSFER_LOG_HISTORY_NAME   "TRANSFER_LOG_HISTORY"
#define MAX_VAR_STR_LENGTH          23

/* External global variables. */
extern char                       *p_work_dir;


/*########################## get_system_data() ##########################*/
int
get_system_data(struct system_data *sd)
{
   int  fd,
        ret = SUCCESS;
   char sysdata_filename[MAX_PATH_LENGTH];

   (void)snprintf(sysdata_filename, MAX_PATH_LENGTH, "%s%s%s",
                  p_work_dir, FIFO_DIR, SYSTEM_DATA_FILE);
   if ((fd = open(sysdata_filename, O_RDONLY)) == -1)
   {
      if (errno != ENOENT)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to open() `%s' : %s",
                    sysdata_filename, strerror(errno));
      }
      ret = INCORRECT;
   }
   else
   {
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat stat_buf;
#endif

#ifdef HAVE_STATX
      if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(fd, &stat_buf) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                    "Failed to statx() `%s' : %s",
#else
                    "Failed to fstat() `%s' : %s",
#endif
                    sysdata_filename, strerror(errno));
         ret = INCORRECT;
      }
      else
      {
#ifdef HAVE_STATX
         if (stat_buf.stx_size > 0)
#else
         if (stat_buf.st_size > 0)
#endif
         {
            char *ptr;

#ifdef HAVE_MMAP
            if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                            stat_buf.stx_size, PROT_READ, MAP_SHARED,
# else
                            stat_buf.st_size, PROT_READ, MAP_SHARED,
# endif
                            fd, 0)) == (caddr_t) -1)
#else
            if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                                stat_buf.stx_size, PROT_READ,
# else
                                stat_buf.st_size, PROT_READ,
# endif
                                MAP_SHARED, sysdata_filename,
                                0)) == (caddr_t) -1)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "mmap() error : %s", strerror(errno));
               ret = INCORRECT;
            }
            else
            {
               int  i, j, k;
               char *p_start = ptr,
#if MAX_LOG_HISTORY > MAX_TIME_T_LENGTH
                    val_str[MAX_LOG_HISTORY + 1],
#else
                    val_str[MAX_TIME_T_LENGTH + 1],
#endif
                    *varlist[] =
                    {
                       FSA_FEATURE_FLAG_NAME,       /*  0 */
                       FRA_FEATURE_FLAG_NAME,       /*  1 */
                       AMG_FORK_COUNTER_NAME,       /*  2 */
                       FD_FORK_COUNTER_NAME,        /*  3 */
                       BURST2_COUNTER_NAME,         /*  4 */
                       MAX_FD_QUEUE_LENGTH_NAME,    /*  5 */
                       DIRS_SCANNED_NAME,           /*  6 */
                       INOTIFY_EVENTS_NAME,         /*  7 */
                       RECEIVE_LOG_INDICATOR_NAME,  /*  8 */
                       RECEIVE_LOG_HISTORY_NAME,    /*  9 */
                       SYSTEM_LOG_INDICATOR_NAME,   /* 10 */
                       SYSTEM_LOG_HISTORY_NAME,     /* 11 */
                       TRANSFER_LOG_INDICATOR_NAME, /* 12 */
#ifdef HAVE_WAIT4
                       TRANSFER_LOG_HISTORY_NAME,   /* 13 */
                       AMG_CHILD_USER_TIME_NAME,    /* 14 */
                       AMG_CHILD_SYSTEM_TIME_NAME,  /* 15 */
                       FD_CHILD_USER_TIME_NAME,     /* 16 */
                       FD_CHILD_SYSTEM_TIME_NAME    /* 17 */
#else
                       TRANSFER_LOG_HISTORY_NAME    /* 13 */
#endif
                    },
                    var_str[MAX_VAR_STR_LENGTH + 1];

               do
               {
                  if (*ptr == '#')
                  {
                     while ((*ptr != '\n') && (*ptr != '\r') &&
#ifdef HAVE_STATX
                            ((ptr - p_start) < stat_buf.stx_size)
#else
                            ((ptr - p_start) < stat_buf.st_size)
#endif
                           )
                     {
                        ptr++;
                     }
                     while (((*ptr == '\n') || (*ptr == '\r')) &&
#ifdef HAVE_STATX
                            ((ptr - p_start) < stat_buf.stx_size)
#else
                            ((ptr - p_start) < stat_buf.st_size)
#endif
                           )
                     {
                        ptr++;
                     }
                  }
                  else
                  {
                     i = 0;
                     while ((*ptr != '|') && (i < MAX_VAR_STR_LENGTH) &&
#ifdef HAVE_STATX
                            ((ptr - p_start) < stat_buf.stx_size)
#else
                            ((ptr - p_start) < stat_buf.st_size)
#endif
                           )
                     {
                        var_str[i] = *ptr;
                        ptr++; i++;
                     }
                     if (*ptr == '|')
                     {
                        var_str[i] = '\0';
                        for (j = 0; j < (sizeof(varlist) / sizeof(*varlist)); j++)
                        {
                           if (my_strcmp(var_str, varlist[j]) == 0)
                           {
                              ptr++;
                              k = 0;
                              while ((*ptr != '\n') && (*ptr != '\r') &&
                                     (*ptr != '.') && (*ptr != '|') &&
#if MAX_LOG_HISTORY > MAX_TIME_T_LENGTH
                                     (k < MAX_LOG_HISTORY) &&
#else
                                     (k < MAX_TIME_T_LENGTH) &&
#endif
#ifdef HAVE_STATX
                                     ((ptr - p_start) < stat_buf.stx_size)
#else
                                     ((ptr - p_start) < stat_buf.st_size)
#endif
                                    )
                              {
                                 val_str[k] = *ptr;
                                 ptr++; k++;
                              }
                              if ((*ptr == '\n') || (*ptr == '\r') ||
                                  (*ptr == '.') || (*ptr == '|'))
                              {
                                 if (k > 0)
                                 {
                                    val_str[k] = '\0';
                                    switch (j)
                                    {
                                       case  0 : /* FSA_FEATURE_FLAG */
                                          sd->fsa_feature_flag = (unsigned char)atoi(val_str);
                                          break;

                                       case  1 : /* FRA_FEATURE_FLAG */
                                          sd->fra_feature_flag = (unsigned char)atoi(val_str);
                                          break;

                                       case  2 : /* AMG_FORK_COUNTER */
                                          sd->amg_fork_counter = (unsigned int)atoi(val_str);
                                          break;

                                       case  3 : /* FD_FORK_COUNTER */
                                          sd->fd_fork_counter = (unsigned int)atoi(val_str);
                                          break;

                                       case  4 : /* BURST2_COUNTER */
                                          sd->burst2_counter = (unsigned int)atoi(val_str);
                                          break;

                                       case  5 : /* MAX_FD_QUEUE_LENGTH */
                                          sd->max_queue_length = (unsigned int)atoi(val_str);
                                          break;

                                       case  6 : /* DIRS_SCANNED */
                                          sd->dir_scans = (unsigned int)atoi(val_str);
                                          break;

                                       case  7 : /* INOTIFY_EVENTS */
#ifdef WITH_INOTIFY
                                          sd->inotify_events = (unsigned int)atoi(val_str);
#endif
                                          break;

                                       case  8 : /* RECEIVE_LOG_INDICATOR */
                                          sd->receive_log_ec = (unsigned int)atoi(val_str);
                                          if (*ptr == '|')
                                          {
                                             i = 0;
                                             while ((*ptr != '\n') &&
                                                    (*ptr != '\r') &&
                                                    (i < LOG_FIFO_SIZE) &&
#ifdef HAVE_STATX
                                                    ((ptr - p_start) < stat_buf.stx_size)
#else
                                                    ((ptr - p_start) < stat_buf.st_size)
#endif
                                                   )
                                             {
                                                switch (*ptr)
                                                {
                                                   case 'I' :
                                                      sd->receive_log_fifo[i] = INFO_ID;
                                                      break;
                                                   case 'E' :
                                                      sd->receive_log_fifo[i] = ERROR_ID;
                                                      break;
                                                   case 'W' :
                                                      sd->receive_log_fifo[i] = WARNING_ID;
                                                      break;
                                                   case 'F' :
                                                      sd->receive_log_fifo[i] = FAULTY_ID;
                                                      break;
                                                   default :
                                                      sd->receive_log_fifo[i] = NO_INFORMATION;
                                                      break;
                                                }
                                                ptr++; i++;
                                             }
                                             if ((*ptr != '\n') &&
                                                 (*ptr != '\r'))
                                             {
                                                while ((*ptr != '\n') &&
                                                       (*ptr != '\r') &&
#ifdef HAVE_STATX
                                                       ((ptr - p_start) < stat_buf.stx_size)
#else
                                                       ((ptr - p_start) < stat_buf.st_size)
#endif
                                                      )
                                                {
                                                   ptr++;
                                                }
                                             }
                                          }
                                          break;

                                       case  9 : /* RECEIVE_LOG_HISTORY */
                                          i = 0;
                                          while (i < k)
                                          {
                                             switch (val_str[i])
                                             {
                                                case 'I' :
                                                   sd->receive_log_history[i] = INFO_ID;
                                                   break;
                                                case 'E' :
                                                   sd->receive_log_history[i] = ERROR_ID;
                                                   break;
                                                case 'W' :
                                                   sd->receive_log_history[i] = WARNING_ID;
                                                   break;
                                                case 'F' :
                                                   sd->receive_log_history[i] = FAULTY_ID;
                                                   break;
                                                default :
                                                   sd->receive_log_history[i] = NO_INFORMATION;
                                                   break;
                                             }
                                             i++;
                                          }
                                          if ((*ptr != '\n') &&
                                              (*ptr != '\r'))
                                          {
                                             while ((*ptr != '\n') &&
                                                    (*ptr != '\r') &&
#ifdef HAVE_STATX
                                                    ((ptr - p_start) < stat_buf.stx_size)
#else
                                                    ((ptr - p_start) < stat_buf.st_size)
#endif
                                                   )
                                             {
                                                ptr++;
                                             }
                                          }
                                          break;

                                       case 10 : /* SYSTEM_LOG_INDICATOR */
                                          sd->sys_log_ec = (unsigned int)atoi(val_str);
                                          if (*ptr == '|')
                                          {
                                             i = 0;
                                             while ((*ptr != '\n') &&
                                                    (*ptr != '\r') &&
                                                    (i < LOG_FIFO_SIZE) &&
#ifdef HAVE_STATX
                                                    ((ptr - p_start) < stat_buf.stx_size)
#else
                                                    ((ptr - p_start) < stat_buf.st_size)
#endif
                                                   )
                                             {
                                                switch (*ptr)
                                                {
                                                   case 'I' :
                                                      sd->sys_log_fifo[i] = INFO_ID;
                                                      break;
                                                   case 'E' :
                                                      sd->sys_log_fifo[i] = ERROR_ID;
                                                      break;
                                                   case 'W' :
                                                      sd->sys_log_fifo[i] = WARNING_ID;
                                                      break;
                                                   case 'C' :
                                                      sd->sys_log_fifo[i] = CONFIG_ID;
                                                      break;
                                                   case 'F' :
                                                      sd->sys_log_fifo[i] = FAULTY_ID;
                                                      break;
                                                   default :
                                                      sd->sys_log_fifo[i] = NO_INFORMATION;
                                                      break;
                                                }
                                                ptr++; i++;
                                             }
                                             if ((*ptr != '\n') &&
                                                 (*ptr != '\r'))
                                             {
                                                while ((*ptr != '\n') &&
                                                       (*ptr != '\r') &&
#ifdef HAVE_STATX
                                                       ((ptr - p_start) < stat_buf.stx_size)
#else
                                                       ((ptr - p_start) < stat_buf.st_size)
#endif
                                                      )
                                                {
                                                   ptr++;
                                                }
                                             }
                                          }
                                          break;

                                       case 11 : /* SYSTEM_LOG_HISTORY */
                                          i = 0;
                                          while (i < k)
                                          {
                                             switch (val_str[i])
                                             {
                                                case 'I' :
                                                   sd->sys_log_history[i] = INFO_ID;
                                                   break;
                                                case 'E' :
                                                   sd->sys_log_history[i] = ERROR_ID;
                                                   break;
                                                case 'W' :
                                                   sd->sys_log_history[i] = WARNING_ID;
                                                   break;
                                                case 'C' :
                                                   sd->sys_log_history[i] = CONFIG_ID;
                                                   break;
                                                case 'F' :
                                                   sd->sys_log_history[i] = FAULTY_ID;
                                                   break;
                                                default :
                                                   sd->sys_log_history[i] = NO_INFORMATION;
                                                   break;
                                             }
                                             i++;
                                          }
                                          if ((*ptr != '\n') &&
                                              (*ptr != '\r'))
                                          {
                                             while ((*ptr != '\n') &&
                                                    (*ptr != '\r') &&
#ifdef HAVE_STATX
                                                    ((ptr - p_start) < stat_buf.stx_size)
#else
                                                    ((ptr - p_start) < stat_buf.st_size)
#endif
                                                   )
                                             {
                                                ptr++;
                                             }
                                          }
                                          break;

                                       case 12 : /* TRANSFER_LOG_INDICATOR */
                                          sd->trans_log_ec = (unsigned int)atoi(val_str);
                                          if (*ptr == '|')
                                          {
                                             i = 0;
                                             while ((*ptr != '\n') &&
                                                    (*ptr != '\r') &&
                                                    (i < LOG_FIFO_SIZE) &&
#ifdef HAVE_STATX
                                                    ((ptr - p_start) < stat_buf.stx_size)
#else
                                                    ((ptr - p_start) < stat_buf.st_size)
#endif
                                                   )
                                             {
                                                switch (*ptr)
                                                {
                                                   case 'I' :
                                                      sd->trans_log_fifo[i] = INFO_ID;
                                                      break;
                                                   case 'E' :
                                                      sd->trans_log_fifo[i] = ERROR_ID;
                                                      break;
                                                   case 'W' :
                                                      sd->trans_log_fifo[i] = WARNING_ID;
                                                      break;
                                                   case 'O' :
                                                      sd->trans_log_fifo[i] = ERROR_OFFLINE_ID;
                                                      break;
                                                   case 'F' :
                                                      sd->trans_log_fifo[i] = FAULTY_ID;
                                                      break;
                                                   default :
                                                      sd->trans_log_fifo[i] = NO_INFORMATION;
                                                      break;
                                                }
                                                ptr++; i++;
                                             }
                                             if ((*ptr != '\n') &&
                                                 (*ptr != '\r'))
                                             {
                                                while ((*ptr != '\n') &&
                                                       (*ptr != '\r') &&
#ifdef HAVE_STATX
                                                       ((ptr - p_start) < stat_buf.stx_size)
#else
                                                       ((ptr - p_start) < stat_buf.st_size)
#endif
                                                      )
                                                {
                                                   ptr++;
                                                }
                                             }
                                          }
                                          break;

                                       case 13 : /* TRANSFER_LOG_HISTORY */
                                          i = 0;
                                          while (i < k)
                                          {
                                             switch (val_str[i])
                                             {
                                                case 'I' :
                                                   sd->trans_log_history[i] = INFO_ID;
                                                   break;
                                                case 'E' :
                                                   sd->trans_log_history[i] = ERROR_ID;
                                                   break;
                                                case 'W' :
                                                   sd->trans_log_history[i] = WARNING_ID;
                                                   break;
                                                case 'O' :
                                                   sd->trans_log_history[i] = ERROR_OFFLINE_ID;
                                                   break;
                                                case 'F' :
                                                   sd->trans_log_history[i] = FAULTY_ID;
                                                   break;
                                                default :
                                                   sd->trans_log_history[i] = NO_INFORMATION;
                                                   break;
                                             }
                                             i++;
                                          }
                                          if ((*ptr != '\n') &&
                                              (*ptr != '\r'))
                                          {
                                             while ((*ptr != '\n') &&
                                                    (*ptr != '\r') &&
#ifdef HAVE_STATX
                                                    ((ptr - p_start) < stat_buf.stx_size)
#else
                                                    ((ptr - p_start) < stat_buf.st_size)
#endif
                                                   )
                                             {
                                                ptr++;
                                             }
                                          }
                                          break;

                                       case 14 : /* AMG_CHILD_USER_TIME */
#ifdef HAVE_WAIT4
# ifdef HAVE_STRTOULL
#  if SIZEOF_TIME_T == 4
                                          if ((sd->amg_child_utime.tv_sec = (time_t)strtoul(val_str, NULL, 10)) == ULONG_MAX)
#  else
                                          if ((sd->amg_child_utime.tv_sec = (time_t)strtoull(val_str, NULL, 10)) == ULLONG_MAX)
#  endif
# else
                                          if ((sd->amg_child_utime.tv_sec = (time_t)strtoul(val_str, NULL, 10)) == ULONG_MAX)
# endif
                                          {
                                             system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                        _("Value to large for %s, setting 0"),
                                                        varlist[j]);
                                             sd->amg_child_utime.tv_sec = 0;
                                          }
                                          if (*ptr == '.')
                                          {
                                             i = 0;
                                             while ((*ptr != '\n') &&
                                                    (*ptr != '\r') &&
                                                    (i < MAX_VAR_STR_LENGTH) &&
# ifdef HAVE_STATX
                                                    ((ptr - p_start) < stat_buf.stx_size)
# else
                                                    ((ptr - p_start) < stat_buf.st_size)
# endif
                                                   )
                                             {
                                                var_str[i] = *ptr;
                                                ptr++; i++;
                                             }
                                             if ((*ptr == '\n') ||
                                                 (*ptr == '\r'))
                                             {
                                                if (k > 0)
                                                {
                                                   val_str[k] = '\0';
# ifdef HAVE_STRTOULL
#  if SIZEOF_SUSECONDS_T == 4
                                                   if ((sd->amg_child_utime.tv_usec = (suseconds_t)strtoul(val_str, NULL, 10)) == ULONG_MAX)
#  else
                                                   if ((sd->amg_child_utime.tv_usec = (suseconds_t)strtoull(val_str, NULL, 10)) == ULLONG_MAX)
#  endif
# else
                                                   if ((sd->amg_child_utime.tv_usec = (suseconds_t)strtoul(val_str, NULL, 10)) == ULONG_MAX)
# endif
                                                   {
                                                      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                                 _("Value to large for %s, setting 0"),
                                                                 varlist[j]);
                                                      sd->amg_child_utime.tv_usec = 0;
                                                   }
                                                }
                                             }
                                             else
                                             {
                                                while ((*ptr != '\n') &&
                                                       (*ptr != '\r') &&
# ifdef HAVE_STATX
                                                       ((ptr - p_start) < stat_buf.stx_size)
# else
                                                       ((ptr - p_start) < stat_buf.st_size)
# endif
                                                      )
                                                {
                                                   ptr++;
                                                }
                                             }
                                          }
                                          else
                                          {
                                             /* Go to end of line. */
                                             while ((*ptr != '\n') &&
                                                    (*ptr != '\r') &&
# ifdef HAVE_STATX
                                                    ((ptr - p_start) < stat_buf.stx_size)
# else
                                                    ((ptr - p_start) < stat_buf.st_size)
# endif
                                                   )
                                             {
                                                ptr++;
                                             }
                                          }
#endif
                                          break;

                                       case 15 : /* AMG_CHILD_SYSTEM_TIME */
#ifdef HAVE_WAIT4
# ifdef HAVE_STRTOULL
#  if SIZEOF_TIME_T == 4
                                          if ((sd->amg_child_stime.tv_sec = (time_t)strtoul(val_str, NULL, 10)) == ULONG_MAX)
#  else
                                          if ((sd->amg_child_stime.tv_sec = (time_t)strtoull(val_str, NULL, 10)) == ULLONG_MAX)
#  endif
# else
                                          if ((sd->amg_child_stime.tv_sec = (time_t)strtoul(val_str, NULL, 10)) == ULONG_MAX)
# endif
                                          {
                                             system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                        _("Value to large for %s, setting 0"),
                                                        varlist[j]);
                                             sd->amg_child_stime.tv_sec = 0;
                                          }
                                          if (*ptr == '.')
                                          {
                                             i = 0;
                                             while ((*ptr != '\n') &&
                                                    (*ptr != '\r') &&
                                                    (i < MAX_VAR_STR_LENGTH) &&
# ifdef HAVE_STATX
                                                    ((ptr - p_start) < stat_buf.stx_size)
# else
                                                    ((ptr - p_start) < stat_buf.st_size)
# endif
                                                   )
                                             {
                                                var_str[i] = *ptr;
                                                ptr++; i++;
                                             }
                                             if ((*ptr == '\n') ||
                                                 (*ptr == '\r'))
                                             {
                                                if (k > 0)
                                                {
                                                   val_str[k] = '\0';
# ifdef HAVE_STRTOULL
#  if SIZEOF_SUSECONDS_T == 4
                                                   if ((sd->amg_child_stime.tv_usec = (suseconds_t)strtoul(val_str, NULL, 10)) == ULONG_MAX)
#  else
                                                   if ((sd->amg_child_stime.tv_usec = (suseconds_t)strtoull(val_str, NULL, 10)) == ULLONG_MAX)
#  endif
# else
                                                   if ((sd->amg_child_stime.tv_usec = (suseconds_t)strtoul(val_str, NULL, 10)) == ULONG_MAX)
# endif
                                                   {
                                                      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                                 _("Value to large for %s, setting 0"),
                                                                 varlist[j]);
                                                      sd->amg_child_stime.tv_usec = 0;
                                                   }
                                                }
                                             }
                                             else
                                             {
                                                while ((*ptr != '\n') &&
                                                       (*ptr != '\r') &&
# ifdef HAVE_STATX
                                                       ((ptr - p_start) < stat_buf.stx_size)
# else
                                                       ((ptr - p_start) < stat_buf.st_size)
# endif
                                                      )
                                                {
                                                   ptr++;
                                                }
                                             }
                                          }
                                          else
                                          {
                                             /* Go to end of line. */
                                             while ((*ptr != '\n') &&
                                                    (*ptr != '\r') &&
# ifdef HAVE_STATX
                                                    ((ptr - p_start) < stat_buf.stx_size)
# else
                                                    ((ptr - p_start) < stat_buf.st_size)
# endif
                                                   )
                                             {
                                                ptr++;
                                             }
                                          }
#endif
                                          break;

                                       case 16 : /* FD_CHILD_USER_TIME */
#ifdef HAVE_WAIT4
# ifdef HAVE_STRTOULL
#  if SIZEOF_TIME_T == 4
                                          if ((sd->fd_child_utime.tv_sec = (time_t)strtoul(val_str, NULL, 10)) == ULONG_MAX)
#  else
                                          if ((sd->fd_child_utime.tv_sec = (time_t)strtoull(val_str, NULL, 10)) == ULLONG_MAX)
#  endif
# else
                                          if ((sd->fd_child_utime.tv_sec = (time_t)strtoul(val_str, NULL, 10)) == ULONG_MAX)
# endif
                                          {
                                             system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                        _("Value to large for %s, setting 0"),
                                                        varlist[j]);
                                             sd->fd_child_utime.tv_sec = 0;
                                          }
                                          if (*ptr == '.')
                                          {
                                             i = 0;
                                             while ((*ptr != '\n') &&
                                                    (*ptr != '\r') &&
                                                    (i < MAX_VAR_STR_LENGTH) &&
# ifdef HAVE_STATX
                                                    ((ptr - p_start) < stat_buf.stx_size)
# else
                                                    ((ptr - p_start) < stat_buf.st_size)
# endif
                                                   )
                                             {
                                                var_str[i] = *ptr;
                                                ptr++; i++;
                                             }
                                             if ((*ptr == '\n') ||
                                                 (*ptr == '\r'))
                                             {
                                                if (k > 0)
                                                {
                                                   val_str[k] = '\0';
# ifdef HAVE_STRTOULL
#  if SIZEOF_SUSECONDS_T == 4
                                                   if ((sd->fd_child_utime.tv_usec = (suseconds_t)strtoul(val_str, NULL, 10)) == ULONG_MAX)
#  else
                                                   if ((sd->fd_child_utime.tv_usec = (suseconds_t)strtoull(val_str, NULL, 10)) == ULLONG_MAX)
#  endif
# else
                                                   if ((sd->fd_child_utime.tv_usec = (suseconds_t)strtoul(val_str, NULL, 10)) == ULONG_MAX)
# endif
                                                   {
                                                      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                                 _("Value to large for %s, setting 0"),
                                                                 varlist[j]);
                                                      sd->fd_child_utime.tv_usec = 0;
                                                   }
                                                }
                                             }
                                             else
                                             {
                                                while ((*ptr != '\n') &&
                                                       (*ptr != '\r') &&
# ifdef HAVE_STATX
                                                       ((ptr - p_start) < stat_buf.stx_size)
# else
                                                       ((ptr - p_start) < stat_buf.st_size)
# endif
                                                      )
                                                {
                                                   ptr++;
                                                }
                                             }
                                          }
                                          else
                                          {
                                             /* Go to end of line. */
                                             while ((*ptr != '\n') &&
                                                    (*ptr != '\r') &&
# ifdef HAVE_STATX
                                                    ((ptr - p_start) < stat_buf.stx_size)
# else
                                                    ((ptr - p_start) < stat_buf.st_size)
# endif
                                                   )
                                             {
                                                ptr++;
                                             }
                                          }
#endif
                                          break;

                                       case 17 : /* FD_CHILD_SYSTEM_TIME */
#ifdef HAVE_WAIT4
# ifdef HAVE_STRTOULL
#  if SIZEOF_TIME_T == 4
                                          if ((sd->fd_child_stime.tv_sec = (time_t)strtoul(val_str, NULL, 10)) == ULONG_MAX)
#  else
                                          if ((sd->fd_child_stime.tv_sec = (time_t)strtoull(val_str, NULL, 10)) == ULLONG_MAX)
#  endif
# else
                                          if ((sd->fd_child_stime.tv_sec = (time_t)strtoul(val_str, NULL, 10)) == ULONG_MAX)
# endif
                                          {
                                             system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                        _("Value to large for %s, setting 0"),
                                                        varlist[j]);
                                             sd->fd_child_stime.tv_sec = 0;
                                          }
                                          if (*ptr == '.')
                                          {
                                             i = 0;
                                             while ((*ptr != '\n') &&
                                                    (*ptr != '\r') &&
                                                    (i < MAX_VAR_STR_LENGTH) &&
# ifdef HAVE_STATX
                                                    ((ptr - p_start) < stat_buf.stx_size)
# else
                                                    ((ptr - p_start) < stat_buf.st_size)
# endif
                                                   )
                                             {
                                                var_str[i] = *ptr;
                                                ptr++; i++;
                                             }
                                             if ((*ptr == '\n') ||
                                                 (*ptr == '\r'))
                                             {
                                                if (k > 0)
                                                {
                                                   val_str[k] = '\0';
# ifdef HAVE_STRTOULL
#  if SIZEOF_SUSECONDS_T == 4
                                                   if ((sd->fd_child_stime.tv_usec = (suseconds_t)strtoul(val_str, NULL, 10)) == ULONG_MAX)
#  else
                                                   if ((sd->fd_child_stime.tv_usec = (suseconds_t)strtoull(val_str, NULL, 10)) == ULLONG_MAX)
#  endif
# else
                                                   if ((sd->fd_child_stime.tv_usec = (suseconds_t)strtoul(val_str, NULL, 10)) == ULONG_MAX)
# endif
                                                   {
                                                      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                                 _("Value to large for %s, setting 0"),
                                                                 varlist[j]);
                                                      sd->fd_child_stime.tv_usec = 0;
                                                   }
                                                }
                                             }
                                             else
                                             {
                                                while ((*ptr != '\n') &&
                                                       (*ptr != '\r') &&
# ifdef HAVE_STATX
                                                       ((ptr - p_start) < stat_buf.stx_size)
# else
                                                       ((ptr - p_start) < stat_buf.st_size)
# endif
                                                      )
                                                {
                                                   ptr++;
                                                }
                                             }
                                          }
                                          else
                                          {
                                             /* Go to end of line. */
                                             while ((*ptr != '\n') &&
                                                    (*ptr != '\r') &&
# ifdef HAVE_STATX
                                                    ((ptr - p_start) < stat_buf.stx_size)
# else
                                                    ((ptr - p_start) < stat_buf.st_size)
# endif
                                                   )
                                             {
                                                ptr++;
                                             }
                                          }
#endif
                                          break;

                                       default : /* Not known. */
                                                 system_log(WARN_SIGN, __FILE__, __LINE__,
                                                            "Programmer needs to extend the code. Please contact maintainer: %s",
                                                            AFD_MAINTAINER);
                                                 break;
                                    }
                                 }
                              }
                              else
                              {
                                 while ((*ptr != '\n') && (*ptr != '\r') &&
#ifdef HAVE_STATX
                                        ((ptr - p_start) < stat_buf.stx_size)
#else
                                        ((ptr - p_start) < stat_buf.st_size)
#endif
                                       )
                                 {
                                    ptr++;
                                 }
                              }
                              while (((*ptr == '\n') || (*ptr == '\r')) &&
#ifdef HAVE_STATX
                                     ((ptr - p_start) < stat_buf.stx_size)
#else
                                     ((ptr - p_start) < stat_buf.st_size)
#endif
                                    )
                              {
                                 ptr++;
                              }

                              break;
                           }
                        }
                     }
                     else
                     {
                        /* Go to end of line. */
                        while ((*ptr != '\n') && (*ptr != '\r') &&
#ifdef HAVE_STATX
                               ((ptr - p_start) < stat_buf.stx_size)
#else
                               ((ptr - p_start) < stat_buf.st_size)
#endif
                              )
                        {
                           ptr++;
                        }
                        while (((*ptr == '\n') || (*ptr == '\r')) &&
#ifdef HAVE_STATX
                               ((ptr - p_start) < stat_buf.stx_size)
#else
                               ((ptr - p_start) < stat_buf.st_size)
#endif
                              )
                        {
                           ptr++;
                        }
                     }
                  }
#ifdef HAVE_STATX
               } while ((ptr - p_start) < stat_buf.stx_size);
#else
               } while ((ptr - p_start) < stat_buf.st_size);
#endif

               /* Unmap from file. */
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
               if (munmap(p_start, stat_buf.stx_size) == -1)
# else
               if (munmap(p_start, stat_buf.st_size) == -1)
# endif
#else
               if (munmap_emu(p_start) == -1)
#endif
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to munmap() `%s' : %s",
                             sysdata_filename, strerror(errno));
               }
            }
         }
         if (close(fd) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to close() `%s' : %s",
                       sysdata_filename, strerror(errno));
         }
      }
   }

   return(ret);
}


/*######################## write_system_data() ##########################*/
int
write_system_data(struct afd_status *p_afd_status,
                  int               fsa_feature_flag,
                  int               fra_feature_flag)
{
   int  fd,
        i;
   FILE *fp;
   char idata_filename[MAX_PATH_LENGTH];

   (void)snprintf(idata_filename, MAX_PATH_LENGTH, "%s%s%s",
                  p_work_dir, FIFO_DIR, SYSTEM_DATA_FILE);
   if ((fd = open(idata_filename, (O_RDWR | O_CREAT | O_TRUNC),
                  (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to open() `%s' : %s",
                 idata_filename, strerror(errno));
      return(INCORRECT);
   }
   if ((fp = fdopen(fd, "w")) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to fdopen() `%s' : %s",
                 idata_filename, strerror(errno));
      (void)close(fd);
      return(INCORRECT);
   }
   (void)fprintf(fp, "# NOTE: Do not edit this file!!!!\n");
   (void)fprintf(fp, "%s|%d\n",
                 FSA_FEATURE_FLAG_NAME, fsa_feature_flag);
   (void)fprintf(fp, "%s|%d\n",
                 FRA_FEATURE_FLAG_NAME, fra_feature_flag);

   (void)fprintf(fp, "%s|%d\n",
                 AMG_FORK_COUNTER_NAME, p_afd_status->amg_fork_counter);
   (void)fprintf(fp, "%s|%d\n",
                 FD_FORK_COUNTER_NAME, p_afd_status->fd_fork_counter);
   (void)fprintf(fp, "%s|%d\n",
                 BURST2_COUNTER_NAME, p_afd_status->burst2_counter);
#ifdef HAVE_WAIT4
   (void)fprintf(fp, "%s|%ld.%ld\n",
                 AMG_CHILD_USER_TIME_NAME,
                 p_afd_status->amg_child_utime.tv_sec,
                 p_afd_status->amg_child_utime.tv_usec);
   (void)fprintf(fp, "%s|%ld.%ld\n",
                 AMG_CHILD_SYSTEM_TIME_NAME,
                 p_afd_status->amg_child_stime.tv_sec,
                 p_afd_status->amg_child_stime.tv_usec);
   (void)fprintf(fp, "%s|%ld.%ld\n",
                 FD_CHILD_USER_TIME_NAME,
                 p_afd_status->fd_child_utime.tv_sec,
                 p_afd_status->fd_child_utime.tv_usec);
   (void)fprintf(fp, "%s|%ld.%ld\n",
                 FD_CHILD_SYSTEM_TIME_NAME,
                 p_afd_status->fd_child_stime.tv_sec,
                 p_afd_status->fd_child_stime.tv_usec);
#endif
   (void)fprintf(fp, "%s|%u\n",
                 MAX_FD_QUEUE_LENGTH_NAME, p_afd_status->max_queue_length);
   (void)fprintf(fp, "%s|%u\n", DIRS_SCANNED_NAME, p_afd_status->dir_scans);
#ifdef WITH_INOTIFY
   (void)fprintf(fp, "%s|%u\n",
                 INOTIFY_EVENTS_NAME, p_afd_status->inotify_events);
#endif
   (void)fprintf(fp, "%s|%u|",
                 RECEIVE_LOG_INDICATOR_NAME, p_afd_status->receive_log_ec);
   for (i = 0; i < LOG_FIFO_SIZE; i++)
   {
      switch (p_afd_status->receive_log_fifo[i])
      {
         case INFO_ID :
            (void)fprintf(fp, "I");
            break;
         case ERROR_ID :
            (void)fprintf(fp, "E");
            break;
         case WARNING_ID :
            (void)fprintf(fp, "W");
            break;
         case FAULTY_ID :
            (void)fprintf(fp, "F");
            break;
         default :
            (void)fprintf(fp, "?");
            break;
      }
   }
   (void)fprintf(fp, "\n");
   (void)fprintf(fp, "%s|", RECEIVE_LOG_HISTORY_NAME);
   for (i = 0; i < MAX_LOG_HISTORY; i++)
   {
      switch (p_afd_status->receive_log_history[i])
      {
         case INFO_ID :
            (void)fprintf(fp, "I");
            break;
         case ERROR_ID :
            (void)fprintf(fp, "E");
            break;
         case WARNING_ID :
            (void)fprintf(fp, "W");
            break;
         case FAULTY_ID :
            (void)fprintf(fp, "F");
            break;
         default :
            (void)fprintf(fp, "?");
            break;
      }
   }
   (void)fprintf(fp, "\n");
   (void)fprintf(fp, "%s|%u|",
                 SYSTEM_LOG_INDICATOR_NAME, p_afd_status->sys_log_ec);
   for (i = 0; i < LOG_FIFO_SIZE; i++)
   {
      switch (p_afd_status->sys_log_fifo[i])
      {
         case INFO_ID :
            (void)fprintf(fp, "I");
            break;
         case ERROR_ID :
            (void)fprintf(fp, "E");
            break;
         case WARNING_ID :
            (void)fprintf(fp, "W");
            break;
         case CONFIG_ID :
            (void)fprintf(fp, "C");
            break;
         case FAULTY_ID :
            (void)fprintf(fp, "F");
            break;
         default :
            (void)fprintf(fp, "?");
            break;
      }
   }
   (void)fprintf(fp, "\n");
   (void)fprintf(fp, "%s|", SYSTEM_LOG_HISTORY_NAME);
   for (i = 0; i < MAX_LOG_HISTORY; i++)
   {
      switch (p_afd_status->sys_log_history[i])
      {
         case INFO_ID :
            (void)fprintf(fp, "I");
            break;
         case ERROR_ID :
            (void)fprintf(fp, "E");
            break;
         case WARNING_ID :
            (void)fprintf(fp, "W");
            break;
         case CONFIG_ID :
            (void)fprintf(fp, "C");
            break;
         case FAULTY_ID :
            (void)fprintf(fp, "F");
            break;
         default :
            (void)fprintf(fp, "?");
            break;
      }
   }
   (void)fprintf(fp, "\n");
   (void)fprintf(fp, "%s|%u|",
                 TRANSFER_LOG_INDICATOR_NAME, p_afd_status->trans_log_ec);
   for (i = 0; i < LOG_FIFO_SIZE; i++)
   {
      switch (p_afd_status->trans_log_fifo[i])
      {
         case INFO_ID :
            (void)fprintf(fp, "I");
            break;
         case ERROR_ID :
            (void)fprintf(fp, "E");
            break;
         case WARNING_ID :
            (void)fprintf(fp, "W");
            break;
         case ERROR_OFFLINE_ID :
            (void)fprintf(fp, "O");
            break;
         case FAULTY_ID :
            (void)fprintf(fp, "F");
            break;
         default :
            (void)fprintf(fp, "?");
            break;
      }
   }
   (void)fprintf(fp, "\n");
   (void)fprintf(fp, "%s|", TRANSFER_LOG_HISTORY_NAME);
   for (i = 0; i < MAX_LOG_HISTORY; i++)
   {
      switch (p_afd_status->trans_log_history[i])
      {
         case INFO_ID :
            (void)fprintf(fp, "I");
            break;
         case ERROR_ID :
            (void)fprintf(fp, "E");
            break;
         case WARNING_ID :
            (void)fprintf(fp, "W");
            break;
         case ERROR_OFFLINE_ID :
            (void)fprintf(fp, "O");
            break;
         case FAULTY_ID :
            (void)fprintf(fp, "F");
            break;
         default :
            (void)fprintf(fp, "?");
            break;
      }
   }
   (void)fprintf(fp, "\n");

   if (fclose(fp) == EOF)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to fclose() `%s' : %s",
                 idata_filename, strerror(errno));
   }

   return(SUCCESS);
}
