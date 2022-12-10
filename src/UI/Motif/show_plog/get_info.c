/*
 *  get_info.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2016 - 2022 Deutscher Wetterdienst (DWD),
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
 **                     double *prod_time)
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
 **   14.09.2014 H.Kiehl Created
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
#include "show_plog.h"
#include "mafd_ctrl.h"

/* Global variables. */
unsigned int                  *current_jid_list;
int                           no_of_current_jobs;

/* External global variables. */
extern int                    file_name_length,
                              log_date_length,
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
static void                   get_job_data(struct job_id_data *);


/*############################### get_info() ############################*/
void
get_info(int item)
{
   int i;

   current_jid_list = NULL;
   no_of_current_jobs = 0;
   if ((item != GOT_DIR_ID_DIR_ONLY) && (item != GOT_JOB_ID_HOST_ONLY) &&
       (item != GOT_JOB_ID_USER_ONLY))
   {
      id.job_id = get_all(item - 1);
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
   for (i = 0; i < *no_of_job_ids; i++)
   {
      if (id.job_id == jd[i].job_id)
      {
         if (item == GOT_DIR_ID_DIR_ONLY)
         {
            (void)strcpy((char *)id.dir, dnb[jd[i].dir_id_pos].dir_name);
            (void)snprintf(id.dir_id_str, MAX_DIR_ALIAS_LENGTH + 1, "%x",
                           id.dir_id);
         }
         else if (item == GOT_JOB_ID_HOST_ONLY)
              {
                 size_t j = 0;

                 while ((j < MAX_HOSTNAME_LENGTH) && (jd[i].host_alias[j] != '\0'))
                 {
                    id.host_alias[j] = jd[i].host_alias[j];
                    j++;
                 }
                 if (j == MAX_HOSTNAME_LENGTH)
                 {
                    id.host_alias[j - 1] = SEPARATOR_CHAR;
                    id.host_alias[j] = '\0';
                 }
                 else
                 {
                    id.host_alias[j] = SEPARATOR_CHAR;
                    id.host_alias[j + 1] = '\0';
                 }
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
              else
              {
                 get_job_data(&jd[i]);
              }

         return;
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
get_sum_data(int    item,
             time_t *date,
             double *orig_file_size,
             double *new_file_size,
             double *prod_time,
             double *cpu_time)
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
      int  onefoureigth_or_greater;
      char *p_start = NULL,
           *ptr,
           buffer[MAX_FILENAME_LENGTH + MAX_FILENAME_LENGTH + MAX_PATH_LENGTH];

      /* Go to beginning of line to read complete line. */
      if (fseek(il[file_no].fp,
                (long)(il[file_no].line_offset[pos]),
                SEEK_SET) == -1)
      {
         (void)xrec(FATAL_DIALOG, "fseek() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }

      if (fgets(buffer, MAX_FILENAME_LENGTH + MAX_FILENAME_LENGTH + MAX_PATH_LENGTH, il[file_no].fp) == NULL)
      {
         (void)xrec(WARN_DIALOG, "fgets() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }

      ptr = buffer + log_date_length;
      *ptr = '\0';
      *date = (time_t)str2timet(buffer, NULL, 16);
      ptr++;

      while ((*ptr != ':') && (*ptr != '_') && (*ptr != '\n'))
      {
         ptr++;
      }
      if (*ptr == ':')
      {
         while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
         {
            ptr++;
         }
         if (*ptr == SEPARATOR_CHAR)
         {
            ptr++;
            p_start = ptr;
            while ((*ptr != '.') && (*ptr != '_') &&
                   (*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
            {
               ptr++;
            }
         }
      }
      if ((*ptr == '.') || (*ptr == SEPARATOR_CHAR))
      {
         if (*ptr == SEPARATOR_CHAR)
         {
            ptr++;
            *prod_time = 0.0;
            *cpu_time = 0.0;
         }
         else
         {
            ptr++;
            while ((*ptr != '.') && (*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
            {
               ptr++;
            }
            if ((*ptr == '.') || (*ptr == SEPARATOR_CHAR))
            {
               char tmp_char = *ptr;

               *ptr = '\0';
               *prod_time  = strtod(p_start, NULL);
               ptr++;
               if (tmp_char == '.')
               {
                  long int cpu_usec;
                  time_t   cpu_sec;

                  p_start = ptr;
                  while ((*ptr != '.') && (*ptr != SEPARATOR_CHAR) &&
                         (*ptr != '\n'))
                  {
                     ptr++;
                  }
                  if ((*ptr == '.') || (*ptr == SEPARATOR_CHAR))
                  {
                     tmp_char = *ptr;
                     *ptr = '\0';
                     cpu_sec = (time_t)str2timet(p_start, NULL, 16);
                     ptr++;
                     if (tmp_char == '.')
                     {
                        p_start = ptr;
                        while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
                        {
                           ptr++;
                        }
                        tmp_char = *ptr;
                        *ptr = '\0';
                        cpu_usec = strtol(p_start, NULL, 16);

                        if (tmp_char == SEPARATOR_CHAR)
                        {
                           ptr++;
                        }
                        else
                        {
                           *ptr = tmp_char;
                        }
                     }
                     else
                     {
                        cpu_usec = 0L;
                     }
                     *cpu_time = (double)(cpu_sec + (cpu_usec / (double)1000000));
                  }
                  else
                  {
                     *cpu_time = 0.0;
                  }
               }
               else
               {
                  *cpu_time = 0.0;
               }
            }
            else
            {
               *prod_time = 0.0;
               *cpu_time = 0.0;
            }
         }
         onefoureigth_or_greater = YES;
      }
      else
      {
         *ptr = '\0';
         *prod_time = *date - (time_t)str2timet(p_start, NULL, 16);
         onefoureigth_or_greater = NO;
      }

      /* Away with unique number + split job counter. */
      while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
      {
         ptr++;
      }
      if (*ptr == SEPARATOR_CHAR)
      {
         ptr++;
      }

      /* Away with Directory ID. */
      while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
      {
         ptr++;
      }
      if (*ptr == SEPARATOR_CHAR)
      {
         ptr++;
      }

      /* Away with Job ID. */
      while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
      {
         ptr++;
      }
      if (*ptr == SEPARATOR_CHAR)
      {
         ptr++;
      }

      /* Away with original file name. */
      while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
      {
         ptr++;
      }
      if (*ptr == SEPARATOR_CHAR)
      {
         ptr++;
      }

      if (onefoureigth_or_greater == YES)
      {
         /* Get original file size. */
         p_start = ptr;
         while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
         {
            ptr++;
         }
         if (*ptr == SEPARATOR_CHAR)
         {
            *ptr = '\0';
#ifdef HAVE_STRTOULL
            *orig_file_size = (double)strtoull(p_start, NULL, 16);
#else
            *orig_file_size = (double)strtoul(p_start, NULL, 16);
#endif
            ptr++;
         }
      }
      else
      {
         *orig_file_size = 0.0;
      }

      /* Away with new file name. */
      while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
      {
         ptr++;
      }
      if (*ptr == SEPARATOR_CHAR)
      {
         ptr++;
      }

      /* Get new file size. */
      p_start = ptr;
      while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
      {
         ptr++;
      }
      if (*ptr == SEPARATOR_CHAR)
      {
         *ptr = '\0';
#ifdef HAVE_STRTOULL
         *new_file_size = (double)strtoull(p_start, NULL, 16);
#else
         *new_file_size = (double)strtoul(p_start, NULL, 16);
#endif
         ptr++;
      }
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

   /* Get everything. */
   if (pos > -1)
   {
      int  i = 0,
           onefoureigth_or_greater;
      char buffer[MAX_FILENAME_LENGTH + MAX_PATH_LENGTH],
           tmp_char,
           *ptr;

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
      ptr = buffer + log_date_length;
      tmp_char = *ptr;
      *ptr = '\0';
      id.time_when_produced = (time_t)str2timet(buffer, NULL, 16);
      *ptr = tmp_char;

      while ((*(ptr + i) != ':') && (*(ptr + i) != '_') && (*(ptr + i) != '\n'))
      {
         i++;
      }
      if (*(ptr + i) == ':')
      {
         *(ptr + i) = '\0';
         id.ratio_1 = (int)strtoul(ptr, NULL, 16);
         ptr += i + 1;
         i = 0;
         while ((*(ptr + i) != SEPARATOR_CHAR) && (*(ptr + i) != '\n'))
         {
            i++;
         }
         if (*(ptr + i) == SEPARATOR_CHAR)
         {
            *(ptr + i) = '\0';
            id.ratio_2 = (int)strtoul(ptr, NULL, 16);
            ptr += i + 1;
            i = 0;
            while ((*(ptr + i) != '.') && (*(ptr + i) != '_') &&
                   (*(ptr + i) != SEPARATOR_CHAR) && (*(ptr + i) != '\n'))
            {
               id.unique_name[i] = *(ptr + i);
               i++;
            }
            id.unique_name[i] = '_';
         }
      }
      else
      {
         id.ratio_1 = -1;
         id.ratio_2 = -1;
      }

      if ((*(ptr + i) == '.') || (*(ptr + i) == SEPARATOR_CHAR))
      {
         if (*(ptr + i) == SEPARATOR_CHAR)
         {
            id.production_time = 0.0;
            id.cpu_time = -1.0;
            ptr += i + 1;
            i = 0;
         }
         else
         {
            i++; /* Away with '.'. */
            while ((*(ptr + i) != '.') && (*(ptr + i) != SEPARATOR_CHAR) &&
                   (*(ptr + i) != '\n'))
            {
               i++;
            }
            if ((*(ptr + i) == '.') || (*(ptr + i) == SEPARATOR_CHAR))
            {
               tmp_char = *(ptr + i);
               *(ptr + i) = '\0';
               id.production_time = strtod(ptr, NULL);
               ptr += i + 1;
               i = 0;
               if (tmp_char == '.')
               {
                  long int cpu_usec;
                  time_t   cpu_sec;

                  while ((*(ptr + i) != '.') &&
                         (*(ptr + i) != SEPARATOR_CHAR) &&
                         (*(ptr + i) != '\n'))
                  {
                     i++;
                  }
                  if ((*(ptr + i) == '.') || (*(ptr + i) == SEPARATOR_CHAR))
                  {
                     tmp_char = *(ptr + i);
                     *(ptr + i) = '\0';
                     cpu_sec = (time_t)str2timet(ptr, NULL, 16);
                     ptr += i + 1;
                     i = 0;
                     if (tmp_char == '.')
                     {
                        while ((*(ptr + i) != SEPARATOR_CHAR) &&
                               (*(ptr + i) != '\n'))
                        {
                           i++;
                        }
                        tmp_char = *(ptr + i);
                        *(ptr + i) = '\0';
                        cpu_usec = strtol(ptr, NULL, 16);
                        if (tmp_char == SEPARATOR_CHAR)
                        {
                           ptr += i + 1;
                        }
                        else
                        {
                           ptr += i;
                        }
                        i = 0;
                        id.cpu_time = (double)(cpu_sec + (cpu_usec / (double)1000000));
                     }
                     else
                     {
                        id.cpu_time = cpu_sec;
                     }
                  }
                  else
                  {
                     id.cpu_time = -1.0;
                  }
               }
               else
               {
                  id.cpu_time = -1.0;
               }
            }
            else
            {
               id.production_time = 0.0;
               id.cpu_time = -1.0;
            }
         }
         while ((*(ptr + i) != '_') && (*(ptr + i) != '\n'))
         {
            id.unique_name[i] = *(ptr + i);
            i++;
         }
         id.unique_name[i] = '_';
         onefoureigth_or_greater = YES;
      }
      else
      {
         onefoureigth_or_greater = NO;
         id.cpu_time = -1.0;
      }

      *(ptr + i) = '\0';
      id.input_time = (time_t)str2timet(ptr, NULL, 16);

      if (onefoureigth_or_greater == NO)
      {
         id.production_time = id.time_when_produced - id.input_time;
      }

      /* Unique number + split job counter. */
      while ((*(ptr + i) != SEPARATOR_CHAR) && (*(ptr + i) != '\n') &&
             (i < MAX_ADD_FNL))
      {
         id.unique_name[i] = *(ptr + i);
         i++;
      }
      id.unique_name[i] = '\0';
      if (i == MAX_ADD_FNL)
      {
         while ((*(ptr + i) != SEPARATOR_CHAR) && (*(ptr + i) != '\n'))
         {
            i++;
         }
      }
      if (*(ptr + i) == SEPARATOR_CHAR)
      {
         ptr += i + 1;
      }
      else
      {
         ptr += i;
      }

      /* Directory ID. */
      i = 0;
      while ((*(ptr + i) != SEPARATOR_CHAR) && (*(ptr + i) != '\n'))
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
         ptr += i;
         id.dir_id = 0;
      }

      /* Job ID */
      i = 0;
      while ((*(ptr + i) != SEPARATOR_CHAR) && (*(ptr + i) != '\n'))
      {
         i++;
      }
      if (*(ptr + i) == SEPARATOR_CHAR)
      {
         *(ptr + i) = '\0';
         id.job_id = (unsigned int)strtoul(ptr, NULL, 16);
         ptr += i + 1;
      }
      else
      {
         ptr += i;
         id.job_id = 0;
      }

      /* Original file name. */
      i = 0;
      while ((*(ptr + i) != SEPARATOR_CHAR) && (*(ptr + i) != '\n') &&
             (i < MAX_FILENAME_LENGTH))
      {
         id.original_filename[i] = *(ptr + i);
         i++;
      }
      if (i == MAX_FILENAME_LENGTH)
      {
         id.original_filename[i - 1] = '\0';
         while ((*(ptr + i) != SEPARATOR_CHAR) && (*(ptr + i) != '\n'))
         {
            i++;
         }
      }
      else
      {
         id.original_filename[i] = '\0';
      }
      if (*(ptr + i) == SEPARATOR_CHAR)
      {
         ptr += i + 1;
      }
      else
      {
         ptr += i;
      }

      if (onefoureigth_or_greater == YES)
      {
         /* Original file size. */
         i = 0;
         while ((*(ptr + i) != SEPARATOR_CHAR) && (*(ptr + i) != '\n'))
         {
            i++;
         }
         if (*(ptr + i) == SEPARATOR_CHAR)
         {
            *(ptr + i) = '\0';
            id.orig_file_size = (off_t)str2offt(ptr, NULL, 16);
            ptr += i + 1;
         }
         else
         {
            ptr += i;
         }
      }

      /* New file name. */
      i = 0;
      while ((*(ptr + i) != SEPARATOR_CHAR) && (*(ptr + i) != '\n') &&
             (i < MAX_FILENAME_LENGTH))
      {
         id.new_filename[i] = *(ptr + i);
         i++;
      }
      if (i == MAX_FILENAME_LENGTH)
      {
         id.new_filename[i - 1] = '\0';
         while ((*(ptr + i) != SEPARATOR_CHAR) && (*(ptr + i) != '\n'))
         {
            i++;
         }
      }
      else
      {
         id.new_filename[i] = '\0';
      }
      if (*(ptr + i) == SEPARATOR_CHAR)
      {
         ptr += i + 1;
      }
      else
      {
         ptr += i;
      }

      /* New file size. */
      i = 0;
      while ((*(ptr + i) != SEPARATOR_CHAR) && (*(ptr + i) != '\n'))
      {
         i++;
      }
      if (*(ptr + i) == SEPARATOR_CHAR)
      {
         if (i == 0)
         {
            id.new_file_size = -1;
         }
         else
         {
            *(ptr + i) = '\0';
            id.new_file_size = (off_t)str2offt(ptr, NULL, 16);
         }
         ptr += i + 1;
      }
      else
      {
         ptr += i;
      }

      /* Store return code. */
      i = 0;
      while ((*(ptr + i) != SEPARATOR_CHAR) && (*(ptr + i) != '\n'))
      {
         i++;
      }
      if (*(ptr + i) == SEPARATOR_CHAR)
      {
         *(ptr + i) = '\0';
         id.return_code = atoi(ptr);
         ptr += i + 1;
      }
      else
      {
         ptr += i;
      }

      /* Store command executed. */
      i = 0;
      while ((*(ptr + i) != '\n') && (i < MAX_OPTION_LENGTH))
      {
         id.command[i] = *(ptr + i);
         i++;
      }
      if (i == MAX_OPTION_LENGTH)
      {
         id.command[i - 1] = '\0';
         while (*(ptr + i) != '\n')
         {
            i++;
         }
      }
      else
      {
         id.command[i] = '\0';
      }
      if (*(ptr + i) == SEPARATOR_CHAR)
      {
         ptr += i + 1;
      }
      else
      {
         ptr += i;
      }

      return(id.job_id);
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
