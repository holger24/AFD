/*
 *  get_info.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **                     double *file_size)
 **
 ** DESCRIPTION
 **   This function searches the AMG history file for the job number
 **   of the selected file item. It then fills the structures item_list
 **   and info_data with data from the AMG history file.
 **
 ** RETURN VALUES
 **   None. The function will exit() with INCORRECT when it fails to
 **   allocate memory or fails to seek in the AMG history file.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.02.1998 H.Kiehl Created
 **   29.08.2013 H.Kiehl Added get_info_free().
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* strerror()                      */
#include <stdlib.h>                   /* strtoul()                       */
#include <unistd.h>                   /* close()                         */
#include <ctype.h>                    /* isxdigit()                      */
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "show_dlog.h"
#include "mafd_ctrl.h"
#include "dr_str.h"

/* Global variables. */
unsigned int               *current_jid_list;
int                        no_of_current_jobs;

/* External global variables. */
extern int                 log_date_length,
                           max_hostname_length,
                           no_of_log_files;
extern char                *p_work_dir;
extern struct item_list    *il;
extern struct info_data    id;

/* Local variables. */
static int                 *no_of_dir_names,
                           *no_of_job_ids;
static off_t               dnb_size,
                           jd_size;
static struct job_id_data  *jd = NULL;
static struct dir_name_buf *dnb = NULL;

/* Local function prototypes. */
static void                get_all(int),
                           get_dir_data(int),
                           get_job_data(struct job_id_data *);


/*############################### get_info() ############################*/
void
get_info(int item)
{
   int i;

   if ((item != GOT_JOB_ID) && (item != GOT_JOB_ID_DIR_ONLY))
   {
      get_all(item - 1);
   }

   /*
    * Go through job ID database and find the job ID.
    */
   if (jd == NULL)
   {
      int          dnb_fd,
                   jd_fd;
      char         job_id_data_file[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat  stat_buf;
#endif

      /* Map to job ID data file. */
      (void)sprintf(job_id_data_file, "%s%s%s", p_work_dir, FIFO_DIR,
                    JOB_ID_DATA_FILE);
      if ((jd_fd = open(job_id_data_file, O_RDONLY)) == -1)
      {
         (void)xrec(ERROR_DIALOG, "Failed to open() %s : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         return;
      }
#ifdef HAVE_STATX
      if (statx(jd_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(jd_fd, &stat_buf) == -1)
#endif
      {
         (void)xrec(ERROR_DIALOG, "Failed to access %s : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         (void)close(jd_fd);
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
                         MAP_SHARED, jd_fd, 0)) == (caddr_t) -1)
         {
            (void)xrec(ERROR_DIALOG, "Failed to mmap() to %s : %s (%s %d)",
                       job_id_data_file, strerror(errno), __FILE__, __LINE__);
            (void)close(jd_fd);
            return;
         }
         if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_JID_VERSION)
         {
            (void)xrec(ERROR_DIALOG, "Incorrect JID version (data=%d current=%d)!",
                       *(ptr + SIZEOF_INT + 1 + 1 + 1), CURRENT_JID_VERSION);
            (void)close(jd_fd);
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
         (void)close(jd_fd);
      }
      else
      {
         (void)xrec(ERROR_DIALOG, "Job ID database file is empty. (%s %d)",
                    __FILE__, __LINE__);
         (void)close(jd_fd);
         return;
      }

      /* Map to directory name buffer. */
      (void)sprintf(job_id_data_file, "%s%s%s", p_work_dir, FIFO_DIR,
                    DIR_NAME_FILE);
      if ((dnb_fd = open(job_id_data_file, O_RDONLY)) == -1)
      {
         (void)xrec(ERROR_DIALOG, "Failed to open() %s : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         return;
      }
#ifdef HAVE_STATX
      if (statx(dnb_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(dnb_fd, &stat_buf) == -1)
#endif
      {
         (void)xrec(ERROR_DIALOG, "Failed to access %s : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         (void)close(dnb_fd);
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
                         MAP_SHARED, dnb_fd, 0)) == (caddr_t) -1)
         {
            (void)xrec(ERROR_DIALOG, "Failed to mmap() to %s : %s (%s %d)",
                       job_id_data_file, strerror(errno), __FILE__, __LINE__);
            (void)close(dnb_fd);
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
         (void)close(dnb_fd);
      }
      else
      {
         (void)xrec(ERROR_DIALOG, "Job ID database file is empty. (%s %d)",
                    __FILE__, __LINE__);
         (void)close(dnb_fd);
         return;
      }
   }

   if (item == GOT_JOB_ID_DIR_ONLY)
   {
      if (id.dir_id != 0)
      {
         for (i = 0; i < *no_of_dir_names; i++)
         {
            if (id.dir_id == dnb[i].dir_id)
            {
               (void)strcpy((char *)id.dir, dnb[i].dir_name);
               (void)sprintf(id.dir_id_str, "%x", id.dir_id);
               break;
            }
         } /* for (i = 0; i < *no_of_dir_names; i++) */
      }
      else if (id.job_id != 0)
           {
              for (i = 0; i < *no_of_job_ids; i++)
              {
                 if (id.job_id == jd[i].job_id)
                 {
                    (void)strcpy((char *)id.dir, dnb[jd[i].dir_id_pos].dir_name);
                    id.dir_id = jd[i].dir_id;
                    (void)sprintf(id.dir_id_str, "%x", id.dir_id);
                    break;
                 }
              }
           }
           else
           {
              id.dir[0] = '\0';
              id.dir_id = 0;
           }
   }
   else
   {
      if (id.job_id != 0)
      {
         for (i = 0; i < *no_of_job_ids; i++)
         {
            if (id.job_id == jd[i].job_id)
            {
               get_job_data(&jd[i]);
               break;
            }
         }
      }
      else if (id.dir_id != 0)
           {
              for (i = 0; i < *no_of_dir_names; i++)
              {
                 if (id.dir_id == dnb[i].dir_id)
                 {
                    get_dir_data(i);
                    break;
                 }
              }
           }
   }

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

   return;
}


/*############################ get_sum_data() ###########################*/
int
get_sum_data(int item, time_t *date, double *file_size)
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
           str_hex_number[23];

      /* Go to beginning of line to read complete line. */
      if (fseek(il[file_no].fp,
                (long)(il[file_no].line_offset[pos] - (log_date_length + 1 + max_hostname_length + 3)),
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
      while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n') && (i < (log_date_length + 1)))
      {
         str_hex_number[i] = *ptr;
         ptr++; i++;
      }
      if (*ptr == SEPARATOR_CHAR)
      {
         str_hex_number[i] = '\0';
         *date = (time_t)str2timet(str_hex_number, NULL, 16);
         ptr++;
      }
      else if (i >= (log_date_length + 1))
           {
              while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
              {
                 ptr++;
              }
              if (*ptr == SEPARATOR_CHAR)
              {
                 ptr++;
              }
              *date = 0L;
           }

      /* Ignore file name. */
      ptr = &buffer[log_date_length + 1 + max_hostname_length + 3];
      while (*ptr != SEPARATOR_CHAR)
      {
         ptr++;
      }
      ptr++;

      /* Get file size. */
      i = 2;
      while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n') && (i < (log_date_length + 1)))
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
      }
      else
      {
         *file_size = 0.0;
      }
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ get_all() +++++++++++++++++++++++++++++*/
/*                                ---------                              */
/* Description: Retrieves the file name deleted, size, job ID, dir ID,   */
/*              process/user and if available the additional reasons out */
/*              of the log file.                                         */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void
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
      int          i;
      unsigned int tmp_id;
      char         *ptr,
                   buffer[4 + MAX_FILENAME_LENGTH + MAX_PATH_LENGTH],
                   str_hex_number[2 + MAX_OFF_T_HEX_LENGTH + 1];

      if (fseek(il[file_no].fp, (long)il[file_no].line_offset[pos] - 4,
                SEEK_SET) == -1)
      {
         (void)xrec(FATAL_DIALOG, "fseek() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }

      if (fgets(buffer, 4 + MAX_FILENAME_LENGTH + MAX_PATH_LENGTH,
                il[file_no].fp) == NULL)
      {
         (void)xrec(WARN_DIALOG, "fgets() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }
      ptr = buffer;
      if ((*(ptr + 3) == SEPARATOR_CHAR) && (isxdigit((int)(*ptr))) &&
          (isxdigit((int)(*(ptr + 1)))) && (isxdigit((int)(*(ptr + 2)))))
      {
         id.offset = 2;
         *(ptr + 3) = '\0';
         id.delete_reason_no = (int)strtol(ptr, NULL, 16);
      }
      else
      {
         id.offset = 0;
         id.delete_reason_no = (int)(*(ptr + 2) - '0');
      }
      ptr += 4;

      /* Get delete reason string. */
      (void)strcpy((char *)id.reason_str, drstr[id.delete_reason_no]);

      i = 0;
      while ((*ptr != SEPARATOR_CHAR) && (i < MAX_FILENAME_LENGTH))
      {
         id.file_name[i] = *ptr;
         i++; ptr++;
      }
      if (i == MAX_FILENAME_LENGTH)
      {
         id.file_name[i - 2] = ' ';
         id.file_name[i - 1] = '\0';
         id.file_size[0] = '0';
         id.file_size[1] = '\0';
         id.proc_user[0] = '\0';
         id.extra_reason[0] = '\0';
         id.dir_id = 0;
         id.job_id = 0;

         return;
      }

      id.file_name[i] = '\0';
      ptr++;

      /* Away with the file size. */
      str_hex_number[0] = '0';
      str_hex_number[1] = 'x';
      i = 2;
      while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n') &&
             (i < (MAX_OFF_T_HEX_LENGTH + 2)))
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
      else if (i >= (MAX_OFF_T_HEX_LENGTH + 2))
           {
              while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
              {
                 ptr++;
              }
              if (*ptr == SEPARATOR_CHAR)
              {
                 ptr++;
              }
              id.file_size[0] = '0';
              id.file_size[1] = '\0';
           }

      /* Get job ID. */
      i = 0;
      while ((*(ptr + i) != SEPARATOR_CHAR) && (i < MAX_INT_HEX_LENGTH) &&
             (*(ptr + i) != '\n'))
      {
         i++;
      }
      *(ptr + i) = '\0';
      tmp_id = (unsigned int)strtoul(ptr, NULL, 16);
      ptr += i;
      if (i == MAX_INT_HEX_LENGTH)
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
      else
      {
         ptr++;
      }
      if (id.offset)
      {
         id.job_id = tmp_id;
         i = 0;
         while ((*(ptr + i) != SEPARATOR_CHAR) && (i < MAX_INT_HEX_LENGTH) &&
                (*(ptr + i) != '\n'))
         {
            i++;
         }
         if (*(ptr + i) == SEPARATOR_CHAR)
         {
            *(ptr + i) = '\0';
            id.dir_id = (unsigned int)strtoul(ptr, NULL, 16);
            ptr += i + 1;
         }
         else
         {
            id.dir_id = 0;
            ptr += i;
            if (i == MAX_INT_HEX_LENGTH)
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
         }
      }
      else
      {
         if ((id.delete_reason_no == AGE_OUTPUT) ||
             (id.delete_reason_no == NO_MESSAGE_FILE_DEL) ||
             (id.delete_reason_no == DUP_OUTPUT))
         {
            id.job_id = tmp_id;
            id.dir_id = 0;
         }
         else
         {
            id.job_id = 0;
            id.dir_id = tmp_id;
         }
      }

      /* Ignore unique ID. */
      while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
      {
         ptr++;
      }
      if (*ptr == SEPARATOR_CHAR)
      {
         ptr++;
      }

      /* Get the process/user. */
      i = 0;
      while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n') &&
             (i < MAX_PROC_USER_LENGTH))
      {
         id.proc_user[i] = *ptr;
         i++; ptr++;
      }
      id.proc_user[i] = '\0';
      if (i == MAX_PROC_USER_LENGTH)
      {
         while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
         {
            ptr++;
         }
      }

      if (*ptr == SEPARATOR_CHAR)
      {
         ptr++;
         i = 0;
         while ((*ptr != '\n') && (i < MAX_PATH_LENGTH))
         {
            id.extra_reason[i] = *ptr;
            i++; ptr++;
         }
         id.extra_reason[i] = '\0';
      }
   }

   return;
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
   register char *p_tmp;

   id.count = 1;

   /* Create or increase the space for the buffer. */
   if ((id.dbe = realloc(id.dbe, sizeof(struct db_entry))) == (struct db_entry *)NULL)
   {
      (void)xrec(FATAL_DIALOG, "realloc() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }

   (void)strcpy((char *)id.dir, dnb[p_jd->dir_id_pos].dir_name);
   id.dir_id = p_jd->dir_id;
   (void)sprintf(id.dir_id_str, "%x", id.dir_id);
   get_dir_options(id.dir_id, &id.d_o);
   id.dbe[0].priority = p_jd->priority;
   get_file_mask_list(p_jd->file_mask_id, &id.dbe[0].no_of_files,
                      &id.dbe[0].files);
   if (id.dbe[0].files == NULL)
   {
      (void)xrec(WARN_DIALOG,
                 "Failed to get file mask list, see system log for more details.");
   }
   id.dbe[0].no_of_loptions = p_jd->no_of_loptions;

   /* Save all AMG (local) options. */
   if (id.dbe[0].no_of_loptions > 0)
   {
      register int i;

      p_tmp = p_jd->loptions;
      for (i = 0; i < id.dbe[0].no_of_loptions; i++)
      {
         (void)strcpy(id.dbe[0].loptions[i], p_tmp);
         NEXT(p_tmp);
      }
   }

   id.dbe[0].no_of_soptions = p_jd->no_of_soptions;

   /* Save all FD (standart) options. */
   if (id.dbe[0].no_of_soptions > 0)
   {
      int size;

      size = strlen(p_jd->soptions);
      if ((id.dbe[0].soptions = calloc(size + 1, sizeof(char))) == NULL)
      {
         (void)xrec(FATAL_DIALOG, "calloc() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }
      (void)memcpy(id.dbe[0].soptions, p_jd->soptions, size);
      id.dbe[0].soptions[size] = '\0';
   }
   else
   {
      id.dbe[0].soptions = NULL;
   }

   (void)strcpy(id.dbe[0].recipient, p_jd->recipient);

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
   (void)sprintf(id.dir_id_str, "%x", id.dir_id);
   get_dir_options(id.dir_id, &id.d_o);

   if (get_current_jid_list() == INCORRECT)
   {
      free(current_jid_list);
      no_of_current_jobs = 0;
      return;
   }

   id.count = 0;
   for (i = (*no_of_job_ids - 1); i > -1; i--)
   {
      if (jd[i].dir_id_pos == dir_pos)
      {
         for (j = 0; j < no_of_current_jobs; j++)
         {
            if (current_jid_list[j] == jd[i].job_id)
            {
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
                     free(current_jid_list);
                     no_of_current_jobs = 0;
                     return;
                  }
               }

               id.dbe[id.count].priority = jd[i].priority;
               get_file_mask_list(jd[i].file_mask_id,
                                  &id.dbe[id.count].no_of_files,
                                  &id.dbe[id.count].files);
               if (id.dbe[id.count].files != NULL)
               {
                  int k;

                  p_file = id.dbe[id.count].files;

                  /*
                   * Only show those entries that really match the current
                   * file name. For this it is necessary to filter the file
                   * names through all the filters.
                   */
                  gotcha = NO;
                  for (k = 0; k < id.dbe[id.count].no_of_files; k++)
                  {
                     if ((ret = pmatch(p_file, (char *)id.file_name, NULL)) == 0)
                     {
                        gotcha = YES;
                        break;
                     }
                     else if (ret == 1)
                          {
                             /* This file is NOT wanted. */
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
                           free(current_jid_list);
                           no_of_current_jobs = 0;
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
                     id.count++;
                  }
                  else
                  {
                     free(id.dbe[id.count].files);
                  }
               }
            } /* if (current_jid_list[j] == jd[i].job_id) */
         } /* for (j = 0; j < no_of_current_jobs; j++) */
      } /* if (jd[i].dir_id_pos == dir_pos) */
   } /* for (i = (*no_of_job_ids - 1); i > -1; i--) */

   free(current_jid_list);
   no_of_current_jobs = 0;

   return;
}
