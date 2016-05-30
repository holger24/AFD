/*
 *  convert_fra.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 - 2016 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   convert_fra - converts the FRA from an old format to new one
 **
 ** SYNOPSIS
 **   char *convert_fra(int           old_fra_fd,
 **                     char          *old_fra_stat,
 **                     off_t         *old_fra_size,
 **                     int           old_no_of_dirs,
 **                     unsigned char old_version,
 **                     unsigned char new_version)
 **
 ** DESCRIPTION
 **   When there is a change in the structure fileretrieve_status (FRA)
 **   This function converts an old FRA to the new one. This one
 **   is currently for converting Version 0 to 4.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   05.05.2002 H.Kiehl Created
 **   22.05.2002 H.Kiehl Separate old file times for unknown and queued files.
 **   14.08.2002 H.Kiehl Option to ignore files which have a certain size.
 **   10.11.2006 H.Kiehl Added Version 4.
 **   31.12.2007 H.Kiehl Added Version 5.
 **   28.05.2012 H.Kiehl Added Version 6.
 **
 */
DESCR__E_M1

#include <stdio.h>                    /* fprintf()                       */
#include <string.h>                   /* strcpy(), strcat(), strerror()  */
#include <stdlib.h>                   /* exit()                          */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>                /* mmap()                          */
#endif
#include <unistd.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>                   /* O_RDWR, O_CREAT, O_WRONLY, etc  */
#endif
#include <errno.h>
#include "amgdefs.h"

/* External global veriables. */
extern unsigned int max_copied_files;
extern off_t        max_copied_file_size;

/* Version 0 */
#define MAX_DIR_ALIAS_LENGTH_0 10
#define MAX_HOSTNAME_LENGTH_0  8
#define MAX_RECIPIENT_LENGTH_0 256
#define AFD_WORD_OFFSET_0      8
struct bd_time_entry_0
       {
#ifdef _WORKING_LONG_LONG
          unsigned long long continuous_minute;
          unsigned long long minute;
#else
          unsigned char      continuous_minute[8];
          unsigned char      minute[8];
#endif
          unsigned int       hour;
          unsigned int       day_of_month;
          unsigned short     month;
          unsigned char      day_of_week;
       };
struct fileretrieve_status_0
       {
          char                   dir_alias[MAX_DIR_ALIAS_LENGTH_0 + 1];
          char                   host_alias[MAX_HOSTNAME_LENGTH_0 + 1];
          char                   url[MAX_RECIPIENT_LENGTH_0];
          struct bd_time_entry_0 te;
          unsigned char          dir_status;
          unsigned char          remove;
          unsigned char          stupid_mode;
          unsigned int           protocol;
          unsigned char          delete_files_flag;
          unsigned char          report_unknown_files;
          unsigned char          important_dir;
          unsigned char          time_option;
          char                   force_reread;
          char                   queued;
          char                   priority;
          unsigned long          bytes_received;
          unsigned int           files_received;
          time_t                 last_retrieval;
          time_t                 next_check_time;
          int                    old_file_time;
          int                    end_character;
          int                    dir_pos;
          int                    fsa_pos;
          int                    no_of_process;
          int                    max_process;
       };

/* Version 1 */
#define MAX_DIR_ALIAS_LENGTH_1 10
#define MAX_HOSTNAME_LENGTH_1  8
#define MAX_RECIPIENT_LENGTH_1 256
#define AFD_WORD_OFFSET_1      8
struct bd_time_entry_1
       {
#ifdef _WORKING_LONG_LONG
          unsigned long long continuous_minute;
          unsigned long long minute;
#else
          unsigned char      continuous_minute[8];
          unsigned char      minute[8];
#endif
          unsigned int       hour;
          unsigned int       day_of_month;
          unsigned short     month;
          unsigned char      day_of_week;
       };
struct fileretrieve_status_1
       {
          char                   dir_alias[MAX_DIR_ALIAS_LENGTH_1 + 1];
          char                   host_alias[MAX_HOSTNAME_LENGTH_1 + 1];
          char                   url[MAX_RECIPIENT_LENGTH_1];
          struct bd_time_entry_1 te;
          unsigned char          dir_status;
          unsigned char          remove;
          unsigned char          stupid_mode;
          unsigned int           protocol;
          unsigned char          delete_files_flag;
          unsigned char          report_unknown_files;
          unsigned char          important_dir;
          unsigned char          time_option;
          char                   force_reread;
          char                   queued;
          char                   priority;
          off_t                  bytes_received;
          unsigned int           files_received;
          unsigned int           dir_flag;
          unsigned int           files_in_dir;
          unsigned int           files_queued;
          off_t                  bytes_in_dir;
          off_t                  bytes_in_queue;
          time_t                 last_retrieval;
          time_t                 next_check_time;
          int                    unknown_file_time;
          int                    queued_file_time;
          int                    end_character;
          int                    dir_pos;
          int                    fsa_pos;
          int                    no_of_process;
          int                    max_process;
       };

/* Version 2 */
#define MAX_DIR_ALIAS_LENGTH_2 10
#define MAX_HOSTNAME_LENGTH_2  8
#define MAX_RECIPIENT_LENGTH_2 256
#define AFD_WORD_OFFSET_2      (SIZEOF_INT + 4 + SIZEOF_INT + 4) /* Changed */
#define MAX_WAIT_FOR_LENGTH_2  64 /* New */
struct bd_time_entry_2
       {
#ifdef _WORKING_LONG_LONG
          unsigned long long continuous_minute;
          unsigned long long minute;
#else
          unsigned char      continuous_minute[8];
          unsigned char      minute[8];
#endif
          unsigned int       hour;
          unsigned int       day_of_month;
          unsigned short     month;
          unsigned char      day_of_week;
       };
struct fileretrieve_status_2
       {
          char                   dir_alias[MAX_DIR_ALIAS_LENGTH_2 + 1];
          char                   host_alias[MAX_HOSTNAME_LENGTH_2 + 1];
          char                   url[MAX_RECIPIENT_LENGTH_2];
          char                   wait_for_filename[MAX_WAIT_FOR_LENGTH]; /* New */
          struct bd_time_entry_2 te;
          struct bd_time_entry_2 ate; /* New */
          unsigned char          dir_status;
          unsigned char          remove;
          unsigned char          stupid_mode;
          unsigned char          delete_files_flag;
          unsigned char          report_unknown_files;
          unsigned char          important_dir;
          unsigned char          time_option;
          char                   force_reread;
          char                   queued;
          char                   priority;
          unsigned int           protocol;
          unsigned int           files_received;
          unsigned int           dir_flag;
          unsigned int           files_in_dir;
          unsigned int           files_queued;
          unsigned int           accumulate; /* New */
          unsigned int           max_copied_files; /* New */
          unsigned int           ignore_file_time; /* New */
          unsigned int           gt_lt_sign; /* New */
#ifdef WITH_DUP_CHECK
          unsigned int           dup_check_flag; /* New */
#endif
          u_off_t                bytes_received;
          off_t                  bytes_in_dir;
          off_t                  bytes_in_queue;
          off_t                  accumulate_size; /* New */
          off_t                  ignore_size; /* New */
          off_t                  max_copied_file_size; /* New */
#ifdef WITH_DUP_CHECK
          time_t                 dup_check_timeout; /* New */
#endif
          time_t                 last_retrieval;
          time_t                 next_check_time;
          int                    unknown_file_time;
          int                    queued_file_time;
          int                    locked_file_time;
          int                    end_character;
          int                    dir_pos;
          int                    fsa_pos;
          int                    no_of_process;
          int                    max_process;
       };

/* Version 3 */
#define MAX_DIR_ALIAS_LENGTH_3 10
#define MAX_HOSTNAME_LENGTH_3  8
#define MAX_RECIPIENT_LENGTH_3 256
#define AFD_WORD_OFFSET_3      (SIZEOF_INT + 4 + SIZEOF_INT + 4)
#define MAX_WAIT_FOR_LENGTH_3  64
struct bd_time_entry_3
       {
#ifdef _WORKING_LONG_LONG
          unsigned long long continuous_minute;
          unsigned long long minute;
#else
          unsigned char      continuous_minute[8];
          unsigned char      minute[8];
#endif
          unsigned int       hour;
          unsigned int       day_of_month;
          unsigned short     month;
          unsigned char      day_of_week;
       };
struct fileretrieve_status_3
       {
          char                   dir_alias[MAX_DIR_ALIAS_LENGTH_3 + 1];
          char                   host_alias[MAX_HOSTNAME_LENGTH_3 + 1];
          char                   url[MAX_RECIPIENT_LENGTH_3];
          char                   wait_for_filename[MAX_WAIT_FOR_LENGTH];
          struct bd_time_entry_3 te;
          struct bd_time_entry_3 ate;
          unsigned char          dir_status;
          unsigned char          remove;
          unsigned char          stupid_mode;
          unsigned char          delete_files_flag;
          unsigned char          report_unknown_files;
          unsigned char          important_dir;
          unsigned char          time_option;
          char                   force_reread;
          char                   queued;
          char                   priority;
          unsigned int           protocol;
          unsigned int           files_received;
          unsigned int           dir_flag;/* Added DIR_DISABLED from dir_status */
          unsigned int           in_dc_flag; /* New */
          unsigned int           files_in_dir;
          unsigned int           files_queued;
          unsigned int           accumulate;
          unsigned int           max_copied_files;
          unsigned int           ignore_file_time;
          unsigned int           gt_lt_sign;
#ifdef WITH_DUP_CHECK
          unsigned int           dup_check_flag;
#endif
          u_off_t                bytes_received;
          off_t                  bytes_in_dir;
          off_t                  bytes_in_queue;
          off_t                  accumulate_size;
          off_t                  ignore_size;
          off_t                  max_copied_file_size;
#ifdef WITH_DUP_CHECK
          time_t                 dup_check_timeout;
#endif
          time_t                 last_retrieval;
          time_t                 next_check_time;
          int                    unknown_file_time;
          int                    queued_file_time;
          int                    locked_file_time;
          int                    end_character;
          unsigned int           dir_id; /* New, Changed */
          int                    fsa_pos;
          int                    no_of_process;
          int                    max_process;
       };

/* Version 4 */
#define MAX_DIR_ALIAS_LENGTH_4 10
#define MAX_HOSTNAME_LENGTH_4  8
#define MAX_RECIPIENT_LENGTH_4 256
#define AFD_WORD_OFFSET_4      (SIZEOF_INT + 4 + SIZEOF_INT + 4)
#define MAX_WAIT_FOR_LENGTH_4  64
struct bd_time_entry_4
       {
#ifdef _WORKING_LONG_LONG
          unsigned long long continuous_minute;
          unsigned long long minute;
#else
          unsigned char      continuous_minute[8];
          unsigned char      minute[8];
#endif
          unsigned int       hour;
          unsigned int       day_of_month;
          unsigned short     month;
          unsigned char      day_of_week;
       };
struct fileretrieve_status_4
       {
          char                   dir_alias[MAX_DIR_ALIAS_LENGTH_4 + 1];
          char                   host_alias[MAX_HOSTNAME_LENGTH_4 + 1];
          char                   url[MAX_RECIPIENT_LENGTH_4];
          char                   wait_for_filename[MAX_WAIT_FOR_LENGTH];
          struct bd_time_entry_4 te;
          struct bd_time_entry_4 ate;
          unsigned char          dir_status;
          unsigned char          remove;
          unsigned char          stupid_mode;
          unsigned char          delete_files_flag;
          unsigned char          report_unknown_files;
          unsigned char          important_dir;
          unsigned char          time_option;
          char                   force_reread;
          char                   queued;
          char                   priority;
          unsigned int           protocol;
          unsigned int           files_received;
          unsigned int           dir_flag;
          unsigned int           in_dc_flag;
          unsigned int           files_in_dir;
          unsigned int           files_queued;
          unsigned int           accumulate;
          unsigned int           max_copied_files;
          unsigned int           ignore_file_time;
          unsigned int           gt_lt_sign;
          unsigned int           keep_connected; /* New */
#ifdef WITH_DUP_CHECK
          unsigned int           dup_check_flag;
#endif
          u_off_t                bytes_received;
          off_t                  bytes_in_dir;
          off_t                  bytes_in_queue;
          off_t                  accumulate_size;
          off_t                  ignore_size;
          off_t                  max_copied_file_size;
#ifdef WITH_DUP_CHECK
          time_t                 dup_check_timeout;
#endif
          time_t                 last_retrieval;
          time_t                 next_check_time;
          time_t                 warn_time;      /* New */
          int                    unknown_file_time;
          int                    queued_file_time;
          int                    locked_file_time;
          int                    end_character;
          unsigned int           dir_id;
          int                    fsa_pos;
          int                    no_of_process;
          int                    max_process;
          int                    max_errors;     /* New */
          unsigned int           error_counter;  /* New */
       };

/* Version 5 */
#define MAX_DIR_ALIAS_LENGTH_5 10
#define MAX_HOSTNAME_LENGTH_5  8
#define MAX_RECIPIENT_LENGTH_5 256
#define AFD_WORD_OFFSET_5      (SIZEOF_INT + 4 + SIZEOF_INT + 4)
#define MAX_FRA_TIME_ENTRIES_5 12
#define MAX_WAIT_FOR_LENGTH_5  64
struct bd_time_entry_5
       {
#ifdef HAVE_LONG_LONG
          unsigned long long continuous_minute;
          unsigned long long minute;
#else
          unsigned char      continuous_minute[8];
          unsigned char      minute[8];
#endif
          unsigned int       hour;
          unsigned int       day_of_month;
          unsigned short     month;
          unsigned char      day_of_week;
       };
struct fileretrieve_status_5
       {
          char                   dir_alias[MAX_DIR_ALIAS_LENGTH_5 + 1];
          char                   host_alias[MAX_HOSTNAME_LENGTH_5 + 1];
          char                   url[MAX_RECIPIENT_LENGTH_5];
          char                   wait_for_filename[MAX_WAIT_FOR_LENGTH];
          struct bd_time_entry_5 te[MAX_FRA_TIME_ENTRIES_5]; /* Changed */
          struct bd_time_entry_5 ate;
          unsigned char          dir_status;
          unsigned char          remove;
          unsigned char          stupid_mode;
          unsigned char          delete_files_flag;
          unsigned char          report_unknown_files;
          unsigned char          important_dir;
          unsigned char          no_of_time_entries;         /* Changed */
          char                   force_reread;
          char                   queued;
          char                   priority;
          unsigned int           protocol;
          unsigned int           files_received;
          unsigned int           dir_flag;
          unsigned int           in_dc_flag;
          unsigned int           files_in_dir;
          unsigned int           files_queued;
          unsigned int           accumulate;
          unsigned int           max_copied_files;
          unsigned int           ignore_file_time;
          unsigned int           gt_lt_sign;
          unsigned int           keep_connected;
#ifdef WITH_DUP_CHECK
          unsigned int           dup_check_flag;
#endif
          u_off_t                bytes_received;
          off_t                  bytes_in_dir;
          off_t                  bytes_in_queue;
          off_t                  accumulate_size;
          off_t                  ignore_size;
          off_t                  max_copied_file_size;
#ifdef WITH_DUP_CHECK
          time_t                 dup_check_timeout;
#endif
          time_t                 last_retrieval;
          time_t                 next_check_time;
          time_t                 warn_time;
          time_t                 start_event_handle;         /* New */
          time_t                 end_event_handle;           /* New */
          int                    unknown_file_time;
          int                    queued_file_time;
          int                    locked_file_time;
          int                    end_character;
          unsigned int           dir_id;
          int                    fsa_pos;
          int                    no_of_process;
          int                    max_process;
          int                    max_errors;
          unsigned int           error_counter;
       };

/* Version 6 */
#define MAX_DIR_ALIAS_LENGTH_6 10
#define MAX_HOSTNAME_LENGTH_6  8
#define MAX_RECIPIENT_LENGTH_6 256
#define AFD_WORD_OFFSET_6      (SIZEOF_INT + 4 + SIZEOF_INT + 4)
#define MAX_FRA_TIME_ENTRIES_6 12
#define MAX_WAIT_FOR_LENGTH_6  64
#define MAX_TIMEZONE_LENGTH_6  32
#define MAX_FILENAME_LENGTH_6  256
struct bd_time_entry_6
       {
#ifdef HAVE_LONG_LONG
          unsigned long long continuous_minute;
          unsigned long long minute;
#else
          unsigned char      continuous_minute[8];
          unsigned char      minute[8];
#endif
          unsigned int       hour;
          unsigned int       day_of_month;
          unsigned short     month;
          unsigned char      day_of_week;
       };
struct fileretrieve_status_6
       {
          char                   dir_alias[MAX_DIR_ALIAS_LENGTH_6 + 1];
          char                   host_alias[MAX_HOSTNAME_LENGTH_6 + 1];
          char                   url[MAX_RECIPIENT_LENGTH_6];
          char                   retrieve_work_dir[MAX_FILENAME_LENGTH_6]; /* New */
          char                   wait_for_filename[MAX_WAIT_FOR_LENGTH];
          char                   timezone[MAX_TIMEZONE_LENGTH_6 + 1]; /* New */
          struct bd_time_entry_6 te[MAX_FRA_TIME_ENTRIES_6];
          struct bd_time_entry_6 ate;
          unsigned char          dir_status;
          unsigned char          remove;
          unsigned char          stupid_mode;
          unsigned char          delete_files_flag;
          unsigned char          report_unknown_files;
          unsigned char          important_dir;
          unsigned char          no_of_time_entries;
          char                   force_reread;
          char                   queued;
          char                   priority;
          unsigned int           protocol;
          unsigned int           files_received;
          unsigned int           dir_flag;
          unsigned int           in_dc_flag;
          unsigned int           files_in_dir;
          unsigned int           files_queued;
          unsigned int           accumulate;
          unsigned int           max_copied_files;
          unsigned int           ignore_file_time;
          unsigned int           gt_lt_sign;
          unsigned int           keep_connected;
#ifdef WITH_DUP_CHECK
          unsigned int           dup_check_flag;
#endif
          mode_t                 dir_mode;                   /* New */
          u_off_t                bytes_received;
          off_t                  bytes_in_dir;
          off_t                  bytes_in_queue;
          off_t                  accumulate_size;
          off_t                  ignore_size;
          off_t                  max_copied_file_size;
#ifdef WITH_DUP_CHECK
          time_t                 dup_check_timeout;
#endif
          time_t                 last_retrieval;
          time_t                 next_check_time;
          time_t                 info_time;                  /* New */
          time_t                 warn_time;
          time_t                 start_event_handle;
          time_t                 end_event_handle;
          int                    unknown_file_time;
          int                    queued_file_time;
          int                    locked_file_time;
          int                    unreadable_file_time;       /* New */
          int                    end_character;
          unsigned int           dir_id;
          int                    fsa_pos;
          int                    no_of_process;
          int                    max_process;
          int                    max_errors;
          unsigned int           error_counter;
       };


/*############################ convert_fra() ############################*/
char *
convert_fra(int           old_fra_fd,
            char          *old_fra_stat,
            off_t         *old_fra_size,
            int           old_no_of_dirs,
            unsigned char old_version,
            unsigned char new_version)
{
   int         i;
   size_t      new_size;
   char        *ptr;
   struct stat stat_buf;

   system_log(INFO_SIGN, NULL, 0, "Converting FRA...");
   if ((old_version == 0) && (new_version == 1))
   {
      struct fileretrieve_status_0 *old_fra;
      struct fileretrieve_status_1 *new_fra;

      /* Get the size of the old FRA file. */
      if (fstat(old_fra_fd, &stat_buf) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to fstat() %s : %s", old_fra_stat, strerror(errno));
         *old_fra_size = -1;
         return(NULL);
      }
      else
      {
         if (stat_buf.st_size > 0)
         {
#ifdef HAVE_MMAP
            if ((ptr = mmap(NULL, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                            MAP_SHARED, old_fra_fd, 0)) == (caddr_t) -1)
#else
            if ((ptr = mmap_emu(NULL, stat_buf.st_size,
                                (PROT_READ | PROT_WRITE),
                                MAP_SHARED, old_fra_stat, 0)) == (caddr_t) -1)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to mmap() to %s : %s",
                          old_fra_stat, strerror(errno));
               *old_fra_size = -1;
               return(NULL);
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "FRA file %s is empty.", old_fra_stat);
            *old_fra_size = -1;
            return(NULL);
         }
      }

      ptr += AFD_WORD_OFFSET_0;
      old_fra = (struct fileretrieve_status_0 *)ptr;

      new_size = old_no_of_dirs * sizeof(struct fileretrieve_status_1);
      if ((ptr = malloc(new_size)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "malloc() error [%d %d] : %s",
                    old_no_of_dirs, new_size, strerror(errno));
         ptr = (char *)old_fra;
         ptr -= AFD_WORD_OFFSET_0;
#ifdef HAVE_MMAP
         if (munmap(ptr, stat_buf.st_size) == -1)
#else
         if (munmap_emu(ptr) == -1)
#endif
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to munmap() %s : %s",
                       old_fra_stat, strerror(errno));
         }
         *old_fra_size = -1;
         return(NULL);
      }
      new_fra = (struct fileretrieve_status_1 *)ptr;

      /*
       * Copy all the old data into the new region.
       */
      for (i = 0; i < old_no_of_dirs; i++)
      {
         (void)strcpy(new_fra[i].dir_alias, old_fra[i].dir_alias);
         (void)strcpy(new_fra[i].host_alias, old_fra[i].host_alias);
         (void)strcpy(new_fra[i].url, old_fra[i].url);
         (void)memcpy(&new_fra[i].te, &old_fra[i].te,
                      sizeof(struct bd_time_entry_0));
         new_fra[i].dir_status           = old_fra[i].dir_status;
         new_fra[i].remove               = old_fra[i].remove;
         new_fra[i].stupid_mode          = old_fra[i].stupid_mode;
         new_fra[i].protocol             = old_fra[i].protocol;
         new_fra[i].delete_files_flag    = old_fra[i].delete_files_flag;
         new_fra[i].report_unknown_files = old_fra[i].report_unknown_files;
         new_fra[i].important_dir        = old_fra[i].important_dir;
         new_fra[i].time_option          = old_fra[i].time_option;
         new_fra[i].force_reread         = old_fra[i].force_reread;
         new_fra[i].queued               = old_fra[i].queued;
         new_fra[i].priority             = old_fra[i].priority;
         new_fra[i].bytes_received       = old_fra[i].bytes_received;
         new_fra[i].files_received       = old_fra[i].files_received;
         new_fra[i].last_retrieval       = old_fra[i].last_retrieval;
         new_fra[i].next_check_time      = old_fra[i].next_check_time;
         new_fra[i].unknown_file_time    = old_fra[i].old_file_time;
         new_fra[i].queued_file_time     = old_fra[i].old_file_time;
         new_fra[i].end_character        = old_fra[i].end_character;
         new_fra[i].dir_pos              = old_fra[i].dir_pos;
         new_fra[i].fsa_pos              = old_fra[i].fsa_pos;
         new_fra[i].no_of_process        = old_fra[i].no_of_process;
         new_fra[i].max_process          = old_fra[i].max_process;
         new_fra[i].dir_flag             = 0;
         new_fra[i].files_in_dir         = 0;
         new_fra[i].files_queued         = 0;
         new_fra[i].bytes_in_dir         = 0;
         new_fra[i].bytes_in_queue       = 0;
      }
      ptr = (char *)old_fra;
      ptr -= AFD_WORD_OFFSET_0;

      /*
       * Resize the old FRA to the size of new one and then copy
       * the new structure into it. Then update the FRA version
       * number.
       */
      if ((ptr = mmap_resize(old_fra_fd, ptr, new_size + AFD_WORD_OFFSET_1)) == (caddr_t) -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to mmap_resize() %s : %s",
                    old_fra_stat, strerror(errno));
         free((void *)new_fra);
         return(NULL);
      }
      ptr += AFD_WORD_OFFSET_1;
      (void)memcpy(ptr, new_fra, new_size);
      free((void *)new_fra);
      ptr -= AFD_WORD_OFFSET_1;
      *(ptr + sizeof(int) + 1 + 1 + 1) = new_version;
      *old_fra_size = new_size + AFD_WORD_OFFSET_1;

      system_log(INFO_SIGN, NULL, 0, "Converted FRA from verion %d to %d.",
                 (int)old_version, (int)new_version);
   }
   else if ((old_version == 1) && (new_version == 2))
        {
           struct fileretrieve_status_1 *old_fra;
           struct fileretrieve_status_2 *new_fra;

           /* Get the size of the old FRA file. */
           if (fstat(old_fra_fd, &stat_buf) < 0)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to fstat() %s : %s",
                         old_fra_stat, strerror(errno));
              *old_fra_size = -1;
              return(NULL);
           }
           else
           {
              if (stat_buf.st_size > 0)
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL, stat_buf.st_size,
                                 (PROT_READ | PROT_WRITE),
                                 MAP_SHARED, old_fra_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL, stat_buf.st_size,
                                     (PROT_READ | PROT_WRITE), MAP_SHARED,
                                     old_fra_stat, 0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fra_stat, strerror(errno));
                    *old_fra_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FRA file %s is empty.", old_fra_stat);
                 *old_fra_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_1;
           old_fra = (struct fileretrieve_status_1 *)ptr;

           new_size = old_no_of_dirs * sizeof(struct fileretrieve_status_2);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_dirs, new_size, strerror(errno));
              ptr = (char *)old_fra;
              ptr -= AFD_WORD_OFFSET_1;
#ifdef HAVE_MMAP
              if (munmap(ptr, stat_buf.st_size) == -1)
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fra_stat, strerror(errno));
              }
              *old_fra_size = -1;
              return(NULL);
           }
           new_fra = (struct fileretrieve_status_2 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_dirs; i++)
           {
              (void)strcpy(new_fra[i].dir_alias, old_fra[i].dir_alias);
              (void)strcpy(new_fra[i].host_alias, old_fra[i].host_alias);
              (void)strcpy(new_fra[i].url, old_fra[i].url);
              (void)memcpy(&new_fra[i].te, &old_fra[i].te,
                           sizeof(struct bd_time_entry_1));
              new_fra[i].dir_status             = old_fra[i].dir_status;
              new_fra[i].remove                 = old_fra[i].remove;
              new_fra[i].stupid_mode            = old_fra[i].stupid_mode;
              new_fra[i].protocol               = old_fra[i].protocol;
              new_fra[i].delete_files_flag      = old_fra[i].delete_files_flag;
              new_fra[i].report_unknown_files   = old_fra[i].report_unknown_files;
              new_fra[i].important_dir          = old_fra[i].important_dir;
              new_fra[i].time_option            = old_fra[i].time_option;
              new_fra[i].force_reread           = old_fra[i].force_reread;
              new_fra[i].queued                 = old_fra[i].queued;
              new_fra[i].priority               = old_fra[i].priority;
              new_fra[i].bytes_received         = old_fra[i].bytes_received;
              new_fra[i].files_received         = old_fra[i].files_received;
              new_fra[i].last_retrieval         = old_fra[i].last_retrieval;
              new_fra[i].next_check_time        = old_fra[i].next_check_time;
              new_fra[i].unknown_file_time      = old_fra[i].unknown_file_time;
              new_fra[i].queued_file_time       = old_fra[i].queued_file_time;
              new_fra[i].end_character          = old_fra[i].end_character;
              new_fra[i].dir_pos                = old_fra[i].dir_pos;
              new_fra[i].fsa_pos                = old_fra[i].fsa_pos;
              new_fra[i].no_of_process          = old_fra[i].no_of_process;
              new_fra[i].max_process            = old_fra[i].max_process;
              new_fra[i].dir_flag               = old_fra[i].dir_flag;
              new_fra[i].files_in_dir           = old_fra[i].files_in_dir;
              new_fra[i].files_queued           = old_fra[i].files_queued;
              new_fra[i].bytes_in_dir           = old_fra[i].bytes_in_dir;
              new_fra[i].bytes_in_queue         = old_fra[i].bytes_in_queue;
              new_fra[i].ignore_file_time       = 0;
              new_fra[i].ignore_size            = 0;
              new_fra[i].gt_lt_sign             = 0;
#ifdef WITH_DUP_CHECK
              new_fra[i].dup_check_flag         = 0;
              new_fra[i].dup_check_timeout      = 0L;
#endif
              new_fra[i].max_copied_files       = max_copied_files;
              new_fra[i].max_copied_file_size   = max_copied_file_size;
              new_fra[i].wait_for_filename[0]   = '\0';
              new_fra[i].accumulate             = 0;
              new_fra[i].accumulate_size        = 0;
              new_fra[i].locked_file_time       = -1;
              (void)memset(&new_fra[i].ate, 0, sizeof(struct bd_time_entry_2));
           }
           ptr = (char *)old_fra;
           ptr -= AFD_WORD_OFFSET_1;

           /*
            * Resize the old FRA to the size of new one and then copy
            * the new structure into it. Then update the FRA version
            * number.
            */
           if ((ptr = mmap_resize(old_fra_fd, ptr, new_size + AFD_WORD_OFFSET_2)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fra_stat, strerror(errno));
              free((void *)new_fra);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_2;
           (void)memcpy(ptr, new_fra, new_size);
           free((void *)new_fra);
           ptr -= AFD_WORD_OFFSET_2;
           *(ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
           *old_fra_size = new_size + AFD_WORD_OFFSET_2;

           system_log(INFO_SIGN, NULL, 0, "Converted FRA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 1) && (new_version == 3))
        {
           struct fileretrieve_status_1 *old_fra;
           struct fileretrieve_status_3 *new_fra;

           /* Get the size of the old FRA file. */
           if (fstat(old_fra_fd, &stat_buf) < 0)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to fstat() %s : %s",
                         old_fra_stat, strerror(errno));
              *old_fra_size = -1;
              return(NULL);
           }
           else
           {
              if (stat_buf.st_size > 0)
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL, stat_buf.st_size,
                                 (PROT_READ | PROT_WRITE),
                                 MAP_SHARED, old_fra_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL, stat_buf.st_size,
                                     (PROT_READ | PROT_WRITE), MAP_SHARED,
                                     old_fra_stat, 0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fra_stat, strerror(errno));
                    *old_fra_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FRA file %s is empty.", old_fra_stat);
                 *old_fra_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_1;
           old_fra = (struct fileretrieve_status_1 *)ptr;

           new_size = old_no_of_dirs * sizeof(struct fileretrieve_status_3);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_dirs, new_size, strerror(errno));
              ptr = (char *)old_fra;
              ptr -= AFD_WORD_OFFSET_1;
#ifdef HAVE_MMAP
              if (munmap(ptr, stat_buf.st_size) == -1)
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fra_stat, strerror(errno));
              }
              *old_fra_size = -1;
              return(NULL);
           }
           new_fra = (struct fileretrieve_status_3 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_dirs; i++)
           {
              (void)strcpy(new_fra[i].dir_alias, old_fra[i].dir_alias);
              (void)strcpy(new_fra[i].host_alias, old_fra[i].host_alias);
              (void)strcpy(new_fra[i].url, old_fra[i].url);
              (void)memcpy(&new_fra[i].te, &old_fra[i].te,
                           sizeof(struct bd_time_entry_1));
              new_fra[i].dir_status             = old_fra[i].dir_status;
              new_fra[i].remove                 = old_fra[i].remove;
              new_fra[i].stupid_mode            = old_fra[i].stupid_mode;
              new_fra[i].protocol               = old_fra[i].protocol;
              new_fra[i].delete_files_flag      = old_fra[i].delete_files_flag;
              new_fra[i].report_unknown_files   = old_fra[i].report_unknown_files;
              new_fra[i].important_dir          = old_fra[i].important_dir;
              new_fra[i].time_option            = old_fra[i].time_option;
              new_fra[i].force_reread           = old_fra[i].force_reread;
              new_fra[i].queued                 = old_fra[i].queued;
              new_fra[i].priority               = old_fra[i].priority;
              new_fra[i].bytes_received         = old_fra[i].bytes_received;
              new_fra[i].files_received         = old_fra[i].files_received;
              new_fra[i].last_retrieval         = old_fra[i].last_retrieval;
              new_fra[i].next_check_time        = old_fra[i].next_check_time;
              new_fra[i].unknown_file_time      = old_fra[i].unknown_file_time;
              new_fra[i].queued_file_time       = old_fra[i].queued_file_time;
              new_fra[i].end_character          = old_fra[i].end_character;
              new_fra[i].dir_id                 = 0;
              new_fra[i].fsa_pos                = old_fra[i].fsa_pos;
              new_fra[i].no_of_process          = old_fra[i].no_of_process;
              new_fra[i].max_process            = old_fra[i].max_process;
              new_fra[i].dir_flag               = old_fra[i].dir_flag;
              if ((old_fra[i].dir_status == DISABLED) &&
                  ((old_fra[i].dir_flag & DIR_DISABLED) == 0))
              {
                 new_fra[i].dir_flag ^= DIR_DISABLED;
              }
              new_fra[i].files_in_dir           = old_fra[i].files_in_dir;
              new_fra[i].files_queued           = old_fra[i].files_queued;
              new_fra[i].bytes_in_dir           = old_fra[i].bytes_in_dir;
              new_fra[i].bytes_in_queue         = old_fra[i].bytes_in_queue;
              new_fra[i].in_dc_flag             = 0;
              new_fra[i].ignore_file_time       = 0;
              new_fra[i].ignore_size            = 0;
              new_fra[i].gt_lt_sign             = 0;
#ifdef WITH_DUP_CHECK
              new_fra[i].dup_check_flag         = 0;
              new_fra[i].dup_check_timeout      = 0L;
#endif
              new_fra[i].max_copied_files       = max_copied_files;
              new_fra[i].max_copied_file_size   = max_copied_file_size;
              new_fra[i].wait_for_filename[0]   = '\0';
              new_fra[i].accumulate             = 0;
              new_fra[i].accumulate_size        = 0;
              new_fra[i].locked_file_time       = -1;
              (void)memset(&new_fra[i].ate, 0, sizeof(struct bd_time_entry_3));
           }
           ptr = (char *)old_fra;
           ptr -= AFD_WORD_OFFSET_1;

           /*
            * Resize the old FRA to the size of new one and then copy
            * the new structure into it. Then update the FRA version
            * number.
            */
           if ((ptr = mmap_resize(old_fra_fd, ptr, new_size + AFD_WORD_OFFSET_3)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fra_stat, strerror(errno));
              free((void *)new_fra);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_3;
           (void)memcpy(ptr, new_fra, new_size);
           free((void *)new_fra);
           ptr -= AFD_WORD_OFFSET_3;
           *(ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
           *old_fra_size = new_size + AFD_WORD_OFFSET_3;

           system_log(INFO_SIGN, NULL, 0, "Converted FRA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 1) && (new_version == 4))
        {
           struct fileretrieve_status_1 *old_fra;
           struct fileretrieve_status_4 *new_fra;

           /* Get the size of the old FRA file. */
           if (fstat(old_fra_fd, &stat_buf) < 0)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to fstat() %s : %s",
                         old_fra_stat, strerror(errno));
              *old_fra_size = -1;
              return(NULL);
           }
           else
           {
              if (stat_buf.st_size > 0)
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL, stat_buf.st_size,
                                 (PROT_READ | PROT_WRITE),
                                 MAP_SHARED, old_fra_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL, stat_buf.st_size,
                                     (PROT_READ | PROT_WRITE), MAP_SHARED,
                                     old_fra_stat, 0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fra_stat, strerror(errno));
                    *old_fra_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FRA file %s is empty.", old_fra_stat);
                 *old_fra_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_1;
           old_fra = (struct fileretrieve_status_1 *)ptr;

           new_size = old_no_of_dirs * sizeof(struct fileretrieve_status_4);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_dirs, new_size, strerror(errno));
              ptr = (char *)old_fra;
              ptr -= AFD_WORD_OFFSET_1;
#ifdef HAVE_MMAP
              if (munmap(ptr, stat_buf.st_size) == -1)
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fra_stat, strerror(errno));
              }
              *old_fra_size = -1;
              return(NULL);
           }
           new_fra = (struct fileretrieve_status_4 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_dirs; i++)
           {
              (void)strcpy(new_fra[i].dir_alias, old_fra[i].dir_alias);
              (void)strcpy(new_fra[i].host_alias, old_fra[i].host_alias);
              (void)strcpy(new_fra[i].url, old_fra[i].url);
              (void)memcpy(&new_fra[i].te, &old_fra[i].te,
                           sizeof(struct bd_time_entry_1));
              new_fra[i].dir_status             = old_fra[i].dir_status;
              new_fra[i].remove                 = old_fra[i].remove;
              new_fra[i].stupid_mode            = old_fra[i].stupid_mode;
              new_fra[i].protocol               = old_fra[i].protocol;
              new_fra[i].delete_files_flag      = old_fra[i].delete_files_flag;
              new_fra[i].report_unknown_files   = old_fra[i].report_unknown_files;
              new_fra[i].important_dir          = old_fra[i].important_dir;
              new_fra[i].time_option            = old_fra[i].time_option;
              new_fra[i].force_reread           = old_fra[i].force_reread;
              new_fra[i].queued                 = old_fra[i].queued;
              new_fra[i].priority               = old_fra[i].priority;
              new_fra[i].bytes_received         = old_fra[i].bytes_received;
              new_fra[i].files_received         = old_fra[i].files_received;
              new_fra[i].last_retrieval         = old_fra[i].last_retrieval;
              new_fra[i].next_check_time        = old_fra[i].next_check_time;
              new_fra[i].unknown_file_time      = old_fra[i].unknown_file_time;
              new_fra[i].queued_file_time       = old_fra[i].queued_file_time;
              new_fra[i].end_character          = old_fra[i].end_character;
              new_fra[i].dir_id                 = 0;
              new_fra[i].fsa_pos                = old_fra[i].fsa_pos;
              new_fra[i].no_of_process          = old_fra[i].no_of_process;
              new_fra[i].max_process            = old_fra[i].max_process;
              new_fra[i].dir_flag               = old_fra[i].dir_flag;
              if ((old_fra[i].dir_status == DISABLED) &&
                  ((old_fra[i].dir_flag & DIR_DISABLED) == 0))
              {
                 new_fra[i].dir_flag ^= DIR_DISABLED;
              }
              new_fra[i].files_in_dir           = old_fra[i].files_in_dir;
              new_fra[i].files_queued           = old_fra[i].files_queued;
              new_fra[i].bytes_in_dir           = old_fra[i].bytes_in_dir;
              new_fra[i].bytes_in_queue         = old_fra[i].bytes_in_queue;
              new_fra[i].in_dc_flag             = 0;
              new_fra[i].ignore_file_time       = 0;
              new_fra[i].ignore_size            = 0;
              new_fra[i].gt_lt_sign             = 0;
              new_fra[i].keep_connected         = DEFAULT_KEEP_CONNECTED_TIME;
#ifdef WITH_DUP_CHECK
              new_fra[i].dup_check_flag         = 0;
              new_fra[i].dup_check_timeout      = 0L;
#endif
              new_fra[i].warn_time              = DEFAULT_DIR_WARN_TIME;
              new_fra[i].max_copied_files       = max_copied_files;
              new_fra[i].max_copied_file_size   = max_copied_file_size;
              new_fra[i].wait_for_filename[0]   = '\0';
              new_fra[i].accumulate             = 0;
              new_fra[i].accumulate_size        = 0;
              new_fra[i].locked_file_time       = -1;
              (void)memset(&new_fra[i].ate, 0, sizeof(struct bd_time_entry_4));
              new_fra[i].max_errors             = 0;
              new_fra[i].error_counter          = 0;
           }
           ptr = (char *)old_fra;
           ptr -= AFD_WORD_OFFSET_1;

           /*
            * Resize the old FRA to the size of new one and then copy
            * the new structure into it. Then update the FRA version
            * number.
            */
           if ((ptr = mmap_resize(old_fra_fd, ptr, new_size + AFD_WORD_OFFSET_4)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fra_stat, strerror(errno));
              free((void *)new_fra);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_4;
           (void)memcpy(ptr, new_fra, new_size);
           free((void *)new_fra);
           ptr -= AFD_WORD_OFFSET_4;
           *(ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
           *old_fra_size = new_size + AFD_WORD_OFFSET_4;

           system_log(INFO_SIGN, NULL, 0, "Converted FRA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 0) && (new_version == 2))
        {
           struct fileretrieve_status_0 *old_fra;
           struct fileretrieve_status_2 *new_fra;

           /* Get the size of the old FRA file. */
           if (fstat(old_fra_fd, &stat_buf) < 0)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to fstat() %s : %s",
                         old_fra_stat, strerror(errno));
              *old_fra_size = -1;
              return(NULL);
           }
           else
           {
              if (stat_buf.st_size > 0)
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL, stat_buf.st_size,
                                 (PROT_READ | PROT_WRITE),
                                 MAP_SHARED, old_fra_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL, stat_buf.st_size,
                                     (PROT_READ | PROT_WRITE), MAP_SHARED,
                                     old_fra_stat, 0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fra_stat, strerror(errno));
                    *old_fra_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FRA file %s is empty.", old_fra_stat);
                 *old_fra_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_0;
           old_fra = (struct fileretrieve_status_0 *)ptr;

           new_size = old_no_of_dirs * sizeof(struct fileretrieve_status_2);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_dirs, new_size, strerror(errno));
              ptr = (char *)old_fra;
              ptr -= AFD_WORD_OFFSET_0;
#ifdef HAVE_MMAP
              if (munmap(ptr, stat_buf.st_size) == -1)
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fra_stat, strerror(errno));
              }
              *old_fra_size = -1;
              return(NULL);
           }
           new_fra = (struct fileretrieve_status_2 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_dirs; i++)
           {
              (void)strcpy(new_fra[i].dir_alias, old_fra[i].dir_alias);
              (void)strcpy(new_fra[i].host_alias, old_fra[i].host_alias);
              (void)strcpy(new_fra[i].url, old_fra[i].url);
              (void)memcpy(&new_fra[i].te, &old_fra[i].te,
                           sizeof(struct bd_time_entry_0));
              new_fra[i].dir_status             = old_fra[i].dir_status;
              new_fra[i].remove                 = old_fra[i].remove;
              new_fra[i].stupid_mode            = old_fra[i].stupid_mode;
              new_fra[i].protocol               = old_fra[i].protocol;
              new_fra[i].delete_files_flag      = old_fra[i].delete_files_flag;
              new_fra[i].report_unknown_files   = old_fra[i].report_unknown_files;
              new_fra[i].important_dir          = old_fra[i].important_dir;
              new_fra[i].time_option            = old_fra[i].time_option;
              new_fra[i].force_reread           = old_fra[i].force_reread;
              new_fra[i].queued                 = old_fra[i].queued;
              new_fra[i].priority               = old_fra[i].priority;
              new_fra[i].bytes_received         = old_fra[i].bytes_received;
              new_fra[i].files_received         = old_fra[i].files_received;
              new_fra[i].last_retrieval         = old_fra[i].last_retrieval;
              new_fra[i].next_check_time        = old_fra[i].next_check_time;
              new_fra[i].unknown_file_time      = old_fra[i].old_file_time;
              new_fra[i].queued_file_time       = old_fra[i].old_file_time;
              new_fra[i].end_character          = old_fra[i].end_character;
              new_fra[i].dir_pos                = old_fra[i].dir_pos;
              new_fra[i].fsa_pos                = old_fra[i].fsa_pos;
              new_fra[i].no_of_process          = old_fra[i].no_of_process;
              new_fra[i].max_process            = old_fra[i].max_process;
              new_fra[i].dir_flag               = 0;
              new_fra[i].files_in_dir           = 0;
              new_fra[i].files_queued           = 0;
              new_fra[i].bytes_in_dir           = 0;
              new_fra[i].bytes_in_queue         = 0;
              new_fra[i].ignore_file_time       = 0;
              new_fra[i].ignore_size            = 0;
              new_fra[i].gt_lt_sign             = 0;
#ifdef WITH_DUP_CHECK
              new_fra[i].dup_check_flag         = 0;
              new_fra[i].dup_check_timeout      = 0L;
#endif
              new_fra[i].max_copied_files       = max_copied_files;
              new_fra[i].max_copied_file_size   = max_copied_file_size;
              new_fra[i].wait_for_filename[0]   = '\0';
              new_fra[i].accumulate             = 0;
              new_fra[i].accumulate_size        = 0;
              new_fra[i].locked_file_time       = -1;
              (void)memset(&new_fra[i].ate, 0, sizeof(struct bd_time_entry_2));
           }
           ptr = (char *)old_fra;
           ptr -= AFD_WORD_OFFSET_0;

           /*
            * Resize the old FRA to the size of new one and then copy
            * the new structure into it. Then update the FRA version
            * number.
            */
           if ((ptr = mmap_resize(old_fra_fd, ptr, new_size + AFD_WORD_OFFSET_2)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fra_stat, strerror(errno));
              free((void *)new_fra);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_2;
           (void)memcpy(ptr, new_fra, new_size);
           free((void *)new_fra);
           ptr -= AFD_WORD_OFFSET_2;
           *(ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
           *old_fra_size = new_size + AFD_WORD_OFFSET_2;

           system_log(DEBUG_SIGN, NULL, 0,
                      "Converted FRA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 0) && (new_version == 3))
        {
           struct fileretrieve_status_0 *old_fra;
           struct fileretrieve_status_3 *new_fra;

           /* Get the size of the old FRA file. */
           if (fstat(old_fra_fd, &stat_buf) < 0)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to fstat() %s : %s",
                         old_fra_stat, strerror(errno));
              *old_fra_size = -1;
              return(NULL);
           }
           else
           {
              if (stat_buf.st_size > 0)
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL, stat_buf.st_size,
                                 (PROT_READ | PROT_WRITE),
                                 MAP_SHARED, old_fra_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL, stat_buf.st_size,
                                     (PROT_READ | PROT_WRITE), MAP_SHARED,
                                     old_fra_stat, 0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fra_stat, strerror(errno));
                    *old_fra_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FRA file %s is empty.", old_fra_stat);
                 *old_fra_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_0;
           old_fra = (struct fileretrieve_status_0 *)ptr;

           new_size = old_no_of_dirs * sizeof(struct fileretrieve_status_3);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_dirs, new_size, strerror(errno));
              ptr = (char *)old_fra;
              ptr -= AFD_WORD_OFFSET_0;
#ifdef HAVE_MMAP
              if (munmap(ptr, stat_buf.st_size) == -1)
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fra_stat, strerror(errno));
              }
              *old_fra_size = -1;
              return(NULL);
           }
           new_fra = (struct fileretrieve_status_3 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_dirs; i++)
           {
              (void)strcpy(new_fra[i].dir_alias, old_fra[i].dir_alias);
              (void)strcpy(new_fra[i].host_alias, old_fra[i].host_alias);
              (void)strcpy(new_fra[i].url, old_fra[i].url);
              (void)memcpy(&new_fra[i].te, &old_fra[i].te,
                           sizeof(struct bd_time_entry_0));
              new_fra[i].dir_status             = old_fra[i].dir_status;
              new_fra[i].remove                 = old_fra[i].remove;
              new_fra[i].stupid_mode            = old_fra[i].stupid_mode;
              new_fra[i].protocol               = old_fra[i].protocol;
              new_fra[i].delete_files_flag      = old_fra[i].delete_files_flag;
              new_fra[i].report_unknown_files   = old_fra[i].report_unknown_files;
              new_fra[i].important_dir          = old_fra[i].important_dir;
              new_fra[i].time_option            = old_fra[i].time_option;
              new_fra[i].force_reread           = old_fra[i].force_reread;
              new_fra[i].queued                 = old_fra[i].queued;
              new_fra[i].priority               = old_fra[i].priority;
              new_fra[i].bytes_received         = old_fra[i].bytes_received;
              new_fra[i].files_received         = old_fra[i].files_received;
              new_fra[i].last_retrieval         = old_fra[i].last_retrieval;
              new_fra[i].next_check_time        = old_fra[i].next_check_time;
              new_fra[i].unknown_file_time      = old_fra[i].old_file_time;
              new_fra[i].queued_file_time       = old_fra[i].old_file_time;
              new_fra[i].end_character          = old_fra[i].end_character;
              new_fra[i].dir_id                 = 0;
              new_fra[i].fsa_pos                = old_fra[i].fsa_pos;
              new_fra[i].no_of_process          = old_fra[i].no_of_process;
              new_fra[i].max_process            = old_fra[i].max_process;
              new_fra[i].dir_flag               = 0;
              if (old_fra[i].dir_status == DISABLED)
              {
                 new_fra[i].dir_flag ^= DIR_DISABLED;
              }
              new_fra[i].in_dc_flag             = 0;
              new_fra[i].files_in_dir           = 0;
              new_fra[i].files_queued           = 0;
              new_fra[i].bytes_in_dir           = 0;
              new_fra[i].bytes_in_queue         = 0;
              new_fra[i].ignore_file_time       = 0;
              new_fra[i].ignore_size            = 0;
              new_fra[i].gt_lt_sign             = 0;
#ifdef WITH_DUP_CHECK
              new_fra[i].dup_check_flag         = 0;
              new_fra[i].dup_check_timeout      = 0L;
#endif
              new_fra[i].max_copied_files       = max_copied_files;
              new_fra[i].max_copied_file_size   = max_copied_file_size;
              new_fra[i].wait_for_filename[0]   = '\0';
              new_fra[i].accumulate             = 0;
              new_fra[i].accumulate_size        = 0;
              new_fra[i].locked_file_time       = -1;
              (void)memset(&new_fra[i].ate, 0, sizeof(struct bd_time_entry_3));
           }
           ptr = (char *)old_fra;
           ptr -= AFD_WORD_OFFSET_0;

           /*
            * Resize the old FRA to the size of new one and then copy
            * the new structure into it. Then update the FRA version
            * number.
            */
           if ((ptr = mmap_resize(old_fra_fd, ptr, new_size + AFD_WORD_OFFSET_3)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fra_stat, strerror(errno));
              free((void *)new_fra);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_3;
           (void)memcpy(ptr, new_fra, new_size);
           free((void *)new_fra);
           ptr -= AFD_WORD_OFFSET_3;
           *(ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
           *old_fra_size = new_size + AFD_WORD_OFFSET_3;

           system_log(DEBUG_SIGN, NULL, 0,
                      "Converted FRA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 0) && (new_version == 4))
        {
           struct fileretrieve_status_0 *old_fra;
           struct fileretrieve_status_4 *new_fra;

           /* Get the size of the old FRA file. */
           if (fstat(old_fra_fd, &stat_buf) < 0)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to fstat() %s : %s",
                         old_fra_stat, strerror(errno));
              *old_fra_size = -1;
              return(NULL);
           }
           else
           {
              if (stat_buf.st_size > 0)
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL, stat_buf.st_size,
                                 (PROT_READ | PROT_WRITE),
                                 MAP_SHARED, old_fra_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL, stat_buf.st_size,
                                     (PROT_READ | PROT_WRITE), MAP_SHARED,
                                     old_fra_stat, 0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fra_stat, strerror(errno));
                    *old_fra_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FRA file %s is empty.", old_fra_stat);
                 *old_fra_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_0;
           old_fra = (struct fileretrieve_status_0 *)ptr;

           new_size = old_no_of_dirs * sizeof(struct fileretrieve_status_4);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_dirs, new_size, strerror(errno));
              ptr = (char *)old_fra;
              ptr -= AFD_WORD_OFFSET_0;
#ifdef HAVE_MMAP
              if (munmap(ptr, stat_buf.st_size) == -1)
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fra_stat, strerror(errno));
              }
              *old_fra_size = -1;
              return(NULL);
           }
           new_fra = (struct fileretrieve_status_4 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_dirs; i++)
           {
              (void)strcpy(new_fra[i].dir_alias, old_fra[i].dir_alias);
              (void)strcpy(new_fra[i].host_alias, old_fra[i].host_alias);
              (void)strcpy(new_fra[i].url, old_fra[i].url);
              (void)memcpy(&new_fra[i].te, &old_fra[i].te,
                           sizeof(struct bd_time_entry_0));
              new_fra[i].dir_status             = old_fra[i].dir_status;
              new_fra[i].remove                 = old_fra[i].remove;
              new_fra[i].stupid_mode            = old_fra[i].stupid_mode;
              new_fra[i].protocol               = old_fra[i].protocol;
              new_fra[i].delete_files_flag      = old_fra[i].delete_files_flag;
              new_fra[i].report_unknown_files   = old_fra[i].report_unknown_files;
              new_fra[i].important_dir          = old_fra[i].important_dir;
              new_fra[i].time_option            = old_fra[i].time_option;
              new_fra[i].force_reread           = old_fra[i].force_reread;
              new_fra[i].queued                 = old_fra[i].queued;
              new_fra[i].priority               = old_fra[i].priority;
              new_fra[i].bytes_received         = old_fra[i].bytes_received;
              new_fra[i].files_received         = old_fra[i].files_received;
              new_fra[i].last_retrieval         = old_fra[i].last_retrieval;
              new_fra[i].next_check_time        = old_fra[i].next_check_time;
              new_fra[i].unknown_file_time      = old_fra[i].old_file_time;
              new_fra[i].queued_file_time       = old_fra[i].old_file_time;
              new_fra[i].end_character          = old_fra[i].end_character;
              new_fra[i].dir_id                 = 0;
              new_fra[i].fsa_pos                = old_fra[i].fsa_pos;
              new_fra[i].no_of_process          = old_fra[i].no_of_process;
              new_fra[i].max_process            = old_fra[i].max_process;
              new_fra[i].dir_flag               = 0;
              if (old_fra[i].dir_status == DISABLED)
              {
                 new_fra[i].dir_flag ^= DIR_DISABLED;
              }
              new_fra[i].in_dc_flag             = 0;
              new_fra[i].files_in_dir           = 0;
              new_fra[i].files_queued           = 0;
              new_fra[i].bytes_in_dir           = 0;
              new_fra[i].bytes_in_queue         = 0;
              new_fra[i].ignore_file_time       = 0;
              new_fra[i].ignore_size            = 0;
              new_fra[i].gt_lt_sign             = 0;
              new_fra[i].keep_connected         = DEFAULT_KEEP_CONNECTED_TIME;
#ifdef WITH_DUP_CHECK
              new_fra[i].dup_check_flag         = 0;
              new_fra[i].dup_check_timeout      = 0L;
#endif
              new_fra[i].warn_time              = DEFAULT_DIR_WARN_TIME;
              new_fra[i].max_copied_files       = max_copied_files;
              new_fra[i].max_copied_file_size   = max_copied_file_size;
              new_fra[i].wait_for_filename[0]   = '\0';
              new_fra[i].accumulate             = 0;
              new_fra[i].accumulate_size        = 0;
              new_fra[i].locked_file_time       = -1;
              (void)memset(&new_fra[i].ate, 0, sizeof(struct bd_time_entry_4));
              new_fra[i].max_errors             = 0;
              new_fra[i].error_counter          = 0;
           }
           ptr = (char *)old_fra;
           ptr -= AFD_WORD_OFFSET_0;

           /*
            * Resize the old FRA to the size of new one and then copy
            * the new structure into it. Then update the FRA version
            * number.
            */
           if ((ptr = mmap_resize(old_fra_fd, ptr, new_size + AFD_WORD_OFFSET_4)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fra_stat, strerror(errno));
              free((void *)new_fra);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_4;
           (void)memcpy(ptr, new_fra, new_size);
           free((void *)new_fra);
           ptr -= AFD_WORD_OFFSET_4;
           *(ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
           *old_fra_size = new_size + AFD_WORD_OFFSET_4;

           system_log(DEBUG_SIGN, NULL, 0,
                      "Converted FRA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 2) && (new_version == 3))
        {
           struct fileretrieve_status_2 *old_fra;
           struct fileretrieve_status_3 *new_fra;

           /* Get the size of the old FRA file. */
           if (fstat(old_fra_fd, &stat_buf) < 0)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to fstat() %s : %s",
                         old_fra_stat, strerror(errno));
              *old_fra_size = -1;
              return(NULL);
           }
           else
           {
              if (stat_buf.st_size > 0)
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL, stat_buf.st_size,
                                 (PROT_READ | PROT_WRITE),
                                 MAP_SHARED, old_fra_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL, stat_buf.st_size,
                                     (PROT_READ | PROT_WRITE), MAP_SHARED,
                                     old_fra_stat, 0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fra_stat, strerror(errno));
                    *old_fra_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FRA file %s is empty.", old_fra_stat);
                 *old_fra_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_2;
           old_fra = (struct fileretrieve_status_2 *)ptr;

           new_size = old_no_of_dirs * sizeof(struct fileretrieve_status_3);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_dirs, new_size, strerror(errno));
              ptr = (char *)old_fra;
              ptr -= AFD_WORD_OFFSET_2;
#ifdef HAVE_MMAP
              if (munmap(ptr, stat_buf.st_size) == -1)
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fra_stat, strerror(errno));
              }
              *old_fra_size = -1;
              return(NULL);
           }
           new_fra = (struct fileretrieve_status_3 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_dirs; i++)
           {
              (void)strcpy(new_fra[i].dir_alias, old_fra[i].dir_alias);
              (void)strcpy(new_fra[i].host_alias, old_fra[i].host_alias);
              (void)strcpy(new_fra[i].url, old_fra[i].url);
              (void)memcpy(&new_fra[i].te, &old_fra[i].te,
                           sizeof(struct bd_time_entry_2));
              new_fra[i].dir_status             = old_fra[i].dir_status;
              new_fra[i].remove                 = old_fra[i].remove;
              new_fra[i].stupid_mode            = old_fra[i].stupid_mode;
              new_fra[i].protocol               = old_fra[i].protocol;
              new_fra[i].delete_files_flag      = old_fra[i].delete_files_flag;
              new_fra[i].report_unknown_files   = old_fra[i].report_unknown_files;
              new_fra[i].important_dir          = old_fra[i].important_dir;
              new_fra[i].time_option            = old_fra[i].time_option;
              new_fra[i].force_reread           = old_fra[i].force_reread;
              new_fra[i].queued                 = old_fra[i].queued;
              new_fra[i].priority               = old_fra[i].priority;
              new_fra[i].bytes_received         = old_fra[i].bytes_received;
              new_fra[i].files_received         = old_fra[i].files_received;
              new_fra[i].last_retrieval         = old_fra[i].last_retrieval;
              new_fra[i].next_check_time        = old_fra[i].next_check_time;
              new_fra[i].unknown_file_time      = old_fra[i].unknown_file_time;
              new_fra[i].queued_file_time       = old_fra[i].queued_file_time;
              new_fra[i].end_character          = old_fra[i].end_character;
              new_fra[i].dir_id                 = 0;
              new_fra[i].fsa_pos                = old_fra[i].fsa_pos;
              new_fra[i].no_of_process          = old_fra[i].no_of_process;
              new_fra[i].max_process            = old_fra[i].max_process;
              new_fra[i].dir_flag               = old_fra[i].dir_flag;
              if ((old_fra[i].dir_status == DISABLED) &&
                  ((old_fra[i].dir_flag & DIR_DISABLED) == 0))
              {
                 new_fra[i].dir_flag ^= DIR_DISABLED;
              }
              new_fra[i].in_dc_flag             = 0;
              new_fra[i].files_in_dir           = old_fra[i].files_in_dir;
              new_fra[i].files_queued           = old_fra[i].files_queued;
              new_fra[i].bytes_in_dir           = old_fra[i].bytes_in_dir;
              new_fra[i].bytes_in_queue         = old_fra[i].bytes_in_queue;
              new_fra[i].ignore_file_time       = old_fra[i].ignore_file_time;
              new_fra[i].ignore_size            = old_fra[i].ignore_size;
              new_fra[i].gt_lt_sign             = old_fra[i].gt_lt_sign;
#ifdef WITH_DUP_CHECK
              new_fra[i].dup_check_flag         = old_fra[i].dup_check_flag;
              new_fra[i].dup_check_timeout      = old_fra[i].dup_check_timeout;
#endif
              new_fra[i].max_copied_files       = max_copied_files;
              new_fra[i].max_copied_file_size   = max_copied_file_size;
              if (old_fra[i].wait_for_filename[0] == '\0')
              {
                 new_fra[i].wait_for_filename[0] = '\0';
              }
              else
              {
                 (void)strcpy(new_fra[i].wait_for_filename,
                              old_fra[i].wait_for_filename);
              }
              new_fra[i].accumulate             = old_fra[i].accumulate;
              new_fra[i].accumulate_size        = old_fra[i].accumulate_size;
              new_fra[i].locked_file_time       = old_fra[i].locked_file_time;
              (void)memcpy(&new_fra[i].ate, &old_fra[i].ate,
                           sizeof(struct bd_time_entry_2));
           }
           ptr = (char *)old_fra;
           ptr -= AFD_WORD_OFFSET_2;

           /*
            * Resize the old FRA to the size of new one and then copy
            * the new structure into it. Then update the FRA version
            * number.
            */
           if ((ptr = mmap_resize(old_fra_fd, ptr, new_size + AFD_WORD_OFFSET_3)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fra_stat, strerror(errno));
              free((void *)new_fra);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_3;
           (void)memcpy(ptr, new_fra, new_size);
           free((void *)new_fra);
           ptr -= AFD_WORD_OFFSET_3;
           *(ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
           *old_fra_size = new_size + AFD_WORD_OFFSET_3;

           system_log(INFO_SIGN, NULL, 0, "Converted FRA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 2) && (new_version == 4))
        {
           struct fileretrieve_status_2 *old_fra;
           struct fileretrieve_status_4 *new_fra;

           /* Get the size of the old FRA file. */
           if (fstat(old_fra_fd, &stat_buf) < 0)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to fstat() %s : %s",
                         old_fra_stat, strerror(errno));
              *old_fra_size = -1;
              return(NULL);
           }
           else
           {
              if (stat_buf.st_size > 0)
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL, stat_buf.st_size,
                                 (PROT_READ | PROT_WRITE),
                                 MAP_SHARED, old_fra_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL, stat_buf.st_size,
                                     (PROT_READ | PROT_WRITE), MAP_SHARED,
                                     old_fra_stat, 0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fra_stat, strerror(errno));
                    *old_fra_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FRA file %s is empty.", old_fra_stat);
                 *old_fra_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_2;
           old_fra = (struct fileretrieve_status_2 *)ptr;

           new_size = old_no_of_dirs * sizeof(struct fileretrieve_status_4);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_dirs, new_size, strerror(errno));
              ptr = (char *)old_fra;
              ptr -= AFD_WORD_OFFSET_2;
#ifdef HAVE_MMAP
              if (munmap(ptr, stat_buf.st_size) == -1)
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fra_stat, strerror(errno));
              }
              *old_fra_size = -1;
              return(NULL);
           }
           new_fra = (struct fileretrieve_status_4 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_dirs; i++)
           {
              (void)strcpy(new_fra[i].dir_alias, old_fra[i].dir_alias);
              (void)strcpy(new_fra[i].host_alias, old_fra[i].host_alias);
              (void)strcpy(new_fra[i].url, old_fra[i].url);
              (void)memcpy(&new_fra[i].te, &old_fra[i].te,
                           sizeof(struct bd_time_entry_2));
              new_fra[i].dir_status             = old_fra[i].dir_status;
              new_fra[i].remove                 = old_fra[i].remove;
              new_fra[i].stupid_mode            = old_fra[i].stupid_mode;
              new_fra[i].protocol               = old_fra[i].protocol;
              new_fra[i].delete_files_flag      = old_fra[i].delete_files_flag;
              new_fra[i].report_unknown_files   = old_fra[i].report_unknown_files;
              new_fra[i].important_dir          = old_fra[i].important_dir;
              new_fra[i].time_option            = old_fra[i].time_option;
              new_fra[i].force_reread           = old_fra[i].force_reread;
              new_fra[i].queued                 = old_fra[i].queued;
              new_fra[i].priority               = old_fra[i].priority;
              new_fra[i].bytes_received         = old_fra[i].bytes_received;
              new_fra[i].files_received         = old_fra[i].files_received;
              new_fra[i].last_retrieval         = old_fra[i].last_retrieval;
              new_fra[i].next_check_time        = old_fra[i].next_check_time;
              new_fra[i].unknown_file_time      = old_fra[i].unknown_file_time;
              new_fra[i].queued_file_time       = old_fra[i].queued_file_time;
              new_fra[i].end_character          = old_fra[i].end_character;
              new_fra[i].dir_id                 = 0;
              new_fra[i].fsa_pos                = old_fra[i].fsa_pos;
              new_fra[i].no_of_process          = old_fra[i].no_of_process;
              new_fra[i].max_process            = old_fra[i].max_process;
              new_fra[i].dir_flag               = old_fra[i].dir_flag;
              if ((old_fra[i].dir_status == DISABLED) &&
                  ((old_fra[i].dir_flag & DIR_DISABLED) == 0))
              {
                 new_fra[i].dir_flag ^= DIR_DISABLED;
              }
              new_fra[i].in_dc_flag             = 0;
              new_fra[i].files_in_dir           = old_fra[i].files_in_dir;
              new_fra[i].files_queued           = old_fra[i].files_queued;
              new_fra[i].bytes_in_dir           = old_fra[i].bytes_in_dir;
              new_fra[i].bytes_in_queue         = old_fra[i].bytes_in_queue;
              new_fra[i].ignore_file_time       = old_fra[i].ignore_file_time;
              new_fra[i].ignore_size            = old_fra[i].ignore_size;
              new_fra[i].gt_lt_sign             = old_fra[i].gt_lt_sign;
              new_fra[i].keep_connected         = DEFAULT_KEEP_CONNECTED_TIME;
#ifdef WITH_DUP_CHECK
              new_fra[i].dup_check_flag         = old_fra[i].dup_check_flag;
              new_fra[i].dup_check_timeout      = old_fra[i].dup_check_timeout;
#endif
              new_fra[i].warn_time              = DEFAULT_DIR_WARN_TIME;
              new_fra[i].max_copied_files       = max_copied_files;
              new_fra[i].max_copied_file_size   = max_copied_file_size;
              if (old_fra[i].wait_for_filename[0] == '\0')
              {
                 new_fra[i].wait_for_filename[0] = '\0';
              }
              else
              {
                 (void)strcpy(new_fra[i].wait_for_filename,
                              old_fra[i].wait_for_filename);
              }
              new_fra[i].accumulate             = old_fra[i].accumulate;
              new_fra[i].accumulate_size        = old_fra[i].accumulate_size;
              new_fra[i].locked_file_time       = old_fra[i].locked_file_time;
              (void)memcpy(&new_fra[i].ate, &old_fra[i].ate,
                           sizeof(struct bd_time_entry_2));
              new_fra[i].max_errors             = 0;
              new_fra[i].error_counter          = 0;
           }
           ptr = (char *)old_fra;
           ptr -= AFD_WORD_OFFSET_2;

           /*
            * Resize the old FRA to the size of new one and then copy
            * the new structure into it. Then update the FRA version
            * number.
            */
           if ((ptr = mmap_resize(old_fra_fd, ptr, new_size + AFD_WORD_OFFSET_4)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fra_stat, strerror(errno));
              free((void *)new_fra);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_4;
           (void)memcpy(ptr, new_fra, new_size);
           free((void *)new_fra);
           ptr -= AFD_WORD_OFFSET_4;
           *(ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
           *old_fra_size = new_size + AFD_WORD_OFFSET_4;

           system_log(INFO_SIGN, NULL, 0, "Converted FRA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 3) && (new_version == 4))
        {
           struct fileretrieve_status_3 *old_fra;
           struct fileretrieve_status_4 *new_fra;

           /* Get the size of the old FRA file. */
           if (fstat(old_fra_fd, &stat_buf) < 0)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to fstat() %s : %s",
                         old_fra_stat, strerror(errno));
              *old_fra_size = -1;
              return(NULL);
           }
           else
           {
              if (stat_buf.st_size > 0)
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL, stat_buf.st_size,
                                 (PROT_READ | PROT_WRITE),
                                 MAP_SHARED, old_fra_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL, stat_buf.st_size,
                                     (PROT_READ | PROT_WRITE), MAP_SHARED,
                                     old_fra_stat, 0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fra_stat, strerror(errno));
                    *old_fra_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FRA file %s is empty.", old_fra_stat);
                 *old_fra_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_3;
           old_fra = (struct fileretrieve_status_3 *)ptr;

           new_size = old_no_of_dirs * sizeof(struct fileretrieve_status_4);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_dirs, new_size, strerror(errno));
              ptr = (char *)old_fra;
              ptr -= AFD_WORD_OFFSET_3;
#ifdef HAVE_MMAP
              if (munmap(ptr, stat_buf.st_size) == -1)
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fra_stat, strerror(errno));
              }
              *old_fra_size = -1;
              return(NULL);
           }
           new_fra = (struct fileretrieve_status_4 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_dirs; i++)
           {
              (void)strcpy(new_fra[i].dir_alias, old_fra[i].dir_alias);
              (void)strcpy(new_fra[i].host_alias, old_fra[i].host_alias);
              (void)strcpy(new_fra[i].url, old_fra[i].url);
              (void)memcpy(&new_fra[i].te, &old_fra[i].te,
                           sizeof(struct bd_time_entry_3));
              new_fra[i].dir_status             = old_fra[i].dir_status;
              new_fra[i].remove                 = old_fra[i].remove;
              new_fra[i].stupid_mode            = old_fra[i].stupid_mode;
              new_fra[i].delete_files_flag      = old_fra[i].delete_files_flag;
              new_fra[i].report_unknown_files   = old_fra[i].report_unknown_files;
              new_fra[i].important_dir          = old_fra[i].important_dir;
              new_fra[i].time_option            = old_fra[i].time_option;
              new_fra[i].force_reread           = old_fra[i].force_reread;
              new_fra[i].queued                 = old_fra[i].queued;
              new_fra[i].priority               = old_fra[i].priority;
              new_fra[i].protocol               = old_fra[i].protocol;
              new_fra[i].bytes_received         = old_fra[i].bytes_received;
              new_fra[i].files_received         = old_fra[i].files_received;
              new_fra[i].last_retrieval         = old_fra[i].last_retrieval;
              new_fra[i].next_check_time        = old_fra[i].next_check_time;
              new_fra[i].unknown_file_time      = old_fra[i].unknown_file_time;
              new_fra[i].queued_file_time       = old_fra[i].queued_file_time;
              new_fra[i].end_character          = old_fra[i].end_character;
              new_fra[i].dir_id                 = old_fra[i].dir_id;
              new_fra[i].fsa_pos                = old_fra[i].fsa_pos;
              new_fra[i].no_of_process          = old_fra[i].no_of_process;
              new_fra[i].max_process            = old_fra[i].max_process;
              new_fra[i].dir_flag               = old_fra[i].dir_flag;
              if ((old_fra[i].dir_status == DISABLED) &&
                  ((old_fra[i].dir_flag & DIR_DISABLED) == 0))
              {
                 new_fra[i].dir_flag ^= DIR_DISABLED;
              }
              new_fra[i].in_dc_flag             = old_fra[i].in_dc_flag;
              new_fra[i].files_in_dir           = old_fra[i].files_in_dir;
              new_fra[i].files_queued           = old_fra[i].files_queued;
              new_fra[i].bytes_in_dir           = old_fra[i].bytes_in_dir;
              new_fra[i].bytes_in_queue         = old_fra[i].bytes_in_queue;
              new_fra[i].ignore_file_time       = old_fra[i].ignore_file_time;
              new_fra[i].ignore_size            = old_fra[i].ignore_size;
              new_fra[i].gt_lt_sign             = old_fra[i].gt_lt_sign;
              new_fra[i].keep_connected         = DEFAULT_KEEP_CONNECTED_TIME;
#ifdef WITH_DUP_CHECK
              new_fra[i].dup_check_flag         = old_fra[i].dup_check_flag;
              new_fra[i].dup_check_timeout      = old_fra[i].dup_check_timeout;
#endif
              new_fra[i].warn_time              = DEFAULT_DIR_WARN_TIME;
              new_fra[i].max_copied_files       = max_copied_files;
              new_fra[i].max_copied_file_size   = max_copied_file_size;
              if (old_fra[i].wait_for_filename[0] == '\0')
              {
                 new_fra[i].wait_for_filename[0] = '\0';
              }
              else
              {
                 (void)strcpy(new_fra[i].wait_for_filename,
                              old_fra[i].wait_for_filename);
              }
              new_fra[i].accumulate             = old_fra[i].accumulate;
              new_fra[i].accumulate_size        = old_fra[i].accumulate_size;
              new_fra[i].locked_file_time       = old_fra[i].locked_file_time;
              (void)memcpy(&new_fra[i].ate, &old_fra[i].ate,
                           sizeof(struct bd_time_entry_3));
              new_fra[i].max_errors             = 0;
              new_fra[i].error_counter          = 0;
           }
           ptr = (char *)old_fra;
           ptr -= AFD_WORD_OFFSET_3;

           /*
            * Resize the old FRA to the size of new one and then copy
            * the new structure into it. Then update the FRA version
            * number.
            */
           if ((ptr = mmap_resize(old_fra_fd, ptr, new_size + AFD_WORD_OFFSET_4)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fra_stat, strerror(errno));
              free((void *)new_fra);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_4;
           (void)memcpy(ptr, new_fra, new_size);
           free((void *)new_fra);
           ptr -= AFD_WORD_OFFSET_4;
           *(ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
           *old_fra_size = new_size + AFD_WORD_OFFSET_4;

           system_log(INFO_SIGN, NULL, 0, "Converted FRA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 0) && (new_version == 5))
        {
           struct fileretrieve_status_0 *old_fra;
           struct fileretrieve_status_5 *new_fra;

           /* Get the size of the old FRA file. */
           if (fstat(old_fra_fd, &stat_buf) < 0)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to fstat() %s : %s",
                         old_fra_stat, strerror(errno));
              *old_fra_size = -1;
              return(NULL);
           }
           else
           {
              if (stat_buf.st_size > 0)
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL, stat_buf.st_size,
                                 (PROT_READ | PROT_WRITE),
                                 MAP_SHARED, old_fra_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL, stat_buf.st_size,
                                     (PROT_READ | PROT_WRITE), MAP_SHARED,
                                     old_fra_stat, 0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fra_stat, strerror(errno));
                    *old_fra_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FRA file %s is empty.", old_fra_stat);
                 *old_fra_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_0;
           old_fra = (struct fileretrieve_status_0 *)ptr;

           new_size = old_no_of_dirs * sizeof(struct fileretrieve_status_5);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_dirs, new_size, strerror(errno));
              ptr = (char *)old_fra;
              ptr -= AFD_WORD_OFFSET_0;
#ifdef HAVE_MMAP
              if (munmap(ptr, stat_buf.st_size) == -1)
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fra_stat, strerror(errno));
              }
              *old_fra_size = -1;
              return(NULL);
           }
           new_fra = (struct fileretrieve_status_5 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_dirs; i++)
           {
              (void)strcpy(new_fra[i].dir_alias, old_fra[i].dir_alias);
              (void)strcpy(new_fra[i].host_alias, old_fra[i].host_alias);
              (void)strcpy(new_fra[i].url, old_fra[i].url);
              (void)memset(&new_fra[i].te[0], 0,
                           (MAX_FRA_TIME_ENTRIES_5 * sizeof(struct bd_time_entry_5)));
              new_fra[i].dir_status             = old_fra[i].dir_status;
              new_fra[i].remove                 = old_fra[i].remove;
              new_fra[i].stupid_mode            = old_fra[i].stupid_mode;
              new_fra[i].protocol               = old_fra[i].protocol;
              new_fra[i].delete_files_flag      = old_fra[i].delete_files_flag;
              new_fra[i].report_unknown_files   = old_fra[i].report_unknown_files;
              new_fra[i].important_dir          = old_fra[i].important_dir;
              new_fra[i].no_of_time_entries     = 0;
              new_fra[i].force_reread           = old_fra[i].force_reread;
              new_fra[i].queued                 = old_fra[i].queued;
              new_fra[i].priority               = old_fra[i].priority;
              new_fra[i].bytes_received         = old_fra[i].bytes_received;
              new_fra[i].files_received         = old_fra[i].files_received;
              new_fra[i].last_retrieval         = old_fra[i].last_retrieval;
              new_fra[i].next_check_time        = old_fra[i].next_check_time;
              new_fra[i].unknown_file_time      = old_fra[i].old_file_time;
              new_fra[i].queued_file_time       = old_fra[i].old_file_time;
              new_fra[i].end_character          = old_fra[i].end_character;
              new_fra[i].dir_id                 = 0;
              new_fra[i].fsa_pos                = old_fra[i].fsa_pos;
              new_fra[i].no_of_process          = old_fra[i].no_of_process;
              new_fra[i].max_process            = old_fra[i].max_process;
              new_fra[i].dir_flag               = 0;
              if (old_fra[i].dir_status == DISABLED)
              {
                 new_fra[i].dir_flag ^= DIR_DISABLED;
              }
              new_fra[i].in_dc_flag             = 0;
              new_fra[i].files_in_dir           = 0;
              new_fra[i].files_queued           = 0;
              new_fra[i].bytes_in_dir           = 0;
              new_fra[i].bytes_in_queue         = 0;
              new_fra[i].ignore_file_time       = 0;
              new_fra[i].ignore_size            = -1;
              new_fra[i].gt_lt_sign             = 0;
              new_fra[i].keep_connected         = DEFAULT_KEEP_CONNECTED_TIME;
#ifdef WITH_DUP_CHECK
              new_fra[i].dup_check_flag         = 0;
              new_fra[i].dup_check_timeout      = 0L;
#endif
              new_fra[i].warn_time              = DEFAULT_DIR_WARN_TIME;
              new_fra[i].start_event_handle     = 0L;
              new_fra[i].end_event_handle       = 0L;
              new_fra[i].max_copied_files       = max_copied_files;
              new_fra[i].max_copied_file_size   = max_copied_file_size;
              new_fra[i].wait_for_filename[0]   = '\0';
              new_fra[i].accumulate             = 0;
              new_fra[i].accumulate_size        = 0;
              new_fra[i].locked_file_time       = -1;
              (void)memset(&new_fra[i].ate, 0, sizeof(struct bd_time_entry_5));
              new_fra[i].max_errors             = 0;
              new_fra[i].error_counter          = 0;
           }
           ptr = (char *)old_fra;
           ptr -= AFD_WORD_OFFSET_0;

           /*
            * Resize the old FRA to the size of new one and then copy
            * the new structure into it. Then update the FRA version
            * number.
            */
           if ((ptr = mmap_resize(old_fra_fd, ptr, new_size + AFD_WORD_OFFSET_5)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fra_stat, strerror(errno));
              free((void *)new_fra);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_5;
           (void)memcpy(ptr, new_fra, new_size);
           free((void *)new_fra);
           ptr -= AFD_WORD_OFFSET_5;
           *(ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
           *old_fra_size = new_size + AFD_WORD_OFFSET_5;

           system_log(DEBUG_SIGN, NULL, 0,
                      "Converted FRA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 1) && (new_version == 5))
        {
           struct fileretrieve_status_1 *old_fra;
           struct fileretrieve_status_5 *new_fra;

           /* Get the size of the old FRA file. */
           if (fstat(old_fra_fd, &stat_buf) < 0)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to fstat() %s : %s",
                         old_fra_stat, strerror(errno));
              *old_fra_size = -1;
              return(NULL);
           }
           else
           {
              if (stat_buf.st_size > 0)
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL, stat_buf.st_size,
                                 (PROT_READ | PROT_WRITE),
                                 MAP_SHARED, old_fra_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL, stat_buf.st_size,
                                     (PROT_READ | PROT_WRITE), MAP_SHARED,
                                     old_fra_stat, 0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fra_stat, strerror(errno));
                    *old_fra_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FRA file %s is empty.", old_fra_stat);
                 *old_fra_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_1;
           old_fra = (struct fileretrieve_status_1 *)ptr;

           new_size = old_no_of_dirs * sizeof(struct fileretrieve_status_5);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_dirs, new_size, strerror(errno));
              ptr = (char *)old_fra;
              ptr -= AFD_WORD_OFFSET_1;
#ifdef HAVE_MMAP
              if (munmap(ptr, stat_buf.st_size) == -1)
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fra_stat, strerror(errno));
              }
              *old_fra_size = -1;
              return(NULL);
           }
           new_fra = (struct fileretrieve_status_5 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_dirs; i++)
           {
              (void)strcpy(new_fra[i].dir_alias, old_fra[i].dir_alias);
              (void)strcpy(new_fra[i].host_alias, old_fra[i].host_alias);
              (void)strcpy(new_fra[i].url, old_fra[i].url);
              (void)memset(&new_fra[i].te[0], 0,
                           (MAX_FRA_TIME_ENTRIES_5 * sizeof(struct bd_time_entry_5)));
              new_fra[i].dir_status             = old_fra[i].dir_status;
              new_fra[i].remove                 = old_fra[i].remove;
              new_fra[i].stupid_mode            = old_fra[i].stupid_mode;
              new_fra[i].protocol               = old_fra[i].protocol;
              new_fra[i].delete_files_flag      = old_fra[i].delete_files_flag;
              new_fra[i].report_unknown_files   = old_fra[i].report_unknown_files;
              new_fra[i].important_dir          = old_fra[i].important_dir;
              new_fra[i].no_of_time_entries     = 0;
              new_fra[i].force_reread           = old_fra[i].force_reread;
              new_fra[i].queued                 = old_fra[i].queued;
              new_fra[i].priority               = old_fra[i].priority;
              new_fra[i].bytes_received         = old_fra[i].bytes_received;
              new_fra[i].files_received         = old_fra[i].files_received;
              new_fra[i].last_retrieval         = old_fra[i].last_retrieval;
              new_fra[i].next_check_time        = old_fra[i].next_check_time;
              new_fra[i].unknown_file_time      = old_fra[i].unknown_file_time;
              new_fra[i].queued_file_time       = old_fra[i].queued_file_time;
              new_fra[i].end_character          = old_fra[i].end_character;
              new_fra[i].dir_id                 = 0;
              new_fra[i].fsa_pos                = old_fra[i].fsa_pos;
              new_fra[i].no_of_process          = old_fra[i].no_of_process;
              new_fra[i].max_process            = old_fra[i].max_process;
              new_fra[i].dir_flag               = old_fra[i].dir_flag;
              if ((old_fra[i].dir_status == DISABLED) &&
                  ((old_fra[i].dir_flag & DIR_DISABLED) == 0))
              {
                 new_fra[i].dir_flag ^= DIR_DISABLED;
              }
              new_fra[i].files_in_dir           = old_fra[i].files_in_dir;
              new_fra[i].files_queued           = old_fra[i].files_queued;
              new_fra[i].bytes_in_dir           = old_fra[i].bytes_in_dir;
              new_fra[i].bytes_in_queue         = old_fra[i].bytes_in_queue;
              new_fra[i].in_dc_flag             = 0;
              new_fra[i].ignore_file_time       = 0;
              new_fra[i].ignore_size            = -1;
              new_fra[i].gt_lt_sign             = 0;
              new_fra[i].keep_connected         = DEFAULT_KEEP_CONNECTED_TIME;
#ifdef WITH_DUP_CHECK
              new_fra[i].dup_check_flag         = 0;
              new_fra[i].dup_check_timeout      = 0L;
#endif
              new_fra[i].warn_time              = DEFAULT_DIR_WARN_TIME;
              new_fra[i].start_event_handle     = 0L;
              new_fra[i].end_event_handle       = 0L;
              new_fra[i].max_copied_files       = max_copied_files;
              new_fra[i].max_copied_file_size   = max_copied_file_size;
              new_fra[i].wait_for_filename[0]   = '\0';
              new_fra[i].accumulate             = 0;
              new_fra[i].accumulate_size        = 0;
              new_fra[i].locked_file_time       = -1;
              (void)memset(&new_fra[i].ate, 0, sizeof(struct bd_time_entry_5));
              new_fra[i].max_errors             = 0;
              new_fra[i].error_counter          = 0;
           }
           ptr = (char *)old_fra;
           ptr -= AFD_WORD_OFFSET_1;

           /*
            * Resize the old FRA to the size of new one and then copy
            * the new structure into it. Then update the FRA version
            * number.
            */
           if ((ptr = mmap_resize(old_fra_fd, ptr, new_size + AFD_WORD_OFFSET_5)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fra_stat, strerror(errno));
              free((void *)new_fra);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_5;
           (void)memcpy(ptr, new_fra, new_size);
           free((void *)new_fra);
           ptr -= AFD_WORD_OFFSET_5;
           *(ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
           *old_fra_size = new_size + AFD_WORD_OFFSET_5;

           system_log(INFO_SIGN, NULL, 0, "Converted FRA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 2) && (new_version == 5))
        {
           struct fileretrieve_status_2 *old_fra;
           struct fileretrieve_status_5 *new_fra;

           /* Get the size of the old FRA file. */
           if (fstat(old_fra_fd, &stat_buf) < 0)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to fstat() %s : %s",
                         old_fra_stat, strerror(errno));
              *old_fra_size = -1;
              return(NULL);
           }
           else
           {
              if (stat_buf.st_size > 0)
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL, stat_buf.st_size,
                                 (PROT_READ | PROT_WRITE),
                                 MAP_SHARED, old_fra_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL, stat_buf.st_size,
                                     (PROT_READ | PROT_WRITE), MAP_SHARED,
                                     old_fra_stat, 0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fra_stat, strerror(errno));
                    *old_fra_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FRA file %s is empty.", old_fra_stat);
                 *old_fra_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_2;
           old_fra = (struct fileretrieve_status_2 *)ptr;

           new_size = old_no_of_dirs * sizeof(struct fileretrieve_status_5);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_dirs, new_size, strerror(errno));
              ptr = (char *)old_fra;
              ptr -= AFD_WORD_OFFSET_2;
#ifdef HAVE_MMAP
              if (munmap(ptr, stat_buf.st_size) == -1)
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fra_stat, strerror(errno));
              }
              *old_fra_size = -1;
              return(NULL);
           }
           new_fra = (struct fileretrieve_status_5 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_dirs; i++)
           {
              (void)strcpy(new_fra[i].dir_alias, old_fra[i].dir_alias);
              (void)strcpy(new_fra[i].host_alias, old_fra[i].host_alias);
              (void)strcpy(new_fra[i].url, old_fra[i].url);
              (void)memset(&new_fra[i].te[0], 0,
                           (MAX_FRA_TIME_ENTRIES_5 * sizeof(struct bd_time_entry_5)));
              new_fra[i].dir_status             = old_fra[i].dir_status;
              new_fra[i].remove                 = old_fra[i].remove;
              new_fra[i].stupid_mode            = old_fra[i].stupid_mode;
              new_fra[i].protocol               = old_fra[i].protocol;
              new_fra[i].delete_files_flag      = old_fra[i].delete_files_flag;
              new_fra[i].report_unknown_files   = old_fra[i].report_unknown_files;
              new_fra[i].important_dir          = old_fra[i].important_dir;
              new_fra[i].no_of_time_entries     = 0;
              new_fra[i].force_reread           = old_fra[i].force_reread;
              new_fra[i].queued                 = old_fra[i].queued;
              new_fra[i].priority               = old_fra[i].priority;
              new_fra[i].bytes_received         = old_fra[i].bytes_received;
              new_fra[i].files_received         = old_fra[i].files_received;
              new_fra[i].last_retrieval         = old_fra[i].last_retrieval;
              new_fra[i].next_check_time        = old_fra[i].next_check_time;
              new_fra[i].unknown_file_time      = old_fra[i].unknown_file_time;
              new_fra[i].queued_file_time       = old_fra[i].queued_file_time;
              new_fra[i].end_character          = old_fra[i].end_character;
              new_fra[i].dir_id                 = 0;
              new_fra[i].fsa_pos                = old_fra[i].fsa_pos;
              new_fra[i].no_of_process          = old_fra[i].no_of_process;
              new_fra[i].max_process            = old_fra[i].max_process;
              new_fra[i].dir_flag               = old_fra[i].dir_flag;
              if ((old_fra[i].dir_status == DISABLED) &&
                  ((old_fra[i].dir_flag & DIR_DISABLED) == 0))
              {
                 new_fra[i].dir_flag ^= DIR_DISABLED;
              }
              new_fra[i].in_dc_flag             = 0;
              new_fra[i].files_in_dir           = old_fra[i].files_in_dir;
              new_fra[i].files_queued           = old_fra[i].files_queued;
              new_fra[i].bytes_in_dir           = old_fra[i].bytes_in_dir;
              new_fra[i].bytes_in_queue         = old_fra[i].bytes_in_queue;
              new_fra[i].ignore_file_time       = old_fra[i].ignore_file_time;
              new_fra[i].ignore_size            = old_fra[i].ignore_size;
              new_fra[i].gt_lt_sign             = old_fra[i].gt_lt_sign;
              new_fra[i].keep_connected         = DEFAULT_KEEP_CONNECTED_TIME;
#ifdef WITH_DUP_CHECK
              new_fra[i].dup_check_flag         = old_fra[i].dup_check_flag;
              new_fra[i].dup_check_timeout      = old_fra[i].dup_check_timeout;
#endif
              new_fra[i].warn_time              = DEFAULT_DIR_WARN_TIME;
              new_fra[i].start_event_handle     = 0L;
              new_fra[i].end_event_handle       = 0L;
              new_fra[i].max_copied_files       = max_copied_files;
              new_fra[i].max_copied_file_size   = max_copied_file_size;
              if (old_fra[i].wait_for_filename[0] == '\0')
              {
                 new_fra[i].wait_for_filename[0] = '\0';
              }
              else
              {
                 (void)strcpy(new_fra[i].wait_for_filename,
                              old_fra[i].wait_for_filename);
              }
              new_fra[i].accumulate             = old_fra[i].accumulate;
              new_fra[i].accumulate_size        = old_fra[i].accumulate_size;
              new_fra[i].locked_file_time       = old_fra[i].locked_file_time;
              (void)memset(&new_fra[i].ate, 0, sizeof(struct bd_time_entry_5));
              new_fra[i].max_errors             = 0;
              new_fra[i].error_counter          = 0;
           }
           ptr = (char *)old_fra;
           ptr -= AFD_WORD_OFFSET_2;

           /*
            * Resize the old FRA to the size of new one and then copy
            * the new structure into it. Then update the FRA version
            * number.
            */
           if ((ptr = mmap_resize(old_fra_fd, ptr, new_size + AFD_WORD_OFFSET_5)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fra_stat, strerror(errno));
              free((void *)new_fra);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_5;
           (void)memcpy(ptr, new_fra, new_size);
           free((void *)new_fra);
           ptr -= AFD_WORD_OFFSET_5;
           *(ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
           *old_fra_size = new_size + AFD_WORD_OFFSET_5;

           system_log(INFO_SIGN, NULL, 0, "Converted FRA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 3) && (new_version == 5))
        {
           struct fileretrieve_status_3 *old_fra;
           struct fileretrieve_status_5 *new_fra;

           /* Get the size of the old FRA file. */
           if (fstat(old_fra_fd, &stat_buf) < 0)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to fstat() %s : %s",
                         old_fra_stat, strerror(errno));
              *old_fra_size = -1;
              return(NULL);
           }
           else
           {
              if (stat_buf.st_size > 0)
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL, stat_buf.st_size,
                                 (PROT_READ | PROT_WRITE),
                                 MAP_SHARED, old_fra_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL, stat_buf.st_size,
                                     (PROT_READ | PROT_WRITE), MAP_SHARED,
                                     old_fra_stat, 0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fra_stat, strerror(errno));
                    *old_fra_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FRA file %s is empty.", old_fra_stat);
                 *old_fra_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_3;
           old_fra = (struct fileretrieve_status_3 *)ptr;

           new_size = old_no_of_dirs * sizeof(struct fileretrieve_status_5);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_dirs, new_size, strerror(errno));
              ptr = (char *)old_fra;
              ptr -= AFD_WORD_OFFSET_3;
#ifdef HAVE_MMAP
              if (munmap(ptr, stat_buf.st_size) == -1)
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fra_stat, strerror(errno));
              }
              *old_fra_size = -1;
              return(NULL);
           }
           new_fra = (struct fileretrieve_status_5 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_dirs; i++)
           {
              (void)strcpy(new_fra[i].dir_alias, old_fra[i].dir_alias);
              (void)strcpy(new_fra[i].host_alias, old_fra[i].host_alias);
              (void)strcpy(new_fra[i].url, old_fra[i].url);
              (void)memset(&new_fra[i].te[0], 0,
                           (MAX_FRA_TIME_ENTRIES_5 * sizeof(struct bd_time_entry_5)));
              new_fra[i].dir_status             = old_fra[i].dir_status;
              new_fra[i].remove                 = old_fra[i].remove;
              new_fra[i].stupid_mode            = old_fra[i].stupid_mode;
              new_fra[i].delete_files_flag      = old_fra[i].delete_files_flag;
              new_fra[i].report_unknown_files   = old_fra[i].report_unknown_files;
              new_fra[i].important_dir          = old_fra[i].important_dir;
              new_fra[i].no_of_time_entries     = 0;
              new_fra[i].force_reread           = old_fra[i].force_reread;
              new_fra[i].queued                 = old_fra[i].queued;
              new_fra[i].priority               = old_fra[i].priority;
              new_fra[i].protocol               = old_fra[i].protocol;
              new_fra[i].bytes_received         = old_fra[i].bytes_received;
              new_fra[i].files_received         = old_fra[i].files_received;
              new_fra[i].last_retrieval         = old_fra[i].last_retrieval;
              new_fra[i].next_check_time        = old_fra[i].next_check_time;
              new_fra[i].unknown_file_time      = old_fra[i].unknown_file_time;
              new_fra[i].queued_file_time       = old_fra[i].queued_file_time;
              new_fra[i].end_character          = old_fra[i].end_character;
              new_fra[i].dir_id                 = old_fra[i].dir_id;
              new_fra[i].fsa_pos                = old_fra[i].fsa_pos;
              new_fra[i].no_of_process          = old_fra[i].no_of_process;
              new_fra[i].max_process            = old_fra[i].max_process;
              new_fra[i].dir_flag               = old_fra[i].dir_flag;
              if ((old_fra[i].dir_status == DISABLED) &&
                  ((old_fra[i].dir_flag & DIR_DISABLED) == 0))
              {
                 new_fra[i].dir_flag ^= DIR_DISABLED;
              }
              new_fra[i].in_dc_flag             = old_fra[i].in_dc_flag;
              new_fra[i].files_in_dir           = old_fra[i].files_in_dir;
              new_fra[i].files_queued           = old_fra[i].files_queued;
              new_fra[i].bytes_in_dir           = old_fra[i].bytes_in_dir;
              new_fra[i].bytes_in_queue         = old_fra[i].bytes_in_queue;
              new_fra[i].ignore_file_time       = old_fra[i].ignore_file_time;
              new_fra[i].ignore_size            = old_fra[i].ignore_size;
              new_fra[i].gt_lt_sign             = old_fra[i].gt_lt_sign;
              new_fra[i].keep_connected         = DEFAULT_KEEP_CONNECTED_TIME;
#ifdef WITH_DUP_CHECK
              new_fra[i].dup_check_flag         = old_fra[i].dup_check_flag;
              new_fra[i].dup_check_timeout      = old_fra[i].dup_check_timeout;
#endif
              new_fra[i].warn_time              = DEFAULT_DIR_WARN_TIME;
              new_fra[i].start_event_handle     = 0L;
              new_fra[i].end_event_handle       = 0L;
              new_fra[i].max_copied_files       = max_copied_files;
              new_fra[i].max_copied_file_size   = max_copied_file_size;
              if (old_fra[i].wait_for_filename[0] == '\0')
              {
                 new_fra[i].wait_for_filename[0] = '\0';
              }
              else
              {
                 (void)strcpy(new_fra[i].wait_for_filename,
                              old_fra[i].wait_for_filename);
              }
              new_fra[i].accumulate             = old_fra[i].accumulate;
              new_fra[i].accumulate_size        = old_fra[i].accumulate_size;
              new_fra[i].locked_file_time       = old_fra[i].locked_file_time;
              (void)memset(&new_fra[i].ate, 0, sizeof(struct bd_time_entry_5));
              new_fra[i].max_errors             = 0;
              new_fra[i].error_counter          = 0;
           }
           ptr = (char *)old_fra;
           ptr -= AFD_WORD_OFFSET_3;

           /*
            * Resize the old FRA to the size of new one and then copy
            * the new structure into it. Then update the FRA version
            * number.
            */
           if ((ptr = mmap_resize(old_fra_fd, ptr, new_size + AFD_WORD_OFFSET_5)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fra_stat, strerror(errno));
              free((void *)new_fra);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_5;
           (void)memcpy(ptr, new_fra, new_size);
           free((void *)new_fra);
           ptr -= AFD_WORD_OFFSET_5;
           *(ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
           *old_fra_size = new_size + AFD_WORD_OFFSET_5;

           system_log(INFO_SIGN, NULL, 0, "Converted FRA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 4) && (new_version == 5))
        {
           struct fileretrieve_status_4 *old_fra;
           struct fileretrieve_status_5 *new_fra;

           /* Get the size of the old FRA file. */
           if (fstat(old_fra_fd, &stat_buf) < 0)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to fstat() %s : %s",
                         old_fra_stat, strerror(errno));
              *old_fra_size = -1;
              return(NULL);
           }
           else
           {
              if (stat_buf.st_size > 0)
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL, stat_buf.st_size,
                                 (PROT_READ | PROT_WRITE),
                                 MAP_SHARED, old_fra_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL, stat_buf.st_size,
                                     (PROT_READ | PROT_WRITE), MAP_SHARED,
                                     old_fra_stat, 0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fra_stat, strerror(errno));
                    *old_fra_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FRA file %s is empty.", old_fra_stat);
                 *old_fra_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_4;
           old_fra = (struct fileretrieve_status_4 *)ptr;

           new_size = old_no_of_dirs * sizeof(struct fileretrieve_status_5);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_dirs, new_size, strerror(errno));
              ptr = (char *)old_fra;
              ptr -= AFD_WORD_OFFSET_4;
#ifdef HAVE_MMAP
              if (munmap(ptr, stat_buf.st_size) == -1)
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fra_stat, strerror(errno));
              }
              *old_fra_size = -1;
              return(NULL);
           }
           new_fra = (struct fileretrieve_status_5 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_dirs; i++)
           {
              (void)strcpy(new_fra[i].dir_alias, old_fra[i].dir_alias);
              (void)strcpy(new_fra[i].host_alias, old_fra[i].host_alias);
              (void)strcpy(new_fra[i].url, old_fra[i].url);
              (void)memset(&new_fra[i].te[0], 0,
                           (MAX_FRA_TIME_ENTRIES_5 * sizeof(struct bd_time_entry_5)));
              new_fra[i].dir_status             = old_fra[i].dir_status;
              new_fra[i].remove                 = old_fra[i].remove;
              new_fra[i].stupid_mode            = old_fra[i].stupid_mode;
              new_fra[i].delete_files_flag      = old_fra[i].delete_files_flag;
              new_fra[i].report_unknown_files   = old_fra[i].report_unknown_files;
              new_fra[i].important_dir          = old_fra[i].important_dir;
              new_fra[i].no_of_time_entries     = 0;
              new_fra[i].force_reread           = old_fra[i].force_reread;
              new_fra[i].queued                 = old_fra[i].queued;
              new_fra[i].priority               = old_fra[i].priority;
              new_fra[i].protocol               = old_fra[i].protocol;
              new_fra[i].bytes_received         = old_fra[i].bytes_received;
              new_fra[i].files_received         = old_fra[i].files_received;
              new_fra[i].last_retrieval         = old_fra[i].last_retrieval;
              new_fra[i].next_check_time        = old_fra[i].next_check_time;
              new_fra[i].unknown_file_time      = old_fra[i].unknown_file_time;
              new_fra[i].queued_file_time       = old_fra[i].queued_file_time;
              new_fra[i].end_character          = old_fra[i].end_character;
              new_fra[i].dir_id                 = old_fra[i].dir_id;
              new_fra[i].fsa_pos                = old_fra[i].fsa_pos;
              new_fra[i].no_of_process          = old_fra[i].no_of_process;
              new_fra[i].max_process            = old_fra[i].max_process;
              new_fra[i].dir_flag               = old_fra[i].dir_flag;
              new_fra[i].in_dc_flag             = old_fra[i].in_dc_flag;
              new_fra[i].files_in_dir           = old_fra[i].files_in_dir;
              new_fra[i].files_queued           = old_fra[i].files_queued;
              new_fra[i].bytes_in_dir           = old_fra[i].bytes_in_dir;
              new_fra[i].bytes_in_queue         = old_fra[i].bytes_in_queue;
              new_fra[i].ignore_file_time       = old_fra[i].ignore_file_time;
              new_fra[i].ignore_size            = old_fra[i].ignore_size;
              new_fra[i].gt_lt_sign             = old_fra[i].gt_lt_sign;
              new_fra[i].keep_connected         = old_fra[i].keep_connected;
#ifdef WITH_DUP_CHECK
              new_fra[i].dup_check_flag         = old_fra[i].dup_check_flag;
              new_fra[i].dup_check_timeout      = old_fra[i].dup_check_timeout;
#endif
              new_fra[i].warn_time              = old_fra[i].warn_time;
              new_fra[i].start_event_handle     = 0L;
              new_fra[i].end_event_handle       = 0L;
              new_fra[i].max_copied_files       = max_copied_files;
              new_fra[i].max_copied_file_size   = max_copied_file_size;
              if (old_fra[i].wait_for_filename[0] == '\0')
              {
                 new_fra[i].wait_for_filename[0] = '\0';
              }
              else
              {
                 (void)strcpy(new_fra[i].wait_for_filename,
                              old_fra[i].wait_for_filename);
              }
              new_fra[i].accumulate             = old_fra[i].accumulate;
              new_fra[i].accumulate_size        = old_fra[i].accumulate_size;
              new_fra[i].locked_file_time       = old_fra[i].locked_file_time;
              (void)memset(&new_fra[i].ate, 0, sizeof(struct bd_time_entry_5));
              new_fra[i].max_errors             = old_fra[i].max_errors;
              new_fra[i].error_counter          = old_fra[i].error_counter;
           }
           ptr = (char *)old_fra;
           ptr -= AFD_WORD_OFFSET_4;

           /*
            * Resize the old FRA to the size of new one and then copy
            * the new structure into it. Then update the FRA version
            * number.
            */
           if ((ptr = mmap_resize(old_fra_fd, ptr, new_size + AFD_WORD_OFFSET_5)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fra_stat, strerror(errno));
              free((void *)new_fra);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_5;
           (void)memcpy(ptr, new_fra, new_size);
           free((void *)new_fra);
           ptr -= AFD_WORD_OFFSET_5;
           *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
           *old_fra_size = new_size + AFD_WORD_OFFSET_5;

           system_log(INFO_SIGN, NULL, 0, "Converted FRA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 0) && (new_version == 6))
        {
           struct fileretrieve_status_0 *old_fra;
           struct fileretrieve_status_6 *new_fra;

           /* Get the size of the old FRA file. */
           if (fstat(old_fra_fd, &stat_buf) < 0)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to fstat() %s : %s",
                         old_fra_stat, strerror(errno));
              *old_fra_size = -1;
              return(NULL);
           }
           else
           {
              if (stat_buf.st_size > 0)
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL, stat_buf.st_size,
                                 (PROT_READ | PROT_WRITE),
                                 MAP_SHARED, old_fra_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL, stat_buf.st_size,
                                     (PROT_READ | PROT_WRITE), MAP_SHARED,
                                     old_fra_stat, 0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fra_stat, strerror(errno));
                    *old_fra_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FRA file %s is empty.", old_fra_stat);
                 *old_fra_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_0;
           old_fra = (struct fileretrieve_status_0 *)ptr;

           new_size = old_no_of_dirs * sizeof(struct fileretrieve_status_6);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_dirs, new_size, strerror(errno));
              ptr = (char *)old_fra;
              ptr -= AFD_WORD_OFFSET_0;
#ifdef HAVE_MMAP
              if (munmap(ptr, stat_buf.st_size) == -1)
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fra_stat, strerror(errno));
              }
              *old_fra_size = -1;
              return(NULL);
           }
           new_fra = (struct fileretrieve_status_6 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_dirs; i++)
           {
              (void)strcpy(new_fra[i].dir_alias, old_fra[i].dir_alias);
              (void)strcpy(new_fra[i].host_alias, old_fra[i].host_alias);
              (void)strcpy(new_fra[i].url, old_fra[i].url);
              new_fra[i].retrieve_work_dir[0] = '\0';
              new_fra[i].timezone[0] = '\0';
              (void)memset(&new_fra[i].te[0], 0,
                           (MAX_FRA_TIME_ENTRIES_6 * sizeof(struct bd_time_entry_6)));
              new_fra[i].dir_status             = old_fra[i].dir_status;
              new_fra[i].remove                 = old_fra[i].remove;
              new_fra[i].stupid_mode            = old_fra[i].stupid_mode;
              new_fra[i].protocol               = old_fra[i].protocol;
              new_fra[i].delete_files_flag      = old_fra[i].delete_files_flag;
              new_fra[i].report_unknown_files   = old_fra[i].report_unknown_files;
              new_fra[i].important_dir          = old_fra[i].important_dir;
              new_fra[i].no_of_time_entries     = 0;
              new_fra[i].force_reread           = old_fra[i].force_reread;
              new_fra[i].queued                 = old_fra[i].queued;
              new_fra[i].priority               = old_fra[i].priority;
              new_fra[i].dir_mode               = 0;
              new_fra[i].bytes_received         = old_fra[i].bytes_received;
              new_fra[i].files_received         = old_fra[i].files_received;
              new_fra[i].last_retrieval         = old_fra[i].last_retrieval;
              new_fra[i].next_check_time        = old_fra[i].next_check_time;
              new_fra[i].unknown_file_time      = old_fra[i].old_file_time;
              new_fra[i].queued_file_time       = old_fra[i].old_file_time;
              new_fra[i].unreadable_file_time   = -1;
              new_fra[i].end_character          = old_fra[i].end_character;
              new_fra[i].dir_id                 = 0;
              new_fra[i].fsa_pos                = old_fra[i].fsa_pos;
              new_fra[i].no_of_process          = old_fra[i].no_of_process;
              new_fra[i].max_process            = old_fra[i].max_process;
              new_fra[i].dir_flag               = 0;
              if (old_fra[i].dir_status == DISABLED)
              {
                 new_fra[i].dir_flag ^= DIR_DISABLED;
              }
              new_fra[i].in_dc_flag             = 0;
              new_fra[i].files_in_dir           = 0;
              new_fra[i].files_queued           = 0;
              new_fra[i].bytes_in_dir           = 0;
              new_fra[i].bytes_in_queue         = 0;
              new_fra[i].ignore_file_time       = 0;
              new_fra[i].ignore_size            = -1;
              new_fra[i].gt_lt_sign             = 0;
              new_fra[i].keep_connected         = DEFAULT_KEEP_CONNECTED_TIME;
#ifdef WITH_DUP_CHECK
              new_fra[i].dup_check_flag         = 0;
              new_fra[i].dup_check_timeout      = 0L;
#endif
              new_fra[i].warn_time              = DEFAULT_DIR_WARN_TIME;
              new_fra[i].info_time              = DEFAULT_DIR_INFO_TIME;
              new_fra[i].start_event_handle     = 0L;
              new_fra[i].end_event_handle       = 0L;
              new_fra[i].max_copied_files       = max_copied_files;
              new_fra[i].max_copied_file_size   = max_copied_file_size;
              new_fra[i].wait_for_filename[0]   = '\0';
              new_fra[i].accumulate             = 0;
              new_fra[i].accumulate_size        = 0;
              new_fra[i].locked_file_time       = -1;
              (void)memset(&new_fra[i].ate, 0, sizeof(struct bd_time_entry_6));
              new_fra[i].max_errors             = 0;
              new_fra[i].error_counter          = 0;
           }
           ptr = (char *)old_fra;
           ptr -= AFD_WORD_OFFSET_0;

           /*
            * Resize the old FRA to the size of new one and then copy
            * the new structure into it. Then update the FRA version
            * number.
            */
           if ((ptr = mmap_resize(old_fra_fd, ptr, new_size + AFD_WORD_OFFSET_6)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fra_stat, strerror(errno));
              free((void *)new_fra);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_6;
           (void)memcpy(ptr, new_fra, new_size);
           free((void *)new_fra);
           ptr -= AFD_WORD_OFFSET_6;
           *(ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
           *old_fra_size = new_size + AFD_WORD_OFFSET_6;

           system_log(DEBUG_SIGN, NULL, 0,
                      "Converted FRA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 1) && (new_version == 6))
        {
           struct fileretrieve_status_1 *old_fra;
           struct fileretrieve_status_6 *new_fra;

           /* Get the size of the old FRA file. */
           if (fstat(old_fra_fd, &stat_buf) < 0)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to fstat() %s : %s",
                         old_fra_stat, strerror(errno));
              *old_fra_size = -1;
              return(NULL);
           }
           else
           {
              if (stat_buf.st_size > 0)
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL, stat_buf.st_size,
                                 (PROT_READ | PROT_WRITE),
                                 MAP_SHARED, old_fra_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL, stat_buf.st_size,
                                     (PROT_READ | PROT_WRITE), MAP_SHARED,
                                     old_fra_stat, 0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fra_stat, strerror(errno));
                    *old_fra_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FRA file %s is empty.", old_fra_stat);
                 *old_fra_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_1;
           old_fra = (struct fileretrieve_status_1 *)ptr;

           new_size = old_no_of_dirs * sizeof(struct fileretrieve_status_6);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_dirs, new_size, strerror(errno));
              ptr = (char *)old_fra;
              ptr -= AFD_WORD_OFFSET_1;
#ifdef HAVE_MMAP
              if (munmap(ptr, stat_buf.st_size) == -1)
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fra_stat, strerror(errno));
              }
              *old_fra_size = -1;
              return(NULL);
           }
           new_fra = (struct fileretrieve_status_6 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_dirs; i++)
           {
              (void)strcpy(new_fra[i].dir_alias, old_fra[i].dir_alias);
              (void)strcpy(new_fra[i].host_alias, old_fra[i].host_alias);
              (void)strcpy(new_fra[i].url, old_fra[i].url);
              new_fra[i].retrieve_work_dir[0] = '\0';
              new_fra[i].timezone[0] = '\0';
              (void)memset(&new_fra[i].te[0], 0,
                           (MAX_FRA_TIME_ENTRIES_6 * sizeof(struct bd_time_entry_6)));
              new_fra[i].dir_status             = old_fra[i].dir_status;
              new_fra[i].remove                 = old_fra[i].remove;
              new_fra[i].stupid_mode            = old_fra[i].stupid_mode;
              new_fra[i].protocol               = old_fra[i].protocol;
              new_fra[i].delete_files_flag      = old_fra[i].delete_files_flag;
              new_fra[i].report_unknown_files   = old_fra[i].report_unknown_files;
              new_fra[i].important_dir          = old_fra[i].important_dir;
              new_fra[i].no_of_time_entries     = 0;
              new_fra[i].force_reread           = old_fra[i].force_reread;
              new_fra[i].queued                 = old_fra[i].queued;
              new_fra[i].priority               = old_fra[i].priority;
              new_fra[i].dir_mode               = 0;
              new_fra[i].bytes_received         = old_fra[i].bytes_received;
              new_fra[i].files_received         = old_fra[i].files_received;
              new_fra[i].last_retrieval         = old_fra[i].last_retrieval;
              new_fra[i].next_check_time        = old_fra[i].next_check_time;
              new_fra[i].unknown_file_time      = old_fra[i].unknown_file_time;
              new_fra[i].queued_file_time       = old_fra[i].queued_file_time;
              new_fra[i].unreadable_file_time   = -1;
              new_fra[i].end_character          = old_fra[i].end_character;
              new_fra[i].dir_id                 = 0;
              new_fra[i].fsa_pos                = old_fra[i].fsa_pos;
              new_fra[i].no_of_process          = old_fra[i].no_of_process;
              new_fra[i].max_process            = old_fra[i].max_process;
              new_fra[i].dir_flag               = old_fra[i].dir_flag;
              if ((old_fra[i].dir_status == DISABLED) &&
                  ((old_fra[i].dir_flag & DIR_DISABLED) == 0))
              {
                 new_fra[i].dir_flag ^= DIR_DISABLED;
              }
              new_fra[i].files_in_dir           = old_fra[i].files_in_dir;
              new_fra[i].files_queued           = old_fra[i].files_queued;
              new_fra[i].bytes_in_dir           = old_fra[i].bytes_in_dir;
              new_fra[i].bytes_in_queue         = old_fra[i].bytes_in_queue;
              new_fra[i].in_dc_flag             = 0;
              new_fra[i].ignore_file_time       = 0;
              new_fra[i].ignore_size            = -1;
              new_fra[i].gt_lt_sign             = 0;
              new_fra[i].keep_connected         = DEFAULT_KEEP_CONNECTED_TIME;
#ifdef WITH_DUP_CHECK
              new_fra[i].dup_check_flag         = 0;
              new_fra[i].dup_check_timeout      = 0L;
#endif
              new_fra[i].warn_time              = DEFAULT_DIR_WARN_TIME;
              new_fra[i].info_time              = DEFAULT_DIR_INFO_TIME;
              new_fra[i].start_event_handle     = 0L;
              new_fra[i].end_event_handle       = 0L;
              new_fra[i].max_copied_files       = max_copied_files;
              new_fra[i].max_copied_file_size   = max_copied_file_size;
              new_fra[i].wait_for_filename[0]   = '\0';
              new_fra[i].accumulate             = 0;
              new_fra[i].accumulate_size        = 0;
              new_fra[i].locked_file_time       = -1;
              (void)memset(&new_fra[i].ate, 0, sizeof(struct bd_time_entry_6));
              new_fra[i].max_errors             = 0;
              new_fra[i].error_counter          = 0;
           }
           ptr = (char *)old_fra;
           ptr -= AFD_WORD_OFFSET_1;

           /*
            * Resize the old FRA to the size of new one and then copy
            * the new structure into it. Then update the FRA version
            * number.
            */
           if ((ptr = mmap_resize(old_fra_fd, ptr, new_size + AFD_WORD_OFFSET_6)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fra_stat, strerror(errno));
              free((void *)new_fra);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_6;
           (void)memcpy(ptr, new_fra, new_size);
           free((void *)new_fra);
           ptr -= AFD_WORD_OFFSET_6;
           *(ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
           *old_fra_size = new_size + AFD_WORD_OFFSET_6;

           system_log(INFO_SIGN, NULL, 0, "Converted FRA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 2) && (new_version == 6))
        {
           struct fileretrieve_status_2 *old_fra;
           struct fileretrieve_status_6 *new_fra;

           /* Get the size of the old FRA file. */
           if (fstat(old_fra_fd, &stat_buf) < 0)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to fstat() %s : %s",
                         old_fra_stat, strerror(errno));
              *old_fra_size = -1;
              return(NULL);
           }
           else
           {
              if (stat_buf.st_size > 0)
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL, stat_buf.st_size,
                                 (PROT_READ | PROT_WRITE),
                                 MAP_SHARED, old_fra_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL, stat_buf.st_size,
                                     (PROT_READ | PROT_WRITE), MAP_SHARED,
                                     old_fra_stat, 0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fra_stat, strerror(errno));
                    *old_fra_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FRA file %s is empty.", old_fra_stat);
                 *old_fra_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_2;
           old_fra = (struct fileretrieve_status_2 *)ptr;

           new_size = old_no_of_dirs * sizeof(struct fileretrieve_status_6);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_dirs, new_size, strerror(errno));
              ptr = (char *)old_fra;
              ptr -= AFD_WORD_OFFSET_2;
#ifdef HAVE_MMAP
              if (munmap(ptr, stat_buf.st_size) == -1)
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fra_stat, strerror(errno));
              }
              *old_fra_size = -1;
              return(NULL);
           }
           new_fra = (struct fileretrieve_status_6 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_dirs; i++)
           {
              (void)strcpy(new_fra[i].dir_alias, old_fra[i].dir_alias);
              (void)strcpy(new_fra[i].host_alias, old_fra[i].host_alias);
              (void)strcpy(new_fra[i].url, old_fra[i].url);
              new_fra[i].retrieve_work_dir[0] = '\0';
              new_fra[i].timezone[0] = '\0';
              (void)memset(&new_fra[i].te[0], 0,
                           (MAX_FRA_TIME_ENTRIES_6 * sizeof(struct bd_time_entry_6)));
              new_fra[i].dir_status             = old_fra[i].dir_status;
              new_fra[i].remove                 = old_fra[i].remove;
              new_fra[i].stupid_mode            = old_fra[i].stupid_mode;
              new_fra[i].protocol               = old_fra[i].protocol;
              new_fra[i].delete_files_flag      = old_fra[i].delete_files_flag;
              new_fra[i].report_unknown_files   = old_fra[i].report_unknown_files;
              new_fra[i].important_dir          = old_fra[i].important_dir;
              new_fra[i].no_of_time_entries     = 0;
              new_fra[i].force_reread           = old_fra[i].force_reread;
              new_fra[i].queued                 = old_fra[i].queued;
              new_fra[i].priority               = old_fra[i].priority;
              new_fra[i].dir_mode               = 0;
              new_fra[i].bytes_received         = old_fra[i].bytes_received;
              new_fra[i].files_received         = old_fra[i].files_received;
              new_fra[i].last_retrieval         = old_fra[i].last_retrieval;
              new_fra[i].next_check_time        = old_fra[i].next_check_time;
              new_fra[i].unknown_file_time      = old_fra[i].unknown_file_time;
              new_fra[i].queued_file_time       = old_fra[i].queued_file_time;
              new_fra[i].unreadable_file_time   = -1;
              new_fra[i].end_character          = old_fra[i].end_character;
              new_fra[i].dir_id                 = 0;
              new_fra[i].fsa_pos                = old_fra[i].fsa_pos;
              new_fra[i].no_of_process          = old_fra[i].no_of_process;
              new_fra[i].max_process            = old_fra[i].max_process;
              new_fra[i].dir_flag               = old_fra[i].dir_flag;
              if ((old_fra[i].dir_status == DISABLED) &&
                  ((old_fra[i].dir_flag & DIR_DISABLED) == 0))
              {
                 new_fra[i].dir_flag ^= DIR_DISABLED;
              }
              new_fra[i].in_dc_flag             = 0;
              new_fra[i].files_in_dir           = old_fra[i].files_in_dir;
              new_fra[i].files_queued           = old_fra[i].files_queued;
              new_fra[i].bytes_in_dir           = old_fra[i].bytes_in_dir;
              new_fra[i].bytes_in_queue         = old_fra[i].bytes_in_queue;
              new_fra[i].ignore_file_time       = old_fra[i].ignore_file_time;
              new_fra[i].ignore_size            = old_fra[i].ignore_size;
              new_fra[i].gt_lt_sign             = old_fra[i].gt_lt_sign;
              new_fra[i].keep_connected         = DEFAULT_KEEP_CONNECTED_TIME;
#ifdef WITH_DUP_CHECK
              new_fra[i].dup_check_flag         = old_fra[i].dup_check_flag;
              new_fra[i].dup_check_timeout      = old_fra[i].dup_check_timeout;
#endif
              new_fra[i].warn_time              = DEFAULT_DIR_WARN_TIME;
              new_fra[i].info_time              = DEFAULT_DIR_INFO_TIME;
              new_fra[i].start_event_handle     = 0L;
              new_fra[i].end_event_handle       = 0L;
              new_fra[i].max_copied_files       = max_copied_files;
              new_fra[i].max_copied_file_size   = max_copied_file_size;
              if (old_fra[i].wait_for_filename[0] == '\0')
              {
                 new_fra[i].wait_for_filename[0] = '\0';
              }
              else
              {
                 (void)strcpy(new_fra[i].wait_for_filename,
                              old_fra[i].wait_for_filename);
              }
              new_fra[i].accumulate             = old_fra[i].accumulate;
              new_fra[i].accumulate_size        = old_fra[i].accumulate_size;
              new_fra[i].locked_file_time       = old_fra[i].locked_file_time;
              (void)memset(&new_fra[i].ate, 0, sizeof(struct bd_time_entry_6));
              new_fra[i].max_errors             = 0;
              new_fra[i].error_counter          = 0;
           }
           ptr = (char *)old_fra;
           ptr -= AFD_WORD_OFFSET_2;

           /*
            * Resize the old FRA to the size of new one and then copy
            * the new structure into it. Then update the FRA version
            * number.
            */
           if ((ptr = mmap_resize(old_fra_fd, ptr, new_size + AFD_WORD_OFFSET_6)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fra_stat, strerror(errno));
              free((void *)new_fra);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_6;
           (void)memcpy(ptr, new_fra, new_size);
           free((void *)new_fra);
           ptr -= AFD_WORD_OFFSET_6;
           *(ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
           *old_fra_size = new_size + AFD_WORD_OFFSET_6;

           system_log(INFO_SIGN, NULL, 0, "Converted FRA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 3) && (new_version == 6))
        {
           struct fileretrieve_status_3 *old_fra;
           struct fileretrieve_status_6 *new_fra;

           /* Get the size of the old FRA file. */
           if (fstat(old_fra_fd, &stat_buf) < 0)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to fstat() %s : %s",
                         old_fra_stat, strerror(errno));
              *old_fra_size = -1;
              return(NULL);
           }
           else
           {
              if (stat_buf.st_size > 0)
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL, stat_buf.st_size,
                                 (PROT_READ | PROT_WRITE),
                                 MAP_SHARED, old_fra_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL, stat_buf.st_size,
                                     (PROT_READ | PROT_WRITE), MAP_SHARED,
                                     old_fra_stat, 0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fra_stat, strerror(errno));
                    *old_fra_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FRA file %s is empty.", old_fra_stat);
                 *old_fra_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_3;
           old_fra = (struct fileretrieve_status_3 *)ptr;

           new_size = old_no_of_dirs * sizeof(struct fileretrieve_status_6);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_dirs, new_size, strerror(errno));
              ptr = (char *)old_fra;
              ptr -= AFD_WORD_OFFSET_3;
#ifdef HAVE_MMAP
              if (munmap(ptr, stat_buf.st_size) == -1)
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fra_stat, strerror(errno));
              }
              *old_fra_size = -1;
              return(NULL);
           }
           new_fra = (struct fileretrieve_status_6 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_dirs; i++)
           {
              (void)strcpy(new_fra[i].dir_alias, old_fra[i].dir_alias);
              (void)strcpy(new_fra[i].host_alias, old_fra[i].host_alias);
              (void)strcpy(new_fra[i].url, old_fra[i].url);
              new_fra[i].retrieve_work_dir[0] = '\0';
              new_fra[i].timezone[0] = '\0';
              (void)memset(&new_fra[i].te[0], 0,
                           (MAX_FRA_TIME_ENTRIES_6 * sizeof(struct bd_time_entry_6)));
              new_fra[i].dir_status             = old_fra[i].dir_status;
              new_fra[i].remove                 = old_fra[i].remove;
              new_fra[i].stupid_mode            = old_fra[i].stupid_mode;
              new_fra[i].delete_files_flag      = old_fra[i].delete_files_flag;
              new_fra[i].report_unknown_files   = old_fra[i].report_unknown_files;
              new_fra[i].important_dir          = old_fra[i].important_dir;
              new_fra[i].no_of_time_entries     = 0;
              new_fra[i].force_reread           = old_fra[i].force_reread;
              new_fra[i].queued                 = old_fra[i].queued;
              new_fra[i].priority               = old_fra[i].priority;
              new_fra[i].protocol               = old_fra[i].protocol;
              new_fra[i].dir_mode               = 0;
              new_fra[i].bytes_received         = old_fra[i].bytes_received;
              new_fra[i].files_received         = old_fra[i].files_received;
              new_fra[i].last_retrieval         = old_fra[i].last_retrieval;
              new_fra[i].next_check_time        = old_fra[i].next_check_time;
              new_fra[i].unknown_file_time      = old_fra[i].unknown_file_time;
              new_fra[i].queued_file_time       = old_fra[i].queued_file_time;
              new_fra[i].unreadable_file_time   = -1;
              new_fra[i].end_character          = old_fra[i].end_character;
              new_fra[i].dir_id                 = old_fra[i].dir_id;
              new_fra[i].fsa_pos                = old_fra[i].fsa_pos;
              new_fra[i].no_of_process          = old_fra[i].no_of_process;
              new_fra[i].max_process            = old_fra[i].max_process;
              new_fra[i].dir_flag               = old_fra[i].dir_flag;
              if ((old_fra[i].dir_status == DISABLED) &&
                  ((old_fra[i].dir_flag & DIR_DISABLED) == 0))
              {
                 new_fra[i].dir_flag ^= DIR_DISABLED;
              }
              new_fra[i].in_dc_flag             = old_fra[i].in_dc_flag;
              new_fra[i].files_in_dir           = old_fra[i].files_in_dir;
              new_fra[i].files_queued           = old_fra[i].files_queued;
              new_fra[i].bytes_in_dir           = old_fra[i].bytes_in_dir;
              new_fra[i].bytes_in_queue         = old_fra[i].bytes_in_queue;
              new_fra[i].ignore_file_time       = old_fra[i].ignore_file_time;
              new_fra[i].ignore_size            = old_fra[i].ignore_size;
              new_fra[i].gt_lt_sign             = old_fra[i].gt_lt_sign;
              new_fra[i].keep_connected         = DEFAULT_KEEP_CONNECTED_TIME;
#ifdef WITH_DUP_CHECK
              new_fra[i].dup_check_flag         = old_fra[i].dup_check_flag;
              new_fra[i].dup_check_timeout      = old_fra[i].dup_check_timeout;
#endif
              new_fra[i].warn_time              = DEFAULT_DIR_WARN_TIME;
              new_fra[i].info_time              = DEFAULT_DIR_INFO_TIME;
              new_fra[i].start_event_handle     = 0L;
              new_fra[i].end_event_handle       = 0L;
              new_fra[i].max_copied_files       = max_copied_files;
              new_fra[i].max_copied_file_size   = max_copied_file_size;
              if (old_fra[i].wait_for_filename[0] == '\0')
              {
                 new_fra[i].wait_for_filename[0] = '\0';
              }
              else
              {
                 (void)strcpy(new_fra[i].wait_for_filename,
                              old_fra[i].wait_for_filename);
              }
              new_fra[i].accumulate             = old_fra[i].accumulate;
              new_fra[i].accumulate_size        = old_fra[i].accumulate_size;
              new_fra[i].locked_file_time       = old_fra[i].locked_file_time;
              (void)memset(&new_fra[i].ate, 0, sizeof(struct bd_time_entry_6));
              new_fra[i].max_errors             = 0;
              new_fra[i].error_counter          = 0;
           }
           ptr = (char *)old_fra;
           ptr -= AFD_WORD_OFFSET_3;

           /*
            * Resize the old FRA to the size of new one and then copy
            * the new structure into it. Then update the FRA version
            * number.
            */
           if ((ptr = mmap_resize(old_fra_fd, ptr, new_size + AFD_WORD_OFFSET_6)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fra_stat, strerror(errno));
              free((void *)new_fra);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_6;
           (void)memcpy(ptr, new_fra, new_size);
           free((void *)new_fra);
           ptr -= AFD_WORD_OFFSET_6;
           *(ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
           *old_fra_size = new_size + AFD_WORD_OFFSET_6;

           system_log(INFO_SIGN, NULL, 0, "Converted FRA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 4) && (new_version == 6))
        {
           struct fileretrieve_status_4 *old_fra;
           struct fileretrieve_status_6 *new_fra;

           /* Get the size of the old FRA file. */
           if (fstat(old_fra_fd, &stat_buf) < 0)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to fstat() %s : %s",
                         old_fra_stat, strerror(errno));
              *old_fra_size = -1;
              return(NULL);
           }
           else
           {
              if (stat_buf.st_size > 0)
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL, stat_buf.st_size,
                                 (PROT_READ | PROT_WRITE),
                                 MAP_SHARED, old_fra_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL, stat_buf.st_size,
                                     (PROT_READ | PROT_WRITE), MAP_SHARED,
                                     old_fra_stat, 0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fra_stat, strerror(errno));
                    *old_fra_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FRA file %s is empty.", old_fra_stat);
                 *old_fra_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_4;
           old_fra = (struct fileretrieve_status_4 *)ptr;

           new_size = old_no_of_dirs * sizeof(struct fileretrieve_status_6);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_dirs, new_size, strerror(errno));
              ptr = (char *)old_fra;
              ptr -= AFD_WORD_OFFSET_4;
#ifdef HAVE_MMAP
              if (munmap(ptr, stat_buf.st_size) == -1)
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fra_stat, strerror(errno));
              }
              *old_fra_size = -1;
              return(NULL);
           }
           new_fra = (struct fileretrieve_status_6 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_dirs; i++)
           {
              (void)strcpy(new_fra[i].dir_alias, old_fra[i].dir_alias);
              (void)strcpy(new_fra[i].host_alias, old_fra[i].host_alias);
              (void)strcpy(new_fra[i].url, old_fra[i].url);
              new_fra[i].retrieve_work_dir[0] = '\0';
              new_fra[i].timezone[0] = '\0';
              (void)memset(&new_fra[i].te[0], 0,
                           (MAX_FRA_TIME_ENTRIES_6 * sizeof(struct bd_time_entry_6)));
              new_fra[i].dir_status             = old_fra[i].dir_status;
              new_fra[i].remove                 = old_fra[i].remove;
              new_fra[i].stupid_mode            = old_fra[i].stupid_mode;
              new_fra[i].delete_files_flag      = old_fra[i].delete_files_flag;
              new_fra[i].report_unknown_files   = old_fra[i].report_unknown_files;
              new_fra[i].important_dir          = old_fra[i].important_dir;
              new_fra[i].no_of_time_entries     = 0;
              new_fra[i].force_reread           = old_fra[i].force_reread;
              new_fra[i].queued                 = old_fra[i].queued;
              new_fra[i].priority               = old_fra[i].priority;
              new_fra[i].protocol               = old_fra[i].protocol;
              new_fra[i].dir_mode               = 0;
              new_fra[i].bytes_received         = old_fra[i].bytes_received;
              new_fra[i].files_received         = old_fra[i].files_received;
              new_fra[i].last_retrieval         = old_fra[i].last_retrieval;
              new_fra[i].next_check_time        = old_fra[i].next_check_time;
              new_fra[i].unknown_file_time      = old_fra[i].unknown_file_time;
              new_fra[i].queued_file_time       = old_fra[i].queued_file_time;
              new_fra[i].unreadable_file_time   = -1;
              new_fra[i].end_character          = old_fra[i].end_character;
              new_fra[i].dir_id                 = old_fra[i].dir_id;
              new_fra[i].fsa_pos                = old_fra[i].fsa_pos;
              new_fra[i].no_of_process          = old_fra[i].no_of_process;
              new_fra[i].max_process            = old_fra[i].max_process;
              new_fra[i].dir_flag               = old_fra[i].dir_flag;
              new_fra[i].in_dc_flag             = old_fra[i].in_dc_flag;
              new_fra[i].files_in_dir           = old_fra[i].files_in_dir;
              new_fra[i].files_queued           = old_fra[i].files_queued;
              new_fra[i].bytes_in_dir           = old_fra[i].bytes_in_dir;
              new_fra[i].bytes_in_queue         = old_fra[i].bytes_in_queue;
              new_fra[i].ignore_file_time       = old_fra[i].ignore_file_time;
              new_fra[i].ignore_size            = old_fra[i].ignore_size;
              new_fra[i].gt_lt_sign             = old_fra[i].gt_lt_sign;
              new_fra[i].keep_connected         = old_fra[i].keep_connected;
#ifdef WITH_DUP_CHECK
              new_fra[i].dup_check_flag         = old_fra[i].dup_check_flag;
              new_fra[i].dup_check_timeout      = old_fra[i].dup_check_timeout;
#endif
              new_fra[i].warn_time              = old_fra[i].warn_time;
              new_fra[i].info_time              = DEFAULT_DIR_INFO_TIME;
              new_fra[i].start_event_handle     = 0L;
              new_fra[i].end_event_handle       = 0L;
              new_fra[i].max_copied_files       = max_copied_files;
              new_fra[i].max_copied_file_size   = max_copied_file_size;
              if (old_fra[i].wait_for_filename[0] == '\0')
              {
                 new_fra[i].wait_for_filename[0] = '\0';
              }
              else
              {
                 (void)strcpy(new_fra[i].wait_for_filename,
                              old_fra[i].wait_for_filename);
              }
              new_fra[i].accumulate             = old_fra[i].accumulate;
              new_fra[i].accumulate_size        = old_fra[i].accumulate_size;
              new_fra[i].locked_file_time       = old_fra[i].locked_file_time;
              (void)memset(&new_fra[i].ate, 0, sizeof(struct bd_time_entry_6));
              new_fra[i].max_errors             = old_fra[i].max_errors;
              new_fra[i].error_counter          = old_fra[i].error_counter;
           }
           ptr = (char *)old_fra;
           ptr -= AFD_WORD_OFFSET_4;

           /*
            * Resize the old FRA to the size of new one and then copy
            * the new structure into it. Then update the FRA version
            * number.
            */
           if ((ptr = mmap_resize(old_fra_fd, ptr, new_size + AFD_WORD_OFFSET_6)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fra_stat, strerror(errno));
              free((void *)new_fra);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_6;
           (void)memcpy(ptr, new_fra, new_size);
           free((void *)new_fra);
           ptr -= AFD_WORD_OFFSET_6;
           *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
           *old_fra_size = new_size + AFD_WORD_OFFSET_6;

           system_log(INFO_SIGN, NULL, 0, "Converted FRA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
   else if ((old_version == 5) && (new_version == 6))
        {
           struct fileretrieve_status_5 *old_fra;
           struct fileretrieve_status_6 *new_fra;

           /* Get the size of the old FRA file. */
           if (fstat(old_fra_fd, &stat_buf) < 0)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to fstat() %s : %s",
                         old_fra_stat, strerror(errno));
              *old_fra_size = -1;
              return(NULL);
           }
           else
           {
              if (stat_buf.st_size > 0)
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL, stat_buf.st_size,
                                 (PROT_READ | PROT_WRITE),
                                 MAP_SHARED, old_fra_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL, stat_buf.st_size,
                                     (PROT_READ | PROT_WRITE), MAP_SHARED,
                                     old_fra_stat, 0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_fra_stat, strerror(errno));
                    *old_fra_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FRA file %s is empty.", old_fra_stat);
                 *old_fra_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_5;
           old_fra = (struct fileretrieve_status_5 *)ptr;

           new_size = old_no_of_dirs * sizeof(struct fileretrieve_status_6);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_dirs, new_size, strerror(errno));
              ptr = (char *)old_fra;
              ptr -= AFD_WORD_OFFSET_5;
#ifdef HAVE_MMAP
              if (munmap(ptr, stat_buf.st_size) == -1)
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_fra_stat, strerror(errno));
              }
              *old_fra_size = -1;
              return(NULL);
           }
           new_fra = (struct fileretrieve_status_6 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_dirs; i++)
           {
              (void)strcpy(new_fra[i].dir_alias, old_fra[i].dir_alias);
              (void)strcpy(new_fra[i].host_alias, old_fra[i].host_alias);
              (void)strcpy(new_fra[i].url, old_fra[i].url);
              new_fra[i].retrieve_work_dir[0] = '\0';
              new_fra[i].timezone[0] = '\0';
              (void)memcpy(&new_fra[i].te[0], &old_fra[i].te[0],
                           (MAX_FRA_TIME_ENTRIES_6 * sizeof(struct bd_time_entry_6)));
              new_fra[i].dir_status             = old_fra[i].dir_status;
              new_fra[i].remove                 = old_fra[i].remove;
              new_fra[i].stupid_mode            = old_fra[i].stupid_mode;
              new_fra[i].delete_files_flag      = old_fra[i].delete_files_flag;
              new_fra[i].report_unknown_files   = old_fra[i].report_unknown_files;
              new_fra[i].important_dir          = old_fra[i].important_dir;
              new_fra[i].no_of_time_entries     = old_fra[i].no_of_time_entries;
              new_fra[i].force_reread           = old_fra[i].force_reread;
              new_fra[i].queued                 = old_fra[i].queued;
              new_fra[i].priority               = old_fra[i].priority;
              new_fra[i].protocol               = old_fra[i].protocol;
              new_fra[i].dir_mode               = 0;
              new_fra[i].bytes_received         = old_fra[i].bytes_received;
              new_fra[i].files_received         = old_fra[i].files_received;
              new_fra[i].last_retrieval         = old_fra[i].last_retrieval;
              new_fra[i].next_check_time        = old_fra[i].next_check_time;
              new_fra[i].unknown_file_time      = old_fra[i].unknown_file_time;
              new_fra[i].queued_file_time       = old_fra[i].queued_file_time;
              new_fra[i].unreadable_file_time   = -1;
              new_fra[i].end_character          = old_fra[i].end_character;
              new_fra[i].dir_id                 = old_fra[i].dir_id;
              new_fra[i].fsa_pos                = old_fra[i].fsa_pos;
              new_fra[i].no_of_process          = old_fra[i].no_of_process;
              new_fra[i].max_process            = old_fra[i].max_process;
              new_fra[i].dir_flag               = old_fra[i].dir_flag;
              new_fra[i].in_dc_flag             = old_fra[i].in_dc_flag;
              new_fra[i].files_in_dir           = old_fra[i].files_in_dir;
              new_fra[i].files_queued           = old_fra[i].files_queued;
              new_fra[i].bytes_in_dir           = old_fra[i].bytes_in_dir;
              new_fra[i].bytes_in_queue         = old_fra[i].bytes_in_queue;
              new_fra[i].ignore_file_time       = old_fra[i].ignore_file_time;
              new_fra[i].ignore_size            = old_fra[i].ignore_size;
              new_fra[i].gt_lt_sign             = old_fra[i].gt_lt_sign;
              new_fra[i].keep_connected         = old_fra[i].keep_connected;
#ifdef WITH_DUP_CHECK
              new_fra[i].dup_check_flag         = old_fra[i].dup_check_flag;
              new_fra[i].dup_check_timeout      = old_fra[i].dup_check_timeout;
#endif
              new_fra[i].warn_time              = old_fra[i].warn_time;
              new_fra[i].info_time              = DEFAULT_DIR_INFO_TIME;
              new_fra[i].start_event_handle     = old_fra[i].start_event_handle;
              new_fra[i].end_event_handle       = old_fra[i].end_event_handle;
              new_fra[i].max_copied_files       = max_copied_files;
              new_fra[i].max_copied_file_size   = max_copied_file_size;
              if (old_fra[i].wait_for_filename[0] == '\0')
              {
                 new_fra[i].wait_for_filename[0] = '\0';
              }
              else
              {
                 (void)strcpy(new_fra[i].wait_for_filename,
                              old_fra[i].wait_for_filename);
              }
              new_fra[i].accumulate             = old_fra[i].accumulate;
              new_fra[i].accumulate_size        = old_fra[i].accumulate_size;
              new_fra[i].locked_file_time       = old_fra[i].locked_file_time;
              (void)memcpy(&new_fra[i].ate, &old_fra[i].ate,
                           sizeof(struct bd_time_entry_5));
              new_fra[i].max_errors             = old_fra[i].max_errors;
              new_fra[i].error_counter          = old_fra[i].error_counter;
           }
           ptr = (char *)old_fra;
           ptr -= AFD_WORD_OFFSET_5;

           /*
            * Resize the old FRA to the size of new one and then copy
            * the new structure into it. Then update the FRA version
            * number.
            */
           if ((ptr = mmap_resize(old_fra_fd, ptr, new_size + AFD_WORD_OFFSET_6)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_fra_stat, strerror(errno));
              free((void *)new_fra);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_6;
           (void)memcpy(ptr, new_fra, new_size);
           free((void *)new_fra);
           ptr -= AFD_WORD_OFFSET_6;
           *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
           *old_fra_size = new_size + AFD_WORD_OFFSET_6;

           system_log(INFO_SIGN, NULL, 0, "Converted FRA from verion %d to %d.",
                      (int)old_version, (int)new_version);
        }
        else
        {
           system_log(ERROR_SIGN, NULL, 0,
                      "Don't know how to convert a version %d FRA to version %d.",
                      old_version, new_version);
           ptr = NULL;
        }

   return(ptr);
}
