/*
 *  extract.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2024 Deutscher Wetterdienst (DWD),
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
 **   extract - breaks up a file containing bulletins to one file per
 **             bulletin
 **
 ** SYNOPSIS
 **   int extract(char         *file_name,
 **               char         *dest_dir,
 **               char         *p_filter,
 **               time_t       creation_time,
 **               unsigned int unique_number,
 **               unsigned int split_job_counter,
 **               unsigned int job_id,
 **               unsigned int dir_id,
 **               char         *full_option,
 **               int          type,
 **               int          options,
 **               int          *files_to_send,
 **               off_t        *file_size)
 **
 ** DESCRIPTION
 **   The function extract reads a WMO bulletin file, and writes each
 **   bulletin into a separate file. The bulletin must have the following
 **   format:
 ** 
 **       <length indicator><SOH><CR><CR><LF>nnn<CR><CR><LF>
 **       WMO header<CR><CR><LF>WMO message<CR><CR><LF><ETX>
 **
 **   Four length indicators with the following length are currently
 **   recognised by extract():
 **
 **                   2 Byte - Vax standard
 **                   4 Byte - Low byte first
 **                   4 Byte - High byte first
 **                   4 Byte - MSS standard
 **                   8 Byte - WMO standard (plus 2 Bytes type indicator)
 **
 **   It is also possible without a length indicator, it must then
 **   have the following format:
 **
 **        <SOH><CR><CR><LF>nnn<CR><CR><LF>
 **        WMO header<CR><CR><LF>WMO message<CR><CR><LF><ETX>
 **
 **   Or a ZCZC at the beginning and an NNNN at the end as shown in
 **   the following format:
 **
 **        <CR><CR><LF>ZCZC<WMO message>NNNN
 **
 **   The file name of the new file will be the WMO header: 
 **
 **       TTAAii_CCCC_YYGGgg[_BBB]
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to read any valid data from the
 **   file. On success SUCCESS will be returned and the number of files
 **   that have been created and the sum of their size.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   15.09.1997 H.Kiehl Created
 **   05.04.1999 H.Kiehl Added WMO standard.
 **   03.10.2004 H.Kiehl Added production log.
 **   16.08.2005 H.Kiehl Added ASCII option.
 **   13.07.2007 H.Kiehl Added option to filter out only those bulletins
 **                      we really nead.
 **   08.04.2008 H.Kiehl Added ZCZC_NNNN option.
 **   07.12.2008 H.Kiehl Added option to extract reports.
 **   15.02.2011 H.Kiehl Added option to add additional header in report
 **                      indicating the report type.
 **   15.02.2013 H.Kiehl Added function wmo_standard_chk() that works
 **                      as wmo_standard() only that it checks if the
 **                      terminating ETX is available and if not tries
 **                      to search for it's end.
 **   06.12.2017 H.Kiehl Add function separator_char().
 **   14.12.2024 H.Kiehl Show job_id in most sub functions, not just in
 **                      top level extract() function.
 **
 */
DESCR__E_M3

#include <stdio.h>                       /* sprintf()                    */
#include <stdlib.h>                      /* strtoul(), malloc(), free()  */
#include <string.h>                      /* strerror()                   */
#include <unistd.h>                      /* close()                      */
#include <ctype.h>                       /* isdigit(), isupper()         */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>                   /* mmap(), munmap()             */
#endif
#include <time.h>
#include <sys/time.h>                    /* struct tm                    */
#include <fcntl.h>                       /* open()                       */
#include <errno.h>
#include "amgdefs.h"
#include "version.h"

#ifndef MAP_FILE    /* Required for BSD          */
#define MAP_FILE 0  /* All others do not need it */
#endif

#ifdef _PRODUCTION_LOG
#define LOG_ENTRY_STEP_SIZE 25
struct prod_log_db
       {
          char           file_name[MAX_FILENAME_LENGTH + 1];
          off_t          size;
          double         production_time;
          struct timeval cpu_usage;
       };
#endif

/* External global variables. */
extern int                        no_of_bc_entries,
                                  no_of_rc_entries,
                                  receive_log_fd;
#ifdef HAVE_HW_CRC32
extern int                        have_hw_crc32;
#endif
#ifdef _PRODUCTION_LOG
extern clock_t                    clktck;
#endif
extern struct wmo_bul_list        *bcdb; /* Bulletin Configuration Database */
extern struct wmo_rep_list        *rcdb; /* Bulletin Configuration Database */
extern struct fileretrieve_status *p_fra;

/* Local global variables. */
static int                        *counter,
                                  counter_fd,
                                  extract_options,
                                  *p_files_to_send;
static mode_t                     file_mode;
static off_t                      *p_file_size;
static char                       *extract_p_filter,
                                  *p_full_file_name,
                                  *p_file_name,
                                  *p_orig_name;
#ifdef _PRODUCTION_LOG
static struct rusage              ru;
static clock_t                    start_time;
static struct tms                 tval;
static int                        no_of_log_entries = 0;
static struct prod_log_db         *pld = NULL;
#endif

/* Local function prototypes. */
static void                       ascii_sohetx(char *, off_t, time_t, char *,
                                               unsigned int),
                                  ascii_zczc_nnnn(char *, off_t, time_t,
                                                  char *, unsigned int),
                                  binary_sohetx(char *, off_t, time_t, char *,
                                                unsigned int),
                                  four_byte(char *, off_t, time_t,
                                            unsigned int),
                                  four_byte_swap(char *, off_t, time_t,
                                                 unsigned int),
                                  four_byte_mss(char *, off_t, time_t,
                                                unsigned int),
                                  four_byte_mss_swap(char *, off_t, time_t,
                                                     unsigned int),
                                  hex_print(char *, char *, int),
                                  separator_char(char *, off_t, time_t, char *,
                                                 char, unsigned int),
                                  show_unknown_report(char *, int, char *,
                                                      unsigned int, char *,
                                                      int),
                                  two_byte_vax(char *, off_t, time_t,
                                               unsigned int),
                                  two_byte_vax_swap(char *, off_t, time_t,
                                                    unsigned int),
                                  wmo_standard(char *, off_t, time_t,
                                               unsigned int),
                                  wmo_standard_chk(char *, off_t, time_t,
                                                   unsigned int);
static int                        check_report(char *, unsigned int, int *),
                                  find_offset(int, char *, int, int *, char *,
                                              unsigned int),
                                  get_rcdb_position(char *, int),
                                  get_station_id(char *, int, int *, char *,
                                                 int, int, int, char **,
                                                 int  *, int *, char *),
                                  write_file(char *, unsigned int, time_t,
                                             int, unsigned int);

/* Local definitions. */
#define MAX_WMO_HEADER_LENGTH  25
#define MAX_REPORT_LINE_LENGTH 80
#define NIL_MESSAGE            -2
#define TEXT_MESSAGE           -3


/*############################### extract() #############################*/
int
extract(char         *file_name,
        char         *dest_dir,
        char         *p_filter,
#ifdef _PRODUCTION_LOG
        time_t       creation_time,
        unsigned int unique_number,
        unsigned int split_job_counter,
        unsigned int job_id,
        unsigned int dir_id,
        char         *full_option,
#endif
        int          type,
        int          options,
        int          *files_to_send,
        off_t        *file_size)
{
   int           byte_order = 1,
                 from_fd;
   char          *src_ptr,
                 fullname[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx  stat_buf;
#else
   struct stat   stat_buf;
#endif

#ifdef _PRODUCTION_LOG
   (void)getrusage(RUSAGE_SELF, &ru);
   start_time = times(&tval);
#endif
   (void)snprintf(fullname, MAX_PATH_LENGTH, "%s/%s", dest_dir, file_name);
   if ((from_fd = open(fullname, O_RDONLY)) == -1)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
#ifdef _PRODUCTION_LOG
                  _("Could not open() `%s' for extracting : %s #%x"),
                  fullname, strerror(errno), job_id);
#else
                  _("Could not open() `%s' for extracting : %s"),
                  fullname, strerror(errno));
#endif
      return(INCORRECT);
   }

   /* Need size and mode of input file. */
#ifdef HAVE_STATX
   if (statx(from_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE | STATX_MTIME | STATX_MODE, &stat_buf) == -1)
#else
   if (fstat(from_fd, &stat_buf) == -1)
#endif
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
#ifdef HAVE_STATX
                  _("statx() error : %s"),
#else
                  _("fstat() error : %s"),
#endif
                  strerror(errno));
      (void)close(from_fd);
      return(INCORRECT);
   }

   /*
    * If the size of the file is less then 10 forget it. There can't
    * be a WMO bulletin in it.
    */
#ifdef HAVE_STATX
   if (stat_buf.stx_size < 10)
#else
   if (stat_buf.st_size < 10)
#endif
   {
      receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
#ifdef _PRODUCTION_LOG
                  _("Got a file for extracting that is %ld bytes long! #%x"),
# ifdef HAVE_STATX
                  stat_buf.stx_size,
# else
                  stat_buf.st_size,
# endif
                  job_id);
#else
                  _("Got a file for extracting that is %ld bytes long!"),
# ifdef HAVE_STATX
                  stat_buf.stx_size);
# else
                  stat_buf.st_size);
# endif
#endif
      (void)close(from_fd);
      return(INCORRECT);
   }

   /*
    * When creating new files take the permissions from the original
    * files. This gives the originator the possibility to set the
    * permissions on the destination site.
    */
#ifdef HAVE_STATX
   file_mode = stat_buf.stx_mode;
#else
   file_mode = stat_buf.st_mode;
#endif

#ifdef HAVE_MMAP
   if ((src_ptr = mmap(NULL,
# ifdef HAVE_STATX
                       stat_buf.stx_size, PROT_READ, (MAP_FILE | MAP_SHARED),
# else
                       stat_buf.st_size, PROT_READ, (MAP_FILE | MAP_SHARED),
# endif
                       from_fd, 0)) == (caddr_t) -1)
#else
   if ((src_ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                           stat_buf.stx_size, PROT_READ, (MAP_FILE | MAP_SHARED),
# else
                           stat_buf.st_size, PROT_READ, (MAP_FILE | MAP_SHARED),
# endif
                           fullname, 0)) == (caddr_t) -1)
#endif
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  _("mmap() error : %s"), strerror(errno));
      (void)close(from_fd);
      return(INCORRECT);
   }

   /* Remove the file now, since it can happen that when we create       */
   /* a new file with exactly the same name, ie. overwrite the original  */
   /* file, we may not do it because we just have permission to read it. */
   if (unlink(fullname) < 0)
   {
      receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to unlink() `%s' : %s"), fullname, strerror(errno));
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

   if (options & EXTRACT_ADD_UNIQUE_NUMBER)
   {
      /* Open counter file. */
      if ((counter_fd = open_counter_file(COUNTER_FILE, &counter)) == -1)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to open counter file!"));
         (void)close(from_fd);
         return(INCORRECT);
      }
   }
   extract_options = options;
   extract_p_filter = p_filter;

   /*
    * Prepare file_name pointer for function write_file(), so we
    * only one simple copy to have the full file name for the
    * new file. Same goes for the file_size and files_to_send.
    * Just to lazy to handle these values right down to write_file().
    */
   p_full_file_name = dest_dir;
   p_file_name = dest_dir + strlen(dest_dir);
   *(p_file_name++) = '/';
   *p_file_name = '\0';
   p_file_size = file_size;
   p_files_to_send = files_to_send;
   p_orig_name = file_name;

   switch (type)
   {
      case ASCII_STANDARD: /* No length indicator, just locate SOH + ETX. */
         ascii_sohetx(src_ptr,
#ifdef HAVE_STATX
                      stat_buf.stx_size, stat_buf.stx_mtime.tv_sec,
#else
                      stat_buf.st_size, stat_buf.st_mtime,
#endif
                      file_name, job_id);
         break;

      case BINARY_STANDARD: /* No length indicator, just cut away header. */
         binary_sohetx(src_ptr,
#ifdef HAVE_STATX
                       stat_buf.stx_size, stat_buf.stx_mtime.tv_sec,
#else
                       stat_buf.st_size, stat_buf.st_mtime,
#endif
                       file_name, job_id);
         break;

      case ZCZC_NNNN: /* No length indicator, just locate ZCZC + NNNN. */
         ascii_zczc_nnnn(src_ptr,
#ifdef HAVE_STATX
                         stat_buf.stx_size, stat_buf.stx_mtime.tv_sec,
#else
                         stat_buf.st_size, stat_buf.st_mtime,
#endif
                         file_name, job_id);
         break;

      case TWO_BYTE      : /* Vax Standard */
         if (*(char *)&byte_order == 1)
         {
            /* little-endian */
            two_byte_vax(src_ptr,
#ifdef HAVE_STATX
                         stat_buf.stx_size, stat_buf.stx_mtime.tv_sec,
#else
                         stat_buf.st_size, stat_buf.st_mtime,
#endif
                         job_id);
         }
         else
         {
            /* big-endian */
            two_byte_vax_swap(src_ptr,
#ifdef HAVE_STATX
                              stat_buf.stx_size, stat_buf.stx_mtime.tv_sec,
#else
                              stat_buf.st_size, stat_buf.st_mtime,
#endif
                              job_id);
         }
         break;

      case FOUR_BYTE_LBF : /* Low byte first */
         if (*(char *)&byte_order == 1)
         {
            /* little-endian */
            four_byte(src_ptr,
#ifdef HAVE_STATX
                      stat_buf.stx_size, stat_buf.stx_mtime.tv_sec,
#else
                      stat_buf.st_size, stat_buf.st_mtime,
#endif
                      job_id);
         }
         else
         {
            /* big-endian */
            four_byte_swap(src_ptr,
#ifdef HAVE_STATX
                           stat_buf.stx_size, stat_buf.stx_mtime.tv_sec,
#else
                           stat_buf.st_size, stat_buf.st_mtime,
#endif
                           job_id);
         }
         break;

      case FOUR_BYTE_HBF : /* High byte first */
         if (*(char *)&byte_order == 1)
         {
            /* little-endian */
            four_byte_swap(src_ptr,
#ifdef HAVE_STATX
                           stat_buf.stx_size, stat_buf.stx_mtime.tv_sec,
#else
                           stat_buf.st_size, stat_buf.st_mtime,
#endif
                           job_id);
         }
         else
         {
            /* big-endian */
            four_byte(src_ptr,
#ifdef HAVE_STATX
                      stat_buf.stx_size, stat_buf.stx_mtime.tv_sec,
#else
                      stat_buf.st_size, stat_buf.st_mtime,
#endif
                      job_id);
         }
         break;

      case FOUR_BYTE_MSS : /* MSS Standard */
         if (*(char *)&byte_order == 1)
         {
            /* little-endian */
            four_byte_mss_swap(src_ptr,
#ifdef HAVE_STATX
                               stat_buf.stx_size, stat_buf.stx_mtime.tv_sec,
#else
                               stat_buf.st_size, stat_buf.st_mtime,
#endif
                               job_id);
         }
         else
         {
            /* big-endian */
            four_byte_mss(src_ptr,
#ifdef HAVE_STATX
                          stat_buf.stx_size, stat_buf.stx_mtime.tv_sec,
#else
                          stat_buf.st_size, stat_buf.st_mtime,
#endif
                          job_id);
         }
         break;

      case WMO_STANDARD  : /* WMO Standard */
         wmo_standard(src_ptr,
#ifdef HAVE_STATX
                      stat_buf.stx_size, stat_buf.stx_mtime.tv_sec,
#else
                      stat_buf.st_size, stat_buf.st_mtime,
#endif
                      job_id);
         break;

      case WMO_STANDARD_CHK : /* WMO Standard with end search. */
         wmo_standard_chk(src_ptr,
#ifdef HAVE_STATX
                          stat_buf.stx_size, stat_buf.stx_mtime.tv_sec,
#else
                          stat_buf.st_size, stat_buf.st_mtime,
#endif
                          job_id);
         break;

      case SP_CHAR : /* No length indicator, just locate separator */
                     /* character at end.                          */
         separator_char(src_ptr,
#ifdef HAVE_STATX
                        stat_buf.stx_size, stat_buf.stx_mtime.tv_sec,
#else
                        stat_buf.st_size, stat_buf.st_mtime,
#endif
                        file_name, '=', job_id);
         break;

      default            : /* Impossible! */
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
#ifdef _PRODUCTION_LOG
                     _("Unknown length type (%d) for extracting bulletins. #%x"),
                     type, job_id);
#else
                     _("Unknown length type (%d) for extracting bulletins."),
                     type);
#endif
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
         (void)munmap((void *)src_ptr, stat_buf.stx_size);
# else
         (void)munmap((void *)src_ptr, stat_buf.st_size);
# endif
#else
         (void)munmap_emu((void *)src_ptr);
#endif
         (void)close(from_fd);
         if (options & EXTRACT_ADD_UNIQUE_NUMBER)
         {
            close_counter_file(counter_fd, &counter);
         }
         *(p_file_name - 1) = '\0';
         return(INCORRECT);
   }

#ifdef HAVE_MMAP
# ifdef HAVE_STATX
   if (munmap((void *)src_ptr, stat_buf.stx_size) == -1)
# else
   if (munmap((void *)src_ptr, stat_buf.st_size) == -1)
# endif
#else
   if (munmap_emu((void *)src_ptr) == -1)
#endif
   {
      receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to munmap() `%s' : %s"), fullname, strerror(errno));
   }

   if (options & EXTRACT_ADD_UNIQUE_NUMBER)
   {
      close_counter_file(counter_fd, &counter);
   }
   if (close(from_fd) == -1)
   {
      receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                  _("close() error : %s"), strerror(errno));
   }
   *(p_file_name - 1) = '\0';

#ifdef _PRODUCTION_LOG
   if ((pld != NULL) && (no_of_log_entries))
   {
      for (byte_order = 0; byte_order < no_of_log_entries; byte_order++)
      {
         production_log(creation_time, 1, no_of_log_entries, unique_number,
                        split_job_counter, job_id, dir_id,
                        pld[byte_order].production_time,
                        pld[byte_order].cpu_usage.tv_sec,
                        pld[byte_order].cpu_usage.tv_usec,
# if SIZEOF_OFF_T == 4
                        "%s%c%lx%c%s%c%lx%c0%c%s",
# else
                        "%s%c%llx%c%s%c%llx%c0%c%s",
# endif
                        p_orig_name, SEPARATOR_CHAR,
# ifdef HAVE_STATX
                        (pri_off_t)stat_buf.stx_size, SEPARATOR_CHAR,
# else
                        (pri_off_t)stat_buf.st_size, SEPARATOR_CHAR,
# endif
                        pld[byte_order].file_name, SEPARATOR_CHAR,
                        (pri_off_t)pld[byte_order].size, SEPARATOR_CHAR,
                        SEPARATOR_CHAR, full_option);
      }
      free(pld);
      no_of_log_entries = 0;
      pld = NULL;
   }
#endif

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++++ ascii_sohetx() ++++++++++++++++++++++++++*/
static void
ascii_sohetx(char         *src_ptr,
             off_t        total_length,
             time_t       mtime,
             char         *file_name,
             unsigned int job_id)
{
   if (*src_ptr == 1)
   {
      char *ptr = src_ptr,
           *ptr_start;

      do
      {
         while ((*ptr != 1) && ((ptr - src_ptr) <= total_length))
         {
            ptr++;
         }
         if (*ptr == 1)
         {
            ptr_start = ptr;
            while ((*ptr != 3) && ((ptr - src_ptr) <= total_length))
            {
               ptr++;
            }
            if (*ptr == 3)
            {
               ptr++;
               if (write_file(ptr_start, (unsigned int)(ptr - ptr_start),
                              mtime, YES, job_id) < 0)
               {
                  return;
               }
               if ((ptr - src_ptr) >= total_length)
               {
                  return;
               }
            }
            else
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           _("Failed to locate terminating ETX in %s. #%x"),
                           file_name, job_id);
               return;
            }
         }
      } while ((ptr - src_ptr) <= total_length);
   }
   else
   {
      (void)write_file(src_ptr, (unsigned int)(total_length), mtime,
                       NO, job_id);
   }

   return;
}


/*++++++++++++++++++++++++++++ binary_sohetx() ++++++++++++++++++++++++++*/
static void
binary_sohetx(char         *src_ptr,
              off_t        total_length,
              time_t       mtime,
              char         *file_name,
              unsigned int job_id)
{
   char *ptr = src_ptr,
        *ptr_start;

   while ((*ptr != 1) && ((ptr - src_ptr) <= total_length))
   {
      ptr++;
   }
   if (*ptr == 1)
   {
      ptr_start = ptr;
      ptr += (total_length - 1);
      if (*ptr == 3)
      {
         ptr++;
         if (write_file(ptr_start, (unsigned int)(ptr - ptr_start), mtime,
                        YES, job_id) < 0)
         {
            return;
         }
         if ((ptr - src_ptr) >= total_length)
         {
            return;
         }
      }
      else
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to locate terminating ETX in %s. #%x"),
                     file_name, job_id);
         return;
      }
   }

   return;
}


/*+++++++++++++++++++++++++++ ascii_zczc_nnnn() +++++++++++++++++++++++++*/
static void
ascii_zczc_nnnn(char         *src_ptr,
                off_t        total_length,
                time_t       mtime,
                char         *file_name,
                unsigned int job_id)
{
   char *ptr = src_ptr,
        *ptr_start;

   do
   {
      ptr_start = ptr;
      while (((*ptr == 13) || (*ptr == 10)) &&
             ((ptr - src_ptr) <= total_length))
      {
         ptr++;
      }
      if (((ptr - src_ptr + 4) <= total_length) &&
          (*ptr == 'Z') && (*(ptr + 1) == 'C') && (*(ptr + 2) == 'Z') &&
          (*(ptr + 3) == 'C'))
      {
         ptr += 4;
         while (((ptr - src_ptr + 5) <= total_length) &&
                (!(((*ptr == 13) || (*ptr == 10)) &&
                   (*(ptr + 1) == 'N') && (*(ptr + 2) == 'N') &&
                   (*(ptr + 3) == 'N') && (*(ptr + 4) == 'N'))))
         {
            ptr++;
         }
         if (((*ptr == 13) || (*ptr == 10)) &&
             (*(ptr + 1) == 'N') && (*(ptr + 2) == 'N') &&
             (*(ptr + 3) == 'N') && (*(ptr + 4) == 'N'))
         {
            ptr += 5;
            if (write_file(ptr_start, (unsigned int)(ptr - ptr_start), mtime,
                           NEITHER, job_id) < 0)
            {
               return;
            }
            if ((ptr - src_ptr) >= total_length)
            {
               return;
            }
         }
         else
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                        _("Failed to locate terminating NNNN in %s. #%x"),
                        file_name, job_id);
            return;
         }
      }
   } while ((ptr - src_ptr) <= total_length);

   return;
}


/*+++++++++++++++++++++++++++++ two_byte_vax() ++++++++++++++++++++++++++*/
static void
two_byte_vax(char         *src_ptr,
             off_t        total_length,
             time_t       mtime,
             unsigned int job_id)
{
   unsigned short length;
   char           *ptr = src_ptr;

   ((char *)&length)[0] = *ptr;
   ((char *)&length)[1] = *(ptr + 1);
   if (((ptr + length) - src_ptr) <= total_length)
   {
      for (;;)
      {
         if (write_file(ptr + 2, (unsigned int)length, mtime, YES, job_id) < 0)
         {
            return;
         }
         ptr += length + 3;
         if ((ptr - src_ptr) >= total_length)
         {
            return;
         }
         ((char *)&length)[0] = *ptr;
         ((char *)&length)[1] = *(ptr + 1);
      }
   }

   return;
}


/*++++++++++++++++++++++++++ two_byte_vax_swap() ++++++++++++++++++++++++*/
static void
two_byte_vax_swap(char         *src_ptr,
                  off_t        total_length,
                  time_t       mtime,
                  unsigned int job_id)
{
   unsigned short length;
   char           *ptr = src_ptr;

   ((char *)&length)[0] = *(ptr + 1);
   ((char *)&length)[1] = *ptr;
   if (((ptr + length) - src_ptr) <= total_length)
   {
      for (;;)
      {
         if (write_file(ptr + 2, (unsigned int)length, mtime, YES, job_id) < 0)
         {
            return;
         }
         ptr += length + 3;
         if ((ptr - src_ptr) >= total_length)
         {
            return;
         }
         ((char *)&length)[0] = *(ptr + 1);
         ((char *)&length)[1] = *ptr;
      }
   }

   return;
}


/*++++++++++++++++++++++++++++++ four_byte() ++++++++++++++++++++++++++++*/
static void
four_byte(char         *src_ptr,
          off_t        total_length,
          time_t       mtime,
          unsigned int job_id)
{
   unsigned int length;
   char         *ptr = src_ptr;

   ((char *)&length)[0] = *ptr;
   ((char *)&length)[1] = *(ptr + 1);
   ((char *)&length)[2] = *(ptr + 2);
   ((char *)&length)[3] = *(ptr + 3);
   if (((ptr + length) - src_ptr) <= total_length)
   {
      for (;;)
      {
         if (write_file(ptr + 4, length, mtime, YES, job_id) < 0)
         {
            return;
         }
         ptr += length + 4;
         if ((ptr - src_ptr) >= total_length)
         {
            return;
         }
         ((char *)&length)[0] = *ptr;
         ((char *)&length)[1] = *(ptr + 1);
         ((char *)&length)[2] = *(ptr + 2);
         ((char *)&length)[3] = *(ptr + 3);
      }
   }

   return;
}


/*+++++++++++++++++++++++++++ four_byte_swap() ++++++++++++++++++++++++++*/
static void
four_byte_swap(char         *src_ptr,
               off_t        total_length,
               time_t       mtime,
               unsigned int job_id)
{
   unsigned int length;
   char         *ptr = src_ptr;

   ((char *)&length)[0] = *(ptr + 3);
   ((char *)&length)[1] = *(ptr + 2);
   ((char *)&length)[2] = *(ptr + 1);
   ((char *)&length)[3] = *ptr;
   if (((ptr + length) - src_ptr) <= total_length)
   {
      for (;;)
      {
         if (write_file(ptr + 4, length, mtime, YES, job_id) < 0)
         {
            return;
         }
         ptr += length + 4;
         if ((ptr - src_ptr) >= total_length)
         {
            return;
         }
         ((char *)&length)[0] = *(ptr + 3);
         ((char *)&length)[1] = *(ptr + 2);
         ((char *)&length)[2] = *(ptr + 1);
         ((char *)&length)[3] = *ptr;
      }
   }

   return;
}


/*++++++++++++++++++++++++++++ four_byte_mss() ++++++++++++++++++++++++++*/
static void
four_byte_mss(char         *src_ptr,
              off_t        total_length,
              time_t       mtime,
              unsigned int job_id)
{
   unsigned int length;
   char         *ptr = src_ptr;

   ((char *)&length)[0] = 0;
   ((char *)&length)[1] = *(ptr + 1);
   ((char *)&length)[2] = *(ptr + 2);
   ((char *)&length)[3] = *(ptr + 3);
   if (((ptr + length) - src_ptr) <= total_length)
   {
      for (;;)
      {
         if (write_file(ptr + 4, length, mtime, YES, job_id) < 0)
         {
            return;
         }
         ptr += length + 4;
         if ((ptr - src_ptr) >= total_length)
         {
            return;
         }
         ((char *)&length)[0] = 0;
         ((char *)&length)[1] = *(ptr + 1);
         ((char *)&length)[2] = *(ptr + 2);
         ((char *)&length)[3] = *(ptr + 3);
      }
   }
   return;
}


/*+++++++++++++++++++++++++ four_byte_mss_swap() ++++++++++++++++++++++++*/
static void
four_byte_mss_swap(char         *src_ptr,
                   off_t        total_length,
                   time_t       mtime,
                   unsigned int job_id)
{
   unsigned int length;
   char         *ptr = src_ptr;

   ((char *)&length)[0] = *(ptr + 3);
   ((char *)&length)[1] = *(ptr + 2);
   ((char *)&length)[2] = *(ptr + 1);
   ((char *)&length)[3] = 0;
   if (((ptr + length) - src_ptr) <= total_length)
   {
      for (;;)
      {
         if (write_file(ptr + 4, length, mtime, YES, job_id) < 0)
         {
            return;
         }
         ptr += length + 4;
         if ((ptr - src_ptr) >= total_length)
         {
            return;
         }
         ((char *)&length)[0] = *(ptr + 3);
         ((char *)&length)[1] = *(ptr + 2);
         ((char *)&length)[2] = *(ptr + 1);
         ((char *)&length)[3] = 0;
      }
   }
   return;
}


/*+++++++++++++++++++++++++++ wmo_standard() ++++++++++++++++++++++++++++*/
static void
wmo_standard(char         *src_ptr,
             off_t        total_length,
             time_t       mtime,
             unsigned int job_id)
{
   int          soh_etx;
   unsigned int length;
   char         *ptr = src_ptr,
                str_length[9];

   str_length[8] = '\0';
   do
   {
      str_length[0] = *ptr;
      str_length[1] = *(ptr + 1);
      str_length[2] = *(ptr + 2);
      str_length[3] = *(ptr + 3);
      str_length[4] = *(ptr + 4);
      str_length[5] = *(ptr + 5);
      str_length[6] = *(ptr + 6);
      str_length[7] = *(ptr + 7);
      length = (unsigned int)strtoul(str_length, NULL, 10);
      if (length > 0)
      {
         if (*(ptr + 9) == '1')
         {
            soh_etx = NO;
         }
         else
         {
            soh_etx = YES;
         }
         if (write_file(ptr + 10, length, mtime, soh_etx, job_id) < 0)
         {
            return;
         }
      }
      ptr += length + 10;
   } while ((ptr - src_ptr) < total_length);

   return;
}


/*+++++++++++++++++++++++++ wmo_standard_chk() ++++++++++++++++++++++++++*/
static void
wmo_standard_chk(char         *src_ptr,
                 off_t        total_length,
                 time_t       mtime,
                 unsigned int job_id)
{
   int          soh_etx;
   unsigned int additional_length,
                length;
   char         *ptr = src_ptr,
                str_length[9];

   str_length[8] = '\0';
   do
   {
      additional_length = 0;
      str_length[0] = *ptr;
      str_length[1] = *(ptr + 1);
      str_length[2] = *(ptr + 2);
      str_length[3] = *(ptr + 3);
      str_length[4] = *(ptr + 4);
      str_length[5] = *(ptr + 5);
      str_length[6] = *(ptr + 6);
      str_length[7] = *(ptr + 7);
      length = (unsigned int)strtoul(str_length, NULL, 10);
      if (length > 0)
      {
         if (*(ptr + 9) == '1')
         {
            soh_etx = NO;
         }
         else
         {
            soh_etx = YES;
            if (*(ptr + length + 10 - 1) != 3)
            {
               while ((ptr - src_ptr + 10 - 1 + additional_length) < total_length)
               {
                  additional_length++;
                  if (*(ptr + length + 10 - 1 + additional_length) == 3)
                  {
                     break;
                  }
               }
               if (*(ptr + length + 10 - 1 + additional_length) != 3)
               {
                  receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                              "Unable to determine terminating ETX in %s. #%x",
                              p_orig_name, job_id);
                  additional_length = 0;
               }
            }
         }
         if (write_file(ptr + 10, length + additional_length, mtime,
                        soh_etx, job_id) < 0)
         {
            return;
         }
      }
      ptr += length + additional_length + 10;
   } while ((ptr - src_ptr) < total_length);

   return;
}


/*++++++++++++++++++++++++++++ separator_char() +++++++++++++++++++++++++*/
static void
separator_char(char         *src_ptr,
               off_t        total_length,
               time_t       mtime,
               char         *file_name,
               char         separator,
               unsigned int job_id)
{
   char *ptr = src_ptr,
        *ptr_start;

   do
   {
      ptr_start = ptr;
      while ((*ptr != separator) && ((ptr - src_ptr) <= total_length))
      {
         ptr++;
      }
      if (*ptr == separator)
      {
         ptr++;
         if (write_file(ptr_start, (unsigned int)(ptr - ptr_start), mtime,
                        NO, job_id) < 0)
         {
            return;
         }
         if ((ptr - src_ptr) >= total_length)
         {
            return;
         }
      }
      else
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to locate terminating character %c in %s. #%x"),
                     separator, file_name, job_id);
         return;
      }
   } while ((ptr - src_ptr) <= total_length);

   return;
}


/*------------------------------ write_file() ---------------------------*/
static int
write_file(char         *msg,
           unsigned int length,
           time_t       mtime,
           int          soh_etx,
           unsigned int job_id)
{
   int   bcdb_pos = -1,
         fd,
         i;
   off_t size;
   char  *ptr,
         *p_start;

   /*
    * Build the file name from the bulletin header.
    */
   if ((soh_etx == YES) && (msg[0] != 1)) /* Must start with SOH */
   {
      receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to read bulletin header. No SOH at start in %s. #%x"),
                  p_orig_name, job_id);
      return(INCORRECT);
   }

   /*
    * Position to start of header ie. after <SOH><CR><CR><LF>nnn<CR><CR><LF>
    * and then store the heading in heading_str. The end of heading is when
    * we hit an unprintable character, in most cases this should be the
    * <CR><CR><LF> after the heading.
    */
   ptr = msg;
   while (((ptr - msg) < length) && (*ptr < 32))
   {
      ptr++;
   }
   if ((ptr + 3 - msg) >= length)
   {
      receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                  _("Failed to read bulletin header. No header found in %s (%d >= %u). #%x"),
                  p_orig_name, (int)(ptr + 3 - msg), length, job_id);
      return(INCORRECT);
   }
   if (((ptr + 4 - msg) <= length) && (*ptr == 'Z') && (*(ptr + 1) == 'C') &&
       (*(ptr + 2) == 'Z') && (*(ptr + 3) == 'C'))
   {
      ptr += 4;
      while (((ptr - msg) < length) && (*ptr == ' '))
      {
         ptr++;
      }
      if ((ptr + 3 - msg) >= length)
      {
         receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to read bulletin header. No header found in %s. #%x"),
                     p_orig_name, job_id);
         return(INCORRECT);
      }
   }
   p_start = ptr;
   while (((ptr - msg) < length) && (isdigit((int)(*ptr))))
   {
      ptr++;
   }
   if ((*ptr != 13) && (*ptr != 10))
   {
      ptr = p_start;
   }
   while (((ptr - msg) < length) &&
          ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
   {
      ptr++;
   }
   p_start = ptr;
   i = 0;
   if (extract_options & EXTRACT_ADD_FULL_DATE)
   {
      int space_counter = 0;

      while (((ptr - msg) < length) &&
             (*ptr > 31) && (i < (MAX_WMO_HEADER_LENGTH + 6)))
      {
         if ((*ptr == ' ') || (*ptr == '/') || (*ptr < ' ') || (*ptr > 'z'))
         {
            p_file_name[i] = '_';
            space_counter++;
            if ((space_counter == 2) && ((ptr + 2 - msg) < length) &&
                (isdigit(*(ptr + 1))) && (isdigit(*(ptr + 2))))
            {
               int       day_of_month,
                         diff_mday;
               struct tm *bd_time;     /* Broken-down time. */

               if (*(ptr + 1) == '0')
               {
                  day_of_month = *(ptr + 2) - '0';
               }
               else
               {
                  day_of_month = ((*(ptr + 1) - '0') * 10) + (*(ptr + 2) - '0');
               }

               bd_time = gmtime(&mtime);
               diff_mday = day_of_month - bd_time->tm_mday;
               if (diff_mday != 0)
               {
                  if (diff_mday > 26)
                  {
                     bd_time->tm_mday = day_of_month;
                     bd_time->tm_mon--;
                     mtime = mktime(bd_time);
                     bd_time = gmtime(&mtime);
                  }
                  else if (diff_mday < -26)
                       {
                          bd_time->tm_mday = day_of_month;
                          bd_time->tm_mon++;
                          mtime = mktime(bd_time);
                          bd_time = gmtime(&mtime);
                       }
               }
               i += snprintf(&p_file_name[i + 1], MAX_PATH_LENGTH - (i + 1),
                             "%04d%02d",
                             (bd_time->tm_year + 1900), (bd_time->tm_mon + 1));
            }
         }
         else
         {
            p_file_name[i] = *ptr;
         }
         ptr++; i++;
      }
   }
   else
   {
      while (((ptr - msg) < length) &&
             (*ptr > 31) && (i < MAX_WMO_HEADER_LENGTH))
      {
         if ((*ptr == ' ') || (*ptr == '/') || (*ptr < ' ') || (*ptr > 'z'))
         {
            p_file_name[i] = '_';
         }
         else
         {
            p_file_name[i] = *ptr;
         }
         ptr++; i++;
      }
   }
   if (i == 0)
   {
      receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                  _("Length of WMO header is 0 in %s!? Discarding file. #%x"),
                  p_orig_name, job_id);
      return(INCORRECT);
   }
   else
   {
      if (p_file_name[i - 1] == '_')
      {
         p_file_name[i - 1] = '\0';
         i--;
      }
      else
      {
         p_file_name[i] = '\0';
      }
   }

   if (extract_options & USE_EXTERNAL_MSG_RULES)
   {
      register int j;

#ifdef FIRST_MATCH_IS_HIT
      for (j = 0; j < no_of_bc_entries; j++)
      {
         if (((bcdb[j].TTAAii[0] == '/') ||
              (bcdb[j].TTAAii[0] == p_file_name[0])) &&
             ((bcdb[j].TTAAii[1] == '/') ||
              (bcdb[j].TTAAii[1] == p_file_name[1])) &&
             ((bcdb[j].TTAAii[2] == '/') ||
              (bcdb[j].TTAAii[2] == p_file_name[2])) &&
             ((bcdb[j].TTAAii[3] == '/') ||
              (bcdb[j].TTAAii[3] == p_file_name[3])) &&
             (((bcdb[j].TTAAii[4] == '\0') && (p_file_name[4] == '_') &&
               ((bcdb[j].CCCC[0] == '/') ||
                (bcdb[j].CCCC[0] == p_file_name[5])) &&
               ((bcdb[j].CCCC[1] == '/') ||
                (bcdb[j].CCCC[1] == p_file_name[6])) &&
               ((bcdb[j].CCCC[2] == '/') ||
                (bcdb[j].CCCC[2] == p_file_name[7])) &&
               ((bcdb[j].CCCC[3] == '/') ||
                (bcdb[j].CCCC[3] == p_file_name[8]))) ||
              (((bcdb[j].TTAAii[4] == '/') ||
                (bcdb[j].TTAAii[4] == p_file_name[4])) &&
               ((bcdb[j].TTAAii[5] == '/') ||
                (bcdb[j].TTAAii[5] == p_file_name[5])) &&
               (bcdb[j].TTAAii[6] == '\0') && (p_file_name[6] == '_') &&
               ((bcdb[j].CCCC[0] == '/') ||
                (bcdb[j].CCCC[0] == p_file_name[7])) &&
               ((bcdb[j].CCCC[1] == '/') ||
                (bcdb[j].CCCC[1] == p_file_name[8])) &&
               ((bcdb[j].CCCC[2] == '/') ||
                (bcdb[j].CCCC[2] == p_file_name[9])) &&
               ((bcdb[j].CCCC[3] == '/') ||
                (bcdb[j].CCCC[3] == p_file_name[10])))))
         {
            if (bcdb[j].type == BUL_TYPE_IGN)
            {
               /* Ignore this bulletin. */
               *p_file_name = '\0';
               return(SUCCESS);
            }
            else
            {
               bcdb_pos = j;
            }
            break;
         }
      }
#else /* Last one that matches counts. */
      for (j = 0; j < no_of_bc_entries; j++)
      {
         if (((bcdb[j].TTAAii[0] == '/') ||
              (bcdb[j].TTAAii[0] == p_file_name[0])) &&
             ((bcdb[j].TTAAii[1] == '/') ||
              (bcdb[j].TTAAii[1] == p_file_name[1])) &&
             ((bcdb[j].TTAAii[2] == '/') ||
              (bcdb[j].TTAAii[2] == p_file_name[2])) &&
             ((bcdb[j].TTAAii[3] == '/') ||
              (bcdb[j].TTAAii[3] == p_file_name[3])) &&
             (((bcdb[j].TTAAii[4] == '\0') && (p_file_name[4] == '_') &&
               ((bcdb[j].CCCC[0] == '/') ||
                (bcdb[j].CCCC[0] == p_file_name[5])) &&
               ((bcdb[j].CCCC[1] == '/') ||
                (bcdb[j].CCCC[1] == p_file_name[6])) &&
               ((bcdb[j].CCCC[2] == '/') ||
                (bcdb[j].CCCC[2] == p_file_name[7])) &&
               ((bcdb[j].CCCC[3] == '/') ||
                (bcdb[j].CCCC[3] == p_file_name[8]))) ||
              (((bcdb[j].TTAAii[4] == '/') ||
                (bcdb[j].TTAAii[4] == p_file_name[4])) &&
               ((bcdb[j].TTAAii[5] == '/') ||
                (bcdb[j].TTAAii[5] == p_file_name[5])) &&
               (bcdb[j].TTAAii[6] == '\0') && (p_file_name[6] == '_') &&
               ((bcdb[j].CCCC[0] == '/') ||
                (bcdb[j].CCCC[0] == p_file_name[7])) &&
               ((bcdb[j].CCCC[1] == '/') ||
                (bcdb[j].CCCC[1] == p_file_name[8])) &&
               ((bcdb[j].CCCC[2] == '/') ||
                (bcdb[j].CCCC[2] == p_file_name[9])) &&
               ((bcdb[j].CCCC[3] == '/') ||
                (bcdb[j].CCCC[3] == p_file_name[10])))))
         {
            bcdb_pos = j;
         }
      }
      if (bcdb[bcdb_pos].type == BUL_TYPE_IGN)
      {
         /* Ignore this bulletin. */
         *p_file_name = '\0';
         return(SUCCESS);
      }
#endif
   }

   if (extract_options & EXTRACT_REPORTS)
   {
      int offset = 0;

      while (((ptr - msg) < length) &&
             ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ') || (*ptr == '>')))
      {
         ptr++;
      }

      if (extract_options & USE_EXTERNAL_MSG_RULES)
      {
         int  rcdb_pos,
              ret = INCORRECT;
         char wid[2];

         rcdb_pos = get_rcdb_position(p_file_name, bcdb_pos);
         wid[0] = '\0';
         if ((bcdb_pos != -1) && (bcdb[bcdb_pos].rss != -1) &&
             (rcdb_pos != -1) && (rcdb[rcdb_pos].rt != RT_NOT_DEFINED) &&
             ((ret = find_offset(rcdb_pos, ptr, length - (ptr - msg),
                                 &offset, wid, job_id)) == SUCCESS))
         {
            int  begin_file_name_offset,
                 bul_name_length = i,
                 end_offset,
                 file_name_offset = i,
                 not_wanted = NO,
                 overwrite_extra_heading;
            char *p_extra_heading = NULL;

            if ((extract_options & EXTRACT_SHOW_REPORT_TYPE) && (offset > 0))
            {
               int  space_count = 0;
               char *tmp_ptr = ptr;

               p_file_name[file_name_offset] = '-';
               file_name_offset++;
               begin_file_name_offset = file_name_offset;
               while ((tmp_ptr < (ptr + offset)) &&
                      (*tmp_ptr != 13) && (*tmp_ptr != 10))
               {
                  if (*tmp_ptr == ' ')
                  {
                     if (space_count == 0)
                     {
                        p_file_name[file_name_offset] = '_';
                        space_count++;
                     }
                     else
                     {
                        break;
                     }
                  }
                  else if (*tmp_ptr == '/')
                       {
                          p_file_name[file_name_offset] = '?';
                       }
                       else
                       {
                          p_file_name[file_name_offset] = *tmp_ptr;
                       }
                  file_name_offset++;
                  tmp_ptr++;
               }
            }
            else
            {
               begin_file_name_offset = -1;
            }
            p_file_name[file_name_offset] = '-';
            file_name_offset++;
            if ((extract_options & EXTRACT_EXTRA_REPORT_HEADING) &&
                (offset > 0))
            {
               p_extra_heading = ptr;
            }
            ptr += offset;

            do
            {
               p_start = ptr;

               /* Ignore any spaces at start. */
               while (((ptr - msg) < length) && ((*ptr == ' ') || (*ptr == '>')))
               {
                  ptr++;
               }

               if (get_station_id(p_file_name, file_name_offset, &end_offset,
                                  ptr, length - (ptr - msg), rcdb_pos,
                                  begin_file_name_offset,
                                  &p_extra_heading, &offset,
                                  &overwrite_extra_heading, wid) != SUCCESS)
               {
                  /* Assume that this is a malformed or NIL report. */
                  while (((ptr - msg) < length) && (*ptr != '='))
                  {
                     ptr++;
                  }
                  while (((ptr - msg) < length) && (*ptr == '='))
                  {
                     ptr++;
                  }
                  if ((*ptr != 13) && (*ptr != 10))
                  {
                     while (((ptr - msg) < length) && (*ptr != 13) &&
                            (*ptr != 10))
                     {
                        ptr++;
                     }
                  }
                  while (((ptr - msg) < length) &&
                         ((*ptr == 13) || (*ptr == 10)))
                  {
                     ptr++;
                  }
                  if (((isprint((int)(*ptr))) &&
                       ((*(ptr + 1) == 13) || (*(ptr + 1) == 10))) ||
                      ((isprint((int)(*ptr))) && (isprint((int)(*(ptr + 1)))) &&
                       ((*(ptr + 2) == 13) || (*(ptr + 2) == 10))) ||
                      ((isprint((int)(*ptr))) && (isprint((int)(*(ptr + 1)))) &&
                       (isprint((int)(*(ptr + 2)))) &&
                       ((*(ptr + 3) == 13) || (*(ptr + 3) == 10))))
                  {
                     ptr++;
                     while (((ptr - msg) < length) && (*ptr != 13) &&
                            (*ptr != 10))
                     {
                        ptr++;
                     }
                     while (((ptr - msg) < length) &&
                            ((*ptr == 13) || (*ptr == 10)))
                     {
                        ptr++;
                     }
                  }
               }
               else
               {
                  ptr += end_offset;

                  if (extract_p_filter != NULL)
                  {
                     p_file_name[file_name_offset + end_offset] = '\0';
                     if (pmatch(extract_p_filter, p_file_name, NULL) != 0)
                     {
                        /* Ignore this report. */
                        not_wanted = YES;
                     }
                     else
                     {
                        not_wanted = NO;
                     }
                  }
                  while (((ptr - msg) < length) && (*ptr != '='))
                  {
                     ptr++;
                  }
                  while (((ptr - msg) < length) && (*ptr == '='))
                  {
                     ptr++;
                  }

                  /*
                   * Do not show any garbage or the last <CR><CR><LF>
                   * in the report.
                   */

                  if (not_wanted == NO)
                  {
                     int additional_offset = 0;

                     if (extract_options & EXTRACT_ADD_ADDITIONAL_INFO)
                     {
                        char bulname[32];

                        if (bul_name_length < 32)
                        {
                           (void)memcpy(bulname, p_file_name, bul_name_length);
                           bulname[bul_name_length] = '\0';
                        }
                        else
                        {
                           receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                       _("bulname to short, should be %d bytes long. #%x"),
                                       bul_name_length, job_id);
                           bulname[0] = '\0';
                        }
                        additional_offset += snprintf(&p_file_name[file_name_offset + end_offset],
                                                      MAX_PATH_LENGTH - (file_name_offset + end_offset),
                                                      "#%s.%s#%s#%s#%s#%s",
                                                      wid, rcdb[rcdb_pos].wid,
                                                      rcdb[rcdb_pos].BTIME,
                                                      rcdb[rcdb_pos].ITIME,
                                                      bulname, p_orig_name);
                     }

                     size = ptr - (p_start + overwrite_extra_heading);
                     if (extract_options & EXTRACT_ADD_CRC_CHECKSUM)
                     {
                        additional_offset += snprintf(&p_file_name[file_name_offset + end_offset + additional_offset],
                                                      MAX_PATH_LENGTH - (file_name_offset + end_offset + additional_offset),
                                                      "-%x",
                                                      get_checksum_crc32c(INITIAL_CRC,
                                                                          p_start + overwrite_extra_heading,
#ifdef HAVE_HW_CRC32
                                                                          (int)size, have_hw_crc32));
#else
                                                                          (int)size));
#endif
                     }
                     if (extract_options & EXTRACT_ADD_UNIQUE_NUMBER)
                     {
                        (void)next_counter(counter_fd, counter, MAX_MSG_PER_SEC);
                        additional_offset += snprintf(&p_file_name[file_name_offset + end_offset + additional_offset],
                                               MAX_PATH_LENGTH - (file_name_offset + end_offset + additional_offset),
                                               "-%04x", *counter);
                     }
                     p_file_name[file_name_offset + end_offset + additional_offset] = '\0';

                     if ((fd = open(p_full_file_name, (O_RDWR | O_CREAT | O_TRUNC),
                                    file_mode)) < 0)
                     {
                        receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                    _("Failed to open() `%s' while extracting reports : %s #%x"),
                                    p_full_file_name, strerror(errno), job_id);
                        *p_file_name = '\0';
                        return(INCORRECT);
                     }
                     if (extract_options & EXTRACT_ADD_BUL_ORIG_FILE)
                     {
                        size_t length;

                        length = strlen(p_file_name);
                        if (writen(fd, p_file_name, length, 0) != (ssize_t)length)
                        {
                           receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                       _("Failed to writen() file name : %s #%x"),
                                       strerror(errno), job_id);
                           (void)close(fd);
                           return(INCORRECT);
                        }
                        *p_file_size += length;
                        if (writen(fd, " ", (size_t)1, 0) != (ssize_t)1)
                        {
                           receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                       _("Failed to writen() space after file name : %s #%x"),
                                       strerror(errno), job_id);
                           (void)close(fd);
                           return(INCORRECT);
                        }
                        *p_file_size += 1;
                        length = strlen(p_orig_name);
                        if (writen(fd, p_orig_name, length, 0) != (ssize_t)length)
                        {
                           receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                       _("Failed to writen() extract file name : %s #%x"),
                                       strerror(errno), job_id);
                           (void)close(fd);
                           return(INCORRECT);
                        }
                        *p_file_size += length;
                        if (writen(fd, "\r\r\n", 3, 0) != 3)
                        {
                           receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                       _("Failed to writen() carriage return, carriage return + line feed : %s #%x"),
                                       strerror(errno), job_id);
                           (void)close(fd);
                           return(INCORRECT);
                        }
                        *p_file_size += 2;
                     }
                     if (p_extra_heading != NULL)
                     {
                        if (writen(fd, p_extra_heading, (size_t)offset, 0) != (ssize_t)offset)
                        {
                           receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                       _("Failed to writen() extra header in report : %s #%x"),
                                       strerror(errno), job_id);
                           (void)close(fd);
                           return(INCORRECT);
                        }
                        *p_file_size += offset;
                     }
                     if (writen(fd, (p_start + overwrite_extra_heading), (size_t)size, 0) != (ssize_t)size)
                     {
                        receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                    _("Failed to writen() report : %s #%x"),
                                    strerror(errno), job_id);
                        (void)close(fd);
                        if (p_extra_heading != NULL)
                        {
                           *p_file_size -= offset;
                        }
                        return(INCORRECT);
                     }
                     if (writen(fd, "\r\r\n", 3, 0) != 3)
                     {
                        receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                    _("Failed to writen() carriage return, carriage return + line feed : %s #%x"),
                                    strerror(errno), job_id);
                        (void)close(fd);
                        return(INCORRECT);
                     }
                     (*p_files_to_send)++;
                     *p_file_size += size + 3;

                     if (close(fd) == -1)
                     {
                        receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                                    _("close() error : %s"), strerror(errno));
                     }
#ifdef _PRODUCTION_LOG
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
                        pld[no_of_log_entries].size = size;
                        pld[no_of_log_entries].production_time = (times(&tval) - start_time) / (double)clktck;
                        get_sum_cpu_usage(&ru, &pld[no_of_log_entries].cpu_usage);
                        start_time = times(&tval);
                        (void)strcpy(pld[no_of_log_entries].file_name, p_file_name);
                        no_of_log_entries++;
                     }
#endif
                  }

                  /* Lets ignore any garbage behind the end of report. */
                  if ((*ptr != 13) && (*ptr != 10))
                  {
                     while (((ptr - msg) < length) && (*ptr != 13) &&
                            (*ptr != 10))
                     {
                        ptr++;
                     }
                  }
                  while (((ptr - msg) < length) &&
                         ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
                  {
                     ptr++;
                  }
                  if (((isprint((int)(*ptr))) &&
                       ((*(ptr + 1) == 13) || (*(ptr + 1) == 10))) ||
                      ((isprint((int)(*ptr))) && (isprint((int)(*(ptr + 1)))) &&
                       ((*(ptr + 2) == 13) || (*(ptr + 2) == 10))) ||
                      ((isprint((int)(*ptr))) && (isprint((int)(*(ptr + 1)))) &&
                       (isprint((int)(*(ptr + 2)))) &&
                       ((*(ptr + 3) == 13) || (*(ptr + 3) == 10))))
                  {
                     ptr++;
                     while (((ptr - msg) < length) && (*ptr != 13) &&
                            (*ptr != 10))
                     {
                        ptr++;
                     }
                     while (((ptr - msg) < length) &&
                            ((*ptr == 13) || (*ptr == 10)))
                     {
                        ptr++;
                     }
                  }
               }
            } while ((ptr + 6 - msg) < length);
         }
         else
         {
            if (ret == INCORRECT)
            {
               char reason[64];

               if (bcdb_pos == -1)
               {
                  (void)strcpy(reason, "bcb_pos is -1");
               }
               else if (bcdb[bcdb_pos].rss == -1)
                    {
                       (void)strcpy(reason, "report sub specification (rss) says it is not a report");
                    }
               else if (rcdb_pos == -1)
                    {
                       (void)strcpy(reason, "rcb_pos is -1");
                    }
               else if (rcdb[rcdb_pos].rt == RT_NOT_DEFINED)
                    {
                       (void)strcpy(reason, "report type is not defined");
                    }
                    else /* Assume we failed to find offset. */
                    {
                       (void)strcpy(reason, "failed to determine offset");
                    }

               receive_log(INFO_SIGN, __FILE__, __LINE__, 0L,
                           "%s: Not extracting reports from %s because %s (pos bul=%d rep=%d). #%x",
                           p_file_name, p_orig_name, reason, bcdb_pos,
                           rcdb_pos, job_id);

#ifdef WHEN_READY
               if (extract_options & EXTRACT_ALSO_WRITE_NO_REPORTS)
               {
                  int file_name_offset = i;

                  p_file_name[file_name_offset] = '-';
                  file_name_offset++;
                  if (p_file_name[4] == '_')
                  {
                     p_file_name[file_name_offset] = p_file_name[5];
                     p_file_name[file_name_offset + 1] = p_file_name[6];
                     p_file_name[file_name_offset + 2] = p_file_name[7];
                     p_file_name[file_name_offset + 3] = p_file_name[8];
                  }
                  else if (p_file_name[6] == '_')
                       {
                          p_file_name[file_name_offset] = p_file_name[7];
                          p_file_name[file_name_offset + 1] = p_file_name[8];
                          p_file_name[file_name_offset + 2] = p_file_name[9];
                          p_file_name[file_name_offset + 3] = p_file_name[10];
                       }
                       else
                       {
                          p_file_name[file_name_offset] = '?';
                          p_file_name[file_name_offset + 1] = '?';
                          p_file_name[file_name_offset + 2] = '?';
                          p_file_name[file_name_offset + 3] = '?';
                       }
                  p_file_name[file_name_offset + 4] = '\0';

                  if ((fd = open(p_full_file_name, (O_RDWR | O_CREAT | O_TRUNC),
                                 file_mode)) < 0)
                  {
                     receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                 _("Failed to open() `%s' while extracting reports : %s"),
                                 p_full_file_name, strerror(errno));
                     *p_file_name = '\0';
                     return(INCORRECT);
                  }
                  if (writen(fd, p_start, (size_t)length, 0) != (ssize_t)length)
                  {
                     receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                 _("Failed to writen() report : %s"), strerror(errno));
                     (void)close(fd);
                     return(INCORRECT);
                  }
                  (*p_files_to_send)++;
                  *p_file_size += length;

                  if (close(fd) == -1)
                  {
                     receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                                 _("close() error : %s"), strerror(errno));
                  }
               }
#endif /* WHEN_READY */
            }
            else if (ret == TEXT_MESSAGE)
                 {
                    receive_log(INFO_SIGN, __FILE__, __LINE__, 0L,
                                "%s: Not extracting reports from %s because report type is TEXT (pos bul=%d rep=%d). #%x",
                                p_file_name, p_orig_name, bcdb_pos,
                                rcdb_pos, job_id);
                 }
         }
      }
      else
      {
         if ((((p_file_name[0] == 'F') &&
               ((p_file_name[1] == 'T') || (p_file_name[1] == 'C'))) ||
              ((p_file_name[0] == 'S') &&
               ((p_file_name[1] == 'A') || (p_file_name[1] == 'H') ||
                (p_file_name[1] == 'I') || (p_file_name[1] == 'M') ||
                (p_file_name[1] == 'N') || (p_file_name[1] == 'P') ||
                (p_file_name[1] == 'X'))) ||
              ((p_file_name[0] == 'U') &&
               ((p_file_name[1] == 'S') || (p_file_name[1] == 'K') ||
                (p_file_name[1] == 'L') || (p_file_name[1] == 'E') ||
                (p_file_name[1] == 'P') || (p_file_name[1] == 'G') ||
                (p_file_name[1] == 'H') || (p_file_name[1] == 'Q')))) &&
             (check_report(ptr, length - (ptr - msg), &offset) == SUCCESS))
         {
            int  bul_name_length = i,
                 end_offset,
                 file_name_offset = i,
                 not_wanted = NO;
            char *p_extra_heading = NULL;

            if ((extract_options & EXTRACT_SHOW_REPORT_TYPE) && (offset > 0))
            {
               char *tmp_ptr = ptr;

               p_file_name[file_name_offset] = '-';
               file_name_offset++;
               while ((tmp_ptr < (ptr + offset)) &&
                      (*tmp_ptr != 13) && (*tmp_ptr != 10))
               {
                  if (*tmp_ptr == ' ')
                  {
                     p_file_name[file_name_offset] = '_';
                  }
                  else
                  {
                     p_file_name[file_name_offset] = *tmp_ptr;
                  }
                  file_name_offset++;
                  tmp_ptr++;
               }
            }
            p_file_name[file_name_offset] = '-';
            file_name_offset++;
            if ((extract_options & EXTRACT_EXTRA_REPORT_HEADING) &&
                (offset > 0))
            {
               p_extra_heading = ptr;
            }
            ptr += offset;

            do
            {
               p_start = ptr;

               /* Ignore any spaces at start. */
               while (((ptr - msg) < length) && (*ptr == ' '))
               {
                  ptr++;
               }

               /* TAF */
               if (((ptr + 9 - msg) < length) && (*ptr == 'T') &&
                   (*(ptr + 1) == 'A') && (*(ptr + 2) == 'F') &&
                   (*(ptr + 3) == ' ') &&
                   (isupper((int)(*(ptr + 4)))) && (isupper((int)(*(ptr + 5)))) &&
                   (isupper((int)(*(ptr + 6)))) && (isupper((int)(*(ptr + 7)))) &&
                   (*(ptr + 8) == ' '))
               {
                  p_file_name[file_name_offset] = *(ptr + 4);
                  p_file_name[file_name_offset + 1] = *(ptr + 5);
                  p_file_name[file_name_offset + 2] = *(ptr + 6);
                  p_file_name[file_name_offset + 3] = *(ptr + 7);
                  end_offset = 4;
                  ptr += 9;
               }
                    /* TAF AMD or COR */
               else if (((ptr + 13 - msg) < length) && (isupper((int)(*ptr))) &&
                        (isupper((int)(*(ptr + 1)))) &&
                        (isupper((int)(*(ptr + 2)))) && (*(ptr + 3) == ' ') &&
                        (isupper((int)(*(ptr + 4)))) &&
                        (isupper((int)(*(ptr + 5)))) &&
                        (isupper((int)(*(ptr + 6)))) && (*(ptr + 7) == ' ') &&
                        (isupper((int)(*(ptr + 8)))) &&
                        (isupper((int)(*(ptr + 9)))) &&
                        (isupper((int)(*(ptr + 10)))) &&
                        (isupper((int)(*(ptr + 11)))) && (*(ptr + 12) == ' '))
                    {
                       p_file_name[file_name_offset] = *(ptr + 8);
                       p_file_name[file_name_offset + 1] = *(ptr + 9);
                       p_file_name[file_name_offset + 2] = *(ptr + 10);
                       p_file_name[file_name_offset + 3] = *(ptr + 11);
                       end_offset = 4;
                       ptr += 13;
                    }
                    /* METAR or SPECI */
               else if (((ptr + 6 - msg) < length) &&
                        (((*ptr == 'M') && (*(ptr + 1) == 'E') &&
                          (*(ptr + 2) == 'T') && (*(ptr + 3) == 'A') &&
                          (*(ptr + 4) == 'R')) || 
                         ((*ptr == 'S') && (*(ptr + 1) == 'P') &&
                          (*(ptr + 2) == 'E') && (*(ptr + 3) == 'C') &&
                          (*(ptr + 4) == 'I'))) && 
                        (*(ptr + 5) == ' '))
                    {
                       while (((ptr + 6 - msg) < length) && (*(ptr + 6) == ' '))
                       {
                          ptr++;
                       }
                       if (((ptr + 4 - msg) < length) &&
                           (*(ptr + 6) == 'C') && (*(ptr + 7) == 'O') &&
                           (*(ptr + 8) == 'R') && (*(ptr + 9) == ' '))
                       {
                          ptr += 4;
                       }
                       if (((ptr + 5 - msg) < length) &&
                           ((isupper((int)(*(ptr + 6)))) ||
                            (isdigit((int)(*(ptr + 6))))) &&
                           ((isupper((int)(*(ptr + 7)))) ||
                            (isdigit((int)(*(ptr + 6))))) &&
                           ((isupper((int)(*(ptr + 8)))) ||
                            (isdigit((int)(*(ptr + 6))))) &&
                           ((isupper((int)(*(ptr + 9)))) ||
                            (isdigit((int)(*(ptr + 6))))) &&
                           (*(ptr + 10) == ' '))
                       {
                          p_file_name[file_name_offset] = *(ptr + 6);
                          p_file_name[file_name_offset + 1] = *(ptr + 7);
                          p_file_name[file_name_offset + 2] = *(ptr + 8);
                          p_file_name[file_name_offset + 3] = *(ptr + 9);
                          end_offset = 4;
                          ptr += 11;
                       }
                       else if (((ptr + 6 - msg) < length) &&
                                ((isupper((int)(*(ptr + 6)))) ||
                                 (isdigit((int)(*(ptr + 6))))) &&
                                ((isupper((int)(*(ptr + 7)))) ||
                                 (isdigit((int)(*(ptr + 7))))) &&
                                ((isupper((int)(*(ptr + 8)))) ||
                                 (isdigit((int)(*(ptr + 8))))) &&
                                ((isupper((int)(*(ptr + 9)))) ||
                                 (isdigit((int)(*(ptr + 9))))) &&
                                ((isupper((int)(*(ptr + 10)))) ||
                                 (isdigit((int)(*(ptr + 10))))) &&
                                (*(ptr + 11) == ' '))
                            {
                               p_file_name[file_name_offset] = *(ptr + 6);
                               p_file_name[file_name_offset + 1] = *(ptr + 7);
                               p_file_name[file_name_offset + 2] = *(ptr + 8);
                               p_file_name[file_name_offset + 3] = *(ptr + 9);
                               p_file_name[file_name_offset + 4] = *(ptr + 10);
                               end_offset = 5;
                               ptr += 12;
                            }
                            else
                            {
                               show_unknown_report(ptr, length, msg,
                                                   job_id,__FILE__, __LINE__);
                               break;
                            }
                    }
                    /* METAR, SPECI, TAF AMD, AAXX or BBXX (in a group) */
               else if (((ptr + 5 - msg) < length) &&
                        ((isupper((int)(*ptr))) || (isdigit((int)(*ptr)))) &&
                        ((isupper((int)(*(ptr + 1)))) ||
                         (isdigit((int)(*(ptr + 1))))) &&
                        ((isupper((int)(*(ptr + 2)))) ||
                         (isdigit((int)(*(ptr + 2))))) &&
                        ((isupper((int)(*(ptr + 3)))) ||
                         (isdigit((int)(*(ptr + 3))))) &&
                        (*(ptr + 4) == ' '))
                    {
                       p_file_name[file_name_offset] = *ptr;
                       p_file_name[file_name_offset + 1] = *(ptr + 1);
                       p_file_name[file_name_offset + 2] = *(ptr + 2);
                       p_file_name[file_name_offset + 3] = *(ptr + 3);
                       end_offset = 4;
                       ptr += 5;
                    }
                    /* German METAR */
               else if (((ptr + 13 - p_start) < length) &&
                        (isupper((int)(*ptr))) && (isupper((int)(*(ptr + 1)))) &&
                        (isupper((int)(*(ptr + 2)))) && (isupper((int)(*(ptr + 3)))) &&
                        (*(ptr + 4) == ' ') && (isdigit((int)(*(ptr + 5)))) &&
                        (isdigit((int)(*(ptr + 6)))) && (isdigit((int)(*(ptr + 7)))) &&
                        (isdigit((int)(*(ptr + 8)))) && (isdigit((int)(*(ptr + 9)))) &&
                        (isdigit((int)(*(ptr + 10)))) && (*(ptr + 11) == 'Z') &&
                        (*(ptr + 12) == ' '))
                    {
                       p_file_name[file_name_offset] = *ptr;
                       p_file_name[file_name_offset + 1] = *(ptr + 1);
                       p_file_name[file_name_offset + 2] = *(ptr + 2);
                       p_file_name[file_name_offset + 3] = *(ptr + 3);
                       end_offset = 4;
                       ptr += 13;
                    }
                    /* AAXX or BBXX (in a group) */
               else if (((ptr + 7 - msg) < length) &&
                        ((isupper((int)(*ptr))) || (isdigit((int)(*ptr)))) &&
                        ((isupper((int)(*(ptr + 1)))) ||
                         (isdigit((int)(*(ptr + 1))))) &&
                        ((isupper((int)(*(ptr + 2)))) ||
                         (isdigit((int)(*(ptr + 2))))) &&
                        ((isupper((int)(*(ptr + 3)))) ||
                         (isdigit((int)(*(ptr + 3))))) &&
                        ((isupper((int)(*(ptr + 4)))) ||
                         (isdigit((int)(*(ptr + 4))))) &&
                        ((isupper((int)(*(ptr + 5)))) ||
                         (isdigit((int)(*(ptr + 5))))) &&
                        (*(ptr + 6) == ' '))
                    {
                       p_file_name[file_name_offset] = *ptr;
                       p_file_name[file_name_offset + 1] = *(ptr + 1);
                       p_file_name[file_name_offset + 2] = *(ptr + 2);
                       p_file_name[file_name_offset + 3] = *(ptr + 3);
                       p_file_name[file_name_offset + 4] = *(ptr + 4);
                       p_file_name[file_name_offset + 5] = *(ptr + 5);
                       end_offset = 6;
                       ptr += 7;
                    }
                    /* AAXX or BBXX (in a group) */
               else if (((ptr + 8 - msg) < length) &&
                        ((isupper((int)(*ptr))) || (isdigit((int)(*ptr)))) &&
                        ((isupper((int)(*(ptr + 1)))) ||
                         (isdigit((int)(*(ptr + 1))))) &&
                        ((isupper((int)(*(ptr + 2)))) ||
                         (isdigit((int)(*(ptr + 2))))) &&
                        ((isupper((int)(*(ptr + 3)))) ||
                         (isdigit((int)(*(ptr + 3))))) &&
                        ((isupper((int)(*(ptr + 4)))) ||
                         (isdigit((int)(*(ptr + 4))))) &&
                        ((isupper((int)(*(ptr + 5)))) ||
                         (isdigit((int)(*(ptr + 5))))) &&
                        ((isupper((int)(*(ptr + 6)))) ||
                         (isdigit((int)(*(ptr + 6))))) &&
                        (*(ptr + 7) == ' '))
                    {
                       p_file_name[file_name_offset] = *ptr;
                       p_file_name[file_name_offset + 1] = *(ptr + 1);
                       p_file_name[file_name_offset + 2] = *(ptr + 2);
                       p_file_name[file_name_offset + 3] = *(ptr + 3);
                       p_file_name[file_name_offset + 4] = *(ptr + 4);
                       p_file_name[file_name_offset + 5] = *(ptr + 5);
                       p_file_name[file_name_offset + 6] = *(ptr + 6);
                       end_offset = 7;
                       ptr += 8;
                    }
                    /* SYNOP, AAXX or BBXX (in a group) */
               else if (((ptr + 6 - msg) < length) &&
                        ((isupper((int)(*ptr))) || (isdigit((int)(*ptr)))) &&
                        ((isupper((int)(*(ptr + 1)))) ||
                         (isdigit((int)(*(ptr + 1))))) &&
                        ((isupper((int)(*(ptr + 2)))) ||
                         (isdigit((int)(*(ptr + 2))))) &&
                        ((isupper((int)(*(ptr + 3)))) ||
                         (isdigit((int)(*(ptr + 3))))) &&
                        ((isupper((int)(*(ptr + 4)))) ||
                         (isdigit((int)(*(ptr + 4))))) &&
                        (*(ptr + 5) == ' '))
                    {
                       p_file_name[file_name_offset] = *ptr;
                       p_file_name[file_name_offset + 1] = *(ptr + 1);
                       p_file_name[file_name_offset + 2] = *(ptr + 2);
                       p_file_name[file_name_offset + 3] = *(ptr + 3);
                       p_file_name[file_name_offset + 4] = *(ptr + 4);
                       end_offset = 5;
                       ptr += 6;
                    }
                    /* SHDL */
               else if (((ptr + 12 - msg) < length) &&
                        (isupper((int)(*ptr))) && (isupper((int)(*(ptr + 1)))) &&
                        (isupper((int)(*(ptr + 2)))) &&
                        (isupper((int)(*(ptr + 3)))) && (*(ptr + 4) == ' ') &&
                        (isdigit((int)(*(ptr + 5)))) &&
                        (isdigit((int)(*(ptr + 6)))) &&
                        (isdigit((int)(*(ptr + 7)))) &&
                        (isdigit((int)(*(ptr + 8)))) &&
                        (isdigit((int)(*(ptr + 9)))) &&
                        (isdigit((int)(*(ptr + 10)))) &&
                        (isdigit((int)(*(ptr + 11)))))
                    {
                       ptr += 12;
                       while (((ptr - msg) < length) &&
                              ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
                       {
                          ptr++;
                       }
                       p_start = ptr;
                       p_file_name[file_name_offset] = *ptr;
                       p_file_name[file_name_offset + 1] = *(ptr + 1);
                       p_file_name[file_name_offset + 2] = *(ptr + 2);
                       p_file_name[file_name_offset + 3] = *(ptr + 3);
                       p_file_name[file_name_offset + 4] = *(ptr + 4);
                       end_offset = 5;
                    }
                    /* NIL */
               else if (((ptr + 4 - msg) < length) && (*ptr == 'N') &&
                        (*(ptr + 1) == 'I') && (*(ptr + 2) == 'L') &&
                        ((*(ptr + 3) == 13) || (*(ptr + 3) == 10)))
                    {
                       ptr += 4;
                       while (((ptr - msg) < length) &&
                              ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
                       {
                          ptr++;
                       }
                       continue;
                    }
                    /* NIL= */
               else if (((ptr + 5 - msg) < length) && (*ptr == 'N') &&
                        (*(ptr + 1) == 'I') && (*(ptr + 2) == 'L') &&
                        (*(ptr + 3) == '=') &&
                        ((*(ptr + 4) == 13) || (*(ptr + 4) == 10)))
                    {
                       ptr += 5;
                       while (((ptr - msg) < length) &&
                              ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
                       {
                          ptr++;
                       }
                       continue;
                    }
                    /* TAF NIL= */
               else if (((ptr + 9 - msg) < length) && (*ptr == 'T') &&
                        (*(ptr + 1) == 'A') && (*(ptr + 2) == 'F') &&
                        (*(ptr + 3) == ' ') && (*(ptr + 4) == 'N') &&
                        (*(ptr + 5) == 'I') && (*(ptr + 6) == 'L') &&
                        (*(ptr + 7) == '=') &&
                        ((*(ptr + 8) == 13) || (*(ptr + 8) == 10)))
                    {
                       ptr += 9;
                       while (((ptr - msg) < length) &&
                              ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
                       {
                          ptr++;
                       }
                       continue;
                    }
                    else
                    {
                       show_unknown_report(ptr, length, msg, job_id,
                                           __FILE__, __LINE__);
                       break;
                 }

               if (extract_p_filter != NULL)
               {
                  p_file_name[file_name_offset + end_offset] = '\0';
                  if (pmatch(extract_p_filter, p_file_name, NULL) != 0)
                  {
                     /* Ignore this report. */
                     not_wanted = YES;
                  }
                  else
                  {
                     not_wanted = NO;
                  }
               }
               while (((ptr - msg) < length) && (*ptr != '='))
               {
                  ptr++;
               }
               while (((ptr - msg) < length) && (*ptr == '='))
               {
                  ptr++;
               }
               while (((ptr - msg) < length) &&
                      ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
               {
                  ptr++;
               }
               if (not_wanted == NO)
               {
                  int additional_offset = 0;

                  if (extract_options & EXTRACT_ADD_ADDITIONAL_INFO)
                  {
                     char bulname[32];

                     if (bul_name_length < 32)
                     {
                        (void)memcpy(bulname, p_file_name, bul_name_length);
                        bulname[bul_name_length] = '\0';
                     }
                     else
                     {
                        receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                    _("bulname to short, should be %d bytes long. #%x"),
                                    bul_name_length, job_id);
                        bulname[0] = '\0';
                     }
                     additional_offset += snprintf(&p_file_name[file_name_offset + end_offset],
                                                   MAX_PATH_LENGTH - (file_name_offset + end_offset),
                                                   "#%s#%s",
                                                   bulname, p_orig_name);
                  }

                  size = ptr - p_start;
                  if (extract_options & EXTRACT_ADD_CRC_CHECKSUM)
                  {
                     additional_offset += snprintf(&p_file_name[file_name_offset + end_offset + additional_offset],
                                                   MAX_PATH_LENGTH - (file_name_offset + end_offset + additional_offset),
                                                   "-%x",
                                                   get_checksum_crc32c(INITIAL_CRC, p_start,
#ifdef HAVE_HW_CRC32
                                                                       (int)size, have_hw_crc32));
#else
                                                                       (int)size));
#endif
                  }
                  if (extract_options & EXTRACT_ADD_UNIQUE_NUMBER)
                  {
                     (void)next_counter(counter_fd, counter, MAX_MSG_PER_SEC);
                     additional_offset += snprintf(&p_file_name[file_name_offset + end_offset + additional_offset],
                                                   MAX_PATH_LENGTH - (file_name_offset + end_offset + additional_offset),
                                                   "-%04x", *counter);
                  }
                  p_file_name[file_name_offset + end_offset + additional_offset] = '\0';

                  if ((fd = open(p_full_file_name, (O_RDWR | O_CREAT | O_TRUNC),
                                 file_mode)) < 0)
                  {
                     receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                 _("Failed to open() `%s' while extracting reports : %s"),
                                 p_full_file_name, strerror(errno));
                     *p_file_name = '\0';
                     return(INCORRECT);
                  }
                  if (extract_options & EXTRACT_ADD_BUL_ORIG_FILE)
                  {
                     size_t length;

                     length = strlen(p_file_name);
                     if (writen(fd, p_file_name, length, 0) != (ssize_t)length)
                     {
                        receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                    _("Failed to writen() file name : %s"),
                                    strerror(errno));
                        (void)close(fd);
                        return(INCORRECT);
                     }
                     *p_file_size += length;
                     if (writen(fd, " ", (size_t)1, 0) != (ssize_t)1)
                     {
                        receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                    _("Failed to writen() space after file name : %s"),
                                    strerror(errno));
                        (void)close(fd);
                        return(INCORRECT);
                     }
                     *p_file_size += 1;
                     length = strlen(p_orig_name);
                     if (writen(fd, p_orig_name, length, 0) != (ssize_t)length)
                     {
                        receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                    _("Failed to writen() extract file name : %s"),
                                    strerror(errno));
                        (void)close(fd);
                        return(INCORRECT);
                     }
                     *p_file_size += length;
                     if (writen(fd, "\r\r\n", 3, 0) != 3)
                     {
                        receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                    _("Failed to writen() carriage return, carriage return + line feed : %s"),
                                    strerror(errno));
                        (void)close(fd);
                        return(INCORRECT);
                     }
                     *p_file_size += 2;
                  }
                  if (p_extra_heading != NULL)
                  {
                     if (writen(fd, p_extra_heading, (size_t)offset, 0) != (ssize_t)offset)
                     {
                        receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                    _("Failed to writen() extra header in report : %s"),
                                    strerror(errno));
                        (void)close(fd);
                        return(INCORRECT);
                     }
                     *p_file_size += offset;
                  }
                  if (writen(fd, p_start, (size_t)size, 0) != (ssize_t)size)
                  {
                     receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                 _("Failed to writen() report : %s"), strerror(errno));
                     (void)close(fd);
                     if (p_extra_heading != NULL)
                     {
                        *p_file_size -= offset;
                     }
                     return(INCORRECT);
                  }
                  (*p_files_to_send)++;
                  *p_file_size += size;

                  if (close(fd) == -1)
                  {
                     receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                                 _("close() error : %s"), strerror(errno));
                  }
#ifdef _PRODUCTION_LOG
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
                     pld[no_of_log_entries].size = size;
                     pld[no_of_log_entries].production_time = (times(&tval) - start_time) / (double)clktck;
                     get_sum_cpu_usage(&ru, &pld[no_of_log_entries].cpu_usage);
                     start_time = times(&tval);
                     (void)strcpy(pld[no_of_log_entries].file_name, p_file_name);
                     no_of_log_entries++;
                  }
#endif
               }
            } while ((ptr + 6 - msg) < length);
         }
         else
         {
            receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                        _("%s not marked as a report. #%x"),
                        p_file_name, job_id);
         }
      }
   }
   else
   {
      if (extract_p_filter != NULL)
      {
         p_file_name[i] = '\0';
         if (pmatch(extract_p_filter, p_file_name, NULL) != 0)
         {
            /* Ignore this bulletin. */
            return(SUCCESS);
         }
      }
      if (extract_options & EXTRACT_ADD_CRC_CHECKSUM)
      {
         i += snprintf(&p_file_name[i], MAX_PATH_LENGTH - i, "-%x",
                       get_checksum_crc32c(INITIAL_CRC, msg,
#ifdef HAVE_HW_CRC32
                                           length, have_hw_crc32));
#else
                                           length));
#endif
      }
      if (extract_options & EXTRACT_ADD_UNIQUE_NUMBER)
      {
         (void)next_counter(counter_fd, counter, MAX_MSG_PER_SEC);
         i += snprintf(&p_file_name[i], MAX_PATH_LENGTH - i, "-%04x", *counter);
      }
      p_file_name[i] = '\0';

      if ((fd = open(p_full_file_name, (O_WRONLY | O_CREAT | O_TRUNC),
                     file_mode)) < 0)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to open() `%s' (mode=%d) while extracting bulletins from `%s' : %s"),
                     p_full_file_name, file_mode, p_orig_name, strerror(errno));
         *p_file_name = '\0';
         return(INCORRECT);
      }

      if ((extract_options & EXTRACT_ADD_SOH_ETX) &&
          ((extract_options & EXTRACT_REMOVE_WMO_HEADER) == 0))
      {
         if (soh_etx == NO)
         {
            if (write(fd, "\001", 1) != 1)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           _("Failed to write() SOH : %s"), strerror(errno));
               (void)close(fd);
               return(INCORRECT);
            }
         }
         if (writen(fd, msg, (size_t)length, 0) != (ssize_t)length)
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                        _("Failed to writen() message : %s"), strerror(errno));
            (void)close(fd);
            return(INCORRECT);
         }
         if (soh_etx == NO)
         {
            if (write(fd, "\003", 1) != 1)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           _("Failed to write() ETX : %s"), strerror(errno));
               (void)close(fd);
               return(INCORRECT);
            }
            size = length + 2;
         }
         else
         {
            size = length;
         }
      }
      else
      {
         if (extract_options & EXTRACT_REMOVE_WMO_HEADER)
         {
            p_start = ptr;
            while ((*p_start == '\012') || (*p_start == '\015'))
            {
               p_start++;
            }
         }

         if (msg[length - 1] == 3)
         {
            length--;
            while ((length > 0) &&
                   ((msg[length - 1] == '\015') || (msg[length - 1] == '\012')))
            {
               length--;
            }
         }
         length -= (p_start - msg);
         if (writen(fd, p_start, (size_t)length, 0) != (ssize_t)length)
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                        _("Failed to writen() message : %s"), strerror(errno));
            (void)close(fd);
            return(INCORRECT);
         }
         size = length;
      }
      (*p_files_to_send)++;
      *p_file_size += size;

      if (close(fd) == -1)
      {
         receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                     _("close() error : %s"), strerror(errno));
      }
#ifdef _PRODUCTION_LOG
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
         pld[no_of_log_entries].size = size;
         pld[no_of_log_entries].production_time = (times(&tval) - start_time) / (double)clktck;
         get_sum_cpu_usage(&ru, &pld[no_of_log_entries].cpu_usage);
         start_time = times(&tval);
         (void)strcpy(pld[no_of_log_entries].file_name, p_file_name);
         no_of_log_entries++;
      }
#endif
   }
   *p_file_name = '\0';

   return(SUCCESS);
}


/*------------------------- get_rcdb_position() -------------------------*/
static int
get_rcdb_position(char *p_file_name, int bcdb_pos)
{
   int i;

   for (i = 0; i < no_of_rc_entries; i++)
   {
      if ((rcdb[i].TT[0] == p_file_name[0]) &&
          (rcdb[i].TT[1] == p_file_name[1]) &&
          (rcdb[i].rss == bcdb[bcdb_pos].rss))
      {
         return(i);
      }
   }

   return(-1);
}


/*---------------------------- find_offset() ----------------------------*/
static int
find_offset(int          rcdb_pos,
            char         *ptr,
            int          length,
            int          *offset,
            char         *p_wid,
            unsigned int job_id)
{
   char *p_start = ptr;

   /* Ignore any spaces at start. */
   while (((ptr - p_start) < length) && (*ptr == ' '))
   {
      ptr++;
   }
   if ((*ptr == 'N') && (*(ptr + 1) == 'I') && (*(ptr + 2) == 'L') &&
       ((*(ptr + 3) == 13) || (*(ptr + 3) == 10) || (*(ptr + 3) == '=')))
   {
      return(NIL_MESSAGE);
   }

   switch (rcdb[rcdb_pos].rt)
   {
      case RT_TEXT       : /* TEXT */
         return(TEXT_MESSAGE);

      case RT_CLIMAT     : /* CLIMAT */
         while (((ptr - p_start) < length) && (*ptr != 13) && (*ptr != 10))
         {
            ptr++;
         }
         if (rcdb[rcdb_pos].mimj[0] != '\0')
         {
            if (isdigit((int)(*(ptr - 1))))
            {
               *p_wid = *(ptr - 1);
               *(p_wid + 1) = '\0';
            }
         }
         while (((ptr - p_start) < length) &&
                ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
         {
            ptr++;
         }
         break;

      case RT_TAF        : /* TAF */
         if ((*ptr == 'T') && (*(ptr + 1) == 'A') && (*(ptr + 2) == 'F') &&
             (*(ptr + 3) == ' ') && (isdigit((int)(*(ptr + 4)))) &&
             (isdigit((int)(*(ptr + 5)))) && (isdigit((int)(*(ptr + 6)))) &&
             (isdigit((int)(*(ptr + 7)))) && (isdigit((int)(*(ptr + 8)))) &&
             (isdigit((int)(*(ptr + 9)))) && (*(ptr + 10) == 'Z') &&
             (*(ptr + 11) == 13) && (*(ptr + 12) == 13) &&
             (*(ptr + 13) == 10) && (*(ptr + 14) == 'T') &&
             (*(ptr + 15) == 'A') && (*(ptr + 16) == 'F') &&
             ((*(ptr + 17) == ' ') || (*(ptr + 17) == '\t') ||
              (*(ptr + 17) == 13)))
         {
            ptr += 14;
         }
         else
         {
            while (((ptr - p_start) < length) &&
                   ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
            {
               ptr++;
            }
         }
         break;

      case RT_SPECIAL_02 : /* SPECIAL-02 */
         while (((ptr - p_start) < length) && (*ptr != 13) && (*ptr != 10))
         {
            ptr++;
         }
         while (((ptr - p_start) < length) &&
                ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
         {
            ptr++;
         }
         break;

      case RT_METAR      : /* METAR */
      case RT_SPECIAL_01 : /* SPECIAL-01 */
      case RT_SPECIAL_03 : /* SPECIAL-03 */
      case RT_SPECIAL_66 : /* SPECIAL-66 */
      case RT_ATEXT      : /* AIR TEXT */
         while (((ptr - p_start) < length) &&
                ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
         {
            ptr++;
         }
         break;

      case RT_SYNOP      : /* SYNOP */
         if ((*ptr == 'A') && (*(ptr + 1) == 'A') && (*(ptr + 2) == 'X'))
         {
            if (*(ptr + 3) == 'X')
            {
               ptr += 4;
            }
            else
            {
               ptr += 3;
            }
            while (((ptr - p_start) < length) && (*ptr == ' '))
            {
               ptr++;
            }
            if ((isdigit((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))) &&
                (isdigit((int)(*(ptr + 2)))) && (isdigit((int)(*(ptr + 3)))) &&
                (isdigit((int)(*(ptr + 4)))) && (*(ptr + 5) == ' '))
            {
               *p_wid = *(ptr + 4);
               *(p_wid + 1) = '\0';
               ptr += 6;
            }
            else
            {
               while (((ptr - p_start) < length) && (*ptr != 13) &&
                      (*ptr != 10))
               {
                  ptr++;
               }
               if (isdigit((int)(*(ptr - 1))))
               {
                  *p_wid = *(ptr - 1);
                  *(p_wid + 1) = '\0';
               }
            }
         }
         while (((ptr - p_start) < length) &&
                ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
         {
            ptr++;
         }
         break;

      case RT_SYNOP_SHIP : /* SYNOP-SHIP */
         if (rcdb[rcdb_pos].mimj[1] == 'X')
         {
            while (((ptr - p_start) < length) && (*ptr != 13) && (*ptr != 10))
            {
               ptr++;
            }
            if (isdigit((int)(*(ptr - 1))))
            {
               *p_wid = *(ptr - 1);
               *(p_wid + 1) = '\0';
            }
         }
         while (((ptr - p_start) < length) &&
                ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
         {
            ptr++;
         }
         break;

      case RT_SYNOP_MOBIL : /* SYNOP-MOBIL */
         while (((ptr - p_start) < length) && (*ptr != 13) && (*ptr != 10))
         {
            ptr++;
         }
         while (((ptr - p_start) < length) &&
                ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
         {
            ptr++;
         }
         break;

      case RT_UPPER_AIR  : /* UPPER-AIR */
         while (((ptr - p_start) < length) &&
                ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
         {
            ptr++;
         }
         break;

      default            : /* Unknown report type */
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     "Unknown report type %d (%d), unable to extract reports. #%x",
                     rcdb[rcdb_pos].rt, rcdb_pos, job_id);
         return(INCORRECT);
   }
   *offset = ptr - p_start;

   return(SUCCESS);
}


/*-------------------------- get_station_id() ---------------------------*/
static int
get_station_id(char *file_name,
               int  file_name_offset,
               int  *station_id_length,
               char *ptr,
               int  msg_length,
               int  rcdb_pos,
               int  begin_file_name_offset,
               char **p_extra_heading,
               int  *offset,
               int  *overwrite_extra_heading,
               char *p_wid)
{
   char *p_start = ptr,
        *station_id = &file_name[file_name_offset];

   *overwrite_extra_heading = 0;
   switch (rcdb[rcdb_pos].rt)
   {
      case RT_TEXT       : /* TEXT */
         station_id[0] = '\0';
         receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                     "%s: TEXT in %s still needs to be done!",
                     p_file_name, p_orig_name);
         hex_print(WARN_SIGN, p_start, msg_length);
         return(INCORRECT);

      case RT_ATEXT      : /* AIR TEXT */
         if ((isupper((int)(*ptr))) && (isupper((int)(*(ptr + 1)))) &&
             (isupper((int)(*(ptr + 2)))) &&
             (isalnum((int)(*(ptr + 3)))) &&
             ((*(ptr + 4) == ' ') || (*(ptr + 4) == '\t')))
         {
            station_id[0] = *ptr;
            station_id[1] = *(ptr + 1);
            station_id[2] = *(ptr + 2);
            station_id[3] = *(ptr + 3);
            *station_id_length = 4;
         }
         else
         {
            station_id[0] = '\0';
            if ((*ptr == 'N') &&
                (((*(ptr + 1) == 'I') && (*(ptr + 2) == 'L')) ||
                 (((*(ptr + 1) == 'N') || (*(ptr + 1) == 'n')) &&
                  ((*(ptr + 2) == 'N') || (*(ptr + 2) == 'n')) &&
                  ((*(ptr + 3) == 'N') || (*(ptr + 3) == 'n')))))
            {
               /* Do not warn if we find NIL or NNNN. */;
            }
            else
            {
               receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                           "%s: Unknown AIR TEXT in %s. If this is a correct report, contact maintainer %s (pos=%d)",
                           p_file_name, p_orig_name, AFD_MAINTAINER, rcdb_pos);
               hex_print(WARN_SIGN, p_start, msg_length);
            }
            return(INCORRECT);
         }
         break;

      case RT_CLIMAT     : /* CLIMAT */
         if ((isdigit((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))) &&
             (isdigit((int)(*(ptr + 2)))) && (isdigit((int)(*(ptr + 3)))) &&
             (isdigit((int)(*(ptr + 4)))) &&
             ((*(ptr + 5) == ' ') || (*(ptr + 5) == 13) || (*(ptr + 5) == 10)))
         {
            station_id[0] = *ptr;
            station_id[1] = *(ptr + 1);
            station_id[2] = *(ptr + 2);
            station_id[3] = *(ptr + 3);
            station_id[4] = *(ptr + 4);
            *station_id_length = 5;
         }
         else
         {
            station_id[0] = '\0';
            if ((*ptr == 'N') &&
                (((*(ptr + 1) == 'I') && (*(ptr + 2) == 'L')) ||
                 (((*(ptr + 1) == 'N') || (*(ptr + 1) == 'n')) &&
                  ((*(ptr + 2) == 'N') || (*(ptr + 2) == 'n')) &&
                  ((*(ptr + 3) == 'N') || (*(ptr + 3) == 'n')))))
            {
               /* Do not warn if we find NIL or NNNN. */;
            }
            else
            {
               receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                           "%s: Unknown CLIMAT in %s. If this is a correct report, contact maintainer %s (pos=%d)",
                           p_file_name, p_orig_name, AFD_MAINTAINER, rcdb_pos);
               hex_print(WARN_SIGN, p_start, msg_length);
            }
            return(INCORRECT);
         }
         break;

      case RT_TAF        : /* TAF */

         /*
          * Possible cases: 1) TAF STID ...=
          *                 2) TAF AMD STID ...=
          *                 3) TAF COR STID ...=
          *                 4) TAF<cr><cr><lf>
          *                    STID ...=
          *                    STID ...=
          */
         if ((*ptr == 'T') && (*(ptr + 1) == 'A') && (*(ptr + 2) == 'F') &&
             ((*(ptr + 3) == ' ') || (*(ptr + 3) == '\t') ||
              (*(ptr + 3) == 13)))
         {
            if ((*(ptr + 3) == 13) && (*(ptr + 4) == 13) &&
                (*(ptr + 5) == 10) && (*(ptr + 6) == 'T') &&
                (*(ptr + 7) == 'A') && (*(ptr + 8) == 'F') &&
                ((*(ptr + 9) == ' ') || (*(ptr + 9) == '\t')))
            {
               ptr += 6;
            }
            else if ((*(ptr + 3) == ' ') &&
                     (((*(ptr + 4) == 'A') && (*(ptr + 5) == 'M') && (*(ptr + 6) == 'D')) ||
                      ((*(ptr + 4) == 'C') && (*(ptr + 5) == 'O') && (*(ptr + 6) == 'R'))) &&
                     (*(ptr + 7) == 13) && (*(ptr + 8) == 13) &&
                     (*(ptr + 9) == 10) && (*(ptr + 10) == 'T') &&
                     (*(ptr + 11) == 'A') && (*(ptr + 12) == 'F'))
                 {
                    ptr += 9;
                 }
            ptr += 4;
            while ((*ptr == 13) || (*ptr == 10))
            {
               ptr++;
            }
            while ((*ptr == ' ') || (*ptr == '\t'))
            {
               ptr++;
            }
            if ((((*ptr == 'A') && (*(ptr + 1) == 'M') && (*(ptr + 2) == 'D')) ||
                 ((*ptr == 'C') && (*(ptr + 1) == 'O') && (*(ptr + 2) == 'R'))) &&
                ((*(ptr + 3) == ' ') || (*(ptr + 3) == '\t') ||
                 (*(ptr + 3) == 13)))
            {
               ptr += 4;
            }
            while ((*ptr == 13) || (*ptr == 10))
            {
               ptr++;
            }
         }
         else if ((*ptr == 'A') && (*(ptr + 1) == 'M') && (*(ptr + 2) == 'D') &&
                  ((*(ptr + 3) == ' ') || (*(ptr + 3) == '\t')))
              {
                 ptr += 4;
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
              }

         if ((isupper((int)(*ptr))) && (isupper((int)(*(ptr + 1)))) &&
             (isupper((int)(*(ptr + 2)))) &&
             (isalnum((int)(*(ptr + 3)))) &&
             ((*(ptr + 4) == ' ') || (*(ptr + 4) == '\t')))
         {
            station_id[0] = *ptr;
            station_id[1] = *(ptr + 1);
            station_id[2] = *(ptr + 2);
            station_id[3] = *(ptr + 3);
            *station_id_length = 4;
         }
         else
         {
            station_id[0] = '\0';
            if (((*ptr == 'N') &&
                 (((*(ptr + 1) == 'I') && (*(ptr + 2) == 'L')) ||
                  (((*(ptr + 1) == 'N') || (*(ptr + 1) == 'n')) &&
                   ((*(ptr + 2) == 'N') || (*(ptr + 2) == 'n')) &&
                   ((*(ptr + 3) == 'N') || (*(ptr + 3) == 'n'))))) ||
                ((*ptr == '/') && (*(ptr + 1) == '/') &&
                 ((*(ptr + 2) == ' ') || (*(ptr + 2) == 'E'))))
            {
               /* Do not warn if we find NIL or NNNN. */;
            }
            else
            {
               receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                           "%s: Unknown TAF in %s. If this is a correct report, contact maintainer %s (pos=%d)",
                           p_file_name, p_orig_name, AFD_MAINTAINER, rcdb_pos);
               hex_print(WARN_SIGN, p_start, msg_length);
            }
            return(INCORRECT);
         }
         break;

      case RT_METAR      : /* METAR or SPECI */

         /*
          * Possible cases: 1) METAR STID ...=
          *                 2) METAR COR STID ...=
          *                 3) METAR RRA STID ...=
          *                 4) METAR<cr><cr><lf>
          *                    STID ...=
          *                    STID ...=
          */
         if ((((*ptr == 'M') && (*(ptr + 1) == 'E') && (*(ptr + 2) == 'T') &&
               (*(ptr + 3) == 'A') && (*(ptr + 4) == 'R')) ||
              ((*ptr == 'S') && (*(ptr + 1) == 'P') && (*(ptr + 2) == 'E') &&
               (*(ptr + 3) == 'C') && (*(ptr + 4) == 'I'))) &&
             ((*(ptr + 5) == ' ') || (*(ptr + 5) == '\t') ||
              (*(ptr + 5) == 13)))
         {
            ptr += 6;
            while ((*ptr == ' ') || (*ptr == '\t'))
            {
               ptr++;
            }
            if ((((*ptr == 'M') && (*(ptr + 1) == 'E') && (*(ptr + 2) == 'T') &&
                  (*(ptr + 3) == 'A') && (*(ptr + 4) == 'R')) ||
                 ((*ptr == 'S') && (*(ptr + 1) == 'P') && (*(ptr + 2) == 'E') &&
                  (*(ptr + 3) == 'C') && (*(ptr + 4) == 'I'))) &&
                ((*(ptr + 5) == ' ') || (*(ptr + 5) == '\t')))
            {
               ptr += 6;
               while ((*ptr == ' ') || (*ptr == '\t'))
               {
                  ptr++;
               }
            }
            if ((isdigit((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))) &&
                (isdigit((int)(*(ptr + 2)))) &&
                (isdigit((int)(*(ptr + 3)))) &&
                (isdigit((int)(*(ptr + 4)))) &&
                (isdigit((int)(*(ptr + 5)))) && (*(ptr + 6) == 'Z') &&
                (*(ptr + 7) == 13) && (*(ptr + 8) == 13) &&
                (*(ptr + 9) == 10) && 
                (((*(ptr + 10) == 'M') && (*(ptr + 11) == 'E') &&
                  (*(ptr + 12) == 'T') && (*(ptr + 13) == 'A') &&
                  (*(ptr + 14) == 'R')) ||
                 ((*(ptr + 10) == 'S') && (*(ptr + 11) == 'P') &&
                  (*(ptr + 12) == 'E') && (*(ptr + 13) == 'C') &&
                  (*(ptr + 14) == 'I'))) &&
                ((*(ptr + 15) == ' ') || (*(ptr + 15) == '\t') ||
                 (*(ptr + 15) == 13)))
            {
               ptr += 16;
            }
            else if ((isdigit((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))) &&
                     (isdigit((int)(*(ptr + 2)))) &&
                     (isdigit((int)(*(ptr + 3)))) &&
                     (isdigit((int)(*(ptr + 4)))) &&
                     (isdigit((int)(*(ptr + 5)))) && (*(ptr + 6) == 'Z') &&
                     (*(ptr + 7) == 13) && (*(ptr + 8) == 13) &&
                     (*(ptr + 9) == 10) && (*(ptr + 10) == 13) &&
                     (*(ptr + 11) == 13) && (*(ptr + 12) == 10) &&
                     (((*(ptr + 13) == 'M') && (*(ptr + 14) == 'E') &&
                       (*(ptr + 15) == 'T') && (*(ptr + 16) == 'A') &&
                       (*(ptr + 17) == 'R')) ||
                      ((*(ptr + 13) == 'S') && (*(ptr + 14) == 'P') &&
                       (*(ptr + 15) == 'E') && (*(ptr + 16) == 'C') &&
                       (*(ptr + 17) == 'I'))) &&
                     ((*(ptr + 18) == ' ') ||
                     (*(ptr + 18) == '\t') || (*(ptr + 18) == 13)))
                 {
                    ptr += 19;
                 }
            while ((*ptr == ' ') || (*ptr == '\t'))
            {
               ptr++;
            }
            if ((((*ptr == 'C') && (*(ptr + 1) == 'O') && (*(ptr + 2) == 'R')) ||
                 ((*ptr == 'R') && (*(ptr + 1) == 'R') && (*(ptr + 2) == 'A'))) &&
                ((*(ptr + 3) == ' ') || (*(ptr + 3) == '\t') ||
                 (*(ptr + 3) == 13)))
            {
               ptr += 4;
            }
            while ((*ptr == 13) || (*ptr == 10))
            {
               ptr++;
            }
         }
         if ((rcdb[rcdb_pos].stid == STID_CCCC) &&
             (isupper((int)(*ptr))) && (isalnum((int)(*(ptr + 1)))) &&
             (isalnum((int)(*(ptr + 2)))) &&
             (isalnum((int)(*(ptr + 3)))) &&
             ((*(ptr + 4) == ' ') || (*(ptr + 4) == '\t')))
         {
            station_id[0] = *ptr;
            station_id[1] = *(ptr + 1);
            station_id[2] = *(ptr + 2);
            station_id[3] = *(ptr + 3);
            *station_id_length = 4;
         }
         else if ((rcdb[rcdb_pos].stid == STID_IIiii) &&
                  (isdigit((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))) &&
                  (isdigit((int)(*(ptr + 2)))) &&
                  (isdigit((int)(*(ptr + 3)))) &&
                  (isdigit((int)(*(ptr + 4)))) &&
                  ((*(ptr + 5) == ' ') || (*(ptr + 5) == '\t')))
              {
                 station_id[0] = *ptr;
                 station_id[1] = *(ptr + 1);
                 station_id[2] = *(ptr + 2);
                 station_id[3] = *(ptr + 3);
                 station_id[4] = *(ptr + 4);
                 *station_id_length = 5;
              }
              else
              {
                 station_id[0] = '\0';
                 if (((*ptr == 'N') &&
                      (((*(ptr + 1) == 'I') && (*(ptr + 2) == 'L')) ||
                       (((*(ptr + 1) == 'N') || (*(ptr + 1) == 'n')) &&
                        ((*(ptr + 2) == 'N') || (*(ptr + 2) == 'n')) &&
                        ((*(ptr + 3) == 'N') || (*(ptr + 3) == 'n'))))) ||
                     ((*p_start == 'M') && (*(p_start + 1) == 'E') &&
                      (*(p_start + 2) == 'T') && (*(p_start + 3) == 'A') &&
                      (*(p_start + 4) == 'R') && (*(p_start + 5) == ' ') &&
                      (isdigit((int)(*(p_start + 6)))) &&
                      (isdigit((int)(*(p_start + 7)))) &&
                      (isdigit((int)(*(p_start + 8)))) &&
                      (isdigit((int)(*(p_start + 9)))) &&
                      (isdigit((int)(*(p_start + 10)))) &&
                      (isdigit((int)(*(p_start + 11)))) &&
                      (*(p_start + 12) == 'Z') && (*(p_start + 13) == 13)))
                 {
                    /* Do not warn if we find NIL, NNNN or METAR xxxxxxZ. */;
                 }
                 else
                 {
                    receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                "%s: Unknown METAR or SPECI in %s. If this is a correct report, contact maintainer %s (pos=%d)",
                                p_file_name, p_orig_name, AFD_MAINTAINER,
                                rcdb_pos);
                    hex_print(WARN_SIGN, p_start, msg_length);
                 }
                 return(INCORRECT);
              }
         break;

      case RT_SPECIAL_01 : /* SPECIAL-01 */
         station_id[0] = '\0';
         receive_log(INFO_SIGN, __FILE__, __LINE__, 0L,
                     "%s: SPECIAL-01 in %s still needs to be done!",
                     p_file_name, p_orig_name);
         hex_print(WARN_SIGN, p_start, msg_length);
         return(INCORRECT);

      case RT_SPECIAL_02 : /* SPECIAL-02 */
         if ((isalpha((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))) &&
             (isdigit((int)(*(ptr + 2)))) && (isdigit((int)(*(ptr + 3)))) &&
             ((*(ptr + 4) == ' ') || (*(ptr + 4) == 13) || (*(ptr + 4) == 10)))
         {
            station_id[0] = *ptr;
            station_id[1] = *(ptr + 1);
            station_id[2] = *(ptr + 2);
            station_id[3] = *(ptr + 3);
            *station_id_length = 4;
         }
         else
         {
            station_id[0] = '\0';
            if ((*ptr == 'N') &&
                (((*(ptr + 1) == 'I') && (*(ptr + 2) == 'L')) ||
                 (((*(ptr + 1) == 'N') || (*(ptr + 1) == 'n')) &&
                  ((*(ptr + 2) == 'N') || (*(ptr + 2) == 'n')) &&
                  ((*(ptr + 3) == 'N') || (*(ptr + 3) == 'n')))))
            {
               /* Do not warn if we find NIL or NNNN. */;
            }
            else
            {
               receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                           "%s: Unknown SPECIAL-02 in %s. If this is a correct report, contact maintainer %s (pos=%d)",
                           p_file_name, p_orig_name, AFD_MAINTAINER, rcdb_pos);
               hex_print(WARN_SIGN, p_start, msg_length);
            }
            return(INCORRECT);
         }
         break;

      case RT_SPECIAL_03 : /* SPECIAL-03 */
         station_id[0] = '\0';
         receive_log(INFO_SIGN, __FILE__, __LINE__, 0L,
                     "%s: SPECIAL-03 in %s still needs to be done!",
                     p_file_name, p_orig_name);
         hex_print(WARN_SIGN, p_start, msg_length);
         return(INCORRECT);

      case RT_SPECIAL_66 : /* SPECIAL-66 */
         station_id[0] = '\0';
         receive_log(INFO_SIGN, __FILE__, __LINE__, 0L,
                     "%s: SPECIAL-66 in %s still needs to be done!",
                     p_file_name, p_orig_name);
         hex_print(WARN_SIGN, p_start, msg_length);
         return(INCORRECT);

      case RT_SYNOP      : /* SYNOP */

         /* This check is in case a new heading appears. */
         if ((isalnum((int)(*ptr))) && (isalnum((int)(*(ptr + 1)))) &&
             (isalnum((int)(*(ptr + 2)))) && (isalnum((int)(*(ptr + 3)))) &&
             (isalnum((int)(*(ptr + 4)))) && (isalnum((int)(*(ptr + 5)))) &&
             (*(ptr + 6) == ' ') && (isupper((int)(*(ptr + 7)))) &&
             (isupper((int)(*(ptr + 8)))) && (isupper((int)(*(ptr + 9)))) &&
             (isupper((int)(*(ptr + 10)))) && (*(ptr + 11) == ' ') &&
             (isdigit((int)(*(ptr + 12)))) && (isdigit((int)(*(ptr + 13)))) &&
             (isdigit((int)(*(ptr + 14)))) && (isdigit((int)(*(ptr + 15)))) &&
             (isdigit((int)(*(ptr + 16)))) && (isdigit((int)(*(ptr + 17)))) &&
             (*(ptr + 18) == 13) && (*(ptr + 19) == 13) &&
             (*(ptr + 20) == 10))
         {
            int i;

            /*
             * FIXME!
             * In most cases this should work. However if the length
             * of the filename does differ we have a problem and we
             * need to handle this! Lets first see how often this happens
             * in the real world.
             */
            p_file_name[0] = *ptr;
            p_file_name[1] = *(ptr + 1);
            p_file_name[2] = *(ptr + 2);
            p_file_name[3] = *(ptr + 3);
            p_file_name[4] = *(ptr + 4);
            p_file_name[5] = *(ptr + 5);
            p_file_name[6] = '_';
            p_file_name[7] = *(ptr + 7);
            p_file_name[8] = *(ptr + 8);
            p_file_name[9] = *(ptr + 9);
            p_file_name[10] = *(ptr + 10);
            p_file_name[11] = '_';
            p_file_name[12] = *(ptr + 12);
            p_file_name[13] = *(ptr + 13);
            p_file_name[14] = *(ptr + 14);
            p_file_name[15] = *(ptr + 15);
            p_file_name[16] = *(ptr + 16);
            p_file_name[17] = *(ptr + 17);
            ptr += 21;
            if (begin_file_name_offset != -1)
            {
               if ((*ptr == 'A') && (*(ptr + 1) == 'A') && (*(ptr + 2) == 'X'))
               {
                  int j;

                  p_file_name[18] = '-';
                  p_file_name[19] = 'A';
                  p_file_name[20] = 'A';
                  p_file_name[21] = 'X';
                  if (*(ptr + 3) == 'X')
                  {
                     p_file_name[22] = 'X';
                     i = 23;
                     j = 4;
                  }
                  else
                  {
                     i = 22;
                     j = 3;
                  }
                  if ((*(ptr + j) == ' ') &&
                      (isdigit((int)(*(ptr + j + 1)))) &&
                      (isdigit((int)(*(ptr + j + 2)))) &&
                      (isdigit((int)(*(ptr + j + 3)))) &&
                      (isdigit((int)(*(ptr + j + 4)))) &&
                      (isdigit((int)(*(ptr + j + 5)))))
                  {
                     p_file_name[i] = '_';
                     p_file_name[i + 1] = *(ptr + j + 1);
                     p_file_name[i + 2] = *(ptr + j + 2);
                     p_file_name[i + 3] = *(ptr + j + 3);
                     p_file_name[i + 4] = *(ptr + j + 4);
                     p_file_name[i + 5] = *(ptr + j + 5);
                     i += 6;
                     *p_wid = *(ptr + j + 5);
                     *(p_wid + 1) = '\0';
                  }
               }
               else
               {
                  i = 18;
               }
            }
            else
            {
               i = 18;
            }
            if ((i + 1) != file_name_offset)
            {
               receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                           "File name (%s) will be wrong, we need to fix this! Source file is %s. (%d!=%d)",
                           p_file_name, p_orig_name, i + 1, file_name_offset);
               hex_print(WARN_SIGN, ptr, msg_length);
            }
         }
         if ((isdigit((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))) &&
             (isdigit((int)(*(ptr + 2)))) && (isdigit((int)(*(ptr + 3)))) &&
             (isdigit((int)(*(ptr + 4)))) &&
             ((*(ptr + 5) == ' ') || (*(ptr + 5) == 13) || (*(ptr + 5) == 10)))
         {
            station_id[0] = *ptr;
            station_id[1] = *(ptr + 1);
            station_id[2] = *(ptr + 2);
            station_id[3] = *(ptr + 3);
            station_id[4] = *(ptr + 4);
            *station_id_length = 5;
         }
              /* Heading apears again. */
         else if ((p_file_name[0] == *ptr) && (p_file_name[1] == *(ptr + 1)) &&
                  (p_file_name[2] == *(ptr + 2)) &&
                  (p_file_name[3] == *(ptr + 3)) &&
                  (p_file_name[4] == *(ptr + 4)) &&
                  (p_file_name[5] == *(ptr + 5)) &&
                  (p_file_name[6] == '_') && (*(ptr + 6) == ' ') &&
                  (p_file_name[7] == *(ptr + 7)) &&
                  (p_file_name[8] == *(ptr + 8)) &&
                  (p_file_name[9] == *(ptr + 9)) &&
                  (p_file_name[10] == *(ptr + 10)) &&
                  (p_file_name[11] == '_') && (*(ptr + 11) == ' '))
              {
                 ptr += 11;
                 while (((ptr - p_start) < msg_length) && (*ptr != 13) &&
                        (*ptr != 10))
                 {
                    ptr++;
                 }
                 while (((ptr - p_start) < msg_length) &&
                        ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
                 {
                    ptr++;
                 }
                 if ((*ptr == 'A') && (*(ptr + 1) == 'A') &&
                     (*(ptr + 2) == 'X'))
                 {
                    char *tmp_ptr = ptr;

                    if (begin_file_name_offset != -1)
                    {
                       int space_count = 0;

                       while (((ptr - p_start) < msg_length) &&
                              (*ptr != 13) && (*ptr != 10))
                       {
                          if (*ptr == ' ')
                          {
                             if (space_count == 0)
                             {
                                p_file_name[begin_file_name_offset] = '_';
                                space_count++;
                             }
                             else
                             {
                                break;
                             }
                          }
                          else if (*ptr == '/')
                               {
                                  p_file_name[begin_file_name_offset] = '?';
                               }
                               else
                               {
                                  p_file_name[begin_file_name_offset] = *ptr;
                               }
                          begin_file_name_offset++;
                          ptr++;
                       }
                    }
                    else
                    {
                       if (*(ptr + 3) == 'X')
                       {
                          ptr += 4;
                       }
                       else
                       {
                          ptr += 3;
                       }
                       while (((ptr - p_start) < msg_length) && (*ptr != 13) &&
                              (*ptr != 10))
                       {
                          ptr++;
                       }
                    }
                    if (isdigit((int)(*(ptr - 1))))
                    {
                       *p_wid = *(ptr - 1);
                       *(p_wid + 1) = '\0';
                    }
                    if (offset != NULL)
                    {
                       *p_extra_heading = tmp_ptr;
                       *offset = ptr - *p_extra_heading;
                       if (*ptr == ' ')
                       {
                          *overwrite_extra_heading = *offset;
                       }
                    }
                 }
                 while (((ptr - p_start) < msg_length) &&
                        ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
                 {
                    ptr++;
                 }
                 if ((isdigit((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))) &&
                     (isdigit((int)(*(ptr + 2)))) && (isdigit((int)(*(ptr + 3)))) &&
                     (isdigit((int)(*(ptr + 4)))) &&
                     ((*(ptr + 5) == ' ') || (*(ptr + 5) == 13) || (*(ptr + 5) == 10)))
                 {
                    station_id[0] = *ptr;
                    station_id[1] = *(ptr + 1);
                    station_id[2] = *(ptr + 2);
                    station_id[3] = *(ptr + 3);
                    station_id[4] = *(ptr + 4);
                    *station_id_length = 5;
                 }
                 else
                 {
                    station_id[0] = '\0';
                    if ((*ptr == 'N') &&
                        (((*(ptr + 1) == 'I') && (*(ptr + 2) == 'L')) ||
                         (((*(ptr + 1) == 'N') || (*(ptr + 1) == 'n')) &&
                          ((*(ptr + 2) == 'N') || (*(ptr + 2) == 'n')) &&
                          ((*(ptr + 3) == 'N') || (*(ptr + 3) == 'n')))))
                    {
                       /* Do not warn if we find NIL or NNNN. */;
                    }
                    else
                    {
                       receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                   "%s: Unknown SYNOP in %s. If this is a correct report, contact maintainer %s (pos=%d)",
                                   p_file_name, p_orig_name, AFD_MAINTAINER,
                                   rcdb_pos);
                       hex_print(WARN_SIGN, p_start, msg_length);
                    }
                    return(INCORRECT);
                 }
              }
              /* Additional AAXX. */
         else if ((*ptr == 'A') && (*(ptr + 1) == 'A') &&
                  (*(ptr + 2) == 'X') && (*(ptr + 3) == 'X') &&
                  (*(ptr + 4) == ' ') && (isdigit((int)(*(ptr + 5)))) &&
                  (isdigit((int)(*(ptr + 6)))) &&
                  (isdigit((int)(*(ptr + 7)))) &&
                  (isdigit((int)(*(ptr + 8)))) &&
                  (isdigit((int)(*(ptr + 9)))) &&
                  ((*(ptr + 10) == 13) || (*(ptr + 10) == 10) ||
                   (*(ptr + 10) == ' ')))
              {
                 char *tmp_ptr = ptr;

                 if (begin_file_name_offset != -1)
                 {
                    int space_count = 0;

                    while (((ptr - p_start) < msg_length) &&
                           (*ptr != 13) && (*ptr != 10))
                    {
                       if (*ptr == ' ')
                       {
                          if (space_count == 0)
                          {
                             p_file_name[begin_file_name_offset] = '_';
                             space_count++;
                          }
                          else
                          {
                             break;
                          }
                       }
                       else if (*ptr == '/')
                            {
                               p_file_name[begin_file_name_offset] = '?';
                            }
                            else
                            {
                               p_file_name[begin_file_name_offset] = *ptr;
                            }
                       begin_file_name_offset++;
                       ptr++;
                    }
                 }
                 else
                 {
                    while (((ptr - p_start) < msg_length) && (*ptr != 13) &&
                           (*ptr != 10))
                    {
                       ptr++;
                    }
                 }
                 if (isdigit((int)(*(ptr - 1))))
                 {
                    *p_wid = *(ptr - 1);
                    *(p_wid + 1) = '\0';
                 }

                 while (((ptr - p_start) < msg_length) &&
                        ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
                 {
                    ptr++;
                 }
                 if (offset != NULL)
                 {
                    *p_extra_heading = tmp_ptr;
                    *offset = ptr - *p_extra_heading;
                    *overwrite_extra_heading = *offset;
                 }
                 if ((isdigit((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))) &&
                     (isdigit((int)(*(ptr + 2)))) &&
                     (isdigit((int)(*(ptr + 3)))) &&
                     (isdigit((int)(*(ptr + 4)))) &&
                     ((*(ptr + 5) == ' ') || (*(ptr + 5) == 13) || (*(ptr + 5) == 10)))
                 {
                    station_id[0] = *ptr;
                    station_id[1] = *(ptr + 1);
                    station_id[2] = *(ptr + 2);
                    station_id[3] = *(ptr + 3);
                    station_id[4] = *(ptr + 4);
                    *station_id_length = 5;
                 }
                 else
                 {
                    station_id[0] = '\0';
                    if ((*ptr == 'N') &&
                        (((*(ptr + 1) == 'I') && (*(ptr + 2) == 'L')) ||
                         (((*(ptr + 1) == 'N') || (*(ptr + 1) == 'n')) &&
                          ((*(ptr + 2) == 'N') || (*(ptr + 2) == 'n')) &&
                          ((*(ptr + 3) == 'N') || (*(ptr + 3) == 'n')))))
                    {
                       /* Do not warn if we find NIL or NNNN. */;
                    }
                    else
                    {
                       receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                   "%s: Unknown SYNOP in %s. If this is a correct report, contact maintainer %s (pos=%d)",
                                   p_file_name, p_orig_name, AFD_MAINTAINER,
                                   rcdb_pos);
                       hex_print(WARN_SIGN, p_start, msg_length);
                    }
                    return(INCORRECT);
                 }
              }
              else
              {
                 station_id[0] = '\0';
                 if (((*ptr == 'N') &&
                      (((*(ptr + 1) == 'I') && (*(ptr + 2) == 'L')) ||
                       (((*(ptr + 1) == 'N') || (*(ptr + 1) == 'n')) &&
                        ((*(ptr + 2) == 'N') || (*(ptr + 2) == 'n')) &&
                        ((*(ptr + 3) == 'N') || (*(ptr + 3) == 'n'))))) ||
                     ((*ptr == '/') && (*(ptr + 1) == '/') &&
                      ((*(ptr + 2) == ' ') || (*(ptr + 2) == 'E'))))
                 {
                    /* Do not warn if we find NIL or NNNN or */
                    /* // END OF ...                         */;
                 }
                 else
                 {
                    receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                "%s: Unknown SYNOP in %s. If this is a correct report, contact maintainer %s (pos=%d) %c%c%c%c",
                                p_file_name, p_orig_name, AFD_MAINTAINER,
                                rcdb_pos, *ptr, *(ptr+1), *(ptr+2), *(ptr+3));
                    hex_print(WARN_SIGN, p_start, msg_length);
                 }
                 return(INCORRECT);
              }
         break;

      case RT_SYNOP_SHIP : /* SYNOP-SHIP */

         if (rcdb[rcdb_pos].mimj[0] == 'B')
         {
            /* First store the ship idendifier (up to 10 characters     */
            /* starting with characters, numbers + ending with numbers) */
            if (isalnum((int)(*ptr)))
            {
               int i = 1;

               station_id[0] = *ptr;
               ptr++;
               while ((i < 10) && (isalnum((int)(*ptr))))
               {
                  station_id[i] = *ptr;
                  i++; ptr++;
               }
               if ((i == 10) && (isalnum((int)(*ptr))))
               {
                  station_id[i] = '\0';
                  receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                              "%s: SHIP identifier in %s more than 10 characters. Ignoring them.",
                              p_file_name, p_orig_name);
                  while (isalnum((int)(*ptr)))
                  {
                     ptr++;
                  }
               }

               while (*ptr == ' ')
               {
                  ptr++;
               }

               /* Now lets try and find the locasion of the ship. */
               if ((isdigit((int)(*ptr))) &&
                   (isdigit((int)(*(ptr + 1)))) &&
                   (isdigit((int)(*(ptr + 2)))) &&
                   (isdigit((int)(*(ptr + 3)))) &&
                   (isdigit((int)(*(ptr + 4)))) &&
                   (*(ptr + 5) == ' ') &&
                   (isdigit((int)(*(ptr + 6)))) &&
                   (isdigit((int)(*(ptr + 7)))) &&
                   (isdigit((int)(*(ptr + 8)))) &&
                   (isdigit((int)(*(ptr + 9)))) &&
                   (isdigit((int)(*(ptr + 10)))) &&
                   (*(ptr + 11) == ' ') &&
                   (isdigit((int)(*(ptr + 12)))) &&
                   (isdigit((int)(*(ptr + 13)))) &&
                   (isdigit((int)(*(ptr + 14)))) &&
                   (isdigit((int)(*(ptr + 15)))) &&
                   (isdigit((int)(*(ptr + 16)))) &&
                   ((*(ptr + 17) == ' ') || (*(ptr + 17) == 13) ||
                    (*(ptr + 17) == 10)))
               {
                  station_id[i] = '_';
                  station_id[i + 1] = *(ptr + 6);
                  station_id[i + 2] = *(ptr + 7);
                  station_id[i + 3] = *(ptr + 8);
                  station_id[i + 4] = *(ptr + 9);
                  station_id[i + 5] = *(ptr + 10);
                  station_id[i + 6] = '_';
                  station_id[i + 7] = *(ptr + 12);
                  station_id[i + 8] = *(ptr + 13);
                  station_id[i + 9] = *(ptr + 14);
                  station_id[i + 10] = *(ptr + 15);
                  station_id[i + 11] = *(ptr + 16);
                  *station_id_length = i + 12;
               }
               else
               {
                  *station_id_length = i;
                  station_id[i] = '\0';
                  receive_log(INFO_SIGN, __FILE__, __LINE__, 0L,
                              "%s: Unable to find location of SHIP in %s, ignoring location.",
                              p_file_name, p_orig_name);
                  hex_print(INFO_SIGN, p_start, msg_length);
               }
            }
            else
            {
               station_id[0] = '\0';
               if ((*ptr == 'N') &&
                   (((*(ptr + 1) == 'I') && (*(ptr + 2) == 'L')) ||
                    (((*(ptr + 1) == 'N') || (*(ptr + 1) == 'n')) &&
                     ((*(ptr + 2) == 'N') || (*(ptr + 2) == 'n')) &&
                     ((*(ptr + 3) == 'N') || (*(ptr + 3) == 'n')))))
               {
                  /* Do not warn if we find NIL or NNNN. */;
               }
               else
               {
                  receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                              "%s: Unknown SYNOP-SHIP in %s. If this is a correct report, contact maintainer %s (pos=%d)",
                              p_file_name, p_orig_name, AFD_MAINTAINER,
                              rcdb_pos);
                  hex_print(WARN_SIGN, p_start, msg_length);
               }
               return(INCORRECT);
            }
         }
         else if (rcdb[rcdb_pos].mimj[0] == 'A')
              {
                 if ((isdigit((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))) &&
                     (isdigit((int)(*(ptr + 2)))) &&
                     (isdigit((int)(*(ptr + 3)))) &&
                     (isdigit((int)(*(ptr + 4)))) &&
                     ((*(ptr + 5) == ' ') || (*(ptr + 5) == 13) ||
                      (*(ptr + 5) == 10)))
                 {
                    station_id[0] = *ptr;
                    station_id[1] = *(ptr + 1);
                    station_id[2] = *(ptr + 2);
                    station_id[3] = *(ptr + 3);
                    station_id[4] = *(ptr + 4);
                    *station_id_length = 5;
                 }
                 else
                 {
                    station_id[0] = '\0';
                    if ((*ptr == 'N') &&
                        (((*(ptr + 1) == 'I') && (*(ptr + 2) == 'L')) ||
                         (((*(ptr + 1) == 'N') || (*(ptr + 1) == 'n')) &&
                          ((*(ptr + 2) == 'N') || (*(ptr + 2) == 'n')) &&
                          ((*(ptr + 3) == 'N') || (*(ptr + 3) == 'n')))))
                    {
                       /* Do not warn if we find NIL or NNNN. */;
                    }
                    else
                    {
                       receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                   "%s: Unknown SYNOP in %s. If this is a correct report, contact maintainer %s (pos=%d)",
                                   p_file_name, p_orig_name, AFD_MAINTAINER,
                                   rcdb_pos);
                       hex_print(WARN_SIGN, p_start, msg_length);
                    }
                    return(INCORRECT);
                 }
              }
              else
              {
                 receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                             "%s: MIMJ needs still to be done for this. (%s %d)",
                             p_file_name, p_orig_name, rcdb_pos);
              }

         break;

      case RT_SYNOP_MOBIL : /* SYNOP-MOBIL */

         if (rcdb[rcdb_pos].mimj[0] == 'O')
         {
            /* First store the mobil idendifier (up to 10 characters    */
            /* starting with characters, numbers + ending with numbers) */
            if (isalnum((int)(*ptr)))
            {
               int i = 1;

               station_id[0] = *ptr;
               ptr++;
               while ((i < 10) && (isalnum((int)(*ptr))))
               {
                  station_id[i] = *ptr;
                  i++; ptr++;
               }
               if ((i == 10) && (isalnum((int)(*ptr))))
               {
                  station_id[i] = '\0';
                  receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                              "%s: MOBIL identifier in %s more than 10 characters. Ignoring them.",
                              p_file_name, p_orig_name);
                  while (isalnum((int)(*ptr)))
                  {
                     ptr++;
                  }
               }

               while (*ptr == ' ')
               {
                  ptr++;
               }

               /* Ignore date string. */
               while (isdigit((int)(*ptr)))
               {
                  ptr++;
               }
               *p_wid = *(ptr - 1);
               *(p_wid + 1) = '\0';

               while (*ptr == ' ')
               {
                  ptr++;
               }

               /* Now lets try and find the location of the mobile. */
               if ((isdigit((int)(*ptr))) &&
                   (isdigit((int)(*(ptr + 1)))) &&
                   (isdigit((int)(*(ptr + 2)))) &&
                   (isdigit((int)(*(ptr + 3)))) &&
                   (isdigit((int)(*(ptr + 4)))) &&
                   (*(ptr + 5) == ' ') &&
                   (isdigit((int)(*(ptr + 6)))) &&
                   (isdigit((int)(*(ptr + 7)))) &&
                   (isdigit((int)(*(ptr + 8)))) &&
                   (isdigit((int)(*(ptr + 9)))) &&
                   (isdigit((int)(*(ptr + 10)))) &&
                   (*(ptr + 11) == ' ') &&
                   ((*(ptr + 12) == '/') || (isdigit((int)(*(ptr + 12))))) &&
                   ((*(ptr + 13) == '/') || (isdigit((int)(*(ptr + 13))))) &&
                   ((*(ptr + 14) == '/') || (isdigit((int)(*(ptr + 14))))) &&
                   ((*(ptr + 15) == '/') || (isdigit((int)(*(ptr + 15))))) &&
                   ((*(ptr + 16) == '/') || (isdigit((int)(*(ptr + 16))))) &&
                   ((*(ptr + 17) == ' ') || (*(ptr + 17) == 13) ||
                    (*(ptr + 17) == 10)))
               {
                  station_id[i] = '_';
                  station_id[i + 1] = *(ptr + 6);
                  station_id[i + 2] = *(ptr + 7);
                  station_id[i + 3] = *(ptr + 8);
                  station_id[i + 4] = *(ptr + 9);
                  station_id[i + 5] = *(ptr + 10);
                  station_id[i + 6] = '_';
                  if (*(ptr + 12) == '/')
                  {
                     station_id[i + 7] = '_';
                  }
                  else
                  {
                     station_id[i + 7] = *(ptr + 12);
                  }
                  if (*(ptr + 13) == '/')
                  {
                     station_id[i + 8] = '_';
                  }
                  else
                  {
                     station_id[i + 8] = *(ptr + 13);
                  }
                  if (*(ptr + 14) == '/')
                  {
                     station_id[i + 9] = '_';
                  }
                  else
                  {
                     station_id[i + 9] = *(ptr + 14);
                  }
                  if (*(ptr + 15) == '/')
                  {
                     station_id[i + 10] = '_';
                  }
                  else
                  {
                     station_id[i + 10] = *(ptr + 15);
                  }
                  if (*(ptr + 16) == '/')
                  {
                     station_id[i + 11] = '_';
                  }
                  else
                  {
                     station_id[i + 11] = *(ptr + 16);
                  }
                  *station_id_length = i + 12;
               }
               else
               {
                  *station_id_length = i;
                  station_id[i] = '\0';
                  receive_log(INFO_SIGN, __FILE__, __LINE__, 0L,
                              "%s: Unable to find location of MOBIL in %s, ignoring location.",
                              p_file_name, p_orig_name);
                  hex_print(INFO_SIGN, p_start, msg_length);
               }
            }
         }
         else
         {
            receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                        "%s: MIMJ needs still to be done for this. (%s %d)",
                        p_file_name, p_orig_name, rcdb_pos);
         }
         break;

      case RT_UPPER_AIR  : /* UPPER-AIR */
         if ((isalnum((int)(*ptr))) && (isalnum((int)(*(ptr + 1)))) &&
             (isalnum((int)(*(ptr + 2)))) && (isalnum((int)(*(ptr + 3)))) &&
             (isalnum((int)(*(ptr + 4)))) && (isalnum((int)(*(ptr + 5)))) &&
             (*(ptr + 6) == ' ') && (isupper((int)(*(ptr + 7)))) &&
             (isupper((int)(*(ptr + 8)))) && (isupper((int)(*(ptr + 9)))) &&
             (isupper((int)(*(ptr + 10)))) && (*(ptr + 11) == ' ') &&
             (isdigit((int)(*(ptr + 12)))) && (isdigit((int)(*(ptr + 13)))) &&
             (isdigit((int)(*(ptr + 14)))) && (isdigit((int)(*(ptr + 15)))) &&
             (isdigit((int)(*(ptr + 16)))) && (isdigit((int)(*(ptr + 17)))) &&
             (*(ptr + 18) == 13) && (*(ptr + 19) == 13) &&
             (*(ptr + 20) == 10))
         {
            int i = 0;

            /*
             * FIXME!
             * In most cases this should work. However if the length
             * of the filename does differ we have a problem and we
             * need to handle this! Lets first see how often this happens
             * in the real world.
             */
            while (((ptr - p_start) < msg_length) &&
                   (*ptr > 31) && (i < MAX_WMO_HEADER_LENGTH))
            {
               if ((*ptr == ' ') || (*ptr == '/') || (*ptr < ' ') ||
                   (*ptr > 'z'))
               {
                  p_file_name[i] = '_';
               }
               else
               {
                  p_file_name[i] = *ptr;
               }
               ptr++; i++;
            }
            while (((ptr - p_start) < msg_length) &&
                   ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
            {
               ptr++;
            }
            if ((i + 1) != file_name_offset)
            {
               receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                           "File name (%s) will be wrong, we need to fix this! Source file is %s. (%d!=%d)",
                           p_file_name, p_orig_name, i + 1, file_name_offset);
               hex_print(WARN_SIGN, ptr, msg_length);
            }
         }
         if ((isalpha((int)(*ptr))) && (isalpha((int)(*(ptr + 1)))) &&
             (isalpha((int)(*(ptr + 2)))) && (isalpha((int)(*(ptr + 3)))) &&
             (*(ptr + 4) == ' ') &&
             ((isdigit((int)(*(ptr + 5)))) || (*(ptr + 5) == '/')) &&
             ((isdigit((int)(*(ptr + 6)))) || (*(ptr + 6) == '/')) &&
             ((isdigit((int)(*(ptr + 7)))) || (*(ptr + 7) == '/')) &&
             ((isdigit((int)(*(ptr + 8)))) || (*(ptr + 8) == '/')) &&
             ((isdigit((int)(*(ptr + 9)))) || (*(ptr + 9) == '/')) &&
             (*(ptr + 10) == ' ') &&
             ((*(ptr + 11) == ' ') || (isdigit((int)(*(ptr + 11))))) &&
             (isdigit((int)(*(ptr + 12)))) && (isdigit((int)(*(ptr + 13)))) &&
             (isdigit((int)(*(ptr + 14)))) && (isdigit((int)(*(ptr + 15)))) &&
             ((*(ptr + 16) == ' ') || (*(ptr + 16) == 13) ||
              ((*(ptr + 11) == ' ') && (isdigit((int)(*(ptr + 16)))) &&
               ((*(ptr + 17) == ' ') || (*(ptr + 17) == 13)))))
         {
            station_id[0] = *(ptr + 11);
            station_id[1] = *(ptr + 12);
            station_id[2] = *(ptr + 13);
            station_id[3] = *(ptr + 14);
            station_id[4] = *(ptr + 15);
            *station_id_length = 5;
         }
         else if ((isalpha((int)(*ptr))) && (isalpha((int)(*(ptr + 1)))) &&
                  (isalpha((int)(*(ptr + 2)))) &&
                  (isalpha((int)(*(ptr + 3)))) &&
                  (*(ptr + 4) == ' ') && (*(ptr + 5) == ' ') &&
                  ((isdigit((int)(*(ptr + 6)))) || (*(ptr + 6) == '/')) &&
                  ((isdigit((int)(*(ptr + 7)))) || (*(ptr + 7) == '/')) &&
                  ((isdigit((int)(*(ptr + 8)))) || (*(ptr + 8) == '/')) &&
                  ((isdigit((int)(*(ptr + 9)))) || (*(ptr + 9) == '/')) &&
                  ((isdigit((int)(*(ptr + 10)))) || (*(ptr + 10) == '/')) &&
                  (*(ptr + 11) == ' ') && (isdigit((int)(*(ptr + 12)))) &&
                  (isdigit((int)(*(ptr + 13)))) &&
                  (isdigit((int)(*(ptr + 14)))) &&
                  (isdigit((int)(*(ptr + 15)))) &&
                  (isdigit((int)(*(ptr + 16)))) &&
                  ((*(ptr + 17) == ' ') || (*(ptr + 17) == 13)))
              {
                 station_id[0] = *(ptr + 12);
                 station_id[1] = *(ptr + 13);
                 station_id[2] = *(ptr + 14);
                 station_id[3] = *(ptr + 15);
                 station_id[4] = *(ptr + 16);
                 *station_id_length = 5;
              }
         else if ((isalpha((int)(*ptr))) && (isalpha((int)(*(ptr + 1)))) &&
                  (isalpha((int)(*(ptr + 2)))) &&
                  (isalpha((int)(*(ptr + 3)))) &&
                  (*(ptr + 4) == ' ') && (*(ptr + 5) == ' ') &&
                  ((isdigit((int)(*(ptr + 6)))) || (*(ptr + 6) == '/')) &&
                  ((isdigit((int)(*(ptr + 7)))) || (*(ptr + 7) == '/')) &&
                  ((isdigit((int)(*(ptr + 8)))) || (*(ptr + 8) == '/')) &&
                  ((isdigit((int)(*(ptr + 9)))) || (*(ptr + 9) == '/')) &&
                  ((isdigit((int)(*(ptr + 10)))) || (*(ptr + 10) == '/')) &&
                  (*(ptr + 11) == ' ') && (*(ptr + 12) == ' ') &&
                  (isdigit((int)(*(ptr + 13)))) &&
                  (isdigit((int)(*(ptr + 14)))) &&
                  (isdigit((int)(*(ptr + 15)))) &&
                  (isdigit((int)(*(ptr + 16)))) &&
                  (isdigit((int)(*(ptr + 17)))) &&
                  ((*(ptr + 18) == ' ') || (*(ptr + 18) == 13)))
              {
                 station_id[0] = *(ptr + 13);
                 station_id[1] = *(ptr + 14);
                 station_id[2] = *(ptr + 15);
                 station_id[3] = *(ptr + 16);
                 station_id[4] = *(ptr + 17);
                 *station_id_length = 5;
              }
         else if ((isalpha((int)(*ptr))) && (isalpha((int)(*(ptr + 1)))) &&
                  (isalpha((int)(*(ptr + 2)))) &&
                  (isalpha((int)(*(ptr + 3)))) &&
                  (*(ptr + 4) == ' ') && (*(ptr + 5) == ' ') &&
                  (*(ptr + 6) == ' ') &&
                  ((isdigit((int)(*(ptr + 7)))) || (*(ptr + 7) == '/')) &&
                  ((isdigit((int)(*(ptr + 8)))) || (*(ptr + 8) == '/')) &&
                  ((isdigit((int)(*(ptr + 9)))) || (*(ptr + 9) == '/')) &&
                  ((isdigit((int)(*(ptr + 10)))) || (*(ptr + 10) == '/')) &&
                  ((isdigit((int)(*(ptr + 11)))) || (*(ptr + 11) == '/')) &&
                  (*(ptr + 12) == ' ') && (isdigit((int)(*(ptr + 13)))) &&
                  (isdigit((int)(*(ptr + 14)))) &&
                  (isdigit((int)(*(ptr + 15)))) &&
                  (isdigit((int)(*(ptr + 16)))) &&
                  (isdigit((int)(*(ptr + 17)))) &&
                  ((*(ptr + 18) == ' ') || (*(ptr + 18) == 13)))
              {
                 station_id[0] = *(ptr + 13);
                 station_id[1] = *(ptr + 14);
                 station_id[2] = *(ptr + 15);
                 station_id[3] = *(ptr + 16);
                 station_id[4] = *(ptr + 17);
                 *station_id_length = 5;
              }
         else if ((isdigit((int)(*ptr))) && (isdigit((int)(*(ptr + 1)))) &&
                  (isdigit((int)(*(ptr + 2)))) &&
                  (isdigit((int)(*(ptr + 3)))) &&
                  (isdigit((int)(*(ptr + 4)))) && (*(ptr + 5) == ' ') &&
                  (*(ptr + 6) == 'N') && (*(ptr + 7) == 'I') &&
                  (*(ptr + 8) == 'L'))
              {
                 station_id[0] = *ptr;
                 station_id[1] = *(ptr + 1);
                 station_id[2] = *(ptr + 2);
                 station_id[3] = *(ptr + 3);
                 station_id[4] = *(ptr + 4);
                 *station_id_length = 5;
              }
              else
              {
                 station_id[0] = '\0';
                 if ((*ptr == 'N') &&
                     (((*(ptr + 1) == 'I') && (*(ptr + 2) == 'L')) ||
                      (((*(ptr + 1) == 'N') || (*(ptr + 1) == 'n')) &&
                       ((*(ptr + 2) == 'N') || (*(ptr + 2) == 'n')) &&
                       ((*(ptr + 3) == 'N') || (*(ptr + 3) == 'n')))))
                 {
                    /* Do not warn if we find NIL or NNNN. */;
                 }
                 else
                 {
                    receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                "%s: Unknown UPPER-AIR in %s. If this is a correct report, contact maintainer %s (pos=%d)",
                                p_file_name, p_orig_name, AFD_MAINTAINER,
                                rcdb_pos);
                    hex_print(WARN_SIGN, p_start, msg_length);
                 }
                 return(INCORRECT);
              }
         break;

      default            : /* Unknown report type */
         station_id[0] = '\0';
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     "%s: Unknown report type %d (pos=%d) for %s, unable to extract reports.",
                     p_file_name, rcdb[rcdb_pos].rt, rcdb_pos, p_orig_name);
         hex_print(WARN_SIGN, p_start, msg_length);
         return(INCORRECT);
   }

   return(SUCCESS);
}


/*---------------------------- check_report() ---------------------------*/
static int
check_report(char *ptr, unsigned int length, int *offset)
{
   int  xxtype = NO;
   char *p_start = ptr;

   /* Ignore any spaces at start. */
   while (((ptr - p_start) < length) && (*ptr == ' '))
   {
      ptr++;
   }

   /*
    * Lets first check if this is a SYNOP, SPECI, TAF, 'TAF AMD', AAXX,
    * BBXX which all have an extra line.
    */
   if (((ptr + 11 - p_start) < length) && (isupper((int)(*ptr))) &&
       (isupper((int)(*(ptr + 1)))) && (isupper((int)(*(ptr + 2)))) &&
       (isupper((int)(*(ptr + 3)))) && (*(ptr + 4) == ' ') &&
       (isdigit((int)(*(ptr + 5)))) && (isdigit((int)(*(ptr + 6)))) &&
       (isdigit((int)(*(ptr + 7)))) && (isdigit((int)(*(ptr + 8)))) &&
       (isdigit((int)(*(ptr + 9)))) &&
       ((*(ptr + 10) == 13) || (*(ptr + 10) == 10)))
   {
      ptr += 11;
      while (((ptr - p_start) < length) &&
             ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
      {
         ptr++;
      }
      *offset = ptr - p_start;
   }
        /* SPECI, METAR */
   else if (((ptr + 6 - p_start) < length) &&
            ((*(ptr + 5) == 13) || (*(ptr + 5) == 10)) &&
            (((*ptr == 'S') && (*(ptr + 1) == 'P') && (*(ptr + 2) == 'E') &&
              (*(ptr + 3) == 'C') && (*(ptr + 4) == 'I')) ||
             ((*ptr == 'M') && (*(ptr + 1) == 'E') && (*(ptr + 2) == 'T') &&
              (*(ptr + 3) == 'A') && (*(ptr + 4) == 'R'))))
        {
           ptr += 6;
           while (((ptr - p_start) < length) &&
                  ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
           {
              ptr++;
           }
           *offset = ptr - p_start;
        }
        /* METAR YYGGggZ */
   else if (((ptr + 14 - p_start) < length) &&
            (*ptr == 'M') && (*(ptr + 1) == 'E') && (*(ptr + 2) == 'T') &&
            (*(ptr + 3) == 'A') && (*(ptr + 4) == 'R') &&
            (*(ptr + 5) == ' ') && (isdigit((int)(*(ptr + 6)))) &&
            (isdigit((int)(*(ptr + 7)))) && (isdigit((int)(*(ptr + 8)))) &&
            (isdigit((int)(*(ptr + 9)))) && (isdigit((int)(*(ptr + 10)))) &&
            (isdigit((int)(*(ptr + 11)))) && (*(ptr + 12) == 'Z') &&
            ((*(ptr + 13) == 13) || (*(ptr + 13) == 10)))
        {
           ptr += 14;
           while (((ptr - p_start) < length) &&
                  ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
           {
              ptr++;
           }
           *offset = ptr - p_start;
        }
        /* METAR COR */
   else if (((ptr + 10 - p_start) < length) &&
            (*ptr == 'M') && (*(ptr + 1) == 'E') && (*(ptr + 2) == 'T') &&
            (*(ptr + 3) == 'A') && (*(ptr + 4) == 'R') &&
            (*(ptr + 5) == ' ') && (*(ptr + 6) == 'C') &&
            (*(ptr + 7) == 'O') && (*(ptr + 8) == 'R') &&
            ((*(ptr + 9) == 13) || (*(ptr + 9) == 10)))
        {
           ptr += 10;
           while (((ptr - p_start) < length) &&
                  ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
           {
              ptr++;
           }
           *offset = ptr - p_start;
        }
        /* SWIS */
   else if (((ptr + 13 - p_start) < length) && (*ptr == 'S') &&
            (*(ptr + 1) == 'W') && (*(ptr + 2) == 'I') &&
            (*(ptr + 3) == 'S') && (*(ptr + 4) == ' ') &&
            (isdigit((int)(*(ptr + 5)))) && (isdigit((int)(*(ptr + 6)))) &&
            (isdigit((int)(*(ptr + 7)))) && (isdigit((int)(*(ptr + 8)))) &&
            (isdigit((int)(*(ptr + 9)))) && (isdigit((int)(*(ptr + 10)))) &&
            (isdigit((int)(*(ptr + 11)))) &&
            ((*(ptr + 12) == 13) || (*(ptr + 12) == 10)))
        {
           ptr += 13;
           while (((ptr - p_start) < length) &&
                  ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
           {
              ptr++;
           }
           *offset = ptr - p_start;
        }
        /* CLIMAT */
   else if (((ptr + 13 - p_start) < length) && (*ptr == 'C') &&
            (*(ptr + 1) == 'L') && (*(ptr + 2) == 'I') &&
            (*(ptr + 3) == 'M') && (*(ptr + 4) == 'A') &&
            (*(ptr + 5) == 'T') && (*(ptr + 6) == ' ') &&
            (isdigit((int)(*(ptr + 7)))) && (isdigit((int)(*(ptr + 8)))) &&
            (isdigit((int)(*(ptr + 9)))) && (isdigit((int)(*(ptr + 10)))) &&
            (isdigit((int)(*(ptr + 11)))) &&
            ((*(ptr + 12) == 13) || (*(ptr + 12) == 10)))
        {
           ptr += 13;
           while (((ptr - p_start) < length) &&
                  ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
           {
              ptr++;
           }
           *offset = ptr - p_start;
        }
        /* AUTOTREND */
   else if (((ptr + 10 - p_start) < length) &&
            (*ptr == 'A') && (*(ptr + 1) == 'U') && (*(ptr + 2) == 'T') &&
            (*(ptr + 3) == 'O') && (*(ptr + 4) == 'T') &&
            (*(ptr + 5) == 'R') && (*(ptr + 6) == 'E') &&
            (*(ptr + 7) == 'N') && (*(ptr + 8) == 'D') &&
            ((*(ptr + 9) == 13) || (*(ptr + 9) == 10)))
        {
           ptr += 10;
           while (((ptr - p_start) < length) &&
                  ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
           {
              ptr++;
           }
           *offset = ptr - p_start;
        }
        /* AAXX or BBXX */
   else if (((ptr + 6 - p_start) < length) &&
            (((*ptr == 'A') && (*(ptr + 1) == 'A')) ||
             ((*ptr == 'B') && (*(ptr + 1) == 'B'))) &&
            (*(ptr + 2) == 'X') && (*(ptr + 3) == 'X') &&
            ((*(ptr + 4) == 13) || (*(ptr + 4) == 10)))
        {
           ptr += 5;
           while (((ptr - p_start) < length) &&
                  ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
           {
              ptr++;
           }
           xxtype = YES;
           *offset = ptr - p_start;
        }
        /* TAF */
   else if (((ptr + 4 - p_start) < length) && (*ptr == 'T') &&
            (*(ptr + 1) == 'A') && (*(ptr + 2) == 'F') &&
            ((*(ptr + 3) == 13) || (*(ptr + 3) == 10)))
        {
           ptr += 4;
           while (((ptr - p_start) < length) &&
                  ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
           {
              ptr++;
           }
           *offset = ptr - p_start;
        }
        /* TAF AMD or COR */
   else if (((ptr + 8 - p_start) < length) && (*ptr == 'T') &&
            (*(ptr + 1) == 'A') && (*(ptr + 2) == 'F') &&
            (*(ptr + 3) == ' ') &&
            (((*(ptr + 4) == 'A') && (*(ptr + 5) == 'M') &&
              (*(ptr + 6) == 'D')) ||
             ((*(ptr + 4) == 'C') && (*(ptr + 5) == 'O') &&
              (*(ptr + 6) == 'R'))) &&
            ((*(ptr + 7) == 13) || (*(ptr + 7) == 10)))
        {
           ptr += 8;
           while (((ptr - p_start) < length) &&
                  ((*ptr == 13) || (*ptr == 10) || (*ptr == ' ')))
           {
              ptr++;
           }
           *offset = ptr - p_start;
        }
        /* Identify german TEXT as bulletins. */
   else if (((ptr + 5 - p_start) < length) && (*ptr == 'T') &&
            (*(ptr + 1) == 'E') && (*(ptr + 2) == 'X') &&
            (*(ptr + 3) == 'T') && (*(ptr + 4) == ' '))
        {
           return(INCORRECT);
        }
        /* GAFOR */
   else if (((ptr + 6 - p_start) < length) && (*ptr == 'G') &&
            (*(ptr + 1) == 'A') && (*(ptr + 2) == 'F') &&
            (*(ptr + 3) == 'O') && (*(ptr + 4) == 'R') &&
            (*(ptr + 5) == ' '))
        {
           return(INCORRECT);
        }
        else
        {
           *offset = 0;
        }

   /* Ignore any spaces at start. */
   while (((ptr - p_start) < length) && (*ptr == ' '))
   {
      ptr++;
   }

   /* TAF */
   if (((ptr + 9 - p_start) < length) && (*ptr == 'T') &&
       (*(ptr + 1) == 'A') && (*(ptr + 2) == 'F') &&
       (*(ptr + 3) == ' ') && (*(ptr + 8) == ' ') &&
       (isupper((int)(*(ptr + 4)))) && (isupper((int)(*(ptr + 5)))) &&
       (isupper((int)(*(ptr + 6)))) && (isupper((int)(*(ptr + 7)))))
   {
      return(SUCCESS);
   }
        /* TAF AMD or COR */
   else if (((ptr + 13 - p_start) < length) && (isupper((int)(*ptr))) &&
            (isupper((int)(*(ptr + 1)))) &&
            (isupper((int)(*(ptr + 2)))) && (*(ptr + 3) == ' ') &&
            (isupper((int)(*(ptr + 4)))) &&
            (isupper((int)(*(ptr + 5)))) &&
            (isupper((int)(*(ptr + 6)))) && (*(ptr + 7) == ' ') &&
            (isupper((int)(*(ptr + 8)))) &&
            (isupper((int)(*(ptr + 9)))) &&
            (isupper((int)(*(ptr + 10)))) &&
            (isupper((int)(*(ptr + 11)))) && (*(ptr + 12) == ' '))
        {
           return(SUCCESS);
        }
        /* METAR or SPECI */
   else if (((ptr + 6 - p_start) < length) &&
            (((*ptr == 'M') && (*(ptr + 1) == 'E') &&
              (*(ptr + 2) == 'T') && (*(ptr + 3) == 'A') &&
              (*(ptr + 4) == 'R')) || 
             ((*ptr == 'S') && (*(ptr + 1) == 'P') &&
              (*(ptr + 2) == 'E') && (*(ptr + 3) == 'C') &&
              (*(ptr + 4) == 'I'))) && 
            (*(ptr + 5) == ' '))
        {
           while (((ptr + 6 - p_start) < length) && (*(ptr + 6) == ' '))
           {
              ptr++;
           }
           if (((ptr + 4 - p_start) < length) &&
               (*(ptr + 6) == 'C') && (*(ptr + 7) == 'O') &&
               (*(ptr + 8) == 'R') && (*(ptr + 9) == ' '))
           {
              ptr += 4;
           }
           if (((ptr + 5 - p_start) < length) &&
               ((isupper((int)(*(ptr + 6)))) || (isdigit((int)(*(ptr + 6))))) &&
               ((isupper((int)(*(ptr + 7)))) || (isdigit((int)(*(ptr + 7))))) &&
               ((isupper((int)(*(ptr + 8)))) || (isdigit((int)(*(ptr + 8))))) &&
               ((isupper((int)(*(ptr + 9)))) || (isdigit((int)(*(ptr + 9))))) &&
               (*(ptr + 10) == ' '))
           {
              return(SUCCESS);
           }
           else if (((ptr + 6 - p_start) < length) &&
                    ((isupper((int)(*(ptr + 6)))) ||
                     (isdigit((int)(*(ptr + 6))))) &&
                    ((isupper((int)(*(ptr + 7)))) ||
                     (isdigit((int)(*(ptr + 7))))) &&
                    ((isupper((int)(*(ptr + 8)))) ||
                     (isdigit((int)(*(ptr + 8))))) &&
                    ((isupper((int)(*(ptr + 9)))) ||
                     (isdigit((int)(*(ptr + 9))))) &&
                    ((isupper((int)(*(ptr + 10)))) ||
                     (isdigit((int)(*(ptr + 10))))) &&
                    (*(ptr + 11) == ' '))
                {
                   return(SUCCESS);
                }
        }
        /* METAR, SPECI, TAF AMD, AAXX or BBXX (in a group) */
   else if (((ptr + 5 - p_start) < length) &&
            ((isupper((int)(*ptr))) || (isdigit((int)(*ptr)))) &&
            ((isupper((int)(*(ptr + 1)))) || (isdigit((int)(*(ptr + 1))))) &&
            ((isupper((int)(*(ptr + 2)))) || (isdigit((int)(*(ptr + 2))))) &&
            ((isupper((int)(*(ptr + 3)))) || (isdigit((int)(*(ptr + 3))))) &&
            (*(ptr + 4) == ' '))
        {
           return(SUCCESS);
        }
        /* AAXX or BBXX (in a group) */
   else if ((xxtype == YES) &&
            ((ptr + 7 - p_start) < length) &&
            ((isupper((int)(*ptr))) || (isdigit((int)(*ptr)))) &&
            ((isupper((int)(*(ptr + 1)))) || (isdigit((int)(*(ptr + 1))))) &&
            ((isupper((int)(*(ptr + 2)))) || (isdigit((int)(*(ptr + 2))))) &&
            ((isupper((int)(*(ptr + 3)))) || (isdigit((int)(*(ptr + 3))))) &&
            ((isupper((int)(*(ptr + 4)))) || (isdigit((int)(*(ptr + 4))))) &&
            ((isupper((int)(*(ptr + 5)))) || (isdigit((int)(*(ptr + 5))))) &&
            (*(ptr + 6) == ' '))
        {
           return(SUCCESS);
        }
        /* AAXX or BBXX (in a group) */
   else if ((xxtype == YES) &&
            ((ptr + 8 - p_start) < length) &&
            ((isupper((int)(*ptr))) || (isdigit((int)(*ptr)))) &&
            ((isupper((int)(*(ptr + 1)))) || (isdigit((int)(*(ptr + 1))))) &&
            ((isupper((int)(*(ptr + 2)))) || (isdigit((int)(*(ptr + 2))))) &&
            ((isupper((int)(*(ptr + 3)))) || (isdigit((int)(*(ptr + 3))))) &&
            ((isupper((int)(*(ptr + 4)))) || (isdigit((int)(*(ptr + 4))))) &&
            ((isupper((int)(*(ptr + 5)))) || (isdigit((int)(*(ptr + 5))))) &&
            ((isupper((int)(*(ptr + 6)))) || (isdigit((int)(*(ptr + 6))))) &&
            (*(ptr + 7) == ' '))
        {
           return(SUCCESS);
        }
        /* SYNOP, AAXX or BBXX (in a group) */
   else if (((ptr + 6 - p_start) < length) &&
            ((isupper((int)(*ptr))) || (isdigit((int)(*ptr)))) &&
            ((isupper((int)(*(ptr + 1)))) || (isdigit((int)(*(ptr + 1))))) &&
            ((isupper((int)(*(ptr + 2)))) || (isdigit((int)(*(ptr + 2))))) &&
            ((isupper((int)(*(ptr + 3)))) || (isdigit((int)(*(ptr + 3))))) &&
            ((isupper((int)(*(ptr + 4)))) || (isdigit((int)(*(ptr + 4))))) &&
            (*(ptr + 5) == ' '))
        {
           return(SUCCESS);
        }
        /* German METAR */
   else if (((ptr + 13 - p_start) < length) &&
            (isupper((int)(*ptr))) && (isupper((int)(*(ptr + 1)))) &&
            (isupper((int)(*(ptr + 2)))) && (isupper((int)(*(ptr + 3)))) &&
            (*(ptr + 4) == ' ') && (isdigit((int)(*(ptr + 5)))) &&
            (isdigit((int)(*(ptr + 6)))) && (isdigit((int)(*(ptr + 7)))) &&
            (isdigit((int)(*(ptr + 8)))) && (isdigit((int)(*(ptr + 9)))) &&
            (isdigit((int)(*(ptr + 10)))) && (*(ptr + 11) == 'Z') &&
            (*(ptr + 12) == ' '))
        {
           return(SUCCESS);
        }

   return(INCORRECT);
}


/*------------------------- show_unknown_report() -----------------------*/
static void
show_unknown_report(char         *ptr,
                    int          length,
                    char         *msg,
                    unsigned int job_id,
                    char         *file,
                    int          line)
{
   int  i = 0;
   char unknown_report[MAX_REPORT_LINE_LENGTH + 1];

   while (((ptr - msg) < length) && (i < MAX_REPORT_LINE_LENGTH) &&
          (*ptr >= ' ') && (*ptr <= 126))
   {
      unknown_report[i] = *ptr;
      i++; ptr++;
   }
   if (i == 0)
   {
      while (((ptr - msg) < length) &&
             (*ptr != 10) && (i < MAX_REPORT_LINE_LENGTH))
      {
         if ((*ptr >= ' ') && (*ptr <= 126))
         {
            unknown_report[i] = *ptr;
            i++; ptr++;
         }
         else
         {
            i += snprintf(&unknown_report[i], MAX_REPORT_LINE_LENGTH - i,
                          "<%d>", (int)*ptr);
            ptr++;
         }
      }
      if ((*ptr == 10) && ((i + 4) < MAX_REPORT_LINE_LENGTH))
      {
         unknown_report[i] = '<';
         unknown_report[i + 1] = '1';
         unknown_report[i + 2] = '0';
         unknown_report[i + 3] = '>';
         unknown_report[i + 4] = '\0';
      }
   }
   else
   {
      if ((*ptr == 13) && ((i + 4) < MAX_REPORT_LINE_LENGTH) &&
          ((ptr - msg) < length))
      {
         unknown_report[i] = '<';
         unknown_report[i + 1] = '1';
         unknown_report[i + 2] = '3';
         unknown_report[i + 3] = '>';
         unknown_report[i + 4] = '\0';
         i += 4;
         ptr++;
      }
      if ((*ptr == 13) && ((i + 4) < MAX_REPORT_LINE_LENGTH) &&
          ((ptr - msg) < length))
      {
         unknown_report[i] = '<';
         unknown_report[i + 1] = '1';
         unknown_report[i + 2] = '3';
         unknown_report[i + 3] = '>';
         unknown_report[i + 4] = '\0';
         i += 4;
         ptr++;
      }
      if ((*ptr == 10) && ((i + 4) < MAX_REPORT_LINE_LENGTH) &&
          ((ptr - msg) < length))
      {
         unknown_report[i] = '<';
         unknown_report[i + 1] = '1';
         unknown_report[i + 2] = '0';
         unknown_report[i + 3] = '>';
         unknown_report[i + 4] = '\0';
         i += 4;
         ptr++;
      }
   }
   unknown_report[i] = '\0';
   receive_log(DEBUG_SIGN, file, line, 0L,
               _("Unknown report type `%s' in %s. #%x"),
               unknown_report, p_orig_name, job_id);

   return;
}


#define ASCII_OFFSET     54
#define DIR_ALIAS_OFFSET 16

/*++++++++++++++++++++++++++++ hex_print() ++++++++++++++++++++++++++++++*/
static void
hex_print(char *sign, char *buffer, int buffer_length)
{
   int       ascii_offset,
             header_length = DIR_ALIAS_OFFSET,
             i,
             line_length = 0,
             offset,
             wpos;
   char      *hex = "0123456789ABCDEF",
             *ptr = p_fra->dir_alias,
             wbuf[MAX_LINE_LENGTH + MAX_LINE_LENGTH + 1];
   time_t    current_time;
   struct tm *p_ts;

   current_time = time(NULL);
   p_ts     = localtime(&current_time);
   wbuf[0]  = (p_ts->tm_mday / 10) + '0';
   wbuf[1]  = (p_ts->tm_mday % 10) + '0';
   wbuf[2]  = ' ';
   wbuf[3]  = (p_ts->tm_hour / 10) + '0';
   wbuf[4]  = (p_ts->tm_hour % 10) + '0';
   wbuf[5]  = ':';
   wbuf[6]  = (p_ts->tm_min / 10) + '0';
   wbuf[7]  = (p_ts->tm_min % 10) + '0';
   wbuf[8]  = ':';
   wbuf[9]  = (p_ts->tm_sec / 10) + '0';
   wbuf[10] = (p_ts->tm_sec % 10) + '0';
   wbuf[11] = ' ';
   wbuf[12] = sign[0];
   if (((sign[1] == 'E') || (sign[1] == 'W')) &&
        ((p_fra->dir_flag & DIR_ERROR_OFFLINE) ||
         (p_fra->dir_flag & DIR_ERROR_OFFL_T)))
   {
      wbuf[13] = 'O';
   }
   else
   {
      wbuf[13] = sign[1];
   }
   wbuf[14] = sign[2];
   wbuf[15] = ' ';

   while ((header_length < (MAX_LINE_LENGTH + MAX_LINE_LENGTH)) && (*ptr != '\0'))
   {
      wbuf[header_length] = *ptr;
      ptr++; header_length++;
   }
   while ((header_length - DIR_ALIAS_OFFSET) < MAX_DIR_ALIAS_LENGTH)
   {
      wbuf[header_length] = ' ';
      header_length++;
   }
   wbuf[header_length]     = ':';
   wbuf[header_length + 1] = ' ';
   header_length += 2;
   ascii_offset = header_length + ASCII_OFFSET,
   wpos = header_length;

   for (i = 0; i < buffer_length; i++)
   {
      if ((i % 16) == 0)
      {
         if (line_length > 0)
         {
            offset = ascii_offset + line_length;
            wbuf[ascii_offset - 1] = ' ';
            wbuf[offset] = '\n';
            if (write(receive_log_fd, wbuf, (offset + 1)) != (offset + 1))
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "write() error : %s", strerror(errno));
            }
            wpos = header_length;
            line_length = 0;
         }
      }
      else
      {
         if ((i % 4) == 0)
         {
            wbuf[wpos] = '|'; wbuf[wpos + 1] = ' ';
            wpos += 2;
         }
      }
      if ((unsigned char)buffer[i] > 15)
      {
         wbuf[wpos] = hex[((unsigned char)buffer[i]) >> 4];
         wbuf[wpos + 1] = hex[((unsigned char)buffer[i]) & 0x0F];
      }
      else
      {
         wbuf[wpos] = '0';
         wbuf[wpos + 1] = hex[(unsigned char)buffer[i]];
      }
      wbuf[wpos + 2] = ' ';
      wpos += 3;
      if (((unsigned char)buffer[i] < 32) || ((unsigned char)buffer[i] > 126))
      {
         wbuf[ascii_offset + line_length] = '.';
      }
      else
      {
         wbuf[ascii_offset + line_length] = buffer[i];
      }
      line_length++;
   }
   if (line_length > 0)
   {
      for (i = line_length; i < 16; i++)
      {
         if ((i % 4) == 0)
         {
            wbuf[wpos] = '|'; wbuf[wpos + 1] = ' ';
            wpos += 2;
         }
         wbuf[wpos] = ' '; wbuf[wpos + 1] = ' '; wbuf[wpos + 2] = ' ';
         wpos += 3;
      }
      offset = ascii_offset + line_length;
      wbuf[ascii_offset - 1] = ' ';
      wbuf[offset] = '\n';
      if (write(receive_log_fd, wbuf, (offset + 1)) != (offset + 1))
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "write() error : %s", strerror(errno));
      }
   }
   return;
}
