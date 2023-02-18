/*
 *  typesize_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2011 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_typesize_data - checks if the data types match with the
 **                         curent compiled version
 **   write_typesize_data - writes the size of all data types used
 **                         by the AFD database
 **
 ** SYNOPSIS
 **   int check_typesize_data(int  *old_value_list,
 **                           FILE *output_fp,
 **                           int  do_conversion)
 **   int write_typesize_data(void)
 **
 ** DESCRIPTION
 **   The function check_typesize_data() checks if the size of all
 **   data types match the current version. Data types that are
 **   checked are all those used by structure filetransfer_status,
 **   fileretrieve_status, job_id_data, dir_name_buf, passwd_buf,
 **   queue_buf and dir_config_list.
 **
 **   write_typesize_data() stores all the above values in a file.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.10.2011 H.Kiehl Created
 **   16.11.2018 H.Kiehl Add option to do conversion of pwb database
 **                      if some of it's variables change.
 **   18.02.2023 H.Kiehl Add size for struct fsa, fra, jid and afd_status
 **                      to write_typesize_data().
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

#define MAX_VAR_STR_LENGTH 30
#define CHAR_STR           "char"
#define INT_STR            "int"
#define OFF_T_STR          "off_t"
#define TIME_T_STR         "time_t"
#define SHORT_STR          "short_t"
#ifdef HAVE_LONG_LONG
# define LONG_LONG_STR     "long long"
#endif
#define PID_T_STR          "pid_t"
#define STRUCT_FSA_STR     "struct filetransfer_status"
#define STRUCT_FRA_STR     "struct fileretrieve_status"
#define STRUCT_ASTAT_STR   "struct afd_status"
#define STRUCT_JID_STR     "struct job_id_data"

/* External global variables. */
extern char *p_work_dir;

/* Local function prototypes. */
static int  adapt_pwb_database(int, int);


/*######################## check_typesize_data() ########################*/
int
check_typesize_data(int *old_value_list, FILE *output_fp, int do_conversion)
{
   int  fd,
        not_match = 0;
   char typesize_filename[MAX_PATH_LENGTH];

   (void)snprintf(typesize_filename, MAX_PATH_LENGTH, "%s%s%s",
                  p_work_dir, FIFO_DIR, TYPESIZE_DATA_FILE);
   if ((fd = open(typesize_filename, O_RDONLY)) == -1)
   {
      char *sign;

      if (errno == ENOENT)
      {
         sign = DEBUG_SIGN;
      }
      else
      {
         sign = ERROR_SIGN;
      }
      system_log(sign, __FILE__, __LINE__,
                 "Failed to open() `%s' : %s",
                 typesize_filename, strerror(errno));
      not_match = INCORRECT;
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
                    typesize_filename, strerror(errno));
         not_match = INCORRECT;
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
                                MAP_SHARED, typesize_filename,
                                0)) == (caddr_t) -1)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "mmap() error : %s", strerror(errno));
               not_match = INCORRECT;
            }
            else
            {
               int  i, j, k,
                    val,
                    vallist[] =
                    {
                       /* NOTE: If this is changed, check mondefs.h and */
                       /*       src/servers/afdd/handle_request.c!!!    */
                       MAX_MSG_NAME_LENGTH,
                       MAX_FILENAME_LENGTH,
                       MAX_HOSTNAME_LENGTH,
                       MAX_REAL_HOSTNAME_LENGTH,
                       MAX_AFDNAME_LENGTH,
                       MAX_PROXY_NAME_LENGTH,
                       MAX_TOGGLE_STR_LENGTH,
                       ERROR_HISTORY_LENGTH,
                       MAX_NO_PARALLEL_JOBS,
                       MAX_DIR_ALIAS_LENGTH,
                       MAX_RECIPIENT_LENGTH,
                       MAX_WAIT_FOR_LENGTH,
                       MAX_FRA_TIME_ENTRIES,
                       MAX_OPTION_LENGTH,
                       MAX_PATH_LENGTH,
                       MAX_USER_NAME_LENGTH,
                       SIZEOF_CHAR,
                       SIZEOF_INT,
                       SIZEOF_OFF_T,
                       SIZEOF_TIME_T,
                       SIZEOF_SHORT,
#ifdef HAVE_LONG_LONG
                       SIZEOF_LONG_LONG,
#endif
                       SIZEOF_PID_T,
                       MAX_TIMEZONE_LENGTH
                    };
               char *p_start = ptr,
                    val_str[MAX_INT_LENGTH + 1],
                    *varlist[] =
                    {
                       /* NOTE: If this is changed, check mondefs.h and */
                       /*       src/servers/afdd/handle_request.c!!!    */
                       MAX_MSG_NAME_LENGTH_STR,     /* MAX_MSG_NAME_LENGTH_NR */
                       MAX_FILENAME_LENGTH_STR,     /* MAX_FILENAME_LENGTH_NR */
                       MAX_HOSTNAME_LENGTH_STR,     /* MAX_HOSTNAME_LENGTH_NR */
                       MAX_REAL_HOSTNAME_LENGTH_STR,/* MAX_REAL_HOSTNAME_LENGTH_NR */
                       MAX_AFDNAME_LENGTH_STR,      /* MAX_AFDNAME_LENGTH_NR */
                       MAX_PROXY_NAME_LENGTH_STR,   /* MAX_PROXY_NAME_LENGTH_NR */
                       MAX_TOGGLE_STR_LENGTH_STR,   /* MAX_TOGGLE_STR_LENGTH_NR */
                       ERROR_HISTORY_LENGTH_STR,    /* ERROR_HISTORY_LENGTH_NR */
                       MAX_NO_PARALLEL_JOBS_STR,    /* MAX_NO_PARALLEL_JOBS_NR */
                       MAX_DIR_ALIAS_LENGTH_STR,    /* MAX_DIR_ALIAS_LENGTH_NR */
                       MAX_RECIPIENT_LENGTH_STR,    /* MAX_RECIPIENT_LENGTH_NR */
                       MAX_WAIT_FOR_LENGTH_STR,     /* MAX_WAIT_FOR_LENGTH_NR */
                       MAX_FRA_TIME_ENTRIES_STR,    /* MAX_FRA_TIME_ENTRIES_NR */
                       MAX_OPTION_LENGTH_STR,       /* MAX_OPTION_LENGTH_NR */
                       MAX_PATH_LENGTH_STR,         /* MAX_PATH_LENGTH_NR */
                       MAX_USER_NAME_LENGTH_STR,    /* MAX_USER_NAME_LENGTH_NR */
                       CHAR_STR,                    /* CHAR_NR */
                       INT_STR,                     /* INT_NR */
                       OFF_T_STR,                   /* OFF_T_NR */
                       TIME_T_STR,                  /* TIME_T_NR */
                       SHORT_STR,                   /* SHORT_NR */
#ifdef HAVE_LONG_LONG
                       LONG_LONG_STR,               /* LONG_LONG_NR */
#endif
                       PID_T_STR,                   /* PID_T_NR */
                       MAX_TIMEZONE_LENGTH_STR      /* MAX_TIMEZONE_LENGTH_NR */
                    },
                    var_str[MAX_VAR_STR_LENGTH + 1];

               if (old_value_list != NULL)
               {
                  (void)memset(old_value_list, 0,
                               ((MAX_CHANGEABLE_VARS) * sizeof(int)));
               }
               do
               {
                  if ((*ptr == '#') || (*ptr == '|'))
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
                                     (k < MAX_INT_LENGTH) &&
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
                              if ((*ptr == '\n') || (*ptr == '\r'))
                              {
                                 if (k > 0)
                                 {
                                    val_str[k] = '\0';
                                    val = atoi(val_str);
                                    if (val != vallist[j])
                                    {
                                       if (output_fp == NULL)
                                       {
                                          system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                     "[%d] %s %d != %d",
                                                     not_match, varlist[j], val,
                                                     vallist[j]);
                                       }
                                       else
                                       {
                                          (void)fprintf(output_fp,
                                                        "[%d] %s %d != %d\n",
                                                        not_match, varlist[j],
                                                        val, vallist[j]);
                                       }
                                       not_match++;
                                       if (old_value_list != NULL)
                                       {
                                          old_value_list[j + 1] = val;
                                          switch (j)
                                          {
                                             case  0 : old_value_list[0] |= MAX_MSG_NAME_LENGTH_NR;
                                                       break;
                                             case  1 : old_value_list[0] |= MAX_FILENAME_LENGTH_NR;
                                                       break;
                                             case  2 : old_value_list[0] |= MAX_HOSTNAME_LENGTH_NR;
                                                       break;
                                             case  3 : old_value_list[0] |= MAX_REAL_HOSTNAME_LENGTH_NR;
                                                       break;
                                             case  4 : old_value_list[0] |= MAX_AFDNAME_LENGTH_NR;
                                                       break;
                                             case  5 : old_value_list[0] |= MAX_PROXY_NAME_LENGTH_NR;
                                                       break;
                                             case  6 : old_value_list[0] |= MAX_TOGGLE_STR_LENGTH_NR;
                                                       break;
                                             case  7 : old_value_list[0] |= ERROR_HISTORY_LENGTH_NR;
                                                       break;
                                             case  8 : old_value_list[0] |= MAX_NO_PARALLEL_JOBS_NR;
                                                       break;
                                             case  9 : old_value_list[0] |= MAX_DIR_ALIAS_LENGTH_NR;
                                                       break;
                                             case 10 : old_value_list[0] |= MAX_RECIPIENT_LENGTH_NR;
                                                       break;
                                             case 11 : old_value_list[0] |= MAX_WAIT_FOR_LENGTH_NR;
                                                       break;
                                             case 12 : old_value_list[0] |= MAX_FRA_TIME_ENTRIES_NR;
                                                       break;
                                             case 13 : old_value_list[0] |= MAX_OPTION_LENGTH_NR;
                                                       break;
                                             case 14 : old_value_list[0] |= MAX_PATH_LENGTH_NR;
                                                       break;
                                             case 15 : old_value_list[0] |= MAX_USER_NAME_LENGTH_NR;
                                                       break;
                                             case 16 : old_value_list[0] |= CHAR_NR;
                                                       break;
                                             case 17 : old_value_list[0] |= INT_NR;
                                                       break;
                                             case 18 : old_value_list[0] |= OFF_T_NR;
                                                       break;
                                             case 19 : old_value_list[0] |= TIME_T_NR;
                                                       break;
                                             case 20 : old_value_list[0] |= SHORT_NR;
                                                       break;
#ifdef HAVE_LONG_LONG
                                             case 21 : old_value_list[0] |= LONG_LONG_NR;
                                                       break;
#endif
                                             case 22 : old_value_list[0] |= PID_T_NR;
                                                       break;
                                             case 23 : old_value_list[0] |= MAX_TIMEZONE_LENGTH_NR;
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
                                       if (old_value_list != NULL)
                                       {
                                          old_value_list[j + 1] = val;
                                       }
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
                             typesize_filename, strerror(errno));
               }

               if ((old_value_list != NULL) && (do_conversion == YES))
               {
                  /* Check if we must convert the password database file. */
                  if ((old_value_list[0] & MAX_REAL_HOSTNAME_LENGTH_NR) ||
                      (old_value_list[0] & MAX_USER_NAME_LENGTH_NR))
                  {
                     (void)adapt_pwb_database(old_value_list[MAX_REAL_HOSTNAME_LENGTH_POS + 1],
                                              old_value_list[MAX_USER_NAME_LENGTH_POS + 1]);
                  }
               }
            }
         }
         if (close(fd) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to close() `%s' : %s",
                       typesize_filename, strerror(errno));
         }
      }
   }

   return(not_match);
}


/*######################## write_typesize_data() ########################*/
int
write_typesize_data(void)
{
   int  fd;
   FILE *fp;
   char typesize_filename[MAX_PATH_LENGTH];

   (void)snprintf(typesize_filename, MAX_PATH_LENGTH, "%s%s%s",
                  p_work_dir, FIFO_DIR, TYPESIZE_DATA_FILE);
   if ((fd = open(typesize_filename, (O_RDWR | O_CREAT | O_TRUNC),
                  (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to open() `%s' : %s",
                 typesize_filename, strerror(errno));
      return(INCORRECT);
   }
   if ((fp = fdopen(fd, "w")) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to fdopen() `%s' : %s",
                 typesize_filename, strerror(errno));
      (void)close(fd);
      return(INCORRECT);
   }
   (void)fprintf(fp, "# NOTE: Under no circumstances edit this file!!!!\n");
   (void)fprintf(fp, "#       Please use the header files in the source code\n");
   (void)fprintf(fp, "#       tree and then recompile AFD.\n");
   (void)fprintf(fp, "%s|%d\n", MAX_MSG_NAME_LENGTH_STR, MAX_MSG_NAME_LENGTH);
   (void)fprintf(fp, "%s|%d\n", MAX_FILENAME_LENGTH_STR, MAX_FILENAME_LENGTH);
   (void)fprintf(fp, "%s|%d\n", MAX_HOSTNAME_LENGTH_STR, MAX_HOSTNAME_LENGTH);
   (void)fprintf(fp, "%s|%d\n", MAX_REAL_HOSTNAME_LENGTH_STR, MAX_REAL_HOSTNAME_LENGTH);
   (void)fprintf(fp, "%s|%d\n", MAX_AFDNAME_LENGTH_STR, MAX_AFDNAME_LENGTH);
   (void)fprintf(fp, "%s|%d\n", MAX_PROXY_NAME_LENGTH_STR, MAX_PROXY_NAME_LENGTH);
   (void)fprintf(fp, "%s|%d\n", MAX_TOGGLE_STR_LENGTH_STR, MAX_TOGGLE_STR_LENGTH);
   (void)fprintf(fp, "%s|%d\n", ERROR_HISTORY_LENGTH_STR, ERROR_HISTORY_LENGTH);
   (void)fprintf(fp, "%s|%d\n", MAX_NO_PARALLEL_JOBS_STR, MAX_NO_PARALLEL_JOBS);
   (void)fprintf(fp, "%s|%d\n", MAX_DIR_ALIAS_LENGTH_STR, MAX_DIR_ALIAS_LENGTH);
   (void)fprintf(fp, "%s|%d\n", MAX_RECIPIENT_LENGTH_STR, MAX_RECIPIENT_LENGTH);
   (void)fprintf(fp, "%s|%d\n", MAX_WAIT_FOR_LENGTH_STR, MAX_WAIT_FOR_LENGTH);
   (void)fprintf(fp, "%s|%d\n", MAX_FRA_TIME_ENTRIES_STR, MAX_FRA_TIME_ENTRIES);
   (void)fprintf(fp, "%s|%d\n", MAX_TIMEZONE_LENGTH_STR, MAX_TIMEZONE_LENGTH);
   (void)fprintf(fp, "%s|%d\n", MAX_OPTION_LENGTH_STR, MAX_OPTION_LENGTH);
   (void)fprintf(fp, "%s|%d\n", MAX_PATH_LENGTH_STR, MAX_PATH_LENGTH);
   (void)fprintf(fp, "%s|%d\n", MAX_USER_NAME_LENGTH_STR,MAX_USER_NAME_LENGTH);

   (void)fprintf(fp, "%s|%d\n", CHAR_STR, SIZEOF_CHAR);
   (void)fprintf(fp, "%s|%d\n", INT_STR, SIZEOF_INT);
   (void)fprintf(fp, "%s|%d\n", OFF_T_STR, SIZEOF_OFF_T);
   (void)fprintf(fp, "%s|%d\n", TIME_T_STR, SIZEOF_TIME_T);
   (void)fprintf(fp, "%s|%d\n", SHORT_STR, SIZEOF_SHORT);
#ifdef HAVE_LONG_LONG
   (void)fprintf(fp, "%s|%d\n", LONG_LONG_STR, SIZEOF_LONG_LONG);
#endif
   (void)fprintf(fp, "%s|%d\n", PID_T_STR, SIZEOF_PID_T);
   (void)fprintf(fp, "%s|%d\n", STRUCT_FSA_STR,
                 (int)sizeof(struct filetransfer_status));
   (void)fprintf(fp, "%s|%d\n", STRUCT_FRA_STR,
                 (int)sizeof(struct fileretrieve_status));
   (void)fprintf(fp, "%s|%d\n", STRUCT_ASTAT_STR,
                 (int)sizeof(struct afd_status));
   (void)fprintf(fp, "%s|%d\n", STRUCT_JID_STR,
                 (int)sizeof(struct job_id_data));

   if (fclose(fp) == EOF)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to fclose() `%s' : %s",
                 typesize_filename, strerror(errno));
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++ adapt_pwb_database() ++++++++++++++++++++++++*/
static int
adapt_pwb_database(int old_real_hostname_length, int old_user_name_length)
{
   int  do_rename = NO,
        old_pwb_fd,
        ret,
        truncated = 0,
        truncated_uh_name = 0,
        truncated_password = 0;
   char new_pwb_file_name[MAX_PATH_LENGTH],
        old_pwb_file_name[MAX_PATH_LENGTH];

   (void)strcpy(old_pwb_file_name, p_work_dir);
   (void)strcat(old_pwb_file_name, FIFO_DIR);
   (void)strcat(old_pwb_file_name, PWB_DATA_FILE);
   if ((old_pwb_fd = open(old_pwb_file_name, O_RDONLY)) == -1)
   {
      if (errno == ENOENT)
      {
         /*
          * It can be that there are absolutly no passwords in DIR_CONFIG,
          * so PWB_DATA_FILE is not created.
          */
         ret = SUCCESS;
      }
      else
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to open() `%s' : %s"),
                    old_pwb_file_name, strerror(errno));
         ret = INCORRECT;
      }
   }
   else
   {
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat stat_buf;
#endif

#ifdef LOCK_DEBUG
      rlock_region(old_pwb_fd, 1, __FILE__, __LINE__);
#else
      rlock_region(old_pwb_fd, 1);
#endif
#ifdef HAVE_STATX
      if (statx(old_pwb_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(old_pwb_fd, &stat_buf) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                    _("Failed to statx() `%s' : %s"),
#else
                    _("Failed to fstat() `%s' : %s"),
#endif
                    old_pwb_file_name, strerror(errno));
         ret = INCORRECT;
      }
      else
      {
#ifdef HAVE_STATX
         if (stat_buf.stx_size <= AFD_WORD_OFFSET)
#else
         if (stat_buf.st_size <= AFD_WORD_OFFSET)
#endif
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("Password file %s is not long enough to contain any valid data."),
                       old_pwb_file_name);
            ret = SUCCESS;
         }
         else
         {
            char *old_ptr;

#ifdef HAVE_MMAP
            if ((old_ptr = mmap(NULL,
# ifdef HAVE_STATX
                                stat_buf.stx_size, PROT_READ, MAP_SHARED,
# else
                                stat_buf.st_size, PROT_READ, MAP_SHARED,
# endif
                                old_pwb_fd, 0)) == (caddr_t) -1)
#else
            if ((old_ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                                    stat_buf.stx_size, PROT_READ, MAP_SHARED,
# else
                                    stat_buf.st_size, PROT_READ, MAP_SHARED,
# endif
                                    old_pwb_file_name, 0)) == (caddr_t) -1)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to mmap() `%s' : %s"),
                          old_pwb_file_name, strerror(errno));
               ret = INCORRECT;
            }
            else
            {
               if (*(old_ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_PWB_VERSION)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Incorrect password version, unable to adapt password database.");

                  /*
                   * Let's return SUCCESS, the conversion should be done
                   * by the function that handles the version change
                   * of the password database. It will then automatically
                   * do the conversion of any variable length changes.
                   */
                  ret = SUCCESS;
               }
               else
               {
                  int                   new_pwb_fd,
                                        no_of_passwd;
                  size_t                new_size;
                  char                  *new_ptr;
                  struct old_passwd_buf
                         {
                            char          uh_name[old_user_name_length + old_real_hostname_length + 1];
                            unsigned char passwd[old_user_name_length];
                            signed char   dup_check;
                         } *old_pwb;

                  no_of_passwd = *(int *)old_ptr;
                  old_ptr += AFD_WORD_OFFSET;
                  old_pwb = (struct old_passwd_buf *)old_ptr;

                  new_size = AFD_WORD_OFFSET +
                             (((no_of_passwd / PWB_STEP_SIZE) + 1) *
                              PWB_STEP_SIZE * sizeof(struct passwd_buf));
#ifdef _DEBUG
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "new_size=%lld sizeof(struct passwd_buf)=%lld no_of_passwd=%d",
                             new_size, sizeof(struct passwd_buf), no_of_passwd);
#endif
                  (void)snprintf(new_pwb_file_name, MAX_PATH_LENGTH,
                                 "%s%s/.tmp_pwb_data_file",
                                 p_work_dir, FIFO_DIR);
                  (void)unlink(new_pwb_file_name);
                  if ((new_pwb_fd = open(new_pwb_file_name,
                                         (O_RDWR | O_CREAT | O_TRUNC),
#ifdef GROUP_CAN_WRITE
                                         (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP))) == -1)
#else
                                         (S_IRUSR | S_IWUSR))) == -1)
#endif
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Failed to open() %s : %s",
                                new_pwb_file_name, strerror(errno));
                     ret = INCORRECT;
                  }
                  else
                  {
                     int  i,
                          loops,
                          rest;
                     char buffer[4096];

                     ret = SUCCESS;
                     loops = new_size / 4096;
                     rest = new_size % 4096;
                     (void)memset(buffer, 0, 4096);
                     for (i = 0; i < loops; i++)
                     {
                        if (write(new_pwb_fd, buffer, 4096) != 4096)
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      "write() error : %s", strerror(errno));
                           ret = INCORRECT;
                           break;
                        }
                     }
                     if ((rest > 0) && (ret != INCORRECT))
                     {
                        if (write(new_pwb_fd, buffer, rest) != rest)
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      "write() error : %s", strerror(errno));
                           ret = INCORRECT;
                        }
                     }
                     if (ret != INCORRECT)
                     {
#ifdef HAVE_MMAP
                        if ((new_ptr = mmap(NULL, new_size,
                                            (PROT_READ | PROT_WRITE), MAP_SHARED,
                                            new_pwb_fd, 0)) == (caddr_t) -1)
#else
                        if ((new_ptr = mmap_emu(NULL, new_size,
                                                (PROT_READ | PROT_WRITE), MAP_SHARED,
                                                new_pwb_file_name, 0)) == (caddr_t) -1)
#endif
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      "Failed to mmap() to %s : %s",
                                      new_pwb_file_name, strerror(errno));
                           ret = INCORRECT;
                        }
                        else
                        {
                           int               truncated_flag;
                           struct passwd_buf *new_pwb;

                           *(int *)new_ptr = no_of_passwd;
                           *(new_ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
                           *(new_ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
                           *(new_ptr + SIZEOF_INT + 1 + 1 + 1) = CURRENT_PWB_VERSION;
                           (void)memset((new_ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
                           *(new_ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
                           *(new_ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
                           *(new_ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
                           *(new_ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
                           new_ptr += AFD_WORD_OFFSET;
                           new_pwb = (struct passwd_buf *)new_ptr;

                           for (i = 0; i < no_of_passwd; i++)
                           {
                              truncated_flag = NO;
                              if (strlen((char *)old_pwb[i].uh_name) >
                                  (MAX_USER_NAME_LENGTH + MAX_REAL_HOSTNAME_LENGTH))
                              {
                                 system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                            "Truncating uh_name %s to %d characters.",
                                            old_pwb[i].uh_name,
                                            MAX_USER_NAME_LENGTH + MAX_REAL_HOSTNAME_LENGTH);
                                 truncated_uh_name++;
                                 truncated_flag = YES;
                              }
                              if ((old_user_name_length + old_real_hostname_length) <= (MAX_USER_NAME_LENGTH + MAX_REAL_HOSTNAME_LENGTH))
                              {
                                 (void)memcpy(new_pwb[i].uh_name,
                                              old_pwb[i].uh_name,
                                              old_user_name_length + old_real_hostname_length);
                              }
                              else
                              {
                                 (void)memcpy(new_pwb[i].uh_name,
                                              old_pwb[i].uh_name,
                                              MAX_USER_NAME_LENGTH + MAX_REAL_HOSTNAME_LENGTH);
                              }
                              if (strlen((char *)old_pwb[i].passwd) > MAX_USER_NAME_LENGTH)
                              {
                                 system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                            "Truncating password for uh_name %s to %d characters.",
                                            old_pwb[i].uh_name, MAX_USER_NAME_LENGTH);
                                 truncated_password++;
                                 truncated_flag = YES;
                              }
                              if (old_user_name_length <= MAX_USER_NAME_LENGTH)
                              {
                                 (void)memcpy(new_pwb[i].passwd,
                                              old_pwb[i].passwd,
                                              old_user_name_length);
                              }
                              else
                              {
                                 (void)memcpy(new_pwb[i].passwd,
                                              old_pwb[i].passwd,
                                              MAX_USER_NAME_LENGTH);
                              }

                              new_pwb[i].dup_check = old_pwb[i].dup_check;
                              if (truncated_flag == YES)
                              {
                                 truncated++;
                              }
                           }
                           new_ptr -= AFD_WORD_OFFSET;
#ifdef HAVE_MMAP
                           if (munmap(new_ptr, new_size) == -1)
#else
                           if (munmap_emu(new_ptr) == -1)
#endif
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                         _("Failed to munmap() from `%s' : %s"),
                                         new_pwb_file_name, strerror(errno));
                           }
                        }
                     }

                     if (close(new_pwb_fd) == -1)
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   _("close() error : %s"), strerror(errno));
                     }
                  }
                  old_ptr -= AFD_WORD_OFFSET;
                  do_rename = YES;
               }
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
               if (munmap(old_ptr, stat_buf.stx_size) == -1)
# else
               if (munmap(old_ptr, stat_buf.st_size) == -1)
# endif
#else
               if (munmap_emu(old_ptr) == -1)
#endif
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             _("Failed to munmap() from `%s' : %s"),
                             old_pwb_file_name, strerror(errno));
               }
            }
         }
      }
      if (close(old_pwb_fd) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__, _("close() error : %s"),
                    strerror(errno));
      }
   }

   if (do_rename == YES)
   {
      if ((truncated_uh_name > 0) || (truncated_password > 0))
      {
         char save_pwb_file_name[MAX_PATH_LENGTH];

         (void)strcpy(save_pwb_file_name, old_pwb_file_name);
         (void)strcat(save_pwb_file_name, ".save");
         (void)unlink(save_pwb_file_name);
         if (rename(old_pwb_file_name, save_pwb_file_name) == -1)
         {
            system_log(INFO_SIGN, __FILE__, __LINE__,
                      _("Failed to rename() `%s' to `%s' : %s"),
                      old_pwb_file_name, save_pwb_file_name, strerror(errno));
            (void)unlink(old_pwb_file_name);
         }
         else
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Since the password database was resized (%d -> %d + %d -> %d) and the size is smaller some passwords and/or user hostname identifiers had to be truncated. Made a backup copy of the database file %s",
                       old_real_hostname_length, MAX_REAL_HOSTNAME_LENGTH,
                       old_user_name_length, MAX_USER_NAME_LENGTH,
                       save_pwb_file_name);
         }
      }
      else
      {
         if ((unlink(old_pwb_file_name) == -1) && (errno != ENOENT))
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to unlink() `%s' : %s"),
                       old_pwb_file_name, strerror(errno));
         }
      }
      if (rename(new_pwb_file_name, old_pwb_file_name) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                   _("Failed to rename() `%s' to `%s' : %s"),
                   new_pwb_file_name, old_pwb_file_name, strerror(errno));
         ret = INCORRECT;
      }
      else
      {
         if ((truncated_uh_name > 0) || (truncated_password > 0))
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Converted password database %s due to changes in structure length (MAX_REAL_HOSTNAME_LENGTH: %d->%d MAX_USER_NAME_LENGTH: %d->%d). However %d are lost because they had to be truncated (passwords=%d uh_name=%d)",
                       old_pwb_file_name,
                       old_real_hostname_length, MAX_REAL_HOSTNAME_LENGTH,
                       old_user_name_length, MAX_USER_NAME_LENGTH, truncated,
                       truncated_password, truncated_uh_name);
         }
         else
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Successfully converted password database %s due to changes in structure length (MAX_REAL_HOSTNAME_LENGTH: %d->%d MAX_USER_NAME_LENGTH: %d->%d).",
                       old_pwb_file_name,
                       old_real_hostname_length, MAX_REAL_HOSTNAME_LENGTH,
                       old_user_name_length, MAX_USER_NAME_LENGTH);
         }
      }
   }

   return(ret);
}
