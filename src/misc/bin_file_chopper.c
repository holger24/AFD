/*
 *  bin_file_chopper.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2022 Deutscher Wetterdienst (DWD),
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
 **   bin_file_chopper - breaks up a file containing bulletins to
 **                      one file per bulletin
 **
 ** SYNOPSIS
 **   int   bin_file_chopper(char         *bin_file,
 **                          int          *files_to_send,
 **                          off_t        *file_size,
 **                          char         *p_filter,
 **                          char         wmo_header_file_name,
 **                          time_t       creation_time,
 **                          unsigned int unique_number,
 **                          unsigned int split_job_counter,
 **                          unsigned int job_id,
 **                          unsigned int dir_id,
 **                          clock_t      clktck,
 **                          char         *full_option,
 **                          char         *p_file_name)
 **   off_t bin_file_convert(char         *src_ptr,
 **                          off_t        total_length,
 **                          int          to_fd,
 **                          char         *file_name,
 **                          unsigned int job_id)
 **
 ** DESCRIPTION
 **   The function bin_file_chopper reads a binary WMO bulletin file,
 **   and writes each bulletin (GRIB, BUFR, BLOK) into a separate file.
 **   These files will have the following file name:
 **
 **       xxxx_yyyy_YYYYMMDDhhmmss_zzzz
 **        |    |         |         |
 **        |    |         |         +----> counter
 **        |    |         +--------------> date when created
 **        |    +------------------------> originator
 **        +-----------------------------> data type
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to read any valid data from the
 **   file. For function bin_file_chopper(), SUCCESS will be returned and
 **   the number of files that have been created and the sum of their size,
 **   will be returned. Function bin_file_convert() will return the number
 **   of bytes stored.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.08.1996 H.Kiehl Created
 **   28.05.1998 H.Kiehl Added support for GRIB edition 1, ie using
 **                      length indicator to find end.
 **   18.11.2002 H.Kiehl WMO header file names.
 **   03.10.2004 H.Kiehl Added production log.
 **   20.07.2006 H.Kiehl Added bin_file_convert().
 **   13.07.2007 H.Kiehl Added option to filter out only those bulletins
 **                      we really nead.
 **   21.01.2009 H.Kiehl Added support for GRIB edition 2.
 **   20.01.2010 H.Kiehl Added support for DWD data type.
 **   13.02.2019 H.Kiehl Show job ID in bin_file_convert() if something
 **                      goes wrong.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>              /* strlen(), strcpy()                   */
#include <time.h>                /* time(), strftime(), gmtime()         */
#include <stdlib.h>              /* malloc(), realloc(), free()          */
#include <unistd.h>              /* close(), read()                      */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>           /* mmap(), munmap()                     */
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "amgdefs.h"

#define _DIR_ORIGINATOR
#define DATA_TYPES 3
#ifdef _PRODUCTION_LOG
#define LOG_ENTRY_STEP_SIZE 10
struct prod_log_db
       {
          char           file_name[MAX_FILENAME_LENGTH + 1];
          off_t          size;
          double         production_time;
          struct timeval cpu_usage;
       };
#endif

/* Global local variables */
static int                counter_fd,
                          id_length[DATA_TYPES] = { 4, 4, 4 },
                          end_id_length[DATA_TYPES] = { 4, 4, 4 };
static char               bul_format[DATA_TYPES][5] =
                          {
                             "GRIB",
                             "BUFR",
                             "BLOK"
                          },
                          end_id[DATA_TYPES][5] =
                          {
                             "7777",
                             "7777",
                             "7777"
                          };
#ifdef _PRODUCTION_LOG
static int                no_of_log_entries = 0;
static struct prod_log_db *pld = NULL;
#endif

/* External global variables */
extern char               *p_work_dir;

/* Local functions */
static char               *bin_search_start(char *, int, int *, off_t *);
static off_t              bin_search_end(char *, char *, off_t);


/*########################## bin_file_chopper() #########################*/
int
bin_file_chopper(char         *bin_file,
                 int          *files_to_send,
                 off_t        *file_size,
                 char         *p_filter,
#ifdef _PRODUCTION_LOG
                 char         wmo_header_file_name,
                 time_t       creation_time,
                 unsigned int unique_number,
                 unsigned int split_job_counter,
                 unsigned int job_id,
                 unsigned int dir_id,
                 clock_t      clktck,
                 char         *full_option,
                 char         *p_file_name)
#else
                 char         wmo_header_file_name)
#endif
{
   int           i,
                 fd,
                 first_time = YES,
                 *counter;     /* Counter to keep file name unique.        */
   off_t         data_length = 0,
                 length,
                 total_length;
   time_t        tvalue;
   mode_t        file_mode;
   char          *buffer,
                 *p_file,
                 *ptr,
                 new_file[MAX_PATH_LENGTH],
                 *p_new_file,
                 date_str[16],
                 originator[MAX_FILENAME_LENGTH];
#ifdef HAVE_STATX
   struct statx  stat_buf;
#else
   struct stat   stat_buf;
#endif
#ifdef _PRODUCTION_LOG
   clock_t       start_time;
   struct tms    tval;
   struct rusage ru;

   (void)getrusage(RUSAGE_SELF, &ru);
   start_time = times(&tval);
#endif

#ifdef HAVE_STATX
   if (statx(0, bin_file, AT_STATX_SYNC_AS_STAT,
             STATX_SIZE | STATX_MODE, &stat_buf) != 0)
#else
   if (stat(bin_file, &stat_buf) != 0)
#endif
   {
      if (errno == ENOENT)
      {
         /*
          * If the file is not there, why should we bother?
          */
         return(SUCCESS);
      }
      else
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
#ifdef HAVE_STATX
                     _("Failed to statx() `%s' : %s"),
#else
                     _("Failed to stat() `%s' : %s"),
#endif
                     bin_file, strerror(errno));
         return(INCORRECT);
      }
   }

   /*
    * If the size of the file is less then 10 forget it. There must be
    * something really wrong.
    */
#ifdef HAVE_STATX
   if (stat_buf.stx_size < 10)
#else
   if (stat_buf.st_size < 10)
#endif
   {
      return(INCORRECT);
   }
#ifdef HAVE_STATX
   file_mode = stat_buf.stx_mode;
#else
   file_mode = stat_buf.st_mode;
#endif

   if ((fd = open(bin_file, O_RDONLY)) < 0)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to open() `%s' : %s"), bin_file, strerror(errno));
      return(INCORRECT);
   }

#ifdef HAVE_MMAP
   if ((buffer = mmap(NULL,
# ifdef HAVE_STATX
                      stat_buf.stx_size, PROT_READ,
# else
                      stat_buf.st_size, PROT_READ,
# endif
                      MAP_SHARED, fd, 0)) == (caddr_t) -1)
#else
   if ((buffer = mmap_emu(NULL,
# ifdef HAVE_STATX
                          stat_buf.stx_size, PROT_READ,
# else
                          stat_buf.st_size, PROT_READ,
# endif
                          MAP_SHARED, bin_file, 0)) == (caddr_t) -1)
#endif
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("mmap() error : %s"), strerror(errno));
      (void)close(fd);
      return(INCORRECT);
   }
   p_file = buffer;

#ifdef HAVE_STATX
   total_length = stat_buf.stx_size;
#else
   total_length = stat_buf.st_size;
#endif

   /* Close the file since we do not need it anymore */
   if (close(fd) == -1)
   {
      receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                  _("close() error : %s"), strerror(errno));
   }

   /* Get directory where files are to be stored. */
   i = strlen(bin_file);
   if (i > (MAX_PATH_LENGTH - 1))
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Buffer to store file name is to small (%d < %d).",
                 MAX_PATH_LENGTH, i);
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
      (void)munmap(p_file, stat_buf.stx_size);
# else
      (void)munmap(p_file, stat_buf.st_size);
# endif
#else
      (void)munmap_emu(p_file);
#endif
      return(INCORRECT);
   }
   (void)strcpy(new_file, bin_file);
   p_new_file = new_file + i;
   while ((*p_new_file != '/') && (p_new_file != new_file))
   {
      p_new_file--;
   }
   if (*p_new_file != '/')
   {
      /* Impossible */
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Cannot determine directory where to store files!"));
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
      (void)munmap(p_file, stat_buf.stx_size);
# else
      (void)munmap(p_file, stat_buf.st_size);
# endif
#else
      (void)munmap_emu(p_file);
#endif
      return(INCORRECT);
   }

   /* Extract the originator */
#ifdef _DIR_ORIGINATOR
   ptr = p_new_file - 1;
   while ((*ptr != '/') && (ptr != new_file))
   {
      ptr--;
   }
   if (*ptr != '/')
   {
      /* Impossible */
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Cannot determine directory where to store files!"));
# ifdef HAVE_MMAP
# ifdef HAVE_STATX
      (void)munmap(p_file, stat_buf.stx_size);
# else
      (void)munmap(p_file, stat_buf.st_size);
# endif
# else
      (void)munmap_emu(p_file);
# endif
      return(INCORRECT);
   }
   ptr++;
   *p_new_file = '\0';
   (void)strcpy(originator, ptr);
   *p_new_file = '/';
#else
   (void)strcpy(originator, "XXX");
#endif
   p_new_file++;

   /*
    * Open AFD counter file to create unique numbers for the new
    * file names.
    */
   if ((counter_fd = open_counter_file(COUNTER_FILE, &counter)) < 0)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to open AFD counter file."));
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
      (void)munmap(p_file, stat_buf.stx_size);
# else
      (void)munmap(p_file, stat_buf.st_size);
# endif
#else
      (void)munmap_emu(p_file);
#endif
      return(INCORRECT);
   }

   while (total_length > 9)
   {
      if ((ptr = bin_search_start(buffer, total_length, &i, &total_length)) != NULL)
      {
         unsigned long long message_length = 0;

         /*
          * When data type is GRIB and it is still using edition
          * 0 we cannot use the length indicator.
          */
         if ((i == 0) && (*(ptr + 3) == 0))
         {
            /*
             * Let's look for the end. If we don't find an end marker
             * try get the next data type. Maybe this is not a good
             * idea and it would be better to discard this file.
             * Experience will show which is the better solution.
             */
            if ((data_length = bin_search_end(end_id[i], ptr, total_length)) == 0)
            {
#ifdef _END_DIFFER
               /*
                * Since we did not find a valid end_marker, it does not
                * mean that all data in this file is incorrect. Ignore
                * this bulletin and try search for the next data type
                * identifier. Since we have no clue where this might start
                * we have to extend the search across the whole file.
                */
               buffer = ptr;
               continue;
#else
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
# ifdef _PRODUCTION_LOG
                           _("Failed to extract data from `%s'. #%x"),
                           bin_file, job_id);
# else
                           _("Failed to extract data from `%s'"), bin_file);
# endif
               close_counter_file(counter_fd, &counter);
# ifdef HAVE_MMAP
#  ifdef HAVE_STATX
               (void)munmap(p_file, stat_buf.stx_size);
#  else
               (void)munmap(p_file, stat_buf.st_size);
#  endif
# else
               (void)munmap_emu(p_file);
# endif
# ifdef _PRODUCTION_LOG
               if (pld != NULL)
               {
                  free(pld);
                  no_of_log_entries = 0;
                  pld = NULL;
               }
# endif
               return(INCORRECT);
#endif
            }
         }
              /* When GRIB it has to be at least edition 2 */
         else if ((i == 0) && (*(ptr + 3) == 2))
              {
                 /*
                  * Determine length by reading byte 8 - 15.
                  */
                 message_length = 0;
                 message_length |= (unsigned char)*(ptr + 4);
                 message_length <<= 8;
                 message_length |= (unsigned char)*(ptr + 5);
                 message_length <<= 8;
                 message_length |= (unsigned char)*(ptr + 6);
                 message_length <<= 8;
                 message_length |= (unsigned char)*(ptr + 7);
                 message_length <<= 8;
                 message_length |= (unsigned char)*(ptr + 8);
                 message_length <<= 8;
                 message_length |= (unsigned char)*(ptr + 9);
                 message_length <<= 8;
                 message_length |= (unsigned char)*(ptr + 10);
                 message_length <<= 8;
                 message_length |= (unsigned char)*(ptr + 11);

                 if (message_length > (total_length + id_length[i]))
                 {
                    if (first_time == YES)
                    {
                       receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
#ifdef _PRODUCTION_LOG
                                   _("Hey! Whats this? Message length (%llu) > then total length (%u) [%s]. #%x"),
                                   message_length, total_length + id_length[i],
                                   bin_file, job_id);
#else
                                   _("Hey! Whats this? Message length (%llu) > then total length (%u) [%s]"),
                                   message_length, total_length + id_length[i],
                                   bin_file);
#endif
                       first_time = NO;
                    }
                    buffer = ptr;
                    continue;
                 }
                 else
                 {
                    char *tmp_ptr = ptr - id_length[i] +
                                    message_length -
                                    end_id_length[i];

                    if (memcmp(tmp_ptr, end_id[i], end_id_length[i]) != 0)
                    {
                       receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
#ifdef _PRODUCTION_LOG
                                   _("Hey! Whats this? End locator not where it should be! #%x"),
                                   job_id);
#else
                                   _("Hey! Whats this? End locator not where it should be!"));
#endif
                       buffer = ptr;
                       continue;
                    }
                 }
              }
              else
              {
                 /*
                  * Determine length by reading byte 4 - 6.
                  */
                 message_length = 0;
                 message_length |= (unsigned char)*ptr;
                 message_length <<= 8;
                 message_length |= (unsigned char)*(ptr + 1);
                 message_length <<= 8;
                 message_length |= (unsigned char)*(ptr + 2);

                 if (message_length > (total_length + id_length[i]))
                 {
                    if (first_time == YES)
                    {
                       receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
#ifdef _PRODUCTION_LOG
                                   _("Hey! Whats this? Message length (%llu) > then total length (%u) [%s]. #%x"),
                                   message_length, total_length + id_length[i],
                                   bin_file, job_id);
#else
                                   _("Hey! Whats this? Message length (%llu) > then total length (%u) [%s]"),
                                   message_length, total_length + id_length[i],
                                   bin_file);
#endif
                       first_time = NO;
                    }
                    buffer = ptr;
                    continue;
                 }
                 else
                 {
                    char *tmp_ptr = ptr - id_length[i] +
                                    message_length -
                                    end_id_length[i];

                    if (memcmp(tmp_ptr, end_id[i], end_id_length[i]) != 0)
                    {
                       receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
#ifdef _PRODUCTION_LOG
                                   _("Hey! Whats this? End locator not where it should be! #%x"),
                                   job_id);
#else
                                   _("Hey! Whats this? End locator not where it should be!"));
#endif
                       buffer = ptr;
                       continue;
                    }
                 }
              }

         if (wmo_header_file_name == YES)
         {
            int  local_counter = 0;
            char *p_end;

            wmoheader_from_grib(ptr - 4, p_new_file, NULL);
            p_end = p_new_file + strlen(p_new_file);
            while (eaccess(new_file, F_OK) == 0)
            {
               (void)snprintf(p_end, MAX_PATH_LENGTH - (p_end - new_file),
                              ";%d", local_counter);
               local_counter++;
            }
         }
         else
         {
            /*
             * Create a unique file name of the following format:
             *
             *       xxxx_yyyy_YYYYMMDDhhmmss_zzzz
             *        |    |         |         |
             *        |    |         |         +----> counter
             *        |    |         +--------------> date when created
             *        |    +------------------------> originator
             *        +-----------------------------> data type
             */
            if (next_counter(counter_fd, counter, MAX_MSG_PER_SEC) < 0)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
#ifdef _PRODUCTION_LOG
                           "Failed to get the next number. #%x", job_id);
#else
                           "Failed to get the next number.");
#endif
#ifdef HAVE_MMAP
#  ifdef HAVE_STATX
               (void)munmap(p_file, stat_buf.stx_size);
#  else
               (void)munmap(p_file, stat_buf.st_size);
#  endif
#else
               (void)munmap_emu(p_file);
#endif
               close_counter_file(counter_fd, &counter);
#ifdef _PRODUCTION_LOG
               if (pld != NULL)
               {
                  free(pld);
                  no_of_log_entries = 0;
                  pld = NULL;
               }
#endif
               return(INCORRECT);
            }
            if (time(&tvalue) == -1)
            {
               receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
#ifdef _PRODUCTION_LOG
                           _("Failed to get time() : %s #%x"),
                           strerror(errno), job_id);
#else
                           _("Failed to get time() : %s"), strerror(errno));
#endif
               (void)strcpy(date_str, "YYYYMMDDhhmmss");
            }
            else
            {
               length = strftime(date_str, sizeof(date_str),
                                 "%Y%m%d%H%M%S", gmtime(&tvalue));
               date_str[length] = '\0';
            }
            (void)snprintf(p_new_file,
                           MAX_PATH_LENGTH - (p_new_file - new_file),
                           "%s_%s_%s_%x", bul_format[i],
                           originator, date_str, *counter);
         }

         if ((p_filter == NULL) || (pmatch(p_filter, p_new_file, NULL) == 0))
         {
            /*
             * Store data of each bulletin into an extra file.
             */
            if ((fd = open(new_file, (O_WRONLY | O_CREAT | O_TRUNC),
                           file_mode)) < 0)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, tvalue,
#ifdef _PRODUCTION_LOG
                           _("Failed to open() `%s' : %s #%x"),
                           new_file, strerror(errno), job_id);
#else
                           _("Failed to open() `%s' : %s"),
                           new_file, strerror(errno));
#endif
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
               (void)munmap(p_file, stat_buf.stx_size);
# else
               (void)munmap(p_file, stat_buf.st_size);
# endif
#else
               (void)munmap_emu(p_file);
#endif
               close_counter_file(counter_fd, &counter);
#ifdef _PRODUCTION_LOG
               if (pld != NULL)
               {
                  free(pld);
                  no_of_log_entries = 0;
                  pld = NULL;
               }
#endif
               return(INCORRECT);
            }

            /* Add data type and end identifier to file. */
            ptr -= id_length[i];
            if (message_length == 0)
            {
               data_length = data_length + id_length[i] + end_id_length[i];
            }
            else
            {
               data_length = message_length;
            }

#ifdef HAVE_STATX
            if (writen(fd, ptr, data_length, stat_buf.stx_blksize) != data_length)
#else
            if (writen(fd, ptr, data_length, stat_buf.st_blksize) != data_length)
#endif
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, tvalue,
                           _("writen() error : %s"), strerror(errno));
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
               (void)munmap(p_file, stat_buf.stx_size);
# else
               (void)munmap(p_file, stat_buf.st_size);
# endif
#else
               (void)munmap_emu(p_file);
#endif
               (void)close(fd);
               (void)unlink(new_file); /* End user should not get any junk! */
               close_counter_file(counter_fd, &counter);
#ifdef _PRODUCTION_LOG
               if (pld != NULL)
               {
                  free(pld);
                  no_of_log_entries = 0;
                  pld = NULL;
               }
#endif
               return(INCORRECT);
            }

            if (close(fd) == -1)
            {
               receive_log(DEBUG_SIGN, __FILE__, __LINE__, tvalue,
                           _("close() error : %s"), strerror(errno));
            }
#ifdef _PRODUCTION_LOG
            if (p_file_name != NULL)
            {
               if (no_of_log_entries == 0)
               {
                  if ((pld = malloc((LOG_ENTRY_STEP_SIZE * sizeof(struct prod_log_db)))) == NULL)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                _("malloc() error : %s"), strerror(errno));
                  }
               }
               else
               {
                  if ((no_of_log_entries % LOG_ENTRY_STEP_SIZE) == 0)
                  {
                     size_t             new_size;
                     struct prod_log_db *tmp_pld;

                     new_size = ((no_of_log_entries / LOG_ENTRY_STEP_SIZE) + 1) *
                                LOG_ENTRY_STEP_SIZE * sizeof(struct prod_log_db);
                     if ((tmp_pld = realloc(pld, new_size)) == NULL)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   _("realloc() error : %s"), strerror(errno));
                        free(pld);
                        no_of_log_entries = 0;
                     }
                     pld = tmp_pld;
                  }
               }
               if (pld != NULL)
               {
                  pld[no_of_log_entries].size = data_length;
                  pld[no_of_log_entries].production_time = (times(&tval) - start_time) / (double)clktck;
                  get_sum_cpu_usage(&ru, &pld[no_of_log_entries].cpu_usage);
                  start_time = times(&tval);
                  (void)strcpy(pld[no_of_log_entries].file_name, p_new_file);
                  no_of_log_entries++;
               }
            }
#endif

            *file_size += data_length;
            (*files_to_send)++;
         }
         else
         {
            if (message_length == 0)
            {
               data_length = data_length + id_length[i] + end_id_length[i];
            }
            else
            {
               data_length = message_length;
            }
         }
         length = data_length;
/*         length = data_length + end_id_length[i]; */
         if (data_length > total_length)
         {
            if ((data_length - total_length) > 5)
            {
               receive_log(DEBUG_SIGN, __FILE__, __LINE__, tvalue,
#ifdef _PRODUCTION_LOG
                           _("Hmmm. data_length (%d) > total_length (%u)? #%x"),
                           data_length, total_length, job_id);
#else
                           _("Hmmm. data_length (%d) > total_length (%u)?"),
                           data_length, total_length);
#endif
            }
            total_length = 0;
         }
         else
         {
/*            total_length -= data_length; */
            total_length -= (data_length - end_id_length[i]);
         }
         if (message_length != 0)
         {
            int rest;

            if ((rest = (message_length % 4)) == 0)
            {
               buffer = ptr + length;
            }
            else
            {
               buffer = ptr + length - rest;
               total_length += rest;
            }
         }
         else
         {
            buffer = ptr + length;
         }
      }
      else
      {
         /*
          * Since we did not find a valid data type identifier, lets
          * forget it.
          */
         break;
      }
   } /* while (total_length > 9) */

   /* Remove the original file */
   if (unlink(bin_file) < 0)
   {
      receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to unlink() original file `%s' : %s"),
                  bin_file, strerror(errno));
   }
   else
   {
#ifdef HAVE_STATX
      *file_size -= stat_buf.stx_size;
#else
      *file_size -= stat_buf.st_size;
#endif
      (*files_to_send)--;
   }
   close_counter_file(counter_fd, &counter);
#ifdef _PRODUCTION_LOG
   if ((pld != NULL) && (no_of_log_entries))
   {
      for (i = 0; i < no_of_log_entries; i++)
      {
         production_log(creation_time, 1, no_of_log_entries, unique_number,
                        split_job_counter, job_id, dir_id,
                        pld[i].production_time, pld[i].cpu_usage.tv_sec,
                        pld[i].cpu_usage.tv_usec,
# if SIZEOF_OFF_T == 4
                        "%s%c%lx%c%s%c%lx%c0%c%s",
# else
                        "%s%c%llx%c%s%c%llx%c0%c%s",
# endif
                        p_file_name, SEPARATOR_CHAR,
# ifdef HAVE_STATX
                        (pri_off_t)stat_buf.stx_size, SEPARATOR_CHAR,
# else
                        (pri_off_t)stat_buf.st_size, SEPARATOR_CHAR,
# endif
                        pld[i].file_name, SEPARATOR_CHAR,
                        (pri_off_t)pld[i].size, SEPARATOR_CHAR,
                        SEPARATOR_CHAR, full_option);
      }
      free(pld);
      no_of_log_entries = 0;
      pld = NULL;
   }
#endif
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
   if (munmap(p_file, stat_buf.stx_size) == -1)
# else
   if (munmap(p_file, stat_buf.st_size) == -1)
# endif
#else
   if (munmap_emu(p_file) == -1)
#endif
   {
      receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                  _("munmap() error : %s"), strerror(errno));
   }

   return(SUCCESS);
}


/*########################## bin_file_convert() #########################*/
off_t
bin_file_convert(char         *src_ptr,
                 off_t        total_length,
                 int          to_fd,
                 char         *file_name,
                 unsigned int job_id)
{
   off_t bytes_written = 0;
   char  *buffer,
         *ptr,
         length_indicator[15];

   buffer = src_ptr;
   if ((*buffer == 0) && (*(buffer + 1) == 0) &&
       (*(buffer + 2) == 0) && (*(buffer + 3) == 0))
   {
      int          byte_order = 1;
      unsigned int data_length;

      ptr = buffer + 4;
      while (total_length > 9)
      {
         if (*(char *)&byte_order == 1)
         {
            ((char *)&data_length)[0] = *(ptr + 3);
            ((char *)&data_length)[1] = *(ptr + 2);
            ((char *)&data_length)[2] = *(ptr + 1);
            ((char *)&data_length)[3] = *ptr;
         }
         else
         {
            ((char *)&data_length)[0] = *ptr;
            ((char *)&data_length)[1] = *(ptr + 1);
            ((char *)&data_length)[2] = *(ptr + 2);
            ((char *)&data_length)[3] = *(ptr + 3);
         }
         if (data_length > total_length)
         {
            receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
#if SIZEOF_OFF_T == 4
                        "In file `%s' given length %ld is larger then the rest of the file %ld. #%x",
#else
                        "In file `%s' given length %lld is larger then the rest of the file %lld. #%x",
#endif
                        file_name, (pri_off_t)data_length,
                        (pri_off_t)total_length, job_id);
            data_length = total_length;
         }
         if (data_length > 99999991)
         {
            (void)strcpy(length_indicator, "9999999900\01\015\015\012");
            receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                        "In file `%s' data length (%u) greater then what is possible in WMO header size, inserting maximum possible 99999999. #%x",
                        file_name, data_length + 4 + 4, job_id);
         }
         else
         {
            (void)snprintf(length_indicator, 15,
                           "%08u00\01\015\015\012", data_length + 4 + 4);
         }
         if (write(to_fd, length_indicator, 14) != 14)
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                        _("write() error : %s"), strerror(errno));
            return(INCORRECT);
         }
         bytes_written += 14;
         if (write(to_fd, ptr + 4, data_length) != data_length)
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                        _("write() error : %s"), strerror(errno));
            return(INCORRECT);
         }
         bytes_written += data_length;
         if (write(to_fd, "\015\015\012\03", 4) != 4)
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                        _("write() error : %s"), strerror(errno));
            return(INCORRECT);
         }
         bytes_written += 4;
         ptr += (4 + data_length + 4);
         total_length -= (4 + data_length + 4);
      }
   }
   else
   {
      int   first_time = YES,
            i;
      off_t data_length = 0,
            length;

      while (total_length > 9)
      {
         if ((ptr = bin_search_start(buffer, total_length, &i, &total_length)) != NULL)
         {
            unsigned int message_length = 0;

            /*
             * When data type is GRIB and it is still using edition
             * 0 we cannot use the length indicator.
             */
            if ((i == 0) && (*(ptr + 3) == 0))
            {
               /*
                * Let's look for the end. If we don't find an end marker
                * try get the next data type. Maybe this is not a good
                * idea and it would be better to discard this file.
                * Experience will show which is the better solution.
                */
               if ((data_length = bin_search_end(end_id[i], ptr, total_length)) == 0)
               {
#ifdef _END_DIFFER
                  /*
                   * Since we did not find a valid end_marker, it does not
                   * mean that all data in this file is incorrect. Ignore
                   * this bulletin and try search for the next data type
                   * identifier. Since we have no clue where this might start
                   * we have to extend the search across the whole file.
                   */
                  buffer = ptr;
                  continue;
#else
                  receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                              _("Failed to extract data. #%x"), job_id);
                  return(INCORRECT);
#endif
               }
            }
                 /* When GRIB it has to be at least edition 2. */
            else if ((i == 0) && (*(ptr + 3) == 2))
                 {
                    /*
                     * Determine length by reading byte 8 - 15.
                     */
                    message_length = 0;
                    message_length |= (unsigned char)*(ptr + 4);
                    message_length <<= 8;
                    message_length |= (unsigned char)*(ptr + 5);
                    message_length <<= 8;
                    message_length |= (unsigned char)*(ptr + 6);
                    message_length <<= 8;
                    message_length |= (unsigned char)*(ptr + 7);
                    message_length <<= 8;
                    message_length |= (unsigned char)*(ptr + 8);
                    message_length <<= 8;
                    message_length |= (unsigned char)*(ptr + 9);
                    message_length <<= 8;
                    message_length |= (unsigned char)*(ptr + 10);
                    message_length <<= 8;
                    message_length |= (unsigned char)*(ptr + 11);

                    if (message_length > (total_length + id_length[i]))
                    {
                       if (first_time == YES)
                       {
                          receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                                      _("Hey! Whats this? In file `%s' message length (%llu) > then total length (%u). #%x"),
                                      file_name, message_length,
                                      total_length + id_length[i], job_id);
                          first_time = NO;
                       }
                       buffer = ptr;
                       continue;
                    }
                    else
                    {
                       char *tmp_ptr = ptr - id_length[i] +
                                       message_length -
                                       end_id_length[i];

                       if (memcmp(tmp_ptr, end_id[i], end_id_length[i]) != 0)
                       {
                          receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                                      _("Hey! Whats this? In file `%s' end locator not where it should be! #%x"),
                                      file_name, job_id);
                          buffer = ptr;
                          continue;
                       }
                    }
                 }
                 else
                 {
                    /*
                     * Determine length by reading byte 4 - 6.
                     */
                    message_length = 0;
                    message_length |= (unsigned char)*ptr;
                    message_length <<= 8;
                    message_length |= (unsigned char)*(ptr + 1);
                    message_length <<= 8;
                    message_length |= (unsigned char)*(ptr + 2);

                    if (message_length > (total_length + id_length[i]))
                    {
                       if (first_time == YES)
                       {
                          receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                                      _("Hey! Whats this? In file `%s' message length (%u) > then total length (%u). #%x"),
                                      file_name, message_length,
                                      total_length + id_length[i], job_id);
                          first_time = NO;
                       }
                       buffer = ptr;
                       continue;
                    }
                    else
                    {
                       char *tmp_ptr = ptr - id_length[i] +
                                       message_length -
                                       end_id_length[i];

                       if (memcmp(tmp_ptr, end_id[i], end_id_length[i]) != 0)
                       {
                          receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                                      _("Hey! Whats this? In file `%s' end locator not where it should be! #%x"),
                                      file_name, job_id);
                          buffer = ptr;
                          continue;
                       }
                    }
                 }

            /* Add data type and end identifier to file. */
            ptr -= id_length[i];
            if (message_length == 0)
            {
               data_length = data_length + id_length[i] + end_id_length[i];
            }
            else
            {
               data_length = message_length;
            }

            if (data_length > 99999991)
            {
               (void)strcpy(length_indicator, "99999999");
               receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
#if SIZEOF_OFF_T == 4
                           "In file `%s' data length (%ld) greater then what is possible in WMO header size, inserting maximum possible 99999999. #%x",
#else
                           "In file `%s' data length (%lld) greater then what is possible in WMO header size, inserting maximum possible 99999999. #%x",
#endif
                           file_name, (pri_off_t)(data_length + 8), job_id);
            }
            else if ((data_length + 8) < 0)
                 {
                    (void)strcpy(length_indicator, "00000000");
                    receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
#if SIZEOF_OFF_T == 4
                                "In file `%s' data length (%ld) is less then 0, inserting 00000000. #%x",
#else
                                "In file `%s' data length (%lld) is less then 0, inserting 00000000. #%x",
#endif
                                file_name, (pri_off_t)(data_length + 8), job_id);
                 }
                 else
                 {
                    (void)snprintf(length_indicator, 15,
#if SIZEOF_OFF_T == 4
                                   "%08ld00", (pri_off_t)(data_length + 8));
#else
                                   "%08lld00", (pri_off_t)(data_length + 8));
#endif
                 }
            length_indicator[10] = 1;
            length_indicator[11] = 13;
            length_indicator[12] = 13;
            length_indicator[13] = 10;
            if (write(to_fd, length_indicator, 14) != 14)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           _("write() error : %s"), strerror(errno));
               return(INCORRECT);
            }
            bytes_written += 14;

            /* Write message body. */
            if (writen(to_fd, ptr, data_length, 0) != data_length)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           _("writen() error : %s"), strerror(errno));
               return(INCORRECT);
            }
            bytes_written += data_length;

            length_indicator[0] = 13;
            length_indicator[1] = 13;
            length_indicator[2] = 10;
            length_indicator[3] = 3;
            if (write(to_fd, length_indicator, 4) != 4)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           _("write() error : %s"), strerror(errno));
               return(INCORRECT);
            }
            bytes_written += 4;

            length = data_length;
/*            length = data_length + end_id_length[i]; */
            if (data_length > total_length)
            {
               if ((data_length - total_length) > 5)
               {
                  receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                              _("Hmmm. data_length (%d) > total_length (%u)? #%x"),
                              data_length, total_length, job_id);
               }
               total_length = 0;
            }
            else
            {
/*               total_length -= data_length; */
               total_length -= (data_length - end_id_length[i]);
            }
            if (message_length != 0)
            {
               int rest;

               if ((rest = (message_length % 4)) == 0)
               {
                  buffer = ptr + length;
               }
               else
               {
                  buffer = ptr + length - rest;
                  total_length += rest;
               }
            }
            else
            {
               buffer = ptr + length;
            }
         }
         else
         {
            /*
             * Since we did not find a valid data type identifier, lets
             * forget it.
             */
            break;
         }
      } /* while (total_length > 9) */
   }

   return(bytes_written);
}


/*+++++++++++++++++++++++++ bin_search_start() ++++++++++++++++++++++++++*/
static char *
bin_search_start(char  *search_text,
                 int   search_length,
                 int   *i,
                 off_t *total_length)
{
   int   hit[DATA_TYPES] = { 0, 0, 0 },
         count[DATA_TYPES] = { 0, 0, 0 },
         counter = 0;
   off_t tmp_length = *total_length;

   while (counter != search_length)
   {
      for (*i = 0; *i < DATA_TYPES; (*i)++)
      {
         if (*search_text == bul_format[*i][count[*i]])
         {
            if (++hit[*i] == 4)
            {
               (*total_length)--;
               return(++search_text);
            }
            (count[*i])++;
         }
         else
         {
            count[*i] = hit[*i] = 0;
         }
      }

      search_text++; counter++;
      (*total_length)--;
   }
   *total_length = tmp_length;

   return(NULL); /* Found nothing */
}


/*++++++++++++++++++++++++++ bin_search_end() +++++++++++++++++++++++++++*/
static off_t
bin_search_end(char *search_string, char *search_text, off_t total_length)
{
   int        hit = 0;
   static int counter;
   size_t     string_length = strlen(search_string);

   counter = 0;
   while (counter != total_length)
   {
      if (*(search_text++) == *(search_string++))
      {
         if (++hit == string_length)
         {
            counter -= (string_length - 1);
            return(counter);
         }
      }
      else
      {
         search_string -= hit + 1;
         hit = 0;
      }

      counter++;
   }

   return(0); /* Found nothing */
}
