/*
 *  alda.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   alda - AFD log data analyser
 **
 ** SYNOPSIS
 **   alda [options] <file name pattern>
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   15.04.2007 H.Kiehl Created
 **
 */
DESCR__E_M3


#include <stdio.h>
#include <string.h>      /* strerror()                                  */
#include <stdlib.h>      /* exit(), atoi()                              */
#include <time.h>        /* time()                                      */
#include <unistd.h>      /* STDOUT_FILENO                               */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include "logdefs.h"
#include "aldadefs.h"
#ifdef WITH_AFD_MON
# include "mondefs.h"
#endif
#include "version.h"

/* Global variables. */
unsigned int               end_alias_counter,
                           *end_id,
                           end_id_counter,
                           end_name_counter,
                           file_pattern_counter,
                           mode,
#ifdef WITH_AFD_MON
                           msa_fd = -1,
                           msa_id,
                           no_of_afds = 0,
#endif
                           protocols,
                           search_dir_alias_counter,
                           *search_dir_id,
                           search_dir_id_counter,
                           search_dir_name_counter,
                           search_duration_flag = 0,
                           search_file_size_flag = 0,
                           search_orig_file_size_flag = 0,
                           search_host_alias_counter,
                           *search_host_id,
                           search_host_id_counter,
                           search_host_name_counter,
                           search_job_id = 0,
                           search_unique_number = 0,
                           search_log_type = SEARCH_ALL_LOGS,
#ifdef _OUTPUT_LOG
                           show_output_type = SHOW_NORMAL_DELIVERED,
#endif
                           start_alias_counter,
                           *start_id,
                           start_id_counter,
                           start_name_counter,
                           start_search_counter;
int                        data_printed,
                           fra_fd = -1,
                           fra_id,
                           fsa_fd = -1,
                           fsa_id,
                           gt_lt_sign,
                           gt_lt_sign_duration,
                           gt_lt_sign_orig,
                           log_date_length = LOG_DATE_LENGTH,
                           max_hostname_length = MAX_HOSTNAME_LENGTH,
                           no_of_dirs = 0,
                           no_of_hosts = 0,
                           rotate_limit,
                           *search_afd_msa_pos = NULL,
                           sys_log_fd = STDOUT_FILENO, /* Used by get_afd_path(). */
                           trace_mode,
                           verbose;
time_t                     end_time_end,
                           end_time_start,
                           init_time_start,
                           max_diff_time,
                           max_search_time,
                           start,
                           start_time_end,
                           start_time_start;
off_t                      log_data_written,
                           search_file_size = -1,
                           search_orig_file_size = -1;
#ifdef HAVE_MMAP
off_t                      fra_size,
                           fsa_size;
#endif
#ifdef WITH_AFD_MON
off_t                      msa_size;
#endif
double                     search_duration;
clock_t                    clktck;
char                       **end_alias,
                           **end_name,
                           **file_pattern,
                           footer_filename[MAX_PATH_LENGTH],
                           *format_str = NULL,
                           header_filename[MAX_PATH_LENGTH],
                           output_filename[MAX_PATH_LENGTH],
                           *p_work_dir,
                           **search_afd_start_alias = NULL,
                           **search_dir_alias,
                           **search_dir_name,
                           **search_host_alias,
                           **search_host_name,
                           **start_alias,
                           **start_name;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;
FILE                       *output_fp;
struct dir_name_area       dna;
struct fileretrieve_status *fra = NULL;
struct filetransfer_status *fsa = NULL;
struct jid_data            jidd;
#ifdef WITH_AFD_MON
unsigned int               adl_entries = 0,
                           ahl_entries = 0;
struct afd_dir_list        *adl = NULL;
struct afd_host_list       *ahl = NULL;
struct afd_typesize_data   *atd = NULL;
struct mon_status_area     *msa;
#endif
struct afd_info            afd;
#ifdef _INPUT_LOG
off_t                      *icp;    /* Input current position. */
struct log_file_data       input;
struct alda_idata          ilog;
#endif
#ifdef _DISTRIBUTION_LOG
struct alda_cache_data     *ucache; /* Local distribution cache.   */
struct alda_position_list  **upl;   /* Distribution position list. */
struct log_file_data       distribution;
struct alda_udata          ulog;
#endif
#ifdef _PRODUCTION_LOG
struct alda_cache_data     *pcache; /* Local production cache.   */
struct alda_position_list  **ppl;   /* Production position list. */
struct log_file_data       production;
struct alda_pdata          plog,
                           success_plog;
#endif
#ifdef _OUTPUT_LOG
int                        odata_entries = 0;
struct alda_cache_data     *ocache; /* Local output cache. */
struct alda_position_list  **opl;   /* Output position list. */
struct log_file_data       output;
struct alda_odata          *odata = NULL,  /* Temp. storage */
                           olog;
#endif
#ifdef _DELETE_LOG
int                        ddata_entries = 0;
struct alda_cache_data     *dcache; /* Local delete cache. */
struct alda_position_list  **dpl;   /* Delete position list. */
struct log_file_data       delete;
struct alda_ddata          *ddata = NULL,  /* Temp. storage */
                           dlog;
#endif
#ifdef WITH_LOG_CACHE
int                        cache_step_size;
#endif

/* Local function prototypes. */
static void                get_afd_info(int),
                           init_file_data(time_t, time_t, int, char *),
#ifdef CACHE_DEBUG
                           print_alda_cache(void),
#endif
                           reshuffel_cache_data(int),
                           search_afd(char *);
#ifdef _INPUT_LOG
static int                 check_input_log(char *, char *, off_t, time_t,
                                           unsigned int);
#endif
#ifdef _DISTRIBUTION_LOG
static int                 check_distribution_log(char *, char *, off_t, time_t,
                                                  unsigned int, unsigned int *);
#endif
#ifdef _PRODUCTION_LOG
static int                 check_production_log(char *, char *, off_t, time_t,
                                                unsigned int, unsigned int,
                                                int, unsigned int *,
                                                unsigned int *);
#endif
#ifdef _OUTPUT_LOG
static int                 check_output_log(char *, char *, off_t, time_t,
                                            unsigned int, unsigned int *,
                                            unsigned int *);
#endif
#ifdef _DELETE_LOG
static int                 check_delete_log(char *, char *, off_t, time_t,
                                            unsigned int, unsigned int *,
                                            unsigned int *);
#endif
#ifdef WITH_AFD_MON
static int                 check_log_availability(int);
static void                add_afd_to_list(int),
                           check_end_afds(void),
                           check_start_afds(void),
                           get_current_afd_mon_list(void);
#endif


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ alda $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   time_t end;
   char   work_dir[MAX_PATH_LENGTH];

   /* Evaluate input arguments. */
   CHECK_FOR_VERSION(argc, argv);
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   if ((clktck = sysconf(_SC_CLK_TCK)) <= 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not get clock ticks per second : %s", strerror(errno));
      exit(INCORRECT);
   }
   eval_input_alda(&argc, argv);

   /* Initialize variables. */
   p_work_dir = work_dir;
   jidd.name[0] = '\0';
   jidd.jd = NULL;
#ifdef WITH_AFD_MON
   jidd.ajl = NULL;
#endif
   jidd.prev_pos = -1;
#ifdef _INPUT_LOG
   RESET_ILOG();
   input.fp = NULL;
   if ((input.line = malloc(MAX_INPUT_LINE_LENGTH)) == NULL)
   {
      (void)fprintf(stderr, "Failed to malloc() %d bytes : %s (%s %d)\n",
                    MAX_INPUT_LINE_LENGTH, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   input.line_length = MAX_INPUT_LINE_LENGTH;
   input.max_log_files = 0;
#endif
#ifdef _DISTRIBUTION_LOG
   RESET_ULOG();
   distribution.fp = NULL;
   if ((distribution.line = malloc(MAX_INPUT_LINE_LENGTH)) == NULL)
   {
      (void)fprintf(stderr, "Failed to malloc() %d bytes : %s (%s %d)\n",
                    MAX_INPUT_LINE_LENGTH, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   distribution.line_length = MAX_INPUT_LINE_LENGTH;
   distribution.max_log_files = 0;
   ucache = NULL;
#endif
#ifdef _PRODUCTION_LOG
   RESET_PLOG();
   production.fp = NULL;
   if ((production.line = malloc(MAX_INPUT_LINE_LENGTH)) == NULL)
   {
      (void)fprintf(stderr, "Failed to malloc() %d bytes : %s (%s %d)\n",
                    MAX_INPUT_LINE_LENGTH, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   production.line_length = MAX_INPUT_LINE_LENGTH;
   production.max_log_files = 0;
   pcache = NULL;
#endif
#ifdef _OUTPUT_LOG
   RESET_OLOG();
   output.fp = NULL;
   if ((output.line = malloc(MAX_INPUT_LINE_LENGTH)) == NULL)
   {
      (void)fprintf(stderr, "Failed to malloc() %d bytes : %s (%s %d)\n",
                    MAX_INPUT_LINE_LENGTH, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   output.line_length = MAX_INPUT_LINE_LENGTH;
   output.max_log_files = 0;
   ocache = NULL;
#endif
#ifdef _DELETE_LOG
   RESET_DLOG();
   delete.fp = NULL;
   if ((delete.line = malloc(MAX_INPUT_LINE_LENGTH)) == NULL)
   {
      (void)fprintf(stderr, "Failed to malloc() %d bytes : %s (%s %d)\n",
                    MAX_INPUT_LINE_LENGTH, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   delete.line_length = MAX_INPUT_LINE_LENGTH;
   delete.max_log_files = 0;
   dcache = NULL;
#endif
#ifdef WITH_LOG_CACHE
    cache_step_size = sizeof(time_t);
    if (sizeof(off_t) > cache_step_size)
    {
       cache_step_size = sizeof(off_t);
    }
#endif

   if (verbose)
   {
      start = time(NULL);
   }

   /* Lets determine what log files we need to search. */
   for (;;)
   {
      data_printed = NO;
      if (mode & ALDA_REMOTE_MODE)
      {
#ifdef WITH_AFD_MON
         int          i;
         unsigned int tmp_search_log_type;

         get_current_afd_mon_list();
         check_start_afds();
         check_end_afds();

         for (i = 0; i < start_search_counter; i++)
         {
            if ((search_log_type & SEARCH_INPUT_LOG) ||
                (search_log_type & SEARCH_DELETE_LOG))
            {
               attach_adl(search_afd_start_alias[i]);
            }
            if (search_log_type & SEARCH_OUTPUT_LOG)
            {
               attach_ahl(search_afd_start_alias[i]);
            }
            attach_atd(search_afd_start_alias[i]);
            alloc_jid(search_afd_start_alias[i]);
            get_afd_info(search_afd_msa_pos[i]);
            tmp_search_log_type = search_log_type;
            search_afd(search_afd_start_alias[i]);
            search_log_type = tmp_search_log_type;
            dealloc_jid();
            detach_atd();
            if ((search_log_type & SEARCH_INPUT_LOG) ||
                (search_log_type & SEARCH_DELETE_LOG))
            {
               detach_adl();
            }
            if (search_log_type & SEARCH_OUTPUT_LOG)
            {
               detach_ahl();
            }
            if ((start_search_counter > 1) ||
                (((mode & ALDA_CONTINUOUS_MODE) == 0) &&
                 ((mode & ALDA_CONTINUOUS_DAEMON_MODE) == 0)))
            {
# ifdef _INPUT_LOG
               if (input.fp != NULL)
               {
                  if (fclose(input.fp) == EOF)
                  {
                     (void)fprintf(stderr,
                                   "Failed to fclose() `%s' : %s (%s %d)\n",
                                   input.log_dir, strerror(errno),
                                   __FILE__, __LINE__);
                  }
                  input.fp = NULL;
                  input.bytes_read = 0;
               }
# endif
# ifdef _DISTRIBUTION_LOG
               if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE))
               {
                  if (ucache != NULL)
                  {
                     free(ucache);
                     ucache = NULL;
                  }
                  if (upl != NULL)
                  {
                     int i;

                     for (i = 0; i < distribution.max_log_files; i++)
                     {
                        free(upl[i]);
                     }
                     free(upl);
                     upl = NULL;
                  }
               }
               if (distribution.fp != NULL)
               {
                  if (fclose(distribution.fp) == EOF)
                  {
                     (void)fprintf(stderr,
                                   "Failed to fclose() `%s' : %s (%s %d)\n",
                                   distribution.log_dir, strerror(errno),
                                   __FILE__, __LINE__);
                  }
                  distribution.fp = NULL;
                  distribution.bytes_read = 0;
               }
# endif
# ifdef _PRODUCTION_LOG
               if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE))
               {
                  if (pcache != NULL)
                  {
                     free(pcache);
                     pcache = NULL;
                  }
                  if (ppl != NULL)
                  {
                     int i;

                     for (i = 0; i < production.max_log_files; i++)
                     {
                        free(ppl[i]);
                     }
                     free(ppl);
                     ppl = NULL;
                  }
               }
               if (production.fp != NULL)
               {
                  if (fclose(production.fp) == EOF)
                  {
                     (void)fprintf(stderr,
                                   "Failed to fclose() `%s' : %s (%s %d)\n",
                                   production.log_dir, strerror(errno),
                                   __FILE__, __LINE__);
                  }
                  production.fp = NULL;
                  production.bytes_read = 0;
               }
# endif
# ifdef _OUTPUT_LOG
               if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE))
               {
                  if (ocache != NULL)
                  {
                     free(ocache);
                     ocache = NULL;
                  }
                  if (opl != NULL)
                  {
                     int i;

                     for (i = 0; i < output.max_log_files; i++)
                     {
                        free(opl[i]);
                     }
                     free(opl);
                     opl = NULL;
                  }
               }
               if (output.fp != NULL)
               {
                  if (fclose(output.fp) == EOF)
                  {
                     (void)fprintf(stderr,
                                   "Failed to fclose() `%s' : %s (%s %d)\n",
                                   output.log_dir, strerror(errno),
                                   __FILE__, __LINE__);
                  }
                  output.fp = NULL;
                  output.bytes_read = 0;
               }
# endif
# ifdef _DELETE_LOG
               if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE))
               {
                  if (dcache != NULL)
                  {
                     free(dcache);
                     dcache = NULL;
                  }
                  if (dpl != NULL)
                  {
                     int i;

                     for (i = 0; i < delete.max_log_files; i++)
                     {
                        free(dpl[i]);
                     }
                     free(dpl);
                     dpl = NULL;
                  }
               }
               if (delete.fp != NULL)
               {
                  if (fclose(delete.fp) == EOF)
                  {
                     (void)fprintf(stderr,
                                   "Failed to fclose() `%s' : %s (%s %d)\n",
                                   delete.log_dir, strerror(errno),
                                   __FILE__, __LINE__);
                  }
                  delete.fp = NULL;
                  delete.bytes_read = 0;
               }
# endif
            }
         }
         (void)msa_detach();
#endif
      }
      else
      {
         alloc_jid(NULL);
         get_afd_info(-1);
         search_afd(NULL);
         dealloc_jid();
      }
      if ((mode & ALDA_CONTINUOUS_MODE) ||
          (mode & ALDA_CONTINUOUS_DAEMON_MODE))
      {
         (void)sleep(1L);
         if (data_printed == NO)
         {
            int          rotate = NO;
#ifdef HAVE_STATX
            struct statx stat_buf;
#else
            struct stat  stat_buf;
#endif

#ifdef _INPUT_LOG
            if (input.fp != NULL)
            {
# ifdef HAVE_STATX
               if (statx(0, input.log_dir, AT_STATX_SYNC_AS_STAT,
                         STATX_INO, &stat_buf) == -1)
# else
               if (stat(input.log_dir, &stat_buf) == -1)
# endif
               {
                  (void)fprintf(stderr, "Failed to access `%s' : %s (%s %d)\n",
                                input.log_dir, strerror(errno),
                                __FILE__, __LINE__);
               }
               else
               {
# ifdef HAVE_STATX
                  if (stat_buf.stx_ino != input.inode_number)
# else
                  if (stat_buf.st_ino != input.inode_number)
# endif
                  {
                     if (fclose(input.fp) == EOF)
                     {
                        (void)fprintf(stderr,
                                      "Failed to fclose() `%s' : %s (%s %d)\n",
                                      input.log_dir, strerror(errno),
                                      __FILE__, __LINE__);
                     }
                     input.fp = NULL;
                     input.bytes_read = 0;
                     rotate = YES;
                  }
               }
            }
#endif
#ifdef _DISTRIBUTION_LOG
            if (distribution.fp != NULL)
            {
# ifdef HAVE_STATX
               if (statx(0, distribution.log_dir, AT_STATX_SYNC_AS_STAT,
                         STATX_INO, &stat_buf) == -1)
# else
               if (stat(distribution.log_dir, &stat_buf) == -1)
# endif
               {
                  (void)fprintf(stderr, "Failed to access `%s' : %s (%s %d)\n",
                                distribution.log_dir, strerror(errno),
                                __FILE__, __LINE__);
               }
               else
               {
# ifdef HAVE_STATX
                  if (stat_buf.stx_ino != distribution.inode_number)
# else
                  if (stat_buf.st_ino != distribution.inode_number)
# endif
                  {
                     if (fclose(distribution.fp) == EOF)
                     {
                        (void)fprintf(stderr,
                                      "Failed to fclose() `%s' : %s (%s %d)\n",
                                      distribution.log_dir, strerror(errno),
                                      __FILE__, __LINE__);
                     }
                     distribution.fp = NULL;
                     distribution.bytes_read = 0;
                     rotate = YES;
                  }
               }
            }
#endif
#ifdef _PRODUCTION_LOG
            if (production.fp != NULL)
            {
# ifdef HAVE_STATX
               if (statx(0, production.log_dir, AT_STATX_SYNC_AS_STAT,
                         STATX_INO, &stat_buf) == -1)
# else
               if (stat(production.log_dir, &stat_buf) == -1)
# endif
               {
                  (void)fprintf(stderr, "Failed to access `%s' : %s (%s %d)\n",
                                production.log_dir, strerror(errno),
                                __FILE__, __LINE__);
               }
               else
               {
# ifdef HAVE_STATX
                  if (stat_buf.stx_ino != production.inode_number)
# else
                  if (stat_buf.st_ino != production.inode_number)
# endif
                  {
                     if (fclose(production.fp) == EOF)
                     {
                        (void)fprintf(stderr,
                                      "Failed to fclose() `%s' : %s (%s %d)\n",
                                      production.log_dir, strerror(errno),
                                      __FILE__, __LINE__);
                     }
                     production.fp = NULL;
                     production.bytes_read = 0;
                     rotate = YES;
                  }
               }
            }
#endif
#ifdef _OUTPUT_LOG
            if (output.fp != NULL)
            {
# ifdef HAVE_STATX
               if (statx(0, output.log_dir, AT_STATX_SYNC_AS_STAT,
                         STATX_INO, &stat_buf) == -1)
# else
               if (stat(output.log_dir, &stat_buf) == -1)
# endif
               {
                  (void)fprintf(stderr, "Failed to access `%s' : %s (%s %d)\n",
                                output.log_dir, strerror(errno),
                                __FILE__, __LINE__);
               }
               else
               {
# ifdef HAVE_STATX
                  if (stat_buf.stx_ino != output.inode_number)
# else
                  if (stat_buf.st_ino != output.inode_number)
# endif
                  {
                     if (fclose(output.fp) == EOF)
                     {
                        (void)fprintf(stderr,
                                      "Failed to fclose() `%s' : %s (%s %d)\n",
                                      output.log_dir, strerror(errno),
                                      __FILE__, __LINE__);
                     }
                     output.fp = NULL;
                     output.bytes_read = 0;
                     rotate = YES;
                  }
               }
            }
#endif
#ifdef _DELETE_LOG
            if (delete.fp != NULL)
            {
# ifdef HAVE_STATX
               if (statx(0, delete.log_dir, AT_STATX_SYNC_AS_STAT,
                         STATX_INO, &stat_buf) == -1)
# else
               if (stat(delete.log_dir, &stat_buf) == -1)
# endif
               {
                  (void)fprintf(stderr, "Failed to access `%s' : %s (%s %d)\n",
                                delete.log_dir, strerror(errno),
                                __FILE__, __LINE__);
               }
               else
               {
# ifdef HAVE_STATX
                  if (stat_buf.stx_ino != delete.inode_number)
# else
                  if (stat_buf.st_ino != delete.inode_number)
# endif
                  {
                     if (fclose(delete.fp) == EOF)
                     {
                        (void)fprintf(stderr,
                                      "Failed to fclose() `%s' : %s (%s %d)\n",
                                      delete.log_dir, strerror(errno),
                                      __FILE__, __LINE__);
                     }
                     delete.fp = NULL;
                     delete.bytes_read = 0;
                     rotate = YES;
                  }
               }
            }
#endif

            if ((rotate == YES) && (output_filename[0] != '\0'))
            {
               int  i,
                    with_rotate_number;
               char dst[MAX_PATH_LENGTH],
                    *p_end,
                    src[MAX_PATH_LENGTH];

               if (fclose(output_fp) == EOF)
               {
                  (void)fprintf(stderr,
                                "Failed to fclose() `%s' : %s (%s %d)\n",
                                output_filename, strerror(errno),
                                __FILE__, __LINE__);
                  exit(INCORRECT);
               }

               (void)strcpy(src, output_filename);
               p_end = src + strlen(output_filename);
               if (((p_end - 2) > src) && (*(p_end - 2) == '.') &&
                   (*(p_end - 1) == '0'))
               {
                  p_end -= 2;
                  with_rotate_number = YES;
               }
               else
               {
                  with_rotate_number = NO;
               }
               for (i = (rotate_limit - 1); i > 0; i--)
               {
                  (void)snprintf(p_end, MAX_PATH_LENGTH - (p_end - dst),
                                 ".%d", i);
                  (void)my_strncpy(dst, src, MAX_PATH_LENGTH);
                  if ((i == 1) && (with_rotate_number == NO))
                  {
                     *p_end = '\0';
                  }
                  else
                  {
                     (void)snprintf(p_end, MAX_PATH_LENGTH - (p_end - dst),
                                    ".%d", i - 1);
                  }

                  if (rename(src, dst) < 0)
                  {
                     if (errno != ENOENT)
                     {
                        (void)fprintf(stderr,
                                      "Failed to rename() `%s' to `%s' : %s (%s %d)\n",
                                      src, dst, strerror(errno),
                                      __FILE__, __LINE__);
                     }
                  }
               }
               if ((output_fp = fopen(output_filename, "a")) == NULL)
               {
                  (void)fprintf(stderr, "Failed to fopen() `%s' : %s (%s %d)\n",
                                output_filename, strerror(errno), __FILE__, __LINE__);
                  exit(INCORRECT);
               }
            }
         }
      }
      else
      {
         break;
      }
   }
   if (verbose)
   {
      end = time(NULL);
   }

   if (verbose)
   {
#if SIZEOF_TIME_T == 4
      (void)printf("Search time = %ld\n", (pri_time_t)(end - start));
#else
      (void)printf("Search time = %lld\n", (pri_time_t)(end - start));
#endif
   }

#ifdef CACHE_DEBUG
   print_alda_cache();
#endif

   exit(SUCCESS);
}


/*########################### get_afd_info() ############################*/
static void
get_afd_info(int msa_pos)
{
   if (msa_pos == -1)
   {
      if (gethostname(afd.hostname, MAX_REAL_HOSTNAME_LENGTH) == -1)
      {
         (void)fprintf(stderr, "gethostname() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         afd.hostname[0] = '\0';
         afd.hostname_length = '\0';
      }
      else
      {
         afd.hostname_length = (int)strlen(afd.hostname);
      }
      if (get_afd_name(afd.aliasname) == INCORRECT)
      {
         (void)strcpy(afd.aliasname, afd.hostname);
         afd.aliasname_length = afd.hostname_length;
      }
      else
      {
         afd.aliasname_length = (int)strlen(afd.aliasname);
      }
      (void)strcpy(afd.version, PACKAGE_VERSION);
   }
   else
   {
#ifdef WITH_AFD_MON
      (void)strcpy(afd.hostname, msa[msa_pos].hostname[(int)msa[msa_pos].afd_toggle]);
      (void)strcpy(afd.aliasname, msa[msa_pos].afd_alias);
      (void)strcpy(afd.version, msa[msa_pos].afd_version);
#else
      afd.hostname[0] = '\0';
      afd.aliasname[0] = '\0';
      afd.version[0] = '\0';
#endif
      afd.hostname_length = (int)strlen(afd.hostname);
      afd.aliasname_length = (int)strlen(afd.aliasname);
   }
   afd.version_length = (int)strlen(afd.version);

   return;
}


/*############################ search_afd() #############################*/
static void
search_afd(char *search_afd)
{
   int          ret,
#ifdef _PRODUCTION_LOG
                prev_proc_cycles,
                prod_counter = 0,
                save_search_loop = -1,
#endif
                search_loop;
#ifdef _DISTRIBUTION_LOG
   int          dis_counter = -1,
                dis_type_counter = -1;
#endif
#if defined (_PRODUCTION_LOG) && defined (_DISTRIBUTION_LOG)
   int          cache_data;
#endif
   unsigned int got_data,
                more_log_data,
#if defined (_INPUT_LOG) || defined (_DISTRIBUTION_LOG) || defined (_PRODUCTION_LOG)
                prev_dir_id,
#endif
                prev_job_id = 0,
                *p_prev_split_job_counter = NULL,
                *p_prev_unique_number = NULL;
   time_t       prev_log_time = 0L,
                start_search_time = 0; /* Silence compiler. */
   off_t        prev_filename_length = 0;
   char         *p_file_pattern;

   log_data_written = 0;
   more_log_data = search_log_type;
   got_data = 0;
   search_loop = 0;
   init_time_start = 0L;
   if (max_search_time)
   {
      start_search_time = time(NULL);
   }
   if (mode & ALDA_FORWARD_MODE)
   {
      do
      {
#ifdef _INPUT_LOG
         if ((search_log_type & SEARCH_INPUT_LOG) && (search_loop == 0))
         {
            RESET_ILOG();
            if ((ret = check_input_log(search_afd, NULL, -1, 0, 0)) == GOT_DATA)
            {
               got_data |= SEARCH_INPUT_LOG;
               init_time_start = ilog.input_time;
               if (trace_mode == ON)
               {
                  start_time_end = 0L;
               }
            }
            else if (ret == NO_LOG_DATA)
                 {
                    if (input.current_file_no == 0)
                    {
                       break;
                    }
                    else
                    {
                       RESET_ILOG();
                       more_log_data &= ~SEARCH_INPUT_LOG;
                    }
                 }
            else if (ret == SEARCH_TIME_UP)
                 {
                    break;
                 }
                 else
                 {
                    search_log_type &= ~SEARCH_INPUT_LOG;
                 }
         }
#endif
#ifdef _DISTRIBUTION_LOG
         if ((search_log_type & SEARCH_DISTRIBUTION_LOG) &&
             ((search_loop == 0) || (search_loop & SEARCH_DISTRIBUTION_LOG)))
         {
# ifdef _INPUT_LOG
            if (((search_log_type & SEARCH_INPUT_LOG) == 0) ||
                (ilog.filename[0] == '\0'))
            {
               p_file_pattern = NULL;
               prev_log_time = 0L;
               p_prev_unique_number = NULL;
               prev_dir_id = 0;
            }
            else
            {
               if (ulog.job_id_list == NULL)
               {
                  p_file_pattern = ilog.filename;
                  prev_filename_length = ilog.filename_length;
                  prev_log_time = ilog.input_time;
                  p_prev_unique_number = &ilog.unique_number;
                  prev_dir_id = ilog.dir_id;
               }
               else
               {
                  p_file_pattern = ilog.filename;
                  prev_filename_length = ilog.filename_length;
                  prev_log_time = 0L;
                  p_prev_unique_number = NULL;
                  prev_dir_id = ilog.dir_id;
               }
            }
# else
            p_file_pattern = NULL;
            prev_log_time = 0L;
            p_prev_unique_number = NULL;
            prev_dir_id = 0;
# endif
            RESET_ULOG_PART();
            ret = check_distribution_log(search_afd, p_file_pattern,
                                         prev_filename_length,
                                         prev_log_time, prev_dir_id,
                                         p_prev_unique_number);
            if (verbose > 3)
            {
               (void)fprintf(stdout,
# if SIZEOF_TIME_T == 4
                             "%06ld DEBUG 4: check_distribution_log() returned %d\n",
# else
                             "%06lld DEBUG 4: check_distribution_log() returned %d\n",
# endif
                             (pri_time_t)(time(NULL) - start), ret);
            }
            if (ret == GOT_DATA)
            {
               got_data |= SEARCH_DISTRIBUTION_LOG;
               init_time_start = ulog.distribution_time;
               dis_counter = 0;
               if (dis_type_counter == -1)
               {
                  dis_type_counter = ulog.no_of_distribution_types;
               }
               if (trace_mode == ON)
               {
                  if (search_unique_number == (unsigned int)-1)
                  {
                     /*
                      * Restore the search_unique_number with the new
                      * changed unique number after the queue was stopped
                      * and then reopened. This is then the second time
                      * we have searched in DISTRIBUTION_LOG.
                      */
                     search_unique_number = ulog.unique_number;
                  }
                  if (ulog.proc_cycles[dis_counter] > 0)
                  {
                     search_loop = SEARCH_PRODUCTION_LOG | SEARCH_OUTPUT_LOG |
                                   SEARCH_DELETE_LOG;
                  }
                  else
                  {
                     search_loop = SEARCH_OUTPUT_LOG | SEARCH_DELETE_LOG;

# ifdef _PRODUCTION_LOG
                     /* We must reset PRODUCTION_LOG data! */
                     RESET_PLOG();
# endif
                  }
                  if (ulog.distribution_type == DISABLED_DIS_TYPE)
                  {
                     search_loop = 0;
# ifdef _DELETE_LOG
                     (void)memcpy(&dlog.bd_delete_time,
                                  &ulog.bd_distribution_time, sizeof(struct tm));
                     (void)memcpy(&dlog.bd_job_creation_time, 
                                  &ulog.bd_input_time, sizeof(struct tm));
                     (void)strcpy(dlog.filename, ulog.filename);
                     dlog.alias_name[0] = '\0';
                     (void)strcpy(dlog.user_process, AMG);
                     (void)strcpy(dlog.add_reason, "Host disabled");
                     dlog.file_size = ulog.file_size;
                     dlog.job_creation_time = ulog.input_time;
                     dlog.delete_time = ulog.distribution_time;
                     dlog.filename_length = ulog.filename_length;
                     dlog.alias_name_length = 0;
                     dlog.user_process_length = AMG_LENGTH;
                     dlog.add_reason_length = sizeof("Host disabled") - 1;
                     dlog.job_id = ulog.job_id_list[dis_counter];
                     dlog.dir_id = ulog.dir_id;
                     dlog.deletion_type = DELETE_HOST_DISABLED;
                     dlog.unique_number = ulog.unique_number;
                     dlog.split_job_counter = 0;
#  ifdef _OUTPUT_LOG
                     olog.output_type = OT_HOST_DISABLED_DELETE;
                     olog.job_id = ulog.job_id_list[dis_counter];
                     olog.output_time = ulog.distribution_time;
#  endif
                     got_data |= SEARCH_DELETE_LOG;
# endif
                  }
# ifdef WITH_DUP_CHECK
                  else if (ulog.distribution_type == DUPCHECK_DIS_TYPE)
                       {
#  ifdef _OUTPUT_LOG
                          olog.output_type = OT_DUPLICATE;
                          olog.job_id = ulog.job_id_list[dis_counter];
                          olog.output_time = ulog.distribution_time;
#  endif
                          search_loop = SEARCH_DELETE_LOG;
                       }
                           
# endif
                  else if (ulog.distribution_type == AGE_LIMIT_DELETE_DIS_TYPE)
                       {
# ifdef _OUTPUT_LOG
                          olog.output_type = OT_AGE_LIMIT_DELETE;
                          olog.job_id = ulog.job_id_list[dis_counter];
                          olog.output_time = ulog.distribution_time;
# endif
                          search_loop = SEARCH_DELETE_LOG;
                       }
                  else if (ulog.distribution_type == QUEUE_STOPPED_DIS_TYPE)
                       {
                          if (search_unique_number != 0)
                          {
                             /*
                              * Temporaly unset the search_unique_number
                              * because when queue stops the it gets a
                              * new unique number. We then need to store
                              * the new unique number after we searched
                              * DISTRIBUTION_LOG again.
                              */
                             search_unique_number = (unsigned int)-1;
                          }
                          if (ulog.proc_cycles[dis_counter] > 0)
                          {
                             search_loop = SEARCH_DISTRIBUTION_LOG | SEARCH_PRODUCTION_LOG | SEARCH_OUTPUT_LOG |
                                           SEARCH_DELETE_LOG;
                          }
                          else
                          {
                             search_loop = SEARCH_DISTRIBUTION_LOG | SEARCH_OUTPUT_LOG | SEARCH_DELETE_LOG;

# ifdef _PRODUCTION_LOG
                             /* We must reset PRODUCTION_LOG data! */
                             RESET_PLOG();
# endif
                          }
                          RESET_ULOG_PART();
                          dis_counter = -1;
                          continue;
                       }
               }
            }
            else if (ret == NO_LOG_DATA)
                 {
                    if ((trace_mode == ON) &&
                        (search_loop & SEARCH_DISTRIBUTION_LOG))
                    {
                       search_loop = 0;
                       dis_counter = -1;
                       continue;
                    }
                    else
                    {
                       RESET_ULOG_PART();
                       more_log_data &= ~SEARCH_DISTRIBUTION_LOG;
                       dis_counter = -1;
                    }
                 }
                 else
                 {
                    search_log_type &= ~SEARCH_DISTRIBUTION_LOG;
                    dis_counter = -1;
                 }
         }
#endif
#ifdef _PRODUCTION_LOG
         if ((search_log_type & SEARCH_PRODUCTION_LOG) &&
# ifdef _DISTRIBUTION_LOG
             ((ulog.filename[0] == '\0') ||
              (ulog.distribution_type < QUEUE_STOPPED_DIS_TYPE)) &&
# endif
             ((search_loop == 0) || (search_loop & SEARCH_PRODUCTION_LOG)))
         {
# ifdef _DISTRIBUTION_LOG
            if (((search_log_type & SEARCH_DISTRIBUTION_LOG) == 0) ||
                (ulog.filename[0] == '\0'))
            {
#  ifdef _INPUT_LOG
               if (((search_log_type & SEARCH_INPUT_LOG) == 0) ||
                   (ilog.filename[0] == '\0'))
               {
                  p_file_pattern = NULL;
                  prev_log_time = 0L;
                  prev_job_id = 0;
                  prev_proc_cycles = -1;
                  p_prev_unique_number = NULL;
                  prev_dir_id = 0;
               }
               else
               {
                  p_file_pattern = ilog.filename;
                  prev_filename_length = ilog.filename_length;
                  prev_log_time = ilog.input_time;
                  prev_job_id = 0;
                  prev_proc_cycles = -1;
                  p_prev_unique_number = &ilog.unique_number;
                  prev_dir_id = ilog.dir_id;
               }
#  else
               p_file_pattern = NULL;
               prev_log_time = 0L;
               prev_job_id = 0;
               prev_proc_cycles = -1;
               p_prev_unique_number = NULL;
               prev_dir_id = 0;
#  endif
            }
            else
            {
               p_file_pattern = ulog.filename;
               prev_filename_length = ulog.filename_length;
               if (dis_counter == -1)
               {
                  prev_job_id = 0;
                  prev_proc_cycles = -1;
               }
               else
               {
                  prev_job_id = ulog.job_id_list[dis_counter];
                  prev_proc_cycles = ulog.proc_cycles[dis_counter];
               }
               if (ulog.distribution_type == TIME_JOB_DIS_TYPE)
               {
                  prev_log_time = 0;
                  p_prev_unique_number = NULL;
               }
               else
               {
                  prev_log_time = ulog.input_time;
                  p_prev_unique_number = &ulog.unique_number;
               }
               prev_dir_id = ulog.dir_id;
            }
# else
#  ifdef _INPUT_LOG
            if (((search_log_type & SEARCH_INPUT_LOG) == 0) ||
                (ilog.filename[0] == '\0'))
            {
               p_file_pattern = NULL;
               prev_log_time = 0L;
               prev_job_id = 0;
               prev_proc_cycles = -1;
               p_prev_unique_number = NULL;
               prev_dir_id = 0;
            }
            else
            {
               p_file_pattern = ilog.filename;
               prev_filename_length = ilog.filename_length;
               prev_log_time = ilog.input_time;
               prev_job_id = 0;
               prev_proc_cycles = -1;
               p_prev_unique_number = &ilog.unique_number;
               prev_dir_id = ilog.dir_id;
            }
#  else
            p_file_pattern = NULL;
            prev_log_time = 0L;
            prev_job_id = 0;
            prev_proc_cycles = -1;
            p_prev_unique_number = NULL;
            prev_dir_id = 0;
#  endif
# endif
            RESET_PLOG();
            if ((ret = check_production_log(search_afd, p_file_pattern,
                                            prev_filename_length,
                                            prev_log_time, prev_dir_id,
                                            prev_job_id, prev_proc_cycles,
                                            p_prev_unique_number,
                                            NULL)) == GOT_DATA)
            {
               got_data |= SEARCH_PRODUCTION_LOG;
               if (trace_mode == ON)
               {
                  if (((plog.return_code != 0) &&
                       (plog.new_filename[0] == '\0')) ||
                      (plog.ratio_2 == 0))
                  {
                     search_loop = SEARCH_PRODUCTION_LOG;
                     prod_counter = 0;
                     if (my_strcmp(plog.what_done, DELETE_ID) == 0)
                     {
# ifdef _DELETE_LOG
                        dlog.alias_name[0] = '\0';
                        (void)strcpy(dlog.user_process, AMG);
                        (void)strcpy(dlog.add_reason, DELETE_ID);
#  ifdef _DISTRIBUTION_LOG
                        dlog.file_size = ulog.file_size;
                        dlog.delete_time = ulog.distribution_time;
                        dlog.dir_id = ulog.dir_id;
#  else
#   ifdef _INPUT_LOG
                        dlog.file_size = ilog.file_size;
                        dlog.delete_time = ilog.input_time;
                        dlog.dir_id = ilog.dir_id;
#   else
                        dlog.file_size = 0;
                        dlog.delete_time = 0;
#    ifdef _PRODUCTION_LOG
                        dlog.dir_id = plog.dir_id;
#    else
                        dlog.dir_id = 0;
#    endif
#   endif
#  endif
#  ifdef _PRODUCTION_LOG
                        (void)memcpy(&dlog.bd_delete_time,
                                     &plog.bd_input_time, sizeof(struct tm));
                        (void)memcpy(&dlog.bd_job_creation_time,
                                     &plog.bd_input_time, sizeof(struct tm));
                        (void)strcpy(dlog.filename, plog.original_filename);
                        dlog.job_creation_time = plog.input_time;
                        dlog.filename_length = plog.original_filename_length;
                        dlog.unique_number = plog.unique_number;
                        dlog.split_job_counter = plog.split_job_counter;
#  else
                        (void)memset(&dlog.bd_delete_time,
                                     0, sizeof(struct tm));
                        (void)memset(&dlog.bd_job_creation_time,
                                     0, sizeof(struct tm));
                        dlog.filename[0] = '\0';
                        dlog.job_creation_time = 0;
                        dlog.filename_length = 0;
                        dlog.unique_number = 0;
                        dlog.split_job_counter = 0;
#  endif
                        dlog.alias_name_length = 0;
                        dlog.user_process_length = AMG_LENGTH;
                        dlog.add_reason_length = DELETE_ID_LENGTH;
                        dlog.job_id = 0;
                        dlog.deletion_type = DELETE_OPTION;
                        got_data |= SEARCH_DELETE_LOG;
# endif
                     }
                     else if (my_strcmp(plog.what_done, TIFF2GTS_ID) == 0)
                          {
                             search_loop |= SEARCH_DELETE_LOG;
                          }

# ifdef _OUTPUT_LOG
                     /* Remove the old OUTPUT_LOG information or it */
                     /* will be shown by print_data() function.     */
                     RESET_OLOG();
# endif
                  }
                  else
                  {
                     search_loop = SEARCH_PRODUCTION_LOG | SEARCH_OUTPUT_LOG;
                     if (prod_counter == 0)
                     {
                        if (plog.ratio_2 > plog.ratio_1)
                        {
                           prod_counter = plog.ratio_2;
                        }
                        else
                        {
                           prod_counter = plog.ratio_1;
                           if (plog.ratio_2 == 1)
                           {
# ifdef _OUTPUT_LOG
                              if (odata != NULL)
                              {
                                 int gotchas = 0,
                                     i;

                                 for (i = 0; i < odata_entries; i++)
                                 {
                                    if (odata[i].cache_todo == odata[i].cache_done)
                                    {
                                       gotchas++;
                                    }
                                 }
                                 if (gotchas == odata_entries)
                                 {
                                    free(odata);
                                    odata = NULL;
                                    odata_entries = 0;
                                 }
                              }
# endif
# ifdef _DELETE_LOG
                              if (ddata != NULL)
                              {
                                 int gotchas = 0,
                                     i;

                                 for (i = 0; i < ddata_entries; i++)
                                 {
                                    if (ddata[i].cache_todo == ddata[i].cache_done)
                                    {
                                       gotchas++;
                                    }
                                 }
                                 if (gotchas == ddata_entries)
                                 {
                                    free(ddata);
                                    ddata = NULL;
                                    ddata_entries = 0;
                                 }
                              }
# endif
                           }
                        }
                        if (prod_counter == 1)
                        {
                           prod_counter = 0;
                        }
                     }
                  }
               }
            }
            else if (ret == NO_LOG_DATA)
                 {
                    RESET_PLOG();
                    more_log_data &= ~SEARCH_PRODUCTION_LOG;
                    search_loop &= ~SEARCH_PRODUCTION_LOG;
                 }
                 else
                 {
                    search_log_type &= ~SEARCH_PRODUCTION_LOG;
                 }
         }
#endif /* _PRODUCTION_LOG */
#ifdef _OUTPUT_LOG
         if ((search_log_type & SEARCH_OUTPUT_LOG) &&
# ifdef _DISTRIBUTION_LOG
             ((ulog.filename[0] == '\0') ||
              (ulog.distribution_type < QUEUE_STOPPED_DIS_TYPE)) &&
# endif
             ((search_loop == 0) || (search_loop & SEARCH_OUTPUT_LOG)))
         {
# if defined (_PRODUCTION_LOG) && defined (_DISTRIBUTION_LOG)
            cache_data = NO;
            if ((plog.ratio_1 > plog.ratio_2) && (odata_entries > 0))
            {
               int i;

               for (i = 0; i < odata_entries; i++)
               {
                  if (odata[i].job_id == plog.job_id)
                  {
                     (void)memcpy(&olog, &odata[i], sizeof(struct alda_odata));
                     odata[i].cache_done++;

                     if ((protocols & olog.protocol) &&
                         (check_host_alias(olog.alias_name, olog.real_hostname,
                                           olog.current_toggle) == SUCCESS))
                     {
                        got_data |= SEARCH_OUTPUT_LOG;
                     }
                     else
                     {
                        got_data = 0;
                     }
                     cache_data = YES;

                     break;
                  }
               }
            }

            if (cache_data == NO)
            {
# endif
# ifdef _PRODUCTION_LOG
               if (((search_log_type & SEARCH_PRODUCTION_LOG) == 0) ||
                   (plog.new_filename[0] == '\0'))
               {
#  ifdef _DISTRIBUTION_LOG
                  if (((search_log_type & SEARCH_DISTRIBUTION_LOG) == 0) ||
                      (ulog.filename[0] == '\0'))
                  {
#   ifdef _INPUT_LOG
                     if (((search_log_type & SEARCH_INPUT_LOG) == 0) ||
                         (ilog.filename[0] == '\0'))
                     {
                        p_file_pattern = NULL;
                        prev_log_time = 0L;
                        prev_job_id = 0;
                        p_prev_unique_number = NULL;
                     }
                     else
                     {
                        p_file_pattern = ilog.filename;
                        prev_filename_length = ilog.filename_length;
                        prev_log_time = ilog.input_time;
                        prev_job_id = 0;
                        p_prev_unique_number = &ilog.unique_number;
                        p_prev_split_job_counter = NULL;
                     }
#   else
                     p_file_pattern = NULL;
                     prev_log_time = 0L;
                     prev_job_id = 0;
                     p_prev_unique_number = NULL;
#   endif
                     p_prev_split_job_counter = NULL;
                  }
                  else
                  {
                     p_file_pattern = ulog.filename;
                     prev_filename_length = ulog.filename_length;
                     prev_log_time = ulog.input_time;
                     if (dis_counter == -1)
                     {
                        prev_job_id = 0;
                     }
                     else
                     {
                        prev_job_id = ulog.job_id_list[dis_counter];
                     }
                     p_prev_unique_number = &ulog.unique_number;
                     p_prev_split_job_counter = NULL;
                  }
#  else
                  p_file_pattern = NULL;
                  prev_log_time = 0L;
                  prev_job_id = 0;
                  p_prev_unique_number = NULL;
                  p_prev_split_job_counter = NULL;
#  endif
               }
               else
               {
                  p_file_pattern = plog.new_filename;
                  prev_filename_length = plog.new_filename_length;
                  prev_log_time = plog.input_time;
                  prev_job_id = plog.job_id;
                  p_prev_unique_number = &plog.unique_number;
                  p_prev_split_job_counter = &plog.split_job_counter;
               }
# else /* !_PRODUCTION_LOG */
#  ifdef _INPUT_LOG
               if (((search_log_type & SEARCH_INPUT_LOG) == 0) ||
                   (ilog.filename[0] == '\0'))
               {
#   ifdef _DISTRIBUTION_LOG
                  if (((search_log_type & SEARCH_DISTRIBUTION_LOG) == 0) ||
                      (ulog.filename[0] == '\0'))
                  {
                     p_file_pattern = ulog.filename;
                     prev_filename_length = ulog.filename_length;
                     prev_log_time = ulog.input_time;
                     if (dis_counter == -1)
                     {
                        prev_job_id = 0;
                     }
                     else
                     {
                        prev_job_id = ulog.job_id_list[dis_counter];
                     }
                     p_prev_unique_number = &ulog.unique_number;
                  }
                  else
                  {
                     p_file_pattern = NULL;
                     prev_log_time = 0L;
                     prev_job_id = 0;
                     p_prev_unique_number = NULL;
                  }
#   else
                  p_file_pattern = NULL;
                  prev_log_time = 0L;
                  prev_job_id = 0;
                  p_prev_unique_number = NULL;
#   endif
                  p_prev_split_job_counter = NULL;
               }
               else
               {
                  p_file_pattern = ilog.filename;
                  prev_filename_length = ilog.filename_length;
                  prev_log_time = ilog.input_time;
                  prev_job_id = 0;
                  p_prev_unique_number = &ilog.unique_number;
                  p_prev_split_job_counter = NULL;
               }
#  else
               p_file_pattern = NULL;
               prev_log_time = 0L;
               prev_job_id = 0;
               p_prev_unique_number = NULL;
               p_prev_split_job_counter = NULL;
#  endif
# endif /* _PRODUCTION_LOG */
               RESET_OLOG();
               if ((ret = check_output_log(search_afd, p_file_pattern,
                                           prev_filename_length, prev_log_time,
                                           prev_job_id, p_prev_unique_number,
                                           p_prev_split_job_counter)) == GOT_DATA)
               {
                  if ((olog.output_type == OT_NORMAL_DELIVERED) ||
                      (olog.output_type == OT_NORMAL_RECEIVED))
                  {
                     if ((protocols & olog.protocol) &&
                         (check_host_alias(olog.alias_name, olog.real_hostname,
                                           olog.current_toggle) == SUCCESS))
                     {
                        got_data |= SEARCH_OUTPUT_LOG;
                     }
                     else
                     {
                        got_data = 0;
                     }
# if defined (_PRODUCTION_LOG) && defined (_DISTRIBUTION_LOG)
                     if ((prod_counter > 0) && (plog.ratio_1 > plog.ratio_2) &&
                         ((plog.ratio_1 - prod_counter) <= ulog.no_of_dist_jobs))
                     {
                        if ((odata_entries % N_TO_1_CACHE_STEP_SIZE) == 0)
                        {
                           size_t cache_size;

                           cache_size = ((odata_entries / N_TO_1_CACHE_STEP_SIZE) + 1) *
                                        N_TO_1_CACHE_STEP_SIZE * sizeof(struct alda_odata);
                           if ((odata = realloc(odata, cache_size)) == NULL)
                           {
                              (void)fprintf(stderr,
                                            "Failed to realloc() memory : %s (%s %d)\n",
                                            strerror(errno), __FILE__, __LINE__);
                              exit(INCORRECT);
                           }
                        }
                        (void)memcpy(&odata[odata_entries], &olog, sizeof(struct alda_odata));
                        odata[odata_entries].cache_done = 1;
                        odata[odata_entries].cache_todo = plog.ratio_1;
                        odata_entries++;
                     }
# endif
                  }
                  else if (olog.output_type == OT_DUPLICATE_STORED) /* Dup check store. */
                       {
                          if (search_log_type == SEARCH_ALL_LOGS)
                          {
                             RESET_OLOG();
                             more_log_data &= ~SEARCH_OUTPUT_LOG;
                          }
                          else
                          {
                             got_data = 0;
                          }
                       }
                       else /* Age limit, dup check delete, file */
                            /* currently transmitted by other    */
                            /* process.                          */
                       {
                          if (search_log_type == SEARCH_ALL_LOGS)
                          {
# ifdef _DELETE_LOG
                             (void)strcpy(dlog.alias_name, olog.alias_name);
                             if (olog.protocol == ALDA_FTP_FLAG)
                             {
                                (void)strcpy(dlog.user_process, SEND_FILE_FTP);
                                dlog.user_process_length = SEND_FILE_FTP_LENGTH;
                             }
                             else if (olog.protocol == ALDA_LOC_FLAG)
                                  {
                                     (void)strcpy(dlog.user_process, SEND_FILE_LOC);
                                     dlog.user_process_length = SEND_FILE_LOC_LENGTH;
                                  }
                             else if (olog.protocol == ALDA_EXEC_FLAG)
                                  {
                                     (void)strcpy(dlog.user_process, SEND_FILE_EXEC);
                                     dlog.user_process_length = SEND_FILE_EXEC_LENGTH;
                                  }
                             else if ((olog.protocol == ALDA_SMTP_FLAG) ||
                                      (olog.protocol == ALDA_DE_MAIL_FLAG))
                                  {
                                     (void)strcpy(dlog.user_process, SEND_FILE_SMTP);
                                     dlog.user_process_length = SEND_FILE_SMTP_LENGTH;
                                  }
                             else if (olog.protocol == ALDA_SFTP_FLAG)
                                  {
                                     (void)strcpy(dlog.user_process, SEND_FILE_SFTP);
                                     dlog.user_process_length = SEND_FILE_SFTP_LENGTH;
                                  }
                             else if (olog.protocol == ALDA_SCP_FLAG)
                                  {
                                     (void)strcpy(dlog.user_process, "sf_scp");
                                     dlog.user_process_length = 6;
                                  }
                             else if (olog.protocol == ALDA_HTTP_FLAG)
                                  {
                                     (void)strcpy(dlog.user_process, SEND_FILE_HTTP);
                                     dlog.user_process_length = SEND_FILE_HTTP_LENGTH;
                                  }
                             else if (olog.protocol == ALDA_HTTPS_FLAG)
                                  {
                                     (void)strcpy(dlog.user_process, "sf_https");
                                     dlog.user_process_length = 8;
                                  }
                             else if (olog.protocol == ALDA_FTPS_FLAG)
                                  {
                                     (void)strcpy(dlog.user_process, "sf_ftps");
                                     dlog.user_process_length = 7;
                                  }
                             else if (olog.protocol == ALDA_WMO_FLAG)
                                  {
                                     (void)strcpy(dlog.user_process, "sf_wmo");
                                     dlog.user_process_length = 6;
                                  }
                             else if (olog.protocol == ALDA_MAP_FLAG)
                                  {
                                     (void)strcpy(dlog.user_process, "sf_map");
                                     dlog.user_process_length = 6;
                                  }
                             else if (olog.protocol == ALDA_DFAX_FLAG)
                                  {
                                     (void)strcpy(dlog.user_process, "sf_dfax");
                                     dlog.user_process_length = 7;
                                  }
                                  else
                                  {
                                     (void)strcpy(dlog.user_process, "sf_xxx");
                                     dlog.user_process_length = 6;
                                  }
                             if (olog.output_type == OT_AGE_LIMIT_DELETE) /* Age-limit */
                             {
                                dlog.deletion_type = AGE_OUTPUT;
                                dlog.add_reason[0] = '\0';
                                dlog.add_reason_length = 0;
                             }
                             else if (olog.output_type == OT_DUPLICATE_DELETE) /* dupcheck delete */
                                  {
                                     dlog.deletion_type = DUP_OUTPUT;
                                     dlog.add_reason[0] = '\0';
                                     dlog.add_reason_length = 0;
                                  }
                             else if (olog.output_type == OT_OTHER_PROC_DELETE) /* currently transmitted */
                                  {
                                     dlog.deletion_type = FILE_CURRENTLY_TRANSMITTED;
                                     dlog.add_reason[0] = '\0';
                                     dlog.add_reason_length = 0;
                                  }
                             else if (olog.output_type == OT_ADRESS_REJ_DELETE) /* Recipient unknown */
                                  {
                                     dlog.deletion_type = RECIPIENT_REJECTED;
                                     dlog.add_reason[0] = '\0';
                                     dlog.add_reason_length = 0;
                                  }
                                  else
                                  {
                                     dlog.deletion_type = 0;
                                     (void)strcpy(dlog.add_reason, UKN_DEL_REASON_STR);
                                     dlog.add_reason_length = UKN_DEL_REASON_STR_LENGTH;
                                  }
                             dlog.file_size = olog.file_size;
                             dlog.delete_time = olog.output_time;
#  ifdef _DISTRIBUTION_LOG
                             dlog.dir_id = ulog.dir_id;
#  else
#   ifdef _INPUT_LOG
                             dlog.dir_id = ilog.dir_id;
#   else
#    ifdef _PRODUCTION_LOG
                             dlog.dir_id = plog.dir_id;
#    else
                             dlog.dir_id = 0;
#    endif
#   endif
#  endif
                             (void)memcpy(&dlog.bd_delete_time,
                                          &olog.bd_output_time, sizeof(struct tm));
                             (void)memcpy(&dlog.bd_job_creation_time,
                                          &olog.bd_job_creation_time, sizeof(struct tm));
                             (void)strcpy(dlog.filename, olog.local_filename);
                             dlog.job_creation_time = olog.job_creation_time;
                             dlog.filename_length = olog.remote_name_length;
                             dlog.unique_number = olog.unique_number;
                             dlog.split_job_counter = olog.split_job_counter;
                             dlog.alias_name_length = olog.alias_name_length;
                             dlog.job_id = olog.job_id;
                             got_data |= SEARCH_DELETE_LOG;
# endif
                             RESET_OLOG();
                             more_log_data &= ~SEARCH_OUTPUT_LOG;
                          }
                          else
                          {
                             got_data = 0;
                          }
                       }
               }
               else if (ret == NO_LOG_DATA)
                    {
                       RESET_OLOG();
                       more_log_data &= ~SEARCH_OUTPUT_LOG;
                       search_loop = SEARCH_DELETE_LOG;
                    }
                    else
                    {
                       search_log_type &= ~SEARCH_OUTPUT_LOG;
                    }
# if defined (_PRODUCTION_LOG) && defined (_DISTRIBUTION_LOG)
            }
# endif
         }
         else
         {
#ifdef _DISTRIBUTION_LOG
            if ((ulog.distribution_type == DISABLED_DIS_TYPE) ||
                (ulog.distribution_type == DUPCHECK_DIS_TYPE) ||
                (ulog.distribution_type == AGE_LIMIT_DELETE_DIS_TYPE))
            {
               int          tmp_output_type = olog.output_type;
               time_t       tmp_output_time = olog.output_time;
               unsigned int tmp_job_id = olog.job_id;

               RESET_OLOG();
               olog.output_type = tmp_output_type;
               olog.output_time = tmp_output_time;
               olog.send_start_time = tmp_output_time;
               olog.job_id = tmp_job_id;
               (void)get_recipient_alias(olog.job_id);
            }
            else
            {
#endif
               RESET_OLOG();
#ifdef _DISTRIBUTION_LOG
            }
#endif
            more_log_data &= ~SEARCH_OUTPUT_LOG;
         }
#endif /* _OUTPUT_LOG */
#ifdef _DELETE_LOG
         if (((search_log_type & SEARCH_DELETE_LOG) &&
# ifdef _DISTRIBUTION_LOG
             ((search_loop == SEARCH_DELETE_LOG) ||
              (ulog.filename[0] == '\0') ||
#  ifdef _PRODUCTION_LOG
              (plog.ratio_2 == 0) ||
#  endif
              (ulog.distribution_type > DISABLED_DIS_TYPE)) &&
# endif
              ((search_loop == 0) || (search_loop & SEARCH_DELETE_LOG)) &&
              ((got_data & SEARCH_DELETE_LOG) == 0))
# ifdef _INPUT_LOG
             && (((search_log_type & SEARCH_INPUT_LOG) == 0)
#  ifdef _OUTPUT_LOG
                 || (search_log_type & SEARCH_OUTPUT_LOG)
#  else
#   ifdef _PRODUCTION_LOG
                 || (search_log_type & SEARCH_PRODUCTION_LOG)
#   endif
#  endif
                 || (ilog.filename[0] == '\0'))
# endif
# ifdef _PRODUCTION_LOG
             && (((search_log_type & SEARCH_PRODUCTION_LOG) == 0)
#  ifdef _OUTPUT_LOG
                 || (search_log_type & SEARCH_OUTPUT_LOG)
#  endif
                 || (plog.new_filename[0] == '\0'))
# endif
# ifdef _OUTPUT_LOG
             && (((search_log_type & SEARCH_OUTPUT_LOG) == 0) ||
                 (olog.local_filename[0] == '\0'))
# endif
             )
         {
# if defined (_PRODUCTION_LOG) && defined (_DISTRIBUTION_LOG)
            cache_data = NO;
            if ((plog.ratio_1 > plog.ratio_2) && (ddata_entries > 0))
            {
               int i;

               for (i = 0; i < ddata_entries; i++)
               {
                  if (ddata[i].job_id == plog.job_id)
                  {
                     (void)memcpy(&dlog, &ddata[i], sizeof(struct alda_ddata));
                     ddata[i].cache_done++;
                     got_data |= SEARCH_DELETE_LOG;
                     cache_data = YES;

                     break;
                  }
               }
            }

            if (cache_data == NO)
            {
# endif
# ifdef _PRODUCTION_LOG
               if (((search_log_type & SEARCH_PRODUCTION_LOG) == 0) ||
                   (plog.new_filename[0] == '\0'))
               {
#  ifdef _INPUT_LOG
                  if (((search_log_type & SEARCH_INPUT_LOG) == 0) ||
                      (ilog.filename[0] == '\0'))
                  {
                     p_file_pattern = NULL;
                     prev_log_time = 0L;
                     prev_job_id = 0;
                     p_prev_unique_number = NULL;
                     p_prev_split_job_counter = NULL;
                  }
                  else
                  {
#   ifdef _PRODUCTION_LOG
                     if ((plog.original_filename[0] != '\0') &&
                         (my_strcmp(plog.original_filename, ilog.filename) != 0))
                     {
                        p_file_pattern = plog.original_filename;
                        prev_filename_length = plog.original_filename_length;
                        prev_log_time = plog.input_time;
                        prev_job_id = plog.job_id;
                        p_prev_unique_number = &plog.unique_number;
                        p_prev_split_job_counter = &plog.split_job_counter;
                     }
                     else
                     {
#   endif
                        p_file_pattern = ilog.filename;
                        prev_filename_length = ilog.filename_length;
                        prev_log_time = ilog.input_time;
                        prev_job_id = 0;
                        p_prev_unique_number = NULL;
                        p_prev_split_job_counter = NULL;
#   ifdef _PRODUCTION_LOG
                     }
#   endif
                  }
#  else
                  p_file_pattern = NULL;
                  prev_log_time = 0L;
                  prev_job_id = 0;
                  p_prev_unique_number = NULL;
                  p_prev_split_job_counter = NULL;
#  endif
               }
               else
               {
                  p_file_pattern = plog.new_filename;
                  prev_filename_length = plog.new_filename_length;
                  prev_log_time = plog.input_time;
                  prev_job_id = plog.job_id;
                  p_prev_unique_number = &plog.unique_number;
                  p_prev_split_job_counter = &plog.split_job_counter;
               }
# else
#  ifdef _INPUT_LOG
               if (((search_log_type & SEARCH_INPUT_LOG) == 0) ||
                   (ilog.filename[0] == '\0'))
               {
                  p_file_pattern = NULL;
                  prev_log_time = 0L;
                  prev_job_id = 0;
                  p_prev_unique_number = NULL;
                  p_prev_split_job_counter = NULL;
               }
               else
               {
                  p_file_pattern = ilog.filename;
                  prev_filename_length = ilog.filename_length;
                  prev_log_time = ilog.input_time;
                  prev_job_id = 0;
                  p_prev_unique_number = NULL;
                  p_prev_split_job_counter = NULL;
               }
#  else
               p_file_pattern = NULL;
               prev_log_time = 0L;
               prev_job_id = 0;
               p_prev_unique_number = NULL;
               p_prev_split_job_counter = NULL;
#  endif
# endif
               RESET_DLOG();
               if ((ret = check_delete_log(search_afd, p_file_pattern,
                                           prev_filename_length,
                                           prev_log_time,
                                           prev_job_id,
                                           p_prev_unique_number,
                                           p_prev_split_job_counter)) == GOT_DATA)
               {
                  got_data |= SEARCH_DELETE_LOG;
# if defined (_PRODUCTION_LOG) && defined (_DISTRIBUTION_LOG)
                  if ((prod_counter > 0) && (plog.ratio_1 > plog.ratio_2) &&
                      ((plog.ratio_1 - prod_counter) <= ulog.no_of_dist_jobs))
                  {
                     if ((ddata_entries % N_TO_1_CACHE_STEP_SIZE) == 0)
                     {
                        size_t cache_size;

                        cache_size = ((ddata_entries / N_TO_1_CACHE_STEP_SIZE) + 1) *
                                     N_TO_1_CACHE_STEP_SIZE * sizeof(struct alda_ddata);
                        if ((ddata = realloc(ddata, cache_size)) == NULL)
                        {
                           (void)fprintf(stderr,
                                         "Failed to realloc() memory : %s (%s %d)\n",
                                         strerror(errno), __FILE__, __LINE__);
                           exit(INCORRECT);
                        }
                     }
                     (void)memcpy(&ddata[ddata_entries], &dlog,
                                  sizeof(struct alda_ddata));
                     ddata[ddata_entries].cache_done = 1;
                     ddata[ddata_entries].cache_todo = plog.ratio_1;
                     ddata_entries++;
                  }
#endif
               }
               else if (ret == NO_LOG_DATA)
                    {
                       RESET_DLOG();
                       more_log_data &= ~SEARCH_DELETE_LOG;
                    }
                    else
                    {
                       search_log_type &= ~SEARCH_DELETE_LOG;
                    }

               search_loop &= ~SEARCH_DELETE_LOG;
# if defined (_PRODUCTION_LOG) && defined (_DISTRIBUTION_LOG)
            }
# endif
         }
#endif /* _DELETE_LOG */
         if (got_data)
         {
            print_alda_data();
            got_data = 0;
         }
#ifdef _DELETE_LOG
         if (dlog.filename[0] != '\0')
         {
            RESET_DLOG();
         }
#endif
         if (max_search_time)
         {
            if ((time(NULL) - start_search_time) > max_search_time)
            {
               (void)fprintf(stdout, "Maximum search time reached.");
               break;
            }
         }
#ifdef _PRODUCTION_LOG
         if (prod_counter > 0)
         {
            prod_counter--;
            if (trace_mode == ON)
            {
               if (prod_counter > 0)
               {
                  if (save_search_loop == -1)
                  {
                     save_search_loop = search_loop;
                  }
                  if (plog.ratio_1 > 1)
                  {
# ifdef _DISTRIBUTION_LOG
                     if (dis_counter != -1)
                     {
                        dis_counter++;
                        if (dis_counter == ulog.no_of_dist_jobs)
                        {
                           dis_counter = -1;
                           search_loop = 0;
                        }
                        else
                        {
                           if (ulog.proc_cycles[dis_counter] > 0)
                           {
                              search_loop |= SEARCH_PRODUCTION_LOG;
                           }
                           else
                           {
                              search_loop &= ~SEARCH_PRODUCTION_LOG;
                           }
                           if (search_log_type & SEARCH_OUTPUT_LOG)
                           {
                              search_loop |= SEARCH_OUTPUT_LOG;
                           }

                           /* We must reset PRODUCTION_LOG data! */
                           RESET_PLOG();
                        }
                     }
                     else
                     {
# endif
                        search_loop = 0;
# ifdef _DISTRIBUTION_LOG
                     }
# endif
                  }
                  else
                  {
                     search_loop = SEARCH_PRODUCTION_LOG | SEARCH_OUTPUT_LOG;
                  }
               }
               else
               {
                  search_loop = 0;
                  save_search_loop = -1;
               }
            }
         }
#endif
#ifdef _DISTRIBUTION_LOG
# ifdef _PRODUCTION_LOG
         else
         {
# endif
            if (dis_counter != -1)
            {
               dis_counter++;
               if (dis_counter == ulog.no_of_dist_jobs)
               {
                  dis_counter = -1;
                  search_loop = 0;
               }
               else
               {
                  if (trace_mode == ON)
                  {
                     if (ulog.proc_cycles[dis_counter] > 0)
                     {
# ifdef _PRODUCTION_LOG
                        search_loop |= SEARCH_PRODUCTION_LOG;
# endif
                     }
                     else
                     {
# ifdef _PRODUCTION_LOG
                        search_loop &= ~SEARCH_PRODUCTION_LOG;
# endif
                        if (search_log_type & SEARCH_OUTPUT_LOG)
                        {
                           search_loop |= SEARCH_OUTPUT_LOG;
                        }
                     }

# ifdef _PRODUCTION_LOG
                     /* We must reset PRODUCTION_LOG data! */
                     RESET_PLOG();
# endif
                  }
               }
            }
            if (dis_counter == -1)
            {
               if (dis_type_counter > 1)
               {
# ifdef _PRODUCTION_LOG
                  search_loop |= SEARCH_DISTRIBUTION_LOG | SEARCH_PRODUCTION_LOG;
# else
                  search_loop |= SEARCH_DISTRIBUTION_LOG;
# endif
                  dis_type_counter--;
               }
               else
               {
                  dis_type_counter = -1;
               }
            }
# ifdef _PRODUCTION_LOG
         }
# endif
#endif
      } while (more_log_data);
   }
   else
   {
#ifdef _PRODUCTION_LOG
      prev_proc_cycles = -1;
#endif
      do
      {
#ifdef _OUTPUT_LOG
         if (search_log_type & SEARCH_OUTPUT_LOG)
         {
            RESET_OLOG();
            if ((ret = check_output_log(search_afd, NULL, -1, 0, 0, 0,
                                        NULL)) == GOT_DATA)
            {
               got_data |= SEARCH_OUTPUT_LOG;
            }
            else if (ret == NO_LOG_DATA)
                 {
                    if (output.current_file_no == 0)
                    {
                       break;
                    }
                    else
                    {
                       more_log_data &= ~SEARCH_OUTPUT_LOG;
                    }
                 }
                 else
                 {
                    search_log_type &= ~SEARCH_OUTPUT_LOG;
                 }
         }
#endif
#ifdef _PRODUCTION_LOG
         if (search_log_type & SEARCH_PRODUCTION_LOG)
         {
# ifdef _OUTPUT_LOG
            if (((search_log_type & SEARCH_OUTPUT_LOG) == 0) ||
                (olog.local_filename[0] == '\0'))
            {
               p_file_pattern = NULL;
               prev_log_time = 0L;
               prev_job_id = 0;
               p_prev_unique_number = NULL;
               p_prev_split_job_counter = NULL;
            }
            else
            {
               p_file_pattern = olog.local_filename;
               prev_filename_length = olog.local_filename_length;
               prev_log_time = olog.send_start_time;
               prev_job_id = olog.job_id;
               p_prev_unique_number = &olog.unique_number;
               p_prev_split_job_counter = &olog.split_job_counter;
            }
# else
            p_file_pattern = NULL;
# endif
            RESET_PLOG();
            if ((ret = check_production_log(search_afd, p_file_pattern,
                                            prev_filename_length,
                                            prev_log_time, 0, prev_job_id,
                                            prev_proc_cycles,
                                            p_prev_unique_number,
                                            p_prev_split_job_counter)) == GOT_DATA)
            {
               got_data |= SEARCH_PRODUCTION_LOG;
            }
            else if (ret == NO_LOG_DATA)
                 {
                    more_log_data &= ~SEARCH_PRODUCTION_LOG;
                 }
                 else
                 {
                    search_log_type &= ~SEARCH_PRODUCTION_LOG;
                 }
         }
#endif
#ifdef _INPUT_LOG
         if (search_log_type & SEARCH_INPUT_LOG)
         {
# ifdef _PRODUCTION_LOG
            if (((search_log_type & SEARCH_PRODUCTION_LOG) == 0) ||
                (plog.new_filename[0] == '\0'))
            {
#  ifdef _OUTPUT_LOG
               if (((search_log_type & SEARCH_OUTPUT_LOG) == 0) ||
                   (olog.local_filename[0] == '\0'))
               {
                  p_file_pattern = NULL;
                  prev_log_time = 0L;
                  prev_dir_id = 0;
               }
               else
               {
                  p_file_pattern = olog.local_filename;
                  prev_filename_length = olog.local_filename_length;
                  prev_log_time = olog.job_creation_time;
                  prev_dir_id = 0;
               }
#  else
               p_file_pattern = NULL;
               prev_log_time = 0L;
               prev_dir_id = 0;
#  endif
            }
            else
            {
               p_file_pattern = plog.new_filename;
               prev_filename_length = plog.new_filename_length;
               prev_log_time = plog.input_time;
               prev_dir_id = plog.dir_id;
            }
# else
#  ifdef _OUTPUT_LOG
            if (((search_log_type & SEARCH_OUTPUT_LOG) == 0) ||
                (olog.local_filename[0] == '\0'))
            {
               p_file_pattern = NULL;
               prev_log_time = 0L;
               prev_dir_id = 0;
            }
            else
            {
               p_file_pattern = olog.local_filename;
               prev_filename_length = olog.local_filename_length;
               prev_log_time = olog.job_creation_time;
               prev_dir_id = 0;
            }
#  else
            p_file_pattern = NULL;
            prev_log_time = 0L;
            prev_dir_id = 0;
#  endif
# endif
            RESET_ILOG();
            if ((ret = check_input_log(search_afd, p_file_pattern,
                                       prev_filename_length, prev_log_time,
                                       prev_dir_id)) == GOT_DATA)
            {
               got_data |= SEARCH_INPUT_LOG;
            }
            else if (ret == NO_LOG_DATA)
                 {
                    more_log_data &= ~SEARCH_INPUT_LOG;
                 }
                 else
                 {
                    search_log_type &= ~SEARCH_INPUT_LOG;
                 }
         }
#endif
#ifdef _DELETE_LOG
         if ((search_log_type & SEARCH_DELETE_LOG)
# ifdef _OUTPUT_LOG
             && (((search_log_type & SEARCH_OUTPUT_LOG) == 0)
#  ifdef _INPUT_LOG
                 || (search_log_type & SEARCH_INPUT_LOG)
#  else
#   ifdef _PRODUCTION_LOG
                 || (search_log_type & SEARCH_PRODUCTION_LOG)
#   endif
#  endif
                 || (olog.local_filename[0] == '\0'))
# endif
# ifdef _PRODUCTION_LOG
             && (((search_log_type & SEARCH_PRODUCTION_LOG) == 0)
#  ifdef _INPUT_LOG
                 || (search_log_type & SEARCH_INPUT_LOG)
#  endif
                 || (plog.new_filename[0] == '\0'))
# endif
# ifdef _INPUT_LOG
             && (((search_log_type & SEARCH_INPUT_LOG) == 0)
                 || (ilog.filename[0] == '\0'))
# endif
             )
         {
# ifdef _PRODUCTION_LOG
            if (((search_log_type & SEARCH_PRODUCTION_LOG) == 0) ||
                (plog.new_filename[0] == '\0'))
            {
#  ifdef _OUTPUT_LOG
               if (((search_log_type & SEARCH_OUTPUT_LOG) == 0) ||
                   (olog.local_filename[0] == '\0'))
               {
                  p_file_pattern = NULL;
                  prev_log_time = 0L;
                  prev_job_id = 0;
                  p_prev_unique_number = NULL;
                  p_prev_split_job_counter = NULL;
               }
               else
               {
                  p_file_pattern = olog.local_filename;
                  prev_filename_length = olog.local_filename_length;
                  prev_log_time = olog.job_creation_time;
                  prev_job_id = olog.job_id;
                  p_prev_unique_number = &olog.unique_number;
                  p_prev_split_job_counter = &olog.split_job_counter;
               }
#  else
               p_file_pattern = NULL;
               prev_log_time = 0L;
               prev_job_id = 0;
               p_prev_unique_number = NULL;
               p_prev_split_job_counter = NULL;
#  endif
            }
            else
            {
               p_file_pattern = plog.new_filename;
               prev_filename_length = plog.new_filename_length;
               prev_log_time = plog.input_time;
               prev_job_id = plog.job_id;
               p_prev_unique_number = &plog.unique_number;
               p_prev_split_job_counter = &plog.split_job_counter;
            }
# else
#  ifdef _OUTPUT_LOG
            if (((search_log_type & SEARCH_OUTPUT_LOG) == 0) ||
                (olog.local_filename[0] == '\0'))
            {
               p_file_pattern = NULL;
               prev_log_time = 0L;
               prev_job_id = 0;
               p_prev_unique_number = NULL;
               p_prev_split_job_counter = NULL;
            }
            else
            {
               p_file_pattern = olog.local_filename;
               prev_filename_length = olog.local_filename_length;
               prev_log_time = olog.job_creation_time;
               prev_job_id = olog.job_id;
               p_prev_unique_number = &olog.unique_number;
               p_prev_split_job_counter = &olog.split_job_counter;
            }
#  else
            p_file_pattern = NULL;
            prev_log_time = 0L;
            prev_job_id = 0;
            p_prev_unique_number = NULL;
            p_prev_split_job_counter = NULL;
#  endif
# endif
            RESET_DLOG();
            if ((ret = check_delete_log(search_afd, p_file_pattern,
                                        prev_filename_length, prev_log_time,
                                        prev_job_id, p_prev_unique_number,
                                        p_prev_split_job_counter)) == GOT_DATA)
            {
               got_data |= SEARCH_DELETE_LOG;
            }
            else if (ret == NO_LOG_DATA)
                 {
                    more_log_data &= ~SEARCH_DELETE_LOG;
                 }
                 else
                 {
                    search_log_type &= ~SEARCH_DELETE_LOG;
                 }
         }
#endif
         if (got_data)
         {
            print_alda_data();
            got_data = 0;
         }
         if (max_search_time)
         {
            if ((time(NULL) - start_search_time) > max_search_time)
            {
               (void)fprintf(stdout, "Maximum search time reached.");
               break;
            }
         }
      } while (more_log_data);
   }
   if ((log_data_written > 0) && (footer_filename[0] != '\0'))
   {
      show_file_content(output_fp, footer_filename);
   }

   return;
}


#ifdef WITH_AFD_MON
/*##################### get_current_afd_mon_list() ######################*/
static void
get_current_afd_mon_list(void)
{
   int ret;

   if ((ret = msa_attach_passive()) < 0)
   {
      if (ret == INCORRECT_VERSION)
      {
         (void)fprintf(stderr,
                       "ERROR   : This program is not able to attach to the MSA due to incorrect version. (%s %d)\n",
                       __FILE__, __LINE__);
      }
      else
      {
         (void)fprintf(stderr, "ERROR   : Failed to attach to MSA. (%s %d)\n",
                       __FILE__, __LINE__);
      }
      exit(INCORRECT);
   }

   return;
}
#endif


#ifdef WITH_AFD_MON
/*######################### check_start_afds() ##########################*/
static void
check_start_afds(void)
{
   int i, j;

   start_search_counter = 0;
   if (search_afd_start_alias != NULL)
   {
      FREE_RT_ARRAY(search_afd_start_alias);
      search_afd_start_alias = NULL;
   }
   for (i = 0; i < start_alias_counter; i++)
   {
      for (j = 0; j < no_of_afds; j++)
      {
         if (check_log_availability(j) == YES)
         {
            if (pmatch(start_alias[i], msa[j].afd_alias, NULL) == 0)
            {
               add_afd_to_list(j);
            }
         }
      }
   }
   for (i = 0; i < start_id_counter; i++)
   {
      for (j = 0; j < no_of_afds; j++)
      {
# ifdef NEW_MSA
         if (start_id[i] == msa[j].afd_id)
# else
         if (start_id[i] == get_str_checksum(msa[j].afd_alias))
# endif
         {
            if (check_log_availability(j) == YES)
            {
               add_afd_to_list(j);
            }
         }
      }
   }
   for (i = 0; i < start_name_counter; i++)
   {
      for (j = 0; j < no_of_afds; j++)
      {
         if (check_log_availability(j) == YES)
         {
            if ((pmatch(start_name[i], msa[j].hostname[0], NULL) == 0) ||
                ((msa[j].hostname[1][0] != '\0') &&
                 (pmatch(start_name[i], msa[j].hostname[i], NULL) == 0)))
            {
               add_afd_to_list(j);
            }
         }
      }
   }
   if (start_search_counter == 0)
   {
      for (i = 0; i < no_of_afds; i++)
      {
         if (check_log_availability(i) == YES)
         {
            add_afd_to_list(i);
         }
      }
   }

   return;
}


/*+++++++++++++++++++++++ check_log_availability() ++++++++++++++++++++++*/
static int
check_log_availability(int pos)
{
   if (((search_log_type & SEARCH_INPUT_LOG) &&
        (msa[pos].options & AFDD_INPUT_LOG) &&
        (msa[pos].log_capabilities & AFDD_INPUT_LOG)) ||
       ((search_log_type & SEARCH_DISTRIBUTION_LOG) &&
        (msa[pos].options & AFDD_DISTRIBUTION_LOG) &&
        (msa[pos].log_capabilities & AFDD_DISTRIBUTION_LOG)) ||
       ((search_log_type & SEARCH_PRODUCTION_LOG) &&
        (msa[pos].options & AFDD_PRODUCTION_LOG) &&
        (msa[pos].log_capabilities & AFDD_PRODUCTION_LOG)) ||
       ((search_log_type & SEARCH_OUTPUT_LOG) &&
        (msa[pos].options & AFDD_OUTPUT_LOG) &&
        (msa[pos].log_capabilities & AFDD_OUTPUT_LOG)) ||
       ((search_log_type & SEARCH_DELETE_LOG) &&
        (msa[pos].options & AFDD_DELETE_LOG) &&
        (msa[pos].log_capabilities & AFDD_DELETE_LOG)))
   {
      return(YES);
   }
   return(NO);
}


#define ALLOC_STEP_SIZE 10

/*++++++++++++++++++++++++++ add_afd_to_list() ++++++++++++++++++++++++++*/
static void
add_afd_to_list(int pos)
{
   if (start_search_counter == 0)
   {
      RT_ARRAY(search_afd_start_alias, ALLOC_STEP_SIZE, MAX_AFDNAME_LENGTH + 1,
               char);
      if ((search_afd_msa_pos = malloc((ALLOC_STEP_SIZE * sizeof(int)))) == NULL)
      {
         (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   else if ((start_search_counter != 0) &&
            ((start_search_counter % ALLOC_STEP_SIZE) == 0))
        {
           int new_size;

           new_size = ((start_search_counter / ALLOC_STEP_SIZE) + 1) * ALLOC_STEP_SIZE;
           REALLOC_RT_ARRAY(search_afd_start_alias, new_size,
                            MAX_AFDNAME_LENGTH + 1, char);

           new_size = new_size * sizeof(int);
           if ((search_afd_msa_pos = realloc(search_afd_msa_pos, new_size)) == NULL)
           {
              (void)fprintf(stderr, "realloc() error : %s (%s %d)\n",
                            strerror(errno), __FILE__, __LINE__);
              exit(INCORRECT);
           }
        }
   (void)strcpy(search_afd_start_alias[start_search_counter],
                msa[pos].afd_alias);
   search_afd_msa_pos[start_search_counter] = pos;
   start_search_counter++;

   return;
}


/*########################## check_end_afds() ###########################*/
static void
check_end_afds(void)
{
   return;
}
#endif


#ifdef _INPUT_LOG
/*++++++++++++++++++++++++++ check_input_log() ++++++++++++++++++++++++++*/
static int
check_input_log(char         *search_afd,
                char         *prev_file_name,
                off_t        prev_filename_length,
                time_t       prev_log_time,
                unsigned int prev_dir_id)
{
   int          ret;
   unsigned int lines_read = 0;
#ifdef HAVE_GETLINE
   ssize_t      n;
#endif

   if (input.fp == NULL)
   {
      init_file_data(start_time_start, end_time_end, SEARCH_INPUT_LOG,
                     search_afd);
      if (input.no_of_log_files == 0)
      {
         if (verbose == 3)
         {
            (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                          "%06ld DEBUG 3: [INPUT] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#else
                          "%06lld DEBUG 3: [INPUT] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#endif
                          (pri_time_t)(time(NULL) - start), lines_read,
                          NO_LOG_DATA, __LINE__);
         }
         return(NO_LOG_DATA);
      }
   }
   do
   {
      if (input.fp == NULL)
      {
         (void)sprintf(input.p_log_number, "%d", input.current_file_no);
         if ((input.fp = fopen(input.log_dir, "r")) == NULL)
         {
            if (errno != ENOENT)
            {
               (void)fprintf(stderr, "Failed to fopen() `%s' : %s (%s %d)\n",
                             input.log_dir, strerror(errno),
                             __FILE__, __LINE__);
               return(INCORRECT);
            }
         }
         else
         {
            if ((mode & ALDA_CONTINUOUS_MODE) ||
                (mode & ALDA_CONTINUOUS_DAEMON_MODE))
            {
#ifdef HAVE_STATX
               struct statx stat_buf;
#else
               struct stat stat_buf;
#endif

               input.fd = fileno(input.fp);
#ifdef HAVE_STATX
               if (statx(input.fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                         STATX_INO, &stat_buf) == -1)
#else
               if (fstat(input.fd, &stat_buf) == -1)
#endif
               {
                  (void)fprintf(stderr, "Failed to access `%s' : %s (%s %d)\n",
                                input.log_dir, strerror(errno),
                                __FILE__, __LINE__);
               }
               else
               {
#ifdef HAVE_STATX
                  input.inode_number = stat_buf.stx_ino;
#else
                  input.inode_number = stat_buf.st_ino;
#endif
               }
               if (mode & ALDA_CONTINUOUS_DAEMON_MODE)
               {
                  if (lseek(input.fd, 0, SEEK_END) == -1)
                  {
                     (void)fprintf(stderr,
                                   "Failed to lseek() `%s' : %s (%s %d)\n",
                                   input.log_dir, strerror(errno),
                                   __FILE__, __LINE__);
                  }
               }
            }
         }
      }
      if (input.fp != NULL)
      {
#ifdef HAVE_GETLINE
         while ((n = getline(&input.line, &input.line_length, input.fp)) != -1)
#else
         while (fgets(input.line, MAX_LINE_LENGTH, input.fp) != NULL)
#endif
         {
            if (verbose > 2)
            {
               if (verbose > 3)
               {
                  (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                                "%06ld DEBUG 4: [INPUT] readline: %s",
#else
                                "%06lld DEBUG 4: [INPUT] readline: %s",
#endif
                                (pri_time_t)(time(NULL) - start), input.line);
               }
               else
               {
                  lines_read++;
               }
            }
#ifdef HAVE_GETLINE
            input.bytes_read += n;
#endif
            if (input.line[0] != '#')
            {
               if ((ret = check_input_line(input.line, prev_file_name,
                                           prev_filename_length,
                                           prev_log_time,
                                           prev_dir_id)) == SUCCESS)
               {
                  if (verbose == 3)
                  {
                     (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                                   "%06ld DEBUG 3: [INPUT] ignored %u lines, returning GOT_DATA (%d) [%d]\n",
#else
                                   "%06lld DEBUG 3: [INPUT] ignored %u lines, returning GOT_DATA (%d) [%d]\n",
#endif
                                   (pri_time_t)(time(NULL) - start), lines_read,
                                   GOT_DATA, __LINE__);
                  }
                  return(GOT_DATA);
               }
               else if (ret == SEARCH_TIME_UP)
                    {
                       if (verbose == 3)
                       {
                          (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                                        "%06ld DEBUG 3: [INPUT] ignored %u lines, returning SEARCH_TIME_UP (%d) [%d]\n",
#else
                                        "%06lld DEBUG 3: [INPUT] ignored %u lines, returning SEARCH_TIME_UP (%d) [%d]\n",
#endif
                                        (pri_time_t)(time(NULL) - start),
                                        lines_read, SEARCH_TIME_UP, __LINE__);
                       }
                       return(ret);
                    }
            }
            else
            {
               if ((input.line[1] == '!') && (input.line[2] == '#'))
               {
                  get_log_type_data(&input.line[3]);
               }
            }
         }
         clearerr(input.fp);
         if ((input.current_file_no != 0) ||
             (((mode & ALDA_CONTINUOUS_MODE) == 0) &&
              ((mode & ALDA_CONTINUOUS_DAEMON_MODE) == 0)))
         {
            if (fclose(input.fp) == EOF)
            {
               (void)fprintf(stderr, "Failed to fclose() `%s' : %s (%s %d)\n",
                             input.log_dir, strerror(errno),
                             __FILE__, __LINE__);
            }
            input.fp = NULL;
            input.bytes_read = 0;
         }
      }
      input.current_file_no--;
   } while (input.current_file_no >= input.end_file_no);

   if (input.current_file_no < input.end_file_no)
   {
      input.current_file_no = input.end_file_no;
   }

   if ((input.current_file_no != 0) ||
       (((mode & ALDA_CONTINUOUS_MODE) == 0) &&
        ((mode & ALDA_CONTINUOUS_DAEMON_MODE) == 0)))
   {
      if (input.fp != NULL)
      {
         if (fclose(input.fp) == EOF)
         {
            (void)fprintf(stderr, "Failed to fclose() `%s' : %s (%s %d)\n",
                          input.log_dir, strerror(errno), __FILE__, __LINE__);
         }
         input.fp = NULL;
         input.bytes_read = 0;
      }
   }
   if (verbose == 3)
   {
      (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                    "%06ld DEBUG 3: [INPUT] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#else
                    "%06lld DEBUG 3: [INPUT] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#endif
                    (pri_time_t)(time(NULL) - start), lines_read, NO_LOG_DATA,
                    __LINE__);
   }

   return(NO_LOG_DATA);
}
#endif


#ifdef _DISTRIBUTION_LOG
/*++++++++++++++++++++++ check_distribution_log() +++++++++++++++++++++++*/
static int
check_distribution_log(char         *search_afd,
                       char         *prev_file_name,
                       off_t        prev_filename_length,
                       time_t       prev_log_time,
                       unsigned int prev_dir_id,
                       unsigned int *prev_unique_number)
{
#ifdef HAVE_GETLINE
   ssize_t      n;
#endif
   unsigned int lines_read = 0;
   int          end_loop = NO,
                new_log_file = NO,
                ret;

   if (distribution.fp == NULL)
   {
#ifdef BLUBB
      int p_max_log_files = distribution.max_log_files;
#endif

      init_file_data((start_time_start == 0) ? init_time_start : start_time_start,
                     end_time_end, SEARCH_DISTRIBUTION_LOG, search_afd);
      if (distribution.no_of_log_files == 0)
      {
         if (verbose == 3)
         {
            (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                          "%06ld DEBUG 3: [DISTRIBUTION] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#else
                          "%06lld DEBUG 3: [DISTRIBUTION] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#endif
                          (pri_time_t)(time(NULL) - start), lines_read,
                          NO_LOG_DATA, __LINE__);
         }
         return(NO_LOG_DATA);
      }
#ifdef BLUBB
      if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE))
      {
         if (ucache != NULL)
         {
            free(ucache);
            ucache = NULL;
         }
         if (upl != NULL)
         {
            int i;

            for (i = 0; i < p_max_log_files; i++)
            {
               free(upl[i]);
            }
            free(upl);
            upl = NULL;
         }
      }
#endif
   }
   do
   {
      if (distribution.fp == NULL)
      {
         (void)sprintf(distribution.p_log_number, "%d",
                       distribution.current_file_no);
         if ((distribution.fp = fopen(distribution.log_dir, "r")) == NULL)
         {
            if (errno != ENOENT)
            {
               (void)fprintf(stderr, "Failed to fopen() `%s' : %s (%s %d)\n",
                             distribution.log_dir, strerror(errno),
                             __FILE__, __LINE__);
               return(INCORRECT);
            }
         }
         else
         {
#ifdef HAVE_STATX
            struct statx stat_buf;
#else
            struct stat stat_buf;
#endif

            distribution.bytes_read = 0;
            distribution.fd = fileno(distribution.fp);
#ifdef HAVE_STATX
            if (statx(distribution.fd, "",
                      AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                      STATX_INO | STATX_MTIME, &stat_buf) == -1)
#else
            if (fstat(distribution.fd, &stat_buf) == -1)
#endif
            {
               (void)fprintf(stderr, "Failed to access `%s' : %s (%s %d)\n",
                             distribution.log_dir, strerror(errno),
                             __FILE__, __LINE__);
            }
            else
            {
#ifdef HAVE_STATX
               distribution.inode_number = stat_buf.stx_ino;
#else
               distribution.inode_number = stat_buf.st_ino;
#endif
            }
            if (mode & ALDA_CONTINUOUS_DAEMON_MODE)
            {
               if (lseek(distribution.fd, 0, SEEK_END) == -1)
               {
                  (void)fprintf(stderr,
                                "Failed to lseek() `%s' : %s (%s %d)\n",
                                distribution.log_dir, strerror(errno),
                                __FILE__, __LINE__);
               }
            }
            if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE))
            {
               if (ucache == NULL)
               {
                  if ((ucache = malloc((distribution.max_log_files * sizeof(struct alda_cache_data)))) == NULL)
                  {
                        (void)fprintf(stderr,
                                      "Failed to malloc() memory : %s (%s %d)\n",
                                      strerror(errno), __FILE__, __LINE__);
                        exit(INCORRECT);
                  }
                  (void)memset(ucache, 0,
                               (distribution.max_log_files * sizeof(struct alda_cache_data)));
               }
               if (upl == NULL)
               {
                  int i;

                  if ((upl = malloc((distribution.max_log_files * sizeof(struct alda_position_list *)))) == NULL)
                  {
                     (void)fprintf(stderr,
                                   "Failed to malloc() memory : %s (%s %d)\n",
                                   strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
                  for (i = 0; i < distribution.max_log_files; i++)
                  {
                     upl[i] = NULL;
                  }
               }
               if (ucache[distribution.current_file_no].inode == 0)
               {
                  ucache[distribution.current_file_no].inode = distribution.inode_number;
               }
               else
               {
                  if (distribution.inode_number != ucache[distribution.current_file_no].inode)
                  {
                     reshuffel_cache_data(__LINE__);
                  }
               }
#ifdef HAVE_STATX
               ucache[distribution.current_file_no].last_entry = stat_buf.stx_mtime.tv_sec;
#else
               ucache[distribution.current_file_no].last_entry = stat_buf.st_mtime;
#endif
            }
         }
      }
      if (distribution.fp != NULL)
      {
          if ((prev_log_time == 0L) ||
              (ucache[distribution.current_file_no].last_entry == 0L) ||
              (ucache[distribution.current_file_no].last_entry >= prev_log_time))
         {
            if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE) &&
                (prev_log_time > 0) && (new_log_file == NO))
            {
               int gotcha = NO,
                   i = -2,
                   j,
                   tmp_current_file_no;

               tmp_current_file_no = j = distribution.current_file_no;
               if (j == 0)
               {
                  end_loop = YES;
               }
               do
               {
                  if ((upl[j] != NULL) && (ucache[j].pc > 0) &&
                      (upl[j][0].time <= prev_log_time) &&
                      (upl[j][ucache[j].pc - 1].time >= prev_log_time))
                  {
                     for (i = (ucache[j].pc - 2); i > -1; i--)
                     {
                        if (upl[j][i].time < prev_log_time)
                        {
                           i++;
                           while ((i < ucache[j].pc) &&
                                  (upl[j][i].gotcha == YES))
                           {
                              i++;
                           }
                           if (i < ucache[j].pc)
                           {
                              ucache[j].pc = i;
                           }
                           else
                           {
                              if (ucache[j].pc > 0)
                              {
                                 ucache[j].pc--;
                              }
                           }
                           distribution.current_file_no = j;
                           gotcha = YES;
                           j = distribution.end_file_no;
                           break;
                        }
                     }
                  }
                  if (gotcha == YES)
                  {
                     break;
                  }
                  j++;
               } while (j <= distribution.start_file_no);

               /*
                * Close current file when we search in a another log file!
                */
               if (tmp_current_file_no != distribution.current_file_no)
               {
                  if (fclose(distribution.fp) == EOF)
                  {
                     (void)fprintf(stderr,
                                   "Failed to fclose() `%s' : %s (%s %d)\n",
                                   distribution.log_dir, strerror(errno),
                                   __FILE__, __LINE__);
                  }
                  distribution.bytes_read = 0;
                  distribution.fp = NULL;

                  (void)sprintf(distribution.p_log_number, "%d",
                                distribution.current_file_no);
                  if ((distribution.fp = fopen(distribution.log_dir, "r")) == NULL)
                  {
                     if (errno != ENOENT)
                     {
                        (void)fprintf(stderr,
                                      "Failed to fopen() `%s' : %s (%s %d)\n",
                                      distribution.log_dir, strerror(errno),
                                      __FILE__, __LINE__);
                        return(INCORRECT);
                     }
                  }
                  else
                  {
#ifdef HAVE_STATX
                     struct statx stat_buf;
#else
                     struct stat stat_buf;
#endif

                     distribution.fd = fileno(distribution.fp);
#ifdef HAVE_STATX
                     if (statx(distribution.fd, "",
                               AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                               STATX_INO | STATX_MTIME, &stat_buf) == -1)
#else
                     if (fstat(distribution.fd, &stat_buf) == -1)
#endif
                     {
                        (void)fprintf(stderr,
                                      "Failed to access `%s' : %s (%s %d)\n",
                                      distribution.log_dir, strerror(errno),
                                      __FILE__, __LINE__);
                     }
                     else
                     {
#ifdef HAVE_STATX
                        distribution.inode_number = stat_buf.stx_ino;
#else
                        distribution.inode_number = stat_buf.st_ino;
#endif
                        if (ucache[distribution.current_file_no].inode == 0)
                        {
                           ucache[distribution.current_file_no].inode = distribution.inode_number;
                        }
                        else
                        {
                           if (distribution.inode_number != ucache[distribution.current_file_no].inode)
                           {
                              reshuffel_cache_data(__LINE__);
                           }
                        }
#ifdef HAVE_STATX
                        ucache[distribution.current_file_no].last_entry = stat_buf.stx_mtime.tv_sec;
#else
                        ucache[distribution.current_file_no].last_entry = stat_buf.st_mtime;
#endif
                     }
                  }
               }
               if ((upl[distribution.current_file_no] != NULL) &&
                   ((gotcha == YES) || (i == -1) ||
                    ((upl[distribution.current_file_no][ucache[distribution.current_file_no].pc - 1].time >= prev_log_time) &&
                     (prev_log_time > 0))))
               {
                  int current_line;

                  current_line = ucache[distribution.current_file_no].pc;
                  if (i == -1)
                  {
                     i = 0;
                     while ((i < ucache[distribution.current_file_no].pc) &&
                            (upl[distribution.current_file_no][i].gotcha == YES))
                     {
                        i++;
                     }
                     if (i == 0)
                     {
                        ucache[distribution.current_file_no].pc = i;
                     }
                     else
                     {
                        ucache[distribution.current_file_no].pc = i - 1;
                     }
                  }
                  if ((i == -2) && (ucache[distribution.current_file_no].pc > 0))
                  {
                     ucache[distribution.current_file_no].pc--;
                  }
                  while ((ucache[distribution.current_file_no].pc > 0) &&
                         (upl[distribution.current_file_no][ucache[distribution.current_file_no].pc].time >= prev_log_time))
                  {
                     ucache[distribution.current_file_no].pc--;
                  }
                  if (fseeko(distribution.fp,
                             upl[distribution.current_file_no][ucache[distribution.current_file_no].pc].pos,
                             SEEK_SET) == -1)
                  {
                     (void)fprintf(stderr,
#if SIZEOF_OFF_T == 4
                                   "Failed to fseeko() to %ld : %s (%s %d)\n",
#else
                                   "Failed to fseeko() to %lld : %s (%s %d)\n",
#endif
                                   (pri_off_t)upl[distribution.current_file_no][ucache[distribution.current_file_no].pc].pos,
                                   strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
                  else
                  {
                     distribution.bytes_read = upl[distribution.current_file_no][ucache[distribution.current_file_no].pc].pos;
                     if (verbose > 0)
                     {
                        if (ucache[distribution.current_file_no].pc == 0)
                        {
                           j = 1;
                        }
                        else
                        {
                           j = 0;
                        }
                        (void)fprintf(stdout,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                      "%06ld DEBUG 1: [DISTRIBUTION] seeking back %d - %d = %d lines %ld - %ld = %ld chars in %s\n",
# else
                                      "%06lld DEBUG 1: [DISTRIBUTION] seeking back %d - %d = %d lines %ld - %ld = %ld chars in %s\n",
# endif
#else
# if SIZEOF_TIME_T == 4
                                      "%06ld DEBUG 1: [DISTRIBUTION] seeking back %d - %d = %d lines %lld - %lld = %lld chars in %s\n",
# else
                                      "%06lld DEBUG 1: [DISTRIBUTION] seeking back %d - %d = %d lines %lld - %lld = %lld chars in %s\n",
# endif
#endif
                                      (pri_time_t)(time(NULL) - start),
                                      current_line,
                                      ucache[distribution.current_file_no].pc,
                                      current_line - ucache[distribution.current_file_no].pc,
                                      (pri_off_t)upl[distribution.current_file_no][current_line - 1].pos,
                                      (pri_off_t)upl[distribution.current_file_no][ucache[distribution.current_file_no].pc - 1 + j].pos,
                                      (pri_off_t)(upl[distribution.current_file_no][current_line - 1].pos - upl[distribution.current_file_no][ucache[distribution.current_file_no].pc - 1 + j].pos),
                                      distribution.log_dir);
                     }
                  }
               }
            }
            else
            {
               new_log_file = NO;
            }
#ifdef HAVE_GETLINE
            while ((n = getline(&distribution.line, &distribution.line_length,
                                distribution.fp)) != -1)
#else
            while (fgets(distribution.line, MAX_LINE_LENGTH,
                         distribution.fp) != NULL)
#endif
            {
               if (verbose > 2)
               {
                  if (verbose > 3)
                  {
                     (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                                   "%06ld DEBUG 4: [DISTRIBUTION] readline: %s",
#else
                                   "%06lld DEBUG 4: [DISTRIBUTION] readline: %s",
#endif
                                   (pri_time_t)(time(NULL) - start),
                                   distribution.line);
                  }
                  else
                  {
                     lines_read++;
                  }
               }
               if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE))
               {
                  if (upl[distribution.current_file_no] == NULL)
                  {
                     if ((upl[distribution.current_file_no] = malloc(LOG_LIST_STEP_SIZE * sizeof(struct alda_position_list))) == NULL)
                     {
                        (void)fprintf(stderr,
                                      "Failed to malloc() memory : %s (%s %d)\n",
                                      strerror(errno), __FILE__, __LINE__);
                        exit(INCORRECT);
                     }
                     ucache[distribution.current_file_no].pc = 0;
                     ucache[distribution.current_file_no].mpc = 0;
                     upl[distribution.current_file_no][0].pos = 0;
                     upl[distribution.current_file_no][0].gotcha = NO;
                  }
                  else
                  {
                     if (ucache[distribution.current_file_no].mpc == ucache[distribution.current_file_no].pc)
                     {
                        if ((ucache[distribution.current_file_no].pc % LOG_LIST_STEP_SIZE) == 0)
                        {
                           size_t new_size;

                           new_size = ((ucache[distribution.current_file_no].pc / LOG_LIST_STEP_SIZE) + 1) *
                                      LOG_LIST_STEP_SIZE * sizeof(struct alda_position_list);
                           if ((upl[distribution.current_file_no] = realloc(upl[distribution.current_file_no],
                                                                            new_size)) == NULL)
                           {
                              (void)fprintf(stderr,
                                            "Failed to malloc() memory : %s (%s %d)\n",
                                            strerror(errno), __FILE__, __LINE__);
                              exit(INCORRECT);
                           }
                        }
                     }
                     upl[distribution.current_file_no][ucache[distribution.current_file_no].pc].pos = distribution.bytes_read;
                  }
               }
#ifdef HAVE_GETLINE
               distribution.bytes_read += n;
#endif
               if (distribution.line[0] != '#')
               {
                  if ((ret = check_distribution_line(distribution.line,
                                                     prev_file_name,
                                                     prev_filename_length,
                                                     prev_log_time, prev_dir_id,
                                                     prev_unique_number)) == SUCCESS)
                  {
                     if ((trace_mode == ON) && (ucache != NULL))
                     {
                        upl[distribution.current_file_no][ucache[distribution.current_file_no].pc - 1].gotcha = YES;
                     }
                     if (verbose == 3)
                     {
                        (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                                      "%06ld DEBUG 3: [DISTRIBUTION] ignored %u lines, returning GOT_DATA (%d) [%d]\n",
#else
                                      "%06lld DEBUG 3: [DISTRIBUTION] ignored %u lines, returning GOT_DATA (%d) [%d]\n",
#endif
                                      (pri_time_t)(time(NULL) - start),
                                      lines_read, GOT_DATA, __LINE__);
                     }
                     return(GOT_DATA);
                  }
                  else if (ret == SEARCH_TIME_UP)
                       {
                          if (verbose == 3)
                          {
                             (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                                           "%06ld DEBUG 3: [DISTRIBUTION] ignored %u lines, returning SEARCH_TIME_UP (%d) [%d]\n",
#else
                                           "%06lld DEBUG 3: [DISTRIBUTION] ignored %u lines, returning SEARCH_TIME_UP (%d) [%d]\n",
#endif
                                           (pri_time_t)(time(NULL) - start),
                                           lines_read, SEARCH_TIME_UP, __LINE__);
                          }
                          return(ret);
                       }
                       else
                       {
                          if (trace_mode == ON)
                          {
                             if ((prev_log_time > 0) &&
                                 ((ulog.distribution_time - prev_log_time) > max_diff_time))
                             {
                                if (verbose == 3)
                                {
                                   (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                                                 "%06ld DEBUG 3: [DISTRIBUTION] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#else
                                                 "%06lld DEBUG 3: [DISTRIBUTION] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#endif
                                                 (pri_time_t)(time(NULL) - start),
                                                 lines_read, NO_LOG_DATA,
                                                 __LINE__);
                                }
                                return(NO_LOG_DATA);
                             }

                             if (ucache[distribution.current_file_no].mpc != ucache[distribution.current_file_no].pc)
                             {
                                int i = ucache[distribution.current_file_no].pc;

                                while ((i < ucache[distribution.current_file_no].mpc) &&
                                       (upl[distribution.current_file_no][i - 1].gotcha == YES))
                                {
                                   i++;
                                }
                                if (i != ucache[distribution.current_file_no].pc)
                                {
                                   int current_line;

                                   current_line = ucache[distribution.current_file_no].pc;
                                   ucache[distribution.current_file_no].pc = i - 1;
                                   if (fseeko(distribution.fp,
                                              upl[distribution.current_file_no][ucache[distribution.current_file_no].pc].pos,
                                              SEEK_SET) == -1)
                                   {
                                      (void)fprintf(stderr,
                                                    "Failed to fseeko() : %s (%s %d)\n",
                                                    strerror(errno), __FILE__, __LINE__);
                                      exit(INCORRECT);
                                   }
                                   else
                                   {
                                      distribution.bytes_read = upl[distribution.current_file_no][ucache[distribution.current_file_no].pc].pos;
                                      if (verbose > 0)
                                      {
                                         (void)fprintf(stdout,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                                       "%06ld DEBUG 1: [DISTRIBUTION] seeking forward %d - %d = %d lines %ld + %ld = %ld chars in %s\n",
# else
                                                       "%06lld DEBUG 1: [DISTRIBUTION] seeking forward %d - %d = %d lines %ld + %ld = %ld chars in %s\n",
# endif
#else
# if SIZEOF_TIME_T == 4
                                                       "%06ld DEBUG 1: [DISTRIBUTION] seeking forward %d - %d = %d lines %lld + %lld = %lld chars in %s\n",
# else
                                                       "%06lld DEBUG 1: [DISTRIBUTION] seeking forward %d - %d = %d lines %lld + %lld = %lld chars in %s\n",
# endif
#endif
                                                       (pri_time_t)(time(NULL) - start),
                                                       ucache[distribution.current_file_no].pc,
                                                       current_line,
                                                       ucache[distribution.current_file_no].pc - current_line,
                                                       (pri_off_t)upl[distribution.current_file_no][ucache[distribution.current_file_no].pc - 1].pos,
                                                       (pri_off_t)upl[distribution.current_file_no][current_line - 1].pos,
                                                       (pri_off_t)(upl[distribution.current_file_no][ucache[distribution.current_file_no].pc - 1].pos - upl[distribution.current_file_no][current_line - 1].pos),
                                                       distribution.log_dir);
                                      }
                                   }
                                }
                             }
                          }
                       }
               }
               else
               {
                  if ((distribution.line[1] == '!') && (distribution.line[2] == '#'))
                  {
                     get_log_type_data(&distribution.line[3]);
                  }
               }
            }
            clearerr(distribution.fp);
            new_log_file = YES;
         }
#ifdef WITH_DEBUG
         (void)fprintf(stderr, "ulog %d: %s\n",
                       distribution.current_file_no, prev_file_name);
#endif
         if ((distribution.current_file_no != 0) ||
             (((mode & ALDA_CONTINUOUS_MODE) == 0) &&
              ((mode & ALDA_CONTINUOUS_DAEMON_MODE) == 0)))
         {
            if (fclose(distribution.fp) == EOF)
            {
               (void)fprintf(stderr, "Failed to fclose() `%s' : %s (%s %d)\n",
                             distribution.log_dir, strerror(errno),
                             __FILE__, __LINE__);
            }
            distribution.fp = NULL;
            distribution.bytes_read = 0;
         }
      }
      distribution.current_file_no--;
   } while ((distribution.current_file_no >= distribution.end_file_no) &&
            (end_loop == NO));

   if (distribution.current_file_no < distribution.end_file_no)
   {
      distribution.current_file_no = distribution.end_file_no;
   }

   if ((distribution.current_file_no != 0) ||
       (((mode & ALDA_CONTINUOUS_MODE) == 0) &&
        ((mode & ALDA_CONTINUOUS_DAEMON_MODE) == 0)))
   {
      if (distribution.fp != NULL)
      {
         if (fclose(distribution.fp) == EOF)
         {
            (void)fprintf(stderr, "Failed to fclose() `%s' : %s (%s %d)\n",
                          distribution.log_dir, strerror(errno), __FILE__, __LINE__);
         }
         distribution.fp = NULL;
         distribution.bytes_read = 0;
      }
   }
   if (verbose == 3)
   {
      (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                    "%06ld DEBUG 3: [DISTRIBUTION] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#else
                    "%06lld DEBUG 3: [DISTRIBUTION] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#endif
                    (pri_time_t)(time(NULL) - start), lines_read, NO_LOG_DATA,
                    __LINE__);
   }

   return(NO_LOG_DATA);
}
#endif /* _DISTRIBUTION_LOG */


#ifdef _PRODUCTION_LOG
/*+++++++++++++++++++++++ check_production_log() ++++++++++++++++++++++++*/
static int
check_production_log(char         *search_afd,
                     char         *prev_file_name,
                     off_t        prev_filename_length,
                     time_t       prev_log_time,
                     unsigned int prev_dir_id,
                     unsigned int prev_job_id,
                     int          prev_proc_cycles,
                     unsigned int *prev_unique_number,
                     unsigned int *prev_split_job_counter)
{
#ifdef HAVE_GETLINE
   ssize_t      n;
#endif
   unsigned int lines_read = 0;
   int          end_loop = NO,
                new_log_file = NO;
   off_t        p_prev_filename_length;
   char         *p_prev_file_name;

   if (prev_proc_cycles == 0)
   {
      if (verbose == 3)
      {
         (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                       "%06ld DEBUG 3: [PRODUCTION] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#else
                       "%06lld DEBUG 3: [PRODUCTION] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#endif
                       (pri_time_t)(time(NULL) - start), lines_read,
                       NO_LOG_DATA, __LINE__);
      }
      return(NO_LOG_DATA);
   }

   if (production.fp == NULL)
   {
#ifdef BLUBB
      int p_max_log_files = production.max_log_files;
#endif

      init_file_data((start_time_start == 0) ? init_time_start : start_time_start,
                     end_time_end, SEARCH_PRODUCTION_LOG, search_afd);
      if (production.no_of_log_files == 0)
      {
         if (verbose == 3)
         {
            (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                          "%06ld DEBUG 3: [PRODUCTION] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#else
                          "%06lld DEBUG 3: [PRODUCTION] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#endif
                          (pri_time_t)(time(NULL) - start), lines_read,
                          NO_LOG_DATA, __LINE__);
         }
         return(NO_LOG_DATA);
      }
#ifdef BLUBB
      if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE))
      {
         if (pcache != NULL)
         {
            free(pcache);
            pcache = NULL;
         }
         if (ppl != NULL)
         {
            int i;

            for (i = 0; i < p_max_log_files; i++)
            {
               free(ppl[i]);
            }
            free(ppl);
            ppl = NULL;
         }
      }
#endif
   }
   success_plog.new_filename[0] = '\0';
   p_prev_file_name = prev_file_name;
   p_prev_filename_length = prev_filename_length;
   do
   {
      if (production.fp == NULL)
      {
         (void)sprintf(production.p_log_number, "%d",
                       production.current_file_no);
         if ((production.fp = fopen(production.log_dir, "r")) == NULL)
         {
            if (errno != ENOENT)
            {
               (void)fprintf(stderr, "Failed to fopen() `%s' : %s (%s %d)\n",
                             production.log_dir, strerror(errno),
                             __FILE__, __LINE__);
               return(INCORRECT);
            }
         }
         else
         {
#ifdef HAVE_STATX
            struct statx stat_buf;
#else
            struct stat stat_buf;
#endif

            production.bytes_read = 0;
            production.fd = fileno(production.fp);
#ifdef HAVE_STATX
            if (statx(production.fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                      STATX_INO | STATX_MTIME, &stat_buf) == -1)
#else
            if (fstat(production.fd, &stat_buf) == -1)
#endif
            {
               (void)fprintf(stderr, "Failed to access `%s' : %s (%s %d)\n",
                             production.log_dir, strerror(errno),
                             __FILE__, __LINE__);
            }
            else
            {
#ifdef HAVE_STATX
               production.inode_number = stat_buf.stx_ino;
#else
               production.inode_number = stat_buf.st_ino;
#endif
            }
            if (mode & ALDA_CONTINUOUS_DAEMON_MODE)
            {
               if (lseek(production.fd, 0, SEEK_END) == -1)
               {
                  (void)fprintf(stderr,
                                "Failed to lseek() `%s' : %s (%s %d)\n",
                                production.log_dir, strerror(errno),
                                __FILE__, __LINE__);
               }
            }
            if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE))
            {
               if (pcache == NULL)
               {
                  if ((pcache = malloc((production.max_log_files * sizeof(struct alda_cache_data)))) == NULL)
                  {
                     (void)fprintf(stderr,
                                   "Failed to malloc() memory : %s (%s %d)\n",
                                   strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
                  (void)memset(pcache, 0,
                               (production.max_log_files * sizeof(struct alda_cache_data)));
               }
               if (ppl == NULL)
               {
                  int i;

                  if ((ppl = malloc((production.max_log_files * sizeof(struct alda_position_list *)))) == NULL)
                  {
                     (void)fprintf(stderr,
                                   "Failed to malloc() memory : %s (%s %d)\n",
                                   strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
                  for (i = 0; i < production.max_log_files; i++)
                  {
                     ppl[i] = NULL;
                  }
               }
               if (pcache[production.current_file_no].inode == 0)
               {
                  pcache[production.current_file_no].inode = production.inode_number;
               }
               else
               {
                  if (production.inode_number != pcache[production.current_file_no].inode)
                  {
                     reshuffel_cache_data(__LINE__);
                  }
               }
#ifdef HAVE_STATX
               pcache[production.current_file_no].last_entry = stat_buf.stx_mtime.tv_sec;
#else
               pcache[production.current_file_no].last_entry = stat_buf.st_mtime;
#endif
            }
         }
      }
      if (production.fp != NULL)
      {
          if ((prev_log_time == 0L) ||
              (pcache[production.current_file_no].last_entry == 0L) ||
              (pcache[production.current_file_no].last_entry >= prev_log_time))
         {
            if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE) &&
                (prev_log_time > 0) && (new_log_file == NO))
            {
               int gotcha = NO,
                   i = -2,
                   j,
                   tmp_current_file_no;

               tmp_current_file_no = j = production.current_file_no;
               if (j == 0)
               {
                  end_loop = YES;
               }
               do
               {
                  if ((ppl[j] != NULL) && (pcache[j].pc > 0) &&
                      (ppl[j][0].time <= prev_log_time) &&
                      (ppl[j][pcache[j].pc - 1].time >= prev_log_time))
                  {
                     for (i = (pcache[j].pc - 2); i > -1; i--)
                     {
                        if (ppl[j][i].time < prev_log_time)
                        {
                           i++;
                           while ((i < pcache[j].pc) && (ppl[j][i].gotcha == YES))
                           {
                              i++;
                           }
                           if (i < pcache[j].pc)
                           {
                              pcache[j].pc = i;
                           }
                           else
                           {
                              if (pcache[j].pc > 0)
                              {
                                 pcache[j].pc--;
                              }
                           }
                           production.current_file_no = j;
                           gotcha = YES;
                           j = production.end_file_no;
                           break;
                        }
                     }
                  }
                  if (gotcha == YES)
                  {
                     break;
                  }
                  j++;
               } while (j <= production.start_file_no);

               /*
                * Close current file when we search in a another log file!
                */
               if (tmp_current_file_no != production.current_file_no)
               {
                  if (fclose(production.fp) == EOF)
                  {
                     (void)fprintf(stderr,
                                   "Failed to fclose() `%s' : %s (%s %d)\n",
                                   production.log_dir, strerror(errno),
                                   __FILE__, __LINE__);
                  }
                  production.bytes_read = 0;
                  production.fp = NULL;

                  (void)sprintf(production.p_log_number, "%d",
                                production.current_file_no);
                  if ((production.fp = fopen(production.log_dir, "r")) == NULL)
                  {
                     if (errno != ENOENT)
                     {
                        (void)fprintf(stderr,
                                      "Failed to fopen() `%s' : %s (%s %d)\n",
                                      production.log_dir, strerror(errno),
                                      __FILE__, __LINE__);
                        return(INCORRECT);
                     }
                  }
                  else
                  {
#ifdef HAVE_STATX
                     struct statx stat_buf;
#else
                     struct stat stat_buf;
#endif

                     production.fd = fileno(production.fp);
#ifdef HAVE_STATX
                     if (statx(production.fd, "",
                               AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                               STATX_INO | STATX_MTIME, &stat_buf) == -1)
#else
                     if (fstat(production.fd, &stat_buf) == -1)
#endif
                     {
                        (void)fprintf(stderr,
                                      "Failed to access `%s' : %s (%s %d)\n",
                                      production.log_dir, strerror(errno),
                                      __FILE__, __LINE__);
                     }
                     else
                     {
#ifdef HAVE_STATX
                        production.inode_number = stat_buf.stx_ino;
#else
                        production.inode_number = stat_buf.st_ino;
#endif
                        if (pcache[production.current_file_no].inode == 0)
                        {
                           pcache[production.current_file_no].inode = production.inode_number;
                        }
                        else
                        {
                           if (production.inode_number != pcache[production.current_file_no].inode)
                           {
                              reshuffel_cache_data(__LINE__);
                           }
                        }
#ifdef HAVE_STATX
                        pcache[production.current_file_no].last_entry = stat_buf.stx_mtime.tv_sec;
#else
                        pcache[production.current_file_no].last_entry = stat_buf.st_mtime;
#endif
                     }
                  }
               }
               if ((ppl[production.current_file_no] != NULL) &&
                   ((gotcha == YES) || (i == -1) ||
                    ((ppl[production.current_file_no][pcache[production.current_file_no].pc - 1].time >= prev_log_time) &&
                     (prev_log_time > 0))))
               {
                  int current_line;

                  current_line = pcache[production.current_file_no].pc;
                  if (i == -1)
                  {
                     i = 0;
                     while ((i < pcache[production.current_file_no].pc) &&
                            (ppl[production.current_file_no][i].gotcha == YES))
                     {
                        i++;
                     }
                     if (i == 0)
                     {
                        pcache[production.current_file_no].pc = i;
                     }
                     else
                     {
                        pcache[production.current_file_no].pc = i - 1;
                     }
                  }
                  if ((i == -2) && (pcache[production.current_file_no].pc > 0))
                  {
                     pcache[production.current_file_no].pc--;
                  }
                  while ((pcache[production.current_file_no].pc > 0) &&
                         (ppl[production.current_file_no][pcache[production.current_file_no].pc].time >= prev_log_time))
                  {
                     pcache[production.current_file_no].pc--;
                  }
                  if (fseeko(production.fp,
                             ppl[production.current_file_no][pcache[production.current_file_no].pc].pos,
                             SEEK_SET) == -1)
                  {
                     (void)fprintf(stderr,
#if SIZEOF_OFF_T == 4
                                   "Failed to fseeko() to %ld : %s (%s %d)\n",
#else
                                   "Failed to fseeko() to %lld : %s (%s %d)\n",
#endif
                                   (pri_off_t)ppl[production.current_file_no][pcache[production.current_file_no].pc].pos,
                                   strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
                  else
                  {
                     production.bytes_read = ppl[production.current_file_no][pcache[production.current_file_no].pc].pos;
                     if (verbose > 0)
                     {
                        if (pcache[production.current_file_no].pc == 0)
                        {
                           j = 1;
                        }
                        else
                        {
                           j = 0;
                        }
                        (void)fprintf(stdout,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                      "%06ld DEBUG 1: [PRODUCTION] seeking back %d - %d = %d lines %ld - %ld = %ld chars in %s\n",
# else
                                      "%06lld DEBUG 1: [PRODUCTION] seeking back %d - %d = %d lines %ld - %ld = %ld chars in %s\n",
# endif
#else
# if SIZEOF_TIME_T == 4
                                      "%06ld DEBUG 1: [PRODUCTION] seeking back %d - %d = %d lines %lld - %lld = %lld chars in %s\n",
# else
                                      "%06lld DEBUG 1: [PRODUCTION] seeking back %d - %d = %d lines %lld - %lld = %lld chars in %s\n",
# endif
#endif
                                      (pri_time_t)(time(NULL) - start),
                                      current_line,
                                      pcache[production.current_file_no].pc,
                                      current_line - pcache[production.current_file_no].pc,
                                      (pri_off_t)ppl[production.current_file_no][current_line - 1].pos,
                                      (pri_off_t)ppl[production.current_file_no][pcache[production.current_file_no].pc - 1 + j].pos,
                                      (pri_off_t)(ppl[production.current_file_no][current_line - 1].pos - ppl[production.current_file_no][pcache[production.current_file_no].pc - 1 + j].pos),
                                      production.log_dir);
                     }
                  }
               }
            }
            else
            {
               new_log_file = NO;
            }
#ifdef HAVE_GETLINE
            while ((n = getline(&production.line, &production.line_length,
                                production.fp)) != -1)
#else
            while (fgets(production.line, MAX_LINE_LENGTH, production.fp) != NULL)
#endif
            {
               if (verbose > 2)
               {
                  if (verbose > 3)
                  {
                     (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                                   "%06ld DEBUG 4: [PRODUCTION] readline: %s",
#else
                                   "%06lld DEBUG 4: [PRODUCTION] readline: %s",
#endif
                                   (pri_time_t)(time(NULL) - start),
                                   production.line);
                  }
                  else
                  {
                     lines_read++;
                  }
               }
               if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE))
               {
                  if (ppl[production.current_file_no] == NULL)
                  {
                     if ((ppl[production.current_file_no] = malloc(LOG_LIST_STEP_SIZE * sizeof(struct alda_position_list))) == NULL)
                     {
                        (void)fprintf(stderr,
                                      "Failed to malloc() memory : %s (%s %d)\n",
                                      strerror(errno), __FILE__, __LINE__);
                        exit(INCORRECT);
                     }
                     pcache[production.current_file_no].pc = 0;
                     pcache[production.current_file_no].mpc = 0;
                     ppl[production.current_file_no][0].pos = 0;
                     ppl[production.current_file_no][0].gotcha = NO;
                  }
                  else
                  {
                     if (pcache[production.current_file_no].mpc == pcache[production.current_file_no].pc)
                     {
                        if ((pcache[production.current_file_no].pc % LOG_LIST_STEP_SIZE) == 0)
                        {
                           size_t new_size;

                           new_size = ((pcache[production.current_file_no].pc / LOG_LIST_STEP_SIZE) + 1) *
                                      LOG_LIST_STEP_SIZE * sizeof(struct alda_position_list);
                           if ((ppl[production.current_file_no] = realloc(ppl[production.current_file_no],
                                                                          new_size)) == NULL)
                           {
                              (void)fprintf(stderr,
                                            "Failed to malloc() memory : %s (%s %d)\n",
                                            strerror(errno), __FILE__, __LINE__);
                              exit(INCORRECT);
                           }
                        }
                     }
                     ppl[production.current_file_no][pcache[production.current_file_no].pc].pos = production.bytes_read;
                  }
               }
#ifdef HAVE_GETLINE
               production.bytes_read += n;
#endif
               if (production.line[0] != '#')
               {
                  if (check_production_line(production.line, p_prev_file_name,
                                            p_prev_filename_length,
                                            prev_log_time, prev_dir_id,
                                            prev_job_id, prev_unique_number,
                                            prev_split_job_counter) == SUCCESS)
                  {
                     if (trace_mode == ON)
                     {
                        if (pcache != NULL)
                        {
                           ppl[production.current_file_no][pcache[production.current_file_no].pc - 1].gotcha = YES;
                        }
                        if (prev_proc_cycles > 0)
                        {
                           if (plog.new_filename[0] != '\0')
                           {
                              p_prev_file_name = plog.new_filename;
                              p_prev_filename_length = plog.new_filename_length;
                           }
                           (void)memcpy(&success_plog, &plog, sizeof(struct alda_pdata));
                        }
                     }
                     prev_proc_cycles--;
                     if (prev_proc_cycles < 1)
                     {
                        if (verbose == 3)
                        {
                           (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                                         "%06ld DEBUG 3: [PRODUCTION] ignored %u lines, returning GOT_DATA (%d) [%d]\n",
#else
                                         "%06lld DEBUG 3: [PRODUCTION] ignored %u lines, returning GOT_DATA (%d) [%d]\n",
#endif
                                         (pri_time_t)(time(NULL) - start),
                                         lines_read, GOT_DATA, __LINE__);
                        }
                        return(GOT_DATA);
                     }
                  }
                  else
                  {
                     if (trace_mode == ON)
                     {
                        if ((prev_log_time > 0) &&
                            ((plog.output_time - prev_log_time) > max_diff_time))
                        {
                           if (verbose == 3)
                           {
                              (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                                            "%06ld DEBUG 3: [PRODUCTION] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#else
                                            "%06lld DEBUG 3: [PRODUCTION] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#endif
                                            (pri_time_t)(time(NULL) - start),
                                            lines_read, NO_LOG_DATA, __LINE__);
                           }
                           return(NO_LOG_DATA);
                        }
                        if (pcache[production.current_file_no].mpc != pcache[production.current_file_no].pc)
                        {
                           int i = pcache[production.current_file_no].pc;

                           while ((i < pcache[production.current_file_no].mpc) &&
                                  (ppl[production.current_file_no][i - 1].gotcha == YES))
                           {
                              i++;
                           }
                           if (i != pcache[production.current_file_no].pc)
                           {
                              int current_line;

                              current_line = pcache[production.current_file_no].pc;
                              pcache[production.current_file_no].pc = i - 1;
                              if (fseeko(production.fp,
                                         ppl[production.current_file_no][pcache[production.current_file_no].pc].pos,
                                         SEEK_SET) == -1)
                              {
                                 (void)fprintf(stderr,
                                               "Failed to fseeko() : %s (%s %d)\n",
                                               strerror(errno), __FILE__, __LINE__);
                                 exit(INCORRECT);
                              }
                              else
                              {
                                 production.bytes_read = ppl[production.current_file_no][pcache[production.current_file_no].pc].pos;
                                 if (verbose > 0)
                                 {
                                    (void)fprintf(stdout,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                                  "%06ld DEBUG 1: [PRODUCTION] seeking forward %d - %d = %d lines %ld - %ld = %ld chars in %s\n",
# else
                                                  "%06lld DEBUG 1: [PRODUCTION] seeking forward %d - %d = %d lines %ld - %ld = %ld chars in %s\n",
# endif
#else
# if SIZEOF_TIME_T == 4
                                                  "%06ld DEBUG 1: [PRODUCTION] seeking forward %d - %d = %d lines %lld - %lld = %lld chars in %s\n",
# else
                                                  "%06lld DEBUG 1: [PRODUCTION] seeking forward %d - %d = %d lines %lld - %lld = %lld chars in %s\n",
# endif
#endif
                                                  (pri_time_t)(time(NULL) - start),
                                                  pcache[production.current_file_no].pc,
                                                  current_line,
                                                  pcache[production.current_file_no].pc - current_line,
                                                  (pri_off_t)ppl[production.current_file_no][pcache[production.current_file_no].pc - 1].pos,
                                                  (pri_off_t)ppl[production.current_file_no][current_line - 1].pos,
                                                  (pri_off_t)(ppl[production.current_file_no][pcache[production.current_file_no].pc - 1].pos - ppl[production.current_file_no][current_line - 1].pos),
                                                  production.log_dir);
                                 }
                              }
                           }
                        }
                     }
                  }
               }
               else
               {
                  if ((production.line[1] == '!') && (production.line[2] == '#'))
                  {
                     get_log_type_data(&production.line[3]);
                  }
               }
            }
            clearerr(production.fp);
#ifdef WITH_DEBUG
            (void)fprintf(stderr, "prod %d: %s\n",
                          production.current_file_no, prev_file_name);
#endif
            new_log_file = YES;
         }
         if ((production.current_file_no != 0) ||
             (((mode & ALDA_CONTINUOUS_MODE) == 0) &&
              ((mode & ALDA_CONTINUOUS_DAEMON_MODE) == 0)))
         {
            if (fclose(production.fp) == EOF)
            {
               (void)fprintf(stderr, "Failed to fclose() `%s' : %s (%s %d)\n",
                             production.log_dir, strerror(errno),
                             __FILE__, __LINE__);
            }
            production.fp = NULL;
            production.bytes_read = 0;
         }
      }
      production.current_file_no--;
   } while ((production.current_file_no >= production.end_file_no) &&
            (end_loop == NO));

   if (production.current_file_no < production.end_file_no)
   {
      production.current_file_no = production.end_file_no;
   }

   if ((production.current_file_no != 0) ||
       (((mode & ALDA_CONTINUOUS_MODE) == 0) &&
        ((mode & ALDA_CONTINUOUS_DAEMON_MODE) == 0)))
   {
      if (production.fp != NULL)
      {
         if (fclose(production.fp) == EOF)
         {
            (void)fprintf(stderr, "Failed to fclose() `%s' : %s (%s %d)\n",
                          production.log_dir, strerror(errno), __FILE__, __LINE__);
         }
         production.fp = NULL;
         production.bytes_read = 0;
      }
   }

   if (success_plog.new_filename[0] != '\0')
   {
      (void)memcpy(&plog, &success_plog, sizeof(struct alda_pdata));
      if (verbose == 3)
      {
         (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                       "%06ld DEBUG 3: [PRODUCTION] ignored %u lines, returning GOT_DATA (%d) [%d]\n",
#else
                       "%06lld DEBUG 3: [PRODUCTION] ignored %u lines, returning GOT_DATA (%d) [%d]\n",
#endif
                       (pri_time_t)(time(NULL) - start), lines_read, GOT_DATA,
                       __LINE__);
      }

      return(GOT_DATA);
   }
   if (verbose == 3)
   {
      (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                    "%06ld DEBUG 3: [PRODUCTION] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#else
                    "%06lld DEBUG 3: [PRODUCTION] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#endif
                    (pri_time_t)(time(NULL) - start), lines_read, NO_LOG_DATA,
                    __LINE__);
   }

   return(NO_LOG_DATA);
}
#endif


#ifdef _OUTPUT_LOG
/*+++++++++++++++++++++++++ check_output_log() ++++++++++++++++++++++++++*/
static int
check_output_log(char         *search_afd,
                 char         *prev_file_name,
                 off_t        prev_filename_length,
                 time_t       prev_log_time,
                 unsigned int prev_job_id,
                 unsigned int *prev_unique_number,
                 unsigned int *prev_split_job_counter)
{
#ifdef HAVE_GETLINE
   ssize_t      n;
#endif
   unsigned int lines_read = 0;
   int          end_loop = NO,
                new_log_file = NO,
                ret;

   if (output.fp == NULL)
   {
#ifdef BLUBB
      int p_max_log_files = output.max_log_files;
#endif

      init_file_data((start_time_start == 0) ? init_time_start : start_time_start,
                     end_time_end, SEARCH_OUTPUT_LOG, search_afd);
      if (output.no_of_log_files == 0)
      {
         if (verbose == 3)
         {
            (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                          "%06ld DEBUG 3: [OUTPUT] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#else
                          "%06lld DEBUG 3: [OUTPUT] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#endif
                          (pri_time_t)(time(NULL) - start), lines_read,
                          NO_LOG_DATA, __LINE__);
         }
         return(NO_LOG_DATA);
      }
#ifdef BLUBB
      if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE))
      {
         if (ocache != NULL)
         {
            free(ocache);
            ocache = NULL;
         }
         if (opl != NULL)
         {
            int i;

            for (i = 0; i < p_max_log_files; i++)
            {
               free(opl[i]);
            }
            free(opl);
            opl = NULL;
         }
      }
#endif
   }
   do
   {
      if (output.fp == NULL)
      {
#ifdef WITH_LOG_CACHE
         (void)sprintf(output.p_log_cache_number, "%d", output.current_file_no);
         if ((output.cache_fd = open(output.log_cache_dir, O_RDONLY)) == -1)
         {
            if (errno != ENOENT)
            {
               (void)fprintf(stderr, "Failed to open() `%s' : %s (%s %d)\n",
                             output.log_cache_dir, strerror(errno),
                             __FILE__, __LINE__);
            }
         }
#endif
         (void)sprintf(output.p_log_number, "%d", output.current_file_no);
         if ((output.fp = fopen(output.log_dir, "r")) == NULL)
         {
            if (errno != ENOENT)
            {
               (void)fprintf(stderr, "Failed to fopen() `%s' : %s (%s %d)\n",
                             output.log_dir, strerror(errno),
                             __FILE__, __LINE__);
               return(INCORRECT);
            }
         }
         else
         {
#ifdef HAVE_STATX
            struct statx stat_buf;
#else
            struct stat stat_buf;
#endif

            output.bytes_read = 0;
            output.fd = fileno(output.fp);
#ifdef HAVE_STATX
            if (statx(output.fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                      STATX_INO | STATX_MTIME, &stat_buf) == -1)
#else
            if (fstat(output.fd, &stat_buf) == -1)
#endif
            {
               (void)fprintf(stderr, "Failed to access `%s' : %s (%s %d)\n",
                             output.log_dir, strerror(errno),
                             __FILE__, __LINE__);
            }
            else
            {
#ifdef HAVE_STATX
               output.inode_number = stat_buf.stx_ino;
#else
               output.inode_number = stat_buf.st_ino;
#endif
            }
            if (mode & ALDA_CONTINUOUS_DAEMON_MODE)
            {
               if (lseek(output.fd, 0, SEEK_END) == -1)
               {
                  (void)fprintf(stderr,
                                "Failed to lseek() `%s' : %s (%s %d)\n",
                                output.log_dir, strerror(errno),
                                __FILE__, __LINE__);
               }
            }
            if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE))
            {
               if (ocache == NULL)
               {
                  if ((ocache = malloc((output.max_log_files * sizeof(struct alda_cache_data)))) == NULL)
                  {
                        (void)fprintf(stderr,
                                      "Failed to malloc() memory : %s (%s %d)\n",
                                      strerror(errno), __FILE__, __LINE__);
                        exit(INCORRECT);
                  }
                  (void)memset(ocache, 0,
                               (output.max_log_files * sizeof(struct alda_cache_data)));
               }
               if (opl == NULL)
               {
                  int i;

                  if ((opl = malloc((output.max_log_files * sizeof(struct alda_position_list *)))) == NULL)
                  {
                     (void)fprintf(stderr,
                                   "Failed to malloc() memory : %s (%s %d)\n",
                                   strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
                  for (i = 0; i < output.max_log_files; i++)
                  {
                     opl[i] = NULL;
                  }
               }
               if (ocache[output.current_file_no].inode == 0)
               {
                  ocache[output.current_file_no].inode = output.inode_number;
               }
               else
               {
                  if (output.inode_number != ocache[output.current_file_no].inode)
                  {
                     reshuffel_cache_data(__LINE__);
                  }
               }
#ifdef HAVE_STATX
               ocache[output.current_file_no].last_entry = stat_buf.stx_mtime.tv_sec;
#else
               ocache[output.current_file_no].last_entry = stat_buf.st_mtime;
#endif
            }
         }
      }
      if (output.fp != NULL)
      {
          if ((prev_log_time == 0L) ||
              (ocache[output.current_file_no].last_entry == 0L) ||
              (ocache[output.current_file_no].last_entry >= prev_log_time))
         {
            if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE) &&
                (prev_log_time > 0) && (new_log_file == NO))
            {
               int gotcha = NO,
                   i = -2,
                   j,
                   tmp_current_file_no;

               tmp_current_file_no = j = output.current_file_no;
               if (j == 0)
               {
                  end_loop = YES;
               }
               do
               {
                  if ((opl[j] != NULL) && (ocache[j].pc > 0) &&
                      (opl[j][0].time <= prev_log_time) &&
                      (opl[j][ocache[j].pc - 1].time >= prev_log_time))
                  {
                     for (i = (ocache[j].pc - 2); i > -1; i--)
                     {
                        if (opl[j][i].time < prev_log_time)
                        {
                           i++;
                           while ((i < ocache[j].pc) &&
                                  (opl[j][i].gotcha == YES))
                           {
                              i++;
                           }
                           if (i < ocache[j].pc)
                           {
                              ocache[j].pc = i;
                           }
                           else
                           {
                              if (ocache[j].pc > 0)
                              {
                                 ocache[j].pc--;
                              }
                           }
                           output.current_file_no = j;
                           gotcha = YES;
                           j = output.end_file_no;
                           break;
                        }
                     }
                  }
                  if (gotcha == YES)
                  {
                     break;
                  }
                  j++;
               } while (j <= output.start_file_no);

               /*
                * Close current file when we search in a another log file!
                */
               if (tmp_current_file_no != output.current_file_no)
               {
#ifdef WITH_LOG_CACHE
                  if (close(output.cache_fd) == -1)
                  {
                     (void)fprintf(stderr,
                                   "Failed to close() `%s' : %s (%s %d)\n",
                                   output.log_cache_dir, strerror(errno),
                                   __FILE__, __LINE__);
                  }
                  output.cache_fd = -1;
#endif
                  if (fclose(output.fp) == EOF)
                  {
                     (void)fprintf(stderr,
                                   "Failed to fclose() `%s' : %s (%s %d)\n",
                                   output.log_dir, strerror(errno),
                                   __FILE__, __LINE__);
                  }
                  output.bytes_read = 0;
                  output.fp = NULL;

#ifdef WITH_LOG_CACHE
                  (void)sprintf(output.p_log_cache_number, "%d",
                                output.current_file_no);
                  if ((output.cache_fd = open(output.log_cache_dir,
                                              O_RDONLY)) == -1)
                  {
                     if (errno != ENOENT)
                     {
                        (void)fprintf(stderr,
                                      "Failed to open() `%s' : %s (%s %d)\n",
                                      output.log_cache_dir, strerror(errno),
                                      __FILE__, __LINE__);
                     }
                  }
#endif
                  (void)sprintf(output.p_log_number, "%d",
                                output.current_file_no);
                  if ((output.fp = fopen(output.log_dir, "r")) == NULL)
                  {
                     if (errno != ENOENT)
                     {
                        (void)fprintf(stderr,
                                      "Failed to fopen() `%s' : %s (%s %d)\n",
                                      output.log_dir, strerror(errno),
                                      __FILE__, __LINE__);
                        return(INCORRECT);
                     }
                  }
                  else
                  {
#ifdef HAVE_STATX
                     struct statx stat_buf;
#else
                     struct stat stat_buf;
#endif

                     output.fd = fileno(output.fp);
#ifdef HAVE_STATX
                     if (statx(output.fd, "",
                               AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                               STATX_INO | STATX_MTIME, &stat_buf) == -1)
#else
                     if (fstat(output.fd, &stat_buf) == -1)
#endif
                     {
                        (void)fprintf(stderr,
                                      "Failed to access `%s' : %s (%s %d)\n",
                                      output.log_dir, strerror(errno),
                                      __FILE__, __LINE__);
                     }
                     else
                     {
#ifdef HAVE_STATX
                        output.inode_number = stat_buf.stx_ino;
#else
                        output.inode_number = stat_buf.st_ino;
#endif
                        if (ocache[output.current_file_no].inode == 0)
                        {
                           ocache[output.current_file_no].inode = output.inode_number;
                        }
                        else
                        {
                           if (output.inode_number != ocache[output.current_file_no].inode)
                           {
                              reshuffel_cache_data(__LINE__);
                           }
                        }
#ifdef HAVE_STATX
                        ocache[output.current_file_no].last_entry = stat_buf.stx_mtime.tv_sec;
#else
                        ocache[output.current_file_no].last_entry = stat_buf.st_mtime;
#endif
                     }
                  }
               }
               if ((opl[output.current_file_no] != NULL) &&
                   ((gotcha == YES) || (i == -1) ||
                    ((opl[output.current_file_no][ocache[output.current_file_no].pc - 1].time >= prev_log_time) &&
                     (prev_log_time > 0))))
               {
                  int current_line;

                  current_line = ocache[output.current_file_no].pc;
                  if (i == -1)
                  {
                     i = 0;
                     while ((i < ocache[output.current_file_no].pc) &&
                            (opl[output.current_file_no][i].gotcha == YES))
                     {
                        i++;
                     }
                     if (i == 0)
                     {
                        ocache[output.current_file_no].pc = i;
                     }
                     else
                     {
                        ocache[output.current_file_no].pc = i - 1;
                     }
                  }
                  if ((i == -2) && (ocache[output.current_file_no].pc > 0))
                  {
                     ocache[output.current_file_no].pc--;
                  }
                  while ((ocache[output.current_file_no].pc > 0) &&
                         (opl[output.current_file_no][ocache[output.current_file_no].pc].time >= prev_log_time))
                  {
                     ocache[output.current_file_no].pc--;
                  }
                  if (fseeko(output.fp,
                             opl[output.current_file_no][ocache[output.current_file_no].pc].pos,
                             SEEK_SET) == -1)
                  {
                     (void)fprintf(stderr,
#if SIZEOF_OFF_T == 4
                                   "Failed to fseeko() to %ld : %s (%s %d)\n",
#else
                                   "Failed to fseeko() to %lld : %s (%s %d)\n",
#endif
                                   (pri_off_t)opl[output.current_file_no][ocache[output.current_file_no].pc].pos,
                                   strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
                  else
                  {
                     output.bytes_read = opl[output.current_file_no][ocache[output.current_file_no].pc].pos;
                     if (verbose > 0)
                     {
                        if (ocache[output.current_file_no].pc == 0)
                        {
                           j = 1;
                        }
                        else
                        {
                           j = 0;
                        }
                        (void)fprintf(stdout,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                      "%06ld DEBUG 1: [OUTPUT] seeking back %d - %d = %d lines %ld - %ld = %ld chars in %s\n",
# else
                                      "%06lld DEBUG 1: [OUTPUT] seeking back %d - %d = %d lines %ld - %ld = %ld chars in %s\n",
# endif
#else
# if SIZEOF_TIME_T == 4
                                      "%06ld DEBUG 1: [OUTPUT] seeking back %d - %d = %d lines %lld - %lld = %lld chars in %s\n",
# else
                                      "%06lld DEBUG 1: [OUTPUT] seeking back %d - %d = %d lines %lld - %lld = %lld chars in %s\n",
# endif
#endif
                                      (pri_time_t)(time(NULL) - start),
                                      current_line,
                                      ocache[output.current_file_no].pc,
                                      current_line - ocache[output.current_file_no].pc,
                                      (pri_off_t)opl[output.current_file_no][current_line - 1].pos,
                                      (pri_off_t)opl[output.current_file_no][ocache[output.current_file_no].pc - 1 + j].pos,
                                      (pri_off_t)(opl[output.current_file_no][current_line - 1].pos - opl[output.current_file_no][ocache[output.current_file_no].pc - 1 + j].pos),
                                      output.log_dir);
                     }
                  }
               }
               else
               {
#ifdef WITH_LOG_CACHE
                  if (output.cache_fd > 0)
                  {
                     seek_cache_position(&output, start_time_start);
                  }
#endif
               }
            }
            else
            {
               new_log_file = NO;
            }
#ifdef HAVE_GETLINE
            while ((n = getline(&output.line, &output.line_length,
                                output.fp)) != -1)
#else
            while (fgets(output.line, MAX_LINE_LENGTH, output.fp) != NULL)
#endif
            {
               if (verbose > 2)
               {
                  if (verbose > 3)
                  {
                     (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                                   "%06ld DEBUG 4: [OUTPUT] readline: %s",
#else
                                   "%06lld DEBUG 4: [OUTPUT] readline: %s",
#endif
                                   (pri_time_t)(time(NULL) - start),
                                   output.line);
                  }
                  else
                  {
                     lines_read++;
                  }
               }
               if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE))
               {
                  if (opl[output.current_file_no] == NULL)
                  {
                     if ((opl[output.current_file_no] = malloc(LOG_LIST_STEP_SIZE * sizeof(struct alda_position_list))) == NULL)
                     {
                        (void)fprintf(stderr,
                                      "Failed to malloc() memory : %s (%s %d)\n",
                                      strerror(errno), __FILE__, __LINE__);
                        exit(INCORRECT);
                     }
                     ocache[output.current_file_no].pc = 0;
                     ocache[output.current_file_no].mpc = 0;
                     opl[output.current_file_no][0].pos = 0;
                     opl[output.current_file_no][0].gotcha = NO;
                  }
                  else
                  {
                     if (ocache[output.current_file_no].mpc == ocache[output.current_file_no].pc)
                     {
                        if ((ocache[output.current_file_no].pc % LOG_LIST_STEP_SIZE) == 0)
                        {
                           size_t new_size;

                           new_size = ((ocache[output.current_file_no].pc / LOG_LIST_STEP_SIZE) + 1) *
                                      LOG_LIST_STEP_SIZE * sizeof(struct alda_position_list);
                           if ((opl[output.current_file_no] = realloc(opl[output.current_file_no],
                                                                      new_size)) == NULL)
                           {
                              (void)fprintf(stderr,
                                            "Failed to malloc() memory : %s (%s %d)\n",
                                            strerror(errno), __FILE__, __LINE__);
                              exit(INCORRECT);
                           }
                        }
                     }
                     opl[output.current_file_no][ocache[output.current_file_no].pc].pos = output.bytes_read;
                  }
               }
#ifdef HAVE_GETLINE
               output.bytes_read += n;
#endif
               if (output.line[0] != '#')
               {
                  ret = check_output_line(output.line, prev_file_name,
                                          prev_filename_length, prev_log_time,
                                          prev_job_id, prev_unique_number,
                                          prev_split_job_counter);
                  if (verbose > 4)
                  {
                     (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                                   "%06ld DEBUG 5: [OUTPUT] check_output_line() returns %d\n",
#else
                                   "%06lld DEBUG 5: [OUTPUT] check_output_line() returns %d\n",
#endif
                                   (pri_time_t)(time(NULL) - start), ret);
                  }
                  if (ret == SUCCESS)
                  {
                     if ((trace_mode == ON) && (ocache != NULL))
                     {
                        opl[output.current_file_no][ocache[output.current_file_no].pc - 1].gotcha = YES;
                     }
                     if (verbose == 3)
                     {
                        (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                                      "%06ld DEBUG 3: [OUTPUT] ignored %u lines, returning GOT_DATA (%d) [%d]\n",
#else
                                      "%06lld DEBUG 3: [OUTPUT] ignored %u lines, returning GOT_DATA (%d) [%d]\n",
#endif
                                      (pri_time_t)(time(NULL) - start),
                                      lines_read, GOT_DATA, __LINE__);
                     }
                     return(GOT_DATA);
                  }
                  else if (ret == SEARCH_TIME_UP)
                       {
                          if (verbose == 3)
                          {
                             (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                                           "%06ld DEBUG 3: [OUTPUT] ignored %u lines, returning SEARCH_TIME_UP (%d) [%d]\n",
#else
                                           "%06lld DEBUG 3: [OUTPUT] ignored %u lines, returning SEARCH_TIME_UP (%d) [%d]\n",
#endif
                                           (pri_time_t)(time(NULL) - start),
                                           lines_read, SEARCH_TIME_UP, __LINE__);
                          }
                          return(ret);
                       }
                       else
                       {
                          if (trace_mode == ON)
                          {
                             if ((prev_log_time > 0) &&
                                 ((olog.output_time - prev_log_time) > max_diff_time))
                             {
                                if (verbose == 3)
                                {
                                   (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                                                 "%06ld DEBUG 3: [OUTPUT] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#else
                                                 "%06lld DEBUG 3: [OUTPUT] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#endif
                                                 (pri_time_t)(time(NULL) - start),
                                                 lines_read, NO_LOG_DATA, __LINE__);
                                }
                                return(NO_LOG_DATA);
                             }

                             if (ocache[output.current_file_no].mpc != ocache[output.current_file_no].pc)
                             {
                                int i = ocache[output.current_file_no].pc;

                                while ((i < ocache[output.current_file_no].mpc) &&
                                       (opl[output.current_file_no][i - 1].gotcha == YES))
                                {
                                   i++;
                                }
                                if (i != ocache[output.current_file_no].pc)
                                {
                                   int current_line;

                                   current_line = ocache[output.current_file_no].pc;
                                   ocache[output.current_file_no].pc = i - 1;
                                   if (fseeko(output.fp,
                                              opl[output.current_file_no][ocache[output.current_file_no].pc].pos,
                                              SEEK_SET) == -1)
                                   {
                                      (void)fprintf(stderr,
#if SIZEOF_OFF_T == 4
                                                    "Failed to fseeko() to %ld : %s (%s %d)\n",
#else
                                                    "Failed to fseeko() to %lld : %s (%s %d)\n",
#endif
                                                    (pri_off_t)opl[output.current_file_no][ocache[output.current_file_no].pc].pos,
                                                    strerror(errno), __FILE__, __LINE__);
                                      exit(INCORRECT);
                                   }
                                   else
                                   {
                                      output.bytes_read = opl[output.current_file_no][ocache[output.current_file_no].pc].pos;
                                      if (verbose > 0)
                                      {
                                         (void)fprintf(stdout,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                                       "%06ld DEBUG 1: [OUTPUT] seeking forward %d - %d = %d lines %ld + %ld = %ld chars in %s\n",
# else
                                                       "%06lld DEBUG 1: [OUTPUT] seeking forward %d - %d = %d lines %ld + %ld = %ld chars in %s\n",
# endif
#else
# if SIZEOF_TIME_T == 4
                                                       "%06ld DEBUG 1: [OUTPUT] seeking forward %d - %d = %d lines %lld + %lld = %lld chars in %s\n",
# else
                                                       "%06lld DEBUG 1: [OUTPUT] seeking forward %d - %d = %d lines %lld + %lld = %lld chars in %s\n",
# endif
#endif
                                                       (pri_time_t)(time(NULL) - start),
                                                       ocache[output.current_file_no].pc,
                                                       current_line,
                                                       ocache[output.current_file_no].pc - current_line,
                                                       (pri_off_t)opl[output.current_file_no][ocache[output.current_file_no].pc - 1].pos,
                                                       (pri_off_t)opl[output.current_file_no][current_line - 1].pos,
                                                       (pri_off_t)(opl[output.current_file_no][ocache[output.current_file_no].pc - 1].pos - opl[output.current_file_no][current_line - 1].pos),
                                                       output.log_dir);
                                      }
                                   }
                                }
                             }
                          }
                       }
               }
               else
               {
                  if ((output.line[1] == '!') && (output.line[2] == '#'))
                  {
                     get_log_type_data(&output.line[3]);
                  }
               }
            }
            clearerr(output.fp);
            new_log_file = YES;
         }
#ifdef WITH_DEBUG
         (void)fprintf(stderr, "olog %d: %s\n",
                       output.current_file_no, prev_file_name);
#endif
         if ((output.current_file_no != 0) ||
             (((mode & ALDA_CONTINUOUS_MODE) == 0) &&
              ((mode & ALDA_CONTINUOUS_DAEMON_MODE) == 0)))
         {
            if (fclose(output.fp) == EOF)
            {
               (void)fprintf(stderr, "Failed to fclose() `%s' : %s (%s %d)\n",
                             output.log_dir, strerror(errno),
                             __FILE__, __LINE__);
            }
            output.fp = NULL;
            output.bytes_read = 0;
         }
      }
      output.current_file_no--;
   } while ((output.current_file_no >= output.end_file_no) && (end_loop == NO));

   if (output.current_file_no < output.end_file_no)
   {
      output.current_file_no = output.end_file_no;
   }

   if ((output.current_file_no != 0) ||
       (((mode & ALDA_CONTINUOUS_MODE) == 0) &&
        ((mode & ALDA_CONTINUOUS_DAEMON_MODE) == 0)))
   {
      if (output.fp != NULL)
      {
         if (fclose(output.fp) == EOF)
         {
            (void)fprintf(stderr, "Failed to fclose() `%s' : %s (%s %d)\n",
                          output.log_dir, strerror(errno), __FILE__, __LINE__);
         }
         output.fp = NULL;
         output.bytes_read = 0;
      }
   }
   if (verbose == 3)
   {
      (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                    "%06ld DEBUG 3: [OUTPUT] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#else
                    "%06lld DEBUG 3: [OUTPUT] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#endif
                    (pri_time_t)(time(NULL) - start), lines_read, NO_LOG_DATA,
                    __LINE__);
   }

   return(NO_LOG_DATA);
}
#endif


#ifdef _DELETE_LOG
/*+++++++++++++++++++++++++ check_delete_log() ++++++++++++++++++++++++++*/
static int
check_delete_log(char         *search_afd,
                 char         *prev_file_name,
                 off_t        prev_filename_length,
                 time_t       prev_log_time,
                 unsigned int prev_job_id,
                 unsigned int *prev_unique_number,
                 unsigned int *prev_split_job_counter)
{
#ifdef HAVE_GETLINE
   ssize_t      n;
#endif
   unsigned int lines_read = 0;
   int          end_loop = NO,
                new_log_file = NO;

   if (delete.fp == NULL)
   {
#ifdef BLUBB
      int p_max_log_files = delete.max_log_files;
#endif

      init_file_data((start_time_start == 0) ? init_time_start : start_time_start,
                     end_time_end, SEARCH_DELETE_LOG, search_afd);
      if (delete.no_of_log_files == 0)
      {
         if (verbose == 3)
         {
            (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                          "%06ld DEBUG 3: [DELETE] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#else
                          "%06lld DEBUG 3: [DELETE] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#endif
                          (pri_time_t)(time(NULL) - start), lines_read,
                          NO_LOG_DATA, __LINE__);
         }
         return(NO_LOG_DATA);
      }
#ifdef BLUBB
      if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE))
      {
         if (dcache != NULL)
         {
            free(dcache);
            dcache = NULL;
         }
         if (dpl != NULL)
         {
            int i;

            for (i = 0; i < p_max_log_files; i++)
            {
               free(dpl[i]);
            }
            free(dpl);
            dpl = NULL;
         }
      }
#endif
   }
   do
   {
      if (delete.fp == NULL)
      {
         (void)sprintf(delete.p_log_number, "%d", delete.current_file_no);
         if ((delete.fp = fopen(delete.log_dir, "r")) == NULL)
         {
            if (errno != ENOENT)
            {
               (void)fprintf(stderr, "Failed to fopen() `%s' : %s (%s %d)\n",
                             delete.log_dir, strerror(errno),
                             __FILE__, __LINE__);
               return(INCORRECT);
            }
         }
         else
         {
#ifdef HAVE_STATX
            struct statx stat_buf;
#else
            struct stat stat_buf;
#endif

            delete.bytes_read = 0;
            delete.fd = fileno(delete.fp);
#ifdef HAVE_STATX
            if (statx(delete.fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                      STATX_INO | STATX_MTIME, &stat_buf) == -1)
#else
            if (fstat(delete.fd, &stat_buf) == -1)
#endif
            {
               (void)fprintf(stderr, "Failed to access `%s' : %s (%s %d)\n",
                             delete.log_dir, strerror(errno),
                             __FILE__, __LINE__);
            }
            else
            {
#ifdef HAVE_STATX
               delete.inode_number = stat_buf.stx_ino;
#else
               delete.inode_number = stat_buf.st_ino;
#endif
            }
            if (mode & ALDA_CONTINUOUS_DAEMON_MODE)
            {
               if (lseek(delete.fd, 0, SEEK_END) == -1)
               {
                  (void)fprintf(stderr,
                                "Failed to lseek() `%s' : %s (%s %d)\n",
                                delete.log_dir, strerror(errno),
                                __FILE__, __LINE__);
               }
            }
            if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE))
            {
               if (dcache == NULL)
               {
                  if ((dcache = malloc((delete.max_log_files * sizeof(struct alda_cache_data)))) == NULL)
                  {
                     (void)fprintf(stderr,
                                   "Failed to malloc() memory : %s (%s %d)\n",
                                   strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
                  (void)memset(dcache, 0,
                               (delete.max_log_files * sizeof(struct alda_cache_data)));
               }
               if (dpl == NULL)
               {
                  int i;

                  if ((dpl = malloc((delete.max_log_files * sizeof(struct alda_position_list *)))) == NULL)
                  {
                     (void)fprintf(stderr,
                                   "Failed to malloc() memory : %s (%s %d)\n",
                                   strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
                  for (i = 0; i < delete.max_log_files; i++)
                  {
                     dpl[i] = NULL;
                  }
               }
               if (dcache[delete.current_file_no].inode == 0)
               {
                  dcache[delete.current_file_no].inode = delete.inode_number;
               }
               else
               {
                  if (delete.inode_number != dcache[delete.current_file_no].inode)
                  {
                     reshuffel_cache_data(__LINE__);
                  }
               }
#ifdef HAVE_STATX
               dcache[delete.current_file_no].last_entry = stat_buf.stx_mtime.tv_sec;
#else
               dcache[delete.current_file_no].last_entry = stat_buf.st_mtime;
#endif
            }
         }
      }
      if (delete.fp != NULL)
      {
          if ((prev_log_time == 0L) ||
              (dcache[delete.current_file_no].last_entry == 0L) ||
              (dcache[delete.current_file_no].last_entry >= prev_log_time))
         {
            if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE) &&
                (prev_log_time > 0) && (new_log_file == NO))
            {
               int gotcha = NO,
                   i = -2,
                   j,
                   tmp_current_file_no;

               tmp_current_file_no = j = delete.current_file_no;
               if (j == 0)
               {
                  end_loop = YES;
               }
               do
               {
                  if ((dpl[j] != NULL) && (dcache[j].pc > 0) &&
                      (dpl[j][0].time <= prev_log_time) &&
                      (dpl[j][dcache[j].pc - 1].time >= prev_log_time))
                  {
                     for (i = (dcache[j].pc - 2); i > -1; i--)
                     {                                  
                        if (dpl[j][i].time < prev_log_time)
                        {
                           i++;
                           while ((i < dcache[j].pc) && (dpl[j][i].gotcha == YES))
                           {
                              i++;
                           }
                           if (i < dcache[j].pc)
                           {
                              dcache[j].pc = i;
                           }
                           else
                           {
                              if (dcache[j].pc > 0)
                              {
                                 dcache[j].pc--;
                              }
                           }
                           delete.current_file_no = j;
                           gotcha = YES;
                           j = delete.end_file_no;
                           break;
                        }
                     }
                  }
                  if (gotcha == YES)
                  {
                     break;
                  }
                  j++;
               } while (j <= delete.start_file_no);

               /*
                * Close current file when we search in a another log file!
                */
               if (tmp_current_file_no != delete.current_file_no)
               {
                  if (fclose(delete.fp) == EOF)
                  {
                     (void)fprintf(stderr,
                                   "Failed to fclose() `%s' : %s (%s %d)\n",
                                   delete.log_dir, strerror(errno),
                                   __FILE__, __LINE__);
                  }
                  delete.bytes_read = 0;
                  delete.fp = NULL;

                  (void)sprintf(delete.p_log_number, "%d", delete.current_file_no);
                  if ((delete.fp = fopen(delete.log_dir, "r")) == NULL)
                  {
                     if (errno != ENOENT)
                     {
                        (void)fprintf(stderr,
                                      "Failed to fopen() `%s' : %s (%s %d)\n",
                                      delete.log_dir, strerror(errno),
                                      __FILE__, __LINE__);
                        return(INCORRECT);
                     }
                  }
                  else
                  {
#ifdef HAVE_STATX
                     struct statx stat_buf;
#else
                     struct stat stat_buf;
#endif

                     delete.fd = fileno(delete.fp);
#ifdef HAVE_STATX
                     if (statx(delete.fd, "",
                               AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                               STATX_INO | STATX_MTIME, &stat_buf) == -1)
#else
                     if (fstat(delete.fd, &stat_buf) == -1)
#endif
                     {
                        (void)fprintf(stderr,
                                      "Failed to access `%s' : %s (%s %d)\n",
                                      delete.log_dir, strerror(errno),
                                      __FILE__, __LINE__);
                     }
                     else
                     {
#ifdef HAVE_STATX
                        delete.inode_number = stat_buf.stx_ino;
#else
                        delete.inode_number = stat_buf.st_ino;
#endif
                        if (dcache[delete.current_file_no].inode == 0)
                        {
                           dcache[delete.current_file_no].inode = delete.inode_number;
                        }
                        else
                        {
                           if (delete.inode_number != dcache[delete.current_file_no].inode)
                           {
                              reshuffel_cache_data(__LINE__);
                           }
                        }
#ifdef HAVE_STATX
                        dcache[delete.current_file_no].last_entry = stat_buf.stx_mtime.tv_sec;
#else
                        dcache[delete.current_file_no].last_entry = stat_buf.st_mtime;
#endif
                     }
                  }
               }

               if ((dpl[delete.current_file_no] != NULL) &&
                   ((gotcha == YES) || (i == -1) ||
                    ((dpl[delete.current_file_no][dcache[delete.current_file_no].pc - 1].time >= prev_log_time) &&
                     (prev_log_time > 0))))
               {
                  int current_line;

                  current_line = dcache[delete.current_file_no].pc;
                  if (i == -1)
                  {
                     i = 0;
                     while ((i < dcache[delete.current_file_no].pc) &&
                            (dpl[delete.current_file_no][i].gotcha == YES))
                     {
                        i++;
                     }
                     if (i == 0)
                     {
                        dcache[delete.current_file_no].pc = i;
                     }
                     else
                     {
                        dcache[delete.current_file_no].pc = i - 1;
                     }
                  }
                  if ((i == -2) && (dcache[delete.current_file_no].pc > 0))
                  {
                     dcache[delete.current_file_no].pc--;
                  }
                  while ((dcache[delete.current_file_no].pc > 0) &&
                         (dpl[delete.current_file_no][dcache[delete.current_file_no].pc].time >= prev_log_time))
                  {
                     dcache[delete.current_file_no].pc--;
                  }
                  if (fseeko(delete.fp,
                             dpl[delete.current_file_no][dcache[delete.current_file_no].pc].pos,
                             SEEK_SET) == -1)
                  {
                     (void)fprintf(stderr,
#if SIZEOF_OFF_T == 4
                                   "Failed to fseeko() to %ld : %s (%s %d)\n",
#else
                                   "Failed to fseeko() to %lld : %s (%s %d)\n",
#endif
                                   (pri_off_t)dpl[delete.current_file_no][dcache[delete.current_file_no].pc].pos,
                                   strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
                  else
                  {
                     delete.bytes_read = dpl[delete.current_file_no][dcache[delete.current_file_no].pc].pos;
                     if (verbose > 0)
                     {
                        if (dcache[delete.current_file_no].pc == 0)
                        {
                           j = 1;
                        }
                        else
                        {
                           j = 0;
                        }
                        (void)fprintf(stdout,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                      "%06ld DEBUG 1: [DELETE] seeking back %d - %d = %d lines %ld - %ld = %ld chars in %s\n",
# else
                                      "%06lld DEBUG 1: [DELETE] seeking back %d - %d = %d lines %ld - %ld = %ld chars in %s\n",
# endif
#else
# if SIZEOF_TIME_T == 4
                                      "%06ld DEBUG 1: [DELETE] seeking back %d - %d = %d lines %lld - %lld = %lld chars in %s\n",
# else
                                      "%06lld DEBUG 1: [DELETE] seeking back %d - %d = %d lines %lld - %lld = %lld chars in %s\n",
# endif
#endif
                                      (pri_time_t)(time(NULL) - start),
                                      current_line,
                                      dcache[delete.current_file_no].pc,
                                      current_line - dcache[delete.current_file_no].pc,
                                      (pri_off_t)dpl[delete.current_file_no][current_line - 1].pos,
                                      (pri_off_t)dpl[delete.current_file_no][dcache[delete.current_file_no].pc - 1 + j].pos,
                                      (pri_off_t)(dpl[delete.current_file_no][current_line - 1].pos - dpl[delete.current_file_no][dcache[delete.current_file_no].pc - 1 + j].pos),
                                      delete.log_dir);
                     }
                  }
               }
            }
            else
            {
               new_log_file = NO;
            }
#ifdef HAVE_GETLINE
            while ((n = getline(&delete.line, &delete.line_length,
                                delete.fp)) != -1)
#else
            while (fgets(delete.line, MAX_LINE_LENGTH, delete.fp) != NULL)
#endif
            {
               if (verbose > 2)
               {
                  if (verbose > 3)
                  {
                     (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                                   "%06ld DEBUG 4: [DELETE] readline: %s",
#else
                                   "%06lld DEBUG 4: [DELETE] readline: %s",
#endif
                                   (pri_time_t)(time(NULL) - start),
                                   delete.line);
                  }
                  else
                  {
                     lines_read++;
                  }
               }
               if ((trace_mode == ON) && (mode & ALDA_FORWARD_MODE))
               {
                  if (dpl[delete.current_file_no] == NULL)
                  {
                     if ((dpl[delete.current_file_no] = malloc(LOG_LIST_STEP_SIZE * sizeof(struct alda_position_list))) == NULL)
                     {
                        (void)fprintf(stderr,
                                      "Failed to malloc() memory : %s (%s %d)\n",
                                      strerror(errno), __FILE__, __LINE__);
                        exit(INCORRECT);
                     }
                     dcache[delete.current_file_no].pc = 0;
                     dcache[delete.current_file_no].mpc = 0;
                     dpl[delete.current_file_no][0].pos = 0;
                     dpl[delete.current_file_no][0].gotcha = NO;
                  }
                  else
                  {
                     if (dcache[delete.current_file_no].mpc == dcache[delete.current_file_no].pc)
                     {
                        if ((dcache[delete.current_file_no].pc % LOG_LIST_STEP_SIZE) == 0)
                        {
                           size_t new_size;

                           new_size = ((dcache[delete.current_file_no].pc / LOG_LIST_STEP_SIZE) + 1) *
                                      LOG_LIST_STEP_SIZE * sizeof(struct alda_position_list);
                           if ((dpl[delete.current_file_no] = realloc(dpl[delete.current_file_no],
                                                                      new_size)) == NULL)
                           {
                              (void)fprintf(stderr,
                                            "Failed to malloc() memory : %s (%s %d)\n",
                                            strerror(errno), __FILE__, __LINE__);
                              exit(INCORRECT);
                           }
                        }
                     }
                     dpl[delete.current_file_no][dcache[delete.current_file_no].pc].pos = delete.bytes_read;
                  }
               }
#ifdef HAVE_GETLINE
               delete.bytes_read += n;
#endif
               if (delete.line[0] != '#')
               {
                  if (check_delete_line(delete.line, prev_file_name,
                                        prev_filename_length, prev_log_time,
                                        prev_job_id, prev_unique_number,
                                        prev_split_job_counter) == SUCCESS)
                  {
                     if ((trace_mode == ON) && (dcache != NULL))
                     {
                        dpl[delete.current_file_no][dcache[delete.current_file_no].pc - 1].gotcha = YES;
                     }
                     if (verbose == 3)
                     {
                        (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                                      "%06ld DEBUG 3: [DELETE] ignored %u lines, returning GOT_DATA (%d) [%d]\n",
#else
                                      "%06lld DEBUG 3: [DELETE] ignored %u lines, returning GOT_DATA (%d) [%d]\n",
#endif
                                      (pri_time_t)(time(NULL) - start),
                                      lines_read, GOT_DATA, __LINE__);
                     }
                     return(GOT_DATA);
                  }
                  else
                  {
                     if (trace_mode == ON)
                     {
                        if ((prev_log_time > 0) &&
                            ((dlog.delete_time - prev_log_time) > max_diff_time))
                        {
                           if (verbose == 3)
                           {
                              (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                                            "%06ld DEBUG 3: [DELETE] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#else
                                            "%06lld DEBUG 3: [DELETE] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#endif
                                            (pri_time_t)(time(NULL) - start),
                                            lines_read, NO_LOG_DATA, __LINE__);
                           }
                           return(NO_LOG_DATA);
                        }
                        if (dcache[delete.current_file_no].mpc != dcache[delete.current_file_no].pc)
                        {
                           int i = dcache[delete.current_file_no].pc;

                           while ((i < dcache[delete.current_file_no].mpc) &&
                                  (dpl[delete.current_file_no][i - 1].gotcha == YES))
                           {
                              i++;
                           }
                           if (i != dcache[delete.current_file_no].pc)
                           {
                              int current_line;

                              current_line = dcache[delete.current_file_no].pc;
                              dcache[delete.current_file_no].pc = i - 1;
                              if (fseeko(delete.fp,
                                         dpl[delete.current_file_no][dcache[delete.current_file_no].pc].pos,
                                         SEEK_SET) == -1)
                              {
                                 (void)fprintf(stderr,
                                               "Failed to fseeko() : %s (%s %d)\n",
                                               strerror(errno), __FILE__, __LINE__);
                                 exit(INCORRECT);
                              }
                              else
                              {
                                 delete.bytes_read = dpl[delete.current_file_no][dcache[delete.current_file_no].pc].pos;
                                 if (verbose > 0)
                                 {
                                    (void)fprintf(stdout,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                                  "%06ld DEBUG 1: [DELETE] seeking forward %d - %d = %d lines %ld + %ld = %ld chars in %s\n",
# else
                                                  "%06lld DEBUG 1: [DELETE] seeking forward %d - %d = %d lines %ld + %ld = %ld chars in %s\n",
# endif
#else
# if SIZEOF_TIME_T == 4
                                                  "%06ld DEBUG 1: [DELETE] seeking forward %d - %d = %d lines %lld + %lld = %lld chars in %s\n",
# else
                                                  "%06lld DEBUG 1: [DELETE] seeking forward %d - %d = %d lines %lld + %lld = %lld chars in %s\n",
# endif
#endif
                                                  (pri_time_t)(time(NULL) - start),
                                                  dcache[delete.current_file_no].pc,
                                                  current_line,
                                                  dcache[delete.current_file_no].pc - current_line,
                                                  (pri_off_t)dpl[delete.current_file_no][dcache[delete.current_file_no].pc - 1].pos,
                                                  (pri_off_t)dpl[delete.current_file_no][current_line - 1].pos,
                                                  (pri_off_t)(dpl[delete.current_file_no][dcache[delete.current_file_no].pc - 1].pos - dpl[delete.current_file_no][current_line - 1].pos),
                                                  delete.log_dir);
                                 }
                              }
                           }
                        }
                     }
                  }
               }
               else
               {
                  if ((delete.line[1] == '!') && (delete.line[2] == '#'))
                  {
                     get_log_type_data(&delete.line[3]);
                  }
               }
            }
            clearerr(delete.fp);
#ifdef WITH_DEBUG
            (void)fprintf(stderr, "del %d: %s\n",
                          delete.current_file_no, prev_file_name);
#endif
            new_log_file = YES;
         }
         if ((delete.current_file_no != 0) ||
             (((mode & ALDA_CONTINUOUS_MODE) == 0) &&
              ((mode & ALDA_CONTINUOUS_DAEMON_MODE) == 0)))
         {
            if (fclose(delete.fp) == EOF)
            {
               (void)fprintf(stderr, "Failed to fclose() `%s' : %s (%s %d)\n",
                             delete.log_dir, strerror(errno),
                             __FILE__, __LINE__);
            }
            delete.fp = NULL;
            delete.bytes_read = 0;
         }
      }
      delete.current_file_no--;
   } while ((delete.current_file_no >= delete.end_file_no) &&
            (end_loop == NO));

   if (delete.current_file_no < delete.end_file_no)
   {
      delete.current_file_no = delete.end_file_no;
   }

   if ((delete.current_file_no != 0) ||
       (((mode & ALDA_CONTINUOUS_MODE) == 0) &&
        ((mode & ALDA_CONTINUOUS_DAEMON_MODE) == 0)))
   {
      if (delete.fp != NULL)
      {
         if (fclose(delete.fp) == EOF)
         {
            (void)fprintf(stderr, "Failed to fclose() `%s' : %s (%s %d)\n",
                          delete.log_dir, strerror(errno), __FILE__, __LINE__);
         }
         delete.fp = NULL;
         delete.bytes_read = 0;
      }
   }
   if (verbose == 3)
   {
      (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                    "%06ld DEBUG 3: [DELETE] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#else
                    "%06lld DEBUG 3: [DELETE] ignored %u lines, returning NO_LOG_DATA (%d) [%d]\n",
#endif
                    (pri_time_t)(time(NULL) - start), lines_read, NO_LOG_DATA,
                    __LINE__);
   }

   return(NO_LOG_DATA);
}
#endif


/*-------------------------- init_file_data() ---------------------------*/
static void
init_file_data(time_t start_time,
               time_t end_time,
               int    log_type,
               char   *search_afd)
{
   int                  no_of_log_files;
#ifdef HAVE_STATX
   struct statx         stat_buf;
#else
   struct stat          stat_buf;
#endif
   struct log_file_data *p_data;

   switch (log_type)
   {
#ifdef _INPUT_LOG
      case SEARCH_INPUT_LOG :
         if (search_afd == NULL)
         {
            input.p_log_number = input.log_dir +
                                 sprintf(input.log_dir, "%s%s/%s",
                                         p_work_dir, LOG_DIR,
                                         INPUT_BUFFER_FILE);
         }
         else
         {
            input.p_log_number = input.log_dir +
                                 sprintf(input.log_dir, "%s%s/%s/%s",
                                         p_work_dir, RLOG_DIR, search_afd,
                                         INPUT_BUFFER_FILE);
         }
         input.max_log_files = MAX_INPUT_LOG_FILES;
         get_max_log_values(&input.max_log_files, MAX_INPUT_LOG_FILES_DEF,
                            MAX_INPUT_LOG_FILES, NULL, NULL, 0,
# ifdef WITH_AFD_MON
                            (mode & ALDA_LOCAL_MODE) ? AFD_CONFIG_FILE : MON_CONFIG_FILE
# else
                            AFD_CONFIG_FILE
# endif
                            );
         no_of_log_files = input.max_log_files;
         p_data = &input;
         break;
#endif

#ifdef _DISTRIBUTION_LOG
      case SEARCH_DISTRIBUTION_LOG :
         if (search_afd == NULL)
         {
            distribution.p_log_number = distribution.log_dir +
                                 sprintf(distribution.log_dir, "%s%s/%s",
                                         p_work_dir, LOG_DIR,
                                         DISTRIBUTION_BUFFER_FILE);
         }
         else
         {
            distribution.p_log_number = distribution.log_dir +
                                 sprintf(distribution.log_dir, "%s%s/%s/%s",
                                         p_work_dir, RLOG_DIR, search_afd,
                                         DISTRIBUTION_BUFFER_FILE);
         }
         distribution.max_log_files = MAX_DISTRIBUTION_LOG_FILES;
         get_max_log_values(&distribution.max_log_files,
                            MAX_DISTRIBUTION_LOG_FILES_DEF,
                            MAX_DISTRIBUTION_LOG_FILES, NULL, NULL, 0,
# ifdef WITH_AFD_MON
                            (mode & ALDA_LOCAL_MODE) ? AFD_CONFIG_FILE : MON_CONFIG_FILE
# else
                            AFD_CONFIG_FILE
# endif
                            );
         no_of_log_files = distribution.max_log_files;
         p_data = &distribution;
         break;
#endif

#ifdef _PRODUCTION_LOG
      case SEARCH_PRODUCTION_LOG :
         if (search_afd == NULL)
         {
            production.p_log_number = production.log_dir +
                                      sprintf(production.log_dir, "%s%s/%s",
                                              p_work_dir, LOG_DIR,
                                              PRODUCTION_BUFFER_FILE);
         }
         else
         {
            production.p_log_number = production.log_dir +
                                      sprintf(production.log_dir, "%s%s/%s/%s",
                                              p_work_dir, RLOG_DIR, search_afd,
                                              PRODUCTION_BUFFER_FILE);
         }
         production.max_log_files = MAX_PRODUCTION_LOG_FILES;
         get_max_log_values(&production.max_log_files,
                            MAX_PRODUCTION_LOG_FILES_DEF,
                            MAX_PRODUCTION_LOG_FILES, NULL, NULL, 0,
# ifdef WITH_AFD_MON
                            (mode & ALDA_LOCAL_MODE) ? AFD_CONFIG_FILE : MON_CONFIG_FILE
# else
                            AFD_CONFIG_FILE
# endif
                            );
         no_of_log_files = production.max_log_files;
         p_data = &production;
         break;
#endif

#ifdef _OUTPUT_LOG
      case SEARCH_OUTPUT_LOG :
         if (search_afd == NULL)
         {
            output.p_log_number = output.log_dir +
                                  sprintf(output.log_dir, "%s%s/%s",
                                          p_work_dir, LOG_DIR,
                                          OUTPUT_BUFFER_FILE);
# ifdef WITH_LOG_CACHE
            output.p_log_cache_number = output.log_cache_dir +
                                        sprintf(output.log_cache_dir,
                                                "%s%s/%s",
                                                p_work_dir, LOG_DIR,
                                                OUTPUT_BUFFER_CACHE_FILE);
# endif
         }
         else
         {
            output.p_log_number = output.log_dir +
                                  sprintf(output.log_dir, "%s%s/%s/%s",
                                          p_work_dir, RLOG_DIR, search_afd,
                                          OUTPUT_BUFFER_FILE);
# ifdef WITH_LOG_CACHE
            output.p_log_cache_number = output.log_cache_dir +
                                        sprintf(output.log_cache_dir,
                                                "%s%s/%s/%s",
                                                p_work_dir, RLOG_DIR,
                                                search_afd,
                                                OUTPUT_BUFFER_CACHE_FILE);
# endif
         }
         output.max_log_files = MAX_OUTPUT_LOG_FILES;
         get_max_log_values(&output.max_log_files, MAX_OUTPUT_LOG_FILES_DEF,
                            MAX_OUTPUT_LOG_FILES, NULL, NULL, 0,
# ifdef WITH_AFD_MON
                            (mode & ALDA_LOCAL_MODE) ? AFD_CONFIG_FILE : MON_CONFIG_FILE
# else
                            AFD_CONFIG_FILE
# endif
                            );
         no_of_log_files = output.max_log_files;
         p_data = &output;
         break;
#endif

#ifdef _DELETE_LOG
      case SEARCH_DELETE_LOG :
         if (search_afd == NULL)
         {
            delete.p_log_number = delete.log_dir +
                                  sprintf(delete.log_dir, "%s%s/%s",
                                          p_work_dir, LOG_DIR,
                                          DELETE_BUFFER_FILE);
         }
         else
         {
            delete.p_log_number = delete.log_dir +
                                  sprintf(delete.log_dir, "%s%s/%s/%s",
                                          p_work_dir, RLOG_DIR, search_afd,
                                          DELETE_BUFFER_FILE);
         }
         delete.max_log_files = MAX_DELETE_LOG_FILES;
         get_max_log_values(&delete.max_log_files, MAX_DELETE_LOG_FILES_DEF,
                            MAX_DELETE_LOG_FILES, NULL, NULL, 0,
# ifdef WITH_AFD_MON
                            (mode & ALDA_LOCAL_MODE) ? AFD_CONFIG_FILE : MON_CONFIG_FILE
# else
                            AFD_CONFIG_FILE
# endif
                            );
         no_of_log_files = delete.max_log_files;
         p_data = &delete;
         break;
#endif

      default : /* Wrong type. */
         (void)fprintf(stderr,
                       "Unknown log type %d, please contact maintainer %s\n",
                       log_type, AFD_MAINTAINER);
         exit(INCORRECT);
   }
   p_data->end_file_no = p_data->start_file_no = -1;

   /* If we are in continuous and daemon mode, we do not want the daemon */
   /* to log everything from the beginning each time it is started.      */
   if (mode & ALDA_CONTINUOUS_DAEMON_MODE)
   {
      p_data->end_file_no = 0;
      p_data->start_file_no = 0;
   }
   else
   {
      int i;

      for (i = 0; i < no_of_log_files; i++)
      {
         (void)sprintf(p_data->p_log_number, "%d", i);
#ifdef HAVE_STATX
         if (statx(0, p_data->log_dir, AT_STATX_SYNC_AS_STAT,
                   STATX_MTIME, &stat_buf) == 0)
#else
         if (stat(p_data->log_dir, &stat_buf) == 0)
#endif
         {
#ifdef HAVE_STATX
            if ((stat_buf.stx_mtime.tv_sec >= start_time) ||
                (p_data->start_file_no == -1))
#else
            if ((stat_buf.st_mtime >= start_time) ||
                (p_data->start_file_no == -1))
#endif
            {
               p_data->start_file_no = i;
            }
            if (end_time == -1)
            {
               p_data->end_file_no = i;
            }
#ifdef HAVE_STATX
            else if ((stat_buf.stx_mtime.tv_sec >= end_time) ||
                     (p_data->end_file_no == -1))
#else
            else if ((stat_buf.st_mtime >= end_time) ||
                     (p_data->end_file_no == -1))
#endif
                  {
                     p_data->end_file_no = i;
                  }
         }
      }
   }
   p_data->no_of_log_files = p_data->start_file_no - p_data->end_file_no + 1;
   p_data->current_file_no = p_data->start_file_no;

   return;
}


#ifdef CACHE_DEBUG
/*++++++++++++++++++++++++++ print_alda_cache() +++++++++++++++++++++++++*/
static void
print_alda_cache(void)
{
   int i, j;

   if (ucache != NULL)
   {
      (void)fprintf(stdout, "\nDISTRIBUTION Cache data:\n");
      for (i = distribution.current_file_no; i <= distribution.start_file_no; i++)
      {
         for (j = 0; j < ucache[i].mpc; j++)
         {
# if SIZEOF_TIME_T == 4
#  if SIZEOF_OFF_T == 4
            (void)fprintf(stdout, "%-2d %-10d: %lx %s %s %ld\n",
#  else
            (void)fprintf(stdout, "%-2d %-10d: %lx %s %s %lld\n",
#  endif
# else
#  if SIZEOF_OFF_T == 4
            (void)fprintf(stdout, "%-2d %-10d: %lx %s %s %ld\n",
#  else
            (void)fprintf(stdout, "%-2d %-10d: %llx %s %s %lld\n",
#  endif
# endif
                          i, j, (pri_time_t)upl[i][j].time, upl[i][j].filename,
                          (upl[i][j].gotcha == YES) ? "YES" : " NO",
                          (pri_off_t)upl[i][j].pos);
         }
      }
   }

   if (pcache != NULL)
   {
      (void)fprintf(stdout, "\nPRODUCTION Cache data:\n");
      for (i = production.current_file_no; i <= production.start_file_no; i++)
      {
         for (j = 0; j < pcache[i].mpc; j++)
         {
# if SIZEOF_TIME_T == 4
#  if SIZEOF_OFF_T == 4
            (void)fprintf(stdout, "%-2d %-10d: %lx %s %s %ld\n",
#  else
            (void)fprintf(stdout, "%-2d %-10d: %lx %s %s %lld\n",
#  endif
# else
#  if SIZEOF_OFF_T == 4
            (void)fprintf(stdout, "%-2d %-10d: %lx %s %s %ld\n",
#  else
            (void)fprintf(stdout, "%-2d %-10d: %llx %s %s %lld\n",
#  endif
# endif
                          i, j, (pri_time_t)ppl[i][j].time, ppl[i][j].filename,
                          (ppl[i][j].gotcha == YES) ? "YES" : " NO",
                          (pri_off_t)ppl[i][j].pos);
         }
      }
   }

   if (ocache != NULL)
   {
      (void)fprintf(stdout, "\nOUTPUT Cache data:\n");
      for (i = output.current_file_no; i <= output.start_file_no; i++)
      {
         for (j = 0; j < ocache[i].mpc; j++)
         {
# if SIZEOF_TIME_T == 4
#  if SIZEOF_OFF_T == 4
            (void)fprintf(stdout, "%-2d %-10d: %lx %s %s %ld\n",
#  else
            (void)fprintf(stdout, "%-2d %-10d: %lx %s %s %lld\n",
#  endif
# else
#  if SIZEOF_OFF_T == 4
            (void)fprintf(stdout, "%-2d %-10d: %lx %s %s %ld\n",
#  else
            (void)fprintf(stdout, "%-2d %-10d: %llx %s %s %lld\n",
#  endif
# endif
                          i, j, (pri_time_t)opl[i][j].time, opl[i][j].filename,
                          (opl[i][j].gotcha == YES) ? "YES" : " NO",
                          (pri_off_t)opl[i][j].pos);
         }
      }
   }

   if (dcache != NULL)
   {
      (void)fprintf(stdout, "\nDELETE Cache data:\n");
      for (i = delete.current_file_no; i <= delete.start_file_no; i++)
      {
         for (j = 0; j < dcache[i].mpc; j++)
         {
# if SIZEOF_TIME_T == 4
#  if SIZEOF_OFF_T == 4
            (void)fprintf(stdout, "%-2d %-10d: %lx %s %s %ld\n",
#  else
            (void)fprintf(stdout, "%-2d %-10d: %lx %s %s %lld\n",
#  endif
# else
#  if SIZEOF_OFF_T == 4
            (void)fprintf(stdout, "%-2d %-10d: %lx %s %s %ld\n",
#  else
            (void)fprintf(stdout, "%-2d %-10d: %llx %s %s %lld\n",
#  endif
# endif
                          i, j, (pri_time_t)dpl[i][j].time, dpl[i][j].filename,
                          (dpl[i][j].gotcha == YES) ? "YES" : " NO",
                          (pri_off_t)dpl[i][j].pos);
         }
      }
   }

   return;
}
#endif /* CACHE_DEBUG */


/*++++++++++++++++++++++++ reshuffel_cache_data() +++++++++++++++++++++++*/
static void
reshuffel_cache_data(int line)
{
   (void)fprintf(stderr,
                 "reshuffel_cache_data(%d) : Not yet done!!!!\n", line);

   return;
}
