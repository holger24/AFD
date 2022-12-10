/*
 *  get_info.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2022 Deutscher Wetterdienst (DWD),
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

DESCR__S_M3
/*
 ** NAME
 **   get_info - retrieves information out of the AMG history file
 **
 ** SYNOPSIS
 **   void get_info(int item)
 **   void get_info_free(void)
 **   int  get_sum_data(int    item,
 **                     time_t *date,
 **                     double *file_size,
 **                     double *trans_time)
 **
 ** DESCRIPTION
 **   This function get_info() searches the AMG history file for the
 **   job number of the selected file item. It then fills the structures
 **   item_list and info_data with data from the AMG history file.
 **
 ** RETURN VALUES
 **   None. The function will exit() with INCORRECT when it fails to
 **   allocate memory or fails to seek in the AMG history file.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.04.1997 H.Kiehl Created
 **   14.01.1998 H.Kiehl Support for remote file name.
 **   31.12.2003 H.Kiehl Read file masks from separate file.
 **   05.11.2012 H.Kiehl Handle mail ID's.
 **   29.08.2013 H.Kiehl Added get_info_free().
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* strerror()                      */
#include <stdlib.h>                   /* strtoul()                       */
#include <unistd.h>                   /* close()                         */
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "show_olog.h"
#include "mafd_ctrl.h"

/* Global variables. */
unsigned int                  *current_jid_list;
int                           no_of_current_jobs;

/* External global variables. */
extern int                    log_date_length,
                              max_hostname_length,
                              no_of_log_files;
extern char                   *p_work_dir;
extern struct item_list       *il;
extern struct info_data       id;

/* Local variables. */
static int                    *no_of_dc_ids,
                              *no_of_dir_names,
                              *no_of_job_ids;
static off_t                  dcl_size,
                              dnb_size,
                              jd_size;
static struct job_id_data     *jd = NULL;
static struct dir_name_buf    *dnb = NULL;
static struct dir_config_list *dcl = NULL;

/* Local function prototypes. */
static unsigned int           get_all(int);
static void                   get_dir_data(int),
                              get_job_data(struct job_id_data *);


/*############################### get_info() ############################*/
void
get_info(int item)
{
   int i;

   current_jid_list = NULL;
   no_of_current_jobs = 0;
   if ((item != GOT_JOB_ID) && (item != GOT_JOB_ID_DIR_ONLY) &&
       (item != GOT_JOB_ID_USER_ONLY) && (item != GOT_JOB_ID_PRIORITY_ONLY))
   {
      id.job_no = get_all(item - 1);
      if (id.is_receive_job == YES)
      {
         if (get_current_jid_list() == INCORRECT)
         {
            free(current_jid_list);
            return;
         }
      }
   }

   /*
    * Go through job ID database and find the job ID.
    */
   if (jd == NULL)
   {
      int          fd;
      char         job_id_data_file[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat  stat_buf;
#endif

      /* Map to job ID data file. */
      (void)sprintf(job_id_data_file, "%s%s%s", p_work_dir, FIFO_DIR,
                    JOB_ID_DATA_FILE);
      if ((fd = open(job_id_data_file, O_RDONLY)) == -1)
      {
         (void)xrec(ERROR_DIALOG, "Failed to open() `%s' : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         free(current_jid_list);
         return;
      }
#ifdef HAVE_STATX
      if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(fd, &stat_buf) == -1)
#endif
      {
         (void)xrec(ERROR_DIALOG, "Failed to access `%s' : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         free(current_jid_list);
         (void)close(fd);
         return;
      }
#ifdef HAVE_STATX
      if (stat_buf.stx_size > 0)
#else
      if (stat_buf.st_size > 0)
#endif
      {
         char *ptr;

         if ((ptr = mmap(NULL,
#ifdef HAVE_STATX
                         stat_buf.stx_size, PROT_READ,
#else
                         stat_buf.st_size, PROT_READ,
#endif
                         MAP_SHARED, fd, 0)) == (caddr_t) -1)
         {
            (void)xrec(ERROR_DIALOG, "Failed to mmap() `%s' : %s (%s %d)",
                       job_id_data_file, strerror(errno), __FILE__, __LINE__);
            (void)close(fd);
            return;
         }
         if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_JID_VERSION)
         {
            (void)xrec(ERROR_DIALOG, "Incorrect JID version (data=%d current=%d)!",
                       *(ptr + SIZEOF_INT + 1 + 1 + 1), CURRENT_JID_VERSION);
            (void)close(fd);
            return;
         }
         no_of_job_ids = (int *)ptr;
         ptr += AFD_WORD_OFFSET;
         jd = (struct job_id_data *)ptr;
#ifdef HAVE_STATX
         jd_size = stat_buf.stx_size;
#else
         jd_size = stat_buf.st_size;
#endif
         (void)close(fd);
      }
      else
      {
         (void)xrec(ERROR_DIALOG, "Job ID database file is empty. (%s %d)",
                    __FILE__, __LINE__);
         (void)close(fd);
         return;
      }

      /* Map to directory name buffer. */
      (void)sprintf(job_id_data_file, "%s%s%s", p_work_dir, FIFO_DIR,
                    DIR_NAME_FILE);
      if ((fd = open(job_id_data_file, O_RDONLY)) == -1)
      {
         (void)xrec(ERROR_DIALOG, "Failed to open() `%s' : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         return;
      }
#ifdef HAVE_STATX
      if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(fd, &stat_buf) == -1)
#endif
      {
         (void)xrec(ERROR_DIALOG, "Failed to access `%s' : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return;
      }
#ifdef HAVE_STATX
      if (stat_buf.stx_size > 0)
#else
      if (stat_buf.st_size > 0)
#endif
      {
         char *ptr;

         if ((ptr = mmap(NULL,
#ifdef HAVE_STATX
                         stat_buf.stx_size, PROT_READ,
#else
                         stat_buf.st_size, PROT_READ,
#endif
                         MAP_SHARED, fd, 0)) == (caddr_t) -1)
         {
            (void)xrec(ERROR_DIALOG, "Failed to mmap() `%s' : %s (%s %d)",
                       job_id_data_file, strerror(errno), __FILE__, __LINE__);
            (void)close(fd);
            return;
         }
         no_of_dir_names = (int *)ptr;
         ptr += AFD_WORD_OFFSET;
         dnb = (struct dir_name_buf *)ptr;
#ifdef HAVE_STATX
         dnb_size = stat_buf.stx_size;
#else
         dnb_size = stat_buf.st_size;
#endif
         (void)close(fd);
      }
      else
      {
         (void)xrec(ERROR_DIALOG, "Dirname database file is empty. (%s %d)",
                    __FILE__, __LINE__);
         (void)close(fd);
         return;
      }

      /* Map to DIR_CONFIG name database. */
      (void)sprintf(job_id_data_file, "%s%s%s", p_work_dir, FIFO_DIR,
                    DC_LIST_FILE);
      if ((fd = open(job_id_data_file, O_RDONLY)) == -1)
      {
         (void)xrec(ERROR_DIALOG, "Failed to open() `%s' : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         return;
      }
#ifdef HAVE_STATX
      if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(fd, &stat_buf) == -1)
#endif
      {
         (void)xrec(ERROR_DIALOG, "Failed to access `%s' : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return;
      }
#ifdef HAVE_STATX
      if (stat_buf.stx_size > 0)
#else
      if (stat_buf.st_size > 0)
#endif
      {
         char *ptr;

         if ((ptr = mmap(NULL,
#ifdef HAVE_STATX
                         stat_buf.stx_size, PROT_READ,
#else
                         stat_buf.st_size, PROT_READ,
#endif
                         MAP_SHARED, fd, 0)) == (caddr_t) -1)
         {
            (void)xrec(ERROR_DIALOG, "Failed to mmap() `%s' : %s (%s %d)",
                       job_id_data_file, strerror(errno), __FILE__, __LINE__);
            (void)close(fd);
            return;
         }
         if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_DCID_VERSION)
         {
            (void)xrec(ERROR_DIALOG, "Incorrect DCID version (data=%d current=%d)!",
                       *(ptr + SIZEOF_INT + 1 + 1 + 1), CURRENT_DCID_VERSION);
            (void)close(fd);
            return;
         }
         no_of_dc_ids = (int *)ptr;
         ptr += AFD_WORD_OFFSET;
         dcl = (struct dir_config_list *)ptr;
#ifdef HAVE_STATX
         dcl_size = stat_buf.stx_size;
#else
         dcl_size = stat_buf.st_size;
#endif
         (void)close(fd);
      }
      else
      {
         (void)xrec(ERROR_DIALOG,
                    "DIR_CONFIG ID database file is empty. (%s %d)",
                    __FILE__, __LINE__);
         (void)close(fd);
         return;
      }
   }

   /* Search through all history files. */
   if (id.is_receive_job == YES)
   {
      if (item == GOT_JOB_ID_DIR_ONLY)
      {
         for (i = 0; i < *no_of_dir_names; i++)
         {
            if (id.job_no == dnb[i].dir_id)
            {
               (void)strcpy((char *)id.dir, dnb[i].dir_name);
               id.dir_id = id.job_no;
               (void)snprintf(id.dir_id_str, MAX_DIR_ALIAS_LENGTH + 1, "%x",
                              id.dir_id);
               break;
            }
         }
      }
      else if (item == GOT_JOB_ID_USER_ONLY)
           {
              id.user[0] = '\0';
              id.mail_destination[0] = '\0';
           }
      else if (item == GOT_JOB_ID_PRIORITY_ONLY)
           {
              id.priority = 0;
           }
           else
           {
              for (i = 0; i < *no_of_dir_names; i++)
              {
                 if (id.job_no == dnb[i].dir_id)
                 {
                    id.dir_id = id.job_no;
                    get_dir_data(i);
                    break;
                 }
              }
           }
   }
   else
   {
      for (i = 0; i < *no_of_job_ids; i++)
      {
         if (id.job_no == jd[i].job_id)
         {
            if (item == GOT_JOB_ID_DIR_ONLY)
            {
               (void)strcpy((char *)id.dir, dnb[jd[i].dir_id_pos].dir_name);
               id.dir_id = jd[i].dir_id;
               (void)snprintf(id.dir_id_str, MAX_DIR_ALIAS_LENGTH + 1, "%x",
                              id.dir_id);
            }
            else if (item == GOT_JOB_ID_USER_ONLY)
                 {
                    char *ptr = jd[i].recipient;

                    while ((*ptr != '/') && (*ptr != '\0'))
                    {
                       if (*ptr == '\\')
                       {
                          ptr++;
                       }
                       ptr++;
                    }
                    if ((*ptr == '/') && (*(ptr + 1) == '/'))
                    {
                       int count = 0;

                       ptr += 2;
                       while ((*ptr != ':') && (*ptr != '@') && (*ptr != '\0'))
                       {
                          if (*ptr == '\\')
                          {
                             ptr++;
                          }
                          id.user[count] = id.mail_destination[count] = *ptr;
                          ptr++; count++;
                       }
                       id.user[count] = ' '; /* for sfilter() */
                       id.user[count + 1] = '\0';

                       /*
                        * Need to check if server= option is set so we can
                        * get the full mail address.
                        */
                       if (*ptr == ':') /* Omit the password. */
                       {
                          while ((*ptr != '@') && (*ptr != '\0'))
                          {
                             if (*ptr == '\\')
                             {
                                ptr++;
                             }
                             ptr++;
                          }
                       }
                       if (*ptr == '@')
                       {
                          id.mail_destination[count] = *ptr;
                          ptr++; count++;
                          while ((*ptr != ';') && (*ptr != ':') &&
                                 (*ptr != '/') && (*ptr != '\0'))
                          {
                             if (*ptr == '\\')
                             {
                                ptr++;
                             }
                             id.mail_destination[count] = *ptr;
                             ptr++; count++;
                          }
                          while ((*ptr != ';') && (*ptr != '\0'))
                          {
                             if (*ptr == '\\')
                             {
                                ptr++;
                             }
                             ptr++;
                          }
                          if ((*ptr == ';') && (*(ptr + 1) == 's') &&
                              (*(ptr + 2) == 'e') && (*(ptr + 3) == 'r') &&
                              (*(ptr + 4) == 'v') && (*(ptr + 5) == 'e') &&
                              (*(ptr + 6) == 'r') && (*(ptr + 7) == '='))
                          {
                             id.mail_destination[count] = ' '; /* for sfilter() */
                             id.mail_destination[count + 1] = '\0';
                          }
                          else
                          {
                             id.mail_destination[0] = '\0';
                          }
                       }
                       else
                       {
                          id.mail_destination[0] = '\0';
                       }
                    }
                 }
            else if (item == GOT_JOB_ID_PRIORITY_ONLY)
                 {
                    id.priority = jd[i].priority;
                 }
                 else
                 {
                    get_job_data(&jd[i]);
                 }

            return;
         }
      }
   }

   free(current_jid_list);
   return;
}


/*############################ get_info_free() ##########################*/
void
get_info_free(void)
{
   if (jd != NULL)
   {
      if (munmap(((char *)jd - AFD_WORD_OFFSET), jd_size) < 0)
      {
         (void)xrec(WARN_DIALOG, "munmap() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
      }
      else
      {
         jd = NULL;
      }
   }
   if (dnb != NULL)
   {
      if (munmap(((char *)dnb - AFD_WORD_OFFSET), dnb_size) < 0)
      {
         (void)xrec(WARN_DIALOG, "munmap() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
      }
      else
      {
         dnb = NULL;
      }
   }
   if (dcl != NULL)
   {
      if (munmap(((char *)dcl - AFD_WORD_OFFSET), dcl_size) < 0)
      {
         (void)xrec(WARN_DIALOG, "munmap() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
      }
      else
      {
         dcl = NULL;
      }
   }

   return;
}


/*############################ get_sum_data() ###########################*/
int
get_sum_data(int item, time_t *date, double *file_size, double *trans_time)
{
   int  file_no,
        total_no_of_items = 0,
        pos = -1;

   /* Determine log file and position in this log file. */
   for (file_no = 0; file_no < no_of_log_files; file_no++)
   {
      total_no_of_items += il[file_no].no_of_items;

      if (item < total_no_of_items)
      {
         pos = item - (total_no_of_items - il[file_no].no_of_items);
         break;
      }
   }

   /* Get the date, file size and transfer time. */
   if (pos > -1)
   {
      int  i;
      char *ptr,
           buffer[MAX_FILENAME_LENGTH + MAX_PATH_LENGTH],
           str_hex_number[23 + 1];

      /* Go to beginning of line to read complete line. */
      if (fseek(il[file_no].fp,
                (long)(il[file_no].line_offset[pos]),
                SEEK_SET) == -1)
      {
         (void)xrec(FATAL_DIALOG, "fseek() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }

      if (fgets(buffer, MAX_FILENAME_LENGTH + MAX_PATH_LENGTH, il[file_no].fp) == NULL)
      {
         (void)xrec(WARN_DIALOG, "fgets() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }
      ptr = buffer;
      str_hex_number[0] = '0';
      str_hex_number[1] = 'x';
      i = 2;
      while ((*ptr != SEPARATOR_CHAR) && (i < (log_date_length + 1)))
      {
         str_hex_number[i] = *ptr;
         ptr++; i++;
      }
      if (*ptr == SEPARATOR_CHAR)
      {
         str_hex_number[i] = '\0';
         *date = (time_t)str2timet(str_hex_number, NULL, 16);
      }
      else
      {
         *date = 0L;
      }

      /* Ignore file name. */
      if (buffer[log_date_length + 1 + max_hostname_length + 2] == ' ')
      {
#ifdef ACTIVATE_THIS_AFTER_VERSION_14
         ptr = &buffer[log_date_length + 1 + max_hostname_length + 7];
#else
         if (buffer[log_date_length + 1 + max_hostname_length + 4] == ' ')
         {
            ptr = &buffer[log_date_length + 1 + max_hostname_length + 7];
         }
         else
         {
            ptr = &buffer[log_date_length + 1 + max_hostname_length + 3];
         }
#endif
      }
      else
      {
         ptr = &buffer[log_date_length + 1 + max_hostname_length + 3];
      }
      while (*ptr != SEPARATOR_CHAR)
      {
         ptr++;
      }
      if (*(ptr + 1) != SEPARATOR_CHAR)
      {
         ptr++;
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
      }
      else
      {
         ptr++;
      }
      ptr++;

      /* Get file size. */
      i = 2;
      while ((*ptr != SEPARATOR_CHAR) && (i < 23))
      {
         str_hex_number[i] = *ptr;
         ptr++; i++;
      }
      if (*ptr == SEPARATOR_CHAR)
      {
         str_hex_number[i] = '\0';
#ifdef HAVE_STRTOULL
         *file_size = (double)strtoull(str_hex_number, NULL, 16);
#else
         *file_size = (double)strtoul(str_hex_number, NULL, 16);
#endif
         ptr++;
      }
      else if (i >= 23)
           {
              while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
              {
                 ptr++;
              }
              if (*ptr == SEPARATOR_CHAR)
              {
                 ptr++;
              }
           }

      /* Get transfer time. */
      *trans_time = strtod(ptr, NULL);
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ get_all() +++++++++++++++++++++++++++++*/
/*                                ---------                              */
/* Description: Retrieves the full local file name, remote file name (if */
/*              it exists), job number and if available the archive      */
/*              directory out of the log file.                           */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static unsigned int
get_all(int item)
{
   int  file_no,
        total_no_of_items = 0,
        pos = -1;

   /* Determine log file and position in this log file. */
   for (file_no = 0; file_no < no_of_log_files; file_no++)
   {
      total_no_of_items += il[file_no].no_of_items;

      if (item < total_no_of_items)
      {
         pos = item - (total_no_of_items - il[file_no].no_of_items);
         break;
      }
   }

   /* Get the job ID and file name. */
   if (pos > -1)
   {
      int  i = 0,
           type_offset;
      char *ptr,
           *p_tmp,
           buffer[MAX_FILENAME_LENGTH + MAX_PATH_LENGTH],
           str_hex_number[23 + 1];

      if (fseek(il[file_no].fp, (long)il[file_no].line_offset[pos], SEEK_SET) == -1)
      {
         (void)xrec(FATAL_DIALOG, "fseek() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
         return(0U);
      }

      if (fgets(buffer, MAX_FILENAME_LENGTH + MAX_PATH_LENGTH, il[file_no].fp) == NULL)
      {
         (void)xrec(WARN_DIALOG, "fgets() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return(0U);
      }

      /* Store the date. */
      ptr = buffer;
      while ((*ptr != ' ') && (i < (log_date_length + 1)))
      {
         ptr++; i++;
      }
      *ptr = '\0';
      id.date_send = (time_t)str2timet(buffer, NULL, 16);

      /* Store local file name. */
      if (buffer[log_date_length + 1 + max_hostname_length + 2] == ' ')
      {
#ifdef ACTIVATE_THIS_AFTER_VERSION_14
         type_offset = 5;
         if (buffer[log_date_length + 1 + max_hostname_length + 1] == ('0' + OT_NORMAL_RECEIVED))
         {
            id.is_receive_job = YES;
         }
         else
         {
            id.is_receive_job = NO;
         }
#else
         if (buffer[log_date_length + 1 + max_hostname_length + 4] == ' ')
         {
            type_offset = 5;
            if (buffer[log_date_length + 1 + max_hostname_length + 1] == ('0' + OT_NORMAL_RECEIVED))
            {
               id.is_receive_job = YES;
            }
            else
            {
               id.is_receive_job = NO;
            }
         }
         else
         {
            type_offset = 3;
            id.is_receive_job = NO;
         }
#endif
      }
      else
      {
         type_offset = 1;
         id.is_receive_job = NO;
      }
      ptr = &buffer[log_date_length + 1 + max_hostname_length + type_offset + 2];
      i = 0;
      while ((*ptr != SEPARATOR_CHAR) && (i < MAX_FILENAME_LENGTH))
      {
         id.local_file_name[i] = *ptr;
         i++; ptr++;
      }
      if (i == MAX_FILENAME_LENGTH)
      {
         id.local_file_name[i - 2] = ' ';
         id.local_file_name[i - 1] = '\0';
         id.remote_file_name[0] = ' ';
         id.remote_file_name[1] = '\0';
         id.file_size[0] = '0';
         id.file_size[1] = '\0';
         id.trans_time[0] = '\0';
         id.unique_name[0] = '\0';
         id.mail_id[0] = '\0';
         id.archive_dir[0] = '\0';

         return(0U);
      }

      id.local_file_name[i] = '\0';
      ptr++;

      /* Store remote file name. */
      i = 0;
      if (*ptr != SEPARATOR_CHAR)
      {
         while ((*ptr != SEPARATOR_CHAR) && (i < MAX_FILENAME_LENGTH))
         {
            id.remote_file_name[i] = *ptr;
            i++; ptr++;
         }
         if (i == MAX_FILENAME_LENGTH)
         {
            id.remote_file_name[i - 2] = ' ';
            id.remote_file_name[i - 1] = '\0';
            id.file_size[0] = '0';
            id.file_size[1] = '\0';
            id.trans_time[0] = '\0';
            id.unique_name[0] = '\0';
            id.mail_id[0] = '\0';
            id.archive_dir[0] = '\0';

            return(0U);
         }
      }
      ptr++;
      id.remote_file_name[i] = '\0';

      /* Away with the file size. */
      str_hex_number[0] = '0';
      str_hex_number[1] = 'x';
      i = 2;
      while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n') && (i < 23))
      {
         str_hex_number[i] = *ptr;
         ptr++; i++;
      }
      if (*ptr == SEPARATOR_CHAR)
      {
         str_hex_number[i] = '\0';
#if SIZEOF_OFF_T == 4
         (void)sprintf(id.file_size, "%ld",
#else
         (void)sprintf(id.file_size, "%lld",
#endif
                       (pri_off_t)str2offt(str_hex_number, NULL, 16));
         ptr++;
      }
      else if (i >= 23)
           {
              while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
              {
                 ptr++;
              }
              if (*ptr == SEPARATOR_CHAR)
              {
                 ptr++;
              }
           }

      /* Away with the transfer time. */
      i = 0;
      while (*ptr != SEPARATOR_CHAR)
      {
         if (i < (MAX_INT_LENGTH + MAX_INT_LENGTH))
         {
            id.trans_time[i] = *ptr;
            i++;
         }
         ptr++;
      }
      id.trans_time[i] = '\0';
      ptr++;

      /* Ignore number of retries. */
      if (type_offset > 1)
      {
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         if (*ptr == SEPARATOR_CHAR)
         {
            ptr++;
         }
      }

      /* Get the job ID. */
      p_tmp = ptr;
      while ((*ptr != '\n') && (*ptr != SEPARATOR_CHAR))
      {
         ptr++;
      }
      if (*ptr == SEPARATOR_CHAR)
      {
         *ptr = '\0';
         ptr++;

         /* Away with unique string. */
         i = 0;
         while ((*ptr != '\n') && (*ptr != SEPARATOR_CHAR) && (*ptr != ' ') &&
                (i < MAX_ADD_FNL))
         {
            id.unique_name[i] = *ptr;
            i++; ptr++;
         }
         id.unique_name[i] = '\0';
         if (i == MAX_ADD_FNL)
         {
            while ((*ptr != '\n') && (*ptr != SEPARATOR_CHAR) && (*ptr != ' '))
            {
               ptr++;
            }
         }
         if (*ptr == ' ')
         {
            ptr++;
            i = 0;

            while ((*ptr != '\n') && (*ptr != SEPARATOR_CHAR) &&
                   (i < MAX_MAIL_ID_LENGTH))
            {
               id.mail_id[i] = *ptr;
               i++; ptr++;
            }
            id.mail_id[i] = '\0';
            if (i == MAX_MAIL_ID_LENGTH)
            {
               while ((*ptr != '\n') && (*ptr != SEPARATOR_CHAR))
               {
                  ptr++;
               }
            }
         }
         else
         {
            id.mail_id[0] = '\0';
         }

         if (*ptr == SEPARATOR_CHAR)
         {
            ptr++;
            i = 0;
            while (*ptr != '\n')
            {
               id.archive_dir[i] = *ptr;
               i++; ptr++;
            }
            id.archive_dir[i] = '\0';
         }
      }
      else
      {
         *ptr = '\0';
      }

      return((unsigned int)strtoul(p_tmp, NULL, 16));
   }
   else
   {
      return(0U);
   }
}


/*++++++++++++++++++++++++++++ get_job_data() +++++++++++++++++++++++++++*/
/*                             --------------                            */
/* Descriptions: This function gets all data that was in the             */
/*               AMG history file and copies them into the global        */
/*               structure 'info_data'.                                  */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void
get_job_data(struct job_id_data *p_jd)
{
   register int  i;
   register char *p_tmp;

   /* Get DIR_CONFIG name. */
   id.dir_config_file[0] = '\0';
   for (i = 0; i < *no_of_dc_ids; i++)
   {
      if (p_jd->dir_config_id == dcl[i].dc_id)
      {
         (void)strcpy(id.dir_config_file, dcl[i].dir_config_file);
         break;
      }
   }

   (void)strcpy((char *)id.dir, dnb[p_jd->dir_id_pos].dir_name);
   id.dir_id = p_jd->dir_id;
   (void)sprintf(id.dir_id_str, "%x", id.dir_id);
   get_dir_options(id.dir_id, &id.d_o);

   id.priority = p_jd->priority;
   get_file_mask_list(p_jd->file_mask_id, &id.no_of_files, &id.files);
   if (id.files == NULL)
   {
      (void)xrec(WARN_DIALOG,
                 "Failed to get file mask list, see system log for more details.");
   }
   id.no_of_loptions = p_jd->no_of_loptions;

   /* Save all AMG (local) options. */
   if (id.no_of_loptions > 0)
   {
#ifdef _WITH_DYNAMIC_MEMORY
      RT_ARRAY(id.loptions, id.no_of_loptions, MAX_OPTION_LENGTH, char);
#endif
      p_tmp = p_jd->loptions;
      for (i = 0; i < id.no_of_loptions; i++)
      {
         (void)strcpy(id.loptions[i], p_tmp);
         NEXT(p_tmp);
      }
   }

   /* Save all FD (standart) options. */
   id.no_of_soptions = p_jd->no_of_soptions;
   if (id.no_of_soptions > 0)
   {
      size_t size;

      size = strlen(p_jd->soptions);
      if ((id.soptions = calloc(size + 1, sizeof(char))) == NULL)
      {
         (void)xrec(FATAL_DIALOG, "calloc() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }
      (void)memcpy(id.soptions, p_jd->soptions, size);
      id.soptions[size] = '\0';
   }
   else
   {
      id.soptions = NULL;
   }

   (void)strcpy(id.recipient, p_jd->recipient);

   return;
}


/*++++++++++++++++++++++++++++ get_dir_data() +++++++++++++++++++++++++++*/
/*                             --------------                            */
/* Descriptions: This function gets all data that was in the             */
/*               AMG history file and copies them into the global        */
/*               structure 'info_data'.                                  */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void
get_dir_data(int dir_pos)
{
   register int  i,
                 j;
   register char *p_file,
                 *p_tmp;
   int           gotcha,
                 ret;

   (void)strcpy((char *)id.dir, dnb[dir_pos].dir_name);

   get_dir_options(id.dir_id, &id.d_o);

   id.count = 0;
   for (i = (*no_of_job_ids - 1); i > -1; i--)
   {
      if (jd[i].dir_id_pos == dir_pos)
      {
         for (j = 0; j < no_of_current_jobs; j++)
         {
            if (current_jid_list[j] == jd[i].job_id)
            {
               int k;

               /* Allocate memory to hold all data. */
               if ((id.count % 10) == 0)
               {
                  size_t new_size;

                  /* Calculate new size. */
                  new_size = ((id.count / 10) + 1) * 10 *
                             sizeof(struct db_entry);

                  /* Create or increase the space for the buffer. */
                  if ((id.dbe = realloc(id.dbe, new_size)) == (struct db_entry *)NULL)
                  {
                     (void)xrec(FATAL_DIALOG, "realloc() error : %s (%s %d)",
                                strerror(errno), __FILE__, __LINE__);
                     return;
                  }
               }

               /* Get DIR_CONFIG name. */
               id.dbe[id.count].dir_config_file[0] = '\0';
               for (k = 0; k < *no_of_dc_ids; k++)
               {
                  if (jd[i].dir_config_id == dcl[k].dc_id)
                  {
                     (void)strcpy(id.dbe[id.count].dir_config_file,
                                  dcl[k].dir_config_file);
                     break;
                  }
               }

               id.dbe[id.count].priority = jd[i].priority;
               get_file_mask_list(jd[i].file_mask_id,
                                  &id.dbe[id.count].no_of_files,
                                  &id.dbe[id.count].files);
               if (id.dbe[id.count].files != NULL)
               {
                  p_file = id.dbe[id.count].files;

                  /*
                   * Only show those entries that really match the current
                   * file name. For this it is necessary to filter the file
                   * names through all the filters.
                   */
                  gotcha = NO;
                  for (k = 0; k < id.dbe[id.count].no_of_files; k++)
                  {
                     if ((ret = pmatch(p_file, (char *)id.local_file_name, NULL)) == 0)
                     {
                        gotcha = YES;
                        break;
                     }
                     else if (ret == 1)
                          {
                             /* This file is NOT wanted! */
                             break;
                          }
                     NEXT(p_file);
                  }
                  if (gotcha == YES)
                  {
                     /* Save all AMG (local) options. */
                     id.dbe[id.count].no_of_loptions = jd[i].no_of_loptions;
                     if (id.dbe[id.count].no_of_loptions > 0)
                     {
                        p_tmp = jd[i].loptions;
                        for (k = 0; k < id.dbe[id.count].no_of_loptions; k++)
                        {
                           (void)strcpy(id.dbe[id.count].loptions[k], p_tmp);
                           NEXT(p_tmp);
                        }
                     }

                     /* Save all FD (standart) options. */
                     id.dbe[id.count].no_of_soptions = jd[i].no_of_soptions;
                     if (id.dbe[id.count].no_of_soptions > 0)
                     {
                        size_t size;

                        size = strlen(jd[i].soptions);
                        if ((id.dbe[id.count].soptions = calloc(size + 1,
                                                                sizeof(char))) == NULL)
                        {
                           (void)xrec(FATAL_DIALOG, "calloc() error : %s (%s %d)",
                                      strerror(errno), __FILE__, __LINE__);
                           return;
                        }
                        (void)memcpy(id.dbe[id.count].soptions, jd[i].soptions,
                                     size);
                        id.dbe[id.count].soptions[size] = '\0';
                     }
                     else
                     {
                        id.dbe[id.count].soptions = NULL;
                     }

                     (void)strcpy(id.dbe[id.count].recipient, jd[i].recipient);
                     id.dbe[id.count].job_id = jd[i].job_id;
                     id.count++;
                  }
                  else
                  {
                     free(id.dbe[id.count].files);
                     id.dbe[id.count].files = NULL;
                  }
               }
            } /* if (current_jid_list[j] == jd[i].job_id) */
         } /* for (j = 0; j < no_of_current_jobs; j++) */
      } /* if (jd[i].dir_id_pos == dir_pos) */
   } /* for (i = (*no_of_job_ids - 1); i > -1; i--) */

   return;
}
